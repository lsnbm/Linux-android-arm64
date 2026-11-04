// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "../../kernel_render/cpu/cpu_raster.h"

#define TEST_WIDTH 640U
#define TEST_HEIGHT 640U
#define TEST_PITCH (TEST_WIDTH * sizeof(u32))
#define TEST_GUARD_SIZE 64U
#define TEST_ITERATIONS 3U
#define TEST_GUARD_BYTE 0xa5U

struct test_buffer
{
    u8 *allocation;
    u8 *pixels;
    size_t allocation_size;
};

static struct test_buffer *active_buffer;
static bool frame_active;

/* Use the production raster and public wrapper with an isolated memory backend. */
#include "../../kernel_render/cpu/cpu_raster.c"

int drm_cpu_render_init(void)
{
    return 0;
}

int drm_cpu_render_exit(void)
{
    return 0;
}

int drm_cpu_render_begin_frame(void)
{
    if (!active_buffer) return -ENODEV;
    if (frame_active) return -EBUSY;
    frame_active = true;
    return 0;
}

int drm_cpu_render_end_frame(void)
{
    if (!frame_active) return -EINVAL;
    frame_active = false;
    return 0;
}

int drm_cpu_render_clear(u32 color)
{
    if (!active_buffer || !frame_active) return -EINVAL;
    cpu_raster_clear(active_buffer->pixels, TEST_WIDTH, TEST_HEIGHT, TEST_PITCH, color);
    return 0;
}

int drm_cpu_render_line(int x0, int y0, int x1, int y1, u32 color, u32 thickness)
{
    if (!active_buffer || !frame_active) return -EINVAL;
    cpu_raster_line(active_buffer->pixels, TEST_WIDTH, TEST_HEIGHT, TEST_PITCH,
                    x0, y0, x1, y1, color, thickness);
    return 0;
}

int drm_cpu_render_rect(int left, int top, int right, int bottom,
                        u32 color, u32 thickness, bool filled)
{
    if (!active_buffer || !frame_active) return -EINVAL;
    cpu_raster_rect(active_buffer->pixels, TEST_WIDTH, TEST_HEIGHT, TEST_PITCH,
                    left, top, right, bottom, color, thickness, filled);
    return 0;
}

int drm_cpu_render_circle(int center_x, int center_y, int radius,
                          u32 color, u32 thickness, bool filled)
{
    if (!active_buffer || !frame_active) return -EINVAL;
    cpu_raster_circle(active_buffer->pixels, TEST_WIDTH, TEST_HEIGHT, TEST_PITCH,
                      center_x, center_y, radius, color, thickness, filled);
    return 0;
}

int drm_cpu_render_text(int left, int top, const char *text, u32 scale, u32 color)
{
    if (!active_buffer || !frame_active) return -EINVAL;
    cpu_raster_text(active_buffer->pixels, TEST_WIDTH, TEST_HEIGHT, TEST_PITCH,
                    left, top, text, scale, color);
    return 0;
}

#include "../../kernel_render/cpu/cpu_render.c"

static bool guard_is_intact(const struct test_buffer *buffer)
{
    size_t index;

    for (index = 0; index < TEST_GUARD_SIZE; index++)
    {
        if (buffer->allocation[index] != TEST_GUARD_BYTE ||
            buffer->pixels[TEST_WIDTH * TEST_HEIGHT * sizeof(u32) + index] != TEST_GUARD_BYTE)
            return false;
    }
    return true;
}

static bool buffer_is_color(const struct test_buffer *buffer, u32 color)
{
    const u32 *pixels = (const u32 *)buffer->pixels;
    u32 index;

    for (index = 0; index < TEST_WIDTH * TEST_HEIGHT; index++)
        if (pixels[index] != color) return false;
    return true;
}

static bool buffer_has_color(const struct test_buffer *buffer, u32 color)
{
    const u32 *pixels = (const u32 *)buffer->pixels;
    u32 index;

    for (index = 0; index < TEST_WIDTH * TEST_HEIGHT; index++)
        if (pixels[index] == color) return true;
    return false;
}

static int test_all_primitives(struct test_buffer *buffer)
{
    u32 iteration;
    int ret;

    for (iteration = 0; iteration < TEST_ITERATIONS; iteration++)
    {
        ret = cpu_render_begin_frame();
        if (ret) return -EIO;

        pr_emerg("cpu_render_test: iteration=%u clear\n", iteration);
        ret = cpu_render_clear(0xff101820U);
        if (!ret && (!guard_is_intact(buffer) || !buffer_is_color(buffer, 0xff101820U))) ret = -EFAULT;
        if (ret) goto frame_fail;

        pr_emerg("cpu_render_test: iteration=%u line\n", iteration);
        ret = cpu_render_line(20, 80, 360, 360, 0xffff453aU, 6);
        if (!ret && (!guard_is_intact(buffer) || !buffer_has_color(buffer, 0xffff453aU))) ret = -EFAULT;
        if (ret) goto frame_fail;

        pr_emerg("cpu_render_test: iteration=%u rect\n", iteration);
        ret = cpu_render_rect(40, 120, 280, 300, 0xff30d5c8U, 4, false);
        if (!ret) ret = cpu_render_rect(80, 160, 180, 240, 0xffffd60aU, 2, true);
        if (!ret && (!guard_is_intact(buffer) ||
                     !buffer_has_color(buffer, 0xff30d5c8U) ||
                     !buffer_has_color(buffer, 0xffffd60aU))) ret = -EFAULT;
        if (ret) goto frame_fail;

        pr_emerg("cpu_render_test: iteration=%u circle\n", iteration);
        ret = cpu_render_circle(320, 320, 120, 0xff64d2ffU, 6, false);
        if (!ret) ret = cpu_render_circle(480, 420, 50, 0xffff375fU, 2, true);
        if (!ret && (!guard_is_intact(buffer) ||
                     !buffer_has_color(buffer, 0xff64d2ffU) ||
                     !buffer_has_color(buffer, 0xffff375fU))) ret = -EFAULT;
        if (ret) goto frame_fail;

        pr_emerg("cpu_render_test: iteration=%u text\n", iteration);
        ret = cpu_render_text(48, 520, "LSDRIVER", 4, 0xffffffffU);
        if (!ret && (!guard_is_intact(buffer) || !buffer_has_color(buffer, 0xffffffffU))) ret = -EFAULT;
        if (ret) goto frame_fail;
        if (cpu_render_end_frame()) return -EIO;
        continue;

frame_fail:
        cpu_render_end_frame();
        return ret;
    }
    return 0;
}

static int __init cpu_render_test_init(void)
{
    struct test_buffer buffer;
    int ret;

    buffer.allocation_size = TEST_GUARD_SIZE +
                             TEST_WIDTH * TEST_HEIGHT * sizeof(u32) +
                             TEST_GUARD_SIZE;
    buffer.allocation = vmalloc(buffer.allocation_size);
    if (!buffer.allocation) return -ENOMEM;

    memset(buffer.allocation, TEST_GUARD_BYTE, buffer.allocation_size);
    buffer.pixels = buffer.allocation + TEST_GUARD_SIZE;
    active_buffer = &buffer;
    ret = cpu_render_init();
    if (!ret) ret = test_all_primitives(&buffer);
    cpu_render_exit();
    active_buffer = NULL;
    vfree(buffer.allocation);

    if (ret)
    {
        pr_emerg("cpu_render_test: FAILED ret=%d\n", ret);
        return ret;
    }
    pr_info("cpu_render_test: PASS iterations=%u\n", TEST_ITERATIONS);
    return 0;
}

static void __exit cpu_render_test_exit(void)
{
    pr_info("cpu_render_test: unloaded\n");
}

module_init(cpu_render_test_init);
module_exit(cpu_render_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Portable CPU render primitive test");
