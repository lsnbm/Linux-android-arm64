#include "arm64_decode.h"

static int arm64_normalize_simd_registers(struct arm64_decoded_insn *decoded)
{
    int use_rd = 1;
    int use_rn = 0;
    int use_rm = 0;
    int use_ra = 0;

    switch (decoded->instruction)
    {
    case ARM64_INSN_MOVI_VECTOR_IMMEDIATE:
    case ARM64_INSN_ORR_VECTOR_IMMEDIATE:
    case ARM64_INSN_FMOV_VECTOR_IMMEDIATE:
    case ARM64_INSN_FMOV_SCALAR_IMMEDIATE:
    case ARM64_INSN_MVNI_VECTOR_IMMEDIATE:
    case ARM64_INSN_BIC_VECTOR_IMMEDIATE:
        break;
    case ARM64_INSN_DUP_ELEMENT_VECTOR:
    case ARM64_INSN_DUP_GENERAL_VECTOR:
    case ARM64_INSN_SMOV_VECTOR_TO_GPR:
    case ARM64_INSN_UMOV_VECTOR_TO_GPR:
    case ARM64_INSN_REV64_VECTOR:
    case ARM64_INSN_REV16_VECTOR:
    case ARM64_INSN_XTN_VECTOR:
    case ARM64_INSN_SQXTN_VECTOR:
    case ARM64_INSN_FCVTNS_SIMD_VECTOR:
    case ARM64_INSN_FCVTMS_SIMD_VECTOR:
    case ARM64_INSN_FCVTAS_SIMD_VECTOR:
    case ARM64_INSN_SCVTF_SIMD_VECTOR:
    case ARM64_INSN_SADDLV_VECTOR:
    case ARM64_INSN_SMAXV_VECTOR:
    case ARM64_INSN_FMAXNMV_SCALAR_REDUCE:
    case ARM64_INSN_FMAXV_SCALAR_REDUCE:
    case ARM64_INSN_SMINV_VECTOR:
    case ARM64_INSN_ADDV_VECTOR:
    case ARM64_INSN_FCMGT_ZERO_VECTOR:
    case ARM64_INSN_FCMEQ_ZERO_VECTOR:
    case ARM64_INSN_FCMLT_ZERO_VECTOR:
    case ARM64_INSN_FABS_VECTOR:
    case ARM64_INSN_FCVTPS_SIMD_VECTOR:
    case ARM64_INSN_FCVTZS_SIMD_VECTOR:
    case ARM64_INSN_FMINNMV_SCALAR_REDUCE:
    case ARM64_INSN_FMINV_SCALAR_REDUCE:
    case ARM64_INSN_SSHR_VECTOR_IMMEDIATE:
    case ARM64_INSN_SHL_VECTOR_IMMEDIATE:
    case ARM64_INSN_FCVTNS_GPR:
    case ARM64_INSN_FMOV_SCALAR:
    case ARM64_INSN_FABS_SCALAR:
    case ARM64_INSN_FCVTNU_GPR:
    case ARM64_INSN_FNEG_SCALAR:
    case ARM64_INSN_FSQRT_SCALAR:
    case ARM64_INSN_SCVTF_S_W:
    case ARM64_INSN_FCVT_D_S:
    case ARM64_INSN_UCVTF_S_W:
    case ARM64_INSN_FCVTAS_GPR:
    case ARM64_INSN_FRINTN_SCALAR:
    case ARM64_INSN_FRINTP_SCALAR:
    case ARM64_INSN_FCVTAU_GPR:
    case ARM64_INSN_FRINTM_SCALAR:
    case ARM64_INSN_FRINTZ_SCALAR:
    case ARM64_INSN_FMOV_FP_TO_GPR:
    case ARM64_INSN_FRINTA_SCALAR:
    case ARM64_INSN_FMOV_GPR_TO_FP:
    case ARM64_INSN_FRINTX_SCALAR:
    case ARM64_INSN_FRINTI_SCALAR:
    case ARM64_INSN_FCVTPS_GPR:
    case ARM64_INSN_FCVTPU_GPR:
    case ARM64_INSN_FCVTMS_GPR:
    case ARM64_INSN_FCVTMU_GPR:
    case ARM64_INSN_FCVTZS_GPR:
    case ARM64_INSN_FCVTZU_GPR:
    case ARM64_INSN_SCVTF_D_W:
    case ARM64_INSN_FCVT_S_D:
    case ARM64_INSN_UCVTF_D_W:
    case ARM64_INSN_REV32_VECTOR:
    case ARM64_INSN_SQXTUN_VECTOR:
    case ARM64_INSN_UQXTN_VECTOR:
    case ARM64_INSN_FCVTNU_SIMD_VECTOR:
    case ARM64_INSN_FCVTMU_SIMD_VECTOR:
    case ARM64_INSN_FCVTAU_SIMD_VECTOR:
    case ARM64_INSN_UCVTF_SIMD_VECTOR:
    case ARM64_INSN_UADDLV_VECTOR:
    case ARM64_INSN_UMAXV_VECTOR:
    case ARM64_INSN_UMINV_VECTOR:
    case ARM64_INSN_FCMGE_ZERO_VECTOR:
    case ARM64_INSN_FCMLE_ZERO_VECTOR:
    case ARM64_INSN_FNEG_VECTOR:
    case ARM64_INSN_FCVTPU_SIMD_VECTOR:
    case ARM64_INSN_FCVTZU_SIMD_VECTOR:
    case ARM64_INSN_FSQRT_VECTOR:
    case ARM64_INSN_USHR_VECTOR_IMMEDIATE:
    case ARM64_INSN_INS_GPR_VECTOR:
    case ARM64_INSN_XTN2_VECTOR:
    case ARM64_INSN_SQXTN2_VECTOR:
    case ARM64_INSN_DUP_ELEMENT_SCALAR:
    case ARM64_INSN_SQXTN_SCALAR:
    case ARM64_INSN_FCVTNS_SIMD_SCALAR:
    case ARM64_INSN_FCVTMS_SIMD_SCALAR:
    case ARM64_INSN_FCVTAS_SIMD_SCALAR:
    case ARM64_INSN_SCVTF_SIMD_SCALAR:
    case ARM64_INSN_FADDP_SCALAR_REDUCE:
    case ARM64_INSN_FCMGT_ZERO_SCALAR:
    case ARM64_INSN_FCMEQ_ZERO_SCALAR:
    case ARM64_INSN_FCMLT_ZERO_SCALAR:
    case ARM64_INSN_FCVTPS_SIMD_SCALAR:
    case ARM64_INSN_FCVTZS_SIMD_SCALAR:
    case ARM64_INSN_INS_ELEMENT_VECTOR:
    case ARM64_INSN_SQXTUN2_VECTOR:
    case ARM64_INSN_UQXTN2_VECTOR:
    case ARM64_INSN_SQXTUN_SCALAR:
    case ARM64_INSN_UQXTN_SCALAR:
    case ARM64_INSN_FCVTNU_SIMD_SCALAR:
    case ARM64_INSN_FCVTMU_SIMD_SCALAR:
    case ARM64_INSN_FCVTAU_SIMD_SCALAR:
    case ARM64_INSN_UCVTF_SIMD_SCALAR:
    case ARM64_INSN_FCMGE_ZERO_SCALAR:
    case ARM64_INSN_FCMLE_ZERO_SCALAR:
    case ARM64_INSN_FCVTPU_SIMD_SCALAR:
    case ARM64_INSN_FCVTZU_SIMD_SCALAR:
    case ARM64_INSN_SCVTF_S_X:
    case ARM64_INSN_UCVTF_S_X:
    case ARM64_INSN_SCVTF_D_X:
    case ARM64_INSN_UCVTF_D_X:
        use_rn = 1;
        break;
    case ARM64_INSN_UZP1_VECTOR:
    case ARM64_INSN_TRN1_VECTOR:
    case ARM64_INSN_ZIP1_VECTOR:
    case ARM64_INSN_UZP2_VECTOR:
    case ARM64_INSN_TRN2_VECTOR:
    case ARM64_INSN_ZIP2_VECTOR:
    case ARM64_INSN_SHADD_VECTOR:
    case ARM64_INSN_SQADD_VECTOR:
    case ARM64_INSN_SRHADD_VECTOR:
    case ARM64_INSN_AND_VECTOR:
    case ARM64_INSN_SHSUB_VECTOR:
    case ARM64_INSN_SQSUB_VECTOR:
    case ARM64_INSN_CMGT_VECTOR:
    case ARM64_INSN_CMGE_VECTOR:
    case ARM64_INSN_SSHL_VECTOR:
    case ARM64_INSN_SQSHL_VECTOR:
    case ARM64_INSN_SRSHL_VECTOR:
    case ARM64_INSN_SQRSHL_VECTOR:
    case ARM64_INSN_SMAX_VECTOR:
    case ARM64_INSN_SMIN_VECTOR:
    case ARM64_INSN_SABD_VECTOR:
    case ARM64_INSN_SABA_VECTOR:
    case ARM64_INSN_ADD_VECTOR:
    case ARM64_INSN_CMTST_VECTOR:
    case ARM64_INSN_MLA_VECTOR:
    case ARM64_INSN_MUL_VECTOR:
    case ARM64_INSN_SMAXP_VECTOR:
    case ARM64_INSN_SMINP_VECTOR:
    case ARM64_INSN_ADDP_VECTOR:
    case ARM64_INSN_FMAXNM_VECTOR:
    case ARM64_INSN_FMLA_VECTOR:
    case ARM64_INSN_FADD_VECTOR:
    case ARM64_INSN_FMULX_VECTOR:
    case ARM64_INSN_FCMEQ_VECTOR:
    case ARM64_INSN_FMLAL_VECTOR:
    case ARM64_INSN_FMAX_VECTOR:
    case ARM64_INSN_FRECPS_VECTOR:
    case ARM64_INSN_BIC_VECTOR:
    case ARM64_INSN_SQDMULH_VECTOR:
    case ARM64_INSN_SDOT_VECTOR:
    case ARM64_INSN_USDOT_VECTOR:
    case ARM64_INSN_ORR_VECTOR:
    case ARM64_INSN_FMINNM_VECTOR:
    case ARM64_INSN_FMLS_VECTOR:
    case ARM64_INSN_FSUB_VECTOR:
    case ARM64_INSN_FAMAX_VECTOR:
    case ARM64_INSN_FMLSL_VECTOR:
    case ARM64_INSN_FMIN_VECTOR:
    case ARM64_INSN_FRSQRTS_VECTOR:
    case ARM64_INSN_ORN_VECTOR:
    case ARM64_INSN_FMLA_VECTOR_BY_ELEMENT:
    case ARM64_INSN_FMLS_VECTOR_BY_ELEMENT:
    case ARM64_INSN_FMUL_VECTOR_BY_ELEMENT:
    case ARM64_INSN_SUDOT_VECTOR_BY_ELEMENT:
    case ARM64_INSN_BFDOT_VECTOR_BY_ELEMENT:
    case ARM64_INSN_FMLAL_VECTOR_BY_ELEMENT:
    case ARM64_INSN_FMLSL_VECTOR_BY_ELEMENT:
    case ARM64_INSN_SDOT_VECTOR_BY_ELEMENT:
    case ARM64_INSN_USDOT_VECTOR_BY_ELEMENT:
    case ARM64_INSN_BFMLALB_VECTOR_BY_ELEMENT:
    case ARM64_INSN_FMUL_SCALAR:
    case ARM64_INSN_FCSEL_SCALAR:
    case ARM64_INSN_FDIV_SCALAR:
    case ARM64_INSN_FADD_SCALAR:
    case ARM64_INSN_FSUB_SCALAR:
    case ARM64_INSN_FMAX_SCALAR:
    case ARM64_INSN_FMIN_SCALAR:
    case ARM64_INSN_FMAXNM_SCALAR:
    case ARM64_INSN_FMINNM_SCALAR:
    case ARM64_INSN_FNMUL_SCALAR:
    case ARM64_INSN_EXT_VECTOR:
    case ARM64_INSN_UHADD_VECTOR:
    case ARM64_INSN_UQADD_VECTOR:
    case ARM64_INSN_URHADD_VECTOR:
    case ARM64_INSN_EOR_VECTOR:
    case ARM64_INSN_UHSUB_VECTOR:
    case ARM64_INSN_UQSUB_VECTOR:
    case ARM64_INSN_CMHI_VECTOR:
    case ARM64_INSN_CMHS_VECTOR:
    case ARM64_INSN_USHL_VECTOR:
    case ARM64_INSN_UQSHL_VECTOR:
    case ARM64_INSN_URSHL_VECTOR:
    case ARM64_INSN_UQRSHL_VECTOR:
    case ARM64_INSN_UMAX_VECTOR:
    case ARM64_INSN_UMIN_VECTOR:
    case ARM64_INSN_UABD_VECTOR:
    case ARM64_INSN_UABA_VECTOR:
    case ARM64_INSN_SUB_VECTOR:
    case ARM64_INSN_CMEQ_VECTOR:
    case ARM64_INSN_MLS_VECTOR:
    case ARM64_INSN_PMUL_VECTOR:
    case ARM64_INSN_UMAXP_VECTOR:
    case ARM64_INSN_UMINP_VECTOR:
    case ARM64_INSN_FMAXNMP_VECTOR:
    case ARM64_INSN_FMLAL2_VECTOR:
    case ARM64_INSN_FADDP_VECTOR:
    case ARM64_INSN_FMUL_VECTOR:
    case ARM64_INSN_FCMGE_VECTOR:
    case ARM64_INSN_FACGE_VECTOR:
    case ARM64_INSN_FMAXP_VECTOR:
    case ARM64_INSN_FDIV_VECTOR:
    case ARM64_INSN_SQRDMLAH_VECTOR:
    case ARM64_INSN_SQRDMLSH_VECTOR:
    case ARM64_INSN_FCMLA_VECTOR:
    case ARM64_INSN_FCADD_VECTOR:
    case ARM64_INSN_BFDOT_VECTOR:
    case ARM64_INSN_BSL_VECTOR:
    case ARM64_INSN_SQRDMULH_VECTOR:
    case ARM64_INSN_UDOT_VECTOR:
    case ARM64_INSN_BIT_VECTOR:
    case ARM64_INSN_FMINNMP_VECTOR:
    case ARM64_INSN_FMLSL2_VECTOR:
    case ARM64_INSN_FABD_VECTOR:
    case ARM64_INSN_FAMIN_VECTOR:
    case ARM64_INSN_FCMGT_VECTOR:
    case ARM64_INSN_FACGT_VECTOR:
    case ARM64_INSN_FMINP_VECTOR:
    case ARM64_INSN_FSCALE_VECTOR:
    case ARM64_INSN_BFMLALB_VECTOR:
    case ARM64_INSN_BIF_VECTOR:
    case ARM64_INSN_FMULX_VECTOR_BY_ELEMENT:
    case ARM64_INSN_FCMLA_VECTOR_BY_ELEMENT:
    case ARM64_INSN_SQRDMLAH_VECTOR_BY_ELEMENT:
    case ARM64_INSN_SQRDMLSH_VECTOR_BY_ELEMENT:
    case ARM64_INSN_FMLAL2_VECTOR_BY_ELEMENT:
    case ARM64_INSN_FMLSL2_VECTOR_BY_ELEMENT:
    case ARM64_INSN_UDOT_VECTOR_BY_ELEMENT:
    case ARM64_INSN_SMMLA_VECTOR:
    case ARM64_INSN_USMMLA_VECTOR:
    case ARM64_INSN_BFMLALT_VECTOR_BY_ELEMENT:
    case ARM64_INSN_SQADD_SCALAR:
    case ARM64_INSN_SQSUB_SCALAR:
    case ARM64_INSN_SQSHL_SCALAR:
    case ARM64_INSN_SQRSHL_SCALAR:
    case ARM64_INSN_FMULX_SCALAR:
    case ARM64_INSN_FCMEQ_SCALAR:
    case ARM64_INSN_FRECPS_SCALAR:
    case ARM64_INSN_SQDMULH_SCALAR:
    case ARM64_INSN_FRSQRTS_SCALAR:
    case ARM64_INSN_CMGT_SCALAR:
    case ARM64_INSN_CMGE_SCALAR:
    case ARM64_INSN_SSHL_SCALAR:
    case ARM64_INSN_SRSHL_SCALAR:
    case ARM64_INSN_ADD_SCALAR:
    case ARM64_INSN_CMTST_SCALAR:
    case ARM64_INSN_FMLA_SCALAR_BY_ELEMENT:
    case ARM64_INSN_FMLS_SCALAR_BY_ELEMENT:
    case ARM64_INSN_FMUL_SCALAR_BY_ELEMENT:
    case ARM64_INSN_BFMMLA_VECTOR:
    case ARM64_INSN_UMMLA_VECTOR:
    case ARM64_INSN_BFMLALT_VECTOR:
    case ARM64_INSN_UQADD_SCALAR:
    case ARM64_INSN_UQSUB_SCALAR:
    case ARM64_INSN_UQSHL_SCALAR:
    case ARM64_INSN_UQRSHL_SCALAR:
    case ARM64_INSN_FCMGE_SCALAR:
    case ARM64_INSN_FACGE_SCALAR:
    case ARM64_INSN_SQRDMLAH_SCALAR:
    case ARM64_INSN_SQRDMLSH_SCALAR:
    case ARM64_INSN_SQRDMULH_SCALAR:
    case ARM64_INSN_FABD_SCALAR:
    case ARM64_INSN_FCMGT_SCALAR:
    case ARM64_INSN_FACGT_SCALAR:
    case ARM64_INSN_CMHI_SCALAR:
    case ARM64_INSN_CMHS_SCALAR:
    case ARM64_INSN_USHL_SCALAR:
    case ARM64_INSN_URSHL_SCALAR:
    case ARM64_INSN_SUB_SCALAR:
    case ARM64_INSN_CMEQ_SCALAR:
    case ARM64_INSN_FMULX_SCALAR_BY_ELEMENT:
    case ARM64_INSN_SQRDMLAH_SCALAR_BY_ELEMENT:
    case ARM64_INSN_SQRDMLSH_SCALAR_BY_ELEMENT:
        use_rn = 1;
        use_rm = 1;
        break;
    case ARM64_INSN_FMADD_SCALAR:
    case ARM64_INSN_FMSUB_SCALAR:
    case ARM64_INSN_FNMADD_SCALAR:
    case ARM64_INSN_FNMSUB_SCALAR:
        use_rn = 1;
        use_rm = 1;
        use_ra = 1;
        break;
    case ARM64_INSN_FCMP_REGISTER_SCALAR:
    case ARM64_INSN_FCMPE_REGISTER_SCALAR:
        use_rd = 0;
        use_rn = 1;
        use_rm = 1;
        break;
    case ARM64_INSN_FCMP_ZERO_SCALAR:
    case ARM64_INSN_FCMPE_ZERO_SCALAR:
        use_rd = 0;
        use_rn = 1;
        break;
    case ARM64_INSN_FCCMP_SCALAR:
    case ARM64_INSN_FCCMPE_SCALAR:
        use_rd = 0;
        use_rn = 1;
        use_rm = 1;
        break;
    default:
        return 0;
    }

    if (!use_rd) decoded->rd = 0;
    if (!use_rn) decoded->rn = 0;
    if (!use_rm) decoded->rm = 0;
    if (!use_ra) decoded->ra = 0;
    return 1;
}
static uint64_t arm64_simd_expand_fp_imm(uint8_t immediate, uint8_t width)
{
    uint64_t sign = (uint64_t)(immediate >> 7) << (width - 1);
    uint64_t exponent_bit = (immediate >> 6) & 1;

    if (width == 16)
    {
        uint64_t exponent = ((!exponent_bit) << 4) | ((exponent_bit ? 0x3ULL : 0) << 2) | ((immediate >> 4) & 0x3);
        return sign | (exponent << 10) | ((uint64_t)(immediate & 0xF) << 6);
    }

    if (width == 32)
    {
        uint64_t exponent = ((!exponent_bit) << 7) | ((exponent_bit ? 0x1FULL : 0) << 2) | ((immediate >> 4) & 0x3);
        return sign | (exponent << 23) | ((uint64_t)(immediate & 0xF) << 19);
    }

    uint64_t exponent = ((!exponent_bit) << 10) | ((exponent_bit ? 0xFFULL : 0) << 2) | ((immediate >> 4) & 0x3);
    return sign | (exponent << 52) | ((uint64_t)(immediate & 0xF) << 48);
}

static uint8_t arm64_simd_scalar_fp_width(uint32_t type)
{
    switch (type)
    {
    case 0:
        return 32;
    case 1:
        return 64;
    case 3:
        return 16;
    default:
        return 0;
    }
}

static enum arm64_decode_status arm64_decode_simd_modified_imm(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    uint8_t immediate = (((raw >> 16) & 0x7) << 5) | ((raw >> 5) & 0x1F);
    uint8_t cmode = (raw >> 12) & 0xF;
    uint8_t op = (raw >> 29) & 1;
    uint64_t value = immediate;

    decoded->immediate = immediate;
    decoded->operand_width = raw & (1U << 30) ? 128 : 64;

    switch ((op << 4) | cmode)
    {
    case 0x00:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 32) | value;
        break;
    case 0x01:
        decoded->instruction = ARM64_INSN_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 32) | value;
        break;
    case 0x02:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 40) | (value << 8);
        break;
    case 0x03:
        decoded->instruction = ARM64_INSN_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 40) | (value << 8);
        break;
    case 0x04:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 48) | (value << 16);
        break;
    case 0x05:
        decoded->instruction = ARM64_INSN_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 48) | (value << 16);
        break;
    case 0x06:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 56) | (value << 24);
        break;
    case 0x07:
        decoded->instruction = ARM64_INSN_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 56) | (value << 24);
        break;
    case 0x08:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        decoded->expanded_immediate = (value << 48) | (value << 32) | (value << 16) | value;
        break;
    case 0x09:
        decoded->instruction = ARM64_INSN_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        decoded->expanded_immediate = (value << 48) | (value << 32) | (value << 16) | value;
        break;
    case 0x0A:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        decoded->expanded_immediate = (value << 56) | (value << 40) | (value << 24) | (value << 8);
        break;
    case 0x0B:
        decoded->instruction = ARM64_INSN_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        decoded->expanded_immediate = (value << 56) | (value << 40) | (value << 24) | (value << 8);
        break;
    case 0x0C:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 40) | (value << 8) | 0x000000FF000000FFULL;
        break;
    case 0x0D:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 48) | (value << 16) | 0x0000FFFF0000FFFFULL;
        break;
    case 0x0E:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 8;
        value |= value << 8;
        value |= value << 16;
        decoded->expanded_immediate = value | (value << 32);
        break;
    case 0x0F:
        decoded->instruction = ARM64_INSN_FMOV_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        value = arm64_simd_expand_fp_imm(immediate, 32);
        decoded->expanded_immediate = value | (value << 32);
        break;
    case 0x10:
        decoded->instruction = ARM64_INSN_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 32) | value;
        break;
    case 0x11:
        decoded->instruction = ARM64_INSN_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 32) | value;
        break;
    case 0x12:
        decoded->instruction = ARM64_INSN_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 40) | (value << 8);
        break;
    case 0x13:
        decoded->instruction = ARM64_INSN_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 40) | (value << 8);
        break;
    case 0x14:
        decoded->instruction = ARM64_INSN_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 48) | (value << 16);
        break;
    case 0x15:
        decoded->instruction = ARM64_INSN_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 48) | (value << 16);
        break;
    case 0x16:
        decoded->instruction = ARM64_INSN_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 56) | (value << 24);
        break;
    case 0x17:
        decoded->instruction = ARM64_INSN_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 56) | (value << 24);
        break;
    case 0x18:
        decoded->instruction = ARM64_INSN_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        decoded->expanded_immediate = (value << 48) | (value << 32) | (value << 16) | value;
        break;
    case 0x19:
        decoded->instruction = ARM64_INSN_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        decoded->expanded_immediate = (value << 48) | (value << 32) | (value << 16) | value;
        break;
    case 0x1A:
        decoded->instruction = ARM64_INSN_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        decoded->expanded_immediate = (value << 56) | (value << 40) | (value << 24) | (value << 8);
        break;
    case 0x1B:
        decoded->instruction = ARM64_INSN_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        decoded->expanded_immediate = (value << 56) | (value << 40) | (value << 24) | (value << 8);
        break;
    case 0x1C:
        decoded->instruction = ARM64_INSN_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 40) | (value << 8) | 0x000000FF000000FFULL;
        break;
    case 0x1D:
        decoded->instruction = ARM64_INSN_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        decoded->expanded_immediate = (value << 48) | (value << 16) | 0x0000FFFF0000FFFFULL;
        break;
    case 0x1E:
        decoded->instruction = ARM64_INSN_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 64;
        value = 0;
        for (uint8_t index = 0; index < 8; index++)
            if (immediate & (1U << index)) value |= 0xFFULL << (index * 8);
        decoded->expanded_immediate = value;
        break;
    case 0x1F:
        if (decoded->operand_width != 128) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INSN_FMOV_VECTOR_IMMEDIATE;
        decoded->element_width = 64;
        decoded->expanded_immediate = arm64_simd_expand_fp_imm(immediate, 64);
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_integer_3reg(struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t q, uint8_t size, uint8_t valid_sizes)
{
    if (!(valid_sizes & (1U << size)) || (!q && size == 3)) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = 8U << size;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_logical(struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t q)
{
    decoded->instruction = instruction;
    decoded->element_width = 8;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_fp_3reg(struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t q, uint8_t size)
{
    decoded->element_width = 32U << (size & 1);
    if (!q && decoded->element_width == 64) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_fhm(struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t q, uint8_t size)
{
    if (size & 1) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = 16;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_fp16_3reg(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    enum arm64_instruction instruction;

    switch ((((raw >> 29) & 1) << 6) | (((raw >> 23) & 1) << 5) | (((raw >> 11) & 0x7) + 24))
    {
    case 0x18:
        instruction = ARM64_INSN_FMAXNM_VECTOR;
        break;
    case 0x19:
        instruction = ARM64_INSN_FMLA_VECTOR;
        break;
    case 0x1A:
        instruction = ARM64_INSN_FADD_VECTOR;
        break;
    case 0x1B:
        instruction = ARM64_INSN_FMULX_VECTOR;
        break;
    case 0x1C:
        instruction = ARM64_INSN_FCMEQ_VECTOR;
        break;
    case 0x1E:
        instruction = ARM64_INSN_FMAX_VECTOR;
        break;
    case 0x1F:
        instruction = ARM64_INSN_FRECPS_VECTOR;
        break;
    case 0x38:
        instruction = ARM64_INSN_FMINNM_VECTOR;
        break;
    case 0x39:
        instruction = ARM64_INSN_FMLS_VECTOR;
        break;
    case 0x3A:
        instruction = ARM64_INSN_FSUB_VECTOR;
        break;
    case 0x3B:
        instruction = ARM64_INSN_FAMAX_VECTOR;
        break;
    case 0x3E:
        instruction = ARM64_INSN_FMIN_VECTOR;
        break;
    case 0x3F:
        instruction = ARM64_INSN_FRSQRTS_VECTOR;
        break;
    case 0x58:
        instruction = ARM64_INSN_FMAXNMP_VECTOR;
        break;
    case 0x5A:
        instruction = ARM64_INSN_FADDP_VECTOR;
        break;
    case 0x5B:
        instruction = ARM64_INSN_FMUL_VECTOR;
        break;
    case 0x5C:
        instruction = ARM64_INSN_FCMGE_VECTOR;
        break;
    case 0x5D:
        instruction = ARM64_INSN_FACGE_VECTOR;
        break;
    case 0x5E:
        instruction = ARM64_INSN_FMAXP_VECTOR;
        break;
    case 0x5F:
        instruction = ARM64_INSN_FDIV_VECTOR;
        break;
    case 0x78:
        instruction = ARM64_INSN_FMINNMP_VECTOR;
        break;
    case 0x7A:
        instruction = ARM64_INSN_FABD_VECTOR;
        break;
    case 0x7B:
        instruction = ARM64_INSN_FAMIN_VECTOR;
        break;
    case 0x7C:
        instruction = ARM64_INSN_FCMGT_VECTOR;
        break;
    case 0x7D:
        instruction = ARM64_INSN_FACGT_VECTOR;
        break;
    case 0x7E:
        instruction = ARM64_INSN_FMINP_VECTOR;
        break;
    case 0x7F:
        instruction = ARM64_INSN_FSCALE_VECTOR;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    decoded->instruction = instruction;
    decoded->element_width = 16;
    decoded->operand_width = raw & (1U << 30) ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_3same(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    /* 路由表索引依次对应 raw 的 u[29]、size[23] 和 opcode[15:11]。size[22]
    只决定 FP 元素宽度，整数有效尺寸仍由 valid_sizes 统一校验。 */
    static const enum arm64_instruction integer_instructions[2][24] = {
        {
            ARM64_INSN_SHADD_VECTOR, ARM64_INSN_SQADD_VECTOR, ARM64_INSN_SRHADD_VECTOR, ARM64_INSN_UNKNOWN, ARM64_INSN_SHSUB_VECTOR, ARM64_INSN_SQSUB_VECTOR, ARM64_INSN_CMGT_VECTOR, ARM64_INSN_CMGE_VECTOR, ARM64_INSN_SSHL_VECTOR, ARM64_INSN_SQSHL_VECTOR, ARM64_INSN_SRSHL_VECTOR, ARM64_INSN_SQRSHL_VECTOR, ARM64_INSN_SMAX_VECTOR, ARM64_INSN_SMIN_VECTOR, ARM64_INSN_SABD_VECTOR, ARM64_INSN_SABA_VECTOR, ARM64_INSN_ADD_VECTOR, ARM64_INSN_CMTST_VECTOR, ARM64_INSN_MLA_VECTOR, ARM64_INSN_MUL_VECTOR, ARM64_INSN_SMAXP_VECTOR, ARM64_INSN_SMINP_VECTOR, ARM64_INSN_SQDMULH_VECTOR, ARM64_INSN_ADDP_VECTOR,
        },
        {
            ARM64_INSN_UHADD_VECTOR, ARM64_INSN_UQADD_VECTOR, ARM64_INSN_URHADD_VECTOR, ARM64_INSN_UNKNOWN, ARM64_INSN_UHSUB_VECTOR, ARM64_INSN_UQSUB_VECTOR, ARM64_INSN_CMHI_VECTOR, ARM64_INSN_CMHS_VECTOR, ARM64_INSN_USHL_VECTOR, ARM64_INSN_UQSHL_VECTOR, ARM64_INSN_URSHL_VECTOR, ARM64_INSN_UQRSHL_VECTOR, ARM64_INSN_UMAX_VECTOR, ARM64_INSN_UMIN_VECTOR, ARM64_INSN_UABD_VECTOR, ARM64_INSN_UABA_VECTOR, ARM64_INSN_SUB_VECTOR, ARM64_INSN_CMEQ_VECTOR, ARM64_INSN_MLS_VECTOR, ARM64_INSN_PMUL_VECTOR, ARM64_INSN_UMAXP_VECTOR, ARM64_INSN_UMINP_VECTOR, ARM64_INSN_SQRDMULH_VECTOR, ARM64_INSN_UNKNOWN,
        },
    };
    static const uint8_t integer_valid_sizes[2][24] = {
        {
            0x7, 0xF, 0x7, 0, 0x7, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0x7, 0x7, 0x7, 0x7, 0xF, 0xF, 0x7, 0x7, 0x7, 0x7, 0x6, 0xF,
        },
        {
            0x7, 0xF, 0x7, 0, 0x7, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0xF, 0x7, 0x7, 0x7, 0x7, 0xF, 0xF, 0x7, 0x1, 0x7, 0x7, 0x6, 0,
        },
    };
    static const enum arm64_instruction logical_instructions[2][4] = {
        {ARM64_INSN_AND_VECTOR, ARM64_INSN_BIC_VECTOR, ARM64_INSN_ORR_VECTOR, ARM64_INSN_ORN_VECTOR},
        {ARM64_INSN_EOR_VECTOR, ARM64_INSN_BSL_VECTOR, ARM64_INSN_BIT_VECTOR, ARM64_INSN_BIF_VECTOR},
    };
    static const enum arm64_instruction fp_instructions[2][2][8] = {
        {
            {ARM64_INSN_FMAXNM_VECTOR, ARM64_INSN_FMLA_VECTOR, ARM64_INSN_FADD_VECTOR, ARM64_INSN_FMULX_VECTOR, ARM64_INSN_FCMEQ_VECTOR, ARM64_INSN_FMLAL_VECTOR, ARM64_INSN_FMAX_VECTOR, ARM64_INSN_FRECPS_VECTOR},
            {ARM64_INSN_FMINNM_VECTOR, ARM64_INSN_FMLS_VECTOR, ARM64_INSN_FSUB_VECTOR, ARM64_INSN_FAMAX_VECTOR, ARM64_INSN_UNKNOWN, ARM64_INSN_FMLSL_VECTOR, ARM64_INSN_FMIN_VECTOR, ARM64_INSN_FRSQRTS_VECTOR},
        },
        {
            {ARM64_INSN_FMAXNMP_VECTOR, ARM64_INSN_FMLAL2_VECTOR, ARM64_INSN_FADDP_VECTOR, ARM64_INSN_FMUL_VECTOR, ARM64_INSN_FCMGE_VECTOR, ARM64_INSN_FACGE_VECTOR, ARM64_INSN_FMAXP_VECTOR, ARM64_INSN_FDIV_VECTOR},
            {ARM64_INSN_FMINNMP_VECTOR, ARM64_INSN_FMLSL2_VECTOR, ARM64_INSN_FABD_VECTOR, ARM64_INSN_FAMIN_VECTOR, ARM64_INSN_FCMGT_VECTOR, ARM64_INSN_FACGT_VECTOR, ARM64_INSN_FMINP_VECTOR, ARM64_INSN_FSCALE_VECTOR},
        },
    };
    uint8_t q = (raw >> 30) & 1;
    uint8_t u = (raw >> 29) & 1;
    uint8_t size = (raw >> 22) & 0x3;
    uint8_t opcode = (raw >> 11) & 0x1F;
    enum arm64_instruction instruction;

    if (opcode < 24)
    {
        if (opcode == 3) return arm64_decode_simd_vector_logical(decoded, logical_instructions[u][size], q);
        instruction = integer_instructions[u][opcode];
        if (instruction == ARM64_INSN_UNKNOWN) return ARM64_DECODE_UNALLOCATED;
        return arm64_decode_simd_vector_integer_3reg(decoded, instruction, q, size, integer_valid_sizes[u][opcode]);
    }

    instruction = fp_instructions[u][size >> 1][opcode - 24];
    if (instruction == ARM64_INSN_UNKNOWN) return ARM64_DECODE_UNALLOCATED;
    if ((!u && opcode == 29) || (u && opcode == 25)) return arm64_decode_simd_vector_fhm(decoded, instruction, q, size);
    return arm64_decode_simd_vector_fp_3reg(decoded, instruction, q, size);
}

static enum arm64_decode_status arm64_decode_simd_scalar_integer_3reg(struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t size, uint8_t valid_sizes)
{
    if (!(valid_sizes & (1U << size))) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = 8U << size;
    decoded->operand_width = decoded->element_width;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_scalar_fp_3reg(struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t element_width)
{
    decoded->instruction = instruction;
    decoded->element_width = element_width;
    decoded->operand_width = element_width;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_scalar_fp16_3reg(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    enum arm64_instruction instruction;

    switch ((((raw >> 29) & 1) << 6) | (((raw >> 23) & 1) << 5) | (((raw >> 11) & 0x7) + 24))
    {
    case 0x1B:
        instruction = ARM64_INSN_FMULX_SCALAR;
        break;
    case 0x1C:
        instruction = ARM64_INSN_FCMEQ_SCALAR;
        break;
    case 0x1F:
        instruction = ARM64_INSN_FRECPS_SCALAR;
        break;
    case 0x3F:
        instruction = ARM64_INSN_FRSQRTS_SCALAR;
        break;
    case 0x5C:
        instruction = ARM64_INSN_FCMGE_SCALAR;
        break;
    case 0x5D:
        instruction = ARM64_INSN_FACGE_SCALAR;
        break;
    case 0x7A:
        instruction = ARM64_INSN_FABD_SCALAR;
        break;
    case 0x7C:
        instruction = ARM64_INSN_FCMGT_SCALAR;
        break;
    case 0x7D:
        instruction = ARM64_INSN_FACGT_SCALAR;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    return arm64_decode_simd_scalar_fp_3reg(decoded, instruction, 16);
}

static enum arm64_decode_status arm64_decode_simd_scalar_3same(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    struct arm64_simd_scalar_3same_route
    {
        enum arm64_instruction instruction;
        uint8_t valid_sizes;
    };
    /* 子表依次对应 raw 的 u[29]、size[23] 和 opcode[15:11]。 */
    static const struct arm64_simd_scalar_3same_route integer_routes[2][2][24] = {
        {
            {
                [1] = {ARM64_INSN_SQADD_SCALAR, 0xF},
                [5] = {ARM64_INSN_SQSUB_SCALAR, 0xF},
                [9] = {ARM64_INSN_SQSHL_SCALAR, 0xF},
                [11] = {ARM64_INSN_SQRSHL_SCALAR, 0xF},
                [22] = {ARM64_INSN_SQDMULH_SCALAR, 0x6},
            },
            {
                [1] = {ARM64_INSN_SQADD_SCALAR, 0xF},
                [5] = {ARM64_INSN_SQSUB_SCALAR, 0xF},
                [6] = {ARM64_INSN_CMGT_SCALAR, 0x8},
                [7] = {ARM64_INSN_CMGE_SCALAR, 0x8},
                [8] = {ARM64_INSN_SSHL_SCALAR, 0x8},
                [9] = {ARM64_INSN_SQSHL_SCALAR, 0xF},
                [10] = {ARM64_INSN_SRSHL_SCALAR, 0x8},
                [11] = {ARM64_INSN_SQRSHL_SCALAR, 0xF},
                [16] = {ARM64_INSN_ADD_SCALAR, 0x8},
                [17] = {ARM64_INSN_CMTST_SCALAR, 0x8},
                [22] = {ARM64_INSN_SQDMULH_SCALAR, 0x6},
            },
        },
        {
            {
                [1] = {ARM64_INSN_UQADD_SCALAR, 0xF},
                [5] = {ARM64_INSN_UQSUB_SCALAR, 0xF},
                [9] = {ARM64_INSN_UQSHL_SCALAR, 0xF},
                [11] = {ARM64_INSN_UQRSHL_SCALAR, 0xF},
                [22] = {ARM64_INSN_SQRDMULH_SCALAR, 0x6},
            },
            {
                [1] = {ARM64_INSN_UQADD_SCALAR, 0xF},
                [5] = {ARM64_INSN_UQSUB_SCALAR, 0xF},
                [6] = {ARM64_INSN_CMHI_SCALAR, 0x8},
                [7] = {ARM64_INSN_CMHS_SCALAR, 0x8},
                [8] = {ARM64_INSN_USHL_SCALAR, 0x8},
                [9] = {ARM64_INSN_UQSHL_SCALAR, 0xF},
                [10] = {ARM64_INSN_URSHL_SCALAR, 0x8},
                [11] = {ARM64_INSN_UQRSHL_SCALAR, 0xF},
                [16] = {ARM64_INSN_SUB_SCALAR, 0x8},
                [17] = {ARM64_INSN_CMEQ_SCALAR, 0x8},
                [22] = {ARM64_INSN_SQRDMULH_SCALAR, 0x6},
            },
        },
    };
    static const enum arm64_instruction fp_instructions[2][2][8] = {
        {
            {
                [3] = ARM64_INSN_FMULX_SCALAR,
                [4] = ARM64_INSN_FCMEQ_SCALAR,
                [7] = ARM64_INSN_FRECPS_SCALAR,
            },
            {
                [7] = ARM64_INSN_FRSQRTS_SCALAR,
            },
        },
        {
            {
                [4] = ARM64_INSN_FCMGE_SCALAR,
                [5] = ARM64_INSN_FACGE_SCALAR,
            },
            {
                [2] = ARM64_INSN_FABD_SCALAR,
                [4] = ARM64_INSN_FCMGT_SCALAR,
                [5] = ARM64_INSN_FACGT_SCALAR,
            },
        },
    };
    uint8_t u = (raw >> 29) & 1;
    uint8_t size = (raw >> 22) & 0x3;
    uint8_t opcode = (raw >> 11) & 0x1F;
    uint8_t size_high = size >> 1;

    if (opcode < 24)
    {
        if (integer_routes[u][size_high][opcode].instruction == ARM64_INSN_UNKNOWN) return ARM64_DECODE_UNALLOCATED;
        return arm64_decode_simd_scalar_integer_3reg(decoded, integer_routes[u][size_high][opcode].instruction, size, integer_routes[u][size_high][opcode].valid_sizes);
    }

    enum arm64_instruction instruction = fp_instructions[u][size_high][opcode - 24];
    if (instruction == ARM64_INSN_UNKNOWN) return ARM64_DECODE_UNALLOCATED;
    return arm64_decode_simd_scalar_fp_3reg(decoded, instruction, 32U << (size & 1));
}

#define ARM64_SIMD_BY_ELEMENT_SELECTOR(scalar, u, opcode, size) (((u) << 7) | ((scalar) << 6) | ((size) << 4) | (opcode))
#define ARM64_SIMD_EXTENDED_3REG_SELECTOR(u, opcode, size)      (((u) << 6) | ((size) << 4) | ((opcode) & 0xF))

static enum arm64_decode_status arm64_decode_simd_rdm_by_element(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t operand_width, uint8_t size)
{
    uint8_t h = (raw >> 11) & 1;
    uint8_t l = (raw >> 21) & 1;
    uint8_t m = (raw >> 20) & 1;

    if (size != 1 && size != 2) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = 8U << size;
    decoded->operand_width = operand_width;
    if (size == 1)
    {
        decoded->rm = (raw >> 16) & 0xF;
        decoded->lane_index = (h << 2) | (l << 1) | m;
    }
    else decoded->lane_index = (h << 1) | l;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_dot_by_element(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t q, uint8_t size, uint8_t element_width)
{
    if (size != 2) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = element_width;
    decoded->lane_index = (((raw >> 11) & 1) << 1) | ((raw >> 21) & 1);
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_bfmlal_by_element(uint32_t raw, struct arm64_decoded_insn *decoded, uint8_t q)
{
    decoded->instruction = q ? ARM64_INSN_BFMLALT_VECTOR_BY_ELEMENT : ARM64_INSN_BFMLALB_VECTOR_BY_ELEMENT;
    decoded->element_width = 16;
    decoded->lane_index = (((raw >> 11) & 1) << 2) | (((raw >> 21) & 1) << 1) | ((raw >> 20) & 1);
    decoded->operand_width = 128;
    decoded->rm = (raw >> 16) & 0xF;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_fcmla_by_element(uint32_t raw, struct arm64_decoded_insn *decoded, uint8_t q, uint8_t size)
{
    uint8_t h = (raw >> 11) & 1;
    uint8_t l = (raw >> 21) & 1;

    if (size == 0 || size == 3) return ARM64_DECODE_UNALLOCATED;
    if (size == 2 && (l || !q)) return ARM64_DECODE_UNALLOCATED;
    if (size == 1 && h && !q) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = ARM64_INSN_FCMLA_VECTOR_BY_ELEMENT;
    decoded->immediate = (raw >> 13) & 0x3;
    decoded->element_width = 8U << size;
    decoded->lane_index = size == 1 ? (h << 1) | l : h;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_fhm_by_element(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t q, uint8_t size)
{
    if (size != 2) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = 16;
    decoded->lane_index = (((raw >> 11) & 1) << 2) | (((raw >> 21) & 1) << 1) | ((raw >> 20) & 1);
    decoded->operand_width = q ? 128 : 64;
    decoded->rm = (raw >> 16) & 0xF;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_fp_by_element(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t operand_width, uint8_t size)
{
    uint8_t h = (raw >> 11) & 1;
    uint8_t l = (raw >> 21) & 1;

    if (size == 1) return ARM64_DECODE_UNALLOCATED;
    if (size == 3 && l) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = size == 0 ? 16 : 8U << size;
    decoded->operand_width = operand_width;
    if (size == 0)
    {
        decoded->rm = (raw >> 16) & 0xF;
        decoded->lane_index = (h << 2) | (l << 1) | ((raw >> 20) & 1);
    }
    else decoded->lane_index = size == 2 ? (h << 1) | l : h;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_reverse(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_instruction instruction, uint8_t valid_sizes)
{
    uint8_t size = (raw >> 22) & 0x3;

    if (!(valid_sizes & (1U << size))) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->operand_width = raw & (1U << 30) ? 128 : 64;
    decoded->element_width = 8U << size;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_integer_reduce(uint32_t raw, struct arm64_decoded_insn *decoded, enum arm64_instruction instruction)
{
    uint8_t q = (raw >> 30) & 1;
    uint8_t size = (raw >> 22) & 0x3;

    if (size == 3 || (size == 2 && !q)) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->operand_width = q ? 128 : 64;
    decoded->element_width = 8U << size;
    return ARM64_DECODE_OK;
}

static int arm64_simd_is_modified_imm_owner(uint32_t raw)
{
    return (raw & 0x9FF80C00U) == 0x0F000400U;
}

static int arm64_simd_is_vector_fp16_3same_owner(uint32_t raw)
{
    return (raw & 0x9F60C400U) == 0x0E400400U;
}

static int arm64_simd_is_scalar_fp16_3same_owner(uint32_t raw)
{
    return (raw & 0xDF60C400U) == 0x5E400400U;
}

static int arm64_simd_is_vector_permute_owner(uint32_t raw)
{
    return (raw & 0xBF208C00U) == 0x0E000800U;
}

static int arm64_simd_is_vector_complex_owner(uint32_t raw)
{
    return (raw & 0xBF20E400U) == 0x2E00C400U || (raw & 0xBF20EC00U) == 0x2E00E400U;
}

static int arm64_simd_is_by_element_shift_owner(uint32_t raw)
{
    if ((raw & 0x9F800400U) != 0x0F000000U || ((raw >> 19) & 0xF) == 0) return 0;

    uint8_t size = (raw >> 22) & 0x3;
    uint8_t opcode = (raw >> 12) & 0xF;
    if (!(raw & (1U << 29)))
    {
        if (opcode == 1 || opcode == 5 || opcode == 9 || opcode == 14 || opcode == 15) return 1;
        return size >= 2 && (opcode == 0 || opcode == 4);
    }

    if (opcode == 1 || opcode == 3 || opcode == 5 || opcode == 7 || opcode == 9 || opcode == 13 || opcode == 14 || opcode == 15) return 1;
    return size >= 2 && (opcode == 8 || opcode == 12);
}

static int arm64_simd_is_compare_zero_owner(uint32_t raw)
{
    if ((raw & 0x8F000C00U) != 0x0E000800U) return 0;

    uint32_t shape = raw & 0x00FF0000U;
    if (shape != 0x00F80000U && shape != 0x00A00000U && shape != 0x00E00000U) return 0;

    uint32_t relation = raw & 0x2000F000U;
    if (relation != 0x0000C000U && relation != 0x0000D000U && relation != 0x0000E000U && relation != 0x2000C000U && relation != 0x2000D000U) return 0;

    return !(raw & (1U << 28)) || raw & (1U << 30);
}

static int arm64_simd_is_vector_unary_owner(uint32_t raw)
{
    uint32_t shape = raw & 0x00FE0000U;
    if (shape != 0x00F80000U && shape != 0x00A00000U && shape != 0x00E00000U) return 0;

    uint32_t signature = raw & 0xBF01FC00U;
    return signature == 0x0E00F800U || signature == 0x2E00F800U || signature == 0x2E01F800U;
}

static int arm64_simd_is_convert_owner(uint32_t raw)
{
    switch (raw & 0xFFFFFC00U)
    {
    case 0x1E220000U:
    case 0x1E22C000U:
    case 0x1E230000U:
    case 0x1E620000U:
    case 0x1E624000U:
    case 0x1E630000U:
    case 0x9E220000U:
    case 0x9E230000U:
    case 0x9E620000U:
    case 0x9E630000U:
        return 1;
    default:
        break;
    }

    switch (raw & 0x8FBFFC00U)
    {
    case 0x0E21A800U:
    case 0x0EA1A800U:
    case 0x0E21B800U:
    case 0x0E21C800U:
    case 0x0EA1B800U:
    case 0x0E21D800U:
        return 1;
    default:
        break;
    }

    if ((raw & 0x7F20FC00U) != 0x1E200000U || ((raw >> 22) & 0x3) > 1) return 0;

    uint32_t rounding = (raw >> 19) & 0x3;
    uint32_t opcode = (raw >> 16) & 0x7;
    return opcode <= 1 || (rounding == 0 && (opcode == 4 || opcode == 5));
}

/*
所有 FP/AdvSIMD 编码签名只在本文件内匹配；成功分支直接选择具体 instruction，
随后填写该 instruction 对应的真实操作数。
*/
static enum arm64_decode_status arm64_decode_simd_impl(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    decoded->insn_class = ARM64_INSN_CLASS_DATA_PROCESSING_SIMD_FP;
    decoded->rd = raw & 0x1F;
    decoded->rn = (raw >> 5) & 0x1F;
    decoded->ra = (raw >> 10) & 0x1F;
    decoded->rm = (raw >> 16) & 0x1F;

    if (arm64_simd_is_modified_imm_owner(raw))
    {
        return arm64_decode_simd_modified_imm(raw, decoded);
    }

    if ((raw & 0xFF000000U) == 0x1F000000U)
    {
        uint8_t width = arm64_simd_scalar_fp_width((raw >> 22) & 0x3);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        switch ((((raw >> 21) & 1) << 1) | ((raw >> 15) & 1))
        {
        case 0:
            decoded->instruction = ARM64_INSN_FMADD_SCALAR;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_FMSUB_SCALAR;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_FNMADD_SCALAR;
            break;
        default:
            decoded->instruction = ARM64_INSN_FNMSUB_SCALAR;
            break;
        }
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF200C00U) == 0x1E200800U && !arm64_simd_is_compare_zero_owner(raw) && !arm64_simd_is_convert_owner(raw))
    {
        uint32_t opcode = (raw >> 12) & 0xF;
        uint8_t width = arm64_simd_scalar_fp_width((raw >> 22) & 0x3);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        switch (opcode)
        {
        case 0:
            decoded->instruction = ARM64_INSN_FMUL_SCALAR;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_FDIV_SCALAR;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_FADD_SCALAR;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_FSUB_SCALAR;
            break;
        case 4:
            decoded->instruction = ARM64_INSN_FMAX_SCALAR;
            break;
        case 5:
            decoded->instruction = ARM64_INSN_FMIN_SCALAR;
            break;
        case 6:
            decoded->instruction = ARM64_INSN_FMAXNM_SCALAR;
            break;
        case 7:
            decoded->instruction = ARM64_INSN_FMINNM_SCALAR;
            break;
        case 8:
            decoded->instruction = ARM64_INSN_FNMUL_SCALAR;
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF201FE0U) == 0x1E201000U)
    {
        uint8_t immediate = (raw >> 13) & 0xFF;
        uint8_t width = arm64_simd_scalar_fp_width((raw >> 22) & 0x3);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INSN_FMOV_SCALAR_IMMEDIATE;
        decoded->immediate = immediate;
        decoded->operand_width = width;
        decoded->element_width = width;
        decoded->expanded_immediate = arm64_simd_expand_fp_imm(immediate, width);
        return ARM64_DECODE_OK;
    }

    if (arm64_simd_is_vector_permute_owner(raw))
    {
        uint8_t opcode = (raw >> 12) & 0x7;
        uint8_t size = (raw >> 22) & 0x3;
        uint8_t q = (raw >> 30) & 1;

        switch (opcode)
        {
        case 1:
            decoded->instruction = ARM64_INSN_UZP1_VECTOR;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_TRN1_VECTOR;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_ZIP1_VECTOR;
            break;
        case 5:
            decoded->instruction = ARM64_INSN_UZP2_VECTOR;
            break;
        case 6:
            decoded->instruction = ARM64_INSN_TRN2_VECTOR;
            break;
        case 7:
            decoded->instruction = ARM64_INSN_ZIP2_VECTOR;
            break;
        default:
            return ARM64_DECODE_UNALLOCATED;
        }

        if (!q && size == 3) return ARM64_DECODE_UNALLOCATED;
        decoded->operand_width = q ? 128 : 64;
        decoded->element_width = 8U << size;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x9FE08400U) == 0x0E000400U)
    {
        uint8_t imm5 = (raw >> 16) & 0x1F;
        uint8_t imm4 = (raw >> 11) & 0xF;
        uint8_t q = (raw >> 30) & 1;

        if (!imm5) return ARM64_DECODE_UNALLOCATED;
        uint8_t size = (uint8_t)__builtin_ctz(imm5);
        if (size > 3) return ARM64_DECODE_UNALLOCATED;

        decoded->element_width = 8U << size;
        if (!q && decoded->element_width == 64) return ARM64_DECODE_UNALLOCATED;
        decoded->lane_index = imm5 >> (size + 1);

        if (raw & (1U << 29))
        {
            if (!q) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INSN_INS_ELEMENT_VECTOR;
            decoded->source_lane_index = imm4 >> size;
            decoded->operand_width = 128;
            return ARM64_DECODE_OK;
        }

        switch (imm4)
        {
        case 0:
            decoded->instruction = ARM64_INSN_DUP_ELEMENT_VECTOR;
            decoded->operand_width = q ? 128 : 64;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_DUP_GENERAL_VECTOR;
            decoded->operand_width = q ? 128 : 64;
            break;
        case 3:
            if (!q) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INSN_INS_GPR_VECTOR;
            decoded->operand_width = 128;
            break;
        case 5:
            if ((!q && decoded->element_width > 16) || (q && decoded->element_width > 32)) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INSN_SMOV_VECTOR_TO_GPR;
            decoded->operand_width = q ? 64 : 32;
            break;
        case 7:
            if ((!q && decoded->element_width > 32) || (q && decoded->element_width != 64)) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INSN_UMOV_VECTOR_TO_GPR;
            decoded->operand_width = q ? 64 : 32;
            break;
        default:
            return ARM64_DECODE_UNALLOCATED;
        }

        return ARM64_DECODE_OK;
    }

    if ((raw & 0x9F200400U) == 0x0E200400U && !arm64_simd_is_vector_fp16_3same_owner(raw)) return arm64_decode_simd_vector_3same(raw, decoded);

    if (!arm64_simd_is_compare_zero_owner(raw)) switch (raw & 0xBF3FFC00U)
        {
        case 0x0E200800U:
            return arm64_decode_simd_vector_reverse(raw, decoded, ARM64_INSN_REV64_VECTOR, 0x7);
        case 0x0E201800U:
            return arm64_decode_simd_vector_reverse(raw, decoded, ARM64_INSN_REV16_VECTOR, 0x1);
        case 0x0E303800U:
            return arm64_decode_simd_vector_integer_reduce(raw, decoded, ARM64_INSN_SADDLV_VECTOR);
        case 0x0E30A800U:
            return arm64_decode_simd_vector_integer_reduce(raw, decoded, ARM64_INSN_SMAXV_VECTOR);
        case 0x0E31A800U:
            return arm64_decode_simd_vector_integer_reduce(raw, decoded, ARM64_INSN_SMINV_VECTOR);
        case 0x0E31B800U:
            return arm64_decode_simd_vector_integer_reduce(raw, decoded, ARM64_INSN_ADDV_VECTOR);
        case 0x2E200800U:
            return arm64_decode_simd_vector_reverse(raw, decoded, ARM64_INSN_REV32_VECTOR, 0x3);
        case 0x2E303800U:
            return arm64_decode_simd_vector_integer_reduce(raw, decoded, ARM64_INSN_UADDLV_VECTOR);
        case 0x2E30A800U:
            return arm64_decode_simd_vector_integer_reduce(raw, decoded, ARM64_INSN_UMAXV_VECTOR);
        case 0x2E31A800U:
            return arm64_decode_simd_vector_integer_reduce(raw, decoded, ARM64_INSN_UMINV_VECTOR);
        default:
            break;
        }

    if (!arm64_simd_is_compare_zero_owner(raw))
    {
        uint32_t size = (raw >> 22) & 0x3;

        switch (raw & 0xFF3FFC00U)
        {
        case 0x0E212800U:
            decoded->instruction = ARM64_INSN_XTN_VECTOR;
            decoded->operand_width = raw & (1U << 30) ? 128 : 64;
            break;
        case 0x0E214800U:
            decoded->instruction = ARM64_INSN_SQXTN_VECTOR;
            decoded->operand_width = raw & (1U << 30) ? 128 : 64;
            break;
        case 0x2E212800U:
            decoded->instruction = ARM64_INSN_SQXTUN_VECTOR;
            decoded->operand_width = raw & (1U << 30) ? 128 : 64;
            break;
        case 0x2E214800U:
            decoded->instruction = ARM64_INSN_UQXTN_VECTOR;
            decoded->operand_width = raw & (1U << 30) ? 128 : 64;
            break;
        case 0x4E212800U:
            decoded->instruction = ARM64_INSN_XTN2_VECTOR;
            decoded->operand_width = 128;
            break;
        case 0x4E214800U:
            decoded->instruction = ARM64_INSN_SQXTN2_VECTOR;
            decoded->operand_width = 128;
            break;
        case 0x5E212800U:
            return ARM64_DECODE_UNALLOCATED;
        case 0x5E214800U:
            decoded->instruction = ARM64_INSN_SQXTN_SCALAR;
            decoded->operand_width = 8U << size;
            break;
        case 0x6E212800U:
            decoded->instruction = ARM64_INSN_SQXTUN2_VECTOR;
            decoded->operand_width = 128;
            break;
        case 0x6E214800U:
            decoded->instruction = ARM64_INSN_UQXTN2_VECTOR;
            decoded->operand_width = 128;
            break;
        case 0x7E212800U:
            decoded->instruction = ARM64_INSN_SQXTUN_SCALAR;
            decoded->operand_width = 8U << size;
            break;
        case 0x7E214800U:
            decoded->instruction = ARM64_INSN_UQXTN_SCALAR;
            decoded->operand_width = 8U << size;
            break;
        default:
            break;
        }

        if (decoded->instruction != ARM64_INSN_UNKNOWN)
        {
            if (size == 3) return ARM64_DECODE_UNALLOCATED;
            decoded->element_width = 16U << size;
            return ARM64_DECODE_OK;
        }
    }

    if (!arm64_simd_is_compare_zero_owner(raw))
    {
        switch (raw & 0xFFFFFC00U)
        {
        case 0x1E220000U:
            decoded->instruction = ARM64_INSN_SCVTF_S_W;
            decoded->operand_width = 32;
            decoded->element_width = 32;
            break;
        case 0x1E22C000U:
            decoded->instruction = ARM64_INSN_FCVT_D_S;
            decoded->operand_width = 64;
            decoded->element_width = 32;
            break;
        case 0x1E230000U:
            decoded->instruction = ARM64_INSN_UCVTF_S_W;
            decoded->operand_width = 32;
            decoded->element_width = 32;
            break;
        case 0x1E620000U:
            decoded->instruction = ARM64_INSN_SCVTF_D_W;
            decoded->operand_width = 64;
            decoded->element_width = 32;
            break;
        case 0x1E624000U:
            decoded->instruction = ARM64_INSN_FCVT_S_D;
            decoded->operand_width = 32;
            decoded->element_width = 64;
            break;
        case 0x1E630000U:
            decoded->instruction = ARM64_INSN_UCVTF_D_W;
            decoded->operand_width = 64;
            decoded->element_width = 32;
            break;
        case 0x9E220000U:
            decoded->instruction = ARM64_INSN_SCVTF_S_X;
            decoded->operand_width = 32;
            decoded->element_width = 64;
            break;
        case 0x9E230000U:
            decoded->instruction = ARM64_INSN_UCVTF_S_X;
            decoded->operand_width = 32;
            decoded->element_width = 64;
            break;
        case 0x9E620000U:
            decoded->instruction = ARM64_INSN_SCVTF_D_X;
            decoded->operand_width = 64;
            decoded->element_width = 64;
            break;
        case 0x9E630000U:
            decoded->instruction = ARM64_INSN_UCVTF_D_X;
            decoded->operand_width = 64;
            decoded->element_width = 64;
            break;
        default:
            break;
        }

        if (decoded->instruction != ARM64_INSN_UNKNOWN) return ARM64_DECODE_OK;

        switch (raw & 0x8FBFFC00U)
        {
        case 0x0E21A800U:
            decoded->instruction = raw & (1U << 29) ? (raw & (1U << 28) ? ARM64_INSN_FCVTNU_SIMD_SCALAR : ARM64_INSN_FCVTNU_SIMD_VECTOR) : (raw & (1U << 28) ? ARM64_INSN_FCVTNS_SIMD_SCALAR : ARM64_INSN_FCVTNS_SIMD_VECTOR);
            break;
        case 0x0EA1A800U:
            decoded->instruction = raw & (1U << 29) ? (raw & (1U << 28) ? ARM64_INSN_FCVTPU_SIMD_SCALAR : ARM64_INSN_FCVTPU_SIMD_VECTOR) : (raw & (1U << 28) ? ARM64_INSN_FCVTPS_SIMD_SCALAR : ARM64_INSN_FCVTPS_SIMD_VECTOR);
            break;
        case 0x0E21B800U:
            decoded->instruction = raw & (1U << 29) ? (raw & (1U << 28) ? ARM64_INSN_FCVTMU_SIMD_SCALAR : ARM64_INSN_FCVTMU_SIMD_VECTOR) : (raw & (1U << 28) ? ARM64_INSN_FCVTMS_SIMD_SCALAR : ARM64_INSN_FCVTMS_SIMD_VECTOR);
            break;
        case 0x0E21C800U:
            decoded->instruction = raw & (1U << 29) ? (raw & (1U << 28) ? ARM64_INSN_FCVTAU_SIMD_SCALAR : ARM64_INSN_FCVTAU_SIMD_VECTOR) : (raw & (1U << 28) ? ARM64_INSN_FCVTAS_SIMD_SCALAR : ARM64_INSN_FCVTAS_SIMD_VECTOR);
            break;
        case 0x0EA1B800U:
            decoded->instruction = raw & (1U << 29) ? (raw & (1U << 28) ? ARM64_INSN_FCVTZU_SIMD_SCALAR : ARM64_INSN_FCVTZU_SIMD_VECTOR) : (raw & (1U << 28) ? ARM64_INSN_FCVTZS_SIMD_SCALAR : ARM64_INSN_FCVTZS_SIMD_VECTOR);
            break;
        case 0x0E21D800U:
            decoded->instruction = raw & (1U << 29) ? (raw & (1U << 28) ? ARM64_INSN_UCVTF_SIMD_SCALAR : ARM64_INSN_UCVTF_SIMD_VECTOR) : (raw & (1U << 28) ? ARM64_INSN_SCVTF_SIMD_SCALAR : ARM64_INSN_SCVTF_SIMD_VECTOR);
            break;
        default:
            break;
        }

        if (decoded->instruction != ARM64_INSN_UNKNOWN)
        {
            decoded->element_width = raw & (1U << 22) ? 64 : 32;
            if (raw & (1U << 28))
            {
                if (!(raw & (1U << 30))) return ARM64_DECODE_UNALLOCATED;
                decoded->operand_width = decoded->element_width;
            }
            else
            {
                decoded->operand_width = raw & (1U << 30) ? 128 : 64;
                if (decoded->operand_width == 64 && decoded->element_width == 64) return ARM64_DECODE_UNALLOCATED;
            }
            return ARM64_DECODE_OK;
        }

        if ((raw & 0x7F20FC00U) == 0x1E200000U && ((raw >> 22) & 0x3) <= 1)
        {
            uint32_t selector = (((raw >> 19) & 0x3) << 3) | ((raw >> 16) & 0x7);

            switch (selector)
            {
            case 0x00:
                decoded->instruction = ARM64_INSN_FCVTNS_GPR;
                break;
            case 0x01:
                decoded->instruction = ARM64_INSN_FCVTNU_GPR;
                break;
            case 0x04:
                decoded->instruction = ARM64_INSN_FCVTAS_GPR;
                break;
            case 0x05:
                decoded->instruction = ARM64_INSN_FCVTAU_GPR;
                break;
            case 0x08:
                decoded->instruction = ARM64_INSN_FCVTPS_GPR;
                break;
            case 0x09:
                decoded->instruction = ARM64_INSN_FCVTPU_GPR;
                break;
            case 0x10:
                decoded->instruction = ARM64_INSN_FCVTMS_GPR;
                break;
            case 0x11:
                decoded->instruction = ARM64_INSN_FCVTMU_GPR;
                break;
            case 0x18:
                decoded->instruction = ARM64_INSN_FCVTZS_GPR;
                break;
            case 0x19:
                decoded->instruction = ARM64_INSN_FCVTZU_GPR;
                break;
            default:
                break;
            }

            if (decoded->instruction != ARM64_INSN_UNKNOWN)
            {
                decoded->operand_width = raw & 0x80000000U ? 64 : 32;
                decoded->element_width = raw & 0x00400000U ? 64 : 32;
                return ARM64_DECODE_OK;
            }
        }
    }

    if (!arm64_simd_is_compare_zero_owner(raw))
    {
        enum arm64_instruction instruction = ARM64_INSN_UNKNOWN;

        switch (raw & 0x9FFFFC00U)
        {
        case 0x0E30C800U:
            instruction = ARM64_INSN_FMAXNMV_SCALAR_REDUCE;
            break;
        case 0x0E30F800U:
            instruction = ARM64_INSN_FMAXV_SCALAR_REDUCE;
            break;
        case 0x0EB0C800U:
            instruction = ARM64_INSN_FMINNMV_SCALAR_REDUCE;
            break;
        case 0x0EB0F800U:
            instruction = ARM64_INSN_FMINV_SCALAR_REDUCE;
            break;
        default:
            break;
        }

        if (instruction != ARM64_INSN_UNKNOWN)
        {
            if (raw & (1U << 29))
            {
                if (!(raw & (1U << 30))) return ARM64_DECODE_UNALLOCATED;
                decoded->element_width = 32;
                decoded->operand_width = 128;
            }
            else
            {
                decoded->element_width = 16;
                decoded->operand_width = raw & (1U << 30) ? 128 : 64;
            }
            decoded->instruction = instruction;
            return ARM64_DECODE_OK;
        }
    }

    if (arm64_simd_is_vector_fp16_3same_owner(raw)) return arm64_decode_simd_vector_fp16_3reg(raw, decoded);

    if ((raw & 0x9F208400U) == 0x0E008400U && !arm64_simd_is_vector_complex_owner(raw))
    {
        uint8_t q = (raw >> 30) & 1;
        uint8_t u = (raw >> 29) & 1;
        uint8_t size = (raw >> 22) & 0x3;
        uint8_t opcode = (raw >> 11) & 0x1F;
        enum arm64_instruction instruction = ARM64_INSN_UNKNOWN;

        decoded->operand_width = q ? 128 : 64;
        switch (ARM64_SIMD_EXTENDED_3REG_SELECTOR(u, opcode, size))
        {
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 2, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 3, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 4, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 5, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 2, 1):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 3, 1):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 4, 1):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 5, 1):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 2, 2):
            instruction = ARM64_INSN_SDOT_VECTOR;
            decoded->element_width = 8;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 3, 2):
            instruction = ARM64_INSN_USDOT_VECTOR;
            decoded->element_width = 8;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 4, 2):
            if (!q) return ARM64_DECODE_UNALLOCATED;
            instruction = ARM64_INSN_SMMLA_VECTOR;
            decoded->element_width = 8;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 5, 2):
            if (!q) return ARM64_DECODE_UNALLOCATED;
            instruction = ARM64_INSN_USMMLA_VECTOR;
            decoded->element_width = 8;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 2, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 3, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 4, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(0, 5, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 0, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 1, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 2, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 4, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 13, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 15, 0):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 0, 1):
            instruction = ARM64_INSN_SQRDMLAH_VECTOR;
            decoded->element_width = 8U << size;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 1, 1):
            instruction = ARM64_INSN_SQRDMLSH_VECTOR;
            decoded->element_width = 8U << size;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 2, 1):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 4, 1):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 13, 1):
            if (!q) return ARM64_DECODE_UNALLOCATED;
            instruction = ARM64_INSN_BFMMLA_VECTOR;
            decoded->element_width = 16;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 15, 1):
            instruction = ARM64_INSN_BFDOT_VECTOR;
            decoded->element_width = 16;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 0, 2):
            instruction = ARM64_INSN_SQRDMLAH_VECTOR;
            decoded->element_width = 8U << size;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 1, 2):
            instruction = ARM64_INSN_SQRDMLSH_VECTOR;
            decoded->element_width = 8U << size;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 2, 2):
            instruction = ARM64_INSN_UDOT_VECTOR;
            decoded->element_width = 8;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 4, 2):
            if (!q) return ARM64_DECODE_UNALLOCATED;
            instruction = ARM64_INSN_UMMLA_VECTOR;
            decoded->element_width = 8;
            break;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 13, 2):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 15, 2):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 0, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 1, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 2, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 4, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 13, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_EXTENDED_3REG_SELECTOR(1, 15, 3):
            instruction = q ? ARM64_INSN_BFMLALT_VECTOR : ARM64_INSN_BFMLALB_VECTOR;
            decoded->operand_width = 128;
            decoded->element_width = 16;
            break;
        default:
            break;
        }

        if (instruction != ARM64_INSN_UNKNOWN)
        {
            decoded->instruction = instruction;
            return ARM64_DECODE_OK;
        }
    }

    if (arm64_simd_is_compare_zero_owner(raw) && !arm64_simd_is_vector_permute_owner(raw))
    {
        uint32_t shape = raw & 0x00FF0000U;
        uint32_t relation = raw & 0x2000F000U;
        uint8_t scalar = (raw >> 28) & 1;
        uint8_t q = (raw >> 30) & 1;
        uint8_t element_width = 0;
        enum arm64_instruction instruction = ARM64_INSN_UNKNOWN;

        if (shape == 0x00F80000U) element_width = 16;
        else if (shape == 0x00A00000U) element_width = 32;
        else if (shape == 0x00E00000U) element_width = 64;

        switch (relation)
        {
        case 0x0000C000U:
            instruction = scalar ? ARM64_INSN_FCMGT_ZERO_SCALAR : ARM64_INSN_FCMGT_ZERO_VECTOR;
            break;
        case 0x0000D000U:
            instruction = scalar ? ARM64_INSN_FCMEQ_ZERO_SCALAR : ARM64_INSN_FCMEQ_ZERO_VECTOR;
            break;
        case 0x0000E000U:
            instruction = scalar ? ARM64_INSN_FCMLT_ZERO_SCALAR : ARM64_INSN_FCMLT_ZERO_VECTOR;
            break;
        case 0x2000C000U:
            instruction = scalar ? ARM64_INSN_FCMGE_ZERO_SCALAR : ARM64_INSN_FCMGE_ZERO_VECTOR;
            break;
        case 0x2000D000U:
            instruction = scalar ? ARM64_INSN_FCMLE_ZERO_SCALAR : ARM64_INSN_FCMLE_ZERO_VECTOR;
            break;
        default:
            break;
        }

        if (element_width && instruction != ARM64_INSN_UNKNOWN && (!scalar || q))
        {
            if (scalar)
            {
                decoded->operand_width = element_width;
            }
            else
            {
                if (!q && element_width == 64) return ARM64_DECODE_UNALLOCATED;
                decoded->operand_width = q ? 128 : 64;
            }

            decoded->instruction = instruction;
            decoded->element_width = element_width;
            return ARM64_DECODE_OK;
        }
    }

    if (arm64_simd_is_vector_unary_owner(raw) && !arm64_simd_is_compare_zero_owner(raw))
    {
        uint32_t shape = raw & 0x00FE0000U;
        uint32_t signature = raw & 0xBF01FC00U;
        uint8_t q = (raw >> 30) & 1;
        uint8_t element_width = 0;
        enum arm64_instruction instruction = ARM64_INSN_UNKNOWN;

        switch (shape)
        {
        case 0x00F80000U:
            element_width = 16;
            break;
        case 0x00A00000U:
            element_width = 32;
            break;
        case 0x00E00000U:
            if (!q) return ARM64_DECODE_UNALLOCATED;
            element_width = 64;
            break;
        default:
            break;
        }

        if (element_width)
        {
            if (signature == 0x0E00F800U) instruction = ARM64_INSN_FABS_VECTOR;
            else if (signature == 0x2E00F800U) instruction = ARM64_INSN_FNEG_VECTOR;
            else if (signature == 0x2E01F800U) instruction = ARM64_INSN_FSQRT_VECTOR;

            if (instruction != ARM64_INSN_UNKNOWN)
            {
                decoded->instruction = instruction;
                decoded->element_width = element_width;
                decoded->operand_width = q ? 128 : 64;
                return ARM64_DECODE_OK;
            }
        }
    }

    if ((raw & 0xDF000400U) == 0x0F000000U || (raw & 0xDF000400U) == 0x4F000000U || (raw & 0xDF000400U) == 0x5F000000U)
    {
        uint8_t size = (raw >> 22) & 0x3;
        uint8_t opcode = (raw >> 12) & 0xF;
        uint8_t scalar = (raw >> 28) & 1;
        uint8_t q = (raw >> 30) & 1;
        uint8_t u = (raw >> 29) & 1;

        switch (ARM64_SIMD_BY_ELEMENT_SELECTOR(scalar, u, opcode, size))
        {
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 1, 0):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLA_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 5, 0):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLS_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 9, 0):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMUL_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 14, 0):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_SDOT_VECTOR_BY_ELEMENT, q, size, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 15, 0):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_SUDOT_VECTOR_BY_ELEMENT, q, 2, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 1, 1):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLA_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 5, 1):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLS_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 9, 1):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMUL_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 14, 1):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_SDOT_VECTOR_BY_ELEMENT, q, size, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 15, 1):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_BFDOT_VECTOR_BY_ELEMENT, q, 2, 16);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 0, 2):
            return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INSN_FMLAL_VECTOR_BY_ELEMENT, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 1, 2):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLA_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 4, 2):
            return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INSN_FMLSL_VECTOR_BY_ELEMENT, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 5, 2):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLS_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 9, 2):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMUL_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 14, 2):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_SDOT_VECTOR_BY_ELEMENT, q, size, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 15, 2):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_USDOT_VECTOR_BY_ELEMENT, q, 2, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 0, 3):
            return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INSN_FMLAL_VECTOR_BY_ELEMENT, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 1, 3):
            if (!q) return ARM64_DECODE_UNALLOCATED;
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLA_VECTOR_BY_ELEMENT, 128, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 4, 3):
            return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INSN_FMLSL_VECTOR_BY_ELEMENT, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 5, 3):
            if (!q) return ARM64_DECODE_UNALLOCATED;
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLS_VECTOR_BY_ELEMENT, 128, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 9, 3):
            if (!q) return ARM64_DECODE_UNALLOCATED;
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMUL_VECTOR_BY_ELEMENT, 128, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 14, 3):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_SDOT_VECTOR_BY_ELEMENT, q, size, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 0, 15, 3):
            return arm64_decode_simd_bfmlal_by_element(raw, decoded, q);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 1, 0):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLA_SCALAR_BY_ELEMENT, 16, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 5, 0):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLS_SCALAR_BY_ELEMENT, 16, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 9, 0):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMUL_SCALAR_BY_ELEMENT, 16, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 1, 1):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLA_SCALAR_BY_ELEMENT, 16, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 5, 1):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLS_SCALAR_BY_ELEMENT, 16, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 9, 1):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMUL_SCALAR_BY_ELEMENT, 16, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 0, 2):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 1, 2):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLA_SCALAR_BY_ELEMENT, 32, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 4, 2):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 5, 2):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLS_SCALAR_BY_ELEMENT, 32, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 9, 2):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMUL_SCALAR_BY_ELEMENT, 32, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 0, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 1, 3):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLA_SCALAR_BY_ELEMENT, 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 4, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 5, 3):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMLS_SCALAR_BY_ELEMENT, 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 0, 9, 3):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMUL_SCALAR_BY_ELEMENT, 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 1, 0):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 3, 0):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 5, 0):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 7, 0):
            return arm64_decode_simd_fcmla_by_element(raw, decoded, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 9, 0):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMULX_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 13, 0):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLAH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 14, 0):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_UDOT_VECTOR_BY_ELEMENT, q, size, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 15, 0):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLSH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 1, 1):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 3, 1):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 5, 1):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 7, 1):
            return arm64_decode_simd_fcmla_by_element(raw, decoded, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 9, 1):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMULX_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 13, 1):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLAH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 14, 1):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_UDOT_VECTOR_BY_ELEMENT, q, size, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 15, 1):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLSH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 1, 2):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 3, 2):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 5, 2):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 7, 2):
            return arm64_decode_simd_fcmla_by_element(raw, decoded, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 8, 2):
            return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INSN_FMLAL2_VECTOR_BY_ELEMENT, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 9, 2):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMULX_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 12, 2):
            return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INSN_FMLSL2_VECTOR_BY_ELEMENT, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 13, 2):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLAH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 14, 2):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_UDOT_VECTOR_BY_ELEMENT, q, size, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 15, 2):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLSH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 1, 3):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 3, 3):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 5, 3):
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 7, 3):
            return arm64_decode_simd_fcmla_by_element(raw, decoded, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 8, 3):
            return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INSN_FMLAL2_VECTOR_BY_ELEMENT, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 9, 3):
            if (!q) return ARM64_DECODE_UNALLOCATED;
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMULX_VECTOR_BY_ELEMENT, 128, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 12, 3):
            return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INSN_FMLSL2_VECTOR_BY_ELEMENT, q, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 13, 3):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLAH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 14, 3):
            return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INSN_UDOT_VECTOR_BY_ELEMENT, q, size, 8);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(0, 1, 15, 3):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLSH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 9, 0):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMULX_SCALAR_BY_ELEMENT, 16, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 13, 0):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLAH_SCALAR_BY_ELEMENT, 8U << size, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 15, 0):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLSH_SCALAR_BY_ELEMENT, 8U << size, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 9, 1):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMULX_SCALAR_BY_ELEMENT, 16, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 13, 1):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLAH_SCALAR_BY_ELEMENT, 8U << size, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 15, 1):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLSH_SCALAR_BY_ELEMENT, 8U << size, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 8, 2):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 9, 2):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMULX_SCALAR_BY_ELEMENT, 32, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 12, 2):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 13, 2):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLAH_SCALAR_BY_ELEMENT, 8U << size, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 15, 2):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLSH_SCALAR_BY_ELEMENT, 8U << size, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 8, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 9, 3):
            return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INSN_FMULX_SCALAR_BY_ELEMENT, 64, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 12, 3):
            return ARM64_DECODE_UNALLOCATED;
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 13, 3):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLAH_SCALAR_BY_ELEMENT, 8U << size, size);
        case ARM64_SIMD_BY_ELEMENT_SELECTOR(1, 1, 15, 3):
            return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INSN_SQRDMLSH_SCALAR_BY_ELEMENT, 8U << size, size);
        default:
            break;
        }
    }

    if ((raw & 0x9F800000U) == 0x0F000000U && ((raw >> 19) & 0xF) != 0 && !arm64_simd_is_modified_imm_owner(raw) && !arm64_simd_is_by_element_shift_owner(raw))
    {
        uint8_t immh = (raw >> 19) & 0xF;
        uint8_t immb = (raw >> 16) & 0x7;
        uint8_t opcode = (raw >> 11) & 0x1F;
        enum arm64_instruction instruction = ARM64_INSN_UNKNOWN;

        if (opcode == 0 && (raw & (1U << 10))) instruction = raw & (1U << 29) ? ARM64_INSN_USHR_VECTOR_IMMEDIATE : ARM64_INSN_SSHR_VECTOR_IMMEDIATE;
        else if (opcode == 10 && !(raw & (1U << 29)))
        {
            if (!(raw & (1U << 10)))
            {
                if (((raw >> 22) & 0x3) == 1) return ARM64_DECODE_UNALLOCATED;
            }
            else instruction = ARM64_INSN_SHL_VECTOR_IMMEDIATE;
        }

        if (instruction != ARM64_INSN_UNKNOWN)
        {
            uint8_t element_width = 8U << (31 - __builtin_clz(immh));
            uint8_t encoded_immediate = (immh << 3) | immb;

            decoded->operand_width = raw & (1U << 30) ? 128 : 64;
            if (decoded->operand_width == 64 && element_width == 64) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = instruction;
            decoded->element_width = element_width;
            decoded->immediate = instruction == ARM64_INSN_SHL_VECTOR_IMMEDIATE ? encoded_immediate - element_width : 2 * element_width - encoded_immediate;
            return ARM64_DECODE_OK;
        }
    }

    uint32_t type = (raw >> 22) & 0x3;
    if ((raw & 0xFF200C00U) == 0x1E200400U)
    {
        uint8_t width = arm64_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = (raw & (1U << 4)) ? ARM64_INSN_FCCMPE_SCALAR : ARM64_INSN_FCCMP_SCALAR;
        decoded->condition = (raw >> 12) & 0xF;
        decoded->immediate = raw & 0xF;
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF200C00U) == 0x1E200C00U)
    {
        uint8_t width = arm64_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INSN_FCSEL_SCALAR;
        decoded->condition = (raw >> 12) & 0xF;
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF20FC00U) == 0x1E202000U)
    {
        uint32_t zero = (raw >> 3) & 1;

        if (type == 2 || (raw & 0x7) || (zero && decoded->rm != 0)) return ARM64_DECODE_UNALLOCATED;
        if (raw & (1U << 4)) decoded->instruction = zero ? ARM64_INSN_FCMPE_ZERO_SCALAR : ARM64_INSN_FCMPE_REGISTER_SCALAR;
        else decoded->instruction = zero ? ARM64_INSN_FCMP_ZERO_SCALAR : ARM64_INSN_FCMP_REGISTER_SCALAR;
        decoded->operand_width = type == 3 ? 16 : type ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF207C00U) == 0x1E204000U && !arm64_simd_is_compare_zero_owner(raw) && !arm64_simd_is_convert_owner(raw))
    {
        uint32_t opcode = (raw >> 15) & 0x3F;
        uint8_t width = arm64_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        switch (opcode)
        {
        case 0:
            decoded->instruction = ARM64_INSN_FMOV_SCALAR;
            break;
        case 1:
            decoded->instruction = ARM64_INSN_FABS_SCALAR;
            break;
        case 2:
            decoded->instruction = ARM64_INSN_FNEG_SCALAR;
            break;
        case 3:
            decoded->instruction = ARM64_INSN_FSQRT_SCALAR;
            break;
        case 8:
            decoded->instruction = ARM64_INSN_FRINTN_SCALAR;
            break;
        case 9:
            decoded->instruction = ARM64_INSN_FRINTP_SCALAR;
            break;
        case 10:
            decoded->instruction = ARM64_INSN_FRINTM_SCALAR;
            break;
        case 11:
            decoded->instruction = ARM64_INSN_FRINTZ_SCALAR;
            break;
        case 12:
            decoded->instruction = ARM64_INSN_FRINTA_SCALAR;
            break;
        case 14:
            decoded->instruction = ARM64_INSN_FRINTX_SCALAR;
            break;
        case 15:
            decoded->instruction = ARM64_INSN_FRINTI_SCALAR;
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7FBEFC00U) == 0x1E260000U && !arm64_simd_is_convert_owner(raw))
    {
        uint32_t sf = (raw >> 31) & 1;

        if (((raw >> 22) & 1) != sf) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = (raw & (1U << 16)) ? ARM64_INSN_FMOV_GPR_TO_FP : ARM64_INSN_FMOV_FP_TO_GPR;
        decoded->operand_width = sf ? 64 : 32;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xBFE08400U) == 0x2E000000U && !arm64_simd_is_compare_zero_owner(raw))
    {
        decoded->instruction = ARM64_INSN_EXT_VECTOR;
        decoded->immediate = (raw >> 11) & 0xF;
        decoded->operand_width = (raw & (1U << 30)) ? 128 : 64;
        if (decoded->operand_width == 64 && decoded->immediate >= 8) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    if (!arm64_simd_is_compare_zero_owner(raw))
    {
        enum arm64_instruction instruction = ARM64_INSN_UNKNOWN;
        uint8_t immediate = 0;

        if ((raw & 0xBF20E400U) == 0x2E00C400U)
        {
            instruction = ARM64_INSN_FCMLA_VECTOR;
            immediate = (raw >> 11) & 0x3;
        }
        else if ((raw & 0xBF20EC00U) == 0x2E00E400U)
        {
            instruction = ARM64_INSN_FCADD_VECTOR;
            immediate = raw & (1U << 12) ? ARM64_SIMD_ROTATION_270 : ARM64_SIMD_ROTATION_90;
        }

        if (instruction != ARM64_INSN_UNKNOWN)
        {
            uint8_t q = (raw >> 30) & 1;
            uint8_t size = (raw >> 22) & 0x3;

            if (size == 0 || (!q && size == 3)) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = instruction;
            decoded->immediate = immediate;
            decoded->element_width = 8U << size;
            decoded->operand_width = q ? 128 : 64;
            return ARM64_DECODE_OK;
        }
    }

    if ((raw & 0xFFE0FC00U) == 0x5E000400U)
    {
        uint8_t imm5 = (raw >> 16) & 0x1F;

        if (!imm5) return ARM64_DECODE_UNALLOCATED;
        uint8_t size = (uint8_t)__builtin_ctz(imm5);
        if (size > 3) return ARM64_DECODE_UNALLOCATED;

        decoded->instruction = ARM64_INSN_DUP_ELEMENT_SCALAR;
        decoded->element_width = 8U << size;
        decoded->lane_index = imm5 >> (size + 1);
        decoded->operand_width = decoded->element_width;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xDF200400U) == 0x5E200400U && !arm64_simd_is_scalar_fp16_3same_owner(raw)) return arm64_decode_simd_scalar_3same(raw, decoded);

    if ((raw & 0xDFBFFC00U) == 0x5E30D800U && !arm64_simd_is_compare_zero_owner(raw))
    {
        if (!(raw & (1U << 29)))
        {
            if (raw & (1U << 22)) return ARM64_DECODE_UNALLOCATED;
            decoded->element_width = 16;
        }
        else
        {
            decoded->element_width = raw & (1U << 22) ? 64 : 32;
        }
        decoded->instruction = ARM64_INSN_FADDP_SCALAR_REDUCE;
        decoded->operand_width = decoded->element_width * 2;
        return ARM64_DECODE_OK;
    }

    if (arm64_simd_is_scalar_fp16_3same_owner(raw)) return arm64_decode_simd_scalar_fp16_3reg(raw, decoded);

    switch (raw & 0xFF208400U)
    {
    case 0x5E008400U:
        return ARM64_DECODE_UNALLOCATED;
    case 0x7E008400U:
    {
        uint8_t size = (raw >> 22) & 0x3;
        uint8_t opcode = (raw >> 11) & 0x1F;

        if (size == 0 || size == 3 || (opcode & 0x0E) != 0) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = raw & (1U << 11) ? ARM64_INSN_SQRDMLSH_SCALAR : ARM64_INSN_SQRDMLAH_SCALAR;
        decoded->element_width = 8U << size;
        decoded->operand_width = decoded->element_width;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    if ((raw & 0x40FE0000U) == 0x00E00000U && !arm64_simd_is_compare_zero_owner(raw)) return ARM64_DECODE_UNALLOCATED;

    return ARM64_DECODE_UNSUPPORTED;
}

enum arm64_decode_status arm64_decode_simd(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    enum arm64_decode_status status = arm64_decode_simd_impl(raw, decoded);

    if (status != ARM64_DECODE_OK)
    {
        decoded->instruction = ARM64_INSN_UNKNOWN;
        return status;
    }
    if (decoded->instruction == ARM64_INSN_UNKNOWN) return ARM64_DECODE_UNSUPPORTED;
    if (!arm64_normalize_simd_registers(decoded)) return ARM64_DECODE_UNSUPPORTED;
    return ARM64_DECODE_OK;
}
