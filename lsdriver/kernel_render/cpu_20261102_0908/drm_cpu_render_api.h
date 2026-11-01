/* SPDX-License-Identifier: GPL-2.0 */
#ifndef LSDRIVER_DRM_CPU_RENDER_API_H
#define LSDRIVER_DRM_CPU_RENDER_API_H

#include <linux/version.h>
#include <linux/xarray.h>
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0) && LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
#include <linux/dma-buf-map.h>
#endif
#include <drm/drm_client.h>

struct drm_cpu_render_api
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    struct drm_minor *(*minor_acquire)(struct xarray *, unsigned int);
    struct xarray *minors_xa;
#else
    struct drm_minor *(*minor_acquire)(unsigned int);
#endif
    void (*minor_release)(struct drm_minor *);
    int (*client_init)(struct drm_device *, struct drm_client_dev *, const char *, const struct drm_client_funcs *);
    void (*client_release)(struct drm_client_dev *);
    struct drm_client_buffer *(*client_framebuffer_create)(struct drm_client_dev *, u32, u32, u32);
    void (*client_buffer_delete)(struct drm_client_buffer *);
    int (*client_framebuffer_flush)(struct drm_client_buffer *, struct drm_rect *);
#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 15, 0)
    void *(*client_buffer_vmap)(struct drm_client_buffer *);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
    int (*client_buffer_vmap)(struct drm_client_buffer *, struct dma_buf_map *);
#else
    int (*client_buffer_vmap)(struct drm_client_buffer *, struct iosys_map *);
#endif
    void (*client_buffer_vunmap)(struct drm_client_buffer *);
    int (*client_modeset_probe)(struct drm_client_dev *, unsigned int, unsigned int);
    int (*client_modeset_commit)(struct drm_client_dev *);
    int (*client_modeset_dpms)(struct drm_client_dev *, int);
};

extern struct drm_cpu_render_api drm_cpu_render_api;

#endif
