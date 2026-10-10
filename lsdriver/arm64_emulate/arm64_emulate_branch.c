/*
ARM64 指令执行器：分支与系统类。

emu_* 函数是已解码指令的固定执行函数，直接完成一条指令的语义并更新软件现场与 PC。
支持 B、BL、BR、BLR、RET、B.cond、CBZ/CBNZ、TBZ/TBNZ，以及 NOP、YIELD、CLREX、DSB、DMB、ISB、DC ZVA、MRS 和 MSR。
emu_build_branch_executor() 负责按指令、条件码、立即数和系统寄存器选择具体执行函数。
*/
#include "emulate_inst.h"

static inline void emu_b_immediate(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc += entry->decoded.offset;
    return;
}
static inline void emu_bl_immediate(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, 30, regs->pc + 4, true);
    regs->pc += entry->decoded.offset;
    return;
}
static inline void emu_cbz_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = (uint32_t)read_gpr_or_zr(regs, entry->decoded.rt) == 0 ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_cbz_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = read_gpr_or_zr(regs, entry->decoded.rt) == 0 ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_cbnz_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = (uint32_t)read_gpr_or_zr(regs, entry->decoded.rt) != 0 ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_cbnz_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = read_gpr_or_zr(regs, entry->decoded.rt) != 0 ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_tbz_bit(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = ((read_gpr_or_zr(regs, entry->decoded.rt) >> entry->decoded.immediate) & 1) == 0 ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_tbnz_bit(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = ((read_gpr_or_zr(regs, entry->decoded.rt) >> entry->decoded.immediate) & 1) != 0 ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}

static inline void emu_b_cond_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 0) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 1) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 2) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 3) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 4) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 5) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 6) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 7) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 8) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 9) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 10) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 11) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 12) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 13) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 14) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_b_cond_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = cond_holds(read_nzcv(regs), 15) ? regs->pc + entry->decoded.offset : regs->pc + 4;
    return;
}
static inline void emu_nop_hint(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc += 4;
    return;
}
static inline void emu_yield_hint(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm0(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm1(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm2(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm5(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm6(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm7(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm9(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm10(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm11(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm12(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm13(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm14(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clrex_imm15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    clrex_imm15(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm0(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm1(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm2(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm5(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm6(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm7(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm9(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm10(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm11(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm12(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm13(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm14(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dsb_imm15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dsb_imm15(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm0(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm1(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm2(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm3(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm4(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm5(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm6(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm7(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm9(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm10(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm11(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm12(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm13(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm14(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dmb_imm15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dmb_imm15(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_dc_zva_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dc_zva_x(read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_isb_sy(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    isb_sy(0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
/*
emu: 模拟
msr: AArch64 的 MSR 指令，把通用寄存器 Xt 的值写入系统寄存器。
register: 表示源操作数来自通用寄存器，而不是立即数。
s3_3_c4_c2_0: 系统寄存器的标准 AArch64 编码字段: S<op0>_<op1>_C<CRn>_C<CRm>_<op2>.这里是针对NZCV
*/
static inline void emu_msr_register_s3_3_c4_c2_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_nzcv(regs, read_gpr_or_zr(regs, entry->decoded.rt));
    regs->pc += 4;
    return;
}
static inline void emu_msr_register_s3_3_c4_c4_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fp_regs->fpcr = (uint32_t)read_gpr_or_zr(regs, entry->decoded.rt);
    regs->pc += 4;
    return;
}
static inline void emu_msr_register_s3_3_c4_c4_1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fp_regs->fpsr = (uint32_t)read_gpr_or_zr(regs, entry->decoded.rt);
    regs->pc += 4;
    return;
}
static inline void emu_msr_register_s3_3_c13_c0_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    arm64_write_tpidr_el0(read_gpr_or_zr(regs, entry->decoded.rt));
    regs->pc += 4;
    return;
}
static inline void emu_mrs_s3_3_c4_c2_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rt, read_nzcv(regs), true);
    regs->pc += 4;
    return;
}
static inline void emu_mrs_s3_3_c4_c4_0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rt, fp_regs->fpcr, true);
    regs->pc += 4;
    return;
}
static inline void emu_mrs_s3_3_c4_c4_1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rt, fp_regs->fpsr, true);
    regs->pc += 4;
    return;
}
static inline void emu_mrs_s3_3_c13_c0_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rt, arm64_read_tpidr_el0(), true);
    regs->pc += 4;
    return;
}
static inline void emu_mrs_s3_3_c13_c0_3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rt, arm64_read_tpidrro_el0(), true);
    regs->pc += 4;
    return;
}
static inline void emu_mrs_s3_3_c14_c0_2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rt, arm64_read_cntvct_el0(), true);
    regs->pc += 4;
    return;
}
static inline void emu_br_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = read_gpr_or_zr(regs, entry->decoded.rn);
    /* 更新 PSTATE.BTYPE[11:10]； */
    regs->pstate = (regs->pstate & ~(3ULL << 10)) | (1ULL << 10);
    return;
}
static inline void emu_blr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    regs->pc = read_gpr_or_zr(regs, entry->decoded.rn);
    write_gpr_or_zr(regs, 30, pc + 4, true);
    /* 更新 PSTATE.BTYPE[11:10]； */
    regs->pstate = (regs->pstate & ~(3ULL << 10)) | (2ULL << 10);
    return;
}
static inline void emu_ret_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    regs->pc = read_gpr_or_zr(regs, entry->decoded.rn);
    return;
}
void (*emu_build_branch_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_B:
        return emu_b_immediate;
    case ARM64_INST_BL:
        return emu_bl_immediate;
    case ARM64_INST_CBZ:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_cbz_w;
        case 64:
            return emu_cbz_x;
        default:
            return NULL;
        }
    case ARM64_INST_CBNZ:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_cbnz_w;
        case 64:
            return emu_cbnz_x;
        default:
            return NULL;
        }
    case ARM64_INST_TBZ:
        return emu_tbz_bit;
    case ARM64_INST_TBNZ:
        return emu_tbnz_bit;
    case ARM64_INST_B_COND:
        switch (decoded->condition)
        {
        case 0:
            return emu_b_cond_eq;
        case 1:
            return emu_b_cond_ne;
        case 2:
            return emu_b_cond_cs;
        case 3:
            return emu_b_cond_cc;
        case 4:
            return emu_b_cond_mi;
        case 5:
            return emu_b_cond_pl;
        case 6:
            return emu_b_cond_vs;
        case 7:
            return emu_b_cond_vc;
        case 8:
            return emu_b_cond_hi;
        case 9:
            return emu_b_cond_ls;
        case 10:
            return emu_b_cond_ge;
        case 11:
            return emu_b_cond_lt;
        case 12:
            return emu_b_cond_gt;
        case 13:
            return emu_b_cond_le;
        case 14:
            return emu_b_cond_al;
        case 15:
            return emu_b_cond_nv;
        default:
            return NULL;
        }
    case ARM64_INST_NOP:
        return emu_nop_hint;
    case ARM64_INST_YIELD:
        return emu_yield_hint;
    case ARM64_INST_CLREX:
        switch (decoded->immediate)
        {
        case 0:
            return emu_clrex_imm0;
        case 1:
            return emu_clrex_imm1;
        case 2:
            return emu_clrex_imm2;
        case 3:
            return emu_clrex_imm3;
        case 4:
            return emu_clrex_imm4;
        case 5:
            return emu_clrex_imm5;
        case 6:
            return emu_clrex_imm6;
        case 7:
            return emu_clrex_imm7;
        case 8:
            return emu_clrex_imm8;
        case 9:
            return emu_clrex_imm9;
        case 10:
            return emu_clrex_imm10;
        case 11:
            return emu_clrex_imm11;
        case 12:
            return emu_clrex_imm12;
        case 13:
            return emu_clrex_imm13;
        case 14:
            return emu_clrex_imm14;
        case 15:
            return emu_clrex_imm15;
        default:
            return NULL;
        }
    case ARM64_INST_DSB:
        switch (decoded->immediate)
        {
        case 0:
            return emu_dsb_imm0;
        case 1:
            return emu_dsb_imm1;
        case 2:
            return emu_dsb_imm2;
        case 3:
            return emu_dsb_imm3;
        case 4:
            return emu_dsb_imm4;
        case 5:
            return emu_dsb_imm5;
        case 6:
            return emu_dsb_imm6;
        case 7:
            return emu_dsb_imm7;
        case 8:
            return emu_dsb_imm8;
        case 9:
            return emu_dsb_imm9;
        case 10:
            return emu_dsb_imm10;
        case 11:
            return emu_dsb_imm11;
        case 12:
            return emu_dsb_imm12;
        case 13:
            return emu_dsb_imm13;
        case 14:
            return emu_dsb_imm14;
        case 15:
            return emu_dsb_imm15;
        default:
            return NULL;
        }
    case ARM64_INST_DMB:
        switch (decoded->immediate)
        {
        case 0:
            return emu_dmb_imm0;
        case 1:
            return emu_dmb_imm1;
        case 2:
            return emu_dmb_imm2;
        case 3:
            return emu_dmb_imm3;
        case 4:
            return emu_dmb_imm4;
        case 5:
            return emu_dmb_imm5;
        case 6:
            return emu_dmb_imm6;
        case 7:
            return emu_dmb_imm7;
        case 8:
            return emu_dmb_imm8;
        case 9:
            return emu_dmb_imm9;
        case 10:
            return emu_dmb_imm10;
        case 11:
            return emu_dmb_imm11;
        case 12:
            return emu_dmb_imm12;
        case 13:
            return emu_dmb_imm13;
        case 14:
            return emu_dmb_imm14;
        case 15:
            return emu_dmb_imm15;
        default:
            return NULL;
        }
    case ARM64_INST_ISB:
        return emu_isb_sy;
    case ARM64_INST_DC_ZVA:
        return emu_dc_zva_x;
    case ARM64_INST_MSR_REGISTER:
        switch (decoded->sysreg)
        {
        case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
            return emu_msr_register_s3_3_c4_c2_0;
        case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
            return emu_msr_register_s3_3_c4_c4_0;
        case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
            return emu_msr_register_s3_3_c4_c4_1;
        case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
            return emu_msr_register_s3_3_c13_c0_2;
        default:
            return NULL;
        }
    case ARM64_INST_MRS:
        switch (decoded->sysreg)
        {
        case ARM64_SYSREG_KEY(3, 3, 4, 2, 0):
            return emu_mrs_s3_3_c4_c2_0;
        case ARM64_SYSREG_KEY(3, 3, 4, 4, 0):
            return emu_mrs_s3_3_c4_c4_0;
        case ARM64_SYSREG_KEY(3, 3, 4, 4, 1):
            return emu_mrs_s3_3_c4_c4_1;
        case ARM64_SYSREG_KEY(3, 3, 13, 0, 2):
            return emu_mrs_s3_3_c13_c0_2;
        case ARM64_SYSREG_KEY(3, 3, 13, 0, 3):
            return emu_mrs_s3_3_c13_c0_3;
        case ARM64_SYSREG_KEY(3, 3, 14, 0, 2):
            return emu_mrs_s3_3_c14_c0_2;
        default:
            return NULL;
        }
    case ARM64_INST_BR:
        return emu_br_x;
    case ARM64_INST_BLR:
        return emu_blr_x;
    case ARM64_INST_RET:
        return emu_ret_x;
    default:
        return NULL;
    }
}
