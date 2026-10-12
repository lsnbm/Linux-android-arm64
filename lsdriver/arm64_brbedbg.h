#ifndef _ARM64_BRBEDBG_H_
#define _ARM64_BRBEDBG_H_

#include <linux/kconfig.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/bitops.h>
#include <linux/string.h>
#include "arm64_reg.h"

struct arm64_brbedbg_record
{
    u64 source;
    u64 target;
    u64 info;
    u32 type;
    u32 exception_level;
    u16 cycles;
    bool valid;
    bool mispredicted;
};

struct arm64_brbedbg_state
{
    struct arm64_brbedbg_record records[64];
    u32 record_count;
    u32 record_capacity;
    bool enabled;
};

static inline u32 arm64_brbedbg_record_capacity(void)
{
    return arm64_reg_brbe_record_count();
}

static inline bool arm64_brbedbg_supported(void)
{
    return arm64_reg_supports_brbe() && arm64_brbedbg_record_capacity() != 0;
}

static inline void arm64_brbedbg_invalidate(void)
{
    arm64_reg_brbe_invalidate();
}

static inline void arm64_brbedbg_select_bank(unsigned int bank)
{
    u64 control = arm64_reg_read_brbe_filter();

    control &= ~ARM64_TRACE_BRBFCR_BANK_MASK;
    control |= (u64)bank << 28;
    arm64_reg_write_brbe_filter(control);
    isb();
}

static inline void arm64_brbedbg_configure(bool trace_user, bool trace_kernel, bool cycles, bool flags)
{
    u64 brbcr = ARM64_TRACE_BRBCR_FZP | ARM64_TRACE_BRBCR_TS_VIRTUAL;
    u64 brbfcr = ARM64_TRACE_BRBFCR_DIRECT | ARM64_TRACE_BRBFCR_INDIRECT |
                 ARM64_TRACE_BRBFCR_RETURN | ARM64_TRACE_BRBFCR_INDIRECT_CALL |
                 ARM64_TRACE_BRBFCR_DIRECT_CALL | ARM64_TRACE_BRBFCR_COND_DIRECT;

    if (trace_user) brbcr |= ARM64_TRACE_BRBCR_E0BRE;
    if (trace_kernel) brbcr |= ARM64_TRACE_BRBCR_EXBRE;
    if (cycles) brbcr |= ARM64_TRACE_BRBCR_CC;
    if (flags) brbcr |= ARM64_TRACE_BRBCR_MPRED;

    arm64_brbedbg_invalidate();
    arm64_reg_write_brbe_control(brbcr);
    isb();
    arm64_reg_write_brbe_filter(brbfcr);
}

static inline int arm64_brbedbg_start(struct arm64_brbedbg_state *state, bool trace_user, bool trace_kernel, bool cycles, bool flags)
{
    if (!state || !arm64_brbedbg_supported()) return -EOPNOTSUPP;

    state->record_capacity = arm64_brbedbg_record_capacity();
    state->record_count = 0;
    arm64_brbedbg_configure(trace_user, trace_kernel, cycles, flags);
    state->enabled = true;
    return 0;
}

static inline void arm64_brbedbg_stop(struct arm64_brbedbg_state *state)
{
    arm64_reg_write_brbe_filter(ARM64_TRACE_BRBFCR_PAUSED);
    arm64_reg_write_brbe_control(0);
    if (state) state->enabled = false;
}

static inline u16 arm64_brbedbg_cycles(u64 info)
{
    u32 exponent;
    u32 mantissa;

    if (info & ARM64_TRACE_BRBINF_CC_UNAVAILABLE) return 0;
    exponent = FIELD_GET(GENMASK(45, 40), info);
    mantissa = FIELD_GET(GENMASK(39, 32), info);
    if (!exponent) return mantissa;
    return min_t(u32, (mantissa | 0x100U) << (exponent - 1), U16_MAX);
}

static inline int arm64_brbedbg_snapshot(struct arm64_brbedbg_state *state)
{
    u32 record_capacity;

    if (!state || !state->enabled) return -EINVAL;

    arm64_reg_write_brbe_filter(ARM64_TRACE_BRBFCR_PAUSED);
    record_capacity = min_t(u32, arm64_brbedbg_record_capacity(), ARRAY_SIZE(state->records));
    state->record_count = 0;

    for (unsigned int bank = 0; bank < DIV_ROUND_UP(record_capacity, 32U); ++bank)
    {
        const u32 records_in_bank = min_t(u32, record_capacity - bank * 32U, 32U);
        arm64_brbedbg_select_bank(bank);

        for (u32 index = 0; index < records_in_bank; ++index)
        {
            struct arm64_brbedbg_record *record = &state->records[state->record_count];
            const u64 info = arm64_reg_read_brbe_info(index);
            const u32 valid = FIELD_GET(ARM64_TRACE_BRBINF_VALID_MASK, info);

            if (!valid) break;
            record->info = info;
            record->source = (valid == 1 || valid == 3) ? arm64_reg_read_brbe_source(index) : 0;
            record->target = (valid == 1 || valid == 2) ? arm64_reg_read_brbe_target(index) : 0;
            record->type = FIELD_GET(ARM64_TRACE_BRBINF_TYPE_MASK, info);
            record->exception_level = FIELD_GET(ARM64_TRACE_BRBINF_EL_MASK, info);
            record->cycles = arm64_brbedbg_cycles(info);
            record->valid = true;
            record->mispredicted = !!(info & ARM64_TRACE_BRBINF_MISPRED);
            ++state->record_count;
        }
    }

    return state->record_count;
}

#endif