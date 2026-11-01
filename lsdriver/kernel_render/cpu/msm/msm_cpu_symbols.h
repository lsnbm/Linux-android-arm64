/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LSDRIVER_MSM_CPU_SYMBOLS_H
#define LSDRIVER_MSM_CPU_SYMBOLS_H

#include <linux/types.h>

struct drm_device;
struct drm_framebuffer;
struct drm_gem_object;
struct drm_minor;
struct drm_mode_fb_cmd2;
int msm_cpu_symbols_init(void);
struct drm_minor *msm_cpu_minor_acquire(unsigned int minor);
void msm_cpu_minor_release(struct drm_minor *minor);
struct drm_gem_object *msm_cpu_gem_new(struct drm_device *dev,
                                       u32 size, u32 flags);
void *msm_cpu_gem_get_vaddr(struct drm_gem_object *object);
void msm_cpu_gem_put_vaddr(struct drm_gem_object *object);
struct drm_framebuffer *msm_cpu_framebuffer_init(
        struct drm_device *dev, const struct drm_mode_fb_cmd2 *cmd,
        struct drm_gem_object **objects);

#endif