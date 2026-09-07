// clang-format off
#include "emulate_inst.h"

static inline enum emu_inst_result emu_adr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (entry->decoded.rd != 31) regs->regs[entry->decoded.rd] = regs->pc + entry->decoded.offset;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adrp(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    if (entry->decoded.rd != 31) regs->regs[entry->decoded.rd] = (regs->pc & ~0xFFFULL) + entry->decoded.offset;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_extr_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    extract_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_extr_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    extract_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    add_w32(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    add_w64(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    adds_w32(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    adds_w64(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sub_w32(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sub_w64(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    subs_w32(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    subs_w64(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smax_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    struct arm64_hw_template_output immediate_output __attribute__((__uninitialized__));
    sxtb_shift(entry->decoded.immediate, 0, 0, 0, 0, &immediate_output);
    smax_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, immediate_output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smax_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    struct arm64_hw_template_output immediate_output __attribute__((__uninitialized__));
    sxtb_shift(entry->decoded.immediate, 0, 0, 0, 0, &immediate_output);
    smax_w64(read_gpr_or_zr(regs, entry->decoded.rn), immediate_output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umax_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umax_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umax_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umax_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smin_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    struct arm64_hw_template_output immediate_output __attribute__((__uninitialized__));
    sxtb_shift(entry->decoded.immediate, 0, 0, 0, 0, &immediate_output);
    smin_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, immediate_output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smin_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    struct arm64_hw_template_output immediate_output __attribute__((__uninitialized__));
    sxtb_shift(entry->decoded.immediate, 0, 0, 0, 0, &immediate_output);
    smin_w64(read_gpr_or_zr(regs, entry->decoded.rn), immediate_output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umin_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umin_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umin_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umin_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_and_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    and_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_and_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    and_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    orr_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    orr_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    eor_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    eor_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ands_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ands_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_movn_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    movn_w32(read_gpr_or_zr(regs, entry->decoded.rd), entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_movn_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    movn_w64(read_gpr_or_zr(regs, entry->decoded.rd), entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_movz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    movz_w32(read_gpr_or_zr(regs, entry->decoded.rd), entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_movz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    movz_w64(read_gpr_or_zr(regs, entry->decoded.rd), entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_movk_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    movk_w32(read_gpr_or_zr(regs, entry->decoded.rd), entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_movk_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    movk_w64(read_gpr_or_zr(regs, entry->decoded.rd), entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sbfm_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sbfm_dynamic_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, entry->decoded.imms, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sbfm_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sbfm_dynamic_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, entry->decoded.imms, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bfm_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    bfm_dynamic_w32(read_gpr_or_zr(regs, entry->decoded.rd) & 0xFFFFFFFFULL, read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bfm_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    bfm_dynamic_w64(read_gpr_or_zr(regs, entry->decoded.rd), read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ubfm_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ubfm_dynamic_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ubfm_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ubfm_dynamic_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
enum emu_inst_result (*emu_build_immediate_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_ADR:
        return emu_adr;
    case ARM64_INST_ADRP:
        return emu_adrp;
    case ARM64_INST_EXTR:
        if (decoded->operand_width == 32) return emu_extr_w32;
        if (decoded->operand_width == 64) return emu_extr_w64;
        return NULL;
    case ARM64_INST_ADD_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_add_immediate_w32;
        if (decoded->operand_width == 64) return emu_add_immediate_w64;
        return NULL;
    case ARM64_INST_ADDS_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_adds_immediate_w32;
        if (decoded->operand_width == 64) return emu_adds_immediate_w64;
        return NULL;
    case ARM64_INST_SUB_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_sub_immediate_w32;
        if (decoded->operand_width == 64) return emu_sub_immediate_w64;
        return NULL;
    case ARM64_INST_SUBS_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_subs_immediate_w32;
        if (decoded->operand_width == 64) return emu_subs_immediate_w64;
        return NULL;
    case ARM64_INST_SMAX_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_smax_immediate_w32;
        if (decoded->operand_width == 64) return emu_smax_immediate_w64;
        return NULL;
    case ARM64_INST_UMAX_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_umax_immediate_w32;
        if (decoded->operand_width == 64) return emu_umax_immediate_w64;
        return NULL;
    case ARM64_INST_SMIN_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_smin_immediate_w32;
        if (decoded->operand_width == 64) return emu_smin_immediate_w64;
        return NULL;
    case ARM64_INST_UMIN_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_umin_immediate_w32;
        if (decoded->operand_width == 64) return emu_umin_immediate_w64;
        return NULL;
    case ARM64_INST_AND_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_and_immediate_w32;
        if (decoded->operand_width == 64) return emu_and_immediate_w64;
        return NULL;
    case ARM64_INST_ORR_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_orr_immediate_w32;
        if (decoded->operand_width == 64) return emu_orr_immediate_w64;
        return NULL;
    case ARM64_INST_EOR_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_eor_immediate_w32;
        if (decoded->operand_width == 64) return emu_eor_immediate_w64;
        return NULL;
    case ARM64_INST_ANDS_IMMEDIATE:
        if (decoded->operand_width == 32) return emu_ands_immediate_w32;
        if (decoded->operand_width == 64) return emu_ands_immediate_w64;
        return NULL;
    case ARM64_INST_MOVN:
        if (decoded->operand_width == 32) return emu_movn_w32;
        if (decoded->operand_width == 64) return emu_movn_w64;
        return NULL;
    case ARM64_INST_MOVZ:
        if (decoded->operand_width == 32) return emu_movz_w32;
        if (decoded->operand_width == 64) return emu_movz_w64;
        return NULL;
    case ARM64_INST_MOVK:
        if (decoded->operand_width == 32) return emu_movk_w32;
        if (decoded->operand_width == 64) return emu_movk_w64;
        return NULL;
    case ARM64_INST_SBFM:
        if (decoded->operand_width == 32) return emu_sbfm_w32;
        if (decoded->operand_width == 64) return emu_sbfm_w64;
        return NULL;
    case ARM64_INST_BFM:
        if (decoded->operand_width == 32) return emu_bfm_w32;
        if (decoded->operand_width == 64) return emu_bfm_w64;
        return NULL;
    case ARM64_INST_UBFM:
        if (decoded->operand_width == 32) return emu_ubfm_w32;
        if (decoded->operand_width == 64) return emu_ubfm_w64;
        return NULL;
    default:
        return NULL;
    }
}
// clang-format on
