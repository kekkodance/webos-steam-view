/* SPDX-License-Identifier: GPL-3.0-or-later */
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
#include <opus/opus_multistream.h>
#include <sys/mman.h>

#include <dlfcn.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#ifndef APPID
#define APPID "com.kekko.steamview"
#endif

static struct {
    bool plugin_ready;
    bool video_open;
    bool audio_open;
    int width, height;
    /* opus decoder */
    OpusDecoder *opus;
    int sample_rate, channels;
    /* PCM staging buffer for one decoded packet */
    int16_t pcm[1152 * 2];
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

bool media_video_open(int width, int height) {
    if (!media_init()) return false;
    if (state.video_open && state.width == width && state.height == height) return true;
    if (state.video_open) {
        LGNC_DIRECTVIDEO_Close();
        state.video_open = false;
    }
    LGNC_VDEC_DATA_INFO_T info = {
            .width = width,
            .height = height,
            .vdecFmt = LGNC_VDEC_FMT_H264,
            .trid_type = LGNC_VDEC_3D_TYPE_NONE,
    };
    if (LGNC_DIRECTVIDEO_Open(&info) != 0) return false;
    state.video_open = true;
    state.width = width;
    state.height = height;
    fit_video(width, height);
    return true;
}

bool media_video_feed(const uint8_t *au, size_t len) {
    if (!state.video_open || !au) return false;
    return LGNC_DIRECTVIDEO_Play(au, (unsigned int) len) == 0;
}

void media_video_close(void) {
    if (state.video_open) {
        LGNC_DIRECTVIDEO_Close();
    }
    state.video_open = false;
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
    state.opus = opus_multistream_decoder_create(sample_rate, channels, 0, NULL, &err);
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
    return true;
}

bool media_audio_decode(const uint8_t *data, size_t len) {
    if (!state.audio_open) return false;
    if (state.opus == NULL) {
        /* raw PCM passthrough */
        return LGNC_DIRECTAUDIO_Play(data, (unsigned int) len) == 0;
    }
    int samples = opus_multistream_decode(state.opus, data, (int) len,
                                          state.pcm, 1152, 0);
    if (samples <= 0) return false;
    size_t pcm_bytes = (size_t) samples * (size_t) state.channels * 2;
    return LGNC_DIRECTAUDIO_Play(state.pcm, (unsigned int) pcm_bytes) == 0;
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
