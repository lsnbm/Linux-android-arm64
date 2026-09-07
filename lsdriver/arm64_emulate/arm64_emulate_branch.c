// clang-format off
#include "emulate_inst.h"
/* ======================== 分支与系统类：缓存条目执行模板 ======================== */
/* 每个固定执行模板直接对应缓存条目中的 execute 函数地址。 */
static inline enum emu_inst_result emu_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = regs->pc + entry->decoded.offset;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    write_gpr_or_zr(regs, 30, regs->pc + 4, true);
    regs->pc = regs->pc + entry->decoded.offset;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_cbz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = (((uint32_t)read_gpr_or_zr(regs, entry->decoded.rt) != 0) == (0 != 0)) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_cbz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = ((read_gpr_or_zr(regs, entry->decoded.rt) != 0) == (0 != 0)) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_cbnz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = (((uint32_t)read_gpr_or_zr(regs, entry->decoded.rt) != 0) == (1 != 0)) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_cbnz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = ((read_gpr_or_zr(regs, entry->decoded.rt) != 0) == (1 != 0)) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_tbz(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = !(((read_gpr_or_zr(regs, entry->decoded.rt) >> entry->decoded.immediate) & 1) != 0) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_tbnz(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = (((read_gpr_or_zr(regs, entry->decoded.rt) >> entry->decoded.immediate) & 1) != 0) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 0) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 1) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 2) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 3) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 4) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 5) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 6) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 7) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 8) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 9) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 10) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 11) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 12) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 13) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 14) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_b_cond_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = cond_holds(read_nzcv(regs), 15) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_nop(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_yield(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    yield_hw(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_0(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_1(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_2(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_3(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_4(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_5(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_6(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_7(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_8(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_9(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_10(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_11(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_12(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_13(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_14(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clrex_15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    clrex_15(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_0(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_1(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_2(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_3(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_4(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_5(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_6(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_7(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_8(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_9(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_10(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_11(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_12(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_13(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_14(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dsb_15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dsb_15(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_0(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_1(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_2(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_3(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_4(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_5(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_6(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_7(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_8(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_9(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_10(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_11(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_12(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_13(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_14(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_dmb_15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    dmb_15(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_isb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;
    isb_hw(0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
/*
emu: 模拟
msr: AArch64 的 MSR 指令，把通用寄存器 Xt 的值写入系统寄存器。
register: 表示源操作数来自通用寄存器，而不是立即数。
s3_3_c4_c2_0: 系统寄存器的标准 AArch64 编码字段: S<op0>_<op1>_C<CRn>_C<CRm>_<op2>.这里是针对NZCV
*/
static inline enum emu_inst_result emu_msr_register_s3_3_c4_c2_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    write_nzcv(regs, read_gpr_or_zr(regs, entry->decoded.rt));
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_msr_register_s3_3_c4_c4_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fp_regs->fpcr = (uint32_t)read_gpr_or_zr(regs, entry->decoded.rt);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_msr_register_s3_3_c4_c4_1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fp_regs->fpsr = (uint32_t)read_gpr_or_zr(regs, entry->decoded.rt);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_msr_register_s3_3_c13_c0_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    arm64_write_tpidr_el0(read_gpr_or_zr(regs, entry->decoded.rt));
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_mrs_s3_3_c4_c2_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    write_gpr_or_zr(regs, entry->decoded.rt, read_nzcv(regs), true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_mrs_s3_3_c4_c4_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rt, fp_regs->fpcr, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_mrs_s3_3_c4_c4_1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rt, fp_regs->fpsr, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_mrs_s3_3_c13_c0_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    write_gpr_or_zr(regs, entry->decoded.rt, arm64_read_tpidr_el0(), true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_mrs_s3_3_c13_c0_3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    write_gpr_or_zr(regs, entry->decoded.rt, arm64_read_tpidrro_el0(), true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_mrs_s3_3_c14_c0_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    write_gpr_or_zr(regs, entry->decoded.rt, arm64_read_cntvct_el0(), true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_br(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = read_gpr_or_zr(regs, entry->decoded.rn);
    /* 更新 PSTATE.BTYPE[11:10]； */
    regs->pstate = (regs->pstate & ~(3ULL << 10)) | (1ULL << 10);
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_blr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t pc = regs->pc;
    regs->pc = read_gpr_or_zr(regs, entry->decoded.rn);
    write_gpr_or_zr(regs, 30, pc + 4, true);
    /* 更新 PSTATE.BTYPE[11:10]； */
    regs->pstate = (regs->pstate & ~(3ULL << 10)) | (2ULL << 10);
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ret(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    regs->pc = read_gpr_or_zr(regs, entry->decoded.rn);
    return EMU_INST_HANDLED;
}
enum emu_inst_result (*emu_build_branch_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_B:
        return emu_b;
    case ARM64_INST_BL:
        return emu_bl;
    case ARM64_INST_CBZ:
        if (decoded->operand_width == 32) return emu_cbz_w32;
        if (decoded->operand_width == 64) return emu_cbz_w64;
        return NULL;
    case ARM64_INST_CBNZ:
        if (decoded->operand_width == 32) return emu_cbnz_w32;
        if (decoded->operand_width == 64) return emu_cbnz_w64;
        return NULL;
    case ARM64_INST_TBZ:
        return emu_tbz;
    case ARM64_INST_TBNZ:
        return emu_tbnz;
    case ARM64_INST_B_COND:
        if (decoded->condition == 0) return emu_b_cond_eq;
        if (decoded->condition == 1) return emu_b_cond_ne;
        if (decoded->condition == 2) return emu_b_cond_cs;
        if (decoded->condition == 3) return emu_b_cond_cc;
        if (decoded->condition == 4) return emu_b_cond_mi;
        if (decoded->condition == 5) return emu_b_cond_pl;
        if (decoded->condition == 6) return emu_b_cond_vs;
        if (decoded->condition == 7) return emu_b_cond_vc;
        if (decoded->condition == 8) return emu_b_cond_hi;
        if (decoded->condition == 9) return emu_b_cond_ls;
        if (decoded->condition == 10) return emu_b_cond_ge;
        if (decoded->condition == 11) return emu_b_cond_lt;
        if (decoded->condition == 12) return emu_b_cond_gt;
        if (decoded->condition == 13) return emu_b_cond_le;
        if (decoded->condition == 14) return emu_b_cond_al;
        if (decoded->condition == 15) return emu_b_cond_nv;
        return NULL;
    case ARM64_INST_NOP:
        return emu_nop;
    case ARM64_INST_YIELD:
        return emu_yield;
    case ARM64_INST_CLREX:
        if (decoded->immediate == 0) return emu_clrex_0;
        if (decoded->immediate == 1) return emu_clrex_1;
        if (decoded->immediate == 2) return emu_clrex_2;
        if (decoded->immediate == 3) return emu_clrex_3;
        if (decoded->immediate == 4) return emu_clrex_4;
        if (decoded->immediate == 5) return emu_clrex_5;
        if (decoded->immediate == 6) return emu_clrex_6;
        if (decoded->immediate == 7) return emu_clrex_7;
        if (decoded->immediate == 8) return emu_clrex_8;
        if (decoded->immediate == 9) return emu_clrex_9;
        if (decoded->immediate == 10) return emu_clrex_10;
        if (decoded->immediate == 11) return emu_clrex_11;
        if (decoded->immediate == 12) return emu_clrex_12;
        if (decoded->immediate == 13) return emu_clrex_13;
        if (decoded->immediate == 14) return emu_clrex_14;
        if (decoded->immediate == 15) return emu_clrex_15;
        return NULL;
    case ARM64_INST_DSB:
        if (decoded->immediate == 0) return emu_dsb_0;
        if (decoded->immediate == 1) return emu_dsb_1;
        if (decoded->immediate == 2) return emu_dsb_2;
        if (decoded->immediate == 3) return emu_dsb_3;
        if (decoded->immediate == 4) return emu_dsb_4;
        if (decoded->immediate == 5) return emu_dsb_5;
        if (decoded->immediate == 6) return emu_dsb_6;
        if (decoded->immediate == 7) return emu_dsb_7;
        if (decoded->immediate == 8) return emu_dsb_8;
        if (decoded->immediate == 9) return emu_dsb_9;
        if (decoded->immediate == 10) return emu_dsb_10;
        if (decoded->immediate == 11) return emu_dsb_11;
        if (decoded->immediate == 12) return emu_dsb_12;
        if (decoded->immediate == 13) return emu_dsb_13;
        if (decoded->immediate == 14) return emu_dsb_14;
        if (decoded->immediate == 15) return emu_dsb_15;
        return NULL;
    case ARM64_INST_DMB:
        if (decoded->immediate == 0) return emu_dmb_0;
        if (decoded->immediate == 1) return emu_dmb_1;
        if (decoded->immediate == 2) return emu_dmb_2;
        if (decoded->immediate == 3) return emu_dmb_3;
        if (decoded->immediate == 4) return emu_dmb_4;
        if (decoded->immediate == 5) return emu_dmb_5;
        if (decoded->immediate == 6) return emu_dmb_6;
        if (decoded->immediate == 7) return emu_dmb_7;
        if (decoded->immediate == 8) return emu_dmb_8;
        if (decoded->immediate == 9) return emu_dmb_9;
        if (decoded->immediate == 10) return emu_dmb_10;
        if (decoded->immediate == 11) return emu_dmb_11;
        if (decoded->immediate == 12) return emu_dmb_12;
        if (decoded->immediate == 13) return emu_dmb_13;
        if (decoded->immediate == 14) return emu_dmb_14;
        if (decoded->immediate == 15) return emu_dmb_15;
        return NULL;
    case ARM64_INST_ISB:
        return emu_isb;
    case ARM64_INST_MSR_REGISTER:
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 2, 0)) return emu_msr_register_s3_3_c4_c2_0;
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 4, 0)) return emu_msr_register_s3_3_c4_c4_0;
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 4, 1)) return emu_msr_register_s3_3_c4_c4_1;
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 13, 0, 2)) return emu_msr_register_s3_3_c13_c0_2;
        return NULL;
    case ARM64_INST_MRS:
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 2, 0)) return emu_mrs_s3_3_c4_c2_0;
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 4, 0)) return emu_mrs_s3_3_c4_c4_0;
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 4, 4, 1)) return emu_mrs_s3_3_c4_c4_1;
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 13, 0, 2)) return emu_mrs_s3_3_c13_c0_2;
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 13, 0, 3)) return emu_mrs_s3_3_c13_c0_3;
        if (decoded->sysreg == ARM64_SYSREG_KEY(3, 3, 14, 0, 2)) return emu_mrs_s3_3_c14_c0_2;
        return NULL;
    case ARM64_INST_BR:
        return emu_br;
    case ARM64_INST_BLR:
        return emu_blr;
    case ARM64_INST_RET:
        return emu_ret;
    default:
        return NULL;
    }
}
// clang-format on
