#include "arm64_decode.h"

/* 当前只识别 SVE 编码空间，完整的 SVE 操作数语义尚未实现。 */
enum arm64_decode_status arm64_decode_sve(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    if ((raw & 0x1E000000U) != 0x04000000U) return ARM64_DECODE_NO_MATCH;

    decoded->insn_class = ARM64_INSN_CLASS_SVE;
    decoded->opcode = ARM64_OP_SVE;
    decoded->rd = raw & 0x1F;
    decoded->rn = (raw >> 5) & 0x1F;
    decoded->rm = (raw >> 16) & 0x1F;
    return ARM64_DECODE_UNSUPPORTED;
}