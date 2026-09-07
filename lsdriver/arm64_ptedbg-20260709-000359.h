#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H
/*
直接patch为brk,页不能访问伪造读+系统调用伪造

*/

#include <linux/atomic.h>
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uio.h>
#include <linux/wait.h>
#include <asm/esr.h>
#include <asm/memory.h>
#include <asm/ptrace.h>
#include <uapi/asm/unistd.h>

#include "arm64_encode/arm64_encode.h"
#include "arm64_reg.h"
#include "export_fun.h"
#include "inline_hook_frame.h"
#include "io_struct.h"
#include "arm64_emulate/emulate_inst.h"
#include "virtual_memory_rw.h"

#define PTEBP_BRK_MARKER_BASE 0xA500U

static inline uint8_t ptebp_instruction_bytes(const struct arm64_decoded_instruction *decoded)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_LDAPURSB: return 1;
    case ARM64_INST_LDAPURSH: return 2;
    case ARM64_INST_LDAPURSW: return 4;
    default: break;
    }
    return decoded->operand_width ? (uint8_t)(decoded->operand_width / 8) : 0;
}

static inline bool ptebp_gpr_single_leaf(uint32_t raw_inst)
{
    uint32_t owner = (raw_inst >> 24) & 0x3F;

    if (owner == 0x39) return true;
    if (owner != 0x38) return false;
    return !(raw_inst & 0x00200000U) || ((raw_inst >> 10) & 0x3) == 2;
}

struct ptebp_slot
{
    pte_t orig_pte;       // 页面安装数据保护前的原始 PTE，同页多个断点共享该快照。
    uint64_t hook_addr;   // 去除地址标签并按 4 字节对齐后的断点虚拟地址。
    uint64_t page_vaddr;  // hook_addr 所在页的页首虚拟地址。
    uint32_t orig_inst;   // 被 BRK 覆盖的原始指令，也是数据读取时返回的逻辑内容。
    uint32_t marker_inst; // 本槽位独占的 BRK immediate 标识，用于缓冲区内容匹配。
};

// 当前只允许存在一组 PTEBP 监控；配置、目标 mm 和槽位状态由同一把锁保护。
static struct break_point *g_ptebp_info;
static struct mm_struct *g_ptebp_mm;
static struct ptebp_slot g_ptebp_slots[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);
// 标记整组撤销已经开始，避免多个停止或异常回退路径重复执行恢复流程。
static bool g_ptebp_stopping;
static atomic_t g_ptebp_syscall_returns_inflight = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(g_ptebp_syscall_return_wait);

static bool ptebp_marker_slot_from_comment(uint32_t comment, size_t *slot_index)
{
    if (comment < PTEBP_BRK_MARKER_BASE || comment >= PTEBP_BRK_MARKER_BASE + ARRAY_SIZE(g_ptebp_slots)) return false;
    if (slot_index) *slot_index = comment - PTEBP_BRK_MARKER_BASE;
    return true;
}

static bool ptebp_marker_slot_from_inst(uint32_t inst, size_t *slot_index)
{
    if ((inst & 0xFFE0001FU) != 0xD4200000U) return false;
    return ptebp_marker_slot_from_comment((inst >> 5) & 0xFFFFU, slot_index);
}

/* ======================== PTE/PFN 基础操作 ======================== */

/*
构造受管页的数据保护 PTE。
不修改 UXN/PXN 等执行属性，只通过 AP、只读和 DBM 相关位撤销 EL0 数据访问。
这样 CPU 可以继续从原页面原生取指；普通用户态 load/store 会进入 DABT permission fault。
内核的 copy_from_user() 若或其他使用 LDTR/STTR 等非特权访存指令，也会按 EL0 权限检查并进入 DABT；
这里禁止的EL1非特权用户拷贝，EL1 的特权指令还是可以访问
*/
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

/*
手动遍历目标 mm 页表取得断点指令的物理地址，不依赖受管页当前的 EL0 数据权限。
写入后同步全部 CPU 的指令缓存并回读校验，确保 BRK 或原始指令已经真实落到代码页。
*/
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

// 调用方持有 g_ptebp_lock；源槽位失配会破坏 BRK 归属，因此拒绝继续模拟。
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
读取范围覆盖断点时，用对应 orig_inst 字节替换物理页中的 BRK 字节。
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

// DABT store 保持原始写语义；覆盖 marker 后该断点失效，重新安装监控即可恢复。
static int ptebp_emu_write_mem(uint64_t addr, int bytes, __uint128_t value)
{
    uint8_t data[sizeof(value)];
    struct ptebp_physical_chunk chunks[2];
    size_t chunk_count;
    unsigned long flags;
    int status;

    status = ptebp_emu_prepare_range(addr, bytes, &addr);
    if (status) return status;

    __builtin_memcpy(data, &value, bytes);

    spin_lock_irqsave(&g_ptebp_lock, flags);
    status = ptebp_emu_translate_range(addr, bytes, chunks, &chunk_count);
    if (status) goto out_unlock;

    for (size_t chunk_index = 0; chunk_index < chunk_count; chunk_index++)
    {
        status = linear_write_physical(chunks[chunk_index].paddr, data + chunks[chunk_index].data_offset, chunks[chunk_index].size);
        if (status) goto out_unlock;
    }
    for (size_t chunk_index = 0; chunk_index < chunk_count; chunk_index++)
    {
        status = arm64_sync_code_range_all_cpus(phys_to_virt(chunks[chunk_index].paddr), chunks[chunk_index].size);
        if (status) goto out_unlock;
    }
out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    return status;
}

/* ======================== 页数据访问异常命中处理 ======================== */

/*
guard PTE 主动撤销受管页的数据访问权限，因此这里处理的 DABT 并非原指令本身非法，
而是原本合法的 load/store 访问受管页时被监控机制主动中断。
为使 BRK 补丁和 guard PTE 保持安装状态、监控能够继续运行，必须模拟并提交整条原始指令，
包括完整数据读写、目标寄存器结果、基址写回和 PC 推进；跨页访问也按原指令的完整范围处理。

按 decoder 给出的架构语义提交一条普通 load/store：
- 成功时更新目标 GPR 或 FP/SIMD 寄存器、基址回写，并把 PC 推进 4 字节；
- pair store 合并为一次写入，避免第一元素成功、第二元素失败造成部分副作用；
- prefetch 没有架构可见的数据结果，直接推进 PC；
- exclusive、CAS/CASP、LSE RMW、SWP 等原子指令没有 case，返回 SKIP 触发整组回退。
*/
static enum emu_inst_result ptebp_emulate_load_store(struct pt_regs *regs, uint32_t raw_inst)
{
    struct arm64_decoded_instruction decoded_result __attribute__((__uninitialized__));
    const struct arm64_decoded_instruction *decoded = &decoded_result;
    uint64_t pc = regs->pc;
    uint64_t base, address;

    if (arm64_decode_instruction(raw_inst, &decoded_result) != ARM64_DECODE_OK) return EMU_INST_SKIP;

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

    if (ptebp_gpr_single_leaf(raw_inst))
    {
        uint32_t owner = (raw_inst >> 24) & 0x3F;
        uint32_t mode = (raw_inst >> 10) & 0x3;
        uint32_t opc = (raw_inst >> 22) & 0x3;
        uint8_t bytes = 1U << ((raw_inst >> 30) & 0x3);
        bool register_offset = owner == 0x38 && (raw_inst & 0x00200000U);
        __uint128_t value;

        base = addr_reg_read(regs, decoded->rn);
        if (register_offset)
        {
            uint64_t index = reg_read(regs, decoded->rm);

            switch (decoded->extend_type)
            {
            case 2: index = (uint32_t)index; break;
            case 3: break;
            case 6: index = (uint64_t)(int64_t)(int32_t)index; break;
            case 7: break;
            default: return EMU_INST_SKIP;
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
            if (ptebp_emu_write_mem(address, bytes, reg_read(regs, decoded->rt))) return EMU_INST_SKIP;
        }
        else
        {
            if (ptebp_emu_read_mem(address, bytes, &value)) return EMU_INST_SKIP;
            reg_write(regs, decoded->rt, opc >= 2 ? sign_extend64((uint64_t)value, bytes * 8 - 1) : (uint64_t)value, decoded->operand_width == 64);
        }

        if (owner == 0x38 && !register_offset && (mode == 1 || mode == 3)) addr_reg_write(regs, decoded->rn, base + decoded->offset);
        regs->pc = pc + 4;
        return EMU_INST_HANDLED;
    }

    // 根据指令形式直接解析目标内存虚拟地址
    switch (decoded->instruction)
    {
    case ARM64_INST_LDR_LITERAL_GPR:
    case ARM64_INST_LDRSW_LITERAL:
    case ARM64_INST_LDRS_LITERAL_FP_SIMD:
    case ARM64_INST_LDRD_LITERAL_FP_SIMD:
    case ARM64_INST_LDRQ_LITERAL_FP_SIMD:
    case ARM64_INST_PRFM_LITERAL:
        address = pc + decoded->offset;
        base = 0;
        break;
    case ARM64_INST_STR_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDR_SIGNED_GPR_POST_INDEX:
    case ARM64_INST_STRB_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRH_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRS_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRD_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRB_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRH_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRS_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRD_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_STP_GPR_POST_INDEX:
    case ARM64_INST_LDP_GPR_POST_INDEX:
    case ARM64_INST_STPS_FP_SIMD_POST_INDEX:
    case ARM64_INST_STPD_FP_SIMD_POST_INDEX:
    case ARM64_INST_STPQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPS_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPD_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPSW_POST_INDEX:
        base = addr_reg_read(regs, decoded->rn);
        address = base;
        break;
    case ARM64_INST_STR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_SIGNED_GPR_REGISTER_OFFSET:
    case ARM64_INST_STRB_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STRH_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STRS_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STRD_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STRQ_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRB_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRH_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRS_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRD_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRQ_FP_SIMD_REGISTER_OFFSET:
    {
        base = addr_reg_read(regs, decoded->rn);
        uint64_t index = reg_read(regs, decoded->rm);
        switch (decoded->extend_type)
        {
        case 2: // UXTW
            base = addr_reg_read(regs, decoded->rn);
            address = base + decoded->offset;
            break;
            break;
        case 3: // LSL / UXTX
            break;
        case 6: // SXTW
            index = (uint64_t)(int64_t)(int32_t)index;
            break;
        case 7: // SXTX
            break;
        default:
            return EMU_INST_SKIP;
        }
        address = base + (index << decoded->shift_amount);
        break;
    }
    default:
        return EMU_INST_SKIP;
    }

    // 分类执行数据读写（成功则 break 汇聚到尾部统一提交，失败则 return EMU_INST_SKIP）
    switch (decoded->instruction)
    {
    /* ----- 通用寄存器 (GPR) 单寄存器加载（含 Acquire） ----- */
    case ARM64_INST_LDLAR:
    case ARM64_INST_LDAR:
    case ARM64_INST_LDAPR:
    case ARM64_INST_LDAPUR:
    case ARM64_INST_LDR_LITERAL_GPR:
    case ARM64_INST_LDUR_GPR:
    case ARM64_INST_LDTR_GPR:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_PRE_INDEX:
    case ARM64_INST_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;
        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &raw)) return EMU_INST_SKIP;
        reg_write(regs, decoded->rt, (uint64_t)raw, decoded->operand_width == 64);
        if (decoded->instruction == ARM64_INST_LDAR || decoded->instruction == ARM64_INST_LDLAR || decoded->instruction == ARM64_INST_LDAPR || decoded->instruction == ARM64_INST_LDAPUR) smp_mb();
        break;
    }

    /* ----- 通用寄存器 (GPR) 有符号扩展加载 ----- */
    case ARM64_INST_LDAPUR_SIGNED:
    case ARM64_INST_LDRSW_LITERAL:
    case ARM64_INST_LDUR_SIGNED_GPR:
    case ARM64_INST_LDTR_SIGNED_GPR:
    case ARM64_INST_LDR_SIGNED_GPR_POST_INDEX:
    case ARM64_INST_LDR_SIGNED_GPR_PRE_INDEX:
    case ARM64_INST_LDR_SIGNED_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_SIGNED_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;
        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &raw)) return EMU_INST_SKIP;
        reg_write(regs, decoded->rt, sign_extend64((uint64_t)raw, ptebp_instruction_bytes(decoded) * 8 - 1), decoded->operand_width == 64);
        if (decoded->instruction == ARM64_INST_LDAPUR_SIGNED) smp_mb();
        break;
    }

    /* ----- 通用寄存器 (GPR) 单寄存器存储（含 Release） ----- */
    case ARM64_INST_STLLR:
    case ARM64_INST_STLR:
    case ARM64_INST_STLUR:
        smp_mb();
        fallthrough;
    case ARM64_INST_STUR_GPR:
    case ARM64_INST_STTR_GPR:
    case ARM64_INST_STR_GPR_POST_INDEX:
    case ARM64_INST_STR_GPR_PRE_INDEX:
    case ARM64_INST_STR_GPR_REGISTER_OFFSET:
    case ARM64_INST_STR_GPR_UNSIGNED_OFFSET:
        if (ptebp_emu_write_mem(address, ptebp_instruction_bytes(decoded), reg_read(regs, decoded->rt))) return EMU_INST_SKIP;
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
        bool is_signed = (decoded->instruction == ARM64_INST_LDPSW_OFFSET || decoded->instruction == ARM64_INST_LDPSW_POST_INDEX || decoded->instruction == ARM64_INST_LDPSW_PRE_INDEX);

        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &raw0) || ptebp_emu_read_mem(address + ptebp_instruction_bytes(decoded), ptebp_instruction_bytes(decoded), &raw1)) return EMU_INST_SKIP;

        if (is_signed)
        {
            reg_write(regs, decoded->rt, sign_extend64((uint64_t)raw0, 31), true);
            reg_write(regs, decoded->rt2, sign_extend64((uint64_t)raw1, 31), true);
        }
        else
        {
            reg_write(regs, decoded->rt, (uint64_t)raw0, decoded->operand_width == 64);
            reg_write(regs, decoded->rt2, (uint64_t)raw1, decoded->operand_width == 64);
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

        if (total_bytes > sizeof(pair)) return EMU_INST_SKIP;

        pair = (__uint128_t)(reg_read(regs, decoded->rt) & mask) | ((__uint128_t)(reg_read(regs, decoded->rt2) & mask) << (ptebp_instruction_bytes(decoded) * 8));

        if (ptebp_emu_write_mem(address, total_bytes, pair)) return EMU_INST_SKIP;
        break;
    }

    /* ----- FP/SIMD 单寄存器加载 ----- */
    case ARM64_INST_LDRS_LITERAL_FP_SIMD:
    case ARM64_INST_LDRD_LITERAL_FP_SIMD:
    case ARM64_INST_LDRQ_LITERAL_FP_SIMD:
    case ARM64_INST_LDURB_FP_SIMD:
    case ARM64_INST_LDURH_FP_SIMD:
    case ARM64_INST_LDURS_FP_SIMD:
    case ARM64_INST_LDURD_FP_SIMD:
    case ARM64_INST_LDURQ_FP_SIMD:
    case ARM64_INST_LDRB_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRH_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRS_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRD_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRB_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRH_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRS_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRD_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRQ_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRB_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRH_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRS_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRD_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRQ_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDRB_FP_SIMD_UNSIGNED_OFFSET:
    case ARM64_INST_LDRH_FP_SIMD_UNSIGNED_OFFSET:
    case ARM64_INST_LDRS_FP_SIMD_UNSIGNED_OFFSET:
    case ARM64_INST_LDRD_FP_SIMD_UNSIGNED_OFFSET:
    case ARM64_INST_LDRQ_FP_SIMD_UNSIGNED_OFFSET:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));
        __uint128_t value;

        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &value)) return EMU_INST_SKIP;

        read_all_q_regs(&fp_regs);
        fp_regs.q[decoded->rt] = value;
        write_all_q_regs(&fp_regs);
        break;
    }

    /* ----- FP/SIMD 单寄存器存储 ----- */
    case ARM64_INST_STURB_FP_SIMD:
    case ARM64_INST_STURH_FP_SIMD:
    case ARM64_INST_STURS_FP_SIMD:
    case ARM64_INST_STURD_FP_SIMD:
    case ARM64_INST_STURQ_FP_SIMD:
    case ARM64_INST_STRB_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRH_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRS_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRD_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRB_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STRH_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STRS_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STRD_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STRQ_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STRB_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STRH_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STRS_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STRD_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STRQ_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_STRB_FP_SIMD_UNSIGNED_OFFSET:
    case ARM64_INST_STRH_FP_SIMD_UNSIGNED_OFFSET:
    case ARM64_INST_STRS_FP_SIMD_UNSIGNED_OFFSET:
    case ARM64_INST_STRD_FP_SIMD_UNSIGNED_OFFSET:
    case ARM64_INST_STRQ_FP_SIMD_UNSIGNED_OFFSET:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));

        read_all_q_regs(&fp_regs);
        if (ptebp_emu_write_mem(address, ptebp_instruction_bytes(decoded), fp_regs.q[decoded->rt])) return EMU_INST_SKIP;
        break;
    }

    /* ----- FP/SIMD 成对加载 ----- */
    case ARM64_INST_LDNPS_FP_SIMD:
    case ARM64_INST_LDNPD_FP_SIMD:
    case ARM64_INST_LDNPQ_FP_SIMD:
    case ARM64_INST_LDPS_FP_SIMD_OFFSET:
    case ARM64_INST_LDPD_FP_SIMD_OFFSET:
    case ARM64_INST_LDPQ_FP_SIMD_OFFSET:
    case ARM64_INST_LDPS_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPD_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPS_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDPD_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDPQ_FP_SIMD_PRE_INDEX:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));
        __uint128_t value0, value1;
        uint8_t bytes = ptebp_instruction_bytes(decoded);

        if (ptebp_emu_read_mem(address, bytes, &value0) || ptebp_emu_read_mem(address + bytes, bytes, &value1)) return EMU_INST_SKIP;

        read_all_q_regs(&fp_regs);
        fp_regs.q[decoded->rt] = value0;
        fp_regs.q[decoded->rt2] = value1;
        write_all_q_regs(&fp_regs);
        break;
    }

    /* ----- FP/SIMD 成对存储 ----- */
    case ARM64_INST_STNPS_FP_SIMD:
    case ARM64_INST_STNPD_FP_SIMD:
    case ARM64_INST_STNPQ_FP_SIMD:
    case ARM64_INST_STPS_FP_SIMD_OFFSET:
    case ARM64_INST_STPD_FP_SIMD_OFFSET:
    case ARM64_INST_STPQ_FP_SIMD_OFFSET:
    case ARM64_INST_STPS_FP_SIMD_POST_INDEX:
    case ARM64_INST_STPD_FP_SIMD_POST_INDEX:
    case ARM64_INST_STPQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_STPS_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STPD_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STPQ_FP_SIMD_PRE_INDEX:
    {
        struct fp_regs fp_regs __attribute__((__uninitialized__));
        uint8_t bytes = ptebp_instruction_bytes(decoded);
        size_t total_bytes = bytes * 2;
        __uint128_t mask, pair;

        if (total_bytes > sizeof(pair)) return EMU_INST_SKIP;

        read_all_q_regs(&fp_regs);
        mask = (bytes == 16) ? ~(__uint128_t)0 : (((__uint128_t)1 << (bytes * 8)) - 1);
        pair = (fp_regs.q[decoded->rt] & mask) | ((fp_regs.q[decoded->rt2] & mask) << (bytes * 8));

        if (ptebp_emu_write_mem(address, total_bytes, pair)) return EMU_INST_SKIP;
        break;
    }

    default:
        return EMU_INST_SKIP;
    }

    // 所有成功的指令统一在此提交 pre/post-index 基址写回并推进 PC
    switch (decoded->instruction)
    {
    case ARM64_INST_STR_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDR_SIGNED_GPR_POST_INDEX:
    case ARM64_INST_STRB_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRH_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRS_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRD_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRB_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRH_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRS_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRD_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_STP_GPR_POST_INDEX:
    case ARM64_INST_LDP_GPR_POST_INDEX:
    case ARM64_INST_STPS_FP_SIMD_POST_INDEX:
    case ARM64_INST_STPD_FP_SIMD_POST_INDEX:
    case ARM64_INST_STPQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPS_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPD_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPQ_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDPSW_POST_INDEX:
    case ARM64_INST_STR_GPR_PRE_INDEX:
    case ARM64_INST_LDR_GPR_PRE_INDEX:
    case ARM64_INST_LDR_SIGNED_GPR_PRE_INDEX:
    case ARM64_INST_STRB_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STRH_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STRS_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STRD_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STRQ_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRB_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRH_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRS_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRD_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDRQ_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STP_GPR_PRE_INDEX:
    case ARM64_INST_LDP_GPR_PRE_INDEX:
    case ARM64_INST_STPS_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STPD_FP_SIMD_PRE_INDEX:
    case ARM64_INST_STPQ_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDPS_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDPD_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDPQ_FP_SIMD_PRE_INDEX:
    case ARM64_INST_LDPSW_PRE_INDEX:
        addr_reg_write(regs, decoded->rn, base + decoded->offset);
        break;
    default:
        break;
    }

    regs->pc = pc + 4;
    return EMU_INST_HANDLED;
}
//撤销整组 BRK/PTE 监控。
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

    // 阶段一：先恢复所有断点的原始指令（确保页面恢复访问前不再含 BRK）
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
/* ======================== BRK 命中处理 ======================== */

//brk_handler 的 hook 工作函数，只接管当前目标 mm 中由本实现写入的 BRK 地址。
static int ptebp_handle_brk(struct pt_regs *hook_regs)
{
    struct fp_regs fp_regs __attribute__((__uninitialized__));
    struct bp_point *hit_point = NULL;
    struct pt_regs *regs;
    uint32_t emulate_inst_word;
    uint64_t pc;
    size_t point_slot;
    unsigned long flags;

    if (!hook_regs) return 0;

    //  快速前置过滤
    regs = (struct pt_regs *)hook_regs->regs[2];
    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING)) return 0;

    if (!ptebp_marker_slot_from_comment(hook_regs->regs[1] & ESR_ELx_BRK64_ISS_COMMENT_MASK, &point_slot)) return 0;

    pc = untagged_addr(regs->pc) & ~0x3ULL;

    //  锁内校验目标 mm 与 PC，提取断点信息
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!ptebp_monitor_active_locked(current->mm))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        return 0; // 非当前受管实例，交由原生处理
    }

    if (g_ptebp_stopping)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        goto brk_handled;
    }

    if (g_ptebp_slots[point_slot].hook_addr == pc)
    {
        hit_point = &g_ptebp_info->points[point_slot];
        emulate_inst_word = g_ptebp_slots[point_slot].orig_inst;
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (!hit_point) return 0;

    // 执行命中回调与单步模拟（共享一套 FP/SIMD 现场）
    read_all_q_regs(&fp_regs);
    if (hit_point->on_hit) hit_point->on_hit(regs, &fp_regs, hit_point);

    // 若回调未改写 PC 则模拟执行原指令；模拟失败则撤销监控使原生重试
    if (regs->pc == pc && !emulate_inst(regs, &fp_regs, emulate_inst_word)) ptebp_drop_all_monitors(false);

    write_all_q_regs(&fp_regs);

brk_handled:
    hook_regs->regs[0] = 0;
    return 1;
}

/* ======================== syscall 输出逻辑视图 ======================== */

/*
13 种测试读取路径按数据真正经过的位置统一处理：
1. /proc/self/exe pread、process_vm_readv libc/direct、/proc/self/mem pread libc/direct、
    /proc/self/mem lseek+read：syscall 完整执行后，只扫描实际成功返回的输出缓冲区；
    遇到 BRK #0xA500+slot marker 就直接用该槽位 orig_inst 覆盖，返回值不变。
2. /proc/self/exe mmap：原生读取未修改的文件映射，作为磁盘基准，这里不接管。
3. memcpy、memmove、volatile u8/u32、AArch64 LDP：直接访问 guard 页，由 EL0 DABT 返回逻辑原指令。
4. pipe write/copy_from_user direct：内核读取用户页时由 EL1 DABT 返回逻辑原指令。

所有写入完全保持原生语义，不检查输入和目标，也不更新 orig_inst 或重新安装 marker；
写入覆盖 marker 后该断点自然失效，需要时由调用方卸载并重新安装监控。
*/

#define PTEBP_SYSCALL_MAX_IOV 1024UL

// do_el0_svc 返回跳板使用 inline hook 预留的 32 字节 metadata，不改变原 syscall 返回值。
struct ptebp_syscall_return_frame
{
    unsigned long return_addr;
    struct pt_regs *regs;
    void (*handler)(struct ptebp_syscall_return_frame *frame);
};

struct ptebp_patch_snapshot
{
    uint32_t marker_inst;
    uint32_t orig_inst;
};

struct ptebp_read_scanner
{
    struct ptebp_patch_snapshot snapshots[BP_CONFIG_MAX];
    void __user *carry_addr[sizeof(uint32_t) - 1];
    uint8_t carry[sizeof(uint32_t) - 1];
    size_t carry_size;
};

static bool ptebp_init_read_scanner(struct ptebp_read_scanner *scanner)
{
    unsigned long flags;

    memset(scanner, 0, sizeof(*scanner));

    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!ptebp_monitor_active_locked(current->mm)) goto out_unlock;
    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(g_ptebp_slots); slot_index++)
    {
        const struct ptebp_slot *slot = &g_ptebp_slots[slot_index];

        if (!slot->hook_addr) continue;
        scanner->snapshots[slot_index] = (struct ptebp_patch_snapshot){.marker_inst = slot->marker_inst, .orig_inst = slot->orig_inst};
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    return true;
out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    return false;
}

/*
扫描 syscall 已成功写入的用户输出片段，把专用 BRK marker 直接替换为对应槽位的 orig_inst。
scanner 保留逻辑流末尾 3 字节及其用户地址，因此 marker 即使未对齐、跨内部扫描块或跨 iovec
也能识别和原位替换；marker immediate 直接编码槽位号，不需要来源虚拟地址或文件偏移。
*/
static void ptebp_scan_read_chunk(struct ptebp_read_scanner *scanner, void __user *destination, size_t size)
{
    uint8_t scan_buffer[259];
    size_t consumed = 0;

    if (!destination || !size) return;

    while (consumed < size)
    {
        size_t chunk_size = min_t(size_t, 256, size - consumed);
        size_t old_carry_size = scanner->carry_size;
        size_t available = old_carry_size + chunk_size;
        size_t offset = 0;

        __builtin_memcpy(scan_buffer, scanner->carry, old_carry_size);
        if (copy_from_user_inatomic_nofault(scan_buffer + old_carry_size, (uint8_t __user *)destination + consumed, chunk_size)) return;
        while (offset + sizeof(uint32_t) <= available)
        {
            uint32_t inst;
            size_t slot_index;

            __builtin_memcpy(&inst, scan_buffer + offset, sizeof(inst));
            if (ptebp_marker_slot_from_inst(inst, &slot_index) && scanner->snapshots[slot_index].marker_inst == inst)
            {
                for (size_t byte_index = 0; byte_index < sizeof(uint32_t); byte_index++)
                {
                    size_t stream_index = offset + byte_index;
                    void __user *target = stream_index < old_carry_size ? scanner->carry_addr[stream_index] : (uint8_t __user *)destination + consumed + stream_index - old_carry_size;

                    (void)copy_to_user_inatomic_nofault(target, (uint8_t *)&scanner->snapshots[slot_index].orig_inst + byte_index, 1);
                }
                __builtin_memcpy(scan_buffer + offset, &scanner->snapshots[slot_index].orig_inst, sizeof(uint32_t));
                offset += sizeof(uint32_t);
            }
            else offset++;
        }

        scanner->carry_size = min_t(size_t, sizeof(uint32_t) - 1, available);
        for (size_t carry_index = 0; carry_index < scanner->carry_size; carry_index++)
        {
            size_t stream_index = available - scanner->carry_size + carry_index;

            scanner->carry[carry_index] = scan_buffer[stream_index];
            scanner->carry_addr[carry_index] = stream_index < old_carry_size ? scanner->carry_addr[stream_index] : (uint8_t __user *)destination + consumed + stream_index - old_carry_size;
        }
        consumed += chunk_size;
    }
}

// read/pread64 返回后，按 x0 的实际成功字节数扫描 x1 指向的用户输出缓冲区。
static void ptebp_return_read(struct ptebp_syscall_return_frame *frame)
{
    struct ptebp_read_scanner scanner;
    long result = (long)frame->regs->regs[0];

    if (result < (long)sizeof(uint32_t) || !ptebp_init_read_scanner(&scanner)) return;
    ptebp_scan_read_chunk(&scanner, (void __user *)(uintptr_t)frame->regs->regs[1], (size_t)result);
}

// process_vm_readv 返回后，复核 orig_x0 中的目标 PID，并按 x0 的成功字节数依次扫描 x1/x2 指定的 local iovec。
static void ptebp_return_process_vm_readv(struct ptebp_syscall_return_frame *frame)
{
    struct pt_regs *regs = frame->regs;
    const struct iovec __user *local_iov = (const struct iovec __user *)(uintptr_t)regs->regs[1];
    unsigned long local_count = regs->regs[2];
    struct ptebp_read_scanner scanner;
    struct iovec local = {0};
    size_t local_offset = 0;
    unsigned long local_index = 0;
    long result = (long)regs->regs[0];

    if (result <= 0 || (pid_t)regs->orig_x0 != current->tgid || !local_iov || !local_count || local_count > PTEBP_SYSCALL_MAX_IOV || !ptebp_init_read_scanner(&scanner)) return;
    size_t completed = (size_t)result;
    while (completed)
    {
        while (local_offset == local.iov_len)
        {
            if (local_index >= local_count || copy_from_user_inatomic_nofault(&local, &local_iov[local_index++], sizeof(local))) return;
            local_offset = 0;
        }

        size_t chunk = min_t(size_t, completed, local.iov_len - local_offset);
        void __user *local_addr = (uint8_t __user *)local.iov_base + local_offset;

        ptebp_scan_read_chunk(&scanner, local_addr, chunk);
        completed -= chunk;
        local_offset += chunk;
    }
}

static void __attribute__((used, __noinline__)) ptebp_handle_syscall_return(struct ptebp_syscall_return_frame *frame)
{
    frame->handler(frame);

    if (atomic_dec_and_test(&g_ptebp_syscall_returns_inflight)) wake_up_all(&g_ptebp_syscall_return_wait);
}

__attribute__((naked, used)) void ret_trampoline_ptebp_syscall(void)
{
    asm volatile("mov x0, sp\n"
                 "bl ptebp_handle_syscall_return\n"
                 "ldp x16, xzr, [sp], #304\n"
                 "ret x16\n");
}

// do_el0_svc 入口只改写内核函数返回 LR；原 syscall 完整执行，输出修补发生在返回跳板。
static int ptebp_syscall_entry_hook(struct pt_regs *hook_regs)
{
    struct ptebp_syscall_return_frame *frame;
    struct pt_regs *regs;
    unsigned long flags;
    long syscallno;

    // 返回帧必须能容纳在 inline hook 为当前调用预留的 metadata 区域中。
    BUILD_BUG_ON(sizeof(struct ptebp_syscall_return_frame) > HOOK_METADATA_BYTES);
    // do_el0_svc 的 hook 参数是合成的 hook 帧；其 x0 保存真实 syscall 的 pt_regs 地址。
    regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];
    frame = hook_frame_metadata(hook_regs);

    // AArch64 syscall 号位于 x8，只接管会把用户输出写回内存的读取类 syscall。
    syscallno = (long)regs->regs[8];
    switch (syscallno)
    {
    case __NR_read:
        frame->handler = ptebp_return_read;
        break;
    case __NR_pread64:
        frame->handler = ptebp_return_read;
        break;
    case __NR_process_vm_readv:
        // 只修补当前进程读回自己的结果，避免影响跨进程读取语义。
        if ((pid_t)regs->regs[0] != current->tgid) return 0;
        frame->handler = ptebp_return_process_vm_readv;
        break;
    default:
        return 0;
    }

    // stopping 置位后禁止新增返回回调；登记 inflight 后再放锁，确保 stop 等待期间不会漏计数。
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!ptebp_monitor_active_locked(current->mm) || g_ptebp_stopping)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        return 0;
    }
    atomic_inc(&g_ptebp_syscall_returns_inflight);
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    // 保存原返回地址和真实 syscall 上下文，供 syscall 完成后选择对应的输出修补逻辑。
    frame->return_addr = hook_regs->regs[30];
    frame->regs = regs;

    //把 inline hook 框架的 304 字节临时栈帧延长到整个 syscall 执行结束。
    hook_regs->sp = (unsigned long)frame;
    //替换 do_el0_svc() 的返回地址
    hook_regs->regs[30] = (unsigned long)ret_trampoline_ptebp_syscall;
    return 0;
}

// 统一接管数据权限异常、原地址 BRK 命中和 syscall 返回输出。
static struct hook_entry g_ptebp_hooks[] = {
    HOOK_ENTRY("do_mem_abort", ptebp_handle_data_abort),
    HOOK_ENTRY("brk_handler", ptebp_handle_brk),
    HOOK_ENTRY("do_el0_svc", ptebp_syscall_entry_hook),
};

/* ======================== 监控停止与安装 ======================== */

// 停止监控时先禁止新的 syscall 返回登记，排空旧返回并恢复代码/PTE，再移除整组 hook。
static inline void stop_ptebp_monitor(void)
{
    unsigned long flags;
    //禁止登记新的 syscall 返回处理
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (g_ptebp_mm && !g_ptebp_stopping) g_ptebp_stopping = true;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    //等待已经登记的 syscall 返回处理结束
    wait_event(g_ptebp_syscall_return_wait, atomic_read(&g_ptebp_syscall_returns_inflight) == 0);
    //恢复所有原始指令和原始 PTE
    ptebp_drop_all_monitors(true);
    //移除异常处理 hook
    inline_hook_remove(g_ptebp_hooks);
}

//安装一个原地址 BRK 槽位：任一步失败都会尽力恢复当前槽位的原始指令并清空软件状态。
static int ptebp_install_slot(struct break_point *info, size_t point_slot)
{
    struct ptebp_slot *slot = &g_ptebp_slots[point_slot];
    struct ptebp_slot *page_owner;
    uint64_t page_vaddr;
    uint64_t hook_addr;
    uint32_t marker_inst;
    pteval_t orig_value;
    int status;

    status = arm64_encode_brk(PTEBP_BRK_MARKER_BASE + point_slot, &marker_inst);
    if (status) return status;

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
        //这个原本的pte如果设置了UXN禁止执行,由于brk需要执行权限就说明不合适安装该断点
        if (orig_value & PTE_UXN) return -EACCES;
    }
    //初始化当前断点槽位的部分软件状态
    *slot = (struct ptebp_slot){
        //如果同页已经有断点，就复用该槽位保存的原始 PTE；如果同页没有断点，就使用刚从页表读取的 orig_value。
        .orig_pte = page_owner ? page_owner->orig_pte : __pte(orig_value),
        .hook_addr = hook_addr,
        .page_vaddr = page_vaddr,
        .marker_inst = marker_inst,
    };

    //初始化当前断点槽位的原始指令状态
    status = ptebp_access_inst(slot, &slot->orig_inst, false);
    if (status) goto clear_slot;

    //刚刚读取到的“原始指令”，是否已经是 PTEBP 自己保留的 BRK marker。
    if (ptebp_marker_slot_from_inst(slot->orig_inst, NULL))
    {
        status = -ESTALE;
        goto clear_slot;
    }

    // 同页只有第一个槽位需要修改 PTE，其余槽位只增加 BRK 补丁。
    status = ptebp_access_inst(slot, &slot->marker_inst, true);
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

// 校验配置、安装异常 hook，并为目标进程的全部执行断点安装 BRK/PTE 状态。
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
    status = inline_hook_install(g_ptebp_hooks);
    if (status) return status;

    // 获取目标进程 mm
    mm = get_mm_by_pid(info->tgid);
    if (!mm)
    {
        inline_hook_remove(g_ptebp_hooks);
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
