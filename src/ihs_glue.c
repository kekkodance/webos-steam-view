/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * ihslib session + client glue: discovery, PIN authorization, streaming
 * request, and media callbacks routed to the LGNC layer.
 */
#include "app.h"
#include "media.h"
#ifdef TARGET_WEBOS
#include "webos/osd.h"
#endif

#include <ihslib.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Device identity. Must be stable across runs: the host remembers paired
 * clients by deviceId. Persisted next to the app's settings (see main.c). */
static uint64_t g_device_id;
static uint8_t g_secret_key[32];

void identity_init(uint64_t device_id, const uint8_t secret[32]) {
    g_device_id = device_id;
    memcpy(g_secret_key, secret, sizeof(g_secret_key));
}

/* ---------- session callbacks ---------- */

IHS_Session *app_session_start(App *app);

static void on_session_initialized(IHS_Session *session, void *context) {
    App *app = context;
    app_set_status(app, "Session initialized");
}

static void on_session_connecting(IHS_Session *session, void *context) {
    App *app = context;
    app_set_status(app, "Connecting to %s", app->host_name);
}

static void on_session_configuring(IHS_Session *session, IHS_SessionConfig *config, void *context) {
    /* H.264 only: the LGNC NetCast decoder on webOS 1-4 has no HEVC. */
    config->enableHevc = false;
    config->enableAudio = true;
    App *app = context;
    app_set_status(app, "Negotiating stream with %s", app->host_name);
}

static void on_session_connected(IHS_Session *session, void *context) {
    App *app = context;
    app->state = APP_STATE_STREAMING;
    app_set_status(app, "Streaming");
}

static void on_session_disconnected(IHS_Session *session, void *context) {
    (void) session;
    App *app = context;
    media_video_close();
    media_audio_close();
    if (app->state == APP_STATE_STREAMING) {
        /* Flag only (worker thread): the main loop rebuilds the client and
         * rediscovers, same as Back-out. Never touches client/session here  - 
         * teardown from the callback is the OK-click crash class. */
        extern void app_request_rediscover(App *a);
        app_request_rediscover(app);
        app_set_status(app, "Disconnected. Restarting discovery...");
    }
}

static void on_session_finalized(IHS_Session *session, void *context) {
    // no-op; resources freed in app teardown
}

/* ---------- video callbacks ---------- */

static int on_video_start(IHS_Session *session, const IHS_StreamVideoConfig *config, void *context) {
    App *app = context;
    if (config->codec != IHS_StreamVideoCodecH264) {
        app_set_status(app, "Unsupported video codec %d (need H.264)", config->codec);
        return -1;
    }
    if (!media_video_open((int) config->width, (int) config->height)) {
        app_set_status(app, "Video open failed");
        return -1;
    }
    app->width = (int) config->width;
    app->height = (int) config->height;
    return 0;
}

static IHS_StreamVideoSubmitResult on_video_submit(IHS_Session *session, IHS_Buffer *data,
                                                   IHS_StreamVideoFrameFlag flags, void *context) {
    App *app = context;
    /* Drop frames outside streaming: during teardown a dying worker can
     * deliver stream AUs after the menu plane reopened (1080p bytes into
     * a 720p decoder = garbage flash). */
    if (app->state != APP_STATE_STREAMING) {
        return IHS_StreamVideoSubmitReportLost;
    }
    const uint8_t *payload = IHS_BufferPointer(data);
    size_t len = data->size;
    if (!media_video_feed(payload, len)) {
        /* Decode hiccup: report lost so ihslib drops state and waits for a
         * keyframe (SubmitFrame handles the reset). Error would kill the
         * whole session, which is wrong for a transient decoder failure. */
        return IHS_StreamVideoSubmitReportLost;
    }
    app->frames++;
    if (flags & IHS_StreamVideoFrameKeyFrame) app->keyframes++;
    return IHS_StreamVideoSubmitOK;
}

static void on_video_stop(IHS_Session *session, void *context) {
    media_video_close();
}

static int on_video_capture_size(IHS_Session *session, int width, int height, void *context) {
    App *app = context;
    if (!media_video_open(width, height)) return -1;
    app->width = width;
    app->height = height;
    return 0;
}

/* ---------- audio callbacks (Opus -> PCM -> DirectAudio) ---------- */

static int on_audio_start(IHS_Session *session, const IHS_StreamAudioConfig *config, void *context) {
    (void) session;
    (void) context;
    /* Audio disabled: the LGNC DirectAudio path on this TV produces
     * delayed garbage regardless of decode/chunk/pacing fixes. Revisit
     * with a capture of the raw Opus bytes if ever needed. */
    (void) config;
    return -1;
}
static int on_audio_submit(IHS_Session *session, IHS_Buffer *data, void *context) {
    App *app = context;
    app->audio_frames++;
#ifdef TARGET_WEBOS
    if ((app->audio_frames % 500) == 1) {
        osd_trace("audio frames=%lu size=%u", app->audio_frames, (unsigned) data->size);
    }
#endif
    /* raw packets are fed through in media_audio_decode; Opus decoded there */
    media_audio_decode(IHS_BufferPointer(data), data->size);
    return 0;
}

static void on_audio_stop(IHS_Session *session, void *context) {
    media_audio_close();
}

/* ---------- client callbacks ---------- */

static void on_host_discovered(IHS_Client *client, const IHS_HostInfo *host, void *context) {
    (void) client;
    App *app = context;
    if (app->state != APP_STATE_DISCOVERY && app->state != APP_STATE_HOST_PICK) return;

    /* dedup by clientId: hosts announce periodically */
    for (int i = 0; i < app->host_count; i++) {
        if (app->hosts[i].client_id == host->clientId) {
            /* refresh */
            app->hosts[i].address = host->address;
            app->hosts[i].universe = host->universe;
            snprintf(app->hosts[i].name, sizeof(app->hosts[i].name), "%s", host->hostname);
            return;
        }
    }
    if (app->host_count >= APP_MAX_HOSTS) return;

    AppHost *slot = &app->hosts[app->host_count++];
    snprintf(slot->name, sizeof(slot->name), "%s", host->hostname);
    slot->client_id = host->clientId;
    slot->address = host->address;
    slot->universe = host->universe;
    app_set_status(app, "%d host%s found. Pick one.", app->host_count, app->host_count == 1 ? "" : "s");
}

/* Generate our pairing PIN (shown on OUR screen; typed into Steam on the
 * host). Random per attempt, like ihsplay  -  it doubles as the key-exchange
 * mask, so it must be unpredictable. */
static void app_make_pin(App *app) {
    /* rand() is seeded in identity_load (main.c); 4 digits = Steam's PIN. */
    int v = 1 + (int) (rand() % 9999);
    snprintf(app->pin, sizeof(app->pin), "%04d", v);
    app->pin_len = (int) strlen(app->pin);
}

static void app_host_info(App *app, IHS_HostInfo *info) {
    AppHost *h = &app->hosts[app->host_selected];
    memset(info, 0, sizeof(*info));
    info->clientId = h->client_id;
    info->address = h->address;
    info->universe = h->universe;
    snprintf(info->hostname, sizeof(info->hostname), "%s", h->name);
}

/* Request a stream from the selected host. Input channel enabled but never
 * fed: Steam needs it (heartbeats) to sustain data flow; view-only clients
 * that disable it starve after the grace period. */
void app_request_stream(App *app) {
    if (app->host_selected < 0 || app->host_selected >= app->host_count) return;
    /* Already paired (key persisted): reclaim the negotiated secret first  - 
     * otherwise the device token proves the wrong key and the host answers
     * Unauthorized, which is the repeated-PIN loop. */
    if (app->host_secret_valid) IHS_ClientSetSecretKey(app->client, app->host_secret);
    IHS_HostInfo info;
    app_host_info(app, &info);
    app->state = APP_STATE_REQUESTING;
    app_set_status(app, "Requesting stream from %s...", info.hostname);
    IHS_StreamingRequest req = {
            .maxResolution = {1920, 1080},
            .streamingEnable = {true, true, true},
            .audioChannelCount = 2,
    };
    IHS_ClientStreamingRequest(app->client, &info, &req);
}

static void on_auth_progress(IHS_Client *client, const IHS_HostInfo *host, void *context) {
    App *app = context;
    app_set_status(app, "Authorizing with %s...", host->hostname);
}

static void on_auth_success(IHS_Client *client, const IHS_HostInfo *host, uint64_t steamId, void *context) {
    App *app = context;
    app->host_steam_id = steamId;
    /* The client secret is now the key negotiated with THIS host; stash it
     * for the session handshake (which runs on a fresh client object that
     * starts life with the long-term identity secret) and persist it so
     * the next launch streams without re-pairing. */
    IHS_ClientGetSecretKey(client, app->host_secret);
    app->host_secret_valid = true;
    extern void app_host_secret_saved(const uint8_t secret[32]);
    app_host_secret_saved(app->host_secret);
    /* Pairing done: PIN is spent. Show progress, not the PIN screen, so the
     * UI moves to Connecting/Streaming instead of sitting on the PIN. */
    app->pin[0] = '\0';
    app->pin_len = 0;
    app->state = APP_STATE_REQUESTING;
    app_set_status(app, "Paired with %s, starting stream...", host->hostname);

    /* Re-request streaming now that we're authorized (view-only). The auth
     * task slot still reads busy until the timer thread reaps the stopped
     * task, so a first attempt from inside this callback is usually
     * rejected  -  defer the request to the main loop, which retries until
     * the slot frees. */
    app->stream_retry = true;
}

static void on_auth_failed(IHS_Client *client, const IHS_HostInfo *host, IHS_AuthorizationResult result, void *context) {
    App *app = context;
    /* Canceled = the user dismissed the dialog on the host: back to the
     * picker, no new PIN. Denied = wrong PIN typed: fresh PIN, retry. */
    if (result == IHS_AuthorizationCanceled) {
        app->pin[0] = '\0';
        app->pin_len = 0;
        app->state = APP_STATE_HOST_PICK;
        app_set_status(app, "%s: pairing canceled", host->hostname);
        return;
    }
    if (result == IHS_AuthorizationDenied) {
        /* Denied (wrong PIN typed, or the host dialog dismissed): back to
         * the picker, PIN cleared, no retry. Fresh PIN on next attempt. */
        app->pin[0] = '\0';
        app->pin_len = 0;
        app->stream_retry = false;
        app->state = APP_STATE_HOST_PICK;
        app_set_status(app, "%s: pairing failed", host->hostname);
        return;
    }
    app_set_status(app, "Authorization failed (%d)", result);
    app->state = APP_STATE_HOST_PICK;
}

static void on_stream_progress(IHS_Client *client, const IHS_HostInfo *host, void *context) {
    App *app = context;
    fprintf(stderr, "[APP] stream progress from %s\n", host->hostname);
    fflush(stderr);
    app_set_status(app, "Stream starting on %s...", host->hostname);
}

static void on_stream_success(IHS_Client *client, const IHS_HostInfo *host, const IHS_SocketAddress *address,
                              const uint8_t *sessionKey, size_t sessionKeyLen, void *context) {
    (void) client;
    (void) host;
    App *app = context;
    fprintf(stderr, "[APP] STREAM SUCCESS keylen=%zu\n", sessionKeyLen);
    fflush(stderr);
    app->stream_addr = *address;
    if (sessionKeyLen > sizeof(app->session_key)) sessionKeyLen = sizeof(app->session_key);
    memcpy(app->session_key, sessionKey, sessionKeyLen);
    app->session_key_len = sessionKeyLen;
    /* Flag only: the main loop stops + joins + destroys the client worker,
     * then starts the session. Doing it here would free the timer task
     * whose callback we are running inside (use-after-free -> the OK-click
     * crash). */
    app->stream_ready = true;
    IHS_ClientStop(client);
}

static void on_stream_failed(IHS_Client *client, const IHS_HostInfo *host, IHS_StreamingResult result, void *context) {
    (void) client;
    App *app = context;
    /* Canceled = the user dismissed the accept dialog on the host while a
     * stream request was pending: back to the picker, PIN cleared, no new
     * PIN minted. Checked before the Unauthorized branch  -  the cancel reply
     * can also carry PINRequired, which must not re-mint. */
    if (result == IHS_StreamingCanceled) {
        app->pin[0] = '\0';
        app->pin_len = 0;
        app->stream_retry = false;
        app->state = APP_STATE_HOST_PICK;
        app_set_status(app, "%s: request canceled", host->hostname);
        return;
    }
    /* Unknown device: generate OUR PIN and start the key exchange. The user
     * types the shown PIN into Steam on the host. A preset PIN (scripted
     * pairing, e.g. test_ok.exe 1234) is kept as-is. */
    if (result == IHS_StreamingUnauthorized || result == IHS_StreamingPINRequired) {
        if (app->pin_len <= 0) app_make_pin(app);
        app->state = APP_STATE_AUTHORIZING;
        app_set_status(app, "%s: pairing...", host->hostname);
        IHS_HostInfo info;
        app_host_info(app, &info);
        IHS_ClientAuthorizationRequest(app->client, &info, app->pin);
        return;
    }
    app_set_status(app, "Stream request failed (%d)", result);
    app->state = APP_STATE_HOST_PICK;
}

static const IHS_StreamVideoCallbacks video_callbacks = {
        .start = on_video_start,
        .submit = on_video_submit,
        .stop = on_video_stop,
        .setCaptureSize = on_video_capture_size,
};

static const IHS_StreamAudioCallbacks audio_callbacks = {
        .start = on_audio_start,
        .submit = on_audio_submit,
        .stop = on_audio_stop,
};

static const IHS_StreamSessionCallbacks session_callbacks = {
        .initialized = on_session_initialized,
        .connecting = on_session_connecting,
        .configuring = on_session_configuring,
        .connected = on_session_connected,
        .disconnected = on_session_disconnected,
        .finalized = on_session_finalized,
};

void app_client_attach(App *app) {
    /* MUST be static: ihslib keeps these pointers and calls them from its
     * worker thread long after this function returns. Stack locals here
     * used to go stale, so discovered hosts were silently dropped. */
    static const IHS_ClientDiscoveryCallbacks discovery = {.discovered = on_host_discovered};
    static const IHS_ClientAuthorizationCallbacks auth = {
            .progress = on_auth_progress,
            .success = on_auth_success,
            .failed = on_auth_failed,
    };
    static const IHS_ClientStreamingCallbacks streaming = {
            .progress = on_stream_progress,
            .success = on_stream_success,
            .failed = on_stream_failed,
    };
    IHS_ClientSetDiscoveryCallbacks(app->client, &discovery, app);
    IHS_ClientSetAuthorizationCallbacks(app->client, &auth, app);
    IHS_ClientSetStreamingCallbacks(app->client, &streaming, app);
}

IHS_ClientConfig *identity_client_config(void) {
    static IHS_ClientConfig config;
    config.deviceId = g_device_id;
    config.secretKey = g_secret_key;
    config.deviceName = "Steam View (webOS TV)";
    return &config;
}

IHS_Session *app_session_start(App *app) {
    /* The session handshake must prove the streaming key with the secret
     * negotiated during pairing, not the long-term identity secret. */
    IHS_ClientConfig *cfg = identity_client_config();
    IHS_ClientConfig session_cfg = *cfg;
    if (app->host_secret_valid) session_cfg.secretKey = app->host_secret;
    extern void app_session_log(IHS_LogLevel level, const char *tag, const char *message);
    IHS_SessionInfo info = {
            .address = app->stream_addr,
            .sessionKeyLen = app->session_key_len,
            .steamId = app->host_steam_id,
    };
    memcpy(info.sessionKey, app->session_key, app->session_key_len);
    IHS_Session *session = IHS_SessionCreate(&session_cfg, &info);
    if (session == NULL) return NULL;
    /* Callbacks BEFORE Connect: the worker starts inside Connect and may
     * deliver frames immediately  -  without these, video vanishes silently. */
    IHS_SessionSetLogFunction(session, app_session_log);
    IHS_SessionSetSessionCallbacks(session, &session_callbacks, app);
    IHS_SessionSetVideoCallbacks(session, &video_callbacks, app);
    IHS_SessionSetAudioCallbacks(session, &audio_callbacks, app);
    if (!IHS_SessionConnect(session)) {
        IHS_SessionDestroy(session);
        return NULL;
    }
    /* Session thread is alive: leave the PIN screen now, not on the first
     * video frame (which may take seconds behind the accept dialog). */
    app->state = APP_STATE_STREAMING;
    AppHost *h = &app->hosts[app->host_selected];
    app_set_status(app, "Connecting to %s...", h->name);
    return session;
}
