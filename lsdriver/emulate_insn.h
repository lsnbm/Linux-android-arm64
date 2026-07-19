#ifndef EMULATE_INSN_H
#define EMULATE_INSN_H

#include <linux/uaccess.h>
#include <linux/errno.h>
#include <asm/ptrace.h>
#include <asm/barrier.h>
#include <asm/sysreg.h>
#include "arm64_reg.h"
#include "arm64_decode/arm64_decode.h"

enum emu_insn_result
{
    EMU_INSN_HANDLED,
    EMU_INSN_SKIP,
    EMU_INSN_NOP,
    EMU_INSN_FAULT,
};

/* =========================================================================
  ARM64 指令模拟器 (emulate_insn)

  作用：断点命中后，在内核里模拟当前用户态指令并推进 pt_regs->pc，避免
  依赖硬件单步。当前主要服务于 HWBP/PTEBP 的命中后步过场景。

    处理结构：emulate_insn() 取指后调用 arm64_decode/ 的纯逻辑解析器完成
    分类、字段提取和语义规范化，再把 struct arm64_decoded_insn 分发给各大类 handler。
    handler 只读取解析结果并执行，不再解释原始指令编码。

    执行原则：C 只负责指令解码、地址/立即数展开和现场搬运；复杂 ALU、条件、
    FP/SIMD 语义直接执行同语义 ARM64 指令片段，再把结果同步回 pt_regs/Q/FPSR。

  已支持的大类：
  - 分支类：emu_simulate_branch_insn() 处理 B、BL、BR、BLR、RET、B.cond、
    CBZ、CBNZ、TBZ、TBNZ。
  - 访存类：emu_simulate_load_store_insn() 处理整数/FP/SIMD 访存、literal load、
    pair load/store、PRFM literal NOP，以及 LSE 原子访存 SWP、LDADD、LDCLR、
    LDEOR、LDSET、LDSMAX、LDSMIN、LDUMAX、LDUMIN、CAS、CASP。
  - FP/SIMD 类：emu_simulate_fp_simd_insn() 通过 arm64_reg.h 读取/写回 Q0-Q31、
    FPSR、FPCR，处理本文件内已实现的标量 FP 和部分 AdvSIMD/NEON 运算。
    - 系统类：emu_simulate_system_insn() 处理 MRS/MSR(register) 白名单系统寄存器。
    - 数据处理类：emu_simulate_data_processing_insn() 处理 ADR/ADRP、ADD/SUB、ADDS/SUBS、
    CMP/CMN、逻辑运算、MOV/MOVK/MOVN/MOVZ、SBFM/UBFM/BFM、EXTR、条件选择、
    除法、移位、乘加/乘减、ADC/SBC、CCMP/CCMN、REV/RBIT、CLZ/CLS、长乘、
    CRC32/CRC32C、CTZ/CNT/ABS、SMAX/SMIN/UMAX/UMIN。

  暂不支持：
  - 数据处理：RMIF、SETF8、SETF16、CFINV、AXFLAG、XAFLAG。
    - 独占指令按普通访存模拟：LDXR、STXR、LDAXR、STLXR、LDXP、STXP、
        LDAXP、STLXP；STXR/STXP 固定返回成功，不保留硬件独占语义。
    - 有序/非特权访存：LDAR、STLR、LDAPR、LDAPUR、STLUR、LDTR、STTR。
  - 指针认证和 MTE：PACIA/AUTIA、LDRAA/LDRAB、IRG/GMI/SUBP 等。
  - SVE/SME 以及向量长度相关指令。
    - 未覆盖的 FP16、复杂 AdvSIMD 重排/结构化访存：TBL/TBX、ZIP/UZP/TRN、
    LD1/ST1/LD1R 等。
    - 异常和大部分系统指令：SVC、HVC、SMC、BRK、未列入白名单的系统寄存器。
  ========================================================================= */

#define EMU_SYSREG_NZCV        ARM64_SYSREG_KEY(3, 3, 4, 2, 0)
#define EMU_SYSREG_FPCR        ARM64_SYSREG_KEY(3, 3, 4, 4, 0)
#define EMU_SYSREG_FPSR        ARM64_SYSREG_KEY(3, 3, 4, 4, 1)
#define EMU_SYSREG_TPIDR_EL0   ARM64_SYSREG_KEY(3, 3, 13, 0, 2)
#define EMU_SYSREG_TPIDRRO_EL0 ARM64_SYSREG_KEY(3, 3, 13, 0, 3)

// 整数寄存器与条件执行辅助
static __always_inline uint64_t reg_read(struct pt_regs *regs, uint32_t n)
{
    return (n == 31) ? 0ULL : regs->regs[n];
}
static __always_inline void reg_write(struct pt_regs *regs, uint32_t n, uint64_t val, bool sf)
{
    if (n != 31) regs->regs[n] = sf ? val : (uint64_t)(uint32_t)val;
}
static __always_inline uint64_t addr_reg_read(struct pt_regs *regs, uint32_t n)
{
    return (n == 31) ? regs->sp : regs->regs[n];
}
static __always_inline void addr_reg_write(struct pt_regs *regs, uint32_t n, uint64_t val)
{
    if (n == 31) regs->sp = val;
    else regs->regs[n] = val;
}

struct emu_mem_access
{
    int (*read)(void *ctx, uint64_t addr, int bytes, __uint128_t *out);
    int (*write)(void *ctx, uint64_t addr, int bytes, __uint128_t value);
    void *ctx;
};

/* 自定义读取器返回 -EOPNOTSUPP 表示该地址不归它处理，继续使用 __get_user。 */
static __always_inline int emu_read_mem(const struct emu_mem_access *access, uint64_t addr, int bytes, __uint128_t *out)
{
    __uint128_t v = 0;
    int status;

    if (access && access->read)
    {
        status = access->read(access->ctx, addr, bytes, out);
        if (status != -EOPNOTSUPP) return status;
    }

    switch (bytes)
    {
    case 1:
    {
        u8 t;
        if (__get_user(t, (u8 __user *)addr)) return -EFAULT;
        v = t;
        break;
    }
    case 2:
    {
        u16 t;
        if (__get_user(t, (u16 __user *)addr)) return -EFAULT;
        v = t;
        break;
    }
    case 4:
    {
        u32 t;
        if (__get_user(t, (u32 __user *)addr)) return -EFAULT;
        v = t;
        break;
    }
    case 8:
    {
        u64 t;
        if (__get_user(t, (u64 __user *)addr)) return -EFAULT;
        v = t;
        break;
    }
    case 16:
    {
        u64 lo, hi;
        if (__get_user(lo, (u64 __user *)addr) || __get_user(hi, (u64 __user *)(addr + 8))) return -EFAULT;
        v = ((__uint128_t)hi << 64) | lo;
        break;
    }
    default:
        return -EFAULT;
    }

    *out = v;
    return 0;
}

/* 自定义写入器返回 -EOPNOTSUPP 表示该地址不归它处理，继续使用 __put_user。 */
static __always_inline int emu_write_mem(const struct emu_mem_access *access, uint64_t addr, int bytes, __uint128_t value)
{
    int status;

    if (access && access->write)
    {
        status = access->write(access->ctx, addr, bytes, value);
        if (status != -EOPNOTSUPP) return status;
    }

    switch (bytes)
    {
    case 1:
        return __put_user((u8)value, (u8 __user *)addr) ? -EFAULT : 0;
    case 2:
        return __put_user((u16)value, (u16 __user *)addr) ? -EFAULT : 0;
    case 4:
        return __put_user((u32)value, (u32 __user *)addr) ? -EFAULT : 0;
    case 8:
        return __put_user((u64)value, (u64 __user *)addr) ? -EFAULT : 0;
    case 16:
        if (__put_user((u64)value, (u64 __user *)addr)) return -EFAULT;
        return __put_user((u64)(value >> 64), (u64 __user *)(addr + 8)) ? -EFAULT : 0;
    default:
        return -EFAULT;
    }
}

/* ---- 跨大类通用逻辑：系统/分支/访存/FP/数据处理按需复用 ---- */

static __always_inline void emu_write_nzcv(struct pt_regs *regs, uint64_t nzcv)
{
    regs->pstate = (regs->pstate & ~((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28))) | (nzcv & ((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28)));
}

#define EMU_COND_HW_CASE(NUM, COND)                 \
    case NUM:                                       \
        asm volatile("msr nzcv, %1\n"               \
                     "cset %w0, " COND "\n"         \
                     : "=r"(take)                   \
                     : "r"(pstate & (0xFULL << 28)) \
                     : "cc");                       \
        break

static __always_inline bool emu_cond_holds_hw(uint64_t pstate, uint32_t cond)
{
    uint32_t take;

    switch (cond)
    {
        EMU_COND_HW_CASE(0x0, "eq");
        EMU_COND_HW_CASE(0x1, "ne");
        EMU_COND_HW_CASE(0x2, "cs");
        EMU_COND_HW_CASE(0x3, "cc");
        EMU_COND_HW_CASE(0x4, "mi");
        EMU_COND_HW_CASE(0x5, "pl");
        EMU_COND_HW_CASE(0x6, "vs");
        EMU_COND_HW_CASE(0x7, "vc");
        EMU_COND_HW_CASE(0x8, "hi");
        EMU_COND_HW_CASE(0x9, "ls");
        EMU_COND_HW_CASE(0xA, "ge");
        EMU_COND_HW_CASE(0xB, "lt");
        EMU_COND_HW_CASE(0xC, "gt");
        EMU_COND_HW_CASE(0xD, "le");
    default:
        return true;
    }
    return take != 0;
}

#undef EMU_COND_HW_CASE

static __always_inline bool emu_cond_select_hw(enum arm64_operation operation, uint64_t a, uint64_t b, uint64_t pstate, uint32_t condition, bool sf, uint64_t *result)
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

// ADD/SUB 扩展寄存器的操作数扩展：option 000..111 = UXTB/UXTH/UXTW/UXTX/SXTB/SXTH/SXTW/SXTX，
// 再左移 shift(0..4) 位。
static __always_inline uint64_t emu_extend_reg(uint64_t val, uint32_t option, uint32_t shift)
{
    uint64_t x;

    switch (option)
    {
    case 0:
        asm volatile("uxtb %w0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break; // UXTB
    case 1:
        asm volatile("uxth %w0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break; // UXTH
    case 2:
        asm volatile("mov %w0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break; // UXTW
    case 4:
        asm volatile("sxtb %0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break; // SXTB
    case 5:
        asm volatile("sxth %0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break; // SXTH
    case 6:
        asm volatile("sxtw %0, %w1\n" : "=r"(x) : "r"((uint32_t)val));
        break; // SXTW
    default:
        asm volatile("mov %0, %1\n" : "=r"(x) : "r"(val));
        break; // UXTX(3) / SXTX(7)：整寄存器
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

static __always_inline uint64_t emu_addsub_hw(uint64_t a, uint64_t b, bool op_sub, bool setflags, bool sf, uint64_t *nzcv)
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

static __always_inline uint64_t emu_logic_hw(uint64_t a, uint64_t b, uint32_t opc, bool invert, bool sf, uint64_t *nzcv)
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

static __always_inline uint64_t emu_minmax_hw(uint64_t a, uint64_t b, bool is_min, bool is_unsigned, bool sf)
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

/* ---- 指令大类模拟 ---- */

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

static __always_inline bool emu_system_option_insn(uint32_t option, uint32_t base)
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

static __always_inline enum emu_insn_result emu_simulate_system_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded, uint64_t pc)
{
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

#undef EMU_SYSTEM_OPTION_CASES
#undef EMU_SYSTEM_OPTION_CASE

static __always_inline enum emu_insn_result emu_simulate_branch_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded, uint64_t pc)
{
    switch (decoded->opcode)
    {
    case ARM64_OP_B:
    case ARM64_OP_BL:
        if (decoded->opcode == ARM64_OP_BL) regs->regs[30] = pc + 4;
        if (!arm64_decode_direct_target(decoded, pc, &regs->pc)) return EMU_INSN_SKIP;
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
            if (!arm64_decode_direct_target(decoded, pc, &regs->pc)) return EMU_INSN_SKIP;
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
            if (!arm64_decode_direct_target(decoded, pc, &regs->pc)) return EMU_INSN_SKIP;
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
            if (!arm64_decode_direct_target(decoded, pc, &regs->pc)) return EMU_INSN_SKIP;
        }
        else regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }
    default:
        return EMU_INSN_SKIP;
    }
}

#define EMU_LDST_MASK(B)     (~0ULL >> (64 - (B) * 8))
#define EMU_LDST_SX(V, B)    ((uint64_t)sign_extend64((uint64_t)(V), (B) * 8 - 1))
#define EMU_LDST_ST(A, B, V) emu_write_mem(mem_access, (A), (B), (V))

static __always_inline enum emu_insn_result emu_simulate_load_store_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded, uint64_t pc, const struct emu_mem_access *mem_access)
{
    const struct arm64_load_store_operands *operands = &decoded->operands.load_store;
    bool is_fp = (decoded->flags & ARM64_INSN_FLAG_FP) != 0;
    int bytes = operands->access_bytes;
    __uint128_t fp_regs[32];
    uint32_t fpsr = 0, fpcr = 0;
    bool fp_dirty = false;

    if (decoded->opcode == ARM64_OP_ATOMIC_RMW)
    {
        uint64_t mask = EMU_LDST_MASK(bytes);
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        uint64_t src = reg_read(regs, decoded->rs) & mask;
        uint64_t old, newval;
        __uint128_t mem;

        if (addr & (bytes - 1)) return EMU_INSN_FAULT;
        if (emu_read_mem(mem_access, addr, bytes, &mem)) return EMU_INSN_FAULT;

        old = (uint64_t)mem & mask;
        switch (decoded->operation)
        {
        case ARM64_OPERATION_LDADD:
            newval = emu_addsub_hw(old, src, false, false, decoded->operand_width == 64, &newval) & mask;
            break;
        case ARM64_OPERATION_LDCLR:
            newval = emu_logic_hw(old, src, 0, true, decoded->operand_width == 64, &newval) & mask;
            break;
        case ARM64_OPERATION_LDEOR:
            newval = emu_logic_hw(old, src, 2, false, decoded->operand_width == 64, &newval) & mask;
            break;
        case ARM64_OPERATION_LDSET:
            newval = emu_logic_hw(old, src, 1, false, decoded->operand_width == 64, &newval) & mask;
            break;
        case ARM64_OPERATION_LDSMAX:
            newval = emu_minmax_hw(EMU_LDST_SX(old, bytes), EMU_LDST_SX(src, bytes), false, false, true) & mask;
            break;
        case ARM64_OPERATION_LDSMIN:
            newval = emu_minmax_hw(EMU_LDST_SX(old, bytes), EMU_LDST_SX(src, bytes), true, false, true) & mask;
            break;
        case ARM64_OPERATION_LDUMAX:
            newval = emu_minmax_hw(old, src, false, true, true) & mask;
            break;
        case ARM64_OPERATION_LDUMIN:
            newval = emu_minmax_hw(old, src, true, true, true) & mask;
            break;
        case ARM64_OPERATION_SWP:
            newval = src;
            break;
        default:
            return EMU_INSN_SKIP;
        }

        if (decoded->flags & ARM64_INSN_FLAG_RELEASE) smp_mb();
        if (EMU_LDST_ST(addr, bytes, newval)) return EMU_INSN_FAULT;
        if (decoded->flags & ARM64_INSN_FLAG_ACQUIRE) smp_mb();

        reg_write(regs, decoded->rt, old, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_CAS)
    {
        uint64_t mask = EMU_LDST_MASK(bytes);
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        uint64_t expected = reg_read(regs, decoded->rs) & mask;
        uint64_t desired = reg_read(regs, decoded->rt) & mask;
        uint64_t old;
        __uint128_t mem;

        if (addr & (bytes - 1)) return EMU_INSN_FAULT;
        if (emu_read_mem(mem_access, addr, bytes, &mem)) return EMU_INSN_FAULT;

        old = (uint64_t)mem & mask;
        if (old == expected)
        {
            if (decoded->flags & ARM64_INSN_FLAG_RELEASE) smp_mb();
            if (EMU_LDST_ST(addr, bytes, desired)) return EMU_INSN_FAULT;
        }
        if (decoded->flags & ARM64_INSN_FLAG_ACQUIRE) smp_mb();

        reg_write(regs, decoded->rs, old, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_CASP)
    {
        int total = bytes * 2;
        uint64_t mask = EMU_LDST_MASK(bytes);
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        uint64_t old0, old1, exp0, exp1, new0, new1;
        __uint128_t mem0, mem1, pair;

        if (addr & (total - 1)) return EMU_INSN_FAULT;
        if (emu_read_mem(mem_access, addr, bytes, &mem0) || emu_read_mem(mem_access, addr + bytes, bytes, &mem1)) return EMU_INSN_FAULT;

        old0 = (uint64_t)mem0 & mask;
        old1 = (uint64_t)mem1 & mask;
        exp0 = reg_read(regs, decoded->rs) & mask;
        exp1 = reg_read(regs, decoded->rs + 1) & mask;
        new0 = reg_read(regs, decoded->rt) & mask;
        new1 = reg_read(regs, decoded->rt + 1) & mask;

        if (old0 == exp0 && old1 == exp1)
        {
            pair = ((__uint128_t)new1 << (bytes * 8)) | new0;
            if (decoded->flags & ARM64_INSN_FLAG_RELEASE) smp_mb();
            if (EMU_LDST_ST(addr, total, pair)) return EMU_INSN_FAULT;
        }
        if (decoded->flags & ARM64_INSN_FLAG_ACQUIRE) smp_mb();

        reg_write(regs, decoded->rs, old0, decoded->operand_width == 64);
        reg_write(regs, decoded->rs + 1, old1, decoded->operand_width == 64);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_EXCLUSIVE)
    {
        bool ordered = (decoded->flags & ARM64_INSN_FLAG_ORDERED) != 0;
        bool load = (decoded->flags & ARM64_INSN_FLAG_LOAD) != 0;
        bool pair = (decoded->flags & ARM64_INSN_FLAG_PAIR) != 0;
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        int total = pair ? bytes * 2 : bytes;
        __uint128_t val0;

        if (addr & (total - 1)) return EMU_INSN_FAULT;

        if (load)
        {
            if (emu_read_mem(mem_access, addr, bytes, &val0)) return EMU_INSN_FAULT;
            reg_write(regs, decoded->rt, (u64)val0, decoded->operand_width == 64);
            if (pair)
            {
                __uint128_t val1;

                if (emu_read_mem(mem_access, addr + bytes, bytes, &val1)) return EMU_INSN_FAULT;
                reg_write(regs, decoded->rt2, (u64)val1, decoded->operand_width == 64);
            }
            if (decoded->flags & ARM64_INSN_FLAG_ACQUIRE) smp_mb();
        }
        else
        {
            if (decoded->flags & ARM64_INSN_FLAG_RELEASE) smp_mb();
            if (emu_write_mem(mem_access, addr, bytes, reg_read(regs, decoded->rt))) return EMU_INSN_FAULT;
            if (pair && emu_write_mem(mem_access, addr + bytes, bytes, reg_read(regs, decoded->rt2))) return EMU_INSN_FAULT;
            if (!ordered) reg_write(regs, decoded->rs, 0, false);
        }

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_RCPC_UNSCALED)
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn) + operands->offset;

        if (decoded->flags & ARM64_INSN_FLAG_STORE)
        {
            smp_mb();
            if (emu_write_mem(mem_access, addr, bytes, reg_read(regs, decoded->rt))) return EMU_INSN_FAULT;
        }
        else
        {
            __uint128_t val;
            uint64_t raw;

            if (emu_read_mem(mem_access, addr, bytes, &val)) return EMU_INSN_FAULT;
            raw = (u64)val;
            if (decoded->flags & ARM64_INSN_FLAG_SIGN_EXTEND) raw = EMU_LDST_SX(raw, bytes);
            reg_write(regs, decoded->rt, raw, decoded->operand_width == 64);
            smp_mb();
        }
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_LDAPR)
    {
        uint64_t addr = addr_reg_read(regs, decoded->rn);
        __uint128_t val;

        if (addr & (bytes - 1)) return EMU_INSN_FAULT;
        if (emu_read_mem(mem_access, addr, bytes, &val)) return EMU_INSN_FAULT;
        reg_write(regs, decoded->rt, (u64)val, decoded->operand_width == 64);
        smp_mb();
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (is_fp)
    {
        int i;

        for (i = 0; i < 32; i++) read_q_reg(i, &fp_regs[i]);
        fpsr = read_fpsr();
        fpcr = read_fpcr();
    }

    if (decoded->opcode == ARM64_OP_PREFETCH || decoded->opcode == ARM64_OP_PREFETCH_LITERAL) return EMU_INSN_NOP;

    if (decoded->opcode == ARM64_OP_LOAD_LITERAL)
    {
        struct arm64_memory_address memory_address;
        uint64_t addr;

        if (!arm64_decode_memory_address(decoded, pc, 0, 0, &memory_address)) return EMU_INSN_SKIP;
        addr = memory_address.address;

        if (is_fp)
        {
            __uint128_t val;

            if (emu_read_mem(mem_access, addr, bytes, &val)) return EMU_INSN_FAULT;
            fp_regs[decoded->rt] = val;
            fp_dirty = true;
        }
        else
        {
            __uint128_t val;

            if (emu_read_mem(mem_access, addr, bytes, &val)) return EMU_INSN_FAULT;
            reg_write(regs, decoded->rt, decoded->flags & ARM64_INSN_FLAG_SIGN_EXTEND ? EMU_LDST_SX(val, bytes) : (u64)val, decoded->operand_width == 64);
        }
        goto done_ldst;
    }

    if (decoded->opcode == ARM64_OP_LOAD_STORE_PAIR)
    {
        struct arm64_memory_address memory_address;
        bool load = (decoded->flags & ARM64_INSN_FLAG_LOAD) != 0;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t addr;

        if (!arm64_decode_memory_address(decoded, pc, base, 0, &memory_address)) return EMU_INSN_SKIP;
        addr = memory_address.address;

        if (load)
        {
            __uint128_t val1, val2;

            if (emu_read_mem(mem_access, addr, bytes, &val1) || emu_read_mem(mem_access, addr + bytes, bytes, &val2)) return EMU_INSN_FAULT;
            if (is_fp)
            {
                fp_regs[decoded->rt] = val1;
                fp_regs[decoded->rt2] = val2;
                fp_dirty = true;
            }
            else if (decoded->flags & ARM64_INSN_FLAG_SIGN_EXTEND)
            {
                reg_write(regs, decoded->rt, EMU_LDST_SX(val1, bytes), true);
                reg_write(regs, decoded->rt2, EMU_LDST_SX(val2, bytes), true);
            }
            else
            {
                reg_write(regs, decoded->rt, (u64)val1, decoded->operand_width == 64);
                reg_write(regs, decoded->rt2, (u64)val2, decoded->operand_width == 64);
            }
        }
        else
        {
            __uint128_t val1 = is_fp ? fp_regs[decoded->rt] : (__uint128_t)reg_read(regs, decoded->rt);
            __uint128_t val2 = is_fp ? fp_regs[decoded->rt2] : (__uint128_t)reg_read(regs, decoded->rt2);

            if (EMU_LDST_ST(addr, bytes, val1) || EMU_LDST_ST(addr + bytes, bytes, val2)) return EMU_INSN_FAULT;
        }
        if (memory_address.writeback) addr_reg_write(regs, decoded->rn, memory_address.writeback_address);
        goto done_ldst;
    }

    if (decoded->opcode != ARM64_OP_LOAD_STORE_SINGLE) return EMU_INSN_SKIP;
    if (decoded->flags & ARM64_INSN_FLAG_UNPRIVILEGED) return EMU_INSN_SKIP;

    {
        struct arm64_memory_address memory_address;
        uint64_t base = addr_reg_read(regs, decoded->rn);
        uint64_t index = operands->address_mode == ARM64_ADDRESS_REGISTER_OFFSET ? reg_read(regs, decoded->rm) : 0;
        uint64_t addr;

        if (!arm64_decode_memory_address(decoded, pc, base, index, &memory_address)) return EMU_INSN_SKIP;
        addr = memory_address.address;

        if (decoded->flags & ARM64_INSN_FLAG_LOAD)
        {
            __uint128_t val;

            if (emu_read_mem(mem_access, addr, bytes, &val)) return EMU_INSN_FAULT;
            if (is_fp)
            {
                fp_regs[decoded->rt] = val;
                fp_dirty = true;
            }
            else
            {
                u64 raw = (u64)val;
                if (decoded->flags & ARM64_INSN_FLAG_SIGN_EXTEND) raw = EMU_LDST_SX(raw, bytes);
                reg_write(regs, decoded->rt, raw, decoded->operand_width == 64);
            }
        }
        else
        {
            __uint128_t val = is_fp ? fp_regs[decoded->rt] : (__uint128_t)reg_read(regs, decoded->rt);

            if (EMU_LDST_ST(addr, bytes, val)) return EMU_INSN_FAULT;
        }
        if (memory_address.writeback) addr_reg_write(regs, decoded->rn, memory_address.writeback_address);
    }

done_ldst:
    if (is_fp && fp_dirty)
    {
        int i;

        for (i = 0; i < 32; i++) write_q_reg(i, &fp_regs[i]);
        write_fpsr(fpsr);
        write_fpcr(fpcr);
    }
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

#undef EMU_LDST_ST
#undef EMU_LDST_SX
#undef EMU_LDST_MASK

/* FP / AdvSIMD 只保留装载现场、执行同语义指令、写回结果的局部缩写。 */
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

#define EMU_FP_CONVERT_SIMD(INST, DST, A, WIDTH, ELEMENT_WIDTH)                                    \
    do                                                                                             \
    {                                                                                              \
        if ((WIDTH) == 32 && (ELEMENT_WIDTH) == 32) EMU_FP_UN(INST " s0, s1", DST, A);             \
        else if ((WIDTH) == 64 && (ELEMENT_WIDTH) == 64) EMU_FP_UN(INST " d0, d1", DST, A);        \
        else if ((WIDTH) == 64 && (ELEMENT_WIDTH) == 32) EMU_FP_UN(INST " v0.2s, v1.2s", DST, A);  \
        else if ((WIDTH) == 128 && (ELEMENT_WIDTH) == 32) EMU_FP_UN(INST " v0.4s, v1.4s", DST, A); \
        else if ((WIDTH) == 128 && (ELEMENT_WIDTH) == 64) EMU_FP_UN(INST " v0.2d, v1.2d", DST, A); \
        else return EMU_INSN_SKIP;                                                                 \
    } while (0)

#define EMU_FP_CONVERT_GPR(INST)                                                                                                                          \
    do                                                                                                                                                    \
    {                                                                                                                                                     \
        if (decoded->operand_width == 32 && operands->element_width == 32)                                                                                \
        {                                                                                                                                                 \
            asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n" INST " %w0, s1\n" : "=r"(wout) : "r"(&fp_regs[rn]) : "memory", "v1"); \
            reg_write(regs, rd, wout, false);                                                                                                             \
        }                                                                                                                                                 \
        else if (decoded->operand_width == 64 && operands->element_width == 32)                                                                           \
        {                                                                                                                                                 \
            asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n" INST " %0, s1\n" : "=r"(xout) : "r"(&fp_regs[rn]) : "memory", "v1");  \
            reg_write(regs, rd, xout, true);                                                                                                              \
        }                                                                                                                                                 \
        else if (decoded->operand_width == 32 && operands->element_width == 64)                                                                           \
        {                                                                                                                                                 \
            asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n" INST " %w0, d1\n" : "=r"(wout) : "r"(&fp_regs[rn]) : "memory", "v1"); \
            reg_write(regs, rd, wout, false);                                                                                                             \
        }                                                                                                                                                 \
        else if (decoded->operand_width == 64 && operands->element_width == 64)                                                                           \
        {                                                                                                                                                 \
            asm volatile(".arch_extension fp\n.arch_extension simd\nldr q1, [%1]\n" INST " %0, d1\n" : "=r"(xout) : "r"(&fp_regs[rn]) : "memory", "v1");  \
            reg_write(regs, rd, xout, true);                                                                                                              \
        }                                                                                                                                                 \
        else return EMU_INSN_SKIP;                                                                                                                        \
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

static __always_inline bool emu_fp_select_hw(void *dst, const void *left, const void *right, uint64_t pstate, uint32_t condition, uint32_t width)
{
    uint32_t take = emu_cond_holds_hw(pstate, condition);

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

static __always_inline uint64_t emu_simd_lane_mask(uint32_t element_width)
{
    return element_width == 64 ? ~0ULL : (1ULL << element_width) - 1;
}

static __always_inline uint64_t emu_simd_extract_lane(__uint128_t value, uint32_t element_width, uint32_t lane)
{
    return (uint64_t)(value >> (lane * element_width)) & emu_simd_lane_mask(element_width);
}

static __always_inline __uint128_t emu_simd_insert_lane(__uint128_t vector, uint64_t value, uint32_t element_width, uint32_t lane)
{
    __uint128_t mask = (__uint128_t)emu_simd_lane_mask(element_width) << (lane * element_width);

    return (vector & ~mask) | (((__uint128_t)value << (lane * element_width)) & mask);
}

static __always_inline __uint128_t emu_simd_replicate_lane(uint64_t value, uint32_t element_width, uint32_t vector_width)
{
    __uint128_t result = 0;
    uint32_t lane;

    for (lane = 0; lane < vector_width / element_width; lane++) result = emu_simd_insert_lane(result, value, element_width, lane);
    return result;
}

#define EMU_SIMD_FP_BY_ELEMENT_EXEC(DST_ARR, SRC_ARR)                                  \
    do                                                                                 \
    {                                                                                  \
        switch (operation)                                                             \
        {                                                                              \
        case ARM64_SIMD_OP_FMLA_BY_ELEMENT:                                            \
            EMU_VEC_ACC("fmla " DST_ARR ", " SRC_ARR, &result, &left_value, &element); \
            break;                                                                     \
        case ARM64_SIMD_OP_FMLS_BY_ELEMENT:                                            \
            EMU_VEC_ACC("fmls " DST_ARR ", " SRC_ARR, &result, &left_value, &element); \
            break;                                                                     \
        case ARM64_SIMD_OP_FMUL_BY_ELEMENT:                                            \
            EMU_FP_BIN("fmul " DST_ARR ", " SRC_ARR, &result, &left_value, &element);  \
            break;                                                                     \
        case ARM64_SIMD_OP_FMULX_BY_ELEMENT:                                           \
            EMU_FP_BIN("fmulx " DST_ARR ", " SRC_ARR, &result, &left_value, &element); \
            break;                                                                     \
        default:                                                                       \
            return false;                                                              \
        }                                                                              \
    } while (0)

#define EMU_SIMD_FP16_BY_ELEMENT_INST(INSTRUCTION)              \
    do                                                          \
    {                                                           \
        asm volatile(".arch_extension fp\n"                    \
                     ".arch_extension simd\n"                  \
                     "ldr q0, [%0]\n"                          \
                     "ldr q1, [%1]\n"                          \
                     "ldr q2, [%2]\n"                          \
                     ".inst " __stringify(INSTRUCTION) "\n"   \
                     "str q0, [%0]\n"                          \
                     :                                          \
                     : "r"(&result), "r"(&left_value), "r"(&element) \
                     : "memory", "v0", "v1", "v2");         \
    } while (0)

#define EMU_SIMD_FP16_BY_ELEMENT_EXEC(FMLA_INST, FMLS_INST, FMUL_INST, FMULX_INST) \
    do                                                                             \
    {                                                                              \
        switch (operation)                                                         \
        {                                                                          \
        case ARM64_SIMD_OP_FMLA_BY_ELEMENT:                                        \
            EMU_SIMD_FP16_BY_ELEMENT_INST(FMLA_INST);                              \
            break;                                                                 \
        case ARM64_SIMD_OP_FMLS_BY_ELEMENT:                                        \
            EMU_SIMD_FP16_BY_ELEMENT_INST(FMLS_INST);                              \
            break;                                                                 \
        case ARM64_SIMD_OP_FMUL_BY_ELEMENT:                                        \
            EMU_SIMD_FP16_BY_ELEMENT_INST(FMUL_INST);                              \
            break;                                                                 \
        case ARM64_SIMD_OP_FMULX_BY_ELEMENT:                                       \
            EMU_SIMD_FP16_BY_ELEMENT_INST(FMULX_INST);                             \
            break;                                                                 \
        default:                                                                   \
            return false;                                                          \
        }                                                                          \
    } while (0)

static __always_inline bool emu_simd_fp_by_element_hw(enum arm64_simd_operation operation, void *dst, const void *left, uint64_t lane_value, uint32_t element_width, uint32_t operand_width)
{
    __uint128_t result = *(__uint128_t *)dst;
    __uint128_t left_value = *(const __uint128_t *)left;
    __uint128_t element;

    if (operand_width == element_width)
    {
        element = lane_value;
        if (element_width == 16) EMU_SIMD_FP16_BY_ELEMENT_EXEC(0x5F021020, 0x5F025020, 0x5F029020, 0x7F029020);
        else if (element_width == 32) EMU_SIMD_FP_BY_ELEMENT_EXEC("s0, s1", "v2.s[0]");
        else if (element_width == 64) EMU_SIMD_FP_BY_ELEMENT_EXEC("d0, d1", "v2.d[0]");
        else return false;
    }
    else
    {
        element = emu_simd_replicate_lane(lane_value, element_width, operand_width);
        if (operand_width == 64 && element_width == 16) EMU_SIMD_FP16_BY_ELEMENT_EXEC(0x0E420C20, 0x0EC20C20, 0x2E421C20, 0x0E421C20);
        else if (operand_width == 128 && element_width == 16) EMU_SIMD_FP16_BY_ELEMENT_EXEC(0x4E420C20, 0x4EC20C20, 0x6E421C20, 0x4E421C20);
        else if (operand_width == 64 && element_width == 32) EMU_SIMD_FP_BY_ELEMENT_EXEC("v0.2s, v1.2s", "v2.2s");
        else if (operand_width == 128 && element_width == 32) EMU_SIMD_FP_BY_ELEMENT_EXEC("v0.4s, v1.4s", "v2.4s");
        else if (operand_width == 128 && element_width == 64) EMU_SIMD_FP_BY_ELEMENT_EXEC("v0.2d, v1.2d", "v2.2d");
        else return false;
    }

    *(__uint128_t *)dst = result;
    return true;
}

#undef EMU_SIMD_FP16_BY_ELEMENT_EXEC
#undef EMU_SIMD_FP16_BY_ELEMENT_INST
#undef EMU_SIMD_FP_BY_ELEMENT_EXEC

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

static __always_inline bool emu_simd_dup_general_hw(void *dst, uint64_t value, uint32_t element_width, uint32_t vector_width)
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

#undef EMU_SIMD_DUP_GENERAL_EXEC

static __always_inline int64_t emu_simd_sign_extend_lane(uint64_t value, uint32_t element_width)
{
    uint64_t sign;

    if (element_width == 64) return (int64_t)value;
    sign = 1ULL << (element_width - 1);
    return (int64_t)((value ^ sign) - sign);
}

#define EMU_SIMD_INTEGER_3REG_EXEC(INST)                                   \
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

static __always_inline bool emu_simd_integer_3reg_hw(enum arm64_simd_operation operation, void *dst, const void *left, const void *right, uint32_t element_width, uint32_t vector_width)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_ADD:
        EMU_SIMD_INTEGER_3REG_EXEC("add");
    case ARM64_SIMD_OP_SUB:
        EMU_SIMD_INTEGER_3REG_EXEC("sub");
    case ARM64_SIMD_OP_CMEQ:
        EMU_SIMD_INTEGER_3REG_EXEC("cmeq");
    case ARM64_SIMD_OP_CMGT:
        EMU_SIMD_INTEGER_3REG_EXEC("cmgt");
    case ARM64_SIMD_OP_CMGE:
        EMU_SIMD_INTEGER_3REG_EXEC("cmge");
    case ARM64_SIMD_OP_CMHI:
        EMU_SIMD_INTEGER_3REG_EXEC("cmhi");
    case ARM64_SIMD_OP_CMHS:
        EMU_SIMD_INTEGER_3REG_EXEC("cmhs");
    default:
        return false;
    }
}

#undef EMU_SIMD_INTEGER_3REG_EXEC

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

static __always_inline bool emu_simd_shift_hw(enum arm64_simd_operation operation, void *dst, const void *source, uint32_t element_width, uint32_t vector_width, uint32_t shift)
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

#undef EMU_SIMD_SHIFT_EXEC

#define EMU_SIMD_EXT_CASE(N, ARR)                                                     \
    case N:                                                                           \
        EMU_FP_BIN("ext v0." ARR ", v1." ARR ", v2." ARR ", #" #N, dst, left, right); \
        return true

static __always_inline bool emu_simd_ext_hw(void *dst, const void *left, const void *right, uint32_t vector_width, uint32_t byte_offset)
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

#undef EMU_SIMD_EXT_CASE

static __always_inline enum emu_insn_result emu_simulate_fp_simd_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded, uint64_t pc)
{
    const struct arm64_simd_operands *operands = &decoded->operands.simd;
    __uint128_t fp_regs[32];
    uint32_t fpsr, fpcr;
    enum emu_insn_result result = EMU_INSN_SKIP;
    int i;

    for (i = 0; i < 32; i++) read_q_reg(i, &fp_regs[i]);
    fpsr = read_fpsr();
    fpcr = read_fpcr();
    write_fpsr(fpsr);
    write_fpcr(fpcr);

    if (operands->group == ARM64_SIMD_GROUP_VECTOR_MODIFIED_IMMEDIATE)
    {
        __uint128_t immediate = operands->expanded_immediate;

        if (decoded->operand_width == 128) immediate |= immediate << 64;
        switch (operands->operation)
        {
        case ARM64_SIMD_OP_MOVI:
        case ARM64_SIMD_OP_FMOV_IMMEDIATE:
            fp_regs[decoded->rd] = immediate;
            break;
        case ARM64_SIMD_OP_MVNI:
            if (decoded->operand_width == 64) EMU_FP_UN("mvn v0.8b, v1.8b", &fp_regs[decoded->rd], &immediate);
            else EMU_FP_UN("mvn v0.16b, v1.16b", &fp_regs[decoded->rd], &immediate);
            break;
        case ARM64_SIMD_OP_ORR_IMMEDIATE:
            if (decoded->operand_width == 64) EMU_FP_BIN("orr v0.8b, v1.8b, v2.8b", &fp_regs[decoded->rd], &fp_regs[decoded->rd], &immediate);
            else EMU_FP_BIN("orr v0.16b, v1.16b, v2.16b", &fp_regs[decoded->rd], &fp_regs[decoded->rd], &immediate);
            break;
        case ARM64_SIMD_OP_BIC_IMMEDIATE:
            if (decoded->operand_width == 64) EMU_FP_BIN("bic v0.8b, v1.8b, v2.8b", &fp_regs[decoded->rd], &fp_regs[decoded->rd], &immediate);
            else EMU_FP_BIN("bic v0.16b, v1.16b, v2.16b", &fp_regs[decoded->rd], &fp_regs[decoded->rd], &immediate);
            break;
        default:
            return EMU_INSN_SKIP;
        }
        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_SCALAR_COPY)
    {
        uint64_t lane_value;

        if (operands->operation != ARM64_SIMD_OP_DUP_ELEMENT) return EMU_INSN_SKIP;
        lane_value = emu_simd_extract_lane(fp_regs[decoded->rn], operands->element_width, operands->lane_index);
        fp_regs[decoded->rd] = emu_simd_replicate_lane(lane_value, operands->element_width, decoded->operand_width);
        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_VECTOR_COPY)
    {
        uint32_t element_width = operands->element_width;
        uint64_t lane_value;

        switch (operands->operation)
        {
        case ARM64_SIMD_OP_DUP_GENERAL:
            if (!emu_simd_dup_general_hw(&fp_regs[decoded->rd], reg_read(regs, decoded->rn), element_width, decoded->operand_width)) return EMU_INSN_SKIP;
            break;
        case ARM64_SIMD_OP_DUP_ELEMENT:
            lane_value = emu_simd_extract_lane(fp_regs[decoded->rn], element_width, operands->lane_index);
            fp_regs[decoded->rd] = emu_simd_replicate_lane(lane_value, element_width, decoded->operand_width);
            break;
        case ARM64_SIMD_OP_INS_GENERAL:
            fp_regs[decoded->rd] = emu_simd_insert_lane(fp_regs[decoded->rd], reg_read(regs, decoded->rn), element_width, operands->lane_index);
            break;
        case ARM64_SIMD_OP_INS_ELEMENT:
            lane_value = emu_simd_extract_lane(fp_regs[decoded->rn], element_width, operands->source_lane_index);
            fp_regs[decoded->rd] = emu_simd_insert_lane(fp_regs[decoded->rd], lane_value, element_width, operands->lane_index);
            break;
        case ARM64_SIMD_OP_UMOV:
            lane_value = emu_simd_extract_lane(fp_regs[decoded->rn], element_width, operands->lane_index);
            reg_write(regs, decoded->rd, lane_value, decoded->operand_width == 64);
            break;
        case ARM64_SIMD_OP_SMOV:
            lane_value = emu_simd_extract_lane(fp_regs[decoded->rn], element_width, operands->lane_index);
            reg_write(regs, decoded->rd, (uint64_t)emu_simd_sign_extend_lane(lane_value, element_width), decoded->operand_width == 64);
            break;
        default:
            return EMU_INSN_SKIP;
        }
        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_VECTOR_SHIFT_IMMEDIATE)
    {
        if (!emu_simd_shift_hw(operands->operation, &fp_regs[decoded->rd], &fp_regs[decoded->rn], operands->element_width, decoded->operand_width, operands->immediate)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_FP_BY_ELEMENT)
    {
        uint64_t lane_value = emu_simd_extract_lane(fp_regs[decoded->rm], operands->element_width, operands->lane_index);

        if (!emu_simd_fp_by_element_hw(operands->operation, &fp_regs[decoded->rd], &fp_regs[decoded->rn], lane_value, operands->element_width, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_VECTOR_3REG && operands->operation >= ARM64_SIMD_OP_ADD && operands->operation <= ARM64_SIMD_OP_CMHS)
    {
        if (!emu_simd_integer_3reg_hw(operands->operation, &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], operands->element_width, decoded->operand_width)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_EXT)
    {
        if (operands->immediate >= decoded->operand_width / 8) return EMU_INSN_SKIP;
        if (!emu_simd_ext_hw(&fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], decoded->operand_width, operands->immediate)) return EMU_INSN_SKIP;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_SCALAR_IMMEDIATE)
    {
        if (operands->operation != ARM64_SIMD_OP_FMOV_IMMEDIATE) return EMU_INSN_SKIP;
        fp_regs[decoded->rd] = decoded->operand_width == 64 ? operands->expanded_immediate : (uint32_t)operands->expanded_immediate;
        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_SCALAR_2SOURCE)
    {
        if (decoded->operand_width == 32)
        {
            switch (operands->operation)
            {
            case ARM64_SIMD_OP_FMUL:
                EMU_FP_BIN("fmul s0, s1, s2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FDIV:
                EMU_FP_BIN("fdiv s0, s1, s2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FADD:
                EMU_FP_BIN("fadd s0, s1, s2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FSUB:
                EMU_FP_BIN("fsub s0, s1, s2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMAX:
                EMU_FP_BIN("fmax s0, s1, s2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMIN:
                EMU_FP_BIN("fmin s0, s1, s2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMAXNM:
                EMU_FP_BIN("fmaxnm s0, s1, s2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMINNM:
                EMU_FP_BIN("fminnm s0, s1, s2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FNMUL:
                EMU_FP_BIN("fnmul s0, s1, s2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }
        else
        {
            switch (operands->operation)
            {
            case ARM64_SIMD_OP_FMUL:
                EMU_FP_BIN("fmul d0, d1, d2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FDIV:
                EMU_FP_BIN("fdiv d0, d1, d2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FADD:
                EMU_FP_BIN("fadd d0, d1, d2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FSUB:
                EMU_FP_BIN("fsub d0, d1, d2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMAX:
                EMU_FP_BIN("fmax d0, d1, d2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMIN:
                EMU_FP_BIN("fmin d0, d1, d2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMAXNM:
                EMU_FP_BIN("fmaxnm d0, d1, d2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FMINNM:
                EMU_FP_BIN("fminnm d0, d1, d2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            case ARM64_SIMD_OP_FNMUL:
                EMU_FP_BIN("fnmul d0, d1, d2", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm]);
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }

        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_SCALAR_1SOURCE)
    {
        if (decoded->operand_width == 32)
        {
            switch (operands->operation)
            {
            case ARM64_SIMD_OP_FMOV:
                fp_regs[decoded->rd] = (uint32_t)fp_regs[decoded->rn];
                break;
            case ARM64_SIMD_OP_FABS:
                EMU_FP_UN("fabs s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FNEG:
                EMU_FP_UN("fneg s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FSQRT:
                EMU_FP_UN("fsqrt s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FRINT:
                switch (operands->rounding_mode)
                {
                case ARM64_FP_ROUND_NEAREST_EVEN:
                    EMU_FP_UN("frintn s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_PLUS_INFINITY:
                    EMU_FP_UN("frintp s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_MINUS_INFINITY:
                    EMU_FP_UN("frintm s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_ZERO:
                    EMU_FP_UN("frintz s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_NEAREST_AWAY:
                    EMU_FP_UN("frinta s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_CURRENT_EXACT:
                    EMU_FP_UN("frintx s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_CURRENT:
                    EMU_FP_UN("frinti s0, s1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                default:
                    return EMU_INSN_SKIP;
                }
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }
        else
        {
            switch (operands->operation)
            {
            case ARM64_SIMD_OP_FMOV:
                fp_regs[decoded->rd] = (uint64_t)fp_regs[decoded->rn];
                break;
            case ARM64_SIMD_OP_FABS:
                EMU_FP_UN("fabs d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FNEG:
                EMU_FP_UN("fneg d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FSQRT:
                EMU_FP_UN("fsqrt d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                break;
            case ARM64_SIMD_OP_FRINT:
                switch (operands->rounding_mode)
                {
                case ARM64_FP_ROUND_NEAREST_EVEN:
                    EMU_FP_UN("frintn d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_PLUS_INFINITY:
                    EMU_FP_UN("frintp d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_MINUS_INFINITY:
                    EMU_FP_UN("frintm d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_ZERO:
                    EMU_FP_UN("frintz d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_NEAREST_AWAY:
                    EMU_FP_UN("frinta d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_CURRENT_EXACT:
                    EMU_FP_UN("frintx d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                case ARM64_FP_ROUND_CURRENT:
                    EMU_FP_UN("frinti d0, d1", &fp_regs[decoded->rd], &fp_regs[decoded->rn]);
                    break;
                default:
                    return EMU_INSN_SKIP;
                }
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }

        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_SCALAR_3SOURCE)
    {
        if (decoded->operand_width == 32)
        {
            if (operands->operation == ARM64_SIMD_OP_FMADD) EMU_FP_TERN("fmadd s0, s1, s2, s3", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], &fp_regs[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FMSUB) EMU_FP_TERN("fmsub s0, s1, s2, s3", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], &fp_regs[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FNMADD) EMU_FP_TERN("fnmadd s0, s1, s2, s3", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], &fp_regs[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FNMSUB) EMU_FP_TERN("fnmsub s0, s1, s2, s3", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], &fp_regs[decoded->ra]);
            else return EMU_INSN_SKIP;
        }
        else
        {
            if (operands->operation == ARM64_SIMD_OP_FMADD) EMU_FP_TERN("fmadd d0, d1, d2, d3", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], &fp_regs[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FMSUB) EMU_FP_TERN("fmsub d0, d1, d2, d3", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], &fp_regs[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FNMADD) EMU_FP_TERN("fnmadd d0, d1, d2, d3", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], &fp_regs[decoded->ra]);
            else if (operands->operation == ARM64_SIMD_OP_FNMSUB) EMU_FP_TERN("fnmsub d0, d1, d2, d3", &fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], &fp_regs[decoded->ra]);
            else return EMU_INSN_SKIP;
        }

        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_SCALAR_COMPARE)
    {
        bool zero = (operands->flags & ARM64_SIMD_FLAG_COMPARE_ZERO) != 0;
        bool signal = operands->operation == ARM64_SIMD_OP_FCMPE;
        uint64_t nzcv;

        if (decoded->operand_width == 32)
        {
            if (zero)
            {
                if (signal)
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmpe s1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[decoded->rn])
                                 : "memory", "cc", "v1");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmp s1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[decoded->rn])
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
                                 : "r"(&fp_regs[decoded->rn]), "r"(&fp_regs[decoded->rm])
                                 : "memory", "cc", "v1", "v2");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "ldr q2, [%2]\n"
                                 "fcmp s1, s2\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[decoded->rn]), "r"(&fp_regs[decoded->rm])
                                 : "memory", "cc", "v1", "v2");
            }
        }
        else
        {
            if (zero)
            {
                if (signal)
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmpe d1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[decoded->rn])
                                 : "memory", "cc", "v1");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmp d1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[decoded->rn])
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
                                 : "r"(&fp_regs[decoded->rn]), "r"(&fp_regs[decoded->rm])
                                 : "memory", "cc", "v1", "v2");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "ldr q2, [%2]\n"
                                 "fcmp d1, d2\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[decoded->rn]), "r"(&fp_regs[decoded->rm])
                                 : "memory", "cc", "v1", "v2");
            }
        }

        emu_write_nzcv(regs, nzcv);
        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_SCALAR_CONDITIONAL_COMPARE)
    {
        bool signal = operands->operation == ARM64_SIMD_OP_FCCMPE;
        uint64_t nzcv;

        if (!emu_cond_holds_hw(regs->pstate, operands->condition))
        {
            emu_write_nzcv(regs, (uint64_t)operands->immediate << 28);
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
                             : "r"(&fp_regs[decoded->rn]), "r"(&fp_regs[decoded->rm])
                             : "memory", "cc", "v1", "v2");
            else
                asm volatile(".arch_extension fp\n.arch_extension simd\n"
                             "ldr q1, [%1]\n"
                             "ldr q2, [%2]\n"
                             "fcmp s1, s2\n"
                             "mrs %0, nzcv\n"
                             : "=r"(nzcv)
                             : "r"(&fp_regs[decoded->rn]), "r"(&fp_regs[decoded->rm])
                             : "memory", "cc", "v1", "v2");
            emu_write_nzcv(regs, nzcv);
            result = EMU_INSN_HANDLED;
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
                             : "r"(&fp_regs[decoded->rn]), "r"(&fp_regs[decoded->rm])
                             : "memory", "cc", "v1", "v2");
            else
                asm volatile(".arch_extension fp\n.arch_extension simd\n"
                             "ldr q1, [%1]\n"
                             "ldr q2, [%2]\n"
                             "fcmp d1, d2\n"
                             "mrs %0, nzcv\n"
                             : "=r"(nzcv)
                             : "r"(&fp_regs[decoded->rn]), "r"(&fp_regs[decoded->rm])
                             : "memory", "cc", "v1", "v2");
            emu_write_nzcv(regs, nzcv);
            result = EMU_INSN_HANDLED;
        }
    }
    else if (operands->group == ARM64_SIMD_GROUP_SCALAR_SELECT)
    {
        if (!emu_fp_select_hw(&fp_regs[decoded->rd], &fp_regs[decoded->rn], &fp_regs[decoded->rm], regs->pstate, operands->condition, decoded->operand_width)) return EMU_INSN_SKIP;

        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_FMOV_GENERAL)
    {
        bool sf = decoded->operand_width == 64;
        bool gp_to_fp = operands->operation == ARM64_SIMD_OP_FMOV_GENERAL_TO_FP;

        if (gp_to_fp)
        {
            if (sf) fp_regs[decoded->rd] = reg_read(regs, decoded->rn);
            else fp_regs[decoded->rd] = (uint32_t)reg_read(regs, decoded->rn);
        }
        else
        {
            if (sf) reg_write(regs, decoded->rd, (uint64_t)fp_regs[decoded->rn], true);
            else reg_write(regs, decoded->rd, (uint32_t)fp_regs[decoded->rn], false);
        }

        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_CONVERT)
    {
        enum arm64_simd_operation operation = operands->operation;
        uint32_t rd = decoded->rd;
        uint32_t rn = decoded->rn;
        uint32_t wout;
        uint64_t xout;

        switch (operation)
        {
        case ARM64_SIMD_OP_SCVTF_S_W:
            asm volatile(".arch_extension fp\n"
                         "scvtf s0, %w1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn))
                         : "memory", "v0");
            break;
        case ARM64_SIMD_OP_SCVTF_S_X:
            asm volatile(".arch_extension fp\n"
                         "scvtf s0, %1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn))
                         : "memory", "v0");
            break;
        case ARM64_SIMD_OP_SCVTF_D_W:
            asm volatile(".arch_extension fp\n"
                         "scvtf d0, %w1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn))
                         : "memory", "v0");
            break;
        case ARM64_SIMD_OP_SCVTF_D_X:
            asm volatile(".arch_extension fp\n"
                         "scvtf d0, %1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn))
                         : "memory", "v0");
            break;
        case ARM64_SIMD_OP_UCVTF_S_W:
            asm volatile(".arch_extension fp\n"
                         "ucvtf s0, %w1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn))
                         : "memory", "v0");
            break;
        case ARM64_SIMD_OP_UCVTF_S_X:
            asm volatile(".arch_extension fp\n"
                         "ucvtf s0, %1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn))
                         : "memory", "v0");
            break;
        case ARM64_SIMD_OP_UCVTF_D_W:
            asm volatile(".arch_extension fp\n"
                         "ucvtf d0, %w1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn))
                         : "memory", "v0");
            break;
        case ARM64_SIMD_OP_UCVTF_D_X:
            asm volatile(".arch_extension fp\n"
                         "ucvtf d0, %1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn))
                         : "memory", "v0");
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
            EMU_FP_UN("fcvt s0, d1", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FCVT_D_S:
            EMU_FP_UN("fcvt d0, s1", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD:
            switch (operands->rounding_mode)
            {
            case ARM64_FP_ROUND_NEAREST_EVEN:
                EMU_FP_CONVERT_SIMD("fcvtns", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_PLUS_INFINITY:
                EMU_FP_CONVERT_SIMD("fcvtps", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_MINUS_INFINITY:
                EMU_FP_CONVERT_SIMD("fcvtms", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_ZERO:
                EMU_FP_CONVERT_SIMD("fcvtzs", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_NEAREST_AWAY:
                EMU_FP_CONVERT_SIMD("fcvtas", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            default:
                return EMU_INSN_SKIP;
            }
            break;
        case ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD:
            switch (operands->rounding_mode)
            {
            case ARM64_FP_ROUND_NEAREST_EVEN:
                EMU_FP_CONVERT_SIMD("fcvtnu", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_PLUS_INFINITY:
                EMU_FP_CONVERT_SIMD("fcvtpu", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_MINUS_INFINITY:
                EMU_FP_CONVERT_SIMD("fcvtmu", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_ZERO:
                EMU_FP_CONVERT_SIMD("fcvtzu", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            case ARM64_FP_ROUND_NEAREST_AWAY:
                EMU_FP_CONVERT_SIMD("fcvtau", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
                break;
            default:
                return EMU_INSN_SKIP;
            }
            break;
        case ARM64_SIMD_OP_SCVTF_SIMD:
            EMU_FP_CONVERT_SIMD("scvtf", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
            break;
        case ARM64_SIMD_OP_UCVTF_SIMD:
            EMU_FP_CONVERT_SIMD("ucvtf", &fp_regs[rd], &fp_regs[rn], decoded->operand_width, operands->element_width);
            break;
        default:
            return EMU_INSN_SKIP;
        }

        result = EMU_INSN_HANDLED;
    }
    else if (operands->group == ARM64_SIMD_GROUP_VECTOR_2REG || operands->group == ARM64_SIMD_GROUP_VECTOR_3REG)
    {
        enum arm64_simd_operation operation = operands->operation;
        uint32_t rd = decoded->rd;
        uint32_t rn = decoded->rn;
        uint32_t rm = decoded->rm;

        switch (operation)
        {
        case ARM64_SIMD_OP_FADD_V2S:
            EMU_FP_BIN("fadd v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FADD_V4S:
            EMU_FP_BIN("fadd v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FADD_V2D:
            EMU_FP_BIN("fadd v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FSUB_V2S:
            EMU_FP_BIN("fsub v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FSUB_V4S:
            EMU_FP_BIN("fsub v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FSUB_V2D:
            EMU_FP_BIN("fsub v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMUL_V2S:
            EMU_FP_BIN("fmul v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMUL_V4S:
            EMU_FP_BIN("fmul v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMUL_V2D:
            EMU_FP_BIN("fmul v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FDIV_V2S:
            EMU_FP_BIN("fdiv v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FDIV_V4S:
            EMU_FP_BIN("fdiv v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FDIV_V2D:
            EMU_FP_BIN("fdiv v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMLA_V4S:
            EMU_VEC_ACC("fmla v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMLS_V4S:
            EMU_VEC_ACC("fmls v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMAX_V2S:
            EMU_FP_BIN("fmax v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMAX_V4S:
            EMU_FP_BIN("fmax v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMAX_V2D:
            EMU_FP_BIN("fmax v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMIN_V2S:
            EMU_FP_BIN("fmin v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMIN_V4S:
            EMU_FP_BIN("fmin v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMIN_V2D:
            EMU_FP_BIN("fmin v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMAXNM_V2S:
            EMU_FP_BIN("fmaxnm v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMAXNM_V4S:
            EMU_FP_BIN("fmaxnm v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMAXNM_V2D:
            EMU_FP_BIN("fmaxnm v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMINNM_V2S:
            EMU_FP_BIN("fminnm v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMINNM_V4S:
            EMU_FP_BIN("fminnm v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMINNM_V2D:
            EMU_FP_BIN("fminnm v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FADDP_V2S:
            EMU_FP_BIN("faddp v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FADDP_V4S:
            EMU_FP_BIN("faddp v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FADDP_V2D:
            EMU_FP_BIN("faddp v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMAXP_V2S:
            EMU_FP_BIN("fmaxp v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMAXP_V4S:
            EMU_FP_BIN("fmaxp v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMAXP_V2D:
            EMU_FP_BIN("fmaxp v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMINP_V2S:
            EMU_FP_BIN("fminp v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMINP_V4S:
            EMU_FP_BIN("fminp v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FMINP_V2D:
            EMU_FP_BIN("fminp v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FCMEQ_V2S:
            EMU_FP_BIN("fcmeq v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FCMEQ_V4S:
            EMU_FP_BIN("fcmeq v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FCMGE_V4S:
            EMU_FP_BIN("fcmge v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FCMGT_V4S:
            EMU_FP_BIN("fcmgt v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_FABS_V2S:
            EMU_FP_UN("fabs v0.2s, v1.2s", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FABS_V4S:
            EMU_FP_UN("fabs v0.4s, v1.4s", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FABS_V2D:
            EMU_FP_UN("fabs v0.2d, v1.2d", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FNEG_V2S:
            EMU_FP_UN("fneg v0.2s, v1.2s", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FNEG_V4S:
            EMU_FP_UN("fneg v0.4s, v1.4s", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FNEG_V2D:
            EMU_FP_UN("fneg v0.2d, v1.2d", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FSQRT_V2S:
            EMU_FP_UN("fsqrt v0.2s, v1.2s", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FSQRT_V4S:
            EMU_FP_UN("fsqrt v0.4s, v1.4s", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_FSQRT_V2D:
            EMU_FP_UN("fsqrt v0.2d, v1.2d", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_REV64_V16B:
            EMU_FP_UN("rev64 v0.16b, v1.16b", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_REV64_V8H:
            EMU_FP_UN("rev64 v0.8h, v1.8h", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_REV32_V16B:
            EMU_FP_UN("rev32 v0.16b, v1.16b", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_REV16_V16B:
            EMU_FP_UN("rev16 v0.16b, v1.16b", &fp_regs[rd], &fp_regs[rn]);
            break;
        case ARM64_SIMD_OP_AND_V16B:
            EMU_FP_BIN("and v0.16b, v1.16b, v2.16b", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_ORR_V16B:
            EMU_FP_BIN("orr v0.16b, v1.16b, v2.16b", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        case ARM64_SIMD_OP_EOR_V16B:
            EMU_FP_BIN("eor v0.16b, v1.16b, v2.16b", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
            break;
        default:
        {
            switch (operation)
            {
            case ARM64_SIMD_OP_FADDP_S_V2S:
                EMU_FP_UN("faddp s0, v1.2s", &fp_regs[rd], &fp_regs[rn]);
                break;
            case ARM64_SIMD_OP_FADDP_D_V2D:
                EMU_FP_UN("faddp d0, v1.2d", &fp_regs[rd], &fp_regs[rn]);
                break;
            case ARM64_SIMD_OP_FMAXV_S_V4S:
                EMU_FP_UN("fmaxv s0, v1.4s", &fp_regs[rd], &fp_regs[rn]);
                break;
            case ARM64_SIMD_OP_FMINV_S_V4S:
                EMU_FP_UN("fminv s0, v1.4s", &fp_regs[rd], &fp_regs[rn]);
                break;
            default:
                return EMU_INSN_SKIP;
            }
            break;
        }
        }

        result = EMU_INSN_HANDLED;
    }

    if (result != EMU_INSN_HANDLED) return result;

    fpsr = read_fpsr();
    for (i = 0; i < 32; i++) write_q_reg(i, &fp_regs[i]);
    write_fpsr(fpsr);
    write_fpcr(fpcr);
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

#undef EMU_VEC_ACC
#undef EMU_FP_TERN
#undef EMU_FP_CONVERT_GPR
#undef EMU_FP_CONVERT_SIMD
#undef EMU_FP_UN
#undef EMU_FP_BIN

/* 数据处理局部缩写：C 解码编码字段，运算结果尽量由同语义 ARM64 指令产生。 */
#define EMU_DP_MASK(SF) ((SF) ? ~0ULL : 0xFFFFFFFFULL)

static __always_inline uint64_t emu_extract_bits(uint64_t high, uint64_t low, uint32_t shift, bool sf)
{
    uint32_t width = sf ? 64 : 32;
    uint64_t mask = EMU_DP_MASK(sf);

    high &= mask;
    low &= mask;
    if (!shift) return low;
    return ((low >> shift) | (high << (width - shift))) & mask;
}

#define EMU_DP_SHIFT(V, T, A, SF)                                                                                     \
    ({                                                                                                                \
        uint64_t __v = (V), __a = (A), __ret;                                                                         \
        uint32_t __ret32;                                                                                             \
        if (SF)                                                                                                       \
        {                                                                                                             \
            switch (T)                                                                                                \
            {                                                                                                         \
            case 0:                                                                                                   \
                asm volatile("lslv %0, %1, %2\n" : "=r"(__ret) : "r"(__v), "r"(__a) : "cc");                          \
                break;                                                                                                \
            case 1:                                                                                                   \
                asm volatile("lsrv %0, %1, %2\n" : "=r"(__ret) : "r"(__v), "r"(__a) : "cc");                          \
                break;                                                                                                \
            case 2:                                                                                                   \
                asm volatile("asrv %0, %1, %2\n" : "=r"(__ret) : "r"(__v), "r"(__a) : "cc");                          \
                break;                                                                                                \
            default:                                                                                                  \
                asm volatile("rorv %0, %1, %2\n" : "=r"(__ret) : "r"(__v), "r"(__a) : "cc");                          \
                break;                                                                                                \
            }                                                                                                         \
        }                                                                                                             \
        else                                                                                                          \
        {                                                                                                             \
            switch (T)                                                                                                \
            {                                                                                                         \
            case 0:                                                                                                   \
                asm volatile("lslv %w0, %w1, %w2\n" : "=r"(__ret32) : "r"((uint32_t)__v), "r"((uint32_t)__a) : "cc"); \
                break;                                                                                                \
            case 1:                                                                                                   \
                asm volatile("lsrv %w0, %w1, %w2\n" : "=r"(__ret32) : "r"((uint32_t)__v), "r"((uint32_t)__a) : "cc"); \
                break;                                                                                                \
            case 2:                                                                                                   \
                asm volatile("asrv %w0, %w1, %w2\n" : "=r"(__ret32) : "r"((uint32_t)__v), "r"((uint32_t)__a) : "cc"); \
                break;                                                                                                \
            default:                                                                                                  \
                asm volatile("rorv %w0, %w1, %w2\n" : "=r"(__ret32) : "r"((uint32_t)__v), "r"((uint32_t)__a) : "cc"); \
                break;                                                                                                \
            }                                                                                                         \
            __ret = __ret32;                                                                                          \
        }                                                                                                             \
        __ret;                                                                                                        \
    })

#define EMU_DP_UN64(INST, A)                                                      \
    ({                                                                            \
        uint64_t __ret;                                                           \
        asm volatile(INST " %0, %1\n" : "=r"(__ret) : "r"((uint64_t)(A)) : "cc"); \
        __ret;                                                                    \
    })

#define EMU_DP_UN32(INST, A)                                                        \
    ({                                                                              \
        uint32_t __ret;                                                             \
        asm volatile(INST " %w0, %w1\n" : "=r"(__ret) : "r"((uint32_t)(A)) : "cc"); \
        __ret;                                                                      \
    })

#define EMU_DP_CNT(V, SF)                                             \
    ({                                                                \
        __uint128_t __saved_q0;                                       \
        uint32_t __ret;                                               \
        if (SF)                                                       \
            asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                         "str q0, [%2]\n"                             \
                         "movi v0.2d, #0\n"                           \
                         "fmov d0, %1\n"                              \
                         "cnt v0.8b, v0.8b\n"                         \
                         "addv b0, v0.8b\n"                           \
                         "umov %w0, v0.b[0]\n"                        \
                         "ldr q0, [%2]\n"                             \
                         : "=&r"(__ret)                               \
                         : "r"((uint64_t)(V)), "r"(&__saved_q0)       \
                         : "memory", "cc");                           \
        else                                                          \
            asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                         "str q0, [%2]\n"                             \
                         "movi v0.2d, #0\n"                           \
                         "fmov s0, %w1\n"                             \
                         "cnt v0.8b, v0.8b\n"                         \
                         "addv b0, v0.8b\n"                           \
                         "umov %w0, v0.b[0]\n"                        \
                         "ldr q0, [%2]\n"                             \
                         : "=&r"(__ret)                               \
                         : "r"((uint32_t)(V)), "r"(&__saved_q0)       \
                         : "memory", "cc");                           \
        __ret;                                                        \
    })

#define EMU_DP_CRC32(INST, A, B)                                                                                              \
    ({                                                                                                                        \
        uint32_t __ret;                                                                                                       \
        asm volatile(".arch_extension crc\n" INST " %w0, %w1, %w2\n" : "=r"(__ret) : "r"((uint32_t)(A)), "r"((uint32_t)(B))); \
        __ret;                                                                                                                \
    })

#define EMU_DP_CRC64(INST, A, B)                                                                                             \
    ({                                                                                                                       \
        uint32_t __ret;                                                                                                      \
        asm volatile(".arch_extension crc\n" INST " %w0, %w1, %2\n" : "=r"(__ret) : "r"((uint32_t)(A)), "r"((uint64_t)(B))); \
        __ret;                                                                                                               \
    })

static __always_inline enum emu_insn_result emu_simulate_data_processing_insn(struct pt_regs *regs, const struct arm64_decoded_insn *decoded)
{
    const struct arm64_data_operands *operands = &decoded->operands.data;
    bool sf = decoded->operand_width == 64;

    if (decoded->opcode == ARM64_OP_ADR || decoded->opcode == ARM64_OP_ADRP)
    {
        uint64_t target;

        if (!arm64_decode_direct_target(decoded, regs->pc, &target)) return EMU_INSN_SKIP;
        if (decoded->rd != 31) regs->regs[decoded->rd] = target;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }

    if (decoded->opcode == ARM64_OP_ADD_SUB_IMMEDIATE)
    {
        uint64_t a, result, nzcv = 0;
        bool setflags = (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) != 0;

        a = addr_reg_read(regs, decoded->rn);
        result = emu_addsub_hw(a, operands->immediate, decoded->operation == ARM64_OPERATION_SUB, setflags, sf, &nzcv);

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
        uint64_t a = reg_read(regs, decoded->rn) & EMU_DP_MASK(sf);
        uint64_t b, result;

        if (is_unsigned) b = operands->immediate;
        else b = sign_extend64(operands->immediate, 7) & EMU_DP_MASK(sf);

        result = emu_minmax_hw(a, b, is_min, is_unsigned, sf);

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_LOGICAL_IMMEDIATE)
    {
        uint32_t opc = decoded->operation == ARM64_OPERATION_AND ? 0 : decoded->operation == ARM64_OPERATION_ORR ? 1 : decoded->operation == ARM64_OPERATION_EOR ? 2 : 3;
        uint64_t a, result, nzcv = 0;

        a = reg_read(regs, decoded->rn) & EMU_DP_MASK(sf);
        result = emu_logic_hw(a, operands->immediate, opc, false, sf, &nzcv);

        if (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) emu_write_nzcv(regs, nzcv);

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_BITFIELD)
    {
        uint32_t width = sf ? 64 : 32;
        uint64_t src, dst, bot, result;

        src = reg_read(regs, decoded->rn) & EMU_DP_MASK(sf);
        dst = reg_read(regs, decoded->rd) & EMU_DP_MASK(sf);
        bot = emu_extract_bits(src, src, operands->immr, sf) & operands->wmask;

        switch (decoded->operation)
        {
        case ARM64_OPERATION_SBFM:
        {
            uint64_t sign_bit = (operands->tmask == EMU_DP_MASK(sf)) ? (1ULL << (width - 1)) : ((operands->tmask + 1) >> 1);

            result = bot & operands->tmask;
            if (bot & sign_bit) result |= ~operands->tmask;
            break;
        }
        case ARM64_OPERATION_BFM:
            result = (dst & ~operands->wmask) | (bot & operands->wmask);
            break;
        case ARM64_OPERATION_UBFM:
            result = bot & operands->tmask;
            break;
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_EXTRACT)
    {
        uint64_t result;

        result = emu_extract_bits(reg_read(regs, decoded->rn), reg_read(regs, decoded->rm), operands->shift_amount, sf);

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_MOVE_WIDE)
    {
        uint64_t result;

        if (decoded->operation == ARM64_OPERATION_MOVN) result = ~(operands->immediate << operands->shift_amount);
        else if (decoded->operation == ARM64_OPERATION_MOVZ) result = operands->immediate << operands->shift_amount;
        else result = (reg_read(regs, decoded->rd) & ~(0xFFFFULL << operands->shift_amount)) | (operands->immediate << operands->shift_amount);

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_ADD_SUB_SHIFTED)
    {
        uint64_t a, b, result, nzcv = 0;
        bool setflags = (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) != 0;

        a = reg_read(regs, decoded->rn);
        b = EMU_DP_SHIFT(reg_read(regs, decoded->rm), operands->shift_type, operands->shift_amount, sf);
        result = emu_addsub_hw(a, b, decoded->operation == ARM64_OPERATION_SUB, setflags, sf, &nzcv);

        if (setflags) emu_write_nzcv(regs, nzcv);
        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_ADD_SUB_EXTENDED)
    {
        uint64_t a, b, result, nzcv = 0;
        bool setflags = (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) != 0;

        a = addr_reg_read(regs, decoded->rn);
        b = emu_extend_reg(reg_read(regs, decoded->rm), operands->option, operands->shift_amount);
        result = emu_addsub_hw(a, b, decoded->operation == ARM64_OPERATION_SUB, setflags, sf, &nzcv);

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
        uint64_t a, b, result, nzcv = 0;

        a = reg_read(regs, decoded->rn);
        b = EMU_DP_SHIFT(reg_read(regs, decoded->rm), operands->shift_type, operands->shift_amount, sf);
        result = emu_logic_hw(a, b, opc, (decoded->flags & ARM64_INSN_FLAG_INVERT) != 0, sf, &nzcv);

        if (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) emu_write_nzcv(regs, nzcv);
        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_CONDITIONAL_SELECT)
    {
        uint64_t a, b, result;

        a = reg_read(regs, decoded->rn);
        b = reg_read(regs, decoded->rm);
        if (!emu_cond_select_hw(decoded->operation, a, b, regs->pstate, operands->condition, sf, &result)) return EMU_INSN_SKIP;

        reg_write(regs, decoded->rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if (decoded->opcode == ARM64_OP_DATA_PROCESSING_2_SOURCE)
    {
        uint64_t a = reg_read(regs, decoded->rn) & EMU_DP_MASK(sf);
        uint64_t b = reg_read(regs, decoded->rm) & EMU_DP_MASK(sf);
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
            result = EMU_DP_CRC32("crc32b", a, b);
            break;
        case ARM64_OPERATION_CRC32H:
            result = EMU_DP_CRC32("crc32h", a, b);
            break;
        case ARM64_OPERATION_CRC32W:
            result = EMU_DP_CRC32("crc32w", a, b);
            break;
        case ARM64_OPERATION_CRC32X:
            result = EMU_DP_CRC64("crc32x", a, b);
            break;
        case ARM64_OPERATION_CRC32CB:
            result = EMU_DP_CRC32("crc32cb", a, b);
            break;
        case ARM64_OPERATION_CRC32CH:
            result = EMU_DP_CRC32("crc32ch", a, b);
            break;
        case ARM64_OPERATION_CRC32CW:
            result = EMU_DP_CRC32("crc32cw", a, b);
            break;
        case ARM64_OPERATION_CRC32CX:
            result = EMU_DP_CRC64("crc32cx", a, b);
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
            uint64_t n = reg_read(regs, decoded->rn) & EMU_DP_MASK(sf);
            uint64_t m = reg_read(regs, decoded->rm) & EMU_DP_MASK(sf);
            uint64_t a = reg_read(regs, decoded->ra) & EMU_DP_MASK(sf);

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
            result = sf ? EMU_DP_UN64("rbit", src) : EMU_DP_UN32("rbit", src);
            break;
        case ARM64_OPERATION_REV16:
            result = sf ? EMU_DP_UN64("rev16", src) : EMU_DP_UN32("rev16", src);
            break;
        case ARM64_OPERATION_REV32:
            result = sf ? EMU_DP_UN64("rev32", src) : EMU_DP_UN32("rev", src);
            break;
        case ARM64_OPERATION_REV64:
            result = EMU_DP_UN64("rev", src);
            break;
        case ARM64_OPERATION_CLZ:
            result = sf ? EMU_DP_UN64("clz", src) : EMU_DP_UN32("clz", src);
            break;
        case ARM64_OPERATION_CLS:
            result = sf ? EMU_DP_UN64("cls", src) : EMU_DP_UN32("cls", src);
            break;
        case ARM64_OPERATION_CTZ:
            result = sf ? EMU_DP_UN64("clz", EMU_DP_UN64("rbit", src)) : EMU_DP_UN32("clz", EMU_DP_UN32("rbit", src));
            break;
        case ARM64_OPERATION_CNT:
            result = EMU_DP_CNT(src, sf);
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

#undef EMU_DP_CRC64
#undef EMU_DP_CRC32
#undef EMU_DP_CNT
#undef EMU_DP_UN32
#undef EMU_DP_UN64
#undef EMU_DP_SHIFT
#undef EMU_DP_MASK

static __always_inline bool emulate_insn(struct pt_regs *regs, const uint32_t *specified_insn, const struct emu_mem_access *mem_access)
{
    uint32_t insn;
    uint64_t pc = regs->pc;
    __uint128_t fetched_insn;
    struct arm64_decoded_insn decoded;
    enum arm64_decode_status decode_status;
    enum emu_insn_result result = EMU_INSN_SKIP;

    if (specified_insn) insn = *specified_insn;
    else
    {
        if (emu_read_mem(mem_access, pc, sizeof(insn), &fetched_insn))
        {
            ls_log_always_tag("emulate_insn", "failed pc=0x%llx insn_read_failed\n", (unsigned long long)pc);
            return false;
        }
        insn = (uint32_t)fetched_insn;
    }

    decode_status = arm64_decode_insn(insn, &decoded);
    if (decode_status == ARM64_DECODE_OK)
    {
        switch (decoded.insn_class)
        {
        case ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM:
            switch (decoded.opcode)
            {
            case ARM64_OP_NOP:
                result = EMU_INSN_NOP;
                break;
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
            result = emu_simulate_load_store_insn(regs, &decoded, pc, mem_access);
            break;
        case ARM64_INSN_CLASS_DATA_PROCESSING_SIMD_FP:
            result = emu_simulate_fp_simd_insn(regs, &decoded, pc);
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
    if (result == EMU_INSN_HANDLED || result == EMU_INSN_NOP) return true;

    ls_log_always_tag("emulate_insn", "failed pc=0x%llx insn=0x%08x bytes=%02x %02x %02x %02x\n", (unsigned long long)pc, insn, insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);
    return false;
}

#endif // EMULATE_INSN_H
