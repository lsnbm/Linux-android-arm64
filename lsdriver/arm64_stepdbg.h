#ifndef ARM64_STEPDBG_H
#define ARM64_STEPDBG_H

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/jiffies.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/sched/task_stack.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/thread_info.h>
#include <linux/wait.h>
#include <asm/debug-monitors.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>
#include <asm/thread_info.h>

#include "inline_hook_frame.h"
#include "lsdriver_log.h"

static struct break_point *g_stepbp_info;
static DEFINE_SPINLOCK(g_stepbp_lock);
static DEFINE_SPINLOCK(g_stepbp_hit_lock);
static DEFINE_MUTEX(g_stepbp_mutex);
static pid_t g_stepbp_active_pid;
static bool g_stepbp_stopping;
static unsigned long g_stepbp_generation;

struct stepbp_point_config
{
    void (*on_hit)(void *regs, void *self);
    enum bp_type bt;
    enum bp_scope bs;
    uint64_t hit_addr;
    struct bp_point *shared_output;
    struct bp_point *private_point;
};

struct stepbp_monitor_config
{
    pid_t pid;
    struct bp_point *private_points;
    struct stepbp_point_config points[BP_CONFIG_MAX];
};

static struct stepbp_monitor_config g_stepbp_config;

#define STEPBP_LOG_LIMITED(counter, limit, fmt, ...)                                            \
    do                                                                                          \
    {                                                                                           \
        if (atomic_inc_return(&(counter)) <= (limit)) ls_log_tag("stepbp", fmt, ##__VA_ARGS__); \
    } while (0)

static atomic_t g_stepbp_log_enable = ATOMIC_INIT(0);
static atomic_t g_stepbp_log_switch = ATOMIC_INIT(0);
static atomic_t g_stepbp_log_syscall = ATOMIC_INIT(0);
static atomic_t g_stepbp_log_hit = ATOMIC_INIT(0);
static atomic_t g_stepbp_returns_inflight = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(g_stepbp_return_wait);

struct stepbp_return_frame
{
    unsigned long return_addr;
    unsigned long generation;
    struct pt_regs *regs;
};

// 判断指定 task 是否属于目标 pid/tgid。
static inline bool stepbp_task_matches(struct task_struct *task, pid_t target_pid)
{
    return task && target_pid > 0 && (target_pid == task->tgid || target_pid == task->pid);
}

// 判断私有配置点是否是有效的 STEPBP 执行断点。
static inline bool stepbp_config_point_is_active(const struct stepbp_point_config *point)
{
    return point && point->hit_addr != 0 && point->bt == BP_BREAKPOINT_X && point->bs >= BP_SCOPE_MAIN_THREAD && point->bs <= BP_SCOPE_ALL_THREADS;
}

static inline bool stepbp_config_point_matches_task(const struct stepbp_point_config *point, struct task_struct *task)
{
    if (!stepbp_config_point_is_active(point) || !task) return false;

    switch (point->bs)
    {
    case BP_SCOPE_MAIN_THREAD:
        return thread_group_leader(task);
    case BP_SCOPE_OTHER_THREADS:
        return !thread_group_leader(task);
    case BP_SCOPE_ALL_THREADS:
        return true;
    default:
        return false;
    }
}

static inline bool stepbp_config_matches_task(const struct stepbp_monitor_config *config, struct task_struct *task)
{
    int point_slot;

    if (!config || !task) return false;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (stepbp_config_point_matches_task(&config->points[point_slot], task)) return true;
    }

    return false;
}

static bool stepbp_snapshot_config(struct break_point *info, struct stepbp_monitor_config *config)
{
    int point_slot;
    bool has_active_point = false;

    if (!config) return false;

    memset(config, 0, sizeof(*config));
    if (!info) return false;

    config->pid = READ_ONCE(info->pid);
    if (config->pid <= 0) return false;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct bp_point *source = &info->points[point_slot];
        struct stepbp_point_config *point = &config->points[point_slot];

        point->on_hit = READ_ONCE(source->on_hit);
        point->bt = READ_ONCE(source->bt);
        point->bs = READ_ONCE(source->bs);
        point->hit_addr = READ_ONCE(source->hit_addr);
        point->shared_output = source;
        if (stepbp_config_point_is_active(point)) has_active_point = true;
    }

    if (!has_active_point) return false;

    config->private_points = vzalloc(sizeof(*config->private_points) * BP_CONFIG_MAX);
    if (!config->private_points) return false;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct bp_point *source = &info->points[point_slot];
        struct stepbp_point_config *point = &config->points[point_slot];

        point->private_point = &config->private_points[point_slot];
        point->private_point->on_hit = point->on_hit;
        point->private_point->bt = point->bt;
        point->private_point->bl = READ_ONCE(source->bl);
        point->private_point->bs = point->bs;
        point->private_point->hit_addr = point->hit_addr;
    }

    return has_active_point;
}

static void stepbp_free_config(struct stepbp_monitor_config *config)
{
    if (!config) return;

    vfree(config->private_points);
    memset(config, 0, sizeof(*config));
}

static inline int stepbp_clamp_record_count(int record_count)
{
    if (record_count < 0) return 0;
    if (record_count > ARRAY_SIZE(((struct bp_point *)0)->records)) return ARRAY_SIZE(((struct bp_point *)0)->records);
    return record_count;
}

static inline void stepbp_publish_monitor(struct break_point *info, pid_t target_pid, bool stopping)
{
    g_stepbp_generation++;
    WRITE_ONCE(g_stepbp_info, info);
    WRITE_ONCE(g_stepbp_active_pid, target_pid);
    WRITE_ONCE(g_stepbp_stopping, stopping);
}

// 设置返回现场的 SPSR.SS 位，配合 ret_to_user 中的 TIF_SINGLESTEP 打开 MDSCR.SS。
static inline void stepbp_set_regs_single_step(struct pt_regs *regs)
{
    if (regs) regs->pstate |= DBG_SPSR_SS;
}

// 清理返回现场的 SPSR.SS 位，停用后不再续发单步异常。
static inline void stepbp_clear_regs_single_step(struct pt_regs *regs)
{
    if (regs) regs->pstate &= ~DBG_SPSR_SS;
}

static inline void stepbp_enable_task_single_step(struct task_struct *task)
{
    if (!task) return;

    // TIF_SINGLESTEP 是线程级状态，必须对每个目标 task 单独设置。
    // ret_to_user 看到该 flag 后会打开当前 CPU 的 MDSCR.SS。
    set_ti_thread_flag(task_thread_info(task), TIF_SINGLESTEP);
    stepbp_set_regs_single_step(task_pt_regs(task));
}

static inline void stepbp_disable_task_single_step(struct task_struct *task)
{
    if (!task) return;

    clear_ti_thread_flag(task_thread_info(task), TIF_SINGLESTEP);
    stepbp_clear_regs_single_step(task_pt_regs(task));
}

static inline void stepbp_disable_current_hardware_step(struct pt_regs *regs)
{
    clear_thread_flag(TIF_SINGLESTEP);
    stepbp_clear_regs_single_step(regs);
    write_sysreg(read_sysreg(mdscr_el1) & ~DBG_MDSCR_SS, mdscr_el1);
    isb();
}

static inline void stepbp_apply_task_single_step(struct task_struct *task, bool enable)
{
    if (enable) stepbp_enable_task_single_step(task);
    else stepbp_disable_task_single_step(task);
}

struct stepbp_cpu_update
{
    pid_t target_pid;
    bool enable;
};

static void stepbp_update_current_cpu(void *data)
{
    struct stepbp_cpu_update *update = data;

    if (!stepbp_task_matches(current, update->target_pid)) return;

    if (update->enable && stepbp_config_matches_task(&g_stepbp_config, current)) stepbp_enable_task_single_step(current);
    else stepbp_disable_current_hardware_step(task_pt_regs(current));
}

static int stepbp_apply_pid_tasks(pid_t target_pid, bool enable)
{
    struct stepbp_cpu_update update = {
        .target_pid = target_pid,
        .enable = enable,
    };
    struct task_struct *target_task;
    struct task_struct *process;
    struct task_struct *task;
    int touched_count = 0;

    if (target_pid <= 0) return 0;

    rcu_read_lock();
    target_task = find_task_by_vpid(target_pid);
    if (!target_task)
    {
        for_each_process_thread(process, task)
        {
            if (!stepbp_task_matches(task, target_pid)) continue;
            stepbp_apply_task_single_step(task, enable && stepbp_config_matches_task(&g_stepbp_config, task));
            touched_count++;
        }
        goto out_unlock;
    }

    if (target_task->tgid == target_pid)
    {
        stepbp_apply_task_single_step(target_task, enable && stepbp_config_matches_task(&g_stepbp_config, target_task));
        touched_count++;
        for_each_thread(target_task, task)
        {
            stepbp_apply_task_single_step(task, enable && stepbp_config_matches_task(&g_stepbp_config, task));
            touched_count++;
        }
    }
    else
    {
        stepbp_apply_task_single_step(target_task, enable && stepbp_config_matches_task(&g_stepbp_config, target_task));
        touched_count = 1;
    }

out_unlock:
    rcu_read_unlock();
    stepbp_update_current_cpu(&update);
    smp_call_function(stepbp_update_current_cpu, &update, 1);
    return touched_count;
}

static void __attribute__((used, __noinline__)) stepbp_finish_syscall_trace_exit(struct stepbp_return_frame *frame);
__attribute__((naked, used)) void ret_trampoline_stepbp_syscall_trace_exit(void)
{
    asm volatile("mov x0, sp\n"
                 "bl stepbp_finish_syscall_trace_exit\n"
                 "ldp x16, xzr, [sp], #304\n"
                 "ret x16\n");
}

static void __attribute__((used, __noinline__)) stepbp_finish_syscall_trace_exit(struct stepbp_return_frame *frame)
{
    unsigned long flags;

    spin_lock_irqsave(&g_stepbp_lock, flags);
    if (frame->generation == g_stepbp_generation)
    {
        if (g_stepbp_info && !g_stepbp_stopping && stepbp_task_matches(current, g_stepbp_active_pid) && stepbp_config_matches_task(&g_stepbp_config, current)) stepbp_enable_task_single_step(current);
        else stepbp_disable_current_hardware_step(frame->regs);
    }
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    if (atomic_dec_and_test(&g_stepbp_returns_inflight)) wake_up_all(&g_stepbp_return_wait);
}

// syscall_trace_exit() 会在 _TIF_SINGLESTEP 下调用 report_syscall(PTRACE_SYSCALL_EXIT)，
// 对用户态表现为 ptrace pseudo-step SIGTRAP。入口临时清 flag，并用返回跳板在
// syscall_trace_exit() 完整执行 audit/trace/rseq 后恢复单步状态。
static int work_trampoline_stepbp_syscall_trace_exit(struct pt_regs *hook_regs)
{
    unsigned long flags;
    unsigned long generation;
    struct stepbp_return_frame *frame;
    struct pt_regs *regs;
    bool target_task;
    bool deferred = false;

    if (!hook_regs) return 0;

    regs = (struct pt_regs *)hook_regs->regs[0];
    if (!regs || !user_mode(regs)) return 0;

    spin_lock_irqsave(&g_stepbp_lock, flags);
    target_task = stepbp_task_matches(current, g_stepbp_active_pid) && (g_stepbp_stopping || (g_stepbp_info && stepbp_config_matches_task(&g_stepbp_config, current)));
    generation = g_stepbp_generation;
    if (target_task && test_thread_flag(TIF_SINGLESTEP))
    {
        frame = hook_frame_metadata(hook_regs);
        frame->return_addr = hook_regs->regs[30];
        frame->generation = generation;
        frame->regs = regs;
        atomic_inc(&g_stepbp_returns_inflight);
        hook_regs->sp = (unsigned long)frame;
        hook_regs->regs[30] = (unsigned long)ret_trampoline_stepbp_syscall_trace_exit;
        stepbp_set_regs_single_step(regs);
        clear_thread_flag(TIF_SINGLESTEP);
        deferred = true;
    }
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    if (deferred) STEPBP_LOG_LIMITED(g_stepbp_log_syscall, 2, "syscall_exit defer single-step restore pid=%d tgid=%d\n", current->pid, current->tgid);
    return 0;
}

// __switch_to 入口补 arm：覆盖安装后才创建/切入的目标线程。
static int work_trampoline_stepbp_switch(struct pt_regs *hook_regs)
{
    struct task_struct *next;
    unsigned long flags;
    bool enable = false;
    pid_t target_pid = 0;

    if (!hook_regs) return 0;

    next = (struct task_struct *)hook_regs->regs[1];
    spin_lock_irqsave(&g_stepbp_lock, flags);
    if (g_stepbp_info && !g_stepbp_stopping && g_stepbp_active_pid > 0 && stepbp_task_matches(next, g_stepbp_active_pid))
    {
        target_pid = g_stepbp_active_pid;
        enable = stepbp_config_matches_task(&g_stepbp_config, next);
    }
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    if (target_pid) stepbp_apply_task_single_step(next, enable);
    if (enable) STEPBP_LOG_LIMITED(g_stepbp_log_switch, 4, "switch arm target=%d next pid=%d tgid=%d comm=%s\n", target_pid, next->pid, next->tgid, next->comm);

    return 0;
}

static int __attribute__((used, __noinline__)) stepbp_finish_call_step_hook(int native_result, struct stepbp_return_frame *frame);
__attribute__((naked, used)) void ret_trampoline_stepbp_call_step_hook(void)
{
    asm volatile("mov x1, sp\n"
                 "bl stepbp_finish_call_step_hook\n"
                 "ldp x16, xzr, [sp], #304\n"
                 "ret x16\n");
}

static int __attribute__((used, __noinline__)) stepbp_finish_call_step_hook(int native_result, struct stepbp_return_frame *frame)
{
    int point_slot;
    int hit_slot = -1;
    int result = native_result;
    unsigned long flags;
    struct stepbp_point_config *hit_point = NULL;
    void (*hit_callback)(void *regs, void *self) = NULL;
    uint64_t hit_addr = 0;
    unsigned long hit_generation = 0;
    bool generation_matches;
    bool target_task = false;
    bool stopping = false;
    struct pt_regs *regs;
    struct bp_point *private_point;
    struct bp_point *shared_point;
    int record_count;

    regs = frame->regs;
    spin_lock_irqsave(&g_stepbp_lock, flags);
    generation_matches = frame->generation == g_stepbp_generation;
    if (generation_matches)
    {
        stopping = g_stepbp_stopping;
        target_task = stepbp_task_matches(current, g_stepbp_active_pid) && (stopping || (g_stepbp_info && stepbp_config_matches_task(&g_stepbp_config, current)));
        if (target_task && !stopping && native_result != DBG_HOOK_HANDLED)
        {
            for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
            {
                struct stepbp_point_config *point = &g_stepbp_config.points[point_slot];

                if (!stepbp_config_point_matches_task(point, current)) continue;
                if ((point->hit_addr & ~0x3ULL) != (regs->pc & ~0x3ULL)) continue;

                hit_point = point;
                hit_callback = point->on_hit;
                hit_addr = point->hit_addr;
                hit_generation = g_stepbp_generation;
                hit_slot = point_slot;
                break;
            }
        }
    }
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    if (!generation_matches || target_task) result = DBG_HOOK_HANDLED;

    if (generation_matches && target_task && !stopping && native_result != DBG_HOOK_HANDLED && hit_point && hit_callback)
    {
        spin_lock_irqsave(&g_stepbp_hit_lock, flags);
        if (!READ_ONCE(g_stepbp_stopping) && READ_ONCE(g_stepbp_generation) == hit_generation)
        {
            private_point = hit_point->private_point;
            shared_point = hit_point->shared_output;
            record_count = stepbp_clamp_record_count(READ_ONCE(shared_point->record_count));
            if (record_count) memcpy(private_point->records, shared_point->records, sizeof(private_point->records[0]) * record_count);
            WRITE_ONCE(private_point->record_count, record_count);

            STEPBP_LOG_LIMITED(g_stepbp_log_hit, 8, "hit slot=%d pid=%d tgid=%d pc=0x%llx hit_addr=0x%llx record_count=%d\n", hit_slot, current->pid, current->tgid, (unsigned long long)regs->pc, (unsigned long long)hit_addr, record_count);
            hit_callback((void *)regs, (void *)private_point);

            record_count = stepbp_clamp_record_count(READ_ONCE(private_point->record_count));
            if (record_count) memcpy(shared_point->records, private_point->records, sizeof(private_point->records[0]) * record_count);
            smp_wmb();
            WRITE_ONCE(shared_point->record_count, record_count);
        }
        spin_unlock_irqrestore(&g_stepbp_hit_lock, flags);
    }

    spin_lock_irqsave(&g_stepbp_lock, flags);
    if (frame->generation == g_stepbp_generation)
    {
        if (g_stepbp_info && !g_stepbp_stopping && stepbp_task_matches(current, g_stepbp_active_pid) && stepbp_config_matches_task(&g_stepbp_config, current)) stepbp_enable_task_single_step(current);
        else stepbp_disable_current_hardware_step(frame->regs);
    }
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    if (atomic_dec_and_test(&g_stepbp_returns_inflight)) wake_up_all(&g_stepbp_return_wait);
    return result;
}

// call_step_hook 入口只安装返回后处理；原生 uprobe/perf step hook 先完整执行。
static int work_trampoline_stepbp_single_step(struct pt_regs *hook_regs)
{
    unsigned long flags;
    unsigned long generation;
    bool target_task;
    struct stepbp_return_frame *frame;
    struct pt_regs *regs;

    if (!hook_regs) return 0;

    regs = (struct pt_regs *)hook_regs->regs[0];
    if (!regs) return 0;

    // user_mode() 判断异常现场是否来自 EL0；STEPBP 只接管用户态单步，不碰 EL1 内核态异常。
    if (!user_mode(regs)) return 0;

    spin_lock_irqsave(&g_stepbp_lock, flags);
    target_task = stepbp_task_matches(current, g_stepbp_active_pid) && (g_stepbp_stopping || (g_stepbp_info && stepbp_config_matches_task(&g_stepbp_config, current)));
    generation = g_stepbp_generation;
    if (target_task)
    {
        frame = hook_frame_metadata(hook_regs);
        frame->return_addr = hook_regs->regs[30];
        frame->generation = generation;
        frame->regs = regs;
        atomic_inc(&g_stepbp_returns_inflight);
        hook_regs->sp = (unsigned long)frame;
        hook_regs->regs[30] = (unsigned long)ret_trampoline_stepbp_call_step_hook;
    }
    spin_unlock_irqrestore(&g_stepbp_lock, flags);
    return 0;
}

static struct hook_entry g_stepbp_required_hooks[] = {
    HOOK_ENTRY("call_step_hook", work_trampoline_stepbp_single_step),
    HOOK_ENTRY("syscall_trace_exit", work_trampoline_stepbp_syscall_trace_exit),
};

static struct hook_entry g_stepbp_switch_hook[] = {
    HOOK_ENTRY("__switch_to", work_trampoline_stepbp_switch),
};

static void stepbp_dump_hook_symbols(void)
{
    int i;

    ls_log_tag("stepbp", "patch_text=0x%llx\n", (unsigned long long)fn_aarch64_insn_patch_text);
    for (i = 0; i < (int)(sizeof(g_stepbp_required_hooks) / sizeof(g_stepbp_required_hooks[0])); i++)
    {
        unsigned long addr = generic_kallsyms_lookup_name(g_stepbp_required_hooks[i].target_sym);
        ls_log_tag("stepbp", "required symbol %s=0x%lx\n", g_stepbp_required_hooks[i].target_sym, addr);
    }

    for (i = 0; i < (int)(sizeof(g_stepbp_switch_hook) / sizeof(g_stepbp_switch_hook[0])); i++)
    {
        unsigned long addr = generic_kallsyms_lookup_name(g_stepbp_switch_hook[i].target_sym);
        ls_log_tag("stepbp", "optional symbol %s=0x%lx\n", g_stepbp_switch_hook[i].target_sym, addr);
    }
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
            ls_log_tag("stepbp", "required hook failed index=%d symbol=%s status=%d target=0x%llx patch_text=0x%llx\n", i, g_stepbp_required_hooks[i].target_sym, ret, (unsigned long long)g_stepbp_required_hooks[i].target_addr, (unsigned long long)fn_aarch64_insn_patch_text);
            while (--i >= 0) hook_entry_remove(&g_stepbp_required_hooks[i]);
            return ret;
        }

        ls_log_tag("stepbp", "required hook ok index=%d symbol=%s target=0x%llx\n", i, g_stepbp_required_hooks[i].target_sym, (unsigned long long)g_stepbp_required_hooks[i].target_addr);
    }

    return 0;
}

static void stepbp_remove_required_hooks(void)
{
    int i;

    for (i = (int)(sizeof(g_stepbp_required_hooks) / sizeof(g_stepbp_required_hooks[0])) - 1; i >= 0; i--) hook_entry_remove(&g_stepbp_required_hooks[i]);
}

static void stepbp_install_optional_switch_hook(void)
{
    int status;

    status = inline_hook_install_count(g_stepbp_switch_hook, sizeof(g_stepbp_switch_hook) / sizeof(g_stepbp_switch_hook[0]));
    if (status)
    {
        ls_log_tag("stepbp", "optional switch hook skipped status=%d target=0x%llx\n", status, (unsigned long long)g_stepbp_switch_hook[0].target_addr);
        return;
    }

    ls_log_tag("stepbp", "optional switch hook ok target=0x%llx\n", (unsigned long long)g_stepbp_switch_hook[0].target_addr);
}

// 调用方持有 g_stepbp_mutex；先清理目标线程保存现场，再移除 hook。
static void stepbp_stop_monitor_locked(void)
{
    struct stepbp_monitor_config old_config;
    pid_t target_pid = 0;
    unsigned long flags;

    memset(&old_config, 0, sizeof(old_config));

    spin_lock_irqsave(&g_stepbp_lock, flags);
    target_pid = g_stepbp_active_pid;
    stepbp_publish_monitor(g_stepbp_info, target_pid, true);
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    spin_lock_irqsave(&g_stepbp_hit_lock, flags);
    spin_unlock_irqrestore(&g_stepbp_hit_lock, flags);

    if (target_pid > 0) stepbp_apply_pid_tasks(target_pid, false);

    inline_hook_remove_count(g_stepbp_switch_hook, sizeof(g_stepbp_switch_hook) / sizeof(g_stepbp_switch_hook[0]));
    stepbp_remove_required_hooks();
    wait_event(g_stepbp_return_wait, atomic_read(&g_stepbp_returns_inflight) == 0);

    spin_lock_irqsave(&g_stepbp_lock, flags);
    old_config = g_stepbp_config;
    stepbp_publish_monitor(NULL, 0, false);
    memset(&g_stepbp_config, 0, sizeof(g_stepbp_config));
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    stepbp_free_config(&old_config);
}

// 停止 STEPBP：串行化安装/卸载，避免并发替换 hook 和全局配置。
static inline void stop_stepbp_monitor(void)
{
    mutex_lock(&g_stepbp_mutex);
    stepbp_stop_monitor_locked();
    mutex_unlock(&g_stepbp_mutex);
}

// 安装 STEPBP：快照配置、hook call_step_hook，并启用目标线程单步。
static inline int start_stepbp_monitor(struct break_point *info)
{
    struct stepbp_monitor_config config;
    int status;
    unsigned long flags;

    if (!stepbp_snapshot_config(info, &config))
    {
        ls_log_tag("stepbp", "start rejected pid=%d no active execute point\n", info ? READ_ONCE(info->pid) : -1);
        return -EINVAL;
    }

    mutex_lock(&g_stepbp_mutex);
    stepbp_stop_monitor_locked();
    atomic_set(&g_stepbp_log_enable, 0);
    atomic_set(&g_stepbp_log_switch, 0);
    atomic_set(&g_stepbp_log_syscall, 0);
    atomic_set(&g_stepbp_log_hit, 0);
    stepbp_dump_hook_symbols();

    status = stepbp_install_required_hooks();
    if (status)
    {
        ls_log_tag("stepbp", "hook install failed pid=%d status=%d\n", config.pid, status);
        stepbp_free_config(&config);
        goto out_unlock;
    }

    stepbp_install_optional_switch_hook();

    g_stepbp_config = config;
    config.private_points = NULL;
    spin_lock_irqsave(&g_stepbp_lock, flags);
    stepbp_publish_monitor(info, config.pid, false);
    spin_unlock_irqrestore(&g_stepbp_lock, flags);

    status = stepbp_apply_pid_tasks(config.pid, true);
    STEPBP_LOG_LIMITED(g_stepbp_log_enable, 2, "enable pid=%d armed_tasks=%d current pid=%d tgid=%d comm=%s\n", config.pid, status, current->pid, current->tgid, current->comm);

    ls_log_tag("stepbp", "start ok pid=%d first_addr=0x%llx bt=0x%x bs=0x%x\n", config.pid, (unsigned long long)config.points[0].hit_addr, config.points[0].bt, config.points[0].bs);
    status = 0;

out_unlock:
    mutex_unlock(&g_stepbp_mutex);
    return status;
}

#endif // ARM64_STEPDBG_H