#include "arm64_decode.h"

static arm64_s64 arm64_sign_extend(arm64_u64 value, arm64_u8 bits)
{
    arm64_u64 sign = 1ULL << (bits - 1);

    return (arm64_s64)((value ^ sign) - sign);
}

static void arm64_decode_ldst_registers(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    decoded->rt = raw & 0x1F;
    decoded->rn = (raw >> 5) & 0x1F;
    decoded->rt2 = (raw >> 10) & 0x1F;
    decoded->rs = (raw >> 16) & 0x1F;
    decoded->rm = decoded->rs;
}

static void arm64_decode_ldst_fp_flag(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    if (raw & 0x04000000U) decoded->flags |= ARM64_INSN_FLAG_FP;
}

static enum arm64_operation arm64_decode_atomic_operation(arm64_u32 operation)
{
    static const enum arm64_operation operations[] = {
        ARM64_OPERATION_LDADD, ARM64_OPERATION_LDCLR, ARM64_OPERATION_LDEOR, ARM64_OPERATION_LDSET, ARM64_OPERATION_LDSMAX, ARM64_OPERATION_LDSMIN, ARM64_OPERATION_LDUMAX, ARM64_OPERATION_LDUMIN, ARM64_OPERATION_SWP,
    };

    return operation < sizeof(operations) / sizeof(operations[0]) ? operations[operation] : ARM64_OPERATION_NONE;
}

enum arm64_ldst_major
{
    ARM64_LDST_MAJOR_INVALID,
    ARM64_LDST_MAJOR_EXCLUSIVE,
    ARM64_LDST_MAJOR_UNSUPPORTED,
    ARM64_LDST_MAJOR_LITERAL,
    ARM64_LDST_MAJOR_SPECIAL,
    ARM64_LDST_MAJOR_PAIR,
    ARM64_LDST_MAJOR_SINGLE,
};

enum arm64_ldst_single_form
{
    ARM64_LDST_SINGLE_UNALLOCATED,
    ARM64_LDST_SINGLE_UNSIGNED_OFFSET,
    ARM64_LDST_SINGLE_IMMEDIATE,
    ARM64_LDST_SINGLE_REGISTER_OFFSET,
    ARM64_LDST_SINGLE_PAUTH,
    ARM64_LDST_SINGLE_ATOMIC,
};

enum arm64_ldst_atomic_leaf
{
    ARM64_LDST_ATOMIC_UNALLOCATED,
    ARM64_LDST_ATOMIC_RMW,
    ARM64_LDST_ATOMIC_THE,
    ARM64_LDST_ATOMIC_LS64,
    ARM64_LDST_ATOMIC_LDAPR,
};

static const arm64_u8 arm64_ldst_major_table[64] = {
    [0x08] = ARM64_LDST_MAJOR_EXCLUSIVE, [0x09] = ARM64_LDST_MAJOR_UNSUPPORTED, [0x0C] = ARM64_LDST_MAJOR_UNSUPPORTED, [0x0D] = ARM64_LDST_MAJOR_UNSUPPORTED, [0x18] = ARM64_LDST_MAJOR_LITERAL, [0x19] = ARM64_LDST_MAJOR_SPECIAL, [0x1C] = ARM64_LDST_MAJOR_LITERAL, [0x1D] = ARM64_LDST_MAJOR_SPECIAL, [0x28] = ARM64_LDST_MAJOR_PAIR, [0x29] = ARM64_LDST_MAJOR_PAIR, [0x2C] = ARM64_LDST_MAJOR_PAIR, [0x2D] = ARM64_LDST_MAJOR_PAIR, [0x38] = ARM64_LDST_MAJOR_SINGLE, [0x39] = ARM64_LDST_MAJOR_SINGLE, [0x3C] = ARM64_LDST_MAJOR_SINGLE, [0x3D] = ARM64_LDST_MAJOR_SINGLE,
};

static enum arm64_decode_status arm64_decode_ldst_exclusive(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    switch (raw & 0x00A07C00U)
    {
    case 0x00A07C00U:
        decoded->opcode = ARM64_OP_CAS;
        decoded->flags = ARM64_INSN_FLAG_LOAD | ARM64_INSN_FLAG_STORE;
        if (raw & 0x00400000U) decoded->flags |= ARM64_INSN_FLAG_ACQUIRE;
        if (raw & 0x00008000U) decoded->flags |= ARM64_INSN_FLAG_RELEASE;
        arm64_decode_ldst_registers(raw, decoded);
        decoded->operands.load_store.access_bytes = 1U << ((raw >> 30) & 0x3);
        decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
        decoded->operand_width = decoded->operands.load_store.access_bytes == 8 ? 64 : 32;
        return ARM64_DECODE_OK;
    case 0x00207C00U:
    {
        arm64_u32 size = (raw >> 30) & 0x3;
        arm64_u32 op = (raw >> 21) & 0xF;
        arm64_u32 rs = (raw >> 16) & 0x1F;
        arm64_u32 rt = raw & 0x1F;

        if (size >= 2 || ((raw >> 10) & 0x1F) != 31 || (op != 1 && op != 3) || ((rs | rt) & 1)) return ARM64_DECODE_UNALLOCATED;
        decoded->opcode = ARM64_OP_CASP;
        decoded->flags = ARM64_INSN_FLAG_LOAD | ARM64_INSN_FLAG_STORE | ARM64_INSN_FLAG_PAIR;
        if (op & 2) decoded->flags |= ARM64_INSN_FLAG_ACQUIRE;
        if (raw & 0x00008000U) decoded->flags |= ARM64_INSN_FLAG_RELEASE;
        arm64_decode_ldst_registers(raw, decoded);
        decoded->operands.load_store.access_bytes = size == 0 ? 4 : 8;
        decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
        decoded->operand_width = decoded->operands.load_store.access_bytes * 8;
        return ARM64_DECODE_OK;
    }
    default:
        break;
    }

    {
        arm64_u32 size = (raw >> 30) & 0x3;
        arm64_u32 ordered = (raw >> 23) & 1;
        arm64_u32 load = (raw >> 22) & 1;
        arm64_u32 pair = (raw >> 21) & 1;
        arm64_u32 acquire_release = (raw >> 15) & 1;
        arm64_u32 rs = (raw >> 16) & 0x1F;
        arm64_u32 rt2 = (raw >> 10) & 0x1F;

        if (ordered)
        {
            if (pair || rs != 31 || rt2 != 31) return ARM64_DECODE_UNALLOCATED;
        }
        else
        {
            if ((pair && size < 2) || (!pair && rt2 != 31) || (load && rs != 31)) return ARM64_DECODE_UNALLOCATED;
        }
        decoded->opcode = ARM64_OP_EXCLUSIVE;
        decoded->flags = 0;
        if (load) decoded->flags |= ARM64_INSN_FLAG_LOAD;
        else decoded->flags |= ARM64_INSN_FLAG_STORE;
        if (pair) decoded->flags |= ARM64_INSN_FLAG_PAIR;
        if (ordered) decoded->flags |= ARM64_INSN_FLAG_ORDERED;
        if (acquire_release && load) decoded->flags |= ARM64_INSN_FLAG_ACQUIRE;
        if (acquire_release && !load) decoded->flags |= ARM64_INSN_FLAG_RELEASE;
        arm64_decode_ldst_registers(raw, decoded);
        decoded->operands.load_store.access_bytes = 1U << size;
        decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
        decoded->operand_width = decoded->operands.load_store.access_bytes == 8 ? 64 : 32;
        if (pair && load && decoded->rt == decoded->rt2) return ARM64_DECODE_UNPREDICTABLE;
        if (!load && rs != 31 && (rs == decoded->rt || (pair && rs == decoded->rt2) || (decoded->rn != 31 && rs == decoded->rn))) return ARM64_DECODE_UNPREDICTABLE;
        return ARM64_DECODE_OK;
    }
}

static enum arm64_decode_status arm64_decode_ldst_literal(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 size = (raw >> 30) & 0x3;

    decoded->opcode = !(raw & 0x04000000U) && size == 3 ? ARM64_OP_PREFETCH_LITERAL : ARM64_OP_LOAD_LITERAL;
    decoded->flags = decoded->opcode == ARM64_OP_LOAD_LITERAL ? ARM64_INSN_FLAG_LOAD : 0;
    arm64_decode_ldst_registers(raw, decoded);
    arm64_decode_ldst_fp_flag(raw, decoded);
    if (decoded->flags & ARM64_INSN_FLAG_FP) decoded->operands.load_store.access_bytes = size == 0 ? 4 : size == 1 ? 8 : size == 2 ? 16 : 0;
    else decoded->operands.load_store.access_bytes = size == 0 ? 4 : size == 1 ? 8 : size == 2 ? 4 : 0;
    decoded->operands.load_store.address_mode = ARM64_ADDRESS_LITERAL;
    decoded->operands.load_store.offset = arm64_sign_extend((arm64_u64)((raw >> 5) & 0x7FFFF) << 2, 21);
    if (!(decoded->flags & ARM64_INSN_FLAG_FP) && size == 2) decoded->flags |= ARM64_INSN_FLAG_SIGN_EXTEND;
    decoded->operand_width = size == 0 ? 32 : 64;
    if ((decoded->flags & ARM64_INSN_FLAG_FP) && !decoded->operands.load_store.access_bytes) return ARM64_DECODE_UNALLOCATED;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_special(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    decoded->opcode = ARM64_OP_UNKNOWN;
    switch (raw & 0xBF20FC00U)
    {
    case 0x19200800U:
        return ARM64_DECODE_UNSUPPORTED;
    case 0x19200C00U:
        if ((((raw >> 16) & 0x1F) | (raw & 0x1F)) & 1) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_UNSUPPORTED;
    case 0x19209000U:
    case 0x1920A000U:
    case 0x1920B000U:
    {
        arm64_u32 rt = raw & 0x1F;
        arm64_u32 rt2 = (raw >> 16) & 0x1F;

        if (rt == 31 || rt2 == 31) return ARM64_DECODE_UNALLOCATED;
        if (rt == rt2) return ARM64_DECODE_UNPREDICTABLE;
        return ARM64_DECODE_UNSUPPORTED;
    }
    default:
        break;
    }

    switch (raw & 0x3F200C00U)
    {
    case 0x19000000U:
    {
        arm64_u32 size = (raw >> 30) & 0x3;
        arm64_u32 opc = (raw >> 22) & 0x3;

        if ((size == 3 && opc > 1) || (size == 2 && opc == 3)) return ARM64_DECODE_UNALLOCATED;
        decoded->opcode = ARM64_OP_RCPC_UNSCALED;
        arm64_decode_ldst_registers(raw, decoded);
        decoded->operands.load_store.access_bytes = 1U << size;
        decoded->operands.load_store.address_mode = ARM64_ADDRESS_UNSCALED_OFFSET;
        decoded->operands.load_store.offset = arm64_sign_extend((raw >> 12) & 0x1FF, 9);
        if (opc) decoded->flags |= ARM64_INSN_FLAG_LOAD;
        else decoded->flags |= ARM64_INSN_FLAG_STORE;
        if (opc >= 2) decoded->flags |= ARM64_INSN_FLAG_SIGN_EXTEND;
        if (decoded->flags & ARM64_INSN_FLAG_LOAD) decoded->flags |= ARM64_INSN_FLAG_ACQUIRE;
        else decoded->flags |= ARM64_INSN_FLAG_RELEASE;
        decoded->operand_width = size == 3 || opc == 2 ? 64 : 32;
        return ARM64_DECODE_OK;
    }
    default:
        return ARM64_DECODE_UNSUPPORTED;
    }
}

static enum arm64_decode_status arm64_decode_ldst_pair(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    arm64_u32 mode = (raw >> 23) & 0x3;
    arm64_u32 opc = (raw >> 30) & 0x3;
    arm64_u32 load = (raw >> 22) & 1;

    decoded->opcode = ARM64_OP_LOAD_STORE_PAIR;
    decoded->flags = ARM64_INSN_FLAG_PAIR;
    arm64_decode_ldst_registers(raw, decoded);
    arm64_decode_ldst_fp_flag(raw, decoded);
    if (load) decoded->flags |= ARM64_INSN_FLAG_LOAD;
    else decoded->flags |= ARM64_INSN_FLAG_STORE;
    if (decoded->flags & ARM64_INSN_FLAG_FP)
    {
        if (opc == 3) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.load_store.access_bytes = 4U << opc;
        decoded->operand_width = decoded->operands.load_store.access_bytes * 8;
    }
    else
    {
        if (opc == 3 || (opc == 1 && mode == 0)) return ARM64_DECODE_UNALLOCATED;
        if (opc == 1 && !load) return ARM64_DECODE_UNSUPPORTED;
        decoded->operands.load_store.access_bytes = opc == 2 ? 8 : 4;
        decoded->operand_width = opc == 0 ? 32 : 64;
        if (opc == 1) decoded->flags |= ARM64_INSN_FLAG_SIGN_EXTEND;
    }
    if (!(decoded->flags & ARM64_INSN_FLAG_FP) && (mode & 1) && decoded->rn != 31 && (decoded->rn == decoded->rt || decoded->rn == decoded->rt2)) return ARM64_DECODE_UNPREDICTABLE;
    if (load && decoded->rt == decoded->rt2) return ARM64_DECODE_UNPREDICTABLE;
    decoded->operands.load_store.offset = arm64_sign_extend((raw >> 15) & 0x7F, 7) * decoded->operands.load_store.access_bytes;
    decoded->operands.load_store.address_mode = mode == 1 ? ARM64_ADDRESS_POST_INDEX : mode == 3 ? ARM64_ADDRESS_PRE_INDEX : ARM64_ADDRESS_BASE;
    if (mode == 0) decoded->flags |= ARM64_INSN_FLAG_NON_TEMPORAL;
    if (mode == 1 || mode == 3) decoded->flags |= ARM64_INSN_FLAG_WRITEBACK;
    return ARM64_DECODE_OK;
}

static enum arm64_ldst_single_form arm64_classify_ldst_single(arm64_u32 raw)
{
    arm64_u32 mode;

    if (raw & 0x01000000U) return ARM64_LDST_SINGLE_UNSIGNED_OFFSET;
    if (!(raw & 0x00200000U)) return ARM64_LDST_SINGLE_IMMEDIATE;

    mode = (raw >> 10) & 0x3;
    if (mode == 2) return ARM64_LDST_SINGLE_REGISTER_OFFSET;
    if (raw & 0x04000000U) return ARM64_LDST_SINGLE_UNALLOCATED;
    if (mode == 0) return ARM64_LDST_SINGLE_ATOMIC;
    if (((raw >> 30) & 0x3) == 3) return ARM64_LDST_SINGLE_PAUTH;
    return ARM64_LDST_SINGLE_UNALLOCATED;
}

static enum arm64_ldst_atomic_leaf arm64_classify_ldst_atomic(arm64_u32 raw)
{
    arm64_u32 operation = (raw >> 12) & 0xF;

    if (operation <= 8) return ARM64_LDST_ATOMIC_RMW;
    switch (operation)
    {
    case 9:
        if ((raw & 0xBF20FC00U) == 0x38209000U) return ARM64_LDST_ATOMIC_THE;
        if ((raw & 0xFFFFFC00U) == 0xF83F9000U) return ARM64_LDST_ATOMIC_LS64;
        break;
    case 10:
        if ((raw & 0xBF20FC00U) == 0x3820A000U) return ARM64_LDST_ATOMIC_THE;
        if ((raw & 0xFFE0FC00U) == 0xF820A000U) return ARM64_LDST_ATOMIC_LS64;
        break;
    case 11:
        if ((raw & 0xBF20FC00U) == 0x3820B000U) return ARM64_LDST_ATOMIC_THE;
        if ((raw & 0xFFE0FC00U) == 0xF820B000U) return ARM64_LDST_ATOMIC_LS64;
        break;
    case 12:
        if ((raw & 0x3FFFFC00U) == 0x38BFC000U) return ARM64_LDST_ATOMIC_LDAPR;
        break;
    case 13:
        if ((raw & 0xFFFFFC00U) == 0xF83FD000U) return ARM64_LDST_ATOMIC_LS64;
        break;
    default:
        break;
    }
    return ARM64_LDST_ATOMIC_UNALLOCATED;
}

static enum arm64_decode_status arm64_decode_ldst_atomic(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    switch (arm64_classify_ldst_atomic(raw))
    {
    case ARM64_LDST_ATOMIC_RMW:
        decoded->opcode = ARM64_OP_ATOMIC_RMW;
        decoded->operation = arm64_decode_atomic_operation((raw >> 12) & 0xF);
        decoded->flags = ARM64_INSN_FLAG_LOAD | ARM64_INSN_FLAG_STORE;
        if (raw & 0x00800000U) decoded->flags |= ARM64_INSN_FLAG_ACQUIRE;
        if (raw & 0x00400000U) decoded->flags |= ARM64_INSN_FLAG_RELEASE;
        arm64_decode_ldst_registers(raw, decoded);
        decoded->operands.load_store.access_bytes = 1U << ((raw >> 30) & 0x3);
        decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
        decoded->operand_width = decoded->operands.load_store.access_bytes == 8 ? 64 : 32;
        return ARM64_DECODE_OK;
    case ARM64_LDST_ATOMIC_THE:
        decoded->opcode = ARM64_OP_UNKNOWN;
        return ARM64_DECODE_UNSUPPORTED;
    case ARM64_LDST_ATOMIC_LS64:
        if ((raw & 0x1F) >= 24 || (raw & 1)) return ARM64_DECODE_UNALLOCATED;
        decoded->opcode = ARM64_OP_UNKNOWN;
        arm64_decode_ldst_registers(raw, decoded);
        return ARM64_DECODE_UNSUPPORTED;
    case ARM64_LDST_ATOMIC_LDAPR:
        decoded->opcode = ARM64_OP_LDAPR;
        decoded->flags = ARM64_INSN_FLAG_LOAD | ARM64_INSN_FLAG_ACQUIRE;
        arm64_decode_ldst_registers(raw, decoded);
        decoded->operands.load_store.access_bytes = 1U << ((raw >> 30) & 0x3);
        decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
        decoded->operand_width = decoded->operands.load_store.access_bytes == 8 ? 64 : 32;
        return ARM64_DECODE_OK;
    default:
        return ARM64_DECODE_UNALLOCATED;
    }
}

static enum arm64_decode_status arm64_decode_ldst_single_regular(arm64_u32 raw, enum arm64_ldst_single_form form, struct arm64_decoded_insn *decoded)
{
    arm64_u32 size = (raw >> 30) & 0x3;
    arm64_u32 mode = (raw >> 10) & 0x3;
    arm64_u32 opc = (raw >> 22) & 0x3;

    decoded->opcode = !(raw & 0x04000000U) && size == 3 && opc == 2 ? ARM64_OP_PREFETCH : ARM64_OP_LOAD_STORE_SINGLE;
    arm64_decode_ldst_registers(raw, decoded);
    arm64_decode_ldst_fp_flag(raw, decoded);
    if ((decoded->flags & ARM64_INSN_FLAG_FP) && opc >= 2 && size != 0) return ARM64_DECODE_UNALLOCATED;
    if (decoded->flags & ARM64_INSN_FLAG_FP) decoded->operands.load_store.access_bytes = size == 0 && (opc & 2) ? 16 : 1U << size;
    else decoded->operands.load_store.access_bytes = 1U << size;
    if ((decoded->flags & ARM64_INSN_FLAG_FP) ? (opc & 1) : opc) decoded->flags |= ARM64_INSN_FLAG_LOAD;
    else decoded->flags |= ARM64_INSN_FLAG_STORE;
    if (!(decoded->flags & ARM64_INSN_FLAG_FP) && opc >= 2) decoded->flags |= ARM64_INSN_FLAG_SIGN_EXTEND;
    if (decoded->opcode == ARM64_OP_PREFETCH) decoded->flags &= ~(ARM64_INSN_FLAG_LOAD | ARM64_INSN_FLAG_STORE | ARM64_INSN_FLAG_SIGN_EXTEND);
    decoded->operand_width = (decoded->flags & ARM64_INSN_FLAG_FP) ? decoded->operands.load_store.access_bytes * 8 : (size == 3 || opc == 2 ? 64 : 32);

    if (!(decoded->flags & ARM64_INSN_FLAG_FP) && ((size == 3 && opc == 3) || (size == 2 && opc == 3))) return ARM64_DECODE_UNALLOCATED;
    if ((decoded->flags & ARM64_INSN_FLAG_FP) && form == ARM64_LDST_SINGLE_IMMEDIATE && mode == 2) return ARM64_DECODE_UNALLOCATED;
    if (decoded->opcode == ARM64_OP_PREFETCH && form == ARM64_LDST_SINGLE_IMMEDIATE && mode != 0) return ARM64_DECODE_UNALLOCATED;

    switch (form)
    {
    case ARM64_LDST_SINGLE_UNSIGNED_OFFSET:
        decoded->operands.load_store.address_mode = ARM64_ADDRESS_UNSIGNED_OFFSET;
        decoded->operands.load_store.offset = ((raw >> 10) & 0xFFF) * decoded->operands.load_store.access_bytes;
        break;
    case ARM64_LDST_SINGLE_REGISTER_OFFSET:
        decoded->operands.load_store.address_mode = ARM64_ADDRESS_REGISTER_OFFSET;
        decoded->operands.load_store.extend_type = (raw >> 13) & 0x7;
        if (decoded->operands.load_store.extend_type != 2 && decoded->operands.load_store.extend_type != 3 && decoded->operands.load_store.extend_type != 6 && decoded->operands.load_store.extend_type != 7) return ARM64_DECODE_UNALLOCATED;
        decoded->operands.load_store.shift_amount = (raw & 0x1000U) ? (arm64_u8)__builtin_ctz(decoded->operands.load_store.access_bytes) : 0;
        break;
    default:
        decoded->operands.load_store.offset = arm64_sign_extend((raw >> 12) & 0x1FF, 9);
        decoded->operands.load_store.address_mode = mode == 1 ? ARM64_ADDRESS_POST_INDEX : mode == 3 ? ARM64_ADDRESS_PRE_INDEX : ARM64_ADDRESS_UNSCALED_OFFSET;
        if (mode == 2) decoded->flags |= ARM64_INSN_FLAG_UNPRIVILEGED;
        if (mode == 1 || mode == 3) decoded->flags |= ARM64_INSN_FLAG_WRITEBACK;
        break;
    }
    if (!(decoded->flags & ARM64_INSN_FLAG_FP) && (decoded->flags & ARM64_INSN_FLAG_WRITEBACK) && decoded->rn != 31 && decoded->rn == decoded->rt) return ARM64_DECODE_UNPREDICTABLE;
    return ARM64_DECODE_OK;
}

static enum arm64_decode_status arm64_decode_ldst_single(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    enum arm64_ldst_single_form form = arm64_classify_ldst_single(raw);

    switch (form)
    {
    case ARM64_LDST_SINGLE_UNSIGNED_OFFSET:
    case ARM64_LDST_SINGLE_IMMEDIATE:
    case ARM64_LDST_SINGLE_REGISTER_OFFSET:
        return arm64_decode_ldst_single_regular(raw, form, decoded);
    case ARM64_LDST_SINGLE_PAUTH:
        decoded->opcode = ARM64_OP_UNKNOWN;
        arm64_decode_ldst_registers(raw, decoded);
        return ARM64_DECODE_UNSUPPORTED;
    case ARM64_LDST_SINGLE_ATOMIC:
        return arm64_decode_ldst_atomic(raw, decoded);
    default:
        return ARM64_DECODE_UNALLOCATED;
    }
}

/*
解码访存、原子和独占指令。bits[29:24] 先确定唯一编码 owner，叶子只校验
本族固定字段和寄存器约束，不依赖宽窄掩码的排列顺序。
*/
enum arm64_decode_status arm64_decode_ldst(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    enum arm64_ldst_major major = (enum arm64_ldst_major)arm64_ldst_major_table[(raw >> 24) & 0x3F];

    if (major == ARM64_LDST_MAJOR_INVALID) return ARM64_DECODE_NO_MATCH;

    decoded->insn_class = ARM64_INSN_CLASS_LOAD_STORE;
    switch (major)
    {
    case ARM64_LDST_MAJOR_EXCLUSIVE:
        return arm64_decode_ldst_exclusive(raw, decoded);
    case ARM64_LDST_MAJOR_LITERAL:
        return arm64_decode_ldst_literal(raw, decoded);
    case ARM64_LDST_MAJOR_SPECIAL:
        return arm64_decode_ldst_special(raw, decoded);
    case ARM64_LDST_MAJOR_PAIR:
        return arm64_decode_ldst_pair(raw, decoded);
    case ARM64_LDST_MAJOR_SINGLE:
        return arm64_decode_ldst_single(raw, decoded);
    case ARM64_LDST_MAJOR_UNSUPPORTED:
        decoded->opcode = ARM64_OP_UNKNOWN;
        return ARM64_DECODE_UNSUPPORTED;
    default:
        return ARM64_DECODE_NO_MATCH;
    }
}