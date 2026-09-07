#ifndef EMULATE_INST_H
#define EMULATE_INST_H

#include <asm/ptrace.h>
#include <asm/sysreg.h>
#include <linux/bits.h>
#include "../arm64_reg.h"
#include "../arm64_decode/arm64_decode.h"
#include "arm64_emulate_hw_templates.h"

enum emu_inst_result
{
    EMU_INST_HANDLED, // 模拟函数已完成指令语义，并自行更新现场和 PC
    EMU_INST_SKIP,    // 不支持或无法执行，不能推进 PC
};

/* =========================================================================
  ARM64 指令执行器


  作用：
  - 在断点命中后执行当前用户态指令语义，将结果同步至 pt_regs 和fp_regs软件现场并推进 PC。软件现场由外部异常处理统一写回cpu
    - 调用者进入时必须保持特权用户访问关闭，并保证执行期间不迁移 CPU；执行器临时打开用户访问，退出前恢复为关闭。
  - 调用者提供完整 GPR、PSTATE、Q0-Q31、FPCR 和 FPSR 软件现场；执行器的架构结果只写入传入现场。
    - 当前 CPU 寄存器只作为固定硬件模板的执行载体；外部异常处理在执行器返回后将完整软件现场统一写回 CPU。
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

static inline uint64_t read_gpr_or_zr(struct pt_regs *regs, uint32_t n)
{
    return (n == 31) ? 0ULL : regs->regs[n];
}

static inline void write_gpr_or_zr(struct pt_regs *regs, uint32_t n, uint64_t val, bool sf)
{
    if (n != 31) regs->regs[n] = sf ? val : (uint64_t)(uint32_t)val;
}

static inline uint64_t read_gpr_or_sp(struct pt_regs *regs, uint32_t n)
{
    return (n == 31) ? regs->sp : regs->regs[n];
}

static inline void write_gpr_or_sp(struct pt_regs *regs, uint32_t n, uint64_t val)
{
    if (n == 31) regs->sp = val;
    else regs->regs[n] = val;
}

static inline uint64_t read_nzcv(const struct pt_regs *regs)
{
    return regs->pstate & GENMASK_ULL(31, 28);
}

static inline void write_nzcv(struct pt_regs *regs, uint64_t nzcv)
{
    regs->pstate = (regs->pstate & ~GENMASK_ULL(31, 28)) | (nzcv & GENMASK_ULL(31, 28));
}

// 直接根据异常现场的 NZCV 计算 A64 条件码是否成立
static inline bool cond_holds(uint64_t nzcv, uint32_t cond)
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

struct arm64_hw_template_output
{
    uint64_t value0;
    uint64_t value1;
    uint64_t value2;
    uint64_t value3;
    uint64_t writeback_value;
};

struct arm64_executor_entry
{
    enum emu_inst_result (*execute)(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry);
    struct arm64_decoded_instruction decoded;
};

bool emu_build_executor_entry(const struct arm64_decoded_instruction *decoded, struct arm64_executor_entry *entry);
__nocfi enum emu_inst_result emu_execute_executor_entry(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry);

enum emu_inst_result (*emu_build_branch_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry);
enum emu_inst_result (*emu_build_ldst_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry);
enum emu_inst_result (*emu_build_simd_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry);
enum emu_inst_result (*emu_build_immediate_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry);
enum emu_inst_result (*emu_build_register_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry);

bool emulate_inst(struct pt_regs *regs, struct fp_regs *fp_regs, uint32_t specified_inst);

#endif // EMULATE_INST_H