#ifndef EMULATE_INSN_H
#define EMULATE_INSN_H

#include <asm/ptrace.h>
#include <asm/sysreg.h>
#include <asm/uaccess.h>
#include <linux/bits.h>
#include "arm64_reg.h"
#include "arm64_decode/arm64_decode.h"

enum emu_insn_result
{
    EMU_INSN_HANDLED, // 模拟函数已完成指令语义，并自行更新现场和 PC
    EMU_INSN_SKIP,    // 不支持或无法执行，不能推进 PC
};

/* =========================================================================
  ARM64 指令执行器


  作用：
  - 在断点命中后执行当前用户态指令语义，将结果同步至 pt_regs 和fp_regs软件现场并推进 PC。软件现场由外部异常处理统一写回cpu
    - 调用者进入时必须保持特权用户访问关闭，并保证执行期间不迁移 CPU；执行器临时打开用户访问，退出前恢复为关闭。
  - 调用者提供完整 GPR、PSTATE、Q0-Q31、FPCR 和 FPSR 软件现场；执行器的架构结果只写入传入现场。
  - 当前 CPU 寄存器只作为固定硬件模板的临时执行载体；FP/SIMD 模板回收结果后恢复原有 FPCR/FPSR。
  - 软件现场不存在的系统状态才直接访问硬件，最终用户寄存器提交由外部异常处理函数统一完成。

  已支持指令：
  - 系统：NOP、YIELD、CLREX、DSB、DMB、ISB，以及仅支持 NZCV、FPCR、FPSR、TPIDR_EL0、TPIDRRO_EL0 和 CNTVCT_EL0 的有限 MRS/MSR 系统寄存器访问。
  - 系统寄存器：NZCV、FPCR、FPSR、TPIDR_EL0、TPIDRRO_EL0、CNTVCT_EL0。
  - 分支：B、BL、BR、BLR、RET、B.cond、CBZ/CBNZ、TBZ/TBNZ。
  - 访存：普通、literal、pair、non-temporal、unprivileged、prefetch、RCpc、LDAPR、ordered、exclusive、LSE RMW、CAS 和 CASP。
  - FP/SIMD：标量 FP 运算、比较、选择、转换和 GPR 传送，以及 AdvSIMD 的复制、移位、排列、逻辑、算术、逐元素、归约、窄化和提取。
  - 数据处理：ADR/ADRP、加减、逻辑、位域、提取、宽立即数、条件选择/比较、单源/双源、乘加和高位乘法。

  未支持指令：
  - 异常生成与异常返回指令。
  - YIELD 以外的 HINT，以及白名单之外的系统寄存器访问。
  - SVE、SME，以及 decoder 未识别或执行器尚无硬件模板的编码。
  ========================================================================= */

/* ======================== 跨大类通用现场辅助 ======================== */

// 读取数据运算使用的通用寄存器；寄存器 31 按 XZR/WZR 语义返回 0。
static inline uint64_t reg_read(struct pt_regs *regs, uint32_t n)
{
    return (n == 31) ? 0ULL : regs->regs[n];
}

// 写入数据运算使用的通用寄存器；寄存器 31 丢弃写入，32 位结果按架构语义零扩展。
static inline void reg_write(struct pt_regs *regs, uint32_t n, uint64_t val, bool sf)
{
    if (n != 31) regs->regs[n] = sf ? val : (uint64_t)(uint32_t)val;
}

// 读取地址计算使用的基址寄存器；寄存器 31 按 读SP 语义处理。
static inline uint64_t addr_reg_read(struct pt_regs *regs, uint32_t n)
{
    return (n == 31) ? regs->sp : regs->regs[n];
}

// 写入地址计算使用的基址寄存器；寄存器 31 按写 SP处理
static inline void addr_reg_write(struct pt_regs *regs, uint32_t n, uint64_t val)
{
    if (n == 31) regs->sp = val;
    else regs->regs[n] = val;
}

// 读取异常现场中的 N/Z/C/V 条件标志，不包含 PSTATE 的其他位。
static inline uint64_t emu_read_nzcv(const struct pt_regs *regs)
{
    return regs->pstate & GENMASK_ULL(31, 28);
}

// 更新异常现场中的 N/Z/C/V 条件标志，同时保留 PSTATE 的其他位。
static inline void emu_write_nzcv(struct pt_regs *regs, uint64_t nzcv)
{
    regs->pstate = (regs->pstate & ~GENMASK_ULL(31, 28)) | (nzcv & GENMASK_ULL(31, 28));
}

// 直接根据异常现场的 NZCV 计算 A64 条件码是否成立
static inline bool emu_cond_holds(uint64_t nzcv, uint32_t cond)
{
    switch (cond >> 1)
    {
    case 0:
        return ((nzcv & PSR_Z_BIT) != 0) ^ ((cond & 1U) != 0);
    case 1:
        return ((nzcv & PSR_C_BIT) != 0) ^ ((cond & 1U) != 0);
    case 2:
        return ((nzcv & PSR_N_BIT) != 0) ^ ((cond & 1U) != 0);
    case 3:
        return ((nzcv & PSR_V_BIT) != 0) ^ ((cond & 1U) != 0);
    case 4:
        return (((nzcv & PSR_C_BIT) != 0) && ((nzcv & PSR_Z_BIT) == 0)) ^ ((cond & 1U) != 0);
    case 5:
        return (((nzcv & PSR_N_BIT) != 0) == ((nzcv & PSR_V_BIT) != 0)) ^ ((cond & 1U) != 0);
    case 6:
        return (((nzcv & PSR_Z_BIT) == 0) && (((nzcv & PSR_N_BIT) != 0) == ((nzcv & PSR_V_BIT) != 0))) ^ ((cond & 1U) != 0);
    default:
        return true;
    }
}

/* ======================== 分支与系统类：屏障模板及完整执行流程 ======================== */

// instruction 唯一确定屏障指令，option 再选择对应的 4 位立即数变体。
// .inst 编码必须在编译期确定，因此每个 instruction/option 组合都映射到固定指令。
// clang-format off
static inline bool emu_barrier_hw(enum arm64_instruction instruction, uint32_t option)
{
    switch (instruction)
    {
    case ARM64_INSN_CLREX:
        switch (option)
        {
        case 0:  asm volatile(".inst 0xD503305F + (0 << 8)" ::: "memory"); break;
        case 1:  asm volatile(".inst 0xD503305F + (1 << 8)" ::: "memory"); break;
        case 2:  asm volatile(".inst 0xD503305F + (2 << 8)" ::: "memory"); break;
        case 3:  asm volatile(".inst 0xD503305F + (3 << 8)" ::: "memory"); break;
        case 4:  asm volatile(".inst 0xD503305F + (4 << 8)" ::: "memory"); break;
        case 5:  asm volatile(".inst 0xD503305F + (5 << 8)" ::: "memory"); break;
        case 6:  asm volatile(".inst 0xD503305F + (6 << 8)" ::: "memory"); break;
        case 7:  asm volatile(".inst 0xD503305F + (7 << 8)" ::: "memory"); break;
        case 8:  asm volatile(".inst 0xD503305F + (8 << 8)" ::: "memory"); break;
        case 9:  asm volatile(".inst 0xD503305F + (9 << 8)" ::: "memory"); break;
        case 10: asm volatile(".inst 0xD503305F + (10 << 8)" ::: "memory"); break;
        case 11: asm volatile(".inst 0xD503305F + (11 << 8)" ::: "memory"); break;
        case 12: asm volatile(".inst 0xD503305F + (12 << 8)" ::: "memory"); break;
        case 13: asm volatile(".inst 0xD503305F + (13 << 8)" ::: "memory"); break;
        case 14: asm volatile(".inst 0xD503305F + (14 << 8)" ::: "memory"); break;
        case 15: asm volatile(".inst 0xD503305F + (15 << 8)" ::: "memory"); break;
        }
        return true;
    case ARM64_INSN_DSB:
        switch (option)
        {
        case 0:  asm volatile(".inst 0xD503309F + (0 << 8)" ::: "memory"); break;
        case 1:  asm volatile(".inst 0xD503309F + (1 << 8)" ::: "memory"); break;
        case 2:  asm volatile(".inst 0xD503309F + (2 << 8)" ::: "memory"); break;
        case 3:  asm volatile(".inst 0xD503309F + (3 << 8)" ::: "memory"); break;
        case 4:  asm volatile(".inst 0xD503309F + (4 << 8)" ::: "memory"); break;
        case 5:  asm volatile(".inst 0xD503309F + (5 << 8)" ::: "memory"); break;
        case 6:  asm volatile(".inst 0xD503309F + (6 << 8)" ::: "memory"); break;
        case 7:  asm volatile(".inst 0xD503309F + (7 << 8)" ::: "memory"); break;
        case 8:  asm volatile(".inst 0xD503309F + (8 << 8)" ::: "memory"); break;
        case 9:  asm volatile(".inst 0xD503309F + (9 << 8)" ::: "memory"); break;
        case 10: asm volatile(".inst 0xD503309F + (10 << 8)" ::: "memory"); break;
        case 11: asm volatile(".inst 0xD503309F + (11 << 8)" ::: "memory"); break;
        case 12: asm volatile(".inst 0xD503309F + (12 << 8)" ::: "memory"); break;
        case 13: asm volatile(".inst 0xD503309F + (13 << 8)" ::: "memory"); break;
        case 14: asm volatile(".inst 0xD503309F + (14 << 8)" ::: "memory"); break;
        case 15: asm volatile(".inst 0xD503309F + (15 << 8)" ::: "memory"); break;
        }
        return true;
    case ARM64_INSN_DMB:
        switch (option)
        {
        case 0:  asm volatile(".inst 0xD50330BF + (0 << 8)" ::: "memory"); break;
        case 1:  asm volatile(".inst 0xD50330BF + (1 << 8)" ::: "memory"); break;
        case 2:  asm volatile(".inst 0xD50330BF + (2 << 8)" ::: "memory"); break;
        case 3:  asm volatile(".inst 0xD50330BF + (3 << 8)" ::: "memory"); break;
        case 4:  asm volatile(".inst 0xD50330BF + (4 << 8)" ::: "memory"); break;
        case 5:  asm volatile(".inst 0xD50330BF + (5 << 8)" ::: "memory"); break;
        case 6:  asm volatile(".inst 0xD50330BF + (6 << 8)" ::: "memory"); break;
        case 7:  asm volatile(".inst 0xD50330BF + (7 << 8)" ::: "memory"); break;
        case 8:  asm volatile(".inst 0xD50330BF + (8 << 8)" ::: "memory"); break;
        case 9:  asm volatile(".inst 0xD50330BF + (9 << 8)" ::: "memory"); break;
        case 10: asm volatile(".inst 0xD50330BF + (10 << 8)" ::: "memory"); break;
        case 11: asm volatile(".inst 0xD50330BF + (11 << 8)" ::: "memory"); break;
        case 12: asm volatile(".inst 0xD50330BF + (12 << 8)" ::: "memory"); break;
        case 13: asm volatile(".inst 0xD50330BF + (13 << 8)" ::: "memory"); break;
        case 14: asm volatile(".inst 0xD50330BF + (14 << 8)" ::: "memory"); break;
        case 15: asm volatile(".inst 0xD50330BF + (15 << 8)" ::: "memory"); break;
        }
        return true;
    case ARM64_INSN_ISB:
        asm volatile("isb" ::: "memory");
        return true;
    default:
        return false;
    }
}
// clang-format on

// 模拟分支、HINT、屏障和系统寄存器访问指令。
static inline enum emu_insn_result emu_simulate_branch_system_insn(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_decoded_insn *decoded)
{
    uint64_t pc = regs->pc;

    switch (decoded->instruction)
    {
    case ARM64_INSN_B:
        regs->pc = pc + decoded->offset;
        return EMU_INSN_HANDLED;
    case ARM64_INSN_CBZ:
    {
        uint64_t val = decoded->operand_width == 64 ? reg_read(regs, decoded->rt) : (uint32_t)reg_read(regs, decoded->rt);

        regs->pc = val == 0 ? pc + decoded->offset : pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_CBNZ:
    {
        uint64_t val = decoded->operand_width == 64 ? reg_read(regs, decoded->rt) : (uint32_t)reg_read(regs, decoded->rt);

        regs->pc = val != 0 ? pc + decoded->offset : pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_TBZ:
    {
        bool bit_set = ((reg_read(regs, decoded->rt) >> decoded->test_bit) & 1) != 0;

        regs->pc = !bit_set ? pc + decoded->offset : pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_TBNZ:
    {
        bool bit_set = ((reg_read(regs, decoded->rt) >> decoded->test_bit) & 1) != 0;

        regs->pc = bit_set ? pc + decoded->offset : pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_B_COND:
        regs->pc = emu_cond_holds(emu_read_nzcv(regs), decoded->condition) ? pc + decoded->offset : pc + 4;
        return EMU_INSN_HANDLED;
    case ARM64_INSN_BL:
        regs->regs[30] = pc + 4;
        regs->pc = pc + decoded->offset;
        return EMU_INSN_HANDLED;
    case ARM64_INSN_NOP:
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    case ARM64_INSN_YIELD:
        asm volatile("yield");
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    case ARM64_INSN_CLREX:
    case ARM64_INSN_DSB:
    case ARM64_INSN_DMB:
    case ARM64_INSN_ISB:
        if (!emu_barrier_hw(decoded->instruction, decoded->option)) return EMU_INSN_SKIP;
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    case ARM64_INSN_MSR_REGISTER:
    {
        uint64_t val = reg_read(regs, decoded->rt);

        switch (decoded->sysreg)
        {
        case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
            emu_write_nzcv(regs, val);
            break;
        case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
            fp_regs->fpcr = (uint32_t)val;
            break;
        case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
            fp_regs->fpsr = (uint32_t)val;
            break;
        case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
            arm64_write_tpidr_el0(val);
            break;
        default:
            return EMU_INSN_SKIP;
        }

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_MRS:
    {
        uint64_t val;

        switch (decoded->sysreg)
        {
        case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
            val = emu_read_nzcv(regs);
            break;
        case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
            val = fp_regs->fpcr;
            break;
        case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
            val = fp_regs->fpsr;
            break;
        case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
            val = arm64_read_tpidr_el0();
            break;
        case ARM64_SYSREG_KEY(3, 3, 13, 0, 3):
            val = arm64_read_tpidrro_el0();
            break;
        case ARM64_SYSREG_KEY(3, 3, 14, 0, 2):
            val = arm64_read_cntvct_el0();
            break;
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, decoded->rt, val, true);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_BR:
        regs->pc = reg_read(regs, decoded->rn);
        return EMU_INSN_HANDLED;
    case ARM64_INSN_BLR:
        regs->pc = reg_read(regs, decoded->rn);
        regs->regs[30] = pc + 4;
        return EMU_INSN_HANDLED;
    case ARM64_INSN_RET:
        regs->pc = reg_read(regs, decoded->rn);
        return EMU_INSN_HANDLED;
    default:
        return EMU_INSN_SKIP;
    }
}

/* ======================== 访存类：寻址与写回辅助 ======================== */

/* decoder 已给出寻址形式；这里只结合当前寄存器现场求出有效地址。 */
static inline bool emu_resolve_memory_address(const struct arm64_decoded_insn *operands, struct pt_regs *regs, uint64_t pc, uint64_t base, uint64_t *address)
{
    uint64_t index;

    switch (operands->memory_address_mode)
    {
    case ARM64_MEMORY_ADDRESS_LITERAL:
        *address = pc + operands->offset;
        return true;
    case ARM64_MEMORY_ADDRESS_BASE_OFFSET:
    case ARM64_MEMORY_ADDRESS_PRE_INDEX:
        *address = base + operands->offset;
        return true;
    case ARM64_MEMORY_ADDRESS_POST_INDEX:
        *address = base;
        return true;
    case ARM64_MEMORY_ADDRESS_REGISTER_OFFSET:
        index = reg_read(regs, operands->rm);
        switch (operands->extend_type)
        {
        case 2:
            index = (uint32_t)index;
            break;
        case 3:
            break;
        case 6:
            index = (uint64_t)(int64_t)(int32_t)index;
            break;
        case 7:
            break;
        default:
            return false;
        }
        *address = base + (index << operands->shift_amount);
        return true;
    default:
        return false;
    }
}

static inline void emu_commit_memory_writeback(const struct arm64_decoded_insn *operands, struct pt_regs *regs, uint64_t base)
{
    if (operands->memory_address_mode != ARM64_MEMORY_ADDRESS_POST_INDEX && operands->memory_address_mode != ARM64_MEMORY_ADDRESS_PRE_INDEX) return;
    addr_reg_write(regs, operands->rn, base + operands->offset);
}

/* ======================== 访存类：固定硬件模板 ======================== */

// clang-format off
static inline bool emu_hw_load_gpr(enum arm64_instruction instruction, uint64_t addr, int bytes, bool sf, uint64_t *out)
{
    uint64_t value;

    switch (instruction)
    {
    case ARM64_INSN_LDR_LITERAL_GPR:
        switch (bytes)
        {
        case 4: asm volatile("ldr %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 8: asm volatile("ldr %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDRSW_LITERAL:
        switch (bytes)
        {
        case 4: asm volatile("ldrsw %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUR_GPR:
        switch (bytes)
        {
        case 1: asm volatile("ldurb %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 2: asm volatile("ldurh %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 4: asm volatile("ldur %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 8: asm volatile("ldur %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUR_SIGNED_GPR:
        switch (bytes)
        {
        case 1:
            switch ((uint8_t)sf)
            {
            case false: asm volatile("ldursb %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            case true:  asm volatile("ldursb %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            }
            break;
        case 2:
            switch ((uint8_t)sf)
            {
            case false: asm volatile("ldursh %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            case true:  asm volatile("ldursh %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            }
            break;
        case 4:
            switch ((uint8_t)sf)
            {
            case false: return false;
            case true:  asm volatile("ldursw %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            }
            break;
        default:
            return false;
        }
        break;
    case ARM64_INSN_LDTR_GPR:
        switch (bytes)
        {
        case 1: asm volatile("ldtrb %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 2: asm volatile("ldtrh %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 4: asm volatile("ldtr %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 8: asm volatile("ldtr %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDTR_SIGNED_GPR:
        switch (bytes)
        {
        case 1:
            switch ((uint8_t)sf)
            {
            case false: asm volatile("ldtrsb %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            case true:  asm volatile("ldtrsb %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            }
            break;
        case 2:
            switch ((uint8_t)sf)
            {
            case false: asm volatile("ldtrsh %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            case true:  asm volatile("ldtrsh %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            }
            break;
        case 4:
            switch ((uint8_t)sf)
            {
            case false: return false;
            case true:  asm volatile("ldtrsw %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            }
            break;
        default:
            return false;
        }
        break;
    case ARM64_INSN_LDR_GPR_POST_INDEX:
    case ARM64_INSN_LDR_GPR_PRE_INDEX:
    case ARM64_INSN_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1: asm volatile("ldrb %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 2: asm volatile("ldrh %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 4: asm volatile("ldr %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        case 8: asm volatile("ldr %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX:
    case ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX:
    case ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET:
    case ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1:
            switch ((uint8_t)sf)
            {
            case false: asm volatile("ldrsb %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            case true:  asm volatile("ldrsb %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            }
            break;
        case 2:
            switch ((uint8_t)sf)
            {
            case false: asm volatile("ldrsh %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            case true:  asm volatile("ldrsh %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            }
            break;
        case 4:
            switch ((uint8_t)sf)
            {
            case false: return false;
            case true:  asm volatile("ldrsw %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
            }
            break;
        default:
            return false;
        }
        break;
    default:
        return false;
    }

    *out = value;
    return true;
}

static inline bool emu_hw_store_gpr(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t value)
{
    switch (instruction)
    {
    case ARM64_INSN_STUR_GPR:
        switch (bytes)
        {
        case 1: asm volatile("sturb %w0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        case 2: asm volatile("sturh %w0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        case 4: asm volatile("stur %w0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        case 8: asm volatile("stur %0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STTR_GPR:
        switch (bytes)
        {
        case 1: asm volatile("sttrb %w0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        case 2: asm volatile("sttrh %w0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        case 4: asm volatile("sttr %w0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        case 8: asm volatile("sttr %0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STR_GPR_POST_INDEX:
    case ARM64_INSN_STR_GPR_PRE_INDEX:
    case ARM64_INSN_STR_GPR_REGISTER_OFFSET:
    case ARM64_INSN_STR_GPR_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1: asm volatile("strb %w0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        case 2: asm volatile("strh %w0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        case 4: asm volatile("str %w0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        case 8: asm volatile("str %0, [%1]" : : "r"(value), "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_load_fp(enum arm64_instruction instruction, uint64_t addr, int bytes, __uint128_t *out)
{
    switch (instruction)
    {
    case ARM64_INSN_LDUR_FP_SIMD:
        switch (bytes)
        {
        case 1:  asm volatile("ldur b0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        case 2:  asm volatile("ldur h0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        case 4:  asm volatile("ldur s0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        case 8:  asm volatile("ldur d0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        case 16: asm volatile("ldur q0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDR_LITERAL_FP_SIMD:
    case ARM64_INSN_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INSN_LDR_FP_SIMD_PRE_INDEX:
    case ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1:  asm volatile("ldr b0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        case 2:  asm volatile("ldr h0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        case 4:  asm volatile("ldr s0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        case 8:  asm volatile("ldr d0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        case 16: asm volatile("ldr q0, [%1]\nstr q0, [%0]" : : "r"(out), "r"(addr) : "memory", "v0"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_store_fp(enum arm64_instruction instruction, uint64_t addr, int bytes, const __uint128_t *value)
{
    switch (instruction)
    {
    case ARM64_INSN_STUR_FP_SIMD:
        switch (bytes)
        {
        case 1:  asm volatile("ldr q0, [%0]\nstur b0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        case 2:  asm volatile("ldr q0, [%0]\nstur h0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        case 4:  asm volatile("ldr q0, [%0]\nstur s0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        case 8:  asm volatile("ldr q0, [%0]\nstur d0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        case 16: asm volatile("ldr q0, [%0]\nstur q0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STR_FP_SIMD_POST_INDEX:
    case ARM64_INSN_STR_FP_SIMD_PRE_INDEX:
    case ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1:  asm volatile("ldr q0, [%0]\nstr b0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        case 2:  asm volatile("ldr q0, [%0]\nstr h0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        case 4:  asm volatile("ldr q0, [%0]\nstr s0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        case 8:  asm volatile("ldr q0, [%0]\nstr d0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        case 16: asm volatile("ldr q0, [%0]\nstr q0, [%1]" : : "r"(value), "r"(addr) : "memory", "v0"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_load_pair_gpr(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t *first, uint64_t *second)
{
    uint64_t value0, value1;

    switch (instruction)
    {
    case ARM64_INSN_LDNP_GPR:
        switch (bytes)
        {
        case 4: asm volatile("ldnp %w0, %w1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory"); break;
        case 8: asm volatile("ldnp %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDP_GPR_OFFSET:
    case ARM64_INSN_LDP_GPR_POST_INDEX:
    case ARM64_INSN_LDP_GPR_PRE_INDEX:
        switch (bytes)
        {
        case 4: asm volatile("ldp %w0, %w1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory"); break;
        case 8: asm volatile("ldp %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDPSW_OFFSET:
    case ARM64_INSN_LDPSW_POST_INDEX:
    case ARM64_INSN_LDPSW_PRE_INDEX:
        switch (bytes)
        {
        case 4: asm volatile("ldpsw %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }

    *first = value0;
    *second = value1;
    return true;
}

static inline bool emu_hw_store_pair_gpr(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t first, uint64_t second)
{
    switch (instruction)
    {
    case ARM64_INSN_STNP_GPR:
        switch (bytes)
        {
        case 4: asm volatile("stnp %w0, %w1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory"); break;
        case 8: asm volatile("stnp %0, %1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STP_GPR_OFFSET:
    case ARM64_INSN_STP_GPR_POST_INDEX:
    case ARM64_INSN_STP_GPR_PRE_INDEX:
        switch (bytes)
        {
        case 4: asm volatile("stp %w0, %w1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory"); break;
        case 8: asm volatile("stp %0, %1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_load_pair_fp(enum arm64_instruction instruction, uint64_t addr, int bytes, __uint128_t *first, __uint128_t *second)
{
    switch (instruction)
    {
    case ARM64_INSN_LDNP_FP_SIMD:
        switch (bytes)
        {
        case 4:  asm volatile("ldnp s0, s1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        case 8:  asm volatile("ldnp d0, d1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        case 16: asm volatile("ldnp q0, q1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INSN_LDP_FP_SIMD_OFFSET:
    case ARM64_INSN_LDP_FP_SIMD_PRE_INDEX:
        switch (bytes)
        {
        case 4:  asm volatile("ldp s0, s1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        case 8:  asm volatile("ldp d0, d1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        case 16: asm volatile("ldp q0, q1, [%2]\nstr q0, [%0]\nstr q1, [%1]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_store_pair_fp(enum arm64_instruction instruction, uint64_t addr, int bytes, const __uint128_t *first, const __uint128_t *second)
{
    switch (instruction)
    {
    case ARM64_INSN_STNP_FP_SIMD:
        switch (bytes)
        {
        case 4:  asm volatile("ldr q0, [%0]\nldr q1, [%1]\nstnp s0, s1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        case 8:  asm volatile("ldr q0, [%0]\nldr q1, [%1]\nstnp d0, d1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nstnp q0, q1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STP_FP_SIMD_POST_INDEX:
    case ARM64_INSN_STP_FP_SIMD_OFFSET:
    case ARM64_INSN_STP_FP_SIMD_PRE_INDEX:
        switch (bytes)
        {
        case 4:  asm volatile("ldr q0, [%0]\nldr q1, [%1]\nstp s0, s1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        case 8:  asm volatile("ldr q0, [%0]\nldr q1, [%1]\nstp d0, d1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nstp q0, q1, [%2]" : : "r"(first), "r"(second), "r"(addr) : "memory", "v0", "v1"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_load_rcpc(enum arm64_instruction instruction, uint64_t addr, int bytes, bool sf, uint64_t *out)
{
    uint64_t value;

    switch (instruction)
    {
    case ARM64_INSN_LDAPUR:
        switch (bytes)
        {
        case 1: asm volatile("mov x1, %1\n.inst 0x19400020\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        case 2: asm volatile("mov x1, %1\n.inst 0x59400020\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        case 4: asm volatile("mov x1, %1\n.inst 0x99400020\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        case 8: asm volatile("mov x1, %1\n.inst 0xD9400020\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDAPUR_SIGNED:
        switch (bytes)
        {
        case 1:
            switch ((uint8_t)sf)
            {
            case false: asm volatile("mov x1, %1\n.inst 0x19C00020\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
            case true: asm volatile("mov x1, %1\n.inst 0x19800020\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
            default: return false;
            }
            break;
        case 2:
            switch ((uint8_t)sf)
            {
            case false: asm volatile("mov x1, %1\n.inst 0x59C00020\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
            case true: asm volatile("mov x1, %1\n.inst 0x59800020\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
            default: return false;
            }
            break;
        case 4:
            switch ((uint8_t)sf)
            {
            case false: return false;
            case true: asm volatile("mov x1, %1\n.inst 0x99800020\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    default:
        return false;
    }

    *out = value;
    return true;
}

static inline bool emu_hw_store_rcpc(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t value)
{
    if (instruction != ARM64_INSN_STLUR) return false;

    switch (bytes)
    {
    case 1: asm volatile("mov x0, %0\nmov x1, %1\n.inst 0x19000020" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
    case 2: asm volatile("mov x0, %0\nmov x1, %1\n.inst 0x59000020" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
    case 4: asm volatile("mov x0, %0\nmov x1, %1\n.inst 0x99000020" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
    case 8: asm volatile("mov x0, %0\nmov x1, %1\n.inst 0xD9000020" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
    default: return false;
    }

    return true;
}

static inline bool emu_hw_load_ldapr(uint64_t addr, int bytes, uint64_t *out)
{
    uint64_t value;

    switch (bytes)
    {
    case 1: asm volatile("ldaprb %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
    case 2: asm volatile("ldaprh %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
    case 4: asm volatile("ldapr %w0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
    case 8: asm volatile("ldapr %0, [%1]" : "=&r"(value) : "r"(addr) : "memory"); break;
    default: return false;
    }

    *out = value;
    return true;
}

static inline bool emu_hw_atomic_rmw(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t src, uint64_t *old)
{
    uint64_t value;

    switch (instruction)
    {
    case ARM64_INSN_LDADD:
        switch (bytes)
        {
        case 1: asm volatile("ldaddb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldaddh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldadd %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldadd %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDADDA:
        switch (bytes)
        {
        case 1: asm volatile("ldaddab %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldaddah %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldadda %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldadda %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDADDL:
        switch (bytes)
        {
        case 1: asm volatile("ldaddlb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldaddlh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldaddl %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldaddl %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDADDAL:
        switch (bytes)
        {
        case 1: asm volatile("ldaddalb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldaddalh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldaddal %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldaddal %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDCLR:
        switch (bytes)
        {
        case 1: asm volatile("ldclrb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldclrh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldclr %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldclr %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDCLRA:
        switch (bytes)
        {
        case 1: asm volatile("ldclrab %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldclrah %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldclra %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldclra %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDCLRL:
        switch (bytes)
        {
        case 1: asm volatile("ldclrlb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldclrlh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldclrl %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldclrl %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDCLRAL:
        switch (bytes)
        {
        case 1: asm volatile("ldclralb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldclralh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldclral %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldclral %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDEOR:
        switch (bytes)
        {
        case 1: asm volatile("ldeorb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldeorh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldeor %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldeor %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDEORA:
        switch (bytes)
        {
        case 1: asm volatile("ldeorab %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldeorah %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldeora %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldeora %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDEORL:
        switch (bytes)
        {
        case 1: asm volatile("ldeorlb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldeorlh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldeorl %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldeorl %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDEORAL:
        switch (bytes)
        {
        case 1: asm volatile("ldeoralb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldeoralh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldeoral %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldeoral %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSET:
        switch (bytes)
        {
        case 1: asm volatile("ldsetb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldseth %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldset %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldset %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSETA:
        switch (bytes)
        {
        case 1: asm volatile("ldsetab %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsetah %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldseta %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldseta %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSETL:
        switch (bytes)
        {
        case 1: asm volatile("ldsetlb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsetlh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsetl %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsetl %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSETAL:
        switch (bytes)
        {
        case 1: asm volatile("ldsetalb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsetalh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsetal %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsetal %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMAX:
        switch (bytes)
        {
        case 1: asm volatile("ldsmaxb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsmaxh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsmax %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsmax %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMAXA:
        switch (bytes)
        {
        case 1: asm volatile("ldsmaxab %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsmaxah %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsmaxa %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsmaxa %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMAXL:
        switch (bytes)
        {
        case 1: asm volatile("ldsmaxlb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsmaxlh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsmaxl %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsmaxl %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMAXAL:
        switch (bytes)
        {
        case 1: asm volatile("ldsmaxalb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsmaxalh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsmaxal %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsmaxal %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMIN:
        switch (bytes)
        {
        case 1: asm volatile("ldsminb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsminh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsmin %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsmin %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMINA:
        switch (bytes)
        {
        case 1: asm volatile("ldsminab %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsminah %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsmina %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsmina %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMINL:
        switch (bytes)
        {
        case 1: asm volatile("ldsminlb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsminlh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsminl %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsminl %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMINAL:
        switch (bytes)
        {
        case 1: asm volatile("ldsminalb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldsminalh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldsminal %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldsminal %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMAX:
        switch (bytes)
        {
        case 1: asm volatile("ldumaxb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldumaxh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldumax %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldumax %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMAXA:
        switch (bytes)
        {
        case 1: asm volatile("ldumaxab %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldumaxah %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldumaxa %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldumaxa %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMAXL:
        switch (bytes)
        {
        case 1: asm volatile("ldumaxlb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldumaxlh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldumaxl %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldumaxl %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMAXAL:
        switch (bytes)
        {
        case 1: asm volatile("ldumaxalb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("ldumaxalh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldumaxal %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldumaxal %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMIN:
        switch (bytes)
        {
        case 1: asm volatile("lduminb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("lduminh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldumin %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldumin %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMINA:
        switch (bytes)
        {
        case 1: asm volatile("lduminab %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("lduminah %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("ldumina %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("ldumina %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMINL:
        switch (bytes)
        {
        case 1: asm volatile("lduminlb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("lduminlh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("lduminl %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("lduminl %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMINAL:
        switch (bytes)
        {
        case 1: asm volatile("lduminalb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("lduminalh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("lduminal %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("lduminal %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_SWP:
        switch (bytes)
        {
        case 1: asm volatile("swpb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("swph %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("swp %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("swp %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_SWPA:
        switch (bytes)
        {
        case 1: asm volatile("swpab %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("swpah %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("swpa %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("swpa %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_SWPL:
        switch (bytes)
        {
        case 1: asm volatile("swplb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("swplh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("swpl %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("swpl %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_SWPAL:
        switch (bytes)
        {
        case 1: asm volatile("swpalb %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 2: asm volatile("swpalh %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 4: asm volatile("swpal %w2, %w0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        case 8: asm volatile("swpal %2, %0, [%1]" : "=&r"(value) : "r"(addr), "r"(src) : "memory"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }

    *old = value;
    return true;
}

static inline bool emu_hw_cas(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t desired, uint64_t *expected)
{
    uint64_t value = *expected;

    switch (instruction)
    {
    case ARM64_INSN_CAS:
        switch (bytes)
        {
        case 1: asm volatile("casb %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 2: asm volatile("cash %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 4: asm volatile("cas %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 8: asm volatile("cas %0, %2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASA:
        switch (bytes)
        {
        case 1: asm volatile("casab %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 2: asm volatile("casah %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 4: asm volatile("casa %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 8: asm volatile("casa %0, %2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASL:
        switch (bytes)
        {
        case 1: asm volatile("caslb %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 2: asm volatile("caslh %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 4: asm volatile("casl %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 8: asm volatile("casl %0, %2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASAL:
        switch (bytes)
        {
        case 1: asm volatile("casalb %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 2: asm volatile("casalh %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 4: asm volatile("casal %w0, %w2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        case 8: asm volatile("casal %0, %2, [%1]" : "+&r"(value) : "r"(addr), "r"(desired) : "memory"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    *expected = value;
    return true;
}

static inline bool emu_hw_casp(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t desired0, uint64_t desired1, uint64_t *expected0, uint64_t *expected1)
{
    uint64_t input0 = *expected0;
    uint64_t input1 = *expected1;
    uint64_t output0, output1;

    switch (instruction)
    {
    case ARM64_INSN_CASP:
        switch (bytes)
        {
        case 4: asm volatile("mov w0, %w2\nmov w1, %w3\nmov w2, %w4\nmov w3, %w5\nmov x4, %6\ncasp w0, w1, w2, w3, [x4]\nmov %w0, w0\nmov %w1, w1" : "=&r"(output0), "=&r"(output1) : "r"(input0), "r"(input1), "r"(desired0), "r"(desired1), "r"(addr) : "memory", "x0", "x1", "x2", "x3", "x4"); break;
        case 8: asm volatile("mov x0, %2\nmov x1, %3\nmov x2, %4\nmov x3, %5\nmov x4, %6\ncasp x0, x1, x2, x3, [x4]\nmov %0, x0\nmov %1, x1" : "=&r"(output0), "=&r"(output1) : "r"(input0), "r"(input1), "r"(desired0), "r"(desired1), "r"(addr) : "memory", "x0", "x1", "x2", "x3", "x4"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASPA:
        switch (bytes)
        {
        case 4: asm volatile("mov w0, %w2\nmov w1, %w3\nmov w2, %w4\nmov w3, %w5\nmov x4, %6\ncaspa w0, w1, w2, w3, [x4]\nmov %w0, w0\nmov %w1, w1" : "=&r"(output0), "=&r"(output1) : "r"(input0), "r"(input1), "r"(desired0), "r"(desired1), "r"(addr) : "memory", "x0", "x1", "x2", "x3", "x4"); break;
        case 8: asm volatile("mov x0, %2\nmov x1, %3\nmov x2, %4\nmov x3, %5\nmov x4, %6\ncaspa x0, x1, x2, x3, [x4]\nmov %0, x0\nmov %1, x1" : "=&r"(output0), "=&r"(output1) : "r"(input0), "r"(input1), "r"(desired0), "r"(desired1), "r"(addr) : "memory", "x0", "x1", "x2", "x3", "x4"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASPL:
        switch (bytes)
        {
        case 4: asm volatile("mov w0, %w2\nmov w1, %w3\nmov w2, %w4\nmov w3, %w5\nmov x4, %6\ncaspl w0, w1, w2, w3, [x4]\nmov %w0, w0\nmov %w1, w1" : "=&r"(output0), "=&r"(output1) : "r"(input0), "r"(input1), "r"(desired0), "r"(desired1), "r"(addr) : "memory", "x0", "x1", "x2", "x3", "x4"); break;
        case 8: asm volatile("mov x0, %2\nmov x1, %3\nmov x2, %4\nmov x3, %5\nmov x4, %6\ncaspl x0, x1, x2, x3, [x4]\nmov %0, x0\nmov %1, x1" : "=&r"(output0), "=&r"(output1) : "r"(input0), "r"(input1), "r"(desired0), "r"(desired1), "r"(addr) : "memory", "x0", "x1", "x2", "x3", "x4"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASPAL:
        switch (bytes)
        {
        case 4: asm volatile("mov w0, %w2\nmov w1, %w3\nmov w2, %w4\nmov w3, %w5\nmov x4, %6\ncaspal w0, w1, w2, w3, [x4]\nmov %w0, w0\nmov %w1, w1" : "=&r"(output0), "=&r"(output1) : "r"(input0), "r"(input1), "r"(desired0), "r"(desired1), "r"(addr) : "memory", "x0", "x1", "x2", "x3", "x4"); break;
        case 8: asm volatile("mov x0, %2\nmov x1, %3\nmov x2, %4\nmov x3, %5\nmov x4, %6\ncaspal x0, x1, x2, x3, [x4]\nmov %0, x0\nmov %1, x1" : "=&r"(output0), "=&r"(output1) : "r"(input0), "r"(input1), "r"(desired0), "r"(desired1), "r"(addr) : "memory", "x0", "x1", "x2", "x3", "x4"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    *expected0 = output0;
    *expected1 = output1;
    return true;
}

static inline bool emu_hw_ordered_load(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t *out)
{
    uint64_t value;

    switch (instruction)
    {
    case ARM64_INSN_LDLAR:
        switch (bytes)
        {
        case 1: asm volatile("mov x1, %1\n.inst 0x08DF7C20\nmov %w0, w0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        case 2: asm volatile("mov x1, %1\n.inst 0x48DF7C20\nmov %w0, w0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        case 4: asm volatile("mov x1, %1\n.inst 0x88DF7C20\nmov %w0, w0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        case 8: asm volatile("mov x1, %1\n.inst 0xC8DF7C20\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDAR:
        switch (bytes)
        {
        case 1: asm volatile("mov x1, %1\n.inst 0x08DFFC20\nmov %w0, w0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        case 2: asm volatile("mov x1, %1\n.inst 0x48DFFC20\nmov %w0, w0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        case 4: asm volatile("mov x1, %1\n.inst 0x88DFFC20\nmov %w0, w0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        case 8: asm volatile("mov x1, %1\n.inst 0xC8DFFC20\nmov %0, x0" : "=&r"(value) : "r"(addr) : "memory", "x0", "x1"); break;
        default: return false;
        }
        break;
    default:
        return false;
    }

    *out = value;
    return true;
}

static inline bool emu_hw_ordered_store(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t value)
{
    switch (instruction)
    {
    case ARM64_INSN_STLLR:
        switch (bytes)
        {
        case 1: asm volatile("mov w0, %w0\nmov x1, %1\n.inst 0x089F7C20" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
        case 2: asm volatile("mov w0, %w0\nmov x1, %1\n.inst 0x489F7C20" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
        case 4: asm volatile("mov w0, %w0\nmov x1, %1\n.inst 0x889F7C20" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
        case 8: asm volatile("mov x0, %0\nmov x1, %1\n.inst 0xC89F7C20" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_STLR:
        switch (bytes)
        {
        case 1: asm volatile("mov w0, %w0\nmov x1, %1\n.inst 0x089FFC20" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
        case 2: asm volatile("mov w0, %w0\nmov x1, %1\n.inst 0x489FFC20" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
        case 4: asm volatile("mov w0, %w0\nmov x1, %1\n.inst 0x889FFC20" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
        case 8: asm volatile("mov x0, %0\nmov x1, %1\n.inst 0xC89FFC20" : : "r"(value), "r"(addr) : "memory", "x0", "x1"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_hw_exclusive_load(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t *first, uint64_t *second)
{
    uint64_t value0, value1 = 0;

    switch (bytes)
    {
    case 1:
        switch (instruction)
        {
        case ARM64_INSN_LDXR: asm volatile("ldxrb %w0, [%1]" : "=&r"(value0) : "r"(addr) : "memory"); break;
        case ARM64_INSN_LDAXR: asm volatile("ldaxrb %w0, [%1]" : "=&r"(value0) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case 2:
        switch (instruction)
        {
        case ARM64_INSN_LDXR: asm volatile("ldxrh %w0, [%1]" : "=&r"(value0) : "r"(addr) : "memory"); break;
        case ARM64_INSN_LDAXR: asm volatile("ldaxrh %w0, [%1]" : "=&r"(value0) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case 4:
        switch (instruction)
        {
        case ARM64_INSN_LDXR: asm volatile("ldxr %w0, [%1]" : "=&r"(value0) : "r"(addr) : "memory"); break;
        case ARM64_INSN_LDAXR: asm volatile("ldaxr %w0, [%1]" : "=&r"(value0) : "r"(addr) : "memory"); break;
        case ARM64_INSN_LDXP: asm volatile("ldxp %w0, %w1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory"); break;
        case ARM64_INSN_LDAXP: asm volatile("ldaxp %w0, %w1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    case 8:
        switch (instruction)
        {
        case ARM64_INSN_LDXR: asm volatile("ldxr %0, [%1]" : "=&r"(value0) : "r"(addr) : "memory"); break;
        case ARM64_INSN_LDAXR: asm volatile("ldaxr %0, [%1]" : "=&r"(value0) : "r"(addr) : "memory"); break;
        case ARM64_INSN_LDXP: asm volatile("ldxp %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory"); break;
        case ARM64_INSN_LDAXP: asm volatile("ldaxp %0, %1, [%2]" : "=&r"(value0), "=&r"(value1) : "r"(addr) : "memory"); break;
        default: return false;
        }
        break;
    default: return false;
    }

    *first = value0;
    if (instruction == ARM64_INSN_LDXP || instruction == ARM64_INSN_LDAXP) *second = value1;
    return true;
}

static inline bool emu_hw_exclusive_store(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t first, uint64_t second, uint32_t *status)
{
    uint32_t result;

    switch (bytes)
    {
    case 1:
        switch (instruction)
        {
        case ARM64_INSN_STXR: asm volatile("stxrb %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory"); break;
        case ARM64_INSN_STLXR: asm volatile("stlxrb %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory"); break;
        default: return false;
        }
        break;
    case 2:
        switch (instruction)
        {
        case ARM64_INSN_STXR: asm volatile("stxrh %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory"); break;
        case ARM64_INSN_STLXR: asm volatile("stlxrh %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory"); break;
        default: return false;
        }
        break;
    case 4:
        switch (instruction)
        {
        case ARM64_INSN_STXR: asm volatile("stxr %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory"); break;
        case ARM64_INSN_STLXR: asm volatile("stlxr %w0, %w2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory"); break;
        case ARM64_INSN_STXP: asm volatile("stxp %w0, %w2, %w3, [%1]" : "=&r"(result) : "r"(addr), "r"(first), "r"(second) : "memory"); break;
        case ARM64_INSN_STLXP: asm volatile("stlxp %w0, %w2, %w3, [%1]" : "=&r"(result) : "r"(addr), "r"(first), "r"(second) : "memory"); break;
        default: return false;
        }
        break;
    case 8:
        switch (instruction)
        {
        case ARM64_INSN_STXR: asm volatile("stxr %w0, %2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory"); break;
        case ARM64_INSN_STLXR: asm volatile("stlxr %w0, %2, [%1]" : "=&r"(result) : "r"(addr), "r"(first) : "memory"); break;
        case ARM64_INSN_STXP: asm volatile("stxp %w0, %2, %3, [%1]" : "=&r"(result) : "r"(addr), "r"(first), "r"(second) : "memory"); break;
        case ARM64_INSN_STLXP: asm volatile("stlxp %w0, %2, %3, [%1]" : "=&r"(result) : "r"(addr), "r"(first), "r"(second) : "memory"); break;
        default: return false;
        }
        break;
    default: return false;
    }

    *status = result;
    return true;
}
// clang-format on

static inline enum emu_insn_result emu_simulate_load_store_insn(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_decoded_insn *decoded)
{
    /*
    每个分支都按“读取软件现场 -> 调用固定硬件模板 -> 写回软件现场”的顺序提交结果。
    helper 不支持当前编码属性时返回 EMU_INSN_SKIP；只有语义完整执行后才将 PC 推进 4 字节。
    带 writeback 的寻址形式也在访存成功后才更新基址寄存器 Rn。
    */

    //保存一个不会随着 regs->pc 写回而变化的原始 PC 值。,防止推进pc后，使用pc相对偏移计算错误
    uint64_t pc = regs->pc;

    switch (decoded->instruction)
    {
    // 单寄存器独占加载：建立独占监视，并将加载值写回 Rt。
    case ARM64_INSN_LDXR:
    case ARM64_INSN_LDAXR:
    {
        // 单寄存器形式复用成对加载 helper，第二个输出不参与架构结果。
        uint64_t value, unused;
        uint64_t addr = addr_reg_read(regs, decoded->rn);

        if (!emu_hw_exclusive_load(decoded->instruction, addr, decoded->access_bytes, &value, &unused)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, value, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占加载：建立独占监视，并将两个结果分别写回 Rt 和 Rt2。
    case ARM64_INSN_LDXP:
    case ARM64_INSN_LDAXP:
    {
        uint64_t value0, value1;
        uint64_t addr = addr_reg_read(regs, decoded->rn);

        if (!emu_hw_exclusive_load(decoded->instruction, addr, decoded->access_bytes, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, value0, decoded->operand_width == 64);
        reg_write(regs, decoded->rt2, value1, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器独占存储：数据来自 Rt，硬件成败状态写回 Ws（0 表示成功）。
    case ARM64_INSN_STXR:
    case ARM64_INSN_STLXR:
    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, decoded->rn);

        if (!emu_hw_exclusive_store(decoded->instruction, addr, decoded->access_bytes, reg_read(regs, decoded->rt), 0, &status)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rs, status, false);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占存储：数据来自 Rt/Rt2，硬件成败状态写回 Ws。
    case ARM64_INSN_STXP:
    case ARM64_INSN_STLXP:
    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, decoded->rn);

        if (!emu_hw_exclusive_store(decoded->instruction, addr, decoded->access_bytes, reg_read(regs, decoded->rt), reg_read(regs, decoded->rt2), &status)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rs, status, false);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 成对比较交换：Rs/Rs+1 提供期望值并接收内存旧值，Rt/Rt+1 提供目标值。
    case ARM64_INSN_CASP:
    case ARM64_INSN_CASPA:
    case ARM64_INSN_CASPL:
    case ARM64_INSN_CASPAL:
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        uint64_t expected0 = reg_read(regs, decoded->rs);
        uint64_t expected1 = reg_read(regs, decoded->rs + 1);

        if (!emu_hw_casp(decoded->instruction, addr, decoded->access_bytes, reg_read(regs, decoded->rt), reg_read(regs, decoded->rt + 1), &expected0, &expected1)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rs, expected0, decoded->operand_width == 64);
        reg_write(regs, decoded->rs + 1, expected1, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 有序存储：按 instruction 保留 release 或 limited-ordering 语义。
    case ARM64_INSN_STLLR:
    case ARM64_INSN_STLR:
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn);

        if (!emu_hw_ordered_store(decoded->instruction, addr, decoded->access_bytes, reg_read(regs, decoded->rt))) return EMU_INSN_SKIP;
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 有序加载：按 instruction 保留 acquire 或 limited-ordering 语义并写回 Rt。
    case ARM64_INSN_LDLAR:
    case ARM64_INSN_LDAR:
    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, decoded->rn);

        if (!emu_hw_ordered_load(decoded->instruction, addr, decoded->access_bytes, &value)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, value, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器比较交换：Rs 提供期望值并接收内存旧值，Rt 提供目标值。
    case ARM64_INSN_CAS:
    case ARM64_INSN_CASA:
    case ARM64_INSN_CASL:
    case ARM64_INSN_CASAL:
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        uint64_t expected = reg_read(regs, decoded->rs);

        if (!emu_hw_cas(decoded->instruction, addr, decoded->access_bytes, reg_read(regs, decoded->rt), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rs, expected, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
    case ARM64_INSN_LDADD:
    case ARM64_INSN_LDADDA:
    case ARM64_INSN_LDADDL:
    case ARM64_INSN_LDADDAL:
    case ARM64_INSN_LDCLR:
    case ARM64_INSN_LDCLRA:
    case ARM64_INSN_LDCLRL:
    case ARM64_INSN_LDCLRAL:
    case ARM64_INSN_LDEOR:
    case ARM64_INSN_LDEORA:
    case ARM64_INSN_LDEORL:
    case ARM64_INSN_LDEORAL:
    case ARM64_INSN_LDSET:
    case ARM64_INSN_LDSETA:
    case ARM64_INSN_LDSETL:
    case ARM64_INSN_LDSETAL:
    case ARM64_INSN_LDSMAX:
    case ARM64_INSN_LDSMAXA:
    case ARM64_INSN_LDSMAXL:
    case ARM64_INSN_LDSMAXAL:
    case ARM64_INSN_LDSMIN:
    case ARM64_INSN_LDSMINA:
    case ARM64_INSN_LDSMINL:
    case ARM64_INSN_LDSMINAL:
    case ARM64_INSN_LDUMAX:
    case ARM64_INSN_LDUMAXA:
    case ARM64_INSN_LDUMAXL:
    case ARM64_INSN_LDUMAXAL:
    case ARM64_INSN_LDUMIN:
    case ARM64_INSN_LDUMINA:
    case ARM64_INSN_LDUMINL:
    case ARM64_INSN_LDUMINAL:
    case ARM64_INSN_SWP:
    case ARM64_INSN_SWPA:
    case ARM64_INSN_SWPL:
    case ARM64_INSN_SWPAL:
    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, decoded->rn);

        if (!emu_hw_atomic_rmw(decoded->instruction, addr, decoded->access_bytes, reg_read(regs, decoded->rs), &old)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, old, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
    case ARM64_INSN_LDAPR:
    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, decoded->rn);

        if (!emu_hw_load_ldapr(addr, decoded->access_bytes, &value)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, value, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 字面量加载：有效地址相对当前指令 PC 计算，不使用 Rn。
    case ARM64_INSN_LDR_LITERAL_GPR:
    case ARM64_INSN_LDRSW_LITERAL:
    {
        uint64_t address;
        uint64_t value;

        if (!emu_resolve_memory_address(decoded, regs, pc, 0, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(decoded->instruction, address, decoded->access_bytes, decoded->operand_width == 64, &value)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, value, decoded->operand_width == 64);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 字面量加载：按 PC 相对地址直接写入目标 Q 寄存器的软件现场。
    case ARM64_INSN_LDR_LITERAL_FP_SIMD:
    {
        uint64_t address;

        if (!emu_resolve_memory_address(decoded, regs, pc, 0, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_load_fp(decoded->instruction, address, decoded->access_bytes, &fp_regs->q[decoded->rt])) return EMU_INSN_SKIP;

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // RCpc 非对齐有序存储：地址为 Rn 加已解码的未缩放偏移。
    case ARM64_INSN_STLUR:
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn) + decoded->offset;

        if (!emu_hw_store_rcpc(decoded->instruction, addr, decoded->access_bytes, reg_read(regs, decoded->rt))) return EMU_INSN_SKIP;
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // RCpc 非对齐有序加载：地址为 Rn 加未缩放偏移，结果按目标宽度写回 Rt。
    case ARM64_INSN_LDAPUR:
    case ARM64_INSN_LDAPUR_SIGNED:
    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, decoded->rn) + decoded->offset;

        if (!emu_hw_load_rcpc(decoded->instruction, addr, decoded->access_bytes, decoded->operand_width == 64, &value)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, value, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
    case ARM64_INSN_LDNP_GPR:
    case ARM64_INSN_LDP_GPR_OFFSET:
    case ARM64_INSN_LDPSW_OFFSET:
    case ARM64_INSN_LDP_GPR_POST_INDEX:
    case ARM64_INSN_LDPSW_POST_INDEX:
    case ARM64_INSN_LDP_GPR_PRE_INDEX:
    case ARM64_INSN_LDPSW_PRE_INDEX:
    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, decoded->rn);

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(decoded->instruction, address, decoded->access_bytes, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, value0, decoded->operand_width == 64);
        reg_write(regs, decoded->rt2, value1, decoded->operand_width == 64);
        emu_commit_memory_writeback(decoded, regs, base);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
    case ARM64_INSN_STNP_GPR:
    case ARM64_INSN_STP_GPR_OFFSET:
    case ARM64_INSN_STP_GPR_POST_INDEX:
    case ARM64_INSN_STP_GPR_PRE_INDEX:
    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, decoded->rn);

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_gpr(decoded->instruction, address, decoded->access_bytes, reg_read(regs, decoded->rt), reg_read(regs, decoded->rt2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对加载：结果直接写入两个 Q 寄存器的软件现场。
    case ARM64_INSN_LDNP_FP_SIMD:
    case ARM64_INSN_LDP_FP_SIMD_OFFSET:
    case ARM64_INSN_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INSN_LDP_FP_SIMD_PRE_INDEX:
    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, decoded->rn);

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_fp(decoded->instruction, address, decoded->access_bytes, &fp_regs->q[decoded->rt], &fp_regs->q[decoded->rt2])) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
    case ARM64_INSN_STNP_FP_SIMD:
    case ARM64_INSN_STP_FP_SIMD_OFFSET:
    case ARM64_INSN_STP_FP_SIMD_POST_INDEX:
    case ARM64_INSN_STP_FP_SIMD_PRE_INDEX:
    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, decoded->rn);

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_fp(decoded->instruction, address, decoded->access_bytes, &fp_regs->q[decoded->rt], &fp_regs->q[decoded->rt2])) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
    case ARM64_INSN_LDUR_GPR:
    case ARM64_INSN_LDUR_SIGNED_GPR:
    case ARM64_INSN_LDTR_GPR:
    case ARM64_INSN_LDTR_SIGNED_GPR:
    case ARM64_INSN_LDR_GPR_POST_INDEX:
    case ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX:
    case ARM64_INSN_LDR_GPR_PRE_INDEX:
    case ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX:
    case ARM64_INSN_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET:
    case ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET:
    case ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET:
    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, decoded->rn);

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(decoded->instruction, address, decoded->access_bytes, decoded->operand_width == 64, &value)) return EMU_INSN_SKIP;
        reg_write(regs, decoded->rt, value, decoded->operand_width == 64);
        emu_commit_memory_writeback(decoded, regs, base);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
    case ARM64_INSN_STUR_GPR:
    case ARM64_INSN_STTR_GPR:
    case ARM64_INSN_STR_GPR_POST_INDEX:
    case ARM64_INSN_STR_GPR_PRE_INDEX:
    case ARM64_INSN_STR_GPR_REGISTER_OFFSET:
    case ARM64_INSN_STR_GPR_UNSIGNED_OFFSET:
    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, decoded->rn);

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(decoded->instruction, address, decoded->access_bytes, reg_read(regs, decoded->rt))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
    case ARM64_INSN_LDUR_FP_SIMD:
    case ARM64_INSN_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INSN_LDR_FP_SIMD_PRE_INDEX:
    case ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET:
    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, decoded->rn);

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_load_fp(decoded->instruction, address, decoded->access_bytes, &fp_regs->q[decoded->rt])) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
    case ARM64_INSN_STUR_FP_SIMD:
    case ARM64_INSN_STR_FP_SIMD_POST_INDEX:
    case ARM64_INSN_STR_FP_SIMD_PRE_INDEX:
    case ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET:
    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, decoded->rn);

        if (!emu_resolve_memory_address(decoded, regs, pc, base, &address)) return EMU_INSN_SKIP;
        if (!emu_hw_store_fp(decoded->instruction, address, decoded->access_bytes, &fp_regs->q[decoded->rt])) return EMU_INSN_SKIP;
        emu_commit_memory_writeback(decoded, regs, base);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
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

/* ======================== FP / AdvSIMD：固定硬件模板与现场转换辅助 ======================== */

// clang-format off
static inline bool emu_fp_unary_merge_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FCVT_D_S:
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvt d0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1");
        return true;
    case ARM64_INSN_FCVT_S_D:
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvt s0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1");
        return true;
    case ARM64_INSN_XTN2_VECTOR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nxtn2 v0.16b, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nxtn2 v0.8h, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nxtn2 v0.4s, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQXTN2_VECTOR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nsqxtn2 v0.16b, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nsqxtn2 v0.8h, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nsqxtn2 v0.4s, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQXTUN2_VECTOR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nsqxtun2 v0.16b, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nsqxtun2 v0.8h, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nsqxtun2 v0.4s, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQXTN2_VECTOR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nuqxtn2 v0.16b, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nuqxtn2 v0.8h, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nuqxtn2 v0.4s, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_gpr_to_fp_hw(enum arm64_instruction instruction, void *dst, uint64_t value)
{
    switch (instruction)
    {
    case ARM64_INSN_SCVTF_S_W: asm volatile("ldr q0, [%0]\nscvtf s0, %w1\nstr q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0"); return true;
    case ARM64_INSN_UCVTF_S_W: asm volatile("ldr q0, [%0]\nucvtf s0, %w1\nstr q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0"); return true;
    case ARM64_INSN_SCVTF_D_W: asm volatile("ldr q0, [%0]\nscvtf d0, %w1\nstr q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0"); return true;
    case ARM64_INSN_UCVTF_D_W: asm volatile("ldr q0, [%0]\nucvtf d0, %w1\nstr q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0"); return true;
    case ARM64_INSN_SCVTF_S_X: asm volatile("ldr q0, [%0]\nscvtf s0, %1\nstr q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0"); return true;
    case ARM64_INSN_UCVTF_S_X: asm volatile("ldr q0, [%0]\nucvtf s0, %1\nstr q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0"); return true;
    case ARM64_INSN_SCVTF_D_X: asm volatile("ldr q0, [%0]\nscvtf d0, %1\nstr q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0"); return true;
    case ARM64_INSN_UCVTF_D_X: asm volatile("ldr q0, [%0]\nucvtf d0, %1\nstr q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0"); return true;
    default: return false;
    }
}
// clang-format on

// 标量转换保留目标寄存器未被标量结果覆盖的位；向量转换覆盖完整目标向量。
// clang-format off
static inline bool emu_fp_convert_simd_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FCVTNS_SIMD_VECTOR:
    case ARM64_INSN_FCVTNS_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtns s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtns d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtns v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtns v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtns v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCVTMS_SIMD_VECTOR:
    case ARM64_INSN_FCVTMS_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtms s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtms d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtms v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtms v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtms v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCVTAS_SIMD_VECTOR:
    case ARM64_INSN_FCVTAS_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtas s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtas d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtas v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtas v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtas v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SCVTF_SIMD_VECTOR:
    case ARM64_INSN_SCVTF_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nscvtf s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nscvtf d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nscvtf v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nscvtf v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nscvtf v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCVTPS_SIMD_VECTOR:
    case ARM64_INSN_FCVTPS_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtps s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtps d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtps v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtps v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtps v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCVTZS_SIMD_VECTOR:
    case ARM64_INSN_FCVTZS_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtzs s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtzs d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtzs v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtzs v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtzs v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCVTNU_SIMD_VECTOR:
    case ARM64_INSN_FCVTNU_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtnu s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtnu d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtnu v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtnu v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtnu v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCVTMU_SIMD_VECTOR:
    case ARM64_INSN_FCVTMU_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtmu s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtmu d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtmu v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtmu v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtmu v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCVTAU_SIMD_VECTOR:
    case ARM64_INSN_FCVTAU_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtau s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtau d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtau v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtau v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtau v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UCVTF_SIMD_VECTOR:
    case ARM64_INSN_UCVTF_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nucvtf s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nucvtf d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nucvtf v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nucvtf v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nucvtf v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCVTPU_SIMD_VECTOR:
    case ARM64_INSN_FCVTPU_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtpu s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtpu d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtpu v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtpu v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtpu v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCVTZU_SIMD_VECTOR:
    case ARM64_INSN_FCVTZU_SIMD_SCALAR:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtzu s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfcvtzu d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcvtzu v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtzu v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtzu v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_fp_to_gpr_hw(enum arm64_instruction instruction, const void *source, uint32_t gpr_width, uint32_t fp_width, uint64_t *value)
{
    uint32_t value32;
    uint64_t value64;

    if (!value) return false;

    switch (instruction)
    {
    case ARM64_INSN_FCVTNS_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtns %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtns %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtns %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtns %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case ARM64_INSN_FCVTNU_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtnu %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtnu %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtnu %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtnu %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case ARM64_INSN_FCVTAS_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtas %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtas %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtas %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtas %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case ARM64_INSN_FCVTAU_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtau %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtau %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtau %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtau %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case ARM64_INSN_FCVTPS_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtps %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtps %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtps %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtps %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case ARM64_INSN_FCVTPU_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtpu %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtpu %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtpu %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtpu %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case ARM64_INSN_FCVTMS_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtms %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtms %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtms %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtms %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case ARM64_INSN_FCVTMU_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtmu %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtmu %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtmu %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtmu %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case ARM64_INSN_FCVTZS_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtzs %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtzs %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtzs %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtzs %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case ARM64_INSN_FCVTZU_GPR:
        switch (gpr_width)
        {
        case 32:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtzu %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtzu %w0, d1\n" : "=r"(value32) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (fp_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcvtzu %0, s1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcvtzu %0, d1\n" : "=r"(value64) : "r"(source) : "memory", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    default:
        return false;
    }

    *value = gpr_width == 32 ? value32 : value64;
    return true;
}

static inline bool emu_fp_select_hw(void *dst, const void *left, const void *right, uint64_t nzcv, uint32_t condition, uint32_t width)
{
    uint32_t take = emu_cond_holds(nzcv, condition);

    switch (width)
    {
    case 16: asm volatile("ldr q1, [%1]\n" "ldr q2, [%2]\n" "cmp %w3, #0\n" "fcsel h0, h1, h2, ne\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(take) : "memory", "cc", "v0", "v1", "v2"); return true;
    case 32: asm volatile("ldr q1, [%1]\n" "ldr q2, [%2]\n" "cmp %w3, #0\n" "fcsel s0, s1, s2, ne\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(take) : "memory", "cc", "v0", "v1", "v2"); return true;
    case 64: asm volatile("ldr q1, [%1]\n" "ldr q2, [%2]\n" "cmp %w3, #0\n" "fcsel d0, d1, d2, ne\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(take) : "memory", "cc", "v0", "v1", "v2"); return true;
    default: return false;
    }
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
        asm volatile("" "fmov s0, %w1\n" "str q0, [%0]\n" : : "r"(dst), "r"((uint32_t)value) : "memory", "v0");
        return true;
    }
    if (width == 64)
    {
        asm volatile("" "fmov d0, %1\n" "str q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0");
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
        asm volatile("" "ldr q1, [%1]\n" "fmov %w0, s1\n" : "=r"(value32) : "r"(source) : "memory", "v1");
        *value = value32;
        return true;
    }
    if (width == 64)
    {
        asm volatile("" "ldr q1, [%1]\n" "fmov %0, d1\n" : "=r"(*value) : "r"(source) : "memory", "v1");
        return true;
    }
    return false;
}

static inline bool emu_simd_dup_general_hw(void *dst, uint64_t value, uint32_t element_width, uint32_t vector_width)
{
    if (vector_width == 64)
    {
        switch (element_width)
        {
        case 8:
            asm volatile("" "dup v0.8b, %w1\n" "str q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0");
            return true;
        case 16:
            asm volatile("" "dup v0.4h, %w1\n" "str q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0");
            return true;
        case 32:
            asm volatile("" "dup v0.2s, %w1\n" "str q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0");
            return true;
        default:
            return false;
        }
    }

    if (vector_width != 128) return false;

    switch (element_width)
    {
    case 8:
        asm volatile("" "dup v0.16b, %w1\n" "str q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0");
        return true;
    case 16:
        asm volatile("" "dup v0.8h, %w1\n" "str q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0");
        return true;
    case 32:
        asm volatile("" "dup v0.4s, %w1\n" "str q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0");
        return true;
    case 64:
        asm volatile("" "dup v0.2d, %1\n" "str q0, [%0]\n" : : "r"(dst), "r"(value) : "memory", "v0");
        return true;
    default:
        return false;
    }
}
// clang-format on

static inline bool emu_simd_materialize_bits_hw(void *dst, uint64_t value, uint32_t vector_width)
{
    if (vector_width == 64) return emu_simd_write_scalar_hw(dst, value, 64);
    if (vector_width == 128) return emu_simd_dup_general_hw(dst, value, 64, 128);
    return false;
}

// clang-format off
static inline bool emu_fp_scalar_binary_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t operand_width)
{
    if (operand_width == 16 && !arm64_current_cpu_has_fp16()) return false;

    switch (instruction)
    {
    case ARM64_INSN_FMUL_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" ".inst 0x1EE20820\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 32: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fmul s0, s1, s2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 64: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fmul d0, d1, d2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        default: return false;
        }
    case ARM64_INSN_FDIV_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" ".inst 0x1EE21820\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 32: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fdiv s0, s1, s2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 64: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fdiv d0, d1, d2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        default: return false;
        }
    case ARM64_INSN_FADD_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" ".inst 0x1EE22820\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 32: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fadd s0, s1, s2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 64: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fadd d0, d1, d2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        default: return false;
        }
    case ARM64_INSN_FSUB_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" ".inst 0x1EE23820\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 32: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fsub s0, s1, s2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 64: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fsub d0, d1, d2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        default: return false;
        }
    case ARM64_INSN_FMAX_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" ".inst 0x1EE24820\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 32: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fmax s0, s1, s2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 64: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fmax d0, d1, d2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        default: return false;
        }
    case ARM64_INSN_FMIN_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" ".inst 0x1EE25820\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 32: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fmin s0, s1, s2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 64: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fmin d0, d1, d2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        default: return false;
        }
    case ARM64_INSN_FMAXNM_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" ".inst 0x1EE26820\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 32: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fmaxnm s0, s1, s2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 64: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fmaxnm d0, d1, d2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        default: return false;
        }
    case ARM64_INSN_FMINNM_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" ".inst 0x1EE27820\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 32: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fminnm s0, s1, s2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 64: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fminnm d0, d1, d2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        default: return false;
        }
    case ARM64_INSN_FNMUL_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" ".inst 0x1EE28820\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 32: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fnmul s0, s1, s2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        case 64: asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "fnmul d0, d1, d2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); return true;
        default: return false;
        }
    default:
        return false;
    }
}
// clang-format on

// 标量 FMOV 覆盖目标值；其余一元指令先装载旧 q0，保留目标寄存器未被标量结果覆盖的位。
// clang-format off
static inline bool emu_fp_scalar_unary_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t operand_width)
{
    if (operand_width == 16 && !arm64_current_cpu_has_fp16()) return false;

    switch (instruction)
    {
    case ARM64_INSN_FMOV_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q1, [%1]\n.inst 0x1EE04020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q1, [%1]\nfmov s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q1, [%1]\nfmov d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FABS_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE0C020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfabs s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfabs d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FNEG_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE14020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfneg s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfneg d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FSQRT_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE1C020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfsqrt s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfsqrt d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FRINTN_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE44020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintn s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintn d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FRINTP_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE4C020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintp s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintp d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FRINTM_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE54020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintm s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintm d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FRINTZ_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE5C020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintz s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintz d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FRINTA_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE64020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrinta s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrinta d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FRINTX_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE74020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintx s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrintx d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    case ARM64_INSN_FRINTI_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\n.inst 0x1EE7C020\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrinti s0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nfrinti d0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); return true;
        default: return false;
        }
    default:
        return false;
    }
}

static inline bool emu_fp_scalar_ternary_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, const void *addend, uint32_t operand_width)
{
    if (operand_width == 16 && !arm64_current_cpu_has_fp16()) return false;

    switch (instruction)
    {
    case ARM64_INSN_FMADD_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\n.inst 0x1FC20C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\nfmadd s0, s1, s2, s3\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\nfmadd d0, d1, d2, d3\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        default: return false;
        }
    case ARM64_INSN_FMSUB_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\n.inst 0x1FC28C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\nfmsub s0, s1, s2, s3\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\nfmsub d0, d1, d2, d3\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        default: return false;
        }
    case ARM64_INSN_FNMADD_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\n.inst 0x1FE20C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\nfnmadd s0, s1, s2, s3\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\nfnmadd d0, d1, d2, d3\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        default: return false;
        }
    case ARM64_INSN_FNMSUB_SCALAR:
        switch (operand_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\n.inst 0x1FE28C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\nfnmsub s0, s1, s2, s3\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nldr q3, [%3]\nfnmsub d0, d1, d2, d3\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right), "r"(addend) : "memory", "v0", "v1", "v2", "v3"); return true;
        default: return false;
        }
    default:
        return false;
    }
}

static inline bool emu_scalar_fp_compare_register_hw(uint32_t operand_width, const void *left, const void *right, uint64_t *nzcv)
{
    if (!nzcv) return false;

    switch (operand_width)
    {
    case 16:
        if (!arm64_current_cpu_has_fp16()) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x1EE22020\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left), "r"(right) : "memory", "cc", "v1", "v2");
        return true;
    case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmp s1, s2\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left), "r"(right) : "memory", "cc", "v1", "v2"); return true;
    case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmp d1, d2\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left), "r"(right) : "memory", "cc", "v1", "v2"); return true;
    default: return false;
    }
}

static inline bool emu_scalar_fp_compare_register_signaling_hw(uint32_t operand_width, const void *left, const void *right, uint64_t *nzcv)
{
    if (!nzcv) return false;

    switch (operand_width)
    {
    case 16:
        if (!arm64_current_cpu_has_fp16()) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x1EE22030\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left), "r"(right) : "memory", "cc", "v1", "v2");
        return true;
    case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmpe s1, s2\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left), "r"(right) : "memory", "cc", "v1", "v2"); return true;
    case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmpe d1, d2\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(left), "r"(right) : "memory", "cc", "v1", "v2"); return true;
    default: return false;
    }
}

static inline bool emu_scalar_fp_compare_zero_hw(uint32_t operand_width, const void *source, uint64_t *nzcv)
{
    if (!nzcv) return false;

    switch (operand_width)
    {
    case 16:
        if (!arm64_current_cpu_has_fp16()) return false;
        asm volatile("ldr q1, [%1]\n.inst 0x1EE02028\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(source) : "memory", "cc", "v1");
        return true;
    case 32: asm volatile("ldr q1, [%1]\nfcmp s1, #0.0\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(source) : "memory", "cc", "v1"); return true;
    case 64: asm volatile("ldr q1, [%1]\nfcmp d1, #0.0\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(source) : "memory", "cc", "v1"); return true;
    default: return false;
    }
}

static inline bool emu_scalar_fp_compare_zero_signaling_hw(uint32_t operand_width, const void *source, uint64_t *nzcv)
{
    if (!nzcv) return false;

    switch (operand_width)
    {
    case 16:
        if (!arm64_current_cpu_has_fp16()) return false;
        asm volatile("ldr q1, [%1]\n.inst 0x1EE02038\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(source) : "memory", "cc", "v1");
        return true;
    case 32: asm volatile("ldr q1, [%1]\nfcmpe s1, #0.0\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(source) : "memory", "cc", "v1"); return true;
    case 64: asm volatile("ldr q1, [%1]\nfcmpe d1, #0.0\nmrs %0, nzcv\n" : "=r"(*nzcv) : "r"(source) : "memory", "cc", "v1"); return true;
    default: return false;
    }
}

static inline bool emu_simd_rdm_accumulate_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SQRDMLAH_VECTOR:
    case ARM64_INSN_SQRDMLAH_VECTOR_BY_ELEMENT:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E428420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E828420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E428420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E828420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQRDMLSH_VECTOR:
    case ARM64_INSN_SQRDMLSH_VECTOR_BY_ELEMENT:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E428C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E828C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E428C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E828C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQRDMLAH_SCALAR:
    case ARM64_INSN_SQRDMLAH_SCALAR_BY_ELEMENT:
        switch (element_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x7E428420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x7E828420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQRDMLSH_SCALAR:
    case ARM64_INSN_SQRDMLSH_SCALAR_BY_ELEMENT:
        switch (element_width)
        {
        case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x7E428C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x7E828C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_permute_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_UZP1_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp1 v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp1 v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp1 v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp1 v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp1 v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp1 v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp1 v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_TRN1_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn1 v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn1 v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn1 v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn1 v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn1 v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn1 v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn1 v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_ZIP1_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip1 v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip1 v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip1 v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip1 v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip1 v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip1 v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip1 v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UZP2_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp2 v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp2 v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp2 v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp2 v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp2 v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp2 v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuzp2 v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_TRN2_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn2 v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn2 v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn2 v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn2 v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn2 v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn2 v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ntrn2 v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_ZIP2_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip2 v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip2 v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip2 v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip2 v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip2 v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip2 v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nzip2 v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_integer_accumulate_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SABA_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nsaba v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nsaba v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nsaba v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nsaba v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nsaba v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nsaba v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_MLA_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmla v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmla v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmla v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmla v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmla v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmla v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UABA_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nuaba v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nuaba v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nuaba v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nuaba v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nuaba v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nuaba v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_MLS_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmls v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmls v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmls v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmls v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmls v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nmls v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_saturating_add_sub_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SQADD_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQSUB_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQADD_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQSUB_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_integer_compare_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_CMGT_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmgt v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmgt v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmgt v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmgt v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmgt v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmgt v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmgt v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_CMGE_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmge v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmge v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmge v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmge v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmge v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmge v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmge v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_CMTST_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmtst v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmtst v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmtst v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmtst v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmtst v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmtst v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmtst v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_CMHI_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhi v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhi v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhi v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhi v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhi v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhi v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhi v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_CMHS_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhs v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhs v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhs v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhs v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhs v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhs v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhs v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_CMEQ_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmeq v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmeq v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmeq v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmeq v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmeq v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmeq v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmeq v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_variable_shift_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SSHL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsshl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsshl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsshl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsshl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsshl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsshl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsshl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQSHL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SRSHL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrshl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrshl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrshl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrshl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrshl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrshl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrshl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQRSHL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_USHL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nushl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nushl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nushl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nushl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nushl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nushl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nushl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQSHL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_URSHL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurshl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurshl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurshl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurshl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurshl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurshl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurshl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQRSHL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_integer_add_sub_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_ADD_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nadd v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nadd v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nadd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nadd v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nadd v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nadd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nadd v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_ADDP_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\naddp v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\naddp v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\naddp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\naddp v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\naddp v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\naddp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\naddp v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SUB_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsub v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsub v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsub v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsub v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsub v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsub v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsub v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_halving_add_sub_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SHADD_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshadd v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshadd v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshadd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshadd v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshadd v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshadd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SRHADD_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrhadd v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrhadd v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrhadd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrhadd v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrhadd v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrhadd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SHSUB_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshsub v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshsub v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshsub v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshsub v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshsub v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nshsub v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UHADD_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhadd v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhadd v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhadd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhadd v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhadd v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhadd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_URHADD_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurhadd v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurhadd v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurhadd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurhadd v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurhadd v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurhadd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UHSUB_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhsub v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhsub v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhsub v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhsub v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhsub v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuhsub v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_min_max_absdiff_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SMAX_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmax v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmax v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmax v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmax v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmax v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmax v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SMIN_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmin v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmin v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmin v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmin v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmin v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmin v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SABD_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsabd v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsabd v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsabd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsabd v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsabd v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsabd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UMAX_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numax v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numax v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numax v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numax v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numax v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numax v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UMIN_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numin v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numin v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numin v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numin v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numin v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numin v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UABD_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuabd v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuabd v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuabd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuabd v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuabd v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuabd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_multiply_pairwise_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_MUL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nmul v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nmul v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nmul v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nmul v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nmul v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nmul v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SMAXP_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmaxp v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmaxp v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmaxp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmaxp v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmaxp v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsmaxp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SMINP_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsminp v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsminp v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsminp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsminp v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsminp v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsminp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UMAXP_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numaxp v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numaxp v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numaxp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numaxp v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numaxp v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numaxp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UMINP_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numinp v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numinp v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numinp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numinp v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numinp v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\numinp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_special_multiply_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SQDMULH_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqdmulh v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqdmulh v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqdmulh v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqdmulh v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQRDMULH_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrdmulh v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrdmulh v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrdmulh v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrdmulh v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_PMUL_VECTOR:
        switch (vector_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\npmul v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\npmul v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_dot_matrix_accumulate_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SDOT_VECTOR:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E829420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E829420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_USDOT_VECTOR:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E829C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E829C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_BFDOT_VECTOR:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E42FC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E42FC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UDOT_VECTOR:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E829420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E829420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SMMLA_VECTOR:
        if (vector_width != 128) return false;
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E82A420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_USMMLA_VECTOR:
        if (vector_width != 128) return false;
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E82AC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_BFMMLA_VECTOR:
        if (vector_width != 128) return false;
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E42EC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_UMMLA_VECTOR:
        if (vector_width != 128) return false;
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E82A420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_saturating_scalar_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SQADD_SCALAR:
        switch (element_width)
        {
        case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd b0, b1, b2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqadd d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQSUB_SCALAR:
        switch (element_width)
        {
        case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub b0, b1, b2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqsub d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQSHL_SCALAR:
        switch (element_width)
        {
        case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl b0, b1, b2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqshl d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQRSHL_SCALAR:
        switch (element_width)
        {
        case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl b0, b1, b2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrshl d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQADD_SCALAR:
        switch (element_width)
        {
        case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd b0, b1, b2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqadd d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQSUB_SCALAR:
        switch (element_width)
        {
        case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub b0, b1, b2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqsub d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQSHL_SCALAR:
        switch (element_width)
        {
        case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl b0, b1, b2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqshl d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQRSHL_SCALAR:
        switch (element_width)
        {
        case 8: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl b0, b1, b2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nuqrshl d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_integer_scalar_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SQDMULH_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqdmulh h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqdmulh s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_CMGT_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmgt d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_CMGE_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmge d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_SSHL_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsshl d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_SRSHL_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsrshl d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_ADD_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\nadd d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_CMTST_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmtst d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_SQRDMULH_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrdmulh h0, h1, h2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsqrdmulh s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_CMHI_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhi d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_CMHS_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmhs d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_USHL_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\nushl d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_URSHL_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\nurshl d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_SUB_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\nsub d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_CMEQ_SCALAR:
        if (element_width != 64) return false;
        asm volatile("ldr q1, [%1]\nldr q2, [%2]\ncmeq d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fp_scalar_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FMULX_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x5E421C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMEQ_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x5E422420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmeq s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmeq d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FRECPS_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x5E423C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrecps s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrecps d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FRSQRTS_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x5EC23C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrsqrts s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrsqrts d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMGE_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x7E422420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmge s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmge d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FACGE_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x7E422C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacge s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacge d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FABD_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x7EC21420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfabd s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfabd d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMGT_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x7EC22420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmgt s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmgt d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FACGT_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x7EC22C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacgt s0, s1, s2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacgt d0, d1, d2\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_indexed_dot_accumulate_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *element, uint32_t vector_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SUDOT_VECTOR_BY_ELEMENT:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0F02F020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4F02F020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_BFDOT_VECTOR_BY_ELEMENT:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0F42F020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4F42F020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SDOT_VECTOR_BY_ELEMENT:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0F82E020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4F82E020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_USDOT_VECTOR_BY_ELEMENT:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0F82F020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4F82F020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UDOT_VECTOR_BY_ELEMENT:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2F82E020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F82E020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fhm_accumulate_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t vector_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FMLAL_VECTOR:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E22EC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E22EC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLSL_VECTOR:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0EA2EC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EA2EC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLAL_VECTOR_BY_ELEMENT:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0F820020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4F820020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLSL_VECTOR_BY_ELEMENT:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0F824020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4F824020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLAL2_VECTOR:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E22CC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E22CC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLSL2_VECTOR:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EA2CC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EA2CC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLAL2_VECTOR_BY_ELEMENT:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2F828020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F828020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLSL2_VECTOR_BY_ELEMENT:
        switch (vector_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2F82C020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F82C020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fcmla_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t operand_width, uint32_t element_width, uint32_t rotation)
{
    switch (instruction)
    {
    case ARM64_INSN_FCMLA_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16:
                switch (rotation)
                {
                case ARM64_SIMD_ROTATION_0: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E42C420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_90: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E42CC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_180: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E42D420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_270: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E42DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                default: return false;
                }
                break;
            case 32:
                switch (rotation)
                {
                case ARM64_SIMD_ROTATION_0: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E82C420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_90: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E82CC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_180: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E82D420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_270: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E82DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                default: return false;
                }
                break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16:
                switch (rotation)
                {
                case ARM64_SIMD_ROTATION_0: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E42C420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_90: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E42CC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_180: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E42D420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_270: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E42DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                default: return false;
                }
                break;
            case 32:
                switch (rotation)
                {
                case ARM64_SIMD_ROTATION_0: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E82C420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_90: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E82CC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_180: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E82D420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_270: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E82DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                default: return false;
                }
                break;
            case 64:
                switch (rotation)
                {
                case ARM64_SIMD_ROTATION_0: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC2C420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_90: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC2CC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_180: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC2D420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_270: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC2DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                default: return false;
                }
                break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMLA_VECTOR_BY_ELEMENT:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16:
                switch (rotation)
                {
                case ARM64_SIMD_ROTATION_0: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2F421020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_90: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2F423020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_180: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2F425020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_270: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2F427020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                default: return false;
                }
                break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16:
                switch (rotation)
                {
                case ARM64_SIMD_ROTATION_0: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F421020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_90: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F423020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_180: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F425020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_270: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F427020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                default: return false;
                }
                break;
            case 32:
                switch (rotation)
                {
                case ARM64_SIMD_ROTATION_0: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F821020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_90: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F823020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_180: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F825020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                case ARM64_SIMD_ROTATION_270: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6F827020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
                default: return false;
                }
                break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fcadd_hw(void *dst, const void *left, const void *right, uint32_t operand_width, uint32_t element_width, uint32_t rotation)
{
    switch (operand_width)
    {
    case 64:
        switch (element_width)
        {
        case 16:
            switch (rotation)
            {
            case ARM64_SIMD_ROTATION_90: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E42E420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case ARM64_SIMD_ROTATION_270: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E42F420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 32:
            switch (rotation)
            {
            case ARM64_SIMD_ROTATION_90: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E82E420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case ARM64_SIMD_ROTATION_270: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E82F420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    case 128:
        switch (element_width)
        {
        case 16:
            switch (rotation)
            {
            case ARM64_SIMD_ROTATION_90: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E42E420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case ARM64_SIMD_ROTATION_270: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E42F420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 32:
            switch (rotation)
            {
            case ARM64_SIMD_ROTATION_90: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E82E420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case ARM64_SIMD_ROTATION_270: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E82F420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 64:
            switch (rotation)
            {
            case ARM64_SIMD_ROTATION_90: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC2E420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case ARM64_SIMD_ROTATION_270: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC2F420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    default: return false;
    }
    return true;
}
// clang-format on

static inline bool emu_simd_fcma_by_element_hw(void *dst, const void *left, const void *right, uint32_t element_width, uint32_t operand_width, uint32_t lane_index, uint32_t rotation)
{
    __uint128_t element;
    uint64_t lane_value;
    uint32_t complex_width = element_width * 2;

    if (!arm64_current_cpu_has_fcma()) return false;
    if (element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
    if ((element_width != 16 && element_width != 32) || !emu_simd_extract_lane_hw(right, complex_width, lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, complex_width)) return false;

    return emu_simd_fcmla_hw(ARM64_INSN_FCMLA_VECTOR_BY_ELEMENT, dst, left, &element, operand_width, element_width, rotation);
}

// clang-format off
static inline bool emu_simd_fp_by_element_accumulate_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *element, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FMLA_VECTOR_BY_ELEMENT:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E420C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmla v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E420C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmla v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmla v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLS_VECTOR_BY_ELEMENT:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0EC20C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmls v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EC20C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmls v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmls v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLA_SCALAR_BY_ELEMENT:
        switch (operand_width)
        {
        case 16:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x5F021020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmla s0, s1, v2.s[0]\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmla d0, d1, v2.d[0]\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLS_SCALAR_BY_ELEMENT:
        switch (operand_width)
        {
        case 16:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x5F025020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmls s0, s1, v2.s[0]\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmls d0, d1, v2.d[0]\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fp_by_element_multiply_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *element, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FMUL_VECTOR_BY_ELEMENT:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E421C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmul v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E421C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmul v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmul v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMULX_VECTOR_BY_ELEMENT:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E421C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E421C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMUL_SCALAR_BY_ELEMENT:
        switch (operand_width)
        {
        case 16:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x5F029020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmul s0, s1, v2.s[0]\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmul d0, d1, v2.d[0]\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMULX_SCALAR_BY_ELEMENT:
        switch (operand_width)
        {
        case 16:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x7F029020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx s0, s1, v2.s[0]\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx d0, d1, v2.d[0]\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(element) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_logical_immediate_hw(enum arm64_instruction instruction, void *dst, const void *immediate, uint32_t operand_width)
{
    switch (instruction)
    {
    case ARM64_INSN_ORR_VECTOR_IMMEDIATE:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\norr v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(dst), "r"(immediate) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q1, [%1]\nldr q2, [%2]\norr v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(dst), "r"(immediate) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_BIC_VECTOR_IMMEDIATE:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nbic v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(dst), "r"(immediate) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nbic v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(dst), "r"(immediate) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_bfmlal_accumulate_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t operand_width, uint32_t element_width)
{
    if (operand_width != 128 || element_width != 16) return false;

    switch (instruction)
    {
    case ARM64_INSN_BFMLALB_VECTOR_BY_ELEMENT:
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0FC2F020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_BFMLALB_VECTOR:
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EC2FC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_BFMLALT_VECTOR_BY_ELEMENT:
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4FC2F020\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case ARM64_INSN_BFMLALT_VECTOR:
        asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC2FC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_reverse_vector_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_REV64_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nrev64 v0.8b, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nrev64 v0.4h, v1.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nrev64 v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nrev64 v0.16b, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nrev64 v0.8h, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nrev64 v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_REV16_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nrev16 v0.8b, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nrev16 v0.16b, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_REV32_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nrev32 v0.8b, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nrev32 v0.4h, v1.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nrev32 v0.16b, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nrev32 v0.8h, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_narrow_vector_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_XTN_VECTOR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nxtn v0.8b, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q1, [%1]\nxtn v0.4h, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q1, [%1]\nxtn v0.2s, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQXTN_VECTOR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nsqxtn v0.8b, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q1, [%1]\nsqxtn v0.4h, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q1, [%1]\nsqxtn v0.2s, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQXTUN_VECTOR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nsqxtun v0.8b, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q1, [%1]\nsqxtun v0.4h, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q1, [%1]\nsqxtun v0.2s, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQXTN_VECTOR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nuqxtn v0.8b, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q1, [%1]\nuqxtn v0.4h, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q1, [%1]\nuqxtn v0.2s, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fp_reduce_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FMAXNMV_SCALAR_REDUCE:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x0E30C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x4E30C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfmaxnmv s0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMAXV_SCALAR_REDUCE:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x0E30F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x4E30F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfmaxv s0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMINNMV_SCALAR_REDUCE:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x0EB0C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x4EB0C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfminnmv s0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMINV_SCALAR_REDUCE:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x0EB0F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x4EB0F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfminv s0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FADDP_SCALAR_REDUCE:
        switch (operand_width)
        {
        case 32:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x5E30D820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfaddp s0, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 64: asm volatile("ldr q1, [%1]\nfaddp d0, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_mvn_immediate_hw(void *dst, const void *immediate, uint32_t operand_width)
{
    switch (operand_width)
    {
    case 64: asm volatile("ldr q1, [%1]\nmvn v0.8b, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(immediate) : "memory", "v0", "v1"); break;
    case 128: asm volatile("ldr q1, [%1]\nmvn v0.16b, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(immediate) : "memory", "v0", "v1"); break;
    default: return false;
    }
    return true;
}

static inline bool emu_simd_logical_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t operand_width)
{
    switch (instruction)
    {
    case ARM64_INSN_AND_VECTOR:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nand v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nand v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_BIC_VECTOR:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nbic v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nbic v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_ORR_VECTOR:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\norr v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q1, [%1]\nldr q2, [%2]\norr v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_ORN_VECTOR:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\norn v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q1, [%1]\nldr q2, [%2]\norn v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_EOR_VECTOR:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\neor v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q1, [%1]\nldr q2, [%2]\neor v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_BSL_VECTOR:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nbsl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nbsl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_BIT_VECTOR:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nbit v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nbit v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_BIF_VECTOR:
        switch (operand_width)
        {
        case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nbif v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        case 128: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nbif v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fp_binary_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FMAXNM_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E420420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmaxnm v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E420420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmaxnm v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmaxnm v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FADD_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E421420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfadd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E421420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfadd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfadd v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMULX_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E421C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E421C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmulx v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMEQ_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E422420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmeq v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E422420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmeq v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmeq v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMAX_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E423420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmax v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E423420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmax v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmax v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FRECPS_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E423C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrecps v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E423C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrecps v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrecps v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMINNM_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0EC20420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfminnm v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EC20420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfminnm v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfminnm v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FSUB_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0EC21420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfsub v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EC21420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfsub v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfsub v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMIN_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0EC23420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmin v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EC23420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmin v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmin v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FRSQRTS_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0EC23C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrsqrts v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EC23C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrsqrts v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfrsqrts v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMAXNMP_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E420420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmaxnmp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E420420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmaxnmp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmaxnmp v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FADDP_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E421420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfaddp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E421420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfaddp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfaddp v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMUL_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E421C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmul v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E421C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmul v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmul v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMGE_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E422420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmge v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E422420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmge v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmge v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FACGE_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E422C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacge v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E422C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacge v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacge v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMAXP_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E423420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmaxp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E423420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmaxp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfmaxp v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FDIV_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2E423C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfdiv v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6E423C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfdiv v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfdiv v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMINNMP_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EC20420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfminnmp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC20420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfminnmp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfminnmp v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FABD_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EC21420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfabd v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC21420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfabd v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfabd v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMGT_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EC22420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmgt v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC22420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmgt v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfcmgt v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FACGT_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EC22C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacgt v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC22C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacgt v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfacgt v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMINP_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EC23420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfminp v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC23420\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfminp v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\nfminp v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fp_accumulate_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FMLA_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0E420C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmla v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4E420C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmla v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmla v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FMLS_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x0EC20C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmls v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EC20C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmls v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q0, [%0]\nldr q1, [%1]\nldr q2, [%2]\nfmls v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fp_special_vector_hw(enum arm64_instruction instruction, void *dst, const void *left, const void *right, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FAMAX_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0EC21C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x0EA2DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EC21C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EA2DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x4EE2DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FAMIN_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EC21C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EA2DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC21C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EA2DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EE2DC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FSCALE_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EC23C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x2EA2FC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EC23C20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EA2FC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\nldr q2, [%2]\n.inst 0x6EE2FC20\nstr q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fp_unary_vector_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FABS_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x0EF8F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfabs v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x4EF8F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfabs v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfabs v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FNEG_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x2EF8F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfneg v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x6EF8F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfneg v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfneg v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FSQRT_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x2EF9F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfsqrt v0.2s, v1.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x6EF9F820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfsqrt v0.4s, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfsqrt v0.2d, v1.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_integer_reduce_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SADDLV_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nsaddlv h0, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nsaddlv s0, v1.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nsaddlv h0, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nsaddlv s0, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nsaddlv d0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SMAXV_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nsmaxv b0, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nsmaxv h0, v1.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nsmaxv b0, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nsmaxv h0, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nsmaxv s0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SMINV_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nsminv b0, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nsminv h0, v1.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nsminv b0, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nsminv h0, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nsminv s0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_ADDV_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\naddv b0, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\naddv h0, v1.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\naddv b0, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\naddv h0, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\naddv s0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UADDLV_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nuaddlv h0, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nuaddlv s0, v1.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\nuaddlv h0, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\nuaddlv s0, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nuaddlv d0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UMAXV_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\numaxv b0, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\numaxv h0, v1.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\numaxv b0, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\numaxv h0, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\numaxv s0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UMINV_VECTOR:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\numinv b0, v1.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\numinv h0, v1.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\numinv b0, v1.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\numinv h0, v1.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\numinv s0, v1.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_narrow_scalar_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_SQXTN_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nsqxtn b0, h1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q1, [%1]\nsqxtn h0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q1, [%1]\nsqxtn s0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SQXTUN_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nsqxtun b0, h1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q1, [%1]\nsqxtun h0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q1, [%1]\nsqxtun s0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_UQXTN_SCALAR:
        switch (element_width)
        {
        case 16: asm volatile("ldr q1, [%1]\nuqxtn b0, h1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 32: asm volatile("ldr q1, [%1]\nuqxtn h0, s1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        case 64: asm volatile("ldr q1, [%1]\nuqxtn s0, d1\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_shift_immediate_vector_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t operand_width, uint32_t element_width, uint64_t shift_amount)
{
    switch (instruction)
    {
    case ARM64_INSN_SSHR_VECTOR_IMMEDIATE:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\ndup v2.8b, %w2\nsshl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\ndup v2.4h, %w2\nsshl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\ndup v2.2s, %w2\nsshl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\ndup v2.16b, %w2\nsshl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\ndup v2.8h, %w2\nsshl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\ndup v2.4s, %w2\nsshl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\ndup v2.2d, %2\nsshl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_SHL_VECTOR_IMMEDIATE:
    case ARM64_INSN_USHR_VECTOR_IMMEDIATE:
        switch (operand_width)
        {
        case 64:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\ndup v2.8b, %w2\nushl v0.8b, v1.8b, v2.8b\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\ndup v2.4h, %w2\nushl v0.4h, v1.4h, v2.4h\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\ndup v2.2s, %w2\nushl v0.2s, v1.2s, v2.2s\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 8: asm volatile("ldr q1, [%1]\ndup v2.16b, %w2\nushl v0.16b, v1.16b, v2.16b\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 16: asm volatile("ldr q1, [%1]\ndup v2.8h, %w2\nushl v0.8h, v1.8h, v2.8h\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 32: asm volatile("ldr q1, [%1]\ndup v2.4s, %w2\nushl v0.4s, v1.4s, v2.4s\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            case 64: asm volatile("ldr q1, [%1]\ndup v2.2d, %2\nushl v0.2d, v1.2d, v2.2d\nstr q0, [%0]\n" : : "r"(dst), "r"(source), "r"(shift_amount) : "memory", "v0", "v1", "v2"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_ext_hw(void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t byte_offset)
{
    if (vector_width == 64)
    {
        switch (byte_offset)
        {
        case 0:
            asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.8b, v1.8b, v2.8b, #0\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
            return true;
        case 1:
            asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.8b, v1.8b, v2.8b, #1\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
            return true;
        case 2:
            asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.8b, v1.8b, v2.8b, #2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
            return true;
        case 3:
            asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.8b, v1.8b, v2.8b, #3\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
            return true;
        case 4:
            asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.8b, v1.8b, v2.8b, #4\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
            return true;
        case 5:
            asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.8b, v1.8b, v2.8b, #5\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
            return true;
        case 6:
            asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.8b, v1.8b, v2.8b, #6\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
            return true;
        case 7:
            asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.8b, v1.8b, v2.8b, #7\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
            return true;
        default:
            return false;
        }
    }

    if (vector_width != 128) return false;

    switch (byte_offset)
    {
    case 0:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #0\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 1:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #1\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 2:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #2\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 3:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #3\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 4:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #4\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 5:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #5\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 6:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #6\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 7:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #7\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 8:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #8\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 9:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #9\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 10:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #10\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 11:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #11\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 12:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #12\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 13:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #13\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 14:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #14\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    case 15:
        asm volatile("" "ldr q1, [%1]\nldr q2, [%2]\n" "ext v0.16b, v1.16b, v2.16b, #15\n" "str q0, [%0]\n" : : "r"(dst), "r"(left), "r"(right) : "memory", "v0", "v1", "v2");
        return true;
    default:
        return false;
    }
}

static inline bool emu_simd_fp_compare_zero_hw(enum arm64_instruction instruction, void *dst, const void *source, uint32_t operand_width, uint32_t element_width)
{
    switch (instruction)
    {
    case ARM64_INSN_FCMGT_ZERO_VECTOR:
    case ARM64_INSN_FCMGT_ZERO_SCALAR:
        switch (operand_width)
        {
        case 16:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x5EF8C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcmgt s0, s1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q1, [%1]\nfcmgt d0, d1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x0EF8C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmgt v0.2s, v1.2s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x4EF8C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmgt v0.4s, v1.4s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcmgt v0.2d, v1.2d, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMEQ_ZERO_VECTOR:
    case ARM64_INSN_FCMEQ_ZERO_SCALAR:
        switch (operand_width)
        {
        case 16:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x5EF8D820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcmeq s0, s1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q1, [%1]\nfcmeq d0, d1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x0EF8D820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmeq v0.2s, v1.2s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x4EF8D820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmeq v0.4s, v1.4s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcmeq v0.2d, v1.2d, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMLT_ZERO_VECTOR:
    case ARM64_INSN_FCMLT_ZERO_SCALAR:
        switch (operand_width)
        {
        case 16:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x5EF8E820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcmlt s0, s1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q1, [%1]\nfcmlt d0, d1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x0EF8E820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmlt v0.2s, v1.2s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x4EF8E820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmlt v0.4s, v1.4s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcmlt v0.2d, v1.2d, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMGE_ZERO_VECTOR:
    case ARM64_INSN_FCMGE_ZERO_SCALAR:
        switch (operand_width)
        {
        case 16:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x7EF8C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcmge s0, s1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q1, [%1]\nfcmge d0, d1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x2EF8C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmge v0.2s, v1.2s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x6EF8C820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmge v0.4s, v1.4s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcmge v0.2d, v1.2d, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    case ARM64_INSN_FCMLE_ZERO_VECTOR:
    case ARM64_INSN_FCMLE_ZERO_SCALAR:
        switch (operand_width)
        {
        case 16:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x7EF8D820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 32:
            switch (element_width)
            {
            case 32: asm volatile("ldr q1, [%1]\nfcmle s0, s1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 64:
            switch (element_width)
            {
            case 64: asm volatile("ldr q1, [%1]\nfcmle d0, d1, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x2EF8D820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmle v0.2s, v1.2s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        case 128:
            switch (element_width)
            {
            case 16: asm volatile("ldr q1, [%1]\n.inst 0x6EF8D820\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 32: asm volatile("ldr q1, [%1]\nfcmle v0.4s, v1.4s, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            case 64: asm volatile("ldr q1, [%1]\nfcmle v0.2d, v1.2d, #0.0\nstr q0, [%0]\n" : : "r"(dst), "r"(source) : "memory", "v0", "v1"); break;
            default: return false;
            }
            break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}
// clang-format on

/* ======================== FP / AdvSIMD：指令语义分派 ======================== */

static inline bool emu_execute_fp_simd_instruction(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_decoded_insn *decoded)
{
    __uint128_t element;
    uint64_t lane_value;
    uint64_t shift_amount;

    switch (decoded->instruction)
    {
    case ARM64_INSN_UZP1_VECTOR: // 0x0E001800
    {
        return emu_simd_permute_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_TRN1_VECTOR: // 0x0E002800
    {
        return emu_simd_permute_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_ZIP1_VECTOR: // 0x0E003800
    {
        return emu_simd_permute_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UZP2_VECTOR: // 0x0E005800
    {
        return emu_simd_permute_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_TRN2_VECTOR: // 0x0E006800
    {
        return emu_simd_permute_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_ZIP2_VECTOR: // 0x0E007800
    {
        return emu_simd_permute_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_DUP_ELEMENT_VECTOR: // 0x0E010400
    {
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rn], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (!emu_simd_dup_general_hw(&fp_regs->q[decoded->rd], lane_value, decoded->element_width, decoded->operand_width)) return false;
        return true;
    }
    case ARM64_INSN_DUP_GENERAL_VECTOR: // 0x0E010C00
    {
        if (!emu_simd_dup_general_hw(&fp_regs->q[decoded->rd], reg_read(regs, decoded->rn), decoded->element_width, decoded->operand_width)) return false;
        return true;
    }
    case ARM64_INSN_SMOV_VECTOR_TO_GPR: // 0x0E012C00
    {
        if (!emu_simd_extract_signed_lane_hw(&fp_regs->q[decoded->rn], decoded->element_width, decoded->lane_index, decoded->operand_width == 64, &lane_value)) return false;
        reg_write(regs, decoded->rd, lane_value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_UMOV_VECTOR_TO_GPR: // 0x0E013C00
    {
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rn], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        reg_write(regs, decoded->rd, lane_value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_SHADD_VECTOR: // 0x0E200400
    {
        return emu_simd_halving_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_REV64_VECTOR: // 0x0E200800
    {
        return emu_simd_reverse_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQADD_VECTOR: // 0x0E200C00
    {
        return emu_simd_saturating_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SRHADD_VECTOR: // 0x0E201400
    {
        return emu_simd_halving_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_REV16_VECTOR: // 0x0E201800
    {
        return emu_simd_reverse_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_AND_VECTOR: // 0x0E201C00
    {
        return emu_simd_logical_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_SHSUB_VECTOR: // 0x0E202400
    {
        return emu_simd_halving_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQSUB_VECTOR: // 0x0E202C00
    {
        return emu_simd_saturating_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_CMGT_VECTOR: // 0x0E203400
    {
        return emu_simd_integer_compare_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_CMGE_VECTOR: // 0x0E203C00
    {
        return emu_simd_integer_compare_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SSHL_VECTOR: // 0x0E204400
    {
        return emu_simd_variable_shift_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQSHL_VECTOR: // 0x0E204C00
    {
        return emu_simd_variable_shift_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SRSHL_VECTOR: // 0x0E205400
    {
        return emu_simd_variable_shift_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQRSHL_VECTOR: // 0x0E205C00
    {
        return emu_simd_variable_shift_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SMAX_VECTOR: // 0x0E206400
    {
        return emu_simd_min_max_absdiff_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SMIN_VECTOR: // 0x0E206C00
    {
        return emu_simd_min_max_absdiff_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SABD_VECTOR: // 0x0E207400
    {
        return emu_simd_min_max_absdiff_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SABA_VECTOR: // 0x0E207C00
    {
        return emu_simd_integer_accumulate_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_ADD_VECTOR: // 0x0E208400
    {
        return emu_simd_integer_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_CMTST_VECTOR: // 0x0E208C00
    {
        return emu_simd_integer_compare_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_MLA_VECTOR: // 0x0E209400
    {
        return emu_simd_integer_accumulate_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_MUL_VECTOR: // 0x0E209C00
    {
        return emu_simd_multiply_pairwise_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SMAXP_VECTOR: // 0x0E20A400
    {
        return emu_simd_multiply_pairwise_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SMINP_VECTOR: // 0x0E20AC00
    {
        return emu_simd_multiply_pairwise_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_ADDP_VECTOR: // 0x0E20BC00
    {
        return emu_simd_integer_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMAXNM_VECTOR: // 0x0E20C400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMLA_VECTOR: // 0x0E20CC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_accumulate_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FADD_VECTOR: // 0x0E20D400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMULX_VECTOR: // 0x0E20DC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMEQ_VECTOR: // 0x0E20E400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMLAL_VECTOR: // 0x0E20EC00
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_fhm()) return false;
        return emu_simd_fhm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FMAX_VECTOR: // 0x0E20F400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FRECPS_VECTOR: // 0x0E20FC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_XTN_VECTOR: // 0x0E212800
    {
        return emu_simd_narrow_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_SQXTN_VECTOR: // 0x0E214800
    {
        return emu_simd_narrow_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_FCVTNS_SIMD_VECTOR: // 0x0E21A800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTMS_SIMD_VECTOR: // 0x0E21B800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTAS_SIMD_VECTOR: // 0x0E21C800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SCVTF_SIMD_VECTOR: // 0x0E21D800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SADDLV_VECTOR: // 0x0E303800
    {
        return emu_simd_integer_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SMAXV_VECTOR: // 0x0E30A800
    {
        return emu_simd_integer_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMAXNMV_SCALAR_REDUCE: // 0x0E30C800
    {
        return emu_simd_fp_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMAXV_SCALAR_REDUCE: // 0x0E30F800
    {
        return emu_simd_fp_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SMINV_VECTOR: // 0x0E31A800
    {
        return emu_simd_integer_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_ADDV_VECTOR: // 0x0E31B800
    {
        return emu_simd_integer_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_BIC_VECTOR: // 0x0E601C00
    {
        return emu_simd_logical_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_SQDMULH_VECTOR: // 0x0E60B400
    {
        return emu_simd_special_multiply_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SDOT_VECTOR: // 0x0E809400
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_dotprod()) return false;
        return emu_simd_dot_matrix_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_USDOT_VECTOR: // 0x0E809C00
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_i8mm()) return false;
        return emu_simd_dot_matrix_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_ORR_VECTOR: // 0x0EA01C00
    {
        return emu_simd_logical_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FMINNM_VECTOR: // 0x0EA0C400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMGT_ZERO_VECTOR: // 0x0EA0C800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMLS_VECTOR: // 0x0EA0CC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_accumulate_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FSUB_VECTOR: // 0x0EA0D400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMEQ_ZERO_VECTOR: // 0x0EA0D800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FAMAX_VECTOR: // 0x0EA0DC00
    {
        if (!arm64_current_cpu_has_faminmax()) return false;
        return emu_simd_fp_special_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMLT_ZERO_VECTOR: // 0x0EA0E800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMLSL_VECTOR: // 0x0EA0EC00
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_fhm()) return false;
        return emu_simd_fhm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FMIN_VECTOR: // 0x0EA0F400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FABS_VECTOR: // 0x0EA0F800
    {
        return emu_simd_fp_unary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FRSQRTS_VECTOR: // 0x0EA0FC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTPS_SIMD_VECTOR: // 0x0EA1A800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTZS_SIMD_VECTOR: // 0x0EA1B800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMINNMV_SCALAR_REDUCE: // 0x0EB0C800
    {
        return emu_simd_fp_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMINV_SCALAR_REDUCE: // 0x0EB0F800
    {
        return emu_simd_fp_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_ORN_VECTOR: // 0x0EE01C00
    {
        return emu_simd_logical_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_MOVI_VECTOR_IMMEDIATE: // 0x0F000400
    {
        if (!emu_simd_materialize_bits_hw(&fp_regs->q[decoded->rd], decoded->expanded_immediate, decoded->operand_width)) return false;
        return true;
    }
    case ARM64_INSN_FMLA_VECTOR_BY_ELEMENT: // 0x0F001000
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (decoded->operand_width == decoded->element_width)
        {
            if (!emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        }
        else if (!emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_fp_by_element_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_ORR_VECTOR_IMMEDIATE: // 0x0F001400
    {
        __uint128_t immediate;

        if (!emu_simd_materialize_bits_hw(&immediate, decoded->expanded_immediate, decoded->operand_width)) return false;
        return emu_simd_logical_immediate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &immediate, decoded->operand_width);
    }
    case ARM64_INSN_FMLS_VECTOR_BY_ELEMENT: // 0x0F005000
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (decoded->operand_width == decoded->element_width)
        {
            if (!emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        }
        else if (!emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_fp_by_element_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMUL_VECTOR_BY_ELEMENT: // 0x0F009000
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (decoded->operand_width == decoded->element_width)
        {
            if (!emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        }
        else if (!emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_fp_by_element_multiply_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SUDOT_VECTOR_BY_ELEMENT: // 0x0F00F000
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_i8mm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 32, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        return emu_simd_indexed_dot_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width);
    }
    case ARM64_INSN_FMOV_VECTOR_IMMEDIATE: // 0x0F00F400
    {
        if (!emu_simd_materialize_bits_hw(&fp_regs->q[decoded->rd], decoded->expanded_immediate, decoded->operand_width)) return false;
        return true;
    }
    case ARM64_INSN_SSHR_VECTOR_IMMEDIATE: // 0x0F080400
    {
        shift_amount = (uint64_t)-(int64_t)decoded->immediate;
        return emu_simd_shift_immediate_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, shift_amount);
    }
    case ARM64_INSN_SHL_VECTOR_IMMEDIATE: // 0x0F085400
    {
        shift_amount = decoded->immediate;
        return emu_simd_shift_immediate_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, shift_amount);
    }
    case ARM64_INSN_BFDOT_VECTOR_BY_ELEMENT: // 0x0F40F000
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_bf16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 32, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        return emu_simd_indexed_dot_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width);
    }
    case ARM64_INSN_FMLAL_VECTOR_BY_ELEMENT: // 0x0F800000
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_fhm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 16, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 16)) return false;
        return emu_simd_fhm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width);
    }
    case ARM64_INSN_FMLSL_VECTOR_BY_ELEMENT: // 0x0F804000
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_fhm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 16, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 16)) return false;
        return emu_simd_fhm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width);
    }
    case ARM64_INSN_SDOT_VECTOR_BY_ELEMENT: // 0x0F80E000
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_dotprod()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 32, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        return emu_simd_indexed_dot_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width);
    }
    case ARM64_INSN_USDOT_VECTOR_BY_ELEMENT: // 0x0F80F000
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_i8mm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 32, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        return emu_simd_indexed_dot_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width);
    }
    case ARM64_INSN_BFMLALB_VECTOR_BY_ELEMENT: // 0x0FC0F000
    {
        if (decoded->operand_width != 128 || decoded->element_width != 16 || !arm64_current_cpu_has_bf16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 16, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 16)) return false;
        return emu_simd_bfmlal_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTNS_GPR: // 0x1E200000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FCCMP_SCALAR: // 0x1E200400
    {
        uint64_t nzcv;

        if (decoded->operand_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_cond_holds(emu_read_nzcv(regs), decoded->condition))
        {
            emu_write_nzcv(regs, (uint64_t)decoded->immediate << 28);
            return true;
        }
        if (!emu_scalar_fp_compare_register_hw(decoded->operand_width, &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &nzcv)) return false;
        emu_write_nzcv(regs, nzcv);
        return true;
    }
    case ARM64_INSN_FCCMPE_SCALAR: // 0x1E200410
    {
        uint64_t nzcv;

        if (decoded->operand_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_cond_holds(emu_read_nzcv(regs), decoded->condition))
        {
            emu_write_nzcv(regs, (uint64_t)decoded->immediate << 28);
            return true;
        }
        if (!emu_scalar_fp_compare_register_signaling_hw(decoded->operand_width, &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &nzcv)) return false;
        emu_write_nzcv(regs, nzcv);
        return true;
    }
    case ARM64_INSN_FMUL_SCALAR: // 0x1E200800
    {
        return emu_fp_scalar_binary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FCSEL_SCALAR: // 0x1E200C00
    {
        if (decoded->operand_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_fp_select_hw(&fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], emu_read_nzcv(regs), decoded->condition, decoded->operand_width)) return false;
        return true;
    }
    case ARM64_INSN_FMOV_SCALAR_IMMEDIATE: // 0x1E201000
    {
        if (decoded->operand_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_simd_write_scalar_hw(&fp_regs->q[decoded->rd], decoded->expanded_immediate, decoded->operand_width)) return false;
        return true;
    }
    case ARM64_INSN_FDIV_SCALAR: // 0x1E201800
    {
        return emu_fp_scalar_binary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FCMP_REGISTER_SCALAR: // 0x1E202000
    {
        uint64_t nzcv;

        if (!emu_scalar_fp_compare_register_hw(decoded->operand_width, &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &nzcv)) return false;
        emu_write_nzcv(regs, nzcv);
        return true;
    }
    case ARM64_INSN_FCMP_ZERO_SCALAR: // 0x1E202008
    {
        uint64_t nzcv;

        if (!emu_scalar_fp_compare_zero_hw(decoded->operand_width, &fp_regs->q[decoded->rn], &nzcv)) return false;
        emu_write_nzcv(regs, nzcv);
        return true;
    }
    case ARM64_INSN_FCMPE_REGISTER_SCALAR: // 0x1E202010
    {
        uint64_t nzcv;

        if (!emu_scalar_fp_compare_register_signaling_hw(decoded->operand_width, &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &nzcv)) return false;
        emu_write_nzcv(regs, nzcv);
        return true;
    }
    case ARM64_INSN_FCMPE_ZERO_SCALAR: // 0x1E202018
    {
        uint64_t nzcv;

        if (!emu_scalar_fp_compare_zero_signaling_hw(decoded->operand_width, &fp_regs->q[decoded->rn], &nzcv)) return false;
        emu_write_nzcv(regs, nzcv);
        return true;
    }
    case ARM64_INSN_FADD_SCALAR: // 0x1E202800
    {
        return emu_fp_scalar_binary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FSUB_SCALAR: // 0x1E203800
    {
        return emu_fp_scalar_binary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FMOV_SCALAR: // 0x1E204000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FMAX_SCALAR: // 0x1E204800
    {
        return emu_fp_scalar_binary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FMIN_SCALAR: // 0x1E205800
    {
        return emu_fp_scalar_binary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FMAXNM_SCALAR: // 0x1E206800
    {
        return emu_fp_scalar_binary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FMINNM_SCALAR: // 0x1E207800
    {
        return emu_fp_scalar_binary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FNMUL_SCALAR: // 0x1E208800
    {
        return emu_fp_scalar_binary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FABS_SCALAR: // 0x1E20C000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FCVTNU_GPR: // 0x1E210000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FNEG_SCALAR: // 0x1E214000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FSQRT_SCALAR: // 0x1E21C000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_SCVTF_S_W: // 0x1E220000
    {
        return emu_gpr_to_fp_hw(decoded->instruction, &fp_regs->q[decoded->rd], reg_read(regs, decoded->rn));
    }
    case ARM64_INSN_FCVT_D_S: // 0x1E22C000
    {
        return emu_fp_unary_merge_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_UCVTF_S_W: // 0x1E230000
    {
        return emu_gpr_to_fp_hw(decoded->instruction, &fp_regs->q[decoded->rd], reg_read(regs, decoded->rn));
    }
    case ARM64_INSN_FCVTAS_GPR: // 0x1E240000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FRINTN_SCALAR: // 0x1E244000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FRINTP_SCALAR: // 0x1E24C000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FCVTAU_GPR: // 0x1E250000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FRINTM_SCALAR: // 0x1E254000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FRINTZ_SCALAR: // 0x1E25C000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FMOV_FP_TO_GPR: // 0x1E260000
    {
        uint64_t value;

        if (!emu_simd_read_scalar_hw(&fp_regs->q[decoded->rn], decoded->operand_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FRINTA_SCALAR: // 0x1E264000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FMOV_GPR_TO_FP: // 0x1E270000
    {
        if (!emu_simd_write_scalar_hw(&fp_regs->q[decoded->rd], reg_read(regs, decoded->rn), decoded->operand_width)) return false;
        return true;
    }
    case ARM64_INSN_FRINTX_SCALAR: // 0x1E274000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FRINTI_SCALAR: // 0x1E27C000
    {
        return emu_fp_scalar_unary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width);
    }
    case ARM64_INSN_FCVTPS_GPR: // 0x1E280000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FCVTPU_GPR: // 0x1E290000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FCVTMS_GPR: // 0x1E300000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FCVTMU_GPR: // 0x1E310000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FCVTZS_GPR: // 0x1E380000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_FCVTZU_GPR: // 0x1E390000
    {
        uint64_t value;

        if (!emu_fp_to_gpr_hw(decoded->instruction, &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, &value)) return false;
        reg_write(regs, decoded->rd, value, decoded->operand_width == 64);
        return true;
    }
    case ARM64_INSN_SCVTF_D_W: // 0x1E620000
    {
        return emu_gpr_to_fp_hw(decoded->instruction, &fp_regs->q[decoded->rd], reg_read(regs, decoded->rn));
    }
    case ARM64_INSN_FCVT_S_D: // 0x1E624000
    {
        return emu_fp_unary_merge_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_UCVTF_D_W: // 0x1E630000
    {
        return emu_gpr_to_fp_hw(decoded->instruction, &fp_regs->q[decoded->rd], reg_read(regs, decoded->rn));
    }
    case ARM64_INSN_FMADD_SCALAR: // 0x1F000000
    {
        return emu_fp_scalar_ternary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra], decoded->operand_width);
    }
    case ARM64_INSN_FMSUB_SCALAR: // 0x1F008000
    {
        return emu_fp_scalar_ternary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra], decoded->operand_width);
    }
    case ARM64_INSN_FNMADD_SCALAR: // 0x1F200000
    {
        return emu_fp_scalar_ternary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra], decoded->operand_width);
    }
    case ARM64_INSN_FNMSUB_SCALAR: // 0x1F208000
    {
        return emu_fp_scalar_ternary_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], &fp_regs->q[decoded->ra], decoded->operand_width);
    }
    case ARM64_INSN_EXT_VECTOR: // 0x2E000000
    {
        if (decoded->immediate >= decoded->operand_width / 8) return false;
        if (!emu_simd_ext_hw(&fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->immediate)) return false;
        return true;
    }
    case ARM64_INSN_UHADD_VECTOR: // 0x2E200400
    {
        return emu_simd_halving_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_REV32_VECTOR: // 0x2E200800
    {
        return emu_simd_reverse_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UQADD_VECTOR: // 0x2E200C00
    {
        return emu_simd_saturating_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_URHADD_VECTOR: // 0x2E201400
    {
        return emu_simd_halving_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_EOR_VECTOR: // 0x2E201C00
    {
        return emu_simd_logical_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_UHSUB_VECTOR: // 0x2E202400
    {
        return emu_simd_halving_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UQSUB_VECTOR: // 0x2E202C00
    {
        return emu_simd_saturating_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_CMHI_VECTOR: // 0x2E203400
    {
        return emu_simd_integer_compare_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_CMHS_VECTOR: // 0x2E203C00
    {
        return emu_simd_integer_compare_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_USHL_VECTOR: // 0x2E204400
    {
        return emu_simd_variable_shift_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UQSHL_VECTOR: // 0x2E204C00
    {
        return emu_simd_variable_shift_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_URSHL_VECTOR: // 0x2E205400
    {
        return emu_simd_variable_shift_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UQRSHL_VECTOR: // 0x2E205C00
    {
        return emu_simd_variable_shift_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UMAX_VECTOR: // 0x2E206400
    {
        return emu_simd_min_max_absdiff_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UMIN_VECTOR: // 0x2E206C00
    {
        return emu_simd_min_max_absdiff_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UABD_VECTOR: // 0x2E207400
    {
        return emu_simd_min_max_absdiff_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UABA_VECTOR: // 0x2E207C00
    {
        return emu_simd_integer_accumulate_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SUB_VECTOR: // 0x2E208400
    {
        return emu_simd_integer_add_sub_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_CMEQ_VECTOR: // 0x2E208C00
    {
        return emu_simd_integer_compare_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_MLS_VECTOR: // 0x2E209400
    {
        return emu_simd_integer_accumulate_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_PMUL_VECTOR: // 0x2E209C00
    {
        return emu_simd_special_multiply_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UMAXP_VECTOR: // 0x2E20A400
    {
        return emu_simd_multiply_pairwise_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UMINP_VECTOR: // 0x2E20AC00
    {
        return emu_simd_multiply_pairwise_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMAXNMP_VECTOR: // 0x2E20C400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMLAL2_VECTOR: // 0x2E20CC00
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_fhm()) return false;
        return emu_simd_fhm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FADDP_VECTOR: // 0x2E20D400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMUL_VECTOR: // 0x2E20DC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMGE_VECTOR: // 0x2E20E400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FACGE_VECTOR: // 0x2E20EC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMAXP_VECTOR: // 0x2E20F400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FDIV_VECTOR: // 0x2E20FC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQXTUN_VECTOR: // 0x2E212800
    {
        return emu_simd_narrow_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_UQXTN_VECTOR: // 0x2E214800
    {
        return emu_simd_narrow_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_FCVTNU_SIMD_VECTOR: // 0x2E21A800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTMU_SIMD_VECTOR: // 0x2E21B800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTAU_SIMD_VECTOR: // 0x2E21C800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UCVTF_SIMD_VECTOR: // 0x2E21D800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UADDLV_VECTOR: // 0x2E303800
    {
        return emu_simd_integer_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UMAXV_VECTOR: // 0x2E30A800
    {
        return emu_simd_integer_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UMINV_VECTOR: // 0x2E31A800
    {
        return emu_simd_integer_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQRDMLAH_VECTOR: // 0x2E408400
    {
        if (!arm64_current_cpu_has_rdm()) return false;
        return emu_simd_rdm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQRDMLSH_VECTOR: // 0x2E408C00
    {
        if (!arm64_current_cpu_has_rdm()) return false;
        return emu_simd_rdm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMLA_VECTOR: // 0x2E40C400
    {
        return emu_simd_fcmla_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width, (uint32_t)decoded->immediate);
    }
    case ARM64_INSN_FCADD_VECTOR: // 0x2E40E400
    {
        return emu_simd_fcadd_hw(&fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width, (uint32_t)decoded->immediate);
    }
    case ARM64_INSN_BFDOT_VECTOR: // 0x2E40FC00
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_bf16()) return false;
        return emu_simd_dot_matrix_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_BSL_VECTOR: // 0x2E601C00
    {
        return emu_simd_logical_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_SQRDMULH_VECTOR: // 0x2E60B400
    {
        return emu_simd_special_multiply_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UDOT_VECTOR: // 0x2E809400
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_dotprod()) return false;
        return emu_simd_dot_matrix_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_BIT_VECTOR: // 0x2EA01C00
    {
        return emu_simd_logical_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FMINNMP_VECTOR: // 0x2EA0C400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMGE_ZERO_VECTOR: // 0x2EA0C800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMLSL2_VECTOR: // 0x2EA0CC00
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_fhm()) return false;
        return emu_simd_fhm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_FABD_VECTOR: // 0x2EA0D400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMLE_ZERO_VECTOR: // 0x2EA0D800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FAMIN_VECTOR: // 0x2EA0DC00
    {
        if (!arm64_current_cpu_has_faminmax()) return false;
        return emu_simd_fp_special_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMGT_VECTOR: // 0x2EA0E400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FACGT_VECTOR: // 0x2EA0EC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMINP_VECTOR: // 0x2EA0F400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_binary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FNEG_VECTOR: // 0x2EA0F800
    {
        return emu_simd_fp_unary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FSCALE_VECTOR: // 0x2EA0FC00
    {
        if (!arm64_current_cpu_has_f8cvt()) return false;
        return emu_simd_fp_special_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTPU_SIMD_VECTOR: // 0x2EA1A800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTZU_SIMD_VECTOR: // 0x2EA1B800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FSQRT_VECTOR: // 0x2EA1F800
    {
        return emu_simd_fp_unary_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_BFMLALB_VECTOR: // 0x2EC0FC00
    {
        if (decoded->element_width != 16 || decoded->operand_width != 128 || !arm64_current_cpu_has_bf16()) return false;
        return emu_simd_bfmlal_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_BIF_VECTOR: // 0x2EE01C00
    {
        return emu_simd_logical_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_MVNI_VECTOR_IMMEDIATE: // 0x2F000400
    {
        __uint128_t immediate;

        if (!emu_simd_materialize_bits_hw(&immediate, decoded->expanded_immediate, decoded->operand_width)) return false;
        return emu_simd_mvn_immediate_hw(&fp_regs->q[decoded->rd], &immediate, decoded->operand_width);
    }
    case ARM64_INSN_BIC_VECTOR_IMMEDIATE: // 0x2F001400
    {
        __uint128_t immediate;

        if (!emu_simd_materialize_bits_hw(&immediate, decoded->expanded_immediate, decoded->operand_width)) return false;
        return emu_simd_logical_immediate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &immediate, decoded->operand_width);
    }
    case ARM64_INSN_FMULX_VECTOR_BY_ELEMENT: // 0x2F009000
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (decoded->operand_width == decoded->element_width)
        {
            if (!emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        }
        else if (!emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_fp_by_element_multiply_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_USHR_VECTOR_IMMEDIATE: // 0x2F080400
    {
        shift_amount = (uint64_t)-(int64_t)decoded->immediate;
        return emu_simd_shift_immediate_vector_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width, shift_amount);
    }
    case ARM64_INSN_FCMLA_VECTOR_BY_ELEMENT: // 0x2F401000
    {
        return emu_simd_fcma_by_element_hw(&fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width, decoded->operand_width, decoded->lane_index, (uint32_t)decoded->immediate);
    }
    case ARM64_INSN_SQRDMLAH_VECTOR_BY_ELEMENT: // 0x2F40D000
    {
        if ((decoded->element_width != 16 && decoded->element_width != 32) || (decoded->operand_width != 64 && decoded->operand_width != 128) || !arm64_current_cpu_has_rdm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value) || !emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_rdm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQRDMLSH_VECTOR_BY_ELEMENT: // 0x2F40F000
    {
        if ((decoded->element_width != 16 && decoded->element_width != 32) || (decoded->operand_width != 64 && decoded->operand_width != 128) || !arm64_current_cpu_has_rdm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value) || !emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_rdm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMLAL2_VECTOR_BY_ELEMENT: // 0x2F808000
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_fhm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 16, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 16)) return false;
        return emu_simd_fhm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width);
    }
    case ARM64_INSN_FMLSL2_VECTOR_BY_ELEMENT: // 0x2F80C000
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_fhm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 16, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 16)) return false;
        return emu_simd_fhm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width);
    }
    case ARM64_INSN_UDOT_VECTOR_BY_ELEMENT: // 0x2F80E000
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_dotprod()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 32, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 32)) return false;
        return emu_simd_indexed_dot_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width);
    }
    case ARM64_INSN_INS_GPR_VECTOR: // 0x4E011C00
    {
        if (!emu_simd_insert_general_hw(&fp_regs->q[decoded->rd], reg_read(regs, decoded->rn), decoded->element_width, decoded->lane_index)) return false;
        return true;
    }
    case ARM64_INSN_XTN2_VECTOR: // 0x4E212800
    {
        return emu_fp_unary_merge_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_SQXTN2_VECTOR: // 0x4E214800
    {
        return emu_fp_unary_merge_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_SMMLA_VECTOR: // 0x4E80A400
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_i8mm()) return false;
        return emu_simd_dot_matrix_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_USMMLA_VECTOR: // 0x4E80AC00
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_i8mm()) return false;
        return emu_simd_dot_matrix_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_BFMLALT_VECTOR_BY_ELEMENT: // 0x4FC0F000
    {
        if (decoded->operand_width != 128 || decoded->element_width != 16 || !arm64_current_cpu_has_bf16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], 16, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, 16)) return false;
        return emu_simd_bfmlal_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_DUP_ELEMENT_SCALAR: // 0x5E010400
    {
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rn], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (!emu_simd_write_scalar_hw(&fp_regs->q[decoded->rd], lane_value, decoded->operand_width)) return false;
        return true;
    }
    case ARM64_INSN_SQADD_SCALAR: // 0x5E200C00
    {
        return emu_simd_saturating_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_SQSUB_SCALAR: // 0x5E202C00
    {
        return emu_simd_saturating_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_SQSHL_SCALAR: // 0x5E204C00
    {
        return emu_simd_saturating_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_SQRSHL_SCALAR: // 0x5E205C00
    {
        return emu_simd_saturating_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FMULX_SCALAR: // 0x5E20DC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FCMEQ_SCALAR: // 0x5E20E400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FRECPS_SCALAR: // 0x5E20FC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_SQXTN_SCALAR: // 0x5E214800
    {
        return emu_simd_narrow_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_FCVTNS_SIMD_SCALAR: // 0x5E21A800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTMS_SIMD_SCALAR: // 0x5E21B800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTAS_SIMD_SCALAR: // 0x5E21C800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SCVTF_SIMD_SCALAR: // 0x5E21D800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FADDP_SCALAR_REDUCE: // 0x5E30D800
    {
        return emu_simd_fp_reduce_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQDMULH_SCALAR: // 0x5E60B400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FCMGT_ZERO_SCALAR: // 0x5EA0C800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMEQ_ZERO_SCALAR: // 0x5EA0D800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMLT_ZERO_SCALAR: // 0x5EA0E800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FRSQRTS_SCALAR: // 0x5EA0FC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FCVTPS_SIMD_SCALAR: // 0x5EA1A800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTZS_SIMD_SCALAR: // 0x5EA1B800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_CMGT_SCALAR: // 0x5EE03400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_CMGE_SCALAR: // 0x5EE03C00
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_SSHL_SCALAR: // 0x5EE04400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_SRSHL_SCALAR: // 0x5EE05400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_ADD_SCALAR: // 0x5EE08400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_CMTST_SCALAR: // 0x5EE08C00
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FMLA_SCALAR_BY_ELEMENT: // 0x5F001000
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (decoded->operand_width == decoded->element_width)
        {
            if (!emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        }
        else if (!emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_fp_by_element_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMLS_SCALAR_BY_ELEMENT: // 0x5F005000
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (decoded->operand_width == decoded->element_width)
        {
            if (!emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        }
        else if (!emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_fp_by_element_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FMUL_SCALAR_BY_ELEMENT: // 0x5F009000
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (decoded->operand_width == decoded->element_width)
        {
            if (!emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        }
        else if (!emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_fp_by_element_multiply_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_INS_ELEMENT_VECTOR: // 0x6E010400
    {
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rn], decoded->element_width, decoded->source_lane_index, &lane_value)) return false;
        if (!emu_simd_insert_general_hw(&fp_regs->q[decoded->rd], lane_value, decoded->element_width, decoded->lane_index)) return false;
        return true;
    }
    case ARM64_INSN_SQXTUN2_VECTOR: // 0x6E212800
    {
        return emu_fp_unary_merge_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_UQXTN2_VECTOR: // 0x6E214800
    {
        return emu_fp_unary_merge_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_BFMMLA_VECTOR: // 0x6E40EC00
    {
        if (decoded->element_width != 16 || !arm64_current_cpu_has_bf16()) return false;
        return emu_simd_dot_matrix_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_UMMLA_VECTOR: // 0x6E80A400
    {
        if (decoded->element_width != 8 || !arm64_current_cpu_has_i8mm()) return false;
        return emu_simd_dot_matrix_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width);
    }
    case ARM64_INSN_BFMLALT_VECTOR: // 0x6EC0FC00
    {
        if (decoded->element_width != 16 || decoded->operand_width != 128 || !arm64_current_cpu_has_bf16()) return false;
        return emu_simd_bfmlal_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UQADD_SCALAR: // 0x7E200C00
    {
        return emu_simd_saturating_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_UQSUB_SCALAR: // 0x7E202C00
    {
        return emu_simd_saturating_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_UQSHL_SCALAR: // 0x7E204C00
    {
        return emu_simd_saturating_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_UQRSHL_SCALAR: // 0x7E205C00
    {
        return emu_simd_saturating_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FCMGE_SCALAR: // 0x7E20E400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FACGE_SCALAR: // 0x7E20EC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_SQXTUN_SCALAR: // 0x7E212800
    {
        return emu_simd_narrow_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_UQXTN_SCALAR: // 0x7E214800
    {
        return emu_simd_narrow_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->element_width);
    }
    case ARM64_INSN_FCVTNU_SIMD_SCALAR: // 0x7E21A800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTMU_SIMD_SCALAR: // 0x7E21B800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTAU_SIMD_SCALAR: // 0x7E21C800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_UCVTF_SIMD_SCALAR: // 0x7E21D800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQRDMLAH_SCALAR: // 0x7E408400
    {
        if (!arm64_current_cpu_has_rdm()) return false;
        return emu_simd_rdm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQRDMLSH_SCALAR: // 0x7E408C00
    {
        if (!arm64_current_cpu_has_rdm()) return false;
        return emu_simd_rdm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQRDMULH_SCALAR: // 0x7E60B400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FCMGE_ZERO_SCALAR: // 0x7EA0C800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FABD_SCALAR: // 0x7EA0D400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FCMLE_ZERO_SCALAR: // 0x7EA0D800
    {
        return emu_simd_fp_compare_zero_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCMGT_SCALAR: // 0x7EA0E400
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FACGT_SCALAR: // 0x7EA0EC00
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        return emu_simd_fp_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FCVTPU_SIMD_SCALAR: // 0x7EA1A800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_FCVTZU_SIMD_SCALAR: // 0x7EA1B800
    {
        return emu_fp_convert_simd_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_CMHI_SCALAR: // 0x7EE03400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_CMHS_SCALAR: // 0x7EE03C00
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_USHL_SCALAR: // 0x7EE04400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_URSHL_SCALAR: // 0x7EE05400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_SUB_SCALAR: // 0x7EE08400
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_CMEQ_SCALAR: // 0x7EE08C00
    {
        return emu_simd_integer_scalar_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &fp_regs->q[decoded->rm], decoded->element_width);
    }
    case ARM64_INSN_FMULX_SCALAR_BY_ELEMENT: // 0x7F009000
    {
        if (decoded->element_width == 16 && !arm64_current_cpu_has_fp16()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value)) return false;
        if (decoded->operand_width == decoded->element_width)
        {
            if (!emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        }
        else if (!emu_simd_dup_general_hw(&element, lane_value, decoded->element_width, decoded->operand_width)) return false;
        return emu_simd_fp_by_element_multiply_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQRDMLAH_SCALAR_BY_ELEMENT: // 0x7F40D000
    {
        if ((decoded->element_width != 16 && decoded->element_width != 32) || decoded->operand_width != decoded->element_width || !arm64_current_cpu_has_rdm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        return emu_simd_rdm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SQRDMLSH_SCALAR_BY_ELEMENT: // 0x7F40F000
    {
        if ((decoded->element_width != 16 && decoded->element_width != 32) || decoded->operand_width != decoded->element_width || !arm64_current_cpu_has_rdm()) return false;
        if (!emu_simd_extract_lane_hw(&fp_regs->q[decoded->rm], decoded->element_width, decoded->lane_index, &lane_value) || !emu_simd_write_scalar_hw(&element, lane_value, decoded->element_width)) return false;
        return emu_simd_rdm_accumulate_hw(decoded->instruction, &fp_regs->q[decoded->rd], &fp_regs->q[decoded->rn], &element, decoded->operand_width, decoded->element_width);
    }
    case ARM64_INSN_SCVTF_S_X: // 0x9E220000
    {
        return emu_gpr_to_fp_hw(decoded->instruction, &fp_regs->q[decoded->rd], reg_read(regs, decoded->rn));
    }
    case ARM64_INSN_UCVTF_S_X: // 0x9E230000
    {
        return emu_gpr_to_fp_hw(decoded->instruction, &fp_regs->q[decoded->rd], reg_read(regs, decoded->rn));
    }
    case ARM64_INSN_SCVTF_D_X: // 0x9E620000
    {
        return emu_gpr_to_fp_hw(decoded->instruction, &fp_regs->q[decoded->rd], reg_read(regs, decoded->rn));
    }
    case ARM64_INSN_UCVTF_D_X: // 0x9E630000
    {
        return emu_gpr_to_fp_hw(decoded->instruction, &fp_regs->q[decoded->rd], reg_read(regs, decoded->rn));
    }
    default:
        return false;
    }
}

/* ======================== FP / AdvSIMD：现场提交与执行流程 ======================== */

static inline enum emu_insn_result emu_simulate_fp_simd_core(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_decoded_insn *decoded)
{
    uint64_t pc = regs->pc;

    if (!emu_execute_fp_simd_instruction(regs, fp_regs, decoded)) return EMU_INSN_SKIP;
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

static inline enum emu_insn_result emu_simulate_fp_simd_insn(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_decoded_insn *decoded)
{
    /* 临时切换到软件现场的 FP 环境；无论执行结果如何，都恢复当前内核硬件环境。 */
    uint32_t current_fpcr = read_fpcr();
    uint32_t current_fpsr = read_fpsr();

    write_fpcr(fp_regs->fpcr);
    write_fpsr(fp_regs->fpsr);
    enum emu_insn_result result = emu_simulate_fp_simd_core(regs, fp_regs, decoded);
    if (result == EMU_INSN_HANDLED)
    {
        fp_regs->fpcr = read_fpcr();
        fp_regs->fpsr = read_fpsr();
    }
    write_fpcr(current_fpcr);
    write_fpsr(current_fpsr);
    return result;
}

/* ======================== 数据处理类：固定硬件模板与纯运算辅助 ======================== */

// clang-format off
static inline bool emu_cond_select_hw(enum arm64_instruction instruction, uint64_t a, uint64_t b, uint64_t nzcv, uint32_t condition, bool sf, uint64_t *result)
{
    uint32_t take = emu_cond_holds(nzcv, condition);

    if (!result) return false;

    switch (instruction)
    {
    case ARM64_INSN_CSEL:
        if (sf) asm volatile("cmp %w3, #0\ncsel %0, %1, %2, ne\n" : "=r"(*result) : "r"(a), "r"(b), "r"(take) : "cc");
        else
        {
            uint32_t result32;

            asm volatile("cmp %w3, #0\ncsel %w0, %w1, %w2, ne\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b), "r"(take) : "cc");
            *result = result32;
        }
        return true;
    case ARM64_INSN_CSINC:
        if (sf) asm volatile("cmp %w3, #0\ncsinc %0, %1, %2, ne\n" : "=r"(*result) : "r"(a), "r"(b), "r"(take) : "cc");
        else
        {
            uint32_t result32;

            asm volatile("cmp %w3, #0\ncsinc %w0, %w1, %w2, ne\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b), "r"(take) : "cc");
            *result = result32;
        }
        return true;
    case ARM64_INSN_CSINV:
        if (sf) asm volatile("cmp %w3, #0\ncsinv %0, %1, %2, ne\n" : "=r"(*result) : "r"(a), "r"(b), "r"(take) : "cc");
        else
        {
            uint32_t result32;

            asm volatile("cmp %w3, #0\ncsinv %w0, %w1, %w2, ne\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b), "r"(take) : "cc");
            *result = result32;
        }
        return true;
    case ARM64_INSN_CSNEG:
        if (sf) asm volatile("cmp %w3, #0\ncsneg %0, %1, %2, ne\n" : "=r"(*result) : "r"(a), "r"(b), "r"(take) : "cc");
        else
        {
            uint32_t result32;

            asm volatile("cmp %w3, #0\ncsneg %w0, %w1, %w2, ne\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b), "r"(take) : "cc");
            *result = result32;
        }
        return true;
    default:
        return false;
    }
}
// clang-format on

/* ADD/SUB 扩展寄存器：option 000..111 对应 UXT 或 SXT 变体，结果再左移 shift 位。 */
// clang-format off
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
    case 3:
        asm volatile("mov %0, %1\n" : "=r"(x) : "r"(val));
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
    case 7:
        asm volatile("mov %0, %1\n" : "=r"(x) : "r"(val));
        break;
    default:
        return val;
    }
    if (!shift) return x;
    asm volatile("lslv %0, %1, %2\n" : "=r"(x) : "r"(x), "r"((uint64_t)shift) : "cc");
    return x;
}

static inline bool emu_addsub_hw(enum arm64_instruction instruction, uint64_t a, uint64_t b, bool sf, uint64_t *result, uint64_t *nzcv, bool *setflags)
{
    uint64_t result64, flags;
    uint32_t result32;

    if (!result || !nzcv || !setflags) return false;
    *setflags = false;
    switch (instruction)
    {
    case ARM64_INSN_ADD_IMMEDIATE:
    case ARM64_INSN_ADD_SHIFTED_REGISTER:
    case ARM64_INSN_ADD_EXTENDED_REGISTER:
        if (sf)
        {
            asm volatile("add %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(b) : "cc");
            *result = result64;
        }
        else
        {
            asm volatile("add %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            *result = result32;
        }
        return true;
    case ARM64_INSN_ADDS_IMMEDIATE:
    case ARM64_INSN_ADDS_SHIFTED_REGISTER:
    case ARM64_INSN_ADDS_EXTENDED_REGISTER:
    case ARM64_INSN_CCMN_REGISTER:
    case ARM64_INSN_CCMN_IMMEDIATE:
        if (sf)
        {
            asm volatile("adds %0, %2, %3\n" "mrs %1, nzcv\n" : "=r"(result64), "=r"(flags) : "r"(a), "r"(b) : "cc");
            *result = result64;
        }
        else
        {
            asm volatile("adds %w0, %w2, %w3\n" "mrs %1, nzcv\n" : "=r"(result32), "=r"(flags) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            *result = result32;
        }
        *nzcv = flags;
        *setflags = true;
        return true;
    case ARM64_INSN_SUB_IMMEDIATE:
    case ARM64_INSN_SUB_SHIFTED_REGISTER:
    case ARM64_INSN_SUB_EXTENDED_REGISTER:
        if (sf)
        {
            asm volatile("sub %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(b) : "cc");
            *result = result64;
        }
        else
        {
            asm volatile("sub %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            *result = result32;
        }
        return true;
    case ARM64_INSN_SUBS_IMMEDIATE:
    case ARM64_INSN_SUBS_SHIFTED_REGISTER:
    case ARM64_INSN_SUBS_EXTENDED_REGISTER:
    case ARM64_INSN_CCMP_REGISTER:
    case ARM64_INSN_CCMP_IMMEDIATE:
        if (sf)
        {
            asm volatile("subs %0, %2, %3\n" "mrs %1, nzcv\n" : "=r"(result64), "=r"(flags) : "r"(a), "r"(b) : "cc");
            *result = result64;
        }
        else
        {
            asm volatile("subs %w0, %w2, %w3\n" "mrs %1, nzcv\n" : "=r"(result32), "=r"(flags) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            *result = result32;
        }
        *nzcv = flags;
        *setflags = true;
        return true;
    default:
        return false;
    }
}

static inline bool emu_addsub_carry_hw(enum arm64_instruction instruction, uint64_t a, uint64_t b, uint64_t input_nzcv, bool sf, uint64_t *result, uint64_t *nzcv, bool *setflags)
{
    uint64_t result64, flags;
    uint32_t result32;

    if (!result || !nzcv || !setflags) return false;
    *setflags = false;
    switch (instruction)
    {
    case ARM64_INSN_ADC:
        if (sf)
        {
            asm volatile("msr nzcv, %1\n" "adc %0, %2, %3\n" : "=r"(result64) : "r"(input_nzcv), "r"(a), "r"(b) : "cc");
            *result = result64;
        }
        else
        {
            asm volatile("msr nzcv, %1\n" "adc %w0, %w2, %w3\n" : "=r"(result32) : "r"(input_nzcv), "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            *result = result32;
        }
        return true;
    case ARM64_INSN_ADCS:
        if (sf)
        {
            asm volatile("msr nzcv, %2\n" "adcs %0, %3, %4\n" "mrs %1, nzcv\n" : "=r"(result64), "=r"(flags) : "r"(input_nzcv), "r"(a), "r"(b) : "cc");
            *result = result64;
        }
        else
        {
            asm volatile("msr nzcv, %2\n" "adcs %w0, %w3, %w4\n" "mrs %1, nzcv\n" : "=r"(result32), "=r"(flags) : "r"(input_nzcv), "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            *result = result32;
        }
        *nzcv = flags;
        *setflags = true;
        return true;
    case ARM64_INSN_SBC:
        if (sf)
        {
            asm volatile("msr nzcv, %1\n" "sbc %0, %2, %3\n" : "=r"(result64) : "r"(input_nzcv), "r"(a), "r"(b) : "cc");
            *result = result64;
        }
        else
        {
            asm volatile("msr nzcv, %1\n" "sbc %w0, %w2, %w3\n" : "=r"(result32) : "r"(input_nzcv), "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            *result = result32;
        }
        return true;
    case ARM64_INSN_SBCS:
        if (sf)
        {
            asm volatile("msr nzcv, %2\n" "sbcs %0, %3, %4\n" "mrs %1, nzcv\n" : "=r"(result64), "=r"(flags) : "r"(input_nzcv), "r"(a), "r"(b) : "cc");
            *result = result64;
        }
        else
        {
            asm volatile("msr nzcv, %2\n" "sbcs %w0, %w3, %w4\n" "mrs %1, nzcv\n" : "=r"(result32), "=r"(flags) : "r"(input_nzcv), "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            *result = result32;
        }
        *nzcv = flags;
        *setflags = true;
        return true;
    default:
        return false;
    }
}

static inline uint64_t emu_logic_hw(enum arm64_instruction instruction, uint64_t a, uint64_t b, bool sf, uint64_t *nzcv, bool *setflags)
{
    uint64_t result64, flags;
    uint32_t result32;

    *setflags = false;

    switch (instruction)
    {
    case ARM64_INSN_AND_IMMEDIATE:
    case ARM64_INSN_AND_SHIFTED_REGISTER:
        if (sf) asm volatile("and %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(b) : "cc");
        else
        {
            asm volatile("and %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            result64 = result32;
        }
        return result64;
    case ARM64_INSN_BIC_SHIFTED_REGISTER:
        if (sf) asm volatile("bic %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(b) : "cc");
        else
        {
            asm volatile("bic %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            result64 = result32;
        }
        return result64;
    case ARM64_INSN_ORR_IMMEDIATE:
    case ARM64_INSN_ORR_SHIFTED_REGISTER:
        if (sf) asm volatile("orr %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(b) : "cc");
        else
        {
            asm volatile("orr %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            result64 = result32;
        }
        return result64;
    case ARM64_INSN_ORN_SHIFTED_REGISTER:
        if (sf) asm volatile("orn %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(b) : "cc");
        else
        {
            asm volatile("orn %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            result64 = result32;
        }
        return result64;
    case ARM64_INSN_EOR_IMMEDIATE:
    case ARM64_INSN_EOR_SHIFTED_REGISTER:
        if (sf) asm volatile("eor %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(b) : "cc");
        else
        {
            asm volatile("eor %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            result64 = result32;
        }
        return result64;
    case ARM64_INSN_EON_SHIFTED_REGISTER:
        if (sf) asm volatile("eon %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(b) : "cc");
        else
        {
            asm volatile("eon %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            result64 = result32;
        }
        return result64;
    case ARM64_INSN_ANDS_IMMEDIATE:
    case ARM64_INSN_ANDS_SHIFTED_REGISTER:
        *setflags = true;
        if (sf)
        {
            asm volatile("ands %0, %2, %3\n" "mrs %1, nzcv\n" : "=r"(result64), "=r"(flags) : "r"(a), "r"(b) : "cc");
        }
        else
        {
            asm volatile("ands %w0, %w2, %w3\n" "mrs %1, nzcv\n" : "=r"(result32), "=r"(flags) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            result64 = result32;
        }
        *nzcv = flags;
        return result64;
    case ARM64_INSN_BICS_SHIFTED_REGISTER:
        *setflags = true;
        if (sf)
        {
            asm volatile("bics %0, %2, %3\n" "mrs %1, nzcv\n" : "=r"(result64), "=r"(flags) : "r"(a), "r"(b) : "cc");
        }
        else
        {
            asm volatile("bics %w0, %w2, %w3\n" "mrs %1, nzcv\n" : "=r"(result32), "=r"(flags) : "r"((uint32_t)a), "r"((uint32_t)b) : "cc");
            result64 = result32;
        }
        *nzcv = flags;
        return result64;
    default:
        return 0;
    }
}

static inline uint64_t emu_sign_extend_byte_hw(uint64_t value)
{
    uint64_t result;

    asm volatile("sxtb %0, %w1\n" : "=r"(result) : "r"((uint32_t)value));
    return result;
}
// clang-format on

static inline uint64_t emu_dp_mask(bool sf)
{
    return sf ? ~0ULL : 0xFFFFFFFFULL;
}

// clang-format off
static inline uint64_t emu_extract_bits(uint64_t high, uint64_t low, uint32_t shift, bool sf)
{
    uint64_t result, left, inverse;
    uint32_t result32, left32, inverse32;

    if (sf)
    {
        asm volatile("neg %2, %5\n" "lslv %1, %3, %2\n" "lsrv %0, %4, %5\n" "cmp %5, #0\n" "csel %1, xzr, %1, eq\n" "orr %0, %0, %1\n" : "=&r"(result), "=&r"(left), "=&r"(inverse) : "r"(high), "r"(low), "r"((uint64_t)shift) : "cc");
        return result;
    }

    asm volatile("neg %w2, %w5\n" "lslv %w1, %w3, %w2\n" "lsrv %w0, %w4, %w5\n" "cmp %w5, #0\n" "csel %w1, wzr, %w1, eq\n" "orr %w0, %w0, %w1\n" : "=&r"(result32), "=&r"(left32), "=&r"(inverse32) : "r"((uint32_t)high), "r"((uint32_t)low), "r"(shift) : "cc");
    return result32;
}

static inline bool emu_bitfield_hw(enum arm64_instruction instruction, uint64_t src, uint64_t dst, uint32_t immr, uint64_t wmask, uint64_t tmask, bool sf, uint64_t *result)
{
    uint64_t bot = emu_extract_bits(src, src, immr, sf);
    uint64_t result64, temporary64, auxiliary64;
    uint32_t result32, temporary32, auxiliary32;

    if (!result) return false;

    if (sf) asm volatile("and %0, %1, %2\n" : "=r"(bot) : "r"(bot), "r"(wmask));
    else asm volatile("and %w0, %w1, %w2\n" : "=r"(bot) : "r"((uint32_t)bot), "r"((uint32_t)wmask));

    switch (instruction)
    {
    case ARM64_INSN_SBFM:
        if (sf)
        {
            asm volatile("add %1, %5, #1\n" "lsr %1, %1, #1\n" "cmp %1, #0\n" "csel %1, %6, %1, eq\n" "and %0, %4, %5\n" "mvn %2, %5\n" "orr %2, %0, %2\n" "tst %4, %1\n" "csel %0, %2, %0, ne\n" : "=&r"(result64), "=&r"(temporary64), "=&r"(auxiliary64) : "0"(0ULL), "r"(bot), "r"(tmask), "r"(1ULL << 63) : "cc");
            *result = result64;
        }
        else
        {
            asm volatile("add %w1, %w5, #1\n" "lsr %w1, %w1, #1\n" "cmp %w1, #0\n" "csel %w1, %w6, %w1, eq\n" "and %w0, %w4, %w5\n" "mvn %w2, %w5\n" "orr %w2, %w0, %w2\n" "tst %w4, %w1\n" "csel %w0, %w2, %w0, ne\n" : "=&r"(result32), "=&r"(temporary32), "=&r"(auxiliary32) : "0"(0U), "r"((uint32_t)bot), "r"((uint32_t)tmask), "r"(1U << 31) : "cc");
            *result = result32;
        }
        return true;
    case ARM64_INSN_BFM:
        if (sf)
        {
            asm volatile("and %2, %4, %5\n" "bic %0, %3, %2\n" "and %1, %6, %2\n" "orr %0, %0, %1\n" : "=&r"(result64), "=&r"(temporary64), "=&r"(auxiliary64) : "r"(dst), "r"(wmask), "r"(tmask), "r"(bot));
            *result = result64;
        }
        else
        {
            asm volatile("and %w2, %w4, %w5\n" "bic %w0, %w3, %w2\n" "and %w1, %w6, %w2\n" "orr %w0, %w0, %w1\n" : "=&r"(result32), "=&r"(temporary32), "=&r"(auxiliary32) : "r"((uint32_t)dst), "r"((uint32_t)wmask), "r"((uint32_t)tmask), "r"((uint32_t)bot));
            *result = result32;
        }
        return true;
    case ARM64_INSN_UBFM:
        if (sf)
        {
            asm volatile("and %0, %1, %2\n" : "=r"(result64) : "r"(bot), "r"(tmask));
            *result = result64;
        }
        else
        {
            asm volatile("and %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)bot), "r"((uint32_t)tmask));
            *result = result32;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_move_wide_hw(enum arm64_instruction instruction, uint64_t dst, uint64_t immediate, uint32_t shift, bool sf, uint64_t *result)
{
    uint64_t result64, shifted64, mask64;
    uint32_t result32, shifted32, mask32;

    if (!result) return false;

    switch (instruction)
    {
    case ARM64_INSN_MOVN:
        if (sf)
        {
            asm volatile("lslv %0, %1, %2\n" "mvn %0, %0\n" : "=&r"(result64) : "r"(immediate), "r"((uint64_t)shift));
            *result = result64;
        }
        else
        {
            asm volatile("lslv %w0, %w1, %w2\n" "mvn %w0, %w0\n" : "=&r"(result32) : "r"((uint32_t)immediate), "r"(shift));
            *result = result32;
        }
        return true;
    case ARM64_INSN_MOVZ:
        if (sf)
        {
            asm volatile("lslv %0, %1, %2\n" : "=r"(result64) : "r"(immediate), "r"((uint64_t)shift));
            *result = result64;
        }
        else
        {
            asm volatile("lslv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)immediate), "r"(shift));
            *result = result32;
        }
        return true;
    case ARM64_INSN_MOVK:
        if (sf)
        {
            asm volatile("lslv %1, %4, %5\n" "lslv %2, %6, %5\n" "bic %0, %3, %2\n" "orr %0, %0, %1\n" : "=&r"(result64), "=&r"(shifted64), "=&r"(mask64) : "r"(dst), "r"(immediate), "r"((uint64_t)shift), "r"(0xFFFFULL));
            *result = result64;
        }
        else
        {
            asm volatile("lslv %w1, %w4, %w5\n" "lslv %w2, %w6, %w5\n" "bic %w0, %w3, %w2\n" "orr %w0, %w0, %w1\n" : "=&r"(result32), "=&r"(shifted32), "=&r"(mask32) : "r"((uint32_t)dst), "r"((uint32_t)immediate), "r"(shift), "r"(0xFFFFU));
            *result = result32;
        }
        return true;
    default:
        return false;
    }
}

static inline uint64_t emu_dp_shift_hw(uint64_t value, uint32_t type, uint32_t amount, bool sf)
{
    uint64_t result;

    switch (type)
    {
    case 0:
        if (sf) asm volatile("lslv %0, %1, %2\n" : "=r"(result) : "r"(value), "r"((uint64_t)amount) : "cc");
        else
        {
            uint32_t result32;

            asm volatile("lslv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)value), "r"(amount) : "cc");
            result = result32;
        }
        break;
    case 1:
        if (sf) asm volatile("lsrv %0, %1, %2\n" : "=r"(result) : "r"(value), "r"((uint64_t)amount) : "cc");
        else
        {
            uint32_t result32;

            asm volatile("lsrv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)value), "r"(amount) : "cc");
            result = result32;
        }
        break;
    case 2:
        if (sf) asm volatile("asrv %0, %1, %2\n" : "=r"(result) : "r"(value), "r"((uint64_t)amount) : "cc");
        else
        {
            uint32_t result32;

            asm volatile("asrv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)value), "r"(amount) : "cc");
            result = result32;
        }
        break;
    default:
        if (sf) asm volatile("rorv %0, %1, %2\n" : "=r"(result) : "r"(value), "r"((uint64_t)amount) : "cc");
        else
        {
            uint32_t result32;

            asm volatile("rorv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)value), "r"(amount) : "cc");
            result = result32;
        }
        break;
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

static inline uint32_t emu_dp_count_bits_hw(uint64_t value, bool sf)
{
    uint32_t result;

    if (sf)
        asm volatile("" "movi v0.2d, #0\n" "fmov d0, %1\n" "cnt v0.8b, v0.8b\n" "addv b0, v0.8b\n" "umov %w0, v0.b[0]\n" : "=&r"(result) : "r"(value) : "cc", "v0");
    else
        asm volatile("" "movi v0.2d, #0\n" "fmov s0, %w1\n" "cnt v0.8b, v0.8b\n" "addv b0, v0.8b\n" "umov %w0, v0.b[0]\n" : "=&r"(result) : "r"((uint32_t)value) : "cc", "v0");
    return result;
}

static inline uint32_t emu_dp_crc32b_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile("crc32b %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32h_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile("crc32h %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32w_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile("crc32w %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32x_hw(uint32_t accumulator, uint64_t value)
{
    uint32_t result;

    asm volatile("crc32x %w0, %w1, %2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32cb_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile("crc32cb %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32ch_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile("crc32ch %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32cw_hw(uint32_t accumulator, uint32_t value)
{
    uint32_t result;

    asm volatile("crc32cw %w0, %w1, %w2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline uint32_t emu_dp_crc32cx_hw(uint32_t accumulator, uint64_t value)
{
    uint32_t result;

    asm volatile("crc32cx %w0, %w1, %2\n" : "=r"(result) : "r"(accumulator), "r"(value));
    return result;
}

static inline bool emu_integer_binary_hw(enum arm64_instruction instruction, uint64_t a, uint64_t signed_b, uint64_t unsigned_b, bool sf, uint64_t *result)
{
    uint64_t result64;
    uint32_t result32;

    if (!result) return false;

    /* MIN/MAX immediate 早于 two-source；仅有 64 位编码的 CRC32X/CX 排在 32 位 CRC 之后。 */
    switch (instruction)
    {
    case ARM64_INSN_SMAX_IMMEDIATE:
    case ARM64_INSN_SMAX_REGISTER:
        if (sf) asm volatile("cmp %1, %2\ncsel %0, %1, %2, gt\n" : "=r"(result64) : "r"(a), "r"(signed_b) : "cc");
        else asm volatile("cmp %w1, %w2\ncsel %w0, %w1, %w2, gt\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)signed_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_UMAX_IMMEDIATE:
    case ARM64_INSN_UMAX_REGISTER:
        if (sf) asm volatile("cmp %1, %2\ncsel %0, %1, %2, hi\n" : "=r"(result64) : "r"(a), "r"(unsigned_b) : "cc");
        else asm volatile("cmp %w1, %w2\ncsel %w0, %w1, %w2, hi\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)unsigned_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_SMIN_IMMEDIATE:
    case ARM64_INSN_SMIN_REGISTER:
        if (sf) asm volatile("cmp %1, %2\ncsel %0, %1, %2, lt\n" : "=r"(result64) : "r"(a), "r"(signed_b) : "cc");
        else asm volatile("cmp %w1, %w2\ncsel %w0, %w1, %w2, lt\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)signed_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_UMIN_IMMEDIATE:
    case ARM64_INSN_UMIN_REGISTER:
        if (sf) asm volatile("cmp %1, %2\ncsel %0, %1, %2, lo\n" : "=r"(result64) : "r"(a), "r"(unsigned_b) : "cc");
        else asm volatile("cmp %w1, %w2\ncsel %w0, %w1, %w2, lo\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)unsigned_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_UDIV:
        if (sf) asm volatile("udiv %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(unsigned_b) : "cc");
        else asm volatile("udiv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)unsigned_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_SDIV:
        if (sf) asm volatile("sdiv %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(signed_b) : "cc");
        else asm volatile("sdiv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)signed_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_LSLV:
        if (sf) asm volatile("lslv %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(unsigned_b) : "cc");
        else asm volatile("lslv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)unsigned_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_LSRV:
        if (sf) asm volatile("lsrv %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(unsigned_b) : "cc");
        else asm volatile("lsrv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)unsigned_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_ASRV:
        if (sf) asm volatile("asrv %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(unsigned_b) : "cc");
        else asm volatile("asrv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)unsigned_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_RORV:
        if (sf) asm volatile("rorv %0, %1, %2\n" : "=r"(result64) : "r"(a), "r"(unsigned_b) : "cc");
        else asm volatile("rorv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)a), "r"((uint32_t)unsigned_b) : "cc");
        *result = sf ? result64 : result32;
        return true;
    case ARM64_INSN_CRC32B:
        *result = emu_dp_crc32b_hw((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32H:
        *result = emu_dp_crc32h_hw((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32W:
        *result = emu_dp_crc32w_hw((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32CB:
        *result = emu_dp_crc32cb_hw((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32CH:
        *result = emu_dp_crc32ch_hw((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32CW:
        *result = emu_dp_crc32cw_hw((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32X:
        *result = emu_dp_crc32x_hw((uint32_t)a, unsigned_b);
        return true;
    case ARM64_INSN_CRC32CX:
        *result = emu_dp_crc32cx_hw((uint32_t)a, unsigned_b);
        return true;
    default:
        return false;
    }
}
// clang-format on

/* ======================== 数据处理类：完整执行流程 ======================== */

static inline enum emu_insn_result emu_simulate_data_processing_immediate_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded)
{
    uint64_t pc = regs->pc;

    switch (decoded->instruction)
    {
    case ARM64_INSN_ADR:
    {
        uint64_t target = pc + decoded->offset;

        if (decoded->rd != 31) regs->regs[decoded->rd] = target;
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_ADD_IMMEDIATE:
    case ARM64_INSN_ADDS_IMMEDIATE:
    case ARM64_INSN_SUB_IMMEDIATE:
    case ARM64_INSN_SUBS_IMMEDIATE:
    {
        uint64_t a = addr_reg_read(regs, decoded->rn);
        uint64_t nzcv = 0;
        bool setflags;
        uint64_t result;

        if (!emu_addsub_hw(decoded->instruction, a, decoded->immediate, decoded->operand_width == 64, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;

        if (setflags)
        {
            emu_write_nzcv(regs, nzcv);
            reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        }
        else
        {
            addr_reg_write(regs, decoded->rd, result);
        }
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_SMAX_IMMEDIATE:
    case ARM64_INSN_UMAX_IMMEDIATE:
    case ARM64_INSN_SMIN_IMMEDIATE:
    case ARM64_INSN_UMIN_IMMEDIATE:
    {
        uint64_t a = reg_read(regs, decoded->rn) & emu_dp_mask(decoded->operand_width == 64);
        uint64_t signed_b = emu_sign_extend_byte_hw(decoded->immediate) & emu_dp_mask(decoded->operand_width == 64);
        uint64_t result;

        if (!emu_integer_binary_hw(decoded->instruction, a, signed_b, decoded->immediate, decoded->operand_width == 64, &result)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_AND_IMMEDIATE:
    case ARM64_INSN_ORR_IMMEDIATE:
    case ARM64_INSN_EOR_IMMEDIATE:
    case ARM64_INSN_ANDS_IMMEDIATE:
    {
        uint64_t a = reg_read(regs, decoded->rn) & emu_dp_mask(decoded->operand_width == 64);
        uint64_t nzcv = 0;
        bool setflags;
        uint64_t result = emu_logic_hw(decoded->instruction, a, decoded->immediate, decoded->operand_width == 64, &nzcv, &setflags);

        if (setflags) emu_write_nzcv(regs, nzcv);

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_MOVN:
    case ARM64_INSN_MOVZ:
    case ARM64_INSN_MOVK:
    {
        uint64_t result;

        if (!emu_move_wide_hw(decoded->instruction, reg_read(regs, decoded->rd), decoded->immediate, decoded->shift_amount, decoded->operand_width == 64, &result)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_SBFM:
    case ARM64_INSN_BFM:
    case ARM64_INSN_UBFM:
    {
        uint64_t src = reg_read(regs, decoded->rn) & emu_dp_mask(decoded->operand_width == 64);
        uint64_t dst = reg_read(regs, decoded->rd) & emu_dp_mask(decoded->operand_width == 64);
        uint64_t result;
        if (!emu_bitfield_hw(decoded->instruction, src, dst, decoded->immr, decoded->wmask, decoded->tmask, decoded->operand_width == 64, &result)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_EXTR:
    {
        uint64_t result = emu_extract_bits(reg_read(regs, decoded->rn), reg_read(regs, decoded->rm), decoded->shift_amount, decoded->operand_width == 64);

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_ADRP:
    {
        uint64_t target = (pc & ~0xFFFULL) + decoded->offset;

        if (decoded->rd != 31) regs->regs[decoded->rd] = target;
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    default:
        return EMU_INSN_SKIP;
    }
}

// clang-format off
static inline enum emu_insn_result emu_simulate_data_processing_register_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded)
{
    uint64_t pc = regs->pc;

    switch (decoded->instruction)
    {
    case ARM64_INSN_AND_SHIFTED_REGISTER:
    case ARM64_INSN_BIC_SHIFTED_REGISTER:
    case ARM64_INSN_ORR_SHIFTED_REGISTER:
    case ARM64_INSN_ORN_SHIFTED_REGISTER:
    case ARM64_INSN_EOR_SHIFTED_REGISTER:
    case ARM64_INSN_EON_SHIFTED_REGISTER:
    case ARM64_INSN_ANDS_SHIFTED_REGISTER:
    case ARM64_INSN_BICS_SHIFTED_REGISTER:
    {
        uint64_t a = reg_read(regs, decoded->rn);
        uint64_t b = emu_dp_shift_hw(reg_read(regs, decoded->rm), decoded->shift_type, decoded->shift_amount, decoded->operand_width == 64);
        uint64_t nzcv = 0;
        bool setflags;
        uint64_t result = emu_logic_hw(decoded->instruction, a, b, decoded->operand_width == 64, &nzcv, &setflags);

        if (setflags) emu_write_nzcv(regs, nzcv);
        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_ADD_SHIFTED_REGISTER:
    case ARM64_INSN_ADDS_SHIFTED_REGISTER:
    case ARM64_INSN_SUB_SHIFTED_REGISTER:
    case ARM64_INSN_SUBS_SHIFTED_REGISTER:
    {
        uint64_t a = reg_read(regs, decoded->rn);
        uint64_t b = emu_dp_shift_hw(reg_read(regs, decoded->rm), decoded->shift_type, decoded->shift_amount, decoded->operand_width == 64);
        uint64_t nzcv = 0;
        bool setflags;
        uint64_t result;

        if (!emu_addsub_hw(decoded->instruction, a, b, decoded->operand_width == 64, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
        if (setflags) emu_write_nzcv(regs, nzcv);
        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_ADD_EXTENDED_REGISTER:
    case ARM64_INSN_ADDS_EXTENDED_REGISTER:
    case ARM64_INSN_SUB_EXTENDED_REGISTER:
    case ARM64_INSN_SUBS_EXTENDED_REGISTER:
    {
        uint64_t a = addr_reg_read(regs, decoded->rn);
        uint64_t b = emu_extend_reg(reg_read(regs, decoded->rm), decoded->option, decoded->shift_amount);
        uint64_t nzcv = 0;
        bool setflags;
        uint64_t result;

        if (!emu_addsub_hw(decoded->instruction, a, b, decoded->operand_width == 64, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;

        if (setflags)
        {
            emu_write_nzcv(regs, nzcv);
            reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        }
        else
        {
            addr_reg_write(regs, decoded->rd, result);
        }
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_ADC:
    case ARM64_INSN_ADCS:
    case ARM64_INSN_SBC:
    case ARM64_INSN_SBCS:
    {
        uint64_t x = reg_read(regs, decoded->rn);
        uint64_t y = reg_read(regs, decoded->rm);
        uint64_t input_nzcv = emu_read_nzcv(regs);
        uint64_t result, nzcv;
        bool setflags;

        if (!emu_addsub_carry_hw(decoded->instruction, x, y, input_nzcv, decoded->operand_width == 64, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
        if (setflags) emu_write_nzcv(regs, nzcv);

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_CSEL:
    case ARM64_INSN_CSINC:
    case ARM64_INSN_CSINV:
    case ARM64_INSN_CSNEG:
    {
        uint64_t a = reg_read(regs, decoded->rn);
        uint64_t b = reg_read(regs, decoded->rm);
        uint64_t result;
        if (!emu_cond_select_hw(decoded->instruction, a, b, emu_read_nzcv(regs), decoded->condition, decoded->operand_width == 64, &result)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_UDIV:
    case ARM64_INSN_SDIV:
    case ARM64_INSN_LSLV:
    case ARM64_INSN_LSRV:
    case ARM64_INSN_ASRV:
    case ARM64_INSN_RORV:
    case ARM64_INSN_CRC32B:
    case ARM64_INSN_CRC32H:
    case ARM64_INSN_CRC32W:
    case ARM64_INSN_CRC32X:
    case ARM64_INSN_CRC32CB:
    case ARM64_INSN_CRC32CH:
    case ARM64_INSN_CRC32CW:
    case ARM64_INSN_CRC32CX:
    case ARM64_INSN_SMAX_REGISTER:
    case ARM64_INSN_UMAX_REGISTER:
    case ARM64_INSN_SMIN_REGISTER:
    case ARM64_INSN_UMIN_REGISTER:
    {
        uint64_t a = reg_read(regs, decoded->rn) & emu_dp_mask(decoded->operand_width == 64);
        uint64_t b = reg_read(regs, decoded->rm) & emu_dp_mask(decoded->operand_width == 64);
        uint64_t result;

        if (!emu_integer_binary_hw(decoded->instruction, a, b, b, decoded->operand_width == 64, &result)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_MADD:
    case ARM64_INSN_MSUB:
    case ARM64_INSN_SMADDL:
    case ARM64_INSN_SMSUBL:
    case ARM64_INSN_SMULH:
    case ARM64_INSN_UMADDL:
    case ARM64_INSN_UMSUBL:
    case ARM64_INSN_UMULH:
    {
        uint64_t result;

        switch (decoded->instruction)
        {
        case ARM64_INSN_MADD:
        {
            uint64_t n = reg_read(regs, decoded->rn) & emu_dp_mask(decoded->operand_width == 64);
            uint64_t m = reg_read(regs, decoded->rm) & emu_dp_mask(decoded->operand_width == 64);
            uint64_t a = reg_read(regs, decoded->ra) & emu_dp_mask(decoded->operand_width == 64);

            if (decoded->operand_width == 64)
            {
                asm volatile("madd %0, %1, %2, %3\n" : "=r"(result) : "r"(n), "r"(m), "r"(a));
            }
            else
            {
                uint32_t result32;

                asm volatile("madd %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)n), "r"((uint32_t)m), "r"((uint32_t)a));
                result = result32;
            }
            break;
        }
        case ARM64_INSN_MSUB:
        {
            uint64_t n = reg_read(regs, decoded->rn) & emu_dp_mask(decoded->operand_width == 64);
            uint64_t m = reg_read(regs, decoded->rm) & emu_dp_mask(decoded->operand_width == 64);
            uint64_t a = reg_read(regs, decoded->ra) & emu_dp_mask(decoded->operand_width == 64);

            if (decoded->operand_width == 64)
            {
                asm volatile("msub %0, %1, %2, %3\n" : "=r"(result) : "r"(n), "r"(m), "r"(a));
            }
            else
            {
                uint32_t result32;

                asm volatile("msub %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)n), "r"((uint32_t)m), "r"((uint32_t)a));
                result = result32;
            }
            break;
        }
        case ARM64_INSN_SMADDL:
        {
            uint64_t a = reg_read(regs, decoded->ra);

            asm volatile("smaddl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, decoded->rn)), "r"((uint32_t)reg_read(regs, decoded->rm)), "r"(a));
            break;
        }
        case ARM64_INSN_SMSUBL:
        {
            uint64_t a = reg_read(regs, decoded->ra);

            asm volatile("smsubl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, decoded->rn)), "r"((uint32_t)reg_read(regs, decoded->rm)), "r"(a));
            break;
        }
        case ARM64_INSN_SMULH:
            asm volatile("smulh %0, %1, %2\n" : "=r"(result) : "r"(reg_read(regs, decoded->rn)), "r"(reg_read(regs, decoded->rm)));
            break;
        case ARM64_INSN_UMADDL:
        {
            uint64_t a = reg_read(regs, decoded->ra);

            asm volatile("umaddl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, decoded->rn)), "r"((uint32_t)reg_read(regs, decoded->rm)), "r"(a));
            break;
        }
        case ARM64_INSN_UMSUBL:
        {
            uint64_t a = reg_read(regs, decoded->ra);

            asm volatile("umsubl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, decoded->rn)), "r"((uint32_t)reg_read(regs, decoded->rm)), "r"(a));
            break;
        }
        case ARM64_INSN_UMULH:
            asm volatile("umulh %0, %1, %2\n" : "=r"(result) : "r"(reg_read(regs, decoded->rn)), "r"(reg_read(regs, decoded->rm)));
            break;
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_CCMN_REGISTER:
    case ARM64_INSN_CCMN_IMMEDIATE:
    case ARM64_INSN_CCMP_REGISTER:
    case ARM64_INSN_CCMP_IMMEDIATE:
    {
        uint64_t a = reg_read(regs, decoded->rn);
        bool immediate = decoded->instruction == ARM64_INSN_CCMN_IMMEDIATE || decoded->instruction == ARM64_INSN_CCMP_IMMEDIATE;
        uint64_t b = immediate ? decoded->immediate : reg_read(regs, decoded->rm);
        uint64_t result, flags;
        bool setflags;

        if (emu_cond_holds(emu_read_nzcv(regs), decoded->condition))
        {
            if (!emu_addsub_hw(decoded->instruction, a, b, decoded->operand_width == 64, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
        }
        else flags = (uint64_t)decoded->nzcv << 28;
        emu_write_nzcv(regs, flags);

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    case ARM64_INSN_RBIT:
    case ARM64_INSN_REV16:
    case ARM64_INSN_REV32:
    case ARM64_INSN_REV64:
    case ARM64_INSN_CLZ:
    case ARM64_INSN_CLS:
    case ARM64_INSN_CTZ:
    case ARM64_INSN_CNT:
    case ARM64_INSN_ABS:
    {
        uint64_t src = reg_read(regs, decoded->rn);
        uint64_t result;

        switch (decoded->instruction)
        {
        case ARM64_INSN_RBIT:
            result = emu_dp_rbit_hw(src, decoded->operand_width == 64);
            break;
        case ARM64_INSN_REV16:
            result = emu_dp_rev16_hw(src, decoded->operand_width == 64);
            break;
        case ARM64_INSN_REV32:
            result = emu_dp_rev32_hw(src, decoded->operand_width == 64);
            break;
        case ARM64_INSN_CLZ:
            result = emu_dp_clz_hw(src, decoded->operand_width == 64);
            break;
        case ARM64_INSN_CLS:
            result = emu_dp_cls_hw(src, decoded->operand_width == 64);
            break;
        case ARM64_INSN_CTZ:
            result = emu_dp_clz_hw(emu_dp_rbit_hw(src, decoded->operand_width == 64), decoded->operand_width == 64);
            break;
        case ARM64_INSN_CNT:
            result = emu_dp_count_bits_hw(src, decoded->operand_width == 64);
            break;
        case ARM64_INSN_ABS:
            if (decoded->operand_width == 64)
                asm volatile("cmp %1, #0\n" "cneg %0, %1, mi\n" : "=r"(result) : "r"(src) : "cc");
            else
            {
                uint32_t result32;

                asm volatile("cmp %w1, #0\n" "cneg %w0, %w1, mi\n" : "=r"(result32) : "r"((uint32_t)src) : "cc");
                result = result32;
            }
            break;
        case ARM64_INSN_REV64:
            result = emu_dp_rev64_hw(src);
            break;
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, decoded->rd, result, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    default:
        return EMU_INSN_SKIP;
    }
}
// clang-format on

/* ======================== 已解码指令：架构大类分派 ======================== */

static inline enum emu_insn_result emu_execute_decoded_insn(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_decoded_insn *decoded)
{
    enum emu_insn_result result = EMU_INSN_SKIP;

    switch (decoded->insn_class)
    {
    case ARM64_INSN_CLASS_LOAD_STORE:
        // 加载、存储、原子操作和预取类指令。
        result = emu_simulate_load_store_insn(regs, fp_regs, decoded);
        break;
    case ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER:
        // 以通用寄存器为主要操作数的数据处理指令。
        result = emu_simulate_data_processing_register_insn(regs, decoded);
        break;
    case ARM64_INSN_CLASS_DATA_PROCESSING_SIMD_FP:
        // 标量浮点和 Advanced SIMD 指令。
        result = emu_simulate_fp_simd_insn(regs, fp_regs, decoded);
        break;
    case ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE:
        // 以立即数为主要操作数的数据处理指令。
        result = emu_simulate_data_processing_immediate_insn(regs, decoded);
        break;
    case ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM:
        // 分支、异常和系统类指令。
        result = emu_simulate_branch_system_insn(regs, fp_regs, decoded);
        break;
    default:
        // 未实现执行语义的类别保持 SKIP。
        break;
    }
    return result;
}

/*
访存类指令使用模板汇编让硬件真实同语义需要注意一个问题：
COW:当前进程准备写入一个仍与其他进程或映射共享的物理页，而该虚拟内存区域在逻辑上属于私有可写。Linux 为避免提前复制页面，先让这些映射共享同一物理页，并将相关 PTE 设置为只读。首次写入触发权限异常后，内核为当前进程建立私有副本，将其 PTE 改为可写，然后重新执行写入指令。
这里执行访存类指令写的时候目标地址页如果刚好处于COW中就会之间panic

还需要注意的:
内核代码中几乎根本不会去写Advanced SIMD/FP类的代码,也不建议你去，所以使用任何clang版本都没有问题
不影响cpu支持这些扩展指令集，然后用户态的新clang可以编译出Advanced SIMD/FP汇编运行
这里为了模拟使用.inst直接写机器码去让cpu执行，不然编译内核的旧clang根本识别不出这些新扩展的助记符
*/

/* ======================== 总入口：指令字获取、解码与执行 ======================== */

// clang-format off
static inline bool emulate_insn(struct pt_regs *regs, struct fp_regs *fp_regs, uint32_t specified_insn)
{
    struct arm64_decoded_insn decoded;
    uint64_t pc = regs->pc;
    uint32_t insn = specified_insn;

    /* 生产调用点来自异常/内核上下文 不建议硬编码
    asm volatile(".inst 0xd500409f" ::: "memory");
    asm volatile(".inst 0xd500419f" ::: "memory");
    安全特性	    硬件支持版本	默认状态        uaccess_enable_privileged 的操作
    MTE	           ARMv8.5+	      开启校验	        mte_disable_tco(); 开启 TCO，忽略校验
    SW PAN	       ARMv8.0	      卸载 TTBR0	   uaccess_ttbr0_disable();重新加载 TTBR0 用户页表
    HW PAN	       ARMv8.1+	      阻止内核访问 EL0	 __uaccess_enable_hw_pan();禁用 HW PAN，允许内核访问 EL0
    uaccess_ttbr0_disable是为了在没有硬件PAN时进行的软件切换基址寄存器，实现PAN,
    支持硬件PAN就不会去走软件PAN,而是快速判断进行返回
    */
    uaccess_enable_privileged();
    if (!insn)
    {
        asm volatile("ldr %w0, [%1]" : "=r"(insn) : "r"(pc) : "memory");
    }

    bool handled = false;
    if (arm64_decode_insn(insn, &decoded) == ARM64_DECODE_OK)
    {
        handled = emu_execute_decoded_insn(regs, fp_regs, &decoded) == EMU_INSN_HANDLED;
    }

    uaccess_disable_privileged();

    if (unlikely(!handled))
    {
        ls_log_always_tag("emulate_insn", "failed pc=0x%llx insn=0x%08x bytes=%02x %02x %02x %02x\n", (unsigned long long)pc, insn, insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, insn >> 24);
    }

    return handled;
}
// clang-format on

#endif // EMULATE_INSN_H
