#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "../arm64_ghost/arm64_page_reloc.h"

static void fail_word(const char *name, uint32_t actual, uint32_t expected)
{
    fprintf(stderr, "%s: actual=%08x expected=%08x\n", name, actual, expected);
    exit(1);
}

static void expect_word(const char *name, uint32_t actual, uint32_t expected)
{
    if (actual != expected) fail_word(name, actual, expected);
}

static void expect_status(const char *name, int actual, int expected)
{
    if (actual != expected)
    {
        fprintf(stderr, "%s: status=%d expected=%d\n", name, actual, expected);
        exit(1);
    }
}

static void expect_u64(const char *name, const uint32_t *words, uint64_t expected)
{
    uint64_t actual;
    memcpy(&actual, words, sizeof(actual));
    if (actual != expected)
    {
        fprintf(stderr, "%s: actual=%016llx expected=%016llx\n", name, (unsigned long long)actual, (unsigned long long)expected);
        exit(1);
    }
}

static void clear_source(uint32_t *source)
{
    for (size_t index = 0; index < ARM64_PAGE_RELOC_SOURCE_WORDS; index++) source[index] = 0xD503201F;
}

static void test_page_relative_relocation(void)
{
    const uint64_t source_page = 0x10000000ULL;
    const uint64_t ghost_page = 0x10080000ULL;
    uint32_t source[ARM64_PAGE_RELOC_SOURCE_WORDS];
    uint32_t *output = calloc(ARM64_PAGE_RELOC_MAX_WORDS, sizeof(*output));
    struct arm64_page_relocation relocation;
    uint32_t expected;

    if (!output) exit(1);
    clear_source(source);

    expect_status("source b", arm64_encode_b(source_page, source_page + 0x40, &source[0]), 0);
    expect_status("source b.cond", arm64_encode_b_cond(source_page + 4, source_page + 0x80, 1, &source[1]), 0);
    expect_status("source cbnz", arm64_encode_cbz(source_page + 8, source_page + 0xC0, true, true, 3, &source[2]), 0);
    expect_status("source tbz", arm64_encode_tbz(source_page + 12, source_page + 0x100, false, 40, 5, &source[3]), 0);
    expect_status("source adr", arm64_encode_adr(7, source_page + 16, source_page + 0x234, &source[4]), 0);
    expect_status("source adrp", arm64_encode_adrp(8, source_page + 20, source_page + 0x9000, &source[5]), 0);
    expect_status("source literal", arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 9, source_page + 24, source_page + 0x300, &source[6]), 0);
    expect_status("source prfm", arm64_encode_prfm_literal(3, source_page + 28, source_page + 0x340, &source[7]), 0);

    expect_status("relocate page relative", arm64_page_relocate(source_page, ghost_page, source, output, ARM64_PAGE_RELOC_MAX_BYTES, &relocation), 0);
    if (relocation.slot_count != 0 || relocation.code_size != ARM64_PAGE_RELOC_BASE_BYTES)
    {
        fprintf(stderr, "page relative metadata mismatch slots=%u size=%zu\n", relocation.slot_count, relocation.code_size);
        exit(1);
    }

    expect_status("expected b", arm64_encode_b(ghost_page, ghost_page + 0x40, &expected), 0);
    expect_word("page b", output[0], expected);
    expect_status("expected b.cond", arm64_encode_b_cond(ghost_page + 4, ghost_page + 0x80, 1, &expected), 0);
    expect_word("page b.cond", output[1], expected);
    expect_status("expected cbnz", arm64_encode_cbz(ghost_page + 8, ghost_page + 0xC0, true, true, 3, &expected), 0);
    expect_word("page cbnz", output[2], expected);
    expect_status("expected tbz", arm64_encode_tbz(ghost_page + 12, ghost_page + 0x100, false, 40, 5, &expected), 0);
    expect_word("page tbz", output[3], expected);
    expect_status("expected adr", arm64_encode_adr(7, ghost_page + 16, source_page + 0x234, &expected), 0);
    expect_word("page adr", output[4], expected);
    expect_status("expected adrp", arm64_encode_adrp(8, ghost_page + 20, source_page + 0x9000, &expected), 0);
    expect_word("page adrp", output[5], expected);
    expect_status("expected literal", arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 9, ghost_page + 24, source_page + 0x300, &expected), 0);
    expect_word("page literal", output[6], expected);
    expect_status("expected prfm", arm64_encode_prfm_literal(3, ghost_page + 28, source_page + 0x340, &expected), 0);
    expect_word("page prfm", output[7], expected);
    expect_status("page return", arm64_encode_b(ghost_page + PAGE_SIZE, source_page + PAGE_SIZE, &expected), 0);
    expect_word("page return branch", output[ARM64_PAGE_RELOC_SOURCE_WORDS], expected);
    expect_word("page return padding 1", output[ARM64_PAGE_RELOC_SOURCE_WORDS + 1], 0xD503201F);
    expect_word("page return padding 2", output[ARM64_PAGE_RELOC_SOURCE_WORDS + 2], 0xD503201F);
    expect_word("page return padding 3", output[ARM64_PAGE_RELOC_SOURCE_WORDS + 3], 0xD503201F);

    if (arm64_page_reloc_to_ghost(&relocation, source_page + 0x234) != ghost_page + 0x234) exit(1);
    if (arm64_page_reloc_to_source(&relocation, ghost_page + 0x234) != source_page + 0x234) exit(1);
    free(output);
}

static void test_far_slots(void)
{
    const uint64_t source_page = 0x10000000ULL;
    const uint64_t ghost_page = 0x7000000000ULL;
    uint32_t source[ARM64_PAGE_RELOC_SOURCE_WORDS];
    uint32_t *output = calloc(ARM64_PAGE_RELOC_MAX_WORDS, sizeof(*output));
    struct arm64_page_relocation relocation;
    uint32_t expected;

    if (!output) exit(1);
    clear_source(source);

    expect_status("source bl", arm64_encode_bl(source_page, source_page + 0x2000, &source[0]), 0);
    expect_status("source adr far", arm64_encode_adr(4, source_page + 4, source_page + 0x400, &source[1]), 0);
    expect_status("source literal far", arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR32, 6, source_page + 8, source_page + 0x500, &source[2]), 0);
    expect_status("source prfm far", arm64_encode_prfm_literal(3, source_page + 12, source_page + 0x600, &source[3]), 0);

    expect_status("relocate far slots", arm64_page_relocate(source_page, ghost_page, source, output, ARM64_PAGE_RELOC_MAX_BYTES, &relocation), 0);
    if (relocation.slot_count != 3)
    {
        fprintf(stderr, "far slot count=%u expected=3\n", relocation.slot_count);
        exit(1);
    }

    size_t slot0 = ARM64_PAGE_RELOC_SLOT_BASE_WORD;
    size_t slot1 = slot0 + ARM64_PAGE_RELOC_SLOT_WORDS;
    size_t slot2 = slot1 + ARM64_PAGE_RELOC_SLOT_WORDS;

    expect_status("bl to slot", arm64_encode_b(ghost_page, ghost_page + slot0 * sizeof(uint32_t), &expected), 0);
    expect_word("far bl branch", output[0], expected);
    expect_status("bl load lr", arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 30, ghost_page + slot0 * 4, ghost_page + (slot0 + 6) * 4, &expected), 0);
    expect_word("far bl lr", output[slot0], expected);
    expect_u64("far bl return pc", &output[slot0 + 6], source_page + 4);

    expect_status("adr to slot", arm64_encode_b(ghost_page + 4, ghost_page + slot1 * 4, &expected), 0);
    expect_word("far adr branch", output[1], expected);
    expect_status("adr slot load", arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 4, ghost_page + slot1 * 4, ghost_page + (slot1 + 2) * 4, &expected), 0);
    expect_word("far adr load", output[slot1], expected);
    expect_u64("far adr target", &output[slot1 + 2], source_page + 0x400);

    expect_status("literal to slot", arm64_encode_b(ghost_page + 8, ghost_page + slot2 * 4, &expected), 0);
    expect_word("far literal branch", output[2], expected);
    expect_status("literal slot address", arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 6, ghost_page + slot2 * 4, ghost_page + (slot2 + 6) * 4, &expected), 0);
    expect_word("far literal address", output[slot2], expected);
    expect_status("literal slot load", arm64_encode_load_store_unsigned(true, ARM64_ENCODE_LS_GPR32, 6, 6, 0, &expected), 0);
    expect_word("far literal load", output[slot2 + 1], expected);
    expect_u64("far literal target", &output[slot2 + 6], source_page + 0x500);
    expect_word("far prfm nop", output[3], 0xD503201F);
    expect_word("far page return ldr", output[ARM64_PAGE_RELOC_SOURCE_WORDS], 0x58000050);
    expect_word("far page return ret", output[ARM64_PAGE_RELOC_SOURCE_WORDS + 1], 0xD65F0200);
    expect_u64("far page return target", &output[ARM64_PAGE_RELOC_SOURCE_WORDS + 2], source_page + PAGE_SIZE);

    free(output);
}

static void test_reject_unsupported(void)
{
    const uint64_t source_page = 0x10000000ULL;
    const uint64_t ghost_page = 0x10080000ULL;
    uint32_t source[ARM64_PAGE_RELOC_SOURCE_WORDS];
    uint32_t *output = calloc(ARM64_PAGE_RELOC_MAX_WORDS, sizeof(*output));
    struct arm64_page_relocation relocation;

    if (!output) exit(1);
    clear_source(source);
    source[0] = 0x54000010U;
    expect_status("reject unsupported relocation", arm64_page_relocate(source_page, ghost_page, source, output, ARM64_PAGE_RELOC_MAX_BYTES, &relocation), -EOPNOTSUPP);
    free(output);
}

int main(void)
{
    test_page_relative_relocation();
    test_far_slots();
    test_reject_unsupported();
    return 0;
}