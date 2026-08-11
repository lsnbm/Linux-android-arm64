#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include "../arm64_encode/arm64_encode.h"

static void expect_word(const char *name, int status, uint32_t actual, uint32_t expected)
{
    if (status || actual != expected)
    {
        fprintf(stderr, "%s: status=%d actual=%08x expected=%08x\n", name, status, actual, expected);
        exit(1);
    }
}

static void expect_status(const char *name, int actual, int expected)
{
    if (actual != expected)
    {
        fprintf(stderr, "%s: status=%d expected=%d\n", name, actual, expected);
        exit(1);
    }
}

#define EXPECT_ENCODE(name, expected, expression)         \
    do                                                    \
    {                                                     \
        instruction = 0;                                  \
        int status = (expression);                        \
        expect_word(name, status, instruction, expected); \
    } while (0)

int main(void)
{
    uint32_t instruction = 0;
    uint32_t jump[6];

    EXPECT_ENCODE("nop", 0xD503201F, arm64_encode_nop(&instruction));
    EXPECT_ENCODE("bti c", 0xD503245F, arm64_encode_bti_c(&instruction));
    EXPECT_ENCODE("brk #0", 0xD4200000, arm64_encode_brk(0, &instruction));
    EXPECT_ENCODE("brk #4", 0xD4200080, arm64_encode_brk(4, &instruction));
    EXPECT_ENCODE("brk #65535", 0xD43FFFE0, arm64_encode_brk(0xFFFF, &instruction));
    EXPECT_ENCODE("b forward", 0x14000040, arm64_encode_b(0x1000, 0x1100, &instruction));
    EXPECT_ENCODE("bl backward", 0x97FFFFC0, arm64_encode_bl(0x1100, 0x1000, &instruction));
    EXPECT_ENCODE("b.eq", 0x54000440, arm64_encode_b_cond(29 * 4, 63 * 4, 0, &instruction));
    EXPECT_ENCODE("cbnz x3", 0xB5000803, arm64_encode_cbz(0x1000, 0x1100, true, true, 3, &instruction));
    EXPECT_ENCODE("tbz x5 bit 40", 0xB647F805, arm64_encode_tbz(0x1000, 0x0F00, false, 40, 5, &instruction));
    EXPECT_ENCODE("br x16", 0xD61F0200, arm64_encode_br(16, &instruction));
    EXPECT_ENCODE("blr x9", 0xD63F0120, arm64_encode_blr(9, &instruction));
    EXPECT_ENCODE("ret x30", 0xD65F03C0, arm64_encode_ret(30, &instruction));
    EXPECT_ENCODE("adr x7", 0x100011A7, arm64_encode_adr(7, 0x1000, 0x1234, &instruction));
    EXPECT_ENCODE("adrp x8", 0x90000228, arm64_encode_adrp(8, 0x12345000, 0x12389000, &instruction));
    EXPECT_ENCODE("sub sp", 0xD104C3FF, arm64_encode_add_sub_imm(true, true, false, 31, 31, 304, &instruction));
    EXPECT_ENCODE("cmp immediate", 0xF100041F, arm64_encode_add_sub_imm(true, true, true, 31, 0, 1, &instruction));
    EXPECT_ENCODE("cmp register", 0xEB09015F, arm64_encode_add_sub_shifted(true, true, true, 31, 10, 9, ARM64_ENCODE_SHIFT_LSL, 0, &instruction));
    EXPECT_ENCODE("str x30", 0xF9008BFE, arm64_encode_load_store_unsigned(false, ARM64_ENCODE_LS_GPR64, 30, 31, 272, &instruction));
    EXPECT_ENCODE("ldr x17", 0xF9407E11, arm64_encode_load_store_unsigned(true, ARM64_ENCODE_LS_GPR64, 17, 16, 248, &instruction));
    EXPECT_ENCODE("str x16 pre", 0xF81F0FF0, arm64_encode_load_store_x_indexed(false, true, 16, 31, -16, &instruction));
    EXPECT_ENCODE("ldr x16 post", 0xF84107F0, arm64_encode_load_store_x_indexed(true, false, 16, 31, 16, &instruction));
    EXPECT_ENCODE("stp x0 x1", 0xA90207E0, arm64_encode_load_store_pair_x(false, 0, 1, 31, 32, &instruction));
    EXPECT_ENCODE("ldp x28 x29", 0xA94E761C, arm64_encode_load_store_pair_x(true, 28, 29, 16, 224, &instruction));
    EXPECT_ENCODE("ldr literal x9", 0x58000C29, arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 9, 19 * 4, 116 * 4, &instruction));
    EXPECT_ENCODE("mrs nzcv", 0xD53B4209, arm64_encode_mrs(9, 3, 3, 4, 2, 0, &instruction));
    EXPECT_ENCODE("msr nzcv", 0xD51B4209, arm64_encode_msr(9, 3, 3, 4, 2, 0, &instruction));

    expect_status("unaligned b", arm64_encode_b(0x1000, 0x1002, &instruction), -EINVAL);
    expect_status("b out of range", arm64_encode_b(0, 1ULL << 27, &instruction), -ERANGE);
    expect_status("brk immediate out of range", arm64_encode_brk(0x10000, &instruction), -ERANGE);
    expect_status("invalid register", arm64_encode_ret(32, &instruction), -EINVAL);
    expect_status("misaligned pair", arm64_encode_load_store_pair_x(true, 0, 1, 2, 4, &instruction), -EINVAL);

    expect_status("absolute jump", arm64_emit_abs_jump(0x1122334455667788ULL, 16, jump, 6), 0);
    expect_word("absolute jump ldr", 0, jump[0], 0x58000050);
    expect_word("absolute jump ret", 0, jump[1], 0xD65F0200);
    if (jump[2] != 0x55667788 || jump[3] != 0x11223344 || jump[4] != 0xD503201F || jump[5] != 0xD503201F)
    {
        fprintf(stderr, "absolute jump payload mismatch\n");
        return 1;
    }

    return 0;
}