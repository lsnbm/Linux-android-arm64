#include <linux/errno.h>
#include <linux/atomic.h>
#include <linux/module.h>
#include <linux/types.h>

#include "../../arm64_reg.h"
#include "../../break_point.h"
#include "../../arm64_dptdbg.h"

static struct break_point g_dptbp_test_info;
static atomic64_t g_dptbp_test_hits = ATOMIC64_INIT(0);

static void dptbp_test_on_hit(void *regs, void *fp_regs, void *hit_point)
{
    (void)hit_point;
    struct pt_regs *user_regs = regs;
    struct fp_regs *user_fp_regs = fp_regs;

    user_regs->regs[15] = 0x1234;
    user_fp_regs->q[8] = ((__uint128_t)0x99aabbccddeeff00ULL << 64) | 0x1122334455667788ULL;
    user_fp_regs->q[31] = ((__uint128_t)0x0123456789abcdefULL << 64) | 0xfedcba9876543210ULL;
    user_fp_regs->fpcr = 0x00400000;
    user_fp_regs->fpsr = 1;
    atomic64_inc(&g_dptbp_test_hits);
}

static bool dptbp_test_state_empty(void)
{
    if (dptbp_context_valid() || g_dptbp_current.mm || g_dptbp_current.info || g_dptbp_current.shadow_pgd || g_dptbp_current.mmu_notifier.ops) return false;
    if (g_dptbp_current.restoring || g_dptbp_current.sync_refs || g_dptbp_current.switch_refs || g_dptbp_current.notifier_refs || g_dptbp_current.mprotect_refs || g_dptbp_current.split_task || g_dptbp_current.mapping_updates) return false;
    for (size_t index = 0; index < ARRAY_SIZE(g_dptbp_current.slots); index++)
    {
        if (g_dptbp_current.slots[index].hook_addr || g_dptbp_current.slots[index].shadow_page) return false;
    }
    for (size_t index = 0; index < ARRAY_SIZE(g_dptbp_current.table_pages); index++)
    {
        if (g_dptbp_current.table_pages[index]) return false;
    }
    for (size_t index = 0; index < ARRAY_SIZE(g_dptbp_hooks); index++)
    {
        if (g_dptbp_hooks[index][0].installed) return false;
    }
    return true;
}

static int __init arm64_dptdbg_test_init(void)
{
    uint64_t composed_ttbr;
    int status;

    bypass_cfi();

    for (enum dptbp_ref_type ref_type = DPTBP_REF_SYNC; ref_type < DPTBP_REF_NONE; ref_type++)
    {
        if (dptbp_try_get_ref(ref_type) || *dptbp_context_refs(ref_type)) return -EINVAL;
        __atomic_store_n(&g_dptbp_current.valid, 1, __ATOMIC_SEQ_CST);
        if (!dptbp_try_get_ref(ref_type) || *dptbp_context_refs(ref_type) != 1) return -EINVAL;
        __atomic_store_n(&g_dptbp_current.valid, 0, __ATOMIC_SEQ_CST);
        if (dptbp_try_get_ref(ref_type) || *dptbp_context_refs(ref_type) != 1) return -EINVAL;
        dptbp_put_ref(ref_type);
    }
    dptbp_wait_refs();
    if (dptbp_try_get_ref(DPTBP_REF_NONE)) return -EINVAL;
    if (!dptbp_begin_restore() || dptbp_begin_restore()) return -EINVAL;
    __atomic_store_n(&g_dptbp_current.restoring, 0, __ATOMIC_SEQ_CST);

    composed_ttbr = dptbp_compose_ttbr0(0xffff000000123456ULL, 0xabc000, true);
    if (composed_ttbr & TTBR_CNP_BIT) return -EINVAL;
    if ((composed_ttbr & GENMASK_ULL(63, 48)) != 0xffff000000000000ULL) return -EINVAL;

    uint64_t address_mask = phys_to_ttbr(PHYS_MASK) & ~TTBR_CNP_BIT;
    phys_addr_t normal_pgd = 0x12344040;
    phys_addr_t shadow_pgd = 0x40000000;
    uint64_t original_ttbr = 0xabcd000000000000ULL | phys_to_ttbr(normal_pgd) | TTBR_CNP_BIT;
    composed_ttbr = dptbp_compose_ttbr0(original_ttbr, shadow_pgd, true);
    if ((composed_ttbr & address_mask) != phys_to_ttbr(shadow_pgd)) return -EINVAL;
    composed_ttbr = dptbp_compose_ttbr0(composed_ttbr, normal_pgd, false);
    if ((composed_ttbr & address_mask) != phys_to_ttbr(normal_pgd)) return -EINVAL;
    if ((composed_ttbr & GENMASK_ULL(63, 48)) != (original_ttbr & GENMASK_ULL(63, 48))) return -EINVAL;

    g_dptbp_test_info.tgid = current->tgid;
    g_dptbp_test_info.num_brps = 1;
    g_dptbp_test_info.points[0].on_hit = dptbp_test_on_hit;
    g_dptbp_test_info.points[0].bt = BP_BREAKPOINT_X;
    g_dptbp_test_info.points[0].bl = BP_BREAKPOINT_LEN_4;
    g_dptbp_test_info.points[0].bs = BP_SCOPE_MAIN_THREAD;
    g_dptbp_test_info.points[0].hit_addr = 0x40000000ULL;

    g_dptbp_test_info.points[1] = g_dptbp_test_info.points[0];
    g_dptbp_test_info.num_brps = 2;
    if (start_dptbp_monitor(&g_dptbp_test_info) != -EEXIST || !dptbp_test_state_empty()) goto failed;
    g_dptbp_test_info.points[1].hit_addr += 2;
    if (start_dptbp_monitor(&g_dptbp_test_info) != -EINVAL || !dptbp_test_state_empty()) goto failed;
    g_dptbp_test_info.points[1].hit_addr = 0x40000008ULL;
    if (start_dptbp_monitor(&g_dptbp_test_info) != -ESTALE || !dptbp_test_state_empty()) goto failed;
    g_dptbp_test_info.points[1].hit_addr = 0x40000004ULL;

    for (unsigned int cycle = 0; cycle < 2; cycle++)
    {
        status = start_dptbp_monitor(&g_dptbp_test_info);
        if (status) goto failed;
        if (!dptbp_context_valid() || !dptbp_task_uses_shadow(current)) goto failed;
        if (g_dptbp_current.slots[0].shadow_page != g_dptbp_current.slots[1].shadow_page) goto failed;
        if (g_dptbp_current.slots[0].orig_inst != 0x91000400U || g_dptbp_current.slots[1].orig_inst != 0xd65f03c0U) goto failed;
        if (READ_ONCE(((uint32_t *)g_dptbp_current.slots[0].shadow_page)[0]) || READ_ONCE(((uint32_t *)g_dptbp_current.slots[0].shadow_page)[1])) goto failed;
        pteval_t orig_value;
        if (read_user_pte_value(current->mm, 0x40000000ULL, &orig_value)) goto failed;
        if (pte_pfn(__pte(orig_value)) != pte_pfn(g_dptbp_current.slots[0].orig_pte)) goto failed;

        const unsigned long protections[] = {PROT_NONE, PROT_READ | PROT_WRITE | PROT_EXEC, PROT_READ | PROT_EXEC};
        for (size_t protection_index = 0; protection_index < ARRAY_SIZE(protections); protection_index++)
        {
            struct pt_regs sys_regs = {};
            struct pt_regs hook_regs = {};
            sys_regs.regs[0] = 0x40000000;
            sys_regs.regs[1] = PAGE_SIZE;
            sys_regs.regs[2] = protections[protection_index];
            hook_regs.regs[0] = (uint64_t)&sys_regs;
            if (dptbp_handle_mprotect(&hook_regs) != 1 || hook_regs.regs[0]) goto failed;
            if (!dptbp_task_uses_shadow(current)) goto failed;
            pte_t *normal_ptep = get_user_pte(current->mm, 0x40000000);
            pte_t *shadow_ptep = dptbp_get_shadow_pte(0x40000000);
            if (!normal_ptep || !shadow_ptep) goto failed;
            pteval_t normal_value = READ_ONCE(pte_val(*normal_ptep));
            pteval_t shadow_value = READ_ONCE(pte_val(*shadow_ptep));
            pteval_t pfn_mask = __phys_to_pte_val(PHYS_MASK & PAGE_MASK);
            if ((normal_value & pfn_mask) != (orig_value & pfn_mask)) goto failed;
            if ((shadow_value & pfn_mask) != __phys_to_pte_val(virt_to_phys(g_dptbp_current.slots[0].shadow_page))) goto failed;
            if ((normal_value & ~pfn_mask) != (shadow_value & ~pfn_mask)) goto failed;
            if (pte_val(g_dptbp_current.slots[0].orig_pte) != normal_value || pte_val(g_dptbp_current.slots[1].orig_pte) != normal_value) goto failed;

            sys_regs.regs[0] = 0x40001000;
            hook_regs.regs[0] = (uint64_t)&sys_regs;
            if (dptbp_handle_mprotect(&hook_regs) != 1 || hook_regs.regs[0]) goto failed;
            if (!dptbp_task_uses_shadow(current)) goto failed;
            normal_ptep = get_user_pte(current->mm, 0x40001000);
            shadow_ptep = dptbp_get_shadow_pte(0x40001000);
            if (!normal_ptep || !shadow_ptep || normal_ptep == shadow_ptep) goto failed;
            if (READ_ONCE(pte_val(*normal_ptep)) != READ_ONCE(pte_val(*shadow_ptep))) goto failed;
        }

        struct mmu_notifier_range range = {
            .mm = current->mm,
            .start = current->mm->start_stack & PGDIR_MASK,
            .end = (current->mm->start_stack & PGDIR_MASK) + PAGE_SIZE,
            .event = MMU_NOTIFY_UNMAP,
        };
        if (dptbp_mmu_notifier_invalidate_range_start(&g_dptbp_current.mmu_notifier, &range)) goto failed;
        if (!dptbp_task_uses_shadow(current)) goto failed;
        range.flags = MMU_NOTIFIER_RANGE_BLOCKABLE;
        if (dptbp_mmu_notifier_invalidate_range_start(&g_dptbp_current.mmu_notifier, &range)) goto failed;
        if (!dptbp_task_uses_shadow(current)) goto failed;
        range.end = range.start + PGDIR_SIZE;
        if (dptbp_unmap_uses_shared_tables(&range)) goto failed;
        range.start = 0x40000000UL;
        range.end = 0x40001000UL;
        WRITE_ONCE(g_dptbp_current.split_task, current);
        range.event = MMU_NOTIFY_CLEAR;
        range.flags = MMU_NOTIFIER_RANGE_BLOCKABLE;
        if (dptbp_mmu_notifier_invalidate_range_start(&g_dptbp_current.mmu_notifier, &range)) goto failed;
        if (g_dptbp_current.mapping_updates != 1 || dptbp_task_uses_shadow(current)) goto failed;
        dptbp_mmu_notifier_invalidate_range_end(&g_dptbp_current.mmu_notifier, &range);
        WRITE_ONCE(g_dptbp_current.split_task, NULL);
        if (g_dptbp_current.mapping_updates || !dptbp_task_uses_shadow(current)) goto failed;
        range.event = MMU_NOTIFY_UNMAP;
        range.flags = 0;
        if (dptbp_mmu_notifier_invalidate_range_start(&g_dptbp_current.mmu_notifier, &range) != -EAGAIN) goto failed;
        if (!dptbp_task_uses_shadow(current)) goto failed;
        range.flags = MMU_NOTIFIER_RANGE_BLOCKABLE;
        if (dptbp_mmu_notifier_invalidate_range_start(&g_dptbp_current.mmu_notifier, &range)) goto failed;
        if (dptbp_mmu_notifier_invalidate_range_start(&g_dptbp_current.mmu_notifier, &range)) goto failed;
        if (dptbp_task_uses_shadow(current) || !dptbp_context_valid() || !g_dptbp_current.info || !g_dptbp_current.mm) goto failed;
        stop_dptbp_monitor();
        stop_dptbp_monitor();
        if (!dptbp_test_state_empty()) goto failed;
    }

    status = start_dptbp_monitor(&g_dptbp_test_info);
    if (status)
    {
        pr_err("DPTDBG_TEST_MONITOR_FAILED status=%d\n", status);
        return status;
    }

    pr_info("DPTDBG_TEST_MONITOR_STARTED tgid=%d addr=0x%llx\n", g_dptbp_test_info.tgid, g_dptbp_test_info.points[0].hit_addr);
    return 0;

failed:
    stop_dptbp_monitor();
    pr_err("DPTDBG_TEST_LIFECYCLE_FAILED\n");
    return -EINVAL;
}

static void __exit arm64_dptdbg_test_exit(void)
{
    int64_t hits;

    stop_dptbp_monitor();
    hits = atomic64_read(&g_dptbp_test_hits);
    if (hits == 6 && dptbp_test_state_empty()) pr_info("DPTDBG_TEST_FUNCTION_PASS hits=%lld\n", (long long)hits);
    else pr_err("DPTDBG_TEST_FUNCTION_FAILED hits=%lld\n", (long long)hits);
    pr_info("DPTDBG_TEST_UNLOAD_OK\n");
}

module_init(arm64_dptdbg_test_init);
module_exit(arm64_dptdbg_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Standalone compile and lifecycle test for arm64_dptdbg.h");