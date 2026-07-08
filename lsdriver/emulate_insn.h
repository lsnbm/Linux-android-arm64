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
 * ARM64 指令模拟器 (emulate_insn)
 *
 * 作用：断点命中后，在内核里模拟当前用户态指令并推进 pt_regs->pc，避免
 * 依赖硬件单步。当前主要服务于 HWBP/PTEBP 的命中后步过场景。
 *
 * 已支持：
 * - 分支：B、BL、BR、BLR、RET、B.cond、CBZ、CBNZ、TBZ、TBNZ。
 * - PC 相对：ADR、ADRP。
 * - 整数访存：LDR/STR、LDUR/STUR、LDP/STP、LDNP/STNP、LDRB/LDRH、
 *   LDRSB/LDRSH/LDRSW、LDR/LDRSW literal。
 * - 数据处理：ADD/SUB、ADDS/SUBS、CMP/CMN、AND/BIC/ORR/ORN/EOR/EON、
 *   ANDS/BICS、MOV/MVN、MOVN/MOVZ/MOVK、SBFM/UBFM/BFM、EXTR、CSEL、
 *   CSINC/CSINV/CSNEG、UDIV/SDIV、LSLV/LSRV/ASRV/RORV、MADD/MSUB、
 *   ADC/SBC、CCMP/CCMN、REV/RBIT、CLZ/CLS、SMADDL/UMADDL、SMSUBL/UMSUBL、
 *   SMULH/UMULH、CRC32/CRC32C、CTZ/CNT/ABS、SMAX/SMIN/UMAX/UMIN。
 * - LSE 原子访存：SWP、LDADD、LDCLR、LDEOR、LDSET、LDSMAX、LDSMIN、
 *   LDUMAX、LDUMIN、CAS、CASP。
 * - 预取：PRFM literal 按 NOP 处理，仅推进 PC。
 * - 系统寄存器：MRS TPIDR_EL0。
 * - FP/SIMD 访存和常见 FP/SIMD 运算：通过 arm64_reg.h 读取/写回
 *   Q0-Q31、FPSR、FPCR，支持本文件内已实现的 S/D/Q 访存、标量 FP、部分 AdvSIMD/NEON。
 *
 * 暂不支持：
 * - 数据处理：RMIF、SETF8、SETF16、CFINV、AXFLAG、XAFLAG。
 * - 独占/有序访存：LDXR、STXR、LDAXR、STLXR、LDAR、STLR。
 * - 其它 LSE/有序访存：LDAPR、LDAPUR 等未列出的编码。
 * - 指针认证和 MTE：PACIA/AUTIA、LDRAA/LDRAB、IRG/GMI/SUBP 等。
 * - SVE/SME 以及向量长度相关指令。
 * - FP16、复杂 AdvSIMD 重排/结构化访存：TBL/TBX、ZIP/UZP/TRN、INS/DUP、
 *   LD1/ST1/LD1R 等。
 * - 异常和大部分系统指令：SVC、HVC、SMC、BRK、MSR、除 TPIDR_EL0 外的 MRS。
 * ========================================================================= */

// 整数寄存器与条件执行辅助
static __always_inline uint64_t reg_read(struct pt_regs *regs, uint32_t n) { return (n == 31) ? 0ULL : regs->regs[n]; }
static __always_inline void reg_write(struct pt_regs *regs, uint32_t n, uint64_t val, bool sf)
{
    if (n != 31)
        regs->regs[n] = sf ? val : (uint64_t)(uint32_t)val;
}
static __always_inline uint64_t addr_reg_read(struct pt_regs *regs, uint32_t n) { return (n == 31) ? regs->sp : regs->regs[n]; }
static __always_inline void addr_reg_write(struct pt_regs *regs, uint32_t n, uint64_t val)
{
    if (n == 31)
        regs->sp = val;
    else
        regs->regs[n] = val;
}

#define EMU_COND_TEST(COND, PSTATE)                               \
    ({                                                            \
        uint32_t __ret;                                           \
        asm volatile("msr nzcv, %1\n"                             \
                     "cset %w0, " COND "\n"                       \
                     : "=r"(__ret)                                \
                     : "r"((uint64_t)((PSTATE) & (0xFULL << 28))) \
                     : "cc");                                     \
        __ret != 0;                                               \
    })

#define EMU_COND_TEST_CASE(NUM, COND) \
    case NUM:                         \
        return EMU_COND_TEST(COND, pstate)

static __always_inline bool emu_cond_test_hw(uint64_t pstate, uint32_t cond)
{
    switch (cond)
    {
        EMU_COND_TEST_CASE(0x0, "eq");
        EMU_COND_TEST_CASE(0x1, "ne");
        EMU_COND_TEST_CASE(0x2, "cs");
        EMU_COND_TEST_CASE(0x3, "cc");
        EMU_COND_TEST_CASE(0x4, "mi");
        EMU_COND_TEST_CASE(0x5, "pl");
        EMU_COND_TEST_CASE(0x6, "vs");
        EMU_COND_TEST_CASE(0x7, "vc");
        EMU_COND_TEST_CASE(0x8, "hi");
        EMU_COND_TEST_CASE(0x9, "ls");
        EMU_COND_TEST_CASE(0xA, "ge");
        EMU_COND_TEST_CASE(0xB, "lt");
        EMU_COND_TEST_CASE(0xC, "gt");
        EMU_COND_TEST_CASE(0xD, "le");
    default:
        return true;
    }
}

#undef EMU_COND_TEST_CASE
#undef EMU_COND_TEST

/* ---- 用户内存定宽读写：Load/Store 各分支共用的通用逻辑 ----
   bytes 仅取 1/2/4/8/16，覆盖 B/H/S/W/D/X/Q 全部访存位宽。
   读出的值一律零扩展进 128 位缓冲(高位清零)，符号扩展交由调用方按需处理。
    成功返回 0，__get_user/__put_user 失败返回 -EFAULT (调用方据此返回 EMU_INSN_FAULT)。 */
static __always_inline int emu_read_mem(uint64_t addr, int bytes, __uint128_t *out)
{
    __uint128_t v = 0;

    switch (bytes)
    {
    case 1:
    {
        u8 t;
        if (__get_user(t, (u8 __user *)addr))
            return -EFAULT;
        v = t;
        break;
    }
    case 2:
    {
        u16 t;
        if (__get_user(t, (u16 __user *)addr))
            return -EFAULT;
        v = t;
        break;
    }
    case 4:
    {
        u32 t;
        if (__get_user(t, (u32 __user *)addr))
            return -EFAULT;
        v = t;
        break;
    }
    case 8:
    {
        u64 t;
        if (__get_user(t, (u64 __user *)addr))
            return -EFAULT;
        v = t;
        break;
    }
    case 16:
    {
        u64 lo, hi;
        if (__get_user(lo, (u64 __user *)addr) || __get_user(hi, (u64 __user *)(addr + 8)))
            return -EFAULT;
        v = ((__uint128_t)hi << 64) | lo;
        break;
    }
    default:
        return -EFAULT;
    }

    *out = v;
    return 0;
}

// 把 val 的低 bytes 字节写入用户内存；bytes 取 1/2/4/8/16。成功 0，失败 -EFAULT。
static __always_inline int emu_write_mem(uint64_t addr, int bytes, __uint128_t val)
{
    switch (bytes)
    {
    case 1:
        return __put_user((u8)val, (u8 __user *)addr) ? -EFAULT : 0;
    case 2:
        return __put_user((u16)val, (u16 __user *)addr) ? -EFAULT : 0;
    case 4:
        return __put_user((u32)val, (u32 __user *)addr) ? -EFAULT : 0;
    case 8:
        return __put_user((u64)val, (u64 __user *)addr) ? -EFAULT : 0;
    case 16:
        if (__put_user((u64)val, (u64 __user *)addr) ||
            __put_user((u64)(val >> 64), (u64 __user *)(addr + 8)))
            return -EFAULT;
        return 0;
    default:
        return -EFAULT;
    }
}

static __always_inline uint64_t emu_mask_for_bytes(int bytes)
{
    return (bytes == 8) ? ~0ULL : ((1ULL << (bytes * 8)) - 1);
}

static __always_inline bool emu_is_lse_atomic(uint32_t insn)
{
    if ((insn & 0x3F200C00) == 0x38200000)
        return true; // SWP / LDADD / LDCLR / LDEOR / LDSET / LD{S,U}{MAX,MIN}
    if ((insn & 0x3FA07C00) == 0x08A07C00)
        return true; // CAS / CASA / CASL / CASAL
    if ((insn & 0x3F000000) == 0x08000000)
    {
        uint32_t size = (insn >> 30) & 0x3;
        uint32_t op = (insn >> 21) & 0xF;
        uint32_t rt2 = (insn >> 10) & 0x1F;

        return !(size & 2) && rt2 == 31 &&
               (op == 0x2 || op == 0x3 || op == 0x6 || op == 0x7);
    }
    return false;
}

/* ---- 数据处理指令通用逻辑：供 emu_simulate_data_processing_insn() 各分支复用 ---- */

static __always_inline void emu_write_nzcv(struct pt_regs *regs, uint64_t nzcv);

// 依据 a (加/减) b 的结果刷新 PSTATE.NZCV，供 ADDS/SUBS/CMP/CMN。
static __always_inline void emu_set_nzcv_addsub(struct pt_regs *regs, uint64_t a, uint64_t b, bool op_sub, bool sf)
{
    uint64_t nzcv;

    if (sf)
    {
        if (op_sub)
            asm volatile("subs xzr, %1, %2\n"
                         "mrs %0, nzcv\n"
                         : "=r"(nzcv)
                         : "r"(a), "r"(b)
                         : "cc");
        else
            asm volatile("adds xzr, %1, %2\n"
                         "mrs %0, nzcv\n"
                         : "=r"(nzcv)
                         : "r"(a), "r"(b)
                         : "cc");
    }
    else
    {
        if (op_sub)
            asm volatile("subs wzr, %w1, %w2\n"
                         "mrs %0, nzcv\n"
                         : "=r"(nzcv)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
        else
            asm volatile("adds wzr, %w1, %w2\n"
                         "mrs %0, nzcv\n"
                         : "=r"(nzcv)
                         : "r"((uint32_t)a), "r"((uint32_t)b)
                         : "cc");
    }
    emu_write_nzcv(regs, nzcv);
}

// 对寄存器值做移位：type 0=LSL 1=LSR 2=ASR 3=ROR；sf 决定 32/64 位。
// 移位量已由调用方保证 < 位宽（32 位时拒绝 imm6>=32）。
static __always_inline uint64_t emu_shift_reg(uint64_t val, uint32_t type, uint32_t amount, bool sf)
{
    uint64_t result64;
    uint32_t result32;

    if (sf)
    {
        switch (type)
        {
        case 0:
            asm volatile("lslv %0, %1, %2\n" : "=r"(result64) : "r"(val), "r"((uint64_t)amount) : "cc");
            return result64;
        case 1:
            asm volatile("lsrv %0, %1, %2\n" : "=r"(result64) : "r"(val), "r"((uint64_t)amount) : "cc");
            return result64;
        case 2:
            asm volatile("asrv %0, %1, %2\n" : "=r"(result64) : "r"(val), "r"((uint64_t)amount) : "cc");
            return result64;
        default:
            asm volatile("rorv %0, %1, %2\n" : "=r"(result64) : "r"(val), "r"((uint64_t)amount) : "cc");
            return result64;
        }
    }
    else
    {
        switch (type)
        {
        case 0:
            asm volatile("lslv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)val), "r"(amount) : "cc");
            return result32;
        case 1:
            asm volatile("lsrv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)val), "r"(amount) : "cc");
            return result32;
        case 2:
            asm volatile("asrv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)val), "r"(amount) : "cc");
            return result32;
        default:
            asm volatile("rorv %w0, %w1, %w2\n" : "=r"(result32) : "r"((uint32_t)val), "r"(amount) : "cc");
            return result32;
        }
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
    if (!shift)
        return x;
    asm volatile("lslv %0, %1, %2\n" : "=r"(x) : "r"(x), "r"((uint64_t)shift) : "cc");
    return x;
}

static __always_inline uint64_t emu_ror_width(uint64_t val, uint32_t shift, uint32_t width)
{
    uint64_t mask = (width == 64) ? ~0ULL : ((1ULL << width) - 1);

    shift &= width - 1;
    val &= mask;
    if (!shift)
        return val;
    return ((val >> shift) | (val << (width - shift))) & mask;
}

static __always_inline uint64_t emu_ror(uint64_t val, uint32_t shift, bool sf)
{
    return emu_ror_width(val, shift, sf ? 64 : 32);
}

static __always_inline uint64_t emu_replicate_bits(uint64_t val, uint32_t esize, bool sf)
{
    uint32_t width = sf ? 64 : 32;
    uint64_t result = 0;
    uint64_t mask = (esize == 64) ? ~0ULL : ((1ULL << esize) - 1);
    uint32_t pos;

    val &= mask;
    for (pos = 0; pos < width; pos += esize)
        result |= val << pos;
    return sf ? result : (uint32_t)result;
}

static __always_inline bool emu_decode_bitmask_imm(uint32_t n, uint32_t immr, uint32_t imms, bool sf, uint64_t *out)
{
    uint32_t len = 0;
    uint32_t levels, s, r, esize;
    uint64_t pattern;
    uint32_t value = (n << 6) | (~imms & 0x3F);
    int bit;

    for (bit = 6; bit >= 0; bit--)
    {
        if (value & (1U << bit))
        {
            len = bit;
            break;
        }
    }
    if (bit < 1)
        return false;
    if (!sf && len == 6)
        return false;

    levels = (1U << len) - 1;
    s = imms & levels;
    r = immr & levels;
    if (s == levels)
        return false;

    esize = 1U << len;
    pattern = (s == 63) ? ~0ULL : ((1ULL << (s + 1)) - 1);
    pattern = emu_ror_width(pattern, r, esize);
    *out = emu_replicate_bits(pattern, esize, sf);
    return true;
}

static __always_inline bool emu_decode_bitfield_masks(uint32_t n, uint32_t immr, uint32_t imms, bool sf,
                                                      uint64_t *wmask, uint64_t *tmask)
{
    uint32_t len = 0;
    uint32_t levels, s, r, diff, esize;
    uint64_t ones, pattern;
    uint32_t value = (n << 6) | (~imms & 0x3F);
    int bit;

    for (bit = 6; bit >= 0; bit--)
    {
        if (value & (1U << bit))
        {
            len = bit;
            break;
        }
    }
    if (bit < 1)
        return false;
    if (!sf && len == 6)
        return false;

    levels = (1U << len) - 1;
    s = imms & levels;
    r = immr & levels;
    diff = (s - r) & levels;
    esize = 1U << len;

    ones = (s == 63) ? ~0ULL : ((1ULL << (s + 1)) - 1);
    *wmask = emu_replicate_bits(emu_ror_width(ones, r, esize), esize, sf);

    pattern = (diff == 63) ? ~0ULL : ((1ULL << (diff + 1)) - 1);
    *tmask = emu_replicate_bits(pattern, esize, sf);
    return true;
}

static __always_inline uint64_t emu_mask_for_width(bool sf)
{
    return sf ? ~0ULL : 0xFFFFFFFFULL;
}

static __always_inline uint32_t emu_cnt_hw(uint64_t val, bool sf)
{
    __uint128_t saved_q0;
    uint32_t result;

    if (sf)
    {
        asm volatile(".arch_extension fp\n.arch_extension simd\n"
                     "str q0, [%2]\n"
                     "movi v0.2d, #0\n"
                     "fmov d0, %1\n"
                     "cnt v0.8b, v0.8b\n"
                     "addv b0, v0.8b\n"
                     "umov %w0, v0.b[0]\n"
                     "ldr q0, [%2]\n"
                     : "=&r"(result)
                     : "r"(val), "r"(&saved_q0)
                     : "memory", "cc");
        return result;
    }

    asm volatile(".arch_extension fp\n.arch_extension simd\n"
                 "str q0, [%2]\n"
                 "movi v0.2d, #0\n"
                 "fmov s0, %w1\n"
                 "cnt v0.8b, v0.8b\n"
                 "addv b0, v0.8b\n"
                 "umov %w0, v0.b[0]\n"
                 "ldr q0, [%2]\n"
                 : "=&r"(result)
                 : "r"((uint32_t)val), "r"(&saved_q0)
                 : "memory", "cc");
    return result;
}

static __always_inline uint32_t emu_crc32_hw(uint32_t acc, uint64_t data, uint32_t opcode)
{
    uint32_t result;

    switch (opcode)
    {
    case 0x10:
        asm volatile(".arch_extension crc\ncrc32b %w0, %w1, %w2\n" : "=r"(result) : "r"(acc), "r"((uint32_t)data));
        break;
    case 0x11:
        asm volatile(".arch_extension crc\ncrc32h %w0, %w1, %w2\n" : "=r"(result) : "r"(acc), "r"((uint32_t)data));
        break;
    case 0x12:
        asm volatile(".arch_extension crc\ncrc32w %w0, %w1, %w2\n" : "=r"(result) : "r"(acc), "r"((uint32_t)data));
        break;
    case 0x13:
        asm volatile(".arch_extension crc\ncrc32x %w0, %w1, %2\n" : "=r"(result) : "r"(acc), "r"(data));
        break;
    case 0x14:
        asm volatile(".arch_extension crc\ncrc32cb %w0, %w1, %w2\n" : "=r"(result) : "r"(acc), "r"((uint32_t)data));
        break;
    case 0x15:
        asm volatile(".arch_extension crc\ncrc32ch %w0, %w1, %w2\n" : "=r"(result) : "r"(acc), "r"((uint32_t)data));
        break;
    case 0x16:
        asm volatile(".arch_extension crc\ncrc32cw %w0, %w1, %w2\n" : "=r"(result) : "r"(acc), "r"((uint32_t)data));
        break;
    default:
        asm volatile(".arch_extension crc\ncrc32cx %w0, %w1, %2\n" : "=r"(result) : "r"(acc), "r"(data));
        break;
    }
    return result;
}

#define EMU_INT_BIN64(INST, A, B)                             \
    ({                                                        \
        uint64_t __ret;                                       \
        asm volatile(INST " %0, %1, %2\n"                     \
                     : "=r"(__ret)                            \
                     : "r"((uint64_t)(A)), "r"((uint64_t)(B)) \
                     : "cc");                                 \
        __ret;                                                \
    })

#define EMU_INT_BIN32(INST, A, B)                             \
    ({                                                        \
        uint32_t __ret;                                       \
        asm volatile(INST " %w0, %w1, %w2\n"                  \
                     : "=r"(__ret)                            \
                     : "r"((uint32_t)(A)), "r"((uint32_t)(B)) \
                     : "cc");                                 \
        __ret;                                                \
    })

#define EMU_INT_UN64(INST, A)             \
    ({                                    \
        uint64_t __ret;                   \
        asm volatile(INST " %0, %1\n"     \
                     : "=r"(__ret)        \
                     : "r"((uint64_t)(A)) \
                     : "cc");             \
        __ret;                            \
    })

#define EMU_INT_UN32(INST, A)             \
    ({                                    \
        uint32_t __ret;                   \
        asm volatile(INST " %w0, %w1\n"   \
                     : "=r"(__ret)        \
                     : "r"((uint32_t)(A)) \
                     : "cc");             \
        __ret;                            \
    })

static __always_inline uint64_t emu_addsub_hw(uint64_t a, uint64_t b, bool op_sub,
                                              bool setflags, bool sf, uint64_t *nzcv)
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

static __always_inline uint64_t emu_logic_hw(uint64_t a, uint64_t b, uint32_t opc,
                                             bool invert, bool sf, uint64_t *nzcv)
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

static __always_inline uint64_t emu_minmax_hw(uint64_t a, uint64_t b, bool is_min,
                                              bool is_unsigned, bool sf)
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

static __always_inline uint64_t emu_ctz_hw(uint64_t val, bool sf)
{
    return sf ? EMU_INT_UN64("clz", EMU_INT_UN64("rbit", val)) : EMU_INT_UN32("clz", EMU_INT_UN32("rbit", val));
}

static __always_inline uint64_t emu_abs_hw(uint64_t val, bool sf)
{
    uint64_t result64;
    uint32_t result32;

    if (sf)
    {
        asm volatile("cmp %1, #0\n"
                     "cneg %0, %1, mi\n"
                     : "=r"(result64)
                     : "r"(val)
                     : "cc");
        return result64;
    }
    asm volatile("cmp %w1, #0\n"
                 "cneg %w0, %w1, mi\n"
                 : "=r"(result32)
                 : "r"((uint32_t)val)
                 : "cc");
    return result32;
}

#define EMU_EXTR64_CASE(LSB)                                     \
    case LSB:                                                    \
        asm volatile("extr %0, %1, %2, #" #LSB "\n"              \
                     : "=r"(result64)                            \
                     : "r"((uint64_t)high), "r"((uint64_t)low)); \
        return result64

#define EMU_EXTR32_CASE(LSB)                                     \
    case LSB:                                                    \
        asm volatile("extr %w0, %w1, %w2, #" #LSB "\n"           \
                     : "=r"(result32)                            \
                     : "r"((uint32_t)high), "r"((uint32_t)low)); \
        return result32

#define EMU_EXTR_CASES_0_31(CASE_MACRO) \
    CASE_MACRO(0);                      \
    CASE_MACRO(1);                      \
    CASE_MACRO(2);                      \
    CASE_MACRO(3);                      \
    CASE_MACRO(4);                      \
    CASE_MACRO(5);                      \
    CASE_MACRO(6);                      \
    CASE_MACRO(7);                      \
    CASE_MACRO(8);                      \
    CASE_MACRO(9);                      \
    CASE_MACRO(10);                     \
    CASE_MACRO(11);                     \
    CASE_MACRO(12);                     \
    CASE_MACRO(13);                     \
    CASE_MACRO(14);                     \
    CASE_MACRO(15);                     \
    CASE_MACRO(16);                     \
    CASE_MACRO(17);                     \
    CASE_MACRO(18);                     \
    CASE_MACRO(19);                     \
    CASE_MACRO(20);                     \
    CASE_MACRO(21);                     \
    CASE_MACRO(22);                     \
    CASE_MACRO(23);                     \
    CASE_MACRO(24);                     \
    CASE_MACRO(25);                     \
    CASE_MACRO(26);                     \
    CASE_MACRO(27);                     \
    CASE_MACRO(28);                     \
    CASE_MACRO(29);                     \
    CASE_MACRO(30);                     \
    CASE_MACRO(31)

#define EMU_EXTR_CASES_32_63(CASE_MACRO) \
    CASE_MACRO(32);                      \
    CASE_MACRO(33);                      \
    CASE_MACRO(34);                      \
    CASE_MACRO(35);                      \
    CASE_MACRO(36);                      \
    CASE_MACRO(37);                      \
    CASE_MACRO(38);                      \
    CASE_MACRO(39);                      \
    CASE_MACRO(40);                      \
    CASE_MACRO(41);                      \
    CASE_MACRO(42);                      \
    CASE_MACRO(43);                      \
    CASE_MACRO(44);                      \
    CASE_MACRO(45);                      \
    CASE_MACRO(46);                      \
    CASE_MACRO(47);                      \
    CASE_MACRO(48);                      \
    CASE_MACRO(49);                      \
    CASE_MACRO(50);                      \
    CASE_MACRO(51);                      \
    CASE_MACRO(52);                      \
    CASE_MACRO(53);                      \
    CASE_MACRO(54);                      \
    CASE_MACRO(55);                      \
    CASE_MACRO(56);                      \
    CASE_MACRO(57);                      \
    CASE_MACRO(58);                      \
    CASE_MACRO(59);                      \
    CASE_MACRO(60);                      \
    CASE_MACRO(61);                      \
    CASE_MACRO(62);                      \
    CASE_MACRO(63)

static __always_inline uint64_t emu_extract_hw(uint64_t high, uint64_t low, uint32_t lsb, bool sf)
{
    uint64_t result64;
    uint32_t result32;

    if (sf)
    {
        switch (lsb)
        {
            EMU_EXTR_CASES_0_31(EMU_EXTR64_CASE);
            EMU_EXTR_CASES_32_63(EMU_EXTR64_CASE);
        default:
            return low;
        }
    }

    switch (lsb)
    {
        EMU_EXTR_CASES_0_31(EMU_EXTR32_CASE);
    default:
        return (uint32_t)low;
    }
}

#undef EMU_EXTR_CASES_32_63
#undef EMU_EXTR_CASES_0_31
#undef EMU_EXTR32_CASE
#undef EMU_EXTR64_CASE

#define EMU_COND_COMPARE64_NZCV_CASE(NZCV, COND)                            \
    case NZCV:                                                              \
        if (op_sub)                                                         \
            asm volatile("msr nzcv, %3\n"                                   \
                         "ccmp %1, %2, #" #NZCV ", " COND "\n"              \
                         "mrs %0, nzcv\n"                                   \
                         : "=r"(flags)                                      \
                         : "r"((uint64_t)a), "r"((uint64_t)b), "r"(nzcv_in) \
                         : "cc");                                           \
        else                                                                \
            asm volatile("msr nzcv, %3\n"                                   \
                         "ccmn %1, %2, #" #NZCV ", " COND "\n"              \
                         "mrs %0, nzcv\n"                                   \
                         : "=r"(flags)                                      \
                         : "r"((uint64_t)a), "r"((uint64_t)b), "r"(nzcv_in) \
                         : "cc");                                           \
        return flags

#define EMU_COND_COMPARE32_NZCV_CASE(NZCV, COND)                            \
    case NZCV:                                                              \
        if (op_sub)                                                         \
            asm volatile("msr nzcv, %3\n"                                   \
                         "ccmp %w1, %w2, #" #NZCV ", " COND "\n"            \
                         "mrs %0, nzcv\n"                                   \
                         : "=r"(flags)                                      \
                         : "r"((uint32_t)a), "r"((uint32_t)b), "r"(nzcv_in) \
                         : "cc");                                           \
        else                                                                \
            asm volatile("msr nzcv, %3\n"                                   \
                         "ccmn %w1, %w2, #" #NZCV ", " COND "\n"            \
                         "mrs %0, nzcv\n"                                   \
                         : "=r"(flags)                                      \
                         : "r"((uint32_t)a), "r"((uint32_t)b), "r"(nzcv_in) \
                         : "cc");                                           \
        return flags

#define EMU_COND_COMPARE_NZCV_CASES(CASE_MACRO, COND) \
    CASE_MACRO(0, COND);                              \
    CASE_MACRO(1, COND);                              \
    CASE_MACRO(2, COND);                              \
    CASE_MACRO(3, COND);                              \
    CASE_MACRO(4, COND);                              \
    CASE_MACRO(5, COND);                              \
    CASE_MACRO(6, COND);                              \
    CASE_MACRO(7, COND);                              \
    CASE_MACRO(8, COND);                              \
    CASE_MACRO(9, COND);                              \
    CASE_MACRO(10, COND);                             \
    CASE_MACRO(11, COND);                             \
    CASE_MACRO(12, COND);                             \
    CASE_MACRO(13, COND);                             \
    CASE_MACRO(14, COND);                             \
    CASE_MACRO(15, COND)

#define EMU_COND_COMPARE_CASE(NUM, COND)                                         \
    case NUM:                                                                    \
        if (sf)                                                                  \
        {                                                                        \
            switch (nzcv)                                                        \
            {                                                                    \
                EMU_COND_COMPARE_NZCV_CASES(EMU_COND_COMPARE64_NZCV_CASE, COND); \
            default:                                                             \
                return nzcv_in;                                                  \
            }                                                                    \
        }                                                                        \
        switch (nzcv)                                                            \
        {                                                                        \
            EMU_COND_COMPARE_NZCV_CASES(EMU_COND_COMPARE32_NZCV_CASE, COND);     \
        default:                                                                 \
            return nzcv_in;                                                      \
        }

static __always_inline uint64_t emu_cond_compare_hw(uint64_t a, uint64_t b,
                                                    uint64_t pstate, uint32_t nzcv,
                                                    uint32_t cond, bool op_sub, bool sf)
{
    uint64_t flags;
    uint64_t nzcv_in = pstate & (0xFULL << 28);

    switch (cond)
    {
        EMU_COND_COMPARE_CASE(0x0, "eq");
        EMU_COND_COMPARE_CASE(0x1, "ne");
        EMU_COND_COMPARE_CASE(0x2, "cs");
        EMU_COND_COMPARE_CASE(0x3, "cc");
        EMU_COND_COMPARE_CASE(0x4, "mi");
        EMU_COND_COMPARE_CASE(0x5, "pl");
        EMU_COND_COMPARE_CASE(0x6, "vs");
        EMU_COND_COMPARE_CASE(0x7, "vc");
        EMU_COND_COMPARE_CASE(0x8, "hi");
        EMU_COND_COMPARE_CASE(0x9, "ls");
        EMU_COND_COMPARE_CASE(0xA, "ge");
        EMU_COND_COMPARE_CASE(0xB, "lt");
        EMU_COND_COMPARE_CASE(0xC, "gt");
        EMU_COND_COMPARE_CASE(0xD, "le");
    default:
        if (sf)
        {
            switch (nzcv)
            {
                EMU_COND_COMPARE_NZCV_CASES(EMU_COND_COMPARE64_NZCV_CASE, "al");
            default:
                return nzcv_in;
            }
        }
        switch (nzcv)
        {
            EMU_COND_COMPARE_NZCV_CASES(EMU_COND_COMPARE32_NZCV_CASE, "al");
        default:
            return nzcv_in;
        }
    }
}

#undef EMU_COND_COMPARE_CASE
#undef EMU_COND_COMPARE_NZCV_CASES
#undef EMU_COND_COMPARE32_NZCV_CASE
#undef EMU_COND_COMPARE64_NZCV_CASE

#define EMU_COND_SELECT64(COND, A, B, NZCV, OP, OP2)                                         \
    ({                                                                                       \
        uint64_t __ret;                                                                      \
        if (OP)                                                                              \
        {                                                                                    \
            if (OP2)                                                                         \
                asm volatile("msr nzcv, %3\n"                                                \
                             "csneg %0, %1, %2, " COND "\n"                                  \
                             : "=r"(__ret)                                                   \
                             : "r"((uint64_t)(A)), "r"((uint64_t)(B)), "r"((uint64_t)(NZCV)) \
                             : "cc");                                                        \
            else                                                                             \
                asm volatile("msr nzcv, %3\n"                                                \
                             "csinv %0, %1, %2, " COND "\n"                                  \
                             : "=r"(__ret)                                                   \
                             : "r"((uint64_t)(A)), "r"((uint64_t)(B)), "r"((uint64_t)(NZCV)) \
                             : "cc");                                                        \
        }                                                                                    \
        else                                                                                 \
        {                                                                                    \
            if (OP2)                                                                         \
                asm volatile("msr nzcv, %3\n"                                                \
                             "csinc %0, %1, %2, " COND "\n"                                  \
                             : "=r"(__ret)                                                   \
                             : "r"((uint64_t)(A)), "r"((uint64_t)(B)), "r"((uint64_t)(NZCV)) \
                             : "cc");                                                        \
            else                                                                             \
                asm volatile("msr nzcv, %3\n"                                                \
                             "csel %0, %1, %2, " COND "\n"                                   \
                             : "=r"(__ret)                                                   \
                             : "r"((uint64_t)(A)), "r"((uint64_t)(B)), "r"((uint64_t)(NZCV)) \
                             : "cc");                                                        \
        }                                                                                    \
        __ret;                                                                               \
    })

#define EMU_COND_SELECT32(COND, A, B, NZCV, OP, OP2)                                         \
    ({                                                                                       \
        uint32_t __ret;                                                                      \
        if (OP)                                                                              \
        {                                                                                    \
            if (OP2)                                                                         \
                asm volatile("msr nzcv, %3\n"                                                \
                             "csneg %w0, %w1, %w2, " COND "\n"                               \
                             : "=r"(__ret)                                                   \
                             : "r"((uint32_t)(A)), "r"((uint32_t)(B)), "r"((uint64_t)(NZCV)) \
                             : "cc");                                                        \
            else                                                                             \
                asm volatile("msr nzcv, %3\n"                                                \
                             "csinv %w0, %w1, %w2, " COND "\n"                               \
                             : "=r"(__ret)                                                   \
                             : "r"((uint32_t)(A)), "r"((uint32_t)(B)), "r"((uint64_t)(NZCV)) \
                             : "cc");                                                        \
        }                                                                                    \
        else                                                                                 \
        {                                                                                    \
            if (OP2)                                                                         \
                asm volatile("msr nzcv, %3\n"                                                \
                             "csinc %w0, %w1, %w2, " COND "\n"                               \
                             : "=r"(__ret)                                                   \
                             : "r"((uint32_t)(A)), "r"((uint32_t)(B)), "r"((uint64_t)(NZCV)) \
                             : "cc");                                                        \
            else                                                                             \
                asm volatile("msr nzcv, %3\n"                                                \
                             "csel %w0, %w1, %w2, " COND "\n"                                \
                             : "=r"(__ret)                                                   \
                             : "r"((uint32_t)(A)), "r"((uint32_t)(B)), "r"((uint64_t)(NZCV)) \
                             : "cc");                                                        \
        }                                                                                    \
        __ret;                                                                               \
    })

#define EMU_COND_SELECT_CASE(NUM, COND) \
    case NUM:                           \
        return sf ? EMU_COND_SELECT64(COND, a, b, nzcv, op, op2) : EMU_COND_SELECT32(COND, a, b, nzcv, op, op2)

static __always_inline uint64_t emu_cond_select_hw(uint64_t a, uint64_t b,
                                                   uint64_t pstate, uint32_t cond,
                                                   bool op, bool op2, bool sf)
{
    uint64_t nzcv = pstate & (0xFULL << 28);

    switch (cond)
    {
        EMU_COND_SELECT_CASE(0x0, "eq");
        EMU_COND_SELECT_CASE(0x1, "ne");
        EMU_COND_SELECT_CASE(0x2, "cs");
        EMU_COND_SELECT_CASE(0x3, "cc");
        EMU_COND_SELECT_CASE(0x4, "mi");
        EMU_COND_SELECT_CASE(0x5, "pl");
        EMU_COND_SELECT_CASE(0x6, "vs");
        EMU_COND_SELECT_CASE(0x7, "vc");
        EMU_COND_SELECT_CASE(0x8, "hi");
        EMU_COND_SELECT_CASE(0x9, "ls");
        EMU_COND_SELECT_CASE(0xA, "ge");
        EMU_COND_SELECT_CASE(0xB, "lt");
        EMU_COND_SELECT_CASE(0xC, "gt");
        EMU_COND_SELECT_CASE(0xD, "le");
    default:
        return sf ? EMU_COND_SELECT64("al", a, b, nzcv, op, op2) : EMU_COND_SELECT32("al", a, b, nzcv, op, op2);
    }
}

#undef EMU_COND_SELECT_CASE

static __always_inline uint64_t emu_sign_extend_hw(uint64_t val, int bytes)
{
    uint64_t result;

    switch (bytes)
    {
    case 1:
        asm volatile("sxtb %0, %w1\n" : "=r"(result) : "r"((uint32_t)val));
        return result;
    case 2:
        asm volatile("sxth %0, %w1\n" : "=r"(result) : "r"((uint32_t)val));
        return result;
    case 4:
        asm volatile("sxtw %0, %w1\n" : "=r"(result) : "r"((uint32_t)val));
        return result;
    default:
        return val;
    }
}

/* ---- FP / AdvSIMD 运算辅助：固定 helper 执行已识别的硬件指令 ---- */

static __always_inline uint32_t emu_fp_low32(__uint128_t v)
{
    return (uint32_t)v;
}

static __always_inline uint64_t emu_fp_low64(__uint128_t v)
{
    return (uint64_t)v;
}

static __always_inline void emu_fp_set_low32(__uint128_t *v, uint32_t x)
{
    *v = (__uint128_t)x;
}

static __always_inline void emu_fp_set_low64(__uint128_t *v, uint64_t x)
{
    *v = (__uint128_t)x;
}

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

#define EMU_FP_EXT64_CASE(IMM)                                    \
    case IMM:                                                     \
        EMU_FP_EXT("ext v0.8b, v1.8b, v2.8b, #" #IMM, dst, a, b); \
        return EMU_INSN_HANDLED

#define EMU_FP_EXT128_CASE(IMM)                                      \
    case IMM:                                                        \
        EMU_FP_EXT("ext v0.16b, v1.16b, v2.16b, #" #IMM, dst, a, b); \
        return EMU_INSN_HANDLED

static __always_inline enum emu_insn_result emu_advsimd_ext_hw(__uint128_t *dst,
                                                               const __uint128_t *a,
                                                               const __uint128_t *b,
                                                               uint32_t imm,
                                                               bool q)
{
    if (q)
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

#undef EMU_FP_EXT128_CASE
#undef EMU_FP_EXT64_CASE

#define EMU_FP_FCSEL(INST, DST, A, B, PSTATE)                     \
    do                                                            \
    {                                                             \
        asm volatile(".arch_extension fp\n.arch_extension simd\n" \
                     "msr nzcv, %3\n"                             \
                     "ldr q1, [%1]\n"                             \
                     "ldr q2, [%2]\n" INST "\n"                   \
                     "str q0, [%0]\n"                             \
                     :                                            \
                     : "r"(DST), "r"(A), "r"(B),                  \
                       "r"((uint64_t)((PSTATE) & (0xFULL << 28))) \
                     : "cc", "memory");                           \
    } while (0)

#define EMU_FP_FCSEL_CASE(NUM, COND)                                    \
    case NUM:                                                           \
        if (type == 0)                                                  \
            EMU_FP_FCSEL("fcsel s0, s1, s2, " COND, dst, a, b, pstate); \
        else                                                            \
            EMU_FP_FCSEL("fcsel d0, d1, d2, " COND, dst, a, b, pstate); \
        return true

static __always_inline bool emu_fp_fcsel_hw(__uint128_t *dst,
                                            const __uint128_t *a,
                                            const __uint128_t *b,
                                            uint64_t pstate,
                                            uint32_t cond,
                                            uint32_t type)
{
    if (type > 1)
        return false;

    switch (cond)
    {
        EMU_FP_FCSEL_CASE(0x0, "eq");
        EMU_FP_FCSEL_CASE(0x1, "ne");
        EMU_FP_FCSEL_CASE(0x2, "cs");
        EMU_FP_FCSEL_CASE(0x3, "cc");
        EMU_FP_FCSEL_CASE(0x4, "mi");
        EMU_FP_FCSEL_CASE(0x5, "pl");
        EMU_FP_FCSEL_CASE(0x6, "vs");
        EMU_FP_FCSEL_CASE(0x7, "vc");
        EMU_FP_FCSEL_CASE(0x8, "hi");
        EMU_FP_FCSEL_CASE(0x9, "ls");
        EMU_FP_FCSEL_CASE(0xA, "ge");
        EMU_FP_FCSEL_CASE(0xB, "lt");
        EMU_FP_FCSEL_CASE(0xC, "gt");
        EMU_FP_FCSEL_CASE(0xD, "le");
    default:
        if (type == 0)
            EMU_FP_FCSEL("fcsel s0, s1, s2, al", dst, a, b, pstate);
        else
            EMU_FP_FCSEL("fcsel d0, d1, d2, al", dst, a, b, pstate);
        return true;
    }
}

#undef EMU_FP_FCSEL_CASE
#undef EMU_FP_FCSEL

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

static __always_inline void emu_write_nzcv(struct pt_regs *regs, uint64_t nzcv)
{
    regs->pstate = (regs->pstate & ~((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28))) |
                   (nzcv & ((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28)));
}

/* ---- 指令位移与小模拟器：按内核 probes/simulate-insn.c 的风格拆分 ---- */

static __always_inline s64 emu_bbl_displacement(uint32_t insn)
{
    return sign_extend64((s64)(insn & 0x3FFFFFF) << 2, 27);
}

static __always_inline s64 emu_bcond_displacement(uint32_t insn)
{
    return sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);
}

static __always_inline s64 emu_cbz_displacement(uint32_t insn)
{
    return sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);
}

static __always_inline s64 emu_tbz_displacement(uint32_t insn)
{
    return sign_extend64((s64)((insn >> 5) & 0x3FFF) << 2, 15);
}

static __always_inline s64 emu_ldr_literal_displacement(uint32_t insn)
{
    return sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);
}

static __always_inline void emu_simulate_b_bl(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    if (insn & (1U << 31))
        regs->regs[30] = pc + 4;
    regs->pc = pc + emu_bbl_displacement(insn);
}

static __always_inline enum emu_insn_result emu_simulate_br_blr_ret(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t rn = (insn >> 5) & 0x1F;
    uint32_t opc = (insn >> 21) & 0x3;
    uint64_t target;

    if (opc > 2)
        return EMU_INSN_SKIP;

    target = reg_read(regs, rn);
    regs->pc = target;
    if (opc == 1)
        regs->regs[30] = pc + 4;

    return EMU_INSN_HANDLED;
}

static __always_inline void emu_simulate_b_cond(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    regs->pc = emu_cond_test_hw(regs->pstate, insn & 0xF) ? (pc + emu_bcond_displacement(insn)) : (pc + 4);
}

static __always_inline void emu_simulate_cbz_cbnz(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t rt = insn & 0x1F;
    uint64_t val = ((insn >> 31) & 1) ? reg_read(regs, rt) : (uint32_t)reg_read(regs, rt);
    bool jump = ((insn >> 24) & 1) ? (val != 0) : (val == 0);

    regs->pc = jump ? (pc + emu_cbz_displacement(insn)) : (pc + 4);
}

static __always_inline void emu_simulate_tbz_tbnz(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t rt = insn & 0x1F;
    uint32_t pos = (((insn >> 31) & 1) << 5) | ((insn >> 19) & 0x1F);
    bool jump = (((reg_read(regs, rt) >> pos) & 1) == ((insn >> 24) & 1));

    regs->pc = jump ? (pc + emu_tbz_displacement(insn)) : (pc + 4);
}

static __always_inline enum emu_insn_result emu_simulate_branch_insn(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t op_branch = insn & 0xFC000000;

    if (op_branch == 0x14000000 || op_branch == 0x94000000)
    {
        emu_simulate_b_bl(regs, insn, pc);
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0xFFFFFC1F) == 0xD61F0000 ||
        (insn & 0xFFFFFC1F) == 0xD63F0000 ||
        (insn & 0xFFFFFC1F) == 0xD65F0000)
        return emu_simulate_br_blr_ret(regs, insn, pc);
    if ((insn & 0xFF000010) == 0x54000000)
    {
        emu_simulate_b_cond(regs, insn, pc);
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7E000000) == 0x34000000)
    {
        emu_simulate_cbz_cbnz(regs, insn, pc);
        return EMU_INSN_HANDLED;
    }
    if ((insn & 0x7E000000) == 0x36000000)
    {
        emu_simulate_tbz_tbnz(regs, insn, pc);
        return EMU_INSN_HANDLED;
    }

    return EMU_INSN_SKIP;
}

static __always_inline enum emu_insn_result emu_simulate_system_insn(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t rt = insn & 0x1F;

    if ((insn & 0xFFFFFFE0) == 0xD53BD040)
    {
        reg_write(regs, rt, read_sysreg(tpidr_el0), true);
        regs->pc = pc + 4;
        return EMU_INSN_HANDLED;
    }

    return EMU_INSN_SKIP;
}

static __always_inline void emu_simulate_adr_adrp(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t rd = insn & 0x1F;
    s64 imm = sign_extend64(((insn >> 5) & 0x7FFFF) << 2 | ((insn >> 29) & 0x3), 20);

    // Rd=31 在 ADR/ADRP 中表示丢弃结果(XZR)；pt_regs.regs[] 只有 0..30，不能写 regs[31]。
    if (rd != 31)
        regs->regs[rd] = (insn & 0x80000000) ? ((pc & ~0xFFFULL) + (imm << 12)) : (pc + imm);
    regs->pc = pc + 4;
}

static __always_inline enum emu_insn_result emu_simulate_lse_rmw(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t size = (insn >> 30) & 0x3;
    bool acquire = (insn >> 23) & 1;
    bool release = (insn >> 22) & 1;
    uint32_t rs = (insn >> 16) & 0x1F;
    uint32_t op = (insn >> 12) & 0xF;
    uint32_t rn = (insn >> 5) & 0x1F;
    uint32_t rt = insn & 0x1F;
    int bytes = 1 << size;
    uint64_t mask = emu_mask_for_bytes(bytes);
    uint64_t addr = addr_reg_read(regs, rn);
    uint64_t src = reg_read(regs, rs) & mask;
    uint64_t old, newval;
    __uint128_t mem;

    if (addr & (bytes - 1))
        return EMU_INSN_FAULT;
    if (op > 8 || op == 9)
        return EMU_INSN_SKIP;
    if (emu_read_mem(addr, bytes, &mem))
        return EMU_INSN_FAULT;

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
        newval = emu_minmax_hw(emu_sign_extend_hw(old, bytes), emu_sign_extend_hw(src, bytes), false, false, true) & mask;
        break;
    case 5: // LDSMIN
        newval = emu_minmax_hw(emu_sign_extend_hw(old, bytes), emu_sign_extend_hw(src, bytes), true, false, true) & mask;
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

    if (release)
        smp_mb();
    if (emu_write_mem(addr, bytes, newval))
        return EMU_INSN_FAULT;
    if (acquire)
        smp_mb();

    reg_write(regs, rt, old, size == 3);
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_lse_cas(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t size = (insn >> 30) & 0x3;
    bool acquire = (insn >> 22) & 1;
    bool release = (insn >> 15) & 1;
    uint32_t rs = (insn >> 16) & 0x1F;
    uint32_t rn = (insn >> 5) & 0x1F;
    uint32_t rt = insn & 0x1F;
    int bytes = 1 << size;
    uint64_t mask = emu_mask_for_bytes(bytes);
    uint64_t addr = addr_reg_read(regs, rn);
    uint64_t expected = reg_read(regs, rs) & mask;
    uint64_t desired = reg_read(regs, rt) & mask;
    uint64_t old;
    __uint128_t mem;

    if (addr & (bytes - 1))
        return EMU_INSN_FAULT;
    if (emu_read_mem(addr, bytes, &mem))
        return EMU_INSN_FAULT;

    old = (uint64_t)mem & mask;
    if (old == expected)
    {
        if (release)
            smp_mb();
        if (emu_write_mem(addr, bytes, desired))
            return EMU_INSN_FAULT;
    }
    if (acquire)
        smp_mb();

    reg_write(regs, rs, old, size == 3);
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_lse_casp(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t size = (insn >> 30) & 0x3;
    uint32_t op = (insn >> 21) & 0xF;
    bool acquire = op & 0x4;
    bool release = op & 0x1;
    uint32_t rs = (insn >> 16) & 0x1F;
    uint32_t rn = (insn >> 5) & 0x1F;
    uint32_t rt = insn & 0x1F;
    int bytes = (size == 0) ? 4 : 8;
    int total = bytes * 2;
    uint64_t mask = emu_mask_for_bytes(bytes);
    uint64_t addr = addr_reg_read(regs, rn);
    uint64_t old0, old1, exp0, exp1, new0, new1;
    __uint128_t mem0, mem1, pair;

    if ((size & 2) || (rs & 1) || rs >= 31 || (rt & 1) || rt >= 31)
        return EMU_INSN_SKIP;
    if (addr & (total - 1))
        return EMU_INSN_FAULT;
    if (emu_read_mem(addr, bytes, &mem0) || emu_read_mem(addr + bytes, bytes, &mem1))
        return EMU_INSN_FAULT;

    old0 = (uint64_t)mem0 & mask;
    old1 = (uint64_t)mem1 & mask;
    exp0 = reg_read(regs, rs) & mask;
    exp1 = reg_read(regs, rs + 1) & mask;
    new0 = reg_read(regs, rt) & mask;
    new1 = reg_read(regs, rt + 1) & mask;

    if (old0 == exp0 && old1 == exp1)
    {
        pair = ((__uint128_t)new1 << (bytes * 8)) | new0;
        if (release)
            smp_mb();
        if (emu_write_mem(addr, total, pair))
            return EMU_INSN_FAULT;
    }
    if (acquire)
        smp_mb();

    reg_write(regs, rs, old0, bytes == 8);
    reg_write(regs, rs + 1, old1, bytes == 8);
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_lse_atomic(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    if ((insn & 0x3F200C00) == 0x38200000)
        return emu_simulate_lse_rmw(regs, insn, pc);
    if ((insn & 0x3FA07C00) == 0x08A07C00)
        return emu_simulate_lse_cas(regs, insn, pc);
    if (emu_is_lse_atomic(insn))
        return emu_simulate_lse_casp(regs, insn, pc);
    return EMU_INSN_SKIP;
}

static __always_inline enum emu_insn_result emu_simulate_load_store_insn(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    bool is_fp = (insn & 0x04000000) != 0;
    uint32_t size = (insn >> 30) & 0x3;
    __uint128_t fp_regs[32];
    uint32_t fpsr = 0, fpcr = 0;
    bool fp_dirty = false;

    if (is_fp)
    {
        int i;

        for (i = 0; i < 32; i++)
            read_q_reg(i, &fp_regs[i]);
        fpsr = read_fpsr();
        fpcr = read_fpcr();
    }

    if ((insn & 0x3B000000) == 0x18000000)
    {
        uint32_t rt = insn & 0x1F;
        uint64_t addr = pc + emu_ldr_literal_displacement(insn);

        if (is_fp)
        {
            int bytes = (size == 0) ? 4 : ((size == 1) ? 8 : 16);
            __uint128_t val;

            if (size > 2)
                return EMU_INSN_SKIP;
            if (emu_read_mem(addr, bytes, &val))
                return EMU_INSN_FAULT;
            fp_regs[rt] = val;
            fp_dirty = true;
        }
        else
        {
            __uint128_t val;

            if (size == 3)
                return EMU_INSN_NOP;
            if (size == 0)
            {
                if (emu_read_mem(addr, 4, &val))
                    return EMU_INSN_FAULT;
                reg_write(regs, rt, (u64)val, false);
            }
            else if (size == 1)
            {
                if (emu_read_mem(addr, 8, &val))
                    return EMU_INSN_FAULT;
                reg_write(regs, rt, (u64)val, true);
            }
            else if (size == 2)
            {
                if (emu_read_mem(addr, 4, &val))
                    return EMU_INSN_FAULT;
                reg_write(regs, rt, emu_sign_extend_hw((u64)val, 4), true);
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
            if (opc_pair == 3)
                return EMU_INSN_SKIP;
            bytes = 4 << opc_pair;
        }
        else
        {
            if (opc_pair == 3 || (opc_pair == 1 && !load))
                return EMU_INSN_SKIP;
            bytes = (opc_pair == 2) ? 8 : 4;
        }
        off = sign_extend64((s64)((insn >> 15) & 0x7F), 6) * bytes;
        addr = (idx == 1) ? base : (base + off);
        if ((idx & 1) && rn != 31 && (rn == rt || rn == rt2))
            return EMU_INSN_SKIP;
        if (load && rt == rt2)
            return EMU_INSN_SKIP;

        if (load)
        {
            __uint128_t val1, val2;

            if (emu_read_mem(addr, bytes, &val1) || emu_read_mem(addr + bytes, bytes, &val2))
                return EMU_INSN_FAULT;
            if (is_fp)
            {
                fp_regs[rt] = val1;
                fp_regs[rt2] = val2;
                fp_dirty = true;
            }
            else if (opc_pair == 1)
            {
                reg_write(regs, rt, emu_sign_extend_hw((u64)val1, 4), true);
                reg_write(regs, rt2, emu_sign_extend_hw((u64)val2, 4), true);
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

            if (emu_write_mem(addr, bytes, val1) || emu_write_mem(addr + bytes, bytes, val2))
                return EMU_INSN_FAULT;
        }
        if (idx & 1)
            addr_reg_write(regs, rn, base + off);
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
            if (size == 0 && (opc & 2))
                bytes = 16;
            else
                bytes = (1 << size);
        }
        else
        {
            bytes = (1 << size);
        }

        if (!is_fp && size == 3 && opc >= 2)
            return (opc == 2) ? EMU_INSN_NOP : EMU_INSN_SKIP;
        if (!is_fp && size == 2 && opc == 3)
            return EMU_INSN_SKIP;

        if ((insn >> 24) & 1)
        {
            addr = base + (((insn >> 10) & 0xFFF) * bytes);
        }
        else
        {
            uint32_t idx = (insn >> 10) & 0x3;
            bool reg_form = ((insn >> 21) & 1) != 0;
            s64 imm9 = sign_extend64((s64)((insn >> 12) & 0x1FF), 8);

            if (reg_form && idx != 2)
                return EMU_INSN_SKIP;

            if (idx == 0)
                addr = base + imm9;
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

                if (opt != 2 && opt != 3 && opt != 6 && opt != 7)
                    return EMU_INSN_SKIP;
                shift = ((insn >> 12) & 1) ? __builtin_ctz(bytes) : 0;
                ext = emu_extend_reg(ext, opt, shift);
                addr = base + ext;
            }
            else
                return EMU_INSN_SKIP;
            if (writeback && rn != 31 && rn == rt)
                return EMU_INSN_SKIP;
        }

        is_load = is_fp ? ((insn >> 22) & 1) : (opc != 0);
        if (is_load)
        {
            __uint128_t val;

            if (emu_read_mem(addr, bytes, &val))
                return EMU_INSN_FAULT;
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
                    raw = emu_sign_extend_hw(raw, bytes);
                }
                reg_write(regs, rt, raw, (size == 3 || opc == 2));
            }
        }
        else
        {
            __uint128_t val = is_fp ? fp_regs[rt] : (__uint128_t)reg_read(regs, rt);

            if (emu_write_mem(addr, bytes, val))
                return EMU_INSN_FAULT;
        }
        if (writeback)
            addr_reg_write(regs, rn, wb_addr);
    }

done_ldst:
    if (is_fp && fp_dirty)
    {
        int i;

        for (i = 0; i < 32; i++)
            write_q_reg(i, &fp_regs[i]);
        write_fpsr(fpsr);
        write_fpcr(fpcr);
    }
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_add_sub_imm(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    bool op_sub = (insn >> 30) & 1;
    bool setflags = (insn >> 29) & 1;
    uint32_t sh = (insn >> 22) & 1;
    uint64_t imm = (insn >> 10) & 0xFFF;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t a, result, nzcv = 0;

    if (sh)
        imm <<= 12;

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

static __always_inline enum emu_insn_result emu_simulate_add_sub_shifted(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    bool op_sub = (insn >> 30) & 1;
    bool setflags = (insn >> 29) & 1;
    uint32_t shift_type = (insn >> 22) & 0x3;
    uint32_t rm = (insn >> 16) & 0x1F, imm6 = (insn >> 10) & 0x3F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t a, b, result, nzcv = 0;

    if (shift_type == 3)
        return EMU_INSN_SKIP;
    if (!sf && (imm6 & 0x20))
        return EMU_INSN_SKIP;

    a = reg_read(regs, rn);
    b = emu_shift_reg(reg_read(regs, rm), shift_type, imm6, sf);
    result = emu_addsub_hw(a, b, op_sub, setflags, sf, &nzcv);

    if (setflags)
        emu_write_nzcv(regs, nzcv);
    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_add_sub_extended(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    bool op_sub = (insn >> 30) & 1;
    bool setflags = (insn >> 29) & 1;
    uint32_t rm = (insn >> 16) & 0x1F, option = (insn >> 13) & 0x7, imm3 = (insn >> 10) & 0x7;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t a, b, result, nzcv = 0;

    if (imm3 > 4)
        return EMU_INSN_SKIP;

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

static __always_inline enum emu_insn_result emu_simulate_logic_shifted(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    uint32_t opc = (insn >> 29) & 0x3;
    uint32_t shift_type = (insn >> 22) & 0x3;
    bool invert = (insn >> 21) & 1;
    uint32_t rm = (insn >> 16) & 0x1F, imm6 = (insn >> 10) & 0x3F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t a, b, result, nzcv = 0;

    if (!sf && (imm6 & 0x20))
        return EMU_INSN_SKIP;

    a = reg_read(regs, rn);
    b = emu_shift_reg(reg_read(regs, rm), shift_type, imm6, sf);
    result = emu_logic_hw(a, b, opc, invert, sf, &nzcv);

    if (opc == 3)
        emu_write_nzcv(regs, nzcv);
    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_move_wide(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    uint32_t opc = (insn >> 29) & 0x3;
    uint32_t hw = (insn >> 21) & 0x3, shift = hw * 16;
    uint64_t imm16 = (insn >> 5) & 0xFFFF;
    uint32_t rd = insn & 0x1F;
    uint64_t result;

    if (opc == 1)
        return EMU_INSN_SKIP;
    if (!sf && (hw & 0x2))
        return EMU_INSN_SKIP;

    if (opc == 0)
        result = ~(imm16 << shift);
    else if (opc == 2)
        result = (imm16 << shift);
    else
        result = (reg_read(regs, rd) & ~(0xFFFFULL << shift)) | (imm16 << shift);

    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_logic_imm(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    uint32_t opc = (insn >> 29) & 0x3;
    uint32_t n = (insn >> 22) & 1;
    uint32_t immr = (insn >> 16) & 0x3F;
    uint32_t imms = (insn >> 10) & 0x3F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t imm, a, result, nzcv = 0;

    if (!emu_decode_bitmask_imm(n, immr, imms, sf, &imm))
        return EMU_INSN_SKIP;

    a = reg_read(regs, rn) & emu_mask_for_width(sf);
    result = emu_logic_hw(a, imm, opc, false, sf, &nzcv);

    if (opc == 3)
        emu_write_nzcv(regs, nzcv);

    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_bitfield(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    uint32_t opc = (insn >> 29) & 0x3;
    uint32_t n = (insn >> 22) & 1;
    uint32_t immr = (insn >> 16) & 0x3F;
    uint32_t imms = (insn >> 10) & 0x3F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint32_t width = sf ? 64 : 32;
    uint64_t src, dst, bot, result, wmask, tmask;

    if (opc == 3)
        return EMU_INSN_SKIP;
    if (sf != !!n)
        return EMU_INSN_SKIP;
    if (!sf && ((immr | imms) & 0x20))
        return EMU_INSN_SKIP;
    if (!emu_decode_bitfield_masks(n, immr, imms, sf, &wmask, &tmask))
        return EMU_INSN_SKIP;

    src = reg_read(regs, rn) & emu_mask_for_width(sf);
    dst = reg_read(regs, rd) & emu_mask_for_width(sf);
    bot = emu_extract_hw(src, src, immr, sf) & wmask;

    switch (opc)
    {
    case 0:
    {
        uint64_t sign_bit = (tmask == emu_mask_for_width(sf)) ? (1ULL << (width - 1)) : ((tmask + 1) >> 1);

        result = bot & tmask;
        if (bot & sign_bit)
            result |= ~tmask;
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

static __always_inline enum emu_insn_result emu_simulate_extract(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    uint32_t n = (insn >> 22) & 1;
    uint32_t rm = (insn >> 16) & 0x1F;
    uint32_t imms = (insn >> 10) & 0x3F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint32_t width = sf ? 64 : 32;
    uint64_t result;

    if (sf != !!n)
        return EMU_INSN_SKIP;
    if (imms >= width)
        return EMU_INSN_SKIP;

    result = emu_extract_hw(reg_read(regs, rn), reg_read(regs, rm), imms, sf);

    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_cond_select(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    bool op = (insn >> 30) & 1;
    bool op2 = (insn >> 10) & 1;
    uint32_t fixed = (insn >> 11) & 1;
    uint32_t rm = (insn >> 16) & 0x1F;
    uint32_t cond = (insn >> 12) & 0xF;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t result;

    if (fixed)
        return EMU_INSN_SKIP;

    result = emu_cond_select_hw(reg_read(regs, rn), reg_read(regs, rm), regs->pstate, cond, op, op2, sf);

    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_data2(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    uint32_t opcode = (insn >> 10) & 0x3F;
    uint32_t rm = (insn >> 16) & 0x1F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t a = reg_read(regs, rn) & emu_mask_for_width(sf);
    uint64_t b = reg_read(regs, rm) & emu_mask_for_width(sf);
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
    case 0x10: // CRC32B/CRC32H/CRC32W/CRC32X
    case 0x11:
    case 0x12:
    case 0x13:
    case 0x14: // CRC32CB/CRC32CH/CRC32CW/CRC32CX
    case 0x15:
    case 0x16:
    case 0x17:
    {
        // CRC 累加器与结果恒为 32 位 (Wn/Wd)，故写回宽度固定 false。
        reg_write(regs, rd, emu_crc32_hw((uint32_t)a, b, opcode), false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    case 0x18: // SMAX (FEAT_CSSC)
    case 0x19: // UMAX
    case 0x1A: // SMIN
    case 0x1B: // UMIN
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

static __always_inline enum emu_insn_result emu_simulate_data3(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    bool op = (insn >> 15) & 1;
    uint32_t rm = (insn >> 16) & 0x1F;
    uint32_t ra = (insn >> 10) & 0x1F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t multiplicand = reg_read(regs, rn) & emu_mask_for_width(sf);
    uint64_t multiplier = reg_read(regs, rm) & emu_mask_for_width(sf);
    uint64_t addend = reg_read(regs, ra) & emu_mask_for_width(sf);
    uint64_t result;

    if (sf)
    {
        if (op)
            asm volatile("msub %0, %1, %2, %3\n" : "=r"(result) : "r"(multiplicand), "r"(multiplier), "r"(addend));
        else
            asm volatile("madd %0, %1, %2, %3\n" : "=r"(result) : "r"(multiplicand), "r"(multiplier), "r"(addend));
    }
    else
    {
        uint32_t result32;

        if (op)
            asm volatile("msub %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)multiplicand), "r"((uint32_t)multiplier), "r"((uint32_t)addend));
        else
            asm volatile("madd %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)multiplicand), "r"((uint32_t)multiplier), "r"((uint32_t)addend));
        result = result32;
    }

    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

// ADC/ADCS/SBC/SBCS (及别名 NGC/NGCS)：带进位加减。
// SBC 语义 = Rn + ~Rm + C，读取 PSTATE.C；setflags 时按 AddWithCarry 结果刷新 NZCV。
static __always_inline enum emu_insn_result emu_simulate_add_sub_carry(struct pt_regs *regs, uint32_t insn)
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

    if (setflags)
        emu_write_nzcv(regs, nzcv);

    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

// CCMP/CCMN (立即数与寄存器两种形态)：条件成立则按比较/相加刷新 NZCV，
// 否则把指令内嵌的 4 位 nzcv 直接写入 PSTATE。
static __always_inline enum emu_insn_result emu_simulate_cond_compare(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    bool op_sub = (insn >> 30) & 1; // 1=CCMP(减)，0=CCMN(加)
    bool is_imm = (insn >> 11) & 1;
    uint32_t cond = (insn >> 12) & 0xF;
    uint32_t rn = (insn >> 5) & 0x1F;
    uint32_t nzcv = insn & 0xF;
    uint32_t rm_or_imm = (insn >> 16) & 0x1F;
    uint64_t a = reg_read(regs, rn);
    uint64_t b = is_imm ? (uint64_t)rm_or_imm : reg_read(regs, rm_or_imm);
    uint64_t flags;

    flags = emu_cond_compare_hw(a, b, regs->pstate, nzcv, cond, op_sub, sf);
    emu_write_nzcv(regs, flags);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

// 单源数据处理：RBIT/REV16/REV32/REV(REV64)/CLZ/CLS，以及 FEAT_CSSC 的 CTZ/CNT/ABS。
// opcode2!=0 (含 PACIA 等指针认证) 一律跳过。
static __always_inline enum emu_insn_result emu_simulate_data1(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    uint32_t opcode2 = (insn >> 16) & 0x1F;
    uint32_t opcode = (insn >> 10) & 0x3F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t src = reg_read(regs, rn);
    uint64_t result;

    if (opcode2 != 0)
        return EMU_INSN_SKIP;

    switch (opcode)
    {
    case 0: // RBIT
        result = sf ? EMU_INT_UN64("rbit", src) : EMU_INT_UN32("rbit", src);
        break;
    case 1: // REV16
        result = sf ? EMU_INT_UN64("rev16", src) : EMU_INT_UN32("rev16", src);
        break;
    case 2: // 32 位=REV(整字节反转)；64 位=REV32(每 32 位字内反转)
        result = sf ? EMU_INT_UN64("rev32", src) : EMU_INT_UN32("rev", src);
        break;
    case 3: // REV(REV64)，仅 64 位合法
        if (!sf)
            return EMU_INSN_SKIP;
        result = EMU_INT_UN64("rev", src);
        break;
    case 4: // CLZ
        result = sf ? EMU_INT_UN64("clz", src) : EMU_INT_UN32("clz", src);
        break;
    case 5: // CLS
        result = sf ? EMU_INT_UN64("cls", src) : EMU_INT_UN32("cls", src);
        break;
    case 6: // CTZ (FEAT_CSSC)
        result = emu_ctz_hw(src, sf);
        break;
    case 7: // CNT (FEAT_CSSC)
        result = emu_cnt_hw(src, sf);
        break;
    case 8: // ABS (FEAT_CSSC)
        result = emu_abs_hw(src, sf);
        break;
    default:
        return EMU_INSN_SKIP;
    }

    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

// 长乘 / 高位乘：SMADDL/UMADDL/SMSUBL/UMSUBL (32x32+64)、SMULH/UMULH (64x64 取高 64 位)，
// 以及同宽 MSUB。MADD 由 emu_simulate_data3 处理，不会进入这里。
static __always_inline enum emu_insn_result emu_simulate_data3_long(struct pt_regs *regs, uint32_t insn)
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
    case 0: // MADD/MSUB 同宽 (此处仅 MSUB 到达)
    {
        uint64_t n = reg_read(regs, rn) & emu_mask_for_width(sf);
        uint64_t m = reg_read(regs, rm) & emu_mask_for_width(sf);
        uint64_t a = reg_read(regs, ra) & emu_mask_for_width(sf);

        if (sf)
        {
            if (o0)
                asm volatile("msub %0, %1, %2, %3\n" : "=r"(result) : "r"(n), "r"(m), "r"(a));
            else
                asm volatile("madd %0, %1, %2, %3\n" : "=r"(result) : "r"(n), "r"(m), "r"(a));
        }
        else
        {
            uint32_t result32;

            if (o0)
                asm volatile("msub %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)n), "r"((uint32_t)m), "r"((uint32_t)a));
            else
                asm volatile("madd %w0, %w1, %w2, %w3\n" : "=r"(result32) : "r"((uint32_t)n), "r"((uint32_t)m), "r"((uint32_t)a));
            result = result32;
        }
        break;
    }
    case 1: // SMADDL/SMSUBL (仅 64 位形态合法)
    {
        uint64_t a;

        if (!sf)
            return EMU_INSN_SKIP;
        a = reg_read(regs, ra);
        if (o0)
            asm volatile("smsubl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, rn)), "r"((uint32_t)reg_read(regs, rm)), "r"(a));
        else
            asm volatile("smaddl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, rn)), "r"((uint32_t)reg_read(regs, rm)), "r"(a));
        break;
    }
    case 2: // SMULH
        if (!sf || o0)
            return EMU_INSN_SKIP;
        asm volatile("smulh %0, %1, %2\n" : "=r"(result) : "r"(reg_read(regs, rn)), "r"(reg_read(regs, rm)));
        break;
    case 5: // UMADDL/UMSUBL
    {
        uint64_t a;

        if (!sf)
            return EMU_INSN_SKIP;
        a = reg_read(regs, ra);
        if (o0)
            asm volatile("umsubl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, rn)), "r"((uint32_t)reg_read(regs, rm)), "r"(a));
        else
            asm volatile("umaddl %0, %w1, %w2, %3\n" : "=r"(result) : "r"((uint32_t)reg_read(regs, rn)), "r"((uint32_t)reg_read(regs, rm)), "r"(a));
        break;
    }
    case 6: // UMULH
        if (!sf || o0)
            return EMU_INSN_SKIP;
        asm volatile("umulh %0, %1, %2\n" : "=r"(result) : "r"(reg_read(regs, rn)), "r"(reg_read(regs, rm)));
        break;
    default:
        return EMU_INSN_SKIP;
    }

    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

// SMAX/SMIN/UMAX/UMIN 立即数形态 (FEAT_CSSC)：
// opc 在 bits[19:18] (bit19=min/max，bit18=有/无符号)，imm8 在 bits[17:10]。
static __always_inline enum emu_insn_result emu_simulate_minmax_imm(struct pt_regs *regs, uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    bool is_min = (insn >> 19) & 1;
    bool is_unsigned = (insn >> 18) & 1;
    uint32_t imm8 = (insn >> 10) & 0xFF;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint64_t a = reg_read(regs, rn) & emu_mask_for_width(sf);
    uint64_t b, result;

    if (is_unsigned)
        b = imm8;
    else
        b = sign_extend64(imm8, 7) & emu_mask_for_width(sf); // 符号扩展 -128..127

    result = emu_minmax_hw(a, b, is_min, is_unsigned, sf);

    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_fp_scalar_bin(__uint128_t fp_regs[32], uint32_t insn)
{
    uint32_t type = (insn >> 22) & 0x3;
    uint32_t opcode = (insn >> 12) & 0xF;
    uint32_t rm = (insn >> 16) & 0x1F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

    if (type > 1)
        return EMU_INSN_SKIP;

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

    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_fp_scalar_un(__uint128_t fp_regs[32], uint32_t insn)
{
    uint32_t type = (insn >> 22) & 0x3;
    uint32_t opcode = (insn >> 15) & 0x3F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

    if (type > 1)
        return EMU_INSN_SKIP;

    if (type == 0)
    {
        switch (opcode)
        {
        case 0:
            emu_fp_set_low32(&fp_regs[rd], emu_fp_low32(fp_regs[rn]));
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
            emu_fp_set_low64(&fp_regs[rd], emu_fp_low64(fp_regs[rn]));
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

    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_fp_scalar_tern(__uint128_t fp_regs[32], uint32_t insn)
{
    uint32_t type = (insn >> 22) & 0x3;
    bool neg = (insn >> 21) & 1;
    bool sub = (insn >> 15) & 1;
    uint32_t rm = (insn >> 16) & 0x1F;
    uint32_t ra = (insn >> 10) & 0x1F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

    if (type > 1)
        return EMU_INSN_SKIP;

    if (type == 0)
    {
        if (!neg && !sub)
            EMU_FP_TERN("fmadd s0, s1, s2, s3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
        else if (!neg && sub)
            EMU_FP_TERN("fmsub s0, s1, s2, s3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
        else if (neg && !sub)
            EMU_FP_TERN("fnmadd s0, s1, s2, s3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
        else
            EMU_FP_TERN("fnmsub s0, s1, s2, s3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
    }
    else
    {
        if (!neg && !sub)
            EMU_FP_TERN("fmadd d0, d1, d2, d3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
        else if (!neg && sub)
            EMU_FP_TERN("fmsub d0, d1, d2, d3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
        else if (neg && !sub)
            EMU_FP_TERN("fnmadd d0, d1, d2, d3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
        else
            EMU_FP_TERN("fnmsub d0, d1, d2, d3", &fp_regs[rd], &fp_regs[rn], &fp_regs[rm], &fp_regs[ra]);
    }

    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_fp_compare(struct pt_regs *regs,
                                                                    __uint128_t fp_regs[32],
                                                                    uint32_t insn)
{
    uint32_t type = (insn >> 22) & 0x3;
    bool zero = (insn >> 3) & 1;
    bool signal = (insn >> 4) & 1;
    uint32_t rm = (insn >> 16) & 0x1F;
    uint32_t rn = (insn >> 5) & 0x1F;
    uint64_t nzcv;

    if (type > 1)
        return EMU_INSN_SKIP;

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
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_fp_fcsel(struct pt_regs *regs,
                                                                  __uint128_t fp_regs[32],
                                                                  uint32_t insn)
{
    uint32_t type = (insn >> 22) & 0x3;
    uint32_t rm = (insn >> 16) & 0x1F;
    uint32_t cond = (insn >> 12) & 0xF;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

    if (!emu_fp_fcsel_hw(&fp_regs[rd], &fp_regs[rn], &fp_regs[rm], regs->pstate, cond, type))
        return EMU_INSN_SKIP;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_fp_mov_gp(struct pt_regs *regs,
                                                                   __uint128_t fp_regs[32],
                                                                   uint32_t insn)
{
    bool sf = (insn >> 31) & 1;
    bool gp_to_fp = (insn >> 16) & 1;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

    if (gp_to_fp)
    {
        if (sf)
            emu_fp_set_low64(&fp_regs[rd], reg_read(regs, rn));
        else
            emu_fp_set_low32(&fp_regs[rd], (uint32_t)reg_read(regs, rn));
    }
    else
    {
        if (sf)
            reg_write(regs, rd, emu_fp_low64(fp_regs[rn]), true);
        else
            reg_write(regs, rd, emu_fp_low32(fp_regs[rn]), false);
    }

    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_fp_convert(struct pt_regs *regs,
                                                                    __uint128_t fp_regs[32],
                                                                    uint32_t insn)
{
    uint32_t sig = insn & 0xFFFFFC00;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
    uint32_t wout;
    uint64_t xout;

    switch (sig)
    {
    case 0x1E220000: // SCVTF Sd, Wn
        asm volatile(".arch_extension fp\n"
                     "scvtf s0, %w1\n"
                     "str q0, [%0]\n"
                     : : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn)) : "memory");
        break;
    case 0x9E220000: // SCVTF Sd, Xn
        asm volatile(".arch_extension fp\n"
                     "scvtf s0, %1\n"
                     "str q0, [%0]\n"
                     : : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn)) : "memory");
        break;
    case 0x1E620000: // SCVTF Dd, Wn
        asm volatile(".arch_extension fp\n"
                     "scvtf d0, %w1\n"
                     "str q0, [%0]\n"
                     : : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn)) : "memory");
        break;
    case 0x9E620000: // SCVTF Dd, Xn
        asm volatile(".arch_extension fp\n"
                     "scvtf d0, %1\n"
                     "str q0, [%0]\n"
                     : : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn)) : "memory");
        break;
    case 0x1E230000: // UCVTF Sd, Wn
        asm volatile(".arch_extension fp\n"
                     "ucvtf s0, %w1\n"
                     "str q0, [%0]\n"
                     : : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn)) : "memory");
        break;
    case 0x9E230000: // UCVTF Sd, Xn
        asm volatile(".arch_extension fp\n"
                     "ucvtf s0, %1\n"
                     "str q0, [%0]\n"
                     : : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn)) : "memory");
        break;
    case 0x1E630000: // UCVTF Dd, Wn
        asm volatile(".arch_extension fp\n"
                     "ucvtf d0, %w1\n"
                     "str q0, [%0]\n"
                     : : "r"(&fp_regs[rd]), "r"((uint32_t)reg_read(regs, rn)) : "memory");
        break;
    case 0x9E630000: // UCVTF Dd, Xn
        asm volatile(".arch_extension fp\n"
                     "ucvtf d0, %1\n"
                     "str q0, [%0]\n"
                     : : "r"(&fp_regs[rd]), "r"(reg_read(regs, rn)) : "memory");
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
    default:
        return EMU_INSN_SKIP;
    }

    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_advsimd_vector(__uint128_t fp_regs[32], uint32_t insn)
{
    uint32_t sig = insn & 0xFFE0FC00;
    uint32_t rm = (insn >> 16) & 0x1F;
    uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;

    if ((insn & 0xBFE08400) == 0x2E000000) // EXT Vd,Vn,Vm,#imm
    {
        uint32_t imm = (insn >> 11) & 0xF;

        return emu_advsimd_ext_hw(&fp_regs[rd], &fp_regs[rn], &fp_regs[rm],
                                  imm, !!(insn & (1U << 30)));
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

    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_fp_simd_insn(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    __uint128_t fp_regs[32];
    uint32_t fpsr, fpcr;
    enum emu_insn_result result = EMU_INSN_SKIP;
    int i;

    for (i = 0; i < 32; i++)
        read_q_reg(i, &fp_regs[i]);
    fpsr = read_fpsr();
    fpcr = read_fpcr();
    write_fpsr(fpsr);
    write_fpcr(fpcr);

    if ((insn & 0xFF200C00) == 0x1E200800)
        result = emu_simulate_fp_scalar_bin(fp_regs, insn);
    else if ((insn & 0xFF207C00) == 0x1E204000)
        result = emu_simulate_fp_scalar_un(fp_regs, insn);
    else if ((insn & 0xFF000000) == 0x1F000000)
        result = emu_simulate_fp_scalar_tern(fp_regs, insn);
    else if ((insn & 0xFF20FC00) == 0x1E202000)
        result = emu_simulate_fp_compare(regs, fp_regs, insn);
    else if ((insn & 0xFF200C00) == 0x1E200C00)
        result = emu_simulate_fp_fcsel(regs, fp_regs, insn);
    else if ((insn & 0x7F3F7C00) == 0x1E260000)
        result = emu_simulate_fp_mov_gp(regs, fp_regs, insn);
    else
    {
        result = emu_simulate_fp_convert(regs, fp_regs, insn);
        if (result == EMU_INSN_SKIP)
            result = emu_simulate_advsimd_vector(fp_regs, insn);
    }

    if (result != EMU_INSN_HANDLED)
        return result;

    fpsr = read_fpsr();
    for (i = 0; i < 32; i++)
        write_q_reg(i, &fp_regs[i]);
    write_fpsr(fpsr);
    write_fpcr(fpcr);
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_data_imm_insn(struct pt_regs *regs, uint32_t insn)
{
    if ((insn & 0x1F800000) == 0x11000000)
        return emu_simulate_add_sub_imm(regs, insn);
    if ((insn & 0x7FF00000) == 0x11C00000)
        return emu_simulate_minmax_imm(regs, insn);
    if ((insn & 0x1F800000) == 0x12000000)
        return emu_simulate_logic_imm(regs, insn);
    if ((insn & 0x7F800000) == 0x13000000 ||
        (insn & 0x7F800000) == 0x33000000 ||
        (insn & 0x7F800000) == 0x53000000)
        return emu_simulate_bitfield(regs, insn);
    if ((insn & 0x7FA00000) == 0x13800000)
        return emu_simulate_extract(regs, insn);
    if ((insn & 0x1F800000) == 0x12800000)
        return emu_simulate_move_wide(regs, insn);

    return EMU_INSN_SKIP;
}

static __always_inline enum emu_insn_result emu_simulate_data_reg_insn(struct pt_regs *regs, uint32_t insn)
{
    if ((insn & 0x1F200000) == 0x0B000000)
        return emu_simulate_add_sub_shifted(regs, insn);
    if ((insn & 0x1FE00000) == 0x0B200000)
        return emu_simulate_add_sub_extended(regs, insn);
    if ((insn & 0x1F000000) == 0x0A000000)
        return emu_simulate_logic_shifted(regs, insn);
    if ((insn & 0x3FE00000) == 0x1A800000)
        return emu_simulate_cond_select(regs, insn);
    if ((insn & 0x7FE00000) == 0x1AC00000)
        return emu_simulate_data2(regs, insn);
    if ((insn & 0x7FE08000) == 0x1B000000)
        return emu_simulate_data3(regs, insn);
    if ((insn & 0x7F000000) == 0x1B000000)
        return emu_simulate_data3_long(regs, insn);
    if ((insn & 0x1FE0FC00) == 0x1A000000)
        return emu_simulate_add_sub_carry(regs, insn);
    if ((insn & 0x3FE00410) == 0x3A400000)
        return emu_simulate_cond_compare(regs, insn);
    if ((insn & 0x7FE00000) == 0x5AC00000)
        return emu_simulate_data1(regs, insn);

    return EMU_INSN_SKIP;
}

static __always_inline enum emu_insn_result emu_simulate_data_processing_insn(struct pt_regs *regs, uint32_t insn)
{
    enum emu_insn_result result;

    result = emu_simulate_data_imm_insn(regs, insn);
    if (result != EMU_INSN_SKIP)
        return result;

    return emu_simulate_data_reg_insn(regs, insn);
}

static __always_inline enum emu_insn_result emu_simulate_int_mem_insn(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    if ((insn & 0xFFC00000) == 0xD5000000)
        return emu_simulate_system_insn(regs, insn, pc);

    if ((insn & 0x1F000000) == 0x10000000)
    {
        emu_simulate_adr_adrp(regs, insn, pc);
        return EMU_INSN_HANDLED;
    }

    if (emu_is_lse_atomic(insn))
        return emu_simulate_lse_atomic(regs, insn, pc);

    if (((insn & 0x3A000000) == 0x28000000) ||
        ((insn & 0x3A000000) == 0x38000000) ||
        ((insn & 0x3B000000) == 0x18000000))
        return emu_simulate_load_store_insn(regs, insn, pc);

    return emu_simulate_data_processing_insn(regs, insn);
}

// 取指后只做大类分发；具体语义由各类 emu_simulate_* handler 完成。
static __always_inline enum emu_insn_result emulate_insn(struct pt_regs *regs)
{
    uint32_t insn;
    uint64_t pc = regs->pc;
    uint32_t iclass;
    enum emu_insn_result result = EMU_INSN_SKIP;

    if (__get_user(insn, (uint32_t __user *)pc))
        return EMU_INSN_FAULT;

    iclass = (insn >> 25) & 0xF;

    if ((iclass & 0xE) == 0xA)
    {
        result = emu_simulate_branch_insn(regs, insn, pc);
    }
    else if (iclass == 0x7 || iclass == 0xF)
    {
        result = emu_simulate_fp_simd_insn(regs, insn, pc);
    }
    else
    {
        result = emu_simulate_int_mem_insn(regs, insn, pc);
    }

    if (result == EMU_INSN_NOP)
        regs->pc = pc + 4;

    return result;
}

#endif // EMULATE_INSN_H
