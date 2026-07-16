#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/memory.h>
#include <asm/ptrace.h>

#include "inline_hook_frame.h"
#include "io_struct.h"
#include "emulate_insn.h"
#include "lsdriver_log.h"

#ifndef PTEBP_UXN
#define PTEBP_UXN (_AT(pteval_t, 1) << 54)
#endif

#define PTEBP_ESR_EC_IABT_LOW 0x20
#define PTEBP_ESR_FSC_MASK    0x3f
#define PTEBP_ESR_FSC_PERM_L3 0x0f

struct ptebp_page
{
    pte_t orig_pte;
    uint64_t addr;
    bool armed;
};

static struct break_point *g_ptebp_info;
static struct mm_struct *g_ptebp_mm;
static struct ptebp_page g_ptebp_pages[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);
static bool g_ptebp_stopping;

static __always_inline bool ptebp_active(const struct bp_point *point)
{
    return point->hit_addr && point->bt == BP_BREAKPOINT_X;
}

static bool ptebp_page_matches(const struct ptebp_page *page, struct mm_struct *mm, pteval_t flags)
{
    pte_t *ptep;
    pte_t pte_now;
    pteval_t mutable = 0;

    ptep = get_user_pte(mm, page->addr);
    if (!ptep) return false;
    pte_now = READ_ONCE(*ptep);
    if (!pte_present(pte_now) || !pfn_valid(pte_pfn(pte_now))) return false;

#ifdef PTE_AF
    mutable |= PTE_AF;
#endif
#ifdef PTE_DIRTY
    mutable |= PTE_DIRTY;
#endif

    return !((pte_val(pte_now) ^ (pte_val(page->orig_pte) | flags)) & ~mutable);
}

static struct ptebp_page *ptebp_find_page(struct ptebp_page *pages, uint64_t addr)
{
    size_t index;

    addr &= PAGE_MASK;
    for (index = 0; index < BP_CONFIG_MAX; index++)
        if (pages[index].addr && pages[index].addr == addr) return &pages[index];
    return NULL;
}

static void ptebp_drop_all_monitors(bool lock_mm)
{
    struct ptebp_page pages[ARRAY_SIZE(g_ptebp_pages)];
    struct mm_struct *mm;
    unsigned long flags;
    size_t point_slot;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    mm = g_ptebp_mm;
    if (!mm || (g_ptebp_stopping && !lock_mm))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        return;
    }

    g_ptebp_stopping = true;
    __builtin_memcpy(pages, g_ptebp_pages, sizeof(pages));
    if (lock_mm)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        mmap_read_lock(mm);
        spin_lock_irqsave(&g_ptebp_lock, flags);
    }

    for (point_slot = 0; point_slot < ARRAY_SIZE(pages); point_slot++)
    {
        struct ptebp_page *page = &pages[point_slot];
        struct ptebp_page *live = &g_ptebp_pages[point_slot];

        if (!page->addr || !page->armed) continue;
        if (!ptebp_page_matches(page, mm, PTEBP_UXN)) live->armed = false;
        else if (!write_user_pte_value(mm, page->addr, pte_val(page->orig_pte))) live->armed = false;
    }

    g_ptebp_info = NULL;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (lock_mm) mmap_read_unlock(mm);
}

static void ptebp_clear_monitors(void)
{
    struct mm_struct *mm;
    unsigned long flags;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    mm = g_ptebp_mm;
    g_ptebp_info = NULL;
    g_ptebp_mm = NULL;
    __builtin_memset(g_ptebp_pages, 0, sizeof(g_ptebp_pages));
    g_ptebp_stopping = false;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (mm) mmput(mm);
}

static int ptebp_handle_exec_fault(struct pt_regs *hook_regs)
{
    uint64_t pc;
    struct pt_regs *regs;
    struct break_point *info = NULL;
    struct bp_point *hit_point = NULL;
    unsigned long flags;
    size_t point_slot;
    bool managed_page = false;
    bool restored_page = false;
    bool stale_page = false;
    bool stopping = false;

    if (!hook_regs) return 0;

    regs = (struct pt_regs *)hook_regs->regs[2];
    if (((hook_regs->regs[1] >> 26) & 0x3f) != PTEBP_ESR_EC_IABT_LOW || (hook_regs->regs[1] & PTEBP_ESR_FSC_MASK) != PTEBP_ESR_FSC_PERM_L3) return 0;
    if (!regs || !current->mm || !user_mode(regs)) return 0;

    pc = untagged_addr(regs->pc) & ~0x3ULL;
    if ((untagged_addr(hook_regs->regs[0]) & PAGE_MASK) != (pc & PAGE_MASK)) return 0;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (g_ptebp_mm != current->mm) goto out_unlock;
    stopping = g_ptebp_stopping;
    if (!stopping) info = g_ptebp_info;
    {
        struct ptebp_page *page = ptebp_find_page(g_ptebp_pages, pc);

        if (!page) goto out_unlock;
        if (page->armed)
        {
            managed_page = true;
            if (!ptebp_page_matches(page, current->mm, PTEBP_UXN))
            {
                restored_page = stopping && ptebp_page_matches(page, current->mm, 0);
                stale_page = !restored_page;
            }
        }
        else if (stopping && ptebp_page_matches(page, current->mm, 0))
        {
            managed_page = restored_page = true;
        }
        else goto out_unlock;
    }
    if (stopping || restored_page) goto out_unlock;

    for (point_slot = 0; point_slot < ARRAY_SIZE(g_ptebp_pages); point_slot++)
    {
        struct ptebp_page *page = &g_ptebp_pages[point_slot];
        struct bp_point *point = info ? &info->points[point_slot] : NULL;

        if (info && page->addr && (!page->armed || !ptebp_page_matches(page, current->mm, PTEBP_UXN)))
        {
            stale_page = true;
            break;
        }
        if (point && ptebp_active(point) && (untagged_addr(point->hit_addr) & ~0x3ULL) == pc) hit_point = point;
    }

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (stale_page)
    {
        ptebp_drop_all_monitors(false);
        if (!managed_page) return 0;
        goto handled;
    }
    if (!managed_page) return 0;
    if (stopping || restored_page) goto handled;

    if (hit_point && hit_point->on_hit) hit_point->on_hit((void *)regs, (void *)hit_point);
    if (!emulate_insn(regs, NULL, NULL)) ptebp_drop_all_monitors(false);

handled:
    hook_regs->regs[0] = 0;
    return 1;
}

static struct hook_entry g_ptebp_fault_hooks[] = {
    HOOK_ENTRY("do_mem_abort", ptebp_handle_exec_fault),
};

static inline void stop_ptebp_monitor(void)
{
    ptebp_drop_all_monitors(true);
    inline_hook_remove(g_ptebp_fault_hooks);
    synchronize_rcu();
    ptebp_clear_monitors();
}

static int ptebp_install_page(struct break_point *info, size_t point_slot, struct mm_struct *mm)
{
    struct bp_point *point;
    struct ptebp_page *page;
    pte_t *ptep;
    pte_t orig_pte;
    uint64_t page_vaddr;
    uint64_t hook_addr;
    size_t scan_slot;
    int status;

    point = &info->points[point_slot];
    page = &g_ptebp_pages[point_slot];
    hook_addr = untagged_addr(point->hit_addr) & ~0x3ULL;
    if (!hook_addr || hook_addr >= READ_ONCE(mm->task_size) || sizeof(uint32_t) > READ_ONCE(mm->task_size) - hook_addr)
    {
        ls_log_tag("ptebp", "install page rejected pid=%d slot=%zu addr=0x%llx task_size=0x%llx status=%d\n", info->pid, point_slot, (unsigned long long)hook_addr, (unsigned long long)READ_ONCE(mm->task_size), -EFAULT);
        return -EFAULT;
    }
    page_vaddr = hook_addr & PAGE_MASK;
    ls_log_tag("ptebp", "install page begin pid=%d slot=%zu addr=0x%llx page=0x%llx\n", info->pid, point_slot, (unsigned long long)hook_addr, (unsigned long long)page_vaddr);

    for (scan_slot = 0; scan_slot < point_slot; scan_slot++)
    {
        struct bp_point *candidate = &info->points[scan_slot];

        if (ptebp_active(candidate) && (untagged_addr(candidate->hit_addr) & ~0x3ULL) == hook_addr)
        {
            ls_log_tag("ptebp", "install page duplicate pid=%d slot=%zu previous_slot=%zu addr=0x%llx status=%d\n", info->pid, point_slot, scan_slot, (unsigned long long)hook_addr, -EEXIST);
            return -EEXIST;
        }
    }

    page = ptebp_find_page(g_ptebp_pages, page_vaddr);
    if (page)
    {
        status = ptebp_page_matches(page, mm, PTEBP_UXN) ? 0 : -EFAULT;
        ls_log_tag("ptebp", "install page reused pid=%d slot=%zu page=0x%llx armed=%d status=%d\n", info->pid, point_slot, (unsigned long long)page_vaddr, page->armed, status);
        return status;
    }

    ptep = get_user_pte(mm, page_vaddr);
    if (!ptep)
    {
        ls_log_tag("ptebp", "install page no pte pid=%d slot=%zu page=0x%llx status=%d\n", info->pid, point_slot, (unsigned long long)page_vaddr, -EFAULT);
        return -EFAULT;
    }

    orig_pte = READ_ONCE(*ptep);
    ls_log_tag("ptebp", "install page pte pid=%d slot=%zu page=0x%llx ptep=0x%llx orig=0x%llx present=%d pfn_valid=%d uxn=%d\n", info->pid, point_slot, (unsigned long long)page_vaddr, (unsigned long long)ptep, (unsigned long long)pte_val(orig_pte), pte_present(orig_pte), pfn_valid(pte_pfn(orig_pte)), !!(pte_val(orig_pte) & PTEBP_UXN));
    if (!pte_present(orig_pte) || !pfn_valid(pte_pfn(orig_pte)))
    {
        ls_log_tag("ptebp", "install page invalid pte pid=%d slot=%zu page=0x%llx status=%d\n", info->pid, point_slot, (unsigned long long)page_vaddr, -EFAULT);
        return -EFAULT;
    }
    if (pte_val(orig_pte) & PTEBP_UXN)
    {
        ls_log_tag("ptebp", "install page already uxn pid=%d slot=%zu page=0x%llx status=%d\n", info->pid, point_slot, (unsigned long long)page_vaddr, -EACCES);
        return -EACCES;
    }

    status = write_user_pte_value(mm, page_vaddr, pte_val(orig_pte) | PTEBP_UXN);
    ls_log_tag("ptebp", "install page write pid=%d slot=%zu page=0x%llx requested=0x%llx readback=0x%llx status=%d\n", info->pid, point_slot, (unsigned long long)page_vaddr, (unsigned long long)(pte_val(orig_pte) | PTEBP_UXN), (unsigned long long)pte_val(READ_ONCE(*ptep)), status);
    if (status) return status;
    page = &g_ptebp_pages[point_slot];
    *page = (struct ptebp_page){.orig_pte = orig_pte, .addr = page_vaddr, .armed = true};
    ls_log_tag("ptebp", "install page ok pid=%d slot=%zu addr=0x%llx page=0x%llx\n", info->pid, point_slot, (unsigned long long)hook_addr, (unsigned long long)page_vaddr);
    return 0;
}

static inline int start_ptebp_monitor(struct break_point *info)
{
    int status;
    size_t point_slot;
    struct mm_struct *mm;
    unsigned long flags;

    if (!info || info->pid <= 0)
    {
        ls_log_tag("ptebp", "start rejected info=0x%llx pid=%d status=%d\n", (unsigned long long)info, info ? info->pid : -1, -EINVAL);
        return -EINVAL;
    }

    ls_log_tag("ptebp", "start begin pid=%d\n", info->pid);

    for (point_slot = 0; point_slot < ARRAY_SIZE(info->points); point_slot++)
        if (ptebp_active(&info->points[point_slot])) break;
    if (point_slot == ARRAY_SIZE(info->points))
    {
        ls_log_tag("ptebp", "start rejected pid=%d no active execute point status=%d\n", info->pid, -EINVAL);
        return -EINVAL;
    }

    ls_log_tag("ptebp", "start stopping previous monitor pid=%d first_slot=%zu first_addr=0x%llx\n", info->pid, point_slot, (unsigned long long)info->points[point_slot].hit_addr);
    stop_ptebp_monitor();

    mm = get_mm_by_pid(info->pid);
    if (!mm)
    {
        ls_log_tag("ptebp", "start get mm failed pid=%d status=%d\n", info->pid, -EINVAL);
        return -EINVAL;
    }

    status = inline_hook_install(g_ptebp_fault_hooks);
    if (status)
    {
        ls_log_tag("ptebp", "start hook install failed pid=%d status=%d\n", info->pid, status);
        goto err_put_mm;
    }
    ls_log_tag("ptebp", "start hook installed pid=%d target=0x%llx\n", info->pid, (unsigned long long)g_ptebp_fault_hooks[0].target_addr);

    mmap_read_lock(mm);
    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_mm = mm;
    for (point_slot = 0; point_slot < ARRAY_SIZE(info->points); point_slot++)
    {
        if (!ptebp_active(&info->points[point_slot])) continue;
        status = ptebp_install_page(info, point_slot, mm);
        if (status) break;
    }
    if (!status) g_ptebp_info = info;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    mmap_read_unlock(mm);

    if (!status)
    {
        ls_log_tag("ptebp", "start ok pid=%d mm=0x%llx\n", info->pid, (unsigned long long)mm);
        return 0;
    }

    ls_log_tag("ptebp", "start page install failed pid=%d slot=%zu status=%d, cleaning up\n", info->pid, point_slot, status);
    stop_ptebp_monitor();
    return status;

err_put_mm:
    mmput(mm);
    return status;
}

#endif // ARM64_PTEDBG_H