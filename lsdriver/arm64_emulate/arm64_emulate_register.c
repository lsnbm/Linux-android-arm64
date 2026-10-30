/*
ARM64 指令执行器：数据处理寄存器类。

emu_* 函数完成移位寄存器、扩展寄存器、加减与逻辑运算、ADC/SBC、条件选择与条件比较、除法和可变移位。
同时覆盖 CRC、位计数与反转、最值、乘加、乘减、高位乘法及相关标志更新；w32/w64 函数区分操作数宽度。
emu_build_register_executor() 负责按指令、宽度、条件码、移位类型和扩展类型选择具体执行函数。
*/
#include "emulate_inst.h"

static inline void emu_and_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    and_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_and_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    and_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_and_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    and_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_and_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    and_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_and_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    and_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_and_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    and_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_and_shifted_register_w_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    and_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_and_shifted_register_x_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    and_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bic_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    bic_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bic_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    bic_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bic_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    bic_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bic_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    bic_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bic_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    bic_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bic_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    bic_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bic_shifted_register_w_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    bic_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bic_shifted_register_x_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    bic_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orr_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orr_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orr_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orr_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orr_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orr_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_shifted_register_w_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orr_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_shifted_register_x_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orr_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orn_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orn_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orn_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orn_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orn_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orn_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orn_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orn_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orn_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orn_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orn_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orn_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orn_shifted_register_w_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orn_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orn_shifted_register_x_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    orn_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eor_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eor_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eor_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eor_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eor_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eor_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_shifted_register_w_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eor_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_shifted_register_x_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eor_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eon_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eon_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eon_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eon_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eon_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eon_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eon_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eon_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eon_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eon_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eon_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eon_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eon_shifted_register_w_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eon_w(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eon_shifted_register_x_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t shifted_output __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &shifted_output, 0, 0, 0, 0);
    eon_x(read_gpr_or_zr(regs, entry->decoded.rn), shifted_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ands_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    ands_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_ands_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    ands_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_ands_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    ands_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_ands_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    ands_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_ands_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    ands_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_ands_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    ands_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_ands_shifted_register_w_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    rorv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    ands_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_ands_shifted_register_x_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    rorv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    ands_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_bics_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    bics_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_bics_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    bics_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_bics_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    bics_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_bics_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    bics_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_bics_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    bics_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_bics_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    bics_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_bics_shifted_register_w_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    rorv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    bics_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_bics_shifted_register_x_ror(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    rorv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    bics_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_add_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    add_shifted_w_lsl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    add_shifted_x_lsl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    add_shifted_w_lsr(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    add_shifted_x_lsr(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    add_shifted_w_asr(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    add_shifted_x_asr(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_adds_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adds_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adds_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_sub_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sub_shifted_w_lsl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sub_shifted_x_lsl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sub_shifted_w_lsr(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sub_shifted_x_lsr(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sub_shifted_w_asr(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sub_shifted_x_asr(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_subs_shifted_register_w_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_shifted_register_x_lsl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_subs_shifted_register_w_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_shifted_register_x_lsr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_subs_shifted_register_w_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_shifted_register_x_asr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_zr(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_w_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_x_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_w_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_x_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_x_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    lslv_x_uxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_x_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    lslv_x_uxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_w_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_x_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_w_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_x_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_x_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    lslv_x_sxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_extended_register_x_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    lslv_x_sxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    add_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_w_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_x_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_w_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_x_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_x_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x_uxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_x_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x_uxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_w_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_x_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_w_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_x_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_x_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x_sxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_adds_extended_register_x_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x_sxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    adds_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_w_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_x_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_w_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_x_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_x_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    uxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    lslv_x_uxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_x_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    lslv_x_uxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_w_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_x_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_w_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_x_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_x_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    lslv_x_sxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_w(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_extended_register_x_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t extended_output __attribute__((__uninitialized__));
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    lslv_x_sxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &extended_output, 0, 0, 0, 0);
    sub_x(read_gpr_or_sp(regs, entry->decoded.rn), extended_output, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_w_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_x_uxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_w_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_x_uxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_x_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x_uxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_x_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x_uxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_w_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_x_sxtb(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxtb_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_w_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_x_sxth(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxth_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_x_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    sxtw_x_shift(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x_sxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_w(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_subs_extended_register_x_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    lslv_x_sxtx(read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    subs_x(read_gpr_or_sp(regs, entry->decoded.rn), template_output0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    return;
}
static inline void emu_adc_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    adc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_adc_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    adc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_adcs_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    adcs_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, output, &flags, 0, 0, 0);
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_adcs_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    adcs_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, output, &flags, 0, 0, 0);
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_sbc_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sbc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sbc_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sbc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sbcs_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sbcs_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, output, &flags, 0, 0, 0);
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_sbcs_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sbcs_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_nzcv(regs), 0, 0, output, &flags, 0, 0, 0);
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_w_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csel_x_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csel_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_w_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinc_x_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinc_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_w_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csinv_x_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csinv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_w_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 0), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 1), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 2), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 3), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 4), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 5), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 6), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 7), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 8), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 9), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 10), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 11), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 12), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 13), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 14), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_csneg_x_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    csneg_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), cond_holds(read_nzcv(regs), 15), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_udiv_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    udiv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_udiv_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    udiv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sdiv_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sdiv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sdiv_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sdiv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lslv_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lslv_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lslv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lsrv_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lsrv_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    lsrv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_asrv_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_asrv_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    asrv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_rorv_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_rorv_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rorv_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_crc32b_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    crc32b(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_crc32h_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    crc32h(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_crc32w_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    crc32w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_crc32x_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    crc32x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_crc32cb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    crc32cb(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_crc32ch_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    crc32ch(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_crc32cw_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    crc32cw(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_crc32cx_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    crc32cx(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smax_register_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smax_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smax_register_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smax_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umax_register_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umax_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umax_register_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umax_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smin_register_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smin_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smin_register_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smin_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umin_register_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umin_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umin_register_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umin_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_madd_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    madd_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_madd_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    madd_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_msub_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    msub_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_msub_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    msub_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smaddl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smaddl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smsubl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smsubl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smulh_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smulh(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umaddl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umaddl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umsubl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umsubl(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.ra), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umulh_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umulh(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_w_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_register_x_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_w_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_register_x_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_w_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        ccmn_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmn_immediate_x_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        ccmn_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_w_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        ccmp_al_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_eq(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 0))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_ne(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 1))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_cs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 2))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_cc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 3))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_mi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 4))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_pl(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 5))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_vs(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 6))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_vc(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 7))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_hi(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 8))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_ls(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 9))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_ge(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 10))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_lt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 11))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_gt(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 12))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_le(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 13))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_al(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 14))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_ccmp_immediate_x_nv(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t flags = (uint64_t)entry->decoded.nzcv << 28;
    if (cond_holds(read_nzcv(regs), 15))
    {
        ccmp_al_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, &flags, 0, 0, 0, 0);
    }
    write_nzcv(regs, flags);
    regs->pc += 4;
    return;
}
static inline void emu_rbit_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rbit_w(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_rbit_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rbit_x(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_rev16_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rev16_w(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_rev16_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rev16_x(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_rev_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rev_w(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_rev32_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rev32_x(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_rev_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    rev_x(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clz_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    clz_w(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_clz_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    clz_x(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_cls_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    cls_w(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_cls_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    cls_x(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ctz_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    ctz_w(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ctz_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    ctz_x(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_cnt_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    cnt_w(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_cnt_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    cnt_x(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_abs_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    abs_w(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_abs_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    abs_x(read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
void (*emu_build_register_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_AND_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_and_shifted_register_w_lsl;
            case 1:
                return emu_and_shifted_register_w_lsr;
            case 2:
                return emu_and_shifted_register_w_asr;
            case 3:
                return emu_and_shifted_register_w_ror;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_and_shifted_register_x_lsl;
            case 1:
                return emu_and_shifted_register_x_lsr;
            case 2:
                return emu_and_shifted_register_x_asr;
            case 3:
                return emu_and_shifted_register_x_ror;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BIC_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_bic_shifted_register_w_lsl;
            case 1:
                return emu_bic_shifted_register_w_lsr;
            case 2:
                return emu_bic_shifted_register_w_asr;
            case 3:
                return emu_bic_shifted_register_w_ror;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_bic_shifted_register_x_lsl;
            case 1:
                return emu_bic_shifted_register_x_lsr;
            case 2:
                return emu_bic_shifted_register_x_asr;
            case 3:
                return emu_bic_shifted_register_x_ror;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ORR_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_orr_shifted_register_w_lsl;
            case 1:
                return emu_orr_shifted_register_w_lsr;
            case 2:
                return emu_orr_shifted_register_w_asr;
            case 3:
                return emu_orr_shifted_register_w_ror;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_orr_shifted_register_x_lsl;
            case 1:
                return emu_orr_shifted_register_x_lsr;
            case 2:
                return emu_orr_shifted_register_x_asr;
            case 3:
                return emu_orr_shifted_register_x_ror;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ORN_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_orn_shifted_register_w_lsl;
            case 1:
                return emu_orn_shifted_register_w_lsr;
            case 2:
                return emu_orn_shifted_register_w_asr;
            case 3:
                return emu_orn_shifted_register_w_ror;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_orn_shifted_register_x_lsl;
            case 1:
                return emu_orn_shifted_register_x_lsr;
            case 2:
                return emu_orn_shifted_register_x_asr;
            case 3:
                return emu_orn_shifted_register_x_ror;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_EOR_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_eor_shifted_register_w_lsl;
            case 1:
                return emu_eor_shifted_register_w_lsr;
            case 2:
                return emu_eor_shifted_register_w_asr;
            case 3:
                return emu_eor_shifted_register_w_ror;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_eor_shifted_register_x_lsl;
            case 1:
                return emu_eor_shifted_register_x_lsr;
            case 2:
                return emu_eor_shifted_register_x_asr;
            case 3:
                return emu_eor_shifted_register_x_ror;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_EON_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_eon_shifted_register_w_lsl;
            case 1:
                return emu_eon_shifted_register_w_lsr;
            case 2:
                return emu_eon_shifted_register_w_asr;
            case 3:
                return emu_eon_shifted_register_w_ror;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_eon_shifted_register_x_lsl;
            case 1:
                return emu_eon_shifted_register_x_lsr;
            case 2:
                return emu_eon_shifted_register_x_asr;
            case 3:
                return emu_eon_shifted_register_x_ror;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ANDS_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_ands_shifted_register_w_lsl;
            case 1:
                return emu_ands_shifted_register_w_lsr;
            case 2:
                return emu_ands_shifted_register_w_asr;
            case 3:
                return emu_ands_shifted_register_w_ror;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_ands_shifted_register_x_lsl;
            case 1:
                return emu_ands_shifted_register_x_lsr;
            case 2:
                return emu_ands_shifted_register_x_asr;
            case 3:
                return emu_ands_shifted_register_x_ror;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BICS_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_bics_shifted_register_w_lsl;
            case 1:
                return emu_bics_shifted_register_w_lsr;
            case 2:
                return emu_bics_shifted_register_w_asr;
            case 3:
                return emu_bics_shifted_register_w_ror;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_bics_shifted_register_x_lsl;
            case 1:
                return emu_bics_shifted_register_x_lsr;
            case 2:
                return emu_bics_shifted_register_x_asr;
            case 3:
                return emu_bics_shifted_register_x_ror;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ADD_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_add_shifted_register_w_lsl;
            case 1:
                return emu_add_shifted_register_w_lsr;
            case 2:
                return emu_add_shifted_register_w_asr;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_add_shifted_register_x_lsl;
            case 1:
                return emu_add_shifted_register_x_lsr;
            case 2:
                return emu_add_shifted_register_x_asr;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ADDS_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_adds_shifted_register_w_lsl;
            case 1:
                return emu_adds_shifted_register_w_lsr;
            case 2:
                return emu_adds_shifted_register_w_asr;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_adds_shifted_register_x_lsl;
            case 1:
                return emu_adds_shifted_register_x_lsr;
            case 2:
                return emu_adds_shifted_register_x_asr;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SUB_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_sub_shifted_register_w_lsl;
            case 1:
                return emu_sub_shifted_register_w_lsr;
            case 2:
                return emu_sub_shifted_register_w_asr;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_sub_shifted_register_x_lsl;
            case 1:
                return emu_sub_shifted_register_x_lsr;
            case 2:
                return emu_sub_shifted_register_x_asr;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SUBS_SHIFTED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_subs_shifted_register_w_lsl;
            case 1:
                return emu_subs_shifted_register_w_lsr;
            case 2:
                return emu_subs_shifted_register_w_asr;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->shift_type)
            {
            case 0:
                return emu_subs_shifted_register_x_lsl;
            case 1:
                return emu_subs_shifted_register_x_lsr;
            case 2:
                return emu_subs_shifted_register_x_asr;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ADD_EXTENDED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->extend_type)
            {
            case 0:
                return emu_add_extended_register_w_uxtb;
            case 1:
                return emu_add_extended_register_w_uxth;
            case 2:
                return emu_add_extended_register_w_uxtw;
            case 3:
                return emu_add_extended_register_w_uxtx;
            case 4:
                return emu_add_extended_register_w_sxtb;
            case 5:
                return emu_add_extended_register_w_sxth;
            case 6:
                return emu_add_extended_register_w_sxtw;
            case 7:
                return emu_add_extended_register_w_sxtx;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->extend_type)
            {
            case 0:
                return emu_add_extended_register_x_uxtb;
            case 1:
                return emu_add_extended_register_x_uxth;
            case 2:
                return emu_add_extended_register_x_uxtw;
            case 3:
                return emu_add_extended_register_x_uxtx;
            case 4:
                return emu_add_extended_register_x_sxtb;
            case 5:
                return emu_add_extended_register_x_sxth;
            case 6:
                return emu_add_extended_register_x_sxtw;
            case 7:
                return emu_add_extended_register_x_sxtx;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ADDS_EXTENDED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->extend_type)
            {
            case 0:
                return emu_adds_extended_register_w_uxtb;
            case 1:
                return emu_adds_extended_register_w_uxth;
            case 2:
                return emu_adds_extended_register_w_uxtw;
            case 3:
                return emu_adds_extended_register_w_uxtx;
            case 4:
                return emu_adds_extended_register_w_sxtb;
            case 5:
                return emu_adds_extended_register_w_sxth;
            case 6:
                return emu_adds_extended_register_w_sxtw;
            case 7:
                return emu_adds_extended_register_w_sxtx;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->extend_type)
            {
            case 0:
                return emu_adds_extended_register_x_uxtb;
            case 1:
                return emu_adds_extended_register_x_uxth;
            case 2:
                return emu_adds_extended_register_x_uxtw;
            case 3:
                return emu_adds_extended_register_x_uxtx;
            case 4:
                return emu_adds_extended_register_x_sxtb;
            case 5:
                return emu_adds_extended_register_x_sxth;
            case 6:
                return emu_adds_extended_register_x_sxtw;
            case 7:
                return emu_adds_extended_register_x_sxtx;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SUB_EXTENDED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->extend_type)
            {
            case 0:
                return emu_sub_extended_register_w_uxtb;
            case 1:
                return emu_sub_extended_register_w_uxth;
            case 2:
                return emu_sub_extended_register_w_uxtw;
            case 3:
                return emu_sub_extended_register_w_uxtx;
            case 4:
                return emu_sub_extended_register_w_sxtb;
            case 5:
                return emu_sub_extended_register_w_sxth;
            case 6:
                return emu_sub_extended_register_w_sxtw;
            case 7:
                return emu_sub_extended_register_w_sxtx;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->extend_type)
            {
            case 0:
                return emu_sub_extended_register_x_uxtb;
            case 1:
                return emu_sub_extended_register_x_uxth;
            case 2:
                return emu_sub_extended_register_x_uxtw;
            case 3:
                return emu_sub_extended_register_x_uxtx;
            case 4:
                return emu_sub_extended_register_x_sxtb;
            case 5:
                return emu_sub_extended_register_x_sxth;
            case 6:
                return emu_sub_extended_register_x_sxtw;
            case 7:
                return emu_sub_extended_register_x_sxtx;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SUBS_EXTENDED_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->extend_type)
            {
            case 0:
                return emu_subs_extended_register_w_uxtb;
            case 1:
                return emu_subs_extended_register_w_uxth;
            case 2:
                return emu_subs_extended_register_w_uxtw;
            case 3:
                return emu_subs_extended_register_w_uxtx;
            case 4:
                return emu_subs_extended_register_w_sxtb;
            case 5:
                return emu_subs_extended_register_w_sxth;
            case 6:
                return emu_subs_extended_register_w_sxtw;
            case 7:
                return emu_subs_extended_register_w_sxtx;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->extend_type)
            {
            case 0:
                return emu_subs_extended_register_x_uxtb;
            case 1:
                return emu_subs_extended_register_x_uxth;
            case 2:
                return emu_subs_extended_register_x_uxtw;
            case 3:
                return emu_subs_extended_register_x_uxtx;
            case 4:
                return emu_subs_extended_register_x_sxtb;
            case 5:
                return emu_subs_extended_register_x_sxth;
            case 6:
                return emu_subs_extended_register_x_sxtw;
            case 7:
                return emu_subs_extended_register_x_sxtx;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ADC:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_adc_w;
        case 64:
            return emu_adc_x;
        default:
            return NULL;
        }
    case ARM64_INST_ADCS:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_adcs_w;
        case 64:
            return emu_adcs_x;
        default:
            return NULL;
        }
    case ARM64_INST_SBC:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_sbc_w;
        case 64:
            return emu_sbc_x;
        default:
            return NULL;
        }
    case ARM64_INST_SBCS:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_sbcs_w;
        case 64:
            return emu_sbcs_x;
        default:
            return NULL;
        }
    case ARM64_INST_CSEL:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->condition)
            {
            case 0:
                return emu_csel_w_eq;
            case 1:
                return emu_csel_w_ne;
            case 2:
                return emu_csel_w_cs;
            case 3:
                return emu_csel_w_cc;
            case 4:
                return emu_csel_w_mi;
            case 5:
                return emu_csel_w_pl;
            case 6:
                return emu_csel_w_vs;
            case 7:
                return emu_csel_w_vc;
            case 8:
                return emu_csel_w_hi;
            case 9:
                return emu_csel_w_ls;
            case 10:
                return emu_csel_w_ge;
            case 11:
                return emu_csel_w_lt;
            case 12:
                return emu_csel_w_gt;
            case 13:
                return emu_csel_w_le;
            case 14:
                return emu_csel_w_al;
            case 15:
                return emu_csel_w_nv;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->condition)
            {
            case 0:
                return emu_csel_x_eq;
            case 1:
                return emu_csel_x_ne;
            case 2:
                return emu_csel_x_cs;
            case 3:
                return emu_csel_x_cc;
            case 4:
                return emu_csel_x_mi;
            case 5:
                return emu_csel_x_pl;
            case 6:
                return emu_csel_x_vs;
            case 7:
                return emu_csel_x_vc;
            case 8:
                return emu_csel_x_hi;
            case 9:
                return emu_csel_x_ls;
            case 10:
                return emu_csel_x_ge;
            case 11:
                return emu_csel_x_lt;
            case 12:
                return emu_csel_x_gt;
            case 13:
                return emu_csel_x_le;
            case 14:
                return emu_csel_x_al;
            case 15:
                return emu_csel_x_nv;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CSINC:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->condition)
            {
            case 0:
                return emu_csinc_w_eq;
            case 1:
                return emu_csinc_w_ne;
            case 2:
                return emu_csinc_w_cs;
            case 3:
                return emu_csinc_w_cc;
            case 4:
                return emu_csinc_w_mi;
            case 5:
                return emu_csinc_w_pl;
            case 6:
                return emu_csinc_w_vs;
            case 7:
                return emu_csinc_w_vc;
            case 8:
                return emu_csinc_w_hi;
            case 9:
                return emu_csinc_w_ls;
            case 10:
                return emu_csinc_w_ge;
            case 11:
                return emu_csinc_w_lt;
            case 12:
                return emu_csinc_w_gt;
            case 13:
                return emu_csinc_w_le;
            case 14:
                return emu_csinc_w_al;
            case 15:
                return emu_csinc_w_nv;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->condition)
            {
            case 0:
                return emu_csinc_x_eq;
            case 1:
                return emu_csinc_x_ne;
            case 2:
                return emu_csinc_x_cs;
            case 3:
                return emu_csinc_x_cc;
            case 4:
                return emu_csinc_x_mi;
            case 5:
                return emu_csinc_x_pl;
            case 6:
                return emu_csinc_x_vs;
            case 7:
                return emu_csinc_x_vc;
            case 8:
                return emu_csinc_x_hi;
            case 9:
                return emu_csinc_x_ls;
            case 10:
                return emu_csinc_x_ge;
            case 11:
                return emu_csinc_x_lt;
            case 12:
                return emu_csinc_x_gt;
            case 13:
                return emu_csinc_x_le;
            case 14:
                return emu_csinc_x_al;
            case 15:
                return emu_csinc_x_nv;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CSINV:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->condition)
            {
            case 0:
                return emu_csinv_w_eq;
            case 1:
                return emu_csinv_w_ne;
            case 2:
                return emu_csinv_w_cs;
            case 3:
                return emu_csinv_w_cc;
            case 4:
                return emu_csinv_w_mi;
            case 5:
                return emu_csinv_w_pl;
            case 6:
                return emu_csinv_w_vs;
            case 7:
                return emu_csinv_w_vc;
            case 8:
                return emu_csinv_w_hi;
            case 9:
                return emu_csinv_w_ls;
            case 10:
                return emu_csinv_w_ge;
            case 11:
                return emu_csinv_w_lt;
            case 12:
                return emu_csinv_w_gt;
            case 13:
                return emu_csinv_w_le;
            case 14:
                return emu_csinv_w_al;
            case 15:
                return emu_csinv_w_nv;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->condition)
            {
            case 0:
                return emu_csinv_x_eq;
            case 1:
                return emu_csinv_x_ne;
            case 2:
                return emu_csinv_x_cs;
            case 3:
                return emu_csinv_x_cc;
            case 4:
                return emu_csinv_x_mi;
            case 5:
                return emu_csinv_x_pl;
            case 6:
                return emu_csinv_x_vs;
            case 7:
                return emu_csinv_x_vc;
            case 8:
                return emu_csinv_x_hi;
            case 9:
                return emu_csinv_x_ls;
            case 10:
                return emu_csinv_x_ge;
            case 11:
                return emu_csinv_x_lt;
            case 12:
                return emu_csinv_x_gt;
            case 13:
                return emu_csinv_x_le;
            case 14:
                return emu_csinv_x_al;
            case 15:
                return emu_csinv_x_nv;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CSNEG:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->condition)
            {
            case 0:
                return emu_csneg_w_eq;
            case 1:
                return emu_csneg_w_ne;
            case 2:
                return emu_csneg_w_cs;
            case 3:
                return emu_csneg_w_cc;
            case 4:
                return emu_csneg_w_mi;
            case 5:
                return emu_csneg_w_pl;
            case 6:
                return emu_csneg_w_vs;
            case 7:
                return emu_csneg_w_vc;
            case 8:
                return emu_csneg_w_hi;
            case 9:
                return emu_csneg_w_ls;
            case 10:
                return emu_csneg_w_ge;
            case 11:
                return emu_csneg_w_lt;
            case 12:
                return emu_csneg_w_gt;
            case 13:
                return emu_csneg_w_le;
            case 14:
                return emu_csneg_w_al;
            case 15:
                return emu_csneg_w_nv;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->condition)
            {
            case 0:
                return emu_csneg_x_eq;
            case 1:
                return emu_csneg_x_ne;
            case 2:
                return emu_csneg_x_cs;
            case 3:
                return emu_csneg_x_cc;
            case 4:
                return emu_csneg_x_mi;
            case 5:
                return emu_csneg_x_pl;
            case 6:
                return emu_csneg_x_vs;
            case 7:
                return emu_csneg_x_vc;
            case 8:
                return emu_csneg_x_hi;
            case 9:
                return emu_csneg_x_ls;
            case 10:
                return emu_csneg_x_ge;
            case 11:
                return emu_csneg_x_lt;
            case 12:
                return emu_csneg_x_gt;
            case 13:
                return emu_csneg_x_le;
            case 14:
                return emu_csneg_x_al;
            case 15:
                return emu_csneg_x_nv;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UDIV:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_udiv_w;
        case 64:
            return emu_udiv_x;
        default:
            return NULL;
        }
    case ARM64_INST_SDIV:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_sdiv_w;
        case 64:
            return emu_sdiv_x;
        default:
            return NULL;
        }
    case ARM64_INST_LSLV:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_lslv_w;
        case 64:
            return emu_lslv_x;
        default:
            return NULL;
        }
    case ARM64_INST_LSRV:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_lsrv_w;
        case 64:
            return emu_lsrv_x;
        default:
            return NULL;
        }
    case ARM64_INST_ASRV:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_asrv_w;
        case 64:
            return emu_asrv_x;
        default:
            return NULL;
        }
    case ARM64_INST_RORV:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_rorv_w;
        case 64:
            return emu_rorv_x;
        default:
            return NULL;
        }
    case ARM64_INST_CRC32B:
        return emu_crc32b_w;
    case ARM64_INST_CRC32H:
        return emu_crc32h_w;
    case ARM64_INST_CRC32W:
        return emu_crc32w_w;
    case ARM64_INST_CRC32X:
        return emu_crc32x_x;
    case ARM64_INST_CRC32CB:
        return emu_crc32cb_w;
    case ARM64_INST_CRC32CH:
        return emu_crc32ch_w;
    case ARM64_INST_CRC32CW:
        return emu_crc32cw_w;
    case ARM64_INST_CRC32CX:
        return emu_crc32cx_x;
    case ARM64_INST_SMAX_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_smax_register_w;
        case 64:
            return emu_smax_register_x;
        default:
            return NULL;
        }
    case ARM64_INST_UMAX_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_umax_register_w;
        case 64:
            return emu_umax_register_x;
        default:
            return NULL;
        }
    case ARM64_INST_SMIN_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_smin_register_w;
        case 64:
            return emu_smin_register_x;
        default:
            return NULL;
        }
    case ARM64_INST_UMIN_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_umin_register_w;
        case 64:
            return emu_umin_register_x;
        default:
            return NULL;
        }
    case ARM64_INST_MADD:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_madd_w;
        case 64:
            return emu_madd_x;
        default:
            return NULL;
        }
    case ARM64_INST_MSUB:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_msub_w;
        case 64:
            return emu_msub_x;
        default:
            return NULL;
        }
    case ARM64_INST_SMADDL:
        return emu_smaddl_x;
    case ARM64_INST_SMSUBL:
        return emu_smsubl_x;
    case ARM64_INST_SMULH:
        return emu_smulh_x;
    case ARM64_INST_UMADDL:
        return emu_umaddl_x;
    case ARM64_INST_UMSUBL:
        return emu_umsubl_x;
    case ARM64_INST_UMULH:
        return emu_umulh_x;
    case ARM64_INST_CCMN_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->condition)
            {
            case 0:
                return emu_ccmn_register_w_eq;
            case 1:
                return emu_ccmn_register_w_ne;
            case 2:
                return emu_ccmn_register_w_cs;
            case 3:
                return emu_ccmn_register_w_cc;
            case 4:
                return emu_ccmn_register_w_mi;
            case 5:
                return emu_ccmn_register_w_pl;
            case 6:
                return emu_ccmn_register_w_vs;
            case 7:
                return emu_ccmn_register_w_vc;
            case 8:
                return emu_ccmn_register_w_hi;
            case 9:
                return emu_ccmn_register_w_ls;
            case 10:
                return emu_ccmn_register_w_ge;
            case 11:
                return emu_ccmn_register_w_lt;
            case 12:
                return emu_ccmn_register_w_gt;
            case 13:
                return emu_ccmn_register_w_le;
            case 14:
                return emu_ccmn_register_w_al;
            case 15:
                return emu_ccmn_register_w_nv;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->condition)
            {
            case 0:
                return emu_ccmn_register_x_eq;
            case 1:
                return emu_ccmn_register_x_ne;
            case 2:
                return emu_ccmn_register_x_cs;
            case 3:
                return emu_ccmn_register_x_cc;
            case 4:
                return emu_ccmn_register_x_mi;
            case 5:
                return emu_ccmn_register_x_pl;
            case 6:
                return emu_ccmn_register_x_vs;
            case 7:
                return emu_ccmn_register_x_vc;
            case 8:
                return emu_ccmn_register_x_hi;
            case 9:
                return emu_ccmn_register_x_ls;
            case 10:
                return emu_ccmn_register_x_ge;
            case 11:
                return emu_ccmn_register_x_lt;
            case 12:
                return emu_ccmn_register_x_gt;
            case 13:
                return emu_ccmn_register_x_le;
            case 14:
                return emu_ccmn_register_x_al;
            case 15:
                return emu_ccmn_register_x_nv;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CCMP_REGISTER:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->condition)
            {
            case 0:
                return emu_ccmp_register_w_eq;
            case 1:
                return emu_ccmp_register_w_ne;
            case 2:
                return emu_ccmp_register_w_cs;
            case 3:
                return emu_ccmp_register_w_cc;
            case 4:
                return emu_ccmp_register_w_mi;
            case 5:
                return emu_ccmp_register_w_pl;
            case 6:
                return emu_ccmp_register_w_vs;
            case 7:
                return emu_ccmp_register_w_vc;
            case 8:
                return emu_ccmp_register_w_hi;
            case 9:
                return emu_ccmp_register_w_ls;
            case 10:
                return emu_ccmp_register_w_ge;
            case 11:
                return emu_ccmp_register_w_lt;
            case 12:
                return emu_ccmp_register_w_gt;
            case 13:
                return emu_ccmp_register_w_le;
            case 14:
                return emu_ccmp_register_w_al;
            case 15:
                return emu_ccmp_register_w_nv;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->condition)
            {
            case 0:
                return emu_ccmp_register_x_eq;
            case 1:
                return emu_ccmp_register_x_ne;
            case 2:
                return emu_ccmp_register_x_cs;
            case 3:
                return emu_ccmp_register_x_cc;
            case 4:
                return emu_ccmp_register_x_mi;
            case 5:
                return emu_ccmp_register_x_pl;
            case 6:
                return emu_ccmp_register_x_vs;
            case 7:
                return emu_ccmp_register_x_vc;
            case 8:
                return emu_ccmp_register_x_hi;
            case 9:
                return emu_ccmp_register_x_ls;
            case 10:
                return emu_ccmp_register_x_ge;
            case 11:
                return emu_ccmp_register_x_lt;
            case 12:
                return emu_ccmp_register_x_gt;
            case 13:
                return emu_ccmp_register_x_le;
            case 14:
                return emu_ccmp_register_x_al;
            case 15:
                return emu_ccmp_register_x_nv;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CCMN_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->condition)
            {
            case 0:
                return emu_ccmn_immediate_w_eq;
            case 1:
                return emu_ccmn_immediate_w_ne;
            case 2:
                return emu_ccmn_immediate_w_cs;
            case 3:
                return emu_ccmn_immediate_w_cc;
            case 4:
                return emu_ccmn_immediate_w_mi;
            case 5:
                return emu_ccmn_immediate_w_pl;
            case 6:
                return emu_ccmn_immediate_w_vs;
            case 7:
                return emu_ccmn_immediate_w_vc;
            case 8:
                return emu_ccmn_immediate_w_hi;
            case 9:
                return emu_ccmn_immediate_w_ls;
            case 10:
                return emu_ccmn_immediate_w_ge;
            case 11:
                return emu_ccmn_immediate_w_lt;
            case 12:
                return emu_ccmn_immediate_w_gt;
            case 13:
                return emu_ccmn_immediate_w_le;
            case 14:
                return emu_ccmn_immediate_w_al;
            case 15:
                return emu_ccmn_immediate_w_nv;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->condition)
            {
            case 0:
                return emu_ccmn_immediate_x_eq;
            case 1:
                return emu_ccmn_immediate_x_ne;
            case 2:
                return emu_ccmn_immediate_x_cs;
            case 3:
                return emu_ccmn_immediate_x_cc;
            case 4:
                return emu_ccmn_immediate_x_mi;
            case 5:
                return emu_ccmn_immediate_x_pl;
            case 6:
                return emu_ccmn_immediate_x_vs;
            case 7:
                return emu_ccmn_immediate_x_vc;
            case 8:
                return emu_ccmn_immediate_x_hi;
            case 9:
                return emu_ccmn_immediate_x_ls;
            case 10:
                return emu_ccmn_immediate_x_ge;
            case 11:
                return emu_ccmn_immediate_x_lt;
            case 12:
                return emu_ccmn_immediate_x_gt;
            case 13:
                return emu_ccmn_immediate_x_le;
            case 14:
                return emu_ccmn_immediate_x_al;
            case 15:
                return emu_ccmn_immediate_x_nv;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CCMP_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->condition)
            {
            case 0:
                return emu_ccmp_immediate_w_eq;
            case 1:
                return emu_ccmp_immediate_w_ne;
            case 2:
                return emu_ccmp_immediate_w_cs;
            case 3:
                return emu_ccmp_immediate_w_cc;
            case 4:
                return emu_ccmp_immediate_w_mi;
            case 5:
                return emu_ccmp_immediate_w_pl;
            case 6:
                return emu_ccmp_immediate_w_vs;
            case 7:
                return emu_ccmp_immediate_w_vc;
            case 8:
                return emu_ccmp_immediate_w_hi;
            case 9:
                return emu_ccmp_immediate_w_ls;
            case 10:
                return emu_ccmp_immediate_w_ge;
            case 11:
                return emu_ccmp_immediate_w_lt;
            case 12:
                return emu_ccmp_immediate_w_gt;
            case 13:
                return emu_ccmp_immediate_w_le;
            case 14:
                return emu_ccmp_immediate_w_al;
            case 15:
                return emu_ccmp_immediate_w_nv;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->condition)
            {
            case 0:
                return emu_ccmp_immediate_x_eq;
            case 1:
                return emu_ccmp_immediate_x_ne;
            case 2:
                return emu_ccmp_immediate_x_cs;
            case 3:
                return emu_ccmp_immediate_x_cc;
            case 4:
                return emu_ccmp_immediate_x_mi;
            case 5:
                return emu_ccmp_immediate_x_pl;
            case 6:
                return emu_ccmp_immediate_x_vs;
            case 7:
                return emu_ccmp_immediate_x_vc;
            case 8:
                return emu_ccmp_immediate_x_hi;
            case 9:
                return emu_ccmp_immediate_x_ls;
            case 10:
                return emu_ccmp_immediate_x_ge;
            case 11:
                return emu_ccmp_immediate_x_lt;
            case 12:
                return emu_ccmp_immediate_x_gt;
            case 13:
                return emu_ccmp_immediate_x_le;
            case 14:
                return emu_ccmp_immediate_x_al;
            case 15:
                return emu_ccmp_immediate_x_nv;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_RBIT:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_rbit_w;
        case 64:
            return emu_rbit_x;
        default:
            return NULL;
        }
    case ARM64_INST_REV16:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_rev16_w;
        case 64:
            return emu_rev16_x;
        default:
            return NULL;
        }
    case ARM64_INST_REV32:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_rev_w;
        case 64:
            return emu_rev32_x;
        default:
            return NULL;
        }
    case ARM64_INST_REV64:
        return emu_rev_x;
    case ARM64_INST_CLZ:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_clz_w;
        case 64:
            return emu_clz_x;
        default:
            return NULL;
        }
    case ARM64_INST_CLS:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_cls_w;
        case 64:
            return emu_cls_x;
        default:
            return NULL;
        }
    case ARM64_INST_CTZ:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ctz_w;
        case 64:
            return emu_ctz_x;
        default:
            return NULL;
        }
    case ARM64_INST_CNT:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_cnt_w;
        case 64:
            return emu_cnt_x;
        default:
            return NULL;
        }
    case ARM64_INST_ABS:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_abs_w;
        case 64:
            return emu_abs_x;
        default:
            return NULL;
        }
    default:
        return NULL;
    }
}
