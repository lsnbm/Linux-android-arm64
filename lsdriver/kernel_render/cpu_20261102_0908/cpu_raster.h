/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LSDRIVER_CPU_RASTER_H
#define LSDRIVER_CPU_RASTER_H

#include <linux/types.h>

void cpu_raster_clear(void *pixels, u32 width, u32 height, u32 pitch, u32 color);
void cpu_raster_line(void *pixels, u32 width, u32 height, u32 pitch, int x0, int y0, int x1, int y1, u32 color, u32 thickness);
void cpu_raster_rect(void *pixels, u32 width, u32 height, u32 pitch, int left, int top, int right, int bottom, u32 color, u32 thickness, bool filled);
void cpu_raster_circle(void *pixels, u32 width, u32 height, u32 pitch, int center_x, int center_y, int radius, u32 color, u32 thickness, bool filled);
void cpu_raster_text(void *pixels, u32 width, u32 height, u32 pitch, int left, int top, const char *text, u32 scale, u32 color);
void cpu_raster_demo(void *pixels, u32 width, u32 height, u32 pitch);

#endif