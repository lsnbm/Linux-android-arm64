#include "arm64_decode.h"

/* 当前只识别 SME encoding space，完整的 SME 操作数语义尚未实现。 */
enum arm64_decode_status arm64_decode_sme(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    /* 经典 SME 主编码空间要求 bit[31] == 1；bit[31] == 0 的 op0=0000
    组合在 A64 顶层表中仍是未分配空间，不能仅凭 op0 将其认领为 SME。 */
    if (!(raw & 0x80000000U)) return ARM64_DECODE_NO_MATCH;

    if (((raw >> 25) & 0xF) != 0) return ARM64_DECODE_NO_MATCH;

    decoded->insn_class = ARM64_INSN_CLASS_SME;
    decoded->opcode = ARM64_OP_SME;
    return ARM64_DECODE_UNSUPPORTED;
}