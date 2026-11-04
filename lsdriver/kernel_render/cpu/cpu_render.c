/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/errno.h>
#include <linux/mutex.h>

#include "../../klog/lsdriver_log.h"
#include "cpu_render.h"
#include "drm_cpu_render.h"

static bool cpu_render_active;
static DEFINE_MUTEX(cpu_render_control_lock);

int cpu_render_init(void)
{
    int ret;

    mutex_lock(&cpu_render_control_lock);
    if (cpu_render_active)
    {
        ls_log_always_tag("cpu_render", "control init rejected: renderer already active, ret=%d\n", -EBUSY);
        mutex_unlock(&cpu_render_control_lock);
        return -EBUSY;
    }
    ret = drm_cpu_render_init();
    if (ret)
    {
        ls_log_always_tag("cpu_render", "backend init failed, ret=%d\n", ret);
        mutex_unlock(&cpu_render_control_lock);
        return ret;
    }
    WRITE_ONCE(cpu_render_active, true);
    mutex_unlock(&cpu_render_control_lock);
    return 0;
}

int cpu_render_exit(void)
{
    int ret;

    mutex_lock(&cpu_render_control_lock);
    ret = drm_cpu_render_exit();
    if (ret)
    {
        mutex_unlock(&cpu_render_control_lock);
        return ret;
    }
    WRITE_ONCE(cpu_render_active, false);
    mutex_unlock(&cpu_render_control_lock);
    return 0;
}

int cpu_render_begin_frame(void)
{
    return READ_ONCE(cpu_render_active) ? drm_cpu_render_begin_frame() :
                                           -ENODEV;
}

int cpu_render_end_frame(void)
{
    return READ_ONCE(cpu_render_active) ? drm_cpu_render_end_frame() :
                                           -ENODEV;
}

int cpu_render_clear(u32 color)
{
    return READ_ONCE(cpu_render_active) ? drm_cpu_render_clear(color) :
                                           -ENODEV;
}

int cpu_render_line(int x0, int y0, int x1, int y1, u32 color,
                    u32 thickness)
{
    return READ_ONCE(cpu_render_active) ?
            drm_cpu_render_line(x0, y0, x1, y1, color, thickness) : -ENODEV;
}

int cpu_render_rect(int left, int top, int right, int bottom, u32 color,
                    u32 thickness, bool filled)
{
    return READ_ONCE(cpu_render_active) ?
            drm_cpu_render_rect(left, top, right, bottom, color, thickness,
                                filled) : -ENODEV;
}

int cpu_render_circle(int center_x, int center_y, int radius, u32 color,
                      u32 thickness, bool filled)
{
    return READ_ONCE(cpu_render_active) ?
            drm_cpu_render_circle(center_x, center_y, radius, color,
                                  thickness, filled) : -ENODEV;
}

int cpu_render_text(int left, int top, const char *text, u32 scale,
                    u32 color)
{
    return READ_ONCE(cpu_render_active) ?
            drm_cpu_render_text(left, top, text, scale, color) : -ENODEV;
}