#ifndef EMULATE_INSN_H
#define EMULATE_INSN_H

#include <linux/uaccess.h>
#include <asm/ptrace.h>
#include <asm/insn.h>
#include "arm64_reg.h"

/* =========================================================================
  ARM64 指令模拟器

  当触发硬件数据断点 或指令断点 时，
  用于在软件层面直接计算出下一条 PC 或模拟内存读写，从而无需依赖

  支持的指令范围 (全寄存器 + 全位宽支持)：
  - 分支跳转：B, BL, BR, BLR, RET, B.cond, CBZ, CBNZ, TBZ, TBNZ
  - 地址计算：ADR, ADRP
  - 整数访存 (W/X 寄存器，8 ~ 64 位)：
      * LDR/STR, LDP/STP, LDRB/H/SW 
  - 浮点/SIMD访存 (B/H/S/D/Q 寄存器，8 ~ 128 位)：
      * LDR/STR (SIMD), LDP/STP (SIMD), LDR (Literal, SIMD)
      * 突破内核限制，直接读取物理 CPU 的 Q0-Q31 寄存器，支持 128-bit 模拟。

  不支持的指令 (遇到会跳过该指令, PC = PC + 4)：
  - ALU 计算指令：ADD, SUB, AND, LSL 等 
  - 原子/独占指令：LDXR, STXR, CAS, SWP 等
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
        // 独占/原子操作 (如 LDXR/STXR/SWP) 绝不模拟，直接跳过交由硬件
        if ((insn & 0x3F000000) == 0x08000000)
            goto next_insn;

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
                // 浮点字面量支持：opc=0(4字节/S), opc=1(8字节/D), opc=2(16字节/Q)
                int bytes = (size == 0) ? 4 : ((size == 1) ? 8 : 16);
                __uint128_t v = 0;
                if (bytes == 16)
                {
                    uint64_t l, h;
                    if (__get_user(l, (uint64_t __user *)addr) || __get_user(h, (uint64_t __user *)(addr + 8)))
                        goto fault;
                    v = ((__uint128_t)h << 64) | l;
                }
                else if (bytes == 8)
                {
                    uint64_t t;
                    if (__get_user(t, (uint64_t __user *)addr))
                        goto fault;
                    v = t;
                }
                else
                {
                    uint32_t t;
                    if (__get_user(t, (uint32_t __user *)addr))
                        goto fault;
                    v = t;
                }
                fp_regs[rt] = v;
                fp_dirty = true; // 赋值给 __uint128_t 自动完成高位清零
            }
            else
            {
                if (size == 1)
                {
                    // LDRSW: 读 4 字节并符号扩展到 8 字节
                    uint32_t t;
                    if (__get_user(t, (uint32_t __user *)addr))
                        goto fault;
                    reg_write(regs, rt, (s64)(s32)t, true);
                }
                else if (size == 3)
                {
                    // LDR Xt: 读 8 字节
                    uint64_t t;
                    if (__get_user(t, (uint64_t __user *)addr))
                        goto fault;
                    reg_write(regs, rt, t, true);
                }
                else
                {
                    // LDR Wt: 读 4 字节（零扩展）
                    uint32_t t;
                    if (__get_user(t, (uint32_t __user *)addr))
                        goto fault;
                    reg_write(regs, rt, t, false);
                }
            }
            goto done_ldst;
        }

        // LDP / STP (Load/Store Pair 成对读写)
        if ((insn & 0x3A000000) == 0x28000000)
        {
            uint32_t opc_pair = (insn >> 30) & 0x3, l = (insn >> 22) & 1, idx = (insn >> 23) & 0x3;
            uint32_t rn = (insn >> 5) & 0x1F, rt = insn & 0x1F, rt2 = (insn >> 10) & 0x1F;

            // 计算浮点与整数对齐字节数
            // 浮点(is_fp): opc=0(4B/S), opc=1(8B/D), opc=2(16B/Q) -> (4 << opc)
            // 整数(!is_fp): opc=0/1(4B/W), opc=2(8B/X)
            int bytes = is_fp ? (4 << opc_pair) : ((opc_pair == 2) ? 8 : 4);
            s64 off = sign_extend64((s64)((insn >> 15) & 0x7F), 6) * bytes;
            uint64_t base = addr_reg_read(regs, rn), addr = (idx == 1) ? base : (base + off);

            if (idx == 0)
                goto next_insn; // 没有这种操作模式，跳过

            if (l)
            { // Load Pair
                if (is_fp)
                {
                    __uint128_t v1 = 0, v2 = 0;
                    if (bytes == 16)
                    {
                        uint64_t l1, h1, l2, h2;
                        if (__get_user(l1, (uint64_t __user *)addr) || __get_user(h1, (uint64_t __user *)(addr + 8)) ||
                            __get_user(l2, (uint64_t __user *)(addr + 16)) || __get_user(h2, (uint64_t __user *)(addr + 24)))
                            goto fault;
                        v1 = ((__uint128_t)h1 << 64) | l1;
                        v2 = ((__uint128_t)h2 << 64) | l2;
                    }
                    else if (bytes == 8)
                    {
                        uint64_t t1, t2;
                        if (__get_user(t1, (uint64_t __user *)addr) || __get_user(t2, (uint64_t __user *)(addr + 8)))
                            goto fault;
                        v1 = t1;
                        v2 = t2;
                    }
                    else
                    {
                        uint32_t t1, t2;
                        if (__get_user(t1, (uint32_t __user *)addr) || __get_user(t2, (uint32_t __user *)(addr + 4)))
                            goto fault;
                        v1 = t1;
                        v2 = t2;
                    }
                    fp_regs[rt] = v1;
                    fp_regs[rt2] = v2;
                    fp_dirty = true;
                }
                else
                {
                    uint64_t v1, v2;
                    if (bytes == 8)
                    {
                        if (__get_user(v1, (uint64_t __user *)addr) || __get_user(v2, (uint64_t __user *)(addr + 8)))
                            goto fault;
                    }
                    else
                    {
                        uint32_t t1, t2;
                        if (__get_user(t1, (uint32_t __user *)addr) || __get_user(t2, (uint32_t __user *)(addr + 4)))
                            goto fault;
                        v1 = (opc_pair == 1) ? (uint64_t)(s64)(s32)t1 : t1;
                        v2 = (opc_pair == 1) ? (uint64_t)(s64)(s32)t2 : t2; // 处理 LDPSW 符号扩展
                    }
                    reg_write(regs, rt, v1, (opc_pair >= 1));
                    reg_write(regs, rt2, v2, (opc_pair >= 1));
                }
            }
            else
            { // Store Pair
                if (is_fp)
                {
                    __uint128_t v1 = fp_regs[rt], v2 = fp_regs[rt2];
                    if (bytes == 16)
                    {
                        if (__put_user((uint64_t)v1, (uint64_t __user *)addr) || __put_user((uint64_t)(v1 >> 64), (uint64_t __user *)(addr + 8)) ||
                            __put_user((uint64_t)v2, (uint64_t __user *)(addr + 16)) || __put_user((uint64_t)(v2 >> 64), (uint64_t __user *)(addr + 24)))
                            goto fault;
                    }
                    else if (bytes == 8)
                    {
                        if (__put_user((uint64_t)v1, (uint64_t __user *)addr) || __put_user((uint64_t)v2, (uint64_t __user *)(addr + 8)))
                            goto fault;
                    }
                    else
                    {
                        if (__put_user((uint32_t)v1, (uint32_t __user *)addr) || __put_user((uint32_t)v2, (uint32_t __user *)(addr + 4)))
                            goto fault;
                    }
                }
                else
                {
                    if (bytes == 8)
                    {
                        if (__put_user(reg_read(regs, rt), (uint64_t __user *)addr) || __put_user(reg_read(regs, rt2), (uint64_t __user *)(addr + 8)))
                            goto fault;
                    }
                    else
                    {
                        if (__put_user((uint32_t)reg_read(regs, rt), (uint32_t __user *)addr) || __put_user((uint32_t)reg_read(regs, rt2), (uint32_t __user *)(addr + 4)))
                            goto fault;
                    }
                }
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
            // 浮点 128-bit (Q) 寄存器: size=00 且 opc=11
            if (size == 0 && opc == 3)
                bytes = 16;
            else
                bytes = (1 << size); // 支持 B(1字节), H(2字节), S(4字节), D(8字节)
        }
        else
        {
            bytes = (1 << size); // 整数: B(1), H(2), W(4), X(8)
        }

        if ((insn >> 24) & 1)
        {
            addr = base + (((insn >> 10) & 0xFFF) * bytes); // 严格乘以真实字节数，确保 Q(16) 和 D(8) 正确
        }
        else
        {
            uint32_t idx = (insn >> 10) & 0x3;
            s64 imm9 = sign_extend64((s64)((insn >> 12) & 0x1FF), 8);
            if (idx == 0)
                addr = base + imm9; // 无扩展 (Unscaled)
            else if (idx == 1 || idx == 3)
                addr = (idx == 3) ? (base + imm9) : base; // Pre / Post-index
            else if (idx == 2 && ((insn >> 21) & 1))
            { // 寄存器偏移 (如 LDR X0, [X1, W2, UXTW #3])
                uint32_t rm = (insn >> 16) & 0x1F, opt = (insn >> 13) & 0x7;
                s64 ext = reg_read(regs, rm);
                if (opt == 6)
                    ext = (s64)(s32)ext;
                else if (opt == 2)
                    ext = (uint64_t)(uint32_t)ext;                                   // 严格区分带符号(SXTW)和无符号(UXTW)
                int shift = ((insn >> 12) & 1) ? __builtin_ctz(bytes) : 0; // 自动推导 LSL 移位量: Q移4, D移3, S移2, H移1
                addr = base + (ext << shift);
            }
            else
                goto next_insn;
            if (idx & 1)
                addr_reg_write(regs, rn, base + imm9); // Write-back
        }

        // 判断是 Load 还是 Store
        bool is_load = is_fp ? ((insn >> 22) & 1) : (opc != 0);

        if (is_load)
        { // Load 单一寄存器
            if (is_fp)
            {
                __uint128_t v = 0;
                if (bytes == 16)
                { // Load Q
                    uint64_t l, h;
                    if (__get_user(l, (uint64_t __user *)addr) || __get_user(h, (uint64_t __user *)(addr + 8)))
                        goto fault;
                    v = ((__uint128_t)h << 64) | l;
                }
                else if (bytes == 8)
                {
                    uint64_t t;
                    if (__get_user(t, (uint64_t __user *)addr))
                        goto fault;
                    v = t;
                } // Load D
                else if (bytes == 4)
                {
                    uint32_t t;
                    if (__get_user(t, (uint32_t __user *)addr))
                        goto fault;
                    v = t;
                } // Load S
                else if (bytes == 2)
                {
                    u16 t;
                    if (__get_user(t, (u16 __user *)addr))
                        goto fault;
                    v = t;
                } // Load H
                else
                {
                    u8 t;
                    if (__get_user(t, (u8 __user *)addr))
                        goto fault;
                    v = t;
                } // Load B

                fp_regs[rt] = v; // 赋值给 u128 会自动将高位清零
                fp_dirty = true;
            }
            else
            {
                uint64_t v = 0;
                if (bytes == 8)
                {
                    if (__get_user(v, (uint64_t __user *)addr))
                        goto fault;
                }
                else if (bytes == 4)
                {
                    uint32_t t;
                    if (__get_user(t, (uint32_t __user *)addr))
                        goto fault;
                    v = t;
                }
                else if (bytes == 2)
                {
                    u16 t;
                    if (__get_user(t, (u16 __user *)addr))
                        goto fault;
                    v = t;
                }
                else
                {
                    u8 t;
                    if (__get_user(t, (u8 __user *)addr))
                        goto fault;
                    v = t;
                }

                if (opc >= 2)
                { // 整数 Load 的高位符号扩展 (如 LDRSB, LDRSH, LDRSW)
                    int b = (bytes << 3) - 1;
                    if (v & (1ULL << b))
                        v |= ~((1ULL << (b + 1)) - 1);
                }
                reg_write(regs, rt, v, (size == 3 || opc == 2));
            }
        }
        else
        { // Store 单一寄存器
            if (is_fp)
            {
                __uint128_t v = fp_regs[rt]; // 对于 Store，只读取不产生脏数据回写，节省开销
                if (bytes == 16)
                {
                    if (__put_user((uint64_t)v, (uint64_t __user *)addr) || __put_user((uint64_t)(v >> 64), (uint64_t __user *)(addr + 8)))
                        goto fault;
                }
                else if (bytes == 8)
                {
                    if (__put_user((uint64_t)v, (uint64_t __user *)addr))
                        goto fault;
                }
                else if (bytes == 4)
                {
                    if (__put_user((uint32_t)v, (uint32_t __user *)addr))
                        goto fault;
                }
                else if (bytes == 2)
                {
                    if (__put_user((u16)v, (u16 __user *)addr))
                        goto fault;
                }
                else
                {
                    if (__put_user((u8)v, (u8 __user *)addr))
                        goto fault;
                }
            }
            else
            {
                uint64_t v = reg_read(regs, rt);
                if (bytes == 8)
                {
                    if (__put_user(v, (uint64_t __user *)addr))
                        goto fault;
                }
                else if (bytes == 4)
                {
                    if (__put_user((uint32_t)v, (uint32_t __user *)addr))
                        goto fault;
                }
                else if (bytes == 2)
                {
                    if (__put_user((u16)v, (u16 __user *)addr))
                        goto fault;
                }
                else
                {
                    if (__put_user((u8)v, (u8 __user *)addr))
                        goto fault;
                }
            }
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

next_insn:
    // 如果遇到完全无法解析、或无需副作用的指令，静默跳过执行
    regs->pc += 4;
    return false;

fault:
    // 触发读写异常时，绝不可强制修改 PC，原样返回交由硬件机制介入
    return false;
}

#endif // EMULATE_INSN_H
