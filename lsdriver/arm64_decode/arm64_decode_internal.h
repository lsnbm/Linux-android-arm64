#ifndef ARM64_DECODE_INTERNAL_H
#define ARM64_DECODE_INTERNAL_H

#include "arm64_decode.h"

static inline arm64_s64 arm64_sign_extend(arm64_u64 value, arm64_u8 bits)
{
    arm64_u64 sign = 1ULL << (bits - 1);

    return (arm64_s64)((value ^ sign) - sign);
}

#endif