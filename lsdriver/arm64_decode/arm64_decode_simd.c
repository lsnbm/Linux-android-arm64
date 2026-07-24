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

    if (width == 16)
    {
        exponent = ((!exponent_bit) << 4) | ((exponent_bit ? 0x3ULL : 0) << 2) | ((immediate >> 4) & 0x3);
        return sign | (exponent << 10) | ((arm64_u64)(immediate & 0xF) << 6);
    }

    if (width == 32)
    {
        exponent = ((!exponent_bit) << 7) | ((exponent_bit ? 0x1FULL : 0) << 2) | ((immediate >> 4) & 0x3);
        return sign | (exponent << 23) | ((arm64_u64)(immediate & 0xF) << 19);
    }

    exponent = ((!exponent_bit) << 10) | ((exponent_bit ? 0xFFULL : 0) << 2) | ((immediate >> 4) & 0x3);
    return sign | (exponent << 52) | ((arm64_u64)(immediate & 0xF) << 48);
}

static arm64_u8 arm64_simd_scalar_fp_width(arm64_u32 type)
{
    switch (type)
    {
    case 0: return 32;
    case 1: return 64;
    case 3: return 16;
    default: return 0;
    }
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

    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_IMMEDIATE;
    decoded->operands.simd.immediate = immediate;
    decoded->operands.simd.expanded_immediate = arm64_simd_expand_modified_imm(immediate, cmode, op);
    decoded->operand_width = raw & (1U << 30) ? 128 : 64;

    if (cmode < 8) decoded->operands.simd.element_width = 32;
    else if (cmode < 12) decoded->operands.simd.element_width = 16;
    else if (cmode < 14) decoded->operands.simd.element_width = 32;
    else if (cmode == 14) decoded->operands.simd.element_width = op ? 64 : 8;
    else decoded->operands.simd.element_width = op ? 64 : 32;

    if (cmode < 12 && (cmode & 1)) decoded->operands.simd.operation = op ? ARM64_SIMD_OP_BIC : ARM64_SIMD_OP_ORR;
    else if (cmode == 15)
    {
        if (op && decoded->operand_width != 128) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMOV;
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

    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_COPY;
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

    decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_COPY;
    decoded->operands.simd.operation = ARM64_SIMD_OP_DUP_ELEMENT;
    decoded->operands.simd.element_width = 8U << size;
    decoded->operands.simd.lane_index = imm5 >> (size + 1);
    decoded->operand_width = decoded->operands.simd.element_width;
    return ARM64_DECODE_OK;
}

static arm64_u8 arm64_simd_is_by_element(arm64_u32 raw)
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

static enum arm64_decode_status arm64_simd_decode_extra_by_element(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 size = (raw >> 22) & 0x3;
    arm64_u8 opcode = (raw >> 12) & 0xF;
    arm64_u8 scalar = (raw >> 28) & 1;
    arm64_u8 q = (raw >> 30) & 1;
    arm64_u8 u = (raw >> 29) & 1;
    arm64_u8 h = (raw >> 11) & 1;
    arm64_u8 l = (raw >> 21) & 1;
    arm64_u8 m = (raw >> 20) & 1;

    if (u && (opcode == 13 || opcode == 15))
    {
        if (size != 1 && size != 2) return ARM64_DECODE_UNALLOCATED;

        decoded->operands.simd.form = scalar ? ARM64_SIMD_FORM_SCALAR_BY_ELEMENT : ARM64_SIMD_FORM_VECTOR_BY_ELEMENT;
        decoded->operands.simd.operation = opcode == 13 ? ARM64_SIMD_OP_SQRDMLAH : ARM64_SIMD_OP_SQRDMLSH;
        decoded->operands.simd.element_width = 8U << size;
        decoded->operand_width = scalar ? decoded->operands.simd.element_width : q ? 128 : 64;
        if (size == 1)
        {
            decoded->rm = (raw >> 16) & 0xF;
            decoded->operands.simd.lane_index = (h << 2) | (l << 1) | m;
        }
        else
        {
            decoded->rm = (m << 4) | ((raw >> 16) & 0xF);
            decoded->operands.simd.lane_index = (h << 1) | l;
        }
        return ARM64_DECODE_OK;
    }

    if (scalar) return ARM64_DECODE_NO_MATCH;

    if (opcode == 14)
    {
        if (size != 2) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = u ? ARM64_SIMD_OP_UDOT : ARM64_SIMD_OP_SDOT;
        decoded->operands.simd.element_width = 8;
    }
    else if (!u && opcode == 15)
    {
        switch (size)
        {
        case 0:
            decoded->operands.simd.operation = ARM64_SIMD_OP_SUDOT;
            decoded->operands.simd.element_width = 8;
            break;
        case 1:
            decoded->operands.simd.operation = ARM64_SIMD_OP_BFDOT;
            decoded->operands.simd.element_width = 16;
            break;
        case 2:
            decoded->operands.simd.operation = ARM64_SIMD_OP_USDOT;
            decoded->operands.simd.element_width = 8;
            break;
        case 3:
            decoded->operands.simd.operation = ARM64_SIMD_OP_BFMLAL;
            decoded->operands.simd.element_width = 16;
            decoded->operands.simd.result_element_width = 32;
            decoded->operands.simd.lane_index = (h << 2) | (l << 1) | m;
            decoded->operand_width = 128;
            decoded->rm = (raw >> 16) & 0xF;
            if (q) decoded->operands.simd.flags |= ARM64_SIMD_FLAG_SOURCE_ODD_ELEMENTS;
            decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_BY_ELEMENT;
            return ARM64_DECODE_OK;
        }
    }
    else return ARM64_DECODE_NO_MATCH;

    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_BY_ELEMENT;
    decoded->operands.simd.result_element_width = 32;
    decoded->operands.simd.lane_index = (h << 1) | l;
    decoded->operand_width = q ? 128 : 64;
    decoded->rm = (m << 4) | ((raw >> 16) & 0xF);
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_by_element(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 size = (raw >> 22) & 0x3;
    arm64_u8 opcode = (raw >> 12) & 0xF;
    arm64_u8 scalar = (raw >> 28) & 1;
    arm64_u8 q = (raw >> 30) & 1;
    arm64_u8 u = (raw >> 29) & 1;
    arm64_u8 h = (raw >> 11) & 1;
    arm64_u8 l = (raw >> 21) & 1;
    arm64_u8 m = (raw >> 20) & 1;
    enum arm64_decode_status extra_status = arm64_simd_decode_extra_by_element(raw, decoded);

    if (extra_status != ARM64_DECODE_NO_MATCH) return extra_status;

    if ((raw & 0xBF009400U) == 0x2F001000U)
    {
        if (size == 0 || size == 3) return ARM64_DECODE_UNALLOCATED;
        if (size == 2 && (l || !q)) return ARM64_DECODE_UNALLOCATED;
        if (size == 1 && h && !q) return ARM64_DECODE_UNALLOCATED;

        decoded->operands.simd.form = ARM64_SIMD_FORM_FP_BY_ELEMENT;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMLA;
        decoded->operands.simd.immediate = (raw >> 13) & 0x3;
        decoded->operands.simd.element_width = 8U << size;
        decoded->operands.simd.lane_index = size == 1 ? (h << 1) | l : h;
        decoded->operand_width = q ? 128 : 64;
        decoded->rm = (m << 4) | ((raw >> 16) & 0xF);
        return ARM64_DECODE_OK;
    }

    if ((size & 2) && !(opcode & 3) && ((opcode >> 3) == u))
    {
        if (scalar || size == 3) return ARM64_DECODE_UNALLOCATED;

        decoded->operands.simd.form = ARM64_SIMD_FORM_FP_BY_ELEMENT;
        decoded->operands.simd.operation = opcode & 4 ? ARM64_SIMD_OP_FMLSL : ARM64_SIMD_OP_FMLAL;
        decoded->operands.simd.element_width = 16;
        decoded->operands.simd.result_element_width = 32;
        decoded->operands.simd.lane_index = (h << 2) | (l << 1) | m;
        decoded->operand_width = q ? 128 : 64;
        decoded->rm = (raw >> 16) & 0xF;
        if (u) decoded->operands.simd.flags |= ARM64_SIMD_FLAG_SOURCE_HIGH_HALF;
        return ARM64_DECODE_OK;
    }

    switch (opcode)
    {
    case 1:
        if (u) return ARM64_DECODE_NO_MATCH;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMLA;
        break;
    case 5:
        if (u) return ARM64_DECODE_NO_MATCH;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMLS;
        break;
    case 9:
        decoded->operands.simd.operation = u ? ARM64_SIMD_OP_FMULX : ARM64_SIMD_OP_FMUL;
        break;
    default:
        return ARM64_DECODE_NO_MATCH;
    }

    if (size == 1) return ARM64_DECODE_UNALLOCATED;
    if (!scalar && size == 3 && !q) return ARM64_DECODE_UNALLOCATED;
    if (size == 3 && l) return ARM64_DECODE_UNALLOCATED;

    decoded->operands.simd.form = ARM64_SIMD_FORM_FP_BY_ELEMENT;
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

static enum arm64_decode_status arm64_simd_decode_complex_vector(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 q = (raw >> 30) & 1;
    arm64_u8 size = (raw >> 22) & 0x3;

    if ((raw & 0xBF20E400U) == 0x2E00C400U)
    {
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMLA;
        decoded->operands.simd.immediate = (raw >> 11) & 0x3;
    }
    else if ((raw & 0xBF20EC00U) == 0x2E00E400U)
    {
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCADD;
        decoded->operands.simd.immediate = raw & (1U << 12) ? ARM64_SIMD_ROTATION_270 : ARM64_SIMD_ROTATION_90;
    }
    else return ARM64_DECODE_NO_MATCH;

    if (size == 0 || (!q && size == 3)) return ARM64_DECODE_UNALLOCATED;
    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_COMPLEX_3REG;
    decoded->operands.simd.element_width = 8U << size;
    decoded->operand_width = q ? 128 : 64;
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
    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_PERMUTE;
    decoded->operand_width = q ? 128 : 64;
    decoded->operands.simd.element_width = 8U << size;
    return ARM64_DECODE_OK;
}

static enum arm64_simd_operation arm64_simd_decode_integer_3same_operation(arm64_u8 u, arm64_u8 opcode, arm64_u8 *valid_sizes)
{
    *valid_sizes = 0;
    switch ((u << 5) | opcode)
    {
    case 0x00: *valid_sizes = 0x7; return ARM64_SIMD_OP_SHADD;
    case 0x01: *valid_sizes = 0xF; return ARM64_SIMD_OP_SQADD;
    case 0x02: *valid_sizes = 0x7; return ARM64_SIMD_OP_SRHADD;
    case 0x04: *valid_sizes = 0x7; return ARM64_SIMD_OP_SHSUB;
    case 0x05: *valid_sizes = 0xF; return ARM64_SIMD_OP_SQSUB;
    case 0x06: *valid_sizes = 0xF; return ARM64_SIMD_OP_CMGT;
    case 0x07: *valid_sizes = 0xF; return ARM64_SIMD_OP_CMGE;
    case 0x08: *valid_sizes = 0xF; return ARM64_SIMD_OP_SSHL;
    case 0x09: *valid_sizes = 0xF; return ARM64_SIMD_OP_SQSHL;
    case 0x0A: *valid_sizes = 0xF; return ARM64_SIMD_OP_SRSHL;
    case 0x0B: *valid_sizes = 0xF; return ARM64_SIMD_OP_SQRSHL;
    case 0x0C: *valid_sizes = 0x7; return ARM64_SIMD_OP_SMAX;
    case 0x0D: *valid_sizes = 0x7; return ARM64_SIMD_OP_SMIN;
    case 0x0E: *valid_sizes = 0x7; return ARM64_SIMD_OP_SABD;
    case 0x0F: *valid_sizes = 0x7; return ARM64_SIMD_OP_SABA;
    case 0x10: *valid_sizes = 0xF; return ARM64_SIMD_OP_ADD;
    case 0x11: *valid_sizes = 0xF; return ARM64_SIMD_OP_CMTST;
    case 0x12: *valid_sizes = 0x7; return ARM64_SIMD_OP_MLA;
    case 0x13: *valid_sizes = 0x7; return ARM64_SIMD_OP_MUL;
    case 0x14: *valid_sizes = 0x7; return ARM64_SIMD_OP_SMAXP;
    case 0x15: *valid_sizes = 0x7; return ARM64_SIMD_OP_SMINP;
    case 0x16: *valid_sizes = 0x6; return ARM64_SIMD_OP_SQDMULH;
    case 0x17: *valid_sizes = 0xF; return ARM64_SIMD_OP_ADDP;
    case 0x20: *valid_sizes = 0x7; return ARM64_SIMD_OP_UHADD;
    case 0x21: *valid_sizes = 0xF; return ARM64_SIMD_OP_UQADD;
    case 0x22: *valid_sizes = 0x7; return ARM64_SIMD_OP_URHADD;
    case 0x24: *valid_sizes = 0x7; return ARM64_SIMD_OP_UHSUB;
    case 0x25: *valid_sizes = 0xF; return ARM64_SIMD_OP_UQSUB;
    case 0x26: *valid_sizes = 0xF; return ARM64_SIMD_OP_CMHI;
    case 0x27: *valid_sizes = 0xF; return ARM64_SIMD_OP_CMHS;
    case 0x28: *valid_sizes = 0xF; return ARM64_SIMD_OP_USHL;
    case 0x29: *valid_sizes = 0xF; return ARM64_SIMD_OP_UQSHL;
    case 0x2A: *valid_sizes = 0xF; return ARM64_SIMD_OP_URSHL;
    case 0x2B: *valid_sizes = 0xF; return ARM64_SIMD_OP_UQRSHL;
    case 0x2C: *valid_sizes = 0x7; return ARM64_SIMD_OP_UMAX;
    case 0x2D: *valid_sizes = 0x7; return ARM64_SIMD_OP_UMIN;
    case 0x2E: *valid_sizes = 0x7; return ARM64_SIMD_OP_UABD;
    case 0x2F: *valid_sizes = 0x7; return ARM64_SIMD_OP_UABA;
    case 0x30: *valid_sizes = 0xF; return ARM64_SIMD_OP_SUB;
    case 0x31: *valid_sizes = 0xF; return ARM64_SIMD_OP_CMEQ;
    case 0x32: *valid_sizes = 0x7; return ARM64_SIMD_OP_MLS;
    case 0x33: *valid_sizes = 0x1; return ARM64_SIMD_OP_PMUL;
    case 0x34: *valid_sizes = 0x7; return ARM64_SIMD_OP_UMAXP;
    case 0x35: *valid_sizes = 0x7; return ARM64_SIMD_OP_UMINP;
    case 0x36: *valid_sizes = 0x6; return ARM64_SIMD_OP_SQRDMULH;
    default: return ARM64_SIMD_OP_NONE;
    }
}

static enum arm64_simd_operation arm64_simd_decode_fp_3same_operation(arm64_u8 u, arm64_u8 alternate, arm64_u8 opcode)
{
    switch ((u << 6) | (alternate << 5) | opcode)
    {
    case 0x18: return ARM64_SIMD_OP_FMAXNM;
    case 0x19: return ARM64_SIMD_OP_FMLA;
    case 0x1A: return ARM64_SIMD_OP_FADD;
    case 0x1B: return ARM64_SIMD_OP_FMULX;
    case 0x1C: return ARM64_SIMD_OP_FCMEQ;
    case 0x1E: return ARM64_SIMD_OP_FMAX;
    case 0x1F: return ARM64_SIMD_OP_FRECPS;
    case 0x38: return ARM64_SIMD_OP_FMINNM;
    case 0x39: return ARM64_SIMD_OP_FMLS;
    case 0x3A: return ARM64_SIMD_OP_FSUB;
    case 0x3B: return ARM64_SIMD_OP_FAMAX;
    case 0x3E: return ARM64_SIMD_OP_FMIN;
    case 0x3F: return ARM64_SIMD_OP_FRSQRTS;
    case 0x58: return ARM64_SIMD_OP_FMAXNMP;
    case 0x5A: return ARM64_SIMD_OP_FADDP;
    case 0x5B: return ARM64_SIMD_OP_FMUL;
    case 0x5C: return ARM64_SIMD_OP_FCMGE;
    case 0x5D: return ARM64_SIMD_OP_FACGE;
    case 0x5E: return ARM64_SIMD_OP_FMAXP;
    case 0x5F: return ARM64_SIMD_OP_FDIV;
    case 0x78: return ARM64_SIMD_OP_FMINNMP;
    case 0x7A: return ARM64_SIMD_OP_FABD;
    case 0x7B: return ARM64_SIMD_OP_FAMIN;
    case 0x7C: return ARM64_SIMD_OP_FCMGT;
    case 0x7D: return ARM64_SIMD_OP_FACGT;
    case 0x7E: return ARM64_SIMD_OP_FMINP;
    case 0x7F: return ARM64_SIMD_OP_FSCALE;
    default: return ARM64_SIMD_OP_NONE;
    }
}

static arm64_u8 arm64_simd_is_scalar_fp_3same_operation(enum arm64_simd_operation operation)
{
    switch (operation)
    {
    case ARM64_SIMD_OP_FMULX:
    case ARM64_SIMD_OP_FCMEQ:
    case ARM64_SIMD_OP_FRECPS:
    case ARM64_SIMD_OP_FRSQRTS:
    case ARM64_SIMD_OP_FCMGE:
    case ARM64_SIMD_OP_FACGE:
    case ARM64_SIMD_OP_FABD:
    case ARM64_SIMD_OP_FCMGT:
    case ARM64_SIMD_OP_FACGT:
        return 1;
    default:
        return 0;
    }
}

static enum arm64_decode_status arm64_simd_decode_vector_3same_extra(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 q;
    arm64_u8 u;
    arm64_u8 size;
    arm64_u8 opcode;
    enum arm64_simd_operation operation;

    if ((raw & 0x9F208400U) != 0x0E008400U) return ARM64_DECODE_NO_MATCH;

    q = (raw >> 30) & 1;
    u = (raw >> 29) & 1;
    size = (raw >> 22) & 0x3;
    opcode = (raw >> 11) & 0x1F;
    decoded->operand_width = q ? 128 : 64;

    switch ((u << 5) | opcode)
    {
    case 0x30:
        if (size == 0 || size == 3) return ARM64_DECODE_UNALLOCATED;
        operation = ARM64_SIMD_OP_SQRDMLAH;
        decoded->operands.simd.element_width = 8U << size;
        break;
    case 0x31:
        if (size == 0 || size == 3) return ARM64_DECODE_UNALLOCATED;
        operation = ARM64_SIMD_OP_SQRDMLSH;
        decoded->operands.simd.element_width = 8U << size;
        break;
    case 0x12:
        if (size != 2) return ARM64_DECODE_UNALLOCATED;
        operation = ARM64_SIMD_OP_SDOT;
        decoded->operands.simd.element_width = 8;
        decoded->operands.simd.result_element_width = 32;
        break;
    case 0x32:
        if (size != 2) return ARM64_DECODE_UNALLOCATED;
        operation = ARM64_SIMD_OP_UDOT;
        decoded->operands.simd.element_width = 8;
        decoded->operands.simd.result_element_width = 32;
        break;
    case 0x13:
        if (size != 2) return ARM64_DECODE_UNALLOCATED;
        operation = ARM64_SIMD_OP_USDOT;
        decoded->operands.simd.element_width = 8;
        decoded->operands.simd.result_element_width = 32;
        break;
    case 0x3F:
        if (size == 1)
        {
            operation = ARM64_SIMD_OP_BFDOT;
            decoded->operands.simd.element_width = 16;
            decoded->operands.simd.result_element_width = 32;
        }
        else if (size == 3)
        {
            operation = ARM64_SIMD_OP_BFMLAL;
            decoded->operand_width = 128;
            decoded->operands.simd.element_width = 16;
            decoded->operands.simd.result_element_width = 32;
            if (q) decoded->operands.simd.flags |= ARM64_SIMD_FLAG_SOURCE_ODD_ELEMENTS;
        }
        else return ARM64_DECODE_UNALLOCATED;
        break;
    case 0x3D:
        if (!q || size != 1) return ARM64_DECODE_UNALLOCATED;
        operation = ARM64_SIMD_OP_BFMMLA;
        decoded->operand_width = 128;
        decoded->operands.simd.element_width = 16;
        decoded->operands.simd.result_element_width = 32;
        break;
    case 0x14:
    case 0x34:
    case 0x15:
        if (!q || size != 2) return ARM64_DECODE_UNALLOCATED;
        if (((u << 5) | opcode) == 0x14) operation = ARM64_SIMD_OP_SMMLA;
        else if (((u << 5) | opcode) == 0x34) operation = ARM64_SIMD_OP_UMMLA;
        else operation = ARM64_SIMD_OP_USMMLA;
        decoded->operand_width = 128;
        decoded->operands.simd.element_width = 8;
        decoded->operands.simd.result_element_width = 32;
        break;
    default:
        return ARM64_DECODE_NO_MATCH;
    }

    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_EXTENDED_3REG;
    decoded->operands.simd.operation = operation;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_vector_3same(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 fp16 = (raw & 0x9F60C400U) == 0x0E400400U;
    arm64_u8 q = (raw >> 30) & 1;
    arm64_u8 u = (raw >> 29) & 1;
    arm64_u8 size = (raw >> 22) & 0x3;
    arm64_u8 opcode = (raw >> 11) & 0x1F;
    enum arm64_simd_operation operation;

    if (!fp16 && (raw & 0x9F200400U) != 0x0E200400U) return ARM64_DECODE_NO_MATCH;

    decoded->operand_width = q ? 128 : 64;

    if (!fp16 && opcode == 3)
    {
        decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_LOGICAL;
        switch ((u << 2) | size)
        {
        case 0: decoded->operands.simd.operation = ARM64_SIMD_OP_AND; break;
        case 1: decoded->operands.simd.operation = ARM64_SIMD_OP_BIC; break;
        case 2: decoded->operands.simd.operation = ARM64_SIMD_OP_ORR; break;
        case 3: decoded->operands.simd.operation = ARM64_SIMD_OP_ORN; break;
        case 4: decoded->operands.simd.operation = ARM64_SIMD_OP_EOR; break;
        case 5: decoded->operands.simd.operation = ARM64_SIMD_OP_BSL; break;
        case 6: decoded->operands.simd.operation = ARM64_SIMD_OP_BIT; break;
        case 7: decoded->operands.simd.operation = ARM64_SIMD_OP_BIF; break;
        }
        decoded->operands.simd.element_width = 8;
        return ARM64_DECODE_OK;
    }

    if (!fp16 && opcode < 24)
    {
        arm64_u8 valid_sizes;

        operation = arm64_simd_decode_integer_3same_operation(u, opcode, &valid_sizes);
        if (operation == ARM64_SIMD_OP_NONE) return ARM64_DECODE_UNALLOCATED;
        if (!(valid_sizes & (1U << size)) || (!q && size == 3)) return ARM64_DECODE_UNALLOCATED;

        decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_INTEGER_3REG;
        decoded->operands.simd.operation = operation;
        decoded->operands.simd.element_width = 8U << size;
        return ARM64_DECODE_OK;
    }

    if (!fp16 && !(size & 1))
    {
        if (!u && opcode == 29)
        {
            operation = (size >> 1) ? ARM64_SIMD_OP_FMLSL : ARM64_SIMD_OP_FMLAL;
        }
        else if (u && opcode == 25)
        {
            operation = (size >> 1) ? ARM64_SIMD_OP_FMLSL : ARM64_SIMD_OP_FMLAL;
            decoded->operands.simd.flags |= ARM64_SIMD_FLAG_SOURCE_HIGH_HALF;
        }
        else operation = ARM64_SIMD_OP_NONE;

        if (operation != ARM64_SIMD_OP_NONE)
        {
            decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_FP_WIDENING_3REG;
            decoded->operands.simd.operation = operation;
            decoded->operands.simd.element_width = 16;
            decoded->operands.simd.result_element_width = 32;
            return ARM64_DECODE_OK;
        }
    }

    operation = arm64_simd_decode_fp_3same_operation(u, size >> 1, fp16 ? opcode + 24 : opcode);
    if (operation == ARM64_SIMD_OP_NONE) return ARM64_DECODE_UNALLOCATED;

    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_FP_3REG;
    decoded->operands.simd.operation = operation;
    if (fp16) decoded->operands.simd.element_width = 16;
    else
    {
        decoded->operands.simd.element_width = 32U << (size & 1);
        if (!q && decoded->operands.simd.element_width == 64) return ARM64_DECODE_UNALLOCATED;
    }
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_scalar_3same(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u8 fp16 = (raw & 0xDF60C400U) == 0x5E400400U;
    arm64_u8 extra = (raw & 0xDF208400U) == 0x5E008400U;
    arm64_u8 u = (raw >> 29) & 1;
    arm64_u8 size = (raw >> 22) & 0x3;
    arm64_u8 opcode = (raw >> 11) & 0x1F;
    enum arm64_simd_operation operation = ARM64_SIMD_OP_NONE;

    if (extra)
    {
        if (!u || size == 0 || size == 3) return ARM64_DECODE_UNALLOCATED;

        decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_SIMD_3REG;
        decoded->operands.simd.operation = raw & (1U << 11) ? ARM64_SIMD_OP_SQRDMLSH : ARM64_SIMD_OP_SQRDMLAH;
        decoded->operands.simd.element_width = 8U << size;
        decoded->operand_width = decoded->operands.simd.element_width;
        return ARM64_DECODE_OK;
    }

    if (!fp16 && (raw & 0xDF200400U) != 0x5E200400U) return ARM64_DECODE_NO_MATCH;

    if (fp16)
    {
        operation = arm64_simd_decode_fp_3same_operation(u, (raw >> 23) & 1, ((raw >> 11) & 0x7) + 24);
        if (!arm64_simd_is_scalar_fp_3same_operation(operation)) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.element_width = 16;
    }
    else
    {
        switch ((u << 5) | opcode)
        {
        case 0x01: operation = ARM64_SIMD_OP_SQADD; break;
        case 0x05: operation = ARM64_SIMD_OP_SQSUB; break;
        case 0x09: operation = ARM64_SIMD_OP_SQSHL; break;
        case 0x0B: operation = ARM64_SIMD_OP_SQRSHL; break;
        case 0x16: operation = ARM64_SIMD_OP_SQDMULH; break;
        case 0x21: operation = ARM64_SIMD_OP_UQADD; break;
        case 0x25: operation = ARM64_SIMD_OP_UQSUB; break;
        case 0x29: operation = ARM64_SIMD_OP_UQSHL; break;
        case 0x2B: operation = ARM64_SIMD_OP_UQRSHL; break;
        case 0x36: operation = ARM64_SIMD_OP_SQRDMULH; break;
        case 0x06: operation = ARM64_SIMD_OP_CMGT; break;
        case 0x07: operation = ARM64_SIMD_OP_CMGE; break;
        case 0x08: operation = ARM64_SIMD_OP_SSHL; break;
        case 0x0A: operation = ARM64_SIMD_OP_SRSHL; break;
        case 0x10: operation = ARM64_SIMD_OP_ADD; break;
        case 0x11: operation = ARM64_SIMD_OP_CMTST; break;
        case 0x26: operation = ARM64_SIMD_OP_CMHI; break;
        case 0x27: operation = ARM64_SIMD_OP_CMHS; break;
        case 0x28: operation = ARM64_SIMD_OP_USHL; break;
        case 0x2A: operation = ARM64_SIMD_OP_URSHL; break;
        case 0x30: operation = ARM64_SIMD_OP_SUB; break;
        case 0x31: operation = ARM64_SIMD_OP_CMEQ; break;
        default:
            operation = arm64_simd_decode_fp_3same_operation(u, size >> 1, opcode);
            if (!arm64_simd_is_scalar_fp_3same_operation(operation)) return ARM64_DECODE_UNALLOCATED;
            decoded->operands.simd.element_width = 32U << (size & 1);
            break;
        }

        if (opcode == 22)
        {
            if (size == 0 || size == 3) return ARM64_DECODE_UNALLOCATED;
        }
        else if (opcode == 6 || opcode == 7 || opcode == 8 || opcode == 10 || opcode == 16 || opcode == 17)
        {
            if (size != 3) return ARM64_DECODE_UNALLOCATED;
        }

        if (!decoded->operands.simd.element_width) decoded->operands.simd.element_width = 8U << size;
    }

    decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_SIMD_3REG;
    decoded->operands.simd.operation = operation;
    decoded->operand_width = decoded->operands.simd.element_width;
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
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMEQ;
        break;
    case 0x2000C000U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMGE;
        break;
    case 0x0000C000U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMGT;
        break;
    case 0x2000D000U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMLE;
        break;
    case 0x0000E000U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCMLT;
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

    decoded->operands.simd.form = ARM64_SIMD_FORM_FP_COMPARE_ZERO;
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
    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_SHIFT;
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

static enum arm64_decode_status arm64_simd_decode_fp_vector_2reg(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 shape = raw & 0x00FE0000U;
    arm64_u32 signature = raw & 0xBF01FC00U;
    arm64_u8 q = (raw >> 30) & 1;

    if (shape == 0x00F80000U) decoded->operands.simd.element_width = 16;
    else if (shape == 0x00A00000U) decoded->operands.simd.element_width = 32;
    else if (shape == 0x00E00000U)
    {
        if (!q) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.element_width = 64;
    }
    else return ARM64_DECODE_NO_MATCH;

    switch (signature)
    {
    case 0x0E00F800U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FABS;
        break;
    case 0x2E00F800U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FNEG;
        break;
    case 0x2E01F800U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FSQRT;
        break;
    default:
        return ARM64_DECODE_NO_MATCH;
    }
    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_FP_UNARY;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_rev(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 size = (raw >> 22) & 0x3;

    switch (raw & 0xBF3FFC00U)
    {
    case 0x0E200800U:
        if (size > 2) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = ARM64_SIMD_OP_REV64;
        break;
    case 0x2E200800U:
        if (size > 1) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = ARM64_SIMD_OP_REV32;
        break;
    case 0x0E201800U:
        if (size != 0) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.operation = ARM64_SIMD_OP_REV16;
        break;
    default:
        return ARM64_DECODE_NO_MATCH;
    }

    decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_REVERSE;
    decoded->operand_width = raw & (1U << 30) ? 128 : 64;
    decoded->operands.simd.element_width = 8U << size;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_simd_decode_fp_reduce(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 signature;

    if ((raw & 0xDFBFFC00U) == 0x5E30D800U)
    {
        if (!(raw & (1U << 29)))
        {
            if (raw & (1U << 22)) return ARM64_DECODE_UNALLOCATED;
            decoded->operands.simd.element_width = 16;
        }
        else
        {
            decoded->operands.simd.element_width = raw & (1U << 22) ? 64 : 32;
        }
        decoded->operands.simd.form = ARM64_SIMD_FORM_FP_REDUCE;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FADDP;
        decoded->operand_width = decoded->operands.simd.element_width * 2;
        return ARM64_DECODE_OK;
    }

    signature = raw & 0x9FFFFC00U;
    switch (signature)
    {
    case 0x0E30C800U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMAXNMV;
        break;
    case 0x0EB0C800U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMINNMV;
        break;
    case 0x0E30F800U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMAXV;
        break;
    case 0x0EB0F800U:
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMINV;
        break;
    default:
        return ARM64_DECODE_NO_MATCH;
    }

    if (raw & (1U << 29))
    {
        if (!(raw & (1U << 30))) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.element_width = 32;
        decoded->operand_width = 128;
    }
    else
    {
        decoded->operands.simd.element_width = 16;
        decoded->operand_width = raw & (1U << 30) ? 128 : 64;
    }
    decoded->operands.simd.form = ARM64_SIMD_FORM_FP_REDUCE;
    return ARM64_DECODE_OK;
}

/*
所有 FP/AdvSIMD 编码签名只在本文件内匹配；对外只返回稳定的 group 和
arm64_simd_operation，调用方不需要了解原始 opcode/signature。
*/
enum arm64_decode_status arm64_decode_simd(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 iclass = (raw >> 25) & 0xF;
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
        arm64_u8 width;

        type = (raw >> 22) & 0x3;
        width = arm64_simd_scalar_fp_width(type);
        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_FP_IMMEDIATE;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FMOV;
        decoded->operands.simd.immediate = immediate;
        decoded->operand_width = width;
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
        enum arm64_decode_status complex_status = arm64_simd_decode_complex_vector(raw, decoded);

        if (complex_status != ARM64_DECODE_NO_MATCH) return complex_status;
    }

    {
        enum arm64_decode_status vector_3same_extra_status = arm64_simd_decode_vector_3same_extra(raw, decoded);

        if (vector_3same_extra_status != ARM64_DECODE_NO_MATCH) return vector_3same_extra_status;
    }

    {
        enum arm64_decode_status vector_3same_status = arm64_simd_decode_vector_3same(raw, decoded);

        if (vector_3same_status != ARM64_DECODE_NO_MATCH) return vector_3same_status;
    }

    {
        enum arm64_decode_status scalar_3same_status = arm64_simd_decode_scalar_3same(raw, decoded);

        if (scalar_3same_status != ARM64_DECODE_NO_MATCH) return scalar_3same_status;
    }

    if (arm64_simd_is_by_element(raw))
    {
        enum arm64_decode_status element_status = arm64_simd_decode_by_element(raw, decoded);

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

    operation = arm64_simd_decode_conversion(raw, decoded, &conversion_unallocated);
    if (operation != ARM64_SIMD_OP_NONE)
    {
        decoded->operands.simd.form = ARM64_SIMD_FORM_CONVERT;
        decoded->operands.simd.operation = operation;
        return ARM64_DECODE_OK;
    }
    if (conversion_unallocated) return ARM64_DECODE_UNALLOCATED;

    type = (raw >> 22) & 0x3;
    if ((raw & 0xFF200C00U) == 0x1E200800U)
    {
        arm64_u32 opcode = (raw >> 12) & 0xF;
        arm64_u8 width = arm64_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        if (opcode > 8) return ARM64_DECODE_UNSUPPORTED;
        decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_FP_BINARY;
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
        decoded->operand_width = width;
        decoded->operands.simd.element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF207C00U) == 0x1E204000U)
    {
        arm64_u32 opcode = (raw >> 15) & 0x3F;
        arm64_u8 width = arm64_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_FP_UNARY;
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
        decoded->operand_width = width;
        decoded->operands.simd.element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF000000U) == 0x1F000000U)
    {
        arm64_u8 width = arm64_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_FP_TERNARY;
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
        decoded->operand_width = width;
        decoded->operands.simd.element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF20FC00U) == 0x1E202000U)
    {
        arm64_u32 zero = (raw >> 3) & 1;

        if (type == 2 || (raw & 0x7)) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_COMPARE;
        decoded->operands.simd.operation = (raw & (1U << 4)) ? ARM64_SIMD_OP_FCMPE : ARM64_SIMD_OP_FCMP;
        if (zero) decoded->operands.simd.flags |= ARM64_SIMD_FLAG_COMPARE_ZERO;
        decoded->operand_width = type == 3 ? 16 : type ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF200C00U) == 0x1E200400U)
    {
        arm64_u8 width = arm64_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_CONDITIONAL_COMPARE;
        decoded->operands.simd.operation = (raw & (1U << 4)) ? ARM64_SIMD_OP_FCCMPE : ARM64_SIMD_OP_FCCMP;
        decoded->operands.simd.condition = (raw >> 12) & 0xF;
        decoded->operands.simd.immediate = raw & 0xF;
        decoded->operand_width = width;
        decoded->operands.simd.element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF200C00U) == 0x1E200C00U)
    {
        arm64_u8 width = arm64_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.form = ARM64_SIMD_FORM_SCALAR_SELECT;
        decoded->operands.simd.operation = ARM64_SIMD_OP_FCSEL;
        decoded->operands.simd.condition = (raw >> 12) & 0xF;
        decoded->operand_width = width;
        decoded->operands.simd.element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FBEFC00U) == 0x1E260000U)
    {
        arm64_u32 sf = (raw >> 31) & 1;

        if (((raw >> 22) & 1) != sf) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.simd.form = ARM64_SIMD_FORM_FP_GPR_TRANSFER;
        decoded->operands.simd.operation = (raw & (1U << 16)) ? ARM64_SIMD_OP_FMOV_GENERAL_TO_FP : ARM64_SIMD_OP_FMOV_FP_TO_GENERAL;
        decoded->operand_width = sf ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xBFE08400U) == 0x2E000000U)
    {
        decoded->operands.simd.form = ARM64_SIMD_FORM_VECTOR_EXTRACT;
        decoded->operands.simd.operation = ARM64_SIMD_OP_EXT;
        decoded->operands.simd.immediate = (raw >> 11) & 0xF;
        decoded->operand_width = (raw & (1U << 30)) ? 128 : 64;
        if (decoded->operand_width == 64 && decoded->operands.simd.immediate >= 8) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    {
        enum arm64_decode_status fp_vector_status = arm64_simd_decode_fp_vector_2reg(raw, decoded);

        if (fp_vector_status != ARM64_DECODE_NO_MATCH) return fp_vector_status;
    }

    {
        enum arm64_decode_status rev_status = arm64_simd_decode_rev(raw, decoded);

        if (rev_status != ARM64_DECODE_NO_MATCH) return rev_status;
    }

    {
        enum arm64_decode_status reduce_status = arm64_simd_decode_fp_reduce(raw, decoded);

        if (reduce_status != ARM64_DECODE_NO_MATCH) return reduce_status;
    }

    return ARM64_DECODE_UNSUPPORTED;
}