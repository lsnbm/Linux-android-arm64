#ifndef ARM64_DPTDBG_H
#define ARM64_DPTDBG_H

/*
DPTDBG 使用“普通页表 + 私有 shadow PGD + trap page”实现主线程执行断点：

1. 启动时通过目标 tgid 获取 mm，遍历执行断点，读取用户 PTE，并把每个
    受管代码页复制到独立的 trap page；每个受管页按断点槽位保存原始指令，
    trap page 对应位置改写为 0（AArch64 UDF 指令）。
2. 为目标 mm 分配独立 PGD，只复制受管代码页经过的页表路径。shadow 路径
    最终把这些虚拟页映射到 trap page，普通页仍沿用目标 mm 的原始映射。
3. 同步 hook 进入 EL0 异常时，先确认异常来自受管页和 UDF trap；主线程
    执行命中回调并模拟原始指令，其他线程切换到 shadow view 后直接执行
    原始映射，从而只拦截主线程。
4. __switch_to hook 在任务切换时选择 normal/shadow TTBR0；不支持 TTBR0
    PAN 的内核直接写当前 CPU 的 TTBR0，支持 TTBR0 PAN 的内核更新目标
    task 保存的 ttbr0。修改页表或 TTBR0 后刷新目标 mm 的 TLB。
5. MMU notifier 发现目标 mm 失效时先停止监控、恢复线程的 normal view，
    再异步回收 trap page、shadow 页表和 mm 引用，避免继续使用陈旧映射。
6. start、stop、异常回滚和 notifier 清理共用生命周期锁；异常 handler
    只使用 RCU 和自旋锁，完成现场恢复后再由 workqueue 执行可睡眠清理。

该文件只保存 DPTDBG 的运行状态和实现，不修改目标进程的普通页表。
*/

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mmu_notifier.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/smp.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>
#include <asm/esr.h>
#include <asm/memory.h>
#include <asm/mmu_context.h>
#include <asm/pgtable-hwdef.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>
#include <asm/tlbflush.h>

#include "arm64_emulate/emulate_inst.h"
#include "arm64_reg.h"
#include "export_fun.h"
#include "inline_hook_frame.h"
#include "io_version/io_struct.h"

struct dptdbg_page
{
    pte_t original_mapping;                 // 启动时的原始 PTE，用于校验源物理页映射
    struct page *trap_page;                 // 复制代码并在断点处写入 UDF 的替代执行页
    uint64_t page_vaddr;                    // 受管虚拟页基址，用于按 PC 查页和计算页内偏移
    uint32_t original_insts[BP_CONFIG_MAX]; // 该页断点的原始指令，索引对应 info->points 的全局槽位
};

struct dptdbg_context
{
    struct break_point *info; // 已发布的断点配置和回调；为 NULL 时异常与切换 hook 不处理监控
    struct mm_struct *mm;     // 持有引用的目标地址空间，用于线程匹配、读取原页表和刷新 TLB

    struct dptdbg_page pages[BP_CONFIG_MAX]; // 按虚拟页去重的受管页列表，page_vaddr 为 0 表示空槽

    struct page *shadow_pgd_page;                                    // 私有影子页表根页，切入断点视图时作为 TTBR0 的页表基址
    struct page *table_pages[BP_CONFIG_MAX * CONFIG_PGTABLE_LEVELS]; // 自行分配的中间页表页，供回收使用；NULL 表示空槽

    struct mmu_notifier mmu_notifier; // 目标映射失效或地址空间释放时通知停止监控

    spinlock_t lock;               // 保护异常、线程切换和停止路径共享的运行状态，不允许持锁睡眠
    struct mutex lifecycle_lock;   // 串行化启动、停止和异步资源回收，允许持锁睡眠
    struct work_struct abort_work; // 将异常或映射失效触发的可睡眠清理交给工作队列

    bool stopping; // 停止切入影子页表，但保留配置供尚未退出的 UDF 异常恢复原指令
};

static struct dptdbg_context g_dptdbg = {
    .lock = __SPIN_LOCK_UNLOCKED(g_dptdbg.lock),
    .lifecycle_lock = __MUTEX_INITIALIZER(g_dptdbg.lifecycle_lock),
};

static struct dptdbg_page *dptdbg_find_page(uint64_t page_vaddr)
{
    for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg.pages); index++)
    {
        if (!g_dptdbg.pages[index].page_vaddr) break;
        if (g_dptdbg.pages[index].page_vaddr == page_vaddr) return &g_dptdbg.pages[index];
    }

    return NULL;
}

static bool dptdbg_get_inst_index(uint64_t page_vaddr, uint64_t hook_addr, size_t *inst_index)
{
    if (!page_vaddr || !inst_index || (hook_addr & (sizeof(uint32_t) - 1U)) || hook_addr < page_vaddr || hook_addr - page_vaddr > (uint64_t)PAGE_SIZE - sizeof(uint32_t)) return false;

    *inst_index = (hook_addr - page_vaddr) / sizeof(uint32_t);
    return true;
}

static bool dptdbg_handler_enter(void)
{
    rcu_read_lock();
    unsigned long flags;
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    bool admitted = g_dptdbg.info != NULL;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);
    if (!admitted) rcu_read_unlock();
    return admitted;
}

static bool dptdbg_task_is_target_locked(const struct task_struct *task)
{
    return task && g_dptdbg.mm && g_dptdbg.info && task->mm == g_dptdbg.mm && READ_ONCE(task->tgid) == READ_ONCE(g_dptdbg.info->tgid);
}

static bool dptdbg_task_needs_clean_view_locked(struct task_struct *task)
{
    return !g_dptdbg.stopping && dptdbg_task_is_target_locked(task) && thread_group_leader(task);
}

static void dptdbg_flush_view_translations(void)
{
    struct mm_struct *mm = READ_ONCE(g_dptdbg.mm);

    if (!mm) return;
    flush_tlb_all();
}

static uint64_t dptdbg_compose_ttbr0(uint64_t template, phys_addr_t pgd_pa, bool clean_view)
{
    const uint64_t ttbr_baddr_mask = GENMASK_ULL(47, 2);
    uint64_t ttbr = (template & ~ttbr_baddr_mask) | ((uint64_t)phys_to_ttbr(pgd_pa) & ttbr_baddr_mask);

    if (clean_view) ttbr &= ~TTBR_CNP_BIT;
    return ttbr;
}

static bool dptdbg_write_ttbr0_locked(phys_addr_t pgd_pa, bool clean_view)
{
    if (!pgd_pa) return false;
    uint64_t current_ttbr = read_sysreg(ttbr0_el1);
    uint64_t next_ttbr = dptdbg_compose_ttbr0(current_ttbr, pgd_pa, clean_view);
    if (current_ttbr == next_ttbr) return false;

    write_sysreg(next_ttbr, ttbr0_el1);
    isb();
    return true;
}

static bool dptdbg_write_task_view_locked(struct task_struct *task, bool clean_view)
{
    if (!task || !g_dptdbg.mm || !g_dptdbg.shadow_pgd_page) return false;
    phys_addr_t pgd_pa = clean_view ? page_to_phys(g_dptdbg.shadow_pgd_page) : virt_to_phys(g_dptdbg.mm->pgd);
    if (system_uses_ttbr0_pan())
    {
        uint64_t current_ttbr = READ_ONCE(task_thread_info(task)->ttbr0);
        uint64_t next_ttbr = dptdbg_compose_ttbr0(current_ttbr, pgd_pa, clean_view);

        if (current_ttbr == next_ttbr) return false;
        WRITE_ONCE(task_thread_info(task)->ttbr0, next_ttbr);
        return true;
    }
    return dptdbg_write_ttbr0_locked(pgd_pa, clean_view);
}

static int dptdbg_handle_switch(struct pt_regs *hook_regs)
{
    if (!hook_regs || !dptdbg_handler_enter()) return 0;
    struct task_struct *next = (struct task_struct *)hook_regs->regs[1];
    if (!next)
    {
        rcu_read_unlock();
        return 0;
    }

    unsigned long flags;
    bool flush_needed = false;
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    if (next->mm == g_dptdbg.mm) flush_needed = dptdbg_write_task_view_locked(next, dptdbg_task_needs_clean_view_locked(next));
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);
    if (flush_needed) dptdbg_flush_view_translations();
    rcu_read_unlock();
    return 0;
}

static void dptdbg_apply_current_thread_view(void *unused)
{
    (void)unused;
    unsigned long flags;
    bool flush_needed = false;
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    if (current->mm == g_dptdbg.mm) flush_needed = dptdbg_write_task_view_locked(current, dptdbg_task_needs_clean_view_locked(current));
    else if (!system_uses_ttbr0_pan())
    {
        struct mm_struct *mm = current->mm ? current->mm : current->active_mm;
        if (mm) flush_needed = dptdbg_write_ttbr0_locked(virt_to_phys(mm->pgd), false);
    }
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);
    if (flush_needed) dptdbg_flush_view_translations();
}

static void dptdbg_restore_saved_thread_views(void)
{
    if (!system_uses_ttbr0_pan()) return;

    unsigned long flags;
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    struct mm_struct *target_mm = g_dptdbg.mm;
    struct break_point *info = g_dptdbg.info;
    pid_t target_tgid = info ? READ_ONCE(info->tgid) : 0;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);

    if (!target_mm || target_tgid <= 0) return;

    struct task_struct *process;
    struct task_struct *task;
    rcu_read_lock();
    for_each_process_thread(process, task)
    {
        if (READ_ONCE(task->tgid) != target_tgid || task->mm != target_mm) continue;
        spin_lock_irqsave(&g_dptdbg.lock, flags);
        if (g_dptdbg.mm == target_mm && g_dptdbg.info == info && READ_ONCE(info->tgid) == target_tgid) (void)dptdbg_write_task_view_locked(task, false);
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
    }
    rcu_read_unlock();
}

static void dptdbg_abort_monitor(void)
{
    unsigned long flags;
    bool flush_needed = false;
    bool schedule_cleanup = false;
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    if (!g_dptdbg.mm)
    {
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        return;
    }

    if (!g_dptdbg.stopping)
    {
        g_dptdbg.stopping = true;
        schedule_cleanup = true;
    }
    if (current->mm == g_dptdbg.mm) flush_needed = dptdbg_write_task_view_locked(current, false);
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);

    if (flush_needed || schedule_cleanup) dptdbg_flush_view_translations();
    if (schedule_cleanup) schedule_work(&g_dptdbg.abort_work);
}

static void dptdbg_mmu_notifier_release(struct mmu_notifier *notifier, struct mm_struct *mm)
{
    (void)notifier;
    unsigned long flags;
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    if (g_dptdbg.mm != mm)
    {
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        return;
    }
    bool schedule_cleanup = !g_dptdbg.stopping;
    g_dptdbg.stopping = true;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);

    dptdbg_restore_saved_thread_views();
    on_each_cpu(dptdbg_apply_current_thread_view, NULL, 1);
    dptdbg_flush_view_translations();
    if (schedule_cleanup) schedule_work(&g_dptdbg.abort_work);
}

static int dptdbg_mmu_notifier_invalidate_range_start(struct mmu_notifier *notifier, const struct mmu_notifier_range *range)
{
    (void)notifier;
    if (!mmu_notifier_range_blockable(range)) return -EAGAIN;

    unsigned long flags;
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    if (g_dptdbg.mm != range->mm || g_dptdbg.stopping)
    {
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        return 0;
    }
    g_dptdbg.stopping = true;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);

    dptdbg_restore_saved_thread_views();
    on_each_cpu(dptdbg_apply_current_thread_view, NULL, 1);
    dptdbg_flush_view_translations();
    schedule_work(&g_dptdbg.abort_work);
    return 0;
}

static const struct mmu_notifier_ops dptdbg_mmu_notifier_ops = {
    .release = dptdbg_mmu_notifier_release,
    .invalidate_range_start = dptdbg_mmu_notifier_invalidate_range_start,
};

static int dptdbg_handle_undef_sync(struct pt_regs *hook_regs)
{
    struct fp_regs fp_regs __attribute__((__uninitialized__));
    unsigned long flags;
    int result = 0;

    if (!hook_regs || !dptdbg_handler_enter()) return 0;
    unsigned int exception_class = ESR_ELx_EC(read_sysreg(esr_el1));
    if (exception_class != ESR_ELx_EC_UNKNOWN)
    {
        if ((exception_class == ESR_ELx_EC_IABT_LOW || exception_class == ESR_ELx_EC_DABT_LOW) && current->mm)
        {
            spin_lock_irqsave(&g_dptdbg.lock, flags);
            bool target_mm = g_dptdbg.mm == current->mm;
            spin_unlock_irqrestore(&g_dptdbg.lock, flags);
            if (target_mm) dptdbg_abort_monitor();
        }
        goto out;
    }
    struct pt_regs *regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];
    if (!regs || !current->mm || !user_mode(regs)) goto out;

    uint64_t pc = untagged_addr(regs->pc);
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    struct break_point *info = g_dptdbg.info;
    if (g_dptdbg.mm != current->mm || !info)
    {
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        goto out;
    }
    struct bp_point *hit_point = bp_info_find_point_by_pc(info, pc);
    if (!hit_point)
    {
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        goto out;
    }
    struct dptdbg_page *managed_page = dptdbg_find_page(pc & PAGE_MASK);
    if (!managed_page || !managed_page->trap_page)
    {
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        goto out;
    }
    uint32_t *trap_code = page_address(managed_page->trap_page);
    size_t inst_index;
    if (!trap_code || !dptdbg_get_inst_index(managed_page->page_vaddr, pc, &inst_index) || READ_ONCE(trap_code[inst_index]) != 0U)
    {
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        goto out;
    }
    uint32_t original_inst = managed_page->original_insts[hit_point - info->points];

    bool stopping = g_dptdbg.stopping;
    bool exiting = !!(READ_ONCE(current->flags) & PF_EXITING);
    bool target_main = dptdbg_task_is_target_locked(current) && thread_group_leader(current);
    if (stopping || exiting)
    {
        bool emulated;

        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        read_all_q_regs(&fp_regs);
        emulated = regs->pc != pc || emulate_inst(regs, &fp_regs, original_inst);
        write_all_q_regs(&fp_regs);
        result = emulated ? 1 : 0;
        goto out;
    }
    if (!target_main)
    {
        bool flush_needed = dptdbg_write_task_view_locked(current, true);

        result = 1;
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        if (flush_needed) dptdbg_flush_view_translations();
        goto out;
    }
    void (*on_hit)(void *regs, void *fp_regs, void *hit_point) = READ_ONCE(hit_point->on_hit);
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);

    result = 1;
    read_all_q_regs(&fp_regs);
    if (on_hit) on_hit(regs, &fp_regs, hit_point);
    if (regs->pc == pc && !emulate_inst(regs, &fp_regs, original_inst))
    {
        dptdbg_abort_monitor();
    }
    write_all_q_regs(&fp_regs);
out:
    rcu_read_unlock();
    return result;
}

static struct hook_entry g_dptdbg_sync_hooks[][1] = {
    {HOOK_ENTRY("el0t_64_sync_handler", dptdbg_handle_undef_sync)},
    {HOOK_ENTRY("el0_sync_handler", dptdbg_handle_undef_sync)},
};

static struct hook_entry g_dptdbg_switch_hook[] = {
    HOOK_ENTRY("__switch_to", dptdbg_handle_switch),
};

__attribute__((no_sanitize("cfi"))) static void dptdbg_release_state(void)
{
    unsigned long flags;

    spin_lock_irqsave(&g_dptdbg.lock, flags);
    if (!g_dptdbg.mm)
    {
        g_dptdbg.info = NULL;
        g_dptdbg.stopping = false;
        spin_unlock_irqrestore(&g_dptdbg.lock, flags);
        inline_hook_remove(g_dptdbg_switch_hook);
        for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg_sync_hooks); index++) inline_hook_remove(g_dptdbg_sync_hooks[index]);
        synchronize_rcu();
        return;
    }
    g_dptdbg.stopping = true;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);

    inline_hook_remove(g_dptdbg_switch_hook);
    synchronize_rcu();
    dptdbg_restore_saved_thread_views();
    on_each_cpu(dptdbg_apply_current_thread_view, NULL, 1);
    dptdbg_flush_view_translations();

    if (g_dptdbg.mmu_notifier.ops)
    {
        void (*unregister_fn)(struct mmu_notifier *subscription, struct mm_struct *mm) = (void *)generic_kallsyms_lookup_name("mmu_notifier_unregister");
        if (unregister_fn) unregister_fn(&g_dptdbg.mmu_notifier, g_dptdbg.mm);
        g_dptdbg.mmu_notifier.ops = NULL;
    }

    spin_lock_irqsave(&g_dptdbg.lock, flags);
    g_dptdbg.info = NULL;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);
    for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg_sync_hooks); index++) inline_hook_remove(g_dptdbg_sync_hooks[index]);
    synchronize_rcu();

    struct page *trap_pages[BP_CONFIG_MAX] = {NULL};
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    struct mm_struct *mm = g_dptdbg.mm;
    for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg.pages); index++)
    {
        if (!g_dptdbg.pages[index].page_vaddr) break;
        trap_pages[index] = g_dptdbg.pages[index].trap_page;
    }

    struct page *shadow_pgd_page = g_dptdbg.shadow_pgd_page;

    g_dptdbg.mm = NULL;
    __builtin_memset(g_dptdbg.pages, 0, sizeof(g_dptdbg.pages));
    g_dptdbg.shadow_pgd_page = NULL;
    g_dptdbg.stopping = false;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);

    for (size_t index = 0; index < ARRAY_SIZE(trap_pages); index++)
        if (trap_pages[index]) __free_page(trap_pages[index]);
    for (size_t index = ARRAY_SIZE(g_dptdbg.table_pages); index > 0; index--)
    {
        struct page *page = g_dptdbg.table_pages[index - 1];
        g_dptdbg.table_pages[index - 1] = NULL;
        if (page) __free_page(page);
    }
    if (shadow_pgd_page) __free_page(shadow_pgd_page);
    mmput(mm);
}

static void dptdbg_abort_workfn(struct work_struct *work)
{
    (void)work;
    mutex_lock(&g_dptdbg.lifecycle_lock);
    unsigned long flags;
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    bool should_release = g_dptdbg.mm && g_dptdbg.stopping;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);
    if (should_release) dptdbg_release_state();
    mutex_unlock(&g_dptdbg.lifecycle_lock);
}

static inline void dptdbg_stop_monitor(void)
{
    mutex_lock(&g_dptdbg.lifecycle_lock);
    dptdbg_release_state();
    mutex_unlock(&g_dptdbg.lifecycle_lock);
    if (g_dptdbg.abort_work.func) cancel_work_sync(&g_dptdbg.abort_work);
}

static struct page *dptdbg_clone_table_page(void *source_table)
{
    if (!source_table) return ERR_PTR(-EINVAL);
    size_t slot = 0;
    while (slot < ARRAY_SIZE(g_dptdbg.table_pages) && g_dptdbg.table_pages[slot]) slot++;
    if (slot == ARRAY_SIZE(g_dptdbg.table_pages)) return ERR_PTR(-ENOSPC);
    struct page *page = alloc_page(GFP_KERNEL);
    if (!page) return ERR_PTR(-ENOMEM);
    void *destination = page_address(page);
    if (!destination)
    {
        __free_page(page);
        return ERR_PTR(-EFAULT);
    }
    __builtin_memcpy(destination, (void *)((unsigned long)source_table & PAGE_MASK), PAGE_SIZE);
    g_dptdbg.table_pages[slot] = page;
    return page;
}

__attribute__((no_sanitize("cfi"))) static int dptdbg_start_monitor(struct break_point *info)
{
    if (!bp_info_find_configured_type(info, BP_BREAKPOINT_X, NULL)) return -EINVAL;

    mutex_lock(&g_dptdbg.lifecycle_lock);
    if (!g_dptdbg.abort_work.func) INIT_WORK(&g_dptdbg.abort_work, dptdbg_abort_workfn);
    dptdbg_release_state();

    int status = -ENOENT;
    for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg_sync_hooks); index++)
    {
        status = inline_hook_install(g_dptdbg_sync_hooks[index]);
        if (!status) break;
    }
    if (status) goto out_unlock;
    status = inline_hook_install(g_dptdbg_switch_hook);
    if (status) goto out_release;

    struct mm_struct *mm = get_mm_by_pid(READ_ONCE(info->tgid));
    if (!mm)
    {
        status = -EINVAL;
        goto out_release;
    }
    unsigned long flags;
    spin_lock_irqsave(&g_dptdbg.lock, flags);
    g_dptdbg.mm = mm;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);

    int (*register_fn)(struct mmu_notifier *subscription, struct mm_struct *mm) = (void *)generic_kallsyms_lookup_name("mmu_notifier_register");
    if (!register_fn || !generic_kallsyms_lookup_name("mmu_notifier_unregister"))
    {
        status = -ENOENT;
        goto out_release;
    }
    g_dptdbg.mmu_notifier.ops = &dptdbg_mmu_notifier_ops;
    status = register_fn(&g_dptdbg.mmu_notifier, mm);
    if (status)
    {
        g_dptdbg.mmu_notifier.ops = NULL;
        goto out_release;
    }

    mmap_read_lock(mm);
    size_t next_slot = 0;
    struct bp_point *point;
    while ((point = bp_info_find_configured_type(info, BP_BREAKPOINT_X, &next_slot)))
    {
        uint64_t hook_addr = untagged_addr(READ_ONCE(point->hit_addr));
        if (hook_addr & (sizeof(uint32_t) - 1U))
        {
            status = -EINVAL;
            goto out_mmap;
        }
        uint64_t page_vaddr = hook_addr & PAGE_MASK;
        if (!hook_addr || hook_addr >= mm->task_size || sizeof(uint32_t) > mm->task_size - hook_addr || page_vaddr > ULONG_MAX - PAGE_SIZE)
        {
            status = -EFAULT;
            goto out_mmap;
        }
        if (bp_info_find_point_by_pc(info, hook_addr) != point)
        {
            status = -EEXIST;
            goto out_mmap;
        }

        struct dptdbg_page *managed_page = dptdbg_find_page(page_vaddr);
        if (!managed_page)
        {
            for (size_t page_index = 0; page_index < ARRAY_SIZE(g_dptdbg.pages); page_index++)
            {
                if (!g_dptdbg.pages[page_index].page_vaddr)
                {
                    managed_page = &g_dptdbg.pages[page_index];
                    break;
                }
            }
            if (!managed_page)
            {
                status = -ENOSPC;
                goto out_mmap;
            }
            struct vm_area_struct *vma = find_vma(mm, page_vaddr);
            if (!vma || vma->vm_start > page_vaddr || vma->vm_end < page_vaddr + PAGE_SIZE)
            {
                status = -EFAULT;
                goto out_mmap;
            }
            vm_flags_t vm_flags = READ_ONCE(vma->vm_flags);
            if ((vm_flags & (VM_EXEC | VM_MAYWRITE)) != (VM_EXEC | VM_MAYWRITE))
            {
                status = -EACCES;
                goto out_mmap;
            }
            if (vm_flags & (VM_WRITE | VM_SHARED | VM_MAYSHARE | VM_HUGETLB | VM_PFNMAP | VM_MIXEDMAP | VM_IO))
            {
                status = -EOPNOTSUPP;
                goto out_mmap;
            }

            pteval_t mapping_value;
            status = read_user_pte_value(mm, page_vaddr, &mapping_value);
            if (status) goto out_mmap;
            pte_t mapping = __pte(mapping_value);
            if (!pte_present(mapping) || !pfn_valid(pte_pfn(mapping)))
            {
                status = -EFAULT;
                goto out_mmap;
            }
            if (mapping_value & PTE_UXN)
            {
                status = -EACCES;
                goto out_mmap;
            }

            managed_page->trap_page = alloc_page(GFP_KERNEL);
            if (!managed_page->trap_page)
            {
                status = -ENOMEM;
                goto out_mmap;
            }
            void *trap_mapping = page_address(managed_page->trap_page);
            void *source_mapping = page_address(pfn_to_page(pte_pfn(mapping)));
            if (!trap_mapping || !source_mapping)
            {
                __free_page(managed_page->trap_page);
                managed_page->trap_page = NULL;
                status = -EFAULT;
                goto out_mmap;
            }
            __builtin_memcpy(trap_mapping, source_mapping, PAGE_SIZE);
            managed_page->original_mapping = mapping;
            managed_page->page_vaddr = page_vaddr;
        }

        uint32_t *trap_code = page_address(managed_page->trap_page);
        if (!trap_code)
        {
            status = -EFAULT;
            goto out_mmap;
        }
        size_t inst_index;
        if (!dptdbg_get_inst_index(managed_page->page_vaddr, hook_addr, &inst_index))
        {
            status = -EFAULT;
            goto out_mmap;
        }
        if (trap_code[inst_index] == 0U)
        {
            status = -ESTALE;
            goto out_mmap;
        }
        managed_page->original_insts[point - info->points] = trap_code[inst_index];
        WRITE_ONCE(trap_code[inst_index], 0U);
    }

    g_dptdbg.shadow_pgd_page = alloc_page(GFP_KERNEL | __GFP_ZERO);
    if (!g_dptdbg.shadow_pgd_page)
    {
        status = -ENOMEM;
        goto out_mmap;
    }
    pgd_t *shadow_root = page_address(g_dptdbg.shadow_pgd_page);
    if (!shadow_root)
    {
        status = -EFAULT;
        goto out_mmap;
    }
    __builtin_memcpy(shadow_root, mm->pgd, PGD_SIZE);
#if defined(PTE_ADDR_MASK)
    const pteval_t address_mask = PTE_ADDR_MASK;
#elif defined(PHYS_TO_PTE_ADDR_MASK)
    const pteval_t address_mask = PHYS_TO_PTE_ADDR_MASK;
#elif defined(PTE_ADDR_HIGH)
    const pteval_t address_mask = PTE_ADDR_LOW | PTE_ADDR_HIGH;
#elif defined(PTE_ADDR_LOW)
    const pteval_t address_mask = PTE_ADDR_LOW;
#else
#error "Unsupported arm64 page-table address mask"
#endif
    for (size_t page_index = 0; page_index < ARRAY_SIZE(g_dptdbg.pages); page_index++)
    {
        if (!g_dptdbg.pages[page_index].page_vaddr) break;
        uint64_t addr = g_dptdbg.pages[page_index].page_vaddr;
        pgd_t *source_pgd = pgd_offset(mm, addr);
        pgd_t *shadow_pgd = shadow_root + pgd_index(addr);
        if (pgd_none(*source_pgd) || pgd_bad(*source_pgd))
        {
            status = -EFAULT;
            goto out_mmap;
        }

        p4d_t *source_p4d = p4d_offset(source_pgd, addr);
        p4d_t *shadow_p4d = p4d_offset(shadow_pgd, addr);
#if CONFIG_PGTABLE_LEVELS > 4
        if (p4d_none(*source_p4d) || p4d_bad(*source_p4d))
        {
            status = -EFAULT;
            goto out_mmap;
        }
        if (pgd_val(*shadow_pgd) == pgd_val(*source_pgd))
        {
            struct page *page = dptdbg_clone_table_page(source_p4d);
            if (IS_ERR(page))
            {
                status = PTR_ERR(page);
                goto out_mmap;
            }
            WRITE_ONCE(*shadow_pgd, __pgd((pgd_val(*source_pgd) & ~address_mask) | __phys_to_pgd_val(page_to_phys(page))));
            shadow_p4d = p4d_offset(shadow_pgd, addr);
        }
#endif

        pud_t *source_pud = pud_offset(source_p4d, addr);
        pud_t *shadow_pud = pud_offset(shadow_p4d, addr);
#if CONFIG_PGTABLE_LEVELS > 3
        if (pud_none(*source_pud) || pud_leaf(*source_pud) || pud_bad(*source_pud))
        {
            status = -EOPNOTSUPP;
            goto out_mmap;
        }
        if (p4d_val(*shadow_p4d) == p4d_val(*source_p4d))
        {
            struct page *page = dptdbg_clone_table_page(source_pud);
            if (IS_ERR(page))
            {
                status = PTR_ERR(page);
                goto out_mmap;
            }
#if CONFIG_PGTABLE_LEVELS > 4
            WRITE_ONCE(*shadow_p4d, __p4d((p4d_val(*source_p4d) & ~address_mask) | __phys_to_p4d_val(page_to_phys(page))));
#else
            WRITE_ONCE(*shadow_pgd, __pgd((pgd_val(*source_pgd) & ~address_mask) | __phys_to_pgd_val(page_to_phys(page))));
#endif
            shadow_pud = pud_offset(p4d_offset(shadow_pgd, addr), addr);
        }
#endif

        pmd_t *source_pmd = pmd_offset(source_pud, addr);
        pmd_t *shadow_pmd = pmd_offset(shadow_pud, addr);
#if CONFIG_PGTABLE_LEVELS > 2
        if (pmd_none(*source_pmd) || pmd_leaf(*source_pmd) || pmd_bad(*source_pmd))
        {
            status = -EOPNOTSUPP;
            goto out_mmap;
        }
        if (pud_val(*shadow_pud) == pud_val(*source_pud))
        {
            struct page *page = dptdbg_clone_table_page(source_pmd);
            if (IS_ERR(page))
            {
                status = PTR_ERR(page);
                goto out_mmap;
            }
#if CONFIG_PGTABLE_LEVELS > 3
            WRITE_ONCE(*shadow_pud, __pud((pud_val(*source_pud) & ~address_mask) | __phys_to_pud_val(page_to_phys(page))));
#else
            WRITE_ONCE(*shadow_pgd, __pgd((pgd_val(*source_pgd) & ~address_mask) | __phys_to_pgd_val(page_to_phys(page))));
#endif
            shadow_pmd = pmd_offset(pud_offset(p4d_offset(shadow_pgd, addr), addr), addr);
        }
#endif

        pte_t *source_pte = pte_offset_kernel(source_pmd, addr);
        if (pmd_val(*shadow_pmd) == pmd_val(*source_pmd))
        {
            struct page *page = dptdbg_clone_table_page(source_pte);
            if (IS_ERR(page))
            {
                status = PTR_ERR(page);
                goto out_mmap;
            }
#if CONFIG_PGTABLE_LEVELS > 2
            WRITE_ONCE(*shadow_pmd, __pmd((pmd_val(*source_pmd) & ~address_mask) | __phys_to_pmd_val(page_to_phys(page))));
#else
            WRITE_ONCE(*shadow_pgd, __pgd((pgd_val(*source_pgd) & ~address_mask) | __phys_to_pgd_val(page_to_phys(page))));
#endif
        }
        pte_t *shadow_pte = pte_offset_kernel(pmd_offset(pud_offset(p4d_offset(shadow_pgd, addr), addr), addr), addr);
        if (!source_pte || !shadow_pte || !pte_present(*source_pte) || !pte_present(*shadow_pte))
        {
            status = -EFAULT;
            goto out_mmap;
        }
        if ((pte_val(*source_pte) & (PTE_UXN | PTE_CONT)) || !pfn_valid(pte_pfn(*source_pte)))
        {
            status = -EOPNOTSUPP;
            goto out_mmap;
        }
        if (pte_pfn(*source_pte) != pte_pfn(g_dptdbg.pages[page_index].original_mapping) || pte_pfn(*shadow_pte) != pte_pfn(*source_pte))
        {
            status = -ESTALE;
            goto out_mmap;
        }

        WRITE_ONCE(*shadow_pte, __pte((pte_val(*source_pte) & ~address_mask) | (__phys_to_pte_val(page_to_phys(g_dptdbg.pages[page_index].trap_page)) & address_mask)));
    }
    dsb(ishst);

    for (size_t page_index = 0; page_index < ARRAY_SIZE(g_dptdbg.pages); page_index++)
    {
        if (!g_dptdbg.pages[page_index].page_vaddr) continue;
        void *trap_code = page_address(g_dptdbg.pages[page_index].trap_page);
        if (!trap_code)
        {
            status = -EFAULT;
            goto out_mmap;
        }
        status = arm64_sync_code_range_all_cpus(trap_code, PAGE_SIZE);
        if (status) goto out_mmap;
    }
    mmap_read_unlock(mm);

    spin_lock_irqsave(&g_dptdbg.lock, flags);
    if (g_dptdbg.stopping) status = -ESTALE;
    else g_dptdbg.info = info;
    spin_unlock_irqrestore(&g_dptdbg.lock, flags);
    if (status) goto out_release;

    on_each_cpu(dptdbg_apply_current_thread_view, NULL, 1);
    goto out_unlock;
out_mmap:
    mmap_read_unlock(mm);
out_release:
    dptdbg_release_state();
out_unlock:
    mutex_unlock(&g_dptdbg.lifecycle_lock);
    return status;
}

#endif // ARM64_DPTDBG_H