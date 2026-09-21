#include "arm64_decode.h"

static void arm64_decode_ldst_rt_rn(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
}

static inline enum arm64_decode_status arm64_decode_ldst_set_instruction(enum arm64_instruction instruction, struct arm64_decoded_instruction *decoded)
{
    if (instruction == ARM64_INST_UNKNOWN)
    {
        return ARM64_DECODE_UNSUPPORTED;
    }
    decoded->instruction = instruction;
    return ARM64_DECODE_OK;
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
    return ARM64_DECODE_SCALE(1U, arm64_decode_ldst_size(raw));
}

static inline uint32_t arm64_decode_ldst_simd_access_bytes(uint32_t raw)
{
    uint32_t size = arm64_decode_ldst_size(raw);
    uint32_t opc = arm64_decode_ldst_opc(raw);

    if (size == 0 && (opc & 2)) return 16;
    return ARM64_DECODE_SCALE(1U, size);
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

static inline enum arm64_decode_status arm64_decode_ldst_literal_instruction(uint32_t size, int is_fp_simd, int prefetch, enum arm64_instruction *instruction)
{
    if (prefetch)
    {
        *instruction = ARM64_INST_PRFM_LITERAL;
        return ARM64_DECODE_OK;
    }
    if (is_fp_simd)
    {
        *instruction = ARM64_INST_LDR_FP_SIMD_LITERAL;
        return ARM64_DECODE_OK;
    }
    if (size == 2)
    {
        *instruction = ARM64_INST_LDRSW_LITERAL;
        return ARM64_DECODE_OK;
    }
    *instruction = ARM64_INST_LDR_GPR_LITERAL;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_ldst_ldapur_instruction(uint32_t size, uint32_t opc, enum arm64_instruction *instruction)
{
    switch (opc)
    {
    case 0:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_STLURB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_STLURH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_STLUR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 1:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDAPURB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDAPURH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDAPUR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    default:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDAPURSB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDAPURSH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDAPURSW;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    }
}

static inline uint32_t arm64_decode_ldst_ldapur_width(uint32_t size, uint32_t opc)
{
    if (size == 3 || opc == 2) return 64;
    return 32;
}

static inline enum arm64_decode_status arm64_decode_ldst_ldapr_instruction(uint32_t size, enum arm64_instruction *instruction)
{
    switch (size)
    {
    case 0:
        *instruction = ARM64_INST_LDAPRB;
        return ARM64_DECODE_OK;
    case 1:
        *instruction = ARM64_INST_LDAPRH;
        return ARM64_DECODE_OK;
    case 2:
    case 3:
        *instruction = ARM64_INST_LDAPR;
        return ARM64_DECODE_OK;

    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

static inline enum arm64_decode_status arm64_decode_ldst_cas_instruction(uint32_t size, uint32_t selector, enum arm64_instruction *instruction)
{
    switch (selector)
    {
    case 0:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_CASB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_CASH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_CAS;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 1:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_CASLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_CASLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_CASL;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 2:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_CASAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_CASAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_CASA;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 3:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_CASALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_CASALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_CASAL;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

static inline enum arm64_decode_status arm64_decode_ldst_exclusive_instruction(uint32_t size, uint32_t selector, enum arm64_instruction *instruction)
{
    switch (selector)
    {
    case 0:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_STXRB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_STXRH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_STXR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 1:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_STLXRB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_STLXRH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_STLXR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 2:
    case 3:
        switch (size)
        {
        case 0:
        case 1:
        case 2:
        case 3:
            *instruction = selector == 2 ? ARM64_INST_STXP : ARM64_INST_STLXP;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 4:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDXRB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDXRH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDXR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 5:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDAXRB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDAXRH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDAXR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 6:
    case 7:
        switch (size)
        {
        case 0:
        case 1:
        case 2:
        case 3:
            *instruction = selector == 6 ? ARM64_INST_LDXP : ARM64_INST_LDAXP;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 8:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_STLLRB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_STLLRH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_STLLR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 9:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_STLRB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_STLRH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_STLR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 12:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDLARB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDLARH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDLAR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 13:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDARB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDARH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDAR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    default:
        return ARM64_DECODE_UNSUPPORTED;
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

static inline enum arm64_decode_status arm64_decode_lse_atomic_instruction(uint32_t raw, enum arm64_instruction *instruction)
{
    uint32_t size = arm64_decode_ldst_size(raw);
    uint32_t selector = ARM64_DECODE_CONCAT(ARM64_DECODE_FIELD(raw, 15, 12), ARM64_DECODE_BIT_PAIR(raw, 23, 22), 2U);

    switch (selector)
    {
    case 0:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDADDB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDADDH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDADD;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 1:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDADDLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDADDLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDADDL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 2:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDADDAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDADDAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDADDA;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 3:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDADDALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDADDALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDADDAL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 4:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDCLRB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDCLRH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDCLR;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 5:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDCLRLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDCLRLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDCLRL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 6:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDCLRAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDCLRAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDCLRA;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 7:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDCLRALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDCLRALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDCLRAL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 8:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDEORB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDEORH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDEOR;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 9:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDEORLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDEORLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDEORL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 10:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDEORAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDEORAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDEORA;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 11:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDEORALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDEORALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDEORAL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 12:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSETB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSETH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSET;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 13:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSETLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSETLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSETL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 14:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSETAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSETAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSETA;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 15:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSETALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSETALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSETAL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 16:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSMAXB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSMAXH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSMAX;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 17:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSMAXLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSMAXLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSMAXL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 18:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSMAXAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSMAXAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSMAXA;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 19:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSMAXALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSMAXALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSMAXAL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 20:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSMINB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSMINH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSMIN;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 21:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSMINLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSMINLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSMINL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 22:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSMINAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSMINAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSMINA;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 23:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDSMINALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDSMINALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDSMINAL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 24:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDUMAXB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDUMAXH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDUMAX;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 25:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDUMAXLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDUMAXLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDUMAXL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 26:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDUMAXAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDUMAXAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDUMAXA;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 27:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDUMAXALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDUMAXALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDUMAXAL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 28:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDUMINB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDUMINH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDUMIN;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 29:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDUMINLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDUMINLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDUMINL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 30:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDUMINAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDUMINAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDUMINA;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 31:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDUMINALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDUMINALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDUMINAL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 32:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_SWPB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_SWPH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_SWP;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 33:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_SWPLB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_SWPLH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_SWPL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 34:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_SWPAB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_SWPAH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_SWPA;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 35:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_SWPALB;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_SWPALH;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_SWPAL;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

static int arm64_decode_ldst_is_prefetch(uint32_t raw, int is_fp_simd)
{
    return !is_fp_simd && arm64_decode_ldst_size(raw) == 3 && arm64_decode_ldst_opc(raw) == 2;
}

static inline enum arm64_decode_status arm64_decode_ldst_single_simd_instruction(uint8_t address_mode, uint32_t load, enum arm64_instruction *instruction)
{
    if (load)
    {
        switch (address_mode)
        {
        case 1:
            *instruction = ARM64_INST_LDUR_FP_SIMD;
            return ARM64_DECODE_OK;
        case 3:
            *instruction = ARM64_INST_LDR_FP_SIMD_POST_INDEX;
            return ARM64_DECODE_OK;
        case 4:
            *instruction = ARM64_INST_LDR_FP_SIMD_PRE_INDEX;
            return ARM64_DECODE_OK;
        case 6:
            *instruction = ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET;
            return ARM64_DECODE_OK;
        case 7:
            *instruction = ARM64_INST_LDR_FP_SIMD_UNSIGNED_OFFSET;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    }

    switch (address_mode)
    {
    case 1:
        *instruction = ARM64_INST_STUR_FP_SIMD;
        return ARM64_DECODE_OK;
    case 3:
        *instruction = ARM64_INST_STR_FP_SIMD_POST_INDEX;
        return ARM64_DECODE_OK;
    case 4:
        *instruction = ARM64_INST_STR_FP_SIMD_PRE_INDEX;
        return ARM64_DECODE_OK;
    case 6:
        *instruction = ARM64_INST_STR_FP_SIMD_REGISTER_OFFSET;
        return ARM64_DECODE_OK;
    case 7:
        *instruction = ARM64_INST_STR_FP_SIMD_UNSIGNED_OFFSET;
        return ARM64_DECODE_OK;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

static inline enum arm64_decode_status arm64_decode_ldst_single_gpr_instruction(uint32_t size, uint32_t opc, uint8_t address_mode, enum arm64_instruction *instruction)
{
    if (opc == 0)
    {
        switch (address_mode)
        {
        case 1:
            *instruction = ARM64_INST_STUR_GPR;
            return ARM64_DECODE_OK;
        case 5:
            *instruction = ARM64_INST_STTR_GPR;
            return ARM64_DECODE_OK;
        case 3:
            *instruction = ARM64_INST_STR_GPR_POST_INDEX;
            return ARM64_DECODE_OK;
        case 4:
            *instruction = ARM64_INST_STR_GPR_PRE_INDEX;
            return ARM64_DECODE_OK;
        case 6:
            *instruction = ARM64_INST_STR_GPR_REGISTER_OFFSET;
            return ARM64_DECODE_OK;
        case 7:
            switch (size)
            {
            case 0:
                *instruction = ARM64_INST_STRB_GPR_UNSIGNED_OFFSET;
                return ARM64_DECODE_OK;
            case 1:
                *instruction = ARM64_INST_STRH_GPR_UNSIGNED_OFFSET;
                return ARM64_DECODE_OK;
            case 2:
            case 3:
                *instruction = ARM64_INST_STR_GPR_UNSIGNED_OFFSET;
                return ARM64_DECODE_OK;

            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        default:
            return ARM64_DECODE_UNSUPPORTED;
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
                *instruction = ARM64_INST_LDURB_GPR;
                return ARM64_DECODE_OK;
            case 1:
                *instruction = ARM64_INST_LDURH_GPR;
                return ARM64_DECODE_OK;
            case 2:
            case 3:
                *instruction = ARM64_INST_LDUR_GPR;
                return ARM64_DECODE_OK;

            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        case 5:
            switch (size)
            {
            case 0:
                *instruction = ARM64_INST_LDTRB_GPR;
                return ARM64_DECODE_OK;
            case 1:
                *instruction = ARM64_INST_LDTRH_GPR;
                return ARM64_DECODE_OK;
            case 2:
            case 3:
                *instruction = ARM64_INST_LDTR_GPR;
                return ARM64_DECODE_OK;

            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        case 3:
            switch (size)
            {
            case 0:
                *instruction = ARM64_INST_LDRB_GPR_POST_INDEX;
                return ARM64_DECODE_OK;
            case 1:
                *instruction = ARM64_INST_LDRH_GPR_POST_INDEX;
                return ARM64_DECODE_OK;
            case 2:
            case 3:
                *instruction = ARM64_INST_LDR_GPR_POST_INDEX;
                return ARM64_DECODE_OK;

            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        case 4:
            switch (size)
            {
            case 0:
                *instruction = ARM64_INST_LDRB_GPR_PRE_INDEX;
                return ARM64_DECODE_OK;
            case 1:
                *instruction = ARM64_INST_LDRH_GPR_PRE_INDEX;
                return ARM64_DECODE_OK;
            case 2:
            case 3:
                *instruction = ARM64_INST_LDR_GPR_PRE_INDEX;
                return ARM64_DECODE_OK;

            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        case 6:
            switch (size)
            {
            case 0:
                *instruction = ARM64_INST_LDRB_GPR_REGISTER_OFFSET;
                return ARM64_DECODE_OK;
            case 1:
                *instruction = ARM64_INST_LDRH_GPR_REGISTER_OFFSET;
                return ARM64_DECODE_OK;
            case 2:
            case 3:
                *instruction = ARM64_INST_LDR_GPR_REGISTER_OFFSET;
                return ARM64_DECODE_OK;

            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        case 7:
            switch (size)
            {
            case 0:
                *instruction = ARM64_INST_LDRB_GPR_UNSIGNED_OFFSET;
                return ARM64_DECODE_OK;
            case 1:
                *instruction = ARM64_INST_LDRH_GPR_UNSIGNED_OFFSET;
                return ARM64_DECODE_OK;
            case 2:
            case 3:
                *instruction = ARM64_INST_LDR_GPR_UNSIGNED_OFFSET;
                return ARM64_DECODE_OK;

            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    }

    switch (address_mode)
    {
    case 1:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDURSB_GPR;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDURSH_GPR;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDURSW_GPR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 5:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDTRSB_GPR;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDTRSH_GPR;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDTRSW_GPR;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 3:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDRSB_GPR_POST_INDEX;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDRSH_GPR_POST_INDEX;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDRSW_GPR_POST_INDEX;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 4:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDRSB_GPR_PRE_INDEX;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDRSH_GPR_PRE_INDEX;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDRSW_GPR_PRE_INDEX;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 6:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDRSB_GPR_REGISTER_OFFSET;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDRSH_GPR_REGISTER_OFFSET;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDRSW_GPR_REGISTER_OFFSET;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    case 7:
        switch (size)
        {
        case 0:
            *instruction = ARM64_INST_LDRSB_GPR_UNSIGNED_OFFSET;
            return ARM64_DECODE_OK;
        case 1:
            *instruction = ARM64_INST_LDRSH_GPR_UNSIGNED_OFFSET;
            return ARM64_DECODE_OK;
        case 2:
        case 3:
            *instruction = ARM64_INST_LDRSW_GPR_UNSIGNED_OFFSET;
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

static inline enum arm64_decode_status arm64_decode_ldst_single_instruction(uint32_t raw, int is_fp_simd, uint8_t address_mode, int prefetch, enum arm64_instruction *instruction)
{
    if (prefetch)
    {
        switch (address_mode)
        {
        case 1:
            *instruction = ARM64_INST_PRFUM;
            return ARM64_DECODE_OK;
        case 6:
            *instruction = ARM64_INST_PRFM_REGISTER_OFFSET;
            return ARM64_DECODE_OK;
        case 7:
            *instruction = ARM64_INST_PRFM_UNSIGNED_OFFSET;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    }

    if (is_fp_simd)
    {
        return arm64_decode_ldst_single_simd_instruction(address_mode, arm64_decode_ldst_opc(raw) & 1, instruction);
    }
    return arm64_decode_ldst_single_gpr_instruction(arm64_decode_ldst_size(raw), arm64_decode_ldst_opc(raw), address_mode, instruction);
}

static inline enum arm64_decode_status arm64_decode_ldst_pair_insn(int is_fp_simd, uint8_t address_mode, uint32_t load, uint32_t opc, enum arm64_instruction *instruction)
{
    if (is_fp_simd)
    {
        if (load)
        {
            switch (address_mode)
            {
            case 2:
                *instruction = ARM64_INST_LDNP_FP_SIMD;
                return ARM64_DECODE_OK;
            case 0:
                *instruction = ARM64_INST_LDP_FP_SIMD_OFFSET;
                return ARM64_DECODE_OK;
            case 3:
                *instruction = ARM64_INST_LDP_FP_SIMD_POST_INDEX;
                return ARM64_DECODE_OK;
            case 4:
                *instruction = ARM64_INST_LDP_FP_SIMD_PRE_INDEX;
                return ARM64_DECODE_OK;
            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        }

        switch (address_mode)
        {
        case 2:
            *instruction = ARM64_INST_STNP_FP_SIMD;
            return ARM64_DECODE_OK;
        case 0:
            *instruction = ARM64_INST_STP_FP_SIMD_OFFSET;
            return ARM64_DECODE_OK;
        case 3:
            *instruction = ARM64_INST_STP_FP_SIMD_POST_INDEX;
            return ARM64_DECODE_OK;
        case 4:
            *instruction = ARM64_INST_STP_FP_SIMD_PRE_INDEX;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    }

    if (load && opc == 1)
    {
        switch (address_mode)
        {
        case 0:
            *instruction = ARM64_INST_LDPSW_OFFSET;
            return ARM64_DECODE_OK;
        case 3:
            *instruction = ARM64_INST_LDPSW_POST_INDEX;
            return ARM64_DECODE_OK;
        case 4:
            *instruction = ARM64_INST_LDPSW_PRE_INDEX;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    }

    if (load)
    {
        switch (address_mode)
        {
        case 2:
            *instruction = ARM64_INST_LDNP_GPR;
            return ARM64_DECODE_OK;
        case 0:
            *instruction = ARM64_INST_LDP_GPR_OFFSET;
            return ARM64_DECODE_OK;
        case 3:
            *instruction = ARM64_INST_LDP_GPR_POST_INDEX;
            return ARM64_DECODE_OK;
        case 4:
            *instruction = ARM64_INST_LDP_GPR_PRE_INDEX;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    }

    switch (address_mode)
    {
    case 2:
        *instruction = ARM64_INST_STNP_GPR;
        return ARM64_DECODE_OK;
    case 0:
        *instruction = ARM64_INST_STP_GPR_OFFSET;
        return ARM64_DECODE_OK;
    case 3:
        *instruction = ARM64_INST_STP_GPR_POST_INDEX;
        return ARM64_DECODE_OK;
    case 4:
        *instruction = ARM64_INST_STP_GPR_PRE_INDEX;
        return ARM64_DECODE_OK;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

static inline enum arm64_decode_status arm64_decode_ldst_atomic(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!(ARM64_DECODE_MATCH(raw, 0x3F200C00U, 0x38200000U))) return ARM64_DECODE_UNSUPPORTED;
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    uint32_t access_bytes = arm64_decode_ldst_access_bytes(raw);

    switch (ARM64_DECODE_FIELD(raw, 15, 12))
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
        arm64_decode_ldst_rt_rn(raw, candidate);
        candidate->operand_width = arm64_decode_ldst_atomic_width(access_bytes);
        enum arm64_instruction instruction;
        enum arm64_decode_status mapping_status = arm64_decode_ldst_ldapr_instruction(ARM64_DECODE_FIELD(raw, 31, 30), &instruction);
        if (mapping_status != ARM64_DECODE_OK) return mapping_status;
        candidate->instruction = instruction;
        *decoded = result;
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

    arm64_decode_ldst_rt_rn(raw, candidate);
    candidate->rs = ARM64_DECODE_FIELD(raw, 20, 16);
    candidate->operand_width = arm64_decode_ldst_atomic_width(access_bytes);
    enum arm64_instruction instruction;
    enum arm64_decode_status mapping_status = arm64_decode_lse_atomic_instruction(raw, &instruction);
    if (mapping_status != ARM64_DECODE_OK) return mapping_status;
    candidate->instruction = instruction;
    *decoded = result;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_ldst_single(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd)
{
    if (!(ARM64_DECODE_MATCH(raw, 0x3A000000U, 0x38000000U) && ARM64_DECODE_BIT(raw, 26) == (uint32_t)is_fp_simd)) return ARM64_DECODE_UNSUPPORTED;
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    uint32_t size = arm64_decode_ldst_size(raw);
    uint32_t opc = arm64_decode_ldst_opc(raw);
    uint32_t access_bytes = arm64_decode_ldst_access_bytes(raw);
    int prefetch = arm64_decode_ldst_is_prefetch(raw, is_fp_simd);

    if (prefetch)
    {
        candidate->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        candidate->immediate = ARM64_DECODE_FIELD(raw, 4, 0);
        candidate->operand_width = 0;
    }
    else
    {
        arm64_decode_ldst_rt_rn(raw, candidate);
        if (is_fp_simd && opc >= 2 && size != 0) return ARM64_DECODE_UNALLOCATED;
        if (is_fp_simd) access_bytes = arm64_decode_ldst_simd_access_bytes(raw);
        candidate->operand_width = arm64_decode_ldst_operand_width(is_fp_simd, access_bytes, size, opc);
    }

    if (!is_fp_simd && opc == 3 && size >= 2) return ARM64_DECODE_UNALLOCATED;
    *decoded = result;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_ldst_literal(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd)
{
    if (!(ARM64_DECODE_MATCH(raw, 0x3B000000U, 0x18000000U) && ARM64_DECODE_BIT(raw, 26) == (uint32_t)is_fp_simd)) return ARM64_DECODE_UNSUPPORTED;
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    uint32_t size = arm64_decode_ldst_size(raw);
    int prefetch = !is_fp_simd && size == 3;
    uint32_t access_bytes = arm64_decode_ldst_literal_access_bytes(size, is_fp_simd);

    if (!prefetch) candidate->rt = ARM64_DECODE_FIELD(raw, 4, 0);
    else candidate->immediate = ARM64_DECODE_FIELD(raw, 4, 0);
    candidate->offset = ARM64_DECODE_SIGNED_FIELD(raw, 23, 5, 2);
    if (prefetch) candidate->operand_width = 0;
    else if (is_fp_simd) candidate->operand_width = access_bytes * 8;
    else candidate->operand_width = arm64_decode_ldst_literal_gpr_width(size);
    if (is_fp_simd && !access_bytes) return ARM64_DECODE_UNALLOCATED;
    enum arm64_instruction instruction;
    enum arm64_decode_status mapping_status = arm64_decode_ldst_literal_instruction(size, is_fp_simd, prefetch, &instruction);
    if (mapping_status != ARM64_DECODE_OK) return mapping_status;
    candidate->instruction = instruction;
    *decoded = result;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_ldst_pair(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd, uint8_t low_mode, uint8_t high_mode)
{
    if (!(ARM64_DECODE_MATCH(raw, 0x3A000000U, 0x28000000U) && ARM64_DECODE_BIT(raw, 26) == (uint32_t)is_fp_simd)) return ARM64_DECODE_UNSUPPORTED;
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    uint32_t opc = arm64_decode_ldst_size(raw);
    uint32_t load = ARM64_DECODE_BIT(raw, 22);
    uint8_t address_mode = low_mode;
    uint32_t access_bytes;

    if (ARM64_DECODE_BIT(raw, 23)) address_mode = high_mode;

    arm64_decode_ldst_rt_rn(raw, candidate);
    candidate->rt2 = ARM64_DECODE_FIELD(raw, 14, 10);
    if (is_fp_simd)
    {
        if (opc == 3) return ARM64_DECODE_UNALLOCATED;
        access_bytes = arm64_decode_ldst_pair_access_bytes(opc, 1);
        candidate->operand_width = access_bytes * 8;
    }
    else
    {
        if (opc == 3 || (opc == 1 && address_mode == 2)) return ARM64_DECODE_UNALLOCATED;
        if (opc == 1 && !load) return ARM64_DECODE_UNSUPPORTED;
        access_bytes = arm64_decode_ldst_pair_access_bytes(opc, 0);
        candidate->operand_width = arm64_decode_ldst_pair_gpr_width(opc);
    }
    if (!is_fp_simd && (address_mode == 3 || address_mode == 4) && candidate->rn != 31 && (candidate->rn == candidate->rt || candidate->rn == candidate->rt2)) return ARM64_DECODE_UNPREDICTABLE;
    if (load && candidate->rt == candidate->rt2) return ARM64_DECODE_UNPREDICTABLE;
    candidate->offset = ARM64_DECODE_SIGN_EXTEND(ARM64_DECODE_FIELD(raw, 21, 15), 7) * access_bytes;
    enum arm64_instruction instruction;
    enum arm64_decode_status mapping_status = arm64_decode_ldst_pair_insn(is_fp_simd, address_mode, load, opc, &instruction);
    if (mapping_status != ARM64_DECODE_OK) return mapping_status;
    candidate->instruction = instruction;
    *decoded = result;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_ldst_unsigned(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd)
{
    if (!(ARM64_DECODE_MATCH(raw, 0x3B000000U, 0x39000000U) && ARM64_DECODE_BIT(raw, 26) == (uint32_t)is_fp_simd)) return ARM64_DECODE_UNSUPPORTED;
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    enum arm64_decode_status status = arm64_decode_ldst_single(raw, candidate, is_fp_simd);
    int prefetch;
    uint32_t access_bytes = is_fp_simd ? arm64_decode_ldst_simd_access_bytes(raw) : arm64_decode_ldst_access_bytes(raw);

    if (status != ARM64_DECODE_OK) return status;
    prefetch = candidate->operand_width == 0;
    candidate->offset = ARM64_DECODE_FIELD(raw, 21, 10) * arm64_decode_ldst_offset_scale(prefetch, access_bytes);
    enum arm64_instruction instruction;
    enum arm64_decode_status mapping_status = arm64_decode_ldst_single_instruction(raw, is_fp_simd, 7, prefetch, &instruction);
    if (mapping_status != ARM64_DECODE_OK) return mapping_status;
    candidate->instruction = instruction;
    *decoded = result;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_ldst_unscaled(uint32_t raw, struct arm64_decoded_instruction *decoded, int is_fp_simd)
{
    if (!(ARM64_DECODE_MATCH(raw, 0x3B000000U, 0x38000000U) && ARM64_DECODE_BIT(raw, 26) == (uint32_t)is_fp_simd)) return ARM64_DECODE_UNSUPPORTED;
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    uint8_t address_mode;
    uint32_t mode;
    int prefetch;

    if (!(raw & 0x00200000U))
    {
        enum arm64_decode_status status = arm64_decode_ldst_single(raw, candidate, is_fp_simd);

        if (status != ARM64_DECODE_OK) return status;
        mode = ARM64_DECODE_FIELD(raw, 11, 10);
        prefetch = candidate->operand_width == 0;
        if (is_fp_simd && mode == 2) return ARM64_DECODE_UNALLOCATED;
        if (prefetch && mode != 0) return ARM64_DECODE_UNALLOCATED;
        address_mode = arm64_decode_ldst_immediate_address_mode(mode);
        candidate->offset = ARM64_DECODE_SIGN_EXTEND(ARM64_DECODE_FIELD(raw, 20, 12), 9);
        enum arm64_instruction instruction;
        if (arm64_decode_ldst_single_instruction(raw, is_fp_simd, address_mode, prefetch, &instruction) != ARM64_DECODE_OK)
        {
            return ARM64_DECODE_UNSUPPORTED;
        }
        candidate->instruction = instruction;
    }
    else
    {
        mode = ARM64_DECODE_FIELD(raw, 11, 10);
        if (mode == 0)
        {
            if (is_fp_simd) return ARM64_DECODE_UNALLOCATED;
            {
                enum arm64_decode_status status = arm64_decode_ldst_atomic(raw, candidate);
                if (status == ARM64_DECODE_OK) *decoded = result;
                return status;
            }
        }
        if (mode != 2)
        {
            if (!is_fp_simd && arm64_decode_ldst_size(raw) == 3) return ARM64_DECODE_UNSUPPORTED;
            return ARM64_DECODE_UNALLOCATED;
        }

        enum arm64_decode_status status = arm64_decode_ldst_single(raw, candidate, is_fp_simd);
        uint32_t access_bytes = is_fp_simd ? arm64_decode_ldst_simd_access_bytes(raw) : arm64_decode_ldst_access_bytes(raw);

        if (status != ARM64_DECODE_OK) return status;
        prefetch = candidate->operand_width == 0;
        if (prefetch && (raw & 0x18U) == 0x18U) return ARM64_DECODE_UNSUPPORTED;
        address_mode = 6;
        candidate->rm = ARM64_DECODE_FIELD(raw, 20, 16);
        candidate->extend_type = ARM64_DECODE_FIELD(raw, 15, 13);
        if (candidate->extend_type != 2 && candidate->extend_type != 3 && candidate->extend_type != 6 && candidate->extend_type != 7) return ARM64_DECODE_UNALLOCATED;
        candidate->shift_amount = 0;
        if (raw & 0x1000U)
        {
            candidate->shift_amount = prefetch ? 3 : (uint8_t)__builtin_ctz(access_bytes);
        }
        enum arm64_instruction instruction;
        if (arm64_decode_ldst_single_instruction(raw, is_fp_simd, address_mode, prefetch, &instruction) != ARM64_DECODE_OK)
        {
            return ARM64_DECODE_UNSUPPORTED;
        }
        candidate->instruction = instruction;
    }

    if (!is_fp_simd && (address_mode == 4 || address_mode == 3) && candidate->rn != 31 && candidate->rn == candidate->rt) return ARM64_DECODE_UNPREDICTABLE;
    *decoded = result;
    return ARM64_DECODE_OK;
}

/*
解码访存、原子和独占指令。bits[29:24] 先确定唯一编码 owner，叶子只校验
本族固定字段和寄存器约束，不依赖宽窄掩码的排列顺序。
*/
static inline enum arm64_decode_status arm64_decode_ldst_casp(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    if (!(ARM64_DECODE_MATCH(raw, 0x3FA07C00U, 0x08207C00U))) return ARM64_DECODE_UNSUPPORTED;
    uint32_t size = ARM64_DECODE_FIELD(raw, 31, 30);
    uint32_t rs = ARM64_DECODE_FIELD(raw, 20, 16);
    uint32_t rt = ARM64_DECODE_FIELD(raw, 4, 0);
    enum arm64_instruction instruction;

    if (size >= 2 || ARM64_DECODE_FIELD(raw, 14, 10) != 31 || ((rs | rt) & 1)) return ARM64_DECODE_UNALLOCATED;
    switch (ARM64_DECODE_BIT_PAIR(raw, 22, 15))
    {
    case 0:
        instruction = ARM64_INST_CASP;
        break;
    case 1:
        instruction = ARM64_INST_CASPL;
        break;
    case 2:
        instruction = ARM64_INST_CASPA;
        break;
    case 3:
        instruction = ARM64_INST_CASPAL;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    arm64_decode_ldst_rt_rn(raw, candidate);
    candidate->rs = ARM64_DECODE_FIELD(raw, 20, 16);
    candidate->operand_width = arm64_decode_ldst_casp_width(size);
    enum arm64_decode_status status = arm64_decode_ldst_set_instruction(instruction, candidate);
    if (status == ARM64_DECODE_OK) *decoded = result;
    return status;
}

static inline enum arm64_decode_status arm64_decode_ldst_cas(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    if (!(ARM64_DECODE_MATCH(raw, 0x3FA07C00U, 0x08A07C00U))) return ARM64_DECODE_UNSUPPORTED;
    uint32_t size = ARM64_DECODE_FIELD(raw, 31, 30);
    enum arm64_instruction instruction;
    enum arm64_decode_status mapping_status = arm64_decode_ldst_cas_instruction(size, ARM64_DECODE_BIT_PAIR(raw, 22, 15), &instruction);
    if (mapping_status != ARM64_DECODE_OK) return mapping_status;

    arm64_decode_ldst_rt_rn(raw, candidate);
    candidate->rs = ARM64_DECODE_FIELD(raw, 20, 16);
    candidate->operand_width = arm64_decode_ldst_atomic_width(ARM64_DECODE_SCALE(1U, size));
    enum arm64_decode_status status = arm64_decode_ldst_set_instruction(instruction, candidate);
    if (status == ARM64_DECODE_OK) *decoded = result;
    return status;
}

static inline enum arm64_decode_status arm64_decode_ldst_exclusive(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    if (!(ARM64_DECODE_MATCH(raw, 0x3F000000U, 0x08000000U))) return ARM64_DECODE_UNSUPPORTED;
    uint32_t size = ARM64_DECODE_FIELD(raw, 31, 30);
    uint32_t ordered = ARM64_DECODE_BIT(raw, 23);
    uint32_t load = ARM64_DECODE_BIT(raw, 22);
    uint32_t pair = ARM64_DECODE_BIT(raw, 21);
    uint32_t rs = ARM64_DECODE_FIELD(raw, 20, 16);
    uint32_t rt2 = ARM64_DECODE_FIELD(raw, 14, 10);
    enum arm64_instruction instruction;

    if (ordered)
    {
        if (pair || rs != 31 || rt2 != 31) return ARM64_DECODE_UNALLOCATED;
    }
    else if ((pair && size < 2) || (!pair && rt2 != 31) || (load && rs != 31))
    {
        return ARM64_DECODE_UNALLOCATED;
    }

    switch (ARM64_DECODE_BIT_QUAD(raw, 23, 22, 21, 15))
    {
    case 0x0:
        if (arm64_decode_ldst_exclusive_instruction(size, 0, &instruction) != ARM64_DECODE_OK) return ARM64_DECODE_UNSUPPORTED;
        break;
    case 0x1:
        if (arm64_decode_ldst_exclusive_instruction(size, 1, &instruction) != ARM64_DECODE_OK) return ARM64_DECODE_UNSUPPORTED;
        break;
    case 0x2:
        instruction = ARM64_INST_STXP;
        break;
    case 0x3:
        instruction = ARM64_INST_STLXP;
        break;
    case 0x4:
        if (arm64_decode_ldst_exclusive_instruction(size, 4, &instruction) != ARM64_DECODE_OK) return ARM64_DECODE_UNSUPPORTED;
        break;
    case 0x5:
        if (arm64_decode_ldst_exclusive_instruction(size, 5, &instruction) != ARM64_DECODE_OK) return ARM64_DECODE_UNSUPPORTED;
        break;
    case 0x6:
        instruction = ARM64_INST_LDXP;
        break;
    case 0x7:
        instruction = ARM64_INST_LDAXP;
        break;
    case 0x8:
        if (arm64_decode_ldst_exclusive_instruction(size, 8, &instruction) != ARM64_DECODE_OK) return ARM64_DECODE_UNSUPPORTED;
        break;
    case 0x9:
        if (arm64_decode_ldst_exclusive_instruction(size, 9, &instruction) != ARM64_DECODE_OK) return ARM64_DECODE_UNSUPPORTED;
        break;
    case 0xC:
        if (arm64_decode_ldst_exclusive_instruction(size, 12, &instruction) != ARM64_DECODE_OK) return ARM64_DECODE_UNSUPPORTED;
        break;
    case 0xD:
        if (arm64_decode_ldst_exclusive_instruction(size, 13, &instruction) != ARM64_DECODE_OK) return ARM64_DECODE_UNSUPPORTED;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }
    arm64_decode_ldst_rt_rn(raw, candidate);
    if (pair) candidate->rt2 = rt2;
    if (!ordered && !load) candidate->rs = rs;
    candidate->operand_width = arm64_decode_ldst_atomic_width(ARM64_DECODE_SCALE(1U, size));
    if (pair && load && candidate->rt == candidate->rt2) return ARM64_DECODE_UNPREDICTABLE;
    if (!load && rs != 31 && (rs == candidate->rt || (pair && rs == candidate->rt2) || (candidate->rn != 31 && rs == candidate->rn))) return ARM64_DECODE_UNPREDICTABLE;
    enum arm64_decode_status status = arm64_decode_ldst_set_instruction(instruction, candidate);
    if (status == ARM64_DECODE_OK) *decoded = result;
    return status;
}

static inline enum arm64_decode_status arm64_decode_ldst_structure_lane(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    if (!(ARM64_DECODE_MATCH(raw, 0x3F000000U, 0x0D000000U))) return ARM64_DECODE_UNSUPPORTED;
    uint32_t opcode = ARM64_DECODE_FIELD(raw, 15, 13);
    uint32_t size = ARM64_DECODE_FIELD(raw, 11, 10);
    uint32_t q = ARM64_DECODE_BIT(raw, 30);
    uint32_t s = ARM64_DECODE_BIT(raw, 12);

    if ((raw & 0x00800000U) || (raw & 0x001F0000U)) return ARM64_DECODE_UNSUPPORTED;
    if (opcode != 0 && opcode != 2 && opcode != 4) return ARM64_DECODE_UNSUPPORTED;
    if ((opcode == 2 && (size & 1)) || (opcode == 4 && size > 1)) return ARM64_DECODE_UNALLOCATED;
    enum arm64_instruction instruction = ARM64_DECODE_BIT(raw, 22) ? ARM64_INST_LD1 : ARM64_INST_ST1;

    arm64_decode_ldst_rt_rn(raw, candidate);
    candidate->element_width = arm64_decode_ldst_ld1_element_width(opcode, size);
    candidate->lane_index = arm64_decode_ldst_ld1_lane(opcode, size, q, s);
    candidate->operand_width = q ? 128 : 64;
    enum arm64_decode_status status = arm64_decode_ldst_set_instruction(instruction, candidate);
    if (status == ARM64_DECODE_OK) *decoded = result;
    return status;
}

static inline enum arm64_decode_status arm64_decode_ldst_ordered_unscaled(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    if (!ARM64_DECODE_MATCH(raw, 0x3F000000U, 0x19000000U)) return ARM64_DECODE_UNSUPPORTED;
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
        uint32_t size = ARM64_DECODE_FIELD(raw, 31, 30);
        uint32_t opc = ARM64_DECODE_FIELD(raw, 23, 22);

        if ((size == 3 && opc > 1) || (size == 2 && opc == 3)) return ARM64_DECODE_UNALLOCATED;
        arm64_decode_ldst_rt_rn(raw, candidate);
        candidate->offset = ARM64_DECODE_SIGN_EXTEND(ARM64_DECODE_FIELD(raw, 20, 12), 9);
        candidate->operand_width = arm64_decode_ldst_ldapur_width(size, opc);
        enum arm64_instruction instruction;
        enum arm64_decode_status mapping_status = arm64_decode_ldst_ldapur_instruction(size, opc, &instruction);
        if (mapping_status != ARM64_DECODE_OK) return mapping_status;
        candidate->instruction = instruction;
        *decoded = result;
        return ARM64_DECODE_OK;
    }
    return ARM64_DECODE_UNSUPPORTED;
}

enum arm64_decode_status arm64_decode_load_store(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;

    candidate->instruction_class = ARM64_INSTRUCTION_CLASS_LOAD_STORE;
    switch (ARM64_DECODE_FIELD(raw, 29, 24))
    {
    case 0x08:
        switch (raw & 0x00A07C00U)
        {
        case 0x00207C00U:
        {
            enum arm64_decode_status status = arm64_decode_ldst_casp(raw, candidate);
            if (status == ARM64_DECODE_OK) *decoded = result;
            return status;
        }
        case 0x00A07C00U:
        {
            enum arm64_decode_status status = arm64_decode_ldst_cas(raw, candidate);
            if (status == ARM64_DECODE_OK) *decoded = result;
            return status;
        }
        default:
            break;
        }

        {
            enum arm64_decode_status status = arm64_decode_ldst_exclusive(raw, candidate);
            if (status == ARM64_DECODE_OK) *decoded = result;
            return status;
        }

    case 0x09:
    case 0x0C:
        return ARM64_DECODE_UNSUPPORTED;
    case 0x0D:
    {
        enum arm64_decode_status status = arm64_decode_ldst_structure_lane(raw, candidate);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }

    case 0x18:
    {
        enum arm64_decode_status status = arm64_decode_ldst_literal(raw, candidate, 0);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }
    case 0x19:
    {
        enum arm64_decode_status status = arm64_decode_ldst_ordered_unscaled(raw, candidate);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }
    case 0x1C:
    {
        enum arm64_decode_status status = arm64_decode_ldst_literal(raw, candidate, 1);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }
    case 0x1D:
        return ARM64_DECODE_UNSUPPORTED;

    case 0x28:
    {
        enum arm64_decode_status status = arm64_decode_ldst_pair(raw, candidate, 0, 2, 3);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }
    case 0x29:
    {
        enum arm64_decode_status status = arm64_decode_ldst_pair(raw, candidate, 0, 0, 4);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }
    case 0x2C:
    {
        enum arm64_decode_status status = arm64_decode_ldst_pair(raw, candidate, 1, 2, 3);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }
    case 0x2D:
    {
        enum arm64_decode_status status = arm64_decode_ldst_pair(raw, candidate, 1, 0, 4);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }

    case 0x38:
    {
        enum arm64_decode_status status = arm64_decode_ldst_unscaled(raw, candidate, 0);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }
    case 0x39:
    {
        enum arm64_decode_status status = arm64_decode_ldst_unsigned(raw, candidate, 0);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }
    case 0x3C:
    {
        enum arm64_decode_status status = arm64_decode_ldst_unscaled(raw, candidate, 1);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }
    case 0x3D:
    {
        enum arm64_decode_status status = arm64_decode_ldst_unsigned(raw, candidate, 1);
        if (status == ARM64_DECODE_OK) *decoded = result;
        return status;
    }

    default:
        return ARM64_DECODE_UNALLOCATED;
    }
}
