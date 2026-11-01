/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <drm/drm_client.h>
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_modeset_lock.h>

#include "../../export_fun.h"
#include "../../klog/lsdriver_log.h"
#include "cpu_raster.h"
#include "cpu_render.h"
#include "drm_cpu_render_api.h"

#define DRM_CPU_MINOR  0U
#define DRM_CPU_FORMAT DRM_FORMAT_ARGB8888

struct drm_cpu_render_context
{
    struct drm_minor *minor;
    struct drm_device *dev;
    struct drm_client_dev client;
    struct drm_client_buffer *buffer;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    struct iosys_map map;
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    struct dma_buf_map map;
#endif
    void *pixels;
    struct mutex draw_lock;
    u32 width;
    u32 height;
    u32 pitch;
    bool initialized;
    bool attached;
    bool client_initialized;
    bool mapped;
};

static struct drm_cpu_render_context drm_cpu_render;
struct drm_cpu_render_api drm_cpu_render_api;

static int resolve_symbols(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    drm_cpu_render_api.minors_xa = (struct xarray *)generic_kallsyms_lookup_name("drm_minors_xa");
    if (!drm_cpu_render_api.minors_xa) return -ENOENT;
#endif
    drm_cpu_render_api.minor_acquire = (typeof(drm_cpu_render_api.minor_acquire))generic_kallsyms_lookup_name("drm_minor_acquire");
    drm_cpu_render_api.minor_release = (typeof(drm_cpu_render_api.minor_release))generic_kallsyms_lookup_name("drm_minor_release");
    if (!drm_cpu_render_api.minor_acquire || !drm_cpu_render_api.minor_release) return -ENOENT;

    drm_cpu_render_api.client_init = (typeof(drm_cpu_render_api.client_init))generic_kallsyms_lookup_name("drm_client_init");
    drm_cpu_render_api.client_release = (typeof(drm_cpu_render_api.client_release))generic_kallsyms_lookup_name("drm_client_release");
    drm_cpu_render_api.client_framebuffer_create = (typeof(drm_cpu_render_api.client_framebuffer_create))generic_kallsyms_lookup_name("drm_client_framebuffer_create");
    drm_cpu_render_api.client_buffer_delete = (typeof(drm_cpu_render_api.client_buffer_delete))generic_kallsyms_lookup_name("drm_client_framebuffer_delete");
    drm_cpu_render_api.client_framebuffer_flush = (typeof(drm_cpu_render_api.client_framebuffer_flush))generic_kallsyms_lookup_name("drm_client_framebuffer_flush");
    drm_cpu_render_api.client_buffer_vunmap = (typeof(drm_cpu_render_api.client_buffer_vunmap))generic_kallsyms_lookup_name("drm_client_buffer_vunmap");
    drm_cpu_render_api.client_modeset_probe = (typeof(drm_cpu_render_api.client_modeset_probe))generic_kallsyms_lookup_name("drm_client_modeset_probe");
    drm_cpu_render_api.client_modeset_commit = (typeof(drm_cpu_render_api.client_modeset_commit))generic_kallsyms_lookup_name("drm_client_modeset_commit");
    drm_cpu_render_api.client_modeset_dpms = (typeof(drm_cpu_render_api.client_modeset_dpms))generic_kallsyms_lookup_name("drm_client_modeset_dpms");
    drm_cpu_render_api.client_buffer_vmap = (typeof(drm_cpu_render_api.client_buffer_vmap))generic_kallsyms_lookup_name("drm_client_buffer_vmap");
    if (!drm_cpu_render_api.client_init || !drm_cpu_render_api.client_release || !drm_cpu_render_api.client_framebuffer_create || !drm_cpu_render_api.client_buffer_vmap || !drm_cpu_render_api.client_framebuffer_flush || !drm_cpu_render_api.client_buffer_delete || !drm_cpu_render_api.client_buffer_vunmap || !drm_cpu_render_api.client_modeset_probe || !drm_cpu_render_api.client_modeset_commit || !drm_cpu_render_api.client_modeset_dpms) return -ENOENT;
    return 0;
}

static struct drm_minor *minor_acquire(unsigned int minor)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    return drm_cpu_render_api.minor_acquire(drm_cpu_render_api.minors_xa, minor);
#else
    return drm_cpu_render_api.minor_acquire(minor);
#endif
}

static int acquire_device(void)
{
    drm_cpu_render.minor = minor_acquire(DRM_CPU_MINOR);
    if (IS_ERR(drm_cpu_render.minor))
    {
        int ret = PTR_ERR(drm_cpu_render.minor);

        drm_cpu_render.minor = NULL;
        return ret;
    }
    drm_cpu_render.dev = drm_cpu_render.minor->dev;
    return 0;
}

static void release_output(void)
{
    drm_cpu_render.attached = false;
    if (drm_cpu_render.mapped)
    {
        drm_cpu_render_api.client_buffer_vunmap(drm_cpu_render.buffer);
        drm_cpu_render.mapped = false;
    }
    drm_cpu_render.pixels = NULL;
    if (drm_cpu_render.buffer)
    {
        drm_cpu_render_api.client_buffer_delete(drm_cpu_render.buffer);
        drm_cpu_render.buffer = NULL;
    }
    if (drm_cpu_render.client_initialized) drm_cpu_render_api.client_release(&drm_cpu_render.client);
    memset(&drm_cpu_render.client, 0, sizeof(drm_cpu_render.client));
    drm_cpu_render.client_initialized = false;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0)
    memset(&drm_cpu_render.map, 0, sizeof(drm_cpu_render.map));
#endif
    drm_cpu_render.width = 0;
    drm_cpu_render.height = 0;
    drm_cpu_render.pitch = 0;
    if (drm_cpu_render.minor) drm_cpu_render_api.minor_release(drm_cpu_render.minor);
    drm_cpu_render.minor = NULL;
    drm_cpu_render.dev = NULL;
}

static int choose_mode(void)
{
    struct drm_mode_set *mode_set;
    u32 width = 0;
    u32 height = 0;
    int ret;

    ret = drm_cpu_render_api.client_modeset_probe(&drm_cpu_render.client, 0, 0);
    if (ret) return ret;

    mutex_lock(&drm_cpu_render.client.modeset_mutex);
    drm_client_for_each_modeset(mode_set, &drm_cpu_render.client)
    {
        if (!mode_set->mode) continue;
        width = mode_set->mode->hdisplay;
        height = mode_set->mode->vdisplay;
        break;
    }
    mutex_unlock(&drm_cpu_render.client.modeset_mutex);
    if (!width || !height) return -EAGAIN;

    drm_cpu_render.width = width;
    drm_cpu_render.height = height;
    return 0;
}

static int create_client_buffer(void)
{
    struct drm_mode_set *mode_set;
    int ret;

    drm_cpu_render.buffer = drm_cpu_render_api.client_framebuffer_create(&drm_cpu_render.client, drm_cpu_render.width, drm_cpu_render.height, DRM_CPU_FORMAT);
    if (IS_ERR(drm_cpu_render.buffer))
    {
        ret = PTR_ERR(drm_cpu_render.buffer);
        drm_cpu_render.buffer = NULL;
        return ret;
    }

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
    drm_cpu_render.pixels = drm_cpu_render_api.client_buffer_vmap(drm_cpu_render.buffer);
    if (IS_ERR_OR_NULL(drm_cpu_render.pixels))
    {
        ret = drm_cpu_render.pixels ? PTR_ERR(drm_cpu_render.pixels) : -ENOMEM;
        drm_cpu_render.pixels = NULL;
        return ret;
    }
    drm_cpu_render.mapped = true;
#else
    ret = drm_cpu_render_api.client_buffer_vmap(drm_cpu_render.buffer, &drm_cpu_render.map);
    if (ret) return ret;
    drm_cpu_render.mapped = true;
    if (drm_cpu_render.map.is_iomem || !drm_cpu_render.map.vaddr) return -EOPNOTSUPP;
    drm_cpu_render.pixels = drm_cpu_render.map.vaddr;
#endif
    drm_cpu_render.pitch = drm_cpu_render.buffer->pitch;

    mutex_lock(&drm_cpu_render.client.modeset_mutex);
    drm_client_for_each_modeset(mode_set, &drm_cpu_render.client) mode_set->fb = drm_cpu_render.buffer->fb;
    mutex_unlock(&drm_cpu_render.client.modeset_mutex);
    return 0;
}

static int acquire_output(void)
{
    int ret;

    ret = acquire_device();
    if (ret)
    {
        ls_log_always_tag("cpu_render", "acquire DRM minor failed, ret=%d\n", ret);
        return ret;
    }
    ret = drm_cpu_render_api.client_init(drm_cpu_render.dev, &drm_cpu_render.client, "lsdriver-cpu-render", NULL);
    if (ret)
    {
        ls_log_always_tag("cpu_render", "drm_client_init failed, ret=%d\n", ret);
        goto fail;
    }
    drm_cpu_render.client_initialized = true;
    ret = choose_mode();
    if (ret)
    {
        ls_log_always_tag("cpu_render", "drm_client_modeset_probe failed, ret=%d\n", ret);
        goto fail;
    }
    ret = create_client_buffer();
    if (ret)
    {
        ls_log_always_tag("cpu_render", "client framebuffer setup failed, ret=%d\n", ret);
        goto fail;
    }

    cpu_raster_demo(drm_cpu_render.pixels, drm_cpu_render.width, drm_cpu_render.height, drm_cpu_render.pitch);
    wmb();
    ret = drm_cpu_render_api.client_framebuffer_flush(drm_cpu_render.buffer, NULL);
    if (ret)
    {
        ls_log_always_tag("cpu_render", "initial framebuffer flush failed, ret=%d\n", ret);
        goto fail;
    }
    ret = drm_cpu_render_api.client_modeset_commit(&drm_cpu_render.client);
    if (ret)
    {
        ls_log_always_tag("cpu_render", "drm_client_modeset_commit failed, ret=%d\n", ret);
        goto fail;
    }
    drm_cpu_render.attached = true;
    return 0;
fail:
    release_output();
    return ret;
}

int drm_cpu_render_init(void)
{
    int ret;

    ret = resolve_symbols();
    if (ret)
    {
        ls_log_always_tag("cpu_render", "DRM symbol resolution failed, ret=%d\n", ret);
        return ret;
    }
    mutex_init(&drm_cpu_render.draw_lock);
    ret = acquire_output();
    if (ret) return ret;
    drm_cpu_render.initialized = true;
    return 0;
}

static int drm_cpu_begin_draw(void)
{
    if (!READ_ONCE(drm_cpu_render.attached) || !drm_cpu_render.pixels) return -ENODEV;
    mutex_lock(&drm_cpu_render.draw_lock);
    if (!READ_ONCE(drm_cpu_render.attached) || !drm_cpu_render.pixels)
    {
        mutex_unlock(&drm_cpu_render.draw_lock);
        return -ENODEV;
    }
    return 0;
}

static int drm_cpu_end_draw(void)
{
    int ret;

    wmb();
    ret = drm_cpu_render_api.client_framebuffer_flush(drm_cpu_render.buffer, NULL);
    mutex_unlock(&drm_cpu_render.draw_lock);
    return ret;
}

int drm_cpu_render_clear(u32 color)
{
    int ret = drm_cpu_begin_draw();

    if (ret) return ret;
    cpu_raster_clear(drm_cpu_render.pixels, drm_cpu_render.width, drm_cpu_render.height, drm_cpu_render.pitch, color);
    return drm_cpu_end_draw();
}

int drm_cpu_render_line(int x0, int y0, int x1, int y1, u32 color, u32 thickness)
{
    int ret = drm_cpu_begin_draw();

    if (ret) return ret;
    cpu_raster_line(drm_cpu_render.pixels, drm_cpu_render.width, drm_cpu_render.height, drm_cpu_render.pitch, x0, y0, x1, y1, color, thickness);
    return drm_cpu_end_draw();
}

int drm_cpu_render_rect(int left, int top, int right, int bottom, u32 color, u32 thickness, bool filled)
{
    int ret = drm_cpu_begin_draw();

    if (ret) return ret;
    cpu_raster_rect(drm_cpu_render.pixels, drm_cpu_render.width, drm_cpu_render.height, drm_cpu_render.pitch, left, top, right, bottom, color, thickness, filled);
    return drm_cpu_end_draw();
}

int drm_cpu_render_circle(int center_x, int center_y, int radius, u32 color, u32 thickness, bool filled)
{
    int ret = drm_cpu_begin_draw();

    if (ret) return ret;
    cpu_raster_circle(drm_cpu_render.pixels, drm_cpu_render.width, drm_cpu_render.height, drm_cpu_render.pitch, center_x, center_y, radius, color, thickness, filled);
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
    cpu_raster_text(drm_cpu_render.pixels, drm_cpu_render.width, drm_cpu_render.height, drm_cpu_render.pitch, left, top, text, scale, color);
    return drm_cpu_end_draw();
}

int drm_cpu_render_exit(void)
{
    int ret = 0;

    if (!drm_cpu_render.initialized) return 0;
    mutex_lock(&drm_cpu_render.draw_lock);
    drm_cpu_render.attached = false;
    mutex_unlock(&drm_cpu_render.draw_lock);
    if (drm_cpu_render.client_initialized) ret = drm_cpu_render_api.client_modeset_dpms(&drm_cpu_render.client, DRM_MODE_DPMS_OFF);
    if (ret)
    {
        drm_cpu_render.attached = true;
        return ret;
    }
    release_output();
    drm_cpu_render.initialized = false;
    return 0;
}
