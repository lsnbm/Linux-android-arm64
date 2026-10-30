/*
ARM64 指令解码器：数据处理寄存器类。

解析移位/扩展寄存器运算、ADC/SBC、条件选择与条件比较、除法、可变移位、CRC、位计数/反转、最值、乘加和高位乘法。
*/
#include "linux_arm92_decode.h"

static inline enum linux_arm92_decode_status linux_arm92_decode_register_conditional_compare(uint32_t raw, struct linux_arm92_decoded_instruction *decoded)
{
    if (!ARM92_DECODE_MATCH(raw, 0x3FE00410U, 0x3A400000U)) return ARM64_DECODE_UNSUPPORTED;
    uint32_t operand = ARM92_DECODE_FIELD(raw, 20, 16);
    uint32_t immediate = ARM92_DECODE_BIT(raw, 11);
    enum arm64_instruction instruction;
    switch (ARM92_DECODE_BIT_PAIR(raw, 30, 11))
    {
    case 0:
        instruction = ARM92_INST_CCMN_REGISTER;
        break;
    case 1:
        instruction = ARM92_INST_CCMN_IMMEDIATE;
        break;
    case 2:
        instruction = ARM92_INST_CCMP_REGISTER;
        break;
    case 3:
        instruction = ARM92_INST_CCMP_IMMEDIATE;
        break;
    default:
        return ARM92_DECODE_UNSUPPORTED;
    }
    decoded->instruction = instruction;
    decoded->rn = ARM92_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = ARM92_DECODE_GPR_WIDTH(raw);
    decoded->condition = ARM92_DECODE_FIELD(raw, 15, 12);
    decoded->nzcv = ARM92_DECODE_FIELD(raw, 3, 0);
    if (immediate) decoded->immediate = operand;
    else decoded->rm = operand;
    return ARM92_DECODE_OK;
}

static inline enum arm64_decode_status arm64_decode_register_conditional_select(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM92_DECODE_MATCH(raw, 0x3FE00000U, 0x1A800000U)) return ARM92_DECODE_UNSUPPORTED;
    if (ARM92_DECODE_BIT(raw, 11)) return ARM92_DECODE_UNALLOCATED;
    enum linux_arm92_instruction instruction;
    switch (ARM92_DECODE_BIT_PAIR(raw, 30, 10))
    {
    case 0:
        instruction = ARM92_INST_CSEL;
        break;
    case 1:
        instruction = ARM92_INST_CSINC;
        break;
    case 2:
        instruction = ARM92_INST_CSINV;
        break;
    case 3:
        instruction = ARM92_INST_CSNEG;
        break;
    default:
        return ARM92_DECODE_UNSUPPORTED;
    }
    decoded->instruction = instruction;
    decoded->rd = ARM92_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM92_DECODE_FIELD(raw, 9, 5);
    decoded->rm = ARM92_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = ARM92_DECODE_GPR_WIDTH(raw);
    decoded->condition = ARM92_DECODE_FIELD(raw, 15, 12);
    return ARM64_DECODE_OK;
}

static inline enum linux_arm92_decode_status arm64_decode_register_logical_shifted(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (!ARM92_DECODE_MATCH(raw, 0x1F000000U, 0x0A000000U)) return ARM64_DECODE_UNSUPPORTED;
    uint32_t width = ARM92_DECODE_GPR_WIDTH(raw);
    uint32_t shift = ARM92_DECODE_FIELD(raw, 15, 10);
    enum arm64_instruction instruction;
    switch (ARM92_DECODE_BIT_TRIPLE(raw, 30, 29, 21))
    {
    case 0:
        instruction = ARM92_INST_AND_SHIFTED_REGISTER;
        break;
    case 1:
        instruction = ARM92_INST_BIC_SHIFTED_REGISTER;
        break;
    case 2:
        instruction = ARM92_INST_ORR_SHIFTED_REGISTER;
        break;
    case 3:
        instruction = ARM92_INST_ORN_SHIFTED_REGISTER;
        break;
    case 4:
        instruction = ARM92_INST_EOR_SHIFTED_REGISTER;
        break;
    case 5:
        instruction = ARM92_INST_EON_SHIFTED_REGISTER;
        break;
    case 6:
        instruction = ARM92_INST_ANDS_SHIFTED_REGISTER;
        break;
    case 7:
        instruction = ARM92_INST_BICS_SHIFTED_REGISTER;
        break;
    default:
        return ARM92_DECODE_UNSUPPORTED;
    }
    if (shift >= width) return ARM92_DECODE_UNALLOCATED;
    decoded->instruction = instruction;
    decoded->rd = ARM92_DECODE_FIELD(raw, 4, 0);
    decoded->rn = ARM92_DECODE_FIELD(raw, 9, 5);
    decoded->rm = ARM92_DECODE_FIELD(raw, 20, 16);
    decoded->operand_width = width;
    decoded->shift_type = ARM92_DECODE_FIELD(raw, 23, 22);
    decoded->shift_amount = shift;
    return ARM92_DECODE_OK;
}

    
