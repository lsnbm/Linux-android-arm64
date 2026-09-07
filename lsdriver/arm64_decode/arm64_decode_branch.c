#include "arm64_decode.h"

/* 分支偏移在这里完成符号扩展和缩放，统一以字节为单位返回。 */
enum arm64_decode_status arm64_decode_branch_exception_system(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    decoded->instruction_class = ARM64_INSTRUCTION_CLASS_BRANCH_EXCEPTION_SYSTEM;

    switch (ARM64_DECODE_FIELD(raw, 31, 24))
    {
    case 0x14:
    case 0x15:
    case 0x16:
    case 0x17:
        switch (ARM64_DECODE_BIT(raw, 31))
        {
        case 0:
            decoded->instruction = ARM64_INST_B;
            break;
        default:
            decoded->instruction = ARM64_INST_BL;
            break;
        }
        decoded->offset = ARM64_DECODE_SIGN_EXTEND((uint64_t)ARM64_DECODE_FIELD(raw, 25, 0) << 2, 28);
        return ARM64_DECODE_OK;

    case 0x34:
    case 0x35:
        switch (raw & 0x7E000000U)
        {
        case 0x34000000U:
            switch (ARM64_DECODE_BIT(raw, 24))
            {
            case 0:
                decoded->instruction = ARM64_INST_CBZ;
                break;
            default:
                decoded->instruction = ARM64_INST_CBNZ;
                break;
            }
            decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
            decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
            decoded->offset = ARM64_DECODE_SIGN_EXTEND((uint64_t)ARM64_DECODE_FIELD(raw, 23, 5) << 2, 21);
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }

    case 0x36:
    case 0x37:
        switch (raw & 0x7E000000U)
        {
        case 0x36000000U:
            switch (ARM64_DECODE_BIT(raw, 24))
            {
            case 0:
                decoded->instruction = ARM64_INST_TBZ;
                break;
            default:
                decoded->instruction = ARM64_INST_TBNZ;
                break;
            }
            decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
            decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
            decoded->immediate = (ARM64_DECODE_BIT(raw, 31) << 5) | ARM64_DECODE_FIELD(raw, 23, 19);
            decoded->offset = ARM64_DECODE_SIGN_EXTEND((uint64_t)ARM64_DECODE_FIELD(raw, 18, 5) << 2, 16);
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }

    case 0x54:
        switch (raw & 0xFF000010U)
        {
        case 0x54000000U:
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }
        decoded->instruction = ARM64_INST_B_COND;
        decoded->condition = ARM64_DECODE_FIELD(raw, 3, 0);
        decoded->offset = ARM64_DECODE_SIGN_EXTEND((uint64_t)ARM64_DECODE_FIELD(raw, 23, 5) << 2, 21);
        return ARM64_DECODE_OK;

    case 0x94:
    case 0x95:
    case 0x96:
    case 0x97:
        switch (ARM64_DECODE_BIT(raw, 31))
        {
        case 0:
            decoded->instruction = ARM64_INST_B;
            break;
        default:
            decoded->instruction = ARM64_INST_BL;
            break;
        }
        decoded->offset = ARM64_DECODE_SIGN_EXTEND((uint64_t)ARM64_DECODE_FIELD(raw, 25, 0) << 2, 28);
        return ARM64_DECODE_OK;

    case 0xB4:
    case 0xB5:
        switch (raw & 0x7E000000U)
        {
        case 0x34000000U:
            switch (ARM64_DECODE_BIT(raw, 24))
            {
            case 0:
                decoded->instruction = ARM64_INST_CBZ;
                break;
            default:
                decoded->instruction = ARM64_INST_CBNZ;
                break;
            }
            decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
            decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
            decoded->offset = ARM64_DECODE_SIGN_EXTEND((uint64_t)ARM64_DECODE_FIELD(raw, 23, 5) << 2, 21);
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }

    case 0xB6:
    case 0xB7:
        switch (raw & 0x7E000000U)
        {
        case 0x36000000U:
            switch (ARM64_DECODE_BIT(raw, 24))
            {
            case 0:
                decoded->instruction = ARM64_INST_TBZ;
                break;
            default:
                decoded->instruction = ARM64_INST_TBNZ;
                break;
            }
            decoded->operand_width = ARM64_DECODE_BIT(raw, 31) ? 64 : 32;
            decoded->rt = ARM64_DECODE_FIELD(raw, 4, 0);
            decoded->immediate = (ARM64_DECODE_BIT(raw, 31) << 5) | ARM64_DECODE_FIELD(raw, 23, 19);
            decoded->offset = ARM64_DECODE_SIGN_EXTEND((uint64_t)ARM64_DECODE_FIELD(raw, 18, 5) << 2, 16);
            return ARM64_DECODE_OK;

        default:
            return ARM64_DECODE_UNSUPPORTED;
        }

    case 0xD4:
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

    case 0xD5:
    {
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
            decoded->immediate = ARM64_DECODE_FIELD(raw, 11, 8);
            if (decoded->immediate != 0xF) return ARM64_DECODE_UNALLOCATED;
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
    case 0xD6:
    {
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
    default:
        break;
    }

    return ARM64_DECODE_UNSUPPORTED;
}