#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <asm/sysreg.h>
#include <asm/processor.h>

// 通过进程TGID和线程名获取TPIDR_EL0
uint64_t get_tpidr_el0_by_name(int32_t tgid, const char *thread_name)
{
    struct task_struct *p, *t;
    struct pid *pid_struct;
    uint64_t tpidr_val = 0;

    if (!thread_name || tgid <= 0)
        return 0;

    // 查找主进程结构体
    pid_struct = find_get_pid(tgid);
    if (!pid_struct)
        return 0;

    p = get_pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct); // 拿到 task 后即可释放 pid 引用
    if (!p)
        return 0;

    //  遍历线程组
    rcu_read_lock();
    for_each_thread(p, t)
    {
        // 比较线程名，注意 task->comm 长度限制为 16 字节
        if (strncmp(t->comm, thread_name, 16) == 0)
        {

            // 提取 TPIDR_EL0
            if (t == current)
            {
                // 如果恰好是当前 CPU 正在运行该线程
                tpidr_val = (uint64_t)read_sysreg(tpidr_el0);
            }
            else
            {
                // 否则从线程保存的上下文镜像中读取
                // task_user_tls 返回的是指向保存地址的指针，需要解引用
                tpidr_val = (uint64_t)(*task_user_tls(t));
            }
            break;
        }
    }
    rcu_read_unlock();

    put_task_struct(p); // 释放 task 引用
    return tpidr_val;
}
