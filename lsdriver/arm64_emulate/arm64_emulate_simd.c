/*
ARM64 指令执行器：Advanced SIMD 与浮点类。

emu_* 函数直接调用 SIMD/FP 硬件语义模板，完成向量排列、复制、提取、插入、移位、逻辑、算术、比较、饱和、窄化和归约。
同时覆盖标量与向量 FP16/FP32/FP64 运算、比较、选择、转换、乘加，以及点积、BF16 和其他扩展指令。
emu_build_simd_executor() 负责按指令、操作数宽度、元素宽度、条件码、立即数和旋转量选择具体执行函数。
*/
#include "emulate_inst.h"

static inline void emu_uzp1_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp1_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp1_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp1_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp1_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp1_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp1_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp1_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp1_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp1_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp1_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp1_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp1_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp1_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn1_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn1_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn1_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn1_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn1_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn1_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn1_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn1_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn1_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn1_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn1_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn1_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn1_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn1_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip1_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip1_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip1_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip1_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip1_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip1_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip1_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip1_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip1_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip1_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip1_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip1_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip1_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip1_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp2_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp2_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp2_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp2_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp2_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp2_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp2_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp2_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp2_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp2_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp2_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp2_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uzp2_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uzp2_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn2_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn2_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn2_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn2_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn2_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn2_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn2_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn2_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn2_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn2_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn2_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn2_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_trn2_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    trn2_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip2_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip2_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip2_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip2_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip2_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip2_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip2_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip2_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip2_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip2_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip2_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip2_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_zip2_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    zip2_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8b_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8b_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8b_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8b_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8b_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8b_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8b_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8b_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_4h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_4h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_4h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_4h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_8((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_9((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_10((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_11((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_12((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_13((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_14((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_16b_lane15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b_indexed_15((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_8h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_2d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2d_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_vector_2d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2d_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_general_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_general_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4h((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_general_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2s((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_general_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_general_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_general_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_general_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2d((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_8((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_9((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_10((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_11((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_12((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_13((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_14((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_b_lane15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_b_indexed_15((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_w_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_w_h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_8((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_9((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_10((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_11((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_12((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_13((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_14((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_b_lane15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_b_indexed_15((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smov_vector_to_gpr_x_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    smov_x_s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_8((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_9((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_10((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_11((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_12((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_13((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_14((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_b_lane15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_b_indexed_15((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_w_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_w_s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_x_d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_x_d_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umov_vector_to_gpr_x_d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    umov_x_d_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shadd_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shadd_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shadd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shadd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shadd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shadd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shadd_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shadd_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shadd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shadd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shadd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shadd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev64_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev64_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev64_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev64_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev64_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev64_2s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev64_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev64_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev64_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev64_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev64_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev64_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srhadd_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srhadd_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srhadd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srhadd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srhadd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srhadd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srhadd_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srhadd_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srhadd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srhadd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srhadd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srhadd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev16_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev16_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev16_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev16_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_and_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    and_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_and_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    and_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shsub_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shsub_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shsub_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shsub_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shsub_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shsub_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shsub_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shsub_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shsub_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shsub_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shsub_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    shsub_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmgt_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmgt_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmgt_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmgt_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmgt_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmgt_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmgt_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmgt_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmgt_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmgt_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmgt_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmgt_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmgt_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmgt_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmge_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmge_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmge_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmge_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmge_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmge_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmge_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmge_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmge_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmge_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmge_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmge_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmge_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmge_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sshl_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sshl_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sshl_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sshl_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sshl_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sshl_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sshl_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sshl_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sshl_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sshl_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sshl_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sshl_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sshl_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sshl_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srshl_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srshl_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srshl_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srshl_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srshl_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srshl_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srshl_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srshl_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srshl_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srshl_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srshl_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srshl_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srshl_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srshl_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smax_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smax_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smax_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smax_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smax_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smax_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smax_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smax_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smax_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smax_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smax_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smax_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smin_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smin_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smin_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smin_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smin_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smin_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smin_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smin_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smin_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smin_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smin_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smin_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sabd_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sabd_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sabd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sabd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sabd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sabd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sabd_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sabd_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sabd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sabd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sabd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sabd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saba_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saba_8b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saba_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saba_4h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saba_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saba_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saba_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saba_16b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saba_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saba_8h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saba_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saba_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_add_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    add_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_add_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    add_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_add_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    add_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_add_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    add_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_add_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    add_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_add_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    add_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_add_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    add_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmtst_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmtst_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmtst_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmtst_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmtst_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmtst_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmtst_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmtst_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmtst_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmtst_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmtst_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmtst_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmtst_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmtst_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mla_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mla_8b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mla_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mla_4h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mla_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mla_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mla_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mla_16b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mla_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mla_8h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mla_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mla_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mul_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mul_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mul_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mul_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mul_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mul_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mul_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mul_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mul_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mul_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mul_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mul_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxp_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxp_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxp_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxp_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminp_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminp_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminp_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminp_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addp_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addp_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addp_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addp_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addp_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addp_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnm_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnm_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnm_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnm_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnm_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnm_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnm_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnm_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnm_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnm_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4h_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_2s_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_8h_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4s_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_2d_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fadd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fadd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fadd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fadd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fadd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fadd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fadd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fadd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fadd_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fadd_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmax_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmax_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmax_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmax_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmax_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmax_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmax_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmax_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmax_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmax_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frecps_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frecps_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frecps_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frecps_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frecps_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frecps_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frecps_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frecps_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frecps_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frecps_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_xtn_vector_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    xtn_8b_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_xtn_vector_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    xtn_4h_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_xtn_vector_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    xtn_2s_2d(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtn_vector_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtn_8b_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtn_vector_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtn_4h_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtn_vector_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtn_2s_2d(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtns_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtns_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtns_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtns_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtns_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtns_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtms_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtms_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtms_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtms_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtms_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtms_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtas_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtas_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtas_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtas_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtas_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtas_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_scvtf_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    scvtf_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_scvtf_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    scvtf_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_scvtf_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    scvtf_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saddlv_vector_h_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saddlv_h_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saddlv_vector_s_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saddlv_s_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saddlv_vector_h_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saddlv_h_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saddlv_vector_s_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saddlv_s_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_saddlv_vector_d_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    saddlv_d_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxv_vector_b_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxv_b_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxv_vector_h_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxv_h_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxv_vector_b_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxv_b_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxv_vector_h_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxv_h_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smaxv_vector_s_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smaxv_s_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnmv_scalar_reduce_h_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnmv_h_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxv_scalar_reduce_h_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxv_h_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminv_vector_b_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminv_b_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminv_vector_h_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminv_h_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminv_vector_b_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminv_b_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminv_vector_h_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminv_h_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sminv_vector_s_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sminv_s_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addv_vector_b_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addv_b_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addv_vector_h_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addv_h_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addv_vector_b_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addv_b_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addv_vector_h_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addv_h_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_addv_vector_s_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    addv_s_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bic_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bic_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bic_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bic_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqdmulh_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqdmulh_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqdmulh_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqdmulh_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqdmulh_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqdmulh_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqdmulh_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqdmulh_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_orr_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    orr_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_orr_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    orr_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnm_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnm_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnm_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnm_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnm_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnm_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnm_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnm_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnm_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnm_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_zero_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_4h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_zero_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_2s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_zero_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_8h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_zero_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_4s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_zero_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_2d_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4h_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_2s_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_8h_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4s_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_2d_indexed_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsub_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsub_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsub_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsub_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsub_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsub_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsub_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsub_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsub_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsub_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_zero_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_4h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_zero_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_2s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_zero_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_8h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_zero_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_4s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_zero_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_2d_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famax_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famax_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famax_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famax_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famax_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famax_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famax_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famax_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famax_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famax_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmlt_zero_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmlt_4h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmlt_zero_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmlt_2s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmlt_zero_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmlt_8h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmlt_zero_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmlt_4s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmlt_zero_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmlt_2d_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmin_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmin_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmin_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmin_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmin_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmin_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmin_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmin_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmin_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmin_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabs_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabs_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabs_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabs_2s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintm_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintm_2s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabs_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabs_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabs_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabs_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabs_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabs_2d(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frsqrts_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frsqrts_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frsqrts_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frsqrts_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frsqrts_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frsqrts_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frsqrts_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frsqrts_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frsqrts_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frsqrts_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtps_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtps_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtps_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtps_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtps_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtps_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzs_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzs_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzs_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzs_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzs_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzs_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnmv_scalar_reduce_h_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnmv_h_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminv_scalar_reduce_h_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminv_h_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_orn_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    orn_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_orn_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    orn_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2s((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d_cmode2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2s((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate << 8, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d_cmode4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2s((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate << 16, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d_cmode6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2s((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate << 24, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d_cmode8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4h((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d_cmode10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4h((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate << 8, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d_cmode12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (entry->decoded.immediate << 8) | 0xff, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d_cmode13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (entry->decoded.immediate << 16) | 0xffff, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d_cmode14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8b((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_d_cmode14_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    movi_d_imm00((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_q(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_q_cmode2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate << 8, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_q_cmode4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate << 16, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_q_cmode6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate << 24, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_q_cmode8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_q_cmode10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_8h((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate << 8, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_q_cmode12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (entry->decoded.immediate << 8) | 0xff, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_q_cmode13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (entry->decoded.immediate << 16) | 0xffff, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_q_cmode14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_16b((uint64_t)&fp_regs->q[entry->decoded.rd], entry->decoded.immediate, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_movi_vector_immediate_2d_cmode14_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    movi_2d_imm00((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_4h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4h_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_4h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4h_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_4h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4h_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_4h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4h_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_8h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_8h_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_8h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_8h_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_8h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_8h_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_8h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_8h_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_8h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_8h_indexed_4_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_8h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_8h_indexed_5_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_8h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_8h_indexed_6_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_8h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_8h_indexed_7_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_2d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_2d_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_vector_by_element_2d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_2d_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_orr_vector_immediate_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{

    orr_2s_imm12((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_orr_vector_immediate_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{

    orr_4s_imm12((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_4h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4h_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_4h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4h_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_4h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4h_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_4h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4h_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_8h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_8h_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_8h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_8h_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_8h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_8h_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_8h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_8h_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_8h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_8h_indexed_4_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_8h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_8h_indexed_5_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_8h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_8h_indexed_6_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_8h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_8h_indexed_7_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_2d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_2d_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_vector_by_element_2d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_2d_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_4h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_4h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_4h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_4h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_2s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_2s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_8h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_8h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_8h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_8h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_8h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_8h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_8h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_8h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_8h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_8h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_8h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_8h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_8h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_8h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_8h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_8h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_2d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_2d_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_by_element_2d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_2d_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sudot_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sudot_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sudot_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sudot_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sudot_vector_by_element_2s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sudot_2s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sudot_vector_by_element_2s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sudot_2s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sudot_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sudot_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sudot_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sudot_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sudot_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sudot_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sudot_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sudot_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_vector_immediate_2s_imm70(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_2s_imm70((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_vector_immediate_2s_immf0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_2s_immf0((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_shl_vector_immediate_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{

    shl_4s_imm2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfdot_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfdot_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfdot_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfdot_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfdot_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfdot_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfdot_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfdot_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_by_element_2s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_2s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_by_element_2s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_2s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_by_element_2s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_2s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_by_element_2s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_2s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_by_element_2s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_2s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_by_element_2s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_2s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sdot_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sdot_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_by_element_2s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_2s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_by_element_2s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_2s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usdot_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usdot_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalb_vector_by_element_8h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalb_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalb_vector_by_element_8h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalb_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalb_vector_by_element_8h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalb_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalb_vector_by_element_8h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalb_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtns_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtns_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtns_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtns_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtns_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtns_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtns_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtns_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fccmp_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    if (!cond_holds(read_nzcv(regs), entry->decoded.condition))
    {
        write_nzcv(regs, (uint64_t)entry->decoded.immediate << 28);
        regs->pc += 4;
        return;
    }
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmp_h((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fccmp_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    if (!cond_holds(read_nzcv(regs), entry->decoded.condition))
    {
        write_nzcv(regs, (uint64_t)entry->decoded.immediate << 28);
        regs->pc += 4;
        return;
    }
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmp_s((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fccmp_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    if (!cond_holds(read_nzcv(regs), entry->decoded.condition))
    {
        write_nzcv(regs, (uint64_t)entry->decoded.immediate << 28);
        regs->pc += 4;
        return;
    }
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmp_d((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fccmpe_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    if (!cond_holds(read_nzcv(regs), entry->decoded.condition))
    {
        write_nzcv(regs, (uint64_t)entry->decoded.immediate << 28);
        regs->pc += 4;
        return;
    }
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmpe_h((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fccmpe_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    if (!cond_holds(read_nzcv(regs), entry->decoded.condition))
    {
        write_nzcv(regs, (uint64_t)entry->decoded.immediate << 28);
        regs->pc += 4;
        return;
    }
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmpe_s((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fccmpe_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    if (!cond_holds(read_nzcv(regs), entry->decoded.condition))
    {
        write_nzcv(regs, (uint64_t)entry->decoded.immediate << 28);
        regs->pc += 4;
        return;
    }
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmpe_d((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcsel_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcsel_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], cond_holds(read_nzcv(regs), entry->decoded.condition), 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcsel_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcsel_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], cond_holds(read_nzcv(regs), entry->decoded.condition), 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcsel_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcsel_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], cond_holds(read_nzcv(regs), entry->decoded.condition), 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_scalar_immediate_s_imm60(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_s_imm60((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_scalar_immediate_s_imm70(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_s_imm70((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_scalar_immediate_s_imme0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_s_imme0((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_scalar_immediate_s_immf0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_s_immf0((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_scalar_immediate_d_imm00(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_d_imm00((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_scalar_immediate_d_imm78(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_d_imm78((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fdiv_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fdiv_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fdiv_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fdiv_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fdiv_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fdiv_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmp_register_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmp_h((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmp_register_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmp_s((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmp_register_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmp_d((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmp_zero_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmp_h_zero((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmp_zero_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmp_s_zero((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmp_zero_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmp_d_zero((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmpe_register_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmpe_h((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmpe_register_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmpe_s((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmpe_register_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmpe_d((uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmpe_zero_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmpe_h_zero((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmpe_zero_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmpe_s_zero((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmpe_zero_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcmpe_d_zero((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_nzcv(regs, template_output0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fadd_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fadd_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fadd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fadd_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fadd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fadd_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsub_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsub_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsub_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsub_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsub_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsub_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmax_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmax_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmax_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmax_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmax_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmax_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmin_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmin_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmin_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmin_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmin_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmin_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnm_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnm_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnm_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnm_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnm_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnm_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnm_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnm_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnm_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnm_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnm_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnm_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fnmul_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fnmul_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fnmul_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fnmul_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fnmul_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fnmul_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabs_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabs_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabs_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabs_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabs_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabs_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtnu_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtnu_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtnu_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtnu_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtnu_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtnu_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtnu_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtnu_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fneg_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fneg_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fneg_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fneg_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fneg_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fneg_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsqrt_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsqrt_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsqrt_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsqrt_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsqrt_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsqrt_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_scvtf_s_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    scvtf_s_w_merge((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvt_d_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvt_d_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ucvtf_s_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ucvtf_s_w_merge((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtas_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtas_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtas_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtas_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtas_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtas_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtas_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtas_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintn_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintn_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintn_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintn_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintn_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintn_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintp_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintp_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintp_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintp_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintp_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintp_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtau_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtau_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtau_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtau_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtau_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtau_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtau_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtau_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintm_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintm_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintm_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintm_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintm_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintm_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintz_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintz_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintz_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintz_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintz_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintz_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_fp_to_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fmov_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_fp_to_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fmov_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frinta_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frinta_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frinta_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frinta_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frinta_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frinta_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_gpr_to_fp_s_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_s_w((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmov_gpr_to_fp_d_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmov_d_x((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintx_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintx_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintx_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintx_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frintx_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frintx_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frinti_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frinti_h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frinti_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frinti_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frinti_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frinti_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtps_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtps_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtps_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtps_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtps_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtps_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtps_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtps_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtpu_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtpu_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtpu_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtpu_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtpu_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtpu_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtpu_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtpu_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtms_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtms_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtms_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtms_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtms_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtms_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtms_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtms_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtmu_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtmu_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtmu_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtmu_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtmu_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtmu_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtmu_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtmu_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzs_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtzs_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzs_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtzs_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzs_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtzs_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzs_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtzs_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzu_gpr_w_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtzu_w_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzu_gpr_w_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtzu_w_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, (uint32_t)template_output0, false);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzu_gpr_x_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtzu_x_s((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzu_gpr_x_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t template_output0 __attribute__((__uninitialized__));
    fcvtzu_x_d((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &template_output0, 0, 0, 0, 0);
    write_gpr_or_zr(regs, entry->decoded.rd, template_output0, true);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_scvtf_d_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    scvtf_d_w_merge((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvt_s_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvt_s_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ucvtf_d_w(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ucvtf_d_w_merge((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmadd_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmadd_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmadd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmadd_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmadd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmadd_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmsub_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmsub_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmsub_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmsub_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmsub_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmsub_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fnmadd_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fnmadd_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fnmadd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fnmadd_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fnmadd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fnmadd_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fnmsub_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fnmsub_h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fnmsub_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fnmsub_s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fnmsub_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fnmsub_d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], (uint64_t)&fp_regs->q[entry->decoded.ra], 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_8b_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_8b_offset_0(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_8b_imm1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_8b_offset_1(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_8b_imm2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_8b_offset_2(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_8b_imm3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_8b_offset_3(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_8b_imm4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_8b_offset_4(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_8b_imm5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_8b_offset_5(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_8b_imm6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_8b_offset_6(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_8b_imm7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_8b_offset_7(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_0(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_1(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_2(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_3(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_4(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_5(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_6(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_7(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_8(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_9(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_10(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_11(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_12(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_13(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_14(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ext_vector_16b_imm15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ext_16b_offset_15(0, (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhadd_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhadd_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhadd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhadd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhadd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhadd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhadd_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhadd_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhadd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhadd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhadd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhadd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev32_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev32_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev32_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev32_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev32_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev32_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_rev32_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    rev32_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urhadd_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urhadd_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urhadd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urhadd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urhadd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urhadd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urhadd_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urhadd_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urhadd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urhadd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urhadd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urhadd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_eor_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    eor_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_eor_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    eor_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhsub_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhsub_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhsub_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhsub_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhsub_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhsub_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhsub_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhsub_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhsub_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhsub_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uhsub_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uhsub_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhi_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhi_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhi_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhi_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhi_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhi_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhi_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhi_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhi_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhi_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhi_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhi_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhi_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhi_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhs_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhs_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhs_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhs_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhs_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhs_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhs_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhs_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhs_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhs_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhs_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhs_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhs_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhs_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ushl_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ushl_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ushl_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ushl_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ushl_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ushl_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ushl_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ushl_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ushl_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ushl_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ushl_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ushl_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ushl_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ushl_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urshl_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urshl_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urshl_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urshl_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urshl_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urshl_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urshl_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urshl_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urshl_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urshl_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urshl_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urshl_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urshl_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urshl_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umax_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umax_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umax_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umax_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umax_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umax_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umax_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umax_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umax_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umax_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umax_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umax_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umin_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umin_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umin_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umin_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umin_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umin_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umin_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umin_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umin_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umin_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umin_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umin_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uabd_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uabd_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uabd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uabd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uabd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uabd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uabd_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uabd_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uabd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uabd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uabd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uabd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaba_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaba_8b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaba_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaba_4h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaba_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaba_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaba_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaba_16b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaba_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaba_8h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaba_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaba_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sub_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sub_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sub_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sub_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sub_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sub_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sub_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sub_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sub_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sub_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sub_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sub_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sub_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sub_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmeq_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmeq_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmeq_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmeq_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmeq_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmeq_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmeq_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmeq_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmeq_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmeq_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmeq_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmeq_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmeq_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmeq_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mls_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mls_8b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mls_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mls_4h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mls_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mls_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mls_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mls_16b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mls_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mls_8h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mls_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    mls_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_pmul_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    pmul_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_pmul_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    pmul_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxp_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxp_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxp_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxp_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminp_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminp_8b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminp_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminp_16b((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnmp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnmp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnmp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnmp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnmp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnmp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnmp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnmp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxnmp_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxnmp_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_faddp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    faddp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_faddp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    faddp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_faddp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    faddp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_faddp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    faddp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_faddp_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    faddp_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facge_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facge_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facge_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facge_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facge_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facge_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facge_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facge_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facge_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facge_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmaxp_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmaxp_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fdiv_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fdiv_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fdiv_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fdiv_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fdiv_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fdiv_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fdiv_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fdiv_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fdiv_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fdiv_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtun_vector_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtun_8b_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtun_vector_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtun_4h_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtun_vector_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtun_2s_2d(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqxtn_vector_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqxtn_8b_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqxtn_vector_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqxtn_4h_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqxtn_vector_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqxtn_2s_2d(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtnu_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtnu_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtnu_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtnu_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtnu_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtnu_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtmu_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtmu_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtmu_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtmu_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtmu_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtmu_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtau_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtau_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtau_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtau_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtau_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtau_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ucvtf_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ucvtf_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ucvtf_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ucvtf_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ucvtf_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ucvtf_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaddlv_vector_h_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaddlv_h_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaddlv_vector_s_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaddlv_s_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaddlv_vector_h_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaddlv_h_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaddlv_vector_s_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaddlv_s_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uaddlv_vector_d_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uaddlv_d_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxv_vector_b_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxv_b_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxv_vector_h_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxv_h_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxv_vector_b_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxv_b_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxv_vector_h_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxv_h_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_umaxv_vector_s_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    umaxv_s_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminv_vector_b_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminv_b_8b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminv_vector_h_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminv_h_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminv_vector_b_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminv_b_16b(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminv_vector_h_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminv_h_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uminv_vector_s_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uminv_s_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_8h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_8h_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_4h_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_rotation_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_4h_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_rotation_90_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_4h_imm180(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_rotation_180_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_4h_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_rotation_270_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_2s_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_2s_rotation_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_2s_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_2s_rotation_90_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_2s_imm180(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_2s_rotation_180_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_2s_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_2s_rotation_270_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_8h_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_rotation_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_8h_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_rotation_90_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_8h_imm180(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_rotation_180_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_8h_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_rotation_270_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_4s_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_rotation_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_4s_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_rotation_90_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_4s_imm180(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_rotation_180_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_4s_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_rotation_270_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_2d_imm0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_2d_rotation_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_2d_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_2d_rotation_90_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_2d_imm180(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_2d_rotation_180_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_2d_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_2d_rotation_270_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_4h_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_4h_rotation_90((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_4h_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_4h_rotation_270((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_2s_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_2s_rotation_90((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_2s_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_2s_rotation_270((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_8h_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_8h_rotation_90((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_8h_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_8h_rotation_270((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_4s_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_4s_rotation_90((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_4s_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_4s_rotation_270((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_2d_imm90(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_2d_rotation_90((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcadd_vector_2d_imm270(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcadd_2d_rotation_270((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfdot_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfdot_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfdot_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfdot_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bsl_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bsl_8b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bsl_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bsl_16b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmulh_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmulh_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmulh_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmulh_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmulh_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmulh_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmulh_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmulh_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bit_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bit_8b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bit_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bit_16b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnmp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnmp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnmp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnmp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnmp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnmp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnmp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnmp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminnmp_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminnmp_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_zero_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_4h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_zero_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_2s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_zero_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_8h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_zero_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_4s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_zero_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_2d_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_2s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabd_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabd_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabd_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabd_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabd_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabd_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabd_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabd_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabd_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabd_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmle_zero_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmle_4h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmle_zero_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmle_2s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmle_zero_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmle_8h_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmle_zero_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmle_4s_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmle_zero_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmle_2d_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famin_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famin_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famin_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famin_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famin_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famin_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famin_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famin_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_famin_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    famin_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facgt_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facgt_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facgt_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facgt_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facgt_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facgt_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facgt_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facgt_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facgt_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facgt_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminp_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminp_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminp_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminp_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminp_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminp_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminp_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminp_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fminp_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fminp_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fneg_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fneg_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fneg_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fneg_2s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fneg_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fneg_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fneg_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fneg_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fneg_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fneg_2d(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fscale_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fscale_4h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fscale_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fscale_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fscale_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fscale_8h((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fscale_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fscale_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fscale_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fscale_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtpu_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtpu_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtpu_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtpu_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtpu_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtpu_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzu_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzu_2s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzu_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzu_4s((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzu_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzu_2d((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsqrt_vector_4h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsqrt_4h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsqrt_vector_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsqrt_2s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsqrt_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsqrt_8h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsqrt_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsqrt_4s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fsqrt_vector_2d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fsqrt_2d(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalb_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalb_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bif_vector_8b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bif_8b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bif_vector_16b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bif_16b_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mvni_vector_immediate_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{

    mvni_2s_imm12((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_mvni_vector_immediate_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{

    mvni_4s_imm12((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bic_vector_immediate_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{

    bic_2s_imm12((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bic_vector_immediate_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{

    bic_4s_imm12((uint64_t)&fp_regs->q[entry->decoded.rd], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_4h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_4h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_4h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_4h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_2s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_2s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_8h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_8h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_8h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_8h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_8h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_8h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_8h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_8h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_8h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_8h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_8h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_8h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_8h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_8h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_8h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_8h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_4s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_2d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_2d_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_vector_by_element_2d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_2d_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4h_imm0_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_indexed_rotation_0_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4h_imm0_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_indexed_rotation_0_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4h_imm90_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_indexed_rotation_90_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4h_imm90_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_indexed_rotation_90_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4h_imm180_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_indexed_rotation_180_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4h_imm180_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_indexed_rotation_180_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4h_imm270_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_indexed_rotation_270_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4h_imm270_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4h_indexed_rotation_270_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm0_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_0_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm0_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_0_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm0_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_0_lane2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm0_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_0_lane3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm90_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_90_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm90_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_90_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm90_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_90_lane2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm90_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_90_lane3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm180_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_180_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm180_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_180_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm180_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_180_lane2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm180_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_180_lane3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm270_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_270_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm270_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_270_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm270_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_270_lane2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_8h_imm270_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_8h_indexed_rotation_270_lane3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4s_imm0_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_indexed_rotation_0_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4s_imm0_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_indexed_rotation_0_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4s_imm90_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_indexed_rotation_90_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4s_imm90_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_indexed_rotation_90_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4s_imm180_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_indexed_rotation_180_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4s_imm180_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_indexed_rotation_180_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4s_imm270_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_indexed_rotation_270_lane0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmla_vector_by_element_4s_imm270_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmla_4s_indexed_rotation_270_lane1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_4h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_4h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_4h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_4h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_2s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_2s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_8h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_8h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_8h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_8h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_8h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_8h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_8h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_8h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_8h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_8h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_8h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_8h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_8h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_8h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_8h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_8h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_4s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_4h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_4h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_4h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_4h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_2s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_2s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_8h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_8h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_8h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_8h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_8h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_8h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_8h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_8h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_8h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_8h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_8h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_8h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_8h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_8h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_8h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_8h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_4s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_by_element_2s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_2s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_by_element_2s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_2s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlal2_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlal2_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_by_element_2s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_2s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_by_element_2s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_2s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmlsl2_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmlsl2_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_by_element_2s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_2s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_by_element_2s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_2s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_by_element_2s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_2s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_by_element_2s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_2s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_by_element_4s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_4s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_by_element_4s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_4s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_by_element_4s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_4s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_udot_vector_by_element_4s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    udot_4s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_8((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_9((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_10((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_11((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_12((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_13((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_14((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_b_lane15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_indexed_15((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_d_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_gpr_vector_d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_d_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_xtn2_vector_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    xtn2_16b_8h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_xtn2_vector_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    xtn2_8h_4s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_xtn2_vector_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    xtn2_4s_2d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtn2_vector_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtn2_16b_8h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtn2_vector_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtn2_8h_4s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtn2_vector_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtn2_4s_2d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_smmla_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    smmla_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_usmmla_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    usmmla_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalt_vector_by_element_8h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalt_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalt_vector_by_element_8h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalt_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalt_vector_by_element_8h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalt_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalt_vector_by_element_8h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalt_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_8((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_9((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_10((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_11((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_12((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_13((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_14((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_b_lane15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_b_indexed_15((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_d_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_dup_element_scalar_d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    dup_d_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_scalar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_b_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqadd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqadd_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_scalar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_b_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqsub_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqsub_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_scalar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_b_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqshl_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqshl_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_scalar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_b_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrshl_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrshl_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frecps_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frecps_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frecps_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frecps_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frecps_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frecps_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtn_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtn_b_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtn_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtn_h_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtn_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtn_s_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtns_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtns_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtns_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtns_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtms_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtms_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtms_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtms_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtas_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtas_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtas_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtas_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_scvtf_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    scvtf_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_scvtf_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    scvtf_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_faddp_scalar_reduce_h_2h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    faddp_h_2h(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_faddp_scalar_reduce_s_2s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    faddp_s_2s(0, (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqdmulh_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqdmulh_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqdmulh_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqdmulh_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_zero_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_h_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_zero_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_s_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_zero_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_d_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_zero_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_h_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_zero_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_s_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmeq_zero_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmeq_d_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmlt_zero_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmlt_h_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmlt_zero_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmlt_s_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmlt_zero_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmlt_d_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frsqrts_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frsqrts_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frsqrts_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frsqrts_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_frsqrts_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    frsqrts_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtps_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtps_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtps_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtps_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzs_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzs_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzs_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzs_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmgt_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmgt_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmge_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmge_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sshl_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sshl_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_srshl_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    srshl_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_add_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    add_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmtst_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmtst_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_h_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_h_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_h_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_h_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_h_indexed_4_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_h_indexed_5_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_h_indexed_6_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_h_indexed_7_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_d_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmla_scalar_by_element_d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmla_d_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_h_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_h_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_h_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_h_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_h_indexed_4_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_h_indexed_5_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_h_indexed_6_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_h_indexed_7_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_d_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmls_scalar_by_element_d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmls_d_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_d_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmul_scalar_by_element_d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmul_d_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_element_vector_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_b_vector((uint64_t)&fp_regs->q[entry->decoded.rn], entry->decoded.immediate, entry->decoded.lane_index, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_element_vector_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_h_vector((uint64_t)&fp_regs->q[entry->decoded.rn], entry->decoded.immediate, entry->decoded.lane_index, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_element_vector_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_s_vector((uint64_t)&fp_regs->q[entry->decoded.rn], entry->decoded.immediate, entry->decoded.lane_index, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ins_element_vector_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ins_d_vector((uint64_t)&fp_regs->q[entry->decoded.rn], entry->decoded.immediate, entry->decoded.lane_index, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtun2_vector_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtun2_16b_8h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtun2_vector_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtun2_8h_4s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtun2_vector_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtun2_4s_2d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqxtn2_vector_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqxtn2_16b_8h_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqxtn2_vector_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqxtn2_8h_4s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqxtn2_vector_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqxtn2_4s_2d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmmla_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmmla_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ummla_vector_4s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ummla_4s_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_bfmlalt_vector_8h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bfmlalt_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_scalar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_b_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqadd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqadd_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_scalar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_b_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqsub_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqsub_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_scalar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_b_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqshl_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqshl_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_scalar_b(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_b_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqrshl_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqrshl_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facge_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facge_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facge_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facge_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facge_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facge_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtun_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtun_b_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtun_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtun_h_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqxtun_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqxtun_s_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqxtn_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqxtn_b_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqxtn_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqxtn_h_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_uqxtn_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uqxtn_s_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtnu_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtnu_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtmu_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtmu_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtnu_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtnu_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtmu_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtmu_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtau_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtau_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtau_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtau_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ucvtf_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ucvtf_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ucvtf_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ucvtf_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}

static inline void emu_sqrdmlah_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_h_scalar_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}

static inline void emu_sqrdmlah_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_s_scalar_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}

static inline void emu_sqrdmlsh_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_h_scalar_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}

static inline void emu_sqrdmlsh_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_s_scalar_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}

static inline void emu_sqrdmulh_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmulh_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmulh_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmulh_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_zero_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_h_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_zero_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_s_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmge_zero_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmge_d_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabd_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabd_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabd_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fabd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fabd_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmle_zero_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmle_h_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmle_zero_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmle_s_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmle_zero_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmle_d_scalar_zero((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcmgt_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcmgt_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facgt_scalar_h(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facgt_h_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facgt_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facgt_s_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_facgt_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    facgt_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtpu_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtpu_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtpu_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtpu_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzu_simd_scalar_s(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzu_s_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fcvtzu_simd_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fcvtzu_d_merge((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhi_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhi_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmhs_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmhs_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ushl_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ushl_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_urshl_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    urshl_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sub_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sub_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_cmeq_scalar_d(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    cmeq_d_scalar((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_h_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_h_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_h_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_h_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_h_indexed_4((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_h_indexed_5((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_h_indexed_6((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_h_indexed_7((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_s_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_s_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_s_indexed_2((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_s_indexed_3((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_d_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_d_indexed_0((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_fmulx_scalar_by_element_d_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    fmulx_d_indexed_1((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}

static inline void emu_sqrdmlah_scalar_by_element_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_h_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_h_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_h_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_h_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_h_indexed_4_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_h_indexed_5_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_h_indexed_6_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_h_indexed_7_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlah_scalar_by_element_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlah_s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_h_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_h_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_h_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_h_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_h_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_h_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_h_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_h_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_h_lane4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_h_indexed_4_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_h_lane5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_h_indexed_5_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_h_lane6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_h_indexed_6_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_h_lane7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_h_indexed_7_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_s_lane0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_s_indexed_0_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_s_lane1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_s_indexed_1_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_s_lane2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_s_indexed_2_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_sqrdmlsh_scalar_by_element_s_lane3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    sqrdmlsh_s_indexed_3_accumulate((uint64_t)&fp_regs->q[entry->decoded.rd], (uint64_t)&fp_regs->q[entry->decoded.rn], (uint64_t)&fp_regs->q[entry->decoded.rm], 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}

static inline void emu_scvtf_s_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    scvtf_s_x_merge((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ucvtf_s_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ucvtf_s_x_merge((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_scvtf_d_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    scvtf_d_x_merge((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
static inline void emu_ucvtf_d_x(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    ucvtf_d_x_merge((uint64_t)&fp_regs->q[entry->decoded.rd], read_gpr_or_zr(regs, entry->decoded.rn), 0, 0, 0, &fp_regs->q[entry->decoded.rd], 0, 0, 0, 0);
    regs->pc += 4;
    read_fpsr(&fp_regs->fpsr);
    return;
}
void (*emu_build_simd_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_UZP1_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uzp1_vector_8b;
            case 16:
                return emu_uzp1_vector_4h;
            case 32:
                return emu_uzp1_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uzp1_vector_16b;
            case 16:
                return emu_uzp1_vector_8h;
            case 32:
                return emu_uzp1_vector_4s;
            case 64:
                return emu_uzp1_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_TRN1_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_trn1_vector_8b;
            case 16:
                return emu_trn1_vector_4h;
            case 32:
                return emu_trn1_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_trn1_vector_16b;
            case 16:
                return emu_trn1_vector_8h;
            case 32:
                return emu_trn1_vector_4s;
            case 64:
                return emu_trn1_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ZIP1_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_zip1_vector_8b;
            case 16:
                return emu_zip1_vector_4h;
            case 32:
                return emu_zip1_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_zip1_vector_16b;
            case 16:
                return emu_zip1_vector_8h;
            case 32:
                return emu_zip1_vector_4s;
            case 64:
                return emu_zip1_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UZP2_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uzp2_vector_8b;
            case 16:
                return emu_uzp2_vector_4h;
            case 32:
                return emu_uzp2_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uzp2_vector_16b;
            case 16:
                return emu_uzp2_vector_8h;
            case 32:
                return emu_uzp2_vector_4s;
            case 64:
                return emu_uzp2_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_TRN2_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_trn2_vector_8b;
            case 16:
                return emu_trn2_vector_4h;
            case 32:
                return emu_trn2_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_trn2_vector_16b;
            case 16:
                return emu_trn2_vector_8h;
            case 32:
                return emu_trn2_vector_4s;
            case 64:
                return emu_trn2_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ZIP2_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_zip2_vector_8b;
            case 16:
                return emu_zip2_vector_4h;
            case 32:
                return emu_zip2_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_zip2_vector_16b;
            case 16:
                return emu_zip2_vector_8h;
            case 32:
                return emu_zip2_vector_4s;
            case 64:
                return emu_zip2_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_DUP_ELEMENT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_vector_8b_lane0;
                case 1:
                    return emu_dup_element_vector_8b_lane1;
                case 2:
                    return emu_dup_element_vector_8b_lane2;
                case 3:
                    return emu_dup_element_vector_8b_lane3;
                case 4:
                    return emu_dup_element_vector_8b_lane4;
                case 5:
                    return emu_dup_element_vector_8b_lane5;
                case 6:
                    return emu_dup_element_vector_8b_lane6;
                case 7:
                    return emu_dup_element_vector_8b_lane7;
                default:
                    return NULL;
                }
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_vector_4h_lane0;
                case 1:
                    return emu_dup_element_vector_4h_lane1;
                case 2:
                    return emu_dup_element_vector_4h_lane2;
                case 3:
                    return emu_dup_element_vector_4h_lane3;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_vector_2s_lane0;
                case 1:
                    return emu_dup_element_vector_2s_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_vector_16b_lane0;
                case 1:
                    return emu_dup_element_vector_16b_lane1;
                case 2:
                    return emu_dup_element_vector_16b_lane2;
                case 3:
                    return emu_dup_element_vector_16b_lane3;
                case 4:
                    return emu_dup_element_vector_16b_lane4;
                case 5:
                    return emu_dup_element_vector_16b_lane5;
                case 6:
                    return emu_dup_element_vector_16b_lane6;
                case 7:
                    return emu_dup_element_vector_16b_lane7;
                case 8:
                    return emu_dup_element_vector_16b_lane8;
                case 9:
                    return emu_dup_element_vector_16b_lane9;
                case 10:
                    return emu_dup_element_vector_16b_lane10;
                case 11:
                    return emu_dup_element_vector_16b_lane11;
                case 12:
                    return emu_dup_element_vector_16b_lane12;
                case 13:
                    return emu_dup_element_vector_16b_lane13;
                case 14:
                    return emu_dup_element_vector_16b_lane14;
                case 15:
                    return emu_dup_element_vector_16b_lane15;
                default:
                    return NULL;
                }
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_vector_8h_lane0;
                case 1:
                    return emu_dup_element_vector_8h_lane1;
                case 2:
                    return emu_dup_element_vector_8h_lane2;
                case 3:
                    return emu_dup_element_vector_8h_lane3;
                case 4:
                    return emu_dup_element_vector_8h_lane4;
                case 5:
                    return emu_dup_element_vector_8h_lane5;
                case 6:
                    return emu_dup_element_vector_8h_lane6;
                case 7:
                    return emu_dup_element_vector_8h_lane7;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_vector_4s_lane0;
                case 1:
                    return emu_dup_element_vector_4s_lane1;
                case 2:
                    return emu_dup_element_vector_4s_lane2;
                case 3:
                    return emu_dup_element_vector_4s_lane3;
                default:
                    return NULL;
                }
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_vector_2d_lane0;
                case 1:
                    return emu_dup_element_vector_2d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_DUP_GENERAL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_dup_general_vector_8b;
            case 16:
                return emu_dup_general_vector_4h;
            case 32:
                return emu_dup_general_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_dup_general_vector_16b;
            case 16:
                return emu_dup_general_vector_8h;
            case 32:
                return emu_dup_general_vector_4s;
            case 64:
                return emu_dup_general_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SMOV_VECTOR_TO_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_smov_vector_to_gpr_w_b_lane0;
                case 1:
                    return emu_smov_vector_to_gpr_w_b_lane1;
                case 2:
                    return emu_smov_vector_to_gpr_w_b_lane2;
                case 3:
                    return emu_smov_vector_to_gpr_w_b_lane3;
                case 4:
                    return emu_smov_vector_to_gpr_w_b_lane4;
                case 5:
                    return emu_smov_vector_to_gpr_w_b_lane5;
                case 6:
                    return emu_smov_vector_to_gpr_w_b_lane6;
                case 7:
                    return emu_smov_vector_to_gpr_w_b_lane7;
                case 8:
                    return emu_smov_vector_to_gpr_w_b_lane8;
                case 9:
                    return emu_smov_vector_to_gpr_w_b_lane9;
                case 10:
                    return emu_smov_vector_to_gpr_w_b_lane10;
                case 11:
                    return emu_smov_vector_to_gpr_w_b_lane11;
                case 12:
                    return emu_smov_vector_to_gpr_w_b_lane12;
                case 13:
                    return emu_smov_vector_to_gpr_w_b_lane13;
                case 14:
                    return emu_smov_vector_to_gpr_w_b_lane14;
                case 15:
                    return emu_smov_vector_to_gpr_w_b_lane15;
                default:
                    return NULL;
                }
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_smov_vector_to_gpr_w_h_lane0;
                case 1:
                    return emu_smov_vector_to_gpr_w_h_lane1;
                case 2:
                    return emu_smov_vector_to_gpr_w_h_lane2;
                case 3:
                    return emu_smov_vector_to_gpr_w_h_lane3;
                case 4:
                    return emu_smov_vector_to_gpr_w_h_lane4;
                case 5:
                    return emu_smov_vector_to_gpr_w_h_lane5;
                case 6:
                    return emu_smov_vector_to_gpr_w_h_lane6;
                case 7:
                    return emu_smov_vector_to_gpr_w_h_lane7;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_smov_vector_to_gpr_x_b_lane0;
                case 1:
                    return emu_smov_vector_to_gpr_x_b_lane1;
                case 2:
                    return emu_smov_vector_to_gpr_x_b_lane2;
                case 3:
                    return emu_smov_vector_to_gpr_x_b_lane3;
                case 4:
                    return emu_smov_vector_to_gpr_x_b_lane4;
                case 5:
                    return emu_smov_vector_to_gpr_x_b_lane5;
                case 6:
                    return emu_smov_vector_to_gpr_x_b_lane6;
                case 7:
                    return emu_smov_vector_to_gpr_x_b_lane7;
                case 8:
                    return emu_smov_vector_to_gpr_x_b_lane8;
                case 9:
                    return emu_smov_vector_to_gpr_x_b_lane9;
                case 10:
                    return emu_smov_vector_to_gpr_x_b_lane10;
                case 11:
                    return emu_smov_vector_to_gpr_x_b_lane11;
                case 12:
                    return emu_smov_vector_to_gpr_x_b_lane12;
                case 13:
                    return emu_smov_vector_to_gpr_x_b_lane13;
                case 14:
                    return emu_smov_vector_to_gpr_x_b_lane14;
                case 15:
                    return emu_smov_vector_to_gpr_x_b_lane15;
                default:
                    return NULL;
                }
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_smov_vector_to_gpr_x_h_lane0;
                case 1:
                    return emu_smov_vector_to_gpr_x_h_lane1;
                case 2:
                    return emu_smov_vector_to_gpr_x_h_lane2;
                case 3:
                    return emu_smov_vector_to_gpr_x_h_lane3;
                case 4:
                    return emu_smov_vector_to_gpr_x_h_lane4;
                case 5:
                    return emu_smov_vector_to_gpr_x_h_lane5;
                case 6:
                    return emu_smov_vector_to_gpr_x_h_lane6;
                case 7:
                    return emu_smov_vector_to_gpr_x_h_lane7;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_smov_vector_to_gpr_x_s_lane0;
                case 1:
                    return emu_smov_vector_to_gpr_x_s_lane1;
                case 2:
                    return emu_smov_vector_to_gpr_x_s_lane2;
                case 3:
                    return emu_smov_vector_to_gpr_x_s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UMOV_VECTOR_TO_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_umov_vector_to_gpr_w_b_lane0;
                case 1:
                    return emu_umov_vector_to_gpr_w_b_lane1;
                case 2:
                    return emu_umov_vector_to_gpr_w_b_lane2;
                case 3:
                    return emu_umov_vector_to_gpr_w_b_lane3;
                case 4:
                    return emu_umov_vector_to_gpr_w_b_lane4;
                case 5:
                    return emu_umov_vector_to_gpr_w_b_lane5;
                case 6:
                    return emu_umov_vector_to_gpr_w_b_lane6;
                case 7:
                    return emu_umov_vector_to_gpr_w_b_lane7;
                case 8:
                    return emu_umov_vector_to_gpr_w_b_lane8;
                case 9:
                    return emu_umov_vector_to_gpr_w_b_lane9;
                case 10:
                    return emu_umov_vector_to_gpr_w_b_lane10;
                case 11:
                    return emu_umov_vector_to_gpr_w_b_lane11;
                case 12:
                    return emu_umov_vector_to_gpr_w_b_lane12;
                case 13:
                    return emu_umov_vector_to_gpr_w_b_lane13;
                case 14:
                    return emu_umov_vector_to_gpr_w_b_lane14;
                case 15:
                    return emu_umov_vector_to_gpr_w_b_lane15;
                default:
                    return NULL;
                }
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_umov_vector_to_gpr_w_h_lane0;
                case 1:
                    return emu_umov_vector_to_gpr_w_h_lane1;
                case 2:
                    return emu_umov_vector_to_gpr_w_h_lane2;
                case 3:
                    return emu_umov_vector_to_gpr_w_h_lane3;
                case 4:
                    return emu_umov_vector_to_gpr_w_h_lane4;
                case 5:
                    return emu_umov_vector_to_gpr_w_h_lane5;
                case 6:
                    return emu_umov_vector_to_gpr_w_h_lane6;
                case 7:
                    return emu_umov_vector_to_gpr_w_h_lane7;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_umov_vector_to_gpr_w_s_lane0;
                case 1:
                    return emu_umov_vector_to_gpr_w_s_lane1;
                case 2:
                    return emu_umov_vector_to_gpr_w_s_lane2;
                case 3:
                    return emu_umov_vector_to_gpr_w_s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_umov_vector_to_gpr_x_d_lane0;
                case 1:
                    return emu_umov_vector_to_gpr_x_d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SHADD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_shadd_vector_8b;
            case 16:
                return emu_shadd_vector_4h;
            case 32:
                return emu_shadd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_shadd_vector_16b;
            case 16:
                return emu_shadd_vector_8h;
            case 32:
                return emu_shadd_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_REV64_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_rev64_vector_8b;
            case 16:
                return emu_rev64_vector_4h;
            case 32:
                return emu_rev64_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_rev64_vector_16b;
            case 16:
                return emu_rev64_vector_8h;
            case 32:
                return emu_rev64_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQADD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sqadd_vector_8b;
            case 16:
                return emu_sqadd_vector_4h;
            case 32:
                return emu_sqadd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sqadd_vector_16b;
            case 16:
                return emu_sqadd_vector_8h;
            case 32:
                return emu_sqadd_vector_4s;
            case 64:
                return emu_sqadd_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SRHADD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_srhadd_vector_8b;
            case 16:
                return emu_srhadd_vector_4h;
            case 32:
                return emu_srhadd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_srhadd_vector_16b;
            case 16:
                return emu_srhadd_vector_8h;
            case 32:
                return emu_srhadd_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_REV16_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_rev16_vector_8b;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_rev16_vector_16b;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_AND_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_and_vector_8b;
        case 128:
            return emu_and_vector_16b;
        default:
            return NULL;
        }
    case ARM64_INST_SHSUB_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_shsub_vector_8b;
            case 16:
                return emu_shsub_vector_4h;
            case 32:
                return emu_shsub_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_shsub_vector_16b;
            case 16:
                return emu_shsub_vector_8h;
            case 32:
                return emu_shsub_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQSUB_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sqsub_vector_8b;
            case 16:
                return emu_sqsub_vector_4h;
            case 32:
                return emu_sqsub_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sqsub_vector_16b;
            case 16:
                return emu_sqsub_vector_8h;
            case 32:
                return emu_sqsub_vector_4s;
            case 64:
                return emu_sqsub_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CMGT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmgt_vector_8b;
            case 16:
                return emu_cmgt_vector_4h;
            case 32:
                return emu_cmgt_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmgt_vector_16b;
            case 16:
                return emu_cmgt_vector_8h;
            case 32:
                return emu_cmgt_vector_4s;
            case 64:
                return emu_cmgt_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CMGE_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmge_vector_8b;
            case 16:
                return emu_cmge_vector_4h;
            case 32:
                return emu_cmge_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmge_vector_16b;
            case 16:
                return emu_cmge_vector_8h;
            case 32:
                return emu_cmge_vector_4s;
            case 64:
                return emu_cmge_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SSHL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sshl_vector_8b;
            case 16:
                return emu_sshl_vector_4h;
            case 32:
                return emu_sshl_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sshl_vector_16b;
            case 16:
                return emu_sshl_vector_8h;
            case 32:
                return emu_sshl_vector_4s;
            case 64:
                return emu_sshl_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQSHL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sqshl_vector_8b;
            case 16:
                return emu_sqshl_vector_4h;
            case 32:
                return emu_sqshl_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sqshl_vector_16b;
            case 16:
                return emu_sqshl_vector_8h;
            case 32:
                return emu_sqshl_vector_4s;
            case 64:
                return emu_sqshl_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SRSHL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_srshl_vector_8b;
            case 16:
                return emu_srshl_vector_4h;
            case 32:
                return emu_srshl_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_srshl_vector_16b;
            case 16:
                return emu_srshl_vector_8h;
            case 32:
                return emu_srshl_vector_4s;
            case 64:
                return emu_srshl_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRSHL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sqrshl_vector_8b;
            case 16:
                return emu_sqrshl_vector_4h;
            case 32:
                return emu_sqrshl_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sqrshl_vector_16b;
            case 16:
                return emu_sqrshl_vector_8h;
            case 32:
                return emu_sqrshl_vector_4s;
            case 64:
                return emu_sqrshl_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SMAX_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_smax_vector_8b;
            case 16:
                return emu_smax_vector_4h;
            case 32:
                return emu_smax_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_smax_vector_16b;
            case 16:
                return emu_smax_vector_8h;
            case 32:
                return emu_smax_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SMIN_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_smin_vector_8b;
            case 16:
                return emu_smin_vector_4h;
            case 32:
                return emu_smin_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_smin_vector_16b;
            case 16:
                return emu_smin_vector_8h;
            case 32:
                return emu_smin_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SABD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sabd_vector_8b;
            case 16:
                return emu_sabd_vector_4h;
            case 32:
                return emu_sabd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sabd_vector_16b;
            case 16:
                return emu_sabd_vector_8h;
            case 32:
                return emu_sabd_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SABA_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_saba_vector_8b;
            case 16:
                return emu_saba_vector_4h;
            case 32:
                return emu_saba_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_saba_vector_16b;
            case 16:
                return emu_saba_vector_8h;
            case 32:
                return emu_saba_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ADD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_add_vector_8b;
            case 16:
                return emu_add_vector_4h;
            case 32:
                return emu_add_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_add_vector_16b;
            case 16:
                return emu_add_vector_8h;
            case 32:
                return emu_add_vector_4s;
            case 64:
                return emu_add_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CMTST_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmtst_vector_8b;
            case 16:
                return emu_cmtst_vector_4h;
            case 32:
                return emu_cmtst_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmtst_vector_16b;
            case 16:
                return emu_cmtst_vector_8h;
            case 32:
                return emu_cmtst_vector_4s;
            case 64:
                return emu_cmtst_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_MLA_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_mla_vector_8b;
            case 16:
                return emu_mla_vector_4h;
            case 32:
                return emu_mla_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_mla_vector_16b;
            case 16:
                return emu_mla_vector_8h;
            case 32:
                return emu_mla_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_MUL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_mul_vector_8b;
            case 16:
                return emu_mul_vector_4h;
            case 32:
                return emu_mul_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_mul_vector_16b;
            case 16:
                return emu_mul_vector_8h;
            case 32:
                return emu_mul_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SMAXP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_smaxp_vector_8b;
            case 16:
                return emu_smaxp_vector_4h;
            case 32:
                return emu_smaxp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_smaxp_vector_16b;
            case 16:
                return emu_smaxp_vector_8h;
            case 32:
                return emu_smaxp_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SMINP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sminp_vector_8b;
            case 16:
                return emu_sminp_vector_4h;
            case 32:
                return emu_sminp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sminp_vector_16b;
            case 16:
                return emu_sminp_vector_8h;
            case 32:
                return emu_sminp_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ADDP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_addp_vector_8b;
            case 16:
                return emu_addp_vector_4h;
            case 32:
                return emu_addp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_addp_vector_16b;
            case 16:
                return emu_addp_vector_8h;
            case 32:
                return emu_addp_vector_4s;
            case 64:
                return emu_addp_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMAXNM_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmaxnm_vector_4h;
            case 32:
                return emu_fmaxnm_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmaxnm_vector_8h;
            case 32:
                return emu_fmaxnm_vector_4s;
            case 64:
                return emu_fmaxnm_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLA_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmla_vector_4h;
            case 32:
                return emu_fmla_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmla_vector_8h;
            case 32:
                return emu_fmla_vector_4s;
            case 64:
                return emu_fmla_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FADD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fadd_vector_4h;
            case 32:
                return emu_fadd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fadd_vector_8h;
            case 32:
                return emu_fadd_vector_4s;
            case 64:
                return emu_fadd_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMULX_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmulx_vector_4h;
            case 32:
                return emu_fmulx_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmulx_vector_8h;
            case 32:
                return emu_fmulx_vector_4s;
            case 64:
                return emu_fmulx_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMEQ_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmeq_vector_4h;
            case 32:
                return emu_fcmeq_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmeq_vector_8h;
            case 32:
                return emu_fcmeq_vector_4s;
            case 64:
                return emu_fcmeq_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLAL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmlal_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmlal_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMAX_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmax_vector_4h;
            case 32:
                return emu_fmax_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmax_vector_8h;
            case 32:
                return emu_fmax_vector_4s;
            case 64:
                return emu_fmax_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FRECPS_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_frecps_vector_4h;
            case 32:
                return emu_frecps_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_frecps_vector_8h;
            case 32:
                return emu_frecps_vector_4s;
            case 64:
                return emu_frecps_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_XTN_VECTOR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_xtn_vector_h;
        case 32:
            return emu_xtn_vector_s;
        case 64:
            return emu_xtn_vector_d;
        default:
            return NULL;
        }
    case ARM64_INST_SQXTN_VECTOR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_sqxtn_vector_h;
        case 32:
            return emu_sqxtn_vector_s;
        case 64:
            return emu_sqxtn_vector_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCVTNS_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtns_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtns_vector_4s;
            case 64:
                return emu_fcvtns_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTMS_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtms_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtms_vector_4s;
            case 64:
                return emu_fcvtms_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTAS_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtas_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtas_vector_4s;
            case 64:
                return emu_fcvtas_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SCVTF_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_scvtf_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_scvtf_vector_4s;
            case 64:
                return emu_scvtf_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SADDLV_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_saddlv_vector_h_8b;
            case 16:
                return emu_saddlv_vector_s_4h;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_saddlv_vector_h_16b;
            case 16:
                return emu_saddlv_vector_s_8h;
            case 32:
                return emu_saddlv_vector_d_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SMAXV_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_smaxv_vector_b_8b;
            case 16:
                return emu_smaxv_vector_h_4h;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_smaxv_vector_b_16b;
            case 16:
                return emu_smaxv_vector_h_8h;
            case 32:
                return emu_smaxv_vector_s_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMAXNMV_SCALAR_REDUCE:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmaxnmv_scalar_reduce_h_4h;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMAXV_SCALAR_REDUCE:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmaxv_scalar_reduce_h_4h;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SMINV_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sminv_vector_b_8b;
            case 16:
                return emu_sminv_vector_h_4h;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sminv_vector_b_16b;
            case 16:
                return emu_sminv_vector_h_8h;
            case 32:
                return emu_sminv_vector_s_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ADDV_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_addv_vector_b_8b;
            case 16:
                return emu_addv_vector_h_4h;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_addv_vector_b_16b;
            case 16:
                return emu_addv_vector_h_8h;
            case 32:
                return emu_addv_vector_s_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BIC_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_bic_vector_8b;
        case 128:
            return emu_bic_vector_16b;
        default:
            return NULL;
        }
    case ARM64_INST_SQDMULH_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqdmulh_vector_4h;
            case 32:
                return emu_sqdmulh_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqdmulh_vector_8h;
            case 32:
                return emu_sqdmulh_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SDOT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sdot_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sdot_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_USDOT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_usdot_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_usdot_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ORR_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_orr_vector_8b;
        case 128:
            return emu_orr_vector_16b;
        default:
            return NULL;
        }
    case ARM64_INST_FMINNM_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fminnm_vector_4h;
            case 32:
                return emu_fminnm_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fminnm_vector_8h;
            case 32:
                return emu_fminnm_vector_4s;
            case 64:
                return emu_fminnm_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMGT_ZERO_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmgt_zero_vector_4h;
            case 32:
                return emu_fcmgt_zero_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmgt_zero_vector_8h;
            case 32:
                return emu_fcmgt_zero_vector_4s;
            case 64:
                return emu_fcmgt_zero_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLS_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmls_vector_4h;
            case 32:
                return emu_fmls_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmls_vector_8h;
            case 32:
                return emu_fmls_vector_4s;
            case 64:
                return emu_fmls_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FSUB_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fsub_vector_4h;
            case 32:
                return emu_fsub_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fsub_vector_8h;
            case 32:
                return emu_fsub_vector_4s;
            case 64:
                return emu_fsub_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMEQ_ZERO_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmeq_zero_vector_4h;
            case 32:
                return emu_fcmeq_zero_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmeq_zero_vector_8h;
            case 32:
                return emu_fcmeq_zero_vector_4s;
            case 64:
                return emu_fcmeq_zero_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FAMAX_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_famax_vector_4h;
            case 32:
                return emu_famax_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_famax_vector_8h;
            case 32:
                return emu_famax_vector_4s;
            case 64:
                return emu_famax_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMLT_ZERO_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmlt_zero_vector_4h;
            case 32:
                return emu_fcmlt_zero_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmlt_zero_vector_8h;
            case 32:
                return emu_fcmlt_zero_vector_4s;
            case 64:
                return emu_fcmlt_zero_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLSL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmlsl_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmlsl_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMIN_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmin_vector_4h;
            case 32:
                return emu_fmin_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmin_vector_8h;
            case 32:
                return emu_fmin_vector_4s;
            case 64:
                return emu_fmin_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FABS_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fabs_vector_4h;
            case 32:
                return emu_fabs_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fabs_vector_8h;
            case 32:
                return emu_fabs_vector_4s;
            case 64:
                return emu_fabs_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FRINTM_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_frintm_vector_2s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FRSQRTS_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_frsqrts_vector_4h;
            case 32:
                return emu_frsqrts_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_frsqrts_vector_8h;
            case 32:
                return emu_frsqrts_vector_4s;
            case 64:
                return emu_frsqrts_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTPS_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtps_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtps_vector_4s;
            case 64:
                return emu_fcvtps_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTZS_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzs_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzs_vector_4s;
            case 64:
                return emu_fcvtzs_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMINNMV_SCALAR_REDUCE:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fminnmv_scalar_reduce_h_4h;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMINV_SCALAR_REDUCE:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fminv_scalar_reduce_h_4h;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ORN_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_orn_vector_8b;
        case 128:
            return emu_orn_vector_16b;
        default:
            return NULL;
        }
    case ARM64_INST_MOVI_VECTOR_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->simd_cmode)
            {
            case 0:
                return emu_movi_vector_immediate_d;
            case 2:
                return emu_movi_vector_immediate_d_cmode2;
            case 4:
                return emu_movi_vector_immediate_d_cmode4;
            case 6:
                return emu_movi_vector_immediate_d_cmode6;
            case 8:
                return emu_movi_vector_immediate_d_cmode8;
            case 10:
                return emu_movi_vector_immediate_d_cmode10;
            case 12:
                return emu_movi_vector_immediate_d_cmode12;
            case 13:
                return emu_movi_vector_immediate_d_cmode13;
            case 14:
                switch (decoded->element_width)
                {
                case 8:
                    return emu_movi_vector_immediate_d_cmode14;
                case 64:
                    switch (decoded->immediate)
                    {
                    case 0:
                        return emu_movi_vector_immediate_d_cmode14_d;
                    default:
                        return NULL;
                    }
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->simd_cmode)
            {
            case 0:
                return emu_movi_vector_immediate_q;
            case 2:
                return emu_movi_vector_immediate_q_cmode2;
            case 4:
                return emu_movi_vector_immediate_q_cmode4;
            case 6:
                return emu_movi_vector_immediate_q_cmode6;
            case 8:
                return emu_movi_vector_immediate_q_cmode8;
            case 10:
                return emu_movi_vector_immediate_q_cmode10;
            case 12:
                return emu_movi_vector_immediate_q_cmode12;
            case 13:
                return emu_movi_vector_immediate_q_cmode13;
            case 14:
                switch (decoded->element_width)
                {
                case 8:
                    return emu_movi_vector_immediate_q_cmode14;
                case 64:
                    switch (decoded->immediate)
                    {
                    case 0:
                        return emu_movi_vector_immediate_2d_cmode14_d;
                    default:
                        return NULL;
                    }
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLA_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmla_vector_by_element_4h_lane0;
                case 1:
                    return emu_fmla_vector_by_element_4h_lane1;
                case 2:
                    return emu_fmla_vector_by_element_4h_lane2;
                case 3:
                    return emu_fmla_vector_by_element_4h_lane3;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmla_vector_by_element_2s_lane0;
                case 1:
                    return emu_fmla_vector_by_element_2s_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmla_vector_by_element_8h_lane0;
                case 1:
                    return emu_fmla_vector_by_element_8h_lane1;
                case 2:
                    return emu_fmla_vector_by_element_8h_lane2;
                case 3:
                    return emu_fmla_vector_by_element_8h_lane3;
                case 4:
                    return emu_fmla_vector_by_element_8h_lane4;
                case 5:
                    return emu_fmla_vector_by_element_8h_lane5;
                case 6:
                    return emu_fmla_vector_by_element_8h_lane6;
                case 7:
                    return emu_fmla_vector_by_element_8h_lane7;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmla_vector_by_element_4s_lane0;
                case 1:
                    return emu_fmla_vector_by_element_4s_lane1;
                case 2:
                    return emu_fmla_vector_by_element_4s_lane2;
                case 3:
                    return emu_fmla_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmla_vector_by_element_2d_lane0;
                case 1:
                    return emu_fmla_vector_by_element_2d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_ORR_VECTOR_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_orr_vector_immediate_2s;
        case 128:
            return emu_orr_vector_immediate_4s;
        default:
            return NULL;
        }
    case ARM64_INST_FMLS_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmls_vector_by_element_4h_lane0;
                case 1:
                    return emu_fmls_vector_by_element_4h_lane1;
                case 2:
                    return emu_fmls_vector_by_element_4h_lane2;
                case 3:
                    return emu_fmls_vector_by_element_4h_lane3;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmls_vector_by_element_2s_lane0;
                case 1:
                    return emu_fmls_vector_by_element_2s_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmls_vector_by_element_8h_lane0;
                case 1:
                    return emu_fmls_vector_by_element_8h_lane1;
                case 2:
                    return emu_fmls_vector_by_element_8h_lane2;
                case 3:
                    return emu_fmls_vector_by_element_8h_lane3;
                case 4:
                    return emu_fmls_vector_by_element_8h_lane4;
                case 5:
                    return emu_fmls_vector_by_element_8h_lane5;
                case 6:
                    return emu_fmls_vector_by_element_8h_lane6;
                case 7:
                    return emu_fmls_vector_by_element_8h_lane7;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmls_vector_by_element_4s_lane0;
                case 1:
                    return emu_fmls_vector_by_element_4s_lane1;
                case 2:
                    return emu_fmls_vector_by_element_4s_lane2;
                case 3:
                    return emu_fmls_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmls_vector_by_element_2d_lane0;
                case 1:
                    return emu_fmls_vector_by_element_2d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMUL_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmul_vector_by_element_4h_lane0;
                case 1:
                    return emu_fmul_vector_by_element_4h_lane1;
                case 2:
                    return emu_fmul_vector_by_element_4h_lane2;
                case 3:
                    return emu_fmul_vector_by_element_4h_lane3;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmul_vector_by_element_2s_lane0;
                case 1:
                    return emu_fmul_vector_by_element_2s_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmul_vector_by_element_8h_lane0;
                case 1:
                    return emu_fmul_vector_by_element_8h_lane1;
                case 2:
                    return emu_fmul_vector_by_element_8h_lane2;
                case 3:
                    return emu_fmul_vector_by_element_8h_lane3;
                case 4:
                    return emu_fmul_vector_by_element_8h_lane4;
                case 5:
                    return emu_fmul_vector_by_element_8h_lane5;
                case 6:
                    return emu_fmul_vector_by_element_8h_lane6;
                case 7:
                    return emu_fmul_vector_by_element_8h_lane7;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmul_vector_by_element_4s_lane0;
                case 1:
                    return emu_fmul_vector_by_element_4s_lane1;
                case 2:
                    return emu_fmul_vector_by_element_4s_lane2;
                case 3:
                    return emu_fmul_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmul_vector_by_element_2d_lane0;
                case 1:
                    return emu_fmul_vector_by_element_2d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SUDOT_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sudot_vector_by_element_2s_lane0;
                case 1:
                    return emu_sudot_vector_by_element_2s_lane1;
                case 2:
                    return emu_sudot_vector_by_element_2s_lane2;
                case 3:
                    return emu_sudot_vector_by_element_2s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sudot_vector_by_element_4s_lane0;
                case 1:
                    return emu_sudot_vector_by_element_4s_lane1;
                case 2:
                    return emu_sudot_vector_by_element_4s_lane2;
                case 3:
                    return emu_sudot_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMOV_VECTOR_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->simd_cmode)
            {
            case 15:
                switch (decoded->element_width)
                {
                case 32:
                    switch (decoded->immediate)
                    {
                    case 0x70:
                        return emu_fmov_vector_immediate_2s_imm70;
                    case 0xF0:
                        return emu_fmov_vector_immediate_2s_immf0;
                    default:
                        return NULL;
                    }
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SHL_VECTOR_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_shl_vector_immediate_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BFDOT_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_bfdot_vector_by_element_2s_lane0;
                case 1:
                    return emu_bfdot_vector_by_element_2s_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_bfdot_vector_by_element_4s_lane0;
                case 1:
                    return emu_bfdot_vector_by_element_4s_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLAL_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmlal_vector_by_element_2s_lane0;
                case 1:
                    return emu_fmlal_vector_by_element_2s_lane1;
                case 2:
                    return emu_fmlal_vector_by_element_2s_lane2;
                case 3:
                    return emu_fmlal_vector_by_element_2s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmlal_vector_by_element_4s_lane0;
                case 1:
                    return emu_fmlal_vector_by_element_4s_lane1;
                case 2:
                    return emu_fmlal_vector_by_element_4s_lane2;
                case 3:
                    return emu_fmlal_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLSL_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmlsl_vector_by_element_2s_lane0;
                case 1:
                    return emu_fmlsl_vector_by_element_2s_lane1;
                case 2:
                    return emu_fmlsl_vector_by_element_2s_lane2;
                case 3:
                    return emu_fmlsl_vector_by_element_2s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmlsl_vector_by_element_4s_lane0;
                case 1:
                    return emu_fmlsl_vector_by_element_4s_lane1;
                case 2:
                    return emu_fmlsl_vector_by_element_4s_lane2;
                case 3:
                    return emu_fmlsl_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SDOT_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sdot_vector_by_element_2s_lane0;
                case 1:
                    return emu_sdot_vector_by_element_2s_lane1;
                case 2:
                    return emu_sdot_vector_by_element_2s_lane2;
                case 3:
                    return emu_sdot_vector_by_element_2s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sdot_vector_by_element_4s_lane0;
                case 1:
                    return emu_sdot_vector_by_element_4s_lane1;
                case 2:
                    return emu_sdot_vector_by_element_4s_lane2;
                case 3:
                    return emu_sdot_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_USDOT_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_usdot_vector_by_element_2s_lane0;
                case 1:
                    return emu_usdot_vector_by_element_2s_lane1;
                case 2:
                    return emu_usdot_vector_by_element_2s_lane2;
                case 3:
                    return emu_usdot_vector_by_element_2s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_usdot_vector_by_element_4s_lane0;
                case 1:
                    return emu_usdot_vector_by_element_4s_lane1;
                case 2:
                    return emu_usdot_vector_by_element_4s_lane2;
                case 3:
                    return emu_usdot_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BFMLALB_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_bfmlalb_vector_by_element_8h_lane0;
                case 1:
                    return emu_bfmlalb_vector_by_element_8h_lane1;
                case 2:
                    return emu_bfmlalb_vector_by_element_8h_lane2;
                case 3:
                    return emu_bfmlalb_vector_by_element_8h_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTNS_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtns_gpr_w_s;
            case 64:
                return emu_fcvtns_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtns_gpr_x_s;
            case 64:
                return emu_fcvtns_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCCMP_SCALAR:
        if (decoded->condition > 15) return NULL;
        switch (decoded->element_width)
        {
        case 16:
            return emu_fccmp_scalar_h;
        case 32:
            return emu_fccmp_scalar_s;
        case 64:
            return emu_fccmp_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCCMPE_SCALAR:
        if (decoded->condition > 15) return NULL;
        switch (decoded->element_width)
        {
        case 16:
            return emu_fccmpe_scalar_h;
        case 32:
            return emu_fccmpe_scalar_s;
        case 64:
            return emu_fccmpe_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMUL_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fmul_scalar_h;
        case 32:
            return emu_fmul_scalar_s;
        case 64:
            return emu_fmul_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCSEL_SCALAR:
        if (decoded->condition > 15) return NULL;
        switch (decoded->element_width)
        {
        case 16:
            return emu_fcsel_scalar_h;
        case 32:
            return emu_fcsel_scalar_s;
        case 64:
            return emu_fcsel_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMOV_SCALAR_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->immediate)
            {
            case 0x60:
                return emu_fmov_scalar_immediate_s_imm60;
            case 0x70:
                return emu_fmov_scalar_immediate_s_imm70;
            case 0xE0:
                return emu_fmov_scalar_immediate_s_imme0;
            case 0xF0:
                return emu_fmov_scalar_immediate_s_immf0;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->immediate)
            {
            case 0x00:
                return emu_fmov_scalar_immediate_d_imm00;
            case 0x78:
                return emu_fmov_scalar_immediate_d_imm78;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FDIV_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fdiv_scalar_h;
        case 32:
            return emu_fdiv_scalar_s;
        case 64:
            return emu_fdiv_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCMP_REGISTER_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fcmp_register_scalar_h;
        case 32:
            return emu_fcmp_register_scalar_s;
        case 64:
            return emu_fcmp_register_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCMP_ZERO_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fcmp_zero_scalar_h;
        case 32:
            return emu_fcmp_zero_scalar_s;
        case 64:
            return emu_fcmp_zero_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCMPE_REGISTER_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fcmpe_register_scalar_h;
        case 32:
            return emu_fcmpe_register_scalar_s;
        case 64:
            return emu_fcmpe_register_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCMPE_ZERO_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fcmpe_zero_scalar_h;
        case 32:
            return emu_fcmpe_zero_scalar_s;
        case 64:
            return emu_fcmpe_zero_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FADD_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fadd_scalar_h;
        case 32:
            return emu_fadd_scalar_s;
        case 64:
            return emu_fadd_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FSUB_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fsub_scalar_h;
        case 32:
            return emu_fsub_scalar_s;
        case 64:
            return emu_fsub_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMOV_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fmov_scalar_h;
        case 32:
            return emu_fmov_scalar_s;
        case 64:
            return emu_fmov_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMAX_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fmax_scalar_h;
        case 32:
            return emu_fmax_scalar_s;
        case 64:
            return emu_fmax_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMIN_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fmin_scalar_h;
        case 32:
            return emu_fmin_scalar_s;
        case 64:
            return emu_fmin_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMAXNM_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fmaxnm_scalar_h;
        case 32:
            return emu_fmaxnm_scalar_s;
        case 64:
            return emu_fmaxnm_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMINNM_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fminnm_scalar_h;
        case 32:
            return emu_fminnm_scalar_s;
        case 64:
            return emu_fminnm_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FNMUL_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fnmul_scalar_h;
        case 32:
            return emu_fnmul_scalar_s;
        case 64:
            return emu_fnmul_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FABS_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fabs_scalar_h;
        case 32:
            return emu_fabs_scalar_s;
        case 64:
            return emu_fabs_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCVTNU_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtnu_gpr_w_s;
            case 64:
                return emu_fcvtnu_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtnu_gpr_x_s;
            case 64:
                return emu_fcvtnu_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FNEG_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fneg_scalar_h;
        case 32:
            return emu_fneg_scalar_s;
        case 64:
            return emu_fneg_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FSQRT_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fsqrt_scalar_h;
        case 32:
            return emu_fsqrt_scalar_s;
        case 64:
            return emu_fsqrt_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_SCVTF_S_W:
        return emu_scvtf_s_w;
    case ARM64_INST_FCVT_D_S:
        switch (decoded->element_width)
        {
        case 32:
            return emu_fcvt_d_s;
        default:
            return NULL;
        }
    case ARM64_INST_UCVTF_S_W:
        return emu_ucvtf_s_w;
    case ARM64_INST_FCVTAS_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtas_gpr_w_s;
            case 64:
                return emu_fcvtas_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtas_gpr_x_s;
            case 64:
                return emu_fcvtas_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FRINTN_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_frintn_scalar_h;
        case 32:
            return emu_frintn_scalar_s;
        case 64:
            return emu_frintn_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FRINTP_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_frintp_scalar_h;
        case 32:
            return emu_frintp_scalar_s;
        case 64:
            return emu_frintp_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCVTAU_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtau_gpr_w_s;
            case 64:
                return emu_fcvtau_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtau_gpr_x_s;
            case 64:
                return emu_fcvtau_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FRINTM_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_frintm_scalar_h;
        case 32:
            return emu_frintm_scalar_s;
        case 64:
            return emu_frintm_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FRINTZ_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_frintz_scalar_h;
        case 32:
            return emu_frintz_scalar_s;
        case 64:
            return emu_frintz_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMOV_FP_TO_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_fmov_fp_to_gpr_w_s;
        case 64:
            return emu_fmov_fp_to_gpr_x_d;
        default:
            return NULL;
        }
    case ARM64_INST_FRINTA_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_frinta_scalar_h;
        case 32:
            return emu_frinta_scalar_s;
        case 64:
            return emu_frinta_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMOV_GPR_TO_FP:
        switch (decoded->operand_width)
        {
        case 32:
            return emu_fmov_gpr_to_fp_s_w;
        case 64:
            return emu_fmov_gpr_to_fp_d_x;
        default:
            return NULL;
        }
    case ARM64_INST_FRINTX_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_frintx_scalar_h;
        case 32:
            return emu_frintx_scalar_s;
        case 64:
            return emu_frintx_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FRINTI_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_frinti_scalar_h;
        case 32:
            return emu_frinti_scalar_s;
        case 64:
            return emu_frinti_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCVTPS_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtps_gpr_w_s;
            case 64:
                return emu_fcvtps_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtps_gpr_x_s;
            case 64:
                return emu_fcvtps_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTPU_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtpu_gpr_w_s;
            case 64:
                return emu_fcvtpu_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtpu_gpr_x_s;
            case 64:
                return emu_fcvtpu_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTMS_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtms_gpr_w_s;
            case 64:
                return emu_fcvtms_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtms_gpr_x_s;
            case 64:
                return emu_fcvtms_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTMU_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtmu_gpr_w_s;
            case 64:
                return emu_fcvtmu_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtmu_gpr_x_s;
            case 64:
                return emu_fcvtmu_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTZS_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzs_gpr_w_s;
            case 64:
                return emu_fcvtzs_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzs_gpr_x_s;
            case 64:
                return emu_fcvtzs_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTZU_GPR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzu_gpr_w_s;
            case 64:
                return emu_fcvtzu_gpr_w_d;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzu_gpr_x_s;
            case 64:
                return emu_fcvtzu_gpr_x_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SCVTF_D_W:
        return emu_scvtf_d_w;
    case ARM64_INST_FCVT_S_D:
        switch (decoded->element_width)
        {
        case 64:
            return emu_fcvt_s_d;
        default:
            return NULL;
        }
    case ARM64_INST_UCVTF_D_W:
        return emu_ucvtf_d_w;
    case ARM64_INST_FMADD_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fmadd_scalar_h;
        case 32:
            return emu_fmadd_scalar_s;
        case 64:
            return emu_fmadd_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMSUB_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fmsub_scalar_h;
        case 32:
            return emu_fmsub_scalar_s;
        case 64:
            return emu_fmsub_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FNMADD_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fnmadd_scalar_h;
        case 32:
            return emu_fnmadd_scalar_s;
        case 64:
            return emu_fnmadd_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FNMSUB_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            return emu_fnmsub_scalar_h;
        case 32:
            return emu_fnmsub_scalar_s;
        case 64:
            return emu_fnmsub_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_EXT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->immediate)
            {
            case 0:
                return emu_ext_vector_8b_imm0;
            case 1:
                return emu_ext_vector_8b_imm1;
            case 2:
                return emu_ext_vector_8b_imm2;
            case 3:
                return emu_ext_vector_8b_imm3;
            case 4:
                return emu_ext_vector_8b_imm4;
            case 5:
                return emu_ext_vector_8b_imm5;
            case 6:
                return emu_ext_vector_8b_imm6;
            case 7:
                return emu_ext_vector_8b_imm7;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->immediate)
            {
            case 0:
                return emu_ext_vector_16b_imm0;
            case 1:
                return emu_ext_vector_16b_imm1;
            case 2:
                return emu_ext_vector_16b_imm2;
            case 3:
                return emu_ext_vector_16b_imm3;
            case 4:
                return emu_ext_vector_16b_imm4;
            case 5:
                return emu_ext_vector_16b_imm5;
            case 6:
                return emu_ext_vector_16b_imm6;
            case 7:
                return emu_ext_vector_16b_imm7;
            case 8:
                return emu_ext_vector_16b_imm8;
            case 9:
                return emu_ext_vector_16b_imm9;
            case 10:
                return emu_ext_vector_16b_imm10;
            case 11:
                return emu_ext_vector_16b_imm11;
            case 12:
                return emu_ext_vector_16b_imm12;
            case 13:
                return emu_ext_vector_16b_imm13;
            case 14:
                return emu_ext_vector_16b_imm14;
            case 15:
                return emu_ext_vector_16b_imm15;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UHADD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uhadd_vector_8b;
            case 16:
                return emu_uhadd_vector_4h;
            case 32:
                return emu_uhadd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uhadd_vector_16b;
            case 16:
                return emu_uhadd_vector_8h;
            case 32:
                return emu_uhadd_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_REV32_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_rev32_vector_8b;
            case 16:
                return emu_rev32_vector_4h;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_rev32_vector_16b;
            case 16:
                return emu_rev32_vector_8h;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UQADD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uqadd_vector_8b;
            case 16:
                return emu_uqadd_vector_4h;
            case 32:
                return emu_uqadd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uqadd_vector_16b;
            case 16:
                return emu_uqadd_vector_8h;
            case 32:
                return emu_uqadd_vector_4s;
            case 64:
                return emu_uqadd_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_URHADD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_urhadd_vector_8b;
            case 16:
                return emu_urhadd_vector_4h;
            case 32:
                return emu_urhadd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_urhadd_vector_16b;
            case 16:
                return emu_urhadd_vector_8h;
            case 32:
                return emu_urhadd_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_EOR_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_eor_vector_8b;
        case 128:
            return emu_eor_vector_16b;
        default:
            return NULL;
        }
    case ARM64_INST_UHSUB_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uhsub_vector_8b;
            case 16:
                return emu_uhsub_vector_4h;
            case 32:
                return emu_uhsub_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uhsub_vector_16b;
            case 16:
                return emu_uhsub_vector_8h;
            case 32:
                return emu_uhsub_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UQSUB_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uqsub_vector_8b;
            case 16:
                return emu_uqsub_vector_4h;
            case 32:
                return emu_uqsub_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uqsub_vector_16b;
            case 16:
                return emu_uqsub_vector_8h;
            case 32:
                return emu_uqsub_vector_4s;
            case 64:
                return emu_uqsub_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CMHI_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmhi_vector_8b;
            case 16:
                return emu_cmhi_vector_4h;
            case 32:
                return emu_cmhi_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmhi_vector_16b;
            case 16:
                return emu_cmhi_vector_8h;
            case 32:
                return emu_cmhi_vector_4s;
            case 64:
                return emu_cmhi_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CMHS_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmhs_vector_8b;
            case 16:
                return emu_cmhs_vector_4h;
            case 32:
                return emu_cmhs_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmhs_vector_16b;
            case 16:
                return emu_cmhs_vector_8h;
            case 32:
                return emu_cmhs_vector_4s;
            case 64:
                return emu_cmhs_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_USHL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_ushl_vector_8b;
            case 16:
                return emu_ushl_vector_4h;
            case 32:
                return emu_ushl_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_ushl_vector_16b;
            case 16:
                return emu_ushl_vector_8h;
            case 32:
                return emu_ushl_vector_4s;
            case 64:
                return emu_ushl_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UQSHL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uqshl_vector_8b;
            case 16:
                return emu_uqshl_vector_4h;
            case 32:
                return emu_uqshl_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uqshl_vector_16b;
            case 16:
                return emu_uqshl_vector_8h;
            case 32:
                return emu_uqshl_vector_4s;
            case 64:
                return emu_uqshl_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_URSHL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_urshl_vector_8b;
            case 16:
                return emu_urshl_vector_4h;
            case 32:
                return emu_urshl_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_urshl_vector_16b;
            case 16:
                return emu_urshl_vector_8h;
            case 32:
                return emu_urshl_vector_4s;
            case 64:
                return emu_urshl_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UQRSHL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uqrshl_vector_8b;
            case 16:
                return emu_uqrshl_vector_4h;
            case 32:
                return emu_uqrshl_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uqrshl_vector_16b;
            case 16:
                return emu_uqrshl_vector_8h;
            case 32:
                return emu_uqrshl_vector_4s;
            case 64:
                return emu_uqrshl_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UMAX_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_umax_vector_8b;
            case 16:
                return emu_umax_vector_4h;
            case 32:
                return emu_umax_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_umax_vector_16b;
            case 16:
                return emu_umax_vector_8h;
            case 32:
                return emu_umax_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UMIN_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_umin_vector_8b;
            case 16:
                return emu_umin_vector_4h;
            case 32:
                return emu_umin_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_umin_vector_16b;
            case 16:
                return emu_umin_vector_8h;
            case 32:
                return emu_umin_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UABD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uabd_vector_8b;
            case 16:
                return emu_uabd_vector_4h;
            case 32:
                return emu_uabd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uabd_vector_16b;
            case 16:
                return emu_uabd_vector_8h;
            case 32:
                return emu_uabd_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UABA_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uaba_vector_8b;
            case 16:
                return emu_uaba_vector_4h;
            case 32:
                return emu_uaba_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uaba_vector_16b;
            case 16:
                return emu_uaba_vector_8h;
            case 32:
                return emu_uaba_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SUB_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sub_vector_8b;
            case 16:
                return emu_sub_vector_4h;
            case 32:
                return emu_sub_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_sub_vector_16b;
            case 16:
                return emu_sub_vector_8h;
            case 32:
                return emu_sub_vector_4s;
            case 64:
                return emu_sub_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CMEQ_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmeq_vector_8b;
            case 16:
                return emu_cmeq_vector_4h;
            case 32:
                return emu_cmeq_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_cmeq_vector_16b;
            case 16:
                return emu_cmeq_vector_8h;
            case 32:
                return emu_cmeq_vector_4s;
            case 64:
                return emu_cmeq_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_MLS_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_mls_vector_8b;
            case 16:
                return emu_mls_vector_4h;
            case 32:
                return emu_mls_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_mls_vector_16b;
            case 16:
                return emu_mls_vector_8h;
            case 32:
                return emu_mls_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_PMUL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_pmul_vector_8b;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_pmul_vector_16b;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UMAXP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_umaxp_vector_8b;
            case 16:
                return emu_umaxp_vector_4h;
            case 32:
                return emu_umaxp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_umaxp_vector_16b;
            case 16:
                return emu_umaxp_vector_8h;
            case 32:
                return emu_umaxp_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UMINP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uminp_vector_8b;
            case 16:
                return emu_uminp_vector_4h;
            case 32:
                return emu_uminp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uminp_vector_16b;
            case 16:
                return emu_uminp_vector_8h;
            case 32:
                return emu_uminp_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMAXNMP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmaxnmp_vector_4h;
            case 32:
                return emu_fmaxnmp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmaxnmp_vector_8h;
            case 32:
                return emu_fmaxnmp_vector_4s;
            case 64:
                return emu_fmaxnmp_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLAL2_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmlal2_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmlal2_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FADDP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_faddp_vector_4h;
            case 32:
                return emu_faddp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_faddp_vector_8h;
            case 32:
                return emu_faddp_vector_4s;
            case 64:
                return emu_faddp_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMUL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmul_vector_4h;
            case 32:
                return emu_fmul_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmul_vector_8h;
            case 32:
                return emu_fmul_vector_4s;
            case 64:
                return emu_fmul_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMGE_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmge_vector_4h;
            case 32:
                return emu_fcmge_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmge_vector_8h;
            case 32:
                return emu_fcmge_vector_4s;
            case 64:
                return emu_fcmge_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FACGE_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_facge_vector_4h;
            case 32:
                return emu_facge_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_facge_vector_8h;
            case 32:
                return emu_facge_vector_4s;
            case 64:
                return emu_facge_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMAXP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmaxp_vector_4h;
            case 32:
                return emu_fmaxp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmaxp_vector_8h;
            case 32:
                return emu_fmaxp_vector_4s;
            case 64:
                return emu_fmaxp_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FDIV_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fdiv_vector_4h;
            case 32:
                return emu_fdiv_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fdiv_vector_8h;
            case 32:
                return emu_fdiv_vector_4s;
            case 64:
                return emu_fdiv_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQXTUN_VECTOR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_sqxtun_vector_h;
        case 32:
            return emu_sqxtun_vector_s;
        case 64:
            return emu_sqxtun_vector_d;
        default:
            return NULL;
        }
    case ARM64_INST_UQXTN_VECTOR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_uqxtn_vector_h;
        case 32:
            return emu_uqxtn_vector_s;
        case 64:
            return emu_uqxtn_vector_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCVTNU_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtnu_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtnu_vector_4s;
            case 64:
                return emu_fcvtnu_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTMU_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtmu_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtmu_vector_4s;
            case 64:
                return emu_fcvtmu_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTAU_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtau_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtau_vector_4s;
            case 64:
                return emu_fcvtau_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UCVTF_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_ucvtf_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_ucvtf_vector_4s;
            case 64:
                return emu_ucvtf_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UADDLV_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uaddlv_vector_h_8b;
            case 16:
                return emu_uaddlv_vector_s_4h;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uaddlv_vector_h_16b;
            case 16:
                return emu_uaddlv_vector_s_8h;
            case 32:
                return emu_uaddlv_vector_d_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UMAXV_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_umaxv_vector_b_8b;
            case 16:
                return emu_umaxv_vector_h_4h;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_umaxv_vector_b_16b;
            case 16:
                return emu_umaxv_vector_h_8h;
            case 32:
                return emu_umaxv_vector_s_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UMINV_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uminv_vector_b_8b;
            case 16:
                return emu_uminv_vector_h_4h;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_uminv_vector_b_16b;
            case 16:
                return emu_uminv_vector_h_8h;
            case 32:
                return emu_uminv_vector_s_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMLAH_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqrdmlah_vector_4h;
            case 32:
                return emu_sqrdmlah_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqrdmlah_vector_8h;
            case 32:
                return emu_sqrdmlah_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMLSH_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqrdmlsh_vector_4h;
            case 32:
                return emu_sqrdmlsh_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqrdmlsh_vector_8h;
            case 32:
                return emu_sqrdmlsh_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMLA_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_0:
                    return emu_fcmla_vector_4h_imm0;
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcmla_vector_4h_imm90;
                case ARM64_SIMD_ROTATION_180:
                    return emu_fcmla_vector_4h_imm180;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcmla_vector_4h_imm270;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_0:
                    return emu_fcmla_vector_2s_imm0;
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcmla_vector_2s_imm90;
                case ARM64_SIMD_ROTATION_180:
                    return emu_fcmla_vector_2s_imm180;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcmla_vector_2s_imm270;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_0:
                    return emu_fcmla_vector_8h_imm0;
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcmla_vector_8h_imm90;
                case ARM64_SIMD_ROTATION_180:
                    return emu_fcmla_vector_8h_imm180;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcmla_vector_8h_imm270;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_0:
                    return emu_fcmla_vector_4s_imm0;
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcmla_vector_4s_imm90;
                case ARM64_SIMD_ROTATION_180:
                    return emu_fcmla_vector_4s_imm180;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcmla_vector_4s_imm270;
                default:
                    return NULL;
                }
            case 64:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_0:
                    return emu_fcmla_vector_2d_imm0;
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcmla_vector_2d_imm90;
                case ARM64_SIMD_ROTATION_180:
                    return emu_fcmla_vector_2d_imm180;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcmla_vector_2d_imm270;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCADD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcadd_vector_4h_imm90;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcadd_vector_4h_imm270;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcadd_vector_2s_imm90;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcadd_vector_2s_imm270;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcadd_vector_8h_imm90;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcadd_vector_8h_imm270;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcadd_vector_4s_imm90;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcadd_vector_4s_imm270;
                default:
                    return NULL;
                }
            case 64:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_90:
                    return emu_fcadd_vector_2d_imm90;
                case ARM64_SIMD_ROTATION_270:
                    return emu_fcadd_vector_2d_imm270;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BFDOT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_bfdot_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_bfdot_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BSL_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_bsl_vector_8b;
        case 128:
            return emu_bsl_vector_16b;
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMULH_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqrdmulh_vector_4h;
            case 32:
                return emu_sqrdmulh_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqrdmulh_vector_8h;
            case 32:
                return emu_sqrdmulh_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UDOT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                return emu_udot_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_udot_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BIT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_bit_vector_8b;
        case 128:
            return emu_bit_vector_16b;
        default:
            return NULL;
        }
    case ARM64_INST_FMINNMP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fminnmp_vector_4h;
            case 32:
                return emu_fminnmp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fminnmp_vector_8h;
            case 32:
                return emu_fminnmp_vector_4s;
            case 64:
                return emu_fminnmp_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMGE_ZERO_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmge_zero_vector_4h;
            case 32:
                return emu_fcmge_zero_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmge_zero_vector_8h;
            case 32:
                return emu_fcmge_zero_vector_4s;
            case 64:
                return emu_fcmge_zero_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLSL2_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmlsl2_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fmlsl2_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FABD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fabd_vector_4h;
            case 32:
                return emu_fabd_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fabd_vector_8h;
            case 32:
                return emu_fabd_vector_4s;
            case 64:
                return emu_fabd_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMLE_ZERO_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmle_zero_vector_4h;
            case 32:
                return emu_fcmle_zero_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmle_zero_vector_8h;
            case 32:
                return emu_fcmle_zero_vector_4s;
            case 64:
                return emu_fcmle_zero_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FAMIN_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_famin_vector_4h;
            case 32:
                return emu_famin_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_famin_vector_8h;
            case 32:
                return emu_famin_vector_4s;
            case 64:
                return emu_famin_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMGT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmgt_vector_4h;
            case 32:
                return emu_fcmgt_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmgt_vector_8h;
            case 32:
                return emu_fcmgt_vector_4s;
            case 64:
                return emu_fcmgt_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FACGT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_facgt_vector_4h;
            case 32:
                return emu_facgt_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_facgt_vector_8h;
            case 32:
                return emu_facgt_vector_4s;
            case 64:
                return emu_facgt_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMINP_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fminp_vector_4h;
            case 32:
                return emu_fminp_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fminp_vector_8h;
            case 32:
                return emu_fminp_vector_4s;
            case 64:
                return emu_fminp_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FNEG_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fneg_vector_4h;
            case 32:
                return emu_fneg_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fneg_vector_8h;
            case 32:
                return emu_fneg_vector_4s;
            case 64:
                return emu_fneg_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FSCALE_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fscale_vector_4h;
            case 32:
                return emu_fscale_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fscale_vector_8h;
            case 32:
                return emu_fscale_vector_4s;
            case 64:
                return emu_fscale_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTPU_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtpu_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtpu_vector_4s;
            case 64:
                return emu_fcvtpu_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTZU_SIMD_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzu_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzu_vector_4s;
            case 64:
                return emu_fcvtzu_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FSQRT_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fsqrt_vector_4h;
            case 32:
                return emu_fsqrt_vector_2s;
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fsqrt_vector_8h;
            case 32:
                return emu_fsqrt_vector_4s;
            case 64:
                return emu_fsqrt_vector_2d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BFMLALB_VECTOR:
        switch (decoded->operand_width)
        {
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_bfmlalb_vector_8h;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BIF_VECTOR:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_bif_vector_8b;
        case 128:
            return emu_bif_vector_16b;
        default:
            return NULL;
        }
    case ARM64_INST_MVNI_VECTOR_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_mvni_vector_immediate_2s;
        case 128:
            return emu_mvni_vector_immediate_4s;
        default:
            return NULL;
        }
    case ARM64_INST_BIC_VECTOR_IMMEDIATE:
        switch (decoded->operand_width)
        {
        case 64:
            return emu_bic_vector_immediate_2s;
        case 128:
            return emu_bic_vector_immediate_4s;
        default:
            return NULL;
        }
    case ARM64_INST_FMULX_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmulx_vector_by_element_4h_lane0;
                case 1:
                    return emu_fmulx_vector_by_element_4h_lane1;
                case 2:
                    return emu_fmulx_vector_by_element_4h_lane2;
                case 3:
                    return emu_fmulx_vector_by_element_4h_lane3;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmulx_vector_by_element_2s_lane0;
                case 1:
                    return emu_fmulx_vector_by_element_2s_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmulx_vector_by_element_8h_lane0;
                case 1:
                    return emu_fmulx_vector_by_element_8h_lane1;
                case 2:
                    return emu_fmulx_vector_by_element_8h_lane2;
                case 3:
                    return emu_fmulx_vector_by_element_8h_lane3;
                case 4:
                    return emu_fmulx_vector_by_element_8h_lane4;
                case 5:
                    return emu_fmulx_vector_by_element_8h_lane5;
                case 6:
                    return emu_fmulx_vector_by_element_8h_lane6;
                case 7:
                    return emu_fmulx_vector_by_element_8h_lane7;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmulx_vector_by_element_4s_lane0;
                case 1:
                    return emu_fmulx_vector_by_element_4s_lane1;
                case 2:
                    return emu_fmulx_vector_by_element_4s_lane2;
                case 3:
                    return emu_fmulx_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmulx_vector_by_element_2d_lane0;
                case 1:
                    return emu_fmulx_vector_by_element_2d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMLA_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_0:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_4h_imm0_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_4h_imm0_lane1;
                    default:
                        return NULL;
                    }
                case ARM64_SIMD_ROTATION_90:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_4h_imm90_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_4h_imm90_lane1;
                    default:
                        return NULL;
                    }
                case ARM64_SIMD_ROTATION_180:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_4h_imm180_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_4h_imm180_lane1;
                    default:
                        return NULL;
                    }
                case ARM64_SIMD_ROTATION_270:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_4h_imm270_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_4h_imm270_lane1;
                    default:
                        return NULL;
                    }
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_0:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_8h_imm0_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_8h_imm0_lane1;
                    case 2:
                        return emu_fcmla_vector_by_element_8h_imm0_lane2;
                    case 3:
                        return emu_fcmla_vector_by_element_8h_imm0_lane3;
                    default:
                        return NULL;
                    }
                case ARM64_SIMD_ROTATION_90:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_8h_imm90_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_8h_imm90_lane1;
                    case 2:
                        return emu_fcmla_vector_by_element_8h_imm90_lane2;
                    case 3:
                        return emu_fcmla_vector_by_element_8h_imm90_lane3;
                    default:
                        return NULL;
                    }
                case ARM64_SIMD_ROTATION_180:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_8h_imm180_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_8h_imm180_lane1;
                    case 2:
                        return emu_fcmla_vector_by_element_8h_imm180_lane2;
                    case 3:
                        return emu_fcmla_vector_by_element_8h_imm180_lane3;
                    default:
                        return NULL;
                    }
                case ARM64_SIMD_ROTATION_270:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_8h_imm270_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_8h_imm270_lane1;
                    case 2:
                        return emu_fcmla_vector_by_element_8h_imm270_lane2;
                    case 3:
                        return emu_fcmla_vector_by_element_8h_imm270_lane3;
                    default:
                        return NULL;
                    }
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->immediate)
                {
                case ARM64_SIMD_ROTATION_0:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_4s_imm0_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_4s_imm0_lane1;
                    default:
                        return NULL;
                    }
                case ARM64_SIMD_ROTATION_90:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_4s_imm90_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_4s_imm90_lane1;
                    default:
                        return NULL;
                    }
                case ARM64_SIMD_ROTATION_180:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_4s_imm180_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_4s_imm180_lane1;
                    default:
                        return NULL;
                    }
                case ARM64_SIMD_ROTATION_270:
                    switch (decoded->lane_index)
                    {
                    case 0:
                        return emu_fcmla_vector_by_element_4s_imm270_lane0;
                    case 1:
                        return emu_fcmla_vector_by_element_4s_imm270_lane1;
                    default:
                        return NULL;
                    }
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMLAH_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlah_vector_by_element_4h_lane0;
                case 1:
                    return emu_sqrdmlah_vector_by_element_4h_lane1;
                case 2:
                    return emu_sqrdmlah_vector_by_element_4h_lane2;
                case 3:
                    return emu_sqrdmlah_vector_by_element_4h_lane3;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlah_vector_by_element_2s_lane0;
                case 1:
                    return emu_sqrdmlah_vector_by_element_2s_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlah_vector_by_element_8h_lane0;
                case 1:
                    return emu_sqrdmlah_vector_by_element_8h_lane1;
                case 2:
                    return emu_sqrdmlah_vector_by_element_8h_lane2;
                case 3:
                    return emu_sqrdmlah_vector_by_element_8h_lane3;
                case 4:
                    return emu_sqrdmlah_vector_by_element_8h_lane4;
                case 5:
                    return emu_sqrdmlah_vector_by_element_8h_lane5;
                case 6:
                    return emu_sqrdmlah_vector_by_element_8h_lane6;
                case 7:
                    return emu_sqrdmlah_vector_by_element_8h_lane7;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlah_vector_by_element_4s_lane0;
                case 1:
                    return emu_sqrdmlah_vector_by_element_4s_lane1;
                case 2:
                    return emu_sqrdmlah_vector_by_element_4s_lane2;
                case 3:
                    return emu_sqrdmlah_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMLSH_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlsh_vector_by_element_4h_lane0;
                case 1:
                    return emu_sqrdmlsh_vector_by_element_4h_lane1;
                case 2:
                    return emu_sqrdmlsh_vector_by_element_4h_lane2;
                case 3:
                    return emu_sqrdmlsh_vector_by_element_4h_lane3;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlsh_vector_by_element_2s_lane0;
                case 1:
                    return emu_sqrdmlsh_vector_by_element_2s_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlsh_vector_by_element_8h_lane0;
                case 1:
                    return emu_sqrdmlsh_vector_by_element_8h_lane1;
                case 2:
                    return emu_sqrdmlsh_vector_by_element_8h_lane2;
                case 3:
                    return emu_sqrdmlsh_vector_by_element_8h_lane3;
                case 4:
                    return emu_sqrdmlsh_vector_by_element_8h_lane4;
                case 5:
                    return emu_sqrdmlsh_vector_by_element_8h_lane5;
                case 6:
                    return emu_sqrdmlsh_vector_by_element_8h_lane6;
                case 7:
                    return emu_sqrdmlsh_vector_by_element_8h_lane7;
                default:
                    return NULL;
                }
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlsh_vector_by_element_4s_lane0;
                case 1:
                    return emu_sqrdmlsh_vector_by_element_4s_lane1;
                case 2:
                    return emu_sqrdmlsh_vector_by_element_4s_lane2;
                case 3:
                    return emu_sqrdmlsh_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLAL2_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmlal2_vector_by_element_2s_lane0;
                case 1:
                    return emu_fmlal2_vector_by_element_2s_lane1;
                case 2:
                    return emu_fmlal2_vector_by_element_2s_lane2;
                case 3:
                    return emu_fmlal2_vector_by_element_2s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmlal2_vector_by_element_4s_lane0;
                case 1:
                    return emu_fmlal2_vector_by_element_4s_lane1;
                case 2:
                    return emu_fmlal2_vector_by_element_4s_lane2;
                case 3:
                    return emu_fmlal2_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLSL2_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmlsl2_vector_by_element_2s_lane0;
                case 1:
                    return emu_fmlsl2_vector_by_element_2s_lane1;
                case 2:
                    return emu_fmlsl2_vector_by_element_2s_lane2;
                case 3:
                    return emu_fmlsl2_vector_by_element_2s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmlsl2_vector_by_element_4s_lane0;
                case 1:
                    return emu_fmlsl2_vector_by_element_4s_lane1;
                case 2:
                    return emu_fmlsl2_vector_by_element_4s_lane2;
                case 3:
                    return emu_fmlsl2_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UDOT_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 64:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_udot_vector_by_element_2s_lane0;
                case 1:
                    return emu_udot_vector_by_element_2s_lane1;
                case 2:
                    return emu_udot_vector_by_element_2s_lane2;
                case 3:
                    return emu_udot_vector_by_element_2s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_udot_vector_by_element_4s_lane0;
                case 1:
                    return emu_udot_vector_by_element_4s_lane1;
                case 2:
                    return emu_udot_vector_by_element_4s_lane2;
                case 3:
                    return emu_udot_vector_by_element_4s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_INS_GPR_VECTOR:
        switch (decoded->element_width)
        {
        case 8:
            switch (decoded->lane_index)
            {
            case 0:
                return emu_ins_gpr_vector_b_lane0;
            case 1:
                return emu_ins_gpr_vector_b_lane1;
            case 2:
                return emu_ins_gpr_vector_b_lane2;
            case 3:
                return emu_ins_gpr_vector_b_lane3;
            case 4:
                return emu_ins_gpr_vector_b_lane4;
            case 5:
                return emu_ins_gpr_vector_b_lane5;
            case 6:
                return emu_ins_gpr_vector_b_lane6;
            case 7:
                return emu_ins_gpr_vector_b_lane7;
            case 8:
                return emu_ins_gpr_vector_b_lane8;
            case 9:
                return emu_ins_gpr_vector_b_lane9;
            case 10:
                return emu_ins_gpr_vector_b_lane10;
            case 11:
                return emu_ins_gpr_vector_b_lane11;
            case 12:
                return emu_ins_gpr_vector_b_lane12;
            case 13:
                return emu_ins_gpr_vector_b_lane13;
            case 14:
                return emu_ins_gpr_vector_b_lane14;
            case 15:
                return emu_ins_gpr_vector_b_lane15;
            default:
                return NULL;
            }
        case 16:
            switch (decoded->lane_index)
            {
            case 0:
                return emu_ins_gpr_vector_h_lane0;
            case 1:
                return emu_ins_gpr_vector_h_lane1;
            case 2:
                return emu_ins_gpr_vector_h_lane2;
            case 3:
                return emu_ins_gpr_vector_h_lane3;
            case 4:
                return emu_ins_gpr_vector_h_lane4;
            case 5:
                return emu_ins_gpr_vector_h_lane5;
            case 6:
                return emu_ins_gpr_vector_h_lane6;
            case 7:
                return emu_ins_gpr_vector_h_lane7;
            default:
                return NULL;
            }
        case 32:
            switch (decoded->lane_index)
            {
            case 0:
                return emu_ins_gpr_vector_s_lane0;
            case 1:
                return emu_ins_gpr_vector_s_lane1;
            case 2:
                return emu_ins_gpr_vector_s_lane2;
            case 3:
                return emu_ins_gpr_vector_s_lane3;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->lane_index)
            {
            case 0:
                return emu_ins_gpr_vector_d_lane0;
            case 1:
                return emu_ins_gpr_vector_d_lane1;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_XTN2_VECTOR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_xtn2_vector_h;
        case 32:
            return emu_xtn2_vector_s;
        case 64:
            return emu_xtn2_vector_d;
        default:
            return NULL;
        }
    case ARM64_INST_SQXTN2_VECTOR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_sqxtn2_vector_h;
        case 32:
            return emu_sqxtn2_vector_s;
        case 64:
            return emu_sqxtn2_vector_d;
        default:
            return NULL;
        }
    case ARM64_INST_SMMLA_VECTOR:
        switch (decoded->operand_width)
        {
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_smmla_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_USMMLA_VECTOR:
        switch (decoded->operand_width)
        {
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_usmmla_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BFMLALT_VECTOR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_bfmlalt_vector_by_element_8h_lane0;
                case 1:
                    return emu_bfmlalt_vector_by_element_8h_lane1;
                case 2:
                    return emu_bfmlalt_vector_by_element_8h_lane2;
                case 3:
                    return emu_bfmlalt_vector_by_element_8h_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_DUP_ELEMENT_SCALAR:
        switch (decoded->operand_width)
        {
        case 8:
            switch (decoded->element_width)
            {
            case 8:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_scalar_b_lane0;
                case 1:
                    return emu_dup_element_scalar_b_lane1;
                case 2:
                    return emu_dup_element_scalar_b_lane2;
                case 3:
                    return emu_dup_element_scalar_b_lane3;
                case 4:
                    return emu_dup_element_scalar_b_lane4;
                case 5:
                    return emu_dup_element_scalar_b_lane5;
                case 6:
                    return emu_dup_element_scalar_b_lane6;
                case 7:
                    return emu_dup_element_scalar_b_lane7;
                case 8:
                    return emu_dup_element_scalar_b_lane8;
                case 9:
                    return emu_dup_element_scalar_b_lane9;
                case 10:
                    return emu_dup_element_scalar_b_lane10;
                case 11:
                    return emu_dup_element_scalar_b_lane11;
                case 12:
                    return emu_dup_element_scalar_b_lane12;
                case 13:
                    return emu_dup_element_scalar_b_lane13;
                case 14:
                    return emu_dup_element_scalar_b_lane14;
                case 15:
                    return emu_dup_element_scalar_b_lane15;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_scalar_h_lane0;
                case 1:
                    return emu_dup_element_scalar_h_lane1;
                case 2:
                    return emu_dup_element_scalar_h_lane2;
                case 3:
                    return emu_dup_element_scalar_h_lane3;
                case 4:
                    return emu_dup_element_scalar_h_lane4;
                case 5:
                    return emu_dup_element_scalar_h_lane5;
                case 6:
                    return emu_dup_element_scalar_h_lane6;
                case 7:
                    return emu_dup_element_scalar_h_lane7;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_scalar_s_lane0;
                case 1:
                    return emu_dup_element_scalar_s_lane1;
                case 2:
                    return emu_dup_element_scalar_s_lane2;
                case 3:
                    return emu_dup_element_scalar_s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_dup_element_scalar_d_lane0;
                case 1:
                    return emu_dup_element_scalar_d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQADD_SCALAR:
        switch (decoded->element_width)
        {
        case 8:
            return emu_sqadd_scalar_b;
        case 16:
            return emu_sqadd_scalar_h;
        case 32:
            return emu_sqadd_scalar_s;
        case 64:
            return emu_sqadd_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_SQSUB_SCALAR:
        switch (decoded->element_width)
        {
        case 8:
            return emu_sqsub_scalar_b;
        case 16:
            return emu_sqsub_scalar_h;
        case 32:
            return emu_sqsub_scalar_s;
        case 64:
            return emu_sqsub_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_SQSHL_SCALAR:
        switch (decoded->element_width)
        {
        case 8:
            return emu_sqshl_scalar_b;
        case 16:
            return emu_sqshl_scalar_h;
        case 32:
            return emu_sqshl_scalar_s;
        case 64:
            return emu_sqshl_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_SQRSHL_SCALAR:
        switch (decoded->element_width)
        {
        case 8:
            return emu_sqrshl_scalar_b;
        case 16:
            return emu_sqrshl_scalar_h;
        case 32:
            return emu_sqrshl_scalar_s;
        case 64:
            return emu_sqrshl_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMULX_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_fmulx_scalar_h;
        case 32:
            return emu_fmulx_scalar_s;
        case 64:
            return emu_fmulx_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCMEQ_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_fcmeq_scalar_h;
        case 32:
            return emu_fcmeq_scalar_s;
        case 64:
            return emu_fcmeq_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FRECPS_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_frecps_scalar_h;
        case 32:
            return emu_frecps_scalar_s;
        case 64:
            return emu_frecps_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_SQXTN_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_sqxtn_scalar_h;
        case 32:
            return emu_sqxtn_scalar_s;
        case 64:
            return emu_sqxtn_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCVTNS_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtns_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtns_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTMS_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtms_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtms_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTAS_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtas_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtas_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SCVTF_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_scvtf_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_scvtf_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FADDP_SCALAR_REDUCE:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 16:
                return emu_faddp_scalar_reduce_h_2h;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 32:
                return emu_faddp_scalar_reduce_s_2s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQDMULH_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_sqdmulh_scalar_h;
        case 32:
            return emu_sqdmulh_scalar_s;
        default:
            return NULL;
        }
    case ARM64_INST_FCMGT_ZERO_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmgt_zero_scalar_h;
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcmgt_zero_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcmgt_zero_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMEQ_ZERO_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmeq_zero_scalar_h;
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcmeq_zero_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcmeq_zero_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMLT_ZERO_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmlt_zero_scalar_h;
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcmlt_zero_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcmlt_zero_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FRSQRTS_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_frsqrts_scalar_h;
        case 32:
            return emu_frsqrts_scalar_s;
        case 64:
            return emu_frsqrts_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCVTPS_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtps_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtps_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTZS_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzs_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtzs_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CMGT_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_cmgt_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_CMGE_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_cmge_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_SSHL_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_sshl_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_SRSHL_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_srshl_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_ADD_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_add_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_CMTST_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_cmtst_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMLA_SCALAR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmla_scalar_by_element_h_lane0;
                case 1:
                    return emu_fmla_scalar_by_element_h_lane1;
                case 2:
                    return emu_fmla_scalar_by_element_h_lane2;
                case 3:
                    return emu_fmla_scalar_by_element_h_lane3;
                case 4:
                    return emu_fmla_scalar_by_element_h_lane4;
                case 5:
                    return emu_fmla_scalar_by_element_h_lane5;
                case 6:
                    return emu_fmla_scalar_by_element_h_lane6;
                case 7:
                    return emu_fmla_scalar_by_element_h_lane7;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmla_scalar_by_element_s_lane0;
                case 1:
                    return emu_fmla_scalar_by_element_s_lane1;
                case 2:
                    return emu_fmla_scalar_by_element_s_lane2;
                case 3:
                    return emu_fmla_scalar_by_element_s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmla_scalar_by_element_d_lane0;
                case 1:
                    return emu_fmla_scalar_by_element_d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMLS_SCALAR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmls_scalar_by_element_h_lane0;
                case 1:
                    return emu_fmls_scalar_by_element_h_lane1;
                case 2:
                    return emu_fmls_scalar_by_element_h_lane2;
                case 3:
                    return emu_fmls_scalar_by_element_h_lane3;
                case 4:
                    return emu_fmls_scalar_by_element_h_lane4;
                case 5:
                    return emu_fmls_scalar_by_element_h_lane5;
                case 6:
                    return emu_fmls_scalar_by_element_h_lane6;
                case 7:
                    return emu_fmls_scalar_by_element_h_lane7;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmls_scalar_by_element_s_lane0;
                case 1:
                    return emu_fmls_scalar_by_element_s_lane1;
                case 2:
                    return emu_fmls_scalar_by_element_s_lane2;
                case 3:
                    return emu_fmls_scalar_by_element_s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmls_scalar_by_element_d_lane0;
                case 1:
                    return emu_fmls_scalar_by_element_d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FMUL_SCALAR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmul_scalar_by_element_h_lane0;
                case 1:
                    return emu_fmul_scalar_by_element_h_lane1;
                case 2:
                    return emu_fmul_scalar_by_element_h_lane2;
                case 3:
                    return emu_fmul_scalar_by_element_h_lane3;
                case 4:
                    return emu_fmul_scalar_by_element_h_lane4;
                case 5:
                    return emu_fmul_scalar_by_element_h_lane5;
                case 6:
                    return emu_fmul_scalar_by_element_h_lane6;
                case 7:
                    return emu_fmul_scalar_by_element_h_lane7;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmul_scalar_by_element_s_lane0;
                case 1:
                    return emu_fmul_scalar_by_element_s_lane1;
                case 2:
                    return emu_fmul_scalar_by_element_s_lane2;
                case 3:
                    return emu_fmul_scalar_by_element_s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmul_scalar_by_element_d_lane0;
                case 1:
                    return emu_fmul_scalar_by_element_d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_INS_ELEMENT_VECTOR:
        switch (decoded->element_width)
        {
        case 8:
            return emu_ins_element_vector_b;
        case 16:
            return emu_ins_element_vector_h;
        case 32:
            return emu_ins_element_vector_s;
        case 64:
            return emu_ins_element_vector_d;
        default:
            return NULL;
        }
    case ARM64_INST_SQXTUN2_VECTOR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_sqxtun2_vector_h;
        case 32:
            return emu_sqxtun2_vector_s;
        case 64:
            return emu_sqxtun2_vector_d;
        default:
            return NULL;
        }
    case ARM64_INST_UQXTN2_VECTOR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_uqxtn2_vector_h;
        case 32:
            return emu_uqxtn2_vector_s;
        case 64:
            return emu_uqxtn2_vector_d;
        default:
            return NULL;
        }
    case ARM64_INST_BFMMLA_VECTOR:
        switch (decoded->operand_width)
        {
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_bfmmla_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UMMLA_VECTOR:
        switch (decoded->operand_width)
        {
        case 128:
            switch (decoded->element_width)
            {
            case 8:
                return emu_ummla_vector_4s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_BFMLALT_VECTOR:
        switch (decoded->operand_width)
        {
        case 128:
            switch (decoded->element_width)
            {
            case 16:
                return emu_bfmlalt_vector_8h;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UQADD_SCALAR:
        switch (decoded->element_width)
        {
        case 8:
            return emu_uqadd_scalar_b;
        case 16:
            return emu_uqadd_scalar_h;
        case 32:
            return emu_uqadd_scalar_s;
        case 64:
            return emu_uqadd_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_UQSUB_SCALAR:
        switch (decoded->element_width)
        {
        case 8:
            return emu_uqsub_scalar_b;
        case 16:
            return emu_uqsub_scalar_h;
        case 32:
            return emu_uqsub_scalar_s;
        case 64:
            return emu_uqsub_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_UQSHL_SCALAR:
        switch (decoded->element_width)
        {
        case 8:
            return emu_uqshl_scalar_b;
        case 16:
            return emu_uqshl_scalar_h;
        case 32:
            return emu_uqshl_scalar_s;
        case 64:
            return emu_uqshl_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_UQRSHL_SCALAR:
        switch (decoded->element_width)
        {
        case 8:
            return emu_uqrshl_scalar_b;
        case 16:
            return emu_uqrshl_scalar_h;
        case 32:
            return emu_uqrshl_scalar_s;
        case 64:
            return emu_uqrshl_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCMGE_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_fcmge_scalar_h;
        case 32:
            return emu_fcmge_scalar_s;
        case 64:
            return emu_fcmge_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FACGE_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_facge_scalar_h;
        case 32:
            return emu_facge_scalar_s;
        case 64:
            return emu_facge_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_SQXTUN_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_sqxtun_scalar_h;
        case 32:
            return emu_sqxtun_scalar_s;
        case 64:
            return emu_sqxtun_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_UQXTN_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_uqxtn_scalar_h;
        case 32:
            return emu_uqxtn_scalar_s;
        case 64:
            return emu_uqxtn_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCVTNU_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtnu_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtnu_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTMU_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtmu_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtmu_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTAU_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtau_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtau_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_UCVTF_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_ucvtf_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_ucvtf_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMLAH_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqrdmlah_scalar_h;
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_sqrdmlah_scalar_s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMLSH_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                return emu_sqrdmlsh_scalar_h;
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_sqrdmlsh_scalar_s;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMULH_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_sqrdmulh_scalar_h;
        case 32:
            return emu_sqrdmulh_scalar_s;
        default:
            return NULL;
        }
    case ARM64_INST_FCMGE_ZERO_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmge_zero_scalar_h;
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcmge_zero_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcmge_zero_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FABD_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_fabd_scalar_h;
        case 32:
            return emu_fabd_scalar_s;
        case 64:
            return emu_fabd_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCMLE_ZERO_SCALAR:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                return emu_fcmle_zero_scalar_h;
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcmle_zero_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcmle_zero_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCMGT_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_fcmgt_scalar_h;
        case 32:
            return emu_fcmgt_scalar_s;
        case 64:
            return emu_fcmgt_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FACGT_SCALAR:
        switch (decoded->element_width)
        {
        case 16:
            return emu_facgt_scalar_h;
        case 32:
            return emu_facgt_scalar_s;
        case 64:
            return emu_facgt_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FCVTPU_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtpu_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtpu_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_FCVTZU_SIMD_SCALAR:
        switch (decoded->operand_width)
        {
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                return emu_fcvtzu_simd_scalar_s;
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                return emu_fcvtzu_simd_scalar_d;
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_CMHI_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_cmhi_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_CMHS_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_cmhs_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_USHL_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_ushl_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_URSHL_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_urshl_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_SUB_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_sub_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_CMEQ_SCALAR:
        switch (decoded->element_width)
        {
        case 64:
            return emu_cmeq_scalar_d;
        default:
            return NULL;
        }
    case ARM64_INST_FMULX_SCALAR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmulx_scalar_by_element_h_lane0;
                case 1:
                    return emu_fmulx_scalar_by_element_h_lane1;
                case 2:
                    return emu_fmulx_scalar_by_element_h_lane2;
                case 3:
                    return emu_fmulx_scalar_by_element_h_lane3;
                case 4:
                    return emu_fmulx_scalar_by_element_h_lane4;
                case 5:
                    return emu_fmulx_scalar_by_element_h_lane5;
                case 6:
                    return emu_fmulx_scalar_by_element_h_lane6;
                case 7:
                    return emu_fmulx_scalar_by_element_h_lane7;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmulx_scalar_by_element_s_lane0;
                case 1:
                    return emu_fmulx_scalar_by_element_s_lane1;
                case 2:
                    return emu_fmulx_scalar_by_element_s_lane2;
                case 3:
                    return emu_fmulx_scalar_by_element_s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 64:
            switch (decoded->element_width)
            {
            case 64:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_fmulx_scalar_by_element_d_lane0;
                case 1:
                    return emu_fmulx_scalar_by_element_d_lane1;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMLAH_SCALAR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlah_scalar_by_element_h_lane0;
                case 1:
                    return emu_sqrdmlah_scalar_by_element_h_lane1;
                case 2:
                    return emu_sqrdmlah_scalar_by_element_h_lane2;
                case 3:
                    return emu_sqrdmlah_scalar_by_element_h_lane3;
                case 4:
                    return emu_sqrdmlah_scalar_by_element_h_lane4;
                case 5:
                    return emu_sqrdmlah_scalar_by_element_h_lane5;
                case 6:
                    return emu_sqrdmlah_scalar_by_element_h_lane6;
                case 7:
                    return emu_sqrdmlah_scalar_by_element_h_lane7;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlah_scalar_by_element_s_lane0;
                case 1:
                    return emu_sqrdmlah_scalar_by_element_s_lane1;
                case 2:
                    return emu_sqrdmlah_scalar_by_element_s_lane2;
                case 3:
                    return emu_sqrdmlah_scalar_by_element_s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SQRDMLSH_SCALAR_BY_ELEMENT:
        switch (decoded->operand_width)
        {
        case 16:
            switch (decoded->element_width)
            {
            case 16:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlsh_scalar_by_element_h_lane0;
                case 1:
                    return emu_sqrdmlsh_scalar_by_element_h_lane1;
                case 2:
                    return emu_sqrdmlsh_scalar_by_element_h_lane2;
                case 3:
                    return emu_sqrdmlsh_scalar_by_element_h_lane3;
                case 4:
                    return emu_sqrdmlsh_scalar_by_element_h_lane4;
                case 5:
                    return emu_sqrdmlsh_scalar_by_element_h_lane5;
                case 6:
                    return emu_sqrdmlsh_scalar_by_element_h_lane6;
                case 7:
                    return emu_sqrdmlsh_scalar_by_element_h_lane7;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        case 32:
            switch (decoded->element_width)
            {
            case 32:
                switch (decoded->lane_index)
                {
                case 0:
                    return emu_sqrdmlsh_scalar_by_element_s_lane0;
                case 1:
                    return emu_sqrdmlsh_scalar_by_element_s_lane1;
                case 2:
                    return emu_sqrdmlsh_scalar_by_element_s_lane2;
                case 3:
                    return emu_sqrdmlsh_scalar_by_element_s_lane3;
                default:
                    return NULL;
                }
            default:
                return NULL;
            }
        default:
            return NULL;
        }
    case ARM64_INST_SCVTF_S_X:
        return emu_scvtf_s_x;
    case ARM64_INST_UCVTF_S_X:
        return emu_ucvtf_s_x;
    case ARM64_INST_SCVTF_D_X:
        return emu_scvtf_d_x;
    case ARM64_INST_UCVTF_D_X:
        return emu_ucvtf_d_x;
    default:
        return NULL;
    }
}
