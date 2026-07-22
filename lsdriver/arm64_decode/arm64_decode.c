#include "arm64_decode.h"

enum arm64_decode_status arm64_decode_data_processing_immediate(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_data_processing_register(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_ldst(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_branch(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_simd(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_sve(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_sme(arm64_u32 raw, struct arm64_decoded_insn *decoded);

static arm64_u64 arm64_decode_effects(const struct arm64_decoded_insn *decoded)
{
    arm64_u64 effects = 0;

    if (!decoded) return 0;

    if (decoded->flags & ARM64_INSN_FLAG_LOAD) effects |= ARM64_EFFECT_READ_MEMORY;
    if (decoded->flags & ARM64_INSN_FLAG_STORE) effects |= ARM64_EFFECT_WRITE_MEMORY;
    if (decoded->flags & ARM64_INSN_FLAG_SETFLAGS) effects |= ARM64_EFFECT_WRITE_FLAGS;
    if (decoded->flags & ARM64_INSN_FLAG_WRITEBACK) effects |= ARM64_EFFECT_WRITEBACK | ARM64_EFFECT_WRITE_GPR;
    if (decoded->flags & ARM64_INSN_FLAG_FP)
    {
        if (decoded->flags & ARM64_INSN_FLAG_LOAD) effects |= ARM64_EFFECT_WRITE_FP_SIMD;
        if (decoded->flags & ARM64_INSN_FLAG_STORE) effects |= ARM64_EFFECT_READ_FP_SIMD;
    }
    else
    {
        if (decoded->flags & ARM64_INSN_FLAG_LOAD) effects |= ARM64_EFFECT_WRITE_GPR;
        if (decoded->flags & ARM64_INSN_FLAG_STORE) effects |= ARM64_EFFECT_READ_GPR;
    }

    switch (decoded->opcode)
    {
    case ARM64_OP_NOP:
        break;
    case ARM64_OP_HINT:
    case ARM64_OP_BARRIER:
    case ARM64_OP_MRS:
    case ARM64_OP_MSR_REGISTER:
        effects |= ARM64_EFFECT_SYSTEM;
        if (decoded->opcode == ARM64_OP_HINT && decoded->operands.system.operation == ARM64_SYSTEM_OP_PACIASP) effects |= ARM64_EFFECT_READ_GPR | ARM64_EFFECT_WRITE_GPR;
        if (decoded->opcode == ARM64_OP_BARRIER) effects |= ARM64_EFFECT_BARRIER;
        if (decoded->opcode == ARM64_OP_MRS) effects |= ARM64_EFFECT_WRITE_GPR;
        if (decoded->opcode == ARM64_OP_MSR_REGISTER) effects |= ARM64_EFFECT_READ_GPR;
        break;
    case ARM64_OP_EXCEPTION_GENERATION:
    case ARM64_OP_EXCEPTION_RETURN:
        effects |= ARM64_EFFECT_SYSTEM | ARM64_EFFECT_EXCEPTION | ARM64_EFFECT_CONTROL_FLOW;
        if (decoded->opcode == ARM64_OP_EXCEPTION_RETURN) effects |= ARM64_EFFECT_RETURN | ARM64_EFFECT_INDIRECT_TARGET;
        break;
    case ARM64_OP_ADR:
    case ARM64_OP_ADRP:
        effects |= ARM64_EFFECT_WRITE_GPR | ARM64_EFFECT_PC_RELATIVE | ARM64_EFFECT_DIRECT_TARGET;
        break;
    case ARM64_OP_B:
    case ARM64_OP_BL:
        effects |= ARM64_EFFECT_CONTROL_FLOW | ARM64_EFFECT_DIRECT_TARGET | ARM64_EFFECT_PC_RELATIVE;
        if (decoded->opcode == ARM64_OP_BL) effects |= ARM64_EFFECT_CALL | ARM64_EFFECT_WRITE_GPR;
        break;
    case ARM64_OP_BR:
    case ARM64_OP_BLR:
    case ARM64_OP_RET:
        effects |= ARM64_EFFECT_CONTROL_FLOW | ARM64_EFFECT_INDIRECT_TARGET | ARM64_EFFECT_READ_GPR;
        if (decoded->opcode == ARM64_OP_BLR) effects |= ARM64_EFFECT_CALL | ARM64_EFFECT_WRITE_GPR;
        if (decoded->opcode == ARM64_OP_RET) effects |= ARM64_EFFECT_RETURN;
        break;
    case ARM64_OP_B_COND:
    case ARM64_OP_CBZ:
    case ARM64_OP_CBNZ:
    case ARM64_OP_TBZ:
    case ARM64_OP_TBNZ:
        effects |= ARM64_EFFECT_CONTROL_FLOW | ARM64_EFFECT_CONDITIONAL | ARM64_EFFECT_DIRECT_TARGET | ARM64_EFFECT_PC_RELATIVE;
        if (decoded->opcode != ARM64_OP_B_COND) effects |= ARM64_EFFECT_READ_GPR;
        if (decoded->opcode == ARM64_OP_B_COND) effects |= ARM64_EFFECT_READ_FLAGS;
        break;
    case ARM64_OP_ATOMIC_RMW:
    case ARM64_OP_CAS:
    case ARM64_OP_CASP:
        effects |= ARM64_EFFECT_ATOMIC | ARM64_EFFECT_READ_GPR | ARM64_EFFECT_WRITE_GPR | ARM64_EFFECT_READ_MEMORY | ARM64_EFFECT_WRITE_MEMORY;
        break;
    case ARM64_OP_EXCLUSIVE:
        effects |= ARM64_EFFECT_EXCLUSIVE | ARM64_EFFECT_READ_GPR;
        if (decoded->flags & ARM64_INSN_FLAG_LOAD) effects |= ARM64_EFFECT_WRITE_GPR;
        break;
    case ARM64_OP_PREFETCH:
    case ARM64_OP_PREFETCH_LITERAL:
        effects |= ARM64_EFFECT_PREFETCH;
        if (decoded->opcode == ARM64_OP_PREFETCH_LITERAL) effects |= ARM64_EFFECT_PC_RELATIVE | ARM64_EFFECT_DIRECT_TARGET;
        break;
    case ARM64_OP_LOAD_LITERAL:
        effects |= ARM64_EFFECT_PC_RELATIVE | ARM64_EFFECT_DIRECT_TARGET;
        break;
    case ARM64_OP_FP_SIMD:
        effects |= ARM64_EFFECT_READ_FP_SIMD | ARM64_EFFECT_WRITE_FP_SIMD;
        switch (decoded->operands.simd.group)
        {
        case ARM64_SIMD_GROUP_SCALAR_COMPARE:
            effects &= ~ARM64_EFFECT_WRITE_FP_SIMD;
            effects |= ARM64_EFFECT_WRITE_FLAGS;
            break;
        case ARM64_SIMD_GROUP_SCALAR_CONDITIONAL_COMPARE:
            effects &= ~ARM64_EFFECT_WRITE_FP_SIMD;
            effects |= ARM64_EFFECT_READ_FLAGS | ARM64_EFFECT_WRITE_FLAGS | ARM64_EFFECT_CONDITIONAL;
            break;
        case ARM64_SIMD_GROUP_SCALAR_SELECT:
            effects |= ARM64_EFFECT_READ_FLAGS | ARM64_EFFECT_CONDITIONAL;
            break;
        case ARM64_SIMD_GROUP_SCALAR_COPY:
            break;
        case ARM64_SIMD_GROUP_FMOV_GENERAL:
            if (decoded->operands.simd.operation == ARM64_SIMD_OP_FMOV_GENERAL_TO_FP)
            {
                effects &= ~ARM64_EFFECT_READ_FP_SIMD;
                effects |= ARM64_EFFECT_READ_GPR;
            }
            else
            {
                effects &= ~ARM64_EFFECT_WRITE_FP_SIMD;
                effects |= ARM64_EFFECT_WRITE_GPR;
            }
            break;
        case ARM64_SIMD_GROUP_VECTOR_COPY:
            switch (decoded->operands.simd.operation)
            {
            case ARM64_SIMD_OP_DUP_GENERAL:
            case ARM64_SIMD_OP_INS_GENERAL:
                effects |= ARM64_EFFECT_READ_GPR;
                break;
            case ARM64_SIMD_OP_UMOV:
            case ARM64_SIMD_OP_SMOV:
                effects &= ~ARM64_EFFECT_WRITE_FP_SIMD;
                effects |= ARM64_EFFECT_WRITE_GPR;
                break;
            default:
                break;
            }
            break;
        case ARM64_SIMD_GROUP_CONVERT:
            switch (decoded->operands.simd.operation)
            {
            case ARM64_SIMD_OP_SCVTF_S_W:
            case ARM64_SIMD_OP_SCVTF_S_X:
            case ARM64_SIMD_OP_SCVTF_D_W:
            case ARM64_SIMD_OP_SCVTF_D_X:
            case ARM64_SIMD_OP_UCVTF_S_W:
            case ARM64_SIMD_OP_UCVTF_S_X:
            case ARM64_SIMD_OP_UCVTF_D_W:
            case ARM64_SIMD_OP_UCVTF_D_X:
                effects &= ~ARM64_EFFECT_READ_FP_SIMD;
                effects |= ARM64_EFFECT_READ_GPR;
                break;
            case ARM64_SIMD_OP_FCVT_TO_SIGNED:
            case ARM64_SIMD_OP_FCVT_TO_UNSIGNED:
                effects &= ~ARM64_EFFECT_WRITE_FP_SIMD;
                effects |= ARM64_EFFECT_WRITE_GPR;
                break;
            default:
                break;
            }
            break;
        default:
            break;
        }
        break;
    default:
        if (decoded->insn_class == ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE || decoded->insn_class == ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER)
        {
            effects |= ARM64_EFFECT_READ_GPR | ARM64_EFFECT_WRITE_GPR;
            if (decoded->opcode == ARM64_OP_CONDITIONAL_SELECT || decoded->opcode == ARM64_OP_CONDITIONAL_COMPARE) effects |= ARM64_EFFECT_READ_FLAGS | ARM64_EFFECT_CONDITIONAL;
        }
        else if (decoded->insn_class == ARM64_INSN_CLASS_DATA_PROCESSING_SIMD_FP) effects |= ARM64_EFFECT_READ_FP_SIMD | ARM64_EFFECT_WRITE_FP_SIMD;
        break;
    }

    return effects;
}

static void arm64_decode_finish(struct arm64_decoded_insn *decoded, enum arm64_decode_status status)
{
    decoded->status = status;
    decoded->effects = arm64_decode_effects(decoded);
}

struct arm64_decoded_insn arm64_decode_insn(arm64_u32 raw)
{
    struct arm64_decoded_insn decoded;
    enum arm64_decode_status status;
    arm64_u32 op0;

    /* 保证未被当前 opcode 使用的字段有稳定的零值。 */
    __builtin_memset(&decoded, 0, sizeof(decoded));
    decoded.raw = raw;

    if ((raw & 0xFFFF0000U) == 0)
    {
        decoded.insn_class = ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM;
        decoded.opcode = ARM64_OP_EXCEPTION_GENERATION;
        decoded.operands.system.immediate = raw & 0xFFFF;
        status = ARM64_DECODE_UNSUPPORTED;
    }
    else
    {
        /* A64 主编码 raw[28:25] 直接确定唯一子解码器。 */
        op0 = (raw >> 25) & 0xF;
        switch (op0)
        {
        case 0x0:
            status = arm64_decode_sme(raw, &decoded);
            break;
        case 0x2:
            status = arm64_decode_sve(raw, &decoded);
            break;
        case 0x5:
        case 0xD:
            status = arm64_decode_data_processing_register(raw, &decoded);
            break;
        case 0x8:
        case 0x9:
            status = arm64_decode_data_processing_immediate(raw, &decoded);
            break;
        case 0xA:
        case 0xB:
            status = arm64_decode_branch(raw, &decoded);
            break;
        case 0x4:
        case 0x6:
        case 0xC:
        case 0xE:
            status = arm64_decode_ldst(raw, &decoded);
            break;
        case 0x7:
        case 0xF:
            status = arm64_decode_simd(raw, &decoded);
            break;
        default:
            status = ARM64_DECODE_NO_MATCH;
            break;
        }
    }

    if (status == ARM64_DECODE_NO_MATCH)
    {
        /* 没有任何编码空间认领该 word，按架构未分配编码处理。 */
        decoded.insn_class = ARM64_INSN_CLASS_UNKNOWN;
        decoded.opcode = ARM64_OP_UNKNOWN;
        status = ARM64_DECODE_UNALLOCATED;
    }

    arm64_decode_finish(&decoded, status);
    return decoded;
}

int arm64_decode_insn_has_effect(arm64_u32 raw, arm64_u64 effects)
{
    struct arm64_decoded_insn decoded;

    if (!effects) return 0;
    decoded = arm64_decode_insn(raw);
    return (decoded.effects & effects) == effects;
}

int arm64_decode_direct_target(const struct arm64_decoded_insn *decoded, arm64_u64 pc, arm64_u64 *target)
{
    arm64_s64 offset;

    if (!decoded || !target || decoded->status != ARM64_DECODE_OK) return 0;

    switch (decoded->opcode)
    {
    case ARM64_OP_ADR:
        offset = decoded->operands.pc_relative.offset;
        break;
    case ARM64_OP_ADRP:
        *target = (pc & ~0xFFFULL) + decoded->operands.pc_relative.offset;
        return 1;
    case ARM64_OP_B:
    case ARM64_OP_BL:
    case ARM64_OP_B_COND:
    case ARM64_OP_CBZ:
    case ARM64_OP_CBNZ:
    case ARM64_OP_TBZ:
    case ARM64_OP_TBNZ:
        offset = decoded->operands.branch.offset;
        break;
    case ARM64_OP_LOAD_LITERAL:
    case ARM64_OP_PREFETCH_LITERAL:
        offset = decoded->operands.load_store.offset;
        break;
    default:
        return 0;
    }

    *target = pc + offset;
    return 1;
}

static arm64_u64 arm64_decode_extend_index(arm64_u64 value, arm64_u8 extend_type)
{
    arm64_u8 source_width;
    arm64_u64 mask;
    arm64_u64 sign;

    switch (extend_type)
    {
    case 2:
    case 6:
        source_width = 32;
        break;
    case 3:
    case 7:
        source_width = 64;
        break;
    default:
        return value;
    }

    if (source_width == 64) return value;
    mask = (1ULL << source_width) - 1;
    value &= mask;
    if (extend_type < 4) return value;
    sign = 1ULL << (source_width - 1);
    return (value ^ sign) - sign;
}

int arm64_decode_memory_address(const struct arm64_decoded_insn *decoded, arm64_u64 pc, arm64_u64 base, arm64_u64 index_value, struct arm64_memory_address *address)
{
    const struct arm64_load_store_operands *operands;
    arm64_u64 offset;

    if (!decoded || !address || decoded->status != ARM64_DECODE_OK || decoded->insn_class != ARM64_INSN_CLASS_LOAD_STORE) return 0;

    operands = &decoded->operands.load_store;
    __builtin_memset(address, 0, sizeof(*address));
    switch (operands->address_mode)
    {
    case ARM64_ADDRESS_LITERAL:
        address->address = pc + operands->offset;
        break;
    case ARM64_ADDRESS_BASE:
        address->address = base + operands->offset;
        break;
    case ARM64_ADDRESS_UNSIGNED_OFFSET:
    case ARM64_ADDRESS_UNSCALED_OFFSET:
        address->address = base + operands->offset;
        break;
    case ARM64_ADDRESS_PRE_INDEX:
        address->address = base + operands->offset;
        address->writeback_address = address->address;
        address->writeback = 1;
        break;
    case ARM64_ADDRESS_POST_INDEX:
        address->address = base;
        address->writeback_address = base + operands->offset;
        address->writeback = 1;
        break;
    case ARM64_ADDRESS_REGISTER_OFFSET:
        offset = arm64_decode_extend_index(index_value, operands->extend_type);
        address->address = base + (offset << operands->shift_amount);
        break;
    default:
        return 0;
    }

    return 1;
}