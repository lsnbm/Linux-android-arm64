#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <asm/esr.h>
#include <asm/memory.h>
#include <asm/ptrace.h>

#include "arm64_encode/arm64_encode.h"
#include "arm64_reg.h"
#include "export_fun.h"
#include "inline_hook_frame.h"
#include "io_struct.h"
#include "emulate_insn.h"
#include "virtual_memory_rw.h"

struct ptebp_slot
{
    pte_t orig_pte;     // 页面安装数据保护前的原始 PTE，同页多个断点共享该快照。
    uint64_t hook_addr; // 去除地址标签并按 4 字节对齐后的断点虚拟地址。
    uint32_t orig_insn; // 被 BRK 覆盖的原始指令，也是数据读取时返回的逻辑内容。
};

// 当前只允许存在一组 PTEBP 监控；配置、目标 mm 和槽位状态由同一把锁保护。
static struct break_point *g_ptebp_info;
static struct mm_struct *g_ptebp_mm;
static struct ptebp_slot g_ptebp_slots[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);
// 标记整组撤销已经开始，避免多个停止或异常回退路径重复执行恢复流程。
static bool g_ptebp_stopping;

/*
构造受管页的数据保护 PTE。
不修改 UXN/PXN 等执行属性，只通过 AP、只读和 DBM 相关位撤销 EL0 数据访问。
这样 CPU 可以继续从原页面原生取指，而普通用户态 load/store 会进入 DABT permission fault。
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
通过安装前 PTE 保存的 PFN 直接访问断点指令，不依赖受管页当前的 EL0 数据权限。
写入后同步全部 CPU 的指令缓存并回读校验，确保 BRK 或原始指令已经真实落到代码页。
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
    if (!write) return linear_read_physical(paddr, insn, sizeof(*insn));

    status = linear_write_physical(paddr, insn, sizeof(*insn));
    if (status) return status;
    status = arm64_sync_code_range_all_cpus(phys_to_virt(paddr), sizeof(*insn));
    if (status) return status;

    status = linear_read_physical(paddr, &readback, sizeof(readback));
    if (status) return status;
    return readback == *insn ? 0 : -EIO;
}

// 校验槽位仍指向安装时的物理页，且当前 PTE 仍是预期的数据保护形态。
static bool ptebp_validate_slot(const struct ptebp_slot *slot, struct mm_struct *mm)
{
    pte_t pte_now;
    pte_t expected_pte;
    pteval_t changed;
    pteval_t current_value;
    pteval_t mutable = 0;
    uint64_t page_vaddr;

    if (!slot || !mm || !slot->hook_addr) return false;

    page_vaddr = slot->hook_addr & PAGE_MASK;
    if (read_user_pte_value(mm, page_vaddr, &current_value)) return false;
    pte_now = __pte(current_value);
    if (!pfn_valid(pte_pfn(pte_now))) return false;

    expected_pte = __pte(ptebp_make_data_guard_pte(pte_val(slot->orig_pte)));
    changed = pte_val(pte_now) ^ pte_val(expected_pte);
#ifdef PTE_AF
    mutable |= PTE_AF;
#endif
#ifdef PTE_DIRTY
    mutable |= PTE_DIRTY;
#endif

    // 访问位和脏页位可能由硬件更新，比较时只忽略这两类运行期可变位。
    return !(changed & ~mutable);
}

// DABT 模拟使用锁内复制的槽位快照，避免锁外解码和访存时直接遍历全局状态。
struct ptebp_emu_mem_ctx
{
    struct ptebp_slot slots[BP_CONFIG_MAX];
};

// 同页可以安装多个 BRK；返回该页任意一个槽位即可取得共享的原始 PTE/PFN。
static struct ptebp_slot *ptebp_emu_find_page(struct ptebp_emu_mem_ctx *ctx, uint64_t page_vaddr)
{
    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(ctx->slots); slot_index++)
        if (ctx->slots[slot_index].hook_addr && (ctx->slots[slot_index].hook_addr & PAGE_MASK) == page_vaddr) return &ctx->slots[slot_index];
    return NULL;
}

/*
读取 DABT 指令需要的数据，并向用户态呈现未打补丁的逻辑代码视图：
- 受管页通过原始 PTE 的 PFN 读取，绕过已经撤销的 EL0 数据权限；
- 相邻非受管页使用 nofault uaccess，避免异常上下文再次触发缺页；
- 读取范围覆盖任意断点时，用对应 orig_insn 字节替换物理页中的 BRK 字节。
*/
static int ptebp_emu_read_mem(struct ptebp_emu_mem_ctx *ctx, uint64_t addr, int bytes, __uint128_t *out)
{
    uint8_t data[sizeof(__uint128_t)];
    uint64_t end;
    uint64_t cursor;
    __uint128_t value = 0;

    if (!ctx || !out || bytes <= 0 || bytes > sizeof(data)) return -EINVAL;
    addr = untagged_addr(addr);
    if (addr > U64_MAX - (uint64_t)bytes) return -EFAULT;

    end = addr + (uint64_t)bytes;

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
        else if (copy_from_user_inatomic_nofault(data + data_offset, (const void __user *)(uintptr_t)cursor, copy_size))
        {
            return -EFAULT;
        }
        cursor += copy_size;
    }

    for (size_t slot_index = 0; slot_index < ARRAY_SIZE(ctx->slots); slot_index++)
    {
        uint64_t start = max(addr, ctx->slots[slot_index].hook_addr);
        uint64_t stop = min(end, ctx->slots[slot_index].hook_addr + sizeof(ctx->slots[slot_index].orig_insn));
        size_t data_offset;
        size_t insn_offset;
        size_t copy_size;

        if (!ctx->slots[slot_index].hook_addr) continue;
        if (start >= stop) continue;

        data_offset = start - addr;
        insn_offset = start - ctx->slots[slot_index].hook_addr;
        copy_size = stop - start;
        if (data_offset > (size_t)bytes || copy_size > (size_t)bytes - data_offset || insn_offset > sizeof(ctx->slots[slot_index].orig_insn) || copy_size > sizeof(ctx->slots[slot_index].orig_insn) - insn_offset) return -EFAULT;

        __builtin_memcpy(data + data_offset, (uint8_t *)&ctx->slots[slot_index].orig_insn + insn_offset, copy_size);
    }

    __builtin_memcpy(&value, data, bytes);
    *out = value;
    return 0;
}

/*
提交 DABT 指令的数据写入，并维持断点页的补丁一致性。

非受管页先通过 nofault uaccess 写入；受管页在锁内重新校验槽位后直接写原始 PFN。
如果写入范围覆盖断点指令，则同时更新快照和全局槽位中的 orig_insn，最后重新写回 BRK。
因此后续数据读取能看到用户刚写入的原始指令，而执行流仍会在同一地址命中断点。
*/
static int ptebp_emu_write_mem(struct ptebp_emu_mem_ctx *ctx, uint64_t addr, int bytes, __uint128_t value)
{
    uint8_t data[sizeof(value)];
    uint32_t brk_insn;
    uint64_t end;
    uint64_t cursor;
    unsigned long flags;
    size_t slot_index;
    int status;

    if (!ctx || bytes <= 0 || bytes > sizeof(data)) return -EINVAL;
    status = arm64_encode_brk(0, &brk_insn);
    if (status) return status;
    addr = untagged_addr(addr);
    if (addr > U64_MAX - (uint64_t)bytes) return -EFAULT;

    end = addr + (uint64_t)bytes;
    if (!ptebp_emu_find_page(ctx, addr & PAGE_MASK) && !ptebp_emu_find_page(ctx, (end - 1) & PAGE_MASK)) return -EOPNOTSUPP;
    __builtin_memcpy(data, &value, bytes);

    for (cursor = addr; cursor < end;)
    {
        uint64_t page_vaddr = cursor & PAGE_MASK;
        size_t data_offset = cursor - addr;
        size_t copy_size = min_t(size_t, end - cursor, PAGE_SIZE - offset_in_page(cursor));
        struct ptebp_slot *owner = ptebp_emu_find_page(ctx, page_vaddr);

        if (!owner && copy_to_user_inatomic_nofault((void __user *)(uintptr_t)cursor, data + data_offset, copy_size)) return -EFAULT;
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
            unsigned long pfn = pte_pfn(owner->orig_pte);
            phys_addr_t paddr;
            int status;

            if (g_ptebp_slots[owner - ctx->slots].hook_addr != owner->hook_addr || !ptebp_validate_slot(&g_ptebp_slots[owner - ctx->slots], current->mm) || !pte_write(owner->orig_pte))
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
            status = arm64_sync_code_range_all_cpus(phys_to_virt(paddr), copy_size);
            if (status)
            {
                spin_unlock_irqrestore(&g_ptebp_lock, flags);
                return status;
            }
        }
        cursor += copy_size;
    }

    for (slot_index = 0; slot_index < ARRAY_SIZE(ctx->slots); slot_index++)
    {
        uint64_t start;
        uint64_t stop;
        size_t data_offset;
        size_t insn_offset;
        size_t copy_size;
        phys_addr_t paddr;

        if (!ctx->slots[slot_index].hook_addr) continue;
        start = max(addr, ctx->slots[slot_index].hook_addr);
        stop = min(end, ctx->slots[slot_index].hook_addr + sizeof(ctx->slots[slot_index].orig_insn));
        if (start >= stop) continue;
        if (g_ptebp_slots[slot_index].hook_addr != ctx->slots[slot_index].hook_addr)
        {
            spin_unlock_irqrestore(&g_ptebp_lock, flags);
            return -ESTALE;
        }

        data_offset = start - addr;
        insn_offset = start - ctx->slots[slot_index].hook_addr;
        copy_size = stop - start;
        if (data_offset > (size_t)bytes || copy_size > (size_t)bytes - data_offset || insn_offset > sizeof(ctx->slots[slot_index].orig_insn) || copy_size > sizeof(ctx->slots[slot_index].orig_insn) - insn_offset)
        {
            spin_unlock_irqrestore(&g_ptebp_lock, flags);
            return -EFAULT;
        }
        __builtin_memcpy((uint8_t *)&ctx->slots[slot_index].orig_insn + insn_offset, data + data_offset, copy_size);
        __builtin_memcpy((uint8_t *)&g_ptebp_slots[slot_index].orig_insn + insn_offset, data + data_offset, copy_size);

        paddr = PFN_PHYS(pte_pfn(ctx->slots[slot_index].orig_pte)) + offset_in_page(ctx->slots[slot_index].hook_addr);
        status = linear_write_physical(paddr, &brk_insn, sizeof(brk_insn));
        if (status)
        {
            spin_unlock_irqrestore(&g_ptebp_lock, flags);
            return status;
        }
        status = arm64_sync_code_range_all_cpus(phys_to_virt(paddr), sizeof(brk_insn));
        if (status)
        {
            spin_unlock_irqrestore(&g_ptebp_lock, flags);
            return status;
        }
    }
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    return 0;
}

// 按实际访存宽度完成有符号 GPR load 的符号扩展。
static uint64_t ptebp_sign_extend_load(uint64_t value, uint8_t bytes)
{
    switch (bytes)
    {
    case 1:
        return (uint64_t)(int64_t)(int8_t)value;
    case 2:
        return (uint64_t)(int64_t)(int16_t)value;
    case 4:
        return (uint64_t)(int64_t)(int32_t)value;
    default:
        return value;
    }
}

// 写入必须完整位于同一页面，避免跨页部分提交后无法安全回退原生重试。
static bool ptebp_write_range_supported(uint64_t address, size_t bytes)
{
    if (!bytes || address > U64_MAX - (bytes - 1)) return false;
    return (address & PAGE_MASK) == ((address + bytes - 1) & PAGE_MASK);
}

/*
按 decoder 给出的架构语义提交一条普通 load/store：
- 成功时更新目标 GPR 或 FP/SIMD 寄存器、基址回写，并把 PC 推进 4 字节；
- pair store 合并为一次写入，避免第一元素成功、第二元素失败造成部分副作用；
- prefetch 没有架构可见的数据结果，直接推进 PC；
- exclusive、CAS/CASP、LSE RMW、SWP 等原子指令没有 case，返回 SKIP 触发整组回退。
*/
static enum emu_insn_result ptebp_emulate_load_store(struct pt_regs *regs, const struct arm64_decoded_insn *decoded, struct ptebp_emu_mem_ctx *ctx)
{
    uint64_t pc = regs->pc;

    switch (decoded->instruction)
    {
    case ARM64_INSN_LDR_LITERAL_GPR:
    case ARM64_INSN_LDUR_GPR:
    case ARM64_INSN_LDTR_GPR:
    case ARM64_INSN_LDR_GPR_POST_INDEX:
    case ARM64_INSN_LDR_GPR_PRE_INDEX:
    case ARM64_INSN_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;
        uint64_t base = decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL ? 0 : addr_reg_read(regs, decoded->rn);
        uint64_t address;
        uint64_t value;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (ptebp_emu_read_mem(ctx, address, decoded->access_bytes, &raw)) return EMU_INSN_SKIP;

        value = (uint64_t)raw;
        reg_write(regs, decoded->rt, value, decoded->operand_width == 64);
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_LDRSW_LITERAL:
    case ARM64_INSN_LDUR_SIGNED_GPR:
    case ARM64_INSN_LDTR_SIGNED_GPR:
    case ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX:
    case ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX:
    case ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET:
    case ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET:
    {
        __uint128_t raw;
        uint64_t base = decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL ? 0 : addr_reg_read(regs, decoded->rn);
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (ptebp_emu_read_mem(ctx, address, decoded->access_bytes, &raw)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rt, ptebp_sign_extend_load((uint64_t)raw, decoded->access_bytes), decoded->operand_width == 64);
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_LDLAR:
    case ARM64_INSN_LDAR:
    case ARM64_INSN_LDAPR:
    case ARM64_INSN_LDAPUR:
    {
        __uint128_t raw;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (ptebp_emu_read_mem(ctx, address, decoded->access_bytes, &raw)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rt, (uint64_t)raw, decoded->operand_width == 64);
        emu_commit_memory_writeback(decoded, regs, base);
        smp_mb();
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_LDAPUR_SIGNED:
    {
        __uint128_t raw;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (ptebp_emu_read_mem(ctx, address, decoded->access_bytes, &raw)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rt, ptebp_sign_extend_load((uint64_t)raw, decoded->access_bytes), decoded->operand_width == 64);
        emu_commit_memory_writeback(decoded, regs, base);
        smp_mb();
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_STUR_GPR:
    case ARM64_INSN_STTR_GPR:
    case ARM64_INSN_STR_GPR_POST_INDEX:
    case ARM64_INSN_STR_GPR_PRE_INDEX:
    case ARM64_INSN_STR_GPR_REGISTER_OFFSET:
    case ARM64_INSN_STR_GPR_UNSIGNED_OFFSET:
    {
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!ptebp_write_range_supported(address, decoded->access_bytes)) return EMU_INSN_SKIP;
        if (ptebp_emu_write_mem(ctx, address, decoded->access_bytes, reg_read(regs, decoded->rt))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_STLLR:
    case ARM64_INSN_STLR:
    case ARM64_INSN_STLUR:
    {
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!ptebp_write_range_supported(address, decoded->access_bytes)) return EMU_INSN_SKIP;
        smp_mb();
        if (ptebp_emu_write_mem(ctx, address, decoded->access_bytes, reg_read(regs, decoded->rt))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_LDNP_GPR:
    case ARM64_INSN_LDP_GPR_OFFSET:
    case ARM64_INSN_LDP_GPR_POST_INDEX:
    case ARM64_INSN_LDP_GPR_PRE_INDEX:
    {
        __uint128_t raw0;
        __uint128_t raw1;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;
        uint64_t value0;
        uint64_t value1;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (ptebp_emu_read_mem(ctx, address, decoded->access_bytes, &raw0) || ptebp_emu_read_mem(ctx, address + decoded->access_bytes, decoded->access_bytes, &raw1)) return EMU_INSN_SKIP;

        value0 = (uint64_t)raw0;
        value1 = (uint64_t)raw1;
        reg_write(regs, decoded->rt, value0, decoded->operand_width == 64);
        reg_write(regs, decoded->rt2, value1, decoded->operand_width == 64);
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_LDPSW_OFFSET:
    case ARM64_INSN_LDPSW_POST_INDEX:
    case ARM64_INSN_LDPSW_PRE_INDEX:
    {
        __uint128_t raw0;
        __uint128_t raw1;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (ptebp_emu_read_mem(ctx, address, decoded->access_bytes, &raw0) || ptebp_emu_read_mem(ctx, address + decoded->access_bytes, decoded->access_bytes, &raw1)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rt, ptebp_sign_extend_load((uint64_t)raw0, decoded->access_bytes), true);
        reg_write(regs, decoded->rt2, ptebp_sign_extend_load((uint64_t)raw1, decoded->access_bytes), true);
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_STNP_GPR:
    case ARM64_INSN_STP_GPR_OFFSET:
    case ARM64_INSN_STP_GPR_POST_INDEX:
    case ARM64_INSN_STP_GPR_PRE_INDEX:
    {
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;
        uint64_t mask;
        __uint128_t pair;
        size_t total_bytes = decoded->access_bytes * 2;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (total_bytes > sizeof(pair) || !ptebp_write_range_supported(address, total_bytes)) return EMU_INSN_SKIP;

        mask = decoded->access_bytes == sizeof(mask) ? U64_MAX : (1ULL << (decoded->access_bytes * 8)) - 1;
        pair = (__uint128_t)(reg_read(regs, decoded->rt) & mask) | ((__uint128_t)(reg_read(regs, decoded->rt2) & mask) << (decoded->access_bytes * 8));
        if (ptebp_emu_write_mem(ctx, address, total_bytes, pair)) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_LDR_LITERAL_FP_SIMD:
    case ARM64_INSN_LDUR_FP_SIMD:
    case ARM64_INSN_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INSN_LDR_FP_SIMD_PRE_INDEX:
    case ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET:
    {
        struct fp_regs fp_regs;
        __uint128_t value;
        uint64_t base = decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL ? 0 : addr_reg_read(regs, decoded->rn);
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (ptebp_emu_read_mem(ctx, address, decoded->access_bytes, &value)) return EMU_INSN_SKIP;
        read_all_q_regs(&fp_regs);
        fp_regs.q[decoded->rt] = value;
        write_all_q_regs(&fp_regs);
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_STUR_FP_SIMD:
    case ARM64_INSN_STR_FP_SIMD_POST_INDEX:
    case ARM64_INSN_STR_FP_SIMD_PRE_INDEX:
    case ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET:
    {
        struct fp_regs fp_regs;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!ptebp_write_range_supported(address, decoded->access_bytes)) return EMU_INSN_SKIP;
        read_all_q_regs(&fp_regs);
        if (ptebp_emu_write_mem(ctx, address, decoded->access_bytes, fp_regs.q[decoded->rt])) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_LDNP_FP_SIMD:
    case ARM64_INSN_LDP_FP_SIMD_OFFSET:
    case ARM64_INSN_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INSN_LDP_FP_SIMD_PRE_INDEX:
    {
        struct fp_regs fp_regs;
        __uint128_t value0;
        __uint128_t value1;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (ptebp_emu_read_mem(ctx, address, decoded->access_bytes, &value0) || ptebp_emu_read_mem(ctx, address + decoded->access_bytes, decoded->access_bytes, &value1)) return EMU_INSN_SKIP;
        read_all_q_regs(&fp_regs);
        fp_regs.q[decoded->rt] = value0;
        fp_regs.q[decoded->rt2] = value1;
        write_all_q_regs(&fp_regs);
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_STNP_FP_SIMD:
    case ARM64_INSN_STP_FP_SIMD_OFFSET:
    case ARM64_INSN_STP_FP_SIMD_POST_INDEX:
    case ARM64_INSN_STP_FP_SIMD_PRE_INDEX:
    {
        struct fp_regs fp_regs;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t address;
        __uint128_t element_mask;
        __uint128_t pair;
        size_t total_bytes = decoded->access_bytes * 2;

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (total_bytes > sizeof(pair) || !ptebp_write_range_supported(address, total_bytes)) return EMU_INSN_SKIP;

        read_all_q_regs(&fp_regs);
        element_mask = (((__uint128_t)1 << (decoded->access_bytes * 8)) - 1);
        pair = (fp_regs.q[decoded->rt] & element_mask) | ((fp_regs.q[decoded->rt2] & element_mask) << (decoded->access_bytes * 8));
        if (ptebp_emu_write_mem(ctx, address, total_bytes, pair)) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_PRFM_LITERAL:
    case ARM64_INSN_PRFUM:
    case ARM64_INSN_PRFM_REGISTER_OFFSET:
    case ARM64_INSN_PRFM_UNSIGNED_OFFSET:
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    default:
        return EMU_INSN_SKIP;
    }
}

/*
撤销整组 BRK/PTE 监控。

恢复顺序必须是“所有原始指令 -> 所有原始 PTE”：只要页面仍禁止 EL0 数据访问，
用户态就不会在 BRK 尚未全部移除时通过普通写入观察或破坏中间状态。
lock_mm=true 用于正常停止路径，在修改页表前持有目标 mm 的 mmap 读锁；异常回退路径已经
位于不可睡眠上下文，只使用现有槽位快照并做保守恢复。
*/
static void ptebp_drop_all_monitors(bool lock_mm)
{
    struct ptebp_slot slots[ARRAY_SIZE(g_ptebp_slots)];
    struct mm_struct *mm;
    unsigned long flags;

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

    // 先发布停止状态并复制槽位，后续重复撤销请求会直接返回。
    g_ptebp_stopping = true;
    memcpy(slots, g_ptebp_slots, sizeof(slots));
    if (lock_mm)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        mmap_read_lock(mm);
        spin_lock_irqsave(&g_ptebp_lock, flags);
    }

    // 第一阶段恢复所有断点地址的原始指令并同步指令缓存。
    for (size_t point_slot = 0; point_slot < ARRAY_SIZE(slots); point_slot++)
        if (slots[point_slot].hook_addr && ptebp_validate_slot(&slots[point_slot], mm)) (void)ptebp_access_insn(&slots[point_slot], &slots[point_slot].orig_insn, true);
    // 第二阶段恢复受管页面的原始 PTE；同页首个槽位恢复后，后续槽位校验会自然跳过。
    for (size_t point_slot = 0; point_slot < ARRAY_SIZE(slots); point_slot++)
        if (slots[point_slot].hook_addr && ptebp_validate_slot(&slots[point_slot], mm)) (void)write_user_pte_value(mm, slots[point_slot].hook_addr & PAGE_MASK, pte_val(slots[point_slot].orig_pte));

    g_ptebp_info = NULL;
    g_ptebp_mm = NULL;
    memset(g_ptebp_slots, 0, sizeof(g_ptebp_slots));
    g_ptebp_stopping = false;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (lock_mm) mmap_read_unlock(mm);
    mmput(mm);
}

/*
brk_handler 的 hook 工作函数，只接管当前目标 mm 中由本实现写入的 BRK 地址。

on_hit 直接操作当前 pt_regs 和完整 FP/SIMD 软件现场。回调若修改 PC，则尊重重定向结果；
PC 未改变时仅模拟该槽位保存的一条 orig_insn，没有页内循环或批量模拟。
*/
static int ptebp_handle_brk(struct pt_regs *hook_regs)
{
    struct pt_regs *regs;
    struct fp_regs fp_regs;
    uint64_t pc;
    unsigned long flags;
    uint32_t emulate_insn_word;
    struct bp_point *hit_point = NULL;

    if (!hook_regs) return 0;

    // brk_handler 的第三个参数是用户态 pt_regs，hook 入口保存在 x2。
    regs = (struct pt_regs *)hook_regs->regs[2];
    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING)) return 0;

    pc = untagged_addr(regs->pc) & ~0x3ULL;

    // 在锁内按目标 mm 和精确 PC 查找槽位，并复制本次需要模拟的原始指令。
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || g_ptebp_mm != current->mm) goto out_unlock;

    for (size_t point_slot = 0; point_slot < ARRAY_SIZE(g_ptebp_slots); point_slot++)
    {
        if (!g_ptebp_slots[point_slot].hook_addr || g_ptebp_slots[point_slot].hook_addr != pc) continue;

        hit_point = &g_ptebp_info->points[point_slot];
        emulate_insn_word = g_ptebp_slots[point_slot].orig_insn;
        break;
    }

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (!hit_point) return 0;

    // 回调和单条模拟共享同一份 FP/SIMD 现场，最终只回写一次。
    read_all_q_regs(&fp_regs);
    if (hit_point->on_hit) hit_point->on_hit(regs, &fp_regs, hit_point);

    // 回调未重定向 PC 时只模拟当前原始指令；失败则撤销监控，让当前 PC 原生重试。
    if (regs->pc == pc && !emulate_insn(regs, &fp_regs, emulate_insn_word)) ptebp_drop_all_monitors(false);
    write_all_q_regs(&fp_regs);
    // hook 跳过原 brk_handler，并向其调用方呈现已处理结果。
    hook_regs->regs[0] = 0;
    return 1;
}

/*
do_mem_abort 的 hook 工作函数，只接管目标 mm 中受管页的 EL0 L3 数据权限异常。
页面取指始终由 CPU 原生执行；这里仅为普通数据访问提供未打补丁的逻辑内存视图。
*/
static int ptebp_handle_data_abort(struct pt_regs *hook_regs)
{
    struct arm64_decoded_insn decoded;
    __uint128_t raw_insn;
    uint64_t far;
    uint64_t esr;
    struct pt_regs *regs;
    uint64_t fault_page;
    unsigned long flags;
    struct ptebp_emu_mem_ctx mem_ctx;
    bool owns_fault_page = false;
    bool stale_slot = false;

    if (!hook_regs) return 0;

    // do_mem_abort(far, esr, regs) 的三个参数分别保存在 hook 入口 x0、x1、x2。
    far = hook_regs->regs[0];
    esr = hook_regs->regs[1];
    regs = (struct pt_regs *)hook_regs->regs[2];
    fault_page = untagged_addr(far) & PAGE_MASK;

    if (ESR_ELx_EC(esr) != ESR_ELx_EC_DABT_LOW) return 0;

    if (!regs || !current->mm || !user_mode(regs) || (current->flags & PF_EXITING)) return 0;

    if ((esr & ESR_ELx_FSC) != (ESR_ELx_FSC_PERM | ESR_ELx_FSC_LEVEL)) return 0;

    // 锁内确认页面归属并复制完整槽位快照，模拟过程不长期持有全局锁。
    spin_lock_irqsave(&g_ptebp_lock, flags);
    if (!g_ptebp_info || g_ptebp_mm != current->mm) goto out_unlock;

    mem_ctx = (struct ptebp_emu_mem_ctx){};
    for (size_t point_slot = 0; point_slot < ARRAY_SIZE(g_ptebp_slots); point_slot++)
    {
        if (!g_ptebp_slots[point_slot].hook_addr) continue;
        if (!ptebp_validate_slot(&g_ptebp_slots[point_slot], current->mm))
        {
            stale_slot = true;
            break;
        }
        if ((g_ptebp_slots[point_slot].hook_addr & PAGE_MASK) == fault_page) owns_fault_page = true;

        mem_ctx.slots[point_slot] = g_ptebp_slots[point_slot];
    }
    // 任一槽位失配都说明映射或权限已被外部修改，整组撤销比局部继续模拟更安全。
    if (stale_slot)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        ptebp_drop_all_monitors(false);
        hook_regs->regs[0] = 0;
        return 1;
    }
    if (!owns_fault_page) goto out_unlock;
out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    if (!owns_fault_page) return 0;

    /*
    取指使用逻辑原始视图，因此即使 PC 位于 BRK 补丁位置，decoder 看到的仍是 orig_insn。
    不支持的指令和提交失败都保持当前 PC，并撤销监控，使原生指令重新执行。
    */
    if (ptebp_emu_read_mem(&mem_ctx, untagged_addr(regs->pc), sizeof(uint32_t), &raw_insn) ||
        arm64_decode_insn((uint32_t)raw_insn, &decoded) != ARM64_DECODE_OK ||
        ptebp_emulate_load_store(regs, &decoded, &mem_ctx) != EMU_INSN_HANDLED)
        ptebp_drop_all_monitors(false);
    hook_regs->regs[0] = 0;
    return 1;
}

// 分别接管数据权限异常和命中原地址 BRK 的异常处理入口。
static struct hook_entry g_ptebp_data_abort_hook = HOOK_ENTRY("do_mem_abort", ptebp_handle_data_abort);
static struct hook_entry g_ptebp_brk_hook = HOOK_ENTRY("brk_handler", ptebp_handle_brk);

// 停止监控时先恢复代码与 PTE，再移除异常 hook，并等待已经进入的 RCU 读侧结束。
static inline void stop_ptebp_monitor(void)
{
    ptebp_drop_all_monitors(true);
    hook_entry_remove(&g_ptebp_data_abort_hook);
    hook_entry_remove(&g_ptebp_brk_hook);
    synchronize_rcu();
}

/*
安装一个原地址 BRK 槽位：
1. 编码 BRK #0，规范化并校验断点地址；
2. 读取页面原始 PTE，拒绝重复地址以及本来就禁止 EL0 执行的新页面；
3. 保存原始指令并把物理代码替换为 BRK；
4. 仅第一个同页槽位安装 data-guard PTE，后续槽位复用同一 orig_pte。

任一步失败都会尽力恢复当前槽位的原始指令并清空软件状态。
*/
static int ptebp_install_slot(struct break_point *info, size_t point_slot, struct mm_struct *mm)
{
    pte_t orig_pte;
    pteval_t orig_value;
    struct ptebp_slot *page_owner;
    uint64_t page_vaddr;
    uint64_t hook_addr;
    uint32_t brk_insn;
    int status = 0;

    if (point_slot >= ARRAY_SIZE(g_ptebp_slots)) return -EINVAL;
    status = arm64_encode_brk(0, &brk_insn);
    if (status) return status;

    hook_addr = untagged_addr(info->points[point_slot].hit_addr) & ~0x3ULL;
    if (!hook_addr || hook_addr >= READ_ONCE(mm->task_size) || sizeof(g_ptebp_slots[point_slot].orig_insn) > READ_ONCE(mm->task_size) - hook_addr) return -EFAULT;
    page_vaddr = hook_addr & PAGE_MASK;

    status = read_user_pte_value(mm, page_vaddr, &orig_value);
    if (status) return status;
    orig_pte = __pte(orig_value);

    // 扫描已安装槽位，同时检测精确地址重复和同页 PTE 的共享 owner。
    page_owner = NULL;
    for (size_t scan_slot = 0; scan_slot < ARRAY_SIZE(g_ptebp_slots); scan_slot++)
    {
        if (!g_ptebp_slots[scan_slot].hook_addr) continue;
        if (g_ptebp_slots[scan_slot].hook_addr == hook_addr) return -EEXIST;
        if ((g_ptebp_slots[scan_slot].hook_addr & PAGE_MASK) == page_vaddr) page_owner = &g_ptebp_slots[scan_slot];
    }

    if (page_owner && !ptebp_validate_slot(page_owner, mm)) return -EFAULT;

    if (!page_owner && (pte_val(orig_pte) & PTE_UXN)) return -EACCES;

    // 同页后续槽位必须沿用第一次安装前的 PTE，而不是读取已经加保护的当前 PTE。
    if (page_owner) orig_pte = page_owner->orig_pte;

    g_ptebp_slots[point_slot] = (struct ptebp_slot){
        .orig_pte = orig_pte,
        .hook_addr = hook_addr,
    };
    // 先保存原始指令，再写入编码器生成的 BRK；原页面已经是同值 BRK 时拒绝接管。
    status = ptebp_access_insn(&g_ptebp_slots[point_slot], &g_ptebp_slots[point_slot].orig_insn, false);
    if (status) goto clear_slot;
    if (g_ptebp_slots[point_slot].orig_insn == brk_insn)
    {
        status = -ESTALE;
        goto clear_slot;
    }

    // 同页只有第一个槽位需要修改 PTE，其余槽位只增加 BRK 补丁。
    status = ptebp_access_insn(&g_ptebp_slots[point_slot], &brk_insn, true);
    if (!status && !page_owner) status = write_user_pte_value(mm, page_vaddr, ptebp_make_data_guard_pte(pte_val(orig_pte)));

    if (!status) return 0;

    if (status) (void)ptebp_access_insn(&g_ptebp_slots[point_slot], &g_ptebp_slots[point_slot].orig_insn, true);
clear_slot:
    memset(&g_ptebp_slots[point_slot], 0, sizeof(g_ptebp_slots[point_slot]));
    return status;
}

// 校验配置、安装两个异常 hook，并为目标进程的全部执行断点安装 BRK/PTE 状态。
static inline int start_ptebp_monitor(struct break_point *info)
{
    bool has_active_point = false;
    int status;
    struct mm_struct *mm;
    unsigned long flags;

    BUILD_BUG_ON(ARRAY_SIZE(g_ptebp_slots) != ARRAY_SIZE(((struct break_point *)0)->points));
    BUILD_BUG_ON(ARRAY_SIZE(g_ptebp_slots) != ARRAY_SIZE(((struct ptebp_emu_mem_ctx *)0)->slots));

    if (!info || info->tgid <= 0) return -EINVAL;

    // 新配置启动前先撤销旧实例，保证全局状态始终只对应一个目标 mm。
    stop_ptebp_monitor();

    mm = get_mm_by_pid(info->tgid);
    if (!mm) return -EINVAL;

    status = hook_entry_install(&g_ptebp_data_abort_hook);
    if (status) goto err_put_mm;

    status = hook_entry_install(&g_ptebp_brk_hook);
    if (status) goto err_remove_data_abort_hook;

    // 持有 mmap 读锁稳定目标页表；自旋锁保护全局槽位对异常处理器的发布过程。
    mmap_read_lock(mm);
    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_mm = mm;
    for (size_t point_slot = 0; point_slot < ARRAY_SIZE(info->points); point_slot++)
    {
        if (!info->points[point_slot].hit_addr || info->points[point_slot].bt != BP_BREAKPOINT_X) continue;
        has_active_point = true;
        status = ptebp_install_slot(info, point_slot, mm);
        if (status) break;
    }
    if (!has_active_point) status = -EINVAL;
    if (!status) g_ptebp_info = info;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    mmap_read_unlock(mm);

    if (!status) return 0;

    // 部分安装失败时统一走停止链路，恢复此前已经写入的全部 BRK 和 PTE。
    stop_ptebp_monitor();
    return status;

err_remove_data_abort_hook:
    hook_entry_remove(&g_ptebp_data_abort_hook);
err_put_mm:
    mmput(mm);
    return status;
}

#endif // ARM64_PTEDBG_H
