/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Media abstraction: LGNC direct video/audio on webOS, stubs on host.
 * Opus decoding for the audio path is provided by the webOS build via
 * libopus; PCM frames go straight to LGNC DirectAudio.
 */
#ifndef STEAMVIEW_MEDIA_H
#define STEAMVIEW_MEDIA_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool media_init(void);

/* H.264 video path */
bool media_video_open(int width, int height);
bool media_video_feed(const uint8_t *au, size_t len);
/* Increments on every successful plane open; OSD uses it to burst. */
unsigned long media_plane_generation(void);

/* Audio path: Opus packets in, PCM out to the audio device */
bool media_audio_open_pcm(int sample_rate, int channels);
bool media_audio_decode(const uint8_t *data, size_t len);
void media_audio_close(void);

void media_quit(void);

#endif
