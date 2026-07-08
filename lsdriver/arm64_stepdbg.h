#ifndef ARM64_STEPDBG_H
#define ARM64_STEPDBG_H

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task_stack.h>
#include <linux/spinlock.h>
#include <linux/thread_info.h>
#include <asm/debug-monitors.h>
#include <asm/ptrace.h>
#include <asm/thread_info.h>

#include "inline_hook_frame.h"
#include "lsdriver_log.h"

static struct break_point *g_stepbp_info;
static DEFINE_SPINLOCK(g_stepbp_lock);
static DEFINE_MUTEX(g_stepbp_mutex);
static pid_t g_stepbp_active_pid;
static bool g_stepbp_stopping;

#define STEPBP_LOG_LIMITED(counter, limit, fmt, ...)  \
    do                                                \
    {                                                 \
        if (atomic_inc_return(&(counter)) <= (limit)) \
            ls_log_tag("stepbp", fmt, ##__VA_ARGS__); \
    } while (0)

static atomic_t g_stepbp_log_enable = ATOMIC_INIT(0);
static atomic_t g_stepbp_log_switch = ATOMIC_INIT(0);
static atomic_t g_stepbp_log_syscall = ATOMIC_INIT(0);
static atomic_t g_stepbp_log_rseq = ATOMIC_INIT(0);
static atomic_t g_stepbp_log_hit = ATOMIC_INIT(0);

static bool stepbp_rseq_hook_installed(void);

static inline void stepbp_debug_reset(void)
{
    atomic_set(&g_stepbp_log_enable, 0);
    atomic_set(&g_stepbp_log_switch, 0);
    atomic_set(&g_stepbp_log_syscall, 0);
    atomic_set(&g_stepbp_log_rseq, 0);
    atomic_set(&g_stepbp_log_hit, 0);
}

// 判断指定 task 是否属于目标 pid/tgid。
static inline bool stepbp_task_matches(struct task_struct *task, pid_t target_pid)
{
    return task && target_pid > 0 && (target_pid == task->tgid || target_pid == task->pid);
}

// 判断配置点是否是有效的 STEPBP 执行断点。
static inline bool stepbp_point_is_active(struct bp_point *point)
{
    return point && point->hit_addr != 0 && (point->bt & BP_BREAKPOINT_X);
}

// 检查断点配置中是否存在至少一个可安装的 STEPBP 点。
static inline bool stepbp_info_has_active_point(struct break_point *info)
{
    int point_slot;

    if (!info || info->pid <= 0)
        return false;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (stepbp_point_is_active(&info->points[point_slot]))
            return true;
    }

    return false;
}

static inline bool stepbp_monitor_active(void)
{
    return READ_ONCE(g_stepbp_info) &&
           READ_ONCE(g_stepbp_active_pid) > 0 &&
           !READ_ONCE(g_stepbp_stopping);
}

static inline void stepbp_publish_monitor(struct break_point *info, bool stopping)
{
    WRITE_ONCE(g_stepbp_info, info);
    WRITE_ONCE(g_stepbp_active_pid, info ? info->pid : 0);
    WRITE_ONCE(g_stepbp_stopping, stopping);
}

// 判断当前线程是否属于目标 pid/tgid。
static inline bool stepbp_current_task_matches(pid_t target_pid)
{
    return stepbp_task_matches(current, target_pid);
}

// 判断单步异常现场 PC 是否命中配置地址，按 ARM64 指令对齐比较。
// 注意：ARM64 single-step 异常通常发生在一条指令执行完成后，regs->pc 可能已经指向下一条指令。
// 如果现场测试仍然 0 命中，下一步应把 regs->pc - 4 也纳入匹配范围。
static inline bool stepbp_addr_matches(struct bp_point *point, uint64_t pc)
{
    if (!point || !point->hit_addr)
        return false;

    return (point->hit_addr & ~0x3ULL) == (pc & ~0x3ULL);
}

// 设置返回现场的 SPSR.SS 位，配合 ret_to_user 中的 TIF_SINGLESTEP 打开 MDSCR.SS。
static inline void stepbp_set_regs_single_step(struct pt_regs *regs)
{
    if (regs)
        regs->pstate |= DBG_SPSR_SS;
}

// 清理返回现场的 SPSR.SS 位，停用后不再续发单步异常。
static inline void stepbp_clear_regs_single_step(struct pt_regs *regs)
{
    if (regs)
        regs->pstate &= ~DBG_SPSR_SS;
}

static inline void stepbp_enable_task_single_step(struct task_struct *task)
{
    if (!task)
        return;

    // TIF_SINGLESTEP 是线程级状态，必须对每个目标 task 单独设置。
    // ret_to_user 看到该 flag 后会打开当前 CPU 的 MDSCR.SS。
    set_ti_thread_flag(task_thread_info(task), TIF_SINGLESTEP);
    stepbp_set_regs_single_step(task_pt_regs(task));
}

static inline void stepbp_disable_task_single_step(struct task_struct *task)
{
    if (!task)
        return;

    clear_ti_thread_flag(task_thread_info(task), TIF_SINGLESTEP);
    stepbp_clear_regs_single_step(task_pt_regs(task));
}

static inline void stepbp_apply_task_single_step(struct task_struct *task, bool enable)
{
    if (enable)
        stepbp_enable_task_single_step(task);
    else
        stepbp_disable_task_single_step(task);
}

static int stepbp_scan_pid_tasks_locked(pid_t target_pid, bool enable)
{
    struct task_struct *process;
    struct task_struct *task;
    int touched_count = 0;

    for_each_process_thread(process, task)
    {
        if (!stepbp_task_matches(task, target_pid))
            continue;

        stepbp_apply_task_single_step(task, enable);
        touched_count++;
    }

    return touched_count;
}

static int stepbp_apply_pid_tasks(pid_t target_pid, bool enable)
{
    struct task_struct *target_task;
    struct task_struct *task;
    int touched_count = 0;

    if (target_pid <= 0)
        return 0;

    rcu_read_lock();
    target_task = find_task_by_vpid(target_pid);
    if (!target_task)
    {
        touched_count = stepbp_scan_pid_tasks_locked(target_pid, enable);
        goto out_unlock;
    }

    if (target_task->tgid == target_pid)
    {
        for_each_thread(target_task, task)
        {
            stepbp_apply_task_single_step(task, enable);
            touched_count++;
        }
    }
    else
    {
        stepbp_apply_task_single_step(target_task, enable);
        touched_count = 1;
    }

out_unlock:
    rcu_read_unlock();
    return touched_count;
}

// 给目标进程下现有线程设置单步返回现场。
static void stepbp_enable_pid_tasks(pid_t target_pid)
{
    int armed_count = stepbp_apply_pid_tasks(target_pid, true);

    STEPBP_LOG_LIMITED(g_stepbp_log_enable, 2,
                       "enable pid=%d armed_tasks=%d current pid=%d tgid=%d comm=%s\n",
                       target_pid, armed_count, current->pid, current->tgid, current->comm);
}

// 清理目标进程线程保存现场中的单步状态。
static void stepbp_disable_pid_tasks(pid_t target_pid)
{
    stepbp_apply_pid_tasks(target_pid, false);
}

// syscall_trace_exit() 会在 _TIF_SINGLESTEP 下调用 report_syscall(PTRACE_SYSCALL_EXIT)，
// 对用户态表现为 ptrace pseudo-step SIGTRAP。STEPBP 的 TIF_SINGLESTEP 只用于驱动
// ret_to_user 打开 MDSCR.SS，不应触发这条 ptrace syscall-exit 语义。
// 有 rseq_syscall hook 时：入口临时清 flag，rseq_syscall 阶段补回。
// 没有 rseq_syscall 符号时：直接跳过原 syscall_trace_exit，保留 TIF_SINGLESTEP 给 ret_to_user。
static int work_trampoline_stepbp_syscall_trace_exit(struct pt_regs *hook_regs)
{
    struct pt_regs *regs;
    pid_t target_pid;

    if (!hook_regs)
        return 0;

    target_pid = READ_ONCE(g_stepbp_active_pid);
    if (!stepbp_monitor_active() || !stepbp_current_task_matches(target_pid))
        return 0;

    if (!test_thread_flag(TIF_SINGLESTEP))
        return 0;

    regs = (struct pt_regs *)hook_regs->regs[0];
    if (!regs || !user_mode(regs))
        return 0;

    if (!stepbp_rseq_hook_installed())
    {
        STEPBP_LOG_LIMITED(g_stepbp_log_syscall, 2,
                           "syscall_exit fallback no_rseq pid=%d tgid=%d\n",
                           current->pid, current->tgid);
        stepbp_set_regs_single_step(regs);
        return 1;
    }

    STEPBP_LOG_LIMITED(g_stepbp_log_syscall, 2,
                       "syscall_exit clear single-step pid=%d tgid=%d\n",
                       current->pid, current->tgid);

    stepbp_set_regs_single_step(regs);
    clear_thread_flag(TIF_SINGLESTEP);
    return 0;
}

// syscall_trace_exit() 尾部固定调用 rseq_syscall(regs)，位置在 report_syscall() 之后、
// ret_to_user 之前。这里看到 “TIF_SINGLESTEP 已被临时清掉但 SPSR.SS 仍在” 时，
// 补回 TIF_SINGLESTEP + SPSR.SS，让 ret_to_user 继续按内核原生路径打开 MDSCR.SS。
static int work_trampoline_stepbp_rseq_syscall(struct pt_regs *hook_regs)
{
    struct pt_regs *regs;
    pid_t target_pid;

    if (!hook_regs)
        return 0;

    target_pid = READ_ONCE(g_stepbp_active_pid);
    if (!stepbp_monitor_active() || !stepbp_current_task_matches(target_pid))
        return 0;

    regs = (struct pt_regs *)hook_regs->regs[0];
    if (!regs || !user_mode(regs))
        return 0;

    if (test_thread_flag(TIF_SINGLESTEP) || !(regs->pstate & DBG_SPSR_SS))
        return 0;

    STEPBP_LOG_LIMITED(g_stepbp_log_rseq, 2,
                       "rseq restore single-step pid=%d tgid=%d\n",
                       current->pid, current->tgid);

    stepbp_enable_task_single_step(current);
    return 0;
}

// __switch_to 入口补 arm：覆盖安装后才创建/切入的目标线程。
static int work_trampoline_stepbp_switch(struct pt_regs *hook_regs)
{
    struct task_struct *next;
    pid_t target_pid;

    if (!hook_regs)
        return 0;

    next = (struct task_struct *)hook_regs->regs[1];
    target_pid = READ_ONCE(g_stepbp_active_pid);

    if (stepbp_monitor_active() && stepbp_task_matches(next, target_pid))
    {
        stepbp_enable_task_single_step(next);
        STEPBP_LOG_LIMITED(g_stepbp_log_switch, 4,
                           "switch arm target=%d next pid=%d tgid=%d comm=%s\n",
                           target_pid, next->pid, next->tgid, next->comm);
    }

    return 0;
}

// single_step_handler 入口 hook：目标进程的 EL0 硬件单步异常由 STEPBP 接管。
static int work_trampoline_stepbp_single_step(struct pt_regs *hook_regs)
{
    int point_slot;
    int hit_slot = -1;
    unsigned long flags;
    struct break_point *info;
    struct bp_point *hit_point = NULL;
    bool target_task;
    struct pt_regs *regs;

    if (!hook_regs)
        return 0;

    regs = (struct pt_regs *)hook_regs->regs[2];
    if (!regs)
        return 0;

    // user_mode() 判断异常现场是否来自 EL0；STEPBP 只接管用户态单步，不碰 EL1 内核态异常。
    if (!user_mode(regs))
        return 0;

    // spin_lock_irqsave() 保护 g_stepbp_info，且适合异常上下文，避免本 CPU 中断打断后重入同一把锁。
    spin_lock_irqsave(&g_stepbp_lock, flags);
    info = g_stepbp_info;
    target_task = info && !g_stepbp_stopping && stepbp_current_task_matches(info->pid);
    if (target_task)
    {
        for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
        {
            struct bp_point *point = &info->points[point_slot];

            if (!stepbp_point_is_active(point))
                continue;

            if (!stepbp_addr_matches(point, regs->pc))
                continue;

            hit_point = point;
            hit_slot = point_slot;
            break;
        }
    }
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    if (!target_task)
        return 0;

    clear_thread_flag(TIF_SINGLESTEP);

    if (hit_point && hit_point->on_hit)
    {
        STEPBP_LOG_LIMITED(g_stepbp_log_hit, 8,
                           "hit slot=%d pid=%d tgid=%d pc=0x%llx hit_addr=0x%llx record_count=%u\n",
                           hit_slot, current->pid, current->tgid,
                           (unsigned long long)regs->pc,
                           (unsigned long long)hit_point->hit_addr,
                           hit_point->record_count);
        hit_point->on_hit((void *)regs, (void *)hit_point);
    }

    // 跳过原 single_step_handler，由 STEPBP 维护下一发用户态单步。
    if (!READ_ONCE(g_stepbp_stopping))
        stepbp_enable_task_single_step(current);
    hook_regs->regs[0] = 0;
    return 1;
}

static struct hook_entry g_stepbp_required_hooks[] = {
    HOOK_ENTRY("single_step_handler", work_trampoline_stepbp_single_step),
    HOOK_ENTRY("syscall_trace_exit", work_trampoline_stepbp_syscall_trace_exit),
};

static struct hook_entry g_stepbp_rseq_hook[] = {
    HOOK_ENTRY("rseq_syscall", work_trampoline_stepbp_rseq_syscall),
};

static struct hook_entry g_stepbp_switch_hook[] = {
    HOOK_ENTRY("__switch_to", work_trampoline_stepbp_switch),
};

static bool stepbp_rseq_hook_installed(void)
{
    return READ_ONCE(g_stepbp_rseq_hook[0].installed);
}

static void stepbp_dump_hook_symbols(void)
{
    int i;

    ls_log_tag("stepbp", "patch_text=0x%llx\n", (unsigned long long)fn_aarch64_insn_patch_text);
    for (i = 0; i < (int)(sizeof(g_stepbp_required_hooks) / sizeof(g_stepbp_required_hooks[0])); i++)
    {
        unsigned long addr = generic_kallsyms_lookup_name(g_stepbp_required_hooks[i].target_sym);
        ls_log_tag("stepbp", "required symbol %s=0x%lx\n", g_stepbp_required_hooks[i].target_sym, addr);
    }

    for (i = 0; i < (int)(sizeof(g_stepbp_rseq_hook) / sizeof(g_stepbp_rseq_hook[0])); i++)
    {
        unsigned long addr = generic_kallsyms_lookup_name(g_stepbp_rseq_hook[i].target_sym);
        ls_log_tag("stepbp", "optional symbol %s=0x%lx\n", g_stepbp_rseq_hook[i].target_sym, addr);
    }

    for (i = 0; i < (int)(sizeof(g_stepbp_switch_hook) / sizeof(g_stepbp_switch_hook[0])); i++)
    {
        unsigned long addr = generic_kallsyms_lookup_name(g_stepbp_switch_hook[i].target_sym);
        ls_log_tag("stepbp", "optional symbol %s=0x%lx\n", g_stepbp_switch_hook[i].target_sym, addr);
    }
}

static void stepbp_install_optional_rseq_hook(void)
{
    int status;

    status = inline_hook_install_count(g_stepbp_rseq_hook, sizeof(g_stepbp_rseq_hook) / sizeof(g_stepbp_rseq_hook[0]));
    if (status)
    {
        ls_log_tag("stepbp", "optional rseq hook skipped status=%d target=0x%llx\n",
                   status,
                   (unsigned long long)g_stepbp_rseq_hook[0].target_addr);
        return;
    }

    ls_log_tag("stepbp", "optional rseq hook ok target=0x%llx\n", (unsigned long long)g_stepbp_rseq_hook[0].target_addr);
}
static int stepbp_install_required_hooks(void)
{
    int i;
    int ret;
    int count = sizeof(g_stepbp_required_hooks) / sizeof(g_stepbp_required_hooks[0]);

    for (i = 0; i < count; i++)
    {
        ret = hook_entry_install(&g_stepbp_required_hooks[i]);
        if (ret)
        {
            ls_log_tag("stepbp", "required hook failed index=%d symbol=%s status=%d target=0x%llx patch_text=0x%llx\n",
                       i,
                       g_stepbp_required_hooks[i].target_sym,
                       ret,
                       (unsigned long long)g_stepbp_required_hooks[i].target_addr,
                       (unsigned long long)fn_aarch64_insn_patch_text);
            while (--i >= 0)
                hook_entry_remove(&g_stepbp_required_hooks[i]);
            return ret;
        }

        ls_log_tag("stepbp", "required hook ok index=%d symbol=%s target=0x%llx\n",
                   i,
                   g_stepbp_required_hooks[i].target_sym,
                   (unsigned long long)g_stepbp_required_hooks[i].target_addr);
    }

    return 0;
}

static void stepbp_remove_required_hooks(void)
{
    int i;

    for (i = (int)(sizeof(g_stepbp_required_hooks) / sizeof(g_stepbp_required_hooks[0])) - 1; i >= 0; i--)
        hook_entry_remove(&g_stepbp_required_hooks[i]);
}

static void stepbp_install_optional_switch_hook(void)
{
    int status;

    status = inline_hook_install_count(g_stepbp_switch_hook, sizeof(g_stepbp_switch_hook) / sizeof(g_stepbp_switch_hook[0]));
    if (status)
    {
        ls_log_tag("stepbp", "optional switch hook skipped status=%d target=0x%llx\n",
                   status,
                   (unsigned long long)g_stepbp_switch_hook[0].target_addr);
        return;
    }

    ls_log_tag("stepbp", "optional switch hook ok target=0x%llx\n", (unsigned long long)g_stepbp_switch_hook[0].target_addr);
}

// 停止 STEPBP：清理目标线程保存现场，再移除 hook。
static inline void stop_stepbp_monitor(void)
{
    pid_t target_pid = 0;
    unsigned long flags;

    // mutex 串行化安装/卸载，避免并发替换 hook 和全局配置。
    mutex_lock(&g_stepbp_mutex);

    spin_lock_irqsave(&g_stepbp_lock, flags);
    if (g_stepbp_info)
        target_pid = g_stepbp_info->pid;
    else
        target_pid = g_stepbp_active_pid;
    stepbp_publish_monitor(g_stepbp_info, true);
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    if (target_pid > 0)
        stepbp_disable_pid_tasks(target_pid);

    spin_lock_irqsave(&g_stepbp_lock, flags);
    stepbp_publish_monitor(NULL, true);
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    inline_hook_remove_count(g_stepbp_switch_hook, sizeof(g_stepbp_switch_hook) / sizeof(g_stepbp_switch_hook[0]));
    inline_hook_remove_count(g_stepbp_rseq_hook, sizeof(g_stepbp_rseq_hook) / sizeof(g_stepbp_rseq_hook[0]));
    stepbp_remove_required_hooks();

    stepbp_publish_monitor(NULL, false);
    mutex_unlock(&g_stepbp_mutex);
}

// 安装 STEPBP：校验配置、hook single_step_handler，并启用目标线程单步。
static inline int start_stepbp_monitor(struct break_point *info)
{
    int status;
    unsigned long flags;

    if (!stepbp_info_has_active_point(info))
    {
        ls_log_tag("stepbp", "start rejected pid=%d no active execute point\n", info ? info->pid : -1);
        return -EINVAL;
    }

    stop_stepbp_monitor();
    stepbp_debug_reset();
    stepbp_dump_hook_symbols();

    status = stepbp_install_required_hooks();
    if (status)
    {
        ls_log_tag("stepbp", "hook install failed pid=%d status=%d\n", info->pid, status);
        return status;
    }

    stepbp_install_optional_rseq_hook();
    stepbp_install_optional_switch_hook();

    spin_lock_irqsave(&g_stepbp_lock, flags);
    stepbp_publish_monitor(info, false);
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    stepbp_enable_pid_tasks(info->pid);

    ls_log_tag("stepbp", "start ok pid=%d first_addr=0x%llx bt=0x%x bs=0x%x\n",
               info->pid,
               (unsigned long long)info->points[0].hit_addr,
               info->points[0].bt,
               info->points[0].bs);

    return 0;
}

#endif // ARM64_STEPDBG_H