#include "arm64_decode.h"

static inline uint8_t arm64_decode_simd_register_mask(enum arm64_instruction instruction)
{
    switch (instruction)
    {
    case ARM64_INST_MOVI_VECTOR_IMMEDIATE:
    case ARM64_INST_ORR_VECTOR_IMMEDIATE:
    case ARM64_INST_FMOV_VECTOR_IMMEDIATE:
    case ARM64_INST_FMOV_SCALAR_IMMEDIATE:
    case ARM64_INST_MVNI_VECTOR_IMMEDIATE:
    case ARM64_INST_BIC_VECTOR_IMMEDIATE:
        return 1;
    case ARM64_INST_DUP_ELEMENT_VECTOR:
    case ARM64_INST_DUP_GENERAL_VECTOR:
    case ARM64_INST_SMOV_VECTOR_TO_GPR:
    case ARM64_INST_UMOV_VECTOR_TO_GPR:
    case ARM64_INST_REV64_VECTOR:
    case ARM64_INST_REV16_VECTOR:
    case ARM64_INST_XTN_VECTOR:
    case ARM64_INST_SQXTN_VECTOR:
    case ARM64_INST_FCVTNS_SIMD_VECTOR:
    case ARM64_INST_FCVTMS_SIMD_VECTOR:
    case ARM64_INST_FCVTAS_SIMD_VECTOR:
    case ARM64_INST_SCVTF_SIMD_VECTOR:
    case ARM64_INST_SADDLV_VECTOR:
    case ARM64_INST_SMAXV_VECTOR:
    case ARM64_INST_FMAXNMV_SCALAR_REDUCE:
    case ARM64_INST_FMAXV_SCALAR_REDUCE:
    case ARM64_INST_SMINV_VECTOR:
    case ARM64_INST_ADDV_VECTOR:
    case ARM64_INST_FCMGT_ZERO_VECTOR:
    case ARM64_INST_FCMEQ_ZERO_VECTOR:
    case ARM64_INST_FCMLT_ZERO_VECTOR:
    case ARM64_INST_FABS_VECTOR:
    case ARM64_INST_FRINTM_VECTOR:
    case ARM64_INST_FCVTPS_SIMD_VECTOR:
    case ARM64_INST_FCVTZS_SIMD_VECTOR:
    case ARM64_INST_FMINNMV_SCALAR_REDUCE:
    case ARM64_INST_FMINV_SCALAR_REDUCE:
    case ARM64_INST_SSHR_VECTOR_IMMEDIATE:
    case ARM64_INST_SHL_VECTOR_IMMEDIATE:
    case ARM64_INST_FCVTNS_GPR:
    case ARM64_INST_FMOV_SCALAR:
    case ARM64_INST_FABS_SCALAR:
    case ARM64_INST_FCVTNU_GPR:
    case ARM64_INST_FNEG_SCALAR:
    case ARM64_INST_FSQRT_SCALAR:
    case ARM64_INST_SCVTF_S_W:
    case ARM64_INST_FCVT_D_S:
    case ARM64_INST_UCVTF_S_W:
    case ARM64_INST_FCVTAS_GPR:
    case ARM64_INST_FRINTN_SCALAR:
    case ARM64_INST_FRINTP_SCALAR:
    case ARM64_INST_FCVTAU_GPR:
    case ARM64_INST_FRINTM_SCALAR:
    case ARM64_INST_FRINTZ_SCALAR:
    case ARM64_INST_FMOV_FP_TO_GPR:
    case ARM64_INST_FRINTA_SCALAR:
    case ARM64_INST_FMOV_GPR_TO_FP:
    case ARM64_INST_FRINTX_SCALAR:
    case ARM64_INST_FRINTI_SCALAR:
    case ARM64_INST_FCVTPS_GPR:
    case ARM64_INST_FCVTPU_GPR:
    case ARM64_INST_FCVTMS_GPR:
    case ARM64_INST_FCVTMU_GPR:
    case ARM64_INST_FCVTZS_GPR:
    case ARM64_INST_FCVTZU_GPR:
    case ARM64_INST_SCVTF_D_W:
    case ARM64_INST_FCVT_S_D:
    case ARM64_INST_UCVTF_D_W:
    case ARM64_INST_REV32_VECTOR:
    case ARM64_INST_SQXTUN_VECTOR:
    case ARM64_INST_UQXTN_VECTOR:
    case ARM64_INST_FCVTNU_SIMD_VECTOR:
    case ARM64_INST_FCVTMU_SIMD_VECTOR:
    case ARM64_INST_FCVTAU_SIMD_VECTOR:
    case ARM64_INST_UCVTF_SIMD_VECTOR:
    case ARM64_INST_UADDLV_VECTOR:
    case ARM64_INST_UMAXV_VECTOR:
    case ARM64_INST_UMINV_VECTOR:
    case ARM64_INST_FCMGE_ZERO_VECTOR:
    case ARM64_INST_FCMLE_ZERO_VECTOR:
    case ARM64_INST_FNEG_VECTOR:
    case ARM64_INST_FCVTPU_SIMD_VECTOR:
    case ARM64_INST_FCVTZU_SIMD_VECTOR:
    case ARM64_INST_FSQRT_VECTOR:
    case ARM64_INST_USHR_VECTOR_IMMEDIATE:
    case ARM64_INST_INS_GPR_VECTOR:
    case ARM64_INST_XTN2_VECTOR:
    case ARM64_INST_SQXTN2_VECTOR:
    case ARM64_INST_DUP_ELEMENT_SCALAR:
    case ARM64_INST_SQXTN_SCALAR:
    case ARM64_INST_FCVTNS_SIMD_SCALAR:
    case ARM64_INST_FCVTMS_SIMD_SCALAR:
    case ARM64_INST_FCVTAS_SIMD_SCALAR:
    case ARM64_INST_SCVTF_SIMD_SCALAR:
    case ARM64_INST_FADDP_SCALAR_REDUCE:
    case ARM64_INST_FCMGT_ZERO_SCALAR:
    case ARM64_INST_FCMEQ_ZERO_SCALAR:
    case ARM64_INST_FCMLT_ZERO_SCALAR:
    case ARM64_INST_FCVTPS_SIMD_SCALAR:
    case ARM64_INST_FCVTZS_SIMD_SCALAR:
    case ARM64_INST_INS_ELEMENT_VECTOR:
    case ARM64_INST_SQXTUN2_VECTOR:
    case ARM64_INST_UQXTN2_VECTOR:
    case ARM64_INST_SQXTUN_SCALAR:
    case ARM64_INST_UQXTN_SCALAR:
    case ARM64_INST_FCVTNU_SIMD_SCALAR:
    case ARM64_INST_FCVTMU_SIMD_SCALAR:
    case ARM64_INST_FCVTAU_SIMD_SCALAR:
    case ARM64_INST_UCVTF_SIMD_SCALAR:
    case ARM64_INST_FCMGE_ZERO_SCALAR:
    case ARM64_INST_FCMLE_ZERO_SCALAR:
    case ARM64_INST_FCVTPU_SIMD_SCALAR:
    case ARM64_INST_FCVTZU_SIMD_SCALAR:
    case ARM64_INST_SCVTF_S_X:
    case ARM64_INST_UCVTF_S_X:
    case ARM64_INST_SCVTF_D_X:
    case ARM64_INST_UCVTF_D_X:
        return 3;
    case ARM64_INST_UZP1_VECTOR:
    case ARM64_INST_TRN1_VECTOR:
    case ARM64_INST_ZIP1_VECTOR:
    case ARM64_INST_UZP2_VECTOR:
    case ARM64_INST_TRN2_VECTOR:
    case ARM64_INST_ZIP2_VECTOR:
    case ARM64_INST_SHADD_VECTOR:
    case ARM64_INST_SQADD_VECTOR:
    case ARM64_INST_SRHADD_VECTOR:
    case ARM64_INST_AND_VECTOR:
    case ARM64_INST_SHSUB_VECTOR:
    case ARM64_INST_SQSUB_VECTOR:
    case ARM64_INST_CMGT_VECTOR:
    case ARM64_INST_CMGE_VECTOR:
    case ARM64_INST_SSHL_VECTOR:
    case ARM64_INST_SQSHL_VECTOR:
    case ARM64_INST_SRSHL_VECTOR:
    case ARM64_INST_SQRSHL_VECTOR:
    case ARM64_INST_SMAX_VECTOR:
    case ARM64_INST_SMIN_VECTOR:
    case ARM64_INST_SABD_VECTOR:
    case ARM64_INST_SABA_VECTOR:
    case ARM64_INST_ADD_VECTOR:
    case ARM64_INST_CMTST_VECTOR:
    case ARM64_INST_MLA_VECTOR:
    case ARM64_INST_MUL_VECTOR:
    case ARM64_INST_SMAXP_VECTOR:
    case ARM64_INST_SMINP_VECTOR:
    case ARM64_INST_ADDP_VECTOR:
    case ARM64_INST_FMAXNM_VECTOR:
    case ARM64_INST_FMLA_VECTOR:
    case ARM64_INST_FADD_VECTOR:
    case ARM64_INST_FMULX_VECTOR:
    case ARM64_INST_FCMEQ_VECTOR:
    case ARM64_INST_FMLAL_VECTOR:
    case ARM64_INST_FMAX_VECTOR:
    case ARM64_INST_FRECPS_VECTOR:
    case ARM64_INST_BIC_VECTOR:
    case ARM64_INST_SQDMULH_VECTOR:
    case ARM64_INST_SDOT_VECTOR:
    case ARM64_INST_USDOT_VECTOR:
    case ARM64_INST_ORR_VECTOR:
    case ARM64_INST_FMINNM_VECTOR:
    case ARM64_INST_FMLS_VECTOR:
    case ARM64_INST_FSUB_VECTOR:
    case ARM64_INST_FAMAX_VECTOR:
    case ARM64_INST_FMLSL_VECTOR:
    case ARM64_INST_FMIN_VECTOR:
    case ARM64_INST_FRSQRTS_VECTOR:
    case ARM64_INST_ORN_VECTOR:
    case ARM64_INST_FMLA_VECTOR_BY_ELEMENT:
    case ARM64_INST_FMLS_VECTOR_BY_ELEMENT:
    case ARM64_INST_FMUL_VECTOR_BY_ELEMENT:
    case ARM64_INST_SUDOT_VECTOR_BY_ELEMENT:
    case ARM64_INST_BFDOT_VECTOR_BY_ELEMENT:
    case ARM64_INST_FMLAL_VECTOR_BY_ELEMENT:
    case ARM64_INST_FMLSL_VECTOR_BY_ELEMENT:
    case ARM64_INST_SDOT_VECTOR_BY_ELEMENT:
    case ARM64_INST_USDOT_VECTOR_BY_ELEMENT:
    case ARM64_INST_BFMLALB_VECTOR_BY_ELEMENT:
    case ARM64_INST_FMUL_SCALAR:
    case ARM64_INST_FCSEL_SCALAR:
    case ARM64_INST_FDIV_SCALAR:
    case ARM64_INST_FADD_SCALAR:
    case ARM64_INST_FSUB_SCALAR:
    case ARM64_INST_FMAX_SCALAR:
    case ARM64_INST_FMIN_SCALAR:
    case ARM64_INST_FMAXNM_SCALAR:
    case ARM64_INST_FMINNM_SCALAR:
    case ARM64_INST_FNMUL_SCALAR:
    case ARM64_INST_EXT_VECTOR:
    case ARM64_INST_UHADD_VECTOR:
    case ARM64_INST_UQADD_VECTOR:
    case ARM64_INST_URHADD_VECTOR:
    case ARM64_INST_EOR_VECTOR:
    case ARM64_INST_UHSUB_VECTOR:
    case ARM64_INST_UQSUB_VECTOR:
    case ARM64_INST_CMHI_VECTOR:
    case ARM64_INST_CMHS_VECTOR:
    case ARM64_INST_USHL_VECTOR:
    case ARM64_INST_UQSHL_VECTOR:
    case ARM64_INST_URSHL_VECTOR:
    case ARM64_INST_UQRSHL_VECTOR:
    case ARM64_INST_UMAX_VECTOR:
    case ARM64_INST_UMIN_VECTOR:
    case ARM64_INST_UABD_VECTOR:
    case ARM64_INST_UABA_VECTOR:
    case ARM64_INST_SUB_VECTOR:
    case ARM64_INST_CMEQ_VECTOR:
    case ARM64_INST_MLS_VECTOR:
    case ARM64_INST_PMUL_VECTOR:
    case ARM64_INST_UMAXP_VECTOR:
    case ARM64_INST_UMINP_VECTOR:
    case ARM64_INST_FMAXNMP_VECTOR:
    case ARM64_INST_FMLAL2_VECTOR:
    case ARM64_INST_FADDP_VECTOR:
    case ARM64_INST_FMUL_VECTOR:
    case ARM64_INST_FCMGE_VECTOR:
    case ARM64_INST_FACGE_VECTOR:
    case ARM64_INST_FMAXP_VECTOR:
    case ARM64_INST_FDIV_VECTOR:
    case ARM64_INST_SQRDMLAH_VECTOR:
    case ARM64_INST_SQRDMLSH_VECTOR:
    case ARM64_INST_FCMLA_VECTOR:
    case ARM64_INST_FCADD_VECTOR:
    case ARM64_INST_BFDOT_VECTOR:
    case ARM64_INST_BSL_VECTOR:
    case ARM64_INST_SQRDMULH_VECTOR:
    case ARM64_INST_UDOT_VECTOR:
    case ARM64_INST_BIT_VECTOR:
    case ARM64_INST_FMINNMP_VECTOR:
    case ARM64_INST_FMLSL2_VECTOR:
    case ARM64_INST_FABD_VECTOR:
    case ARM64_INST_FAMIN_VECTOR:
    case ARM64_INST_FCMGT_VECTOR:
    case ARM64_INST_FACGT_VECTOR:
    case ARM64_INST_FMINP_VECTOR:
    case ARM64_INST_FSCALE_VECTOR:
    case ARM64_INST_BFMLALB_VECTOR:
    case ARM64_INST_BIF_VECTOR:
    case ARM64_INST_FMULX_VECTOR_BY_ELEMENT:
    case ARM64_INST_FCMLA_VECTOR_BY_ELEMENT:
    case ARM64_INST_SQRDMLAH_VECTOR_BY_ELEMENT:
    case ARM64_INST_SQRDMLSH_VECTOR_BY_ELEMENT:
    case ARM64_INST_FMLAL2_VECTOR_BY_ELEMENT:
    case ARM64_INST_FMLSL2_VECTOR_BY_ELEMENT:
    case ARM64_INST_UDOT_VECTOR_BY_ELEMENT:
    case ARM64_INST_SMMLA_VECTOR:
    case ARM64_INST_USMMLA_VECTOR:
    case ARM64_INST_BFMLALT_VECTOR_BY_ELEMENT:
    case ARM64_INST_SQADD_SCALAR:
    case ARM64_INST_SQSUB_SCALAR:
    case ARM64_INST_SQSHL_SCALAR:
    case ARM64_INST_SQRSHL_SCALAR:
    case ARM64_INST_FMULX_SCALAR:
    case ARM64_INST_FCMEQ_SCALAR:
    case ARM64_INST_FRECPS_SCALAR:
    case ARM64_INST_SQDMULH_SCALAR:
    case ARM64_INST_FRSQRTS_SCALAR:
    case ARM64_INST_CMGT_SCALAR:
    case ARM64_INST_CMGE_SCALAR:
    case ARM64_INST_SSHL_SCALAR:
    case ARM64_INST_SRSHL_SCALAR:
    case ARM64_INST_ADD_SCALAR:
    case ARM64_INST_CMTST_SCALAR:
    case ARM64_INST_FMLA_SCALAR_BY_ELEMENT:
    case ARM64_INST_FMLS_SCALAR_BY_ELEMENT:
    case ARM64_INST_FMUL_SCALAR_BY_ELEMENT:
    case ARM64_INST_BFMMLA_VECTOR:
    case ARM64_INST_UMMLA_VECTOR:
    case ARM64_INST_BFMLALT_VECTOR:
    case ARM64_INST_UQADD_SCALAR:
    case ARM64_INST_UQSUB_SCALAR:
    case ARM64_INST_UQSHL_SCALAR:
    case ARM64_INST_UQRSHL_SCALAR:
    case ARM64_INST_FCMGE_SCALAR:
    case ARM64_INST_FACGE_SCALAR:
    case ARM64_INST_SQRDMLAH_SCALAR:
    case ARM64_INST_SQRDMLSH_SCALAR:
    case ARM64_INST_SQRDMULH_SCALAR:
    case ARM64_INST_FABD_SCALAR:
    case ARM64_INST_FCMGT_SCALAR:
    case ARM64_INST_FACGT_SCALAR:
    case ARM64_INST_CMHI_SCALAR:
    case ARM64_INST_CMHS_SCALAR:
    case ARM64_INST_USHL_SCALAR:
    case ARM64_INST_URSHL_SCALAR:
    case ARM64_INST_SUB_SCALAR:
    case ARM64_INST_CMEQ_SCALAR:
    case ARM64_INST_FMULX_SCALAR_BY_ELEMENT:
    case ARM64_INST_SQRDMLAH_SCALAR_BY_ELEMENT:
    case ARM64_INST_SQRDMLSH_SCALAR_BY_ELEMENT:
        return 7;
    case ARM64_INST_FMADD_SCALAR:
    case ARM64_INST_FMSUB_SCALAR:
    case ARM64_INST_FNMADD_SCALAR:
    case ARM64_INST_FNMSUB_SCALAR:
        return 15;
    case ARM64_INST_FCMP_REGISTER_SCALAR:
    case ARM64_INST_FCMPE_REGISTER_SCALAR:
        return 6;
    case ARM64_INST_FCMP_ZERO_SCALAR:
    case ARM64_INST_FCMPE_ZERO_SCALAR:
        return 2;
    case ARM64_INST_FCCMP_SCALAR:
    case ARM64_INST_FCCMPE_SCALAR:
        return 6;
    default:
        return 0;
    }
}

static inline int arm64_decode_simd_normalize_registers(struct arm64_decoded_instruction *decoded)
{
    uint8_t register_mask = arm64_decode_simd_register_mask(decoded->instruction);

    if (!register_mask) return 0;

    if (!(register_mask & 1)) decoded->rd = 0;
    if (!(register_mask & 2)) decoded->rn = 0;
    if (!(register_mask & 4)) decoded->rm = 0;
    if (!(register_mask & 8)) decoded->ra = 0;
    return 1;
}
static inline uint8_t arm64_decode_simd_scalar_fp_width(uint32_t type)
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

static inline enum arm64_decode_status arm64_decode_simd_modified_imm(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint8_t encoded_immediate = (ARM64_DECODE_FIELD(raw, 18, 16) << 5) | ARM64_DECODE_FIELD(raw, 9, 5);
    uint8_t cmode = ARM64_DECODE_FIELD(raw, 15, 12);
    uint8_t op = ARM64_DECODE_BIT(raw, 29);
    decoded->simd_cmode = cmode;

    decoded->immediate = encoded_immediate;
    decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;

    switch ((op << 4) | cmode)
    {
    case 0x00:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x01:
        decoded->instruction = ARM64_INST_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x02:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x03:
        decoded->instruction = ARM64_INST_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x04:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x05:
        decoded->instruction = ARM64_INST_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x06:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x07:
        decoded->instruction = ARM64_INST_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x08:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        break;
    case 0x09:
        decoded->instruction = ARM64_INST_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        break;
    case 0x0A:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        break;
    case 0x0B:
        decoded->instruction = ARM64_INST_ORR_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        break;
    case 0x0C:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x0D:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x0E:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 8;
        break;
    case 0x0F:
        decoded->instruction = ARM64_INST_FMOV_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x10:
        decoded->instruction = ARM64_INST_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x11:
        decoded->instruction = ARM64_INST_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x12:
        decoded->instruction = ARM64_INST_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x13:
        decoded->instruction = ARM64_INST_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x14:
        decoded->instruction = ARM64_INST_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x15:
        decoded->instruction = ARM64_INST_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x16:
        decoded->instruction = ARM64_INST_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x17:
        decoded->instruction = ARM64_INST_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x18:
        decoded->instruction = ARM64_INST_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        break;
    case 0x19:
        decoded->instruction = ARM64_INST_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        break;
    case 0x1A:
        decoded->instruction = ARM64_INST_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        break;
    case 0x1B:
        decoded->instruction = ARM64_INST_BIC_VECTOR_IMMEDIATE;
        decoded->element_width = 16;
        break;
    case 0x1C:
        decoded->instruction = ARM64_INST_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x1D:
        decoded->instruction = ARM64_INST_MVNI_VECTOR_IMMEDIATE;
        decoded->element_width = 32;
        break;
    case 0x1E:
        decoded->instruction = ARM64_INST_MOVI_VECTOR_IMMEDIATE;
        decoded->element_width = 64;
        break;
    case 0x1F:
        if (decoded->operand_width != 128) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INST_FMOV_VECTOR_IMMEDIATE;
        decoded->element_width = 64;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_simd_vector_fp16_3reg(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    enum arm64_instruction instruction;

    switch ((ARM64_DECODE_BIT(raw, 29) << 6) | (ARM64_DECODE_BIT(raw, 23) << 5) | (ARM64_DECODE_FIELD(raw, 13, 11) + 24))
    {
    case 0x18:
        instruction = ARM64_INST_FMAXNM_VECTOR;
        break;
    case 0x19:
        instruction = ARM64_INST_FMLA_VECTOR;
        break;
    case 0x1A:
        instruction = ARM64_INST_FADD_VECTOR;
        break;
    case 0x1B:
        instruction = ARM64_INST_FMULX_VECTOR;
        break;
    case 0x1C:
        instruction = ARM64_INST_FCMEQ_VECTOR;
        break;
    case 0x1E:
        instruction = ARM64_INST_FMAX_VECTOR;
        break;
    case 0x1F:
        instruction = ARM64_INST_FRECPS_VECTOR;
        break;
    case 0x38:
        instruction = ARM64_INST_FMINNM_VECTOR;
        break;
    case 0x39:
        instruction = ARM64_INST_FMLS_VECTOR;
        break;
    case 0x3A:
        instruction = ARM64_INST_FSUB_VECTOR;
        break;
    case 0x3B:
        instruction = ARM64_INST_FAMAX_VECTOR;
        break;
    case 0x3E:
        instruction = ARM64_INST_FMIN_VECTOR;
        break;
    case 0x3F:
        instruction = ARM64_INST_FRSQRTS_VECTOR;
        break;
    case 0x58:
        instruction = ARM64_INST_FMAXNMP_VECTOR;
        break;
    case 0x5A:
        instruction = ARM64_INST_FADDP_VECTOR;
        break;
    case 0x5B:
        instruction = ARM64_INST_FMUL_VECTOR;
        break;
    case 0x5C:
        instruction = ARM64_INST_FCMGE_VECTOR;
        break;
    case 0x5D:
        instruction = ARM64_INST_FACGE_VECTOR;
        break;
    case 0x5E:
        instruction = ARM64_INST_FMAXP_VECTOR;
        break;
    case 0x5F:
        instruction = ARM64_INST_FDIV_VECTOR;
        break;
    case 0x78:
        instruction = ARM64_INST_FMINNMP_VECTOR;
        break;
    case 0x7A:
        instruction = ARM64_INST_FABD_VECTOR;
        break;
    case 0x7B:
        instruction = ARM64_INST_FAMIN_VECTOR;
        break;
    case 0x7C:
        instruction = ARM64_INST_FCMGT_VECTOR;
        break;
    case 0x7D:
        instruction = ARM64_INST_FACGT_VECTOR;
        break;
    case 0x7E:
        instruction = ARM64_INST_FMINP_VECTOR;
        break;
    case 0x7F:
        instruction = ARM64_INST_FSCALE_VECTOR;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    decoded->instruction = instruction;
    decoded->element_width = 16;
    decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_simd_vector_3same(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint8_t q = ARM64_DECODE_BIT(raw, 30);
    uint8_t u = ARM64_DECODE_BIT(raw, 29);
    uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);
    uint8_t opcode = ARM64_DECODE_FIELD(raw, 15, 11);
    enum arm64_instruction instruction = ARM64_INST_UNKNOWN;
    uint8_t valid_sizes = 0;

    if (opcode < 24)
    {
        if (opcode == 3)
        {
            switch (u)
            {
            case 0:
                switch (size)
                {
                case 0:
                    decoded->instruction = ARM64_INST_AND_VECTOR;
                    decoded->element_width = 8;
                    decoded->operand_width = q ? 128 : 64;
                    return ARM64_DECODE_OK;
                case 1:
                    decoded->instruction = ARM64_INST_BIC_VECTOR;
                    decoded->element_width = 8;
                    decoded->operand_width = q ? 128 : 64;
                    return ARM64_DECODE_OK;
                case 2:
                    decoded->instruction = ARM64_INST_ORR_VECTOR;
                    decoded->element_width = 8;
                    decoded->operand_width = q ? 128 : 64;
                    return ARM64_DECODE_OK;
                default:
                    decoded->instruction = ARM64_INST_ORN_VECTOR;
                    decoded->element_width = 8;
                    decoded->operand_width = q ? 128 : 64;
                    return ARM64_DECODE_OK;
                }
            default:
                switch (size)
                {
                case 0:
                    decoded->instruction = ARM64_INST_EOR_VECTOR;
                    decoded->element_width = 8;
                    decoded->operand_width = q ? 128 : 64;
                    return ARM64_DECODE_OK;
                case 1:
                    decoded->instruction = ARM64_INST_BSL_VECTOR;
                    decoded->element_width = 8;
                    decoded->operand_width = q ? 128 : 64;
                    return ARM64_DECODE_OK;
                case 2:
                    decoded->instruction = ARM64_INST_BIT_VECTOR;
                    decoded->element_width = 8;
                    decoded->operand_width = q ? 128 : 64;
                    return ARM64_DECODE_OK;
                default:
                    decoded->instruction = ARM64_INST_BIF_VECTOR;
                    decoded->element_width = 8;
                    decoded->operand_width = q ? 128 : 64;
                    return ARM64_DECODE_OK;
                }
            }
        }

        switch (u)
        {
        case 0:
            switch (opcode)
            {
            case 0:
                instruction = ARM64_INST_SHADD_VECTOR;
                valid_sizes = 0x7;
                break;
            case 1:
                instruction = ARM64_INST_SQADD_VECTOR;
                valid_sizes = 0xF;
                break;
            case 2:
                instruction = ARM64_INST_SRHADD_VECTOR;
                valid_sizes = 0x7;
                break;
            case 4:
                instruction = ARM64_INST_SHSUB_VECTOR;
                valid_sizes = 0x7;
                break;
            case 5:
                instruction = ARM64_INST_SQSUB_VECTOR;
                valid_sizes = 0xF;
                break;
            case 6:
                instruction = ARM64_INST_CMGT_VECTOR;
                valid_sizes = 0xF;
                break;
            case 7:
                instruction = ARM64_INST_CMGE_VECTOR;
                valid_sizes = 0xF;
                break;
            case 8:
                instruction = ARM64_INST_SSHL_VECTOR;
                valid_sizes = 0xF;
                break;
            case 9:
                instruction = ARM64_INST_SQSHL_VECTOR;
                valid_sizes = 0xF;
                break;
            case 10:
                instruction = ARM64_INST_SRSHL_VECTOR;
                valid_sizes = 0xF;
                break;
            case 11:
                instruction = ARM64_INST_SQRSHL_VECTOR;
                valid_sizes = 0xF;
                break;
            case 12:
                instruction = ARM64_INST_SMAX_VECTOR;
                valid_sizes = 0x7;
                break;
            case 13:
                instruction = ARM64_INST_SMIN_VECTOR;
                valid_sizes = 0x7;
                break;
            case 14:
                instruction = ARM64_INST_SABD_VECTOR;
                valid_sizes = 0x7;
                break;
            case 15:
                instruction = ARM64_INST_SABA_VECTOR;
                valid_sizes = 0x7;
                break;
            case 16:
                instruction = ARM64_INST_ADD_VECTOR;
                valid_sizes = 0xF;
                break;
            case 17:
                instruction = ARM64_INST_CMTST_VECTOR;
                valid_sizes = 0xF;
                break;
            case 18:
                instruction = ARM64_INST_MLA_VECTOR;
                valid_sizes = 0x7;
                break;
            case 19:
                instruction = ARM64_INST_MUL_VECTOR;
                valid_sizes = 0x7;
                break;
            case 20:
                instruction = ARM64_INST_SMAXP_VECTOR;
                valid_sizes = 0x7;
                break;
            case 21:
                instruction = ARM64_INST_SMINP_VECTOR;
                valid_sizes = 0x7;
                break;
            case 22:
                instruction = ARM64_INST_SQDMULH_VECTOR;
                valid_sizes = 0x6;
                break;
            case 23:
                instruction = ARM64_INST_ADDP_VECTOR;
                valid_sizes = 0xF;
                break;
            default:
                return ARM64_DECODE_UNALLOCATED;
            }
            break;
        default:
            switch (opcode)
            {
            case 0:
                instruction = ARM64_INST_UHADD_VECTOR;
                valid_sizes = 0x7;
                break;
            case 1:
                instruction = ARM64_INST_UQADD_VECTOR;
                valid_sizes = 0xF;
                break;
            case 2:
                instruction = ARM64_INST_URHADD_VECTOR;
                valid_sizes = 0x7;
                break;
            case 4:
                instruction = ARM64_INST_UHSUB_VECTOR;
                valid_sizes = 0x7;
                break;
            case 5:
                instruction = ARM64_INST_UQSUB_VECTOR;
                valid_sizes = 0xF;
                break;
            case 6:
                instruction = ARM64_INST_CMHI_VECTOR;
                valid_sizes = 0xF;
                break;
            case 7:
                instruction = ARM64_INST_CMHS_VECTOR;
                valid_sizes = 0xF;
                break;
            case 8:
                instruction = ARM64_INST_USHL_VECTOR;
                valid_sizes = 0xF;
                break;
            case 9:
                instruction = ARM64_INST_UQSHL_VECTOR;
                valid_sizes = 0xF;
                break;
            case 10:
                instruction = ARM64_INST_URSHL_VECTOR;
                valid_sizes = 0xF;
                break;
            case 11:
                instruction = ARM64_INST_UQRSHL_VECTOR;
                valid_sizes = 0xF;
                break;
            case 12:
                instruction = ARM64_INST_UMAX_VECTOR;
                valid_sizes = 0x7;
                break;
            case 13:
                instruction = ARM64_INST_UMIN_VECTOR;
                valid_sizes = 0x7;
                break;
            case 14:
                instruction = ARM64_INST_UABD_VECTOR;
                valid_sizes = 0x7;
                break;
            case 15:
                instruction = ARM64_INST_UABA_VECTOR;
                valid_sizes = 0x7;
                break;
            case 16:
                instruction = ARM64_INST_SUB_VECTOR;
                valid_sizes = 0xF;
                break;
            case 17:
                instruction = ARM64_INST_CMEQ_VECTOR;
                valid_sizes = 0xF;
                break;
            case 18:
                instruction = ARM64_INST_MLS_VECTOR;
                valid_sizes = 0x7;
                break;
            case 19:
                instruction = ARM64_INST_PMUL_VECTOR;
                valid_sizes = 0x1;
                break;
            case 20:
                instruction = ARM64_INST_UMAXP_VECTOR;
                valid_sizes = 0x7;
                break;
            case 21:
                instruction = ARM64_INST_UMINP_VECTOR;
                valid_sizes = 0x7;
                break;
            case 22:
                instruction = ARM64_INST_SQRDMULH_VECTOR;
                valid_sizes = 0x6;
                break;
            default:
                return ARM64_DECODE_UNALLOCATED;
            }
            break;
        }
        if (!(valid_sizes & (1U << size)) || (!q && size == 3)) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = instruction;
        decoded->element_width = 8U << size;
        decoded->operand_width = q ? 128 : 64;
        return ARM64_DECODE_OK;
    }

    switch (u)
    {
    case 0:
        switch (size >> 1)
        {
        case 0:
            switch (opcode)
            {
            case 24:
                instruction = ARM64_INST_FMAXNM_VECTOR;
                break;
            case 25:
                instruction = ARM64_INST_FMLA_VECTOR;
                break;
            case 26:
                instruction = ARM64_INST_FADD_VECTOR;
                break;
            case 27:
                instruction = ARM64_INST_FMULX_VECTOR;
                break;
            case 28:
                instruction = ARM64_INST_FCMEQ_VECTOR;
                break;
            case 29:
                instruction = ARM64_INST_FMLAL_VECTOR;
                break;
            case 30:
                instruction = ARM64_INST_FMAX_VECTOR;
                break;
            case 31:
                instruction = ARM64_INST_FRECPS_VECTOR;
                break;
            }
            break;
        default:
            switch (opcode)
            {
            case 24:
                instruction = ARM64_INST_FMINNM_VECTOR;
                break;
            case 25:
                instruction = ARM64_INST_FMLS_VECTOR;
                break;
            case 26:
                instruction = ARM64_INST_FSUB_VECTOR;
                break;
            case 27:
                instruction = ARM64_INST_FAMAX_VECTOR;
                break;
            case 29:
                instruction = ARM64_INST_FMLSL_VECTOR;
                break;
            case 30:
                instruction = ARM64_INST_FMIN_VECTOR;
                break;
            case 31:
                instruction = ARM64_INST_FRSQRTS_VECTOR;
                break;
            }
            break;
        }
        break;
    default:
        switch (size >> 1)
        {
        case 0:
            switch (opcode)
            {
            case 24:
                instruction = ARM64_INST_FMAXNMP_VECTOR;
                break;
            case 25:
                instruction = ARM64_INST_FMLAL2_VECTOR;
                break;
            case 26:
                instruction = ARM64_INST_FADDP_VECTOR;
                break;
            case 27:
                instruction = ARM64_INST_FMUL_VECTOR;
                break;
            case 28:
                instruction = ARM64_INST_FCMGE_VECTOR;
                break;
            case 29:
                instruction = ARM64_INST_FACGE_VECTOR;
                break;
            case 30:
                instruction = ARM64_INST_FMAXP_VECTOR;
                break;
            case 31:
                instruction = ARM64_INST_FDIV_VECTOR;
                break;
            }
            break;
        default:
            switch (opcode)
            {
            case 24:
                instruction = ARM64_INST_FMINNMP_VECTOR;
                break;
            case 25:
                instruction = ARM64_INST_FMLSL2_VECTOR;
                break;
            case 26:
                instruction = ARM64_INST_FABD_VECTOR;
                break;
            case 27:
                instruction = ARM64_INST_FAMIN_VECTOR;
                break;
            case 28:
                instruction = ARM64_INST_FCMGT_VECTOR;
                break;
            case 29:
                instruction = ARM64_INST_FACGT_VECTOR;
                break;
            case 30:
                instruction = ARM64_INST_FMINP_VECTOR;
                break;
            case 31:
                instruction = ARM64_INST_FSCALE_VECTOR;
                break;
            }
            break;
        }
        break;
    }
    if (instruction == ARM64_INST_UNKNOWN) return ARM64_DECODE_UNALLOCATED;
    if ((!u && opcode == 29) || (u && opcode == 25))
    {
        if (size & 1) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = instruction;
        decoded->element_width = 16;
        decoded->operand_width = q ? 128 : 64;
        return ARM64_DECODE_OK;
    }
    decoded->element_width = 32U << (size & 1);
    if (!q && decoded->element_width == 64) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_simd_scalar_fp16_3reg(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    enum arm64_instruction instruction;

    switch ((ARM64_DECODE_BIT(raw, 29) << 6) | (ARM64_DECODE_BIT(raw, 23) << 5) | (ARM64_DECODE_FIELD(raw, 13, 11) + 24))
    {
    case 0x1B:
        instruction = ARM64_INST_FMULX_SCALAR;
        break;
    case 0x1C:
        instruction = ARM64_INST_FCMEQ_SCALAR;
        break;
    case 0x1F:
        instruction = ARM64_INST_FRECPS_SCALAR;
        break;
    case 0x3F:
        instruction = ARM64_INST_FRSQRTS_SCALAR;
        break;
    case 0x5C:
        instruction = ARM64_INST_FCMGE_SCALAR;
        break;
    case 0x5D:
        instruction = ARM64_INST_FACGE_SCALAR;
        break;
    case 0x7A:
        instruction = ARM64_INST_FABD_SCALAR;
        break;
    case 0x7C:
        instruction = ARM64_INST_FCMGT_SCALAR;
        break;
    case 0x7D:
        instruction = ARM64_INST_FACGT_SCALAR;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    decoded->instruction = instruction;
    decoded->element_width = 16;
    decoded->operand_width = 16;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_simd_scalar_3same(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint8_t u = ARM64_DECODE_BIT(raw, 29);
    uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);
    uint8_t opcode = ARM64_DECODE_FIELD(raw, 15, 11);
    uint8_t size_high = size >> 1;
    enum arm64_instruction instruction = ARM64_INST_UNKNOWN;
    uint8_t valid_sizes = 0;

    if (opcode < 24)
    {
        switch (u)
        {
        case 0:
            switch (size_high)
            {
            case 0:
                switch (opcode)
                {
                case 1:
                    instruction = ARM64_INST_SQADD_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 5:
                    instruction = ARM64_INST_SQSUB_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 9:
                    instruction = ARM64_INST_SQSHL_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 11:
                    instruction = ARM64_INST_SQRSHL_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 22:
                    instruction = ARM64_INST_SQDMULH_SCALAR;
                    valid_sizes = 0x6;
                    break;
                default:
                    return ARM64_DECODE_UNALLOCATED;
                }
                break;
            default:
                switch (opcode)
                {
                case 1:
                    instruction = ARM64_INST_SQADD_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 5:
                    instruction = ARM64_INST_SQSUB_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 6:
                    instruction = ARM64_INST_CMGT_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 7:
                    instruction = ARM64_INST_CMGE_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 8:
                    instruction = ARM64_INST_SSHL_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 9:
                    instruction = ARM64_INST_SQSHL_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 10:
                    instruction = ARM64_INST_SRSHL_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 11:
                    instruction = ARM64_INST_SQRSHL_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 16:
                    instruction = ARM64_INST_ADD_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 17:
                    instruction = ARM64_INST_CMTST_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 22:
                    instruction = ARM64_INST_SQDMULH_SCALAR;
                    valid_sizes = 0x6;
                    break;
                default:
                    return ARM64_DECODE_UNALLOCATED;
                }
                break;
            }
            break;
        default:
            switch (size_high)
            {
            case 0:
                switch (opcode)
                {
                case 1:
                    instruction = ARM64_INST_UQADD_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 5:
                    instruction = ARM64_INST_UQSUB_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 9:
                    instruction = ARM64_INST_UQSHL_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 11:
                    instruction = ARM64_INST_UQRSHL_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 22:
                    instruction = ARM64_INST_SQRDMULH_SCALAR;
                    valid_sizes = 0x6;
                    break;
                default:
                    return ARM64_DECODE_UNALLOCATED;
                }
                break;
            default:
                switch (opcode)
                {
                case 1:
                    instruction = ARM64_INST_UQADD_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 5:
                    instruction = ARM64_INST_UQSUB_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 6:
                    instruction = ARM64_INST_CMHI_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 7:
                    instruction = ARM64_INST_CMHS_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 8:
                    instruction = ARM64_INST_USHL_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 9:
                    instruction = ARM64_INST_UQSHL_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 10:
                    instruction = ARM64_INST_URSHL_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 11:
                    instruction = ARM64_INST_UQRSHL_SCALAR;
                    valid_sizes = 0xF;
                    break;
                case 16:
                    instruction = ARM64_INST_SUB_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 17:
                    instruction = ARM64_INST_CMEQ_SCALAR;
                    valid_sizes = 0x8;
                    break;
                case 22:
                    instruction = ARM64_INST_SQRDMULH_SCALAR;
                    valid_sizes = 0x6;
                    break;
                default:
                    return ARM64_DECODE_UNALLOCATED;
                }
                break;
            }
            break;
        }
        if (!(valid_sizes & (1U << size))) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = instruction;
        decoded->element_width = 8U << size;
        decoded->operand_width = 8U << size;
        return ARM64_DECODE_OK;
    }

    switch (u)
    {
    case 0:
        switch (size_high)
        {
        case 0:
            switch (opcode)
            {
            case 27:
                instruction = ARM64_INST_FMULX_SCALAR;
                break;
            case 28:
                instruction = ARM64_INST_FCMEQ_SCALAR;
                break;
            case 31:
                instruction = ARM64_INST_FRECPS_SCALAR;
                break;
            }
            break;
        default:
            if (opcode == 31) instruction = ARM64_INST_FRSQRTS_SCALAR;
            break;
        }
        break;
    default:
        switch (size_high)
        {
        case 0:
            switch (opcode)
            {
            case 28:
                instruction = ARM64_INST_FCMGE_SCALAR;
                break;
            case 29:
                instruction = ARM64_INST_FACGE_SCALAR;
                break;
            }
            break;
        default:
            switch (opcode)
            {
            case 26:
                instruction = ARM64_INST_FABD_SCALAR;
                break;
            case 28:
                instruction = ARM64_INST_FCMGT_SCALAR;
                break;
            case 29:
                instruction = ARM64_INST_FACGT_SCALAR;
                break;
            }
            break;
        }
        break;
    }
    if (instruction == ARM64_INST_UNKNOWN) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->element_width = 32U << (size & 1);
    decoded->operand_width = 32U << (size & 1);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_simd_rdm_by_element(uint32_t raw, struct arm64_decoded_instruction *decoded, enum arm64_instruction instruction, uint8_t operand_width, uint8_t size)
{
    uint8_t h = ARM64_DECODE_BIT(raw, 11);
    uint8_t l = ARM64_DECODE_BIT(raw, 21);
    uint8_t m = ARM64_DECODE_BIT(raw, 20);

    if (size != 1 && size != 2) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = 8U << size;
    decoded->operand_width = operand_width;
    if (size == 1)
    {
        decoded->rm = ARM64_DECODE_FIELD(raw, 19, 16);
        decoded->lane_index = (h << 2) | (l << 1) | m;
    }
    else decoded->lane_index = (h << 1) | l;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_simd_dot_by_element(uint32_t raw, struct arm64_decoded_instruction *decoded, enum arm64_instruction instruction, uint8_t q, uint8_t size, uint8_t element_width)
{
    if (size != 2) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = element_width;
    decoded->lane_index = (ARM64_DECODE_BIT(raw, 11) << 1) | ARM64_DECODE_BIT(raw, 21);
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_simd_fcmla_by_element(uint32_t raw, struct arm64_decoded_instruction *decoded, uint8_t q, uint8_t size)
{
    uint8_t h = ARM64_DECODE_BIT(raw, 11);
    uint8_t l = ARM64_DECODE_BIT(raw, 21);

    if (size == 0 || size == 3) return ARM64_DECODE_UNALLOCATED;
    if (size == 2 && (l || !q)) return ARM64_DECODE_UNALLOCATED;
    if (size == 1 && h && !q) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = ARM64_INST_FCMLA_VECTOR_BY_ELEMENT;
    decoded->immediate = ARM64_DECODE_FIELD(raw, 14, 13);
    decoded->element_width = 8U << size;
    decoded->lane_index = size == 1 ? (h << 1) | l : h;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_simd_fhm_by_element(uint32_t raw, struct arm64_decoded_instruction *decoded, enum arm64_instruction instruction, uint8_t q, uint8_t size)
{
    if (size != 2) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = 16;
    decoded->lane_index = (ARM64_DECODE_BIT(raw, 11) << 2) | (ARM64_DECODE_BIT(raw, 21) << 1) | ARM64_DECODE_BIT(raw, 20);
    decoded->operand_width = q ? 128 : 64;
    decoded->rm = ARM64_DECODE_FIELD(raw, 19, 16);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_simd_fp_by_element(uint32_t raw, struct arm64_decoded_instruction *decoded, enum arm64_instruction instruction, uint8_t operand_width, uint8_t size)
{
    uint8_t h = ARM64_DECODE_BIT(raw, 11);
    uint8_t l = ARM64_DECODE_BIT(raw, 21);

    if (size == 1) return ARM64_DECODE_UNALLOCATED;
    if (size == 3 && l) return ARM64_DECODE_UNALLOCATED;

    decoded->instruction = instruction;
    decoded->element_width = size == 0 ? 16 : 8U << size;
    decoded->operand_width = operand_width;
    if (size == 0)
    {
        decoded->rm = ARM64_DECODE_FIELD(raw, 19, 16);
        decoded->lane_index = (h << 2) | (l << 1) | ARM64_DECODE_BIT(raw, 20);
    }
    else decoded->lane_index = size == 2 ? (h << 1) | l : h;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_scalar_fp_basic(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint32_t type = ARM64_DECODE_FIELD(raw, 23, 22);
    uint8_t width;

    switch (raw & 0xFF000000U)
    {
    case 0x1F000000U:
        width = arm64_decode_simd_scalar_fp_width(type);
        if (!width) return ARM64_DECODE_UNALLOCATED;
        switch ((ARM64_DECODE_BIT(raw, 21) << 1) | ARM64_DECODE_BIT(raw, 15))
        {
        case 0:
            decoded->instruction = ARM64_INST_FMADD_SCALAR;
            break;
        case 1:
            decoded->instruction = ARM64_INST_FMSUB_SCALAR;
            break;
        case 2:
            decoded->instruction = ARM64_INST_FNMADD_SCALAR;
            break;
        default:
            decoded->instruction = ARM64_INST_FNMSUB_SCALAR;
            break;
        }
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    default:
        break;
    }

    switch (raw & 0xFF200C00U)
    {
    case 0x1E200800U:
    {
        int conversion_owner = 0;

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
            conversion_owner = 1;
            break;
        default:
            switch (raw & 0x8FBFFC00U)
            {
            case 0x0E21A800U:
            case 0x0E21B800U:
            case 0x0E21C800U:
            case 0x0E21D800U:
            case 0x0EA1A800U:
            case 0x0EA1B800U:
                conversion_owner = 1;
                break;
            default:
                break;
            }
            break;
        }

        if (conversion_owner) break;
        width = arm64_decode_simd_scalar_fp_width(type);
        if (!width) return ARM64_DECODE_UNALLOCATED;
        switch (ARM64_DECODE_FIELD(raw, 15, 12))
        {
        case 0:
            decoded->instruction = ARM64_INST_FMUL_SCALAR;
            break;
        case 1:
            decoded->instruction = ARM64_INST_FDIV_SCALAR;
            break;
        case 2:
            decoded->instruction = ARM64_INST_FADD_SCALAR;
            break;
        case 3:
            decoded->instruction = ARM64_INST_FSUB_SCALAR;
            break;
        case 4:
            decoded->instruction = ARM64_INST_FMAX_SCALAR;
            break;
        case 5:
            decoded->instruction = ARM64_INST_FMIN_SCALAR;
            break;
        case 6:
            decoded->instruction = ARM64_INST_FMAXNM_SCALAR;
            break;
        case 7:
            decoded->instruction = ARM64_INST_FMINNM_SCALAR;
            break;
        case 8:
            decoded->instruction = ARM64_INST_FNMUL_SCALAR;
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    switch (raw & 0xFF201FE0U)
    {
    case 0x1E201000U:
        width = arm64_decode_simd_scalar_fp_width(type);
        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INST_FMOV_SCALAR_IMMEDIATE;
        decoded->operand_width = width;
        decoded->element_width = width;
        decoded->immediate = ARM64_DECODE_FIELD(raw, 20, 13);
        return ARM64_DECODE_OK;
    default:
        return ARM64_DECODE_OK;
    }
}

static enum arm64_decode_status arm64_decode_simd_vector_permute(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint8_t opcode = ARM64_DECODE_FIELD(raw, 14, 12);
    uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);
    uint8_t q = ARM64_DECODE_BIT(raw, 30);

    switch (opcode)
    {
    case 1:
        decoded->instruction = ARM64_INST_UZP1_VECTOR;
        break;
    case 2:
        decoded->instruction = ARM64_INST_TRN1_VECTOR;
        break;
    case 3:
        decoded->instruction = ARM64_INST_ZIP1_VECTOR;
        break;
    case 5:
        decoded->instruction = ARM64_INST_UZP2_VECTOR;
        break;
    case 6:
        decoded->instruction = ARM64_INST_TRN2_VECTOR;
        break;
    case 7:
        decoded->instruction = ARM64_INST_ZIP2_VECTOR;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    if (!q && size == 3) return ARM64_DECODE_UNALLOCATED;
    decoded->operand_width = q ? 128 : 64;
    decoded->element_width = 8U << size;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_reduce(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint8_t q = ARM64_DECODE_BIT(raw, 30);
    uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);
    uint8_t valid_sizes = 0;
    enum arm64_instruction instruction = ARM64_INST_UNKNOWN;

    switch (raw & 0xBF3FFC00U)
    {
    case 0x0E200800U:
        instruction = ARM64_INST_REV64_VECTOR;
        valid_sizes = 0x7;
        break;
    case 0x0E201800U:
        instruction = ARM64_INST_REV16_VECTOR;
        valid_sizes = 0x1;
        break;
    case 0x0E303800U:
        instruction = ARM64_INST_SADDLV_VECTOR;
        valid_sizes = q ? 0x7 : 0x3;
        break;
    case 0x0E30A800U:
        instruction = ARM64_INST_SMAXV_VECTOR;
        valid_sizes = q ? 0x7 : 0x3;
        break;
    case 0x0E31A800U:
        instruction = ARM64_INST_SMINV_VECTOR;
        valid_sizes = q ? 0x7 : 0x3;
        break;
    case 0x0E31B800U:
        instruction = ARM64_INST_ADDV_VECTOR;
        valid_sizes = q ? 0x7 : 0x3;
        break;
    case 0x2E200800U:
        instruction = ARM64_INST_REV32_VECTOR;
        valid_sizes = 0x3;
        break;
    case 0x2E303800U:
        instruction = ARM64_INST_UADDLV_VECTOR;
        valid_sizes = q ? 0x7 : 0x3;
        break;
    case 0x2E30A800U:
        instruction = ARM64_INST_UMAXV_VECTOR;
        valid_sizes = q ? 0x7 : 0x3;
        break;
    case 0x2E31A800U:
        instruction = ARM64_INST_UMINV_VECTOR;
        valid_sizes = q ? 0x7 : 0x3;
        break;
    default:
        return ARM64_DECODE_OK;
    }

    if (!(valid_sizes & (1U << size))) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->operand_width = q ? 128 : 64;
    decoded->element_width = 8U << size;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_unary(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint32_t shape = raw & 0x00FE0000U;
    uint32_t signature = raw & 0xBF01FC00U;
    uint8_t q = ARM64_DECODE_BIT(raw, 30);
    uint8_t element_width;

    switch (shape)
    {
    case 0x00200000U:
        element_width = 32;
        break;
    case 0x00A00000U:
        element_width = 32;
        break;
    case 0x00E00000U:
        if (!q) return ARM64_DECODE_UNALLOCATED;
        element_width = 64;
        break;
    case 0x00F80000U:
        element_width = 16;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    switch (signature)
    {
    case 0x0E019800U:
        if (element_width != 32 || q) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INST_FRINTM_VECTOR;
        break;
    case 0x0E00F800U:
        decoded->instruction = ARM64_INST_FABS_VECTOR;
        break;
    case 0x2E00F800U:
        decoded->instruction = ARM64_INST_FNEG_VECTOR;
        break;
    case 0x2E01F800U:
        decoded->instruction = ARM64_INST_FSQRT_VECTOR;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }

    decoded->element_width = element_width;
    decoded->operand_width = q ? 128 : 64;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_vector_dot_family(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    switch (raw & 0x9F208400U)
    {
    case 0x0E008400U:
        switch (raw & 0xBF20E400U)
        {
        case 0x2E00C400U:
            return ARM64_DECODE_OK;
        default:
            break;
        }
        switch (raw & 0xBF20EC00U)
        {
        case 0x2E00E400U:
            return ARM64_DECODE_OK;
        default:
            break;
        }
        {
            uint8_t q = ARM64_DECODE_BIT(raw, 30);
            uint8_t u = ARM64_DECODE_BIT(raw, 29);
            uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);
            uint8_t opcode = ARM64_DECODE_FIELD(raw, 15, 11);
            enum arm64_instruction instruction = ARM64_INST_UNKNOWN;

            decoded->operand_width = q ? 128 : 64;
            switch (u)
            {
            case 0:
                switch (size)
                {
                case 0:
                case 1:
                case 3:
                    switch (opcode & 0xF)
                    {
                    case 2:
                    case 3:
                    case 4:
                    case 5:
                        return ARM64_DECODE_UNALLOCATED;
                    default:
                        break;
                    }
                    break;
                case 2:
                    switch (opcode & 0xF)
                    {
                    case 2:
                        instruction = ARM64_INST_SDOT_VECTOR;
                        decoded->element_width = 8;
                        break;
                    case 3:
                        instruction = ARM64_INST_USDOT_VECTOR;
                        decoded->element_width = 8;
                        break;
                    case 4:
                        if (!q) return ARM64_DECODE_UNALLOCATED;
                        instruction = ARM64_INST_SMMLA_VECTOR;
                        decoded->element_width = 8;
                        break;
                    case 5:
                        if (!q) return ARM64_DECODE_UNALLOCATED;
                        instruction = ARM64_INST_USMMLA_VECTOR;
                        decoded->element_width = 8;
                        break;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
                break;
            default:
                switch (size)
                {
                case 0:
                    switch (opcode & 0xF)
                    {
                    case 0:
                    case 1:
                    case 2:
                    case 4:
                    case 13:
                    case 15:
                        return ARM64_DECODE_UNALLOCATED;
                    default:
                        break;
                    }
                    break;
                case 1:
                    switch (opcode & 0xF)
                    {
                    case 0:
                        instruction = ARM64_INST_SQRDMLAH_VECTOR;
                        decoded->element_width = 8U << size;
                        break;
                    case 1:
                        instruction = ARM64_INST_SQRDMLSH_VECTOR;
                        decoded->element_width = 8U << size;
                        break;
                    case 2:
                    case 4:
                        return ARM64_DECODE_UNALLOCATED;
                    case 13:
                        if (!q) return ARM64_DECODE_UNALLOCATED;
                        instruction = ARM64_INST_BFMMLA_VECTOR;
                        decoded->element_width = 16;
                        break;
                    case 15:
                        instruction = ARM64_INST_BFDOT_VECTOR;
                        decoded->element_width = 16;
                        break;
                    default:
                        break;
                    }
                    break;
                case 2:
                    switch (opcode & 0xF)
                    {
                    case 0:
                        instruction = ARM64_INST_SQRDMLAH_VECTOR;
                        decoded->element_width = 8U << size;
                        break;
                    case 1:
                        instruction = ARM64_INST_SQRDMLSH_VECTOR;
                        decoded->element_width = 8U << size;
                        break;
                    case 2:
                        instruction = ARM64_INST_UDOT_VECTOR;
                        decoded->element_width = 8;
                        break;
                    case 4:
                        if (!q) return ARM64_DECODE_UNALLOCATED;
                        instruction = ARM64_INST_UMMLA_VECTOR;
                        decoded->element_width = 8;
                        break;
                    case 13:
                    case 15:
                        return ARM64_DECODE_UNALLOCATED;
                    default:
                        break;
                    }
                    break;
                case 3:
                    switch (opcode & 0xF)
                    {
                    case 0:
                    case 1:
                    case 2:
                    case 4:
                    case 13:
                        return ARM64_DECODE_UNALLOCATED;
                    case 15:
                        switch (q)
                        {
                        case 0:
                            instruction = ARM64_INST_BFMLALB_VECTOR;
                            break;
                        default:
                            instruction = ARM64_INST_BFMLALT_VECTOR;
                            break;
                        }
                        decoded->operand_width = 128;
                        decoded->element_width = 16;
                        break;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
                break;
            }

            if (instruction != ARM64_INST_UNKNOWN)
            {
                decoded->instruction = instruction;
                return ARM64_DECODE_OK;
            }
            break;
        }
    default:
        break;
    }
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_simd_conversion(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    switch (raw & 0xFFFFFC00U)
    {
    case 0x1E220000U:
        decoded->instruction = ARM64_INST_SCVTF_S_W;
        decoded->operand_width = 32;
        decoded->element_width = 32;
        break;
    case 0x1E22C000U:
        decoded->instruction = ARM64_INST_FCVT_D_S;
        decoded->operand_width = 64;
        decoded->element_width = 32;
        break;
    case 0x1E230000U:
        decoded->instruction = ARM64_INST_UCVTF_S_W;
        decoded->operand_width = 32;
        decoded->element_width = 32;
        break;
    case 0x1E620000U:
        decoded->instruction = ARM64_INST_SCVTF_D_W;
        decoded->operand_width = 64;
        decoded->element_width = 32;
        break;
    case 0x1E624000U:
        decoded->instruction = ARM64_INST_FCVT_S_D;
        decoded->operand_width = 32;
        decoded->element_width = 64;
        break;
    case 0x1E630000U:
        decoded->instruction = ARM64_INST_UCVTF_D_W;
        decoded->operand_width = 64;
        decoded->element_width = 32;
        break;
    case 0x9E220000U:
        decoded->instruction = ARM64_INST_SCVTF_S_X;
        decoded->operand_width = 32;
        decoded->element_width = 64;
        break;
    case 0x9E230000U:
        decoded->instruction = ARM64_INST_UCVTF_S_X;
        decoded->operand_width = 32;
        decoded->element_width = 64;
        break;
    case 0x9E620000U:
        decoded->instruction = ARM64_INST_SCVTF_D_X;
        decoded->operand_width = 64;
        decoded->element_width = 64;
        break;
    case 0x9E630000U:
        decoded->instruction = ARM64_INST_UCVTF_D_X;
        decoded->operand_width = 64;
        decoded->element_width = 64;
        break;
    default:
        break;
    }

    if (decoded->instruction != ARM64_INST_UNKNOWN)
    {
        return ARM64_DECODE_OK;
    }

    switch (raw & 0x8FBFFC00U)
    {
    case 0x0E21A800U:
        switch (ARM64_DECODE_BIT(raw, 29))
        {
        case 0:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTNS_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTNS_SIMD_SCALAR;
                break;
            }
            break;
        default:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTNU_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTNU_SIMD_SCALAR;
                break;
            }
            break;
        }
        break;
    case 0x0E21B800U:
        switch (ARM64_DECODE_BIT(raw, 29))
        {
        case 0:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTMS_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTMS_SIMD_SCALAR;
                break;
            }
            break;
        default:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTMU_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTMU_SIMD_SCALAR;
                break;
            }
            break;
        }
        break;
    case 0x0E21C800U:
        switch (ARM64_DECODE_BIT(raw, 29))
        {
        case 0:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTAS_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTAS_SIMD_SCALAR;
                break;
            }
            break;
        default:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTAU_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTAU_SIMD_SCALAR;
                break;
            }
            break;
        }
        break;
    case 0x0E21D800U:
        switch (ARM64_DECODE_BIT(raw, 29))
        {
        case 0:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_SCVTF_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_SCVTF_SIMD_SCALAR;
                break;
            }
            break;
        default:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_UCVTF_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_UCVTF_SIMD_SCALAR;
                break;
            }
            break;
        }
        break;
    case 0x0EA1A800U:
        switch (ARM64_DECODE_BIT(raw, 29))
        {
        case 0:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTPS_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTPS_SIMD_SCALAR;
                break;
            }
            break;
        default:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTPU_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTPU_SIMD_SCALAR;
                break;
            }
            break;
        }
        break;
    case 0x0EA1B800U:
        switch (ARM64_DECODE_BIT(raw, 29))
        {
        case 0:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTZS_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTZS_SIMD_SCALAR;
                break;
            }
            break;
        default:
            switch (ARM64_DECODE_BIT(raw, 28))
            {
            case 0:
                decoded->instruction = ARM64_INST_FCVTZU_SIMD_VECTOR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCVTZU_SIMD_SCALAR;
                break;
            }
            break;
        }
        break;
    default:
        break;
    }

    if (decoded->instruction != ARM64_INST_UNKNOWN)
    {
        decoded->element_width = ARM64_DECODE_BIT(raw, 22) ? 64 : 32;
        if (ARM64_DECODE_BIT(raw, 28))
        {
            if (!ARM64_DECODE_BIT(raw, 30)) return ARM64_DECODE_UNALLOCATED;
            decoded->operand_width = decoded->element_width;
        }
        else
        {
            decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;
            if (decoded->operand_width == 64 && decoded->element_width == 64) return ARM64_DECODE_UNALLOCATED;
        }
        return ARM64_DECODE_OK;
    }

    switch (raw & 0x7F20FC00U)
    {
    case 0x1E200000U:
        if (ARM64_DECODE_FIELD(raw, 23, 22) > 1) return ARM64_DECODE_OK;
        {
            uint32_t selector = (ARM64_DECODE_FIELD(raw, 20, 19) << 3) | ARM64_DECODE_FIELD(raw, 18, 16);

            switch (selector)
            {
            case 0x00:
                decoded->instruction = ARM64_INST_FCVTNS_GPR;
                break;
            case 0x01:
                decoded->instruction = ARM64_INST_FCVTNU_GPR;
                break;
            case 0x04:
                decoded->instruction = ARM64_INST_FCVTAS_GPR;
                break;
            case 0x05:
                decoded->instruction = ARM64_INST_FCVTAU_GPR;
                break;
            case 0x08:
                decoded->instruction = ARM64_INST_FCVTPS_GPR;
                break;
            case 0x09:
                decoded->instruction = ARM64_INST_FCVTPU_GPR;
                break;
            case 0x10:
                decoded->instruction = ARM64_INST_FCVTMS_GPR;
                break;
            case 0x11:
                decoded->instruction = ARM64_INST_FCVTMU_GPR;
                break;
            case 0x18:
                decoded->instruction = ARM64_INST_FCVTZS_GPR;
                break;
            case 0x19:
                decoded->instruction = ARM64_INST_FCVTZU_GPR;
                break;
            default:
                return ARM64_DECODE_OK;
            }

            decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
            decoded->element_width = ARM64_DECODE_BIT(raw, 22) ? 64 : 32;
            return ARM64_DECODE_OK;
        }
    default:
        return ARM64_DECODE_OK;
    }
}

/*
所有 FP/AdvSIMD 编码签名只在本文件内匹配；成功分支直接选择具体 instruction，
随后填写该 instruction 对应的真实操作数。
*/
static inline enum arm64_decode_status arm64_decode_simd_impl(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    decoded->instruction_class = ARM64_INSTRUCTION_CLASS_DATA_PROCESSING_SIMD_FP;
    decoded->instruction = ARM64_INST_UNKNOWN;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->ra = ARM64_DECODE_FIELD(raw, 14, 10);
    decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);

    switch (raw & 0x9FF80C00U)
    {
    case 0x0F000400U:
        return arm64_decode_simd_modified_imm(raw, decoded);
    default:
        break;
    }

    {
        enum arm64_decode_status status = arm64_decode_simd_scalar_fp_basic(raw, decoded);

        if (status != ARM64_DECODE_OK || decoded->instruction != ARM64_INST_UNKNOWN) return status;
    }

    switch (raw & 0xBF208C00U)
    {
    case 0x0E000800U:
        return arm64_decode_simd_vector_permute(raw, decoded);
    default:
        break;
    }

    switch (raw & 0x9FE08400U)
    {
    case 0x0E000400U:
    {
        uint8_t imm5 = ARM64_DECODE_FIELD(raw, 20, 16);
        uint8_t imm4 = ARM64_DECODE_FIELD(raw, 14, 11);
        uint8_t q = ARM64_DECODE_BIT(raw, 30);

        if (!imm5) return ARM64_DECODE_UNALLOCATED;
        uint8_t size = (uint8_t)__builtin_ctz(imm5);
        if (size > 3) return ARM64_DECODE_UNALLOCATED;

        decoded->element_width = 8U << size;
        if (!q && decoded->element_width == 64) return ARM64_DECODE_UNALLOCATED;
        decoded->lane_index = imm5 >> (size + 1);

        if (ARM64_DECODE_BIT(raw, 29))
        {
            if (!q) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INST_INS_ELEMENT_VECTOR;
            decoded->immediate = imm4 >> size;
            decoded->operand_width = 128;
            return ARM64_DECODE_OK;
        }

        switch (imm4)
        {
        case 0:
            decoded->instruction = ARM64_INST_DUP_ELEMENT_VECTOR;
            decoded->operand_width = q ? 128 : 64;
            break;
        case 1:
            decoded->instruction = ARM64_INST_DUP_GENERAL_VECTOR;
            decoded->operand_width = q ? 128 : 64;
            break;
        case 3:
            if (!q) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INST_INS_GPR_VECTOR;
            decoded->operand_width = 128;
            break;
        case 5:
            if ((!q && decoded->element_width > 16) || (q && decoded->element_width > 32)) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INST_SMOV_VECTOR_TO_GPR;
            decoded->operand_width = q ? 64 : 32;
            break;
        case 7:
            if ((!q && decoded->element_width > 32) || (q && decoded->element_width != 64)) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INST_UMOV_VECTOR_TO_GPR;
            decoded->operand_width = q ? 64 : 32;
            break;
        default:
            return ARM64_DECODE_UNALLOCATED;
        }

        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    switch (raw & 0x9F200400U)
    {
    case 0x0E200400U:
        switch (raw & 0x9F60C400U)
        {
        case 0x0E400400U:
            break;
        default:
            return arm64_decode_simd_vector_3same(raw, decoded);
        }
        break;
    default:
        break;
    }

    switch (raw & 0x8F000C00U)
    {
    case 0x0E000800U:
    {
        uint32_t shape = raw & 0x00FF0000U;
        uint32_t relation = raw & 0x2000F000U;
        uint8_t scalar = ARM64_DECODE_BIT(raw, 28);
        uint8_t q = ARM64_DECODE_BIT(raw, 30);
        uint8_t element_width = 0;
        enum arm64_instruction instruction = ARM64_INST_UNKNOWN;

        switch (shape)
        {
        case 0x00F80000U:
            element_width = 16;
            break;
        case 0x00A00000U:
            element_width = 32;
            break;
        case 0x00E00000U:
            element_width = 64;
            break;
        default:
            break;
        }

        switch (relation)
        {
        case 0x0000C000U:
            switch (scalar)
            {
            case 0:
                instruction = ARM64_INST_FCMGT_ZERO_VECTOR;
                break;
            default:
                instruction = ARM64_INST_FCMGT_ZERO_SCALAR;
                break;
            }
            break;
        case 0x0000D000U:
            switch (scalar)
            {
            case 0:
                instruction = ARM64_INST_FCMEQ_ZERO_VECTOR;
                break;
            default:
                instruction = ARM64_INST_FCMEQ_ZERO_SCALAR;
                break;
            }
            break;
        case 0x0000E000U:
            switch (scalar)
            {
            case 0:
                instruction = ARM64_INST_FCMLT_ZERO_VECTOR;
                break;
            default:
                instruction = ARM64_INST_FCMLT_ZERO_SCALAR;
                break;
            }
            break;
        case 0x2000C000U:
            switch (scalar)
            {
            case 0:
                instruction = ARM64_INST_FCMGE_ZERO_VECTOR;
                break;
            default:
                instruction = ARM64_INST_FCMGE_ZERO_SCALAR;
                break;
            }
            break;
        case 0x2000D000U:
            switch (scalar)
            {
            case 0:
                instruction = ARM64_INST_FCMLE_ZERO_VECTOR;
                break;
            default:
                instruction = ARM64_INST_FCMLE_ZERO_SCALAR;
                break;
            }
            break;
        default:
            break;
        }

        if (element_width && instruction != ARM64_INST_UNKNOWN && (!scalar || q))
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
        break;
    }
    default:
        break;
    }

    {
        enum arm64_decode_status status = arm64_decode_simd_vector_reduce(raw, decoded);

        if (status != ARM64_DECODE_OK || decoded->instruction != ARM64_INST_UNKNOWN) return status;
    }

    {
        enum arm64_decode_status status = arm64_decode_simd_conversion(raw, decoded);

        if (status != ARM64_DECODE_OK || decoded->instruction != ARM64_INST_UNKNOWN) return status;
    }

    {
        uint32_t size = ARM64_DECODE_FIELD(raw, 23, 22);

        switch (raw & 0xFF3FFC00U)
        {
        case 0x0E212800U:
            decoded->instruction = ARM64_INST_XTN_VECTOR;
            decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;
            break;
        case 0x0E214800U:
            decoded->instruction = ARM64_INST_SQXTN_VECTOR;
            decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;
            break;
        case 0x2E212800U:
            decoded->instruction = ARM64_INST_SQXTUN_VECTOR;
            decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;
            break;
        case 0x2E214800U:
            decoded->instruction = ARM64_INST_UQXTN_VECTOR;
            decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;
            break;
        case 0x4E212800U:
            decoded->instruction = ARM64_INST_XTN2_VECTOR;
            decoded->operand_width = 128;
            break;
        case 0x4E214800U:
            decoded->instruction = ARM64_INST_SQXTN2_VECTOR;
            decoded->operand_width = 128;
            break;
        case 0x5E212800U:
            return ARM64_DECODE_UNALLOCATED;
        case 0x5E214800U:
            decoded->instruction = ARM64_INST_SQXTN_SCALAR;
            decoded->operand_width = 8U << size;
            break;
        case 0x6E212800U:
            decoded->instruction = ARM64_INST_SQXTUN2_VECTOR;
            decoded->operand_width = 128;
            break;
        case 0x6E214800U:
            decoded->instruction = ARM64_INST_UQXTN2_VECTOR;
            decoded->operand_width = 128;
            break;
        case 0x7E212800U:
            decoded->instruction = ARM64_INST_SQXTUN_SCALAR;
            decoded->operand_width = 8U << size;
            break;
        case 0x7E214800U:
            decoded->instruction = ARM64_INST_UQXTN_SCALAR;
            decoded->operand_width = 8U << size;
            break;
        default:
            break;
        }

        if (decoded->instruction != ARM64_INST_UNKNOWN)
        {
            if (size == 3) return ARM64_DECODE_UNALLOCATED;
            decoded->element_width = 16U << size;
            return ARM64_DECODE_OK;
        }
    }

    {
        enum arm64_instruction instruction = ARM64_INST_UNKNOWN;

        switch (raw & 0x9FFFFC00U)
        {
        case 0x0E30C800U:
            instruction = ARM64_INST_FMAXNMV_SCALAR_REDUCE;
            break;
        case 0x0E30F800U:
            instruction = ARM64_INST_FMAXV_SCALAR_REDUCE;
            break;
        case 0x0EB0C800U:
            instruction = ARM64_INST_FMINNMV_SCALAR_REDUCE;
            break;
        case 0x0EB0F800U:
            instruction = ARM64_INST_FMINV_SCALAR_REDUCE;
            break;
        default:
            break;
        }

        if (instruction != ARM64_INST_UNKNOWN)
        {
            if (ARM64_DECODE_BIT(raw, 29))
            {
                if (!ARM64_DECODE_BIT(raw, 30)) return ARM64_DECODE_UNALLOCATED;
                decoded->element_width = 32;
                decoded->operand_width = 128;
            }
            else
            {
                decoded->element_width = 16;
                decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;
            }
            decoded->instruction = instruction;
            return ARM64_DECODE_OK;
        }
    }

    switch (raw & 0x9F60C400U)
    {
    case 0x0E400400U:
        return arm64_decode_simd_vector_fp16_3reg(raw, decoded);
    default:
        break;
    }

    {
        enum arm64_decode_status status = arm64_decode_simd_vector_dot_family(raw, decoded);

        if (status != ARM64_DECODE_OK || decoded->instruction != ARM64_INST_UNKNOWN) return status;
    }

    switch (raw & 0x00FE0000U)
    {
    case 0x00200000U:
    case 0x00F80000U:
    case 0x00A00000U:
    case 0x00E00000U:
        switch (raw & 0xBF01FC00U)
        {
        case 0x0E019800U:
        case 0x0E00F800U:
        case 0x2E00F800U:
        case 0x2E01F800U:
            return arm64_decode_simd_vector_unary(raw, decoded);
        default:
            break;
        }
        break;
    default:
        break;
    }

    switch (raw & 0xDF000400U)
    {
    case 0x0F000000U:
    case 0x4F000000U:
    case 0x5F000000U:
    {
        uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);
        uint8_t opcode = ARM64_DECODE_FIELD(raw, 15, 12);
        uint8_t scalar = ARM64_DECODE_BIT(raw, 28);
        uint8_t q = ARM64_DECODE_BIT(raw, 30);
        uint8_t u = ARM64_DECODE_BIT(raw, 29);

        switch (scalar)
        {
        case 0:
            switch (u)
            {
            case 0:
                switch (size)
                {
                case 0:
                    switch (opcode)
                    {
                    case 1:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLA_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 5:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLS_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 9:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMUL_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 14:
                        return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_SDOT_VECTOR_BY_ELEMENT, q, size, 8);
                    case 15:
                        return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_SUDOT_VECTOR_BY_ELEMENT, q, 2, 8);
                    default:
                        break;
                    }
                    break;
                case 1:
                    switch (opcode)
                    {
                    case 1:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLA_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 5:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLS_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 9:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMUL_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 14:
                        return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_SDOT_VECTOR_BY_ELEMENT, q, size, 8);
                    case 15:
                        return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_BFDOT_VECTOR_BY_ELEMENT, q, 2, 16);
                    default:
                        break;
                    }
                    break;
                case 2:
                case 3:
                    switch (opcode)
                    {
                    case 0:
                        return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INST_FMLAL_VECTOR_BY_ELEMENT, q, size);
                    case 1:
                        if (size == 3 && !q) return ARM64_DECODE_UNALLOCATED;
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLA_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 4:
                        return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INST_FMLSL_VECTOR_BY_ELEMENT, q, size);
                    case 5:
                        if (size == 3 && !q) return ARM64_DECODE_UNALLOCATED;
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLS_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 9:
                        if (size == 3 && !q) return ARM64_DECODE_UNALLOCATED;
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMUL_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 14:
                        return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_SDOT_VECTOR_BY_ELEMENT, q, size, 8);
                    case 15:
                        if (size == 2) return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_USDOT_VECTOR_BY_ELEMENT, q, 2, 8);
                        switch (q)
                        {
                        case 0:
                            decoded->instruction = ARM64_INST_BFMLALB_VECTOR_BY_ELEMENT;
                            break;
                        default:
                            decoded->instruction = ARM64_INST_BFMLALT_VECTOR_BY_ELEMENT;
                            break;
                        }
                        decoded->element_width = 16;
                        decoded->lane_index = (ARM64_DECODE_BIT(raw, 11) << 2) | (ARM64_DECODE_BIT(raw, 21) << 1) | ARM64_DECODE_BIT(raw, 20);
                        decoded->operand_width = 128;
                        decoded->rm = ARM64_DECODE_FIELD(raw, 19, 16);
                        return ARM64_DECODE_OK;
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
                break;
            default:
                switch (size)
                {
                case 0:
                    switch (opcode)
                    {
                    case 1:
                    case 3:
                    case 5:
                    case 7:
                        return arm64_decode_simd_fcmla_by_element(raw, decoded, q, size);
                    case 9:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMULX_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 13:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLAH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 14:
                        return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_UDOT_VECTOR_BY_ELEMENT, q, size, 8);
                    case 15:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLSH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    default:
                        break;
                    }
                    break;
                case 1:
                    switch (opcode)
                    {
                    case 1:
                    case 3:
                    case 5:
                    case 7:
                        return arm64_decode_simd_fcmla_by_element(raw, decoded, q, size);
                    case 9:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMULX_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 13:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLAH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 14:
                        return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_UDOT_VECTOR_BY_ELEMENT, q, size, 8);
                    case 15:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLSH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    default:
                        break;
                    }
                    break;
                case 2:
                    switch (opcode)
                    {
                    case 1:
                    case 3:
                    case 5:
                    case 7:
                        return arm64_decode_simd_fcmla_by_element(raw, decoded, q, size);
                    case 8:
                        return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INST_FMLAL2_VECTOR_BY_ELEMENT, q, size);
                    case 9:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMULX_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 12:
                        return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INST_FMLSL2_VECTOR_BY_ELEMENT, q, size);
                    case 13:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLAH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 14:
                        return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_UDOT_VECTOR_BY_ELEMENT, q, size, 8);
                    case 15:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLSH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    default:
                        break;
                    }
                    break;
                case 3:
                    switch (opcode)
                    {
                    case 1:
                    case 3:
                    case 5:
                    case 7:
                        return arm64_decode_simd_fcmla_by_element(raw, decoded, q, size);
                    case 8:
                        return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INST_FMLAL2_VECTOR_BY_ELEMENT, q, size);
                    case 9:
                        if (!q) return ARM64_DECODE_UNALLOCATED;
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMULX_VECTOR_BY_ELEMENT, 128, size);
                    case 12:
                        return arm64_decode_simd_fhm_by_element(raw, decoded, ARM64_INST_FMLSL2_VECTOR_BY_ELEMENT, q, size);
                    case 13:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLAH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    case 14:
                        return arm64_decode_simd_dot_by_element(raw, decoded, ARM64_INST_UDOT_VECTOR_BY_ELEMENT, q, size, 8);
                    case 15:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLSH_VECTOR_BY_ELEMENT, q ? 128 : 64, size);
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
                break;
            }
            break;
        case 1:
            switch (u)
            {
            case 0:
                switch (size)
                {
                case 0:
                case 1:
                    switch (opcode)
                    {
                    case 1:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLA_SCALAR_BY_ELEMENT, 16, size);
                    case 5:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLS_SCALAR_BY_ELEMENT, 16, size);
                    case 9:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMUL_SCALAR_BY_ELEMENT, 16, size);
                    default:
                        break;
                    }
                    break;
                case 2:
                case 3:
                    switch (opcode)
                    {
                    case 1:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLA_SCALAR_BY_ELEMENT, size == 2 ? 32 : 64, size);
                    case 5:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMLS_SCALAR_BY_ELEMENT, size == 2 ? 32 : 64, size);
                    case 9:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMUL_SCALAR_BY_ELEMENT, size == 2 ? 32 : 64, size);
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
                break;
            default:
                switch (size)
                {
                case 0:
                case 1:
                    switch (opcode)
                    {
                    case 9:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMULX_SCALAR_BY_ELEMENT, 16, size);
                    case 13:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLAH_SCALAR_BY_ELEMENT, 8U << size, size);
                    case 15:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLSH_SCALAR_BY_ELEMENT, 8U << size, size);
                    default:
                        break;
                    }
                    break;
                case 2:
                case 3:
                    switch (opcode)
                    {
                    case 8:
                    case 12:
                        return ARM64_DECODE_UNALLOCATED;
                    case 9:
                        return arm64_decode_simd_fp_by_element(raw, decoded, ARM64_INST_FMULX_SCALAR_BY_ELEMENT, size == 2 ? 32 : 64, size);
                    case 13:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLAH_SCALAR_BY_ELEMENT, 8U << size, size);
                    case 15:
                        return arm64_decode_simd_rdm_by_element(raw, decoded, ARM64_INST_SQRDMLSH_SCALAR_BY_ELEMENT, 8U << size, size);
                    default:
                        break;
                    }
                    break;
                default:
                    break;
                }
                break;
            }
            break;
        default:
            break;
        }
    }
    break;
    default:
        break;
    }

    switch (raw & 0x9F800000U)
    {
    case 0x0F000000U:
    {
        uint8_t immh = ARM64_DECODE_FIELD(raw, 22, 19);
        uint8_t immb = ARM64_DECODE_FIELD(raw, 18, 16);
        uint8_t opcode = ARM64_DECODE_FIELD(raw, 15, 11);
        enum arm64_instruction instruction = ARM64_INST_UNKNOWN;

        if (!immh) break;
        switch (raw & 0x9FF80C00U)
        {
        case 0x0F000400U:
            break;
        default:
        {
            int by_element_shift_owner = 0;
            uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);
            uint8_t by_element_opcode = ARM64_DECODE_FIELD(raw, 15, 12);

            switch (raw & 0x9F800400U)
            {
            case 0x0F000000U:
                switch (ARM64_DECODE_BIT(raw, 29))
                {
                case 0:
                    switch (by_element_opcode)
                    {
                    case 1:
                    case 5:
                    case 9:
                    case 14:
                    case 15:
                        by_element_shift_owner = 1;
                        break;
                    case 0:
                    case 4:
                        by_element_shift_owner = size >= 2;
                        break;
                    default:
                        break;
                    }
                    break;
                default:
                    switch (by_element_opcode)
                    {
                    case 1:
                    case 3:
                    case 5:
                    case 7:
                    case 9:
                    case 13:
                    case 14:
                    case 15:
                        by_element_shift_owner = 1;
                        break;
                    case 8:
                    case 12:
                        by_element_shift_owner = size >= 2;
                        break;
                    default:
                        break;
                    }
                    break;
                }
                break;
            default:
                break;
            }

            if (!by_element_shift_owner)
            {
                switch (opcode)
                {
                case 0:
                    if (ARM64_DECODE_BIT(raw, 10))
                    {
                        switch (ARM64_DECODE_BIT(raw, 29))
                        {
                        case 0:
                            instruction = ARM64_INST_SSHR_VECTOR_IMMEDIATE;
                            break;
                        default:
                            instruction = ARM64_INST_USHR_VECTOR_IMMEDIATE;
                            break;
                        }
                    }
                    break;
                case 10:
                    if (!ARM64_DECODE_BIT(raw, 29))
                    {
                        if (!ARM64_DECODE_BIT(raw, 10))
                        {
                            if (ARM64_DECODE_FIELD(raw, 23, 22) == 1) return ARM64_DECODE_UNALLOCATED;
                        }
                        else instruction = ARM64_INST_SHL_VECTOR_IMMEDIATE;
                    }
                    break;
                default:
                    break;
                }
            }
        }
        break;
        }

        if (instruction != ARM64_INST_UNKNOWN)
        {
            uint8_t element_width = 8U << ARM64_DECODE_HIGHEST_SET_BIT(immh);
            uint8_t encoded_immediate = (immh << 3) | immb;

            decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;
            if (decoded->operand_width == 64 && element_width == 64) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = instruction;
            decoded->element_width = element_width;
            decoded->immediate = instruction == ARM64_INST_SHL_VECTOR_IMMEDIATE ? encoded_immediate - element_width : 2 * element_width - encoded_immediate;
            return ARM64_DECODE_OK;
        }
        break;
    }
    default:
        break;
    }

    uint32_t type = ARM64_DECODE_FIELD(raw, 23, 22);
    switch (raw & 0xFF200C00U)
    {
    case 0x1E200400U:
    {
        uint8_t width = arm64_decode_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        switch (ARM64_DECODE_BIT(raw, 4))
        {
        case 0:
            decoded->instruction = ARM64_INST_FCCMP_SCALAR;
            break;
        default:
            decoded->instruction = ARM64_INST_FCCMPE_SCALAR;
            break;
        }
        decoded->condition = ARM64_DECODE_FIELD(raw, 15, 12);
        decoded->immediate = ARM64_DECODE_FIELD(raw, 3, 0);
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    }

    case 0x1E200C00U:
    {
        uint8_t width = arm64_decode_simd_scalar_fp_width(type);

        if (!width) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INST_FCSEL_SCALAR;
        decoded->condition = ARM64_DECODE_FIELD(raw, 15, 12);
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    }

    default:
        break;
    }

    switch (raw & 0xFF20FC00U)
    {
    case 0x1E202000U:
    {
        uint32_t zero = ARM64_DECODE_BIT(raw, 3);

        if (type == 2 || ARM64_DECODE_FIELD(raw, 2, 0) || (zero && decoded->rm != 0)) return ARM64_DECODE_UNALLOCATED;
        switch (ARM64_DECODE_BIT(raw, 4))
        {
        case 0:
            switch (zero)
            {
            case 0:
                decoded->instruction = ARM64_INST_FCMP_REGISTER_SCALAR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCMP_ZERO_SCALAR;
                break;
            }
            break;
        default:
            switch (zero)
            {
            case 0:
                decoded->instruction = ARM64_INST_FCMPE_REGISTER_SCALAR;
                break;
            default:
                decoded->instruction = ARM64_INST_FCMPE_ZERO_SCALAR;
                break;
            }
            break;
        }
        decoded->element_width = type == 3 ? 16 : type ? 64 : 32;
        decoded->operand_width = type == 3 ? 16 : type ? 64 : 32;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    switch (raw & 0xFF207C00U)
    {
    case 0x1E204000U:
    {
        int conversion_owner = 0;
        uint32_t opcode = ARM64_DECODE_FIELD(raw, 20, 15);
        uint8_t width = arm64_decode_simd_scalar_fp_width(type);

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
            conversion_owner = 1;
            break;
        default:
            switch (raw & 0x8FBFFC00U)
            {
            case 0x0E21A800U:
            case 0x0E21B800U:
            case 0x0E21C800U:
            case 0x0E21D800U:
            case 0x0EA1A800U:
            case 0x0EA1B800U:
                conversion_owner = 1;
                break;
            default:
                break;
            }
            break;
        }
        if (conversion_owner) break;

        if (!width) return ARM64_DECODE_UNALLOCATED;
        switch (opcode)
        {
        case 0:
            decoded->instruction = ARM64_INST_FMOV_SCALAR;
            break;
        case 1:
            decoded->instruction = ARM64_INST_FABS_SCALAR;
            break;
        case 2:
            decoded->instruction = ARM64_INST_FNEG_SCALAR;
            break;
        case 3:
            decoded->instruction = ARM64_INST_FSQRT_SCALAR;
            break;
        case 8:
            decoded->instruction = ARM64_INST_FRINTN_SCALAR;
            break;
        case 9:
            decoded->instruction = ARM64_INST_FRINTP_SCALAR;
            break;
        case 10:
            decoded->instruction = ARM64_INST_FRINTM_SCALAR;
            break;
        case 11:
            decoded->instruction = ARM64_INST_FRINTZ_SCALAR;
            break;
        case 12:
            decoded->instruction = ARM64_INST_FRINTA_SCALAR;
            break;
        case 14:
            decoded->instruction = ARM64_INST_FRINTX_SCALAR;
            break;
        case 15:
            decoded->instruction = ARM64_INST_FRINTI_SCALAR;
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        decoded->operand_width = width;
        decoded->element_width = width;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    switch (raw & 0x7FBEFC00U)
    {
    case 0x1E260000U:
    {
        int conversion_owner = 0;
        uint32_t sf = ARM64_DECODE_BIT(raw, 31);

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
            conversion_owner = 1;
            break;
        default:
            switch (raw & 0x8FBFFC00U)
            {
            case 0x0E21A800U:
            case 0x0E21B800U:
            case 0x0E21C800U:
            case 0x0E21D800U:
            case 0x0EA1A800U:
            case 0x0EA1B800U:
                conversion_owner = 1;
                break;
            default:
                break;
            }
            break;
        }
        if (conversion_owner) break;

        if (ARM64_DECODE_BIT(raw, 22) != sf) return ARM64_DECODE_UNALLOCATED;
        switch (ARM64_DECODE_BIT(raw, 16))
        {
        case 0:
            decoded->instruction = ARM64_INST_FMOV_FP_TO_GPR;
            break;
        default:
            decoded->instruction = ARM64_INST_FMOV_GPR_TO_FP;
            break;
        }
        decoded->element_width = sf ? 64 : 32;
        decoded->operand_width = sf ? 64 : 32;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    switch (raw & 0xBFE08400U)
    {
    case 0x2E000000U:
        decoded->instruction = ARM64_INST_EXT_VECTOR;
        decoded->immediate = ARM64_DECODE_FIELD(raw, 14, 11);
        decoded->operand_width = ARM64_DECODE_BIT(raw, 30) ? 128 : 64;
        if (decoded->operand_width == 64 && decoded->immediate >= 8) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    default:
        break;
    }

    switch (raw & 0xBF20E400U)
    {
    case 0x2E00C400U:
    {
        uint8_t q = ARM64_DECODE_BIT(raw, 30);
        uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);

        if (size == 0 || (!q && size == 3)) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INST_FCMLA_VECTOR;
        decoded->immediate = ARM64_DECODE_FIELD(raw, 12, 11);
        decoded->element_width = 8U << size;
        decoded->operand_width = q ? 128 : 64;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    switch (raw & 0xBF20EC00U)
    {
    case 0x2E00E400U:
    {
        uint8_t q = ARM64_DECODE_BIT(raw, 30);
        uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);

        if (size == 0 || (!q && size == 3)) return ARM64_DECODE_UNALLOCATED;
        decoded->instruction = ARM64_INST_FCADD_VECTOR;
        decoded->immediate = ARM64_DECODE_BIT(raw, 12) ? ARM64_SIMD_ROTATION_270 : ARM64_SIMD_ROTATION_90;
        decoded->element_width = 8U << size;
        decoded->operand_width = q ? 128 : 64;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    switch (raw & 0xFFE0FC00U)
    {
    case 0x5E000400U:
    {
        uint8_t imm5 = ARM64_DECODE_FIELD(raw, 20, 16);

        if (!imm5) return ARM64_DECODE_UNALLOCATED;
        uint8_t size = (uint8_t)__builtin_ctz(imm5);
        if (size > 3) return ARM64_DECODE_UNALLOCATED;

        decoded->instruction = ARM64_INST_DUP_ELEMENT_SCALAR;
        decoded->element_width = 8U << size;
        decoded->lane_index = imm5 >> (size + 1);
        decoded->operand_width = decoded->element_width;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    switch (raw & 0xDF200400U)
    {
    case 0x5E200400U:
        switch (raw & 0xDF60C400U)
        {
        case 0x5E400400U:
            break;
        default:
            return arm64_decode_simd_scalar_3same(raw, decoded);
        }
        break;
    default:
        break;
    }

    switch (raw & 0xDFBFFC00U)
    {
    case 0x5E30D800U:
        if (!ARM64_DECODE_BIT(raw, 29))
        {
            if (ARM64_DECODE_BIT(raw, 22)) return ARM64_DECODE_UNALLOCATED;
            decoded->element_width = 16;
        }
        else
        {
            decoded->element_width = ARM64_DECODE_BIT(raw, 22) ? 64 : 32;
        }
        decoded->instruction = ARM64_INST_FADDP_SCALAR_REDUCE;
        decoded->operand_width = decoded->element_width * 2;
        return ARM64_DECODE_OK;
    default:
        break;
    }

    switch (raw & 0xDF60C400U)
    {
    case 0x5E400400U:
        return arm64_decode_simd_scalar_fp16_3reg(raw, decoded);
    default:
        break;
    }

    switch (raw & 0xFF208400U)
    {
    case 0x5E008400U:
        return ARM64_DECODE_UNALLOCATED;
    case 0x7E008400U:
    {
        uint8_t size = ARM64_DECODE_FIELD(raw, 23, 22);
        uint8_t opcode = ARM64_DECODE_FIELD(raw, 15, 11);

        if (size == 0 || size == 3 || (opcode & 0x0E) != 0) return ARM64_DECODE_UNALLOCATED;
        switch (ARM64_DECODE_BIT(raw, 11))
        {
        case 0:
            decoded->instruction = ARM64_INST_SQRDMLAH_SCALAR;
            break;
        default:
            decoded->instruction = ARM64_INST_SQRDMLSH_SCALAR;
            break;
        }
        decoded->element_width = 8U << size;
        decoded->operand_width = decoded->element_width;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    switch (raw & 0x40FE0000U)
    {
    case 0x00E00000U:
        return ARM64_DECODE_UNALLOCATED;
    default:
        break;
    }

    return ARM64_DECODE_UNSUPPORTED;
}

enum arm64_decode_status arm64_decode_simd_fp(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    enum arm64_decode_status status = arm64_decode_simd_impl(raw, decoded);

    if (status != ARM64_DECODE_OK)
    {
        decoded->instruction = ARM64_INST_UNKNOWN;
        return status;
    }
    if (decoded->instruction == ARM64_INST_UNKNOWN) return ARM64_DECODE_UNSUPPORTED;
    if (!arm64_decode_simd_normalize_registers(decoded)) return ARM64_DECODE_UNSUPPORTED;
    return ARM64_DECODE_OK;
}
