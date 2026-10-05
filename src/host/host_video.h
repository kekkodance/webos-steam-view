/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef STEAMVIEW_HOST_VIDEO_H
#define STEAMVIEW_HOST_VIDEO_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

bool host_video_init(void);
void host_video_quit(void);

/* OSD menu frame (I420 planes) -> window. */
void host_video_osd(const uint8_t *y, const uint8_t *u, const uint8_t *v,
                    int w, int h);

/* Stream AU -> decode -> window.[out] dimensions on success. */
bool host_video_feed(const uint8_t *au, size_t len, int *out_w, int *out_h);

#endif
