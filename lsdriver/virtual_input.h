#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/compiler.h>

/*
在 Linux 输入子系统中，上报一个手指的事件不是通过一个结构体一次性发过去的，而是像下面这样流式（Stream）分步发送的：

步骤 1： ABS_MT_SLOT = 5 （修改全局当前槽位为 5）
步骤 2： ABS_MT_POSITION_X = 100 （向当前槽位写入 X）
步骤 3： ABS_MT_POSITION_Y = 200 （向当前槽位写入 Y）
步骤 4： SYN_REPORT （提交）

*/
// ==================== 配置调整 ====================
#define VTOUCH_TRACKING_ID_BASE 40000
#define ORIGINAL_SLOTS 10                // 物理驱动原始 slot 总数
#define PHYSICAL_SLOTS 5                 // 物理驱动占用 slot 0–4 (共5个)
#define VIRTUAL_SLOTS 5                  // 虚拟驱动占用 slot 5–9 (共5个)
#define VIRTUAL_SLOT_BASE PHYSICAL_SLOTS // 虚拟 slot 在硬件上的起始索引 (5)
#define TOTAL_SLOTS ORIGINAL_SLOTS       // 总 slot 数 (10)
#define VTOUCH_REPORT_RETRY 32           // 遇到真实驱动半帧上报时，短暂重试

// ==================== 动态符号导入 ====================
static void (*input_handle_event)(struct input_dev *dev, unsigned int type, unsigned int code, int value);

// 虚拟触摸上下文
static struct
{
    struct input_dev *dev;

    //  对应 5 个虚拟手指
    int tracking_ids[VIRTUAL_SLOTS];

    bool has_touch_major;
    bool has_width_major;
    bool has_pressure;
    bool global_keys_locked;
    bool initialized;
} vt = {
    // 5 个虚拟手指初始化为 -1
    .tracking_ids = {-1, -1, -1, -1, -1},
    .global_keys_locked = false,
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

// 修改触摸屏设备的slot数量
static inline int hijack_init_slots(struct input_dev *dev)
{
    struct input_mt *mt = dev->mt;

    if (!mt)
        return -EINVAL;

    /*
 这里把内核中触摸屏设备支持的slot截断到0-4(也就是5个数量)
 会有个问题:触摸屏驱动还会自己单独认为有0-9个slot，
     触摸驱动会继续遍历所有slot上报存活事件(注意:不是说有多少个真实手指就触摸驱动就上报多少，而是会固定遍历所有上报，被使用的slot就上报存活，没有使用的slot上报不存活)

     会出现这种情况:
         真实手指在触摸到4个时：驱动发(把当前活跃slot切换到4事件)，
         内核输入子系统检测到触摸屏设备支持到slot4 ,会把当前活跃dev->mt->slot写为4，
         驱动继续发(存活事件)，当前活跃slot就标记为存活了
         驱动会继续发(切换到slot5事件)
         内核输入子系统检测设备只支持到0-4，会拒绝dev->mt->slot写为5
         驱动继续发(不存活事件)，前面说了会固定循环

     关键:当前活跃 Slot 指针（dev->mt->slot）依然停留在上一步成功的 Slot 4 上
         然后跟在后面的不存活事件,内核输入子系统把当前活跃slot4标记为不存活!!!
         就出现4刚刚上报存活，就被上报不存活，
         存活和不存活间隙过于短了
         上层Android系统会直接当这个slot为无效,自然Android就不会响应这次的物理驱动事件，肉眼看就是手指按下系统不响应

    当前先不实现“避雷针”方案，仍保持真实驱动和虚拟触摸 5/5 分配：
     Slot 0 - 4（共5个）：分配给真实手指。
     Slot 5 - 9（共5个）：分配给虚拟手指。
         */
    mt->num_slots = PHYSICAL_SLOTS;

    // --- Flag 设置 ---
    mt->flags &= ~INPUT_MT_DROP_UNUSED; // 即使没更新也不要丢弃
    mt->flags |= INPUT_MT_DIRECT;
    mt->flags &= ~INPUT_MT_POINTER; // 禁用内核自动按键计算，防止 Key Flapping

    // --- 告诉 Android 我们有 10 个 Slot ---
    // 虽然触摸设备num_slots 设为 5 (给输入子系统看)，但我们要告诉 Android 我们支持到 10个
    input_set_abs_params(dev, ABS_MT_SLOT, 0, TOTAL_SLOTS - 1, 0, 0); // 0~9:10,0-10:11,so:-1

    return 0;
}
// 暂时剥夺(或恢复)整个输入设备发送"全局触摸状态"的能力，以此来强行拦截物理驱动的(UP)信号，防止虚拟滑动被打断。
static inline void set_global_key_bits(struct input_dev *dev, bool enable)
{
    if (enable)
    {
        set_bit(BTN_TOUCH, dev->keybit);
        set_bit(BTN_TOOL_FINGER, dev->keybit);
        set_bit(BTN_TOOL_DOUBLETAP, dev->keybit);
    }
    else
    {
        clear_bit(BTN_TOUCH, dev->keybit);
        clear_bit(BTN_TOOL_FINGER, dev->keybit);
        clear_bit(BTN_TOOL_DOUBLETAP, dev->keybit);
    }
}

// 锁内安全的全局按键更新
static inline void update_global_keys_locked(void)
{
    struct input_dev *dev = vt.dev;
    struct input_mt *mt = dev->mt;
    bool relock_keys = vt.global_keys_locked;
    int count = 0;
    int i;

    // 遍历前5个物理 Slot (0-4)，检查是否有真实手指按在屏幕上
    // 通过读取 mt 结构体中的 tracking_id 来判断
    // tracking_id != -1 表示该 Slot 处于按下状态
    for (i = 0; i < PHYSICAL_SLOTS; i++)
    {
        if (input_mt_get_value(&mt->slots[i], ABS_MT_TRACKING_ID) != -1)
            count++;
    }

    // 统计所有手指：物理+虚拟
    count += vt_active_count();

    /*
    如果当前正在锁定全局按键，keybit 是被清掉的。
    input_handle_event 仍然会检查 keybit，所以这里在 event_lock 内临时恢复能力，
    发完我们自己的 BTN_TOUCH/BTN_TOOL_* 后再清回去，只拦物理驱动，不拦虚拟注入。
    */
    if (relock_keys)
        set_global_key_bits(dev, true);

    // 注意：在持有 event_lock 时，绝对不能调用 input_report_key
    // 必须直接使用 input_handle_event 以防死锁
    input_handle_event(dev, EV_KEY, BTN_TOUCH, count > 0);
    input_handle_event(dev, EV_KEY, BTN_TOOL_FINGER, count == 1);
    input_handle_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, count >= 2);

    if (relock_keys)
        set_global_key_bits(dev, false);
}

// 设置全局触摸按键锁定状态：locked=true 时拦住物理驱动的 BTN_TOUCH/BTN_TOOL_*。
static void set_global_key_lock_state(struct input_dev *dev, bool locked)
{
    unsigned long flags;

    if (!dev)
        return;

    spin_lock_irqsave(&dev->event_lock, flags);
    set_global_key_bits(dev, !locked);
    vt.global_keys_locked = locked;
    spin_unlock_irqrestore(&dev->event_lock, flags);
}

// 调用者传 0–4，内部映射到硬件 slot 5–9
static inline int send_report(int vslot, int x, int y, bool touching)
{
    struct input_dev *dev = vt.dev;
    struct input_mt *mt = dev->mt;
    int hw_slot = VIRTUAL_SLOT_BASE + vslot;
    int tracking_id;
    int old_slot;
    int retry;
    unsigned long flags;

    if (!dev || !mt || !input_handle_event)
        return -ENODEV;

    if ((unsigned)vslot >= VIRTUAL_SLOTS)
        return -EINVAL;

    if (touching && vt.tracking_ids[vslot] == -1)
        return -EINVAL;

    tracking_id = touching ? vt.tracking_ids[vslot] : -1;

    /*
    input_event() 会拿 dev->event_lock。
    我们直接拿同一把锁，再调用 input_handle_event()，这样虚拟触摸的一整包事件不会被物理驱动插队。
    */
    for (retry = 0; retry < VTOUCH_REPORT_RETRY; retry++)
    {
        spin_lock_irqsave(&dev->event_lock, flags);

        /*
        dev->num_vals != 0 说明真实驱动可能已经写入了半帧事件但还没 SYN_REPORT。
        这时强行注入会把真实半帧和虚拟帧混在一起，所以先让路，等真实驱动把当前帧收尾。
        */
        if (likely(dev->num_vals == 0))
            break;

        spin_unlock_irqrestore(&dev->event_lock, flags);
        udelay(10);
    }

    if (retry == VTOUCH_REPORT_RETRY)
        return -EAGAIN;

    // 记住当前物理驱动正在操作的 slot
    old_slot = mt->slot;

    // 瞬间开启所有slot
    mt->num_slots = TOTAL_SLOTS;

    // 选中目标虚拟 slot
    input_handle_event(dev, EV_ABS, ABS_MT_SLOT, hw_slot);

    // 报告状态，注意了这里如果上报死亡：后续严禁对一个已经宣告死亡的 Slot 上报任何物理属性（ABS）。
    input_handle_event(dev, EV_ABS, ABS_MT_TRACKING_ID, tracking_id);

    if (touching)
    {
        // 上报坐标
        input_handle_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
        input_handle_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);

        // 上报伪造面积和压力
        if (vt.has_touch_major)
            input_handle_event(dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 10);
        if (vt.has_width_major)
            input_handle_event(dev, EV_ABS, ABS_MT_WIDTH_MAJOR, 10);
        if (vt.has_pressure)
            input_handle_event(dev, EV_ABS, ABS_MT_PRESSURE, 60);
    }

    // 删除 input_mt_sync_frame(dev);
    // 手动精准控制了虚拟 Slot 的所有属性，且是 Type B 协议，
    // 绝对不能调用 sync_frame，否则会强制刷新真实手指的残缺帧。导致真实手指也出现抖动

    // 恢复: 这是解决"跳跃"最核心的一步，把接下来的写入权还给刚才被打断的真实坑位。
    // 这样真实驱动即使醒来，它的坐标依然会安全地写进 old_slot，而不会污染我们的虚拟 Slot。
    input_handle_event(dev, EV_ABS, ABS_MT_SLOT, old_slot);

    // 上报结束立刻收回slot，物理驱动依然只看到 5 个 slot
    mt->num_slots = PHYSICAL_SLOTS;

    // 锁内安全地手动控制按键
    update_global_keys_locked();

    // 提交总帧
    input_handle_event(dev, EV_SYN, SYN_REPORT, 0);

    // 释放锁，物理驱动安全醒来，此时活跃 slot 已经复原
    spin_unlock_irqrestore(&dev->event_lock, flags);

    return 0;
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

    if (!input_handle_event)
    {
        input_handle_event = (void *)generic_kallsyms_lookup_name("input_handle_event");
        if (!input_handle_event)
        {
            pr_debug("vtouch: input_handle_event 查找失败\n");
            return -EFAULT;
        }
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

    // 发送所有仍按下的虚拟 slot 的抬起信号
    for (i = 0; i < VIRTUAL_SLOTS; i++)
    {
        if (vt.tracking_ids[i] != -1)
        {
            vt.tracking_ids[i] = -1;
            send_report(i, 0, 0, false);
        }
    }

    // 把控制权还给物理驱动
    if (vt.dev)
        set_global_key_lock_state(vt.dev, false);

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
    int old_tracking_id;
    int ret;
    bool last_virtual;

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
            // 第一个虚拟手指按下时，通常还没有锁定全局按键；send_report 会自己在 event_lock 内安全上报按键
            if (vt_active_count() == 1)
            {
                ret = send_report(slot, x, y, true);
                if (ret)
                {
                    vt.tracking_ids[slot] = -1;
                    return;
                }
                // 发送完毕立刻上锁
                // 此时物理手指无论怎么抬起，内核触发的 BTN_TOUCH=0 都会被静默丢弃，无法打断虚拟滑动
                set_global_key_lock_state(vt.dev, true);
            }
            else
            {
                // 已有虚拟手指按住（已锁），直接注入
                ret = send_report(slot, x, y, true);
                if (ret)
                    vt.tracking_ids[slot] = -1;
            }
        }
    }
    else if (op == op_up)
    {
        if (vt.tracking_ids[slot] != -1)
        {
            old_tracking_id = vt.tracking_ids[slot];
            vt.tracking_ids[slot] = -1;
            last_virtual = vt_active_count() == 0;

            // 虚拟手指抬起了，只有所有虚拟 slot 全部抬起时才解锁，才允许系统上报真实的抬起事件
            ret = send_report(slot, 0, 0, false);
            if (ret)
            {
                vt.tracking_ids[slot] = old_tracking_id;
                if (last_virtual)
                    set_global_key_lock_state(vt.dev, true);
                return;
            }

            // 最后一个虚拟手指抬起并成功提交后，再把全局按键能力还给物理驱动
            if (last_virtual)
                set_global_key_lock_state(vt.dev, false);
        }
    }
}
