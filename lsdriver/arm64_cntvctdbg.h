/*
指定进程 CNTVCT_EL0 读取监控
*/
#ifndef ARM64_CNTVCTDBG_H
#define ARM64_CNTVCTDBG_H

#include <linux/cpu.h>
#include <linux/mutex.h>
#include <linux/percpu.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/smp.h>
#include <linux/tracepoint.h>
#include <clocksource/arm_arch_timer.h>
#include <asm/cpucaps.h>
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

static DEFINE_MUTEX(g_cntvct_monitor_mutex);
static DEFINE_PER_CPU(struct cntvct_monitor_cpu_state, g_cntvct_monitor_cpu_state);
static pid_t g_cntvct_monitor_tgid;
static struct tracepoint *g_cntvct_monitor_sys_exit_tp;
static bool (*g_cntvct_monitor_this_cpu_has_cap)(unsigned int cap);

static __always_inline pid_t cntvct_monitor_target_tgid(void)
{
    return READ_ONCE(g_cntvct_monitor_tgid);
}

static __always_inline void cntvct_monitor_restore_cpu(void)
{
    struct cntvct_monitor_cpu_state *state = this_cpu_ptr(&g_cntvct_monitor_cpu_state);

    if (!state->armed) return;

    sysreg_clear_set(cntkctl_el1, ARCH_TIMER_USR_VCT_ACCESS_EN, state->access_was_enabled ? ARCH_TIMER_USR_VCT_ACCESS_EN : 0);
    isb();
    state->armed = false;
}

static __always_inline void cntvct_monitor_arm_cpu(void)
{
    struct cntvct_monitor_cpu_state *state = this_cpu_ptr(&g_cntvct_monitor_cpu_state);
    unsigned long cntkctl;

    cntkctl = read_sysreg(cntkctl_el1);
    if (!state->armed)
    {
        state->access_was_enabled = !!(cntkctl & ARCH_TIMER_USR_VCT_ACCESS_EN);
        state->armed = true;
    }
    else if (cntkctl & ARCH_TIMER_USR_VCT_ACCESS_EN)
    {
        state->access_was_enabled = true;
    }
    write_sysreg(cntkctl & ~ARCH_TIMER_USR_VCT_ACCESS_EN, cntkctl_el1);
    isb();
}

static void cntvct_monitor_update_current_cpu(void *unused)
{
    pid_t target_tgid = cntvct_monitor_target_tgid();

    (void)unused;
    cntvct_monitor_restore_cpu();
    if (target_tgid > 0 && current->tgid == target_tgid) cntvct_monitor_arm_cpu();
}

static int cntvct_monitor_update_online_cpus(void)
{
    int cpu;
    int status = 0;
    int cpu_status;

    cpus_read_lock();
    for_each_online_cpu(cpu)
    {
        cpu_status = smp_call_function_single(cpu, cntvct_monitor_update_current_cpu, NULL, 1);
        if (status == 0 && cpu_status < 0) status = cpu_status;
    }
    cpus_read_unlock();
    return status;
}

/*
__switch_to(prev, next) 内部会先调用 erratum_1418040_thread_switch(next)：
受影响 CPU 切到 32 位任务时清除 EL0VCTEN，切到 64 位任务时重新设置 EL0VCTEN。
因此如果 hook __switch_to 入口，我们写入的 CNTKCTL_EL1 会被该函数随后覆盖。

这里改在紧随其后的 cpu_switch_to(prev, next) 入口处理，x1 仍然是 next，并且
内核针对 next 的 CNTKCTL_EL1 更新已经完成。workaround 生效时，当前寄存器值就是
next 的原生基线，不能再恢复旧任务保存的值，只清除 armed 后按目标任务重新设置。
*/
static int cntvct_monitor_switch_hook_work(struct pt_regs *hook_regs)
{
    struct cntvct_monitor_cpu_state *state;
    struct task_struct *next;
    pid_t target_tgid;

    if (!hook_regs) return 0;

    state = this_cpu_ptr(&g_cntvct_monitor_cpu_state);
    next = (struct task_struct *)(uintptr_t)hook_regs->regs[1];
    target_tgid = cntvct_monitor_target_tgid();
    if (state->armed && IS_ENABLED(CONFIG_ARM64_ERRATUM_1418040) && g_cntvct_monitor_this_cpu_has_cap && g_cntvct_monitor_this_cpu_has_cap(ARM64_WORKAROUND_1418040))
    {
        state->armed = false;
    }
    else
    {
        cntvct_monitor_restore_cpu();
    }
    if (target_tgid > 0 && next && next->tgid == target_tgid) cntvct_monitor_arm_cpu();
    return 0;
}

static void cntvct_monitor_sys_exit_probe(void *data, struct pt_regs *regs, long ret)
{
    pid_t target_tgid = cntvct_monitor_target_tgid();

    (void)data;
    (void)regs;
    (void)ret;
    if (target_tgid > 0 && current->tgid == target_tgid)
    {
        preempt_disable();
        cntvct_monitor_arm_cpu();
        preempt_enable();
    }
}

static int cntvct_monitor_read_hook_work(struct pt_regs *hook_regs)
{
    unsigned long esr;
    struct pt_regs *regs;
    pid_t target_tgid;

    if (!hook_regs) return 0;

    target_tgid = cntvct_monitor_target_tgid();
    if (target_tgid <= 0 || current->tgid != target_tgid) return 0;

    esr = hook_regs->regs[0];
    if ((esr & ESR_ELx_SYS64_ISS_SYS_OP_MASK) != ESR_ELx_SYS64_ISS_SYS_CNTVCT) return 0;

    regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[1];
    if (!regs || !user_mode(regs)) return 0;

    ls_log_always_tag("cntvct", "tgid=%d pid=%d comm=%s pc=0x%llx\n", current->tgid, current->pid, current->comm, (unsigned long long)regs->pc);
    return 0;
}

static struct hook_entry g_cntvct_monitor_hooks[] = {
    HOOK_ENTRY("cntvct_read_handler", cntvct_monitor_read_hook_work),
    HOOK_ENTRY("cpu_switch_to", cntvct_monitor_switch_hook_work),
};

static int cntvct_monitor_validate_tgid(pid_t tgid)
{
    struct task_struct *task;
    bool valid;

    if (tgid <= 0) return -EINVAL;

    rcu_read_lock();
    task = find_task_by_vpid(tgid);
    valid = task && task->tgid == tgid;
    rcu_read_unlock();

    return valid ? 0 : -ESRCH;
}

static void cntvct_monitor_stop_locked(void)
{
    pid_t target_tgid = cntvct_monitor_target_tgid();
    int cpu;

    if (!g_cntvct_monitor_sys_exit_tp && !g_cntvct_monitor_hooks[0].installed && !g_cntvct_monitor_hooks[1].installed && target_tgid <= 0) return;

    WRITE_ONCE(g_cntvct_monitor_tgid, 0);
    smp_mb();

    if (g_cntvct_monitor_sys_exit_tp)
    {
        tracepoint_probe_unregister(g_cntvct_monitor_sys_exit_tp, (void *)cntvct_monitor_sys_exit_probe, NULL);
        g_cntvct_monitor_sys_exit_tp = NULL;
        tracepoint_synchronize_unregister();
    }

    cntvct_monitor_update_online_cpus();
    inline_hook_remove(g_cntvct_monitor_hooks);
    for_each_possible_cpu(cpu) per_cpu(g_cntvct_monitor_cpu_state, cpu).armed = false;
    if (target_tgid > 0) ls_log_always_tag("cntvct", "stop tgid=%d\n", target_tgid);
}

static int cntvct_monitor_install(pid_t target_tgid)
{
    struct tracepoint *sys_exit_tracepoint;
    int status;

    status = cntvct_monitor_validate_tgid(target_tgid);
    if (status < 0) return status;

    mutex_lock(&g_cntvct_monitor_mutex);

    if (IS_ENABLED(CONFIG_ARM64_ERRATUM_1418040) && !g_cntvct_monitor_this_cpu_has_cap)
    {
        g_cntvct_monitor_this_cpu_has_cap = (bool (*)(unsigned int))generic_kallsyms_lookup_name("this_cpu_has_cap");
        if (!g_cntvct_monitor_this_cpu_has_cap)
        {
            status = -ENOENT;
            goto out_unlock;
        }
    }

    sys_exit_tracepoint = (struct tracepoint *)generic_kallsyms_lookup_name("__tracepoint_sys_exit");
    if (!sys_exit_tracepoint)
    {
        status = -ENOENT;
        goto out_unlock;
    }

    status = tracepoint_probe_register(sys_exit_tracepoint, (void *)cntvct_monitor_sys_exit_probe, NULL);
    if (status < 0) goto out_unlock;
    g_cntvct_monitor_sys_exit_tp = sys_exit_tracepoint;

    status = inline_hook_install(g_cntvct_monitor_hooks);
    if (status < 0) goto out_stop;

    smp_wmb();
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

static void cntvct_monitor_remove(pid_t tgid)
{
    pid_t active_tgid;

    if (tgid <= 0) return;

    mutex_lock(&g_cntvct_monitor_mutex);
    active_tgid = cntvct_monitor_target_tgid();
    if (active_tgid <= 0 || tgid == active_tgid) cntvct_monitor_stop_locked();
    mutex_unlock(&g_cntvct_monitor_mutex);
}

static void cntvct_monitor_remove_all(void)
{
    mutex_lock(&g_cntvct_monitor_mutex);
    cntvct_monitor_stop_locked();
    mutex_unlock(&g_cntvct_monitor_mutex);
}

#endif /* ARM64_CNTVCTDBG_H */