#include "arm64_decode.h"

/* ======================== Data Processing -- Register ======================== */

static inline enum arm64_decode_status arm64_decode_register_conditional_compare(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    uint32_t operand;

    switch (raw & 0x3FE00410U)
    {
    case 0x3A400000U:
        break;
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
    operand = ARM64_DECODE_FIELD(raw, 20, 16);
    decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
    decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
    decoded->condition = ARM64_DECODE_FIELD(raw, 15, 12);
    decoded->nzcv = raw & 0xF;
    if (raw & 0x00000800U)
    {
        decoded->instruction = (raw & 0x40000000U) ? ARM64_INST_CCMP_IMMEDIATE : ARM64_INST_CCMN_IMMEDIATE;
        decoded->immediate = operand;
    }
    else
    {
        decoded->instruction = (raw & 0x40000000U) ? ARM64_INST_CCMP_REGISTER : ARM64_INST_CCMN_REGISTER;
        decoded->rm = operand;
    }
    return ARM64_DECODE_OK;
}

/* 解码寄存器移位、扩展、条件选择及算术逻辑编码。 */
enum arm64_decode_status arm64_decode_data_processing_register(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    decoded->instruction_class = ARM64_INSTRUCTION_CLASS_DATA_PROCESSING_REGISTER;

    switch (raw & 0x1F000000U)
    {
    /* AND/BIC/ORR/ORN/EOR/EON/ANDS/BICS (shifted register). */
    case 0x0A000000U:
    {
        switch ((ARM64_DECODE_FIELD(raw, 30, 29) << 1) | ARM64_DECODE_BIT(raw, 21))
        {
        case 0:
            decoded->instruction = ARM64_INST_AND_SHIFTED_REGISTER;
            break;
        case 1:
            decoded->instruction = ARM64_INST_BIC_SHIFTED_REGISTER;
            break;
        case 2:
            decoded->instruction = ARM64_INST_ORR_SHIFTED_REGISTER;
            break;
        case 3:
            decoded->instruction = ARM64_INST_ORN_SHIFTED_REGISTER;
            break;
        case 4:
            decoded->instruction = ARM64_INST_EOR_SHIFTED_REGISTER;
            break;
        case 5:
            decoded->instruction = ARM64_INST_EON_SHIFTED_REGISTER;
            break;
        case 6:
            decoded->instruction = ARM64_INST_ANDS_SHIFTED_REGISTER;
            break;
        case 7:
            decoded->instruction = ARM64_INST_BICS_SHIFTED_REGISTER;
            break;
        }
        decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
        decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
        decoded->shift_type = ARM64_DECODE_FIELD(raw, 23, 22);
        decoded->shift_amount = ARM64_DECODE_FIELD(raw, 15, 10);
        if (decoded->operand_width == 32 && (decoded->shift_amount & 0x20))
        {
            return ARM64_DECODE_UNALLOCATED;
        }
        return ARM64_DECODE_OK;
    }

    /* ADD/ADDS/SUB/SUBS (shifted or extended register). */
    case 0x0B000000U:
    {
        switch (raw & 0x1F200000U)
        {
        /* ADD/ADDS/SUB/SUBS (shifted register). */
        case 0x0B000000U:
        {
            switch (ARM64_DECODE_FIELD(raw, 30, 29))
            {
            case 0:
                decoded->instruction = ARM64_INST_ADD_SHIFTED_REGISTER;
                break;
            case 1:
                decoded->instruction = ARM64_INST_ADDS_SHIFTED_REGISTER;
                break;
            case 2:
                decoded->instruction = ARM64_INST_SUB_SHIFTED_REGISTER;
                break;
            default:
                decoded->instruction = ARM64_INST_SUBS_SHIFTED_REGISTER;
                break;
            }
            decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
            decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
            decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
            decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
            decoded->shift_type = ARM64_DECODE_FIELD(raw, 23, 22);
            decoded->shift_amount = ARM64_DECODE_FIELD(raw, 15, 10);
            if (decoded->shift_type == 3 || (decoded->operand_width == 32 && (decoded->shift_amount & 0x20)))
            {
                return ARM64_DECODE_UNALLOCATED;
            }
            return ARM64_DECODE_OK;
        }

        /* ADD/ADDS/SUB/SUBS (extended register). */
        case 0x0B200000U:
        {
            switch (ARM64_DECODE_FIELD(raw, 30, 29))
            {
            case 0:
                decoded->instruction = ARM64_INST_ADD_EXTENDED_REGISTER;
                break;
            case 1:
                decoded->instruction = ARM64_INST_ADDS_EXTENDED_REGISTER;
                break;
            case 2:
                decoded->instruction = ARM64_INST_SUB_EXTENDED_REGISTER;
                break;
            default:
                decoded->instruction = ARM64_INST_SUBS_EXTENDED_REGISTER;
                break;
            }
            decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
            decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
            decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
            decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
            decoded->extend_type = ARM64_DECODE_FIELD(raw, 15, 13);
            decoded->shift_amount = ARM64_DECODE_FIELD(raw, 12, 10);
            if (decoded->shift_amount > 4)
            {
                return ARM64_DECODE_UNALLOCATED;
            }
            return ARM64_DECODE_OK;
        }
        default:
        {
            return ARM64_DECODE_UNSUPPORTED;
        }
        }
    }

    /* ADC/ADCS/SBC/SBCS, conditional select and register arithmetic. */
    case 0x1A000000U:
    {
        switch (raw & 0x60000000U)
        {
        case 0x00000000U:
        {
            switch (raw & 0x00E00000U)
            {
            case 0x00000000U:
            {
                uint32_t opc = (ARM64_DECODE_BIT(raw, 30) << 1) | ARM64_DECODE_BIT(raw, 29);

                switch (raw & 0x1FE0FC00U)
                {
                case 0x1A000000U:
                    break;
                default:
                    return ARM64_DECODE_UNSUPPORTED;
                }
                decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
                decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
                decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
                decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
                switch (opc)
                {
                case 0:
                    decoded->instruction = ARM64_INST_ADC;
                    break;
                case 1:
                    decoded->instruction = ARM64_INST_ADCS;
                    break;
                case 2:
                    decoded->instruction = ARM64_INST_SBC;
                    break;
                default:
                    decoded->instruction = ARM64_INST_SBCS;
                    break;
                }
                return ARM64_DECODE_OK;
            }

            case 0x00800000U:
            {
                uint32_t opc;

                switch (raw & 0x3FE00000U)
                {
                case 0x1A800000U:
                    break;
                default:
                    return ARM64_DECODE_UNSUPPORTED;
                }
                decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
                decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
                decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
                decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
                if (raw & 0x00000800U)
                {
                    return ARM64_DECODE_UNALLOCATED;
                }
                decoded->condition = ARM64_DECODE_FIELD(raw, 15, 12);
                opc = (ARM64_DECODE_BIT(raw, 30) << 1) | ARM64_DECODE_BIT(raw, 10);
                switch (opc)
                {
                case 0:
                    decoded->instruction = ARM64_INST_CSEL;
                    break;
                case 1:
                    decoded->instruction = ARM64_INST_CSINC;
                    break;
                case 2:
                    decoded->instruction = ARM64_INST_CSINV;
                    break;
                default:
                    decoded->instruction = ARM64_INST_CSNEG;
                    break;
                }
                return ARM64_DECODE_OK;
            }

            case 0x00C00000U:
            {
                uint32_t opc = ARM64_DECODE_FIELD(raw, 15, 10);

                switch (raw & 0x7FE00000U)
                {
                case 0x1AC00000U:
                    break;
                default:
                    return ARM64_DECODE_UNSUPPORTED;
                }
                decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
                decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
                decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
                decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
                switch (opc)
                {
                case 2:
                    decoded->instruction = ARM64_INST_UDIV;
                    break;
                case 3:
                    decoded->instruction = ARM64_INST_SDIV;
                    break;
                case 8:
                    decoded->instruction = ARM64_INST_LSLV;
                    break;
                case 9:
                    decoded->instruction = ARM64_INST_LSRV;
                    break;
                case 10:
                    decoded->instruction = ARM64_INST_ASRV;
                    break;
                case 11:
                    decoded->instruction = ARM64_INST_RORV;
                    break;
                case 0x10:
                    decoded->instruction = ARM64_INST_CRC32B;
                    break;
                case 0x11:
                    decoded->instruction = ARM64_INST_CRC32H;
                    break;
                case 0x12:
                    decoded->instruction = ARM64_INST_CRC32W;
                    break;
                case 0x13:
                    decoded->instruction = ARM64_INST_CRC32X;
                    break;
                case 0x14:
                    decoded->instruction = ARM64_INST_CRC32CB;
                    break;
                case 0x15:
                    decoded->instruction = ARM64_INST_CRC32CH;
                    break;
                case 0x16:
                    decoded->instruction = ARM64_INST_CRC32CW;
                    break;
                case 0x17:
                    decoded->instruction = ARM64_INST_CRC32CX;
                    break;
                case 0x18:
                    decoded->instruction = ARM64_INST_SMAX_REGISTER;
                    break;
                case 0x19:
                    decoded->instruction = ARM64_INST_UMAX_REGISTER;
                    break;
                case 0x1A:
                    decoded->instruction = ARM64_INST_SMIN_REGISTER;
                    break;
                case 0x1B:
                    decoded->instruction = ARM64_INST_UMIN_REGISTER;
                    break;
                default:
                    return ARM64_DECODE_UNSUPPORTED;
                }
                if ((opc >= 0x10 && opc <= 0x12) || (opc >= 0x14 && opc <= 0x16))
                {
                    if (decoded->operand_width != 32)
                    {
                        return ARM64_DECODE_UNALLOCATED;
                    }
                }
                else if (opc == 0x13 || opc == 0x17)
                {
                    if (decoded->operand_width != 64)
                    {
                        return ARM64_DECODE_UNALLOCATED;
                    }
                }
                return ARM64_DECODE_OK;
            }

            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        }
        case 0x20000000U:
            return arm64_decode_register_conditional_compare(raw, decoded);

        case 0x40000000U:
        {
            uint32_t opc;

            switch (raw & 0x7FFF0000U)
            {
            case 0x5AC00000U:
                break;
            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
            decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
            decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
            decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
            opc = ARM64_DECODE_FIELD(raw, 15, 10);
            switch (opc)
            {
            case 0:
                decoded->instruction = ARM64_INST_RBIT;
                break;
            case 1:
                decoded->instruction = ARM64_INST_REV16;
                break;
            case 2:
                decoded->instruction = ARM64_INST_REV32;
                break;
            case 3:
                decoded->instruction = ARM64_INST_REV64;
                break;
            case 4:
                decoded->instruction = ARM64_INST_CLZ;
                break;
            case 5:
                decoded->instruction = ARM64_INST_CLS;
                break;
            case 6:
                decoded->instruction = ARM64_INST_CTZ;
                break;
            case 7:
                decoded->instruction = ARM64_INST_CNT;
                break;
            case 8:
                decoded->instruction = ARM64_INST_ABS;
                break;
            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
            if (decoded->instruction == ARM64_INST_REV64 && decoded->operand_width != 64)
            {
                return ARM64_DECODE_UNALLOCATED;
            }
            return ARM64_DECODE_OK;
        }

        case 0x60000000U:
            return arm64_decode_register_conditional_compare(raw, decoded);

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
    }

    /* MADD/MSUB、SMADDL/SMSUBL、SMULH、UMADDL/UMSUBL、UMULH. */
    case 0x1B000000U:
    {
        uint32_t op31 = ARM64_DECODE_FIELD(raw, 23, 21);
        uint32_t subtract = ARM64_DECODE_BIT(raw, 15);
        uint32_t ra = ARM64_DECODE_FIELD(raw, 14, 10);

        switch (raw & 0x7F000000U)
        {
        case 0x1B000000U:
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        decoded->rd = ARM64_DECODE_FIELD(raw, 4, 0);
        decoded->rn = ARM64_DECODE_FIELD(raw, 9, 5);
        decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
        decoded->rm = ARM64_DECODE_FIELD(raw, 20, 16);
        switch (op31)
        {
        case 0:
            if (subtract)
            {
                decoded->instruction = ARM64_INST_MSUB;
            }
            else
            {
                decoded->instruction = ARM64_INST_MADD;
            }
            break;
        case 1:
            if (decoded->operand_width != 64)
            {
                return ARM64_DECODE_UNALLOCATED;
            }
            if (subtract)
            {
                decoded->instruction = ARM64_INST_SMSUBL;
            }
            else
            {
                decoded->instruction = ARM64_INST_SMADDL;
            }
            break;
        case 2:
            if (decoded->operand_width != 64 || subtract || ra != 31)
            {
                return ARM64_DECODE_UNALLOCATED;
            }
            decoded->instruction = ARM64_INST_SMULH;
            break;
        case 3:
            if (decoded->operand_width != 64)
            {
                return ARM64_DECODE_UNALLOCATED;
            }
            return ARM64_DECODE_UNSUPPORTED;
        case 5:
            if (decoded->operand_width != 64)
            {
                return ARM64_DECODE_UNALLOCATED;
            }
            if (subtract)
            {
                decoded->instruction = ARM64_INST_UMSUBL;
            }
            else
            {
                decoded->instruction = ARM64_INST_UMADDL;
            }
            break;
        case 6:
            if (decoded->operand_width != 64 || subtract || ra != 31)
            {
                return ARM64_DECODE_UNALLOCATED;
            }
            decoded->instruction = ARM64_INST_UMULH;
            break;
        default:
            return ARM64_DECODE_UNALLOCATED;
        }
        if (op31 == 0 || op31 == 1 || op31 == 5)
        {
            decoded->ra = ra;
        }
        return ARM64_DECODE_OK;
    }

    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}
