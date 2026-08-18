#include "arm64_emulate_internal.h"

/* ======================== 分支与系统类：屏障模板及完整执行流程 ======================== */

// instruction 唯一确定屏障指令，option 再选择对应的 4 位立即数变体。
// 每个 instruction/option 组合都映射到一个固定助记符模板。
// clang-format off
static inline bool emu_barrier_hw(enum arm64_instruction instruction, uint32_t option)
{
    switch (instruction)
    {
    case ARM64_INSN_CLREX:
        switch (option)
        {
        case 0:  emu_template_clrex_option_0(); break;
        case 1:  emu_template_clrex_option_1(); break;
        case 2:  emu_template_clrex_option_2(); break;
        case 3:  emu_template_clrex_option_3(); break;
        case 4:  emu_template_clrex_option_4(); break;
        case 5:  emu_template_clrex_option_5(); break;
        case 6:  emu_template_clrex_option_6(); break;
        case 7:  emu_template_clrex_option_7(); break;
        case 8:  emu_template_clrex_option_8(); break;
        case 9:  emu_template_clrex_option_9(); break;
        case 10: emu_template_clrex_option_10(); break;
        case 11: emu_template_clrex_option_11(); break;
        case 12: emu_template_clrex_option_12(); break;
        case 13: emu_template_clrex_option_13(); break;
        case 14: emu_template_clrex_option_14(); break;
        case 15: emu_template_clrex_option_15(); break;
        }
        return true;
    case ARM64_INSN_DSB:
        switch (option)
        {
        case 0:  emu_template_dsb_option_0(); break;
        case 1:  emu_template_dsb_option_1(); break;
        case 2:  emu_template_dsb_option_2(); break;
        case 3:  emu_template_dsb_option_3(); break;
        case 4:  emu_template_dsb_option_4(); break;
        case 5:  emu_template_dsb_option_5(); break;
        case 6:  emu_template_dsb_option_6(); break;
        case 7:  emu_template_dsb_option_7(); break;
        case 8:  emu_template_dsb_option_8(); break;
        case 9:  emu_template_dsb_option_9(); break;
        case 10: emu_template_dsb_option_10(); break;
        case 11: emu_template_dsb_option_11(); break;
        case 12: emu_template_dsb_option_12(); break;
        case 13: emu_template_dsb_option_13(); break;
        case 14: emu_template_dsb_option_14(); break;
        case 15: emu_template_dsb_option_15(); break;
        }
        return true;
    case ARM64_INSN_DMB:
        switch (option)
        {
        case 0:  emu_template_dmb_option_0(); break;
        case 1:  emu_template_dmb_option_1(); break;
        case 2:  emu_template_dmb_option_2(); break;
        case 3:  emu_template_dmb_option_3(); break;
        case 4:  emu_template_dmb_option_4(); break;
        case 5:  emu_template_dmb_option_5(); break;
        case 6:  emu_template_dmb_option_6(); break;
        case 7:  emu_template_dmb_option_7(); break;
        case 8:  emu_template_dmb_option_8(); break;
        case 9:  emu_template_dmb_option_9(); break;
        case 10: emu_template_dmb_option_10(); break;
        case 11: emu_template_dmb_option_11(); break;
        case 12: emu_template_dmb_option_12(); break;
        case 13: emu_template_dmb_option_13(); break;
        case 14: emu_template_dmb_option_14(); break;
        case 15: emu_template_dmb_option_15(); break;
        }
        return true;
    case ARM64_INSN_ISB:
        emu_template_isb();
        return true;
    default:
        return false;
    }
}
// clang-format on

/* ======================== 分支与系统类：缓存条目执行模板 ======================== */

/* 每个固定执行模板直接对应缓存条目中的 execute 函数地址。 */

static enum emu_insn_result emu_execute_branch_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = pc + entry->operand0;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_bl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->regs[30] = pc + 4;
    regs->pc = pc + entry->operand0;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_cbz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    uint64_t value = (uint32_t)reg_read(regs, entry->reg0);
    bool take = (value != 0) == (0 != 0);

    (void)fp_regs;
    regs->pc = take ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_cbz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    uint64_t value = reg_read(regs, entry->reg0);
    bool take = (value != 0) == (0 != 0);

    (void)fp_regs;
    regs->pc = take ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_cbnz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    uint64_t value = (uint32_t)reg_read(regs, entry->reg0);
    bool take = (value != 0) == (1 != 0);

    (void)fp_regs;
    regs->pc = take ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_cbnz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    uint64_t value = reg_read(regs, entry->reg0);
    bool take = (value != 0) == (1 != 0);

    (void)fp_regs;
    regs->pc = take ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_tbz(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    bool bit_set = ((reg_read(regs, entry->reg0) >> entry->reg1) & 1) != 0;

    (void)fp_regs;
    regs->pc = !bit_set ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_tbnz(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    bool bit_set = ((reg_read(regs, entry->reg0) >> entry->reg1) & 1) != 0;

    (void)fp_regs;
    regs->pc = bit_set ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 0) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 1) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 2) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 3) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 4) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 5) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 6) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 7) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 8) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 9) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 10) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 11) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 12) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 13) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 14) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_b_cond_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;

    (void)fp_regs;
    regs->pc = emu_cond_holds(emu_read_nzcv(regs), 15) ? pc + entry->operand0 : pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_nop(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_yield(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    emu_template_yield();
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 0)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 1)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 2)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 3)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 4)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 5)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 6)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 7)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 8)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 9)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 10)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 11)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 12)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 13)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 14)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_clrex_opt15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_CLREX, 15)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 0)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 1)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 2)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 3)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 4)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 5)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 6)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 7)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 8)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 9)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 10)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 11)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 12)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 13)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 14)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dsb_opt15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DSB, 15)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 0)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 1)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 2)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 3)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 4)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 5)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 6)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 7)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 8)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 9)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 10)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 11)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 12)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 13)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 14)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_dmb_opt15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_DMB, 15)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_isb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (!emu_barrier_hw(ARM64_INSN_ISB, 0)) return EMU_INSN_SKIP;
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_msr_register_s3_3_c4_c2_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value = reg_read(regs, entry->reg0);

    switch (ARM64_SYSREG_KEY(3, 3, 4, 2, 0))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        emu_write_nzcv(regs, value);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        fp_regs->fpcr = (uint32_t)value;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        fp_regs->fpsr = (uint32_t)value;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        arm64_write_tpidr_el0(value);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_msr_register_s3_3_c4_c4_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value = reg_read(regs, entry->reg0);

    switch (ARM64_SYSREG_KEY(3, 3, 4, 4, 0))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        emu_write_nzcv(regs, value);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        fp_regs->fpcr = (uint32_t)value;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        fp_regs->fpsr = (uint32_t)value;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        arm64_write_tpidr_el0(value);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_msr_register_s3_3_c4_c4_1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value = reg_read(regs, entry->reg0);

    switch (ARM64_SYSREG_KEY(3, 3, 4, 4, 1))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        emu_write_nzcv(regs, value);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        fp_regs->fpcr = (uint32_t)value;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        fp_regs->fpsr = (uint32_t)value;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        arm64_write_tpidr_el0(value);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_msr_register_s3_3_c13_c0_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value = reg_read(regs, entry->reg0);

    switch (ARM64_SYSREG_KEY(3, 3, 13, 0, 2))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        emu_write_nzcv(regs, value);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        fp_regs->fpcr = (uint32_t)value;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        fp_regs->fpsr = (uint32_t)value;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        arm64_write_tpidr_el0(value);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_mrs_s3_3_c4_c2_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value;

    switch (ARM64_SYSREG_KEY(3, 3, 4, 2, 0))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        value = emu_read_nzcv(regs);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        value = fp_regs->fpcr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        value = fp_regs->fpsr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        value = arm64_read_tpidr_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 3):
        value = arm64_read_tpidrro_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 14, 0, 2):
        value = arm64_read_cntvct_el0();
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg0, value, true);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_mrs_s3_3_c4_c4_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value;

    switch (ARM64_SYSREG_KEY(3, 3, 4, 4, 0))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        value = emu_read_nzcv(regs);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        value = fp_regs->fpcr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        value = fp_regs->fpsr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        value = arm64_read_tpidr_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 3):
        value = arm64_read_tpidrro_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 14, 0, 2):
        value = arm64_read_cntvct_el0();
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg0, value, true);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_mrs_s3_3_c4_c4_1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value;

    switch (ARM64_SYSREG_KEY(3, 3, 4, 4, 1))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        value = emu_read_nzcv(regs);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        value = fp_regs->fpcr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        value = fp_regs->fpsr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        value = arm64_read_tpidr_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 3):
        value = arm64_read_tpidrro_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 14, 0, 2):
        value = arm64_read_cntvct_el0();
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg0, value, true);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_mrs_s3_3_c13_c0_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value;

    switch (ARM64_SYSREG_KEY(3, 3, 13, 0, 2))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        value = emu_read_nzcv(regs);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        value = fp_regs->fpcr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        value = fp_regs->fpsr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        value = arm64_read_tpidr_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 3):
        value = arm64_read_tpidrro_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 14, 0, 2):
        value = arm64_read_cntvct_el0();
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg0, value, true);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_mrs_s3_3_c13_c0_3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value;

    switch (ARM64_SYSREG_KEY(3, 3, 13, 0, 3))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        value = emu_read_nzcv(regs);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        value = fp_regs->fpcr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        value = fp_regs->fpsr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        value = arm64_read_tpidr_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 3):
        value = arm64_read_tpidrro_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 14, 0, 2):
        value = arm64_read_cntvct_el0();
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg0, value, true);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_system_mrs_s3_3_c14_c0_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t value;

    switch (ARM64_SYSREG_KEY(3, 3, 14, 0, 2))
    {
    case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
        value = emu_read_nzcv(regs);
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
        value = fp_regs->fpcr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
        value = fp_regs->fpsr;
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
        value = arm64_read_tpidr_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 13, 0, 3):
        value = arm64_read_tpidrro_el0();
        break;
    case ARM64_SYSREG_KEY(3, 3, 14, 0, 2):
        value = arm64_read_cntvct_el0();
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg0, value, true);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_br(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t target = reg_read(regs, entry->reg0);

    (void)fp_regs;
    regs->pc = target;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_blr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    uint64_t target = reg_read(regs, entry->reg0);

    (void)fp_regs;
    regs->pc = target;
    regs->regs[30] = pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_branch_ret(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t target = reg_read(regs, entry->reg0);

    (void)fp_regs;
    regs->pc = target;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result (*emu_select_branch_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)

{

    switch (decoded->instruction)

    {

    case ARM64_INSN_B:

        return emu_execute_branch_b;

    case ARM64_INSN_BL:

        return emu_execute_branch_bl;

    case ARM64_INSN_CBZ:

        if (decoded->operand_width == 32) return emu_execute_branch_cbz_w32;

        if (decoded->operand_width == 64) return emu_execute_branch_cbz_w64;

        return NULL;

    case ARM64_INSN_CBNZ:

        if (decoded->operand_width == 32) return emu_execute_branch_cbnz_w32;

        if (decoded->operand_width == 64) return emu_execute_branch_cbnz_w64;

        return NULL;

    case ARM64_INSN_TBZ:

        return emu_execute_branch_tbz;

    case ARM64_INSN_TBNZ:

        return emu_execute_branch_tbnz;

    case ARM64_INSN_B_COND:

        if (decoded->condition == 0) return emu_execute_branch_b_cond_cond0;

        if (decoded->condition == 1) return emu_execute_branch_b_cond_cond1;

        if (decoded->condition == 2) return emu_execute_branch_b_cond_cond2;

        if (decoded->condition == 3) return emu_execute_branch_b_cond_cond3;

        if (decoded->condition == 4) return emu_execute_branch_b_cond_cond4;

        if (decoded->condition == 5) return emu_execute_branch_b_cond_cond5;

        if (decoded->condition == 6) return emu_execute_branch_b_cond_cond6;

        if (decoded->condition == 7) return emu_execute_branch_b_cond_cond7;

        if (decoded->condition == 8) return emu_execute_branch_b_cond_cond8;

        if (decoded->condition == 9) return emu_execute_branch_b_cond_cond9;

        if (decoded->condition == 10) return emu_execute_branch_b_cond_cond10;

        if (decoded->condition == 11) return emu_execute_branch_b_cond_cond11;

        if (decoded->condition == 12) return emu_execute_branch_b_cond_cond12;

        if (decoded->condition == 13) return emu_execute_branch_b_cond_cond13;

        if (decoded->condition == 14) return emu_execute_branch_b_cond_cond14;

        if (decoded->condition == 15) return emu_execute_branch_b_cond_cond15;

        return NULL;

    case ARM64_INSN_NOP:

        return emu_execute_system_nop;

    case ARM64_INSN_YIELD:

        return emu_execute_system_yield;

    case ARM64_INSN_CLREX:

        if (decoded->option == 0) return emu_execute_system_clrex_opt0;

        if (decoded->option == 1) return emu_execute_system_clrex_opt1;

        if (decoded->option == 2) return emu_execute_system_clrex_opt2;

        if (decoded->option == 3) return emu_execute_system_clrex_opt3;

        if (decoded->option == 4) return emu_execute_system_clrex_opt4;

        if (decoded->option == 5) return emu_execute_system_clrex_opt5;

        if (decoded->option == 6) return emu_execute_system_clrex_opt6;

        if (decoded->option == 7) return emu_execute_system_clrex_opt7;

        if (decoded->option == 8) return emu_execute_system_clrex_opt8;

        if (decoded->option == 9) return emu_execute_system_clrex_opt9;

        if (decoded->option == 10) return emu_execute_system_clrex_opt10;

        if (decoded->option == 11) return emu_execute_system_clrex_opt11;

        if (decoded->option == 12) return emu_execute_system_clrex_opt12;

        if (decoded->option == 13) return emu_execute_system_clrex_opt13;

        if (decoded->option == 14) return emu_execute_system_clrex_opt14;

        if (decoded->option == 15) return emu_execute_system_clrex_opt15;

        return NULL;

    case ARM64_INSN_DSB:

        if (decoded->option == 0) return emu_execute_system_dsb_opt0;

        if (decoded->option == 1) return emu_execute_system_dsb_opt1;

        if (decoded->option == 2) return emu_execute_system_dsb_opt2;

        if (decoded->option == 3) return emu_execute_system_dsb_opt3;

        if (decoded->option == 4) return emu_execute_system_dsb_opt4;

        if (decoded->option == 5) return emu_execute_system_dsb_opt5;

        if (decoded->option == 6) return emu_execute_system_dsb_opt6;

        if (decoded->option == 7) return emu_execute_system_dsb_opt7;

        if (decoded->option == 8) return emu_execute_system_dsb_opt8;

        if (decoded->option == 9) return emu_execute_system_dsb_opt9;

        if (decoded->option == 10) return emu_execute_system_dsb_opt10;

        if (decoded->option == 11) return emu_execute_system_dsb_opt11;

        if (decoded->option == 12) return emu_execute_system_dsb_opt12;

        if (decoded->option == 13) return emu_execute_system_dsb_opt13;

        if (decoded->option == 14) return emu_execute_system_dsb_opt14;

        if (decoded->option == 15) return emu_execute_system_dsb_opt15;

        return NULL;

    case ARM64_INSN_DMB:

        if (decoded->option == 0) return emu_execute_system_dmb_opt0;

        if (decoded->option == 1) return emu_execute_system_dmb_opt1;

        if (decoded->option == 2) return emu_execute_system_dmb_opt2;

        if (decoded->option == 3) return emu_execute_system_dmb_opt3;

        if (decoded->option == 4) return emu_execute_system_dmb_opt4;

        if (decoded->option == 5) return emu_execute_system_dmb_opt5;

        if (decoded->option == 6) return emu_execute_system_dmb_opt6;

        if (decoded->option == 7) return emu_execute_system_dmb_opt7;

        if (decoded->option == 8) return emu_execute_system_dmb_opt8;

        if (decoded->option == 9) return emu_execute_system_dmb_opt9;

        if (decoded->option == 10) return emu_execute_system_dmb_opt10;

        if (decoded->option == 11) return emu_execute_system_dmb_opt11;

        if (decoded->option == 12) return emu_execute_system_dmb_opt12;

        if (decoded->option == 13) return emu_execute_system_dmb_opt13;

        if (decoded->option == 14) return emu_execute_system_dmb_opt14;

        if (decoded->option == 15) return emu_execute_system_dmb_opt15;

        return NULL;

    case ARM64_INSN_ISB:

        return emu_execute_system_isb;

    case ARM64_INSN_MSR_REGISTER:

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 2, 0)) return emu_execute_system_msr_register_s3_3_c4_c2_0;

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 4, 0)) return emu_execute_system_msr_register_s3_3_c4_c4_0;

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 4, 1)) return emu_execute_system_msr_register_s3_3_c4_c4_1;

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 13, 0, 2)) return emu_execute_system_msr_register_s3_3_c13_c0_2;

        return NULL;

    case ARM64_INSN_MRS:

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 2, 0)) return emu_execute_system_mrs_s3_3_c4_c2_0;

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 4, 0)) return emu_execute_system_mrs_s3_3_c4_c4_0;

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 4, 1)) return emu_execute_system_mrs_s3_3_c4_c4_1;

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 13, 0, 2)) return emu_execute_system_mrs_s3_3_c13_c0_2;

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 13, 0, 3)) return emu_execute_system_mrs_s3_3_c13_c0_3;

        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 14, 0, 2)) return emu_execute_system_mrs_s3_3_c14_c0_2;

        return NULL;

    case ARM64_INSN_BR:

        return emu_execute_branch_br;

    case ARM64_INSN_BLR:

        return emu_execute_branch_blr;

    case ARM64_INSN_RET:

        return emu_execute_branch_ret;

    default:

        return NULL;
    }
}

/* ======================== 分支与系统类：解码结果构建缓存条目 ======================== */

bool emu_build_branch_executor(const struct arm64_decoded_instruction *decoded, struct arm64_executor_entry *entry)
{
    entry->execute = emu_select_branch_executor(decoded);
    if (!entry->execute) return false;

    entry->operand0 = decoded->offset;
    if (decoded->instruction == ARM64_INSN_MSR_REGISTER || decoded->instruction == ARM64_INSN_MRS) entry->operand0 = decoded->sysreg;
    entry->reg0 = decoded->instruction == ARM64_INSN_BR || decoded->instruction == ARM64_INSN_BLR || decoded->instruction == ARM64_INSN_RET ? decoded->rn : decoded->rt;
    if (decoded->instruction == ARM64_INSN_B_COND) entry->reg0 = decoded->condition;
    entry->reg1 = decoded->test_bit;
    entry->option0 = decoded->option;
    return true;
}
