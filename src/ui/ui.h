/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Steam View UI screens  -  LVGL 10-foot layout in Steam's visual language.
 *
 * Backend note: LVGL renders into its own framebuffer; the platform layer
 * blits it to the SDL window (PC) or the TV surface (webOS). No EGL, no
 * Wayland window calls, no GPU textures  -  the exact calls that SIGFPE
 * ihsplay on webOS 2. This file only builds widgets and layout.
 */
#ifndef STEAMVIEW_UI_H
#define STEAMVIEW_UI_H

#include "../app.h"

typedef enum UiScreen {
    UI_HOSTS,     /* host picker cards */
    UI_PAIRING,   /* big PIN digits */
    UI_STREAMING, /* minimal overlay (video is on the LGNC plane) */
    UI_ERROR,
} UiScreen;

void ui_init(void);                        /* lv_init + display + theme */
void ui_show(UiScreen screen, App *app);    /* rebuild active screen */
void ui_tick(uint32_t ms);                  /* lv_timer_handler wrapper */
void ui_set_hosts(App *app);                /* refresh host cards */
void ui_set_status(const char *text);       /* status/footer line */
void ui_set_pin(const char *pin);           /* pairing digits */
void ui_set_stats(int w, int h, unsigned long frames, unsigned long audio);

#endif
