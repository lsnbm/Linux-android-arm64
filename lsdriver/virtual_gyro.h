#pragma once

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
#include "inline_hook_frame.h"
#include "lsdriver_log.h"

/*
  Android sensors_event_t / ASensorEvent:
    version(0), sensor(4), type(8), reserved0(12), timestamp(16), data[0](24)
  Gyroscope is SENSOR_TYPE_GYROSCOPE == 4. Values are rad/s.

    用户层 ABI: gyro_x_mrad_s/gyro_y_mrad_s/gyro_z_mrad_s = rad/s * 1000
  限制不建议使用float主要来自 Linux 内核对 FPU/NEON/SIMD 上下文的管理规则，
  不是 C 语言本身限制，也不是编译器语法限制。
  内核为了性能不会像用户态那样在每次内核进入/退出、抢占、中断时都自动保存和恢复浮点寄存器。
  ARM64 上浮点/NEON 寄存器属于任务上下文的一部分，
  随便在内核态用 float 计算，可能破坏当前进程的用户态浮点寄存器状态，
  用户态可以直接用浮点和 NEON，是因为内核把用户进程的 FP/SIMD 状态当作进程上下文来隔离和管理；
  内核态普通代码不能随便用，是因为内核默认不为自己每段代码自动保存/恢复 FP/SIMD 状态。

  实现方式:
    1. inline_hook_install() 挂钩 __arm64_sys_sendto 或 __sys_sendto
        2. AOSP BitTube 使用 send() 写入本地 socket；arm64 Linux 上 send() 通常进入 sendto 路径，因此在 sendto hook 中拦截用户缓冲区
    3. 在 104 字节 ASensorEvent 中找到 gyro/gyro_uncalibrated
    4. 修改 data[0]/data[1]/data[2] 后 copy_to_user 写回

在 Android 系统中，传感器数据的传输路径如下：
    Sensor HAL（硬件抽象层） 从硬件获取到陀螺仪等数据。
    system_server 进程中的 SensorService 负责统一管理这些数据。
        App 通过 Binder 调用 SensorService 创建 SensorEventConnection、enable/disable 传感器、设置采样率和 flush。
        SensorEventConnection 创建 BitTube，并通过 Binder reply 把 BitTube 的接收端 fd 传给 App。
        高频 ASensorEvent/sensors_event_t 事件本身不会逐条通过 Binder 传输；SensorService 调用 SensorEventQueue::write() 写入 BitTube。
        BitTube 底层使用 socketpair(AF_UNIX, SOCK_SEQPACKET) 创建一对 Unix Domain Socket，并通过 send()/recv() 传递事件数据。
        UDS 不经过网卡，也不走 TCP/IP 网络协议栈；数据通过内核 socket 缓冲区在本地进程之间传递，适合高频、小数据量事件分发。
*/
#define VGYRO_SENSOR_TYPE_GYROSCOPE              4
#define VGYRO_SENSOR_TYPE_GYROSCOPE_UNCALIBRATED 16
#define VGYRO_ASENSOR_EVENT_SIZE                 104
#define VGYRO_ASENSOR_VERSION_OFFSET             0
#define VGYRO_ASENSOR_TYPE_OFFSET                8
#define VGYRO_ASENSOR_DATA_OFFSET                24
#define VGYRO_SCALE_MILLI                        1000
#define VGYRO_MAX_SCAN_BYTES                     (2 * 1024 * 1024)

enum vgyro_sendto_arg_mode
{
    VGYRO_SENDTO_ARGS_ARM64_SYSCALL = 0,
    VGYRO_SENDTO_ARGS_DIRECT = 1,
};

static struct
{
    bool active;
    int gyro_x_mrad_s;
    int gyro_y_mrad_s;
    int gyro_z_mrad_s;
} vg = {
    .active = false,
    .gyro_x_mrad_s = 0,
    .gyro_y_mrad_s = 0,
    .gyro_z_mrad_s = 0,
};

static DEFINE_MUTEX(vgyro_lock);

// 将用户层传入的 mrad/s 整数转换为 IEEE754 float bit。
static u32 vgyro_milli_to_float_bits(int value)
{
    u32 sign = 0;
    u64 mag;
    u64 q;
    int top;
    int exp;
    u32 mant;

    if (!value) return 0;

    if (value < 0)
    {
        sign = 0x80000000U;
        mag = (u64)(-(s64)value);
    }
    else
    {
        mag = (u64)value;
    }

    q = (mag << 24) / VGYRO_SCALE_MILLI;
    if (!q) return sign;

    top = fls64(q) - 1;
    exp = top - 24 + 127;
    if (exp <= 0) return sign;
    if (exp >= 255) return sign | 0x7f800000U;

    if (top > 23)
    {
        int shift = top - 23;
        u64 half = 1ULL << (shift - 1);
        u64 mask = (1ULL << shift) - 1;
        u64 rounded = q >> shift;

        if ((q & mask) >= half) rounded++;
        if (rounded >= (1ULL << 24))
        {
            rounded >>= 1;
            exp++;
            if (exp >= 255) return sign | 0x7f800000U;
        }
        mant = (u32)rounded;
    }
    else
    {
        mant = (u32)(q << (23 - top));
    }

    return sign | ((u32)exp << 23) | (mant & 0x7fffffU);
}

// 在不使用内核浮点的情况下，对两个 IEEE754 float bit 做加法。
static u32 vgyro_float_bits_add(u32 a, u32 b)
{
    u32 abs_a = a & 0x7fffffffU;
    u32 abs_b = b & 0x7fffffffU;
    u32 sign_a, sign_b, sign_r;
    int exp_a, exp_b, exp_r;
    u64 mant_a, mant_b, mant_r;
    int diff;

    if (!abs_a) return b;
    if (!abs_b) return a;
    if ((abs_a & 0x7f800000U) == 0x7f800000U) return a;
    if ((abs_b & 0x7f800000U) == 0x7f800000U) return b;

    sign_a = a >> 31;
    sign_b = b >> 31;
    exp_a = (a >> 23) & 0xff;
    exp_b = (b >> 23) & 0xff;
    mant_a = a & 0x7fffffU;
    mant_b = b & 0x7fffffU;

    if (exp_a) mant_a |= 0x800000U;
    else exp_a = 1;
    if (exp_b) mant_b |= 0x800000U;
    else exp_b = 1;

    mant_a <<= 8;
    mant_b <<= 8;

    if (exp_b > exp_a)
    {
        u32 tmp_sign = sign_a;
        int tmp_exp = exp_a;
        u64 tmp_mant = mant_a;

        sign_a = sign_b;
        sign_b = tmp_sign;
        exp_a = exp_b;
        exp_b = tmp_exp;
        mant_a = mant_b;
        mant_b = tmp_mant;
    }

    diff = exp_a - exp_b;
    if (diff >= 56) mant_b = 0;
    else mant_b >>= diff;

    exp_r = exp_a;
    if (sign_a == sign_b)
    {
        sign_r = sign_a;
        mant_r = mant_a + mant_b;
        if (mant_r & (0x1000000ULL << 8))
        {
            mant_r >>= 1;
            exp_r++;
        }
    }
    else
    {
        if (mant_a >= mant_b)
        {
            sign_r = sign_a;
            mant_r = mant_a - mant_b;
        }
        else
        {
            sign_r = sign_b;
            mant_r = mant_b - mant_a;
        }

        if (!mant_r) return 0;

        if (mant_r < (0x800000ULL << 8))
        {
            int shift = 31 - (fls64(mant_r) - 1);

            if (shift >= exp_r) shift = exp_r - 1;
            mant_r <<= shift;
            exp_r -= shift;
        }
    }

    if (mant_r & 0x80)
    {
        mant_r += 0x100;
        if (mant_r & (0x1000000ULL << 8))
        {
            mant_r >>= 1;
            exp_r++;
        }
    }

    if (exp_r >= 255) return (sign_r << 31) | 0x7f800000U;
    if (exp_r <= 0) return sign_r << 31;

    return (sign_r << 31) | ((u32)exp_r << 23) | (((u32)(mant_r >> 8)) & 0x7fffffU);
}

// 判断 sendto 缓冲区长度是否可能是一组 ASensorEvent。
static bool vgyro_buffer_maybe_events(size_t len)
{
    if (len < VGYRO_ASENSOR_EVENT_SIZE) return false;
    if (len > VGYRO_MAX_SCAN_BYTES) return false;
    if (len % VGYRO_ASENSOR_EVENT_SIZE) return false;
    return true;
}

// 判断传感器事件类型是否为陀螺仪或未校准陀螺仪。
static bool vgyro_is_gyro_event_type(int type)
{
    return type == VGYRO_SENSOR_TYPE_GYROSCOPE || type == VGYRO_SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
}

// 扫描内核临时缓冲区并给陀螺仪事件叠加虚拟三轴值。
static int vgyro_patch_kernel_events(char *buf, size_t len)
{
    size_t off;
    int patched = 0;
    u32 fx = vgyro_milli_to_float_bits(READ_ONCE(vg.gyro_x_mrad_s));
    u32 fy = vgyro_milli_to_float_bits(READ_ONCE(vg.gyro_y_mrad_s));
    u32 fz = vgyro_milli_to_float_bits(READ_ONCE(vg.gyro_z_mrad_s));

    if (!buf || !vgyro_buffer_maybe_events(len)) return 0;

    for (off = 0; off + VGYRO_ASENSOR_EVENT_SIZE <= len; off += VGYRO_ASENSOR_EVENT_SIZE)
    {
        int version = *(int *)(buf + off + VGYRO_ASENSOR_VERSION_OFFSET);
        int type = *(int *)(buf + off + VGYRO_ASENSOR_TYPE_OFFSET);
        u32 *data = (u32 *)(buf + off + VGYRO_ASENSOR_DATA_OFFSET);

        if (version != VGYRO_ASENSOR_EVENT_SIZE) continue;
        if (!vgyro_is_gyro_event_type(type)) continue;

        data[0] = vgyro_float_bits_add(data[0], fx);
        data[1] = vgyro_float_bits_add(data[1], fy);
        data[2] = vgyro_float_bits_add(data[2], fz);
        patched++;
    }

    return patched;
}

// 处理被 inline hook 拦截到的 sendto 参数并回写修改后的用户缓冲区。
static int vgyro_handle_sendto(struct pt_regs *regs, enum vgyro_sendto_arg_mode mode)
{
    struct pt_regs *sys_regs = NULL;
    char __user *ubuf;
    size_t len;
    char *kbuf;
    int patched;

    if (!READ_ONCE(vg.active)) return 0;
    if (!regs) return 0;

    if (mode == VGYRO_SENDTO_ARGS_ARM64_SYSCALL)
    {
        sys_regs = (struct pt_regs *)regs->regs[0];
        if (!sys_regs) return 0;

        ubuf = (char __user *)sys_regs->regs[1];
        len = (size_t)sys_regs->regs[2];
    }
    else
    {
        ubuf = (char __user *)regs->regs[1];
        len = (size_t)regs->regs[2];
    }

    if (!ubuf || !vgyro_buffer_maybe_events(len)) return 0;

    kbuf = vmalloc(len);
    if (!kbuf) return 0;

    if (copy_from_user(kbuf, ubuf, len)) goto out;

    patched = vgyro_patch_kernel_events(kbuf, len);
    if (patched > 0)
    {
        unsigned long missing = copy_to_user(ubuf, kbuf, len);

        if (missing) ls_log_tag("vgyro", "sendto copy back missing=%lu len=%zu\n", missing, len);
        else ls_log_tag("vgyro", "sendto patched %d gyro event(s) len=%zu mode=%d mrad=%d/%d/%d\n", patched, len, mode, READ_ONCE(vg.gyro_x_mrad_s), READ_ONCE(vg.gyro_y_mrad_s), READ_ONCE(vg.gyro_z_mrad_s));
    }

out:
    vfree(kbuf);
    return 0;
}

// 适配 __arm64_sys_sendto 入口，x0 是用户 syscall pt_regs 指针。
static int vgyro_arm64_sys_sendto_hook(struct pt_regs *regs)
{
    return vgyro_handle_sendto(regs, VGYRO_SENDTO_ARGS_ARM64_SYSCALL);
}

// 适配 __sys_sendto 入口，sendto 参数直接位于当前寄存器快照中。
static int vgyro_direct_sendto_hook(struct pt_regs *regs)
{
    return vgyro_handle_sendto(regs, VGYRO_SENDTO_ARGS_DIRECT);
}

static struct hook_entry vgyro_sendto_hook_targets[][1] = {
    {HOOK_ENTRY("__arm64_sys_sendto", vgyro_arm64_sys_sendto_hook)},
    {HOOK_ENTRY("__sys_sendto", vgyro_direct_sendto_hook)},
};

// 判断任意 sendto inline hook 是否已经安装。
static bool vgyro_sendto_hook_installed(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(vgyro_sendto_hook_targets); i++)
    {
        if (vgyro_sendto_hook_targets[i][0].installed) return true;
    }

    return false;
}

// 安装 sendto inline hook，优先挂 syscall wrapper，失败后回退到内部实现。
static int vgyro_install_hook_locked(void)
{
    int i;
    int ret;

    for (i = 0; i < ARRAY_SIZE(vgyro_sendto_hook_targets); i++)
    {
        ret = inline_hook_install(vgyro_sendto_hook_targets[i]);
        if (!ret)
        {
            ls_log_tag("vgyro", "inline hook on %s registered\n", vgyro_sendto_hook_targets[i][0].target_sym);
            return 0;
        }

        ls_log_tag("vgyro", "inline hook on %s failed: %d\n", vgyro_sendto_hook_targets[i][0].target_sym, ret);
    }

    return ret;
}

// 初始化虚拟陀螺仪状态并确保 sendto hook 已安装。
static inline int v_gyro_init(void)
{
    int ret;

    mutex_lock(&vgyro_lock);

    WRITE_ONCE(vg.gyro_x_mrad_s, 0);
    WRITE_ONCE(vg.gyro_y_mrad_s, 0);
    WRITE_ONCE(vg.gyro_z_mrad_s, 0);
    WRITE_ONCE(vg.active, true);
    ret = vgyro_install_hook_locked();

    mutex_unlock(&vgyro_lock);

    ls_log_tag("vgyro", "init sendto_inline_hook=%d active=1\n", ret);
    return ret;
}

// 更新虚拟陀螺仪三轴偏移值，单位为 rad/s * 1000。
static inline int v_gyro_report(int gyro_x_mrad_s, int gyro_y_mrad_s, int gyro_z_mrad_s)
{
    WRITE_ONCE(vg.gyro_x_mrad_s, gyro_x_mrad_s);
    WRITE_ONCE(vg.gyro_y_mrad_s, gyro_y_mrad_s);
    WRITE_ONCE(vg.gyro_z_mrad_s, gyro_z_mrad_s);
    WRITE_ONCE(vg.active, true);

    ls_log_tag("vgyro", "report mrad=%d/%d/%d hook=%d\n", gyro_x_mrad_s, gyro_y_mrad_s, gyro_z_mrad_s, vgyro_sendto_hook_installed());
    return 0;
}

// 停用虚拟陀螺仪并卸载 sendto inline hook。
static inline void v_gyro_destroy(void)
{
    int i;

    mutex_lock(&vgyro_lock);

    WRITE_ONCE(vg.active, false);
    WRITE_ONCE(vg.gyro_x_mrad_s, 0);
    WRITE_ONCE(vg.gyro_y_mrad_s, 0);
    WRITE_ONCE(vg.gyro_z_mrad_s, 0);

    for (i = 0; i < ARRAY_SIZE(vgyro_sendto_hook_targets); i++) inline_hook_remove(vgyro_sendto_hook_targets[i]);

    ls_log_tag("vgyro", "inline hook unregistered\n");

    ls_log_tag("vgyro", "destroy\n");

    mutex_unlock(&vgyro_lock);
}
