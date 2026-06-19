// 感谢https://github.com/LinYuFlower(林雨)

#ifndef HIDE_KGSL_H
#define HIDE_KGSL_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/types.h>

#include "export_fun.h"
#include "inline_hook_frame.h"

#define HIDE_KGSL_MAX_PIDS 8

// KGSL 隐藏表保存目标进程 tgid；0 表示空槽。
static pid_t g_hide_kgsl_pids[HIDE_KGSL_MAX_PIDS];
static DEFINE_MUTEX(g_hide_kgsl_lock);

// 快速判断 KGSL 隐藏表是否还有目标，空表时可卸载 hook。
static bool hide_kgsl_has_pid(void)
{
    int i;

    for (i = 0; i < HIDE_KGSL_MAX_PIDS; i++)
        if (READ_ONCE(g_hide_kgsl_pids[i]))
            return true;
    return false;
}

// 判断当前 task 是否属于需要隐藏 KGSL 节点的目标进程。
static bool should_hide(void)
{
    int i;

    for (i = 0; i < HIDE_KGSL_MAX_PIDS; i++)
    {
        pid_t hide_pid = READ_ONCE(g_hide_kgsl_pids[i]);

        if (hide_pid && current->tgid == hide_pid)
            return true;
    }
    return false;
}
// 判断指定对象是否为kgsl下
static bool kobj_under_kgsl(struct kobject *kobj)
{
    struct kobject *p;
    int depth = 0;

    for (p = kobj->parent; p && depth < 8; p = p->parent, depth++)
    {
        if (p->name && strstr(p->name, "kgsl"))
            return true;
    }
    return false;
}

// ARM64：伪造 -ENOMEM 并跳过函数体
static void fake_enomem_and_return(struct pt_regs *regs)
{
    regs->regs[0] = (u64)(long)(-ENOMEM);
    regs->pc = regs->regs[30]; /* LR -> 返回调用者 */
}

// kgsl_process_init_sysfs / kgsl_process_init_debugfs inline hook 工作函数
static int kgsl_process_init_hook_work(struct pt_regs *regs)
{
    if (should_hide())
    {
        fake_enomem_and_return(regs);
        return 1; /* 非零：恢复现场后不执行原函数 */
    }
    return 0;
}

static int sysfs_create_group_hook_work(struct pt_regs *regs)
{
    struct kobject *kobj = (struct kobject *)regs->regs[0];

    if (!kobj || !kobj->name)
        return 0;

    if (!kobj_under_kgsl(kobj))
        return 0;

    if (should_hide())
    {
        regs->regs[0] = -ENOMEM;
        regs->pc = regs->regs[30];
        return 1;
    }
    return 0;
}

static struct hook_entry g_kgsl_hooks[] = {
    HOOK_ENTRY("kgsl_process_init_sysfs", kgsl_process_init_hook_work),
    HOOK_ENTRY("kgsl_process_init_debugfs", kgsl_process_init_hook_work),
    HOOK_ENTRY("sysfs_create_group", sysfs_create_group_hook_work),

};

// 安装隐藏支持多个 PID 同时隐藏 KGSL/GPU 节点初始化信息
int hide_kgsl_install(pid_t pid)
{
    int ret = 0;
    int i, empty = -1;

    if (pid <= 0)
        return -EINVAL;

    mutex_lock(&g_hide_kgsl_lock);

    // 检查高通平台符号是否存在。MTK跳过不需要隐藏
    if (!generic_kallsyms_lookup_name("kgsl_process_init_sysfs") ||
        !generic_kallsyms_lookup_name("kgsl_process_init_debugfs"))
    {
        pr_debug("kgsl_hide: KGSL symbols not found, skip (non-Qualcomm?)\n");
        goto out_unlock;
    }

    ret = inline_hook_install(g_kgsl_hooks);
    if (ret)
    {
        pr_err("kgsl_hide: inline hook install failed: %d\n", ret);
        goto out_unlock;
    }
    pr_debug("kgsl_hide: inline hook installed\n");

    // hook 安装成功后再写隐藏表，避免表里有 PID 但拦截点没生效。
    for (i = 0; i < HIDE_KGSL_MAX_PIDS; i++)
    {
        pid_t hidden_pid = READ_ONCE(g_hide_kgsl_pids[i]);

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

    WRITE_ONCE(g_hide_kgsl_pids[empty], pid);
    pr_debug("kgsl_hide: hidden PID %d\n", pid);

out_unlock:
    mutex_unlock(&g_hide_kgsl_lock);
    return ret;
}

// 删除指定目标 PID；如果隐藏表空了，就卸载 KGSL hooks。
void hide_kgsl_remove(pid_t pid)
{
    int i;

    if (pid <= 0)
        return;

    mutex_lock(&g_hide_kgsl_lock);
    for (i = 0; i < HIDE_KGSL_MAX_PIDS; i++)
    {
        if (READ_ONCE(g_hide_kgsl_pids[i]) == pid)
            WRITE_ONCE(g_hide_kgsl_pids[i], 0);
    }

    if (!hide_kgsl_has_pid())
        inline_hook_remove(g_kgsl_hooks);
    mutex_unlock(&g_hide_kgsl_lock);
}

#endif /* HIDE_KGSL_H */
