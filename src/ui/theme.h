/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Steam View UI theme  -  Steam store design tokens (from SteamTracking CSS).
 * Single place for the 10-foot Steam look: Big Picture uses large type,
 * generous spacing, and the store's blue/green accent language.
 */
#ifndef STEAMVIEW_UI_THEME_H
#define STEAMVIEW_UI_THEME_H

#include <stdint.h>

/* ---- Steam palette (store.steampowered.com v6 + shared_global) ---- */
#define FV_BG_TOP       0x2A475E  /* page gradient start */
#define FV_BG_BOTTOM    0x1B2838  /* page gradient end / game_bg */
#define FV_BG_DARKEST   0x000F18
#define FV_PANEL        0x262626  /* .block content */
#define FV_PANEL_2      0x303030
#define FV_BORDER       0x4D4B49
#define FV_TEXT         0xC6D4DF  /* body copy */
#define FV_TEXT_BRIGHT  0xFFFFFF  /* headings */
#define FV_TEXT_MUTED   0x8F98A0
#define FV_TEXT_FAINT   0x626366
#define FV_BLUE         0x66C0F4  /* links, accents */
#define FV_BLUE_HI      0x1A9FFF  /* primary action */
#define FV_BLUE_LIGHT   0xB3DFFF
#define FV_FOCUS        0x8BB9E0  /* hover/selected border */
#define FV_GREEN        0x5BA32B
#define FV_GREEN_HI     0x59BF40
#define FV_CTA_A        0xA4D007  /* green CTA gradient start */
#define FV_CTA_B        0x536904  /* green CTA gradient end */
#define FV_CTA_TEXT     0xD2E885
#define FV_ALERT        0xE35E1C

/* ---- 10-foot layout (1280x720 fixed surface) ---- */
#define FV_W 1280
#define FV_H 720
#define FV_GUTTER 64          /* side margins at 720p (store: 24px + 2vw) */
#define FV_HEADER_H 96        /* Steam header bar, scaled for TV */
#define FV_CARD_GAP 24        /* store gutter */
#define FV_RADIUS 4           /* store corners are 1-3px; TV gets a touch more */

/* ---- type scale (Motiva Sans; store base 12px @ desktop -> TV ~2x) ---- */
#define FV_FONT_TITLE 40      /* page headers (store h1 30px) */
#define FV_FONT_HEAD 28       /* section heads, card titles */
#define FV_FONT_BODY 20       /* body copy (store 12-14px) */

#endif
