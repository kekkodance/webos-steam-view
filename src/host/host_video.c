/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Host video: SDL window, OpenH264 decodes stream AUs to I420, uploaded to
 * an SDL texture. OSD menu frames blit straight from the shared framebuffer
 * (no encode needed on host).
 */
#include "host_video.h"

#include "osd_fb.h"

#include <wels/codec_api.h>
#include <SDL.h>

#include <string.h>

static SDL_Window *win = NULL;
static SDL_Renderer *ren = NULL;
static SDL_Texture *tex = NULL;
static int tex_w = 0, tex_h = 0;
static ISVCDecoder *decoder = NULL;

bool host_video_init(void) {
    if (win != NULL) return true;
    win = SDL_CreateWindow("Steam View",
                           SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
                           1280, 720, SDL_WINDOW_RESIZABLE);
    if (win == NULL) return false;
    ren = SDL_CreateRenderer(win, -1, 0);
    if (ren == NULL) return false;
    return true;
}

void host_video_quit(void) {
    if (tex != NULL) SDL_DestroyTexture(tex);
    tex = NULL;
    if (ren != NULL) SDL_DestroyRenderer(ren);
    ren = NULL;
    if (win != NULL) SDL_DestroyWindow(win);
    win = NULL;
    if (decoder != NULL) {
        (*decoder)->Uninitialize(decoder);
        WelsDestroyDecoder(decoder);
        decoder = NULL;
    }
}

static bool ensure_tex(int w, int h) {
    if (tex != NULL && tex_w == w && tex_h == h) return true;
    if (tex != NULL) SDL_DestroyTexture(tex);
    tex = SDL_CreateTexture(ren, SDL_PIXELFORMAT_IYUV,
                            SDL_TEXTUREACCESS_STREAMING, w, h);
    if (tex == NULL) return false;
    tex_w = w;
    tex_h = h;
    return true;
}

/* OSD menu frame -> window. */
void host_video_osd(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                    int w, int h) {
    if (ren == NULL || !ensure_tex(w, h)) return;
    SDL_UpdateYUVTexture(tex, NULL, y, w, u, w / 2, v, w / 2);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
}

/* Stream AU -> decode -> window. Returns false on transient failure (caller
 * reports the frame lost so ihslib waits for a keyframe). */
bool host_video_feed(const uint8_t *au, size_t len, int *out_w, int *out_h) {
    if (decoder == NULL) {
        if (WelsCreateDecoder(&decoder) != 0 || decoder == NULL) return false;
        SDecodingParam param;
        memset(&param, 0, sizeof(param));
        param.eEcActiveIdc = ERROR_CON_SLICE_COPY;
        param.sVideoProperty.eVideoBsType = VIDEO_BITSTREAM_AVC;
        if ((*decoder)->Initialize(decoder, &param) != cmResultSuccess) return false;
    }
    uint8_t *dst[3] = {NULL, NULL, NULL};
    SBufferInfo info;
    memset(&info, 0, sizeof(info));
    DECODING_STATE state = (*decoder)->DecodeFrame2(decoder, au, (int) len, dst, &info);
    if (state != dsErrorFree) return false;
    if (info.iBufferStatus != 1) return true; /* no displayable frame yet */
    int w = (int) info.UsrData.sSystemBuffer.iWidth;
    int h = (int) info.UsrData.sSystemBuffer.iHeight;
    if (ren == NULL || !ensure_tex(w, h)) return false;
    SDL_UpdateYUVTexture(tex, NULL,
                         dst[0], info.UsrData.sSystemBuffer.iStride[0],
                         dst[1], info.UsrData.sSystemBuffer.iStride[1],
                         dst[2], info.UsrData.sSystemBuffer.iStride[1]);
    SDL_RenderClear(ren);
    SDL_RenderCopy(ren, tex, NULL, NULL);
    SDL_RenderPresent(ren);
    if (out_w != NULL) *out_w = w;
    if (out_h != NULL) *out_h = h;
    return true;
}
