#include "arm64_decode.h"

static inline enum arm64_decode_status arm64_decode_branch_immediate(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x7C000000U, 0x14000000U))
    {
        return ARM64_DECODE_UNSUPPORTED;
    }
    decoded->instruction = ARM64_DECODE_BIT(raw, 31) ? ARM64_INST_BL : ARM64_INST_B;
    decoded->offset = ARM64_DECODE_SIGNED_FIELD(raw, 25, 0, 2);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_branch_compare(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x7E000000U, 0x34000000U))
    {
        return ARM64_DECODE_UNSUPPORTED;
    }
    decoded->instruction = ARM64_DECODE_BIT(raw, 24) ? ARM64_INST_CBNZ : ARM64_INST_CBZ;
    decoded->operand_width = ARM64_DECODE_GPR_WIDTH(raw);
    decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->offset = ARM64_DECODE_SIGNED_FIELD(raw, 23, 5, 2);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_branch_test(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x7E000000U, 0x36000000U))
    {
        return ARM64_DECODE_UNSUPPORTED;
    }
    decoded->instruction = ARM64_DECODE_BIT(raw, 24) ? ARM64_INST_TBNZ : ARM64_INST_TBZ;
    decoded->operand_width = ARM64_DECODE_GPR_WIDTH(raw);
    decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->immediate = ARM64_DECODE_JOIN_BIT_FIELD(raw, 31, 23, 19);
    decoded->offset = ARM64_DECODE_SIGNED_FIELD(raw, 18, 5, 2);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_branch_conditional(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0xFF000010U, 0x54000000U))
    {
        return ARM64_DECODE_UNSUPPORTED;
    }
    decoded->instruction = ARM64_INST_B_COND;
    decoded->condition = ARM64_DECODE_FIELD(raw, 3, 0);
    decoded->offset = ARM64_DECODE_SIGNED_FIELD(raw, 23, 5, 2);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_branch_exception(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    switch (raw & 0xFFE0001FU)
    {
    case 0xD4000001U:
        decoded->instruction = ARM64_INST_SVC;
        break;
    case 0xD4000002U:
        decoded->instruction = ARM64_INST_HVC;
        break;
    case 0xD4000003U:
        decoded->instruction = ARM64_INST_SMC;
        break;
    case 0xD4200000U:
        decoded->instruction = ARM64_INST_BRK;
        break;
    case 0xD4400000U:
        decoded->instruction = ARM64_INST_HLT;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }

    decoded->immediate = ARM64_DECODE_FIELD(raw, 20, 5);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_branch_system(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0xFF000000U, 0xD5000000U))
    {
        return ARM64_DECODE_UNSUPPORTED;
    }

    if (ARM64_DECODE_MATCH(raw, 0xFFFFFFE0U, 0xD50B7420U))
    {
        decoded->instruction = ARM64_INST_DC_ZVA;
        decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->operand_width = 64;
        return ARM64_DECODE_OK;
    }
    switch (raw & 0xFFFFF01FU)
    {
    case 0xD503201FU:
    {
        switch (ARM64_DECODE_FIELD(raw, 11, 5))
        {
        case 0:
            decoded->instruction = ARM64_INST_NOP;
            return ARM64_DECODE_OK;
        case 1:
            decoded->instruction = ARM64_INST_YIELD;
            return ARM64_DECODE_OK;
        case 2:
            decoded->instruction = ARM64_INST_WFE;
            return ARM64_DECODE_OK;
        case 3:
            decoded->instruction = ARM64_INST_WFI;
            return ARM64_DECODE_OK;
        case 4:
            decoded->instruction = ARM64_INST_SEV;
            return ARM64_DECODE_OK;
        case 5:
            decoded->instruction = ARM64_INST_SEVL;
            return ARM64_DECODE_OK;
        case 0x19:
            decoded->instruction = ARM64_INST_PACIASP;
            return ARM64_DECODE_OK;
        case 0x20:
        case 0x22:
        case 0x24:
        case 0x26:
            decoded->instruction = ARM64_INST_BTI;
            decoded->immediate = ARM64_DECODE_FIELD(raw, 11, 5) - 0x20;
            return ARM64_DECODE_OK;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    }
    default:
        break;
    }

    switch (raw & 0xFFFFF0FFU)
    {
    case 0xD503305FU:
        decoded->instruction = ARM64_INST_CLREX;
        decoded->immediate = ARM64_DECODE_FIELD(raw, 11, 8);
        return ARM64_DECODE_OK;
    case 0xD503309FU:
        decoded->instruction = ARM64_INST_DSB;
        decoded->immediate = ARM64_DECODE_FIELD(raw, 11, 8);
        return ARM64_DECODE_OK;
    case 0xD50330BFU:
        decoded->instruction = ARM64_INST_DMB;
        decoded->immediate = ARM64_DECODE_FIELD(raw, 11, 8);
        return ARM64_DECODE_OK;
    case 0xD50330DFU:
        if (ARM64_DECODE_FIELD(raw, 11, 8) != 0xF) return ARM64_DECODE_UNALLOCATED;
        decoded->immediate = 0xF;
        decoded->instruction = ARM64_INST_ISB;
        return ARM64_DECODE_OK;
    default:
        break;
    }

    switch (raw & 0xFFF00000U)
    {
    case 0xD5100000U:
        decoded->instruction = ARM64_INST_MSR_REGISTER;
        decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->sysreg = ARM64_DECODE_FIELD(raw, 20, 5);
        decoded->operand_width = 64;
        return ARM64_DECODE_OK;
    case 0xD5300000U:
        decoded->instruction = ARM64_INST_MRS;
        decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->sysreg = ARM64_DECODE_FIELD(raw, 20, 5);
        decoded->operand_width = 64;
        return ARM64_DECODE_OK;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

static inline enum arm64_decode_status arm64_decode_branch_register(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0xFF000000U, 0xD6000000U))
    {
        return ARM64_DECODE_UNSUPPORTED;
    }

    switch (raw & 0xFFFFFC1FU)
    {
    case 0xD61F0000U:
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->instruction = ARM64_INST_BR;
        decoded->operand_width = 64;
        return ARM64_DECODE_OK;
    case 0xD63F0000U:
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->instruction = ARM64_INST_BLR;
        decoded->operand_width = 64;
        return ARM64_DECODE_OK;
    case 0xD65F0000U:
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->instruction = ARM64_INST_RET;
        decoded->operand_width = 64;
        return ARM64_DECODE_OK;
    default:
        break;
    }

    switch (raw)
    {
    case 0xD69F03E0U:
        decoded->instruction = ARM64_INST_ERET;
        return ARM64_DECODE_OK;
    case 0xD6BF03E0U:
        decoded->instruction = ARM64_INST_DRPS;
        return ARM64_DECODE_OK;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

enum arm64_decode_status arm64_decode_branch_exception_system(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;
    enum arm64_decode_status status;

    switch (ARM64_DECODE_FIELD(raw, 31, 24))
    {
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
    case 0x94:
    case 0x95:
    case 0x96:
    case 0x97:
        status = arm64_decode_branch_immediate(raw, candidate);
        break;
    case 0x34:
    case 0x35:
    case 0xB4:
    case 0xB5:
        status = arm64_decode_branch_compare(raw, candidate);
        break;
    case 0x36:
    case 0x37:
    case 0xB6:
    case 0xB7:
        status = arm64_decode_branch_test(raw, candidate);
        break;
    case 0x54:
        status = arm64_decode_branch_conditional(raw, candidate);
        break;
    case 0xD4:
        status = arm64_decode_branch_exception(raw, candidate);
        break;
    case 0xD5:
        status = arm64_decode_branch_system(raw, candidate);
        break;
    case 0xD6:
        status = arm64_decode_branch_register(raw, candidate);
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    if (status == ARM64_DECODE_OK)
    {
        candidate->instruction_class = ARM64_INSTRUCTION_CLASS_BRANCH_EXCEPTION_SYSTEM;
        *decoded = result;
    }
    return status;
}