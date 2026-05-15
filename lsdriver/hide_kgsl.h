#ifndef HIDE_KGSL_H
#define HIDE_KGSL_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/types.h>

#include "export_fun.h"
#include "inline_hook_frame.h"

// 隐藏状态：目标 pid，0 表示未激活
static pid_t g_hide_pid __read_mostly = 0;
static DEFINE_MUTEX(g_hide_kgsl_lock);

// 判断当前cpu运行的task是否为目标task
static bool should_hide(void)
{
    pid_t hide_pid = READ_ONCE(g_hide_pid);

    return hide_pid != 0 && task_pid_nr(current) == hide_pid;
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

// 安装
int hide_kgsl_install(pid_t pid)
{
    int ret = 0;

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

    if (g_kgsl_hooks[0].installed && g_kgsl_hooks[1].installed)
    {
        WRITE_ONCE(g_hide_pid, pid);
        pr_debug("kgsl_hide: update hidden PID %d\n", pid);
        goto out_unlock;
    }

    if (!g_kgsl_hooks[0].installed || !g_kgsl_hooks[1].installed)
    {
        ret = inline_hook_install(g_kgsl_hooks);
        if (ret)
        {
            pr_err("kgsl_hide: inline hook install failed: %d\n", ret);
            goto out_unlock;
        }
        pr_debug("kgsl_hide: inline hook installed\n");
    }

    WRITE_ONCE(g_hide_pid, pid);
    pr_debug("kgsl_hide: hidden PID %d\n", pid);

out_unlock:
    mutex_unlock(&g_hide_kgsl_lock);
    return ret;
}

// 清理
void hide_kgsl_remove(void)
{
    mutex_lock(&g_hide_kgsl_lock);
    inline_hook_remove(g_kgsl_hooks);
    WRITE_ONCE(g_hide_pid, 0);
    mutex_unlock(&g_hide_kgsl_lock);
}

#endif /* HIDE_KGSL_H */
