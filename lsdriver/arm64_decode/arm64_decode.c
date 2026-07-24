#include "arm64_decode.h"

enum arm64_decode_status arm64_decode_data_processing_immediate(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_data_processing_register(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_ldst(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_branch(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_simd(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_sve(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_sme(arm64_u32 raw, struct arm64_decoded_insn *decoded);

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

    decoded.status = status;
    return decoded;
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