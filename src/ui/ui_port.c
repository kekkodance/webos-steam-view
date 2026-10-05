/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Steam View LVGL platform port  -  SDL display binding (PC + webOS).
 *
 * v9 API: lv_sdl_window_create() owns the SDL window, renderer, texture,
 * and framebuffer. No EGL, no Wayland window calls, no GPU textures  - 
 * the exact calls that SIGFPE ihsplay on webOS 2 (undefined wl_egl_*,
 * then zero-size textures). LVGL renders into its own RAM framebuffer;
 * the SDL driver blits it with SDL_UpdateTexture, which works on the
 * software renderer our backport provides.
 *
 * Remote input: arrows + OK drive LVGL keypad groups (registered here,
 * fed from main.c's SDL_KEYDOWN path).
 */
#include "ui.h"

#include <lvgl.h>
#include <src/dev/sdl/lv_sdl_window.h>
#include <src/dev/sdl/lv_sdl_keyboard.h>

#include <SDL.h>

static lv_display_t *g_disp;
static lv_group_t *g_keys;
static lv_indev_t *g_kbdev;

void ui_port_init(int width, int height) {
    g_disp = lv_sdl_window_create(width, height);
    g_keys = lv_group_create();
    g_kbdev = lv_sdl_keyboard_create();
    lv_indev_set_group(g_kbdev, g_keys);
    lv_group_set_default(g_keys);
}

lv_group_t *ui_port_key_group(void) {
    return g_keys;
}

void ui_port_set_title(const char *title) {
    if (g_disp != NULL) lv_sdl_window_set_title(g_disp, title);
}
