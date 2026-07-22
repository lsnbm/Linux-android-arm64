#include "arm64_decode.h"

static void arm64_decode_simd_registers(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    decoded->rd = raw & 0x1F;
    decoded->rn = (raw >> 5) & 0x1F;
    decoded->ra = (raw >> 10) & 0x1F;
    decoded->rm = (raw >> 16) & 0x1F;
}

static arm64_u64 arm64_simd_expand_fp_imm(arm64_u8 immediate, arm64_u8 width)
{
    arm64_u64 sign = (arm64_u64)(immediate >> 7) << (width - 1);
    arm64_u64 exponent_bit = (immediate >> 6) & 1;
    arm64_u64 exponent;

    if (width == 32)
    {
        exponent = ((!exponent_bit) << 7) | ((exponent_bit ? 0x1FULL : 0) << 2) | ((immediate >> 4) & 0x3);
        return sign | (exponent << 23) | ((arm64_u64)(immediate & 0xF) << 19);
    }

    exponent = ((!exponent_bit) << 10) | ((exponent_bit ? 0xFFULL : 0) << 2) | ((immediate >> 4) & 0x3);
    return sign | (exponent << 52) | ((arm64_u64)(immediate & 0xF) << 48);
}

static arm64_u64 arm64_simd_expand_modified_imm(arm64_u8 immediate, arm64_u8 cmode, arm64_u8 op)
{
    arm64_u64 value = immediate;
    arm64_u8 index;

    switch (cmode)
    {
    case 0:
    case 1:
        return (value << 32) | value;
    case 2:
    case 3:
        return (value << 40) | (value << 8);
    case 4:
    case 5:
        return (value << 48) | (value << 16);
    case 6:
    case 7:
        return (value << 56) | (value << 24);
    case 8:
    case 9:
        return (value << 48) | (value << 32) | (value << 16) | value;
    case 10:
    case 11:
        return (value << 56) | (value << 40) | (value << 24) | (value << 8);
    case 12:
        return (value << 40) | (value << 8) | 0x000000FF000000FFULL;
    case 13:
        return (value << 48) | (value << 16) | 0x0000FFFF0000FFFFULL;
    case 14:
        if (!op)
        {
            value |= value << 8;
            value |= value << 16;
            return value | (value << 32);
        }
        value = 0;
        for (index = 0; index < 8; index++)
            if (immediate & (1U << index)) value |= 0xFFULL << (index * 8);
        return value;
    default:
        if (!op)
        {
            value = arm64_simd_expand_fp_imm(immediate, 32);
            return value | (value << 32);
        }
        return arm64_simd_expand_fp_imm(immediate, 64);
    }
}

static enum arm64_decode_status arm64_simd_decode_modified_immediate(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 cmode = (raw >> 12) & 0xF;
    arm64_u8 immediate = (((raw >> 16) & 0x7) << 5) | ((raw >> 5) & 0x1F);
    arm64_u8 op = (raw >> 29) & 1;

    decoded->operands.simd.group = ARM64_SIMD_GROUP_VECTOR_MODIFIED_IMMEDIATE;
    decoded->operands.simd.immediate = immediate;
    decoded->operands.simd.expanded_immediate = arm64_simd_expand_modified_imm(immediate, cmode, op);
    decoded->operand_width = raw & (1U << 30) ? 128 : 64;

    if (cmode < 8) decoded->operands.simd.element_width = 32;
    else if (cmode < 12) decoded->operands.simd.element_width = 16;
    else if (cmode < 14) decoded->operands.simd.element_width = 32;
    else if (cmode == 14) decoded->operands.simd.element_width = op ? 64 : 8;
    else decoded->operands.simd.element_width = op ? 64 : 32;

    if (cmode < 12 && (cmode & 1)) decoded->operands.simd.operation = op ? ARM64_SIMD_OP_BIC_IMMEDIATE : ARM64_SIMD_OP_ORR_IMMEDIATE;
    else if (cmode == 15)
    {
        if (op && decoded->operand_width != 128) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMOV_IMMEDIATE;
    }
    else if (cmode == 14 && op) decoded->operands.simd.operation = ARM64_SIMD_OP_MOVI;
    else decoded->operands.simd.operation = op ? ARM64_SIMD_OP_MVNI : ARM64_SIMD_OP_MOVI;

    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_copy(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 imm5 = (raw >> 16) & 0x1F;
    arm64_u8 imm4 = (raw >> 11) & 0xF;
    arm64_u8 size;
    arm64_u8 q = (raw >> 30) & 1;

    if (!imm5) return ARM64_DECODE_UNALLOCATED;
    size = (arm64_u8)__builtin_ctz(imm5);
    if (size > 3) return ARM64_DECODE_UNALLOCATED;

    decoded->operands.simd.group = ARM64_SIMD_GROUP_VECTOR_COPY;
    decoded->operands.simd.element_width = 8U << size;
    if (!q && decoded->operands.simd.element_width == 64) return ARM64_DECODE_UNALLOCATED;
    decoded->operands.simd.lane_index = imm5 >> (size + 1);

    if (raw & (1U << 29))
    {
        if (!q) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = ARM64_SIMD_OP_INS_ELEMENT;
        decoded->operands.simd.source_lane_index = imm4 >> size;
        decoded->operand_width = 128;
        return ARM64_DECODE_OK;
    }

    switch (imm4)
    {
    case 0:
        decoded->operands.simd.operation = ARM64_SIMD_OP_DUP_ELEMENT;
        decoded->operand_width = q ? 128 : 64;
        break;
    case 1:
        decoded->operands.simd.operation = ARM64_SIMD_OP_DUP_GENERAL;
        decoded->operand_width = q ? 128 : 64;
        break;
    case 3:
        if (!q) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = ARM64_SIMD_OP_INS_GENERAL;
        decoded->operand_width = 128;
        break;
    case 5:
        if ((!q && decoded->operands.simd.element_width > 16) || (q && decoded->operands.simd.element_width > 32)) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = ARM64_SIMD_OP_SMOV;
        decoded->operand_width = q ? 64 : 32;
        break;
    case 7:
        if ((!q && decoded->operands.simd.element_width > 32) || (q && decoded->operands.simd.element_width != 64)) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = ARM64_SIMD_OP_UMOV;
        decoded->operand_width = q ? 64 : 32;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_scalar_copy(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 imm5 = (raw >> 16) & 0x1F;
    arm64_u8 size;

    if (!imm5) return ARM64_DECODE_UNALLOCATED;
    size = (arm64_u8)__builtin_ctz(imm5);
    if (size > 3) return ARM64_DECODE_UNALLOCATED;

    decoded->operands.simd.group = ARM64_SIMD_GROUP_SCALAR_COPY;
    decoded->operands.simd.operation = ARM64_SIMD_OP_DUP_ELEMENT;
    decoded->operands.simd.element_width = 8U << size;
    decoded->operands.simd.lane_index = imm5 >> (size + 1);
    decoded->operand_width = decoded->operands.simd.element_width;
    return ARM64_DECODE_OK;
}

static arm64_u8 arm64_simd_is_fp_by_element(arm64_u32 raw)
{
    switch (raw & 0xDF000400U)
    {
    case 0x0F000000U:
    case 0x4F000000U:
    case 0x5F000000U:
        return 1;
    default:
        return 0;
    }
}

static enum arm64_decode_status arm64_simd_decode_fp_by_element(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 size = (raw >> 22) & 0x3;
    arm64_u8 opcode = (raw >> 12) & 0xF;
    arm64_u8 scalar = (raw >> 28) & 1;
    arm64_u8 q = (raw >> 30) & 1;
    arm64_u8 u = (raw >> 29) & 1;
    arm64_u8 h = (raw >> 11) & 1;
    arm64_u8 l = (raw >> 21) & 1;
    arm64_u8 m = (raw >> 20) & 1;

    switch (opcode)
    {
    case 1:
        if (u) return ARM64_DECODE_NO_MATCH;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMLA_BY_ELEMENT;
        break;
    case 5:
        if (u) return ARM64_DECODE_NO_MATCH;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMLS_BY_ELEMENT;
        break;
    case 9:
        decoded->operands.simd.operation = u ? ARM64_SIMD_OP_FMULX_BY_ELEMENT : ARM64_SIMD_OP_FMUL_BY_ELEMENT;
        break;
    default:
        return ARM64_DECODE_NO_MATCH;
    }

    if (size == 1) return ARM64_DECODE_UNALLOCATED;
    if (!scalar && size == 3 && !q) return ARM64_DECODE_UNALLOCATED;
    if (size == 3 && l) return ARM64_DECODE_UNALLOCATED;

    decoded->operands.simd.group = ARM64_SIMD_GROUP_FP_BY_ELEMENT;
    decoded->operands.simd.element_width = size == 0 ? 16 : 8U << size;
    decoded->operand_width = scalar ? decoded->operands.simd.element_width : q ? 128 : 64;

    if (size == 0)
    {
        decoded->rm = (raw >> 16) & 0xF;
        decoded->operands.simd.lane_index = (h << 2) | (l << 1) | m;
    }
    else
    {
        decoded->rm = (m << 4) | ((raw >> 16) & 0xF);
        decoded->operands.simd.lane_index = size == 2 ? (h << 1) | l : h;
    }

    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_integer_3reg(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 signature = raw & 0x9F20FC00U;
    arm64_u8 unsigned_operation = (raw >> 29) & 1;
    arm64_u8 size = (raw >> 22) & 0x3;

    switch (signature)
    {
    case 0x0E208400U:
        decoded->operands.simd.operation = unsigned_operation ? ARM64_SIMD_OP_SUB : ARM64_SIMD_OP_ADD;
        break;
    case 0x0E208C00U:
        if (!unsigned_operation) return ARM64_DECODE_NO_MATCH;
        decoded->operands.simd.operation = ARM64_SIMD_OP_CMEQ;
        break;
    case 0x0E203400U:
        decoded->operands.simd.operation = unsigned_operation ? ARM64_SIMD_OP_CMHI : ARM64_SIMD_OP_CMGT;
        break;
    case 0x0E203C00U:
        decoded->operands.simd.operation = unsigned_operation ? ARM64_SIMD_OP_CMHS : ARM64_SIMD_OP_CMGE;
        break;
    default:
        return ARM64_DECODE_NO_MATCH;
    }

    decoded->operand_width = raw & (1U << 30) ? 128 : 64;
    decoded->operands.simd.element_width = 8U << size;
    if (decoded->operand_width == 64 && decoded->operands.simd.element_width == 64) return ARM64_DECODE_UNALLOCATED;
    decoded->operands.simd.group = ARM64_SIMD_GROUP_VECTOR_3REG;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_permute(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 opcode = (raw >> 12) & 0x7;
    arm64_u8 size = (raw >> 22) & 0x3;
    arm64_u8 q = (raw >> 30) & 1;

    if ((raw & 0xBF208C00U) != 0x0E000800U) return ARM64_DECODE_NO_MATCH;

    switch (opcode)
    {
    case 1:
        decoded->operands.simd.operation = ARM64_SIMD_OP_UZP1;
        break;
    case 2:
        decoded->operands.simd.operation = ARM64_SIMD_OP_TRN1;
        break;
    case 3:
        decoded->operands.simd.operation = ARM64_SIMD_OP_ZIP1;
        break;
    case 5:
        decoded->operands.simd.operation = ARM64_SIMD_OP_UZP2;
        break;
    case 6:
        decoded->operands.simd.operation = ARM64_SIMD_OP_TRN2;
        break;
    case 7:
        decoded->operands.simd.operation = ARM64_SIMD_OP_ZIP2;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    if (!q && size == 3) return ARM64_DECODE_UNALLOCATED;
    decoded->operands.simd.group = ARM64_SIMD_GROUP_VECTOR_PERMUTE;
    decoded->operand_width = q ? 128 : 64;
    decoded->operands.simd.element_width = 8U << size;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_logical(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 opcode = (((raw >> 29) & 1) << 2) | ((raw >> 22) & 0x3);

    if ((raw & 0x9F20FC00U) != 0x0E201C00U) return ARM64_DECODE_NO_MATCH;

    switch (opcode)
    {
    case 0:
        decoded->operands.simd.operation = ARM64_SIMD_OP_AND_VECTOR;
        break;
    case 1:
        decoded->operands.simd.operation = ARM64_SIMD_OP_BIC_VECTOR;
        break;
    case 2:
        decoded->operands.simd.operation = ARM64_SIMD_OP_ORR_VECTOR;
        break;
    case 3:
        decoded->operands.simd.operation = ARM64_SIMD_OP_ORN_VECTOR;
        break;
    case 4:
        decoded->operands.simd.operation = ARM64_SIMD_OP_EOR_VECTOR;
        break;
    case 5:
        decoded->operands.simd.operation = ARM64_SIMD_OP_BSL_VECTOR;
        break;
    case 6:
        decoded->operands.simd.operation = ARM64_SIMD_OP_BIT_VECTOR;
        break;
    case 7:
        decoded->operands.simd.operation = ARM64_SIMD_OP_BIF_VECTOR;
        break;
    }

    decoded->operands.simd.group = ARM64_SIMD_GROUP_VECTOR_LOGICAL;
    decoded->operand_width = raw & (1U << 30) ? 128 : 64;
    decoded->operands.simd.element_width = 8;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_fp_accumulate(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 signature = raw & 0xBFE0FC00U;
    arm64_u8 q = (raw >> 30) & 1;

    switch (signature)
    {
    case 0x0E400C00U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMLA_VECTOR;
        decoded->operands.simd.element_width = 16;
        break;
    case 0x0EC00C00U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMLS_VECTOR;
        decoded->operands.simd.element_width = 16;
        break;
    default:
        signature = raw & 0xBFA0FC00U;
        if (signature == 0x0E20CC00U) decoded->operands.simd.operation = ARM64_SIMD_OP_FMLA_VECTOR;
        else if (signature == 0x0EA0CC00U) decoded->operands.simd.operation = ARM64_SIMD_OP_FMLS_VECTOR;
        else return ARM64_DECODE_NO_MATCH;

        decoded->operands.simd.element_width = raw & (1U << 22) ? 64 : 32;
        if (!q && decoded->operands.simd.element_width == 64) return ARM64_DECODE_UNALLOCATED;
        break;
    }

    decoded->operands.simd.group = ARM64_SIMD_GROUP_VECTOR_3REG;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_fp_compare_zero(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 shape = raw & 0x00FF0000U;
    arm64_u32 relation = raw & 0x2000F000U;
    arm64_u8 scalar = (raw >> 28) & 1;
    arm64_u8 q = (raw >> 30) & 1;

    if ((raw & 0x8F000C00U) != 0x0E000800U) return ARM64_DECODE_NO_MATCH;

    switch (shape)
    {
    case 0x00F80000U:
        decoded->operands.simd.element_width = 16;
        break;
    case 0x00A00000U:
        decoded->operands.simd.element_width = 32;
        break;
    case 0x00E00000U:
        decoded->operands.simd.element_width = 64;
        break;
    default:
        return ARM64_DECODE_NO_MATCH;
    }

    switch (relation)
    {
    case 0x0000D000U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMEQ_ZERO;
        break;
    case 0x2000C000U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMGE_ZERO;
        break;
    case 0x0000C000U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMGT_ZERO;
        break;
    case 0x2000D000U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMLE_ZERO;
        break;
    case 0x0000E000U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMLT_ZERO;
        break;
    default:
        return ARM64_DECODE_NO_MATCH;
    }

    if (scalar)
    {
        if (!q) return ARM64_DECODE_NO_MATCH;
        decoded->operand_width = decoded->operands.simd.element_width;
    }
    else
    {
        if (!q && decoded->operands.simd.element_width == 64) return ARM64_DECODE_UNALLOCATED;
        decoded->operand_width = q ? 128 : 64;
    }

    decoded->operands.simd.group = ARM64_SIMD_GROUP_FP_COMPARE_ZERO;
    return ARM64_DECODE_OK;
}

static arm64_u8 arm64_simd_is_shift_immediate(arm64_u32 raw)
{
    return (raw & 0x9F800000U) == 0x0F000000U && ((raw >> 19) & 0xF) != 0;
}

static enum arm64_decode_status arm64_simd_decode_shift_immediate(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 immh = (raw >> 19) & 0xF;
    arm64_u8 immb = (raw >> 16) & 0x7;
    arm64_u8 opcode = (raw >> 11) & 0x1F;
    arm64_u8 element_width;
    arm64_u8 encoded_immediate;

    if (opcode == 0 && (raw & (1U << 10))) decoded->operands.simd.operation = raw & (1U << 29) ? ARM64_SIMD_OP_USHR : ARM64_SIMD_OP_SSHR;
    else if (opcode == 10 && !(raw & (1U << 29)))
    {
        if (!(raw & (1U << 10))) return ((raw >> 22) & 0x3) == 1 ? ARM64_DECODE_UNALLOCATED : ARM64_DECODE_NO_MATCH;
        decoded->operands.simd.operation = ARM64_SIMD_OP_SHL;
    }
    else return ARM64_DECODE_NO_MATCH;
    if (!immh) return ARM64_DECODE_UNALLOCATED;

    element_width = 8U << (31 - __builtin_clz(immh));
    decoded->operand_width = raw & (1U << 30) ? 128 : 64;
    if (decoded->operand_width == 64 && element_width == 64) return ARM64_DECODE_UNALLOCATED;
    encoded_immediate = (immh << 3) | immb;
    decoded->operands.simd.group = ARM64_SIMD_GROUP_VECTOR_SHIFT_IMMEDIATE;
    decoded->operands.simd.element_width = element_width;
    decoded->operands.simd.immediate = decoded->operands.simd.operation == ARM64_SIMD_OP_SHL ? encoded_immediate - element_width : 2 * element_width - encoded_immediate;
    return ARM64_DECODE_OK;
}

static enum arm64_simd_operation arm64_simd_decode_conversion(arm64_u32 raw, struct arm64_decoded_insn *decoded, arm64_u8 *unallocated)
{
    arm64_u32 signature = raw & 0xFFFFFC00U;
    arm64_u32 rounding = (raw >> 19) & 0x3;
    arm64_u32 opcode = (raw >> 16) & 0x7;
    enum arm64_simd_operation operation;

    switch (signature)
    {
    case 0x1E220000U:
        decoded->operand_width = 32;
        decoded->operands.simd.element_width = 32;
        return ARM64_SIMD_OP_SCVTF_S_W;
    case 0x9E220000U:
        decoded->operand_width = 32;
        decoded->operands.simd.element_width = 64;
        return ARM64_SIMD_OP_SCVTF_S_X;
    case 0x1E620000U:
        decoded->operand_width = 64;
        decoded->operands.simd.element_width = 32;
        return ARM64_SIMD_OP_SCVTF_D_W;
    case 0x9E620000U:
        decoded->operand_width = 64;
        decoded->operands.simd.element_width = 64;
        return ARM64_SIMD_OP_SCVTF_D_X;
    case 0x1E230000U:
        decoded->operand_width = 32;
        decoded->operands.simd.element_width = 32;
        return ARM64_SIMD_OP_UCVTF_S_W;
    case 0x9E230000U:
        decoded->operand_width = 32;
        decoded->operands.simd.element_width = 64;
        return ARM64_SIMD_OP_UCVTF_S_X;
    case 0x1E630000U:
        decoded->operand_width = 64;
        decoded->operands.simd.element_width = 32;
        return ARM64_SIMD_OP_UCVTF_D_W;
    case 0x9E630000U:
        decoded->operand_width = 64;
        decoded->operands.simd.element_width = 64;
        return ARM64_SIMD_OP_UCVTF_D_X;
    case 0x1E624000U:
        decoded->operand_width = 32;
        decoded->operands.simd.element_width = 64;
        return ARM64_SIMD_OP_FCVT_S_D;
    case 0x1E22C000U:
        decoded->operand_width = 64;
        decoded->operands.simd.element_width = 32;
        return ARM64_SIMD_OP_FCVT_D_S;
    default:
        break;
    }

    /* Normalize U, Q, scalar/vector, element size, and register fields. */
    signature = raw & 0x8FBFFC00U;
    switch (signature)
    {
    case 0x0E21A800U:
        decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_NEAREST_EVEN;
        operation = raw & (1U << 29) ? ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD : ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD;
        break;
    case 0x0EA1A800U:
        decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_PLUS_INFINITY;
        operation = raw & (1U << 29) ? ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD : ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD;
        break;
    case 0x0E21B800U:
        decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_MINUS_INFINITY;
        operation = raw & (1U << 29) ? ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD : ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD;
        break;
    case 0x0E21C800U:
        decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_NEAREST_AWAY;
        operation = raw & (1U << 29) ? ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD : ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD;
        break;
    case 0x0EA1B800U:
        decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_ZERO;
        operation = raw & (1U << 29) ? ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD : ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD;
        break;
    case 0x0E21D800U:
        operation = raw & (1U << 29) ? ARM64_SIMD_OP_UCVTF_SIMD : ARM64_SIMD_OP_SCVTF_SIMD;
        break;
    default:
        operation = ARM64_SIMD_OP_NONE;
        break;
    }

    if (operation != ARM64_SIMD_OP_NONE)
    {
        decoded->operands.simd.element_width = raw & (1U << 22) ? 64 : 32;
        if (raw & (1U << 28))
        {
            if (!(raw & (1U << 30)))
            {
                *unallocated = 1;
                return ARM64_SIMD_OP_NONE;
            }
            decoded->operand_width = decoded->operands.simd.element_width;
        }
        else
        {
            decoded->operand_width = raw & (1U << 30) ? 128 : 64;
            if (decoded->operand_width == 64 && decoded->operands.simd.element_width == 64)
            {
                *unallocated = 1;
                return ARM64_SIMD_OP_NONE;
            }
        }
        return operation;
    }

    if ((raw & 0x7F20FC00U) != 0x1E200000U) return ARM64_SIMD_OP_NONE;
    if (((raw >> 22) & 0x3) > 1) return ARM64_SIMD_OP_NONE;
    if (opcode <= 1)
    {
        static const enum arm64_fp_rounding_mode rounding_modes[] = {
            ARM64_FP_ROUND_NEAREST_EVEN,
            ARM64_FP_ROUND_PLUS_INFINITY,
            ARM64_FP_ROUND_MINUS_INFINITY,
            ARM64_FP_ROUND_ZERO,
        };

        decoded->operands.simd.rounding_mode = rounding_modes[rounding];
    }
    else if (rounding == 0 && (opcode == 4 || opcode == 5))
    {
        decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_NEAREST_AWAY;
    }
    else
    {
        return ARM64_SIMD_OP_NONE;
    }

    decoded->operand_width = raw & 0x80000000U ? 64 : 32;
    decoded->operands.simd.element_width = raw & 0x00400000U ? 64 : 32;
    return (opcode & 1) ? ARM64_SIMD_OP_FCVT_TO_UNSIGNED : ARM64_SIMD_OP_FCVT_TO_SIGNED;
}

static enum arm64_simd_operation arm64_simd_decode_vector_3reg(arm64_u32 signature)
{
    switch (signature)
    {
    case 0x0E20D400U:
        return ARM64_SIMD_OP_FADD_V2S;
    case 0x4E20D400U:
        return ARM64_SIMD_OP_FADD_V4S;
    case 0x4E60D400U:
        return ARM64_SIMD_OP_FADD_V2D;
    case 0x0EA0D400U:
        return ARM64_SIMD_OP_FSUB_V2S;
    case 0x4EA0D400U:
        return ARM64_SIMD_OP_FSUB_V4S;
    case 0x4EE0D400U:
        return ARM64_SIMD_OP_FSUB_V2D;
    case 0x2E20DC00U:
        return ARM64_SIMD_OP_FMUL_V2S;
    case 0x6E20DC00U:
        return ARM64_SIMD_OP_FMUL_V4S;
    case 0x6E60DC00U:
        return ARM64_SIMD_OP_FMUL_V2D;
    case 0x2E20FC00U:
        return ARM64_SIMD_OP_FDIV_V2S;
    case 0x6E20FC00U:
        return ARM64_SIMD_OP_FDIV_V4S;
    case 0x6E60FC00U:
        return ARM64_SIMD_OP_FDIV_V2D;
    case 0x0E20F400U:
        return ARM64_SIMD_OP_FMAX_V2S;
    case 0x4E20F400U:
        return ARM64_SIMD_OP_FMAX_V4S;
    case 0x4E60F400U:
        return ARM64_SIMD_OP_FMAX_V2D;
    case 0x0EA0F400U:
        return ARM64_SIMD_OP_FMIN_V2S;
    case 0x4EA0F400U:
        return ARM64_SIMD_OP_FMIN_V4S;
    case 0x4EE0F400U:
        return ARM64_SIMD_OP_FMIN_V2D;
    case 0x0E20C400U:
        return ARM64_SIMD_OP_FMAXNM_V2S;
    case 0x4E20C400U:
        return ARM64_SIMD_OP_FMAXNM_V4S;
    case 0x4E60C400U:
        return ARM64_SIMD_OP_FMAXNM_V2D;
    case 0x0EA0C400U:
        return ARM64_SIMD_OP_FMINNM_V2S;
    case 0x4EA0C400U:
        return ARM64_SIMD_OP_FMINNM_V4S;
    case 0x4EE0C400U:
        return ARM64_SIMD_OP_FMINNM_V2D;
    case 0x2E20D400U:
        return ARM64_SIMD_OP_FADDP_V2S;
    case 0x6E20D400U:
        return ARM64_SIMD_OP_FADDP_V4S;
    case 0x6E60D400U:
        return ARM64_SIMD_OP_FADDP_V2D;
    case 0x2E20F400U:
        return ARM64_SIMD_OP_FMAXP_V2S;
    case 0x6E20F400U:
        return ARM64_SIMD_OP_FMAXP_V4S;
    case 0x6E60F400U:
        return ARM64_SIMD_OP_FMAXP_V2D;
    case 0x2EA0F400U:
        return ARM64_SIMD_OP_FMINP_V2S;
    case 0x6EA0F400U:
        return ARM64_SIMD_OP_FMINP_V4S;
    case 0x6EE0F400U:
        return ARM64_SIMD_OP_FMINP_V2D;
    case 0x0E20E400U:
        return ARM64_SIMD_OP_FCMEQ_V2S;
    case 0x4E20E400U:
        return ARM64_SIMD_OP_FCMEQ_V4S;
    case 0x6E20E400U:
        return ARM64_SIMD_OP_FCMGE_V4S;
    case 0x6EA0E400U:
        return ARM64_SIMD_OP_FCMGT_V4S;
    default:
        return ARM64_SIMD_OP_NONE;
    }
}

static enum arm64_simd_operation arm64_simd_decode_vector_2reg(arm64_u32 signature)
{
    switch (signature)
    {
    case 0x0EA0F800U:
        return ARM64_SIMD_OP_FABS_V2S;
    case 0x4EA0F800U:
        return ARM64_SIMD_OP_FABS_V4S;
    case 0x4EE0F800U:
        return ARM64_SIMD_OP_FABS_V2D;
    case 0x2EA0F800U:
        return ARM64_SIMD_OP_FNEG_V2S;
    case 0x6EA0F800U:
        return ARM64_SIMD_OP_FNEG_V4S;
    case 0x6EE0F800U:
        return ARM64_SIMD_OP_FNEG_V2D;
    case 0x2EA1F800U:
        return ARM64_SIMD_OP_FSQRT_V2S;
    case 0x6EA1F800U:
        return ARM64_SIMD_OP_FSQRT_V4S;
    case 0x6EE1F800U:
        return ARM64_SIMD_OP_FSQRT_V2D;
    case 0x0EA00800U:
        return ARM64_SIMD_OP_REV64_V2S;
    case 0x4E200800U:
        return ARM64_SIMD_OP_REV64_V16B;
    case 0x4E600800U:
        return ARM64_SIMD_OP_REV64_V8H;
    case 0x6E200800U:
        return ARM64_SIMD_OP_REV32_V16B;
    case 0x4E201800U:
        return ARM64_SIMD_OP_REV16_V16B;
    case 0x7E30D800U:
        return ARM64_SIMD_OP_FADDP_S_V2S;
    case 0x7E70D800U:
        return ARM64_SIMD_OP_FADDP_D_V2D;
    case 0x6E30F800U:
        return ARM64_SIMD_OP_FMAXV_S_V4S;
    case 0x6EB0F800U:
        return ARM64_SIMD_OP_FMINV_S_V4S;
    default:
        return ARM64_SIMD_OP_NONE;
    }
}

static void arm64_simd_set_operation_shape(struct arm64_decoded_insn *decoded)
{
    switch (decoded->operands.simd.operation)
    {
    case ARM64_SIMD_OP_FADD_V2S:
    case ARM64_SIMD_OP_FSUB_V2S:
    case ARM64_SIMD_OP_FMUL_V2S:
    case ARM64_SIMD_OP_FDIV_V2S:
    case ARM64_SIMD_OP_FMAX_V2S:
    case ARM64_SIMD_OP_FMIN_V2S:
    case ARM64_SIMD_OP_FMAXNM_V2S:
    case ARM64_SIMD_OP_FMINNM_V2S:
    case ARM64_SIMD_OP_FADDP_V2S:
    case ARM64_SIMD_OP_FMAXP_V2S:
    case ARM64_SIMD_OP_FMINP_V2S:
    case ARM64_SIMD_OP_FCMEQ_V2S:
    case ARM64_SIMD_OP_FABS_V2S:
    case ARM64_SIMD_OP_FNEG_V2S:
    case ARM64_SIMD_OP_FSQRT_V2S:
    case ARM64_SIMD_OP_REV64_V2S:
        decoded->operand_width = 64;
        decoded->operands.simd.element_width = 32;
        break;
    case ARM64_SIMD_OP_FADD_V4S:
    case ARM64_SIMD_OP_FSUB_V4S:
    case ARM64_SIMD_OP_FMUL_V4S:
    case ARM64_SIMD_OP_FDIV_V4S:
    case ARM64_SIMD_OP_FMAX_V4S:
    case ARM64_SIMD_OP_FMIN_V4S:
    case ARM64_SIMD_OP_FMAXNM_V4S:
    case ARM64_SIMD_OP_FMINNM_V4S:
    case ARM64_SIMD_OP_FADDP_V4S:
    case ARM64_SIMD_OP_FMAXP_V4S:
    case ARM64_SIMD_OP_FMINP_V4S:
    case ARM64_SIMD_OP_FCMEQ_V4S:
    case ARM64_SIMD_OP_FCMGE_V4S:
    case ARM64_SIMD_OP_FCMGT_V4S:
    case ARM64_SIMD_OP_FABS_V4S:
    case ARM64_SIMD_OP_FNEG_V4S:
    case ARM64_SIMD_OP_FSQRT_V4S:
        decoded->operand_width = 128;
        decoded->operands.simd.element_width = 32;
        break;
    case ARM64_SIMD_OP_FADD_V2D:
    case ARM64_SIMD_OP_FSUB_V2D:
    case ARM64_SIMD_OP_FMUL_V2D:
    case ARM64_SIMD_OP_FDIV_V2D:
    case ARM64_SIMD_OP_FMAX_V2D:
    case ARM64_SIMD_OP_FMIN_V2D:
    case ARM64_SIMD_OP_FMAXNM_V2D:
    case ARM64_SIMD_OP_FMINNM_V2D:
    case ARM64_SIMD_OP_FADDP_V2D:
    case ARM64_SIMD_OP_FMAXP_V2D:
    case ARM64_SIMD_OP_FMINP_V2D:
    case ARM64_SIMD_OP_FABS_V2D:
    case ARM64_SIMD_OP_FNEG_V2D:
    case ARM64_SIMD_OP_FSQRT_V2D:
        decoded->operand_width = 128;
        decoded->operands.simd.element_width = 64;
        break;
    case ARM64_SIMD_OP_REV64_V16B:
    case ARM64_SIMD_OP_REV32_V16B:
    case ARM64_SIMD_OP_REV16_V16B:
        decoded->operand_width = 128;
        decoded->operands.simd.element_width = 8;
        break;
    case ARM64_SIMD_OP_REV64_V8H:
        decoded->operand_width = 128;
        decoded->operands.simd.element_width = 16;
        break;
    case ARM64_SIMD_OP_FADDP_S_V2S:
    case ARM64_SIMD_OP_FMAXV_S_V4S:
    case ARM64_SIMD_OP_FMINV_S_V4S:
        decoded->operand_width = 32;
        decoded->operands.simd.element_width = 32;
        break;
    case ARM64_SIMD_OP_FADDP_D_V2D:
        decoded->operand_width = 64;
        decoded->operands.simd.element_width = 64;
        break;
    default:
        break;
    }
}

/*
所有 FP/AdvSIMD 编码签名只在本文件内匹配；对外只返回稳定的 group 和
arm64_simd_operation，调用方不需要了解原始 opcode/signature。
*/
enum arm64_decode_status arm64_decode_simd(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 iclass = (raw >> 25) & 0xF;
    arm64_u32 signature;
    arm64_u32 type;
    arm64_u8 conversion_unallocated = 0;
    enum arm64_simd_operation operation;

    if (iclass != 0x7 && iclass != 0xF) return ARM64_DECODE_NO_MATCH;

    decoded->insn_class = ARM64_INSN_CLASS_DATA_PROCESSING_SIMD_FP;
    decoded->opcode = ARM64_OP_FP_SIMD;
    decoded->flags = ARM64_INSN_FLAG_FP;
    arm64_decode_simd_registers(raw, decoded);

    if ((raw & 0xFF201FE0U) == 0x1E201000U)
    {
        arm64_u8 immediate = (raw >> 13) & 0xFF;

        type = (raw >> 22) & 0x3;
        if (type > 1) return ARM64_DECODE_UNSUPPORTED;
        decoded->operands.simd.group = ARM64_SIMD_GROUP_SCALAR_IMMEDIATE;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMOV_IMMEDIATE;
        decoded->operands.simd.immediate = immediate;
        decoded->operand_width = type ? 64 : 32;
        decoded->operands.simd.element_width = decoded->operand_width;
        decoded->operands.simd.expanded_immediate = arm64_simd_expand_fp_imm(immediate, decoded->operand_width);
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x9FF80C00U) == 0x0F000400U) return arm64_simd_decode_modified_immediate(raw, decoded);

    if ((raw & 0xFFE0FC00U) == 0x5E000400U) return arm64_simd_decode_scalar_copy(raw, decoded);

    if ((raw & 0x9FE08400U) == 0x0E000400U) return arm64_simd_decode_copy(raw, decoded);

    {
        enum arm64_decode_status permute_status = arm64_simd_decode_permute(raw, decoded);

        if (permute_status != ARM64_DECODE_NO_MATCH) return permute_status;
    }

    {
        enum arm64_decode_status logical_status = arm64_simd_decode_logical(raw, decoded);

        if (logical_status != ARM64_DECODE_NO_MATCH) return logical_status;
    }

    {
        enum arm64_decode_status accumulate_status = arm64_simd_decode_fp_accumulate(raw, decoded);

        if (accumulate_status != ARM64_DECODE_NO_MATCH) return accumulate_status;
    }

    if (arm64_simd_is_fp_by_element(raw))
    {
        enum arm64_decode_status element_status = arm64_simd_decode_fp_by_element(raw, decoded);

        if (element_status != ARM64_DECODE_NO_MATCH) return element_status;
    }

    if (arm64_simd_is_shift_immediate(raw))
    {
        enum arm64_decode_status shift_status = arm64_simd_decode_shift_immediate(raw, decoded);

        if (shift_status != ARM64_DECODE_NO_MATCH) return shift_status;
    }

    {
        enum arm64_decode_status compare_zero_status = arm64_simd_decode_fp_compare_zero(raw, decoded);

        if (compare_zero_status != ARM64_DECODE_NO_MATCH) return compare_zero_status;
    }

    {
        enum arm64_decode_status integer_status = arm64_simd_decode_integer_3reg(raw, decoded);

        if (integer_status != ARM64_DECODE_NO_MATCH) return integer_status;
    }

    signature = raw & 0xFFFFFC00U;
    operation = arm64_simd_decode_conversion(raw, decoded, &conversion_unallocated);
    if (operation != ARM64_SIMD_OP_NONE)
    {
        decoded->operands.simd.group = ARM64_SIMD_GROUP_CONVERT;
        decoded->operands.simd.operation = operation;
        return ARM64_DECODE_OK;
    }
    if (conversion_unallocated) return ARM64_DECODE_UNALLOCATED;

    type = (raw >> 22) & 0x3;
    if ((raw & 0xFF200C00U) == 0x1E200800U)
    {
        arm64_u32 opcode = (raw >> 12) & 0xF;

        if (type > 1) return ARM64_DECODE_UNSUPPORTED;
        if (opcode > 8) return ARM64_DECODE_UNSUPPORTED;
        decoded->operands.simd.group = ARM64_SIMD_GROUP_SCALAR_2SOURCE;
        switch (opcode)
        {
        case 0:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FMUL;
            break;
        case 1:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FDIV;
            break;
        case 2:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FADD;
            break;
        case 3:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FSUB;
            break;
        case 4:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FMAX;
            break;
        case 5:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FMIN;
            break;
        case 6:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FMAXNM;
            break;
        case 7:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FMINNM;
            break;
        default:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FNMUL;
            break;
        }
        decoded->operand_width = type ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF207C00U) == 0x1E204000U)
    {
        arm64_u32 opcode = (raw >> 15) & 0x3F;

        if (type > 1) return ARM64_DECODE_UNSUPPORTED;
        decoded->operands.simd.group = ARM64_SIMD_GROUP_SCALAR_1SOURCE;
        switch (opcode)
        {
        case 0:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FMOV;
            break;
        case 1:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FABS;
            break;
        case 2:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FNEG;
            break;
        case 3:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FSQRT;
            break;
        case 8:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FRINT;
            decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_NEAREST_EVEN;
            break;
        case 9:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FRINT;
            decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_PLUS_INFINITY;
            break;
        case 10:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FRINT;
            decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_MINUS_INFINITY;
            break;
        case 11:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FRINT;
            decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_ZERO;
            break;
        case 12:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FRINT;
            decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_NEAREST_AWAY;
            break;
        case 14:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FRINT;
            decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_CURRENT_EXACT;
            break;
        case 15:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FRINT;
            decoded->operands.simd.rounding_mode = ARM64_FP_ROUND_CURRENT;
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        decoded->operand_width = type ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF000000U) == 0x1F000000U)
    {
        if (type == 2) return ARM64_DECODE_UNALLOCATED;
        if (type == 3) return ARM64_DECODE_UNSUPPORTED;
        decoded->operands.simd.group = ARM64_SIMD_GROUP_SCALAR_3SOURCE;
        switch ((((raw >> 21) & 1) << 1) | ((raw >> 15) & 1))
        {
        case 0:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FMADD;
            break;
        case 1:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FMSUB;
            break;
        case 2:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FNMADD;
            break;
        default:
            decoded->operands.simd.operation = ARM64_SIMD_OP_FNMSUB;
            break;
        }
        decoded->operand_width = type ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF20FC00U) == 0x1E202000U)
    {
        arm64_u32 zero = (raw >> 3) & 1;

        if (type == 2 || (raw & 0x7)) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.group = ARM64_SIMD_GROUP_SCALAR_COMPARE;
        decoded->operands.simd.operation = (raw & (1U << 4)) ? ARM64_SIMD_OP_FCMPE : ARM64_SIMD_OP_FCMP;
        if (zero) decoded->operands.simd.flags |= ARM64_SIMD_FLAG_COMPARE_ZERO;
        decoded->operand_width = type == 3 ? 16 : type ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF200C00U) == 0x1E200400U)
    {
        if (type > 1) return ARM64_DECODE_UNSUPPORTED;
        decoded->operands.simd.group = ARM64_SIMD_GROUP_SCALAR_CONDITIONAL_COMPARE;
        decoded->operands.simd.operation = (raw & (1U << 4)) ? ARM64_SIMD_OP_FCCMPE : ARM64_SIMD_OP_FCCMP;
        decoded->operands.simd.condition = (raw >> 12) & 0xF;
        decoded->operands.simd.immediate = raw & 0xF;
        decoded->operand_width = type ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF200C00U) == 0x1E200C00U)
    {
        if (type > 1) return ARM64_DECODE_UNSUPPORTED;
        decoded->operands.simd.group = ARM64_SIMD_GROUP_SCALAR_SELECT;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCSEL;
        decoded->operands.simd.condition = (raw >> 12) & 0xF;
        decoded->operand_width = type ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FBEFC00U) == 0x1E260000U)
    {
        arm64_u32 sf = (raw >> 31) & 1;

        if (((raw >> 22) & 1) != sf) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.group = ARM64_SIMD_GROUP_FMOV_GENERAL;
        decoded->operands.simd.operation = (raw & (1U << 16)) ? ARM64_SIMD_OP_FMOV_GENERAL_TO_FP : ARM64_SIMD_OP_FMOV_FP_TO_GENERAL;
        decoded->operand_width = sf ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xBFE08400U) == 0x2E000000U)
    {
        decoded->operands.simd.group = ARM64_SIMD_GROUP_EXT;
        decoded->operands.simd.operation = ARM64_SIMD_OP_EXT;
        decoded->operands.simd.immediate = (raw >> 11) & 0xF;
        decoded->operand_width = (raw & (1U << 30)) ? 128 : 64;
        if (decoded->operand_width == 64 && decoded->operands.simd.immediate >= 8) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    signature = raw & 0xFFE0FC00U;
    operation = arm64_simd_decode_vector_3reg(signature);
    if (operation != ARM64_SIMD_OP_NONE)
    {
        decoded->operands.simd.group = ARM64_SIMD_GROUP_VECTOR_3REG;
        decoded->operands.simd.operation = operation;
        arm64_simd_set_operation_shape(decoded);
        return ARM64_DECODE_OK;
    }

    signature = raw & 0xFFFFFC00U;
    operation = arm64_simd_decode_vector_2reg(signature);
    if (operation != ARM64_SIMD_OP_NONE)
    {
        decoded->operands.simd.group = ARM64_SIMD_GROUP_VECTOR_2REG;
        decoded->operands.simd.operation = operation;
        arm64_simd_set_operation_shape(decoded);
        return ARM64_DECODE_OK;
    }

    return ARM64_DECODE_UNSUPPORTED;
}