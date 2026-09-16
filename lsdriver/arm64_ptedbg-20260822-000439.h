#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

/*
使用 UDF 替换执行断点指令。安装时将每个受管用户虚拟页对应物理页的完整内容复制到内核页，
作为同一用户页内全部断点槽位共享的影子页。
受管页上由受支持读类指令触发的权限异常从影子页取数；写权限异常则撤销整组监控并重试原指令。
*/

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/esr.h>
#include <asm/memory.h>
#include <asm/mman.h>
#include <asm/ptrace.h>

#include "arm64_reg.h"
#include "export_fun.h"
#include "inline_hook_frame.h"
#include "io_struct.h"
#include "arm64_emulate/emulate_inst.h"
#include "virtual_memory_rw.h"

#ifndef ARCH_VM_PKEY_FLAGS
#define ARCH_VM_PKEY_FLAGS 0
#endif

#define PTEBP_UDF_INST     0x00000000U
#define PTEBP_IC_IVAU_MASK 0xFFFFFFE0U
#define PTEBP_IC_IVAU_INST 0xD50B7520U

struct ptebp_slot
{
    pte_t orig_pte;
    uint64_t hook_addr;
    uint64_t page_vaddr;
    void *shadow_page;
};

static struct break_point *g_ptebp_info;
static struct mm_struct *g_ptebp_mm;
static struct ptebp_slot g_ptebp_slots[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);
static DEFINE_MUTEX(g_ptebp_control_lock);
static bool g_ptebp_stopping;

// 从原始 PTE 派生禁止用户态读写的 guard PTE，同时保留地址及其他属性位。
static inline pteval_t ptebp_make_data_guard_pte(pteval_t value)
{
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

// 将断点地址对应的物理指令改写为 inst，刷新指令缓存并回读校验。
static inline int ptebp_write_inst(struct ptebp_slot *slot, const uint32_t *inst)
{
    phys_addr_t paddr;
    uint32_t readback;
    int status;

    if (!g_ptebp_mm || !slot || !inst) return -EINVAL;
    status = walk_translate_va_to_pa(g_ptebp_mm, slot->hook_addr, &paddr);
    if (status) return status;

    status = linear_write_physical(paddr, inst, sizeof(*inst));
    if (status) return status;
    status = arm64_sync_code_range_all_cpus(phys_to_virt(paddr), sizeof(*inst));
    if (status) return status;

    status = linear_read_physical(paddr, &readback, sizeof(readback));
    if (status) return status;
    return readback == *inst ? 0 : -EIO;
}

// 从槽位关联的影子页读取断点地址处的原始 32 位指令。
static inline int ptebp_read_shadow_inst(const struct ptebp_slot *slot, uint32_t *inst)
{
    uint64_t hook_addr;
    uint64_t page_vaddr;
    void *shadow_page;
    size_t offset;

    if (!slot || !inst) return -EINVAL;
    hook_addr = READ_ONCE(slot->hook_addr);
    page_vaddr = READ_ONCE(slot->page_vaddr);
    shadow_page = READ_ONCE(slot->shadow_page);
    if (!shadow_page) return -EINVAL;
    offset = hook_addr - page_vaddr;
    if (offset > PAGE_SIZE - sizeof(*inst)) return -EFAULT;

    __builtin_memcpy(inst, (uint8_t *)shadow_page + offset, sizeof(*inst));
    return 0;
}

// 从影子页取出原始指令并写回目标代码页。
static inline int ptebp_restore_inst(struct ptebp_slot *slot)
{
    uint32_t inst;
    int status;

    status = ptebp_read_shadow_inst(slot, &inst);
    if (status) return status;
    return ptebp_write_inst(slot, &inst);
}

// 无锁按页查找共享影子页，供只读异常路径使用。
static void *ptebp_find_shadow_page(uint64_t page_vaddr)
{
    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(g_ptebp_slots); slot_index++)
    {
        if (READ_ONCE(g_ptebp_slots[slot_index].hook_addr) && READ_ONCE(g_ptebp_slots[slot_index].page_vaddr) == page_vaddr) return READ_ONCE(g_ptebp_slots[slot_index].shadow_page);
    }

    return NULL;
}

// 检查当前 PTE 是否仍匹配安装时的 guard PTE，允许硬件更新 AF 和 DIRTY 位。
static bool ptebp_validate_guard_pte(struct mm_struct *mm, uint64_t addr, pte_t orig_pte)
{
    pteval_t changed;
    pteval_t current_value;
    pteval_t mutable = 0;

    if (!addr || read_user_pte_value(mm, addr & PAGE_MASK, &current_value)) return false;
    changed = current_value ^ ptebp_make_data_guard_pte(pte_val(orig_pte));
#ifdef PTE_AF
    mutable |= PTE_AF;
#endif
#ifdef PTE_DIRTY
    mutable |= PTE_DIRTY;
#endif

    return !(changed & ~mutable);
}

// ======================== 影子页数据读取 ========================

/*
从受管页对应的影子页读取模拟访存数据，不翻译或读取真实用户物理页。
异常入口只保证 FAR 所在页受管；若一次 load 跨页，则逐页查找并拼接各受管页的影子数据。
跨入任一没有影子页的页面时返回错误，上层撤销整组监控并保持原 PC，让原始 load 重试。
*/
static int ptebp_emu_read_mem(uint64_t addr, size_t bytes, __uint128_t *out)
{
    size_t data_offset = 0;

    if (!bytes || bytes > sizeof(__uint128_t)) return -EINVAL;
    *out = 0;
    addr = untagged_addr(addr);
    if (addr > U64_MAX - ((uint64_t)bytes - 1)) return -EFAULT;

    while (data_offset < bytes)
    {
        uint64_t current_addr = addr + data_offset;
        size_t page_offset = current_addr & ~((uint64_t)PAGE_MASK);
        size_t chunk_size = min_t(size_t, bytes - data_offset, PAGE_SIZE - page_offset);
        void *shadow_page = ptebp_find_shadow_page(current_addr & PAGE_MASK);

        if (!shadow_page) return -EFAULT;

        __builtin_memcpy((uint8_t *)out + data_offset, (uint8_t *)shadow_page + page_offset, chunk_size);
        data_offset += chunk_size;
    }

    return 0;
}

// ======================== 页数据访问异常处理 ========================

/*
guard PTE 会主动撤销受管页的数据访问权限，因此这里处理的是监控触发的权限异常，
而不是原指令本身非法。此函数只模拟读类指令：
- IC IVAU 通过物理别名完成指令缓存同步；
- PRFM 没有架构可见的数据结果，只推进 PC；
- 普通 GPR、FP/SIMD 和 pair load 从影子页取数，提交寄存器、基址写回和 PC；
- store、exclusive、CAS/CASP、LSE RMW、SWP 等指令不在这里模拟。
写权限异常由调用方先撤销监控，再让原始 store 重试。
*/
static enum emu_inst_result ptebp_emulate_load_store(struct pt_regs *regs, uint32_t raw_inst)
{
    struct arm64_decoded_instruction decoded_result __attribute__((__uninitialized__));
    uint64_t pc = regs->pc;
    uint64_t base, address;

    if ((raw_inst & PTEBP_IC_IVAU_MASK) == PTEBP_IC_IVAU_INST)
    {
        phys_addr_t paddr;

        // IC IVAU, Xt 按 Xt 给出的用户 VA 失效对应指令缓存行；Rt=31 按 XZR 读取为 0。
        address = untagged_addr(read_gpr_or_zr(regs, raw_inst & 0x1FU));
        if (!current->mm || address >= READ_ONCE(current->mm->task_size)) goto emulate_failed;
        if (walk_translate_va_to_pa(current->mm, address, &paddr)) goto emulate_failed;

        // guard PTE 阻止直接使用用户 VA，改用同一物理位置的内核线性别名完成 D-cache 清理和 I-cache 失效。
        if (arm64_sync_code_range_all_cpus(phys_to_virt(paddr), 1)) goto emulate_failed;

        regs->pc = pc + 4;
        return EMU_INST_HANDLED;
    }

    if (arm64_decode_instruction(raw_inst, &decoded_result) != ARM64_DECODE_OK) goto emulate_failed;
    // PRFM 不产生架构可见的数据结果，直接推进 PC。
    switch (decoded_result.instruction)
    {
    case ARM64_INST_PRFM_LITERAL:
    case ARM64_INST_PRFUM:
    case ARM64_INST_PRFM_REGISTER_OFFSET:
    case ARM64_INST_PRFM_UNSIGNED_OFFSET:
        regs->pc = pc + 4;
        return EMU_INST_HANDLED;
    default:
        break;
    }

    // 按指令寻址模式计算有效地址；不支持的形式会在后续派发阶段失败。
    switch (decoded_result.instruction)
    {
    case ARM64_INST_LDR_GPR_LITERAL:
    case ARM64_INST_LDRSW_LITERAL:
    case ARM64_INST_LDR_FP_SIMD_LITERAL:
        address = pc + decoded_result.offset;
        break;
    case ARM64_INST_LDRB_GPR_POST_INDEX:
    case ARM64_INST_LDRH_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDRSB_GPR_POST_INDEX:
    case ARM64_INST_LDRSH_GPR_POST_INDEX:
    case ARM64_INST_LDRSW_GPR_POST_INDEX:
    case ARM64_INST_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDP_GPR_POST_INDEX:
    case ARM64_INST_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPSW_POST_INDEX:
        base = read_gpr_or_sp(regs, decoded_result.rn);
        address = base;
        break;
    case ARM64_INST_LDRB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSW_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET:
    {
        base = read_gpr_or_sp(regs, decoded_result.rn);
        uint64_t index = read_gpr_or_zr(regs, decoded_result.rm);
        switch (decoded_result.extend_type)
        {
        case 2: // UXTW
            index = (uint32_t)index;
            break;
        case 3: // LSL / UXTX
            break;
        case 6: // SXTW
            index = (uint64_t)(int64_t)(int32_t)index;
            break;
        case 7: // SXTX
            break;
        default:
            goto emulate_failed;
        }
        address = base + (index << decoded_result.shift_amount);
        break;
    }
    default:
        base = read_gpr_or_sp(regs, decoded_result.rn);
        address = base + decoded_result.offset;
        break;
    }

    // 按指令族和 operand_width 派发影子页读取，成功后统一提交结果。
    switch (decoded_result.instruction)
    {
    // ----- 通用寄存器 (GPR) 无符号字节加载 -----
    case ARM64_INST_LDLARB:
    case ARM64_INST_LDARB:
    case ARM64_INST_LDAPRB:
    case ARM64_INST_LDAPURB:
    case ARM64_INST_LDURB_GPR:
    case ARM64_INST_LDTRB_GPR:
    case ARM64_INST_LDRB_GPR_POST_INDEX:
    case ARM64_INST_LDRB_GPR_PRE_INDEX:
    case ARM64_INST_LDRB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRB_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;

        if (decoded_result.operand_width != 32 || ptebp_emu_read_mem(address, 1, &raw)) goto emulate_failed;
        write_gpr_or_zr(regs, decoded_result.rt, (uint64_t)raw, false);
        switch (decoded_result.instruction)
        {
        case ARM64_INST_LDLARB:
        case ARM64_INST_LDARB:
        case ARM64_INST_LDAPRB:
        case ARM64_INST_LDAPURB:
            smp_mb();
            break;
        default:
            break;
        }
        break;
    }

    // ----- 通用寄存器 (GPR) 无符号半字加载 -----
    case ARM64_INST_LDLARH:
    case ARM64_INST_LDARH:
    case ARM64_INST_LDAPRH:
    case ARM64_INST_LDAPURH:
    case ARM64_INST_LDURH_GPR:
    case ARM64_INST_LDTRH_GPR:
    case ARM64_INST_LDRH_GPR_POST_INDEX:
    case ARM64_INST_LDRH_GPR_PRE_INDEX:
    case ARM64_INST_LDRH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRH_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;

        if (decoded_result.operand_width != 32 || ptebp_emu_read_mem(address, 2, &raw)) goto emulate_failed;
        write_gpr_or_zr(regs, decoded_result.rt, (uint64_t)raw, false);
        switch (decoded_result.instruction)
        {
        case ARM64_INST_LDLARH:
        case ARM64_INST_LDARH:
        case ARM64_INST_LDAPRH:
        case ARM64_INST_LDAPURH:
            smp_mb();
            break;
        default:
            break;
        }
        break;
    }

    // ----- 通用寄存器 (GPR) 无符号字/双字加载 -----
    case ARM64_INST_LDLAR:
    case ARM64_INST_LDAR:
    case ARM64_INST_LDAPR:
    case ARM64_INST_LDAPUR:
    case ARM64_INST_LDR_GPR_LITERAL:
    case ARM64_INST_LDUR_GPR:
    case ARM64_INST_LDTR_GPR:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_PRE_INDEX:
    case ARM64_INST_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;

        switch (decoded_result.operand_width)
        {
        case 32:
            if (ptebp_emu_read_mem(address, 4, &raw)) goto emulate_failed;
            write_gpr_or_zr(regs, decoded_result.rt, (uint64_t)raw, false);
            break;
        case 64:
            if (ptebp_emu_read_mem(address, 8, &raw)) goto emulate_failed;
            write_gpr_or_zr(regs, decoded_result.rt, (uint64_t)raw, true);
            break;
        default:
            goto emulate_failed;
        }
        switch (decoded_result.instruction)
        {
        case ARM64_INST_LDLAR:
        case ARM64_INST_LDAR:
        case ARM64_INST_LDAPR:
        case ARM64_INST_LDAPUR:
            smp_mb();
            break;
        default:
            break;
        }
        break;
    }

    // ----- 通用寄存器 (GPR) 有符号字节加载 -----
    case ARM64_INST_LDAPURSB:
    case ARM64_INST_LDURSB_GPR:
    case ARM64_INST_LDTRSB_GPR:
    case ARM64_INST_LDRSB_GPR_POST_INDEX:
    case ARM64_INST_LDRSB_GPR_PRE_INDEX:
    case ARM64_INST_LDRSB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSB_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;

        if (ptebp_emu_read_mem(address, 1, &raw)) goto emulate_failed;
        switch (decoded_result.operand_width)
        {
        case 32:
            write_gpr_or_zr(regs, decoded_result.rt, sign_extend64((uint64_t)raw, 7), false);
            break;
        case 64:
            write_gpr_or_zr(regs, decoded_result.rt, sign_extend64((uint64_t)raw, 7), true);
            break;
        default:
            goto emulate_failed;
        }
        if (decoded_result.instruction == ARM64_INST_LDAPURSB) smp_mb();
        break;
    }

    // ----- 通用寄存器 (GPR) 有符号半字加载 -----
    case ARM64_INST_LDAPURSH:
    case ARM64_INST_LDURSH_GPR:
    case ARM64_INST_LDTRSH_GPR:
    case ARM64_INST_LDRSH_GPR_POST_INDEX:
    case ARM64_INST_LDRSH_GPR_PRE_INDEX:
    case ARM64_INST_LDRSH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSH_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;

        if (ptebp_emu_read_mem(address, 2, &raw)) goto emulate_failed;
        switch (decoded_result.operand_width)
        {
        case 32:
            write_gpr_or_zr(regs, decoded_result.rt, sign_extend64((uint64_t)raw, 15), false);
            break;
        case 64:
            write_gpr_or_zr(regs, decoded_result.rt, sign_extend64((uint64_t)raw, 15), true);
            break;
        default:
            goto emulate_failed;
        }
        if (decoded_result.instruction == ARM64_INST_LDAPURSH) smp_mb();
        break;
    }

    // ----- 通用寄存器 (GPR) 有符号字加载 -----
    case ARM64_INST_LDAPURSW:
    case ARM64_INST_LDRSW_LITERAL:
    case ARM64_INST_LDURSW_GPR:
    case ARM64_INST_LDTRSW_GPR:
    case ARM64_INST_LDRSW_GPR_POST_INDEX:
    case ARM64_INST_LDRSW_GPR_PRE_INDEX:
    case ARM64_INST_LDRSW_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSW_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;

        if (decoded_result.operand_width != 64 || ptebp_emu_read_mem(address, 4, &raw)) goto emulate_failed;
        write_gpr_or_zr(regs, decoded_result.rt, sign_extend64((uint64_t)raw, 31), true);
        if (decoded_result.instruction == ARM64_INST_LDAPURSW) smp_mb();
        break;
    }

    // ----- 通用寄存器 (GPR) 成对字/双字加载 -----
    case ARM64_INST_LDNP_GPR:
    case ARM64_INST_LDP_GPR_OFFSET:
    case ARM64_INST_LDP_GPR_POST_INDEX:
    case ARM64_INST_LDP_GPR_PRE_INDEX:
    {
        __uint128_t raw0, raw1;

        switch (decoded_result.operand_width)
        {
        case 32:
            if (ptebp_emu_read_mem(address, 4, &raw0) || ptebp_emu_read_mem(address + 4, 4, &raw1)) goto emulate_failed;
            write_gpr_or_zr(regs, decoded_result.rt, (uint64_t)raw0, false);
            write_gpr_or_zr(regs, decoded_result.rt2, (uint64_t)raw1, false);
            break;
        case 64:
            if (ptebp_emu_read_mem(address, 8, &raw0) || ptebp_emu_read_mem(address + 8, 8, &raw1)) goto emulate_failed;
            write_gpr_or_zr(regs, decoded_result.rt, (uint64_t)raw0, true);
            write_gpr_or_zr(regs, decoded_result.rt2, (uint64_t)raw1, true);
            break;
        default:
            goto emulate_failed;
        }
        break;
    }

    // ----- 通用寄存器 (GPR) 成对有符号字加载 -----
    case ARM64_INST_LDPSW_OFFSET:
    case ARM64_INST_LDPSW_POST_INDEX:
    case ARM64_INST_LDPSW_PRE_INDEX:
    {
        __uint128_t raw0, raw1;

        if (decoded_result.operand_width != 64 || ptebp_emu_read_mem(address, 4, &raw0) || ptebp_emu_read_mem(address + 4, 4, &raw1)) goto emulate_failed;
        write_gpr_or_zr(regs, decoded_result.rt, sign_extend64((uint64_t)raw0, 31), true);
        write_gpr_or_zr(regs, decoded_result.rt2, sign_extend64((uint64_t)raw1, 31), true);
        break;
    }

    // ----- FP/SIMD 单寄存器加载 -----
    case ARM64_INST_LDR_FP_SIMD_LITERAL:
    case ARM64_INST_LDUR_FP_SIMD:
    case ARM64_INST_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDR_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDR_FP_SIMD_UNSIGNED_OFFSET:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));
        __uint128_t value;

        switch (decoded_result.operand_width)
        {
        case 8:
            if (ptebp_emu_read_mem(address, 1, &value)) goto emulate_failed;
            break;
        case 16:
            if (ptebp_emu_read_mem(address, 2, &value)) goto emulate_failed;
            break;
        case 32:
            if (ptebp_emu_read_mem(address, 4, &value)) goto emulate_failed;
            break;
        case 64:
            if (ptebp_emu_read_mem(address, 8, &value)) goto emulate_failed;
            break;
        case 128:
            if (ptebp_emu_read_mem(address, 16, &value)) goto emulate_failed;
            break;
        default:
            goto emulate_failed;
        }

        read_all_q_regs(&fp_regs);
        fp_regs.q[decoded_result.rt] = value;
        write_all_q_regs(&fp_regs);
        break;
    }

    // ----- FP/SIMD 成对加载 -----
    case ARM64_INST_LDNP_FP_SIMD:
    case ARM64_INST_LDP_FP_SIMD_OFFSET:
    case ARM64_INST_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDP_FP_SIMD_PRE_INDEX:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));
        __uint128_t value0, value1;

        switch (decoded_result.operand_width)
        {
        case 32:
            if (ptebp_emu_read_mem(address, 4, &value0) || ptebp_emu_read_mem(address + 4, 4, &value1)) goto emulate_failed;
            break;
        case 64:
            if (ptebp_emu_read_mem(address, 8, &value0) || ptebp_emu_read_mem(address + 8, 8, &value1)) goto emulate_failed;
            break;
        case 128:
            if (ptebp_emu_read_mem(address, 16, &value0) || ptebp_emu_read_mem(address + 16, 16, &value1)) goto emulate_failed;
            break;
        default:
            goto emulate_failed;
        }

        read_all_q_regs(&fp_regs);
        fp_regs.q[decoded_result.rt] = value0;
        fp_regs.q[decoded_result.rt2] = value1;
        write_all_q_regs(&fp_regs);
        break;
    }

    default:
        goto emulate_failed;
    }

    // 支持的 load 成功后统一提交 pre/post-index 基址写回并推进 PC。
    switch (decoded_result.instruction)
    {
    case ARM64_INST_LDRB_GPR_POST_INDEX:
    case ARM64_INST_LDRH_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDRSB_GPR_POST_INDEX:
    case ARM64_INST_LDRSH_GPR_POST_INDEX:
    case ARM64_INST_LDRSW_GPR_POST_INDEX:
    case ARM64_INST_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDP_GPR_POST_INDEX:
    case ARM64_INST_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPSW_POST_INDEX:
    case ARM64_INST_LDRB_GPR_PRE_INDEX:
    case ARM64_INST_LDRH_GPR_PRE_INDEX:
    case ARM64_INST_LDR_GPR_PRE_INDEX:
    case ARM64_INST_LDRSB_GPR_PRE_INDEX:
    case ARM64_INST_LDRSH_GPR_PRE_INDEX:
    case ARM64_INST_LDRSW_GPR_PRE_INDEX:
    case ARM64_INST_LDR_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDP_GPR_PRE_INDEX:
    case ARM64_INST_LDP_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDPSW_PRE_INDEX:
        write_gpr_or_sp(regs, decoded_result.rn, base + decoded_result.offset);
        break;
    default:
        break;
    }

    regs->pc = pc + 4;
    return EMU_INST_HANDLED;

emulate_failed:
    ls_log_always_tag("ptebp", "emulate skip pc=0x%llx inst=0x%08x\n", (unsigned long long)pc, raw_inst);
    return EMU_INST_SKIP;
}
/*
停止整组 PTEBP 监控：对 PTE 仍匹配 guard 的槽位尝试恢复原始指令和原始 PTE，
清空全局状态，并释放去重后的共享影子页。
lock_mm 为 true 时函数自行获取目标 mm 的 mmap 读锁；为 false 时供异常路径调用，
不额外获取该锁。
*/
static void ptebp_drop_all_monitors(bool lock_mm)
{
    void *shadow_pages[BP_CONFIG_MAX] = {NULL};
    size_t shadow_page_count = 0;
    struct mm_struct *mm;
    unsigned long flags;

    spin_lock_irqsave(&g_ptebp_lock, flags);

    // 无需撤销监控或已在停止流程中时直接退出。
    if (g_ptebp_stopping || !g_ptebp_mm) goto out_unlock;

    g_ptebp_stopping = true;
    mm = g_ptebp_mm;

    // 正常路径需睡眠，临时释放自旋锁获取 mmap 读锁。
    if (lock_mm)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        mmap_read_lock(mm);
        spin_lock_irqsave(&g_ptebp_lock, flags);
    }

    // 阶段一：逐槽位尝试恢复仍匹配 guard 的原始指令，避免恢复访问后保留 UDF。
    for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
    {
        if (!g_ptebp_slots[i].hook_addr) continue;

        if (g_ptebp_slots[i].shadow_page)
        {
            bool already_collected = false;

            for (size_t page_index = 0; page_index < shadow_page_count; page_index++)
                if (shadow_pages[page_index] == g_ptebp_slots[i].shadow_page)
                {
                    already_collected = true;
                    break;
                }

            if (!already_collected) shadow_pages[shadow_page_count++] = g_ptebp_slots[i].shadow_page;
        }

        if (ptebp_validate_guard_pte(mm, g_ptebp_slots[i].hook_addr, g_ptebp_slots[i].orig_pte)) (void)ptebp_restore_inst(&g_ptebp_slots[i]);
    }

    // 阶段二：恢复受管页面的原始 PTE
    for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
    {
        if (g_ptebp_slots[i].hook_addr && ptebp_validate_guard_pte(mm, g_ptebp_slots[i].hook_addr, g_ptebp_slots[i].orig_pte)) (void)write_user_pte_value(mm, g_ptebp_slots[i].page_vaddr, pte_val(g_ptebp_slots[i].orig_pte));
    }

    // 重置全部全局状态。
    g_ptebp_info = NULL;
    g_ptebp_mm = NULL;
    memset(g_ptebp_slots, 0, sizeof(g_ptebp_slots));
    g_ptebp_stopping = false;

    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    // 释放 mmap 读锁、共享影子页和 mm 引用。
    if (lock_mm) mmap_read_unlock(mm);
    for (size_t page_index = 0; page_index < shadow_page_count; page_index++) free_page((unsigned long)shadow_pages[page_index]);
    mmput(mm);
    ls_log_always_tag("ptebp", "stop complete source=%s shadow_pages=%zu\n", lock_mm ? "control" : "exception", shadow_page_count);
    return;

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
}

/*
do_mem_abort hook：只处理目标 mm 中命中受管页的 EL0/EL1 L3 权限异常。
读异常模拟支持的读类指令，写异常撤销监控后重试原指令。
*/
static int ptebp_handle_data_abort(struct pt_regs *hook_regs)
{
    const struct ptebp_slot *slot = NULL;
    struct pt_regs *regs;
    uint64_t far, esr, fault_page;
    uint32_t raw_inst;

    far = hook_regs->regs[0];
    esr = hook_regs->regs[1];
    regs = (struct pt_regs *)hook_regs->regs[2];

    // 快速过滤：非目标任务、正在退出或非 L3 权限故障，放行给原生异常处理
    if (!regs || !current->mm || (current->flags & PF_EXITING)) return 0;
    if ((esr & ESR_ELx_FSC) != (ESR_ELx_FSC_PERM | ESR_ELx_FSC_LEVEL)) return 0;

    fault_page = untagged_addr(far) & PAGE_MASK;

    // 确认当前进程属于监控目标。
    if (!g_ptebp_info || g_ptebp_mm != current->mm) return 0;

    // 匹配命中当前故障页的首个有效槽位
    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(g_ptebp_slots); slot_index++)
    {
        if (g_ptebp_slots[slot_index].hook_addr && g_ptebp_slots[slot_index].page_vaddr == fault_page)
        {
            slot = &g_ptebp_slots[slot_index];
            break;
        }
    }

    // 槽位不存在，说明非本模块接管的异常
    if (!slot) return 0;

    // 停止流程正在进行时不再模拟，保持原 PC 等待 PTE 恢复后重试。
    if (g_ptebp_stopping) goto abort_handled;

    // 受管页发生写权限异常时不修改影子页或真实页，先撤销整组监控，让原始 store 重试。
    if (esr & ESR_ELx_WNR) goto fallback_drop;

    // 获取故障指令：EL0 优先从受管代码页的影子页取指，否则读取其物理映射；EL1 直接取指。
    if (user_mode(regs))
    {
        uint64_t pc = untagged_addr(regs->pc);
        phys_addr_t paddr;
        void *shadow_page;

        if (!IS_ALIGNED(pc, sizeof(raw_inst)) || pc >= current->mm->task_size) goto fallback_drop;

        if (!g_ptebp_info || g_ptebp_mm != current->mm || g_ptebp_stopping) goto fallback_drop;

        shadow_page = (pc & PAGE_MASK) == slot->page_vaddr ? slot->shadow_page : NULL;
        if (!shadow_page) shadow_page = ptebp_find_shadow_page(pc & PAGE_MASK);
        if (shadow_page)
        {
            __builtin_memcpy(&raw_inst, (uint8_t *)shadow_page + (pc & ~((uint64_t)PAGE_MASK)), sizeof(raw_inst));
        }
        else
        {
            if (walk_translate_va_to_pa(current->mm, pc, &paddr) || linear_read_physical(paddr, &raw_inst, sizeof(raw_inst))) goto fallback_drop;
        }
    }
    else
    {
        raw_inst = READ_ONCE(*(const uint32_t *)(uintptr_t)regs->pc);
    }

    // 模拟读类指令；跨入无影子页等读取失败时撤销整组监控，原 PC 不变并由原始 load 重试。
    if (ptebp_emulate_load_store(regs, raw_inst) == EMU_INST_HANDLED)
    {
        goto abort_handled;
    }

fallback_drop:
    ptebp_drop_all_monitors(false);

abort_handled:
    hook_regs->regs[0] = 0;
    return 1;
}
// ======================== UDF 断点命中处理 ========================

// 处理目标 mm 中由本实现写入的 UDF #0，回调后模拟对应的原始指令。
static int ptebp_handle_undef_sync(struct pt_regs *hook_regs)
{
    struct fp_regs fp_regs __attribute__((__uninitialized__));
    struct bp_point *hit_point = NULL;
    struct pt_regs *regs;
    uint32_t emulate_inst_word;
    uint64_t pc;

    // UDF #0 进入 Unknown/Uncategorized；其他同步异常交给原生分发器。
    if (ESR_ELx_EC(read_sysreg(esr_el1)) != ESR_ELx_EC_UNKNOWN) return 0;

    // 两代同步入口都在 x0 传入真实用户 pt_regs。
    regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];
    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING)) return 0;

    pc = untagged_addr(regs->pc);

    // 确认目标 mm，按 PC 找到断点槽位并从影子页取出原始指令。
    if (!g_ptebp_info || g_ptebp_mm != current->mm) return 0;

    for (size_t point_slot = 0; point_slot < ARRAY_SIZE(g_ptebp_slots); point_slot++)
    {
        if (g_ptebp_slots[point_slot].hook_addr != pc) continue;
        if (ptebp_read_shadow_inst(&g_ptebp_slots[point_slot], &emulate_inst_word)) return 0;
        hit_point = &g_ptebp_info->points[point_slot];
        break;
    }

    if (!hit_point) return 0;
    if (g_ptebp_stopping) return 1;

    read_all_q_regs(&fp_regs);
    if (hit_point->on_hit) hit_point->on_hit(regs, &fp_regs, hit_point);

    // 回调未改写 PC 时模拟原始指令；模拟失败则撤销监控，使原生路径重试。
    if (regs->pc == pc && !emulate_inst(regs, &fp_regs, emulate_inst_word)) ptebp_drop_all_monitors(false);

    write_all_q_regs(&fp_regs);
    return 1;
}

// 拦截覆盖受管页的 mprotect：更新 VMA，受管页保留 guard PTE，普通页按新权限更新 PTE。
static int ptebp_handle_mprotect(struct pt_regs *hook_regs)
{
    static int (*fn_split_vma)(struct mm_struct *, struct vm_area_struct *, unsigned long, int) = NULL;
    static void (*fn_vma_set_page_prot)(struct vm_area_struct *) = NULL;
    struct pt_regs *sys_regs;
    struct vm_area_struct *vma;
    unsigned long start, len, end, prot, cursor;
    int status = 0;
    bool is_covered = false;

    if (!current->mm || (current->flags & PF_EXITING)) return 0;

    // __arm64_sys_mprotect 是全局 hook；先用当前进程 mm 做快速过滤，非目标进程立即放行。
    if (!g_ptebp_info || g_ptebp_mm != current->mm || g_ptebp_stopping) return 0;

    sys_regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];
    if (!user_mode(sys_regs)) return 0;

    // 1. 参数对齐与合法性校验
    start = untagged_addr(sys_regs->regs[0]);
    if (!IS_ALIGNED(start, PAGE_SIZE) || !sys_regs->regs[1]) return 0;

    len = PAGE_ALIGN(sys_regs->regs[1]);
    if (!len || (start + len < start)) return 0; // 溢出检查
    end = start + len;
    prot = sys_regs->regs[2];

    // 2. 检查 mprotect 范围是否触及受管页（未命中则直接放行）
    for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
    {
        if (g_ptebp_slots[i].hook_addr && g_ptebp_slots[i].page_vaddr >= start && g_ptebp_slots[i].page_vaddr < end)
        {
            is_covered = true;
            break;
        }
    }
    if (!is_covered) return 0;

    // 3. 获取非导出内核函数地址与 mm 锁
    if (!fn_split_vma)
    {
        fn_split_vma = (void *)generic_kallsyms_lookup_name("split_vma");
        if (!fn_split_vma)
        {
            hook_regs->regs[0] = -ENOSYS;
            return 1;
        }
    }

    if (!fn_vma_set_page_prot)
    {
        fn_vma_set_page_prot = (void *)generic_kallsyms_lookup_name("vma_set_page_prot");
        if (!fn_vma_set_page_prot)
        {
            hook_regs->regs[0] = -ENOSYS;
            return 1;
        }
    }

    if (mmap_write_lock_killable(current->mm))
    {
        hook_regs->regs[0] = -EINTR;
        return 1;
    }

    // hook 安装在全局 syscall 符号上；拿到 mmap 锁后再次确认目标 mm，避免停止或换目标期间误接管其他进程。
    if (!g_ptebp_info || g_ptebp_mm != current->mm || g_ptebp_stopping)
    {
        mmap_write_unlock(current->mm);
        return 0;
    }

    // 4. 核心循环：拆分 VMA、更新 VMA 权限，并按需更新 PTE
    cursor = start;
    while (cursor < end)
    {
        unsigned long seg_end, newflags;
        unsigned long eff_prot = prot;

        vma = find_vma(current->mm, cursor);
        if (!vma || vma->vm_start > cursor)
        {
            status = -ENOMEM;
            break;
        }

        // 起点不在 VMA 开头，向前拆分
        if (cursor != vma->vm_start)
        {
            if (fn_split_vma(current->mm, vma, cursor, 1))
            {
                status = -ENOMEM;
                break;
            }
            vma = find_vma(current->mm, cursor);
            if (!vma || vma->vm_start != cursor)
            {
                status = -ENOMEM;
                break;
            }
        }

        // 终点不在 VMA 末尾，向后拆分
        seg_end = min_t(unsigned long, vma->vm_end, end);
        if (seg_end != vma->vm_end)
        {
            if (fn_split_vma(current->mm, vma, seg_end, 0))
            {
                status = -ENOMEM;
                break;
            }
        }

        // 计算并写入 VMA 新标志位
        if ((current->personality & READ_IMPLIES_EXEC) && (eff_prot & PROT_READ) && (vma->vm_flags & VM_MAYEXEC)) eff_prot |= PROT_EXEC;

        newflags = calc_vm_prot_bits(eff_prot, -1) | (vma->vm_flags & ~(VM_READ | VM_WRITE | VM_EXEC | VM_FLAGS_CLEAR));
        if ((newflags & ~(newflags >> 4)) & VM_ACCESS_FLAGS)
        {
            status = -EACCES;
            break;
        }

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
        vm_flags_reset(vma, newflags);
#else
        WRITE_ONCE(vma->vm_flags, newflags);
#endif
        fn_vma_set_page_prot(vma);

        // 逐页检查：普通页修改 PTE，受监管的页跳过 PTE 修改
        for (unsigned long page = cursor; page < seg_end; page += PAGE_SIZE)
        {
            bool is_guarded = false;

            for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
            {
                if (g_ptebp_slots[i].hook_addr && g_ptebp_slots[i].page_vaddr == page)
                {
                    is_guarded = true;
                    break;
                }
            }

            // 遇到监管页直接跳过，保持当前的 Guard PTE 不变
            if (is_guarded) continue;

            // 普通页：正常更新 PTE 权限
            pte_t *ptep = get_user_pte(current->mm, page);
            if (ptep)
            {
                pte_t old_pte = READ_ONCE(*ptep);
                if (pte_present(old_pte))
                {
                    set_pte(ptep, pte_modify(old_pte, vma->vm_page_prot));
                    flush_tlb_addr_all_asid_all_cpus(page);
                }
            }
        }

        cursor = seg_end;
    }

    mmap_write_unlock(current->mm);
    hook_regs->regs[0] = status;
    return 1; // 无需再执行原系统调用
}

// 注册 PTEBP 的数据异常、UDF 断点和 mprotect hook。
static struct hook_entry g_ptebp_hooks[][1] = {
    {HOOK_ENTRY("do_mem_abort", ptebp_handle_data_abort)},
    {HOOK_ENTRY("el0t_64_sync_handler", ptebp_handle_undef_sync)},
    {HOOK_ENTRY("el0_sync_handler", ptebp_handle_undef_sync)},
    {HOOK_ENTRY("__arm64_sys_mprotect", ptebp_handle_mprotect)},
};

// ======================== 监控停止与安装 ========================

// 调用方持有控制锁时，撤销代码/PTE 状态并移除全部 hook。
static void ptebp_stop_locked(void)
{
    ptebp_drop_all_monitors(true);
    for (int hook_index = ARRAY_SIZE(g_ptebp_hooks) - 1; hook_index >= 0; hook_index--) inline_hook_remove(g_ptebp_hooks[hook_index]);
}

// 串行撤销仍由本模块持有的代码/PTE 状态和 hook。
static inline void stop_ptebp_monitor(void)
{
    mutex_lock(&g_ptebp_control_lock);
    ptebp_stop_locked();
    mutex_unlock(&g_ptebp_control_lock);
}

/*
校验执行断点配置，安装异常 hook，并为目标进程的每个受管页创建或复用内核影子页，
写入 UDF、设置 guard PTE；全部断点成功后才发布监控状态。
*/
static int start_ptebp_monitor(struct break_point *info)
{
    struct mm_struct *mm;
    unsigned long flags;
    int status;

    // 先校验配置，避免停止旧监控后才发现新配置无效。
    if (!bp_info_find_configured_type(info, BP_BREAKPOINT_X, NULL)) return -EINVAL;

    mutex_lock(&g_ptebp_control_lock);

    // 配置有效后停止已有监控实例。
    ptebp_stop_locked();

    // 安装底层 hook
    status = inline_hook_install(g_ptebp_hooks[0]);
    if (!status)
    {
        status = inline_hook_install(g_ptebp_hooks[1]);
        if (status) status = inline_hook_install(g_ptebp_hooks[2]);
        if (!status) status = inline_hook_install(g_ptebp_hooks[3]);
    }
    if (status) goto remove_hooks;

    // 获取目标进程 mm
    mm = get_mm_by_pid(info->tgid);
    if (!mm)
    {
        status = -EINVAL;
        goto remove_hooks;
    }

    // 在 mmap 读锁下安装各断点槽位。
    mmap_read_lock(mm);
    spin_lock_irqsave(&g_ptebp_lock, flags);

    g_ptebp_mm = mm;

    // 逐个安装配置中的执行断点，全部成功后再一次性发布监控状态。
    for (size_t i = 0; i < ARRAY_SIZE(info->points); i++)
    {
        if (!bp_point_is_configured_type(&info->points[i], BP_BREAKPOINT_X)) continue;

        size_t page_owner_slot = ARRAY_SIZE(g_ptebp_slots);
        void *shadow_page = NULL;
        uint64_t hook_addr = untagged_addr(info->points[i].hit_addr) & ~0x3ULL;
        uint64_t page_vaddr;
        uint32_t original_inst;
        uint32_t marker_inst = PTEBP_UDF_INST;
        pteval_t orig_value = 0;

        if (!hook_addr || hook_addr >= g_ptebp_mm->task_size)
        {
            status = -EFAULT;
            break;
        }
        page_vaddr = hook_addr & PAGE_MASK;

        for (size_t scan_slot = 0; scan_slot < ARRAY_SIZE(g_ptebp_slots); scan_slot++)
        {
            if (!g_ptebp_slots[scan_slot].hook_addr) continue;
            if (g_ptebp_slots[scan_slot].hook_addr == hook_addr)
            {
                status = -EEXIST;
                break;
            }
            if (g_ptebp_slots[scan_slot].page_vaddr == page_vaddr) page_owner_slot = scan_slot;
        }
        if (status) break;

        if (page_owner_slot < ARRAY_SIZE(g_ptebp_slots))
        {
            if (!ptebp_validate_guard_pte(g_ptebp_mm, g_ptebp_slots[page_owner_slot].hook_addr, g_ptebp_slots[page_owner_slot].orig_pte))
            {
                status = -EFAULT;
                break;
            }
            shadow_page = g_ptebp_slots[page_owner_slot].shadow_page;
            if (!shadow_page)
            {
                status = -ESTALE;
                break;
            }
        }
        else
        {
            status = read_user_pte_value(g_ptebp_mm, page_vaddr, &orig_value);
            if (status) break;
            if (orig_value & PTE_UXN)
            {
                status = -EACCES;
                break;
            }

            phys_addr_t paddr;
            status = walk_translate_va_to_pa(g_ptebp_mm, page_vaddr, &paddr);
            if (status) break;

            shadow_page = (void *)__get_free_page(GFP_ATOMIC | __GFP_ZERO);
            if (!shadow_page)
            {
                status = -ENOMEM;
                break;
            }

            status = linear_read_physical(paddr, shadow_page, PAGE_SIZE);
            if (status)
            {
                free_page((unsigned long)shadow_page);
                shadow_page = NULL;
                break;
            }
        }

        g_ptebp_slots[i] = (struct ptebp_slot){
            .orig_pte = page_owner_slot < ARRAY_SIZE(g_ptebp_slots) ? g_ptebp_slots[page_owner_slot].orig_pte : __pte(orig_value),
            .hook_addr = hook_addr,
            .page_vaddr = page_vaddr,
            .shadow_page = shadow_page,
        };

        status = ptebp_read_shadow_inst(&g_ptebp_slots[i], &original_inst);
        if (status) goto clear_slot;
        if (original_inst == PTEBP_UDF_INST)
        {
            status = -ESTALE;
            goto clear_slot;
        }

        status = ptebp_write_inst(&g_ptebp_slots[i], &marker_inst);
        if (status) goto restore_slot;
        if (page_owner_slot == ARRAY_SIZE(g_ptebp_slots))
        {
            status = write_user_pte_value(g_ptebp_mm, page_vaddr, ptebp_make_data_guard_pte(pte_val(g_ptebp_slots[i].orig_pte)));
            if (status) goto restore_slot;
        }
        continue;

    restore_slot:
        (void)ptebp_restore_inst(&g_ptebp_slots[i]);
    clear_slot:
        if (page_owner_slot == ARRAY_SIZE(g_ptebp_slots) && g_ptebp_slots[i].shadow_page) free_page((unsigned long)g_ptebp_slots[i].shadow_page);
        memset(&g_ptebp_slots[i], 0, sizeof(g_ptebp_slots[i]));
        break;
    }

    // 全部成功才正式发布对外可见的 info
    if (!status) g_ptebp_info = info;

    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    mmap_read_unlock(mm);

    // 若中途失败，统一通过 stop 彻底回滚已安装的资源。
    if (status) ptebp_stop_locked();

    mutex_unlock(&g_ptebp_control_lock);
    return status;

remove_hooks:
    ptebp_stop_locked();
    mutex_unlock(&g_ptebp_control_lock);
    return status;
}

#endif // ARM64_PTEDBG_H
