#include "arm64_decode.h"

#define ARM64_SYSREG_INSN_MASK 0xFFF00000U
#define ARM64_SYSREG_MRS_INSN  0xD5300000U
#define ARM64_SYSREG_MSR_INSN  0xD5100000U
#define ARM64_HINT_NOP_INSN    0xD503201FU

static arm64_s64 arm64_sign_extend(arm64_u64 value, arm64_u8 bits)
{
    arm64_u64 sign = 1ULL << (bits - 1);

    return (arm64_s64)((value ^ sign) - sign);
}

static void arm64_decode_sysreg(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    struct arm64_system_operands *system = &decoded->operands.system;

    decoded->rt = raw & 0x1F;
    system->op0 = (raw >> 19) & 0x3;
    system->op1 = (raw >> 16) & 0x7;
    system->crn = (raw >> 12) & 0xF;
    system->crm = (raw >> 8) & 0xF;
    system->op2 = (raw >> 5) & 0x7;
}

/* 分支偏移在这里完成符号扩展和缩放，统一以字节为单位返回。 */
enum arm64_decode_status arm64_decode_branch(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    struct arm64_system_operands *system = &decoded->operands.system;
    arm64_u32 branch_reg;
    arm64_u32 iclass = (raw >> 25) & 0xF;

    if (raw == ARM64_HINT_NOP_INSN)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = ARM64_OP_NOP;
        decoded->flags = ARM64_INSN_FLAG_RELOCATABLE;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFFFFF01FU) == ARM64_HINT_NOP_INSN)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->flags = ARM64_INSN_FLAG_RELOCATABLE;
        switch ((raw >> 5) & 0x7F)
        {
        case 1:
            decoded->opcode = ARM64_OP_HINT;
            system->operation = ARM64_SYSTEM_OP_YIELD;
            return ARM64_DECODE_OK;
        case 2:
            decoded->opcode = ARM64_OP_HINT;
            system->operation = ARM64_SYSTEM_OP_WFE;
            return ARM64_DECODE_OK;
        case 3:
            decoded->opcode = ARM64_OP_HINT;
            system->operation = ARM64_SYSTEM_OP_WFI;
            return ARM64_DECODE_OK;
        case 4:
            decoded->opcode = ARM64_OP_HINT;
            system->operation = ARM64_SYSTEM_OP_SEV;
            return ARM64_DECODE_OK;
        case 5:
            decoded->opcode = ARM64_OP_HINT;
            system->operation = ARM64_SYSTEM_OP_SEVL;
            return ARM64_DECODE_OK;
        default:
            decoded->opcode = ARM64_OP_UNKNOWN;
            return ARM64_DECODE_UNSUPPORTED;
        }
    }

    if ((raw & 0xFFFFF0FFU) == 0xD503305FU || (raw & 0xFFFFF0FFU) == 0xD503309FU || (raw & 0xFFFFF0FFU) == 0xD50330BFU || (raw & 0xFFFFF0FFU) == 0xD50330DFU)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = ARM64_OP_BARRIER;
        decoded->flags = ARM64_INSN_FLAG_RELOCATABLE;
        system->option = (raw >> 8) & 0xF;
        switch (raw & 0xFF)
        {
        case 0x5F:
            system->operation = ARM64_SYSTEM_OP_CLREX;
            break;
        case 0x9F:
            system->operation = ARM64_SYSTEM_OP_DSB;
            break;
        case 0xBF:
            system->operation = ARM64_SYSTEM_OP_DMB;
            break;
        default:
            if (system->option != 0xF) return ARM64_DECODE_UNALLOCATED;
            system->operation = ARM64_SYSTEM_OP_ISB;
            break;
        }
        return ARM64_DECODE_OK;
    }

    switch (raw & 0xFFE0001FU)
    {
    case 0xD4000001U:
        system->operation = ARM64_SYSTEM_OP_SVC;
        break;
    case 0xD4000002U:
        system->operation = ARM64_SYSTEM_OP_HVC;
        break;
    case 0xD4000003U:
        system->operation = ARM64_SYSTEM_OP_SMC;
        break;
    case 0xD4200000U:
        system->operation = ARM64_SYSTEM_OP_BRK;
        break;
    case 0xD4400000U:
        system->operation = ARM64_SYSTEM_OP_HLT;
        break;
    default:
        system->operation = ARM64_SYSTEM_OP_NONE;
        break;
    }
    if (system->operation != ARM64_SYSTEM_OP_NONE)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = ARM64_OP_EXCEPTION_GENERATION;
        system->immediate = (raw >> 5) & 0xFFFF;
        return ARM64_DECODE_OK;
    }

    if (raw == 0xD69F03E0U || raw == 0xD6BF03E0U)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = ARM64_OP_EXCEPTION_RETURN;
        system->operation = raw == 0xD69F03E0U ? ARM64_SYSTEM_OP_ERET : ARM64_SYSTEM_OP_DRPS;
        return ARM64_DECODE_OK;
    }

    if ((raw & ARM64_SYSREG_INSN_MASK) == ARM64_SYSREG_MRS_INSN || (raw & ARM64_SYSREG_INSN_MASK) == ARM64_SYSREG_MSR_INSN)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = (raw & ARM64_SYSREG_INSN_MASK) == ARM64_SYSREG_MRS_INSN ? ARM64_OP_MRS : ARM64_OP_MSR_REGISTER;
        arm64_decode_sysreg(raw, decoded);
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFC000000U) == 0x14000000U || (raw & 0xFC000000U) == 0x94000000U)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = (raw & 0x80000000U) ? ARM64_OP_BL : ARM64_OP_B;
        decoded->operands.branch.offset = arm64_sign_extend((arm64_u64)(raw & 0x03FFFFFFU) << 2, 28);
        return ARM64_DECODE_OK;
    }

    branch_reg = raw & 0xFFFFFC1FU;
    if (branch_reg == 0xD61F0000U || branch_reg == 0xD63F0000U || branch_reg == 0xD65F0000U)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->rn = (raw >> 5) & 0x1F;
        if (branch_reg == 0xD61F0000U) decoded->opcode = ARM64_OP_BR;
        else if (branch_reg == 0xD63F0000U) decoded->opcode = ARM64_OP_BLR;
        else decoded->opcode = ARM64_OP_RET;
        return ARM64_DECODE_OK;
    }

    if ((raw & 0xFF000010U) == 0x54000000U)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = ARM64_OP_B_COND;
        decoded->operands.branch.condition = raw & 0xF;
        decoded->operands.branch.offset = arm64_sign_extend((arm64_u64)((raw >> 5) & 0x7FFFFU) << 2, 21);
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7E000000U) == 0x34000000U)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = (raw & 0x01000000U) ? ARM64_OP_CBNZ : ARM64_OP_CBZ;
        decoded->flags = (raw & 0x80000000U) ? ARM64_INSN_FLAG_64BIT : 0;
        decoded->operand_width = (raw & 0x80000000U) ? 64 : 32;
        decoded->rt = raw & 0x1F;
        decoded->operands.branch.offset = arm64_sign_extend((arm64_u64)((raw >> 5) & 0x7FFFFU) << 2, 21);
        return ARM64_DECODE_OK;
    }

    if ((raw & 0x7E000000U) == 0x36000000U)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = (raw & 0x01000000U) ? ARM64_OP_TBNZ : ARM64_OP_TBZ;
        decoded->rt = raw & 0x1F;
        decoded->operands.branch.test_bit = ((raw >> 26) & 0x20) | ((raw >> 19) & 0x1F);
        decoded->operands.branch.offset = arm64_sign_extend((arm64_u64)((raw >> 5) & 0x3FFFU) << 2, 16);
        return ARM64_DECODE_OK;
    }

    if ((iclass & 0xE) == 0xA)
    {
        decoded->insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded->opcode = ARM64_OP_UNKNOWN;
        return ARM64_DECODE_UNSUPPORTED;
    }

    return ARM64_DECODE_NO_MATCH;
}