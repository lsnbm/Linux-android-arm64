#include "arm64_decode.h"

enum arm64_decode_status arm64_decode_data_processing_immediate(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_data_processing_register(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_ldst(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_branch(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_simd(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_sve(arm64_u32 raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_sme(arm64_u32 raw, struct arm64_decoded_insn *decoded);

void arm64_decode_insn(arm64_u32 raw, struct arm64_decoded_insn *decoded)
{
    enum arm64_decode_status status;

    /* 直接写入调用方的最终对象，避免大结构体返回临时槽及其复制。 */
    __builtin_memset(decoded, 0, sizeof(*decoded));
    decoded->raw = raw;

    if ((raw & 0xFFFF0000U) == 0)
    {
        status = arm64_decode_branch(raw, decoded);
    }
    else
    {
        /* A64 主编码 raw[28:25] 直接确定唯一子解码器。 */
        arm64_u32 op0 = (raw >> 25) & 0xF;
        switch (op0)
        {
        case 0x0:
            status = arm64_decode_sme(raw, decoded);
            break;
        case 0x2:
            status = arm64_decode_sve(raw, decoded);
            break;
        case 0x5:
        case 0xD:
            status = arm64_decode_data_processing_register(raw, decoded);
            break;
        case 0x8:
        case 0x9:
            status = arm64_decode_data_processing_immediate(raw, decoded);
            break;
        case 0xA:
        case 0xB:
            status = arm64_decode_branch(raw, decoded);
            break;
        case 0x4:
        case 0x6:
        case 0xC:
        case 0xE:
            status = arm64_decode_ldst(raw, decoded);
            break;
        case 0x7:
        case 0xF:
            status = arm64_decode_simd(raw, decoded);
            break;
        default:
            status = ARM64_DECODE_UNALLOCATED;
            break;
        }
    }

    decoded->status = status;
}