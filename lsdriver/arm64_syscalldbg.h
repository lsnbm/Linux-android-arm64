/*
指定进程 syscall 入口监控
*/
#ifndef ARM64_SYSCALLDBG_H
#define ARM64_SYSCALLDBG_H

#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include <asm/ptrace.h>
#include <uapi/asm/unistd.h>

#include "export_fun.h"
#include "inline_hook_frame.h"
#include "lsdriver_log.h"

// 最多同时监控 8 个进程组；按 tgid 匹配会覆盖目标进程的所有线程。
#define ARM_SYSCALL_MONITOR_MAX_PIDS 8
// 用户路径和字符串只做有限预览，避免恶意超长字符串占满内核日志。
#define SYSCALL_MONITOR_PATH_PREVIEW 128
// write/send 等输入数据最多打印前 32 字节，剩余数据使用 "..." 表示。
#define SYSCALL_MONITOR_DATA_PREVIEW 32
// 单条语义化日志的临时缓冲区上限，不允许参数展开无限增长。
#define SYSCALL_MONITOR_LOG_SIZE 768

// syscall 监控目标表保存进程 tgid；0 表示空槽。
static pid_t g_syscall_monitor_pids[ARM_SYSCALL_MONITOR_MAX_PIDS];
static DEFINE_MUTEX(g_syscall_monitor_lock);

struct syscall_monitor_name
{
    // __SYSCALL 的编号参数文本，例如 "__NR_openat"；包装宏展开后也可能只是 "56"。
    const char *nr_name;
    // 目标内核处理函数文本，例如 "sys_openat"，用于编号宏名丢失时回退取名。
    const char *fn_name;
};

/*
复用目标内核的 ARM64 syscall 表生成名称映射：普通 __SYSCALL 项从
__NR_* 宏名提取名称；经 __SC_COMP/__SC_3264 展开的项若只剩数字，
则从 sys_* 处理函数名提取名称。这里不表示已支持 32 位 compat syscall。
*/
#undef __SYSCALL
#define __SYSCALL(nr, call) [nr] = {#nr, #call},
static const struct syscall_monitor_name g_syscall_monitor_names[__NR_syscalls] = {
#include <uapi/asm/unistd.h>
};
#undef __SYSCALL
#define __SYSCALL(nr, call)

// 判断 syscall 监控表是否还有目标，空表时可卸载 do_el0_svc hook。
static bool syscall_monitor_has_pid(void)
{
    int i;

    for (i = 0; i < ARM_SYSCALL_MONITOR_MAX_PIDS; i++)
        if (READ_ONCE(g_syscall_monitor_pids[i])) return true;
    return false;
}

// 判断当前 task 是否属于需要监控 syscall 的目标进程。
static bool syscall_monitor_should_trace(void)
{
    int i;

    for (i = 0; i < ARM_SYSCALL_MONITOR_MAX_PIDS; i++)
    {
        pid_t target_tgid = READ_ONCE(g_syscall_monitor_pids[i]);

        if (target_tgid && current->tgid == target_tgid) return true;
    }
    return false;
}

// 将 arm64 syscall 号转换成由目标内核 syscall 表生成的可读名称。
static const char *syscall_monitor_name(long scno)
{
    const struct syscall_monitor_name *entry;

    if ((unsigned long)scno >= ARRAY_SIZE(g_syscall_monitor_names)) return "unknown";

    entry = &g_syscall_monitor_names[scno];
    if (!entry->fn_name) return "unknown";

    // 这两个 64 位兼容包装的处理函数名与用户态 syscall 名不同。
#ifdef __NR_fstat
    if (scno == __NR_fstat) return "fstat";
#endif
#ifdef __NR_fadvise64
    if (scno == __NR_fadvise64) return "fadvise64";
#endif

    if (entry->nr_name[0] == '_') return entry->nr_name + (entry->nr_name[4] == '_' ? sizeof("__NR_") - 1 : sizeof("__NR3264_") - 1);

    return entry->fn_name + sizeof("sys_") - 1;
}

/*
从当前进程用户地址空间复制一个 NUL 结尾字符串到日志。
strncpy_from_user 会安全处理不可访问的用户地址；失败时仅输出 <fault>，
不会直接用 %s 解引用用户指针。控制字符替换为 '.'，避免伪造多行内核日志。
*/
static void syscall_monitor_append_user_string(char *text, size_t size, size_t *pos, const char *label, unsigned long addr)
{
    char value[SYSCALL_MONITOR_PATH_PREVIEW];
    long copied;
    int i;

    if (*pos >= size) return;
    if (!addr)
    {
        *pos += scnprintf(text + *pos, size - *pos, " %s=NULL", label);
        return;
    }

    copied = strncpy_from_user(value, (const char __user *)(uintptr_t)addr, sizeof(value) - 1);
    if (copied < 0)
    {
        *pos += scnprintf(text + *pos, size - *pos, " %s=<fault>", label);
        return;
    }

    value[min_t(size_t, copied, sizeof(value) - 1)] = '\0';
    for (i = 0; value[i]; i++)
        if ((unsigned char)value[i] < 0x20 || value[i] == 0x7f) value[i] = '.';

    *pos += scnprintf(text + *pos, size - *pos, " %s=\"%s\"%s", label, value, copied >= sizeof(value) - 1 ? "..." : "");
}

/*
预览 write/pwrite64/sendto 等 syscall 的输入缓冲区。
入口时这些数据已经由用户态准备好，可以安全复制；只复制固定上限，
既控制 hook 延迟，也避免大块 I/O 生成同等大小的 printk 日志。
*/
static void syscall_monitor_append_data(char *text, size_t size, size_t *pos, const char *label, unsigned long addr, size_t length)
{
    u8 data[SYSCALL_MONITOR_DATA_PREVIEW];
    size_t preview = min_t(size_t, length, sizeof(data));
    size_t i;

    if (*pos >= size || !length) return;
    if (!addr)
    {
        *pos += scnprintf(text + *pos, size - *pos, " %s=NULL", label);
        return;
    }
    if (copy_from_user(data, (const void __user *)(uintptr_t)addr, preview))
    {
        *pos += scnprintf(text + *pos, size - *pos, " %s=<fault>", label);
        return;
    }

    *pos += scnprintf(text + *pos, size - *pos, " %s=", label);
    for (i = 0; i < preview && *pos < size; i++) *pos += scnprintf(text + *pos, size - *pos, "%02x", data[i]);
    if (preview < length && *pos < size) *pos += scnprintf(text + *pos, size - *pos, "...");
}

// 各大类都复用这几个格式化动作，但 syscall 识别和控制流保留在各自函数内。
#define SM_ADD(fmt, ...)                                                                              \
    do                                                                                                \
    {                                                                                                 \
        if (pos < sizeof(text)) pos += scnprintf(text + pos, sizeof(text) - pos, fmt, ##__VA_ARGS__); \
    } while (0)
#define SM_ARG(label, reg)               SM_ADD(" %s=0x%llx", label, (unsigned long long)regs->regs[reg])
#define SM_STR(label, reg)               syscall_monitor_append_user_string(text, sizeof(text), &pos, label, regs->regs[reg])
#define SM_DATA(label, ptr_reg, len_reg) syscall_monitor_append_data(text, sizeof(text), &pos, label, regs->regs[ptr_reg], regs->regs[len_reg])

static void syscall_monitor_emit(struct task_struct *task, long scno, const char *text)
{
    ls_log_always_tag("sysmon", "tgid=%d pid=%d comm=%s syscall=%ld(%s)%s\n", task->tgid, task->pid, task->comm, scno, syscall_monitor_name(scno), text);
}

// 文件和普通 I/O：路径、偏移、输入数据预览均在本函数内完成。
static bool syscall_monitor_handle_file(struct task_struct *task, struct pt_regs *regs, long scno)
{
    char text[SYSCALL_MONITOR_LOG_SIZE] = {0};
    size_t pos = 0;

    switch (scno)
    {
#ifdef __NR_read
    case __NR_read:
        SM_ARG("fd", 0);
        SM_ARG("buf", 1);
        SM_ARG("count", 2);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_write
    case __NR_write:
        SM_ARG("fd", 0);
        SM_ARG("buf", 1);
        SM_ARG("count", 2);
        SM_DATA("data", 1, 2);
        break;
#endif
#ifdef __NR_readv
    case __NR_readv:
        SM_ARG("fd", 0);
        SM_ARG("iov", 1);
        SM_ARG("iovcnt", 2);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_writev
    case __NR_writev:
        SM_ARG("fd", 0);
        SM_ARG("iov", 1);
        SM_ARG("iovcnt", 2);
        break;
#endif
#ifdef __NR_pread64
    case __NR_pread64:
        SM_ARG("fd", 0);
        SM_ARG("buf", 1);
        SM_ARG("count", 2);
        SM_ARG("offset", 3);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_pwrite64
    case __NR_pwrite64:
        SM_ARG("fd", 0);
        SM_ARG("buf", 1);
        SM_ARG("count", 2);
        SM_ARG("offset", 3);
        SM_DATA("data", 1, 2);
        break;
#endif
#ifdef __NR_openat
    case __NR_openat:
        SM_ARG("dfd", 0);
        SM_ARG("filename", 1);
        SM_ARG("flags", 2);
        SM_ARG("mode", 3);
        SM_STR("path", 1);
        break;
#endif
#ifdef __NR_close
    case __NR_close:
        SM_ARG("fd", 0);
        break;
#endif
#ifdef __NR_lseek
    case __NR_lseek:
    {
        const char *whence = regs->regs[2] == 0 ? "SEEK_SET" : regs->regs[2] == 1 ? "SEEK_CUR" : regs->regs[2] == 2 ? "SEEK_END" : "unknown";

        SM_ARG("fd", 0);
        SM_ARG("offset", 1);
        SM_ARG("whence", 2);
        SM_ADD(" offset_signed=%lld whence_name=%s", (long long)regs->regs[1], whence);
        break;
    }
#endif
#ifdef __NR_ioctl
    case __NR_ioctl:
        SM_ARG("fd", 0);
        SM_ARG("cmd", 1);
        SM_ARG("arg", 2);
        break;
#endif
#ifdef __NR_fstat
    case __NR_fstat:
        SM_ARG("fd", 0);
        SM_ARG("statbuf", 1);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_newfstatat
    case __NR_newfstatat:
        SM_ARG("dfd", 0);
        SM_ARG("filename", 1);
        SM_ARG("statbuf", 2);
        SM_ARG("flags", 3);
        SM_STR("path", 1);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_getdents64
    case __NR_getdents64:
        SM_ARG("fd", 0);
        SM_ARG("dirent", 1);
        SM_ARG("count", 2);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_unlinkat
    case __NR_unlinkat:
        SM_ARG("dfd", 0);
        SM_ARG("pathname", 1);
        SM_ARG("flags", 2);
        SM_STR("path", 1);
        break;
#endif
#ifdef __NR_renameat
    case __NR_renameat:
        SM_ARG("olddfd", 0);
        SM_ARG("oldname", 1);
        SM_ARG("newdfd", 2);
        SM_ARG("newname", 3);
        SM_STR("oldpath", 1);
        SM_STR("newpath", 3);
        break;
#endif
#ifdef __NR_renameat2
    case __NR_renameat2:
        SM_ARG("olddfd", 0);
        SM_ARG("oldname", 1);
        SM_ARG("newdfd", 2);
        SM_ARG("newname", 3);
        SM_ARG("flags", 4);
        SM_STR("oldpath", 1);
        SM_STR("newpath", 3);
        break;
#endif
    default:
        return false;
    }

    syscall_monitor_emit(task, scno, text);
    return true;
}

// 内存：只解释地址、长度和策略；输出型跨进程读取留待 syscall 退出处理。
static bool syscall_monitor_handle_memory(struct task_struct *task, struct pt_regs *regs, long scno)
{
    char text[SYSCALL_MONITOR_LOG_SIZE] = {0};
    size_t pos = 0;

    switch (scno)
    {
#ifdef __NR_mmap
    case __NR_mmap:
        SM_ARG("addr", 0);
        SM_ARG("length", 1);
        SM_ARG("prot", 2);
        SM_ARG("flags", 3);
        SM_ARG("fd", 4);
        SM_ARG("offset", 5);
        break;
#endif
#ifdef __NR_munmap
    case __NR_munmap:
        SM_ARG("addr", 0);
        SM_ARG("length", 1);
        break;
#endif
#ifdef __NR_mprotect
    case __NR_mprotect:
        SM_ARG("addr", 0);
        SM_ARG("length", 1);
        SM_ARG("prot", 2);
        break;
#endif
#ifdef __NR_mremap
    case __NR_mremap:
        SM_ARG("old_addr", 0);
        SM_ARG("old_size", 1);
        SM_ARG("new_size", 2);
        SM_ARG("flags", 3);
        SM_ARG("new_addr", 4);
        break;
#endif
#ifdef __NR_brk
    case __NR_brk:
        SM_ARG("addr", 0);
        break;
#endif
#ifdef __NR_madvise
    case __NR_madvise:
        SM_ARG("addr", 0);
        SM_ARG("length", 1);
        SM_ARG("advice", 2);
        break;
#endif
#ifdef __NR_futex
    case __NR_futex:
        SM_ARG("uaddr", 0);
        SM_ARG("op", 1);
        SM_ARG("val", 2);
        SM_ARG("timeout", 3);
        SM_ARG("uaddr2", 4);
        SM_ARG("val3", 5);
        break;
#endif
#ifdef __NR_process_vm_readv
    case __NR_process_vm_readv:
        SM_ARG("pid", 0);
        SM_ARG("lvec", 1);
        SM_ARG("liovcnt", 2);
        SM_ARG("rvec", 3);
        SM_ARG("riovcnt", 4);
        SM_ARG("flags", 5);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_process_vm_writev
    case __NR_process_vm_writev:
        SM_ARG("pid", 0);
        SM_ARG("lvec", 1);
        SM_ARG("liovcnt", 2);
        SM_ARG("rvec", 3);
        SM_ARG("riovcnt", 4);
        SM_ARG("flags", 5);
        break;
#endif
    default:
        return false;
    }

    syscall_monitor_emit(task, scno, text);
    return true;
}

// 进程：创建、执行、信号和进程控制在一个直线 switch 中处理。
static bool syscall_monitor_handle_process(struct task_struct *task, struct pt_regs *regs, long scno)
{
    char text[SYSCALL_MONITOR_LOG_SIZE] = {0};
    size_t pos = 0;

    switch (scno)
    {
#ifdef __NR_clone
    case __NR_clone:
        SM_ARG("flags", 0);
        SM_ARG("newsp", 1);
        SM_ARG("parent_tid", 2);
        SM_ARG("child_tid", 3);
        SM_ARG("tls", 4);
        break;
#endif
#ifdef __NR_clone3
    case __NR_clone3:
        SM_ARG("args", 0);
        SM_ARG("size", 1);
        break;
#endif
#ifdef __NR_execve
    case __NR_execve:
        SM_ARG("filename", 0);
        SM_ARG("argv", 1);
        SM_ARG("envp", 2);
        SM_STR("path", 0);
        break;
#endif
#ifdef __NR_execveat
    case __NR_execveat:
        SM_ARG("dfd", 0);
        SM_ARG("filename", 1);
        SM_ARG("argv", 2);
        SM_ARG("envp", 3);
        SM_ARG("flags", 4);
        SM_STR("path", 1);
        break;
#endif
#ifdef __NR_kill
    case __NR_kill:
        SM_ARG("pid", 0);
        SM_ARG("sig", 1);
        break;
#endif
#ifdef __NR_tgkill
    case __NR_tgkill:
        SM_ARG("tgid", 0);
        SM_ARG("pid", 1);
        SM_ARG("sig", 2);
        break;
#endif
#ifdef __NR_ptrace
    case __NR_ptrace:
        SM_ARG("request", 0);
        SM_ARG("pid", 1);
        SM_ARG("addr", 2);
        SM_ARG("data", 3);
        break;
#endif
#ifdef __NR_prctl
    case __NR_prctl:
        SM_ARG("option", 0);
        SM_ARG("arg2", 1);
        SM_ARG("arg3", 2);
        SM_ARG("arg4", 3);
        SM_ARG("arg5", 4);
        break;
#endif
    default:
        return false;
    }

    syscall_monitor_emit(task, scno, text);
    return true;
}

// 网络：发送缓冲区可在入口预览，接收缓冲区只标记为退出后可用。
static bool syscall_monitor_handle_network(struct task_struct *task, struct pt_regs *regs, long scno)
{
    char text[SYSCALL_MONITOR_LOG_SIZE] = {0};
    size_t pos = 0;

    switch (scno)
    {
#ifdef __NR_socket
    case __NR_socket:
        SM_ARG("domain", 0);
        SM_ARG("type", 1);
        SM_ARG("protocol", 2);
        break;
#endif
#ifdef __NR_socketpair
    case __NR_socketpair:
        SM_ARG("domain", 0);
        SM_ARG("type", 1);
        SM_ARG("protocol", 2);
        SM_ARG("sv", 3);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_bind
    case __NR_bind:
        SM_ARG("fd", 0);
        SM_ARG("addr", 1);
        SM_ARG("addrlen", 2);
        break;
#endif
#ifdef __NR_connect
    case __NR_connect:
        SM_ARG("fd", 0);
        SM_ARG("addr", 1);
        SM_ARG("addrlen", 2);
        break;
#endif
#ifdef __NR_listen
    case __NR_listen:
        SM_ARG("fd", 0);
        SM_ARG("backlog", 1);
        break;
#endif
#ifdef __NR_accept
    case __NR_accept:
        SM_ARG("fd", 0);
        SM_ARG("addr", 1);
        SM_ARG("addrlen", 2);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_accept4
    case __NR_accept4:
        SM_ARG("fd", 0);
        SM_ARG("addr", 1);
        SM_ARG("addrlen", 2);
        SM_ARG("flags", 3);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_sendto
    case __NR_sendto:
        SM_ARG("fd", 0);
        SM_ARG("buf", 1);
        SM_ARG("length", 2);
        SM_ARG("flags", 3);
        SM_ARG("addr", 4);
        SM_ARG("addrlen", 5);
        SM_DATA("data", 1, 2);
        break;
#endif
#ifdef __NR_recvfrom
    case __NR_recvfrom:
        SM_ARG("fd", 0);
        SM_ARG("buf", 1);
        SM_ARG("length", 2);
        SM_ARG("flags", 3);
        SM_ARG("addr", 4);
        SM_ARG("addrlen", 5);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_sendmsg
    case __NR_sendmsg:
        SM_ARG("fd", 0);
        SM_ARG("msg", 1);
        SM_ARG("flags", 2);
        break;
#endif
#ifdef __NR_recvmsg
    case __NR_recvmsg:
        SM_ARG("fd", 0);
        SM_ARG("msg", 1);
        SM_ARG("flags", 2);
        SM_ADD(" output=available_on_exit");
        break;
#endif
#ifdef __NR_shutdown
    case __NR_shutdown:
        SM_ARG("fd", 0);
        SM_ARG("how", 1);
        break;
#endif
    default:
        return false;
    }

    syscall_monitor_emit(task, scno, text);
    return true;
}

// 内核模块：模块名、参数字符串和有限镜像预览均在此处完成。
static bool syscall_monitor_handle_module(struct task_struct *task, struct pt_regs *regs, long scno)
{
    char text[SYSCALL_MONITOR_LOG_SIZE] = {0};
    size_t pos = 0;

    switch (scno)
    {
#ifdef __NR_finit_module
    case __NR_finit_module:
        SM_ARG("fd", 0);
        SM_ARG("params", 1);
        SM_ARG("flags", 2);
        SM_STR("params_text", 1);
        break;
#endif
#ifdef __NR_init_module
    case __NR_init_module:
        SM_ARG("image", 0);
        SM_ARG("length", 1);
        SM_ARG("params", 2);
        SM_DATA("image_preview", 0, 1);
        SM_STR("params_text", 2);
        break;
#endif
#ifdef __NR_delete_module
    case __NR_delete_module:
        SM_ARG("name", 0);
        SM_ARG("flags", 1);
        SM_STR("module", 0);
        break;
#endif
    default:
        return false;
    }

    syscall_monitor_emit(task, scno, text);
    return true;
}

#undef SM_DATA
#undef SM_STR
#undef SM_ARG
#undef SM_ADD

/*
do_el0_svc 入口 hook：hook_regs 是 do_el0_svc 函数入口的寄存器快照，
其中 x0 才是用户异常现场 struct pt_regs *。按文件、内存、进程、网络、模块
顺序交给独立大类函数处理；未识别项继续记录 syscall 号和 arm64 x0-x5 原始参数。
这里只记录 syscall 入口，尚不包含返回值、实际完成长度和 read/recv 输出数据。
*/
static int syscall_monitor_do_el0_svc_hook_work(struct pt_regs *hook_regs)
{
    struct pt_regs *sys_regs;
    struct task_struct *task = current;
    long scno;

    if (!syscall_monitor_should_trace()) return 0;

    if (!hook_regs) return 0;

    // do_el0_svc(struct pt_regs *regs) 的 x0 指向用户态异常现场。
    sys_regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];
    if (!sys_regs || !user_mode(sys_regs)) return 0;

    /*
    arm64 64 位 syscall ABI：
      x8      = syscall number
      x0-x5   = syscall arguments
    */
    scno = (long)sys_regs->regs[8];

    if (syscall_monitor_handle_file(task, sys_regs, scno) || syscall_monitor_handle_memory(task, sys_regs, scno) || syscall_monitor_handle_process(task, sys_regs, scno) || syscall_monitor_handle_network(task, sys_regs, scno) || syscall_monitor_handle_module(task, sys_regs, scno)) return 0;

    ls_log_always_tag("sysmon",
                      "tgid=%d pid=%d comm=%s syscall=%ld(%s) "
                      "x0=0x%llx x1=0x%llx x2=0x%llx x3=0x%llx x4=0x%llx x5=0x%llx\n",
                      task->tgid, task->pid, task->comm, scno, syscall_monitor_name(scno), (unsigned long long)sys_regs->regs[0], (unsigned long long)sys_regs->regs[1], (unsigned long long)sys_regs->regs[2], (unsigned long long)sys_regs->regs[3], (unsigned long long)sys_regs->regs[4], (unsigned long long)sys_regs->regs[5]);

    return 0;
}

// 当前只监控 64 位 arm64 syscall 入口；32 位 compat 入口后续可追加 do_el0_svc_compat。
static struct hook_entry g_syscall_monitor_hooks[] = {
    HOOK_ENTRY("do_el0_svc", syscall_monitor_do_el0_svc_hook_work),
};

// 安装 syscall 监控并添加目标进程 tgid；tgid 表示进程组，能覆盖该进程的所有线程。
static int syscall_monitor_install(pid_t target_tgid)
{
    int ret;
    int i, empty = -1;

    if (target_tgid <= 0) return -EINVAL;

    mutex_lock(&g_syscall_monitor_lock);
    ret = inline_hook_install(g_syscall_monitor_hooks);
    if (!ret)
    {
        for (i = 0; i < ARM_SYSCALL_MONITOR_MAX_PIDS; i++)
        {
            pid_t monitored_tgid = READ_ONCE(g_syscall_monitor_pids[i]);

            if (monitored_tgid == target_tgid) goto out_unlock;
            if (!monitored_tgid && empty < 0) empty = i;
        }

        if (empty < 0)
        {
            ret = -ENOSPC;
            goto out_unlock;
        }

        WRITE_ONCE(g_syscall_monitor_pids[empty], target_tgid);
    }

out_unlock:
    mutex_unlock(&g_syscall_monitor_lock);

    return ret;
}

// 删除指定目标进程 tgid；如果监控表空了，就卸载 do_el0_svc hook。
static void syscall_monitor_remove(pid_t target_tgid)
{
    int i;

    if (target_tgid <= 0) return;

    mutex_lock(&g_syscall_monitor_lock);
    for (i = 0; i < ARM_SYSCALL_MONITOR_MAX_PIDS; i++)
    {
        if (READ_ONCE(g_syscall_monitor_pids[i]) == target_tgid) WRITE_ONCE(g_syscall_monitor_pids[i], 0);
    }

    if (!syscall_monitor_has_pid()) inline_hook_remove(g_syscall_monitor_hooks);
    mutex_unlock(&g_syscall_monitor_lock);
}

// 控制端异常退出时清空全部目标，避免残留 PID 继续占用 hook。
static void syscall_monitor_remove_all(void)
{
    int i;

    mutex_lock(&g_syscall_monitor_lock);
    for (i = 0; i < ARM_SYSCALL_MONITOR_MAX_PIDS; i++) WRITE_ONCE(g_syscall_monitor_pids[i], 0);
    inline_hook_remove(g_syscall_monitor_hooks);
    mutex_unlock(&g_syscall_monitor_lock);
}

#endif /* ARM64_SYSCALLDBG_H */