/*
ARM64 指令执行器：数据处理立即数类。

emu_* 函数直接调用对应硬件语义模板，完成 ADR/ADRP、EXTR、加减立即数、逻辑立即数、宽立即数移动和位域操作。
其中带 S 后缀的指令同时提交 NZCV；带 w32/w64 后缀的函数分别实现 32 位和 64 位操作数宽度。
emu_build_immediate_executor() 负责按指令和操作数宽度选择具体执行函数。
*/
#include "emulate_inst.h"

static inline void emu_adr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rd, regs->pc + entry->decoded.offset, true);
    regs->pc += 4;
    return;
}
static inline void emu_adrp_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    write_gpr_or_zr(regs, entry->decoded.rd, (regs->pc & ~0xFFFULL) + entry->decoded.offset, true);
    regs->pc += 4;
    return;
}
static inline void emu_extr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    extr_w(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_extr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    extr_x(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_extr_w_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    extr_w_imm0(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_extr_x_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    extr_x_imm0(read_gpr_or_zr(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    add_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_add_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    add_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_adds_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    adds_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    regs->pc += 4;
    return;
}
static inline void emu_adds_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    adds_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    regs->pc += 4;
    return;
}
static inline void emu_sub_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sub_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sub_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    sub_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_subs_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    subs_w(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    regs->pc += 4;
    return;
}
static inline void emu_subs_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    subs_x(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    regs->pc += 4;
    return;
}
static inline void emu_smax_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smax_w(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smax_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smax_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umax_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umax_w(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umax_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umax_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smin_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smin_w(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_smin_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    smin_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umin_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umin_w(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_umin_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    umin_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_and_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    and_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_and_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    and_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    orr_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_orr_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    orr_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    eor_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_eor_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t *output = gpr_output_or_sp(regs, entry->decoded.rd);
    eor_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ands_immediate_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    ands_w(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    regs->pc += 4;
    return;
}
static inline void emu_ands_immediate_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    ands_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.immediate, 0, 0, 0, output, &template_output1, 0, 0, 0);
    write_nzcv(regs, template_output1);
    regs->pc += 4;
    return;
}
static inline void emu_movn_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    movn_w(0, entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_movn_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    movn_x(0, entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_movz_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    movz_w(0, entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_movz_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    movz_x(0, entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_movk_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    movk_w(read_gpr_or_zr(regs, entry->decoded.rd), entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_movk_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    movk_x(read_gpr_or_zr(regs, entry->decoded.rd), entry->decoded.immediate, entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sbfm_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sbfm_dynamic_w(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, entry->decoded.imms, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sbfm_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    sbfm_dynamic_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, entry->decoded.imms, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bfm_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    bfm_dynamic_w(read_gpr_or_zr(regs, entry->decoded.rd) & 0xFFFFFFFFULL, read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_bfm_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    bfm_dynamic_x(read_gpr_or_zr(regs, entry->decoded.rd), read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ubfm_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    ubfm_dynamic_w(read_gpr_or_zr(regs, entry->decoded.rn) & 0xFFFFFFFFULL, entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ubfm_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rd, &discard);
    ubfm_dynamic_x(read_gpr_or_zr(regs, entry->decoded.rn), entry->decoded.bitfield_wmask, entry->decoded.bitfield_tmask, entry->decoded.immr, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
void (*emu_build_immediate_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_ADR:
        return emu_adr_x;
    case ARM64_INST_ADRP:
        return emu_adrp_x;
    case ARM64_INST_EXTR:
        switch (decoded->operand_width)
        {
        case 32:
            return decoded->shift_amount == 0 ? emu_extr_w_imm0 : emu_extr_w;
        case 64:
            return decoded->shift_amount == 0 ? emu_extr_x_imm0 : emu_extr_x;
        default:
            return NULL;
        }
    case ARM64_INST_ADD_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_add_immediate_w;
        case 64:
            return emu_add_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_ADDS_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_adds_immediate_w;
        case 64:
            return emu_adds_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_SUB_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_sub_immediate_w;
        case 64:
            return emu_sub_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_SUBS_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_subs_immediate_w;
        case 64:
            return emu_subs_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_SMAX_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_smax_immediate_w;
        case 64:
            return emu_smax_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_UMAX_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_umax_immediate_w;
        case 64:
            return emu_umax_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_SMIN_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_smin_immediate_w;
        case 64:
            return emu_smin_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_UMIN_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_umin_immediate_w;
        case 64:
            return emu_umin_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_AND_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_and_immediate_w;
        case 64:
            return emu_and_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_ORR_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_orr_immediate_w;
        case 64:
            return emu_orr_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_EOR_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_eor_immediate_w;
        case 64:
            return emu_eor_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_ANDS_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ands_immediate_w;
        case 64:
            return emu_ands_immediate_x;
        default:
            return NULL;
        }
    case ARM64_INST_MOVN:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_movn_w;
        case 64:
            return emu_movn_x;
        default:
            return NULL;
        }
    case ARM64_INST_MOVZ:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_movz_w;
        case 64:
            return emu_movz_x;
        default:
            return NULL;
        }
    case ARM64_INST_MOVK:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_movk_w;
        case 64:
            return emu_movk_x;
        default:
            return NULL;
        }
    case ARM64_INST_SBFM:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_sbfm_w;
        case 64:
            return emu_sbfm_x;
        default:
            return NULL;
        }
    case ARM64_INST_BFM:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_bfm_w;
        case 64:
            return emu_bfm_x;
        default:
            return NULL;
        }
    case ARM64_INST_UBFM:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ubfm_w;
        case 64:
            return emu_ubfm_x;
        default:
            return NULL;
        }
    default:
        return NULL;
    }
}
