#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/memory.h>
#include <linux/stop_machine.h>
#include <linux/version.h>
#include <asm/cacheflush.h>
#include <asm/debug-monitors.h>
#include <asm/insn.h>
#include <asm/virt.h>
#include <trace/events/sched.h>
#include "export_fun.h"
#include "hwbp_debug_reg.h"

struct breakpoint_config
{
    pid_t pid;          // 目标进程 pid
    enum hwbp_type bt;  // 断点类型
    enum hwbp_len bl;   // 断点长度
    enum hwbp_scope bs; // 断点作用线程范围
    uint64_t addr;      // 断点地址

    int suspended_step;                 // 当前是否在步过
    int suspended_type;                 // 1=执行断点步过，2=访问断点步过
    u32 suspended_ctrl;                 // 步过前保存的控制寄存器
    struct task_struct *suspended_task; // 正在步过的线程，防止其他线程误恢复

    // 触发回调，断点/观察点命中时调用
    // regs: 命中时的寄存器现场 self: 指向本结构体自身，方便回调访问配置信息
    void (*on_hit)(struct pt_regs *regs, struct breakpoint_config *self);

    // 允许携带私有的数据
    struct hwbp_info *bp_info;
};

/*
这里用全局变量来传递异常回调和断点写入上下文
应为异常处理路径的调用约定是硬件决定的，我没办法附加参数
注册线程调度回调那个可以附加参数，但是只能附加一个参数
既然使用全局指针传递上下文，那么<统一>使用传递的全局上下文，不在使用附带参数
内核很多子系统的做法也一样
*/
struct breakpoint_config *g_bp_config = NULL;

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

// 单步步过相关
static struct step_hook hwbp_step_hook;
static void (*fn_user_enable_single_step)(struct task_struct *task);
static void (*fn_user_disable_single_step)(struct task_struct *task);
static void (*fn_register_user_step_hook)(struct step_hook *hook);
static void (*fn_unregister_user_step_hook)(struct step_hook *hook);

// 命中断点后临时关闭当前控制寄存器，并开启单步执行一条指令
static void hwbp_begin_step(struct breakpoint_config *cfg, int type, struct pt_regs *regs)
{
    u32 ctrl;

    // 只处理用户态命中
    if (!cfg || !user_mode(regs))
        return;

    if (type == 1)
    {
        // 清空控制寄存器
        ctrl = read_wb_reg(AARCH64_DBG_REG_BCR, 0);
        write_wb_reg(AARCH64_DBG_REG_BCR, 0, 0);
    }
    else
    {
        // 清空控制寄存器
        ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, 0);
        write_wb_reg(AARCH64_DBG_REG_WCR, 0, 0);
    }

    // 记录恢复所需状态，single-step 异常回来时使用
    cfg->suspended_step = 1;
    cfg->suspended_type = type;
    cfg->suspended_ctrl = ctrl;
    cfg->suspended_task = current;

    // 开启用户态单步，返回用户态执行一条指令后会进入 hwbp_user_step_handler
    fn_user_enable_single_step(current);
}

// 单步异常回调：恢复刚才临时关闭的 BCR/WCR
static int hwbp_user_step_handler(struct pt_regs *regs, unsigned long esr)
{
    struct breakpoint_config *cfg = g_bp_config;

    (void)esr;

    // 不是我们发起的单步就交给系统原有处理链
    if (!cfg || !cfg->suspended_step || cfg->suspended_task != current)
        return DBG_HOOK_ERROR;

    if (cfg->suspended_type == 1)
        // 恢复执行断点
        write_wb_reg(AARCH64_DBG_REG_BCR, 0, cfg->suspended_ctrl);
    else if (cfg->suspended_type == 2)
        // 恢复访问断点
        write_wb_reg(AARCH64_DBG_REG_WCR, 0, cfg->suspended_ctrl);

    // 清理步过状态
    cfg->suspended_step = 0;
    cfg->suspended_type = 0;
    cfg->suspended_ctrl = 0;
    cfg->suspended_task = NULL;

    // 关闭本次单步，告诉异常分发器这次 single-step 已经处理
    fn_user_disable_single_step(current);
    return DBG_HOOK_HANDLED;
}

// 安装用户态 single-step hook，用来在步过后恢复断点
static int hwbp_step_hook_install(void)
{
    // user_enable_single_step:返回用户态后，执行下一条指令之后触发一次硬件单步异常。
    fn_user_enable_single_step = (void *)generic_kallsyms_lookup_name("user_enable_single_step");
    fn_user_disable_single_step = (void *)generic_kallsyms_lookup_name("user_disable_single_step");
    fn_register_user_step_hook = (void *)generic_kallsyms_lookup_name("register_user_step_hook");
    fn_unregister_user_step_hook = (void *)generic_kallsyms_lookup_name("unregister_user_step_hook");

    if (!fn_user_enable_single_step || !fn_user_disable_single_step ||
        !fn_register_user_step_hook || !fn_unregister_user_step_hook)
    {
        pr_debug("[driver] cannot find single-step symbols\n");
        return -ENOENT;
    }

    // 注册到 arm64 debug-monitors 的用户态单步回调链
    memset(&hwbp_step_hook, 0, sizeof(hwbp_step_hook));
    hwbp_step_hook.fn = (void *)hwbp_user_step_handler;
    fn_register_user_step_hook(&hwbp_step_hook);

    return 0;
}
static void hwbp_step_hook_remove(void)
{
    fn_unregister_user_step_hook(&hwbp_step_hook);
}

// 编码一条 ARM64 B 指令(跳转指令所在地址, 跳转目标地址)
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
static noinline int trampoline_breakpoint(unsigned long unused, unsigned long esr, struct pt_regs *regs)
{
    struct breakpoint_config *cfg = g_bp_config;
    if (cfg && cfg->on_hit)
        cfg->on_hit(regs, cfg);

    hwbp_begin_step(cfg, 1, regs);

    return 0;
}
// 数据断异常处理跳板
static noinline int trampoline_watchpoint(unsigned long addr, unsigned long esr, struct pt_regs *regs)
{
    struct breakpoint_config *cfg = g_bp_config;
    if (cfg && cfg->on_hit)
        cfg->on_hit(regs, cfg);

    hwbp_begin_step(cfg, 2, regs);
    return 0;
}

// inline hook(目标函数地址，跳板函数地址，输出参数原始指令)
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

// 还原单个目标函数的原始指令(目标函数地址，原始指令)
static void unhook_one(unsigned long target_addr, u32 saved_insn)
{
    fn_aarch64_insn_patch_text_nosync((void *)target_addr, saved_insn);
    pr_debug("[driver] restored 0x%lx (insn: 0x%08x)\n", target_addr, saved_insn);
}
// 安装hook接管断点异常
int hw_breakpoint_hook_install(void)
{
    int ret;

    ret = hwbp_step_hook_install();
    if (ret)
        return ret;

    bp_addr = generic_kallsyms_lookup_name("breakpoint_handler");
    if (!bp_addr)
    {
        pr_debug("[driver] cannot find symbol: breakpoint_handler\n");
        hwbp_step_hook_remove();
        return -ENOENT;
    }
    wp_addr = generic_kallsyms_lookup_name("watchpoint_handler");
    if (!wp_addr)
    {
        pr_debug("[driver] cannot find symbol: watchpoint_handler\n");
        hwbp_step_hook_remove();
        return -ENOENT;
    }

    // 安装 breakpoint_handler 的 inline hook
    ret = hook_one(bp_addr, (unsigned long)trampoline_breakpoint,
                   &orig_bp_insn);
    if (ret)
    {
        pr_debug("[driver] hook breakpoint_handler failed: %d\n", ret);
        hwbp_step_hook_remove();
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
        hwbp_step_hook_remove();
        return ret;
    }

    pr_debug("[driver] hook installed\n");
    return 0;
}
void hw_breakpoint_hook_remove(void)
{
    // 还原 watchpoint_handler 原始指令
    unhook_one(wp_addr, orig_wp_insn);

    // 还原 breakpoint_handler 原始指令
    unhook_one(bp_addr, orig_bp_insn);

    hwbp_step_hook_remove();

    pr_debug("[driver] hook removed\n");
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
    // 使用 start_task_run_monitor 传递到全局的上下文
    struct breakpoint_config *bp_config = g_bp_config;

    if (!bp_config)
        return;

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
            if (bp_config->suspended_step && bp_config->suspended_task == next && bp_config->suspended_type == 1)
                write_wb_reg(AARCH64_DBG_REG_BCR, 0, 0);
            else
                write_wb_reg(AARCH64_DBG_REG_BCR, 0, encode_ctrl_reg(info.ctrl) | 0x1);
            // write_wb_reg(AARCH64_DBG_REG_BCR, 0, encode_ctrl_reg(info.ctrl) & ~0x1);
        }
        else
        {
            write_wb_reg(AARCH64_DBG_REG_WVR, 0, info.address);
            //"| 0x1"表示立即生效,
            //"& ~0x1"表示写入的寄存器配置，但是禁用不生效
            //"0"给控制寄存器请0就删除了断点
            if (bp_config->suspended_step && bp_config->suspended_task == next && bp_config->suspended_type == 2)
                write_wb_reg(AARCH64_DBG_REG_WCR, 0, 0);
            else
                write_wb_reg(AARCH64_DBG_REG_WCR, 0, encode_ctrl_reg(info.ctrl) | 0x1);
            // write_wb_reg(AARCH64_DBG_REG_WCR, 0, encode_ctrl_reg(info.ctrl) & ~0x1);
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
    // 传递上下文给全局，让异常处理和断点写入都能互相传递配置信息
    g_bp_config = bp_config;

    ret = register_trace_sched_switch(probe_sched_switch, NULL);
    if (ret)
    {
        pr_debug("register_trace_sched_switch failed: %d\n", ret);
        return ret;
    }

    pr_debug("monitor start, target tgid=%d\n", g_bp_config->pid);
    return 0;
}

// 注销回调，取消监听
static void stop_task_run_monitor(void)
{
    unregister_trace_sched_switch(probe_sched_switch, NULL);
    pr_debug("monitor stop, target tgid=%d\n", g_bp_config->pid);
}
