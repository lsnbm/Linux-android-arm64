#include "arm64_decode_internal.h"

static void arm64_decode_rd_rn(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    decoded->rd = raw & 0x1F;
    decoded->rn = (raw >> 5) & 0x1F;
    decoded->operand_width = (raw & 0x80000000U) ? 64 : 32;
}

static void arm64_decode_rd_rn_rm(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    arm64_decode_rd_rn(raw, decoded);
    decoded->rm = (raw >> 16) & 0x1F;
}

static uint64_t arm64_low_mask(uint8_t bits)
{
    return bits >= 64 ? ~0ULL : (1ULL << bits) - 1;
}

static uint64_t arm64_ror_element(uint64_t value, uint8_t rotation, uint8_t width)
{
    uint64_t mask = arm64_low_mask(width);

    rotation %= width;
    value &= mask;
    if (!rotation) return value;
    return ((value >> rotation) | (value << (width - rotation))) & mask;
}

static uint64_t arm64_replicate(uint64_t value, uint8_t element_width, uint8_t width)
{
    uint64_t result = 0;

    value &= arm64_low_mask(element_width);
    for (uint8_t offset = 0; offset < width; offset += element_width) result |= value << offset;
    return result;
}

static int arm64_decode_bit_masks(uint8_t n, uint8_t immr, uint8_t imms, uint8_t width, int immediate, uint64_t *wmask, uint64_t *tmask)
{
    /* 按 ARM ARM DecodeBitMasks 规则展开逻辑立即数和位域掩码。 */
    uint32_t value = ((uint32_t)n << 6) | (~imms & 0x3F);

    if (!value) return 0;
    uint8_t len = (uint8_t)(31U - __builtin_clz(value));
    if (len < 1 || (width == 32 && len == 6)) return 0;

    uint8_t levels = (1U << len) - 1;
    uint8_t s = imms & levels;
    uint8_t r = immr & levels;
    if (immediate && s == levels) return 0;

    uint8_t element_width = 1U << len;
    *wmask = arm64_replicate(arm64_ror_element(arm64_low_mask(s + 1), r, element_width), element_width, width);
    if (!immediate)
    {
        uint8_t diff = (s - r) & levels;

        *tmask = arm64_replicate(arm64_low_mask(diff + 1), element_width, width);
    }
    return 1;
}

/* 解码 Data Processing -- Immediate，并展开 PC-relative/普通立即数。 */
enum arm64_decode_status arm64_decode_data_processing_immediate(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    decoded->insn_class = ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE;

    if ((raw & 0x1F000000U) == 0x10000000U)
    {
        uint64_t imm21 = (((uint64_t)raw >> 5) & 0x7FFFFULL) << 2 | ((raw >> 29) & 0x3);

        decoded->instruction = (raw & 0x80000000U) ? ARM64_INSN_ADRP : ARM64_INSN_ADR;
        decoded->rd = raw & 0x1F;
        decoded->operand_width = 64;
        decoded->offset = (raw & 0x80000000U) ? arm64_sign_extend(imm21 << 12, 33) : arm64_sign_extend(imm21, 21);
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1F800000U) == 0x11000000U)
    {
        switch ((raw >> 29) & 0x3)
        {
        case 0:
            decoded->instruction = ARM64_INSN_ADD_IMMEDIATE;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_ADDS_IMMEDIATE;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_SUB_IMMEDIATE;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_SUBS_IMMEDIATE;
            break;
        }
        arm64_decode_rd_rn(raw, decoded);
        decoded->immediate = (raw >> 10) & 0xFFF;
        decoded->shift_amount = (raw & 0x00400000U) ? 12 : 0;
        if (decoded->shift_amount) decoded->immediate <<= 12;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FF00000U) == 0x11C00000U)
    {
        arm64_decode_rd_rn(raw, decoded);
        decoded->immediate = (raw >> 10) & 0xFF;
        switch ((raw >> 18) & 0x3)
        {
        case 0:
            decoded->instruction = ARM64_INSN_SMAX_IMMEDIATE;
            decoded->immediate = (uint64_t)arm64_sign_extend(decoded->immediate, 8);
            break;
        case 1:
            decoded->instruction = ARM64_INSN_UMAX_IMMEDIATE;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_SMIN_IMMEDIATE;
            decoded->immediate = (uint64_t)arm64_sign_extend(decoded->immediate, 8);
            break;
        case 3:
            decoded->instruction = ARM64_INSN_UMIN_IMMEDIATE;
            break;
        }
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1F800000U) == 0x12000000U)
    {
        uint8_t n = (raw >> 22) & 1;
        uint8_t immr = (raw >> 16) & 0x3F;
        uint8_t imms = (raw >> 10) & 0x3F;

        switch ((raw >> 29) & 0x3)
        {
        case 0:
            decoded->instruction = ARM64_INSN_AND_IMMEDIATE;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_ORR_IMMEDIATE;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_EOR_IMMEDIATE;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_ANDS_IMMEDIATE;
            break;
        }
        arm64_decode_rd_rn(raw, decoded);
        if (!arm64_decode_bit_masks(n, immr, imms, decoded->operand_width, 1, &decoded->immediate, &decoded->tmask)) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1F800000U) == 0x12800000U)
    {
        decoded->rd = raw & 0x1F;
        decoded->operand_width = (raw & 0x80000000U) ? 64 : 32;
        uint32_t opc = (raw >> 29) & 0x3;
        if (opc == 1) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = opc == 0 ? ARM64_INSN_MOVN : opc == 2 ? ARM64_INSN_MOVZ : ARM64_INSN_MOVK;
        decoded->immediate = (raw >> 5) & 0xFFFF;
        decoded->shift_amount = ((raw >> 21) & 0x3) * 16;
        if (decoded->operand_width == 32 && decoded->shift_amount >= 32) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1F800000U) == 0x13000000U)
    {
        uint8_t n = (raw >> 22) & 1;
        uint8_t imms = (raw >> 10) & 0x3F;

        arm64_decode_rd_rn(raw, decoded);
        uint32_t opc = (raw >> 29) & 0x3;
        if (opc == 3) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = opc == 0 ? ARM64_INSN_SBFM : opc == 1 ? ARM64_INSN_BFM : ARM64_INSN_UBFM;
        decoded->immr = (raw >> 16) & 0x3F;
        if (n != (decoded->operand_width == 64) || (decoded->operand_width == 32 && ((decoded->immr | imms) & 0x20))) return ARM64_DECODE_UNALLOCATED;
        if (!arm64_decode_bit_masks(n, decoded->immr, imms, decoded->operand_width, 0, &decoded->wmask, &decoded->tmask)) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FA00000U) == 0x13800000U)
    {
        uint8_t n = (raw >> 22) & 1;

        decoded->instruction = ARM64_INSN_EXTR;
        arm64_decode_rd_rn_rm(raw, decoded);
        decoded->shift_amount = (raw >> 10) & 0x3F;
        if (n != (decoded->operand_width == 64) || decoded->shift_amount >= decoded->operand_width) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    return ARM64_DECODE_UNSUPPORTED;
}

/* 解码 Data Processing -- Register，并把编码选择位转换成 operation。 */
enum arm64_decode_status arm64_decode_data_processing_register(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    decoded->insn_class = ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER;

    if ((raw & 0x1F000000U) == 0x0A000000U)
    {
        switch ((((raw >> 29) & 0x3) << 1) | ((raw >> 21) & 1))
        {
        case 0:
            decoded->instruction = ARM64_INSN_AND_SHIFTED_REGISTER;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_BIC_SHIFTED_REGISTER;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_ORR_SHIFTED_REGISTER;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_ORN_SHIFTED_REGISTER;
            break;
        case 4:
            decoded->instruction = ARM64_INSN_EOR_SHIFTED_REGISTER;
            break;
        case 5:
            decoded->instruction = ARM64_INSN_EON_SHIFTED_REGISTER;
            break;
        case 6:
            decoded->instruction = ARM64_INSN_ANDS_SHIFTED_REGISTER;
            break;
        case 7:
            decoded->instruction = ARM64_INSN_BICS_SHIFTED_REGISTER;
            break;
        }
        arm64_decode_rd_rn_rm(raw, decoded);
        decoded->shift_type = (raw >> 22) & 0x3;
        decoded->shift_amount = (raw >> 10) & 0x3F;
        if (decoded->operand_width == 32 && (decoded->shift_amount & 0x20)) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1F200000U) == 0x0B000000U)
    {
        switch ((raw >> 29) & 0x3)
        {
        case 0:
            decoded->instruction = ARM64_INSN_ADD_SHIFTED_REGISTER;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_ADDS_SHIFTED_REGISTER;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_SUB_SHIFTED_REGISTER;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_SUBS_SHIFTED_REGISTER;
            break;
        }
        arm64_decode_rd_rn_rm(raw, decoded);
        decoded->shift_type = (raw >> 22) & 0x3;
        decoded->shift_amount = (raw >> 10) & 0x3F;
        if (decoded->shift_type == 3 || (decoded->operand_width == 32 && (decoded->shift_amount & 0x20))) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1FE00000U) == 0x0B200000U)
    {
        switch ((raw >> 29) & 0x3)
        {
        case 0:
            decoded->instruction = ARM64_INSN_ADD_EXTENDED_REGISTER;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_ADDS_EXTENDED_REGISTER;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_SUB_EXTENDED_REGISTER;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_SUBS_EXTENDED_REGISTER;
            break;
        }
        arm64_decode_rd_rn_rm(raw, decoded);
        decoded->option = (raw >> 13) & 0x7;
        decoded->shift_amount = (raw >> 10) & 0x7;
        if (decoded->shift_amount > 4) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1FE0FC00U) == 0x1A000000U)
    {
        arm64_decode_rd_rn_rm(raw, decoded);
        uint32_t opc = ((raw >> 30) & 1) << 1 | ((raw >> 29) & 1);
        decoded->instruction = opc == 0 ? ARM64_INSN_ADC : opc == 1 ? ARM64_INSN_ADCS : opc == 2 ? ARM64_INSN_SBC : ARM64_INSN_SBCS;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x3FE00000U) == 0x1A800000U)
    {
        arm64_decode_rd_rn_rm(raw, decoded);
        if (raw & 0x00000800U) return ARM64_DECODE_UNALLOCATED;
        decoded->condition = (raw >> 12) & 0xF;
        uint32_t opc = ((raw >> 30) & 1) << 1 | ((raw >> 10) & 1);
        decoded->instruction = opc == 0 ? ARM64_INSN_CSEL : opc == 1 ? ARM64_INSN_CSINC : opc == 2 ? ARM64_INSN_CSINV : ARM64_INSN_CSNEG;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FE00000U) == 0x1AC00000U)
    {
        arm64_decode_rd_rn_rm(raw, decoded);
        uint32_t opc = (raw >> 10) & 0x3F;
        switch (opc)
        {
        case 2:
            decoded->instruction = ARM64_INSN_UDIV;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_SDIV;
            break;
        case 8:
            decoded->instruction = ARM64_INSN_LSLV;
            break;
        case 9:
            decoded->instruction = ARM64_INSN_LSRV;
            break;
        case 10:
            decoded->instruction = ARM64_INSN_ASRV;
            break;
        case 11:
            decoded->instruction = ARM64_INSN_RORV;
            break;
        case 0x10:
            decoded->instruction = ARM64_INSN_CRC32B;
            break;
        case 0x11:
            decoded->instruction = ARM64_INSN_CRC32H;
            break;
        case 0x12:
            decoded->instruction = ARM64_INSN_CRC32W;
            break;
        case 0x13:
            decoded->instruction = ARM64_INSN_CRC32X;
            break;
        case 0x14:
            decoded->instruction = ARM64_INSN_CRC32CB;
            break;
        case 0x15:
            decoded->instruction = ARM64_INSN_CRC32CH;
            break;
        case 0x16:
            decoded->instruction = ARM64_INSN_CRC32CW;
            break;
        case 0x17:
            decoded->instruction = ARM64_INSN_CRC32CX;
            break;
        case 0x18:
            decoded->instruction = ARM64_INSN_SMAX_REGISTER;
            break;
        case 0x19:
            decoded->instruction = ARM64_INSN_UMAX_REGISTER;
            break;
        case 0x1A:
            decoded->instruction = ARM64_INSN_SMIN_REGISTER;
            break;
        case 0x1B:
            decoded->instruction = ARM64_INSN_UMIN_REGISTER;
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        if ((opc >= 0x10 && opc <= 0x12) || (opc >= 0x14 && opc <= 0x16))
        {
            if (decoded->operand_width != 32) return ARM64_DECODE_UNALLOCATED;
        }
        else if (opc == 0x13 || opc == 0x17)
        {
            if (decoded->operand_width != 64) return ARM64_DECODE_UNALLOCATED;
        }
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7F000000U) == 0x1B000000U)
    {
        uint32_t op31 = (raw >> 21) & 0x7;
        uint32_t subtract = (raw >> 15) & 1;
        uint32_t ra = (raw >> 10) & 0x1F;

        arm64_decode_rd_rn_rm(raw, decoded);
        switch (op31)
        {
        case 0:
            decoded->instruction = subtract ? ARM64_INSN_MSUB : ARM64_INSN_MADD;
            break;
        case 1:
            if (decoded->operand_width != 64) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = subtract ? ARM64_INSN_SMSUBL : ARM64_INSN_SMADDL;
            break;
        case 2:
            if (decoded->operand_width != 64 || subtract || ra != 31) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INSN_SMULH;
            break;
        case 3:
            if (decoded->operand_width != 64) return ARM64_DECODE_UNALLOCATED;
            return ARM64_DECODE_UNSUPPORTED;
        case 5:
            if (decoded->operand_width != 64) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = subtract ? ARM64_INSN_UMSUBL : ARM64_INSN_UMADDL;
            break;
        case 6:
            if (decoded->operand_width != 64 || subtract || ra != 31) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INSN_UMULH;
            break;
        default:
            return ARM64_DECODE_UNALLOCATED;
        }
        if (op31 == 0 || op31 == 1 || op31 == 5) decoded->ra = ra;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x3FE00410U) == 0x3A400000U)
    {
        uint32_t operand = (raw >> 16) & 0x1F;

        decoded->rn = (raw >> 5) & 0x1F;
        decoded->operand_width = (raw & 0x80000000U) ? 64 : 32;
        decoded->condition = (raw >> 12) & 0xF;
        decoded->nzcv = raw & 0xF;
        if (raw & 0x00000800U)
        {
            decoded->instruction = (raw & 0x40000000U) ? ARM64_INSN_CCMP_IMMEDIATE : ARM64_INSN_CCMN_IMMEDIATE;
            decoded->immediate = operand;
        }
        else
        {
            decoded->instruction = (raw & 0x40000000U) ? ARM64_INSN_CCMP_REGISTER : ARM64_INSN_CCMN_REGISTER;
            decoded->rm = operand;
        }
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FFF0000U) == 0x5AC00000U)
    {
        arm64_decode_rd_rn(raw, decoded);
        uint32_t opc = (raw >> 10) & 0x3F;
        switch (opc)
        {
        case 0:
            decoded->instruction = ARM64_INSN_RBIT;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_REV16;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_REV32;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_REV64;
            break;
        case 4:
            decoded->instruction = ARM64_INSN_CLZ;
            break;
        case 5:
            decoded->instruction = ARM64_INSN_CLS;
            break;
        case 6:
            decoded->instruction = ARM64_INSN_CTZ;
            break;
        case 7:
            decoded->instruction = ARM64_INSN_CNT;
            break;
        case 8:
            decoded->instruction = ARM64_INSN_ABS;
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        if (decoded->instruction == ARM64_INSN_REV64 && decoded->operand_width != 64) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFFFFC000U) == 0xDAC10000U)
    {
        if ((raw & 0x00002000U) && ((raw >> 5) & 0x1F) != 31) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_UNSUPPORTED;
    }

    if ((raw & 0xFFFFFFE0U) == 0xDAC143E0U || (raw & 0xFFFFFFE0U) == 0xDAC147E0U)
    {
        return ARM64_DECODE_UNSUPPORTED;
    }

    return ARM64_DECODE_UNSUPPORTED;
}