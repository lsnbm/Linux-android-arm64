/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LSDRIVER_CPU_RENDER_H
#define LSDRIVER_CPU_RENDER_H

#include <linux/types.h>

enum cpu_render_platform {
    CPU_RENDER_PLATFORM_UNKNOWN = 0,
    CPU_RENDER_PLATFORM_MSM,
    CPU_RENDER_PLATFORM_MEDIATEK,
};

struct cpu_render_ops {
    enum cpu_render_platform platform;
    const char *name;
    int (*init)(void);
    void (*exit)(void);
    int (*clear)(u32 color);
    int (*line)(int x0, int y0, int x1, int y1, u32 color, u32 thickness);
    int (*rect)(int left, int top, int right, int bottom, u32 color,
                u32 thickness, bool filled);
    int (*circle)(int center_x, int center_y, int radius, u32 color,
                  u32 thickness, bool filled);
    int (*text)(int left, int top, const char *text, u32 scale, u32 color);
};

int cpu_render_init(void);
void cpu_render_exit(void);
enum cpu_render_platform cpu_render_platform(void);
int cpu_render_clear(u32 color);
int cpu_render_line(int x0, int y0, int x1, int y1, u32 color,
                    u32 thickness);
int cpu_render_rect(int left, int top, int right, int bottom, u32 color,
                    u32 thickness, bool filled);
int cpu_render_circle(int center_x, int center_y, int radius, u32 color,
                      u32 thickness, bool filled);
int cpu_render_text(int left, int top, const char *text, u32 scale,
                    u32 color);

const struct cpu_render_ops *msm_cpu_render_get_ops(void);
const struct cpu_render_ops *mtk_cpu_render_get_ops(void);

#endif