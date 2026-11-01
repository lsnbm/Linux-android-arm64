/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LSDRIVER_MSM_CPU_RENDER_H
#define LSDRIVER_MSM_CPU_RENDER_H

#include <linux/types.h>
#include "../cpu_render.h"

int msm_cpu_render_init(void);
void msm_cpu_render_exit(void);
int msm_cpu_render_clear(u32 color);
int msm_cpu_render_line(int x0, int y0, int x1, int y1, u32 color,
						u32 thickness);
int msm_cpu_render_rect(int left, int top, int right, int bottom, u32 color,
						u32 thickness, bool filled);
int msm_cpu_render_circle(int center_x, int center_y, int radius, u32 color,
						  u32 thickness, bool filled);
int msm_cpu_render_text(int left, int top, const char *text, u32 scale,
						u32 color);
const struct cpu_render_ops *msm_cpu_render_get_ops(void);

#endif