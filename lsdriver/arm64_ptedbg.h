#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/ptrace.h>

#include "inline_hook_frame.h"
#include "io_struct.h"
#include "lsdriver_log.h"

#ifndef PTEBP_UXN
#define PTEBP_UXN (_AT(pteval_t, 1) << 54)
#endif

#define PTEBP_ESR_EC_IABT_LOW 0x20
#define PTEBP_ESR_FSC_MASK 0x3f
#define PTEBP_ESR_FSC_PERM_L3 0x0f
#define PTEBP_ADDR_MASK 0xFFFFFFFFFFFFULL

#define PTEBP_STATE_INACTIVE 0
#define PTEBP_STATE_ARMED 1
#define PTEBP_STATE_STICKY 2

struct ptebp_slot
{
    pid_t pid;
    pid_t tgid;
    struct mm_struct *mm;
    pte_t *ptep;
    pte_t orig_pte;
    struct bp_point *point;
    uint64_t target_vaddr;
    uint64_t page_vaddr;
    int state;
    pid_t sticky_pid;
    uint64_t sticky_page;
};

static struct break_point *g_ptebp_info;
static struct ptebp_slot g_ptebp_slots[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);
static DEFINE_MUTEX(g_ptebp_mutex);
static struct hook_entry g_ptebp_fault_hook = HOOK_ENTRY("do_mem_abort", NULL);
static struct hook_entry g_ptebp_switch_hook = HOOK_ENTRY("__switch_to", NULL);
static bool g_ptebp_fault_hooked;
static bool g_ptebp_switch_hooked;
static bool g_ptebp_rearm_enabled;

static inline bool ptebp_point_is_active(struct bp_point *point)
{
    return point && point->hit_addr != 0 && (point->bt & BP_BREAKPOINT_X);
}

static inline bool ptebp_info_has_active_point(struct break_point *info)
{
    int point_slot;

    if (!info || info->pid <= 0)
        return false;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (ptebp_point_is_active(&info->points[point_slot]))
            return true;
    }

    return false;
}

static inline bool ptebp_current_task_matches(pid_t target_pid)
{
    return target_pid > 0 && (target_pid == current->tgid || target_pid == current->pid);
}

static inline bool ptebp_addr_matches(struct bp_point *point, uint64_t fault_addr)
{
    if (!point || !point->hit_addr)
        return false;

    return (point->hit_addr & ~0x3ULL) == (fault_addr & ~0x3ULL);
}

static inline void ptebp_dispatch_hit(struct pt_regs *regs, struct bp_point *point)
{
    if (!regs || !point || !point->on_hit)
        return;

    point->on_hit((void *)regs, (void *)point);
}

static struct bp_point *ptebp_find_point_on_page_locked(uint64_t page_vaddr, uint64_t pc)
{
    int point_slot;

    if (!g_ptebp_info)
        return NULL;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct bp_point *point = &g_ptebp_info->points[point_slot];

        if (!ptebp_point_is_active(point) || (point->hit_addr & PAGE_MASK) != page_vaddr)
            continue;
        if (ptebp_addr_matches(point, pc))
            return point;
    }

    return NULL;
}

static bool ptebp_page_already_installed_locked(struct mm_struct *mm, uint64_t page_vaddr)
{
    int point_slot;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct ptebp_slot *slot = &g_ptebp_slots[point_slot];

        if (slot->state != PTEBP_STATE_INACTIVE && slot->mm == mm && slot->page_vaddr == page_vaddr)
            return true;
    }

    return false;
}

static inline void ptebp_flush_page(uint64_t page_vaddr)
{
    dsb(ishst);
    isb();
    flush_user_tlb_addr_all_asid(page_vaddr);
}

static inline void ptebp_write_pte(struct ptebp_slot *slot, pteval_t value)
{
    set_pte(slot->ptep, __pte(value));
    ptebp_flush_page(slot->page_vaddr);
}

static inline void ptebp_set_uxn(struct ptebp_slot *slot, bool armed)
{
    pteval_t value;

    value = pte_val(READ_ONCE(*slot->ptep));
    if (armed)
        value |= PTEBP_UXN;
    else
        value &= ~PTEBP_UXN;

    ptebp_write_pte(slot, value);
}

static inline void ptebp_restore_orig(struct ptebp_slot *slot)
{
    ptebp_write_pte(slot, pte_val(slot->orig_pte));
}

static inline void ptebp_dec_min_flt(void)
{
    if (current->min_flt > 0)
        current->min_flt--;
}

static inline void ptebp_clear_sticky_locked(struct ptebp_slot *slot)
{
    slot->sticky_pid = 0;
    slot->sticky_page = 0;
}

static bool ptebp_validate_slot(struct ptebp_slot *slot)
{
    pte_t *fresh_ptep;
    pte_t pte_now;

    if (!slot || !slot->mm || !slot->ptep || !slot->page_vaddr)
        return false;

    fresh_ptep = get_user_pte(slot->mm, slot->page_vaddr);
    if (!fresh_ptep || fresh_ptep != slot->ptep)
        return false;

    pte_now = READ_ONCE(*fresh_ptep);
    if (pte_none(pte_now) || !pte_present(pte_now))
        return false;

    return true;
}

static struct mm_struct *ptebp_get_live_mm(struct ptebp_slot *slot)
{
    struct pid *pid_ref;
    struct task_struct *task;
    struct mm_struct *mm = NULL;

    if (!slot || !slot->mm || slot->pid <= 0)
        return NULL;

    pid_ref = find_get_pid(slot->pid);
    if (!pid_ref && slot->tgid > 0)
        pid_ref = find_get_pid(slot->tgid);
    if (!pid_ref)
        return NULL;

    task = get_pid_task(pid_ref, PIDTYPE_PID);
    put_pid(pid_ref);
    if (!task)
        return NULL;

    if (!(task->flags & PF_EXITING) && task->tgid == slot->tgid)
    {
        mm = get_task_mm(task);
        if (mm != slot->mm)
        {
            if (mm)
                mmput(mm);
            mm = NULL;
        }
    }

    put_task_struct(task);
    return mm;
}

static struct mm_struct *ptebp_deactivate_locked(struct ptebp_slot *slot)
{
    struct mm_struct *mm = slot->mm;

    memset(slot, 0, sizeof(*slot));
    return mm;
}

static void ptebp_restore_if_live(struct ptebp_slot *slot)
{
    struct mm_struct *live_mm;

    live_mm = ptebp_get_live_mm(slot);
    if (!live_mm)
        return;

    mmap_read_lock(live_mm);
    if (ptebp_validate_slot(slot))
        ptebp_restore_orig(slot);
    mmap_read_unlock(live_mm);
    mmput(live_mm);
}

static bool ptebp_try_rearm_from_sticky_locked(struct ptebp_slot *slot, struct mm_struct **drop_mm)
{
    if (!ptebp_validate_slot(slot))
    {
        if (drop_mm)
            *drop_mm = ptebp_deactivate_locked(slot);
        return false;
    }

    ptebp_set_uxn(slot, true);
    slot->state = PTEBP_STATE_ARMED;
    ptebp_clear_sticky_locked(slot);
    return true;
}

static int work_trampoline_ptebp_switch(struct pt_regs *hook_regs)
{
    struct task_struct *prev;
    struct task_struct *next;
    struct mm_struct *drop_mm = NULL;
    unsigned long flags;
    int point_slot;

    if (!hook_regs)
        return 0;

    prev = (struct task_struct *)hook_regs->regs[0];
    next = (struct task_struct *)hook_regs->regs[1];

    if (!prev || !prev->mm || (prev->flags & PF_EXITING))
        return 0;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (g_ptebp_rearm_enabled)
    {
        for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
        {
            struct ptebp_slot *slot = &g_ptebp_slots[point_slot];

            if (slot->state == PTEBP_STATE_STICKY &&
                slot->mm == prev->mm &&
                slot->tgid == prev->tgid &&
                (!next || next->tgid != slot->tgid))
            {
                ptebp_try_rearm_from_sticky_locked(slot, &drop_mm);
                break;
            }
        }
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (drop_mm)
        mmdrop(drop_mm);

    return 0;
}

static int ptebp_handle_fault(uint64_t far, uint64_t esr, struct pt_regs *regs)
{
    uint64_t pc;
    uint64_t pc_page;
    uint64_t fault_page;
    unsigned int ifsc;
    unsigned long flags;
    int point_slot;
    int handled = 0;
    bool exact_hit = false;
    struct ptebp_slot *slot = NULL;
    struct bp_point *hit_point = NULL;
    struct mm_struct *drop_mm = NULL;

    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING))
        return 0;

    ifsc = esr & PTEBP_ESR_FSC_MASK;
    pc = regs->pc & PTEBP_ADDR_MASK;
    pc_page = pc & PAGE_MASK;
    fault_page = (far & PTEBP_ADDR_MASK) & PAGE_MASK;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || !ptebp_current_task_matches(g_ptebp_info->pid))
        goto out_unlock;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct ptebp_slot *candidate = &g_ptebp_slots[point_slot];

        if (candidate->state != PTEBP_STATE_INACTIVE &&
            candidate->mm == current->mm &&
            (pc_page == candidate->page_vaddr || fault_page == candidate->page_vaddr))
        {
            slot = candidate;
            break;
        }
    }

    if (!slot)
        goto out_unlock;

    if (slot->state == PTEBP_STATE_STICKY)
    {
        if (pc_page == slot->sticky_page || fault_page == slot->sticky_page)
        {
            if (ifsc == PTEBP_ESR_FSC_PERM_L3 && fault_page == slot->sticky_page)
            {
                if (ptebp_validate_slot(slot))
                {
                    ptebp_set_uxn(slot, false);
                    ptebp_dec_min_flt();
                    handled = 1;
                }
                else
                    drop_mm = ptebp_deactivate_locked(slot);
            }
            goto out_unlock;
        }

        if (slot->sticky_pid != current->pid)
            goto out_unlock;

        ptebp_try_rearm_from_sticky_locked(slot, &drop_mm);
        goto out_unlock;
    }

    if (slot->state != PTEBP_STATE_ARMED || ifsc != PTEBP_ESR_FSC_PERM_L3 ||
        pc_page != slot->page_vaddr || fault_page != slot->page_vaddr)
        goto out_unlock;

    if (!ptebp_validate_slot(slot))
    {
        drop_mm = ptebp_deactivate_locked(slot);
        goto out_unlock;
    }

    hit_point = ptebp_find_point_on_page_locked(slot->page_vaddr, pc);
    if (hit_point)
    {
        exact_hit = true;
    }

    if (exact_hit)
        ptebp_dispatch_hit(regs, hit_point);

    // PTEBP 是页粒度。非目标同页 fault 或精确命中后都先放开整页并进入 STICKY，
    // 避免页内前置指令反复触发，导致目标地址永远执行不到。
    ptebp_set_uxn(slot, false);
    slot->state = PTEBP_STATE_STICKY;
    slot->sticky_pid = current->pid;
    slot->sticky_page = slot->page_vaddr;
    ptebp_dec_min_flt();
    handled = 1;

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    if (drop_mm)
        mmdrop(drop_mm);

    return handled;
}

static int work_trampoline_ptebp(struct pt_regs *hook_regs)
{
    uint64_t far;
    uint64_t esr;
    uint64_t ec;
    struct pt_regs *regs;

    if (!hook_regs)
        return 0;

    far = hook_regs->regs[0];
    esr = hook_regs->regs[1];
    regs = (struct pt_regs *)hook_regs->regs[2];

    ec = (esr >> 26) & 0x3f;
    if (ec != PTEBP_ESR_EC_IABT_LOW)
        return 0;

    if (ptebp_handle_fault(far, esr, regs))
    {
        hook_regs->regs[0] = 0;
        return 1;
    }

    return 0;
}

static void ptebp_clear_slots_locked(struct ptebp_slot old_slots[BP_CONFIG_MAX], bool have_old[BP_CONFIG_MAX], struct mm_struct *drop_mms[BP_CONFIG_MAX])
{
    int point_slot;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (g_ptebp_slots[point_slot].state == PTEBP_STATE_INACTIVE)
            continue;

        old_slots[point_slot] = g_ptebp_slots[point_slot];
        have_old[point_slot] = true;
        drop_mms[point_slot] = ptebp_deactivate_locked(&g_ptebp_slots[point_slot]);
    }
}

static inline void stop_ptebp_monitor(void)
{
    struct ptebp_slot old_slots[BP_CONFIG_MAX];
    struct mm_struct *drop_mms[BP_CONFIG_MAX];
    bool have_old[BP_CONFIG_MAX];
    unsigned long flags;
    int point_slot;

    memset(old_slots, 0, sizeof(old_slots));
    memset(drop_mms, 0, sizeof(drop_mms));
    memset(have_old, 0, sizeof(have_old));

    mutex_lock(&g_ptebp_mutex);

    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_info = NULL;
    g_ptebp_rearm_enabled = false;
    ptebp_clear_slots_locked(old_slots, have_old, drop_mms);
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (have_old[point_slot])
            ptebp_restore_if_live(&old_slots[point_slot]);
        if (drop_mms[point_slot])
            mmdrop(drop_mms[point_slot]);
    }

    if (g_ptebp_fault_hooked)
    {
        hook_entry_remove(&g_ptebp_fault_hook);
        synchronize_rcu();
        g_ptebp_fault_hooked = false;
    }

    if (g_ptebp_switch_hooked)
    {
        hook_entry_remove(&g_ptebp_switch_hook);
        synchronize_rcu();
        g_ptebp_switch_hooked = false;
    }

    mutex_unlock(&g_ptebp_mutex);
}

static int ptebp_install_slot(struct break_point *info, int point_slot)
{
    struct bp_point *point = &info->points[point_slot];
    struct ptebp_slot *slot = &g_ptebp_slots[point_slot];
    struct mm_struct *mm;
    pte_t *ptep;
    pte_t orig_pte;
    uint64_t page_vaddr;
    unsigned long flags;
    pid_t tgid;
    struct task_struct *task;
    int status = 0;

    if (!ptebp_point_is_active(point))
        return 0;

    task = get_task_by_pid(info->pid);
    if (!task)
        return -ESRCH;
    tgid = task->tgid;
    mm = get_task_mm(task);
    put_task_struct(task);
    if (!mm)
        return -EINVAL;

    page_vaddr = point->hit_addr & PAGE_MASK;

    mmap_read_lock(mm);
    ptep = get_user_pte(mm, page_vaddr);
    if (!ptep || pte_none(READ_ONCE(*ptep)) || !pte_present(READ_ONCE(*ptep)))
    {
        status = -EFAULT;
        goto out_unlock_mm;
    }

    orig_pte = READ_ONCE(*ptep);

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (ptebp_page_already_installed_locked(mm, page_vaddr))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        goto out_unlock_mm;
    }

    if (pte_val(orig_pte) & PTEBP_UXN)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        status = -EACCES;
        goto out_unlock_mm;
    }

    mmgrab(mm);
    *slot = (struct ptebp_slot){
        .pid = info->pid,
        .tgid = tgid,
        .mm = mm,
        .ptep = ptep,
        .orig_pte = orig_pte,
        .point = point,
        .target_vaddr = point->hit_addr,
        .page_vaddr = page_vaddr,
        .state = PTEBP_STATE_ARMED,
    };
    ptebp_set_uxn(slot, true);
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

out_unlock_mm:
    mmap_read_unlock(mm);
    mmput(mm);
    return status;
}

static inline int start_ptebp_monitor(struct break_point *info)
{
    int status;
    int point_slot;
    unsigned long flags;

    if (!ptebp_info_has_active_point(info))
        return -EINVAL;

    stop_ptebp_monitor();

    mutex_lock(&g_ptebp_mutex);

    g_ptebp_fault_hook.work_fn = work_trampoline_ptebp;
    status = hook_entry_install(&g_ptebp_fault_hook);
    if (status)
        goto out_unlock;
    g_ptebp_fault_hooked = true;

    g_ptebp_switch_hook.work_fn = work_trampoline_ptebp_switch;
    status = hook_entry_install(&g_ptebp_switch_hook);
    if (status)
        ls_log_tag("ptebp", "__switch_to hook install failed status=%d\n", status);
    else
        g_ptebp_switch_hooked = true;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_info = info;
    g_ptebp_rearm_enabled = true;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        status = ptebp_install_slot(info, point_slot);
        if (status)
            goto err_out;
    }

    mutex_unlock(&g_ptebp_mutex);
    return 0;

err_out:
    mutex_unlock(&g_ptebp_mutex);
    stop_ptebp_monitor();
    return status;

out_unlock:
    mutex_unlock(&g_ptebp_mutex);
    return status;
}

#endif // ARM64_PTEDBG_H