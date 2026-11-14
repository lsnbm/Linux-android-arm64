
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
#include <uapi/linux/pidfd.h>

#include "export_fun.h"
#include "inline_hook_frame.h"
#include "klog/lsdriver_log.h"

#define HIDE_TASK_MAX_PIDS 8

// 隐藏表保存内核侧 pid/tgid 数值；filldir64 里只拿得到目录项 name，所以后面转成字符串比较。
static pid_t g_hidden_pids[HIDE_TASK_MAX_PIDS];
static DEFINE_MUTEX(g_hide_task_lock);

// filldir64 没有 struct file 参数，无法确认当前目录是否为 /proc；这里用 DT_DIR + 精确 PID 名字匹配。
static bool hide_task_match_pid_name(const char *name, int namlen, unsigned int d_type)
{
    char pid_str[16];

    if (d_type != DT_DIR || !name || namlen <= 0 || namlen >= sizeof(pid_str)) return false;

    for (int i = 0; i < HIDE_TASK_MAX_PIDS; i++)
    {
        pid_t hidden_pid = READ_ONCE(g_hidden_pids[i]);

        if (!hidden_pid) continue;

        // name 不是 NUL 结尾字符串，必须按 namlen 做定长比较。
        int pid_len = snprintf(pid_str, sizeof(pid_str), "%d", hidden_pid);
        if (pid_len == namlen && __builtin_memcmp(name, pid_str, namlen) == 0) return true;
    }
    return false;
}

// 进程 CPU 时钟的 clockid 编码为 (~pid << 3) | clock；clock=2 表示进程 CPU 时间。
static bool hide_task_match_process_clock(int clock_id)
{
    if (((unsigned int)clock_id & 7U) != 2U) return false;

    pid_t pid = (pid_t) ~(clock_id >> 3);
    if (pid <= 0) return false;

    for (int i = 0; i < HIDE_TASK_MAX_PIDS; i++)
        if (READ_ONCE(g_hidden_pids[i]) == pid) return true;

    return false;
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

// 使 /proc 根目录下已缓存的隐藏 PID 目录项失效。
static int pid_revalidate_hook_work(struct pt_regs *regs)
{
    bool legacy_signature = __builtin_types_compatible_p(typeof(((struct dentry_operations *)0)->d_revalidate), int (*)(struct dentry *, unsigned int));
    const struct dentry *dentry = (const struct dentry *)regs->regs[legacy_signature ? 0 : 2];

    if (dentry->d_parent != dentry->d_sb->s_root || !hide_task_match_pid_name(dentry->d_name.name, dentry->d_name.len, DT_DIR)) return 0;

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

    pid_t pid = (pid_t)sys_regs->regs[0];
    if (pid <= 0) return 0;

    for (int i = 0; i < HIDE_TASK_MAX_PIDS; i++)
    {
        if (READ_ONCE(g_hidden_pids[i]) != pid) continue;
        regs->regs[0] = (unsigned long)-ESRCH;
        return 1;
    }
    return 0;
}

// 隐藏进程的 clock_gettime 探测按不存在的进程处理。
static int clock_gettime_hook_work(struct pt_regs *regs)
{
    const struct pt_regs *sys_regs = (const struct pt_regs *)(uintptr_t)regs->regs[0];
    if (!sys_regs || !hide_task_match_process_clock((int)sys_regs->regs[0])) return 0;

    regs->regs[0] = (unsigned long)-ESRCH;
    return 1;
}

static struct hook_entry g_hide_task_hooks[] = {
    HOOK_ENTRY("filldir64", filldir64_hook_work), 
    HOOK_ENTRY("proc_pid_lookup", proc_pid_lookup_hook_work),
    HOOK_ENTRY("pid_revalidate", pid_revalidate_hook_work), 
    HOOK_ENTRY("__arm64_sys_pidfd_open", pidfd_open_hook_work), 
    HOOK_ENTRY("__arm64_sys_clock_gettime", clock_gettime_hook_work),
};

// 安装 hook，隐藏目标 pid。
static int hide_task_install(pid_t pid)
{
    int ret = 0;
    int empty = -1;
    bool has_hidden_pid = false;

    if (pid <= 0)
    {
        ls_log_always_tag("task_hide", "install failed: invalid PID %d\n", pid);
        return -EINVAL;
    }

    mutex_lock(&g_hide_task_lock);

    for (int i = 0; i < HIDE_TASK_MAX_PIDS; i++)
    {
        pid_t hidden_pid = READ_ONCE(g_hidden_pids[i]);

        if (hidden_pid == pid) goto out_unlock;
        if (hidden_pid) has_hidden_pid = true;
        else if (empty < 0) empty = i;
    }

    if (empty < 0)
    {
        ret = -ENOSPC;
        ls_log_always_tag("task_hide", "install failed: PID table full, PID %d\n", pid);
        goto out_unlock;
    }

    if (!has_hidden_pid)
    {
        ret = inline_hook_install(g_hide_task_hooks);
        if (ret)
        {
            ls_log_always_tag("task_hide", "proc PID hooks install failed: %d, PID %d\n", ret, pid);
            goto out_unlock;
        }
    }

    // hook 安装成功后再写隐藏表，避免表里有 PID 但拦截点没生效。
    WRITE_ONCE(g_hidden_pids[empty], pid);

out_unlock:
    mutex_unlock(&g_hide_task_lock);
    return ret;
}

// 删除一个隐藏 PID；如果隐藏表已经空了，就卸载隐藏相关 hook。
static void hide_task_remove(pid_t pid)
{
    bool has_hidden_pid = false;
    bool removed = false;

    if (pid <= 0)
    {
        ls_log_always_tag("task_hide", "remove failed: invalid PID %d\n", pid);
        return;
    }

    mutex_lock(&g_hide_task_lock);
    for (int i = 0; i < HIDE_TASK_MAX_PIDS; i++)
    {
        pid_t hidden_pid = READ_ONCE(g_hidden_pids[i]);

        if (hidden_pid == pid)
        {
            WRITE_ONCE(g_hidden_pids[i], 0);
            removed = true;
        }
        else if (hidden_pid) has_hidden_pid = true;
    }

    if (removed && !has_hidden_pid) inline_hook_remove(g_hide_task_hooks);
    mutex_unlock(&g_hide_task_lock);
}

#endif /* HIDE_TASK_H */
