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
    false : 未真正模拟，分两种情况，靠 PC 是否变化区分：
            - 跳过 (标签 next_insn)：不支持或无需副作用的指令，已 PC+=4。
            - 取指/访存失败 (标签 fault)：__get_user / __put_user 出错，
              PC 保持不变，交由硬件缺页/重放机制处理。

  【分发结构】按 ARM64 顶层编码组 (iclass = insn[28:25]) 逐层分流：
    第一部分  分支 / 异常 / 系统组      (iclass & 0xE) == 0xA
        -> B, BL, BR/BLR/RET, B.cond, CBZ/CBNZ, TBZ/TBNZ
           其余成员 (SVC/HVC/SMC/BRK、MSR/MRS/系统、HINT 等) 落到 next_insn 跳过。
    第二部分  PC 相对地址计算           (insn & 0x1F000000) == 0x10000000
        -> ADR, ADRP。
    第三部分  Load/Store 访存           三条入口掩码 (LDP/STP 对 / 单寄存器 / 字面量)
        -> 成对：LDP/STP/LDNP/STNP (含 LDPSW)
           单个：LDR/STR —— unsigned-imm、unscaled(LDUR/STUR)、pre/post-index、
                 register-offset(含 UXTW/SXTW/LSL/SXTX)
           字面：LDR(literal)、LDRSW(literal)
           整数 (X/W) 与浮点/SIMD (B/H/S/D/Q) 在此统一处理。
    第四部分  数据处理指令 (常见于函数序言/寄存器搬运)
        -> ADD/SUB 立即数     (insn & 0x1F800000)==0x11000000  含 SP 语义、ADDS/SUBS
           ADD/SUB 移位寄存器 (insn & 0x1F200000)==0x0B000000  LSL/LSR/ASR
           ADD/SUB 扩展寄存器 (insn & 0x1FE00000)==0x0B200000  含 SP 与 U/SXTB..X
           逻辑 移位寄存器    (insn & 0x1F000000)==0x0A000000  AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS
           宽立即数           (insn & 0x1F800000)==0x12800000  MOVN/MOVZ/MOVK

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
        AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS (移位寄存器)、MOV/MVN (别名)、MOVN/MOVZ/MOVK
        * 立即数/扩展寄存器形态正确区分 SP 与 XZR 语义；移位含 LSL/LSR/ASR/ROR。

  【暂不支持】(遇到即跳过：仅 PC+=4，不产生其它副作用)
    - 数据处理(未覆盖)：逻辑立即数(AND/ORR/EOR #bimm)、位域 SBFM/UBFM/BFM、
        EXTR、条件选择 CSEL/CSINC、乘除 MADD/MUL/UDIV/SDIV、CCMP、地址无关的 REV/CLZ 等
    - 独占/有序访存：LDXR/STXR/LDAXR/STLXR/LDAR/STLR (编码段不同，天然不进入本模块)
    - LSE 原子：SWP/CAS/CASP/LDADD/LDSET/LDCLR/LDEOR/LDSMAX/LDSMIN/... (识别后跳过)
    - 指针认证加载：LDRAA/LDRAB (识别后跳过)
    - 预取：PRFM (立即数/寄存器/字面量，按 nop 处理，只推进 PC，不实际预取)
    - 异常/系统：SVC/HVC/SMC/BRK、MSR/MRS 及系统指令
    - 向量结构化访存：LD1/ST1/LD1R 等 Advanced SIMD 多元素访存

  【后续扩展指令的方法】
    1) 判断新指令属于哪个顶层编码组，在对应部分新增解码分支；
    2) 命中后按语义写回：PC / 通用寄存器用 reg_write(XZR 语义)、基址用 addr_reg_*(SP 语义)；
    3) 访问用户内存一律用 __get_user / __put_user，失败 goto fault；
    4) 需要浮点寄存器时用 read_q_reg / write_q_reg，Load 修改后置 fp_dirty 触发回写；
    5) 能完整模拟的返回 true，无法处理但可安全跳过的走 next_insn。

  【辅助函数】(均定义在 emulate_insn 之前，遵循"定义在前、使用在后")
    reg_read / reg_write     : X/W 通用寄存器，n==31 视为 XZR (读 0 / 写丢弃)
    addr_reg_read / _write   : 基址寄存器，n==31 视为 SP
    eval_cond_fast           : 依据 PSTATE.NZCV 求条件码 (供 B.cond)
    emu_read_mem             : 按 1/2/4/8/16 字节从用户内存读入 128 位缓冲(高位零扩展)
    emu_write_mem            : 按 1/2/4/8/16 字节把值的低位写回用户内存
                               二者失败返回 -EFAULT，是三类访存分支共用的读写内核。
    emu_set_nzcv_addsub      : 依加/减结果刷新 PSTATE.NZCV (供 ADDS/SUBS/CMP/CMN)
    emu_shift_reg            : 寄存器移位 LSL/LSR/ASR/ROR (供移位寄存器类)
    emu_extend_reg           : 寄存器扩展 U/SXTB..X + 左移 (供 ADD/SUB 扩展寄存器)
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
   成功返回 0，__get_user/__put_user 失败返回 -EFAULT (调用方据此 goto fault)。 */
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

/* ---- 数据处理指令通用逻辑：供第四部分各分支复用 ---- */

// 依据 a (加/减) b 的结果刷新 PSTATE.NZCV，供 ADDS/SUBS/CMP/CMN。
// op_sub=false 为加法、true 为减法；sf=true 为 64 位、false 为 32 位。
static __always_inline void emu_set_nzcv_addsub(struct pt_regs *regs, uint64_t a, uint64_t b, bool op_sub, bool sf)
{
    bool n, z, c, v;

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

    uint64_t pstate = regs->pstate & ~((1ULL << 31) | (1ULL << 30) | (1ULL << 29) | (1ULL << 28));
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

// 模拟执行函数
static __always_inline bool emulate_insn(struct pt_regs *regs)
{
    uint32_t insn;
    uint64_t pc = regs->pc;

    if (__get_user(insn, (uint32_t __user *)pc))
        goto fault;

    uint32_t iclass = (insn >> 25) & 0xF;

    // --- 第一部分：跳转指令 ---
    if ((iclass & 0xE) == 0xA)
    {
        uint32_t op_branch = insn & 0xFC000000;
        if (op_branch == 0x14000000) // B
        {
            regs->pc = pc + sign_extend64((s64)(insn & 0x3FFFFFF) << 2, 27);
            return true;
        }
        if (op_branch == 0x94000000) // BL
        {
            regs->regs[30] = pc + 4;
            regs->pc = pc + sign_extend64((s64)(insn & 0x3FFFFFF) << 2, 27);
            return true;
        }
        if ((insn & 0xFF9F0000) == 0xD61F0000) // BR/BLR/RET
        {
            uint32_t rn = (insn >> 5) & 0x1F, opc = (insn >> 21) & 0x3;
            if (opc == 1)
                regs->regs[30] = pc + 4;
            if (opc <= 2)
            {
                regs->pc = reg_read(regs, rn);
                return true;
            }
        }
        if ((insn & 0xFF000010) == 0x54000000) // B.cond
        {
            s64 offset = sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);
            regs->pc = eval_cond_fast(regs->pstate, insn & 0xF) ? (pc + offset) : (pc + 4);
            return true;
        }
        if ((insn & 0x7E000000) == 0x34000000) // CBZ/CBNZ
        {
            uint32_t rt = insn & 0x1F;
            uint64_t val = ((insn >> 31) & 1) ? reg_read(regs, rt) : (uint32_t)reg_read(regs, rt);
            bool jump = ((insn >> 24) & 1) ? (val != 0) : (val == 0);
            regs->pc = jump ? (pc + sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20)) : (pc + 4);
            return true;
        }
        if ((insn & 0x7E000000) == 0x36000000) // TBZ/TBNZ
        {
            uint32_t rt = insn & 0x1F, pos = (((insn >> 31) & 1) << 5) | ((insn >> 19) & 0x1F);
            bool jump = (((reg_read(regs, rt) >> pos) & 1) == ((insn >> 24) & 1));
            regs->pc = jump ? (pc + sign_extend64((s64)((insn >> 5) & 0x3FFF) << 2, 15)) : (pc + 4);
            return true;
        }
        goto next_insn;
    }

    // --- 第二部分：地址计算 ADR / ADRP ---
    if ((insn & 0x1F000000) == 0x10000000)
    {
        uint32_t rd = insn & 0x1F;
        s64 imm = sign_extend64(((insn >> 5) & 0x7FFFF) << 2 | ((insn >> 29) & 0x3), 20);
        // Rd=31 在 ADR/ADRP 中表示丢弃结果(XZR)；pt_regs.regs[] 只有 0..30，
        // 直接写 regs[31] 会越界踩到紧邻的 sp 字段，必须跳过。
        if (rd != 31)
            regs->regs[rd] = (insn & 0x80000000) ? ((pc & ~0xFFFULL) + (imm << 12)) : (pc + imm);
        regs->pc += 4;
        return true;
    }

    // --- 第三部分：Load/Store 访存 ---
    // 高级掩码过滤：忽略了第26位(V位)，同时精准捕获整数和浮点访存指令
    if (((insn & 0x3A000000) == 0x28000000) || // LDP/STP (成对访存)
        ((insn & 0x3A000000) == 0x38000000) || // LDR/STR (单寄存器)
        ((insn & 0x3B000000) == 0x18000000))   // LDR (基于 PC 的字面量)
    {
        // 说明：LDXR/STXR/CAS/LDAR/STLR 等独占与有序访存属于 bits[29:24]=001000 编码段，
        // 三个入口掩码都要求 bit27=1 且 bit29/bit28 至少一个为 1，因此它们根本进不来；
        // 而 LSE 原子操作(SWP/LDADD...)会命中单寄存器掩码，统一在下方寻址分支识别并跳过。

        // V位 (第26位) 为 1 时，代表这是浮点/SIMD指令
        bool is_fp = (insn & 0x04000000) != 0;
        uint32_t size = (insn >> 30) & 0x3;

        __uint128_t fp_regs[32];
        uint32_t fpsr = 0, fpcr = 0;
        bool fp_dirty = false;

        // 仅当确认是浮点指令时，按需拉取物理 CPU 当前的 FPU 状态
        if (is_fp)
        {
            int i;

            for (i = 0; i < 32; i++)
                read_q_reg(i, &fp_regs[i]);
            fpsr = read_fpsr();
            fpcr = read_fpcr();
        }

        // 字面量加载 LDR (Literal) [PC 相对寻址]
        if ((insn & 0x3B000000) == 0x18000000)
        {
            uint32_t rt = insn & 0x1F;
            uint64_t addr = pc + sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);

            if (is_fp)
            {
                // 浮点字面量: opc(=size)=0(S/4B), 1(D/8B), 2(Q/16B)
                int bytes = (size == 0) ? 4 : ((size == 1) ? 8 : 16);
                __uint128_t v;
                if (emu_read_mem(addr, bytes, &v))
                    goto fault;
                fp_regs[rt] = v;
                fp_dirty = true;
            }
            else
            {
                // 字面量整数加载 opc(=size): 00=LDR Wt(4B 零扩展), 01=LDR Xt(8B),
                // 10=LDRSW(4B 符号扩展到 64 位), 11=PRFM(预取, 不写寄存器)
                __uint128_t v;
                if (size == 0)
                {
                    if (emu_read_mem(addr, 4, &v))
                        goto fault;
                    reg_write(regs, rt, (u64)v, false); // LDR Wt
                }
                else if (size == 1)
                {
                    if (emu_read_mem(addr, 8, &v))
                        goto fault;
                    reg_write(regs, rt, (u64)v, true); // LDR Xt
                }
                else if (size == 2)
                {
                    if (emu_read_mem(addr, 4, &v))
                        goto fault;
                    reg_write(regs, rt, (s64)(s32)(u32)v, true); // LDRSW
                }
                // size == 3 为 PRFM (literal) 预取，无寄存器写入，落到 done_ldst 仅 PC+=4
            }
            goto done_ldst;
        }

        // LDP / STP (Load/Store Pair 成对读写)
        if ((insn & 0x3A000000) == 0x28000000)
        {
            uint32_t opc_pair = (insn >> 30) & 0x3, l = (insn >> 22) & 1, idx = (insn >> 23) & 0x3;
            uint32_t rn = (insn >> 5) & 0x1F, rt = insn & 0x1F, rt2 = (insn >> 10) & 0x1F;

            // 每个寄存器字节数: 浮点 opc=0/1/2 -> S(4)/D(8)/Q(16); 整数 opc=2 -> X(8), 否则 W(4)
            int bytes = is_fp ? (4 << opc_pair) : ((opc_pair == 2) ? 8 : 4);
            s64 off = sign_extend64((s64)((insn >> 15) & 0x7F), 6) * bytes;
            uint64_t base = addr_reg_read(regs, rn), addr = (idx == 1) ? base : (base + off);
            // idx: 0=LDNP/STNP(非临时,base+off,无回写), 1=post-index, 2=offset, 3=pre-index
            // 四种都按 base±off 正常访存; idx=0 与 offset 等价(无回写), 无需特殊跳过。
            // 成对的两个元素分别位于 addr 与 addr+bytes。

            if (l)
            { // Load Pair
                __uint128_t v1, v2;
                if (emu_read_mem(addr, bytes, &v1) || emu_read_mem(addr + bytes, bytes, &v2))
                    goto fault;
                if (is_fp)
                {
                    fp_regs[rt] = v1;
                    fp_regs[rt2] = v2;
                    fp_dirty = true;
                }
                else if (opc_pair == 1)
                { // LDPSW: 32 位符号扩展到 64 位
                    reg_write(regs, rt, (s64)(s32)(u32)v1, true);
                    reg_write(regs, rt2, (s64)(s32)(u32)v2, true);
                }
                else
                { // LDP: opc=0(W,32位) / opc=2(X,64位)
                    reg_write(regs, rt, (u64)v1, bytes == 8);
                    reg_write(regs, rt2, (u64)v2, bytes == 8);
                }
            }
            else
            { // Store Pair
                __uint128_t v1 = is_fp ? fp_regs[rt] : (__uint128_t)reg_read(regs, rt);
                __uint128_t v2 = is_fp ? fp_regs[rt2] : (__uint128_t)reg_read(regs, rt2);
                if (emu_write_mem(addr, bytes, v1) || emu_write_mem(addr + bytes, bytes, v2))
                    goto fault;
            }
            if (idx & 1)
                addr_reg_write(regs, rn, base + off); // 回写基址 Write-back
            goto done_ldst;
        }

        // LDR / STR (单寄存器基础寻址)
        uint32_t rn = (insn >> 5) & 0x1F, rt = insn & 0x1F, opc = (insn >> 22) & 0x3;
        uint64_t base = addr_reg_read(regs, rn), addr = base;

        int bytes;
        if (is_fp)
        {
            // 浮点 128-bit (Q) 寄存器: size=00 且 bit23=1 (opc=10 存 Q / opc=11 取 Q)
            if (size == 0 && (opc & 2))
                bytes = 16;
            else
                bytes = (1 << size); // 支持 B(1字节), H(2字节), S(4字节), D(8字节)
        }
        else
        {
            bytes = (1 << size); // 整数: B(1), H(2), W(4), X(8)
        }

        // 整数域 size=11 且 opc=1x 为 PRFM 预取(及未分配编码): 无数据搬运，且若继续
        // 走有符号加载会触发 1<<64 的未定义移位，这里在计算地址前直接按 nop 跳过。
        if (!is_fp && size == 3 && opc >= 2)
            goto next_insn;

        if ((insn >> 24) & 1)
        {
            addr = base + (((insn >> 10) & 0xFFF) * bytes); // 严格乘以真实字节数，确保 Q(16) 和 D(8) 正确
        }
        else
        {
            uint32_t idx = (insn >> 10) & 0x3;
            bool reg_form = ((insn >> 21) & 1) != 0;
            s64 imm9 = sign_extend64((s64)((insn >> 12) & 0x1FF), 8);

            // bit21=1 时唯一能当普通访存模拟的是"寄存器偏移"(bits[11:10]=10, 即 idx==2)。
            // 其余 bit21=1 组合是 LSE 原子操作(SWP/LDADD/LDSET/LDCLR...)或指针认证加载
            // (LDRAA/LDRAB)，语义无法用一次普通读写替代，直接交由硬件重放。
            if (reg_form && idx != 2)
                goto next_insn;

            if (idx == 0)
                addr = base + imm9; // 无扩展 (Unscaled: LDUR/STUR)
            else if (idx == 1 || idx == 3)
                addr = (idx == 3) ? (base + imm9) : base; // Pre / Post-index
            else if (idx == 2 && reg_form)
            { // 寄存器偏移 (如 LDR X0, [X1, W2, UXTW #3])
                uint32_t rm = (insn >> 16) & 0x1F, opt = (insn >> 13) & 0x7;
                s64 ext = reg_read(regs, rm);
                if (opt == 6)
                    ext = (s64)(s32)ext;
                else if (opt == 2)
                    ext = (uint64_t)(uint32_t)ext;                         // 严格区分带符号(SXTW)和无符号(UXTW)
                int shift = ((insn >> 12) & 1) ? __builtin_ctz(bytes) : 0; // 自动推导 LSL 移位量: Q移4, D移3, S移2, H移1
                addr = base + (ext << shift);
            }
            else
                goto next_insn; // idx==2 但 bit21=0，非法编码，跳过
            if (idx & 1)
                addr_reg_write(regs, rn, base + imm9); // Write-back
        }

        // 判断是 Load 还是 Store
        bool is_load = is_fp ? ((insn >> 22) & 1) : (opc != 0);

        if (is_load)
        { // Load 单一寄存器
            __uint128_t v;
            if (emu_read_mem(addr, bytes, &v))
                goto fault;
            if (is_fp)
            {
                fp_regs[rt] = v; // 128 位缓冲已把高位清零
                fp_dirty = true;
            }
            else
            {
                u64 raw = (u64)v;
                if (opc >= 2)
                { // 有符号 Load (LDRSB/LDRSH/LDRSW)：自最高有效位起符号扩展
                    int b = (bytes << 3) - 1;
                    if (raw & (1ULL << b))
                        raw |= ~((1ULL << (b + 1)) - 1);
                }
                reg_write(regs, rt, raw, (size == 3 || opc == 2));
            }
        }
        else
        { // Store 单一寄存器：整数只读低 64 位，浮点直接取 128 位
            __uint128_t v = is_fp ? fp_regs[rt] : (__uint128_t)reg_read(regs, rt);
            if (emu_write_mem(addr, bytes, v))
                goto fault;
        }

    done_ldst:
        // 如果处理了浮点指令，并且是一条 Load (产生了数据修改)，则强制回写到物理 CPU
        if (is_fp && fp_dirty)
        {
            int i;

            for (i = 0; i < 32; i++)
                write_q_reg(i, &fp_regs[i]);
            write_fpsr(fpsr);
            write_fpcr(fpcr);
        }
        regs->pc += 4;
        return true;
    }

    // --- 第四部分：数据处理指令 (常见于函数序言/寄存器搬运) ---
    // ADD/SUB (立即数)：覆盖 SUB SP,SP,#N、ADD X29,SP,#N、MOV Xd,SP(=ADD #0) 等。
    // 立即数形态 Rn/Rd=31 表示 SP(非 XZR)；但 ADDS/SUBS 的 Rd=31 表示 XZR(即 CMP/CMN)。
    if ((insn & 0x1F800000) == 0x11000000)
    {
        bool sf = (insn >> 31) & 1;
        bool op_sub = (insn >> 30) & 1;   // 0=ADD, 1=SUB
        bool setflags = (insn >> 29) & 1; // 1=ADDS/SUBS
        uint32_t sh = (insn >> 22) & 1;   // 1 => 立即数左移 12
        uint64_t imm = (insn >> 10) & 0xFFF;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a, result;

        if (sh)
            imm <<= 12;

        a = addr_reg_read(regs, rn); // Rn=31 读 SP
        result = op_sub ? (a - imm) : (a + imm);
        if (!sf)
            result = (uint32_t)result;

        if (setflags)
        {
            emu_set_nzcv_addsub(regs, a, imm, op_sub, sf);
            reg_write(regs, rd, result, sf); // Rd=31 => XZR(丢弃)
        }
        else
        {
            addr_reg_write(regs, rd, result); // Rd=31 => SP
        }
        regs->pc += 4;
        return true;
    }

    // ADD/SUB (移位寄存器)：Rn/Rd/Rm=31 均为 XZR。SUBS Rd=31 即 CMP。
    if ((insn & 0x1F200000) == 0x0B000000)
    {
        bool sf = (insn >> 31) & 1;
        bool op_sub = (insn >> 30) & 1;
        bool setflags = (insn >> 29) & 1;
        uint32_t shift_type = (insn >> 22) & 0x3;
        uint32_t rm = (insn >> 16) & 0x1F, imm6 = (insn >> 10) & 0x3F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a, b, result;

        if (shift_type == 3) // ROR 对 add/sub 非法
            goto next_insn;
        if (!sf && (imm6 & 0x20)) // 32 位时移位量 >=32 非法
            goto next_insn;

        a = reg_read(regs, rn);
        b = emu_shift_reg(reg_read(regs, rm), shift_type, imm6, sf);
        result = op_sub ? (a - b) : (a + b);
        if (!sf)
            result = (uint32_t)result;

        if (setflags)
            emu_set_nzcv_addsub(regs, a, b, op_sub, sf);
        reg_write(regs, rd, result, sf);
        regs->pc += 4;
        return true;
    }

    // ADD/SUB (扩展寄存器)：Rn/Rd=31 为 SP(ADDS/SUBS 的 Rd=31 为 XZR)，Rm 按 option 扩展。
    if ((insn & 0x1FE00000) == 0x0B200000)
    {
        bool sf = (insn >> 31) & 1;
        bool op_sub = (insn >> 30) & 1;
        bool setflags = (insn >> 29) & 1;
        uint32_t rm = (insn >> 16) & 0x1F, option = (insn >> 13) & 0x7, imm3 = (insn >> 10) & 0x7;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a, b, result;

        if (imm3 > 4) // 扩展左移量合法范围 0..4
            goto next_insn;

        a = addr_reg_read(regs, rn); // Rn=31 读 SP
        b = emu_extend_reg(reg_read(regs, rm), option, imm3);
        result = op_sub ? (a - b) : (a + b);
        if (!sf)
            result = (uint32_t)result;

        if (setflags)
        {
            emu_set_nzcv_addsub(regs, a, b, op_sub, sf);
            reg_write(regs, rd, result, sf); // Rd=31 => XZR
        }
        else
        {
            addr_reg_write(regs, rd, result); // Rd=31 => SP
        }
        regs->pc += 4;
        return true;
    }

    // 逻辑 (移位寄存器)：AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS，含别名 MOV Xd,Xm / MVN。
    if ((insn & 0x1F000000) == 0x0A000000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t opc = (insn >> 29) & 0x3; // 00=AND 01=ORR 10=EOR 11=ANDS
        uint32_t shift_type = (insn >> 22) & 0x3;
        bool invert = (insn >> 21) & 1; // N: 1 => BIC/ORN/EON/BICS
        uint32_t rm = (insn >> 16) & 0x1F, imm6 = (insn >> 10) & 0x3F;
        uint32_t rn = (insn >> 5) & 0x1F, rd = insn & 0x1F;
        uint64_t a, b, result;

        if (!sf && (imm6 & 0x20)) // 32 位时移位量 >=32 非法
            goto next_insn;

        a = reg_read(regs, rn);
        b = emu_shift_reg(reg_read(regs, rm), shift_type, imm6, sf);
        if (invert)
            b = ~b;

        switch (opc)
        {
        case 0:
            result = a & b;
            break; // AND / BIC
        case 1:
            result = a | b;
            break; // ORR / ORN (含 MOV/MVN)
        case 2:
            result = a ^ b;
            break; // EOR / EON
        default:
            result = a & b;
            break; // ANDS / BICS
        }
        if (!sf)
            result = (uint32_t)result;

        if (opc == 3) // ANDS/BICS 更新 NZCV：N/Z 依结果，C=V=0
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
        return true;
    }

    // 宽立即数搬运：MOVN(00)/MOVZ(10)/MOVK(11)，opc=01 未分配。
    if ((insn & 0x1F800000) == 0x12800000)
    {
        bool sf = (insn >> 31) & 1;
        uint32_t opc = (insn >> 29) & 0x3;
        uint32_t hw = (insn >> 21) & 0x3, shift = hw * 16;
        uint64_t imm16 = (insn >> 5) & 0xFFFF;
        uint32_t rd = insn & 0x1F;
        uint64_t result;

        if (opc == 1) // 未分配编码
            goto next_insn;
        if (!sf && (hw & 0x2)) // 32 位时 hw 只能是 0/1
            goto next_insn;

        if (opc == 0)
            result = ~(imm16 << shift); // MOVN
        else if (opc == 2)
            result = (imm16 << shift); // MOVZ
        else
            result = (reg_read(regs, rd) & ~(0xFFFFULL << shift)) | (imm16 << shift); // MOVK

        reg_write(regs, rd, result, sf); // sf=0 时截断到 32 位
        regs->pc += 4;
        return true;
    }

next_insn:
    // 如果遇到完全无法解析、或无需副作用的指令，静默跳过执行
    regs->pc += 4;
    return false;

fault:
    // 触发读写异常时，绝不可强制修改 PC，原样返回交由硬件机制介入
    return false;
}

#endif // EMULATE_INSN_H
