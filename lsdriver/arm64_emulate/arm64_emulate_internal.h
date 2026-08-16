#ifndef ARM64_EMULATE_INTERNAL_H
#define ARM64_EMULATE_INTERNAL_H

#include "emulate_insn.h"
#include "../arm64_decode/arm64_decode.h"
#include "arm64_emulate_hw_templates.h"

/*
缓存条目只保存由机器码决定的静态信息。当前寄存器值、有效地址、条件判断结果、
FPCR/FPSR 和 exclusive monitor 等运行时状态仍由 executor 每次执行时读取。
*/
struct arm64_executor_entry
{
    enum emu_insn_result (*execute)(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry);
    uint64_t operand0;
    uint64_t operand1;
    uint8_t reg0;
    uint8_t reg1;
    uint8_t reg2;
    uint8_t reg3;
    uint8_t reg4;
    uint8_t reg5;
    uint8_t option0;
    uint8_t option1;
};

_Static_assert(sizeof(struct arm64_executor_entry) == 32, "arm64 executor cache entry must remain 32 bytes");

bool emu_build_executor_entry(const struct arm64_decoded_insn *decoded, struct arm64_executor_entry *entry);
__nocfi enum emu_insn_result emu_execute_executor_entry(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry);

bool emu_build_branch_executor(const struct arm64_decoded_insn *decoded, struct arm64_executor_entry *entry);
bool emu_build_ldst_executor(const struct arm64_decoded_insn *decoded, struct arm64_executor_entry *entry);
bool emu_build_simd_executor(const struct arm64_decoded_insn *decoded, struct arm64_executor_entry *entry);
bool emu_build_immediate_executor(const struct arm64_decoded_insn *decoded, struct arm64_executor_entry *entry);
bool emu_build_register_executor(const struct arm64_decoded_insn *decoded, struct arm64_executor_entry *entry);

/* ========== 数据处理立即数类与寄存器类共享：固定硬件模板与纯运算辅助 ========== */

// clang-format off
static inline bool emu_addsub_hw(enum arm64_instruction instruction, uint64_t a, uint64_t b, bool sf, uint64_t *result, uint64_t *nzcv, bool *setflags)
{
    if (!result || !nzcv || !setflags) return false;
    *setflags = false;
    switch (instruction)
    {
    case ARM64_INSN_ADD_IMMEDIATE:
    case ARM64_INSN_ADD_SHIFTED_REGISTER:
    case ARM64_INSN_ADD_EXTENDED_REGISTER:
        *result = sf ? emu_template_add_w64(a, b) : emu_template_add_w32(a, b);
        return true;
    case ARM64_INSN_ADDS_IMMEDIATE:
    case ARM64_INSN_ADDS_SHIFTED_REGISTER:
    case ARM64_INSN_ADDS_EXTENDED_REGISTER:
    case ARM64_INSN_CCMN_REGISTER:
    case ARM64_INSN_CCMN_IMMEDIATE:
        *result = sf ? emu_template_adds_w64(a, b, nzcv) : emu_template_adds_w32(a, b, nzcv);
        *setflags = true;
        return true;
    case ARM64_INSN_SUB_IMMEDIATE:
    case ARM64_INSN_SUB_SHIFTED_REGISTER:
    case ARM64_INSN_SUB_EXTENDED_REGISTER:
        *result = sf ? emu_template_sub_w64(a, b) : emu_template_sub_w32(a, b);
        return true;
    case ARM64_INSN_SUBS_IMMEDIATE:
    case ARM64_INSN_SUBS_SHIFTED_REGISTER:
    case ARM64_INSN_SUBS_EXTENDED_REGISTER:
    case ARM64_INSN_CCMP_REGISTER:
    case ARM64_INSN_CCMP_IMMEDIATE:
        *result = sf ? emu_template_subs_w64(a, b, nzcv) : emu_template_subs_w32(a, b, nzcv);
        *setflags = true;
        return true;
    default:
        return false;
    }
}

static inline uint64_t emu_logic_hw(enum arm64_instruction instruction, uint64_t a, uint64_t b, bool sf, uint64_t *nzcv, bool *setflags)
{
    *setflags = false;

    switch (instruction)
    {
    case ARM64_INSN_AND_IMMEDIATE:
    case ARM64_INSN_AND_SHIFTED_REGISTER:
        return sf ? emu_template_and_w64(a, b) : emu_template_and_w32(a, b);
    case ARM64_INSN_BIC_SHIFTED_REGISTER:
        return sf ? emu_template_bic_w64(a, b) : emu_template_bic_w32(a, b);
    case ARM64_INSN_ORR_IMMEDIATE:
    case ARM64_INSN_ORR_SHIFTED_REGISTER:
        return sf ? emu_template_orr_w64(a, b) : emu_template_orr_w32(a, b);
    case ARM64_INSN_ORN_SHIFTED_REGISTER:
        return sf ? emu_template_orn_w64(a, b) : emu_template_orn_w32(a, b);
    case ARM64_INSN_EOR_IMMEDIATE:
    case ARM64_INSN_EOR_SHIFTED_REGISTER:
        return sf ? emu_template_eor_w64(a, b) : emu_template_eor_w32(a, b);
    case ARM64_INSN_EON_SHIFTED_REGISTER:
        return sf ? emu_template_eon_w64(a, b) : emu_template_eon_w32(a, b);
    case ARM64_INSN_ANDS_IMMEDIATE:
    case ARM64_INSN_ANDS_SHIFTED_REGISTER:
        *setflags = true;
        return sf ? emu_template_ands_w64(a, b, nzcv) : emu_template_ands_w32(a, b, nzcv);
    case ARM64_INSN_BICS_SHIFTED_REGISTER:
        *setflags = true;
        return sf ? emu_template_bics_w64(a, b, nzcv) : emu_template_bics_w32(a, b, nzcv);
    default:
        return 0;
    }
}
// clang-format on

static inline uint64_t emu_dp_mask(bool sf)
{
    return sf ? ~0ULL : 0xFFFFFFFFULL;
}

// clang-format off
static inline bool emu_integer_binary_hw(enum arm64_instruction instruction, uint64_t a, uint64_t signed_b, uint64_t unsigned_b, bool sf, uint64_t *result)
{
    if (!result) return false;

    /* MIN/MAX immediate 早于 two-source；仅有 64 位编码的 CRC32X/CX 排在 32 位 CRC 之后。 */
    switch (instruction)
    {
    case ARM64_INSN_SMAX_IMMEDIATE:
    case ARM64_INSN_SMAX_REGISTER:
        *result = sf ? emu_template_smax_w64(a, signed_b) : emu_template_smax_w32(a, signed_b);
        return true;
    case ARM64_INSN_UMAX_IMMEDIATE:
    case ARM64_INSN_UMAX_REGISTER:
        *result = sf ? emu_template_umax_w64(a, unsigned_b) : emu_template_umax_w32(a, unsigned_b);
        return true;
    case ARM64_INSN_SMIN_IMMEDIATE:
    case ARM64_INSN_SMIN_REGISTER:
        *result = sf ? emu_template_smin_w64(a, signed_b) : emu_template_smin_w32(a, signed_b);
        return true;
    case ARM64_INSN_UMIN_IMMEDIATE:
    case ARM64_INSN_UMIN_REGISTER:
        *result = sf ? emu_template_umin_w64(a, unsigned_b) : emu_template_umin_w32(a, unsigned_b);
        return true;
    case ARM64_INSN_UDIV:
        *result = sf ? emu_template_udiv_w64(a, unsigned_b) : emu_template_udiv_w32(a, unsigned_b);
        return true;
    case ARM64_INSN_SDIV:
        *result = sf ? emu_template_sdiv_w64(a, signed_b) : emu_template_sdiv_w32(a, signed_b);
        return true;
    case ARM64_INSN_LSLV:
        *result = sf ? emu_template_lslv_w64(a, unsigned_b) : emu_template_lslv_w32(a, unsigned_b);
        return true;
    case ARM64_INSN_LSRV:
        *result = sf ? emu_template_lsrv_w64(a, unsigned_b) : emu_template_lsrv_w32(a, unsigned_b);
        return true;
    case ARM64_INSN_ASRV:
        *result = sf ? emu_template_asrv_w64(a, unsigned_b) : emu_template_asrv_w32(a, unsigned_b);
        return true;
    case ARM64_INSN_RORV:
        *result = sf ? emu_template_rorv_w64(a, unsigned_b) : emu_template_rorv_w32(a, unsigned_b);
        return true;
    case ARM64_INSN_CRC32B:
        *result = emu_template_crc32b((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32H:
        *result = emu_template_crc32h((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32W:
        *result = emu_template_crc32w((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32CB:
        *result = emu_template_crc32cb((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32CH:
        *result = emu_template_crc32ch((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32CW:
        *result = emu_template_crc32cw((uint32_t)a, (uint32_t)unsigned_b);
        return true;
    case ARM64_INSN_CRC32X:
        *result = emu_template_crc32x((uint32_t)a, unsigned_b);
        return true;
    case ARM64_INSN_CRC32CX:
        *result = emu_template_crc32cx((uint32_t)a, unsigned_b);
        return true;
    default:
        return false;
    }
}
// clang-format on

#endif // ARM64_EMULATE_INTERNAL_H
