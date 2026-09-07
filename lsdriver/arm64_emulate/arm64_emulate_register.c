// clang-format off
#include "emulate_inst.h"
/* ======================== 数据处理寄存器类：缓存条目执行模板 ======================== */
/* 每个固定寄存器执行模板直接对应缓存条目中的 execute 函数地址。 */
static inline enum emu_inst_result emu_and_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    and_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_and_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    and_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_and_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    and_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_and_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    and_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_and_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    and_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_and_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    and_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_and_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    and_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_and_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    and_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bic_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bic_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bic_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bic_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bic_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bic_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bic_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bic_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bic_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bic_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bic_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bic_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bic_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bic_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bic_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bic_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orr_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orr_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orr_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orr_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orr_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orr_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orr_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orr_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orr_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orn_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orn_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orn_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orn_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orn_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orn_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orn_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orn_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orn_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orn_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orn_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orn_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orn_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orn_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_orn_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    orn_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eor_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eor_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eor_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eor_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eor_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eor_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eor_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eor_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eor_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eon_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eon_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eon_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eon_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eon_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eon_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eon_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eon_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eon_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eon_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eon_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eon_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eon_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eon_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_eon_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    eon_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    ands_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    ands_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    ands_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    ands_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    ands_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    ands_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    ands_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ands_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    ands_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bics_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bics_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bics_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bics_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bics_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bics_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bics_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bics_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bics_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bics_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bics_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bics_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bics_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bics_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_bics_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    bics_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w32_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w64_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w32_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w64_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w32_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w64_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w32_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w64_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w32_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w64_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w32_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w64_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w32_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w64_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w32_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_add_extended_register_w64_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    add_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w32_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w64_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w32_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w64_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w32_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w64_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w32_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w64_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w32_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w64_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w32_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w64_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w32_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w64_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w32_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adds_extended_register_w64_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    adds_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w32_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w64_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w32_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w64_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w32_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w64_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w32_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w64_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w32_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w64_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w32_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w64_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w32_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w64_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w32_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sub_extended_register_w64_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    sub_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_gpr_or_sp(regs, entry->decoded.rd, output.value0);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w32_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w64_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w32_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w64_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w32_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w64_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w32_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w64_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    uxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w32_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w64_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtb_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w32_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w64_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxth_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w32_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w64_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtw_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w32_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w32(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_subs_extended_register_w64_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sxtx_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &output);
    subs_w64(read_gpr_or_sp(regs, entry->decoded.rn), output.value0, 0, 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adc_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    adc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adc_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    adc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adcs_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    adcs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_adcs_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    adcs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sbc_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sbc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sbc_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sbc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sbcs_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sbcs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sbcs_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sbcs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, &output);
    write_nzcv(regs, output.value1);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w32_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csel_w64_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csel_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w32_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinc_w64_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinc_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w32_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csinv_w64_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csinv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w32_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_csneg_w64_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    csneg_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_udiv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    udiv_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_udiv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    udiv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sdiv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sdiv_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_sdiv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    sdiv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_lslv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_lslv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lslv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_lsrv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_lsrv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    lsrv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_asrv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_asrv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    asrv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rorv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rorv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rorv_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32b_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32b(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32b_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32b(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32h_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32h(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32h_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32h(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32w_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32w(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32w_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32x_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32x(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32x_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32cb_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32cb(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32cb_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32cb(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32ch_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32ch(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32ch_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32ch(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32cw_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32cw(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32cw_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32cw(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32cx_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32cx(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_crc32cx_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    crc32cx(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smax_register_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smax_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smax_register_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smax_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umax_register_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umax_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umax_register_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umax_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smin_register_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smin_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smin_register_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smin_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umin_register_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umin_w32(read_gpr_or_zr(regs, entry->decoded.rn) & 0x00000000ffffffffULL, read_gpr_or_zr(regs, entry->decoded.rm) & 0x00000000ffffffffULL, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umin_register_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umin_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_madd_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    madd_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_madd_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    madd_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_msub_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    msub_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_msub_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    msub_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smaddl_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smaddl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smaddl_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smaddl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smsubl_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smsubl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smsubl_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smsubl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smulh_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smulh(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_smulh_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    smulh(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umaddl_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umaddl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umaddl_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umaddl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umsubl_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umsubl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umsubl_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umsubl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umulh_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umulh(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_umulh_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    umulh(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w32_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_register_w64_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w32_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_register_w64_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w32_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmn_immediate_w64_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        adds_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w32_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w32(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ccmp_immediate_w64_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        struct arm64_hw_template_output output __attribute__((__uninitialized__));
        subs_w64(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &output);
        flags = output.value1;
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rbit_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rbit_w32(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rbit_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rbit_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rev16_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rev16_w32(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rev16_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rev16_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rev32_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rev32_w32(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rev32_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rev32_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rev64_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rev64_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_rev64_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rev64_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    clz_w32(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_clz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    clz_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_cls_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    cls_w32(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_cls_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    cls_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ctz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rbit_w32(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    clz_w32(output.value0, 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_ctz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    rbit_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    clz_w64(output.value0, 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_cnt_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    cnt_w32(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_cnt_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    cnt_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_abs_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    abs_w32(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, false);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
static inline enum emu_inst_result emu_abs_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    struct arm64_hw_template_output output __attribute__((__uninitialized__));
    abs_w64(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, &output);
    write_gpr_or_zr(regs, entry->decoded.rd, output.value0, true);
    regs->pc += 4;
    return EMU_INST_HANDLED;
}
enum emu_inst_result (*emu_build_register_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_AND_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_and_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_and_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_and_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_and_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_and_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_and_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_and_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_and_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_BIC_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_bic_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_bic_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_bic_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_bic_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_bic_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_bic_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_bic_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_bic_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_ORR_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_orr_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_orr_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_orr_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_orr_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_orr_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_orr_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_orr_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_orr_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_ORN_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_orn_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_orn_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_orn_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_orn_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_orn_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_orn_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_orn_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_orn_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_EOR_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_eor_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_eor_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_eor_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_eor_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_eor_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_eor_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_eor_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_eor_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_EON_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_eon_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_eon_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_eon_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_eon_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_eon_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_eon_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_eon_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_eon_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_ANDS_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_ands_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_ands_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_ands_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_ands_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_ands_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_ands_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_ands_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_ands_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_BICS_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_bics_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_bics_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_bics_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_bics_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_bics_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_bics_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_bics_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_bics_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_ADD_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_add_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_add_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_add_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_add_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_add_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_add_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_add_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_add_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_ADDS_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_adds_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_adds_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_adds_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_adds_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_adds_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_adds_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_adds_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_adds_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_SUB_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_sub_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_sub_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_sub_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_sub_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_sub_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_sub_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_sub_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_sub_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_SUBS_SHIFTED_REGISTER:
        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_subs_shifted_register_w32_shift0;
        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_subs_shifted_register_w64_shift0;
        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_subs_shifted_register_w32_shift1;
        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_subs_shifted_register_w64_shift1;
        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_subs_shifted_register_w32_shift2;
        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_subs_shifted_register_w64_shift2;
        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_subs_shifted_register_w32_shift3;
        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_subs_shifted_register_w64_shift3;
        return NULL;
    case ARM64_INST_ADD_EXTENDED_REGISTER:
        if (decoded->operand_width == 32 && decoded->extend_type == 0) return emu_add_extended_register_w32_uxtb;
        if (decoded->operand_width == 64 && decoded->extend_type == 0) return emu_add_extended_register_w64_uxtb;
        if (decoded->operand_width == 32 && decoded->extend_type == 1) return emu_add_extended_register_w32_uxth;
        if (decoded->operand_width == 64 && decoded->extend_type == 1) return emu_add_extended_register_w64_uxth;
        if (decoded->operand_width == 32 && decoded->extend_type == 2) return emu_add_extended_register_w32_uxtw;
        if (decoded->operand_width == 64 && decoded->extend_type == 2) return emu_add_extended_register_w64_uxtw;
        if (decoded->operand_width == 32 && decoded->extend_type == 3) return emu_add_extended_register_w32_uxtx;
        if (decoded->operand_width == 64 && decoded->extend_type == 3) return emu_add_extended_register_w64_uxtx;
        if (decoded->operand_width == 32 && decoded->extend_type == 4) return emu_add_extended_register_w32_sxtb;
        if (decoded->operand_width == 64 && decoded->extend_type == 4) return emu_add_extended_register_w64_sxtb;
        if (decoded->operand_width == 32 && decoded->extend_type == 5) return emu_add_extended_register_w32_sxth;
        if (decoded->operand_width == 64 && decoded->extend_type == 5) return emu_add_extended_register_w64_sxth;
        if (decoded->operand_width == 32 && decoded->extend_type == 6) return emu_add_extended_register_w32_sxtw;
        if (decoded->operand_width == 64 && decoded->extend_type == 6) return emu_add_extended_register_w64_sxtw;
        if (decoded->operand_width == 32 && decoded->extend_type == 7) return emu_add_extended_register_w32_sxtx;
        if (decoded->operand_width == 64 && decoded->extend_type == 7) return emu_add_extended_register_w64_sxtx;
        return NULL;
    case ARM64_INST_ADDS_EXTENDED_REGISTER:
        if (decoded->operand_width == 32 && decoded->extend_type == 0) return emu_adds_extended_register_w32_uxtb;
        if (decoded->operand_width == 64 && decoded->extend_type == 0) return emu_adds_extended_register_w64_uxtb;
        if (decoded->operand_width == 32 && decoded->extend_type == 1) return emu_adds_extended_register_w32_uxth;
        if (decoded->operand_width == 64 && decoded->extend_type == 1) return emu_adds_extended_register_w64_uxth;
        if (decoded->operand_width == 32 && decoded->extend_type == 2) return emu_adds_extended_register_w32_uxtw;
        if (decoded->operand_width == 64 && decoded->extend_type == 2) return emu_adds_extended_register_w64_uxtw;
        if (decoded->operand_width == 32 && decoded->extend_type == 3) return emu_adds_extended_register_w32_uxtx;
        if (decoded->operand_width == 64 && decoded->extend_type == 3) return emu_adds_extended_register_w64_uxtx;
        if (decoded->operand_width == 32 && decoded->extend_type == 4) return emu_adds_extended_register_w32_sxtb;
        if (decoded->operand_width == 64 && decoded->extend_type == 4) return emu_adds_extended_register_w64_sxtb;
        if (decoded->operand_width == 32 && decoded->extend_type == 5) return emu_adds_extended_register_w32_sxth;
        if (decoded->operand_width == 64 && decoded->extend_type == 5) return emu_adds_extended_register_w64_sxth;
        if (decoded->operand_width == 32 && decoded->extend_type == 6) return emu_adds_extended_register_w32_sxtw;
        if (decoded->operand_width == 64 && decoded->extend_type == 6) return emu_adds_extended_register_w64_sxtw;
        if (decoded->operand_width == 32 && decoded->extend_type == 7) return emu_adds_extended_register_w32_sxtx;
        if (decoded->operand_width == 64 && decoded->extend_type == 7) return emu_adds_extended_register_w64_sxtx;
        return NULL;
    case ARM64_INST_SUB_EXTENDED_REGISTER:
        if (decoded->operand_width == 32 && decoded->extend_type == 0) return emu_sub_extended_register_w32_uxtb;
        if (decoded->operand_width == 64 && decoded->extend_type == 0) return emu_sub_extended_register_w64_uxtb;
        if (decoded->operand_width == 32 && decoded->extend_type == 1) return emu_sub_extended_register_w32_uxth;
        if (decoded->operand_width == 64 && decoded->extend_type == 1) return emu_sub_extended_register_w64_uxth;
        if (decoded->operand_width == 32 && decoded->extend_type == 2) return emu_sub_extended_register_w32_uxtw;
        if (decoded->operand_width == 64 && decoded->extend_type == 2) return emu_sub_extended_register_w64_uxtw;
        if (decoded->operand_width == 32 && decoded->extend_type == 3) return emu_sub_extended_register_w32_uxtx;
        if (decoded->operand_width == 64 && decoded->extend_type == 3) return emu_sub_extended_register_w64_uxtx;
        if (decoded->operand_width == 32 && decoded->extend_type == 4) return emu_sub_extended_register_w32_sxtb;
        if (decoded->operand_width == 64 && decoded->extend_type == 4) return emu_sub_extended_register_w64_sxtb;
        if (decoded->operand_width == 32 && decoded->extend_type == 5) return emu_sub_extended_register_w32_sxth;
        if (decoded->operand_width == 64 && decoded->extend_type == 5) return emu_sub_extended_register_w64_sxth;
        if (decoded->operand_width == 32 && decoded->extend_type == 6) return emu_sub_extended_register_w32_sxtw;
        if (decoded->operand_width == 64 && decoded->extend_type == 6) return emu_sub_extended_register_w64_sxtw;
        if (decoded->operand_width == 32 && decoded->extend_type == 7) return emu_sub_extended_register_w32_sxtx;
        if (decoded->operand_width == 64 && decoded->extend_type == 7) return emu_sub_extended_register_w64_sxtx;
        return NULL;
    case ARM64_INST_SUBS_EXTENDED_REGISTER:
        if (decoded->operand_width == 32 && decoded->extend_type == 0) return emu_subs_extended_register_w32_uxtb;
        if (decoded->operand_width == 64 && decoded->extend_type == 0) return emu_subs_extended_register_w64_uxtb;
        if (decoded->operand_width == 32 && decoded->extend_type == 1) return emu_subs_extended_register_w32_uxth;
        if (decoded->operand_width == 64 && decoded->extend_type == 1) return emu_subs_extended_register_w64_uxth;
        if (decoded->operand_width == 32 && decoded->extend_type == 2) return emu_subs_extended_register_w32_uxtw;
        if (decoded->operand_width == 64 && decoded->extend_type == 2) return emu_subs_extended_register_w64_uxtw;
        if (decoded->operand_width == 32 && decoded->extend_type == 3) return emu_subs_extended_register_w32_uxtx;
        if (decoded->operand_width == 64 && decoded->extend_type == 3) return emu_subs_extended_register_w64_uxtx;
        if (decoded->operand_width == 32 && decoded->extend_type == 4) return emu_subs_extended_register_w32_sxtb;
        if (decoded->operand_width == 64 && decoded->extend_type == 4) return emu_subs_extended_register_w64_sxtb;
        if (decoded->operand_width == 32 && decoded->extend_type == 5) return emu_subs_extended_register_w32_sxth;
        if (decoded->operand_width == 64 && decoded->extend_type == 5) return emu_subs_extended_register_w64_sxth;
        if (decoded->operand_width == 32 && decoded->extend_type == 6) return emu_subs_extended_register_w32_sxtw;
        if (decoded->operand_width == 64 && decoded->extend_type == 6) return emu_subs_extended_register_w64_sxtw;
        if (decoded->operand_width == 32 && decoded->extend_type == 7) return emu_subs_extended_register_w32_sxtx;
        if (decoded->operand_width == 64 && decoded->extend_type == 7) return emu_subs_extended_register_w64_sxtx;
        return NULL;
    case ARM64_INST_ADC:
        if (decoded->operand_width == 32) return emu_adc_w32;
        if (decoded->operand_width == 64) return emu_adc_w64;
        return NULL;
    case ARM64_INST_ADCS:
        if (decoded->operand_width == 32) return emu_adcs_w32;
        if (decoded->operand_width == 64) return emu_adcs_w64;
        return NULL;
    case ARM64_INST_SBC:
        if (decoded->operand_width == 32) return emu_sbc_w32;
        if (decoded->operand_width == 64) return emu_sbc_w64;
        return NULL;
    case ARM64_INST_SBCS:
        if (decoded->operand_width == 32) return emu_sbcs_w32;
        if (decoded->operand_width == 64) return emu_sbcs_w64;
        return NULL;
    case ARM64_INST_CSEL:
        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_csel_w32_eq;
        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_csel_w32_ne;
        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_csel_w32_cs;
        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_csel_w32_cc;
        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_csel_w32_mi;
        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_csel_w32_pl;
        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_csel_w32_vs;
        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_csel_w32_vc;
        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_csel_w32_hi;
        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_csel_w32_ls;
        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_csel_w32_ge;
        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_csel_w32_lt;
        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_csel_w32_gt;
        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_csel_w32_le;
        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_csel_w32_al;
        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_csel_w32_nv;
        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_csel_w64_eq;
        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_csel_w64_ne;
        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_csel_w64_cs;
        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_csel_w64_cc;
        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_csel_w64_mi;
        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_csel_w64_pl;
        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_csel_w64_vs;
        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_csel_w64_vc;
        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_csel_w64_hi;
        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_csel_w64_ls;
        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_csel_w64_ge;
        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_csel_w64_lt;
        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_csel_w64_gt;
        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_csel_w64_le;
        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_csel_w64_al;
        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_csel_w64_nv;
        return NULL;
    case ARM64_INST_CSINC:
        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_csinc_w32_eq;
        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_csinc_w32_ne;
        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_csinc_w32_cs;
        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_csinc_w32_cc;
        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_csinc_w32_mi;
        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_csinc_w32_pl;
        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_csinc_w32_vs;
        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_csinc_w32_vc;
        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_csinc_w32_hi;
        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_csinc_w32_ls;
        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_csinc_w32_ge;
        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_csinc_w32_lt;
        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_csinc_w32_gt;
        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_csinc_w32_le;
        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_csinc_w32_al;
        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_csinc_w32_nv;
        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_csinc_w64_eq;
        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_csinc_w64_ne;
        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_csinc_w64_cs;
        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_csinc_w64_cc;
        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_csinc_w64_mi;
        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_csinc_w64_pl;
        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_csinc_w64_vs;
        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_csinc_w64_vc;
        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_csinc_w64_hi;
        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_csinc_w64_ls;
        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_csinc_w64_ge;
        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_csinc_w64_lt;
        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_csinc_w64_gt;
        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_csinc_w64_le;
        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_csinc_w64_al;
        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_csinc_w64_nv;
        return NULL;
    case ARM64_INST_CSINV:
        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_csinv_w32_eq;
        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_csinv_w32_ne;
        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_csinv_w32_cs;
        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_csinv_w32_cc;
        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_csinv_w32_mi;
        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_csinv_w32_pl;
        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_csinv_w32_vs;
        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_csinv_w32_vc;
        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_csinv_w32_hi;
        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_csinv_w32_ls;
        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_csinv_w32_ge;
        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_csinv_w32_lt;
        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_csinv_w32_gt;
        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_csinv_w32_le;
        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_csinv_w32_al;
        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_csinv_w32_nv;
        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_csinv_w64_eq;
        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_csinv_w64_ne;
        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_csinv_w64_cs;
        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_csinv_w64_cc;
        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_csinv_w64_mi;
        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_csinv_w64_pl;
        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_csinv_w64_vs;
        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_csinv_w64_vc;
        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_csinv_w64_hi;
        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_csinv_w64_ls;
        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_csinv_w64_ge;
        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_csinv_w64_lt;
        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_csinv_w64_gt;
        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_csinv_w64_le;
        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_csinv_w64_al;
        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_csinv_w64_nv;
        return NULL;
    case ARM64_INST_CSNEG:
        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_csneg_w32_eq;
        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_csneg_w32_ne;
        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_csneg_w32_cs;
        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_csneg_w32_cc;
        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_csneg_w32_mi;
        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_csneg_w32_pl;
        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_csneg_w32_vs;
        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_csneg_w32_vc;
        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_csneg_w32_hi;
        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_csneg_w32_ls;
        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_csneg_w32_ge;
        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_csneg_w32_lt;
        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_csneg_w32_gt;
        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_csneg_w32_le;
        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_csneg_w32_al;
        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_csneg_w32_nv;
        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_csneg_w64_eq;
        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_csneg_w64_ne;
        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_csneg_w64_cs;
        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_csneg_w64_cc;
        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_csneg_w64_mi;
        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_csneg_w64_pl;
        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_csneg_w64_vs;
        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_csneg_w64_vc;
        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_csneg_w64_hi;
        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_csneg_w64_ls;
        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_csneg_w64_ge;
        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_csneg_w64_lt;
        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_csneg_w64_gt;
        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_csneg_w64_le;
        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_csneg_w64_al;
        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_csneg_w64_nv;
        return NULL;
    case ARM64_INST_UDIV:
        if (decoded->operand_width == 32) return emu_udiv_w32;
        if (decoded->operand_width == 64) return emu_udiv_w64;
        return NULL;
    case ARM64_INST_SDIV:
        if (decoded->operand_width == 32) return emu_sdiv_w32;
        if (decoded->operand_width == 64) return emu_sdiv_w64;
        return NULL;
    case ARM64_INST_LSLV:
        if (decoded->operand_width == 32) return emu_lslv_w32;
        if (decoded->operand_width == 64) return emu_lslv_w64;
        return NULL;
    case ARM64_INST_LSRV:
        if (decoded->operand_width == 32) return emu_lsrv_w32;
        if (decoded->operand_width == 64) return emu_lsrv_w64;
        return NULL;
    case ARM64_INST_ASRV:
        if (decoded->operand_width == 32) return emu_asrv_w32;
        if (decoded->operand_width == 64) return emu_asrv_w64;
        return NULL;
    case ARM64_INST_RORV:
        if (decoded->operand_width == 32) return emu_rorv_w32;
        if (decoded->operand_width == 64) return emu_rorv_w64;
        return NULL;
    case ARM64_INST_CRC32B:
        if (decoded->operand_width == 32) return emu_crc32b_w32;
        if (decoded->operand_width == 64) return emu_crc32b_w64;
        return NULL;
    case ARM64_INST_CRC32H:
        if (decoded->operand_width == 32) return emu_crc32h_w32;
        if (decoded->operand_width == 64) return emu_crc32h_w64;
        return NULL;
    case ARM64_INST_CRC32W:
        if (decoded->operand_width == 32) return emu_crc32w_w32;
        if (decoded->operand_width == 64) return emu_crc32w_w64;
        return NULL;
    case ARM64_INST_CRC32X:
        if (decoded->operand_width == 32) return emu_crc32x_w32;
        if (decoded->operand_width == 64) return emu_crc32x_w64;
        return NULL;
    case ARM64_INST_CRC32CB:
        if (decoded->operand_width == 32) return emu_crc32cb_w32;
        if (decoded->operand_width == 64) return emu_crc32cb_w64;
        return NULL;
    case ARM64_INST_CRC32CH:
        if (decoded->operand_width == 32) return emu_crc32ch_w32;
        if (decoded->operand_width == 64) return emu_crc32ch_w64;
        return NULL;
    case ARM64_INST_CRC32CW:
        if (decoded->operand_width == 32) return emu_crc32cw_w32;
        if (decoded->operand_width == 64) return emu_crc32cw_w64;
        return NULL;
    case ARM64_INST_CRC32CX:
        if (decoded->operand_width == 32) return emu_crc32cx_w32;
        if (decoded->operand_width == 64) return emu_crc32cx_w64;
        return NULL;
    case ARM64_INST_SMAX_REGISTER:
        if (decoded->operand_width == 32) return emu_smax_register_w32;
        if (decoded->operand_width == 64) return emu_smax_register_w64;
        return NULL;
    case ARM64_INST_UMAX_REGISTER:
        if (decoded->operand_width == 32) return emu_umax_register_w32;
        if (decoded->operand_width == 64) return emu_umax_register_w64;
        return NULL;
    case ARM64_INST_SMIN_REGISTER:
        if (decoded->operand_width == 32) return emu_smin_register_w32;
        if (decoded->operand_width == 64) return emu_smin_register_w64;
        return NULL;
    case ARM64_INST_UMIN_REGISTER:
        if (decoded->operand_width == 32) return emu_umin_register_w32;
        if (decoded->operand_width == 64) return emu_umin_register_w64;
        return NULL;
    case ARM64_INST_MADD:
        if (decoded->operand_width == 32) return emu_madd_w32;
        if (decoded->operand_width == 64) return emu_madd_w64;
        return NULL;
    case ARM64_INST_MSUB:
        if (decoded->operand_width == 32) return emu_msub_w32;
        if (decoded->operand_width == 64) return emu_msub_w64;
        return NULL;
    case ARM64_INST_SMADDL:
        if (decoded->operand_width == 32) return emu_smaddl_w32;
        if (decoded->operand_width == 64) return emu_smaddl_w64;
        return NULL;
    case ARM64_INST_SMSUBL:
        if (decoded->operand_width == 32) return emu_smsubl_w32;
        if (decoded->operand_width == 64) return emu_smsubl_w64;
        return NULL;
    case ARM64_INST_SMULH:
        if (decoded->operand_width == 32) return emu_smulh_w32;
        if (decoded->operand_width == 64) return emu_smulh_w64;
        return NULL;
    case ARM64_INST_UMADDL:
        if (decoded->operand_width == 32) return emu_umaddl_w32;
        if (decoded->operand_width == 64) return emu_umaddl_w64;
        return NULL;
    case ARM64_INST_UMSUBL:
        if (decoded->operand_width == 32) return emu_umsubl_w32;
        if (decoded->operand_width == 64) return emu_umsubl_w64;
        return NULL;
    case ARM64_INST_UMULH:
        if (decoded->operand_width == 32) return emu_umulh_w32;
        if (decoded->operand_width == 64) return emu_umulh_w64;
        return NULL;
    case ARM64_INST_CCMN_REGISTER:
        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_ccmn_register_w32_eq;
        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_ccmn_register_w32_ne;
        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_ccmn_register_w32_cs;
        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_ccmn_register_w32_cc;
        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_ccmn_register_w32_mi;
        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_ccmn_register_w32_pl;
        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_ccmn_register_w32_vs;
        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_ccmn_register_w32_vc;
        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_ccmn_register_w32_hi;
        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_ccmn_register_w32_ls;
        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_ccmn_register_w32_ge;
        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_ccmn_register_w32_lt;
        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_ccmn_register_w32_gt;
        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_ccmn_register_w32_le;
        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_ccmn_register_w32_al;
        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_ccmn_register_w32_nv;
        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_ccmn_register_w64_eq;
        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_ccmn_register_w64_ne;
        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_ccmn_register_w64_cs;
        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_ccmn_register_w64_cc;
        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_ccmn_register_w64_mi;
        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_ccmn_register_w64_pl;
        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_ccmn_register_w64_vs;
        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_ccmn_register_w64_vc;
        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_ccmn_register_w64_hi;
        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_ccmn_register_w64_ls;
        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_ccmn_register_w64_ge;
        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_ccmn_register_w64_lt;
        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_ccmn_register_w64_gt;
        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_ccmn_register_w64_le;
        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_ccmn_register_w64_al;
        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_ccmn_register_w64_nv;
        return NULL;
    case ARM64_INST_CCMP_REGISTER:
        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_ccmp_register_w32_eq;
        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_ccmp_register_w32_ne;
        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_ccmp_register_w32_cs;
        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_ccmp_register_w32_cc;
        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_ccmp_register_w32_mi;
        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_ccmp_register_w32_pl;
        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_ccmp_register_w32_vs;
        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_ccmp_register_w32_vc;
        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_ccmp_register_w32_hi;
        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_ccmp_register_w32_ls;
        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_ccmp_register_w32_ge;
        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_ccmp_register_w32_lt;
        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_ccmp_register_w32_gt;
        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_ccmp_register_w32_le;
        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_ccmp_register_w32_al;
        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_ccmp_register_w32_nv;
        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_ccmp_register_w64_eq;
        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_ccmp_register_w64_ne;
        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_ccmp_register_w64_cs;
        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_ccmp_register_w64_cc;
        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_ccmp_register_w64_mi;
        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_ccmp_register_w64_pl;
        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_ccmp_register_w64_vs;
        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_ccmp_register_w64_vc;
        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_ccmp_register_w64_hi;
        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_ccmp_register_w64_ls;
        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_ccmp_register_w64_ge;
        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_ccmp_register_w64_lt;
        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_ccmp_register_w64_gt;
        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_ccmp_register_w64_le;
        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_ccmp_register_w64_al;
        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_ccmp_register_w64_nv;
        return NULL;
    case ARM64_INST_CCMN_IMMEDIATE:
        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_ccmn_immediate_w32_eq;
        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_ccmn_immediate_w32_ne;
        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_ccmn_immediate_w32_cs;
        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_ccmn_immediate_w32_cc;
        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_ccmn_immediate_w32_mi;
        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_ccmn_immediate_w32_pl;
        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_ccmn_immediate_w32_vs;
        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_ccmn_immediate_w32_vc;
        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_ccmn_immediate_w32_hi;
        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_ccmn_immediate_w32_ls;
        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_ccmn_immediate_w32_ge;
        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_ccmn_immediate_w32_lt;
        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_ccmn_immediate_w32_gt;
        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_ccmn_immediate_w32_le;
        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_ccmn_immediate_w32_al;
        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_ccmn_immediate_w32_nv;
        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_ccmn_immediate_w64_eq;
        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_ccmn_immediate_w64_ne;
        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_ccmn_immediate_w64_cs;
        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_ccmn_immediate_w64_cc;
        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_ccmn_immediate_w64_mi;
        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_ccmn_immediate_w64_pl;
        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_ccmn_immediate_w64_vs;
        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_ccmn_immediate_w64_vc;
        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_ccmn_immediate_w64_hi;
        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_ccmn_immediate_w64_ls;
        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_ccmn_immediate_w64_ge;
        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_ccmn_immediate_w64_lt;
        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_ccmn_immediate_w64_gt;
        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_ccmn_immediate_w64_le;
        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_ccmn_immediate_w64_al;
        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_ccmn_immediate_w64_nv;
        return NULL;
    case ARM64_INST_CCMP_IMMEDIATE:
        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_ccmp_immediate_w32_eq;
        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_ccmp_immediate_w32_ne;
        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_ccmp_immediate_w32_cs;
        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_ccmp_immediate_w32_cc;
        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_ccmp_immediate_w32_mi;
        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_ccmp_immediate_w32_pl;
        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_ccmp_immediate_w32_vs;
        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_ccmp_immediate_w32_vc;
        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_ccmp_immediate_w32_hi;
        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_ccmp_immediate_w32_ls;
        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_ccmp_immediate_w32_ge;
        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_ccmp_immediate_w32_lt;
        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_ccmp_immediate_w32_gt;
        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_ccmp_immediate_w32_le;
        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_ccmp_immediate_w32_al;
        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_ccmp_immediate_w32_nv;
        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_ccmp_immediate_w64_eq;
        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_ccmp_immediate_w64_ne;
        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_ccmp_immediate_w64_cs;
        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_ccmp_immediate_w64_cc;
        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_ccmp_immediate_w64_mi;
        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_ccmp_immediate_w64_pl;
        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_ccmp_immediate_w64_vs;
        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_ccmp_immediate_w64_vc;
        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_ccmp_immediate_w64_hi;
        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_ccmp_immediate_w64_ls;
        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_ccmp_immediate_w64_ge;
        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_ccmp_immediate_w64_lt;
        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_ccmp_immediate_w64_gt;
        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_ccmp_immediate_w64_le;
        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_ccmp_immediate_w64_al;
        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_ccmp_immediate_w64_nv;
        return NULL;
    case ARM64_INST_RBIT:
        if (decoded->operand_width == 32) return emu_rbit_w32;
        if (decoded->operand_width == 64) return emu_rbit_w64;
        return NULL;
    case ARM64_INST_REV16:
        if (decoded->operand_width == 32) return emu_rev16_w32;
        if (decoded->operand_width == 64) return emu_rev16_w64;
        return NULL;
    case ARM64_INST_REV32:
        if (decoded->operand_width == 32) return emu_rev32_w32;
        if (decoded->operand_width == 64) return emu_rev32_w64;
        return NULL;
    case ARM64_INST_REV64:
        if (decoded->operand_width == 32) return emu_rev64_w32;
        if (decoded->operand_width == 64) return emu_rev64_w64;
        return NULL;
    case ARM64_INST_CLZ:
        if (decoded->operand_width == 32) return emu_clz_w32;
        if (decoded->operand_width == 64) return emu_clz_w64;
        return NULL;
    case ARM64_INST_CLS:
        if (decoded->operand_width == 32) return emu_cls_w32;
        if (decoded->operand_width == 64) return emu_cls_w64;
        return NULL;
    case ARM64_INST_CTZ:
        if (decoded->operand_width == 32) return emu_ctz_w32;
        if (decoded->operand_width == 64) return emu_ctz_w64;
        return NULL;
    case ARM64_INST_CNT:
        if (decoded->operand_width == 32) return emu_cnt_w32;
        if (decoded->operand_width == 64) return emu_cnt_w64;
        return NULL;
    case ARM64_INST_ABS:
        if (decoded->operand_width == 32) return emu_abs_w32;
        if (decoded->operand_width == 64) return emu_abs_w64;
        return NULL;
    default:
        return NULL;
    }
}
// clang-format on
