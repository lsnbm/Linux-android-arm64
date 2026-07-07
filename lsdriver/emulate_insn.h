#ifndef EMULATE_INSN_H
#define EMULATE_INSN_H

#include <linux/uaccess.h>
#include <linux/errno.h>
#include <asm/ptrace.h>
#include <asm/insn.h>
#include "arm64_reg.h"

/* =========================================================================
  ARM64 单步指令模拟器 (emulate_insn)

  【用途】
  硬件执行/访问断点、PTE 断点命中后，在软件层面把"命中的这一条指令"直接
  算出来：分支类更新 PC，访存类完成内存读写并 PC+=4。以此跳过该指令，
  不必依赖硬件单步 (MDSCR_EL1.SS)，避免反复进出调试异常。

    【返回值约定】emulate_insn() 返回 bool：
        true  : 指令已被识别并完整模拟。PC 已按语义更新
                        (分支落到目标地址；访存 / ADR / 被当作 nop 的字面量 PRFM 为 PC+=4)。
        false : 指令未被完整模拟，分两种情况，靠 PC 是否变化区分：
                        - handler 返回 EMU_INSN_SKIP：不支持或无需副作用的指令，外层统一 PC+=4。
                        - 取指失败或 handler 返回 EMU_INSN_FAULT：__get_user / __put_user 出错，
                            PC 保持不变，交由硬件缺页/重放机制处理。

    【分发结构】：emulate_insn() 只负责取指、按 ARM64 顶层编码组分发、收敛 EMU_INSN_* 结果；
        具体语义拆给 emu_simulate_* 小函数

        - emu_simulate_branch_insn()
                B, BL, BR/BLR/RET, B.cond, CBZ/CBNZ, TBZ/TBNZ。
                其余成员 (SVC/HVC/SMC/BRK、MSR/MRS/系统、HINT 等) 返回 EMU_INSN_SKIP。
        - emu_simulate_adr_adrp()
                ADR, ADRP。
        - emu_simulate_load_store_insn()
                成对：LDP/STP/LDNP/STNP (含 LDPSW)
                单个：LDR/STR —— unsigned-imm、unscaled(LDUR/STUR)、pre/post-index、
                            register-offset(含 UXTW/SXTW/LSL/SXTX)
                字面：LDR(literal)、LDRSW(literal)、PRFM(literal 按 nop)
                整数 (X/W) 与浮点/SIMD (B/H/S/D/Q) 在此统一处理。
        - emu_simulate_data_processing_insn()
                ADD/SUB 立即数、ADD/SUB 移位寄存器、ADD/SUB 扩展寄存器、
            逻辑立即数/移位寄存器、位域/EXTR、条件选择、二/三源整数运算、MOVN/MOVZ/MOVK。

  【已支持指令】(全寄存器 + 全位宽)
    - 分支跳转：B, BL, BR, BLR, RET, B.cond, CBZ, CBNZ, TBZ, TBNZ (含条件码求值)
    - PC 相对：ADR, ADRP
    - 整数访存 (8/16/32/64 位)：
        LDR/STR、LDUR/STUR、LDP/STP/LDNP/STNP、
        LDRB/LDRH/LDRSB/LDRSH/LDRSW (零扩展 / 符号扩展)、LDR·LDRSW(literal)
    - 浮点/SIMD 访存 (8/16/32/64/128 位)：
        LDR/STR、LDP/STP、LDR(literal)
        * 直接读写物理 CPU 的 Q0-Q31 + FPSR/FPCR，支持 128-bit(Q) 存取；
          FP 通路进入时读入全部 Q 寄存器，仅 Load 命中后才整体回写。
    - 数据处理 (整数, 32/64 位)：
        ADD/SUB (立即数/移位寄存器/扩展寄存器)、ADDS/SUBS/CMP/CMN (更新 NZCV)、
        AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS (立即数/移位寄存器)、MOV/MVN (别名)、MOVN/MOVZ/MOVK、
        SBFM/UBFM/BFM (含 SXT/UXT/LSL/LSR/ASR/BFI/BFXIL 等别名)、EXTR/ROR(别名)、
        CSEL/CSINC/CSINV/CSNEG、UDIV/SDIV、LSLV/LSRV/ASRV/RORV、MADD/MSUB/MUL/MNEG(别名)
        * 立即数/扩展寄存器形态正确区分 SP 与 XZR 语义；移位含 LSL/LSR/ASR/ROR。

  【暂不支持】(遇到即跳过：仅 PC+=4，不产生其它副作用)
    - 数据处理(未覆盖)：CCMP/CCMN、REV/REV16/REV32、CLZ/CLS、RBIT、ADC/SBC、CRC 等
    - 独占/有序访存：LDXR/STXR/LDAXR/STLXR/LDAR/STLR (编码段不同，天然不进入本模块)
    - LSE 原子：SWP/CAS/CASP/LDADD/LDSET/LDCLR/LDEOR/LDSMAX/LDSMIN/... (识别后跳过)
    - 指针认证加载：LDRAA/LDRAB (识别后跳过)
    - 预取：PRFM (立即数/寄存器/字面量，按 nop 处理，只推进 PC，不实际预取)
    - 异常/系统：SVC/HVC/SMC/BRK、MSR/MRS 及系统指令
    - 向量结构化访存：LD1/ST1/LD1R 等 Advanced SIMD 多元素访存

  【后续扩展指令的方法】
        1) 判断新指令属于哪个顶层编码组，在对应 emu_simulate_* 分发函数里新增解码分支；
    2) 命中后按语义写回：PC / 通用寄存器用 reg_write(XZR 语义)、基址用 addr_reg_*(SP 语义)；
        3) 访问用户内存一律用 __get_user / __put_user，失败返回 EMU_INSN_FAULT；
    4) 需要浮点寄存器时用 read_q_reg / write_q_reg，Load 修改后置 fp_dirty 触发回写；
        5) 能完整模拟的返回 EMU_INSN_HANDLED，无法处理但可安全跳过的返回 EMU_INSN_SKIP。

  【辅助函数】(均定义在 emulate_insn 之前，遵循"定义在前、使用在后")
    reg_read / reg_write     : X/W 通用寄存器，n==31 视为 XZR (读 0 / 写丢弃)
    addr_reg_read / _write   : 基址寄存器，n==31 视为 SP
    eval_cond_fast           : 依据 PSTATE.NZCV 求条件码 (供 B.cond)
    emu_read_mem             : 按 1/2/4/8/16 字节从用户内存读入 128 位缓冲(高位零扩展)
    emu_write_mem            : 按 1/2/4/8/16 字节把值的低位写回用户内存
                                                             二者失败返回 -EFAULT，是访存 handler 共用的读写核心。
    emu_set_nzcv_addsub      : 依加/减结果刷新 PSTATE.NZCV (供 ADDS/SUBS/CMP/CMN)
    emu_shift_reg            : 寄存器移位 LSL/LSR/ASR/ROR (供移位寄存器类)
    emu_extend_reg           : 寄存器扩展 U/SXTB..X + 左移 (供 ADD/SUB 扩展寄存器)
        emu_*_displacement       : 分支和 literal 指令的带符号位移提取
        emu_simulate_*           : 每类指令的小模拟器，返回 EMU_INSN_HANDLED/SKIP/FAULT
  ========================================================================= */

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

static __always_inline bool eval_cond_fast(uint64_t pstate, uint32_t cond)
{
    bool n = (pstate >> 31) & 1, z = (pstate >> 30) & 1;
    bool c = (pstate >> 29) & 1, v = (pstate >> 28) & 1, res;
    switch (cond >> 1)
    {
    case 0:
        res = z;
        break;
    case 1:
        res = c;
        break;
    case 2:
        res = n;
        break;
    case 3:
        res = v;
        break;
    case 4:
        res = c && !z;
        break;
    case 5:
        res = (n == v);
        break;
    case 6:
        res = (n == v) && !z;
        break;
    default:
        res = true;
        break;
    }
    return ((cond & 1) && (cond != 0xf)) ? !res : res;
}

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

/* ---- 数据处理指令通用逻辑：供 emu_simulate_data_processing_insn() 各分支复用 ---- */

// 依据 a (加/减) b 的结果刷新 PSTATE.NZCV，供 ADDS/SUBS/CMP/CMN。
// op_sub=false 为加法、true 为减法；sf=true 为 64 位、false 为 32 位。
static __always_inline void emu_set_nzcv_addsub(struct pt_regs *regs, uint64_t a, uint64_t b, bool op_sub, bool sf)
{
    bool n, z, c, v;
    uint64_t pstate;

    if (sf)
    {
        uint64_t res = op_sub ? (a - b) : (a + b);
        if (op_sub)
        {
            c = (a >= b);                          // 无借位 => C=1
            v = (((a ^ b) & (a ^ res)) >> 63) & 1; // 操作数异号且结果与被减数异号 => 溢出
        }
        else
        {
            c = (res < a);                          // 和回绕 => 进位
            v = ((~(a ^ b) & (a ^ res)) >> 63) & 1; // 操作数同号但结果异号 => 溢出
        }
        n = (res >> 63) & 1;
        z = (res == 0);
    }
    else
    {
        uint32_t a32 = (uint32_t)a, b32 = (uint32_t)b;
        uint32_t res = op_sub ? (a32 - b32) : (a32 + b32);
        if (op_sub)
        {
            c = (a32 >= b32);
            v = (((a32 ^ b32) & (a32 ^ res)) >> 31) & 1;
        }
        else
        {
            c = (res < a32);
            v = ((~(a32 ^ b32) & (a32 ^ res)) >> 31) & 1;
        }
        n = (res >> 31) & 1;
        z = (res == 0);
    }

    pstate = regs->pstate & ~((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28));
    if (n)
        pstate |= (1ULL << 31);
    if (z)
        pstate |= (1ULL << 30);
    if (c)
        pstate |= (1ULL << 29);
    if (v)
        pstate |= (1ULL << 28);
    regs->pstate = pstate;
}

// 对寄存器值做移位：type 0=LSL 1=LSR 2=ASR 3=ROR；sf 决定 32/64 位。
// 移位量已由调用方保证 < 位宽（32 位时拒绝 imm6>=32）。
static __always_inline uint64_t emu_shift_reg(uint64_t val, uint32_t type, uint32_t amount, bool sf)
{
    if (sf)
    {
        switch (type)
        {
        case 0:
            return val << amount;
        case 1:
            return val >> amount;
        case 2:
            return (uint64_t)((int64_t)val >> amount);
        default:
            return amount ? ((val >> amount) | (val << (64 - amount))) : val;
        }
    }
    else
    {
        uint32_t v = (uint32_t)val;
        switch (type)
        {
        case 0:
            return (uint32_t)(v << amount);
        case 1:
            return v >> amount;
        case 2:
            return (uint32_t)((int32_t)v >> amount);
        default:
            return amount ? ((v >> amount) | (v << (32 - amount))) : v;
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
        x = (uint8_t)val;
        break; // UXTB
    case 1:
        x = (uint16_t)val;
        break; // UXTH
    case 2:
        x = (uint32_t)val;
        break; // UXTW
    case 4:
        x = (uint64_t)(int8_t)val;
        break; // SXTB
    case 5:
        x = (uint64_t)(int16_t)val;
        break; // SXTH
    case 6:
        x = (uint64_t)(int32_t)val;
        break; // SXTW
    default:
        x = val;
        break; // UXTX(3) / SXTX(7)：整寄存器
    }
    return x << shift;
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

enum emu_insn_result
{
    EMU_INSN_HANDLED,
    EMU_INSN_SKIP,
    EMU_INSN_FAULT,
};

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
    regs->pc = eval_cond_fast(regs->pstate, insn & 0xF) ? (pc + emu_bcond_displacement(insn)) : (pc + 4);
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
    if ((insn & 0xFF9F0000) == 0xD61F0000)
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

static __always_inline void emu_simulate_adr_adrp(struct pt_regs *regs, uint32_t insn, uint64_t pc)
{
    uint32_t rd = insn & 0x1F;
    s64 imm = sign_extend64(((insn >> 5) & 0x7FFFF) << 2 | ((insn >> 29) & 0x3), 20);

    // Rd=31 在 ADR/ADRP 中表示丢弃结果(XZR)；pt_regs.regs[] 只有 0..30，不能写 regs[31]。
    if (rd != 31)
        regs->regs[rd] = (insn & 0x80000000) ? ((pc & ~0xFFFULL) + (imm << 12)) : (pc + imm);
    regs->pc = pc + 4;
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

            if (emu_read_mem(addr, bytes, &val))
                return EMU_INSN_FAULT;
            fp_regs[rt] = val;
            fp_dirty = true;
        }
        else
        {
            __uint128_t val;

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
                reg_write(regs, rt, (s64)(s32)(u32)val, true);
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
        int bytes = is_fp ? (4 << opc_pair) : ((opc_pair == 2) ? 8 : 4);
        s64 off = sign_extend64((s64)((insn >> 15) & 0x7F), 6) * bytes;
        uint64_t base = addr_reg_read(regs, rn);
        uint64_t addr = (idx == 1) ? base : (base + off);

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
                reg_write(regs, rt, (s64)(s32)(u32)val1, true);
                reg_write(regs, rt2, (s64)(s32)(u32)val2, true);
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
        int bytes;
        bool is_load;

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
                addr = (idx == 3) ? (base + imm9) : base;
            else if (idx == 2 && reg_form)
            {
                uint32_t rm = (insn >> 16) & 0x1F, opt = (insn >> 13) & 0x7;
                s64 ext = reg_read(regs, rm);
                int shift;

                if (opt == 6)
                    ext = (s64)(s32)ext;
                else if (opt == 2)
                    ext = (uint64_t)(uint32_t)ext;
                shift = ((insn >> 12) & 1) ? __builtin_ctz(bytes) : 0;
                addr = base + (ext << shift);
            }
            else
                return EMU_INSN_SKIP;
            if (idx & 1)
                addr_reg_write(regs, rn, base + imm9);
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
                    int b = (bytes << 3) - 1;
                    if (raw & (1ULL << b))
                        raw |= ~((1ULL << (b + 1)) - 1);
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
    uint64_t a, result;

    if (sh)
        imm <<= 12;

    a = addr_reg_read(regs, rn);
    result = op_sub ? (a - imm) : (a + imm);
    if (!sf)
        result = (uint32_t)result;

    if (setflags)
    {
        emu_set_nzcv_addsub(regs, a, imm, op_sub, sf);
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
    uint64_t a, b, result;

    if (shift_type == 3)
        return EMU_INSN_SKIP;
    if (!sf && (imm6 & 0x20))
        return EMU_INSN_SKIP;

    a = reg_read(regs, rn);
    b = emu_shift_reg(reg_read(regs, rm), shift_type, imm6, sf);
    result = op_sub ? (a - b) : (a + b);
    if (!sf)
        result = (uint32_t)result;

    if (setflags)
        emu_set_nzcv_addsub(regs, a, b, op_sub, sf);
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
    uint64_t a, b, result;

    if (imm3 > 4)
        return EMU_INSN_SKIP;

    a = addr_reg_read(regs, rn);
    b = emu_extend_reg(reg_read(regs, rm), option, imm3);
    result = op_sub ? (a - b) : (a + b);
    if (!sf)
        result = (uint32_t)result;

    if (setflags)
    {
        emu_set_nzcv_addsub(regs, a, b, op_sub, sf);
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
    uint64_t a, b, result;

    if (!sf && (imm6 & 0x20))
        return EMU_INSN_SKIP;

    a = reg_read(regs, rn);
    b = emu_shift_reg(reg_read(regs, rm), shift_type, imm6, sf);
    if (invert)
        b = ~b;

    switch (opc)
    {
    case 0:
        result = a & b;
        break;
    case 1:
        result = a | b;
        break;
    case 2:
        result = a ^ b;
        break;
    default:
        result = a & b;
        break;
    }
    if (!sf)
        result = (uint32_t)result;

    if (opc == 3)
    {
        uint64_t pstate = regs->pstate & ~((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28));
        if (sf ? ((result >> 63) & 1) : ((result >> 31) & 1))
            pstate |= (1ULL << 31);
        if (result == 0)
            pstate |= (1ULL << 30);
        regs->pstate = pstate;
    }
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
    uint64_t imm, a, result;

    if (!emu_decode_bitmask_imm(n, immr, imms, sf, &imm))
        return EMU_INSN_SKIP;

    a = reg_read(regs, rn) & emu_mask_for_width(sf);
    switch (opc)
    {
    case 0:
        result = a & imm;
        break;
    case 1:
        result = a | imm;
        break;
    case 2:
        result = a ^ imm;
        break;
    default:
        result = a & imm;
        break;
    }
    result &= emu_mask_for_width(sf);

    if (opc == 3)
    {
        uint64_t pstate = regs->pstate & ~((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28));

        if (sf ? ((result >> 63) & 1) : ((result >> 31) & 1))
            pstate |= (1ULL << 31);
        if (result == 0)
            pstate |= (1ULL << 30);
        regs->pstate = pstate;
    }

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
    bot = emu_ror(src, immr, sf) & wmask;

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
    uint64_t high, low, result;

    if (sf != !!n)
        return EMU_INSN_SKIP;
    if (imms >= width)
        return EMU_INSN_SKIP;

    high = reg_read(regs, rn) & emu_mask_for_width(sf);
    low = reg_read(regs, rm) & emu_mask_for_width(sf);
    if (!imms)
        result = low;
    else
        result = (low >> imms) | (high << (width - imms));

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

    if (eval_cond_fast(regs->pstate, cond))
    {
        result = reg_read(regs, rn);
    }
    else
    {
        result = reg_read(regs, rm);
        if (op)
            result = ~result;
        if (op2)
            result++;
    }

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
    uint32_t width = sf ? 64 : 32;
    uint64_t a = reg_read(regs, rn) & emu_mask_for_width(sf);
    uint64_t b = reg_read(regs, rm) & emu_mask_for_width(sf);
    uint64_t result;
    uint32_t shift;

    switch (opcode)
    {
    case 2:
        result = b ? (a / b) : 0;
        break;
    case 3:
        if (b == 0)
        {
            result = 0;
        }
        else if (sf && a == (1ULL << 63) && b == ~0ULL)
        {
            result = a;
        }
        else if (!sf && (uint32_t)a == (1U << 31) && (uint32_t)b == 0xFFFFFFFFU)
        {
            result = (uint32_t)a;
        }
        else if (sf)
        {
            result = (uint64_t)((s64)a / (s64)b);
        }
        else
        {
            result = (uint32_t)((s32)(uint32_t)a / (s32)(uint32_t)b);
        }
        break;
    case 8:
        shift = b & (width - 1);
        result = a << shift;
        break;
    case 9:
        shift = b & (width - 1);
        result = a >> shift;
        break;
    case 10:
        shift = b & (width - 1);
        result = sf ? (uint64_t)((s64)a >> shift) : (uint32_t)((s32)(uint32_t)a >> shift);
        break;
    case 11:
        shift = b & (width - 1);
        result = emu_ror(a, shift, sf);
        break;
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
    uint64_t product, result;

    if (sf)
        product = multiplicand * multiplier;
    else
        product = (uint32_t)multiplicand * (uint32_t)multiplier;

    result = op ? (addend - product) : (addend + product);
    reg_write(regs, rd, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static __always_inline enum emu_insn_result emu_simulate_data_processing_insn(struct pt_regs *regs, uint32_t insn)
{
    if ((insn & 0x1F800000) == 0x11000000)
        return emu_simulate_add_sub_imm(regs, insn);
    if ((insn & 0x1F800000) == 0x12000000)
        return emu_simulate_logic_imm(regs, insn);
    if ((insn & 0x7F800000) == 0x13000000 ||
        (insn & 0x7F800000) == 0x33000000 ||
        (insn & 0x7F800000) == 0x53000000)
        return emu_simulate_bitfield(regs, insn);
    if ((insn & 0x7FA00000) == 0x13800000)
        return emu_simulate_extract(regs, insn);
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
    if ((insn & 0x1F800000) == 0x12800000)
        return emu_simulate_move_wide(regs, insn);

    return EMU_INSN_SKIP;
}

// 取指后只做顶层分发；具体语义由 emu_simulate_* handler 完成。
static __always_inline bool emulate_insn(struct pt_regs *regs)
{
    uint32_t insn;
    uint64_t pc = regs->pc;
    uint32_t iclass;
    enum emu_insn_result result = EMU_INSN_SKIP;

    if (__get_user(insn, (uint32_t __user *)pc))
        return false;

    iclass = (insn >> 25) & 0xF;

    if ((iclass & 0xE) == 0xA)
    {
        result = emu_simulate_branch_insn(regs, insn, pc);
    }
    else if ((insn & 0x1F000000) == 0x10000000)
    {
        emu_simulate_adr_adrp(regs, insn, pc);
        result = EMU_INSN_HANDLED;
    }
    else if (((insn & 0x3A000000) == 0x28000000) ||
             ((insn & 0x3A000000) == 0x38000000) ||
             ((insn & 0x3B000000) == 0x18000000))
    {
        result = emu_simulate_load_store_insn(regs, insn, pc);
    }
    else
    {
        result = emu_simulate_data_processing_insn(regs, insn);
    }

    if (result == EMU_INSN_HANDLED)
        return true;
    if (result == EMU_INSN_SKIP)
        regs->pc = pc + 4;

    return false;
}

#endif // EMULATE_INSN_H
