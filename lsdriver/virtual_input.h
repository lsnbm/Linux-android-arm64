#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/compiler.h>

// 配置
#define VTOUCH_TRACKING_ID_BASE 40000    // 跟踪ID :Linux/Android 多点触摸协议 ABS_MT_TRACKING_ID
#define ORIGINAL_SLOTS 10                // 物理驱动原始 slot 总数（0–9）
#define PHYSICAL_SLOTS 4                 // 切割后留给物理驱动的 slot 数
#define VIRTUAL_SLOTS 6                  // 虚拟 slot 数量（占用 slot 4–9）
#define VIRTUAL_SLOT_BASE PHYSICAL_SLOTS // 虚拟 slot 在硬件上的起始索引
#define TOTAL_SLOTS ORIGINAL_SLOTS       // // 总 slot 数

// 虚拟触摸上下文
static struct
{
    struct input_dev *dev;

    // 每个虚拟 slot 独立管理
    int tracking_ids[VIRTUAL_SLOTS]; // -1 表示未按下

    bool has_touch_major;
    bool has_width_major;
    bool has_pressure;
    bool initialized;
} vt = {
    // 复合字面量初始化数组全为 -1
    .tracking_ids = {-1, -1, -1, -1, -1, -1},
    .initialized = false,
};

// 返回当前有多少虚拟 slot 处于按下状态
static inline int vt_active_count(void)
{
    int i, count = 0;
    for (i = 0; i < VIRTUAL_SLOTS; i++)
        if (vt.tracking_ids[i] != -1)
            count++;
    return count;
}

// 劫持：只做两件事
// 把 num_slots 从 10 砍到 4，让驱动看不到 slot 4–9
// 把 ABS_MT_SLOT 的 max 扩到 9，让 Android 知道有 10 个 slot
static inline int hijack_init_slots(struct input_dev *dev)
{
    struct input_mt *mt = dev->mt;

    if (!mt)
        return -EINVAL;

    // 直接砍掉后半段对驱动的可见性
    mt->num_slots = PHYSICAL_SLOTS; // 物理屏驱动只循环 slot 0–3

    // --- Flag 设置 ---
    mt->flags &= ~INPUT_MT_DROP_UNUSED; // 即使没更新也不要丢弃
    mt->flags |= INPUT_MT_DIRECT;
    mt->flags &= ~INPUT_MT_POINTER; // 禁用内核自动按键计算，防止 Key Flapping

    // --- 告诉 Android 我们有 10 个 Slot ---
    // 虽然 num_slots 设为 4 (给驱动看)，但我们要告诉 Android 我们支持到 10个
    input_set_abs_params(dev, ABS_MT_SLOT, 0, TOTAL_SLOTS - 1, 0, 0); // 0~9:10,0-10:11,so:-1

    return 0;
}

// 统计当前所有活跃的手指（物理 + 虚拟）并更新全局按键
static inline void update_global_keys(void)
{
    struct input_dev *dev = vt.dev;
    struct input_mt *mt = dev->mt;
    int count = 0;
    int i;

    // 遍历前4个物理 Slot (0-3)，检查是否有真实手指按在屏幕上
    // 通过读取 mt 结构体中的 tracking_id 来判断
    // tracking_id != -1 表示该 Slot 处于按下状态
    for (i = 0; i < PHYSICAL_SLOTS; i++)
    {
        // 提取 tracking_id
        if (input_mt_get_value(&mt->slots[i], ABS_MT_TRACKING_ID) != -1)
            count++;
    }

    // 统计所有手指：物理+虚拟
    count += vt_active_count();

    // 根据总数量正确上报全局按键
    // 只要有任意手指（真实或虚拟），BTN_TOUCH 就必须是 1
    input_report_key(dev, BTN_TOUCH, count > 0);
    // 处理具体的手指数量标志 (Android通常只看 BTN_TOUCH)
    input_report_key(dev, BTN_TOOL_FINGER, count == 1);
    input_report_key(dev, BTN_TOOL_DOUBLETAP, count >= 2);
}

// 调用者传 0–5，内部映射到硬件 slot 4–9
static inline void send_report(int vslot, int x, int y, bool touching)
{
    struct input_dev *dev = vt.dev;
    struct input_mt *mt = dev->mt;
    int hw_slot = VIRTUAL_SLOT_BASE + vslot;
    int old_slot;
    unsigned long flags;

    // 关闭本地硬中断
    // 防止在执行虚拟触摸注入的这几微秒内，真实触摸屏硬件中断突然触发导致代码交错
    local_irq_save(flags);

    // 保存: 记住当前输入子系统正在操作的是哪个 Slot (真实手指所在的 Slot)
    old_slot = dev->absinfo[ABS_MT_SLOT].value;

    // 瞬间开启所有slot
    mt->num_slots = TOTAL_SLOTS;

    // 选中目标虚拟 slot
    input_event(dev, EV_ABS, ABS_MT_SLOT, hw_slot);

    // 报告状态，注意了这里如果上报死亡：后续严禁对一个已经宣告死亡的 Slot 上报任何物理属性（ABS）。
    input_mt_report_slot_state(dev, MT_TOOL_FINGER, touching);

    if (touching)
    {
        // 上报坐标
        input_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
        input_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);

        // 上报伪造面积和压力
        if (vt.has_touch_major)
            input_event(dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 10);
        if (vt.has_width_major)
            input_event(dev, EV_ABS, ABS_MT_WIDTH_MAJOR, 10);
        if (vt.has_pressure)
            input_event(dev, EV_ABS, ABS_MT_PRESSURE, 60);
    }

    // 删除 input_mt_sync_frame(dev);
    // 手动精准控制了虚拟 Slot 的所有属性，且是 Type B 协议，
    // 绝对不能调用 sync_frame，否则会强制刷新真实手指的残缺帧。导致真实手指也出现抖动

    // 上报结束立刻收回slot，物理驱动依然只看到 4 个 slot
    mt->num_slots = PHYSICAL_SLOTS;

    // 恢复: 这是解决"跳跃"最核心的一步，把接下来的写入权还给刚才被打断的真实坑位。
    // 这样真实驱动即使醒来，它的坐标依然会安全地写进 old_slot，而不会污染我们的虚拟 Slot。
    input_event(dev, EV_ABS, ABS_MT_SLOT, old_slot);

    // 手动控制按键
    update_global_keys();

    // 提交总帧
    input_sync(dev);

    // 恢复中断
    local_irq_restore(flags);
}

static int match_touchscreen(struct device *dev, void *data)
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
static void lock_global_keys(struct input_dev *dev)
{
    clear_bit(BTN_TOUCH, dev->keybit);
    clear_bit(BTN_TOOL_FINGER, dev->keybit);
    clear_bit(BTN_TOOL_DOUBLETAP, dev->keybit);
}

// 解锁按键：恢复设备发送全局触摸状态的能力
static void unlock_global_keys(struct input_dev *dev)
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
    int i;

    // 防止重复调用
    if (!vt.initialized)
        return;

    // 把控制权还给物理驱动
    if (vt.dev)
        unlock_global_keys(vt.dev);

    // 发送所有仍按下的虚拟 slot 的抬起信号
    for (i = 0; i < VIRTUAL_SLOTS; i++)
    {
        if (vt.tracking_ids[i] != -1)
        {
            vt.tracking_ids[i] = -1;
            send_report(i, 0, 0, false);
        }
    }

    // 恢复 num_slots 为原始值，让驱动重新看到全部 10 个 slot
    if (vt.dev && vt.dev->mt)
    {
        vt.dev->mt->num_slots = ORIGINAL_SLOTS;
        input_set_abs_params(vt.dev, ABS_MT_SLOT, 0, ORIGINAL_SLOTS - 1, 0, 0);
        vt.dev->mt->flags |= INPUT_MT_DROP_UNUSED;
        vt.dev->mt->flags &= ~INPUT_MT_DIRECT;
    }

    if (vt.dev)
    {
        put_device(&vt.dev->dev);
        vt.dev = NULL;
    }

    vt.initialized = false;

    for (i = 0; i < VIRTUAL_SLOTS; i++)
        vt.tracking_ids[i] = -1;
}

static inline void v_touch_event(enum sm_req_op op, int slot, int x, int y)
{
    if (!vt.initialized)
        return;

    // 越界保护,slot定义的是int,不是short,与内核字节对齐吧
    if ((unsigned)slot >= VIRTUAL_SLOTS)
        return;

    if (op == op_move)
    {
        if (vt.tracking_ids[slot] != -1)
        {
            send_report(slot, x, y, true);
        }
    }
    else if (op == op_down)
    {
        if (vt.tracking_ids[slot] == -1)
        {
            vt.tracking_ids[slot] = VTOUCH_TRACKING_ID_BASE + slot;

            // 按下前，确保系统允许发送触摸按键
            // 只有第一个虚拟手指按下时才需要解锁
            if (vt_active_count() == 1)
            {
                unlock_global_keys(vt.dev);
                send_report(slot, x, y, true);
                // 发送完毕立刻上锁
                // 此时物理手指无论怎么抬起，内核触发的 BTN_TOUCH=0 都会被静默丢弃，无法打断虚拟滑动
                lock_global_keys(vt.dev);
            }
            else
            {
                // 已有虚拟手指按住（已锁），直接注入
                send_report(slot, x, y, true);
            }
        }
    }
    else if (op == op_up)
    {
        if (vt.tracking_ids[slot] != -1)
        {
            vt.tracking_ids[slot] = -1;

            // 虚拟手指抬起了，只有所有虚拟 slot 全部抬起时才解锁，才允许系统上报真实的抬起事件
            if (vt_active_count() == 0)
                unlock_global_keys(vt.dev);

            send_report(slot, 0, 0, false);
        }
    }
}