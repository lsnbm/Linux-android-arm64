/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/errno.h>
#include <linux/mutex.h>

#include "../../klog/lsdriver_log.h"
#include "cpu_render.h"
#include "cpu_render_platform.h"

static const struct cpu_render_ops *active_ops;
static enum cpu_render_platform active_platform = CPU_RENDER_PLATFORM_UNKNOWN;
static DEFINE_MUTEX(cpu_render_control_lock);

int cpu_render_init(void)
{
    enum cpu_render_platform platform = cpu_render_platform_detect();
    const struct cpu_render_ops *ops;
    int ret;

    mutex_lock(&cpu_render_control_lock);
    if (READ_ONCE(active_ops)) {
        mutex_unlock(&cpu_render_control_lock);
        return -EBUSY;
    }
    if (platform == CPU_RENDER_PLATFORM_UNKNOWN) {
        ls_log_always_tag("cpu_render",
                          "CPU renderer platform signature is ambiguous or missing\n");
        mutex_unlock(&cpu_render_control_lock);
        return 0;
    }

    ops = platform == CPU_RENDER_PLATFORM_MSM ? msm_cpu_render_get_ops() :
                                                mtk_cpu_render_get_ops();
    if (!ops || !ops->init) {
        mutex_unlock(&cpu_render_control_lock);
        return -ENODEV;
    }
    ret = ops->init();
    if (ret) {
        mutex_unlock(&cpu_render_control_lock);
        return ret;
    }
    WRITE_ONCE(active_ops, ops);
    WRITE_ONCE(active_platform, platform);
    ls_log_always_tag("cpu_render", "selected platform: %s\n", ops->name);
    mutex_unlock(&cpu_render_control_lock);
    return 0;
}

void cpu_render_exit(void)
{
    const struct cpu_render_ops *ops;

    mutex_lock(&cpu_render_control_lock);
    ops = READ_ONCE(active_ops);
    if (ops && ops->exit)
        ops->exit();
    WRITE_ONCE(active_ops, NULL);
    WRITE_ONCE(active_platform, CPU_RENDER_PLATFORM_UNKNOWN);
    mutex_unlock(&cpu_render_control_lock);
}

enum cpu_render_platform cpu_render_platform(void)
{
    return READ_ONCE(active_platform);
}

#define CPU_RENDER_FORWARD(name, args, call) \
int cpu_render_##name args \
{ \
    const struct cpu_render_ops *ops = READ_ONCE(active_ops); \
    return ops && ops->name ? ops->name call : -ENODEV; \
}

CPU_RENDER_FORWARD(clear, (u32 color), (color));
CPU_RENDER_FORWARD(line, (int x0, int y0, int x1, int y1, u32 color,
                          u32 thickness), (x0, y0, x1, y1, color, thickness));
CPU_RENDER_FORWARD(rect, (int left, int top, int right, int bottom, u32 color,
                          u32 thickness, bool filled),
                   (left, top, right, bottom, color, thickness, filled));
CPU_RENDER_FORWARD(circle, (int center_x, int center_y, int radius, u32 color,
                            u32 thickness, bool filled),
                   (center_x, center_y, radius, color, thickness, filled));
CPU_RENDER_FORWARD(text, (int left, int top, const char *text, u32 scale,
                          u32 color), (left, top, text, scale, color));

#undef CPU_RENDER_FORWARD