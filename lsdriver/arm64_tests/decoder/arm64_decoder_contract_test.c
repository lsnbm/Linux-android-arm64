#include "../../arm64_decode/arm64_decode.h"
#include <stdio.h>
#include <string.h>

enum arm64_decode_status arm64_decode_data_processing_immediate(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_data_processing_register(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_load_store(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_branch_exception_system(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_simd_fp(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_sve(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_sme(uint32_t raw, struct arm64_decoded_instruction *decoded);

static int check_failure(uint32_t raw, enum arm64_decode_status expected)
{
    struct arm64_decoded_instruction decoded;
    memset(&decoded, 0xA5, sizeof(decoded));
    enum arm64_decode_status status = arm64_decode_instruction(raw, &decoded);
    const unsigned char *bytes = (const unsigned char *)&decoded;
    for (size_t index = 0; index < sizeof(decoded); ++index)
    {
        if (status != expected || bytes[index] != 0)
        {
            fprintf(stderr, "rejection raw=%08x status=%u byte=%zu value=%u\n", raw, status, index, bytes[index]);
            return 1;
        }
    }
    return 0;
}

static int check_atomic(void)
{
    enum arm64_instruction cas[4][4] = {{ARM64_INST_CASB, ARM64_INST_CASLB, ARM64_INST_CASAB, ARM64_INST_CASALB}, {ARM64_INST_CASH, ARM64_INST_CASLH, ARM64_INST_CASAH, ARM64_INST_CASALH}, {ARM64_INST_CAS, ARM64_INST_CASL, ARM64_INST_CASA, ARM64_INST_CASAL}, {ARM64_INST_CAS, ARM64_INST_CASL, ARM64_INST_CASA, ARM64_INST_CASAL}};
    enum arm64_instruction casp[4] = {ARM64_INST_CASP, ARM64_INST_CASPL, ARM64_INST_CASPA, ARM64_INST_CASPAL};
    for (uint32_t size = 0; size < 4; ++size)
    {
        for (uint32_t ordering = 0; ordering < 4; ++ordering)
        {
            uint32_t raw = 0x08A07C82U | (size << 30) | ((ordering >> 1) << 22) | ((ordering & 1) << 15);
            struct arm64_decoded_instruction decoded;
            if (arm64_decode_instruction(raw, &decoded) != ARM64_DECODE_OK || decoded.instruction != cas[size][ordering] || decoded.operand_width != (size == 3 ? 64 : 32) || decoded.rn != 4 || decoded.rt != 2 || decoded.rs != 0)
            {
                fprintf(stderr, "CAS raw=%08x\n", raw);
                return 1;
            }
            if (size >= 2) continue;
            raw = 0x08207C82U | (size << 30) | ((ordering >> 1) << 22) | ((ordering & 1) << 15);
            if (arm64_decode_instruction(raw, &decoded) != ARM64_DECODE_OK || decoded.instruction != casp[ordering] || decoded.operand_width != (size ? 64 : 32) || decoded.rn != 4 || decoded.rt != 2 || decoded.rs != 0)
            {
                fprintf(stderr, "CASP raw=%08x\n", raw);
                return 1;
            }
        }
    }
    return 0;
}

static int check_rcpc_loads(void)
{
    const struct
    {
        uint32_t raw;
        enum arm64_instruction instruction;
        uint32_t width;
        uint32_t rn;
        uint32_t rt;
    } cases[] = {
        {0xB8BFC020U, ARM64_INST_LDAPR, 32, 1, 0},
        {0x38BFC062U, ARM64_INST_LDAPRB, 32, 3, 2},
        {0x78BFC0A4U, ARM64_INST_LDAPRH, 32, 5, 4},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index)
    {
        struct arm64_decoded_instruction decoded;
        enum arm64_decode_status status = arm64_decode_instruction(cases[index].raw, &decoded);
        if (status != ARM64_DECODE_OK || decoded.instruction != cases[index].instruction || decoded.operand_width != cases[index].width || decoded.rn != cases[index].rn || decoded.rt != cases[index].rt)
        {
            fprintf(stderr, "RCpc load raw=%08x status=%u instruction=%u\n", cases[index].raw, status, decoded.instruction);
            return 1;
        }
    }
    puts("RCpc load encodings: PASS; cases=3");
    return 0;
}

static int check_single_store_widths(void)
{
    struct single_store_case
    {
        uint32_t raw;
        enum arm64_instruction instruction;
    } cases[] = {
        {0x38000000U | (4U << 5) | 2U, ARM64_INST_STURB_GPR},
        {0x78000000U | (4U << 5) | 2U, ARM64_INST_STURH_GPR},
        {0x38000800U | (4U << 5) | 2U, ARM64_INST_STTRB_GPR},
        {0x78000800U | (4U << 5) | 2U, ARM64_INST_STTRH_GPR},
        {0x38000400U | (4U << 5) | 2U, ARM64_INST_STRB_GPR_POST_INDEX},
        {0x78000400U | (4U << 5) | 2U, ARM64_INST_STRH_GPR_POST_INDEX},
        {0x38000C00U | (4U << 5) | 2U, ARM64_INST_STRB_GPR_PRE_INDEX},
        {0x78000C00U | (4U << 5) | 2U, ARM64_INST_STRH_GPR_PRE_INDEX},
        {0x38200800U | (2U << 13) | (4U << 5) | 2U, ARM64_INST_STRB_GPR_REGISTER_OFFSET},
        {0x78200800U | (2U << 13) | (4U << 5) | 2U, ARM64_INST_STRH_GPR_REGISTER_OFFSET},
    };

    for (size_t index = 0; index < sizeof(cases) / sizeof(cases[0]); ++index)
    {
        struct arm64_decoded_instruction decoded;
        if (arm64_decode_instruction(cases[index].raw, &decoded) != ARM64_DECODE_OK || decoded.instruction != cases[index].instruction || decoded.operand_width != 32 || decoded.rn != 4 || decoded.rt != 2)
        {
            fprintf(stderr, "single store raw=%08x\n", cases[index].raw);
            return 1;
        }
    }
    return 0;
}

static int check_barrier_options(void)
{
    const uint32_t bases[] = {0xD503309FU, 0xD50330BFU, 0xD50330DFU, 0xD503305FU};
    const enum arm64_instruction instructions[] = {ARM64_INST_DSB, ARM64_INST_DMB, ARM64_INST_ISB, ARM64_INST_CLREX};
    for (size_t base_index = 0; base_index < sizeof(bases) / sizeof(bases[0]); ++base_index)
    {
        for (uint32_t option = 0; option < 16; ++option)
        {
            uint32_t raw = bases[base_index] | (option << 8);
            struct arm64_decoded_instruction decoded;
            enum arm64_decode_status status = arm64_decode_instruction(raw, &decoded);
            if (status != ARM64_DECODE_OK || decoded.instruction != instructions[base_index] || decoded.immediate != option)
            {
                fprintf(stderr, "barrier raw=%08x status=%u option=%u\n", raw, status, option);
                return 1;
            }
        }
    }
    puts("barrier/CLREX option encodings: PASS; cases=64");
    return 0;
}

static int check_hints_and_branch_conditions(void)
{
    const uint32_t hints[] = {0, 1, 2, 3, 4, 5, 0x19, 0x20, 0x22, 0x24, 0x26};
    const enum arm64_instruction instructions[] = {ARM64_INST_NOP, ARM64_INST_YIELD, ARM64_INST_WFE, ARM64_INST_WFI, ARM64_INST_SEV, ARM64_INST_SEVL, ARM64_INST_PACIASP, ARM64_INST_BTI, ARM64_INST_BTI, ARM64_INST_BTI, ARM64_INST_BTI};
    for (size_t index = 0; index < sizeof(hints) / sizeof(hints[0]); ++index)
    {
        uint32_t raw = 0xD503201FU | (hints[index] << 5);
        struct arm64_decoded_instruction decoded;
        uint64_t immediate = hints[index] >= 0x20 ? hints[index] - 0x20 : 0;
        if (arm64_decode_instruction(raw, &decoded) != ARM64_DECODE_OK || decoded.instruction != instructions[index] || decoded.immediate != immediate)
        {
            fprintf(stderr, "hint raw=%08x\n", raw);
            return 1;
        }
    }
    if (check_failure(0xD50320FFU, ARM64_DECODE_UNSUPPORTED) || check_failure(0xD503301FU, ARM64_DECODE_UNSUPPORTED)) return 1;
    const int64_t offsets[] = {-1048576, 8, 1048572};
    for (uint32_t condition = 0; condition < 16; ++condition)
    {
        for (size_t index = 0; index < sizeof(offsets) / sizeof(offsets[0]); ++index)
        {
            uint32_t raw = 0x54000000U | ((((uint32_t)(offsets[index] / 4)) & 0x7FFFFU) << 5) | condition;
            struct arm64_decoded_instruction decoded;
            if (arm64_decode_instruction(raw, &decoded) != ARM64_DECODE_OK || decoded.instruction != ARM64_INST_B_COND || decoded.condition != condition || decoded.offset != offsets[index])
            {
                fprintf(stderr, "branch condition raw=%08x\n", raw);
                return 1;
            }
        }
    }
    puts("HINT/branch condition encodings: PASS; hints=11 boundaries=2 branches=48");
    return 0;
}

static int check_extended_register_options(void)
{
    const enum arm64_instruction instructions[] = {ARM64_INST_ADD_EXTENDED_REGISTER, ARM64_INST_ADDS_EXTENDED_REGISTER, ARM64_INST_SUB_EXTENDED_REGISTER, ARM64_INST_SUBS_EXTENDED_REGISTER};
    for (uint32_t sf = 0; sf < 2; ++sf)
    {
        for (uint32_t operation = 0; operation < 4; ++operation)
        {
            for (uint32_t option = 0; option < 8; ++option)
            {
                for (uint32_t shift = 0; shift < 8; ++shift)
                {
                    uint32_t raw = 0x0B200000U | (sf << 31) | (operation << 29) | (3U << 16) | (option << 13) | (shift << 10) | (4U << 5) | 2U;
                    if (shift > 4)
                    {
                        if (check_failure(raw, ARM64_DECODE_UNALLOCATED)) return 1;
                        continue;
                    }
                    struct arm64_decoded_instruction decoded;
                    enum arm64_decode_status status = arm64_decode_instruction(raw, &decoded);
                    if (status != ARM64_DECODE_OK || decoded.instruction != instructions[operation] || decoded.operand_width != (32U << sf) || decoded.extend_type != option || decoded.shift_amount != shift || decoded.rd != 2 || decoded.rn != 4 || decoded.rm != 3)
                    {
                        fprintf(stderr, "extended register raw=%08x status=%u option=%u shift=%u\n", raw, status, option, shift);
                        return 1;
                    }
                }
            }
        }
    }
    puts("extended register encodings: PASS; cases=512");
    return 0;
}

static int check_logical_shift_types(void)
{
    const enum arm64_instruction instructions[] = {ARM64_INST_AND_SHIFTED_REGISTER, ARM64_INST_BIC_SHIFTED_REGISTER, ARM64_INST_ORR_SHIFTED_REGISTER, ARM64_INST_ORN_SHIFTED_REGISTER, ARM64_INST_EOR_SHIFTED_REGISTER, ARM64_INST_EON_SHIFTED_REGISTER, ARM64_INST_ANDS_SHIFTED_REGISTER, ARM64_INST_BICS_SHIFTED_REGISTER};
    for (uint32_t sf = 0; sf < 2; ++sf)
    {
        for (uint32_t operation = 0; operation < 8; ++operation)
        {
            for (uint32_t shift_type = 0; shift_type < 4; ++shift_type)
            {
                for (uint32_t shift = 0; shift < 64; ++shift)
                {
                    uint32_t raw = 0x0A000000U | (sf << 31) | ((operation >> 1) << 29) | ((operation & 1U) << 21) | (shift_type << 22) | (3U << 16) | (shift << 10) | (4U << 5) | 2U;
                    if (!sf && shift >= 32)
                    {
                        if (check_failure(raw, ARM64_DECODE_UNALLOCATED)) return 1;
                        continue;
                    }
                    struct arm64_decoded_instruction decoded;
                    enum arm64_decode_status status = arm64_decode_instruction(raw, &decoded);
                    if (status != ARM64_DECODE_OK || decoded.instruction != instructions[operation] || decoded.operand_width != (32U << sf) || decoded.shift_type != shift_type || decoded.shift_amount != shift || decoded.rd != 2 || decoded.rn != 4 || decoded.rm != 3)
                    {
                        fprintf(stderr, "logical shift raw=%08x status=%u type=%u shift=%u\n", raw, status, shift_type, shift);
                        return 1;
                    }
                }
            }
        }
    }
    puts("logical shift encodings: PASS; cases=4096");
    return 0;
}

static int check_movi_64bit_forms(void)
{
    for (uint32_t q = 0; q < 2; ++q)
    {
        for (uint32_t immediate = 0; immediate < 256; ++immediate)
        {
            uint32_t raw = 0x2F00E400U | (q << 30) | ((immediate >> 5) << 16) | ((immediate & 31U) << 5);
            struct arm64_decoded_instruction decoded;
            enum arm64_decode_status status = arm64_decode_instruction(raw, &decoded);
            if (status != ARM64_DECODE_OK || decoded.instruction != ARM64_INST_MOVI_VECTOR_IMMEDIATE || decoded.operand_width != (64U << q) || decoded.element_width != 64 || decoded.immediate != immediate || decoded.simd_cmode != 14 || decoded.rd != 0)
            {
                fprintf(stderr, "MOVI raw=%08x status=%u q=%u immediate=%u\n", raw, status, q, immediate);
                return 1;
            }
        }
    }
    puts("MOVI scalar/vector encodings: PASS; cases=512");
    return 0;
}

static int check_family_contracts(void)
{
    enum arm64_decode_status (*const decoders[])(uint32_t, struct arm64_decoded_instruction *) = {arm64_decode_data_processing_immediate, arm64_decode_data_processing_register, arm64_decode_load_store, arm64_decode_branch_exception_system, arm64_decode_simd_fp, arm64_decode_sve, arm64_decode_sme};
    uint32_t random = 0xC0DEC0DEU;
    unsigned failures = 0;
    unsigned successes = 0;
    for (unsigned family = 0; family < sizeof(decoders) / sizeof(decoders[0]); ++family)
    {
        for (unsigned sample = 0; sample < 100000; ++sample)
        {
            random ^= random << 13;
            random ^= random >> 17;
            random ^= random << 5;
            struct arm64_decoded_instruction decoded;
            memset(&decoded, 0xA5, sizeof(decoded));
            decoded.instruction = ARM64_INST_UNKNOWN;
            struct arm64_decoded_instruction before;
            memcpy(&before, &decoded, sizeof(before));
            enum arm64_decode_status status = decoders[family](random, &decoded);
            if (status == ARM64_DECODE_OK)
            {
                ++successes;
                if (decoded.instruction != ARM64_INST_UNKNOWN) continue;
            }
            else
            {
                ++failures;
                if (!memcmp(&before, &decoded, sizeof(decoded))) continue;
            }
            fprintf(stderr, "contract family=%u raw=%08x status=%u instruction=%u\n", family, random, status, decoded.instruction);
            return 1;
        }
    }
    printf("family contracts: samples=700000 success=%u rejected=%u\n", successes, failures);
    return 0;
}

int main(void)
{
    int failures = check_extended_register_options();
    failures |= check_logical_shift_types();
    failures |= check_movi_64bit_forms();
    failures |= check_hints_and_branch_conditions();
    if (failures || check_failure(0x5A9F1BE9U, ARM64_DECODE_UNALLOCATED) || check_failure(0x52C00000U, ARM64_DECODE_UNALLOCATED) || check_failure(0x13808000U, ARM64_DECODE_UNALLOCATED) || check_failure(0x0B600000U, ARM64_DECODE_UNALLOCATED) || check_failure(0x08207C83U, ARM64_DECODE_UNALLOCATED) || check_failure(0x80800000U, ARM64_DECODE_UNSUPPORTED) || check_failure(0x2518E3E0U, ARM64_DECODE_UNSUPPORTED) || check_atomic() || check_rcpc_loads() || check_single_store_widths() || check_barrier_options() || check_family_contracts()) return 1;
    puts("decoder contracts: PASS; CAS/CASP variants=24");
    return 0;
}