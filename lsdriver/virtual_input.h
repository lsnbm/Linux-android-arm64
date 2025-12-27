#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/mutex.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/version.h>

// --- 配置区域 ---
#define VTOUCH_TRACKING_ID_BASE 40000
#define TARGET_SLOT_IDX 9 // Android 最大支持到 Slot 9 (第10个手指)
// ----------------

static DEFINE_MUTEX(g_lock);

static struct input_dev *g_touch_input_dev = NULL;
static struct input_mt *g_original_mt = NULL;
static struct input_mt *g_hijacked_mt = NULL;
static bool g_initialized = false;
static int g_tracking_id = -1;

// 核心函数：扩容并初始化 MT 结构体
// 策略：确保存储空间足够 10 个 Slot，但默认只告诉驱动有 9 个
static int hijack_init_slots(struct input_dev *dev)
{
    struct input_mt *old_mt = dev->mt;
    struct input_mt *new_mt;
    int old_num_slots;
    int alloc_num_slots;
    int size;

    if (!old_mt)
        return -1;

    old_num_slots = old_mt->num_slots;

    // 我们至少需要 10 个空间 (0-9)
    // 如果原设备小于 10，我们扩容到 10。
    // 如果原设备大于等于 10，我们保持原大小。
    alloc_num_slots = (old_num_slots < 10) ? 10 : old_num_slots;

    size = sizeof(struct input_mt) + alloc_num_slots * sizeof(struct input_mt_slot);

    new_mt = kzalloc(size, GFP_KERNEL);
    if (!new_mt)
    {
        pr_debug("vtouch: 内存分配失败\n");
        return -1;
    }

    // 复制旧数据
    new_mt->trkid = old_mt->trkid;
    // --- 关键欺骗 ---
    // 我们将 num_slots 设置为 9 (前提是 alloc >= 10)
    // 这样物理驱动只会循环 0-8，Slot 9 对它来说是不存在的
    new_mt->num_slots = 9;

    new_mt->flags = old_mt->flags;
    new_mt->frame = old_mt->frame;
    // new_mt->red = old_mt->red; // 视内核版本
    // 如果原驱动使用了 red 矩阵（软追踪），我们也必须分配，否则内核计算时会崩溃
    if (old_mt->red)
    {
        // 矩阵大小通常是 slot数 * slot数
        // 我们使用 alloc_num_slots (实际分配的大小) 来计算，保证足够大
        unsigned int red_size = alloc_num_slots * alloc_num_slots * sizeof(int);

        new_mt->red = kzalloc(red_size, GFP_KERNEL);
        if (!new_mt->red)
        {
            pr_debug("vtouch: red matrix 内存分配失败\n");
            kfree(new_mt);
            return -ENOMEM;
        }
        // 注意：新分配的内存是全0，这正是我们需要的（表示没有关联代价），不需要从 old_mt 复制
    }

    // 复制旧的 slot 状态 (只复制原来有效的部分)
    memcpy(new_mt->slots, old_mt->slots, old_num_slots * sizeof(struct input_mt_slot));

    // --- Flag 设置 ---
    new_mt->flags &= ~INPUT_MT_DROP_UNUSED; // 即使没更新也不要丢弃
    new_mt->flags |= INPUT_MT_DIRECT;
    new_mt->flags &= ~INPUT_MT_POINTER; // 禁用内核自动按键计算，防止 Key Flapping

    // 替换指针
    g_original_mt = old_mt;
    g_hijacked_mt = new_mt;
    dev->mt = new_mt;

    // --- 告诉 Android 我们有 10 个 Slot ---
    // 虽然 num_slots 设为 9 (给驱动看)，但我们要告诉 Android 我们支持到 9 (即10个)
    input_set_abs_params(dev, ABS_MT_SLOT, 0, 9, 0, 0);

    pr_debug("vtouch: 劫持成功！内存容量: %d, 伪装 Slot 数: 9. 目标 Slot: 9\n", alloc_num_slots);

    return 0;
}

// 统计当前所有活跃的手指（物理 + 虚拟）
static void vtouch_update_global_keys(struct input_dev *dev, bool virtual_is_touching)
{
    int i;
    int physical_count = 0;
    int total_count = 0;

    // 1. 遍历前9个物理 Slot (0-8)，检查是否有真实手指按在屏幕上
    // 通过读取 mt 结构体中的 tracking_id 来判断
    // tracking_id != -1 表示该 Slot 处于按下状态
    for (i = 0; i < 9; i++)
    {
        if (input_mt_get_value(&dev->mt->slots[i], ABS_MT_TRACKING_ID) != -1)
        {
            physical_count++;
        }
    }

    // 2. 计算总手指数量
    total_count = physical_count + (virtual_is_touching ? 1 : 0);

    // 3. 根据总数量正确上报全局按键
    // 只要有任意手指（真实或虚拟），BTN_TOUCH 就必须是 1
    input_report_key(dev, BTN_TOUCH, total_count > 0);

    // 处理具体的手指数量标志 (Android通常只看 BTN_TOUCH)
    if (total_count == 0)
    {
        input_report_key(dev, BTN_TOOL_FINGER, 0);
        input_report_key(dev, BTN_TOOL_DOUBLETAP, 0);
    }
    else if (total_count == 1)
    {
        input_report_key(dev, BTN_TOOL_FINGER, 1);
        input_report_key(dev, BTN_TOOL_DOUBLETAP, 0);
    }
    else
    {                                                 // 2个或更多手指
        input_report_key(dev, BTN_TOOL_FINGER, 0);    // 单指标志熄灭
        input_report_key(dev, BTN_TOOL_DOUBLETAP, 1); // 双指标志亮起
    }
}

static void vtouch_send_report(int x, int y, bool is_touching)
{
    if (!g_touch_input_dev)
        return;

    // --- 瞬间开启 Slot 9 ---
    // 物理驱动读到的是 9，平时不会碰 Slot 9。
    // 我们现在把门打开，写入数据，然后立刻关上。
    g_touch_input_dev->mt->num_slots = 10;

    // 1. 选中 Slot 9
    input_mt_slot(g_touch_input_dev, TARGET_SLOT_IDX);

    // 2. 报告状态
    input_mt_report_slot_state(g_touch_input_dev, MT_TOOL_FINGER, is_touching);

    if (is_touching)
    {
        input_report_abs(g_touch_input_dev, ABS_MT_POSITION_X, x);
        input_report_abs(g_touch_input_dev, ABS_MT_POSITION_Y, y);

        // 伪造面积 (必须有)
        if (test_bit(ABS_MT_TOUCH_MAJOR, g_touch_input_dev->absbit))
            input_report_abs(g_touch_input_dev, ABS_MT_TOUCH_MAJOR, 10);
        if (test_bit(ABS_MT_WIDTH_MAJOR, g_touch_input_dev->absbit))
            input_report_abs(g_touch_input_dev, ABS_MT_WIDTH_MAJOR, 10);

        // 压力
        if (test_bit(ABS_MT_PRESSURE, g_touch_input_dev->absbit))
            input_report_abs(g_touch_input_dev, ABS_MT_PRESSURE, 60);
    }

    // 3. 同步 Slot 帧
    // 这里因为 num_slots 暂时是 10，sync_frame 会扫描到 Slot 9 并生成事件
    input_mt_sync_frame(g_touch_input_dev);

    // --- 瞬间关闭 Slot 9 ---
    // 恢复为 9，防止物理驱动下一次中断时清洗 Slot 9
    g_touch_input_dev->mt->num_slots = 9;

    // 4. 手动控制按键 (因为禁用了 POINTER 标志)
    // 智能计算并上报全局按键
    // 不再盲目发送 0 或 1，而是根据当前所有手指状态决定
    vtouch_update_global_keys(g_touch_input_dev, is_touching);

    // 5. 提交总帧
    input_sync(g_touch_input_dev);
}

static int _match_physical_touchscreen(struct device *dev, void *data)
{
    struct input_dev *input_dev = to_input_dev(dev);
    struct input_dev **result = data;

    if (test_bit(EV_ABS, input_dev->evbit) &&
        test_bit(ABS_MT_SLOT, input_dev->absbit) &&
        input_dev->mt)
    {
        if (test_bit(BTN_TOUCH, input_dev->keybit))
        {
            *result = input_dev;
            return 1;
        }
    }
    return 0;
}

int v_touch_init(int *max_x, int *max_y)
{
    struct input_dev *found_dev = NULL;
    struct class *input_class = NULL;
    int ret = 0;

    if (!max_x || !max_y)
    {
        return -EINVAL;
    }

    mutex_lock(&g_lock);

    if (g_initialized)
    {
        *max_x = g_touch_input_dev->absinfo[ABS_MT_POSITION_X].maximum;
        *max_y = g_touch_input_dev->absinfo[ABS_MT_POSITION_Y].maximum;
        mutex_unlock(&g_lock);
        return 0;
    }

    input_class = (struct class *)generic_kallsyms_lookup_name("input_class");
    pr_debug(" input_class 地址: %px\n", input_class);

    if (!input_class)
    {
        ret = -EFAULT;
        goto cleanup;
    }

    class_for_each_device(input_class, NULL, &found_dev, _match_physical_touchscreen);

    if (!found_dev)
    {

        ret = -ENODEV;
        goto cleanup;
    }

    get_device(&found_dev->dev);
    g_touch_input_dev = found_dev;

    if (hijack_init_slots(g_touch_input_dev) != 0)
    {

        ret = -ENOMEM;
        put_device(&g_touch_input_dev->dev);
        goto cleanup;
    }

    *max_x = g_touch_input_dev->absinfo[ABS_MT_POSITION_X].maximum;
    *max_y = g_touch_input_dev->absinfo[ABS_MT_POSITION_Y].maximum;

    g_initialized = true;

    pr_debug("vtouch: [19] 初始化成功 [%s] X_MAX=%d, Y_MAX=%d\n",
             g_touch_input_dev->name ? g_touch_input_dev->name : "Unknown", *max_x, *max_y);

    mutex_unlock(&g_lock);

    return 0;

cleanup:
    mutex_unlock(&g_lock);
    return ret;
}

void v_touch_destroy(void)
{

    mutex_lock(&g_lock);

    if (!g_initialized)
    {
        // 未初始化，直接跳过
        goto cleanup;
    }

    // 检查是否有未抬起的坐标
    if (g_tracking_id != -1)
    {
        g_tracking_id = -1;
        vtouch_send_report(0, 0, false);
        // 已发送抬起信号
    }

    // 检查是否需要恢复mt结构体
    if (g_touch_input_dev && g_original_mt)
    {
        // 恢复原始 g_original_mt
        g_touch_input_dev->mt = g_original_mt;
        input_set_abs_params(g_touch_input_dev, ABS_MT_SLOT, 0, g_original_mt->num_slots - 1, 0, 0);
    }

    if (g_hijacked_mt)
    {

        // 先释放 red 指针指向的内存
        if (g_hijacked_mt->red)
        {
            kfree(g_hijacked_mt->red);
            g_hijacked_mt->red = NULL;
        }

        // 释放 hijacked_mt
        kfree(g_hijacked_mt);
        g_hijacked_mt = NULL;
    }

    if (g_touch_input_dev)
    {
        // put_device 释放引用
        put_device(&g_touch_input_dev->dev);
        g_touch_input_dev = NULL;
    }

    g_initialized = false;
    g_tracking_id = -1;

cleanup:
    mutex_unlock(&g_lock);
}

void v_touch_down(int x, int y)
{
    mutex_lock(&g_lock);
    if (!g_initialized)
        goto cleanup;

    if (g_tracking_id == -1)
    {
        g_tracking_id = VTOUCH_TRACKING_ID_BASE;
        vtouch_send_report(x, y, true);
    }

cleanup:
    mutex_unlock(&g_lock);
}

void v_touch_move(int x, int y)
{
    mutex_lock(&g_lock);
    if (!g_initialized || g_tracking_id == -1)
        goto cleanup;
    vtouch_send_report(x, y, true);
cleanup:
    mutex_unlock(&g_lock);
}

void v_touch_up(void)
{
    mutex_lock(&g_lock);
    if (!g_initialized || g_tracking_id == -1)
        goto cleanup;
    g_tracking_id = -1;
    vtouch_send_report(0, 0, false);
cleanup:
    mutex_unlock(&g_lock);
}
