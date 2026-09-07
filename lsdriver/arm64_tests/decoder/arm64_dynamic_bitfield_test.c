typedef unsigned char uint8_t;
typedef unsigned int uint32_t;
typedef unsigned long long uint64_t;
typedef unsigned long size_t;

#include "../arm64_emulate/arm64_emulate_hw_templates.h"

struct test_output
{
    uint64_t value0;
};

static uint64_t low_mask(uint8_t bits)
{
    return bits == 64 ? ~0ULL : (1ULL << bits) - 1;
}

static uint64_t rotate_right(uint64_t value, uint8_t rotation, uint8_t width)
{
    uint64_t mask = low_mask(width);

    value &= mask;
    rotation %= width;
    return rotation == 0 ? value : ((value >> rotation) | (value << (width - rotation))) & mask;
}

static uint64_t replicate(uint64_t value, uint8_t element_width, uint8_t width)
{
    uint64_t result = 0;

    value &= low_mask(element_width);
    for (uint8_t offset = 0; offset < width; offset += element_width)
    {
        result |= value << offset;
    }
    return result;
}

static void decode_masks(uint8_t immr, uint8_t imms, uint8_t width, uint64_t *wmask, uint64_t *tmask)
{
    uint8_t len = width == 32 ? 5 : 6;
    uint8_t levels = (1U << len) - 1;
    uint8_t s = imms & levels;
    uint8_t r = immr & levels;
    uint8_t element_width = 1U << len;
    uint8_t d = (s - r) & levels;
    uint64_t welem = rotate_right(low_mask(s + 1), r, element_width);

    *wmask = replicate(welem, element_width, width);
    *tmask = replicate(low_mask(d + 1), element_width, width);
}

static uint64_t reference_sbfm(uint64_t source, uint64_t wmask, uint64_t tmask, uint8_t immr, uint8_t imms, uint8_t width)
{
    uint64_t sign = 0ULL - ((source >> imms) & 1ULL);
    uint64_t result = rotate_right(source, immr, width) & wmask & tmask;

    return result | (sign & ~tmask);
}

static uint64_t reference_bfm(uint64_t destination, uint64_t source, uint64_t wmask, uint64_t tmask, uint8_t immr, uint8_t width)
{
    uint64_t bot = (destination & ~wmask) | (rotate_right(source, immr, width) & wmask);

    return (destination & ~tmask) | (bot & tmask);
}

static uint64_t reference_ubfm(uint64_t source, uint64_t wmask, uint64_t tmask, uint8_t immr, uint8_t width)
{
    return rotate_right(source, immr, width) & wmask & tmask;
}

static uint64_t next_random(uint64_t *state)
{
    *state = *state * 6364136223846793005ULL + 1442695040888963407ULL;
    return *state;
}

static long syscall3(long number, long arg0, long arg1, long arg2)
{
    register long x0 __asm__("x0") = arg0;
    register long x1 __asm__("x1") = arg1;
    register long x2 __asm__("x2") = arg2;
    register long x8 __asm__("x8") = number;

    __asm__ volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory");
    return x0;
}

static void report_failure(const char *message)
{
    size_t length = 0;

    while (message[length]) length++;
    syscall3(64, 2, (long)message, (long)length);
    syscall3(93, 1, 0, 0);
    for (;;) __asm__ volatile("wfi");
}

static void check_value(uint64_t actual, uint64_t expected, uint8_t width, uint8_t operation, uint8_t immr, uint8_t imms)
{
    uint64_t mask = low_mask(width);

    if ((actual & mask) != (expected & mask) || (width == 32 && (actual >> 32) != 0))
    {
        static const char message[] = "dynamic bitfield template mismatch\n";
        (void)operation;
        (void)immr;
        (void)imms;
        report_failure(message);
    }
}

static void test_width(uint8_t width)
{
    static const uint64_t fixed_values[] = {
        0ULL,
        1ULL,
        ~0ULL,
        0x8000000000000000ULL,
        0xAAAAAAAAAAAAAAAAULL,
        0x5555555555555555ULL,
        0x0123456789ABCDEFULL,
        0xFEDCBA9876543210ULL,
    };
    uint64_t random_state = 0xC001D00D12345678ULL + width;
    uint8_t limit = width == 32 ? 32 : 64;

    for (uint8_t immr = 0; immr < limit; immr++)
    {
        for (uint8_t imms = 0; imms < limit; imms++)
        {
            uint64_t wmask;
            uint64_t tmask;

            decode_masks(immr, imms, width, &wmask, &tmask);
            for (uint8_t value_index = 0; value_index < sizeof(fixed_values) / sizeof(fixed_values[0]) + 4; value_index++)
            {
                uint64_t source = value_index < sizeof(fixed_values) / sizeof(fixed_values[0]) ? fixed_values[value_index] : next_random(&random_state);
                uint64_t destination = next_random(&random_state);
                struct test_output output;

                if (width == 32)
                {
                    source &= 0xFFFFFFFFULL;
                    destination &= 0xFFFFFFFFULL;
                    sbfm_dynamic_w32(source, wmask, tmask, immr, imms, &output);
                    check_value(output.value0, reference_sbfm(source, wmask, tmask, immr, imms, width), width, 0, immr, imms);
                    bfm_dynamic_w32(destination, source, wmask, tmask, immr, &output);
                    check_value(output.value0, reference_bfm(destination, source, wmask, tmask, immr, width), width, 1, immr, imms);
                    ubfm_dynamic_w32(source, wmask, tmask, immr, 0, &output);
                }
                else
                {
                    sbfm_dynamic_w64(source, wmask, tmask, immr, imms, &output);
                    check_value(output.value0, reference_sbfm(source, wmask, tmask, immr, imms, width), width, 0, immr, imms);
                    bfm_dynamic_w64(destination, source, wmask, tmask, immr, &output);
                    check_value(output.value0, reference_bfm(destination, source, wmask, tmask, immr, width), width, 1, immr, imms);
                    ubfm_dynamic_w64(source, wmask, tmask, immr, 0, &output);
                }
                check_value(output.value0, reference_ubfm(source, wmask, tmask, immr, width), width, 2, immr, imms);
            }
        }
    }
}

void _start(void)
{
    test_width(32);
    test_width(64);
    syscall3(93, 0, 0, 0);
    for (;;) __asm__ volatile("wfi");
}