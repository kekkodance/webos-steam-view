/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef STEAMVIEW_OSD_FB_H
#define STEAMVIEW_OSD_FB_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define OSD_W 640
#define OSD_H 360

bool osd_fb_init(void);
void osd_fb_quit(void);

/* Render a menu screen into the framebuffer. */
void osd_fb_show(const char *title, const char **lines, int nlines, int selected);

/* Read back the I420 planes (valid after osd_fb_show). */
void osd_fb_planes(const uint8_t **y, const uint8_t **u, const uint8_t **v);

#endif
