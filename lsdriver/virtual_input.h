#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/device.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/version.h>
#include <linux/compiler.h>
#include "lsdriver_log.h"

/*
在 Linux 输入子系统中，上报一个手指的事件不是通过一个结构体一次性发过去的，而是像下面这样流式（Stream）分步发送的：

步骤 1： ABS_MT_SLOT = 5 （修改全局当前槽位为 5）
步骤 2： ABS_MT_POSITION_X = 100 （向当前槽位写入 X）
步骤 3： ABS_MT_POSITION_Y = 200 （向当前槽位写入 Y）
步骤 4： SYN_REPORT （提交）

*/
// ==================== 配置调整 ====================
#define MAX_VIRTUAL_SLOTS 16
// 运行时可配置的触摸参数（在 init_virtual_input_params 中动态初始化）
static int vtouch_tracking_id_base = 0; // 虚拟手指 tracking_id 起始值
static int original_slots = 0;          // 物理驱动原始 slot 总数
static int physical_slots = 0;          // 物理驱动占用 slot 数量
static int virtual_slots = 0;           // 虚拟驱动占用 slot 数量
static int virtual_slot_base = 0;       // 虚拟 slot 在硬件上的起始索引 (= physical_slots)

// 虚拟触摸上下文
static struct
{
    struct input_dev *dev;

    //  对应虚拟手指的 tracking id，最大支持 MAX_VIRTUAL_SLOTS 个
    int tracking_ids[MAX_VIRTUAL_SLOTS];

    bool has_touch_major;
    bool has_width_major;
    bool has_pressure;
    bool global_keys_locked;
    bool initialized;
} vt = {
    .global_keys_locked = false,
    .initialized = false,
};
// 返回当前有多少虚拟 slot 处于按下状态
static inline int vt_active_count(void)
{
    int i, count = 0;
    for (i = 0; i < virtual_slots; i++)
        if (vt.tracking_ids[i] != -1) count++;
    return count;
}

// 动态初始化虚拟触摸的运行时参数，若设备实际 slot 数不足，init 后 v_touch_init 会返回 -EINVAL。
static inline int init_virtual_input_params(int requested_virtual_slots)
{
    int i;

    if (requested_virtual_slots <= 0 || requested_virtual_slots > MAX_VIRTUAL_SLOTS) return -EINVAL;

    virtual_slots = requested_virtual_slots;
    original_slots = 10; // 固定以 10 个 slot 作为总池
    physical_slots = original_slots - virtual_slots;
    if (physical_slots <= 0) return -EINVAL;

    virtual_slot_base = physical_slots;
    vtouch_tracking_id_base = 40000;

    for (i = 0; i < MAX_VIRTUAL_SLOTS; i++) vt.tracking_ids[i] = -1;

    return 0;
}

// 修改触摸屏设备的slot数量
static inline int hijack_init_slots(struct input_dev *dev)
{
    struct input_mt *mt = dev->mt;

    if (!mt) return -EINVAL;

    /*
 这里把内核中触摸屏设备支持的slot截断到0~(physical_slots-1)
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

     当前方案：保留若干 slot 给物理驱动，其余 slot 分配给虚拟触摸。
         */
    mt->num_slots = physical_slots;

    // --- Flag 设置 ---
    mt->flags &= ~INPUT_MT_DROP_UNUSED; // 即使没更新也不要丢弃
    mt->flags |= INPUT_MT_DIRECT;
    mt->flags &= ~INPUT_MT_POINTER; // 禁用内核自动按键计算，防止 Key Flapping

    // --- 告诉 Android 我们支持 original_slots 个 Slot ---
    // 虽然触摸设备 num_slots 被截断为 physical_slots (给输入子系统看),
    // 但我们要让 Android 看到完整的 slot 范围, 否则虚拟手指无法使用。
    input_set_abs_params(dev, ABS_MT_SLOT, 0, original_slots - 1, 0, 0); // 例如 0~9 => 10 个

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

// 全局按键更新
static inline void update_global_keys(void)
{
    struct input_dev *dev = vt.dev;
    struct input_mt *mt = dev->mt;
    bool relock_keys = vt.global_keys_locked;
    int count = 0;
    int i;

    // 遍历物理 Slot (0 ~ physical_slots-1)，检查是否有真实手指按在屏幕上
    // 通过读取 mt 结构体中的 tracking_id 来判断
    // tracking_id != -1 表示该 Slot 处于按下状态
    for (i = 0; i < physical_slots; i++)
    {
        if (input_mt_get_value(&mt->slots[i], ABS_MT_TRACKING_ID) != -1) count++;
    }

    // 统计所有手指：物理+虚拟
    count += vt_active_count();

    /*
    如果当前正在锁定全局按键，keybit 是被清掉的。
    input_event 会检查 keybit，所以这里临时恢复能力，
    发完我们自己的 BTN_TOUCH/BTN_TOOL_* 后再清回去，只拦物理驱动，不拦虚拟注入。
    */
    if (relock_keys) set_global_key_bits(dev, true);

    // 注意：这里绝对不能调用 input_report_key，必须直接使用 input_event
    input_event(dev, EV_KEY, BTN_TOUCH, count > 0);
    input_event(dev, EV_KEY, BTN_TOOL_FINGER, count == 1);
    input_event(dev, EV_KEY, BTN_TOOL_DOUBLETAP, count >= 2);

    if (relock_keys) set_global_key_bits(dev, false);
}

// 调用者传 0 ~ virtual_slots-1，内部映射到硬件 slot
static inline int send_report(int vslot, int x, int y, bool touching)
{
    struct input_dev *dev = vt.dev;
    struct input_mt *mt = dev->mt;
    int hw_slot = virtual_slot_base + vslot;
    int tracking_id;
    int old_slot;

    if (!dev || !mt) return -ENODEV;

    if ((unsigned)vslot >= virtual_slots) return -EINVAL;

    if (touching && vt.tracking_ids[vslot] == -1) return -EINVAL;

    tracking_id = touching ? vt.tracking_ids[vslot] : -1;

    // 记住当前物理驱动正在操作的 slot
    old_slot = mt->slot;

    // 瞬间开启所有 slot
    mt->num_slots = original_slots;

    // 选中目标虚拟 slot
    input_event(dev, EV_ABS, ABS_MT_SLOT, hw_slot);

    // 报告状态，注意了这里如果上报死亡：后续严禁对一个已经宣告死亡的 Slot 上报任何物理属性（ABS）。
    input_event(dev, EV_ABS, ABS_MT_TRACKING_ID, tracking_id);

    if (touching)
    {
        // 上报坐标
        input_event(dev, EV_ABS, ABS_MT_POSITION_X, x);
        input_event(dev, EV_ABS, ABS_MT_POSITION_Y, y);

        // 上报伪造面积和压力
        if (vt.has_touch_major) input_event(dev, EV_ABS, ABS_MT_TOUCH_MAJOR, 10);
        if (vt.has_width_major) input_event(dev, EV_ABS, ABS_MT_WIDTH_MAJOR, 10);
        if (vt.has_pressure) input_event(dev, EV_ABS, ABS_MT_PRESSURE, 60);
    }

    // 删除 input_mt_sync_frame(dev);
    // 手动精准控制了虚拟 Slot 的所有属性，且是 Type B 协议，
    // 绝对不能调用 sync_frame，否则会强制刷新真实手指的残缺帧。导致真实手指也出现抖动

    // 恢复: 这是解决"跳跃"最核心的一步，把接下来的写入权还给刚才被打断的真实坑位。
    // 这样真实驱动即使醒来，它的坐标依然会安全地写进 old_slot，而不会污染我们的虚拟 Slot。
    input_event(dev, EV_ABS, ABS_MT_SLOT, old_slot);

    // 上报结束立刻收回slot，物理驱动只看到 physical_slots 个 slot
    mt->num_slots = physical_slots;

    // 更新全局按键状态
    update_global_keys();

    // 提交总帧
    input_event(dev, EV_SYN, SYN_REPORT, 0);

    return 0;
}

static int match_touchscreen(struct device *dev, void *data)
{
    struct input_dev *input = to_input_dev(dev);
    struct input_dev **result = data;

    if (test_bit(EV_ABS, input->evbit) && test_bit(ABS_MT_SLOT, input->absbit) && test_bit(BTN_TOUCH, input->keybit) && input->mt)
    {
        *result = input;
        return 1;
    }
    return 0;
}

static inline int v_touch_init(int request_virtual_slots, int *max_x, int *max_y)
{
    struct input_dev *found = NULL;
    struct class *input_class;
    int ret;

    if (!max_x || !max_y) return -EINVAL;

    if (vt.initialized)
    {
        *max_x = vt.dev->absinfo[ABS_MT_POSITION_X].maximum;
        *max_y = vt.dev->absinfo[ABS_MT_POSITION_Y].maximum;
        return 0;
    }

    // request_virtual_slots == 0 表示用户不启用虚拟触摸，直接返回成功
    if (request_virtual_slots <= 0)
    {
        *max_x = 0;
        *max_y = 0;
        return 0;
    }

    ret = init_virtual_input_params(request_virtual_slots);
    if (ret) return ret;

    input_class = (struct class *)generic_kallsyms_lookup_name("input_class");
    if (!input_class)
    {
        ls_log_tag("vtouch", "input_class 查找失败\n");
        return -EFAULT;
    }

    class_for_each_device(input_class, NULL, &found, match_touchscreen);
    if (!found)
    {
        ls_log_tag("vtouch", "未找到触摸屏设备\n");
        return -ENODEV;
    }

    get_device(&found->dev);
    vt.dev = found;

    ret = hijack_init_slots(found);
    if (ret)
    {
        ls_log_tag("vtouch", "MT 劫持失败\n");
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
    if (!vt.initialized) return;

    // 发送所有仍按下的虚拟 slot 的抬起信号
    for (i = 0; i < virtual_slots; i++)
    {
        if (vt.tracking_ids[i] != -1)
        {
            vt.tracking_ids[i] = -1;
            send_report(i, 0, 0, false);
        }
    }

    // 把控制权还给物理驱动
    if (vt.dev)
    {
        set_global_key_bits(vt.dev, true);
        vt.global_keys_locked = false;
    }

    // 恢复 num_slots 为原始值，让驱动重新看到全部 original_slots 个 slot
    if (vt.dev && vt.dev->mt)
    {
        vt.dev->mt->num_slots = original_slots;
        input_set_abs_params(vt.dev, ABS_MT_SLOT, 0, original_slots - 1, 0, 0);
        vt.dev->mt->flags |= INPUT_MT_DROP_UNUSED;
        vt.dev->mt->flags &= ~INPUT_MT_DIRECT;
    }

    if (vt.dev)
    {
        put_device(&vt.dev->dev);
        vt.dev = NULL;
    }

    vt.initialized = false;

    for (i = 0; i < virtual_slots; i++) vt.tracking_ids[i] = -1;
}

static inline void v_touch_event(enum request_op op, int slot, int x, int y)
{
    int old_tracking_id;
    int ret;
    bool last_virtual;
    int max_x;
    int max_y;

    if (!vt.initialized) return;

    // 越界保护,slot定义的是int,不是short,与内核字节对齐吧
    if ((unsigned)slot >= virtual_slots) return;

    // 坐标安全检查：只检查按下/移动，抬起事件不依赖 x/y。
    // ABS 最大值本身也拒绝，避免 TouchUp(1,1,1,1) 这类脏坐标变成 raw 最大点后参与下一次 DOWN。和防止其他异常状态坐标上报
    if (op == request_op_touch_down || op == request_op_touch_move)
    {
        if (!vt.dev || !vt.dev->absinfo) return;

        max_x = vt.dev->absinfo[ABS_MT_POSITION_X].maximum;
        max_y = vt.dev->absinfo[ABS_MT_POSITION_Y].maximum;

        if (max_x <= 0 || max_y <= 0) return;

        if (x < 0 || y < 0 || x >= max_x || y >= max_y) return;
    }

    if (op == request_op_touch_move)
    {
        if (vt.tracking_ids[slot] != -1)
        {
            send_report(slot, x, y, true);
        }
    }
    else if (op == request_op_touch_down)
    {
        if (vt.tracking_ids[slot] == -1)
        {
            vt.tracking_ids[slot] = vtouch_tracking_id_base + slot;

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
                set_global_key_bits(vt.dev, false);
                vt.global_keys_locked = true;
            }
            else
            {
                // 已有虚拟手指按住（已锁），直接注入
                ret = send_report(slot, x, y, true);
                if (ret) vt.tracking_ids[slot] = -1;
            }
        }
    }
    else if (op == request_op_touch_up)
    {
        if (vt.tracking_ids[slot] != -1)
        {
            old_tracking_id = vt.tracking_ids[slot];
            vt.tracking_ids[slot] = -1;
            last_virtual = vt_active_count() == 0;

            // 虚拟手指抬起了
            send_report(slot, 0, 0, false);

            // 最后一个虚拟手指抬起了，再把全局按键能力还给物理驱动，
            // 这里不要看最后的虚拟slot是否抬起成功，强行把自己记录slot被抬起，一定要解锁设备的全局按键能力，然后设备驱动清理残留slot
            if (last_virtual)
            {
                set_global_key_bits(vt.dev, true);
                vt.global_keys_locked = false;
            }
        }
    }
}
