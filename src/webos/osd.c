/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * TV menu present: shared OSD framebuffer, encoded to baseline H.264 with
 * OpenH264, fed to the LGNC plane as AUs.
 */
#include "osd.h"

#include "../osd_fb.h"
#include "media.h"

#include <wels/codec_api.h>

#include <string.h>
#include <stdlib.h>

void osd_show(const char *title, const char **lines, int nlines, int selected) {
    osd_fb_show(title, lines, nlines, selected);
    osd_present();
}


/* Encode one frame and push it. IDR every 30 frames keeps the plane happy
 * after signal hiccups. */
static ISVCEncoder *encoder = NULL;
static int frame_no = 0;

static bool osd_encoder_init(void) {
    if (encoder != NULL) return true;
    if (WelsCreateSVCEncoder(&encoder) != 0 || encoder == NULL) return false;
    SEncParamExt param;
    memset(&param, 0, sizeof(param));
    param.iUsageType = CAMERA_VIDEO_REAL_TIME;
    param.iPicWidth = OSD_W;
    param.iPicHeight = OSD_H;
    param.iTargetBitrate = 500000;
    param.iMaxBitrate = 800000;
    param.iRCMode = RC_BITRATE_MODE;
    param.fMaxFrameRate = 10.0f;
    param.iTemporalLayerNum = 1;
    param.iSpatialLayerNum = 1;
    param.eSpsPpsIdStrategy = CONSTANT_ID;
    param.bEnableFrameSkip = true;
    param.iEntropyCodingModeFlag = 0;
    if ((*encoder)->InitializeExt(encoder, &param) != cmResultSuccess) return false;
    return true;
}

void osd_present(void) {
    if (!osd_encoder_init()) return;
    if (!media_video_open(OSD_W, OSD_H)) return;
    const uint8_t *y, *u, *v;
    osd_fb_planes(&y, &u, &v);
    if (y == NULL || u == NULL || v == NULL) return;
    SSourcePicture pic;
    memset(&pic, 0, sizeof(pic));
    pic.iPicWidth = OSD_W;
    pic.iPicHeight = OSD_H;
    pic.iColorFormat = videoFormatI420;
    pic.iStride[0] = OSD_W;
    pic.iStride[1] = OSD_W / 2;
    pic.iStride[2] = OSD_W / 2;
    pic.pData[0] = (uint8_t *) y;
    pic.pData[1] = (uint8_t *) u;
    pic.pData[2] = (uint8_t *) v;
    SFrameBSInfo info;
    memset(&info, 0, sizeof(info));
    if (frame_no % 30 == 0) {
        (*encoder)->ForceIntraFrame(encoder, true);
    }
    if ((*encoder)->EncodeFrame(encoder, &pic, &info) != cmResultSuccess) return;
    frame_no++;
    for (int i = 0; i < info.iLayerNum; i++) {
        const SLayerBSInfo *layer = &info.sLayerInfo[i];
        for (int j = 0; j < layer->iNalCount; j++) {
            media_video_feed(layer->pBsBuf + layer->pNalLengthInByte[j],
                             layer->pNalLengthInByte[j]);
        }
    }
}
