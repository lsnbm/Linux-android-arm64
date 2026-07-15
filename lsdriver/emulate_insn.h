#ifndef EMULATE_INSN_H
#define EMULATE_INSN_H

#include <linux/uaccess.h>
#include <linux/errno.h>
#include <asm/ptrace.h>
#include <asm/insn.h>
#include <asm/barrier.h>
#include <asm/sysreg.h>
#include "arm64_reg.h"

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

    处理结构：emulate_insn() 取指后只做大类分发；每个大类 handler 内集中处理
    本类指令。只有跨多个大类复用的寄存器、访存、NZCV、条件判断和 ALU
    硬件封装保留为通用函数；单类重复片段使用紧贴 handler、用完即撤销的宏。

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
    - 系统/PC 相对类：emu_simulate_system_insn() 处理 ADR/ADRP、MRS/MSR(register)
        白名单系统寄存器。
  - 数据处理类：emu_simulate_data_processing_insn() 处理 ADD/SUB、ADDS/SUBS、
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
  - FP16、复杂 AdvSIMD 重排/结构化访存：TBL/TBX、ZIP/UZP/TRN、INS/DUP、
    LD1/ST1/LD1R 等。
    - 异常和大部分系统指令：SVC、HVC、SMC、BRK、未列入白名单的系统寄存器。
  ========================================================================= */

#define EMU_SYSREG_INSN_MASK                    0xFFF00000U
#define EMU_SYSREG_MRS_INSN                     0xD5300000U
#define EMU_SYSREG_MSR_INSN                     0xD5100000U
#define EMU_HINT_NOP_INSN                       0xD503201FU
#define EMU_SYSREG_KEY(OP0, OP1, CRN, CRM, OP2) ((((OP0) & 0x3) << 14) | (((OP1) & 0x7) << 11) | (((CRN) & 0xF) << 7) | (((CRM) & 0xF) << 3) | ((OP2) & 0x7))
#define EMU_SYSREG_KEY_FROM_INSN(INSN)          (((INSN) >> 5) & 0xFFFFU)
#define EMU_SYSREG_NZCV                         EMU_SYSREG_KEY(3, 3, 4, 2, 0)
#define EMU_SYSREG_FPCR                         EMU_SYSREG_KEY(3, 3, 4, 4, 0)
#define EMU_SYSREG_FPSR                         EMU_SYSREG_KEY(3, 3, 4, 4, 1)
#define EMU_SYSREG_TPIDR_EL0                    EMU_SYSREG_KEY(3, 3, 13, 0, 2)
#define EMU_SYSREG_TPIDRRO_EL0                  EMU_SYSREG_KEY(3, 3, 13, 0, 3)

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

typedef int (*emu_read_mem_fn)(void *ctx, uint64_t addr, int bytes, __uint128_t *out);
typedef int (*emu_write_mem_fn)(void *ctx, uint64_t addr, int bytes, __uint128_t value);

struct emu_mem_access
{
    emu_read_mem_fn read;
    emu_write_mem_fn write;
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

static __always_inline bool emu_is_lse_atomic(uint32_t insn)
{
    if ((insn & 0x3F200C00) == 0x38200000 && ((insn >> 12) & 0xF) <= 8)
    {
        uint32_t op = (insn >> 12) & 0xF;

        if (op <= 8) return true; // SWP / LDADD / LDCLR / LDEOR / LDSET / LD{S,U}{MAX,MIN}
    }
    if ((insn & 0x3FA07C00) == 0x08A07C00) return true; // CAS / CASA / CASL / CASAL
    if ((insn & 0x3FA07C00) == 0x08207C00)
    {
        uint32_t size = (insn >> 30) & 0x3;
        uint32_t op = (insn >> 21) & 0xF;
        uint32_t rt2 = (insn >> 10) & 0x1F;

        return size < 2 && rt2 == 31 && (op == 0x1 || op == 0x3);
    }
    return false;
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

static __always_inline enum emu_insn_result emu_simulate_system_insn(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    if (insn == EMU_HINT_NOP_INSN) return EMU_INSN_NOP;

    if (((insn & EMU_SYSREG_INSN_MASK) == EMU_SYSREG_MRS_INSN) || ((insn & EMU_SYSREG_INSN_MASK) == EMU_SYSREG_MSR_INSN))
    {
        uint32_t rt = insn & 0x1F;
        uint32_t sysreg = EMU_SYSREG_KEY_FROM_INSN(insn);
        bool is_mrs = ((insn & EMU_SYSREG_INSN_MASK) == EMU_SYSREG_MRS_INSN);
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

    if ((insn & 0x1F000000) == 0x10000000)
    {
        uint32_t rd = insn & 0x1F;
        s64 imm = sign_extend64(((insn >> 5) & 0x7FFFF) << 2 | ((insn >> 29) & 0x3), 20);

        // Rd=31 在 ADR/ADRP 中表示丢弃结果(XZR)；pt_regs.regs[] 只有 0..30，不能写 regs[31]。
        if (rd != 31) regs->regs[rd] = (insn & 0x80000000) ? ((pc & ~0xFFFULL) + (imm << 12)) : (pc + imm);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    return EMU_INSN_SKIP;
}

static __always_inline enum emu_insn_result emu_simulate_branch_insn(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t op_branch = insn & 0xFC000000;

    if (op_branch == 0x14000000 || op_branch == 0x94000000)
    {
        if (insn & (1U << 31)) regs->regs[30] = pc + 4;
        regs->pc = pc + sign_extend64((s64)(insn & 0x3FFFFFF) << 2, 27);
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0xFFFFFC1F) == 0xD61F0000 || (insn & 0xFFFFFC1F) == 0xD63F0000 || (insn & 0xFFFFFC1F) == 0xD65F0000)
    {
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t opc = (insn >> 21) & 0x3;

        if (opc > 2) return EMU_INSN_SKIP;

        regs->pc = reg_read(regs, rn);
        if (opc == 1) regs->regs[30] = pc + 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0xFF000010) == 0x54000000)
    {
        regs->pc = emu_cond_holds_hw(regs->pstate, insn & 0xF) ? (pc + sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20)) : (pc + 4);
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7E000000) == 0x34000000)
    {
        uint32_t rt = insn & 0x1F;
        uint64_t val = ((insn >> 31) & 1) ? reg_read(regs, rt) : (uint32_t)reg_read(regs, rt);
        bool jump = ((insn >> 24) & 1) ? (val != 0) : (val == 0);

        regs->pc = jump ? (pc + sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20)) : (pc + 4);
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7E000000) == 0x36000000)
    {
        uint32_t rt = insn & 0x1F;
        uint32_t pos = (((insn >> 31) & 1) << 5) | ((insn >> 19) & 0x1F);
        bool jump = (((reg_read(regs, rt) >> pos) & 1) == ((insn >> 24) & 1));

        regs->pc = jump ? (pc + sign_extend64((s64)((insn >> 5) & 0x3FFF) << 2, 15)) : (pc + 4);
        return EMU_INSN_HANDLED;
    }

    return EMU_INSN_SKIP;
}

#define EMU_LDST_MASK(B)     (~0ULL >> (64 - (B) * 8))
#define EMU_LDST_SX(V, B)    ((uint64_t)sign_extend64((uint64_t)(V), (B) * 8 - 1))
#define EMU_LDST_ST(A, B, V) emu_write_mem(mem_access, (A), (B), (V))

static __always_inline enum emu_insn_result emu_simulate_load_store_insn(struct pt_regs *regs, uint32_t insn, uint64_t pc, const struct emu_mem_access *mem_access)
{
    bool is_fp = (insn & 0x04000000) != 0;
    uint32_t size = (insn >> 30) & 0x3;
    __uint128_t fp_regs[32];
    uint32_t fpsr = 0, fpcr = 0;
    bool fp_dirty = false;

    if ((insn & 0x3F200C00) == 0x38200000)
    {
        uint32_t op = (insn >> 12) & 0xF;
        bool acquire = (insn >> 23) & 1;
        bool release = (insn >> 22) & 1;
        uint32_t rs = (insn >> 16) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t rt = insn & 0x1F;
        int bytes = 1 << size;
        uint64_t mask = EMU_LDST_MASK(bytes);
        uint64_t addr = addr_reg_read(regs, rn);
        uint64_t src = reg_read(regs, rs) & mask;
        uint64_t old, newval;
        __uint128_t mem;

        if (op > 8) return EMU_INSN_SKIP;
        if (addr & (bytes - 1)) return EMU_INSN_FAULT;
        if (emu_read_mem(mem_access, addr, bytes, &mem)) return EMU_INSN_FAULT;

        old = (uint64_t)mem & mask;
        switch (op)
        {
        case 0: // LDADD
            newval = emu_addsub_hw(old, src, false, false, size == 3, &newval) & mask;
            break;
        case 1: // LDCLR
            newval = emu_logic_hw(old, src, 0, true, size == 3, &newval) & mask;
            break;
        case 2: // LDEOR
            newval = emu_logic_hw(old, src, 2, false, size == 3, &newval) & mask;
            break;
        case 3: // LDSET
            newval = emu_logic_hw(old, src, 1, false, size == 3, &newval) & mask;
            break;
        case 4: // LDSMAX
            newval = emu_minmax_hw(EMU_LDST_SX(old, bytes), EMU_LDST_SX(src, bytes), false, false, true) & mask;
            break;
        case 5: // LDSMIN
            newval = emu_minmax_hw(EMU_LDST_SX(old, bytes), EMU_LDST_SX(src, bytes), true, false, true) & mask;
            break;
        case 6: // LDUMAX
            newval = emu_minmax_hw(old, src, false, true, true) & mask;
            break;
        case 7: // LDUMIN
            newval = emu_minmax_hw(old, src, true, true, true) & mask;
            break;
        case 8: // SWP
            newval = src;
            break;
        default:
            return EMU_INSN_SKIP;
        }

        if (release) smp_mb();
        if (EMU_LDST_ST(addr, bytes, newval)) return EMU_INSN_FAULT;
        if (acquire) smp_mb();

        reg_write(regs, rt, old, size == 3);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if ((insn & 0x3FA07C00) == 0x08A07C00)
    {
        bool acquire = (insn >> 22) & 1;
        bool release = (insn >> 15) & 1;
        uint32_t rs = (insn >> 16) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t rt = insn & 0x1F;
        int bytes = 1 << size;
        uint64_t mask = EMU_LDST_MASK(bytes);
        uint64_t addr = addr_reg_read(regs, rn);
        uint64_t expected = reg_read(regs, rs) & mask;
        uint64_t desired = reg_read(regs, rt) & mask;
        uint64_t old;
        __uint128_t mem;

        if (addr & (bytes - 1)) return EMU_INSN_FAULT;
        if (emu_read_mem(mem_access, addr, bytes, &mem)) return EMU_INSN_FAULT;

        old = (uint64_t)mem & mask;
        if (old == expected)
        {
            if (release) smp_mb();
            if (EMU_LDST_ST(addr, bytes, desired)) return EMU_INSN_FAULT;
        }
        if (acquire) smp_mb();

        reg_write(regs, rs, old, size == 3);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if (emu_is_lse_atomic(insn))
    {
        uint32_t op = (insn >> 21) & 0xF;
        bool acquire = op & 0x2;
        bool release = (insn >> 15) & 1;
        uint32_t rs = (insn >> 16) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t rt = insn & 0x1F;
        int bytes = (size == 0) ? 4 : 8;
        int total = bytes * 2;
        uint64_t mask = EMU_LDST_MASK(bytes);
        uint64_t addr = addr_reg_read(regs, rn);
        uint64_t old0, old1, exp0, exp1, new0, new1;
        __uint128_t mem0, mem1, pair;

        if ((size & 2) || (rs & 1) || rs >= 31 || (rt & 1) || rt >= 31) return EMU_INSN_SKIP;
        if (addr & (total - 1)) return EMU_INSN_FAULT;
        if (emu_read_mem(mem_access, addr, bytes, &mem0) || emu_read_mem(mem_access, addr + bytes, bytes, &mem1)) return EMU_INSN_FAULT;

        old0 = (uint64_t)mem0 & mask;
        old1 = (uint64_t)mem1 & mask;
        exp0 = reg_read(regs, rs) & mask;
        exp1 = reg_read(regs, rs + 1) & mask;
        new0 = reg_read(regs, rt) & mask;
        new1 = reg_read(regs, rt + 1) & mask;

        if (old0 == exp0 && old1 == exp1)
        {
            pair = ((__uint128_t)new1 << (bytes * 8)) | new0;
            if (release) smp_mb();
            if (EMU_LDST_ST(addr, total, pair)) return EMU_INSN_FAULT;
        }
        if (acquire) smp_mb();

        reg_write(regs, rs, old0, bytes == 8);
        reg_write(regs, rs + 1, old1, bytes == 8);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if ((insn & 0x3F000000) == 0x08000000)
    {
        bool ordered = (insn >> 23) & 1;
        bool load = (insn >> 22) & 1;
        bool pair = (insn >> 21) & 1;
        bool acquire_release = (insn >> 15) & 1;
        uint32_t rs = (insn >> 16) & 0x1F;
        uint32_t rt2 = (insn >> 10) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t rt = insn & 0x1F;
        uint64_t addr = addr_reg_read(regs, rn);
        int bytes = 1 << size;
        int total = pair ? bytes * 2 : bytes;
        __uint128_t val0;

        if (ordered)
        {
            if (pair || !acquire_release || rs != 31 || rt2 != 31) return EMU_INSN_SKIP;
        }
        else
        {
            if (pair && size < 2) return EMU_INSN_SKIP;
            if (!pair && rt2 != 31) return EMU_INSN_SKIP;
            if (load && rs != 31) return EMU_INSN_SKIP;
        }
        if (addr & (total - 1)) return EMU_INSN_FAULT;

        if (load)
        {
            if (emu_read_mem(mem_access, addr, bytes, &val0)) return EMU_INSN_FAULT;
            reg_write(regs, rt, (u64)val0, size == 3);
            if (pair)
            {
                __uint128_t val1;

                if (emu_read_mem(mem_access, addr + bytes, bytes, &val1)) return EMU_INSN_FAULT;
                reg_write(regs, rt2, (u64)val1, size == 3);
            }
            if (ordered || acquire_release) smp_mb();
        }
        else
        {
            if (ordered || acquire_release) smp_mb();
            if (emu_write_mem(mem_access, addr, bytes, reg_read(regs, rt))) return EMU_INSN_FAULT;
            if (pair && emu_write_mem(mem_access, addr + bytes, bytes, reg_read(regs, rt2))) return EMU_INSN_FAULT;
            if (!ordered) reg_write(regs, rs, 0, false);
        }

        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if ((insn & 0x3F200C00) == 0x19000000)
    {
        uint32_t opc = (insn >> 22) & 0x3;
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t rt = insn & 0x1F;
        int bytes = 1 << size;
        s64 imm9 = sign_extend64((s64)((insn >> 12) & 0x1FF), 8);
        uint64_t addr = addr_reg_read(regs, rn) + imm9;

        if (size == 3 && opc > 1) return EMU_INSN_SKIP;
        if (size == 2 && opc == 3) return EMU_INSN_SKIP;

        if (opc == 0)
        {
            smp_mb();
            if (emu_write_mem(mem_access, addr, bytes, reg_read(regs, rt))) return EMU_INSN_FAULT;
        }
        else
        {
            __uint128_t val;
            uint64_t raw;

            if (emu_read_mem(mem_access, addr, bytes, &val)) return EMU_INSN_FAULT;
            raw = (u64)val;
            if (opc >= 2) raw = EMU_LDST_SX(raw, bytes);
            reg_write(regs, rt, raw, size == 3 || opc == 2);
            smp_mb();
        }
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    if ((insn & 0x3FFFFC00) == 0x38BFC000)
    {
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t rt = insn & 0x1F;
        int bytes = 1 << size;
        uint64_t addr = addr_reg_read(regs, rn);
        __uint128_t val;

        if (addr & (bytes - 1)) return EMU_INSN_FAULT;
        if (emu_read_mem(mem_access, addr, bytes, &val)) return EMU_INSN_FAULT;
        reg_write(regs, rt, (u64)val, size == 3);
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

    if ((insn & 0x3B000000) == 0x18000000)
    {
        uint32_t rt = insn & 0x1F;
        uint64_t addr = pc + sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);

        if (is_fp)
        {
            int bytes = (size == 0) ? 4 : ((size == 1) ? 8 : 16);
            __uint128_t val;

            if (size > 2) return EMU_INSN_SKIP;
            if (emu_read_mem(mem_access, addr, bytes, &val)) return EMU_INSN_FAULT;
            fp_regs[rt] = val;
            fp_dirty = true;
        }
        else
        {
            __uint128_t val;

            if (size == 3) return EMU_INSN_NOP;
            if (size == 0)
            {
                if (emu_read_mem(mem_access, addr, 4, &val)) return EMU_INSN_FAULT;
                reg_write(regs, rt, (u64)val, false);
            }
            else if (size == 1)
            {
                if (emu_read_mem(mem_access, addr, 8, &val)) return EMU_INSN_FAULT;
                reg_write(regs, rt, (u64)val, true);
            }
            else if (size == 2)
            {
                if (emu_read_mem(mem_access, addr, 4, &val)) return EMU_INSN_FAULT;
                reg_write(regs, rt, EMU_LDST_SX(val, 4), true);
            }
        }
        goto done_ldst;
    }

    if ((insn & 0x3A000000) == 0x28000000)
    {
        uint32_t opc_pair = (insn >> 30) & 0x3;
        uint32_t load = (insn >> 22) & 1;
        uint32_t idx = (insn >> 23) & 0x3;
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t rt = insn & 0x1F;
        uint32_t rt2 = (insn >> 10) & 0x1F;
        int bytes;
        s64 off;
        uint64_t base = addr_reg_read(regs, rn);
        uint64_t addr;

        if (is_fp)
        {
            if (opc_pair == 3) return EMU_INSN_SKIP;
            bytes = 4 << opc_pair;
        }
        else
        {
            if (opc_pair == 3 || (opc_pair == 1 && !load)) return EMU_INSN_SKIP;
            bytes = (opc_pair == 2) ? 8 : 4;
        }
        off = sign_extend64((s64)((insn >> 15) & 0x7F), 6) * bytes;
        addr = (idx == 1) ? base : (base + off);
        if (!is_fp && (idx & 1) && rn != 31 && (rn == rt || rn == rt2)) return EMU_INSN_SKIP;
        if (!is_fp && load && rt == rt2) return EMU_INSN_SKIP;

        if (load)
        {
            __uint128_t val1, val2;

            if (emu_read_mem(mem_access, addr, bytes, &val1) || emu_read_mem(mem_access, addr + bytes, bytes, &val2)) return EMU_INSN_FAULT;
            if (is_fp)
            {
                fp_regs[rt] = val1;
                fp_regs[rt2] = val2;
                fp_dirty = true;
            }
            else if (opc_pair == 1)
            {
                reg_write(regs, rt, EMU_LDST_SX(val1, 4), true);
                reg_write(regs, rt2, EMU_LDST_SX(val2, 4), true);
            }
            else
            {
                reg_write(regs, rt, (u64)val1, bytes == 8);
                reg_write(regs, rt2, (u64)val2, bytes == 8);
            }
        }
        else
        {
            __uint128_t val1 = is_fp ? fp_regs[rt] : (__uint128_t)reg_read(regs, rt);
            __uint128_t val2 = is_fp ? fp_regs[rt2] : (__uint128_t)reg_read(regs, rt2);

            if (EMU_LDST_ST(addr, bytes, val1) || EMU_LDST_ST(addr + bytes, bytes, val2)) return EMU_INSN_FAULT;
        }
        if (idx & 1) addr_reg_write(regs, rn, base + off);
        goto done_ldst;
    }

    {
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t rt = insn & 0x1F;
        uint32_t opc = (insn >> 22) & 0x3;
        uint64_t base = addr_reg_read(regs, rn);
        uint64_t addr = base;
        uint64_t wb_addr = base;
        int bytes;
        bool is_load;
        bool writeback = false;

        if (is_fp)
        {
            if (size == 0 && (opc & 2)) bytes = 16;
            else bytes = (1 << size);
        }
        else
        {
            bytes = (1 << size);
        }

        if (!is_fp && size == 3 && opc >= 2) return (opc == 2) ? EMU_INSN_NOP : EMU_INSN_SKIP;
        if (!is_fp && size == 2 && opc == 3) return EMU_INSN_SKIP;

        if ((insn >> 24) & 1)
        {
            addr = base + (((insn >> 10) & 0xFFF) * bytes);
        }
        else
        {
            uint32_t idx = (insn >> 10) & 0x3;
            bool reg_form = ((insn >> 21) & 1) != 0;
            s64 imm9 = sign_extend64((s64)((insn >> 12) & 0x1FF), 8);

            if (reg_form && idx != 2) return EMU_INSN_SKIP;

            if (idx == 0) addr = base + imm9;
            else if (idx == 1 || idx == 3)
            {
                addr = (idx == 3) ? (base + imm9) : base;
                wb_addr = base + imm9;
                writeback = true;
            }
            else if (idx == 2 && reg_form)
            {
                uint32_t rm = (insn >> 16) & 0x1F, opt = (insn >> 13) & 0x7;
                s64 ext = reg_read(regs, rm);
                int shift;

                if (opt != 2 && opt != 3 && opt != 6 && opt != 7) return EMU_INSN_SKIP;
                shift = ((insn >> 12) & 1) ? __builtin_ctz(bytes) : 0;
                ext = emu_extend_reg(ext, opt, shift);
                addr = base + ext;
            }
            else if (idx == 2)
            {
                addr = base + imm9;
            }
            else return EMU_INSN_SKIP;
            if (!is_fp && writeback && rn != 31 && rn == rt) return EMU_INSN_SKIP;
        }

        is_load = is_fp ? ((insn >> 22) & 1) : (opc != 0);
        if (is_load)
        {
            __uint128_t val;

            if (emu_read_mem(mem_access, addr, bytes, &val)) return EMU_INSN_FAULT;
            if (is_fp)
            {
                fp_regs[rt] = val;
                fp_dirty = true;
            }
            else
            {
                u64 raw = (u64)val;
                if (opc >= 2)
                {
                    raw = EMU_LDST_SX(raw, bytes);
                }
                reg_write(regs, rt, raw, (size == 3 || opc == 2));
            }
        }
        else
        {
            __uint128_t val = is_fp ? fp_regs[rt] : (__uint128_t)reg_read(regs, rt);

            if (EMU_LDST_ST(addr, bytes, val)) return EMU_INSN_FAULT;
        }
        if (writeback) addr_reg_write(regs, rn, wb_addr);
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
                     : "memory");                                 \
    } while (0)

#define EMU_FP_UN(INST, DST, A)                                   \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "ldr q1, [%1]\n" INST "\n"                   \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(DST), "r"(A)                           \
                     : "memory");                                 \
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
                     : "memory");                                 \
    } while (0)

#define EMU_FP_EXT(INST, DST, A, B)                               \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "ldr q1, [%1]\n"                             \
                     "ldr q2, [%2]\n"                             \
                     "movi v0.2d, #0\n" INST "\n"                 \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(DST), "r"(A), "r"(B)                   \
                     : "memory");                                 \
    } while (0)

#define EMU_FP_EXT64_CASE(IMM)                                                       \
    case IMM:                                                                        \
        EMU_FP_EXT("ext v0.8b, v1.8b, v2.8b, #" #IMM, ext_dst, ext_left, ext_right); \
        break

#define EMU_FP_EXT128_CASE(IMM)                                                         \
    case IMM:                                                                           \
        EMU_FP_EXT("ext v0.16b, v1.16b, v2.16b, #" #IMM, ext_dst, ext_left, ext_right); \
        break

#define EMU_FP_FCSEL(INST, DST, A, B, TAKE)                            \
    do                                                                 \
    {                                                                  \
        asm volatile(".arch_extension fp\n.arch_extension simd\n"      \
                     "cmp %w3, #0\n"                                   \
                     "ldr q1, [%1]\n"                                  \
                     "ldr q2, [%2]\n" INST "\n"                        \
                     "str q0, [%0]\n"                                  \
                     :                                                 \
                     : "r"(DST), "r"(A), "r"(B), "r"((uint32_t)(TAKE)) \
                     : "cc", "memory");                                \
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
                     : "memory");                                 \
    } while (0)

static __always_inline enum emu_insn_result emu_simulate_fp_simd_insn(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    __uint128_t fp_regs[32];
    uint32_t fpsr, fpcr;
    enum emu_insn_result result = EMU_INSN_SKIP;
    int i;

    for (i = 0; i < 32; i++) read_q_reg(i, &fp_regs[i]);
    fpsr = read_fpsr();
    fpcr = read_fpcr();
    write_fpsr(fpsr);
    write_fpcr(fpcr);

    if ((insn & 0xFFFFFFE0) == 0x6F00E400) // MOVI Vd.2D, #0
    {
        uint32_t rd = insn & 0x1F;

        fp_regs[rd] = 0;
        result = EMU_INSN_HANDLED;
    }
    else if ((insn & 0xFF200C00) == 0x1E200800)
    {
        uint32_t type = (insn >> 22) & 0x3;
        uint32_t opcode = (insn >> 12) & 0xF;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

        if (type > 1) return EMU_INSN_SKIP;

        if (type == 0)
        {
            switch (opcode)
            {
            case 0:
                EMU_FP_BIN("fmul s0, s1, s2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 2:
                EMU_FP_BIN("fadd s0, s1, s2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 3:
                EMU_FP_BIN("fsub s0, s1, s2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 4:
                EMU_FP_BIN("fmax s0, s1, s2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 5:
                EMU_FP_BIN("fmin s0, s1, s2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 6:
                EMU_FP_BIN("fmaxnm s0, s1, s2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 7:
                EMU_FP_BIN("fminnm s0, s1, s2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 8:
                EMU_FP_BIN("fnmul s0, s1, s2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }
        else
        {
            switch (opcode)
            {
            case 0:
                EMU_FP_BIN("fmul d0, d1, d2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 1:
                EMU_FP_BIN("fdiv d0, d1, d2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 2:
                EMU_FP_BIN("fadd d0, d1, d2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 3:
                EMU_FP_BIN("fsub d0, d1, d2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 4:
                EMU_FP_BIN("fmax d0, d1, d2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 5:
                EMU_FP_BIN("fmin d0, d1, d2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 6:
                EMU_FP_BIN("fmaxnm d0, d1, d2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 7:
                EMU_FP_BIN("fminnm d0, d1, d2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 8:
                EMU_FP_BIN("fnmul d0, d1, d2", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }

        result = EMU_INSN_HANDLED;
    }
    else if ((insn & 0xFF207C00) == 0x1E204000)
    {
        uint32_t type = (insn >> 22) & 0x3;
        uint32_t opcode = (insn >> 15) & 0x3F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

        if (type > 1) return EMU_INSN_SKIP;

        if (type == 0)
        {
            switch (opcode)
            {
            case 0:
                fp_regs[rd] = (uint32_t)fp_regs[rn];
                break;
            case 1:
                EMU_FP_UN("fabs s0, s1", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 2:
                EMU_FP_UN("fneg s0, s1", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 3:
                EMU_FP_UN("fsqrt s0, s1", &fp_regs[rd], &fp_regs[rn]);
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }
        else
        {
            switch (opcode)
            {
            case 0:
                fp_regs[rd] = (uint64_t)fp_regs[rn];
                break;
            case 1:
                EMU_FP_UN("fabs d0, d1", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 2:
                EMU_FP_UN("fneg d0, d1", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 3:
                EMU_FP_UN("fsqrt d0, d1", &fp_regs[rd], &fp_regs[rn]);
                break;
            default:
                return EMU_INSN_SKIP;
            }
        }

        result = EMU_INSN_HANDLED;
    }
    else if ((insn & 0xFF000000) == 0x1F000000)
    {
        uint32_t type = (insn >> 22) & 0x3;
        bool neg = (insn >> 21) & 1;
        bool sub = (insn >> 15) & 1;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t ra = (insn >> 10) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

        if (type > 1) return EMU_INSN_SKIP;

        if (type == 0)
        {
            if (!neg && !sub) EMU_FP_TERN("fmadd s0, s1, s2, s3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
            else if (!neg && sub) EMU_FP_TERN("fmsub s0, s1, s2, s3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
            else if (neg && !sub) EMU_FP_TERN("fnmadd s0, s1, s2, s3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
            else EMU_FP_TERN("fnmsub s0, s1, s2, s3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
        }
        else
        {
            if (!neg && !sub) EMU_FP_TERN("fmadd d0, d1, d2, d3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
            else if (!neg && sub) EMU_FP_TERN("fmsub d0, d1, d2, d3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
            else if (neg && !sub) EMU_FP_TERN("fnmadd d0, d1, d2, d3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
            else EMU_FP_TERN("fnmsub d0, d1, d2, d3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
        }

        result = EMU_INSN_HANDLED;
    }
    else if ((insn & 0xFF20FC00) == 0x1E202000)
    {
        uint32_t type = (insn >> 22) & 0x3;
        bool zero = (insn >> 3) & 1;
        bool signal = (insn >> 4) & 1;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F;
        uint64_t nzcv;

        if (type > 1) return EMU_INSN_SKIP;

        if (type == 0)
        {
            if (zero)
            {
                if (signal)
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmpe s1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[rn])
                                 : "memory");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmp s1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[rn])
                                 : "memory");
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
                                 : "r"(&fp_regs[rn]), "r"(&fp_regs[rm])
                                 : "memory");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "ldr q2, [%2]\n"
                                 "fcmp s1, s2\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[rn]), "r"(&fp_regs[rm])
                                 : "memory");
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
                                 : "r"(&fp_regs[rn])
                                 : "memory");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "fcmp d1, #0.0\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[rn])
                                 : "memory");
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
                                 : "r"(&fp_regs[rn]), "r"(&fp_regs[rm])
                                 : "memory");
                else
                    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                                 "ldr q1, [%1]\n"
                                 "ldr q2, [%2]\n"
                                 "fcmp d1, d2\n"
                                 "mrs %0, nzcv\n"
                                 : "=r"(nzcv)
                                 : "r"(&fp_regs[rn]), "r"(&fp_regs[rm])
                                 : "memory");
            }
        }

        emu_write_nzcv(regs, nzcv);
        result = EMU_INSN_HANDLED;
    }
    else if ((insn & 0xFF200C00) == 0x1E200C00)
    {
        uint32_t type = (insn >> 22) & 0x3;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t cond = (insn >> 12) & 0xF;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        __uint128_t *fcsel_dst = &fp_regs[rd];
        const __uint128_t *fcsel_left = &fp_regs[rn];
        const __uint128_t *fcsel_right = &fp_regs[rm];
        bool take = emu_cond_holds_hw(regs->pstate, cond);

        if (type > 1) return EMU_INSN_SKIP;

        if (type == 0) EMU_FP_FCSEL("fcsel s0, s1, s2, ne", fcsel_dst, fcsel_left, fcsel_right, take);
        else EMU_FP_FCSEL("fcsel d0, d1, d2, ne", fcsel_dst, fcsel_left, fcsel_right, take);

        result = EMU_INSN_HANDLED;
    }
    else if ((insn & 0x7F3E7C00) == 0x1E260000)
    {
        bool sf = (insn >> 31) & 1;
        bool gp_to_fp = (insn >> 16) & 1;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

        if (gp_to_fp)
        {
            if (sf) fp_regs[rd] = reg_read(regs, rn);
            else fp_regs[rd] = (uint32_t)reg_read(regs, rn);
        }
        else
        {
            if (sf) reg_write(regs, rd, (uint64_t)fp_regs[rn], true);
            else reg_write(regs, rd, (uint32_t)fp_regs[rn], false);
        }

        result = EMU_INSN_HANDLED;
    }
    else
    {
        uint32_t sig = insn & 0xFFFFFC00;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t wout;
        uint64_t xout;
        bool convert_handled = true;

        switch (sig)
        {
        case 0x1E220000: // SCVTF Sd, Wn
            asm volatile(".arch_extension fp\n"
                         "scvtf s0, %w1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn))
                         : "memory");
            break;
        case 0x9E220000: // SCVTF Sd, Xn
            asm volatile(".arch_extension fp\n"
                         "scvtf s0, %1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn))
                         : "memory");
            break;
        case 0x1E620000: // SCVTF Dd, Wn
            asm volatile(".arch_extension fp\n"
                         "scvtf d0, %w1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn))
                         : "memory");
            break;
        case 0x9E620000: // SCVTF Dd, Xn
            asm volatile(".arch_extension fp\n"
                         "scvtf d0, %1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn))
                         : "memory");
            break;
        case 0x1E230000: // UCVTF Sd, Wn
            asm volatile(".arch_extension fp\n"
                         "ucvtf s0, %w1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn))
                         : "memory");
            break;
        case 0x9E230000: // UCVTF Sd, Xn
            asm volatile(".arch_extension fp\n"
                         "ucvtf s0, %1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn))
                         : "memory");
            break;
        case 0x1E630000: // UCVTF Dd, Wn
            asm volatile(".arch_extension fp\n"
                         "ucvtf d0, %w1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn))
                         : "memory");
            break;
        case 0x9E630000: // UCVTF Dd, Xn
            asm volatile(".arch_extension fp\n"
                         "ucvtf d0, %1\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn))
                         : "memory");
            break;
        case 0x1E380000: // FCVTZS Wd, Sn
            asm volatile(".arch_extension fp\n.arch_extension simd\n"
                         "ldr q1, [%1]\n"
                         "fcvtzs %w0, s1\n"
                         : "=r"(wout)
                         : "r"(&fp_regs[rn])
                         : "memory");
            reg_write(regs, rd, wout, false);
            break;
        case 0x9E380000: // FCVTZS Xd, Sn
            asm volatile(".arch_extension fp\n.arch_extension simd\n"
                         "ldr q1, [%1]\n"
                         "fcvtzs %0, s1\n"
                         : "=r"(xout)
                         : "r"(&fp_regs[rn])
                         : "memory");
            reg_write(regs, rd, xout, true);
            break;
        case 0x1E780000: // FCVTZS Wd, Dn
            asm volatile(".arch_extension fp\n.arch_extension simd\n"
                         "ldr q1, [%1]\n"
                         "fcvtzs %w0, d1\n"
                         : "=r"(wout)
                         : "r"(&fp_regs[rn])
                         : "memory");
            reg_write(regs, rd, wout, false);
            break;
        case 0x9E780000: // FCVTZS Xd, Dn
            asm volatile(".arch_extension fp\n.arch_extension simd\n"
                         "ldr q1, [%1]\n"
                         "fcvtzs %0, d1\n"
                         : "=r"(xout)
                         : "r"(&fp_regs[rn])
                         : "memory");
            reg_write(regs, rd, xout, true);
            break;
        case 0x1E390000: // FCVTZU Wd, Sn
            asm volatile(".arch_extension fp\n.arch_extension simd\n"
                         "ldr q1, [%1]\n"
                         "fcvtzu %w0, s1\n"
                         : "=r"(wout)
                         : "r"(&fp_regs[rn])
                         : "memory");
            reg_write(regs, rd, wout, false);
            break;
        case 0x9E390000: // FCVTZU Xd, Sn
            asm volatile(".arch_extension fp\n.arch_extension simd\n"
                         "ldr q1, [%1]\n"
                         "fcvtzu %0, s1\n"
                         : "=r"(xout)
                         : "r"(&fp_regs[rn])
                         : "memory");
            reg_write(regs, rd, xout, true);
            break;
        case 0x1E790000: // FCVTZU Wd, Dn
            asm volatile(".arch_extension fp\n.arch_extension simd\n"
                         "ldr q1, [%1]\n"
                         "fcvtzu %w0, d1\n"
                         : "=r"(wout)
                         : "r"(&fp_regs[rn])
                         : "memory");
            reg_write(regs, rd, wout, false);
            break;
        case 0x9E790000: // FCVTZU Xd, Dn
            asm volatile(".arch_extension fp\n.arch_extension simd\n"
                         "ldr q1, [%1]\n"
                         "fcvtzu %0, d1\n"
                         : "=r"(xout)
                         : "r"(&fp_regs[rn])
                         : "memory");
            reg_write(regs, rd, xout, true);
            break;
        case 0x1E624000: // FCVT Sd, Dn
            EMU_FP_UN("fcvt s0, d1", &fp_regs[rd], &fp_regs[rn]);
            break;
        case 0x1E22C000: // FCVT Dd, Sn
            EMU_FP_UN("fcvt d0, s1", &fp_regs[rd], &fp_regs[rn]);
            break;
        case 0x7E61D800: // UCVTF Dd, Dn
            EMU_FP_UN("ucvtf d0, d1", &fp_regs[rd], &fp_regs[rn]);
            break;
        default:
            convert_handled = false;
            break;
        }

        if (convert_handled) result = EMU_INSN_HANDLED;
        else
        {
            sig = insn & 0xFFE0FC00;

            if ((insn & 0xBFE08400) == 0x2E000000) // EXT Vd,Vn,Vm,#imm
            {
                uint32_t imm = (insn >> 11) & 0xF;
                __uint128_t *ext_dst = &fp_regs[rd];
                const __uint128_t *ext_left = &fp_regs[rn];
                const __uint128_t *ext_right = &fp_regs[rm];

                if (insn & (1U << 30))
                {
                    switch (imm)
                    {
                        EMU_FP_EXT128_CASE(0);
                        EMU_FP_EXT128_CASE(1);
                        EMU_FP_EXT128_CASE(2);
                        EMU_FP_EXT128_CASE(3);
                        EMU_FP_EXT128_CASE(4);
                        EMU_FP_EXT128_CASE(5);
                        EMU_FP_EXT128_CASE(6);
                        EMU_FP_EXT128_CASE(7);
                        EMU_FP_EXT128_CASE(8);
                        EMU_FP_EXT128_CASE(9);
                        EMU_FP_EXT128_CASE(10);
                        EMU_FP_EXT128_CASE(11);
                        EMU_FP_EXT128_CASE(12);
                        EMU_FP_EXT128_CASE(13);
                        EMU_FP_EXT128_CASE(14);
                        EMU_FP_EXT128_CASE(15);
                    default:
                        return EMU_INSN_SKIP;
                    }
                }
                else
                {
                    switch (imm)
                    {
                        EMU_FP_EXT64_CASE(0);
                        EMU_FP_EXT64_CASE(1);
                        EMU_FP_EXT64_CASE(2);
                        EMU_FP_EXT64_CASE(3);
                        EMU_FP_EXT64_CASE(4);
                        EMU_FP_EXT64_CASE(5);
                        EMU_FP_EXT64_CASE(6);
                        EMU_FP_EXT64_CASE(7);
                    default:
                        return EMU_INSN_SKIP;
                    }
                }

                return EMU_INSN_HANDLED;
            }

            switch (sig)
            {
            case 0x0E20D400:
                EMU_FP_BIN("fadd v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4E20D400:
                EMU_FP_BIN("fadd v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4E60D400:
                EMU_FP_BIN("fadd v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x0EA0D400:
                EMU_FP_BIN("fsub v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4EA0D400:
                EMU_FP_BIN("fsub v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4EE0D400:
                EMU_FP_BIN("fsub v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x2E20DC00:
                EMU_FP_BIN("fmul v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E20DC00:
                EMU_FP_BIN("fmul v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E60DC00:
                EMU_FP_BIN("fmul v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x2E20FC00:
                EMU_FP_BIN("fdiv v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E20FC00:
                EMU_FP_BIN("fdiv v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E60FC00:
                EMU_FP_BIN("fdiv v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4E20CC00:
                EMU_VEC_ACC("fmla v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4EA0CC00:
                EMU_VEC_ACC("fmls v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x0E20F400:
                EMU_FP_BIN("fmax v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4E20F400:
                EMU_FP_BIN("fmax v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4E60F400:
                EMU_FP_BIN("fmax v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x0EA0F400:
                EMU_FP_BIN("fmin v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4EA0F400:
                EMU_FP_BIN("fmin v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4EE0F400:
                EMU_FP_BIN("fmin v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x0E20C400:
                EMU_FP_BIN("fmaxnm v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4E20C400:
                EMU_FP_BIN("fmaxnm v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4E60C400:
                EMU_FP_BIN("fmaxnm v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x0EA0C400:
                EMU_FP_BIN("fminnm v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4EA0C400:
                EMU_FP_BIN("fminnm v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4EE0C400:
                EMU_FP_BIN("fminnm v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x2E20D400:
                EMU_FP_BIN("faddp v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E20D400:
                EMU_FP_BIN("faddp v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E60D400:
                EMU_FP_BIN("faddp v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x2E20F400:
                EMU_FP_BIN("fmaxp v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E20F400:
                EMU_FP_BIN("fmaxp v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E60F400:
                EMU_FP_BIN("fmaxp v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x2EA0F400:
                EMU_FP_BIN("fminp v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6EA0F400:
                EMU_FP_BIN("fminp v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6EE0F400:
                EMU_FP_BIN("fminp v0.2d, v1.2d, v2.2d", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x0E20E400:
                EMU_FP_BIN("fcmeq v0.2s, v1.2s, v2.2s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4E20E400:
                EMU_FP_BIN("fcmeq v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E20E400:
                EMU_FP_BIN("fcmge v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6EA0E400:
                EMU_FP_BIN("fcmgt v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x0EA0F800:
                EMU_FP_UN("fabs v0.2s, v1.2s", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x4EA0F800:
                EMU_FP_UN("fabs v0.4s, v1.4s", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x4EE0F800:
                EMU_FP_UN("fabs v0.2d, v1.2d", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x2EA0F800:
                EMU_FP_UN("fneg v0.2s, v1.2s", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x6EA0F800:
                EMU_FP_UN("fneg v0.4s, v1.4s", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x6EE0F800:
                EMU_FP_UN("fneg v0.2d, v1.2d", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x2EA1F800:
                EMU_FP_UN("fsqrt v0.2s, v1.2s", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x6EA1F800:
                EMU_FP_UN("fsqrt v0.4s, v1.4s", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x6EE1F800:
                EMU_FP_UN("fsqrt v0.2d, v1.2d", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x4E200800:
                EMU_FP_UN("rev64 v0.16b, v1.16b", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x4E600800:
                EMU_FP_UN("rev64 v0.8h, v1.8h", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x6E200800:
                EMU_FP_UN("rev32 v0.16b, v1.16b", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x4E201800:
                EMU_FP_UN("rev16 v0.16b, v1.16b", &fp_regs[rd], &fp_regs[rn]);
                break;
            case 0x4E201C00:
                EMU_FP_BIN("and v0.16b, v1.16b, v2.16b", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4EA01C00:
                EMU_FP_BIN("orr v0.16b, v1.16b, v2.16b", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6E201C00:
                EMU_FP_BIN("eor v0.16b, v1.16b, v2.16b", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x4EA08400:
                EMU_FP_BIN("add v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6EA08400:
                EMU_FP_BIN("sub v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            case 0x6EA08C00:
                EMU_FP_BIN("cmeq v0.4s, v1.4s, v2.4s", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm]);
                break;
            default:
            {
                uint32_t reduce_sig = insn & 0xFFFFFC00;

                switch (reduce_sig)
                {
                case 0x7E30D800:
                    EMU_FP_UN("faddp s0, v1.2s", &fp_regs[rd], &fp_regs[rn]);
                    break;
                case 0x7E70D800:
                    EMU_FP_UN("faddp d0, v1.2d", &fp_regs[rd], &fp_regs[rn]);
                    break;
                case 0x6E30F800:
                    EMU_FP_UN("fmaxv s0, v1.4s", &fp_regs[rd], &fp_regs[rn]);
                    break;
                case 0x6EB0F800:
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
#undef EMU_FP_FCSEL
#undef EMU_FP_EXT128_CASE
#undef EMU_FP_EXT64_CASE
#undef EMU_FP_EXT
#undef EMU_FP_TERN
#undef EMU_FP_UN
#undef EMU_FP_BIN

/* 数据处理局部缩写：C 解码编码字段，运算结果尽量由同语义 ARM64 指令产生。 */
#define EMU_DP_MASK(SF) ((SF) ? ~0ULL : 0xFFFFFFFFULL)

#define EMU_DP_ROR_HW(V, S, SF)                                                                                   \
    ({                                                                                                            \
        uint64_t __v = (V), __s = (S), __ret;                                                                     \
        uint32_t __ret32;                                                                                         \
        if (SF) asm volatile("rorv %0, %1, %2\n" : "=r"(__ret) : "r"(__v), "r"(__s) : "cc");                      \
        else                                                                                                      \
        {                                                                                                         \
            asm volatile("rorv %w0, %w1, %w2\n" : "=r"(__ret32) : "r"((uint32_t)__v), "r"((uint32_t)__s) : "cc"); \
            __ret = __ret32;                                                                                      \
        }                                                                                                         \
        __ret;                                                                                                    \
    })

#define EMU_DP_REPL(V, E, SF)                                     \
    ({                                                            \
        uint64_t __v = (V), __ret = 0;                            \
        uint32_t __e = (E), __w = (SF) ? 64 : 32, __p;            \
        uint64_t __m = (__e == 64) ? ~0ULL : ((1ULL << __e) - 1); \
        __v &= __m;                                               \
        for (__p = 0; __p < __w; __p += __e) __ret |= __v << __p; \
        (SF) ? __ret : (uint64_t)(uint32_t)__ret;                 \
    })

#define EMU_DP_DECODE_LOGICAL(N, R, S, SF, OUT)                                           \
    ({                                                                                    \
        uint32_t __n = (N), __r = (R), __s = (S), __len = 0;                              \
        uint32_t __levels, __esize, __value = (__n << 6) | (~__s & 0x3F);                 \
        uint64_t __pattern, *__out = (OUT);                                               \
        bool __sf = (SF), __ok = true;                                                    \
        int __bit;                                                                        \
        for (__bit = 6; __bit >= 0; __bit--)                                              \
            if (__value & (1U << __bit))                                                  \
            {                                                                             \
                __len = __bit;                                                            \
                break;                                                                    \
            }                                                                             \
        if (__bit < 1 || (!__sf && __len == 6)) __ok = false;                             \
        if (__ok)                                                                         \
        {                                                                                 \
            __levels = (1U << __len) - 1;                                                 \
            __s &= __levels;                                                              \
            __r &= __levels;                                                              \
            if (__s == __levels) __ok = false;                                            \
            else                                                                          \
            {                                                                             \
                __esize = 1U << __len;                                                    \
                __pattern = (__s == 63) ? ~0ULL : ((1ULL << (__s + 1)) - 1);              \
                *__out = EMU_DP_ROR_HW(EMU_DP_REPL(__pattern, __esize, __sf), __r, __sf); \
            }                                                                             \
        }                                                                                 \
        __ok;                                                                             \
    })

#define EMU_DP_DECODE_BITFIELD(N, R, S, SF, WMASK, TMASK)                            \
    ({                                                                               \
        uint32_t __n = (N), __r = (R), __s = (S), __len = 0;                         \
        uint32_t __levels, __diff, __esize, __value = (__n << 6) | (~__s & 0x3F);    \
        uint64_t __ones, __pattern, *__wmask = (WMASK), *__tmask = (TMASK);          \
        bool __sf = (SF), __ok = true;                                               \
        int __bit;                                                                   \
        for (__bit = 6; __bit >= 0; __bit--)                                         \
            if (__value & (1U << __bit))                                             \
            {                                                                        \
                __len = __bit;                                                       \
                break;                                                               \
            }                                                                        \
        if (__bit < 1 || (!__sf && __len == 6)) __ok = false;                        \
        if (__ok)                                                                    \
        {                                                                            \
            __levels = (1U << __len) - 1;                                            \
            __s &= __levels;                                                         \
            __r &= __levels;                                                         \
            __diff = (__s - __r) & __levels;                                         \
            __esize = 1U << __len;                                                   \
            __ones = (__s == 63) ? ~0ULL : ((1ULL << (__s + 1)) - 1);                \
            *__wmask = EMU_DP_ROR_HW(EMU_DP_REPL(__ones, __esize, __sf), __r, __sf); \
            __pattern = (__diff == 63) ? ~0ULL : ((1ULL << (__diff + 1)) - 1);       \
            *__tmask = EMU_DP_REPL(__pattern, __esize, __sf);                        \
        }                                                                            \
        __ok;                                                                        \
    })

#define EMU_DP_EXTR64_CASE(LSB)                                                            \
    case LSB:                                                                              \
        asm volatile("extr %0, %1, %2, #" #LSB "\n" : "=r"(__ret) : "r"(__hi), "r"(__lo)); \
        break

#define EMU_DP_EXTR32_CASE(LSB)                                                                                     \
    case LSB:                                                                                                       \
        asm volatile("extr %w0, %w1, %w2, #" #LSB "\n" : "=r"(__ret32) : "r"((uint32_t)__hi), "r"((uint32_t)__lo)); \
        break

#define EMU_DP_CASES_0_31(CASE) \
    CASE(0);                    \
    CASE(1);                    \
    CASE(2);                    \
    CASE(3);                    \
    CASE(4);                    \
    CASE(5);                    \
    CASE(6);                    \
    CASE(7);                    \
    CASE(8);                    \
    CASE(9);                    \
    CASE(10);                   \
    CASE(11);                   \
    CASE(12);                   \
    CASE(13);                   \
    CASE(14);                   \
    CASE(15);                   \
    CASE(16);                   \
    CASE(17);                   \
    CASE(18);                   \
    CASE(19);                   \
    CASE(20);                   \
    CASE(21);                   \
    CASE(22);                   \
    CASE(23);                   \
    CASE(24);                   \
    CASE(25);                   \
    CASE(26);                   \
    CASE(27);                   \
    CASE(28);                   \
    CASE(29);                   \
    CASE(30);                   \
    CASE(31)

#define EMU_DP_CASES_32_63(CASE) \
    CASE(32);                    \
    CASE(33);                    \
    CASE(34);                    \
    CASE(35);                    \
    CASE(36);                    \
    CASE(37);                    \
    CASE(38);                    \
    CASE(39);                    \
    CASE(40);                    \
    CASE(41);                    \
    CASE(42);                    \
    CASE(43);                    \
    CASE(44);                    \
    CASE(45);                    \
    CASE(46);                    \
    CASE(47);                    \
    CASE(48);                    \
    CASE(49);                    \
    CASE(50);                    \
    CASE(51);                    \
    CASE(52);                    \
    CASE(53);                    \
    CASE(54);                    \
    CASE(55);                    \
    CASE(56);                    \
    CASE(57);                    \
    CASE(58);                    \
    CASE(59);                    \
    CASE(60);                    \
    CASE(61);                    \
    CASE(62);                    \
    CASE(63)

#define EMU_DP_EXTR(H, L, S, SF)                        \
    ({                                                  \
        uint64_t __hi = (H), __lo = (L), __ret;         \
        uint32_t __ret32;                               \
        if (SF)                                         \
        {                                               \
            switch (S)                                  \
            {                                           \
                EMU_DP_CASES_0_31(EMU_DP_EXTR64_CASE);  \
                EMU_DP_CASES_32_63(EMU_DP_EXTR64_CASE); \
            default:                                    \
                __ret = __lo;                           \
                break;                                  \
            }                                           \
        }                                               \
        else                                            \
        {                                               \
            switch (S)                                  \
            {                                           \
                EMU_DP_CASES_0_31(EMU_DP_EXTR32_CASE);  \
            default:                                    \
                __ret32 = (uint32_t)__lo;               \
                break;                                  \
            }                                           \
            __ret = __ret32;                            \
        }                                               \
        __ret;                                          \
    })

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

#define EMU_DP_SEL64(INST, A, B, TAKE)                                                                                                               \
    ({                                                                                                                                               \
        uint64_t __ret;                                                                                                                              \
        asm volatile("cmp %w3, #0\n" INST " %0, %1, %2, ne\n" : "=r"(__ret) : "r"((uint64_t)(A)), "r"((uint64_t)(B)), "r"((uint32_t)(TAKE)) : "cc"); \
        __ret;                                                                                                                                       \
    })

#define EMU_DP_SEL32(INST, A, B, TAKE)                                                                                                                  \
    ({                                                                                                                                                  \
        uint32_t __ret;                                                                                                                                 \
        asm volatile("cmp %w3, #0\n" INST " %w0, %w1, %w2, ne\n" : "=r"(__ret) : "r"((uint32_t)(A)), "r"((uint32_t)(B)), "r"((uint32_t)(TAKE)) : "cc"); \
        __ret;                                                                                                                                          \
    })

#define EMU_DP_CCMP64_CASE(NZCV)                                                   \
    case NZCV:                                                                     \
        if (op_sub)                                                                \
            asm volatile("cmp %w3, #0\n"                                           \
                         "ccmp %1, %2, #" #NZCV ", ne\n"                           \
                         "mrs %0, nzcv\n"                                          \
                         : "=r"(flags)                                             \
                         : "r"((uint64_t)a), "r"((uint64_t)b), "r"((uint32_t)take) \
                         : "cc");                                                  \
        else                                                                       \
            asm volatile("cmp %w3, #0\n"                                           \
                         "ccmn %1, %2, #" #NZCV ", ne\n"                           \
                         "mrs %0, nzcv\n"                                          \
                         : "=r"(flags)                                             \
                         : "r"((uint64_t)a), "r"((uint64_t)b), "r"((uint32_t)take) \
                         : "cc");                                                  \
        break

#define EMU_DP_CCMP32_CASE(NZCV)                                                   \
    case NZCV:                                                                     \
        if (op_sub)                                                                \
            asm volatile("cmp %w3, #0\n"                                           \
                         "ccmp %w1, %w2, #" #NZCV ", ne\n"                         \
                         "mrs %0, nzcv\n"                                          \
                         : "=r"(flags)                                             \
                         : "r"((uint32_t)a), "r"((uint32_t)b), "r"((uint32_t)take) \
                         : "cc");                                                  \
        else                                                                       \
            asm volatile("cmp %w3, #0\n"                                           \
                         "ccmn %w1, %w2, #" #NZCV ", ne\n"                         \
                         "mrs %0, nzcv\n"                                          \
                         : "=r"(flags)                                             \
                         : "r"((uint32_t)a), "r"((uint32_t)b), "r"((uint32_t)take) \
                         : "cc");                                                  \
        break

#define EMU_DP_CCMP_CASES(CASE) \
    CASE(0);                    \
    CASE(1);                    \
    CASE(2);                    \
    CASE(3);                    \
    CASE(4);                    \
    CASE(5);                    \
    CASE(6);                    \
    CASE(7);                    \
    CASE(8);                    \
    CASE(9);                    \
    CASE(10);                   \
    CASE(11);                   \
    CASE(12);                   \
    CASE(13);                   \
    CASE(14);                   \
    CASE(15)

static __always_inline enum emu_insn_result emu_simulate_data_processing_insn(struct pt_regs *regs, uint32_t insn)
{
    if ((insn & 0x1F800000) == 0x11000000)
    {
        bool sf = (insn >> 31) & 1;
        bool op_sub = (insn >> 30) & 1;
        bool setflags = (insn >> 29) & 1;
        uint32_t sh = (insn >> 22) & 1;
        uint64_t imm = (insn >> 10) & 0xFFF;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a, result, nzcv = 0;

        if (sh) imm <<= 12;

        a = addr_reg_read(regs, rn);
        result = emu_addsub_hw(a, imm, op_sub, setflags, sf, &nzcv);

        if (setflags)
        {
            emu_write_nzcv(regs, nzcv);
            reg_write(regs, rd, result, sf);
        }
        else
        {
            addr_reg_write(regs, rd, result);
        }
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7FF00000) == 0x11C00000)
    {
        bool sf = (insn >> 31) & 1;
        bool is_min = (insn >> 19) & 1;
        bool is_unsigned = (insn >> 18) & 1;
        uint32_t imm8 = (insn >> 10) & 0xFF;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a = reg_read(regs, rn) & EMU_DP_MASK(sf);
        uint64_t b, result;

        if (is_unsigned) b = imm8;
        else b = sign_extend64(imm8, 7) & EMU_DP_MASK(sf);

        result = emu_minmax_hw(a, b, is_min, is_unsigned, sf);

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x1F800000) == 0x12000000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t opc = (insn >> 29) & 0x3;
        uint32_t n = (insn >> 22) & 1;
        uint32_t immr = (insn >> 16) & 0x3F;
        uint32_t imms = (insn >> 10) & 0x3F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t imm, a, result, nzcv = 0;

        if (!EMU_DP_DECODE_LOGICAL(n, immr, imms, sf, &imm)) return EMU_INSN_SKIP;

        a = reg_read(regs, rn) & EMU_DP_MASK(sf);
        result = emu_logic_hw(a, imm, opc, false, sf, &nzcv);

        if (opc == 3) emu_write_nzcv(regs, nzcv);

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7F800000) == 0x13000000 || (insn & 0x7F800000) == 0x33000000 || (insn & 0x7F800000) == 0x53000000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t opc = (insn >> 29) & 0x3;
        uint32_t n = (insn >> 22) & 1;
        uint32_t immr = (insn >> 16) & 0x3F;
        uint32_t imms = (insn >> 10) & 0x3F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint32_t width = sf ? 64 : 32;
        uint64_t src, dst, bot, result, wmask, tmask;

        if (opc == 3) return EMU_INSN_SKIP;
        if (sf != !!n) return EMU_INSN_SKIP;
        if (!sf && ((immr | imms) & 0x20)) return EMU_INSN_SKIP;
        if (!EMU_DP_DECODE_BITFIELD(n, immr, imms, sf, &wmask, &tmask)) return EMU_INSN_SKIP;

        src = reg_read(regs, rn) & EMU_DP_MASK(sf);
        dst = reg_read(regs, rd) & EMU_DP_MASK(sf);
        bot = EMU_DP_EXTR(src, src, immr, sf) & wmask;

        switch (opc)
        {
        case 0:
        {
            uint64_t sign_bit = (tmask == EMU_DP_MASK(sf)) ? (1ULL << (width - 1)) : ((tmask + 1) >> 1);

            result = bot & tmask;
            if (bot & sign_bit) result |= ~tmask;
            break;
        }
        case 1:
            result = (dst & ~wmask) | (bot & wmask);
            break;
        case 2:
            result = bot & tmask;
            break;
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7FA00000) == 0x13800000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t n = (insn >> 22) & 1;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t imms = (insn >> 10) & 0x3F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint32_t width = sf ? 64 : 32;
        uint64_t result;

        if (sf != !!n) return EMU_INSN_SKIP;
        if (imms >= width) return EMU_INSN_SKIP;

        result = EMU_DP_EXTR(reg_read(regs, rn), reg_read(regs, rm), imms, sf);

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x1F800000) == 0x12800000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t opc = (insn >> 29) & 0x3;
        uint32_t hw = (insn >> 21) & 0x3, shift = hw * 16;
        uint64_t imm16 = (insn >> 5) & 0xFFFF;
        uint32_t rd = insn & 0x1F;
        uint64_t result;

        if (opc == 1) return EMU_INSN_SKIP;
        if (!sf && (hw & 0x2)) return EMU_INSN_SKIP;

        if (opc == 0) result = ~(imm16 << shift);
        else if (opc == 2) result = imm16 << shift;
        else result = (reg_read(regs, rd) & ~(0xFFFFULL << shift)) | (imm16 << shift);

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x1F200000) == 0x0B000000)
    {
        bool sf = (insn >> 31) & 1;
        bool op_sub = (insn >> 30) & 1;
        bool setflags = (insn >> 29) & 1;
        uint32_t shift_type = (insn >> 22) & 0x3;
        uint32_t rm = (insn >> 16) & 0x1F, imm6 = (insn >> 10) & 0x3F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a, b, result, nzcv = 0;

        if (shift_type == 3) return EMU_INSN_SKIP;
        if (!sf && (imm6 & 0x20)) return EMU_INSN_SKIP;

        a = reg_read(regs, rn);
        b = EMU_DP_SHIFT(reg_read(regs, rm), shift_type, imm6, sf);
        result = emu_addsub_hw(a, b, op_sub, setflags, sf, &nzcv);

        if (setflags) emu_write_nzcv(regs, nzcv);
        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x1FE00000) == 0x0B200000)
    {
        bool sf = (insn >> 31) & 1;
        bool op_sub = (insn >> 30) & 1;
        bool setflags = (insn >> 29) & 1;
        uint32_t rm = (insn >> 16) & 0x1F, option = (insn >> 13) & 0x7, imm3 = (insn >> 10) & 0x7;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a, b, result, nzcv = 0;

        if (imm3 > 4) return EMU_INSN_SKIP;

        a = addr_reg_read(regs, rn);
        b = emu_extend_reg(reg_read(regs, rm), option, imm3);
        result = emu_addsub_hw(a, b, op_sub, setflags, sf, &nzcv);

        if (setflags)
        {
            emu_write_nzcv(regs, nzcv);
            reg_write(regs, rd, result, sf);
        }
        else
        {
            addr_reg_write(regs, rd, result);
        }
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x1F000000) == 0x0A000000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t opc = (insn >> 29) & 0x3;
        uint32_t shift_type = (insn >> 22) & 0x3;
        bool invert = (insn >> 21) & 1;
        uint32_t rm = (insn >> 16) & 0x1F, imm6 = (insn >> 10) & 0x3F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a, b, result, nzcv = 0;

        if (!sf && (imm6 & 0x20)) return EMU_INSN_SKIP;

        a = reg_read(regs, rn);
        b = EMU_DP_SHIFT(reg_read(regs, rm), shift_type, imm6, sf);
        result = emu_logic_hw(a, b, opc, invert, sf, &nzcv);

        if (opc == 3) emu_write_nzcv(regs, nzcv);
        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x3FE00000) == 0x1A800000)
    {
        bool sf = (insn >> 31) & 1;
        bool op = (insn >> 30) & 1;
        bool op2 = (insn >> 10) & 1;
        uint32_t fixed = (insn >> 11) & 1;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t cond = (insn >> 12) & 0xF;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a, b, result;
        bool take;

        if (fixed) return EMU_INSN_SKIP;

        a = reg_read(regs, rn);
        b = reg_read(regs, rm);
        take = emu_cond_holds_hw(regs->pstate, cond);
        switch ((op << 1) | op2)
        {
        case 0:
            result = sf ? EMU_DP_SEL64("csel", a, b, take) : EMU_DP_SEL32("csel", a, b, take);
            break;
        case 1:
            result = sf ? EMU_DP_SEL64("csinc", a, b, take) : EMU_DP_SEL32("csinc", a, b, take);
            break;
        case 2:
            result = sf ? EMU_DP_SEL64("csinv", a, b, take) : EMU_DP_SEL32("csinv", a, b, take);
            break;
        default:
            result = sf ? EMU_DP_SEL64("csneg", a, b, take) : EMU_DP_SEL32("csneg", a, b, take);
            break;
        }

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7FE00000) == 0x1AC00000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t opcode = (insn >> 10) & 0x3F;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a = reg_read(regs, rn) & EMU_DP_MASK(sf);
        uint64_t b = reg_read(regs, rm) & EMU_DP_MASK(sf);
        uint64_t result;

        switch (opcode)
        {
        case 2:
            result = sf ? EMU_INT_BIN64("udiv", a, b) : EMU_INT_BIN32("udiv", a, b);
            break;
        case 3:
            result = sf ? EMU_INT_BIN64("sdiv", a, b) : EMU_INT_BIN32("sdiv", a, b);
            break;
        case 8:
            result = sf ? EMU_INT_BIN64("lslv", a, b) : EMU_INT_BIN32("lslv", a, b);
            break;
        case 9:
            result = sf ? EMU_INT_BIN64("lsrv", a, b) : EMU_INT_BIN32("lsrv", a, b);
            break;
        case 10:
            result = sf ? EMU_INT_BIN64("asrv", a, b) : EMU_INT_BIN32("asrv", a, b);
            break;
        case 11:
            result = sf ? EMU_INT_BIN64("rorv", a, b) : EMU_INT_BIN32("rorv", a, b);
            break;
        case 0x10:
            result = EMU_DP_CRC32("crc32b", a, b);
            break;
        case 0x11:
            result = EMU_DP_CRC32("crc32h", a, b);
            break;
        case 0x12:
            result = EMU_DP_CRC32("crc32w", a, b);
            break;
        case 0x13:
            result = EMU_DP_CRC64("crc32x", a, b);
            break;
        case 0x14:
            result = EMU_DP_CRC32("crc32cb", a, b);
            break;
        case 0x15:
            result = EMU_DP_CRC32("crc32ch", a, b);
            break;
        case 0x16:
            result = EMU_DP_CRC32("crc32cw", a, b);
            break;
        case 0x17:
            result = EMU_DP_CRC64("crc32cx", a, b);
            break;
        case 0x18:
        case 0x19:
        case 0x1A:
        case 0x1B:
        {
            bool is_min = (opcode >> 1) & 1;
            bool is_unsigned = opcode & 1;

            result = emu_minmax_hw(a, b, is_min, is_unsigned, sf);
            break;
        }
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7FE08000) == 0x1B000000)
    {
        bool sf = (insn >> 31) & 1;
        bool op = (insn >> 15) & 1;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t ra = (insn >> 10) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t multiplicand = reg_read(regs, rn) & EMU_DP_MASK(sf);
        uint64_t multiplier = reg_read(regs, rm) & EMU_DP_MASK(sf);
        uint64_t addend = reg_read(regs, ra) & EMU_DP_MASK(sf);
        uint64_t result;

        if (sf)
        {
            if (op) asm volatile("msub %0, %1, %2, %3\n" : "=r"(result) : "r"(multiplicand), "r"(multiplier), "r"(addend));
            else asm volatile("madd %0, %1, %2, %3\n" : "=r"(result) : "r"(multiplicand), "r"(multiplier), "r"(addend));
        }
        else
        {
            uint32_t result32;

            if (op) asm volatile("msub %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)multiplicand), "r"((uint32_t)multiplier), "r"((uint32_t)addend));
            else asm volatile("madd %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)multiplicand), "r"((uint32_t)multiplier), "r"((uint32_t)addend));
            result = result32;
        }

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7F000000) == 0x1B000000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t op31 = (insn >> 21) & 0x7;
        bool o0 = (insn >> 15) & 1;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t ra = (insn >> 10) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t result;

        switch (op31)
        {
        case 0:
        {
            uint64_t n = reg_read(regs, rn) & EMU_DP_MASK(sf);
            uint64_t m = reg_read(regs, rm) & EMU_DP_MASK(sf);
            uint64_t a = reg_read(regs, ra) & EMU_DP_MASK(sf);

            if (sf)
            {
                if (o0) asm volatile("msub %0, %1, %2, %3\n" : "=r"(result) : "r"(n), "r"(m), "r"(a));
                else asm volatile("madd %0, %1, %2, %3\n" : "=r"(result) : "r"(n), "r"(m), "r"(a));
            }
            else
            {
                uint32_t result32;

                if (o0) asm volatile("msub %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)n), "r"((uint32_t)m), "r"((uint32_t)a));
                else asm volatile("madd %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)n), "r"((uint32_t)m), "r"((uint32_t)a));
                result = result32;
            }
            break;
        }
        case 1:
        {
            uint64_t a;

            if (!sf) return EMU_INSN_SKIP;
            a = reg_read(regs, ra);
            if (o0) asm volatile("smsubl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, rn)), "r"((uint32_t)reg_read(regs, rm)), "r"(a));
            else asm volatile("smaddl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, rn)), "r"((uint32_t)reg_read(regs, rm)), "r"(a));
            break;
        }
        case 2:
            if (!sf || o0) return EMU_INSN_SKIP;
            asm volatile("smulh %0, %1, %2\n" : "=r"(result) : "r"(reg_read(regs, rn)), "r"(reg_read(regs, rm)));
            break;
        case 5:
        {
            uint64_t a;

            if (!sf) return EMU_INSN_SKIP;
            a = reg_read(regs, ra);
            if (o0) asm volatile("umsubl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, rn)), "r"((uint32_t)reg_read(regs, rm)), "r"(a));
            else asm volatile("umaddl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, rn)), "r"((uint32_t)reg_read(regs, rm)), "r"(a));
            break;
        }
        case 6:
            if (!sf || o0) return EMU_INSN_SKIP;
            asm volatile("umulh %0, %1, %2\n" : "=r"(result) : "r"(reg_read(regs, rn)), "r"(reg_read(regs, rm)));
            break;
        default:
            return EMU_INSN_SKIP;
        }

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x1FE0FC00) == 0x1A000000)
    {
        bool sf = (insn >> 31) & 1;
        bool op_sub = (insn >> 30) & 1;
        bool setflags = (insn >> 29) & 1;
        uint32_t rm = (insn >> 16) & 0x1F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t x = reg_read(regs, rn);
        uint64_t y = reg_read(regs, rm);
        uint64_t result, nzcv;

        asm volatile("msr nzcv, %0\n" : : "r"(regs->pstate & (0xFULL << 28)) : "cc");

        if (sf)
        {
            if (op_sub)
                asm volatile("sbcs %0, %2, %3\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result), "=r"(nzcv)
                             : "r"(x), "r"(y)
                             : "cc");
            else
                asm volatile("adcs %0, %2, %3\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result), "=r"(nzcv)
                             : "r"(x), "r"(y)
                             : "cc");
        }
        else
        {
            uint32_t result32;

            if (op_sub)
                asm volatile("sbcs %w0, %w2, %w3\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result32), "=r"(nzcv)
                             : "r"((uint32_t)x), "r"((uint32_t)y)
                             : "cc");
            else
                asm volatile("adcs %w0, %w2, %w3\n"
                             "mrs %1, nzcv\n"
                             : "=r"(result32), "=r"(nzcv)
                             : "r"((uint32_t)x), "r"((uint32_t)y)
                             : "cc");
            result = result32;
        }

        if (setflags) emu_write_nzcv(regs, nzcv);

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x3FE00410) == 0x3A400000)
    {
        bool sf = (insn >> 31) & 1;
        bool op_sub = (insn >> 30) & 1;
        bool is_imm = (insn >> 11) & 1;
        uint32_t cond = (insn >> 12) & 0xF;
        uint32_t rn = (insn >> 5) & 0x1F;
        uint32_t nzcv = insn & 0xF;
        uint32_t rm_or_imm = (insn >> 16) & 0x1F;
        uint64_t a = reg_read(regs, rn);
        uint64_t b = is_imm ? (uint64_t)rm_or_imm : reg_read(regs, rm_or_imm);
        uint64_t flags;
        bool take = emu_cond_holds_hw(regs->pstate, cond);

        if (sf)
        {
            switch (nzcv)
            {
                EMU_DP_CCMP_CASES(EMU_DP_CCMP64_CASE);
            }
        }
        else
        {
            switch (nzcv)
            {
                EMU_DP_CCMP_CASES(EMU_DP_CCMP32_CASE);
            }
        }
        emu_write_nzcv(regs, flags);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7FE00000) == 0x5AC00000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t opcode2 = (insn >> 16) & 0x1F;
        uint32_t opcode = (insn >> 10) & 0x3F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t src = reg_read(regs, rn);
        uint64_t result;

        if (opcode2 != 0) return EMU_INSN_SKIP;

        switch (opcode)
        {
        case 0:
            result = sf ? EMU_DP_UN64("rbit", src) : EMU_DP_UN32("rbit", src);
            break;
        case 1:
            result = sf ? EMU_DP_UN64("rev16", src) : EMU_DP_UN32("rev16", src);
            break;
        case 2:
            result = sf ? EMU_DP_UN64("rev32", src) : EMU_DP_UN32("rev", src);
            break;
        case 3:
            if (!sf) return EMU_INSN_SKIP;
            result = EMU_DP_UN64("rev", src);
            break;
        case 4:
            result = sf ? EMU_DP_UN64("clz", src) : EMU_DP_UN32("clz", src);
            break;
        case 5:
            result = sf ? EMU_DP_UN64("cls", src) : EMU_DP_UN32("cls", src);
            break;
        case 6:
            result = sf ? EMU_DP_UN64("clz", EMU_DP_UN64("rbit", src)) : EMU_DP_UN32("clz", EMU_DP_UN32("rbit", src));
            break;
        case 7:
            result = EMU_DP_CNT(src, sf);
            break;
        case 8:
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

        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }

    return EMU_INSN_SKIP;
}

#undef EMU_DP_CCMP_CASES
#undef EMU_DP_CCMP32_CASE
#undef EMU_DP_CCMP64_CASE
#undef EMU_DP_SEL32
#undef EMU_DP_SEL64
#undef EMU_DP_CRC64
#undef EMU_DP_CRC32
#undef EMU_DP_CNT
#undef EMU_DP_UN32
#undef EMU_DP_UN64
#undef EMU_DP_SHIFT
#undef EMU_DP_EXTR
#undef EMU_DP_CASES_32_63
#undef EMU_DP_CASES_0_31
#undef EMU_DP_EXTR32_CASE
#undef EMU_DP_EXTR64_CASE
#undef EMU_DP_DECODE_BITFIELD
#undef EMU_DP_DECODE_LOGICAL
#undef EMU_DP_REPL
#undef EMU_DP_ROR_HW
#undef EMU_DP_MASK

static __always_inline bool emulate_insn(struct pt_regs *regs, const uint32_t *specified_insn, const struct emu_mem_access *mem_access)
{
    uint32_t insn;
    uint64_t pc = regs->pc;
    uint32_t iclass;
    __uint128_t fetched_insn;
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

    iclass = (insn >> 25) & 0xF;

    if (((insn & EMU_SYSREG_INSN_MASK) == EMU_SYSREG_MRS_INSN) || ((insn & EMU_SYSREG_INSN_MASK) == EMU_SYSREG_MSR_INSN) || insn == EMU_HINT_NOP_INSN || ((insn & 0x1F000000) == 0x10000000)) result = emu_simulate_system_insn(regs, insn, pc);
    else if ((iclass & 0xE) == 0xA) result = emu_simulate_branch_insn(regs, insn, pc);
    else if (iclass == 0x7 || iclass == 0xF) result = emu_simulate_fp_simd_insn(regs, insn, pc);
    else if (emu_is_lse_atomic(insn) || ((insn & 0x3F000000) == 0x08000000) || ((insn & 0x3F200C00) == 0x19000000) || ((insn & 0x3FFFFC00) == 0x38BFC000) || ((insn & 0x3A000000) == 0x28000000) || ((insn & 0x3A000000) == 0x38000000) || ((insn & 0x3B000000) == 0x18000000)) result = emu_simulate_load_store_insn(regs, insn, pc, mem_access);
    else result = emu_simulate_data_processing_insn(regs, insn);

    if (result == EMU_INSN_NOP) regs->pc = pc + 4;
    if (result == EMU_INSN_HANDLED || result == EMU_INSN_NOP) return true;

    ls_log_always_tag("emulate_insn", "failed pc=0x%llx insn=0x%08x bytes=%02x %02x %02x %02x\n", (unsigned long long)pc, insn, insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, (insn >> 24) & 0xff);
    return false;
}

#endif // EMULATE_INSN_H
