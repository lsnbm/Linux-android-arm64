/*
指定进程环境参数获取
*/
#ifndef ARM64_ENV_H
#define ARM64_ENV_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/pid.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/types.h>
#include <asm/sysreg.h>

#include "export_fun.h"

static inline int get_tpidr_el0_by_name(int32_t tgid, const char *thread_name, uint64_t *tpidr_el0)
{
    struct task_struct *process_task, *thread_task;

    if (!thread_name || !tpidr_el0 || tgid <= 0) return -EINVAL;

    *tpidr_el0 = 0;
    process_task = get_task_by_pid(tgid);
    if (!process_task) return -ESRCH;

    for_each_thread(process_task, thread_task)
    {
        if (__builtin_strncmp(thread_task->comm, thread_name, TASK_COMM_LEN) == 0)
        {
            if (thread_task == current) *tpidr_el0 = (uint64_t)read_sysreg(tpidr_el0);
            else *tpidr_el0 = (uint64_t)(*task_user_tls(thread_task));
            break;
        }
    }

    put_task_struct(process_task);
    return *tpidr_el0 ? 0 : -ESRCH;
}

static inline int get_pacga_key(pid_t pid, unsigned long *lo, unsigned long *hi)
{
#ifdef CONFIG_ARM64_PTR_AUTH
    struct task_struct *src;

    if (pid <= 0 || !lo || !hi) return -EINVAL;

    *lo = 0;
    *hi = 0;

    rcu_read_lock();

    src = find_task_by_vpid(pid);
    if (!src)
    {
        rcu_read_unlock();
        return -ESRCH;
    }

    get_task_struct(src);
    rcu_read_unlock();

    *lo = src->thread.keys_user.apga.lo;
    *hi = src->thread.keys_user.apga.hi;

    put_task_struct(src);
    return 0;
#else
    (void)pid;
    (void)lo;
    (void)hi;
    return -EOPNOTSUPP;
#endif
}

static inline int get_env_params(pid_t pid, const char *thread_name, uint64_t *tpidr_el0, uint64_t *pacga_lo, uint64_t *pacga_hi, int *tls_status, int *pacga_status)
{
    unsigned long lo = 0;
    unsigned long hi = 0;
    int tls_ret;
    int pacga_ret;

    if (pid <= 0 || !thread_name || !tpidr_el0 || !pacga_lo || !pacga_hi || !tls_status || !pacga_status) return -EINVAL;

    *tpidr_el0 = 0;
    *pacga_lo = 0;
    *pacga_hi = 0;

    tls_ret = get_tpidr_el0_by_name(pid, thread_name, tpidr_el0);
    pacga_ret = get_pacga_key(pid, &lo, &hi);

    *pacga_lo = (uint64_t)lo;
    *pacga_hi = (uint64_t)hi;
    *tls_status = tls_ret;
    *pacga_status = pacga_ret;

    if (tls_ret == 0 || pacga_ret == 0) return 0;
    return tls_ret ? tls_ret : pacga_ret;
}

#endif /* ARM64_ENV_H */