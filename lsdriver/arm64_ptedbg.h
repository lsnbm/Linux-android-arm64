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
    struct mm_struct *mm;
    pte_t *ptep;
    pte_t orig_pte;
    pte_t installed_pte;
    uint64_t page_vaddr;
    uint64_t hook_addr;
    uint32_t orig_insn;
    bool patched;
    bool pte_installed;
    bool owns_pte;
};

static struct break_point *g_ptebp_info;
static struct ptebp_slot g_ptebp_slots[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);

static inline bool ptebp_point_is_active(struct bp_point *point)
{
    return point && point->hit_addr != 0 && point->bt == BP_BREAKPOINT_X;
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

static struct ptebp_slot *ptebp_find_page_owner_locked(struct mm_struct *mm, uint64_t page_vaddr)
{
    int point_slot;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct ptebp_slot *slot = &g_ptebp_slots[point_slot];

        if (ptebp_slot_active(slot) && slot->mm == mm &&
            slot->page_vaddr == page_vaddr && slot->owns_pte)
            return slot;
    }

    return NULL;
}

static bool ptebp_hook_already_installed_locked(struct mm_struct *mm, uint64_t hook_addr)
{
    int point_slot;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct ptebp_slot *slot = &g_ptebp_slots[point_slot];

        if (ptebp_slot_active(slot) && slot->mm == mm && slot->hook_addr == hook_addr)
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

/*
 * PTEBP 页安装后是 execute-only，AT S1E0R 无法再为
 * virtual_memory_rw() 完成数据访问翻译。直接使用已验证 PTE 的 PFN
 * 写入并回读指令，避免撤销时原指令恢复失败而残留 BRK。
 */
static inline int ptebp_write_insn(struct ptebp_slot *slot, uint32_t insn)
{
    pte_t pte;
    unsigned long pfn;
    phys_addr_t paddr;
    uint32_t readback = 0;
    int status;

    if (!slot || !slot->mm || !slot->ptep ||
        (slot->hook_addr & (PAGE_SIZE - 1)) > PAGE_SIZE - sizeof(insn))
        return -EINVAL;

    pte = READ_ONCE(*slot->ptep);
    if (!pte_present(pte))
        return -ESTALE;

    pfn = pte_pfn(pte);
    if (!pfn_valid(pfn))
        return -EFAULT;

    paddr = ((phys_addr_t)pfn << PAGE_SHIFT) |
            (slot->hook_addr & ~PAGE_MASK);
    status = pte_write_physical(paddr, &insn, sizeof(insn));
    if (status)
        return status;

    status = pte_read_physical(paddr, &readback, sizeof(readback));
    if (status)
        return status;
    return readback == insn ? 0 : -EIO;
}

static inline int ptebp_patch_brk(struct ptebp_slot *slot)
{
    int status;

    status = ptebp_write_insn(slot, PTEBP_BRK_INSN);
    if (!status)
        slot->patched = true;
    return status;
}

static inline int ptebp_restore_patch(struct ptebp_slot *slot)
{
    int status;

    if (!slot || !slot->patched)
        return 0;

    status = ptebp_write_insn(slot, slot->orig_insn);
    if (!status)
        slot->patched = false;
    return status;
}

static inline void ptebp_restore_pte(struct ptebp_slot *slot)
{
    if (!slot || !slot->mm || !slot->owns_pte || !slot->pte_installed)
        return;

    (void)write_user_pte_value(slot->mm, slot->page_vaddr, pte_val(slot->orig_pte));
    slot->pte_installed = false;
}

static inline pteval_t ptebp_mutable_pte_mask(void)
{
    pteval_t mask = 0;

#ifdef PTE_AF
    mask |= PTE_AF;
#endif
#ifdef PTE_DIRTY
    mask |= PTE_DIRTY;
#endif

    return mask;
}

static bool ptebp_validate_slot(struct ptebp_slot *slot)
{
    pte_t *fresh_ptep;
    pte_t pte_now;
    pte_t expected_pte;
    pteval_t changed;

    if (!slot || !slot->mm || !slot->ptep || !slot->page_vaddr)
        return false;

    fresh_ptep = get_user_pte(slot->mm, slot->page_vaddr);
    if (!fresh_ptep || fresh_ptep != slot->ptep)
        return false;

    pte_now = READ_ONCE(*fresh_ptep);
    if (!pte_present(pte_now))
        return false;

    expected_pte = slot->pte_installed ? slot->installed_pte : slot->orig_pte;
    if (pte_pfn(pte_now) != pte_pfn(expected_pte))
        return false;

    changed = pte_val(pte_now) ^ pte_val(expected_pte);
    if (changed & ~ptebp_mutable_pte_mask())
        return false;

    return true;
}

static struct mm_struct *ptebp_deactivate_locked(struct ptebp_slot *slot)
{
    struct mm_struct *mm = slot->mm;

    memset(slot, 0, sizeof(*slot));
    return mm;
}

struct ptebp_emu_patch
{
    uint64_t hook_addr;
    uint32_t orig_insn;
};

struct ptebp_emu_mem_ctx
{
    pte_t pte;
    uint64_t page_vaddr;
    int patch_count;
    struct ptebp_emu_patch patches[BP_CONFIG_MAX];
};

/*
 * PTEBP 把目标页改成 execute-only 后，__get_user 会按 EL0 权限再次触发
 * Data Abort。这里通过当前 PTE 的 PFN 从内核线性映射读取，并在读取范围
 * 覆盖补丁位置时还原 orig_insn，供 LDP/LDR/向量读取等通用模拟路径使用。
 */
static int ptebp_emu_read_mem(void *opaque, uint64_t addr, int bytes, __uint128_t *out)
{
    struct ptebp_emu_mem_ctx *ctx = opaque;
    uint8_t data[sizeof(__uint128_t)];
    uint8_t orig_bytes[sizeof(uint32_t)];
    uint64_t end;
    uint64_t page_end;
    uint64_t overlap_start;
    uint64_t overlap_end;
    unsigned long pfn;
    phys_addr_t paddr;
    __uint128_t value = 0;
    int patch_index;
    int status;

    if (!ctx || !out || bytes <= 0 || bytes > sizeof(data))
        return -EINVAL;
    if (addr > U64_MAX - (uint64_t)bytes)
        return -EFAULT;

    end = addr + (uint64_t)bytes;
    page_end = ctx->page_vaddr + PAGE_SIZE;

    if (end <= ctx->page_vaddr || addr >= page_end)
        return -EOPNOTSUPP;
    if (addr < ctx->page_vaddr || end > page_end)
        return -EFAULT;
    if (!pte_present(ctx->pte))
        return -EFAULT;

    pfn = pte_pfn(ctx->pte);
    if (!pfn_valid(pfn))
        return -EFAULT;

    paddr = ((phys_addr_t)pfn << PAGE_SHIFT) + (addr - ctx->page_vaddr);
    status = linear_read_physical(paddr, data, bytes);
    if (status)
        return status;

    for (patch_index = 0; patch_index < ctx->patch_count; patch_index++)
    {
        struct ptebp_emu_patch *patch = &ctx->patches[patch_index];

        overlap_start = addr > patch->hook_addr ? addr : patch->hook_addr;
        overlap_end = end < patch->hook_addr + sizeof(patch->orig_insn)
                          ? end
                          : patch->hook_addr + sizeof(patch->orig_insn);
        if (overlap_start < overlap_end)
        {
            __builtin_memcpy(orig_bytes, &patch->orig_insn, sizeof(orig_bytes));
            __builtin_memcpy(data + (overlap_start - addr),
                             orig_bytes + (overlap_start - patch->hook_addr),
                             overlap_end - overlap_start);
        }
    }

    __builtin_memcpy(&value, data, bytes);
    *out = value;
    return 0;
}

// 目标进程把代码页当数据读取时，FAR 是被读取的数据地址，regs->pc 是触发异常的读取指令地址。
// 如果读取范围完全落在被 BRK 覆盖的 4 字节内，就直接从 orig_insn 合成读取结果，
// 写回读取指令的目标寄存器并推进 PC；整个过程不需要临时撤销 BRK 或恢复页面权限。
// 当前快速路径只处理 1/2/4 字节整数读取，其他形式返回 false 交给通用模拟器处理。
static bool ptebp_try_return_orig_insn(struct pt_regs *regs, struct ptebp_slot *slot,
                                       uint64_t far, const struct emu_mem_access *mem_access)
{
    uint64_t addr = untagged_addr(far);
    uint64_t hook_addr = slot->hook_addr;
    uint32_t insn;
    __uint128_t fetched_insn;
    uint32_t size;
    uint32_t opc;
    uint32_t rn;
    uint32_t rt;
    uint32_t idx;
    int bytes;
    bool reg_form;
    bool writeback = false;
    s64 imm9;
    uint64_t wb_addr = 0;
    uint64_t shift;
    uint64_t value;
    uint64_t sign;

    // 只有读取起始地址命中 BRK 所在的 4 字节，才需要用 orig_insn 替换补丁内容。
    if (addr < hook_addr || addr >= hook_addr + sizeof(slot->orig_insn))
        return false;

    // 读取引发 Data Abort 的用户态指令，用它解析读取宽度、Rt、Rn 和寻址模式。
    if (emu_read_mem(mem_access, regs->pc, sizeof(insn), &fetched_insn))
        return false;
    insn = (uint32_t)fetched_insn;

    // 仅接受整数单寄存器 load/store 编码族，并排除 FP/SIMD 编码。
    if ((insn & 0x3A000000) != 0x38000000 || (insn & 0x04000000))
        return false;

    size = (insn >> 30) & 0x3;
    opc = (insn >> 22) & 0x3;
    rn = (insn >> 5) & 0x1F;
    rt = insn & 0x1F;
    bytes = 1 << size;

    if (!opc || size == 3 || (size == 2 && opc == 3))
        return false;
    if (emu_is_lse_atomic(insn))
        return false;
    // 当前读取必须完整包含在 orig_insn 的 4 字节范围内，跨边界读取交给通用路径。
    if ((addr + bytes) > (hook_addr + sizeof(slot->orig_insn)))
        return false;

    // pre-index/post-index 读取除了写回 Rt，还必须同步更新基址寄存器 Rn/SP。
    if (!((insn >> 24) & 1))
    {
        idx = (insn >> 10) & 0x3;
        reg_form = ((insn >> 21) & 1) != 0;

        if (reg_form && idx != 2)
            return false;

        if (!reg_form && (idx == 1 || idx == 3))
        {
            if (rn != 31 && rn == rt)
                return false;

            imm9 = (s64)((insn >> 12) & 0x1FF);
            if (imm9 & 0x100)
                imm9 -= 0x200;

            wb_addr = addr_reg_read(regs, rn) + (uint64_t)imm9;
            writeback = true;
        }
    }

    // 根据 FAR 在 4 字节原指令中的偏移，提取本次 1/2/4 字节读取应返回的原始内容。
    shift = (addr - hook_addr) * 8;
    value = slot->orig_insn >> shift;
    if (bytes != 4)
        value &= (1ULL << (bytes * 8)) - 1;
    if (opc >= 2)
    {
        sign = 1ULL << (bytes * 8 - 1);
        value = (value ^ sign) - sign;
    }

    // 直接构造读取指令执行后的用户态现场，然后跳到下一条指令继续执行。
    if (rt != 31)
        regs->regs[rt] = (opc == 2) ? value : (uint32_t)value;
    if (writeback)
        addr_reg_write(regs, rn, wb_addr);
    regs->pc += 4;
    return true;
}

static void ptebp_restore_slot_patch(struct ptebp_slot *slot)
{
    bool valid;

    if (!slot || !slot->mm)
        return;

    mmap_read_lock(slot->mm);
    valid = ptebp_validate_slot(slot);
    ls_log_tag("ptebp", "restore patch pid=%d hook=0x%llx page=0x%llx valid=%d patched=%d owner=%d\n",
               slot->pid, (unsigned long long)slot->hook_addr,
               (unsigned long long)slot->page_vaddr, valid,
               slot->patched, slot->owns_pte);
    if (valid)
        (void)ptebp_restore_patch(slot);
    mmap_read_unlock(slot->mm);
}

static void ptebp_restore_slot_pte(struct ptebp_slot *slot)
{
    bool valid;

    if (!slot || !slot->mm || !slot->owns_pte)
        return;

    mmap_read_lock(slot->mm);
    valid = ptebp_validate_slot(slot);
    ls_log_tag("ptebp", "restore pte pid=%d page=0x%llx valid=%d installed=%d\n",
               slot->pid, (unsigned long long)slot->page_vaddr,
               valid, slot->pte_installed);
    if (valid)
        ptebp_restore_pte(slot);
    mmap_read_unlock(slot->mm);
}

static void ptebp_drop_all_monitors(void)
{
    struct mm_struct *drop_mms[BP_CONFIG_MAX];
    struct ptebp_slot *slot;
    unsigned long flags;
    int point_slot;

    memset(drop_mms, 0, sizeof(drop_mms));

    spin_lock_irqsave(&g_ptebp_lock, flags);
    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        slot = &g_ptebp_slots[point_slot];
        if (!ptebp_slot_active(slot))
            continue;

        if (ptebp_validate_slot(slot))
            (void)ptebp_restore_patch(slot);
    }
    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        slot = &g_ptebp_slots[point_slot];
        if (!ptebp_slot_active(slot) || !slot->owns_pte)
            continue;

        if (ptebp_validate_slot(slot))
            ptebp_restore_pte(slot);
    }
    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        slot = &g_ptebp_slots[point_slot];
        if (!ptebp_slot_active(slot))
            continue;

        drop_mms[point_slot] = ptebp_deactivate_locked(slot);
    }

    g_ptebp_info = NULL;
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
    uint32_t emulate_insn_word;
    struct bp_point *hit_point = NULL;
    bool emulate_ok;

    if (!hook_regs)
        return 0;

    regs = (struct pt_regs *)hook_regs->regs[2];
    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING))
        return 0;

    pc = untagged_addr(regs->pc) & ~0x3ULL;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info)
        goto out_unlock;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct ptebp_slot *slot = &g_ptebp_slots[point_slot];

        if (!ptebp_slot_active(slot) ||
            slot->mm != current->mm ||
            slot->hook_addr != pc)
            continue;

        hit_point = &g_ptebp_info->points[point_slot];
        emulate_insn_word = slot->orig_insn;
        break;
    }

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (!hit_point)
        return 0;

    if (hit_point->on_hit)
        hit_point->on_hit((void *)regs, (void *)hit_point);

    emulate_ok = emulate_insn(regs, &emulate_insn_word);
    if (emulate_ok)
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
    int insn_status = -EINVAL;
    uint32_t fault_insn = 0;
    struct ptebp_slot *slot = NULL;
    struct ptebp_slot slot_snapshot;
    struct ptebp_emu_mem_ctx mem_ctx;
    struct emu_mem_access mem_access = {
        .read = ptebp_emu_read_mem,
        .ctx = &mem_ctx,
    };
    bool drop_all = false;

    if (!hook_regs)
        return 0;

    // do_mem_abort(far, esr, regs)：far 是数据访问地址，regs->pc 是触发访问的指令地址。
    far = hook_regs->regs[0];
    esr = hook_regs->regs[1];
    regs = (struct pt_regs *)hook_regs->regs[2];
    ec = (esr >> 26) & 0x3f;
    fsc = esr & PTEBP_ESR_FSC_MASK;
    fault_page = untagged_addr(far) & PAGE_MASK;

    if (ec != PTEBP_ESR_EC_DABT_LOW)
        return 0;

    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING))
        return 0;

    // PTEBP 只接管本模块 execute-only PTE 产生的 L3 权限异常。
    if (fsc != PTEBP_ESR_FSC_PERM_L3)
        return 0;

    // 根据当前 mm 和 FAR 页地址定位对应 slot，避免接管其他进程或其他页面的权限异常。
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info)
        goto out_unlock;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct ptebp_slot *candidate = &g_ptebp_slots[point_slot];

        if (ptebp_slot_active(candidate) && candidate->owns_pte &&
            candidate->mm == current->mm &&
            fault_page == candidate->page_vaddr)
        {
            slot = candidate;
            break;
        }
    }

    if (!slot)
        goto out_unlock;

    if (!ptebp_validate_slot(slot))
    {
        drop_all = true;
        slot = NULL;
        goto out_unlock;
    }

    slot_snapshot = *slot;
    mem_ctx = (struct ptebp_emu_mem_ctx){
        .pte = READ_ONCE(*slot->ptep),
        .page_vaddr = slot->page_vaddr,
    };
    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct ptebp_slot *candidate = &g_ptebp_slots[point_slot];

        if (!ptebp_slot_active(candidate) ||
            candidate->mm != current->mm ||
            candidate->page_vaddr != fault_page)
            continue;

        mem_ctx.patches[mem_ctx.patch_count].hook_addr = candidate->hook_addr;
        mem_ctx.patches[mem_ctx.patch_count].orig_insn = candidate->orig_insn;
        mem_ctx.patch_count++;

        if (untagged_addr(far) >= candidate->hook_addr &&
            untagged_addr(far) < candidate->hook_addr + sizeof(candidate->orig_insn))
        {
            slot_snapshot.hook_addr = candidate->hook_addr;
            slot_snapshot.orig_insn = candidate->orig_insn;
        }
    }
    slot = &slot_snapshot;

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    if (drop_all)
        ptebp_drop_all_monitors();

    if (!slot)
        return 0;

    {
        __uint128_t fetched_insn;

        insn_status = emu_read_mem(&mem_access, regs->pc, sizeof(fault_insn),
                                   &fetched_insn);
        if (!insn_status)
            fault_insn = (uint32_t)fetched_insn;
    }

    // 快速路径：读取命中 BRK 的 4 字节时，直接把保存的 orig_insn 内容写入目标寄存器。
    // 成功后 regs->pc 已推进，返回 1 跳过原始 do_mem_abort。
    if (ptebp_try_return_orig_insn(regs, slot, far, &mem_access))
    {
        hook_regs->regs[0] = 0;
        return 1;
    }

    // 通用路径：模拟整条触发 Data Abort 的读取指令，由模拟器更新寄存器和 regs->pc。
    // PTEBP 专用读取器通过 PFN 读取目标页，并覆盖回 BRK 对应的 orig_insn 字节。
    if (!insn_status && emulate_insn_with_mem(regs, &fault_insn, &mem_access))
    {
        hook_regs->regs[0] = 0;
        return 1;
    }

    // 无法模拟时撤销全部 PTEBP；这里不推进 regs->pc，让用户态从当前指令重新执行。
    ptebp_drop_all_monitors();
    hook_regs->regs[0] = 0;
    return 1;
}

static struct hook_entry g_ptebp_fault_hook = HOOK_ENTRY("do_mem_abort", ptebp_handle_fault);
static struct hook_entry g_ptebp_brk_hook = HOOK_ENTRY("brk_handler", ptebp_handle_brk);

static void ptebp_clear_slots_locked(struct ptebp_slot old_slots[BP_CONFIG_MAX], struct mm_struct *drop_mms[BP_CONFIG_MAX])
{
    int point_slot;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (!ptebp_slot_active(&g_ptebp_slots[point_slot]))
            continue;

        old_slots[point_slot] = g_ptebp_slots[point_slot];
        drop_mms[point_slot] = ptebp_deactivate_locked(&g_ptebp_slots[point_slot]);
    }
}

static inline void stop_ptebp_monitor(void)
{
    struct ptebp_slot old_slots[BP_CONFIG_MAX];
    struct mm_struct *drop_mms[BP_CONFIG_MAX];
    unsigned long flags;
    int point_slot;

    memset(old_slots, 0, sizeof(old_slots));
    memset(drop_mms, 0, sizeof(drop_mms));

    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_info = NULL;
    ptebp_clear_slots_locked(old_slots, drop_mms);
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (ptebp_slot_active(&old_slots[point_slot]))
            ptebp_restore_slot_patch(&old_slots[point_slot]);
    }
    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (ptebp_slot_active(&old_slots[point_slot]) && old_slots[point_slot].owns_pte)
            ptebp_restore_slot_pte(&old_slots[point_slot]);
    }
    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
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
    struct ptebp_slot *page_owner;
    uint64_t page_vaddr;
    uint64_t hook_addr;
    uint32_t orig_insn;
    unsigned long flags;
    bool slot_owns_mm = false;
    int status = 0;

    if (!ptebp_point_is_active(point))
        return 0;

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
    if (orig_insn == PTEBP_BRK_INSN)
    {
        status = -ESTALE;
        goto out_unlock_mm;
    }

    ptep = get_user_pte(mm, page_vaddr);
    if (!ptep)
    {
        status = -EFAULT;
        goto out_unlock_mm;
    }

    orig_pte = READ_ONCE(*ptep);
    if (!pte_present(orig_pte))
    {
        status = -EFAULT;
        goto out_unlock_mm;
    }

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (ptebp_hook_already_installed_locked(mm, hook_addr))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        status = -EEXIST;
        goto out_unlock_mm;
    }

    page_owner = ptebp_find_page_owner_locked(mm, page_vaddr);
    if (page_owner && !ptebp_validate_slot(page_owner))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        status = -EFAULT;
        goto out_unlock_mm;
    }

    if (!page_owner && (pte_val(orig_pte) & PTEBP_UXN))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        status = -EACCES;
        goto out_unlock_mm;
    }

    if (page_owner)
    {
        ptep = page_owner->ptep;
        orig_pte = page_owner->orig_pte;
    }

    *slot = (struct ptebp_slot){
        .pid = info->pid,
        .mm = mm,
        .ptep = ptep,
        .orig_pte = orig_pte,
        .installed_pte = page_owner
                             ? page_owner->installed_pte
                             : __pte(ptebp_make_execute_only_pte(pte_val(orig_pte))),
        .page_vaddr = page_vaddr,
        .hook_addr = hook_addr,
        .orig_insn = orig_insn,
        .pte_installed = page_owner != NULL,
        .owns_pte = page_owner == NULL,
    };
    ls_log_tag("ptebp", "install slot=%d pid=%d hook=0x%llx page=0x%llx owner=%d orig_insn=0x%08x orig_pte=0x%llx new_pte=0x%llx\n",
               point_slot, info->pid,
               (unsigned long long)hook_addr,
               (unsigned long long)page_vaddr,
               slot->owns_pte,
               orig_insn,
               (unsigned long long)pte_val(orig_pte),
               (unsigned long long)pte_val(slot->installed_pte));
    status = ptebp_patch_brk(slot);
    ls_log_tag("ptebp", "install patch slot=%d status=%d patched=%d\n",
               point_slot, status, slot->patched);
    if (!status && slot->owns_pte)
    {
        status = write_user_pte_value(slot->mm, slot->page_vaddr, pte_val(slot->installed_pte));
        if (!status)
            slot->pte_installed = true;
        ls_log_tag("ptebp", "install pte slot=%d status=%d current_pte=0x%llx installed=%d\n",
                   point_slot, status,
                   (unsigned long long)pte_val(READ_ONCE(*slot->ptep)),
                   slot->pte_installed);
    }

    if (!status)
    {
        slot_owns_mm = true;
    }
    else
    {
        (void)ptebp_restore_patch(slot);
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