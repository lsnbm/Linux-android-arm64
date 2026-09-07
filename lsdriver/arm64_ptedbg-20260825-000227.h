#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H
/*
直接patch为udf,MTE不可访问伪造数据读返回

*/
#include <linux/bitops.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/cpufeature.h>
#include <asm/esr.h>
#include <asm/memory.h>
#include <asm/mte-kasan.h>
#include <asm/ptrace.h>

#include "arm64_reg.h"
#include "export_fun.h"
#include "inline_hook_frame.h"
#include "io_struct.h"
#include "arm64_emulate/emulate_inst.h"
#include "virtual_memory_rw.h"

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

/*
PTEBP 同时使用两条互不替代的路径：
1. 执行断点：把目标地址的原始指令替换为 UDF #0，由同步异常入口捕获执行；
2. 数据断点：把目标地址所在 16-byte MTE granule 的 allocation tag 改成监控值，
   由同步 MTE tag-check fault 捕获对该 granule 的 load/store。

两条路径共享槽位中的原始指令和物理映射状态。UDF 路径模拟原始执行，MTE 路径模拟原始访存；
无法安全模拟时撤销整组监控，让处理器重新执行未修改的原始代码。
*/
struct ptebp_slot
{
    phys_addr_t inst_paddr; // 安装时 hook_addr 对应的物理指令地址，用于检测映射是否被替换。
    phys_addr_t tag_paddr;  // hook_addr 所在 16-byte MTE granule 的物理起始地址。
    uint64_t hook_addr;     // 去除地址标签并按 4 字节对齐后的断点虚拟地址。
    uint32_t orig_inst;     // 被 UDF #0 覆盖的原始指令，也是逻辑访存视图中应看到的指令字节。
    uint8_t orig_tag;       // 安装前 allocation tag，停止监控或安装失败时恢复。
    uint8_t monitor_tag;    // 监控期间写入该 granule 的 allocation tag，通常为 orig_tag ^ 1。
};

// 当前只允许存在一组 PTEBP 监控；配置、目标 mm 和槽位状态由同一把锁保护。
static struct break_point *g_ptebp_info;
static struct mm_struct *g_ptebp_mm;
static struct ptebp_slot g_ptebp_slots[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);
// 标记整组撤销已经开始；停止路径和异常回退路径可能并发进入，只允许一方恢复资源。
static bool g_ptebp_stopping;

/* ======================== UDF 与 MTE tag 基础操作 ======================== */

/*
通过目标 mm 页表把用户虚拟地址转换为物理地址，再使用内核线性映射访问。
这样不需要内核直接用用户 VA 访问受 MTE 监控的内存，也不依赖用户页当前是否允许 EL0 数据访问。
首次读取时记录 inst_paddr；后续访问必须仍得到相同物理地址，否则返回 -ESTALE，避免写入新映射。
写入指令后同步所有 CPU 的指令缓存并回读校验，确保 UDF #0 或原始指令真实落到代码页。
调用方通常持有 g_ptebp_lock；本函数不获取该锁。
*/
static inline int ptebp_access_inst(struct ptebp_slot *slot, uint32_t *inst, bool write)
{
    phys_addr_t paddr;
    uint32_t readback;
    int status;

    if (!g_ptebp_mm) return -ESTALE;
    status = walk_translate_va_to_pa(g_ptebp_mm, slot->hook_addr, &paddr);
    if (status) return status;
    if (slot->inst_paddr && slot->inst_paddr != paddr) return -ESTALE;
    if (!slot->inst_paddr) slot->inst_paddr = paddr;
    if (!write) return linear_read_physical(paddr, inst, sizeof(*inst));

    status = linear_write_physical(paddr, inst, sizeof(*inst));
    if (status) return status;
    status = arm64_sync_code_range_all_cpus(phys_to_virt(paddr), sizeof(*inst));
    if (status) return status;

    status = linear_read_physical(paddr, &readback, sizeof(readback));
    if (status) return status;
    return readback == *inst ? 0 : -EIO;
}

static inline uint8_t ptebp_read_allocation_tag(phys_addr_t tag_paddr)
{
    uint64_t tagged_addr = (uint64_t)(uintptr_t)phys_to_virt(tag_paddr);

    // LDG 从对应物理 granule 读取 allocation tag，不读取指针本身的逻辑 tag。
    asm(__MTE_PREAMBLE "ldg %0, [%0]" : "+r"(tagged_addr));
    return (tagged_addr & MTE_TAG_MASK) >> MTE_TAG_SHIFT;
}

static inline int ptebp_write_allocation_tag(phys_addr_t tag_paddr, uint8_t tag)
{
    uint64_t tagged_addr = (uint64_t)(uintptr_t)phys_to_virt(tag_paddr);

    // STG 使用指针高字节携带的新 tag 写入当前 16-byte granule；allocation tag 只保留 4 bit。
    tagged_addr = (tagged_addr & ~MTE_TAG_MASK) | ((uint64_t)(tag & 0xFU) << MTE_TAG_SHIFT);
    asm volatile(__MTE_PREAMBLE "stg %0, [%0]" : : "r"(tagged_addr) : "memory");
    dsb(ish);
    // 读回确认硬件接受了写入；失败时上层必须回滚已经改写的状态。
    return ptebp_read_allocation_tag(tag_paddr) == (tag & 0xFU) ? 0 : -EIO;
}

// 只验证 hook_addr 当前仍映射到安装时的物理指令地址，不检查 allocation tag。
static bool ptebp_slot_mapping_matches(struct mm_struct *mm, const struct ptebp_slot *slot)
{
    phys_addr_t paddr;

    return mm && slot->hook_addr && !walk_translate_va_to_pa(mm, slot->hook_addr, &paddr) && paddr == slot->inst_paddr;
}

// 先验证物理映射，再验证 monitor_tag；任一失配都禁止继续模拟，避免对错误页面产生副作用。
static bool ptebp_slot_tag_matches(struct mm_struct *mm, const struct ptebp_slot *slot)
{
    if (!ptebp_slot_mapping_matches(mm, slot)) return false;

    return ptebp_read_allocation_tag(slot->tag_paddr) == slot->monitor_tag;
}

/* ======================== MTE fault 逻辑内存与访存模拟 ======================== */

struct ptebp_slot_overlap
{
    // 数据访问与 UDF 四字节补丁重叠时，用这三个偏移定位需要覆盖的字节。
    size_t data_offset;
    size_t inst_offset;
    size_t copy_size;
};

struct ptebp_physical_chunk
{
    // 一次最多 16-byte 的访问最多拆成两个页内物理片段。
    phys_addr_t paddr;
    size_t data_offset;
    size_t size;
};

// 调用方持有 g_ptebp_lock；只判断当前监控是否归属于指定 mm，不包含 stopping 状态。
static bool ptebp_monitor_active_locked(struct mm_struct *mm)
{
    return g_ptebp_info && g_ptebp_mm == mm;
}

// 调用方持有 g_ptebp_lock；逐槽验证物理映射和 monitor_tag，供 fault 与逻辑取指共享。
static bool ptebp_all_slots_valid_locked(struct mm_struct *mm)
{
    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(g_ptebp_slots); slot_index++)
        if (g_ptebp_slots[slot_index].hook_addr && !ptebp_slot_tag_matches(mm, &g_ptebp_slots[slot_index])) return false;
    return true;
}

// 规范化模拟访问范围：去除用户指针 tag，限制访问宽度，并拒绝地址整数回绕。
static int ptebp_emu_prepare_range(uint64_t raw_addr, int bytes, uint64_t *addr)
{
    if (bytes <= 0 || bytes > sizeof(__uint128_t)) return -EINVAL;

    *addr = untagged_addr(raw_addr);
    if (*addr > U64_MAX - (uint64_t)bytes) return -EFAULT;
    return 0;
}

// 将一次最多 16 字节的模拟访问预先翻译为页内物理片段；相邻虚拟页不假设物理连续。
// 调用方持有 g_ptebp_lock。
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

// 计算一次数据访问与槽位原始指令四字节补丁的交集，供读视图和写后槽位更新共用。
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

/*
读取异常现场的原始指令。PC 不必位于触发 MTE fault 的 granule，也不一定对应某个槽位，
因此统一遍历当前 mm 页表后从物理别名读取。
如果 PC 正好是本模块写入 UDF 的地址，则把物理内存中的 marker 替换为槽位保存的 orig_inst，
让 load/store 模拟器看到程序原本要执行的指令。
*/
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
读取已经确认由本模块 MTE tag-check fault 触发的数据。
访问跨页时逐页翻译，不要求整个范围都位于受管 granule，也不假设虚拟相邻页物理连续；
读取范围覆盖 UDF 补丁时，用对应 orig_inst 字节覆盖物理内存中的 marker，
确保被模拟的 load 看到目标进程应有的最新指令内容。
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

/*
模拟 store 的原子提交：先计算所有被 UDF 覆盖槽位的新 orig_inst，
再写入完整访存范围，最后同步指令缓存并提交新的槽位状态。
这样一次跨越多个断点的 store 不会留下物理内存已更新而槽位逻辑视图过期的状态。
*/
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

/* ======================== MTE 数据访问异常命中处理 ======================== */

/*
MTE allocation tag 与访问指针的逻辑 tag 不匹配时，处理器在 load/store 处产生同步 MTE fault。
这里的 fault 不是原指令非法，而是监控机制主动中断了一条本来合法的数据访问。
为了让 UDF 执行补丁和 MTE 监控 tag 继续保持安装状态，必须模拟并提交整条原始指令，
包括完整数据读写、目标寄存器结果、基址写回和 PC 推进；跨页访问也按原指令的完整范围处理。

按 decoder 给出的架构语义提交一条普通 load/store：
- 成功时更新目标 GPR 或 FP/SIMD 寄存器、基址回写，并把 PC 推进 4 字节；
- pair load/store 按完整范围处理；store 合并为一次写入，避免第一元素成功、第二元素失败造成部分副作用；
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
        address = untagged_addr(reg_read(regs, raw_inst & 0x1FU));
        if (!current->mm || address >= READ_ONCE(current->mm->task_size)) goto emulate_failed;
        if (walk_translate_va_to_pa(current->mm, address, &paddr)) goto emulate_failed;

        // 使用同一物理位置的内核线性别名完成 D-cache 清理和 I-cache 失效，避免异常路径再次触发用户 tag check。
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
            if (ptebp_emu_write_mem(address, bytes, reg_read(regs, decoded->rt))) goto emulate_failed;
        }
        else
        {
            if (ptebp_emu_read_mem(address, bytes, &value)) goto emulate_failed;
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
        base = addr_reg_read(regs, decoded->rn);
        address = base + decoded->offset;
        break;
    }

    // 当前 PC 和原始指令在这里仍然成对可用；后续读写、寄存器写回和寻址更新都由 decoder 结果驱动。
    ls_log_always_tag("ptebp", "emulate pc=0x%llx inst=0x%08x type=%d bytes=%u addr=0x%llx\n", (unsigned long long)pc, raw_inst, (int)decoded->instruction, (unsigned int)ptebp_instruction_bytes(decoded), (unsigned long long)address);

    // 按指令类别执行数据读写；成功分支汇聚到尾部统一推进 PC，无法安全模拟则返回 SKIP。
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
        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &raw)) goto emulate_failed;
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
        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &raw)) goto emulate_failed;
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
        if (ptebp_emu_write_mem(address, ptebp_instruction_bytes(decoded), reg_read(regs, decoded->rt))) goto emulate_failed;
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

        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &raw0) || ptebp_emu_read_mem(address + ptebp_instruction_bytes(decoded), ptebp_instruction_bytes(decoded), &raw1)) goto emulate_failed;

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

        if (total_bytes > sizeof(pair)) goto emulate_failed;

        pair = (__uint128_t)(reg_read(regs, decoded->rt) & mask) | ((__uint128_t)(reg_read(regs, decoded->rt2) & mask) << (ptebp_instruction_bytes(decoded) * 8));

        if (ptebp_emu_write_mem(address, total_bytes, pair)) goto emulate_failed;
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

        if (ptebp_emu_read_mem(address, ptebp_instruction_bytes(decoded), &value)) goto emulate_failed;

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
        if (ptebp_emu_write_mem(address, ptebp_instruction_bytes(decoded), fp_regs.q[decoded->rt])) goto emulate_failed;
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

        if (ptebp_emu_read_mem(address, bytes, &value0) || ptebp_emu_read_mem(address + bytes, bytes, &value1)) goto emulate_failed;

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

    // 所有成功的指令统一在此提交 pre/post-index 基址写回并推进 PC。
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

emulate_failed:
    ls_log_always_tag("ptebp", "emulate skip pc=0x%llx inst=0x%08x\n", (unsigned long long)pc, raw_inst);
    return EMU_INST_SKIP;
}
/*
撤销整组 UDF/MTE 监控。
lock_mm=true 用于普通停止路径，此时函数自己获取 mmap 读锁；
lock_mm=false 用于异常回退路径，只执行当前上下文允许的恢复操作，不能等待 mmap 锁。
恢复顺序固定为“先恢复原始指令，再恢复 allocation tag，最后清空全局状态”，
避免其他 CPU 看到半 UDF、半监控的槽位组合。
*/
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

    // 阶段一：先恢复所有断点的原始指令，使代码页回到程序原貌。
    for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
    {
        struct ptebp_slot *s = &g_ptebp_slots[i];
        if (s->hook_addr && ptebp_slot_mapping_matches(mm, s)) (void)ptebp_access_inst(s, &s->orig_inst, true);
    }

    // 阶段二：同一 16-byte granule 只恢复一次原始 tag；只有仍为 monitor_tag 时才覆盖恢复，避免覆盖用户的新 tag。
    for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
    {
        struct ptebp_slot *s = &g_ptebp_slots[i];
        bool first_granule_slot = true;

        if (!s->hook_addr || !ptebp_slot_mapping_matches(mm, s)) continue;
        for (size_t previous = 0; previous < i; previous++)
            if (g_ptebp_slots[previous].hook_addr && g_ptebp_slots[previous].tag_paddr == s->tag_paddr)
            {
                first_granule_slot = false;
                break;
            }
        if (first_granule_slot && ptebp_read_allocation_tag(s->tag_paddr) == s->monitor_tag) (void)ptebp_write_allocation_tag(s->tag_paddr, s->orig_tag);
    }

    // 在临界区内原子化重置全部全局状态；从此刻起异常 hook 不再匹配旧槽位。
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

/*
do_mem_abort 的 hook 工作函数。inline hook 的 synthetic pt_regs 参数约定为：
hook_regs->regs[0] = far，hook_regs->regs[1] = esr，hook_regs->regs[2] = 真实异常 pt_regs。

这里只接管当前目标 mm 的同步 MTE fault，并按 FAR 所在 16-byte granule 查找槽位。
处理成功后把 hook_regs->regs[0] 置 0 并返回 1，表示 fault 已由模拟器消费；
未命中时返回 0，交给内核原生 MTE fault 处理。
*/
static int ptebp_handle_data_abort(struct pt_regs *hook_regs)
{
    const struct ptebp_slot *slot = NULL;
    struct pt_regs *regs;
    uint64_t far, esr, fault_granule;
    uint32_t raw_inst;
    unsigned long flags;

    far = hook_regs->regs[0];
    esr = hook_regs->regs[1];
    regs = (struct pt_regs *)hook_regs->regs[2];

    // 快速过滤：非目标任务、正在退出或非同步 MTE tag-check fault，放行给原生异常处理。
    if (!regs || !current->mm || (current->flags & PF_EXITING)) return 0;
    if ((esr & ESR_ELx_FSC) != ESR_ELx_FSC_MTE) return 0;

    fault_granule = untagged_addr(far) & MTE_GRANULE_MASK;

    // 状态检查：确认当前进程属于目标 mm，且槽位的物理映射和 monitor_tag 仍然有效。
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!ptebp_monitor_active_locked(current->mm)) goto out_not_ours;

    // 匹配命中当前故障 granule 的首个有效槽位。
    for (size_t i = 0; i < ARRAY_SIZE(g_ptebp_slots); i++)
    {
        if (g_ptebp_slots[i].hook_addr && (g_ptebp_slots[i].hook_addr & MTE_GRANULE_MASK) == fault_granule)
        {
            slot = &g_ptebp_slots[i];
            break;
        }
    }

    // 槽位不存在、映射已替换或 allocation tag 已变化时，不接管该异常。
    if (!slot || !ptebp_slot_tag_matches(current->mm, slot)) goto out_not_ours;

    // 正在停止时不再模拟；停止路径会恢复 tag，返回已处理让原始访问稍后重新执行。
    if (g_ptebp_stopping)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        goto abort_handled;
    }

    // 若任一槽位失效，放锁后统一回滚整组监控，避免只模拟当前访问而留下不一致状态。
    if (!ptebp_all_slots_valid_locked(current->mm))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        ptebp_drop_all_monitors(false);
        goto abort_handled;
    }

    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    // 读取触发 fault 的原始指令；用户态取指通过物理别名读取，并把本模块 UDF marker 还原为 orig_inst。
    if (user_mode(regs))
    {
        if (ptebp_read_user_inst(regs->pc, &raw_inst)) goto fallback_drop;
    }
    else
    {
        raw_inst = READ_ONCE(*(const uint32_t *)(uintptr_t)regs->pc);
    }

    // 模拟原始 load/store；成功时已经更新 regs 和 PC，失败时撤销整组监控并让原始路径重试。
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
/* ======================== UDF 执行断点命中处理 ======================== */

/*
EL0 64 位同步异常入口的统一 hook 工作函数，只接管本模块写入的 UDF #0。
两个候选同步入口都把真实用户 pt_regs 放在 hook_regs->regs[0]；
这里只按 ESR 的 Unknown/Uncategorized 类别和异常 PC 匹配槽位，其他同步异常返回 0。
命中后执行用户回调，再模拟 orig_inst；回调或模拟器改变 PC 时不额外推进 PC。
*/
static int ptebp_handle_undef_sync(struct pt_regs *hook_regs)
{
    struct fp_regs fp_regs __attribute__((__uninitialized__));
    struct bp_point *hit_point = NULL;
    struct pt_regs *regs;
    uint32_t emulate_inst_word;
    uint64_t pc;
    unsigned long flags;
    bool stopping;

    // UDF #0 进入 Unknown/Uncategorized；其他同步异常交给原生分发器。
    if (ESR_ELx_EC(read_sysreg(esr_el1)) != ESR_ELx_EC_UNKNOWN) return 0;

    // 两代同步入口都在 x0 传入真实用户 pt_regs。
    regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];
    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING)) return 0;

    pc = untagged_addr(regs->pc);

    // 锁内确认目标 mm 仍有活动监控，并按异常 PC 查找对应断点槽位。
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

    // 若回调未改写 PC 则模拟执行原指令；模拟失败则撤销监控使原生重试。
    if (regs->pc == pc && !emulate_inst(regs, &fp_regs, emulate_inst_word)) ptebp_drop_all_monitors(false);

    write_all_q_regs(&fp_regs);
    return 1;
}

/*
hook 表的安装语义：do_mem_abort 是 MTE 数据断点的必需入口，必须安装成功；
两个 el0 同步入口只兼容不同 Android 内核版本，成功安装其中一个即可。
每行保持单 entry 数组形状，以匹配 inline_hook_install/inline_hook_remove 的既有接口。
*/
static struct hook_entry g_ptebp_hooks[][1] = {
    {HOOK_ENTRY("do_mem_abort", ptebp_handle_data_abort)},
    {HOOK_ENTRY("el0t_64_sync_handler", ptebp_handle_undef_sync)},
    {HOOK_ENTRY("el0_sync_handler", ptebp_handle_undef_sync)},
};

static inline int ptebp_install_hooks(void)
{
    int status = -ENOENT;

    // 先安装必需的 do_mem_abort，再尝试两个同步入口候选；候选成功后跳过另一个。
    for (int hook_index = 0; hook_index < ARRAY_SIZE(g_ptebp_hooks); hook_index++)
    {
        // 同步入口是跨内核版本候选；任意一个成功后跳过另一个候选。
        if (hook_index >= 1 && (g_ptebp_hooks[1][0].installed || g_ptebp_hooks[2][0].installed)) continue;

        status = inline_hook_install(g_ptebp_hooks[hook_index]);
        if (status && hook_index < 1)
        {
            for (int remove_index = 0; remove_index <= hook_index; remove_index++) inline_hook_remove(g_ptebp_hooks[remove_index]);
            return status;
        }
    }

    if (!g_ptebp_hooks[1][0].installed && !g_ptebp_hooks[2][0].installed)
    {
        for (int remove_index = 0; remove_index < ARRAY_SIZE(g_ptebp_hooks); remove_index++) inline_hook_remove(g_ptebp_hooks[remove_index]);
        return status;
    }

    return 0;
}

static inline void stop_ptebp_monitor(void)
{
    // 先恢复 UDF 和 allocation tag，再按 installed 状态卸载实际成功的 hook。
    ptebp_drop_all_monitors(true);
    // 遍历这一张表逐行传入；hook 框架按 installed 状态卸载实际成功的条目。
    for (int hook_index = ARRAY_SIZE(g_ptebp_hooks) - 1; hook_index >= 0; hook_index--) inline_hook_remove(g_ptebp_hooks[hook_index]);
}

/*
安装流程为：规范化地址、确认物理映射、保存原始指令、设置 MTE tag、写入 UDF marker。
页面是否允许用户态取指不影响安装，UXN/PXN 页面也照常安装；这类页面仍可使用 MTE 监控读写。
任一步失败都清除当前槽位；如果已经改过 tag 或指令，则先恢复它们。
调用方持有 g_ptebp_lock 和目标 mm 的 mmap 读锁。
*/
static int ptebp_install_slot(struct break_point *info, size_t point_slot)
{
    struct ptebp_slot *slot = &g_ptebp_slots[point_slot];
    struct ptebp_slot *granule_owner = NULL;
    uint64_t hook_addr;
    uint32_t marker_inst;
    int status;
    marker_inst = PTEBP_UDF_INST;

    // 去除 MTE/TBI 顶字节标签，并按 A64 指令的 4-byte 对齐要求规范化地址。
    hook_addr = untagged_addr(info->points[point_slot].hit_addr) & ~0x3ULL;
    if (!hook_addr || hook_addr >= g_ptebp_mm->task_size) return -EFAULT;

    // 扫描已安装槽位；同一 16-byte granule 的多个断点共享 orig_tag/monitor_tag 状态。
    for (size_t scan_slot = 0; scan_slot < ARRAY_SIZE(g_ptebp_slots); scan_slot++)
    {
        if (!g_ptebp_slots[scan_slot].hook_addr) continue;
        if (g_ptebp_slots[scan_slot].hook_addr == hook_addr) return -EEXIST;
        if ((g_ptebp_slots[scan_slot].hook_addr & MTE_GRANULE_MASK) == (hook_addr & MTE_GRANULE_MASK)) granule_owner = &g_ptebp_slots[scan_slot];
    }

    *slot = (struct ptebp_slot){
        .hook_addr = hook_addr,
    };

    // 通过页表翻译确认地址存在并保存物理指令地址；不检查也不修改 UXN/PXN 等权限位。
    status = ptebp_access_inst(slot, &slot->orig_inst, false);
    if (status) goto clear_slot;

    // 原始指令已经是 UDF #0 时无法区分本模块 marker 与目标程序自身非法指令，因此拒绝安装。
    if (slot->orig_inst == PTEBP_UDF_INST)
    {
        status = -ESTALE;
        goto clear_slot;
    }

    // allocation tag 按 16-byte granule 保存；指令地址所在 granule 的起始物理地址作为 tag 操作地址。
    slot->tag_paddr = slot->inst_paddr & MTE_GRANULE_MASK;
    if (granule_owner)
    {
        // 同一 granule 内可能有多个 4-byte 执行断点，它们必须共享同一个 allocation tag 状态。
        // 如果物理 granule 或已有槽位的 monitor_tag 已变化，说明映射/监控状态失效，拒绝继续安装。
        if (granule_owner->tag_paddr != slot->tag_paddr || !ptebp_slot_tag_matches(g_ptebp_mm, granule_owner))
        {
            status = -ESTALE;
            goto clear_slot;
        }
        // 复用首个槽位保存的原始 tag 和监控 tag，停止时只由首个槽位负责恢复一次。
        slot->orig_tag = granule_owner->orig_tag;
        slot->monitor_tag = granule_owner->monitor_tag;
    }
    else
    {
        // 当前是该 granule 的首个槽位：保存用户原始 tag，并选择一个不同的 tag 作为监控值。
        slot->orig_tag = ptebp_read_allocation_tag(slot->tag_paddr);
        slot->monitor_tag = slot->orig_tag ^ 1U;
        // 改写 allocation tag 后，访问该 granule 的指针若仍带原 tag，就会进入同步 MTE fault。
        status = ptebp_write_allocation_tag(slot->tag_paddr, slot->monitor_tag);
        if (status) goto err_restore_tag;
    }

    // 每个槽位都写入 UDF 以保留执行断点；同一 granule 的 allocation tag 只由首个槽位设置。
    status = ptebp_access_inst(slot, &marker_inst, true);
    if (status) goto err_restore_tag;

    return 0;

err_restore_tag:
    if (!granule_owner) (void)ptebp_write_allocation_tag(slot->tag_paddr, slot->orig_tag);
    (void)ptebp_access_inst(slot, &slot->orig_inst, true);
clear_slot:
    memset(slot, 0, sizeof(*slot));
    return status;
}

//启动一组 PTEBP 监控。
static int start_ptebp_monitor(struct break_point *info)
{
    struct mm_struct *mm;
    unsigned long flags;
    int status;

    if (!system_supports_mte()) return -EOPNOTSUPP;

    // 先校验能力和入参，避免清理已有实例后才发现新配置无法启动。
    if (!bp_info_find_configured_type(info, BP_BREAKPOINT_X, NULL)) return -EINVAL;

    // 校验通过后，再撤销旧实例。
    stop_ptebp_monitor();

    // 安装 MTE fault 和 UDF 同步入口 hook。
    status = ptebp_install_hooks();
    if (status) return status;

    // 获取并持有目标进程 mm 引用，直到所有槽位清理完成。
    mm = get_mm_by_pid(info->tgid);
    if (!mm)
    {
        for (int hook_index = ARRAY_SIZE(g_ptebp_hooks) - 1; hook_index >= 0; hook_index--) inline_hook_remove(g_ptebp_hooks[hook_index]);
        return -EINVAL;
    }

    // mmap 读锁保护页表映射，g_ptebp_lock 保护槽位和发布状态。
    mmap_read_lock(mm);
    spin_lock_irqsave(&g_ptebp_lock, flags);

    g_ptebp_mm = mm; // 在临界区内正式绑定 mm

    // 逐个安装全部执行断点，全部成功后再一次性发布为有效监控。
    for (size_t i = 0; i < ARRAY_SIZE(info->points); i++)
    {
        if (!bp_point_is_configured_type(&info->points[i], BP_BREAKPOINT_X)) continue;

        status = ptebp_install_slot(info, i);
        if (status) break;
    }

    // 只有全部成功，fault/UDF hook 才能通过 g_ptebp_info 看到这组监控。
    if (!status) g_ptebp_info = info;

    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    mmap_read_unlock(mm);

    // 中途失败时统一回滚已安装的 UDF、allocation tag、mm 引用和 hook 状态。
    if (status) stop_ptebp_monitor();

    return status;
}

#endif // ARM64_PTEDBG_H
