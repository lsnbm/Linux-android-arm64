#ifndef EMULATE_INSN_H
#define EMULATE_INSN_H

#include <asm/ptrace.h>
#include <asm/sysreg.h>
#include "arm64_reg.h"
#include "arm64_decode/arm64_decode.h"

enum emu_insn_result
{
    EMU_INSN_HANDLED,
    EMU_INSN_SKIP,
    EMU_INSN_NOP,
};

/* =========================================================================
 * ARM64 指令执行器
 *
 * 说明：
 * - 本文件是 decoder 架构语义到固定硬件模板的单文件执行层，不改写或执行原始指令编码。
 *
 * 作用：
 * - 在断点命中后执行当前用户态指令语义，将结果同步至 pt_regs 并推进 PC。
 * - 入口关闭 PAN，出口重新打开 PAN；取指和访存直接使用用户虚拟地址。
 * - 调用者提供完整 Q0-Q31 现场；执行器只更新该现场，FP/SIMD 访存额外保护 FPCR 与 FPSR。
 *
 * 已支持指令：
 * - 系统：NOP、YIELD、CLREX、DSB、DMB、ISB，以及仅支持 NZCV、FPCR、FPSR、TPIDR_EL0、TPIDRRO_EL0 和 CNTVCT_EL0 的有限 MRS/MSR 系统寄存器访问。
 * - 系统寄存器：NZCV、FPCR、FPSR、TPIDR_EL0、TPIDRRO_EL0、CNTVCT_EL0。
 * - 分支：B、BL、BR、BLR、RET、B.cond、CBZ/CBNZ、TBZ/TBNZ。
 * - 访存：普通、literal、pair、non-temporal、unprivileged、prefetch、RCpc、LDAPR、ordered、exclusive、LSE RMW、CAS 和 CASP。
 * - FP/SIMD：标量 FP 运算、比较、选择、转换和 GPR 传送，以及 AdvSIMD 的复制、移位、排列、逻辑、算术、逐元素、归约、窄化和提取。
 * - 数据处理：ADR/ADRP、加减、逻辑、位域、提取、宽立即数、条件选择/比较、单源/双源、乘加和高位乘法。
 *
 * 未支持指令：
 * - 异常生成与异常返回指令。
 * - YIELD 以外的 HINT，以及白名单之外的系统寄存器访问。
 * - SVE、SME，以及 decoder 未识别或执行器尚无硬件模板的编码。
 * ========================================================================= */

/* ======================== 跨大类通用现场辅助 ======================== */

static inline uint64_t reg_read(struct pt_regs *regs, uint32_t n)
{
    return (n == 31) ? 0ULL : regs->regs[n];
}
static inline void reg_write(struct pt_regs *regs, uint32_t n, uint64_t val, bool sf)
{
    if (n != 31) regs->regs[n] = sf ? val : (uint64_t)(uint32_t)val;
}
static inline uint64_t addr_reg_read(struct pt_regs *regs, uint32_t n)
{
    return (n == 31) ? regs->sp : regs->regs[n];
}
static inline void addr_reg_write(struct pt_regs *regs, uint32_t n, uint64_t val)
{
    if (n == 31) regs->sp = val;
    else regs->regs[n] = val;
}

static inline void emu_write_nzcv(struct pt_regs *regs, uint64_t nzcv)
{
    regs->pstate = (regs->pstate & ~((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28))) | (nzcv & ((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28)));
}

static inline bool emu_cond_holds_hw(uint64_t pstate, uint32_t cond)
{
    uint32_t take;

    switch (cond)
    {
    case 0x0:
        asm volatile("msr nzcv, %1\ncset %w0, eq\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0x1:
        asm volatile("msr nzcv, %1\ncset %w0, ne\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0x2:
        asm volatile("msr nzcv, %1\ncset %w0, cs\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0x3:
        asm volatile("msr nzcv, %1\ncset %w0, cc\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0x4:
        asm volatile("msr nzcv, %1\ncset %w0, mi\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0x5:
        asm volatile("msr nzcv, %1\ncset %w0, pl\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0x6:
        asm volatile("msr nzcv, %1\ncset %w0, vs\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0x7:
        asm volatile("msr nzcv, %1\ncset %w0, vc\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0x8:
        asm volatile("msr nzcv, %1\ncset %w0, hi\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0x9:
        asm volatile("msr nzcv, %1\ncset %w0, ls\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0xA:
        asm volatile("msr nzcv, %1\ncset %w0, ge\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0xB:
        asm volatile("msr nzcv, %1\ncset %w0, lt\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0xC:
        asm volatile("msr nzcv, %1\ncset %w0, gt\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    case 0xD:
        asm volatile("msr nzcv, %1\ncset %w0, le\n" : "=r"(take) : "r"(pstate & (0xFULL << 28)) : "cc");
        break;
    default:
        return true;
    }
    return take != 0;
}

/* ======================== 系统类：私有模板与完整执行流程 ======================== */

#define EMU_SYSREG_NZCV        ARM64_SYSREG_KEY(3, 3, 4, 2, 0)
#define EMU_SYSREG_FPCR        ARM64_SYSREG_KEY(3, 3, 4, 4, 0)
#define EMU_SYSREG_FPSR        ARM64_SYSREG_KEY(3, 3, 4, 4, 1)
#define EMU_SYSREG_TPIDR_EL0   ARM64_SYSREG_KEY(3, 3, 13, 0, 2)
#define EMU_SYSREG_TPIDRRO_EL0 ARM64_SYSREG_KEY(3, 3, 13, 0, 3)
#define EMU_SYSREG_CNTVCT_EL0  ARM64_SYSREG_KEY(3, 3, 14, 0, 2)

#define EMU_SYSTEM_OPTION_CASE(OPTION, BASE)                               \
    case OPTION:                                                           \
        asm volatile(".inst " #BASE " + (" #OPTION " << 8)" ::: "memory"); \
        break

#define EMU_SYSTEM_OPTION_CASES(BASE) \
    EMU_SYSTEM_OPTION_CASE(0, BASE);  \
    EMU_SYSTEM_OPTION_CASE(1, BASE);  \
    EMU_SYSTEM_OPTION_CASE(2, BASE);  \
    EMU_SYSTEM_OPTION_CASE(3, BASE);  \
    EMU_SYSTEM_OPTION_CASE(4, BASE);  \
    EMU_SYSTEM_OPTION_CASE(5, BASE);  \
    EMU_SYSTEM_OPTION_CASE(6, BASE);  \
    EMU_SYSTEM_OPTION_CASE(7, BASE);  \
    EMU_SYSTEM_OPTION_CASE(8, BASE);  \
    EMU_SYSTEM_OPTION_CASE(9, BASE);  \
    EMU_SYSTEM_OPTION_CASE(10, BASE); \
    EMU_SYSTEM_OPTION_CASE(11, BASE); \
    EMU_SYSTEM_OPTION_CASE(12, BASE); \
    EMU_SYSTEM_OPTION_CASE(13, BASE); \
    EMU_SYSTEM_OPTION_CASE(14, BASE); \
    EMU_SYSTEM_OPTION_CASE(15, BASE)

static inline bool emu_system_option_insn(uint32_t option, uint32_t base)
{
    switch (base)
    {
    case 0xD503305FU:
        switch (option)
        {
            EMU_SYSTEM_OPTION_CASES(0xD503305F);
        }
        return true;
    case 0xD503309FU:
        switch (option)
        {
            EMU_SYSTEM_OPTION_CASES(0xD503309F);
        }
        return true;
    case 0xD50330BFU:
        switch (option)
        {
            EMU_SYSTEM_OPTION_CASES(0xD50330BF);
        }
        return true;
    default:
        return false;
    }
}

static inline enum emu_insn_result emu_simulate_system_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded, uint64_t pc)
{
    if (decoded->opcode == ARM64_OP_NOP) return EMU_INSN_NOP;

    if (decoded->opcode == ARM64_OP_HINT)
    {
        if (decoded->operands.system.operation != ARM64_SYSTEM_OP_YIELD) return EMU_INSN_SKIP;
        asm volatile("yield");
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_BARRIER)
    {
        switch (decoded->operands.system.operation)
        {
        case ARM64_SYSTEM_OP_CLREX:
            if (!emu_system_option_insn(decoded->operands.system.option, 0xD503305FU)) return EMU_INSN_SKIP;
            break;
        case ARM64_SYSTEM_OP_DSB:
            if (!emu_system_option_insn(decoded->operands.system.option, 0xD503309FU)) return EMU_INSN_SKIP;
            break;
        case ARM64_SYSTEM_OP_DMB:
            if (!emu_system_option_insn(decoded->operands.system.option, 0xD50330BFU)) return EMU_INSN_SKIP;
            break;
        case ARM64_SYSTEM_OP_ISB:
            asm volatile("isb" ::: "memory");
            break;
        default:
            return EMU_INSN_SKIP;
        }
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_MRS || decoded->opcode == ARM64_OP_MSR_REGISTER)
    {
        const struct arm64_system_operands *system = &decoded->operands.system;
        uint32_t rt = decoded->rt;
        uint32_t sysreg = ARM64_SYSREG_KEY(system->op0, system->op1, system->crn, system->crm, system->op2);
        bool is_mrs = decoded->opcode == ARM64_OP_MRS;
        bool handled = true;
        uint64_t val;

        if (is_mrs)
        {
            switch (sysreg)
            {
            case EMU_SYSREG_NZCV:
                val = regs->pstate & (0xFULL << 28);
                break;
            case EMU_SYSREG_FPCR:
                val = read_fpcr();
                break;
            case EMU_SYSREG_FPSR:
                val = read_fpsr();
                break;
            case EMU_SYSREG_TPIDR_EL0:
                val = read_sysreg(tpidr_el0);
                break;
            case EMU_SYSREG_TPIDRRO_EL0:
                val = read_sysreg(tpidrro_el0);
                break;
            case EMU_SYSREG_CNTVCT_EL0:
                val = read_sysreg(cntvct_el0);
                break;
            default:
                handled = false;
                val = 0;
                break;
            }

            if (handled) reg_write(regs, rt, val, true);
        }
        else
        {
            val = reg_read(regs, rt);
            switch (sysreg)
            {
            case EMU_SYSREG_NZCV:
                emu_write_nzcv(regs, val);
                break;
            case EMU_SYSREG_FPCR:
                write_fpcr((uint32_t)val);
                break;
            case EMU_SYSREG_FPSR:
                write_fpsr((uint32_t)val);
                break;
            case EMU_SYSREG_TPIDR_EL0:
                write_sysreg(val, tpidr_el0);
                break;
            default:
                handled = false;
                break;
            }
        }

        if (!handled) return EMU_INSN_SKIP;

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    return EMU_INSN_SKIP;
}

/* ======================== 分支类：完整执行流程 ======================== */

static inline enum emu_insn_result emu_simulate_branch_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded, uint64_t pc)
{
    switch (decoded->opcode)
    {
    case ARM64_OP_B:
    case ARM64_OP_BL:
        if (decoded->opcode == ARM64_OP_BL) regs->regs[30] = pc + 4;
        regs->pc = pc + decoded->operands.branch.offset;
        return EMU_INSN_HANDLED;
    case ARM64_OP_BR:
    case ARM64_OP_BLR:
    case ARM64_OP_RET:
        regs->pc = reg_read(regs, decoded->rn);
        if (decoded->opcode == ARM64_OP_BLR) regs->regs[30] = pc + 4;
        return EMU_INSN_HANDLED;
    case ARM64_OP_B_COND:
        if (emu_cond_holds_hw(regs->pstate, decoded->operands.branch.condition))
        {
            regs->pc = pc + decoded->operands.branch.offset;
        }
        else regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    case ARM64_OP_CBZ:
    case ARM64_OP_CBNZ:
    {
        uint64_t val = (decoded->flags & ARM64_INSN_FLAG_64BIT) ? reg_read(regs, decoded->rt) : (uint32_t)reg_read(regs, decoded->rt);
        bool jump = decoded->opcode == ARM64_OP_CBNZ ? val != 0 : val == 0;

        if (jump)
        {
            regs->pc = pc + decoded->operands.branch.offset;
        }
        else regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_OP_TBZ:
    case ARM64_OP_TBNZ:
    {
        bool bit_set = ((reg_read(regs, decoded->rt) >> decoded->operands.branch.test_bit) & 1) != 0;
        bool jump = decoded->opcode == ARM64_OP_TBNZ ? bit_set : !bit_set;

        if (jump)
        {
            regs->pc = pc + decoded->operands.branch.offset;
        }
        else regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    default:
        return EMU_INSN_SKIP;
    }
}

/* ======================== 访存类：纯机械硬件模板 ======================== */

struct emu_memory_address
{
    uint64_t address;
    uint64_t writeback_address;
    bool writeback;
};

static inline uint64_t emu_extend_memory_index(uint64_t value, uint8_t extend_type)
{
    switch (extend_type)
    {
    case 2:
        return (uint32_t)value;
    case 6:
        return (uint64_t)(int64_t)(int32_t)value;
    default:
        return value;
    }
}

/* 地址求值依赖当前 PC 和寄存器现场，因此属于执行阶段而非 decoder 固有语义。 */
static inline bool emu_resolve_memory_address(const struct arm64_load_store_operands *operands, uint64_t pc, uint64_t base, uint64_t index, struct emu_memory_address *address)
{
    address->address = 0;
    address->writeback_address = 0;
    address->writeback = false;

    switch (operands->address_mode)
    {
    case ARM64_ADDRESS_LITERAL:
        address->address = pc + operands->offset;
        break;
    case ARM64_ADDRESS_BASE:
    case ARM64_ADDRESS_UNSIGNED_OFFSET:
    case ARM64_ADDRESS_UNSCALED_OFFSET:
        address->address = base + operands->offset;
        break;
    case ARM64_ADDRESS_PRE_INDEX:
        address->address = base + operands->offset;
        address->writeback_address = address->address;
        address->writeback = true;
        break;
    case ARM64_ADDRESS_POST_INDEX:
        address->address = base;
        address->writeback_address = base + operands->offset;
        address->writeback = true;
        break;
    case ARM64_ADDRESS_REGISTER_OFFSET:
        address->address = base + (emu_extend_memory_index(index, operands->extend_type) << operands->shift_amount);
        break;
    default:
        return false;
    }

    return true;
}

#define EMU_HW_LD(INST, VALUE, ADDR)  asm volatile(INST " %0, [%1]" : "=&r"(VALUE) : "r"(ADDR) : "memory")
#define EMU_HW_LDW(INST, VALUE, ADDR) asm volatile(INST " %w0, [%1]" : "=&r"(VALUE) : "r"(ADDR) : "memory")
#define EMU_HW_ST(INST, VALUE, ADDR)  asm volatile(INST " %0, [%1]" : : "r"(VALUE), "r"(ADDR) : "memory")
#define EMU_HW_STW(INST, VALUE, ADDR) asm volatile(INST " %w0, [%1]" : : "r"(VALUE), "r"(ADDR) : "memory")

static inline bool emu_hw_load_gpr(uint64_t addr, int bytes, bool sign_extend, bool unprivileged, bool sf, uint64_t *out)
{
    uint64_t value;

    if (unprivileged)
    {
        if (bytes == 1 && sign_extend && sf) EMU_HW_LD("ldtrsb", value, addr);
        else if (bytes == 1 && sign_extend) EMU_HW_LDW("ldtrsb", value, addr);
        else if (bytes == 1) EMU_HW_LDW("ldtrb", value, addr);
        else if (bytes == 2 && sign_extend && sf) EMU_HW_LD("ldtrsh", value, addr);
        else if (bytes == 2 && sign_extend) EMU_HW_LDW("ldtrsh", value, addr);
        else if (bytes == 2) EMU_HW_LDW("ldtrh", value, addr);
        else if (bytes == 4 && sign_extend && sf) EMU_HW_LD("ldtrsw", value, addr);
        else if (bytes == 4 && !sign_extend) EMU_HW_LDW("ldtr", value, addr);
        else if (bytes == 8 && !sign_extend) EMU_HW_LD("ldtr", value, addr);
        else return false;
    }
    else
    {
        if (bytes == 1 && sign_extend && sf) EMU_HW_LD("ldrsb", value, addr);
        else if (bytes == 1 && sign_extend) EMU_HW_LDW("ldrsb", value, addr);
        else if (bytes == 1) EMU_HW_LDW("ldrb", value, addr);
        else if (bytes == 2 && sign_extend && sf) EMU_HW_LD("ldrsh", value, addr);
        else if (bytes == 2 && sign_extend) EMU_HW_LDW("ldrsh", value, addr);
        else if (bytes == 2) EMU_HW_LDW("ldrh", value, addr);
        else if (bytes == 4 && sign_extend && sf) EMU_HW_LD("ldrsw", value, addr);
        else if (bytes == 4 && !sign_extend) EMU_HW_LDW("ldr", value, addr);
        else if (bytes == 8 && !sign_extend) EMU_HW_LD("ldr", value, addr);
        else return false;
    }

    *out = value;
    return true;
}

static inline bool emu_hw_store_gpr(uint64_t addr, int bytes, bool unprivileged, uint64_t value)
{
    if (unprivileged)
    {
        if (bytes == 1) EMU_HW_STW("sttrb", value, addr);
        else if (bytes == 2) EMU_HW_STW("sttrh", value, addr);
        else if (bytes == 4) EMU_HW_STW("sttr", value, addr);
        else if (bytes == 8) EMU_HW_ST("sttr", value, addr);
        else return false;
    }
    else
    {
        if (bytes == 1) EMU_HW_STW("strb", value, addr);
        else if (bytes == 2) EMU_HW_STW("strh", value, addr);
        else if (bytes == 4) EMU_HW_STW("str", value, addr);
        else if (bytes == 8) EMU_HW_ST("str", value, addr);
        else return false;
    }
    return true;
}

static inline bool emu_hw_load_fp(uint64_t addr, int bytes, __uint128_t *out)
{
    if (bytes == 1) asm volatile(".arch_extension fp\n.arch_extension simd\nldr b0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0");
    else if (bytes == 2) asm volatile(".arch_extension fp\n.arch_extension simd\nldr h0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0");
    else if (bytes == 4) asm volatile(".arch_extension fp\n.arch_extension simd\nldr s0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0");
    else if (bytes == 8) asm volatile(".arch_extension fp\n.arch_extension simd\nldr d0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0");
    else if (bytes == 16) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0");
    else return false;
    return true;
}

static inline bool emu_hw_store_fp(uint64_t addr, int bytes, const __uint128_t *value)
{
    if (bytes == 1) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nstr b0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0");
    else if (bytes == 2) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nstr h0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0");
    else if (bytes == 4) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nstr s0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0");
    else if (bytes == 8) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nstr d0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0");
    else if (bytes == 16) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nstr q0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0");
    else return false;
    return true;
}

static inline bool emu_hw_load_pair_gpr(uint64_t addr, int bytes, bool sign_extend, bool non_temporal, uint64_t *first, uint64_t *second)
{
    uint64_t value0, value1;

    if (sign_extend && bytes == 4) asm volatile("ldpsw %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory");
    else if (bytes == 4 && non_temporal) asm volatile("ldnp %w0, %w1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory");
    else if (bytes == 4) asm volatile("ldp %w0, %w1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory");
    else if (bytes == 8 && non_temporal) asm volatile("ldnp %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory");
    else if (bytes == 8) asm volatile("ldp %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory");
    else return false;

    *first = value0;
    *second = value1;
    return true;
}

static inline bool emu_hw_store_pair_gpr(uint64_t addr, int bytes, bool non_temporal, uint64_t first, uint64_t second)
{
    if (bytes == 4 && non_temporal) asm volatile("stnp %w0, %w1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory");
    else if (bytes == 4) asm volatile("stp %w0, %w1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory");
    else if (bytes == 8 && non_temporal) asm volatile("stnp %0, %1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory");
    else if (bytes == 8) asm volatile("stp %0, %1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory");
    else return false;
    return true;
}

static inline bool emu_hw_load_pair_fp(uint64_t addr, int bytes, bool non_temporal, __uint128_t *first, __uint128_t *second)
{
    if (bytes == 4 && non_temporal) asm volatile(".arch_extension fp\n.arch_extension simd\nldnp s0, s1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 4) asm volatile(".arch_extension fp\n.arch_extension simd\nldp s0, s1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 8 && non_temporal) asm volatile(".arch_extension fp\n.arch_extension simd\nldnp d0, d1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 8) asm volatile(".arch_extension fp\n.arch_extension simd\nldp d0, d1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 16 && non_temporal) asm volatile(".arch_extension fp\n.arch_extension simd\nldnp q0, q1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 16) asm volatile(".arch_extension fp\n.arch_extension simd\nldp q0, q1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else return false;
    return true;
}

static inline bool emu_hw_store_pair_fp(uint64_t addr, int bytes, bool non_temporal, const __uint128_t *first, const __uint128_t *second)
{
    if (bytes == 4 && non_temporal) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nldr q1, [%1]\nstnp s0, s1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 4) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nldr q1, [%1]\nstp s0, s1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 8 && non_temporal) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nldr q1, [%1]\nstnp d0, d1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 8) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nldr q1, [%1]\nstp d0, d1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 16 && non_temporal) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nldr q1, [%1]\nstnp q0, q1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else if (bytes == 16) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q0, [%0]\nldr q1, [%1]\nstp q0, q1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1");
    else return false;
    return true;
}

#define EMU_HW_RCPC_LD(INST, VALUE, ADDR) asm volatile("mov x1, %1\n.inst " __stringify(INST) "\nmov %0, x0" : "=&r"(VALUE) : "r"(ADDR) : "memory", "x0", "x1")
#define EMU_HW_RCPC_ST(INST, VALUE, ADDR) asm volatile("mov x0, %0\nmov x1, %1\n.inst " __stringify(INST) : : "r"(VALUE), "r"(ADDR) : "memory", "x0", "x1")

static inline bool emu_hw_load_rcpc(uint64_t addr, int bytes, bool sign_extend, bool sf, uint64_t *out)
{
    uint64_t value;

    if (bytes == 1 && sign_extend && sf) EMU_HW_RCPC_LD(0x19800020, value, addr);
    else if (bytes == 1 && sign_extend) EMU_HW_RCPC_LD(0x19C00020, value, addr);
    else if (bytes == 1) EMU_HW_RCPC_LD(0x19400020, value, addr);
    else if (bytes == 2 && sign_extend && sf) EMU_HW_RCPC_LD(0x59800020, value, addr);
    else if (bytes == 2 && sign_extend) EMU_HW_RCPC_LD(0x59C00020, value, addr);
    else if (bytes == 2) EMU_HW_RCPC_LD(0x59400020, value, addr);
    else if (bytes == 4 && sign_extend && sf) EMU_HW_RCPC_LD(0x99800020, value, addr);
    else if (bytes == 4 && !sign_extend) EMU_HW_RCPC_LD(0x99400020, value, addr);
    else if (bytes == 8 && !sign_extend) EMU_HW_RCPC_LD(0xD9400020, value, addr);
    else return false;

    *out = value;
    return true;
}

static inline bool emu_hw_store_rcpc(uint64_t addr, int bytes, uint64_t value)
{
    if (bytes == 1) EMU_HW_RCPC_ST(0x19000020, value, addr);
    else if (bytes == 2) EMU_HW_RCPC_ST(0x59000020, value, addr);
    else if (bytes == 4) EMU_HW_RCPC_ST(0x99000020, value, addr);
    else if (bytes == 8) EMU_HW_RCPC_ST(0xD9000020, value, addr);
    else return false;
    return true;
}

static inline bool emu_hw_load_ldapr(uint64_t addr, int bytes, uint64_t *out)
{
    uint64_t value;

    if (bytes == 1) EMU_HW_LDW(".arch_extension rcpc\nldaprb", value, addr);
    else if (bytes == 2) EMU_HW_LDW(".arch_extension rcpc\nldaprh", value, addr);
    else if (bytes == 4) EMU_HW_LDW(".arch_extension rcpc\nldapr", value, addr);
    else if (bytes == 8) EMU_HW_LD(".arch_extension rcpc\nldapr", value, addr);
    else return false;

    *out = value;
    return true;
}

#define EMU_HW_ATOMIC_EXEC(INST, ORDER, SRC, OLD, ADDR, BYTES)                                                                                    \
    do                                                                                                                                            \
    {                                                                                                                                             \
        if ((BYTES) == 1) asm volatile(".arch_extension lse\n" INST ORDER "b %w2, %w0, [%1]" : "=&r"(OLD) : "r"(ADDR), "r"(SRC) : "memory");      \
        else if ((BYTES) == 2) asm volatile(".arch_extension lse\n" INST ORDER "h %w2, %w0, [%1]" : "=&r"(OLD) : "r"(ADDR), "r"(SRC) : "memory"); \
        else if ((BYTES) == 4) asm volatile(".arch_extension lse\n" INST ORDER " %w2, %w0, [%1]" : "=&r"(OLD) : "r"(ADDR), "r"(SRC) : "memory");  \
        else if ((BYTES) == 8) asm volatile(".arch_extension lse\n" INST ORDER " %2, %0, [%1]" : "=&r"(OLD) : "r"(ADDR), "r"(SRC) : "memory");    \
        else return false;                                                                                                                        \
    } while (0)

#define EMU_HW_ATOMIC_SELECT(INST, SRC, OLD, ADDR, BYTES, ACQUIRE, RELEASE)                \
    do                                                                                     \
    {                                                                                      \
        if ((ACQUIRE) && (RELEASE)) EMU_HW_ATOMIC_EXEC(INST, "al", SRC, OLD, ADDR, BYTES); \
        else if (ACQUIRE) EMU_HW_ATOMIC_EXEC(INST, "a", SRC, OLD, ADDR, BYTES);            \
        else if (RELEASE) EMU_HW_ATOMIC_EXEC(INST, "l", SRC, OLD, ADDR, BYTES);            \
        else EMU_HW_ATOMIC_EXEC(INST, "", SRC, OLD, ADDR, BYTES);                          \
    } while (0)

static inline bool emu_hw_atomic_rmw(enum arm64_operation operation, uint64_t addr, int bytes, bool acquire, bool release, uint64_t src, uint64_t *old)
{
    uint64_t value;

    switch (operation)
    {
    case ARM64_OPERATION_LDADD:
        EMU_HW_ATOMIC_SELECT("ldadd", src, value, addr, bytes, acquire, release);
        break;
    case ARM64_OPERATION_LDCLR:
        EMU_HW_ATOMIC_SELECT("ldclr", src, value, addr, bytes, acquire, release);
        break;
    case ARM64_OPERATION_LDEOR:
        EMU_HW_ATOMIC_SELECT("ldeor", src, value, addr, bytes, acquire, release);
        break;
    case ARM64_OPERATION_LDSET:
        EMU_HW_ATOMIC_SELECT("ldset", src, value, addr, bytes, acquire, release);
        break;
    case ARM64_OPERATION_LDSMAX:
        EMU_HW_ATOMIC_SELECT("ldsmax", src, value, addr, bytes, acquire, release);
        break;
    case ARM64_OPERATION_LDSMIN:
        EMU_HW_ATOMIC_SELECT("ldsmin", src, value, addr, bytes, acquire, release);
        break;
    case ARM64_OPERATION_LDUMAX:
        EMU_HW_ATOMIC_SELECT("ldumax", src, value, addr, bytes, acquire, release);
        break;
    case ARM64_OPERATION_LDUMIN:
        EMU_HW_ATOMIC_SELECT("ldumin", src, value, addr, bytes, acquire, release);
        break;
    case ARM64_OPERATION_SWP:
        EMU_HW_ATOMIC_SELECT("swp", src, value, addr, bytes, acquire, release);
        break;
    default:
        return false;
    }

    *old = value;
    return true;
}

#define EMU_HW_CAS_EXEC(ORDER, EXPECTED, DESIRED, ADDR, BYTES)                                                                                           \
    do                                                                                                                                                   \
    {                                                                                                                                                    \
        if ((BYTES) == 1) asm volatile(".arch_extension lse\ncas" ORDER "b %w0, %w2, [%1]" : "+&r"(EXPECTED) : "r"(ADDR), "r"(DESIRED) : "memory");      \
        else if ((BYTES) == 2) asm volatile(".arch_extension lse\ncas" ORDER "h %w0, %w2, [%1]" : "+&r"(EXPECTED) : "r"(ADDR), "r"(DESIRED) : "memory"); \
        else if ((BYTES) == 4) asm volatile(".arch_extension lse\ncas" ORDER " %w0, %w2, [%1]" : "+&r"(EXPECTED) : "r"(ADDR), "r"(DESIRED) : "memory");  \
        else if ((BYTES) == 8) asm volatile(".arch_extension lse\ncas" ORDER " %0, %2, [%1]" : "+&r"(EXPECTED) : "r"(ADDR), "r"(DESIRED) : "memory");    \
        else return false;                                                                                                                               \
    } while (0)

static inline bool emu_hw_cas(uint64_t addr, int bytes, bool acquire, bool release, uint64_t desired, uint64_t *expected)
{
    uint64_t value = *expected;

    if (acquire && release) EMU_HW_CAS_EXEC("al", value, desired, addr, bytes);
    else if (acquire) EMU_HW_CAS_EXEC("a", value, desired, addr, bytes);
    else if (release) EMU_HW_CAS_EXEC("l", value, desired, addr, bytes);
    else EMU_HW_CAS_EXEC("", value, desired, addr, bytes);
    *expected = value;
    return true;
}

#define EMU_HW_CASP64(ORDER, OUT0, OUT1, EXPECTED0, EXPECTED1, DESIRED0, DESIRED1, ADDR)   \
    asm volatile(".arch_extension lse\n"                                                   \
                 "mov x0, %2\nmov x1, %3\nmov x2, %4\nmov x3, %5\nmov x4, %6\n"            \
                 "casp" ORDER " x0, x1, x2, x3, [x4]\nmov %0, x0\nmov %1, x1"              \
                 : "=&r"(OUT0), "=&r"(OUT1)                                                \
                 : "r"(EXPECTED0), "r"(EXPECTED1), "r"(DESIRED0), "r"(DESIRED1), "r"(ADDR) \
                 : "memory", "x0", "x1", "x2", "x3", "x4")

#define EMU_HW_CASP32(ORDER, OUT0, OUT1, EXPECTED0, EXPECTED1, DESIRED0, DESIRED1, ADDR)   \
    asm volatile(".arch_extension lse\n"                                                   \
                 "mov w0, %w2\nmov w1, %w3\nmov w2, %w4\nmov w3, %w5\nmov x4, %6\n"        \
                 "casp" ORDER " w0, w1, w2, w3, [x4]\nmov %w0, w0\nmov %w1, w1"            \
                 : "=&r"(OUT0), "=&r"(OUT1)                                                \
                 : "r"(EXPECTED0), "r"(EXPECTED1), "r"(DESIRED0), "r"(DESIRED1), "r"(ADDR) \
                 : "memory", "x0", "x1", "x2", "x3", "x4")

#define EMU_HW_CASP_EXEC(ORDER, BYTES, OUT0, OUT1, EXPECTED0, EXPECTED1, DESIRED0, DESIRED1, ADDR)               \
    do                                                                                                           \
    {                                                                                                            \
        if ((BYTES) == 4) EMU_HW_CASP32(ORDER, OUT0, OUT1, EXPECTED0, EXPECTED1, DESIRED0, DESIRED1, ADDR);      \
        else if ((BYTES) == 8) EMU_HW_CASP64(ORDER, OUT0, OUT1, EXPECTED0, EXPECTED1, DESIRED0, DESIRED1, ADDR); \
        else return false;                                                                                       \
    } while (0)

static inline bool emu_hw_casp(uint64_t addr, int bytes, bool acquire, bool release, uint64_t desired0, uint64_t desired1, uint64_t *expected0, uint64_t *expected1)
{
    uint64_t input0 = *expected0;
    uint64_t input1 = *expected1;
    uint64_t output0, output1;

    if (acquire && release) EMU_HW_CASP_EXEC("al", bytes, output0, output1, input0, input1, desired0, desired1, addr);
    else if (acquire) EMU_HW_CASP_EXEC("a", bytes, output0, output1, input0, input1, desired0, desired1, addr);
    else if (release) EMU_HW_CASP_EXEC("l", bytes, output0, output1, input0, input1, desired0, desired1, addr);
    else EMU_HW_CASP_EXEC("", bytes, output0, output1, input0, input1, desired0, desired1, addr);
    *expected0 = output0;
    *expected1 = output1;
    return true;
}

#define EMU_HW_ORDERED_LD(INST, VALUE, ADDR)  asm volatile("mov x1, %1\n.inst " __stringify(INST) "\nmov %w0, w0" : "=&r"(VALUE) : "r"(ADDR) : "memory", "x0", "x1")
#define EMU_HW_ORDERED_LDX(INST, VALUE, ADDR) asm volatile("mov x1, %1\n.inst " __stringify(INST) "\nmov %0, x0" : "=&r"(VALUE) : "r"(ADDR) : "memory", "x0", "x1")
#define EMU_HW_ORDERED_ST(INST, VALUE, ADDR)  asm volatile("mov w0, %w0\nmov x1, %1\n.inst " __stringify(INST) : : "r"(VALUE), "r"(ADDR) : "memory", "x0", "x1")
#define EMU_HW_ORDERED_STX(INST, VALUE, ADDR) asm volatile("mov x0, %0\nmov x1, %1\n.inst " __stringify(INST) : : "r"(VALUE), "r"(ADDR) : "memory", "x0", "x1")

static inline bool emu_hw_ordered_load(uint64_t addr, int bytes, bool acquire, uint64_t *out)
{
    uint64_t value;

    if (bytes == 1 && acquire) EMU_HW_ORDERED_LD(0x08DFFC20, value, addr);
    else if (bytes == 1) EMU_HW_ORDERED_LD(0x08DF7C20, value, addr);
    else if (bytes == 2 && acquire) EMU_HW_ORDERED_LD(0x48DFFC20, value, addr);
    else if (bytes == 2) EMU_HW_ORDERED_LD(0x48DF7C20, value, addr);
    else if (bytes == 4 && acquire) EMU_HW_ORDERED_LD(0x88DFFC20, value, addr);
    else if (bytes == 4) EMU_HW_ORDERED_LD(0x88DF7C20, value, addr);
    else if (bytes == 8 && acquire) EMU_HW_ORDERED_LDX(0xC8DFFC20, value, addr);
    else if (bytes == 8) EMU_HW_ORDERED_LDX(0xC8DF7C20, value, addr);
    else return false;

    *out = value;
    return true;
}

static inline bool emu_hw_ordered_store(uint64_t addr, int bytes, bool release, uint64_t value)
{
    if (bytes == 1 && release) EMU_HW_ORDERED_ST(0x089FFC20, value, addr);
    else if (bytes == 1) EMU_HW_ORDERED_ST(0x089F7C20, value, addr);
    else if (bytes == 2 && release) EMU_HW_ORDERED_ST(0x489FFC20, value, addr);
    else if (bytes == 2) EMU_HW_ORDERED_ST(0x489F7C20, value, addr);
    else if (bytes == 4 && release) EMU_HW_ORDERED_ST(0x889FFC20, value, addr);
    else if (bytes == 4) EMU_HW_ORDERED_ST(0x889F7C20, value, addr);
    else if (bytes == 8 && release) EMU_HW_ORDERED_STX(0xC89FFC20, value, addr);
    else if (bytes == 8) EMU_HW_ORDERED_STX(0xC89F7C20, value, addr);
    else return false;
    return true;
}

static inline bool emu_hw_exclusive_load(uint64_t addr, int bytes, bool pair, bool acquire, uint64_t *first, uint64_t *second)
{
    uint64_t value0, value1 = 0;

    if (pair)
    {
        if (bytes == 4 && acquire) asm volatile("ldaxp %w0, %w1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory");
        else if (bytes == 4) asm volatile("ldxp %w0, %w1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory");
        else if (bytes == 8 && acquire) asm volatile("ldaxp %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory");
        else if (bytes == 8) asm volatile("ldxp %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory");
        else return false;
    }
    else if (bytes == 1 && acquire) EMU_HW_LDW("ldaxrb", value0, addr);
    else if (bytes == 1) EMU_HW_LDW("ldxrb", value0, addr);
    else if (bytes == 2 && acquire) EMU_HW_LDW("ldaxrh", value0, addr);
    else if (bytes == 2) EMU_HW_LDW("ldxrh", value0, addr);
    else if (bytes == 4 && acquire) EMU_HW_LDW("ldaxr", value0, addr);
    else if (bytes == 4) EMU_HW_LDW("ldxr", value0, addr);
    else if (bytes == 8 && acquire) EMU_HW_LD("ldaxr", value0, addr);
    else if (bytes == 8) EMU_HW_LD("ldxr", value0, addr);
    else return false;

    *first = value0;
    if (pair) *second = value1;
    return true;
}

static inline bool emu_hw_exclusive_store(uint64_t addr, int bytes, bool pair, bool release, uint64_t first, uint64_t second, uint32_t *status)
{
    uint32_t result;

    if (pair)
    {
        if (bytes == 4 && release) asm volatile("stlxp %w0, %w2, %w3, [%1]" : "=&r"(result) : "r"(addr), "r"(first), "r"(second) : "memory");
        else if (bytes == 4) asm volatile("stxp %w0, %w2, %w3, [%1]" : "=&r"(result) : "r"(addr), "r"(first), "r"(second) : "memory");
        else if (bytes == 8 && release) asm volatile("stlxp %w0, %2, %3, [%1]" : "=&r"(result) : "r"(addr), "r"(first), "r"(second) : "memory");
        else if (bytes == 8) asm volatile("stxp %w0, %2, %3, [%1]" : "=&r"(result) : "r"(addr), "r"(first), "r"(second) : "memory");
        else return false;
    }
    else if (bytes == 1 && release) asm volatile("stlxrb %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory");
    else if (bytes == 1) asm volatile("stxrb %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory");
    else if (bytes == 2 && release) asm volatile("stlxrh %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory");
    else if (bytes == 2) asm volatile("stxrh %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory");
    else if (bytes == 4 && release) asm volatile("stlxr %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory");
    else if (bytes == 4) asm volatile("stxr %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory");
    else if (bytes == 8 && release) asm volatile("stlxr %w0, %2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory");
    else if (bytes == 8) asm volatile("stxr %w0, %2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory");
    else return false;

    *status = result;
    return true;
}

/* ======================== 访存类：完整执行流程 ======================== */

static inline enum emu_insn_result emu_simulate_load_store_insn(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_decoded_insn *decoded, uint64_t pc)
{
    const struct arm64_load_store_operands *operands = &decoded->operands.load_store;
    uint32_t flags = decoded->flags;
    bool acquire = (flags & ARM64_INSN_FLAG_ACQUIRE) != 0;
    bool release = (flags & ARM64_INSN_FLAG_RELEASE) != 0;
    bool is_load = (flags & ARM64_INSN_FLAG_LOAD) != 0;
    bool sign_extend = (flags & ARM64_INSN_FLAG_SIGN_EXTEND) != 0;
    bool is_fp = (flags & ARM64_INSN_FLAG_FP) != 0;
    bool sf = decoded->operand_width == 64;
    int bytes = operands->access_bytes;

    /* LSE atomic read-modify-write. */
    if (decoded->opcode == ARM64_OP_ATOMIC_RMW)
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        uint64_t old;

        if (!emu_hw_atomic_rmw(decoded->operation, addr, bytes, acquire, release, reg_read(regs, decoded->rs), &old)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rt, old, sf);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    /* LSE compare-and-swap. */
    if (decoded->opcode == ARM64_OP_CAS)
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        uint64_t expected = reg_read(regs, decoded->rs);

        if (!emu_hw_cas(addr, bytes, acquire, release, reg_read(regs, decoded->rt), &expected)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rs, expected, sf);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    /* LSE pair compare-and-swap. */
    if (decoded->opcode == ARM64_OP_CASP)
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        uint64_t expected0 = reg_read(regs, decoded->rs);
        uint64_t expected1 = reg_read(regs, decoded->rs + 1);

        if (!emu_hw_casp(addr, bytes, acquire, release, reg_read(regs, decoded->rt), reg_read(regs, decoded->rt + 1), &expected0, &expected1)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rs, expected0, sf);
        reg_write(regs, decoded->rs + 1, expected1, sf);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    /* Ordered and exclusive accesses share the architectural encoding family. */
    if (decoded->opcode == ARM64_OP_EXCLUSIVE)
    {
        bool ordered = (flags & ARM64_INSN_FLAG_ORDERED) != 0;
        bool pair = (flags & ARM64_INSN_FLAG_PAIR) != 0;
        uint64_t addr = addr_reg_read(regs, decoded->rn);

        if (ordered)
        {
            if (is_load)
            {
                uint64_t value;

                if (!emu_hw_ordered_load(addr, bytes, acquire, &value)) return EMU_INSN_SKIP;
                reg_write(regs, decoded->rt, value, sf);
            }
            else if (!emu_hw_ordered_store(addr, bytes, release, reg_read(regs, decoded->rt))) return EMU_INSN_SKIP;
        }
        else if (is_load)
        {
            uint64_t value0, value1;

            if (!emu_hw_exclusive_load(addr, bytes, pair, acquire, &value0, &value1)) return EMU_INSN_SKIP;
            reg_write(regs, decoded->rt, value0, sf);
            if (pair) reg_write(regs, decoded->rt2, value1, sf);
        }
        else
        {
            uint32_t status;

            if (!emu_hw_exclusive_store(addr, bytes, pair, release, reg_read(regs, decoded->rt), pair ? reg_read(regs, decoded->rt2) : 0, &status)) return EMU_INSN_SKIP;
            reg_write(regs, decoded->rs, status, false);
        }

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    /* RCpc unscaled load/store. */
    if (decoded->opcode == ARM64_OP_RCPC_UNSCALED)
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn) + operands->offset;
        bool is_store = (flags & ARM64_INSN_FLAG_STORE) != 0;

        if (is_store)
        {
            if (!emu_hw_store_rcpc(addr, bytes, reg_read(regs, decoded->rt))) return EMU_INSN_SKIP;
        }
        else
        {
            uint64_t value;

            if (!emu_hw_load_rcpc(addr, bytes, sign_extend, sf, &value)) return EMU_INSN_SKIP;
            reg_write(regs, decoded->rt, value, sf);
        }
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    /* RCpc acquire load. */
    if (decoded->opcode == ARM64_OP_LDAPR)
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        uint64_t value;

        if (!emu_hw_load_ldapr(addr, bytes, &value)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, value, sf);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_PREFETCH || decoded->opcode == ARM64_OP_PREFETCH_LITERAL) return EMU_INSN_NOP;

    /* Literal load. */
    if (decoded->opcode == ARM64_OP_LOAD_LITERAL)
    {
        struct emu_memory_address memory_address;
        uint64_t addr;

        if (!emu_resolve_memory_address(operands, pc, 0, 0, &memory_address)) return EMU_INSN_SKIP;
        addr = memory_address.address;

        if (is_fp)
        {
            if (!emu_hw_load_fp(addr, bytes, &fp_regs->q[decoded->rt])) return EMU_INSN_SKIP;
        }
        else
        {
            uint64_t value;

            if (!emu_hw_load_gpr(addr, bytes, sign_extend, false, sf, &value)) return EMU_INSN_SKIP;
            reg_write(regs, decoded->rt, value, sf);
        }

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    /* Pair load/store. */
    if (decoded->opcode == ARM64_OP_LOAD_STORE_PAIR)
    {
        struct emu_memory_address memory_address;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t addr;
        bool non_temporal = (flags & ARM64_INSN_FLAG_NON_TEMPORAL) != 0;

        if (!emu_resolve_memory_address(operands, pc, base, 0, &memory_address)) return EMU_INSN_SKIP;
        addr = memory_address.address;

        if (is_load)
        {
            if (is_fp)
            {
                if (!emu_hw_load_pair_fp(addr, bytes, non_temporal, &fp_regs->q[decoded->rt], &fp_regs->q[decoded->rt2])) return EMU_INSN_SKIP;
            }
            else
            {
                uint64_t value0, value1;

                if (!emu_hw_load_pair_gpr(addr, bytes, sign_extend, non_temporal, &value0, &value1)) return EMU_INSN_SKIP;
                reg_write(regs, decoded->rt, value0, sf);
                reg_write(regs, decoded->rt2, value1, sf);
            }
        }
        else
        {
            if (is_fp)
            {
                if (!emu_hw_store_pair_fp(addr, bytes, non_temporal, &fp_regs->q[decoded->rt], &fp_regs->q[decoded->rt2])) return EMU_INSN_SKIP;
            }
            else if (!emu_hw_store_pair_gpr(addr, bytes, non_temporal, reg_read(regs, decoded->rt), reg_read(regs, decoded->rt2))) return EMU_INSN_SKIP;
        }
        if (memory_address.writeback) addr_reg_write(regs, decoded->rn, memory_address.writeback_address);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode != ARM64_OP_LOAD_STORE_SINGLE) return EMU_INSN_SKIP;

    /* Single-register load/store. */
    {
        struct emu_memory_address memory_address;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t index = operands->address_mode == ARM64_ADDRESS_REGISTER_OFFSET ? reg_read(regs, decoded->rm) : 0;
        uint64_t addr;
        bool unprivileged = (flags & ARM64_INSN_FLAG_UNPRIVILEGED) != 0;

        if (!emu_resolve_memory_address(operands, pc, base, index, &memory_address)) return EMU_INSN_SKIP;
        addr = memory_address.address;

        if (is_load)
        {
            if (is_fp)
            {
                if (!emu_hw_load_fp(addr, bytes, &fp_regs->q[decoded->rt])) return EMU_INSN_SKIP;
            }
            else
            {
                uint64_t value;

                if (!emu_hw_load_gpr(addr, bytes, sign_extend, unprivileged, sf, &value)) return EMU_INSN_SKIP;
                reg_write(regs, decoded->rt, value, sf);
            }
        }
        else
        {
            if (is_fp)
            {
                if (!emu_hw_store_fp(addr, bytes, &fp_regs->q[decoded->rt])) return EMU_INSN_SKIP;
            }
            else if (!emu_hw_store_gpr(addr, bytes, unprivileged, reg_read(regs, decoded->rt))) return EMU_INSN_SKIP;
        }
        if (memory_address.writeback) addr_reg_write(regs, decoded->rn, memory_address.writeback_address);
    }

    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

/* ======================== FP / AdvSIMD：私有硬件模板 ======================== */

/* 局部缩写只负责装载现场、执行同语义指令并写回结果。 */
#define EMU_FP_BIN(INST, DST, A, B)                               \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "ldr q1, [%1]\n"                             \
                     "ldr q2, [%2]\n" INST "\n"                   \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(DST), "r"(A), "r"(B)                   \
                     : "memory", "v0", "v1", "v2");               \
    } while (0)

#define EMU_FP_UN(INST, DST, A)                                   \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "ldr q1, [%1]\n" INST "\n"                   \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(DST), "r"(A)                           \
                     : "memory", "v0", "v1");                     \
    } while (0)

#define EMU_FP_UN_MERGE(INST, DST, A)                             \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "ldr q0, [%0]\n"                             \
                     "ldr q1, [%1]\n" INST "\n"                   \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(DST), "r"(A)                           \
                     : "memory", "v0", "v1");                     \
    } while (0)

#define EMU_GPR_TO_FP_MERGE(INST, DST, VALUE)                     \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "ldr q0, [%0]\n" INST "\n"                   \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(DST), "r"(VALUE)                       \
                     : "memory", "v0");                           \
    } while (0)

#define EMU_FP_CONVERT_SIMD(INST, DST, A, WIDTH, ELEMENT_WIDTH)                                    \
    do                                                                                             \
    {                                                                                              \
        if ((WIDTH) == 32 && (ELEMENT_WIDTH) == 32) EMU_FP_UN_MERGE(INST " s0, s1", DST, A);       \
        else if ((WIDTH) == 64 && (ELEMENT_WIDTH) == 64) EMU_FP_UN_MERGE(INST " d0, d1", DST, A);  \
        else if ((WIDTH) == 64 && (ELEMENT_WIDTH) == 32) EMU_FP_UN(INST " v0.2s, v1.2s", DST, A);  \
        else if ((WIDTH) == 128 && (ELEMENT_WIDTH) == 32) EMU_FP_UN(INST " v0.4s, v1.4s", DST, A); \
        else if ((WIDTH) == 128 && (ELEMENT_WIDTH) == 64) EMU_FP_UN(INST " v0.2d, v1.2d", DST, A); \
        else return EMU_INSN_SKIP;                                                                 \
    } while (0)

#define EMU_FP_CONVERT_GPR(INST)                                                                                                                             \
    do                                                                                                                                                       \
    {                                                                                                                                                        \
        if (decoded->operand_width == 32 && operands->element_width == 32)                                                                                   \
        {                                                                                                                                                    \
            asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n" INST " %w0, s1\n" : "=r"(wout) : "r"(&fp_regs->q[rn]) : "memory", "v1"); \
            reg_write(regs, rd, wout, false);                                                                                                                \
        }                                                                                                                                                    \
        else if (decoded->operand_width == 64 && operands->element_width == 32)                                                                              \
        {                                                                                                                                                    \
            asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n" INST " %0, s1\n" : "=r"(xout) : "r"(&fp_regs->q[rn]) : "memory", "v1");  \
            reg_write(regs, rd, xout, true);                                                                                                                 \
        }                                                                                                                                                    \
        else if (decoded->operand_width == 32 && operands->element_width == 64)                                                                              \
        {                                                                                                                                                    \
            asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n" INST " %w0, d1\n" : "=r"(wout) : "r"(&fp_regs->q[rn]) : "memory", "v1"); \
            reg_write(regs, rd, wout, false);                                                                                                                \
        }                                                                                                                                                    \
        else if (decoded->operand_width == 64 && operands->element_width == 64)                                                                              \
        {                                                                                                                                                    \
            asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n" INST " %0, d1\n" : "=r"(xout) : "r"(&fp_regs->q[rn]) : "memory", "v1");  \
            reg_write(regs, rd, xout, true);                                                                                                                 \
        }                                                                                                                                                    \
        else return EMU_INSN_SKIP;                                                                                                                           \
    } while (0)

#define EMU_FP_TERN(INST, DST, A, B, C)                           \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "ldr q1, [%1]\n"                             \
                     "ldr q2, [%2]\n"                             \
                     "ldr q3, [%3]\n" INST "\n"                   \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(DST), "r"(A), "r"(B), "r"(C)           \
                     : "memory", "v0", "v1", "v2", "v3");         \
    } while (0)

#define EMU_VEC_ACC(INST, DST, A, B)                              \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "ldr q0, [%0]\n"                             \
                     "ldr q1, [%1]\n"                             \
                     "ldr q2, [%2]\n" INST "\n"                   \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(DST), "r"(A), "r"(B)                   \
                     : "memory", "v0", "v1", "v2");               \
    } while (0)

static inline bool emu_fp_select_hw(void *dst, const void *left, const void *right, uint64_t pstate, uint32_t condition, uint32_t width)
{
    uint32_t take = emu_cond_holds_hw(pstate, condition);

    if (width == 16)
    {
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "ldr q1, [%1]\n"
                     "ldr q2, [%2]\n"
                     "cmp %w3, #0\n"
                     ".inst " __stringify(0x1EE21C20) "\n"
                                                      "str q0, [%0]\n"
                     :
                     : "r"(dst), "r"(left), "r"(right), "r"(take)
                     : "memory", "cc", "v0", "v1", "v2");
        return true;
    }
    if (width == 32)
    {
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "ldr q1, [%1]\n"
                     "ldr q2, [%2]\n"
                     "cmp %w3, #0\n"
                     "fcsel s0, s1, s2, ne\n"
                     "str q0, [%0]\n"
                     :
                     : "r"(dst), "r"(left), "r"(right), "r"(take)
                     : "memory", "cc", "v0", "v1", "v2");
        return true;
    }
    if (width == 64)
    {
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "ldr q1, [%1]\n"
                     "ldr q2, [%2]\n"
                     "cmp %w3, #0\n"
                     "fcsel d0, d1, d2, ne\n"
                     "str q0, [%0]\n"
                     :
                     : "r"(dst), "r"(left), "r"(right), "r"(take)
                     : "memory", "cc", "v0", "v1", "v2");
        return true;
    }

    return false;
}

static inline bool emu_simd_extract_lane_hw(const void *source, uint32_t element_width, uint32_t lane, uint64_t *value)
{
    uint32_t value32;

    if (!value) return false;

    switch (element_width)
    {
    case 8:
        if (lane >= 16) return false;
        asm volatile("ldrb %w0, [%1, %w2, uxtw]\n" : "=r"(value32) : "r"(source), "r"(lane) : "memory");
        break;
    case 16:
        if (lane >= 8) return false;
        asm volatile("ldrh %w0, [%1, %w2, uxtw #1]\n" : "=r"(value32) : "r"(source), "r"(lane) : "memory");
        break;
    case 32:
        if (lane >= 4) return false;
        asm volatile("ldr %w0, [%1, %w2, uxtw #2]\n" : "=r"(value32) : "r"(source), "r"(lane) : "memory");
        break;
    case 64:
        if (lane >= 2) return false;
        asm volatile("ldr %0, [%1, %w2, uxtw #3]\n" : "=r"(*value) : "r"(source), "r"(lane) : "memory");
        return true;
    default:
        return false;
    }

    *value = value32;
    return true;
}

static inline bool emu_simd_extract_signed_lane_hw(const void *source, uint32_t element_width, uint32_t lane, bool sf, uint64_t *value)
{
    uint32_t value32;
    uint64_t value64;

    if (!value) return false;

    if (!sf)
    {
        switch (element_width)
        {
        case 8:
            if (lane >= 16) return false;
            asm volatile("ldrsb %w0, [%1, %w2, uxtw]\n" : "=r"(value32) : "r"(source), "r"(lane) : "memory");
            break;
        case 16:
            if (lane >= 8) return false;
            asm volatile("ldrsh %w0, [%1, %w2, uxtw #1]\n" : "=r"(value32) : "r"(source), "r"(lane) : "memory");
            break;
        default:
            return false;
        }

        *value = value32;
        return true;
    }

    switch (element_width)
    {
    case 8:
        if (lane >= 16) return false;
        asm volatile("ldrsb %0, [%1, %w2, uxtw]\n" : "=r"(value64) : "r"(source), "r"(lane) : "memory");
        break;
    case 16:
        if (lane >= 8) return false;
        asm volatile("ldrsh %0, [%1, %w2, uxtw #1]\n" : "=r"(value64) : "r"(source), "r"(lane) : "memory");
        break;
    case 32:
        if (lane >= 4) return false;
        asm volatile("ldrsw %0, [%1, %w2, uxtw #2]\n" : "=r"(value64) : "r"(source), "r"(lane) : "memory");
        break;
    default:
        return false;
    }

    *value = value64;
    return true;
}

static inline bool emu_simd_insert_general_hw(void *dst, uint64_t value, uint32_t element_width, uint32_t lane)
{
    switch (element_width)
    {
    case 8:
        if (lane >= 16) return false;
        asm volatile("strb %w1, [%0, %w2, uxtw]\n" : : "r"(dst), "r"((uint32_t)value), "r"(lane) : "memory");
        return true;
    case 16:
        if (lane >= 8) return false;
        asm volatile("strh %w1, [%0, %w2, uxtw #1]\n" : : "r"(dst), "r"((uint32_t)value), "r"(lane) : "memory");
        return true;
    case 32:
        if (lane >= 4) return false;
        asm volatile("str %w1, [%0, %w2, uxtw #2]\n" : : "r"(dst), "r"((uint32_t)value), "r"(lane) : "memory");
        return true;
    case 64:
        if (lane >= 2) return false;
        asm volatile("str %1, [%0, %w2, uxtw #3]\n" : : "r"(dst), "r"(value), "r"(lane) : "memory");
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_write_scalar_hw(void *dst, uint64_t value, uint32_t width)
{
    if (width == 8 || width == 16 || width == 32)
    {
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "fmov s0, %w1\n"
                     "str q0, [%0]\n"
                     :
                     : "r"(dst), "r"((uint32_t)value)
                     : "memory", "v0");
        return true;
    }
    if (width == 64)
    {
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "fmov d0, %1\n"
                     "str q0, [%0]\n"
                     :
                     : "r"(dst), "r"(value)
                     : "memory", "v0");
        return true;
    }
    return false;
}

static inline bool emu_simd_read_scalar_hw(const void *source, uint32_t width, uint64_t *value)
{
    uint32_t value32;

    if (!value) return false;
    if (width == 32)
    {
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "ldr q1, [%1]\n"
                     "fmov %w0, s1\n"
                     : "=r"(value32)
                     : "r"(source)
                     : "memory", "v1");
        *value = value32;
        return true;
    }
    if (width == 64)
    {
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "ldr q1, [%1]\n"
                     "fmov %0, d1\n"
                     : "=r"(*value)
                     : "r"(source)
                     : "memory", "v1");
        return true;
    }
    return false;
}

#define EMU_SIMD_FP_BY_ELEMENT_EXEC(DST_ARR, SRC_ARR)                       \
    do                                                                      \
    {                                                                       \
        switch (operation)                                                  \
        {                                                                   \
        case ARM64_SIMD_OP_FMLA:                                            \
            EMU_VEC_ACC("fmla " DST_ARR ", " SRC_ARR, dst, left, &element); \
            break;                                                          \
        case ARM64_SIMD_OP_FMLS:                                            \
            EMU_VEC_ACC("fmls " DST_ARR ", " SRC_ARR, dst, left, &element); \
            break;                                                          \
        case ARM64_SIMD_OP_FMUL:                                            \
            EMU_FP_BIN("fmul " DST_ARR ", " SRC_ARR, dst, left, &element);  \
            break;                                                          \
        case ARM64_SIMD_OP_FMULX:                                           \
            EMU_FP_BIN("fmulx " DST_ARR ", " SRC_ARR, dst, left, &element); \
            break;                                                          \
        default:                                                            \
            return false;                                                   \
        }                                                                   \
    } while (0)

#define EMU_SIMD_FP16_BY_ELEMENT_INST(INSTRUCTION)                      \
    do                                                                  \
    {                                                                   \
        asm volatile(".arch_extension fp\n"                             \
                     ".arch_extension simd\n"                           \
                     "ldr q0, [%0]\n"                                   \
                     "ldr q1, [%1]\n"                                   \
                     "ldr q2, [%2]\n"                                   \
                     ".inst " __stringify(INSTRUCTION) "\n"             \
                                                       "str q0, [%0]\n" \
                     :                                                  \
                     : "r"(dst), "r"(left), "r"(&element)               \
                     : "memory", "v0", "v1", "v2");                     \
    } while (0)

#define EMU_SIMD_FP16_BY_ELEMENT_EXEC(FMLA_INST, FMLS_INST, FMUL_INST, FMULX_INST) \
    do                                                                             \
    {                                                                              \
        switch (operation)                                                         \
        {                                                                          \
        case ARM64_SIMD_OP_FMLA:                                                   \
            EMU_SIMD_FP16_BY_ELEMENT_INST(FMLA_INST);                              \
            break;                                                                 \
        case ARM64_SIMD_OP_FMLS:                                                   \
            EMU_SIMD_FP16_BY_ELEMENT_INST(FMLS_INST);                              \
            break;                                                                 \
        case ARM64_SIMD_OP_FMUL:                                                   \
            EMU_SIMD_FP16_BY_ELEMENT_INST(FMUL_INST);                              \
            break;                                                                 \
        case ARM64_SIMD_OP_FMULX:                                                  \
            EMU_SIMD_FP16_BY_ELEMENT_INST(FMULX_INST);                             \
            break;                                                                 \
        default:                                                                   \
            return false;                                                          \
        }                                                                          \
    } while (0)

#define EMU_SIMD_DUP_GENERAL_EXEC(ARR, VALUE)                     \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "dup v0." ARR ", " VALUE "\n"                \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(dst), "r"(value)                       \
                     : "memory", "v0");                           \
        return true;                                              \
    } while (0)

static inline bool emu_simd_dup_general_hw(void *dst, uint64_t value, uint32_t element_width, uint32_t vector_width)
{
    if (vector_width == 64)
    {
        switch (element_width)
        {
        case 8:
            EMU_SIMD_DUP_GENERAL_EXEC("8b", "%w1");
        case 16:
            EMU_SIMD_DUP_GENERAL_EXEC("4h", "%w1");
        case 32:
            EMU_SIMD_DUP_GENERAL_EXEC("2s", "%w1");
        default:
            return false;
        }
    }

    switch (element_width)
    {
    case 8:
        EMU_SIMD_DUP_GENERAL_EXEC("16b", "%w1");
    case 16:
        EMU_SIMD_DUP_GENERAL_EXEC("8h", "%w1");
    case 32:
        EMU_SIMD_DUP_GENERAL_EXEC("4s", "%w1");
    case 64:
        EMU_SIMD_DUP_GENERAL_EXEC("2d", "%1");
    default:
        return false;
    }
}

static inline bool emu_simd_fp_by_element_hw(enum arm64_simd_operation operation, void *dst, const void *left, uint64_t lane_value, uint32_t element_width, uint32_t operand_width)
{
    __uint128_t element;

    if (operand_width == element_width)
    {
        if (!emu_simd_write_scalar_hw(&element, lane_value, element_width)) return false;
        if (element_width == 16) EMU_SIMD_FP16_BY_ELEMENT_EXEC(0x5F021020, 0x5F025020, 0x5F029020, 0x7F029020);
        else if (element_width == 32) EMU_SIMD_FP_BY_ELEMENT_EXEC("s0, s1", "v2.s[0]");
        else if (element_width == 64) EMU_SIMD_FP_BY_ELEMENT_EXEC("d0, d1", "v2.d[0]");
        else return false;
    }
    else
    {
        if (!emu_simd_dup_general_hw(&element, lane_value, element_width, operand_width)) return false;
        if (operand_width == 64 && element_width == 16) EMU_SIMD_FP16_BY_ELEMENT_EXEC(0x0E420C20, 0x0EC20C20, 0x2E421C20, 0x0E421C20);
        else if (operand_width == 128 && element_width == 16) EMU_SIMD_FP16_BY_ELEMENT_EXEC(0x4E420C20, 0x4EC20C20, 0x6E421C20, 0x4E421C20);
        else if (operand_width == 64 && element_width == 32) EMU_SIMD_FP_BY_ELEMENT_EXEC("v0.2s, v1.2s", "v2.2s");
        else if (operand_width == 128 && element_width == 32) EMU_SIMD_FP_BY_ELEMENT_EXEC("v0.4s, v1.4s", "v2.4s");
        else if (operand_width == 128 && element_width == 64) EMU_SIMD_FP_BY_ELEMENT_EXEC("v0.2d, v1.2d", "v2.2d");
        else return false;
    }

    return true;
}

static inline bool emu_simd_materialize_bits_hw(void *dst, uint64_t value, uint32_t vector_width)
{
    if (vector_width == 64) return emu_simd_write_scalar_hw(dst, value, 64);
    if (vector_width == 128) return emu_simd_dup_general_hw(dst, value, 64, 128);
    return false;
}

enum emu_simd_cpu_feature
{
    EMU_SIMD_CPU_FEATURE_RDM,
    EMU_SIMD_CPU_FEATURE_DOTPROD,
    EMU_SIMD_CPU_FEATURE_FHM,
    EMU_SIMD_CPU_FEATURE_FCMA,
    EMU_SIMD_CPU_FEATURE_BF16,
    EMU_SIMD_CPU_FEATURE_I8MM,
};

static inline bool emu_simd_current_cpu_has_feature(enum emu_simd_cpu_feature feature)
{
    uint64_t value;
    uint32_t shift;

    switch (feature)
    {
    case EMU_SIMD_CPU_FEATURE_RDM:
        value = read_sysreg(id_aa64isar0_el1);
        shift = 28;
        break;
    case EMU_SIMD_CPU_FEATURE_DOTPROD:
        value = read_sysreg(id_aa64isar0_el1);
        shift = 44;
        break;
    case EMU_SIMD_CPU_FEATURE_FHM:
        value = read_sysreg(id_aa64isar0_el1);
        shift = 48;
        break;
    case EMU_SIMD_CPU_FEATURE_FCMA:
        value = read_sysreg(id_aa64isar1_el1);
        shift = 16;
        break;
    case EMU_SIMD_CPU_FEATURE_BF16:
        value = read_sysreg(id_aa64isar1_el1);
        shift = 44;
        break;
    case EMU_SIMD_CPU_FEATURE_I8MM:
        value = read_sysreg(id_aa64isar1_el1);
        shift = 52;
        break;
    default:
        return false;
    }

    return ((value >> shift) & 0xFULL) >= 1;
}

static inline bool emu_simd_current_cpu_has_fp16(void)
{
    return ((read_sysreg(id_aa64pfr0_el1) >> 20) & 0xFULL) == 1;
}

static inline bool emu_simd_current_cpu_has_faminmax(void)
{
    uint64_t value;

    asm volatile("mrs %0, S3_0_C0_C6_3" : "=r"(value));
    return ((value >> 4) & 0xFULL) >= 1;
}

static inline bool emu_simd_current_cpu_has_f8cvt(void)
{
    uint64_t value;

    asm volatile("mrs %0, S3_0_C0_C4_7" : "=r"(value));
    return value & (1ULL << 31);
}

static inline bool emu_fp16_scalar_2source_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_FMUL:
        EMU_FP_BIN(".inst " __stringify(0x1EE20820), dst, left, right);
        return true;
    case ARM64_SIMD_OP_FDIV:
        EMU_FP_BIN(".inst " __stringify(0x1EE21820), dst, left, right);
        return true;
    case ARM64_SIMD_OP_FADD:
        EMU_FP_BIN(".inst " __stringify(0x1EE22820), dst, left, right);
        return true;
    case ARM64_SIMD_OP_FSUB:
        EMU_FP_BIN(".inst " __stringify(0x1EE23820), dst, left, right);
        return true;
    case ARM64_SIMD_OP_FMAX:
        EMU_FP_BIN(".inst " __stringify(0x1EE24820), dst, left, right);
        return true;
    case ARM64_SIMD_OP_FMIN:
        EMU_FP_BIN(".inst " __stringify(0x1EE25820), dst, left, right);
        return true;
    case ARM64_SIMD_OP_FMAXNM:
        EMU_FP_BIN(".inst " __stringify(0x1EE26820), dst, left, right);
        return true;
    case ARM64_SIMD_OP_FMINNM:
        EMU_FP_BIN(".inst " __stringify(0x1EE27820), dst, left, right);
        return true;
    case ARM64_SIMD_OP_FNMUL:
        EMU_FP_BIN(".inst " __stringify(0x1EE28820), dst, left, right);
        return true;
    default:
        return false;
    }
}

static inline bool emu_fp16_scalar_1source_hw(enum arm64_simd_operation operation, enum arm64_fp_rounding_mode rounding_mode, void *dst, const void *source)
{
    if (operation != ARM64_SIMD_OP_FRINT)
    {
        switch (operation)
        {
        case ARM64_SIMD_OP_FMOV:
            EMU_FP_UN(".inst " __stringify(0x1EE04020), dst, source);
            return true;
        case ARM64_SIMD_OP_FABS:
            EMU_FP_UN_MERGE(".inst " __stringify(0x1EE0C020), dst, source);
            return true;
        case ARM64_SIMD_OP_FNEG:
            EMU_FP_UN_MERGE(".inst " __stringify(0x1EE14020), dst, source);
            return true;
        case ARM64_SIMD_OP_FSQRT:
            EMU_FP_UN_MERGE(".inst " __stringify(0x1EE1C020), dst, source);
            return true;
        default:
            return false;
        }
    }

    switch (rounding_mode)
    {
    case ARM64_FP_ROUND_NEAREST_EVEN:
        EMU_FP_UN_MERGE(".inst " __stringify(0x1EE44020), dst, source);
        return true;
    case ARM64_FP_ROUND_PLUS_INFINITY:
        EMU_FP_UN_MERGE(".inst " __stringify(0x1EE4C020), dst, source);
        return true;
    case ARM64_FP_ROUND_MINUS_INFINITY:
        EMU_FP_UN_MERGE(".inst " __stringify(0x1EE54020), dst, source);
        return true;
    case ARM64_FP_ROUND_ZERO:
        EMU_FP_UN_MERGE(".inst " __stringify(0x1EE5C020), dst, source);
        return true;
    case ARM64_FP_ROUND_NEAREST_AWAY:
        EMU_FP_UN_MERGE(".inst " __stringify(0x1EE64020), dst, source);
        return true;
    case ARM64_FP_ROUND_CURRENT_EXACT:
        EMU_FP_UN_MERGE(".inst " __stringify(0x1EE74020), dst, source);
        return true;
    case ARM64_FP_ROUND_CURRENT:
        EMU_FP_UN_MERGE(".inst " __stringify(0x1EE7C020), dst, source);
        return true;
    default:
        return false;
    }
}

static inline bool emu_fp16_scalar_3source_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, const void *addend)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_FMADD:
        EMU_FP_TERN(".inst " __stringify(0x1FC20C20), dst, left, right, addend);
        return true;
    case ARM64_SIMD_OP_FMSUB:
        EMU_FP_TERN(".inst " __stringify(0x1FC28C20), dst, left, right, addend);
        return true;
    case ARM64_SIMD_OP_FNMADD:
        EMU_FP_TERN(".inst " __stringify(0x1FE20C20), dst, left, right, addend);
        return true;
    case ARM64_SIMD_OP_FNMSUB:
        EMU_FP_TERN(".inst " __stringify(0x1FE28C20), dst, left, right, addend);
        return true;
    default:
        return false;
    }
}

static inline bool emu_fp16_compare_hw(bool signal, bool zero, const void *left, const void *right, uint64_t *nzcv)
{
    if (!nzcv) return false;

    if (zero)
    {
        if (signal) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n.inst " __stringify(0x1EE02038) "\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left) : "memory", "cc", "v1");
        else asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n.inst " __stringify(0x1EE02028) "\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left) : "memory", "cc", "v1");
        return true;
    }

    if (signal) asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\nldr q2, [%2]\n.inst " __stringify(0x1EE22030) "\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left), "r"(right) : "memory", "cc", "v1", "v2");
    else asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\nldr q2, [%2]\n.inst " __stringify(0x1EE22020) "\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left), "r"(right) : "memory", "cc", "v1", "v2");
    return true;
}

#define EMU_SIMD_RDM_VECTOR_EXEC(V4H_INST, V8H_INST, V2S_INST, V4S_INST)                                                    \
    do                                                                                                                      \
    {                                                                                                                       \
        if (vector_width == 64 && element_width == 16) EMU_VEC_ACC(".inst " __stringify(V4H_INST), dst, left, right);       \
        else if (vector_width == 128 && element_width == 16) EMU_VEC_ACC(".inst " __stringify(V8H_INST), dst, left, right); \
        else if (vector_width == 64 && element_width == 32) EMU_VEC_ACC(".inst " __stringify(V2S_INST), dst, left, right);  \
        else if (vector_width == 128 && element_width == 32) EMU_VEC_ACC(".inst " __stringify(V4S_INST), dst, left, right); \
        else return false;                                                                                                  \
        return true;                                                                                                        \
    } while (0)

#define EMU_SIMD_VECTOR_3REG_EXEC(INST)                                    \
    do                                                                     \
    {                                                                      \
        if (vector_width == 64)                                            \
        {                                                                  \
            switch (element_width)                                         \
            {                                                              \
            case 8:                                                        \
                EMU_FP_BIN(INST " v0.8b, v1.8b, v2.8b", dst, left, right); \
                return true;                                               \
            case 16:                                                       \
                EMU_FP_BIN(INST " v0.4h, v1.4h, v2.4h", dst, left, right); \
                return true;                                               \
            case 32:                                                       \
                EMU_FP_BIN(INST " v0.2s, v1.2s, v2.2s", dst, left, right); \
                return true;                                               \
            default:                                                       \
                return false;                                              \
            }                                                              \
        }                                                                  \
        switch (element_width)                                             \
        {                                                                  \
        case 8:                                                            \
            EMU_FP_BIN(INST " v0.16b, v1.16b, v2.16b", dst, left, right);  \
            return true;                                                   \
        case 16:                                                           \
            EMU_FP_BIN(INST " v0.8h, v1.8h, v2.8h", dst, left, right);     \
            return true;                                                   \
        case 32:                                                           \
            EMU_FP_BIN(INST " v0.4s, v1.4s, v2.4s", dst, left, right);     \
            return true;                                                   \
        case 64:                                                           \
            EMU_FP_BIN(INST " v0.2d, v1.2d, v2.2d", dst, left, right);     \
            return true;                                                   \
        default:                                                           \
            return false;                                                  \
        }                                                                  \
    } while (0)

#define EMU_SIMD_VECTOR_3REG_BHS_EXEC(INST)                                \
    do                                                                     \
    {                                                                      \
        if (vector_width == 64)                                            \
        {                                                                  \
            switch (element_width)                                         \
            {                                                              \
            case 8:                                                        \
                EMU_FP_BIN(INST " v0.8b, v1.8b, v2.8b", dst, left, right); \
                return true;                                               \
            case 16:                                                       \
                EMU_FP_BIN(INST " v0.4h, v1.4h, v2.4h", dst, left, right); \
                return true;                                               \
            case 32:                                                       \
                EMU_FP_BIN(INST " v0.2s, v1.2s, v2.2s", dst, left, right); \
                return true;                                               \
            default:                                                       \
                return false;                                              \
            }                                                              \
        }                                                                  \
        switch (element_width)                                             \
        {                                                                  \
        case 8:                                                            \
            EMU_FP_BIN(INST " v0.16b, v1.16b, v2.16b", dst, left, right);  \
            return true;                                                   \
        case 16:                                                           \
            EMU_FP_BIN(INST " v0.8h, v1.8h, v2.8h", dst, left, right);     \
            return true;                                                   \
        case 32:                                                           \
            EMU_FP_BIN(INST " v0.4s, v1.4s, v2.4s", dst, left, right);     \
            return true;                                                   \
        default:                                                           \
            return false;                                                  \
        }                                                                  \
    } while (0)

#define EMU_SIMD_VECTOR_3REG_HS_EXEC(INST)                                                                              \
    do                                                                                                                  \
    {                                                                                                                   \
        if (vector_width == 64 && element_width == 16) EMU_FP_BIN(INST " v0.4h, v1.4h, v2.4h", dst, left, right);       \
        else if (vector_width == 128 && element_width == 16) EMU_FP_BIN(INST " v0.8h, v1.8h, v2.8h", dst, left, right); \
        else if (vector_width == 64 && element_width == 32) EMU_FP_BIN(INST " v0.2s, v1.2s, v2.2s", dst, left, right);  \
        else if (vector_width == 128 && element_width == 32) EMU_FP_BIN(INST " v0.4s, v1.4s, v2.4s", dst, left, right); \
        else return false;                                                                                              \
        return true;                                                                                                    \
    } while (0)

#define EMU_SIMD_VECTOR_3REG_B_EXEC(INST)                                                                                 \
    do                                                                                                                    \
    {                                                                                                                     \
        if (vector_width == 64 && element_width == 8) EMU_FP_BIN(INST " v0.8b, v1.8b, v2.8b", dst, left, right);          \
        else if (vector_width == 128 && element_width == 8) EMU_FP_BIN(INST " v0.16b, v1.16b, v2.16b", dst, left, right); \
        else return false;                                                                                                \
        return true;                                                                                                      \
    } while (0)

#define EMU_SIMD_VECTOR_3REG_ACC_EXEC(INST)                                 \
    do                                                                      \
    {                                                                       \
        if (vector_width == 64)                                             \
        {                                                                   \
            switch (element_width)                                          \
            {                                                               \
            case 8:                                                         \
                EMU_VEC_ACC(INST " v0.8b, v1.8b, v2.8b", dst, left, right); \
                return true;                                                \
            case 16:                                                        \
                EMU_VEC_ACC(INST " v0.4h, v1.4h, v2.4h", dst, left, right); \
                return true;                                                \
            case 32:                                                        \
                EMU_VEC_ACC(INST " v0.2s, v1.2s, v2.2s", dst, left, right); \
                return true;                                                \
            default:                                                        \
                return false;                                               \
            }                                                               \
        }                                                                   \
        switch (element_width)                                              \
        {                                                                   \
        case 8:                                                             \
            EMU_VEC_ACC(INST " v0.16b, v1.16b, v2.16b", dst, left, right);  \
            return true;                                                    \
        case 16:                                                            \
            EMU_VEC_ACC(INST " v0.8h, v1.8h, v2.8h", dst, left, right);     \
            return true;                                                    \
        case 32:                                                            \
            EMU_VEC_ACC(INST " v0.4s, v1.4s, v2.4s", dst, left, right);     \
            return true;                                                    \
        default:                                                            \
            return false;                                                   \
        }                                                                   \
    } while (0)

static inline bool emu_simd_integer_3reg_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width, uint32_t vector_width)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_SHADD:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("shadd");
    case ARM64_SIMD_OP_SQADD:
        EMU_SIMD_VECTOR_3REG_EXEC("sqadd");
    case ARM64_SIMD_OP_SRHADD:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("srhadd");
    case ARM64_SIMD_OP_SHSUB:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("shsub");
    case ARM64_SIMD_OP_SQSUB:
        EMU_SIMD_VECTOR_3REG_EXEC("sqsub");
    case ARM64_SIMD_OP_SSHL:
        EMU_SIMD_VECTOR_3REG_EXEC("sshl");
    case ARM64_SIMD_OP_SQSHL:
        EMU_SIMD_VECTOR_3REG_EXEC("sqshl");
    case ARM64_SIMD_OP_SRSHL:
        EMU_SIMD_VECTOR_3REG_EXEC("srshl");
    case ARM64_SIMD_OP_SQRSHL:
        EMU_SIMD_VECTOR_3REG_EXEC("sqrshl");
    case ARM64_SIMD_OP_SMAX:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("smax");
    case ARM64_SIMD_OP_SMIN:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("smin");
    case ARM64_SIMD_OP_SABD:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("sabd");
    case ARM64_SIMD_OP_SABA:
        EMU_SIMD_VECTOR_3REG_ACC_EXEC("saba");
    case ARM64_SIMD_OP_ADD:
        EMU_SIMD_VECTOR_3REG_EXEC("add");
    case ARM64_SIMD_OP_SUB:
        EMU_SIMD_VECTOR_3REG_EXEC("sub");
    case ARM64_SIMD_OP_CMTST:
        EMU_SIMD_VECTOR_3REG_EXEC("cmtst");
    case ARM64_SIMD_OP_MLA:
        EMU_SIMD_VECTOR_3REG_ACC_EXEC("mla");
    case ARM64_SIMD_OP_MUL:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("mul");
    case ARM64_SIMD_OP_SMAXP:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("smaxp");
    case ARM64_SIMD_OP_SMINP:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("sminp");
    case ARM64_SIMD_OP_SQDMULH:
        EMU_SIMD_VECTOR_3REG_HS_EXEC("sqdmulh");
    case ARM64_SIMD_OP_ADDP:
        EMU_SIMD_VECTOR_3REG_EXEC("addp");
    case ARM64_SIMD_OP_UHADD:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("uhadd");
    case ARM64_SIMD_OP_UQADD:
        EMU_SIMD_VECTOR_3REG_EXEC("uqadd");
    case ARM64_SIMD_OP_URHADD:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("urhadd");
    case ARM64_SIMD_OP_UHSUB:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("uhsub");
    case ARM64_SIMD_OP_UQSUB:
        EMU_SIMD_VECTOR_3REG_EXEC("uqsub");
    case ARM64_SIMD_OP_USHL:
        EMU_SIMD_VECTOR_3REG_EXEC("ushl");
    case ARM64_SIMD_OP_UQSHL:
        EMU_SIMD_VECTOR_3REG_EXEC("uqshl");
    case ARM64_SIMD_OP_URSHL:
        EMU_SIMD_VECTOR_3REG_EXEC("urshl");
    case ARM64_SIMD_OP_UQRSHL:
        EMU_SIMD_VECTOR_3REG_EXEC("uqrshl");
    case ARM64_SIMD_OP_UMAX:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("umax");
    case ARM64_SIMD_OP_UMIN:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("umin");
    case ARM64_SIMD_OP_UABD:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("uabd");
    case ARM64_SIMD_OP_UABA:
        EMU_SIMD_VECTOR_3REG_ACC_EXEC("uaba");
    case ARM64_SIMD_OP_MLS:
        EMU_SIMD_VECTOR_3REG_ACC_EXEC("mls");
    case ARM64_SIMD_OP_PMUL:
        EMU_SIMD_VECTOR_3REG_B_EXEC("pmul");
    case ARM64_SIMD_OP_UMAXP:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("umaxp");
    case ARM64_SIMD_OP_UMINP:
        EMU_SIMD_VECTOR_3REG_BHS_EXEC("uminp");
    case ARM64_SIMD_OP_SQRDMULH:
        EMU_SIMD_VECTOR_3REG_HS_EXEC("sqrdmulh");
    case ARM64_SIMD_OP_CMEQ:
        EMU_SIMD_VECTOR_3REG_EXEC("cmeq");
    case ARM64_SIMD_OP_CMGT:
        EMU_SIMD_VECTOR_3REG_EXEC("cmgt");
    case ARM64_SIMD_OP_CMGE:
        EMU_SIMD_VECTOR_3REG_EXEC("cmge");
    case ARM64_SIMD_OP_CMHI:
        EMU_SIMD_VECTOR_3REG_EXEC("cmhi");
    case ARM64_SIMD_OP_CMHS:
        EMU_SIMD_VECTOR_3REG_EXEC("cmhs");
    default:
        return false;
    }
}

#define EMU_SIMD_EXTRA_ACC_WIDTH_EXEC(V64_INST, V128_INST)                                            \
    do                                                                                                \
    {                                                                                                 \
        if (vector_width == 64) EMU_VEC_ACC(".inst " __stringify(V64_INST), dst, left, right);        \
        else if (vector_width == 128) EMU_VEC_ACC(".inst " __stringify(V128_INST), dst, left, right); \
        else return false;                                                                            \
        return true;                                                                                  \
    } while (0)

#define EMU_SIMD_EXTRA_ACC_128_EXEC(INST)                          \
    do                                                             \
    {                                                              \
        if (vector_width != 128) return false;                     \
        EMU_VEC_ACC(".inst " __stringify(INST), dst, left, right); \
        return true;                                               \
    } while (0)

static inline bool emu_simd_vector_3same_extra_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width, uint32_t result_element_width, uint32_t vector_width, uint32_t flags)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_SQRDMLAH:
        if (!emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_RDM)) return false;
        EMU_SIMD_RDM_VECTOR_EXEC(0x2E428420, 0x6E428420, 0x2E828420, 0x6E828420);
    case ARM64_SIMD_OP_SQRDMLSH:
        if (!emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_RDM)) return false;
        EMU_SIMD_RDM_VECTOR_EXEC(0x2E428C20, 0x6E428C20, 0x2E828C20, 0x6E828C20);
    case ARM64_SIMD_OP_SDOT:
        if (element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_DOTPROD)) return false;
        EMU_SIMD_EXTRA_ACC_WIDTH_EXEC(0x0E829420, 0x4E829420);
    case ARM64_SIMD_OP_UDOT:
        if (element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_DOTPROD)) return false;
        EMU_SIMD_EXTRA_ACC_WIDTH_EXEC(0x2E829420, 0x6E829420);
    case ARM64_SIMD_OP_USDOT:
        if (element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_I8MM)) return false;
        EMU_SIMD_EXTRA_ACC_WIDTH_EXEC(0x0E829C20, 0x4E829C20);
    case ARM64_SIMD_OP_BFDOT:
        if (element_width != 16 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_BF16)) return false;
        EMU_SIMD_EXTRA_ACC_WIDTH_EXEC(0x2E42FC20, 0x6E42FC20);
    case ARM64_SIMD_OP_BFMLAL:
        if (element_width != 16 || result_element_width != 32 || vector_width != 128 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_BF16)) return false;
        if (flags & ARM64_SIMD_FLAG_SOURCE_ODD_ELEMENTS) EMU_VEC_ACC(".inst " __stringify(0x6EC2FC20), dst, left, right);
        else EMU_VEC_ACC(".inst " __stringify(0x2EC2FC20), dst, left, right);
        return true;
    case ARM64_SIMD_OP_BFMMLA:
        if (element_width != 16 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_BF16)) return false;
        EMU_SIMD_EXTRA_ACC_128_EXEC(0x6E42EC20);
    case ARM64_SIMD_OP_SMMLA:
        if (element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_I8MM)) return false;
        EMU_SIMD_EXTRA_ACC_128_EXEC(0x4E82A420);
    case ARM64_SIMD_OP_UMMLA:
        if (element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_I8MM)) return false;
        EMU_SIMD_EXTRA_ACC_128_EXEC(0x6E82A420);
    case ARM64_SIMD_OP_USMMLA:
        if (element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_I8MM)) return false;
        EMU_SIMD_EXTRA_ACC_128_EXEC(0x4E82AC20);
    default:
        return false;
    }
}

static inline bool emu_simd_scalar_rdm_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width)
{
    uint32_t instruction;

    if (!emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_RDM)) return false;

    switch (operation)
    {
    case ARM64_SIMD_OP_SQRDMLAH:
        instruction = element_width == 16 ? 0x7E428420 : 0x7E828420;
        break;
    case ARM64_SIMD_OP_SQRDMLSH:
        instruction = element_width == 16 ? 0x7E428C20 : 0x7E828C20;
        break;
    default:
        return false;
    }

    if (element_width != 16 && element_width != 32) return false;
    switch (instruction)
    {
    case 0x7E428420:
        EMU_VEC_ACC(".inst " __stringify(0x7E428420), dst, left, right);
        break;
    case 0x7E828420:
        EMU_VEC_ACC(".inst " __stringify(0x7E828420), dst, left, right);
        break;
    case 0x7E428C20:
        EMU_VEC_ACC(".inst " __stringify(0x7E428C20), dst, left, right);
        break;
    case 0x7E828C20:
        EMU_VEC_ACC(".inst " __stringify(0x7E828C20), dst, left, right);
        break;
    }
    return true;
}

#define EMU_SIMD_SCALAR_BHSD_EXEC(INST)                                                 \
    do                                                                                  \
    {                                                                                   \
        if (element_width == 8) EMU_FP_BIN(INST " b0, b1, b2", dst, left, right);       \
        else if (element_width == 16) EMU_FP_BIN(INST " h0, h1, h2", dst, left, right); \
        else if (element_width == 32) EMU_FP_BIN(INST " s0, s1, s2", dst, left, right); \
        else if (element_width == 64) EMU_FP_BIN(INST " d0, d1, d2", dst, left, right); \
        else return false;                                                              \
        return true;                                                                    \
    } while (0)

#define EMU_SIMD_SCALAR_HS_EXEC(INST)                                                   \
    do                                                                                  \
    {                                                                                   \
        if (element_width == 16) EMU_FP_BIN(INST " h0, h1, h2", dst, left, right);      \
        else if (element_width == 32) EMU_FP_BIN(INST " s0, s1, s2", dst, left, right); \
        else return false;                                                              \
        return true;                                                                    \
    } while (0)

#define EMU_SIMD_SCALAR_D_EXEC(INST)                      \
    do                                                    \
    {                                                     \
        if (element_width != 64) return false;            \
        EMU_FP_BIN(INST " d0, d1, d2", dst, left, right); \
        return true;                                      \
    } while (0)

#define EMU_SIMD_SCALAR_FP_EXEC(INST, H_INST)                                                \
    do                                                                                       \
    {                                                                                        \
        if (element_width == 16) EMU_FP_BIN(".inst " __stringify(H_INST), dst, left, right); \
        else if (element_width == 32) EMU_FP_BIN(INST " s0, s1, s2", dst, left, right);      \
        else if (element_width == 64) EMU_FP_BIN(INST " d0, d1, d2", dst, left, right);      \
        else return false;                                                                   \
        return true;                                                                         \
    } while (0)

static inline bool emu_simd_scalar_3same_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_SQADD:
        EMU_SIMD_SCALAR_BHSD_EXEC("sqadd");
    case ARM64_SIMD_OP_SQSUB:
        EMU_SIMD_SCALAR_BHSD_EXEC("sqsub");
    case ARM64_SIMD_OP_SQSHL:
        EMU_SIMD_SCALAR_BHSD_EXEC("sqshl");
    case ARM64_SIMD_OP_SQRSHL:
        EMU_SIMD_SCALAR_BHSD_EXEC("sqrshl");
    case ARM64_SIMD_OP_SQDMULH:
        EMU_SIMD_SCALAR_HS_EXEC("sqdmulh");
    case ARM64_SIMD_OP_UQADD:
        EMU_SIMD_SCALAR_BHSD_EXEC("uqadd");
    case ARM64_SIMD_OP_UQSUB:
        EMU_SIMD_SCALAR_BHSD_EXEC("uqsub");
    case ARM64_SIMD_OP_UQSHL:
        EMU_SIMD_SCALAR_BHSD_EXEC("uqshl");
    case ARM64_SIMD_OP_UQRSHL:
        EMU_SIMD_SCALAR_BHSD_EXEC("uqrshl");
    case ARM64_SIMD_OP_SQRDMULH:
        EMU_SIMD_SCALAR_HS_EXEC("sqrdmulh");
    case ARM64_SIMD_OP_CMGT:
        EMU_SIMD_SCALAR_D_EXEC("cmgt");
    case ARM64_SIMD_OP_CMGE:
        EMU_SIMD_SCALAR_D_EXEC("cmge");
    case ARM64_SIMD_OP_SSHL:
        EMU_SIMD_SCALAR_D_EXEC("sshl");
    case ARM64_SIMD_OP_SRSHL:
        EMU_SIMD_SCALAR_D_EXEC("srshl");
    case ARM64_SIMD_OP_ADD:
        EMU_SIMD_SCALAR_D_EXEC("add");
    case ARM64_SIMD_OP_CMTST:
        EMU_SIMD_SCALAR_D_EXEC("cmtst");
    case ARM64_SIMD_OP_CMHI:
        EMU_SIMD_SCALAR_D_EXEC("cmhi");
    case ARM64_SIMD_OP_CMHS:
        EMU_SIMD_SCALAR_D_EXEC("cmhs");
    case ARM64_SIMD_OP_USHL:
        EMU_SIMD_SCALAR_D_EXEC("ushl");
    case ARM64_SIMD_OP_URSHL:
        EMU_SIMD_SCALAR_D_EXEC("urshl");
    case ARM64_SIMD_OP_SUB:
        EMU_SIMD_SCALAR_D_EXEC("sub");
    case ARM64_SIMD_OP_CMEQ:
        EMU_SIMD_SCALAR_D_EXEC("cmeq");
    case ARM64_SIMD_OP_SQRDMLAH:
    case ARM64_SIMD_OP_SQRDMLSH:
        return emu_simd_scalar_rdm_hw(operation, dst, left, right, element_width);
    case ARM64_SIMD_OP_FMULX:
        if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
        EMU_SIMD_SCALAR_FP_EXEC("fmulx", 0x5E421C20);
    case ARM64_SIMD_OP_FCMEQ:
        if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
        EMU_SIMD_SCALAR_FP_EXEC("fcmeq", 0x5E422420);
    case ARM64_SIMD_OP_FRECPS:
        if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
        EMU_SIMD_SCALAR_FP_EXEC("frecps", 0x5E423C20);
    case ARM64_SIMD_OP_FRSQRTS:
        if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
        EMU_SIMD_SCALAR_FP_EXEC("frsqrts", 0x5EC23C20);
    case ARM64_SIMD_OP_FCMGE:
        if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
        EMU_SIMD_SCALAR_FP_EXEC("fcmge", 0x7E422420);
    case ARM64_SIMD_OP_FACGE:
        if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
        EMU_SIMD_SCALAR_FP_EXEC("facge", 0x7E422C20);
    case ARM64_SIMD_OP_FABD:
        if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
        EMU_SIMD_SCALAR_FP_EXEC("fabd", 0x7EC21420);
    case ARM64_SIMD_OP_FCMGT:
        if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
        EMU_SIMD_SCALAR_FP_EXEC("fcmgt", 0x7EC22420);
    case ARM64_SIMD_OP_FACGT:
        if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
        EMU_SIMD_SCALAR_FP_EXEC("facgt", 0x7EC22C20);
    default:
        return false;
    }
}

#define EMU_SIMD_INDEXED_ACC_WIDTH_EXEC(V64_INST, V128_INST)                                              \
    do                                                                                                    \
    {                                                                                                     \
        if (operand_width == 64) EMU_VEC_ACC(".inst " __stringify(V64_INST), dst, left, &element);        \
        else if (operand_width == 128) EMU_VEC_ACC(".inst " __stringify(V128_INST), dst, left, &element); \
        else return false;                                                                                \
        return true;                                                                                      \
    } while (0)

static inline bool emu_simd_extra_by_element_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width, uint32_t result_element_width, uint32_t operand_width, uint32_t lane_index, uint32_t flags, bool scalar)
{
    __uint128_t element;
    uint64_t lane_value;

    switch (operation)
    {
    case ARM64_SIMD_OP_SQRDMLAH:
    case ARM64_SIMD_OP_SQRDMLSH:
        if ((element_width != 16 && element_width != 32) || (scalar ? operand_width != element_width : (operand_width != 64 && operand_width != 128)) || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_RDM)) return false;
        if (!emu_simd_extract_lane_hw(right, element_width, lane_index, &lane_value)) return false;
        if (scalar)
        {
            if (!emu_simd_write_scalar_hw(&element, lane_value, element_width)) return false;
            return emu_simd_scalar_rdm_hw(operation, dst, left, &element, element_width);
        }
        if (!emu_simd_dup_general_hw(&element, lane_value, element_width, operand_width)) return false;
        return emu_simd_vector_3same_extra_hw(operation, dst, left, &element, element_width, result_element_width, operand_width, flags);
    case ARM64_SIMD_OP_SDOT:
        if (scalar || element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_DOTPROD)) return false;
        if (!emu_simd_extract_lane_hw(right, 32, lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        EMU_SIMD_INDEXED_ACC_WIDTH_EXEC(0x0F82E020, 0x4F82E020);
    case ARM64_SIMD_OP_UDOT:
        if (scalar || element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_DOTPROD)) return false;
        if (!emu_simd_extract_lane_hw(right, 32, lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        EMU_SIMD_INDEXED_ACC_WIDTH_EXEC(0x2F82E020, 0x6F82E020);
    case ARM64_SIMD_OP_USDOT:
        if (scalar || element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_I8MM)) return false;
        if (!emu_simd_extract_lane_hw(right, 32, lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        EMU_SIMD_INDEXED_ACC_WIDTH_EXEC(0x0F82F020, 0x4F82F020);
    case ARM64_SIMD_OP_SUDOT:
        if (scalar || element_width != 8 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_I8MM)) return false;
        if (!emu_simd_extract_lane_hw(right, 32, lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        EMU_SIMD_INDEXED_ACC_WIDTH_EXEC(0x0F02F020, 0x4F02F020);
    case ARM64_SIMD_OP_BFDOT:
        if (scalar || element_width != 16 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_BF16)) return false;
        if (!emu_simd_extract_lane_hw(right, 32, lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        EMU_SIMD_INDEXED_ACC_WIDTH_EXEC(0x0F42F020, 0x4F42F020);
    case ARM64_SIMD_OP_BFMLAL:
        if (scalar || operand_width != 128 || element_width != 16 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_BF16)) return false;
        if (!emu_simd_extract_lane_hw(right, 16, lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 16)) return false;
        if (flags & ARM64_SIMD_FLAG_SOURCE_ODD_ELEMENTS) EMU_VEC_ACC(".inst " __stringify(0x4FC2F020), dst, left, &element);
        else EMU_VEC_ACC(".inst " __stringify(0x0FC2F020), dst, left, &element);
        return true;
    default:
        return false;
    }
}

#define EMU_SIMD_FHM_WIDTH_EXEC(V64_INST, V128_INST, RIGHT)                                            \
    do                                                                                                 \
    {                                                                                                  \
        if (operand_width == 64) EMU_VEC_ACC(".inst " __stringify(V64_INST), dst, left, RIGHT);        \
        else if (operand_width == 128) EMU_VEC_ACC(".inst " __stringify(V128_INST), dst, left, RIGHT); \
        else return false;                                                                             \
        return true;                                                                                   \
    } while (0)

static inline bool emu_simd_fhm_vector_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width, uint32_t result_element_width, uint32_t operand_width, uint32_t flags)
{
    bool high_half = flags & ARM64_SIMD_FLAG_SOURCE_HIGH_HALF;

    if (element_width != 16 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_FHM)) return false;

    switch (operation)
    {
    case ARM64_SIMD_OP_FMLAL:
        if (high_half) EMU_SIMD_FHM_WIDTH_EXEC(0x2E22CC20, 0x6E22CC20, right);
        EMU_SIMD_FHM_WIDTH_EXEC(0x0E22EC20, 0x4E22EC20, right);
    case ARM64_SIMD_OP_FMLSL:
        if (high_half) EMU_SIMD_FHM_WIDTH_EXEC(0x2EA2CC20, 0x6EA2CC20, right);
        EMU_SIMD_FHM_WIDTH_EXEC(0x0EA2EC20, 0x4EA2EC20, right);
    default:
        return false;
    }
}

static inline bool emu_simd_fhm_by_element_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width, uint32_t result_element_width, uint32_t operand_width, uint32_t lane_index, uint32_t flags)
{
    __uint128_t element;
    uint64_t lane_value;
    bool high_half = flags & ARM64_SIMD_FLAG_SOURCE_HIGH_HALF;

    if (element_width != 16 || result_element_width != 32 || !emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_FHM)) return false;
    if (!emu_simd_extract_lane_hw(right, 16, lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 16)) return false;

    switch (operation)
    {
    case ARM64_SIMD_OP_FMLAL:
        if (high_half) EMU_SIMD_FHM_WIDTH_EXEC(0x2F828020, 0x6F828020, &element);
        EMU_SIMD_FHM_WIDTH_EXEC(0x0F820020, 0x4F820020, &element);
    case ARM64_SIMD_OP_FMLSL:
        if (high_half) EMU_SIMD_FHM_WIDTH_EXEC(0x2F82C020, 0x6F82C020, &element);
        EMU_SIMD_FHM_WIDTH_EXEC(0x0F824020, 0x4F824020, &element);
    default:
        return false;
    }
}

#define EMU_SIMD_FCMLA_ROTATION_EXEC(INST0, INST90, INST180, INST270, RIGHT) \
    do                                                                       \
    {                                                                        \
        switch (rotation)                                                    \
        {                                                                    \
        case ARM64_SIMD_ROTATION_0:                                          \
            EMU_VEC_ACC(".inst " __stringify(INST0), dst, left, RIGHT);      \
            break;                                                           \
        case ARM64_SIMD_ROTATION_90:                                         \
            EMU_VEC_ACC(".inst " __stringify(INST90), dst, left, RIGHT);     \
            break;                                                           \
        case ARM64_SIMD_ROTATION_180:                                        \
            EMU_VEC_ACC(".inst " __stringify(INST180), dst, left, RIGHT);    \
            break;                                                           \
        case ARM64_SIMD_ROTATION_270:                                        \
            EMU_VEC_ACC(".inst " __stringify(INST270), dst, left, RIGHT);    \
            break;                                                           \
        default:                                                             \
            return false;                                                    \
        }                                                                    \
        return true;                                                         \
    } while (0)

#define EMU_SIMD_FCADD_ROTATION_EXEC(INST90, INST270)                                                              \
    do                                                                                                             \
    {                                                                                                              \
        if (rotation == ARM64_SIMD_ROTATION_90) EMU_FP_BIN(".inst " __stringify(INST90), dst, left, right);        \
        else if (rotation == ARM64_SIMD_ROTATION_270) EMU_FP_BIN(".inst " __stringify(INST270), dst, left, right); \
        else return false;                                                                                         \
        return true;                                                                                               \
    } while (0)

static inline bool emu_simd_fcma_vector_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width, uint32_t operand_width, uint32_t rotation)
{
    if (!emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_FCMA)) return false;
    if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;

    if (operation == ARM64_SIMD_OP_FCMLA)
    {
        if (operand_width == 64 && element_width == 16) EMU_SIMD_FCMLA_ROTATION_EXEC(0x2E42C420, 0x2E42CC20, 0x2E42D420, 0x2E42DC20, right);
        if (operand_width == 128 && element_width == 16) EMU_SIMD_FCMLA_ROTATION_EXEC(0x6E42C420, 0x6E42CC20, 0x6E42D420, 0x6E42DC20, right);
        if (operand_width == 64 && element_width == 32) EMU_SIMD_FCMLA_ROTATION_EXEC(0x2E82C420, 0x2E82CC20, 0x2E82D420, 0x2E82DC20, right);
        if (operand_width == 128 && element_width == 32) EMU_SIMD_FCMLA_ROTATION_EXEC(0x6E82C420, 0x6E82CC20, 0x6E82D420, 0x6E82DC20, right);
        if (operand_width == 128 && element_width == 64) EMU_SIMD_FCMLA_ROTATION_EXEC(0x6EC2C420, 0x6EC2CC20, 0x6EC2D420, 0x6EC2DC20, right);
        return false;
    }

    if (operation != ARM64_SIMD_OP_FCADD) return false;
    if (operand_width == 64 && element_width == 16) EMU_SIMD_FCADD_ROTATION_EXEC(0x2E42E420, 0x2E42F420);
    if (operand_width == 128 && element_width == 16) EMU_SIMD_FCADD_ROTATION_EXEC(0x6E42E420, 0x6E42F420);
    if (operand_width == 64 && element_width == 32) EMU_SIMD_FCADD_ROTATION_EXEC(0x2E82E420, 0x2E82F420);
    if (operand_width == 128 && element_width == 32) EMU_SIMD_FCADD_ROTATION_EXEC(0x6E82E420, 0x6E82F420);
    if (operand_width == 128 && element_width == 64) EMU_SIMD_FCADD_ROTATION_EXEC(0x6EC2E420, 0x6EC2F420);
    return false;
}

static inline bool emu_simd_fcma_by_element_hw(void *dst, const void *left, const void *right, uint32_t element_width, uint32_t operand_width, uint32_t lane_index, uint32_t rotation)
{
    __uint128_t element;
    uint64_t lane_value;
    uint32_t complex_width = element_width * 2;

    if (!emu_simd_current_cpu_has_feature(EMU_SIMD_CPU_FEATURE_FCMA)) return false;
    if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;
    if ((element_width != 16 && element_width != 32) || !emu_simd_extract_lane_hw(right, complex_width, lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, complex_width)) return false;

    if (operand_width == 64 && element_width == 16) EMU_SIMD_FCMLA_ROTATION_EXEC(0x2F421020, 0x2F423020, 0x2F425020, 0x2F427020, &element);
    if (operand_width == 128 && element_width == 16) EMU_SIMD_FCMLA_ROTATION_EXEC(0x6F421020, 0x6F423020, 0x6F425020, 0x6F427020, &element);
    if (operand_width == 128 && element_width == 32) EMU_SIMD_FCMLA_ROTATION_EXEC(0x6F821020, 0x6F823020, 0x6F825020, 0x6F827020, &element);
    return false;
}

static inline bool emu_simd_permute_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width, uint32_t vector_width)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_UZP1:
        EMU_SIMD_VECTOR_3REG_EXEC("uzp1");
    case ARM64_SIMD_OP_TRN1:
        EMU_SIMD_VECTOR_3REG_EXEC("trn1");
    case ARM64_SIMD_OP_ZIP1:
        EMU_SIMD_VECTOR_3REG_EXEC("zip1");
    case ARM64_SIMD_OP_UZP2:
        EMU_SIMD_VECTOR_3REG_EXEC("uzp2");
    case ARM64_SIMD_OP_TRN2:
        EMU_SIMD_VECTOR_3REG_EXEC("trn2");
    case ARM64_SIMD_OP_ZIP2:
        EMU_SIMD_VECTOR_3REG_EXEC("zip2");
    default:
        return false;
    }
}

#define EMU_SIMD_LOGICAL_BIN_EXEC(INST)                                   \
    do                                                                    \
    {                                                                     \
        if (vector_width == 64)                                           \
        {                                                                 \
            EMU_FP_BIN(INST " v0.8b, v1.8b, v2.8b", dst, left, right);    \
            return true;                                                  \
        }                                                                 \
        if (vector_width == 128)                                          \
        {                                                                 \
            EMU_FP_BIN(INST " v0.16b, v1.16b, v2.16b", dst, left, right); \
            return true;                                                  \
        }                                                                 \
        return false;                                                     \
    } while (0)

#define EMU_SIMD_LOGICAL_MASK_EXEC(INST)                                   \
    do                                                                     \
    {                                                                      \
        if (vector_width == 64)                                            \
        {                                                                  \
            EMU_VEC_ACC(INST " v0.8b, v1.8b, v2.8b", dst, left, right);    \
            return true;                                                   \
        }                                                                  \
        if (vector_width == 128)                                           \
        {                                                                  \
            EMU_VEC_ACC(INST " v0.16b, v1.16b, v2.16b", dst, left, right); \
            return true;                                                   \
        }                                                                  \
        return false;                                                      \
    } while (0)

static inline bool emu_simd_logical_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t vector_width)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_AND:
        EMU_SIMD_LOGICAL_BIN_EXEC("and");
    case ARM64_SIMD_OP_BIC:
        EMU_SIMD_LOGICAL_BIN_EXEC("bic");
    case ARM64_SIMD_OP_ORR:
        EMU_SIMD_LOGICAL_BIN_EXEC("orr");
    case ARM64_SIMD_OP_ORN:
        EMU_SIMD_LOGICAL_BIN_EXEC("orn");
    case ARM64_SIMD_OP_EOR:
        EMU_SIMD_LOGICAL_BIN_EXEC("eor");
    case ARM64_SIMD_OP_BSL:
        EMU_SIMD_LOGICAL_MASK_EXEC("bsl");
    case ARM64_SIMD_OP_BIT:
        EMU_SIMD_LOGICAL_MASK_EXEC("bit");
    case ARM64_SIMD_OP_BIF:
        EMU_SIMD_LOGICAL_MASK_EXEC("bif");
    default:
        return false;
    }
}

#define EMU_SIMD_FP_VECTOR_BIN_SHAPE(INST, V4H_INST, V8H_INST)                                                             \
    do                                                                                                                     \
    {                                                                                                                      \
        if (vector_width == 64 && element_width == 16) EMU_FP_BIN(".inst " __stringify(V4H_INST), dst, left, right);       \
        else if (vector_width == 128 && element_width == 16) EMU_FP_BIN(".inst " __stringify(V8H_INST), dst, left, right); \
        else if (vector_width == 64 && element_width == 32) EMU_FP_BIN(INST " v0.2s, v1.2s, v2.2s", dst, left, right);     \
        else if (vector_width == 128 && element_width == 32) EMU_FP_BIN(INST " v0.4s, v1.4s, v2.4s", dst, left, right);    \
        else if (vector_width == 128 && element_width == 64) EMU_FP_BIN(INST " v0.2d, v1.2d, v2.2d", dst, left, right);    \
        else return false;                                                                                                 \
        return true;                                                                                                       \
    } while (0)

#define EMU_SIMD_FP_VECTOR_ACC_SHAPE(INST, V4H_INST, V8H_INST)                                                              \
    do                                                                                                                      \
    {                                                                                                                       \
        if (vector_width == 64 && element_width == 16) EMU_VEC_ACC(".inst " __stringify(V4H_INST), dst, left, right);       \
        else if (vector_width == 128 && element_width == 16) EMU_VEC_ACC(".inst " __stringify(V8H_INST), dst, left, right); \
        else if (vector_width == 64 && element_width == 32) EMU_VEC_ACC(INST " v0.2s, v1.2s, v2.2s", dst, left, right);     \
        else if (vector_width == 128 && element_width == 32) EMU_VEC_ACC(INST " v0.4s, v1.4s, v2.4s", dst, left, right);    \
        else if (vector_width == 128 && element_width == 64) EMU_VEC_ACC(INST " v0.2d, v1.2d, v2.2d", dst, left, right);    \
        else return false;                                                                                                  \
        return true;                                                                                                        \
    } while (0)

#define EMU_SIMD_FP_VECTOR_INST_SHAPE(V4H_INST, V8H_INST, V2S_INST, V4S_INST, V2D_INST)                                    \
    do                                                                                                                     \
    {                                                                                                                      \
        if (vector_width == 64 && element_width == 16) EMU_FP_BIN(".inst " __stringify(V4H_INST), dst, left, right);       \
        else if (vector_width == 128 && element_width == 16) EMU_FP_BIN(".inst " __stringify(V8H_INST), dst, left, right); \
        else if (vector_width == 64 && element_width == 32) EMU_FP_BIN(".inst " __stringify(V2S_INST), dst, left, right);  \
        else if (vector_width == 128 && element_width == 32) EMU_FP_BIN(".inst " __stringify(V4S_INST), dst, left, right); \
        else if (vector_width == 128 && element_width == 64) EMU_FP_BIN(".inst " __stringify(V2D_INST), dst, left, right); \
        else return false;                                                                                                 \
        return true;                                                                                                       \
    } while (0)

static inline bool emu_simd_fp_vector_3reg_feature_available(enum arm64_simd_operation operation, uint32_t element_width)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_FAMAX:
    case ARM64_SIMD_OP_FAMIN:
        return emu_simd_current_cpu_has_faminmax();
    case ARM64_SIMD_OP_FSCALE:
        return emu_simd_current_cpu_has_f8cvt();
    default:
        return element_width != 16 || emu_simd_current_cpu_has_fp16();
    }
}

static inline bool emu_simd_fp_vector_3reg_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width, uint32_t vector_width)
{
    if (!emu_simd_fp_vector_3reg_feature_available(operation, element_width)) return false;

    switch (operation)
    {
    case ARM64_SIMD_OP_FADD:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fadd", 0x0E421420, 0x4E421420);
    case ARM64_SIMD_OP_FSUB:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fsub", 0x0EC21420, 0x4EC21420);
    case ARM64_SIMD_OP_FMUL:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fmul", 0x2E421C20, 0x6E421C20);
    case ARM64_SIMD_OP_FMULX:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fmulx", 0x0E421C20, 0x4E421C20);
    case ARM64_SIMD_OP_FDIV:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fdiv", 0x2E423C20, 0x6E423C20);
    case ARM64_SIMD_OP_FMLA:
        EMU_SIMD_FP_VECTOR_ACC_SHAPE("fmla", 0x0E420C20, 0x4E420C20);
    case ARM64_SIMD_OP_FMLS:
        EMU_SIMD_FP_VECTOR_ACC_SHAPE("fmls", 0x0EC20C20, 0x4EC20C20);
    case ARM64_SIMD_OP_FMAX:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fmax", 0x0E423420, 0x4E423420);
    case ARM64_SIMD_OP_FMIN:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fmin", 0x0EC23420, 0x4EC23420);
    case ARM64_SIMD_OP_FMAXNM:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fmaxnm", 0x0E420420, 0x4E420420);
    case ARM64_SIMD_OP_FMINNM:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fminnm", 0x0EC20420, 0x4EC20420);
    case ARM64_SIMD_OP_FADDP:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("faddp", 0x2E421420, 0x6E421420);
    case ARM64_SIMD_OP_FMAXP:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fmaxp", 0x2E423420, 0x6E423420);
    case ARM64_SIMD_OP_FMINP:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fminp", 0x2EC23420, 0x6EC23420);
    case ARM64_SIMD_OP_FMAXNMP:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fmaxnmp", 0x2E420420, 0x6E420420);
    case ARM64_SIMD_OP_FMINNMP:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fminnmp", 0x2EC20420, 0x6EC20420);
    case ARM64_SIMD_OP_FABD:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fabd", 0x2EC21420, 0x6EC21420);
    case ARM64_SIMD_OP_FRECPS:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("frecps", 0x0E423C20, 0x4E423C20);
    case ARM64_SIMD_OP_FRSQRTS:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("frsqrts", 0x0EC23C20, 0x4EC23C20);
    case ARM64_SIMD_OP_FCMEQ:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fcmeq", 0x0E422420, 0x4E422420);
    case ARM64_SIMD_OP_FCMGE:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fcmge", 0x2E422420, 0x6E422420);
    case ARM64_SIMD_OP_FCMGT:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("fcmgt", 0x2EC22420, 0x6EC22420);
    case ARM64_SIMD_OP_FACGE:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("facge", 0x2E422C20, 0x6E422C20);
    case ARM64_SIMD_OP_FACGT:
        EMU_SIMD_FP_VECTOR_BIN_SHAPE("facgt", 0x2EC22C20, 0x6EC22C20);
    case ARM64_SIMD_OP_FAMAX:
        EMU_SIMD_FP_VECTOR_INST_SHAPE(0x0EC21C20, 0x4EC21C20, 0x0EA2DC20, 0x4EA2DC20, 0x4EE2DC20);
    case ARM64_SIMD_OP_FAMIN:
        EMU_SIMD_FP_VECTOR_INST_SHAPE(0x2EC21C20, 0x6EC21C20, 0x2EA2DC20, 0x6EA2DC20, 0x6EE2DC20);
    case ARM64_SIMD_OP_FSCALE:
        EMU_SIMD_FP_VECTOR_INST_SHAPE(0x2EC23C20, 0x6EC23C20, 0x2EA2FC20, 0x6EA2FC20, 0x6EE2FC20);
    default:
        return false;
    }
}

#define EMU_SIMD_FP_VECTOR_UN_SHAPE(INST, V4H_INST, V8H_INST)                                                        \
    do                                                                                                               \
    {                                                                                                                \
        if (vector_width == 64 && element_width == 16) EMU_FP_UN(".inst " __stringify(V4H_INST), dst, source);       \
        else if (vector_width == 128 && element_width == 16) EMU_FP_UN(".inst " __stringify(V8H_INST), dst, source); \
        else if (vector_width == 64 && element_width == 32) EMU_FP_UN(INST " v0.2s, v1.2s", dst, source);            \
        else if (vector_width == 128 && element_width == 32) EMU_FP_UN(INST " v0.4s, v1.4s", dst, source);           \
        else if (vector_width == 128 && element_width == 64) EMU_FP_UN(INST " v0.2d, v1.2d", dst, source);           \
        else return false;                                                                                           \
        return true;                                                                                                 \
    } while (0)

static inline bool emu_simd_fp_vector_2reg_hw(enum arm64_simd_operation operation, void *dst, const void *source, uint32_t element_width, uint32_t vector_width)
{
    if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;

    switch (operation)
    {
    case ARM64_SIMD_OP_FABS:
        EMU_SIMD_FP_VECTOR_UN_SHAPE("fabs", 0x0EF8F820, 0x4EF8F820);
    case ARM64_SIMD_OP_FNEG:
        EMU_SIMD_FP_VECTOR_UN_SHAPE("fneg", 0x2EF8F820, 0x6EF8F820);
    case ARM64_SIMD_OP_FSQRT:
        EMU_SIMD_FP_VECTOR_UN_SHAPE("fsqrt", 0x2EF9F820, 0x6EF9F820);
    default:
        return false;
    }
}

static inline bool emu_simd_rev_hw(enum arm64_simd_operation operation, void *dst, const void *source, uint32_t element_width, uint32_t vector_width)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_REV64:
        if (element_width == 8 && vector_width == 64) EMU_FP_UN("rev64 v0.8b, v1.8b", dst, source);
        else if (element_width == 8 && vector_width == 128) EMU_FP_UN("rev64 v0.16b, v1.16b", dst, source);
        else if (element_width == 16 && vector_width == 64) EMU_FP_UN("rev64 v0.4h, v1.4h", dst, source);
        else if (element_width == 16 && vector_width == 128) EMU_FP_UN("rev64 v0.8h, v1.8h", dst, source);
        else if (element_width == 32 && vector_width == 64) EMU_FP_UN("rev64 v0.2s, v1.2s", dst, source);
        else if (element_width == 32 && vector_width == 128) EMU_FP_UN("rev64 v0.4s, v1.4s", dst, source);
        else return false;
        return true;
    case ARM64_SIMD_OP_REV32:
        if (element_width == 8 && vector_width == 64) EMU_FP_UN("rev32 v0.8b, v1.8b", dst, source);
        else if (element_width == 8 && vector_width == 128) EMU_FP_UN("rev32 v0.16b, v1.16b", dst, source);
        else if (element_width == 16 && vector_width == 64) EMU_FP_UN("rev32 v0.4h, v1.4h", dst, source);
        else if (element_width == 16 && vector_width == 128) EMU_FP_UN("rev32 v0.8h, v1.8h", dst, source);
        else return false;
        return true;
    case ARM64_SIMD_OP_REV16:
        if (element_width == 8 && vector_width == 64) EMU_FP_UN("rev16 v0.8b, v1.8b", dst, source);
        else if (element_width == 8 && vector_width == 128) EMU_FP_UN("rev16 v0.16b, v1.16b", dst, source);
        else return false;
        return true;
    default:
        return false;
    }
}

#define EMU_SIMD_INTEGER_REDUCE_EXEC(INST)                                                              \
    do                                                                                                  \
    {                                                                                                   \
        if (vector_width == 64 && element_width == 8) EMU_FP_UN(INST " b0, v1.8b", dst, source);        \
        else if (vector_width == 128 && element_width == 8) EMU_FP_UN(INST " b0, v1.16b", dst, source); \
        else if (vector_width == 64 && element_width == 16) EMU_FP_UN(INST " h0, v1.4h", dst, source);  \
        else if (vector_width == 128 && element_width == 16) EMU_FP_UN(INST " h0, v1.8h", dst, source); \
        else if (vector_width == 128 && element_width == 32) EMU_FP_UN(INST " s0, v1.4s", dst, source); \
        else return false;                                                                              \
        return true;                                                                                    \
    } while (0)

#define EMU_SIMD_INTEGER_REDUCE_LONG_EXEC(INST)                                                         \
    do                                                                                                  \
    {                                                                                                   \
        if (vector_width == 64 && element_width == 8) EMU_FP_UN(INST " h0, v1.8b", dst, source);        \
        else if (vector_width == 128 && element_width == 8) EMU_FP_UN(INST " h0, v1.16b", dst, source); \
        else if (vector_width == 64 && element_width == 16) EMU_FP_UN(INST " s0, v1.4h", dst, source);  \
        else if (vector_width == 128 && element_width == 16) EMU_FP_UN(INST " s0, v1.8h", dst, source); \
        else if (vector_width == 128 && element_width == 32) EMU_FP_UN(INST " d0, v1.4s", dst, source); \
        else return false;                                                                              \
        return true;                                                                                    \
    } while (0)

static inline bool emu_simd_integer_reduce_hw(enum arm64_simd_operation operation, void *dst, const void *source, uint32_t element_width, uint32_t result_element_width, uint32_t vector_width)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_ADDV:
        if (result_element_width != element_width) return false;
        EMU_SIMD_INTEGER_REDUCE_EXEC("addv");
    case ARM64_SIMD_OP_SADDLV:
        if (result_element_width != element_width * 2) return false;
        EMU_SIMD_INTEGER_REDUCE_LONG_EXEC("saddlv");
    case ARM64_SIMD_OP_UADDLV:
        if (result_element_width != element_width * 2) return false;
        EMU_SIMD_INTEGER_REDUCE_LONG_EXEC("uaddlv");
    case ARM64_SIMD_OP_SMAXV:
        if (result_element_width != element_width) return false;
        EMU_SIMD_INTEGER_REDUCE_EXEC("smaxv");
    case ARM64_SIMD_OP_SMINV:
        if (result_element_width != element_width) return false;
        EMU_SIMD_INTEGER_REDUCE_EXEC("sminv");
    case ARM64_SIMD_OP_UMAXV:
        if (result_element_width != element_width) return false;
        EMU_SIMD_INTEGER_REDUCE_EXEC("umaxv");
    case ARM64_SIMD_OP_UMINV:
        if (result_element_width != element_width) return false;
        EMU_SIMD_INTEGER_REDUCE_EXEC("uminv");
    default:
        return false;
    }
}

#define EMU_SIMD_VECTOR_NARROW_EXEC(INST, INST2)                                 \
    do                                                                           \
    {                                                                            \
        if (result_element_width == 8 && element_width == 16)                    \
        {                                                                        \
            if (high_half) EMU_FP_UN_MERGE(INST2 " v0.16b, v1.8h", dst, source); \
            else EMU_FP_UN(INST " v0.8b, v1.8h", dst, source);                   \
        }                                                                        \
        else if (result_element_width == 16 && element_width == 32)              \
        {                                                                        \
            if (high_half) EMU_FP_UN_MERGE(INST2 " v0.8h, v1.4s", dst, source);  \
            else EMU_FP_UN(INST " v0.4h, v1.4s", dst, source);                   \
        }                                                                        \
        else if (result_element_width == 32 && element_width == 64)              \
        {                                                                        \
            if (high_half) EMU_FP_UN_MERGE(INST2 " v0.4s, v1.2d", dst, source);  \
            else EMU_FP_UN(INST " v0.2s, v1.2d", dst, source);                   \
        }                                                                        \
        else return false;                                                       \
        return true;                                                             \
    } while (0)

#define EMU_SIMD_SCALAR_NARROW_EXEC(INST)                                                                   \
    do                                                                                                      \
    {                                                                                                       \
        if (result_element_width == 8 && element_width == 16) EMU_FP_UN(INST " b0, h1", dst, source);       \
        else if (result_element_width == 16 && element_width == 32) EMU_FP_UN(INST " h0, s1", dst, source); \
        else if (result_element_width == 32 && element_width == 64) EMU_FP_UN(INST " s0, d1", dst, source); \
        else return false;                                                                                  \
        return true;                                                                                        \
    } while (0)

static inline bool emu_simd_narrow_hw(enum arm64_simd_operation operation, void *dst, const void *source, uint32_t element_width, uint32_t result_element_width, uint32_t flags, bool scalar)
{
    bool high_half = flags & ARM64_SIMD_FLAG_DEST_HIGH_HALF;

    if (scalar)
    {
        if (high_half) return false;
        switch (operation)
        {
        case ARM64_SIMD_OP_SQXTN:
            EMU_SIMD_SCALAR_NARROW_EXEC("sqxtn");
        case ARM64_SIMD_OP_UQXTN:
            EMU_SIMD_SCALAR_NARROW_EXEC("uqxtn");
        case ARM64_SIMD_OP_SQXTUN:
            EMU_SIMD_SCALAR_NARROW_EXEC("sqxtun");
        default:
            return false;
        }
    }

    switch (operation)
    {
    case ARM64_SIMD_OP_XTN:
        EMU_SIMD_VECTOR_NARROW_EXEC("xtn", "xtn2");
    case ARM64_SIMD_OP_SQXTN:
        EMU_SIMD_VECTOR_NARROW_EXEC("sqxtn", "sqxtn2");
    case ARM64_SIMD_OP_UQXTN:
        EMU_SIMD_VECTOR_NARROW_EXEC("uqxtn", "uqxtn2");
    case ARM64_SIMD_OP_SQXTUN:
        EMU_SIMD_VECTOR_NARROW_EXEC("sqxtun", "sqxtun2");
    default:
        return false;
    }
}

static inline bool emu_simd_fp_reduce_hw(enum arm64_simd_operation operation, void *dst, const void *source, uint32_t element_width, uint32_t vector_width)
{
    if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;

    switch (operation)
    {
    case ARM64_SIMD_OP_FADDP:
        if (element_width == 16 && vector_width == 32) EMU_FP_UN(".inst 0x5E30D820", dst, source);
        else if (element_width == 32 && vector_width == 64) EMU_FP_UN("faddp s0, v1.2s", dst, source);
        else if (element_width == 64 && vector_width == 128) EMU_FP_UN("faddp d0, v1.2d", dst, source);
        else return false;
        return true;
    case ARM64_SIMD_OP_FMAXNMV:
        if (element_width == 16 && vector_width == 64) EMU_FP_UN(".inst 0x0E30C820", dst, source);
        else if (element_width == 16 && vector_width == 128) EMU_FP_UN(".inst 0x4E30C820", dst, source);
        else if (element_width == 32 && vector_width == 128) EMU_FP_UN("fmaxnmv s0, v1.4s", dst, source);
        else return false;
        return true;
    case ARM64_SIMD_OP_FMINNMV:
        if (element_width == 16 && vector_width == 64) EMU_FP_UN(".inst 0x0EB0C820", dst, source);
        else if (element_width == 16 && vector_width == 128) EMU_FP_UN(".inst 0x4EB0C820", dst, source);
        else if (element_width == 32 && vector_width == 128) EMU_FP_UN("fminnmv s0, v1.4s", dst, source);
        else return false;
        return true;
    case ARM64_SIMD_OP_FMAXV:
        if (element_width == 16 && vector_width == 64) EMU_FP_UN(".inst 0x0E30F820", dst, source);
        else if (element_width == 16 && vector_width == 128) EMU_FP_UN(".inst 0x4E30F820", dst, source);
        else if (element_width == 32 && vector_width == 128) EMU_FP_UN("fmaxv s0, v1.4s", dst, source);
        else return false;
        return true;
    case ARM64_SIMD_OP_FMINV:
        if (element_width == 16 && vector_width == 64) EMU_FP_UN(".inst 0x0EB0F820", dst, source);
        else if (element_width == 16 && vector_width == 128) EMU_FP_UN(".inst 0x4EB0F820", dst, source);
        else if (element_width == 32 && vector_width == 128) EMU_FP_UN("fminv s0, v1.4s", dst, source);
        else return false;
        return true;
    default:
        return false;
    }
}

#define EMU_SIMD_SHIFT_EXEC(INST, ARR, AMOUNT)                                                   \
    do                                                                                           \
    {                                                                                            \
        asm volatile(".arch_extension fp\n.arch_extension simd\n"                                \
                     "ldr q1, [%1]\n"                                                            \
                     "dup v2." ARR ", " AMOUNT "\n" INST " v0." ARR ", v1." ARR ", v2." ARR "\n" \
                     "str q0, [%0]\n"                                                            \
                     :                                                                           \
                     : "r"(dst), "r"(source), "r"(shift_amount)                                  \
                     : "memory", "v0", "v1", "v2");                                              \
        return true;                                                                             \
    } while (0)

static inline bool emu_simd_shift_hw(enum arm64_simd_operation operation, void *dst, const void *source, uint32_t element_width, uint32_t vector_width, uint32_t shift)
{
    uint64_t shift_amount = operation == ARM64_SIMD_OP_SHL ? shift : (uint64_t)-(int64_t)shift;
    const char *instruction;

    if (operation == ARM64_SIMD_OP_SHL || operation == ARM64_SIMD_OP_USHR) instruction = "ushl";
    else if (operation == ARM64_SIMD_OP_SSHR) instruction = "sshl";
    else return false;

    if (vector_width == 64)
    {
        switch (element_width)
        {
        case 8:
            if (instruction[0] == 'u') EMU_SIMD_SHIFT_EXEC("ushl", "8b", "%w2");
            EMU_SIMD_SHIFT_EXEC("sshl", "8b", "%w2");
        case 16:
            if (instruction[0] == 'u') EMU_SIMD_SHIFT_EXEC("ushl", "4h", "%w2");
            EMU_SIMD_SHIFT_EXEC("sshl", "4h", "%w2");
        case 32:
            if (instruction[0] == 'u') EMU_SIMD_SHIFT_EXEC("ushl", "2s", "%w2");
            EMU_SIMD_SHIFT_EXEC("sshl", "2s", "%w2");
        default:
            return false;
        }
    }

    switch (element_width)
    {
    case 8:
        if (instruction[0] == 'u') EMU_SIMD_SHIFT_EXEC("ushl", "16b", "%w2");
        EMU_SIMD_SHIFT_EXEC("sshl", "16b", "%w2");
    case 16:
        if (instruction[0] == 'u') EMU_SIMD_SHIFT_EXEC("ushl", "8h", "%w2");
        EMU_SIMD_SHIFT_EXEC("sshl", "8h", "%w2");
    case 32:
        if (instruction[0] == 'u') EMU_SIMD_SHIFT_EXEC("ushl", "4s", "%w2");
        EMU_SIMD_SHIFT_EXEC("sshl", "4s", "%w2");
    case 64:
        if (instruction[0] == 'u') EMU_SIMD_SHIFT_EXEC("ushl", "2d", "%2");
        EMU_SIMD_SHIFT_EXEC("sshl", "2d", "%2");
    default:
        return false;
    }
}

#define EMU_SIMD_EXT_CASE(N, ARR)                                                     \
    case N:                                                                           \
        EMU_FP_BIN("ext v0." ARR ", v1." ARR ", v2." ARR ", #" #N, dst, left, right); \
        return true

static inline bool emu_simd_ext_hw(void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t byte_offset)
{
    if (vector_width == 64)
    {
        switch (byte_offset)
        {
            EMU_SIMD_EXT_CASE(0, "8b");
            EMU_SIMD_EXT_CASE(1, "8b");
            EMU_SIMD_EXT_CASE(2, "8b");
            EMU_SIMD_EXT_CASE(3, "8b");
            EMU_SIMD_EXT_CASE(4, "8b");
            EMU_SIMD_EXT_CASE(5, "8b");
            EMU_SIMD_EXT_CASE(6, "8b");
            EMU_SIMD_EXT_CASE(7, "8b");
        default:
            return false;
        }
    }

    switch (byte_offset)
    {
        EMU_SIMD_EXT_CASE(0, "16b");
        EMU_SIMD_EXT_CASE(1, "16b");
        EMU_SIMD_EXT_CASE(2, "16b");
        EMU_SIMD_EXT_CASE(3, "16b");
        EMU_SIMD_EXT_CASE(4, "16b");
        EMU_SIMD_EXT_CASE(5, "16b");
        EMU_SIMD_EXT_CASE(6, "16b");
        EMU_SIMD_EXT_CASE(7, "16b");
        EMU_SIMD_EXT_CASE(8, "16b");
        EMU_SIMD_EXT_CASE(9, "16b");
        EMU_SIMD_EXT_CASE(10, "16b");
        EMU_SIMD_EXT_CASE(11, "16b");
        EMU_SIMD_EXT_CASE(12, "16b");
        EMU_SIMD_EXT_CASE(13, "16b");
        EMU_SIMD_EXT_CASE(14, "16b");
        EMU_SIMD_EXT_CASE(15, "16b");
    default:
        return false;
    }
}

#define EMU_SIMD_FP_COMPARE_ZERO_FP16_INST(INSTRUCTION)                 \
    do                                                                  \
    {                                                                   \
        asm volatile(".arch_extension fp\n"                             \
                     ".arch_extension simd\n"                           \
                     "ldr q1, [%1]\n"                                   \
                     ".inst " __stringify(INSTRUCTION) "\n"             \
                                                       "str q0, [%0]\n" \
                     :                                                  \
                     : "r"(dst), "r"(source)                            \
                     : "memory", "v0", "v1");                           \
        return true;                                                    \
    } while (0)

#define EMU_SIMD_FP_COMPARE_ZERO_SHAPE(INST, H_INST, V4H_INST, V8H_INST)                                          \
    do                                                                                                            \
    {                                                                                                             \
        if (operand_width == 16 && element_width == 16) EMU_SIMD_FP_COMPARE_ZERO_FP16_INST(H_INST);               \
        else if (operand_width == 32 && element_width == 32) EMU_FP_UN(INST " s0, s1, #0.0", dst, source);        \
        else if (operand_width == 64 && element_width == 64) EMU_FP_UN(INST " d0, d1, #0.0", dst, source);        \
        else if (operand_width == 64 && element_width == 16) EMU_SIMD_FP_COMPARE_ZERO_FP16_INST(V4H_INST);        \
        else if (operand_width == 64 && element_width == 32) EMU_FP_UN(INST " v0.2s, v1.2s, #0.0", dst, source);  \
        else if (operand_width == 128 && element_width == 16) EMU_SIMD_FP_COMPARE_ZERO_FP16_INST(V8H_INST);       \
        else if (operand_width == 128 && element_width == 32) EMU_FP_UN(INST " v0.4s, v1.4s, #0.0", dst, source); \
        else if (operand_width == 128 && element_width == 64) EMU_FP_UN(INST " v0.2d, v1.2d, #0.0", dst, source); \
        else return false;                                                                                        \
        return true;                                                                                              \
    } while (0)

static inline bool emu_simd_fp_compare_zero_hw(enum arm64_simd_operation operation, void *dst, const void *source, uint32_t operand_width, uint32_t element_width)
{
    if (element_width == 16 && !emu_simd_current_cpu_has_fp16()) return false;

    switch (operation)
    {
    case ARM64_SIMD_OP_FCMEQ:
        EMU_SIMD_FP_COMPARE_ZERO_SHAPE("fcmeq", 0x5EF8D820, 0x0EF8D820, 0x4EF8D820);
    case ARM64_SIMD_OP_FCMGE:
        EMU_SIMD_FP_COMPARE_ZERO_SHAPE("fcmge", 0x7EF8C820, 0x2EF8C820, 0x6EF8C820);
    case ARM64_SIMD_OP_FCMGT:
        EMU_SIMD_FP_COMPARE_ZERO_SHAPE("fcmgt", 0x5EF8C820, 0x0EF8C820, 0x4EF8C820);
    case ARM64_SIMD_OP_FCMLE:
        EMU_SIMD_FP_COMPARE_ZERO_SHAPE("fcmle", 0x7EF8D820, 0x2EF8D820, 0x6EF8D820);
    case ARM64_SIMD_OP_FCMLT:
        EMU_SIMD_FP_COMPARE_ZERO_SHAPE("fcmlt", 0x5EF8E820, 0x0EF8E820, 0x4EF8E820);
    default:
        return false;
    }
}

/* ======================== FP / AdvSIMD：完整执行流程 ======================== */

static inline enum emu_insn_result emu_simulate_fp_simd_insn(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_decoded_insn *decoded, uint64_t pc)
{
    const struct arm64_simd_operands *operands = &decoded->operands.simd;
    enum emu_insn_result result = EMU_INSN_SKIP;

    if (operands->form == ARM64_SIMD_FORM_VECTOR_IMMEDIATE)
    {
        switch (operands->operation)
        {
        case ARM64_SIMD_OP_MOVI:
        case ARM64_SIMD_OP_FMOV:
            if (!emu_simd_materialize_bits_hw(&fp_regs->q[decoded->rd], operands->expanded_immediate, decoded->operand_width)) return EMU_INSN_SKIP;
            break;
        case ARM64_SIMD_OP_MVNI:
        case ARM64_SIMD_OP_ORR:
        case ARM64_SIMD_OP_BIC:
        {
            __uint128_t immediate;

            if (!emu_simd_materialize_bits_hw(&immediate, operands->expanded_immediate, decoded->operand_width)) return EMU_INSN_SKIP;
            if (operands->operation == ARM64_SIMD_OP_MVNI)
            {
                if (decoded->operand_width == 64) EMU_FP_UN("mvn v0.8b, v1.8b", &fp_regs->q[decoded->rd], &immediate);
                else EMU_FP_UN("mvn v0.16b, v1.16b", &fp_regs->q[decoded->rd], &immediate);
            }
            else if (operands->operation == ARM64_SIMD_OP_ORR)
            {
                if (decoded->operand_width == 64) EMU_FP_BIN("orr v0.8b, v1.8b, v2.8b", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rd], &immediate);
                else EMU_FP_BIN("orr v0.16b, v1.16b, v2.16b", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rd], &immediate);
            }
            else
            {
                if (decoded->operand_width == 64) EMU_FP_BIN("bic v0.8b, v1.8b, v2.8b", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rd], &immediate);
                else EMU_FP_BIN("bic v0.16b, v1.16b, v2.16b", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rd], &immediate);
            }
            break;
        }
        default:
            return EMU_INSN_SKIP;
        }
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_SCALAR_COPY)
    {
        uint64_t lane_value;

        if (operands->operation != ARM64_SIMD_OP_DUP_ELEMENT) return EMU_INSN_SKIP;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rn], operands->element_width, operands->lane_index, &lane_value)) return EMU_INSN_SKIP;
        if (!emu_simd_write_scalar_hw(&fp_regs->q[decoded->rd], lane_value, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_COPY)
    {
        uint32_t element_width = operands->element_width;
        uint64_t lane_value;

        switch (operands->operation)
        {
        case ARM64_SIMD_OP_DUP_GENERAL:
            if (!emu_simd_dup_general_hw(&fp_regs->q[decoded->rd], reg_read(regs, decoded->rn), element_width, decoded->operand_width)) return EMU_INSN_SKIP;
            break;
        case ARM64_SIMD_OP_DUP_ELEMENT:
            if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rn], element_width, operands->lane_index, &lane_value)) return EMU_INSN_SKIP;
            if (!emu_simd_dup_general_hw(&fp_regs->q[decoded->rd], lane_value, element_width, decoded->operand_width)) return EMU_INSN_SKIP;
            break;
        case ARM64_SIMD_OP_INS_GENERAL:
            if (!emu_simd_insert_general_hw(&fp_regs->q[decoded->rd], reg_read(regs, decoded->rn), element_width, operands->lane_index)) return EMU_INSN_SKIP;
            break;
        case ARM64_SIMD_OP_INS_ELEMENT:
            if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rn], element_width, operands->source_lane_index, &lane_value)) return EMU_INSN_SKIP;
            if (!emu_simd_insert_general_hw(&fp_regs->q[decoded->rd], lane_value, element_width, operands->lane_index)) return EMU_INSN_SKIP;
            break;
        case ARM64_SIMD_OP_UMOV:
            if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rn], element_width, operands->lane_index, &lane_value)) return EMU_INSN_SKIP;
            reg_write(regs, decoded->rd, lane_value, decoded->operand_width == 64);
            break;
        case ARM64_SIMD_OP_SMOV:
            if (!emu_simd_extract_signed_lane_hw(&fp_regs->q[decoded->rn], element_width, operands->lane_index, decoded->operand_width == 64, &lane_value)) return EMU_INSN_SKIP;
            reg_write(regs, decoded->rd, lane_value, decoded->operand_width == 64);
            break;
        default:
            return EMU_INSN_SKIP;
        }
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_SHIFT)
    {
        if (!emu_simd_shift_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], operands->element_width, decoded->operand_width, operands->immediate)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_FP_COMPARE_ZERO)
    {
        if (!emu_simd_fp_compare_zero_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, operands->element_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_FP_BY_ELEMENT)
    {
        switch (operands->operation)
        {
        case ARM64_SIMD_OP_FMLAL:
        case ARM64_SIMD_OP_FMLSL:
            if (!emu_simd_fhm_by_element_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width, operands->result_element_width, decoded->operand_width, operands->lane_index, operands->flags)) return EMU_INSN_SKIP;
            break;
        case ARM64_SIMD_OP_FCMLA:
            if (!emu_simd_fcma_by_element_hw(&fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width, decoded->operand_width, operands->lane_index, operands->immediate)) return EMU_INSN_SKIP;
            break;
        default:
        {
            uint64_t lane_value;

            if (operands->element_width == 16 && !emu_simd_current_cpu_has_fp16()) return EMU_INSN_SKIP;
            if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], operands->element_width, operands->lane_index, &lane_value)) return EMU_INSN_SKIP;
            if (!emu_simd_fp_by_element_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], lane_value, operands->element_width, decoded->operand_width)) return EMU_INSN_SKIP;
            break;
        }
        }
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_BY_ELEMENT || operands->form == ARM64_SIMD_FORM_SCALAR_BY_ELEMENT)
    {
        if (!emu_simd_extra_by_element_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width, operands->result_element_width, decoded->operand_width, operands->lane_index, operands->flags, operands->form == ARM64_SIMD_FORM_SCALAR_BY_ELEMENT)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_SCALAR_SIMD_3REG)
    {
        if (!emu_simd_scalar_3same_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_PERMUTE)
    {
        if (!emu_simd_permute_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_LOGICAL)
    {
        if (!emu_simd_logical_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_INTEGER_3REG)
    {
        if (!emu_simd_integer_3reg_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_EXTENDED_3REG)
    {
        if (!emu_simd_vector_3same_extra_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width, operands->result_element_width, decoded->operand_width, operands->flags)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_FP_3REG)
    {
        if (!emu_simd_fp_vector_3reg_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_FP_WIDENING_3REG)
    {
        if (!emu_simd_fhm_vector_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width, operands->result_element_width, decoded->operand_width, operands->flags)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_COMPLEX_3REG)
    {
        if (!emu_simd_fcma_vector_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], operands->element_width, decoded->operand_width, operands->immediate)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_FP_UNARY)
    {
        if (!emu_simd_fp_vector_2reg_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], operands->element_width, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_REVERSE)
    {
        if (!emu_simd_rev_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], operands->element_width, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_FP_REDUCE)
    {
        if (!emu_simd_fp_reduce_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], operands->element_width, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_INTEGER_REDUCE)
    {
        if (!emu_simd_integer_reduce_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], operands->element_width, operands->result_element_width, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_NARROW || operands->form == ARM64_SIMD_FORM_SCALAR_NARROW)
    {
        if (!emu_simd_narrow_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], operands->element_width, operands->result_element_width, operands->flags, operands->form == ARM64_SIMD_FORM_SCALAR_NARROW)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_VECTOR_EXTRACT)
    {
        if (operands->immediate >= decoded->operand_width / 8) return EMU_INSN_SKIP;
        if (!emu_simd_ext_hw(&fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, operands->immediate)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_SCALAR_FP_IMMEDIATE)
    {
        if (operands->operation != ARM64_SIMD_OP_FMOV) return EMU_INSN_SKIP;
        if (decoded->operand_width == 16 && !emu_simd_current_cpu_has_fp16()) return EMU_INSN_SKIP;
        if (!emu_simd_write_scalar_hw(&fp_regs->q[decoded->rd], operands->expanded_immediate, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_SCALAR_FP_BINARY)
    {
        if (decoded->operand_width == 16)
        {
            if (!emu_simd_current_cpu_has_fp16() || !emu_fp16_scalar_2source_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm])) return EMU_INSN_SKIP;
        }
        else if (decoded->operand_width == 32)
        {
            switch (operands->operation)
            {
            case ARM64_SIMD_OP_FMUL:
                EMU_FP_BIN("fmul s0, s1, s2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FDIV:
                EMU_FP_BIN("fdiv s0, s1, s2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FADD:
                EMU_FP_BIN("fadd s0, s1, s2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FSUB:
                EMU_FP_BIN("fsub s0, s1, s2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMAX:
                EMU_FP_BIN("fmax s0, s1, s2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMIN:
                EMU_FP_BIN("fmin s0, s1, s2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMAXNM:
                EMU_FP_BIN("fmaxnm s0, s1, s2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMINNM:
                EMU_FP_BIN("fminnm s0, s1, s2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FNMUL:
                EMU_FP_BIN("fnmul s0, s1, s2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }
        else if (decoded->operand_width == 64)
        {
            switch (operands->operation)
            {
            case ARM64_SIMD_OP_FMUL:
                EMU_FP_BIN("fmul d0, d1, d2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FDIV:
                EMU_FP_BIN("fdiv d0, d1, d2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FADD:
                EMU_FP_BIN("fadd d0, d1, d2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FSUB:
                EMU_FP_BIN("fsub d0, d1, d2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMAX:
                EMU_FP_BIN("fmax d0, d1, d2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMIN:
                EMU_FP_BIN("fmin d0, d1, d2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMAXNM:
                EMU_FP_BIN("fmaxnm d0, d1, d2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMINNM:
                EMU_FP_BIN("fminnm d0, d1, d2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FNMUL:
                EMU_FP_BIN("fnmul d0, d1, d2", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm]);
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }
        else return EMU_INSN_SKIP;

        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_SCALAR_FP_UNARY)
    {
        if (decoded->operand_width == 16)
        {
            if (!emu_simd_current_cpu_has_fp16() || !emu_fp16_scalar_1source_hw(operands->operation, operands->rounding_mode, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn])) return EMU_INSN_SKIP;
        }
        else if (decoded->operand_width == 32)
        {
            switch (operands->operation)
            {
            case ARM64_SIMD_OP_FMOV:
                EMU_FP_UN("fmov s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FABS:
                EMU_FP_UN_MERGE("fabs s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FNEG:
                EMU_FP_UN_MERGE("fneg s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FSQRT:
                EMU_FP_UN_MERGE("fsqrt s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FRINT:
                switch (operands->rounding_mode)
                {
                case ARM64_FP_ROUND_NEAREST_EVEN:
                    EMU_FP_UN_MERGE("frintn s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_PLUS_INFINITY:
                    EMU_FP_UN_MERGE("frintp s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_MINUS_INFINITY:
                    EMU_FP_UN_MERGE("frintm s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_ZERO:
                    EMU_FP_UN_MERGE("frintz s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_NEAREST_AWAY:
                    EMU_FP_UN_MERGE("frinta s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_CURRENT_EXACT:
                    EMU_FP_UN_MERGE("frintx s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_CURRENT:
                    EMU_FP_UN_MERGE("frinti s0, s1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                default:
                    return EMU_INSN_SKIP;
                }
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }
        else if (decoded->operand_width == 64)
        {
            switch (operands->operation)
            {
            case ARM64_SIMD_OP_FMOV:
                EMU_FP_UN("fmov d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FABS:
                EMU_FP_UN_MERGE("fabs d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FNEG:
                EMU_FP_UN_MERGE("fneg d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FSQRT:
                EMU_FP_UN_MERGE("fsqrt d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FRINT:
                switch (operands->rounding_mode)
                {
                case ARM64_FP_ROUND_NEAREST_EVEN:
                    EMU_FP_UN_MERGE("frintn d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_PLUS_INFINITY:
                    EMU_FP_UN_MERGE("frintp d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_MINUS_INFINITY:
                    EMU_FP_UN_MERGE("frintm d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_ZERO:
                    EMU_FP_UN_MERGE("frintz d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_NEAREST_AWAY:
                    EMU_FP_UN_MERGE("frinta d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_CURRENT_EXACT:
                    EMU_FP_UN_MERGE("frintx d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_CURRENT:
                    EMU_FP_UN_MERGE("frinti d0, d1", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn]);
                    break;
                default:
                    return EMU_INSN_SKIP;
                }
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }
        else return EMU_INSN_SKIP;

        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_SCALAR_FP_TERNARY)
    {
        if (decoded->operand_width == 16)
        {
            if (!emu_simd_current_cpu_has_fp16() || !emu_fp16_scalar_3source_hw(operands->operation, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra])) return EMU_INSN_SKIP;
        }
        else if (decoded->operand_width == 32)
        {
            if (operands->operation == ARM64_SIMD_OP_FMADD) EMU_FP_TERN("fmadd s0, s1, s2, s3", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FMSUB) EMU_FP_TERN("fmsub s0, s1, s2, s3", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FNMADD) EMU_FP_TERN("fnmadd s0, s1, s2, s3", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FNMSUB) EMU_FP_TERN("fnmsub s0, s1, s2, s3", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra]);
            else return EMU_INSN_SKIP;
        }
        else if (decoded->operand_width == 64)
        {
            if (operands->operation == ARM64_SIMD_OP_FMADD) EMU_FP_TERN("fmadd d0, d1, d2, d3", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FMSUB) EMU_FP_TERN("fmsub d0, d1, d2, d3", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FNMADD) EMU_FP_TERN("fnmadd d0, d1, d2, d3", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FNMSUB) EMU_FP_TERN("fnmsub d0, d1, d2, d3", &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra]);
            else return EMU_INSN_SKIP;
        }
        else return EMU_INSN_SKIP;

        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_SCALAR_COMPARE)
    {
        bool zero = (operands->flags & ARM64_SIMD_FLAG_COMPARE_ZERO) != 0;
        bool signal = operands->operation == ARM64_SIMD_OP_FCMPE;
        uint64_t nzcv;

        if (decoded->operand_width == 16)
        {
            if (!emu_simd_current_cpu_has_fp16() || !emu_fp16_compare_hw(signal, zero, &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &nzcv)) return EMU_INSN_SKIP;
        }
        else if (decoded->operand_width == 32)
        {
            if (zero)
            {
                if (signal)
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmpe s1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs->q[decoded->rn])
                                 : "memory", "cc", "v1");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmp s1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs->q[decoded->rn])
                                 : "memory", "cc", "v1");
            }
            else
            {
                if (signal)
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "ldr q2, [%2]\n"
                                 "fcmpe s1, s2\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs->q[decoded->rn]), "r"(&fp_regs->q[decoded->rm])
                                 : "memory", "cc", "v1", "v2");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "ldr q2, [%2]\n"
                                 "fcmp s1, s2\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs->q[decoded->rn]), "r"(&fp_regs->q[decoded->rm])
                                 : "memory", "cc", "v1", "v2");
            }
        }
        else if (decoded->operand_width == 64)
        {
            if (zero)
            {
                if (signal)
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmpe d1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs->q[decoded->rn])
                                 : "memory", "cc", "v1");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmp d1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs->q[decoded->rn])
                                 : "memory", "cc", "v1");
            }
            else
            {
                if (signal)
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "ldr q2, [%2]\n"
                                 "fcmpe d1, d2\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs->q[decoded->rn]), "r"(&fp_regs->q[decoded->rm])
                                 : "memory", "cc", "v1", "v2");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "ldr q2, [%2]\n"
                                 "fcmp d1, d2\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs->q[decoded->rn]), "r"(&fp_regs->q[decoded->rm])
                                 : "memory", "cc", "v1", "v2");
            }
        }
        else return EMU_INSN_SKIP;

        emu_write_nzcv(regs, nzcv);
        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_SCALAR_CONDITIONAL_COMPARE)
    {
        bool signal = operands->operation == ARM64_SIMD_OP_FCCMPE;
        uint64_t nzcv;

        if (decoded->operand_width == 16 && !emu_simd_current_cpu_has_fp16()) return EMU_INSN_SKIP;
        if (!emu_cond_holds_hw(regs->pstate, operands->condition))
        {
            emu_write_nzcv(regs, (uint64_t)operands->immediate << 28);
            result = EMU_INSN_HANDLED;
        }
        else if (decoded->operand_width == 16)
        {
            if (!emu_fp16_compare_hw(signal, false, &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &nzcv)) return EMU_INSN_SKIP;
            emu_write_nzcv(regs, nzcv);
            result = EMU_INSN_HANDLED;
        }
        else if (decoded->operand_width == 32)
        {
            if (signal)
                asm volatile(".arch_extension fp\n.arch_extension simd\n"
                             "ldr q1, [%1]\n"
                             "ldr q2, [%2]\n"
                             "fcmpe s1, s2\n"
                             "mrs %0, nzcv\n"
                             : "=r"(nzcv)
                             : "r"(&fp_regs->q[decoded->rn]), "r"(&fp_regs->q[decoded->rm])
                             : "memory", "cc", "v1", "v2");
            else
                asm volatile(".arch_extension fp\n.arch_extension simd\n"
                             "ldr q1, [%1]\n"
                             "ldr q2, [%2]\n"
                             "fcmp s1, s2\n"
                             "mrs %0, nzcv\n"
                             : "=r"(nzcv)
                             : "r"(&fp_regs->q[decoded->rn]), "r"(&fp_regs->q[decoded->rm])
                             : "memory", "cc", "v1", "v2");
            emu_write_nzcv(regs, nzcv);
            result = EMU_INSN_HANDLED;
        }
        else if (decoded->operand_width == 64)
        {
            if (signal)
                asm volatile(".arch_extension fp\n.arch_extension simd\n"
                             "ldr q1, [%1]\n"
                             "ldr q2, [%2]\n"
                             "fcmpe d1, d2\n"
                             "mrs %0, nzcv\n"
                             : "=r"(nzcv)
                             : "r"(&fp_regs->q[decoded->rn]), "r"(&fp_regs->q[decoded->rm])
                             : "memory", "cc", "v1", "v2");
            else
                asm volatile(".arch_extension fp\n.arch_extension simd\n"
                             "ldr q1, [%1]\n"
                             "ldr q2, [%2]\n"
                             "fcmp d1, d2\n"
                             "mrs %0, nzcv\n"
                             : "=r"(nzcv)
                             : "r"(&fp_regs->q[decoded->rn]), "r"(&fp_regs->q[decoded->rm])
                             : "memory", "cc", "v1", "v2");
            emu_write_nzcv(regs, nzcv);
            result = EMU_INSN_HANDLED;
        }
        else return EMU_INSN_SKIP;
    }
    else if (operands->form == ARM64_SIMD_FORM_SCALAR_SELECT)
    {
        if (decoded->operand_width == 16 && !emu_simd_current_cpu_has_fp16()) return EMU_INSN_SKIP;
        if (!emu_fp_select_hw(&fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], regs->pstate, operands->condition, decoded->operand_width)) return EMU_INSN_SKIP;

        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_FP_GPR_TRANSFER)
    {
        bool sf = decoded->operand_width == 64;
        bool gp_to_fp = operands->operation == ARM64_SIMD_OP_FMOV_GENERAL_TO_FP;
        uint64_t value;

        if (gp_to_fp)
        {
            if (!emu_simd_write_scalar_hw(&fp_regs->q[decoded->rd], reg_read(regs, decoded->rn), decoded->operand_width)) return EMU_INSN_SKIP;
        }
        else
        {
            if (!emu_simd_read_scalar_hw(&fp_regs->q[decoded->rn], decoded->operand_width, &value)) return EMU_INSN_SKIP;
            reg_write(regs, decoded->rd, value, sf);
        }

        result = EMU_INSN_HANDLED;
    }
    else if (operands->form == ARM64_SIMD_FORM_CONVERT)
    {
        enum arm64_simd_operation operation = operands->operation;
        uint32_t rd = decoded->rd;
        uint32_t rn = decoded->rn;
        uint32_t wout;
        uint64_t xout;

        switch (operation)
        {
        case ARM64_SIMD_OP_SCVTF_S_W:
            EMU_GPR_TO_FP_MERGE("scvtf s0, %w1", &fp_regs->q[rd], (uint32_t)reg_read(regs, rn));
            break;
        case ARM64_SIMD_OP_SCVTF_S_X:
            EMU_GPR_TO_FP_MERGE("scvtf s0, %1", &fp_regs->q[rd], reg_read(regs, rn));
            break;
        case ARM64_SIMD_OP_SCVTF_D_W:
            EMU_GPR_TO_FP_MERGE("scvtf d0, %w1", &fp_regs->q[rd], (uint32_t)reg_read(regs, rn));
            break;
        case ARM64_SIMD_OP_SCVTF_D_X:
            EMU_GPR_TO_FP_MERGE("scvtf d0, %1", &fp_regs->q[rd], reg_read(regs, rn));
            break;
        case ARM64_SIMD_OP_UCVTF_S_W:
            EMU_GPR_TO_FP_MERGE("ucvtf s0, %w1", &fp_regs->q[rd], (uint32_t)reg_read(regs, rn));
            break;
        case ARM64_SIMD_OP_UCVTF_S_X:
            EMU_GPR_TO_FP_MERGE("ucvtf s0, %1", &fp_regs->q[rd], reg_read(regs, rn));
            break;
        case ARM64_SIMD_OP_UCVTF_D_W:
            EMU_GPR_TO_FP_MERGE("ucvtf d0, %w1", &fp_regs->q[rd], (uint32_t)reg_read(regs, rn));
            break;
        case ARM64_SIMD_OP_UCVTF_D_X:
            EMU_GPR_TO_FP_MERGE("ucvtf d0, %1", &fp_regs->q[rd], reg_read(regs, rn));
            break;
        case ARM64_SIMD_OP_FCVT_TO_SIGNED:
        case ARM64_SIMD_OP_FCVT_TO_UNSIGNED:
        {
            bool signed_result = operation == ARM64_SIMD_OP_FCVT_TO_SIGNED;

            if (signed_result)
            {
                switch (operands->rounding_mode)
                {
                case ARM64_FP_ROUND_NEAREST_EVEN:
                    EMU_FP_CONVERT_GPR("fcvtns");
                    break;
                case ARM64_FP_ROUND_PLUS_INFINITY:
                    EMU_FP_CONVERT_GPR("fcvtps");
                    break;
                case ARM64_FP_ROUND_MINUS_INFINITY:
                    EMU_FP_CONVERT_GPR("fcvtms");
                    break;
                case ARM64_FP_ROUND_ZERO:
                    EMU_FP_CONVERT_GPR("fcvtzs");
                    break;
                case ARM64_FP_ROUND_NEAREST_AWAY:
                    EMU_FP_CONVERT_GPR("fcvtas");
                    break;
                default:
                    return EMU_INSN_SKIP;
                }
            }
            else
            {
                switch (operands->rounding_mode)
                {
                case ARM64_FP_ROUND_NEAREST_EVEN:
                    EMU_FP_CONVERT_GPR("fcvtnu");
                    break;
                case ARM64_FP_ROUND_PLUS_INFINITY:
                    EMU_FP_CONVERT_GPR("fcvtpu");
                    break;
                case ARM64_FP_ROUND_MINUS_INFINITY:
                    EMU_FP_CONVERT_GPR("fcvtmu");
                    break;
                case ARM64_FP_ROUND_ZERO:
                    EMU_FP_CONVERT_GPR("fcvtzu");
                    break;
                case ARM64_FP_ROUND_NEAREST_AWAY:
                    EMU_FP_CONVERT_GPR("fcvtau");
                    break;
                default:
                    return EMU_INSN_SKIP;
                }
            }
            break;
        }
        case ARM64_SIMD_OP_FCVT_S_D:
            EMU_FP_UN_MERGE("fcvt s0, d1", &fp_regs->q[rd], &fp_regs->q[rn]);
            break;
        case ARM64_SIMD_OP_FCVT_D_S:
            EMU_FP_UN_MERGE("fcvt d0, s1", &fp_regs->q[rd], &fp_regs->q[rn]);
            break;
        case ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD:
            switch (operands->rounding_mode)
            {
            case ARM64_FP_ROUND_NEAREST_EVEN:
                EMU_FP_CONVERT_SIMD("fcvtns", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_PLUS_INFINITY:
                EMU_FP_CONVERT_SIMD("fcvtps", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_MINUS_INFINITY:
                EMU_FP_CONVERT_SIMD("fcvtms", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_ZERO:
                EMU_FP_CONVERT_SIMD("fcvtzs", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_NEAREST_AWAY:
                EMU_FP_CONVERT_SIMD("fcvtas", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            default:
                return EMU_INSN_SKIP;
            }
            break;
        case ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD:
            switch (operands->rounding_mode)
            {
            case ARM64_FP_ROUND_NEAREST_EVEN:
                EMU_FP_CONVERT_SIMD("fcvtnu", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_PLUS_INFINITY:
                EMU_FP_CONVERT_SIMD("fcvtpu", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_MINUS_INFINITY:
                EMU_FP_CONVERT_SIMD("fcvtmu", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_ZERO:
                EMU_FP_CONVERT_SIMD("fcvtzu", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_NEAREST_AWAY:
                EMU_FP_CONVERT_SIMD("fcvtau", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
                break;
            default:
                return EMU_INSN_SKIP;
            }
            break;
        case ARM64_SIMD_OP_SCVTF_SIMD:
            EMU_FP_CONVERT_SIMD("scvtf", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
            break;
        case ARM64_SIMD_OP_UCVTF_SIMD:
            EMU_FP_CONVERT_SIMD("ucvtf", &fp_regs->q[rd], &fp_regs->q[rn], decoded->operand_width, operands->element_width);
            break;
        default:
            return EMU_INSN_SKIP;
        }

        result = EMU_INSN_HANDLED;
    }
    if (result != EMU_INSN_HANDLED) return result;

    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

/* ======================== 数据处理类：私有硬件模板 ======================== */

static inline bool emu_cond_select_hw(enum arm64_operation operation, uint64_t a, uint64_t b, uint64_t pstate, uint32_t condition, bool sf, uint64_t *result)
{
    uint32_t take = emu_cond_holds_hw(pstate, condition);

    if (!result) return false;

    if (sf)
    {
        switch (operation)
        {
        case ARM64_OPERATION_CSEL:
            asm volatile("cmp %w3, #0\n"
                         "csel %0, %1, %2, ne\n"
                         : "=r"(*result)
                         : "r"(a), "r"(b), "r"(take)
                         : "cc");
            return true;
        case ARM64_OPERATION_CSINC:
            asm volatile("cmp %w3, #0\n"
                         "csinc %0, %1, %2, ne\n"
                         : "=r"(*result)
                         : "r"(a), "r"(b), "r"(take)
                         : "cc");
            return true;
        case ARM64_OPERATION_CSINV:
            asm volatile("cmp %w3, #0\n"
                         "csinv %0, %1, %2, ne\n"
                         : "=r"(*result)
                         : "r"(a), "r"(b), "r"(take)
                         : "cc");
            return true;
        case ARM64_OPERATION_CSNEG:
            asm volatile("cmp %w3, #0\n"
                         "csneg %0, %1, %2, ne\n"
                         : "=r"(*result)
                         : "r"(a), "r"(b), "r"(take)
                         : "cc");
            return true;
        default:
            return false;
        }
    }

    switch (operation)
    {
    case ARM64_OPERATION_CSEL:
        asm volatile("cmp %w3, #0\n"
                     "csel %w0, %w1, %w2, ne\n"
                     : "=r"(*result)
                     : "r"((uint32_t)a), "r"((uint32_t)b), "r"(take)
                     : "cc");
        return true;
    case ARM64_OPERATION_CSINC:
        asm volatile("cmp %w3, #0\n"
                     "csinc %w0, %w1, %w2, ne\n"
                     : "=r"(*result)
                     : "r"((uint32_t)a), "r"((uint32_t)b), "r"(take)
                     : "cc");
        return true;
    case ARM64_OPERATION_CSINV:
        asm volatile("cmp %w3, #0\n"
                     "csinv %w0, %w1, %w2, ne\n"
                     : "=r"(*result)
                     : "r"((uint32_t)a), "r"((uint32_t)b), "r"(take)
                     : "cc");
        return true;
    case ARM64_OPERATION_CSNEG:
        asm volatile("cmp %w3, #0\n"
                     "csneg %w0, %w1, %w2, ne\n"
                     : "=r"(*result)
                     : "r"((uint32_t)a), "r"((uint32_t)b), "r"(take)
                     : "cc");
        return true;
    default:
        return false;
    }
}

/* ADD/SUB 扩展寄存器：option 000..111 对应 UXT 或 SXT 变体，结果再左移 shift 位。 */
static inline uint64_t emu_extend_reg(uint64_t val, uint32_t option, uint32_t shift)
{
    uint64_t x;

    switch (option)
    {
    case 0:
        asm volatile("uxtb %w0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break;
    case 1:
        asm volatile("uxth %w0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break;
    case 2:
        asm volatile("mov %w0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break;
    case 4:
        asm volatile("sxtb %0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break;
    case 5:
        asm volatile("sxth %0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break;
    case 6:
        asm volatile("sxtw %0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break;
    default:
        asm volatile("mov %0, %1\n" : "=r"(x) : "r"(val));
        break;
    }
    if (!shift) return x;
    asm volatile("lslv %0, %1, %2\n" : "=r"(x) : "r"(x), "r"((uint64_t)shift) : "cc");
    return x;
}

#define EMU_INT_BIN64(INST, A, B)                                                                         \
    ({                                                                                                    \
        uint64_t __ret;                                                                                   \
        asm volatile(INST " %0, %1, %2\n" : "=r"(__ret) : "r"((uint64_t)(A)), "r"((uint64_t)(B)) : "cc"); \
        __ret;                                                                                            \
    })

#define EMU_INT_BIN32(INST, A, B)                                                                            \
    ({                                                                                                       \
        uint32_t __ret;                                                                                      \
        asm volatile(INST " %w0, %w1, %w2\n" : "=r"(__ret) : "r"((uint32_t)(A)), "r"((uint32_t)(B)) : "cc"); \
        __ret;                                                                                               \
    })

static inline uint64_t emu_addsub_hw(uint64_t a, uint64_t b, bool op_sub, bool setflags, bool sf, uint64_t *nzcv)
{
    uint64_t result64, flags;
    uint32_t result32;

    if (sf)
    {
        if (setflags)
        {
            if (op_sub)
                asm volatile("subs %0, %2, %3\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result64), "=r"(flags)
                             : "r"(a), "r"(b)
                             : "cc");
            else
                asm volatile("adds %0, %2, %3\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result64), "=r"(flags)
                             : "r"(a), "r"(b)
                             : "cc");
            *nzcv = flags;
            return result64;
        }
        return op_sub ? EMU_INT_BIN64("sub", a, b) : EMU_INT_BIN64("add", a, b);
    }

    if (setflags)
    {
        if (op_sub)
            asm volatile("subs %w0, %w2, %w3\n"
                         "mrs %1, nzcv\n"
                         : "=r"(result32), "=r"(flags)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
        else
            asm volatile("adds %w0, %w2, %w3\n"
                         "mrs %1, nzcv\n"
                         : "=r"(result32), "=r"(flags)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
        *nzcv = flags;
        return result32;
    }
    return op_sub ? EMU_INT_BIN32("sub", a, b) : EMU_INT_BIN32("add", a, b);
}

static inline uint64_t emu_logic_hw(uint64_t a, uint64_t b, uint32_t opc, bool invert, bool sf, uint64_t *nzcv)
{
    uint64_t result64, flags;
    uint32_t result32;

    if (sf)
    {
        switch (opc)
        {
        case 0:
            return invert ? EMU_INT_BIN64("bic", a, b) : EMU_INT_BIN64("and", a, b);
        case 1:
            return invert ? EMU_INT_BIN64("orn", a, b) : EMU_INT_BIN64("orr", a, b);
        case 2:
            return invert ? EMU_INT_BIN64("eon", a, b) : EMU_INT_BIN64("eor", a, b);
        default:
            if (invert)
                asm volatile("bics %0, %2, %3\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result64), "=r"(flags)
                             : "r"(a), "r"(b)
                             : "cc");
            else
                asm volatile("ands %0, %2, %3\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result64), "=r"(flags)
                             : "r"(a), "r"(b)
                             : "cc");
            *nzcv = flags;
            return result64;
        }
    }

    switch (opc)
    {
    case 0:
        return invert ? EMU_INT_BIN32("bic", a, b) : EMU_INT_BIN32("and", a, b);
    case 1:
        return invert ? EMU_INT_BIN32("orn", a, b) : EMU_INT_BIN32("orr", a, b);
    case 2:
        return invert ? EMU_INT_BIN32("eon", a, b) : EMU_INT_BIN32("eor", a, b);
    default:
        if (invert)
            asm volatile("bics %w0, %w2, %w3\n"
                         "mrs %1, nzcv\n"
                         : "=r"(result32), "=r"(flags)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
        else
            asm volatile("ands %w0, %w2, %w3\n"
                         "mrs %1, nzcv\n"
                         : "=r"(result32), "=r"(flags)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
        *nzcv = flags;
        return result32;
    }
}

static inline uint64_t emu_minmax_hw(uint64_t a, uint64_t b, bool is_min, bool is_unsigned, bool sf)
{
    uint64_t result64;
    uint32_t result32;

    if (sf)
    {
        if (is_unsigned)
        {
            if (is_min)
                asm volatile("cmp %1, %2\n"
                             "csel %0, %1, %2, lo\n"
                             : "=r"(result64)
                             : "r"(a), "r"(b)
                             : "cc");
            else
                asm volatile("cmp %1, %2\n"
                             "csel %0, %1, %2, hi\n"
                             : "=r"(result64)
                             : "r"(a), "r"(b)
                             : "cc");
        }
        else
        {
            if (is_min)
                asm volatile("cmp %1, %2\n"
                             "csel %0, %1, %2, lt\n"
                             : "=r"(result64)
                             : "r"(a), "r"(b)
                             : "cc");
            else
                asm volatile("cmp %1, %2\n"
                             "csel %0, %1, %2, gt\n"
                             : "=r"(result64)
                             : "r"(a), "r"(b)
                             : "cc");
        }
        return result64;
    }

    if (is_unsigned)
    {
        if (is_min)
            asm volatile("cmp %w1, %w2\n"
                         "csel %w0, %w1, %w2, lo\n"
                         : "=r"(result32)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
        else
            asm volatile("cmp %w1, %w2\n"
                         "csel %w0, %w1, %w2, hi\n"
                         : "=r"(result32)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
    }
    else
    {
        if (is_min)
            asm volatile("cmp %w1, %w2\n"
                         "csel %w0, %w1, %w2, lt\n"
                         : "=r"(result32)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
        else
            asm volatile("cmp %w1, %w2\n"
                         "csel %w0, %w1, %w2, gt\n"
                         : "=r"(result32)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
    }
    return result32;
}

static inline uint64_t emu_sign_extend_byte_hw(uint64_t value)
{
    uint64_t result;

    asm volatile("sxtb %0, %w1\n" : "=r"(result) : "r"((uint32_t)value));
    return result;
}

static inline uint64_t emu_dp_mask(bool sf)
{
    return sf ? ~0ULL : 0xFFFFFFFFULL;
}

static inline uint64_t emu_extract_bits(uint64_t high, uint64_t low, uint32_t shift, bool sf)
{
    uint64_t result, left, inverse;
    uint32_t result32, left32, inverse32;

    if (sf)
    {
        asm volatile("neg %2, %5\n"
                     "lslv %1, %3, %2\n"
                     "lsrv %0, %4, %5\n"
                     "cmp %5, #0\n"
                     "csel %1, xzr, %1, eq\n"
                     "orr %0, %0, %1\n"
                     : "=&r"(result), "=&r"(left), "=&r"(inverse)
                     : "r"(high), "r"(low), "r"((uint64_t)shift)
                     : "cc");
        return result;
    }

    asm volatile("neg %w2, %w5\n"
                 "lslv %w1, %w3, %w2\n"
                 "lsrv %w0, %w4, %w5\n"
                 "cmp %w5, #0\n"
                 "csel %w1, wzr, %w1, eq\n"
                 "orr %w0, %w0, %w1\n"
                 : "=&r"(result32), "=&r"(left32), "=&r"(inverse32)
                 : "r"((uint32_t)high), "r"((uint32_t)low), "r"(shift)
                 : "cc");
    return result32;
}

static inline bool emu_bitfield_hw(enum arm64_operation operation, uint64_t src, uint64_t dst, uint32_t immr, uint64_t wmask, uint64_t tmask, bool sf, uint64_t *result)
{
    uint64_t bot = emu_extract_bits(src, src, immr, sf);
    uint64_t result64, temporary64, auxiliary64;
    uint32_t result32, temporary32, auxiliary32;

    if (!result) return false;

    if (sf)
    {
        asm volatile("and %0, %1, %2\n" : "=r"(bot) : "r"(bot), "r"(wmask));
        switch (operation)
        {
        case ARM64_OPERATION_SBFM:
            asm volatile("add %1, %5, #1\n"
                         "lsr %1, %1, #1\n"
                         "cmp %1, #0\n"
                         "csel %1, %6, %1, eq\n"
                         "and %0, %4, %5\n"
                         "mvn %2, %5\n"
                         "orr %2, %0, %2\n"
                         "tst %4, %1\n"
                         "csel %0, %2, %0, ne\n"
                         : "=&r"(result64), "=&r"(temporary64), "=&r"(auxiliary64)
                         : "0"(0ULL), "r"(bot), "r"(tmask), "r"(1ULL << 63)
                         : "cc");
            break;
        case ARM64_OPERATION_BFM:
            asm volatile("and %2, %4, %5\n"
                         "bic %0, %3, %2\n"
                         "and %1, %6, %2\n"
                         "orr %0, %0, %1\n"
                         : "=&r"(result64), "=&r"(temporary64), "=&r"(auxiliary64)
                         : "r"(dst), "r"(wmask), "r"(tmask), "r"(bot));
            break;
        case ARM64_OPERATION_UBFM:
            asm volatile("and %0, %1, %2\n" : "=r"(result64) : "r"(bot), "r"(tmask));
            break;
        default:
            return false;
        }
        *result = result64;
        return true;
    }

    asm volatile("and %w0, %w1, %w2\n" : "=r"(bot) : "r"((uint32_t)bot), "r"((uint32_t)wmask));
    switch (operation)
    {
    case ARM64_OPERATION_SBFM:
        asm volatile("add %w1, %w5, #1\n"
                     "lsr %w1, %w1, #1\n"
                     "cmp %w1, #0\n"
                     "csel %w1, %w6, %w1, eq\n"
                     "and %w0, %w4, %w5\n"
                     "mvn %w2, %w5\n"
                     "orr %w2, %w0, %w2\n"
                     "tst %w4, %w1\n"
                     "csel %w0, %w2, %w0, ne\n"
                     : "=&r"(result32), "=&r"(temporary32), "=&r"(auxiliary32)
                     : "0"(0U), "r"((uint32_t)bot), "r"((uint32_t)tmask), "r"(1U << 31)
                     : "cc");
        break;
    case ARM64_OPERATION_BFM:
        asm volatile("and %w2, %w4, %w5\n"
                     "bic %w0, %w3, %w2\n"
                     "and %w1, %w6, %w2\n"
                     "orr %w0, %w0, %w1\n"
                     : "=&r"(result32), "=&r"(temporary32), "=&r"(auxiliary32)
                     : "r"((uint32_t)dst), "r"((uint32_t)wmask), "r"((uint32_t)tmask), "r"((uint32_t)bot));
        break;
    case ARM64_OPERATION_UBFM:
        asm volatile("and %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)bot), "r"((uint32_t)tmask));
        break;
    default:
        return false;
    }
    *result = result32;
    return true;
}

static inline bool emu_move_wide_hw(enum arm64_operation operation, uint64_t dst, uint64_t immediate, uint32_t shift, bool sf, uint64_t *result)
{
    uint64_t result64, shifted64, mask64;
    uint32_t result32, shifted32, mask32;

    if (!result) return false;

    if (sf)
    {
        switch (operation)
        {
        case ARM64_OPERATION_MOVN:
            asm volatile("lslv %0, %1, %2\n"
                         "mvn %0, %0\n"
                         : "=&r"(result64)
                         : "r"(immediate), "r"((uint64_t)shift));
            break;
        case ARM64_OPERATION_MOVZ:
            asm volatile("lslv %0, %1, %2\n" : "=r"(result64) : "r"(immediate), "r"((uint64_t)shift));
            break;
        case ARM64_OPERATION_MOVK:
            asm volatile("lslv %1, %4, %5\n"
                         "lslv %2, %6, %5\n"
                         "bic %0, %3, %2\n"
                         "orr %0, %0, %1\n"
                         : "=&r"(result64), "=&r"(shifted64), "=&r"(mask64)
                         : "r"(dst), "r"(immediate), "r"((uint64_t)shift), "r"(0xFFFFULL));
            break;
        default:
            return false;
        }
        *result = result64;
        return true;
    }

    switch (operation)
    {
    case ARM64_OPERATION_MOVN:
        asm volatile("lslv %w0, %w1, %w2\n"
                     "mvn %w0, %w0\n"
                     : "=&r"(result32)
                     : "r"((uint32_t)immediate), "r"(shift));
        break;
    case ARM64_OPERATION_MOVZ:
        asm volatile("lslv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)immediate), "r"(shift));
        break;
    case ARM64_OPERATION_MOVK:
        asm volatile("lslv %w1, %w4, %w5\n"
                     "lslv %w2, %w6, %w5\n"
                     "bic %w0, %w3, %w2\n"
                     "orr %w0, %w0, %w1\n"
                     : "=&r"(result32), "=&r"(shifted32), "=&r"(mask32)
                     : "r"((uint32_t)dst), "r"((uint32_t)immediate), "r"(shift), "r"(0xFFFFU));
        break;
    default:
        return false;
    }
    *result = result32;
    return true;
}

static inline uint64_t emu_dp_shift_hw(uint64_t value, uint32_t type, uint32_t amount, bool sf)
{
    uint64_t result;

    if (sf)
    {
        switch (type)
        {
        case 0:
            asm volatile("lslv %0, %1, %2\n" : "=r"(result) : "r"(value), "r"((uint64_t)amount) : "cc");
            break;
        case 1:
            asm volatile("lsrv %0, %1, %2\n" : "=r"(result) : "r"(value), "r"((uint64_t)amount) : "cc");
            break;
        case 2:
            asm volatile("asrv %0, %1, %2\n" : "=r"(result) : "r"(value), "r"((uint64_t)amount) : "cc");
            break;
        default:
            asm volatile("rorv %0, %1, %2\n" : "=r"(result) : "r"(value), "r"((uint64_t)amount) : "cc");
            break;
        }
    }
    else
    {
        uint32_t result32;

        switch (type)
        {
        case 0:
            asm volatile("lslv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)value), "r"(amount) : "cc");
            break;
        case 1:
            asm volatile("lsrv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)value), "r"(amount) : "cc");
            break;
        case 2:
            asm volatile("asrv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)value), "r"(amount) : "cc");
            break;
        default:
            asm volatile("rorv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)value), "r"(amount) : "cc");
            break;
        }
        result = result32;
    }

    return result;
}

static inline uint64_t emu_dp_rbit_hw(uint64_t value, bool sf)
{
    uint64_t result;

    if (sf) asm volatile("rbit %0, %1\n" : "=r"(result) : "r"(value) : "cc");
    else
    {
        uint32_t result32;

        asm volatile("rbit %w0, %w1\n" : "=r"(result32) : "r"((uint32_t)value) : "cc");
        result = result32;
    }
    return result;
}

static inline uint64_t emu_dp_rev16_hw(uint64_t value, bool sf)
{
    uint64_t result;

    if (sf) asm volatile("rev16 %0, %1\n" : "=r"(result) : "r"(value) : "cc");
    else
    {
        uint32_t result32;

        asm volatile("rev16 %w0, %w1\n" : "=r"(result32) : "r"((uint32_t)value) : "cc");
        result = result32;
    }
    return result;
}

static inline uint64_t emu_dp_rev32_hw(uint64_t value, bool sf)
{
    uint64_t result;

    if (sf) asm volatile("rev32 %0, %1\n" : "=r"(result) : "r"(value) : "cc");
    else
    {
        uint32_t result32;

        asm volatile("rev %w0, %w1\n" : "=r"(result32) : "r"((uint32_t)value) : "cc");
        result = result32;
    }
    return result;
}

static inline uint64_t emu_dp_rev64_hw(uint64_t value)
{
    uint64_t result;

    asm volatile("rev %0, %1\n" : "=r"(result) : "r"(value) : "cc");
    return result;
}

static inline uint64_t emu_dp_clz_hw(uint64_t value, bool sf)
{
    uint64_t result;

    if (sf) asm volatile("clz %0, %1\n" : "=r"(result) : "r"(value) : "cc");
    else
    {
        uint32_t result32;

        asm volatile("clz %w0, %w1\n" : "=r"(result32) : "r"((uint32_t)value) : "cc");
        result = result32;
    }
    return result;
}

static inline uint64_t emu_dp_cls_hw(uint64_t value, bool sf)
{
    uint64_t result;

    if (sf) asm volatile("cls %0, %1\n" : "=r"(result) : "r"(value) : "cc");
    else
    {
        uint32_t result32;

        asm volatile("cls %w0, %w1\n" : "=r"(result32) : "r"((uint32_t)value) : "cc");
        result = result32;
    }
    return result;
}

static inline uint64_t emu_dp_ctz_hw(uint64_t value, bool sf)
{
    return emu_dp_clz_hw(emu_dp_rbit_hw(value, sf), sf);
}

static inline uint32_t emu_dp_count_bits_hw(uint64_t value, bool sf)
{
    __uint128_t saved_q0;
    uint32_t result;

    if (sf)
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "str q0, [%2]\n"
                     "movi v0.2d, #0\n"
                     "fmov d0, %1\n"
                     "cnt v0.8b, v0.8b\n"
                     "addv b0, v0.8b\n"
                     "umov %w0, v0.b[0]\n"
                     "ldr q0, [%2]\n"
                     : "=&r"(result)
                     : "r"(value), "r"(&saved_q0)
                     : "memory", "cc");
    else
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "str q0, [%2]\n"
                     "movi v0.2d, #0\n"
                     "fmov s0, %w1\n"
                     "cnt v0.8b, v0.8b\n"
                     "addv b0, v0.8b\n"
                     "umov %w0, v0.b[0]\n"
                     "ldr q0, [%2]\n"
                     : "=&r"(result)
                     : "r"((uint32_t)value), "r"(&saved_q0)
                     : "memory", "cc");
    return result;
}

static inline uint32_t emu_dp_crc32b_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile(".arch_extension crc\ncrc32b %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32h_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile(".arch_extension crc\ncrc32h %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32w_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile(".arch_extension crc\ncrc32w %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32x_hw(uint32_t accumulator, uint64_t value)
{
    uint32_t result;

    asm volatile(".arch_extension crc\ncrc32x %w0, %w1, %2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32cb_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile(".arch_extension crc\ncrc32cb %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32ch_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile(".arch_extension crc\ncrc32ch %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32cw_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile(".arch_extension crc\ncrc32cw %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32cx_hw(uint32_t accumulator, uint64_t value)
{
    uint32_t result;

    asm volatile(".arch_extension crc\ncrc32cx %w0, %w1, %2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

/* ======================== 数据处理类：完整执行流程 ======================== */

static inline enum emu_insn_result emu_simulate_data_processing_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded)
{
    const struct arm64_data_operands *operands = &decoded->operands.data;
    bool sf = decoded->operand_width == 64;

    if (decoded->opcode == ARM64_OP_ADR || decoded->opcode == ARM64_OP_ADRP)
    {
        uint64_t base = decoded->opcode == ARM64_OP_ADRP ? regs->pc & ~0xFFFULL : regs->pc;
        uint64_t target = base + decoded->operands.pc_relative.offset;

        if (decoded->rd != 31) regs->regs[decoded->rd] = target;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_ADD_SUB_IMMEDIATE)
    {
        bool setflags = (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) != 0;
        uint64_t a = addr_reg_read(regs, decoded->rn);
        uint64_t nzcv = 0;
        uint64_t result = emu_addsub_hw(a, operands->immediate, decoded->operation == ARM64_OPERATION_SUB, setflags, sf, &nzcv);

        if (setflags)
        {
            emu_write_nzcv(regs, nzcv);
            reg_write(regs, decoded->rd, result, sf);
        }
        else
        {
            addr_reg_write(regs, decoded->rd, result);
        }
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_MIN_MAX_IMMEDIATE)
    {
        bool is_min = decoded->operation == ARM64_OPERATION_SMIN || decoded->operation == ARM64_OPERATION_UMIN;
        bool is_unsigned = decoded->operation == ARM64_OPERATION_UMAX || decoded->operation == ARM64_OPERATION_UMIN;
        uint64_t a = reg_read(regs, decoded->rn) & emu_dp_mask(sf);
        uint64_t b = is_unsigned ? operands->immediate : emu_sign_extend_byte_hw(operands->immediate) & emu_dp_mask(sf);
        uint64_t result = emu_minmax_hw(a, b, is_min, is_unsigned, sf);

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_LOGICAL_IMMEDIATE)
    {
        uint32_t opc = decoded->operation == ARM64_OPERATION_AND ? 0 : decoded->operation == ARM64_OPERATION_ORR ? 1 : decoded->operation == ARM64_OPERATION_EOR ? 2 : 3;
        uint64_t a = reg_read(regs, decoded->rn) & emu_dp_mask(sf);
        uint64_t nzcv = 0;
        uint64_t result = emu_logic_hw(a, operands->immediate, opc, false, sf, &nzcv);

        if (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) emu_write_nzcv(regs, nzcv);

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_BITFIELD)
    {
        uint64_t src = reg_read(regs, decoded->rn) & emu_dp_mask(sf);
        uint64_t dst = reg_read(regs, decoded->rd) & emu_dp_mask(sf);
        uint64_t result;
        if (!emu_bitfield_hw(decoded->operation, src, dst, operands->immr, operands->wmask, operands->tmask, sf, &result)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_EXTRACT)
    {
        uint64_t result = emu_extract_bits(reg_read(regs, decoded->rn), reg_read(regs, decoded->rm), operands->shift_amount, sf);

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_MOVE_WIDE)
    {
        uint64_t result;

        if (!emu_move_wide_hw(decoded->operation, reg_read(regs, decoded->rd), operands->immediate, operands->shift_amount, sf, &result)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_ADD_SUB_SHIFTED)
    {
        bool setflags = (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) != 0;
        uint64_t a = reg_read(regs, decoded->rn);
        uint64_t b = emu_dp_shift_hw(reg_read(regs, decoded->rm), operands->shift_type, operands->shift_amount, sf);
        uint64_t nzcv = 0;
        uint64_t result = emu_addsub_hw(a, b, decoded->operation == ARM64_OPERATION_SUB, setflags, sf, &nzcv);

        if (setflags) emu_write_nzcv(regs, nzcv);
        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_ADD_SUB_EXTENDED)
    {
        bool setflags = (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) != 0;
        uint64_t a = addr_reg_read(regs, decoded->rn);
        uint64_t b = emu_extend_reg(reg_read(regs, decoded->rm), operands->option, operands->shift_amount);
        uint64_t nzcv = 0;
        uint64_t result = emu_addsub_hw(a, b, decoded->operation == ARM64_OPERATION_SUB, setflags, sf, &nzcv);

        if (setflags)
        {
            emu_write_nzcv(regs, nzcv);
            reg_write(regs, decoded->rd, result, sf);
        }
        else
        {
            addr_reg_write(regs, decoded->rd, result);
        }
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_LOGICAL_SHIFTED)
    {
        uint32_t opc = decoded->operation == ARM64_OPERATION_AND ? 0 : decoded->operation == ARM64_OPERATION_ORR ? 1 : decoded->operation == ARM64_OPERATION_EOR ? 2 : 3;
        uint64_t a = reg_read(regs, decoded->rn);
        uint64_t b = emu_dp_shift_hw(reg_read(regs, decoded->rm), operands->shift_type, operands->shift_amount, sf);
        uint64_t nzcv = 0;
        uint64_t result = emu_logic_hw(a, b, opc, (decoded->flags & ARM64_INSN_FLAG_INVERT) != 0, sf, &nzcv);

        if (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) emu_write_nzcv(regs, nzcv);
        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_CONDITIONAL_SELECT)
    {
        uint64_t a = reg_read(regs, decoded->rn);
        uint64_t b = reg_read(regs, decoded->rm);
        uint64_t result;
        if (!emu_cond_select_hw(decoded->operation, a, b, regs->pstate, operands->condition, sf, &result)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_DATA_PROCESSING_2_SOURCE)
    {
        uint64_t a = reg_read(regs, decoded->rn) & emu_dp_mask(sf);
        uint64_t b = reg_read(regs, decoded->rm) & emu_dp_mask(sf);
        uint64_t result;

        switch (decoded->operation)
        {
        case ARM64_OPERATION_UDIV:
            result = sf ? EMU_INT_BIN64("udiv", a, b) : EMU_INT_BIN32("udiv", a, b);
            break;
        case ARM64_OPERATION_SDIV:
            result = sf ? EMU_INT_BIN64("sdiv", a, b) : EMU_INT_BIN32("sdiv", a, b);
            break;
        case ARM64_OPERATION_LSLV:
            result = sf ? EMU_INT_BIN64("lslv", a, b) : EMU_INT_BIN32("lslv", a, b);
            break;
        case ARM64_OPERATION_LSRV:
            result = sf ? EMU_INT_BIN64("lsrv", a, b) : EMU_INT_BIN32("lsrv", a, b);
            break;
        case ARM64_OPERATION_ASRV:
            result = sf ? EMU_INT_BIN64("asrv", a, b) : EMU_INT_BIN32("asrv", a, b);
            break;
        case ARM64_OPERATION_RORV:
            result = sf ? EMU_INT_BIN64("rorv", a, b) : EMU_INT_BIN32("rorv", a, b);
            break;
        case ARM64_OPERATION_CRC32B:
            result = emu_dp_crc32b_hw((uint32_t)a, (uint32_t)b);
            break;
        case ARM64_OPERATION_CRC32H:
            result = emu_dp_crc32h_hw((uint32_t)a, (uint32_t)b);
            break;
        case ARM64_OPERATION_CRC32W:
            result = emu_dp_crc32w_hw((uint32_t)a, (uint32_t)b);
            break;
        case ARM64_OPERATION_CRC32X:
            result = emu_dp_crc32x_hw((uint32_t)a, b);
            break;
        case ARM64_OPERATION_CRC32CB:
            result = emu_dp_crc32cb_hw((uint32_t)a, (uint32_t)b);
            break;
        case ARM64_OPERATION_CRC32CH:
            result = emu_dp_crc32ch_hw((uint32_t)a, (uint32_t)b);
            break;
        case ARM64_OPERATION_CRC32CW:
            result = emu_dp_crc32cw_hw((uint32_t)a, (uint32_t)b);
            break;
        case ARM64_OPERATION_CRC32CX:
            result = emu_dp_crc32cx_hw((uint32_t)a, b);
            break;
        case ARM64_OPERATION_SMAX:
            result = emu_minmax_hw(a, b, false, false, sf);
            break;
        case ARM64_OPERATION_UMAX:
            result = emu_minmax_hw(a, b, false, true, sf);
            break;
        case ARM64_OPERATION_SMIN:
            result = emu_minmax_hw(a, b, true, false, sf);
            break;
        case ARM64_OPERATION_UMIN:
            result = emu_minmax_hw(a, b, true, true, sf);
            break;
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_MULTIPLY_ADD || decoded->opcode == ARM64_OP_MULTIPLY_HIGH)
    {
        uint64_t result;

        switch (decoded->operation)
        {
        case ARM64_OPERATION_MADD:
        case ARM64_OPERATION_MSUB:
        {
            uint64_t n = reg_read(regs, decoded->rn) & emu_dp_mask(sf);
            uint64_t m = reg_read(regs, decoded->rm) & emu_dp_mask(sf);
            uint64_t a = reg_read(regs, decoded->ra) & emu_dp_mask(sf);

            if (sf)
            {
                if (decoded->operation == ARM64_OPERATION_MSUB) asm volatile("msub %0, %1, %2, %3\n" : "=r"(result) : "r"(n), "r"(m), "r"(a));
                else asm volatile("madd %0, %1, %2, %3\n" : "=r"(result) : "r"(n), "r"(m), "r"(a));
            }
            else
            {
                uint32_t result32;

                if (decoded->operation == ARM64_OPERATION_MSUB) asm volatile("msub %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)n), "r"((uint32_t)m), "r"((uint32_t)a));
                else asm volatile("madd %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)n), "r"((uint32_t)m), "r"((uint32_t)a));
                result = result32;
            }
            break;
        }
        case ARM64_OPERATION_SMADDL:
        case ARM64_OPERATION_SMSUBL:
        {
            uint64_t a = reg_read(regs, decoded->ra);

            if (decoded->operation == ARM64_OPERATION_SMSUBL) asm volatile("smsubl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, decoded->rn)), "r"((uint32_t)reg_read(regs, decoded->rm)), "r"(a));
            else asm volatile("smaddl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, decoded->rn)), "r"((uint32_t)reg_read(regs, decoded->rm)), "r"(a));
            break;
        }
        case ARM64_OPERATION_SMULH:
            asm volatile("smulh %0, %1, %2\n" : "=r"(result) : "r"(reg_read(regs, decoded->rn)), "r"(reg_read(regs, decoded->rm)));
            break;
        case ARM64_OPERATION_UMADDL:
        case ARM64_OPERATION_UMSUBL:
        {
            uint64_t a = reg_read(regs, decoded->ra);

            if (decoded->operation == ARM64_OPERATION_UMSUBL) asm volatile("umsubl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, decoded->rn)), "r"((uint32_t)reg_read(regs, decoded->rm)), "r"(a));
            else asm volatile("umaddl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, decoded->rn)), "r"((uint32_t)reg_read(regs, decoded->rm)), "r"(a));
            break;
        }
        case ARM64_OPERATION_UMULH:
            asm volatile("umulh %0, %1, %2\n" : "=r"(result) : "r"(reg_read(regs, decoded->rn)), "r"(reg_read(regs, decoded->rm)));
            break;
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_ADD_SUB_CARRY)
    {
        bool op_sub = (decoded->flags & ARM64_INSN_FLAG_SUBTRACT) != 0;
        uint64_t x = reg_read(regs, decoded->rn);
        uint64_t y = reg_read(regs, decoded->rm);
        uint64_t input_nzcv = regs->pstate & (0xFULL << 28);
        uint64_t result, nzcv;

        if (sf)
        {
            if (op_sub)
                asm volatile("msr nzcv, %2\n"
                             "sbcs %0, %3, %4\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result), "=r"(nzcv)
                             : "r"(input_nzcv), "r"(x), "r"(y)
                             : "cc");
            else
                asm volatile("msr nzcv, %2\n"
                             "adcs %0, %3, %4\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result), "=r"(nzcv)
                             : "r"(input_nzcv), "r"(x), "r"(y)
                             : "cc");
        }
        else
        {
            uint32_t result32;

            if (op_sub)
                asm volatile("msr nzcv, %2\n"
                             "sbcs %w0, %w3, %w4\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result32), "=r"(nzcv)
                             : "r"(input_nzcv), "r"((uint32_t)x), "r"((uint32_t)y)
                             : "cc");
            else
                asm volatile("msr nzcv, %2\n"
                             "adcs %w0, %w3, %w4\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result32), "=r"(nzcv)
                             : "r"(input_nzcv), "r"((uint32_t)x), "r"((uint32_t)y)
                             : "cc");
            result = result32;
        }

        if (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) emu_write_nzcv(regs, nzcv);

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_CONDITIONAL_COMPARE)
    {
        bool op_sub = decoded->operation == ARM64_OPERATION_CCMP;
        uint64_t a = reg_read(regs, decoded->rn);
        uint64_t b = decoded->flags & ARM64_INSN_FLAG_IMMEDIATE ? operands->immediate : reg_read(regs, decoded->rm);
        uint64_t flags;
        bool take = emu_cond_holds_hw(regs->pstate, operands->condition);

        if (take) emu_addsub_hw(a, b, op_sub, true, sf, &flags);
        else flags = (uint64_t)operands->nzcv << 28;
        emu_write_nzcv(regs, flags);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_DATA_PROCESSING_1_SOURCE)
    {
        uint64_t src = reg_read(regs, decoded->rn);
        uint64_t result;

        switch (decoded->operation)
        {
        case ARM64_OPERATION_RBIT:
            result = emu_dp_rbit_hw(src, sf);
            break;
        case ARM64_OPERATION_REV16:
            result = emu_dp_rev16_hw(src, sf);
            break;
        case ARM64_OPERATION_REV32:
            result = emu_dp_rev32_hw(src, sf);
            break;
        case ARM64_OPERATION_REV64:
            result = emu_dp_rev64_hw(src);
            break;
        case ARM64_OPERATION_CLZ:
            result = emu_dp_clz_hw(src, sf);
            break;
        case ARM64_OPERATION_CLS:
            result = emu_dp_cls_hw(src, sf);
            break;
        case ARM64_OPERATION_CTZ:
            result = emu_dp_ctz_hw(src, sf);
            break;
        case ARM64_OPERATION_CNT:
            result = emu_dp_count_bits_hw(src, sf);
            break;
        case ARM64_OPERATION_ABS:
            if (sf)
                asm volatile("cmp %1, #0\n"
                             "cneg %0, %1, mi\n"
                             : "=r"(result)
                             : "r"(src)
                             : "cc");
            else
            {
                uint32_t result32;

                asm volatile("cmp %w1, #0\n"
                             "cneg %w0, %w1, mi\n"
                             : "=r"(result32)
                             : "r"((uint32_t)src)
                             : "cc");
                result = result32;
            }
            break;
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }

    return EMU_INSN_SKIP;
}

/* ======================== 总入口：解码与架构大类分派 ======================== */

//访存类指令使用模板汇编让硬件真实同语义需要注意一个问题：
//COW:当前进程准备写入一个仍与其他进程或映射共享的物理页，而该虚拟内存区域在逻辑上属于私有可写。Linux 为避免提前复制页面，先让这些映射共享同一物理页，并将相关 PTE 设置为只读。首次写入触发权限异常后，内核为当前进程建立私有副本，将其 PTE 改为可写，然后重新执行写入指令。
//这里执行访存类指令写的时候目标地址页如果刚好处于COW中就会之间panic
static inline bool emulate_insn(struct pt_regs *regs, struct fp_regs *fp_regs, const uint32_t *specified_insn)
{
    uint32_t insn;
    uint64_t pc;
    struct arm64_decoded_insn decoded;
    __uint128_t saved_fp_regs[2];
    uint32_t saved_fp_reg_indices[2];
    uint32_t saved_fp_reg_count;
    enum emu_insn_result result;
    bool handled;

    asm volatile(".inst 0xd500409f" ::: "memory");
    result = EMU_INSN_SKIP;
    saved_fp_reg_count = 0;
    pc = regs->pc;

    if (specified_insn) insn = *specified_insn;
    else asm volatile("ldr %w0, [%1]" : "=&r"(insn) : "r"(pc) : "memory");

    arm64_decode_insn(insn, &decoded);
    if (decoded.status == ARM64_DECODE_OK)
    {
        switch (decoded.insn_class)
        {
        case ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM:
            switch (decoded.opcode)
            {
            case ARM64_OP_NOP:
            case ARM64_OP_HINT:
            case ARM64_OP_BARRIER:
            case ARM64_OP_EXCEPTION_GENERATION:
            case ARM64_OP_EXCEPTION_RETURN:
            case ARM64_OP_MRS:
            case ARM64_OP_MSR_REGISTER:
                result = emu_simulate_system_insn(regs, &decoded, pc);
                break;
            case ARM64_OP_B:
            case ARM64_OP_BL:
            case ARM64_OP_BR:
            case ARM64_OP_BLR:
            case ARM64_OP_RET:
            case ARM64_OP_B_COND:
            case ARM64_OP_CBZ:
            case ARM64_OP_CBNZ:
            case ARM64_OP_TBZ:
            case ARM64_OP_TBNZ:
                result = emu_simulate_branch_insn(regs, &decoded, pc);
                break;
            default:
                break;
            }
            break;
        case ARM64_INSN_CLASS_LOAD_STORE:
            if ((decoded.flags & (ARM64_INSN_FLAG_FP | ARM64_INSN_FLAG_LOAD)) == (ARM64_INSN_FLAG_FP | ARM64_INSN_FLAG_LOAD))
            {
                saved_fp_reg_indices[saved_fp_reg_count] = decoded.rt;
                saved_fp_regs[saved_fp_reg_count++] = fp_regs->q[decoded.rt];
                if (decoded.opcode == ARM64_OP_LOAD_STORE_PAIR)
                {
                    saved_fp_reg_indices[saved_fp_reg_count] = decoded.rt2;
                    saved_fp_regs[saved_fp_reg_count++] = fp_regs->q[decoded.rt2];
                }
            }
            result = emu_simulate_load_store_insn(regs, fp_regs, &decoded, pc);
            break;
        case ARM64_INSN_CLASS_DATA_PROCESSING_SIMD_FP:
            saved_fp_reg_indices[0] = decoded.rd;
            saved_fp_regs[0] = fp_regs->q[decoded.rd];
            saved_fp_reg_count = 1;
            result = emu_simulate_fp_simd_insn(regs, fp_regs, &decoded, pc);
            break;
        case ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE:
        case ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER:
            result = emu_simulate_data_processing_insn(regs, &decoded);
            break;
        default:
            break;
        }
    }

    if (result == EMU_INSN_NOP) regs->pc = pc + 4;
    handled = result == EMU_INSN_HANDLED || result == EMU_INSN_NOP;
    if (!handled)
    {
        while (saved_fp_reg_count)
        {
            saved_fp_reg_count--;
            fp_regs->q[saved_fp_reg_indices[saved_fp_reg_count]] = saved_fp_regs[saved_fp_reg_count];
        }
    }
    if (!handled) ls_log_always_tag("emulate_insn", "failed pc=0x%llx insn=0x%08x bytes=%02x %02x %02x %02x\n", (unsigned long long)pc, insn, insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);

    asm volatile(".inst 0xd500419f" ::: "memory");
    return handled;
}

#endif // EMULATE_INSN_H
