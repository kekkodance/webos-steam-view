/* SPDX-License-Identifier: GPL-3.0-or-later */
/* LVGL config for Steam View: software rendering into our own surface.
 * No GPU, no EGL, no Wayland window calls  -  the ihsplay webOS 2 crash path.
 * Display binding lives in src/ui/ui_port.c (per platform). */
#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_CONF_INCLUDE_SIMPLE 1

/* Pinned fork (mariotaku/lvgl master): the dev-version #warning is noise. */
#define LV_USE_DEV_VERSION

#define LV_COLOR_DEPTH 32
#define LV_MEM_SIZE (256 * 1024U)
#define LV_USE_LOG 0

/* UI strings are UTF-8 (•,  - , …). Without this LVGL decodes text as ASCII
 * and every multibyte glyph misses the font → tofu squares. */
#define LV_TXT_ENC LV_TXT_ENC_UTF8
#define LV_FONT_MONTSERRAT_12 0
#define LV_FONT_MONTSERRAT_14 0
#define LV_FONT_MONTSERRAT_16 0
/* No Montserrat compiled in  -  Motiva (converted, assets/fonts) is the only
 * family. It must be the default, or fresh widgets deref NULL fonts. */
#define LV_FONT_DEFAULT &motiva_20
#define LV_FONT_CUSTOM_DECLARE LV_FONT_DECLARE(motiva_20)

/* Converted Motiva fonts use RLE bitmaps (bitmap_format=COMPRESSED);
 * without this LVGL skips decompression and draws garbage/squares. */
#define LV_USE_FONT_COMPRESSED 1
#define LV_USE_LABEL 1
#define LV_USE_OBJ 1
#define LV_USE_STYLE 1

#define LV_USE_SDL 1
#define LV_SDL_INCLUDE_PATH <SDL.h>
#define LV_SDL_RENDER_MODE LV_DISPLAY_RENDER_MODE_DIRECT

#endif
