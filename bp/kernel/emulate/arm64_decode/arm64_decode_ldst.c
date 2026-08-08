#include "arm64_decode_internal.h"

static void arm64_decode_ldst_rt_rn(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    decoded->rt = raw & 0x1F;
    decoded->rn = (raw >> 5) & 0x1F;
}

static void arm64_decode_ldst_register_kind(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    decoded->operands.load_store.register_kind = (raw & 0x04000000U) ? ARM64_MEMORY_REGISTER_FP_SIMD : ARM64_MEMORY_REGISTER_GPR;
}

static enum arm64_memory_order arm64_decode_memory_order(uint32_t acquire, uint32_t release)
{
    if (acquire) return release ? ARM64_MEMORY_ORDER_ACQUIRE_RELEASE : ARM64_MEMORY_ORDER_ACQUIRE;
    return release ? ARM64_MEMORY_ORDER_RELEASE : ARM64_MEMORY_ORDER_NONE;
}

enum arm64_ldst_single_form
{
    ARM64_LDST_SINGLE_UNSIGNED_OFFSET,
    ARM64_LDST_SINGLE_IMMEDIATE,
    ARM64_LDST_SINGLE_REGISTER_OFFSET,
    ARM64_LDST_SINGLE_PAUTH,
    ARM64_LDST_SINGLE_ATOMIC,
};

/*
解码访存、原子和独占指令。bits[29:24] 先确定唯一编码 owner，叶子只校验
本族固定字段和寄存器约束，不依赖宽窄掩码的排列顺序。
*/
enum arm64_decode_status arm64_decode_ldst(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    switch ((raw >> 24) & 0x3F)
    {
    case 0x08:
        decoded->insn_class = ARM64_INSN_CLASS_LOAD_STORE;
        switch (raw & 0x00A07C00U)
        {
        case 0x00A07C00U:
            decoded->opcode = ARM64_OP_CAS;
            decoded->operands.load_store.access = ARM64_MEMORY_ACCESS_READ_WRITE;
            decoded->operands.load_store.register_kind = ARM64_MEMORY_REGISTER_GPR;
            decoded->operands.load_store.memory_order = arm64_decode_memory_order(raw & 0x00400000U, raw & 0x00008000U);
            decoded->operands.load_store.extension = ARM64_MEMORY_EXTENSION_ZERO;
            decoded->operands.load_store.transfer = ARM64_MEMORY_TRANSFER_SINGLE;
            arm64_decode_ldst_rt_rn(raw, decoded);
            decoded->rs = (raw >> 16) & 0x1F;
            decoded->operands.load_store.access_bytes = 1U << ((raw >> 30) & 0x3);
            decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
            decoded->operand_width = decoded->operands.load_store.access_bytes == 8 ? 64 : 32;
            return ARM64_DECODE_OK;
        case 0x00207C00U:
        {
            uint32_t size = (raw >> 30) & 0x3;
            uint32_t op = (raw >> 21) & 0xF;
            uint32_t rs = (raw >> 16) & 0x1F;
            uint32_t rt = raw & 0x1F;

            if (size >= 2 || ((raw >> 10) & 0x1F) != 31 || (op != 1 && op != 3) || ((rs | rt) & 1)) return ARM64_DECODE_UNALLOCATED;
            decoded->opcode = ARM64_OP_CASP;
            decoded->operands.load_store.access = ARM64_MEMORY_ACCESS_READ_WRITE;
            decoded->operands.load_store.register_kind = ARM64_MEMORY_REGISTER_GPR;
            decoded->operands.load_store.memory_order = arm64_decode_memory_order(op & 2, raw & 0x00008000U);
            decoded->operands.load_store.extension = ARM64_MEMORY_EXTENSION_ZERO;
            decoded->operands.load_store.transfer = ARM64_MEMORY_TRANSFER_PAIR;
            arm64_decode_ldst_rt_rn(raw, decoded);
            decoded->rs = (raw >> 16) & 0x1F;
            decoded->operands.load_store.access_bytes = size == 0 ? 4 : 8;
            decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
            decoded->operand_width = decoded->operands.load_store.access_bytes * 8;
            return ARM64_DECODE_OK;
        }
        default:
            break;
        }

        {
            uint32_t size = (raw >> 30) & 0x3;
            uint32_t ordered = (raw >> 23) & 1;
            uint32_t load = (raw >> 22) & 1;
            uint32_t pair = (raw >> 21) & 1;
            uint32_t acquire_release = (raw >> 15) & 1;
            uint32_t rs = (raw >> 16) & 0x1F;
            uint32_t rt2 = (raw >> 10) & 0x1F;

            if (ordered)
            {
                if (pair || rs != 31 || rt2 != 31) return ARM64_DECODE_UNALLOCATED;
            }
            else if ((pair && size < 2) || (!pair && rt2 != 31) || (load && rs != 31))
            {
                return ARM64_DECODE_UNALLOCATED;
            }

            decoded->opcode = ordered ? ARM64_OP_ORDERED : ARM64_OP_EXCLUSIVE;
            decoded->operands.load_store.access = load ? ARM64_MEMORY_ACCESS_LOAD : ARM64_MEMORY_ACCESS_STORE;
            decoded->operands.load_store.register_kind = ARM64_MEMORY_REGISTER_GPR;
            if (ordered && !acquire_release) decoded->operands.load_store.memory_order = load ? ARM64_MEMORY_ORDER_LO_ACQUIRE : ARM64_MEMORY_ORDER_LO_RELEASE;
            else decoded->operands.load_store.memory_order = arm64_decode_memory_order(acquire_release && load, acquire_release && !load);
            if (load) decoded->operands.load_store.extension = ARM64_MEMORY_EXTENSION_ZERO;
            decoded->operands.load_store.transfer = pair ? ARM64_MEMORY_TRANSFER_PAIR : ARM64_MEMORY_TRANSFER_SINGLE;
            arm64_decode_ldst_rt_rn(raw, decoded);
            if (pair) decoded->rt2 = rt2;
            if (!ordered && !load) decoded->rs = rs;
            decoded->operands.load_store.access_bytes = 1U << size;
            decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
            decoded->operand_width = decoded->operands.load_store.access_bytes == 8 ? 64 : 32;
            if (pair && load && decoded->rt == decoded->rt2) return ARM64_DECODE_UNPREDICTABLE;
            if (!load && rs != 31 && (rs == decoded->rt || (pair && rs == decoded->rt2) || (decoded->rn != 31 && rs == decoded->rn))) return ARM64_DECODE_UNPREDICTABLE;
            return ARM64_DECODE_OK;
        }

    case 0x18:
    case 0x1C:
    {
        uint32_t size = (raw >> 30) & 0x3;

        decoded->insn_class = ARM64_INSN_CLASS_LOAD_STORE;
        decoded->opcode = !(raw & 0x04000000U) && size == 3 ? ARM64_OP_PREFETCH_LITERAL : ARM64_OP_LOAD_LITERAL;
        if (decoded->opcode == ARM64_OP_LOAD_LITERAL)
        {
            decoded->rt = raw & 0x1F;
            decoded->operands.load_store.access = ARM64_MEMORY_ACCESS_LOAD;
            decoded->operands.load_store.transfer = ARM64_MEMORY_TRANSFER_SINGLE;
            arm64_decode_ldst_register_kind(raw, decoded);
        }
        else decoded->operands.load_store.prefetch_operation = raw & 0x1F;
        if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_FP_SIMD) decoded->operands.load_store.access_bytes = size == 0 ? 4 : size == 1 ? 8 : size == 2 ? 16 : 0;
        else decoded->operands.load_store.access_bytes = size == 0 ? 4 : size == 1 ? 8 : size == 2 ? 4 : 0;
        decoded->operands.load_store.address_mode = ARM64_ADDRESS_LITERAL;
        decoded->operands.load_store.offset = arm64_sign_extend((uint64_t)((raw >> 5) & 0x7FFFF) << 2, 21);
        if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_GPR) decoded->operands.load_store.extension = size == 2 ? ARM64_MEMORY_EXTENSION_SIGN : ARM64_MEMORY_EXTENSION_ZERO;
        decoded->operand_width = decoded->opcode == ARM64_OP_PREFETCH_LITERAL ? 0 : decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_FP_SIMD ? decoded->operands.load_store.access_bytes * 8 : (size == 0 ? 32 : 64);
        if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_FP_SIMD && !decoded->operands.load_store.access_bytes) return ARM64_DECODE_UNALLOCATED;
        return ARM64_DECODE_OK;
    }

    case 0x19:
    case 0x1D:
        decoded->insn_class = ARM64_INSN_CLASS_LOAD_STORE;
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
            uint32_t rt = raw & 0x1F;
            uint32_t rt2 = (raw >> 16) & 0x1F;

            if (rt == 31 || rt2 == 31) return ARM64_DECODE_UNALLOCATED;
            if (rt == rt2) return ARM64_DECODE_UNPREDICTABLE;
            return ARM64_DECODE_UNSUPPORTED;
        }
        default:
            break;
        }

        if ((raw & 0x3F200C00U) == 0x19000000U)
        {
            uint32_t size = (raw >> 30) & 0x3;
            uint32_t opc = (raw >> 22) & 0x3;

            if ((size == 3 && opc > 1) || (size == 2 && opc == 3)) return ARM64_DECODE_UNALLOCATED;
            decoded->opcode = ARM64_OP_RCPC_UNSCALED;
            arm64_decode_ldst_rt_rn(raw, decoded);
            decoded->operands.load_store.access = opc ? ARM64_MEMORY_ACCESS_LOAD : ARM64_MEMORY_ACCESS_STORE;
            decoded->operands.load_store.register_kind = ARM64_MEMORY_REGISTER_GPR;
            decoded->operands.load_store.memory_order = opc ? ARM64_MEMORY_ORDER_ACQUIRE : ARM64_MEMORY_ORDER_RELEASE;
            if (decoded->operands.load_store.access == ARM64_MEMORY_ACCESS_LOAD) decoded->operands.load_store.extension = opc >= 2 ? ARM64_MEMORY_EXTENSION_SIGN : ARM64_MEMORY_EXTENSION_ZERO;
            decoded->operands.load_store.transfer = ARM64_MEMORY_TRANSFER_SINGLE;
            decoded->operands.load_store.access_bytes = 1U << size;
            decoded->operands.load_store.address_mode = ARM64_ADDRESS_UNSCALED_OFFSET;
            decoded->operands.load_store.offset = arm64_sign_extend((raw >> 12) & 0x1FF, 9);
            decoded->operand_width = size == 3 || opc == 2 ? 64 : 32;
            return ARM64_DECODE_OK;
        }
        return ARM64_DECODE_UNSUPPORTED;

    case 0x28:
    case 0x29:
    case 0x2C:
    case 0x2D:
    {
        uint32_t mode = (raw >> 23) & 0x3;
        uint32_t opc = (raw >> 30) & 0x3;
        uint32_t load = (raw >> 22) & 1;

        decoded->insn_class = ARM64_INSN_CLASS_LOAD_STORE;
        decoded->opcode = ARM64_OP_LOAD_STORE_PAIR;
        arm64_decode_ldst_rt_rn(raw, decoded);
        decoded->rt2 = (raw >> 10) & 0x1F;
        arm64_decode_ldst_register_kind(raw, decoded);
        decoded->operands.load_store.access = load ? ARM64_MEMORY_ACCESS_LOAD : ARM64_MEMORY_ACCESS_STORE;
        decoded->operands.load_store.transfer = ARM64_MEMORY_TRANSFER_PAIR;
        if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_FP_SIMD)
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
            if (load) decoded->operands.load_store.extension = opc == 1 ? ARM64_MEMORY_EXTENSION_SIGN : ARM64_MEMORY_EXTENSION_ZERO;
        }
        if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_GPR && (mode & 1) && decoded->rn != 31 && (decoded->rn == decoded->rt || decoded->rn == decoded->rt2)) return ARM64_DECODE_UNPREDICTABLE;
        if (load && decoded->rt == decoded->rt2) return ARM64_DECODE_UNPREDICTABLE;
        decoded->operands.load_store.offset = arm64_sign_extend((raw >> 15) & 0x7F, 7) * decoded->operands.load_store.access_bytes;
        decoded->operands.load_store.address_mode = mode == 0 ? ARM64_ADDRESS_NON_TEMPORAL_OFFSET : mode == 1 ? ARM64_ADDRESS_POST_INDEX : mode == 3 ? ARM64_ADDRESS_PRE_INDEX : ARM64_ADDRESS_BASE;
        return ARM64_DECODE_OK;
    }

    case 0x38:
    case 0x39:
    case 0x3C:
    case 0x3D:
    {
        enum arm64_ldst_single_form form;

        decoded->insn_class = ARM64_INSN_CLASS_LOAD_STORE;
        if (raw & 0x01000000U) form = ARM64_LDST_SINGLE_UNSIGNED_OFFSET;
        else if (!(raw & 0x00200000U)) form = ARM64_LDST_SINGLE_IMMEDIATE;
        else
        {
            uint32_t mode = (raw >> 10) & 0x3;

            if (mode == 2) form = ARM64_LDST_SINGLE_REGISTER_OFFSET;
            else if (raw & 0x04000000U) return ARM64_DECODE_UNALLOCATED;
            else if (mode == 0) form = ARM64_LDST_SINGLE_ATOMIC;
            else if (((raw >> 30) & 0x3) == 3) form = ARM64_LDST_SINGLE_PAUTH;
            else return ARM64_DECODE_UNALLOCATED;
        }

        if (form == ARM64_LDST_SINGLE_PAUTH)
        {
            decoded->opcode = ARM64_OP_UNKNOWN;
            return ARM64_DECODE_UNSUPPORTED;
        }

        if (form == ARM64_LDST_SINGLE_ATOMIC)
        {
            uint32_t operation = (raw >> 12) & 0xF;

            if (operation <= 8)
            {
                decoded->opcode = ARM64_OP_ATOMIC_RMW;
                switch (operation)
                {
                case 0:
                    decoded->operation = ARM64_OPERATION_LDADD;
                    break;
                case 1:
                    decoded->operation = ARM64_OPERATION_LDCLR;
                    break;
                case 2:
                    decoded->operation = ARM64_OPERATION_LDEOR;
                    break;
                case 3:
                    decoded->operation = ARM64_OPERATION_LDSET;
                    break;
                case 4:
                    decoded->operation = ARM64_OPERATION_LDSMAX;
                    break;
                case 5:
                    decoded->operation = ARM64_OPERATION_LDSMIN;
                    break;
                case 6:
                    decoded->operation = ARM64_OPERATION_LDUMAX;
                    break;
                case 7:
                    decoded->operation = ARM64_OPERATION_LDUMIN;
                    break;
                default:
                    decoded->operation = ARM64_OPERATION_SWP;
                    break;
                }
                decoded->operands.load_store.access = ARM64_MEMORY_ACCESS_READ_WRITE;
                decoded->operands.load_store.register_kind = ARM64_MEMORY_REGISTER_GPR;
                decoded->operands.load_store.memory_order = arm64_decode_memory_order(raw & 0x00800000U, raw & 0x00400000U);
                decoded->operands.load_store.extension = ARM64_MEMORY_EXTENSION_ZERO;
                decoded->operands.load_store.transfer = ARM64_MEMORY_TRANSFER_SINGLE;
                arm64_decode_ldst_rt_rn(raw, decoded);
                decoded->rs = (raw >> 16) & 0x1F;
                decoded->operands.load_store.access_bytes = 1U << ((raw >> 30) & 0x3);
                decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
                decoded->operand_width = decoded->operands.load_store.access_bytes == 8 ? 64 : 32;
                return ARM64_DECODE_OK;
            }

            if (operation == 12 && (raw & 0x3FFFFC00U) == 0x38BFC000U)
            {
                decoded->opcode = ARM64_OP_LDAPR;
                decoded->operands.load_store.access = ARM64_MEMORY_ACCESS_LOAD;
                decoded->operands.load_store.register_kind = ARM64_MEMORY_REGISTER_GPR;
                decoded->operands.load_store.memory_order = ARM64_MEMORY_ORDER_ACQUIRE;
                decoded->operands.load_store.extension = ARM64_MEMORY_EXTENSION_ZERO;
                decoded->operands.load_store.transfer = ARM64_MEMORY_TRANSFER_SINGLE;
                arm64_decode_ldst_rt_rn(raw, decoded);
                decoded->operands.load_store.access_bytes = 1U << ((raw >> 30) & 0x3);
                decoded->operands.load_store.address_mode = ARM64_ADDRESS_BASE;
                decoded->operand_width = decoded->operands.load_store.access_bytes == 8 ? 64 : 32;
                return ARM64_DECODE_OK;
            }

            if ((operation == 9 && (raw & 0xBF20FC00U) == 0x38209000U) || (operation == 10 && (raw & 0xBF20FC00U) == 0x3820A000U) || (operation == 11 && (raw & 0xBF20FC00U) == 0x3820B000U))
            {
                decoded->opcode = ARM64_OP_UNKNOWN;
                return ARM64_DECODE_UNSUPPORTED;
            }

            if ((operation == 9 && (raw & 0xFFFFFC00U) == 0xF83F9000U) || (operation == 10 && (raw & 0xFFE0FC00U) == 0xF820A000U) || (operation == 11 && (raw & 0xFFE0FC00U) == 0xF820B000U) || (operation == 13 && (raw & 0xFFFFFC00U) == 0xF83FD000U))
            {
                if ((raw & 0x1F) >= 24 || (raw & 1)) return ARM64_DECODE_UNALLOCATED;
                decoded->opcode = ARM64_OP_UNKNOWN;
                return ARM64_DECODE_UNSUPPORTED;
            }

            return ARM64_DECODE_UNALLOCATED;
        }

        {
            uint32_t size = (raw >> 30) & 0x3;
            uint32_t mode = (raw >> 10) & 0x3;
            uint32_t opc = (raw >> 22) & 0x3;

            decoded->opcode = !(raw & 0x04000000U) && size == 3 && opc == 2 ? ARM64_OP_PREFETCH : ARM64_OP_LOAD_STORE_SINGLE;
            if (decoded->opcode == ARM64_OP_PREFETCH && form == ARM64_LDST_SINGLE_REGISTER_OFFSET && (raw & 0x18U) == 0x18U)
            {
                decoded->opcode = ARM64_OP_RPRFM;
                return ARM64_DECODE_UNSUPPORTED;
            }
            if (decoded->opcode == ARM64_OP_PREFETCH)
            {
                decoded->rn = (raw >> 5) & 0x1F;
                if (form == ARM64_LDST_SINGLE_REGISTER_OFFSET) decoded->rm = (raw >> 16) & 0x1F;
                decoded->operands.load_store.prefetch_operation = raw & 0x1F;
                decoded->operands.load_store.access = ARM64_MEMORY_ACCESS_NONE;
                decoded->operands.load_store.register_kind = ARM64_MEMORY_REGISTER_NONE;
                decoded->operands.load_store.extension = ARM64_MEMORY_EXTENSION_NONE;
                decoded->operands.load_store.transfer = ARM64_MEMORY_TRANSFER_NONE;
                decoded->operand_width = 0;
            }
            else
            {
                arm64_decode_ldst_rt_rn(raw, decoded);
                arm64_decode_ldst_register_kind(raw, decoded);
                if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_FP_SIMD && opc >= 2 && size != 0) return ARM64_DECODE_UNALLOCATED;
                if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_FP_SIMD) decoded->operands.load_store.access_bytes = size == 0 && (opc & 2) ? 16 : 1U << size;
                else decoded->operands.load_store.access_bytes = 1U << size;
                decoded->operands.load_store.access = (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_FP_SIMD ? (opc & 1) : opc) ? ARM64_MEMORY_ACCESS_LOAD : ARM64_MEMORY_ACCESS_STORE;
                if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_GPR && decoded->operands.load_store.access == ARM64_MEMORY_ACCESS_LOAD) decoded->operands.load_store.extension = opc >= 2 ? ARM64_MEMORY_EXTENSION_SIGN : ARM64_MEMORY_EXTENSION_ZERO;
                decoded->operands.load_store.transfer = ARM64_MEMORY_TRANSFER_SINGLE;
                decoded->operand_width = decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_FP_SIMD ? decoded->operands.load_store.access_bytes * 8 : (size == 3 || opc == 2 ? 64 : 32);
            }

            if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_GPR && ((size == 3 && opc == 3) || (size == 2 && opc == 3))) return ARM64_DECODE_UNALLOCATED;
            if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_FP_SIMD && form == ARM64_LDST_SINGLE_IMMEDIATE && mode == 2) return ARM64_DECODE_UNALLOCATED;
            if (decoded->opcode == ARM64_OP_PREFETCH && form == ARM64_LDST_SINGLE_IMMEDIATE && mode != 0) return ARM64_DECODE_UNALLOCATED;

            switch (form)
            {
            case ARM64_LDST_SINGLE_UNSIGNED_OFFSET:
                decoded->operands.load_store.address_mode = ARM64_ADDRESS_UNSIGNED_OFFSET;
                decoded->operands.load_store.offset = ((raw >> 10) & 0xFFF) * (decoded->opcode == ARM64_OP_PREFETCH ? 8 : decoded->operands.load_store.access_bytes);
                break;
            case ARM64_LDST_SINGLE_REGISTER_OFFSET:
                decoded->operands.load_store.address_mode = ARM64_ADDRESS_REGISTER_OFFSET;
                decoded->rm = (raw >> 16) & 0x1F;
                decoded->operands.load_store.extend_type = (raw >> 13) & 0x7;
                if (decoded->operands.load_store.extend_type != 2 && decoded->operands.load_store.extend_type != 3 && decoded->operands.load_store.extend_type != 6 && decoded->operands.load_store.extend_type != 7) return ARM64_DECODE_UNALLOCATED;
                decoded->operands.load_store.shift_amount = (raw & 0x1000U) ? (decoded->opcode == ARM64_OP_PREFETCH ? 3 : (uint8_t)__builtin_ctz(decoded->operands.load_store.access_bytes)) : 0;
                break;
            default:
                decoded->operands.load_store.offset = arm64_sign_extend((raw >> 12) & 0x1FF, 9);
                decoded->operands.load_store.address_mode = mode == 1 ? ARM64_ADDRESS_POST_INDEX : mode == 2 ? ARM64_ADDRESS_UNPRIVILEGED_OFFSET : mode == 3 ? ARM64_ADDRESS_PRE_INDEX : ARM64_ADDRESS_UNSCALED_OFFSET;
                break;
            }
            if (decoded->operands.load_store.register_kind == ARM64_MEMORY_REGISTER_GPR && (decoded->operands.load_store.address_mode == ARM64_ADDRESS_PRE_INDEX || decoded->operands.load_store.address_mode == ARM64_ADDRESS_POST_INDEX) && decoded->rn != 31 && decoded->rn == decoded->rt) return ARM64_DECODE_UNPREDICTABLE;
            return ARM64_DECODE_OK;
        }
    }

    case 0x09:
    case 0x0C:
    case 0x0D:
        decoded->insn_class = ARM64_INSN_CLASS_LOAD_STORE;
        decoded->opcode = ARM64_OP_UNKNOWN;
        return ARM64_DECODE_UNSUPPORTED;

    default:
        return ARM64_DECODE_UNALLOCATED;
    }
}