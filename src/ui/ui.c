/* SPDX-License-Identifier: GPL-3.0-or-later */
/*
 * Steam View UI  -  LVGL screens in Steam's visual language.
 *
 * Each screen is a plain function building LVGL widgets; the app calls
 * ui_show() on state transitions and ui_set_* for live updates. Input
 * stays in main.c (remote arrows + OK drive LVGL groups).
 *
 * Steam patterns used: full-bleed dark gradient backdrop, 66px-style
 * header bar, .block cards with 25px headers, green CTA buttons with
 * 2px radius, small_cap host rows with focus border.
 */
#include "ui.h"
#include "theme.h"

#include <lvgl.h>

#include <stdio.h>
#include <string.h>

/* Motiva Sans, converted with lv_font_conv (see assets/fonts/). */
LV_FONT_DECLARE(motiva_20);
LV_FONT_DECLARE(motiva_28);
LV_FONT_DECLARE(motiva_40);
LV_FONT_DECLARE(motiva_medium_28);
LV_FONT_DECLARE(motiva_bold_28);
LV_FONT_DECLARE(motiva_pin_96);

static lv_obj_t *status_label;
static lv_obj_t *body_container;
static lv_obj_t *pin_label;
static lv_obj_t *stats_label;

static uint32_t rgb(uint32_t hex) {
    return hex;
}

static void backdrop(lv_obj_t *scr) {
    static lv_style_t bg;
    lv_style_init(&bg);
    lv_style_set_bg_color(&bg, lv_color_hex(FV_BG_BOTTOM));
    lv_style_set_bg_grad_color(&bg, lv_color_hex(FV_BG_TOP));
    lv_style_set_bg_grad_dir(&bg, LV_GRAD_DIR_VER);
    lv_obj_add_style(scr, &bg, 0);
    (void) rgb;
}

static void header_bar(lv_obj_t *scr, const char *title) {
    /* The 1280x720 surface is ours: kill LVGL's default screen/container
     * padding + scrollbars (the phantom right inset and stray bottom line
     * in screenshots). Children are positioned absolutely. */
    lv_obj_set_style_pad_all(scr, 0, 0);
    lv_obj_set_scrollbar_mode(scr, LV_SCROLLBAR_MODE_OFF);
    lv_obj_t *bar = lv_obj_create(scr);
    lv_obj_set_size(bar, FV_W, FV_HEADER_H);
    lv_obj_set_pos(bar, 0, 0);
    lv_obj_set_style_bg_color(bar, lv_color_hex(0x171D25), 0);
    lv_obj_set_style_border_width(bar, 0, 0);
    lv_obj_set_style_radius(bar, 0, 0);
    lv_obj_set_style_pad_all(bar, 0, 0);
    lv_obj_set_scrollbar_mode(bar, LV_SCROLLBAR_MODE_OFF);

    lv_obj_t *t = lv_label_create(bar);
    lv_label_set_text(t, title);
    lv_obj_set_style_text_font(t, &motiva_bold_28, 0);
    lv_obj_set_style_text_color(t, lv_color_hex(FV_TEXT_BRIGHT), 0);
    lv_obj_align(t, LV_ALIGN_LEFT_MID, FV_GUTTER, 0);

    lv_obj_t *sub = lv_label_create(bar);
    lv_label_set_text(sub, "STEAM REMOTE VIEW");
    lv_obj_set_style_text_font(sub, &motiva_20, 0);
    lv_obj_set_style_text_color(sub, lv_color_hex(FV_BLUE), 0);
    lv_obj_align(sub, LV_ALIGN_RIGHT_MID, -FV_GUTTER, 0);
}

static void footer_hint(lv_obj_t *scr, const char *text) {
    status_label = lv_label_create(scr);
    lv_label_set_text(status_label, text);
    lv_obj_set_style_text_font(status_label, &motiva_20, 0);
    lv_obj_set_style_text_color(status_label, lv_color_hex(FV_TEXT_MUTED), 0);
    lv_obj_align(status_label, LV_ALIGN_BOTTOM_MID, 0, -40);
}

static void hosts_screen(App *app) {
    body_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(body_container, FV_W - 2 * FV_GUTTER, FV_H - FV_HEADER_H - 120);
    lv_obj_set_pos(body_container, FV_GUTTER, FV_HEADER_H + 24);
    lv_obj_set_style_bg_opa(body_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body_container, 0, 0);
    lv_obj_set_style_pad_all(body_container, 0, 0);
    lv_obj_set_scrollbar_mode(body_container, LV_SCROLLBAR_MODE_OFF);

    if (app->host_count == 0) {
        lv_obj_t *t = lv_label_create(body_container);
        lv_label_set_text(t, "Searching for Steam hosts on your network...");
        lv_obj_set_style_text_font(t, &motiva_28, 0);
        lv_obj_set_style_text_color(t, lv_color_hex(FV_TEXT_MUTED), 0);
        lv_obj_center(t);
        return;
    }
    for (int i = 0; i < app->host_count; i++) {
        bool sel = (i == app->host_selected);
        /* Real border drawn AFTER the background (border_post): stays fully
         * inside the box, so all four edges render at the same coords for
         * selected and unselected cards  -  no width-dependent clipping, and
         * the 3px focus state grows inward, not into the gutter. */
        lv_obj_t *card = lv_obj_create(body_container);
        lv_obj_set_size(card, FV_W - 2 * FV_GUTTER, 96);
        lv_obj_set_pos(card, 0, i * (96 + FV_CARD_GAP));
        lv_obj_set_style_bg_color(card, lv_color_hex(sel ? 0x2A3F5A : FV_PANEL), 0);
        lv_obj_set_style_border_width(card, sel ? 3 : 1, 0);
        lv_obj_set_style_border_color(card, lv_color_hex(sel ? FV_FOCUS : FV_BORDER), 0);
        lv_obj_set_style_border_post(card, true, 0);
        lv_obj_set_style_pad_all(card, 0, 0);
        lv_obj_set_scrollbar_mode(card, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_style_radius(card, FV_RADIUS, 0);

        lv_obj_t *name = lv_label_create(card);
        lv_label_set_text(name, app->hosts[i].name);
        lv_obj_set_style_text_font(name, &motiva_medium_28, 0);
        lv_obj_set_style_text_color(name, lv_color_hex(FV_TEXT_BRIGHT), 0);
        lv_obj_align(name, LV_ALIGN_LEFT_MID, 32, -14);

        lv_obj_t *sub = lv_label_create(card);
        lv_label_set_text(sub, sel ? "Press OK to watch" : "Steam host");
        lv_obj_set_style_text_font(sub, &motiva_20, 0);
        lv_obj_set_style_text_color(sub, lv_color_hex(sel ? FV_BLUE_LIGHT : FV_TEXT_MUTED), 0);
        lv_obj_align(sub, LV_ALIGN_LEFT_MID, 32, 22);
    }
}

static void pairing_screen(App *app) {
    body_container = lv_obj_create(lv_scr_act());
    lv_obj_set_size(body_container, FV_W - 2 * FV_GUTTER, FV_H - FV_HEADER_H - 120);
    lv_obj_set_pos(body_container, FV_GUTTER, FV_HEADER_H + 24);
    lv_obj_set_style_bg_opa(body_container, LV_OPA_TRANSP, 0);
    lv_obj_set_style_border_width(body_container, 0, 0);
    lv_obj_set_style_pad_all(body_container, 0, 0);
    lv_obj_set_scrollbar_mode(body_container, LV_SCROLLBAR_MODE_OFF);
    /* Real 96px bitmap digits (digits + dot only, ~41KB): Big Picture scale,
     * no zoom blur. Letter-spaced and centered as a unit. */
    pin_label = lv_label_create(body_container);
    lv_label_set_text(pin_label, app->pin_len > 0 ? app->pin : "....");
    lv_obj_set_style_text_font(pin_label, &motiva_pin_96, 0);
    lv_obj_set_style_text_color(pin_label, lv_color_hex(FV_TEXT_BRIGHT), 0);
    lv_obj_set_style_text_letter_space(pin_label, 24, 0);
    lv_obj_set_width(pin_label, LV_SIZE_CONTENT);
    lv_obj_align(pin_label, LV_ALIGN_CENTER, 0, -40);

    lv_obj_t *hint = lv_label_create(body_container);
    lv_label_set_text(hint, "Type this PIN into Steam on the host to pair.");
    lv_obj_set_style_text_font(hint, &motiva_20, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(FV_TEXT_MUTED), 0);
    lv_obj_align(hint, LV_ALIGN_CENTER, 0, 80);
}

static void streaming_screen(App *app) {
#ifndef TARGET_WEBOS
    (void) app;
    stats_label = lv_label_create(lv_scr_act());
    lv_label_set_text(stats_label, "");
    lv_obj_set_style_text_font(stats_label, &motiva_20, 0);
    lv_obj_set_style_text_color(stats_label, lv_color_hex(0x59BF40), 0);
    lv_obj_set_style_bg_color(stats_label, lv_color_hex(0x000000), 0);
    lv_obj_set_style_bg_opa(stats_label, LV_OPA_60, 0);
    lv_obj_align(stats_label, LV_ALIGN_BOTTOM_LEFT, FV_GUTTER, -24);
#else
    /* TV: picture fills the screen via LGNC; keep the SDL surface empty. */
    (void) app;
    stats_label = NULL;
#endif
}

void ui_init(void) {
    lv_init();
}

void ui_show(UiScreen screen, App *app) {
    /* v9: build on the display's active screen (lv_sdl_window_create makes
     * one); lv_obj_create(NULL) parents to nothing and its children crash
     * in event dispatch (NULL display lookup). Clean previous children. */
    lv_obj_t *scr = lv_scr_act();
    lv_obj_clean(scr);
    backdrop(scr);
    switch (screen) {
        case UI_HOSTS:
            header_bar(scr, "Choose a host");
            hosts_screen(app);
            /* Hint only once hosts exist; while searching the body already
             * says so. No "OK to watch"  -  the selected card says that. */
            if (app->host_count > 0) footer_hint(scr, "Up / Down to choose");
            break;
        case UI_PAIRING:
            header_bar(scr, "Pair this TV");
            pairing_screen(app);
            footer_hint(scr, "Waiting for the host...");
            break;
        case UI_STREAMING:
            streaming_screen(app);
            break;
        case UI_ERROR:
            header_bar(scr, "Something went wrong");
            footer_hint(scr, "Press Back");
            break;
    }
}

void ui_tick(uint32_t ms) {
    (void) ms;
    lv_timer_handler();
}

void ui_set_hosts(App *app) {
    ui_show(UI_HOSTS, app);
}

void ui_set_status(const char *text) {
    if (status_label != NULL) lv_label_set_text(status_label, text);
}

void ui_set_pin(const char *pin) {
    if (pin_label != NULL) lv_label_set_text(pin_label, pin);
}

void ui_set_stats(int w, int h, unsigned long frames, unsigned long audio) {
#ifndef TARGET_WEBOS
    if (stats_label == NULL) return;
    char buf[128];
    snprintf(buf, sizeof(buf), "VIDEO %dx%d  FRAMES %lu  AUDIO %lu", w, h, frames, audio);
    lv_label_set_text(stats_label, buf);
#else
    /* TV: video is on the LGNC plane  -  no overlay text over the picture.
     * Counters still live in app.frames for console logging. */
    (void) w;
    (void) h;
    (void) frames;
    (void) audio;
#endif
}
