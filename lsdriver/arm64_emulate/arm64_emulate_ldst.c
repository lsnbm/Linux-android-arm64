/*
ARM64 指令执行器：加载与存储类。

emu_* 函数直接调用访存硬件语义模板，完成 GPR 和 FP/SIMD 的单寄存器、成对、字面量、偏移、索引、非特权及预取访问。
同时覆盖独占访问、带序访问、RCpc/LDAP、LSE 原子读改写，以及 CAS/CASP 等原子比较交换指令。
emu_build_ldst_executor() 负责按指令、宽度、元素宽度、扩展类型和寻址模式选择具体执行函数。
*/
#include "emulate_inst.h"

static inline void emu_sturb_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sturb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sturh_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sturh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stur_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stur_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sttrb_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sttrb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sttrh_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sttrh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sttr_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sttr_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_sttr_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sttr_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldurb_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldurb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldurh_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldurh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldur_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldur_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldur_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldur_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldursb_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldursb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldursb_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldursb_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldursh_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldursh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldursh_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldursh_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldursw_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldursw_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldtrb_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldtrb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldtrh_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldtrh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldtr_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldtr_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldtr_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldtr_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldtrsb_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldtrsb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldtrsb_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldtrsb_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldtrsh_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldtrsh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldtrsh_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldtrsh_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldtrsw_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldtrsw_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strb_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    strb_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_strh_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    strh_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_post_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrb_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrb_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrh_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrh_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_post_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsb_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_post_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsb_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsh_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_post_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsh_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsw_gpr_post_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsw_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_strb_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    strb_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_strh_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    strh_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_pre_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrb_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrb_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrh_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrh_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_pre_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsb_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_pre_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsb_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsh_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_pre_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsh_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsw_gpr_pre_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldrsw_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, 0, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_strb_gpr_unsigned_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strb_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strh_gpr_unsigned_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strh_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_unsigned_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_unsigned_offset_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrb_gpr_unsigned_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrb_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrh_gpr_unsigned_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrh_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_unsigned_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_unsigned_offset_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_unsigned_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_unsigned_offset_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_unsigned_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_unsigned_offset_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsw_gpr_unsigned_offset_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsw_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strb_gpr_register_offset_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strb_w_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strh_gpr_register_offset_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strh_w_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_register_offset_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_w_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_register_offset_x_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_x_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrb_gpr_register_offset_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrb_w_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrh_gpr_register_offset_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrh_w_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_register_offset_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_w_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_register_offset_x_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_x_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_register_offset_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_w_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_register_offset_x_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_x_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_register_offset_w_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_w_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_register_offset_x_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_x_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsw_gpr_register_offset_x_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsw_x_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strb_gpr_register_offset_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strb_w_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strh_gpr_register_offset_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strh_w_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_register_offset_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_w_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_register_offset_x_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_x_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrb_gpr_register_offset_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrb_w_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrh_gpr_register_offset_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrh_w_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_register_offset_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_w_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_register_offset_x_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_x_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_register_offset_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_w_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_register_offset_x_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_x_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_register_offset_w_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_w_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_register_offset_x_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_x_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsw_gpr_register_offset_x_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsw_x_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strb_gpr_register_offset_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strb_w_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strh_gpr_register_offset_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strh_w_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_register_offset_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_w_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_register_offset_x_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_x_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrb_gpr_register_offset_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrb_w_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrh_gpr_register_offset_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrh_w_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_register_offset_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_w_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_register_offset_x_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_x_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_register_offset_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_w_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_register_offset_x_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_x_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_register_offset_w_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_w_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_register_offset_x_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_x_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsw_gpr_register_offset_x_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsw_x_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strb_gpr_register_offset_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strb_w_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_strh_gpr_register_offset_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    strh_w_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_register_offset_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_w_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_gpr_register_offset_x_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_x_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), read_gpr_or_zr(regs, entry->decoded.rt), entry->decoded.shift_amount, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrb_gpr_register_offset_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrb_w_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrh_gpr_register_offset_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrh_w_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_register_offset_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_w_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_gpr_register_offset_x_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_x_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_register_offset_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_w_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsb_gpr_register_offset_x_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsb_x_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_register_offset_w_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_w_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsh_gpr_register_offset_x_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsh_x_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsw_gpr_register_offset_x_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsw_x_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stur_fp_simd_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_b_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stur_fp_simd_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_h_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stur_fp_simd_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_s_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stur_fp_simd_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_d_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stur_fp_simd_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stur_q_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldur_fp_simd_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldur_b_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldur_fp_simd_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldur_h_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldur_fp_simd_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldur_s_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldur_fp_simd_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldur_d_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldur_fp_simd_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldur_q_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_post_index_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_b_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_post_index_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_h_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_post_index_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_s_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_post_index_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_d_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_post_index_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_q_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_post_index_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_b_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_post_index_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_h_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_post_index_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_s_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_post_index_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_d_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_post_index_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_q_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_pre_index_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_b_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_pre_index_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_h_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_pre_index_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_s_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_pre_index_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_d_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_pre_index_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    str_q_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_pre_index_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_b_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_pre_index_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_h_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_pre_index_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_s_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_pre_index_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_d_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_pre_index_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldr_q_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, &template_output4);

    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_unsigned_offset_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_b_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_unsigned_offset_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_h_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_unsigned_offset_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_unsigned_offset_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_unsigned_offset_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_unsigned_offset_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_b_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_unsigned_offset_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_h_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_unsigned_offset_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_unsigned_offset_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_unsigned_offset_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_b_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_b_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_h_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_h_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_s_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_s_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_d_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_d_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_q_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_q_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_b_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_b_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_h_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_h_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_s_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_s_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_d_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_d_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_q_uxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_q_register_offset_uxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_b_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_b_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_h_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_h_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_s_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_s_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_d_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_d_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_q_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_q_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_b_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_b_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_h_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_h_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_s_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_s_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_d_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_d_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_q_uxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_q_register_offset_uxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_b_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_b_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_h_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_h_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_s_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_s_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_d_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_d_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_q_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_q_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_b_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_b_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_h_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_h_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_s_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_s_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_d_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_d_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_q_sxtw(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_q_register_offset_sxtw(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_b_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_b_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_h_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_h_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_s_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_s_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_d_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_d_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_str_fp_simd_register_offset_q_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    str_q_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, (uint64_t)&fp_regs->q[entry->decoded.rt], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_b_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_b_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_h_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_h_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_s_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_s_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_d_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_d_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_fp_simd_register_offset_q_sxtx(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_q_register_offset_sxtx(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rm), entry->decoded.shift_amount, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldr_literal_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_w_literal(regs->pc, entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_literal_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldr_x_literal(regs->pc, entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldrsw_literal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldrsw_x_literal(regs->pc, entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_literal_fp_simd_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_s_literal(regs->pc, entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_literal_fp_simd_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_d_literal(regs->pc, entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldr_literal_fp_simd_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldr_q_literal(regs->pc, entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);

    regs->pc += 4;
    return;
}
static inline void emu_ldxrb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldxrb_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldxrh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldxrh_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldxr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldxr_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldxr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldxr_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaxrb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaxrb_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaxrh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaxrh_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaxr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaxr_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaxr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaxr_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldxp_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    ldxp_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, false);
    regs->pc += 4;
    return;
}
static inline void emu_ldxp_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    ldxp_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, true);
    regs->pc += 4;
    return;
}
static inline void emu_ldaxp_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    ldaxp_w(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, false);
    regs->pc += 4;
    return;
}
static inline void emu_ldaxp_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    ldaxp_x(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, true);
    regs->pc += 4;
    return;
}
static inline void emu_stxrb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    stxrb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stxrh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    stxrh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stxr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    stxr_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stxr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    stxr_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlxrb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    stlxrb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlxrh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    stlxrh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlxr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    stlxr_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlxr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    stlxr_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stxp_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    stxp_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_stxp_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    stxp_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_stlxp_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    stlxp_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_stlxp_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    stlxp_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, false);
    regs->pc += 4;
    return;
}
static inline void emu_casp_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    casp_w(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, template_output1, false);
    regs->pc += 4;
    return;
}
static inline void emu_casp_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    casp_x(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, template_output1, true);
    regs->pc += 4;
    return;
}
static inline void emu_caspa_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    caspa_w(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, template_output1, false);
    regs->pc += 4;
    return;
}
static inline void emu_caspa_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    caspa_x(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, template_output1, true);
    regs->pc += 4;
    return;
}
static inline void emu_caspl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    caspl_w(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, template_output1, false);
    regs->pc += 4;
    return;
}
static inline void emu_caspl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    caspl_x(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, template_output1, true);
    regs->pc += 4;
    return;
}
static inline void emu_caspal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    caspal_w(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, template_output1, false);
    regs->pc += 4;
    return;
}
static inline void emu_caspal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t expected0 = read_gpr_or_zr(regs, entry->decoded.rs);
    uint64_t expected1 = read_gpr_or_zr(regs, entry->decoded.rs + 1);
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    caspal_x(read_gpr_or_sp(regs, entry->decoded.rn), expected0, expected1, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt + 1), &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rs, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rs + 1, template_output1, true);
    regs->pc += 4;
    return;
}
static inline void emu_casb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_cash_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    cash_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_cas_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    cas_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_cas_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    cas_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casa_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casa_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casa_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casa_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_caslb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    caslb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_caslh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    caslh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casalb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casalb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casalh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casalh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_casal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rs, &discard);
    casal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddalb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddalb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddalh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddalh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldadd_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldadd_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldadd_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldadd_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddlb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddlb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddlh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddlh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaddah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaddah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldadda_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldadda_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldadda_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldadda_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclrb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclrb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclrh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclrh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclr_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclr_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclrlb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclrlb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclrlh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclrlh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclrl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclrl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclrl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclrl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclrab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclrab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclrah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclrah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclra_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclra_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclra_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclra_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclralb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclralb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclralh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclralh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclral_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclral_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldclral_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldclral_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeorb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeorb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeorh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeorh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeor_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeor_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeor_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeor_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeorlb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeorlb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeorlh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeorlh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeorl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeorl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeorl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeorl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeorab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeorab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeorah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeorah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeora_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeora_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeora_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeora_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeoralb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeoralb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeoralh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeoralh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeoral_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeoral_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldeoral_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldeoral_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldseth_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldseth_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldset_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldset_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldset_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetlb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetlb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetlh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetlh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldseta_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldseta_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldseta_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldseta_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetalb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetalb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetalh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetalh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsetal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsetal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmax_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmax_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmax_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmax_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxlb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxlb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxlh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxlh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxa_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxa_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxa_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxa_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxalb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxalb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxalh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxalh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmaxal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmaxal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmin_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmin_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmin_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmin_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminlb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminlb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminlh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminlh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmina_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmina_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsmina_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsmina_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminalb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminalb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminalh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminalh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldsminal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldsminal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumax_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumax_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumax_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumax_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxlb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxlb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxlh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxlh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxa_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxa_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxa_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxa_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxalb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxalb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxalh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxalh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumaxal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumaxal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumin_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumin_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumin_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumin_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminlb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminlb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminlh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminlh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumina_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumina_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldumina_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldumina_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminalb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminalb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminalh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminalh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_lduminal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    lduminal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swph_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swph_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swp_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swp_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swp_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swp_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swplb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swplb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swplh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swplh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpl_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpl_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpl_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpl_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpab_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpab_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpah_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpah_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpa_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpa_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpa_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpa_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpalb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpalb_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpalh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpalh_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpal_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpal_w(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_swpal_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    swpal_x(read_gpr_or_sp(regs, entry->decoded.rn), read_gpr_or_zr(regs, entry->decoded.rs), 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stllrb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stllrb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stllrh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stllrh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stllr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stllr_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stllr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stllr_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlrb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stlrb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlrh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stlrh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stlr_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stlr_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldlarb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldlarb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldlarh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldlarh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldlar_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldlar_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldlar_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldlar_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldarb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldarb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldarh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldarh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldar_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldar_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldar_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldar_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaprb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaprb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldaprh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldaprh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapr_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapr_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), 0, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlurb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stlurb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlurh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stlurh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlur_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stlur_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stlur_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stlur_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapurb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapurb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapurh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapurh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapur_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapur_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapur_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapur_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapursb_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapursb_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapursb_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapursb_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapursh_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapursh_w_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapursh_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapursh_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldapursw_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t discard;
    uint64_t *output = gpr_output_or_discard(regs, entry->decoded.rt, &discard);
    ldapursw_x_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, output, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldnp_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    ldnp_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, false);
    regs->pc += 4;
    return;
}
static inline void emu_ldnp_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    ldnp_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, true);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_gpr_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    ldp_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, false);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_gpr_offset_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    ldp_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, true);
    regs->pc += 4;
    return;
}
static inline void emu_ldpsw_offset_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    ldpsw_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, true);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_gpr_post_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldpsw_post_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldpsw_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, false);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, false);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_gpr_pre_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldpsw_pre_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    uint64_t template_output1 __attribute__((__uninitialized__));
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldpsw_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &template_output0, &template_output1, 0, 0, &template_output4);
    write_gpr_or_zr(regs, entry->decoded.rt, template_output0, true);
    write_gpr_or_zr(regs, entry->decoded.rt2, template_output1, true);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stnp_gpr_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stnp_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stnp_gpr_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stnp_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stp_gpr_offset_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_w_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stp_gpr_offset_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_x_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stp_gpr_post_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_w_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stp_gpr_post_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_x_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stp_gpr_pre_index_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_w_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stp_gpr_pre_index_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_x_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, read_gpr_or_zr(regs, entry->decoded.rt), read_gpr_or_zr(regs, entry->decoded.rt2), 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldnp_fp_simd_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldnp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldnp_fp_simd_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldnp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldnp_fp_simd_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldnp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_fp_simd_offset_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_fp_simd_offset_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_fp_simd_offset_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ldp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_fp_simd_post_index_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_s_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_fp_simd_post_index_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_d_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_fp_simd_post_index_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_q_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_fp_simd_pre_index_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_s_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_fp_simd_pre_index_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_d_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_ldp_fp_simd_pre_index_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    ldp_q_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, &fp_regs->q[entry->decoded.rt2], 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stnp_fp_simd_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stnp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stnp_fp_simd_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stnp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stnp_fp_simd_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stnp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stp_fp_simd_offset_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_s_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stp_fp_simd_offset_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_d_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stp_fp_simd_offset_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    stp_q_base_offset(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_stp_fp_simd_post_index_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_s_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stp_fp_simd_post_index_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_d_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stp_fp_simd_post_index_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_q_post_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stp_fp_simd_pre_index_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_s_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stp_fp_simd_pre_index_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_d_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_stp_fp_simd_pre_index_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output4 __attribute__((__uninitialized__));
    stp_q_pre_index(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.offset, (uint64_t)&fp_regs->q[entry->decoded.rt], (uint64_t)&fp_regs->q[entry->decoded.rt2], 0, 0, 0, 0, 0, &template_output4);
    write_gpr_or_sp(regs, entry->decoded.rn, template_output4);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op0(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op1(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op2(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op3(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op4(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op5(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op6(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op7(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op8(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op9(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op10(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op11(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op12(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op13(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op14(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op15(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op16(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op17(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op18(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op19(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op20(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op21(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op22(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op23(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op24(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op24(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op25(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op25(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op26(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op26(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op27(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op27(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op28(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op28(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op29(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op29(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op30(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op30(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_literal_op31(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op31(regs->pc + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op0(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op1(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op2(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op3(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op4(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op5(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op6(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op7(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op8(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op9(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op10(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op11(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op12(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op13(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op14(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op15(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op16(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op17(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op18(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op19(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op20(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op21(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op22(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op23(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op24(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op24(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op25(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op25(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op26(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op26(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op27(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op27(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op28(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op28(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op29(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op29(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op30(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op30(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_unsigned_offset_op31(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op31(read_gpr_or_sp(regs, entry->decoded.rn) + entry->decoded.offset, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op0(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op1(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op2(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op3(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op4(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op5(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op6(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op7(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op8(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op9(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op10(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op11(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op12(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op13(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op14(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op15(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op16(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op17(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op18(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op19(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op20(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op21(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op22(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtw_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op23(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(uint32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op0(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op1(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op2(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op3(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op4(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op5(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op6(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op7(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op8(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op9(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op10(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op11(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op12(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op13(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op14(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op15(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op16(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op17(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op18(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op19(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op20(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op21(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op22(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_uxtx_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op23(read_gpr_or_sp(regs, entry->decoded.rn) + (read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op0(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op1(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op2(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op3(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op4(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op5(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op6(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op7(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op8(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op9(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op10(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op11(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op12(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op13(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op14(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op15(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op16(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op16(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op17(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op17(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op18(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op18(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op19(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op19(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op20(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op20(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op21(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op21(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op22(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op22(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_prfm_register_offset_sxtw_op23(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    prfm_op23(read_gpr_or_sp(regs, entry->decoded.rn) + ((uint64_t)(int64_t)(int32_t)read_gpr_or_zr(regs, entry->decoded.rm) << entry->decoded.shift_amount), 0, 0, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ld1_single_structure_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ld1_b_lane(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.lane_index, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ld1_single_structure_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ld1_h_lane(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.lane_index, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ld1_single_structure_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ld1_s_lane(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.lane_index, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_ld1_single_structure_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ld1_d_lane(read_gpr_or_sp(regs, entry->decoded.rn), entry->decoded.lane_index, 0, 0, 0, &fp_regs->q[entry->decoded.rt], 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_st1_single_structure_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    st1_b_lane(read_gpr_or_sp(regs, entry->decoded.rn), (uint64_t)&fp_regs->q[entry->decoded.rt], entry->decoded.lane_index, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_st1_single_structure_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    st1_h_lane(read_gpr_or_sp(regs, entry->decoded.rn), (uint64_t)&fp_regs->q[entry->decoded.rt], entry->decoded.lane_index, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_st1_single_structure_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    st1_s_lane(read_gpr_or_sp(regs, entry->decoded.rn), (uint64_t)&fp_regs->q[entry->decoded.rt], entry->decoded.lane_index, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
static inline void emu_st1_single_structure_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    st1_d_lane(read_gpr_or_sp(regs, entry->decoded.rn), (uint64_t)&fp_regs->q[entry->decoded.rt], entry->decoded.lane_index, 0, 0, 0, 0, 0, 0, 0);
    regs->pc += 4;
    return;
}
void (*emu_build_ldst_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_LD1:
        switch (decoded->element_width)
        {
        case 8:
            return emu_ld1_single_structure_b;
        case 16:
            return emu_ld1_single_structure_h;
        case 32:
            return emu_ld1_single_structure_s;
        case 64:
            return emu_ld1_single_structure_d;
        default:
            return 0;
        }
    case ARM64_INST_ST1:
        switch (decoded->element_width)
        {
        case 8:
            return emu_st1_single_structure_b;
        case 16:
            return emu_st1_single_structure_h;
        case 32:
            return emu_st1_single_structure_s;
        case 64:
            return emu_st1_single_structure_d;
        default:
            return 0;
        }
    case ARM64_INST_STURB_GPR:
        return emu_sturb_gpr_w;
    case ARM64_INST_STURH_GPR:
        return emu_sturh_gpr_w;
    case ARM64_INST_STUR_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stur_gpr_w;
        default:
            return emu_stur_gpr_x;
        }
    case ARM64_INST_STTRB_GPR:
        return emu_sttrb_gpr_w;
    case ARM64_INST_STTRH_GPR:
        return emu_sttrh_gpr_w;
    case ARM64_INST_STTR_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_sttr_gpr_w;
        default:
            return emu_sttr_gpr_x;
        }
    case ARM64_INST_STRB_GPR_POST_INDEX:
        return emu_strb_gpr_post_index_w;
    case ARM64_INST_STRH_GPR_POST_INDEX:
        return emu_strh_gpr_post_index_w;
    case ARM64_INST_STR_GPR_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_str_gpr_post_index_w;
        default:
            return emu_str_gpr_post_index_x;
        }
    case ARM64_INST_STRB_GPR_PRE_INDEX:
        return emu_strb_gpr_pre_index_w;
    case ARM64_INST_STRH_GPR_PRE_INDEX:
        return emu_strh_gpr_pre_index_w;
    case ARM64_INST_STR_GPR_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_str_gpr_pre_index_w;
        default:
            return emu_str_gpr_pre_index_x;
        }
    case ARM64_INST_STRB_GPR_UNSIGNED_OFFSET:
        return emu_strb_gpr_unsigned_offset_w;
    case ARM64_INST_STRH_GPR_UNSIGNED_OFFSET:
        return emu_strh_gpr_unsigned_offset_w;
    case ARM64_INST_STR_GPR_UNSIGNED_OFFSET:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_str_gpr_unsigned_offset_w;
        default:
            return emu_str_gpr_unsigned_offset_x;
        }
    case ARM64_INST_STRB_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_strb_gpr_register_offset_w_uxtw;
        case 3:
            return emu_strb_gpr_register_offset_w_uxtx;
        case 6:
            return emu_strb_gpr_register_offset_w_sxtw;
        case 7:
            return emu_strb_gpr_register_offset_w_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_STRH_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_strh_gpr_register_offset_w_uxtw;
        case 3:
            return emu_strh_gpr_register_offset_w_uxtx;
        case 6:
            return emu_strh_gpr_register_offset_w_sxtw;
        case 7:
            return emu_strh_gpr_register_offset_w_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_STR_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_str_gpr_register_offset_w_uxtw;
            default:
                return emu_str_gpr_register_offset_x_uxtw;
            }
        case 3:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_str_gpr_register_offset_w_uxtx;
            default:
                return emu_str_gpr_register_offset_x_uxtx;
            }
        case 6:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_str_gpr_register_offset_w_sxtw;
            default:
                return emu_str_gpr_register_offset_x_sxtw;
            }
        case 7:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_str_gpr_register_offset_w_sxtx;
            default:
                return emu_str_gpr_register_offset_x_sxtx;
            }
        default:
            return 0;
        }
    case ARM64_INST_LDURB_GPR:
        return emu_ldurb_gpr_w;
    case ARM64_INST_LDURH_GPR:
        return emu_ldurh_gpr_w;
    case ARM64_INST_LDUR_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldur_gpr_w;
        default:
            return emu_ldur_gpr_x;
        }
    case ARM64_INST_LDURSB_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldursb_gpr_w;
        default:
            return emu_ldursb_gpr_x;
        }
    case ARM64_INST_LDURSH_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldursh_gpr_w;
        default:
            return emu_ldursh_gpr_x;
        }
    case ARM64_INST_LDURSW_GPR:
        return emu_ldursw_gpr_x;
    case ARM64_INST_LDTRB_GPR:
        return emu_ldtrb_gpr_w;
    case ARM64_INST_LDTRH_GPR:
        return emu_ldtrh_gpr_w;
    case ARM64_INST_LDTR_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldtr_gpr_w;
        default:
            return emu_ldtr_gpr_x;
        }
    case ARM64_INST_LDTRSB_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldtrsb_gpr_w;
        default:
            return emu_ldtrsb_gpr_x;
        }
    case ARM64_INST_LDTRSH_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldtrsh_gpr_w;
        default:
            return emu_ldtrsh_gpr_x;
        }
    case ARM64_INST_LDTRSW_GPR:
        return emu_ldtrsw_gpr_x;
    case ARM64_INST_LDRB_GPR_POST_INDEX:
        return emu_ldrb_gpr_post_index_w;
    case ARM64_INST_LDRH_GPR_POST_INDEX:
        return emu_ldrh_gpr_post_index_w;
    case ARM64_INST_LDR_GPR_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldr_gpr_post_index_w;
        default:
            return emu_ldr_gpr_post_index_x;
        }
    case ARM64_INST_LDRSB_GPR_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldrsb_gpr_post_index_w;
        default:
            return emu_ldrsb_gpr_post_index_x;
        }
    case ARM64_INST_LDRSH_GPR_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldrsh_gpr_post_index_w;
        default:
            return emu_ldrsh_gpr_post_index_x;
        }
    case ARM64_INST_LDRSW_GPR_POST_INDEX:
        return emu_ldrsw_gpr_post_index_x;
    case ARM64_INST_LDRB_GPR_PRE_INDEX:
        return emu_ldrb_gpr_pre_index_w;
    case ARM64_INST_LDRH_GPR_PRE_INDEX:
        return emu_ldrh_gpr_pre_index_w;
    case ARM64_INST_LDR_GPR_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldr_gpr_pre_index_w;
        default:
            return emu_ldr_gpr_pre_index_x;
        }
    case ARM64_INST_LDRSB_GPR_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldrsb_gpr_pre_index_w;
        default:
            return emu_ldrsb_gpr_pre_index_x;
        }
    case ARM64_INST_LDRSH_GPR_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldrsh_gpr_pre_index_w;
        default:
            return emu_ldrsh_gpr_pre_index_x;
        }
    case ARM64_INST_LDRSW_GPR_PRE_INDEX:
        return emu_ldrsw_gpr_pre_index_x;
    case ARM64_INST_LDRB_GPR_UNSIGNED_OFFSET:
        return emu_ldrb_gpr_unsigned_offset_w;
    case ARM64_INST_LDRH_GPR_UNSIGNED_OFFSET:
        return emu_ldrh_gpr_unsigned_offset_w;
    case ARM64_INST_LDR_GPR_UNSIGNED_OFFSET:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldr_gpr_unsigned_offset_w;
        default:
            return emu_ldr_gpr_unsigned_offset_x;
        }
    case ARM64_INST_LDRSB_GPR_UNSIGNED_OFFSET:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldrsb_gpr_unsigned_offset_w;
        default:
            return emu_ldrsb_gpr_unsigned_offset_x;
        }
    case ARM64_INST_LDRSH_GPR_UNSIGNED_OFFSET:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldrsh_gpr_unsigned_offset_w;
        default:
            return emu_ldrsh_gpr_unsigned_offset_x;
        }
    case ARM64_INST_LDRSW_GPR_UNSIGNED_OFFSET:
        return emu_ldrsw_gpr_unsigned_offset_x;
    case ARM64_INST_LDRB_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_ldrb_gpr_register_offset_w_uxtw;
        case 3:
            return emu_ldrb_gpr_register_offset_w_uxtx;
        case 6:
            return emu_ldrb_gpr_register_offset_w_sxtw;
        case 7:
            return emu_ldrb_gpr_register_offset_w_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_LDRH_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_ldrh_gpr_register_offset_w_uxtw;
        case 3:
            return emu_ldrh_gpr_register_offset_w_uxtx;
        case 6:
            return emu_ldrh_gpr_register_offset_w_sxtw;
        case 7:
            return emu_ldrh_gpr_register_offset_w_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_LDR_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldr_gpr_register_offset_w_uxtw;
            default:
                return emu_ldr_gpr_register_offset_x_uxtw;
            }
        case 3:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldr_gpr_register_offset_w_uxtx;
            default:
                return emu_ldr_gpr_register_offset_x_uxtx;
            }
        case 6:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldr_gpr_register_offset_w_sxtw;
            default:
                return emu_ldr_gpr_register_offset_x_sxtw;
            }
        case 7:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldr_gpr_register_offset_w_sxtx;
            default:
                return emu_ldr_gpr_register_offset_x_sxtx;
            }
        default:
            return 0;
        }
    case ARM64_INST_LDRSB_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldrsb_gpr_register_offset_w_uxtw;
            default:
                return emu_ldrsb_gpr_register_offset_x_uxtw;
            }
        case 3:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldrsb_gpr_register_offset_w_uxtx;
            default:
                return emu_ldrsb_gpr_register_offset_x_uxtx;
            }
        case 6:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldrsb_gpr_register_offset_w_sxtw;
            default:
                return emu_ldrsb_gpr_register_offset_x_sxtw;
            }
        case 7:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldrsb_gpr_register_offset_w_sxtx;
            default:
                return emu_ldrsb_gpr_register_offset_x_sxtx;
            }
        default:
            return 0;
        }
    case ARM64_INST_LDRSH_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldrsh_gpr_register_offset_w_uxtw;
            default:
                return emu_ldrsh_gpr_register_offset_x_uxtw;
            }
        case 3:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldrsh_gpr_register_offset_w_uxtx;
            default:
                return emu_ldrsh_gpr_register_offset_x_uxtx;
            }
        case 6:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldrsh_gpr_register_offset_w_sxtw;
            default:
                return emu_ldrsh_gpr_register_offset_x_sxtw;
            }
        case 7:
            switch (decoded->operand_width)
            {
            case 32:
                return emu_ldrsh_gpr_register_offset_w_sxtx;
            default:
                return emu_ldrsh_gpr_register_offset_x_sxtx;
            }
        default:
            return 0;
        }
    case ARM64_INST_LDRSW_GPR_REGISTER_OFFSET:
        switch (decoded->extend_type)
        {
        case 2:
            return emu_ldrsw_gpr_register_offset_x_uxtw;
        case 3:
            return emu_ldrsw_gpr_register_offset_x_uxtx;
        case 6:
            return emu_ldrsw_gpr_register_offset_x_sxtw;
        case 7:
            return emu_ldrsw_gpr_register_offset_x_sxtx;
        default:
            return 0;
        }
    case ARM64_INST_STUR_FP_SIMD:
        switch (decoded->operand_width)
        {
        case 8:
            return emu_stur_fp_simd_b;
        case 16:
            return emu_stur_fp_simd_h;
        case 32:
            return emu_stur_fp_simd_s;
        case 64:
            return emu_stur_fp_simd_d;
        case 128:
            return emu_stur_fp_simd_q;
        default:
            return 0;
        }
    case ARM64_INST_LDUR_FP_SIMD:
        switch (decoded->operand_width)
        {
        case 8:
            return emu_ldur_fp_simd_b;
        case 16:
            return emu_ldur_fp_simd_h;
        case 32:
            return emu_ldur_fp_simd_s;
        case 64:
            return emu_ldur_fp_simd_d;
        case 128:
            return emu_ldur_fp_simd_q;
        default:
            return 0;
        }
    case ARM64_INST_STR_FP_SIMD_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 8:
            return emu_str_fp_simd_post_index_b;
        case 16:
            return emu_str_fp_simd_post_index_h;
        case 32:
            return emu_str_fp_simd_post_index_s;
        case 64:
            return emu_str_fp_simd_post_index_d;
        case 128:
            return emu_str_fp_simd_post_index_q;
        default:
            return 0;
        }
    case ARM64_INST_LDR_FP_SIMD_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 8:
            return emu_ldr_fp_simd_post_index_b;
        case 16:
            return emu_ldr_fp_simd_post_index_h;
        case 32:
            return emu_ldr_fp_simd_post_index_s;
        case 64:
            return emu_ldr_fp_simd_post_index_d;
        case 128:
            return emu_ldr_fp_simd_post_index_q;
        default:
            return 0;
        }
    case ARM64_INST_STR_FP_SIMD_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 8:
            return emu_str_fp_simd_pre_index_b;
        case 16:
            return emu_str_fp_simd_pre_index_h;
        case 32:
            return emu_str_fp_simd_pre_index_s;
        case 64:
            return emu_str_fp_simd_pre_index_d;
        case 128:
            return emu_str_fp_simd_pre_index_q;
        default:
            return 0;
        }
    case ARM64_INST_LDR_FP_SIMD_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 8:
            return emu_ldr_fp_simd_pre_index_b;
        case 16:
            return emu_ldr_fp_simd_pre_index_h;
        case 32:
            return emu_ldr_fp_simd_pre_index_s;
        case 64:
            return emu_ldr_fp_simd_pre_index_d;
        case 128:
            return emu_ldr_fp_simd_pre_index_q;
        default:
            return 0;
        }
    case ARM64_INST_STR_FP_SIMD_UNSIGNED_OFFSET:
        switch (decoded->operand_width)
        {
        case 8:
            return emu_str_fp_simd_unsigned_offset_b;
        case 16:
            return emu_str_fp_simd_unsigned_offset_h;
        case 32:
            return emu_str_fp_simd_unsigned_offset_s;
        case 64:
            return emu_str_fp_simd_unsigned_offset_d;
        case 128:
            return emu_str_fp_simd_unsigned_offset_q;
        default:
            return 0;
        }
    case ARM64_INST_LDR_FP_SIMD_UNSIGNED_OFFSET:
        switch (decoded->operand_width)
        {
        case 8:
            return emu_ldr_fp_simd_unsigned_offset_b;
        case 16:
            return emu_ldr_fp_simd_unsigned_offset_h;
        case 32:
            return emu_ldr_fp_simd_unsigned_offset_s;
        case 64:
            return emu_ldr_fp_simd_unsigned_offset_d;
        case 128:
            return emu_ldr_fp_simd_unsigned_offset_q;
        default:
            return 0;
        }
    case ARM64_INST_STR_FP_SIMD_REGISTER_OFFSET:
        switch (decoded->operand_width)
        {
        case 8:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_str_fp_simd_register_offset_b_uxtw;
            case 3:
                return emu_str_fp_simd_register_offset_b_uxtx;
            case 6:
                return emu_str_fp_simd_register_offset_b_sxtw;
            case 7:
                return emu_str_fp_simd_register_offset_b_sxtx;
            default:
                return 0;
            }
        case 16:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_str_fp_simd_register_offset_h_uxtw;
            case 3:
                return emu_str_fp_simd_register_offset_h_uxtx;
            case 6:
                return emu_str_fp_simd_register_offset_h_sxtw;
            case 7:
                return emu_str_fp_simd_register_offset_h_sxtx;
            default:
                return 0;
            }
        case 32:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_str_fp_simd_register_offset_s_uxtw;
            case 3:
                return emu_str_fp_simd_register_offset_s_uxtx;
            case 6:
                return emu_str_fp_simd_register_offset_s_sxtw;
            case 7:
                return emu_str_fp_simd_register_offset_s_sxtx;
            default:
                return 0;
            }
        case 64:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_str_fp_simd_register_offset_d_uxtw;
            case 3:
                return emu_str_fp_simd_register_offset_d_uxtx;
            case 6:
                return emu_str_fp_simd_register_offset_d_sxtw;
            case 7:
                return emu_str_fp_simd_register_offset_d_sxtx;
            default:
                return 0;
            }
        case 128:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_str_fp_simd_register_offset_q_uxtw;
            case 3:
                return emu_str_fp_simd_register_offset_q_uxtx;
            case 6:
                return emu_str_fp_simd_register_offset_q_sxtw;
            case 7:
                return emu_str_fp_simd_register_offset_q_sxtx;
            default:
                return 0;
            }
        default:
            return 0;
        }
    case ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET:
        switch (decoded->operand_width)
        {
        case 8:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_ldr_fp_simd_register_offset_b_uxtw;
            case 3:
                return emu_ldr_fp_simd_register_offset_b_uxtx;
            case 6:
                return emu_ldr_fp_simd_register_offset_b_sxtw;
            case 7:
                return emu_ldr_fp_simd_register_offset_b_sxtx;
            default:
                return 0;
            }
        case 16:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_ldr_fp_simd_register_offset_h_uxtw;
            case 3:
                return emu_ldr_fp_simd_register_offset_h_uxtx;
            case 6:
                return emu_ldr_fp_simd_register_offset_h_sxtw;
            case 7:
                return emu_ldr_fp_simd_register_offset_h_sxtx;
            default:
                return 0;
            }
        case 32:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_ldr_fp_simd_register_offset_s_uxtw;
            case 3:
                return emu_ldr_fp_simd_register_offset_s_uxtx;
            case 6:
                return emu_ldr_fp_simd_register_offset_s_sxtw;
            case 7:
                return emu_ldr_fp_simd_register_offset_s_sxtx;
            default:
                return 0;
            }
        case 64:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_ldr_fp_simd_register_offset_d_uxtw;
            case 3:
                return emu_ldr_fp_simd_register_offset_d_uxtx;
            case 6:
                return emu_ldr_fp_simd_register_offset_d_sxtw;
            case 7:
                return emu_ldr_fp_simd_register_offset_d_sxtx;
            default:
                return 0;
            }
        case 128:
            switch (decoded->extend_type)
            {
            case 2:
                return emu_ldr_fp_simd_register_offset_q_uxtw;
            case 3:
                return emu_ldr_fp_simd_register_offset_q_uxtx;
            case 6:
                return emu_ldr_fp_simd_register_offset_q_sxtw;
            case 7:
                return emu_ldr_fp_simd_register_offset_q_sxtx;
            default:
                return 0;
            }
        default:
            return 0;
        }
    case ARM64_INST_LDXRB:
        return emu_ldxrb_w;
    case ARM64_INST_LDXRH:
        return emu_ldxrh_w;
    case ARM64_INST_LDXR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldxr_w;
        default:
            return emu_ldxr_x;
        }
    case ARM64_INST_LDAXRB:
        return emu_ldaxrb_w;
    case ARM64_INST_LDAXRH:
        return emu_ldaxrh_w;
    case ARM64_INST_LDAXR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldaxr_w;
        default:
            return emu_ldaxr_x;
        }
    case ARM64_INST_LDXP:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldxp_w;
        default:
            return emu_ldxp_x;
        }
    case ARM64_INST_LDAXP:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldaxp_w;
        default:
            return emu_ldaxp_x;
        }
    case ARM64_INST_STXRB:
        return emu_stxrb_w;
    case ARM64_INST_STXRH:
        return emu_stxrh_w;
    case ARM64_INST_STXR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stxr_w;
        default:
            return emu_stxr_x;
        }
    case ARM64_INST_STLXRB:
        return emu_stlxrb_w;
    case ARM64_INST_STLXRH:
        return emu_stlxrh_w;
    case ARM64_INST_STLXR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stlxr_w;
        default:
            return emu_stlxr_x;
        }
    case ARM64_INST_STXP:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stxp_w;
        default:
            return emu_stxp_x;
        }
    case ARM64_INST_STLXP:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stlxp_w;
        default:
            return emu_stlxp_x;
        }
    case ARM64_INST_CASP:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_casp_w;
        default:
            return emu_casp_x;
        }
    case ARM64_INST_CASPA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_caspa_w;
        default:
            return emu_caspa_x;
        }
    case ARM64_INST_CASPL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_caspl_w;
        default:
            return emu_caspl_x;
        }
    case ARM64_INST_CASPAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_caspal_w;
        default:
            return emu_caspal_x;
        }
    case ARM64_INST_CASB:
        return emu_casb_w;
    case ARM64_INST_CASH:
        return emu_cash_w;
    case ARM64_INST_CAS:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_cas_w;
        default:
            return emu_cas_x;
        }
    case ARM64_INST_CASAB:
        return emu_casab_w;
    case ARM64_INST_CASAH:
        return emu_casah_w;
    case ARM64_INST_CASA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_casa_w;
        default:
            return emu_casa_x;
        }
    case ARM64_INST_CASLB:
        return emu_caslb_w;
    case ARM64_INST_CASLH:
        return emu_caslh_w;
    case ARM64_INST_CASL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_casl_w;
        default:
            return emu_casl_x;
        }
    case ARM64_INST_CASALB:
        return emu_casalb_w;
    case ARM64_INST_CASALH:
        return emu_casalh_w;
    case ARM64_INST_CASAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_casal_w;
        default:
            return emu_casal_x;
        }
    case ARM64_INST_LDADDALB:
        return emu_ldaddalb_w;
    case ARM64_INST_LDADDALH:
        return emu_ldaddalh_w;
    case ARM64_INST_LDADDAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldaddal_w;
        default:
            return emu_ldaddal_x;
        }
    case ARM64_INST_LDADDB:
        return emu_ldaddb_w;
    case ARM64_INST_LDADDH:
        return emu_ldaddh_w;
    case ARM64_INST_LDADD:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldadd_w;
        default:
            return emu_ldadd_x;
        }
    case ARM64_INST_LDADDLB:
        return emu_ldaddlb_w;
    case ARM64_INST_LDADDLH:
        return emu_ldaddlh_w;
    case ARM64_INST_LDADDL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldaddl_w;
        default:
            return emu_ldaddl_x;
        }
    case ARM64_INST_LDADDAB:
        return emu_ldaddab_w;
    case ARM64_INST_LDADDAH:
        return emu_ldaddah_w;
    case ARM64_INST_LDADDA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldadda_w;
        default:
            return emu_ldadda_x;
        }
    case ARM64_INST_LDCLRB:
        return emu_ldclrb_w;
    case ARM64_INST_LDCLRH:
        return emu_ldclrh_w;
    case ARM64_INST_LDCLR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldclr_w;
        default:
            return emu_ldclr_x;
        }
    case ARM64_INST_LDCLRLB:
        return emu_ldclrlb_w;
    case ARM64_INST_LDCLRLH:
        return emu_ldclrlh_w;
    case ARM64_INST_LDCLRL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldclrl_w;
        default:
            return emu_ldclrl_x;
        }
    case ARM64_INST_LDCLRAB:
        return emu_ldclrab_w;
    case ARM64_INST_LDCLRAH:
        return emu_ldclrah_w;
    case ARM64_INST_LDCLRA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldclra_w;
        default:
            return emu_ldclra_x;
        }
    case ARM64_INST_LDCLRALB:
        return emu_ldclralb_w;
    case ARM64_INST_LDCLRALH:
        return emu_ldclralh_w;
    case ARM64_INST_LDCLRAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldclral_w;
        default:
            return emu_ldclral_x;
        }
    case ARM64_INST_LDEORB:
        return emu_ldeorb_w;
    case ARM64_INST_LDEORH:
        return emu_ldeorh_w;
    case ARM64_INST_LDEOR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldeor_w;
        default:
            return emu_ldeor_x;
        }
    case ARM64_INST_LDEORLB:
        return emu_ldeorlb_w;
    case ARM64_INST_LDEORLH:
        return emu_ldeorlh_w;
    case ARM64_INST_LDEORL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldeorl_w;
        default:
            return emu_ldeorl_x;
        }
    case ARM64_INST_LDEORAB:
        return emu_ldeorab_w;
    case ARM64_INST_LDEORAH:
        return emu_ldeorah_w;
    case ARM64_INST_LDEORA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldeora_w;
        default:
            return emu_ldeora_x;
        }
    case ARM64_INST_LDEORALB:
        return emu_ldeoralb_w;
    case ARM64_INST_LDEORALH:
        return emu_ldeoralh_w;
    case ARM64_INST_LDEORAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldeoral_w;
        default:
            return emu_ldeoral_x;
        }
    case ARM64_INST_LDSETB:
        return emu_ldsetb_w;
    case ARM64_INST_LDSETH:
        return emu_ldseth_w;
    case ARM64_INST_LDSET:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldset_w;
        default:
            return emu_ldset_x;
        }
    case ARM64_INST_LDSETLB:
        return emu_ldsetlb_w;
    case ARM64_INST_LDSETLH:
        return emu_ldsetlh_w;
    case ARM64_INST_LDSETL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsetl_w;
        default:
            return emu_ldsetl_x;
        }
    case ARM64_INST_LDSETAB:
        return emu_ldsetab_w;
    case ARM64_INST_LDSETAH:
        return emu_ldsetah_w;
    case ARM64_INST_LDSETA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldseta_w;
        default:
            return emu_ldseta_x;
        }
    case ARM64_INST_LDSETALB:
        return emu_ldsetalb_w;
    case ARM64_INST_LDSETALH:
        return emu_ldsetalh_w;
    case ARM64_INST_LDSETAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsetal_w;
        default:
            return emu_ldsetal_x;
        }
    case ARM64_INST_LDSMAXB:
        return emu_ldsmaxb_w;
    case ARM64_INST_LDSMAXH:
        return emu_ldsmaxh_w;
    case ARM64_INST_LDSMAX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsmax_w;
        default:
            return emu_ldsmax_x;
        }
    case ARM64_INST_LDSMAXLB:
        return emu_ldsmaxlb_w;
    case ARM64_INST_LDSMAXLH:
        return emu_ldsmaxlh_w;
    case ARM64_INST_LDSMAXL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsmaxl_w;
        default:
            return emu_ldsmaxl_x;
        }
    case ARM64_INST_LDSMAXAB:
        return emu_ldsmaxab_w;
    case ARM64_INST_LDSMAXAH:
        return emu_ldsmaxah_w;
    case ARM64_INST_LDSMAXA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsmaxa_w;
        default:
            return emu_ldsmaxa_x;
        }
    case ARM64_INST_LDSMAXALB:
        return emu_ldsmaxalb_w;
    case ARM64_INST_LDSMAXALH:
        return emu_ldsmaxalh_w;
    case ARM64_INST_LDSMAXAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsmaxal_w;
        default:
            return emu_ldsmaxal_x;
        }
    case ARM64_INST_LDSMINB:
        return emu_ldsminb_w;
    case ARM64_INST_LDSMINH:
        return emu_ldsminh_w;
    case ARM64_INST_LDSMIN:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsmin_w;
        default:
            return emu_ldsmin_x;
        }
    case ARM64_INST_LDSMINLB:
        return emu_ldsminlb_w;
    case ARM64_INST_LDSMINLH:
        return emu_ldsminlh_w;
    case ARM64_INST_LDSMINL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsminl_w;
        default:
            return emu_ldsminl_x;
        }
    case ARM64_INST_LDSMINAB:
        return emu_ldsminab_w;
    case ARM64_INST_LDSMINAH:
        return emu_ldsminah_w;
    case ARM64_INST_LDSMINA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsmina_w;
        default:
            return emu_ldsmina_x;
        }
    case ARM64_INST_LDSMINALB:
        return emu_ldsminalb_w;
    case ARM64_INST_LDSMINALH:
        return emu_ldsminalh_w;
    case ARM64_INST_LDSMINAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldsminal_w;
        default:
            return emu_ldsminal_x;
        }
    case ARM64_INST_LDUMAXB:
        return emu_ldumaxb_w;
    case ARM64_INST_LDUMAXH:
        return emu_ldumaxh_w;
    case ARM64_INST_LDUMAX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldumax_w;
        default:
            return emu_ldumax_x;
        }
    case ARM64_INST_LDUMAXLB:
        return emu_ldumaxlb_w;
    case ARM64_INST_LDUMAXLH:
        return emu_ldumaxlh_w;
    case ARM64_INST_LDUMAXL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldumaxl_w;
        default:
            return emu_ldumaxl_x;
        }
    case ARM64_INST_LDUMAXAB:
        return emu_ldumaxab_w;
    case ARM64_INST_LDUMAXAH:
        return emu_ldumaxah_w;
    case ARM64_INST_LDUMAXA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldumaxa_w;
        default:
            return emu_ldumaxa_x;
        }
    case ARM64_INST_LDUMAXALB:
        return emu_ldumaxalb_w;
    case ARM64_INST_LDUMAXALH:
        return emu_ldumaxalh_w;
    case ARM64_INST_LDUMAXAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldumaxal_w;
        default:
            return emu_ldumaxal_x;
        }
    case ARM64_INST_LDUMINB:
        return emu_lduminb_w;
    case ARM64_INST_LDUMINH:
        return emu_lduminh_w;
    case ARM64_INST_LDUMIN:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldumin_w;
        default:
            return emu_ldumin_x;
        }
    case ARM64_INST_LDUMINLB:
        return emu_lduminlb_w;
    case ARM64_INST_LDUMINLH:
        return emu_lduminlh_w;
    case ARM64_INST_LDUMINL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_lduminl_w;
        default:
            return emu_lduminl_x;
        }
    case ARM64_INST_LDUMINAB:
        return emu_lduminab_w;
    case ARM64_INST_LDUMINAH:
        return emu_lduminah_w;
    case ARM64_INST_LDUMINA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldumina_w;
        default:
            return emu_ldumina_x;
        }
    case ARM64_INST_LDUMINALB:
        return emu_lduminalb_w;
    case ARM64_INST_LDUMINALH:
        return emu_lduminalh_w;
    case ARM64_INST_LDUMINAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_lduminal_w;
        default:
            return emu_lduminal_x;
        }
    case ARM64_INST_SWPB:
        return emu_swpb_w;
    case ARM64_INST_SWPH:
        return emu_swph_w;
    case ARM64_INST_SWP:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_swp_w;
        default:
            return emu_swp_x;
        }
    case ARM64_INST_SWPLB:
        return emu_swplb_w;
    case ARM64_INST_SWPLH:
        return emu_swplh_w;
    case ARM64_INST_SWPL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_swpl_w;
        default:
            return emu_swpl_x;
        }
    case ARM64_INST_SWPAB:
        return emu_swpab_w;
    case ARM64_INST_SWPAH:
        return emu_swpah_w;
    case ARM64_INST_SWPA:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_swpa_w;
        default:
            return emu_swpa_x;
        }
    case ARM64_INST_SWPALB:
        return emu_swpalb_w;
    case ARM64_INST_SWPALH:
        return emu_swpalh_w;
    case ARM64_INST_SWPAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_swpal_w;
        default:
            return emu_swpal_x;
        }
    case ARM64_INST_STLLRB:
        return emu_stllrb_w;
    case ARM64_INST_STLLRH:
        return emu_stllrh_w;
    case ARM64_INST_STLLR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stllr_w;
        default:
            return emu_stllr_x;
        }
    case ARM64_INST_STLRB:
        return emu_stlrb_w;
    case ARM64_INST_STLRH:
        return emu_stlrh_w;
    case ARM64_INST_STLR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stlr_w;
        default:
            return emu_stlr_x;
        }
    case ARM64_INST_LDLARB:
        return emu_ldlarb_w;
    case ARM64_INST_LDLARH:
        return emu_ldlarh_w;
    case ARM64_INST_LDLAR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldlar_w;
        default:
            return emu_ldlar_x;
        }
    case ARM64_INST_LDARB:
        return emu_ldarb_w;
    case ARM64_INST_LDARH:
        return emu_ldarh_w;
    case ARM64_INST_LDAR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldar_w;
        default:
            return emu_ldar_x;
        }
    case ARM64_INST_LDAPRB:
        return emu_ldaprb_w;
    case ARM64_INST_LDAPRH:
        return emu_ldaprh_w;
    case ARM64_INST_LDAPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldapr_w;
        default:
            return emu_ldapr_x;
        }
    case ARM64_INST_STLURB:
        return emu_stlurb_w;
    case ARM64_INST_STLURH:
        return emu_stlurh_w;
    case ARM64_INST_STLUR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stlur_w;
        default:
            return emu_stlur_x;
        }
    case ARM64_INST_LDAPURB:
        return emu_ldapurb_w;
    case ARM64_INST_LDAPURH:
        return emu_ldapurh_w;
    case ARM64_INST_LDAPUR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldapur_w;
        default:
            return emu_ldapur_x;
        }
    case ARM64_INST_LDAPURSB:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldapursb_w;
        default:
            return emu_ldapursb_x;
        }
    case ARM64_INST_LDAPURSH:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldapursh_w;
        default:
            return emu_ldapursh_x;
        }
    case ARM64_INST_LDAPURSW:
        return emu_ldapursw_x;
    case ARM64_INST_LDR_GPR_LITERAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldr_literal_gpr_w;
        default:
            return emu_ldr_literal_gpr_x;
        }
    case ARM64_INST_LDRSW_LITERAL:
        return emu_ldrsw_literal_x;
    case ARM64_INST_LDR_FP_SIMD_LITERAL:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldr_literal_fp_simd_s;
        case 64:
            return emu_ldr_literal_fp_simd_d;
        case 128:
            return emu_ldr_literal_fp_simd_q;
        default:
            return 0;
        }
    case ARM64_INST_LDNP_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldnp_gpr_w;
        default:
            return emu_ldnp_gpr_x;
        }
    case ARM64_INST_LDP_GPR_OFFSET:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldp_gpr_offset_w;
        default:
            return emu_ldp_gpr_offset_x;
        }
    case ARM64_INST_LDPSW_OFFSET:
        return emu_ldpsw_offset_x;
    case ARM64_INST_LDP_GPR_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldp_gpr_post_index_w;
        default:
            return emu_ldp_gpr_post_index_x;
        }
    case ARM64_INST_LDPSW_POST_INDEX:
        return emu_ldpsw_post_index_x;
    case ARM64_INST_LDP_GPR_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldp_gpr_pre_index_w;
        default:
            return emu_ldp_gpr_pre_index_x;
        }
    case ARM64_INST_LDPSW_PRE_INDEX:
        return emu_ldpsw_pre_index_x;
    case ARM64_INST_STNP_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stnp_gpr_w;
        default:
            return emu_stnp_gpr_x;
        }
    case ARM64_INST_STP_GPR_OFFSET:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stp_gpr_offset_w;
        default:
            return emu_stp_gpr_offset_x;
        }
    case ARM64_INST_STP_GPR_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stp_gpr_post_index_w;
        default:
            return emu_stp_gpr_post_index_x;
        }
    case ARM64_INST_STP_GPR_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stp_gpr_pre_index_w;
        default:
            return emu_stp_gpr_pre_index_x;
        }
    case ARM64_INST_LDNP_FP_SIMD:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldnp_fp_simd_s;
        case 64:
            return emu_ldnp_fp_simd_d;
        case 128:
            return emu_ldnp_fp_simd_q;
        default:
            return 0;
        }
    case ARM64_INST_LDP_FP_SIMD_OFFSET:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldp_fp_simd_offset_s;
        case 64:
            return emu_ldp_fp_simd_offset_d;
        case 128:
            return emu_ldp_fp_simd_offset_q;
        default:
            return 0;
        }
    case ARM64_INST_LDP_FP_SIMD_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldp_fp_simd_post_index_s;
        case 64:
            return emu_ldp_fp_simd_post_index_d;
        case 128:
            return emu_ldp_fp_simd_post_index_q;
        default:
            return 0;
        }
    case ARM64_INST_LDP_FP_SIMD_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_ldp_fp_simd_pre_index_s;
        case 64:
            return emu_ldp_fp_simd_pre_index_d;
        case 128:
            return emu_ldp_fp_simd_pre_index_q;
        default:
            return 0;
        }
    case ARM64_INST_STNP_FP_SIMD:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stnp_fp_simd_s;
        case 64:
            return emu_stnp_fp_simd_d;
        case 128:
            return emu_stnp_fp_simd_q;
        default:
            return 0;
        }
    case ARM64_INST_STP_FP_SIMD_OFFSET:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stp_fp_simd_offset_s;
        case 64:
            return emu_stp_fp_simd_offset_d;
        case 128:
            return emu_stp_fp_simd_offset_q;
        default:
            return 0;
        }
    case ARM64_INST_STP_FP_SIMD_POST_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stp_fp_simd_post_index_s;
        case 64:
            return emu_stp_fp_simd_post_index_d;
        case 128:
            return emu_stp_fp_simd_post_index_q;
        default:
            return 0;
        }
    case ARM64_INST_STP_FP_SIMD_PRE_INDEX:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_stp_fp_simd_pre_index_s;
        case 64:
            return emu_stp_fp_simd_pre_index_d;
        case 128:
            return emu_stp_fp_simd_pre_index_q;
        default:
            return 0;
        }
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
            default:
                return 0;
            }
        case 3:
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
            default:
                return 0;
            }
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
