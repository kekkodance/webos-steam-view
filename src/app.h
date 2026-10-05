/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Steam View  -  Steam Remote Play client for webOS 1-4 TVs (LGTV NetCast path).
 *
 * Streams the Steam Frame's spectator view to the TV using the open-source
 * ihslib implementation of the Steam In-Home Streaming protocol, and decodes
 * with the LGNC direct video API (the proven low-latency H.264 path on these
 * TVs, same as moonlight-tv's ss4s-lgnc module).
 *
 * Nothing is installed on the headset: the Frame is a first-party Remote
 * Play host (connect to "frame" via Steam Link per Valve's docs). This app
 * is the missing Remote Play *client* for old webOS.
 */
#ifndef STEAMVIEW_APP_H
#define STEAMVIEW_APP_H

#include <stdbool.h>
#include <stddef.h>
#include <ihslib/common.h>

typedef enum AppState {
    APP_STATE_DISCOVERY,
    APP_STATE_HOST_PICK,     /* host list shown; user navigates */
    APP_STATE_REQUESTING,    /* stream request sent; waiting on host */
    APP_STATE_AUTHORIZING,   /* pairing: our PIN shown, user types it on host */
    APP_STATE_STREAMING,
    APP_STATE_ERROR,
} AppState;

typedef struct AppHost {
    char name[65];
    uint64_t client_id;
    IHS_SocketAddress address;
    IHS_SteamUniverse universe;
} AppHost;

#define APP_MAX_HOSTS 8

typedef struct App {
    char status[192];
    AppState state;
    struct IHS_Client *client;
    struct IHS_Session *session;
    /* discovered hosts */
    AppHost hosts[APP_MAX_HOSTS];
    int host_count;
    int host_selected;  /* index into hosts, -1 = none */
    /* chosen host */
    char host_name[65];
    char host_address[48];
    uint16_t host_port;
    /* session handoff from streaming-success callback */
    uint8_t session_key[64];
    size_t session_key_len;
    uint16_t stream_port;
    uint64_t host_steam_id;
    /* secret key negotiated with this host (scaronni key exchange); the
     * session handshake must use it, not the long-term identity secret.
     * Persisted in .steamview-identity so re-pairing isn't needed. */
    uint8_t host_secret[32];
    bool host_secret_valid;
    bool stream_ready;  /* set by worker callback, consumed by main loop */
    bool stream_retry;  /* auth done, re-request stream when slot frees */
    bool rediscover;    /* session dropped: rebuild client + rediscover */
    IHS_SocketAddress stream_addr;
    char pin[16];
    int pin_len;
    /* stats */
    unsigned long frames, keyframes;
    unsigned long audio_frames;
    int width, height;
    bool running;
} App;

void app_set_status(App *app, const char *fmt, ...);

#endif
