

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/compiler.h>

// 配置
#define VTOUCH_TRACKING_ID_BASE 40000
#define TARGET_SLOT_IDX 9
#define PHYSICAL_SLOTS 9
#define TOTAL_SLOTS 10

// 虚拟触摸上下文
static struct
{
    struct input_dev *dev;
    struct input_mt *original_mt;
    struct input_mt *hijacked_mt;
    int tracking_id;

    bool has_touch_major;
    bool has_width_major;
    bool has_pressure;
    bool initialized;
} vt = {
    .tracking_id = -1,
    .initialized = false,
};

// 扩容并初始化 MT 结构体
// 策略：确保存储空间足够 10 个 Slot，但默认只告诉驱动有 9 个
static inline int hijack_init_slots(struct input_dev *dev)
{
    struct input_mt *old_mt = dev->mt;
    struct input_mt *new_mt;
    int old_num_slots, alloc_slots;
    size_t size;

    if (!old_mt)
        return -EINVAL;

    old_num_slots = old_mt->num_slots;

    // 我们至少需要 10 个空间 (0-9)
    // 如果原设备小于 10，我们扩容到 10。
    // 如果原设备大于等于 10，我们保持原大小。
    alloc_slots = (old_num_slots < TOTAL_SLOTS) ? TOTAL_SLOTS : old_num_slots;
    size = sizeof(struct input_mt) + alloc_slots * sizeof(struct input_mt_slot);

    new_mt = kzalloc(size, GFP_KERNEL);
    if (!new_mt)
        return -ENOMEM;
    // 复制旧数据
    new_mt->trkid = old_mt->trkid;
    // --- 关键欺骗 ---
    // 我们将 num_slots 设置为 9 (前提是 alloc >= 10)
    // 这样物理驱动只会循环 0-8，Slot 9 对它来说是不存在的
    new_mt->num_slots = PHYSICAL_SLOTS;
    new_mt->flags = old_mt->flags;
    new_mt->frame = old_mt->frame;
    // 如果原驱动使用了 red 矩阵（软追踪），我们也必须分配，否则内核计算时会崩溃
    // 矩阵大小通常是 slot数 * slot数
    // 我们使用 alloc_slots (实际分配的大小) 来计算，保证足够大
    if (old_mt->red)
    {
        size_t red_size = alloc_slots * alloc_slots * sizeof(int);
        new_mt->red = kzalloc(red_size, GFP_KERNEL);
        if (!new_mt->red)
        {
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
    vt.original_mt = old_mt;
    vt.hijacked_mt = new_mt;
    dev->mt = new_mt;
    // --- 告诉 Android 我们有 10 个 Slot ---
    // 虽然 num_slots 设为 9 (给驱动看)，但我们要告诉 Android 我们支持到 10个
    input_set_abs_params(dev, ABS_MT_SLOT, 0, TOTAL_SLOTS, 0, 0);

    return 0;
}

// 统计当前所有活跃的手指（物理 + 虚拟）并更新全局按键
static inline void update_global_keys(bool virtual_touching)
{
    struct input_dev *dev = vt.dev;
    struct input_mt *mt = dev->mt;
    int count = 0;
    int i;

    // 遍历前9个物理 Slot (0-8)，检查是否有真实手指按在屏幕上
    // 通过读取 mt 结构体中的 tracking_id 来判断
    // tracking_id != -1 表示该 Slot 处于按下状态
    for (i = 0; i < PHYSICAL_SLOTS; i++)
    {
        // 提取 tracking_id，通过直接的内存偏移访问替代函数调用
        if (input_mt_get_value(&mt->slots[i], ABS_MT_TRACKING_ID) != -1)
            count++;
    }

    // 计算总手指数量
    if (virtual_touching)
        count++;

    // 根据总数量正确上报全局按键
    // 只要有任意手指（真实或虚拟），BTN_TOUCH 就必须是 1
    input_report_key(dev, BTN_TOUCH, count > 0);
    // 处理具体的手指数量标志 (Android通常只看 BTN_TOUCH)
    input_report_key(dev, BTN_TOOL_FINGER, count == 1);
    input_report_key(dev, BTN_TOOL_DOUBLETAP, count >= 2);
}

static inline void send_report(int x, int y, bool touching)
{
    // 寄存器缓存，避免多次访问全局结构体内存
    struct input_dev *dev = vt.dev;
    struct input_mt *mt = dev->mt;
    int old_slot;
    unsigned long flags;

    // 关闭本地硬中断
    // 防止在执行虚拟触摸注入的这几微秒内，真实触摸屏硬件中断突然触发导致代码交错
    local_irq_save(flags);

    // 保存: 记住当前输入子系统正在操作的是哪个 Slot (极有可能是真实手指所在的 Slot)
    old_slot = dev->absinfo[ABS_MT_SLOT].value;

    //  瞬间开启 Slot 9
    mt->num_slots = TOTAL_SLOTS;

    // 选中 Slot 9
    input_event(dev, EV_ABS, ABS_MT_SLOT, TARGET_SLOT_IDX);

    // 报告状态，注意了如果上报死亡：严禁对一个已经宣告死亡的 Slot 上报任何物理属性（ABS）。
    input_mt_report_slot_state(dev, MT_TOOL_FINGER, touching);

    if (likely(touching))
    {
        input_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
        input_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);

        // 伪造面积和压力
        if (vt.has_touch_major)
            input_event(dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 10);
        if (vt.has_width_major)
            input_event(dev, EV_ABS, ABS_MT_WIDTH_MAJOR, 10);
        if (vt.has_pressure)
            input_event(dev, EV_ABS, ABS_MT_PRESSURE, 60);
    }

    // 删除 input_mt_sync_frame(dev);
    // 手动精准控制了 Slot 9 的所有属性，且是 Type B 协议，
    // 绝对不能调用 sync_frame，否则会强制刷新真实手指的残缺帧。导致真实手指也出现抖动

    // 瞬间关闭 Slot 9
    mt->num_slots = PHYSICAL_SLOTS;

    // 恢复: 这是解决“跳跃”最核心的一步！把接下来的写入权还给刚才被打断的真实坑位。
    // 这样真实驱动即使醒来，它的坐标依然会安全地写进 old_slot，而不会污染我们的 Slot 9。
    input_event(dev, EV_ABS, ABS_MT_SLOT, old_slot);

    // 手动控制按键
    update_global_keys(touching);

    // 提交总帧
    input_sync(dev);

    // 恢复中断
    local_irq_restore(flags);
}

static inline int match_touchscreen(struct device *dev, void *data)
{
    struct input_dev *input = to_input_dev(dev);
    struct input_dev **result = data;

    if (test_bit(EV_ABS, input->evbit) &&
        test_bit(ABS_MT_SLOT, input->absbit) &&
        test_bit(BTN_TOUCH, input->keybit) &&
        input->mt)
    {
        *result = input;
        return 1;
    }
    return 0;
}

// 锁定按键：暂时剥夺设备发送全局触摸状态的能力
static inline void lock_global_keys(struct input_dev *dev)
{
    clear_bit(BTN_TOUCH, dev->keybit);
    clear_bit(BTN_TOOL_FINGER, dev->keybit);
    clear_bit(BTN_TOOL_DOUBLETAP, dev->keybit);
}

// 解锁按键：恢复设备发送全局触摸状态的能力
static inline void unlock_global_keys(struct input_dev *dev)
{
    set_bit(BTN_TOUCH, dev->keybit);
    set_bit(BTN_TOOL_FINGER, dev->keybit);
    set_bit(BTN_TOOL_DOUBLETAP, dev->keybit);
}

static inline int v_touch_init(int *max_x, int *max_y)
{
    struct input_dev *found = NULL;
    struct class *input_class;
    int ret = 0;

    if (!max_x || !max_y)
        return -EINVAL;

    if (vt.initialized)
    {
        *max_x = vt.dev->absinfo[ABS_MT_POSITION_X].maximum;
        *max_y = vt.dev->absinfo[ABS_MT_POSITION_Y].maximum;
        return 0;
    }

    input_class = (struct class *)generic_kallsyms_lookup_name("input_class");
    if (!input_class)
    {
        pr_debug("vtouch: input_class 查找失败\n");
        return -EFAULT;
    }

    class_for_each_device(input_class, NULL, &found, match_touchscreen);
    if (!found)
    {
        pr_debug("vtouch: 未找到触摸屏设备\n");
        return -ENODEV;
    }

    get_device(&found->dev);
    vt.dev = found;

    ret = hijack_init_slots(found);
    if (ret)
    {
        pr_debug("vtouch: MT 劫持失败\n");
        put_device(&found->dev);
        vt.dev = NULL;
        return ret;
    }

    // 初始化时缓存设备能力，让 120Hz/240Hz 循环不再做原子位运算
    vt.has_touch_major = test_bit(ABS_MT_TOUCH_MAJOR, found->absbit);
    vt.has_width_major = test_bit(ABS_MT_WIDTH_MAJOR, found->absbit);
    vt.has_pressure = test_bit(ABS_MT_PRESSURE, found->absbit);

    *max_x = found->absinfo[ABS_MT_POSITION_X].maximum;
    *max_y = found->absinfo[ABS_MT_POSITION_Y].maximum;
    vt.initialized = true;

    return 0;
}

static inline void v_touch_destroy(void)
{
    // 防止重复调用
    if (!vt.initialized)
        return;
    // 把控制权还给物理驱动
    if (vt.dev)
    {
        unlock_global_keys(vt.dev);
    }
    // 发送抬起信号
    if (vt.tracking_id != -1)
    {
        vt.tracking_id = -1;
        send_report(0, 0, false);
    }

    // 恢复原始 MT
    if (vt.dev && vt.original_mt)
    {
        vt.dev->mt = vt.original_mt;
        input_set_abs_params(vt.dev, ABS_MT_SLOT, 0,
                             vt.original_mt->num_slots - 1, 0, 0);
    }
    // 释放劫持的 MT
    if (vt.hijacked_mt)
    {
        kfree(vt.hijacked_mt->red);
        kfree(vt.hijacked_mt);
        vt.hijacked_mt = NULL;
    }

    if (vt.dev)
    {
        put_device(&vt.dev->dev);
        vt.dev = NULL;
    }

    vt.original_mt = NULL;
    vt.initialized = false;
    vt.tracking_id = -1;
}

static inline void v_touch_event(req_op Op, int x, int y)
{

    if (unlikely(!vt.initialized))
        return;

    if (likely(Op == op_move))
    {
        if (likely(vt.tracking_id != -1))
        {
            send_report(x, y, true);
        }
    }
    else if (Op == op_down)
    {
        if (vt.tracking_id == -1)
        {
            vt.tracking_id = VTOUCH_TRACKING_ID_BASE;

            // 按下前，确保系统允许发送触摸按键
            unlock_global_keys(vt.dev);

            send_report(x, y, true);

            // 发送完毕立刻上锁！
            // 此时物理手指无论怎么抬起，内核触发的 BTN_TOUCH=0 都会被静默丢弃，绝对无法打断虚拟滑动
            lock_global_keys(vt.dev);
        }
    }
    else if (Op == op_up)
    {
        if (vt.tracking_id != -1)
        {
            vt.tracking_id = -1;

            // 虚拟手指抬起了，必须解锁，允许系统上报真实的抬起事件
            unlock_global_keys(vt.dev);

            send_report(0, 0, false);
        }
    }
}