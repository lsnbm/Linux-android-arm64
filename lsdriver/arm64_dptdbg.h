#ifndef ARM64_DPTDBG_H
#define ARM64_DPTDBG_H

/*
为每个受管代码页创建含 UDF 断点的执行副本，同页断点槽位共享这一页。
私有 PGD 只复制受管地址经过的页表路径，将对应叶子 PTE 指向执行副本。
目标主线程切入私有页表，其他线程使用普通页表；原始指令不被修改。
私有 PTE 表覆盖范围内的 mprotect 同步普通页表与执行副本权限，保留各自物理页和断点。

资源分工：
安装函数创建 slot、执行副本和私有页表，注册 hook 与 notifier 后发布资源。
异常、任务切换、mprotect 和 notifier 先增加分类引用，再检查 valid，持引用使用 context。
异常回退只发布撤销意图并恢复当前线程视图，控制卸载统一回收页、mm 和 hook。

运行状态：
valid=1、restoring=0：资源已发布，允许目标主线程进入断点视图。
valid=1、restoring=1：停止切入断点视图，仍允许处理已触发的 UDF 并返回普通页重试。
valid=0：拒绝新引用访问资源，控制面等待已有引用退出后释放资源。
restoring 在完整卸载结束时清零；异常回退不会立即关闭 valid 或清空 info。
mapping_updates 非零时暂停执行视图；受控 VMA 拆分结束校验并同步 PTE 后恢复。

卸载顺序：停止切入并移除切换 hook，恢复线程视图，注销 notifier，关闭 valid，
等待三类引用排空，再移除异常 hook、释放私有页表和执行副本，最后归还 mm 引用。
*/

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mman.h>
#include <linux/mmu_notifier.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/smp.h>
#include <asm/esr.h>
#include <asm/memory.h>
#include <asm/mmu_context.h>
#include <asm/pgtable-hwdef.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>
#include <asm/tlbflush.h>

#include "arm64_emulate/emulate_inst.h"
#include "arm64_reg.h"
#include "export_fun.h"
#include "inline_hook_frame.h"
#include "io_version/io_struct.h"

// 槽位下标与 info->points 保持一致，同页槽位共享执行副本并各自缓存原始指令。
struct dptbp_slot
{
    uint64_t page_vaddr; // 断点所属用户页的页对齐虚拟地址
    uint64_t hook_addr;  // 当前断点地址；为 0 表示该槽位未配置
    void *shadow_page;   // 含 UDF 的执行副本内核地址，由同页槽位共享
    pte_t orig_pte;      // 复制代码时的源 PTE，用于建立私有映射时校验物理页未变
    uint32_t orig_inst;  // 改写 UDF 前缓存的原始指令，命中后用于指令模拟
    // 按缓存行对齐，避免相邻槽位共享同一缓存行。
} ____cacheline_aligned;

enum dptbp_ref_type
{
    DPTBP_REF_SYNC,     // EL0 同步异常路径
    DPTBP_REF_SWITCH,   // 任务切换路径
    DPTBP_REF_NOTIFIER, // MMU notifier 回调路径
    DPTBP_REF_MPROTECT,
    DPTBP_REF_NONE, // 不对应任何引用计数
};

// valid 决定资源访问准入，分类引用保证已经进入的使用者退出前资源不会被释放。
struct dptbp_context
{
    struct mm_struct *mm;                                     // 持有 get_mm_by_pid() 返回的一份引用，由控制卸载归还
    struct break_point *info;                                 // 保留共享配置指针，命中回调直接访问原 points 中的寄存器现场
    struct dptbp_slot slots[BP_CONFIG_MAX];                   // 与共享配置逐槽位对应的断点资源
    pgd_t *shadow_pgd;                                        // 私有页表根页，目标主线程的断点视图使用此根
    void *table_pages[BP_CONFIG_MAX * CONFIG_PGTABLE_LEVELS]; // 自行复制的下级页表页，集中记录供回收
    struct mmu_notifier mmu_notifier;                         // 目标映射失效时撤销视图；ops 用于判断卸载时是否需要注销

    int sync_refs;     // 同步异常使用者数量，通过原子操作增减
    int switch_refs;   // 任务切换使用者数量
    int notifier_refs; // 映射失效回调使用者数量
    int mprotect_refs;
    struct task_struct *split_task;
    int mapping_updates;
    int valid;     // 非零允许新引用访问已发布资源，不单独表示断点视图仍然启用
    int restoring; // 非零禁止继续切入断点视图，保持到完整卸载结束
};

static struct dptbp_context g_dptbp_current;
// 串行化 start/stop 和资源分配回收；异常与任务切换路径不获取此互斥锁。
static DEFINE_MUTEX(g_dptbp_control_lock);

// 读取 context 的资源发布状态。
static inline int dptbp_context_valid(void)
{
    return __atomic_load_n(&g_dptbp_current.valid, __ATOMIC_SEQ_CST);
}

// 根据入口类型返回对应的引用计数地址。
static inline int *dptbp_context_refs(enum dptbp_ref_type ref_type)
{
    switch (ref_type)
    {
    case DPTBP_REF_SYNC:
        return &g_dptbp_current.sync_refs;
    case DPTBP_REF_SWITCH:
        return &g_dptbp_current.switch_refs;
    case DPTBP_REF_NOTIFIER:
        return &g_dptbp_current.notifier_refs;
    case DPTBP_REF_MPROTECT:
        return &g_dptbp_current.mprotect_refs;
    default:
        return NULL;
    }
}

// 释放当前入口持有的一份引用；调用后不得继续使用本次引用保护的资源。
static inline void dptbp_put_ref(enum dptbp_ref_type ref_type)
{
    int *refs = dptbp_context_refs(ref_type);

    if (refs) __atomic_sub_fetch(refs, 1, __ATOMIC_SEQ_CST);
}

// 必须先加引用再检查 valid，避免检查通过后、增加引用前资源已被卸载。
// 未发布或已关闭时只回退引用，不访问 mm、info、slot 或私有页表。
static inline bool dptbp_try_get_ref(enum dptbp_ref_type ref_type)
{
    int *refs = dptbp_context_refs(ref_type);

    if (!refs) return false;
    __atomic_add_fetch(refs, 1, __ATOMIC_SEQ_CST);
    if (likely(dptbp_context_valid())) return true;
    dptbp_put_ref(ref_type);
    return false;
}

// 控制面关闭 valid 后等待各类引用排空；此函数可能调度，不能从持引用的异常路径调用。
static inline void dptbp_wait_refs(void)
{
    while (__atomic_load_n(&g_dptbp_current.sync_refs, __ATOMIC_SEQ_CST) || __atomic_load_n(&g_dptbp_current.switch_refs, __ATOMIC_SEQ_CST) || __atomic_load_n(&g_dptbp_current.notifier_refs, __ATOMIC_SEQ_CST) || __atomic_load_n(&g_dptbp_current.mprotect_refs, __ATOMIC_SEQ_CST)) cond_resched();
}

// 原子发布停止切入标志；返回值仅表示本次是否首次置位，不关闭 valid 或释放资源。
static inline bool dptbp_begin_restore(void)
{
    int expected = 0;

    return __atomic_compare_exchange_n(&g_dptbp_current.restoring, &expected, 1, false, __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
}

// ======================== 线程视图选择与 TTBR0 切换 ========================

// 只允许目标 mm、目标 tgid 中尚未退出的主线程使用断点视图。
// 调用方需持资源引用或控制锁；restoring 置位后不再读取 info 中的线程条件。
static inline bool dptbp_task_uses_shadow(struct task_struct *task)
{
    return !__atomic_load_n(&g_dptbp_current.restoring, __ATOMIC_SEQ_CST) && !__atomic_load_n(&g_dptbp_current.mapping_updates, __ATOMIC_SEQ_CST) && task->mm == g_dptbp_current.mm && task->tgid == READ_ONCE(g_dptbp_current.info->tgid) && thread_group_leader(task) && !(READ_ONCE(task->flags) & PF_EXITING);
}

// 替换 TTBR0 的页表基址并保留 ASID 等其他位；私有页表清除 CnP 共享提示。
static uint64_t dptbp_compose_ttbr0(uint64_t template, phys_addr_t pgd_pa, bool shadow)
{
    // 普通 PGD 可能来自小于 PAGE_SIZE 的 slab 对象，必须替换完整 BADDR，不能保留页内地址位。
    uint64_t address_mask = phys_to_ttbr(PHYS_MASK) & ~TTBR_CNP_BIT;
    uint64_t ttbr = (template & ~address_mask) | (phys_to_ttbr(pgd_pa) & address_mask);

    if (shadow) ttbr &= ~TTBR_CNP_BIT;
    return ttbr;
}

// 更新当前 CPU 的硬件 TTBR0，返回是否发生切换；TLB 刷新由调用方完成。
static bool dptbp_write_ttbr0(phys_addr_t pgd_pa, bool shadow)
{
    uint64_t current_ttbr = read_sysreg(ttbr0_el1);
    uint64_t next_ttbr = dptbp_compose_ttbr0(current_ttbr, pgd_pa, shadow);

    if (current_ttbr == next_ttbr) return false;
    write_sysreg(next_ttbr, ttbr0_el1);
    isb();
    return true;
}

// TTBR0 PAN 模式更新线程保存的 ttbr0，供后续恢复用户访问时使用；其余模式直接写当前 CPU。
// shadow=true 选择私有根页，false 选择目标 mm 的普通根页。
static bool dptbp_write_task_view(struct task_struct *task, bool shadow)
{
    phys_addr_t pgd_pa = virt_to_phys(shadow ? g_dptbp_current.shadow_pgd : g_dptbp_current.mm->pgd);

#ifdef CONFIG_ARM64_SW_TTBR0_PAN
    if (system_uses_ttbr0_pan())
    {
        uint64_t current_ttbr = READ_ONCE(task_thread_info(task)->ttbr0);
        uint64_t next_ttbr = dptbp_compose_ttbr0(current_ttbr, pgd_pa, shadow);

        if (current_ttbr == next_ttbr) return false;
        WRITE_ONCE(task_thread_info(task)->ttbr0, next_ttbr);
        return true;
    }
#endif
    return dptbp_write_ttbr0(pgd_pa, shadow);
}

// on_each_cpu() 回调：按运行状态更新当前线程视图，调用方负责保持 context 资源有效。
static void dptbp_apply_current_view(void *unused)
{
    (void)unused;
    bool changed = false;

    if (current->mm == g_dptbp_current.mm) changed = dptbp_write_task_view(current, dptbp_task_uses_shadow(current));
    else if (!system_uses_ttbr0_pan())
    {
        // 非目标线程恢复自身地址空间；内核线程使用其借用的 active_mm。
        struct mm_struct *mm = current->mm ? current->mm : current->active_mm;
        if (mm) changed = dptbp_write_ttbr0(virt_to_phys(mm->pgd), false);
    }
    if (changed) flush_tlb_all();
}

// 按 restoring 和 mapping_updates 同步线程保存视图与各 CPU 当前视图，并刷新 TLB。
// 包含同步跨 CPU 调用，由控制路径或可阻塞 notifier 调用。
static void dptbp_restore_thread_views(void)
{
    if (system_uses_ttbr0_pan())
    {
        struct task_struct *process;
        struct task_struct *task;

        // 此处 RCU 保护任务链表遍历；context 生命周期由调用方持有的控制锁或引用保护。
        rcu_read_lock();
        for_each_process_thread(process, task)
        {
            if (task->mm == g_dptbp_current.mm) dptbp_write_task_view(task, false);
        }
        rcu_read_unlock();
    }
    on_each_cpu(dptbp_apply_current_view, NULL, 1);
    flush_tlb_all();
}

// ======================== 异常回退与任务切换（热） ========================

// 发布整组撤销意图并恢复当前目标线程，不等待其他 CPU、不回收资源。
// 即使其他入口已经置位 restoring，本次仍需恢复自己所在的线程视图。
static bool dptbp_drop_all_monitors(void)
{
    bool first_restore = dptbp_begin_restore();

    if (current->mm == g_dptbp_current.mm && dptbp_write_task_view(current, false)) flush_tlb_all();
    return first_restore;
}

// __switch_to 的第二个参数是 next；这里只调整其视图，返回 0 继续内核原有切换流程。
static int dptbp_handle_switch(struct pt_regs *hook_regs)
{
    if (unlikely(!hook_regs || !dptbp_try_get_ref(DPTBP_REF_SWITCH))) return 0;
    struct task_struct *next = (struct task_struct *)hook_regs->regs[1];

    if (next && next->mm == g_dptbp_current.mm)
    {
        bool changed = dptbp_write_task_view(next, dptbp_task_uses_shadow(next));

        // 补检选择视图到写入之间是否开始撤销；若已撤销，将刚写入的视图改回普通根页。
        if (__atomic_load_n(&g_dptbp_current.restoring, __ATOMIC_SEQ_CST) || __atomic_load_n(&g_dptbp_current.mapping_updates, __ATOMIC_SEQ_CST)) changed |= dptbp_write_task_view(next, false);
        if (changed) flush_tlb_all();
    }
    dptbp_put_ref(DPTBP_REF_SWITCH);
    return 0;
}

// 只接管目标 mm 中 PC 精确匹配断点槽位的 EL0 UDF，其余异常交回内核。
static int dptbp_handle_undef_sync(struct pt_regs *hook_regs)
{
    if (unlikely(!hook_regs)) return 0;
    uint64_t esr = read_sysreg(esr_el1);
    if (unlikely(ESR_ELx_EC(esr) != ESR_ELx_EC_UNKNOWN)) return 0;
    if (unlikely(!dptbp_try_get_ref(DPTBP_REF_SYNC))) return 0;
    int result = 0;
    struct pt_regs *regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[0];

    // hook_regs 是函数入口现场，参数 0 指向真正的用户异常寄存器现场。
    if (unlikely(!regs || !user_mode(regs) || current->mm != g_dptbp_current.mm)) goto out;

    uint64_t pc = regs->pc;
    for (size_t index = 0; index < ARRAY_SIZE(g_dptbp_current.slots); index++)
    {
        const struct dptbp_slot *slot = &g_dptbp_current.slots[index];

        if (slot->hook_addr != pc) continue;
        result = 1;
        if (!dptbp_task_uses_shadow(current))
        {
            // 撤销中、退出中或非目标主线程：保持用户 PC，切回普通页后重试原指令。
            if (dptbp_write_task_view(current, false)) flush_tlb_all();
            break;
        }

        // 槽位与共享 points 一一对应，回调接收共享对象而不是临时副本。
        struct bp_point *hit_point = &g_dptbp_current.info->points[index];
        void (*on_hit)(void *, void *, void *) = READ_ONCE(hit_point->on_hit);
        struct fp_regs fp_regs __attribute__((__uninitialized__));

        // 保存 SIMD 现场；只有回调未修改 PC 时才模拟原始指令，失败则撤销断点视图。
        read_all_q_regs(&fp_regs);
        if (on_hit) on_hit(regs, &fp_regs, hit_point);
        if (regs->pc == pc && unlikely(!emulate_inst(regs, &fp_regs, slot->orig_inst)))
        {
            if (dptbp_drop_all_monitors())
            {
                ls_log_always_tag("dptbp", "emulate failed; monitor cancelled tgid=%d tid=%d slot=%zu addr=0x%llx inst=0x%08x esr=0x%llx\n", current->tgid, current->pid, index, (unsigned long long)pc, slot->orig_inst, (unsigned long long)esr);
                for (unsigned int register_index = 0; register_index < ARRAY_SIZE(regs->regs); register_index++)
                {
                    ls_log_always_tag("dptbp", "gpr[%u]=0x%016llx\n", register_index, (unsigned long long)regs->regs[register_index]);
                }
                ls_log_always_tag("dptbp", "sp=0x%016llx pc=0x%016llx pstate=0x%016llx\n", (unsigned long long)regs->sp, (unsigned long long)regs->pc, (unsigned long long)regs->pstate);
                for (unsigned int register_index = 0; register_index < ARRAY_SIZE(fp_regs.q); register_index++)
                {
                    const __uint128_t value = fp_regs.q[register_index];
                    ls_log_always_tag("dptbp", "q[%u]=0x%016llx%016llx\n", register_index, (unsigned long long)(value >> 64), (unsigned long long)value);
                }
                ls_log_always_tag("dptbp", "fpcr=0x%08x fpsr=0x%08x\n", fp_regs.fpcr, fp_regs.fpsr);
            }
        }
        write_all_q_regs(&fp_regs);
        break;
    }
out:
    // 返回 1 跳过原异常处理函数，返回 0 继续内核处理；两条路径都归还引用。
    dptbp_put_ref(DPTBP_REF_SYNC);
    return result;
}

static pte_t *dptbp_get_shadow_pte(unsigned long addr)
{
    pgd_t *pgd = g_dptbp_current.shadow_pgd + pgd_index(addr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) return NULL;
    p4d_t *p4d = p4d_offset(pgd, addr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) return NULL;
    pud_t *pud = pud_offset(p4d, addr);
    if (pud_none(*pud) || pud_leaf(*pud) || pud_bad(*pud)) return NULL;
    pmd_t *pmd = pmd_offset(pud, addr);
    if (pmd_none(*pmd) || pmd_leaf(*pmd) || pmd_bad(*pmd)) return NULL;
    return pte_offset_kernel(pmd, addr);
}

__attribute__((no_sanitize("cfi", "kcfi"))) static int dptbp_split_vma(struct vm_area_struct *vma, unsigned long addr, int new_below)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    static int (*fn_split_vma)(struct vma_iterator *, struct vm_area_struct *, unsigned long, int);
#else
    static int (*fn_split_vma)(struct mm_struct *, struct vm_area_struct *, unsigned long, int);
#endif
    if (!fn_split_vma) fn_split_vma = (typeof(fn_split_vma))generic_kallsyms_lookup_name("split_vma");
    if (!fn_split_vma) return -ENOSYS;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 3, 0)
    VMA_ITERATOR(vmi, vma->vm_mm, vma->vm_start);
    if (vma_next(&vmi) != vma) return -ESTALE;
    WRITE_ONCE(g_dptbp_current.split_task, current);
    int status = fn_split_vma(&vmi, vma, addr, new_below);
#else
    WRITE_ONCE(g_dptbp_current.split_task, current);
    int status = fn_split_vma(vma->vm_mm, vma, addr, new_below);
#endif
    WRITE_ONCE(g_dptbp_current.split_task, NULL);
    return status;
}

__attribute__((no_sanitize("cfi", "kcfi"))) static int dptbp_handle_mprotect(struct pt_regs *hook_regs)
{
    static void (*fn_vma_set_page_prot)(struct vm_area_struct *);
    int status = 0;
    bool is_covered = false;

    if (unlikely(!hook_regs || !dptbp_try_get_ref(DPTBP_REF_MPROTECT))) return 0;
    if (unlikely(g_dptbp_current.mm != current->mm || __atomic_load_n(&g_dptbp_current.restoring, __ATOMIC_SEQ_CST))) goto unhandled;
    struct pt_regs *sys_regs = (struct pt_regs *)hook_regs->regs[0];
    if (!sys_regs) goto unhandled;
    unsigned long start = untagged_addr(sys_regs->regs[0]);
    if (!IS_ALIGNED(start, PAGE_SIZE) || !sys_regs->regs[1]) goto unhandled;
    unsigned long len = PAGE_ALIGN(sys_regs->regs[1]);
    if (!len || start + len < start) goto unhandled;
    unsigned long end = start + len;
    unsigned long prot = sys_regs->regs[2];
    if (prot & ~(PROT_READ | PROT_WRITE | PROT_EXEC)) goto unhandled;

    for (size_t index = 0; index < ARRAY_SIZE(g_dptbp_current.slots); index++)
    {
        const struct dptbp_slot *slot = &g_dptbp_current.slots[index];
        unsigned long table_start = slot->page_vaddr & PMD_MASK;
        if (slot->hook_addr && table_start < end && table_start + PMD_SIZE > start)
        {
            is_covered = true;
            break;
        }
    }
    if (!is_covered) goto unhandled;
    if (unlikely(!fn_vma_set_page_prot))
    {
        fn_vma_set_page_prot = (typeof(fn_vma_set_page_prot))generic_kallsyms_lookup_name("vma_set_page_prot");
        if (!fn_vma_set_page_prot)
        {
            status = -ENOSYS;
            goto handled;
        }
    }
    // 内核原生 mprotect 使用 if (mmap_write_lock_killable(current->mm)) return -EINTR;
    // 部分设备未导出其底层 down_write_killable，改用普通写锁。
    // 普通锁等待锁期间不再响应致命信号
    mmap_write_lock(current->mm);
    if (__atomic_load_n(&g_dptbp_current.restoring, __ATOMIC_SEQ_CST))
    {
        mmap_write_unlock(current->mm);
        goto unhandled;
    }

    unsigned long cursor = start;
    while (cursor < end)
    {
        struct vm_area_struct *vma = find_vma(current->mm, cursor);
        unsigned long eff_prot = prot;

        if (!vma || vma->vm_start > cursor)
        {
            status = -ENOMEM;
            break;
        }
        if (cursor != vma->vm_start)
        {
            status = dptbp_split_vma(vma, cursor, 1);
            if (status) break;
            vma = find_vma(current->mm, cursor);
            if (!vma || vma->vm_start != cursor)
            {
                status = -ENOMEM;
                break;
            }
        }
        unsigned long seg_end = min_t(unsigned long, vma->vm_end, end);
        if (seg_end != vma->vm_end)
        {
            status = dptbp_split_vma(vma, seg_end, 0);
            if (status) break;
        }
        if ((current->personality & READ_IMPLIES_EXEC) && (eff_prot & PROT_READ) && (vma->vm_flags & VM_MAYEXEC)) eff_prot |= PROT_EXEC;
        unsigned long newflags = calc_vm_prot_bits(eff_prot, -1) | (vma->vm_flags & ~(VM_READ | VM_WRITE | VM_EXEC | VM_FLAGS_CLEAR));
        if ((newflags & ~(newflags >> 4)) & VM_ACCESS_FLAGS)
        {
            status = -EACCES;
            break;
        }
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
        vm_flags_reset(vma, newflags);
#else
        WRITE_ONCE(vma->vm_flags, newflags);
#endif
        fn_vma_set_page_prot(vma);

        for (unsigned long page = cursor; page < seg_end; page += PAGE_SIZE)
        {
            pte_t *ptep = get_user_pte(current->mm, page);
            pte_t *shadow_ptep = dptbp_get_shadow_pte(page);
            if (ptep)
            {
                pte_t old_pte = __pte(READ_ONCE(pte_val(*ptep)));
                if (pte_present(old_pte)) set_pte(ptep, pte_modify(old_pte, vma->vm_page_prot));
            }
            if (shadow_ptep && shadow_ptep != ptep)
            {
                pte_t old_pte = __pte(READ_ONCE(pte_val(*shadow_ptep)));
                if (pte_present(old_pte)) set_pte(shadow_ptep, pte_modify(old_pte, vma->vm_page_prot));
            }
            for (size_t index = 0; index < ARRAY_SIZE(g_dptbp_current.slots); index++)
            {
                struct dptbp_slot *slot = &g_dptbp_current.slots[index];
                if (slot->hook_addr && slot->page_vaddr == page) WRITE_ONCE(slot->orig_pte, pte_modify(slot->orig_pte, vma->vm_page_prot));
            }
        }
        flush_tlb_range_all_asid_all_cpus(cursor, seg_end);
        cursor = seg_end;
    }
    mmap_write_unlock(current->mm);
handled:
    if (status) ls_log_always_tag("dptbp", "mprotect failed tgid=%d start=0x%lx end=0x%lx prot=0x%lx status=%d\n", current->tgid, start, end, prot, status);
    hook_regs->regs[0] = status;
    dptbp_put_ref(DPTBP_REF_MPROTECT);
    return 1;
unhandled:
    dptbp_put_ref(DPTBP_REF_MPROTECT);
    return 0;
}

// ======================== 映射失效通知 ========================

// notifier 释放回调只撤销视图；注销订阅和释放 mm、私有页表仍由控制面完成。
static void dptbp_mmu_notifier_release(struct mmu_notifier *notifier, struct mm_struct *mm)
{
    (void)notifier;
    if (!dptbp_try_get_ref(DPTBP_REF_NOTIFIER)) return;
    if (mm == g_dptbp_current.mm)
    {
        if (dptbp_drop_all_monitors()) ls_log_always_tag("dptbp", "monitor cancelled source=mm-release tgid=%d tid=%d\n", READ_ONCE(g_dptbp_current.info->tgid), current->pid);
        dptbp_restore_thread_views();
    }
    dptbp_put_ref(DPTBP_REF_NOTIFIER);
}

static bool dptbp_unmap_uses_shared_tables(const struct mmu_notifier_range *range)
{
    if (range->event != MMU_NOTIFY_UNMAP || range->start >= range->end || range->end > range->mm->task_size) return false;
    if ((CONFIG_PGTABLE_LEVELS == 5 && mm_p4d_folded(range->mm)) || (CONFIG_PGTABLE_LEVELS == 4 && mm_pud_folded(range->mm))) return false;

    for (unsigned long addr = range->start; addr < range->end; addr = pgd_addr_end(addr, range->end))
    {
        pgd_t source_pgd = __pgd(READ_ONCE(pgd_val(*pgd_offset(range->mm, addr))));
        pgd_t shadow_pgd = __pgd(READ_ONCE(pgd_val(g_dptbp_current.shadow_pgd[pgd_index(addr)])));
        if (pgd_none(source_pgd) || pgd_bad(source_pgd) || pgd_val(source_pgd) != pgd_val(shadow_pgd)) return false;

        unsigned long entry_start = addr & PGDIR_MASK;
        unsigned long entry_end = entry_start + PGDIR_SIZE;
        bool retained = false;
        if (entry_start < range->start)
        {
            struct vm_area_struct *vma = find_vma(range->mm, entry_start);
            retained = vma && vma->vm_start < range->start && vma->vm_start < entry_end;
        }
        if (!retained && range->end < entry_end)
        {
            struct vm_area_struct *vma = find_vma(range->mm, range->end);
            retained = vma && vma->vm_start < entry_end;
        }
        if (!retained) return false;
    }
    return true;
}

// 共享根表仍由其他 VMA 保留时，无关 UNMAP 不影响私有路径；其他失效保留撤销保护。
static int dptbp_mmu_notifier_invalidate_range_start(struct mmu_notifier *notifier, const struct mmu_notifier_range *range)
{
    (void)notifier;
    if (!dptbp_try_get_ref(DPTBP_REF_NOTIFIER)) return 0;
    int status = 0;

    if (range->mm == g_dptbp_current.mm && !dptbp_unmap_uses_shared_tables(range))
    {
        // 全 CPU 视图恢复需要可阻塞上下文；非阻塞通知返回 -EAGAIN 交由上层处理。
        if (!mmu_notifier_range_blockable(range)) status = -EAGAIN;
        else if (READ_ONCE(g_dptbp_current.split_task) == current && range->event == MMU_NOTIFY_CLEAR)
        {
            __atomic_add_fetch(&g_dptbp_current.mapping_updates, 1, __ATOMIC_SEQ_CST);
            dptbp_restore_thread_views();
        }
        else
        {
            if (dptbp_drop_all_monitors()) ls_log_always_tag("dptbp", "monitor cancelled source=mmu-invalidate tgid=%d tid=%d start=0x%lx end=0x%lx event=%u\n", READ_ONCE(g_dptbp_current.info->tgid), current->pid, range->start, range->end, range->event);
            dptbp_restore_thread_views();
        }
    }
    dptbp_put_ref(DPTBP_REF_NOTIFIER);
    return status;
}

static void dptbp_mmu_notifier_invalidate_range_end(struct mmu_notifier *notifier, const struct mmu_notifier_range *range)
{
    (void)notifier;
    if (!dptbp_try_get_ref(DPTBP_REF_NOTIFIER)) return;
    if (range->mm != g_dptbp_current.mm || READ_ONCE(g_dptbp_current.split_task) != current || range->event != MMU_NOTIFY_CLEAR) goto out;
    if (!__atomic_load_n(&g_dptbp_current.mapping_updates, __ATOMIC_SEQ_CST)) goto out;

    for (unsigned long page = range->start & PAGE_MASK; page < range->end; page += PAGE_SIZE)
    {
        pte_t *source_ptep = get_user_pte(range->mm, page);
        pte_t *shadow_ptep = dptbp_get_shadow_pte(page);
        pte_t source_pte = source_ptep ? __pte(READ_ONCE(pte_val(*source_ptep))) : __pte(0);
        pte_t shadow_pte = source_pte;

        for (size_t index = 0; index < ARRAY_SIZE(g_dptbp_current.slots); index++)
        {
            struct dptbp_slot *slot = &g_dptbp_current.slots[index];
            if (!slot->hook_addr || slot->page_vaddr != page) continue;
            if (!shadow_ptep || shadow_ptep == source_ptep || !pte_present(source_pte) || pte_pfn(source_pte) != pte_pfn(slot->orig_pte) || (pte_val(source_pte) & PTE_CONT))
            {
                if (dptbp_drop_all_monitors()) ls_log_always_tag("dptbp", "monitor cancelled source=vma-split tgid=%d page=0x%lx status=%d\n", READ_ONCE(g_dptbp_current.info->tgid), page, -ESTALE);
                goto finish;
            }
            WRITE_ONCE(slot->orig_pte, source_pte);
            shadow_pte = __pte((pte_val(source_pte) & ~__phys_to_pte_val(PHYS_MASK & PAGE_MASK)) | __phys_to_pte_val(virt_to_phys(slot->shadow_page)));
        }
        if (shadow_ptep && shadow_ptep != source_ptep) set_pte(shadow_ptep, shadow_pte);
    }
finish:
    dsb(ishst);
    __atomic_sub_fetch(&g_dptbp_current.mapping_updates, 1, __ATOMIC_SEQ_CST);
    dptbp_restore_thread_views();
out:
    dptbp_put_ref(DPTBP_REF_NOTIFIER);
}

// 地址空间释放和映射失效共用视图撤销流程。
static const struct mmu_notifier_ops dptbp_mmu_notifier_ops = {
    .release = dptbp_mmu_notifier_release,
    .invalidate_range_start = dptbp_mmu_notifier_invalidate_range_start,
    .invalidate_range_end = dptbp_mmu_notifier_invalidate_range_end,
};

// ======================== hook 与控制面资源回收 ========================

// 两个 EL0 同步入口择一安装，另外安装任务切换与受管 mprotect 入口。
static struct hook_entry g_dptbp_hooks[][1] = {
    {HOOK_ENTRY("el0t_64_sync_handler", dptbp_handle_undef_sync)},
    {HOOK_ENTRY("el0_sync_handler", dptbp_handle_undef_sync)},
    {HOOK_ENTRY("__switch_to", dptbp_handle_switch)},
    {HOOK_ENTRY("__arm64_sys_mprotect", dptbp_handle_mprotect)},
};

// 先安装异常入口，再安装任务切换入口；安装失败由 start 的统一停止路径清理。
static int dptbp_install_hooks(void)
{
    int status = inline_hook_install(g_dptbp_hooks[0]);

    if (status) status = inline_hook_install(g_dptbp_hooks[1]);
    if (status) return status;
    status = inline_hook_install(g_dptbp_hooks[2]);
    if (status) return status;
    return inline_hook_install(g_dptbp_hooks[3]);
}

// 控制面回收共享执行副本和私有页表；已发布资源必须先退出所有视图并排空引用。
static void dptbp_release_shadow_pages(void)
{
    for (size_t index = 0; index < ARRAY_SIZE(g_dptbp_current.slots); index++)
    {
        void *page = g_dptbp_current.slots[index].shadow_page;

        if (!page) continue;
        free_page((unsigned long)page);
        // 同页断点共享同一分配，清除所有别名，避免后续槽位重复释放。
        for (size_t next = index; next < ARRAY_SIZE(g_dptbp_current.slots); next++)
        {
            if (g_dptbp_current.slots[next].shadow_page == page) g_dptbp_current.slots[next].shadow_page = NULL;
        }
    }
    __builtin_memset(g_dptbp_current.slots, 0, sizeof(g_dptbp_current.slots));
    // 页表路径页按分配记录逆序释放，最后释放独立的根页。
    for (size_t index = ARRAY_SIZE(g_dptbp_current.table_pages); index > 0; index--)
    {
        if (g_dptbp_current.table_pages[index - 1]) free_page((unsigned long)g_dptbp_current.table_pages[index - 1]);
        g_dptbp_current.table_pages[index - 1] = NULL;
    }
    if (g_dptbp_current.shadow_pgd) free_page((unsigned long)g_dptbp_current.shadow_pgd);
    g_dptbp_current.shadow_pgd = NULL;
}

/*
调用方持有控制锁，统一处理正常停止和安装失败清理。
先停止 mprotect 和新的视图切入并恢复已有视图，valid 仍允许在途 UDF 进入回退分支。
注销 notifier 后再关闭资源准入，等待各类引用排空，最后释放 hook、页和 mm。
未发布成功的安装也走此路径，按实际已取得的资源逐项清理。
*/
__attribute__((no_sanitize("cfi", "kcfi"))) static void dptbp_stop_monitor_locked(void)
{
    struct mm_struct *mm = g_dptbp_current.mm;

    // 阶段一：停止视图切入，等已有切换入口退出，再恢复保存视图和各 CPU 当前视图。
    __atomic_store_n(&g_dptbp_current.restoring, 1, __ATOMIC_SEQ_CST);
    inline_hook_remove(g_dptbp_hooks[3]);
    inline_hook_remove(g_dptbp_hooks[2]);
    while (__atomic_load_n(&g_dptbp_current.switch_refs, __ATOMIC_SEQ_CST) || __atomic_load_n(&g_dptbp_current.mprotect_refs, __ATOMIC_SEQ_CST)) cond_resched();
    if (dptbp_context_valid()) dptbp_restore_thread_views();

    // 阶段二：注销 notifier，使后续资源回收不再与映射失效回调交错。
    if (g_dptbp_current.mmu_notifier.ops)
    {
        void (*unregister_fn)(struct mmu_notifier *, struct mm_struct *) = (void *)generic_kallsyms_lookup_name("mmu_notifier_unregister");

        unregister_fn(&g_dptbp_current.mmu_notifier, mm);
        g_dptbp_current.mmu_notifier.ops = NULL;
    }
    // 阶段三：关闭新资源引用并排空已有使用者，再移除异常入口和释放私有页。
    __atomic_store_n(&g_dptbp_current.valid, 0, __ATOMIC_SEQ_CST);
    dptbp_wait_refs();
    inline_hook_remove(g_dptbp_hooks[1]);
    inline_hook_remove(g_dptbp_hooks[0]);
    dptbp_release_shadow_pages();
    // 阶段四：撤销共享配置指针，归还唯一 mm 引用，清除停止标志以便下次安装。
    WRITE_ONCE(g_dptbp_current.info, NULL);
    WRITE_ONCE(g_dptbp_current.mm, NULL);
    if (mm) mmput(mm);
    __atomic_store_n(&g_dptbp_current.restoring, 0, __ATOMIC_SEQ_CST);
}

// 控制面停止入口，串行化整轮视图恢复和资源回收。
static inline void stop_dptbp_monitor(void)
{
    mutex_lock(&g_dptbp_control_lock);
    dptbp_stop_monitor_locked();
    mutex_unlock(&g_dptbp_control_lock);
}

// ======================== 私有页表路径复制 ========================

// 复制 source_table 所在整页，并记录分配归属；部分安装失败时也由统一回收路径释放。
static void *dptbp_clone_table_page(void *source_table)
{
    size_t index = 0;

    while (index < ARRAY_SIZE(g_dptbp_current.table_pages) && g_dptbp_current.table_pages[index]) index++;
    if (index == ARRAY_SIZE(g_dptbp_current.table_pages)) return ERR_PTR(-ENOSPC);
    void *page = (void *)__get_free_page(GFP_KERNEL);

    if (!page) return ERR_PTR(-ENOMEM);
    __builtin_memcpy(page, (void *)((unsigned long)source_table & PAGE_MASK), PAGE_SIZE);
    g_dptbp_current.table_pages[index] = page;
    return page;
}

/*
沿受管地址逐级复制页表路径，只改私有父表中的地址位并保留原描述符属性。
私有父项仍与源父项相等时才分配子表副本，同一路径的后续断点复用已有副本。
最后校验源 PTE 的物理页仍与复制代码时一致，将私有叶子项映射到含 UDF 的执行副本。
通过内核折叠查询处理编译期和运行时层级变化；当前实现不拆分 block 映射或 contiguous PTE。
*/
static int dptbp_map_shadow_page(const struct dptbp_slot *slot)
{
    uint64_t addr = slot->page_vaddr;
    const char *stage = "pgd-entry";
    int status = -EFAULT;
    pteval_t address_mask = __phys_to_pte_val(PHYS_MASK & PAGE_MASK);
    pgd_t *source_pgd = pgd_offset(g_dptbp_current.mm, addr);
    pgd_t *shadow_pgd = g_dptbp_current.shadow_pgd + pgd_index(addr);

    // PGD 到 P4D：只有实际存在这一层时才复制，折叠层直接沿用内核访问接口。
    if (pgd_none(*source_pgd) || pgd_bad(*source_pgd)) goto failed;
    p4d_t *source_p4d = p4d_offset(source_pgd, addr);
    p4d_t *shadow_p4d = p4d_offset(shadow_pgd, addr);
    stage = "p4d-entry";
    if (p4d_none(*source_p4d) || p4d_bad(*source_p4d)) goto failed;
    if (!mm_p4d_folded(g_dptbp_current.mm) && pgd_val(*shadow_pgd) == pgd_val(*source_pgd))
    {
        void *page = dptbp_clone_table_page(source_p4d);

        if (IS_ERR(page))
        {
            stage = "p4d-copy";
            status = PTR_ERR(page);
            goto failed;
        }
        WRITE_ONCE(*shadow_pgd, __pgd((pgd_val(*source_pgd) & ~address_mask) | __phys_to_pgd_val(virt_to_phys(page))));
        shadow_p4d = p4d_offset(shadow_pgd, addr);
    }

    // P4D 到 PUD：父项相等表示仍共享源子表，写入前需创建私有副本。
    pud_t *source_pud = pud_offset(source_p4d, addr);
    pud_t *shadow_pud = pud_offset(shadow_p4d, addr);
    stage = "pud-entry";
    status = -EOPNOTSUPP;
    if (pud_leaf(*source_pud) || pud_bad(*source_pud)) goto failed;
    if (!mm_pud_folded(g_dptbp_current.mm) && p4d_val(*shadow_p4d) == p4d_val(*source_p4d))
    {
        void *page = dptbp_clone_table_page(source_pud);

        if (IS_ERR(page))
        {
            stage = "pud-copy";
            status = PTR_ERR(page);
            goto failed;
        }
        WRITE_ONCE(*shadow_p4d, __p4d((p4d_val(*source_p4d) & ~address_mask) | __phys_to_p4d_val(virt_to_phys(page))));
        shadow_pud = pud_offset(p4d_offset(shadow_pgd, addr), addr);
    }

    // PUD 到 PMD：保留同一表页中的其他映射，只为受管路径复制下一级。
    pmd_t *source_pmd = pmd_offset(source_pud, addr);
    pmd_t *shadow_pmd = pmd_offset(shadow_pud, addr);
    stage = "pmd-entry";
    if (pmd_leaf(*source_pmd) || pmd_bad(*source_pmd)) goto failed;
    if (!mm_pmd_folded(g_dptbp_current.mm) && pud_val(*shadow_pud) == pud_val(*source_pud))
    {
        void *page = dptbp_clone_table_page(source_pmd);

        if (IS_ERR(page))
        {
            stage = "pmd-copy";
            status = PTR_ERR(page);
            goto failed;
        }
        WRITE_ONCE(*shadow_pud, __pud((pud_val(*source_pud) & ~address_mask) | __phys_to_pud_val(virt_to_phys(page))));
        shadow_pmd = pmd_offset(pud_offset(p4d_offset(shadow_pgd, addr), addr), addr);
    }

    // PMD 到 PTE：复制叶子表页后，修改不会写入目标 mm 的普通页表。
    pte_t *source_pte = pte_offset_kernel(source_pmd, addr);
    if (pmd_val(*shadow_pmd) == pmd_val(*source_pmd))
    {
        void *page = dptbp_clone_table_page(source_pte);

        if (IS_ERR(page))
        {
            stage = "pte-copy";
            status = PTR_ERR(page);
            goto failed;
        }
        WRITE_ONCE(*shadow_pmd, __pmd((pmd_val(*source_pmd) & ~address_mask) | __phys_to_pmd_val(virt_to_phys(page))));
    }
    // 防止复制代码后源物理页发生变化，也不单独改写 contiguous 映射组中的一项。
    pte_t mapping = __pte(READ_ONCE(pte_val(*source_pte)));
    stage = "pte-changed";
    status = -ESTALE;
    if (!pte_present(mapping) || pte_pfn(mapping) != pte_pfn(slot->orig_pte)) goto failed;
    stage = "pte-attributes";
    status = -EOPNOTSUPP;
    if (pte_val(mapping) & (PTE_UXN | PTE_CONT)) goto failed;

    // 使用源叶子项的权限属性，仅将 PFN 替换为执行副本的物理页。
    pte_t *shadow_pte = pte_offset_kernel(pmd_offset(pud_offset(p4d_offset(shadow_pgd, addr), addr), addr), addr);
    WRITE_ONCE(*shadow_pte, __pte((pte_val(mapping) & ~address_mask) | __phys_to_pte_val(virt_to_phys(slot->shadow_page))));
    return 0;
failed:
    ls_log_always_tag("dptbp", "map failed stage=%s page=0x%llx addr=0x%llx orig_pte=0x%llx status=%d\n", stage, (unsigned long long)addr, (unsigned long long)slot->hook_addr, (unsigned long long)pte_val(slot->orig_pte), status);
    return status;
}

// ======================== 断点资源准备与发布 ========================

static int dptbp_prepare_failed(const struct break_point *info, const char *stage, int slot_index, uint64_t addr, int status)
{
    ls_log_always_tag("dptbp", "prepare failed stage=%s tgid=%d slot=%d addr=0x%llx status=%d\n", stage, READ_ONCE(info->tgid), slot_index, (unsigned long long)addr, status);
    return status;
}

/*
调用方持有控制锁和目标 mm 的 mmap 写锁，资源准备期间 valid 保持关闭。
先按配置槽位创建或复用执行副本，缓存原指令并写入 UDF，再建立私有页表路径。
同页副本和映射只创建一次，所有断点写入完成后按页同步指令缓存。
本函数不自行释放部分资源，失败后由 start 调用统一停止路径回收。
*/
static int dptbp_prepare_slots(struct break_point *info)
{
    struct mm_struct *mm = g_dptbp_current.mm;

    for (size_t index = 0; index < ARRAY_SIZE(info->points); index++)
    {
        struct bp_point *point = &info->points[index];

        if (!bp_point_is_configured_type(point, BP_BREAKPOINT_X)) continue;
        uint64_t hook_addr = untagged_addr(READ_ONCE(point->hit_addr));
        if (hook_addr & (sizeof(uint32_t) - 1)) return dptbp_prepare_failed(info, "address-alignment", index, hook_addr, -EINVAL);
        if (!hook_addr || hook_addr >= mm->task_size || sizeof(uint32_t) > mm->task_size - hook_addr) return dptbp_prepare_failed(info, "address-range", index, hook_addr, -EFAULT);
        uint64_t page_vaddr = hook_addr & PAGE_MASK;
        struct dptbp_slot *slot = &g_dptbp_current.slots[index];

        // 拒绝重复断点地址；同页槽位复用执行副本和复制时记录的原始 PTE。
        for (size_t prev = 0; prev < index; prev++)
        {
            const struct dptbp_slot *owner = &g_dptbp_current.slots[prev];

            if (!owner->hook_addr) continue;
            if (owner->hook_addr == hook_addr) return dptbp_prepare_failed(info, "duplicate-address", index, hook_addr, -EEXIST);
            if (owner->page_vaddr != page_vaddr) continue;
            slot->shadow_page = owner->shadow_page;
            slot->orig_pte = owner->orig_pte;
        }
        if (!slot->shadow_page)
        {
            // 仅接管整页覆盖的私有可执行映射，拒绝可写页、共享页和特殊映射。
            struct vm_area_struct *vma = find_vma(mm, page_vaddr);
            if (!vma || vma->vm_start > page_vaddr || vma->vm_end - page_vaddr < PAGE_SIZE) return dptbp_prepare_failed(info, "vma-range", index, hook_addr, -EFAULT);
            vm_flags_t flags = READ_ONCE(vma->vm_flags);
            if (!(flags & VM_EXEC)) return dptbp_prepare_failed(info, "vma-execute", index, hook_addr, -EACCES);
            if (flags & (VM_WRITE | VM_SHARED | VM_MAYSHARE | VM_HUGETLB | VM_PFNMAP | VM_MIXEDMAP | VM_IO)) return dptbp_prepare_failed(info, "vma-attributes", index, hook_addr, -EOPNOTSUPP);

            pteval_t value;
            int status = read_user_pte_value(mm, page_vaddr, &value);
            if (status) return dptbp_prepare_failed(info, "read-pte", index, hook_addr, status);
            pte_t mapping = __pte(value);
            if (!pte_present(mapping) || !pfn_valid(pte_pfn(mapping))) return dptbp_prepare_failed(info, "pte-present", index, hook_addr, -EFAULT);
            if (value & (PTE_UXN | PTE_CONT)) return dptbp_prepare_failed(info, "pte-attributes", index, hook_addr, -EOPNOTSUPP);
            slot->shadow_page = (void *)__get_free_page(GFP_KERNEL);
            if (!slot->shadow_page) return dptbp_prepare_failed(info, "shadow-allocate", index, hook_addr, -ENOMEM);
            __builtin_memcpy(slot->shadow_page, page_address(pfn_to_page(pte_pfn(mapping))), PAGE_SIZE);
            slot->orig_pte = mapping;
        }

        // UDF #0 的编码为 0；先缓存原指令，再改写副本，原指令已为 UDF 时拒绝安装。
        uint32_t *instruction = (uint32_t *)((uint8_t *)slot->shadow_page + offset_in_page(hook_addr));
        slot->orig_inst = READ_ONCE(*instruction);
        if (!slot->orig_inst) return dptbp_prepare_failed(info, "original-udf", index, hook_addr, -ESTALE);
        slot->hook_addr = hook_addr;
        slot->page_vaddr = page_vaddr;
        WRITE_ONCE(*instruction, 0U);
    }

    // 根页先复制整个地址空间入口，随后只复制受管地址经过的下级页表。
    g_dptbp_current.shadow_pgd = (void *)__get_free_page(GFP_KERNEL | __GFP_ZERO);
    if (!g_dptbp_current.shadow_pgd) return dptbp_prepare_failed(info, "pgd-allocate", -1, 0, -ENOMEM);
    // 高层在运行时折叠时，内核为根表分配整页；PGD_SIZE 仍是编译期大小，不能用于截断复制。
    size_t root_size = PGD_SIZE;
    if ((CONFIG_PGTABLE_LEVELS == 5 && mm_p4d_folded(mm)) || (CONFIG_PGTABLE_LEVELS == 4 && mm_pud_folded(mm))) root_size = PAGE_SIZE;
    __builtin_memcpy(g_dptbp_current.shadow_pgd, mm->pgd, root_size);
    for (size_t index = 0; index < ARRAY_SIZE(g_dptbp_current.slots); index++)
    {
        const struct dptbp_slot *slot = &g_dptbp_current.slots[index];
        bool page_processed = false;

        if (!slot->hook_addr) continue;
        for (size_t prev = 0; prev < index; prev++)
        {
            if (g_dptbp_current.slots[prev].shadow_page == slot->shadow_page) page_processed = true;
        }
        if (page_processed) continue;
        // 此时同页全部 UDF 已写完，映射并同步整页一次即可。
        int status = dptbp_map_shadow_page(slot);
        if (status) return dptbp_prepare_failed(info, "map-shadow-page", index, slot->hook_addr, status);
        status = arm64_sync_insn_fast(slot->shadow_page, slot->page_vaddr, PAGE_SIZE);
        if (status) return dptbp_prepare_failed(info, "instruction-cache", index, slot->hook_addr, status);
    }
    // 在发布断点视图前保证私有页表写入可见。
    dsb(ishst);
    return 0;
}

/*
安装入口：串行停止旧监控，安装 hook 并取得目标 mm 引用，再准备全部私有资源。
页表准备、__mmu_notifier_register() 和运行态发布均放在 mmap 写锁内完成。
最后发布 info 与 valid 并通知各 CPU 应用视图；任一步失败都交给 stop_locked() 清理。
*/
__attribute__((no_sanitize("cfi", "kcfi"))) static int start_dptbp_monitor(struct break_point *info)
{
    if (!bp_info_find_configured_type(info, BP_BREAKPOINT_X, NULL))
    {
        ls_log_always_tag("dptbp", "start failed stage=validate-config tgid=%d status=%d\n", info ? READ_ONCE(info->tgid) : 0, -EINVAL);
        return -EINVAL;
    }
    mutex_lock(&g_dptbp_control_lock);
    dptbp_stop_monitor_locked();

    const char *stage = "install-hooks";
    int status = dptbp_install_hooks();
    if (status) goto out_release;
    stage = "get-mm";
    struct mm_struct *mm = get_mm_by_pid(READ_ONCE(info->tgid));
    if (!mm)
    {
        status = -ESRCH;
        goto out_release;
    }
    // 从这里开始 mm 引用归 context 持有，失败回滚和正常卸载统一负责 mmput()。
    WRITE_ONCE(g_dptbp_current.mm, mm);

    stage = "resolve-notifier";
    int (*register_fn)(struct mmu_notifier *, struct mm_struct *) = (void *)generic_kallsyms_lookup_name("__mmu_notifier_register");
    if (!register_fn || !generic_kallsyms_lookup_name("mmu_notifier_unregister"))
    {
        status = -ENOENT;
        goto out_release;
    }

    // __mmu_notifier_register() 要求调用方持有 mmap 写锁，准备资源和注册使用同一锁区间。
    mmap_write_lock(mm);
    stage = "prepare-slots";
    status = dptbp_prepare_slots(info);
    if (status) goto out_mmap;
    stage = "register-notifier";
    g_dptbp_current.mmu_notifier.ops = &dptbp_mmu_notifier_ops;
    status = register_fn(&g_dptbp_current.mmu_notifier, mm);
    if (status)
    {
        // 注册失败不能走注销流程；已分配的其余资源仍由统一停止路径回收。
        g_dptbp_current.mmu_notifier.ops = NULL;
        goto out_mmap;
    }

    // 先发布完整配置和资源，再允许视图切入，使首个 UDF 命中时能够取得对应槽位。
    WRITE_ONCE(g_dptbp_current.info, info);
    __atomic_store_n(&g_dptbp_current.valid, 1, __ATOMIC_SEQ_CST);
    on_each_cpu(dptbp_apply_current_view, NULL, 1);
out_mmap:
    mmap_write_unlock(mm);
out_release:
    // 清理可能注销 notifier 或归还 mm 引用，放在 mmap 写锁释放之后执行。
    if (status)
    {
        ls_log_always_tag("dptbp", "start failed stage=%s tgid=%d status=%d\n", stage, READ_ONCE(info->tgid), status);
        dptbp_stop_monitor_locked();
    }
    mutex_unlock(&g_dptbp_control_lock);
    return status;
}

#endif