/*
指定进程线程 TLS 获取
*/
#ifndef ARM64_TLS_H
#define ARM64_TLS_H

#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/types.h>
#include <asm/sysreg.h>

#include "export_fun.h"

static inline uint64_t get_tpidr_el0_by_name(int32_t tgid, const char *thread_name)
{
    struct task_struct *process_task, *thread_task;
    uint64_t tpidr_val = 0;

    if (!thread_name || tgid <= 0)
        return 0;

    process_task = get_task_by_pid(tgid);
    if (!process_task)
        return 0;

    for_each_thread(process_task, thread_task)
    {
        if (__builtin_strncmp(thread_task->comm, thread_name, TASK_COMM_LEN) == 0)
        {
            if (thread_task == current)
                tpidr_val = (uint64_t)read_sysreg(tpidr_el0);
            else
                tpidr_val = (uint64_t)(*task_user_tls(thread_task));
            break;
        }
    }

    put_task_struct(process_task);
    return tpidr_val;
}

#endif /* ARM64_TLS_H */