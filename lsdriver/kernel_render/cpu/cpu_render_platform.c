/* SPDX-License-Identifier: GPL-2.0 */
#include <linux/errno.h>
#include <linux/version.h>

#include "../../export_fun.h"
#include "cpu_render_platform.h"

static const char *const msm_symbols[] = {
    "drm_minor_acquire",
    "drm_minor_release",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 6, 0)
    "drm_minors_xa",
#endif
    "msm_gem_new",
    "msm_framebuffer_init",
    "msm_gem_get_vaddr",
    "msm_gpu_submit",
};

static const char *const mediatek_symbols[] = {
    "drm_minor_acquire",
    "drm_minor_release",
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    "drm_minors_xa",
    "mtk_gem_create",
    "mtk_crtc_create",
    "mtk_gem_prime_vmap",
#else
    "mtk_drm_gem_create",
    "mtk_drm_crtc_create",
    "mtk_drm_gem_prime_vmap",
#endif
    "mtk_plane_init",
};

static bool symbol_set_present(const char *const *symbols, size_t count)
{
    size_t index;

    for (index = 0; index < count; index++)
        if (!generic_kallsyms_lookup_name(symbols[index]))
            return false;
    return true;
}

enum cpu_render_platform cpu_render_platform_detect(void)
{
    bool msm_present = symbol_set_present(msm_symbols, ARRAY_SIZE(msm_symbols));
    bool mediatek_present = symbol_set_present(mediatek_symbols,
                                              ARRAY_SIZE(mediatek_symbols));

    if (msm_present == mediatek_present)
        return CPU_RENDER_PLATFORM_UNKNOWN;
    return msm_present ? CPU_RENDER_PLATFORM_MSM : CPU_RENDER_PLATFORM_MEDIATEK;
}