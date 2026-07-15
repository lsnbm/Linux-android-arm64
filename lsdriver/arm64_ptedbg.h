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
#define PTEBP_ESR_FSC_MASK    0x3f
#define PTEBP_ESR_FSC_PERM_L3 0x0f

struct ptebp_slot
{
    pte_t orig_pte;
    uint64_t hook_addr;
    uint32_t orig_insn;
};

static struct break_point *g_ptebp_info;
static struct mm_struct *g_ptebp_mm;
static struct ptebp_slot g_ptebp_slots[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);
static bool g_ptebp_stopping;

static inline bool ptebp_point_is_active(struct bp_point *point)
{
    return point && point->hit_addr != 0 && point->bt == BP_BREAKPOINT_X;
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

static inline void ptebp_sync_insn(phys_addr_t paddr)
{
    unsigned long addr = (unsigned long)phys_to_virt(paddr);

    asm volatile("dc cvau, %0\n\t"
                 "dsb ish\n\t"
                 "ic ialluis\n\t"
                 "dsb ish\n\t"
                 "isb\n\t"
                 :
                 : "r"(addr)
                 : "memory");
}

/*
 * PTEBP 页可能已经是 execute-only，不能依赖 EL0 数据访问权限翻译。
 * 指令读写统一通过当前 PTE 的 PFN 完成，写入后回读校验。
 */
static inline int ptebp_access_insn(struct ptebp_slot *slot, uint32_t *insn, bool write)
{
    unsigned long pfn;
    phys_addr_t paddr;
    uint32_t readback = 0;
    int status;

    if (!pte_present(slot->orig_pte)) return -ESTALE;

    pfn = pte_pfn(slot->orig_pte);
    if (!pfn_valid(pfn)) return -EFAULT;

    paddr = PFN_PHYS(pfn) + offset_in_page(slot->hook_addr);
    if (!write) return pte_read_physical(paddr, insn, sizeof(*insn));

    status = linear_write_physical(paddr, insn, sizeof(*insn));
    if (status) return status;
    ptebp_sync_insn(paddr);

    status = linear_read_physical(paddr, &readback, sizeof(readback));
    if (status) return status;
    return readback == *insn ? 0 : -EIO;
}

static bool ptebp_validate_slot(struct ptebp_slot *slot, struct mm_struct *mm)
{
    pte_t *fresh_ptep;
    pte_t pte_now;
    pte_t expected_pte;
    pteval_t changed;
    pteval_t mutable = 0;
    uint64_t page_vaddr;

    if (!slot || !mm || !slot->hook_addr) return false;

    page_vaddr = slot->hook_addr & PAGE_MASK;
    fresh_ptep = get_user_pte(mm, page_vaddr);
    if (!fresh_ptep) return false;

    pte_now = READ_ONCE(*fresh_ptep);
    if (!pte_present(pte_now) || !pfn_valid(pte_pfn(pte_now))) return false;

    expected_pte = __pte(ptebp_make_execute_only_pte(pte_val(slot->orig_pte)));
    changed = pte_val(pte_now) ^ pte_val(expected_pte);
#ifdef PTE_AF
    mutable |= PTE_AF;
#endif
#ifdef PTE_DIRTY
    mutable |= PTE_DIRTY;
#endif

    return !(changed & ~mutable);
}

struct ptebp_emu_mem_ctx
{
    struct ptebp_slot slots[BP_CONFIG_MAX];
};

static struct ptebp_slot *ptebp_emu_find_page(struct ptebp_emu_mem_ctx *ctx, uint64_t page_vaddr)
{
    size_t slot_index;

    for (slot_index = 0; slot_index < ARRAY_SIZE(ctx->slots); slot_index++)
    {
        struct ptebp_slot *slot = &ctx->slots[slot_index];

        if (slot->hook_addr && (slot->hook_addr & PAGE_MASK) == page_vaddr) return slot;
    }
    return NULL;
}

static bool ptebp_emu_range_is_managed(struct ptebp_emu_mem_ctx *ctx, uint64_t addr, int bytes)
{
    uint64_t end = addr + (uint64_t)bytes;

    return ptebp_emu_find_page(ctx, addr & PAGE_MASK) || ptebp_emu_find_page(ctx, (end - 1) & PAGE_MASK);
}

/*
 * PTEBP 把目标页改成 execute-only 后，__get_user 会按 EL0 权限再次触发
 * Data Abort。这里按受监控页的 PFN 分段读写，并在读取范围覆盖补丁位置时
 * 还原 orig_insn，供跨页 LDP/LDR/STR 和原子普通化等模拟路径使用。
 */
static int ptebp_emu_read_mem(void *opaque, uint64_t addr, int bytes, __uint128_t *out)
{
    struct ptebp_emu_mem_ctx *ctx = opaque;
    uint8_t data[sizeof(__uint128_t)];
    uint64_t end;
    uint64_t cursor;
    __uint128_t value = 0;
    size_t slot_index;

    if (!ctx || !out || bytes <= 0 || bytes > sizeof(data)) return -EINVAL;
    addr = untagged_addr(addr);
    if (addr > U64_MAX - (uint64_t)bytes) return -EFAULT;

    end = addr + (uint64_t)bytes;
    if (!ptebp_emu_range_is_managed(ctx, addr, bytes)) return -EOPNOTSUPP;

    for (cursor = addr; cursor < end;)
    {
        uint64_t page_vaddr = cursor & PAGE_MASK;
        size_t data_offset = cursor - addr;
        size_t copy_size = min_t(size_t, end - cursor, PAGE_SIZE - offset_in_page(cursor));
        struct ptebp_slot *owner = ptebp_emu_find_page(ctx, page_vaddr);

        if (owner)
        {
            unsigned long pfn = pte_pfn(owner->orig_pte);
            phys_addr_t paddr;
            int status;

            if (!pte_present(owner->orig_pte) || !pfn_valid(pfn)) return -ESTALE;
            paddr = PFN_PHYS(pfn) + offset_in_page(cursor);
            status = linear_read_physical(paddr, data + data_offset, copy_size);
            if (status) return status;
        }
        else if (__copy_from_user(data + data_offset, (void __user *)(uintptr_t)cursor, copy_size))
        {
            return -EFAULT;
        }
        cursor += copy_size;
    }

    for (slot_index = 0; slot_index < ARRAY_SIZE(ctx->slots); slot_index++)
    {
        const struct ptebp_slot *slot = &ctx->slots[slot_index];
        uint64_t start = max(addr, slot->hook_addr);
        uint64_t stop = min(end, slot->hook_addr + sizeof(slot->orig_insn));
        size_t data_offset;
        size_t insn_offset;
        size_t copy_size;

        if (!slot->hook_addr) continue;
        if (start >= stop) continue;

        data_offset = start - addr;
        insn_offset = start - slot->hook_addr;
        copy_size = stop - start;
        if (data_offset > (size_t)bytes || copy_size > (size_t)bytes - data_offset || insn_offset > sizeof(slot->orig_insn) || copy_size > sizeof(slot->orig_insn) - insn_offset) return -EFAULT;

        __builtin_memcpy(data + data_offset, (uint8_t *)&slot->orig_insn + insn_offset, copy_size);
    }

    __builtin_memcpy(&value, data, bytes);
    *out = value;
    return 0;
}

static int ptebp_emu_write_mem(void *opaque, uint64_t addr, int bytes, __uint128_t value)
{
    struct ptebp_emu_mem_ctx *ctx = opaque;
    uint8_t data[sizeof(value)];
    uint64_t end;
    uint64_t cursor;
    unsigned long flags;
    size_t slot_index;

    if (!ctx || bytes <= 0 || bytes > sizeof(data)) return -EINVAL;
    addr = untagged_addr(addr);
    if (addr > U64_MAX - (uint64_t)bytes) return -EFAULT;

    end = addr + (uint64_t)bytes;
    if (!ptebp_emu_range_is_managed(ctx, addr, bytes)) return -EOPNOTSUPP;
    __builtin_memcpy(data, &value, bytes);

    for (cursor = addr; cursor < end;)
    {
        uint64_t page_vaddr = cursor & PAGE_MASK;
        size_t data_offset = cursor - addr;
        size_t copy_size = min_t(size_t, end - cursor, PAGE_SIZE - offset_in_page(cursor));
        struct ptebp_slot *owner = ptebp_emu_find_page(ctx, page_vaddr);

        if (!owner && __copy_to_user((void __user *)(uintptr_t)cursor, data + data_offset, copy_size)) return -EFAULT;
        cursor += copy_size;
    }

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || g_ptebp_mm != current->mm)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        return -ESTALE;
    }

    for (cursor = addr; cursor < end;)
    {
        uint64_t page_vaddr = cursor & PAGE_MASK;
        size_t data_offset = cursor - addr;
        size_t copy_size = min_t(size_t, end - cursor, PAGE_SIZE - offset_in_page(cursor));
        struct ptebp_slot *owner = ptebp_emu_find_page(ctx, page_vaddr);

        if (owner)
        {
            size_t owner_index = owner - ctx->slots;
            struct ptebp_slot *slot = &g_ptebp_slots[owner_index];
            unsigned long pfn = pte_pfn(owner->orig_pte);
            phys_addr_t paddr;
            int status;

            if (slot->hook_addr != owner->hook_addr || !ptebp_validate_slot(slot, current->mm) || !pte_write(owner->orig_pte))
            {
                spin_unlock_irqrestore(&g_ptebp_lock, flags);
                return -EFAULT;
            }
            if (!pfn_valid(pfn))
            {
                spin_unlock_irqrestore(&g_ptebp_lock, flags);
                return -ESTALE;
            }

            paddr = PFN_PHYS(pfn) + offset_in_page(cursor);
            status = linear_write_physical(paddr, data + data_offset, copy_size);
            if (status)
            {
                spin_unlock_irqrestore(&g_ptebp_lock, flags);
                return status;
            }
            ptebp_sync_insn(paddr);
            if (copy_size > 1) ptebp_sync_insn(paddr + copy_size - 1);
        }
        cursor += copy_size;
    }

    for (slot_index = 0; slot_index < ARRAY_SIZE(ctx->slots); slot_index++)
    {
        struct ptebp_slot *shadow = &ctx->slots[slot_index];
        struct ptebp_slot *slot = &g_ptebp_slots[slot_index];
        uint64_t start;
        uint64_t stop;
        size_t data_offset;
        size_t insn_offset;
        size_t copy_size;
        uint32_t brk_insn = PTEBP_BRK_INSN;
        phys_addr_t paddr;
        int status;

        if (!shadow->hook_addr) continue;
        start = max(addr, shadow->hook_addr);
        stop = min(end, shadow->hook_addr + sizeof(shadow->orig_insn));
        if (start >= stop) continue;
        if (slot->hook_addr != shadow->hook_addr)
        {
            spin_unlock_irqrestore(&g_ptebp_lock, flags);
            return -ESTALE;
        }

        data_offset = start - addr;
        insn_offset = start - shadow->hook_addr;
        copy_size = stop - start;
        if (data_offset > (size_t)bytes || copy_size > (size_t)bytes - data_offset || insn_offset > sizeof(shadow->orig_insn) || copy_size > sizeof(shadow->orig_insn) - insn_offset)
        {
            spin_unlock_irqrestore(&g_ptebp_lock, flags);
            return -EFAULT;
        }
        __builtin_memcpy((uint8_t *)&shadow->orig_insn + insn_offset, data + data_offset, copy_size);
        __builtin_memcpy((uint8_t *)&slot->orig_insn + insn_offset, data + data_offset, copy_size);

        paddr = PFN_PHYS(pte_pfn(shadow->orig_pte)) + offset_in_page(shadow->hook_addr);
        status = linear_write_physical(paddr, &brk_insn, sizeof(brk_insn));
        if (status)
        {
            spin_unlock_irqrestore(&g_ptebp_lock, flags);
            return status;
        }
        ptebp_sync_insn(paddr);
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    return 0;
}

static void ptebp_drop_all_monitors(bool lock_mm)
{
    struct ptebp_slot slots[ARRAY_SIZE(g_ptebp_slots)];
    struct mm_struct *mm;
    unsigned long flags;
    size_t point_slot;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (g_ptebp_stopping)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        return;
    }

    mm = g_ptebp_mm;
    if (!mm)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        return;
    }

    g_ptebp_stopping = true;
    memcpy(slots, g_ptebp_slots, sizeof(slots));
    if (lock_mm)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        mmap_read_lock(mm);
        spin_lock_irqsave(&g_ptebp_lock, flags);
    }

    for (point_slot = 0; point_slot < ARRAY_SIZE(slots); point_slot++)
    {
        struct ptebp_slot *slot = &slots[point_slot];

        if (!slot->hook_addr) continue;
        if (ptebp_validate_slot(slot, mm)) (void)ptebp_access_insn(slot, &slot->orig_insn, true);
    }
    for (point_slot = 0; point_slot < ARRAY_SIZE(slots); point_slot++)
    {
        struct ptebp_slot *slot = &slots[point_slot];

        if (!slot->hook_addr) continue;
        if (ptebp_validate_slot(slot, mm)) (void)write_user_pte_value(mm, slot->hook_addr & PAGE_MASK, pte_val(slot->orig_pte));
    }

    g_ptebp_info = NULL;
    g_ptebp_mm = NULL;
    memset(g_ptebp_slots, 0, sizeof(g_ptebp_slots));
    g_ptebp_stopping = false;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (lock_mm) mmap_read_unlock(mm);
    mmput(mm);
}

static int ptebp_handle_brk(struct pt_regs *hook_regs)
{
    struct pt_regs *regs;
    uint64_t pc;
    unsigned long flags;
    size_t point_slot;
    uint32_t emulate_insn_word;
    struct bp_point *hit_point = NULL;

    if (!hook_regs) return 0;

    regs = (struct pt_regs *)hook_regs->regs[2];
    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING)) return 0;

    pc = untagged_addr(regs->pc) & ~0x3ULL;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || g_ptebp_mm != current->mm) goto out_unlock;

    for (point_slot = 0; point_slot < ARRAY_SIZE(g_ptebp_slots); point_slot++)
    {
        struct ptebp_slot *slot = &g_ptebp_slots[point_slot];

        if (!slot->hook_addr || slot->hook_addr != pc) continue;

        hit_point = &g_ptebp_info->points[point_slot];
        emulate_insn_word = slot->orig_insn;
        break;
    }

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (!hit_point) return 0;

    if (hit_point->on_hit) hit_point->on_hit((void *)regs, (void *)hit_point);

    if (!emulate_insn(regs, &emulate_insn_word, NULL)) ptebp_drop_all_monitors(false);
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
    size_t point_slot;
    struct ptebp_slot *owner = NULL;
    struct ptebp_emu_mem_ctx mem_ctx;
    bool stale_slot = false;
    struct emu_mem_access mem_access = {
        .read = ptebp_emu_read_mem,
        .write = ptebp_emu_write_mem,
        .ctx = &mem_ctx,
    };

    if (!hook_regs) return 0;

    // do_mem_abort(far, esr, regs)：far 是数据访问地址，regs->pc 是触发访问的指令地址。
    far = hook_regs->regs[0];
    esr = hook_regs->regs[1];
    regs = (struct pt_regs *)hook_regs->regs[2];
    ec = (esr >> 26) & 0x3f;
    fsc = esr & PTEBP_ESR_FSC_MASK;
    fault_page = untagged_addr(far) & PAGE_MASK;

    if (ec != PTEBP_ESR_EC_DABT_LOW) return 0;

    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING)) return 0;

    // PTEBP 只接管本模块 execute-only PTE 产生的 L3 权限异常。
    if (fsc != PTEBP_ESR_FSC_PERM_L3) return 0;

    // 根据当前 mm 和 FAR 页地址定位对应 slot，避免接管其他进程或其他页面的权限异常。
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || g_ptebp_mm != current->mm) goto out_unlock;

    mem_ctx = (struct ptebp_emu_mem_ctx){};
    for (point_slot = 0; point_slot < ARRAY_SIZE(g_ptebp_slots); point_slot++)
    {
        struct ptebp_slot *slot = &g_ptebp_slots[point_slot];
        struct ptebp_slot *shadow;

        if (!slot->hook_addr) continue;
        if (!ptebp_validate_slot(slot, current->mm))
        {
            stale_slot = true;
            break;
        }
        if (!owner && (slot->hook_addr & PAGE_MASK) == fault_page) owner = slot;

        shadow = &mem_ctx.slots[point_slot];
        *shadow = *slot;
    }
    if (stale_slot)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        ptebp_drop_all_monitors(false);
        hook_regs->regs[0] = 0;
        return 1;
    }
    if (!owner) goto out_unlock;
out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    if (!owner) return 0;

    if (!emulate_insn(regs, NULL, &mem_access)) ptebp_drop_all_monitors(false);
    hook_regs->regs[0] = 0;
    return 1;
}

static struct hook_entry g_ptebp_fault_hook = HOOK_ENTRY("do_mem_abort", ptebp_handle_fault);
static struct hook_entry g_ptebp_brk_hook = HOOK_ENTRY("brk_handler", ptebp_handle_brk);

static inline void stop_ptebp_monitor(void)
{
    ptebp_drop_all_monitors(true);
    hook_entry_remove(&g_ptebp_fault_hook);
    hook_entry_remove(&g_ptebp_brk_hook);
    synchronize_rcu();
}

static int ptebp_install_slot(struct break_point *info, size_t point_slot, struct mm_struct *mm)
{
    struct bp_point *point;
    struct ptebp_slot *slot;
    pte_t *ptep;
    pte_t orig_pte;
    struct ptebp_slot *page_owner;
    uint64_t page_vaddr;
    uint64_t hook_addr;
    uint32_t brk_insn = PTEBP_BRK_INSN;
    size_t scan_slot;
    int status = 0;

    if (point_slot >= ARRAY_SIZE(g_ptebp_slots)) return -EINVAL;

    point = &info->points[point_slot];
    slot = &g_ptebp_slots[point_slot];
    hook_addr = untagged_addr(point->hit_addr) & ~0x3ULL;
    if (!hook_addr || hook_addr >= READ_ONCE(mm->task_size) || sizeof(slot->orig_insn) > READ_ONCE(mm->task_size) - hook_addr) return -EFAULT;
    page_vaddr = hook_addr & PAGE_MASK;

    ptep = get_user_pte(mm, page_vaddr);
    if (!ptep) return -EFAULT;

    orig_pte = READ_ONCE(*ptep);
    if (!pte_present(orig_pte)) return -EFAULT;

    page_owner = NULL;
    for (scan_slot = 0; scan_slot < ARRAY_SIZE(g_ptebp_slots); scan_slot++)
    {
        struct ptebp_slot *candidate = &g_ptebp_slots[scan_slot];

        if (!candidate->hook_addr) continue;
        if (candidate->hook_addr == hook_addr) return -EEXIST;
        if ((candidate->hook_addr & PAGE_MASK) == page_vaddr) page_owner = candidate;
    }

    if (page_owner && !ptebp_validate_slot(page_owner, mm)) return -EFAULT;

    if (!page_owner && (pte_val(orig_pte) & PTEBP_UXN)) return -EACCES;

    if (page_owner) orig_pte = page_owner->orig_pte;

    *slot = (struct ptebp_slot){
        .orig_pte = orig_pte,
        .hook_addr = hook_addr,
    };
    status = ptebp_access_insn(slot, &slot->orig_insn, false);
    if (status) goto clear_slot;
    if (slot->orig_insn == PTEBP_BRK_INSN)
    {
        status = -ESTALE;
        goto clear_slot;
    }

    status = ptebp_access_insn(slot, &brk_insn, true);
    if (!status && !page_owner) status = write_user_pte_value(mm, page_vaddr, ptebp_make_execute_only_pte(pte_val(orig_pte)));

    if (!status) return 0;

    if (status) (void)ptebp_access_insn(slot, &slot->orig_insn, true);
clear_slot:
    memset(slot, 0, sizeof(*slot));
    return status;
}

static inline int start_ptebp_monitor(struct break_point *info)
{
    int status;
    size_t point_slot;
    struct mm_struct *mm;
    unsigned long flags;

    BUILD_BUG_ON(ARRAY_SIZE(g_ptebp_slots) != ARRAY_SIZE(((struct break_point *)0)->points));
    BUILD_BUG_ON(ARRAY_SIZE(g_ptebp_slots) != ARRAY_SIZE(((struct ptebp_emu_mem_ctx *)0)->slots));

    if (!info || info->pid <= 0) return -EINVAL;

    for (point_slot = 0; point_slot < ARRAY_SIZE(info->points); point_slot++)
        if (ptebp_point_is_active(&info->points[point_slot])) break;
    if (point_slot == ARRAY_SIZE(info->points)) return -EINVAL;

    stop_ptebp_monitor();

    mm = get_mm_by_pid(info->pid);
    if (!mm) return -EINVAL;

    status = hook_entry_install(&g_ptebp_fault_hook);
    if (status) goto err_put_mm;

    status = hook_entry_install(&g_ptebp_brk_hook);
    if (status) goto err_remove_fault_hook;

    mmap_read_lock(mm);
    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_mm = mm;
    for (point_slot = 0; point_slot < ARRAY_SIZE(info->points); point_slot++)
    {
        if (!ptebp_point_is_active(&info->points[point_slot])) continue;
        status = ptebp_install_slot(info, point_slot, mm);
        if (status) break;
    }
    if (!status) g_ptebp_info = info;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    mmap_read_unlock(mm);

    if (!status) return 0;

    stop_ptebp_monitor();
    return status;

err_remove_fault_hook:
    hook_entry_remove(&g_ptebp_fault_hook);
err_put_mm:
    mmput(mm);
    return status;
}

#endif // ARM64_PTEDBG_H