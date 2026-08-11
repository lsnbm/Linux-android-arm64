#include "../arm64_decode/arm64_decode.h"

#include <stddef.h>
#include <stdio.h>

static int failures;

#define CHECK(EXPRESSION)                                        \
    do                                                           \
    {                                                            \
        if (!(EXPRESSION))                                       \
        {                                                        \
            fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #EXPRESSION); \
            failures++;                                          \
        }                                                        \
    } while (0)

static struct arm64_decoded_insn decode_ok(uint32_t raw)
{
    struct arm64_decoded_insn decoded;

    enum arm64_decode_status status = arm64_decode_insn(raw, &decoded);

    CHECK(status == ARM64_DECODE_OK);
    return decoded;
}

static void decode_status_is(uint32_t raw, enum arm64_decode_status expected)
{
    struct arm64_decoded_insn decoded;

    enum arm64_decode_status status = arm64_decode_insn(raw, &decoded);

    CHECK(status == expected);
}

static void test_fp_simd_instruction_identities(void)
{
    // 补齐其他专项测试未直接引用的 FP/SIMD executor 身份。
    static const struct
    {
        uint32_t raw;
        enum arm64_instruction instruction;
    } samples[] = {
        {0x0E010C00U, ARM64_INSN_DUP_GENERAL_VECTOR},
        {0x2E000000U, ARM64_INSN_EXT_VECTOR},
        {0x1E200400U, ARM64_INSN_FCCMP_SCALAR},
        {0x1E202000U, ARM64_INSN_FCMP_REGISTER_SCALAR},
        {0x1E202008U, ARM64_INSN_FCMP_ZERO_SCALAR},
        {0x1E260000U, ARM64_INSN_FMOV_FP_TO_GPR},
        {0x1E270000U, ARM64_INSN_FMOV_GPR_TO_FP},
        {0x1E214000U, ARM64_INSN_FNEG_SCALAR},
        {0x1E264000U, ARM64_INSN_FRINTA_SCALAR},
        {0x1E27C000U, ARM64_INSN_FRINTI_SCALAR},
        {0x1E254000U, ARM64_INSN_FRINTM_SCALAR},
        {0x1E24C000U, ARM64_INSN_FRINTP_SCALAR},
        {0x1E274000U, ARM64_INSN_FRINTX_SCALAR},
        {0x1E25C000U, ARM64_INSN_FRINTZ_SCALAR},
        {0x1E21C000U, ARM64_INSN_FSQRT_SCALAR},
        {0x4E011C00U, ARM64_INSN_INS_GPR_VECTOR},
        {0x1E620000U, ARM64_INSN_SCVTF_D_W},
        {0x9E620000U, ARM64_INSN_SCVTF_D_X},
        {0x1E220000U, ARM64_INSN_SCVTF_S_W},
        {0x9E220000U, ARM64_INSN_SCVTF_S_X},
        {0x0F080400U, ARM64_INSN_SSHR_VECTOR_IMMEDIATE},
        {0x1E630000U, ARM64_INSN_UCVTF_D_W},
        {0x9E630000U, ARM64_INSN_UCVTF_D_X},
        {0x1E230000U, ARM64_INSN_UCVTF_S_W},
        {0x9E230000U, ARM64_INSN_UCVTF_S_X},
        {0x0E013C00U, ARM64_INSN_UMOV_VECTOR_TO_GPR},
    };

    for (size_t index = 0; index < sizeof(samples) / sizeof(samples[0]); index++)
    {
        struct arm64_decoded_insn decoded = decode_ok(samples[index].raw);

        CHECK(decoded.instruction == samples[index].instruction);
    }
}

static void test_fp_conversions(void)
{
    static const struct
    {
        uint32_t base;
        enum arm64_instruction vector_instruction;
        enum arm64_instruction scalar_instruction;
    } simd_operations[] = {
        {0x0E21A800U, ARM64_INSN_FCVTNS_SIMD_VECTOR, ARM64_INSN_FCVTNS_SIMD_SCALAR}, {0x2E21A800U, ARM64_INSN_FCVTNU_SIMD_VECTOR, ARM64_INSN_FCVTNU_SIMD_SCALAR}, {0x0EA1A800U, ARM64_INSN_FCVTPS_SIMD_VECTOR, ARM64_INSN_FCVTPS_SIMD_SCALAR}, {0x2EA1A800U, ARM64_INSN_FCVTPU_SIMD_VECTOR, ARM64_INSN_FCVTPU_SIMD_SCALAR}, {0x0E21B800U, ARM64_INSN_FCVTMS_SIMD_VECTOR, ARM64_INSN_FCVTMS_SIMD_SCALAR}, {0x2E21B800U, ARM64_INSN_FCVTMU_SIMD_VECTOR, ARM64_INSN_FCVTMU_SIMD_SCALAR}, {0x0E21C800U, ARM64_INSN_FCVTAS_SIMD_VECTOR, ARM64_INSN_FCVTAS_SIMD_SCALAR}, {0x2E21C800U, ARM64_INSN_FCVTAU_SIMD_VECTOR, ARM64_INSN_FCVTAU_SIMD_SCALAR}, {0x0EA1B800U, ARM64_INSN_FCVTZS_SIMD_VECTOR, ARM64_INSN_FCVTZS_SIMD_SCALAR}, {0x2EA1B800U, ARM64_INSN_FCVTZU_SIMD_VECTOR, ARM64_INSN_FCVTZU_SIMD_SCALAR}, {0x0E21D800U, ARM64_INSN_SCVTF_SIMD_VECTOR, ARM64_INSN_SCVTF_SIMD_SCALAR}, {0x2E21D800U, ARM64_INSN_UCVTF_SIMD_VECTOR, ARM64_INSN_UCVTF_SIMD_SCALAR},
    };
    static const struct
    {
        uint32_t bits;
        uint8_t scalar;
        uint8_t operand_width;
        uint8_t element_width;
    } simd_shapes[] = {
        {0x50000000U, 1, 32, 32}, {0x50400000U, 1, 64, 64}, {0x00000000U, 0, 64, 32}, {0x40000000U, 0, 128, 32}, {0x40400000U, 0, 128, 64},
    };
    static const struct
    {
        uint32_t base;
        enum arm64_instruction instruction;
    } gpr_operations[] = {
        {0x1E200000U, ARM64_INSN_FCVTNS_GPR}, {0x1E210000U, ARM64_INSN_FCVTNU_GPR}, {0x1E280000U, ARM64_INSN_FCVTPS_GPR}, {0x1E290000U, ARM64_INSN_FCVTPU_GPR}, {0x1E300000U, ARM64_INSN_FCVTMS_GPR}, {0x1E310000U, ARM64_INSN_FCVTMU_GPR}, {0x1E380000U, ARM64_INSN_FCVTZS_GPR}, {0x1E390000U, ARM64_INSN_FCVTZU_GPR}, {0x1E240000U, ARM64_INSN_FCVTAS_GPR}, {0x1E250000U, ARM64_INSN_FCVTAU_GPR},
    };
    static const struct
    {
        uint32_t bits;
        uint8_t operand_width;
        uint8_t element_width;
    } gpr_shapes[] = {
        {0x00000000U, 32, 32},
        {0x80000000U, 64, 32},
        {0x00400000U, 32, 64},
        {0x80400000U, 64, 64},
    };
    struct arm64_decoded_insn decoded;

    decoded = decode_ok(0x5EA1B842U);
    CHECK(decoded.instruction == ARM64_INSN_FCVTZS_SIMD_SCALAR);
    CHECK(decoded.operand_width == 32);
    CHECK(decoded.element_width == 32);
    CHECK(decoded.rd == 2);
    CHECK(decoded.rn == 2);

    decoded = decode_ok(0x1E22C020U);
    CHECK(decoded.instruction == ARM64_INSN_FCVT_D_S);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 32);
    CHECK(decoded.rd == 0);
    CHECK(decoded.rn == 1);

    decoded = decode_ok(0x1E624020U);
    CHECK(decoded.instruction == ARM64_INSN_FCVT_S_D);
    CHECK(decoded.operand_width == 32);
    CHECK(decoded.element_width == 64);
    CHECK(decoded.rd == 0);
    CHECK(decoded.rn == 1);

    for (size_t operation_index = 0; operation_index < sizeof(simd_operations) / sizeof(simd_operations[0]); operation_index++)
    {
        for (size_t shape_index = 0; shape_index < sizeof(simd_shapes) / sizeof(simd_shapes[0]); shape_index++)
        {
            decoded = decode_ok(simd_operations[operation_index].base | simd_shapes[shape_index].bits | 0x62U);
            CHECK(decoded.instruction == (simd_shapes[shape_index].scalar ? simd_operations[operation_index].scalar_instruction : simd_operations[operation_index].vector_instruction));
            CHECK(decoded.operand_width == simd_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == simd_shapes[shape_index].element_width);
            CHECK(decoded.rd == 2);
            CHECK(decoded.rn == 3);
        }
    }

    for (size_t operation_index = 0; operation_index < sizeof(gpr_operations) / sizeof(gpr_operations[0]); operation_index++)
    {
        for (size_t shape_index = 0; shape_index < sizeof(gpr_shapes) / sizeof(gpr_shapes[0]); shape_index++)
        {
            decoded = decode_ok(gpr_operations[operation_index].base | gpr_shapes[shape_index].bits | 0x62U);
            CHECK(decoded.instruction == gpr_operations[operation_index].instruction);
            CHECK(decoded.operand_width == gpr_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == gpr_shapes[shape_index].element_width);
            CHECK(decoded.rd == 2);
            CHECK(decoded.rn == 3);
        }
    }

    decode_status_is(0x1E21A820U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x0E61A820U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x5E79A820U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x7EF9B820U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x5E79D820U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x5F3FFC20U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x6F7CE420U, ARM64_DECODE_UNSUPPORTED);
}

static void test_dispatch(void)
{
    struct arm64_decoded_insn decoded = decode_ok(0x91000420U);

    CHECK(decoded.insn_class == ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE);
    CHECK(decoded.instruction == ARM64_INSN_ADD_IMMEDIATE);
    CHECK(decoded.rd == 0);
    CHECK(decoded.rn == 1);
    CHECK(decoded.immediate == 1);

    decoded = decode_ok(0xB1000420U);
    CHECK(decoded.instruction == ARM64_INSN_ADDS_IMMEDIATE);

    decoded = decode_ok(0xD1000420U);
    CHECK(decoded.instruction == ARM64_INSN_SUB_IMMEDIATE);

    decoded = decode_ok(0xF1000420U);
    CHECK(decoded.instruction == ARM64_INSN_SUBS_IMMEDIATE);

    decoded = decode_ok(0x8B020020U);
    CHECK(decoded.insn_class == ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER);
    CHECK(decoded.instruction == ARM64_INSN_ADD_SHIFTED_REGISTER);
    CHECK(decoded.rm == 2);

    static const struct
    {
        uint32_t raw;
        enum arm64_instruction instruction;
    } logical_operations[] = {
        {0x8A220020U, ARM64_INSN_BIC_SHIFTED_REGISTER},
        {0xAA220020U, ARM64_INSN_ORN_SHIFTED_REGISTER},
        {0xCA220020U, ARM64_INSN_EON_SHIFTED_REGISTER},
        {0xEA220020U, ARM64_INSN_BICS_SHIFTED_REGISTER},
    };

    for (size_t index = 0; index < sizeof(logical_operations) / sizeof(logical_operations[0]); index++)
    {
        decoded = decode_ok(logical_operations[index].raw);
        CHECK(decoded.instruction == logical_operations[index].instruction);
        CHECK(decoded.rd == 0);
        CHECK(decoded.rn == 1);
        CHECK(decoded.rm == 2);
    }

    static const struct
    {
        uint32_t raw;
        enum arm64_instruction instruction;
    } conditional_select_operations[] = {
        {0x9A820020U, ARM64_INSN_CSEL},
        {0x9A820420U, ARM64_INSN_CSINC},
        {0xDA820020U, ARM64_INSN_CSINV},
        {0xDA820420U, ARM64_INSN_CSNEG},
    };

    for (size_t index = 0; index < sizeof(conditional_select_operations) / sizeof(conditional_select_operations[0]); index++)
    {
        decoded = decode_ok(conditional_select_operations[index].raw);
        CHECK(decoded.instruction == conditional_select_operations[index].instruction);
        CHECK(decoded.condition == 0);
        CHECK(decoded.rd == 0);
        CHECK(decoded.rn == 1);
        CHECK(decoded.rm == 2);
        CHECK(decoded.operand_width == 64);
    }

    decoded = decode_ok(0xFA410000U);
    CHECK(decoded.instruction == ARM64_INSN_CCMP_REGISTER);
    CHECK(decoded.rm == 1);

    decoded = decode_ok(0xBA410800U);
    CHECK(decoded.instruction == ARM64_INSN_CCMN_IMMEDIATE);
    CHECK(decoded.immediate == 1);

    decoded = decode_ok(0x91C3FFFFU);
    CHECK(decoded.instruction == ARM64_INSN_SMAX_IMMEDIATE);

    decode_status_is(0x73000000U, ARM64_DECODE_UNALLOCATED);
    CHECK(decoded.immediate == UINT64_MAX);

    decode_status_is(0xDAC10020U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xDAC11820U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xDAC123E0U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xDAC12020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0xDAC143E0U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xDAC147E0U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x0000C19FU, ARM64_DECODE_UNALLOCATED);

    decoded = decode_ok(0x90000000U);
    CHECK(decoded.insn_class == ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE);
    CHECK(decoded.instruction == ARM64_INSN_ADRP);
    CHECK(decoded.offset == 0);

    decode_status_is(0x02000000U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x04000000U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xC00800FFU, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x40000000U, ARM64_DECODE_UNALLOCATED);
    CHECK(arm64_decode_insn(0x80000000U, &decoded) == ARM64_DECODE_UNSUPPORTED);
    CHECK(decoded.insn_class == ARM64_INSN_CLASS_SME);
}

static void test_system(void)
{
    struct arm64_decoded_insn decoded = decode_ok(0xD5033BBFU);
    static const struct
    {
        uint32_t raw;
        uint8_t option;
    } bti_cases[] = {
        {0xD503241FU, 0},
        {0xD503245FU, 2},
        {0xD503249FU, 4},
        {0xD50324DFU, 6},
    };

    decoded = decode_ok(0xD503201FU);
    CHECK(decoded.instruction == ARM64_INSN_NOP);

    CHECK(arm64_decode_insn(0xD50320DFU, &decoded) == ARM64_DECODE_UNSUPPORTED);
    CHECK(decoded.insn_class == ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM);
    CHECK(decoded.instruction == ARM64_INSN_UNKNOWN);

    decoded = decode_ok(0xD5033BBFU);
    CHECK(decoded.instruction == ARM64_INSN_DMB);
    CHECK(decoded.option == 0xB);

    decoded = decode_ok(0xD4202460U);
    CHECK(decoded.instruction == ARM64_INSN_BRK);
    CHECK(decoded.immediate == 0x123);

    decoded = decode_ok(0xD53B4209U);
    CHECK(decoded.instruction == ARM64_INSN_MRS);
    CHECK(decoded.rt == 9);
    CHECK(decoded.sysreg == ARM64_SYSREG_KEY(3, 3, 4, 2, 0));

    decoded = decode_ok(0xD51B4209U);
    CHECK(decoded.instruction == ARM64_INSN_MSR_REGISTER);
    CHECK(decoded.rt == 9);
    CHECK(decoded.sysreg == ARM64_SYSREG_KEY(3, 3, 4, 2, 0));

    decoded = decode_ok(0xD503233FU);
    CHECK(decoded.instruction == ARM64_INSN_PACIASP);

    for (unsigned int index = 0; index < sizeof(bti_cases) / sizeof(bti_cases[0]); index++)
    {
        decoded = decode_ok(bti_cases[index].raw);
        CHECK(decoded.instruction == ARM64_INSN_BTI);
        CHECK(decoded.option == bti_cases[index].option);
    }
}

static void test_load_store(void)
{
    struct arm64_decoded_insn decoded = decode_ok(0xA8410440U);

    CHECK(decoded.instruction == ARM64_INSN_LDNP_GPR);
    CHECK(decoded.memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET);
    CHECK(decoded.offset == 16);

    decoded = decode_ok(0xA8C10440U);
    CHECK(decoded.instruction == ARM64_INSN_LDP_GPR_POST_INDEX);
    CHECK(decoded.memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX);
    CHECK(decoded.offset == 16);

    decoded = decode_ok(0xA9C10440U);
    CHECK(decoded.instruction == ARM64_INSN_LDP_GPR_PRE_INDEX);
    CHECK(decoded.memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX);
    CHECK(decoded.offset == 16);

    decoded = decode_ok(0xF84098E6U);
    CHECK(decoded.instruction == ARM64_INSN_LDTR_GPR);
    CHECK(decoded.memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET);
    CHECK(decoded.offset == 9);

    decoded = decode_ok(0xF862D820U);
    CHECK(decoded.instruction == ARM64_INSN_LDR_GPR_REGISTER_OFFSET);
    CHECK(decoded.memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);
    CHECK(decoded.rs == 0);
    CHECK(decoded.extend_type == 6);
    CHECK(decoded.shift_amount == 3);

    decoded = decode_ok(0xF8657883U);
    CHECK(decoded.instruction == ARM64_INSN_LDR_GPR_REGISTER_OFFSET);
    CHECK(decoded.extend_type == 3);
    CHECK(decoded.shift_amount == 3);

    decoded = decode_ok(0xF9802020U);
    CHECK(decoded.instruction == ARM64_INSN_PRFM_UNSIGNED_OFFSET);
    CHECK(decoded.offset == 0x40);
    CHECK(decoded.prefetch_operation == 0);
    CHECK(decoded.access_bytes == 0);
    CHECK(decoded.operand_width == 0);

    decoded = decode_ok(0xF8800020U);
    CHECK(decoded.instruction == ARM64_INSN_PRFUM);

    decoded = decode_ok(0xF8A26820U);
    CHECK(decoded.instruction == ARM64_INSN_PRFM_REGISTER_OFFSET);
    CHECK(decoded.rm == 2);
    CHECK(decoded.rt == 0);

    CHECK(arm64_decode_insn(0xF8A04818U, &decoded) == ARM64_DECODE_UNSUPPORTED);
    CHECK(decoded.insn_class == ARM64_INSN_CLASS_LOAD_STORE);
    CHECK(decoded.instruction == ARM64_INSN_UNKNOWN);

    decoded = decode_ok(0xB93336D0U);
    CHECK(decoded.instruction == ARM64_INSN_STR_GPR_UNSIGNED_OFFSET);
    CHECK(decoded.rm == 0);
    CHECK(decoded.rt2 == 0);
    CHECK(decoded.rs == 0);

    decoded = decode_ok(0x3D77D3FEU);
    CHECK(decoded.instruction == ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET);
    CHECK(decoded.access_bytes == 1);

    decoded = decode_ok(0xF9000020U);
    CHECK(decoded.instruction == ARM64_INSN_STR_GPR_UNSIGNED_OFFSET);
    CHECK(decoded.access_bytes == 8);

    decoded = decode_ok(0xA9000440U);
    CHECK(decoded.instruction == ARM64_INSN_STP_GPR_OFFSET);
    CHECK(decoded.access_bytes == 8);
    CHECK(decoded.rm == 0);
    CHECK(decoded.rs == 0);

    decoded = decode_ok(0xBD400020U);
    CHECK(decoded.instruction == ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET);
    CHECK(decoded.access_bytes == 4);

    decoded = decode_ok(0xF9B314B8U);
    CHECK(decoded.instruction == ARM64_INSN_PRFM_UNSIGNED_OFFSET);

    decoded = decode_ok(0x69400440U);
    CHECK(decoded.instruction == ARM64_INSN_LDPSW_OFFSET);
    CHECK(decoded.operand_width == 64);

    decoded = decode_ok(0x68C10440U);
    CHECK(decoded.instruction == ARM64_INSN_LDPSW_POST_INDEX);

    decoded = decode_ok(0x08A07C00U);
    CHECK(decoded.instruction == ARM64_INSN_CAS);
    CHECK(decoded.access_bytes == 1);

    decoded = decode_ok(0x08207C00U);
    CHECK(decoded.instruction == ARM64_INSN_CASP);
    CHECK(decoded.operand_width == 32);
    CHECK(decoded.rm == 0);
    CHECK(decoded.rt2 == 0);

    decoded = decode_ok(0x48207C00U);
    CHECK(decoded.instruction == ARM64_INSN_CASP);
    CHECK(decoded.operand_width == 64);

    decoded = decode_ok(0xC89F7C20U);
    CHECK(decoded.instruction == ARM64_INSN_STLLR);
    CHECK(decoded.rm == 0);
    CHECK(decoded.rt2 == 0);
    CHECK(decoded.rs == 0);

    decoded = decode_ok(0xC8DF7C20U);
    CHECK(decoded.instruction == ARM64_INSN_LDLAR);
    CHECK(decoded.access_bytes == 8);

    decoded = decode_ok(0x78BFC0C5U);
    CHECK(decoded.instruction == ARM64_INSN_LDAPR);
    CHECK(decoded.access_bytes == 2);

    decoded = decode_ok(0x38200000U);
    CHECK(decoded.instruction == ARM64_INSN_LDADD);

    CHECK(decode_ok(0x38A00000U).instruction == ARM64_INSN_LDADDA);
    CHECK(decode_ok(0x38600000U).instruction == ARM64_INSN_LDADDL);
    CHECK(decode_ok(0x38E00000U).instruction == ARM64_INSN_LDADDAL);
    CHECK(decode_ok(0x08E07C00U).instruction == ARM64_INSN_CASA);
    CHECK(decode_ok(0x08A0FC00U).instruction == ARM64_INSN_CASL);
    CHECK(decode_ok(0x08E0FC00U).instruction == ARM64_INSN_CASAL);
    CHECK(decode_ok(0xC89FFC20U).instruction == ARM64_INSN_STLR);
    CHECK(decode_ok(0xC8DFFC20U).instruction == ARM64_INSN_LDAR);

    decoded = decode_ok(0x19000000U);
    CHECK(decoded.instruction == ARM64_INSN_STLUR);
    CHECK(decoded.rm == 0);
    CHECK(decoded.rt2 == 0);
    CHECK(decoded.rs == 0);

    decoded = decode_ok(0x3DC00000U);
    CHECK(decoded.instruction == ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET);
    CHECK(decoded.access_bytes == 16);
    CHECK(decoded.operand_width == 128);

    decoded = decode_ok(0x58000020U);
    CHECK(decoded.instruction == ARM64_INSN_LDR_LITERAL_GPR);
    CHECK(decoded.memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL);
    CHECK(decoded.offset == 4);
    CHECK(decoded.rn == 0);
    CHECK(decoded.rm == 0);
    CHECK(decoded.rt2 == 0);
    CHECK(decoded.rs == 0);

    decoded = decode_ok(0x9C000020U);
    CHECK(decoded.instruction == ARM64_INSN_LDR_LITERAL_FP_SIMD);
    CHECK(decoded.memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL);
    CHECK(decoded.access_bytes == 16);
    CHECK(decoded.operand_width == 128);

    decoded = decode_ok(0xD8000023U);
    CHECK(decoded.instruction == ARM64_INSN_PRFM_LITERAL);
    CHECK(decoded.prefetch_operation == 3);
    CHECK(decoded.rt == 0);
    CHECK(decoded.operand_width == 0);

    decode_status_is(0xFC4098E6U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x6868F17DU, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0xF89105E7U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0xF8828A22U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x083F7C1FU, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x690CDA48U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xF871A5C0U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xF8A35CDBU, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x19200881U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x59200C86U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x59210C86U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x78209081U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x59209081U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x5920909FU, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x59219081U, ARM64_DECODE_UNPREDICTABLE);
    decode_status_is(0xF83FD1A0U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xF83F91AEU, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xF821B1B4U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xF821A1B6U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xF83FD1A1U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0xF83F91B8U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0xF821B1B5U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0xF821A1B8U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0xA9400020U, ARM64_DECODE_UNPREDICTABLE);
    decode_status_is(0xA8C10400U, ARM64_DECODE_UNPREDICTABLE);
    decode_status_is(0xA8810400U, ARM64_DECODE_UNPREDICTABLE);
    decode_status_is(0xF8408400U, ARM64_DECODE_UNPREDICTABLE);
}

static void test_control_flow(void)
{
    struct arm64_decoded_insn decoded = decode_ok(0x14000004U);

    CHECK(decoded.instruction == ARM64_INSN_B);
    CHECK(decoded.offset == 16);

    decoded = decode_ok(0x90000000U);
    CHECK(decoded.instruction == ARM64_INSN_ADRP);
    CHECK(decoded.offset == 0);

    decoded = decode_ok(0x10FFFFE0U);
    CHECK(decoded.instruction == ARM64_INSN_ADR);
    CHECK(decoded.offset == -4);

    decoded = decode_ok(0x10000081U);
    CHECK(decoded.instruction == ARM64_INSN_ADR);
    CHECK(decoded.offset == 16);

    decoded = decode_ok(0xB0000002U);
    CHECK(decoded.instruction == ARM64_INSN_ADRP);
    CHECK(decoded.offset == 0x1000);

    decoded = decode_ok(0xF0FFFFE3U);
    CHECK(decoded.instruction == ARM64_INSN_ADRP);
    CHECK(decoded.offset == -0x1000);

    decoded = decode_ok(0x34000000U);
    CHECK(decoded.instruction == ARM64_INSN_CBZ);
    CHECK(decoded.operand_width == 32);

    decoded = decode_ok(0xB4000000U);
    CHECK(decoded.instruction == ARM64_INSN_CBZ);
    CHECK(decoded.operand_width == 64);
}

static void test_scalar_fp(void)
{
    static const enum arm64_instruction two_source_instructions[] = {
        ARM64_INSN_FMUL_SCALAR, ARM64_INSN_FDIV_SCALAR, ARM64_INSN_FADD_SCALAR, ARM64_INSN_FSUB_SCALAR, ARM64_INSN_FMAX_SCALAR, ARM64_INSN_FMIN_SCALAR, ARM64_INSN_FMAXNM_SCALAR, ARM64_INSN_FMINNM_SCALAR, ARM64_INSN_FNMUL_SCALAR,
    };
    struct arm64_decoded_insn decoded = decode_ok(0x1E2E1000U);

    CHECK(decoded.instruction == ARM64_INSN_FMOV_SCALAR_IMMEDIATE);
    CHECK(decoded.expanded_immediate == 0x3F800000ULL);
    CHECK(decoded.rn == 0);
    CHECK(decoded.rm == 0);
    CHECK(decoded.ra == 0);

    decoded = decode_ok(0x1E701001U);
    CHECK(decoded.instruction == ARM64_INSN_FMOV_SCALAR_IMMEDIATE);
    CHECK(decoded.expanded_immediate == 0xC000000000000000ULL);

    decoded = decode_ok(0x1EEE1002U);
    CHECK(decoded.instruction == ARM64_INSN_FMOV_SCALAR_IMMEDIATE);
    CHECK(decoded.operand_width == 16);
    CHECK(decoded.element_width == 16);
    CHECK(decoded.expanded_immediate == 0x3C00ULL);

    decoded = decode_ok(0x1E244062U);
    CHECK(decoded.instruction == ARM64_INSN_FRINTN_SCALAR);

    decoded = decode_ok(0x1E63145AU);
    CHECK(decoded.instruction == ARM64_INSN_FCCMPE_SCALAR);
    CHECK(decoded.condition == 1);
    CHECK(decoded.immediate == 0xA);

    decoded = decode_ok(0x1E2021F8U);
    CHECK(decoded.instruction == ARM64_INSN_FCMPE_ZERO_SCALAR);
    CHECK(decoded.rd == 0);
    CHECK(decoded.rm == 0);
    CHECK(decoded.ra == 0);

    decode_status_is(0x1E2F21F8U, ARM64_DECODE_UNALLOCATED);

    decoded = decode_ok(0x1EF322D0U);
    CHECK(decoded.instruction == ARM64_INSN_FCMPE_REGISTER_SCALAR);
    CHECK(decoded.operand_width == 16);

    decoded = decode_ok(0x1E204020U);
    CHECK(decoded.instruction == ARM64_INSN_FMOV_SCALAR);

    decoded = decode_ok(0x1EE0C062U);
    CHECK(decoded.instruction == ARM64_INSN_FABS_SCALAR);
    CHECK(decoded.operand_width == 16);
    CHECK(decoded.element_width == 16);

    decoded = decode_ok(0x1EE3145AU);
    CHECK(decoded.instruction == ARM64_INSN_FCCMPE_SCALAR);
    CHECK(decoded.operand_width == 16);
    CHECK(decoded.element_width == 16);

    decoded = decode_ok(0x1EE20C20U);
    CHECK(decoded.instruction == ARM64_INSN_FCSEL_SCALAR);
    CHECK(decoded.operand_width == 16);
    CHECK(decoded.element_width == 16);

    for (size_t operation_index = 0; operation_index < sizeof(two_source_instructions) / sizeof(two_source_instructions[0]); operation_index++)
    {
        uint32_t raw = 0x1EE00820U | ((uint32_t)operation_index << 12) | 0x62U;

        decoded = decode_ok(raw);
        CHECK(decoded.instruction == two_source_instructions[operation_index]);
        CHECK(decoded.operand_width == 16);
        CHECK(decoded.element_width == 16);
        CHECK(decoded.rd == 2);
        CHECK(decoded.rn == 3);
        CHECK(decoded.rm == 0);
    }

    decode_status_is(0x1EA20820U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x1EAE1000U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x1EA04020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x1EA3145AU, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x1EA20C20U, ARM64_DECODE_UNALLOCATED);
}

static void test_scalar_copy(void)
{
    static const struct
    {
        uint32_t raw;
        uint8_t element_width;
        uint8_t lane_index;
    } cases[] = {
        {0x5E010420U, 8, 0}, {0x5E1F0462U, 8, 15}, {0x5E0204A4U, 16, 0}, {0x5E1E04E6U, 16, 7}, {0x5E040528U, 32, 0}, {0x5E0C0422U, 32, 1}, {0x5E1C056AU, 32, 3}, {0x5E0805ACU, 64, 0}, {0x5E1805EEU, 64, 1},
    };
    struct arm64_decoded_insn decoded;

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); index++)
    {
        decoded = decode_ok(cases[index].raw);
        CHECK(decoded.instruction == ARM64_INSN_DUP_ELEMENT_SCALAR);
        CHECK(decoded.element_width == cases[index].element_width);
        CHECK(decoded.lane_index == cases[index].lane_index);
        CHECK(decoded.operand_width == cases[index].element_width);
    }

    decoded = decode_ok(0x5E0C0422U);
    CHECK(decoded.rd == 2);
    CHECK(decoded.rn == 1);

    for (uint32_t imm5 = 0; imm5 < 32; imm5++)
    {
        uint32_t raw = 0x5E000420U | (imm5 << 16);

        if (!imm5 || imm5 == 16)
        {
            decode_status_is(raw, ARM64_DECODE_UNALLOCATED);
            continue;
        }

        uint8_t size = (uint8_t)__builtin_ctz(imm5);
        decoded = decode_ok(raw);
    CHECK(decoded.instruction == ARM64_INSN_DUP_ELEMENT_SCALAR);
        CHECK(decoded.element_width == (8U << size));
        CHECK(decoded.lane_index == (imm5 >> (size + 1)));
        CHECK(decoded.operand_width == decoded.element_width);
    }
}

static void test_fp_by_element(void)
{
    static const struct
    {
        uint32_t bits;
        enum arm64_instruction vector_instruction;
        enum arm64_instruction scalar_instruction;
    } operations[] = {
        {0x00000000U, ARM64_INSN_FMLA_VECTOR_BY_ELEMENT, ARM64_INSN_FMLA_SCALAR_BY_ELEMENT},
        {0x00004000U, ARM64_INSN_FMLS_VECTOR_BY_ELEMENT, ARM64_INSN_FMLS_SCALAR_BY_ELEMENT},
        {0x00008000U, ARM64_INSN_FMUL_VECTOR_BY_ELEMENT, ARM64_INSN_FMUL_SCALAR_BY_ELEMENT},
        {0x20008000U, ARM64_INSN_FMULX_VECTOR_BY_ELEMENT, ARM64_INSN_FMULX_SCALAR_BY_ELEMENT},
    };
    static const struct
    {
        uint32_t fmla;
        uint8_t scalar;
        uint8_t operand_width;
        uint8_t element_width;
        uint8_t lane_index;
        uint8_t rm;
    } shapes[] = {
        {0x0F3F1883U, 0, 64, 16, 7, 15}, {0x4F3F1883U, 0, 128, 16, 7, 15}, {0x0FBF1883U, 0, 64, 32, 3, 31}, {0x4FBF1883U, 0, 128, 32, 3, 31}, {0x4FDF1883U, 0, 128, 64, 1, 31}, {0x5F3F1883U, 1, 16, 16, 7, 15}, {0x5FBF1883U, 1, 32, 32, 3, 31}, {0x5FDF1883U, 1, 64, 64, 1, 31},
    };
    struct arm64_decoded_insn decoded;

    for (size_t operation_index = 0; operation_index < sizeof(operations) / sizeof(operations[0]); operation_index++)
    {
        for (size_t shape_index = 0; shape_index < sizeof(shapes) / sizeof(shapes[0]); shape_index++)
        {
            decoded = decode_ok(shapes[shape_index].fmla | operations[operation_index].bits);
            CHECK(decoded.instruction == (shapes[shape_index].scalar ? operations[operation_index].scalar_instruction : operations[operation_index].vector_instruction));
            CHECK(decoded.operand_width == shapes[shape_index].operand_width);
            CHECK(decoded.element_width == shapes[shape_index].element_width);
            CHECK(decoded.lane_index == shapes[shape_index].lane_index);
            CHECK(decoded.rm == shapes[shape_index].rm);
            CHECK(decoded.rd == 3);
            CHECK(decoded.rn == 4);
        }
    }

    decoded = decode_ok(0x0F861232U);
    CHECK(decoded.instruction == ARM64_INSN_FMLA_VECTOR_BY_ELEMENT);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 32);
    CHECK(decoded.lane_index == 0);
    CHECK(decoded.rm == 6);
    CHECK(decoded.rd == 18);
    CHECK(decoded.rn == 17);

    decode_status_is(0x0F421020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x0FC21020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x4FEE1020U, ARM64_DECODE_UNALLOCATED);

    decode_status_is(0x2F420020U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x6FAB4949U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x0F4E81ACU, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x4FA7DAD5U, ARM64_DECODE_UNSUPPORTED);
}

static void test_fhm_by_element(void)
{
    static const struct
    {
        uint32_t base;
        enum arm64_instruction instruction;
    } operations[] = {
        {0x0F800000U, ARM64_INSN_FMLAL_VECTOR_BY_ELEMENT},
        {0x0F804000U, ARM64_INSN_FMLSL_VECTOR_BY_ELEMENT},
        {0x2F808000U, ARM64_INSN_FMLAL2_VECTOR_BY_ELEMENT},
        {0x2F80C000U, ARM64_INSN_FMLSL2_VECTOR_BY_ELEMENT},
    };
    static const struct
    {
        uint32_t bits;
        uint8_t lane_index;
    } lanes[] = {
        {0x00000000U, 0},
        {0x00300800U, 7},
    };
    struct arm64_decoded_insn decoded;

    for (size_t operation_index = 0; operation_index < sizeof(operations) / sizeof(operations[0]); operation_index++)
    {
        for (size_t lane_index = 0; lane_index < sizeof(lanes) / sizeof(lanes[0]); lane_index++)
        {
            for (size_t q = 0; q < 2; q++)
            {
                uint32_t raw = operations[operation_index].base | lanes[lane_index].bits | ((uint32_t)q << 30) | 0x20064U;

                decoded = decode_ok(raw);
                CHECK(decoded.instruction == operations[operation_index].instruction);
                CHECK(decoded.operand_width == (q ? 128 : 64));
                CHECK(decoded.element_width == 16);
                CHECK(decoded.lane_index == lanes[lane_index].lane_index);
                CHECK(decoded.rd == 4);
                CHECK(decoded.rn == 3);
                CHECK(decoded.rm == 2);
            }
        }
    }

    decode_status_is(0x0FF20820U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x1FB20820U, ARM64_DECODE_UNALLOCATED);
}

static void test_extra_by_element(void)
{
    struct arm64_decoded_insn decoded;

    decoded = decode_ok(0x2F72D020U);
    CHECK(decoded.instruction == ARM64_INSN_SQRDMLAH_VECTOR_BY_ELEMENT);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 16);
    CHECK(decoded.lane_index == 3);
    CHECK(decoded.rm == 2);

    decoded = decode_ok(0x6FA2F820U);
    CHECK(decoded.instruction == ARM64_INSN_SQRDMLSH_VECTOR_BY_ELEMENT);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.element_width == 32);
    CHECK(decoded.lane_index == 3);
    CHECK(decoded.rm == 2);

    decoded = decode_ok(0x7F72D020U);
    CHECK(decoded.instruction == ARM64_INSN_SQRDMLAH_SCALAR_BY_ELEMENT);
    CHECK(decoded.operand_width == 16);
    CHECK(decoded.element_width == 16);
    CHECK(decoded.lane_index == 3);

    decoded = decode_ok(0x7FA2F820U);
    CHECK(decoded.instruction == ARM64_INSN_SQRDMLSH_SCALAR_BY_ELEMENT);
    CHECK(decoded.operand_width == 32);
    CHECK(decoded.element_width == 32);
    CHECK(decoded.lane_index == 3);

    decoded = decode_ok(0x0FA2E020U);
    CHECK(decoded.instruction == ARM64_INSN_SDOT_VECTOR_BY_ELEMENT);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 8);
    CHECK(decoded.lane_index == 1);
    CHECK(decoded.rm == 2);

    decoded = decode_ok(0x6F82E820U);
    CHECK(decoded.instruction == ARM64_INSN_UDOT_VECTOR_BY_ELEMENT);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.lane_index == 2);

    decoded = decode_ok(0x0FA2F83FU);
    CHECK(decoded.instruction == ARM64_INSN_USDOT_VECTOR_BY_ELEMENT);
    CHECK(decoded.lane_index == 3);
    CHECK(decoded.rd == 31);
    CHECK(decoded.rn == 1);
    CHECK(decoded.rm == 2);

    decoded = decode_ok(0x0F22F83FU);
    CHECK(decoded.instruction == ARM64_INSN_SUDOT_VECTOR_BY_ELEMENT);
    CHECK(decoded.element_width == 8);
    CHECK(decoded.lane_index == 3);

    decoded = decode_ok(0x0F44F062U);
    CHECK(decoded.instruction == ARM64_INSN_BFDOT_VECTOR_BY_ELEMENT);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 16);
    CHECK(decoded.lane_index == 0);
    CHECK(decoded.rm == 4);

    decoded = decode_ok(0x4F64F862U);
    CHECK(decoded.instruction == ARM64_INSN_BFDOT_VECTOR_BY_ELEMENT);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.lane_index == 3);

    decoded = decode_ok(0x0FFAFAAEU);
    CHECK(decoded.instruction == ARM64_INSN_BFMLALB_VECTOR_BY_ELEMENT);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.element_width == 16);
    CHECK(decoded.lane_index == 7);
    CHECK(decoded.rm == 10);

    decoded = decode_ok(0x4FFEF955U);
    CHECK(decoded.instruction == ARM64_INSN_BFMLALT_VECTOR_BY_ELEMENT);
    CHECK(decoded.lane_index == 7);
    CHECK(decoded.rm == 14);

    decode_status_is(0x2F32D020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2FF2F020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x0F42E020U, ARM64_DECODE_UNALLOCATED);
}

static void test_fcma(void)
{
    static const struct
    {
        uint8_t q;
        uint8_t size;
        uint8_t operand_width;
        uint8_t element_width;
    } vector_shapes[] = {
        {0, 1, 64, 16}, {1, 1, 128, 16}, {0, 2, 64, 32}, {1, 2, 128, 32}, {1, 3, 128, 64},
    };
    static const struct
    {
        uint8_t q;
        uint8_t size;
        uint8_t h;
        uint8_t l;
        uint8_t operand_width;
        uint8_t element_width;
        uint8_t lane_index;
    } element_shapes[] = {
        {0, 1, 0, 0, 64, 16, 0}, {0, 1, 0, 1, 64, 16, 1}, {1, 1, 0, 0, 128, 16, 0}, {1, 1, 0, 1, 128, 16, 1}, {1, 1, 1, 0, 128, 16, 2}, {1, 1, 1, 1, 128, 16, 3}, {1, 2, 0, 0, 128, 32, 0}, {1, 2, 1, 0, 128, 32, 1},
    };
    struct arm64_decoded_insn decoded;

    for (size_t shape_index = 0; shape_index < sizeof(vector_shapes) / sizeof(vector_shapes[0]); shape_index++)
    {
        for (size_t rotation = 0; rotation < 4; rotation++)
        {
            uint32_t raw = 0x2E00C400U | ((uint32_t)vector_shapes[shape_index].q << 30) | ((uint32_t)vector_shapes[shape_index].size << 22) | ((uint32_t)rotation << 11) | 0x20064U;

            decoded = decode_ok(raw);
            CHECK(decoded.instruction == ARM64_INSN_FCMLA_VECTOR);
            CHECK(decoded.immediate == rotation);
            CHECK(decoded.operand_width == vector_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == vector_shapes[shape_index].element_width);
            CHECK(decoded.rd == 4);
            CHECK(decoded.rn == 3);
            CHECK(decoded.rm == 2);
        }

        for (size_t rotation = 0; rotation < 2; rotation++)
        {
            uint32_t raw = 0x2E00E400U | ((uint32_t)vector_shapes[shape_index].q << 30) | ((uint32_t)vector_shapes[shape_index].size << 22) | ((uint32_t)rotation << 12) | 0x20064U;

            decoded = decode_ok(raw);
            CHECK(decoded.instruction == ARM64_INSN_FCADD_VECTOR);
            CHECK(decoded.immediate == (rotation ? ARM64_SIMD_ROTATION_270 : ARM64_SIMD_ROTATION_90));
            CHECK(decoded.operand_width == vector_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == vector_shapes[shape_index].element_width);
        }
    }

    for (size_t shape_index = 0; shape_index < sizeof(element_shapes) / sizeof(element_shapes[0]); shape_index++)
    {
        for (size_t rotation = 0; rotation < 4; rotation++)
        {
            uint32_t raw = 0x2F001000U | ((uint32_t)element_shapes[shape_index].q << 30) | ((uint32_t)element_shapes[shape_index].size << 22) | ((uint32_t)element_shapes[shape_index].l << 21) | (1U << 20) | (2U << 16) | ((uint32_t)rotation << 13) | ((uint32_t)element_shapes[shape_index].h << 11) | 0x64U;

            decoded = decode_ok(raw);
            CHECK(decoded.instruction == ARM64_INSN_FCMLA_VECTOR_BY_ELEMENT);
            CHECK(decoded.immediate == rotation);
            CHECK(decoded.operand_width == element_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == element_shapes[shape_index].element_width);
            CHECK(decoded.lane_index == element_shapes[shape_index].lane_index);
            CHECK(decoded.rm == 18);
        }
    }

    decode_status_is(0x2E00C420U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2EC0C420U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2F001020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2FC01020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2F801020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x6FA01020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2F601820U, ARM64_DECODE_UNALLOCATED);
}

static void test_vector_3same_extra(void)
{
    static const struct
    {
        uint8_t u;
        uint8_t opcode;
        uint8_t valid_sizes;
        uint8_t element_width;
        enum arm64_instruction instruction;
    } operations[] = {
        {1, 16, 0x6, 0, ARM64_INSN_SQRDMLAH_VECTOR}, {1, 17, 0x6, 0, ARM64_INSN_SQRDMLSH_VECTOR}, {0, 18, 0x4, 8, ARM64_INSN_SDOT_VECTOR}, {1, 18, 0x4, 8, ARM64_INSN_UDOT_VECTOR}, {0, 19, 0x4, 8, ARM64_INSN_USDOT_VECTOR},
    };
    struct arm64_decoded_insn decoded;

    for (size_t operation_index = 0; operation_index < sizeof(operations) / sizeof(operations[0]); operation_index++)
    {
        for (size_t q = 0; q < 2; q++)
        {
            for (size_t size = 0; size < 4; size++)
            {
                uint32_t raw = 0x0E008400U | ((uint32_t)q << 30) | ((uint32_t)operations[operation_index].u << 29) | ((uint32_t)size << 22) | ((uint32_t)operations[operation_index].opcode << 11) | 0x20064U;

                if (!(operations[operation_index].valid_sizes & (1U << size)))
                {
                    decode_status_is(raw, ARM64_DECODE_UNALLOCATED);
                    continue;
                }

                decoded = decode_ok(raw);
                CHECK(decoded.instruction == operations[operation_index].instruction);
                CHECK(decoded.operand_width == (q ? 128 : 64));
                CHECK(decoded.element_width == (operations[operation_index].element_width ? operations[operation_index].element_width : (8U << size)));
                CHECK(decoded.rd == 4);
                CHECK(decoded.rn == 3);
                CHECK(decoded.rm == 2);
            }
        }
    }

    decoded = decode_ok(0x2E428420U);
    CHECK(decoded.instruction == ARM64_INSN_SQRDMLAH_VECTOR);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 16);

    decoded = decode_ok(0x2E428C20U);
    CHECK(decoded.instruction == ARM64_INSN_SQRDMLSH_VECTOR);

    decoded = decode_ok(0x0E829420U);
    CHECK(decoded.instruction == ARM64_INSN_SDOT_VECTOR);
    CHECK(decoded.element_width == 8);

    decoded = decode_ok(0x2E829420U);
    CHECK(decoded.instruction == ARM64_INSN_UDOT_VECTOR);

    decoded = decode_ok(0x0E829C20U);
    CHECK(decoded.instruction == ARM64_INSN_USDOT_VECTOR);

    decoded = decode_ok(0x2E44FC62U);
    CHECK(decoded.instruction == ARM64_INSN_BFDOT_VECTOR);
    CHECK(decoded.rd == 2);
    CHECK(decoded.rn == 3);
    CHECK(decoded.rm == 4);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 16);

    decoded = decode_ok(0x6E44FC62U);
    CHECK(decoded.instruction == ARM64_INSN_BFDOT_VECTOR);
    CHECK(decoded.operand_width == 128);

    decoded = decode_ok(0x2ECEFEAAU);
    CHECK(decoded.instruction == ARM64_INSN_BFMLALB_VECTOR);
    CHECK(decoded.operand_width == 128);

    decoded = decode_ok(0x6ECAFDD5U);
    CHECK(decoded.instruction == ARM64_INSN_BFMLALT_VECTOR);
    CHECK(decoded.operand_width == 128);

    decoded = decode_ok(0x6E44EC62U);
    CHECK(decoded.instruction == ARM64_INSN_BFMMLA_VECTOR);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.element_width == 16);

    decoded = decode_ok(0x4E9FA601U);
    CHECK(decoded.instruction == ARM64_INSN_SMMLA_VECTOR);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.element_width == 8);

    decoded = decode_ok(0x6E9FA601U);
    CHECK(decoded.instruction == ARM64_INSN_UMMLA_VECTOR);

    decoded = decode_ok(0x4E9FAE01U);
    CHECK(decoded.instruction == ARM64_INSN_USMMLA_VECTOR);

    decode_status_is(0x2E44EC62U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x0E9FA601U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2E9FA601U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x0E9FAE01U, ARM64_DECODE_UNALLOCATED);
}

static void test_scalar_3same(void)
{
    static const struct
    {
        uint8_t u;
        uint8_t opcode;
        uint8_t valid_sizes;
        enum arm64_instruction instruction;
    } integer_operations[] = {
        {0, 1, 0xF, ARM64_INSN_SQADD_SCALAR}, {0, 5, 0xF, ARM64_INSN_SQSUB_SCALAR}, {0, 9, 0xF, ARM64_INSN_SQSHL_SCALAR}, {0, 11, 0xF, ARM64_INSN_SQRSHL_SCALAR}, {0, 22, 0x6, ARM64_INSN_SQDMULH_SCALAR}, {1, 1, 0xF, ARM64_INSN_UQADD_SCALAR}, {1, 5, 0xF, ARM64_INSN_UQSUB_SCALAR}, {1, 9, 0xF, ARM64_INSN_UQSHL_SCALAR}, {1, 11, 0xF, ARM64_INSN_UQRSHL_SCALAR}, {1, 22, 0x6, ARM64_INSN_SQRDMULH_SCALAR}, {0, 6, 0x8, ARM64_INSN_CMGT_SCALAR}, {0, 7, 0x8, ARM64_INSN_CMGE_SCALAR}, {0, 8, 0x8, ARM64_INSN_SSHL_SCALAR}, {0, 10, 0x8, ARM64_INSN_SRSHL_SCALAR}, {0, 16, 0x8, ARM64_INSN_ADD_SCALAR}, {0, 17, 0x8, ARM64_INSN_CMTST_SCALAR}, {1, 6, 0x8, ARM64_INSN_CMHI_SCALAR}, {1, 7, 0x8, ARM64_INSN_CMHS_SCALAR}, {1, 8, 0x8, ARM64_INSN_USHL_SCALAR}, {1, 10, 0x8, ARM64_INSN_URSHL_SCALAR}, {1, 16, 0x8, ARM64_INSN_SUB_SCALAR}, {1, 17, 0x8, ARM64_INSN_CMEQ_SCALAR},
    };
    static const struct
    {
        uint8_t u;
        uint8_t alternate;
        uint8_t opcode;
        enum arm64_instruction instruction;
    } fp_operations[] = {
        {0, 0, 27, ARM64_INSN_FMULX_SCALAR}, {0, 0, 28, ARM64_INSN_FCMEQ_SCALAR}, {0, 0, 31, ARM64_INSN_FRECPS_SCALAR}, {0, 1, 31, ARM64_INSN_FRSQRTS_SCALAR}, {1, 0, 28, ARM64_INSN_FCMGE_SCALAR}, {1, 0, 29, ARM64_INSN_FACGE_SCALAR}, {1, 1, 26, ARM64_INSN_FABD_SCALAR}, {1, 1, 28, ARM64_INSN_FCMGT_SCALAR}, {1, 1, 29, ARM64_INSN_FACGT_SCALAR},
    };
    static const struct
    {
        uint8_t u;
        uint8_t alternate;
        uint8_t opcode;
        enum arm64_instruction instruction;
    } fp16_operations[] = {
        {0, 0, 3, ARM64_INSN_FMULX_SCALAR}, {0, 0, 4, ARM64_INSN_FCMEQ_SCALAR}, {0, 0, 7, ARM64_INSN_FRECPS_SCALAR}, {0, 1, 7, ARM64_INSN_FRSQRTS_SCALAR}, {1, 0, 4, ARM64_INSN_FCMGE_SCALAR}, {1, 0, 5, ARM64_INSN_FACGE_SCALAR}, {1, 1, 2, ARM64_INSN_FABD_SCALAR}, {1, 1, 4, ARM64_INSN_FCMGT_SCALAR}, {1, 1, 5, ARM64_INSN_FACGT_SCALAR},
    };
    struct arm64_decoded_insn decoded;

    for (size_t index = 0; index < sizeof(integer_operations) / sizeof(integer_operations[0]); index++)
    {
        for (size_t size = 0; size < 4; size++)
        {
            uint32_t raw = 0x5E200400U | ((uint32_t)integer_operations[index].u << 29) | ((uint32_t)size << 22) | ((uint32_t)integer_operations[index].opcode << 11) | 0x20064U;

            if (!(integer_operations[index].valid_sizes & (1U << size)))
            {
                decode_status_is(raw, ARM64_DECODE_UNALLOCATED);
                continue;
            }

            decoded = decode_ok(raw);
            CHECK(decoded.instruction == integer_operations[index].instruction);
            CHECK(decoded.operand_width == (8U << size));
            CHECK(decoded.element_width == (8U << size));
            CHECK(decoded.rd == 4);
            CHECK(decoded.rn == 3);
            CHECK(decoded.rm == 2);
        }
    }

    for (size_t index = 0; index < sizeof(fp_operations) / sizeof(fp_operations[0]); index++)
    {
        for (size_t size = 0; size < 2; size++)
        {
            uint32_t raw = 0x5E200400U | ((uint32_t)fp_operations[index].u << 29) | ((uint32_t)((fp_operations[index].alternate << 1) | size) << 22) | ((uint32_t)fp_operations[index].opcode << 11) | 0x20064U;

            decoded = decode_ok(raw);
            CHECK(decoded.instruction == fp_operations[index].instruction);
            CHECK(decoded.operand_width == (32U << size));
            CHECK(decoded.element_width == (32U << size));
        }
    }

    for (size_t index = 0; index < sizeof(fp16_operations) / sizeof(fp16_operations[0]); index++)
    {
        uint32_t raw = 0x5E400400U | ((uint32_t)fp16_operations[index].u << 29) | ((uint32_t)fp16_operations[index].alternate << 23) | ((uint32_t)fp16_operations[index].opcode << 11) | 0x20064U;

        decoded = decode_ok(raw);
        CHECK(decoded.instruction == fp16_operations[index].instruction);
        CHECK(decoded.operand_width == 16);
        CHECK(decoded.element_width == 16);
    }

    for (size_t index = 0; index < 2; index++)
    {
        for (size_t opcode = 16; opcode < 32; opcode++)
        {
            for (size_t size = 0; size < 4; size++)
            {
                uint32_t raw = 0x5E000400U | ((uint32_t)index << 29) | ((uint32_t)size << 22) | ((uint32_t)opcode << 11) | 0x20064U;

                if (!index || (opcode != 16 && opcode != 17) || size == 0 || size == 3)
                {
                    decode_status_is(raw, ARM64_DECODE_UNALLOCATED);
                    continue;
                }

                decoded = decode_ok(raw);
                CHECK(decoded.instruction == (opcode == 17 ? ARM64_INSN_SQRDMLSH_SCALAR : ARM64_INSN_SQRDMLAH_SCALAR));
                CHECK(decoded.operand_width == (8U << size));
                CHECK(decoded.element_width == (8U << size));
            }
        }
    }

    decode_status_is(0x5E428420U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x5E203C64U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x5E401464U, ARM64_DECODE_UNALLOCATED);
}

static void test_scalar_fp_3source(void)
{
    static const struct
    {
        uint32_t bits;
        enum arm64_instruction instruction;
    } operations[] = {
        {0x00000000U, ARM64_INSN_FMADD_SCALAR},
        {0x00008000U, ARM64_INSN_FMSUB_SCALAR},
        {0x00200000U, ARM64_INSN_FNMADD_SCALAR},
        {0x00208000U, ARM64_INSN_FNMSUB_SCALAR},
    };
    static const struct
    {
        uint32_t bits;
        uint8_t width;
    } shapes[] = {
        {0x00000000U, 32},
        {0x00400000U, 64},
        {0x00C00000U, 16},
    };
    struct arm64_decoded_insn decoded;

    for (size_t operation_index = 0; operation_index < sizeof(operations) / sizeof(operations[0]); operation_index++)
    {
        for (size_t shape_index = 0; shape_index < sizeof(shapes) / sizeof(shapes[0]); shape_index++)
        {
            decoded = decode_ok(0x1F041000U | operations[operation_index].bits | shapes[shape_index].bits);
            CHECK(decoded.instruction == operations[operation_index].instruction);
            CHECK(decoded.operand_width == shapes[shape_index].width);
            CHECK(decoded.element_width == shapes[shape_index].width);
            CHECK(decoded.rd == 0);
            CHECK(decoded.rn == 0);
            CHECK(decoded.ra == 4);
            CHECK(decoded.rm == 4);
        }

        decode_status_is(0x1F841000U, ARM64_DECODE_UNALLOCATED);
    }

    decode_status_is(0x1F821020U, ARM64_DECODE_UNALLOCATED);
}

static void test_vector(void)
{
    static const struct
    {
        uint32_t bits;
        enum arm64_instruction instruction;
    } logical_operations[] = {
        {0x00000000U, ARM64_INSN_AND_VECTOR}, {0x00400000U, ARM64_INSN_BIC_VECTOR}, {0x00800000U, ARM64_INSN_ORR_VECTOR}, {0x00C00000U, ARM64_INSN_ORN_VECTOR}, {0x20000000U, ARM64_INSN_EOR_VECTOR}, {0x20400000U, ARM64_INSN_BSL_VECTOR}, {0x20800000U, ARM64_INSN_BIT_VECTOR}, {0x20C00000U, ARM64_INSN_BIF_VECTOR},
    };
    static const struct
    {
        uint32_t bits;
        uint8_t operand_width;
    } logical_shapes[] = {
        {0x00000000U, 64},
        {0x40000000U, 128},
    };
    static const struct
    {
        uint8_t u;
        uint8_t opcode;
        uint8_t valid_sizes;
        enum arm64_instruction instruction;
    } integer_3same_operations[] = {
        {0, 0, 0x7, ARM64_INSN_SHADD_VECTOR}, {0, 1, 0xF, ARM64_INSN_SQADD_VECTOR}, {0, 2, 0x7, ARM64_INSN_SRHADD_VECTOR}, {0, 4, 0x7, ARM64_INSN_SHSUB_VECTOR}, {0, 5, 0xF, ARM64_INSN_SQSUB_VECTOR}, {0, 6, 0xF, ARM64_INSN_CMGT_VECTOR}, {0, 7, 0xF, ARM64_INSN_CMGE_VECTOR}, {0, 8, 0xF, ARM64_INSN_SSHL_VECTOR}, {0, 9, 0xF, ARM64_INSN_SQSHL_VECTOR}, {0, 10, 0xF, ARM64_INSN_SRSHL_VECTOR}, {0, 11, 0xF, ARM64_INSN_SQRSHL_VECTOR}, {0, 12, 0x7, ARM64_INSN_SMAX_VECTOR}, {0, 13, 0x7, ARM64_INSN_SMIN_VECTOR}, {0, 14, 0x7, ARM64_INSN_SABD_VECTOR}, {0, 15, 0x7, ARM64_INSN_SABA_VECTOR}, {0, 16, 0xF, ARM64_INSN_ADD_VECTOR}, {0, 17, 0xF, ARM64_INSN_CMTST_VECTOR}, {0, 18, 0x7, ARM64_INSN_MLA_VECTOR}, {0, 19, 0x7, ARM64_INSN_MUL_VECTOR},  {0, 20, 0x7, ARM64_INSN_SMAXP_VECTOR}, {0, 21, 0x7, ARM64_INSN_SMINP_VECTOR}, {0, 22, 0x6, ARM64_INSN_SQDMULH_VECTOR},  {0, 23, 0xF, ARM64_INSN_ADDP_VECTOR},
        {1, 0, 0x7, ARM64_INSN_UHADD_VECTOR}, {1, 1, 0xF, ARM64_INSN_UQADD_VECTOR}, {1, 2, 0x7, ARM64_INSN_URHADD_VECTOR}, {1, 4, 0x7, ARM64_INSN_UHSUB_VECTOR}, {1, 5, 0xF, ARM64_INSN_UQSUB_VECTOR}, {1, 6, 0xF, ARM64_INSN_CMHI_VECTOR}, {1, 7, 0xF, ARM64_INSN_CMHS_VECTOR}, {1, 8, 0xF, ARM64_INSN_USHL_VECTOR}, {1, 9, 0xF, ARM64_INSN_UQSHL_VECTOR}, {1, 10, 0xF, ARM64_INSN_URSHL_VECTOR}, {1, 11, 0xF, ARM64_INSN_UQRSHL_VECTOR}, {1, 12, 0x7, ARM64_INSN_UMAX_VECTOR}, {1, 13, 0x7, ARM64_INSN_UMIN_VECTOR}, {1, 14, 0x7, ARM64_INSN_UABD_VECTOR}, {1, 15, 0x7, ARM64_INSN_UABA_VECTOR}, {1, 16, 0xF, ARM64_INSN_SUB_VECTOR}, {1, 17, 0xF, ARM64_INSN_CMEQ_VECTOR},  {1, 18, 0x7, ARM64_INSN_MLS_VECTOR}, {1, 19, 0x1, ARM64_INSN_PMUL_VECTOR}, {1, 20, 0x7, ARM64_INSN_UMAXP_VECTOR}, {1, 21, 0x7, ARM64_INSN_UMINP_VECTOR}, {1, 22, 0x6, ARM64_INSN_SQRDMULH_VECTOR},
    };
    static const struct
    {
        uint32_t base;
        enum arm64_instruction instruction;
    } fhm_operations[] = {
        {0x0E20EC00U, ARM64_INSN_FMLAL_VECTOR},
        {0x0EA0EC00U, ARM64_INSN_FMLSL_VECTOR},
        {0x2E20CC00U, ARM64_INSN_FMLAL2_VECTOR},
        {0x2EA0CC00U, ARM64_INSN_FMLSL2_VECTOR},
    };
    static const struct
    {
        uint32_t bits;
        enum arm64_instruction instruction;
    } permute_operations[] = {
        {0x00001000U, ARM64_INSN_UZP1_VECTOR}, {0x00002000U, ARM64_INSN_TRN1_VECTOR}, {0x00003000U, ARM64_INSN_ZIP1_VECTOR}, {0x00005000U, ARM64_INSN_UZP2_VECTOR}, {0x00006000U, ARM64_INSN_TRN2_VECTOR}, {0x00007000U, ARM64_INSN_ZIP2_VECTOR},
    };
    static const struct
    {
        uint32_t base;
        uint8_t operand_width;
        uint8_t element_width;
    } permute_shapes[] = {
        {0x0E000800U, 64, 8}, {0x4E000800U, 128, 8}, {0x0E400800U, 64, 16}, {0x4E400800U, 128, 16}, {0x0E800800U, 64, 32}, {0x4E800800U, 128, 32}, {0x4EC00800U, 128, 64},
    };
    static const struct
    {
        uint32_t fp16_base;
        uint32_t fp32_base;
        enum arm64_instruction instruction;
    } fp_vector_3reg_operations[] = {
        {0x0E401400U, 0x0E20D400U, ARM64_INSN_FADD_VECTOR},    {0x0EC01400U, 0x0EA0D400U, ARM64_INSN_FSUB_VECTOR},    {0x2E401C00U, 0x2E20DC00U, ARM64_INSN_FMUL_VECTOR}, {0x0E401C00U, 0x0E20DC00U, ARM64_INSN_FMULX_VECTOR},  {0x2E403C00U, 0x2E20FC00U, ARM64_INSN_FDIV_VECTOR},    {0x0E400C00U, 0x0E20CC00U, ARM64_INSN_FMLA_VECTOR},  {0x0EC00C00U, 0x0EA0CC00U, ARM64_INSN_FMLS_VECTOR},  {0x0E403400U, 0x0E20F400U, ARM64_INSN_FMAX_VECTOR},  {0x0EC03400U, 0x0EA0F400U, ARM64_INSN_FMIN_VECTOR},  {0x0E400400U, 0x0E20C400U, ARM64_INSN_FMAXNM_VECTOR}, {0x0EC00400U, 0x0EA0C400U, ARM64_INSN_FMINNM_VECTOR}, {0x2E401400U, 0x2E20D400U, ARM64_INSN_FADDP_VECTOR}, {0x2E403400U, 0x2E20F400U, ARM64_INSN_FMAXP_VECTOR},  {0x2EC03400U, 0x2EA0F400U, ARM64_INSN_FMINP_VECTOR},
        {0x2E400400U, 0x2E20C400U, ARM64_INSN_FMAXNMP_VECTOR}, {0x2EC00400U, 0x2EA0C400U, ARM64_INSN_FMINNMP_VECTOR}, {0x2EC01400U, 0x2EA0D400U, ARM64_INSN_FABD_VECTOR}, {0x0E403C00U, 0x0E20FC00U, ARM64_INSN_FRECPS_VECTOR}, {0x0EC03C00U, 0x0EA0FC00U, ARM64_INSN_FRSQRTS_VECTOR}, {0x0E402400U, 0x0E20E400U, ARM64_INSN_FCMEQ_VECTOR}, {0x2E402400U, 0x2E20E400U, ARM64_INSN_FCMGE_VECTOR}, {0x2EC02400U, 0x2EA0E400U, ARM64_INSN_FCMGT_VECTOR}, {0x2E402C00U, 0x2E20EC00U, ARM64_INSN_FACGE_VECTOR}, {0x2EC02C00U, 0x2EA0EC00U, ARM64_INSN_FACGT_VECTOR},  {0x0EC01C00U, 0x0EA0DC00U, ARM64_INSN_FAMAX_VECTOR},  {0x2EC01C00U, 0x2EA0DC00U, ARM64_INSN_FAMIN_VECTOR}, {0x2EC03C00U, 0x2EA0FC00U, ARM64_INSN_FSCALE_VECTOR},
    };
    static const struct
    {
        uint8_t fp16;
        uint32_t bits;
        uint8_t operand_width;
        uint8_t element_width;
    } fp_vector_shapes[] = {
        {1, 0x00000000U, 64, 16}, {1, 0x40000000U, 128, 16}, {0, 0x00000000U, 64, 32}, {0, 0x40000000U, 128, 32}, {0, 0x40400000U, 128, 64},
    };
    static const struct
    {
        uint32_t fp16_base;
        uint32_t fp32_base;
        enum arm64_instruction instruction;
    } fp_vector_2reg_operations[] = {
        {0x0EF8F800U, 0x0EA0F800U, ARM64_INSN_FABS_VECTOR},
        {0x2EF8F800U, 0x2EA0F800U, ARM64_INSN_FNEG_VECTOR},
        {0x2EF9F800U, 0x2EA1F800U, ARM64_INSN_FSQRT_VECTOR},
    };
    static const struct
    {
        uint32_t base;
        enum arm64_instruction instruction;
        uint8_t max_size;
    } rev_operations[] = {
        {0x0E200800U, ARM64_INSN_REV64_VECTOR, 2},
        {0x2E200800U, ARM64_INSN_REV32_VECTOR, 1},
        {0x0E201800U, ARM64_INSN_REV16_VECTOR, 0},
    };
    static const struct
    {
        uint32_t base;
        enum arm64_instruction instruction;
        uint8_t operand_width;
        uint8_t element_width;
    } fp_reduce_cases[] = {
        {0x5E30D800U, ARM64_INSN_FADDP_SCALAR_REDUCE, 32, 16}, {0x7E30D800U, ARM64_INSN_FADDP_SCALAR_REDUCE, 64, 32}, {0x7E70D800U, ARM64_INSN_FADDP_SCALAR_REDUCE, 128, 64}, {0x0E30C800U, ARM64_INSN_FMAXNMV_SCALAR_REDUCE, 64, 16}, {0x4E30C800U, ARM64_INSN_FMAXNMV_SCALAR_REDUCE, 128, 16}, {0x6E30C800U, ARM64_INSN_FMAXNMV_SCALAR_REDUCE, 128, 32}, {0x0EB0C800U, ARM64_INSN_FMINNMV_SCALAR_REDUCE, 64, 16}, {0x4EB0C800U, ARM64_INSN_FMINNMV_SCALAR_REDUCE, 128, 16}, {0x6EB0C800U, ARM64_INSN_FMINNMV_SCALAR_REDUCE, 128, 32}, {0x0E30F800U, ARM64_INSN_FMAXV_SCALAR_REDUCE, 64, 16}, {0x4E30F800U, ARM64_INSN_FMAXV_SCALAR_REDUCE, 128, 16}, {0x6E30F800U, ARM64_INSN_FMAXV_SCALAR_REDUCE, 128, 32}, {0x0EB0F800U, ARM64_INSN_FMINV_SCALAR_REDUCE, 64, 16}, {0x4EB0F800U, ARM64_INSN_FMINV_SCALAR_REDUCE, 128, 16}, {0x6EB0F800U, ARM64_INSN_FMINV_SCALAR_REDUCE, 128, 32},
    };
    static const struct
    {
        uint32_t base;
        enum arm64_instruction instruction;
    } integer_reduce_cases[] = {
        {0x0E31B800U, ARM64_INSN_ADDV_VECTOR}, {0x0E303800U, ARM64_INSN_SADDLV_VECTOR}, {0x2E303800U, ARM64_INSN_UADDLV_VECTOR}, {0x0E30A800U, ARM64_INSN_SMAXV_VECTOR}, {0x0E31A800U, ARM64_INSN_SMINV_VECTOR}, {0x2E30A800U, ARM64_INSN_UMAXV_VECTOR}, {0x2E31A800U, ARM64_INSN_UMINV_VECTOR},
    };
    static const struct
    {
        uint32_t vector_base;
        uint32_t scalar_base;
        enum arm64_instruction vector_instruction;
        enum arm64_instruction vector2_instruction;
        enum arm64_instruction scalar_instruction;
    } narrow_cases[] = {
        {0x0E212800U, 0, ARM64_INSN_XTN_VECTOR, ARM64_INSN_XTN2_VECTOR, ARM64_INSN_UNKNOWN},
        {0x0E214800U, 0x5E214800U, ARM64_INSN_SQXTN_VECTOR, ARM64_INSN_SQXTN2_VECTOR, ARM64_INSN_SQXTN_SCALAR},
        {0x2E214800U, 0x7E214800U, ARM64_INSN_UQXTN_VECTOR, ARM64_INSN_UQXTN2_VECTOR, ARM64_INSN_UQXTN_SCALAR},
        {0x2E212800U, 0x7E212800U, ARM64_INSN_SQXTUN_VECTOR, ARM64_INSN_SQXTUN2_VECTOR, ARM64_INSN_SQXTUN_SCALAR},
    };
    static const struct
    {
        uint32_t bits;
        enum arm64_instruction vector_instruction;
        enum arm64_instruction scalar_instruction;
    } compare_zero_relations[] = {
        {0x0000D000U, ARM64_INSN_FCMEQ_ZERO_VECTOR, ARM64_INSN_FCMEQ_ZERO_SCALAR}, {0x2000C000U, ARM64_INSN_FCMGE_ZERO_VECTOR, ARM64_INSN_FCMGE_ZERO_SCALAR}, {0x0000C000U, ARM64_INSN_FCMGT_ZERO_VECTOR, ARM64_INSN_FCMGT_ZERO_SCALAR}, {0x2000D000U, ARM64_INSN_FCMLE_ZERO_VECTOR, ARM64_INSN_FCMLE_ZERO_SCALAR}, {0x0000E000U, ARM64_INSN_FCMLT_ZERO_VECTOR, ARM64_INSN_FCMLT_ZERO_SCALAR},
    };
    static const struct
    {
        uint32_t bits;
        uint8_t scalar;
        uint8_t operand_width;
        uint8_t element_width;
    } compare_zero_shapes[] = {
        {0x0EF80800U, 0, 64, 16}, {0x4EF80800U, 0, 128, 16}, {0x0EA00800U, 0, 64, 32}, {0x4EA00800U, 0, 128, 32}, {0x4EE00800U, 0, 128, 64}, {0x5EF80800U, 1, 16, 16}, {0x5EA00800U, 1, 32, 32}, {0x5EE00800U, 1, 64, 64},
    };
    struct arm64_decoded_insn decoded = decode_ok(0x4F00E640U);

    CHECK(decoded.instruction == ARM64_INSN_MOVI_VECTOR_IMMEDIATE);
    CHECK(decoded.expanded_immediate == 0x1212121212121212ULL);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.element_width == 8);

    decoded = decode_ok(0x6F00E423U);
    CHECK(decoded.instruction == ARM64_INSN_MOVI_VECTOR_IMMEDIATE);
    CHECK(decoded.expanded_immediate == 0xFFULL);
    CHECK(decoded.element_width == 64);

    decoded = decode_ok(0x4F001640U);
    CHECK(decoded.instruction == ARM64_INSN_ORR_VECTOR_IMMEDIATE);
    CHECK(decoded.expanded_immediate == 0x0000001200000012ULL);
    CHECK(decoded.element_width == 32);

    decoded = decode_ok(0x6F001640U);
    CHECK(decoded.instruction == ARM64_INSN_BIC_VECTOR_IMMEDIATE);
    CHECK(decoded.expanded_immediate == 0x0000001200000012ULL);

    decoded = decode_ok(0x6F000640U);
    CHECK(decoded.instruction == ARM64_INSN_MVNI_VECTOR_IMMEDIATE);
    CHECK(decoded.expanded_immediate == 0x0000001200000012ULL);

    decoded = decode_ok(0x0F03F600U);
    CHECK(decoded.instruction == ARM64_INSN_FMOV_VECTOR_IMMEDIATE);
    CHECK(decoded.expanded_immediate == 0x3F8000003F800000ULL);
    CHECK(decoded.element_width == 32);

    decoded = decode_ok(0x6F03F600U);
    CHECK(decoded.instruction == ARM64_INSN_FMOV_VECTOR_IMMEDIATE);
    CHECK(decoded.expanded_immediate == 0x3FF0000000000000ULL);
    CHECK(decoded.element_width == 64);

    decode_status_is(0x2F03F600U, ARM64_DECODE_UNALLOCATED);

    decoded = decode_ok(0x4E0C0462U);
    CHECK(decoded.instruction == ARM64_INSN_DUP_ELEMENT_VECTOR);
    CHECK(decoded.element_width == 32);
    CHECK(decoded.lane_index == 1);

    decoded = decode_ok(0x6E0E6462U);
    CHECK(decoded.instruction == ARM64_INSN_INS_ELEMENT_VECTOR);
    CHECK(decoded.element_width == 16);
    CHECK(decoded.lane_index == 3);
    CHECK(decoded.source_lane_index == 6);

    decoded = decode_ok(0x6E1E6F3BU);
    CHECK(decoded.instruction == ARM64_INSN_INS_ELEMENT_VECTOR);
    CHECK(decoded.element_width == 16);
    CHECK(decoded.lane_index == 7);
    CHECK(decoded.source_lane_index == 6);

    decode_status_is(0x0E180462U, ARM64_DECODE_UNALLOCATED);

    decoded = decode_ok(0x4E1C2DEEU);
    CHECK(decoded.instruction == ARM64_INSN_SMOV_VECTOR_TO_GPR);
    CHECK(decoded.element_width == 32);
    CHECK(decoded.lane_index == 3);
    CHECK(decoded.operand_width == 64);

    decoded = decode_ok(0x0E228420U);
    CHECK(decoded.instruction == ARM64_INSN_ADD_VECTOR);
    CHECK(decoded.element_width == 8);
    CHECK(decoded.operand_width == 64);

    decoded = decode_ok(0x4EF48672U);
    CHECK(decoded.instruction == ARM64_INSN_ADD_VECTOR);
    CHECK(decoded.element_width == 64);
    CHECK(decoded.operand_width == 128);

    decoded = decode_ok(0x6E2834E6U);
    CHECK(decoded.instruction == ARM64_INSN_CMHI_VECTOR);
    CHECK(decoded.element_width == 8);

    for (size_t operation_index = 0; operation_index < sizeof(integer_3same_operations) / sizeof(integer_3same_operations[0]); operation_index++)
    {
        for (size_t q_index = 0; q_index < 2; q_index++)
        {
            for (size_t size_index = 0; size_index < 4; size_index++)
            {
                uint32_t raw = 0x0E200400U | ((uint32_t)q_index << 30) | ((uint32_t)integer_3same_operations[operation_index].u << 29) | ((uint32_t)size_index << 22) | ((uint32_t)integer_3same_operations[operation_index].opcode << 11) | 0x20064U;

                if (!(integer_3same_operations[operation_index].valid_sizes & (1U << size_index)) || (!q_index && size_index == 3))
                {
                    decode_status_is(raw, ARM64_DECODE_UNALLOCATED);
                    continue;
                }

                decoded = decode_ok(raw);
                CHECK(decoded.instruction == integer_3same_operations[operation_index].instruction);
                CHECK(decoded.rd == 4);
                CHECK(decoded.rn == 3);
                CHECK(decoded.rm == 2);
                CHECK(decoded.operand_width == (q_index ? 128 : 64));
                CHECK(decoded.element_width == (8U << size_index));
            }
        }
    }

    decode_status_is(0x2E20BC64U, ARM64_DECODE_UNALLOCATED);

    decoded = decode_ok(0x4E20D400U);
    CHECK(decoded.instruction == ARM64_INSN_FADD_VECTOR);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.element_width == 32);

    for (size_t operation_index = 0; operation_index < sizeof(logical_operations) / sizeof(logical_operations[0]); operation_index++)
    {
        for (size_t shape_index = 0; shape_index < sizeof(logical_shapes) / sizeof(logical_shapes[0]); shape_index++)
        {
            decoded = decode_ok(0x0E201C00U | logical_operations[operation_index].bits | logical_shapes[shape_index].bits | 0x30041U);
            CHECK(decoded.instruction == logical_operations[operation_index].instruction);
            CHECK(decoded.rd == 1);
            CHECK(decoded.rn == 2);
            CHECK(decoded.rm == 3);
            CHECK(decoded.operand_width == logical_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == 8);
        }
    }

    decoded = decode_ok(0x2E631C41U);
    CHECK(decoded.instruction == ARM64_INSN_BSL_VECTOR);
    CHECK(decoded.rd == 1);
    CHECK(decoded.rn == 2);
    CHECK(decoded.rm == 3);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 8);

    for (size_t operation_index = 0; operation_index < sizeof(permute_operations) / sizeof(permute_operations[0]); operation_index++)
    {
        for (size_t shape_index = 0; shape_index < sizeof(permute_shapes) / sizeof(permute_shapes[0]); shape_index++)
        {
            decoded = decode_ok(permute_shapes[shape_index].base | permute_operations[operation_index].bits | 0x50081U);
            CHECK(decoded.instruction == permute_operations[operation_index].instruction);
            CHECK(decoded.rd == 1);
            CHECK(decoded.rn == 4);
            CHECK(decoded.rm == 5);
            CHECK(decoded.operand_width == permute_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == permute_shapes[shape_index].element_width);
        }
    }

    decoded = decode_ok(0x0E853881U);
    CHECK(decoded.instruction == ARM64_INSN_ZIP1_VECTOR);
    CHECK(decoded.rd == 1);
    CHECK(decoded.rn == 4);
    CHECK(decoded.rm == 5);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 32);
    decode_status_is(0x0EC23820U, ARM64_DECODE_UNALLOCATED);

    for (size_t operation_index = 0; operation_index < sizeof(fp_vector_3reg_operations) / sizeof(fp_vector_3reg_operations[0]); operation_index++)
    {
        for (size_t shape_index = 0; shape_index < sizeof(fp_vector_shapes) / sizeof(fp_vector_shapes[0]); shape_index++)
        {
            uint32_t base = fp_vector_shapes[shape_index].fp16 ? fp_vector_3reg_operations[operation_index].fp16_base : fp_vector_3reg_operations[operation_index].fp32_base;

            decoded = decode_ok(base | fp_vector_shapes[shape_index].bits | 0x20064U);
            CHECK(decoded.instruction == fp_vector_3reg_operations[operation_index].instruction);
            CHECK(decoded.rd == 4);
            CHECK(decoded.rn == 3);
            CHECK(decoded.rm == 2);
            CHECK(decoded.operand_width == fp_vector_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == fp_vector_shapes[shape_index].element_width);
        }
    }

    decoded = decode_ok(0x0E22CC64U);
    CHECK(decoded.instruction == ARM64_INSN_FMLA_VECTOR);
    CHECK(decoded.rd == 4);
    CHECK(decoded.rn == 3);
    CHECK(decoded.rm == 2);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 32);
    decode_status_is(0x0E62CC20U, ARM64_DECODE_UNALLOCATED);

    for (size_t operation_index = 0; operation_index < sizeof(fhm_operations) / sizeof(fhm_operations[0]); operation_index++)
    {
        for (size_t q_index = 0; q_index < 2; q_index++)
        {
            uint32_t raw = fhm_operations[operation_index].base | ((uint32_t)q_index << 30) | 0x20064U;

            decoded = decode_ok(raw);
            CHECK(decoded.instruction == fhm_operations[operation_index].instruction);
            CHECK(decoded.rd == 4);
            CHECK(decoded.rn == 3);
            CHECK(decoded.rm == 2);
            CHECK(decoded.operand_width == (q_index ? 128 : 64));
            CHECK(decoded.element_width == 16);
        }

        decode_status_is(fhm_operations[operation_index].base | 0x00420064U, ARM64_DECODE_UNALLOCATED);
    }

    for (size_t operation_index = 0; operation_index < sizeof(fp_vector_2reg_operations) / sizeof(fp_vector_2reg_operations[0]); operation_index++)
    {
        for (size_t shape_index = 0; shape_index < sizeof(fp_vector_shapes) / sizeof(fp_vector_shapes[0]); shape_index++)
        {
            uint32_t base = fp_vector_shapes[shape_index].fp16 ? fp_vector_2reg_operations[operation_index].fp16_base : fp_vector_2reg_operations[operation_index].fp32_base;

            decoded = decode_ok(base | fp_vector_shapes[shape_index].bits | 0x64U);
            CHECK(decoded.instruction == fp_vector_2reg_operations[operation_index].instruction);
            CHECK(decoded.rd == 4);
            CHECK(decoded.rn == 3);
            CHECK(decoded.operand_width == fp_vector_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == fp_vector_shapes[shape_index].element_width);
        }
    }

    decoded = decode_ok(0x2EA2E422U);
    CHECK(decoded.instruction == ARM64_INSN_FCMGT_VECTOR);
    CHECK(decoded.rd == 2);
    CHECK(decoded.rn == 1);
    CHECK(decoded.rm == 2);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 32);
    decode_status_is(0x2EE2E420U, ARM64_DECODE_UNALLOCATED);

    for (size_t operation_index = 0; operation_index < sizeof(rev_operations) / sizeof(rev_operations[0]); operation_index++)
    {
        for (size_t shape_index = 0; shape_index <= rev_operations[operation_index].max_size; shape_index++)
        {
            for (size_t q_index = 0; q_index < 2; q_index++)
            {
                decoded = decode_ok(rev_operations[operation_index].base | ((uint32_t)shape_index << 22) | ((uint32_t)q_index << 30) | 0x64U);
                CHECK(decoded.instruction == rev_operations[operation_index].instruction);
                CHECK(decoded.rd == 4);
                CHECK(decoded.rn == 3);
                CHECK(decoded.operand_width == (q_index ? 128 : 64));
                CHECK(decoded.element_width == (8U << shape_index));
            }
        }
    }

    decode_status_is(0x0EE00820U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2EA00820U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x0E601820U, ARM64_DECODE_UNALLOCATED);

    for (size_t operation_index = 0; operation_index < sizeof(integer_reduce_cases) / sizeof(integer_reduce_cases[0]); operation_index++)
    {
        for (size_t q_index = 0; q_index < 2; q_index++)
        {
            for (size_t size_index = 0; size_index < 3; size_index++)
            {
                uint32_t raw = integer_reduce_cases[operation_index].base | ((uint32_t)q_index << 30) | ((uint32_t)size_index << 22) | 0x63U;

                if (size_index == 2 && !q_index)
                {
                    decode_status_is(raw, ARM64_DECODE_UNALLOCATED);
                    continue;
                }

                decoded = decode_ok(raw);
                CHECK(decoded.instruction == integer_reduce_cases[operation_index].instruction);
                CHECK(decoded.rd == 3);
                CHECK(decoded.rn == 3);
                CHECK(decoded.operand_width == (q_index ? 128 : 64));
                CHECK(decoded.element_width == (8U << size_index));
            }
        }
        decode_status_is(integer_reduce_cases[operation_index].base | 0x00C00063U, ARM64_DECODE_UNALLOCATED);
    }

    for (size_t operation_index = 0; operation_index < sizeof(narrow_cases) / sizeof(narrow_cases[0]); operation_index++)
    {
        for (size_t q_index = 0; q_index < 2; q_index++)
        {
            for (size_t size_index = 0; size_index < 3; size_index++)
            {
                decoded = decode_ok(narrow_cases[operation_index].vector_base | ((uint32_t)q_index << 30) | ((uint32_t)size_index << 22) | 0x63U);
                CHECK(decoded.instruction == (q_index ? narrow_cases[operation_index].vector2_instruction : narrow_cases[operation_index].vector_instruction));
                CHECK(decoded.rd == 3);
                CHECK(decoded.rn == 3);
                CHECK(decoded.operand_width == (q_index ? 128 : 64));
                CHECK(decoded.element_width == (16U << size_index));
            }
        }
        decode_status_is(narrow_cases[operation_index].vector_base | 0x00C00063U, ARM64_DECODE_UNALLOCATED);

        if (narrow_cases[operation_index].scalar_base)
        {
            for (size_t size_index = 0; size_index < 3; size_index++)
            {
                decoded = decode_ok(narrow_cases[operation_index].scalar_base | ((uint32_t)size_index << 22) | 0x63U);
                CHECK(decoded.instruction == narrow_cases[operation_index].scalar_instruction);
                CHECK(decoded.rd == 3);
                CHECK(decoded.rn == 3);
                CHECK(decoded.operand_width == (8U << size_index));
                CHECK(decoded.element_width == (16U << size_index));
            }
            decode_status_is(narrow_cases[operation_index].scalar_base | 0x00C00063U, ARM64_DECODE_UNALLOCATED);
        }
    }
    decode_status_is(0x5E212863U, ARM64_DECODE_UNALLOCATED);

    for (size_t operation_index = 0; operation_index < sizeof(fp_reduce_cases) / sizeof(fp_reduce_cases[0]); operation_index++)
    {
        decoded = decode_ok(fp_reduce_cases[operation_index].base | 0x64U);
        CHECK(decoded.instruction == fp_reduce_cases[operation_index].instruction);
        CHECK(decoded.rd == 4);
        CHECK(decoded.rn == 3);
        CHECK(decoded.operand_width == fp_reduce_cases[operation_index].operand_width);
        CHECK(decoded.element_width == fp_reduce_cases[operation_index].element_width);
    }

    decode_status_is(0x5E70D820U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2E30C820U, ARM64_DECODE_UNALLOCATED);

    for (size_t relation_index = 0; relation_index < sizeof(compare_zero_relations) / sizeof(compare_zero_relations[0]); relation_index++)
    {
        for (size_t shape_index = 0; shape_index < sizeof(compare_zero_shapes) / sizeof(compare_zero_shapes[0]); shape_index++)
        {
            decoded = decode_ok(compare_zero_shapes[shape_index].bits | compare_zero_relations[relation_index].bits | 0x61U);
            CHECK(decoded.instruction == (compare_zero_shapes[shape_index].scalar ? compare_zero_relations[relation_index].scalar_instruction : compare_zero_relations[relation_index].vector_instruction));
            CHECK(decoded.rd == 1);
            CHECK(decoded.rn == 3);
            CHECK(decoded.operand_width == compare_zero_shapes[shape_index].operand_width);
            CHECK(decoded.element_width == compare_zero_shapes[shape_index].element_width);
        }
    }

    decode_status_is(0x0EE0D820U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2EA0E820U, ARM64_DECODE_UNSUPPORTED);

    decoded = decode_ok(0x0EA0E861U);
    CHECK(decoded.instruction == ARM64_INSN_FCMLT_ZERO_VECTOR);
    CHECK(decoded.rd == 1);
    CHECK(decoded.rn == 3);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.element_width == 32);

    decoded = decode_ok(0x4EA0F800U);
    CHECK(decoded.instruction == ARM64_INSN_FABS_VECTOR);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.element_width == 32);

    decoded = decode_ok(0x4F2355ACU);
    CHECK(decoded.instruction == ARM64_INSN_SHL_VECTOR_IMMEDIATE);
    CHECK(decoded.element_width == 32);
    CHECK(decoded.immediate == 3);

    decoded = decode_ok(0x6F790630U);
    CHECK(decoded.instruction == ARM64_INSN_USHR_VECTOR_IMMEDIATE);
    CHECK(decoded.element_width == 64);
    CHECK(decoded.immediate == 7);

    decode_status_is(0x4F5650CBU, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x0F610271U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x9B760A29U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x9B6EF8EAU, ARM64_DECODE_UNSUPPORTED);
}

static int run_tests(void)
{
    test_dispatch();
    test_system();
    test_control_flow();
    test_load_store();
    test_fp_simd_instruction_identities();
    test_scalar_fp();
    test_scalar_copy();
    test_fp_by_element();
    test_fhm_by_element();
    test_extra_by_element();
    test_fcma();
    test_vector_3same_extra();
    test_scalar_3same();
    test_scalar_fp_3source();
    test_fp_conversions();
    test_vector();
    return failures;
}

#ifdef ARM64_DECODE_FREESTANDING_TEST
void *memset(void *destination, int value, size_t count)
{
    unsigned char *bytes = destination;

    for (size_t index = 0; index < count; index++) bytes[index] = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *destination_bytes = destination;
    const unsigned char *source_bytes = source;

    for (size_t index = 0; index < count; index++) destination_bytes[index] = source_bytes[index];
    return destination;
}

#if defined(__x86_64__)
__attribute__((noreturn, force_align_arg_pointer))
#else
__attribute__((noreturn))
#endif
void _start(void)
{
    long status = run_tests();

    __asm__ volatile("syscall" : : "a"(60L), "D"(status) : "rcx", "r11", "memory");
    __builtin_unreachable();
}
#else
int main(void)
{
    return run_tests();
}
#endif