

#pragma once

#include <linux/bitops.h>
#include <linux/ioctl.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <linux/vmalloc.h>
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
        匹配到位置回调后，只替换 latitude/longitude 两个 double 字段，让 App 收到虚拟定位结果。
*/
#define VGNSS_COORD_SCALE_E7         10000000ULL
#define VGNSS_MAX_BINDER_WRITE_BYTES (256 * 1024)
#define VGNSS_MAX_PARCEL_BYTES       (256 * 1024)
#define VGNSS_MAX_PROVIDER_BYTES     64

#define VGNSS_LOCATION_HAS_ELAPSED_REALTIME_UNCERTAINTY_MASK (1U << 8)
#define VGNSS_LOCATION_KNOWN_FIELD_MASK                      0x7ffU

#define VGNSS_DOUBLE_ABS_LAT_90  0x4056800000000000ULL
#define VGNSS_DOUBLE_ABS_LON_180 0x4066800000000000ULL
#define VGNSS_DOUBLE_EXP_MASK    0x7ff0000000000000ULL
#define VGNSS_DOUBLE_ABS_MASK    0x7fffffffffffffffULL

typedef u64 vgnss_binder_size_t;
typedef u64 vgnss_binder_uintptr_t;

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
        u32 handle;
        vgnss_binder_uintptr_t ptr;
    } target;
    vgnss_binder_uintptr_t cookie;
    u32 code;
    u32 flags;
    s32 sender_pid;
    u32 sender_euid;
    vgnss_binder_size_t data_size;
    vgnss_binder_size_t offsets_size;
    union
    {
        struct
        {
            vgnss_binder_uintptr_t buffer;
            vgnss_binder_uintptr_t offsets;
        } ptr;
        u8 buf[8];
    } data;
};

struct vgnss_binder_transaction_data_sg
{
    struct vgnss_binder_transaction_data transaction_data;
    vgnss_binder_size_t buffers_size;
};

#define VGNSS_BINDER_WRITE_READ _IOWR('b', 1, struct vgnss_binder_write_read)
#define VGNSS_BC_TRANSACTION    _IOW('c', 0, struct vgnss_binder_transaction_data)
#define VGNSS_BC_REPLY          _IOW('c', 1, struct vgnss_binder_transaction_data)
#define VGNSS_BC_TRANSACTION_SG _IOW('c', 17, struct vgnss_binder_transaction_data_sg)
#define VGNSS_BC_REPLY_SG       _IOW('c', 18, struct vgnss_binder_transaction_data_sg)

enum vgnss_ioctl_arg_mode
{
    VGNSS_IOCTL_ARGS_ARM64_SYSCALL = 0,
    VGNSS_IOCTL_ARGS_DIRECT = 1,
};

static struct
{
    bool enabled;
    bool has_fix;
    int latitude_e7;
    int longitude_e7;
} vgps = {
    .enabled = false,
    .has_fix = false,
    .latitude_e7 = 0,
    .longitude_e7 = 0,
};

static DEFINE_MUTEX(vgnss_lock);

// 将 Parcel 偏移按 4 字节边界对齐。
static size_t vgnss_align4(size_t value)
{
    return (value + 3) & ~((size_t)3);
}

// 从临时缓冲区读取 32 位无符号整数，避免未对齐直接解引用。
static u32 vgnss_read_u32(const char *buf)
{
    u32 value;

    __builtin_memcpy(&value, buf, sizeof(value));
    return value;
}

// 从临时缓冲区读取 32 位有符号整数，避免未对齐直接解引用。
static s32 vgnss_read_s32(const char *buf)
{
    s32 value;

    __builtin_memcpy(&value, buf, sizeof(value));
    return value;
}

// 从临时缓冲区读取 64 位整数，避免未对齐直接解引用。
static u64 vgnss_read_u64(const char *buf)
{
    u64 value;

    __builtin_memcpy(&value, buf, sizeof(value));
    return value;
}

// 将 64 位整数写入临时缓冲区中的 Parcel 字段。
static void vgnss_write_u64(char *buf, u64 value)
{
    __builtin_memcpy(buf, &value, sizeof(value));
}

// 判断 IEEE754 double bit 的绝对值是否在指定范围内，并排除 NaN/Inf。
static bool vgnss_double_abs_le(u64 bits, u64 limit)
{
    u64 abs_bits = bits & VGNSS_DOUBLE_ABS_MASK;

    if ((abs_bits & VGNSS_DOUBLE_EXP_MASK) == VGNSS_DOUBLE_EXP_MASK) return false;

    return abs_bits <= limit;
}

// 用纯整数把 e7 经纬度转换为 IEEE754 double bit。
static u64 vgnss_e7_to_double_bits(int value)
{
    u64 sign = 0;
    u64 mag;
    u64 fixed;
    int top;
    int exp;
    u64 mant;

    if (value < 0)
    {
        sign = 0x8000000000000000ULL;
        mag = (u64)(-(s64)value);
    }
    else
    {
        mag = (u64)value;
    }

    if (!mag) return sign;

    fixed = ((mag << 32) + (VGNSS_COORD_SCALE_E7 / 2)) / VGNSS_COORD_SCALE_E7;
    if (!fixed) return sign;

    top = fls64(fixed) - 1;
    exp = top - 32 + 1023;
    if (exp <= 0) return sign;
    if (exp >= 2047) return sign | VGNSS_DOUBLE_EXP_MASK;

    if (top > 52) mant = fixed >> (top - 52);
    else mant = fixed << (52 - top);

    return sign | ((u64)exp << 52) | (mant & 0x000fffffffffffffULL);
}

// 在 UTF-16LE Parcel 字符串中匹配 ASCII interface token。
static bool vgnss_match_utf16le_ascii(const char *buf, size_t len, size_t pos, const char *token)
{
    size_t i;
    size_t token_len = __builtin_strlen(token);

    if (pos + token_len * 2 > len) return false;

    for (i = 0; i < token_len; i++)
    {
        if ((u8)buf[pos + i * 2] != (u8)token[i]) return false;
        if (buf[pos + i * 2 + 1] != 0) return false;
    }

    return true;
}

// 在 Parcel 中查找定位回调 interface token，并返回 token 结束位置。
static bool vgnss_find_location_token(const char *buf, size_t len, size_t *token_end)
{
    static const char listener_token[] = "android.location.ILocationListener";
    static const char callback_token[] = "android.location.ILocationCallback";
    size_t i;

    for (i = 0; i + 16 < len; i++)
    {
        if (vgnss_match_utf16le_ascii(buf, len, i, listener_token))
        {
            *token_end = i + __builtin_strlen(listener_token) * 2;
            return true;
        }

        if (vgnss_match_utf16le_ascii(buf, len, i, callback_token))
        {
            *token_end = i + __builtin_strlen(callback_token) * 2;
            return true;
        }
    }

    return false;
}

// 粗略校验 Location 的 wall time 和 elapsed realtime 字段是否合理。
static bool vgnss_time_fields_plausible(u64 time_ms, u64 elapsed_ns)
{
    if (!time_ms || !elapsed_ns) return false;
    if (time_ms > 4102444800000ULL) return false;

    return true;
}

// 按 Location Parcelable 布局校验并替换指定位置处的经纬度字段。
static bool vgnss_patch_location_at(char *buf, size_t len, size_t start, u64 lat_bits, u64 lon_bits)
{
    s32 provider_len;
    size_t pos;
    u32 fields_mask;
    u64 time_ms;
    u64 elapsed_ns;
    u64 old_lat;
    u64 old_lon;

    if (start + sizeof(u32) > len) return false;

    provider_len = vgnss_read_s32(buf + start);
    if (provider_len < -1 || provider_len > VGNSS_MAX_PROVIDER_BYTES) return false;

    pos = start + sizeof(u32);
    if (provider_len >= 0)
    {
        if (pos + (size_t)provider_len + 1 > len) return false;
        if (buf[pos + provider_len] != 0) return false;
        pos = vgnss_align4(pos + (size_t)provider_len + 1);
    }

    if (pos + sizeof(u32) + sizeof(u64) * 2 > len) return false;

    fields_mask = vgnss_read_u32(buf + pos);
    if (fields_mask & ~VGNSS_LOCATION_KNOWN_FIELD_MASK) return false;

    pos += sizeof(u32);
    time_ms = vgnss_read_u64(buf + pos);
    elapsed_ns = vgnss_read_u64(buf + pos + sizeof(u64));
    if (!vgnss_time_fields_plausible(time_ms, elapsed_ns)) return false;

    pos += sizeof(u64) * 2;
    if (fields_mask & VGNSS_LOCATION_HAS_ELAPSED_REALTIME_UNCERTAINTY_MASK) pos += sizeof(u64);

    if (pos + sizeof(u64) * 2 > len) return false;

    old_lat = vgnss_read_u64(buf + pos);
    old_lon = vgnss_read_u64(buf + pos + sizeof(u64));
    if (!vgnss_double_abs_le(old_lat, VGNSS_DOUBLE_ABS_LAT_90)) return false;
    if (!vgnss_double_abs_le(old_lon, VGNSS_DOUBLE_ABS_LON_180)) return false;

    vgnss_write_u64(buf + pos, lat_bits);
    vgnss_write_u64(buf + pos + sizeof(u64), lon_bits);
    return true;
}

// 扫描 Parcel 数据，定位并替换所有符合布局的 Location 对象。
static int vgnss_patch_location_parcel(char *buf, size_t len)
{
    size_t token_end = 0;
    size_t pos;
    int patched = 0;
    u64 lat_bits;
    u64 lon_bits;

    if (!vgnss_find_location_token(buf, len, &token_end)) return 0;

    lat_bits = vgnss_e7_to_double_bits(READ_ONCE(vgps.latitude_e7));
    lon_bits = vgnss_e7_to_double_bits(READ_ONCE(vgps.longitude_e7));

    for (pos = vgnss_align4(token_end); pos + 40 <= len; pos += 4)
    {
        if (vgnss_patch_location_at(buf, len, pos, lat_bits, lon_bits)) patched++;
    }

    return patched;
}

// 复制并修补单个 Binder transaction 中的 Parcel 数据。
static int vgnss_patch_transaction(const struct vgnss_binder_transaction_data *tr)
{
    char __user *user_buffer;
    char *parcel;
    size_t data_size;
    int patched;

    data_size = (size_t)tr->data_size;
    if (!data_size || data_size > VGNSS_MAX_PARCEL_BYTES) return 0;
    if (!tr->data.ptr.buffer) return 0;

    user_buffer = (char __user *)(uintptr_t)tr->data.ptr.buffer;
    parcel = vmalloc(data_size);
    if (!parcel) return 0;

    if (copy_from_user(parcel, user_buffer, data_size))
    {
        vfree(parcel);
        return 0;
    }

    patched = vgnss_patch_location_parcel(parcel, data_size);
    if (patched > 0)
    {
        unsigned long missing = copy_to_user(user_buffer, parcel, data_size);

        if (missing) ls_log_tag("vgnss", "parcel copy back missing=%lu size=%zu\n", missing, data_size);
        else ls_log_tag("vgnss", "patched %d Location object(s) size=%zu e7=%d/%d\n", patched, data_size, READ_ONCE(vgps.latitude_e7), READ_ONCE(vgps.longitude_e7));
    }

    vfree(parcel);
    return patched;
}

// 解析 Binder write buffer 中的 BC_* 命令并修补其中的位置回调 transaction。
static int vgnss_patch_binder_write_buffer(char *buf, size_t len)
{
    size_t pos = 0;
    int patched = 0;

    while (pos + sizeof(u32) <= len)
    {
        u32 cmd = vgnss_read_u32(buf + pos);

        pos += sizeof(u32);
        if (cmd == VGNSS_BC_TRANSACTION || cmd == VGNSS_BC_REPLY)
        {
            struct vgnss_binder_transaction_data tr;

            if (pos + sizeof(tr) > len) break;
            __builtin_memcpy(&tr, buf + pos, sizeof(tr));
            patched += vgnss_patch_transaction(&tr);
            pos += sizeof(tr);
            continue;
        }

        if (cmd == VGNSS_BC_TRANSACTION_SG || cmd == VGNSS_BC_REPLY_SG)
        {
            struct vgnss_binder_transaction_data_sg tr_sg;

            if (pos + sizeof(tr_sg) > len) break;
            __builtin_memcpy(&tr_sg, buf + pos, sizeof(tr_sg));
            patched += vgnss_patch_transaction(&tr_sg.transaction_data);
            pos += sizeof(tr_sg);
            continue;
        }

        break;
    }

    return patched;
}

// 处理被 inline hook 拦截到的 ioctl 参数，只拦截 Binder BINDER_WRITE_READ。
static int vgnss_handle_ioctl(struct pt_regs *regs, enum vgnss_ioctl_arg_mode mode)
{
    struct pt_regs *sys_regs;
    unsigned int cmd;
    void __user *argp;
    struct vgnss_binder_write_read bwr;
    char __user *write_user;
    char *write_buf;
    size_t write_size;

    if (!READ_ONCE(vgps.enabled) || !READ_ONCE(vgps.has_fix)) return 0;
    if (!regs) return 0;

    if (mode == VGNSS_IOCTL_ARGS_ARM64_SYSCALL)
    {
        sys_regs = (struct pt_regs *)regs->regs[0];
        if (!sys_regs) return 0;

        cmd = (unsigned int)sys_regs->regs[1];
        argp = (void __user *)sys_regs->regs[2];
    }
    else
    {
        cmd = (unsigned int)regs->regs[1];
        argp = (void __user *)regs->regs[2];
    }

    if (cmd != VGNSS_BINDER_WRITE_READ || !argp) return 0;

    if (copy_from_user(&bwr, argp, sizeof(bwr))) return 0;

    write_size = (size_t)bwr.write_size;
    if (!write_size || write_size > VGNSS_MAX_BINDER_WRITE_BYTES || !bwr.write_buffer) return 0;

    write_user = (char __user *)(uintptr_t)bwr.write_buffer;
    write_buf = vmalloc(write_size);
    if (!write_buf) return 0;

    if (!copy_from_user(write_buf, write_user, write_size)) vgnss_patch_binder_write_buffer(write_buf, write_size);

    vfree(write_buf);
    return 0;
}

// 适配 __arm64_sys_ioctl 入口，x0 是用户 syscall pt_regs 指针。
static int vgnss_arm64_sys_ioctl_hook(struct pt_regs *regs)
{
    return vgnss_handle_ioctl(regs, VGNSS_IOCTL_ARGS_ARM64_SYSCALL);
}

// 适配 __se_sys_ioctl 入口，ioctl 参数直接位于当前寄存器快照中。
static int vgnss_direct_ioctl_hook(struct pt_regs *regs)
{
    return vgnss_handle_ioctl(regs, VGNSS_IOCTL_ARGS_DIRECT);
}

static struct hook_entry vgnss_ioctl_hook_targets[][1] = {
    {HOOK_ENTRY("__arm64_sys_ioctl", vgnss_arm64_sys_ioctl_hook)},
    {HOOK_ENTRY("__se_sys_ioctl", vgnss_direct_ioctl_hook)},
};

// 安装 Binder ioctl inline hook，优先挂 syscall wrapper，失败后回退到内部实现。
static int vgnss_install_hook_locked(void)
{
    int i;
    int ret = -ENOENT;

    for (i = 0; i < ARRAY_SIZE(vgnss_ioctl_hook_targets); i++)
    {
        ret = inline_hook_install(vgnss_ioctl_hook_targets[i]);
        if (!ret)
        {
            ls_log_tag("vgnss", "inline hook on %s registered\n", vgnss_ioctl_hook_targets[i][0].target_sym);
            return 0;
        }

        ls_log_tag("vgnss", "inline hook on %s failed: %d\n", vgnss_ioctl_hook_targets[i][0].target_sym, ret);
    }

    return ret;
}

// 初始化虚拟定位状态并确保 Binder ioctl hook 已安装。
static inline int v_gnss_init(void)
{
    int ret;

    mutex_lock(&vgnss_lock);

    WRITE_ONCE(vgps.latitude_e7, 0);
    WRITE_ONCE(vgps.longitude_e7, 0);
    WRITE_ONCE(vgps.has_fix, false);
    WRITE_ONCE(vgps.enabled, true);
    ret = vgnss_install_hook_locked();

    mutex_unlock(&vgnss_lock);

    ls_log_tag("vgnss", "init binder_ioctl_inline_hook=%d enabled=1\n", ret);
    return ret;
}

// 更新虚拟经纬度，单位为 degrees * 10000000。
static inline int v_gnss_report(int latitude_e7, int longitude_e7)
{
    if (latitude_e7 < -900000000 || latitude_e7 > 900000000) return -EINVAL;
    if (longitude_e7 < -1800000000 || longitude_e7 > 1800000000) return -EINVAL;

    WRITE_ONCE(vgps.latitude_e7, latitude_e7);
    WRITE_ONCE(vgps.longitude_e7, longitude_e7);
    WRITE_ONCE(vgps.has_fix, true);
    WRITE_ONCE(vgps.enabled, true);

    ls_log_tag("vgnss", "report e7=%d/%d\n", latitude_e7, longitude_e7);
    return 0;
}

// 停用虚拟定位并卸载 Binder ioctl inline hook。
static inline void v_gnss_destroy(void)
{
    int i;

    mutex_lock(&vgnss_lock);

    WRITE_ONCE(vgps.enabled, false);
    WRITE_ONCE(vgps.has_fix, false);
    WRITE_ONCE(vgps.latitude_e7, 0);
    WRITE_ONCE(vgps.longitude_e7, 0);

    for (i = 0; i < ARRAY_SIZE(vgnss_ioctl_hook_targets); i++) inline_hook_remove(vgnss_ioctl_hook_targets[i]);

    ls_log_tag("vgnss", "inline hook unregistered\n");
    ls_log_tag("vgnss", "destroy\n");

    mutex_unlock(&vgnss_lock);
}
