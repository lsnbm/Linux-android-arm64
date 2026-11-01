/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/version.h>
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
#include <drm/drm_modeset_helper.h>
#include <drm/drm_plane.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#include <linux/xarray.h>
#endif

#include "../../../export_fun.h"
#include "../cpu_render.h"
#include "../msm/msm_cpu_raster.h"

#define MTK_CPU_NO_CFI __attribute__((no_sanitize("cfi", "kcfi")))
#define MTK_CPU_SCANOUT_FORMAT DRM_FORMAT_ARGB8888

struct mtk_cpu_gem_obj {
    struct drm_gem_object base;
    void *cookie;
    void *kvaddr;
    dma_addr_t dma_addr;
    unsigned long dma_attrs;
    struct sg_table *sg;
    struct page **pages;
};

struct mtk_gem_obj;
struct mtk_drm_gem_obj;

struct mtk_cpu_framebuffer {
    struct drm_framebuffer base;
};

struct mtk_cpu_context {
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
    struct mutex draw_lock;
    u32 width;
    u32 height;
    u32 pitch;
    u32 x;
    u32 y;
    bool initialized;
    bool attached;
    bool valid;
    bool hook_ref;
    int atomic_check_refs;
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static struct drm_minor *(*fn_drm_minor_acquire)(struct xarray *, unsigned int);
static struct xarray *fn_drm_minors_xa;
#else
static struct drm_minor *(*fn_drm_minor_acquire)(unsigned int);
#endif
static void (*fn_drm_minor_release)(struct drm_minor *);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
static struct mtk_gem_obj *(*fn_mtk_gem_create)(struct drm_device *, size_t,
                                                 bool);
#else
static struct mtk_drm_gem_obj *(*fn_mtk_drm_gem_create)(struct drm_device *,
                                                         size_t, bool);
#endif
static struct mtk_cpu_context mtk_cpu;

static inline bool mtk_cpu_atomic_check_try_get(void)
{
    __atomic_add_fetch(&mtk_cpu.atomic_check_refs, 1, __ATOMIC_ACQ_REL);
    if (likely(READ_ONCE(mtk_cpu.valid)))
        return true;
    __atomic_sub_fetch(&mtk_cpu.atomic_check_refs, 1, __ATOMIC_RELEASE);
    return false;
}

static inline void mtk_cpu_atomic_check_put(void)
{
    __atomic_sub_fetch(&mtk_cpu.atomic_check_refs, 1, __ATOMIC_RELEASE);
}

static void mtk_cpu_atomic_check_wait_refs(void)
{
    while (__atomic_load_n(&mtk_cpu.atomic_check_refs, __ATOMIC_ACQUIRE))
        cond_resched();
}

static bool plane_supports_argb(struct drm_plane *plane)
{
    unsigned int index;

    for (index = 0; index < plane->format_count; index++)
        if (plane->format_types[index] == MTK_CPU_SCANOUT_FORMAT)
            return true;
    return false;
}

static void mtk_cpu_fb_destroy(struct drm_framebuffer *fb)
{
    struct mtk_cpu_framebuffer *mtk_fb =
            container_of(fb, struct mtk_cpu_framebuffer, base);
    struct drm_gem_object *object = fb->obj[0];

    drm_framebuffer_cleanup(fb);
    if (object)
        drm_gem_object_put(object);
    kfree(mtk_fb);
}

static const struct drm_framebuffer_funcs mtk_cpu_fb_funcs = {
    .destroy = mtk_cpu_fb_destroy,
};

static struct drm_framebuffer *mtk_cpu_fb_init(struct drm_device *dev,
                                               const struct drm_mode_fb_cmd2 *cmd,
                                               struct drm_gem_object *object)
{
    struct mtk_cpu_framebuffer *mtk_fb;
    int ret;

    mtk_fb = kzalloc(sizeof(*mtk_fb), GFP_KERNEL);
    if (!mtk_fb)
        return ERR_PTR(-ENOMEM);
    mtk_fb->base.obj[0] = object;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    {
        const struct drm_format_info *info = drm_get_format_info(
            dev, cmd->pixel_format, cmd->modifier[0]);

        if (!info) {
            kfree(mtk_fb);
            return ERR_PTR(-EINVAL);
        }
        drm_helper_mode_fill_fb_struct(dev, &mtk_fb->base, info, cmd);
    }
#else
    drm_helper_mode_fill_fb_struct(dev, &mtk_fb->base, cmd);
#endif
    ret = drm_framebuffer_init(dev, &mtk_fb->base, &mtk_cpu_fb_funcs);
    if (ret) {
        kfree(mtk_fb);
        return ERR_PTR(ret);
    }
    return &mtk_fb->base;
}

static int resolve_symbols(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    fn_drm_minors_xa = (struct xarray *)generic_kallsyms_lookup_name(
        "drm_minors_xa");
#endif
    fn_drm_minor_acquire = (typeof(fn_drm_minor_acquire))
            generic_kallsyms_lookup_name("drm_minor_acquire");
    fn_drm_minor_release = (typeof(fn_drm_minor_release))
            generic_kallsyms_lookup_name("drm_minor_release");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    fn_mtk_gem_create = (typeof(fn_mtk_gem_create))
        generic_kallsyms_lookup_name("mtk_gem_create");
#else
    fn_mtk_drm_gem_create = (typeof(fn_mtk_drm_gem_create))
            generic_kallsyms_lookup_name("mtk_drm_gem_create");
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    if (!fn_drm_minors_xa)
    return -ENOENT;
#endif
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    if (!fn_drm_minor_acquire || !fn_drm_minor_release ||
    !fn_mtk_gem_create)
        return -ENOENT;
#else
    if (!fn_drm_minor_acquire || !fn_drm_minor_release ||
    !fn_mtk_drm_gem_create)
    return -ENOENT;
#endif
    return 0;
}

static int select_output(void)
{
    struct drm_connector *connector;
    struct drm_connector_list_iter connector_iter;
    struct drm_crtc *crtc;
    struct drm_plane *plane;

    mtk_cpu.crtc = NULL;
    drm_connector_list_iter_begin(mtk_cpu.dev, &connector_iter);
    drm_for_each_connector_iter(connector, &connector_iter) {
        if (connector->connector_type == DRM_MODE_CONNECTOR_DSI &&
            connector->state && connector->state->crtc &&
            connector->state->crtc->state &&
            connector->state->crtc->state->active &&
            connector->state->crtc->state->enable) {
            mtk_cpu.crtc = connector->state->crtc;
            break;
        }
    }
    drm_connector_list_iter_end(&connector_iter);
    if (!mtk_cpu.crtc) {
        drm_for_each_crtc(crtc, mtk_cpu.dev) {
            if (crtc->state && crtc->state->active && crtc->state->enable) {
                mtk_cpu.crtc = crtc;
                break;
            }
        }
    }
    if (!mtk_cpu.crtc)
        return -EAGAIN;

    mtk_cpu.width = min_t(u32, 640, mtk_cpu.crtc->state->mode.hdisplay);
    mtk_cpu.height = min_t(u32, 640, mtk_cpu.crtc->state->mode.vdisplay);
    mtk_cpu.x = (mtk_cpu.crtc->state->mode.hdisplay - mtk_cpu.width) / 2;
    mtk_cpu.y = (mtk_cpu.crtc->state->mode.vdisplay - mtk_cpu.height) / 2;

    mtk_cpu.plane = NULL;
    drm_for_each_plane(plane, mtk_cpu.dev) {
        if (plane->type == DRM_PLANE_TYPE_CURSOR ||
            !(plane->possible_crtcs & drm_crtc_mask(mtk_cpu.crtc)) ||
            !plane->state || plane->state->fb || plane->state->crtc ||
            !plane_supports_argb(plane))
            continue;
        mtk_cpu.plane = plane;
        break;
    }
    return mtk_cpu.plane ? 0 : -EBUSY;
}

static void release_image(void)
{
    if (mtk_cpu.fb)
        drm_framebuffer_put(mtk_cpu.fb);
    else if (mtk_cpu.object)
        drm_gem_object_put(mtk_cpu.object);
    mtk_cpu.fb = NULL;
    mtk_cpu.object = NULL;
    mtk_cpu.pixels = NULL;
}

static int create_image(void)
{
    struct drm_mode_fb_cmd2 command = {};
    struct mtk_cpu_gem_obj *gem;
    u32 size;
    int ret;

    mtk_cpu.pitch = ALIGN(mtk_cpu.width * 4, 64);
    size = PAGE_ALIGN(mtk_cpu.pitch * mtk_cpu.height);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    gem = (struct mtk_cpu_gem_obj *)fn_mtk_gem_create(mtk_cpu.dev, size, true);
#else
    gem = (struct mtk_cpu_gem_obj *)fn_mtk_drm_gem_create(mtk_cpu.dev, size,
                                                          true);
#endif
    if (IS_ERR(gem))
        return PTR_ERR(gem);
    mtk_cpu.object = &gem->base;
    mtk_cpu.pixels = gem->kvaddr;
    if (!mtk_cpu.pixels) {
        ret = -ENOMEM;
        release_image();
        return ret;
    }

    msm_cpu_raster_demo(mtk_cpu.pixels, mtk_cpu.width, mtk_cpu.height,
                        mtk_cpu.pitch);
    wmb();
    command.width = mtk_cpu.width;
    command.height = mtk_cpu.height;
    command.pixel_format = MTK_CPU_SCANOUT_FORMAT;
    command.pitches[0] = mtk_cpu.pitch;
    mtk_cpu.fb = mtk_cpu_fb_init(mtk_cpu.dev, &command, mtk_cpu.object);
    if (IS_ERR(mtk_cpu.fb)) {
        ret = PTR_ERR(mtk_cpu.fb);
        mtk_cpu.fb = NULL;
        release_image();
        return ret;
    }
    return 0;
}

static int apply_overlay(struct drm_atomic_state *state, bool *changed)
{
    struct drm_crtc_state *crtc_state;
    struct drm_plane_state *plane_state;
    int ret;

    *changed = false;
    crtc_state = drm_atomic_get_crtc_state(state, mtk_cpu.crtc);
    if (IS_ERR(crtc_state))
        return PTR_ERR(crtc_state);
    plane_state = drm_atomic_get_plane_state(state, mtk_cpu.plane);
    if (IS_ERR(plane_state))
        return PTR_ERR(plane_state);
    if (plane_state->fb && plane_state->fb != mtk_cpu.fb)
        return 0;
    if (!crtc_state->active || !crtc_state->enable)
        return 0;

    ret = drm_atomic_set_crtc_for_plane(plane_state, mtk_cpu.crtc);
    if (ret)
        return ret;
    drm_atomic_set_fb_for_plane(plane_state, mtk_cpu.fb);
    plane_state->crtc_x = mtk_cpu.x;
    plane_state->crtc_y = mtk_cpu.y;
    plane_state->crtc_w = mtk_cpu.width;
    plane_state->crtc_h = mtk_cpu.height;
    plane_state->src_x = 0;
    plane_state->src_y = 0;
    plane_state->src_w = mtk_cpu.width << 16;
    plane_state->src_h = mtk_cpu.height << 16;
    plane_state->rotation = DRM_MODE_ROTATE_0;
    plane_state->alpha = DRM_BLEND_ALPHA_OPAQUE;
    plane_state->pixel_blend_mode = DRM_MODE_BLEND_PREMULTI;
    crtc_state->planes_changed = true;
    *changed = true;
    return 0;
}

static int mtk_cpu_atomic_check(struct drm_device *dev,
                                struct drm_atomic_state *state)
{
    bool changed;
    int ret;

    if (!mtk_cpu_atomic_check_try_get())
        return -ESHUTDOWN;
    if (!state->async_update && !state->legacy_cursor_update &&
        READ_ONCE(mtk_cpu.attached) &&
        (drm_atomic_get_new_crtc_state(state, mtk_cpu.crtc) ||
         drm_atomic_get_new_plane_state(state, mtk_cpu.plane))) {
        ret = apply_overlay(state, &changed);
        if (ret)
            goto out;
    }
    ret = mtk_cpu.original_funcs->atomic_check(dev, state);
out:
    mtk_cpu_atomic_check_put();
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
    ret = drm_modeset_lock_all_ctx(mtk_cpu.dev, &acquire);
    if (ret)
        goto out;
    state = drm_atomic_state_alloc(mtk_cpu.dev);
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

static int mtk_cpu_detach(void);

static int mtk_cpu_attach(void)
{
    bool overlay_committed = false;
    int ret;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    mtk_cpu.minor = fn_drm_minor_acquire(fn_drm_minors_xa, 0);
#else
    mtk_cpu.minor = fn_drm_minor_acquire(0);
#endif
    if (IS_ERR(mtk_cpu.minor)) {
        ret = PTR_ERR(mtk_cpu.minor);
        mtk_cpu.minor = NULL;
        return ret;
    }
    mtk_cpu.dev = mtk_cpu.minor->dev;
    ret = select_output();
    if (ret)
        goto fail;
    ret = create_image();
    if (ret)
        goto fail;
    mtk_cpu.original_funcs = READ_ONCE(mtk_cpu.dev->mode_config.funcs);
    if (!mtk_cpu.original_funcs || !mtk_cpu.original_funcs->atomic_check) {
        ret = -EOPNOTSUPP;
        goto fail;
    }
    ret = commit_overlay();
    if (ret)
        goto fail;
    overlay_committed = true;
    mtk_cpu.hooked_funcs = *mtk_cpu.original_funcs;
    mtk_cpu.hooked_funcs.atomic_check = mtk_cpu_atomic_check;
    if (!try_module_get(THIS_MODULE)) {
        ret = -ENODEV;
        goto fail;
    }
    if (cmpxchg(&mtk_cpu.dev->mode_config.funcs, mtk_cpu.original_funcs,
                &mtk_cpu.hooked_funcs) != mtk_cpu.original_funcs) {
        module_put(THIS_MODULE);
        ret = -EBUSY;
        goto fail;
    }
    mtk_cpu.hook_ref = true;
    WRITE_ONCE(mtk_cpu.attached, true);
    WRITE_ONCE(mtk_cpu.valid, true);
    return 0;
fail:
    if (overlay_committed) {
        int detach_ret = mtk_cpu_detach();

        if (detach_ret)
            pr_warn_ratelimited("[lsdriver] MTK CPU framebuffer rollback failed: %d\n",
                                detach_ret);
    }
    release_image();
    if (mtk_cpu.minor)
        fn_drm_minor_release(mtk_cpu.minor);
    mtk_cpu.minor = NULL;
    mtk_cpu.dev = NULL;
    return ret;
}

static void mtk_cpu_worker(struct work_struct *work)
{
    int ret;

    if (!READ_ONCE(mtk_cpu.attached)) {
        ret = mtk_cpu_attach();
        if (ret && ret != -EAGAIN && ret != -EBUSY)
            pr_warn_ratelimited("[lsdriver] MTK CPU renderer attach failed: %d\n",
                                ret);
    }
    if (mtk_cpu.initialized && !READ_ONCE(mtk_cpu.attached))
        schedule_delayed_work(&mtk_cpu.work, HZ);
}

static int mtk_cpu_detach(void)
{
    struct drm_modeset_acquire_ctx acquire;
    struct drm_atomic_state *state = NULL;
    struct drm_crtc_state *crtc_state;
    struct drm_plane_state *plane_state;
    int ret;

    drm_modeset_acquire_init(&acquire, 0);
retry:
    ret = drm_modeset_lock_all_ctx(mtk_cpu.dev, &acquire);
    if (ret)
        goto out;
    state = drm_atomic_state_alloc(mtk_cpu.dev);
    if (!state) {
        ret = -ENOMEM;
        goto unlock;
    }
    state->acquire_ctx = &acquire;
    plane_state = drm_atomic_get_plane_state(state, mtk_cpu.plane);
    if (IS_ERR(plane_state)) {
        ret = PTR_ERR(plane_state);
        goto unlock;
    }
    if (plane_state->fb != mtk_cpu.fb) {
        ret = 0;
        goto unlock;
    }
    ret = drm_atomic_set_crtc_for_plane(plane_state, NULL);
    if (ret)
        goto unlock;
    drm_atomic_set_fb_for_plane(plane_state, NULL);
    crtc_state = drm_atomic_get_crtc_state(state, mtk_cpu.crtc);
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

static int mtk_cpu_render_init(void)
{
    int ret;

    ret = resolve_symbols();
    if (ret)
        return ret;
    mutex_init(&mtk_cpu.draw_lock);
    INIT_DELAYED_WORK(&mtk_cpu.work, mtk_cpu_worker);
    mtk_cpu.initialized = true;
    schedule_delayed_work(&mtk_cpu.work, HZ);
    return 0;
}

static int mtk_cpu_begin_draw(void)
{
    if (!READ_ONCE(mtk_cpu.attached) || !mtk_cpu.pixels)
        return -ENODEV;
    mutex_lock(&mtk_cpu.draw_lock);
    if (!READ_ONCE(mtk_cpu.attached) || !mtk_cpu.pixels) {
        mutex_unlock(&mtk_cpu.draw_lock);
        return -ENODEV;
    }
    return 0;
}

static void mtk_cpu_end_draw(void)
{
    wmb();
    mutex_unlock(&mtk_cpu.draw_lock);
}

static int mtk_cpu_clear(u32 color)
{
    int ret = mtk_cpu_begin_draw();

    if (ret)
        return ret;
    msm_cpu_raster_clear(mtk_cpu.pixels, mtk_cpu.width, mtk_cpu.height,
                         mtk_cpu.pitch, color);
    mtk_cpu_end_draw();
    return 0;
}

static int mtk_cpu_line(int x0, int y0, int x1, int y1, u32 color,
                        u32 thickness)
{
    int ret = mtk_cpu_begin_draw();

    if (ret)
        return ret;
    msm_cpu_raster_line(mtk_cpu.pixels, mtk_cpu.width, mtk_cpu.height,
                        mtk_cpu.pitch, x0, y0, x1, y1, color, thickness);
    mtk_cpu_end_draw();
    return 0;
}

static int mtk_cpu_rect(int left, int top, int right, int bottom, u32 color,
                        u32 thickness, bool filled)
{
    int ret = mtk_cpu_begin_draw();

    if (ret)
        return ret;
    msm_cpu_raster_rect(mtk_cpu.pixels, mtk_cpu.width, mtk_cpu.height,
                        mtk_cpu.pitch, left, top, right, bottom, color,
                        thickness, filled);
    mtk_cpu_end_draw();
    return 0;
}

static int mtk_cpu_circle(int center_x, int center_y, int radius, u32 color,
                          u32 thickness, bool filled)
{
    int ret = mtk_cpu_begin_draw();

    if (ret)
        return ret;
    msm_cpu_raster_circle(mtk_cpu.pixels, mtk_cpu.width, mtk_cpu.height,
                          mtk_cpu.pitch, center_x, center_y, radius, color,
                          thickness, filled);
    mtk_cpu_end_draw();
    return 0;
}

static int mtk_cpu_text(int left, int top, const char *text, u32 scale,
                        u32 color)
{
    int ret = mtk_cpu_begin_draw();

    if (ret)
        return ret;
    if (!text) {
        mutex_unlock(&mtk_cpu.draw_lock);
        return -EINVAL;
    }
    msm_cpu_raster_text(mtk_cpu.pixels, mtk_cpu.width, mtk_cpu.height,
                        mtk_cpu.pitch, left, top, text, scale, color);
    mtk_cpu_end_draw();
    return 0;
}

static void mtk_cpu_render_exit(void)
{
    int ret;

    if (!mtk_cpu.initialized)
        return;
    mtk_cpu.initialized = false;
    cancel_delayed_work_sync(&mtk_cpu.work);
    mutex_lock(&mtk_cpu.draw_lock);
    WRITE_ONCE(mtk_cpu.attached, false);
    mutex_unlock(&mtk_cpu.draw_lock);
    if (mtk_cpu.fb) {
        ret = mtk_cpu_detach();
        if (ret) {
            WRITE_ONCE(mtk_cpu.attached, true);
            pr_warn_ratelimited("[lsdriver] MTK CPU framebuffer disable failed: %d\n",
                                ret);
            return;
        }
    }
    WRITE_ONCE(mtk_cpu.valid, false);
    mtk_cpu_atomic_check_wait_refs();
    if (mtk_cpu.hook_ref) {
        cmpxchg(&mtk_cpu.dev->mode_config.funcs,
                &mtk_cpu.hooked_funcs, mtk_cpu.original_funcs);
        mtk_cpu.hook_ref = false;
        module_put(THIS_MODULE);
    }
    release_image();
    if (mtk_cpu.minor)
        fn_drm_minor_release(mtk_cpu.minor);
    mtk_cpu.minor = NULL;
    mtk_cpu.dev = NULL;
}

static const struct cpu_render_ops mtk_cpu_render_ops = {
    .platform = CPU_RENDER_PLATFORM_MEDIATEK,
    .name = "mediatek",
    .init = mtk_cpu_render_init,
    .exit = mtk_cpu_render_exit,
    .clear = mtk_cpu_clear,
    .line = mtk_cpu_line,
    .rect = mtk_cpu_rect,
    .circle = mtk_cpu_circle,
    .text = mtk_cpu_text,
};

const struct cpu_render_ops *mtk_cpu_render_get_ops(void)
{
    return &mtk_cpu_render_ops;
}