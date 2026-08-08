#ifndef BP_TYPES_H
#define BP_TYPES_H
/*
 * bp_types.h - kernel-internal breakpoint types for the bp driver.
 * Extracted from upstream lsdriver io_struct.h (breakpoint subset only).
 * Never exposed to userspace; the userspace ABI lives in bp_uapi.h.
 * NOTE: on_hit stays a kernel-internal callback slot; it is NOT part of
 * any shared-memory or ioctl protocol in this driver.
 */
#include <linux/types.h>
#include "arm64_reg.h"

#define BP_OP_NONE    0x0 /* 00: no operation */
#define BP_OP_READ    0x1 /* 01: read into record */
#define BP_OP_WRITE   0x2 /* 10: write back from record */
#define BP_CONFIG_MAX 16
#define BP_RECORD_MAX 0x10

#define BP_SET_MASK(record, reg, op)                            \
    do                                                          \
    {                                                           \
        int byte_idx = (reg) >> 2;                              \
        int bit_offset = ((reg) & 0x3) << 1;                    \
        (record)->mask[byte_idx] &= ~(0x3 << bit_offset);       \
        (record)->mask[byte_idx] |= ((op) & 0x3) << bit_offset; \
    } while (0)

#define BP_GET_MASK(record, reg) (((record)->mask[(reg) >> 2] >> (((reg) & 0x3) << 1)) & 0x3)

enum bp_type
{
    BP_BREAKPOINT_EMPTY = 0,
    BP_BREAKPOINT_R = 1,
    BP_BREAKPOINT_W = 2,
    BP_BREAKPOINT_RW = BP_BREAKPOINT_R | BP_BREAKPOINT_W,
    BP_BREAKPOINT_X = 4,
    BP_BREAKPOINT_INVALID = BP_BREAKPOINT_RW | BP_BREAKPOINT_X,
};

enum bp_len
{
    BP_BREAKPOINT_LEN_1 = 1,
    BP_BREAKPOINT_LEN_2 = 2,
    BP_BREAKPOINT_LEN_3 = 3,
    BP_BREAKPOINT_LEN_4 = 4,
    BP_BREAKPOINT_LEN_5 = 5,
    BP_BREAKPOINT_LEN_6 = 6,
    BP_BREAKPOINT_LEN_7 = 7,
    BP_BREAKPOINT_LEN_8 = 8,
};

enum bp_scope
{
    BP_SCOPE_MAIN_THREAD,
    BP_SCOPE_OTHER_THREADS,
    BP_SCOPE_ALL_THREADS
};

enum bp_reg_idx
{
    IDX_PC = 0,
    IDX_HIT_COUNT,
    IDX_LR,
    IDX_SP,
    IDX_ORIG_X0,
    IDX_SYSCALLNO,
    IDX_PSTATE,
    IDX_X0,
    IDX_X1,
    IDX_X2,
    IDX_X3,
    IDX_X4,
    IDX_X5,
    IDX_X6,
    IDX_X7,
    IDX_X8,
    IDX_X9,
    IDX_X10,
    IDX_X11,
    IDX_X12,
    IDX_X13,
    IDX_X14,
    IDX_X15,
    IDX_X16,
    IDX_X17,
    IDX_X18,
    IDX_X19,
    IDX_X20,
    IDX_X21,
    IDX_X22,
    IDX_X23,
    IDX_X24,
    IDX_X25,
    IDX_X26,
    IDX_X27,
    IDX_X28,
    IDX_X29,
    IDX_FPSR,
    IDX_FPCR,
    IDX_Q0,
    IDX_Q1,
    IDX_Q2,
    IDX_Q3,
    IDX_Q4,
    IDX_Q5,
    IDX_Q6,
    IDX_Q7,
    IDX_Q8,
    IDX_Q9,
    IDX_Q10,
    IDX_Q11,
    IDX_Q12,
    IDX_Q13,
    IDX_Q14,
    IDX_Q15,
    IDX_Q16,
    IDX_Q17,
    IDX_Q18,
    IDX_Q19,
    IDX_Q20,
    IDX_Q21,
    IDX_Q22,
    IDX_Q23,
    IDX_Q24,
    IDX_Q25,
    IDX_Q26,
    IDX_Q27,
    IDX_Q28,
    IDX_Q29,
    IDX_Q30,
    IDX_Q31,
    MAX_REG_COUNT
};

struct bp_record
{
    uint8_t mask[18];
    uint64_t hit_count;
    uint64_t pc;
    uint64_t lr;
    uint64_t sp;
    uint64_t orig_x0;
    int32_t syscallno;
    uint64_t pstate;
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9;
    uint64_t x10, x11, x12, x13, x14, x15, x16, x17, x18, x19;
    uint64_t x20, x21, x22, x23, x24, x25, x26, x27, x28, x29;
    uint32_t fpsr;
    uint32_t fpcr;
    __uint128_t q0, q1, q2, q3, q4, q5, q6, q7, q8, q9;
    __uint128_t q10, q11, q12, q13, q14, q15, q16, q17, q18, q19;
    __uint128_t q20, q21, q22, q23, q24, q25, q26, q27, q28, q29;
    __uint128_t q30, q31;
};

struct bp_point
{
    void (*on_hit)(void *regs, void *fp_regs, void *hit_point);
    enum bp_type bt;
    enum bp_len bl;
    enum bp_scope bs;
    uint64_t hit_addr;
    int record_count;
    struct bp_record records[BP_RECORD_MAX];
};

struct break_point
{
    uint64_t num_brps;
    uint64_t num_wrps;
    int tgid;
    struct bp_point points[BP_CONFIG_MAX];
};

#endif /* BP_TYPES_H */
