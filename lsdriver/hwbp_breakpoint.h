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

    // 触发回调，命中时调用
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

// 保存原始的第一条指令
static u32 orig_bp_insn;
static u32 orig_wp_insn;

// 用extern申明2个链接符号，去引用下面汇编标签符号的地址，运行时把下面汇编nop修改为保存的原指令
extern char bp_orig_insn_slot[];
extern char wp_orig_insn_slot[];

// 保存的被hook函数地址
static unsigned long breakpoint_handler;
static unsigned long watchpoint_handler;

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

__attribute__((used)) static void work_trampoline_breakpoint(unsigned long unused, unsigned long esr, struct pt_regs *regs)
{
    struct breakpoint_config *cfg = g_bp_config;
    if (cfg && cfg->on_hit)
        cfg->on_hit(regs, cfg);

    /*
    这里说明一下为何可以这么做进行步过
        现在代码安装断点的方式是线程被调度到cpu上就写入对应的cpu寄存器进行断点，调度走就清空控制寄存器删除断点，这样就实现了断点跟着task走
        但是呢这里的异常回调我们关闭寄存器了进行步过后，要是线程一直运行没有被调度，断点就不会被重新打开对不对!

        其实不用担心这个不会被调度问题，因为我实际测试下面这种代码
        while (1){a++;}
        这种只进行纯!算数运算!的进程才70%不会被调度走一直运行，下面有说原因
        所以一个正常的用户使用的进程,绝对不会出现这个整个进程的线程组都在无限算数运算

        一个正常进程100%会出现下面情况，这些情况都会导致被调度走，一旦线程组中有task被调度都能收到并重新安装好因步过关闭的断点
        1.当前任务主动睡眠，           不怎么出现;                             sleep() / nanosleep() / msleep()...
        2.阻塞 IO 操作，               必出现，    网络请求和系统调用和日志之类的;  printf()/ read() / recv() / send() / connect() / accept()....
        3.锁竞争会触发调度，           几乎必出现， 多线程下非常常见对资源的保护;                  std::mutex / std::shared_mutex / std::spinlock...
        4.时间片到了CFS 抢占，         必出现，     调度器的核心机制，不过要等时间片，很久才会调度
        5.高优先级任务被唤醒会触发抢占，必出现，    不过要等被抢占，不怎么会被调度
        6.硬件中断，                   必出现，    不过中断时内核可能不会运行抢占任务，不确定会不会被调度
        7.page fault 缺页，            可能出现，  访问的虚拟地址会没有对应的物理页会触发一次，因为访问了会常驻了，很久才会调度
        8.新task创建，                 不怎么出现，就创建一次长期运行
        9.图形渲染提交画面，            几乎必出现，opengl/vulkan 之类的渲染提交
        10.等等等太多了，我就只知道这一部分
        所以放心在异常回调关断步过
        */

    /*
    这里先实时读取了执行控制寄存器配置，并只修改了bit 0 enabled是否启用位
    为何不直接清空的原因就是
        用户态如果也用perf下断，原本的硬件 debug 异常入口需要控制寄存器中的len/type/privilege
        由于 BCR/WCR 被清空，原硬件 debug 异常入口无法通过 BVR/BCR 或 WVR/WCR 匹配到
        对应的 perf_event owner，也就不会执行 perf_bp_event() 和后续disable + single-step + restore 的步过状态机。
        硬件debug异常分发直接结束并返回已处理

      结果是：硬件debug异常分发结束了，但 perf子系统没有收到这次命中的信息和步过闭环，状态机推进异常就死了
    */

    u64 ctrl = read_wb_reg(AARCH64_DBG_REG_BCR, 5);
    write_wb_reg(AARCH64_DBG_REG_BCR, 5, ctrl & ~0x1);
}
__attribute__((used)) static void work_trampoline_watchpoint(unsigned long addr, unsigned long esr, struct pt_regs *regs)
{
    struct breakpoint_config *cfg = g_bp_config;
    if (cfg && cfg->on_hit)
        cfg->on_hit(regs, cfg);
    u64 ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, 3);
    write_wb_reg(AARCH64_DBG_REG_WCR, 3, ctrl & ~0x1);
}

// 执行断异常处理跳板
__attribute__((naked, __noinline__)) static int trampoline_breakpoint(unsigned long unused, unsigned long esr, struct pt_regs *regs)
{
    asm volatile(
        // 开辟272 字节栈空间
        "sub sp, sp, #272\n"

        // 保存所有通用寄存器
        "stp x0,  x1,  [sp, #0]\n"
        "stp x2,  x3,  [sp, #16]\n"
        "stp x4,  x5,  [sp, #32]\n"
        "stp x6,  x7,  [sp, #48]\n"
        "stp x8,  x9,  [sp, #64]\n"
        "stp x10, x11, [sp, #80]\n"
        "stp x12, x13, [sp, #96]\n"
        "stp x14, x15, [sp, #112]\n"
        "stp x16, x17, [sp, #128]\n"
        "stp x18, x19, [sp, #144]\n"
        "stp x20, x21, [sp, #160]\n"
        "stp x22, x23, [sp, #176]\n"
        "stp x24, x25, [sp, #192]\n"
        "stp x26, x27, [sp, #208]\n"
        "stp x28, x29, [sp, #224]\n"
        "str x30,      [sp, #240]\n"

        // 保存 NZCV 条件标志
        "mrs x9, nzcv\n"
        "str x9, [sp, #256]\n"

        // 给 trampoline 自己建立临时栈帧
        "mov x29, sp\n"

        // 调用工作逻辑
        "bl work_trampoline_breakpoint\n"

        // 恢复 NZCV 条件标志
        "ldr x9, [sp, #256]\n"
        "msr nzcv, x9\n"

        // 恢复所有通用寄存器
        "ldr x30,      [sp, #240]\n"
        "ldp x28, x29, [sp, #224]\n"
        "ldp x26, x27, [sp, #208]\n"
        "ldp x24, x25, [sp, #192]\n"
        "ldp x22, x23, [sp, #176]\n"
        "ldp x20, x21, [sp, #160]\n"
        "ldp x18, x19, [sp, #144]\n"
        "ldp x16, x17, [sp, #128]\n"
        "ldp x14, x15, [sp, #112]\n"
        "ldp x12, x13, [sp, #96]\n"
        "ldp x10, x11, [sp, #80]\n"
        "ldp x8,  x9,  [sp, #64]\n"
        "ldp x6,  x7,  [sp, #48]\n"
        "ldp x4,  x5,  [sp, #32]\n"
        "ldp x2,  x3,  [sp, #16]\n"
        "ldp x0,  x1,  [sp, #0]\n"

        // 恢复原始 sp
        "add sp, sp, #272\n"

        // 初始是 nop，上面符号链接到这个地址，安装 hook 时 patch 成 orig_bp_insn。
        ".global bp_orig_insn_slot\n"
        "bp_orig_insn_slot:\n"
        ".inst 0xd503201f\n"

        // 跳回保存的 breakpoint_handler + 4。
        "ldr x16, =breakpoint_handler\n"
        "ldr x16, [x16]\n"
        "add x16, x16, #4\n"
        "br x16\n"
        :
        :
        : "memory");
}
// 访问断异常处理跳板
__attribute__((naked, __noinline__)) static int trampoline_watchpoint(unsigned long addr, unsigned long esr, struct pt_regs *regs)
{
    asm volatile(
        // 开辟272 字节栈空间
        "sub sp, sp, #272\n"

        // 保存所有通用寄存器
        "stp x0,  x1,  [sp, #0]\n"
        "stp x2,  x3,  [sp, #16]\n"
        "stp x4,  x5,  [sp, #32]\n"
        "stp x6,  x7,  [sp, #48]\n"
        "stp x8,  x9,  [sp, #64]\n"
        "stp x10, x11, [sp, #80]\n"
        "stp x12, x13, [sp, #96]\n"
        "stp x14, x15, [sp, #112]\n"
        "stp x16, x17, [sp, #128]\n"
        "stp x18, x19, [sp, #144]\n"
        "stp x20, x21, [sp, #160]\n"
        "stp x22, x23, [sp, #176]\n"
        "stp x24, x25, [sp, #192]\n"
        "stp x26, x27, [sp, #208]\n"
        "stp x28, x29, [sp, #224]\n"
        "str x30,      [sp, #240]\n"

        // 保存 NZCV 条件标志
        "mrs x9, nzcv\n"
        "str x9, [sp, #256]\n"

        // 给 trampoline 自己建立临时栈帧
        "mov x29, sp\n"

        // 调用工作逻辑
        "bl work_trampoline_watchpoint\n"

        // 恢复 NZCV 条件标志
        "ldr x9, [sp, #256]\n"
        "msr nzcv, x9\n"

        // 恢复所有通用寄存器
        "ldr x30,      [sp, #240]\n"
        "ldp x28, x29, [sp, #224]\n"
        "ldp x26, x27, [sp, #208]\n"
        "ldp x24, x25, [sp, #192]\n"
        "ldp x22, x23, [sp, #176]\n"
        "ldp x20, x21, [sp, #160]\n"
        "ldp x18, x19, [sp, #144]\n"
        "ldp x16, x17, [sp, #128]\n"
        "ldp x14, x15, [sp, #112]\n"
        "ldp x12, x13, [sp, #96]\n"
        "ldp x10, x11, [sp, #80]\n"
        "ldp x8,  x9,  [sp, #64]\n"
        "ldp x6,  x7,  [sp, #48]\n"
        "ldp x4,  x5,  [sp, #32]\n"
        "ldp x2,  x3,  [sp, #16]\n"
        "ldp x0,  x1,  [sp, #0]\n"

        // 恢复原始 sp
        "add sp, sp, #272\n"

        // 初始是 nop，上面符号链接到这个地址，安装 hook 时 patch 成 orig_bp_insn。
        ".global wp_orig_insn_slot\n"
        "wp_orig_insn_slot:\n"
        ".inst 0xd503201f\n"

        // 跳回保存的 watchpoint_handler + 4。
        "ldr x16, =watchpoint_handler\n"
        "ldr x16, [x16]\n"
        "add x16, x16, #4\n"
        "br x16\n"
        :
        :
        : "memory");
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

    //防止重复安装，下面删除hook时会清空来判断是否重新安装
    if (breakpoint_handler || watchpoint_handler)
    {
        pr_debug("[driver] hook install skipped, already installed\n");
        return 0;
    }

    breakpoint_handler = generic_kallsyms_lookup_name("breakpoint_handler");
    if (!breakpoint_handler)
    {
        pr_debug("[driver] cannot find symbol: breakpoint_handler\n");
        return -ENOENT;
    }
    watchpoint_handler = generic_kallsyms_lookup_name("watchpoint_handler");
    if (!watchpoint_handler)
    {
        pr_debug("[driver] cannot find symbol: watchpoint_handler\n");
        return -ENOENT;
    }

    // 安装 breakpoint_handler 的 inline hook
    ret = hook_one(breakpoint_handler, (unsigned long)trampoline_breakpoint, &orig_bp_insn);
    if (ret)
    {
        pr_debug("[driver] hook breakpoint_handler failed: %d\n", ret);
        return ret;
    }
    // 安装 watchpoint_handler 的 inline hook
    ret = hook_one(watchpoint_handler, (unsigned long)trampoline_watchpoint, &orig_wp_insn);
    if (ret)
    {
        pr_debug("[driver] hook watchpoint_handler failed: %d\n", ret);
        unhook_one(breakpoint_handler, orig_bp_insn);
        breakpoint_handler = 0;
        watchpoint_handler = 0;
        return ret;
    }
    //  把 trampoline 里的 nop 指令改成原始指令,执行因hook被修改的第一条指令
    fn_aarch64_insn_patch_text_nosync((void *)bp_orig_insn_slot, orig_bp_insn);
    fn_aarch64_insn_patch_text_nosync((void *)wp_orig_insn_slot, orig_wp_insn);

    pr_debug("[driver] hook installed\n");
    return 0;
}
void hw_breakpoint_hook_remove(void)
{
    if (!breakpoint_handler && !watchpoint_handler)
    {
        pr_debug("[driver] hook remove skipped, not installed\n");
        return;
    }

    // 还原 watchpoint_handler 原始指令
    if (watchpoint_handler)
        unhook_one(watchpoint_handler, orig_wp_insn);

    // 还原 breakpoint_handler 原始指令
    if (breakpoint_handler)
        unhook_one(breakpoint_handler, orig_bp_insn);

    breakpoint_handler = 0;
    watchpoint_handler = 0;
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
// encode_ctrl_reg是控制码转ARM架构内部格式

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
            // 执行地址寄存器
            write_wb_reg(AARCH64_DBG_REG_BVR, 5, info.address);
            // 执行控制寄存器
            //"| 0x1"表示立即生效,
            //"& ~0x1"表示写入的寄存器配置，但是禁用不生效
            //"0"给控制寄存器请0，就删除了断点
            write_wb_reg(AARCH64_DBG_REG_BCR, 5, encode_ctrl_reg(info.ctrl) | 0x1);
            // write_wb_reg(AARCH64_DBG_REG_BCR, 5, encode_ctrl_reg(info.ctrl) & ~0x1);
        }
        else
        {
            // 访问地址寄存器
            write_wb_reg(AARCH64_DBG_REG_WVR, 3, info.address);
            // 访问控制寄存器
            //"| 0x1"表示立即生效,
            //"& ~0x1"表示写入的寄存器配置，但是禁用不生效
            //"0"给控制寄存器请0就删除了断点
            write_wb_reg(AARCH64_DBG_REG_WCR, 3, encode_ctrl_reg(info.ctrl) | 0x1);
            // write_wb_reg(AARCH64_DBG_REG_WCR, 3, encode_ctrl_reg(info.ctrl) & ~0x1);
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

        // 请0执行控制寄存器和访问控制寄存器
        write_wb_reg(AARCH64_DBG_REG_BCR, 5, 0);
        write_wb_reg(AARCH64_DBG_REG_WCR, 3, 0);

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
    if (g_bp_config)//已有配置进行了注册，防止重复注册
    {
        g_bp_config = bp_config;
        pr_debug("monitor already running, update target tgid=%d\n", g_bp_config->pid);
        return 0;
    }

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
    if (!g_bp_config)
    {
        pr_debug("monitor stop skipped, not running\n");
        return;
    }

    unregister_trace_sched_switch(probe_sched_switch, NULL);
    pr_debug("monitor stop, target tgid=%d\n", g_bp_config->pid);
    g_bp_config = NULL;
}

// // 单步异常步过的内核api，我不使用了，上面异常回调直接关寄存器
// static struct step_hook hwbp_step_hook;
// static void (*fn_user_enable_single_step)(struct task_struct *task);
// static void (*fn_user_disable_single_step)(struct task_struct *task);
// static void (*fn_register_user_step_hook)(struct step_hook *hook);
// static void (*fn_unregister_user_step_hook)(struct step_hook *hook);

// // 命中断点后临时关闭当前控制寄存器，并开启单步执行一条指令
// static void hwbp_begin_step(struct breakpoint_config *cfg, int type, struct pt_regs *regs)
// {
//     u32 ctrl;

//     // 只处理用户态命中
//     if (!cfg || !user_mode(regs))
//         return;

//     if (type == 1)
//     {
//         // 清空控制寄存器
//         ctrl = read_wb_reg(AARCH64_DBG_REG_BCR, 0);
//         write_wb_reg(AARCH64_DBG_REG_BCR, 0, 0);
//     }
//     else
//     {
//         // 清空控制寄存器
//         ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, 0);
//         write_wb_reg(AARCH64_DBG_REG_WCR, 0, 0);
//     }

//     // 记录恢复所需状态，single-step 异常回来时使用
//     cfg->suspended_step = 1;
//     cfg->suspended_type = type;
//     cfg->suspended_ctrl = ctrl;
//     cfg->suspended_task = current;

//     // 开启用户态单步，返回用户态执行一条指令后会进入 hwbp_user_step_handler
//     fn_user_enable_single_step(current);
// }

// // 单步异常回调：恢复刚才临时关闭的 BCR/WCR
// static int hwbp_user_step_handler(struct pt_regs *regs, unsigned long esr)
// {
//     struct breakpoint_config *cfg = g_bp_config;

//     (void)esr;

//     // 不是我们发起的单步就交给系统原有处理链
//     if (!cfg || !cfg->suspended_step || cfg->suspended_task != current)
//         return DBG_HOOK_ERROR;

//     if (cfg->suspended_type == 1)
//         // 恢复执行断点
//         write_wb_reg(AARCH64_DBG_REG_BCR, 0, cfg->suspended_ctrl);
//     else if (cfg->suspended_type == 2)
//         // 恢复访问断点
//         write_wb_reg(AARCH64_DBG_REG_WCR, 0, cfg->suspended_ctrl);

//     // 清理步过状态
//     cfg->suspended_step = 0;
//     cfg->suspended_type = 0;
//     cfg->suspended_ctrl = 0;
//     cfg->suspended_task = NULL;

//     // 关闭本次单步，告诉异常分发器这次 single-step 已经处理
//     fn_user_disable_single_step(current);
//     return DBG_HOOK_HANDLED;
// }

// // 安装用户态 single-step hook，用来在步过后恢复断点
// static int hwbp_step_hook_install(void)
// {
//     // user_enable_single_step:返回用户态后，执行下一条指令之后触发一次硬件单步异常。
//     fn_user_enable_single_step = (void *)generic_kallsyms_lookup_name("user_enable_single_step");
//     fn_user_disable_single_step = (void *)generic_kallsyms_lookup_name("user_disable_single_step");
//     fn_register_user_step_hook = (void *)generic_kallsyms_lookup_name("register_user_step_hook");
//     fn_unregister_user_step_hook = (void *)generic_kallsyms_lookup_name("unregister_user_step_hook");

//     if (!fn_user_enable_single_step || !fn_user_disable_single_step ||
//         !fn_register_user_step_hook || !fn_unregister_user_step_hook)
//     {
//         pr_debug("[driver] cannot find single-step symbols\n");
//         return -ENOENT;
//     }

//     // 注册到 arm64 debug-monitors 的用户态单步回调链
//     memset(&hwbp_step_hook, 0, sizeof(hwbp_step_hook));
//     hwbp_step_hook.fn = (void *)hwbp_user_step_handler;
//     fn_register_user_step_hook(&hwbp_step_hook);

//     return 0;
// }
// static void hwbp_step_hook_remove(void)
// {
//     fn_unregister_user_step_hook(&hwbp_step_hook);
// }
