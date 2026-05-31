// 感谢qq:1277981260(花开富贵)

/*
这个隐藏还是有问题，不知道为何只有部分小米设备死机，其他设备均无问题

为什么这里必须过滤 ctx->actor，而不是只 hook proc_fill_cache：

1. Google common kernel 源码里，/proc 根目录数字 PID 枚举通常是：
   proc_root_readdir(file, ctx)
     -> proc_pid_readdir(file, ctx)
       -> next_tgid()
       -> name = snprintf("%u", iter.tgid)
       -> proc_fill_cache(file, ctx, name, len, proc_pid_instantiate, iter.task, NULL)
       -> dir_emit(ctx, name, len, ino, type)
       -> ctx->actor(ctx, name, len, ctx->pos, ino, type)

2. 但实机调试过 OPlus Android 14 / 6.1.25 内核后发现：
   - proc_root_readdir 会进入；
   - proc_pid_readdir 会进入；
   - /proc 数字 PID 目录仍可见；
   - hook proc_fill_cache 虽然能安装，也会被其他 proc 子目录调用，
     但 /proc 根目录数字 PID 枚举时没有经过该符号入口。
   也就是说厂商内核可能把 proc_fill_cache 内联、改写，或在 proc_pid_readdir 中绕开了该符号。

3. 不管 proc_pid_readdir 内部最终是调用 proc_fill_cache、dir_emit，还是厂商自定义 emit 路径，
   要把目录项返回给 getdents64，最后都必须调用当前 struct dir_context 的 ctx->actor。
   因此在 proc 枚举入口临时替换 ctx->actor，是比 hook proc_fill_cache 更靠近用户层返回结果的拦截点。

4. 命中隐藏 PID 时不能返回“失败/停止枚举”：
   - 6.1+ 的 filldir_t 返回 bool，true 表示当前项处理成功并继续枚举；
   - 5.10/5.15 的 filldir_t 返回 int，0 表示当前项处理成功并继续枚举。
   所以隐藏时要假装当前目录项已经成功处理，但不调用原 actor，避免截断后续 /proc 枚举。

5. 不能用单个全局 orig_actor：多线程同时 ls /proc 时会互相覆盖。
   这里使用多个 actor slot，把不同的原 actor 和固定 wrapper 绑定，避免并发错调。
*/
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
#include <linux/spinlock.h>

#include "export_fun.h"
#include "inline_hook_frame.h"

// 这里保存几种不同的 filldir 回调类型。
// 正常只会用到 filldir / filldir64 / compat_filldir，8 个槽够留余量。
#define HIDE_PROCESS_ACTOR_SLOTS 8
#define HIDE_PROCESS_MAX_PIDS 8

static pid_t g_hidden_pids[HIDE_PROCESS_MAX_PIDS];
static struct hook_entry g_proc_iterate_hook[1];
static DEFINE_MUTEX(g_hide_process_lock);

// 只保护 g_hide_actor_slots 的注册/清空，不在锁里调用原 actor。
static DEFINE_SPINLOCK(g_hide_actor_lock);

struct hide_actor_slot
{
    // getdents 原本要调用的 actor，比如 filldir、filldir64、compat_filldir。
    filldir_t orig_actor;

    // 我们替换进 ctx->actor 的包装函数，先过滤 PID，再调用 orig_actor。
    filldir_t hide_actor;
};

// 不能用单个全局 g_orig_actor，否则多线程 ls /proc 时会互相覆盖。
// 每种原 actor 都绑定一个固定 wrapper，避免 filldir64 被当成 filldir 调。
static struct hide_actor_slot g_hide_actor_slots[HIDE_PROCESS_ACTOR_SLOTS];

static bool hide_process_has_pid(void)
{
    int i;

    for (i = 0; i < HIDE_PROCESS_MAX_PIDS; i++)
        if (READ_ONCE(g_hidden_pids[i]))
            return true;
    return false;
}

static int hide_process_add_pid(pid_t pid)
{
    int i, empty = -1;

    if (pid <= 0)
        return -EINVAL;

    for (i = 0; i < HIDE_PROCESS_MAX_PIDS; i++)
    {
        pid_t hidden_pid = READ_ONCE(g_hidden_pids[i]);

        if (hidden_pid == pid)
            return 0;
        if (!hidden_pid && empty < 0)
            empty = i;
    }

    if (empty < 0)
        return -ENOSPC;

    WRITE_ONCE(g_hidden_pids[empty], pid);
    return 0;
}

// 判断当前 /proc 目录项是不是要隐藏的 PID。
static bool hide_process_match_pid(const char *name, int namlen, unsigned int d_type)
{
    char pid_str[16];
    int i, pid_len;

    if (d_type != DT_DIR || namlen <= 0 || namlen >= sizeof(pid_str))
        return false;

    for (i = 0; i < HIDE_PROCESS_MAX_PIDS; i++)
    {
        pid_t hidden_pid = READ_ONCE(g_hidden_pids[i]);

        if (!hidden_pid)
            continue;

        pid_len = snprintf(pid_str, sizeof(pid_str), "%d", hidden_pid);
        if (pid_len == namlen && __builtin_memcmp(name, pid_str, namlen) == 0)
            return true;
    }
    return false;
}

// 内核 6.1 起 filldir_t 返回 bool；老版本返回 int。
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
#define HIDE_FILLDIR_RET bool

static bool hide_filldir_dispatch(int slot, struct dir_context *ctx, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type)
{
    filldir_t orig_actor;

    // 6.1+ 返回 true 表示当前项处理成功并继续枚举。
    // 这里不调用 orig_actor，相当于跳过当前 PID 目录项。
    if (hide_process_match_pid(name, namlen, d_type))
        return true;

    // 按 wrapper 槽位找到对应的原 actor，保证类型不会串。
    orig_actor = READ_ONCE(g_hide_actor_slots[slot].orig_actor);
    return orig_actor ? orig_actor(ctx, name, namlen, offset, ino, d_type) : false;
}
#else
#define HIDE_FILLDIR_RET int

static int hide_filldir_dispatch(int slot, struct dir_context *ctx, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type)
{
    filldir_t orig_actor;

    // 5.10/5.15 返回 0 表示当前项处理成功并继续枚举。
    // 这里不调用 orig_actor，相当于跳过当前 PID 目录项。
    if (hide_process_match_pid(name, namlen, d_type))
        return 0;

    // 按 wrapper 槽位找到对应的原 actor，保证类型不会串。
    orig_actor = READ_ONCE(g_hide_actor_slots[slot].orig_actor);
    return orig_actor ? orig_actor(ctx, name, namlen, offset, ino, d_type) : 0;
}
#endif

// 生成 8 个独立 wrapper。
// 关键点：每个 wrapper 自带固定 index，所以能反查自己的 orig_actor。
#define DEFINE_HIDE_FILLDIR_WRAPPER(index)                                                                                                           \
    static HIDE_FILLDIR_RET hide_filldir_##index(struct dir_context *ctx, const char *name, int namlen, loff_t offset, u64 ino, unsigned int d_type) \
    {                                                                                                                                                \
        return hide_filldir_dispatch(index, ctx, name, namlen, offset, ino, d_type);                                                                 \
    }

DEFINE_HIDE_FILLDIR_WRAPPER(0)
DEFINE_HIDE_FILLDIR_WRAPPER(1)
DEFINE_HIDE_FILLDIR_WRAPPER(2)
DEFINE_HIDE_FILLDIR_WRAPPER(3)
DEFINE_HIDE_FILLDIR_WRAPPER(4)
DEFINE_HIDE_FILLDIR_WRAPPER(5)
DEFINE_HIDE_FILLDIR_WRAPPER(6)
DEFINE_HIDE_FILLDIR_WRAPPER(7)

static void hide_actor_slots_init(void)
{
    // hide_actor 固定不变；orig_actor 后面遇到真实 getdents 调用时再填。
    g_hide_actor_slots[0].hide_actor = (filldir_t)hide_filldir_0;
    g_hide_actor_slots[1].hide_actor = (filldir_t)hide_filldir_1;
    g_hide_actor_slots[2].hide_actor = (filldir_t)hide_filldir_2;
    g_hide_actor_slots[3].hide_actor = (filldir_t)hide_filldir_3;
    g_hide_actor_slots[4].hide_actor = (filldir_t)hide_filldir_4;
    g_hide_actor_slots[5].hide_actor = (filldir_t)hide_filldir_5;
    g_hide_actor_slots[6].hide_actor = (filldir_t)hide_filldir_6;
    g_hide_actor_slots[7].hide_actor = (filldir_t)hide_filldir_7;
}

static bool hide_actor_is_wrapper(filldir_t actor)
{
    int i;

    // 防止已经替换过的 ctx->actor 被重复包装。
    for (i = 0; i < HIDE_PROCESS_ACTOR_SLOTS; i++)
    {
        if (actor == READ_ONCE(g_hide_actor_slots[i].hide_actor))
            return true;
    }
    return false;
}

static filldir_t hide_actor_get_wrapper(filldir_t orig_actor)
{
    unsigned long flags;
    filldir_t wrapper = NULL;
    int i;

    if (!orig_actor || hide_actor_is_wrapper(orig_actor))
        return NULL;

    spin_lock_irqsave(&g_hide_actor_lock, flags);

    // 这个原 actor 之前见过，直接复用同一个 wrapper。
    for (i = 0; i < HIDE_PROCESS_ACTOR_SLOTS; i++)
    {
        if (g_hide_actor_slots[i].orig_actor == orig_actor)
        {
            wrapper = g_hide_actor_slots[i].hide_actor;
            goto out_unlock;
        }
    }

    // 第一次见到这个原 actor，找一个空槽绑定。
    for (i = 0; i < HIDE_PROCESS_ACTOR_SLOTS; i++)
    {
        if (!g_hide_actor_slots[i].orig_actor)
        {
            WRITE_ONCE(g_hide_actor_slots[i].orig_actor, orig_actor);
            wrapper = g_hide_actor_slots[i].hide_actor;
            goto out_unlock;
        }
    }

    // 槽满时返回 NULL，保持原 actor 不动，宁愿不隐藏也不冒险错调。
out_unlock:
    spin_unlock_irqrestore(&g_hide_actor_lock, flags);
    return wrapper;
}

static void hide_actor_slots_reset(void)
{
    unsigned long flags;
    int i;

    spin_lock_irqsave(&g_hide_actor_lock, flags);

    // 卸载时只清 orig_actor，hide_actor wrapper 函数地址不用变。
    for (i = 0; i < HIDE_PROCESS_ACTOR_SLOTS; i++)
        WRITE_ONCE(g_hide_actor_slots[i].orig_actor, NULL);
    spin_unlock_irqrestore(&g_hide_actor_lock, flags);
}

// inline hook 工作函数，返回 0 表示继续执行原函数
static int proc_iterate_hook_work(struct pt_regs *regs)
{
    // arm64 调用约定：iterate_shared(file, ctx)，所以 x1 是 struct dir_context *。
    struct dir_context *ctx = (struct dir_context *)regs->regs[1];
    filldir_t actor;
    filldir_t wrapper;

    if (!hide_process_has_pid() || !ctx)
        return 0;

    // 保存当前 getdents 使用的真实 actor，并替换成对应 wrapper。
    actor = READ_ONCE(ctx->actor);
    if (!actor || hide_actor_is_wrapper(actor))
        return 0;

    wrapper = hide_actor_get_wrapper(actor);
    if (!wrapper)
        return 0;

    // 原函数继续执行时，就会调用我们的 wrapper 进行 PID 过滤。
    WRITE_ONCE(ctx->actor, wrapper);
    return 0;
}

// 安装hook,隐藏目标pid
static int hide_process_install(pid_t pid)
{
    struct file_operations *fops;
    unsigned long iterate_addr;
    int ret = 0;

    mutex_lock(&g_hide_process_lock);

    if (!g_proc_iterate_hook[0].target_addr)
    {
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
    }

    hide_actor_slots_init();

    ret = inline_hook_install(g_proc_iterate_hook);
    if (ret)
    {
        pr_debug("hide_process: inline hook 安装失败 %d\n", ret);
        goto out_unlock;
    }

    ret = hide_process_add_pid(pid);
    if (ret)
    {
        goto out_unlock;
    }
    pr_debug("hide_process: 隐藏 PID %d (iterate_shared=%px)\n",
             pid, (void *)g_proc_iterate_hook[0].target_addr);

out_unlock:
    mutex_unlock(&g_hide_process_lock);
    return ret;
}

// 卸载hook
static void hide_process_remove(void)
{
    int i;

    mutex_lock(&g_hide_process_lock);
    inline_hook_remove(g_proc_iterate_hook);
    for (i = 0; i < HIDE_PROCESS_MAX_PIDS; i++)
        WRITE_ONCE(g_hidden_pids[i], 0);
    hide_actor_slots_reset();
    mutex_unlock(&g_hide_process_lock);
    pr_debug("hide_process: hook 已卸载\n");
}

#endif /* HIDE_PROCESS_H */
