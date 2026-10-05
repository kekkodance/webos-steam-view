/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Shared OSD framebuffer: 5x7 text into I420. Both platforms render menus
 * here; TV encodes it to H.264 (osd present), host blits it to SDL.
 */
#include "osd_fb.h"
#include "font5x7_data.h"

#include <string.h>
#include <stdlib.h>

static uint8_t *frame_y = NULL;
static uint8_t *frame_uv = NULL;

bool osd_fb_init(void) {
    if (frame_y != NULL) return true;
    frame_y = malloc(OSD_W * OSD_H);
    frame_uv = malloc(OSD_W * OSD_H / 2);
    if (frame_y == NULL || frame_uv == NULL) return false;
    return true;
}

void osd_fb_quit(void) {
    free(frame_y);
    free(frame_uv);
    frame_y = NULL;
    frame_uv = NULL;
}

static void osd_clear(uint8_t bg) {
    memset(frame_y, bg, OSD_W * OSD_H);
    memset(frame_uv, 128, OSD_W * OSD_H / 2);
}

static void osd_text(const char *text, int x, int y, uint8_t fg) {
    for (const char *p = text; *p; p++) {
        unsigned char c = (unsigned char) *p;
        if (c < 0x20 || c > 0x7e) c = '?';
        const unsigned char *glyph = FONT5X7[c - 0x20];
        for (int col = 0; col < 5; col++) {
            unsigned char bits = glyph[col];
            for (int row = 0; row < 7; row++) {
                if (bits & (1 << row)) {
                    int px = x + col, py = y + row;
                    if (px >= 0 && px < OSD_W && py >= 0 && py < OSD_H) {
                        frame_y[py * OSD_W + px] = fg;
                    }
                }
            }
        }
        x += 6;
    }
}

/* Screens are plain line lists; selection highlighted by inversion. */
void osd_fb_show(const char *title, const char **lines, int nlines, int selected) {
    if (!osd_fb_init()) return;
    osd_clear(16);
    osd_text(title, 32, 24, 235);
    for (int i = 0; i < nlines && i < 12; i++) {
        int y = 64 + i * 20;
        if (i == selected) {
            for (int x = 24; x < OSD_W - 24; x++) {
                for (int r = 0; r < 12; r++) {
                    frame_y[(y - 2 + r) * OSD_W + x] = 90;
                }
            }
            osd_text(lines[i], 32, y, 255);
        } else {
            osd_text(lines[i], 32, y, 180);
        }
    }
}

void osd_fb_planes(const uint8_t **y, const uint8_t **u, const uint8_t **v) {
    if (y != NULL) *y = frame_y;
    if (u != NULL) *u = frame_uv;
    if (v != NULL) *v = frame_uv != NULL ? frame_uv + OSD_W * OSD_H / 4 : NULL;
}
