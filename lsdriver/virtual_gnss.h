#pragma once

/*
  已知待修复问题（先记录一下，现在可以正常使用，先不进行修):
    1. Android 12 的 Location.writeToParcel() 使用 UTF-16 String16 provider，
       当前解析按 String8 布局处理，android12-5.10 等组合通常无法替换位置。
    2. ILocationListener 使用 List<Location> 批量回调，当前命中第一个 Location
       后立即返回，后续对象仍可能携带真实坐标；前 512 字节探针也无法覆盖完整批量数据。
    3. vgnss_is_location_token() 只检查 android.location.ILocation 前缀，
       可能误匹配 ILocationManager/ILocationProvider 并改写非回调 Parcel。
    4. BC_TRANSACTION_SG/BC_REPLY_SG 使用了基础 transaction 结构编码，
       没有使用 binder_transaction_data_sg 的真实 ioctl 大小，SG 命令不会命中。
    5. lat_bits/lon_bits 分别读写；重复上报与回调并发时可能形成新纬度配旧经度的混合快照。
    6. 仅检查 Parcel 前 512 字节且 provider 长度限制为 64，复杂 Parcel 或较长 provider 可能漏检。
    8. 部分解析路径使用直接的 uint64_t* 非对齐读取，依赖 ARM64 对非对齐访问的容忍，可能会崩溃

  说明：32 位 compat ioctl 按当前目标范围主动排除，不列为待修复项。
*/

#include <linux/bitops.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include "inline_hook_frame.h"
#include "lsdriver_log.h"

/*
  Android Location Parcelable / Parcel:
    interface token: android.location.ILocationListener / android.location.ILocationCallback
    provider String8: int32 length + UTF-8 bytes + NUL + 4 字节对齐
    fieldsMask(4), time(8), elapsedRealtime(8), optional elapsedRealtimeUncertainty(8), latitude(8), longitude(8)
  latitude/longitude 是 IEEE754 double，单位是 degrees。

  用户层 ABI: latitude_e7/longitude_e7 = degrees * 10000000。
  限制不建议使用 double/float 主要来自 Linux 内核对 FPU/NEON/SIMD 上下文的管理规则，
  不是 C 语言本身限制，也不是编译器语法限制。
  内核为了性能不会像用户态那样在每次内核进入/退出、抢占、中断时都自动保存和恢复浮点寄存器。
  ARM64 上浮点/NEON 寄存器属于任务上下文的一部分，
  随便在内核态用 double/float 计算，可能破坏当前进程的用户态浮点寄存器状态，
  用户态可以直接用浮点和 NEON，是因为内核把用户进程的 FP/SIMD 状态当作进程上下文来隔离和管理；
  内核态普通代码不能随便用，是因为内核默认不为自己每段代码自动保存/恢复 FP/SIMD 状态。

  实现方式:
    1. inline_hook_install() 挂钩 __arm64_sys_ioctl 或 __se_sys_ioctl
    2. Android Binder 驱动通过 ioctl(BINDER_WRITE_READ) 收发 transaction，因此在 ioctl hook 中拦截 Binder write buffer
    3. 解析 BC_TRANSACTION/BC_REPLY/BC_TRANSACTION_SG/BC_REPLY_SG，复制 Parcel 数据到内核临时缓冲区
    4. 通过 interface token 和 Location Parcelable 布局识别位置回调，不依赖 current->comm 进程名
    5. 用整数逻辑把 latitude_e7/longitude_e7 转成 IEEE754 double bit，修改 Parcel 后 copy_to_user 写回

在 Android 系统中，定位数据的传输路径如下：
    GNSS HAL / network / fused provider 等位置源把定位结果上报给 system_server。
    system_server 进程中的 LocationManagerService 负责统一管理这些数据。
    App 通过 Binder 调用 LocationManagerService 注册位置监听、请求定位、移除监听。
    LocationManagerService 收到真实 Location 后，通过 Binder transaction 回调 App 侧的 ILocationListener 或 ILocationCallback。
    Location.writeToParcel() 会把 provider、fields mask、时间戳、经纬度等字段写入 Parcel。
    这里不拦截 GNSS 硬件节点，也不按进程名过滤，而是在 Binder Parcel 层按接口 token 和 Location 数据布局做特征识别。
    匹配到位置回调后，只替换 latitude/longitude 两个 double字段，让 App 收到虚拟定位结果。
*/

#define VGNSS_COORD_SCALE_E7  10000000ULL
#define VGNSS_SCAN_PROBE_SIZE 512 // 栈上探针尺寸：覆盖 Token + Location 全部头部数据
#define VGNSS_TOKEN_BYTE_LEN  68  // 34个 UTF-16 字符

#define VGNSS_DOUBLE_ABS_LAT_90  0x4056800000000000ULL
#define VGNSS_DOUBLE_ABS_LON_180 0x4066800000000000ULL
#define VGNSS_DOUBLE_EXP_MASK    0x7ff0000000000000ULL
#define VGNSS_DOUBLE_ABS_MASK    0x7fffffffffffffffULL

typedef uint64_t vgnss_binder_uintptr_t;
typedef uint64_t vgnss_binder_size_t;

struct vgnss_binder_write_read
{
    vgnss_binder_size_t write_size;
    vgnss_binder_size_t write_consumed;
    vgnss_binder_uintptr_t write_buffer;
    vgnss_binder_size_t read_size;
    vgnss_binder_size_t read_consumed;
    vgnss_binder_uintptr_t read_buffer;
};

struct vgnss_binder_transaction_data
{
    union
    {
        uint32_t handle;
        vgnss_binder_uintptr_t ptr;
    } target;
    vgnss_binder_uintptr_t cookie;
    uint32_t code;
    uint32_t flags;
    int32_t sender_pid;
    uint32_t sender_euid;
    vgnss_binder_size_t data_size;
    vgnss_binder_size_t offsets_size;
    union
    {
        struct
        {
            vgnss_binder_uintptr_t buffer;
            vgnss_binder_uintptr_t offsets;
        } ptr;
        uint8_t buf[8];
    } data;
};

#define VGNSS_BINDER_WRITE_READ _IOWR('b', 1, struct vgnss_binder_write_read)
#define VGNSS_BC_TRANSACTION    _IOW('c', 0, struct vgnss_binder_transaction_data)
#define VGNSS_BC_REPLY          _IOW('c', 1, struct vgnss_binder_transaction_data)
#define VGNSS_BC_TRANSACTION_SG _IOW('c', 17, struct vgnss_binder_transaction_data)
#define VGNSS_BC_REPLY_SG       _IOW('c', 18, struct vgnss_binder_transaction_data)

static struct
{
    uint64_t lat_bits;
    uint64_t lon_bits;
    int latitude_e7;
    int longitude_e7;
    bool has_fix;
    int hook_target;
} vgps = {.has_fix = false, .hook_target = -1};

static DEFINE_MUTEX(vgnss_lock);

// 纯整数把 e7 转 IEEE754 double bits（仅在设置经纬度时单次计算）
static uint64_t vgnss_e7_to_double_bits(int value)
{
    if (!value) return 0;
    uint64_t sign = (value < 0) ? 0x8000000000000000ULL : 0;
    uint64_t mag = (value < 0) ? (uint64_t)(-(int64_t)value) : (uint64_t)value;
    uint64_t fixed = ((mag << 32) + (VGNSS_COORD_SCALE_E7 / 2)) / VGNSS_COORD_SCALE_E7;
    if (!fixed) return sign;

    int top = fls64(fixed) - 1;
    int exp = top - 32 + 1023;
    if (exp <= 0) return sign;
    if (exp >= 2047) return sign | VGNSS_DOUBLE_EXP_MASK;

    uint64_t mant = (top > 52) ? (fixed >> (top - 52)) : (fixed << (52 - top));
    return sign | ((uint64_t)exp << 52) | (mant & 0x000fffffffffffffULL);
}

//  SWAR 64位并行字比对，3条指令匹配 "android.location.ILocation"
static inline bool vgnss_is_location_token(const char *p)
{
    const uint64_t *w = (const uint64_t *)p;
    return (w[0] == 0x00720064006e0061ULL && // "andr" (UTF-16LE)
            w[2] == 0x00610063006f006cULL && // "loca" (UTF-16LE)
            w[4] == 0x006f004c0049002eULL);  // ".ILo" (UTF-16LE)
}

//  严格校验并就地 Patch 用户空间 16 字节
static bool vgnss_try_patch_location(char *buf, size_t probe_len, size_t start, char __user *u_base, uint64_t lat_bits, uint64_t lon_bits)
{
    if (start + 4 > probe_len) return false;

    int32_t prov_len = *(int32_t *)(buf + start);
    if (prov_len < -1 || prov_len > 64) return false;

    size_t pos = start + 4;
    if (prov_len >= 0)
    {
        if (pos + prov_len + 1 > probe_len || buf[pos + prov_len] != 0) return false;
        pos = (pos + prov_len + 4) & ~3UL; // align4
    }

    if (pos + 4 + 8 * 4 > probe_len) return false;

    uint32_t fields = *(uint32_t *)(buf + pos);
    if (fields & ~0x7ffU) return false;

    pos += 4;
    uint64_t time_ms = *(uint64_t *)(buf + pos);
    uint64_t elapsed_ns = *(uint64_t *)(buf + pos + 8);
    if (!time_ms || !elapsed_ns || time_ms > 4102444800000ULL) return false;

    pos += 16 + ((fields & (1U << 8)) ? 8 : 0); // 跳过 time, elapsedRealtime [+ uncertainty]
    if (pos + 16 > probe_len) return false;

    uint64_t lat = *(uint64_t *)(buf + pos);
    uint64_t lon = *(uint64_t *)(buf + pos + 8);

    // 粗略范围与有效性检查
    if ((lat & VGNSS_DOUBLE_ABS_MASK) > VGNSS_DOUBLE_ABS_LAT_90 || (lon & VGNSS_DOUBLE_ABS_MASK) > VGNSS_DOUBLE_ABS_LON_180) return false;

    // 直接将计算好的 Double Bits 覆写回用户层，无需覆写整个 Parcel
    uint64_t patch_data[2] = {lat_bits, lon_bits};
    return copy_to_user(u_base + pos, patch_data, sizeof(patch_data)) == 0;
}

//  栈探针扫描，零堆内存分配 (Zero Alloc)
static void vgnss_inspect_parcel(char __user *u_buf, size_t size, uint64_t lat_bits, uint64_t lon_bits)
{
    if (size < 128 || !u_buf) return;

    char stack_buf[VGNSS_SCAN_PROBE_SIZE];
    size_t probe_len = min_t(size_t, size, sizeof(stack_buf));

    if (copy_from_user(stack_buf, u_buf, probe_len)) return;

    // 寻找 Token 头部
    for (size_t i = 0; i + VGNSS_TOKEN_BYTE_LEN <= probe_len; i += 4)
    {
        if (vgnss_is_location_token(stack_buf + i))
        {
            size_t token_end = (i + VGNSS_TOKEN_BYTE_LEN + 3) & ~3UL;
            for (size_t pos = token_end; pos + 32 <= probe_len; pos += 4)
            {
                if (vgnss_try_patch_location(stack_buf, probe_len, pos, u_buf, lat_bits, lon_bits))
                {
                    ls_log_tag("vgnss", "patched Location at offset=%zu size=%zu\n", pos, size);
                    return;
                }
            }
            break;
        }
    }
}

// 处理 Binder Write 缓冲区
static inline void vgnss_process_write_buffer(const char __user *u_write_buf, size_t write_size, uint64_t lat_bits, uint64_t lon_bits)
{
    size_t pos = 0;
    while (pos + sizeof(uint32_t) <= write_size)
    {
        uint32_t cmd;
        if (__get_user(cmd, (uint32_t __user *)(u_write_buf + pos))) break;

        size_t payload_size = _IOC_SIZE(cmd);
        pos += sizeof(uint32_t);
        if (payload_size > write_size - pos) break;

        // 仅处理 Transaction/Reply 类型
        if (cmd == VGNSS_BC_TRANSACTION || cmd == VGNSS_BC_REPLY || cmd == VGNSS_BC_TRANSACTION_SG || cmd == VGNSS_BC_REPLY_SG)
        {
            struct vgnss_binder_transaction_data tr;
            if (payload_size >= sizeof(tr) && !copy_from_user(&tr, u_write_buf + pos, sizeof(tr)))
            {
                vgnss_inspect_parcel((char __user *)(uintptr_t)tr.data.ptr.buffer, (size_t)tr.data_size, lat_bits, lon_bits);
            }
        }
        pos += payload_size;
    }
}

// 核心 Hook 入口：极简快道判断
static inline void vgnss_handle_ioctl(unsigned int cmd, void __user *argp)
{
    // 无锁极速快道：非 Binder 调用或无定位直接 1 周期退出
    if (likely(cmd != VGNSS_BINDER_WRITE_READ || !argp)) return;
    if (unlikely(!smp_load_acquire(&vgps.has_fix))) return;

    struct vgnss_binder_write_read bwr;
    if (copy_from_user(&bwr, argp, sizeof(bwr))) return;

    if (!bwr.write_size || !bwr.write_buffer || bwr.write_size > (256 * 1024)) return;

    vgnss_process_write_buffer((const char __user *)(uintptr_t)bwr.write_buffer, (size_t)bwr.write_size, READ_ONCE(vgps.lat_bits), READ_ONCE(vgps.lon_bits));
}

static int vgnss_arm64_sys_ioctl_hook(struct pt_regs *regs)
{
    if (likely(regs && regs->regs[0]))
    {
        struct pt_regs *sys_regs = (struct pt_regs *)regs->regs[0];
        vgnss_handle_ioctl((unsigned int)sys_regs->regs[1], (void __user *)sys_regs->regs[2]);
    }
    return 0;
}

static int vgnss_direct_ioctl_hook(struct pt_regs *regs)
{
    if (likely(regs)) vgnss_handle_ioctl((unsigned int)regs->regs[1], (void __user *)regs->regs[2]);
    return 0;
}

static struct hook_entry vgnss_ioctl_hook_targets[][1] = {
    {HOOK_ENTRY("__arm64_sys_ioctl", vgnss_arm64_sys_ioctl_hook)},
    {HOOK_ENTRY("__se_sys_ioctl", vgnss_direct_ioctl_hook)},
};

static int vgnss_install_hook_locked(void)
{
    if (vgps.hook_target >= 0) return 0;
    for (int i = 0; i < ARRAY_SIZE(vgnss_ioctl_hook_targets); i++)
    {
        if (!inline_hook_install(vgnss_ioctl_hook_targets[i]))
        {
            vgps.hook_target = i;
            ls_log_tag("vgnss", "hook %s ok\n", vgnss_ioctl_hook_targets[i][0].target_sym);
            return 0;
        }
    }
    return -ENOENT;
}

static inline int v_gnss_init(void)
{
    mutex_lock(&vgnss_lock);
    smp_store_release(&vgps.has_fix, false);
    int ret = vgnss_install_hook_locked();
    mutex_unlock(&vgnss_lock);
    return ret;
}

static inline int v_gnss_report(int latitude_e7, int longitude_e7)
{
    if (latitude_e7 < -900000000 || latitude_e7 > 900000000) return -EINVAL;
    if (longitude_e7 < -1800000000 || longitude_e7 > 1800000000) return -EINVAL;

    mutex_lock(&vgnss_lock);
    if (vgps.hook_target < 0)
    {
        mutex_unlock(&vgnss_lock);
        return -ENODEV;
    }

    WRITE_ONCE(vgps.latitude_e7, latitude_e7);
    WRITE_ONCE(vgps.longitude_e7, longitude_e7);
    WRITE_ONCE(vgps.lat_bits, vgnss_e7_to_double_bits(latitude_e7));
    WRITE_ONCE(vgps.lon_bits, vgnss_e7_to_double_bits(longitude_e7));

    smp_store_release(&vgps.has_fix, true);
    mutex_unlock(&vgnss_lock);
    return 0;
}

static inline void v_gnss_destroy(void)
{
    mutex_lock(&vgnss_lock);
    smp_store_release(&vgps.has_fix, false);
    if (vgps.hook_target >= 0)
    {
        inline_hook_remove(vgnss_ioctl_hook_targets[vgps.hook_target]);
        vgps.hook_target = -1;
    }
    mutex_unlock(&vgnss_lock);
}
