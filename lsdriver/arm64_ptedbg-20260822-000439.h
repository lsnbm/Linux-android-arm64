#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

/*
直接patch为udf,页不能访问伪造读返回
*/

#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
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

static inline uint8_t ptebp_instruction_bytes(const struct arm64_decoded_instruction *decoded)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_LDAPURSB: return 1;
    case ARM64_INST_LDAPURSH: return 2;
    case ARM64_INST_LDAPURSW: return 4;
    case ARM64_INST_LDRSW_LITERAL: return 4;
    default:
        return decoded->operand_width ? (uint8_t)(decoded->operand_width / 8) : 0;
    }
}

struct ptebp_slot
{
    pte_t orig_pte;
    uint64_t hook_addr;
    uint64_t page_vaddr;
    uint32_t orig_inst;
};

static struct break_point *g_ptebp_info;
static struct mm_struct *g_ptebp_mm;
static struct ptebp_slot g_ptebp_slots[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);
static bool g_ptebp_stopping;

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

static inline int ptebp_access_inst(struct ptebp_slot *slot, uint32_t *inst, bool write)
{
    phys_addr_t paddr;
    uint32_t readback;
    int status;

    if (!g_ptebp_mm) return -ESTALE;
    status = walk_translate_va_to_pa(g_ptebp_mm, slot->hook_addr, &paddr);
    if (status) return status;
    if (!write) return linear_read_physical(paddr, inst, sizeof(*inst));

    status = linear_write_physical(paddr, inst, sizeof(*inst));
    if (status) return status;
    status = arm64_sync_code_range_all_cpus(phys_to_virt(paddr), sizeof(*inst));
    if (status) return status;

    status = linear_read_physical(paddr, &readback, sizeof(readback));
    if (status) return status;
    return readback == *inst ? 0 : -EIO;
}

// 校验指定地址仍指向安装时的物理页，并保持预期的数据保护 PTE。
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

/* ======================== DABT 逻辑内存与访存模拟 ======================== */

struct ptebp_slot_overlap
{
    size_t data_offset;
    size_t inst_offset;
    size_t copy_size;
};

struct ptebp_physical_chunk
{
    phys_addr_t paddr;
    size_t data_offset;
    size_t size;
};

// 调用方持有 g_ptebp_lock；只判断当前监控是否归属于指定 mm，不包含 stopping 状态。
static bool ptebp_monitor_active_locked(struct mm_struct *mm)
{
    return g_ptebp_info && g_ptebp_mm == mm;
}

// 调用方持有 g_ptebp_lock；源槽位失配会破坏 UDF 归属，因此拒绝继续模拟。
static bool ptebp_all_slots_valid_locked(struct mm_struct *mm)
{
    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(g_ptebp_slots); slot_index++)
        if (g_ptebp_slots[slot_index].hook_addr && !ptebp_validate_guard_pte(mm, g_ptebp_slots[slot_index].hook_addr, g_ptebp_slots[slot_index].orig_pte)) return false;
    return true;
}

// 规范化模拟访问范围并拒绝非法宽度和地址回绕。
static int ptebp_emu_prepare_range(uint64_t raw_addr, int bytes, uint64_t *addr)
{
    if (bytes <= 0 || bytes > sizeof(__uint128_t)) return -EINVAL;

    *addr = untagged_addr(raw_addr);
    if (*addr > U64_MAX - (uint64_t)bytes) return -EFAULT;
    return 0;
}

// 将一次最多 16 字节的模拟访问预先翻译为页内物理片段，避免假设相邻虚拟页物理连续。
static int ptebp_emu_translate_range(uint64_t addr, size_t bytes, struct ptebp_physical_chunk chunks[2], size_t *chunk_count)
{
    size_t data_offset = 0;

    *chunk_count = 0;
    while (data_offset < bytes)
    {
        uint64_t current_addr = addr + data_offset;
        size_t chunk_size = min_t(size_t, bytes - data_offset, PAGE_SIZE - (current_addr & ~PAGE_MASK));
        int status;

        if (*chunk_count >= 2) return -E2BIG;
        status = walk_translate_va_to_pa(current->mm, current_addr, &chunks[*chunk_count].paddr);
        if (status) return status;

        chunks[*chunk_count].data_offset = data_offset;
        chunks[*chunk_count].size = chunk_size;
        (*chunk_count)++;
        data_offset += chunk_size;
    }
    return 0;
}

// 计算一次访问与槽位原始指令的字节交集，供逻辑读取和断点保持共用。
static bool ptebp_get_patch_overlap(uint64_t patch_addr, size_t patch_size, uint64_t addr, uint64_t end, struct ptebp_slot_overlap *overlap)
{
    uint64_t start;
    uint64_t stop;

    if (patch_addr > U64_MAX - patch_size) return false;
    start = max(addr, patch_addr);
    stop = min(end, patch_addr + patch_size);
    if (start >= stop) return false;

    overlap->data_offset = start - addr;
    overlap->inst_offset = start - patch_addr;
    overlap->copy_size = stop - start;
    return true;
}

// 读取 EL0 故障指令。PC 不必位于受管页，统一手动遍历目标 mm 页表后读取线性映射。
static int ptebp_read_user_inst(uint64_t raw_pc, uint32_t *inst)
{
    uint64_t pc = untagged_addr(raw_pc);
    phys_addr_t paddr;
    unsigned long flags;
    int status;

    if (!inst || !current->mm || !IS_ALIGNED(pc, sizeof(*inst)) || pc >= READ_ONCE(current->mm->task_size) || READ_ONCE(current->mm->task_size) - pc < sizeof(*inst)) return -EFAULT;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!ptebp_monitor_active_locked(current->mm) || g_ptebp_stopping || !ptebp_all_slots_valid_locked(current->mm))
    {
        status = -ESTALE;
        goto out_unlock;
    }

    status = walk_translate_va_to_pa(current->mm, pc, &paddr);
    if (status) goto out_unlock;
    status = linear_read_physical(paddr, inst, sizeof(*inst));
    if (status) goto out_unlock;

    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(g_ptebp_slots); slot_index++)
        if (g_ptebp_slots[slot_index].hook_addr == pc)
        {
            *inst = g_ptebp_slots[slot_index].orig_inst;
            break;
        }

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    return status;
}

/*
读取已经确认由本模块 guard PTE 触发 DABT 的数据。
访问跨页时逐页翻译并读取，不要求整个范围都位于受管页，也不假设虚拟相邻页物理连续；
读取范围覆盖断点时，用对应 orig_inst 字节补齐读取视图，确保返回目标进程最新写入内容。
*/
static int ptebp_emu_read_mem(uint64_t addr, int bytes, __uint128_t *out)
{
    uint8_t data[sizeof(__uint128_t)];
    struct ptebp_physical_chunk chunks[2];
    uint64_t end;
    __uint128_t value = 0;
    size_t chunk_count;
    unsigned long flags;
    int status;

    status = ptebp_emu_prepare_range(addr, bytes, &addr);
    if (status) return status;
    end = addr + (uint64_t)bytes;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    status = ptebp_emu_translate_range(addr, bytes, chunks, &chunk_count);
    if (status) goto out_unlock;

    for (size_t chunk_index = 0; chunk_index < chunk_count; chunk_index++)
    {
        status = linear_read_physical(chunks[chunk_index].paddr, data + chunks[chunk_index].data_offset, chunks[chunk_index].size);
        if (status) goto out_unlock;
    }

    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(g_ptebp_slots); slot_index++)
    {
        const struct ptebp_slot *slot = &g_ptebp_slots[slot_index];
        struct ptebp_slot_overlap overlap;

        if (!slot->hook_addr || !ptebp_get_patch_overlap(slot->hook_addr, sizeof(slot->orig_inst), addr, end, &overlap)) continue;
        __builtin_memcpy(data + overlap.data_offset, (uint8_t *)&slot->orig_inst + overlap.inst_offset, overlap.copy_size);
    }

    __builtin_memcpy(&value, data, bytes);
    *out = value;
out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    return status;
}

// DABT store 在整个访问范围内原样写入；覆盖断点时只额外同步槽位保存的逻辑指令。
static int ptebp_emu_write_mem(uint64_t addr, int bytes, __uint128_t value)
{
    uint8_t write_data[sizeof(__uint128_t)];
    uint32_t updated_orig_inst[BP_CONFIG_MAX];
    bool slot_updated[BP_CONFIG_MAX] = {false};
    struct ptebp_physical_chunk chunks[2];
    uint64_t end;
    size_t chunk_count;
    unsigned long flags;
    int status;

    status = ptebp_emu_prepare_range(addr, bytes, &addr);
    if (status) return status;
    end = addr + (uint64_t)bytes;
    __builtin_memcpy(write_data, &value, bytes);

    spin_lock_irqsave(&g_ptebp_lock, flags);
    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(g_ptebp_slots); slot_index++)
    {
        struct ptebp_slot *slot = &g_ptebp_slots[slot_index];
        struct ptebp_slot_overlap overlap;

        if (!slot->hook_addr || !ptebp_get_patch_overlap(slot->hook_addr, sizeof(slot->orig_inst), addr, end, &overlap)) continue;

        updated_orig_inst[slot_index] = slot->orig_inst;
        __builtin_memcpy((uint8_t *)&updated_orig_inst[slot_index] + overlap.inst_offset, write_data + overlap.data_offset, overlap.copy_size);
        slot_updated[slot_index] = true;
    }

    status = ptebp_emu_translate_range(addr, bytes, chunks, &chunk_count);
    if (status) goto out_unlock;

    for (size_t chunk_index = 0; chunk_index < chunk_count; chunk_index++)
    {
        status = linear_write_physical(chunks[chunk_index].paddr, write_data + chunks[chunk_index].data_offset, chunks[chunk_index].size);
        if (status) goto out_unlock;
    }
    for (size_t chunk_index = 0; chunk_index < chunk_count; chunk_index++)
    {
        status = arm64_sync_code_range_all_cpus(phys_to_virt(chunks[chunk_index].paddr), chunks[chunk_index].size);
        if (status) goto out_unlock;
    }

    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(g_ptebp_slots); slot_index++)
        if (slot_updated[slot_index]) g_ptebp_slots[slot_index].orig_inst = updated_orig_inst[slot_index];

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    return status;
}

/* ======================== 页数据访问异常命中处理 ======================== */

/*
guard PTE 主动撤销受管页的数据访问权限，因此这里处理的 DABT 并非原指令本身非法，
而是原本合法的 load/store 访问受管页时被监控机制主动中断。
为使 UDF 补丁和 guard PTE 保持安装状态、监控能够继续运行，必须模拟并提交整条原始指令，
包括完整数据读写、目标寄存器结果、基址写回和 PC 推进；跨页访问也按原指令的完整范围处理。

按 decoder 给出的架构语义提交一条普通 load/store：
- 成功时更新目标 GPR 或 FP/SIMD 寄存器、基址回写，并把 PC 推进 4 字节；
- pair store 合并为一次写入，避免第一元素成功、第二元素失败造成部分副作用；
- prefetch 没有架构可见的数据结果，直接推进 PC；
- IC IVAU 将 Xt 指定的用户 VA 翻译到物理别名后执行缓存同步；
- exclusive、CAS/CASP、LSE RMW、SWP 等原子指令没有 case，返回 SKIP 触发整组回退。
*/
static enum emu_inst_result ptebp_emulate_load_store(struct pt_regs *regs, uint32_t raw_inst)
{
    struct arm64_decoded_instruction decoded_result __attribute__((__uninitialized__));
    const struct arm64_decoded_instruction *decoded = &decoded_result;
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
    // Prefetch (PRFM) 无实际内存数据交互，直接推进 PC 并返回
    switch (decoded->instruction)
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

    uint32_t owner = (raw_inst >> 24) & 0x3F;

    if (owner == 0x39 || (owner == 0x38 && (!(raw_inst & 0x00200000U) || ((raw_inst >> 10) & 0x3) == 2)))
    {
        uint32_t mode = (raw_inst >> 10) & 0x3;
        uint32_t opc = (raw_inst >> 22) & 0x3;
        uint8_t bytes = 1U << ((raw_inst >> 30) & 0x3);
        bool register_offset = owner == 0x38 && (raw_inst & 0x00200000U);
        __uint128_t value;

        base = read_gpr_or_sp(regs, decoded->rn);
        if (register_offset)
        {
            uint64_t index = read_gpr_or_zr(regs, decoded->rm);

            switch (decoded->extend_type)
            {
            case 2: index = (uint32_t)index; break;
            case 3: break;
            case 6: index = (uint64_t)(int64_t)(int32_t)index; break;
            case 7: break;
            default: goto emulate_failed;
            }
            address = base + (index << decoded->shift_amount);
        }
        else
        {
            address = owner == 0x38 && mode == 1 ? base : base + decoded->offset;
        }

        ls_log_always_tag("ptebp", "emulate pc=0x%llx inst=0x%08x type=%d bytes=%u addr=0x%llx\n", (unsigned long long)pc, raw_inst, (int)decoded->instruction, (unsigned int)bytes, (unsigned long long)address);
        if (opc == 0)
        {
            if (ptebp_emu_write_mem(address, bytes, read_gpr_or_zr(regs, decoded->rt))) goto emulate_failed;
        }
        else
        {
            if (ptebp_emu_read_mem(address, bytes, &value)) goto emulate_failed;
            write_gpr_or_zr(regs, decoded->rt, opc >= 2 ? sign_extend64((uint64_t)value, bytes * 8 - 1) : (uint64_t)value, decoded->operand_width == 64);
        }

        if (owner == 0x38 && !register_offset && (mode == 1 || mode == 3)) write_gpr_or_sp(regs, decoded->rn, base + decoded->offset);
        regs->pc = pc + 4;
        return EMU_INST_HANDLED;
    }

    // 根据指令形式直接解析目标内存虚拟地址
    switch (decoded->instruction)
    {
    case ARM64_INST_LDR_GPR_LITERAL:
    case ARM64_INST_LDRSW_LITERAL:
    case ARM64_INST_LDR_FP_SIMD_LITERAL:
    case ARM64_INST_PRFM_LITERAL:
        address = pc + decoded->offset;
        base = 0;
        break;
    case ARM64_INST_STRB_GPR_POST_INDEX:
    case ARM64_INST_STRH_GPR_POST_INDEX:
    case ARM64_INST_STR_GPR_POST_INDEX:
    case ARM64_INST_LDRB_GPR_POST_INDEX:
    case ARM64_INST_LDRH_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDRSB_GPR_POST_INDEX:
    case ARM64_INST_LDRSH_GPR_POST_INDEX:
    case ARM64_INST_LDRSW_GPR_POST_INDEX:
    case ARM64_INST_STR_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INST_STP_GPR_POST_INDEX:
    case ARM64_INST_LDP_GPR_POST_INDEX:
    case ARM64_INST_STP_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPSW_POST_INDEX:
        base = read_gpr_or_sp(regs, decoded->rn);
        address = base;
        break;
    case ARM64_INST_STRB_GPR_REGISTER_OFFSET:
    case ARM64_INST_STRH_GPR_REGISTER_OFFSET:
    case ARM64_INST_STR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSW_GPR_REGISTER_OFFSET:
    case ARM64_INST_STR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET:
    {
        base = read_gpr_or_sp(regs, decoded->rn);
        uint64_t index = read_gpr_or_zr(regs, decoded->rm);
        switch (decoded->extend_type)
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
        address = base + (index << decoded->shift_amount);
        break;
    }
    default:
        base = read_gpr_or_sp(regs, decoded->rn);
        address = base + decoded->offset;
        break;
    }

    // 当前 PC 和原始指令在这里仍然成对可用；后续的读写模拟都由本次解码结果驱动。
    ls_log_always_tag("ptebp", "emulate pc=0x%llx inst=0x%08x type=%d bytes=%zu addr=0x%llx\n", (unsigned long long)pc, raw_inst, (int)decoded->instruction, (size_t)ptebp_instruction_bytes(decoded), (unsigned long long)address);

    // 分类执行数据读写（成功则 break 汇聚到尾部统一提交，失败则 return EMU_INST_SKIP）
    switch (decoded->instruction)
    {
    /* ----- 通用寄存器 (GPR) 单寄存器加载（含 Acquire） ----- */
    case ARM64_INST_LDLARB:
    case ARM64_INST_LDLARH:
    case ARM64_INST_LDLAR:
    case ARM64_INST_LDARB:
    case ARM64_INST_LDARH:
    case ARM64_INST_LDAR:
    case ARM64_INST_LDAPRB:
    case ARM64_INST_LDAPRH:
    case ARM64_INST_LDAPR:
    case ARM64_INST_LDAPURB:
    case ARM64_INST_LDAPURH:
    case ARM64_INST_LDAPUR:
    case ARM64_INST_LDR_GPR_LITERAL:
    case ARM64_INST_LDURB_GPR:
    case ARM64_INST_LDURH_GPR:
    case ARM64_INST_LDUR_GPR:
    case ARM64_INST_LDTRB_GPR:
    case ARM64_INST_LDTRH_GPR:
    case ARM64_INST_LDTR_GPR:
    case ARM64_INST_LDRB_GPR_POST_INDEX:
    case ARM64_INST_LDRH_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDRB_GPR_PRE_INDEX:
    case ARM64_INST_LDRH_GPR_PRE_INDEX:
    case ARM64_INST_LDR_GPR_PRE_INDEX:
    case ARM64_INST_LDRB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRB_GPR_UNSIGNED_OFFSET:
    case ARM64_INST_LDRH_GPR_UNSIGNED_OFFSET:
    case ARM64_INST_LDR_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;
        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &raw)) goto emulate_failed;
        write_gpr_or_zr(regs, decoded->rt, (uint64_t)raw, decoded->operand_width == 64);
        switch (decoded->instruction)
        {
        case ARM64_INST_LDLARB:
        case ARM64_INST_LDLARH:
        case ARM64_INST_LDLAR:
        case ARM64_INST_LDARB:
        case ARM64_INST_LDARH:
        case ARM64_INST_LDAR:
        case ARM64_INST_LDAPRB:
        case ARM64_INST_LDAPRH:
        case ARM64_INST_LDAPR:
        case ARM64_INST_LDAPURB:
        case ARM64_INST_LDAPURH:
        case ARM64_INST_LDAPUR:
            smp_mb();
            break;
        default:
            break;
        }
        break;
    }

    /* ----- 通用寄存器 (GPR) 有符号扩展加载 ----- */
    case ARM64_INST_LDAPURSB:
    case ARM64_INST_LDAPURSH:
    case ARM64_INST_LDAPURSW:
    case ARM64_INST_LDRSW_LITERAL:
    case ARM64_INST_LDURSB_GPR:
    case ARM64_INST_LDURSH_GPR:
    case ARM64_INST_LDURSW_GPR:
    case ARM64_INST_LDTRSB_GPR:
    case ARM64_INST_LDTRSH_GPR:
    case ARM64_INST_LDTRSW_GPR:
    case ARM64_INST_LDRSB_GPR_POST_INDEX:
    case ARM64_INST_LDRSH_GPR_POST_INDEX:
    case ARM64_INST_LDRSW_GPR_POST_INDEX:
    case ARM64_INST_LDRSB_GPR_PRE_INDEX:
    case ARM64_INST_LDRSH_GPR_PRE_INDEX:
    case ARM64_INST_LDRSW_GPR_PRE_INDEX:
    case ARM64_INST_LDRSB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSW_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSB_GPR_UNSIGNED_OFFSET:
    case ARM64_INST_LDRSH_GPR_UNSIGNED_OFFSET:
    case ARM64_INST_LDRSW_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;
        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &raw)) goto emulate_failed;
        write_gpr_or_zr(regs, decoded->rt, sign_extend64((uint64_t)raw, ptebp_instruction_bytes(decoded) * 8 - 1), decoded->operand_width == 64);
        switch (decoded->instruction)
        {
        case ARM64_INST_LDAPURSB:
        case ARM64_INST_LDAPURSH:
        case ARM64_INST_LDAPURSW:
            smp_mb();
            break;
        default:
            break;
        }
        break;
    }

    /* ----- 通用寄存器 (GPR) 单寄存器存储（含 Release） ----- */
    case ARM64_INST_STLLRB:
    case ARM64_INST_STLLRH:
    case ARM64_INST_STLLR:
    case ARM64_INST_STLRB:
    case ARM64_INST_STLRH:
    case ARM64_INST_STLR:
    case ARM64_INST_STLURB:
    case ARM64_INST_STLURH:
    case ARM64_INST_STLUR:
        smp_mb();
        fallthrough;
    case ARM64_INST_STURB_GPR:
    case ARM64_INST_STURH_GPR:
    case ARM64_INST_STUR_GPR:
    case ARM64_INST_STTRB_GPR:
    case ARM64_INST_STTRH_GPR:
    case ARM64_INST_STTR_GPR:
    case ARM64_INST_STRB_GPR_POST_INDEX:
    case ARM64_INST_STRH_GPR_POST_INDEX:
    case ARM64_INST_STR_GPR_POST_INDEX:
    case ARM64_INST_STRB_GPR_PRE_INDEX:
    case ARM64_INST_STRH_GPR_PRE_INDEX:
    case ARM64_INST_STR_GPR_PRE_INDEX:
    case ARM64_INST_STRB_GPR_REGISTER_OFFSET:
    case ARM64_INST_STRH_GPR_REGISTER_OFFSET:
    case ARM64_INST_STR_GPR_REGISTER_OFFSET:
    case ARM64_INST_STRB_GPR_UNSIGNED_OFFSET:
    case ARM64_INST_STRH_GPR_UNSIGNED_OFFSET:
    case ARM64_INST_STR_GPR_UNSIGNED_OFFSET:
        if (ptebp_emu_write_mem(address, ptebp_instruction_bytes(decoded), read_gpr_or_zr(regs, decoded->rt))) goto emulate_failed;
        break;

    /* ----- 通用寄存器 (GPR) 成对加载 ----- */
    case ARM64_INST_LDNP_GPR:
    case ARM64_INST_LDP_GPR_OFFSET:
    case ARM64_INST_LDP_GPR_POST_INDEX:
    case ARM64_INST_LDP_GPR_PRE_INDEX:
    case ARM64_INST_LDPSW_OFFSET:
    case ARM64_INST_LDPSW_POST_INDEX:
    case ARM64_INST_LDPSW_PRE_INDEX:
    {
        __uint128_t raw0, raw1;
        bool is_signed = decoded->instruction == ARM64_INST_LDPSW_OFFSET || decoded->instruction == ARM64_INST_LDPSW_POST_INDEX || decoded->instruction == ARM64_INST_LDPSW_PRE_INDEX;

        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &raw0) || ptebp_emu_read_mem(address + ptebp_instruction_bytes(decoded), ptebp_instruction_bytes(decoded), &raw1)) goto emulate_failed;

        if (is_signed)
        {
            write_gpr_or_zr(regs, decoded->rt, sign_extend64((uint64_t)raw0, 31), true);
            write_gpr_or_zr(regs, decoded->rt2, sign_extend64((uint64_t)raw1, 31), true);
        }
        else
        {
            write_gpr_or_zr(regs, decoded->rt, (uint64_t)raw0, decoded->operand_width == 64);
            write_gpr_or_zr(regs, decoded->rt2, (uint64_t)raw1, decoded->operand_width == 64);
        }
        break;
    }

    /* ----- 通用寄存器 (GPR) 成对存储 ----- */
    case ARM64_INST_STNP_GPR:
    case ARM64_INST_STP_GPR_OFFSET:
    case ARM64_INST_STP_GPR_POST_INDEX:
    case ARM64_INST_STP_GPR_PRE_INDEX:
    {
        size_t total_bytes = ptebp_instruction_bytes(decoded) * 2;
        uint64_t mask = (ptebp_instruction_bytes(decoded) == sizeof(mask)) ? U64_MAX : (1ULL << (ptebp_instruction_bytes(decoded) * 8)) - 1;
        __uint128_t pair;

        if (total_bytes > sizeof(pair)) goto emulate_failed;

        pair = (__uint128_t)(read_gpr_or_zr(regs, decoded->rt) & mask) | ((__uint128_t)(read_gpr_or_zr(regs, decoded->rt2) & mask) << (ptebp_instruction_bytes(decoded) * 8));

        if (ptebp_emu_write_mem(address, total_bytes, pair)) goto emulate_failed;
        break;
    }

    /* ----- FP/SIMD 单寄存器加载 ----- */
    case ARM64_INST_LDR_FP_SIMD_LITERAL:
    case ARM64_INST_LDUR_FP_SIMD:
    case ARM64_INST_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDR_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDR_FP_SIMD_UNSIGNED_OFFSET:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));
        __uint128_t value;

        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &value)) goto emulate_failed;

        read_all_q_regs(&fp_regs);
        fp_regs.q[decoded->rt] = value;
        write_all_q_regs(&fp_regs);
        break;
    }

    /* ----- FP/SIMD 单寄存器存储 ----- */
    case ARM64_INST_STUR_FP_SIMD:
    case ARM64_INST_STR_FP_SIMD_POST_INDEX:
    case ARM64_INST_STR_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STR_FP_SIMD_UNSIGNED_OFFSET:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));

        read_all_q_regs(&fp_regs);
        if (ptebp_emu_write_mem(address, ptebp_instruction_bytes(decoded), fp_regs.q[decoded->rt])) goto emulate_failed;
        break;
    }

    /* ----- FP/SIMD 成对加载 ----- */
    case ARM64_INST_LDNP_FP_SIMD:
    case ARM64_INST_LDP_FP_SIMD_OFFSET:
    case ARM64_INST_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDP_FP_SIMD_PRE_INDEX:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));
        __uint128_t value0, value1;
        uint8_t bytes = ptebp_instruction_bytes(decoded);

        if (ptebp_emu_read_mem(address, bytes, &value0) || ptebp_emu_read_mem(address + bytes, bytes, &value1)) goto emulate_failed;

        read_all_q_regs(&fp_regs);
        fp_regs.q[decoded->rt] = value0;
        fp_regs.q[decoded->rt2] = value1;
        write_all_q_regs(&fp_regs);
        break;
    }

    /* ----- FP/SIMD 成对存储 ----- */
    case ARM64_INST_STNP_FP_SIMD:
    case ARM64_INST_STP_FP_SIMD_OFFSET:
    case ARM64_INST_STP_FP_SIMD_POST_INDEX:
    case ARM64_INST_STP_FP_SIMD_PRE_INDEX:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));
        uint8_t bytes = ptebp_instruction_bytes(decoded);
        size_t total_bytes = bytes * 2;
        __uint128_t mask, pair;

        if (total_bytes > sizeof(pair)) goto emulate_failed;

        read_all_q_regs(&fp_regs);
        mask = (bytes == 16) ? ~(__uint128_t)0 : (((__uint128_t)1 << (bytes * 8)) - 1);
        pair = (fp_regs.q[decoded->rt] & mask) | ((fp_regs.q[decoded->rt2] & mask) << (bytes * 8));

        if (ptebp_emu_write_mem(address, total_bytes, pair)) goto emulate_failed;
        break;
    }

    default:
        goto emulate_failed;
    }

    // 所有成功的指令统一在此提交 pre/post-index 基址写回并推进 PC
    switch (decoded->instruction)
    {
    case ARM64_INST_STRB_GPR_POST_INDEX:
    case ARM64_INST_STRH_GPR_POST_INDEX:
    case ARM64_INST_STR_GPR_POST_INDEX:
    case ARM64_INST_LDRB_GPR_POST_INDEX:
    case ARM64_INST_LDRH_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDRSB_GPR_POST_INDEX:
    case ARM64_INST_LDRSH_GPR_POST_INDEX:
    case ARM64_INST_LDRSW_GPR_POST_INDEX:
    case ARM64_INST_STR_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INST_STP_GPR_POST_INDEX:
    case ARM64_INST_LDP_GPR_POST_INDEX:
    case ARM64_INST_STP_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPSW_POST_INDEX:
    case ARM64_INST_STRB_GPR_PRE_INDEX:
    case ARM64_INST_STRH_GPR_PRE_INDEX:
    case ARM64_INST_STR_GPR_PRE_INDEX:
    case ARM64_INST_LDRB_GPR_PRE_INDEX:
    case ARM64_INST_LDRH_GPR_PRE_INDEX:
    case ARM64_INST_LDR_GPR_PRE_INDEX:
    case ARM64_INST_LDRSB_GPR_PRE_INDEX:
    case ARM64_INST_LDRSH_GPR_PRE_INDEX:
    case ARM64_INST_LDRSW_GPR_PRE_INDEX:
    case ARM64_INST_STR_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDR_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STP_GPR_PRE_INDEX:
    case ARM64_INST_LDP_GPR_PRE_INDEX:
    case ARM64_INST_STP_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDP_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDPSW_PRE_INDEX:
        write_gpr_or_sp(regs, decoded->rn, base + decoded->offset);
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
//撤销整组 UDF/PTE 监控。
static void ptebp_drop_all_monitors(bool lock_mm)
{
    struct mm_struct *mm;
    unsigned long flags;

    spin_lock_irqsave(&g_ptebp_lock, flags);

    //  无需撤销监控或已在停止流程中时直接退出
    if ((g_ptebp_stopping && !lock_mm) || !g_ptebp_mm) goto out_unlock;

    g_ptebp_stopping = true;
    mm = g_ptebp_mm;

    // 正常路径需睡眠，临时放锁以获取 mmap 读锁
    if (lock_mm)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        mmap_read_lock(mm);
        spin_lock_irqsave(&g_ptebp_lock, flags);
    }

    // 阶段一：先恢复所有断点的原始指令（确保页面恢复访问前不再含 UDF）
    for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
    {
        struct ptebp_slot *s = &g_ptebp_slots[i];
        if (s->hook_addr && ptebp_validate_guard_pte(mm, s->hook_addr, s->orig_pte)) (void)ptebp_access_inst(s, &s->orig_inst, true);
    }

    // 阶段二：恢复受管页面的原始 PTE
    for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
    {
        struct ptebp_slot *s = &g_ptebp_slots[i];
        if (s->hook_addr && ptebp_validate_guard_pte(mm, s->hook_addr, s->orig_pte)) (void)write_user_pte_value(mm, s->page_vaddr, pte_val(s->orig_pte));
    }

    //  在临界区内原子化重置全部全局状态
    g_ptebp_info = NULL;
    g_ptebp_mm = NULL;
    memset(g_ptebp_slots, 0, sizeof(g_ptebp_slots));
    g_ptebp_stopping = false; // 直接在此复位，避免在末尾重复加锁

    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    // 在锁外释放 mmap 读锁与 mm 引用
    if (lock_mm) mmap_read_unlock(mm);
    mmput(mm);
    return;

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
}

//do_mem_abort 的 hook 工作函数，只接管目标 mm 和受管页的 EL0/EL1 L3 数据权限异常
static int ptebp_handle_data_abort(struct pt_regs *hook_regs)
{
    const struct ptebp_slot *slot = NULL;
    struct pt_regs *regs;
    uint64_t far, esr, fault_page;
    uint32_t raw_inst;
    unsigned long flags;

    if (!hook_regs) return 0;

    far = hook_regs->regs[0];
    esr = hook_regs->regs[1];
    regs = (struct pt_regs *)hook_regs->regs[2];

    // 快速过滤：非目标任务、正在退出或非 L3 权限故障，放行给原生异常处理
    if (!regs || !current->mm || (current->flags & PF_EXITING)) return 0;
    if ((esr & ESR_ELx_FSC) != (ESR_ELx_FSC_PERM | ESR_ELx_FSC_LEVEL)) return 0;

    fault_page = untagged_addr(far) & PAGE_MASK;

    //  状态检查：确认受管页所有权与监控有效性
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!ptebp_monitor_active_locked(current->mm)) goto out_not_ours;

    // 匹配命中当前故障页的首个有效槽位
    for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
    {
        if (g_ptebp_slots[i].hook_addr && g_ptebp_slots[i].page_vaddr == fault_page)
        {
            slot = &g_ptebp_slots[i];
            break;
        }
    }

    // 槽位不存在或 guard PTE 校验失败，说明非本模块接管的异常
    if (!slot || !ptebp_validate_guard_pte(current->mm, slot->hook_addr, slot->orig_pte)) goto out_not_ours;

    // 正在停止时直接异常处理返回，等待停止线程恢复PTE后自动重试成功
    if (g_ptebp_stopping)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        goto abort_handled;
    }

    // 校验槽位完整性：若有槽位失效，放锁后统一回滚监控
    if (!ptebp_all_slots_valid_locked(current->mm))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        ptebp_drop_all_monitors(false);
        goto abort_handled;
    }

    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    // 读取故障指令（EL0 读用户原始指令，EL1的非特权指令进异常直接解引用取指，EL1的特权指令拦不住，需要新特性EPAN拦）
    if (user_mode(regs))
    {
        if (ptebp_read_user_inst(regs->pc, &raw_inst)) goto fallback_drop;
    }
    else
    {
        raw_inst = READ_ONCE(*(const uint32_t *)(uintptr_t)regs->pc);
    }

    //模拟指令
    if (ptebp_emulate_load_store(regs, raw_inst) == EMU_INST_HANDLED)
    {
        goto abort_handled;
    }

fallback_drop:
    ptebp_drop_all_monitors(false);

abort_handled:
    hook_regs->regs[0] = 0;
    return 1;

out_not_ours:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    return 0;
}
/* ======================== UDF 命中处理 ======================== */

// EL0 同步异常入口的 hook 工作函数，只接管当前目标 mm 中由本实现写入的 UDF #0 地址。
static int ptebp_handle_undef_sync(struct pt_regs *hook_regs)
{
    struct fp_regs fp_regs __attribute__((__uninitialized__));
    struct bp_point *hit_point = NULL;
    struct pt_regs *regs;
    uint32_t emulate_inst_word;
    uint64_t pc;
    unsigned long flags;
    bool stopping;

    if (!hook_regs) return 0;

    // UDF #0 进入 Unknown/Uncategorized；其他同步异常交给原生分发器。
    if (ESR_ELx_EC(read_sysreg(esr_el1)) != ESR_ELx_EC_UNKNOWN) return 0;

    // 两代同步入口都在 x0 传入真实用户 pt_regs。
    regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];
    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING)) return 0;

    pc = untagged_addr(regs->pc);

    // 锁内校验目标 mm 与 PC，提取断点信息。
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!ptebp_monitor_active_locked(current->mm))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        return 0;
    }

    for (size_t point_slot = 0; point_slot < ARRAY_SIZE(g_ptebp_slots); point_slot++)
    {
        if (g_ptebp_slots[point_slot].hook_addr != pc) continue;
        hit_point = &g_ptebp_info->points[point_slot];
        emulate_inst_word = g_ptebp_slots[point_slot].orig_inst;
        break;
    }
    stopping = g_ptebp_stopping;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (!hit_point) return 0;
    if (stopping) return 1;

    read_all_q_regs(&fp_regs);
    if (hit_point->on_hit) hit_point->on_hit(regs, &fp_regs, hit_point);

    // 若回调未改写 PC 则模拟执行原指令；模拟失败则撤销监控使原生重试
    if (regs->pc == pc && !emulate_inst(regs, &fp_regs, emulate_inst_word)) ptebp_drop_all_monitors(false);

    write_all_q_regs(&fp_regs);
    return 1;
}

// 覆盖当前 PTEBP data guard 时只更新 VMA；
static int ptebp_handle_mprotect(struct pt_regs *hook_regs)
{
    static int (*fn_split_vma)(struct mm_struct *, struct vm_area_struct *, unsigned long, int) = NULL;
    static void (*fn_vma_set_page_prot)(struct vm_area_struct *) = NULL;
    struct pt_regs *sys_regs;
    struct vm_area_struct *vma;
    unsigned long start, len, end, prot, cursor;
    unsigned long flags;
    int status = 0;
    bool is_covered = false;

    if (!hook_regs || !current->mm || (current->flags & PF_EXITING)) return 0;

    // __arm64_sys_mprotect 是全局 hook；先用当前进程 mm 做快速过滤，非目标进程立即放行。
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || g_ptebp_mm != current->mm || g_ptebp_stopping)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        return 0;
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    sys_regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];
    if (!sys_regs || !user_mode(sys_regs)) return 0;

    /* 1. 参数对齐与合法性校验 */
    start = untagged_addr(sys_regs->regs[0]);
    if (!IS_ALIGNED(start, PAGE_SIZE) || !sys_regs->regs[1]) return 0;

    len = PAGE_ALIGN(sys_regs->regs[1]);
    if (!len || (start + len < start)) return 0; // 溢出检查
    end = start + len;
    prot = sys_regs->regs[2];

    /* 2. 检查 mprotect 范围是否触及监管页 (未命中则直接放行交由原系统调用处理) */
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (g_ptebp_info && g_ptebp_mm == current->mm && !g_ptebp_stopping)
    {
        for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
        {
            unsigned long page = g_ptebp_slots[i].page_vaddr;
            if (g_ptebp_slots[i].hook_addr && page >= start && page < end)
            {
                is_covered = true;
                break;
            }
        }
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    if (!is_covered) return 0;

    /* 3. 获取非导出内核函数地址与 mm 锁 */
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
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || g_ptebp_mm != current->mm || g_ptebp_stopping)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        mmap_write_unlock(current->mm);
        return 0;
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    /* 4. 核心循环：拆分 VMA、更新 VMA 权限，并按需更新 PTE */
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

            spin_lock_irqsave(&g_ptebp_lock, flags);
            for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
            {
                if (g_ptebp_slots[i].hook_addr && g_ptebp_slots[i].page_vaddr == page)
                {
                    is_guarded = true;
                    break;
                }
            }
            spin_unlock_irqrestore(&g_ptebp_lock, flags);

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

// 统一接管数据权限异常、原地址 UDF 命中和 mprotect。
static struct hook_entry g_ptebp_hooks[][1] = {
    {HOOK_ENTRY("do_mem_abort", ptebp_handle_data_abort)},
    {HOOK_ENTRY("el0t_64_sync_handler", ptebp_handle_undef_sync)},
    {HOOK_ENTRY("el0_sync_handler", ptebp_handle_undef_sync)},
    {HOOK_ENTRY("__arm64_sys_mprotect", ptebp_handle_mprotect)},
};

static inline int ptebp_install_hooks(void)
{
    int status;

    status = inline_hook_install(g_ptebp_hooks[0]);
    if (status) return status;

    status = inline_hook_install(g_ptebp_hooks[1]);
    if (status) status = inline_hook_install(g_ptebp_hooks[2]);
    if (status) goto err_remove_hooks;

    status = inline_hook_install(g_ptebp_hooks[3]);
    if (!status) return 0;

err_remove_hooks:
    for (int hook_index = ARRAY_SIZE(g_ptebp_hooks) - 1; hook_index >= 0; hook_index--) inline_hook_remove(g_ptebp_hooks[hook_index]);
    return status;
}

/* ======================== 监控停止与安装 ======================== */

// 停止监控时恢复代码/PTE，再移除整组 hook。
static inline void stop_ptebp_monitor(void)
{
    //恢复所有原始指令和原始 PTE
    ptebp_drop_all_monitors(true);
    //移除异常处理 hook
    for (int hook_index = ARRAY_SIZE(g_ptebp_hooks) - 1; hook_index >= 0; hook_index--) inline_hook_remove(g_ptebp_hooks[hook_index]);
}

//安装一个原地址 UDF 槽位：任一步失败都会尽力恢复当前槽位的原始指令并清空软件状态。
static int ptebp_install_slot(struct break_point *info, size_t point_slot)
{
    struct ptebp_slot *slot = &g_ptebp_slots[point_slot];
    struct ptebp_slot *page_owner;
    uint64_t page_vaddr;
    uint64_t hook_addr;
    uint32_t marker_inst;
    pteval_t orig_value;
    int status;
    marker_inst = PTEBP_UDF_INST;

    //去除MTE/TBI 顶字节标签，并地址对齐到4字节边界
    hook_addr = untagged_addr(info->points[point_slot].hit_addr) & ~0x3ULL;
    if (!hook_addr || hook_addr >= g_ptebp_mm->task_size) return -EFAULT;

    //清除地址的页内偏移，因为结果从 0x12345 变成较小的 0x12000，所以叫向下对齐到页首。
    //向下对齐：0x12345 → 0x12000，取得当前页页首
    //向上对齐：0x12345 → 0x13000，取得下一页页首
    //已经对齐：0x12000 → 0x12000，保持不变
    page_vaddr = hook_addr & PAGE_MASK;

    // 扫描已安装槽位，找到当前要安装的断点所在页面，之前有没有安装过其他断点
    page_owner = NULL;
    for (size_t scan_slot = 0; scan_slot < ARRAY_SIZE(g_ptebp_slots); scan_slot++)
    {
        //hook地址不为真跳过
        if (!g_ptebp_slots[scan_slot].hook_addr) continue;
        //hook地址等于当前要hook的地址返回已经存在
        if (g_ptebp_slots[scan_slot].hook_addr == hook_addr) return -EEXIST;
        // 记录同页已安装槽位，后续复用其原始 PTE，避免重复修改该页 PTE。
        if (g_ptebp_slots[scan_slot].page_vaddr == page_vaddr) page_owner = &g_ptebp_slots[scan_slot];
    }

    //已经安装过了，说明改页面已经设置位受管页，并且已经有hook了
    if (page_owner)
    {
        //确认该页面当前仍然是本模块设置的 guard PTE，并且物理页没有被替换。
        if (!ptebp_validate_guard_pte(g_ptebp_mm, page_owner->hook_addr, page_owner->orig_pte)) return -EFAULT;
    }
    //没有安装过，说明没有设置为受管页
    else
    {
        //因此不能复用已有槽位的 orig_pte，需要从目标进程页表中读取这个页面当前的 PTE。
        status = read_user_pte_value(g_ptebp_mm, page_vaddr, &orig_value);
        if (status) return status;
        //这个原本的pte如果设置了UXN禁止执行,由于UDF需要执行权限就说明不合适安装该断点
        if (orig_value & PTE_UXN) return -EACCES;
    }
    //初始化当前断点槽位的部分软件状态
    *slot = (struct ptebp_slot){
        //如果同页已经有断点，就复用该槽位保存的原始 PTE；如果同页没有断点，就使用刚从页表读取的 orig_value。
        .orig_pte = page_owner ? page_owner->orig_pte : __pte(orig_value),
        .hook_addr = hook_addr,
        .page_vaddr = page_vaddr,
    };

    //初始化当前断点槽位的原始指令状态
    status = ptebp_access_inst(slot, &slot->orig_inst, false);
    if (status) goto clear_slot;

    // 原始指令已经是 UDF #0 时无法区分本模块 marker 与目标程序自身非法指令，因此拒绝安装。
    if (slot->orig_inst == PTEBP_UDF_INST)
    {
        status = -ESTALE;
        goto clear_slot;
    }

    // 同页只有第一个槽位需要修改 PTE，其余槽位只增加 UDF 补丁。
    status = ptebp_access_inst(slot, &marker_inst, true);
    if (status) goto clear_slot;

    //如果当前断点是该页面上的第一个断点，就把这个页面的原始 PTE 修改为 guard PTE。
    if (!page_owner)
    {
        status = write_user_pte_value(g_ptebp_mm, page_vaddr, ptebp_make_data_guard_pte(pte_val(slot->orig_pte)));
        if (status) goto err_restore_inst;
    }

    return 0;

err_restore_inst:
    (void)ptebp_access_inst(slot, &slot->orig_inst, true);
clear_slot:
    memset(slot, 0, sizeof(*slot));
    return status;
}

// 校验配置、安装异常 hook，并为目标进程的全部执行断点安装 UDF/PTE 状态。
static int start_ptebp_monitor(struct break_point *info)
{
    struct mm_struct *mm;
    unsigned long flags;
    int status;

    // 【防御性调整先校验入参，避免当前正在运行的监控清理了后，才发现当前配置不合法
    if (!bp_info_find_configured_type(info, BP_BREAKPOINT_X, NULL)) return -EINVAL;

    //  校验通过后，再撤销旧实例
    stop_ptebp_monitor();

    //  安装底层 hook
    status = ptebp_install_hooks();
    if (status) return status;

    // 获取目标进程 mm
    mm = get_mm_by_pid(info->tgid);
    if (!mm)
    {
        for (int hook_index = ARRAY_SIZE(g_ptebp_hooks) - 1; hook_index >= 0; hook_index--) inline_hook_remove(g_ptebp_hooks[hook_index]);
        return -EINVAL;
    }

    // 持锁安装各断点槽位
    mmap_read_lock(mm);
    spin_lock_irqsave(&g_ptebp_lock, flags);

    g_ptebp_mm = mm; // 在临界区内正式绑定 mm

    //把配置中的全部执行断点逐个安装，全部成功后再一次性发布为有效监控。
    for (size_t i = 0; i < ARRAY_SIZE(info->points); i++)
    {
        if (!bp_point_is_configured_type(&info->points[i], BP_BREAKPOINT_X)) continue;

        status = ptebp_install_slot(info, i);
        if (status) break;
    }

    // 全部成功才正式发布对外可见的 info
    if (!status) g_ptebp_info = info;

    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    mmap_read_unlock(mm);

    //  若中途失败，统一通过 stop 彻底回滚已安装的资源
    if (status) stop_ptebp_monitor();

    return status;
}

#endif // ARM64_PTEDBG_H
