/*
监控指定进程对 CNTVCT_EL0 的读取
*/
#ifndef ARM64_CNTVCT_MONITOR_H
#define ARM64_CNTVCT_MONITOR_H

#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/smp.h>
#include <clocksource/arm_arch_timer.h>
#include <asm/esr.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>

#include "export_fun.h"
#include "inline_hook_frame.h"
#include "lsdriver_log.h"

struct cntvct_monitor_cpu_state
{
    bool armed;
    bool access_was_enabled;
};

#define CNTVCT_MONITOR_USER_STACK_DEPTH 8

struct cntvct_monitor_user_frame
{
    unsigned long fp;
    unsigned long lr;
};

static DEFINE_MUTEX(g_cntvct_monitor_mutex);
static DEFINE_PER_CPU(struct cntvct_monitor_cpu_state, g_cntvct_monitor_cpu_state);
static pid_t g_cntvct_monitor_tgid;

// 沿用户态 x29 帧链输出有限深度的返回地址，读取失败时立即停止且不触发缺页。
static void cntvct_monitor_log_user_callchain(const struct pt_regs *regs)
{
    if (!regs || !current->mm) return;

    unsigned long task_size = READ_ONCE(current->mm->task_size);
    if (!task_size) return;

    char text[384];
    size_t pos = 0;
    unsigned long address_mask = task_size - 1;
    unsigned long frame_pointer = regs->regs[29] & address_mask;
    unsigned long pc = regs->pc & address_mask;
    unsigned long lr = regs->regs[30] & address_mask;

    pos += scnprintf(text + pos, sizeof(text) - pos, "pc=0x%lx lr=0x%lx", pc, lr);
    for (unsigned int depth = 0; depth < CNTVCT_MONITOR_USER_STACK_DEPTH; depth++)
    {
        struct cntvct_monitor_user_frame frame;

        if (!frame_pointer || (frame_pointer & 0xf) || frame_pointer >= task_size || task_size - frame_pointer < sizeof(frame)) break;
        if (copy_from_user_inatomic_nofault(&frame, (const void __user *)(uintptr_t)frame_pointer, sizeof(frame))) break;

        unsigned long return_address = frame.lr & address_mask;
        pos += scnprintf(text + pos, sizeof(text) - pos, " #%u=0x%lx", depth, return_address);

        unsigned long previous_frame_pointer = frame.fp & address_mask;
        if (previous_frame_pointer <= frame_pointer) break;
        frame_pointer = previous_frame_pointer;
    }

    ls_log_always_tag("cntvct", "tgid=%d pid=%d callchain %s\n", current->tgid, current->pid, text);
}

// 恢复当前 CPU 在启用监控前的用户态计数器访问权限
static __always_inline void cntvct_monitor_restore_cpu(void)
{
    struct cntvct_monitor_cpu_state *state = this_cpu_ptr(&g_cntvct_monitor_cpu_state);

    if (!state->armed) return;

    unsigned long cntkctl = read_sysreg(cntkctl_el1);
    unsigned long restored_cntkctl = state->access_was_enabled ? cntkctl | ARCH_TIMER_USR_VCT_ACCESS_EN : cntkctl & ~ARCH_TIMER_USR_VCT_ACCESS_EN;
    if (restored_cntkctl != cntkctl)
    {
        write_sysreg(restored_cntkctl, cntkctl_el1);
        isb();
    }
    state->armed = false;
}

// 保存当前 CPU 的原始权限并关闭用户态 CNTVCT_EL0 直接访问
static __always_inline void cntvct_monitor_arm_cpu(void)
{
    struct cntvct_monitor_cpu_state *state = this_cpu_ptr(&g_cntvct_monitor_cpu_state);

    unsigned long cntkctl = read_sysreg(cntkctl_el1);
    if (!state->armed)
    {
        state->access_was_enabled = !!(cntkctl & ARCH_TIMER_USR_VCT_ACCESS_EN);
        state->armed = true;
    }
    else if (cntkctl & ARCH_TIMER_USR_VCT_ACCESS_EN)
    {
        state->access_was_enabled = true;
    }
    if (cntkctl & ARCH_TIMER_USR_VCT_ACCESS_EN)
    {
        write_sysreg(cntkctl & ~ARCH_TIMER_USR_VCT_ACCESS_EN, cntkctl_el1);
        isb();
    }
}

// 根据当前运行任务刷新当前 CPU 的监控状态
static void cntvct_monitor_update_current_cpu(void *unused)
{
    pid_t target_tgid = READ_ONCE(g_cntvct_monitor_tgid);

    (void)unused;
    cntvct_monitor_restore_cpu();
    if (target_tgid > 0 && current->tgid == target_tgid) cntvct_monitor_arm_cpu();
}

// 同步刷新所有在线 CPU 的监控状态
static int cntvct_monitor_update_online_cpus(void)
{
    int cpu;
    int status = 0;

    cpus_read_lock();
    for_each_online_cpu(cpu)
    {
        int cpu_status = smp_call_function_single(cpu, cntvct_monitor_update_current_cpu, NULL, 1);
        if (status == 0 && cpu_status < 0) status = cpu_status;
    }
    cpus_read_unlock();
    return status;
}

/*
hook cpu_switch_to(prev, next) 时 x1 仍然是 next，且内核对 next 的 CNTKCTL_EL1
更新已经完成。ARM erratum 1418040 下，恢复旧值可能重新开放 32 位任务的
EL0VCTEN；最小实现不额外查询 CPU capability，接受此限制。
*/
// 在任务切换时恢复旧任务权限并为目标任务启用读取监控
static int cntvct_monitor_switch_hook_work(struct pt_regs *hook_regs)
{
    if (!hook_regs) return 0;

    struct task_struct *next = (struct task_struct *)(uintptr_t)hook_regs->regs[1];
    pid_t target_tgid = READ_ONCE(g_cntvct_monitor_tgid);
    cntvct_monitor_restore_cpu();
    if (target_tgid > 0 && next && next->tgid == target_tgid) cntvct_monitor_arm_cpu();
    return 0;
}

// 记录目标进程触发的 CNTVCT_EL0 读取位置
static int cntvct_monitor_read_hook_work(struct pt_regs *hook_regs)
{
    if (!hook_regs) return 0;

    pid_t target_tgid = READ_ONCE(g_cntvct_monitor_tgid);
    if (target_tgid <= 0 || current->tgid != target_tgid) return 0;

    unsigned long esr = hook_regs->regs[0];
    if ((esr & ESR_ELx_SYS64_ISS_SYS_OP_MASK) != ESR_ELx_SYS64_ISS_SYS_CNTVCT) return 0;

    struct pt_regs *regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[1];
    if (!regs || !user_mode(regs)) return 0;

    unsigned int rt = ESR_ELx_SYS64_ISS_RT(esr);
    ls_log_always_tag("cntvct", "tgid=%d pid=%d comm=%s esr=0x%lx rt=%u pc=0x%llx lr=0x%llx sp=0x%llx fp=0x%llx pstate=0x%llx\n", current->tgid, current->pid, current->comm, esr, rt, (unsigned long long)regs->pc, (unsigned long long)regs->regs[30], (unsigned long long)regs->sp, (unsigned long long)regs->regs[29], (unsigned long long)regs->pstate);
    cntvct_monitor_log_user_callchain(regs);
    ls_log_always_tag("cntvct", "tgid=%d pid=%d pc=0x%llx x0=%016llx x1=%016llx x2=%016llx x3=%016llx x4=%016llx\n", current->tgid, current->pid, (unsigned long long)regs->pc, (unsigned long long)regs->regs[0], (unsigned long long)regs->regs[1], (unsigned long long)regs->regs[2], (unsigned long long)regs->regs[3], (unsigned long long)regs->regs[4]);
    ls_log_always_tag("cntvct", "tgid=%d pid=%d pc=0x%llx x5=%016llx x6=%016llx x7=%016llx x8=%016llx x9=%016llx\n", current->tgid, current->pid, (unsigned long long)regs->pc, (unsigned long long)regs->regs[5], (unsigned long long)regs->regs[6], (unsigned long long)regs->regs[7], (unsigned long long)regs->regs[8], (unsigned long long)regs->regs[9]);
    ls_log_always_tag("cntvct", "tgid=%d pid=%d pc=0x%llx x10=%016llx x11=%016llx x12=%016llx x13=%016llx x14=%016llx\n", current->tgid, current->pid, (unsigned long long)regs->pc, (unsigned long long)regs->regs[10], (unsigned long long)regs->regs[11], (unsigned long long)regs->regs[12], (unsigned long long)regs->regs[13], (unsigned long long)regs->regs[14]);
    ls_log_always_tag("cntvct", "tgid=%d pid=%d pc=0x%llx x15=%016llx x16=%016llx x17=%016llx x18=%016llx x19=%016llx\n", current->tgid, current->pid, (unsigned long long)regs->pc, (unsigned long long)regs->regs[15], (unsigned long long)regs->regs[16], (unsigned long long)regs->regs[17], (unsigned long long)regs->regs[18], (unsigned long long)regs->regs[19]);
    ls_log_always_tag("cntvct", "tgid=%d pid=%d pc=0x%llx x20=%016llx x21=%016llx x22=%016llx x23=%016llx x24=%016llx\n", current->tgid, current->pid, (unsigned long long)regs->pc, (unsigned long long)regs->regs[20], (unsigned long long)regs->regs[21], (unsigned long long)regs->regs[22], (unsigned long long)regs->regs[23], (unsigned long long)regs->regs[24]);
    ls_log_always_tag("cntvct", "tgid=%d pid=%d pc=0x%llx x25=%016llx x26=%016llx x27=%016llx x28=%016llx x29=%016llx x30=%016llx\n", current->tgid, current->pid, (unsigned long long)regs->pc, (unsigned long long)regs->regs[25], (unsigned long long)regs->regs[26], (unsigned long long)regs->regs[27], (unsigned long long)regs->regs[28], (unsigned long long)regs->regs[29], (unsigned long long)regs->regs[30]);
    return 0;
}

static struct hook_entry g_cntvct_monitor_hooks[] = {
    HOOK_ENTRY("cntvct_read_handler", cntvct_monitor_read_hook_work),
    HOOK_ENTRY("cpu_switch_to", cntvct_monitor_switch_hook_work),
};

// 在持有监控互斥锁时停止监控并恢复所有 CPU 状态
static void cntvct_monitor_stop_locked(void)
{
    pid_t target_tgid = READ_ONCE(g_cntvct_monitor_tgid);
    int cpu;

    if (!g_cntvct_monitor_hooks[0].installed && !g_cntvct_monitor_hooks[1].installed && target_tgid <= 0) return;

    WRITE_ONCE(g_cntvct_monitor_tgid, 0);

    cntvct_monitor_update_online_cpus();
    inline_hook_remove(g_cntvct_monitor_hooks);
    for_each_possible_cpu(cpu) per_cpu(g_cntvct_monitor_cpu_state, cpu).armed = false;
    if (target_tgid > 0) ls_log_always_tag("cntvct", "stop tgid=%d\n", target_tgid);
}

// 安装两个内联钩子并开始监控指定线程组
static int cntvct_monitor_install(pid_t target_tgid)
{
    if (target_tgid <= 0) return -EINVAL;

    struct task_struct *task = get_task_by_pid(target_tgid);
    if (!task) return -ESRCH;

    bool valid = task->tgid == target_tgid;
    put_task_struct(task);
    if (!valid) return -ESRCH;

    mutex_lock(&g_cntvct_monitor_mutex);

    int status = inline_hook_install(g_cntvct_monitor_hooks);
    if (status < 0) goto out_unlock;

    WRITE_ONCE(g_cntvct_monitor_tgid, target_tgid);
    status = cntvct_monitor_update_online_cpus();
    if (status < 0) goto out_stop;

    ls_log_always_tag("cntvct", "start tgid=%d\n", target_tgid);
    mutex_unlock(&g_cntvct_monitor_mutex);
    return 0;

out_stop:
    cntvct_monitor_stop_locked();
out_unlock:
    mutex_unlock(&g_cntvct_monitor_mutex);
    return status;
}

// 正 TGID 匹配当前目标时停止监控，非正值表示无条件停止
static void cntvct_monitor_remove(pid_t tgid)
{
    mutex_lock(&g_cntvct_monitor_mutex);
    pid_t active_tgid = READ_ONCE(g_cntvct_monitor_tgid);
    if (tgid <= 0 || active_tgid <= 0 || tgid == active_tgid) cntvct_monitor_stop_locked();
    mutex_unlock(&g_cntvct_monitor_mutex);
}

#endif /* ARM64_CNTVCTDBG_H */