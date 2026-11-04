/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <drm/drm_atomic.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_plane.h>

#include "../../export_fun.h"
#include "../../klog/lsdriver_log.h"
#include "cpu_raster.h"
#include "drm_cpu_render.h"

#define DRM_CPU_MINOR 0U
#define DRM_CPU_OVERLAY_WIDTH 640U
#define DRM_CPU_OVERLAY_HEIGHT 640U
#define DRM_CPU_OVERLAY_PITCH (DRM_CPU_OVERLAY_WIDTH * sizeof(u32))

struct drm_cpu_render_api
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    struct drm_minor *(*minor_acquire)(struct xarray *, unsigned int);
    struct xarray *minors_xa;
#else
    struct drm_minor *(*minor_acquire)(unsigned int);
#endif
    void (*minor_release)(struct drm_minor *);
    void *(*msm_gem_get_vaddr)(struct drm_gem_object *);
    void (*msm_gem_put_vaddr)(struct drm_gem_object *);
};

struct drm_cpu_render_context
{
    struct drm_minor *minor;
    struct drm_device *dev;
    void *overlay_pixels;
    struct drm_framebuffer *active_fb;
    struct mutex draw_lock;
    const struct drm_mode_config_funcs *original_funcs;
    struct drm_mode_config_funcs hooked_funcs;
    bool attached;
    bool hook_installed;
    u64 overlay_generation;
    u64 presented_generation;
    bool frame_active;
    struct task_struct *frame_owner;
};

static struct drm_cpu_render_context drm_cpu_render;
static struct drm_cpu_render_api drm_cpu_render_api;

static void set_active_framebuffer(struct drm_framebuffer *fb)
{
    if (fb == drm_cpu_render.active_fb) return;
    if (fb) drm_framebuffer_get(fb);
    if (drm_cpu_render.active_fb) drm_framebuffer_put(drm_cpu_render.active_fb);
    drm_cpu_render.active_fb = fb;
    drm_cpu_render.presented_generation = 0;
}

static int framebuffer_swap_channels(const struct drm_framebuffer *fb)
{
    if (!fb || !fb->format) return -EINVAL;
    switch (fb->format->format)
    {
    case DRM_FORMAT_ARGB8888:
    case DRM_FORMAT_XRGB8888:
        return 0;
    case DRM_FORMAT_ABGR8888:
    case DRM_FORMAT_XBGR8888:
        return 1;
    default:
        return -EOPNOTSUPP;
    }
}

static struct drm_framebuffer *find_primary_framebuffer(struct drm_atomic_state *state)
{
    struct drm_plane *plane;

    drm_for_each_plane(plane, drm_cpu_render.dev)
    {
        struct drm_plane_state *plane_state = state ?
                drm_atomic_get_new_plane_state(state, plane) : plane->state;
        struct drm_framebuffer *fb;

        if (plane->type != DRM_PLANE_TYPE_PRIMARY || !plane_state ||
            !plane_state->crtc)
            continue;
        fb = plane_state->fb;
        if (!fb || !fb->obj[0] || framebuffer_swap_channels(fb) < 0 ||
            fb->modifier != DRM_FORMAT_MOD_LINEAR ||
            fb->pitches[0] < fb->width * 4)
            continue;
        return fb;
    }
    return NULL;
}

static int blend_into_framebuffer(struct drm_framebuffer *fb)
{
    struct drm_gem_object *object;
    void *pixels;
    int swap_channels;

    if (!fb || !fb->obj[0]) return -ENODEV;
    swap_channels = framebuffer_swap_channels(fb);
    if (swap_channels < 0) return swap_channels;
    if (fb->modifier != DRM_FORMAT_MOD_LINEAR || fb->pitches[0] < fb->width * 4)
        return -EOPNOTSUPP;

    object = fb->obj[0];
    pixels = drm_cpu_render_api.msm_gem_get_vaddr(object);
    if (IS_ERR_OR_NULL(pixels)) return pixels ? PTR_ERR(pixels) : -ENOMEM;
    cpu_raster_blend_format((u8 *)pixels + fb->offsets[0], fb->width,
                            fb->height, fb->pitches[0],
                            drm_cpu_render.overlay_pixels,
                            DRM_CPU_OVERLAY_WIDTH, DRM_CPU_OVERLAY_HEIGHT,
                            DRM_CPU_OVERLAY_PITCH,
                            ((int)fb->width - DRM_CPU_OVERLAY_WIDTH) / 2,
                            ((int)fb->height - DRM_CPU_OVERLAY_HEIGHT) / 2,
                            swap_channels != 0);
    wmb();
    drm_cpu_render_api.msm_gem_put_vaddr(object);
    return 0;
}

static int drm_cpu_render_atomic_commit(struct drm_device *dev,
                                        struct drm_atomic_state *state,
                                        bool nonblock)
{
    bool locked = dev == drm_cpu_render.dev && READ_ONCE(drm_cpu_render.attached);
    int ret;

    if (locked)
    {
        struct drm_framebuffer *fb;

        mutex_lock(&drm_cpu_render.draw_lock);
        fb = find_primary_framebuffer(state);
        if (fb)
        {
            set_active_framebuffer(fb);
            if (drm_cpu_render.presented_generation != drm_cpu_render.overlay_generation &&
                !blend_into_framebuffer(fb))
                drm_cpu_render.presented_generation = drm_cpu_render.overlay_generation;
        }
        else set_active_framebuffer(NULL);
    }
    ret = drm_cpu_render.original_funcs->atomic_commit(dev, state, nonblock);
    if (locked) mutex_unlock(&drm_cpu_render.draw_lock);
    return ret;
}

static int release_output(void)
{
    if (drm_cpu_render.hook_installed)
    {
        if (cmpxchg(&drm_cpu_render.dev->mode_config.funcs,
                    &drm_cpu_render.hooked_funcs,
                    drm_cpu_render.original_funcs) != &drm_cpu_render.hooked_funcs)
            return -EBUSY;
        drm_cpu_render.hook_installed = false;
        module_put(THIS_MODULE);
    }
    drm_cpu_render.attached = false;
    set_active_framebuffer(NULL);
    vfree(drm_cpu_render.overlay_pixels);
    drm_cpu_render.overlay_pixels = NULL;
    if (drm_cpu_render.minor)
        drm_cpu_render_api.minor_release(drm_cpu_render.minor);
    drm_cpu_render.minor = NULL;
    drm_cpu_render.dev = NULL;
    drm_cpu_render.original_funcs = NULL;
    return 0;
}

static int acquire_output(void)
{
    int ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    drm_cpu_render.minor = drm_cpu_render_api.minor_acquire(drm_cpu_render_api.minors_xa, DRM_CPU_MINOR);
#else
    drm_cpu_render.minor = drm_cpu_render_api.minor_acquire(DRM_CPU_MINOR);
#endif
    if (IS_ERR(drm_cpu_render.minor))
    {
        ret = PTR_ERR(drm_cpu_render.minor);
        drm_cpu_render.minor = NULL;
        return ret;
    }
    drm_cpu_render.dev = drm_cpu_render.minor->dev;
    if (strcmp(drm_cpu_render.dev->driver->name, "msm") && strcmp(drm_cpu_render.dev->driver->name, "msm_drm"))
    {
        ret = -ENODEV;
        goto fail;
    }
    drm_cpu_render.original_funcs = READ_ONCE(drm_cpu_render.dev->mode_config.funcs);
    if (!drm_cpu_render.original_funcs || !drm_cpu_render.original_funcs->atomic_commit)
    {
        ret = -EOPNOTSUPP;
        goto fail;
    }
    drm_cpu_render.overlay_pixels = vzalloc((size_t)DRM_CPU_OVERLAY_PITCH * DRM_CPU_OVERLAY_HEIGHT);
    if (!drm_cpu_render.overlay_pixels)
    {
        ret = -ENOMEM;
        goto fail;
    }
    if (!try_module_get(THIS_MODULE))
    {
        ret = -ENODEV;
        goto fail;
    }
    drm_cpu_render.hooked_funcs = *drm_cpu_render.original_funcs;
    drm_cpu_render.hooked_funcs.atomic_commit = drm_cpu_render_atomic_commit;
    if (cmpxchg(&drm_cpu_render.dev->mode_config.funcs,
                drm_cpu_render.original_funcs,
                &drm_cpu_render.hooked_funcs) != drm_cpu_render.original_funcs)
    {
        module_put(THIS_MODULE);
        ret = -EBUSY;
        goto fail;
    }
    drm_cpu_render.hook_installed = true;

    drm_modeset_lock_all(drm_cpu_render.dev);
    {
        struct drm_framebuffer *fb = find_primary_framebuffer(NULL);

        set_active_framebuffer(fb);
        if (!fb)
            ls_log_always_tag("cpu_render", "active primary framebuffer unavailable, ret=%d\n", -EOPNOTSUPP);
    }
    drm_modeset_unlock_all(drm_cpu_render.dev);
    drm_cpu_render.attached = true;
    return 0;
fail:
    {
        int cleanup_ret = release_output();

        if (cleanup_ret)
            ls_log_always_tag("cpu_render", "Direct framebuffer cleanup deferred, ret=%d\n", cleanup_ret);
    }
    return ret;
}

int drm_cpu_render_init(void)
{
    int ret = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    drm_cpu_render_api.minors_xa = (struct xarray *)generic_kallsyms_lookup_name("drm_minors_xa");
    if (!drm_cpu_render_api.minors_xa) ret = -ENOENT;
#endif
    if (!ret)
    {
        drm_cpu_render_api.minor_acquire = (typeof(drm_cpu_render_api.minor_acquire))generic_kallsyms_lookup_name("drm_minor_acquire");
        drm_cpu_render_api.minor_release = (typeof(drm_cpu_render_api.minor_release))generic_kallsyms_lookup_name("drm_minor_release");
        drm_cpu_render_api.msm_gem_get_vaddr = (typeof(drm_cpu_render_api.msm_gem_get_vaddr))generic_kallsyms_lookup_name("msm_gem_get_vaddr");
        drm_cpu_render_api.msm_gem_put_vaddr = (typeof(drm_cpu_render_api.msm_gem_put_vaddr))generic_kallsyms_lookup_name("msm_gem_put_vaddr");
        if (!drm_cpu_render_api.minor_acquire || !drm_cpu_render_api.minor_release ||
            !drm_cpu_render_api.msm_gem_get_vaddr || !drm_cpu_render_api.msm_gem_put_vaddr)
            ret = -ENOENT;
    }
    if (ret)
    {
        ls_log_always_tag("cpu_render", "Direct framebuffer symbol resolution failed, ret=%d\n", ret);
        return ret;
    }
    mutex_init(&drm_cpu_render.draw_lock);
    ret = acquire_output();
    if (ret)
    {
        ls_log_always_tag("cpu_render", "Direct framebuffer initialization failed, ret=%d\n", ret);
        return ret;
    }
    return 0;
}

static int drm_cpu_require_frame(void)
{
    if (!READ_ONCE(drm_cpu_render.attached) || !drm_cpu_render.overlay_pixels) return -ENODEV;
    if (!READ_ONCE(drm_cpu_render.frame_active)) return -EINVAL;
    return READ_ONCE(drm_cpu_render.frame_owner) == current ? 0 : -EBUSY;
}

int drm_cpu_render_begin_frame(void)
{
    if (!READ_ONCE(drm_cpu_render.attached) || !drm_cpu_render.overlay_pixels) return -ENODEV;
    mutex_lock(&drm_cpu_render.draw_lock);
    if (!READ_ONCE(drm_cpu_render.attached) || !drm_cpu_render.overlay_pixels)
    {
        mutex_unlock(&drm_cpu_render.draw_lock);
        return -ENODEV;
    }
    if (drm_cpu_render.frame_active)
    {
        mutex_unlock(&drm_cpu_render.draw_lock);
        return -EBUSY;
    }
    drm_cpu_render.frame_active = true;
    drm_cpu_render.frame_owner = current;
    return 0;
}

int drm_cpu_render_end_frame(void)
{
    struct drm_framebuffer *fb;
    u64 generation;
    int blend_ret = 0;
    int ret = 0;

    if (!READ_ONCE(drm_cpu_render.frame_active) ||
        READ_ONCE(drm_cpu_render.frame_owner) != current)
        return -EINVAL;

    wmb();
    generation = ++drm_cpu_render.overlay_generation;
    fb = drm_cpu_render.active_fb;
    if (!fb)
    {
        drm_cpu_render.frame_active = false;
        drm_cpu_render.frame_owner = NULL;
        mutex_unlock(&drm_cpu_render.draw_lock);
        return -ENODEV;
    }
    drm_framebuffer_get(fb);
    mutex_unlock(&drm_cpu_render.draw_lock);

    if (fb->funcs && fb->funcs->dirty)
        ret = fb->funcs->dirty(fb, NULL, 0, 0, NULL, 0);
    else
        ret = -EOPNOTSUPP;

    mutex_lock(&drm_cpu_render.draw_lock);
    if (drm_cpu_render.presented_generation != generation &&
        drm_cpu_render.active_fb)
        blend_ret = blend_into_framebuffer(drm_cpu_render.active_fb);
    if (!blend_ret) drm_cpu_render.presented_generation = generation;
    mutex_unlock(&drm_cpu_render.draw_lock);
    drm_framebuffer_put(fb);
    if (ret && !blend_ret)
        ls_log_always_tag("cpu_render", "framebuffer dirty present failed, ret=%d\n", ret);
    if (!blend_ret) ret = 0;

    mutex_lock(&drm_cpu_render.draw_lock);
    drm_cpu_render.frame_active = false;
    drm_cpu_render.frame_owner = NULL;
    mutex_unlock(&drm_cpu_render.draw_lock);
    return blend_ret ? blend_ret : ret;
}

int drm_cpu_render_clear(u32 color)
{
    int ret = drm_cpu_require_frame();

    if (ret) return ret;
    cpu_raster_clear(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                     DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH, color);
    return 0;
}

int drm_cpu_render_line(int x0, int y0, int x1, int y1, u32 color, u32 thickness)
{
    int ret = drm_cpu_require_frame();

    if (ret) return ret;
    cpu_raster_line(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                    DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH,
                    x0, y0, x1, y1, color, thickness);
    return 0;
}

int drm_cpu_render_rect(int left, int top, int right, int bottom, u32 color, u32 thickness, bool filled)
{
    int ret = drm_cpu_require_frame();

    if (ret) return ret;
    cpu_raster_rect(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                    DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH,
                    left, top, right, bottom, color, thickness, filled);
    return 0;
}

int drm_cpu_render_circle(int center_x, int center_y, int radius, u32 color, u32 thickness, bool filled)
{
    int ret = drm_cpu_require_frame();

    if (ret) return ret;
    cpu_raster_circle(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                      DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH,
                      center_x, center_y, radius, color, thickness, filled);
    return 0;
}

int drm_cpu_render_text(int left, int top, const char *text, u32 scale, u32 color)
{
    int ret;

    if (!text) return -EINVAL;
    ret = drm_cpu_require_frame();
    if (ret) return ret;
    cpu_raster_text(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                    DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH,
                    left, top, text, scale, color);
    return 0;
}

int drm_cpu_render_exit(void)
{
    int ret;

    if (!drm_cpu_render.minor && !drm_cpu_render.hook_installed && !drm_cpu_render.overlay_pixels)
        return 0;
    mutex_lock(&drm_cpu_render.draw_lock);
    WRITE_ONCE(drm_cpu_render.attached, false);
    mutex_unlock(&drm_cpu_render.draw_lock);
    ret = release_output();
    if (ret)
    {
        WRITE_ONCE(drm_cpu_render.attached, true);
        return ret;
    }
    return 0;
}
