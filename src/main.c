/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Steam View  -  main loop. SDL window for status/PIN UI; video is decoded on
 * the LGNC plane, so the SDL surface is only used for text.
 */
#include "app.h"
#include "media.h"
#include "ui/ui.h"

#include <ihslib.h>
#include <SDL.h>

#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <winsock2.h>
#endif

void identity_init(uint64_t device_id, const uint8_t secret[32]);
void app_client_attach(App *app);
IHS_Session *app_session_start(App *app);
IHS_ClientConfig *identity_client_config(void);

static volatile sig_atomic_t g_running = 1;

static void ihs_log(IHS_LogLevel level, const char *tag, const char *message) {
    /* Info and below is per-packet chatter; Warn+ is real signal. */
    if (level > IHS_LogLevelWarn) return;
    fprintf(stderr, "[IHS %d %s] %s\n", level, tag, message);
    fflush(stderr);
}

/* Session logs route here so protocol chatter never touches the UI text. */
void app_session_log(IHS_LogLevel level, const char *tag, const char *message) {
    ihs_log(level, tag, message);
}

static void on_signal(int sig) {
    (void) sig;
    g_running = 0;
}

#ifdef TARGET_WEBOS
/* TEMP-DIAG watchdog: abort mid-hang so the core captures the video-init
 * stack. alarm() needs no SDL thread subsystem. See main() usage. */
#include <unistd.h>
static volatile bool video_done_flag = false;
static void video_watchdog_handler(int sig) {
    (void) sig;
    if (!video_done_flag) abort();
}

void video_watchdog_start(void) {
    signal(SIGALRM, video_watchdog_handler);
    alarm(3);
}

static void video_watchdog_stop(void) {
    alarm(0);
    video_done_flag = true;
}
#endif

static App app;

void app_set_status(App *a, const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(a->status, sizeof(a->status), fmt, ap);
    va_end(ap);
}

/* Stable device identity: random on first run, persisted to a file.
 * The host pairs per deviceId, so this must survive restarts. The
 * negotiated per-host secret follows the identity secret; without it
 * every launch re-pairs (the repeated PIN prompt). */
static void identity_load(void) {
    extern void app_host_secret_loaded(const uint8_t secret[32]);
    char path[512];
    const char *home = getenv("HOME");
    if (home == NULL) home = ".";
    snprintf(path, sizeof(path), "%s/.steamview-identity", home);
    uint64_t device_id;
    uint8_t secret[32];
    uint8_t host_secret[32];
    FILE *f = fopen(path, "rb");
    if (f != NULL) {
        if (fread(&device_id, sizeof(device_id), 1, f) == 1 &&
            fread(secret, sizeof(secret), 1, f) == 1) {
            identity_init(device_id, secret);
            if (fread(host_secret, sizeof(host_secret), 1, f) == 1) {
                app_host_secret_loaded(host_secret);
            }
            fclose(f);
            return;
        }
        fclose(f);
    }
    srand((unsigned) time(NULL) ^ (unsigned) SDL_GetPerformanceCounter());
    device_id = 0x7f00000000000000ULL | ((uint64_t) rand() << 32) | (uint64_t) rand();
    for (int i = 0; i < 32; i++) secret[i] = (uint8_t) rand();
    f = fopen(path, "wb");
    if (f != NULL) {
        fwrite(&device_id, sizeof(device_id), 1, f);
        fwrite(secret, sizeof(secret), 1, f);
        fclose(f);
    }
    identity_init(device_id, secret);
}

/* Glue callbacks (no app.h leakage of file layout): the negotiated
 * per-host key is appended to .steamview-identity after pairing, and
 * read back on the next launch. */
static void identity_secret_path(char *out, size_t len) {
    const char *home = getenv("HOME");
    if (home == NULL) home = ".";
    snprintf(out, len, "%s/.steamview-identity", home);
}

void app_host_secret_saved(const uint8_t secret[32]) {
    /* Append after the 8+32 identity bytes; loader reads it back if present. */
    char path[512];
    identity_secret_path(path, sizeof(path));
    FILE *f = fopen(path, "r+b");
    if (f == NULL) return;
    fseek(f, 8 + 32, SEEK_SET);
    fwrite(secret, 1, sizeof(uint8_t) * 32, f);
    fclose(f);
}

void app_host_secret_loaded(const uint8_t secret[32]) {
    memcpy(app.host_secret, secret, sizeof(app.host_secret));
    app.host_secret_valid = true;
}

/* ---------- host picker ---------- */
static bool g_discovery_running = false;

void app_request_stream(App *app);

static void hosts_handle_key(SDL_Keycode key) {
    if (app.host_count == 0) return;
    switch (key) {
        case SDLK_UP:
            if (app.host_selected > 0) app.host_selected--;
            break;
        case SDLK_DOWN:
            if (app.host_selected < app.host_count - 1) app.host_selected++;
            break;
        case SDLK_RETURN:
            app_request_stream(&app);
            break;
        default:
            break;
    }
}

/* Tear down the live session (blocks for thread join). Main loop only  - 
 * never from a session callback (same crash class as the OK-click bug). */
static void app_teardown_session(void) {
    if (app.session == NULL) return;
    IHS_SessionDisconnect(app.session);
    IHS_SessionThreadedJoin(app.session);
    IHS_SessionDestroy(app.session);
    app.session = NULL;
}

/* Rebuild the client worker (gone since the stream started) and restart
 * discovery with a cleared host list. Shared by Back-out and host-side
 * drops. Safe to call with client == NULL or a live client. */
static void app_rediscover(void) {
    if (app.client != NULL) {
        IHS_ClientStop(app.client);
        IHS_ClientThreadedJoin(app.client);
        IHS_ClientDestroy(app.client);
        app.client = NULL;
    }
    app.host_count = 0;
    app.host_selected = -1;
    app.rediscover = false;
    app.client = IHS_ClientCreate(identity_client_config());
    if (app.host_secret_valid) {
        IHS_ClientSetSecretKey(app.client, app.host_secret);
    }
    app_client_attach(&app);
    IHS_ClientStartDiscovery(app.client, 5000);
    app.state = APP_STATE_DISCOVERY;
    app_set_status(&app, "Searching for Steam hosts...");
}

/* Worker-thread entry: only sets the flag, the main loop does the work. */
void app_request_rediscover(App *a) {
    a->rediscover = true;
}

/* Last UI screen pushed; ui_show rebuilds on transitions only. Live values
 * (PIN digits, frame stats) update in place via ui_set_*. */
static int g_ui_screen = -1;

static void ui_sync(AppState state) {
    int want = -1;
    switch (state) {
        case APP_STATE_DISCOVERY:
        case APP_STATE_HOST_PICK:
            want = UI_HOSTS;
            break;
        case APP_STATE_REQUESTING:
        case APP_STATE_AUTHORIZING:
            want = UI_PAIRING;
            break;
        case APP_STATE_STREAMING:
            want = UI_STREAMING;
            break;
        case APP_STATE_ERROR:
            want = UI_ERROR;
            break;
    }
    if (want != g_ui_screen) {
        g_ui_screen = want;
        ui_show(want, &app);
    } else if (want == UI_HOSTS) {
        /* Host list grows after the screen builds (discovery is async):
         * rebuild the cards so "Searching..." becomes the picker. */
        ui_set_hosts(&app);
    }
    if (want == UI_PAIRING) ui_set_pin(app.pin);
    if (want == UI_STREAMING) ui_set_stats(app.width, app.height, app.frames, app.audio_frames);
}

int main(int argc, char **argv) {
    /* optional: <ip> unicast-probes a host directly (broadcast filter fallback) */
    const char *probe_ip = (argc > 1 && argv[1][0] != '-') ? argv[1] : NULL;
#ifdef _WIN32
    WSADATA wsa;
#endif

#ifdef TARGET_WEBOS
    /* TV has no visible stderr under the app manager: trace startup to a
     * file so a splash-hang can be located. */
    FILE *trace = fopen("/tmp/steamview-trace.log", "w");
#define TRACE(fmt, ...) do { if (trace) { fprintf(trace, fmt "\n", ##__VA_ARGS__); fflush(trace); } } while (0)
#else
#define TRACE(fmt, ...) do {} while (0)
#endif

    memset(&app, 0, sizeof(app));
    app.state = APP_STATE_DISCOVERY;
    app.host_selected = -1;
    app.running = true;
    app_set_status(&app, "Starting up...");
    TRACE("startup");

    identity_load();
    TRACE("identity ok");

#ifdef TARGET_WEBOS
    /* Moonlight-tv order: LGNC/NDL media first, SDL video after. The Wayland
     * display init hangs/aborts on webOS 2 if the media layer is not up. */
    if (!media_init()) {
        TRACE("media_init FAILED");
    }
    TRACE("media ok");
#endif
    /* Split init, mirroring moonlight-tv: bare init first (lets the backport
     * settle + hints register), video subsystem after. Single-shot
     * SDL_Init(VIDEO|EVENTS) hangs inside Wayland setup on webOS 2. */
    if (SDL_Init(0) != 0) {
        fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        TRACE("SDL_Init(0) FAILED: %s", SDL_GetError());
        return 1;
    }
    TRACE("sdl base ok");
#ifdef TARGET_WEBOS
    SDL_SetHint("SDL_WEBOS_ACCESS_POLICY_KEYS_BACK", "true");
    SDL_SetHint("SDL_WEBOS_ACCESS_POLICY_KEYS_EXIT", "true");
#endif
    if (SDL_InitSubSystem(SDL_INIT_EVENTS) != 0) {
        fprintf(stderr, "SDL events: %s\n", SDL_GetError());
        TRACE("SDL events FAILED: %s", SDL_GetError());
        return 1;
    }
    TRACE("sdl events ok");
#ifdef TARGET_WEBOS
    /* TEMP-DIAG: if video init hangs, abort after 8s so the core shows
     * exactly where. Remove once the hang is understood. */
    extern void video_watchdog_start(void);
    extern void video_watchdog_stop(void);
    video_watchdog_start();
#endif
    if (SDL_InitSubSystem(SDL_INIT_VIDEO) != 0) {
        fprintf(stderr, "SDL video: %s\n", SDL_GetError());
        TRACE("SDL video FAILED: %s", SDL_GetError());
        return 1;
    }
#ifdef TARGET_WEBOS
    video_watchdog_stop();
#endif
    TRACE("sdl ok");
    /* LVGL owns the window from here: lv_sdl_window_create() builds its own
     * SDL window + renderer + framebuffer (software path, no EGL/Wayland).
     * The old fixed-size window + bitmap render() is retired. */
    extern void ui_port_init(int width, int height);
    extern void ui_port_set_title(const char *title);
    ui_init();
    TRACE("lvgl init ok");
    ui_port_init(1280, 720);
    TRACE("ui_port ok");
    ui_port_set_title("Steam View");
    IHS_Init();
    TRACE("ihs init ok");

    app.client = IHS_ClientCreate(identity_client_config());
    IHS_ClientSetLogFunction(app.client, ihs_log);
    app_client_attach(&app);
    IHS_ClientStartDiscovery(app.client, 5000);
    if (probe_ip != NULL) {
        IHS_IPAddress ip;
        if (IHS_IPAddressFromString(&ip, probe_ip)) IHS_ClientDiscoverAt(app.client, &ip);
    }

    Uint32 last_render = 0;
    while (g_running && app.running) {
        SDL_Event ev;
        while (SDL_PollEvent(&ev)) {
            if (ev.type == SDL_QUIT) {
                g_running = 0;
            } else if (ev.type == SDL_KEYDOWN) {
                SDL_Keycode k = ev.key.keysym.sym;
                if (k == SDLK_ESCAPE || k == SDLK_AC_BACK) {
                    if (app.state == APP_STATE_STREAMING && app.session != NULL) {
                        app_teardown_session();
                        app_rediscover();
                    } else {
                        g_running = 0;
                    }
                } else if (app.state == APP_STATE_DISCOVERY || app.state == APP_STATE_HOST_PICK) {
                    hosts_handle_key(k);
                }
            }
        }

        /* first host seen: move to picker */
        if (app.state == APP_STATE_DISCOVERY && app.host_count > 0) {
            if (app.host_selected < 0) app.host_selected = 0;
            app.state = APP_STATE_HOST_PICK;
        }
        /* stream_ready comes from the worker callback: the client worker is
         * already stopped; join + destroy it here (NOT in the callback  - 
         * that's the use-after-free that crashed the OK click), then start
         * the session on its own fresh socket. */
        if (app.stream_ready && app.session == NULL) {
            app.stream_ready = false;
            IHS_ClientThreadedJoin(app.client);
            IHS_ClientDestroy(app.client);
            app.client = NULL;
            app.session = app_session_start(&app);
            if (app.session == NULL) {
                app_set_status(&app, "Failed to start session");
                app.state = APP_STATE_ERROR;
            }
            continue;
        }
        /* Post-pairing stream request: the auth task slot reads busy until
         * the timer thread reaps it, so retry from here (never from inside
         * the auth callback  -  same re-entrancy crash class). */
        if (app.stream_retry && app.client != NULL) {
            AppHost *h = &app.hosts[app.host_selected];
            IHS_HostInfo info;
            memset(&info, 0, sizeof(info));
            info.clientId = h->client_id;
            info.address = h->address;
            info.universe = h->universe;
            snprintf(info.hostname, sizeof(info.hostname), "%s", h->name);
            IHS_StreamingRequest req = {
                    .maxResolution = {1920, 1080},
                    .streamingEnable = {true, true, false},
                    .audioChannelCount = 2,
            };
            if (IHS_ClientStreamingRequest(app.client, &info, &req)) {
                app.stream_retry = false;
            }
        }
        /* LVGL UI: rebuild screens on state change, live-update values,
         * then pump the LVGL timer (renders + presents via SDL driver).
         * Console prints on status change only  -  the old per-tick print
         * spammed "1 host found" every 50ms. */
        ui_sync(app.state);
        static char last_console[192] = {0};
        if (strcmp(app.status, last_console) != 0) {
            snprintf(last_console, sizeof(last_console), "%s", app.status);
            printf("%s | state=%d frames=%lu\n", app.status, app.state, app.frames);
            fflush(stdout);
        }
        ui_tick(50);
        SDL_Delay(50);
    }

    if (app.session != NULL) {
        IHS_SessionDisconnect(app.session);
        IHS_SessionThreadedJoin(app.session);
        IHS_SessionDestroy(app.session);
    }
    if (app.client != NULL) {
        IHS_ClientStop(app.client);
        IHS_ClientThreadedJoin(app.client);
        IHS_ClientDestroy(app.client);
    }
    IHS_Quit();
    media_quit();
    /* Window + renderer belong to lv_sdl_window; it cleans up on exit. */
    SDL_Quit();
    return 0;
}
