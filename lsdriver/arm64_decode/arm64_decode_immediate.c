#include "arm64_decode.h"

/* ======================== 位掩码立即数 ======================== */

static inline uint64_t arm64_decode_immediate_low_mask(uint8_t bits)
{
    if (bits >= 64)
    {
        return ~0ULL;
    }
    return (1ULL << bits) - 1;
}

static inline uint64_t arm64_decode_immediate_ror_element(uint64_t value, uint8_t rotation, uint8_t width)
{
    uint64_t mask = arm64_decode_immediate_low_mask(width);

    rotation %= width;
    value &= mask;
    if (!rotation)
    {
        return value;
    }
    return ((value >> rotation) | (value << (width - rotation))) & mask;
}

static inline uint64_t arm64_decode_immediate_replicate(uint64_t value, uint8_t element_width, uint8_t width)
{
    uint64_t result = 0;

    value &= arm64_decode_immediate_low_mask(element_width);
    for (uint8_t offset = 0; offset < width; offset += element_width)
    {
        result |= value << offset;
    }
    return result;
}

static inline int arm64_decode_immediate_bit_masks(uint8_t n, uint8_t immr, uint8_t imms, uint8_t width, int immediate, uint64_t *wmask, uint64_t *tmask)
{
    /* 按 ARM ARM DecodeBitMasks 规则校验编码并展开 WMask/TMask。 */
    uint32_t value = ((uint32_t)n << 6) | (~imms & 0x3F);

    if (!value)
    {
        return 0;
    }
    uint8_t len = (uint8_t)ARM64_DECODE_HIGHEST_SET_BIT(value);
    if (len < 1 || (width == 32 && len == 6))
    {
        return 0;
    }

    uint8_t levels = (1U << len) - 1;
    uint8_t s = imms & levels;
    uint8_t r = immr & levels;
    if (immediate && s == levels)
    {
        return 0;
    }

    uint8_t element_width = 1U << len;
    uint8_t d = (s - r) & levels;
    uint64_t welem = arm64_decode_immediate_ror_element(arm64_decode_immediate_low_mask(s + 1), r, element_width);
    uint64_t telem = arm64_decode_immediate_low_mask(d + 1);
    *wmask = arm64_decode_immediate_replicate(welem, element_width, width);
    *tmask = arm64_decode_immediate_replicate(telem, element_width, width);
    return 1;
}

static inline enum arm64_decode_status arm64_decode_immediate_pc_relative(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x1F000000U, 0x10000000U)) return ARM64_DECODE_UNSUPPORTED;
    uint64_t immediate = ARM64_DECODE_CONCAT(ARM64_DECODE_FIELD(raw, 23, 5), ARM64_DECODE_FIELD(raw, 30, 29), 2U);
    uint32_t page = ARM64_DECODE_BIT(raw, 31);

    decoded->instruction = page ? ARM64_INST_ADRP : ARM64_INST_ADR;
    decoded->offset = page ? ARM64_DECODE_SIGN_EXTEND(ARM64_DECODE_SCALE(immediate, 12U), 33) : ARM64_DECODE_SIGN_EXTEND(immediate, 21);
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->operand_width = 64;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_immediate_add_sub(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x1F800000U, 0x11000000U)) return ARM64_DECODE_UNSUPPORTED;
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_FIELD(raw, 30, 29))
    {
    case 0:
        instruction = ARM64_INST_ADD_IMMEDIATE;
        break;
    case 1:
        instruction = ARM64_INST_ADDS_IMMEDIATE;
        break;
    case 2:
        instruction = ARM64_INST_SUB_IMMEDIATE;
        break;
    case 3:
        instruction = ARM64_INST_SUBS_IMMEDIATE;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    uint32_t shift = ARM64_DECODE_BIT(raw, 22) ? 12U : 0U;

    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = ARM64_DECODE_GPR_WIDTH(raw);
    decoded->shift_amount = shift;
    decoded->immediate = ARM64_DECODE_SHIFTED_FIELD(raw, 21, 10, shift);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_immediate_minmax(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x7FF00000U, 0x11C00000U)) return ARM64_DECODE_UNSUPPORTED;
    uint64_t immediate = ARM64_DECODE_FIELD(raw, 17, 10);
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_FIELD(raw, 19, 18))
    {
    case 0:
        instruction = ARM64_INST_SMAX_IMMEDIATE;
        immediate = ARM64_DECODE_SIGN_EXTEND(immediate, 8);
        break;
    case 1:
        instruction = ARM64_INST_UMAX_IMMEDIATE;
        break;
    case 2:
        instruction = ARM64_INST_SMIN_IMMEDIATE;
        immediate = ARM64_DECODE_SIGN_EXTEND(immediate, 8);
        break;
    case 3:
        instruction = ARM64_INST_UMIN_IMMEDIATE;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }

    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = ARM64_DECODE_GPR_WIDTH(raw);
    decoded->immediate = immediate;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_immediate_logical(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x1F800000U, 0x12000000U)) return ARM64_DECODE_UNSUPPORTED;
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_FIELD(raw, 30, 29))
    {
    case 0:
        instruction = ARM64_INST_AND_IMMEDIATE;
        break;
    case 1:
        instruction = ARM64_INST_ORR_IMMEDIATE;
        break;
    case 2:
        instruction = ARM64_INST_EOR_IMMEDIATE;
        break;
    case 3:
        instruction = ARM64_INST_ANDS_IMMEDIATE;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    uint32_t width = ARM64_DECODE_GPR_WIDTH(raw);
    uint8_t immr = ARM64_DECODE_FIELD(raw, 21, 16);
    uint64_t wmask;
    uint64_t tmask;
    if (!arm64_decode_immediate_bit_masks(ARM64_DECODE_BIT(raw, 22), immr, ARM64_DECODE_FIELD(raw, 15, 10), width, 1, &wmask, &tmask)) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = width;
    decoded->immr = immr;
    decoded->imms = 0;
    decoded->immediate = wmask;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_immediate_move_wide(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x1F800000U, 0x12800000U)) return ARM64_DECODE_UNSUPPORTED;
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_FIELD(raw, 30, 29))
    {
    case 0:
        instruction = ARM64_INST_MOVN;
        break;
    case 1:
        return ARM64_DECODE_UNALLOCATED;
    case 2:
        instruction = ARM64_INST_MOVZ;
        break;
    case 3:
        instruction = ARM64_INST_MOVK;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    uint32_t width = ARM64_DECODE_GPR_WIDTH(raw);
    uint32_t shift = ARM64_DECODE_SHIFTED_FIELD(raw, 22, 21, 4);
    if (shift >= width) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->operand_width = width;
    decoded->immediate = ARM64_DECODE_FIELD(raw, 20, 5);
    decoded->shift_amount = shift;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_immediate_bitfield(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x1F800000U, 0x13000000U)) return ARM64_DECODE_UNSUPPORTED;
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_FIELD(raw, 30, 29))
    {
    case 0:
        instruction = ARM64_INST_SBFM;
        break;
    case 1:
        instruction = ARM64_INST_BFM;
        break;
    case 2:
        instruction = ARM64_INST_UBFM;
        break;
    case 3:
        return ARM64_DECODE_UNALLOCATED;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    uint32_t width = ARM64_DECODE_GPR_WIDTH(raw);
    uint8_t high_bit = ARM64_DECODE_BIT(raw, 22);
    uint8_t immr = ARM64_DECODE_FIELD(raw, 21, 16);
    uint8_t imms = ARM64_DECODE_FIELD(raw, 15, 10);
    uint64_t wmask;
    uint64_t tmask;
    if (high_bit != (width == 64) || (width == 32 && ((immr | imms) & 0x20))) return ARM64_DECODE_UNALLOCATED;
    if (!arm64_decode_immediate_bit_masks(high_bit, immr, imms, width, 0, &wmask, &tmask)) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = width;
    decoded->immr = immr;
    decoded->imms = imms;
    decoded->bitfield_wmask = wmask;
    decoded->bitfield_tmask = tmask;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_immediate_extract(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x7FA00000U, 0x13800000U)) return ARM64_DECODE_UNSUPPORTED;
    uint32_t width = ARM64_DECODE_GPR_WIDTH(raw);
    uint32_t shift = ARM64_DECODE_FIELD(raw, 15, 10);
    if (ARM64_DECODE_BIT(raw, 22) != (width == 64) || shift >= width) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = ARM64_INST_EXTR;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = width;
    decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->shift_amount = shift;
    return ARM64_DECODE_OK;
}

enum arm64_decode_status arm64_decode_data_processing_immediate(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;
    enum arm64_decode_status status;
    switch (raw & 0x1F800000U)
    {
    case 0x10000000U:
    case 0x10800000U:
        status = arm64_decode_immediate_pc_relative(raw, candidate);
        break;
    case 0x11000000U:
        status = arm64_decode_immediate_add_sub(raw, candidate);
        break;
    case 0x11800000U:
        status = arm64_decode_immediate_minmax(raw, candidate);
        break;
    case 0x12000000U:
        status = arm64_decode_immediate_logical(raw, candidate);
        break;
    case 0x12800000U:
        status = arm64_decode_immediate_move_wide(raw, candidate);
        break;
    case 0x13000000U:
        status = arm64_decode_immediate_bitfield(raw, candidate);
        break;
    case 0x13800000U:
        status = arm64_decode_immediate_extract(raw, candidate);
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    if (status == ARM64_DECODE_OK)
    {
        candidate->instruction_class = ARM64_INSTRUCTION_CLASS_DATA_PROCESSING_IMMEDIATE;
        *decoded = result;
    }
    return status;
}
