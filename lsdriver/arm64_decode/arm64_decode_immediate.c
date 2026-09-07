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

static inline int arm64_decode_immediate_masks(uint32_t raw, struct arm64_decoded_instruction *decoded, int logical_immediate)
{
    uint8_t n = ARM64_DECODE_BIT(raw, 22);
    uint8_t immr = ARM64_DECODE_FIELD(raw, 21, 16);
    uint8_t imms = ARM64_DECODE_FIELD(raw, 15, 10);
    uint64_t wmask;
    uint64_t tmask;

    decoded->immr = immr;
    decoded->imms = logical_immediate ? 0 : imms;
    if (!logical_immediate && (n != (decoded->operand_width == 64) || (decoded->operand_width == 32 && ((immr | imms) & 0x20))))
    {
        return 0;
    }
    if (!arm64_decode_immediate_bit_masks(n, immr, imms, decoded->operand_width, logical_immediate, &wmask, &tmask))
    {
        return 0;
    }
    if (logical_immediate)
    {
        decoded->immediate = wmask;
    }
    else
    {
        decoded->bitfield_wmask = wmask;
        decoded->bitfield_tmask = tmask;
    }
    return 1;
}

static inline enum arm64_decode_status decode_conditional_compare(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint32_t operand;

    switch (raw & 0x3FE00410U)
    {
    case 0x3A400000U:
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    operand = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
    decoded->condition = ARM64_DECODE_FIELD(raw, 15, 12);
    decoded->nzcv = raw & 0xF;
    if (raw & 0x00000800U)
    {
        decoded->instruction = (raw & 0x40000000U) ? ARM64_INST_CCMP_IMMEDIATE : ARM64_INST_CCMN_IMMEDIATE;
        decoded->immediate = operand;
    }
    else
    {
        decoded->instruction = (raw & 0x40000000U) ? ARM64_INST_CCMP_REGISTER : ARM64_INST_CCMN_REGISTER;
        decoded->rm = operand;
    }
    return ARM64_DECODE_OK;
}

/* ======================== Data Processing -- Immediate ======================== */

/* 解码 PC-relative 地址生成指令。 */
enum arm64_decode_status arm64_decode_data_processing_immediate(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    decoded->instruction_class = ARM64_INSTRUCTION_CLASS_DATA_PROCESSING_IMMEDIATE;

    switch (raw & 0x1F800000U)
    {
    case 0x10000000U:
    case 0x10800000U:
    {
        uint64_t imm21 = ((uint64_t)ARM64_DECODE_FIELD(raw, 23, 5) << 2) | ARM64_DECODE_FIELD(raw, 30, 29);

        switch (ARM64_DECODE_BIT(raw, 31))
        {
        case 0:
            decoded->instruction = ARM64_INST_ADR;
            decoded->offset = ARM64_DECODE_SIGN_EXTEND(imm21, 21);
            break;
        default:
            decoded->instruction = ARM64_INST_ADRP;
            decoded->offset = ARM64_DECODE_SIGN_EXTEND(imm21 << 12, 33);
            break;
        }
        decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->operand_width = 64;
        return ARM64_DECODE_OK;
    }

    /* ADD/SUB (immediate). */
    case 0x11000000U:
    {
        switch (ARM64_DECODE_FIELD(raw, 30, 29))
        {
        case 0:
            decoded->instruction = ARM64_INST_ADD_IMMEDIATE;
            break;
        case 1:
            decoded->instruction = ARM64_INST_ADDS_IMMEDIATE;
            break;
        case 2:
            decoded->instruction = ARM64_INST_SUB_IMMEDIATE;
            break;
        case 3:
            decoded->instruction = ARM64_INST_SUBS_IMMEDIATE;
            break;
        }
        decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
        decoded->immediate = ARM64_DECODE_FIELD(raw, 21, 10);
        switch (ARM64_DECODE_BIT(raw, 22))
        {
        case 0:
            decoded->shift_amount = 0;
            break;
        default:
            decoded->shift_amount = 12;
            decoded->immediate <<= 12;
            break;
        }
        return ARM64_DECODE_OK;
    }

    /* UMAX/UMIN/SMAX/SMIN (immediate). */
    case 0x11800000U:
    {
        switch (raw & 0x7FF00000U)
        {
        case 0x11C00000U:
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
        decoded->immediate = ARM64_DECODE_FIELD(raw, 17, 10);
        switch (ARM64_DECODE_FIELD(raw, 19, 18))
        {
        case 0:
            decoded->instruction = ARM64_INST_SMAX_IMMEDIATE;
            decoded->immediate = (uint64_t)ARM64_DECODE_SIGN_EXTEND(decoded->immediate, 8);
            break;
        case 1:
            decoded->instruction = ARM64_INST_UMAX_IMMEDIATE;
            break;
        case 2:
            decoded->instruction = ARM64_INST_SMIN_IMMEDIATE;
            decoded->immediate = (uint64_t)ARM64_DECODE_SIGN_EXTEND(decoded->immediate, 8);
            break;
        case 3:
            decoded->instruction = ARM64_INST_UMIN_IMMEDIATE;
            break;
        }
        return ARM64_DECODE_OK;
    }

    /* Logical immediate. */
    case 0x12000000U:
    {
        switch (ARM64_DECODE_FIELD(raw, 30, 29))
        {
        case 0:
            decoded->instruction = ARM64_INST_AND_IMMEDIATE;
            break;
        case 1:
            decoded->instruction = ARM64_INST_ORR_IMMEDIATE;
            break;
        case 2:
            decoded->instruction = ARM64_INST_EOR_IMMEDIATE;
            break;
        case 3:
            decoded->instruction = ARM64_INST_ANDS_IMMEDIATE;
            break;
        }
        decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
        if (!arm64_decode_immediate_masks(raw, decoded, 1))
        {
            return ARM64_DECODE_UNALLOCATED;
        }
        return ARM64_DECODE_OK;
    }

    /* Move wide immediate. */
    case 0x12800000U:
    {
        decoded->rd = raw & 0x1F;
        decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
        uint32_t opc = ARM64_DECODE_FIELD(raw, 30, 29);
        if (opc == 1)
        {
            return ARM64_DECODE_UNALLOCATED;
        }
        switch (opc)
        {
        case 0:
            decoded->instruction = ARM64_INST_MOVN;
            break;
        case 2:
            decoded->instruction = ARM64_INST_MOVZ;
            break;
        default:
            decoded->instruction = ARM64_INST_MOVK;
            break;
        }
        decoded->immediate = ARM64_DECODE_FIELD(raw, 20, 5);
        decoded->shift_amount = ARM64_DECODE_FIELD(raw, 22, 21) * 16;
        if (decoded->operand_width == 32 && decoded->shift_amount >= 32)
        {
            return ARM64_DECODE_UNALLOCATED;
        }
        return ARM64_DECODE_OK;
    }

    /* Bitfield immediate. */
    case 0x13000000U:
    {
        decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
        uint32_t opc = ARM64_DECODE_FIELD(raw, 30, 29);
        if (opc == 3)
        {
            return ARM64_DECODE_UNALLOCATED;
        }
        switch (opc)
        {
        case 0:
            decoded->instruction = ARM64_INST_SBFM;
            break;
        case 1:
            decoded->instruction = ARM64_INST_BFM;
            break;
        default:
            decoded->instruction = ARM64_INST_UBFM;
            break;
        }
        if (!arm64_decode_immediate_masks(raw, decoded, 0))
        {
            return ARM64_DECODE_UNALLOCATED;
        }
        return ARM64_DECODE_OK;
    }

    /* Extract register. */
    case 0x13800000U:
    {
        switch (raw & 0x7FA00000U)
        {
        case 0x13800000U:
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        uint8_t n = ARM64_DECODE_BIT(raw, 22);

        decoded->instruction = ARM64_INST_EXTR;
        decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
        decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
        decoded->shift_amount = ARM64_DECODE_FIELD(raw, 15, 10);
        if (n != (decoded->operand_width == 64) || decoded->shift_amount >= decoded->operand_width)
        {
            return ARM64_DECODE_UNALLOCATED;
        }
        return ARM64_DECODE_OK;
    }

    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

