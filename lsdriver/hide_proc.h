#ifndef HIDE_PROC_H
#define HIDE_PROC_H

#include <linux/proc_fs.h>
/*
参考https://github.com/OnePlusOSS/android_kernel_modules_and_devicetree_oneplus_mt6993/blob/oneplus/mt6993_b_16.0_ace_6_ultra/vendor/oplus/kernel/secureguard/gki2.0/rootguard_new/oplus_kernel_security_check.c
*/
#include "inline_hook_frame.h"

static const char *const proc_create_block_names[] = {
    "inte_ko",
    "inte_systbl",
    "inte_status",
};

static int proc_create_block_hook_work(struct pt_regs *regs)
{
    const char *name = (const char *)(uintptr_t)regs->regs[0];

    if (!name || regs->regs[2]) return 0;

    for (size_t index = 0; index < ARRAY_SIZE(proc_create_block_names); index++)
    {
        if (__builtin_strcmp(name, proc_create_block_names[index]) != 0) continue;

        regs->regs[0] = 0;
        regs->pc = regs->regs[30];
        return 1;
    }

    return 0;
}

static struct hook_entry proc_create_block_hooks[] = {
    HOOK_ENTRY("proc_create_data", proc_create_block_hook_work),
};

static int proc_create_block_init(void)
{
    int ret = inline_hook_install(proc_create_block_hooks);

    if (ret < 0)
    {
        ls_log_always_tag("proc_block", "inline hook install failed: %d\n", ret);
        return ret;
    }

    for (size_t index = 0; index < ARRAY_SIZE(proc_create_block_names); index++)
    {
        ret = remove_proc_subtree(proc_create_block_names[index], NULL);
        if (ret < 0 && ret != -ENOENT) ls_log_always_tag("proc_block", "remove /proc/%s failed: %d\n", proc_create_block_names[index], ret);
    }

    return 0;
}

#endif