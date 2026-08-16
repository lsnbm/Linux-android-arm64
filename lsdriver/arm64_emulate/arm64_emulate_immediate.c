#include "arm64_emulate_internal.h"

/* ======================== 数据处理立即数类：专属硬件语义辅助 ======================== */

static inline uint64_t emu_sign_extend_byte_hw(uint64_t value)
{
    return emu_template_sxtb_shift(value, 0);
}

static inline uint64_t emu_extract_bits(uint64_t high, uint64_t low, uint32_t shift, bool sf)
{
    return sf ? emu_template_extract_w64(high, low, shift) : emu_template_extract_w32(high, low, shift);
}

// clang-format off
static inline bool emu_bitfield_hw(enum arm64_instruction instruction, uint64_t src, uint64_t dst, uint32_t immr, uint64_t wmask, uint64_t tmask, bool sf, uint64_t *result)
{
    if (!result) return false;

    switch (instruction)
    {
    case ARM64_INSN_SBFM:
        *result = sf ? emu_template_sbfm_w64(src, dst, immr, wmask, tmask) : emu_template_sbfm_w32(src, dst, immr, wmask, tmask);
        return true;
    case ARM64_INSN_BFM:
        *result = sf ? emu_template_bfm_w64(src, dst, immr, wmask, tmask) : emu_template_bfm_w32(src, dst, immr, wmask, tmask);
        return true;
    case ARM64_INSN_UBFM:
        *result = sf ? emu_template_ubfm_w64(src, dst, immr, wmask, tmask) : emu_template_ubfm_w32(src, dst, immr, wmask, tmask);
        return true;
    default:
        return false;
    }
}

static inline bool emu_move_wide_hw(enum arm64_instruction instruction, uint64_t dst, uint64_t immediate, uint32_t shift, bool sf, uint64_t *result)
{
    if (!result) return false;

    switch (instruction)
    {
    case ARM64_INSN_MOVN:
        *result = sf ? emu_template_movn_w64(dst, immediate, shift) : emu_template_movn_w32(dst, immediate, shift);
        return true;
    case ARM64_INSN_MOVZ:
        *result = sf ? emu_template_movz_w64(dst, immediate, shift) : emu_template_movz_w32(dst, immediate, shift);
        return true;
    case ARM64_INSN_MOVK:
        *result = sf ? emu_template_movk_w64(dst, immediate, shift) : emu_template_movk_w32(dst, immediate, shift);
        return true;
    default:
        return false;
    }
}
// clang-format on

/* ======================== 数据处理立即数类：缓存条目执行模板 ======================== */

/* 每个固定立即数执行模板直接对应缓存条目中的 execute 函数地址。 */

static enum emu_insn_result emu_execute_pc_relative_adr(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    uint64_t base = pc;

    (void)fp_regs;
    if (entry->reg0 != 31) regs->regs[entry->reg0] = base + entry->operand0;
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_pc_relative_adrp(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    uint64_t pc = regs->pc;
    uint64_t base = pc & ~0xFFFULL;

    (void)fp_regs;
    if (entry->reg0 != 31) regs->regs[entry->reg0] = base + entry->operand0;
    regs->pc = pc + 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_extract_extr_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    result = emu_extract_bits(reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), entry->operand0, sf);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_extract_extr_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    result = emu_extract_bits(reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), entry->operand0, sf);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_immediate_add_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_IMMEDIATE;
    bool sf = false;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), entry->operand0, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg1, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg1, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_immediate_add_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_IMMEDIATE;
    bool sf = true;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), entry->operand0, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg1, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg1, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_immediate_adds_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_IMMEDIATE;
    bool sf = false;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), entry->operand0, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg1, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg1, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_immediate_adds_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_IMMEDIATE;
    bool sf = true;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), entry->operand0, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg1, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg1, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_immediate_sub_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_IMMEDIATE;
    bool sf = false;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), entry->operand0, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg1, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg1, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_immediate_sub_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_IMMEDIATE;
    bool sf = true;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), entry->operand0, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg1, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg1, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_immediate_subs_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_IMMEDIATE;
    bool sf = false;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), entry->operand0, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg1, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg1, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_immediate_subs_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_IMMEDIATE;
    bool sf = true;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), entry->operand0, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg1, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg1, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_minmax_immediate_smax_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMAX_IMMEDIATE;
    bool sf = false;
    uint64_t immediate = entry->operand0;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t signed_b = emu_sign_extend_byte_hw(immediate) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, signed_b, immediate, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_minmax_immediate_smax_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMAX_IMMEDIATE;
    bool sf = true;
    uint64_t immediate = entry->operand0;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t signed_b = emu_sign_extend_byte_hw(immediate) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, signed_b, immediate, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_minmax_immediate_umax_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMAX_IMMEDIATE;
    bool sf = false;
    uint64_t immediate = entry->operand0;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t signed_b = emu_sign_extend_byte_hw(immediate) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, signed_b, immediate, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_minmax_immediate_umax_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMAX_IMMEDIATE;
    bool sf = true;
    uint64_t immediate = entry->operand0;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t signed_b = emu_sign_extend_byte_hw(immediate) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, signed_b, immediate, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_minmax_immediate_smin_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMIN_IMMEDIATE;
    bool sf = false;
    uint64_t immediate = entry->operand0;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t signed_b = emu_sign_extend_byte_hw(immediate) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, signed_b, immediate, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_minmax_immediate_smin_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMIN_IMMEDIATE;
    bool sf = true;
    uint64_t immediate = entry->operand0;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t signed_b = emu_sign_extend_byte_hw(immediate) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, signed_b, immediate, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_minmax_immediate_umin_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMIN_IMMEDIATE;
    bool sf = false;
    uint64_t immediate = entry->operand0;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t signed_b = emu_sign_extend_byte_hw(immediate) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, signed_b, immediate, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_minmax_immediate_umin_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMIN_IMMEDIATE;
    bool sf = true;
    uint64_t immediate = entry->operand0;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t signed_b = emu_sign_extend_byte_hw(immediate) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, signed_b, immediate, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_immediate_and_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_IMMEDIATE;
    bool sf = false;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), entry->operand0, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_immediate_and_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_IMMEDIATE;
    bool sf = true;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), entry->operand0, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_immediate_orr_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_IMMEDIATE;
    bool sf = false;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), entry->operand0, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_immediate_orr_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_IMMEDIATE;
    bool sf = true;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), entry->operand0, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_immediate_eor_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_IMMEDIATE;
    bool sf = false;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), entry->operand0, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_immediate_eor_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_IMMEDIATE;
    bool sf = true;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), entry->operand0, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_immediate_ands_immediate_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_IMMEDIATE;
    bool sf = false;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), entry->operand0, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_immediate_ands_immediate_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_IMMEDIATE;
    bool sf = true;
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), entry->operand0, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_move_wide_movn_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MOVN;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_move_wide_hw(instruction, reg_read(regs, entry->reg0), entry->operand0, entry->option0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg0, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_move_wide_movn_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MOVN;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_move_wide_hw(instruction, reg_read(regs, entry->reg0), entry->operand0, entry->option0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg0, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_move_wide_movz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MOVZ;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_move_wide_hw(instruction, reg_read(regs, entry->reg0), entry->operand0, entry->option0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg0, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_move_wide_movz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MOVZ;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_move_wide_hw(instruction, reg_read(regs, entry->reg0), entry->operand0, entry->option0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg0, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_move_wide_movk_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MOVK;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_move_wide_hw(instruction, reg_read(regs, entry->reg0), entry->operand0, entry->option0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg0, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_move_wide_movk_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MOVK;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_move_wide_hw(instruction, reg_read(regs, entry->reg0), entry->operand0, entry->option0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg0, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_bitfield_sbfm_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    static const enum arm64_instruction instructions[] = {
        ARM64_INSN_SBFM,
        ARM64_INSN_BFM,
        ARM64_INSN_UBFM,
    };
    enum arm64_instruction instruction;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (0 >= sizeof(instructions) / sizeof(instructions[0])) return EMU_INSN_SKIP;
    instruction = instructions[0];
    if (!emu_bitfield_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), reg_read(regs, entry->reg1) & emu_dp_mask(sf), entry->reg2, entry->operand0, entry->operand1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_bitfield_sbfm_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    static const enum arm64_instruction instructions[] = {
        ARM64_INSN_SBFM,
        ARM64_INSN_BFM,
        ARM64_INSN_UBFM,
    };
    enum arm64_instruction instruction;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (0 >= sizeof(instructions) / sizeof(instructions[0])) return EMU_INSN_SKIP;
    instruction = instructions[0];
    if (!emu_bitfield_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), reg_read(regs, entry->reg1) & emu_dp_mask(sf), entry->reg2, entry->operand0, entry->operand1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_bitfield_bfm_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    static const enum arm64_instruction instructions[] = {
        ARM64_INSN_SBFM,
        ARM64_INSN_BFM,
        ARM64_INSN_UBFM,
    };
    enum arm64_instruction instruction;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (1 >= sizeof(instructions) / sizeof(instructions[0])) return EMU_INSN_SKIP;
    instruction = instructions[1];
    if (!emu_bitfield_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), reg_read(regs, entry->reg1) & emu_dp_mask(sf), entry->reg2, entry->operand0, entry->operand1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_bitfield_bfm_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    static const enum arm64_instruction instructions[] = {
        ARM64_INSN_SBFM,
        ARM64_INSN_BFM,
        ARM64_INSN_UBFM,
    };
    enum arm64_instruction instruction;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (1 >= sizeof(instructions) / sizeof(instructions[0])) return EMU_INSN_SKIP;
    instruction = instructions[1];
    if (!emu_bitfield_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), reg_read(regs, entry->reg1) & emu_dp_mask(sf), entry->reg2, entry->operand0, entry->operand1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_bitfield_ubfm_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    static const enum arm64_instruction instructions[] = {
        ARM64_INSN_SBFM,
        ARM64_INSN_BFM,
        ARM64_INSN_UBFM,
    };
    enum arm64_instruction instruction;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (2 >= sizeof(instructions) / sizeof(instructions[0])) return EMU_INSN_SKIP;
    instruction = instructions[2];
    if (!emu_bitfield_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), reg_read(regs, entry->reg1) & emu_dp_mask(sf), entry->reg2, entry->operand0, entry->operand1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_bitfield_ubfm_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    static const enum arm64_instruction instructions[] = {
        ARM64_INSN_SBFM,
        ARM64_INSN_BFM,
        ARM64_INSN_UBFM,
    };
    enum arm64_instruction instruction;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (2 >= sizeof(instructions) / sizeof(instructions[0])) return EMU_INSN_SKIP;
    instruction = instructions[2];
    if (!emu_bitfield_hw(instruction, reg_read(regs, entry->reg0) & emu_dp_mask(sf), reg_read(regs, entry->reg1) & emu_dp_mask(sf), entry->reg2, entry->operand0, entry->operand1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}



static enum emu_insn_result (*emu_select_immediate_executor(const struct arm64_decoded_insn *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)

{

    switch (decoded->instruction)

    {

    case ARM64_INSN_ADR:

        return emu_execute_pc_relative_adr;

    case ARM64_INSN_ADRP:

        return emu_execute_pc_relative_adrp;

    case ARM64_INSN_EXTR:

        if (decoded->operand_width == 32) return emu_execute_extract_extr_w32;

        if (decoded->operand_width == 64) return emu_execute_extract_extr_w64;

        return NULL;

    case ARM64_INSN_ADD_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_addsub_immediate_add_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_addsub_immediate_add_immediate_w64;

        return NULL;

    case ARM64_INSN_ADDS_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_addsub_immediate_adds_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_addsub_immediate_adds_immediate_w64;

        return NULL;

    case ARM64_INSN_SUB_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_addsub_immediate_sub_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_addsub_immediate_sub_immediate_w64;

        return NULL;

    case ARM64_INSN_SUBS_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_addsub_immediate_subs_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_addsub_immediate_subs_immediate_w64;

        return NULL;

    case ARM64_INSN_SMAX_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_minmax_immediate_smax_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_minmax_immediate_smax_immediate_w64;

        return NULL;

    case ARM64_INSN_UMAX_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_minmax_immediate_umax_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_minmax_immediate_umax_immediate_w64;

        return NULL;

    case ARM64_INSN_SMIN_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_minmax_immediate_smin_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_minmax_immediate_smin_immediate_w64;

        return NULL;

    case ARM64_INSN_UMIN_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_minmax_immediate_umin_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_minmax_immediate_umin_immediate_w64;

        return NULL;

    case ARM64_INSN_AND_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_logic_immediate_and_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_logic_immediate_and_immediate_w64;

        return NULL;

    case ARM64_INSN_ORR_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_logic_immediate_orr_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_logic_immediate_orr_immediate_w64;

        return NULL;

    case ARM64_INSN_EOR_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_logic_immediate_eor_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_logic_immediate_eor_immediate_w64;

        return NULL;

    case ARM64_INSN_ANDS_IMMEDIATE:

        if (decoded->operand_width == 32) return emu_execute_logic_immediate_ands_immediate_w32;

        if (decoded->operand_width == 64) return emu_execute_logic_immediate_ands_immediate_w64;

        return NULL;

    case ARM64_INSN_MOVN:

        if (decoded->operand_width == 32) return emu_execute_move_wide_movn_w32;

        if (decoded->operand_width == 64) return emu_execute_move_wide_movn_w64;

        return NULL;

    case ARM64_INSN_MOVZ:

        if (decoded->operand_width == 32) return emu_execute_move_wide_movz_w32;

        if (decoded->operand_width == 64) return emu_execute_move_wide_movz_w64;

        return NULL;

    case ARM64_INSN_MOVK:

        if (decoded->operand_width == 32) return emu_execute_move_wide_movk_w32;

        if (decoded->operand_width == 64) return emu_execute_move_wide_movk_w64;

        return NULL;

    case ARM64_INSN_SBFM:

        if (decoded->operand_width == 32) return emu_execute_bitfield_sbfm_w32;

        if (decoded->operand_width == 64) return emu_execute_bitfield_sbfm_w64;

        return NULL;

    case ARM64_INSN_BFM:

        if (decoded->operand_width == 32) return emu_execute_bitfield_bfm_w32;

        if (decoded->operand_width == 64) return emu_execute_bitfield_bfm_w64;

        return NULL;

    case ARM64_INSN_UBFM:

        if (decoded->operand_width == 32) return emu_execute_bitfield_ubfm_w32;

        if (decoded->operand_width == 64) return emu_execute_bitfield_ubfm_w64;

        return NULL;

    default:

        return NULL;

    }

}

/* ======================== 数据处理立即数类：解码结果构建缓存条目 ======================== */

bool emu_build_immediate_executor(const struct arm64_decoded_insn *decoded, struct arm64_executor_entry *entry)
{
    entry->execute = emu_select_immediate_executor(decoded);
    if (!entry->execute) return false;

    entry->operand0 = decoded->immediate;
    if (decoded->instruction == ARM64_INSN_ADR || decoded->instruction == ARM64_INSN_ADRP) entry->operand0 = decoded->offset;
    else if (decoded->instruction == ARM64_INSN_SBFM || decoded->instruction == ARM64_INSN_BFM || decoded->instruction == ARM64_INSN_UBFM) entry->operand0 = decoded->wmask;
    else if (decoded->instruction == ARM64_INSN_EXTR) entry->operand0 = decoded->shift_amount;
    entry->operand1 = decoded->tmask;
    entry->reg0 = decoded->instruction == ARM64_INSN_ADR || decoded->instruction == ARM64_INSN_ADRP || decoded->instruction == ARM64_INSN_MOVN || decoded->instruction == ARM64_INSN_MOVZ || decoded->instruction == ARM64_INSN_MOVK ? decoded->rd : decoded->rn;
    entry->reg1 = decoded->instruction == ARM64_INSN_EXTR ? decoded->rm : decoded->rd;
    entry->reg2 = decoded->instruction == ARM64_INSN_EXTR ? decoded->rd : decoded->immr;
    entry->option0 = decoded->shift_amount;
    return true;
}
