/*
外部进程对隐藏线程组的探测手段：
1. /proc：getdents64 枚举 PID 目录，直接查找或复验已缓存的 PID 目录项。
2. pidfd：pidfd_open 按 PID 获取进程句柄。
3. CPU 时钟：clock_gettime/clock_getres 查询进程 PROF、VIRT、SCHED 时钟，
   timer_create 创建绑定这些进程 CPU 时钟的未启动定时器。
4. 优先级：getpriority(PRIO_PROCESS, who) 按 PID 查询 nice 值。
5. cgroup：读取 v1 的 cgroup.procs/tasks 或 v2 的 cgroup.procs/cgroup.threads。
6. fcntl：私有文件的 F_SETOWN/F_GETOWN，以及 F_OWNER_PID/F_OWNER_TID 的设置/读回。
7. hwbinder：BINDER_GET_FROZEN_INFO 按 PID 查询驱动中的注册对象。

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
   clockid 的 bit 2 为零表示进程时钟，低两位区分 PROF=0、VIRT=1、SCHED=2；
   从编码中还原 PID 并匹配 TGID，三种时钟均按原生不存在进程的行为返回 -EINVAL。
6. __arm64_sys_getpriority
   只处理 PRIO_PROCESS 且 who 非零的查询；TGID 命中时返回原生不存在进程的 -ESRCH，
   who 为零的当前进程查询以及 PRIO_PGRP/PRIO_USER 保持原行为。
7. cgroup_seqfile_show
   这是四个目标 cgroup 文件共用的 seq show 包装，只过滤指定文件，不影响其他文件。
   v1 的 arg 是 PID 列表元素：tasks 按 PID 查任务，cgroup.procs 按 TGID 对象身份比较。
   v2 的 arg 是 task_struct：直接读取其 TGID；按目标 cftype 是否使用 pidlist 区分参数类型。
   命中时不调用原 show，返回 0 且不输出字节；seq_file 继续原生 next，不留下空行或
   分页占位，不需要替换 PID 列表、任务迭代器或修改缓存。
8. posix_cpu_timer_create
   timer_create 的 PROF/VIRT/SCHED 进程 CPU 时钟复用相同的 clockid 解码；命中隐藏
   TGID 时返回 -EINVAL，使外层 do_timer_create 按原生失败路径回收定时器对象。
   不提前跳过整个 timer_create，保留其 ID 分配、created_timer_id 写入和失败清理顺序。
9. security_file_fcntl
   原生及 compat fcntl/fcntl64 均在持有 fd 引用后调用此函数，统一获得 file/cmd/arg。
   普通 F_SETOWN/F_GETOWN 使用 PIDTYPE_TGID；F_OWNER_PID/F_OWNER_TID 分别使用
   PIDTYPE_TGID/PIDTYPE_PID。设置隐藏 owner 时返回 -ESRCH；读回时返回原生空 owner：
   F_GETOWN 为 0，F_GETOWN_EX 为保留原 type 的 pid=0 结构。
   F_OWNER_PGRP 不是本次探测类型，不将 PGID 数值当 PID；F_GETOWNER_UIDS 不返回 PID，
   二者均保持原生行为。设置时按 struct pid 对象类型解析；读回时在文件 owner 锁和 RCU
   内解析保存的对象，不依赖 PID 数值全局匹配，也不访问目标进程自己的文件描述符。
   未隐藏目标继续执行原生 LSM 分派；隐藏目标跳过剩余 LSM 分派和 do_fcntl，由外层
   按正常路径释放 fd 引用。无需重复 fget/fput，也不依赖可能被内联的静态 do_fcntl。
10. binder_ioctl
   仅处理 BINDER_GET_FROZEN_INFO，不拦截会修改状态的 BINDER_FREEZE。
   先按 UAPI 结构和 _IOC 大小校验参数，命中隐藏 PID 时按 binder_proc 未找到返回
   -EINVAL，其他命令及未隐藏 PID 继续原生执行；这不等同于证明 Binder 对象仍在使用。

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

// 普通 PID/TID 均按任务所属线程组匹配；不把进程组编号当作 PID。
static bool hide_task_match_pid_type(pid_t pid, enum pid_type type)
{
    bool matched = false;

    if (pid <= 0 || (type != PIDTYPE_PID && type != PIDTYPE_TGID)) return false;

    rcu_read_lock();
    struct task_struct *task = pid_task(find_vpid(pid), type);
    if (task) matched = hide_task_match_tgid(READ_ONCE(task->tgid));
    rcu_read_unlock();

    return matched;
}

// 与 f_getown/f_getown_ex 相同，持锁防止并发设置更换 PID 对象，并快照读回所需类型。
static bool hide_task_match_f_owner(struct fown_struct *f_owner, enum pid_type *pid_type)
{
    bool matched = false;

    read_lock_irq(&f_owner->lock);
    *pid_type = f_owner->pid_type;
    if (*pid_type == PIDTYPE_TGID || *pid_type == PIDTYPE_PID)
    {
        rcu_read_lock();
        struct task_struct *task = pid_task(f_owner->pid, *pid_type);
        if (task) matched = hide_task_match_tgid(READ_ONCE(task->tgid));
        rcu_read_unlock();
    }
    read_unlock_irq(&f_owner->lock);

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

// 进程 CPU 时钟的 bit 2 为零；低两位 PROF=0、VIRT=1、SCHED=2 都是有效时钟类型。
static bool hide_task_match_process_clock(int clock_id)
{
    unsigned int type = (unsigned int)clock_id & 3U;

    if (((unsigned int)clock_id & 4U) || type > 2U) return false;

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

// timer_create 在此已分配定时器 ID；拒绝 CPU 时钟初始化后由原生失败出口释放对象。
static int hide_task_cpu_timer_create_hook_work(struct pt_regs *regs)
{
    const struct k_itimer *timer = (const struct k_itimer *)regs->regs[0];
    if (!timer || !hide_task_match_process_clock(timer->it_clock)) return 0;

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

static bool hide_task_match_cgroup_file(const struct cftype *cft)
{
    const char *name = cft->name;

    return __builtin_strcmp(name, "cgroup.procs") == 0 || __builtin_strcmp(name, "tasks") == 0 || __builtin_strcmp(name, "cgroup.threads") == 0;
}

// v1/v2 共用此入口：v1 的 arg 是 PID 列表元素，v2 的 arg 是任务。
static int cgroup_seqfile_show_hook_work(struct pt_regs *regs)
{
    struct seq_file *seq = (struct seq_file *)regs->regs[0];
    void *arg = (void *)regs->regs[1];
    const struct cftype *cft = seq_cft(seq);
    bool hidden;

    if (!hide_task_match_cgroup_file(cft)) return 0;

    /*
    当前使用 cft->seq_stop 区分参数类型：它读取目标 cftype 已有字段
    v1 的 cgroup.procs/tasks 通过 cgroup_pidlist_start/next/stop 遍历 PID 数组，
    arg 是 pid_t 数组元素指针；procs 保存 TGID，tasks 保存线程 PID，两者均设置 seq_stop。
    v2 的 cgroup.procs/cgroup.threads 通过 css_task_iter_next() 遍历任务，
    arg 是 task_struct *，procs 遍历线程组 leader，threads 遍历线程，两者均不设置 seq_stop。
    因此 !cft->seq_stop 时按任务读取 TGID，否则按 pid_t 解引用，再保留 v1 两种文件的匹配方式。
    不能用 __CFTYPE_ONLY_ON_DFL：核心 v2 文件直接由 cgroup_init_cftypes() 初始化，没有该标志。
    不能直接引用 cgrp_dfl_root：它会新增模块导入，KMI 符号表中没有时设备报 Unknown symbol。
    */
    if (!cft->seq_stop) hidden = hide_task_match_tgid(READ_ONCE(((struct task_struct *)arg)->tgid));
    else if (__builtin_strcmp(cft->name, "cgroup.procs") == 0) hidden = hide_task_match_cgroup_tgid(*(pid_t *)arg);
    else hidden = hide_task_match_pid(*(pid_t *)arg);
    if (!hidden) return 0;

    // 不调用原 show，不输出字节；seq_file 会继续调用原生 next，分页位置只统计实际输出。
    regs->regs[0] = 0;
    return 1;
}

// 参数由入口适配器按各原生函数签名传入；返回值 0/1 继续表示执行/跳过原函数。
static int hide_task_fcntl_owner(struct pt_regs *regs, struct file *file, unsigned int cmd, unsigned long arg)
{
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
            // 此处尚未进入内核原子区，与原生 f_setown_ex 一样允许用户页缺页。
            if (copy_from_user(&owner, (void __user *)arg, sizeof(owner)))
            {
                regs->regs[0] = (unsigned long)-EFAULT;
                return 1;
            }

            if (owner.type == F_OWNER_PID) pid_type = PIDTYPE_TGID;
            else if (owner.type == F_OWNER_TID) pid_type = PIDTYPE_PID;
            else return 0; // 非本次 F_OWNER_PID/TID 探测，保留 PGRP 和非法 type 的原生处理。
        }

        pid_t pid = cmd == F_SETOWN ? (pid_t)arg : owner.pid;
        if (!hide_task_match_pid_type(pid, pid_type)) return 0;

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

    hidden = hide_task_match_f_owner(f_owner, &pid_type);
    if (!hidden) return 0; // 非目标 PID/TID、PGRP 和失效 PID 均保留原生处理。

    if (cmd == F_GETOWN) regs->regs[0] = 0;
    else
    {
        // 与 f_getown_ex 一致：空 owner 保留已快照类型、pid=0，复制允许缺页。
        owner.type = pid_type == PIDTYPE_TGID ? F_OWNER_PID : F_OWNER_TID;
        regs->regs[0] = copy_to_user((void __user *)arg, &owner, sizeof(owner)) ? (unsigned long)-EFAULT : 0;
    }
    return 1;
}

// security_file_fcntl(file, cmd, arg)：调用者已持有 fd 引用，原生与 compat fcntl 共用此入口。
static int hide_task_security_fcntl_hook_work(struct pt_regs *regs)
{
    return hide_task_fcntl_owner(regs, (struct file *)regs->regs[0], (unsigned int)regs->regs[1], regs->regs[2]);
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

static struct hook_entry g_hide_task_hooks[] = {
    HOOK_ENTRY("filldir64", filldir64_hook_work), 
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
