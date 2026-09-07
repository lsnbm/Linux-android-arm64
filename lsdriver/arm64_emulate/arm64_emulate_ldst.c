// clang-format off
#include "emulate_inst.h"
static inline enum emu_inst_result emu_sturb_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stur_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sturh_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stur_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sturw_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stur_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sturx_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stur_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sttrb_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    sttr_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sttrh_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    sttr_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sttrw_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    sttr_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sttrx_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    sttr_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldurb_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldur_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldurh_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldur_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldurw_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldur_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldurx_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldur_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldursb_w_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldursb_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldursb_x_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldursb_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldursh_w_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldursh_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldursh_x_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldursh_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldursw_x_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldursw_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldtrb_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldtr_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldtrh_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldtr_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldtrw_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldtr_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldtrx_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldtr_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldtrsb_w_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldtrsb_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldtrsb_x_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldtrsb_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldtrsh_w_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldtrsh_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldtrsh_x_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldtrsh_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldtrsw_x_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldtrsw_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_b_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_h_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strw_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strx_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_b_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_h_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrw_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrx_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_w_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_addr_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_x_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_addr_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_w_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_addr_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_x_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_addr_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsw_x_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsw_addr_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_b_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_h_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strw_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strx_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_b_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_h_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrw_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrx_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_w_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_addr_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_x_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_addr_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_w_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_addr_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_x_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_addr_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsw_x_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsw_addr_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_addr_b_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_addr_h_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strw_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_addr_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strx_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_addr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_b_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_h_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrw_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrx_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_addr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_w_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_addr_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_x_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_addr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_w_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_addr_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_x_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_addr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsw_x_gpr_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsw_addr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_b_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_h_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strw_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_w_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strx_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_x_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_b_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_h_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrw_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_w_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrx_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_x_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_w_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_reg_w_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_x_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_reg_x_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_w_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_reg_w_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_x_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_reg_x_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsw_x_gpr_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsw_reg_x_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_b_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_h_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strw_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_w_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strx_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_x_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_b_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_h_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrw_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_w_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrx_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_x_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_w_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_reg_w_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_x_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_reg_x_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_w_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_reg_w_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_x_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_reg_x_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsw_x_gpr_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsw_reg_x_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_b_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_h_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strw_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_w_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strx_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_x_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_b_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_h_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrw_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_w_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrx_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_x_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_w_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_reg_w_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_x_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_reg_x_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_w_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_reg_w_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_x_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_reg_x_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsw_x_gpr_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsw_reg_x_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_b_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_h_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strw_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_w_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strx_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    str_reg_x_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_b_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_h_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrw_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_w_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrx_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_reg_x_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_w_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_reg_w_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsb_x_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsb_reg_x_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_w_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_reg_w_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsh_x_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsh_reg_x_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsw_x_gpr_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsw_reg_x_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sturb_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_addr_fp_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sturh_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_addr_fp_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sturs_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_addr_fp_s(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sturd_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_addr_fp_d(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sturq_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_addr_fp_q(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldurb_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldur_addr_fp_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldurh_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldur_addr_fp_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldurs_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldur_addr_fp_s(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldurd_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldur_addr_fp_d(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldurq_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldur_addr_fp_q(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_b_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_h_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strs_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_s_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strd_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_d_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strq_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_q_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_b_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_h_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrs_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_s_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrd_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_d_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrq_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_q_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_b_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_h_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strs_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_s_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strd_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_d_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strq_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    str_addr_fp_q_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_b_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_h_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrs_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_s_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrd_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_d_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrq_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_q_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_addr_fp_b_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_addr_fp_h_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strs_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_addr_fp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strd_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_addr_fp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strq_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_addr_fp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_b_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_h_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrs_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrd_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrq_fp_simd_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_addr_fp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_b_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_h_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strs_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_s_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strd_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_d_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strq_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_q_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_b_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_h_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrs_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_s_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrd_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_d_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrq_fp_simd_register_offset_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_q_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_b_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_h_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strs_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_s_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strd_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_d_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strq_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_q_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_b_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_h_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrs_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_s_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrd_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_d_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrq_fp_simd_register_offset_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_q_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_b_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_h_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strs_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_s_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strd_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_d_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strq_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_q_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_b_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_h_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrs_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_s_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrd_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_d_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrq_fp_simd_register_offset_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_q_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strb_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_b_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strh_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_h_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strs_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_s_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strd_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_d_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_strq_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_reg_fp_q_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrb_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_b_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrh_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_h_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrs_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_s_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrd_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_d_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrq_fp_simd_register_offset_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldr_reg_fp_q_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrw_literal_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_literal_w(regs->pc, entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrx_literal_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_literal_x(regs->pc, entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrsw_literal(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldrsw_literal_x(regs->pc, entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrs_literal_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_literal_fp_s(regs->pc, entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrd_literal_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_literal_fp_d(regs->pc, entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldrq_literal_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldr_literal_fp_q(regs->pc, entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldxrb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldxrb_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldxrh(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldxrh_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldxrw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldxr_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldxrx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldxr_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaxrb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaxrb_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaxrh(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaxrh_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaxrw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaxr_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaxrx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaxr_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldxpw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldxp_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldxpx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldxp_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaxpw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaxp_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaxpx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaxp_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stxrb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stxrb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stxrh(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stxrh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stxrw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stxr_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stxrx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stxr_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlxrb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stlxrb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlxrh(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stlxrh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlxrw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stlxr_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlxrx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stlxr_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stxpw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stxp_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stxpx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stxp_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlxpw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stlxp_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlxpx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stlxp_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caspw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casp_w(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, output.value1, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caspx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casp_x(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, output.value1, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caspaw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    caspa_w(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, output.value1, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caspax(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    caspa_x(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, output.value1, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casplw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    caspl_w(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, output.value1, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casplx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    caspl_x(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, output.value1, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caspalw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    caspal_w(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, output.value1, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caspalx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    caspal_x(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, output.value1, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    cas_b(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_cash(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    cas_h(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    cas_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    cas_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casab(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casa_b(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casah(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casa_h(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casaw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casa_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casax(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casa_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caslb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casl_b(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caslh(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casl_h(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caslw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_caslx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casalb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casal_b(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casalh(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casal_h(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casalw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_casalx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    casal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rs, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaddal_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaddal_b(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaddal_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaddal_h(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaddal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaddal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldaddal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldaddal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stllr_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stllr_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stllr_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stllr_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stllr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stllr_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stllr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stllr_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlr_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stlr_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlr_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stlr_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stlr_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stlr_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldlar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldlar_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldlar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldlar_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldlar_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldlar_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldlar_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldlar_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldar_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldar_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldar_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldar_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldar_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldar_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapr_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapr_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapr_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapr_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapr_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapr_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlur_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stlur_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlur_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stlur_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlur_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stlur_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stlur_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stlur_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapur_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapur_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapur_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapur_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapur_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapur_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapur_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapur_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapursb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapur_addr_sb_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapursb_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapur_addr_sb_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapursh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapur_addr_sh_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapursh_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapur_addr_sh_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldapursw_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldapur_addr_sw_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldnpw_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldnp_addr_gpr_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldnpx_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldnp_addr_gpr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpw_gpr_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldp_addr_gpr_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpx_gpr_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldp_addr_gpr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpsw_gpr_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldpsw_addr_gpr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpw_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldp_addr_gpr_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpx_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldp_addr_gpr_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpsw_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldpsw_addr_gpr_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpw_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldp_addr_gpr_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, false);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpx_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldp_addr_gpr_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpsw_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    ldpsw_addr_gpr_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rt, output.value0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, output.value1, true);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stnpw_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stnp_addr_gpr_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stnpx_gpr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stnp_addr_gpr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpw_gpr_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stp_addr_gpr_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpx_gpr_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    stp_addr_gpr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpw_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_gpr_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpx_gpr_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_gpr_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpw_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_gpr_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpx_gpr_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_gpr_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldnps_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldnpd_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldnpq_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldps_fp_simd_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpd_fp_simd_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpq_fp_simd_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldps_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_s_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpd_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_d_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpq_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_q_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldps_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_s_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpd_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_d_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ldpq_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output = {0};
    ldp_addr_fp_q_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    fp_regs->q[entry->decoded.rt] = ((__uint128_t)output.value1 << 64) | output.value0;
    fp_regs->q[entry->decoded.rt2] = ((__uint128_t)output.value3 << 64) | output.value2;
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stnps_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_addr_fp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stnpd_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_addr_fp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stnpq_fp_simd(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_addr_fp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stps_fp_simd_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_addr_fp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpd_fp_simd_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_addr_fp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpq_fp_simd_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_addr_fp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stps_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_fp_s_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpd_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_fp_d_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpq_fp_simd_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_fp_q_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stps_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_fp_s_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpd_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_fp_d_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_stpq_fp_simd_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    stp_addr_fp_q_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rn, output.writeback_value);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_0(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_1(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_2(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_3(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_4(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_5(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_6(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_7(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_8(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_9(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_10(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_11(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_12(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_13(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_14(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_15(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_16(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_17(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_18(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_19(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_20(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_21(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_22(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_23(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op24(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_24(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op25(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_25(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op26(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_26(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op27(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_27(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op28(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_28(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op29(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_29(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op30(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_30(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_literal_op31(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_31(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_0(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_1(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_2(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_3(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_4(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_5(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_6(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_7(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_8(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_9(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_10(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_11(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_12(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_13(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_14(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_15(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_16(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_17(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_18(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_19(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_20(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_21(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_22(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_23(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op24(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_24(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op25(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_25(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op26(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_26(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op27(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_27(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op28(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_28(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op29(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_29(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op30(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_30(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_unsigned_offset_op31(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_31(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_0(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_1(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_2(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_3(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_4(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_5(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_6(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_7(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_8(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_9(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_10(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_11(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_12(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_13(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_14(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_15(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_16(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_17(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_18(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_19(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_20(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_21(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_22(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_23(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op24(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_24(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op25(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_25(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op26(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_26(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op27(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_27(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op28(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_28(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op29(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_29(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op30(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_30(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtw_op31(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_31(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_0(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_1(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_2(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_3(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_4(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_5(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_6(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_7(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_8(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_9(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_10(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_11(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_12(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_13(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_14(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_15(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_16(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_17(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_18(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_19(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_20(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_21(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_22(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_23(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op24(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_24(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op25(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_25(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op26(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_26(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op27(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_27(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op28(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_28(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op29(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_29(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op30(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_30(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_uxtx_op31(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_31(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_0(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_1(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_2(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_3(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_4(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_5(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_6(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_7(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_8(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_9(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_10(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_11(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_12(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_13(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_14(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_15(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_16(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_17(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_18(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_19(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_20(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_21(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_22(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_23(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op24(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_24(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op25(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_25(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op26(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_26(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op27(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_27(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op28(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_28(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op29(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_29(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op30(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_30(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_prfm_register_offset_sxtw_op31(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    prfm_31(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ld1_single_structure_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    if ((entry->decoded.lane_index + 1) * 8 > entry->decoded.operand_width) return EMU_INST_SKIP;
    ld1_addr_simd_element_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    simd_insert_b((uint64_t)&fp_regs->q[entry->decoded.rt], output.value0, entry->decoded.lane_index, 0, 0, &output);
    __builtin_memcpy(&fp_regs->q[entry->decoded.rt], &output.value0, 16);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ld1_single_structure_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    if ((entry->decoded.lane_index + 1) * 16 > entry->decoded.operand_width) return EMU_INST_SKIP;
    ld1_addr_simd_element_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    simd_insert_h((uint64_t)&fp_regs->q[entry->decoded.rt], output.value0, entry->decoded.lane_index, 0, 0, &output);
    __builtin_memcpy(&fp_regs->q[entry->decoded.rt], &output.value0, 16);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ld1_single_structure_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    if ((entry->decoded.lane_index + 1) * 32 > entry->decoded.operand_width) return EMU_INST_SKIP;
    ld1_addr_simd_element_s(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    simd_insert_s((uint64_t)&fp_regs->q[entry->decoded.rt], output.value0, entry->decoded.lane_index, 0, 0, &output);
    __builtin_memcpy(&fp_regs->q[entry->decoded.rt], &output.value0, 16);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ld1_single_structure_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    if ((entry->decoded.lane_index + 1) * 64 > entry->decoded.operand_width) return EMU_INST_SKIP;
    ld1_addr_simd_element_d(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &output);
    simd_insert_d((uint64_t)&fp_regs->q[entry->decoded.rt], output.value0, entry->decoded.lane_index, 0, 0, &output);
    __builtin_memcpy(&fp_regs->q[entry->decoded.rt], &output.value0, 16);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_st1_single_structure_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    if ((entry->decoded.lane_index + 1) * 8 > entry->decoded.operand_width) return EMU_INST_SKIP;
    simd_extract_b((uint64_t)&fp_regs->q[entry->decoded.rt], entry->decoded.lane_index, 0, 0, 0, &output);
    stur_addr_b(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, output.value0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_st1_single_structure_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    if ((entry->decoded.lane_index + 1) * 16 > entry->decoded.operand_width) return EMU_INST_SKIP;
    simd_extract_h((uint64_t)&fp_regs->q[entry->decoded.rt], entry->decoded.lane_index, 0, 0, 0, &output);
    stur_addr_h(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, output.value0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_st1_single_structure_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    if ((entry->decoded.lane_index + 1) * 32 > entry->decoded.operand_width) return EMU_INST_SKIP;
    simd_extract_s((uint64_t)&fp_regs->q[entry->decoded.rt], entry->decoded.lane_index, 0, 0, 0, &output);
    stur_addr_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, output.value0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_st1_single_structure_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    if ((entry->decoded.lane_index + 1) * 64 > entry->decoded.operand_width) return EMU_INST_SKIP;
    simd_extract_d((uint64_t)&fp_regs->q[entry->decoded.rt], entry->decoded.lane_index, 0, 0, 0, &output);
    stur_addr_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, output.value0, 0, 0, 0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
enum emu_inst_result (*emu_build_ldst_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_LD1:
        switch (decoded->element_width)
        {
        case 8: return emu_ld1_single_structure_b;
        case 16: return emu_ld1_single_structure_h;
        case 32: return emu_ld1_single_structure_s;
        case 64: return emu_ld1_single_structure_d;
        default: return 0;
        }
    case ARM64_INST_ST1:
        switch (decoded->element_width)
        {
        case 8: return emu_st1_single_structure_b;
        case 16: return emu_st1_single_structure_h;
        case 32: return emu_st1_single_structure_s;
        case 64: return emu_st1_single_structure_d;
        default: return 0;
        }
    case ARM64_INST_STURB_GPR:
        return emu_sturb_gpr;
    case ARM64_INST_STURH_GPR:
        return emu_sturh_gpr;
    case ARM64_INST_STUR_GPR:
        return decoded->operand_width == 32 ? emu_sturw_gpr : emu_sturx_gpr;
    case ARM64_INST_STTRB_GPR:
        return emu_sttrb_gpr;
    case ARM64_INST_STTRH_GPR:
        return emu_sttrh_gpr;
    case ARM64_INST_STTR_GPR:
        return decoded->operand_width == 32 ? emu_sttrw_gpr : emu_sttrx_gpr;
    case ARM64_INST_STRB_GPR_POST_INDEX:
        return emu_strb_gpr_post_index;
    case ARM64_INST_STRH_GPR_POST_INDEX:
        return emu_strh_gpr_post_index;
    case ARM64_INST_STR_GPR_POST_INDEX:
        return decoded->operand_width == 32 ? emu_strw_gpr_post_index : emu_strx_gpr_post_index;
    case ARM64_INST_STRB_GPR_PRE_INDEX:
        return emu_strb_gpr_pre_index;
    case ARM64_INST_STRH_GPR_PRE_INDEX:
        return emu_strh_gpr_pre_index;
    case ARM64_INST_STR_GPR_PRE_INDEX:
        return decoded->operand_width == 32 ? emu_strw_gpr_pre_index : emu_strx_gpr_pre_index;
    case ARM64_INST_STRB_GPR_UNSIGNED_OFFSET:
        return emu_strb_gpr_unsigned_offset;
    case ARM64_INST_STRH_GPR_UNSIGNED_OFFSET:
        return emu_strh_gpr_unsigned_offset;
    case ARM64_INST_STR_GPR_UNSIGNED_OFFSET:
        return decoded->operand_width == 32 ? emu_strw_gpr_unsigned_offset : emu_strx_gpr_unsigned_offset;
    case ARM64_INST_STRB_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_strb_gpr_register_offset_uxtw;
        case 3:
            return emu_strb_gpr_register_offset_uxtx;
        case 6:
            return emu_strb_gpr_register_offset_sxtw;
        case 7:
            return emu_strb_gpr_register_offset_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_STRH_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_strh_gpr_register_offset_uxtw;
        case 3:
            return emu_strh_gpr_register_offset_uxtx;
        case 6:
            return emu_strh_gpr_register_offset_sxtw;
        case 7:
            return emu_strh_gpr_register_offset_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_STR_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return decoded->operand_width == 32 ? emu_strw_gpr_register_offset_uxtw : emu_strx_gpr_register_offset_uxtw;
        case 3:
            return decoded->operand_width == 32 ? emu_strw_gpr_register_offset_uxtx : emu_strx_gpr_register_offset_uxtx;
        case 6:
            return decoded->operand_width == 32 ? emu_strw_gpr_register_offset_sxtw : emu_strx_gpr_register_offset_sxtw;
        case 7:
            return decoded->operand_width == 32 ? emu_strw_gpr_register_offset_sxtw : emu_strx_gpr_register_offset_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_LDURB_GPR:
        return emu_ldurb_gpr;
    case ARM64_INST_LDURH_GPR:
        return emu_ldurh_gpr;
    case ARM64_INST_LDUR_GPR:
        return decoded->operand_width == 32 ? emu_ldurw_gpr : emu_ldurx_gpr;
    case ARM64_INST_LDURSB_GPR:
        return decoded->operand_width == 32 ? emu_ldursb_w_gpr : emu_ldursb_x_gpr;
    case ARM64_INST_LDURSH_GPR:
        return decoded->operand_width == 32 ? emu_ldursh_w_gpr : emu_ldursh_x_gpr;
    case ARM64_INST_LDURSW_GPR:
        return emu_ldursw_x_gpr;
    case ARM64_INST_LDTRB_GPR:
        return emu_ldtrb_gpr;
    case ARM64_INST_LDTRH_GPR:
        return emu_ldtrh_gpr;
    case ARM64_INST_LDTR_GPR:
        return decoded->operand_width == 32 ? emu_ldtrw_gpr : emu_ldtrx_gpr;
    case ARM64_INST_LDTRSB_GPR:
        return decoded->operand_width == 32 ? emu_ldtrsb_w_gpr : emu_ldtrsb_x_gpr;
    case ARM64_INST_LDTRSH_GPR:
        return decoded->operand_width == 32 ? emu_ldtrsh_w_gpr : emu_ldtrsh_x_gpr;
    case ARM64_INST_LDTRSW_GPR:
        return emu_ldtrsw_x_gpr;
    case ARM64_INST_LDRB_GPR_POST_INDEX:
        return emu_ldrb_gpr_post_index;
    case ARM64_INST_LDRH_GPR_POST_INDEX:
        return emu_ldrh_gpr_post_index;
    case ARM64_INST_LDR_GPR_POST_INDEX:
        return decoded->operand_width == 32 ? emu_ldrw_gpr_post_index : emu_ldrx_gpr_post_index;
    case ARM64_INST_LDRSB_GPR_POST_INDEX:
        return decoded->operand_width == 32 ? emu_ldrsb_w_gpr_post_index : emu_ldrsb_x_gpr_post_index;
    case ARM64_INST_LDRSH_GPR_POST_INDEX:
        return decoded->operand_width == 32 ? emu_ldrsh_w_gpr_post_index : emu_ldrsh_x_gpr_post_index;
    case ARM64_INST_LDRSW_GPR_POST_INDEX:
        return emu_ldrsw_x_gpr_post_index;
    case ARM64_INST_LDRB_GPR_PRE_INDEX:
        return emu_ldrb_gpr_pre_index;
    case ARM64_INST_LDRH_GPR_PRE_INDEX:
        return emu_ldrh_gpr_pre_index;
    case ARM64_INST_LDR_GPR_PRE_INDEX:
        return decoded->operand_width == 32 ? emu_ldrw_gpr_pre_index : emu_ldrx_gpr_pre_index;
    case ARM64_INST_LDRSB_GPR_PRE_INDEX:
        return decoded->operand_width == 32 ? emu_ldrsb_w_gpr_pre_index : emu_ldrsb_x_gpr_pre_index;
    case ARM64_INST_LDRSH_GPR_PRE_INDEX:
        return decoded->operand_width == 32 ? emu_ldrsh_w_gpr_pre_index : emu_ldrsh_x_gpr_pre_index;
    case ARM64_INST_LDRSW_GPR_PRE_INDEX:
        return emu_ldrsw_x_gpr_pre_index;
    case ARM64_INST_LDRB_GPR_UNSIGNED_OFFSET:
        return emu_ldrb_gpr_unsigned_offset;
    case ARM64_INST_LDRH_GPR_UNSIGNED_OFFSET:
        return emu_ldrh_gpr_unsigned_offset;
    case ARM64_INST_LDR_GPR_UNSIGNED_OFFSET:
        return decoded->operand_width == 32 ? emu_ldrw_gpr_unsigned_offset : emu_ldrx_gpr_unsigned_offset;
    case ARM64_INST_LDRSB_GPR_UNSIGNED_OFFSET:
        return decoded->operand_width == 32 ? emu_ldrsb_w_gpr_unsigned_offset : emu_ldrsb_x_gpr_unsigned_offset;
    case ARM64_INST_LDRSH_GPR_UNSIGNED_OFFSET:
        return decoded->operand_width == 32 ? emu_ldrsh_w_gpr_unsigned_offset : emu_ldrsh_x_gpr_unsigned_offset;
    case ARM64_INST_LDRSW_GPR_UNSIGNED_OFFSET:
        return emu_ldrsw_x_gpr_unsigned_offset;
    case ARM64_INST_LDRB_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_ldrb_gpr_register_offset_uxtw;
        case 3:
            return emu_ldrb_gpr_register_offset_uxtx;
        case 6:
            return emu_ldrb_gpr_register_offset_sxtw;
        case 7:
            return emu_ldrb_gpr_register_offset_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_LDRH_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_ldrh_gpr_register_offset_uxtw;
        case 3:
            return emu_ldrh_gpr_register_offset_uxtx;
        case 6:
            return emu_ldrh_gpr_register_offset_sxtw;
        case 7:
            return emu_ldrh_gpr_register_offset_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_LDR_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return decoded->operand_width == 32 ? emu_ldrw_gpr_register_offset_uxtw : emu_ldrx_gpr_register_offset_uxtw;
        case 3:
            return decoded->operand_width == 32 ? emu_ldrw_gpr_register_offset_uxtx : emu_ldrx_gpr_register_offset_uxtx;
        case 6:
            return decoded->operand_width == 32 ? emu_ldrw_gpr_register_offset_sxtw : emu_ldrx_gpr_register_offset_sxtw;
        case 7:
            return decoded->operand_width == 32 ? emu_ldrw_gpr_register_offset_sxtx : emu_ldrx_gpr_register_offset_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_LDRSB_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return decoded->operand_width == 32 ? emu_ldrsb_w_gpr_register_offset_uxtw : emu_ldrsb_x_gpr_register_offset_uxtw;
        case 3:
            return decoded->operand_width == 32 ? emu_ldrsb_w_gpr_register_offset_uxtx : emu_ldrsb_x_gpr_register_offset_uxtx;
        case 6:
            return decoded->operand_width == 32 ? emu_ldrsb_w_gpr_register_offset_sxtw : emu_ldrsb_x_gpr_register_offset_sxtw;
        case 7:
            return decoded->operand_width == 32 ? emu_ldrsb_w_gpr_register_offset_sxtx : emu_ldrsb_x_gpr_register_offset_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_LDRSH_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return decoded->operand_width == 32 ? emu_ldrsh_w_gpr_register_offset_uxtw : emu_ldrsh_x_gpr_register_offset_uxtw;
        case 3:
            return decoded->operand_width == 32 ? emu_ldrsh_w_gpr_register_offset_uxtx : emu_ldrsh_x_gpr_register_offset_uxtx;
        case 6:
            return decoded->operand_width == 32 ? emu_ldrsh_w_gpr_register_offset_sxtw : emu_ldrsh_x_gpr_register_offset_sxtw;
        case 7:
            return decoded->operand_width == 32 ? emu_ldrsh_w_gpr_register_offset_sxtx : emu_ldrsh_x_gpr_register_offset_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_LDRSW_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_ldrsw_x_gpr_register_offset_uxtw;
        case 3:
            return emu_ldrsw_x_gpr_register_offset_uxtx;
        case 6:
            return emu_ldrsw_x_gpr_register_offset_sxtw;
        case 7:
            return emu_ldrsw_x_gpr_register_offset_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_STUR_FP_SIMD:
        switch (decoded->operand_width)
        {
        case 8: return emu_sturb_fp_simd;
        case 16: return emu_sturh_fp_simd;
        case 32: return emu_sturs_fp_simd;
        case 64: return emu_sturd_fp_simd;
        case 128: return emu_sturq_fp_simd;
        default: return 0;
        }
    case ARM64_INST_LDUR_FP_SIMD:
        switch (decoded->operand_width)
        {
        case 8: return emu_ldurb_fp_simd;
        case 16: return emu_ldurh_fp_simd;
        case 32: return emu_ldurs_fp_simd;
        case 64: return emu_ldurd_fp_simd;
        case 128: return emu_ldurq_fp_simd;
        default: return 0;
        }
    case ARM64_INST_STR_FP_SIMD_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 8: return emu_strb_fp_simd_post_index;
        case 16: return emu_strh_fp_simd_post_index;
        case 32: return emu_strs_fp_simd_post_index;
        case 64: return emu_strd_fp_simd_post_index;
        case 128: return emu_strq_fp_simd_post_index;
        default: return 0;
        }
    case ARM64_INST_LDR_FP_SIMD_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 8: return emu_ldrb_fp_simd_post_index;
        case 16: return emu_ldrh_fp_simd_post_index;
        case 32: return emu_ldrs_fp_simd_post_index;
        case 64: return emu_ldrd_fp_simd_post_index;
        case 128: return emu_ldrq_fp_simd_post_index;
        default: return 0;
        }
    case ARM64_INST_STR_FP_SIMD_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 8: return emu_strb_fp_simd_pre_index;
        case 16: return emu_strh_fp_simd_pre_index;
        case 32: return emu_strs_fp_simd_pre_index;
        case 64: return emu_strd_fp_simd_pre_index;
        case 128: return emu_strq_fp_simd_pre_index;
        default: return 0;
        }
    case ARM64_INST_LDR_FP_SIMD_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 8: return emu_ldrb_fp_simd_pre_index;
        case 16: return emu_ldrh_fp_simd_pre_index;
        case 32: return emu_ldrs_fp_simd_pre_index;
        case 64: return emu_ldrd_fp_simd_pre_index;
        case 128: return emu_ldrq_fp_simd_pre_index;
        default: return 0;
        }
    case ARM64_INST_STR_FP_SIMD_UNSIGNED_OFFSET:
        switch (decoded->operand_width)
        {
        case 8: return emu_strb_fp_simd_unsigned_offset;
        case 16: return emu_strh_fp_simd_unsigned_offset;
        case 32: return emu_strs_fp_simd_unsigned_offset;
        case 64: return emu_strd_fp_simd_unsigned_offset;
        case 128: return emu_strq_fp_simd_unsigned_offset;
        default: return 0;
        }
    case ARM64_INST_LDR_FP_SIMD_UNSIGNED_OFFSET:
        switch (decoded->operand_width)
        {
        case 8: return emu_ldrb_fp_simd_unsigned_offset;
        case 16: return emu_ldrh_fp_simd_unsigned_offset;
        case 32: return emu_ldrs_fp_simd_unsigned_offset;
        case 64: return emu_ldrd_fp_simd_unsigned_offset;
        case 128: return emu_ldrq_fp_simd_unsigned_offset;
        default: return 0;
        }
    case ARM64_INST_STR_FP_SIMD_REGISTER_OFFSET:
        switch (decoded->operand_width)
        {
        case 8:
            switch (decoded->extend_type) { case 2: return emu_strb_fp_simd_register_offset_uxtw; case 3: return emu_strb_fp_simd_register_offset_uxtx; case 6: return emu_strb_fp_simd_register_offset_sxtw; case 7: return emu_strb_fp_simd_register_offset_sxtx; default: return 0; }
        case 16:
            switch (decoded->extend_type) { case 2: return emu_strh_fp_simd_register_offset_uxtw; case 3: return emu_strh_fp_simd_register_offset_uxtx; case 6: return emu_strh_fp_simd_register_offset_sxtw; case 7: return emu_strh_fp_simd_register_offset_sxtx; default: return 0; }
        case 32:
            switch (decoded->extend_type) { case 2: return emu_strs_fp_simd_register_offset_uxtw; case 3: return emu_strs_fp_simd_register_offset_uxtx; case 6: return emu_strs_fp_simd_register_offset_sxtw; case 7: return emu_strs_fp_simd_register_offset_sxtx; default: return 0; }
        case 64:
            switch (decoded->extend_type) { case 2: return emu_strd_fp_simd_register_offset_uxtw; case 3: return emu_strd_fp_simd_register_offset_uxtx; case 6: return emu_strd_fp_simd_register_offset_sxtw; case 7: return emu_strd_fp_simd_register_offset_sxtx; default: return 0; }
        case 128:
            switch (decoded->extend_type) { case 2: return emu_strq_fp_simd_register_offset_uxtw; case 3: return emu_strq_fp_simd_register_offset_uxtx; case 6: return emu_strq_fp_simd_register_offset_sxtw; case 7: return emu_strq_fp_simd_register_offset_sxtx; default: return 0; }
        default:
            return 0;
        }
    case ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET:
        switch (decoded->operand_width)
        {
        case 8:
            switch (decoded->extend_type) { case 2: return emu_ldrb_fp_simd_register_offset_uxtw; case 3: return emu_ldrb_fp_simd_register_offset_uxtx; case 6: return emu_ldrb_fp_simd_register_offset_sxtw; case 7: return emu_ldrb_fp_simd_register_offset_sxtx; default: return 0; }
        case 16:
            switch (decoded->extend_type) { case 2: return emu_ldrh_fp_simd_register_offset_uxtw; case 3: return emu_ldrh_fp_simd_register_offset_uxtx; case 6: return emu_ldrh_fp_simd_register_offset_sxtw; case 7: return emu_ldrh_fp_simd_register_offset_sxtx; default: return 0; }
        case 32:
            switch (decoded->extend_type) { case 2: return emu_ldrs_fp_simd_register_offset_uxtw; case 3: return emu_ldrs_fp_simd_register_offset_uxtx; case 6: return emu_ldrs_fp_simd_register_offset_sxtw; case 7: return emu_ldrs_fp_simd_register_offset_sxtx; default: return 0; }
        case 64:
            switch (decoded->extend_type) { case 2: return emu_ldrd_fp_simd_register_offset_uxtw; case 3: return emu_ldrd_fp_simd_register_offset_uxtx; case 6: return emu_ldrd_fp_simd_register_offset_sxtw; case 7: return emu_ldrd_fp_simd_register_offset_sxtx; default: return 0; }
        case 128:
            switch (decoded->extend_type) { case 2: return emu_ldrq_fp_simd_register_offset_uxtw; case 3: return emu_ldrq_fp_simd_register_offset_uxtx; case 6: return emu_ldrq_fp_simd_register_offset_sxtw; case 7: return emu_ldrq_fp_simd_register_offset_sxtx; default: return 0; }
        default:
            return 0;
        }
    case ARM64_INST_LDXRB:
        return emu_ldxrb;
    case ARM64_INST_LDXRH:
        return emu_ldxrh;
    case ARM64_INST_LDXR:
        return decoded->operand_width == 32 ? emu_ldxrw : emu_ldxrx;
    case ARM64_INST_LDAXRB:
        return emu_ldaxrb;
    case ARM64_INST_LDAXRH:
        return emu_ldaxrh;
    case ARM64_INST_LDAXR:
        return decoded->operand_width == 32 ? emu_ldaxrw : emu_ldaxrx;
    case ARM64_INST_LDXP:
        return decoded->operand_width == 32 ? emu_ldxpw : emu_ldxpx;
    case ARM64_INST_LDAXP:
        return decoded->operand_width == 32 ? emu_ldaxpw : emu_ldaxpx;
    case ARM64_INST_STXRB:
        return emu_stxrb;
    case ARM64_INST_STXRH:
        return emu_stxrh;
    case ARM64_INST_STXR:
        return decoded->operand_width == 32 ? emu_stxrw : emu_stxrx;
    case ARM64_INST_STLXRB:
        return emu_stlxrb;
    case ARM64_INST_STLXRH:
        return emu_stlxrh;
    case ARM64_INST_STLXR:
        return decoded->operand_width == 32 ? emu_stlxrw : emu_stlxrx;
    case ARM64_INST_STXP:
        return decoded->operand_width == 32 ? emu_stxpw : emu_stxpx;
    case ARM64_INST_STLXP:
        return decoded->operand_width == 32 ? emu_stlxpw : emu_stlxpx;
    case ARM64_INST_CASP:
        return decoded->operand_width == 32 ? emu_caspw : emu_caspx;
    case ARM64_INST_CASPA:
        return decoded->operand_width == 32 ? emu_caspaw : emu_caspax;
    case ARM64_INST_CASPL:
        return decoded->operand_width == 32 ? emu_casplw : emu_casplx;
    case ARM64_INST_CASPAL:
        return decoded->operand_width == 32 ? emu_caspalw : emu_caspalx;
    case ARM64_INST_CASB:
        return emu_casb;
    case ARM64_INST_CASH:
        return emu_cash;
    case ARM64_INST_CAS:
        return decoded->operand_width == 32 ? emu_casw : emu_casx;
    case ARM64_INST_CASAB:
        return emu_casab;
    case ARM64_INST_CASAH:
        return emu_casah;
    case ARM64_INST_CASA:
        return decoded->operand_width == 32 ? emu_casaw : emu_casax;
    case ARM64_INST_CASLB:
        return emu_caslb;
    case ARM64_INST_CASLH:
        return emu_caslh;
    case ARM64_INST_CASL:
        return decoded->operand_width == 32 ? emu_caslw : emu_caslx;
    case ARM64_INST_CASALB:
        return emu_casalb;
    case ARM64_INST_CASALH:
        return emu_casalh;
    case ARM64_INST_CASAL:
        return decoded->operand_width == 32 ? emu_casalw : emu_casalx;
    case ARM64_INST_LDADDALB:
        return emu_ldaddal_b;
    case ARM64_INST_LDADDALH:
        return emu_ldaddal_h;
    case ARM64_INST_LDADDAL:
        return decoded->operand_width == 32 ? emu_ldaddal_w : emu_ldaddal_x;
    case ARM64_INST_STLLR:
        return decoded->operand_width == 32 ? emu_stllr_w : emu_stllr_x;
    case ARM64_INST_STLRH:
        return emu_stlr_h;
    case ARM64_INST_STLR:
        return decoded->operand_width == 32 ? emu_stlr_w : emu_stlr_x;
    case ARM64_INST_LDLAR:
        return decoded->operand_width == 32 ? emu_ldlar_w : emu_ldlar_x;
    case ARM64_INST_LDAR:
        return decoded->operand_width == 32 ? emu_ldar_w : emu_ldar_x;
    case ARM64_INST_LDAPRH:
        return emu_ldapr_h;
    case ARM64_INST_LDAPR:
        return decoded->operand_width == 32 ? emu_ldapr_w : emu_ldapr_x;
    case ARM64_INST_STLUR:
        return decoded->operand_width == 32 ? emu_stlur_w : emu_stlur_x;
    case ARM64_INST_LDAPUR:
        return decoded->operand_width == 32 ? emu_ldapur_w : emu_ldapur_x;
    case ARM64_INST_LDAPURSB:
        return decoded->operand_width == 32 ? emu_ldapursb_w : emu_ldapursb_x;
    case ARM64_INST_LDAPURSH:
        return decoded->operand_width == 32 ? emu_ldapursh_w : emu_ldapursh_x;
    case ARM64_INST_LDAPURSW:
        return emu_ldapursw_x;
    case ARM64_INST_LDR_GPR_LITERAL:
        return decoded->operand_width == 32 ? emu_ldrw_literal_gpr : emu_ldrx_literal_gpr;
    case ARM64_INST_LDRSW_LITERAL:
        return emu_ldrsw_literal;
    case ARM64_INST_LDR_FP_SIMD_LITERAL:
        switch (decoded->operand_width) { case 32: return emu_ldrs_literal_fp_simd; case 64: return emu_ldrd_literal_fp_simd; case 128: return emu_ldrq_literal_fp_simd; default: return 0; }
    case ARM64_INST_LDNP_GPR:
        return decoded->operand_width == 32 ? emu_ldnpw_gpr : emu_ldnpx_gpr;
    case ARM64_INST_LDP_GPR_OFFSET:
        return decoded->operand_width == 32 ? emu_ldpw_gpr_offset : emu_ldpx_gpr_offset;
    case ARM64_INST_LDPSW_OFFSET:
        return emu_ldpsw_gpr_offset;
    case ARM64_INST_LDP_GPR_POST_INDEX:
        return decoded->operand_width == 32 ? emu_ldpw_gpr_post_index : emu_ldpx_gpr_post_index;
    case ARM64_INST_LDPSW_POST_INDEX:
        return emu_ldpsw_gpr_post_index;
    case ARM64_INST_LDP_GPR_PRE_INDEX:
        return decoded->operand_width == 32 ? emu_ldpw_gpr_pre_index : emu_ldpx_gpr_pre_index;
    case ARM64_INST_LDPSW_PRE_INDEX:
        return emu_ldpsw_gpr_pre_index;
    case ARM64_INST_STNP_GPR:
        return decoded->operand_width == 32 ? emu_stnpw_gpr : emu_stnpx_gpr;
    case ARM64_INST_STP_GPR_OFFSET:
        return decoded->operand_width == 32 ? emu_stpw_gpr_offset : emu_stpx_gpr_offset;
    case ARM64_INST_STP_GPR_POST_INDEX:
        return decoded->operand_width == 32 ? emu_stpw_gpr_post_index : emu_stpx_gpr_post_index;
    case ARM64_INST_STP_GPR_PRE_INDEX:
        return decoded->operand_width == 32 ? emu_stpw_gpr_pre_index : emu_stpx_gpr_pre_index;
    case ARM64_INST_LDNP_FP_SIMD:
        switch (decoded->operand_width) { case 32: return emu_ldnps_fp_simd; case 64: return emu_ldnpd_fp_simd; case 128: return emu_ldnpq_fp_simd; default: return 0; }
    case ARM64_INST_LDP_FP_SIMD_OFFSET:
        switch (decoded->operand_width) { case 32: return emu_ldps_fp_simd_offset; case 64: return emu_ldpd_fp_simd_offset; case 128: return emu_ldpq_fp_simd_offset; default: return 0; }
    case ARM64_INST_LDP_FP_SIMD_POST_INDEX:
        switch (decoded->operand_width) { case 32: return emu_ldps_fp_simd_post_index; case 64: return emu_ldpd_fp_simd_post_index; case 128: return emu_ldpq_fp_simd_post_index; default: return 0; }
    case ARM64_INST_LDP_FP_SIMD_PRE_INDEX:
        switch (decoded->operand_width) { case 32: return emu_ldps_fp_simd_pre_index; case 64: return emu_ldpd_fp_simd_pre_index; case 128: return emu_ldpq_fp_simd_pre_index; default: return 0; }
    case ARM64_INST_STNP_FP_SIMD:
        switch (decoded->operand_width) { case 32: return emu_stnps_fp_simd; case 64: return emu_stnpd_fp_simd; case 128: return emu_stnpq_fp_simd; default: return 0; }
    case ARM64_INST_STP_FP_SIMD_OFFSET:
        switch (decoded->operand_width) { case 32: return emu_stps_fp_simd_offset; case 64: return emu_stpd_fp_simd_offset; case 128: return emu_stpq_fp_simd_offset; default: return 0; }
    case ARM64_INST_STP_FP_SIMD_POST_INDEX:
        switch (decoded->operand_width) { case 32: return emu_stps_fp_simd_post_index; case 64: return emu_stpd_fp_simd_post_index; case 128: return emu_stpq_fp_simd_post_index; default: return 0; }
    case ARM64_INST_STP_FP_SIMD_PRE_INDEX:
        switch (decoded->operand_width) { case 32: return emu_stps_fp_simd_pre_index; case 64: return emu_stpd_fp_simd_pre_index; case 128: return emu_stpq_fp_simd_pre_index; default: return 0; }
    case ARM64_INST_PRFM_LITERAL:
        switch (decoded->immediate)
        {
        case 0:
            return emu_prfm_literal_op0;
        case 1:
            return emu_prfm_literal_op1;
        case 2:
            return emu_prfm_literal_op2;
        case 3:
            return emu_prfm_literal_op3;
        case 4:
            return emu_prfm_literal_op4;
        case 5:
            return emu_prfm_literal_op5;
        case 6:
            return emu_prfm_literal_op6;
        case 7:
            return emu_prfm_literal_op7;
        case 8:
            return emu_prfm_literal_op8;
        case 9:
            return emu_prfm_literal_op9;
        case 10:
            return emu_prfm_literal_op10;
        case 11:
            return emu_prfm_literal_op11;
        case 12:
            return emu_prfm_literal_op12;
        case 13:
            return emu_prfm_literal_op13;
        case 14:
            return emu_prfm_literal_op14;
        case 15:
            return emu_prfm_literal_op15;
        case 16:
            return emu_prfm_literal_op16;
        case 17:
            return emu_prfm_literal_op17;
        case 18:
            return emu_prfm_literal_op18;
        case 19:
            return emu_prfm_literal_op19;
        case 20:
            return emu_prfm_literal_op20;
        case 21:
            return emu_prfm_literal_op21;
        case 22:
            return emu_prfm_literal_op22;
        case 23:
            return emu_prfm_literal_op23;
        case 24:
            return emu_prfm_literal_op24;
        case 25:
            return emu_prfm_literal_op25;
        case 26:
            return emu_prfm_literal_op26;
        case 27:
            return emu_prfm_literal_op27;
        case 28:
            return emu_prfm_literal_op28;
        case 29:
            return emu_prfm_literal_op29;
        case 30:
            return emu_prfm_literal_op30;
        case 31:
            return emu_prfm_literal_op31;
        default:
            return 0;
        }
    case ARM64_INST_PRFM_UNSIGNED_OFFSET:
    case ARM64_INST_PRFUM:
        switch (decoded->immediate)
        {
        case 0:
            return emu_prfm_unsigned_offset_op0;
        case 1:
            return emu_prfm_unsigned_offset_op1;
        case 2:
            return emu_prfm_unsigned_offset_op2;
        case 3:
            return emu_prfm_unsigned_offset_op3;
        case 4:
            return emu_prfm_unsigned_offset_op4;
        case 5:
            return emu_prfm_unsigned_offset_op5;
        case 6:
            return emu_prfm_unsigned_offset_op6;
        case 7:
            return emu_prfm_unsigned_offset_op7;
        case 8:
            return emu_prfm_unsigned_offset_op8;
        case 9:
            return emu_prfm_unsigned_offset_op9;
        case 10:
            return emu_prfm_unsigned_offset_op10;
        case 11:
            return emu_prfm_unsigned_offset_op11;
        case 12:
            return emu_prfm_unsigned_offset_op12;
        case 13:
            return emu_prfm_unsigned_offset_op13;
        case 14:
            return emu_prfm_unsigned_offset_op14;
        case 15:
            return emu_prfm_unsigned_offset_op15;
        case 16:
            return emu_prfm_unsigned_offset_op16;
        case 17:
            return emu_prfm_unsigned_offset_op17;
        case 18:
            return emu_prfm_unsigned_offset_op18;
        case 19:
            return emu_prfm_unsigned_offset_op19;
        case 20:
            return emu_prfm_unsigned_offset_op20;
        case 21:
            return emu_prfm_unsigned_offset_op21;
        case 22:
            return emu_prfm_unsigned_offset_op22;
        case 23:
            return emu_prfm_unsigned_offset_op23;
        case 24:
            return emu_prfm_unsigned_offset_op24;
        case 25:
            return emu_prfm_unsigned_offset_op25;
        case 26:
            return emu_prfm_unsigned_offset_op26;
        case 27:
            return emu_prfm_unsigned_offset_op27;
        case 28:
            return emu_prfm_unsigned_offset_op28;
        case 29:
            return emu_prfm_unsigned_offset_op29;
        case 30:
            return emu_prfm_unsigned_offset_op30;
        case 31:
            return emu_prfm_unsigned_offset_op31;
        default:
            return 0;
        }
    case ARM64_INST_PRFM_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            switch (decoded->immediate)
            {
            case 0:
                return emu_prfm_register_offset_uxtw_op0;
            case 1:
                return emu_prfm_register_offset_uxtw_op1;
            case 2:
                return emu_prfm_register_offset_uxtw_op2;
            case 3:
                return emu_prfm_register_offset_uxtw_op3;
            case 4:
                return emu_prfm_register_offset_uxtw_op4;
            case 5:
                return emu_prfm_register_offset_uxtw_op5;
            case 6:
                return emu_prfm_register_offset_uxtw_op6;
            case 7:
                return emu_prfm_register_offset_uxtw_op7;
            case 8:
                return emu_prfm_register_offset_uxtw_op8;
            case 9:
                return emu_prfm_register_offset_uxtw_op9;
            case 10:
                return emu_prfm_register_offset_uxtw_op10;
            case 11:
                return emu_prfm_register_offset_uxtw_op11;
            case 12:
                return emu_prfm_register_offset_uxtw_op12;
            case 13:
                return emu_prfm_register_offset_uxtw_op13;
            case 14:
                return emu_prfm_register_offset_uxtw_op14;
            case 15:
                return emu_prfm_register_offset_uxtw_op15;
            case 16:
                return emu_prfm_register_offset_uxtw_op16;
            case 17:
                return emu_prfm_register_offset_uxtw_op17;
            case 18:
                return emu_prfm_register_offset_uxtw_op18;
            case 19:
                return emu_prfm_register_offset_uxtw_op19;
            case 20:
                return emu_prfm_register_offset_uxtw_op20;
            case 21:
                return emu_prfm_register_offset_uxtw_op21;
            case 22:
                return emu_prfm_register_offset_uxtw_op22;
            case 23:
                return emu_prfm_register_offset_uxtw_op23;
            case 24:
                return emu_prfm_register_offset_uxtw_op24;
            case 25:
                return emu_prfm_register_offset_uxtw_op25;
            case 26:
                return emu_prfm_register_offset_uxtw_op26;
            case 27:
                return emu_prfm_register_offset_uxtw_op27;
            case 28:
                return emu_prfm_register_offset_uxtw_op28;
            case 29:
                return emu_prfm_register_offset_uxtw_op29;
            case 30:
                return emu_prfm_register_offset_uxtw_op30;
            case 31:
                return emu_prfm_register_offset_uxtw_op31;
            default:
                return 0;
            }
        case 3:
        case 7:
            switch (decoded->immediate)
            {
            case 0:
                return emu_prfm_register_offset_uxtx_op0;
            case 1:
                return emu_prfm_register_offset_uxtx_op1;
            case 2:
                return emu_prfm_register_offset_uxtx_op2;
            case 3:
                return emu_prfm_register_offset_uxtx_op3;
            case 4:
                return emu_prfm_register_offset_uxtx_op4;
            case 5:
                return emu_prfm_register_offset_uxtx_op5;
            case 6:
                return emu_prfm_register_offset_uxtx_op6;
            case 7:
                return emu_prfm_register_offset_uxtx_op7;
            case 8:
                return emu_prfm_register_offset_uxtx_op8;
            case 9:
                return emu_prfm_register_offset_uxtx_op9;
            case 10:
                return emu_prfm_register_offset_uxtx_op10;
            case 11:
                return emu_prfm_register_offset_uxtx_op11;
            case 12:
                return emu_prfm_register_offset_uxtx_op12;
            case 13:
                return emu_prfm_register_offset_uxtx_op13;
            case 14:
                return emu_prfm_register_offset_uxtx_op14;
            case 15:
                return emu_prfm_register_offset_uxtx_op15;
            case 16:
                return emu_prfm_register_offset_uxtx_op16;
            case 17:
                return emu_prfm_register_offset_uxtx_op17;
            case 18:
                return emu_prfm_register_offset_uxtx_op18;
            case 19:
                return emu_prfm_register_offset_uxtx_op19;
            case 20:
                return emu_prfm_register_offset_uxtx_op20;
            case 21:
                return emu_prfm_register_offset_uxtx_op21;
            case 22:
                return emu_prfm_register_offset_uxtx_op22;
            case 23:
                return emu_prfm_register_offset_uxtx_op23;
            case 24:
                return emu_prfm_register_offset_uxtx_op24;
            case 25:
                return emu_prfm_register_offset_uxtx_op25;
            case 26:
                return emu_prfm_register_offset_uxtx_op26;
            case 27:
                return emu_prfm_register_offset_uxtx_op27;
            case 28:
                return emu_prfm_register_offset_uxtx_op28;
            case 29:
                return emu_prfm_register_offset_uxtx_op29;
            case 30:
                return emu_prfm_register_offset_uxtx_op30;
            case 31:
                return emu_prfm_register_offset_uxtx_op31;
            default:
                return 0;
            }
        case 6:
            switch (decoded->immediate)
            {
            case 0:
                return emu_prfm_register_offset_sxtw_op0;
            case 1:
                return emu_prfm_register_offset_sxtw_op1;
            case 2:
                return emu_prfm_register_offset_sxtw_op2;
            case 3:
                return emu_prfm_register_offset_sxtw_op3;
            case 4:
                return emu_prfm_register_offset_sxtw_op4;
            case 5:
                return emu_prfm_register_offset_sxtw_op5;
            case 6:
                return emu_prfm_register_offset_sxtw_op6;
            case 7:
                return emu_prfm_register_offset_sxtw_op7;
            case 8:
                return emu_prfm_register_offset_sxtw_op8;
            case 9:
                return emu_prfm_register_offset_sxtw_op9;
            case 10:
                return emu_prfm_register_offset_sxtw_op10;
            case 11:
                return emu_prfm_register_offset_sxtw_op11;
            case 12:
                return emu_prfm_register_offset_sxtw_op12;
            case 13:
                return emu_prfm_register_offset_sxtw_op13;
            case 14:
                return emu_prfm_register_offset_sxtw_op14;
            case 15:
                return emu_prfm_register_offset_sxtw_op15;
            case 16:
                return emu_prfm_register_offset_sxtw_op16;
            case 17:
                return emu_prfm_register_offset_sxtw_op17;
            case 18:
                return emu_prfm_register_offset_sxtw_op18;
            case 19:
                return emu_prfm_register_offset_sxtw_op19;
            case 20:
                return emu_prfm_register_offset_sxtw_op20;
            case 21:
                return emu_prfm_register_offset_sxtw_op21;
            case 22:
                return emu_prfm_register_offset_sxtw_op22;
            case 23:
                return emu_prfm_register_offset_sxtw_op23;
            case 24:
                return emu_prfm_register_offset_sxtw_op24;
            case 25:
                return emu_prfm_register_offset_sxtw_op25;
            case 26:
                return emu_prfm_register_offset_sxtw_op26;
            case 27:
                return emu_prfm_register_offset_sxtw_op27;
            case 28:
                return emu_prfm_register_offset_sxtw_op28;
            case 29:
                return emu_prfm_register_offset_sxtw_op29;
            case 30:
                return emu_prfm_register_offset_sxtw_op30;
            case 31:
                return emu_prfm_register_offset_sxtw_op31;
            default:
                return 0;
            }
        default:
            return 0;
        }
    default:
        return NULL;
    }
}
// clang-format on
