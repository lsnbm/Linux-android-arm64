#include "arm64_decode.h"

#include <stddef.h>

static int failures;

#define CHECK(EXPRESSION)              \
    do                                 \
    {                                  \
        if (!(EXPRESSION)) failures++; \
    } while (0)

static struct arm64_decoded_insn decode_ok(arm64_u32 raw)
{
    struct arm64_decoded_insn decoded = arm64_decode_insn(raw);

    CHECK(decoded.status == ARM64_DECODE_OK);
    return decoded;
}

static void decode_status_is(arm64_u32 raw, enum arm64_decode_status expected)
{
    struct arm64_decoded_insn decoded = arm64_decode_insn(raw);

    CHECK(decoded.status == expected);
}

static void test_fp_conversions(void)
{
    static const struct
    {
        arm64_u32 base;
        enum arm64_simd_operation operation;
        enum arm64_fp_rounding_mode rounding_mode;
    } simd_operations[] = {
        {0x0E21A800U, ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD, ARM64_FP_ROUND_NEAREST_EVEN}, {0x2E21A800U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD, ARM64_FP_ROUND_NEAREST_EVEN}, {0x0EA1A800U, ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD, ARM64_FP_ROUND_PLUS_INFINITY}, {0x2EA1A800U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD, ARM64_FP_ROUND_PLUS_INFINITY}, {0x0E21B800U, ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD, ARM64_FP_ROUND_MINUS_INFINITY}, {0x2E21B800U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD, ARM64_FP_ROUND_MINUS_INFINITY}, {0x0E21C800U, ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD, ARM64_FP_ROUND_NEAREST_AWAY}, {0x2E21C800U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD, ARM64_FP_ROUND_NEAREST_AWAY}, {0x0EA1B800U, ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD, ARM64_FP_ROUND_ZERO}, {0x2EA1B800U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD, ARM64_FP_ROUND_ZERO}, {0x0E21D800U, ARM64_SIMD_OP_SCVTF_SIMD, ARM64_FP_ROUND_NONE}, {0x2E21D800U, ARM64_SIMD_OP_UCVTF_SIMD, ARM64_FP_ROUND_NONE},
    };
    static const struct
    {
        arm64_u32 bits;
        arm64_u8 operand_width;
        arm64_u8 element_width;
    } simd_shapes[] = {
        {0x50000000U, 32, 32}, {0x50400000U, 64, 64}, {0x00000000U, 64, 32}, {0x40000000U, 128, 32}, {0x40400000U, 128, 64},
    };
    static const struct
    {
        arm64_u32 base;
        enum arm64_simd_operation operation;
        enum arm64_fp_rounding_mode rounding_mode;
    } gpr_operations[] = {
        {0x1E200000U, ARM64_SIMD_OP_FCVT_TO_SIGNED, ARM64_FP_ROUND_NEAREST_EVEN}, {0x1E210000U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED, ARM64_FP_ROUND_NEAREST_EVEN}, {0x1E280000U, ARM64_SIMD_OP_FCVT_TO_SIGNED, ARM64_FP_ROUND_PLUS_INFINITY}, {0x1E290000U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED, ARM64_FP_ROUND_PLUS_INFINITY}, {0x1E300000U, ARM64_SIMD_OP_FCVT_TO_SIGNED, ARM64_FP_ROUND_MINUS_INFINITY}, {0x1E310000U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED, ARM64_FP_ROUND_MINUS_INFINITY}, {0x1E380000U, ARM64_SIMD_OP_FCVT_TO_SIGNED, ARM64_FP_ROUND_ZERO}, {0x1E390000U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED, ARM64_FP_ROUND_ZERO}, {0x1E240000U, ARM64_SIMD_OP_FCVT_TO_SIGNED, ARM64_FP_ROUND_NEAREST_AWAY}, {0x1E250000U, ARM64_SIMD_OP_FCVT_TO_UNSIGNED, ARM64_FP_ROUND_NEAREST_AWAY},
    };
    static const struct
    {
        arm64_u32 bits;
        arm64_u8 operand_width;
        arm64_u8 element_width;
    } gpr_shapes[] = {
        {0x00000000U, 32, 32},
        {0x80000000U, 64, 32},
        {0x00400000U, 32, 64},
        {0x80400000U, 64, 64},
    };
    struct arm64_decoded_insn decoded;
    size_t operation_index;
    size_t shape_index;

    decoded = decode_ok(0x5EA1B842U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_CONVERT);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD);
    CHECK(decoded.operands.simd.rounding_mode == ARM64_FP_ROUND_ZERO);
    CHECK(decoded.operand_width == 32);
    CHECK(decoded.operands.simd.element_width == 32);
    CHECK(decoded.rd == 2);
    CHECK(decoded.rn == 2);
    CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
    CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);
    CHECK(!(decoded.effects & ARM64_EFFECT_WRITE_GPR));

    for (operation_index = 0; operation_index < sizeof(simd_operations) / sizeof(simd_operations[0]); operation_index++)
    {
        for (shape_index = 0; shape_index < sizeof(simd_shapes) / sizeof(simd_shapes[0]); shape_index++)
        {
            decoded = decode_ok(simd_operations[operation_index].base | simd_shapes[shape_index].bits | 0x62U);
            CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_CONVERT);
            CHECK(decoded.operands.simd.operation == simd_operations[operation_index].operation);
            CHECK(decoded.operands.simd.rounding_mode == simd_operations[operation_index].rounding_mode);
            CHECK(decoded.operand_width == simd_shapes[shape_index].operand_width);
            CHECK(decoded.operands.simd.element_width == simd_shapes[shape_index].element_width);
            CHECK(decoded.rd == 2);
            CHECK(decoded.rn == 3);
            CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
            CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);
            CHECK(!(decoded.effects & ARM64_EFFECT_READ_GPR));
            CHECK(!(decoded.effects & ARM64_EFFECT_WRITE_GPR));
        }
    }

    for (operation_index = 0; operation_index < sizeof(gpr_operations) / sizeof(gpr_operations[0]); operation_index++)
    {
        for (shape_index = 0; shape_index < sizeof(gpr_shapes) / sizeof(gpr_shapes[0]); shape_index++)
        {
            decoded = decode_ok(gpr_operations[operation_index].base | gpr_shapes[shape_index].bits | 0x62U);
            CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_CONVERT);
            CHECK(decoded.operands.simd.operation == gpr_operations[operation_index].operation);
            CHECK(decoded.operands.simd.rounding_mode == gpr_operations[operation_index].rounding_mode);
            CHECK(decoded.operand_width == gpr_shapes[shape_index].operand_width);
            CHECK(decoded.operands.simd.element_width == gpr_shapes[shape_index].element_width);
            CHECK(decoded.rd == 2);
            CHECK(decoded.rn == 3);
            CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
            CHECK(!(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD));
            CHECK(!(decoded.effects & ARM64_EFFECT_READ_GPR));
            CHECK(decoded.effects & ARM64_EFFECT_WRITE_GPR);
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
    CHECK(decoded.opcode == ARM64_OP_ADD_SUB_IMMEDIATE);
    CHECK(decoded.operation == ARM64_OPERATION_ADD);
    CHECK(decoded.rd == 0);
    CHECK(decoded.rn == 1);
    CHECK(decoded.operands.data.immediate == 1);

    decoded = decode_ok(0x8B020020U);
    CHECK(decoded.insn_class == ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER);
    CHECK(decoded.opcode == ARM64_OP_ADD_SUB_SHIFTED);
    CHECK(decoded.operation == ARM64_OPERATION_ADD);
    CHECK(decoded.rm == 2);

    decode_status_is(0xDAC10020U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xDAC11820U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xDAC123E0U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xDAC12020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0xDAC143E0U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xDAC147E0U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x0000C19FU, ARM64_DECODE_UNSUPPORTED);

    decoded = decode_ok(0x90000000U);
    CHECK(decoded.insn_class == ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE);
    CHECK(decoded.opcode == ARM64_OP_ADRP);
    CHECK(decoded.operands.pc_relative.offset == 0);

    decode_status_is(0x02000000U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x04000000U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0xC00800FFU, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x40000000U, ARM64_DECODE_UNALLOCATED);
    decoded = arm64_decode_insn(0x80000000U);
    CHECK(decoded.status == ARM64_DECODE_UNSUPPORTED);
    CHECK(decoded.insn_class == ARM64_INSN_CLASS_SME);

    CHECK(arm64_decode_insn_has_effect(0x10000000U, ARM64_EFFECT_PC_RELATIVE));
    CHECK(!arm64_decode_insn_has_effect(0xD503201FU, ARM64_EFFECT_PC_RELATIVE));
    CHECK(arm64_decode_insn_has_effect(0xD503233FU, ARM64_EFFECT_SYSTEM | ARM64_EFFECT_READ_GPR | ARM64_EFFECT_WRITE_GPR));
    CHECK(!arm64_decode_insn_has_effect(0x10000000U, 0));
}

static void test_system(void)
{
    struct arm64_decoded_insn decoded = decode_ok(0xD5033BBFU);
    static const struct
    {
        arm64_u32 raw;
        enum arm64_bti_type type;
    } bti_cases[] = {
        {0xD503241FU, ARM64_BTI_TYPE_NONE},
        {0xD503245FU, ARM64_BTI_TYPE_C},
        {0xD503249FU, ARM64_BTI_TYPE_J},
        {0xD50324DFU, ARM64_BTI_TYPE_JC},
    };
    unsigned int index;

    CHECK(decoded.raw == 0xD5033BBFU);
    CHECK(decoded.status == ARM64_DECODE_OK);
    CHECK(decoded.effects & ARM64_EFFECT_SYSTEM);
    CHECK(decoded.effects & ARM64_EFFECT_BARRIER);
    CHECK(decoded.opcode == ARM64_OP_BARRIER);
    CHECK(decoded.operands.system.operation == ARM64_SYSTEM_OP_DMB);
    CHECK(decoded.operands.system.option == 0xB);

    decoded = decode_ok(0xD4202460U);
    CHECK(decoded.opcode == ARM64_OP_EXCEPTION_GENERATION);
    CHECK(decoded.operands.system.operation == ARM64_SYSTEM_OP_BRK);
    CHECK(decoded.operands.system.immediate == 0x123);

    decoded = decode_ok(0xD503233FU);
    CHECK(decoded.opcode == ARM64_OP_HINT);
    CHECK(decoded.operands.system.operation == ARM64_SYSTEM_OP_PACIASP);
    CHECK(decoded.effects & ARM64_EFFECT_SYSTEM);
    CHECK(decoded.effects & ARM64_EFFECT_READ_GPR);
    CHECK(decoded.effects & ARM64_EFFECT_WRITE_GPR);
    CHECK(!(decoded.effects & ARM64_EFFECT_PC_RELATIVE));

    for (index = 0; index < sizeof(bti_cases) / sizeof(bti_cases[0]); index++)
    {
        decoded = decode_ok(bti_cases[index].raw);
        CHECK(decoded.opcode == ARM64_OP_HINT);
        CHECK(decoded.operands.system.operation == ARM64_SYSTEM_OP_BTI);
        CHECK(decoded.operands.system.option == bti_cases[index].type);
        CHECK(decoded.effects & ARM64_EFFECT_SYSTEM);
        CHECK(!(decoded.effects & ARM64_EFFECT_READ_GPR));
        CHECK(!(decoded.effects & ARM64_EFFECT_WRITE_GPR));
        CHECK(!(decoded.effects & ARM64_EFFECT_PC_RELATIVE));
    }
}

static void test_load_store(void)
{
    struct arm64_decoded_insn decoded = decode_ok(0xA8410440U);
    struct arm64_memory_address address;

    CHECK(decoded.opcode == ARM64_OP_LOAD_STORE_PAIR);
    CHECK(decoded.flags & ARM64_INSN_FLAG_NON_TEMPORAL);
    CHECK(decoded.flags & ARM64_INSN_FLAG_LOAD);
    CHECK(decoded.operands.load_store.offset == 16);
    CHECK(decoded.effects & ARM64_EFFECT_READ_MEMORY);
    CHECK(!(decoded.effects & ARM64_EFFECT_WRITE_MEMORY));
    CHECK(arm64_decode_memory_address(&decoded, 0, 0x1000, 0, &address));
    CHECK(address.address == 0x1010);
    CHECK(!address.writeback);

    decoded = decode_ok(0xA8C10440U);
    CHECK(arm64_decode_memory_address(&decoded, 0, 0x1000, 0, &address));
    CHECK(address.address == 0x1000);
    CHECK(address.writeback);
    CHECK(address.writeback_address == 0x1010);

    decoded = decode_ok(0xF84098E6U);
    CHECK(decoded.opcode == ARM64_OP_LOAD_STORE_SINGLE);
    CHECK(decoded.flags & ARM64_INSN_FLAG_UNPRIVILEGED);
    CHECK(decoded.flags & ARM64_INSN_FLAG_LOAD);
    CHECK(decoded.operands.load_store.offset == 9);

    decoded = decode_ok(0xF862D820U);
    CHECK(decoded.operands.load_store.address_mode == ARM64_ADDRESS_REGISTER_OFFSET);
    CHECK(arm64_decode_memory_address(&decoded, 0, 0x1000, 0xFFFFFFFFULL, &address));
    CHECK(address.address == 0xFF8);

    decoded = decode_ok(0xF8657883U);
    CHECK(arm64_decode_memory_address(&decoded, 0, 0x1000, 3, &address));
    CHECK(address.address == 0x1018);

    decoded = decode_ok(0xF9802020U);
    CHECK(decoded.opcode == ARM64_OP_PREFETCH);
    CHECK(decoded.effects == ARM64_EFFECT_PREFETCH);
    CHECK(arm64_decode_memory_address(&decoded, 0, 0x1000, 0, &address));
    CHECK(address.address == 0x1040);

    decoded = decode_ok(0xF8800020U);
    CHECK(decoded.opcode == ARM64_OP_PREFETCH);
    CHECK(decoded.operands.load_store.address_mode == ARM64_ADDRESS_UNSCALED_OFFSET);

    decoded = decode_ok(0xF8A26820U);
    CHECK(decoded.opcode == ARM64_OP_PREFETCH);
    CHECK(decoded.operands.load_store.address_mode == ARM64_ADDRESS_REGISTER_OFFSET);

    decoded = decode_ok(0xB93336D0U);
    CHECK(decoded.opcode == ARM64_OP_LOAD_STORE_SINGLE);
    CHECK(decoded.operands.load_store.address_mode == ARM64_ADDRESS_UNSIGNED_OFFSET);

    decoded = decode_ok(0x3D77D3FEU);
    CHECK(decoded.opcode == ARM64_OP_LOAD_STORE_SINGLE);
    CHECK(decoded.flags & ARM64_INSN_FLAG_FP);

    decoded = decode_ok(0xF9B314B8U);
    CHECK(decoded.opcode == ARM64_OP_PREFETCH);
    CHECK(decoded.operands.load_store.address_mode == ARM64_ADDRESS_UNSIGNED_OFFSET);

    decoded = decode_ok(0x69400440U);
    CHECK(decoded.opcode == ARM64_OP_LOAD_STORE_PAIR);
    CHECK(decoded.flags & ARM64_INSN_FLAG_SIGN_EXTEND);

    decoded = decode_ok(0x68C10440U);
    CHECK(decoded.opcode == ARM64_OP_LOAD_STORE_PAIR);
    CHECK(decoded.flags & ARM64_INSN_FLAG_WRITEBACK);

    decoded = decode_ok(0x08A07C00U);
    CHECK(decoded.opcode == ARM64_OP_CAS);
    CHECK(decoded.flags & ARM64_INSN_FLAG_LOAD);
    CHECK(decoded.flags & ARM64_INSN_FLAG_STORE);
    CHECK(decoded.operands.load_store.access_bytes == 1);

    decoded = decode_ok(0x08207C00U);
    CHECK(decoded.opcode == ARM64_OP_CASP);
    CHECK(decoded.operand_width == 32);

    decoded = decode_ok(0x48207C00U);
    CHECK(decoded.opcode == ARM64_OP_CASP);
    CHECK(decoded.operand_width == 64);

    decoded = decode_ok(0xC89F7C20U);
    CHECK(decoded.opcode == ARM64_OP_EXCLUSIVE);
    CHECK(decoded.flags & ARM64_INSN_FLAG_ORDERED);
    CHECK(decoded.flags & ARM64_INSN_FLAG_STORE);
    CHECK(!(decoded.flags & ARM64_INSN_FLAG_RELEASE));

    decoded = decode_ok(0xC8DF7C20U);
    CHECK(decoded.opcode == ARM64_OP_EXCLUSIVE);
    CHECK(decoded.flags & ARM64_INSN_FLAG_ORDERED);
    CHECK(decoded.flags & ARM64_INSN_FLAG_LOAD);
    CHECK(!(decoded.flags & ARM64_INSN_FLAG_ACQUIRE));

    decoded = decode_ok(0x78BFC0C5U);
    CHECK(decoded.opcode == ARM64_OP_LDAPR);
    CHECK(decoded.flags & ARM64_INSN_FLAG_LOAD);
    CHECK(decoded.flags & ARM64_INSN_FLAG_ACQUIRE);
    CHECK(decoded.operands.load_store.access_bytes == 2);

    decoded = decode_ok(0x38200000U);
    CHECK(decoded.opcode == ARM64_OP_ATOMIC_RMW);
    CHECK(decoded.operation == ARM64_OPERATION_LDADD);
    CHECK(decoded.flags & ARM64_INSN_FLAG_LOAD);
    CHECK(decoded.flags & ARM64_INSN_FLAG_STORE);
    CHECK(decoded.operands.load_store.address_mode == ARM64_ADDRESS_BASE);

    decoded = decode_ok(0x19000000U);
    CHECK(decoded.opcode == ARM64_OP_RCPC_UNSCALED);
    CHECK(decoded.flags & ARM64_INSN_FLAG_STORE);
    CHECK(decoded.flags & ARM64_INSN_FLAG_RELEASE);
    CHECK(!(decoded.flags & ARM64_INSN_FLAG_FP));
    CHECK(decoded.operands.load_store.address_mode == ARM64_ADDRESS_UNSCALED_OFFSET);

    decoded = decode_ok(0x3DC00000U);
    CHECK(decoded.flags & ARM64_INSN_FLAG_FP);
    CHECK(decoded.flags & ARM64_INSN_FLAG_LOAD);
    CHECK(decoded.operands.load_store.access_bytes == 16);
    CHECK(decoded.operand_width == 128);

    decoded = decode_ok(0x58000020U);
    CHECK(decoded.opcode == ARM64_OP_LOAD_LITERAL);
    CHECK(decoded.flags & ARM64_INSN_FLAG_LOAD);
    CHECK(!(decoded.flags & ARM64_INSN_FLAG_FP));
    CHECK(decoded.operands.load_store.offset == 4);
    CHECK(decoded.effects & ARM64_EFFECT_PC_RELATIVE);

    decoded = decode_ok(0xD8000020U);
    CHECK(decoded.opcode == ARM64_OP_PREFETCH_LITERAL);
    CHECK(decoded.effects & ARM64_EFFECT_PREFETCH);
    CHECK(decoded.effects & ARM64_EFFECT_PC_RELATIVE);

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
    arm64_u64 target = 0;

    CHECK(decoded.effects & ARM64_EFFECT_CONTROL_FLOW);
    CHECK(decoded.effects & ARM64_EFFECT_DIRECT_TARGET);
    CHECK(arm64_decode_direct_target(&decoded, 0x1000, &target));
    CHECK(target == 0x1010);

    decoded = decode_ok(0x90000000U);
    CHECK(arm64_decode_direct_target(&decoded, 0x12345, &target));
    CHECK(target == 0x12000);

    decoded = decode_ok(0x10FFFFE0U);
    CHECK(decoded.opcode == ARM64_OP_ADR);
    CHECK(decoded.operands.pc_relative.offset == -4);
    CHECK(decoded.effects & ARM64_EFFECT_WRITE_GPR);
    CHECK(decoded.effects & ARM64_EFFECT_PC_RELATIVE);
    CHECK(arm64_decode_direct_target(&decoded, 0x1004, &target));
    CHECK(target == 0x1000);

    decoded = decode_ok(0x10000081U);
    CHECK(decoded.opcode == ARM64_OP_ADR);
    CHECK(decoded.operands.pc_relative.offset == 16);
    CHECK(arm64_decode_direct_target(&decoded, 0x1008, &target));
    CHECK(target == 0x1018);

    decoded = decode_ok(0xB0000002U);
    CHECK(decoded.opcode == ARM64_OP_ADRP);
    CHECK(decoded.operands.pc_relative.offset == 0x1000);
    CHECK(arm64_decode_direct_target(&decoded, 0x12345, &target));
    CHECK(target == 0x13000);

    decoded = decode_ok(0xF0FFFFE3U);
    CHECK(decoded.opcode == ARM64_OP_ADRP);
    CHECK(decoded.operands.pc_relative.offset == -0x1000);
    CHECK(arm64_decode_direct_target(&decoded, 0x12345, &target));
    CHECK(target == 0x11000);

    decoded = decode_ok(0x34000000U);
    CHECK(decoded.opcode == ARM64_OP_CBZ);
    CHECK(decoded.operand_width == 32);

    decoded = decode_ok(0xB4000000U);
    CHECK(decoded.opcode == ARM64_OP_CBZ);
    CHECK(decoded.operand_width == 64);
}

static void test_scalar_fp(void)
{
    struct arm64_decoded_insn decoded = decode_ok(0x1E2E1000U);

    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_SCALAR_IMMEDIATE);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FMOV_IMMEDIATE);
    CHECK(decoded.operands.simd.expanded_immediate == 0x3F800000ULL);

    decoded = decode_ok(0x1E701001U);
    CHECK(decoded.operands.simd.expanded_immediate == 0xC000000000000000ULL);

    decoded = decode_ok(0x1E244062U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_SCALAR_1SOURCE);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FRINT);
    CHECK(decoded.operands.simd.rounding_mode == ARM64_FP_ROUND_NEAREST_EVEN);

    decoded = decode_ok(0x1E63145AU);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_SCALAR_CONDITIONAL_COMPARE);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FCCMPE);
    CHECK(decoded.operands.simd.condition == 1);
    CHECK(decoded.operands.simd.immediate == 0xA);
    CHECK(decoded.effects & ARM64_EFFECT_READ_FLAGS);
    CHECK(decoded.effects & ARM64_EFFECT_WRITE_FLAGS);
    CHECK(!(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD));

    decoded = decode_ok(0x1E2F21F8U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_SCALAR_COMPARE);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FCMPE);
    CHECK(decoded.operands.simd.flags & ARM64_SIMD_FLAG_COMPARE_ZERO);

    decoded = decode_ok(0x1EF322D0U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_SCALAR_COMPARE);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FCMPE);
    CHECK(decoded.operand_width == 16);

    decoded = decode_ok(0x1E204020U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_SCALAR_1SOURCE);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FMOV);
}

static void test_scalar_copy(void)
{
    static const struct
    {
        arm64_u32 raw;
        arm64_u8 element_width;
        arm64_u8 lane_index;
    } cases[] = {
        {0x5E010420U, 8, 0}, {0x5E1F0462U, 8, 15}, {0x5E0204A4U, 16, 0}, {0x5E1E04E6U, 16, 7}, {0x5E040528U, 32, 0}, {0x5E0C0422U, 32, 1}, {0x5E1C056AU, 32, 3}, {0x5E0805ACU, 64, 0}, {0x5E1805EEU, 64, 1},
    };
    struct arm64_decoded_insn decoded;
    arm64_u32 imm5;
    size_t index;

    for (index = 0; index < sizeof(cases) / sizeof(cases[0]); index++)
    {
        decoded = decode_ok(cases[index].raw);
        CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_SCALAR_COPY);
        CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_DUP_ELEMENT);
        CHECK(decoded.operands.simd.element_width == cases[index].element_width);
        CHECK(decoded.operands.simd.lane_index == cases[index].lane_index);
        CHECK(decoded.operand_width == cases[index].element_width);
        CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
        CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);
        CHECK(!(decoded.effects & ARM64_EFFECT_READ_GPR));
        CHECK(!(decoded.effects & ARM64_EFFECT_WRITE_GPR));
    }

    decoded = decode_ok(0x5E0C0422U);
    CHECK(decoded.rd == 2);
    CHECK(decoded.rn == 1);

    for (imm5 = 0; imm5 < 32; imm5++)
    {
        arm64_u32 raw = 0x5E000420U | (imm5 << 16);
        arm64_u8 size;

        if (!imm5 || imm5 == 16)
        {
            decode_status_is(raw, ARM64_DECODE_UNALLOCATED);
            continue;
        }

        size = (arm64_u8)__builtin_ctz(imm5);
        decoded = decode_ok(raw);
        CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_SCALAR_COPY);
        CHECK(decoded.operands.simd.element_width == (8U << size));
        CHECK(decoded.operands.simd.lane_index == (imm5 >> (size + 1)));
        CHECK(decoded.operand_width == decoded.operands.simd.element_width);
    }
}

static void test_fp_by_element(void)
{
    static const struct
    {
        arm64_u32 bits;
        enum arm64_simd_operation operation;
    } operations[] = {
        {0x00000000U, ARM64_SIMD_OP_FMLA_BY_ELEMENT},
        {0x00004000U, ARM64_SIMD_OP_FMLS_BY_ELEMENT},
        {0x00008000U, ARM64_SIMD_OP_FMUL_BY_ELEMENT},
        {0x20008000U, ARM64_SIMD_OP_FMULX_BY_ELEMENT},
    };
    static const struct
    {
        arm64_u32 fmla;
        arm64_u8 operand_width;
        arm64_u8 element_width;
        arm64_u8 lane_index;
        arm64_u8 rm;
    } shapes[] = {
        {0x0F3F1883U, 64, 16, 7, 15}, {0x4F3F1883U, 128, 16, 7, 15}, {0x0FBF1883U, 64, 32, 3, 31}, {0x4FBF1883U, 128, 32, 3, 31}, {0x4FDF1883U, 128, 64, 1, 31}, {0x5F3F1883U, 16, 16, 7, 15}, {0x5FBF1883U, 32, 32, 3, 31}, {0x5FDF1883U, 64, 64, 1, 31},
    };
    struct arm64_decoded_insn decoded;
    size_t operation_index;
    size_t shape_index;

    for (operation_index = 0; operation_index < sizeof(operations) / sizeof(operations[0]); operation_index++)
    {
        for (shape_index = 0; shape_index < sizeof(shapes) / sizeof(shapes[0]); shape_index++)
        {
            decoded = decode_ok(shapes[shape_index].fmla | operations[operation_index].bits);
            CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_FP_BY_ELEMENT);
            CHECK(decoded.operands.simd.operation == operations[operation_index].operation);
            CHECK(decoded.operand_width == shapes[shape_index].operand_width);
            CHECK(decoded.operands.simd.element_width == shapes[shape_index].element_width);
            CHECK(decoded.operands.simd.lane_index == shapes[shape_index].lane_index);
            CHECK(decoded.rm == shapes[shape_index].rm);
            CHECK(decoded.rd == 3);
            CHECK(decoded.rn == 4);
            CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
            CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);
            CHECK(!(decoded.effects & ARM64_EFFECT_READ_GPR));
            CHECK(!(decoded.effects & ARM64_EFFECT_WRITE_GPR));
        }
    }

    decoded = decode_ok(0x0F861232U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_FP_BY_ELEMENT);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FMLA_BY_ELEMENT);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.operands.simd.element_width == 32);
    CHECK(decoded.operands.simd.lane_index == 0);
    CHECK(decoded.rm == 6);
    CHECK(decoded.rd == 18);
    CHECK(decoded.rn == 17);

    decode_status_is(0x6F821020U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x6F825820U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x0F421020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x0FC21020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x4FEE1020U, ARM64_DECODE_UNALLOCATED);

    decode_status_is(0x2F420020U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x6FAB4949U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x0F4E81ACU, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x4FA7DAD5U, ARM64_DECODE_UNSUPPORTED);
}

static void test_scalar_fp_3source(void)
{
    static const struct
    {
        arm64_u32 bits;
        enum arm64_simd_operation operation;
    } operations[] = {
        {0x00000000U, ARM64_SIMD_OP_FMADD},
        {0x00008000U, ARM64_SIMD_OP_FMSUB},
        {0x00200000U, ARM64_SIMD_OP_FNMADD},
        {0x00208000U, ARM64_SIMD_OP_FNMSUB},
    };
    static const struct
    {
        arm64_u32 bits;
        arm64_u8 width;
    } shapes[] = {
        {0x00000000U, 32},
        {0x00400000U, 64},
    };
    struct arm64_decoded_insn decoded;
    size_t operation_index;
    size_t shape_index;

    for (operation_index = 0; operation_index < sizeof(operations) / sizeof(operations[0]); operation_index++)
    {
        for (shape_index = 0; shape_index < sizeof(shapes) / sizeof(shapes[0]); shape_index++)
        {
            decoded = decode_ok(0x1F041000U | operations[operation_index].bits | shapes[shape_index].bits);
            CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_SCALAR_3SOURCE);
            CHECK(decoded.operands.simd.operation == operations[operation_index].operation);
            CHECK(decoded.operand_width == shapes[shape_index].width);
            CHECK(decoded.rd == 0);
            CHECK(decoded.rn == 0);
            CHECK(decoded.ra == 4);
            CHECK(decoded.rm == 4);
        }
    }

    decode_status_is(0x1F821020U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x1FC01000U, ARM64_DECODE_UNSUPPORTED);
}

static void test_vector(void)
{
    static const struct
    {
        arm64_u32 bits;
        enum arm64_simd_operation operation;
    } logical_operations[] = {
        {0x00000000U, ARM64_SIMD_OP_AND_VECTOR}, {0x00400000U, ARM64_SIMD_OP_BIC_VECTOR}, {0x00800000U, ARM64_SIMD_OP_ORR_VECTOR}, {0x00C00000U, ARM64_SIMD_OP_ORN_VECTOR}, {0x20000000U, ARM64_SIMD_OP_EOR_VECTOR}, {0x20400000U, ARM64_SIMD_OP_BSL_VECTOR}, {0x20800000U, ARM64_SIMD_OP_BIT_VECTOR}, {0x20C00000U, ARM64_SIMD_OP_BIF_VECTOR},
    };
    static const struct
    {
        arm64_u32 bits;
        arm64_u8 operand_width;
    } logical_shapes[] = {
        {0x00000000U, 64},
        {0x40000000U, 128},
    };
    static const struct
    {
        arm64_u32 bits;
        enum arm64_simd_operation operation;
    } permute_operations[] = {
        {0x00001000U, ARM64_SIMD_OP_UZP1}, {0x00002000U, ARM64_SIMD_OP_TRN1}, {0x00003000U, ARM64_SIMD_OP_ZIP1}, {0x00005000U, ARM64_SIMD_OP_UZP2}, {0x00006000U, ARM64_SIMD_OP_TRN2}, {0x00007000U, ARM64_SIMD_OP_ZIP2},
    };
    static const struct
    {
        arm64_u32 base;
        arm64_u8 operand_width;
        arm64_u8 element_width;
    } permute_shapes[] = {
        {0x0E000800U, 64, 8}, {0x4E000800U, 128, 8}, {0x0E400800U, 64, 16}, {0x4E400800U, 128, 16}, {0x0E800800U, 64, 32}, {0x4E800800U, 128, 32}, {0x4EC00800U, 128, 64},
    };
    static const struct
    {
        arm64_u32 bits;
        enum arm64_simd_operation operation;
    } fp_accumulate_operations[] = {
        {0x00000000U, ARM64_SIMD_OP_FMLA_VECTOR},
        {0x00800000U, ARM64_SIMD_OP_FMLS_VECTOR},
    };
    static const struct
    {
        arm64_u32 base;
        arm64_u8 operand_width;
        arm64_u8 element_width;
    } fp_accumulate_shapes[] = {
        {0x0E400C00U, 64, 16}, {0x4E400C00U, 128, 16}, {0x0E20CC00U, 64, 32}, {0x4E20CC00U, 128, 32}, {0x4E60CC00U, 128, 64},
    };
    static const struct
    {
        arm64_u32 bits;
        enum arm64_simd_operation operation;
    } compare_zero_relations[] = {
        {0x0000D000U, ARM64_SIMD_OP_FCMEQ_ZERO}, {0x2000C000U, ARM64_SIMD_OP_FCMGE_ZERO}, {0x0000C000U, ARM64_SIMD_OP_FCMGT_ZERO}, {0x2000D000U, ARM64_SIMD_OP_FCMLE_ZERO}, {0x0000E000U, ARM64_SIMD_OP_FCMLT_ZERO},
    };
    static const struct
    {
        arm64_u32 bits;
        arm64_u8 operand_width;
        arm64_u8 element_width;
    } compare_zero_shapes[] = {
        {0x0EF80800U, 64, 16}, {0x4EF80800U, 128, 16}, {0x0EA00800U, 64, 32}, {0x4EA00800U, 128, 32}, {0x4EE00800U, 128, 64}, {0x5EF80800U, 16, 16}, {0x5EA00800U, 32, 32}, {0x5EE00800U, 64, 64},
    };
    struct arm64_decoded_insn decoded = decode_ok(0x4F00E640U);
    size_t operation_index;
    size_t relation_index;
    size_t shape_index;

    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_MODIFIED_IMMEDIATE);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_MOVI);
    CHECK(decoded.operands.simd.expanded_immediate == 0x1212121212121212ULL);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.operands.simd.element_width == 8);

    decoded = decode_ok(0x6F00E423U);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_MOVI);
    CHECK(decoded.operands.simd.expanded_immediate == 0xFFULL);
    CHECK(decoded.operands.simd.element_width == 64);

    decoded = decode_ok(0x4E0C0462U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_COPY);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_DUP_ELEMENT);
    CHECK(decoded.operands.simd.element_width == 32);
    CHECK(decoded.operands.simd.lane_index == 1);

    decoded = decode_ok(0x6E0E6462U);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_INS_ELEMENT);
    CHECK(decoded.operands.simd.element_width == 16);
    CHECK(decoded.operands.simd.lane_index == 3);
    CHECK(decoded.operands.simd.source_lane_index == 6);

    decoded = decode_ok(0x6E1E6F3BU);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_INS_ELEMENT);
    CHECK(decoded.operands.simd.element_width == 16);
    CHECK(decoded.operands.simd.lane_index == 7);
    CHECK(decoded.operands.simd.source_lane_index == 6);

    decode_status_is(0x0E180462U, ARM64_DECODE_UNALLOCATED);

    decoded = decode_ok(0x4E1C2DEEU);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_SMOV);
    CHECK(decoded.operands.simd.element_width == 32);
    CHECK(decoded.operands.simd.lane_index == 3);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.effects & ARM64_EFFECT_WRITE_GPR);
    CHECK(!(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD));

    decoded = decode_ok(0x0E228420U);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_ADD);
    CHECK(decoded.operands.simd.element_width == 8);
    CHECK(decoded.operand_width == 64);

    decoded = decode_ok(0x4EF48672U);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_ADD);
    CHECK(decoded.operands.simd.element_width == 64);
    CHECK(decoded.operand_width == 128);

    decoded = decode_ok(0x6E2834E6U);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_CMHI);
    CHECK(decoded.operands.simd.element_width == 8);

    decoded = decode_ok(0x4E20D400U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_3REG);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FADD_V4S);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.operands.simd.element_width == 32);

    for (operation_index = 0; operation_index < sizeof(logical_operations) / sizeof(logical_operations[0]); operation_index++)
    {
        for (shape_index = 0; shape_index < sizeof(logical_shapes) / sizeof(logical_shapes[0]); shape_index++)
        {
            decoded = decode_ok(0x0E201C00U | logical_operations[operation_index].bits | logical_shapes[shape_index].bits | 0x30041U);
            CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_LOGICAL);
            CHECK(decoded.operands.simd.operation == logical_operations[operation_index].operation);
            CHECK(decoded.rd == 1);
            CHECK(decoded.rn == 2);
            CHECK(decoded.rm == 3);
            CHECK(decoded.operand_width == logical_shapes[shape_index].operand_width);
            CHECK(decoded.operands.simd.element_width == 8);
            CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
            CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);
        }
    }

    decoded = decode_ok(0x2E631C41U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_LOGICAL);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_BSL_VECTOR);
    CHECK(decoded.rd == 1);
    CHECK(decoded.rn == 2);
    CHECK(decoded.rm == 3);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.operands.simd.element_width == 8);

    for (operation_index = 0; operation_index < sizeof(permute_operations) / sizeof(permute_operations[0]); operation_index++)
    {
        for (shape_index = 0; shape_index < sizeof(permute_shapes) / sizeof(permute_shapes[0]); shape_index++)
        {
            decoded = decode_ok(permute_shapes[shape_index].base | permute_operations[operation_index].bits | 0x50081U);
            CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_PERMUTE);
            CHECK(decoded.operands.simd.operation == permute_operations[operation_index].operation);
            CHECK(decoded.rd == 1);
            CHECK(decoded.rn == 4);
            CHECK(decoded.rm == 5);
            CHECK(decoded.operand_width == permute_shapes[shape_index].operand_width);
            CHECK(decoded.operands.simd.element_width == permute_shapes[shape_index].element_width);
            CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
            CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);
        }
    }

    decoded = decode_ok(0x0E853881U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_PERMUTE);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_ZIP1);
    CHECK(decoded.rd == 1);
    CHECK(decoded.rn == 4);
    CHECK(decoded.rm == 5);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.operands.simd.element_width == 32);
    decode_status_is(0x0EC23820U, ARM64_DECODE_UNALLOCATED);

    for (operation_index = 0; operation_index < sizeof(fp_accumulate_operations) / sizeof(fp_accumulate_operations[0]); operation_index++)
    {
        for (shape_index = 0; shape_index < sizeof(fp_accumulate_shapes) / sizeof(fp_accumulate_shapes[0]); shape_index++)
        {
            decoded = decode_ok(fp_accumulate_shapes[shape_index].base | fp_accumulate_operations[operation_index].bits | 0x20064U);
            CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_3REG);
            CHECK(decoded.operands.simd.operation == fp_accumulate_operations[operation_index].operation);
            CHECK(decoded.rd == 4);
            CHECK(decoded.rn == 3);
            CHECK(decoded.rm == 2);
            CHECK(decoded.operand_width == fp_accumulate_shapes[shape_index].operand_width);
            CHECK(decoded.operands.simd.element_width == fp_accumulate_shapes[shape_index].element_width);
            CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
            CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);
        }
    }

    decoded = decode_ok(0x0E22CC64U);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FMLA_VECTOR);
    CHECK(decoded.rd == 4);
    CHECK(decoded.rn == 3);
    CHECK(decoded.rm == 2);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.operands.simd.element_width == 32);
    decode_status_is(0x0E62CC20U, ARM64_DECODE_UNALLOCATED);

    decoded = decode_ok(0x0EA00826U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_2REG);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_REV64_V2S);
    CHECK(decoded.rd == 6);
    CHECK(decoded.rn == 1);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.operands.simd.element_width == 32);
    CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
    CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);

    for (relation_index = 0; relation_index < sizeof(compare_zero_relations) / sizeof(compare_zero_relations[0]); relation_index++)
    {
        for (shape_index = 0; shape_index < sizeof(compare_zero_shapes) / sizeof(compare_zero_shapes[0]); shape_index++)
        {
            decoded = decode_ok(compare_zero_shapes[shape_index].bits | compare_zero_relations[relation_index].bits | 0x61U);
            CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_FP_COMPARE_ZERO);
            CHECK(decoded.operands.simd.operation == compare_zero_relations[relation_index].operation);
            CHECK(decoded.rd == 1);
            CHECK(decoded.rn == 3);
            CHECK(decoded.operand_width == compare_zero_shapes[shape_index].operand_width);
            CHECK(decoded.operands.simd.element_width == compare_zero_shapes[shape_index].element_width);
            CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
            CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);
        }
    }

    decode_status_is(0x0EE0D820U, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x2EA0E820U, ARM64_DECODE_UNSUPPORTED);

    decoded = decode_ok(0x0EA0E861U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_FP_COMPARE_ZERO);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FCMLT_ZERO);
    CHECK(decoded.rd == 1);
    CHECK(decoded.rn == 3);
    CHECK(decoded.operand_width == 64);
    CHECK(decoded.operands.simd.element_width == 32);
    CHECK(decoded.effects & ARM64_EFFECT_READ_FP_SIMD);
    CHECK(decoded.effects & ARM64_EFFECT_WRITE_FP_SIMD);

    decoded = decode_ok(0x4EA0F800U);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_2REG);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_FABS_V4S);
    CHECK(decoded.operand_width == 128);
    CHECK(decoded.operands.simd.element_width == 32);

    decoded = decode_ok(0x4F2355ACU);
    CHECK(decoded.operands.simd.group == ARM64_SIMD_GROUP_VECTOR_SHIFT_IMMEDIATE);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_SHL);
    CHECK(decoded.operands.simd.element_width == 32);
    CHECK(decoded.operands.simd.immediate == 3);

    decoded = decode_ok(0x6F790630U);
    CHECK(decoded.operands.simd.operation == ARM64_SIMD_OP_USHR);
    CHECK(decoded.operands.simd.element_width == 64);
    CHECK(decoded.operands.simd.immediate == 7);

    decode_status_is(0x4F5650CBU, ARM64_DECODE_UNALLOCATED);
    decode_status_is(0x6F5B70CEU, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x0F610271U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x2F4572BFU, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x9B760A29U, ARM64_DECODE_UNSUPPORTED);
    decode_status_is(0x9B6EF8EAU, ARM64_DECODE_UNSUPPORTED);
}

static int run_tests(void)
{
    test_dispatch();
    test_system();
    test_control_flow();
    test_load_store();
    test_scalar_fp();
    test_scalar_copy();
    test_fp_by_element();
    test_scalar_fp_3source();
    test_fp_conversions();
    test_vector();
    return failures;
}

#ifdef ARM64_DECODE_FREESTANDING_TEST
void *memset(void *destination, int value, size_t count)
{
    unsigned char *bytes = destination;
    size_t index;

    for (index = 0; index < count; index++) bytes[index] = (unsigned char)value;
    return destination;
}

void *memcpy(void *destination, const void *source, size_t count)
{
    unsigned char *destination_bytes = destination;
    const unsigned char *source_bytes = source;
    size_t index;

    for (index = 0; index < count; index++) destination_bytes[index] = source_bytes[index];
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