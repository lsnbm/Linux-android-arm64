#ifndef ARM64_ENCODE_H
#define ARM64_ENCODE_H

#ifdef __KERNEL__
#include <linux/stddef.h>
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#endif

enum arm64_encode_shift
{
    ARM64_ENCODE_SHIFT_LSL,
    ARM64_ENCODE_SHIFT_LSR,
    ARM64_ENCODE_SHIFT_ASR,
};

enum arm64_encode_load_store_kind
{
    ARM64_ENCODE_LS_GPR32,
    ARM64_ENCODE_LS_GPR64,
    ARM64_ENCODE_LS_LDRSW,
    ARM64_ENCODE_LS_FP32,
    ARM64_ENCODE_LS_FP64,
    ARM64_ENCODE_LS_FP128,
};

int arm64_encode_nop(uint32_t *instruction);
int arm64_encode_bti_c(uint32_t *instruction);
int arm64_encode_brk(uint32_t immediate, uint32_t *instruction);
int arm64_encode_b(uint64_t pc, uint64_t target, uint32_t *instruction);
int arm64_encode_bl(uint64_t pc, uint64_t target, uint32_t *instruction);
int arm64_encode_b_cond(uint64_t pc, uint64_t target, uint8_t condition, uint32_t *instruction);
int arm64_encode_cbz(uint64_t pc, uint64_t target, bool nonzero, bool is_64bit, uint8_t rt, uint32_t *instruction);
int arm64_encode_tbz(uint64_t pc, uint64_t target, bool nonzero, uint8_t test_bit, uint8_t rt, uint32_t *instruction);
int arm64_encode_br(uint8_t rn, uint32_t *instruction);
int arm64_encode_blr(uint8_t rn, uint32_t *instruction);
int arm64_encode_ret(uint8_t rn, uint32_t *instruction);
int arm64_encode_adr(uint8_t rd, uint64_t pc, uint64_t target, uint32_t *instruction);
int arm64_encode_adrp(uint8_t rd, uint64_t pc, uint64_t target, uint32_t *instruction);
int arm64_encode_add_sub_imm(bool is_64bit, bool subtract, bool set_flags, uint8_t rd, uint8_t rn, uint64_t immediate, uint32_t *instruction);
int arm64_encode_add_sub_shifted(bool is_64bit, bool subtract, bool set_flags, uint8_t rd, uint8_t rn, uint8_t rm, enum arm64_encode_shift shift, uint8_t shift_amount, uint32_t *instruction);
int arm64_encode_load_store_unsigned(bool load, enum arm64_encode_load_store_kind kind, uint8_t rt, uint8_t rn, uint32_t byte_offset, uint32_t *instruction);
int arm64_encode_load_store_x_indexed(bool load, bool pre_index, uint8_t rt, uint8_t rn, int16_t byte_offset, uint32_t *instruction);
int arm64_encode_load_store_pair_x(bool load, uint8_t rt, uint8_t rt2, uint8_t rn, int16_t byte_offset, uint32_t *instruction);
int arm64_encode_ldr_literal(enum arm64_encode_load_store_kind kind, uint8_t rt, uint64_t pc, uint64_t target, uint32_t *instruction);
int arm64_encode_prfm_literal(uint8_t operation, uint64_t pc, uint64_t target, uint32_t *instruction);
int arm64_encode_mrs(uint8_t rt, uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2, uint32_t *instruction);
int arm64_encode_msr(uint8_t rt, uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2, uint32_t *instruction);
int arm64_encode_fill_nops(uint32_t *output, size_t word_count);
int arm64_emit_abs_jump(uint64_t target, uint8_t scratch, uint32_t *output, size_t word_count);

#endif