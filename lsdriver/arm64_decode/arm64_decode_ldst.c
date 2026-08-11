#include "arm64_decode_internal.h"

static void arm64_decode_ldst_rt_rn(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    decoded->rt = raw & 0x1F;
    decoded->rn = (raw >> 5) & 0x1F;
}

static enum arm64_instruction arm64_decode_lse_atomic_instruction(uint32_t raw)
{
    switch ((((raw >> 12) & 0xF) << 2) | (((raw >> 23) & 1) << 1) | ((raw >> 22) & 1))
    {
    case 0x00:
        return ARM64_INSN_LDADD;
    case 0x01:
        return ARM64_INSN_LDADDL;
    case 0x02:
        return ARM64_INSN_LDADDA;
    case 0x03:
        return ARM64_INSN_LDADDAL;
    case 0x04:
        return ARM64_INSN_LDCLR;
    case 0x05:
        return ARM64_INSN_LDCLRL;
    case 0x06:
        return ARM64_INSN_LDCLRA;
    case 0x07:
        return ARM64_INSN_LDCLRAL;
    case 0x08:
        return ARM64_INSN_LDEOR;
    case 0x09:
        return ARM64_INSN_LDEORL;
    case 0x0A:
        return ARM64_INSN_LDEORA;
    case 0x0B:
        return ARM64_INSN_LDEORAL;
    case 0x0C:
        return ARM64_INSN_LDSET;
    case 0x0D:
        return ARM64_INSN_LDSETL;
    case 0x0E:
        return ARM64_INSN_LDSETA;
    case 0x0F:
        return ARM64_INSN_LDSETAL;
    case 0x10:
        return ARM64_INSN_LDSMAX;
    case 0x11:
        return ARM64_INSN_LDSMAXL;
    case 0x12:
        return ARM64_INSN_LDSMAXA;
    case 0x13:
        return ARM64_INSN_LDSMAXAL;
    case 0x14:
        return ARM64_INSN_LDSMIN;
    case 0x15:
        return ARM64_INSN_LDSMINL;
    case 0x16:
        return ARM64_INSN_LDSMINA;
    case 0x17:
        return ARM64_INSN_LDSMINAL;
    case 0x18:
        return ARM64_INSN_LDUMAX;
    case 0x19:
        return ARM64_INSN_LDUMAXL;
    case 0x1A:
        return ARM64_INSN_LDUMAXA;
    case 0x1B:
        return ARM64_INSN_LDUMAXAL;
    case 0x1C:
        return ARM64_INSN_LDUMIN;
    case 0x1D:
        return ARM64_INSN_LDUMINL;
    case 0x1E:
        return ARM64_INSN_LDUMINA;
    case 0x1F:
        return ARM64_INSN_LDUMINAL;
    case 0x20:
        return ARM64_INSN_SWP;
    case 0x21:
        return ARM64_INSN_SWPL;
    case 0x22:
        return ARM64_INSN_SWPA;
    case 0x23:
        return ARM64_INSN_SWPAL;
    default:
        return ARM64_INSN_UNKNOWN;
    }
}

enum arm64_ldst_form
{
    ARM64_LDST_FORM_UNSIGNED_OFFSET,
    ARM64_LDST_FORM_IMMEDIATE,
    ARM64_LDST_FORM_REGISTER_OFFSET,
    ARM64_LDST_FORM_PAUTH,
    ARM64_LDST_FORM_ATOMIC,
};

enum arm64_ldst_register_kind
{
    ARM64_LDST_REGISTER_GPR,
    ARM64_LDST_REGISTER_FP_SIMD,
};

enum arm64_ldst_address_mode
{
    ARM64_LDST_ADDRESS_BASE,
    ARM64_LDST_ADDRESS_UNSCALED_OFFSET,
    ARM64_LDST_ADDRESS_NON_TEMPORAL_OFFSET,
    ARM64_LDST_ADDRESS_POST_INDEX,
    ARM64_LDST_ADDRESS_PRE_INDEX,
    ARM64_LDST_ADDRESS_UNPRIVILEGED_OFFSET,
    ARM64_LDST_ADDRESS_REGISTER_OFFSET,
    ARM64_LDST_ADDRESS_UNSIGNED_OFFSET,
};

static enum arm64_memory_address_mode arm64_normalize_ldst_address_mode(enum arm64_ldst_address_mode address_mode)
{
    switch (address_mode)
    {
    case ARM64_LDST_ADDRESS_POST_INDEX:
        return ARM64_MEMORY_ADDRESS_POST_INDEX;
    case ARM64_LDST_ADDRESS_PRE_INDEX:
        return ARM64_MEMORY_ADDRESS_PRE_INDEX;
    case ARM64_LDST_ADDRESS_REGISTER_OFFSET:
        return ARM64_MEMORY_ADDRESS_REGISTER_OFFSET;
    case ARM64_LDST_ADDRESS_BASE:
    case ARM64_LDST_ADDRESS_UNSCALED_OFFSET:
    case ARM64_LDST_ADDRESS_NON_TEMPORAL_OFFSET:
    case ARM64_LDST_ADDRESS_UNPRIVILEGED_OFFSET:
    case ARM64_LDST_ADDRESS_UNSIGNED_OFFSET:
        return ARM64_MEMORY_ADDRESS_BASE_OFFSET;
    default:
        return ARM64_MEMORY_ADDRESS_NONE;
    }
}

static int arm64_ldst_is_prefetch(uint32_t raw, enum arm64_ldst_register_kind register_kind)
{
    return register_kind == ARM64_LDST_REGISTER_GPR && ((raw >> 30) & 0x3) == 3 && ((raw >> 22) & 0x3) == 2;
}

static enum arm64_instruction arm64_decode_ldst_single_instruction(uint32_t raw, enum arm64_ldst_register_kind register_kind, enum arm64_ldst_address_mode address_mode, int prefetch)
{
    uint32_t opc = (raw >> 22) & 0x3;

    if (prefetch)
    {
        switch (address_mode)
        {
        case ARM64_LDST_ADDRESS_UNSCALED_OFFSET:
            return ARM64_INSN_PRFUM;
        case ARM64_LDST_ADDRESS_REGISTER_OFFSET:
            return ARM64_INSN_PRFM_REGISTER_OFFSET;
        case ARM64_LDST_ADDRESS_UNSIGNED_OFFSET:
            return ARM64_INSN_PRFM_UNSIGNED_OFFSET;
        default:
            return ARM64_INSN_UNKNOWN;
        }
    }

    if (register_kind == ARM64_LDST_REGISTER_FP_SIMD)
    {
        if (opc & 1)
        {
            switch (address_mode)
            {
            case ARM64_LDST_ADDRESS_UNSCALED_OFFSET:
                return ARM64_INSN_LDUR_FP_SIMD;
            case ARM64_LDST_ADDRESS_POST_INDEX:
                return ARM64_INSN_LDR_FP_SIMD_POST_INDEX;
            case ARM64_LDST_ADDRESS_PRE_INDEX:
                return ARM64_INSN_LDR_FP_SIMD_PRE_INDEX;
            case ARM64_LDST_ADDRESS_REGISTER_OFFSET:
                return ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET;
            case ARM64_LDST_ADDRESS_UNSIGNED_OFFSET:
                return ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET;
            default:
                return ARM64_INSN_UNKNOWN;
            }
        }

        switch (address_mode)
        {
        case ARM64_LDST_ADDRESS_UNSCALED_OFFSET:
            return ARM64_INSN_STUR_FP_SIMD;
        case ARM64_LDST_ADDRESS_POST_INDEX:
            return ARM64_INSN_STR_FP_SIMD_POST_INDEX;
        case ARM64_LDST_ADDRESS_PRE_INDEX:
            return ARM64_INSN_STR_FP_SIMD_PRE_INDEX;
        case ARM64_LDST_ADDRESS_REGISTER_OFFSET:
            return ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET;
        case ARM64_LDST_ADDRESS_UNSIGNED_OFFSET:
            return ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET;
        default:
            return ARM64_INSN_UNKNOWN;
        }
    }

    if (opc == 0)
    {
        switch (address_mode)
        {
        case ARM64_LDST_ADDRESS_UNSCALED_OFFSET:
            return ARM64_INSN_STUR_GPR;
        case ARM64_LDST_ADDRESS_UNPRIVILEGED_OFFSET:
            return ARM64_INSN_STTR_GPR;
        case ARM64_LDST_ADDRESS_POST_INDEX:
            return ARM64_INSN_STR_GPR_POST_INDEX;
        case ARM64_LDST_ADDRESS_PRE_INDEX:
            return ARM64_INSN_STR_GPR_PRE_INDEX;
        case ARM64_LDST_ADDRESS_REGISTER_OFFSET:
            return ARM64_INSN_STR_GPR_REGISTER_OFFSET;
        case ARM64_LDST_ADDRESS_UNSIGNED_OFFSET:
            return ARM64_INSN_STR_GPR_UNSIGNED_OFFSET;
        default:
            return ARM64_INSN_UNKNOWN;
        }
    }

    if (opc == 1)
    {
        switch (address_mode)
        {
        case ARM64_LDST_ADDRESS_UNSCALED_OFFSET:
            return ARM64_INSN_LDUR_GPR;
        case ARM64_LDST_ADDRESS_UNPRIVILEGED_OFFSET:
            return ARM64_INSN_LDTR_GPR;
        case ARM64_LDST_ADDRESS_POST_INDEX:
            return ARM64_INSN_LDR_GPR_POST_INDEX;
        case ARM64_LDST_ADDRESS_PRE_INDEX:
            return ARM64_INSN_LDR_GPR_PRE_INDEX;
        case ARM64_LDST_ADDRESS_REGISTER_OFFSET:
            return ARM64_INSN_LDR_GPR_REGISTER_OFFSET;
        case ARM64_LDST_ADDRESS_UNSIGNED_OFFSET:
            return ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET;
        default:
            return ARM64_INSN_UNKNOWN;
        }
    }

    switch (address_mode)
    {
    case ARM64_LDST_ADDRESS_UNSCALED_OFFSET:
        return ARM64_INSN_LDUR_SIGNED_GPR;
    case ARM64_LDST_ADDRESS_UNPRIVILEGED_OFFSET:
        return ARM64_INSN_LDTR_SIGNED_GPR;
    case ARM64_LDST_ADDRESS_POST_INDEX:
        return ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX;
    case ARM64_LDST_ADDRESS_PRE_INDEX:
        return ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX;
    case ARM64_LDST_ADDRESS_REGISTER_OFFSET:
        return ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET;
    case ARM64_LDST_ADDRESS_UNSIGNED_OFFSET:
        return ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET;
    default:
        return ARM64_INSN_UNKNOWN;
    }
}

static enum arm64_instruction arm64_decode_ldst_pair_instruction(enum arm64_ldst_register_kind register_kind, enum arm64_ldst_address_mode address_mode, uint32_t load, uint32_t opc)
{
    if (register_kind == ARM64_LDST_REGISTER_FP_SIMD)
    {
        if (load)
        {
            switch (address_mode)
            {
            case ARM64_LDST_ADDRESS_NON_TEMPORAL_OFFSET:
                return ARM64_INSN_LDNP_FP_SIMD;
            case ARM64_LDST_ADDRESS_BASE:
                return ARM64_INSN_LDP_FP_SIMD_OFFSET;
            case ARM64_LDST_ADDRESS_POST_INDEX:
                return ARM64_INSN_LDP_FP_SIMD_POST_INDEX;
            case ARM64_LDST_ADDRESS_PRE_INDEX:
                return ARM64_INSN_LDP_FP_SIMD_PRE_INDEX;
            default:
                return ARM64_INSN_UNKNOWN;
            }
        }

        switch (address_mode)
        {
        case ARM64_LDST_ADDRESS_NON_TEMPORAL_OFFSET:
            return ARM64_INSN_STNP_FP_SIMD;
        case ARM64_LDST_ADDRESS_BASE:
            return ARM64_INSN_STP_FP_SIMD_OFFSET;
        case ARM64_LDST_ADDRESS_POST_INDEX:
            return ARM64_INSN_STP_FP_SIMD_POST_INDEX;
        case ARM64_LDST_ADDRESS_PRE_INDEX:
            return ARM64_INSN_STP_FP_SIMD_PRE_INDEX;
        default:
            return ARM64_INSN_UNKNOWN;
        }
    }

    if (load && opc == 1)
    {
        switch (address_mode)
        {
        case ARM64_LDST_ADDRESS_BASE:
            return ARM64_INSN_LDPSW_OFFSET;
        case ARM64_LDST_ADDRESS_POST_INDEX:
            return ARM64_INSN_LDPSW_POST_INDEX;
        case ARM64_LDST_ADDRESS_PRE_INDEX:
            return ARM64_INSN_LDPSW_PRE_INDEX;
        default:
            return ARM64_INSN_UNKNOWN;
        }
    }

    if (load)
    {
        switch (address_mode)
        {
        case ARM64_LDST_ADDRESS_NON_TEMPORAL_OFFSET:
            return ARM64_INSN_LDNP_GPR;
        case ARM64_LDST_ADDRESS_BASE:
            return ARM64_INSN_LDP_GPR_OFFSET;
        case ARM64_LDST_ADDRESS_POST_INDEX:
            return ARM64_INSN_LDP_GPR_POST_INDEX;
        case ARM64_LDST_ADDRESS_PRE_INDEX:
            return ARM64_INSN_LDP_GPR_PRE_INDEX;
        default:
            return ARM64_INSN_UNKNOWN;
        }
    }

    switch (address_mode)
    {
    case ARM64_LDST_ADDRESS_NON_TEMPORAL_OFFSET:
        return ARM64_INSN_STNP_GPR;
    case ARM64_LDST_ADDRESS_BASE:
        return ARM64_INSN_STP_GPR_OFFSET;
    case ARM64_LDST_ADDRESS_POST_INDEX:
        return ARM64_INSN_STP_GPR_POST_INDEX;
    case ARM64_LDST_ADDRESS_PRE_INDEX:
        return ARM64_INSN_STP_GPR_PRE_INDEX;
    default:
        return ARM64_INSN_UNKNOWN;
    }
}

static enum arm64_decode_status arm64_decode_ldst_atomic(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    switch ((raw >> 12) & 0xF)
    {
    case 0:
    case 1:
    case 2:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        break;
    case 9:
        if (!(raw & 0x80000000U)) return ARM64_DECODE_UNSUPPORTED;
        if ((raw & 0xC0DF0000U) == 0xC01F0000U)
        {
            if ((raw & 0x1F) >= 24 || (raw & 1)) return ARM64_DECODE_UNALLOCATED;
            return ARM64_DECODE_UNSUPPORTED;
        }
        return ARM64_DECODE_UNALLOCATED;
    case 10:
        if (!(raw & 0x80000000U)) return ARM64_DECODE_UNSUPPORTED;
        if ((raw & 0xC0C00000U) == 0xC0000000U)
        {
            if ((raw & 0x1F) >= 24 || (raw & 1)) return ARM64_DECODE_UNALLOCATED;
            return ARM64_DECODE_UNSUPPORTED;
        }
        return ARM64_DECODE_UNALLOCATED;
    case 11:
        if (!(raw & 0x80000000U)) return ARM64_DECODE_UNSUPPORTED;
        if ((raw & 0xC0C00000U) == 0xC0000000U)
        {
            if ((raw & 0x1F) >= 24 || (raw & 1)) return ARM64_DECODE_UNALLOCATED;
            return ARM64_DECODE_UNSUPPORTED;
        }
        return ARM64_DECODE_UNALLOCATED;
    case 12:
        if ((raw & 0x00DF0000U) != 0x009F0000U) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INSN_LDAPR;
        arm64_decode_ldst_rt_rn(raw, decoded);
        decoded->access_bytes = 1U << ((raw >> 30) & 0x3);
        decoded->operand_width = decoded->access_bytes == 8 ? 64 : 32;
        return ARM64_DECODE_OK;
    case 13:
        if ((raw & 0xC0DF0000U) == 0xC01F0000U)
        {
            if ((raw & 0x1F) >= 24 || (raw & 1)) return ARM64_DECODE_UNALLOCATED;
            return ARM64_DECODE_UNSUPPORTED;
        }
        return ARM64_DECODE_UNALLOCATED;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    decoded->instruction = arm64_decode_lse_atomic_instruction(raw);
    arm64_decode_ldst_rt_rn(raw, decoded);
    decoded->rs = (raw >> 16) & 0x1F;
    decoded->access_bytes = 1U << ((raw >> 30) & 0x3);
    decoded->operand_width = decoded->access_bytes == 8 ? 64 : 32;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_single(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_ldst_register_kind register_kind)
{
    uint32_t size = (raw >> 30) & 0x3;
    uint32_t opc = (raw >> 22) & 0x3;
    int prefetch = arm64_ldst_is_prefetch(raw, register_kind);

    if (prefetch)
    {
        decoded->rn = (raw >> 5) & 0x1F;
        decoded->prefetch_operation = raw & 0x1F;
        decoded->operand_width = 0;
    }
    else
    {
        arm64_decode_ldst_rt_rn(raw, decoded);
        if (register_kind == ARM64_LDST_REGISTER_FP_SIMD && opc >= 2 && size != 0) return ARM64_DECODE_UNALLOCATED;
        if (register_kind == ARM64_LDST_REGISTER_FP_SIMD) decoded->access_bytes = size == 0 && (opc & 2) ? 16 : 1U << size;
        else decoded->access_bytes = 1U << size;
        decoded->operand_width = register_kind == ARM64_LDST_REGISTER_FP_SIMD ? decoded->access_bytes * 8 : (size == 3 || opc == 2 ? 64 : 32);
    }

    if (register_kind == ARM64_LDST_REGISTER_GPR && opc == 3 && size >= 2) return ARM64_DECODE_UNALLOCATED;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_literal(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_ldst_register_kind register_kind)
{
    uint32_t size = (raw >> 30) & 0x3;
    int prefetch = register_kind == ARM64_LDST_REGISTER_GPR && size == 3;

    if (!prefetch) decoded->rt = raw & 0x1F;
    else decoded->prefetch_operation = raw & 0x1F;
    decoded->memory_address_mode = ARM64_MEMORY_ADDRESS_LITERAL;
    if (register_kind == ARM64_LDST_REGISTER_FP_SIMD) decoded->access_bytes = size == 0 ? 4 : size == 1 ? 8 : size == 2 ? 16 : 0;
    else decoded->access_bytes = size == 0 ? 4 : size == 1 ? 8 : size == 2 ? 4 : 0;
    decoded->offset = arm64_sign_extend((uint64_t)((raw >> 5) & 0x7FFFF) << 2, 21);
    decoded->operand_width = prefetch ? 0 : register_kind == ARM64_LDST_REGISTER_FP_SIMD ? decoded->access_bytes * 8 : (size == 0 ? 32 : 64);
    if (register_kind == ARM64_LDST_REGISTER_FP_SIMD && !decoded->access_bytes) return ARM64_DECODE_UNALLOCATED;
    if (prefetch) decoded->instruction = ARM64_INSN_PRFM_LITERAL;
    else if (register_kind == ARM64_LDST_REGISTER_FP_SIMD) decoded->instruction = ARM64_INSN_LDR_LITERAL_FP_SIMD;
    else decoded->instruction = size == 2 ? ARM64_INSN_LDRSW_LITERAL : ARM64_INSN_LDR_LITERAL_GPR;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_pair(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_ldst_register_kind register_kind, enum arm64_ldst_address_mode low_mode, enum arm64_ldst_address_mode high_mode)
{
    uint32_t opc = (raw >> 30) & 0x3;
    uint32_t load = (raw >> 22) & 1;
    enum arm64_ldst_address_mode address_mode = (raw & 0x00800000U) ? high_mode : low_mode;

    arm64_decode_ldst_rt_rn(raw, decoded);
    decoded->memory_address_mode = arm64_normalize_ldst_address_mode(address_mode);
    decoded->rt2 = (raw >> 10) & 0x1F;
    if (register_kind == ARM64_LDST_REGISTER_FP_SIMD)
    {
        if (opc == 3) return ARM64_DECODE_UNALLOCATED;
        decoded->access_bytes = 4U << opc;
        decoded->operand_width = decoded->access_bytes * 8;
    }
    else
    {
        if (opc == 3 || (opc == 1 && address_mode == ARM64_LDST_ADDRESS_NON_TEMPORAL_OFFSET)) return ARM64_DECODE_UNALLOCATED;
        if (opc == 1 && !load) return ARM64_DECODE_UNSUPPORTED;
        decoded->access_bytes = opc == 2 ? 8 : 4;
        decoded->operand_width = opc == 0 ? 32 : 64;
    }
    if (register_kind == ARM64_LDST_REGISTER_GPR && (address_mode == ARM64_LDST_ADDRESS_POST_INDEX || address_mode == ARM64_LDST_ADDRESS_PRE_INDEX) && decoded->rn != 31 && (decoded->rn == decoded->rt || decoded->rn == decoded->rt2)) return ARM64_DECODE_UNPREDICTABLE;
    if (load && decoded->rt == decoded->rt2) return ARM64_DECODE_UNPREDICTABLE;
    decoded->offset = arm64_sign_extend((raw >> 15) & 0x7F, 7) * decoded->access_bytes;
    decoded->instruction = arm64_decode_ldst_pair_instruction(register_kind, address_mode, load, opc);
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_unsigned(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_ldst_register_kind register_kind)
{
    enum arm64_decode_status status = arm64_decode_ldst_single(raw, decoded, register_kind);
    int prefetch;

    if (status != ARM64_DECODE_OK) return status;
    prefetch = decoded->operand_width == 0;
    decoded->offset = ((raw >> 10) & 0xFFF) * (prefetch ? 8 : decoded->access_bytes);
    decoded->instruction = arm64_decode_ldst_single_instruction(raw, register_kind, ARM64_LDST_ADDRESS_UNSIGNED_OFFSET, prefetch);
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_unscaled(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_ldst_register_kind register_kind)
{
    enum arm64_ldst_form form;
    enum arm64_ldst_address_mode address_mode = ARM64_LDST_ADDRESS_BASE;

    if (!(raw & 0x00200000U)) form = ARM64_LDST_FORM_IMMEDIATE;
    else
    {
        uint32_t mode = (raw >> 10) & 0x3;

        if (mode == 2) form = ARM64_LDST_FORM_REGISTER_OFFSET;
        else if (register_kind == ARM64_LDST_REGISTER_FP_SIMD) return ARM64_DECODE_UNALLOCATED;
        else if (mode == 0) form = ARM64_LDST_FORM_ATOMIC;
        else if (((raw >> 30) & 0x3) == 3) form = ARM64_LDST_FORM_PAUTH;
        else return ARM64_DECODE_UNALLOCATED;
    }

    switch (form)
    {
    case ARM64_LDST_FORM_IMMEDIATE:
    {
        enum arm64_decode_status status = arm64_decode_ldst_single(raw, decoded, register_kind);
        uint32_t mode = (raw >> 10) & 0x3;
        int prefetch;

        if (status != ARM64_DECODE_OK) return status;
        prefetch = decoded->operand_width == 0;
        if (register_kind == ARM64_LDST_REGISTER_FP_SIMD && mode == 2) return ARM64_DECODE_UNALLOCATED;
        if (prefetch && mode != 0) return ARM64_DECODE_UNALLOCATED;
        decoded->offset = arm64_sign_extend((raw >> 12) & 0x1FF, 9);
        address_mode = mode == 1 ? ARM64_LDST_ADDRESS_POST_INDEX : mode == 2 ? ARM64_LDST_ADDRESS_UNPRIVILEGED_OFFSET : mode == 3 ? ARM64_LDST_ADDRESS_PRE_INDEX : ARM64_LDST_ADDRESS_UNSCALED_OFFSET;
        decoded->instruction = arm64_decode_ldst_single_instruction(raw, register_kind, address_mode, prefetch);
        break;
    }
    case ARM64_LDST_FORM_ATOMIC:
        return arm64_decode_ldst_atomic(raw, decoded);
    case ARM64_LDST_FORM_PAUTH:
        return ARM64_DECODE_UNSUPPORTED;
    case ARM64_LDST_FORM_REGISTER_OFFSET:
    {
        enum arm64_decode_status status = arm64_decode_ldst_single(raw, decoded, register_kind);
        int prefetch;

        if (status != ARM64_DECODE_OK) return status;
        prefetch = decoded->operand_width == 0;
        if (prefetch && (raw & 0x18U) == 0x18U) return ARM64_DECODE_UNSUPPORTED;
        address_mode = ARM64_LDST_ADDRESS_REGISTER_OFFSET;
        decoded->rm = (raw >> 16) & 0x1F;
        decoded->extend_type = (raw >> 13) & 0x7;
        if (decoded->extend_type != 2 && decoded->extend_type != 3 && decoded->extend_type != 6 && decoded->extend_type != 7) return ARM64_DECODE_UNALLOCATED;
        decoded->shift_amount = (raw & 0x1000U) ? (prefetch ? 3 : (uint8_t)__builtin_ctz(decoded->access_bytes)) : 0;
        decoded->instruction = arm64_decode_ldst_single_instruction(raw, register_kind, address_mode, prefetch);
        break;
    }
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    if (register_kind == ARM64_LDST_REGISTER_GPR && (address_mode == ARM64_LDST_ADDRESS_PRE_INDEX || address_mode == ARM64_LDST_ADDRESS_POST_INDEX) && decoded->rn != 31 && decoded->rn == decoded->rt) return ARM64_DECODE_UNPREDICTABLE;
    decoded->memory_address_mode = arm64_normalize_ldst_address_mode(address_mode);
    return ARM64_DECODE_OK;
}

/*
解码访存、原子和独占指令。bits[29:24] 先确定唯一编码 owner，叶子只校验
本族固定字段和寄存器约束，不依赖宽窄掩码的排列顺序。
*/
enum arm64_decode_status arm64_decode_ldst(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    decoded->insn_class = ARM64_INSN_CLASS_LOAD_STORE;
    decoded->memory_address_mode = ARM64_MEMORY_ADDRESS_BASE_OFFSET;

    switch ((raw >> 24) & 0x3F)
    {
    case 0x08:
        switch (raw & 0x00A07C00U)
        {
        case 0x00207C00U:
        {
            uint32_t size = (raw >> 30) & 0x3;
            uint32_t rs = (raw >> 16) & 0x1F;
            uint32_t rt = raw & 0x1F;

            if (size >= 2 || ((raw >> 10) & 0x1F) != 31 || ((rs | rt) & 1)) return ARM64_DECODE_UNALLOCATED;
            switch (((raw >> 21) & 2) | ((raw >> 15) & 1))
            {
            case 0:
                decoded->instruction = ARM64_INSN_CASP;
                break;
            case 1:
                decoded->instruction = ARM64_INSN_CASPL;
                break;
            case 2:
                decoded->instruction = ARM64_INSN_CASPA;
                break;
            case 3:
                decoded->instruction = ARM64_INSN_CASPAL;
                break;
            }
            arm64_decode_ldst_rt_rn(raw, decoded);
            decoded->rs = (raw >> 16) & 0x1F;
            decoded->access_bytes = size == 0 ? 4 : 8;
            decoded->operand_width = decoded->access_bytes * 8;
            return ARM64_DECODE_OK;
        }
        case 0x00A07C00U:
            switch (((raw >> 21) & 2) | ((raw >> 15) & 1))
            {
            case 0:
                decoded->instruction = ARM64_INSN_CAS;
                break;
            case 1:
                decoded->instruction = ARM64_INSN_CASL;
                break;
            case 2:
                decoded->instruction = ARM64_INSN_CASA;
                break;
            case 3:
                decoded->instruction = ARM64_INSN_CASAL;
                break;
            }
            arm64_decode_ldst_rt_rn(raw, decoded);
            decoded->rs = (raw >> 16) & 0x1F;
            decoded->access_bytes = 1U << ((raw >> 30) & 0x3);
            decoded->operand_width = decoded->access_bytes == 8 ? 64 : 32;
            return ARM64_DECODE_OK;
        default:
            break;
        }

        {
            uint32_t size = (raw >> 30) & 0x3;
            uint32_t ordered = (raw >> 23) & 1;
            uint32_t load = (raw >> 22) & 1;
            uint32_t pair = (raw >> 21) & 1;
            uint32_t acquire_release = (raw >> 15) & 1;
            uint32_t rs = (raw >> 16) & 0x1F;
            uint32_t rt2 = (raw >> 10) & 0x1F;

            if (ordered)
            {
                if (pair || rs != 31 || rt2 != 31) return ARM64_DECODE_UNALLOCATED;
            }
            else if ((pair && size < 2) || (!pair && rt2 != 31) || (load && rs != 31))
            {
                return ARM64_DECODE_UNALLOCATED;
            }

            switch ((ordered << 3) | (load << 2) | (pair << 1) | acquire_release)
            {
            case 0x0:
                decoded->instruction = ARM64_INSN_STXR;
                break;
            case 0x1:
                decoded->instruction = ARM64_INSN_STLXR;
                break;
            case 0x2:
                decoded->instruction = ARM64_INSN_STXP;
                break;
            case 0x3:
                decoded->instruction = ARM64_INSN_STLXP;
                break;
            case 0x4:
                decoded->instruction = ARM64_INSN_LDXR;
                break;
            case 0x5:
                decoded->instruction = ARM64_INSN_LDAXR;
                break;
            case 0x6:
                decoded->instruction = ARM64_INSN_LDXP;
                break;
            case 0x7:
                decoded->instruction = ARM64_INSN_LDAXP;
                break;
            case 0x8:
                decoded->instruction = ARM64_INSN_STLLR;
                break;
            case 0x9:
                decoded->instruction = ARM64_INSN_STLR;
                break;
            case 0xC:
                decoded->instruction = ARM64_INSN_LDLAR;
                break;
            case 0xD:
                decoded->instruction = ARM64_INSN_LDAR;
                break;
            default:
                return ARM64_DECODE_UNALLOCATED;
            }
            arm64_decode_ldst_rt_rn(raw, decoded);
            if (pair) decoded->rt2 = rt2;
            if (!ordered && !load) decoded->rs = rs;
            decoded->access_bytes = 1U << size;
            decoded->operand_width = decoded->access_bytes == 8 ? 64 : 32;
            if (pair && load && decoded->rt == decoded->rt2) return ARM64_DECODE_UNPREDICTABLE;
            if (!load && rs != 31 && (rs == decoded->rt || (pair && rs == decoded->rt2) || (decoded->rn != 31 && rs == decoded->rn))) return ARM64_DECODE_UNPREDICTABLE;
            return ARM64_DECODE_OK;
        }

    case 0x09:
    case 0x0C:
    case 0x0D:
        return ARM64_DECODE_UNSUPPORTED;

    case 0x18:
        return arm64_decode_ldst_literal(raw, decoded, ARM64_LDST_REGISTER_GPR);
    case 0x19:
        switch (raw & 0x8020FC00U)
        {
        case 0x00200800U:
            return ARM64_DECODE_UNSUPPORTED;
        case 0x00200C00U:
            if ((((raw >> 16) & 0x1F) | (raw & 0x1F)) & 1) return ARM64_DECODE_UNALLOCATED;
            return ARM64_DECODE_UNSUPPORTED;
        case 0x00209000U:
        case 0x0020A000U:
        case 0x0020B000U:
        {
            uint32_t rt = raw & 0x1F;
            uint32_t rt2 = (raw >> 16) & 0x1F;

            if (rt == 31 || rt2 == 31) return ARM64_DECODE_UNALLOCATED;
            if (rt == rt2) return ARM64_DECODE_UNPREDICTABLE;
            return ARM64_DECODE_UNSUPPORTED;
        }
        default:
            break;
        }

        if ((raw & 0x00200C00U) == 0)
        {
            uint32_t size = (raw >> 30) & 0x3;
            uint32_t opc = (raw >> 22) & 0x3;

            if ((size == 3 && opc > 1) || (size == 2 && opc == 3)) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = opc == 0 ? ARM64_INSN_STLUR : opc == 1 ? ARM64_INSN_LDAPUR : ARM64_INSN_LDAPUR_SIGNED;
            arm64_decode_ldst_rt_rn(raw, decoded);
            decoded->access_bytes = 1U << size;
            decoded->offset = arm64_sign_extend((raw >> 12) & 0x1FF, 9);
            decoded->operand_width = size == 3 || opc == 2 ? 64 : 32;
            return ARM64_DECODE_OK;
        }
        return ARM64_DECODE_UNSUPPORTED;
    case 0x1C:
        return arm64_decode_ldst_literal(raw, decoded, ARM64_LDST_REGISTER_FP_SIMD);
    case 0x1D:
        return ARM64_DECODE_UNSUPPORTED;

    case 0x28:
        return arm64_decode_ldst_pair(raw, decoded, ARM64_LDST_REGISTER_GPR, ARM64_LDST_ADDRESS_NON_TEMPORAL_OFFSET, ARM64_LDST_ADDRESS_POST_INDEX);
    case 0x29:
        return arm64_decode_ldst_pair(raw, decoded, ARM64_LDST_REGISTER_GPR, ARM64_LDST_ADDRESS_BASE, ARM64_LDST_ADDRESS_PRE_INDEX);
    case 0x2C:
        return arm64_decode_ldst_pair(raw, decoded, ARM64_LDST_REGISTER_FP_SIMD, ARM64_LDST_ADDRESS_NON_TEMPORAL_OFFSET, ARM64_LDST_ADDRESS_POST_INDEX);
    case 0x2D:
        return arm64_decode_ldst_pair(raw, decoded, ARM64_LDST_REGISTER_FP_SIMD, ARM64_LDST_ADDRESS_BASE, ARM64_LDST_ADDRESS_PRE_INDEX);

    case 0x38:
        return arm64_decode_ldst_unscaled(raw, decoded, ARM64_LDST_REGISTER_GPR);
    case 0x39:
        return arm64_decode_ldst_unsigned(raw, decoded, ARM64_LDST_REGISTER_GPR);
    case 0x3C:
        return arm64_decode_ldst_unscaled(raw, decoded, ARM64_LDST_REGISTER_FP_SIMD);
    case 0x3D:
        return arm64_decode_ldst_unsigned(raw, decoded, ARM64_LDST_REGISTER_FP_SIMD);

    default:
        return ARM64_DECODE_UNALLOCATED;
    }
}