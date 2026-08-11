#include "arm64_decode.h"

/* 当前只识别 SVE 编码空间，完整的 SVE 操作数语义尚未实现。 */
enum arm64_decode_status arm64_decode_sve(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    (void)raw;
    decoded->insn_class = ARM64_INSN_CLASS_SVE;
    return ARM64_DECODE_UNSUPPORTED;
}