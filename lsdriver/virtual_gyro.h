#pragma once

#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/uaccess.h>
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

// 栈上切片探针：每次批量处理 8 个事件（832 字节），完全规避堆内存分配
#define VGYRO_CHUNK_EVENTS 8
#define VGYRO_CHUNK_BYTES  (VGYRO_CHUNK_EVENTS * VGYRO_ASENSOR_EVENT_SIZE)

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
    uint32_t fx_bits;
    uint32_t fy_bits;
    uint32_t fz_bits;
} vg = {
    .active = false,
    .gyro_x_mrad_s = 0,
    .gyro_y_mrad_s = 0,
    .gyro_z_mrad_s = 0,
    .fx_bits = 0,
    .fy_bits = 0,
    .fz_bits = 0,
};

static DEFINE_MUTEX(vgyro_lock);

// 将用户层传入的 mrad/s 转换为 IEEE754 float bit（仅在设置坐标时单次执行）
static uint32_t vgyro_milli_to_float_bits(int value)
{
    if (!value) return 0;

    uint32_t sign = (value < 0) ? 0x80000000U : 0;
    uint64_t mag = (value < 0) ? (uint64_t)(-(int64_t)value) : (uint64_t)value;

    uint64_t q = (mag << 24) / VGYRO_SCALE_MILLI;
    if (!q) return sign;

    int top = fls64(q) - 1;
    int exp = top - 24 + 127;
    if (exp <= 0) return sign;
    if (exp >= 255) return sign | 0x7f800000U;

    uint32_t mant;
    if (top > 23)
    {
        int shift = top - 23;
        uint64_t half = 1ULL << (shift - 1);
        uint64_t mask = (1ULL << shift) - 1;
        uint64_t rounded = q >> shift;

        if ((q & mask) >= half) rounded++;
        if (rounded >= (1ULL << 24))
        {
            rounded >>= 1;
            exp++;
            if (exp >= 255) return sign | 0x7f800000U;
        }
        mant = (uint32_t)rounded;
    }
    else
    {
        mant = (uint32_t)(q << (23 - top));
    }

    return sign | ((uint32_t)exp << 23) | (mant & 0x7fffffU);
}

// 高性能 IEEE754 单精度软浮点加法器
static uint32_t vgyro_float_bits_add(uint32_t a, uint32_t b)
{
    uint32_t abs_a = a & 0x7fffffffU, abs_b = b & 0x7fffffffU;
    if (!abs_a) return b;
    if (!abs_b) return a;
    if (abs_a >= 0x7f800000U) return a; // NaN / Inf
    if (abs_b >= 0x7f800000U) return b;

    uint32_t sign_a = a >> 31, sign_b = b >> 31;
    int exp_a = (a >> 23) & 0xff, exp_b = (b >> 23) & 0xff;
    uint32_t mant_a = (a & 0x7fffffU) | (exp_a ? 0x800000U : 0);
    uint32_t mant_b = (b & 0x7fffffU) | (exp_b ? 0x800000U : 0);

    if (exp_a < exp_b || (exp_a == exp_b && mant_a < mant_b))
    {
        swap(sign_a, sign_b);
        swap(exp_a, exp_b);
        swap(mant_a, mant_b);
    }

    int shift = exp_a - exp_b;
    mant_b = (shift >= 31) ? 0 : (mant_b >> shift);

    uint32_t res_mant;
    int res_exp = exp_a;
    uint32_t res_sign = sign_a;

    if (sign_a == sign_b)
    {
        res_mant = mant_a + mant_b;
        if (res_mant & 0x1000000U)
        {
            res_mant >>= 1;
            res_exp++;
        }
    }
    else
    {
        res_mant = mant_a - mant_b;
        if (!res_mant) return 0;
        int lz = __builtin_clz(res_mant) - 8; // 对齐至第23位隐式1
        if (lz >= res_exp) lz = res_exp - 1;
        res_mant <<= lz;
        res_exp -= lz;
    }

    if (res_exp >= 255) return (res_sign << 31) | 0x7f800000U;
    if (res_exp <= 0) return res_sign << 31;

    return (res_sign << 31) | ((uint32_t)res_exp << 23) | (res_mant & 0x7fffffU);
}

// 快速校验传感器事件类型
static inline bool vgyro_is_gyro_event_type(int type)
{
    return type == VGYRO_SENSOR_TYPE_GYROSCOPE || type == VGYRO_SENSOR_TYPE_GYROSCOPE_UNCALIBRATED;
}

// 处理 sendto 参数：栈切片处理 + 精准 12 字节原地写回
static int vgyro_handle_sendto(struct pt_regs *regs, enum vgyro_sendto_arg_mode mode)
{
    if (likely(!smp_load_acquire(&vg.active) || !regs)) return 0;

    char __user *ubuf;
    size_t len;

    if (mode == VGYRO_SENDTO_ARGS_ARM64_SYSCALL)
    {
        struct pt_regs *sys_regs = (struct pt_regs *)regs->regs[0];
        if (unlikely(!sys_regs)) return 0;
        ubuf = (char __user *)sys_regs->regs[1];
        len = (size_t)sys_regs->regs[2];
    }
    else
    {
        ubuf = (char __user *)regs->regs[1];
        len = (size_t)regs->regs[2];
    }

    // 极速快道：非 104 字节倍数或异常尺寸直接秒退
    if (len < VGYRO_ASENSOR_EVENT_SIZE || len > VGYRO_MAX_SCAN_BYTES || (len % VGYRO_ASENSOR_EVENT_SIZE) != 0 || !ubuf) return 0;

    uint32_t fx = READ_ONCE(vg.fx_bits);
    uint32_t fy = READ_ONCE(vg.fy_bits);
    uint32_t fz = READ_ONCE(vg.fz_bits);

    char chunk[VGYRO_CHUNK_BYTES];
    size_t processed = 0;
    int patched = 0;

    // 零堆内存分配（Zero Alloc）分块处理
    while (processed < len)
    {
        size_t cur_chunk_len = min_t(size_t, len - processed, VGYRO_CHUNK_BYTES);
        if (copy_from_user(chunk, ubuf + processed, cur_chunk_len)) break;

        for (size_t off = 0; off + VGYRO_ASENSOR_EVENT_SIZE <= cur_chunk_len; off += VGYRO_ASENSOR_EVENT_SIZE)
        {
            char *ev = chunk + off;
            int version = *(int *)(ev + VGYRO_ASENSOR_VERSION_OFFSET);
            int type = *(int *)(ev + VGYRO_ASENSOR_TYPE_OFFSET);

            if (version != VGYRO_ASENSOR_EVENT_SIZE || !vgyro_is_gyro_event_type(type)) continue;

            uint32_t *data = (uint32_t *)(ev + VGYRO_ASENSOR_DATA_OFFSET);
            uint32_t patch_data[3];

            patch_data[0] = vgyro_float_bits_add(data[0], fx);
            patch_data[1] = vgyro_float_bits_add(data[1], fy);
            patch_data[2] = vgyro_float_bits_add(data[2], fz);

            // 精准覆写 12 字节到用户空间，避免回写整个 Buffer
            if (!copy_to_user(ubuf + processed + off + VGYRO_ASENSOR_DATA_OFFSET, patch_data, sizeof(patch_data)))
            {
                patched++;
            }
        }
        processed += cur_chunk_len;
    }

    if (patched > 0)
    {
        ls_log_tag("vgyro", "sendto patched %d gyro event(s) len=%zu mrad=%d/%d/%d\n", patched, len, READ_ONCE(vg.gyro_x_mrad_s), READ_ONCE(vg.gyro_y_mrad_s), READ_ONCE(vg.gyro_z_mrad_s));
    }

    return 0;
}

static int vgyro_arm64_sys_sendto_hook(struct pt_regs *regs)
{
    return vgyro_handle_sendto(regs, VGYRO_SENDTO_ARGS_ARM64_SYSCALL);
}

static int vgyro_direct_sendto_hook(struct pt_regs *regs)
{
    return vgyro_handle_sendto(regs, VGYRO_SENDTO_ARGS_DIRECT);
}

static struct hook_entry vgyro_sendto_hook_targets[][1] = {
    {HOOK_ENTRY("__arm64_sys_sendto", vgyro_arm64_sys_sendto_hook)},
    {HOOK_ENTRY("__sys_sendto", vgyro_direct_sendto_hook)},
};

static bool vgyro_sendto_hook_installed(void)
{
    for (int i = 0; i < ARRAY_SIZE(vgyro_sendto_hook_targets); i++)
    {
        if (vgyro_sendto_hook_targets[i][0].installed) return true;
    }
    return false;
}

static int vgyro_install_hook_locked(void)
{
    int ret = -ENOENT;
    for (int i = 0; i < ARRAY_SIZE(vgyro_sendto_hook_targets); i++)
    {
        ret = inline_hook_install(vgyro_sendto_hook_targets[i]);
        if (!ret)
        {
            ls_log_tag("vgyro", "inline hook on %s registered\n", vgyro_sendto_hook_targets[i][0].target_sym);
            return 0;
        }
    }
    return ret;
}

// 初始化虚拟陀螺仪状态
static inline int v_gyro_init(void)
{
    mutex_lock(&vgyro_lock);

    WRITE_ONCE(vg.gyro_x_mrad_s, 0);
    WRITE_ONCE(vg.gyro_y_mrad_s, 0);
    WRITE_ONCE(vg.gyro_z_mrad_s, 0);
    WRITE_ONCE(vg.fx_bits, 0);
    WRITE_ONCE(vg.fy_bits, 0);
    WRITE_ONCE(vg.fz_bits, 0);
    smp_store_release(&vg.active, true);

    int ret = vgyro_install_hook_locked();
    mutex_unlock(&vgyro_lock);

    ls_log_tag("vgyro", "init sendto_inline_hook=%d active=1\n", ret);
    return ret;
}

// 更新虚拟陀螺仪三轴偏移值（单位: rad/s * 1000）
static inline int v_gyro_report(int gyro_x_mrad_s, int gyro_y_mrad_s, int gyro_z_mrad_s)
{
    // 提前计算浮点 Bits，移出热路径
    uint32_t fx = vgyro_milli_to_float_bits(gyro_x_mrad_s);
    uint32_t fy = vgyro_milli_to_float_bits(gyro_y_mrad_s);
    uint32_t fz = vgyro_milli_to_float_bits(gyro_z_mrad_s);

    WRITE_ONCE(vg.gyro_x_mrad_s, gyro_x_mrad_s);
    WRITE_ONCE(vg.gyro_y_mrad_s, gyro_y_mrad_s);
    WRITE_ONCE(vg.gyro_z_mrad_s, gyro_z_mrad_s);
    WRITE_ONCE(vg.fx_bits, fx);
    WRITE_ONCE(vg.fy_bits, fy);
    WRITE_ONCE(vg.fz_bits, fz);
    smp_store_release(&vg.active, true);

    ls_log_tag("vgyro", "report mrad=%d/%d/%d hook=%d\n", gyro_x_mrad_s, gyro_y_mrad_s, gyro_z_mrad_s, vgyro_sendto_hook_installed());
    return 0;
}

// 停用虚拟陀螺仪并卸载 hook
static inline void v_gyro_destroy(void)
{
    mutex_lock(&vgyro_lock);

    smp_store_release(&vg.active, false);
    WRITE_ONCE(vg.gyro_x_mrad_s, 0);
    WRITE_ONCE(vg.gyro_y_mrad_s, 0);
    WRITE_ONCE(vg.gyro_z_mrad_s, 0);
    WRITE_ONCE(vg.fx_bits, 0);
    WRITE_ONCE(vg.fy_bits, 0);
    WRITE_ONCE(vg.fz_bits, 0);

    for (int i = 0; i < ARRAY_SIZE(vgyro_sendto_hook_targets); i++)
    {
        inline_hook_remove(vgyro_sendto_hook_targets[i]);
    }

    ls_log_tag("vgyro", "destroy\n");
    mutex_unlock(&vgyro_lock);
}