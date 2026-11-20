
/*
外部进程对隐藏线程组的探测手段：
1. /proc：getdents64 枚举 PID 目录，直接查找或复验已缓存的 PID 目录项。
2. pidfd：pidfd_open 按 PID 获取进程句柄。
3. CPU 时钟：clock_gettime/clock_getres 查询进程 PROF、VIRT、SCHED 时钟。
4. 优先级：getpriority(PRIO_PROCESS, who) 按 PID 查询 nice 值。
5. cgroup：读取 v1 的 cgroup.procs/tasks 或 v2 的 cgroup.procs/cgroup.threads。

统一以 TGID 表 g_hidden_tgids 为隐藏依据：
- 各 hook 收到线程 PID 后，在 RCU 保护下通过 find_task_by_vpid() 查找任务，
  再读取 task->tgid 匹配隐藏表，因此主线程及同一线程组内其他线程的 PID 都会被隐藏。
- 安装和删除接口接收 TGID；隐藏表只保存 TGID，不逐个保存线程 PID。

各 hook 的隐藏方式：
1. filldir64
   getdents64 的 procfs 枚举最终通过 ctx.actor 调用 filldir64。按 DT_DIR + 数字目录名
   解析 PID 并匹配 TGID；命中时跳过原函数，但返回“当前项处理成功”：6.1+ 返回 true，
   5.10/5.15 返回 0。这样当前 PID 不写入用户缓冲，同时继续枚举其他目录项。
   不临时替换 ctx->actor，避免并发枚举时 actor 槽位绑定错乱，也不依赖 proc_fill_cache。
2. proc_pid_lookup
   直接查找隐藏 PID 对应的 /proc 目录时，跳过原函数并返回 ERR_PTR(-ENOENT)。
3. pid_revalidate
   已缓存的 PID 目录项按 TGID 复验；命中时返回 0 使 dentry 失效，随后由
   proc_pid_lookup 返回 ENOENT。不再限制必须是 proc 根目录的直接子项。
4. __arm64_sys_pidfd_open
   从系统调用现场取得 PID，参数 flags 合法且 TGID 命中时返回 -ESRCH；
   非法 flags、无效 PID 等仍交给原系统调用处理。
5. __arm64_sys_clock_gettime / __arm64_sys_clock_getres
   识别 clockid 低三位为 2 的进程 CPU 时钟，从编码中还原 PID 并匹配 TGID。
   命中 PROF、VIRT、SCHED 时均按原生不存在进程的行为返回 -EINVAL。
6. __arm64_sys_getpriority
   只处理 PRIO_PROCESS 且 who 非零的查询；TGID 命中时返回原生不存在进程的 -ESRCH，
   who 为零的当前进程查询以及 PRIO_PGRP/PRIO_USER 保持原行为。
7. cgroup_seqfile_show
   这是四个目标 cgroup 文件共用的 seq show 包装，只过滤指定文件，不影响其他文件。
   v1 的 arg 是 PID 列表元素：tasks 按 PID 查任务，cgroup.procs 按 TGID 对象身份比较。
   v2 的 arg 是 task_struct：直接读取其 TGID；通过 cftype 的层级标志区分参数类型。
   命中时不调用原 show，返回 0 且不输出字节；seq_file 继续原生 next，不留下空行或
   分页占位，不需要替换 PID 列表、任务迭代器或修改缓存。

限制：
- 32 位 compat 路径的 compat_filldir 未覆盖。
- filldir64 没有 struct file，无法确认当前目录是否为 /proc；按 DT_DIR + 数字 PID 名
  匹配意味着其他目录下同名的数字目录也会被隐藏。
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
#include <linux/cgroup.h>
#include <linux/kernfs.h>
#include <linux/seq_file.h>
#include <linux/pid_namespace.h>
#include <uapi/linux/pidfd.h>
#include <uapi/linux/resource.h>

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

// PROF、VIRT、SCHED 的 clock_gettime/clock_getres 都按不存在的进程处理。
static int hide_task_cpu_clock_hook_work(struct pt_regs *regs)
{
    const struct pt_regs *sys_regs = (const struct pt_regs *)(uintptr_t)regs->regs[0];
    if (!sys_regs || !hide_task_match_process_clock((int)sys_regs->regs[0])) return 0;

    regs->regs[0] = (unsigned long)-EINVAL;
    return 1;
}

// PRIO_PROCESS 的 who 是 PID；原生没有目标任务时返回 -ESRCH，而不是 nice 值 20。
static int getpriority_hook_work(struct pt_regs *regs)
{
    const struct pt_regs *sys_regs = (const struct pt_regs *)(uintptr_t)regs->regs[0];
    if (!sys_regs || sys_regs->regs[0] != PRIO_PROCESS) return 0;

    pid_t who = (pid_t)sys_regs->regs[1];
    if (!who || !hide_task_match_pid(who)) return 0;

    regs->regs[0] = (unsigned long)-ESRCH;
    return 1;
}

// v1 cgroup.procs 保存 TGID 而非线程 PID；按命名空间中的 TGID 对象身份比较。
static bool hide_task_match_cgroup_tgid(pid_t tgid)
{
    bool matched = false;

    if (tgid <= 0) return false;

    rcu_read_lock();
    struct pid *pid = find_vpid(tgid);
    if (pid)
    {
        for (int i = 0; i < HIDE_TASK_MAX_TGIDS; i++)
        {
            pid_t hidden_tgid = READ_ONCE(g_hidden_tgids[i]);
            if (hidden_tgid > 0 && pid == find_pid_ns(hidden_tgid, &init_pid_ns))
            {
                matched = true;
                break;
            }
        }
    }
    rcu_read_unlock();

    return matched;
}

static bool hide_task_match_cgroup_file(struct seq_file *seq)
{
    const struct cftype *cft = seq_cft(seq);
    const char *name = cft->name;

    return __builtin_strcmp(name, "cgroup.procs") == 0 || __builtin_strcmp(name, "tasks") == 0 || __builtin_strcmp(name, "cgroup.threads") == 0;
}

// v1/v2 共用此入口：v1 的 arg 是 PID 列表元素，v2 的 arg 是任务。
static int cgroup_seqfile_show_hook_work(struct pt_regs *regs)
{
    struct seq_file *seq = (struct seq_file *)regs->regs[0];
    void *arg = (void *)regs->regs[1];
    bool hidden;

    if (!hide_task_match_cgroup_file(seq)) return 0;

    const struct cftype *cft = seq_cft(seq);
    // v2 文件由 cgroup_add_dfl_cftypes() 设置该标志；v1 文件使用 PID 列表。
    if (cft->flags & __CFTYPE_ONLY_ON_DFL)
        hidden = hide_task_match_tgid(READ_ONCE(((struct task_struct *)arg)->tgid));
    else if (__builtin_strcmp(cft->name, "cgroup.procs") == 0)
        hidden = hide_task_match_cgroup_tgid(*(pid_t *)arg);
    else
        hidden = hide_task_match_pid(*(pid_t *)arg);
    if (!hidden) return 0;

    // 不调用原 show，不输出字节；seq_file 会继续调用原生 next，分页位置只统计实际输出。
    regs->regs[0] = 0;
    return 1;
}

static struct hook_entry g_hide_task_hooks[] = {
    HOOK_ENTRY("filldir64", filldir64_hook_work),
    HOOK_ENTRY("proc_pid_lookup", proc_pid_lookup_hook_work),
    HOOK_ENTRY("pid_revalidate", pid_revalidate_hook_work),
    HOOK_ENTRY("__arm64_sys_pidfd_open", pidfd_open_hook_work),
    HOOK_ENTRY("__arm64_sys_clock_gettime", hide_task_cpu_clock_hook_work),
    HOOK_ENTRY("__arm64_sys_clock_getres", hide_task_cpu_clock_hook_work),
    HOOK_ENTRY("__arm64_sys_getpriority", getpriority_hook_work),
    HOOK_ENTRY("cgroup_seqfile_show", cgroup_seqfile_show_hook_work),
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
