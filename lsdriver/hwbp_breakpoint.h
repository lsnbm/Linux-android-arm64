#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/memory.h>
#include <linux/stop_machine.h>
#include <asm/cacheflush.h>
#include <asm/insn.h>
#include <asm/virt.h>
#include <trace/events/sched.h>
#include "export_fun.h"

struct breakpoint_config
{
    pid_t pid;          // 目标进程 pid
    enum hwbp_type bt;  // 断点类型
    enum hwbp_len bl;   // 断点长度
    enum hwbp_scope bs; // 断点作用线程范围
    uint64_t addr;      // 断点地址

    // 触发回调，断点/观察点命中时调用
    // regs: 命中时的寄存器现场 self: 指向本结构体自身，方便回调访问配置信息
    void (*on_hit)(struct pt_regs *regs, struct breakpoint_config *self);

    // 允许携带私有的数据
    struct hwbp_info *bp_info;
};

// 这里用全局变量来传递异常回调上下文，应为异常处理路径的调用约定是硬件决定的，我没办法附加参数
// 内核很多子系统的做法也一样
struct breakpoint_config *g_exception = NULL;

/*
kprobe 被 NOKPROBE_SYMBOL 拒绝(-EINVAL)，ftrace 未开启
因此改用 inline hook 方案：

需要注意的是PACIASP 指令（指针认证，ARM64 PAC 特性），这是函数的第一条
*/

// 保存原始的第一条指令，用于 trampoline 中还原执行
static u32 orig_bp_insn;
static u32 orig_wp_insn;

// 目标函数地址
static unsigned long bp_addr;
static unsigned long wp_addr;

/*
arm64_encode_branch - 编码一条 ARM64 B 指令
// from: 跳转指令所在地址
// to  : 跳转目标地址
// 返回: 编码好的 32 位指令字
*/
static u32 arm64_encode_branch(unsigned long from, unsigned long to)
{
    long offset = (long)(to - from);

    // B 指令范围 ±128MB，超出范围则无法使用
    if (offset < -(1L << 27) || offset >= (1L << 27))
    {
        pr_debug("[driver] branch offset out of range: 0x%lx -> 0x%lx\n",
                 from, to);
        return 0;
    }

    // [31:26]=000101b, [25:0]=offset>>2
    return (0x14000000U) | ((u32)((offset >> 2) & 0x3FFFFFF));
}

// 执行断异常处理跳板
static noinline void trampoline_breakpoint(unsigned long unused, unsigned long esr, struct pt_regs *regs)
{
    struct breakpoint_config *cfg = g_exception;
    if (cfg && cfg->on_hit)
        cfg->on_hit(regs, cfg);

    // 后续可以在这里执行原函数，先注释说明一下，以后再看
}
// 数据断异常处理跳板
static noinline void trampoline_watchpoint(unsigned long addr, unsigned long esr, struct pt_regs *regs)
{
    struct breakpoint_config *cfg = g_exception;
    if (cfg && cfg->on_hit)
        cfg->on_hit(regs, cfg);
}

/*
// hook_one -  inline hook
// target_addr : 目标函数地址
// trampoline  : 我们的跳板函数地址
// saved_insn  : 输出参数，保存被覆盖的原始指令
*/
static int hook_one(unsigned long target_addr, unsigned long trampoline_addr, u32 *saved_insn)
{
    u32 branch;

    // 保存目标函数入口原始指令，卸载时用于还原
    *saved_insn = *(u32 *)target_addr;

    // 编码跳转到 trampoline 的 B 指令
    branch = arm64_encode_branch(target_addr, trampoline_addr);
    if (!branch)
        return -ERANGE;

    // 将 B 指令写入目标函数入口
    fn_aarch64_insn_patch_text_nosync((void *)target_addr, branch);

    pr_debug("[driver] hooked 0x%lx -> 0x%lx (orig insn: 0x%08x)\n", target_addr, trampoline_addr, *saved_insn);
    return 0;
}

/*
unhook_one - 还原单个目标函数的原始指令
target_addr : 目标函数地址
saved_insn  : 原始指令
*/
static void unhook_one(unsigned long target_addr, u32 saved_insn)
{
    fn_aarch64_insn_patch_text_nosync((void *)target_addr, saved_insn);
    pr_debug("[driver] restored 0x%lx (insn: 0x%08x)\n", target_addr, saved_insn);
}
// 安装hook接管断点异常
int hw_breakpoint_hook_install(struct breakpoint_config *exception)
{
    int ret;
    // 传递上下文给异常处理，让异常处理调用回掉
    g_exception = exception;

    bp_addr = generic_kallsyms_lookup_name("breakpoint_handler");
    if (!bp_addr)
    {
        pr_debug("[driver] cannot find symbol: breakpoint_handler\n");
        return -ENOENT;
    }
    wp_addr = generic_kallsyms_lookup_name("watchpoint_handler");
    if (!wp_addr)
    {
        pr_debug("[driver] cannot find symbol: watchpoint_handler\n");
        return -ENOENT;
    }

    // 安装 breakpoint_handler 的 inline hook
    ret = hook_one(bp_addr, (unsigned long)trampoline_breakpoint,
                   &orig_bp_insn);
    if (ret)
    {
        pr_debug("[driver] hook breakpoint_handler failed: %d\n", ret);
        return ret;
    }

    // 安装 watchpoint_handler 的 inline hook
    ret = hook_one(wp_addr, (unsigned long)trampoline_watchpoint,
                   &orig_wp_insn);
    if (ret)
    {
        pr_debug("[driver] hook watchpoint_handler failed: %d\n", ret);
        // 回滚已安装的 breakpoint hook
        unhook_one(bp_addr, orig_bp_insn);
        return ret;
    }

    pr_debug("[driver] hook installed\n");
    return 0;
}
// 删除hook
void hw_breakpoint_hook_remove(void)
{
    // 还原 watchpoint_handler 原始指令
    unhook_one(wp_addr, orig_wp_insn);

    // 还原 breakpoint_handler 原始指令
    unhook_one(bp_addr, orig_bp_insn);

    pr_debug("[driver] hook removed\n");
}

// 读写调试寄存器的宏
#define read_sysreg(r) ({                                  \
    u64 __val;                                             \
    asm volatile("mrs %0, " __stringify(r) : "=r"(__val)); \
    __val;                                                 \
})

#define write_sysreg(v, r)                         \
    do                                             \
    {                                              \
        u64 __val = (u64)(v);                      \
        asm volatile("msr " __stringify(r) ", %x0" \
                     : : "rZ"(__val));             \
    } while (0)

#define AARCH64_DBG_READ(N, REG, VAL)         \
    do                                        \
    {                                         \
        VAL = read_sysreg(dbg##REG##N##_el1); \
    } while (0)

#define AARCH64_DBG_WRITE(N, REG, VAL)        \
    do                                        \
    {                                         \
        write_sysreg(VAL, dbg##REG##N##_el1); \
    } while (0)

#define READ_WB_REG_CASE(OFF, N, REG, VAL) \
    case (OFF + N):                        \
        AARCH64_DBG_READ(N, REG, VAL);     \
        break

#define WRITE_WB_REG_CASE(OFF, N, REG, VAL) \
    case (OFF + N):                         \
        AARCH64_DBG_WRITE(N, REG, VAL);     \
        break

#define GEN_READ_WB_REG_CASES(OFF, REG, VAL) \
    READ_WB_REG_CASE(OFF, 0, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 1, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 2, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 3, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 4, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 5, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 6, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 7, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 8, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 9, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 10, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 11, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 12, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 13, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 14, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 15, REG, VAL)

#define GEN_WRITE_WB_REG_CASES(OFF, REG, VAL) \
    WRITE_WB_REG_CASE(OFF, 0, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 1, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 2, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 3, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 4, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 5, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 6, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 7, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 8, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 9, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 10, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 11, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 12, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 13, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 14, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 15, REG, VAL)

// reg:读哪一类寄存器，n:该类寄存器中的槽位编号 return:对应寄存器中的64位值
static u64 read_wb_reg(int reg, int n)
{
    u64 val = 0;

    switch (reg + n)
    {
        GEN_READ_WB_REG_CASES(AARCH64_DBG_REG_BVR, AARCH64_DBG_REG_NAME_BVR, val);
        GEN_READ_WB_REG_CASES(AARCH64_DBG_REG_BCR, AARCH64_DBG_REG_NAME_BCR, val);
        GEN_READ_WB_REG_CASES(AARCH64_DBG_REG_WVR, AARCH64_DBG_REG_NAME_WVR, val);
        GEN_READ_WB_REG_CASES(AARCH64_DBG_REG_WCR, AARCH64_DBG_REG_NAME_WCR, val);
    default:
        pr_debug("[driver] attempt to read from unknown breakpoint register %d\n", n);
    }

    return val;
}

// reg:写哪一类寄存器，n:该类寄存器中的槽位编号，val:要写入寄存器的 64 位值
static void write_wb_reg(int reg, int n, u64 val)
{
    switch (reg + n)
    {
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_BVR, AARCH64_DBG_REG_NAME_BVR, val);
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_BCR, AARCH64_DBG_REG_NAME_BCR, val);
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_WVR, AARCH64_DBG_REG_NAME_WVR, val);
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_WCR, AARCH64_DBG_REG_NAME_WCR, val);
    default:
        pr_debug("[driver] attempt to write to unknown breakpoint register %d\n", n);
    }
    isb();
}

// 获取执行给观察寄存器数量
static inline int get_brps_num(void)
{
    u64 dfr0;
    asm volatile("mrs %0, id_aa64dfr0_el1" : "=r"(dfr0));
    return ((dfr0 >> 12) & 0xF) + 1;
}
static inline int get_wrps_num(void)
{
    u64 dfr0;
    asm volatile("mrs %0, id_aa64dfr0_el1" : "=r"(dfr0));
    return ((dfr0 >> 20) & 0xF) + 1;
}

// 解锁操作系统调试锁和全局启用硬件调试功能
static inline void enable_hardware_debug_on_cpu(void *unused)
{
    uint64_t mdscr;

    (void)unused;

    // 解锁 OS Lock，允许访问调试寄存器
    __asm__ volatile(
        "msr oslar_el1, xzr\n\t"
        "isb\n\t" ::: "memory");

    /*
    读取 MDSCR_EL1，置位后写回：
    bit 15 (MDE): Monitor Debug Enable，用户态调试使能(EL0)
    bit 13 (KDE): Kernel Debug Enable，内核态调试使能(EL1)
    */
    __asm__ volatile(
        "mrs %[val], mdscr_el1\n\t"                     // 读取调试寄存器
        "orr %[val], %[val], %[mask]\n\t"               // 置位 MDE 和 KDE 位
        "msr mdscr_el1, %[val]\n\t"                     // 写回
        "isb\n\t"                                       // 指令同步
        : [val] "=&r"(mdscr)                            // 输出变量
        : [mask] "r"((uint64_t)((1 << 15) | (1 << 13))) // 输入掩码
        : "memory");                                    // 编译器屏障
}

// 关闭当前 CPU 上的自托管硬件调试；重新上 OS Lock
static inline void disable_hardware_debug_on_cpu(void *unused)
{
    uint64_t mdscr;

    (void)unused;

    // 清掉 MDSCR_EL1 的 MDE(bit15) 和 KDE(bit13)
    __asm__ volatile(
        "mrs    %[val], mdscr_el1\n\t"
        "bic    %[val], %[val], %[mask]\n\t" // 清 MDE|KDE
        "msr    mdscr_el1, %[val]\n\t"
        "isb\n\t"
        : [val] "=&r"(mdscr)
        : [mask] "r"((uint64_t)((1UL << 15) | (1UL << 13)))
        : "memory");

    // 重新锁住 OS Lock
    __asm__ volatile(
        "mov    x0, #1\n\t"
        "msr    oslar_el1, x0\n\t" // bit0=1 -> lock
        "isb\n\t"
        :
        :
        : "x0", "memory");
}

/*
 把外部断点参数转换成ARM架构内部格式，并完成基础检测/修正。
 这里只处理用户态断点（EL0）场景。
 在32位的task和per-cpu 场景不能按compat处理，要=0
 */
static int hw_breakpoint_parse(struct breakpoint_config *cfg, bool is_compat, struct arch_hw_breakpoint *hw)
{
    u64 alignment_mask, offset;

    if (!cfg || !hw)
        return -EINVAL;

    memset(hw, 0, sizeof(*hw));

    // 类型转换：对应 arch_build_bp_info()
    switch (cfg->bt)
    {
    case HW_BREAKPOINT_X:
        hw->ctrl.type = ARM_BREAKPOINT_EXECUTE;
        break;
    case HW_BREAKPOINT_R:
        hw->ctrl.type = ARM_BREAKPOINT_LOAD;
        break;
    case HW_BREAKPOINT_W:
        hw->ctrl.type = ARM_BREAKPOINT_STORE;
        break;
    case HW_BREAKPOINT_RW:
        hw->ctrl.type = ARM_BREAKPOINT_LOAD | ARM_BREAKPOINT_STORE;
        break;
    default:
        return -EINVAL;
    }

    // 长度转换：对应 arch_build_bp_info()
    switch (cfg->bl)
    {
    case HW_BREAKPOINT_LEN_1:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_1;
        break;
    case HW_BREAKPOINT_LEN_2:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_2;
        break;
    case HW_BREAKPOINT_LEN_3:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_3;
        break;
    case HW_BREAKPOINT_LEN_4:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_4;
        break;
    case HW_BREAKPOINT_LEN_5:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_5;
        break;
    case HW_BREAKPOINT_LEN_6:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_6;
        break;
    case HW_BREAKPOINT_LEN_7:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_7;
        break;
    case HW_BREAKPOINT_LEN_8:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_8;
        break;
    default:
        return -EINVAL;
    }

    // 执行断点/观察点长度合法性检查：对应 arch_build_bp_info()
    if (hw->ctrl.type == ARM_BREAKPOINT_EXECUTE)
    {
        if (is_compat)
        {
            if (hw->ctrl.len != ARM_BREAKPOINT_LEN_2 &&
                hw->ctrl.len != ARM_BREAKPOINT_LEN_4)
                return -EINVAL;
        }
        else
        {
            // AArch64 执行断点只允许 4 字节。源码里这里不是直接报错，而是修正成 4。
            if (hw->ctrl.len != ARM_BREAKPOINT_LEN_4)
                hw->ctrl.len = ARM_BREAKPOINT_LEN_4;
        }
    }

    // 地址初始值：对应 arch_build_bp_info()
    hw->address = cfg->addr;

    // 权限：这里只做用户态断点
    hw->ctrl.privilege = AARCH64_BREAKPOINT_EL0;
    hw->ctrl.enabled = 1;

    // 对齐检查和修正：对应内核源码 hw_breakpoint_arch_parse()
    if (is_compat)
    {

        if (hw->ctrl.len == ARM_BREAKPOINT_LEN_8)
            alignment_mask = 0x7;
        else
            alignment_mask = 0x3;

        offset = hw->address & alignment_mask;

        switch (offset)
        {
        case 0:
            break;
        case 1:
        case 2:
            if (hw->ctrl.len == ARM_BREAKPOINT_LEN_2)
                break;
            fallthrough;
        case 3:
            if (hw->ctrl.len == ARM_BREAKPOINT_LEN_1)
                break;
            fallthrough;
        default:
            return -EINVAL;
        }
    }
    else
    {
        if (hw->ctrl.type == ARM_BREAKPOINT_EXECUTE)
            alignment_mask = 0x3;
        else
            alignment_mask = 0x7;

        offset = hw->address & alignment_mask;
    }

    // 地址向下对齐到硬件要求的边界
    hw->address &= ~alignment_mask;
    hw->ctrl.len <<= offset;

    return 0;
}

// 控制码转ARM架构内部格式，这里注释了使用内核头文件自带的
// static inline u32 encode_ctrl_reg(struct arch_hw_breakpoint_ctrl ctrl)
// {
//     u32 val = (ctrl.len << 5) | (ctrl.type << 3) | (ctrl.privilege << 1) |
//               ctrl.enabled;

//     if (is_kernel_in_hyp_mode() && ctrl.privilege == AARCH64_BREAKPOINT_EL1)
//         val |= DBG_HMC_HYP;

//     return val;
// }

// 线程切换回调,6.1系是分水岭，内核整体上下区别变化大
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static void probe_sched_switch(void *data, bool preempt,
                               struct task_struct *prev,
                               struct task_struct *next,
                               unsigned int prev_state)
#else // 这个回调运行在发生切换的那颗 CPU上，(task被切换到cpu5,这个回调就是cpu5运行)
static void probe_sched_switch(void *data, bool preempt,
                               struct task_struct *prev,
                               struct task_struct *next)
#endif
{
    // 取传入结构体指针
    struct breakpoint_config *bp_config = (struct breakpoint_config *)data;

    // 目标进程的线程组被切入(线程组id就是进程的pid)
    if (next->tgid == bp_config->pid)
    {
        // 线程id==线程组id就是主线程,否则子线程
        if (next->pid == next->tgid)
        {
            pr_debug("目标进程的主线程被切换进来: pid=%d comm=%s cpu=%d\n", next->pid, next->comm, raw_smp_processor_id());
        }
        else
        {
            pr_debug("目标进程的子线程被切换进来: pid=%d comm=%s cpu=%d\n", next->pid, next->comm, raw_smp_processor_id());
        }

        // task被切入到cpu进行解锁OS+开启硬件调试
        enable_hardware_debug_on_cpu(NULL);

        // 把断点描述信息转化为arm架构内部格式
        struct arch_hw_breakpoint info;
        hw_breakpoint_parse(bp_config, 0, &info);

        // 根据断点类型进行分发
        if (info.ctrl.type == ARM_BREAKPOINT_EXECUTE)
        {
            write_wb_reg(AARCH64_DBG_REG_BVR, 0, info.address);
            //"| 0x1"表示立即生效,
            //"& ~0x1"表示写入的寄存器配置，但是禁用不生效
            //"0"给控制寄存器请0，就删除了断点
            write_wb_reg(AARCH64_DBG_REG_BCR, 0, encode_ctrl_reg(info.ctrl) | 0x1);
            // write_wb_reg(AARCH64_DBG_REG_BCR, 0, encode_ctrl_reg(info->ctrl) & ~0x1);
        }
        else
        {
            write_wb_reg(AARCH64_DBG_REG_WVR, 0, info.address);
            //"| 0x1"表示立即生效,
            //"& ~0x1"表示写入的寄存器配置，但是禁用不生效
            //"0"给控制寄存器请0就删除了断点
            write_wb_reg(AARCH64_DBG_REG_WCR, 0, encode_ctrl_reg(info.ctrl) | 0x1);
            // write_wb_reg(AARCH64_DBG_REG_WCR, 0, encode_ctrl_reg(info->ctrl) & ~0x1);
        }
    }

    if (prev->tgid == bp_config->pid)
    {
        if (prev->pid == prev->tgid)
        {
            pr_debug("目标进程的主线程被切换走: pid=%d comm=%s cpu=%d\n", prev->pid, prev->comm, raw_smp_processor_id());
        }
        else
        {
            pr_debug("目标进程的子线程被切换走: pid=%d comm=%s cpu=%d\n", prev->pid, prev->comm, raw_smp_processor_id());
        }

        // 请0执行寄存器和访问寄存器的控制寄存器
        write_wb_reg(AARCH64_DBG_REG_BCR, 0, 0);
        write_wb_reg(AARCH64_DBG_REG_WCR, 0, 0);

        // task被切出cpu进行管全局调试+上锁OS
        disable_hardware_debug_on_cpu(NULL);
    }
}

// 注册线程切换回调，开始监听
static int start_task_run_monitor(struct breakpoint_config *bp_config)
{
    int ret;

    if (bp_config->pid <= 0)
    {
        pr_debug("pid error\n");
        return -EINVAL;
    }

    ret = register_trace_sched_switch(probe_sched_switch, (void *)bp_config);
    if (ret)
    {
        pr_debug("register_trace_sched_switch failed: %d\n", ret);
        return ret;
    }

    pr_debug("monitor start, target tgid=%d\n", bp_config->pid);
    return 0;
}

// 注销回调，取消监听
static void stop_task_run_monitor(struct breakpoint_config *bp_config)
{
    unregister_trace_sched_switch(probe_sched_switch, (void *)bp_config);
    pr_debug("monitor stop, target tgid=%d\n", bp_config->pid);
}
