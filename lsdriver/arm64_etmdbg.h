#ifndef _ARM64_ETMDBG_H_
#define _ARM64_ETMDBG_H_

#include <linux/kconfig.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/gfp.h>
#include <linux/mm.h>
#include <linux/bitops.h>
#include <linux/smp.h>
#include <linux/string.h>
#include <asm/ptrace.h>
#include "arm64_reg.h"
#include "klog/lsdriver_log.h"

struct arm64_etmdbg_cpu_state
{
    struct page *buffer_page;
    void *buffer;
    unsigned long buffer_addr;
    size_t buffer_size;
    u8 trace_id;
    bool trace_enabled;
    bool trbe_enabled;
    bool trace_filter_saved;
    u64 saved_trace_filter;
    u64 last_status;
    u64 last_write;
    u64 last_exception_pc;
};

static inline void arm64_etmdbg_log_start_failure(const char *phase, int status,
                                                  u64 start, u64 stop)
{
    u64 dfr0 = arm64_reg_read_dfr0();
    bool trace_version = arm64_reg_supports_sysreg_trace();
    bool trbe_version = ((dfr0 >> ARM64_TRACE_DFR0_TRACEBUFFER_SHIFT) & 0xfU) != 0;
    u64 devarch = trace_version ? arm64_reg_read_etm_devarch() : 0;
    u64 oslsr = trace_version ? arm64_reg_read_etm_os_lock_status() : 0;
    u64 trbidr = trbe_version ? arm64_reg_read_trbe_id() : 0;
    bool sysreg_control = trace_version && arm64_reg_has_etm_sysreg_control();
    bool trbe_control = trbe_version && arm64_reg_supports_trbe();
    bool trace_source = trace_version && arm64_reg_has_etm_source();

    ls_log_always_tag("etm",
                      "start failed phase=%s status=%d cpu=%u start=0x%llx stop=0x%llx dfr0=0x%llx devarch=0x%llx oslsr=0x%llx trbidr=0x%llx trace=%u sysreg=%u trbe=%u source=%u\n",
                      phase, status, smp_processor_id(), start, stop, dfr0, devarch, oslsr, trbidr,
                      trace_version, sysreg_control, trbe_control, trace_source);
}

static inline u64 arm64_etmdbg_exlevel_mask(bool exclude_kernel, bool exclude_user)
{
    u64 mask = ARM64_TRACE_EXLEVEL_HYP;

    if (exclude_kernel) mask |= ARM64_TRACE_EXLEVEL_OS;
    if (exclude_user) mask |= ARM64_TRACE_EXLEVEL_APP;
    return mask;
}

static inline int arm64_etmdbg_alloc_buffer(struct arm64_etmdbg_cpu_state *state, unsigned int order)
{
    if (!state || state->buffer_page || order == 0 || order > 6) return -EINVAL;

    state->buffer_page = alloc_pages(GFP_KERNEL | __GFP_ZERO, order);
    if (!state->buffer_page) return -ENOMEM;

    state->buffer = page_address(state->buffer_page);
    state->buffer_addr = (unsigned long)state->buffer;
    state->buffer_size = PAGE_SIZE << order;
    if (!state->buffer || !IS_ALIGNED(state->buffer_addr, PAGE_SIZE))
    {
        __free_pages(state->buffer_page, order);
        memset(state, 0, sizeof(*state));
        return -EINVAL;
    }

    return 0;
}

static inline void arm64_etmdbg_free_buffer(struct arm64_etmdbg_cpu_state *state, unsigned int order)
{
    if (!state) return;

    if (state->buffer_page) __free_pages(state->buffer_page, order);
    memset(state, 0, sizeof(*state));
}

static inline int arm64_etmdbg_configure_trbe(struct arm64_etmdbg_cpu_state *state)
{
    unsigned long limit;

    if (!state || !state->buffer || !state->buffer_size) return -EINVAL;
    if (!arm64_reg_supports_trbe()) return -EOPNOTSUPP;
    if (state->buffer_addr > ULONG_MAX - state->buffer_size) return -EOVERFLOW;

    limit = state->buffer_addr + state->buffer_size;
    if (!IS_ALIGNED(limit, PAGE_SIZE)) return -EINVAL;

    arm64_reg_write_trbe_limit(0);
    isb();
    dsb(nsh);
    arm64_reg_write_trbe_base(state->buffer_addr);
    arm64_reg_write_trbe_pointer(state->buffer_addr);
    arm64_reg_write_trbe_limit(limit & ARM64_TRACE_TRBLIMITR_LIMIT_MASK);
    arm64_reg_write_trbe_status(0);
    isb();
    state->trbe_enabled = false;
    return 0;
}

static inline void arm64_etmdbg_set_trbe_enabled(struct arm64_etmdbg_cpu_state *state, bool enabled)
{
    u64 limit;

    if (!state || !state->buffer_size) return;

    limit = (state->buffer_addr + state->buffer_size) & ARM64_TRACE_TRBLIMITR_LIMIT_MASK;
    if (enabled)
    {
        /* STOP mode avoids depending on the kernel's TRBE IRQ handler. */
        arm64_reg_write_trbe_limit(limit | ARM64_TRACE_TRBLIMITR_TM_STOP | ARM64_TRACE_TRBLIMITR_E);
        isb();
    }
    else
    {
        arm64_reg_write_trbe_limit(limit);
        dsb(nsh);
        isb();
    }
    state->trbe_enabled = enabled;
}

static inline int arm64_etmdbg_program_source(struct arm64_etmdbg_cpu_state *state, u64 start, u64 stop, bool exclude_kernel, bool exclude_user)
{
    u64 exlevel;
    u64 comparator_access;
    u64 victlr;

    if (!state || !start || start >= stop) return -EINVAL;
    if (!arm64_reg_supports_sysreg_trace() || !arm64_reg_has_etm_sysreg_control() ||
        !arm64_reg_has_etm_source()) return -EOPNOTSUPP;

    exlevel = arm64_etmdbg_exlevel_mask(exclude_kernel, exclude_user);
    comparator_access = exlevel << ARM64_TRACE_TRCACATR_EXLEVEL_SHIFT;
    victlr = ARM64_TRACE_TRCVICTLR_EVENT_ALWAYS | ARM64_TRACE_TRCVICTLR_SSSTATUS | (exlevel << 16);
    state->trace_id = state->trace_id ? state->trace_id : ARM64_TRACE_ID_BASE + (smp_processor_id() & 0x3f);

    arm64_reg_write_etm_os_lock(0);
    arm64_reg_write_etm_program_control(0);
    isb();
    arm64_reg_write_etm_trace_id(state->trace_id);
    arm64_reg_write_etm_view_inst_control(victlr);
    arm64_reg_write_etm_view_inst_include(BIT(0));
    arm64_reg_write_etm_view_inst_start_stop(0);
    arm64_reg_write_etm_address_value(0, start);
    arm64_reg_write_etm_address_access(0, comparator_access);
    arm64_reg_write_etm_address_value(1, stop);
    arm64_reg_write_etm_address_access(1, comparator_access);

    if (arm64_reg_supports_trace_filter())
    {
        u64 trfcr = ARM64_TRACE_TRFCR_TS_VIRTUAL;

        state->saved_trace_filter = arm64_reg_read_etm_trace_filter();
        state->trace_filter_saved = true;
        if (!exclude_user) trfcr |= ARM64_TRACE_TRFCR_E0TRE;
        if (!exclude_kernel) trfcr |= ARM64_TRACE_TRFCR_EXTRE;
        arm64_reg_write_etm_trace_filter(trfcr);
    }

    arm64_reg_write_etm_program_control(1);
    isb();
    state->trace_enabled = true;
    return 0;
}

static inline void arm64_etmdbg_restore_trace_filter(struct arm64_etmdbg_cpu_state *state)
{
    if (!state || !state->trace_filter_saved) return;

    arm64_reg_write_etm_trace_filter(state->saved_trace_filter);
    isb();
    state->trace_filter_saved = false;
}

static inline int arm64_etmdbg_start(struct arm64_etmdbg_cpu_state *state, u64 start, u64 stop, bool exclude_kernel, bool exclude_user)
{
    int ret = arm64_etmdbg_configure_trbe(state);
    if (ret)
    {
        arm64_etmdbg_log_start_failure("configure_trbe", ret, start, stop);
        return ret;
    }

    arm64_etmdbg_set_trbe_enabled(state, true);
    ret = arm64_etmdbg_program_source(state, start, stop, exclude_kernel, exclude_user);
    if (ret)
    {
        arm64_etmdbg_set_trbe_enabled(state, false);
        arm64_etmdbg_restore_trace_filter(state);
        arm64_etmdbg_log_start_failure("program_source", ret, start, stop);
        return ret;
    }

    return 0;
}

/*
 * Polling path for a module that deliberately does not register a CoreSight
 * or perf driver. The caller should run this on the owning CPU and copy the
 * bytes before re-arming the buffer after a STOP/FILL event.
 */
static inline bool arm64_etmdbg_poll(struct arm64_etmdbg_cpu_state *state)
{
    u64 status;
    u64 write_pointer;
    u64 limit;

    if (!state || !state->trbe_enabled || !state->buffer_size) return false;

    status = arm64_reg_read_trbe_status();
    write_pointer = arm64_reg_read_trbe_pointer();
    limit = (state->buffer_addr + state->buffer_size) & ARM64_TRACE_TRBLIMITR_LIMIT_MASK;
    state->last_status = status;
    state->last_write = write_pointer;

    return !!(status & (ARM64_TRACE_TRBSR_STOP | ARM64_TRACE_TRBSR_WRAP | ARM64_TRACE_TRBSR_ABORT)) || write_pointer >= limit;
}

static inline u64 arm64_etmdbg_stop(struct arm64_etmdbg_cpu_state *state, struct pt_regs *regs)
{
    u64 status;

    if (!state) return 0;
    if (state->trace_enabled)
    {
        if (state->trace_filter_saved)
        {
            arm64_reg_write_etm_trace_filter(state->saved_trace_filter &
                                             ~(ARM64_TRACE_TRFCR_E0TRE | ARM64_TRACE_TRFCR_EXTRE));
            isb();
        }
        dsb(sy);
        isb();
        tsb_csync();
        arm64_reg_write_etm_program_control(0);
        isb();
        state->trace_enabled = false;
    }

    arm64_etmdbg_set_trbe_enabled(state, false);
    status = arm64_reg_read_trbe_status();
    state->last_status = status;
    state->last_write = arm64_reg_read_trbe_pointer();
    state->last_exception_pc = regs ? regs->pc : 0;
    arm64_etmdbg_restore_trace_filter(state);
    return status;
}

static inline bool arm64_etmdbg_handle_trbe(struct arm64_etmdbg_cpu_state *state, struct pt_regs *regs)
{
    u64 status;

    if (!state) return false;
    status = arm64_reg_read_trbe_status();
    if (!(status & (ARM64_TRACE_TRBSR_IRQ | ARM64_TRACE_TRBSR_STOP |
                    ARM64_TRACE_TRBSR_WRAP | ARM64_TRACE_TRBSR_ABORT))) return false;

    state->last_status = status;
    state->last_write = arm64_reg_read_trbe_pointer();
    state->last_exception_pc = regs ? regs->pc : 0;
    if (status & ARM64_TRACE_TRBSR_IRQ) arm64_reg_write_trbe_status(status & ~ARM64_TRACE_TRBSR_IRQ);
    return true;
}

static inline size_t arm64_etmdbg_trace_size(const struct arm64_etmdbg_cpu_state *state)
{
    u64 write_pointer;

    if (!state || !state->buffer || !state->buffer_size) return 0;
    if (state->last_status & ARM64_TRACE_TRBSR_WRAP) return state->buffer_size;
    write_pointer = state->last_write ? state->last_write : arm64_reg_read_trbe_pointer();
    if (write_pointer < state->buffer_addr || write_pointer > state->buffer_addr + state->buffer_size) return 0;
    return write_pointer - state->buffer_addr;
}

static inline size_t arm64_etmdbg_copy_trace(const struct arm64_etmdbg_cpu_state *state, void *destination, size_t capacity)
{
    size_t size;

    if (!state || !destination) return 0;
    size = min(arm64_etmdbg_trace_size(state), capacity);
    if (size) memcpy(destination, state->buffer, size);
    return size;
}

#endif