#include "arm64_decode.h"

static arm64_s64 arm64_sign_extend(arm64_u64 value, arm64_u8 bits)
{
    arm64_u64 sign = 1ULL << (bits - 1);

    return (arm64_s64)((value ^ sign) - sign);
}

static void arm64_decode_base_registers(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    decoded->rd = raw & 0x1F;
    decoded->rn = (raw >> 5) & 0x1F;
    decoded->ra = (raw >> 10) & 0x1F;
    decoded->rm = (raw >> 16) & 0x1F;
    decoded->operand_width = (raw & 0x80000000U) ? 64 : 32;
    if (raw & 0x80000000U) decoded->flags |= ARM64_INSN_FLAG_64BIT;
}

static enum arm64_operation arm64_decode_add_sub_operation(arm64_u32 raw)
{
    return (raw & 0x40000000U) ? ARM64_OPERATION_SUB : ARM64_OPERATION_ADD;
}

static enum arm64_operation arm64_decode_logical_operation(arm64_u32 raw)
{
    switch ((raw >> 29) & 0x3)
    {
    case 0:
        return ARM64_OPERATION_AND;
    case 1:
        return ARM64_OPERATION_ORR;
    case 2:
        return ARM64_OPERATION_EOR;
    default:
        return ARM64_OPERATION_ANDS;
    }
}

static arm64_u64 arm64_low_mask(arm64_u8 bits)
{
    return bits >= 64 ? ~0ULL : (1ULL << bits) - 1;
}

static arm64_u64 arm64_ror_element(arm64_u64 value, arm64_u8 rotation, arm64_u8 width)
{
    arm64_u64 mask = arm64_low_mask(width);

    rotation %= width;
    value &= mask;
    if (!rotation) return value;
    return ((value >> rotation) | (value << (width - rotation))) & mask;
}

static arm64_u64 arm64_replicate(arm64_u64 value, arm64_u8 element_width, arm64_u8 width)
{
    arm64_u64 result = 0;
    arm64_u8 offset;

    value &= arm64_low_mask(element_width);
    for (offset = 0; offset < width; offset += element_width) result |= value << offset;
    return result;
}

static int arm64_decode_bit_masks(arm64_u8 n, arm64_u8 immr, arm64_u8 imms, arm64_u8 width, int immediate, arm64_u64 *wmask, arm64_u64 *tmask)
{
    /* 按 ARM ARM DecodeBitMasks 规则展开逻辑立即数和位域掩码。 */
    arm64_u32 value = ((arm64_u32)n << 6) | (~imms & 0x3F);
    arm64_u8 len;
    arm64_u8 levels;
    arm64_u8 s;
    arm64_u8 r;
    arm64_u8 diff;
    arm64_u8 element_width;

    if (!value) return 0;
    len = (arm64_u8)(31U - __builtin_clz(value));
    if (len < 1 || (width == 32 && len == 6)) return 0;

    levels = (1U << len) - 1;
    s = imms & levels;
    r = immr & levels;
    if (immediate && s == levels) return 0;

    diff = (s - r) & levels;
    element_width = 1U << len;
    *wmask = arm64_replicate(arm64_ror_element(arm64_low_mask(s + 1), r, element_width), element_width, width);
    *tmask = arm64_replicate(arm64_low_mask(diff + 1), element_width, width);
    return 1;
}

static enum arm64_operation arm64_decode_2source_operation(arm64_u32 selector)
{
    switch (selector)
    {
    case 2:
        return ARM64_OPERATION_UDIV;
    case 3:
        return ARM64_OPERATION_SDIV;
    case 8:
        return ARM64_OPERATION_LSLV;
    case 9:
        return ARM64_OPERATION_LSRV;
    case 10:
        return ARM64_OPERATION_ASRV;
    case 11:
        return ARM64_OPERATION_RORV;
    case 0x10:
        return ARM64_OPERATION_CRC32B;
    case 0x11:
        return ARM64_OPERATION_CRC32H;
    case 0x12:
        return ARM64_OPERATION_CRC32W;
    case 0x13:
        return ARM64_OPERATION_CRC32X;
    case 0x14:
        return ARM64_OPERATION_CRC32CB;
    case 0x15:
        return ARM64_OPERATION_CRC32CH;
    case 0x16:
        return ARM64_OPERATION_CRC32CW;
    case 0x17:
        return ARM64_OPERATION_CRC32CX;
    case 0x18:
        return ARM64_OPERATION_SMAX;
    case 0x19:
        return ARM64_OPERATION_UMAX;
    case 0x1A:
        return ARM64_OPERATION_SMIN;
    case 0x1B:
        return ARM64_OPERATION_UMIN;
    default:
        return ARM64_OPERATION_NONE;
    }
}

static enum arm64_operation arm64_decode_1source_operation(arm64_u32 selector)
{
    switch (selector)
    {
    case 0:
        return ARM64_OPERATION_RBIT;
    case 1:
        return ARM64_OPERATION_REV16;
    case 2:
        return ARM64_OPERATION_REV32;
    case 3:
        return ARM64_OPERATION_REV64;
    case 4:
        return ARM64_OPERATION_CLZ;
    case 5:
        return ARM64_OPERATION_CLS;
    case 6:
        return ARM64_OPERATION_CTZ;
    case 7:
        return ARM64_OPERATION_CNT;
    case 8:
        return ARM64_OPERATION_ABS;
    default:
        return ARM64_OPERATION_NONE;
    }
}

/* 解码 Data Processing -- Immediate，并展开 PC-relative/普通立即数。 */
enum arm64_decode_status arm64_decode_data_processing_immediate(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 opc;

    decoded->insn_class = ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE;

    if ((raw & 0x1F000000U) == 0x10000000U)
    {
        arm64_u64 imm21 = (((arm64_u64)raw >> 5) & 0x7FFFFULL) << 2 | ((raw >> 29) & 0x3);

        decoded->opcode = (raw & 0x80000000U) ? ARM64_OP_ADRP : ARM64_OP_ADR;
        decoded->rd = raw & 0x1F;
        decoded->operand_width = 64;
        decoded->flags = ARM64_INSN_FLAG_64BIT;
        decoded->operands.pc_relative.offset = decoded->opcode == ARM64_OP_ADRP ? arm64_sign_extend(imm21 << 12, 33) : arm64_sign_extend(imm21, 21);
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1F800000U) == 0x11000000U)
    {
        decoded->opcode = ARM64_OP_ADD_SUB_IMMEDIATE;
        decoded->operation = arm64_decode_add_sub_operation(raw);
        arm64_decode_base_registers(raw, decoded);
        decoded->operands.data.immediate = (raw >> 10) & 0xFFF;
        decoded->operands.data.shift_amount = (raw & 0x00400000U) ? 12 : 0;
        if (decoded->operands.data.shift_amount) decoded->operands.data.immediate <<= 12;
        if (raw & 0x20000000U) decoded->flags |= ARM64_INSN_FLAG_SETFLAGS;
        if (raw & 0x40000000U) decoded->flags |= ARM64_INSN_FLAG_SUBTRACT;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FF00000U) == 0x11C00000U)
    {
        decoded->opcode = ARM64_OP_MIN_MAX_IMMEDIATE;
        arm64_decode_base_registers(raw, decoded);
        decoded->operands.data.immediate = (raw >> 10) & 0xFF;
        opc = (raw >> 18) & 0x3;
        decoded->operation = opc == 0 ? ARM64_OPERATION_SMAX : opc == 1 ? ARM64_OPERATION_UMAX : opc == 2 ? ARM64_OPERATION_SMIN : ARM64_OPERATION_UMIN;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1F800000U) == 0x12000000U)
    {
        arm64_u8 imms = (raw >> 10) & 0x3F;

        decoded->opcode = ARM64_OP_LOGICAL_IMMEDIATE;
        decoded->operation = arm64_decode_logical_operation(raw);
        arm64_decode_base_registers(raw, decoded);
        decoded->operands.data.option = (raw >> 22) & 1;
        decoded->operands.data.immr = (raw >> 16) & 0x3F;
        if (!arm64_decode_bit_masks(decoded->operands.data.option, decoded->operands.data.immr, imms, decoded->operand_width, 1, &decoded->operands.data.immediate, &decoded->operands.data.tmask)) return ARM64_DECODE_UNALLOCATED;
        if (((raw >> 29) & 0x3) == 3) decoded->flags |= ARM64_INSN_FLAG_SETFLAGS;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7F800000U) == 0x13000000U || (raw & 0x7F800000U) == 0x33000000U || (raw & 0x7F800000U) == 0x53000000U)
    {
        arm64_u8 imms = (raw >> 10) & 0x3F;

        decoded->opcode = ARM64_OP_BITFIELD;
        arm64_decode_base_registers(raw, decoded);
        opc = (raw >> 29) & 0x3;
        if (opc == 3) return ARM64_DECODE_UNALLOCATED;
        decoded->operation = opc == 0 ? ARM64_OPERATION_SBFM : opc == 1 ? ARM64_OPERATION_BFM : ARM64_OPERATION_UBFM;
        decoded->operands.data.option = (raw >> 22) & 1;
        decoded->operands.data.immr = (raw >> 16) & 0x3F;
        if (decoded->operands.data.option != (decoded->operand_width == 64) || (decoded->operand_width == 32 && ((decoded->operands.data.immr | imms) & 0x20))) return ARM64_DECODE_UNALLOCATED;
        if (!arm64_decode_bit_masks(decoded->operands.data.option, decoded->operands.data.immr, imms, decoded->operand_width, 0, &decoded->operands.data.wmask, &decoded->operands.data.tmask)) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FA00000U) == 0x13800000U)
    {
        decoded->opcode = ARM64_OP_EXTRACT;
        decoded->operation = ARM64_OPERATION_EXTR;
        arm64_decode_base_registers(raw, decoded);
        decoded->operands.data.option = (raw >> 22) & 1;
        decoded->operands.data.shift_amount = (raw >> 10) & 0x3F;
        if (decoded->operands.data.option != (decoded->operand_width == 64) || decoded->operands.data.shift_amount >= decoded->operand_width) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1F800000U) == 0x12800000U)
    {
        decoded->opcode = ARM64_OP_MOVE_WIDE;
        arm64_decode_base_registers(raw, decoded);
        opc = (raw >> 29) & 0x3;
        if (opc == 1) return ARM64_DECODE_UNALLOCATED;
        decoded->operation = opc == 0 ? ARM64_OPERATION_MOVN : opc == 2 ? ARM64_OPERATION_MOVZ : ARM64_OPERATION_MOVK;
        decoded->operands.data.immediate = (raw >> 5) & 0xFFFF;
        decoded->operands.data.shift_amount = ((raw >> 21) & 0x3) * 16;
        if (decoded->operand_width == 32 && decoded->operands.data.shift_amount >= 32) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    decoded->opcode = ARM64_OP_UNKNOWN;
    return ARM64_DECODE_UNSUPPORTED;
}

/* 解码 Data Processing -- Register，并把编码选择位转换成 operation。 */
enum arm64_decode_status arm64_decode_data_processing_register(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 opc;

    decoded->insn_class = ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER;

    if ((raw & 0x1F200000U) == 0x0B000000U || (raw & 0x1FE00000U) == 0x0B200000U)
    {
        decoded->opcode = (raw & 0x00200000U) ? ARM64_OP_ADD_SUB_EXTENDED : ARM64_OP_ADD_SUB_SHIFTED;
        decoded->operation = arm64_decode_add_sub_operation(raw);
        arm64_decode_base_registers(raw, decoded);
        if (decoded->opcode == ARM64_OP_ADD_SUB_SHIFTED)
        {
            decoded->operands.data.shift_type = (raw >> 22) & 0x3;
            decoded->operands.data.shift_amount = (raw >> 10) & 0x3F;
            if (decoded->operands.data.shift_type == 3 || (decoded->operand_width == 32 && (decoded->operands.data.shift_amount & 0x20))) return ARM64_DECODE_UNALLOCATED;
        }
        else
        {
            decoded->operands.data.option = (raw >> 13) & 0x7;
            decoded->operands.data.shift_amount = (raw >> 10) & 0x7;
            if (decoded->operands.data.shift_amount > 4) return ARM64_DECODE_UNALLOCATED;
        }
        if (raw & 0x20000000U) decoded->flags |= ARM64_INSN_FLAG_SETFLAGS;
        if (raw & 0x40000000U) decoded->flags |= ARM64_INSN_FLAG_SUBTRACT;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1F000000U) == 0x0A000000U)
    {
        decoded->opcode = ARM64_OP_LOGICAL_SHIFTED;
        decoded->operation = arm64_decode_logical_operation(raw);
        arm64_decode_base_registers(raw, decoded);
        decoded->operands.data.shift_type = (raw >> 22) & 0x3;
        decoded->operands.data.shift_amount = (raw >> 10) & 0x3F;
        if (decoded->operand_width == 32 && (decoded->operands.data.shift_amount & 0x20)) return ARM64_DECODE_UNALLOCATED;
        if (raw & 0x00200000U) decoded->flags |= ARM64_INSN_FLAG_INVERT;
        if (((raw >> 29) & 0x3) == 3) decoded->flags |= ARM64_INSN_FLAG_SETFLAGS;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x3FE00000U) == 0x1A800000U)
    {
        decoded->opcode = ARM64_OP_CONDITIONAL_SELECT;
        arm64_decode_base_registers(raw, decoded);
        if (raw & 0x00000800U) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.data.condition = (raw >> 12) & 0xF;
        opc = ((raw >> 29) & 1) << 1 | ((raw >> 10) & 1);
        decoded->operation = opc == 0 ? ARM64_OPERATION_CSEL : opc == 1 ? ARM64_OPERATION_CSINC : opc == 2 ? ARM64_OPERATION_CSINV : ARM64_OPERATION_CSNEG;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FE00000U) == 0x1AC00000U)
    {
        decoded->opcode = ARM64_OP_DATA_PROCESSING_2_SOURCE;
        arm64_decode_base_registers(raw, decoded);
        opc = (raw >> 10) & 0x3F;
        decoded->operation = arm64_decode_2source_operation(opc);
        if (decoded->operation == ARM64_OPERATION_NONE) return ARM64_DECODE_UNSUPPORTED;
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
        arm64_u32 op31 = (raw >> 21) & 0x7;
        arm64_u32 subtract = (raw >> 15) & 1;

        arm64_decode_base_registers(raw, decoded);
        switch (op31)
        {
        case 0:
            decoded->opcode = ARM64_OP_MULTIPLY_ADD;
            decoded->operation = subtract ? ARM64_OPERATION_MSUB : ARM64_OPERATION_MADD;
            break;
        case 1:
            if (decoded->operand_width != 64) return ARM64_DECODE_UNALLOCATED;
            decoded->opcode = ARM64_OP_MULTIPLY_ADD;
            decoded->operation = subtract ? ARM64_OPERATION_SMSUBL : ARM64_OPERATION_SMADDL;
            break;
        case 2:
            if (decoded->operand_width != 64 || subtract || decoded->ra != 31) return ARM64_DECODE_UNALLOCATED;
            decoded->opcode = ARM64_OP_MULTIPLY_HIGH;
            decoded->operation = ARM64_OPERATION_SMULH;
            break;
        case 3:
            if (decoded->operand_width != 64) return ARM64_DECODE_UNALLOCATED;
            return ARM64_DECODE_UNSUPPORTED;
        case 5:
            if (decoded->operand_width != 64) return ARM64_DECODE_UNALLOCATED;
            decoded->opcode = ARM64_OP_MULTIPLY_ADD;
            decoded->operation = subtract ? ARM64_OPERATION_UMSUBL : ARM64_OPERATION_UMADDL;
            break;
        case 6:
            if (decoded->operand_width != 64 || subtract || decoded->ra != 31) return ARM64_DECODE_UNALLOCATED;
            decoded->opcode = ARM64_OP_MULTIPLY_HIGH;
            decoded->operation = ARM64_OPERATION_UMULH;
            break;
        default:
            return ARM64_DECODE_UNALLOCATED;
        }
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x1FE0FC00U) == 0x1A000000U)
    {
        decoded->opcode = ARM64_OP_ADD_SUB_CARRY;
        arm64_decode_base_registers(raw, decoded);
        opc = ((raw >> 30) & 1) << 1 | ((raw >> 29) & 1);
        decoded->operation = opc == 0 ? ARM64_OPERATION_ADC : opc == 1 ? ARM64_OPERATION_ADCS : opc == 2 ? ARM64_OPERATION_SBC : ARM64_OPERATION_SBCS;
        if (raw & 0x20000000U) decoded->flags |= ARM64_INSN_FLAG_SETFLAGS;
        if (raw & 0x40000000U) decoded->flags |= ARM64_INSN_FLAG_SUBTRACT;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x3FE00410U) == 0x3A400000U)
    {
        decoded->opcode = ARM64_OP_CONDITIONAL_COMPARE;
        arm64_decode_base_registers(raw, decoded);
        decoded->operation = (raw & 0x40000000U) ? ARM64_OPERATION_CCMP : ARM64_OPERATION_CCMN;
        decoded->operands.data.condition = (raw >> 12) & 0xF;
        decoded->operands.data.nzcv = raw & 0xF;
        if (raw & 0x00000800U)
        {
            decoded->flags |= ARM64_INSN_FLAG_IMMEDIATE;
            decoded->operands.data.immediate = decoded->rm;
        }
        decoded->flags |= ARM64_INSN_FLAG_SETFLAGS;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFFFFC000U) == 0xDAC10000U)
    {
        arm64_decode_base_registers(raw, decoded);
        if ((raw & 0x00002000U) && decoded->rn != 31) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_UNSUPPORTED;
    }

    if ((raw & 0xFFFFFFE0U) == 0xDAC143E0U || (raw & 0xFFFFFFE0U) == 0xDAC147E0U)
    {
        arm64_decode_base_registers(raw, decoded);
        return ARM64_DECODE_UNSUPPORTED;
    }

    if ((raw & 0x7FE00000U) == 0x5AC00000U)
    {
        decoded->opcode = ARM64_OP_DATA_PROCESSING_1_SOURCE;
        arm64_decode_base_registers(raw, decoded);
        if (((raw >> 16) & 0x1F) != 0) return ARM64_DECODE_UNALLOCATED;
        opc = (raw >> 10) & 0x3F;
        decoded->operation = arm64_decode_1source_operation(opc);
        if (decoded->operation == ARM64_OPERATION_NONE) return ARM64_DECODE_UNSUPPORTED;
        if (decoded->operation == ARM64_OPERATION_REV64 && decoded->operand_width != 64) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    decoded->opcode = ARM64_OP_UNKNOWN;
    return ARM64_DECODE_UNSUPPORTED;
}