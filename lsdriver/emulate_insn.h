#ifndef EMULATE_INSN_H
#define EMULATE_INSN_H

#include <linux/uaccess.h>
#include <asm/ptrace.h>
#include <asm/insn.h>

/* =========================================================================
  ARM64 指令模拟器(主要用于处理数据访问断点)

 当触发硬件数据断点 或指令断点 时，
 用于在软件层面直接计算出下一条 PC 或模拟内存读写，从而无需依赖


断点方式1：常规 ptrace 与 BRK 软断点
    实现方式： 调试器通过 ptrace 附加，向内存写入 0xD4200000 (BRK指令)。
    可能被检测点：
    特征文件： /proc/self/status 里的 TracerPid 不为 0。
    内存完整性： 任何最基本的 CRC/Hash 内存校验都会发现代码段被篡改。
    信号拦截： 进程可以注册 SIGTRAP 信号处理器，发现异常。
    隐蔽性：0 / 10
 断点方式2： 硬件断点 (BRP/WRP) + 硬件单步 (SPSR.SS)
    这是目前大部分所谓“无痕调试器”或内核 Hook 采用的方法。
    实现方式：
        x86 的实现： 设置 EFLAGS 寄存器的 TF (Trap Flag) 标志位。CPU 执行一条指令后，触发 INT 1 (#DB) 异常。
        ARM64 的实现：设置 MDSCR_EL1.SS (Single Step 开启) 和 SPSR_EL1.SS (PSTATE 的单步状态位)。CPU 执行完下一条指令后，硬件会主动触发异常（ESR_EL1 的 Exception Class 为 0x32）。
    可能被检测点：
    调试寄存器暴露： 是用户态程序，通过调用特定的系统 API（如 ptrace 获取自己的 NT_ARM_HW_BREAK 寄存器集）也能发现，如果反作弊引擎在内核有驱动，它可以直接读取 MDSCR_EL1 或 DBGBVR 寄存器，瞬间发现有断点。
    线程上下文暴露： 当程序被硬件单步时，反作弊线程如果刚好获取目标线程的上下文，会发现其 PSTATE 中的 SS 标志位被莫名其妙置起（或者发现某些用于暂存断点的内核变量被修改）。
    隐蔽性：4 / 10

  支持的指令
  分支跳转：B, BL, BR, BLR, RET, B.cond, CBZ, CBNZ, TBZ, TBNZ
  常规访存：LDR, STR, LDP, STP, LDRB/H/SW (支持 Pre/Post-index 及寄存器偏移)
  地址计算：ADR, ADRP

  不支持的指令 (遇到会跳过该指令, PC = PC + 4)
  ALU 计算指令：ADD, SUB, AND, LSL 等。
  SIMD/FP 访存：LDR Qn, STP Dn 等 (避免内核浮点异常)。
  原子/独占指令：LDXR, STXR, CAS, SWP 等 (必须拒绝，否则破坏硬件一致性导致死锁)。


  =========================================================================

   优化点：
   使用 __get_user/__put_user 替代 get_user/put_user (移除 access_ok)。
   移除所有内核打印，避免高频触发时的性能崩溃。
   辅助函数全内联，优化寄存器压栈开销。
   引入指令预过滤机制，O(1) 时间排除非模拟指令。
  */

// 内联寄存器操作
static __always_inline u64 reg_read(struct pt_regs *regs, u32 n)
{
    return (n == 31) ? 0ULL : regs->regs[n];
}

static __always_inline void reg_write(struct pt_regs *regs, u32 n, u64 val, bool sf)
{
    if (n != 31)
        regs->regs[n] = sf ? (u64)val : (u64)(u32)val;
}

static __always_inline u64 addr_reg_read(struct pt_regs *regs, u32 n)
{
    return (n == 31) ? regs->sp : regs->regs[n];
}

static __always_inline void addr_reg_write(struct pt_regs *regs, u32 n, u64 val)
{
    if (n == 31)
        regs->sp = val;
    else
        regs->regs[n] = val;
}

// 快速条件状态计算
static __always_inline bool eval_cond_fast(u64 pstate, u32 cond)
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

// 核心模拟执行函数
static __always_inline bool emulate_insn(struct pt_regs *regs)
{
    u32 insn;
    u64 pc = regs->pc;

    if (unlikely(__get_user(insn, (u32 __user *)pc)))
    {
        regs->pc += 4;
        return false;
    }

    // 预过滤：iclass [28:25]
    u32 iclass = (insn >> 25) & 0xF;

    // --- 第一部分：跳转指令 (Class: 101x) ---
    if ((iclass & 0xE) == 0xA)
    {
        u32 op_branch = insn & 0xFC000000;
        if (op_branch == 0x14000000)
        { // B <imm26>
            regs->pc = pc + sign_extend64((s64)(insn & 0x3FFFFFF) << 2, 27);
            return true;
        }
        if (op_branch == 0x94000000)
        { // BL <imm26>
            regs->regs[30] = pc + 4;
            regs->pc = pc + sign_extend64((s64)(insn & 0x3FFFFFF) << 2, 27);
            return true;
        }
        if ((insn & 0xFE1F03E0) == 0xD61F0000)
        { // BR / BLR / RET
            u32 rn = (insn >> 5) & 0x1F, opc = (insn >> 21) & 0x3;
            if (opc == 1)
                regs->regs[30] = pc + 4; // BLR
            if (opc <= 2)
            {
                regs->pc = regs->regs[rn];
                return true;
            }
        }
        if ((insn & 0xFF000010) == 0x54000000)
        { // B.cond
            s64 offset = sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);
            regs->pc = eval_cond_fast(regs->pstate, insn & 0xF) ? (pc + offset) : (pc + 4);
            return true;
        }
        if ((insn & 0x7E000000) == 0x34000000)
        { // CBZ / CBNZ
            u32 rt = insn & 0x1F;
            u64 val = ((insn >> 31) & 1) ? reg_read(regs, rt) : (u32)reg_read(regs, rt);
            bool jump = ((insn >> 24) & 1) ? (val != 0) : (val == 0);
            regs->pc = jump ? (pc + sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20)) : (pc + 4);
            return true;
        }
        if ((insn & 0x7E000000) == 0x36000000)
        { // TBZ / TBNZ
            u32 rt = insn & 0x1F, pos = ((insn >> 31) & 1) << 5 | ((insn >> 19) & 0x1F);
            bool bit_set = (reg_read(regs, rt) >> pos) & 1;
            bool jump = (bit_set == ((insn >> 24) & 1));
            regs->pc = jump ? (pc + sign_extend64((s64)((insn >> 5) & 0x3FFF) << 2, 15)) : (pc + 4);
            return true;
        }
        goto next_insn;
    }

    // --- 第二部分：地址计算 ADR / ADRP (Class 100x / bits 28:24 == 10000) ---
    // 这类指令决定了程序是否能正确访问全局变量/字符串
    if ((insn & 0x1F000000) == 0x10000000)
    {
        u32 rd = insn & 0x1F;
        s64 imm = (s64)((insn << 5) & 0x001FFFF8) | ((insn >> 29) & 0x3);
        imm = sign_extend64(imm, 20);
        if (insn & 0x80000000)
        { // ADRP: 基地址是当前页 4KB 对齐后的地址
            regs->regs[rd] = (pc & ~0xFFFULL) + (imm << 12);
        }
        else
        { // ADR
            regs->regs[rd] = pc + imm;
        }
        regs->pc += 4;
        return true;
    }

    // --- 第三部分：Load/Store 指令 (Class: x1x0) ---
    if ((iclass & 0x5) == 0x4)
    {
        // 排除 FP/SIMD 和 原子独占指令
        if ((insn & 0x04000000) || (insn & 0x3F000000) == 0x08000000)
            goto next_insn;

        u32 size = (insn >> 30) & 0x3;

        // LDR literal (PC-relative)
        if ((insn & 0x3B000000) == 0x18000000)
        {
            u32 rt = insn & 0x1F;
            u64 addr = pc + sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);
            if (size == 2 && ((insn >> 22) & 1))
            { // LDRSW
                u32 t;
                if (__get_user(t, (u32 __user *)addr))
                    goto fault;
                reg_write(regs, rt, (s64)(s32)t, true);
            }
            else
            {
                if (size == 3)
                {
                    u64 t;
                    if (__get_user(t, (u64 __user *)addr))
                        goto fault;
                    reg_write(regs, rt, t, true);
                }
                else
                {
                    u32 t;
                    if (__get_user(t, (u32 __user *)addr))
                        goto fault;
                    reg_write(regs, rt, t, false);
                }
            }
            regs->pc += 4;
            return true;
        }

        // LDP / STP (Load/Store pair)
        if ((insn & 0x3A000000) == 0x28000000)
        {
            u32 opc_pair = (insn >> 30) & 0x3, l = (insn >> 22) & 1, idx = (insn >> 23) & 0x3;
            u32 rn = (insn >> 5) & 0x1F, rt = insn & 0x1F, rt2 = (insn >> 10) & 0x1F;
            s64 off = sign_extend64((s64)((insn >> 15) & 0x7F), 6) * ((opc_pair == 2) ? 8 : 4);
            u64 base = addr_reg_read(regs, rn), addr = (idx == 1) ? base : (base + off);
            if (unlikely(idx == 0))
                goto fault;

            if (l)
            { // Load
                u64 v1, v2;
                if (opc_pair == 2)
                {
                    if (__get_user(v1, (u64 __user *)addr) || __get_user(v2, (u64 __user *)(addr + 8)))
                        goto fault;
                }
                else
                {
                    u32 t1, t2;
                    if (__get_user(t1, (u32 __user *)addr) || __get_user(t2, (u32 __user *)(addr + 4)))
                        goto fault;
                    v1 = (opc_pair == 1) ? (u64)(s64)(s32)t1 : t1;
                    v2 = (opc_pair == 1) ? (u64)(s64)(s32)t2 : t2;
                }
                if (idx & 1)
                    addr_reg_write(regs, rn, base + off);
                reg_write(regs, rt, v1, (opc_pair >= 1));
                reg_write(regs, rt2, v2, (opc_pair >= 1));
            }
            else
            { // Store
                if (opc_pair == 2)
                {
                    if (__put_user(reg_read(regs, rt), (u64 __user *)addr) || __put_user(reg_read(regs, rt2), (u64 __user *)(addr + 8)))
                        goto fault;
                }
                else
                {
                    if (__put_user((u32)reg_read(regs, rt), (u32 __user *)addr) || __put_user((u32)reg_read(regs, rt2), (u32 __user *)(addr + 4)))
                        goto fault;
                }
                if (idx & 1)
                    addr_reg_write(regs, rn, base + off);
            }
            regs->pc += 4;
            return true;
        }

        // LDR / STR (Single Register)
        if ((insn & 0x3A000000) == 0x38000000)
        {
            u32 rn = (insn >> 5) & 0x1F, rt = insn & 0x1F, opc = (insn >> 22) & 0x3;
            u64 base = addr_reg_read(regs, rn), addr = base;
            int bytes = (1 << size);
            if ((insn >> 24) & 1)
                addr = base + (((insn >> 10) & 0xFFF) << size);
            else
            {
                u32 idx = (insn >> 10) & 0x3;
                s64 imm9 = sign_extend64((s64)((insn >> 12) & 0x1FF), 8);
                if (idx == 0)
                    addr = base + imm9;
                else if (idx == 1 || idx == 3)
                    addr = (idx == 3) ? (base + imm9) : base;
                else if (idx == 2 && ((insn >> 21) & 1))
                {
                    u32 rm = (insn >> 16) & 0x1F, opt = (insn >> 13) & 0x7;
                    s64 ext = reg_read(regs, rm);
                    if (opt == 2 || opt == 6)
                        ext = (s64)(s32)ext;
                    addr = base + ((((insn >> 12) & 1) ? (ext << size) : ext));
                }
                else
                    goto fault;
                if (idx & 1)
                    addr_reg_write(regs, rn, base + imm9);
            }
            if (opc != 0)
            { // Load
                u64 v = 0;
                if (bytes == 8)
                {
                    if (__get_user(v, (u64 __user *)addr))
                        goto fault;
                }
                else if (bytes == 4)
                {
                    u32 t;
                    if (__get_user(t, (u32 __user *)addr))
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
                {
                    int b = (bytes << 3) - 1;
                    if (v & (1ULL << b))
                        v |= ~((1ULL << (b + 1)) - 1);
                }
                reg_write(regs, rt, v, (size == 3 || opc == 2));
            }
            else
            { // Store
                u64 v = reg_read(regs, rt);
                if (bytes == 8)
                {
                    if (__put_user(v, (u64 __user *)addr))
                        goto fault;
                }
                else if (bytes == 4)
                {
                    if (__put_user((u32)v, (u32 __user *)addr))
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
            regs->pc += 4;
            return true;
        }
    }

next_insn:
    regs->pc += 4;
    return false;
fault:
    return false;
}

#endif // EMULATE_INSN_H