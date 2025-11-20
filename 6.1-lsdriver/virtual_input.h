// #ifndef VIRTUAL_INPUT_H
// #define VIRTUAL_INPUT_H

// #include <linux/module.h>
// #include <linux/kernel.h>
// #include <linux/init.h>
// #include <linux/input.h>
// #include <linux/input/mt.h>
// #include <linux/device.h>
// #include <linux/slab.h>
// #include <linux/kallsyms.h>
// #include <linux/random.h> 
// #include <linux/delay.h>  

// #define MAX_VIRTUAL_SLOTS 10 // 我们最多模拟10个手指

// // 描述一个虚拟触摸点的状态
// struct virtual_touch_slot
// {
//     bool active;     // 此插槽是否正在被使用
//     int tracking_id; // 此触摸点的唯一跟踪ID
// };

// 
// static struct input_dev *real_ts_dev = NULL; // 指向被克隆的物理触摸屏设备
// static struct input_dev *v_input_dev = NULL; // 我们的虚拟触摸屏设备

// // --- 虚拟触摸状态机变量 ---
// static struct virtual_touch_slot v_slots[MAX_VIRTUAL_SLOTS];
// static int active_touch_count = 0;
// static int next_tracking_id = 1000; // 跟踪ID的起始值

// static int slots; // 保存分配的触摸点槽位

// static int match_and_set_touchscreen(struct device *dev, void *data)
// {
//     struct input_dev *input_dev = to_input_dev(dev);

//     // 通过设备能力判断是否为多点触摸屏
//     if (test_bit(EV_ABS, input_dev->evbit) &&
//         test_bit(ABS_MT_POSITION_X, input_dev->absbit) &&
//         test_bit(ABS_MT_POSITION_Y, input_dev->absbit))
//     {
//         // 确保我们不会克隆我们自己或其他已知的虚拟设备
//         if (input_dev->id.bustype == BUS_VIRTUAL)
//         {
//             return 0;
//         }

//         pr_debug("克隆触摸: 发现物理触摸屏设备: '%s' (Bus: %d)\n", input_dev->name, input_dev->id.bustype);

//         // 获取设备引用，防止其在使用中被卸载
//         real_ts_dev = input_get_device(input_dev);
//         return 1; // 返回1表示已找到并停止遍历
//     }

//     return 0;
// }

// // 初始化虚拟触摸状态机
// static inline void virtual_touch_init(void)
// {
//     memset(v_slots, 0, sizeof(v_slots));
//     active_touch_count = 0;
//     pr_debug("克隆触摸: 虚拟触摸状态机已初始化。\n");
// }


// //初始化调用
// static int __maybe_unused create_virtual_touch_from_clone(void)
// {
//     int error;
//     int i;
//     struct class *input_class_ptr;

//     // 查找并锁定物理触摸屏设备 
//     input_class_ptr = (struct class *)generic_kallsyms_lookup_name("input_class");
//     if (!input_class_ptr)
//     {
//         pr_err("克隆触摸: 严重错误 - 'input_class' 符号未找到！\n");
//         return -EFAULT;
//     }

//     class_for_each_device(input_class_ptr, NULL, NULL, match_and_set_touchscreen);

//     if (!real_ts_dev)
//     {
//         pr_err("克隆触摸: 未找到可供克隆的物理触摸屏, 模块加载中止。\n");
//         return -ENODEV;
//     }

//     // 分配并克隆设备属性 
//     v_input_dev = input_allocate_device();
//     if (!v_input_dev)
//     {
//         pr_err("克隆触摸: 分配虚拟输入设备内存失败。\n");
//         input_put_device(real_ts_dev); // 释放之前获取的设备引用
//         return -ENOMEM;
//     }

//     pr_debug("克隆触摸: 正在从 '%s' 克隆属性。\n", real_ts_dev->name);

//     // 克隆所有关键属性
//     v_input_dev->name = kasprintf(GFP_KERNEL, "%s", real_ts_dev->name);
//     if (real_ts_dev->phys)
//         v_input_dev->phys = kasprintf(GFP_KERNEL, "%s", real_ts_dev->phys);
//     if (real_ts_dev->uniq)
//         v_input_dev->uniq = kasprintf(GFP_KERNEL, "%s", real_ts_dev->uniq);

//     v_input_dev->id.bustype = real_ts_dev->id.bustype;
//     v_input_dev->id.vendor = real_ts_dev->id.vendor;
//     v_input_dev->id.product = real_ts_dev->id.product;
//     v_input_dev->id.version = real_ts_dev->id.version;

//     // 克隆所有能力位图
//     memcpy(v_input_dev->evbit, real_ts_dev->evbit, sizeof(v_input_dev->evbit));
//     memcpy(v_input_dev->keybit, real_ts_dev->keybit, sizeof(v_input_dev->keybit));
//     memcpy(v_input_dev->relbit, real_ts_dev->relbit, sizeof(v_input_dev->relbit));
//     memcpy(v_input_dev->absbit, real_ts_dev->absbit, sizeof(v_input_dev->absbit));
//     memcpy(v_input_dev->mscbit, real_ts_dev->mscbit, sizeof(v_input_dev->mscbit));
//     memcpy(v_input_dev->ledbit, real_ts_dev->ledbit, sizeof(v_input_dev->ledbit));
//     memcpy(v_input_dev->sndbit, real_ts_dev->sndbit, sizeof(v_input_dev->sndbit));
//     memcpy(v_input_dev->ffbit, real_ts_dev->ffbit, sizeof(v_input_dev->ffbit));
//     memcpy(v_input_dev->swbit, real_ts_dev->swbit, sizeof(v_input_dev->swbit));
//     memcpy(v_input_dev->propbit, real_ts_dev->propbit, sizeof(v_input_dev->propbit));

//     // 克隆绝对坐标轴信息 (维度, 精度等)
//     v_input_dev->absinfo = kcalloc(ABS_CNT, sizeof(struct input_absinfo), GFP_KERNEL);
//     if (!v_input_dev->absinfo)
//     {
//         error = -ENOMEM;
//         goto err_free_dev;
//     }
//     for (i = 0; i < ABS_CNT; i++)
//     {
//         if (test_bit(i, real_ts_dev->absbit))
//         {
//             v_input_dev->absinfo[i] = real_ts_dev->absinfo[i];
//         }
//     }

//     // 注册设备并初始化状态机 
//     error = input_register_device(v_input_dev);
//     if (error)
//     {
//         pr_err("克隆触摸: 注册虚拟设备失败, 错误码: %d\n", error);
//         goto err_free_dev;
//     }

//     virtual_touch_init(); // 初始化我们的触摸状态机

//     pr_debug("克隆触摸: 虚拟触摸屏 '%s' 创建并注册成功！\n", v_input_dev->name);
//     return 0;

// err_free_dev:
//     kfree(v_input_dev->name);
//     kfree(v_input_dev->phys);
//     kfree(v_input_dev->uniq);
//     kfree(v_input_dev->absinfo);
//     input_free_device(v_input_dev);
//     v_input_dev = NULL;
//     input_put_device(real_ts_dev); // 释放物理设备引用
//     real_ts_dev = NULL;
//     return error;
// }

// // req->TOUCHSCREEN_MAX_X = v_input_dev->absinfo[ABS_MT_POSITION_X].maximum + 1;
// // req->TOUCHSCREEN_MAX_Y = v_input_dev->absinfo[ABS_MT_POSITION_Y].maximum + 1;
// //结束清理调用
// static inline void destroy_virtual_touch(void)
// {
//     if (v_input_dev)
//     {
//         input_unregister_device(v_input_dev);
//         v_input_dev = NULL;
//     }
//     if (real_ts_dev)
//     {
//         input_put_device(real_ts_dev);
//         real_ts_dev = NULL;
//     }
//     pr_debug("克隆触摸: 虚拟触摸屏已销毁。\n");
// }


// static inline void v_touch_down(int x, int y)
// {
//     int slot = -1, i;

//     if (!v_input_dev)
//         return;

//     for (i = 1; i < MAX_VIRTUAL_SLOTS; i++)
//     {
//         if (!v_slots[i].active)
//         {
//             slot = i;
//             break;
//         }
//     }
//     if (slot == -1)
//         return; // 没有可用插槽

//     v_slots[slot].active = true;
//     v_slots[slot].tracking_id = next_tracking_id++;
//     active_touch_count++;

//     // 切换到当前操作的插槽
//     input_event(v_input_dev, EV_ABS, ABS_MT_SLOT, slot);
//     input_event(v_input_dev, EV_ABS, ABS_MT_TRACKING_ID, v_slots[slot].tracking_id);

//     // 第一个手指按下时，才发送 BTN_TOUCH DOWN
//     if (active_touch_count == 1)
//     {
//         input_event(v_input_dev, EV_KEY, BTN_TOUCH, 1);
//         input_event(v_input_dev, EV_KEY, BTN_TOOL_FINGER, 1);
//     }

//     // 上报带有“人性化”随机噪声的物理属性
//     input_event(v_input_dev, EV_ABS, ABS_MT_WIDTH_MAJOR, 8 + (get_random_u32() % 5));
//     input_event(v_input_dev, EV_ABS, ABS_MT_PRESSURE, 8 + (get_random_u32() % 5));
//     input_event(v_input_dev, EV_ABS, ABS_MT_POSITION_X, x);
//     input_event(v_input_dev, EV_ABS, ABS_MT_POSITION_Y, y);
//     if (test_bit(ABS_PROFILE, v_input_dev->absbit))
//     {
//         input_event(v_input_dev, EV_ABS, ABS_PROFILE, 100 + (get_random_u32() % 500));
//     }

//     input_sync(v_input_dev);
//     slots = slot; // 保存到全局方便后续操作
// }

// static inline void v_touch_move(int x, int y)
// {
//     if (!v_input_dev || slots < 0 || slots >= MAX_VIRTUAL_SLOTS || !v_slots[slots].active)
//         return;

//     input_event(v_input_dev, EV_ABS, ABS_MT_SLOT, slots);
//     input_event(v_input_dev, EV_ABS, ABS_MT_POSITION_X, x);
//     input_event(v_input_dev, EV_ABS, ABS_MT_POSITION_Y, y);
//     input_event(v_input_dev, EV_ABS, ABS_MT_PRESSURE, 7 + (get_random_u32() % 6));

//     input_sync(v_input_dev);
// }


// static inline void v_touch_up(void)
// {
//     if (!v_input_dev || slots < 0 || slots >= MAX_VIRTUAL_SLOTS || !v_slots[slots].active)
//         return;

//     active_touch_count--;
//     v_slots[slots].active = false;

//     input_event(v_input_dev, EV_ABS, ABS_MT_SLOT, slots);
//     input_event(v_input_dev, EV_ABS, ABS_MT_TRACKING_ID, -1);

//     // 最后一个手指抬起时，才发送 BTN_TOUCH UP
//     if (active_touch_count == 0)
//     {
//         input_event(v_input_dev, EV_KEY, BTN_TOUCH, 0);
//         input_event(v_input_dev, EV_KEY, BTN_TOOL_FINGER, 0);
//     }

//     input_sync(v_input_dev);
// }

// #endif // VIRTUAL_INPUT_H