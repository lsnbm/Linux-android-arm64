#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/kallsyms.h>
#include <linux/errno.h>

// 隐藏状态：目标 pid，0 表示未激活
static pid_t g_hide_pid __read_mostly = 0;

// 判断当前cpu运行的task是否为目标task
static bool should_hide(void)
{
    return g_hide_pid != 0 && task_pid_nr(current) == g_hide_pid;
}

// ARM64：伪造 -ENOMEM 并跳过函数体
static void fake_enomem_and_return(struct pt_regs *regs)
{
    regs->regs[0] = (u64)(long)(-ENOMEM);
    regs->pc = regs->regs[30]; /* LR → 返回调用者 */
}

// kgsl_process_init_sysfs
static int pre_handler_kgsl_sysfs(struct kprobe *kp, struct pt_regs *regs)
{
    if (should_hide())
    {
        fake_enomem_and_return(regs);
        return 1; /* 非零：跳过原函数 */
    }
    return 0;
}

// kgsl_process_init_debugfs
static int pre_handler_kgsl_debugfs(struct kprobe *kp, struct pt_regs *regs)
{
    if (should_hide())
    {
        fake_enomem_and_return(regs);
        return 1;
    }
    return 0;
}

static struct kprobe kp_kgsl_sysfs = {
    .symbol_name = "kgsl_process_init_sysfs",
    .pre_handler = pre_handler_kgsl_sysfs,
};

static struct kprobe kp_kgsl_debugfs = {
    .symbol_name = "kgsl_process_init_debugfs",
    .pre_handler = pre_handler_kgsl_debugfs,
};

static struct kprobe *g_kprobes[] = {
    &kp_kgsl_sysfs,
    &kp_kgsl_debugfs,
};

static int kprobes_install(void)
{
    int i, ret;

    for (i = 0; i < ARRAY_SIZE(g_kprobes); i++)
    {
        ret = register_kprobe(g_kprobes[i]);
        if (ret < 0)
        {
            pr_err("kgsl_hide: register_kprobe[%d] (%s) failed: %d\n",
                   i, g_kprobes[i]->symbol_name, ret);
            /* 回滚：注销已成功注册的探针 */
            while (--i >= 0)
                unregister_kprobe(g_kprobes[i]);
            return ret;
        }
        pr_debug("kgsl_hide: kprobe installed on %s @ %pf\n",
                 g_kprobes[i]->symbol_name, g_kprobes[i]->addr);
    }
    return 0;
}

static void kprobes_remove(void)
{
    int i;

    for (i = 0; i < ARRAY_SIZE(g_kprobes); i++)
        unregister_kprobe(g_kprobes[i]);
}

// 安装
int hide_kgsl_install(pid_t pid)
{
    if (pid <= 0)
        return -EINVAL;

    // 检查高通平台符号是否存在。MTK跳过不需要隐藏
    if (!generic_kallsyms_lookup_name("kgsl_process_init_sysfs") ||
        !generic_kallsyms_lookup_name("kgsl_process_init_debugfs"))
    {
        pr_debug("kgsl_hide: KGSL symbols not found, skip (non-Qualcomm?)\n");
        return 0;
    }

    g_hide_pid = pid;

    return kprobes_install();
}

// 清理
void hide_kgsl_remove(void)
{
    kprobes_remove();
    g_hide_pid = 0;
}