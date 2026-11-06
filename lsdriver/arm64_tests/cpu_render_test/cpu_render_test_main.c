// SPDX-License-Identifier: GPL-2.0
#include <linux/errno.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>

#include "../../kernel_render/cpu/cpu_raster.h"

#define TEST_WIDTH 640U
#define TEST_HEIGHT 640U
#define TEST_PITCH (TEST_WIDTH * sizeof(u32))
#define TEST_GUARD_SIZE 64U
#define TEST_ITERATIONS 3U
#define TEST_GUARD_BYTE 0xa5U

static bool hardware;
module_param(hardware, bool, 0400);
MODULE_PARM_DESC(hardware, "Submit a five-second overlay on the active DSI display");

struct test_buffer
{
    u8 *allocation;
    u8 *pixels;
    size_t allocation_size;
};

#include "../../kernel_render/cpu/cpu_raster.c"
#include "../../kernel_render/cpu/drm_cpu_render.c"
#include "../../kernel_render/cpu/cpu_render.c"

static struct
{
    struct drm_device dev;
    struct drm_connector connector;
    struct drm_connector_state connector_state;
    struct drm_crtc crtc;
    struct drm_crtc_state crtc_state;
    struct drm_plane planes[3];
    struct drm_plane_state plane_states[3];
    struct drm_object_properties properties[3];
    struct drm_property zpos;
    struct drm_property alpha;
    struct drm_property blend;
    struct drm_property multirect;
    struct drm_property_enum premultiplied;
    uint64_t zpos_range[2];
    uint64_t alpha_range[2];
    struct drm_client_buffer buffers[2];
    struct drm_framebuffer framebuffers[3];
    uint32_t format;
    unsigned int allocated;
    unsigned int freed;
    unsigned int checked;
    unsigned int committed;
    unsigned int deleted;
    unsigned int released;
    unsigned int backed_off;
    int flush_error;
    int commit_error;
    bool deadlock;
    bool locked;
    bool invalid;
} fixture;

struct test_atomic_state
{
    struct drm_atomic_state base;
    struct drm_plane_state pending;
    uint64_t blend_value;
    bool crtc_added;
    bool checked;
};

static struct drm_atomic_state *test_state_alloc(struct drm_device *dev)
{
    struct test_atomic_state *state = kzalloc(sizeof(*state), GFP_KERNEL);
    if (!state) return NULL;
    state->base.dev = dev;
    kref_init(&state->base.ref);
    fixture.allocated++;
    return &state->base;
}

static void test_state_clear(struct drm_atomic_state *base)
{
    struct test_atomic_state *state = container_of(base, struct test_atomic_state, base);
    memset(&state->pending, 0, sizeof(state->pending));
    state->blend_value = 0;
    state->crtc_added = false;
    state->checked = false;
}

static void test_state_free(struct kref *ref)
{
    fixture.freed++;
    kfree(container_of(ref, struct test_atomic_state, base.ref));
}

static void test_acquire_init(struct drm_modeset_acquire_ctx *ctx, uint32_t flags)
{
    memset(ctx, 0, sizeof(*ctx));
}

static void test_acquire_fini(struct drm_modeset_acquire_ctx *ctx)
{
    if (fixture.locked) fixture.invalid = true;
}

static int test_lock_all(struct drm_device *dev, struct drm_modeset_acquire_ctx *ctx)
{
    if (fixture.deadlock)
    {
        fixture.deadlock = false;
        return -EDEADLK;
    }
    fixture.locked = true;
    return 0;
}

static void test_drop_locks(struct drm_modeset_acquire_ctx *ctx)
{
    fixture.locked = false;
}

static int test_backoff(struct drm_modeset_acquire_ctx *ctx)
{
    fixture.backed_off++;
    fixture.locked = false;
    return 0;
}

static struct drm_crtc_state *test_get_crtc(struct drm_atomic_state *base, struct drm_crtc *crtc)
{
    struct test_atomic_state *state = container_of(base, struct test_atomic_state, base);
    state->crtc_added = true;
    return &fixture.crtc_state;
}

static struct drm_plane_state *test_get_plane(struct drm_atomic_state *base, struct drm_plane *plane)
{
    struct test_atomic_state *state = container_of(base, struct test_atomic_state, base);
    state->pending = *plane->state;
    return &state->pending;
}

static int test_set_crtc(struct drm_plane_state *state, struct drm_crtc *crtc)
{
    state->crtc = crtc;
    return 0;
}

static void test_set_fb(struct drm_plane_state *state, struct drm_framebuffer *fb)
{
    state->fb = fb;
}

static int test_get_property(struct drm_mode_object *obj, struct drm_property *prop, uint64_t *value)
{
    *value = 5;
    return 0;
}

static int test_set_property(struct drm_atomic_state *base, struct drm_file *file,
                              struct drm_mode_object *obj, struct drm_property *prop, uint64_t value)
{
    struct test_atomic_state *state = container_of(base, struct test_atomic_state, base);
    if (prop == &fixture.blend)
    {
        state->blend_value = value;
        return 0;
    }
    if (value > prop->values[1]) return -ERANGE;
    if (prop == &fixture.zpos) state->pending.zpos = value;
    else if (prop == &fixture.alpha) state->pending.alpha = value;
    else return -EINVAL;
    return 0;
}

static int test_check_only(struct drm_atomic_state *base)
{
    struct test_atomic_state *state = container_of(base, struct test_atomic_state, base);
    if (!fixture.locked || !state->crtc_added || base->allow_modeset) fixture.invalid = true;
    fixture.checked++;
    state->checked = true;
    if (state->pending.plane == &fixture.planes[1]) return -ENOSPC;
    return 0;
}

static int test_commit(struct drm_atomic_state *base)
{
    struct test_atomic_state *state = container_of(base, struct test_atomic_state, base);
    if (!fixture.locked || !state->crtc_added || state->checked || base->allow_modeset)
        fixture.invalid = true;
    if (state->pending.fb && state->blend_value != 2) fixture.invalid = true;
    fixture.committed++;
    if (fixture.commit_error) return fixture.commit_error;
    *state->pending.plane->state = state->pending;
    return 0;
}

static int test_flush(struct drm_client_buffer *buffer, struct drm_rect *rect)
{
    for (unsigned int index = 0; index < ARRAY_SIZE(fixture.planes); index++)
        if (fixture.plane_states[index].fb == buffer->fb) fixture.invalid = true;
    return fixture.flush_error;
}

static void test_delete(struct drm_client_buffer *buffer)
{
    for (unsigned int index = 0; index < ARRAY_SIZE(fixture.planes); index++)
        if (fixture.plane_states[index].fb == buffer->fb) fixture.invalid = true;
    fixture.deleted++;
}

static void test_release(struct drm_client_dev *client)
{
    fixture.released++;
}

static const struct drm_plane_funcs test_plane_funcs;

static void setup_fixture(struct test_buffer *buffers)
{
    memset(&fixture, 0, sizeof(fixture));
    memset(&drm_cpu_render, 0, sizeof(drm_cpu_render));
    drm_cpu_api = (typeof(drm_cpu_api)){
        .state_alloc = test_state_alloc, .state_clear = test_state_clear, .state_free = test_state_free,
        .acquire_init = test_acquire_init, .acquire_fini = test_acquire_fini,
        .lock_all = test_lock_all, .drop_locks = test_drop_locks, .backoff = test_backoff,
        .get_crtc_state = test_get_crtc, .get_plane_state = test_get_plane,
        .set_crtc = test_set_crtc, .set_fb = test_set_fb,
        .get_property = test_get_property, .set_property = test_set_property,
        .check_only = test_check_only, .commit = test_commit,
        .framebuffer_flush = test_flush, .framebuffer_delete = test_delete, .client_release = test_release,
    };
    INIT_LIST_HEAD(&fixture.dev.mode_config.connector_list);
    INIT_LIST_HEAD(&fixture.dev.mode_config.plane_list);
    fixture.connector.connector_type = DRM_MODE_CONNECTOR_DSI;
    fixture.connector.state = &fixture.connector_state;
    fixture.connector_state.crtc = &fixture.crtc;
    list_add_tail(&fixture.connector.head, &fixture.dev.mode_config.connector_list);
    fixture.crtc.state = &fixture.crtc_state;
    fixture.crtc_state.active = true;
    fixture.crtc_state.mode.hdisplay = 1264;
    fixture.crtc_state.mode.vdisplay = 2780;
    strcpy(fixture.zpos.name, "zpos");
    fixture.zpos.flags = DRM_MODE_PROP_RANGE;
    fixture.zpos.num_values = 2;
    fixture.zpos_range[1] = 7;
    fixture.zpos.values = fixture.zpos_range;
    strcpy(fixture.alpha.name, "alpha");
    fixture.alpha.flags = DRM_MODE_PROP_RANGE;
    fixture.alpha.num_values = 2;
    fixture.alpha_range[1] = 255;
    fixture.alpha.values = fixture.alpha_range;
    strcpy(fixture.blend.name, "blend_op");
    fixture.blend.flags = DRM_MODE_PROP_ENUM;
    INIT_LIST_HEAD(&fixture.blend.enum_list);
    strcpy(fixture.premultiplied.name, "premultiplied");
    fixture.premultiplied.value = 2;
    list_add_tail(&fixture.premultiplied.head, &fixture.blend.enum_list);
    strcpy(fixture.multirect.name, "multirect_mode");
    fixture.format = DRM_FORMAT_ARGB8888;
    for (unsigned int index = 0; index < ARRAY_SIZE(fixture.planes); index++)
    {
        fixture.planes[index].type = index == 1 ? DRM_PLANE_TYPE_OVERLAY : DRM_PLANE_TYPE_PRIMARY;
        fixture.planes[index].possible_crtcs = 1;
        fixture.planes[index].state = &fixture.plane_states[index];
        fixture.planes[index].funcs = &test_plane_funcs;
        fixture.planes[index].format_count = 1;
        fixture.planes[index].format_types = &fixture.format;
        fixture.planes[index].base.properties = &fixture.properties[index];
        fixture.properties[index].count = 3;
        fixture.properties[index].properties[0] = &fixture.zpos;
        fixture.properties[index].properties[1] = &fixture.alpha;
        fixture.properties[index].properties[2] = &fixture.blend;
        fixture.plane_states[index].plane = &fixture.planes[index];
        list_add_tail(&fixture.planes[index].head, &fixture.dev.mode_config.plane_list);
    }
    fixture.plane_states[0].crtc = &fixture.crtc;
    fixture.plane_states[0].fb = &fixture.framebuffers[2];
    for (unsigned int index = 0; index < ARRAY_SIZE(fixture.buffers); index++)
    {
        fixture.buffers[index].fb = &fixture.framebuffers[index];
        fixture.buffers[index].pitch = TEST_PITCH;
        drm_cpu_render.buffers[index] = &fixture.buffers[index];
        drm_cpu_render.pixels[index] = buffers[index].pixels;
    }
    drm_cpu_render.client.dev = &fixture.dev;
    drm_cpu_render.initialized = true;
    cpu_render_active = true;
}

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

static int test_all_primitives(struct test_buffer *buffers)
{
    u32 iteration;
    int ret;

    for (iteration = 0; iteration < TEST_ITERATIONS; iteration++)
    {
        struct test_buffer *buffer = &buffers[drm_cpu_render.back];
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

#define REQUIRE(condition) \
    do { if (!(condition)) { pr_emerg("cpu_render_test: FAIL line=%d %s\n", __LINE__, #condition); return -EINVAL; } } while (0)

static int test_transactions(struct test_buffer *buffers)
{
    REQUIRE(!fixture.invalid && fixture.checked == 2 && fixture.committed == TEST_ITERATIONS);
    REQUIRE(fixture.plane_states[2].crtc_x == 312 && fixture.plane_states[2].crtc_y == 1070);
    REQUIRE(fixture.plane_states[2].src_w == 640U << 16 && fixture.plane_states[2].src_h == 640U << 16);
    REQUIRE(fixture.plane_states[2].zpos == 6 && fixture.plane_states[2].alpha == 255);
    REQUIRE(cpu_render_end_frame() == -EINVAL && cpu_render_clear(0) == -EINVAL);
    REQUIRE(cpu_render_begin_frame() == 0);
    REQUIRE(cpu_render_begin_frame() == -EBUSY && cpu_render_exit() == -EBUSY);
    REQUIRE(cpu_render_end_frame() == 0);

    unsigned int back = drm_cpu_render.back;
    unsigned int commits = fixture.committed;
    fixture.flush_error = -EIO;
    REQUIRE(cpu_render_begin_frame() == 0 && cpu_render_end_frame() == -EIO);
    REQUIRE(drm_cpu_render.back == back && fixture.committed == commits);
    fixture.flush_error = 0;
    fixture.commit_error = -EIO;
    REQUIRE(cpu_render_begin_frame() == 0 && cpu_render_end_frame() == -EIO);
    REQUIRE(drm_cpu_render.back == back && fixture.committed == commits + 1);
    REQUIRE(cpu_render_exit() == -EIO && !fixture.deleted && cpu_render_active);
    fixture.commit_error = 0;
    fixture.deadlock = true;
    REQUIRE(cpu_render_begin_frame() == 0 && cpu_render_end_frame() == 0);
    REQUIRE(fixture.backed_off == 1 && drm_cpu_render.back != back);

    fixture.crtc_state.active = false;
    REQUIRE(cpu_render_begin_frame() == 0 && cpu_render_end_frame() == -ENODEV);
    fixture.crtc_state.active = true;
    fixture.crtc_state.mode.vdisplay = 320;
    REQUIRE(cpu_render_begin_frame() == 0 && cpu_render_end_frame() == -ERANGE);
    fixture.crtc_state.mode.vdisplay = 2780;
    REQUIRE(cpu_render_exit() == 0);
    REQUIRE(!fixture.plane_states[2].fb && !fixture.plane_states[2].crtc);
    REQUIRE(fixture.deleted == 2 && fixture.released == 1 && fixture.allocated == fixture.freed);
    REQUIRE(!fixture.invalid && cpu_render_begin_frame() == -ENODEV);

    setup_fixture(buffers);
    fixture.plane_states[1].crtc = &fixture.crtc;
    fixture.plane_states[1].fb = &fixture.framebuffers[2];
    fixture.plane_states[2].crtc = &fixture.crtc;
    fixture.plane_states[2].fb = &fixture.framebuffers[2];
    REQUIRE(cpu_render_begin_frame() == 0 && cpu_render_end_frame() == -ENOSPC);
    REQUIRE(!fixture.committed && !drm_cpu_render.back);
    fixture.plane_states[2].fb = NULL;
    fixture.plane_states[2].crtc = NULL;
    fixture.plane_states[1].fb = NULL;
    fixture.plane_states[1].crtc = NULL;
    fixture.properties[1].properties[3] = &fixture.multirect;
    fixture.properties[1].count = 4;
    REQUIRE(cpu_render_begin_frame() == 0 && cpu_render_end_frame() == 0);
    REQUIRE(fixture.checked == 1 && drm_cpu_render.plane == &fixture.planes[2]);
    fixture.plane_states[2].fb = &fixture.framebuffers[2];
    commits = fixture.committed;
    REQUIRE(cpu_render_begin_frame() == 0 && cpu_render_end_frame() == -EBUSY);
    REQUIRE(cpu_render_exit() == 0 && fixture.committed == commits);
    REQUIRE(fixture.plane_states[2].fb == &fixture.framebuffers[2]);
    REQUIRE(fixture.deleted == 2 && fixture.released == 1 && fixture.allocated == fixture.freed);
    REQUIRE(!fixture.invalid && !fixture.locked);
    return 0;
}

static int __init cpu_render_test_init(void)
{
    if (hardware)
    {
        int ret = cpu_render_init();
        if (ret) return ret;
        unsigned int frames = 0;
        unsigned long deadline = jiffies + 5 * HZ;
        while (time_before(jiffies, deadline))
        {
            ret = cpu_render_begin_frame();
            if (ret) break;
            ret = cpu_render_clear(0xff101820U);
            if (!ret) ret = cpu_render_text(224, 160, "LSDRIVER", 4, 0xffffffffU);
            if (!ret) ret = cpu_render_rect(140, 220, 500, 420, 0xff00ff00U, 0, true);
            int present_ret = cpu_render_end_frame();
            if (!ret) ret = present_ret;
            if (ret) break;
            frames++;
            usleep_range(16000, 17000);
        }
        int detach_ret = cpu_render_exit();
        if (detach_ret)
        {
            __module_get(THIS_MODULE);
            pr_err("cpu_render_test: detach failed ret=%d; module pinned until reboot\n", detach_ret);
            return 0;
        }
        pr_info("cpu_render_test: hardware submissions=%u ret=%d detached=1\n", frames, ret);
        return ret;
    }
    struct test_buffer buffers[2] = {};
    int ret = -ENOMEM;

    for (unsigned int index = 0; index < ARRAY_SIZE(buffers); index++)
    {
        buffers[index].allocation_size = 2 * TEST_GUARD_SIZE + TEST_WIDTH * TEST_HEIGHT * sizeof(u32);
        buffers[index].allocation = vmalloc(buffers[index].allocation_size);
        if (!buffers[index].allocation) goto out;
        memset(buffers[index].allocation, TEST_GUARD_BYTE, buffers[index].allocation_size);
        buffers[index].pixels = buffers[index].allocation + TEST_GUARD_SIZE;
    }
    setup_fixture(buffers);
    ret = test_all_primitives(buffers);
    if (!ret) ret = test_transactions(buffers);
out:
    if (drm_cpu_render.frame_owner == current)
    {
        drm_cpu_render.frame_owner = NULL;
        mutex_unlock(&drm_cpu_lock);
    }
    for (unsigned int index = 0; index < ARRAY_SIZE(buffers); index++) vfree(buffers[index].allocation);

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
