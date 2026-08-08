#include "arm64_encode.h"

#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/string.h>
#else
#include <errno.h>
#include <string.h>
#endif

#define ARM64_NOP 0xD503201FU

static bool arm64_encode_valid_reg(uint8_t reg)
{
    return reg <= 31;
}

static int arm64_encode_signed_delta(uint64_t from, uint64_t to, int64_t *delta)
{
    if (!delta) return -EINVAL;

    if (to >= from)
    {
        uint64_t magnitude = to - from;
        if (magnitude > 0x7FFFFFFFFFFFFFFFULL) return -ERANGE;
        *delta = (int64_t)magnitude;
    }
    else
    {
        uint64_t magnitude = from - to;
        if (magnitude > 0x8000000000000000ULL) return -ERANGE;
        *delta = magnitude == 0x8000000000000000ULL ? (int64_t)0x8000000000000000ULL : -(int64_t)magnitude;
    }

    return 0;
}

static int arm64_encode_pc_delta(uint64_t pc, uint64_t target, int64_t *delta)
{
    if ((pc | target) & 0x3U) return -EINVAL;
    return arm64_encode_signed_delta(pc, target, delta);
}

static int arm64_encode_scaled_imm(uint32_t base, int64_t byte_offset, uint8_t imm_bits, uint8_t imm_shift, uint32_t *instruction)
{
    if (!instruction || (byte_offset & 0x3) || !imm_bits || imm_bits > 31 || imm_shift + imm_bits > 32) return -EINVAL;

    int64_t immediate = byte_offset / 4;
    int64_t limit = 1LL << (imm_bits - 1);
    if (immediate < -limit || immediate >= limit) return -ERANGE;

    uint32_t mask = (uint32_t)((1ULL << imm_bits) - 1);
    *instruction = base | (((uint32_t)immediate & mask) << imm_shift);
    return 0;
}

static int arm64_encode_direct_branch(uint32_t base, uint64_t pc, uint64_t target, uint32_t *instruction)
{
    int64_t offset;
    int status = arm64_encode_pc_delta(pc, target, &offset);
    if (status) return status;
    return arm64_encode_scaled_imm(base, offset, 26, 0, instruction);
}

static int arm64_encode_register_branch(uint32_t base, uint8_t rn, uint32_t *instruction)
{
    if (!instruction || !arm64_encode_valid_reg(rn)) return -EINVAL;
    *instruction = base | ((uint32_t)rn << 5);
    return 0;
}

static int arm64_encode_pc_address(uint32_t base, uint8_t rd, int64_t immediate, uint32_t *instruction)
{
    if (!instruction || !arm64_encode_valid_reg(rd)) return -EINVAL;

    int64_t limit = 1LL << 20;
    if (immediate < -limit || immediate >= limit) return -ERANGE;

    uint32_t encoded = (uint32_t)immediate & 0x1FFFFFU;
    *instruction = base | ((encoded & 0x3U) << 29) | ((encoded >> 2) << 5) | rd;
    return 0;
}

static int arm64_encode_literal_base(enum arm64_encode_load_store_kind kind, uint32_t *base, uint8_t *access_bytes)
{
    if (!base || !access_bytes) return -EINVAL;

    switch (kind)
    {
    case ARM64_ENCODE_LS_GPR32:
        *base = 0x18000000U;
        *access_bytes = 4;
        return 0;
    case ARM64_ENCODE_LS_GPR64:
        *base = 0x58000000U;
        *access_bytes = 8;
        return 0;
    case ARM64_ENCODE_LS_LDRSW:
        *base = 0x98000000U;
        *access_bytes = 4;
        return 0;
    case ARM64_ENCODE_LS_FP32:
        *base = 0x1C000000U;
        *access_bytes = 4;
        return 0;
    case ARM64_ENCODE_LS_FP64:
        *base = 0x5C000000U;
        *access_bytes = 8;
        return 0;
    case ARM64_ENCODE_LS_FP128:
        *base = 0x9C000000U;
        *access_bytes = 16;
        return 0;
    default:
        return -EINVAL;
    }
}

static int arm64_encode_unsigned_base(bool load, enum arm64_encode_load_store_kind kind, uint32_t *base, uint8_t *access_bytes)
{
    if (!base || !access_bytes) return -EINVAL;

    switch (kind)
    {
    case ARM64_ENCODE_LS_GPR32:
        *base = load ? 0xB9400000U : 0xB9000000U;
        *access_bytes = 4;
        return 0;
    case ARM64_ENCODE_LS_GPR64:
        *base = load ? 0xF9400000U : 0xF9000000U;
        *access_bytes = 8;
        return 0;
    case ARM64_ENCODE_LS_LDRSW:
        if (!load) return -EINVAL;
        *base = 0xB9800000U;
        *access_bytes = 4;
        return 0;
    case ARM64_ENCODE_LS_FP32:
        *base = load ? 0xBD400000U : 0xBD000000U;
        *access_bytes = 4;
        return 0;
    case ARM64_ENCODE_LS_FP64:
        *base = load ? 0xFD400000U : 0xFD000000U;
        *access_bytes = 8;
        return 0;
    case ARM64_ENCODE_LS_FP128:
        *base = load ? 0x3DC00000U : 0x3D800000U;
        *access_bytes = 16;
        return 0;
    default:
        return -EINVAL;
    }
}

static int arm64_encode_system_register(uint32_t base, uint8_t rt, uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2, uint32_t *instruction)
{
    if (!instruction || !arm64_encode_valid_reg(rt) || op0 > 3 || op1 > 7 || crn > 15 || crm > 15 || op2 > 7) return -EINVAL;

    *instruction = base | ((uint32_t)op0 << 19) | ((uint32_t)op1 << 16) | ((uint32_t)crn << 12) | ((uint32_t)crm << 8) | ((uint32_t)op2 << 5) | rt;
    return 0;
}

int arm64_encode_nop(uint32_t *instruction)
{
    if (!instruction) return -EINVAL;
    *instruction = ARM64_NOP;
    return 0;
}

int arm64_encode_b(uint64_t pc, uint64_t target, uint32_t *instruction)
{
    return arm64_encode_direct_branch(0x14000000U, pc, target, instruction);
}

int arm64_encode_bl(uint64_t pc, uint64_t target, uint32_t *instruction)
{
    return arm64_encode_direct_branch(0x94000000U, pc, target, instruction);
}

int arm64_encode_b_cond(uint64_t pc, uint64_t target, uint8_t condition, uint32_t *instruction)
{
    if (condition > 15) return -EINVAL;

    int64_t offset;
    int status = arm64_encode_pc_delta(pc, target, &offset);
    if (status) return status;
    return arm64_encode_scaled_imm(0x54000000U | condition, offset, 19, 5, instruction);
}

int arm64_encode_cbz(uint64_t pc, uint64_t target, bool nonzero, bool is_64bit, uint8_t rt, uint32_t *instruction)
{
    if (!arm64_encode_valid_reg(rt)) return -EINVAL;

    int64_t offset;
    int status = arm64_encode_pc_delta(pc, target, &offset);
    if (status) return status;

    uint32_t base = 0x34000000U | (nonzero ? 0x01000000U : 0) | (is_64bit ? 0x80000000U : 0) | rt;
    return arm64_encode_scaled_imm(base, offset, 19, 5, instruction);
}

int arm64_encode_tbz(uint64_t pc, uint64_t target, bool nonzero, uint8_t test_bit, uint8_t rt, uint32_t *instruction)
{
    if (!arm64_encode_valid_reg(rt) || test_bit > 63) return -EINVAL;

    int64_t offset;
    int status = arm64_encode_pc_delta(pc, target, &offset);
    if (status) return status;

    uint32_t base = 0x36000000U | (nonzero ? 0x01000000U : 0) | ((uint32_t)(test_bit & 0x20U) << 26) | ((uint32_t)(test_bit & 0x1FU) << 19) | rt;
    return arm64_encode_scaled_imm(base, offset, 14, 5, instruction);
}

int arm64_encode_br(uint8_t rn, uint32_t *instruction)
{
    return arm64_encode_register_branch(0xD61F0000U, rn, instruction);
}

int arm64_encode_blr(uint8_t rn, uint32_t *instruction)
{
    return arm64_encode_register_branch(0xD63F0000U, rn, instruction);
}

int arm64_encode_ret(uint8_t rn, uint32_t *instruction)
{
    return arm64_encode_register_branch(0xD65F0000U, rn, instruction);
}

int arm64_encode_adr(uint8_t rd, uint64_t pc, uint64_t target, uint32_t *instruction)
{
    int64_t immediate;
    int status = arm64_encode_signed_delta(pc, target, &immediate);
    if (status) return status;
    return arm64_encode_pc_address(0x10000000U, rd, immediate, instruction);
}

int arm64_encode_adrp(uint8_t rd, uint64_t pc, uint64_t target, uint32_t *instruction)
{
    int64_t byte_offset;
    int status = arm64_encode_signed_delta(pc & ~0xFFFULL, target & ~0xFFFULL, &byte_offset);
    if (status) return status;
    return arm64_encode_pc_address(0x90000000U, rd, byte_offset / 0x1000, instruction);
}

int arm64_encode_add_sub_imm(bool is_64bit, bool subtract, bool set_flags, uint8_t rd, uint8_t rn, uint64_t immediate, uint32_t *instruction)
{
    if (!instruction || !arm64_encode_valid_reg(rd) || !arm64_encode_valid_reg(rn)) return -EINVAL;

    uint32_t shift = 0;
    uint64_t encoded_immediate = immediate;
    if (encoded_immediate > 0xFFF)
    {
        if (encoded_immediate & 0xFFF) return -ERANGE;
        encoded_immediate >>= 12;
        shift = 1;
    }
    if (encoded_immediate > 0xFFF) return -ERANGE;

    *instruction = 0x11000000U | (is_64bit ? 1U << 31 : 0) | (subtract ? 1U << 30 : 0) | (set_flags ? 1U << 29 : 0) | (shift << 22) | ((uint32_t)encoded_immediate << 10) | ((uint32_t)rn << 5) | rd;
    return 0;
}

int arm64_encode_add_sub_shifted(bool is_64bit, bool subtract, bool set_flags, uint8_t rd, uint8_t rn, uint8_t rm, enum arm64_encode_shift shift, uint8_t shift_amount, uint32_t *instruction)
{
    if (!instruction || !arm64_encode_valid_reg(rd) || !arm64_encode_valid_reg(rn) || !arm64_encode_valid_reg(rm)) return -EINVAL;
    if (shift > ARM64_ENCODE_SHIFT_ASR || shift_amount >= (is_64bit ? 64 : 32)) return -EINVAL;

    *instruction = 0x0B000000U | (is_64bit ? 1U << 31 : 0) | (subtract ? 1U << 30 : 0) | (set_flags ? 1U << 29 : 0) | ((uint32_t)shift << 22) | ((uint32_t)rm << 16) | ((uint32_t)shift_amount << 10) | ((uint32_t)rn << 5) | rd;
    return 0;
}

int arm64_encode_load_store_unsigned(bool load, enum arm64_encode_load_store_kind kind, uint8_t rt, uint8_t rn, uint32_t byte_offset, uint32_t *instruction)
{
    if (!instruction || !arm64_encode_valid_reg(rt) || !arm64_encode_valid_reg(rn)) return -EINVAL;

    uint32_t base;
    uint8_t access_bytes;
    int status = arm64_encode_unsigned_base(load, kind, &base, &access_bytes);
    if (status) return status;
    if (byte_offset % access_bytes) return -EINVAL;

    uint32_t immediate = byte_offset / access_bytes;
    if (immediate > 0xFFF) return -ERANGE;

    *instruction = base | (immediate << 10) | ((uint32_t)rn << 5) | rt;
    return 0;
}

int arm64_encode_load_store_x_indexed(bool load, bool pre_index, uint8_t rt, uint8_t rn, int16_t byte_offset, uint32_t *instruction)
{
    if (!instruction || !arm64_encode_valid_reg(rt) || !arm64_encode_valid_reg(rn)) return -EINVAL;
    if (byte_offset < -256 || byte_offset > 255) return -ERANGE;

    uint32_t base = load ? 0xF8400000U : 0xF8000000U;
    base |= pre_index ? 0x00000C00U : 0x00000400U;
    *instruction = base | (((uint32_t)byte_offset & 0x1FFU) << 12) | ((uint32_t)rn << 5) | rt;
    return 0;
}

int arm64_encode_load_store_pair_x(bool load, uint8_t rt, uint8_t rt2, uint8_t rn, int16_t byte_offset, uint32_t *instruction)
{
    if (!instruction || !arm64_encode_valid_reg(rt) || !arm64_encode_valid_reg(rt2) || !arm64_encode_valid_reg(rn)) return -EINVAL;
    if (byte_offset & 0x7) return -EINVAL;

    int16_t immediate = byte_offset / 8;
    if (immediate < -64 || immediate > 63) return -ERANGE;

    *instruction = (load ? 0xA9400000U : 0xA9000000U) | (((uint32_t)immediate & 0x7FU) << 15) | ((uint32_t)rt2 << 10) | ((uint32_t)rn << 5) | rt;
    return 0;
}

int arm64_encode_ldr_literal(enum arm64_encode_load_store_kind kind, uint8_t rt, uint64_t pc, uint64_t target, uint32_t *instruction)
{
    if (!arm64_encode_valid_reg(rt)) return -EINVAL;

    uint32_t base;
    uint8_t access_bytes;
    int status = arm64_encode_literal_base(kind, &base, &access_bytes);
    if (status) return status;
    (void)access_bytes;

    int64_t offset;
    status = arm64_encode_pc_delta(pc, target, &offset);
    if (status) return status;
    return arm64_encode_scaled_imm(base | rt, offset, 19, 5, instruction);
}

int arm64_encode_prfm_literal(uint8_t operation, uint64_t pc, uint64_t target, uint32_t *instruction)
{
    if (operation > 31) return -EINVAL;

    int64_t offset;
    int status = arm64_encode_pc_delta(pc, target, &offset);
    if (status) return status;
    return arm64_encode_scaled_imm(0xD8000000U | operation, offset, 19, 5, instruction);
}

int arm64_encode_mrs(uint8_t rt, uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2, uint32_t *instruction)
{
    return arm64_encode_system_register(0xD5300000U, rt, op0, op1, crn, crm, op2, instruction);
}

int arm64_encode_msr(uint8_t rt, uint8_t op0, uint8_t op1, uint8_t crn, uint8_t crm, uint8_t op2, uint32_t *instruction)
{
    return arm64_encode_system_register(0xD5100000U, rt, op0, op1, crn, crm, op2, instruction);
}

int arm64_encode_fill_nops(uint32_t *output, size_t word_count)
{
    if (!output && word_count) return -EINVAL;
    for (size_t index = 0; index < word_count; index++) output[index] = ARM64_NOP;
    return 0;
}

int arm64_emit_abs_jump(uint64_t target, uint8_t scratch, uint32_t *output, size_t word_count)
{
    if (!output || word_count < 4 || !arm64_encode_valid_reg(scratch)) return -EINVAL;

    int status = arm64_encode_fill_nops(output, word_count);
    if (status) return status;
    status = arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, scratch, 0, 2 * sizeof(uint32_t), &output[0]);
    if (status) return status;
    status = arm64_encode_ret(scratch, &output[1]);
    if (status) return status;
    memcpy(&output[2], &target, sizeof(target));
    return 0;
}