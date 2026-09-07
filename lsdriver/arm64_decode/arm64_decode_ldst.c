#include "arm64_decode.h"

static void arm64_decode_ldst_rt_rn(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
}

static inline uint32_t arm64_decode_ldst_size(uint32_t raw)
{
    return ARM64_DECODE_FIELD(raw, 31, 30);
}

static inline uint32_t arm64_decode_ldst_opc(uint32_t raw)
{
    return ARM64_DECODE_FIELD(raw, 23, 22);
}

static inline uint32_t arm64_decode_ldst_access_bytes(uint32_t raw)
{
    return 1U << arm64_decode_ldst_size(raw);
}

static inline uint32_t arm64_decode_ldst_simd_access_bytes(uint32_t raw)
{
    uint32_t size = arm64_decode_ldst_size(raw);
    uint32_t opc = arm64_decode_ldst_opc(raw);

    if (size == 0 && (opc & 2)) return 16;
    return 1U << size;
}

static inline uint32_t arm64_decode_ldst_gpr_width(uint32_t size, uint32_t opc)
{
    if (size == 3 || opc == 2) return 64;
    return 32;
}

static inline uint32_t arm64_decode_ldst_operand_width(int is_fp_simd, uint32_t access_bytes, uint32_t size, uint32_t opc)
{
    if (is_fp_simd) return access_bytes * 8;
    return arm64_decode_ldst_gpr_width(size, opc);
}

static inline uint32_t arm64_decode_ldst_offset_scale(int prefetch, uint32_t access_bytes)
{
    if (prefetch) return 8;
    return access_bytes;
}

static inline uint32_t arm64_decode_ldst_pair_gpr_width(uint32_t opc)
{
    switch (opc)
    {
    case 0:
        return 32;
    default:
        return 64;
    }
}

static inline uint32_t arm64_decode_ldst_literal_gpr_width(uint32_t size)
{
    switch (size)
    {
    case 0:
        return 32;
    default:
        return 64;
    }
}

static inline uint32_t arm64_decode_ldst_casp_width(uint32_t size)
{
    switch (size)
    {
    case 0:
        return 32;
    default:
        return 64;
    }
}

static enum arm64_instruction arm64_decode_ldst_literal_instruction(uint32_t size, int is_fp_simd, int prefetch)
{
    if (prefetch) return ARM64_INST_PRFM_LITERAL;
    if (is_fp_simd) return ARM64_INST_LDR_FP_SIMD_LITERAL;
    if (size == 2) return ARM64_INST_LDRSW_LITERAL;
    return ARM64_INST_LDR_GPR_LITERAL;
}

static enum arm64_instruction arm64_decode_ldst_ldapur_instruction(uint32_t size, uint32_t opc)
{
    switch (opc)
    {
    case 0:
        switch (size)
        {
        case 0:
            return ARM64_INST_STLURB;
        case 1:
            return ARM64_INST_STLURH;
        default:
            return ARM64_INST_STLUR;
        }
    case 1:
        switch (size)
        {
        case 0:
            return ARM64_INST_LDAPURB;
        case 1:
            return ARM64_INST_LDAPURH;
        default:
            return ARM64_INST_LDAPUR;
        }
    default:
        switch (size)
        {
        case 0:
            return ARM64_INST_LDAPURSB;
        case 1:
            return ARM64_INST_LDAPURSH;
        default:
            return ARM64_INST_LDAPURSW;
        }
    }
}

static inline uint32_t arm64_decode_ldst_ldapur_width(uint32_t size, uint32_t opc)
{
    if (size == 3 || opc == 2) return 64;
    return 32;
}

static enum arm64_instruction arm64_decode_ldst_ldapr_instruction(uint32_t size)
{
    switch (size)
    {
    case 0:
        return ARM64_INST_LDAPRB;
    case 1:
        return ARM64_INST_LDAPRH;
    default:
        return ARM64_INST_LDAPR;
    }
}

static enum arm64_instruction arm64_decode_ldst_cas_instruction(uint32_t size, uint32_t selector)
{
    switch (selector)
    {
    case 0:
        switch (size)
        {
        case 0: return ARM64_INST_CASB;
        case 1: return ARM64_INST_CASH;
        default: return ARM64_INST_CAS;
        }
    case 1:
        switch (size)
        {
        case 0: return ARM64_INST_CASLB;
        case 1: return ARM64_INST_CASLH;
        default: return ARM64_INST_CASL;
        }
    case 2:
        switch (size)
        {
        case 0: return ARM64_INST_CASAB;
        case 1: return ARM64_INST_CASAH;
        default: return ARM64_INST_CASA;
        }
    case 3:
        switch (size)
        {
        case 0: return ARM64_INST_CASALB;
        case 1: return ARM64_INST_CASALH;
        default: return ARM64_INST_CASAL;
        }
    default:
        return ARM64_INST_UNKNOWN;
    }
}

static enum arm64_instruction arm64_decode_ldst_exclusive_instruction(uint32_t size, uint32_t selector)
{
    switch (selector)
    {
    case 0:
        switch (size)
        {
        case 0: return ARM64_INST_STXRB;
        case 1: return ARM64_INST_STXRH;
        default: return ARM64_INST_STXR;
        }
    case 1:
        switch (size)
        {
        case 0: return ARM64_INST_STLXRB;
        case 1: return ARM64_INST_STLXRH;
        default: return ARM64_INST_STLXR;
        }
    case 2:
    case 3:
        switch (size)
        {
        default: return selector == 2 ? ARM64_INST_STXP : ARM64_INST_STLXP;
        }
    case 4:
        switch (size)
        {
        case 0: return ARM64_INST_LDXRB;
        case 1: return ARM64_INST_LDXRH;
        default: return ARM64_INST_LDXR;
        }
    case 5:
        switch (size)
        {
        case 0: return ARM64_INST_LDAXRB;
        case 1: return ARM64_INST_LDAXRH;
        default: return ARM64_INST_LDAXR;
        }
    case 6:
    case 7:
        switch (size)
        {
        default: return selector == 6 ? ARM64_INST_LDXP : ARM64_INST_LDAXP;
        }
    case 8:
        switch (size)
        {
        case 0: return ARM64_INST_STLLRB;
        case 1: return ARM64_INST_STLLRH;
        default: return ARM64_INST_STLLR;
        }
    case 9:
        switch (size)
        {
        case 0: return ARM64_INST_STLRB;
        case 1: return ARM64_INST_STLRH;
        default: return ARM64_INST_STLR;
        }
    case 12:
        switch (size)
        {
        case 0: return ARM64_INST_LDLARB;
        case 1: return ARM64_INST_LDLARH;
        default: return ARM64_INST_LDLAR;
        }
    case 13:
        switch (size)
        {
        case 0: return ARM64_INST_LDARB;
        case 1: return ARM64_INST_LDARH;
        default: return ARM64_INST_LDAR;
        }
    default:
        return ARM64_INST_UNKNOWN;
    }
}

static inline uint32_t arm64_decode_ldst_ld1_element_width(uint32_t opcode, uint32_t size)
{
    if (opcode == 0) return 8;
    if (opcode == 2) return 16;
    return 32U << (size & 1);
}

static inline uint32_t arm64_decode_ldst_ld1_lane(uint32_t opcode, uint32_t size, uint32_t q, uint32_t s)
{
    if (opcode == 0) return (q << 3) | (s << 2) | size;
    if (opcode == 2) return (q << 2) | (s << 1) | (size >> 1);
    if (size != 3) return (q << 1) | s;
    return q;
}

static inline uint32_t arm64_decode_ldst_atomic_width(uint32_t access_bytes)
{
    switch (access_bytes)
    {
    case 8:
        return 64;
    default:
        return 32;
    }
}

static inline uint32_t arm64_decode_ldst_pair_access_bytes(uint32_t opc, int is_fp_simd)
{
    if (is_fp_simd) return 4U << opc;
    if (opc == 2) return 8;
    return 4;
}

static inline uint32_t arm64_decode_ldst_literal_access_bytes(uint32_t size, int is_fp_simd)
{
    if (is_fp_simd)
    {
        switch (size)
        {
        case 0:
            return 4;
        case 1:
            return 8;
        case 2:
            return 16;
        default:
            return 0;
        }
    }

    switch (size)
    {
    case 0:
        return 4;
    case 1:
        return 8;
    case 2:
        return 4;
    default:
        return 0;
    }
}

static inline uint8_t arm64_decode_ldst_immediate_address_mode(uint32_t mode)
{
    switch (mode)
    {
    case 0:
        return 1;
    case 1:
        return 3;
    case 2:
        return 5;
    default:
        return 4;
    }
}

static enum arm64_instruction arm64_decode_lse_atomic_instruction(uint32_t raw)
{
    uint32_t size = arm64_decode_ldst_size(raw);
    uint32_t selector = (ARM64_DECODE_FIELD(raw, 15, 12) << 2) | (ARM64_DECODE_BIT(raw, 23) << 1) | ARM64_DECODE_BIT(raw, 22);

    switch (selector)
    {
    case 0:
        switch (size) { case 0: return ARM64_INST_LDADDB; case 1: return ARM64_INST_LDADDH; default: return ARM64_INST_LDADD; }
    case 1:
        switch (size) { case 0: return ARM64_INST_LDADDLB; case 1: return ARM64_INST_LDADDLH; default: return ARM64_INST_LDADDL; }
    case 2:
        switch (size) { case 0: return ARM64_INST_LDADDAB; case 1: return ARM64_INST_LDADDAH; default: return ARM64_INST_LDADDA; }
    case 3:
        switch (size) { case 0: return ARM64_INST_LDADDALB; case 1: return ARM64_INST_LDADDALH; default: return ARM64_INST_LDADDAL; }
    case 4:
        switch (size) { case 0: return ARM64_INST_LDCLRB; case 1: return ARM64_INST_LDCLRH; default: return ARM64_INST_LDCLR; }
    case 5:
        switch (size) { case 0: return ARM64_INST_LDCLRLB; case 1: return ARM64_INST_LDCLRLH; default: return ARM64_INST_LDCLRL; }
    case 6:
        switch (size) { case 0: return ARM64_INST_LDCLRAB; case 1: return ARM64_INST_LDCLRAH; default: return ARM64_INST_LDCLRA; }
    case 7:
        switch (size) { case 0: return ARM64_INST_LDCLRALB; case 1: return ARM64_INST_LDCLRALH; default: return ARM64_INST_LDCLRAL; }
    case 8:
        switch (size) { case 0: return ARM64_INST_LDEORB; case 1: return ARM64_INST_LDEORH; default: return ARM64_INST_LDEOR; }
    case 9:
        switch (size) { case 0: return ARM64_INST_LDEORLB; case 1: return ARM64_INST_LDEORLH; default: return ARM64_INST_LDEORL; }
    case 10:
        switch (size) { case 0: return ARM64_INST_LDEORAB; case 1: return ARM64_INST_LDEORAH; default: return ARM64_INST_LDEORA; }
    case 11:
        switch (size) { case 0: return ARM64_INST_LDEORALB; case 1: return ARM64_INST_LDEORALH; default: return ARM64_INST_LDEORAL; }
    case 12:
        switch (size) { case 0: return ARM64_INST_LDSETB; case 1: return ARM64_INST_LDSETH; default: return ARM64_INST_LDSET; }
    case 13:
        switch (size) { case 0: return ARM64_INST_LDSETLB; case 1: return ARM64_INST_LDSETLH; default: return ARM64_INST_LDSETL; }
    case 14:
        switch (size) { case 0: return ARM64_INST_LDSETAB; case 1: return ARM64_INST_LDSETAH; default: return ARM64_INST_LDSETA; }
    case 15:
        switch (size) { case 0: return ARM64_INST_LDSETALB; case 1: return ARM64_INST_LDSETALH; default: return ARM64_INST_LDSETAL; }
    case 16:
        switch (size) { case 0: return ARM64_INST_LDSMAXB; case 1: return ARM64_INST_LDSMAXH; default: return ARM64_INST_LDSMAX; }
    case 17:
        switch (size) { case 0: return ARM64_INST_LDSMAXLB; case 1: return ARM64_INST_LDSMAXLH; default: return ARM64_INST_LDSMAXL; }
    case 18:
        switch (size) { case 0: return ARM64_INST_LDSMAXAB; case 1: return ARM64_INST_LDSMAXAH; default: return ARM64_INST_LDSMAXA; }
    case 19:
        switch (size) { case 0: return ARM64_INST_LDSMAXALB; case 1: return ARM64_INST_LDSMAXALH; default: return ARM64_INST_LDSMAXAL; }
    case 20:
        switch (size) { case 0: return ARM64_INST_LDSMINB; case 1: return ARM64_INST_LDSMINH; default: return ARM64_INST_LDSMIN; }
    case 21:
        switch (size) { case 0: return ARM64_INST_LDSMINLB; case 1: return ARM64_INST_LDSMINLH; default: return ARM64_INST_LDSMINL; }
    case 22:
        switch (size) { case 0: return ARM64_INST_LDSMINAB; case 1: return ARM64_INST_LDSMINAH; default: return ARM64_INST_LDSMINA; }
    case 23:
        switch (size) { case 0: return ARM64_INST_LDSMINALB; case 1: return ARM64_INST_LDSMINALH; default: return ARM64_INST_LDSMINAL; }
    case 24:
        switch (size) { case 0: return ARM64_INST_LDUMAXB; case 1: return ARM64_INST_LDUMAXH; default: return ARM64_INST_LDUMAX; }
    case 25:
        switch (size) { case 0: return ARM64_INST_LDUMAXLB; case 1: return ARM64_INST_LDUMAXLH; default: return ARM64_INST_LDUMAXL; }
    case 26:
        switch (size) { case 0: return ARM64_INST_LDUMAXAB; case 1: return ARM64_INST_LDUMAXAH; default: return ARM64_INST_LDUMAXA; }
    case 27:
        switch (size) { case 0: return ARM64_INST_LDUMAXALB; case 1: return ARM64_INST_LDUMAXALH; default: return ARM64_INST_LDUMAXAL; }
    case 28:
        switch (size) { case 0: return ARM64_INST_LDUMINB; case 1: return ARM64_INST_LDUMINH; default: return ARM64_INST_LDUMIN; }
    case 29:
        switch (size) { case 0: return ARM64_INST_LDUMINLB; case 1: return ARM64_INST_LDUMINLH; default: return ARM64_INST_LDUMINL; }
    case 30:
        switch (size) { case 0: return ARM64_INST_LDUMINAB; case 1: return ARM64_INST_LDUMINAH; default: return ARM64_INST_LDUMINA; }
    case 31:
        switch (size) { case 0: return ARM64_INST_LDUMINALB; case 1: return ARM64_INST_LDUMINALH; default: return ARM64_INST_LDUMINAL; }
    case 32:
        switch (size) { case 0: return ARM64_INST_SWPB; case 1: return ARM64_INST_SWPH; default: return ARM64_INST_SWP; }
    case 33:
        switch (size) { case 0: return ARM64_INST_SWPLB; case 1: return ARM64_INST_SWPLH; default: return ARM64_INST_SWPL; }
    case 34:
        switch (size) { case 0: return ARM64_INST_SWPAB; case 1: return ARM64_INST_SWPAH; default: return ARM64_INST_SWPA; }
    case 35:
        switch (size) { case 0: return ARM64_INST_SWPALB; case 1: return ARM64_INST_SWPALH; default: return ARM64_INST_SWPAL; }
    default:
        return ARM64_INST_UNKNOWN;
    }
}

static int arm64_decode_ldst_is_prefetch(uint32_t raw, int is_fp_simd)
{
    return !is_fp_simd && arm64_decode_ldst_size(raw) == 3 && arm64_decode_ldst_opc(raw) == 2;
}

static enum arm64_instruction arm64_decode_ldst_single_simd_instruction(uint8_t address_mode, uint32_t load)
{
    if (load)
    {
        switch (address_mode)
        {
        case 1:
            return ARM64_INST_LDUR_FP_SIMD;
        case 3:
            return ARM64_INST_LDR_FP_SIMD_POST_INDEX;
        case 4:
            return ARM64_INST_LDR_FP_SIMD_PRE_INDEX;
        case 6:
            return ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET;
        case 7:
            return ARM64_INST_LDR_FP_SIMD_UNSIGNED_OFFSET;
        default:
            return ARM64_INST_UNKNOWN;
        }
    }

    switch (address_mode)
    {
    case 1:
        return ARM64_INST_STUR_FP_SIMD;
    case 3:
        return ARM64_INST_STR_FP_SIMD_POST_INDEX;
    case 4:
        return ARM64_INST_STR_FP_SIMD_PRE_INDEX;
    case 6:
        return ARM64_INST_STR_FP_SIMD_REGISTER_OFFSET;
    case 7:
        return ARM64_INST_STR_FP_SIMD_UNSIGNED_OFFSET;
    default:
        return ARM64_INST_UNKNOWN;
    }
}

static enum arm64_instruction arm64_decode_ldst_single_gpr_instruction(uint32_t size, uint32_t opc, uint8_t address_mode)
{
    if (opc == 0)
    {
        switch (address_mode)
        {
        case 1:
            return ARM64_INST_STUR_GPR;
        case 5:
            return ARM64_INST_STTR_GPR;
        case 3:
            return ARM64_INST_STR_GPR_POST_INDEX;
        case 4:
            return ARM64_INST_STR_GPR_PRE_INDEX;
        case 6:
            return ARM64_INST_STR_GPR_REGISTER_OFFSET;
        case 7:
            switch (size)
            {
            case 0:
                return ARM64_INST_STRB_GPR_UNSIGNED_OFFSET;
            case 1:
                return ARM64_INST_STRH_GPR_UNSIGNED_OFFSET;
            default:
                return ARM64_INST_STR_GPR_UNSIGNED_OFFSET;
            }
        default:
            return ARM64_INST_UNKNOWN;
        }
    }

    if (opc == 1)
    {
        switch (address_mode)
        {
        case 1:
            switch (size)
            {
            case 0:
                return ARM64_INST_LDURB_GPR;
            case 1:
                return ARM64_INST_LDURH_GPR;
            default:
                return ARM64_INST_LDUR_GPR;
            }
        case 5:
            switch (size)
            {
            case 0:
                return ARM64_INST_LDTRB_GPR;
            case 1:
                return ARM64_INST_LDTRH_GPR;
            default:
                return ARM64_INST_LDTR_GPR;
            }
        case 3:
            switch (size)
            {
            case 0:
                return ARM64_INST_LDRB_GPR_POST_INDEX;
            case 1:
                return ARM64_INST_LDRH_GPR_POST_INDEX;
            default:
                return ARM64_INST_LDR_GPR_POST_INDEX;
            }
        case 4:
            switch (size)
            {
            case 0:
                return ARM64_INST_LDRB_GPR_PRE_INDEX;
            case 1:
                return ARM64_INST_LDRH_GPR_PRE_INDEX;
            default:
                return ARM64_INST_LDR_GPR_PRE_INDEX;
            }
        case 6:
            switch (size)
            {
            case 0:
                return ARM64_INST_LDRB_GPR_REGISTER_OFFSET;
            case 1:
                return ARM64_INST_LDRH_GPR_REGISTER_OFFSET;
            default:
                return ARM64_INST_LDR_GPR_REGISTER_OFFSET;
            }
        case 7:
            switch (size)
            {
            case 0:
                return ARM64_INST_LDRB_GPR_UNSIGNED_OFFSET;
            case 1:
                return ARM64_INST_LDRH_GPR_UNSIGNED_OFFSET;
            default:
                return ARM64_INST_LDR_GPR_UNSIGNED_OFFSET;
            }
        default:
            return ARM64_INST_UNKNOWN;
        }
    }

    switch (address_mode)
    {
    case 1:
        switch (size)
        {
        case 0:
            return ARM64_INST_LDURSB_GPR;
        case 1:
            return ARM64_INST_LDURSH_GPR;
        default:
            return ARM64_INST_LDURSW_GPR;
        }
    case 5:
        switch (size)
        {
        case 0:
            return ARM64_INST_LDTRSB_GPR;
        case 1:
            return ARM64_INST_LDTRSH_GPR;
        default:
            return ARM64_INST_LDTRSW_GPR;
        }
    case 3:
        switch (size)
        {
        case 0:
            return ARM64_INST_LDRSB_GPR_POST_INDEX;
        case 1:
            return ARM64_INST_LDRSH_GPR_POST_INDEX;
        default:
            return ARM64_INST_LDRSW_GPR_POST_INDEX;
        }
    case 4:
        switch (size)
        {
        case 0:
            return ARM64_INST_LDRSB_GPR_PRE_INDEX;
        case 1:
            return ARM64_INST_LDRSH_GPR_PRE_INDEX;
        default:
            return ARM64_INST_LDRSW_GPR_PRE_INDEX;
        }
    case 6:
        switch (size)
        {
        case 0:
            return ARM64_INST_LDRSB_GPR_REGISTER_OFFSET;
        case 1:
            return ARM64_INST_LDRSH_GPR_REGISTER_OFFSET;
        default:
            return ARM64_INST_LDRSW_GPR_REGISTER_OFFSET;
        }
    case 7:
        switch (size)
        {
        case 0:
            return ARM64_INST_LDRSB_GPR_UNSIGNED_OFFSET;
        case 1:
            return ARM64_INST_LDRSH_GPR_UNSIGNED_OFFSET;
        default:
            return ARM64_INST_LDRSW_GPR_UNSIGNED_OFFSET;
        }
    default:
        return ARM64_INST_UNKNOWN;
    }
}

static enum arm64_instruction arm64_decode_ldst_single_instruction(uint32_t raw, int is_fp_simd, uint8_t address_mode, int prefetch)
{
    if (prefetch)
    {
        switch (address_mode)
        {
        case 1:
            return ARM64_INST_PRFUM;
        case 6:
            return ARM64_INST_PRFM_REGISTER_OFFSET;
        case 7:
            return ARM64_INST_PRFM_UNSIGNED_OFFSET;
        default:
            return ARM64_INST_UNKNOWN;
        }
    }

    if (is_fp_simd)
    {
        return arm64_decode_ldst_single_simd_instruction(address_mode, arm64_decode_ldst_opc(raw) & 1);
    }
    return arm64_decode_ldst_single_gpr_instruction(arm64_decode_ldst_size(raw), arm64_decode_ldst_opc(raw), address_mode);
}

static enum arm64_instruction arm64_decode_ldst_pair_insn(int is_fp_simd, uint8_t address_mode, uint32_t load, uint32_t opc)
{
    if (is_fp_simd)
    {
        if (load)
        {
            switch (address_mode)
            {
            case 2:
                return ARM64_INST_LDNP_FP_SIMD;
            case 0:
                return ARM64_INST_LDP_FP_SIMD_OFFSET;
            case 3:
                return ARM64_INST_LDP_FP_SIMD_POST_INDEX;
            case 4:
                return ARM64_INST_LDP_FP_SIMD_PRE_INDEX;
            default:
                return ARM64_INST_UNKNOWN;
            }
        }

        switch (address_mode)
        {
        case 2:
            return ARM64_INST_STNP_FP_SIMD;
        case 0:
            return ARM64_INST_STP_FP_SIMD_OFFSET;
        case 3:
            return ARM64_INST_STP_FP_SIMD_POST_INDEX;
        case 4:
            return ARM64_INST_STP_FP_SIMD_PRE_INDEX;
        default:
            return ARM64_INST_UNKNOWN;
        }
    }

    if (load && opc == 1)
    {
        switch (address_mode)
        {
        case 0:
            return ARM64_INST_LDPSW_OFFSET;
        case 3:
            return ARM64_INST_LDPSW_POST_INDEX;
        case 4:
            return ARM64_INST_LDPSW_PRE_INDEX;
        default:
            return ARM64_INST_UNKNOWN;
        }
    }

    if (load)
    {
        switch (address_mode)
        {
        case 2:
            return ARM64_INST_LDNP_GPR;
        case 0:
            return ARM64_INST_LDP_GPR_OFFSET;
        case 3:
            return ARM64_INST_LDP_GPR_POST_INDEX;
        case 4:
            return ARM64_INST_LDP_GPR_PRE_INDEX;
        default:
            return ARM64_INST_UNKNOWN;
        }
    }

    switch (address_mode)
    {
    case 2:
        return ARM64_INST_STNP_GPR;
    case 0:
        return ARM64_INST_STP_GPR_OFFSET;
    case 3:
        return ARM64_INST_STP_GPR_POST_INDEX;
    case 4:
        return ARM64_INST_STP_GPR_PRE_INDEX;
    default:
        return ARM64_INST_UNKNOWN;
    }
}

static enum arm64_decode_status arm64_decode_ldst_atomic(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint32_t access_bytes = arm64_decode_ldst_access_bytes(raw);

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
        decoded->instruction = arm64_decode_ldst_ldapr_instruction(ARM64_DECODE_FIELD(raw, 31, 30));
        arm64_decode_ldst_rt_rn(raw, decoded);
        decoded->operand_width = arm64_decode_ldst_atomic_width(access_bytes);
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
    decoded->rs = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = arm64_decode_ldst_atomic_width(access_bytes);
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_single(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd)
{
    uint32_t size = arm64_decode_ldst_size(raw);
    uint32_t opc = arm64_decode_ldst_opc(raw);
    uint32_t access_bytes = arm64_decode_ldst_access_bytes(raw);
    int prefetch = arm64_decode_ldst_is_prefetch(raw, is_fp_simd);

    if (prefetch)
    {
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->immediate = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->operand_width = 0;
    }
    else
    {
        arm64_decode_ldst_rt_rn(raw, decoded);
        if (is_fp_simd && opc >= 2 && size != 0) return ARM64_DECODE_UNALLOCATED;
        if (is_fp_simd) access_bytes = arm64_decode_ldst_simd_access_bytes(raw);
        decoded->operand_width = arm64_decode_ldst_operand_width(is_fp_simd, access_bytes, size, opc);
    }

    if (!is_fp_simd && opc == 3 && size >= 2) return ARM64_DECODE_UNALLOCATED;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_literal(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd)
{
    uint32_t size = arm64_decode_ldst_size(raw);
    int prefetch = !is_fp_simd && size == 3;
    uint32_t access_bytes = arm64_decode_ldst_literal_access_bytes(size, is_fp_simd);

    if (!prefetch) decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
    else decoded->immediate = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->offset = ARM64_DECODE_SIGN_EXTEND((uint64_t)ARM64_DECODE_FIELD(raw, 23, 5) << 2, 21);
    if (prefetch) decoded->operand_width = 0;
    else if (is_fp_simd) decoded->operand_width = access_bytes * 8;
    else decoded->operand_width = arm64_decode_ldst_literal_gpr_width(size);
    if (is_fp_simd && !access_bytes) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = arm64_decode_ldst_literal_instruction(size, is_fp_simd, prefetch);
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_pair(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd, uint8_t low_mode, uint8_t high_mode)
{
    uint32_t opc = arm64_decode_ldst_size(raw);
    uint32_t load = ARM64_DECODE_BIT(raw, 22);
    uint8_t address_mode = low_mode;
    uint32_t access_bytes;

    if (ARM64_DECODE_BIT(raw, 23)) address_mode = high_mode;

    arm64_decode_ldst_rt_rn(raw, decoded);
    decoded->rt2 = ARM64_DECODE_FIELD(raw, 14, 10);
    if (is_fp_simd)
    {
        if (opc == 3) return ARM64_DECODE_UNALLOCATED;
        access_bytes = arm64_decode_ldst_pair_access_bytes(opc, 1);
        decoded->operand_width = access_bytes * 8;
    }
    else
    {
        if (opc == 3 || (opc == 1 && address_mode == 2)) return ARM64_DECODE_UNALLOCATED;
        if (opc == 1 && !load) return ARM64_DECODE_UNSUPPORTED;
        access_bytes = arm64_decode_ldst_pair_access_bytes(opc, 0);
        decoded->operand_width = arm64_decode_ldst_pair_gpr_width(opc);
    }
    if (!is_fp_simd && (address_mode == 3 || address_mode == 4) && decoded->rn != 31 && (decoded->rn == decoded->rt || decoded->rn == decoded->rt2)) return ARM64_DECODE_UNPREDICTABLE;
    if (load && decoded->rt == decoded->rt2) return ARM64_DECODE_UNPREDICTABLE;
    decoded->offset = ARM64_DECODE_SIGN_EXTEND(ARM64_DECODE_FIELD(raw, 21, 15), 7) * access_bytes;
    decoded->instruction = arm64_decode_ldst_pair_insn(is_fp_simd, address_mode, load, opc);
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_unsigned(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd)
{
    enum arm64_decode_status status = arm64_decode_ldst_single(raw, decoded, is_fp_simd);
    int prefetch;
    uint32_t access_bytes = is_fp_simd ? arm64_decode_ldst_simd_access_bytes(raw) : arm64_decode_ldst_access_bytes(raw);

    if (status != ARM64_DECODE_OK) return status;
    prefetch = decoded->operand_width == 0;
    decoded->offset = ARM64_DECODE_FIELD(raw, 21, 10) * arm64_decode_ldst_offset_scale(prefetch, access_bytes);
    decoded->instruction = arm64_decode_ldst_single_instruction(raw, is_fp_simd, 7, prefetch);
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_unscaled(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd)
{
    uint8_t address_mode;
    uint32_t mode;
    int prefetch;

    if (!(raw & 0x00200000U))
    {
        enum arm64_decode_status status = arm64_decode_ldst_single(raw, decoded, is_fp_simd);

        if (status != ARM64_DECODE_OK) return status;
        mode = ARM64_DECODE_FIELD(raw, 11, 10);
        prefetch = decoded->operand_width == 0;
        if (is_fp_simd && mode == 2) return ARM64_DECODE_UNALLOCATED;
        if (prefetch && mode != 0) return ARM64_DECODE_UNALLOCATED;
        address_mode = arm64_decode_ldst_immediate_address_mode(mode);
        decoded->offset = ARM64_DECODE_SIGN_EXTEND(ARM64_DECODE_FIELD(raw, 20, 12), 9);
        decoded->instruction = arm64_decode_ldst_single_instruction(raw, is_fp_simd, address_mode, prefetch);
    }
    else
    {
        mode = ARM64_DECODE_FIELD(raw, 11, 10);
        if (mode == 0)
        {
            if (is_fp_simd) return ARM64_DECODE_UNALLOCATED;
            return arm64_decode_ldst_atomic(raw, decoded);
        }
        if (mode != 2)
        {
            if (!is_fp_simd && arm64_decode_ldst_size(raw) == 3) return ARM64_DECODE_UNSUPPORTED;
            return ARM64_DECODE_UNALLOCATED;
        }

        enum arm64_decode_status status = arm64_decode_ldst_single(raw, decoded, is_fp_simd);
        uint32_t access_bytes = is_fp_simd ? arm64_decode_ldst_simd_access_bytes(raw) : arm64_decode_ldst_access_bytes(raw);

        if (status != ARM64_DECODE_OK) return status;
        prefetch = decoded->operand_width == 0;
        if (prefetch && (raw & 0x18U) == 0x18U) return ARM64_DECODE_UNSUPPORTED;
        address_mode = 6;
        decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
        decoded->extend_type = ARM64_DECODE_FIELD(raw, 15, 13);
        if (decoded->extend_type != 2 && decoded->extend_type != 3 && decoded->extend_type != 6 && decoded->extend_type != 7) return ARM64_DECODE_UNALLOCATED;
        decoded->shift_amount = 0;
        if (raw & 0x1000U)
        {
            decoded->shift_amount = prefetch ? 3 : (uint8_t)__builtin_ctz(access_bytes);
        }
        decoded->instruction = arm64_decode_ldst_single_instruction(raw, is_fp_simd, address_mode, prefetch);
    }

    if (!is_fp_simd && (address_mode == 4 || address_mode == 3) && decoded->rn != 31 && decoded->rn == decoded->rt) return ARM64_DECODE_UNPREDICTABLE;
    return ARM64_DECODE_OK;
}

/*
解码访存、原子和独占指令。bits[29:24] 先确定唯一编码 owner，叶子只校验
本族固定字段和寄存器约束，不依赖宽窄掩码的排列顺序。
*/
enum arm64_decode_status arm64_decode_load_store(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    decoded->instruction_class = ARM64_INSTRUCTION_CLASS_LOAD_STORE;
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
                decoded->instruction = ARM64_INST_CASP;
                break;
            case 1:
                decoded->instruction = ARM64_INST_CASPL;
                break;
            case 2:
                decoded->instruction = ARM64_INST_CASPA;
                break;
            case 3:
                decoded->instruction = ARM64_INST_CASPAL;
                break;
            }
            arm64_decode_ldst_rt_rn(raw, decoded);
            decoded->rs = (raw >> 16) & 0x1F;
            decoded->operand_width = arm64_decode_ldst_casp_width(size);
            return ARM64_DECODE_OK;
        }
        case 0x00A07C00U:
        {
            uint32_t size = (raw >> 30) & 0x3;
            switch (((raw >> 21) & 2) | ((raw >> 15) & 1))
            {
            case 0:
                decoded->instruction = arm64_decode_ldst_cas_instruction(size, 0);
                break;
            case 1:
                decoded->instruction = arm64_decode_ldst_cas_instruction(size, 1);
                break;
            case 2:
                decoded->instruction = arm64_decode_ldst_cas_instruction(size, 2);
                break;
            case 3:
                decoded->instruction = arm64_decode_ldst_cas_instruction(size, 3);
                break;
            }
            arm64_decode_ldst_rt_rn(raw, decoded);
            decoded->rs = (raw >> 16) & 0x1F;
            decoded->operand_width = arm64_decode_ldst_atomic_width(1U << size);
            return ARM64_DECODE_OK;
        }
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
                decoded->instruction = arm64_decode_ldst_exclusive_instruction(size, 0);
                break;
            case 0x1:
                decoded->instruction = arm64_decode_ldst_exclusive_instruction(size, 1);
                break;
            case 0x2:
                decoded->instruction = ARM64_INST_STXP;
                break;
            case 0x3:
                decoded->instruction = ARM64_INST_STLXP;
                break;
            case 0x4:
                decoded->instruction = arm64_decode_ldst_exclusive_instruction(size, 4);
                break;
            case 0x5:
                decoded->instruction = arm64_decode_ldst_exclusive_instruction(size, 5);
                break;
            case 0x6:
                decoded->instruction = ARM64_INST_LDXP;
                break;
            case 0x7:
                decoded->instruction = ARM64_INST_LDAXP;
                break;
            case 0x8:
                decoded->instruction = arm64_decode_ldst_exclusive_instruction(size, 8);
                break;
            case 0x9:
                decoded->instruction = arm64_decode_ldst_exclusive_instruction(size, 9);
                break;
            case 0xC:
                decoded->instruction = arm64_decode_ldst_exclusive_instruction(size, 12);
                break;
            case 0xD:
                decoded->instruction = arm64_decode_ldst_exclusive_instruction(size, 13);
                break;
            default:
                return ARM64_DECODE_UNALLOCATED;
            }
            arm64_decode_ldst_rt_rn(raw, decoded);
            if (pair) decoded->rt2 = rt2;
            if (!ordered && !load) decoded->rs = rs;
            decoded->operand_width = arm64_decode_ldst_atomic_width(1U << size);
            if (pair && load && decoded->rt == decoded->rt2) return ARM64_DECODE_UNPREDICTABLE;
            if (!load && rs != 31 && (rs == decoded->rt || (pair && rs == decoded->rt2) || (decoded->rn != 31 && rs == decoded->rn))) return ARM64_DECODE_UNPREDICTABLE;
            return ARM64_DECODE_OK;
        }

    case 0x09:
    case 0x0C:
        return ARM64_DECODE_UNSUPPORTED;
    case 0x0D:
    {
        uint32_t opcode = (raw >> 13) & 0x7;
        uint32_t size = (raw >> 10) & 0x3;
        uint32_t q = (raw >> 30) & 1;
        uint32_t s = (raw >> 12) & 1;

        if ((raw & 0x00800000U) || (raw & 0x001F0000U)) return ARM64_DECODE_UNSUPPORTED;
        if (opcode != 0 && opcode != 2 && opcode != 4) return ARM64_DECODE_UNSUPPORTED;
        if ((opcode == 2 && (size & 1)) || (opcode == 4 && size > 1)) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = (raw >> 22) & 1 ? ARM64_INST_LD1 : ARM64_INST_ST1;
        arm64_decode_ldst_rt_rn(raw, decoded);
        decoded->element_width = arm64_decode_ldst_ld1_element_width(opcode, size);
        decoded->lane_index = arm64_decode_ldst_ld1_lane(opcode, size, q, s);
        decoded->operand_width = q ? 128 : 64;
        return ARM64_DECODE_OK;
    }

    case 0x18:
        return arm64_decode_ldst_literal(raw, decoded, 0);
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
            decoded->instruction = arm64_decode_ldst_ldapur_instruction(size, opc);
            arm64_decode_ldst_rt_rn(raw, decoded);
            decoded->offset = ARM64_DECODE_SIGN_EXTEND(ARM64_DECODE_FIELD(raw, 20, 12), 9);
            decoded->operand_width = arm64_decode_ldst_ldapur_width(size, opc);
            return ARM64_DECODE_OK;
        }
        return ARM64_DECODE_UNSUPPORTED;
    case 0x1C:
        return arm64_decode_ldst_literal(raw, decoded, 1);
    case 0x1D:
        return ARM64_DECODE_UNSUPPORTED;

    case 0x28:
        return arm64_decode_ldst_pair(raw, decoded, 0, 2, 3);
    case 0x29:
        return arm64_decode_ldst_pair(raw, decoded, 0, 0, 4);
    case 0x2C:
        return arm64_decode_ldst_pair(raw, decoded, 1, 2, 3);
    case 0x2D:
        return arm64_decode_ldst_pair(raw, decoded, 1, 0, 4);

    case 0x38:
        return arm64_decode_ldst_unscaled(raw, decoded, 0);
    case 0x39:
        return arm64_decode_ldst_unsigned(raw, decoded, 0);
    case 0x3C:
        return arm64_decode_ldst_unscaled(raw, decoded, 1);
    case 0x3D:
        return arm64_decode_ldst_unsigned(raw, decoded, 1);

    default:
        return ARM64_DECODE_UNALLOCATED;
    }
}
