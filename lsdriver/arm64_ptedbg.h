#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <asm/memory.h>
#include <asm/ptrace.h>

#include "inline_hook_frame.h"
#include "io_struct.h"
#include "lsdriver_log.h"
#include "emulate_insn.h"
#include "virtual_memory_rw.h"

#ifndef PTEBP_UXN
#define PTEBP_UXN (_AT(pteval_t, 1) << 54)
#endif
#ifndef PTEBP_PXN
#define PTEBP_PXN (_AT(pteval_t, 1) << 53)
#endif
#ifndef PTEBP_BRK_INSN
#define PTEBP_BRK_INSN 0xD4200000U
#endif

#define PTEBP_ESR_EC_DABT_LOW 0x24
#define PTEBP_ESR_FSC_MASK 0x3f
#define PTEBP_ESR_FSC_PERM_L3 0x0f

struct ptebp_slot
{
    pid_t pid;
    pid_t tgid;
    struct mm_struct *mm;
    pte_t *ptep;
    pte_t orig_pte;
    uint64_t page_vaddr;
    uint64_t hook_addr;
    uint32_t orig_insn;
    bool patched;
};

static struct break_point *g_ptebp_info;
static struct ptebp_slot g_ptebp_slots[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);

static inline bool ptebp_point_is_active(struct bp_point *point)
{
    return point && point->hit_addr != 0 && (point->bt & BP_BREAKPOINT_X);
}

static inline bool ptebp_slot_active(struct ptebp_slot *slot)
{
    return slot && slot->mm;
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

static bool ptebp_page_already_installed_locked(struct mm_struct *mm, uint64_t page_vaddr)
{
    int point_slot;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct ptebp_slot *slot = &g_ptebp_slots[point_slot];

        if (ptebp_slot_active(slot) && slot->mm == mm && slot->page_vaddr == page_vaddr)
            return true;
    }

    return false;
}

static inline pteval_t ptebp_make_execute_only_pte(pteval_t value)
{
    value &= ~PTEBP_UXN;
    value |= PTEBP_PXN;
#ifdef PTE_USER
    value &= ~PTE_USER;
#endif
#ifdef PTE_WRITE
    value &= ~PTE_WRITE;
#endif
#ifdef PTE_RDONLY
    value |= PTE_RDONLY;
#endif
#ifdef PTE_DBM
    value &= ~PTE_DBM;
#endif

    return value;
}

static inline int ptebp_patch_brk(struct ptebp_slot *slot)
{
    int status;
    uint32_t insn = PTEBP_BRK_INSN;

    status = virtual_memory_rw(request_op_vmem_write, slot->pid, slot->hook_addr, &insn, sizeof(insn));
    if (status == sizeof(insn))
    {
        slot->patched = true;
        return 0;
    }
    return status < 0 ? status : -EFAULT;
}

static inline void ptebp_restore_patch(struct ptebp_slot *slot)
{
    if (slot && slot->patched)
    {
        if (virtual_memory_rw(request_op_vmem_write, slot->pid, slot->hook_addr, &slot->orig_insn, sizeof(slot->orig_insn)) == sizeof(slot->orig_insn))
            slot->patched = false;
    }
}

static inline int ptebp_restore_orig(struct ptebp_slot *slot)
{
    ptebp_restore_patch(slot);
    return write_user_pte_value(slot->mm, slot->page_vaddr, pte_val(slot->orig_pte));
}

static inline void ptebp_dec_min_flt(void)
{
    if (current->min_flt > 0)
        current->min_flt--;
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
    struct task_struct *task;
    struct mm_struct *mm = NULL;

    if (!slot || !slot->mm || slot->pid <= 0)
        return NULL;

    task = get_task_by_pid(slot->pid);
    if (!task && slot->tgid > 0 && slot->tgid != slot->pid)
        task = get_task_by_pid(slot->tgid);
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

static bool ptebp_try_return_orig_insn(struct pt_regs *regs, struct ptebp_slot *slot, uint64_t far)
{
    uint64_t addr = untagged_addr(far);
    uint64_t hook_addr = untagged_addr(slot->hook_addr) & ~0x3ULL;
    uint32_t insn;
    uint32_t size;
    uint32_t opc;
    uint32_t rt;
    int bytes;
    uint64_t shift;
    uint64_t value;
    uint64_t sign;

    if (addr < hook_addr || addr >= hook_addr + sizeof(slot->orig_insn))
        return false;

    if (__get_user(insn, (uint32_t __user *)regs->pc))
        return false;

    if ((insn & 0x3A000000) != 0x38000000 || (insn & 0x04000000))
        return false;

    size = (insn >> 30) & 0x3;
    opc = (insn >> 22) & 0x3;
    rt = insn & 0x1F;
    bytes = 1 << size;

    if (!opc || size == 3 || (size == 2 && opc == 3))
        return false;
    if ((addr + bytes) > (hook_addr + sizeof(slot->orig_insn)))
        return false;

    shift = (addr - hook_addr) * 8;
    value = slot->orig_insn >> shift;
    if (bytes != 4)
        value &= (1ULL << (bytes * 8)) - 1;
    if (opc >= 2)
    {
        sign = 1ULL << (bytes * 8 - 1);
        value = (value ^ sign) - sign;
    }

    if (rt != 31)
        regs->regs[rt] = (opc == 2) ? value : (uint32_t)value;
    regs->pc += 4;
    return true;
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

static void ptebp_drop_all_monitors(void)
{
    struct mm_struct *drop_mms[BP_CONFIG_MAX];
    struct ptebp_slot *slot;
    unsigned long flags;
    int point_slot;

    memset(drop_mms, 0, sizeof(drop_mms));

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (g_ptebp_info)
    {
        memset(g_ptebp_info, 0, sizeof(*g_ptebp_info));
        g_ptebp_info = NULL;
    }

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        slot = &g_ptebp_slots[point_slot];
        if (!ptebp_slot_active(slot))
            continue;

        if (ptebp_validate_slot(slot))
            ptebp_restore_orig(slot);
        drop_mms[point_slot] = ptebp_deactivate_locked(slot);
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (drop_mms[point_slot])
            mmput(drop_mms[point_slot]);
    }
}

static int ptebp_handle_brk(struct pt_regs *hook_regs)
{
    struct pt_regs *regs;
    uint64_t pc;
    unsigned long flags;
    int point_slot;
    uint32_t emulate_insn_word = 0;
    bool hit_ours = false;
    struct bp_point *hit_point = NULL;

    if (!hook_regs)
        return 0;

    regs = (struct pt_regs *)hook_regs->regs[2];
    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING))
        return 0;

    pc = untagged_addr(regs->pc) & ~0x3ULL;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || !ptebp_current_task_matches(g_ptebp_info->pid))
        goto out_unlock;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct bp_point *point = &g_ptebp_info->points[point_slot];
        struct ptebp_slot *slot = &g_ptebp_slots[point_slot];

        if (!ptebp_point_is_active(point) || (untagged_addr(point->hit_addr) & ~0x3ULL) != pc)
            continue;
        if (!ptebp_slot_active(slot))
            break;

        hit_point = point;
        hit_ours = true;
        emulate_insn_word = slot->orig_insn;
        break;
    }

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (!hit_ours)
        return 0;

    if (hit_point && hit_point->on_hit)
        hit_point->on_hit((void *)regs, (void *)hit_point);

    if (emulate_insn(regs, &emulate_insn_word))
    {
        hook_regs->regs[0] = 0;
        return 1;
    }

    ptebp_drop_all_monitors();
    hook_regs->regs[0] = 0;
    return 1;
}

static int ptebp_handle_fault(struct pt_regs *hook_regs)
{
    uint64_t far;
    uint64_t esr;
    uint64_t ec;
    struct pt_regs *regs;
    uint64_t fault_page;
    unsigned int fsc;
    unsigned long flags;
    int point_slot;
    struct ptebp_slot *slot = NULL;
    struct mm_struct *drop_mm = NULL;

    if (!hook_regs)
        return 0;

    far = hook_regs->regs[0];
    esr = hook_regs->regs[1];
    regs = (struct pt_regs *)hook_regs->regs[2];
    ec = (esr >> 26) & 0x3f;
    if (ec != PTEBP_ESR_EC_DABT_LOW)
        return 0;

    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING))
        return 0;

    fsc = esr & PTEBP_ESR_FSC_MASK;
    fault_page = untagged_addr(far) & PAGE_MASK;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || !ptebp_current_task_matches(g_ptebp_info->pid))
        goto out_unlock;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct ptebp_slot *candidate = &g_ptebp_slots[point_slot];

        if (ptebp_slot_active(candidate) &&
            candidate->mm == current->mm &&
            fault_page == candidate->page_vaddr)
        {
            slot = candidate;
            break;
        }
    }

    if (!slot)
        goto out_unlock;

    if (fsc != PTEBP_ESR_FSC_PERM_L3)
    {
        slot = NULL;
        goto out_unlock;
    }

    if (!ptebp_validate_slot(slot))
    {
        drop_mm = ptebp_deactivate_locked(slot);
        slot = NULL;
        goto out_unlock;
    }

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    if (drop_mm)
        mmput(drop_mm);

    if (!slot)
        return 0;

    if (ptebp_try_return_orig_insn(regs, slot, far))
    {
        ptebp_dec_min_flt();
        hook_regs->regs[0] = 0;
        return 1;
    }

    if (emulate_insn(regs, NULL))
    {
        ptebp_dec_min_flt();
        hook_regs->regs[0] = 0;
        return 1;
    }

    ptebp_drop_all_monitors();
    ptebp_dec_min_flt();
    hook_regs->regs[0] = 0;
    return 1;
}

static struct hook_entry g_ptebp_fault_hook = HOOK_ENTRY("do_mem_abort", ptebp_handle_fault);
static struct hook_entry g_ptebp_brk_hook = HOOK_ENTRY("brk_handler", ptebp_handle_brk);

static void ptebp_clear_slots_locked(struct ptebp_slot old_slots[BP_CONFIG_MAX], bool have_old[BP_CONFIG_MAX], struct mm_struct *drop_mms[BP_CONFIG_MAX])
{
    int point_slot;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (!ptebp_slot_active(&g_ptebp_slots[point_slot]))
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

    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_info = NULL;
    ptebp_clear_slots_locked(old_slots, have_old, drop_mms);
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (have_old[point_slot])
            ptebp_restore_if_live(&old_slots[point_slot]);
        if (drop_mms[point_slot])
            mmput(drop_mms[point_slot]);
    }

    hook_entry_remove(&g_ptebp_fault_hook);
    hook_entry_remove(&g_ptebp_brk_hook);
    synchronize_rcu();
}

static int ptebp_install_slot(struct break_point *info, int point_slot)
{
    struct bp_point *point = &info->points[point_slot];
    struct ptebp_slot *slot = &g_ptebp_slots[point_slot];
    struct mm_struct *mm;
    pte_t *ptep;
    pte_t orig_pte;
    uint64_t page_vaddr;
    uint64_t hook_addr;
    uint32_t orig_insn;
    unsigned long flags;
    pid_t tgid;
    struct task_struct *task;
    bool slot_owns_mm = false;
    int status = 0;

    if (!ptebp_point_is_active(point))
        return 0;

    task = get_task_by_pid(info->pid);
    if (!task)
        return -ESRCH;
    tgid = task->tgid;
    put_task_struct(task);

    mm = get_mm_by_pid(info->pid);
    if (!mm)
        return -EINVAL;

    hook_addr = untagged_addr(point->hit_addr) & ~0x3ULL;
    page_vaddr = hook_addr & PAGE_MASK;

    mmap_read_lock(mm);
    status = virtual_memory_rw(request_op_vmem_read, info->pid, hook_addr, &orig_insn, sizeof(orig_insn));
    if (status != sizeof(orig_insn))
    {
        status = status < 0 ? status : -EFAULT;
        goto out_unlock_mm;
    }
    status = 0;

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

    *slot = (struct ptebp_slot){
        .pid = info->pid,
        .tgid = tgid,
        .mm = mm,
        .ptep = ptep,
        .orig_pte = orig_pte,
        .page_vaddr = page_vaddr,
        .hook_addr = hook_addr,
        .orig_insn = orig_insn,
    };
    status = ptebp_patch_brk(slot);
    if (!status)
        status = write_user_pte_value(slot->mm, slot->page_vaddr, ptebp_make_execute_only_pte(pte_val(READ_ONCE(*slot->ptep))));

    if (!status)
    {
        slot_owns_mm = true;
    }
    else
    {
        ptebp_restore_patch(slot);
        ptebp_deactivate_locked(slot);
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

out_unlock_mm:
    mmap_read_unlock(mm);
    if (!slot_owns_mm)
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

    status = hook_entry_install(&g_ptebp_fault_hook);
    if (status)
        return status;

    status = hook_entry_install(&g_ptebp_brk_hook);
    if (status)
        goto err_remove_fault_hook;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_info = info;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        status = ptebp_install_slot(info, point_slot);
        if (status)
            goto err_out;
    }

    return 0;

err_out:
    stop_ptebp_monitor();
    return status;

err_remove_fault_hook:
    hook_entry_remove(&g_ptebp_fault_hook);
    return status;
}

#endif // ARM64_PTEDBG_H