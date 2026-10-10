#include "../../arm64_encode/arm64_encode.h"
#include <errno.h>
#include <stdio.h>

static int expect_status(const char *case_name, int actual, int expected)
{
    if (actual == expected) return 0;
    fprintf(stderr, "%s: status=%d expected=%d\n", case_name, actual, expected);
    return 1;
}

static int expect_word(const char *case_name, uint32_t actual, uint32_t expected)
{
    if (actual == expected) return 0;
    fprintf(stderr, "%s: word=%08x expected=%08x\n", case_name, actual, expected);
    return 1;
}

static int expect_words(const char *case_name, const uint32_t *actual, const uint32_t *expected, size_t word_count)
{
    for (size_t index = 0; index < word_count; index++)
    {
        if (actual[index] == expected[index]) continue;
        fprintf(stderr, "%s: word[%zu]=%08x expected=%08x\n", case_name, index, actual[index], expected[index]);
        return 1;
    }
    return 0;
}

static int check_fixed_instructions(void)
{
    int failures = 0;
    uint32_t instruction = 0;

    failures += expect_status("nop", arm64_encode_nop(&instruction), 0);
    failures += expect_word("nop", instruction, 0xD503201FU);
    failures += expect_status("bti c", arm64_encode_bti_c(&instruction), 0);
    failures += expect_word("bti c", instruction, 0xD503245FU);
    failures += expect_status("brk #0xffff", arm64_encode_brk(0xFFFFU, &instruction), 0);
    failures += expect_word("brk #0xffff", instruction, 0xD43FFFE0U);
    failures += expect_status("brk range", arm64_encode_brk(0x10000U, &instruction), -ERANGE);
    failures += expect_status("br x7", arm64_encode_br(7, &instruction), 0);
    failures += expect_word("br x7", instruction, 0xD61F00E0U);
    failures += expect_status("blr x8", arm64_encode_blr(8, &instruction), 0);
    failures += expect_word("blr x8", instruction, 0xD63F0100U);
    failures += expect_status("ret x9", arm64_encode_ret(9, &instruction), 0);
    failures += expect_word("ret x9", instruction, 0xD65F0120U);
    failures += expect_status("nop null", arm64_encode_nop(NULL), -EINVAL);
    return failures;
}

static int check_pc_relative_branches(void)
{
    int failures = 0;
    uint32_t instruction = 0;

    failures += expect_status("b +8", arm64_encode_b(0x1000, 0x1008, &instruction), 0);
    failures += expect_word("b +8", instruction, 0x14000002U);
    failures += expect_status("bl +8", arm64_encode_bl(0x1000, 0x1008, &instruction), 0);
    failures += expect_word("bl +8", instruction, 0x94000002U);
    failures += expect_status("b.eq +8", arm64_encode_b_cond(0, 8, 0, &instruction), 0);
    failures += expect_word("b.eq +8", instruction, 0x54000040U);
    failures += expect_status("cbnz x2 +8", arm64_encode_cbnz(0x10, 0x18, true, 2, &instruction), 0);
    failures += expect_word("cbnz x2 +8", instruction, 0xB5000042U);
    failures += expect_status("tbz x0 bit32 +8", arm64_encode_tbz(8, 16, 32, 0, &instruction), 0);
    failures += expect_word("tbz x0 bit32 +8", instruction, 0xB6000040U);
    failures += expect_status("tbnz x1 bit63 -8", arm64_encode_tbnz(12, 4, 63, 1, &instruction), 0);
    failures += expect_word("tbnz x1 bit63 -8", instruction, 0xB7FFFFC1U);
    failures += expect_status("b unaligned", arm64_encode_b(0, 2, &instruction), -EINVAL);
    failures += expect_status("b out of range", arm64_encode_b(0, (1ULL << 27), &instruction), -ERANGE);
    failures += expect_status("b address wrap", arm64_encode_b(0xFFFFFFFFFFFFFFFCULL, 0, &instruction), 0);
    failures += expect_word("b address wrap", instruction, 0x14000001U);
    failures += expect_status("b.cond invalid condition", arm64_encode_b_cond(0, 4, 16, &instruction), -EINVAL);
    failures += expect_status("tbz invalid bit", arm64_encode_tbz(0, 4, 64, 0, &instruction), -EINVAL);
    return failures;
}

static int check_pc_addresses(void)
{
    int failures = 0;
    uint32_t instruction = 0;

    failures += expect_status("adr x10 +0x20", arm64_encode_adr(10, 0, 0x20, &instruction), 0);
    failures += expect_word("adr x10 +0x20", instruction, 0x1000010AU);
    failures += expect_status("adr unaligned target", arm64_encode_adr(0, 0, 0x21, &instruction), 0);
    failures += expect_word("adr unaligned target", instruction, 0x30000100U);
    failures += expect_status("adrp x11 +1 page", arm64_encode_adrp(11, 0, 0x1000, &instruction), 0);
    failures += expect_word("adrp x11 +1 page", instruction, 0xB000000BU);
    failures += expect_status("adr unaligned pc", arm64_encode_adr(0, 1, 0x100, &instruction), -EINVAL);
    failures += expect_status("adrp unaligned pc", arm64_encode_adrp(0, 1, 0x1000, &instruction), -EINVAL);
    failures += expect_status("adr out of range", arm64_encode_adr(0, 0, 1LL << 20, &instruction), -ERANGE);
    failures += expect_status("adrp out of range", arm64_encode_adrp(0, 0, 1ULL << 32, &instruction), -ERANGE);
    return failures;
}

static int check_integer_instructions(void)
{
    int failures = 0;
    uint32_t instruction = 0;

    failures += expect_status("add x12,x13,#0x123", arm64_encode_add_sub_imm(true, false, false, 12, 13, 0x123, &instruction), 0);
    failures += expect_word("add x12,x13,#0x123", instruction, 0x91048DACU);
    failures += expect_status("sub w14,w15,#0x1000", arm64_encode_add_sub_imm(false, true, false, 14, 15, 0x1000, &instruction), 0);
    failures += expect_word("sub w14,w15,#0x1000", instruction, 0x514005EEU);
    failures += expect_status("add x16,x17,x18,asr #63", arm64_encode_add_sub_shifted(true, false, false, 16, 17, 18, ARM64_ENCODE_SHIFT_ASR, 63, &instruction), 0);
    failures += expect_word("add x16,x17,x18,asr #63", instruction, 0x8B92FE30U);
    failures += expect_status("add immediate alignment", arm64_encode_add_sub_imm(true, false, false, 0, 0, 0x1001, &instruction), -ERANGE);
    failures += expect_status("add immediate range", arm64_encode_add_sub_imm(true, false, false, 0, 0, 0x1000000, &instruction), -ERANGE);
    failures += expect_status("shift amount 64", arm64_encode_add_sub_shifted(true, false, false, 0, 0, 0, ARM64_ENCODE_SHIFT_LSL, 64, &instruction), -EINVAL);
    failures += expect_status("shift amount 32", arm64_encode_add_sub_shifted(false, false, false, 0, 0, 0, ARM64_ENCODE_SHIFT_LSL, 32, &instruction), -EINVAL);
    failures += expect_status("invalid shift enum", arm64_encode_add_sub_shifted(true, false, false, 0, 0, 0, (enum arm64_encode_shift)-1, 0, &instruction), -EINVAL);
    return failures;
}

static int check_load_store_instructions(void)
{
    int failures = 0;
    uint32_t instruction = 0;

    failures += expect_status("ldr w19", arm64_encode_load_store_unsigned_offset(true, ARM64_ENCODE_LS_GPR32, 19, 20, 0x10, &instruction), 0);
    failures += expect_word("ldr w19", instruction, 0xB9401293U);
    failures += expect_status("str x21", arm64_encode_load_store_unsigned_offset(false, ARM64_ENCODE_LS_GPR64, 21, 22, 0x20, &instruction), 0);
    failures += expect_word("str x21", instruction, 0xF90012D5U);
    failures += expect_status("ldrsw x23", arm64_encode_load_store_unsigned_offset(true, ARM64_ENCODE_LS_LDRSW, 23, 24, 4, &instruction), 0);
    failures += expect_word("ldrsw x23", instruction, 0xB9800717U);
    failures += expect_status("ldr s25", arm64_encode_load_store_unsigned_offset(true, ARM64_ENCODE_LS_FP32, 25, 26, 4, &instruction), 0);
    failures += expect_word("ldr s25", instruction, 0xBD400759U);
    failures += expect_status("ldr d27", arm64_encode_load_store_unsigned_offset(true, ARM64_ENCODE_LS_FP64, 27, 28, 8, &instruction), 0);
    failures += expect_word("ldr d27", instruction, 0xFD40079BU);
    failures += expect_status("ldr q29", arm64_encode_load_store_unsigned_offset(true, ARM64_ENCODE_LS_SIMD128, 29, 31, 0x10, &instruction), 0);
    failures += expect_word("ldr q29", instruction, 0x3DC007FDU);
    failures += expect_status("ldrsw store invalid", arm64_encode_load_store_unsigned_offset(false, ARM64_ENCODE_LS_LDRSW, 0, 0, 0, &instruction), -EINVAL);
    failures += expect_status("unsigned offset alignment", arm64_encode_load_store_unsigned_offset(true, ARM64_ENCODE_LS_GPR64, 0, 0, 4, &instruction), -EINVAL);
    failures += expect_status("unsigned offset range", arm64_encode_load_store_unsigned_offset(true, ARM64_ENCODE_LS_GPR64, 0, 0, 0x8000, &instruction), -ERANGE);
    failures += expect_status("str x30 pre-index", arm64_encode_load_store_gpr64_pre_post_indexed(false, true, 30, 31, -16, &instruction), 0);
    failures += expect_word("str x30 pre-index", instruction, 0xF81F0FFEU);
    failures += expect_status("ldr x16 post-index", arm64_encode_load_store_gpr64_pre_post_indexed(true, false, 16, 31, 16, &instruction), 0);
    failures += expect_word("ldr x16 post-index", instruction, 0xF84107F0U);
    failures += expect_status("indexed writeback overlap", arm64_encode_load_store_gpr64_pre_post_indexed(false, true, 0, 0, 8, &instruction), -EINVAL);
    failures += expect_status("ldp x0,x1", arm64_encode_load_store_pair_gpr64(true, 0, 1, 2, -16, &instruction), 0);
    failures += expect_word("ldp x0,x1", instruction, 0xA97F0440U);
    failures += expect_status("stp duplicate source", arm64_encode_load_store_pair_gpr64(false, 0, 0, 1, 0, &instruction), 0);
    failures += expect_status("ldp duplicate destination", arm64_encode_load_store_pair_gpr64(true, 0, 0, 1, 0, &instruction), -EINVAL);
    failures += expect_status("pair offset alignment", arm64_encode_load_store_pair_gpr64(true, 0, 1, 2, 4, &instruction), -EINVAL);
    failures += expect_status("pair offset range", arm64_encode_load_store_pair_gpr64(true, 0, 1, 2, 512, &instruction), -ERANGE);
    return failures;
}

static int check_literal_instructions(void)
{
    int failures = 0;
    uint32_t instruction = 0;

    failures += expect_status("ldr x0 literal", arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 0, 0, 8, &instruction), 0);
    failures += expect_word("ldr x0 literal", instruction, 0x58000040U);
    failures += expect_status("ldr w1 literal", arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR32, 1, 0, 8, &instruction), 0);
    failures += expect_word("ldr w1 literal", instruction, 0x18000041U);
    failures += expect_status("ldr q2 literal", arm64_encode_ldr_literal(ARM64_ENCODE_LS_SIMD128, 2, 0, 8, &instruction), 0);
    failures += expect_word("ldr q2 literal", instruction, 0x9C000042U);
    failures += expect_status("prfm literal", arm64_encode_prfm_literal(0, 0, 8, &instruction), 0);
    failures += expect_word("prfm literal", instruction, 0xD8000040U);
    failures += expect_status("literal unaligned pc", arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 0, 2, 8, &instruction), -EINVAL);
    failures += expect_status("prfm operation range", arm64_encode_prfm_literal(32, 0, 8, &instruction), -EINVAL);
    return failures;
}

static int check_system_and_sequences(void)
{
    int failures = 0;
    uint32_t instruction = 0;
    uint32_t output[5] = {0};
    const uint32_t expected_jump[5] = {0x58000050U, 0xD65F0200U, 0x9ABCDEF0U, 0x12345678U, 0xD503201FU};

    failures += expect_status("mrs s2_3_c13_c0_2", arm64_encode_mrs(4, 2, 3, 13, 0, 2, &instruction), 0);
    failures += expect_word("mrs s2_3_c13_c0_2", instruction, 0xD533D044U);
    failures += expect_status("mrs tpidr_el0", arm64_encode_mrs(4, 3, 3, 13, 0, 2, &instruction), 0);
    failures += expect_word("mrs tpidr_el0", instruction, 0xD53BD044U);
    failures += expect_status("msr tpidr_el0", arm64_encode_msr_register(5, 3, 3, 13, 0, 2, &instruction), 0);
    failures += expect_word("msr tpidr_el0", instruction, 0xD51BD045U);
    failures += expect_status("system op0 invalid", arm64_encode_mrs(0, 1, 3, 13, 0, 2, &instruction), -EINVAL);
    failures += expect_status("fill nops", arm64_encode_fill_nops(output, 5), 0);
    failures += expect_words("fill nops", output, (const uint32_t[]){0xD503201FU, 0xD503201FU, 0xD503201FU, 0xD503201FU, 0xD503201FU}, 5);
    failures += expect_status("fill nops null zero", arm64_encode_fill_nops(NULL, 0), 0);
    failures += expect_status("fill nops null", arm64_encode_fill_nops(NULL, 1), -EINVAL);
    failures += expect_status("absolute jump", arm64_emit_abs_jump(0x123456789ABCDEF0ULL, 16, output, 5), 0);
    failures += expect_words("absolute jump", output, expected_jump, 5);
    failures += expect_status("absolute jump scratch 31", arm64_emit_abs_jump(0, 31, output, 4), -EINVAL);
    failures += expect_status("absolute jump short buffer", arm64_emit_abs_jump(0, 16, output, 3), -EINVAL);
    return failures;
}

int main(void)
{
    int failures = 0;
    failures += check_fixed_instructions();
    failures += check_pc_relative_branches();
    failures += check_pc_addresses();
    failures += check_integer_instructions();
    failures += check_load_store_instructions();
    failures += check_literal_instructions();
    failures += check_system_and_sequences();
    if (failures)
    {
        fprintf(stderr, "encoder contracts: FAIL; failures=%d\n", failures);
        return 1;
    }

    puts("encoder contracts: PASS; groups=7");
    return 0;
}
