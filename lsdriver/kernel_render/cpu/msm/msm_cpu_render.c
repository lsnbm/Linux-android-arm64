/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/atomic.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <drm/drm_atomic.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_blend.h>
#include <drm/drm_connector.h>
#include <drm/drm_crtc.h>
#include <drm/drm_device.h>
#include <drm/drm_drv.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#include <drm/drm_modeset_lock.h>
#include <drm/drm_plane.h>
#include <drm/drm_property.h>

#include "../../../klog/lsdriver_log.h"
#include "msm_cpu_raster.h"
#include "msm_cpu_render.h"
#include "msm_cpu_symbols.h"

#define MSM_BO_SCANOUT 0x00000001U
#define MSM_BO_WC 0x00020000U

struct msm_cpu_render_context {
    struct delayed_work work;
    struct drm_minor *minor;
    struct drm_device *dev;
    struct drm_crtc *crtc;
    struct drm_plane *plane;
    struct drm_gem_object *object;
    struct drm_framebuffer *fb;
    void *pixels;
    const struct drm_mode_config_funcs *original_funcs;
    struct drm_mode_config_funcs hooked_funcs;
    u32 width;
    u32 height;
    u32 pitch;
    u32 x;
    u32 y;
    struct mutex draw_lock;
    int atomic_check_refs;
    bool initialized;
    bool attached;
    bool valid;
    bool hook_ref;
};

static struct msm_cpu_render_context msm_cpu_render;

static inline bool msm_cpu_atomic_check_try_get(void)
{
    __atomic_add_fetch(&msm_cpu_render.atomic_check_refs, 1,
                       __ATOMIC_ACQ_REL);
    if (likely(READ_ONCE(msm_cpu_render.valid)))
        return true;
    __atomic_sub_fetch(&msm_cpu_render.atomic_check_refs, 1,
                       __ATOMIC_RELEASE);
    return false;
}

static inline void msm_cpu_atomic_check_put(void)
{
    __atomic_sub_fetch(&msm_cpu_render.atomic_check_refs, 1,
                       __ATOMIC_RELEASE);
}

static void msm_cpu_atomic_check_wait_refs(void)
{
    while (__atomic_load_n(&msm_cpu_render.atomic_check_refs,
                           __ATOMIC_ACQUIRE))
        cond_resched();
}

static struct drm_property *find_plane_property(struct drm_plane *plane,
                                                const char *name)
{
    unsigned int index;

    for (index = 0; index < plane->properties.count; index++) {
        struct drm_property *property = plane->properties.properties[index];

        if (!strcmp(property->name, name))
            return property;
    }
    return NULL;
}

static bool plane_supports_argb(struct drm_plane *plane)
{
    unsigned int index;

    for (index = 0; index < plane->format_count; index++)
        if (plane->format_types[index] == DRM_FORMAT_ARGB8888)
            return true;
    return false;
}

static int select_output(void)
{
    struct drm_crtc *crtc;
    struct drm_connector *connector;
    struct drm_connector_list_iter iter;
    struct drm_plane *plane;

    msm_cpu_render.crtc = NULL;
    drm_connector_list_iter_begin(msm_cpu_render.dev, &iter);
    drm_for_each_connector_iter(connector, &iter) {
        if (connector->connector_type == DRM_MODE_CONNECTOR_DSI &&
            connector->state && connector->state->crtc &&
            connector->state->crtc->state &&
            connector->state->crtc->state->active &&
            connector->state->crtc->state->enable) {
            msm_cpu_render.crtc = connector->state->crtc;
            break;
        }
    }
    drm_connector_list_iter_end(&iter);
    if (msm_cpu_render.crtc)
        goto have_crtc;

    drm_for_each_crtc(crtc, msm_cpu_render.dev) {
        if (crtc->state && crtc->state->active && crtc->state->enable) {
            msm_cpu_render.crtc = crtc;
            break;
        }
    }
    if (!msm_cpu_render.crtc)
        return -EAGAIN;

have_crtc:
    msm_cpu_render.width = min_t(u32, 640,
                                 msm_cpu_render.crtc->state->mode.hdisplay);
    msm_cpu_render.height = min_t(u32, 640,
                                  msm_cpu_render.crtc->state->mode.vdisplay);
    msm_cpu_render.x = (msm_cpu_render.crtc->state->mode.hdisplay -
                        msm_cpu_render.width) / 2;
    msm_cpu_render.y = (msm_cpu_render.crtc->state->mode.vdisplay -
                        msm_cpu_render.height) / 2;

    msm_cpu_render.plane = NULL;
    drm_for_each_plane(plane, msm_cpu_render.dev) {
        if (plane->type == DRM_PLANE_TYPE_CURSOR ||
            !(plane->possible_crtcs & drm_crtc_mask(msm_cpu_render.crtc)) ||
            !plane->state || plane->state->fb || plane->state->crtc ||
            !plane_supports_argb(plane))
            continue;
        msm_cpu_render.plane = plane;
        break;
    }
    return msm_cpu_render.plane ? 0 : -EBUSY;
}

static void release_image(void)
{
    if (msm_cpu_render.pixels)
        msm_cpu_gem_put_vaddr(msm_cpu_render.object);
    msm_cpu_render.pixels = NULL;
    if (msm_cpu_render.fb)
        drm_framebuffer_put(msm_cpu_render.fb);
    else if (msm_cpu_render.object)
        drm_gem_object_put(msm_cpu_render.object);
    msm_cpu_render.fb = NULL;
    msm_cpu_render.object = NULL;
}

static int create_image(void)
{
    struct drm_mode_fb_cmd2 command = {};
    struct drm_gem_object *objects[4] = {};
    u32 size;
    int ret;

    msm_cpu_render.pitch = ALIGN(msm_cpu_render.width * 4, 256);
    size = PAGE_ALIGN(msm_cpu_render.pitch * msm_cpu_render.height);
    msm_cpu_render.object = msm_cpu_gem_new(msm_cpu_render.dev, size,
                                            MSM_BO_SCANOUT | MSM_BO_WC);
    if (IS_ERR(msm_cpu_render.object)) {
        ret = PTR_ERR(msm_cpu_render.object);
        msm_cpu_render.object = NULL;
        return ret;
    }

    msm_cpu_render.pixels = msm_cpu_gem_get_vaddr(msm_cpu_render.object);
    if (IS_ERR_OR_NULL(msm_cpu_render.pixels)) {
        ret = msm_cpu_render.pixels ? PTR_ERR(msm_cpu_render.pixels) : -ENOMEM;
        msm_cpu_render.pixels = NULL;
        release_image();
        return ret;
    }

    msm_cpu_raster_demo(msm_cpu_render.pixels, msm_cpu_render.width,
                        msm_cpu_render.height, msm_cpu_render.pitch);
    wmb();

    command.width = msm_cpu_render.width;
    command.height = msm_cpu_render.height;
    command.pixel_format = DRM_FORMAT_ARGB8888;
    command.pitches[0] = msm_cpu_render.pitch;
    objects[0] = msm_cpu_render.object;
    msm_cpu_render.fb = msm_cpu_framebuffer_init(msm_cpu_render.dev, &command,
                                                  objects);
    if (IS_ERR(msm_cpu_render.fb)) {
        ret = PTR_ERR(msm_cpu_render.fb);
        msm_cpu_render.fb = NULL;
        release_image();
        return ret;
    }
    return 0;
}

static int apply_overlay(struct drm_atomic_state *state, bool *changed)
{
    struct drm_crtc_state *crtc_state;
    struct drm_plane_state *plane_state;
    struct drm_property *property;
    int ret;

    *changed = false;
    crtc_state = drm_atomic_get_crtc_state(state, msm_cpu_render.crtc);
    if (IS_ERR(crtc_state))
        return PTR_ERR(crtc_state);
    plane_state = drm_atomic_get_plane_state(state, msm_cpu_render.plane);
    if (IS_ERR(plane_state))
        return PTR_ERR(plane_state);
    if (plane_state->fb && plane_state->fb != msm_cpu_render.fb)
        return 0;
    if (!crtc_state->active || !crtc_state->enable)
        return 0;

    ret = drm_atomic_set_crtc_for_plane(plane_state, msm_cpu_render.crtc);
    if (ret)
        return ret;
    drm_atomic_set_fb_for_plane(plane_state, msm_cpu_render.fb);
    plane_state->crtc_x = msm_cpu_render.x;
    plane_state->crtc_y = msm_cpu_render.y;
    plane_state->crtc_w = msm_cpu_render.width;
    plane_state->crtc_h = msm_cpu_render.height;
    plane_state->src_x = 0;
    plane_state->src_y = 0;
    plane_state->src_w = msm_cpu_render.width << 16;
    plane_state->src_h = msm_cpu_render.height << 16;
    plane_state->rotation = DRM_MODE_ROTATE_0;
    plane_state->alpha = DRM_BLEND_ALPHA_OPAQUE;
    plane_state->pixel_blend_mode = DRM_MODE_BLEND_PREMULTI;

    property = find_plane_property(msm_cpu_render.plane, "zpos");
    if (property && property->num_values >= 2) {
        plane_state->zpos = property->values[1];
    }
    property = find_plane_property(msm_cpu_render.plane, "alpha");
    if (property && property->num_values >= 2) {
        plane_state->alpha = property->values[1];
    }
    crtc_state->planes_changed = true;
    *changed = true;
    return 0;
}

static int msm_cpu_atomic_check(struct drm_device *dev,
                                struct drm_atomic_state *state)
{
    bool changed;
    int ret;

    if (!msm_cpu_atomic_check_try_get())
        return -ESHUTDOWN;
    if (!state->async_update && !state->legacy_cursor_update &&
        READ_ONCE(msm_cpu_render.attached) &&
        (drm_atomic_get_new_crtc_state(state, msm_cpu_render.crtc) ||
         drm_atomic_get_new_plane_state(state, msm_cpu_render.plane))) {
        ret = apply_overlay(state, &changed);
        if (ret)
            goto out;
    }
    ret = msm_cpu_render.original_funcs->atomic_check(dev, state);
out:
    msm_cpu_atomic_check_put();
    return ret;
}

static int commit_overlay(void)
{
    struct drm_modeset_acquire_ctx acquire;
    struct drm_atomic_state *state = NULL;
    bool changed;
    int ret;

    drm_modeset_acquire_init(&acquire, 0);
retry:
    ret = drm_modeset_lock_all_ctx(msm_cpu_render.dev, &acquire);
    if (ret)
        goto out;
    state = drm_atomic_state_alloc(msm_cpu_render.dev);
    if (!state) {
        ret = -ENOMEM;
        goto unlock;
    }
    state->acquire_ctx = &acquire;
    ret = apply_overlay(state, &changed);
    if (!ret && !changed)
        ret = -ECANCELED;
    if (!ret)
        ret = drm_atomic_check_only(state);
    if (ret == -EDEADLK) {
        drm_atomic_state_put(state);
        state = NULL;
        drm_modeset_drop_locks(&acquire);
        ret = drm_modeset_backoff(&acquire);
        if (!ret)
            goto retry;
        goto out;
    }
    if (ret)
        goto unlock;
    ret = drm_atomic_commit(state);
unlock:
    if (state)
        drm_atomic_state_put(state);
    drm_modeset_drop_locks(&acquire);
out:
    drm_modeset_acquire_fini(&acquire);
    return ret;
}

static int detach_output(void);

static int attach_output(void)
{
    bool overlay_committed = false;
    int ret;

    msm_cpu_render.minor = msm_cpu_minor_acquire(0);
    if (IS_ERR(msm_cpu_render.minor)) {
        ret = PTR_ERR(msm_cpu_render.minor);
        msm_cpu_render.minor = NULL;
        return ret;
    }
    msm_cpu_render.dev = msm_cpu_render.minor->dev;
    ret = select_output();
    if (ret)
        goto fail;
    ret = create_image();
    if (ret)
        goto fail;
    msm_cpu_render.original_funcs = READ_ONCE(msm_cpu_render.dev->mode_config.funcs);
    if (!msm_cpu_render.original_funcs ||
        !msm_cpu_render.original_funcs->atomic_check) {
        ret = -EOPNOTSUPP;
        goto fail;
    }
    ret = commit_overlay();
    if (ret)
        goto fail;
    overlay_committed = true;
    msm_cpu_render.hooked_funcs = *msm_cpu_render.original_funcs;
    msm_cpu_render.hooked_funcs.atomic_check = msm_cpu_atomic_check;
    if (!try_module_get(THIS_MODULE)) {
        ret = -ENODEV;
        goto fail;
    }
    if (cmpxchg(&msm_cpu_render.dev->mode_config.funcs,
                msm_cpu_render.original_funcs,
                &msm_cpu_render.hooked_funcs) != msm_cpu_render.original_funcs) {
        module_put(THIS_MODULE);
        ret = -EBUSY;
        goto fail;
    }
    msm_cpu_render.hook_ref = true;
    WRITE_ONCE(msm_cpu_render.attached, true);
    WRITE_ONCE(msm_cpu_render.valid, true);
    return 0;

fail:
    if (overlay_committed) {
        int detach_ret = detach_output();

        if (detach_ret)
            ls_log_always_tag("msm_cpu_render",
                              "回滚 MSM CPU framebuffer 失败: %d\n",
                              detach_ret);
    }
    release_image();
    if (msm_cpu_render.minor)
        msm_cpu_minor_release(msm_cpu_render.minor);
    msm_cpu_render.minor = NULL;
    msm_cpu_render.dev = NULL;
    return ret;
}

static void msm_cpu_render_worker(struct work_struct *work)
{
    int ret;

    if (!READ_ONCE(msm_cpu_render.attached)) {
        ret = attach_output();
        if (ret && ret != -EAGAIN && ret != -EBUSY && ret != -ENODEV)
            ls_log_always_tag("msm_cpu_render",
                              "绑定 MSM CPU framebuffer 失败: %d\n", ret);
    }
    if (msm_cpu_render.initialized && !READ_ONCE(msm_cpu_render.attached))
        schedule_delayed_work(&msm_cpu_render.work, HZ);
}

static int detach_output(void)
{
    struct drm_modeset_acquire_ctx acquire;
    struct drm_atomic_state *state = NULL;
    struct drm_crtc_state *crtc_state;
    struct drm_plane_state *plane_state;
    int ret;

    drm_modeset_acquire_init(&acquire, 0);
retry:
    ret = drm_modeset_lock_all_ctx(msm_cpu_render.dev, &acquire);
    if (ret)
        goto out;
    state = drm_atomic_state_alloc(msm_cpu_render.dev);
    if (!state) {
        ret = -ENOMEM;
        goto unlock;
    }
    state->acquire_ctx = &acquire;
    plane_state = drm_atomic_get_plane_state(state, msm_cpu_render.plane);
    if (IS_ERR(plane_state)) {
        ret = PTR_ERR(plane_state);
        goto unlock;
    }
    if (plane_state->fb != msm_cpu_render.fb) {
        ret = 0;
        goto unlock;
    }
    ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
    if (ret)
        goto unlock;
    drm_atomic_set_fb_for_plane(plane_state, NULL);
    crtc_state = drm_atomic_get_crtc_state(state, msm_cpu_render.crtc);
    if (IS_ERR(crtc_state)) {
        ret = PTR_ERR(crtc_state);
        goto unlock;
    }
    crtc_state->planes_changed = true;
    ret = drm_atomic_check_only(state);
    if (ret == -EDEADLK) {
        drm_atomic_state_put(state);
        state = NULL;
        drm_modeset_drop_locks(&acquire);
        ret = drm_modeset_backoff(&acquire);
        if (!ret)
            goto retry;
        goto out;
    }
    if (ret)
        goto unlock;
    ret = drm_atomic_commit(state);
unlock:
    if (state)
        drm_atomic_state_put(state);
    drm_modeset_drop_locks(&acquire);
out:
    drm_modeset_acquire_fini(&acquire);
    return ret;
}

int msm_cpu_render_init(void)
{
    int ret;

    ret = msm_cpu_symbols_init();
    if (ret) {
        ls_log_always_tag("msm_cpu_render",
                          "MSM 显示符号不完整，无法初始化 CPU 渲染: %d\n", ret);
        return ret;
    }
    mutex_init(&msm_cpu_render.draw_lock);
    INIT_DELAYED_WORK(&msm_cpu_render.work, msm_cpu_render_worker);
    msm_cpu_render.initialized = true;
    schedule_delayed_work(&msm_cpu_render.work, HZ);
    return 0;
}

void msm_cpu_render_exit(void)
{
    int ret;

    if (!msm_cpu_render.initialized)
        return;
    msm_cpu_render.initialized = false;
    cancel_delayed_work_sync(&msm_cpu_render.work);
    mutex_lock(&msm_cpu_render.draw_lock);
    WRITE_ONCE(msm_cpu_render.attached, false);
    mutex_unlock(&msm_cpu_render.draw_lock);
    if (msm_cpu_render.fb) {
        ret = detach_output();
        if (ret) {
            WRITE_ONCE(msm_cpu_render.attached, true);
            WRITE_ONCE(msm_cpu_render.valid, true);
            ls_log_always_tag("msm_cpu_render",
                              "禁用 MSM CPU framebuffer 失败: %d\n", ret);
            return;
        }
    }
    WRITE_ONCE(msm_cpu_render.valid, false);
    msm_cpu_atomic_check_wait_refs();
    if (msm_cpu_render.hook_ref) {
        cmpxchg(&msm_cpu_render.dev->mode_config.funcs,
                &msm_cpu_render.hooked_funcs,
                msm_cpu_render.original_funcs);
        msm_cpu_render.hook_ref = false;
        module_put(THIS_MODULE);
    }
    release_image();
    if (msm_cpu_render.minor)
        msm_cpu_minor_release(msm_cpu_render.minor);
    msm_cpu_render.minor = NULL;
    msm_cpu_render.dev = NULL;
}

static int begin_draw(void)
{
    if (!READ_ONCE(msm_cpu_render.attached) || !msm_cpu_render.pixels)
        return -ENODEV;
    mutex_lock(&msm_cpu_render.draw_lock);
    if (!READ_ONCE(msm_cpu_render.attached) || !msm_cpu_render.pixels) {
        mutex_unlock(&msm_cpu_render.draw_lock);
        return -ENODEV;
    }
    return 0;
}

static void end_draw(void)
{
    wmb();
    mutex_unlock(&msm_cpu_render.draw_lock);
}

int msm_cpu_render_clear(u32 color)
{
    int ret = begin_draw();

    if (ret)
        return ret;
    msm_cpu_raster_clear(msm_cpu_render.pixels, msm_cpu_render.width,
                         msm_cpu_render.height, msm_cpu_render.pitch, color);
    end_draw();
    return 0;
}

int msm_cpu_render_line(int x0, int y0, int x1, int y1, u32 color,
                        u32 thickness)
{
    int ret = begin_draw();

    if (ret)
        return ret;
    msm_cpu_raster_line(msm_cpu_render.pixels, msm_cpu_render.width,
                        msm_cpu_render.height, msm_cpu_render.pitch,
                        x0, y0, x1, y1, color, thickness);
    end_draw();
    return 0;
}

int msm_cpu_render_rect(int left, int top, int right, int bottom, u32 color,
                        u32 thickness, bool filled)
{
    int ret = begin_draw();

    if (ret)
        return ret;
    msm_cpu_raster_rect(msm_cpu_render.pixels, msm_cpu_render.width,
                        msm_cpu_render.height, msm_cpu_render.pitch,
                        left, top, right, bottom, color, thickness, filled);
    end_draw();
    return 0;
}

int msm_cpu_render_circle(int center_x, int center_y, int radius, u32 color,
                          u32 thickness, bool filled)
{
    int ret = begin_draw();

    if (ret)
        return ret;
    msm_cpu_raster_circle(msm_cpu_render.pixels, msm_cpu_render.width,
                          msm_cpu_render.height, msm_cpu_render.pitch,
                          center_x, center_y, radius, color, thickness, filled);
    end_draw();
    return 0;
}

int msm_cpu_render_text(int left, int top, const char *text, u32 scale,
                        u32 color)
{
    int ret = begin_draw();

    if (ret)
        return ret;
    if (!text) {
        mutex_unlock(&msm_cpu_render.draw_lock);
        return -EINVAL;
    }
    msm_cpu_raster_text(msm_cpu_render.pixels, msm_cpu_render.width,
                        msm_cpu_render.height, msm_cpu_render.pitch,
                        left, top, text, scale, color);
    end_draw();
    return 0;
}

static const struct cpu_render_ops msm_cpu_render_ops = {
    .platform = CPU_RENDER_PLATFORM_MSM,
    .name = "qualcomm-msm",
    .init = msm_cpu_render_init,
    .exit = msm_cpu_render_exit,
    .clear = msm_cpu_render_clear,
    .line = msm_cpu_render_line,
    .rect = msm_cpu_render_rect,
    .circle = msm_cpu_render_circle,
    .text = msm_cpu_render_text,
};

const struct cpu_render_ops *msm_cpu_render_get_ops(void)
{
    return &msm_cpu_render_ops;
}