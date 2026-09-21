#include "arm64_decode.h"

static inline enum arm64_decode_status arm64_decode_register_conditional_compare(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x3FE00410U, 0x3A400000U)) return ARM64_DECODE_UNSUPPORTED;
    uint32_t operand = ARM64_DECODE_FIELD(raw, 20, 16);
    uint32_t immediate = ARM64_DECODE_BIT(raw, 11);
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_BIT_PAIR(raw, 30, 11))
    {
    case 0:
        instruction = ARM64_INST_CCMN_REGISTER;
        break;
    case 1:
        instruction = ARM64_INST_CCMN_IMMEDIATE;
        break;
    case 2:
        instruction = ARM64_INST_CCMP_REGISTER;
        break;
    case 3:
        instruction = ARM64_INST_CCMP_IMMEDIATE;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    decoded->instruction = instruction;
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = ARM64_DECODE_GPR_WIDTH(raw);
    decoded->condition = ARM64_DECODE_FIELD(raw, 15, 12);
    decoded->nzcv = ARM64_DECODE_FIELD(raw, 3, 0);
    if (immediate) decoded->immediate = operand;
    else decoded->rm = operand;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_register_conditional_select(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x3FE00000U, 0x1A800000U)) return ARM64_DECODE_UNSUPPORTED;
    if (ARM64_DECODE_BIT(raw, 11)) return ARM64_DECODE_UNALLOCATED;
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_BIT_PAIR(raw, 30, 10))
    {
    case 0:
        instruction = ARM64_INST_CSEL;
        break;
    case 1:
        instruction = ARM64_INST_CSINC;
        break;
    case 2:
        instruction = ARM64_INST_CSINV;
        break;
    case 3:
        instruction = ARM64_INST_CSNEG;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = ARM64_DECODE_GPR_WIDTH(raw);
    decoded->condition = ARM64_DECODE_FIELD(raw, 15, 12);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_register_logical_shifted(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x1F000000U, 0x0A000000U)) return ARM64_DECODE_UNSUPPORTED;
    uint32_t width = ARM64_DECODE_GPR_WIDTH(raw);
    uint32_t shift = ARM64_DECODE_FIELD(raw, 15, 10);
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_BIT_TRIPLE(raw, 30, 29, 21))
    {
    case 0:
        instruction = ARM64_INST_AND_SHIFTED_REGISTER;
        break;
    case 1:
        instruction = ARM64_INST_BIC_SHIFTED_REGISTER;
        break;
    case 2:
        instruction = ARM64_INST_ORR_SHIFTED_REGISTER;
        break;
    case 3:
        instruction = ARM64_INST_ORN_SHIFTED_REGISTER;
        break;
    case 4:
        instruction = ARM64_INST_EOR_SHIFTED_REGISTER;
        break;
    case 5:
        instruction = ARM64_INST_EON_SHIFTED_REGISTER;
        break;
    case 6:
        instruction = ARM64_INST_ANDS_SHIFTED_REGISTER;
        break;
    case 7:
        instruction = ARM64_INST_BICS_SHIFTED_REGISTER;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    if (shift >= width) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = width;
    decoded->shift_type = ARM64_DECODE_FIELD(raw, 23, 22);
    decoded->shift_amount = shift;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_register_add_sub_shifted(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x1F200000U, 0x0B000000U)) return ARM64_DECODE_UNSUPPORTED;
    uint32_t width = ARM64_DECODE_GPR_WIDTH(raw);
    uint32_t shift = ARM64_DECODE_FIELD(raw, 15, 10);
    uint32_t shift_type = ARM64_DECODE_FIELD(raw, 23, 22);
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_FIELD(raw, 30, 29))
    {
    case 0:
        instruction = ARM64_INST_ADD_SHIFTED_REGISTER;
        break;
    case 1:
        instruction = ARM64_INST_ADDS_SHIFTED_REGISTER;
        break;
    case 2:
        instruction = ARM64_INST_SUB_SHIFTED_REGISTER;
        break;
    case 3:
        instruction = ARM64_INST_SUBS_SHIFTED_REGISTER;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    if (shift_type == 3 || shift >= width) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = width;
    decoded->shift_type = shift_type;
    decoded->shift_amount = shift;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_register_add_sub_extended(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x1F200000U, 0x0B200000U)) return ARM64_DECODE_UNSUPPORTED;
    if (ARM64_DECODE_FIELD(raw, 23, 22)) return ARM64_DECODE_UNALLOCATED;
    uint32_t shift = ARM64_DECODE_FIELD(raw, 12, 10);
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_FIELD(raw, 30, 29))
    {
    case 0:
        instruction = ARM64_INST_ADD_EXTENDED_REGISTER;
        break;
    case 1:
        instruction = ARM64_INST_ADDS_EXTENDED_REGISTER;
        break;
    case 2:
        instruction = ARM64_INST_SUB_EXTENDED_REGISTER;
        break;
    case 3:
        instruction = ARM64_INST_SUBS_EXTENDED_REGISTER;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    if (shift > 4) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = ARM64_DECODE_GPR_WIDTH(raw);
    decoded->extend_type = ARM64_DECODE_FIELD(raw, 15, 13);
    decoded->shift_amount = shift;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_register_carry(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x1FE0FC00U, 0x1A000000U)) return ARM64_DECODE_UNSUPPORTED;
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_FIELD(raw, 30, 29))
    {
    case 0:
        instruction = ARM64_INST_ADC;
        break;
    case 1:
        instruction = ARM64_INST_ADCS;
        break;
    case 2:
        instruction = ARM64_INST_SBC;
        break;
    case 3:
        instruction = ARM64_INST_SBCS;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = ARM64_DECODE_GPR_WIDTH(raw);
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_register_two_source(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x7FE00000U, 0x1AC00000U)) return ARM64_DECODE_UNSUPPORTED;
    uint32_t opcode = ARM64_DECODE_FIELD(raw, 15, 10);
    uint32_t width = ARM64_DECODE_GPR_WIDTH(raw);
    enum arm64_instruction instruction;
    switch (opcode)
    {
    case 2:
        instruction = ARM64_INST_UDIV;
        break;
    case 3:
        instruction = ARM64_INST_SDIV;
        break;
    case 8:
        instruction = ARM64_INST_LSLV;
        break;
    case 9:
        instruction = ARM64_INST_LSRV;
        break;
    case 10:
        instruction = ARM64_INST_ASRV;
        break;
    case 11:
        instruction = ARM64_INST_RORV;
        break;
    case 0x10:
        instruction = ARM64_INST_CRC32B;
        break;
    case 0x11:
        instruction = ARM64_INST_CRC32H;
        break;
    case 0x12:
        instruction = ARM64_INST_CRC32W;
        break;
    case 0x13:
        instruction = ARM64_INST_CRC32X;
        break;
    case 0x14:
        instruction = ARM64_INST_CRC32CB;
        break;
    case 0x15:
        instruction = ARM64_INST_CRC32CH;
        break;
    case 0x16:
        instruction = ARM64_INST_CRC32CW;
        break;
    case 0x17:
        instruction = ARM64_INST_CRC32CX;
        break;
    case 0x18:
        instruction = ARM64_INST_SMAX_REGISTER;
        break;
    case 0x19:
        instruction = ARM64_INST_UMAX_REGISTER;
        break;
    case 0x1A:
        instruction = ARM64_INST_SMIN_REGISTER;
        break;
    case 0x1B:
        instruction = ARM64_INST_UMIN_REGISTER;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    if (((opcode >= 0x10 && opcode <= 0x12) || (opcode >= 0x14 && opcode <= 0x16)) && width != 32) return ARM64_DECODE_UNALLOCATED;
    if ((opcode == 0x13 || opcode == 0x17) && width != 64) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = width;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_register_one_source(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x7FFF0000U, 0x5AC00000U)) return ARM64_DECODE_UNSUPPORTED;
    uint32_t width = ARM64_DECODE_GPR_WIDTH(raw);
    enum arm64_instruction instruction;
    switch (ARM64_DECODE_FIELD(raw, 15, 10))
    {
    case 0:
        instruction = ARM64_INST_RBIT;
        break;
    case 1:
        instruction = ARM64_INST_REV16;
        break;
    case 2:
        instruction = ARM64_INST_REV32;
        break;
    case 3:
        instruction = ARM64_INST_REV64;
        break;
    case 4:
        instruction = ARM64_INST_CLZ;
        break;
    case 5:
        instruction = ARM64_INST_CLS;
        break;
    case 6:
        instruction = ARM64_INST_CTZ;
        break;
    case 7:
        instruction = ARM64_INST_CNT;
        break;
    case 8:
        instruction = ARM64_INST_ABS;
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    if (instruction == ARM64_INST_REV64 && width != 64) return ARM64_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = width;
    return ARM64_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_register_three_source(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM64_DECODE_MATCH(raw, 0x7F000000U, 0x1B000000U)) return ARM64_DECODE_UNSUPPORTED;
    uint32_t opcode = ARM64_DECODE_FIELD(raw, 23, 21);
    uint32_t subtract = ARM64_DECODE_BIT(raw, 15);
    uint32_t accumulator = ARM64_DECODE_FIELD(raw, 14, 10);
    uint32_t width = ARM64_DECODE_GPR_WIDTH(raw);
    enum arm64_instruction instruction;
    switch (opcode)
    {
    case 0:
        instruction = subtract ? ARM64_INST_MSUB : ARM64_INST_MADD;
        break;
    case 1:
        if (width != 64) return ARM64_DECODE_UNALLOCATED;
        instruction = subtract ? ARM64_INST_SMSUBL : ARM64_INST_SMADDL;
        break;
    case 2:
        if (width != 64 || subtract || accumulator != 31) return ARM64_DECODE_UNALLOCATED;
        instruction = ARM64_INST_SMULH;
        break;
    case 3:
        return width == 64 ? ARM64_DECODE_UNSUPPORTED : ARM64_DECODE_UNALLOCATED;
    case 5:
        if (width != 64) return ARM64_DECODE_UNALLOCATED;
        instruction = subtract ? ARM64_INST_UMSUBL : ARM64_INST_UMADDL;
        break;
    case 6:
        if (width != 64 || subtract || accumulator != 31) return ARM64_DECODE_UNALLOCATED;
        instruction = ARM64_INST_UMULH;
        break;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }
    decoded->instruction = instruction;
    decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = width;
    if (opcode == 0 || opcode == 1 || opcode == 5) decoded->ra = accumulator;
    return ARM64_DECODE_OK;
}

enum arm64_decode_status arm64_decode_data_processing_register(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    struct arm64_decoded_instruction result = *decoded;
    struct arm64_decoded_instruction *candidate = &result;
    enum arm64_decode_status status;
    switch (raw & 0x1F000000U)
    {
    case 0x0A000000U:
        status = arm64_decode_register_logical_shifted(raw, candidate);
        break;
    case 0x0B000000U:
        status = ARM64_DECODE_BIT(raw, 21) ? arm64_decode_register_add_sub_extended(raw, candidate) : arm64_decode_register_add_sub_shifted(raw, candidate);
        break;
    case 0x1A000000U:
        switch (raw & 0x00E00000U)
        {
        case 0x00000000U:
            status = arm64_decode_register_carry(raw, candidate);
            break;
        case 0x00400000U:
            status = arm64_decode_register_conditional_compare(raw, candidate);
            break;
        case 0x00800000U:
            status = arm64_decode_register_conditional_select(raw, candidate);
            break;
        case 0x00C00000U:
            status = ARM64_DECODE_BIT(raw, 30) ? arm64_decode_register_one_source(raw, candidate) : arm64_decode_register_two_source(raw, candidate);
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        break;
    case 0x1B000000U:
        status = arm64_decode_register_three_source(raw, candidate);
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    if (status == ARM64_DECODE_OK)
    {
        candidate->instruction_class = ARM64_INSTRUCTION_CLASS_DATA_PROCESSING_REGISTER;
        *decoded = result;
    }
    return status;
}