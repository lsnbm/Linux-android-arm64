/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LSDRIVER_DRM_CPU_RENDER_H
#define LSDRIVER_DRM_CPU_RENDER_H

#include <linux/types.h>

int drm_cpu_render_init(void);
int drm_cpu_render_exit(void);
int drm_cpu_render_begin_frame(void);
int drm_cpu_render_end_frame(void);
int drm_cpu_render_clear(u32 color);
int drm_cpu_render_line(int x0, int y0, int x1, int y1, u32 color, u32 thickness);
int drm_cpu_render_rect(int left, int top, int right, int bottom, u32 color, u32 thickness, bool filled);
int drm_cpu_render_circle(int center_x, int center_y, int radius, u32 color, u32 thickness, bool filled);
int drm_cpu_render_text(int left, int top, const char *text, u32 scale, u32 color);

#endif