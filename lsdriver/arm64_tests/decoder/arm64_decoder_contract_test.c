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
    enum arm64_decode_status status = arm64_decode_instruction(raw, &decoded);
    if (status != expected || decoded.instruction != ARM64_INST_UNKNOWN)
    {
        fprintf(stderr, "rejection raw=%08x status=%u instruction=%u\n", raw, status, decoded.instruction);
        return 1;
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
    if (check_failure(0x5A9F1BE9U, ARM64_DECODE_UNALLOCATED) || check_failure(0x52C00000U, ARM64_DECODE_UNALLOCATED) || check_failure(0x13808000U, ARM64_DECODE_UNALLOCATED) || check_failure(0x0B600000U, ARM64_DECODE_UNALLOCATED) || check_failure(0xD50330DFU, ARM64_DECODE_UNALLOCATED) || check_failure(0x08207C83U, ARM64_DECODE_UNALLOCATED) || check_atomic() || check_family_contracts()) return 1;
    puts("decoder contracts: PASS; CAS/CASP variants=24");
    return 0;
}