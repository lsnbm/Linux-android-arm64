#include <linux/errno.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/types.h>

#include "../../break_point.h"
#include "../../arm64_dptdbg.h"

static struct break_point g_dptdbg_test_info;
static atomic64_t g_dptdbg_test_hits = ATOMIC64_INIT(0);

static void dptdbg_test_on_hit(void *regs, void *fp_regs, void *hit_point)
{
    (void)regs;
    (void)fp_regs;
    (void)hit_point;
    atomic64_inc(&g_dptdbg_test_hits);
}

static int __init arm64_dptdbg_test_init(void)
{
    size_t inst_index;
    uint64_t composed_ttbr;
    int status;

    bypass_cfi();

    if (!dptdbg_get_inst_index(PAGE_SIZE, PAGE_SIZE + sizeof(uint32_t), &inst_index) || inst_index != 1) return -EINVAL;
    if (dptdbg_get_inst_index(PAGE_SIZE, PAGE_SIZE + 2, &inst_index)) return -EINVAL;

    composed_ttbr = dptdbg_compose_ttbr0(0xffff000000123456ULL, 0xabc000, true);
    if (composed_ttbr & TTBR_CNP_BIT) return -EINVAL;

    g_dptdbg_test_info.tgid = current->tgid;
    g_dptdbg_test_info.num_brps = 1;
    g_dptdbg_test_info.points[0].on_hit = dptdbg_test_on_hit;
    g_dptdbg_test_info.points[0].bt = BP_BREAKPOINT_X;
    g_dptdbg_test_info.points[0].bl = BP_BREAKPOINT_LEN_4;
    g_dptdbg_test_info.points[0].bs = BP_SCOPE_MAIN_THREAD;
    g_dptdbg_test_info.points[0].hit_addr = 0x40000000ULL;

    status = dptdbg_start_monitor(&g_dptdbg_test_info);
    if (status)
    {
        pr_err("DPTDBG_TEST_MONITOR_FAILED status=%d\n", status);
        return status;
    }

    pr_info("DPTDBG_TEST_MONITOR_STARTED tgid=%d addr=0x%llx\n", g_dptdbg_test_info.tgid, g_dptdbg_test_info.points[0].hit_addr);
    return 0;
}

static void __exit arm64_dptdbg_test_exit(void)
{
    int64_t hits;

    dptdbg_stop_monitor();
    hits = atomic64_read(&g_dptdbg_test_hits);
    if (hits == 3) pr_info("DPTDBG_TEST_FUNCTION_PASS hits=%lld\n", (long long)hits);
    else pr_err("DPTDBG_TEST_FUNCTION_FAILED hits=%lld\n", (long long)hits);
    pr_info("DPTDBG_TEST_UNLOAD_OK\n");
}

module_init(arm64_dptdbg_test_init);
module_exit(arm64_dptdbg_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Standalone compile and lifecycle test for arm64_dptdbg.h");