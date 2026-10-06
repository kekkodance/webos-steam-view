/* SPDX-License-Identifier: GPL-3.0-or-later */
#define _GNU_SOURCE /* memmem */
/*
 * LGNC (NetCast legacy) media layer for webOS 1.x-4.x TVs.
 *
 * Mirrors moonlight-tv's ss4s-lgnc module, the proven low-latency H.264
 * path on these systems:
 *   - LGNC_PLUGIN_Initialize + SetAppId(APPID) before any playback
 *   - LGNC_DIRECTVIDEO_Open with explicit w/h/fmt, then Play() per AU
 *   - 4:3/16:9 aspect fit into the 1920x1080 display window
 *   - DirectAudio takes PCM S16LE; Opus is decoded to PCM with libopus
 */
#include "media.h"

#include <lgnc_plugin.h>
#include <lgnc_directaudio.h>
#include <lgnc_directvideo.h>
#include <opus/opus.h>
#include <opus/opus_multistream.h>
#include <sys/mman.h>

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
/* Weak dlsym: the NDK sysroot's static libdl.a is broken (undefined
 * __dlsym), so we link no libdl at all. If the loader provides dlsym the
 * m3 fix uses it; otherwise the pointer is NULL and the fix is skipped. */
__attribute__((weak)) void *dlsym(void *handle, const char *symbol);

#ifndef APPID
#define APPID "com.kekko.steamview"
#endif

/* Serializes plane open/feed/close between the main loop (menu
 * repeat-feed) and the session worker (stream open + submits). Without
 * this the first stream open can race a menu feed and corrupt the plane
 * (black first stream, works on re-pick). */
#include <pthread.h>
static pthread_mutex_t video_lock = PTHREAD_MUTEX_INITIALIZER;

static struct {
    bool plugin_ready;
    bool video_open;
    bool audio_open;
    int width, height;
    /* opus multistream decoder (same as ihsplay: Steam sends coupled
     * stereo/surround; plain opus_decode misdecodes it as noise) */
    OpusMSDecoder *opus;
    int sample_rate, channels;
    /* PCM ring: DirectAudio may consume async (after Play returns), so
     * a single reused staging buffer aliases queued packets into
     * garbage. 8 slots round-robin; pacing drops stale instead of
     * queueing seconds of delay. */
    int16_t pcm_ring[8][1152 * 8];
    unsigned pcm_slot;
    size_t pcm_unit;
    /* wall-clock pacing: audio must not run ahead of real time */
    uint64_t audio_start_ms;
    uint64_t audio_fed_samples;
} state;

/* Read the SoC machine name, used by the m3 kadp fix. */
static int read_machine_name(char *machine_name, size_t size) {
    FILE *f = fopen("/etc/prefs/properties/machineName", "r");
    if (f == NULL) return -1;
    size_t read_len = fread(machine_name, 1, size, f);
    fclose(f);
    if (read_len <= 0) return -1;
    return 0;
}

/*
 * MS_VDEC_Init in libkadaptor.so rejects H.264 on some m3 SoCs by checking
 * codecType against HEVC only. moonlight-tv patches the two comparison
 * instructions to nops so H.264 is accepted; same fix, best-effort.
 */
static void m3_kadp_fix(void) {
    char machine_name[16] = {0};
    if (read_machine_name(machine_name, sizeof(machine_name)) != 0) return;
    if (strcmp(machine_name, "m3") != 0 && strcmp(machine_name, "m3lp") != 0) return;

    if (dlsym == NULL) return;
    void *fn = dlsym(NULL, "MS_VDEC_Init");
    if (fn == NULL) return;

    const unsigned char instructions[] = {
            0x0b, 0x2a, /* cmp codecType, H264(#0xb) */
            0x18, 0xbf, /* it ne */
            0x10, 0x2a, /* cmp codecType, HEVC(#0x10) */
    };
    size_t page_size = (size_t) sysconf(_SC_PAGESIZE);
    void *page_start = (void *) ((size_t) fn & ~(page_size - 1));
    if (mprotect(page_start, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) != 0) return;
    size_t offset = 0;
    const size_t max_size = 1600;
    while (offset < max_size) {
        unsigned char *memory = memmem(fn + offset, max_size - offset, instructions, sizeof(instructions));
        if (memory == NULL) break;
        memory[0] = 0x00;
        memory[1] = 0xbf;
        memory[2] = 0x00;
        offset = (size_t) (memory - (unsigned char *) fn) + sizeof(instructions);
    }
    mprotect(page_start, page_size, PROT_READ | PROT_EXEC);
}

bool media_init(void) {
    if (state.plugin_ready) return true;
    LGNC_CALLBACKS_T callbacks = {.msgHandler = NULL};
    if (LGNC_PLUGIN_Initialize(&callbacks) != 0) return false;
    LGNC_PLUGIN_SetAppId(APPID);
    /* Reset VDEC state left by the previous app: without this the first
     * plane open displays stale decoder buffers (top-half launch glitch). */
    LGNC_DIRECTVIDEO_Close();
    LGNC_DIRECTAUDIO_Close();
    m3_kadp_fix();
    state.plugin_ready = true;
    return true;
}
/* Fit an arbitrary aspect ratio into the 1920x1080 display window. */
static void fit_video(int width, int height) {
    int scaled_height = 1920 * height / width;
    if (scaled_height == 1080) {
        _LGNC_DIRECTVIDEO_SetDisplayWindow(0, 0, 1920, 1080);
    } else if (scaled_height < 1080) {
        _LGNC_DIRECTVIDEO_SetDisplayWindow(0, (1080 - scaled_height) / 2, 1920, scaled_height);
    } else {
        int scaled_width = 1920 * 1080 / scaled_height;
        _LGNC_DIRECTVIDEO_SetDisplayWindow((1920 - scaled_width) / 2, 0, scaled_width, 1080);
    }
}

static unsigned long plane_generation = 0;

unsigned long media_plane_generation(void) {
    return plane_generation;
}

bool media_video_open(int width, int height) {
    if (!media_init()) return false;
    pthread_mutex_lock(&video_lock);
    bool ok = false;
    if (state.video_open && state.width == width && state.height == height) {
        ok = true;
    } else {
        if (state.video_open) {
            LGNC_DIRECTVIDEO_Close();
            state.video_open = false;
            /* Close is async in the VDEC driver: reopening immediately races
             * the teardown and decodes against half-dead state (top-half
             * garbage on stream/menu switches). Let it settle. */
            usleep(200000);
        }
        LGNC_VDEC_DATA_INFO_T info = {
                .width = width,
                .height = height,
                .vdecFmt = LGNC_VDEC_FMT_H264,
                .trid_type = LGNC_VDEC_3D_TYPE_NONE,
        };
        int rc = LGNC_DIRECTVIDEO_Open(&info);
        state.video_open = (rc == 0);
        state.width = width;
        state.height = height;
        if (rc == 0) {
            plane_generation++;
            fit_video(width, height);
            ok = true;
        }
    }
    pthread_mutex_unlock(&video_lock);
    return ok;
}

bool media_video_feed(const uint8_t *au, size_t len) {
    if (!state.video_open || !au) return false;
    extern void osd_trace(const char *fmt, ...);
    pthread_mutex_lock(&video_lock);
    int rc = LGNC_DIRECTVIDEO_Play(au, (unsigned int) len);
    pthread_mutex_unlock(&video_lock);
    if (rc != 0) osd_trace("Play FAILED rc=%d len=%u", rc, (unsigned) len);
    return rc == 0;
}

void media_video_close(void) {
    pthread_mutex_lock(&video_lock);
    if (state.video_open) {
        LGNC_DIRECTVIDEO_Close();
    }
    state.video_open = false;
    pthread_mutex_unlock(&video_lock);
}

bool media_audio_open_pcm(int sample_rate, int channels) {
    if (!media_init()) return false;
    if (state.audio_open) {
        LGNC_DIRECTAUDIO_Close();
        state.audio_open = false;
    }
    if (state.opus != NULL) {
        opus_multistream_decoder_destroy(state.opus);
        state.opus = NULL;
    }
    int err = 0;
    /* ihsplay layout: mono = 1 stream; stereo = 1 coupled pair;
     * surround = N mono streams (mapping identity). */
    unsigned char mapping[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    int streams = channels, coupled = 0;
    if (channels == 2) {
        streams = 1;
        coupled = 1;
    }
    state.opus = opus_multistream_decoder_create(sample_rate, channels,
                                                 streams, coupled,
                                                 mapping, &err);
    if (state.opus == NULL || err != OPUS_OK) {
        fprintf(stderr, "Opus decoder init failed: %d\n", err);
        return false;
    }
    LGNC_ADEC_DATA_INFO_T info = {
            .codec = LGNC_ADEC_FMT_PCM,
            .AChannel = LGNC_ADEC_CH_INDEX_MAIN,
            .samplingFreq = LGNC_ADEC_SAMPLING_FREQ_OF(sample_rate),
            .numberOfChannel = (unsigned int) channels,
            .bitPerSample = 16,
    };
    if (LGNC_DIRECTAUDIO_Open(&info) != 0) {
        opus_multistream_decoder_destroy(state.opus);
        state.opus = NULL;
        return false;
    }
    state.audio_open = true;
    state.sample_rate = sample_rate;
    state.channels = channels;
    state.pcm_unit = (size_t) channels * sizeof(int16_t);
    state.pcm_slot = 0;
    state.audio_start_ms = 0;
    state.audio_fed_samples = 0;
    return true;
}

static uint64_t audio_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t) ts.tv_sec * 1000 + (uint64_t) ts.tv_nsec / 1000000;
}

bool media_audio_decode(const uint8_t *data, size_t len) {
    if (!state.audio_open) return false;
    if (state.opus == NULL) {
        /* raw PCM passthrough */
        return LGNC_DIRECTAUDIO_Play(data, (unsigned int) len) == 0;
    }
    int16_t *pcm = state.pcm_ring[state.pcm_slot];
    state.pcm_slot = (state.pcm_slot + 1) % 8;
    int samples = opus_multistream_decode(state.opus, data, (int) len,
                                          pcm, 1152, 0);
    if (samples <= 0) return false;
    /* Wall-clock pacing: never run more than 500ms ahead of real time.
     * Without this the driver queue grows unboundedly (seconds of lag);
     * with it, stale packets drop and live audio stays live. */
    uint64_t now = audio_now_ms();
    if (state.audio_fed_samples == 0) {
        state.audio_start_ms = now;
    }
    state.audio_fed_samples += (uint64_t) samples;
    uint64_t fed_ms = state.audio_fed_samples * 1000 / (uint64_t) state.sample_rate;
    uint64_t elapsed_ms = now - state.audio_start_ms;
    if (fed_ms > elapsed_ms + 500) {
        state.audio_fed_samples -= (uint64_t) samples;
        return true;
    }
    /* Resync: if we fall more than 2s behind (seek, stall), restart clock. */
    if (elapsed_ms > fed_ms + 2000) {
        state.audio_start_ms = now;
        state.audio_fed_samples = (uint64_t) samples;
    }
    /* Feed in ≤240-sample chunks (ihsplay's frame size): large single
     * Plays glitch on this driver (explosions on loud packets). */
    const uint8_t *pp = (const uint8_t *) pcm;
    int remaining = samples;
    while (remaining > 0) {
        int n = remaining > 240 ? 240 : remaining;
        if (LGNC_DIRECTAUDIO_Play(pp, (unsigned int) ((size_t) n * state.pcm_unit)) != 0) {
            return false;
        }
        pp += (size_t) n * state.pcm_unit;
        remaining -= n;
    }
    return true;
}

void media_audio_close(void) {
    if (state.audio_open) {
        LGNC_DIRECTAUDIO_Close();
    }
    state.audio_open = false;
    if (state.opus != NULL) {
        opus_multistream_decoder_destroy(state.opus);
        state.opus = NULL;
    }
}

void media_quit(void) {
    media_video_close();
    media_audio_close();
    if (state.plugin_ready) {
        LGNC_PLUGIN_Finalize();
    }
    state.plugin_ready = false;
}
