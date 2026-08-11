#include "arm64_decode_internal.h"

#define ARM64_SYSREG_INSN_MASK 0xFFF00000U
#define ARM64_SYSREG_MRS_INSN  0xD5300000U
#define ARM64_SYSREG_MSR_INSN  0xD5100000U
#define ARM64_HINT_INSN        0xD503201FU

/* 分支偏移在这里完成符号扩展和缩放，统一以字节为单位返回。 */
enum arm64_decode_status arm64_decode_branch(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;

    if ((raw & 0x7C000000U) == 0x14000000U)
    {
        decoded->instruction = (raw & 0x80000000U) ? ARM64_INSN_BL : ARM64_INSN_B;
        decoded->offset = arm64_sign_extend((uint64_t)(raw & 0x03FFFFFFU) << 2, 28);
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7E000000U) == 0x34000000U)
    {
        decoded->instruction = (raw & 0x01000000U) ? ARM64_INSN_CBNZ : ARM64_INSN_CBZ;
        decoded->operand_width = (raw & 0x80000000U) ? 64 : 32;
        decoded->rt = raw & 0x1F;
        decoded->offset = arm64_sign_extend((uint64_t)((raw >> 5) & 0x7FFFFU) << 2, 21);
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7E000000U) == 0x36000000U)
    {
        decoded->instruction = (raw & 0x01000000U) ? ARM64_INSN_TBNZ : ARM64_INSN_TBZ;
        decoded->operand_width = (raw & 0x80000000U) ? 64 : 32;
        decoded->rt = raw & 0x1F;
        decoded->test_bit = ((raw >> 26) & 0x20) | ((raw >> 19) & 0x1F);
        decoded->offset = arm64_sign_extend((uint64_t)((raw >> 5) & 0x3FFFU) << 2, 16);
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF000010U) == 0x54000000U)
    {
        decoded->instruction = ARM64_INSN_B_COND;
        decoded->condition = raw & 0xF;
        decoded->offset = arm64_sign_extend((uint64_t)((raw >> 5) & 0x7FFFFU) << 2, 21);
        return ARM64_DECODE_OK;
    }

    switch (raw >> 24)
    {
    case 0xD4:
    {
        enum arm64_instruction instruction;

        switch (raw & 0xFFE0001FU)
        {
        case 0xD4000001U:
            instruction = ARM64_INSN_SVC;
            break;
        case 0xD4000002U:
            instruction = ARM64_INSN_HVC;
            break;
        case 0xD4000003U:
            instruction = ARM64_INSN_SMC;
            break;
        case 0xD4200000U:
            instruction = ARM64_INSN_BRK;
            break;
        case 0xD4400000U:
            instruction = ARM64_INSN_HLT;
            break;
        default:
            return ARM64_DECODE_UNSUPPORTED;
        }

        decoded->instruction = instruction;
        decoded->immediate = (raw >> 5) & 0xFFFF;
        return ARM64_DECODE_OK;
    }
    case 0xD5:
    {
        if ((raw & 0xFFFFF01FU) == ARM64_HINT_INSN)
        {
            switch ((raw >> 5) & 0x7F)
            {
            case 0:
                decoded->instruction = ARM64_INSN_NOP;
                return ARM64_DECODE_OK;
            case 1:
                decoded->instruction = ARM64_INSN_YIELD;
                return ARM64_DECODE_OK;
            case 2:
                decoded->instruction = ARM64_INSN_WFE;
                return ARM64_DECODE_OK;
            case 3:
                decoded->instruction = ARM64_INSN_WFI;
                return ARM64_DECODE_OK;
            case 4:
                decoded->instruction = ARM64_INSN_SEV;
                return ARM64_DECODE_OK;
            case 5:
                decoded->instruction = ARM64_INSN_SEVL;
                return ARM64_DECODE_OK;
            case 0x19:
                decoded->instruction = ARM64_INSN_PACIASP;
                return ARM64_DECODE_OK;
            case 0x20:
            case 0x22:
            case 0x24:
            case 0x26:
                decoded->instruction = ARM64_INSN_BTI;
                decoded->option = ((raw >> 5) & 0x7F) - 0x20;
                return ARM64_DECODE_OK;
            default:
                return ARM64_DECODE_UNSUPPORTED;
            }
        }

        switch (raw & 0xFFFFF0FFU)
        {
        case 0xD503305FU:
            decoded->instruction = ARM64_INSN_CLREX;
            decoded->option = (raw >> 8) & 0xF;
            return ARM64_DECODE_OK;
        case 0xD503309FU:
            decoded->instruction = ARM64_INSN_DSB;
            decoded->option = (raw >> 8) & 0xF;
            return ARM64_DECODE_OK;
        case 0xD50330BFU:
            decoded->instruction = ARM64_INSN_DMB;
            decoded->option = (raw >> 8) & 0xF;
            return ARM64_DECODE_OK;
        case 0xD50330DFU:
            decoded->option = (raw >> 8) & 0xF;
            if (decoded->option != 0xF) return ARM64_DECODE_UNALLOCATED;
            decoded->instruction = ARM64_INSN_ISB;
            return ARM64_DECODE_OK;
        default:
            break;
        }

        switch (raw & ARM64_SYSREG_INSN_MASK)
        {
        case ARM64_SYSREG_MSR_INSN:
            decoded->instruction = ARM64_INSN_MSR_REGISTER;
            decoded->rt = raw & 0x1F;
            decoded->sysreg = (raw >> 5) & 0xFFFF;
            return ARM64_DECODE_OK;
        case ARM64_SYSREG_MRS_INSN:
            decoded->instruction = ARM64_INSN_MRS;
            decoded->rt = raw & 0x1F;
            decoded->sysreg = (raw >> 5) & 0xFFFF;
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
            decoded->rn = (raw >> 5) & 0x1F;
            decoded->instruction = ARM64_INSN_BR;
            return ARM64_DECODE_OK;
        case 0xD63F0000U:
            decoded->rn = (raw >> 5) & 0x1F;
            decoded->instruction = ARM64_INSN_BLR;
            return ARM64_DECODE_OK;
        case 0xD65F0000U:
            decoded->rn = (raw >> 5) & 0x1F;
            decoded->instruction = ARM64_INSN_RET;
            return ARM64_DECODE_OK;
        default:
            break;
        }

        switch (raw)
        {
        case 0xD69F03E0U:
            decoded->instruction = ARM64_INSN_ERET;
            return ARM64_DECODE_OK;
        case 0xD6BF03E0U:
            decoded->instruction = ARM64_INSN_DRPS;
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