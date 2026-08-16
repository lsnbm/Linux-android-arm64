#include "arm64_emulate_internal.h"

/* ======================== 数据处理寄存器类：专属硬件语义辅助 ======================== */

// clang-format off
static inline bool emu_cond_select_hw(enum arm64_instruction instruction, uint64_t a, uint64_t b, uint64_t nzcv, uint32_t condition, bool sf, uint64_t *result)
{
    uint32_t take = emu_cond_holds(nzcv, condition);

    if (!result) return false;

    switch (instruction)
    {
    case ARM64_INSN_CSEL:
        *result = sf ? emu_template_csel_w64(a, b, take) : emu_template_csel_w32(a, b, take);
        return true;
    case ARM64_INSN_CSINC:
        *result = sf ? emu_template_csinc_w64(a, b, take) : emu_template_csinc_w32(a, b, take);
        return true;
    case ARM64_INSN_CSINV:
        *result = sf ? emu_template_csinv_w64(a, b, take) : emu_template_csinv_w32(a, b, take);
        return true;
    case ARM64_INSN_CSNEG:
        *result = sf ? emu_template_csneg_w64(a, b, take) : emu_template_csneg_w32(a, b, take);
        return true;
    default:
        return false;
    }
}

/* ADD/SUB 扩展寄存器：option 000..111 对应 UXT 或 SXT 变体，结果再左移 shift 位。 */
static inline uint64_t emu_extend_reg(uint64_t val, uint32_t option, uint32_t shift)
{
    switch (option)
    {
    case 0: return emu_template_uxtb_shift(val, shift);
    case 1: return emu_template_uxth_shift(val, shift);
    case 2: return emu_template_uxtw_shift(val, shift);
    case 3: return emu_template_uxtx_shift(val, shift);
    case 4: return emu_template_sxtb_shift(val, shift);
    case 5: return emu_template_sxth_shift(val, shift);
    case 6: return emu_template_sxtw_shift(val, shift);
    case 7: return emu_template_sxtx_shift(val, shift);
    default:
        return val;
    }
}

static inline bool emu_addsub_carry_hw(enum arm64_instruction instruction, uint64_t a, uint64_t b, uint64_t input_nzcv, bool sf, uint64_t *result, uint64_t *nzcv, bool *setflags)
{
    if (!result || !nzcv || !setflags) return false;
    *setflags = false;
    switch (instruction)
    {
    case ARM64_INSN_ADC:
        *result = sf ? emu_template_adc_w64(a, b, input_nzcv) : emu_template_adc_w32(a, b, input_nzcv);
        return true;
    case ARM64_INSN_ADCS:
        *result = sf ? emu_template_adcs_w64(a, b, input_nzcv, nzcv) : emu_template_adcs_w32(a, b, input_nzcv, nzcv);
        *setflags = true;
        return true;
    case ARM64_INSN_SBC:
        *result = sf ? emu_template_sbc_w64(a, b, input_nzcv) : emu_template_sbc_w32(a, b, input_nzcv);
        return true;
    case ARM64_INSN_SBCS:
        *result = sf ? emu_template_sbcs_w64(a, b, input_nzcv, nzcv) : emu_template_sbcs_w32(a, b, input_nzcv, nzcv);
        *setflags = true;
        return true;
    default:
        return false;
    }
}

static inline uint64_t emu_dp_shift_hw(uint64_t value, uint32_t type, uint32_t amount, bool sf)
{
    switch (type)
    {
    case 0: return sf ? emu_template_lslv_w64(value, amount) : emu_template_lslv_w32(value, amount);
    case 1: return sf ? emu_template_lsrv_w64(value, amount) : emu_template_lsrv_w32(value, amount);
    case 2: return sf ? emu_template_asrv_w64(value, amount) : emu_template_asrv_w32(value, amount);
    default: return sf ? emu_template_rorv_w64(value, amount) : emu_template_rorv_w32(value, amount);
    }
}
// clang-format on

static inline uint64_t emu_dp_rbit_hw(uint64_t value, bool sf)
{
    return sf ? emu_template_rbit_w64(value) : emu_template_rbit_w32(value);
}

static inline uint64_t emu_dp_rev16_hw(uint64_t value, bool sf)
{
    return sf ? emu_template_rev16_w64(value) : emu_template_rev16_w32(value);
}

static inline uint64_t emu_dp_rev32_hw(uint64_t value, bool sf)
{
    return sf ? emu_template_rev32_w64(value) : emu_template_rev32_w32(value);
}

static inline uint64_t emu_dp_rev64_hw(uint64_t value)
{
    return emu_template_rev64_w64(value);
}

static inline uint64_t emu_dp_clz_hw(uint64_t value, bool sf)
{
    return sf ? emu_template_clz_w64(value) : emu_template_clz_w32(value);
}

static inline uint64_t emu_dp_cls_hw(uint64_t value, bool sf)
{
    return sf ? emu_template_cls_w64(value) : emu_template_cls_w32(value);
}

static inline uint32_t emu_dp_count_bits_hw(uint64_t value, bool sf)
{
    return sf ? emu_template_cnt_w64(value) : emu_template_cnt_w32(value);
}

/* ======================== 数据处理寄存器类：缓存条目执行模板 ======================== */

/* 每个固定寄存器执行模板直接对应缓存条目中的 execute 函数地址。 */

static enum emu_insn_result emu_execute_logic_shifted_and_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_and_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_and_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_and_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_and_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_and_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_and_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_and_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_AND_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bic_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BIC_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bic_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BIC_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bic_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BIC_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bic_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BIC_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bic_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BIC_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bic_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BIC_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bic_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BIC_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bic_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BIC_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orr_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orr_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orr_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orr_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orr_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orr_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orr_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orr_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORR_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orn_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORN_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orn_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORN_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orn_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORN_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orn_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORN_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orn_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORN_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orn_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORN_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orn_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORN_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_orn_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ORN_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eor_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eor_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eor_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eor_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eor_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eor_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eor_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eor_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EOR_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eon_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EON_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eon_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EON_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eon_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EON_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eon_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EON_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eon_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EON_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eon_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EON_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eon_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EON_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_eon_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_EON_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_ands_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_ands_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_ands_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_ands_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_ands_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_ands_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_ands_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_ands_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ANDS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bics_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BICS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bics_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BICS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bics_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BICS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bics_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BICS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bics_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BICS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bics_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BICS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bics_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BICS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_logic_shifted_bics_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_BICS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    result = emu_logic_hw(instruction, reg_read(regs, entry->reg0), b, sf, &nzcv, &setflags);
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_add_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_add_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_add_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_add_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_add_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_add_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_add_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_add_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_adds_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_adds_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_adds_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_adds_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_adds_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_adds_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_adds_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_adds_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_sub_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_sub_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_sub_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_sub_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_sub_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_sub_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_sub_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_sub_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_subs_shifted_register_w32_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_subs_shifted_register_w64_shift0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 0, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_subs_shifted_register_w32_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_subs_shifted_register_w64_shift1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 1, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_subs_shifted_register_w32_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_subs_shifted_register_w64_shift2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 2, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_subs_shifted_register_w32_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_SHIFTED_REGISTER;
    bool sf = false;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_shifted_subs_shifted_register_w64_shift3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_SHIFTED_REGISTER;
    bool sf = true;
    uint64_t b = emu_dp_shift_hw(reg_read(regs, entry->reg1), 3, entry->option0, sf);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w32_extend0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 0, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w64_extend0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 0, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w32_extend1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 1, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w64_extend1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 1, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w32_extend2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 2, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w64_extend2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 2, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w32_extend3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 3, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w64_extend3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 3, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w32_extend4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 4, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w64_extend4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 4, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w32_extend5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 5, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w64_extend5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 5, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w32_extend6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 6, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w64_extend6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 6, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w32_extend7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 7, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_add_extended_register_w64_extend7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADD_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 7, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w32_extend0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 0, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w64_extend0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 0, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w32_extend1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 1, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w64_extend1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 1, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w32_extend2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 2, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w64_extend2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 2, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w32_extend3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 3, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w64_extend3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 3, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w32_extend4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 4, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w64_extend4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 4, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w32_extend5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 5, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w64_extend5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 5, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w32_extend6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 6, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w64_extend6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 6, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w32_extend7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 7, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_adds_extended_register_w64_extend7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADDS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 7, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w32_extend0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 0, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w64_extend0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 0, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w32_extend1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 1, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w64_extend1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 1, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w32_extend2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 2, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w64_extend2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 2, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w32_extend3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 3, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w64_extend3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 3, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w32_extend4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 4, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w64_extend4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 4, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w32_extend5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 5, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w64_extend5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 5, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w32_extend6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 6, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w64_extend6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 6, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w32_extend7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 7, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_sub_extended_register_w64_extend7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUB_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 7, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w32_extend0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 0, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w64_extend0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 0, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w32_extend1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 1, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w64_extend1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 1, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w32_extend2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 2, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w64_extend2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 2, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w32_extend3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 3, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w64_extend3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 3, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w32_extend4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 4, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w64_extend4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 4, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w32_extend5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 5, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w64_extend5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 5, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w32_extend6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 6, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w64_extend6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 6, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w32_extend7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = false;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 7, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_extended_subs_extended_register_w64_extend7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SUBS_EXTENDED_REGISTER;
    bool sf = true;
    uint64_t b = emu_extend_reg(reg_read(regs, entry->reg1), 7, entry->option0);
    uint64_t nzcv = 0;
    bool setflags;
    uint64_t result;

    (void)fp_regs;
    if (!emu_addsub_hw(instruction, addr_reg_read(regs, entry->reg0), b, sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags)
    {
        emu_write_nzcv(regs, nzcv);
        reg_write(regs, entry->reg2, result, sf);
    }
    else
    {
        addr_reg_write(regs, entry->reg2, result);
    }
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_carry_adc_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADC;
    bool sf = false;
    uint64_t result, nzcv;
    bool setflags;

    (void)fp_regs;
    if (!emu_addsub_carry_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_carry_adc_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADC;
    bool sf = true;
    uint64_t result, nzcv;
    bool setflags;

    (void)fp_regs;
    if (!emu_addsub_carry_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_carry_adcs_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADCS;
    bool sf = false;
    uint64_t result, nzcv;
    bool setflags;

    (void)fp_regs;
    if (!emu_addsub_carry_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_carry_adcs_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ADCS;
    bool sf = true;
    uint64_t result, nzcv;
    bool setflags;

    (void)fp_regs;
    if (!emu_addsub_carry_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_carry_sbc_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SBC;
    bool sf = false;
    uint64_t result, nzcv;
    bool setflags;

    (void)fp_regs;
    if (!emu_addsub_carry_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_carry_sbc_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SBC;
    bool sf = true;
    uint64_t result, nzcv;
    bool setflags;

    (void)fp_regs;
    if (!emu_addsub_carry_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_carry_sbcs_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SBCS;
    bool sf = false;
    uint64_t result, nzcv;
    bool setflags;

    (void)fp_regs;
    if (!emu_addsub_carry_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_addsub_carry_sbcs_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SBCS;
    bool sf = true;
    uint64_t result, nzcv;
    bool setflags;

    (void)fp_regs;
    if (!emu_addsub_carry_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), sf, &result, &nzcv, &setflags)) return EMU_INSN_SKIP;
    if (setflags) emu_write_nzcv(regs, nzcv);
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 2, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 3, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 4, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 5, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 6, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 7, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 8, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 9, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 10, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 11, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 12, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 13, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 14, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w32_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 15, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 2, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 3, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 4, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 5, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 6, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 7, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 8, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 9, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 10, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 11, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 12, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 13, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 14, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csel_w64_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSEL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 15, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 2, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 3, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 4, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 5, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 6, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 7, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 8, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 9, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 10, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 11, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 12, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 13, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 14, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w32_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 15, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 2, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 3, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 4, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 5, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 6, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 7, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 8, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 9, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 10, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 11, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 12, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 13, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 14, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinc_w64_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINC;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 15, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 2, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 3, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 4, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 5, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 6, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 7, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 8, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 9, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 10, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 11, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 12, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 13, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 14, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w32_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 15, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 2, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 3, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 4, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 5, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 6, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 7, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 8, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 9, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 10, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 11, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 12, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 13, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 14, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csinv_w64_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSINV;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 15, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 2, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 3, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 4, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 5, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 6, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 7, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 8, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 9, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 10, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 11, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 12, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 13, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 14, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w32_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 15, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 0, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 1, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 2, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 3, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 4, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 5, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 6, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 7, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 8, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 9, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 10, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 11, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 12, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 13, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 14, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_select_csneg_w64_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CSNEG;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_cond_select_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), emu_read_nzcv(regs), 15, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_udiv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UDIV;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_udiv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UDIV;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_sdiv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SDIV;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_sdiv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SDIV;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_lslv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_LSLV;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_lslv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_LSLV;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_lsrv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_LSRV;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_lsrv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_LSRV;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_asrv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ASRV;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_asrv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ASRV;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_rorv_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_RORV;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_rorv_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_RORV;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32b_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32B;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32b_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32B;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32h_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32H;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32h_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32H;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32w_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32W;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32w_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32W;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32x_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32X;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32x_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32X;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32cb_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32CB;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32cb_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32CB;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32ch_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32CH;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32ch_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32CH;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32cw_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32CW;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32cw_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32CW;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32cx_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32CX;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_crc32cx_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CRC32CX;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_smax_register_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMAX_REGISTER;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_smax_register_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMAX_REGISTER;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_umax_register_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMAX_REGISTER;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_umax_register_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMAX_REGISTER;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static inline bool emu_multiply_hw(enum arm64_instruction instruction, uint64_t n, uint64_t m, uint64_t a, bool sf, uint64_t *result)
{
    if (!result) return false;
    switch (instruction)
    {
    case ARM64_INSN_MADD: *result = sf ? emu_template_madd_w64(n, m, a) : emu_template_madd_w32(n, m, a); return true;
    case ARM64_INSN_MSUB: *result = sf ? emu_template_msub_w64(n, m, a) : emu_template_msub_w32(n, m, a); return true;
    case ARM64_INSN_SMADDL: *result = emu_template_smaddl(n, m, a); return true;
    case ARM64_INSN_SMSUBL: *result = emu_template_smsubl(n, m, a); return true;
    case ARM64_INSN_SMULH: *result = emu_template_smulh(n, m); return true;
    case ARM64_INSN_UMADDL: *result = emu_template_umaddl(n, m, a); return true;
    case ARM64_INSN_UMSUBL: *result = emu_template_umsubl(n, m, a); return true;
    case ARM64_INSN_UMULH: *result = emu_template_umulh(n, m); return true;
    default: return false;
    }
}

static enum emu_insn_result emu_execute_integer_binary_smin_register_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMIN_REGISTER;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_smin_register_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMIN_REGISTER;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_umin_register_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMIN_REGISTER;
    bool sf = false;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_binary_umin_register_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMIN_REGISTER;
    bool sf = true;
    uint64_t a = reg_read(regs, entry->reg0) & emu_dp_mask(sf);
    uint64_t b = reg_read(regs, entry->reg1) & emu_dp_mask(sf);
    uint64_t result;

    (void)fp_regs;
    if (!emu_integer_binary_hw(instruction, a, b, b, sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg2, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_madd_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MADD;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_madd_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MADD;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_msub_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MSUB;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_msub_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_MSUB;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_smaddl_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMADDL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_smaddl_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMADDL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_smsubl_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMSUBL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_smsubl_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMSUBL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_smulh_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMULH;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_smulh_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_SMULH;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_umaddl_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMADDL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_umaddl_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMADDL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_umsubl_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMSUBL;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_umsubl_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMSUBL;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_umulh_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMULH;
    bool sf = false;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_multiply_umulh_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_UMULH;
    bool sf = true;
    uint64_t result;

    (void)fp_regs;
    if (!emu_multiply_hw(instruction, reg_read(regs, entry->reg0), reg_read(regs, entry->reg1), reg_read(regs, entry->reg2), sf, &result)) return EMU_INSN_SKIP;
    reg_write(regs, entry->reg3, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 0))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 1))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 2))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 3))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 4))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 5))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 6))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 7))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 8))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 9))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 10))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 11))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 12))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 13))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 14))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w32_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 15))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 0))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 1))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 2))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 3))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 4))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 5))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 6))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 7))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 8))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 9))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 10))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 11))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 12))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 13))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 14))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_register_w64_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 15))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 0))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 1))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 2))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 3))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 4))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 5))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 6))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 7))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 8))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 9))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 10))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 11))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 12))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 13))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 14))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w32_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = false;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 15))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 0))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 1))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 2))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 3))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 4))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 5))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 6))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 7))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 8))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 9))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 10))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 11))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 12))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 13))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 14))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_register_w64_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_REGISTER;
    bool sf = true;
    uint64_t b = reg_read(regs, entry->reg1);
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 15))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 0))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 1))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 2))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 3))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 4))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 5))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 6))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 7))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 8))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 9))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 10))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 11))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 12))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 13))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 14))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w32_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 15))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 0))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 1))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 2))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 3))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 4))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 5))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 6))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 7))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 8))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 9))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 10))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 11))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 12))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 13))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 14))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmn_immediate_w64_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMN_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 15))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 0))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 1))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 2))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 3))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 4))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 5))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 6))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 7))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 8))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 9))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 10))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 11))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 12))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 13))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 14))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w32_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = false;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 15))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond0(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 0))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond1(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 1))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond2(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 2))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond3(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 3))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond4(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 4))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond5(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 5))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond6(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 6))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond7(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 7))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond8(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 8))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond9(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 9))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond10(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 10))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond11(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 11))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond12(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 12))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond13(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 13))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond14(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 14))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_cond_compare_ccmp_immediate_w64_cond15(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CCMP_IMMEDIATE;
    bool sf = true;
    uint64_t b = entry->operand0;
    uint64_t result, flags;
    bool setflags;

    (void)fp_regs;
    if (emu_cond_holds(emu_read_nzcv(regs), 15))
    {
        if (!emu_addsub_hw(instruction, reg_read(regs, entry->reg0), b, sf, &result, &flags, &setflags) || !setflags) return EMU_INSN_SKIP;
    }
    else
    {
        flags = (uint64_t)entry->reg3 << 28;
    }
    emu_write_nzcv(regs, flags);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_rbit_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_RBIT;
    bool sf = false;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_rbit_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_RBIT;
    bool sf = true;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_rev16_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_REV16;
    bool sf = false;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_rev16_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_REV16;
    bool sf = true;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_rev32_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_REV32;
    bool sf = false;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_rev32_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_REV32;
    bool sf = true;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_rev64_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_REV64;
    bool sf = false;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_rev64_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_REV64;
    bool sf = true;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_clz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CLZ;
    bool sf = false;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_clz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CLZ;
    bool sf = true;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_cls_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CLS;
    bool sf = false;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_cls_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CLS;
    bool sf = true;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_ctz_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CTZ;
    bool sf = false;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_ctz_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CTZ;
    bool sf = true;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_cnt_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CNT;
    bool sf = false;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_cnt_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_CNT;
    bool sf = true;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_abs_w32(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ABS;
    bool sf = false;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}

static enum emu_insn_result emu_execute_integer_unary_abs_w64(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    enum arm64_instruction instruction = (enum arm64_instruction)ARM64_INSN_ABS;
    bool sf = true;
    uint64_t src = reg_read(regs, entry->reg0);
    uint64_t result;

    (void)fp_regs;
    switch (instruction)
    {
    case ARM64_INSN_RBIT:
        result = emu_dp_rbit_hw(src, sf);
        break;
    case ARM64_INSN_REV16:
        result = emu_dp_rev16_hw(src, sf);
        break;
    case ARM64_INSN_REV32:
        result = emu_dp_rev32_hw(src, sf);
        break;
    case ARM64_INSN_REV64:
        result = emu_dp_rev64_hw(src);
        break;
    case ARM64_INSN_CLZ:
        result = emu_dp_clz_hw(src, sf);
        break;
    case ARM64_INSN_CLS:
        result = emu_dp_cls_hw(src, sf);
        break;
    case ARM64_INSN_CTZ:
        result = emu_dp_clz_hw(emu_dp_rbit_hw(src, sf), sf);
        break;
    case ARM64_INSN_CNT:
        result = emu_dp_count_bits_hw(src, sf);
        break;
    case ARM64_INSN_ABS:
        result = sf ? emu_template_abs_w64(src) : emu_template_abs_w32(src);
        break;
    default:
        return EMU_INSN_SKIP;
    }
    reg_write(regs, entry->reg1, result, sf);
    regs->pc += 4;
    return EMU_INSN_HANDLED;
}



static enum emu_insn_result (*emu_select_register_executor(const struct arm64_decoded_insn *decoded))(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)

{

    switch (decoded->instruction)

    {

    case ARM64_INSN_AND_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_logic_shifted_and_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_logic_shifted_and_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_logic_shifted_and_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_logic_shifted_and_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_logic_shifted_and_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_logic_shifted_and_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_logic_shifted_and_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_logic_shifted_and_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_BIC_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_logic_shifted_bic_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_logic_shifted_bic_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_logic_shifted_bic_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_logic_shifted_bic_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_logic_shifted_bic_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_logic_shifted_bic_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_logic_shifted_bic_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_logic_shifted_bic_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_ORR_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_logic_shifted_orr_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_logic_shifted_orr_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_logic_shifted_orr_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_logic_shifted_orr_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_logic_shifted_orr_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_logic_shifted_orr_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_logic_shifted_orr_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_logic_shifted_orr_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_ORN_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_logic_shifted_orn_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_logic_shifted_orn_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_logic_shifted_orn_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_logic_shifted_orn_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_logic_shifted_orn_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_logic_shifted_orn_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_logic_shifted_orn_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_logic_shifted_orn_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_EOR_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_logic_shifted_eor_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_logic_shifted_eor_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_logic_shifted_eor_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_logic_shifted_eor_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_logic_shifted_eor_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_logic_shifted_eor_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_logic_shifted_eor_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_logic_shifted_eor_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_EON_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_logic_shifted_eon_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_logic_shifted_eon_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_logic_shifted_eon_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_logic_shifted_eon_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_logic_shifted_eon_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_logic_shifted_eon_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_logic_shifted_eon_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_logic_shifted_eon_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_ANDS_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_logic_shifted_ands_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_logic_shifted_ands_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_logic_shifted_ands_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_logic_shifted_ands_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_logic_shifted_ands_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_logic_shifted_ands_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_logic_shifted_ands_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_logic_shifted_ands_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_BICS_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_logic_shifted_bics_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_logic_shifted_bics_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_logic_shifted_bics_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_logic_shifted_bics_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_logic_shifted_bics_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_logic_shifted_bics_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_logic_shifted_bics_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_logic_shifted_bics_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_ADD_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_addsub_shifted_add_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_addsub_shifted_add_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_addsub_shifted_add_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_addsub_shifted_add_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_addsub_shifted_add_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_addsub_shifted_add_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_addsub_shifted_add_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_addsub_shifted_add_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_ADDS_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_addsub_shifted_adds_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_addsub_shifted_adds_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_addsub_shifted_adds_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_addsub_shifted_adds_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_addsub_shifted_adds_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_addsub_shifted_adds_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_addsub_shifted_adds_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_addsub_shifted_adds_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_SUB_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_addsub_shifted_sub_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_addsub_shifted_sub_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_addsub_shifted_sub_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_addsub_shifted_sub_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_addsub_shifted_sub_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_addsub_shifted_sub_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_addsub_shifted_sub_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_addsub_shifted_sub_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_SUBS_SHIFTED_REGISTER:

        if (decoded->operand_width == 32 && decoded->shift_type == 0) return emu_execute_addsub_shifted_subs_shifted_register_w32_shift0;

        if (decoded->operand_width == 64 && decoded->shift_type == 0) return emu_execute_addsub_shifted_subs_shifted_register_w64_shift0;

        if (decoded->operand_width == 32 && decoded->shift_type == 1) return emu_execute_addsub_shifted_subs_shifted_register_w32_shift1;

        if (decoded->operand_width == 64 && decoded->shift_type == 1) return emu_execute_addsub_shifted_subs_shifted_register_w64_shift1;

        if (decoded->operand_width == 32 && decoded->shift_type == 2) return emu_execute_addsub_shifted_subs_shifted_register_w32_shift2;

        if (decoded->operand_width == 64 && decoded->shift_type == 2) return emu_execute_addsub_shifted_subs_shifted_register_w64_shift2;

        if (decoded->operand_width == 32 && decoded->shift_type == 3) return emu_execute_addsub_shifted_subs_shifted_register_w32_shift3;

        if (decoded->operand_width == 64 && decoded->shift_type == 3) return emu_execute_addsub_shifted_subs_shifted_register_w64_shift3;

        return NULL;

    case ARM64_INSN_ADD_EXTENDED_REGISTER:

        if (decoded->operand_width == 32 && decoded->option == 0) return emu_execute_addsub_extended_add_extended_register_w32_extend0;

        if (decoded->operand_width == 64 && decoded->option == 0) return emu_execute_addsub_extended_add_extended_register_w64_extend0;

        if (decoded->operand_width == 32 && decoded->option == 1) return emu_execute_addsub_extended_add_extended_register_w32_extend1;

        if (decoded->operand_width == 64 && decoded->option == 1) return emu_execute_addsub_extended_add_extended_register_w64_extend1;

        if (decoded->operand_width == 32 && decoded->option == 2) return emu_execute_addsub_extended_add_extended_register_w32_extend2;

        if (decoded->operand_width == 64 && decoded->option == 2) return emu_execute_addsub_extended_add_extended_register_w64_extend2;

        if (decoded->operand_width == 32 && decoded->option == 3) return emu_execute_addsub_extended_add_extended_register_w32_extend3;

        if (decoded->operand_width == 64 && decoded->option == 3) return emu_execute_addsub_extended_add_extended_register_w64_extend3;

        if (decoded->operand_width == 32 && decoded->option == 4) return emu_execute_addsub_extended_add_extended_register_w32_extend4;

        if (decoded->operand_width == 64 && decoded->option == 4) return emu_execute_addsub_extended_add_extended_register_w64_extend4;

        if (decoded->operand_width == 32 && decoded->option == 5) return emu_execute_addsub_extended_add_extended_register_w32_extend5;

        if (decoded->operand_width == 64 && decoded->option == 5) return emu_execute_addsub_extended_add_extended_register_w64_extend5;

        if (decoded->operand_width == 32 && decoded->option == 6) return emu_execute_addsub_extended_add_extended_register_w32_extend6;

        if (decoded->operand_width == 64 && decoded->option == 6) return emu_execute_addsub_extended_add_extended_register_w64_extend6;

        if (decoded->operand_width == 32 && decoded->option == 7) return emu_execute_addsub_extended_add_extended_register_w32_extend7;

        if (decoded->operand_width == 64 && decoded->option == 7) return emu_execute_addsub_extended_add_extended_register_w64_extend7;

        return NULL;

    case ARM64_INSN_ADDS_EXTENDED_REGISTER:

        if (decoded->operand_width == 32 && decoded->option == 0) return emu_execute_addsub_extended_adds_extended_register_w32_extend0;

        if (decoded->operand_width == 64 && decoded->option == 0) return emu_execute_addsub_extended_adds_extended_register_w64_extend0;

        if (decoded->operand_width == 32 && decoded->option == 1) return emu_execute_addsub_extended_adds_extended_register_w32_extend1;

        if (decoded->operand_width == 64 && decoded->option == 1) return emu_execute_addsub_extended_adds_extended_register_w64_extend1;

        if (decoded->operand_width == 32 && decoded->option == 2) return emu_execute_addsub_extended_adds_extended_register_w32_extend2;

        if (decoded->operand_width == 64 && decoded->option == 2) return emu_execute_addsub_extended_adds_extended_register_w64_extend2;

        if (decoded->operand_width == 32 && decoded->option == 3) return emu_execute_addsub_extended_adds_extended_register_w32_extend3;

        if (decoded->operand_width == 64 && decoded->option == 3) return emu_execute_addsub_extended_adds_extended_register_w64_extend3;

        if (decoded->operand_width == 32 && decoded->option == 4) return emu_execute_addsub_extended_adds_extended_register_w32_extend4;

        if (decoded->operand_width == 64 && decoded->option == 4) return emu_execute_addsub_extended_adds_extended_register_w64_extend4;

        if (decoded->operand_width == 32 && decoded->option == 5) return emu_execute_addsub_extended_adds_extended_register_w32_extend5;

        if (decoded->operand_width == 64 && decoded->option == 5) return emu_execute_addsub_extended_adds_extended_register_w64_extend5;

        if (decoded->operand_width == 32 && decoded->option == 6) return emu_execute_addsub_extended_adds_extended_register_w32_extend6;

        if (decoded->operand_width == 64 && decoded->option == 6) return emu_execute_addsub_extended_adds_extended_register_w64_extend6;

        if (decoded->operand_width == 32 && decoded->option == 7) return emu_execute_addsub_extended_adds_extended_register_w32_extend7;

        if (decoded->operand_width == 64 && decoded->option == 7) return emu_execute_addsub_extended_adds_extended_register_w64_extend7;

        return NULL;

    case ARM64_INSN_SUB_EXTENDED_REGISTER:

        if (decoded->operand_width == 32 && decoded->option == 0) return emu_execute_addsub_extended_sub_extended_register_w32_extend0;

        if (decoded->operand_width == 64 && decoded->option == 0) return emu_execute_addsub_extended_sub_extended_register_w64_extend0;

        if (decoded->operand_width == 32 && decoded->option == 1) return emu_execute_addsub_extended_sub_extended_register_w32_extend1;

        if (decoded->operand_width == 64 && decoded->option == 1) return emu_execute_addsub_extended_sub_extended_register_w64_extend1;

        if (decoded->operand_width == 32 && decoded->option == 2) return emu_execute_addsub_extended_sub_extended_register_w32_extend2;

        if (decoded->operand_width == 64 && decoded->option == 2) return emu_execute_addsub_extended_sub_extended_register_w64_extend2;

        if (decoded->operand_width == 32 && decoded->option == 3) return emu_execute_addsub_extended_sub_extended_register_w32_extend3;

        if (decoded->operand_width == 64 && decoded->option == 3) return emu_execute_addsub_extended_sub_extended_register_w64_extend3;

        if (decoded->operand_width == 32 && decoded->option == 4) return emu_execute_addsub_extended_sub_extended_register_w32_extend4;

        if (decoded->operand_width == 64 && decoded->option == 4) return emu_execute_addsub_extended_sub_extended_register_w64_extend4;

        if (decoded->operand_width == 32 && decoded->option == 5) return emu_execute_addsub_extended_sub_extended_register_w32_extend5;

        if (decoded->operand_width == 64 && decoded->option == 5) return emu_execute_addsub_extended_sub_extended_register_w64_extend5;

        if (decoded->operand_width == 32 && decoded->option == 6) return emu_execute_addsub_extended_sub_extended_register_w32_extend6;

        if (decoded->operand_width == 64 && decoded->option == 6) return emu_execute_addsub_extended_sub_extended_register_w64_extend6;

        if (decoded->operand_width == 32 && decoded->option == 7) return emu_execute_addsub_extended_sub_extended_register_w32_extend7;

        if (decoded->operand_width == 64 && decoded->option == 7) return emu_execute_addsub_extended_sub_extended_register_w64_extend7;

        return NULL;

    case ARM64_INSN_SUBS_EXTENDED_REGISTER:

        if (decoded->operand_width == 32 && decoded->option == 0) return emu_execute_addsub_extended_subs_extended_register_w32_extend0;

        if (decoded->operand_width == 64 && decoded->option == 0) return emu_execute_addsub_extended_subs_extended_register_w64_extend0;

        if (decoded->operand_width == 32 && decoded->option == 1) return emu_execute_addsub_extended_subs_extended_register_w32_extend1;

        if (decoded->operand_width == 64 && decoded->option == 1) return emu_execute_addsub_extended_subs_extended_register_w64_extend1;

        if (decoded->operand_width == 32 && decoded->option == 2) return emu_execute_addsub_extended_subs_extended_register_w32_extend2;

        if (decoded->operand_width == 64 && decoded->option == 2) return emu_execute_addsub_extended_subs_extended_register_w64_extend2;

        if (decoded->operand_width == 32 && decoded->option == 3) return emu_execute_addsub_extended_subs_extended_register_w32_extend3;

        if (decoded->operand_width == 64 && decoded->option == 3) return emu_execute_addsub_extended_subs_extended_register_w64_extend3;

        if (decoded->operand_width == 32 && decoded->option == 4) return emu_execute_addsub_extended_subs_extended_register_w32_extend4;

        if (decoded->operand_width == 64 && decoded->option == 4) return emu_execute_addsub_extended_subs_extended_register_w64_extend4;

        if (decoded->operand_width == 32 && decoded->option == 5) return emu_execute_addsub_extended_subs_extended_register_w32_extend5;

        if (decoded->operand_width == 64 && decoded->option == 5) return emu_execute_addsub_extended_subs_extended_register_w64_extend5;

        if (decoded->operand_width == 32 && decoded->option == 6) return emu_execute_addsub_extended_subs_extended_register_w32_extend6;

        if (decoded->operand_width == 64 && decoded->option == 6) return emu_execute_addsub_extended_subs_extended_register_w64_extend6;

        if (decoded->operand_width == 32 && decoded->option == 7) return emu_execute_addsub_extended_subs_extended_register_w32_extend7;

        if (decoded->operand_width == 64 && decoded->option == 7) return emu_execute_addsub_extended_subs_extended_register_w64_extend7;

        return NULL;

    case ARM64_INSN_ADC:

        if (decoded->operand_width == 32) return emu_execute_addsub_carry_adc_w32;

        if (decoded->operand_width == 64) return emu_execute_addsub_carry_adc_w64;

        return NULL;

    case ARM64_INSN_ADCS:

        if (decoded->operand_width == 32) return emu_execute_addsub_carry_adcs_w32;

        if (decoded->operand_width == 64) return emu_execute_addsub_carry_adcs_w64;

        return NULL;

    case ARM64_INSN_SBC:

        if (decoded->operand_width == 32) return emu_execute_addsub_carry_sbc_w32;

        if (decoded->operand_width == 64) return emu_execute_addsub_carry_sbc_w64;

        return NULL;

    case ARM64_INSN_SBCS:

        if (decoded->operand_width == 32) return emu_execute_addsub_carry_sbcs_w32;

        if (decoded->operand_width == 64) return emu_execute_addsub_carry_sbcs_w64;

        return NULL;

    case ARM64_INSN_CSEL:

        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_execute_cond_select_csel_w32_cond0;

        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_execute_cond_select_csel_w32_cond1;

        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_execute_cond_select_csel_w32_cond2;

        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_execute_cond_select_csel_w32_cond3;

        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_execute_cond_select_csel_w32_cond4;

        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_execute_cond_select_csel_w32_cond5;

        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_execute_cond_select_csel_w32_cond6;

        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_execute_cond_select_csel_w32_cond7;

        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_execute_cond_select_csel_w32_cond8;

        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_execute_cond_select_csel_w32_cond9;

        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_execute_cond_select_csel_w32_cond10;

        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_execute_cond_select_csel_w32_cond11;

        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_execute_cond_select_csel_w32_cond12;

        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_execute_cond_select_csel_w32_cond13;

        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_execute_cond_select_csel_w32_cond14;

        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_execute_cond_select_csel_w32_cond15;

        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_execute_cond_select_csel_w64_cond0;

        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_execute_cond_select_csel_w64_cond1;

        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_execute_cond_select_csel_w64_cond2;

        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_execute_cond_select_csel_w64_cond3;

        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_execute_cond_select_csel_w64_cond4;

        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_execute_cond_select_csel_w64_cond5;

        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_execute_cond_select_csel_w64_cond6;

        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_execute_cond_select_csel_w64_cond7;

        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_execute_cond_select_csel_w64_cond8;

        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_execute_cond_select_csel_w64_cond9;

        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_execute_cond_select_csel_w64_cond10;

        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_execute_cond_select_csel_w64_cond11;

        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_execute_cond_select_csel_w64_cond12;

        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_execute_cond_select_csel_w64_cond13;

        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_execute_cond_select_csel_w64_cond14;

        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_execute_cond_select_csel_w64_cond15;

        return NULL;

    case ARM64_INSN_CSINC:

        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_execute_cond_select_csinc_w32_cond0;

        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_execute_cond_select_csinc_w32_cond1;

        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_execute_cond_select_csinc_w32_cond2;

        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_execute_cond_select_csinc_w32_cond3;

        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_execute_cond_select_csinc_w32_cond4;

        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_execute_cond_select_csinc_w32_cond5;

        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_execute_cond_select_csinc_w32_cond6;

        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_execute_cond_select_csinc_w32_cond7;

        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_execute_cond_select_csinc_w32_cond8;

        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_execute_cond_select_csinc_w32_cond9;

        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_execute_cond_select_csinc_w32_cond10;

        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_execute_cond_select_csinc_w32_cond11;

        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_execute_cond_select_csinc_w32_cond12;

        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_execute_cond_select_csinc_w32_cond13;

        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_execute_cond_select_csinc_w32_cond14;

        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_execute_cond_select_csinc_w32_cond15;

        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_execute_cond_select_csinc_w64_cond0;

        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_execute_cond_select_csinc_w64_cond1;

        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_execute_cond_select_csinc_w64_cond2;

        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_execute_cond_select_csinc_w64_cond3;

        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_execute_cond_select_csinc_w64_cond4;

        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_execute_cond_select_csinc_w64_cond5;

        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_execute_cond_select_csinc_w64_cond6;

        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_execute_cond_select_csinc_w64_cond7;

        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_execute_cond_select_csinc_w64_cond8;

        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_execute_cond_select_csinc_w64_cond9;

        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_execute_cond_select_csinc_w64_cond10;

        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_execute_cond_select_csinc_w64_cond11;

        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_execute_cond_select_csinc_w64_cond12;

        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_execute_cond_select_csinc_w64_cond13;

        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_execute_cond_select_csinc_w64_cond14;

        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_execute_cond_select_csinc_w64_cond15;

        return NULL;

    case ARM64_INSN_CSINV:

        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_execute_cond_select_csinv_w32_cond0;

        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_execute_cond_select_csinv_w32_cond1;

        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_execute_cond_select_csinv_w32_cond2;

        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_execute_cond_select_csinv_w32_cond3;

        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_execute_cond_select_csinv_w32_cond4;

        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_execute_cond_select_csinv_w32_cond5;

        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_execute_cond_select_csinv_w32_cond6;

        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_execute_cond_select_csinv_w32_cond7;

        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_execute_cond_select_csinv_w32_cond8;

        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_execute_cond_select_csinv_w32_cond9;

        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_execute_cond_select_csinv_w32_cond10;

        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_execute_cond_select_csinv_w32_cond11;

        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_execute_cond_select_csinv_w32_cond12;

        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_execute_cond_select_csinv_w32_cond13;

        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_execute_cond_select_csinv_w32_cond14;

        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_execute_cond_select_csinv_w32_cond15;

        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_execute_cond_select_csinv_w64_cond0;

        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_execute_cond_select_csinv_w64_cond1;

        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_execute_cond_select_csinv_w64_cond2;

        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_execute_cond_select_csinv_w64_cond3;

        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_execute_cond_select_csinv_w64_cond4;

        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_execute_cond_select_csinv_w64_cond5;

        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_execute_cond_select_csinv_w64_cond6;

        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_execute_cond_select_csinv_w64_cond7;

        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_execute_cond_select_csinv_w64_cond8;

        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_execute_cond_select_csinv_w64_cond9;

        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_execute_cond_select_csinv_w64_cond10;

        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_execute_cond_select_csinv_w64_cond11;

        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_execute_cond_select_csinv_w64_cond12;

        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_execute_cond_select_csinv_w64_cond13;

        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_execute_cond_select_csinv_w64_cond14;

        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_execute_cond_select_csinv_w64_cond15;

        return NULL;

    case ARM64_INSN_CSNEG:

        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_execute_cond_select_csneg_w32_cond0;

        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_execute_cond_select_csneg_w32_cond1;

        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_execute_cond_select_csneg_w32_cond2;

        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_execute_cond_select_csneg_w32_cond3;

        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_execute_cond_select_csneg_w32_cond4;

        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_execute_cond_select_csneg_w32_cond5;

        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_execute_cond_select_csneg_w32_cond6;

        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_execute_cond_select_csneg_w32_cond7;

        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_execute_cond_select_csneg_w32_cond8;

        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_execute_cond_select_csneg_w32_cond9;

        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_execute_cond_select_csneg_w32_cond10;

        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_execute_cond_select_csneg_w32_cond11;

        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_execute_cond_select_csneg_w32_cond12;

        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_execute_cond_select_csneg_w32_cond13;

        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_execute_cond_select_csneg_w32_cond14;

        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_execute_cond_select_csneg_w32_cond15;

        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_execute_cond_select_csneg_w64_cond0;

        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_execute_cond_select_csneg_w64_cond1;

        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_execute_cond_select_csneg_w64_cond2;

        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_execute_cond_select_csneg_w64_cond3;

        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_execute_cond_select_csneg_w64_cond4;

        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_execute_cond_select_csneg_w64_cond5;

        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_execute_cond_select_csneg_w64_cond6;

        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_execute_cond_select_csneg_w64_cond7;

        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_execute_cond_select_csneg_w64_cond8;

        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_execute_cond_select_csneg_w64_cond9;

        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_execute_cond_select_csneg_w64_cond10;

        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_execute_cond_select_csneg_w64_cond11;

        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_execute_cond_select_csneg_w64_cond12;

        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_execute_cond_select_csneg_w64_cond13;

        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_execute_cond_select_csneg_w64_cond14;

        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_execute_cond_select_csneg_w64_cond15;

        return NULL;

    case ARM64_INSN_UDIV:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_udiv_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_udiv_w64;

        return NULL;

    case ARM64_INSN_SDIV:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_sdiv_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_sdiv_w64;

        return NULL;

    case ARM64_INSN_LSLV:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_lslv_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_lslv_w64;

        return NULL;

    case ARM64_INSN_LSRV:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_lsrv_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_lsrv_w64;

        return NULL;

    case ARM64_INSN_ASRV:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_asrv_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_asrv_w64;

        return NULL;

    case ARM64_INSN_RORV:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_rorv_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_rorv_w64;

        return NULL;

    case ARM64_INSN_CRC32B:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_crc32b_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_crc32b_w64;

        return NULL;

    case ARM64_INSN_CRC32H:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_crc32h_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_crc32h_w64;

        return NULL;

    case ARM64_INSN_CRC32W:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_crc32w_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_crc32w_w64;

        return NULL;

    case ARM64_INSN_CRC32X:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_crc32x_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_crc32x_w64;

        return NULL;

    case ARM64_INSN_CRC32CB:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_crc32cb_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_crc32cb_w64;

        return NULL;

    case ARM64_INSN_CRC32CH:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_crc32ch_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_crc32ch_w64;

        return NULL;

    case ARM64_INSN_CRC32CW:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_crc32cw_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_crc32cw_w64;

        return NULL;

    case ARM64_INSN_CRC32CX:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_crc32cx_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_crc32cx_w64;

        return NULL;

    case ARM64_INSN_SMAX_REGISTER:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_smax_register_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_smax_register_w64;

        return NULL;

    case ARM64_INSN_UMAX_REGISTER:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_umax_register_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_umax_register_w64;

        return NULL;

    case ARM64_INSN_SMIN_REGISTER:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_smin_register_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_smin_register_w64;

        return NULL;

    case ARM64_INSN_UMIN_REGISTER:

        if (decoded->operand_width == 32) return emu_execute_integer_binary_umin_register_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_binary_umin_register_w64;

        return NULL;

    case ARM64_INSN_MADD:

        if (decoded->operand_width == 32) return emu_execute_multiply_madd_w32;

        if (decoded->operand_width == 64) return emu_execute_multiply_madd_w64;

        return NULL;

    case ARM64_INSN_MSUB:

        if (decoded->operand_width == 32) return emu_execute_multiply_msub_w32;

        if (decoded->operand_width == 64) return emu_execute_multiply_msub_w64;

        return NULL;

    case ARM64_INSN_SMADDL:

        if (decoded->operand_width == 32) return emu_execute_multiply_smaddl_w32;

        if (decoded->operand_width == 64) return emu_execute_multiply_smaddl_w64;

        return NULL;

    case ARM64_INSN_SMSUBL:

        if (decoded->operand_width == 32) return emu_execute_multiply_smsubl_w32;

        if (decoded->operand_width == 64) return emu_execute_multiply_smsubl_w64;

        return NULL;

    case ARM64_INSN_SMULH:

        if (decoded->operand_width == 32) return emu_execute_multiply_smulh_w32;

        if (decoded->operand_width == 64) return emu_execute_multiply_smulh_w64;

        return NULL;

    case ARM64_INSN_UMADDL:

        if (decoded->operand_width == 32) return emu_execute_multiply_umaddl_w32;

        if (decoded->operand_width == 64) return emu_execute_multiply_umaddl_w64;

        return NULL;

    case ARM64_INSN_UMSUBL:

        if (decoded->operand_width == 32) return emu_execute_multiply_umsubl_w32;

        if (decoded->operand_width == 64) return emu_execute_multiply_umsubl_w64;

        return NULL;

    case ARM64_INSN_UMULH:

        if (decoded->operand_width == 32) return emu_execute_multiply_umulh_w32;

        if (decoded->operand_width == 64) return emu_execute_multiply_umulh_w64;

        return NULL;

    case ARM64_INSN_CCMN_REGISTER:

        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_execute_cond_compare_ccmn_register_w32_cond0;

        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_execute_cond_compare_ccmn_register_w32_cond1;

        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_execute_cond_compare_ccmn_register_w32_cond2;

        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_execute_cond_compare_ccmn_register_w32_cond3;

        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_execute_cond_compare_ccmn_register_w32_cond4;

        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_execute_cond_compare_ccmn_register_w32_cond5;

        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_execute_cond_compare_ccmn_register_w32_cond6;

        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_execute_cond_compare_ccmn_register_w32_cond7;

        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_execute_cond_compare_ccmn_register_w32_cond8;

        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_execute_cond_compare_ccmn_register_w32_cond9;

        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_execute_cond_compare_ccmn_register_w32_cond10;

        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_execute_cond_compare_ccmn_register_w32_cond11;

        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_execute_cond_compare_ccmn_register_w32_cond12;

        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_execute_cond_compare_ccmn_register_w32_cond13;

        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_execute_cond_compare_ccmn_register_w32_cond14;

        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_execute_cond_compare_ccmn_register_w32_cond15;

        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_execute_cond_compare_ccmn_register_w64_cond0;

        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_execute_cond_compare_ccmn_register_w64_cond1;

        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_execute_cond_compare_ccmn_register_w64_cond2;

        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_execute_cond_compare_ccmn_register_w64_cond3;

        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_execute_cond_compare_ccmn_register_w64_cond4;

        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_execute_cond_compare_ccmn_register_w64_cond5;

        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_execute_cond_compare_ccmn_register_w64_cond6;

        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_execute_cond_compare_ccmn_register_w64_cond7;

        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_execute_cond_compare_ccmn_register_w64_cond8;

        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_execute_cond_compare_ccmn_register_w64_cond9;

        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_execute_cond_compare_ccmn_register_w64_cond10;

        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_execute_cond_compare_ccmn_register_w64_cond11;

        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_execute_cond_compare_ccmn_register_w64_cond12;

        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_execute_cond_compare_ccmn_register_w64_cond13;

        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_execute_cond_compare_ccmn_register_w64_cond14;

        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_execute_cond_compare_ccmn_register_w64_cond15;

        return NULL;

    case ARM64_INSN_CCMP_REGISTER:

        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_execute_cond_compare_ccmp_register_w32_cond0;

        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_execute_cond_compare_ccmp_register_w32_cond1;

        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_execute_cond_compare_ccmp_register_w32_cond2;

        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_execute_cond_compare_ccmp_register_w32_cond3;

        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_execute_cond_compare_ccmp_register_w32_cond4;

        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_execute_cond_compare_ccmp_register_w32_cond5;

        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_execute_cond_compare_ccmp_register_w32_cond6;

        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_execute_cond_compare_ccmp_register_w32_cond7;

        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_execute_cond_compare_ccmp_register_w32_cond8;

        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_execute_cond_compare_ccmp_register_w32_cond9;

        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_execute_cond_compare_ccmp_register_w32_cond10;

        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_execute_cond_compare_ccmp_register_w32_cond11;

        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_execute_cond_compare_ccmp_register_w32_cond12;

        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_execute_cond_compare_ccmp_register_w32_cond13;

        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_execute_cond_compare_ccmp_register_w32_cond14;

        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_execute_cond_compare_ccmp_register_w32_cond15;

        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_execute_cond_compare_ccmp_register_w64_cond0;

        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_execute_cond_compare_ccmp_register_w64_cond1;

        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_execute_cond_compare_ccmp_register_w64_cond2;

        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_execute_cond_compare_ccmp_register_w64_cond3;

        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_execute_cond_compare_ccmp_register_w64_cond4;

        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_execute_cond_compare_ccmp_register_w64_cond5;

        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_execute_cond_compare_ccmp_register_w64_cond6;

        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_execute_cond_compare_ccmp_register_w64_cond7;

        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_execute_cond_compare_ccmp_register_w64_cond8;

        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_execute_cond_compare_ccmp_register_w64_cond9;

        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_execute_cond_compare_ccmp_register_w64_cond10;

        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_execute_cond_compare_ccmp_register_w64_cond11;

        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_execute_cond_compare_ccmp_register_w64_cond12;

        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_execute_cond_compare_ccmp_register_w64_cond13;

        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_execute_cond_compare_ccmp_register_w64_cond14;

        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_execute_cond_compare_ccmp_register_w64_cond15;

        return NULL;

    case ARM64_INSN_CCMN_IMMEDIATE:

        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_execute_cond_compare_ccmn_immediate_w32_cond0;

        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_execute_cond_compare_ccmn_immediate_w32_cond1;

        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_execute_cond_compare_ccmn_immediate_w32_cond2;

        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_execute_cond_compare_ccmn_immediate_w32_cond3;

        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_execute_cond_compare_ccmn_immediate_w32_cond4;

        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_execute_cond_compare_ccmn_immediate_w32_cond5;

        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_execute_cond_compare_ccmn_immediate_w32_cond6;

        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_execute_cond_compare_ccmn_immediate_w32_cond7;

        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_execute_cond_compare_ccmn_immediate_w32_cond8;

        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_execute_cond_compare_ccmn_immediate_w32_cond9;

        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_execute_cond_compare_ccmn_immediate_w32_cond10;

        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_execute_cond_compare_ccmn_immediate_w32_cond11;

        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_execute_cond_compare_ccmn_immediate_w32_cond12;

        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_execute_cond_compare_ccmn_immediate_w32_cond13;

        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_execute_cond_compare_ccmn_immediate_w32_cond14;

        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_execute_cond_compare_ccmn_immediate_w32_cond15;

        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_execute_cond_compare_ccmn_immediate_w64_cond0;

        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_execute_cond_compare_ccmn_immediate_w64_cond1;

        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_execute_cond_compare_ccmn_immediate_w64_cond2;

        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_execute_cond_compare_ccmn_immediate_w64_cond3;

        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_execute_cond_compare_ccmn_immediate_w64_cond4;

        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_execute_cond_compare_ccmn_immediate_w64_cond5;

        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_execute_cond_compare_ccmn_immediate_w64_cond6;

        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_execute_cond_compare_ccmn_immediate_w64_cond7;

        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_execute_cond_compare_ccmn_immediate_w64_cond8;

        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_execute_cond_compare_ccmn_immediate_w64_cond9;

        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_execute_cond_compare_ccmn_immediate_w64_cond10;

        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_execute_cond_compare_ccmn_immediate_w64_cond11;

        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_execute_cond_compare_ccmn_immediate_w64_cond12;

        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_execute_cond_compare_ccmn_immediate_w64_cond13;

        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_execute_cond_compare_ccmn_immediate_w64_cond14;

        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_execute_cond_compare_ccmn_immediate_w64_cond15;

        return NULL;

    case ARM64_INSN_CCMP_IMMEDIATE:

        if (decoded->operand_width == 32 && decoded->condition == 0) return emu_execute_cond_compare_ccmp_immediate_w32_cond0;

        if (decoded->operand_width == 32 && decoded->condition == 1) return emu_execute_cond_compare_ccmp_immediate_w32_cond1;

        if (decoded->operand_width == 32 && decoded->condition == 2) return emu_execute_cond_compare_ccmp_immediate_w32_cond2;

        if (decoded->operand_width == 32 && decoded->condition == 3) return emu_execute_cond_compare_ccmp_immediate_w32_cond3;

        if (decoded->operand_width == 32 && decoded->condition == 4) return emu_execute_cond_compare_ccmp_immediate_w32_cond4;

        if (decoded->operand_width == 32 && decoded->condition == 5) return emu_execute_cond_compare_ccmp_immediate_w32_cond5;

        if (decoded->operand_width == 32 && decoded->condition == 6) return emu_execute_cond_compare_ccmp_immediate_w32_cond6;

        if (decoded->operand_width == 32 && decoded->condition == 7) return emu_execute_cond_compare_ccmp_immediate_w32_cond7;

        if (decoded->operand_width == 32 && decoded->condition == 8) return emu_execute_cond_compare_ccmp_immediate_w32_cond8;

        if (decoded->operand_width == 32 && decoded->condition == 9) return emu_execute_cond_compare_ccmp_immediate_w32_cond9;

        if (decoded->operand_width == 32 && decoded->condition == 10) return emu_execute_cond_compare_ccmp_immediate_w32_cond10;

        if (decoded->operand_width == 32 && decoded->condition == 11) return emu_execute_cond_compare_ccmp_immediate_w32_cond11;

        if (decoded->operand_width == 32 && decoded->condition == 12) return emu_execute_cond_compare_ccmp_immediate_w32_cond12;

        if (decoded->operand_width == 32 && decoded->condition == 13) return emu_execute_cond_compare_ccmp_immediate_w32_cond13;

        if (decoded->operand_width == 32 && decoded->condition == 14) return emu_execute_cond_compare_ccmp_immediate_w32_cond14;

        if (decoded->operand_width == 32 && decoded->condition == 15) return emu_execute_cond_compare_ccmp_immediate_w32_cond15;

        if (decoded->operand_width == 64 && decoded->condition == 0) return emu_execute_cond_compare_ccmp_immediate_w64_cond0;

        if (decoded->operand_width == 64 && decoded->condition == 1) return emu_execute_cond_compare_ccmp_immediate_w64_cond1;

        if (decoded->operand_width == 64 && decoded->condition == 2) return emu_execute_cond_compare_ccmp_immediate_w64_cond2;

        if (decoded->operand_width == 64 && decoded->condition == 3) return emu_execute_cond_compare_ccmp_immediate_w64_cond3;

        if (decoded->operand_width == 64 && decoded->condition == 4) return emu_execute_cond_compare_ccmp_immediate_w64_cond4;

        if (decoded->operand_width == 64 && decoded->condition == 5) return emu_execute_cond_compare_ccmp_immediate_w64_cond5;

        if (decoded->operand_width == 64 && decoded->condition == 6) return emu_execute_cond_compare_ccmp_immediate_w64_cond6;

        if (decoded->operand_width == 64 && decoded->condition == 7) return emu_execute_cond_compare_ccmp_immediate_w64_cond7;

        if (decoded->operand_width == 64 && decoded->condition == 8) return emu_execute_cond_compare_ccmp_immediate_w64_cond8;

        if (decoded->operand_width == 64 && decoded->condition == 9) return emu_execute_cond_compare_ccmp_immediate_w64_cond9;

        if (decoded->operand_width == 64 && decoded->condition == 10) return emu_execute_cond_compare_ccmp_immediate_w64_cond10;

        if (decoded->operand_width == 64 && decoded->condition == 11) return emu_execute_cond_compare_ccmp_immediate_w64_cond11;

        if (decoded->operand_width == 64 && decoded->condition == 12) return emu_execute_cond_compare_ccmp_immediate_w64_cond12;

        if (decoded->operand_width == 64 && decoded->condition == 13) return emu_execute_cond_compare_ccmp_immediate_w64_cond13;

        if (decoded->operand_width == 64 && decoded->condition == 14) return emu_execute_cond_compare_ccmp_immediate_w64_cond14;

        if (decoded->operand_width == 64 && decoded->condition == 15) return emu_execute_cond_compare_ccmp_immediate_w64_cond15;

        return NULL;

    case ARM64_INSN_RBIT:

        if (decoded->operand_width == 32) return emu_execute_integer_unary_rbit_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_unary_rbit_w64;

        return NULL;

    case ARM64_INSN_REV16:

        if (decoded->operand_width == 32) return emu_execute_integer_unary_rev16_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_unary_rev16_w64;

        return NULL;

    case ARM64_INSN_REV32:

        if (decoded->operand_width == 32) return emu_execute_integer_unary_rev32_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_unary_rev32_w64;

        return NULL;

    case ARM64_INSN_REV64:

        if (decoded->operand_width == 32) return emu_execute_integer_unary_rev64_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_unary_rev64_w64;

        return NULL;

    case ARM64_INSN_CLZ:

        if (decoded->operand_width == 32) return emu_execute_integer_unary_clz_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_unary_clz_w64;

        return NULL;

    case ARM64_INSN_CLS:

        if (decoded->operand_width == 32) return emu_execute_integer_unary_cls_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_unary_cls_w64;

        return NULL;

    case ARM64_INSN_CTZ:

        if (decoded->operand_width == 32) return emu_execute_integer_unary_ctz_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_unary_ctz_w64;

        return NULL;

    case ARM64_INSN_CNT:

        if (decoded->operand_width == 32) return emu_execute_integer_unary_cnt_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_unary_cnt_w64;

        return NULL;

    case ARM64_INSN_ABS:

        if (decoded->operand_width == 32) return emu_execute_integer_unary_abs_w32;

        if (decoded->operand_width == 64) return emu_execute_integer_unary_abs_w64;

        return NULL;

    default:

        return NULL;

    }

}

/* ======================== 数据处理寄存器类：解码结果构建缓存条目 ======================== */

bool emu_build_register_executor(const struct arm64_decoded_insn *decoded, struct arm64_executor_entry *entry)
{
    bool multiply;
    bool conditional_compare;
    bool unary;

    entry->execute = emu_select_register_executor(decoded);
    if (!entry->execute) return false;

    multiply = decoded->instruction == ARM64_INSN_MADD || decoded->instruction == ARM64_INSN_MSUB || decoded->instruction == ARM64_INSN_SMADDL || decoded->instruction == ARM64_INSN_SMSUBL || decoded->instruction == ARM64_INSN_SMULH || decoded->instruction == ARM64_INSN_UMADDL || decoded->instruction == ARM64_INSN_UMSUBL || decoded->instruction == ARM64_INSN_UMULH;
    conditional_compare = decoded->instruction == ARM64_INSN_CCMN_REGISTER || decoded->instruction == ARM64_INSN_CCMP_REGISTER || decoded->instruction == ARM64_INSN_CCMN_IMMEDIATE || decoded->instruction == ARM64_INSN_CCMP_IMMEDIATE;
    unary = decoded->instruction == ARM64_INSN_RBIT || decoded->instruction == ARM64_INSN_REV16 || decoded->instruction == ARM64_INSN_REV32 || decoded->instruction == ARM64_INSN_REV64 || decoded->instruction == ARM64_INSN_CLZ || decoded->instruction == ARM64_INSN_CLS || decoded->instruction == ARM64_INSN_CTZ || decoded->instruction == ARM64_INSN_CNT || decoded->instruction == ARM64_INSN_ABS;

    entry->operand0 = decoded->immediate;
    entry->reg0 = decoded->rn;
    entry->reg1 = unary ? decoded->rd : decoded->rm;
    entry->reg2 = multiply ? decoded->ra : conditional_compare ? decoded->condition : decoded->rd;
    entry->reg3 = multiply ? decoded->rd : conditional_compare ? decoded->nzcv : decoded->condition;
    entry->option0 = decoded->shift_amount;
    return true;
}
