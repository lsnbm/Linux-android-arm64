#include "arm64_emulate_internal.h"

/* ======================== 访存类：固定硬件模板 ======================== */

static inline enum arm64_instruction emu_ldst_instruction(const struct arm64_executor_entry *entry)
{
    return (enum arm64_instruction)(uint32_t)entry->operand1;
}

static inline enum arm64_memory_address_mode emu_ldst_address_mode(const struct arm64_executor_entry *entry)
{
    return (enum arm64_memory_address_mode)((entry->operand1 >> 32) & 0x7);
}

static inline uint32_t emu_ldst_extend_type(const struct arm64_executor_entry *entry)
{
    return (entry->operand1 >> 35) & 0x7;
}

static inline uint32_t emu_ldst_shift_amount(const struct arm64_executor_entry *entry)
{
    return (entry->operand1 >> 38) & 0xFF;
}

/* decoder 已给出寻址形式；这里只结合当前寄存器现场求出有效地址。 */
static inline bool emu_resolve_memory_address_entry(const struct arm64_executor_entry *entry, struct pt_regs *regs, uint64_t pc, uint64_t base, uint64_t *address, enum arm64_memory_address_mode address_mode, uint32_t extend_type)
{
    uint64_t index;

    switch (address_mode)
    {
    case ARM64_MEMORY_ADDRESS_LITERAL:
        *address = pc + entry->operand0;
        return true;
    case ARM64_MEMORY_ADDRESS_BASE_OFFSET:
    case ARM64_MEMORY_ADDRESS_PRE_INDEX:
        *address = base + entry->operand0;
        return true;
    case ARM64_MEMORY_ADDRESS_POST_INDEX:
        *address = base;
        return true;
    case ARM64_MEMORY_ADDRESS_REGISTER_OFFSET:
        index = reg_read(regs, entry->reg1);
        switch (extend_type)
        {
        case 2:
            index = (uint32_t)index;
            break;
        case 3:
            break;
        case 6:
            index = (uint64_t)(int64_t)(int32_t)index;
            break;
        case 7:
            break;
        default:
            return false;
        }
        *address = base + (index << emu_ldst_shift_amount(entry));
        return true;
    default:
        return false;
    }
}

static inline void emu_commit_memory_writeback_entry(const struct arm64_executor_entry *entry, struct pt_regs *regs, uint64_t base, enum arm64_memory_address_mode address_mode)
{
    if (address_mode != ARM64_MEMORY_ADDRESS_POST_INDEX && address_mode != ARM64_MEMORY_ADDRESS_PRE_INDEX) return;
    addr_reg_write(regs, entry->reg0, base + entry->operand0);
}





// clang-format off
static inline bool emu_hw_load_gpr(enum arm64_instruction instruction, uint64_t addr, int bytes, bool sf, uint64_t *out)
{
    uint64_t value;

    switch (instruction)
    {
    case ARM64_INSN_LDR_LITERAL_GPR:
        switch (bytes)
        {
        case 4: value = emu_template_ldr_w(addr); break;
        case 8: value = emu_template_ldr_x(addr); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDRSW_LITERAL:
        switch (bytes)
        {
        case 4: value = emu_template_ldrsw_x(addr); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUR_GPR:
        switch (bytes)
        {
        case 1: value = emu_template_ldurb_w(addr); break;
        case 2: value = emu_template_ldurh_w(addr); break;
        case 4: value = emu_template_ldur_w(addr); break;
        case 8: value = emu_template_ldur_x(addr); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUR_SIGNED_GPR:
        switch (bytes)
        {
        case 1:
            switch ((uint8_t)sf)
            {
            case false: value = emu_template_ldursb_w(addr); break;
            case true:  value = emu_template_ldursb_x(addr); break;
            }
            break;
        case 2:
            switch ((uint8_t)sf)
            {
            case false: value = emu_template_ldursh_w(addr); break;
            case true:  value = emu_template_ldursh_x(addr); break;
            }
            break;
        case 4:
            switch ((uint8_t)sf)
            {
            case false: return false;
            case true:  value = emu_template_ldursw_x(addr); break;
            }
            break;
        default:
            return false;
        }
        break;
    case ARM64_INSN_LDTR_GPR:
        switch (bytes)
        {
        case 1: value = emu_template_ldtrb_w(addr); break;
        case 2: value = emu_template_ldtrh_w(addr); break;
        case 4: value = emu_template_ldtr_w(addr); break;
        case 8: value = emu_template_ldtr_x(addr); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDTR_SIGNED_GPR:
        switch (bytes)
        {
        case 1:
            switch ((uint8_t)sf)
            {
            case false: value = emu_template_ldtrsb_w(addr); break;
            case true:  value = emu_template_ldtrsb_x(addr); break;
            }
            break;
        case 2:
            switch ((uint8_t)sf)
            {
            case false: value = emu_template_ldtrsh_w(addr); break;
            case true:  value = emu_template_ldtrsh_x(addr); break;
            }
            break;
        case 4:
            switch ((uint8_t)sf)
            {
            case false: return false;
            case true:  value = emu_template_ldtrsw_x(addr); break;
            }
            break;
        default:
            return false;
        }
        break;
    case ARM64_INSN_LDR_GPR_POST_INDEX:
    case ARM64_INSN_LDR_GPR_PRE_INDEX:
    case ARM64_INSN_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1: value = emu_template_ldrb_w(addr); break;
        case 2: value = emu_template_ldrh_w(addr); break;
        case 4: value = emu_template_ldr_w(addr); break;
        case 8: value = emu_template_ldr_x(addr); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX:
    case ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX:
    case ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET:
    case ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1:
            switch ((uint8_t)sf)
            {
            case false: value = emu_template_ldrsb_w(addr); break;
            case true:  value = emu_template_ldrsb_x(addr); break;
            }
            break;
        case 2:
            switch ((uint8_t)sf)
            {
            case false: value = emu_template_ldrsh_w(addr); break;
            case true:  value = emu_template_ldrsh_x(addr); break;
            }
            break;
        case 4:
            switch ((uint8_t)sf)
            {
            case false: return false;
            case true:  value = emu_template_ldrsw_x(addr); break;
            }
            break;
        default:
            return false;
        }
        break;
    default:
        return false;
    }

    *out = value;
    return true;
}

static inline bool emu_hw_store_gpr(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t value)
{
    switch (instruction)
    {
    case ARM64_INSN_STUR_GPR:
        switch (bytes)
        {
        case 1: emu_template_sturb_w(addr, value); break;
        case 2: emu_template_sturh_w(addr, value); break;
        case 4: emu_template_stur_w(addr, value); break;
        case 8: emu_template_stur_x(addr, value); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STTR_GPR:
        switch (bytes)
        {
        case 1: emu_template_sttrb_w(addr, value); break;
        case 2: emu_template_sttrh_w(addr, value); break;
        case 4: emu_template_sttr_w(addr, value); break;
        case 8: emu_template_sttr_x(addr, value); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STR_GPR_POST_INDEX:
    case ARM64_INSN_STR_GPR_PRE_INDEX:
    case ARM64_INSN_STR_GPR_REGISTER_OFFSET:
    case ARM64_INSN_STR_GPR_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1: emu_template_strb_w(addr, value); break;
        case 2: emu_template_strh_w(addr, value); break;
        case 4: emu_template_str_w(addr, value); break;
        case 8: emu_template_str_x(addr, value); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_load_fp(enum arm64_instruction instruction, uint64_t addr, int bytes, __uint128_t *out)
{
    switch (instruction)
    {
    case ARM64_INSN_LDUR_FP_SIMD:
        switch (bytes)
        {
        case 1:  emu_template_ldur_fp_b(addr, out); break;
        case 2:  emu_template_ldur_fp_h(addr, out); break;
        case 4:  emu_template_ldur_fp_s(addr, out); break;
        case 8:  emu_template_ldur_fp_d(addr, out); break;
        case 16: emu_template_ldur_fp_q(addr, out); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDR_LITERAL_FP_SIMD:
    case ARM64_INSN_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INSN_LDR_FP_SIMD_PRE_INDEX:
    case ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1:  emu_template_ldr_fp_b(addr, out); break;
        case 2:  emu_template_ldr_fp_h(addr, out); break;
        case 4:  emu_template_ldr_fp_s(addr, out); break;
        case 8:  emu_template_ldr_fp_d(addr, out); break;
        case 16: emu_template_ldr_fp_q(addr, out); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_store_fp(enum arm64_instruction instruction, uint64_t addr, int bytes, const __uint128_t *value)
{
    switch (instruction)
    {
    case ARM64_INSN_STUR_FP_SIMD:
        switch (bytes)
        {
        case 1:  emu_template_stur_fp_b(addr, value); break;
        case 2:  emu_template_stur_fp_h(addr, value); break;
        case 4:  emu_template_stur_fp_s(addr, value); break;
        case 8:  emu_template_stur_fp_d(addr, value); break;
        case 16: emu_template_stur_fp_q(addr, value); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STR_FP_SIMD_POST_INDEX:
    case ARM64_INSN_STR_FP_SIMD_PRE_INDEX:
    case ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET:
        switch (bytes)
        {
        case 1:  emu_template_str_fp_b(addr, value); break;
        case 2:  emu_template_str_fp_h(addr, value); break;
        case 4:  emu_template_str_fp_s(addr, value); break;
        case 8:  emu_template_str_fp_d(addr, value); break;
        case 16: emu_template_str_fp_q(addr, value); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_load_pair_gpr(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t *first, uint64_t *second)
{
    switch (instruction)
    {
    case ARM64_INSN_LDNP_GPR:
        switch (bytes)
        {
        case 4: emu_template_ldnp_gpr_w(addr, first, second); break;
        case 8: emu_template_ldnp_gpr_x(addr, first, second); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDP_GPR_OFFSET:
    case ARM64_INSN_LDP_GPR_POST_INDEX:
    case ARM64_INSN_LDP_GPR_PRE_INDEX:
        switch (bytes)
        {
        case 4: emu_template_ldp_gpr_w(addr, first, second); break;
        case 8: emu_template_ldp_gpr_x(addr, first, second); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDPSW_OFFSET:
    case ARM64_INSN_LDPSW_POST_INDEX:
    case ARM64_INSN_LDPSW_PRE_INDEX:
        switch (bytes)
        {
        case 4: emu_template_ldpsw_gpr_x(addr, first, second); break;
        default: return false;
        }
        break;
    default:
        return false;
    }

    return true;
}

static inline bool emu_hw_store_pair_gpr(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t first, uint64_t second)
{
    switch (instruction)
    {
    case ARM64_INSN_STNP_GPR:
        switch (bytes)
        {
        case 4: emu_template_stnp_gpr_w(addr, first, second); break;
        case 8: emu_template_stnp_gpr_x(addr, first, second); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STP_GPR_OFFSET:
    case ARM64_INSN_STP_GPR_POST_INDEX:
    case ARM64_INSN_STP_GPR_PRE_INDEX:
        switch (bytes)
        {
        case 4: emu_template_stp_gpr_w(addr, first, second); break;
        case 8: emu_template_stp_gpr_x(addr, first, second); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_load_pair_fp(enum arm64_instruction instruction, uint64_t addr, int bytes, __uint128_t *first, __uint128_t *second)
{
    switch (instruction)
    {
    case ARM64_INSN_LDNP_FP_SIMD:
        switch (bytes)
        {
        case 4:  emu_template_ldnp_fp_s(addr, first, second); break;
        case 8:  emu_template_ldnp_fp_d(addr, first, second); break;
        case 16: emu_template_ldnp_fp_q(addr, first, second); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INSN_LDP_FP_SIMD_OFFSET:
    case ARM64_INSN_LDP_FP_SIMD_PRE_INDEX:
        switch (bytes)
        {
        case 4:  emu_template_ldp_fp_s(addr, first, second); break;
        case 8:  emu_template_ldp_fp_d(addr, first, second); break;
        case 16: emu_template_ldp_fp_q(addr, first, second); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_store_pair_fp(enum arm64_instruction instruction, uint64_t addr, int bytes, const __uint128_t *first, const __uint128_t *second)
{
    switch (instruction)
    {
    case ARM64_INSN_STNP_FP_SIMD:
        switch (bytes)
        {
        case 4:  emu_template_stnp_fp_s(addr, first, second); break;
        case 8:  emu_template_stnp_fp_d(addr, first, second); break;
        case 16: emu_template_stnp_fp_q(addr, first, second); break;
        default: return false;
        }
        break;
    case ARM64_INSN_STP_FP_SIMD_POST_INDEX:
    case ARM64_INSN_STP_FP_SIMD_OFFSET:
    case ARM64_INSN_STP_FP_SIMD_PRE_INDEX:
        switch (bytes)
        {
        case 4:  emu_template_stp_fp_s(addr, first, second); break;
        case 8:  emu_template_stp_fp_d(addr, first, second); break;
        case 16: emu_template_stp_fp_q(addr, first, second); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    return true;
}

static inline bool emu_hw_load_rcpc(enum arm64_instruction instruction, uint64_t addr, int bytes, bool sf, uint64_t *out)
{
    uint64_t value;

    switch (instruction)
    {
    case ARM64_INSN_LDAPUR:
        switch (bytes)
        {
        case 1: value = emu_template_ldapurb_w(addr); break;
        case 2: value = emu_template_ldapurh_w(addr); break;
        case 4: value = emu_template_ldapur_w(addr); break;
        case 8: value = emu_template_ldapur_x(addr); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDAPUR_SIGNED:
        switch (bytes)
        {
        case 1:
            switch ((uint8_t)sf)
            {
            case false: value = emu_template_ldapursb_w(addr); break;
            case true: value = emu_template_ldapursb_x(addr); break;
            default: return false;
            }
            break;
        case 2:
            switch ((uint8_t)sf)
            {
            case false: value = emu_template_ldapursh_w(addr); break;
            case true: value = emu_template_ldapursh_x(addr); break;
            default: return false;
            }
            break;
        case 4:
            switch ((uint8_t)sf)
            {
            case false: return false;
            case true: value = emu_template_ldapursw_x(addr); break;
            default: return false;
            }
            break;
        default: return false;
        }
        break;
    default:
        return false;
    }

    *out = value;
    return true;
}

static inline bool emu_hw_store_rcpc(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t value)
{
    if (instruction != ARM64_INSN_STLUR) return false;

    switch (bytes)
    {
    case 1: emu_template_stlurb_w(addr, value); break;
    case 2: emu_template_stlurh_w(addr, value); break;
    case 4: emu_template_stlur_w(addr, value); break;
    case 8: emu_template_stlur_x(addr, value); break;
    default: return false;
    }

    return true;
}

static inline bool emu_hw_load_ldapr(uint64_t addr, int bytes, uint64_t *out)
{
    uint64_t value;

    switch (bytes)
    {
    case 1: value = emu_template_ldaprb_w(addr); break;
    case 2: value = emu_template_ldaprh_w(addr); break;
    case 4: value = emu_template_ldapr_w(addr); break;
    case 8: value = emu_template_ldapr_x(addr); break;
    default: return false;
    }

    *out = value;
    return true;
}

static inline bool emu_hw_atomic_rmw(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t src, uint64_t *old)
{
    uint64_t value;

    switch (instruction)
    {
    case ARM64_INSN_LDADD:
        switch (bytes)
        {
        case 1: value = emu_template_ldadd_b(addr, src); break;
        case 2: value = emu_template_ldadd_h(addr, src); break;
        case 4: value = emu_template_ldadd_w(addr, src); break;
        case 8: value = emu_template_ldadd_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDADDA:
        switch (bytes)
        {
        case 1: value = emu_template_ldadda_b(addr, src); break;
        case 2: value = emu_template_ldadda_h(addr, src); break;
        case 4: value = emu_template_ldadda_w(addr, src); break;
        case 8: value = emu_template_ldadda_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDADDL:
        switch (bytes)
        {
        case 1: value = emu_template_ldaddl_b(addr, src); break;
        case 2: value = emu_template_ldaddl_h(addr, src); break;
        case 4: value = emu_template_ldaddl_w(addr, src); break;
        case 8: value = emu_template_ldaddl_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDADDAL:
        switch (bytes)
        {
        case 1: value = emu_template_ldaddal_b(addr, src); break;
        case 2: value = emu_template_ldaddal_h(addr, src); break;
        case 4: value = emu_template_ldaddal_w(addr, src); break;
        case 8: value = emu_template_ldaddal_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDCLR:
        switch (bytes)
        {
        case 1: value = emu_template_ldclr_b(addr, src); break;
        case 2: value = emu_template_ldclr_h(addr, src); break;
        case 4: value = emu_template_ldclr_w(addr, src); break;
        case 8: value = emu_template_ldclr_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDCLRA:
        switch (bytes)
        {
        case 1: value = emu_template_ldclra_b(addr, src); break;
        case 2: value = emu_template_ldclra_h(addr, src); break;
        case 4: value = emu_template_ldclra_w(addr, src); break;
        case 8: value = emu_template_ldclra_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDCLRL:
        switch (bytes)
        {
        case 1: value = emu_template_ldclrl_b(addr, src); break;
        case 2: value = emu_template_ldclrl_h(addr, src); break;
        case 4: value = emu_template_ldclrl_w(addr, src); break;
        case 8: value = emu_template_ldclrl_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDCLRAL:
        switch (bytes)
        {
        case 1: value = emu_template_ldclral_b(addr, src); break;
        case 2: value = emu_template_ldclral_h(addr, src); break;
        case 4: value = emu_template_ldclral_w(addr, src); break;
        case 8: value = emu_template_ldclral_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDEOR:
        switch (bytes)
        {
        case 1: value = emu_template_ldeor_b(addr, src); break;
        case 2: value = emu_template_ldeor_h(addr, src); break;
        case 4: value = emu_template_ldeor_w(addr, src); break;
        case 8: value = emu_template_ldeor_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDEORA:
        switch (bytes)
        {
        case 1: value = emu_template_ldeora_b(addr, src); break;
        case 2: value = emu_template_ldeora_h(addr, src); break;
        case 4: value = emu_template_ldeora_w(addr, src); break;
        case 8: value = emu_template_ldeora_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDEORL:
        switch (bytes)
        {
        case 1: value = emu_template_ldeorl_b(addr, src); break;
        case 2: value = emu_template_ldeorl_h(addr, src); break;
        case 4: value = emu_template_ldeorl_w(addr, src); break;
        case 8: value = emu_template_ldeorl_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDEORAL:
        switch (bytes)
        {
        case 1: value = emu_template_ldeoral_b(addr, src); break;
        case 2: value = emu_template_ldeoral_h(addr, src); break;
        case 4: value = emu_template_ldeoral_w(addr, src); break;
        case 8: value = emu_template_ldeoral_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSET:
        switch (bytes)
        {
        case 1: value = emu_template_ldset_b(addr, src); break;
        case 2: value = emu_template_ldset_h(addr, src); break;
        case 4: value = emu_template_ldset_w(addr, src); break;
        case 8: value = emu_template_ldset_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSETA:
        switch (bytes)
        {
        case 1: value = emu_template_ldseta_b(addr, src); break;
        case 2: value = emu_template_ldseta_h(addr, src); break;
        case 4: value = emu_template_ldseta_w(addr, src); break;
        case 8: value = emu_template_ldseta_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSETL:
        switch (bytes)
        {
        case 1: value = emu_template_ldsetl_b(addr, src); break;
        case 2: value = emu_template_ldsetl_h(addr, src); break;
        case 4: value = emu_template_ldsetl_w(addr, src); break;
        case 8: value = emu_template_ldsetl_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSETAL:
        switch (bytes)
        {
        case 1: value = emu_template_ldsetal_b(addr, src); break;
        case 2: value = emu_template_ldsetal_h(addr, src); break;
        case 4: value = emu_template_ldsetal_w(addr, src); break;
        case 8: value = emu_template_ldsetal_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMAX:
        switch (bytes)
        {
        case 1: value = emu_template_ldsmax_b(addr, src); break;
        case 2: value = emu_template_ldsmax_h(addr, src); break;
        case 4: value = emu_template_ldsmax_w(addr, src); break;
        case 8: value = emu_template_ldsmax_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMAXA:
        switch (bytes)
        {
        case 1: value = emu_template_ldsmaxa_b(addr, src); break;
        case 2: value = emu_template_ldsmaxa_h(addr, src); break;
        case 4: value = emu_template_ldsmaxa_w(addr, src); break;
        case 8: value = emu_template_ldsmaxa_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMAXL:
        switch (bytes)
        {
        case 1: value = emu_template_ldsmaxl_b(addr, src); break;
        case 2: value = emu_template_ldsmaxl_h(addr, src); break;
        case 4: value = emu_template_ldsmaxl_w(addr, src); break;
        case 8: value = emu_template_ldsmaxl_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMAXAL:
        switch (bytes)
        {
        case 1: value = emu_template_ldsmaxal_b(addr, src); break;
        case 2: value = emu_template_ldsmaxal_h(addr, src); break;
        case 4: value = emu_template_ldsmaxal_w(addr, src); break;
        case 8: value = emu_template_ldsmaxal_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMIN:
        switch (bytes)
        {
        case 1: value = emu_template_ldsmin_b(addr, src); break;
        case 2: value = emu_template_ldsmin_h(addr, src); break;
        case 4: value = emu_template_ldsmin_w(addr, src); break;
        case 8: value = emu_template_ldsmin_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMINA:
        switch (bytes)
        {
        case 1: value = emu_template_ldsmina_b(addr, src); break;
        case 2: value = emu_template_ldsmina_h(addr, src); break;
        case 4: value = emu_template_ldsmina_w(addr, src); break;
        case 8: value = emu_template_ldsmina_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMINL:
        switch (bytes)
        {
        case 1: value = emu_template_ldsminl_b(addr, src); break;
        case 2: value = emu_template_ldsminl_h(addr, src); break;
        case 4: value = emu_template_ldsminl_w(addr, src); break;
        case 8: value = emu_template_ldsminl_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDSMINAL:
        switch (bytes)
        {
        case 1: value = emu_template_ldsminal_b(addr, src); break;
        case 2: value = emu_template_ldsminal_h(addr, src); break;
        case 4: value = emu_template_ldsminal_w(addr, src); break;
        case 8: value = emu_template_ldsminal_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMAX:
        switch (bytes)
        {
        case 1: value = emu_template_ldumax_b(addr, src); break;
        case 2: value = emu_template_ldumax_h(addr, src); break;
        case 4: value = emu_template_ldumax_w(addr, src); break;
        case 8: value = emu_template_ldumax_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMAXA:
        switch (bytes)
        {
        case 1: value = emu_template_ldumaxa_b(addr, src); break;
        case 2: value = emu_template_ldumaxa_h(addr, src); break;
        case 4: value = emu_template_ldumaxa_w(addr, src); break;
        case 8: value = emu_template_ldumaxa_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMAXL:
        switch (bytes)
        {
        case 1: value = emu_template_ldumaxl_b(addr, src); break;
        case 2: value = emu_template_ldumaxl_h(addr, src); break;
        case 4: value = emu_template_ldumaxl_w(addr, src); break;
        case 8: value = emu_template_ldumaxl_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMAXAL:
        switch (bytes)
        {
        case 1: value = emu_template_ldumaxal_b(addr, src); break;
        case 2: value = emu_template_ldumaxal_h(addr, src); break;
        case 4: value = emu_template_ldumaxal_w(addr, src); break;
        case 8: value = emu_template_ldumaxal_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMIN:
        switch (bytes)
        {
        case 1: value = emu_template_ldumin_b(addr, src); break;
        case 2: value = emu_template_ldumin_h(addr, src); break;
        case 4: value = emu_template_ldumin_w(addr, src); break;
        case 8: value = emu_template_ldumin_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMINA:
        switch (bytes)
        {
        case 1: value = emu_template_ldumina_b(addr, src); break;
        case 2: value = emu_template_ldumina_h(addr, src); break;
        case 4: value = emu_template_ldumina_w(addr, src); break;
        case 8: value = emu_template_ldumina_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMINL:
        switch (bytes)
        {
        case 1: value = emu_template_lduminl_b(addr, src); break;
        case 2: value = emu_template_lduminl_h(addr, src); break;
        case 4: value = emu_template_lduminl_w(addr, src); break;
        case 8: value = emu_template_lduminl_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDUMINAL:
        switch (bytes)
        {
        case 1: value = emu_template_lduminal_b(addr, src); break;
        case 2: value = emu_template_lduminal_h(addr, src); break;
        case 4: value = emu_template_lduminal_w(addr, src); break;
        case 8: value = emu_template_lduminal_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_SWP:
        switch (bytes)
        {
        case 1: value = emu_template_swp_b(addr, src); break;
        case 2: value = emu_template_swp_h(addr, src); break;
        case 4: value = emu_template_swp_w(addr, src); break;
        case 8: value = emu_template_swp_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_SWPA:
        switch (bytes)
        {
        case 1: value = emu_template_swpa_b(addr, src); break;
        case 2: value = emu_template_swpa_h(addr, src); break;
        case 4: value = emu_template_swpa_w(addr, src); break;
        case 8: value = emu_template_swpa_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_SWPL:
        switch (bytes)
        {
        case 1: value = emu_template_swpl_b(addr, src); break;
        case 2: value = emu_template_swpl_h(addr, src); break;
        case 4: value = emu_template_swpl_w(addr, src); break;
        case 8: value = emu_template_swpl_x(addr, src); break;
        default: return false;
        }
        break;
    case ARM64_INSN_SWPAL:
        switch (bytes)
        {
        case 1: value = emu_template_swpal_b(addr, src); break;
        case 2: value = emu_template_swpal_h(addr, src); break;
        case 4: value = emu_template_swpal_w(addr, src); break;
        case 8: value = emu_template_swpal_x(addr, src); break;
        default: return false;
        }
        break;
    default:
        return false;
    }

    *old = value;
    return true;
}

static inline bool emu_hw_cas(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t desired, uint64_t *expected)
{
    uint64_t value = *expected;

    switch (instruction)
    {
    case ARM64_INSN_CAS:
        switch (bytes)
        {
        case 1: value = emu_template_cas_b(addr, value, desired); break;
        case 2: value = emu_template_cas_h(addr, value, desired); break;
        case 4: value = emu_template_cas_w(addr, value, desired); break;
        case 8: value = emu_template_cas_x(addr, value, desired); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASA:
        switch (bytes)
        {
        case 1: value = emu_template_casa_b(addr, value, desired); break;
        case 2: value = emu_template_casa_h(addr, value, desired); break;
        case 4: value = emu_template_casa_w(addr, value, desired); break;
        case 8: value = emu_template_casa_x(addr, value, desired); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASL:
        switch (bytes)
        {
        case 1: value = emu_template_casl_b(addr, value, desired); break;
        case 2: value = emu_template_casl_h(addr, value, desired); break;
        case 4: value = emu_template_casl_w(addr, value, desired); break;
        case 8: value = emu_template_casl_x(addr, value, desired); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASAL:
        switch (bytes)
        {
        case 1: value = emu_template_casal_b(addr, value, desired); break;
        case 2: value = emu_template_casal_h(addr, value, desired); break;
        case 4: value = emu_template_casal_w(addr, value, desired); break;
        case 8: value = emu_template_casal_x(addr, value, desired); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    *expected = value;
    return true;
}

static inline bool emu_hw_casp(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t desired0, uint64_t desired1, uint64_t *expected0, uint64_t *expected1)
{
    uint64_t input0 = *expected0;
    uint64_t input1 = *expected1;
    uint64_t output[2];

    switch (instruction)
    {
    case ARM64_INSN_CASP:
        switch (bytes)
        {
        case 4: emu_template_casp_w(addr, input0, input1, desired0, desired1, output); break;
        case 8: emu_template_casp_x(addr, input0, input1, desired0, desired1, output); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASPA:
        switch (bytes)
        {
        case 4: emu_template_caspa_w(addr, input0, input1, desired0, desired1, output); break;
        case 8: emu_template_caspa_x(addr, input0, input1, desired0, desired1, output); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASPL:
        switch (bytes)
        {
        case 4: emu_template_caspl_w(addr, input0, input1, desired0, desired1, output); break;
        case 8: emu_template_caspl_x(addr, input0, input1, desired0, desired1, output); break;
        default: return false;
        }
        break;
    case ARM64_INSN_CASPAL:
        switch (bytes)
        {
        case 4: emu_template_caspal_w(addr, input0, input1, desired0, desired1, output); break;
        case 8: emu_template_caspal_x(addr, input0, input1, desired0, desired1, output); break;
        default: return false;
        }
        break;
    default:
        return false;
    }
    *expected0 = output[0];
    *expected1 = output[1];
    return true;
}

static inline bool emu_hw_ordered_load(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t *out)
{
    uint64_t value;

    switch (instruction)
    {
    case ARM64_INSN_LDLAR:
        switch (bytes)
        {
        case 1: value = emu_template_ldlarb_w(addr); break;
        case 2: value = emu_template_ldlarh_w(addr); break;
        case 4: value = emu_template_ldlar_w(addr); break;
        case 8: value = emu_template_ldlar_x(addr); break;
        default: return false;
        }
        break;
    case ARM64_INSN_LDAR:
        switch (bytes)
        {
        case 1: value = emu_template_ldarb_w(addr); break;
        case 2: value = emu_template_ldarh_w(addr); break;
        case 4: value = emu_template_ldar_w(addr); break;
        case 8: value = emu_template_ldar_x(addr); break;
        default: return false;
        }
        break;
    default:
        return false;
    }

    *out = value;
    return true;
}

static inline bool emu_hw_ordered_store(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t value)
{
    switch (instruction)
    {
    case ARM64_INSN_STLLR:
        switch (bytes)
        {
        case 1: emu_template_stllrb_w(addr, value); break;
        case 2: emu_template_stllrh_w(addr, value); break;
        case 4: emu_template_stllr_w(addr, value); break;
        case 8: emu_template_stllr_x(addr, value); break;
        default: return false;
        }
        return true;
    case ARM64_INSN_STLR:
        switch (bytes)
        {
        case 1: emu_template_stlrb_w(addr, value); break;
        case 2: emu_template_stlrh_w(addr, value); break;
        case 4: emu_template_stlr_w(addr, value); break;
        case 8: emu_template_stlr_x(addr, value); break;
        default: return false;
        }
        return true;
    default:
        return false;
    }
}

static inline bool emu_hw_exclusive_load(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t *first, uint64_t *second)
{
    uint64_t value0, value1 = 0;

    switch (bytes)
    {
    case 1:
        switch (instruction)
        {
        case ARM64_INSN_LDXR: value0 = emu_template_ldxrb_w(addr); break;
        case ARM64_INSN_LDAXR: value0 = emu_template_ldaxrb_w(addr); break;
        default: return false;
        }
        break;
    case 2:
        switch (instruction)
        {
        case ARM64_INSN_LDXR: value0 = emu_template_ldxrh_w(addr); break;
        case ARM64_INSN_LDAXR: value0 = emu_template_ldaxrh_w(addr); break;
        default: return false;
        }
        break;
    case 4:
        switch (instruction)
        {
        case ARM64_INSN_LDXR: value0 = emu_template_ldxr_w(addr); break;
        case ARM64_INSN_LDAXR: value0 = emu_template_ldaxr_w(addr); break;
        case ARM64_INSN_LDXP: emu_template_ldxp_w(addr, &value0, &value1); break;
        case ARM64_INSN_LDAXP: emu_template_ldaxp_w(addr, &value0, &value1); break;
        default: return false;
        }
        break;
    case 8:
        switch (instruction)
        {
        case ARM64_INSN_LDXR: value0 = emu_template_ldxr_x(addr); break;
        case ARM64_INSN_LDAXR: value0 = emu_template_ldaxr_x(addr); break;
        case ARM64_INSN_LDXP: emu_template_ldxp_x(addr, &value0, &value1); break;
        case ARM64_INSN_LDAXP: emu_template_ldaxp_x(addr, &value0, &value1); break;
        default: return false;
        }
        break;
    default: return false;
    }

    *first = value0;
    if (instruction == ARM64_INSN_LDXP || instruction == ARM64_INSN_LDAXP) *second = value1;
    return true;
}

static inline bool emu_hw_exclusive_store(enum arm64_instruction instruction, uint64_t addr, int bytes, uint64_t first, uint64_t second, uint32_t *status)
{
    uint32_t result;

    switch (bytes)
    {
    case 1:
        switch (instruction)
        {
        case ARM64_INSN_STXR: result = emu_template_stxrb_w(addr, first); break;
        case ARM64_INSN_STLXR: result = emu_template_stlxrb_w(addr, first); break;
        default: return false;
        }
        break;
    case 2:
        switch (instruction)
        {
        case ARM64_INSN_STXR: result = emu_template_stxrh_w(addr, first); break;
        case ARM64_INSN_STLXR: result = emu_template_stlxrh_w(addr, first); break;
        default: return false;
        }
        break;
    case 4:
        switch (instruction)
        {
        case ARM64_INSN_STXR: result = emu_template_stxr_w(addr, first); break;
        case ARM64_INSN_STLXR: result = emu_template_stlxr_w(addr, first); break;
        case ARM64_INSN_STXP: result = emu_template_stxp_w(addr, first, second); break;
        case ARM64_INSN_STLXP: result = emu_template_stlxp_w(addr, first, second); break;
        default: return false;
        }
        break;
    case 8:
        switch (instruction)
        {
        case ARM64_INSN_STXR: result = emu_template_stxr_x(addr, first); break;
        case ARM64_INSN_STLXR: result = emu_template_stlxr_x(addr, first); break;
        case ARM64_INSN_STXP: result = emu_template_stxp_x(addr, first, second); break;
        case ARM64_INSN_STLXP: result = emu_template_stlxp_x(addr, first, second); break;
        default: return false;
        }
        break;
    default: return false;
    }

    *status = result;
    return true;
}
// clang-format on

/* ======================== 访存类：缓存条目执行模板 ======================== */

/* 每个固定访存执行模板直接对应缓存条目中的 execute 函数地址。 */

static enum emu_insn_result emu_execute_ldst_ldxr_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        // 单寄存器形式复用成对加载 helper，第二个输出不参与架构结果。
        uint64_t value, unused;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDXR, addr, 1, &value, &unused)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占加载：建立独占监视，并将两个结果分别写回 Rt 和 Rt2。
}

static enum emu_insn_result emu_execute_ldst_ldxr_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        // 单寄存器形式复用成对加载 helper，第二个输出不参与架构结果。
        uint64_t value, unused;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDXR, addr, 2, &value, &unused)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占加载：建立独占监视，并将两个结果分别写回 Rt 和 Rt2。
}

static enum emu_insn_result emu_execute_ldst_ldxr_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        // 单寄存器形式复用成对加载 helper，第二个输出不参与架构结果。
        uint64_t value, unused;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDXR, addr, 4, &value, &unused)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占加载：建立独占监视，并将两个结果分别写回 Rt 和 Rt2。
}

static enum emu_insn_result emu_execute_ldst_ldxr_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        // 单寄存器形式复用成对加载 helper，第二个输出不参与架构结果。
        uint64_t value, unused;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDXR, addr, 8, &value, &unused)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占加载：建立独占监视，并将两个结果分别写回 Rt 和 Rt2。
}

static enum emu_insn_result emu_execute_ldst_ldaxr_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        // 单寄存器形式复用成对加载 helper，第二个输出不参与架构结果。
        uint64_t value, unused;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDAXR, addr, 1, &value, &unused)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占加载：建立独占监视，并将两个结果分别写回 Rt 和 Rt2。
}

static enum emu_insn_result emu_execute_ldst_ldaxr_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        // 单寄存器形式复用成对加载 helper，第二个输出不参与架构结果。
        uint64_t value, unused;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDAXR, addr, 2, &value, &unused)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占加载：建立独占监视，并将两个结果分别写回 Rt 和 Rt2。
}

static enum emu_insn_result emu_execute_ldst_ldaxr_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        // 单寄存器形式复用成对加载 helper，第二个输出不参与架构结果。
        uint64_t value, unused;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDAXR, addr, 4, &value, &unused)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占加载：建立独占监视，并将两个结果分别写回 Rt 和 Rt2。
}

static enum emu_insn_result emu_execute_ldst_ldaxr_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        // 单寄存器形式复用成对加载 helper，第二个输出不参与架构结果。
        uint64_t value, unused;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDAXR, addr, 8, &value, &unused)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占加载：建立独占监视，并将两个结果分别写回 Rt 和 Rt2。
}

static enum emu_insn_result emu_execute_ldst_ldxp_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value0, value1;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDXP, addr, 4, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, false);
        reg_write(regs, entry->reg3, value1, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器独占存储：数据来自 Rt，硬件成败状态写回 Ws（0 表示成功）。
}

static enum emu_insn_result emu_execute_ldst_ldxp_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value0, value1;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDXP, addr, 8, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, true);
        reg_write(regs, entry->reg3, value1, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器独占存储：数据来自 Rt，硬件成败状态写回 Ws（0 表示成功）。
}

static enum emu_insn_result emu_execute_ldst_ldaxp_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value0, value1;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDAXP, addr, 4, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, false);
        reg_write(regs, entry->reg3, value1, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器独占存储：数据来自 Rt，硬件成败状态写回 Ws（0 表示成功）。
}

static enum emu_insn_result emu_execute_ldst_ldaxp_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value0, value1;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_load(ARM64_INSN_LDAXP, addr, 8, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, true);
        reg_write(regs, entry->reg3, value1, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器独占存储：数据来自 Rt，硬件成败状态写回 Ws（0 表示成功）。
}

static enum emu_insn_result emu_execute_ldst_stxr_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STXR, addr, 1, reg_read(regs, entry->reg2), 0, &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占存储：数据来自 Rt/Rt2，硬件成败状态写回 Ws。
}

static enum emu_insn_result emu_execute_ldst_stxr_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STXR, addr, 2, reg_read(regs, entry->reg2), 0, &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占存储：数据来自 Rt/Rt2，硬件成败状态写回 Ws。
}

static enum emu_insn_result emu_execute_ldst_stxr_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STXR, addr, 4, reg_read(regs, entry->reg2), 0, &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占存储：数据来自 Rt/Rt2，硬件成败状态写回 Ws。
}

static enum emu_insn_result emu_execute_ldst_stxr_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STXR, addr, 8, reg_read(regs, entry->reg2), 0, &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占存储：数据来自 Rt/Rt2，硬件成败状态写回 Ws。
}

static enum emu_insn_result emu_execute_ldst_stlxr_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STLXR, addr, 1, reg_read(regs, entry->reg2), 0, &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占存储：数据来自 Rt/Rt2，硬件成败状态写回 Ws。
}

static enum emu_insn_result emu_execute_ldst_stlxr_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STLXR, addr, 2, reg_read(regs, entry->reg2), 0, &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占存储：数据来自 Rt/Rt2，硬件成败状态写回 Ws。
}

static enum emu_insn_result emu_execute_ldst_stlxr_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STLXR, addr, 4, reg_read(regs, entry->reg2), 0, &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占存储：数据来自 Rt/Rt2，硬件成败状态写回 Ws。
}

static enum emu_insn_result emu_execute_ldst_stlxr_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STLXR, addr, 8, reg_read(regs, entry->reg2), 0, &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对独占存储：数据来自 Rt/Rt2，硬件成败状态写回 Ws。
}

static enum emu_insn_result emu_execute_ldst_stxp_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STXP, addr, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3), &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对比较交换：Rs/Rs+1 提供期望值并接收内存旧值，Rt/Rt+1 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_stxp_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STXP, addr, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3), &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对比较交换：Rs/Rs+1 提供期望值并接收内存旧值，Rt/Rt+1 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_stlxp_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STLXP, addr, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3), &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对比较交换：Rs/Rs+1 提供期望值并接收内存旧值，Rt/Rt+1 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_stlxp_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint32_t status;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_exclusive_store(ARM64_INSN_STLXP, addr, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3), &status)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, status, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 成对比较交换：Rs/Rs+1 提供期望值并接收内存旧值，Rt/Rt+1 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_casp_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected0 = reg_read(regs, entry->reg4);
        uint64_t expected1 = reg_read(regs, entry->reg4 + 1);

        if (!emu_hw_casp(ARM64_INSN_CASP, addr, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg2 + 1), &expected0, &expected1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected0, false);
        reg_write(regs, entry->reg4 + 1, expected1, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序存储：按 instruction 保留 release 或 limited-ordering 语义。
}

static enum emu_insn_result emu_execute_ldst_casp_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected0 = reg_read(regs, entry->reg4);
        uint64_t expected1 = reg_read(regs, entry->reg4 + 1);

        if (!emu_hw_casp(ARM64_INSN_CASP, addr, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg2 + 1), &expected0, &expected1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected0, true);
        reg_write(regs, entry->reg4 + 1, expected1, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序存储：按 instruction 保留 release 或 limited-ordering 语义。
}

static enum emu_insn_result emu_execute_ldst_caspa_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected0 = reg_read(regs, entry->reg4);
        uint64_t expected1 = reg_read(regs, entry->reg4 + 1);

        if (!emu_hw_casp(ARM64_INSN_CASPA, addr, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg2 + 1), &expected0, &expected1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected0, false);
        reg_write(regs, entry->reg4 + 1, expected1, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序存储：按 instruction 保留 release 或 limited-ordering 语义。
}

static enum emu_insn_result emu_execute_ldst_caspa_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected0 = reg_read(regs, entry->reg4);
        uint64_t expected1 = reg_read(regs, entry->reg4 + 1);

        if (!emu_hw_casp(ARM64_INSN_CASPA, addr, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg2 + 1), &expected0, &expected1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected0, true);
        reg_write(regs, entry->reg4 + 1, expected1, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序存储：按 instruction 保留 release 或 limited-ordering 语义。
}

static enum emu_insn_result emu_execute_ldst_caspl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected0 = reg_read(regs, entry->reg4);
        uint64_t expected1 = reg_read(regs, entry->reg4 + 1);

        if (!emu_hw_casp(ARM64_INSN_CASPL, addr, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg2 + 1), &expected0, &expected1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected0, false);
        reg_write(regs, entry->reg4 + 1, expected1, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序存储：按 instruction 保留 release 或 limited-ordering 语义。
}

static enum emu_insn_result emu_execute_ldst_caspl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected0 = reg_read(regs, entry->reg4);
        uint64_t expected1 = reg_read(regs, entry->reg4 + 1);

        if (!emu_hw_casp(ARM64_INSN_CASPL, addr, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg2 + 1), &expected0, &expected1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected0, true);
        reg_write(regs, entry->reg4 + 1, expected1, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序存储：按 instruction 保留 release 或 limited-ordering 语义。
}

static enum emu_insn_result emu_execute_ldst_caspal_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected0 = reg_read(regs, entry->reg4);
        uint64_t expected1 = reg_read(regs, entry->reg4 + 1);

        if (!emu_hw_casp(ARM64_INSN_CASPAL, addr, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg2 + 1), &expected0, &expected1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected0, false);
        reg_write(regs, entry->reg4 + 1, expected1, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序存储：按 instruction 保留 release 或 limited-ordering 语义。
}

static enum emu_insn_result emu_execute_ldst_caspal_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected0 = reg_read(regs, entry->reg4);
        uint64_t expected1 = reg_read(regs, entry->reg4 + 1);

        if (!emu_hw_casp(ARM64_INSN_CASPAL, addr, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg2 + 1), &expected0, &expected1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected0, true);
        reg_write(regs, entry->reg4 + 1, expected1, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序存储：按 instruction 保留 release 或 limited-ordering 语义。
}

static enum emu_insn_result emu_execute_ldst_stllr_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_store(ARM64_INSN_STLLR, addr, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序加载：按 instruction 保留 acquire 或 limited-ordering 语义并写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stllr_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_store(ARM64_INSN_STLLR, addr, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序加载：按 instruction 保留 acquire 或 limited-ordering 语义并写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stllr_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_store(ARM64_INSN_STLLR, addr, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序加载：按 instruction 保留 acquire 或 limited-ordering 语义并写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stllr_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_store(ARM64_INSN_STLLR, addr, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序加载：按 instruction 保留 acquire 或 limited-ordering 语义并写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stlr_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_store(ARM64_INSN_STLR, addr, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序加载：按 instruction 保留 acquire 或 limited-ordering 语义并写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stlr_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_store(ARM64_INSN_STLR, addr, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序加载：按 instruction 保留 acquire 或 limited-ordering 语义并写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stlr_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_store(ARM64_INSN_STLR, addr, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序加载：按 instruction 保留 acquire 或 limited-ordering 语义并写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stlr_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_store(ARM64_INSN_STLR, addr, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 有序加载：按 instruction 保留 acquire 或 limited-ordering 语义并写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldlar_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_load(ARM64_INSN_LDLAR, addr, 1, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器比较交换：Rs 提供期望值并接收内存旧值，Rt 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_ldlar_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_load(ARM64_INSN_LDLAR, addr, 2, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器比较交换：Rs 提供期望值并接收内存旧值，Rt 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_ldlar_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_load(ARM64_INSN_LDLAR, addr, 4, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器比较交换：Rs 提供期望值并接收内存旧值，Rt 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_ldlar_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_load(ARM64_INSN_LDLAR, addr, 8, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器比较交换：Rs 提供期望值并接收内存旧值，Rt 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_ldar_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_load(ARM64_INSN_LDAR, addr, 1, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器比较交换：Rs 提供期望值并接收内存旧值，Rt 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_ldar_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_load(ARM64_INSN_LDAR, addr, 2, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器比较交换：Rs 提供期望值并接收内存旧值，Rt 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_ldar_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_load(ARM64_INSN_LDAR, addr, 4, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器比较交换：Rs 提供期望值并接收内存旧值，Rt 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_ldar_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_ordered_load(ARM64_INSN_LDAR, addr, 8, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 单寄存器比较交换：Rs 提供期望值并接收内存旧值，Rt 提供目标值。
}

static enum emu_insn_result emu_execute_ldst_cas_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CAS, addr, 1, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_cas_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CAS, addr, 2, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_cas_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CAS, addr, 4, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_cas_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CAS, addr, 8, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casa_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASA, addr, 1, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casa_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASA, addr, 2, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casa_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASA, addr, 4, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casa_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASA, addr, 8, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASL, addr, 1, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASL, addr, 2, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASL, addr, 4, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASL, addr, 8, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casal_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASAL, addr, 1, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casal_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASAL, addr, 2, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casal_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASAL, addr, 4, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_casal_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0);
        uint64_t expected = reg_read(regs, entry->reg4);

        if (!emu_hw_cas(ARM64_INSN_CASAL, addr, 8, reg_read(regs, entry->reg2), &expected)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg4, expected, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LSE 原子读改写：Rs 提供运算源，Rt 接收修改前的内存值。
}

static enum emu_insn_result emu_execute_ldst_ldadd_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADD, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldadd_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADD, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldadd_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADD, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldadd_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADD, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldadda_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDA, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldadda_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDA, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldadda_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDA, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldadda_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDA, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldaddl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldaddl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldaddl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldaddl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldaddal_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDAL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldaddal_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDAL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldaddal_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDAL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldaddal_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDADDAL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclr_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLR, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclr_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLR, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclr_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLR, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclr_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLR, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclra_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRA, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclra_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRA, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclra_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRA, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclra_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRA, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclrl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclrl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclrl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclrl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclral_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRAL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclral_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRAL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclral_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRAL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldclral_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDCLRAL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeor_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEOR, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeor_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEOR, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeor_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEOR, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeor_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEOR, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeora_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORA, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeora_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORA, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeora_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORA, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeora_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORA, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeorl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeorl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeorl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeorl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeoral_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORAL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeoral_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORAL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeoral_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORAL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldeoral_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDEORAL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldset_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSET, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldset_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSET, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldset_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSET, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldset_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSET, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldseta_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETA, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldseta_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETA, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldseta_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETA, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldseta_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETA, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsetl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsetl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsetl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsetl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsetal_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETAL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsetal_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETAL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsetal_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETAL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsetal_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSETAL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmax_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAX, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmax_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAX, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmax_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAX, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmax_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAX, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxa_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXA, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxa_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXA, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxa_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXA, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxa_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXA, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxal_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXAL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxal_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXAL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxal_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXAL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmaxal_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMAXAL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmin_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMIN, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmin_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMIN, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmin_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMIN, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmin_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMIN, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmina_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINA, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmina_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINA, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmina_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINA, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsmina_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINA, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsminl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsminl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsminl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsminl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsminal_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINAL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsminal_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINAL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsminal_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINAL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldsminal_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDSMINAL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumax_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAX, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumax_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAX, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumax_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAX, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumax_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAX, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxa_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXA, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxa_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXA, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxa_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXA, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxa_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXA, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxal_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXAL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxal_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXAL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxal_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXAL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumaxal_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMAXAL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumin_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMIN, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumin_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMIN, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumin_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMIN, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumin_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMIN, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumina_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINA, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumina_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINA, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumina_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINA, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldumina_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINA, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_lduminl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_lduminl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_lduminl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_lduminl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_lduminal_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINAL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_lduminal_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINAL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_lduminal_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINAL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_lduminal_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_LDUMINAL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swp_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWP, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swp_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWP, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swp_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWP, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swp_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWP, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpa_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPA, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpa_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPA, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpa_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPA, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpa_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPA, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpl_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpl_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpl_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpl_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpal_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPAL, addr, 1, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpal_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPAL, addr, 2, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpal_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPAL, addr, 4, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_swpal_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t old;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_atomic_rmw(ARM64_INSN_SWPAL, addr, 8, reg_read(regs, entry->reg4), &old)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, old, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // LDAPR 执行 RCpc acquire 加载，并将结果写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldapr_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_load_ldapr(addr, 1, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 字面量加载：有效地址相对当前指令 PC 计算，不使用 Rn。
}

static enum emu_insn_result emu_execute_ldst_ldapr_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_load_ldapr(addr, 2, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 字面量加载：有效地址相对当前指令 PC 计算，不使用 Rn。
}

static enum emu_insn_result emu_execute_ldst_ldapr_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_load_ldapr(addr, 4, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 字面量加载：有效地址相对当前指令 PC 计算，不使用 Rn。
}

static enum emu_insn_result emu_execute_ldst_ldapr_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0);

        if (!emu_hw_load_ldapr(addr, 8, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 字面量加载：有效地址相对当前指令 PC 计算，不使用 Rn。
}

static enum emu_insn_result emu_execute_ldst_ldr_literal_gpr_w32_b4_mode_literal(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, 0, &address, ARM64_MEMORY_ADDRESS_LITERAL, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_LITERAL_GPR, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 字面量加载：按 PC 相对地址直接写入目标 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldr_literal_gpr_w64_b8_mode_literal(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, 0, &address, ARM64_MEMORY_ADDRESS_LITERAL, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_LITERAL_GPR, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 字面量加载：按 PC 相对地址直接写入目标 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldrsw_literal_w64_b4_mode_literal(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, 0, &address, ARM64_MEMORY_ADDRESS_LITERAL, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDRSW_LITERAL, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 字面量加载：按 PC 相对地址直接写入目标 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldr_literal_fp_simd_b4_mode_literal(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, 0, &address, ARM64_MEMORY_ADDRESS_LITERAL, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_LITERAL_FP_SIMD, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // RCpc 非对齐有序存储：地址为 Rn 加已解码的未缩放偏移。
}

static enum emu_insn_result emu_execute_ldst_ldr_literal_fp_simd_b8_mode_literal(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, 0, &address, ARM64_MEMORY_ADDRESS_LITERAL, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_LITERAL_FP_SIMD, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // RCpc 非对齐有序存储：地址为 Rn 加已解码的未缩放偏移。
}

static enum emu_insn_result emu_execute_ldst_ldr_literal_fp_simd_b16_mode_literal(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, 0, &address, ARM64_MEMORY_ADDRESS_LITERAL, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_LITERAL_FP_SIMD, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // RCpc 非对齐有序存储：地址为 Rn 加已解码的未缩放偏移。
}

static enum emu_insn_result emu_execute_ldst_stlur_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_store_rcpc(ARM64_INSN_STLUR, addr, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // RCpc 非对齐有序加载：地址为 Rn 加未缩放偏移，结果按目标宽度写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stlur_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_store_rcpc(ARM64_INSN_STLUR, addr, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // RCpc 非对齐有序加载：地址为 Rn 加未缩放偏移，结果按目标宽度写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stlur_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_store_rcpc(ARM64_INSN_STLUR, addr, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // RCpc 非对齐有序加载：地址为 Rn 加未缩放偏移，结果按目标宽度写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_stlur_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_store_rcpc(ARM64_INSN_STLUR, addr, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // RCpc 非对齐有序加载：地址为 Rn 加未缩放偏移，结果按目标宽度写回 Rt。
}

static enum emu_insn_result emu_execute_ldst_ldapur_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_load_rcpc(ARM64_INSN_LDAPUR, addr, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldapur_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_load_rcpc(ARM64_INSN_LDAPUR, addr, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldapur_w32_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_load_rcpc(ARM64_INSN_LDAPUR, addr, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldapur_w64_b8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_load_rcpc(ARM64_INSN_LDAPUR, addr, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldapur_signed_w32_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_load_rcpc(ARM64_INSN_LDAPUR_SIGNED, addr, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldapur_signed_w64_b1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_load_rcpc(ARM64_INSN_LDAPUR_SIGNED, addr, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldapur_signed_w32_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_load_rcpc(ARM64_INSN_LDAPUR_SIGNED, addr, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldapur_signed_w64_b2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_load_rcpc(ARM64_INSN_LDAPUR_SIGNED, addr, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldapur_signed_w64_b4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t value;
        uint64_t addr = addr_reg_read(regs, entry->reg0) + entry->operand0;

        if (!emu_hw_load_rcpc(ARM64_INSN_LDAPUR_SIGNED, addr, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对加载：先完成两个内存读取和目标写回，再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldnp_gpr_w32_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDNP_GPR, address, 4, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, false);
        reg_write(regs, entry->reg3, value1, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldnp_gpr_w64_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDNP_GPR, address, 8, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, true);
        reg_write(regs, entry->reg3, value1, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldp_gpr_offset_w32_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDP_GPR_OFFSET, address, 4, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, false);
        reg_write(regs, entry->reg3, value1, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldp_gpr_offset_w64_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDP_GPR_OFFSET, address, 8, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, true);
        reg_write(regs, entry->reg3, value1, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldpsw_offset_w64_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDPSW_OFFSET, address, 4, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, true);
        reg_write(regs, entry->reg3, value1, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldp_gpr_post_index_w32_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDP_GPR_POST_INDEX, address, 4, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, false);
        reg_write(regs, entry->reg3, value1, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldp_gpr_post_index_w64_b8_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDP_GPR_POST_INDEX, address, 8, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, true);
        reg_write(regs, entry->reg3, value1, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldpsw_post_index_w64_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDPSW_POST_INDEX, address, 4, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, true);
        reg_write(regs, entry->reg3, value1, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldp_gpr_pre_index_w32_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDP_GPR_PRE_INDEX, address, 4, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, false);
        reg_write(regs, entry->reg3, value1, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldp_gpr_pre_index_w64_b8_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDP_GPR_PRE_INDEX, address, 8, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, true);
        reg_write(regs, entry->reg3, value1, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_ldpsw_pre_index_w64_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value0, value1;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_pair_gpr(ARM64_INSN_LDPSW_PRE_INDEX, address, 4, &value0, &value1)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value0, true);
        reg_write(regs, entry->reg3, value1, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // GPR 成对存储：数据来自 Rt/Rt2，成功后再提交可选的 Rn writeback。
}

static enum emu_insn_result emu_execute_ldst_stnp_gpr_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_gpr(ARM64_INSN_STNP_GPR, address, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对加载：结果直接写入两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_stnp_gpr_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_gpr(ARM64_INSN_STNP_GPR, address, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对加载：结果直接写入两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_stp_gpr_offset_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_gpr(ARM64_INSN_STP_GPR_OFFSET, address, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对加载：结果直接写入两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_stp_gpr_offset_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_gpr(ARM64_INSN_STP_GPR_OFFSET, address, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对加载：结果直接写入两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_stp_gpr_post_index_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_gpr(ARM64_INSN_STP_GPR_POST_INDEX, address, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对加载：结果直接写入两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_stp_gpr_post_index_b8_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_gpr(ARM64_INSN_STP_GPR_POST_INDEX, address, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对加载：结果直接写入两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_stp_gpr_pre_index_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_gpr(ARM64_INSN_STP_GPR_PRE_INDEX, address, 4, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对加载：结果直接写入两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_stp_gpr_pre_index_b8_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_pair_gpr(ARM64_INSN_STP_GPR_PRE_INDEX, address, 8, reg_read(regs, entry->reg2), reg_read(regs, entry->reg3))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // FP/SIMD 成对加载：结果直接写入两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldnp_fp_simd_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDNP_FP_SIMD, address, 4, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldnp_fp_simd_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDNP_FP_SIMD, address, 8, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldnp_fp_simd_b16_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDNP_FP_SIMD, address, 16, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldp_fp_simd_offset_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDP_FP_SIMD_OFFSET, address, 4, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldp_fp_simd_offset_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDP_FP_SIMD_OFFSET, address, 8, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldp_fp_simd_offset_b16_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDP_FP_SIMD_OFFSET, address, 16, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldp_fp_simd_post_index_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDP_FP_SIMD_POST_INDEX, address, 4, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldp_fp_simd_post_index_b8_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDP_FP_SIMD_POST_INDEX, address, 8, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldp_fp_simd_post_index_b16_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDP_FP_SIMD_POST_INDEX, address, 16, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldp_fp_simd_pre_index_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDP_FP_SIMD_PRE_INDEX, address, 4, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldp_fp_simd_pre_index_b8_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDP_FP_SIMD_PRE_INDEX, address, 8, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_ldp_fp_simd_pre_index_b16_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_pair_fp(ARM64_INSN_LDP_FP_SIMD_PRE_INDEX, address, 16, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // FP/SIMD 成对存储：数据来自两个 Q 寄存器的软件现场。
}

static enum emu_insn_result emu_execute_ldst_stnp_fp_simd_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STNP_FP_SIMD, address, 4, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stnp_fp_simd_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STNP_FP_SIMD, address, 8, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stnp_fp_simd_b16_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STNP_FP_SIMD, address, 16, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stp_fp_simd_offset_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STP_FP_SIMD_OFFSET, address, 4, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stp_fp_simd_offset_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STP_FP_SIMD_OFFSET, address, 8, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stp_fp_simd_offset_b16_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STP_FP_SIMD_OFFSET, address, 16, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stp_fp_simd_post_index_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STP_FP_SIMD_POST_INDEX, address, 4, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stp_fp_simd_post_index_b8_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STP_FP_SIMD_POST_INDEX, address, 8, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stp_fp_simd_post_index_b16_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STP_FP_SIMD_POST_INDEX, address, 16, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stp_fp_simd_pre_index_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STP_FP_SIMD_PRE_INDEX, address, 4, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stp_fp_simd_pre_index_b8_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STP_FP_SIMD_PRE_INDEX, address, 8, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stp_fp_simd_pre_index_b16_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_pair_fp(ARM64_INSN_STP_FP_SIMD_PRE_INDEX, address, 16, &fp_regs->q[entry->reg2], &fp_regs->q[entry->reg3])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 GPR 加载：统一解析偏移/索引寻址，加载成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldur_gpr_w32_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDUR_GPR, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldur_gpr_w32_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDUR_GPR, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldur_gpr_w32_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDUR_GPR, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldur_gpr_w64_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDUR_GPR, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldur_signed_gpr_w32_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDUR_SIGNED_GPR, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldur_signed_gpr_w64_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDUR_SIGNED_GPR, address, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldur_signed_gpr_w32_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDUR_SIGNED_GPR, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldur_signed_gpr_w64_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDUR_SIGNED_GPR, address, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldur_signed_gpr_w64_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDUR_SIGNED_GPR, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldtr_gpr_w32_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDTR_GPR, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldtr_gpr_w32_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDTR_GPR, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldtr_gpr_w32_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDTR_GPR, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldtr_gpr_w64_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDTR_GPR, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldtr_signed_gpr_w32_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDTR_SIGNED_GPR, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldtr_signed_gpr_w64_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDTR_SIGNED_GPR, address, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldtr_signed_gpr_w32_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDTR_SIGNED_GPR, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldtr_signed_gpr_w64_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDTR_SIGNED_GPR, address, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldtr_signed_gpr_w64_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDTR_SIGNED_GPR, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_post_index_w32_b1_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_POST_INDEX, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_post_index_w32_b2_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_POST_INDEX, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_post_index_w32_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_POST_INDEX, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_post_index_w64_b8_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_POST_INDEX, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_post_index_w32_b1_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_post_index_w64_b1_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX, address, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_post_index_w32_b2_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_post_index_w64_b2_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX, address, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_post_index_w64_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_pre_index_w32_b1_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_PRE_INDEX, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_pre_index_w32_b2_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_PRE_INDEX, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_pre_index_w32_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_PRE_INDEX, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_pre_index_w64_b8_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_PRE_INDEX, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_pre_index_w32_b1_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_pre_index_w64_b1_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX, address, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_pre_index_w32_b2_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_pre_index_w64_b2_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX, address, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_pre_index_w64_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b1_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b1_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b1_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b1_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b2_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b2_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b2_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b2_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b4_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b4_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b4_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w32_b4_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w64_b8_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w64_b8_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w64_b8_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_register_offset_w64_b8_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_REGISTER_OFFSET, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b1_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b1_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b1_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b1_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b1_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b1_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b1_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b1_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b2_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b2_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b2_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b2_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b2_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b2_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b2_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b2_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b4_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b4_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b4_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b4_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_unsigned_offset_w32_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_unsigned_offset_w32_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_unsigned_offset_w32_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET, address, 4, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_gpr_unsigned_offset_w64_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET, address, 8, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w32_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET, address, 1, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w64_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET, address, 1, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w32_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET, address, 2, false, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, false);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w64_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET, address, 2, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w64_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t value;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_load_gpr(ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET, address, 4, true, &value)) return EMU_INSN_SKIP;
        reg_write(regs, entry->reg2, value, true);
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 GPR 存储：从 Rt 取值，存储成功后执行可选 writeback。
}

static enum emu_insn_result emu_execute_ldst_stur_gpr_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STUR_GPR, address, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_stur_gpr_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STUR_GPR, address, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_stur_gpr_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STUR_GPR, address, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_stur_gpr_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STUR_GPR, address, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_sttr_gpr_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STTR_GPR, address, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_sttr_gpr_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STTR_GPR, address, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_sttr_gpr_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STTR_GPR, address, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_sttr_gpr_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STTR_GPR, address, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_post_index_b1_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_POST_INDEX, address, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_post_index_b2_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_POST_INDEX, address, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_post_index_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_POST_INDEX, address, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_post_index_b8_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_POST_INDEX, address, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_pre_index_b1_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_PRE_INDEX, address, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_pre_index_b2_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_PRE_INDEX, address, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_pre_index_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_PRE_INDEX, address, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_pre_index_b8_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_PRE_INDEX, address, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b1_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b1_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b1_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b1_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b2_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b2_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b2_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b2_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b4_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b4_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b4_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b4_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b8_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b8_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b8_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_register_offset_b8_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_REGISTER_OFFSET, address, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_unsigned_offset_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_UNSIGNED_OFFSET, address, 1, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_unsigned_offset_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_UNSIGNED_OFFSET, address, 2, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_unsigned_offset_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_UNSIGNED_OFFSET, address, 4, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_str_gpr_unsigned_offset_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;

    {
        uint64_t address;
        uint64_t base = addr_reg_read(regs, entry->reg0);

        if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
        if (!emu_hw_store_gpr(ARM64_INSN_STR_GPR_UNSIGNED_OFFSET, address, 8, reg_read(regs, entry->reg2))) return EMU_INSN_SKIP;
        emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

        regs->pc += 4;
        return EMU_INSN_HANDLED;
    }
    // 普通 FP/SIMD 加载：目标是 Q 寄存器软件现场中的对应低位元素。
}

static enum emu_insn_result emu_execute_ldst_ldur_fp_simd_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDUR_FP_SIMD, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldur_fp_simd_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDUR_FP_SIMD, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldur_fp_simd_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDUR_FP_SIMD, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldur_fp_simd_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDUR_FP_SIMD, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldur_fp_simd_b16_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDUR_FP_SIMD, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_post_index_b1_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_POST_INDEX, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_post_index_b2_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_POST_INDEX, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_post_index_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_POST_INDEX, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_post_index_b8_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_POST_INDEX, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_post_index_b16_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_POST_INDEX, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_pre_index_b1_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_PRE_INDEX, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_pre_index_b2_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_PRE_INDEX, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_pre_index_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_PRE_INDEX, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_pre_index_b8_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_PRE_INDEX, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_pre_index_b16_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_PRE_INDEX, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b1_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b1_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b1_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b1_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b2_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b2_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b2_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b2_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b4_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b4_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b4_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b4_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b8_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b8_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b8_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b8_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b16_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b16_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b16_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_register_offset_b16_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_unsigned_offset_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_unsigned_offset_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_unsigned_offset_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_unsigned_offset_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_ldr_fp_simd_unsigned_offset_b16_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_load_fp(ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 普通 FP/SIMD 存储：从 Q 寄存器软件现场读取指定宽度的数据。
}

static enum emu_insn_result emu_execute_ldst_stur_fp_simd_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STUR_FP_SIMD, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_stur_fp_simd_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STUR_FP_SIMD, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_stur_fp_simd_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STUR_FP_SIMD, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_stur_fp_simd_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STUR_FP_SIMD, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_stur_fp_simd_b16_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STUR_FP_SIMD, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_post_index_b1_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_POST_INDEX, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_post_index_b2_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_POST_INDEX, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_post_index_b4_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_POST_INDEX, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_post_index_b8_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_POST_INDEX, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_post_index_b16_mode_post_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_POST_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_POST_INDEX, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_POST_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_pre_index_b1_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_PRE_INDEX, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_pre_index_b2_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_PRE_INDEX, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_pre_index_b4_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_PRE_INDEX, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_pre_index_b8_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_PRE_INDEX, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_pre_index_b16_mode_pre_index(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_PRE_INDEX, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_PRE_INDEX, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_PRE_INDEX);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b1_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b1_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b1_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b1_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b2_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b2_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b2_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b2_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b4_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b4_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b4_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b4_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b8_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b8_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b8_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b8_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b16_mode_register_offset_ext2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 2)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b16_mode_register_offset_ext3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 3)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b16_mode_register_offset_ext6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 6)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_register_offset_b16_mode_register_offset_ext7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET, 7)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_REGISTER_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_unsigned_offset_b1_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET, address, 1, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_unsigned_offset_b2_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET, address, 2, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_unsigned_offset_b4_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET, address, 4, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_unsigned_offset_b8_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET, address, 8, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_str_fp_simd_unsigned_offset_b16_mode_base_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t address;
    uint64_t base = addr_reg_read(regs, entry->reg0);

    if (!emu_resolve_memory_address_entry(entry, regs, regs->pc, base, &address, ARM64_MEMORY_ADDRESS_BASE_OFFSET, 0)) return EMU_INSN_SKIP;
    if (!emu_hw_store_fp(ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET, address, 16, &fp_regs->q[entry->reg2])) return EMU_INSN_SKIP;
    emu_commit_memory_writeback_entry(entry, regs, base, ARM64_MEMORY_ADDRESS_BASE_OFFSET);

    regs->pc += 4;
    return EMU_INSN_HANDLED;
    // 预取是无架构可见结果的性能提示；无需实际访存即可视为已处理。
}

static enum emu_insn_result emu_execute_ldst_prfm_literal(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;

        regs->pc += 4;
        return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_ldst_prfum(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;

        regs->pc += 4;
        return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_ldst_prfm_register_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;

        regs->pc += 4;
        return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_ldst_prfm_unsigned_offset(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    (void)fp_regs;
    (void)entry;

        regs->pc += 4;
        return EMU_INSN_HANDLED;
}



static enum emu_insn_result (*emu_select_ldst_executor(const struct arm64_decoded_instruction *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)

{

    switch (decoded->instruction)

    {

    case ARM64_INSN_LDXR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldxr_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldxr_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldxr_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldxr_w64_b8;

        return NULL;

    case ARM64_INSN_LDAXR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldaxr_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldaxr_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldaxr_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldaxr_w64_b8;

        return NULL;

    case ARM64_INSN_LDXP:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldxp_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldxp_w64_b8;

        return NULL;

    case ARM64_INSN_LDAXP:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldaxp_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldaxp_w64_b8;

        return NULL;

    case ARM64_INSN_STXR:

        if (decoded->access_bytes == 1) return emu_execute_ldst_stxr_b1;

        if (decoded->access_bytes == 2) return emu_execute_ldst_stxr_b2;

        if (decoded->access_bytes == 4) return emu_execute_ldst_stxr_b4;

        if (decoded->access_bytes == 8) return emu_execute_ldst_stxr_b8;

        return NULL;

    case ARM64_INSN_STLXR:

        if (decoded->access_bytes == 1) return emu_execute_ldst_stlxr_b1;

        if (decoded->access_bytes == 2) return emu_execute_ldst_stlxr_b2;

        if (decoded->access_bytes == 4) return emu_execute_ldst_stlxr_b4;

        if (decoded->access_bytes == 8) return emu_execute_ldst_stlxr_b8;

        return NULL;

    case ARM64_INSN_STXP:

        if (decoded->access_bytes == 4) return emu_execute_ldst_stxp_b4;

        if (decoded->access_bytes == 8) return emu_execute_ldst_stxp_b8;

        return NULL;

    case ARM64_INSN_STLXP:

        if (decoded->access_bytes == 4) return emu_execute_ldst_stlxp_b4;

        if (decoded->access_bytes == 8) return emu_execute_ldst_stlxp_b8;

        return NULL;

    case ARM64_INSN_CASP:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_casp_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_casp_w64_b8;

        return NULL;

    case ARM64_INSN_CASPA:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_caspa_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_caspa_w64_b8;

        return NULL;

    case ARM64_INSN_CASPL:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_caspl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_caspl_w64_b8;

        return NULL;

    case ARM64_INSN_CASPAL:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_caspal_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_caspal_w64_b8;

        return NULL;

    case ARM64_INSN_STLLR:

        if (decoded->access_bytes == 1) return emu_execute_ldst_stllr_b1;

        if (decoded->access_bytes == 2) return emu_execute_ldst_stllr_b2;

        if (decoded->access_bytes == 4) return emu_execute_ldst_stllr_b4;

        if (decoded->access_bytes == 8) return emu_execute_ldst_stllr_b8;

        return NULL;

    case ARM64_INSN_STLR:

        if (decoded->access_bytes == 1) return emu_execute_ldst_stlr_b1;

        if (decoded->access_bytes == 2) return emu_execute_ldst_stlr_b2;

        if (decoded->access_bytes == 4) return emu_execute_ldst_stlr_b4;

        if (decoded->access_bytes == 8) return emu_execute_ldst_stlr_b8;

        return NULL;

    case ARM64_INSN_LDLAR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldlar_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldlar_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldlar_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldlar_w64_b8;

        return NULL;

    case ARM64_INSN_LDAR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldar_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldar_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldar_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldar_w64_b8;

        return NULL;

    case ARM64_INSN_CAS:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_cas_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_cas_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_cas_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_cas_w64_b8;

        return NULL;

    case ARM64_INSN_CASA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_casa_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_casa_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_casa_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_casa_w64_b8;

        return NULL;

    case ARM64_INSN_CASL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_casl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_casl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_casl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_casl_w64_b8;

        return NULL;

    case ARM64_INSN_CASAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_casal_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_casal_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_casal_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_casal_w64_b8;

        return NULL;

    case ARM64_INSN_LDADD:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldadd_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldadd_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldadd_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldadd_w64_b8;

        return NULL;

    case ARM64_INSN_LDADDA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldadda_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldadda_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldadda_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldadda_w64_b8;

        return NULL;

    case ARM64_INSN_LDADDL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldaddl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldaddl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldaddl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldaddl_w64_b8;

        return NULL;

    case ARM64_INSN_LDADDAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldaddal_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldaddal_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldaddal_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldaddal_w64_b8;

        return NULL;

    case ARM64_INSN_LDCLR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldclr_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldclr_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldclr_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldclr_w64_b8;

        return NULL;

    case ARM64_INSN_LDCLRA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldclra_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldclra_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldclra_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldclra_w64_b8;

        return NULL;

    case ARM64_INSN_LDCLRL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldclrl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldclrl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldclrl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldclrl_w64_b8;

        return NULL;

    case ARM64_INSN_LDCLRAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldclral_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldclral_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldclral_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldclral_w64_b8;

        return NULL;

    case ARM64_INSN_LDEOR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldeor_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldeor_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldeor_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldeor_w64_b8;

        return NULL;

    case ARM64_INSN_LDEORA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldeora_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldeora_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldeora_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldeora_w64_b8;

        return NULL;

    case ARM64_INSN_LDEORL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldeorl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldeorl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldeorl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldeorl_w64_b8;

        return NULL;

    case ARM64_INSN_LDEORAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldeoral_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldeoral_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldeoral_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldeoral_w64_b8;

        return NULL;

    case ARM64_INSN_LDSET:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldset_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldset_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldset_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldset_w64_b8;

        return NULL;

    case ARM64_INSN_LDSETA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldseta_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldseta_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldseta_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldseta_w64_b8;

        return NULL;

    case ARM64_INSN_LDSETL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsetl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsetl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsetl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsetl_w64_b8;

        return NULL;

    case ARM64_INSN_LDSETAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsetal_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsetal_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsetal_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsetal_w64_b8;

        return NULL;

    case ARM64_INSN_LDSMAX:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsmax_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsmax_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsmax_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsmax_w64_b8;

        return NULL;

    case ARM64_INSN_LDSMAXA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsmaxa_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsmaxa_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsmaxa_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsmaxa_w64_b8;

        return NULL;

    case ARM64_INSN_LDSMAXL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsmaxl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsmaxl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsmaxl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsmaxl_w64_b8;

        return NULL;

    case ARM64_INSN_LDSMAXAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsmaxal_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsmaxal_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsmaxal_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsmaxal_w64_b8;

        return NULL;

    case ARM64_INSN_LDSMIN:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsmin_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsmin_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsmin_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsmin_w64_b8;

        return NULL;

    case ARM64_INSN_LDSMINA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsmina_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsmina_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsmina_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsmina_w64_b8;

        return NULL;

    case ARM64_INSN_LDSMINL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsminl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsminl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsminl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsminl_w64_b8;

        return NULL;

    case ARM64_INSN_LDSMINAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldsminal_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldsminal_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldsminal_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldsminal_w64_b8;

        return NULL;

    case ARM64_INSN_LDUMAX:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldumax_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldumax_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldumax_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldumax_w64_b8;

        return NULL;

    case ARM64_INSN_LDUMAXA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldumaxa_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldumaxa_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldumaxa_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldumaxa_w64_b8;

        return NULL;

    case ARM64_INSN_LDUMAXL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldumaxl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldumaxl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldumaxl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldumaxl_w64_b8;

        return NULL;

    case ARM64_INSN_LDUMAXAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldumaxal_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldumaxal_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldumaxal_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldumaxal_w64_b8;

        return NULL;

    case ARM64_INSN_LDUMIN:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldumin_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldumin_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldumin_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldumin_w64_b8;

        return NULL;

    case ARM64_INSN_LDUMINA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldumina_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldumina_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldumina_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldumina_w64_b8;

        return NULL;

    case ARM64_INSN_LDUMINL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_lduminl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_lduminl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_lduminl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_lduminl_w64_b8;

        return NULL;

    case ARM64_INSN_LDUMINAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_lduminal_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_lduminal_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_lduminal_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_lduminal_w64_b8;

        return NULL;

    case ARM64_INSN_SWP:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_swp_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_swp_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_swp_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_swp_w64_b8;

        return NULL;

    case ARM64_INSN_SWPA:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_swpa_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_swpa_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_swpa_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_swpa_w64_b8;

        return NULL;

    case ARM64_INSN_SWPL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_swpl_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_swpl_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_swpl_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_swpl_w64_b8;

        return NULL;

    case ARM64_INSN_SWPAL:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_swpal_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_swpal_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_swpal_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_swpal_w64_b8;

        return NULL;

    case ARM64_INSN_LDAPR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldapr_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldapr_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldapr_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldapr_w64_b8;

        return NULL;

    case ARM64_INSN_LDR_LITERAL_GPR:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL) return emu_execute_ldst_ldr_literal_gpr_w32_b4_mode_literal;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL) return emu_execute_ldst_ldr_literal_gpr_w64_b8_mode_literal;

        return NULL;

    case ARM64_INSN_LDRSW_LITERAL:

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL) return emu_execute_ldst_ldrsw_literal_w64_b4_mode_literal;

        return NULL;

    case ARM64_INSN_LDR_LITERAL_FP_SIMD:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL) return emu_execute_ldst_ldr_literal_fp_simd_b4_mode_literal;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL) return emu_execute_ldst_ldr_literal_fp_simd_b8_mode_literal;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_LITERAL) return emu_execute_ldst_ldr_literal_fp_simd_b16_mode_literal;

        return NULL;

    case ARM64_INSN_STLUR:

        if (decoded->access_bytes == 1) return emu_execute_ldst_stlur_b1;

        if (decoded->access_bytes == 2) return emu_execute_ldst_stlur_b2;

        if (decoded->access_bytes == 4) return emu_execute_ldst_stlur_b4;

        if (decoded->access_bytes == 8) return emu_execute_ldst_stlur_b8;

        return NULL;

    case ARM64_INSN_LDAPUR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldapur_w32_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldapur_w32_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32) return emu_execute_ldst_ldapur_w32_b4;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64) return emu_execute_ldst_ldapur_w64_b8;

        return NULL;

    case ARM64_INSN_LDAPUR_SIGNED:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32) return emu_execute_ldst_ldapur_signed_w32_b1;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64) return emu_execute_ldst_ldapur_signed_w64_b1;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32) return emu_execute_ldst_ldapur_signed_w32_b2;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64) return emu_execute_ldst_ldapur_signed_w64_b2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64) return emu_execute_ldst_ldapur_signed_w64_b4;

        return NULL;

    case ARM64_INSN_LDNP_GPR:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldnp_gpr_w32_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldnp_gpr_w64_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDP_GPR_OFFSET:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldp_gpr_offset_w32_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldp_gpr_offset_w64_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDPSW_OFFSET:

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldpsw_offset_w64_b4_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDP_GPR_POST_INDEX:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldp_gpr_post_index_w32_b4_mode_post_index;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldp_gpr_post_index_w64_b8_mode_post_index;

        return NULL;

    case ARM64_INSN_LDPSW_POST_INDEX:

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldpsw_post_index_w64_b4_mode_post_index;

        return NULL;

    case ARM64_INSN_LDP_GPR_PRE_INDEX:

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldp_gpr_pre_index_w32_b4_mode_pre_index;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldp_gpr_pre_index_w64_b8_mode_pre_index;

        return NULL;

    case ARM64_INSN_LDPSW_PRE_INDEX:

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldpsw_pre_index_w64_b4_mode_pre_index;

        return NULL;

    case ARM64_INSN_STNP_GPR:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stnp_gpr_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stnp_gpr_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_STP_GPR_OFFSET:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stp_gpr_offset_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stp_gpr_offset_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_STP_GPR_POST_INDEX:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_stp_gpr_post_index_b4_mode_post_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_stp_gpr_post_index_b8_mode_post_index;

        return NULL;

    case ARM64_INSN_STP_GPR_PRE_INDEX:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_stp_gpr_pre_index_b4_mode_pre_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_stp_gpr_pre_index_b8_mode_pre_index;

        return NULL;

    case ARM64_INSN_LDNP_FP_SIMD:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldnp_fp_simd_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldnp_fp_simd_b8_mode_base_offset;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldnp_fp_simd_b16_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDP_FP_SIMD_OFFSET:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldp_fp_simd_offset_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldp_fp_simd_offset_b8_mode_base_offset;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldp_fp_simd_offset_b16_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDP_FP_SIMD_POST_INDEX:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldp_fp_simd_post_index_b4_mode_post_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldp_fp_simd_post_index_b8_mode_post_index;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldp_fp_simd_post_index_b16_mode_post_index;

        return NULL;

    case ARM64_INSN_LDP_FP_SIMD_PRE_INDEX:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldp_fp_simd_pre_index_b4_mode_pre_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldp_fp_simd_pre_index_b8_mode_pre_index;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldp_fp_simd_pre_index_b16_mode_pre_index;

        return NULL;

    case ARM64_INSN_STNP_FP_SIMD:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stnp_fp_simd_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stnp_fp_simd_b8_mode_base_offset;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stnp_fp_simd_b16_mode_base_offset;

        return NULL;

    case ARM64_INSN_STP_FP_SIMD_OFFSET:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stp_fp_simd_offset_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stp_fp_simd_offset_b8_mode_base_offset;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stp_fp_simd_offset_b16_mode_base_offset;

        return NULL;

    case ARM64_INSN_STP_FP_SIMD_POST_INDEX:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_stp_fp_simd_post_index_b4_mode_post_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_stp_fp_simd_post_index_b8_mode_post_index;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_stp_fp_simd_post_index_b16_mode_post_index;

        return NULL;

    case ARM64_INSN_STP_FP_SIMD_PRE_INDEX:

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_stp_fp_simd_pre_index_b4_mode_pre_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_stp_fp_simd_pre_index_b8_mode_pre_index;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_stp_fp_simd_pre_index_b16_mode_pre_index;

        return NULL;

    case ARM64_INSN_LDUR_GPR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_gpr_w32_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_gpr_w32_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_gpr_w32_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_gpr_w64_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDUR_SIGNED_GPR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_signed_gpr_w32_b1_mode_base_offset;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_signed_gpr_w64_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_signed_gpr_w32_b2_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_signed_gpr_w64_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_signed_gpr_w64_b4_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDTR_GPR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldtr_gpr_w32_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldtr_gpr_w32_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldtr_gpr_w32_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldtr_gpr_w64_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDTR_SIGNED_GPR:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldtr_signed_gpr_w32_b1_mode_base_offset;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldtr_signed_gpr_w64_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldtr_signed_gpr_w32_b2_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldtr_signed_gpr_w64_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldtr_signed_gpr_w64_b4_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDR_GPR_POST_INDEX:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_gpr_post_index_w32_b1_mode_post_index;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_gpr_post_index_w32_b2_mode_post_index;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_gpr_post_index_w32_b4_mode_post_index;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_gpr_post_index_w64_b8_mode_post_index;

        return NULL;

    case ARM64_INSN_LDR_SIGNED_GPR_POST_INDEX:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_signed_gpr_post_index_w32_b1_mode_post_index;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_signed_gpr_post_index_w64_b1_mode_post_index;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_signed_gpr_post_index_w32_b2_mode_post_index;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_signed_gpr_post_index_w64_b2_mode_post_index;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_signed_gpr_post_index_w64_b4_mode_post_index;

        return NULL;

    case ARM64_INSN_LDR_GPR_PRE_INDEX:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_gpr_pre_index_w32_b1_mode_pre_index;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_gpr_pre_index_w32_b2_mode_pre_index;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_gpr_pre_index_w32_b4_mode_pre_index;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_gpr_pre_index_w64_b8_mode_pre_index;

        return NULL;

    case ARM64_INSN_LDR_SIGNED_GPR_PRE_INDEX:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_signed_gpr_pre_index_w32_b1_mode_pre_index;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_signed_gpr_pre_index_w64_b1_mode_pre_index;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_signed_gpr_pre_index_w32_b2_mode_pre_index;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_signed_gpr_pre_index_w64_b2_mode_pre_index;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_signed_gpr_pre_index_w64_b4_mode_pre_index;

        return NULL;

    case ARM64_INSN_LDR_GPR_REGISTER_OFFSET:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_gpr_register_offset_w32_b1_mode_register_offset_ext2;

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_gpr_register_offset_w32_b1_mode_register_offset_ext3;

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_gpr_register_offset_w32_b1_mode_register_offset_ext6;

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_gpr_register_offset_w32_b1_mode_register_offset_ext7;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_gpr_register_offset_w32_b2_mode_register_offset_ext2;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_gpr_register_offset_w32_b2_mode_register_offset_ext3;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_gpr_register_offset_w32_b2_mode_register_offset_ext6;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_gpr_register_offset_w32_b2_mode_register_offset_ext7;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_gpr_register_offset_w32_b4_mode_register_offset_ext2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_gpr_register_offset_w32_b4_mode_register_offset_ext3;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_gpr_register_offset_w32_b4_mode_register_offset_ext6;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_gpr_register_offset_w32_b4_mode_register_offset_ext7;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_gpr_register_offset_w64_b8_mode_register_offset_ext2;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_gpr_register_offset_w64_b8_mode_register_offset_ext3;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_gpr_register_offset_w64_b8_mode_register_offset_ext6;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_gpr_register_offset_w64_b8_mode_register_offset_ext7;

        return NULL;

    case ARM64_INSN_LDR_SIGNED_GPR_REGISTER_OFFSET:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b1_mode_register_offset_ext2;

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b1_mode_register_offset_ext3;

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b1_mode_register_offset_ext6;

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b1_mode_register_offset_ext7;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b1_mode_register_offset_ext2;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b1_mode_register_offset_ext3;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b1_mode_register_offset_ext6;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b1_mode_register_offset_ext7;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b2_mode_register_offset_ext2;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b2_mode_register_offset_ext3;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b2_mode_register_offset_ext6;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_signed_gpr_register_offset_w32_b2_mode_register_offset_ext7;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b2_mode_register_offset_ext2;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b2_mode_register_offset_ext3;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b2_mode_register_offset_ext6;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b2_mode_register_offset_ext7;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b4_mode_register_offset_ext2;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b4_mode_register_offset_ext3;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b4_mode_register_offset_ext6;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_signed_gpr_register_offset_w64_b4_mode_register_offset_ext7;

        return NULL;

    case ARM64_INSN_LDR_GPR_UNSIGNED_OFFSET:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_gpr_unsigned_offset_w32_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_gpr_unsigned_offset_w32_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_gpr_unsigned_offset_w32_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_gpr_unsigned_offset_w64_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDR_SIGNED_GPR_UNSIGNED_OFFSET:

        if (decoded->access_bytes == 1 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w32_b1_mode_base_offset;

        if (decoded->access_bytes == 1 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w64_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->operand_width == 32 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w32_b2_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w64_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->operand_width == 64 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_signed_gpr_unsigned_offset_w64_b4_mode_base_offset;

        return NULL;

    case ARM64_INSN_STUR_GPR:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stur_gpr_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stur_gpr_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stur_gpr_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stur_gpr_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_STTR_GPR:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_sttr_gpr_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_sttr_gpr_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_sttr_gpr_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_sttr_gpr_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_STR_GPR_POST_INDEX:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_str_gpr_post_index_b1_mode_post_index;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_str_gpr_post_index_b2_mode_post_index;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_str_gpr_post_index_b4_mode_post_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_str_gpr_post_index_b8_mode_post_index;

        return NULL;

    case ARM64_INSN_STR_GPR_PRE_INDEX:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_str_gpr_pre_index_b1_mode_pre_index;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_str_gpr_pre_index_b2_mode_pre_index;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_str_gpr_pre_index_b4_mode_pre_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_str_gpr_pre_index_b8_mode_pre_index;

        return NULL;

    case ARM64_INSN_STR_GPR_REGISTER_OFFSET:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_str_gpr_register_offset_b1_mode_register_offset_ext2;

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_str_gpr_register_offset_b1_mode_register_offset_ext3;

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_str_gpr_register_offset_b1_mode_register_offset_ext6;

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_str_gpr_register_offset_b1_mode_register_offset_ext7;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_str_gpr_register_offset_b2_mode_register_offset_ext2;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_str_gpr_register_offset_b2_mode_register_offset_ext3;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_str_gpr_register_offset_b2_mode_register_offset_ext6;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_str_gpr_register_offset_b2_mode_register_offset_ext7;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_str_gpr_register_offset_b4_mode_register_offset_ext2;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_str_gpr_register_offset_b4_mode_register_offset_ext3;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_str_gpr_register_offset_b4_mode_register_offset_ext6;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_str_gpr_register_offset_b4_mode_register_offset_ext7;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_str_gpr_register_offset_b8_mode_register_offset_ext2;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_str_gpr_register_offset_b8_mode_register_offset_ext3;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_str_gpr_register_offset_b8_mode_register_offset_ext6;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_str_gpr_register_offset_b8_mode_register_offset_ext7;

        return NULL;

    case ARM64_INSN_STR_GPR_UNSIGNED_OFFSET:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_str_gpr_unsigned_offset_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_str_gpr_unsigned_offset_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_str_gpr_unsigned_offset_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_str_gpr_unsigned_offset_b8_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDUR_FP_SIMD:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_fp_simd_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_fp_simd_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_fp_simd_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_fp_simd_b8_mode_base_offset;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldur_fp_simd_b16_mode_base_offset;

        return NULL;

    case ARM64_INSN_LDR_FP_SIMD_POST_INDEX:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_fp_simd_post_index_b1_mode_post_index;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_fp_simd_post_index_b2_mode_post_index;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_fp_simd_post_index_b4_mode_post_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_fp_simd_post_index_b8_mode_post_index;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_ldr_fp_simd_post_index_b16_mode_post_index;

        return NULL;

    case ARM64_INSN_LDR_FP_SIMD_PRE_INDEX:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_fp_simd_pre_index_b1_mode_pre_index;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_fp_simd_pre_index_b2_mode_pre_index;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_fp_simd_pre_index_b4_mode_pre_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_fp_simd_pre_index_b8_mode_pre_index;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_ldr_fp_simd_pre_index_b16_mode_pre_index;

        return NULL;

    case ARM64_INSN_LDR_FP_SIMD_REGISTER_OFFSET:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_fp_simd_register_offset_b1_mode_register_offset_ext2;

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_fp_simd_register_offset_b1_mode_register_offset_ext3;

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_fp_simd_register_offset_b1_mode_register_offset_ext6;

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_fp_simd_register_offset_b1_mode_register_offset_ext7;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_fp_simd_register_offset_b2_mode_register_offset_ext2;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_fp_simd_register_offset_b2_mode_register_offset_ext3;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_fp_simd_register_offset_b2_mode_register_offset_ext6;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_fp_simd_register_offset_b2_mode_register_offset_ext7;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_fp_simd_register_offset_b4_mode_register_offset_ext2;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_fp_simd_register_offset_b4_mode_register_offset_ext3;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_fp_simd_register_offset_b4_mode_register_offset_ext6;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_fp_simd_register_offset_b4_mode_register_offset_ext7;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_fp_simd_register_offset_b8_mode_register_offset_ext2;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_fp_simd_register_offset_b8_mode_register_offset_ext3;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_fp_simd_register_offset_b8_mode_register_offset_ext6;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_fp_simd_register_offset_b8_mode_register_offset_ext7;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_ldr_fp_simd_register_offset_b16_mode_register_offset_ext2;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_ldr_fp_simd_register_offset_b16_mode_register_offset_ext3;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_ldr_fp_simd_register_offset_b16_mode_register_offset_ext6;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_ldr_fp_simd_register_offset_b16_mode_register_offset_ext7;

        return NULL;

    case ARM64_INSN_LDR_FP_SIMD_UNSIGNED_OFFSET:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_fp_simd_unsigned_offset_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_fp_simd_unsigned_offset_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_fp_simd_unsigned_offset_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_fp_simd_unsigned_offset_b8_mode_base_offset;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_ldr_fp_simd_unsigned_offset_b16_mode_base_offset;

        return NULL;

    case ARM64_INSN_STUR_FP_SIMD:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stur_fp_simd_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stur_fp_simd_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stur_fp_simd_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stur_fp_simd_b8_mode_base_offset;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_stur_fp_simd_b16_mode_base_offset;

        return NULL;

    case ARM64_INSN_STR_FP_SIMD_POST_INDEX:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_str_fp_simd_post_index_b1_mode_post_index;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_str_fp_simd_post_index_b2_mode_post_index;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_str_fp_simd_post_index_b4_mode_post_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_str_fp_simd_post_index_b8_mode_post_index;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_POST_INDEX) return emu_execute_ldst_str_fp_simd_post_index_b16_mode_post_index;

        return NULL;

    case ARM64_INSN_STR_FP_SIMD_PRE_INDEX:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_str_fp_simd_pre_index_b1_mode_pre_index;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_str_fp_simd_pre_index_b2_mode_pre_index;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_str_fp_simd_pre_index_b4_mode_pre_index;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_str_fp_simd_pre_index_b8_mode_pre_index;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_PRE_INDEX) return emu_execute_ldst_str_fp_simd_pre_index_b16_mode_pre_index;

        return NULL;

    case ARM64_INSN_STR_FP_SIMD_REGISTER_OFFSET:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_str_fp_simd_register_offset_b1_mode_register_offset_ext2;

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_str_fp_simd_register_offset_b1_mode_register_offset_ext3;

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_str_fp_simd_register_offset_b1_mode_register_offset_ext6;

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_str_fp_simd_register_offset_b1_mode_register_offset_ext7;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_str_fp_simd_register_offset_b2_mode_register_offset_ext2;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_str_fp_simd_register_offset_b2_mode_register_offset_ext3;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_str_fp_simd_register_offset_b2_mode_register_offset_ext6;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_str_fp_simd_register_offset_b2_mode_register_offset_ext7;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_str_fp_simd_register_offset_b4_mode_register_offset_ext2;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_str_fp_simd_register_offset_b4_mode_register_offset_ext3;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_str_fp_simd_register_offset_b4_mode_register_offset_ext6;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_str_fp_simd_register_offset_b4_mode_register_offset_ext7;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_str_fp_simd_register_offset_b8_mode_register_offset_ext2;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_str_fp_simd_register_offset_b8_mode_register_offset_ext3;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_str_fp_simd_register_offset_b8_mode_register_offset_ext6;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_str_fp_simd_register_offset_b8_mode_register_offset_ext7;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 2) return emu_execute_ldst_str_fp_simd_register_offset_b16_mode_register_offset_ext2;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 3) return emu_execute_ldst_str_fp_simd_register_offset_b16_mode_register_offset_ext3;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 6) return emu_execute_ldst_str_fp_simd_register_offset_b16_mode_register_offset_ext6;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_REGISTER_OFFSET && decoded->extend_type == 7) return emu_execute_ldst_str_fp_simd_register_offset_b16_mode_register_offset_ext7;

        return NULL;

    case ARM64_INSN_STR_FP_SIMD_UNSIGNED_OFFSET:

        if (decoded->access_bytes == 1 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_str_fp_simd_unsigned_offset_b1_mode_base_offset;

        if (decoded->access_bytes == 2 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_str_fp_simd_unsigned_offset_b2_mode_base_offset;

        if (decoded->access_bytes == 4 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_str_fp_simd_unsigned_offset_b4_mode_base_offset;

        if (decoded->access_bytes == 8 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_str_fp_simd_unsigned_offset_b8_mode_base_offset;

        if (decoded->access_bytes == 16 && decoded->memory_address_mode == ARM64_MEMORY_ADDRESS_BASE_OFFSET) return emu_execute_ldst_str_fp_simd_unsigned_offset_b16_mode_base_offset;

        return NULL;

    case ARM64_INSN_PRFM_LITERAL:

        return emu_execute_ldst_prfm_literal;

    case ARM64_INSN_PRFUM:

        return emu_execute_ldst_prfum;

    case ARM64_INSN_PRFM_REGISTER_OFFSET:

        return emu_execute_ldst_prfm_register_offset;

    case ARM64_INSN_PRFM_UNSIGNED_OFFSET:

        return emu_execute_ldst_prfm_unsigned_offset;

    default:

        return NULL;

    }

}

/* ======================== 访存类：解码结果构建缓存条目 ======================== */

bool emu_build_ldst_executor(const struct arm64_decoded_instruction *decoded, struct arm64_executor_entry *entry)
{
    entry->execute = emu_select_ldst_executor(decoded);
    if (!entry->execute) return false;

    entry->operand0 = decoded->offset;
    entry->operand1 = (uint64_t)(uint32_t)decoded->instruction |
                      ((uint64_t)decoded->memory_address_mode << 32) |
                      ((uint64_t)decoded->extend_type << 35) |
                      ((uint64_t)decoded->shift_amount << 38);
    entry->reg0 = decoded->rn;
    entry->reg1 = decoded->rm;
    entry->reg2 = decoded->rt;
    entry->reg3 = decoded->rt2;
    entry->reg4 = decoded->rs;
    entry->reg5 = decoded->access_bytes;
    entry->option0 = decoded->operand_width;
    return true;
}
