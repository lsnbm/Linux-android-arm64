/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/version.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_blend.h>
#include <drm/drm_client.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_plane.h>
#include <drm/drm_property.h>

#include "../../export_fun.h"
#include "../../klog/lsdriver_log.h"
#include "cpu_raster.h"
#include "drm_cpu_render.h"

#define DRM_CPU_WIDTH 640U
#define DRM_CPU_HEIGHT 640U
#define DRM_CPU_FORMAT DRM_FORMAT_ARGB8888

static struct
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    struct drm_minor *(*minor_acquire)(struct xarray *, unsigned int);
    struct xarray *minors_xa;
#else
    struct drm_minor *(*minor_acquire)(unsigned int);
#endif
    void (*minor_release)(struct drm_minor *);
    typeof(drm_client_init) *client_init;
    typeof(drm_client_release) *client_release;
    typeof(drm_client_framebuffer_create) *framebuffer_create;
    typeof(drm_client_framebuffer_delete) *framebuffer_delete;
    typeof(drm_client_framebuffer_flush) *framebuffer_flush;
    typeof(drm_client_buffer_vmap) *buffer_vmap;
    typeof(drm_modeset_acquire_init) *acquire_init;
    typeof(drm_modeset_acquire_fini) *acquire_fini;
    typeof(drm_modeset_lock_all_ctx) *lock_all;
    typeof(drm_modeset_drop_locks) *drop_locks;
    typeof(drm_modeset_backoff) *backoff;
    typeof(drm_atomic_state_alloc) *state_alloc;
    typeof(drm_atomic_state_clear) *state_clear;
    typeof(__drm_atomic_state_free) *state_free;
    typeof(drm_atomic_get_crtc_state) *get_crtc_state;
    typeof(drm_atomic_get_plane_state) *get_plane_state;
    typeof(drm_atomic_set_crtc_for_plane) *set_crtc;
    typeof(drm_atomic_set_fb_for_plane) *set_fb;
    typeof(drm_atomic_check_only) *check_only;
    typeof(drm_atomic_commit) *commit;
    int (*get_property)(struct drm_mode_object *, struct drm_property *, uint64_t *);
    int (*set_property)(struct drm_atomic_state *, struct drm_file *,
                        struct drm_mode_object *, struct drm_property *, uint64_t);
} drm_cpu_api;

static struct
{
    struct drm_minor *minor;
    struct drm_client_dev client;
    struct drm_client_buffer *buffers[2];
    void *pixels[2];
    struct drm_plane *plane;
    struct task_struct *frame_owner;
    unsigned int back;
    bool initialized;
} drm_cpu_render;

static DEFINE_MUTEX(drm_cpu_lock);

static bool owns_framebuffer(struct drm_framebuffer *fb)
{
    return fb && ((drm_cpu_render.buffers[0] && fb == drm_cpu_render.buffers[0]->fb) ||
                  (drm_cpu_render.buffers[1] && fb == drm_cpu_render.buffers[1]->fb));
}

static struct drm_property *plane_property(struct drm_plane *plane, const char *name)
{
    for (int index = 0; index < plane->base.properties->count; index++)
    {
        struct drm_property *property = plane->base.properties->properties[index];

        if (!__builtin_strcmp(property->name, name)) return property;
    }
    return NULL;
}

static int commit_plane(bool disable)
{
    struct drm_modeset_acquire_ctx acquire;
    struct drm_atomic_state *state = drm_cpu_api.state_alloc(drm_cpu_render.client.dev);
    if (!state) return -ENOMEM;

    drm_cpu_api.acquire_init(&acquire, 0);
    state->acquire_ctx = &acquire;
    state->allow_modeset = false;
    int ret;
retry:
    ret = drm_cpu_api.lock_all(drm_cpu_render.client.dev, &acquire);
    if (ret) goto out;

    if (drm_cpu_render.plane && drm_cpu_render.plane->state->fb &&
        !owns_framebuffer(drm_cpu_render.plane->state->fb))
    {
        ret = disable ? 0 : -EBUSY;
        goto out;
    }
    if (disable)
    {
        if (!drm_cpu_render.plane || !owns_framebuffer(drm_cpu_render.plane->state->fb))
        {
            ret = 0;
            goto out;
        }
        if (drm_cpu_render.plane->state->crtc)
        {
            struct drm_crtc_state *crtc_state = drm_cpu_api.get_crtc_state(state, drm_cpu_render.plane->state->crtc);
            if (IS_ERR(crtc_state))
            {
                ret = PTR_ERR(crtc_state);
                goto out;
            }
        }
        struct drm_plane_state *plane_state = drm_cpu_api.get_plane_state(state, drm_cpu_render.plane);
        if (IS_ERR(plane_state))
        {
            ret = PTR_ERR(plane_state);
            goto out;
        }
        drm_cpu_api.set_fb(plane_state, NULL);
        ret = drm_cpu_api.set_crtc(plane_state, NULL);
        if (!ret) ret = drm_cpu_api.commit(state);
        goto out;
    }

    struct drm_crtc *crtc = NULL;
    struct drm_connector *connector;
    list_for_each_entry(connector, &drm_cpu_render.client.dev->mode_config.connector_list, head)
    {
        if (connector->connector_type != DRM_MODE_CONNECTOR_DSI ||
            !connector->state || !connector->state->crtc)
            continue;
        if (connector->state->crtc->state->active)
        {
            crtc = connector->state->crtc;
            break;
        }
    }
    if (!crtc)
    {
        ret = -ENODEV;
        goto out;
    }
    if (crtc->state->mode.hdisplay < DRM_CPU_WIDTH || crtc->state->mode.vdisplay < DRM_CPU_HEIGHT)
    {
        ret = -ERANGE;
        goto out;
    }
    struct drm_plane *plane;
    ret = -ENOSPC;
    drm_for_each_plane(plane, drm_cpu_render.client.dev)
    {
        if (drm_cpu_render.plane && drm_cpu_render.plane != plane) continue;
        if (plane->type == DRM_PLANE_TYPE_CURSOR || plane_property(plane, "multirect_mode") ||
            !(plane->possible_crtcs & drm_crtc_mask(crtc)) ||
            (plane->state->crtc && !owns_framebuffer(plane->state->fb)) ||
            (plane->state->fb && !owns_framebuffer(plane->state->fb)))
            continue;

        bool supported = false;
        for (unsigned int index = 0; index < plane->format_count; index++)
            if (plane->format_types[index] == DRM_CPU_FORMAT)
            {
                supported = true;
                break;
            }
        if (!supported) continue;
        if (plane->funcs->format_mod_supported &&
            !plane->funcs->format_mod_supported(plane, DRM_CPU_FORMAT, DRM_FORMAT_MOD_LINEAR))
            continue;

        bool validated = false;
    prepare_candidate:;
        struct drm_crtc_state *crtc_state = drm_cpu_api.get_crtc_state(state, crtc);
        if (IS_ERR(crtc_state))
        {
            ret = PTR_ERR(crtc_state);
            goto out;
        }
        struct drm_plane_state *plane_state = drm_cpu_api.get_plane_state(state, plane);
        if (IS_ERR(plane_state))
        {
            ret = PTR_ERR(plane_state);
            goto out;
        }
        ret = drm_cpu_api.set_crtc(plane_state, crtc);
        if (ret) goto out;
        drm_cpu_api.set_fb(plane_state, drm_cpu_render.buffers[drm_cpu_render.back]->fb);
        plane_state->src_x = 0;
        plane_state->src_y = 0;
        plane_state->src_w = DRM_CPU_WIDTH << 16;
        plane_state->src_h = DRM_CPU_HEIGHT << 16;
        plane_state->crtc_x = (crtc->state->mode.hdisplay - (int)DRM_CPU_WIDTH) / 2;
        plane_state->crtc_y = (crtc->state->mode.vdisplay - (int)DRM_CPU_HEIGHT) / 2;
        plane_state->crtc_w = DRM_CPU_WIDTH;
        plane_state->crtc_h = DRM_CPU_HEIGHT;
        plane_state->rotation = DRM_MODE_ROTATE_0;
        plane_state->alpha = DRM_BLEND_ALPHA_OPAQUE;
        plane_state->pixel_blend_mode = DRM_MODE_BLEND_PREMULTI;

        uint64_t zpos = 0;
        struct drm_plane *other;
        drm_for_each_plane(other, drm_cpu_render.client.dev)
        {
            if (other == plane || other->state->crtc != crtc || !other->state->fb) continue;
            struct drm_property *property = plane_property(other, "zpos");
            uint64_t value;
            if (!property)
            {
                ret = -EOPNOTSUPP;
                goto out;
            }
            ret = drm_cpu_api.get_property(&other->base, property, &value);
            if (ret) goto out;
            if (value >= UINT_MAX)
            {
                ret = -ERANGE;
                goto out;
            }
            zpos = max_t(uint64_t, zpos, value + 1);
        }
        struct drm_property *property = plane_property(plane, "zpos");
        ret = -EOPNOTSUPP;
        if (property && !(property->flags & DRM_MODE_PROP_IMMUTABLE))
            ret = drm_cpu_api.set_property(state, drm_cpu_render.client.file,
                                            &plane->base, property, zpos);
        if (!ret)
        {
            property = plane_property(plane, "alpha");
            if (property)
            {
                if (!(property->flags & DRM_MODE_PROP_RANGE) || property->num_values != 2 ||
                    (property->flags & DRM_MODE_PROP_IMMUTABLE))
                    ret = -EOPNOTSUPP;
                else
                    ret = drm_cpu_api.set_property(state, drm_cpu_render.client.file,
                                                    &plane->base, property, property->values[1]);
            }
        }
        if (!ret)
        {
            property = plane_property(plane, "blend_op");
            if (property)
            {
                struct drm_property_enum *entry;
                ret = -EOPNOTSUPP;
                list_for_each_entry(entry, &property->enum_list, head)
                {
                    if (__builtin_strcmp(entry->name, "premultiplied")) continue;
                    ret = drm_cpu_api.set_property(state, drm_cpu_render.client.file,
                                                    &plane->base, property, entry->value);
                    break;
                }
            }
        }
        if (!ret && !drm_cpu_render.plane && !validated) ret = drm_cpu_api.check_only(state);
        if (!ret && !drm_cpu_render.plane && !validated)
        {
            drm_cpu_api.state_clear(state);
            validated = true;
            goto prepare_candidate;
        }
        if (ret)
        {
            if (ret == -EDEADLK || drm_cpu_render.plane) goto out;
            drm_cpu_api.state_clear(state);
            continue;
        }
        ret = drm_cpu_api.commit(state);
        if (!ret)
        {
            drm_cpu_render.plane = plane;
            drm_cpu_render.back ^= 1;
        }
        goto out;
    }
out:
    if (ret == -EDEADLK)
    {
        drm_cpu_api.state_clear(state);
        ret = drm_cpu_api.backoff(&acquire);
        if (!ret) goto retry;
    }
    kref_put(&state->ref, drm_cpu_api.state_free);
    drm_cpu_api.drop_locks(&acquire);
    drm_cpu_api.acquire_fini(&acquire);
    return ret;
}

static void release_buffers(void)
{
    for (unsigned int index = 0; index < ARRAY_SIZE(drm_cpu_render.buffers); index++)
    {
        if (drm_cpu_render.buffers[index]) drm_cpu_api.framebuffer_delete(drm_cpu_render.buffers[index]);
        drm_cpu_render.buffers[index] = NULL;
        drm_cpu_render.pixels[index] = NULL;
    }
    if (drm_cpu_render.client.dev)
    {
        drm_cpu_api.client_release(&drm_cpu_render.client);
        __builtin_memset(&drm_cpu_render.client, 0, sizeof(drm_cpu_render.client));
    }
    if (drm_cpu_render.minor) drm_cpu_api.minor_release(drm_cpu_render.minor);
    drm_cpu_render.minor = NULL;
    drm_cpu_render.plane = NULL;
    drm_cpu_render.back = 0;
}

int drm_cpu_render_init(void)
{
    mutex_lock(&drm_cpu_lock);
    if (drm_cpu_render.initialized)
    {
        mutex_unlock(&drm_cpu_lock);
        return -EBUSY;
    }
    int ret = -ENOENT;
#define RESOLVE_CPU_API(member, symbol) \
    do { \
        drm_cpu_api.member = (typeof(drm_cpu_api.member))generic_kallsyms_lookup_name(#symbol); \
        if (!drm_cpu_api.member) { \
            ls_log_always_tag("cpu_render", "missing DRM symbol: %s\n", #symbol); \
            goto fail; \
        } \
    } while (0)
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    RESOLVE_CPU_API(minors_xa, drm_minors_xa);
#endif
    RESOLVE_CPU_API(minor_acquire, drm_minor_acquire);
    RESOLVE_CPU_API(minor_release, drm_minor_release);
    RESOLVE_CPU_API(client_init, drm_client_init);
    RESOLVE_CPU_API(client_release, drm_client_release);
    RESOLVE_CPU_API(framebuffer_create, drm_client_framebuffer_create);
    RESOLVE_CPU_API(framebuffer_delete, drm_client_framebuffer_delete);
    RESOLVE_CPU_API(framebuffer_flush, drm_client_framebuffer_flush);
    RESOLVE_CPU_API(buffer_vmap, drm_client_buffer_vmap);
    RESOLVE_CPU_API(acquire_init, drm_modeset_acquire_init);
    RESOLVE_CPU_API(acquire_fini, drm_modeset_acquire_fini);
    RESOLVE_CPU_API(lock_all, drm_modeset_lock_all_ctx);
    RESOLVE_CPU_API(drop_locks, drm_modeset_drop_locks);
    RESOLVE_CPU_API(backoff, drm_modeset_backoff);
    RESOLVE_CPU_API(state_alloc, drm_atomic_state_alloc);
    RESOLVE_CPU_API(state_clear, drm_atomic_state_clear);
    RESOLVE_CPU_API(state_free, __drm_atomic_state_free);
    RESOLVE_CPU_API(get_crtc_state, drm_atomic_get_crtc_state);
    RESOLVE_CPU_API(get_plane_state, drm_atomic_get_plane_state);
    RESOLVE_CPU_API(set_crtc, drm_atomic_set_crtc_for_plane);
    RESOLVE_CPU_API(set_fb, drm_atomic_set_fb_for_plane);
    RESOLVE_CPU_API(check_only, drm_atomic_check_only);
    RESOLVE_CPU_API(commit, drm_atomic_commit);
    RESOLVE_CPU_API(get_property, drm_atomic_get_property);
    RESOLVE_CPU_API(set_property, drm_atomic_set_property);
#undef RESOLVE_CPU_API

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    drm_cpu_render.minor = drm_cpu_api.minor_acquire(drm_cpu_api.minors_xa, 0);
#else
    drm_cpu_render.minor = drm_cpu_api.minor_acquire(0);
#endif
    if (IS_ERR(drm_cpu_render.minor))
    {
        ret = PTR_ERR(drm_cpu_render.minor);
        drm_cpu_render.minor = NULL;
        goto fail;
    }
    if (__builtin_strcmp(drm_cpu_render.minor->dev->driver->name, "msm") &&
        __builtin_strcmp(drm_cpu_render.minor->dev->driver->name, "msm_drm"))
    {
        ret = -ENODEV;
        goto fail;
    }
    if (!drm_drv_uses_atomic_modeset(drm_cpu_render.minor->dev))
    {
        ret = -EOPNOTSUPP;
        goto fail;
    }
    ret = drm_cpu_api.client_init(drm_cpu_render.minor->dev, &drm_cpu_render.client, "ls-cpu-overlay", NULL);
    if (ret)
    {
        __builtin_memset(&drm_cpu_render.client, 0, sizeof(drm_cpu_render.client));
        goto fail;
    }
    for (unsigned int index = 0; index < ARRAY_SIZE(drm_cpu_render.buffers); index++)
    {
        drm_cpu_render.buffers[index] = drm_cpu_api.framebuffer_create(&drm_cpu_render.client,
                DRM_CPU_WIDTH, DRM_CPU_HEIGHT, DRM_CPU_FORMAT);
        if (IS_ERR(drm_cpu_render.buffers[index]))
        {
            ret = PTR_ERR(drm_cpu_render.buffers[index]);
            drm_cpu_render.buffers[index] = NULL;
            goto fail;
        }
        if (drm_cpu_render.buffers[index]->fb->modifier != DRM_FORMAT_MOD_LINEAR ||
            drm_cpu_render.buffers[index]->fb->format->format != DRM_CPU_FORMAT)
        {
            ret = -EOPNOTSUPP;
            goto fail;
        }
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
        drm_cpu_render.pixels[index] = drm_cpu_api.buffer_vmap(drm_cpu_render.buffers[index]);
        if (IS_ERR_OR_NULL(drm_cpu_render.pixels[index]))
        {
            ret = drm_cpu_render.pixels[index] ? PTR_ERR(drm_cpu_render.pixels[index]) : -ENOMEM;
            drm_cpu_render.pixels[index] = NULL;
            goto fail;
        }
#else
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
        struct dma_buf_map map;
#else
        struct iosys_map map;
#endif
        ret = drm_cpu_api.buffer_vmap(drm_cpu_render.buffers[index], &map);
        if (ret) goto fail;
        if (map.is_iomem || !map.vaddr)
        {
            ret = -EOPNOTSUPP;
            goto fail;
        }
        drm_cpu_render.pixels[index] = map.vaddr;
#endif
        cpu_raster_clear(drm_cpu_render.pixels[index], DRM_CPU_WIDTH, DRM_CPU_HEIGHT,
                          drm_cpu_render.buffers[index]->pitch, 0);
    }
    drm_cpu_render.initialized = true;
    mutex_unlock(&drm_cpu_lock);
    return 0;
fail:
    release_buffers();
    mutex_unlock(&drm_cpu_lock);
    ls_log_always_tag("cpu_render", "overlay initialization failed, ret=%d\n", ret);
    return ret;
}

int drm_cpu_render_begin_frame(void)
{
    if (READ_ONCE(drm_cpu_render.frame_owner) == current) return -EBUSY;
    mutex_lock(&drm_cpu_lock);
    if (!drm_cpu_render.initialized)
    {
        mutex_unlock(&drm_cpu_lock);
        return -ENODEV;
    }
    WRITE_ONCE(drm_cpu_render.frame_owner, current);
    return 0;
}

int drm_cpu_render_end_frame(void)
{
    if (READ_ONCE(drm_cpu_render.frame_owner) != current) return -EINVAL;
    wmb();
    int ret = drm_cpu_api.framebuffer_flush(drm_cpu_render.buffers[drm_cpu_render.back], NULL);
    if (!ret) ret = commit_plane(false);
    WRITE_ONCE(drm_cpu_render.frame_owner, NULL);
    mutex_unlock(&drm_cpu_lock);
    if (ret) ls_log_always_tag("cpu_render", "overlay plane commit failed, ret=%d\n", ret);
    return ret;
}

int drm_cpu_render_clear(u32 color)
{
    if (READ_ONCE(drm_cpu_render.frame_owner) != current) return -EINVAL;
    cpu_raster_clear(drm_cpu_render.pixels[drm_cpu_render.back], DRM_CPU_WIDTH,
                      DRM_CPU_HEIGHT, drm_cpu_render.buffers[drm_cpu_render.back]->pitch, color);
    return 0;
}

int drm_cpu_render_line(int x0, int y0, int x1, int y1, u32 color, u32 thickness)
{
    if (READ_ONCE(drm_cpu_render.frame_owner) != current) return -EINVAL;
    cpu_raster_line(drm_cpu_render.pixels[drm_cpu_render.back], DRM_CPU_WIDTH,
                     DRM_CPU_HEIGHT, drm_cpu_render.buffers[drm_cpu_render.back]->pitch,
                     x0, y0, x1, y1, color, thickness);
    return 0;
}

int drm_cpu_render_rect(int left, int top, int right, int bottom, u32 color, u32 thickness, bool filled)
{
    if (READ_ONCE(drm_cpu_render.frame_owner) != current) return -EINVAL;
    cpu_raster_rect(drm_cpu_render.pixels[drm_cpu_render.back], DRM_CPU_WIDTH,
                     DRM_CPU_HEIGHT, drm_cpu_render.buffers[drm_cpu_render.back]->pitch,
                     left, top, right, bottom, color, thickness, filled);
    return 0;
}

int drm_cpu_render_circle(int center_x, int center_y, int radius, u32 color, u32 thickness, bool filled)
{
    if (READ_ONCE(drm_cpu_render.frame_owner) != current) return -EINVAL;
    cpu_raster_circle(drm_cpu_render.pixels[drm_cpu_render.back], DRM_CPU_WIDTH,
                       DRM_CPU_HEIGHT, drm_cpu_render.buffers[drm_cpu_render.back]->pitch,
                       center_x, center_y, radius, color, thickness, filled);
    return 0;
}

int drm_cpu_render_text(int left, int top, const char *text, u32 scale, u32 color)
{
    if (READ_ONCE(drm_cpu_render.frame_owner) != current || !text) return -EINVAL;
    cpu_raster_text(drm_cpu_render.pixels[drm_cpu_render.back], DRM_CPU_WIDTH,
                     DRM_CPU_HEIGHT, drm_cpu_render.buffers[drm_cpu_render.back]->pitch,
                     left, top, text, scale, color);
    return 0;
}

int drm_cpu_render_exit(void)
{
    if (READ_ONCE(drm_cpu_render.frame_owner) == current) return -EBUSY;
    mutex_lock(&drm_cpu_lock);
    int ret = drm_cpu_render.initialized ? commit_plane(true) : 0;
    if (!ret)
    {
        drm_cpu_render.initialized = false;
        release_buffers();
    }
    mutex_unlock(&drm_cpu_lock);
    if (ret) ls_log_always_tag("cpu_render", "overlay plane detach failed, ret=%d\n", ret);
    return ret;
}