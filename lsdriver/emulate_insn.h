
// ============================================================
// ARM64 指令模拟器
//
// 覆盖范围：
//   1. 跳转/调用类：b / bl / br / blr / ret / b.cond / cbz / cbnz / tbz / tbnz
//   2. Load/Store 类（数据断点唯一可能触发的指令大类）：
//      - LDR/STR  立即数偏移（无符号/符号，pre/post-index）
//      - LDR/STR  寄存器偏移
//      - LDR      PC相对（literal）
//      - LDP/STP  寄存器对（signed offset / pre / post）
//      - LDRB/STRB / LDRH/STRH（字节/半字）
//      - LDRSB/LDRSH/LDRSW（符号扩展 load）
//      - LDUR/STUR（无符号偏移，unscaled）
//
// 警告：
// 如果硬件断点下在了普通的 ALU（如 add, sub）或 SIMD 指令上，
// 本模拟器不支持这些指令的计算，会直接跳过（PC+4）
// 因此，此模拟器强烈建议仅用于【数据观察点】或【跳转指令断点】。
// ============================================================

#ifndef EMULATE_INSN_H
#define EMULATE_INSN_H

#include <linux/uaccess.h>
#include <asm/ptrace.h>
#include <asm/insn.h>

// ---- 辅助宏 ------------------------------------------------
#define REG_READ(regs, n) ((n) == 31 ? 0ULL : (regs)->regs[(n)])

#define REG_WRITE(regs, n, val, sf)                     \
    do                                                  \
    {                                                   \
        if ((n) != 31)                                  \
            (regs)->regs[(n)] = (sf) ? (u64)(val)       \
                                     : (u64)(u32)(val); \
    } while (0)

#define ADDR_REG(regs, n) ((n) == 31 ? (regs)->sp : (regs)->regs[(n)])

#define ADDR_REG_WRITE(regs, n, val)        \
    do                                      \
    {                                       \
        if ((n) == 31)                      \
            (regs)->sp = (u64)(val);        \
        else                                \
            (regs)->regs[(n)] = (u64)(val); \
    } while (0)

#define PSTATE_N(pstate) (((pstate) >> 31) & 1)
#define PSTATE_Z(pstate) (((pstate) >> 30) & 1)
#define PSTATE_C(pstate) (((pstate) >> 29) & 1)
#define PSTATE_V(pstate) (((pstate) >> 28) & 1)

static bool eval_cond(u64 pstate, u32 cond)
{
    bool n = PSTATE_N(pstate), z = PSTATE_Z(pstate);
    bool c = PSTATE_C(pstate), v = PSTATE_V(pstate), result;
    switch (cond >> 1)
    {
    case 0:
        result = z;
        break;
    case 1:
        result = c;
        break;
    case 2:
        result = n;
        break;
    case 3:
        result = v;
        break;
    case 4:
        result = c && !z;
        break;
    case 5:
        result = (n == v);
        break;
    case 6:
        result = (n == v) && !z;
        break;
    case 7:
        result = true;
        break;
    default:
        result = false;
        break;
    }
    if ((cond & 1) && (cond != 0xf))
        result = !result;
    return result;
}

// 使用 get_user() 和 put_user()用于真实模拟 Store 指令
static inline int user_read_u8(u64 addr, u8 *val) { return get_user(*val, (u8 __user *)(uintptr_t)addr); }
static inline int user_read_u16(u64 addr, u16 *val) { return get_user(*val, (u16 __user *)(uintptr_t)addr); }
static inline int user_read_u32(u64 addr, u32 *val) { return get_user(*val, (u32 __user *)(uintptr_t)addr); }
static inline int user_read_u64(u64 addr, u64 *val) { return get_user(*val, (u64 __user *)(uintptr_t)addr); }

static inline int user_write_u8(u64 addr, u8 val) { return put_user(val, (u8 __user *)(uintptr_t)addr); }
static inline int user_write_u16(u64 addr, u16 val) { return put_user(val, (u16 __user *)(uintptr_t)addr); }
static inline int user_write_u32(u64 addr, u32 val) { return put_user(val, (u32 __user *)(uintptr_t)addr); }
static inline int user_write_u64(u64 addr, u64 val) { return put_user(val, (u64 __user *)(uintptr_t)addr); }

static inline int read_user_insn(u64 pc, u32 *insn) { return get_user(*insn, (u32 __user *)(uintptr_t)pc); }

// ============================================================
// 模拟执行 ARM64 指令
// ============================================================
static bool emulate_insn(struct pt_regs *regs)
{
    u32 insn;
    u64 pc = regs->pc;

    if (read_user_insn(pc, &insn) != 0)
    {
        regs->pc += 4;
        return false;
    }

    // --- 第一部分：跳转 / 调用指令 ---
    if ((insn & 0xFC000000) == 0x14000000)
    { // b <imm26>
        regs->pc = pc + sign_extend64((s64)(insn & 0x3FFFFFF) << 2, 27);
        return true;
    }
    if ((insn & 0xFC000000) == 0x94000000)
    { // bl <imm26>
        regs->regs[30] = pc + 4;
        regs->pc = pc + sign_extend64((s64)(insn & 0x3FFFFFF) << 2, 27);
        return true;
    }
    if ((insn & 0xFE1F03E0) == 0xD61F0000)
    { // br / blr / ret
        u32 rn = (insn >> 5) & 0x1F, opc = (insn >> 21) & 0x3;
        if (opc == 1)
            regs->regs[30] = pc + 4; // blr
        if (opc <= 2)
        {
            regs->pc = regs->regs[rn];
            return true;
        }
    }
    if ((insn & 0xFF000010) == 0x54000000)
    { // b.cond
        s64 offset = sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);
        regs->pc = eval_cond(regs->pstate, insn & 0xF) ? (pc + offset) : (pc + 4);
        return true;
    }
    if ((insn & 0x7E000000) == 0x34000000)
    { // cbz / cbnz
        u32 rt = insn & 0x1F;
        u64 val = ((insn >> 31) & 1) ? REG_READ(regs, rt) : (u32)REG_READ(regs, rt);
        bool jump = ((insn >> 24) & 1) ? (val != 0) : (val == 0);
        regs->pc = jump ? (pc + sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20)) : (pc + 4);
        return true;
    }
    if ((insn & 0x7E000000) == 0x36000000)
    { // tbz / tbnz
        u32 rt = insn & 0x1F, bit_pos = ((insn >> 31) & 1) << 5 | ((insn >> 19) & 0x1F);
        bool bit_set = (REG_READ(regs, rt) >> bit_pos) & 1;
        bool jump = ((insn >> 24) & 1) ? bit_set : !bit_set;
        regs->pc = jump ? (pc + sign_extend64((s64)((insn >> 5) & 0x3FFF) << 2, 15)) : (pc + 4);
        return true;
    }

    // --- 第二部分：Load/Store 指令 ---
    if (!((insn & 0x0A000000) == 0x08000000))
        goto not_load_store;

    {
        u32 size = (insn >> 30) & 0x3;
        if ((insn >> 26) & 1)
            goto not_load_store; // 不支持 FP/SIMD

        // ---- LDR literal（PC 相对寻址）----
        if ((insn & 0x3B000000) == 0x18000000)
        {
            u32 rt = insn & 0x1F;
            u64 addr = pc + sign_extend64((s64)((insn >> 5) & 0x7FFFF) << 2, 20);
            if (((insn >> 30) & 0x3) == 2)
            { // LDRSW
                u32 t;
                if (user_read_u32(addr, &t))
                    goto fault;
                REG_WRITE(regs, rt, (s64)(s32)t, 1);
            }
            else if ((insn >> 30) & 1)
            { // LDR X
                u64 t;
                if (user_read_u64(addr, &t))
                    goto fault;
                REG_WRITE(regs, rt, t, 1);
            }
            else
            { // LDR W
                u32 t;
                if (user_read_u32(addr, &t))
                    goto fault;
                REG_WRITE(regs, rt, t, 0);
            }
            regs->pc += 4;
            return true;
        }

        // ---- Load/Store pair (LDP / STP) ----
        if ((insn & 0x3A000000) == 0x28000000)
        {
            u32 opc_pair = (insn >> 30) & 0x3, l = (insn >> 22) & 1;
            u32 idx = (insn >> 23) & 0x3; // ARMv8: 1=post, 2=signed, 3=pre
            u32 rn = (insn >> 5) & 0x1F, rt = insn & 0x1F, rt2 = (insn >> 10) & 0x1F;

            int pair_bytes = (opc_pair == 2) ? 8 : 4;
            s64 offset = sign_extend64((s64)((insn >> 15) & 0x7F), 6) * pair_bytes;
            u64 base = ADDR_REG(regs, rn), addr;

            if (idx == 1)
                addr = base; // Post-index
            else if (idx == 2 || idx == 3)
                addr = base + offset; // Signed offset or Pre-index
            else
                goto fault; // idx=0 是未定义指令

            if (l)
            { // Load
                if (opc_pair == 2)
                { // 64-bit
                    u64 v1, v2;
                    if (user_read_u64(addr, &v1) || user_read_u64(addr + 8, &v2))
                        goto fault;
                    REG_WRITE(regs, rt, v1, 1);
                    REG_WRITE(regs, rt2, v2, 1);
                }
                else if (opc_pair == 1)
                { // LDPSW
                    u32 v1, v2;
                    if (user_read_u32(addr, &v1) || user_read_u32(addr + 4, &v2))
                        goto fault;
                    REG_WRITE(regs, rt, (s64)(s32)v1, 1);
                    REG_WRITE(regs, rt2, (s64)(s32)v2, 1);
                }
                else
                { // 32-bit
                    u32 v1, v2;
                    if (user_read_u32(addr, &v1) || user_read_u32(addr + 4, &v2))
                        goto fault;
                    REG_WRITE(regs, rt, v1, 0);
                    REG_WRITE(regs, rt2, v2, 0);
                }
            }
            else
            { // Store
                if (opc_pair == 2)
                {
                    if (user_write_u64(addr, REG_READ(regs, rt)) || user_write_u64(addr + 8, REG_READ(regs, rt2)))
                        goto fault;
                }
                else
                {
                    if (user_write_u32(addr, (u32)REG_READ(regs, rt)) || user_write_u32(addr + 4, (u32)REG_READ(regs, rt2)))
                        goto fault;
                }
            }

            // 更新基址
            if (idx == 1 || idx == 3)
                ADDR_REG_WRITE(regs, rn, base + offset);
            regs->pc += 4;
            return true;
        }

        // ---- 普通 Load/Store 单个寄存器 (LDR/STR) ----
        if ((insn & 0x3E000000) == 0x38000000 || (insn & 0x3E000000) == 0x3A000000 ||
            (insn & 0x3E000000) == 0x3C000000 || (insn & 0x3E000000) == 0x3E000000 ||
            (insn & 0x3F000000) == 0x39000000 || (insn & 0x3F000000) == 0x79000000 ||
            (insn & 0x3F000000) == 0xB9000000 || (insn & 0x3F000000) == 0xF9000000)
        {
            u32 rn = (insn >> 5) & 0x1F, rt = insn & 0x1F;
            u32 opc = (insn >> 22) & 0x3;

            // opc 不为 0 才是 Load！
            bool is_load = (opc != 0);
            bool sign_ext = (opc == 2) || (opc == 3);
            bool dest_64 = (size == 3) || (opc == 2);
            int access_bytes = (1 << size);

            u64 base = ADDR_REG(regs, rn), addr;

            // 通过 Bit 24 和 Bits 11:10 精确判断寻址模式
            bool is_unsigned_offset = ((insn >> 24) & 1) == 1;

            if (is_unsigned_offset)
            { // Unsigned offset
                addr = base + ((insn >> 10) & 0xFFF) * (u64)access_bytes;
            }
            else
            {
                u32 idx = (insn >> 10) & 0x3;
                if (idx == 0)
                { // LDUR/STUR (unscaled)
                    addr = base + sign_extend64((s64)((insn >> 12) & 0x1FF), 8);
                }
                else if (idx == 1)
                { // Post-index
                    addr = base;
                    ADDR_REG_WRITE(regs, rn, base + sign_extend64((s64)((insn >> 12) & 0x1FF), 8));
                }
                else if (idx == 3)
                { // Pre-index
                    addr = base + sign_extend64((s64)((insn >> 12) & 0x1FF), 8);
                    ADDR_REG_WRITE(regs, rn, addr);
                }
                else if (idx == 2)
                { // Register offset
                    if (((insn >> 21) & 1) == 1)
                    {
                        u32 rm = (insn >> 16) & 0x1F, option = (insn >> 13) & 0x7;
                        s64 ext_val;
                        u64 rm_val = REG_READ(regs, rm);
                        switch (option)
                        {
                        case 2:
                            ext_val = (s64)(u32)rm_val;
                            break;
                        case 3:
                            ext_val = (s64)rm_val;
                            break;
                        case 6:
                            ext_val = (s64)(s32)rm_val;
                            break;
                        case 7:
                            ext_val = (s64)rm_val;
                            break;
                        default:
                            ext_val = (s64)rm_val;
                            break;
                        }
                        if ((insn >> 12) & 1)
                            ext_val <<= size; // Shift
                        addr = base + (u64)ext_val;
                    }
                    else
                        goto fault;
                }
            }

            if (is_load)
            {
                u64 val = 0;
                switch (access_bytes)
                {
                case 1:
                {
                    u8 t;
                    if (user_read_u8(addr, &t))
                        goto fault;
                    val = t;
                }
                break;
                case 2:
                {
                    u16 t;
                    if (user_read_u16(addr, &t))
                        goto fault;
                    val = t;
                }
                break;
                case 4:
                {
                    u32 t;
                    if (user_read_u32(addr, &t))
                        goto fault;
                    val = t;
                }
                break;
                case 8:
                {
                    u64 t;
                    if (user_read_u64(addr, &t))
                        goto fault;
                    val = t;
                }
                break;
                }
                if (sign_ext)
                { // 符号扩展
                    int bits = access_bytes * 8 - 1;
                    if (val & (1ULL << bits))
                        val |= ~((1ULL << (bits + 1)) - 1);
                }
                REG_WRITE(regs, rt, val, dest_64 ? 1 : 0);
            }
            else
            { // Store
                u64 val = REG_READ(regs, rt);
                switch (access_bytes)
                {
                case 1:
                    if (user_write_u8(addr, (u8)val))
                        goto fault;
                    break;
                case 2:
                    if (user_write_u16(addr, (u16)val))
                        goto fault;
                    break;
                case 4:
                    if (user_write_u32(addr, (u32)val))
                        goto fault;
                    break;
                case 8:
                    if (user_write_u64(addr, val))
                        goto fault;
                    break;
                }
            }
            regs->pc += 4;
            return true;
        }
    }

not_load_store:
    // ALU / FP / SIMD 等指令不支持模拟。
    // 如果对普通加法、移位等指令下了执行断点，会走到这里强行跳过。
    pr_debug("hwbp: 不支持模拟 insn=0x%08x pc=0x%llx，退化为 pc+4\n", insn, pc);
    regs->pc += 4;
    return false;
fault:
    // 访存失败，放弃模拟，保留原PC
    pr_debug("hwbp: 模拟访存失败 insn=0x%08x pc=0x%llx\n", insn, pc);
    return false;
}
#endif // EMULATE_INSN_H