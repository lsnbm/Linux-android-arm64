#ifndef ARM64_DPTDBG_H
#define ARM64_DPTDBG_H

/*
 * The process main thread runs with a private shadow PGD that maps trap copies
 * of the managed code pages. Every other thread keeps the normal page table.
 */
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/list.h>
#include <linux/mm.h>
#include <linux/mmu_notifier.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/slab.h>
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
#include "io_struct.h"

#define DPTDBG_UDF_INST        0x00000000U
#define DPTDBG_TTBR_BADDR_MASK GENMASK_ULL(47, 2)
#if defined(PTE_ADDR_MASK)
#define DPTDBG_PTE_ADDR_MASK   PTE_ADDR_MASK
#elif defined(PHYS_TO_PTE_ADDR_MASK)
#define DPTDBG_PTE_ADDR_MASK   PHYS_TO_PTE_ADDR_MASK
#elif defined(PTE_ADDR_HIGH)
#define DPTDBG_PTE_ADDR_MASK   (PTE_ADDR_LOW | PTE_ADDR_HIGH)
#elif defined(PTE_ADDR_LOW)
#define DPTDBG_PTE_ADDR_MASK   PTE_ADDR_LOW
#else
#error "Unsupported arm64 page-table address mask"
#endif

struct dptdbg_table_page
{
    struct list_head node;
    struct page *page;
};

struct dptdbg_page
{
    pte_t original_mapping;
    struct page *trap_page;
    uint64_t page_vaddr;
};

static struct break_point *g_dptdbg_info;
static struct mm_struct *g_dptdbg_mm;
static struct dptdbg_page g_dptdbg_pages[BP_CONFIG_MAX];
static uint32_t g_dptdbg_original_insts[BP_CONFIG_MAX];
static struct page *g_dptdbg_shadow_pgd_page;
static pgd_t *g_dptdbg_shadow_pgd;
static LIST_HEAD(g_dptdbg_table_pages);
static struct mmu_notifier g_dptdbg_mmu_notifier;
static DEFINE_SPINLOCK(g_dptdbg_lock);
static DEFINE_MUTEX(g_dptdbg_lifecycle_lock);
static struct work_struct g_dptdbg_abort_work;
static bool g_dptdbg_work_initialized;
static bool g_dptdbg_notifier_registered;
static bool g_dptdbg_stopping;
static pid_t g_dptdbg_tgid;

static struct dptdbg_page *dptdbg_find_page(uint64_t page_vaddr)
{
    for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg_pages); index++)
        if (g_dptdbg_pages[index].page_vaddr == page_vaddr) return &g_dptdbg_pages[index];

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
    unsigned long flags;
    bool admitted;

    rcu_read_lock();
    spin_lock_irqsave(&g_dptdbg_lock, flags);
    admitted = g_dptdbg_info != NULL;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);
    if (!admitted) rcu_read_unlock();
    return admitted;
}

static inline void dptdbg_handler_leave(void)
{
    rcu_read_unlock();
}

static bool dptdbg_task_is_target_locked(const struct task_struct *task)
{
    return task && g_dptdbg_mm && g_dptdbg_tgid > 0 && task->mm == g_dptdbg_mm && READ_ONCE(task->tgid) == g_dptdbg_tgid;
}

static bool dptdbg_task_needs_clean_view_locked(struct task_struct *task)
{
    return !g_dptdbg_stopping && dptdbg_task_is_target_locked(task) && thread_group_leader(task);
}

static struct page *dptdbg_clone_table_page(void *source_table)
{
    struct dptdbg_table_page *table_page;
    void *shadow_mapping;

    if (!source_table) return ERR_PTR(-EINVAL);
    source_table = (void *)((unsigned long)source_table & PAGE_MASK);
    table_page = kmalloc(sizeof(*table_page), GFP_KERNEL);
    if (!table_page) return ERR_PTR(-ENOMEM);
    table_page->page = alloc_page(GFP_KERNEL | __GFP_ZERO);
    if (!table_page->page)
    {
        kfree(table_page);
        return ERR_PTR(-ENOMEM);
    }
    shadow_mapping = page_address(table_page->page);
    if (!shadow_mapping)
    {
        __free_page(table_page->page);
        kfree(table_page);
        return ERR_PTR(-EFAULT);
    }
    __builtin_memcpy(shadow_mapping, source_table, PAGE_SIZE);
    list_add_tail(&table_page->node, &g_dptdbg_table_pages);
    return table_page->page;
}

static int dptdbg_build_shadow_path(struct dptdbg_page *managed_page)
{
    uint64_t addr = managed_page->page_vaddr;
    pgd_t *source_pgd = pgd_offset(g_dptdbg_mm, addr);
    pgd_t *shadow_pgd = g_dptdbg_shadow_pgd + pgd_index(addr);

    if (pgd_none(*source_pgd) || pgd_bad(*source_pgd)) return -EFAULT;

    p4d_t *source_p4d = p4d_offset(source_pgd, addr);
    p4d_t *shadow_p4d = p4d_offset(shadow_pgd, addr);
#if CONFIG_PGTABLE_LEVELS > 4
    if (p4d_none(*source_p4d) || p4d_bad(*source_p4d)) return -EFAULT;
    if (pgd_val(*shadow_pgd) == pgd_val(*source_pgd))
    {
        struct page *page = dptdbg_clone_table_page(source_p4d);

        if (IS_ERR(page)) return PTR_ERR(page);
        WRITE_ONCE(*shadow_pgd, __pgd((pgd_val(*source_pgd) & ~DPTDBG_PTE_ADDR_MASK) | __phys_to_pgd_val(page_to_phys(page))));
        shadow_p4d = p4d_offset(shadow_pgd, addr);
    }
#endif

    pud_t *source_pud = pud_offset(source_p4d, addr);
    pud_t *shadow_pud = pud_offset(shadow_p4d, addr);
#if CONFIG_PGTABLE_LEVELS > 3
    if (pud_none(*source_pud) || pud_leaf(*source_pud) || pud_bad(*source_pud)) return -EOPNOTSUPP;
    if (p4d_val(*shadow_p4d) == p4d_val(*source_p4d))
    {
        struct page *page = dptdbg_clone_table_page(source_pud);

        if (IS_ERR(page)) return PTR_ERR(page);
#if CONFIG_PGTABLE_LEVELS > 4
        WRITE_ONCE(*shadow_p4d, __p4d((p4d_val(*source_p4d) & ~DPTDBG_PTE_ADDR_MASK) | __phys_to_p4d_val(page_to_phys(page))));
#else
        WRITE_ONCE(*shadow_pgd, __pgd((pgd_val(*source_pgd) & ~DPTDBG_PTE_ADDR_MASK) | __phys_to_pgd_val(page_to_phys(page))));
#endif
        shadow_pud = pud_offset(p4d_offset(shadow_pgd, addr), addr);
    }
#endif

    pmd_t *source_pmd = pmd_offset(source_pud, addr);
    pmd_t *shadow_pmd = pmd_offset(shadow_pud, addr);
#if CONFIG_PGTABLE_LEVELS > 2
    if (pmd_none(*source_pmd) || pmd_leaf(*source_pmd) || pmd_bad(*source_pmd)) return -EOPNOTSUPP;
    if (pud_val(*shadow_pud) == pud_val(*source_pud))
    {
        struct page *page = dptdbg_clone_table_page(source_pmd);

        if (IS_ERR(page)) return PTR_ERR(page);
#if CONFIG_PGTABLE_LEVELS > 3
        WRITE_ONCE(*shadow_pud, __pud((pud_val(*source_pud) & ~DPTDBG_PTE_ADDR_MASK) | __phys_to_pud_val(page_to_phys(page))));
#else
        WRITE_ONCE(*shadow_pgd, __pgd((pgd_val(*source_pgd) & ~DPTDBG_PTE_ADDR_MASK) | __phys_to_pgd_val(page_to_phys(page))));
#endif
        shadow_pmd = pmd_offset(pud_offset(p4d_offset(shadow_pgd, addr), addr), addr);
    }
#endif

    pte_t *source_pte = pte_offset_kernel(source_pmd, addr);
    if (pmd_val(*shadow_pmd) == pmd_val(*source_pmd))
    {
        struct page *page = dptdbg_clone_table_page(source_pte);

        if (IS_ERR(page)) return PTR_ERR(page);
#if CONFIG_PGTABLE_LEVELS > 2
        WRITE_ONCE(*shadow_pmd, __pmd((pmd_val(*source_pmd) & ~DPTDBG_PTE_ADDR_MASK) | __phys_to_pmd_val(page_to_phys(page))));
#else
        WRITE_ONCE(*shadow_pgd, __pgd((pgd_val(*source_pgd) & ~DPTDBG_PTE_ADDR_MASK) | __phys_to_pgd_val(page_to_phys(page))));
#endif
    }
    pte_t *shadow_pte = pte_offset_kernel(pmd_offset(pud_offset(p4d_offset(shadow_pgd, addr), addr), addr), addr);
    pteval_t mapping_value;

    if (!source_pte || !shadow_pte || !pte_present(*source_pte) || !pte_present(*shadow_pte)) return -EFAULT;
    if ((pte_val(*source_pte) & (PTE_UXN | PTE_CONT)) || !pfn_valid(pte_pfn(*source_pte))) return -EOPNOTSUPP;
    if (pte_pfn(*source_pte) != pte_pfn(managed_page->original_mapping) || pte_pfn(*shadow_pte) != pte_pfn(*source_pte)) return -ESTALE;

    mapping_value = pte_val(*source_pte);
    WRITE_ONCE(*shadow_pte, __pte((mapping_value & ~DPTDBG_PTE_ADDR_MASK) | (__phys_to_pte_val(page_to_phys(managed_page->trap_page)) & DPTDBG_PTE_ADDR_MASK)));
    dsb(ishst);
    return 0;
}

static void dptdbg_release_table_pages(void)
{
    struct dptdbg_table_page *table_page;
    struct dptdbg_table_page *next_table_page;

    list_for_each_entry_safe(table_page, next_table_page, &g_dptdbg_table_pages, node)
    {
        list_del(&table_page->node);
        __free_page(table_page->page);
        kfree(table_page);
    }
}

static void dptdbg_flush_view_translations(void)
{
    struct mm_struct *mm = READ_ONCE(g_dptdbg_mm);

    if (!mm) return;
    flush_tlb_mm(mm);
    isb();
}

static uint64_t dptdbg_compose_ttbr0(uint64_t template, phys_addr_t pgd_pa, bool clean_view)
{
    uint64_t ttbr = (template & ~DPTDBG_TTBR_BADDR_MASK) | ((uint64_t)phys_to_ttbr(pgd_pa) & DPTDBG_TTBR_BADDR_MASK);

    if (clean_view) ttbr &= ~TTBR_CNP_BIT;
    return ttbr;
}

static bool dptdbg_write_ttbr0_locked(phys_addr_t pgd_pa, bool clean_view)
{
    uint64_t current_ttbr;
    uint64_t next_ttbr;

    if (!pgd_pa) return false;
    current_ttbr = read_sysreg(ttbr0_el1);
    next_ttbr = dptdbg_compose_ttbr0(current_ttbr, pgd_pa, clean_view);
    if (current_ttbr == next_ttbr) return false;

    write_sysreg(next_ttbr, ttbr0_el1);
    isb();
    return true;
}

static bool dptdbg_write_task_view_locked(struct task_struct *task, bool clean_view)
{
    phys_addr_t pgd_pa;

    if (!task || !g_dptdbg_mm || !g_dptdbg_shadow_pgd_page) return false;
    pgd_pa = clean_view ? page_to_phys(g_dptdbg_shadow_pgd_page) : virt_to_phys(g_dptdbg_mm->pgd);
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
    struct task_struct *next;
    unsigned long flags;
    bool flush_needed = false;

    if (!hook_regs || !dptdbg_handler_enter()) return 0;
    next = (struct task_struct *)hook_regs->regs[1];
    if (!next)
    {
        dptdbg_handler_leave();
        return 0;
    }

    spin_lock_irqsave(&g_dptdbg_lock, flags);
    if (next->mm == g_dptdbg_mm)
        flush_needed = dptdbg_write_task_view_locked(next, dptdbg_task_needs_clean_view_locked(next));
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);
    if (flush_needed) dptdbg_flush_view_translations();
    dptdbg_handler_leave();
    return 0;
}

static void dptdbg_apply_current_thread_view(void *unused)
{
    struct mm_struct *mm;
    unsigned long flags;
    bool flush_needed = false;

    (void)unused;
    spin_lock_irqsave(&g_dptdbg_lock, flags);
    if (current->mm == g_dptdbg_mm)
        flush_needed = dptdbg_write_task_view_locked(current, dptdbg_task_needs_clean_view_locked(current));
    else if (!system_uses_ttbr0_pan())
    {
        mm = current->mm ? current->mm : current->active_mm;
        if (mm) flush_needed = dptdbg_write_ttbr0_locked(virt_to_phys(mm->pgd), false);
    }
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);
    if (flush_needed) dptdbg_flush_view_translations();
}

static void dptdbg_restore_saved_thread_views(void)
{
    struct task_struct *process;
    struct task_struct *task;
    unsigned long flags;
    struct mm_struct *target_mm;
    pid_t target_tgid;

    if (!system_uses_ttbr0_pan()) return;

    spin_lock_irqsave(&g_dptdbg_lock, flags);
    target_mm = g_dptdbg_mm;
    target_tgid = g_dptdbg_tgid;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);

    if (!target_mm || target_tgid <= 0) return;

    rcu_read_lock();
    for_each_process_thread(process, task)
    {
        if (READ_ONCE(task->tgid) != target_tgid || task->mm != target_mm) continue;
        spin_lock_irqsave(&g_dptdbg_lock, flags);
        if (g_dptdbg_mm == target_mm && g_dptdbg_tgid == target_tgid) (void)dptdbg_write_task_view_locked(task, false);
        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
    }
    rcu_read_unlock();
}

static void dptdbg_abort_monitor(void)
{
    unsigned long flags;
    bool flush_needed = false;
    bool schedule_cleanup = false;

    spin_lock_irqsave(&g_dptdbg_lock, flags);
    if (!g_dptdbg_mm)
    {
        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
        return;
    }

    if (!g_dptdbg_stopping)
    {
        g_dptdbg_stopping = true;
        schedule_cleanup = true;
    }
    if (current->mm == g_dptdbg_mm) flush_needed = dptdbg_write_task_view_locked(current, false);
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);

    if (flush_needed || schedule_cleanup) dptdbg_flush_view_translations();
    if (schedule_cleanup) schedule_work(&g_dptdbg_abort_work);
}

static void dptdbg_mmu_notifier_release(struct mmu_notifier *notifier, struct mm_struct *mm)
{
    unsigned long flags;
    bool schedule_cleanup;

    (void)notifier;
    spin_lock_irqsave(&g_dptdbg_lock, flags);
    if (g_dptdbg_mm != mm)
    {
        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
        return;
    }
    schedule_cleanup = !g_dptdbg_stopping;
    g_dptdbg_stopping = true;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);

    dptdbg_restore_saved_thread_views();
    on_each_cpu(dptdbg_apply_current_thread_view, NULL, 1);
    dptdbg_flush_view_translations();
    if (schedule_cleanup) schedule_work(&g_dptdbg_abort_work);
}

static int dptdbg_mmu_notifier_invalidate_range_start(struct mmu_notifier *notifier, const struct mmu_notifier_range *range)
{
    unsigned long flags;
    bool active;

    (void)notifier;
    if (!mmu_notifier_range_blockable(range)) return -EAGAIN;

    spin_lock_irqsave(&g_dptdbg_lock, flags);
    active = g_dptdbg_mm == range->mm;
    if (active) g_dptdbg_stopping = true;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);
    if (!active) return 0;

    dptdbg_restore_saved_thread_views();
    on_each_cpu(dptdbg_apply_current_thread_view, NULL, 1);
    dptdbg_flush_view_translations();
    schedule_work(&g_dptdbg_abort_work);
    return 0;
}

static const struct mmu_notifier_ops g_dptdbg_mmu_notifier_ops = {
    .release = dptdbg_mmu_notifier_release,
    .invalidate_range_start = dptdbg_mmu_notifier_invalidate_range_start,
};

static int dptdbg_handle_undef_sync(struct pt_regs *hook_regs)
{
    struct fp_regs fp_regs __attribute__((__uninitialized__));
    struct dptdbg_page *managed_page;
    struct bp_point *hit_point = NULL;
    struct pt_regs *regs;
    void (*on_hit)(void *regs, void *fp_regs, void *hit_point) = NULL;
    uint32_t original_inst = 0;
    uint64_t pc;
    uint32_t *trap_code;
    unsigned long flags;
    size_t point_slot;
    size_t inst_index;
    bool target_main;
    bool exiting;
    bool stopping;
    bool abort_for_fault = false;
    unsigned int exception_class;
    int result = 0;

    if (!hook_regs || !dptdbg_handler_enter()) return 0;
    exception_class = ESR_ELx_EC(read_sysreg(esr_el1));
    if (exception_class != ESR_ELx_EC_UNKNOWN)
    {
        if ((exception_class == ESR_ELx_EC_IABT_LOW || exception_class == ESR_ELx_EC_DABT_LOW) && current->mm)
        {
            spin_lock_irqsave(&g_dptdbg_lock, flags);
            abort_for_fault = g_dptdbg_mm == current->mm;
            spin_unlock_irqrestore(&g_dptdbg_lock, flags);
            if (abort_for_fault) dptdbg_abort_monitor();
        }
        goto out;
    }
    regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];
    if (!regs || !current->mm || !user_mode(regs)) goto out;

    pc = untagged_addr(regs->pc);
    spin_lock_irqsave(&g_dptdbg_lock, flags);
    if (g_dptdbg_mm != current->mm || !g_dptdbg_info)
    {
        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
        goto out;
    }
    hit_point = bp_info_find_point_by_pc(g_dptdbg_info, pc);
    if (!hit_point)
    {
        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
        goto out;
    }
    point_slot = hit_point - g_dptdbg_info->points;
    managed_page = dptdbg_find_page(pc & PAGE_MASK);
    if (!managed_page || !managed_page->trap_page)
    {
        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
        goto out;
    }
    trap_code = page_address(managed_page->trap_page);
    if (!trap_code || !dptdbg_get_inst_index(managed_page->page_vaddr, pc, &inst_index) || READ_ONCE(trap_code[inst_index]) != DPTDBG_UDF_INST)
    {
        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
        goto out;
    }
    original_inst = g_dptdbg_original_insts[point_slot];

    stopping = g_dptdbg_stopping;
    exiting = !!(READ_ONCE(current->flags) & PF_EXITING);
    target_main = dptdbg_task_is_target_locked(current) && thread_group_leader(current);
    if (stopping || exiting)
    {
        bool emulated;

        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
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
        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
        if (flush_needed) dptdbg_flush_view_translations();
        goto out;
    }
    on_hit = READ_ONCE(hit_point->on_hit);
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);

    result = 1;
    read_all_q_regs(&fp_regs);
    if (on_hit) on_hit(regs, &fp_regs, hit_point);
    if (regs->pc == pc && !emulate_inst(regs, &fp_regs, original_inst))
    {
        dptdbg_abort_monitor();
    }
    write_all_q_regs(&fp_regs);
out:
    dptdbg_handler_leave();
    return result;
}

static struct hook_entry g_dptdbg_sync_hooks[][1] = {
    {HOOK_ENTRY("el0t_64_sync_handler", dptdbg_handle_undef_sync)},
    {HOOK_ENTRY("el0_sync_handler", dptdbg_handle_undef_sync)},
};

static struct hook_entry g_dptdbg_switch_hook[] = {
    HOOK_ENTRY("__switch_to", dptdbg_handle_switch),
};

static void dptdbg_quiesce_hooks(void)
{
    inline_hook_remove(g_dptdbg_switch_hook);
    for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg_sync_hooks); index++) inline_hook_remove(g_dptdbg_sync_hooks[index]);
    synchronize_rcu();
}

static int dptdbg_install_point(struct break_point *info, struct bp_point *point)
{
    struct dptdbg_page *managed_page;
    struct bp_point *duplicate_point;
    struct vm_area_struct *vma;
    struct page *source_page;
    uint32_t *trap_code;
    uint64_t hook_addr;
    pte_t mapping;
    pteval_t mapping_value;
    vm_flags_t vm_flags;
    void *source_mapping;
    void *trap_mapping;
    size_t point_slot;
    size_t inst_index;
    int status;

    if (!point || (READ_ONCE(point->hit_addr) & 0x3ULL)) return -EINVAL;
    point_slot = point - info->points;
    hook_addr = untagged_addr(READ_ONCE(point->hit_addr));
    if (!hook_addr || hook_addr >= g_dptdbg_mm->task_size || sizeof(uint32_t) > g_dptdbg_mm->task_size - hook_addr) return -EFAULT;

    duplicate_point = bp_info_find_point_by_pc(info, hook_addr);
    if (duplicate_point != point) return -EEXIST;

    managed_page = dptdbg_find_page(hook_addr & PAGE_MASK);
    if (!managed_page)
    {
        managed_page = NULL;
        for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg_pages); index++)
        {
            if (!g_dptdbg_pages[index].page_vaddr)
            {
                managed_page = &g_dptdbg_pages[index];
                break;
            }
        }
        if (!managed_page) return -ENOSPC;

        if (!g_dptdbg_mm || (hook_addr & PAGE_MASK) > ULONG_MAX - PAGE_SIZE) return -EINVAL;
        vma = find_vma(g_dptdbg_mm, hook_addr & PAGE_MASK);
        if (!vma || vma->vm_start > (hook_addr & PAGE_MASK) || vma->vm_end < (hook_addr & PAGE_MASK) + PAGE_SIZE) return -EFAULT;
        vm_flags = READ_ONCE(vma->vm_flags);
        if (!(vm_flags & VM_EXEC) || !(vm_flags & VM_MAYWRITE)) return -EACCES;
        if (vm_flags & (VM_WRITE | VM_SHARED | VM_MAYSHARE | VM_HUGETLB | VM_PFNMAP | VM_MIXEDMAP | VM_IO)) return -EOPNOTSUPP;

        status = read_user_pte_value(g_dptdbg_mm, hook_addr & PAGE_MASK, &mapping_value);
        if (status) return status;
        mapping = __pte(mapping_value);
        if (!pte_present(mapping) || !pfn_valid(pte_pfn(mapping))) return -EFAULT;
        if (mapping_value & PTE_UXN) return -EACCES;

        source_page = pfn_to_page(pte_pfn(mapping));
        source_mapping = page_address(source_page);
        if (!source_mapping) return -EFAULT;

        managed_page->trap_page = alloc_page(GFP_KERNEL | __GFP_ZERO);
        if (!managed_page->trap_page) return -ENOMEM;
        trap_mapping = page_address(managed_page->trap_page);
        if (!trap_mapping)
        {
            __free_page(managed_page->trap_page);
            managed_page->trap_page = NULL;
            return -EFAULT;
        }

        __builtin_memcpy(trap_mapping, source_mapping, PAGE_SIZE);
        managed_page->original_mapping = mapping;
        managed_page->page_vaddr = hook_addr & PAGE_MASK;
    }

    trap_code = page_address(managed_page->trap_page);
    if (!trap_code) return -EFAULT;
    if (!dptdbg_get_inst_index(managed_page->page_vaddr, hook_addr, &inst_index)) return -EFAULT;
    if (trap_code[inst_index] == DPTDBG_UDF_INST) return -ESTALE;

    g_dptdbg_original_insts[point_slot] = trap_code[inst_index];
    WRITE_ONCE(trap_code[inst_index], DPTDBG_UDF_INST);
    return 0;
}

static int dptdbg_sync_trap_pages(void)
{
    for (size_t page_index = 0; page_index < ARRAY_SIZE(g_dptdbg_pages); page_index++)
    {
        struct dptdbg_page *managed_page = &g_dptdbg_pages[page_index];

        if (!managed_page->page_vaddr) continue;
        void *trap_code = page_address(managed_page->trap_page);
        if (!trap_code) return -EFAULT;
        int status = arm64_sync_code_range_all_cpus(trap_code, PAGE_SIZE);
        if (status) return status;
    }
    return 0;
}

static int dptdbg_release_state(void)
{
    struct page *trap_pages[BP_CONFIG_MAX] = {NULL};
    struct page *shadow_pgd_page;
    struct mm_struct *mm;
    unsigned long flags;

    spin_lock_irqsave(&g_dptdbg_lock, flags);
    if (!g_dptdbg_mm)
    {
        g_dptdbg_info = NULL;
        g_dptdbg_tgid = 0;
        g_dptdbg_stopping = false;
        spin_unlock_irqrestore(&g_dptdbg_lock, flags);
        dptdbg_quiesce_hooks();
        return 0;
    }
    g_dptdbg_stopping = true;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);

    inline_hook_remove(g_dptdbg_switch_hook);
    synchronize_rcu();
    dptdbg_restore_saved_thread_views();
    on_each_cpu(dptdbg_apply_current_thread_view, NULL, 1);
    dptdbg_flush_view_translations();

    if (g_dptdbg_notifier_registered)
    {
        mmu_notifier_unregister(&g_dptdbg_mmu_notifier, g_dptdbg_mm);
        g_dptdbg_notifier_registered = false;
    }

    spin_lock_irqsave(&g_dptdbg_lock, flags);
    g_dptdbg_info = NULL;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);
    for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg_sync_hooks); index++) inline_hook_remove(g_dptdbg_sync_hooks[index]);
    synchronize_rcu();

    spin_lock_irqsave(&g_dptdbg_lock, flags);
    mm = g_dptdbg_mm;
    for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg_pages); index++)
    {
        trap_pages[index] = g_dptdbg_pages[index].trap_page;
    }

    shadow_pgd_page = g_dptdbg_shadow_pgd_page;

    g_dptdbg_mm = NULL;
    g_dptdbg_tgid = 0;
    __builtin_memset(g_dptdbg_pages, 0, sizeof(g_dptdbg_pages));
    __builtin_memset(g_dptdbg_original_insts, 0, sizeof(g_dptdbg_original_insts));
    g_dptdbg_shadow_pgd_page = NULL;
    g_dptdbg_shadow_pgd = NULL;
    g_dptdbg_stopping = false;
    g_dptdbg_notifier_registered = false;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);

    for (size_t index = 0; index < ARRAY_SIZE(trap_pages); index++)
        if (trap_pages[index]) __free_page(trap_pages[index]);
    dptdbg_release_table_pages();
    if (shadow_pgd_page) __free_page(shadow_pgd_page);
    mmput(mm);
    return 0;
}

static void dptdbg_abort_workfn(struct work_struct *work)
{
    unsigned long flags;
    bool should_release;

    mutex_lock(&g_dptdbg_lifecycle_lock);
    spin_lock_irqsave(&g_dptdbg_lock, flags);
    should_release = g_dptdbg_mm && g_dptdbg_stopping;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);
    if (should_release) (void)dptdbg_release_state();
    mutex_unlock(&g_dptdbg_lifecycle_lock);
}

static inline void dptdbg_stop_monitor(void)
{
    mutex_lock(&g_dptdbg_lifecycle_lock);
    (void)dptdbg_release_state();
    mutex_unlock(&g_dptdbg_lifecycle_lock);
    if (g_dptdbg_work_initialized) cancel_work_sync(&g_dptdbg_abort_work);
}

static int dptdbg_start_monitor(struct break_point *info)
{
    /*
     * 启动 DPTDBG 的顺序必须保持为：
     *
     *   1. 安装异常入口和任务切换入口，但先禁止 handler 进入；
     *   2. 读取目标进程的执行页，并为断点页建立 trap page；
      *   3. 注册目标 mm 的 MMU notifier，并为受管代码页建立私有 shadow 路径；
     *   4. 在每个 trap page 中写入 UDF，并同步指令缓存；
      *   5. 一次发布运行态，让目标主线程开始使用 shadow view。
        *
      * 构建期间 handler 始终被阻塞，不会接触未完成的页表或断点状态；
      * 目标 mm 的任意页表失效都会冻结并撤销当前实例，避免继续使用陈旧
      * 的 shadow 映射。任一阶段失败时，统一清理路径回收已创建的 trap page、
      * shadow 路径页、mm 引用以及 inline hook。
     */
    struct mm_struct *mm;
    unsigned long flags;
    size_t next_slot = 0;
    struct bp_point *point;
    int status;

    /* next_slot 为空时只查询首个匹配项；在释放旧实例前完成无副作用校验。 */
    if (!bp_info_find_configured_type(info, BP_BREAKPOINT_X, NULL)) return -EINVAL;

    /*
     * start、stop 和异步 abort 共用同一把生命周期锁，避免清理线程在
     * 启动过程的中间状态上再次释放资源。
     */
    mutex_lock(&g_dptdbg_lifecycle_lock);
    if (!g_dptdbg_work_initialized)
    {
        /* abort 可能在异常 handler 中异步排队，因此 work 只初始化一次。 */
        INIT_WORK(&g_dptdbg_abort_work, dptdbg_abort_workfn);
        g_dptdbg_work_initialized = true;
    }
    /*
     * 允许重复设置监控：先完整撤销旧实例，再从干净状态建立新实例。
     * dptdbg_release_state() 内部还会等待正在运行的 handler 离开。
     */
    status = dptdbg_release_state();
    if (status) goto out_unlock;

    /*
    * 不同 Android 内核可能导出不同名字的 EL0 同步入口，按候选表尝试。
    * switch hook 负责在线程切换时重新选择 normal/shadow 页表；它和同步
    * hook 必须成对安装，否则异常可能进入但无法维持正确的线程 view。
     */
    status = -ENOENT;
    for (int index = 0; index < ARRAY_SIZE(g_dptdbg_sync_hooks); index++)
    {
        status = inline_hook_install(g_dptdbg_sync_hooks[index]);
        if (!status) break;
    }
    if (!status)
    {
        status = inline_hook_install(g_dptdbg_switch_hook);
        if (status) for (size_t index = 0; index < ARRAY_SIZE(g_dptdbg_sync_hooks); index++) inline_hook_remove(g_dptdbg_sync_hooks[index]);
    }
    if (status) goto out_unlock;

    mm = get_mm_by_pid(READ_ONCE(info->tgid));
    /* get_mm_by_pid() 返回带引用的 mm，引用由 dptdbg_release_state() 释放。 */
    if (!mm)
    {
        dptdbg_quiesce_hooks();
        status = -EINVAL;
        goto out_unlock;
    }

    /*
    * 先登记目标 mm 和配置，但 handler 仍被阻塞。此时这些全局字段只用于
    * 构建阶段的辅助函数，不代表监控已经生效。
     */
    spin_lock_irqsave(&g_dptdbg_lock, flags);
    g_dptdbg_mm = mm;
    g_dptdbg_tgid = READ_ONCE(info->tgid);
    g_dptdbg_stopping = false;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);

    g_dptdbg_mmu_notifier.ops = &g_dptdbg_mmu_notifier_ops;
    g_dptdbg_notifier_registered = true;
    status = mmu_notifier_register(&g_dptdbg_mmu_notifier, mm);
    if (status)
    {
        g_dptdbg_notifier_registered = false;
        (void)dptdbg_release_state();
        goto out_unlock;
    }

    /*
     * dptdbg_install_point() 需要稳定地读取 VMA 和用户 PTE，并复制原始
     * 指令到独立的 trap page；整个批次建立期间持有 mmap 读锁。
     */
    mmap_read_lock(mm);

    /*
     * 逐个收集所有执行断点。该阶段只保存原始指令和原始映射，还不会写
     * UDF，因此即使中途失败，目标进程也不会看到半成品的断点页。
     */
    status = 0;
    while ((point = bp_info_find_configured_type(info, BP_BREAKPOINT_X, &next_slot)))
    {
        status = dptdbg_install_point(info, point);
        if (status) break;
    }

    if (!status)
    {
        /*
         * 为目标 mm 分配独立 PGD。shadow PGD 只复制受管代码页所需的页表
         * 路径并替换 trap 映射；normal 页表本身不被修改。
         */
        g_dptdbg_shadow_pgd_page = alloc_page(GFP_KERNEL | __GFP_ZERO);
        if (!g_dptdbg_shadow_pgd_page) status = -ENOMEM;
        else
        {
            g_dptdbg_shadow_pgd = page_address(g_dptdbg_shadow_pgd_page);
            if (!g_dptdbg_shadow_pgd)
            {
                __free_page(g_dptdbg_shadow_pgd_page);
                g_dptdbg_shadow_pgd_page = NULL;
                status = -EFAULT;
            }
            else
            {
                __builtin_memcpy(g_dptdbg_shadow_pgd, g_dptdbg_mm->pgd, PGD_SIZE);
                /* 仅为受管代码页建立独立的页表路径。 */
                for (size_t page_index = 0; page_index < ARRAY_SIZE(g_dptdbg_pages) && !status; page_index++)
                {
                    if (!g_dptdbg_pages[page_index].page_vaddr) continue;
                    status = dptdbg_build_shadow_path(&g_dptdbg_pages[page_index]);
                }
                if (!status)
                {
                    dsb(ishst);
                }
                else
                {
                    /* 页表构造失败时，局部创建的页表页和 PGD 立即回收。 */
                    dptdbg_release_table_pages();
                    __free_page(g_dptdbg_shadow_pgd_page);
                    g_dptdbg_shadow_pgd_page = NULL;
                    g_dptdbg_shadow_pgd = NULL;
                }
            }
        }
    }
    if (!status) status = dptdbg_sync_trap_pages();
    /* 此后不再读取受 mmap 锁保护的 VMA/PTE 信息。 */
    mmap_read_unlock(mm);

    if (status)
    {
        /*
         * 统一回滚：release_state() 会先停止 handler、恢复 marker 和线程
         * 页表，再释放 trap page、shadow page table、mm 引用及 inline hook。
         */
        (void)dptdbg_release_state();
        goto out_unlock;
    }

    /* 发布运行态前确认构建期间没有页表失效。 */
    spin_lock_irqsave(&g_dptdbg_lock, flags);
    if (g_dptdbg_stopping) status = -ESTALE;
    else g_dptdbg_info = info;
    spin_unlock_irqrestore(&g_dptdbg_lock, flags);
    if (status)
    {
        (void)dptdbg_release_state();
        goto out_unlock;
    }

    /* 发布后统一切换所有 CPU 的视图。 */
    on_each_cpu(dptdbg_apply_current_thread_view, NULL, 1);
out_unlock:
    /* 所有返回路径都必须释放生命周期锁。 */
    mutex_unlock(&g_dptdbg_lifecycle_lock);
    return status;
}

#endif // ARM64_DPTDBG_H