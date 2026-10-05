/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Host media layer: OpenH264 decodes stream video to the SDL window,
 * OpenH264... no audio yet (stereo PCM to SDL audio queue).
 * Mirrors webos/media.c's interface so ihs_glue stays shared.
 */
#include "media.h"
#include "host_video.h"

#include <SDL.h>

#include <string.h>

bool media_init(void) { return host_video_init(); }

bool media_video_open(int width, int height) {
    (void) width;
    (void) height;
    return true;
}

bool media_video_feed(const uint8_t *au, size_t len) {
    if (au == NULL || len == 0) return false;
    int w = 0, h = 0;
    return host_video_feed(au, len, &w, &h);
}

void media_video_close(void) {}

bool media_audio_open_pcm(int sample_rate, int channels) {
    (void) sample_rate;
    (void) channels;
    return true;
}

bool media_audio_decode(const uint8_t *data, size_t len) {
    (void) data;
    (void) len;
    return true;
}

void media_audio_close(void) {}

void media_quit(void) { host_video_quit(); }
