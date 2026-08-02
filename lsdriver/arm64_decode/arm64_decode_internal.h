#ifndef ARM64_DECODE_INTERNAL_H
#define ARM64_DECODE_INTERNAL_H

#include "arm64_decode.h"

static inline int64_t arm64_sign_extend(uint64_t value, uint8_t bits)
{
    uint64_t sign = 1ULL << (bits - 1);

    return (int64_t)((value ^ sign) - sign);
}

#endif