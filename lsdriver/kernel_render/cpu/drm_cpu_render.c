/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/version.h>
#include <linux/vmalloc.h>
#include <drm/drm_atomic.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_plane.h>

#include "../../export_fun.h"
#include "../../klog/lsdriver_log.h"
#include "cpu_raster.h"

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
    struct mutex draw_lock;
    const struct drm_mode_config_funcs *original_funcs;
    struct drm_mode_config_funcs hooked_funcs;
    bool attached;
    bool hook_installed;
};

static struct drm_cpu_render_context drm_cpu_render;
static struct drm_cpu_render_api drm_cpu_render_api;

static int resolve_symbols(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    drm_cpu_render_api.minors_xa = (struct xarray *)generic_kallsyms_lookup_name("drm_minors_xa");
    if (!drm_cpu_render_api.minors_xa) return -ENOENT;
#endif
    drm_cpu_render_api.minor_acquire = (typeof(drm_cpu_render_api.minor_acquire))generic_kallsyms_lookup_name("drm_minor_acquire");
    drm_cpu_render_api.minor_release = (typeof(drm_cpu_render_api.minor_release))generic_kallsyms_lookup_name("drm_minor_release");
    drm_cpu_render_api.msm_gem_get_vaddr = (typeof(drm_cpu_render_api.msm_gem_get_vaddr))generic_kallsyms_lookup_name("msm_gem_get_vaddr");
    drm_cpu_render_api.msm_gem_put_vaddr = (typeof(drm_cpu_render_api.msm_gem_put_vaddr))generic_kallsyms_lookup_name("msm_gem_put_vaddr");
    if (!drm_cpu_render_api.minor_acquire || !drm_cpu_render_api.minor_release ||
        !drm_cpu_render_api.msm_gem_get_vaddr || !drm_cpu_render_api.msm_gem_put_vaddr)
        return -ENOENT;
    return 0;
}

static void draw_into_primary_framebuffer(struct drm_atomic_state *state)
{
    struct drm_plane *plane;
    struct drm_plane_state *plane_state;

    drm_for_each_plane(plane, drm_cpu_render.dev)
    {
        plane_state = drm_atomic_get_new_plane_state(state, plane);
        if (!plane_state) plane_state = plane->state;
        if (!plane_state) continue;
        struct drm_framebuffer *fb = plane_state->fb;
        struct drm_gem_object *object;
        void *pixels;

        if (plane->type != DRM_PLANE_TYPE_PRIMARY || !plane_state->crtc || !fb || !fb->obj[0]) continue;
        if (!fb->format || (fb->format->format != DRM_FORMAT_ARGB8888 && fb->format->format != DRM_FORMAT_XRGB8888)) continue;
        if (fb->modifier != DRM_FORMAT_MOD_LINEAR || fb->pitches[0] < fb->width * 4) continue;

        object = fb->obj[0];
        pixels = drm_cpu_render_api.msm_gem_get_vaddr(object);
        if (IS_ERR_OR_NULL(pixels)) continue;
        if (READ_ONCE(drm_cpu_render.attached))
        {
            cpu_raster_blend((u8 *)pixels + fb->offsets[0], fb->width, fb->height,
                             fb->pitches[0], drm_cpu_render.overlay_pixels,
                             DRM_CPU_OVERLAY_WIDTH, DRM_CPU_OVERLAY_HEIGHT,
                             DRM_CPU_OVERLAY_PITCH,
                             ((int)fb->width - DRM_CPU_OVERLAY_WIDTH) / 2,
                             ((int)fb->height - DRM_CPU_OVERLAY_HEIGHT) / 2);
            wmb();
        }
        drm_cpu_render_api.msm_gem_put_vaddr(object);
        return;
    }
}

static int drm_cpu_render_atomic_commit(struct drm_device *dev,
                                        struct drm_atomic_state *state,
                                        bool nonblock)
{
    bool locked = dev == drm_cpu_render.dev && READ_ONCE(drm_cpu_render.attached);
    int ret;

    if (locked)
    {
        mutex_lock(&drm_cpu_render.draw_lock);
        draw_into_primary_framebuffer(state);
    }
    ret = drm_cpu_render.original_funcs->atomic_commit(dev, state, nonblock);
    if (locked) mutex_unlock(&drm_cpu_render.draw_lock);
    return ret;
}

static int install_atomic_hook(void)
{
    if (!try_module_get(THIS_MODULE)) return -ENODEV;
    drm_cpu_render.hooked_funcs = *drm_cpu_render.original_funcs;
    drm_cpu_render.hooked_funcs.atomic_commit = drm_cpu_render_atomic_commit;
    if (cmpxchg(&drm_cpu_render.dev->mode_config.funcs, drm_cpu_render.original_funcs,
                &drm_cpu_render.hooked_funcs) != drm_cpu_render.original_funcs)
    {
        module_put(THIS_MODULE);
        return -EBUSY;
    }
    drm_cpu_render.hook_installed = true;
    return 0;
}

static int restore_atomic_hook(void)
{
    if (!drm_cpu_render.hook_installed) return 0;
    if (cmpxchg(&drm_cpu_render.dev->mode_config.funcs, &drm_cpu_render.hooked_funcs,
                drm_cpu_render.original_funcs) != &drm_cpu_render.hooked_funcs)
        return -EBUSY;
    drm_cpu_render.hook_installed = false;
    module_put(THIS_MODULE);
    return 0;
}

static int release_output(void)
{
    int ret = restore_atomic_hook();

    if (ret) return ret;
    drm_cpu_render.attached = false;
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
    ret = install_atomic_hook();
    if (ret) goto fail;
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
    int ret;

    ret = resolve_symbols();
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

static int drm_cpu_begin_draw(void)
{
    if (!READ_ONCE(drm_cpu_render.attached) || !drm_cpu_render.overlay_pixels) return -ENODEV;
    mutex_lock(&drm_cpu_render.draw_lock);
    if (!READ_ONCE(drm_cpu_render.attached) || !drm_cpu_render.overlay_pixels)
    {
        mutex_unlock(&drm_cpu_render.draw_lock);
        return -ENODEV;
    }
    return 0;
}

static int drm_cpu_end_draw(void)
{
    wmb();
    mutex_unlock(&drm_cpu_render.draw_lock);
    return 0;
}

int drm_cpu_render_clear(u32 color)
{
    int ret = drm_cpu_begin_draw();

    if (ret) return ret;
    cpu_raster_clear(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                     DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH, color);
    return drm_cpu_end_draw();
}

int drm_cpu_render_line(int x0, int y0, int x1, int y1, u32 color, u32 thickness)
{
    int ret = drm_cpu_begin_draw();

    if (ret) return ret;
    cpu_raster_line(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                    DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH,
                    x0, y0, x1, y1, color, thickness);
    return drm_cpu_end_draw();
}

int drm_cpu_render_rect(int left, int top, int right, int bottom, u32 color, u32 thickness, bool filled)
{
    int ret = drm_cpu_begin_draw();

    if (ret) return ret;
    cpu_raster_rect(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                    DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH,
                    left, top, right, bottom, color, thickness, filled);
    return drm_cpu_end_draw();
}

int drm_cpu_render_circle(int center_x, int center_y, int radius, u32 color, u32 thickness, bool filled)
{
    int ret = drm_cpu_begin_draw();

    if (ret) return ret;
    cpu_raster_circle(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                      DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH,
                      center_x, center_y, radius, color, thickness, filled);
    return drm_cpu_end_draw();
}

int drm_cpu_render_text(int left, int top, const char *text, u32 scale, u32 color)
{
    int ret = drm_cpu_begin_draw();

    if (ret) return ret;
    if (!text)
    {
        mutex_unlock(&drm_cpu_render.draw_lock);
        return -EINVAL;
    }
    cpu_raster_text(drm_cpu_render.overlay_pixels, DRM_CPU_OVERLAY_WIDTH,
                    DRM_CPU_OVERLAY_HEIGHT, DRM_CPU_OVERLAY_PITCH,
                    left, top, text, scale, color);
    return drm_cpu_end_draw();
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
