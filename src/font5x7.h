/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * 5x7 bitmap font (public domain, classic style), rendered into SDL_Rect
 * cells of 6x8. Covers ASCII 0x20-0x7E. Enough for status text.
 */
#ifndef STEAMVIEW_FONT5X7_H
#define STEAMVIEW_FONT5X7_H

#include <SDL.h>

/* Draw text at (x, y) with 6x8 cells. Returns advanced x. */
int font5x7_draw(SDL_Renderer *r, const char *text, int x, int y, SDL_Color color);

/* Measure width in pixels. */
int font5x7_width(const char *text);

#endif
