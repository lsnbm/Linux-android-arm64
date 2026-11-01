/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/errno.h>
#include <linux/version.h>
#include <linux/module.h>
#include <drm/drm_atomic_uapi.h>
#include <drm/drm_device.h>
#include <drm/drm_file.h>
#include <drm/drm_fourcc.h>
#include <drm/drm_framebuffer.h>
#include <drm/drm_gem.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
#include <linux/xarray.h>
#endif

#include "../../../export_fun.h"
#include "msm_cpu_symbols.h"

#define MSM_CPU_NO_CFI __attribute__((no_sanitize("cfi", "kcfi")))

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
static struct drm_minor *(*fn_drm_minor_acquire)(struct xarray *, unsigned int);
static struct xarray *fn_drm_minors_xa;
#else
static struct drm_minor *(*fn_drm_minor_acquire)(unsigned int);
#endif
static void (*fn_drm_minor_release)(struct drm_minor *);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
static struct drm_gem_object *(*fn_msm_gem_new)(struct drm_device *,
                         size_t, u32);
static struct drm_framebuffer *(*fn_msm_framebuffer_init)(
    struct drm_device *, const struct drm_format_info *,
    const struct drm_mode_fb_cmd2 *, struct drm_gem_object **);
#else
static struct drm_gem_object *(*fn_msm_gem_new)(struct drm_device *, u32, u32);
#endif
static void *(*fn_msm_gem_get_vaddr)(struct drm_gem_object *);
static void (*fn_msm_gem_put_vaddr)(struct drm_gem_object *);
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 18, 0)
static struct drm_framebuffer *(*fn_msm_framebuffer_init)(
        struct drm_device *, const struct drm_mode_fb_cmd2 *,
        struct drm_gem_object **);
#endif

int msm_cpu_symbols_init(void)
{
#define RESOLVE(member, symbol) do { \
    member = (typeof(member))generic_kallsyms_lookup_name(symbol); \
    if (!member) \
        return -ENOENT; \
} while (0)
    RESOLVE(fn_drm_minor_acquire, "drm_minor_acquire");
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    fn_drm_minors_xa = (struct xarray *)generic_kallsyms_lookup_name(
            "drm_minors_xa");
    if (!fn_drm_minors_xa)
        return -ENOENT;
#endif
    RESOLVE(fn_drm_minor_release, "drm_minor_release");
    RESOLVE(fn_msm_gem_new, "msm_gem_new");
    RESOLVE(fn_msm_gem_get_vaddr, "msm_gem_get_vaddr");
    RESOLVE(fn_msm_gem_put_vaddr, "msm_gem_put_vaddr");
    RESOLVE(fn_msm_framebuffer_init, "msm_framebuffer_init");
#undef RESOLVE
    return 0;
}

struct drm_minor *MSM_CPU_NO_CFI msm_cpu_minor_acquire(unsigned int minor)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    return fn_drm_minor_acquire(fn_drm_minors_xa, minor);
#else
    return fn_drm_minor_acquire(minor);
#endif
}

void MSM_CPU_NO_CFI msm_cpu_minor_release(struct drm_minor *minor)
{
    fn_drm_minor_release(minor);
}

struct drm_gem_object *MSM_CPU_NO_CFI msm_cpu_gem_new(
        struct drm_device *dev, u32 size, u32 flags)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    return fn_msm_gem_new(dev, (size_t)size, flags);
#else
    return fn_msm_gem_new(dev, size, flags);
#endif
}

void *MSM_CPU_NO_CFI msm_cpu_gem_get_vaddr(struct drm_gem_object *object)
{
    return fn_msm_gem_get_vaddr(object);
}

void MSM_CPU_NO_CFI msm_cpu_gem_put_vaddr(struct drm_gem_object *object)
{
    fn_msm_gem_put_vaddr(object);
}

struct drm_framebuffer *MSM_CPU_NO_CFI msm_cpu_framebuffer_init(
        struct drm_device *dev, const struct drm_mode_fb_cmd2 *cmd,
        struct drm_gem_object **objects)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    const struct drm_format_info *info = drm_get_format_info(
            dev, cmd->pixel_format, cmd->modifier[0]);

    if (!info)
        return ERR_PTR(-EINVAL);
    return fn_msm_framebuffer_init(dev, info, cmd, objects);
#else
    return fn_msm_framebuffer_init(dev, cmd, objects);
#endif
}