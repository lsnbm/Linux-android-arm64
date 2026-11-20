
/*
// 隐藏逻辑思路大改
隐藏 /proc PID 目录：过滤目录枚举，并拦截直接查找和缓存目录项复验。


1. 用户态 ls /proc 最终会走 getdents64。
2. getdents64 在内核里会创建 struct getdents_callback64，里面的 ctx.actor 指向 filldir64。
3. procfs 内部无论是 proc_fill_cache -> dir_emit，还是厂商改写后的 emit 路径，最终都要调用 ctx->actor。
4. 与其在 proc_root_readdir/proc_pid_readdir 入口临时替换 ctx->actor，不如直接 hook actor 本体 filldir64。
5. proc_pid_lookup 命中隐藏 PID 时返回 ERR_PTR(-ENOENT)。
6. pid_revalidate 使已缓存的隐藏 PID 根目录项失效，重新查找后同样返回 ENOENT。

- 不依赖厂商 proc_pid_readdir 是否调用 proc_fill_cache。
- 不需要保存/替换 ctx->actor，避开并发枚举时 actor 槽位绑定错乱的风险。
- getdents64 目录项输出前统一经过 filldir64，命中隐藏 PID 时直接返回“当前项成功处理”。

限制：
- 目录枚举只覆盖使用 filldir64 的 getdents64 路径；如果 32 位 compat 路径使用 compat_filldir，需要另 hook。
- filldir64 参数里没有 struct file，无法精确判断当前目录是否是 /proc。
  不过不影响：这里按 DT_DIR + 数字 PID 名字过滤，理论上其他目录下同名数字目录也会被隐藏。

*/
#ifndef HIDE_TASK_H
#define HIDE_TASK_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <uapi/linux/pidfd.h>

#include "export_fun.h"
#include "inline_hook_frame.h"
#include "klog/lsdriver_log.h"

#define HIDE_TASK_MAX_TGIDS 0x10

// 隐藏表保存线程组 TGID；所有入口先把 PID 解析为任务，再按任务的 TGID 匹配。
static pid_t g_hidden_tgids[HIDE_TASK_MAX_TGIDS];
static DEFINE_MUTEX(g_hide_task_lock);

static bool hide_task_match_tgid(pid_t tgid)
{
    for (int i = 0; i < HIDE_TASK_MAX_TGIDS; i++)
        if (READ_ONCE(g_hidden_tgids[i]) == tgid) return true;

    return false;
}

// 在 RCU 保护下读取任务的 TGID，覆盖线程组中的主线程和其他线程 PID。
static bool hide_task_match_pid(pid_t pid)
{
    bool matched = false;

    if (pid <= 0) return false;

    rcu_read_lock();
    struct task_struct *task = find_task_by_vpid(pid);
    if (task) matched = hide_task_match_tgid(READ_ONCE(task->tgid));
    rcu_read_unlock();

    return matched;
}

// filldir64 没有 struct file 参数，无法确认当前目录是否为 /proc；这里按 DT_DIR + 数字目录名解析 PID。
static bool hide_task_match_pid_name(const char *name, int namlen, unsigned int d_type)
{
    pid_t pid = 0;

    // 排除 d_type 的附加标志，仅比较文件类型位。
    if ((d_type & S_DT_MASK) != DT_DIR || !name || namlen <= 0 || namlen > 10) return false;

    // name 不是 NUL 结尾字符串，逐位解析并拒绝前导符号、非数字和整数溢出。
    for (int i = 0; i < namlen; i++)
    {
        if (name[i] < '0' || name[i] > '9') return false;

        int digit = name[i] - '0';
        if (pid > (INT_MAX - digit) / 10) return false;
        pid = pid * 10 + digit;
    }

    return hide_task_match_pid(pid);
}

// 进程 CPU 时钟的 clockid 编码为 (~pid << 3) | clock；clock=2 表示进程 CPU 时间。
static bool hide_task_match_process_clock(int clock_id)
{
    if (((unsigned int)clock_id & 7U) != 2U) return false;

    return hide_task_match_pid((pid_t) ~(clock_id >> 3));
}

// 设置成功返回值并跳过 filldir64，省略当前目录项。
static int filldir64_skip_entry(struct pt_regs *regs)
{
    // 命中后伪造“当前目录项已成功处理”，并跳过原 filldir64，不写入用户缓冲。
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    regs->regs[0] = true; // filldir64 在 6.1 以上返回 true 表示成功处理。
#else
    regs->regs[0] = 0; // filldir64 在 6.1 以下返回 0 表示没有错误。
#endif
    return 1; // 不继续执行 filldir64。
}

// inline hook 工作函数，返回 0 表示继续执行原 filldir64，返回 1 表示跳过原 filldir64。
static int filldir64_hook_work(struct pt_regs *regs)
{
    // arm64 调用约定：filldir64(ctx, name, namlen, offset, ino, d_type)
    const char *name = (const char *)regs->regs[1];
    int namlen = (int)regs->regs[2];
    unsigned int d_type = (unsigned int)regs->regs[5];

    return hide_task_match_pid_name(name, namlen, d_type) ? filldir64_skip_entry(regs) : 0;
}

// 拦截隐藏 PID 的直接查找，返回 ENOENT。
static int proc_pid_lookup_hook_work(struct pt_regs *regs)
{
    const struct dentry *dentry = (const struct dentry *)regs->regs[0];

    if (!hide_task_match_pid_name(dentry->d_name.name, dentry->d_name.len, DT_DIR)) return 0;

    regs->regs[0] = (unsigned long)ERR_PTR(-ENOENT);
    return 1;
}

// 使已缓存的隐藏 PID 目录项失效，按任务的 TGID 匹配隐藏表。
static int pid_revalidate_hook_work(struct pt_regs *regs)
{
    bool legacy_signature = __builtin_types_compatible_p(typeof(((struct dentry_operations *)0)->d_revalidate), int (*)(struct dentry *, unsigned int));
    const struct dentry *dentry = (const struct dentry *)regs->regs[legacy_signature ? 0 : 2];

    if (!hide_task_match_pid_name(dentry->d_name.name, dentry->d_name.len, DT_DIR)) return 0;

    regs->regs[0] = 0;
    return 1;
}

// pidfd_open 的 x0 指向系统调用现场；隐藏表中的 TGID 按不存在处理。
static int pidfd_open_hook_work(struct pt_regs *regs)
{
    const struct pt_regs *sys_regs = (const struct pt_regs *)(uintptr_t)regs->regs[0];
    if (!sys_regs) return 0;

    unsigned int allowed_flags = PIDFD_NONBLOCK;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    allowed_flags |= PIDFD_THREAD;
#endif
    if ((unsigned int)sys_regs->regs[1] & ~allowed_flags) return 0;
    if (!hide_task_match_pid((pid_t)sys_regs->regs[0])) return 0;

    regs->regs[0] = (unsigned long)-ESRCH;
    return 1;
}

// 隐藏进程的 clock_gettime 探测按不存在的进程处理。
static int clock_gettime_hook_work(struct pt_regs *regs)
{
    const struct pt_regs *sys_regs = (const struct pt_regs *)(uintptr_t)regs->regs[0];
    if (!sys_regs || !hide_task_match_process_clock((int)sys_regs->regs[0])) return 0;

    // 内核对不存在的进程 CPU 时钟返回 -EINVAL，而非 -ESRCH。
    regs->regs[0] = (unsigned long)-EINVAL;
    return 1;
}

static struct hook_entry g_hide_task_hooks[] = {
    HOOK_ENTRY("filldir64", filldir64_hook_work), HOOK_ENTRY("proc_pid_lookup", proc_pid_lookup_hook_work), HOOK_ENTRY("pid_revalidate", pid_revalidate_hook_work), HOOK_ENTRY("__arm64_sys_pidfd_open", pidfd_open_hook_work), HOOK_ENTRY("__arm64_sys_clock_gettime", clock_gettime_hook_work),
};

// 安装 hook，隐藏目标 TGID 对应的整个线程组。
static int hide_task_install(pid_t tgid)
{
    int ret = 0;
    int empty = -1;
    bool has_hidden_tgid = false;

    if (tgid <= 0)
    {
        ls_log_always_tag("task_hide", "install failed: invalid TGID %d\n", tgid);
        return -EINVAL;
    }

    mutex_lock(&g_hide_task_lock);

    for (int i = 0; i < HIDE_TASK_MAX_TGIDS; i++)
    {
        pid_t hidden_tgid = READ_ONCE(g_hidden_tgids[i]);

        if (hidden_tgid == tgid) goto out_unlock;
        if (hidden_tgid) has_hidden_tgid = true;
        else if (empty < 0) empty = i;
    }

    if (empty < 0)
    {
        ret = -ENOSPC;
        ls_log_always_tag("task_hide", "install failed: TGID table full, TGID %d\n", tgid);
        goto out_unlock;
    }

    if (!has_hidden_tgid)
    {
        ret = inline_hook_install(g_hide_task_hooks);
        if (ret)
        {
            ls_log_always_tag("task_hide", "proc PID hooks install failed: %d, TGID %d\n", ret, tgid);
            goto out_unlock;
        }
    }

    // hook 安装成功后再写隐藏表，避免表里有 TGID 但拦截点没生效。
    WRITE_ONCE(g_hidden_tgids[empty], tgid);

out_unlock:
    mutex_unlock(&g_hide_task_lock);
    return ret;
}

// 删除一个隐藏 TGID；如果隐藏表已经空了，就卸载隐藏相关 hook。
static void hide_task_remove(pid_t tgid)
{
    bool has_hidden_tgid = false;
    bool removed = false;

    if (tgid <= 0)
    {
        ls_log_always_tag("task_hide", "remove failed: invalid TGID %d\n", tgid);
        return;
    }

    mutex_lock(&g_hide_task_lock);
    for (int i = 0; i < HIDE_TASK_MAX_TGIDS; i++)
    {
        pid_t hidden_tgid = READ_ONCE(g_hidden_tgids[i]);

        if (hidden_tgid == tgid)
        {
            WRITE_ONCE(g_hidden_tgids[i], 0);
            removed = true;
        }
        else if (hidden_tgid) has_hidden_tgid = true;
    }

    if (removed && !has_hidden_tgid) inline_hook_remove(g_hide_task_hooks);
    mutex_unlock(&g_hide_task_lock);
}

#endif /* HIDE_TASK_H */
