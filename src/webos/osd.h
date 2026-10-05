/* SPDX-License-Identifier: GPL-3.0-or-later */
#ifndef STEAMVIEW_OSD_H
#define STEAMVIEW_OSD_H

#include <stdbool.h>

/* Full-screen menu: title + line list with highlighted selection.
 * Renders to the shared framebuffer, encodes, feeds the LGNC plane. */
void osd_show(const char *title, const char **lines, int nlines, int selected);

/* Push the current framebuffer to the LGNC plane. */
void osd_present(void);

/* Re-feed the last menu AU (keeps the VDEC pipeline fed while menus show). */
void osd_repeat(void);

/* Diagnostic trace (appends to the trace log on TV). */
void osd_trace(const char *fmt, ...);

#endif
