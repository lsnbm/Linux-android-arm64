#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/esr.h>
#include <asm/memory.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>

#include "inline_hook_frame.h"
#include "io_struct.h"
#include "emulate_insn.h"
#include "lsdriver_log.h"

/*
单次取指异常中连续模拟的最大指令数量，避免页内循环长期占用异常上下文。
也不建议改小，定义为0x1或其他小范围

原因就是，这种类似的典型指令
retry:
    ldxr x14, [x9]        // 读取，并建立 CPU 本地独占监视
    eor  x14, x14, x11
    stxr w15, x14, [x9]   // 使用前面的独占监视尝试写入
    cbnz w15, retry
LDXR 本身只执行一次，STXR 也只执行一次；但二者需要尽量在同一段连续执行流中完成。
CPU在执行 LDXR 后保存的 reservation 不在 pt_regs 里，异常进入/返回可能清除它。
因此，
为0x1时的整体错误处理异常流程
    用户态准备执行 LDXR
        | UXN取指异常
    EL1模拟 LDXR，PC推进到 EOR
        | ERET返回用户态
    用户态准备取 EOR，还没执行
        | UXN取指异常
    EL1模拟 EOR，PC推进到 STXR
        | ERET返回用户态
    用户态准备取 STXR，还没执行
        | UXN取指异常
    EL1模拟 STXR，但独占监视已经失效，CPU在执行 LDXR 后保存的 reservation 不在 pt_regs 里，异常边界导致独占监视失效(异常进入/返回可能清除它)，会使 STXR 反复返回失败状态。
        |
    w15 = 1，PC推进到 CBNZ
        | ERET返回用户态
    用户态准备取 CBNZ
        | UXN取指异常
    EL1模拟 CBNZ，因为 w15 != 0(是STXR失败状态导致)，跳回 LDXR
*/
#define PTEBP_BATCH_INSN_LIMIT 0x10000

// 保存页面的原始 PTE，以便撤销 UXN 监控时准确恢复。
struct ptebp_page
{
    pte_t orig_pte;
    uint64_t page_vaddr;
    bool armed;
};

static struct break_point *g_ptebp_info;
static struct mm_struct *g_ptebp_mm;
static struct ptebp_page g_ptebp_pages[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);
static bool g_ptebp_stopping;

// 判断配置项是否为有效的执行断点。
static inline bool ptebp_active(const struct bp_point *point)
{
    return point->hit_addr && point->bt == BP_BREAKPOINT_X;
}

// 检查受管页面的当前 PTE 是否仍与原始 PTE 及指定标志匹配。
static bool ptebp_page_matches(const struct ptebp_page *page, struct mm_struct *mm, pteval_t flags)
{
    pteval_t mutable = 0;

    pte_t *ptep = get_user_pte(mm, page->page_vaddr);
    if (!ptep) return false;
    pte_t pte_now = READ_ONCE(*ptep);
    if (!pte_present(pte_now) || !pfn_valid(pte_pfn(pte_now))) return false;

#ifdef PTE_AF
    mutable |= PTE_AF;
#endif
#ifdef PTE_DIRTY
    mutable |= PTE_DIRTY;
#endif

    // UXN 生效期间硬件仍可能更新访问位和脏页位，比较时忽略这些可变位。
    return !((pte_val(pte_now) ^ (pte_val(page->orig_pte) | flags)) & ~mutable);
}

// 根据任意虚拟地址，查找它所在页面对应的 PTE 断点页面记录。
static struct ptebp_page *ptebp_find_page(struct ptebp_page *pages, uint64_t addr)
{
    addr &= PAGE_MASK;
    for (size_t index = 0; index < BP_CONFIG_MAX; index++)
        if (pages[index].page_vaddr && pages[index].page_vaddr == addr) return &pages[index];
    return NULL;
}

// 按指令地址查找当前配置中对应的执行断点。
static struct bp_point *ptebp_find_point(struct break_point *info, uint64_t addr)
{
    if (!info) return NULL;

    // 批量模拟期间中间指令不会再次触发页异常，因此需要主动匹配对应断点。
    addr = untagged_addr(addr) & ~0x3ULL;
    for (size_t point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct bp_point *point = &info->points[point_slot];

        if (ptebp_active(point) && (untagged_addr(point->hit_addr) & ~0x3ULL) == addr) return point;
    }
    return NULL;
}

//停止 PTE 执行断点，并尝试把所有受监控页面的 PTE 从 orig_pte | PTE_UXN 恢复为原始 orig_pte。
static void ptebp_drop_all_monitors(bool lock_mm)
{
    struct ptebp_page pages[ARRAY_SIZE(g_ptebp_pages)];
    struct mm_struct *mm;
    unsigned long flags;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    mm = g_ptebp_mm;
    if (!mm || (g_ptebp_stopping && !lock_mm))
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        return;
    }

    // 在复制待恢复页面前阻止新的批量模拟进入。
    g_ptebp_stopping = true;
    __builtin_memcpy(pages, g_ptebp_pages, sizeof(pages));
    if (lock_mm)
    {
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
        mmap_read_lock(mm);
        spin_lock_irqsave(&g_ptebp_lock, flags);
    }

    for (size_t point_slot = 0; point_slot < ARRAY_SIZE(pages); point_slot++)
    {
        struct ptebp_page *page = &pages[point_slot];
        struct ptebp_page *live = &g_ptebp_pages[point_slot];

        if (!page->page_vaddr || !page->armed) continue;

        // 页面 PTE 若已被其他路径修改，则不能用旧快照覆盖。
        if (!ptebp_page_matches(page, mm, PTE_UXN)) live->armed = false;
        else if (!write_user_pte_value(mm, page->page_vaddr, pte_val(page->orig_pte))) live->armed = false;
    }

    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (lock_mm) mmap_read_unlock(mm);
}

// 清空 PTE 执行断点的全部软件状态，并释放启动监控时持有的 mm_struct 引用
static void ptebp_clear_monitors(void)
{
    struct break_point *info;
    struct mm_struct *mm;
    unsigned long flags;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    info = g_ptebp_info;
    mm = g_ptebp_mm;
    g_ptebp_info = NULL;
    g_ptebp_mm = NULL;
    __builtin_memset(g_ptebp_pages, 0, sizeof(g_ptebp_pages));
    g_ptebp_stopping = false;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (mm) mmput(mm);
    if (info) __builtin_memset(info, 0, sizeof(*info));
}

/*

hook工作函数，使用HOOK_ENTRY("el0t_64_sync_handler", ptebp_handle_exec_fault)挂接
接管用户态三级指令(L3)权限异常

TTBRx
  |
  硬件MMU层级           Linux内核命名
L0 条目：512GiB 块          PGD
L1 条目：1GiB   块          PUD
L2 条目：2MiB   块          PMD
L3 条目：4KiB   页          PTE
  |
物理页

*/
// 处理受管 UXN 页的用户态取指异常，并在一次异常中批量模拟当前页指令。
static int ptebp_handle_exec_fault(struct pt_regs *hook_regs)
{
    // 最先读取并过滤异常类型，非 EL0 三级指令权限异常不访问任何 PTEBP 状态。
    uint64_t esr = read_sysreg(esr_el1);
    if (ESR_ELx_EC(esr) != ESR_ELx_EC_IABT_LOW || (esr & ESR_ELx_FSC) != (ESR_ELx_FSC_PERM | ESR_ELx_FSC_LEVEL)) return 0;

    // info 是本次异常处理使用的配置快照；两个布尔值分别描述页面归属和停止阶段。
    struct break_point *info = NULL;
    unsigned long flags;
    bool managed_page = false;
    bool stopping = false;

    // 内核态软件寄存器现场 为空，交回原函数处理。
    if (!hook_regs) return 0;

    // 用户态软件寄存器现场 el0t_64_sync_handler(regs) 的唯一参数位于 x0。
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[0];

    // IABT_LOW 已经确认异常来自 EL0；这里只需验证真实寄存器现场和用户地址空间。
    if (!regs || !current->mm) return 0;

    // 去掉 ARM64 地址标签MTE，再清除低 2 位，得到对齐后的指令地址。
    uint64_t pc = untagged_addr(regs->pc) & ~0x3ULL;

    // 页面表、目标 mm 和停止标志共享同一把锁；先在锁内判断异常是否属于当前监控实例。
    spin_lock_irqsave(&g_ptebp_lock, flags);

    // 即使虚拟地址相同，不同 mm 中也可能对应完全不同的映射，必须精确匹配目标地址空间。
    if (g_ptebp_mm != current->mm) goto out_unlock;

    // 保存停止状态的锁内快照。停止期间不再取得 info，确保后面不会启动新的批量模拟。
    stopping = g_ptebp_stopping;
    if (!stopping) info = g_ptebp_info;
    {
        // PTE_UXN 是按页安装的，因此先确认故障 PC 所在页确实存在于受管页面表中。
        struct ptebp_page *page = ptebp_find_page(g_ptebp_pages, pc);

        // 正常运行时仅接管 armed 页面；停止阶段也接管已恢复页面产生的迟到异常。
        if (!page || (!page->armed && !stopping)) goto out_unlock;
        managed_page = true;
    }

out_unlock:
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    // 页面不属于当前 PTE 断点时返回 0，让 hook 跳板继续执行原 el0t_64_sync_handler。
    if (!managed_page) return 0;
    // 停止阶段的受管迟到异常已经完成归属确认，直接跳过原异常处理函数。
    if (stopping) goto handled;

    // 模拟器使用独立的软件 FP/SIMD 现场。整批只在开始时读取一次，避免每条指令重复搬运 Q0-Q31、FPCR 和 FPSR。
    struct fp_regs fp_regs;
    read_all_q_regs(&fp_regs);

    //取出本次异常发生时，PC 所在页面的起始虚拟地址，并将它作为本批指令模拟的页面边界。
    uint64_t batch_page = pc & PAGE_MASK;

    //记录本次取指异常中已经成功模拟了多少条指令。
    unsigned int executed = 0;

    //本批指令模拟是否以安全状态结束，能否继续保留 PTE UXN 监控。
    bool batch_ok = true;

    //只要当前待执行的 PC 仍位于本次触发异常的页面中，就继续在内核里模拟下一条指令。
    while ((untagged_addr(regs->pc) & PAGE_MASK) == batch_page)
    {
        // 达到上限不是模拟失败：保留 UXN 并返回，当前页下一次取指异常会继续下一批。
        if (executed >= PTEBP_BATCH_INSN_LIMIT) break;

        // 锁外批量模拟期间监控可能被另一 CPU 停止或替换；每条指令前都验证原配置仍然有效。
        if (READ_ONCE(g_ptebp_stopping) || READ_ONCE(g_ptebp_mm) != current->mm || READ_ONCE(g_ptebp_info) != info) break;

        // UXN 只能报告“进入了受管页”，页内的精确断点需要按当前 PC 在软件中逐条匹配。
        struct bp_point *hit_point = ptebp_find_point(info, regs->pc);
        if (hit_point && hit_point->on_hit)
        {
            // 通用寄存器和完整 FP/SIMD 状态都直接使用当前软件现场，回调修改会由后续模拟继续继承。
            hit_point->on_hit(regs, &fp_regs, hit_point);
        }

        // 断点回调执行后，PC 是否仍在本批模拟的原始页面中。
        if ((untagged_addr(regs->pc) & PAGE_MASK) != batch_page) break;

        // 保存模拟前 PC，用于防止模拟器报告成功却没有推进执行流，进而形成无限异常循环。
        uint64_t old_pc = regs->pc;

        // emulate_insn 同时更新 regs 和软件 FP/SIMD 现场；不支持的指令或 PC 未推进都使本批不再安全。
        bool emulated = emulate_insn(regs, &fp_regs, 0);
        if (!emulated || regs->pc == old_pc)
        {
            batch_ok = false;
            break;
        }
        executed++;
    }

    // 将最后一批软件模拟结果提交到真实 Q0-Q31、FPCR 和 FPSR，使返回 EL0 后看到与原生执行一致的寄存器状态。
    write_all_q_regs(&fp_regs);

    // 模拟失败时不能让同一条 UXN 指令持续重入异常；撤销整组监控后让用户代码从当前 PC 原生重试。
    if (!batch_ok) ptebp_drop_all_monitors(false);

handled:
    // work_fn 返回 1 会让 hook 跳板跳过原 el0t_64_sync_handler，直接进入 ret_to_user。
    return 1;
}

static struct hook_entry g_ptebp_fault_hooks[] = {
    HOOK_ENTRY("el0t_64_sync_handler", ptebp_handle_exec_fault),
};

// 停止 PTE 执行断点，移除异常钩子并清理全部监控状态。
static inline void stop_ptebp_monitor(void)
{
    ptebp_drop_all_monitors(true);
    inline_hook_remove(g_ptebp_fault_hooks);
    ptebp_clear_monitors();
}

// 为指定执行断点所在页面保存原始 PTE 并设置 UXN 监控。
static int ptebp_install_page(struct break_point *info, size_t point_slot, struct mm_struct *mm)
{
    struct bp_point *point = &info->points[point_slot];
    uint64_t hook_addr = untagged_addr(point->hit_addr) & ~0x3ULL;
    if (!hook_addr || hook_addr >= READ_ONCE(mm->task_size) || sizeof(uint32_t) > READ_ONCE(mm->task_size) - hook_addr)
    {
        ls_log_tag("ptebp", "install page rejected tgid=%d slot=%zu addr=0x%llx task_size=0x%llx status=%d\n", info->tgid, point_slot, (unsigned long long)hook_addr, (unsigned long long)READ_ONCE(mm->task_size), -EFAULT);
        return -EFAULT;
    }
    uint64_t page_vaddr = hook_addr & PAGE_MASK;
    ls_log_tag("ptebp", "install page begin tgid=%d slot=%zu addr=0x%llx page=0x%llx\n", info->tgid, point_slot, (unsigned long long)hook_addr, (unsigned long long)page_vaddr);

    for (size_t scan_slot = 0; scan_slot < point_slot; scan_slot++)
    {
        struct bp_point *candidate = &info->points[scan_slot];

        if (ptebp_active(candidate) && (untagged_addr(candidate->hit_addr) & ~0x3ULL) == hook_addr)
        {
            ls_log_tag("ptebp", "install page duplicate tgid=%d slot=%zu previous_slot=%zu addr=0x%llx status=%d\n", info->tgid, point_slot, scan_slot, (unsigned long long)hook_addr, -EEXIST);
            return -EEXIST;
        }
    }

    struct ptebp_page *page = ptebp_find_page(g_ptebp_pages, page_vaddr);
    if (page)
    {
        int status = ptebp_page_matches(page, mm, PTE_UXN) ? 0 : -EFAULT;
        ls_log_tag("ptebp", "install page reused tgid=%d slot=%zu page=0x%llx armed=%d status=%d\n", info->tgid, point_slot, (unsigned long long)page_vaddr, page->armed, status);
        return status;
    }

    pte_t *ptep = get_user_pte(mm, page_vaddr);
    if (!ptep)
    {
        ls_log_tag("ptebp", "install page no pte tgid=%d slot=%zu page=0x%llx status=%d\n", info->tgid, point_slot, (unsigned long long)page_vaddr, -EFAULT);
        return -EFAULT;
    }

    pte_t orig_pte = READ_ONCE(*ptep);
    ls_log_tag("ptebp", "install page pte tgid=%d slot=%zu page=0x%llx ptep=0x%llx orig=0x%llx present=%d pfn_valid=%d uxn=%d\n", info->tgid, point_slot, (unsigned long long)page_vaddr, (unsigned long long)ptep, (unsigned long long)pte_val(orig_pte), pte_present(orig_pte), pfn_valid(pte_pfn(orig_pte)), !!(pte_val(orig_pte) & PTE_UXN));
    if (!pte_present(orig_pte) || !pfn_valid(pte_pfn(orig_pte)))
    {
        ls_log_tag("ptebp", "install page invalid pte tgid=%d slot=%zu page=0x%llx status=%d\n", info->tgid, point_slot, (unsigned long long)page_vaddr, -EFAULT);
        return -EFAULT;
    }
    if (pte_val(orig_pte) & PTE_UXN)
    {
        ls_log_tag("ptebp", "install page already uxn tgid=%d slot=%zu page=0x%llx status=%d\n", info->tgid, point_slot, (unsigned long long)page_vaddr, -EACCES);
        return -EACCES;
    }

    int status = write_user_pte_value(mm, page_vaddr, pte_val(orig_pte) | PTE_UXN);
    ls_log_tag("ptebp", "install page write tgid=%d slot=%zu page=0x%llx requested=0x%llx readback=0x%llx status=%d\n", info->tgid, point_slot, (unsigned long long)page_vaddr, (unsigned long long)(pte_val(orig_pte) | PTE_UXN), (unsigned long long)pte_val(READ_ONCE(*ptep)), status);
    if (status) return status;
    page = &g_ptebp_pages[point_slot];
    *page = (struct ptebp_page){.orig_pte = orig_pte, .page_vaddr = page_vaddr, .armed = true};
    ls_log_tag("ptebp", "install page ok tgid=%d slot=%zu addr=0x%llx page=0x%llx\n", info->tgid, point_slot, (unsigned long long)hook_addr, (unsigned long long)page_vaddr);
    return 0;
}

// 校验断点配置、安装异常钩子并启用全部 PTE 执行断点页面。
static inline int start_ptebp_monitor(struct break_point *info)
{
    int status;
    size_t point_slot;
    struct mm_struct *mm;
    unsigned long flags;

    if (!info || info->tgid <= 0)
    {
        ls_log_tag("ptebp", "start rejected info=0x%llx tgid=%d status=%d\n", (unsigned long long)info, info ? info->tgid : -1, -EINVAL);
        return -EINVAL;
    }

    ls_log_tag("ptebp", "start begin tgid=%d\n", info->tgid);

    for (point_slot = 0; point_slot < ARRAY_SIZE(info->points); point_slot++)
        if (ptebp_active(&info->points[point_slot])) break;
    if (point_slot == ARRAY_SIZE(info->points))
    {
        ls_log_tag("ptebp", "start rejected tgid=%d no active execute point status=%d\n", info->tgid, -EINVAL);
        return -EINVAL;
    }

    mm = get_mm_by_pid(info->tgid);
    if (!mm)
    {
        ls_log_tag("ptebp", "start get mm failed tgid=%d status=%d\n", info->tgid, -EINVAL);
        return -EINVAL;
    }

    status = inline_hook_install(g_ptebp_fault_hooks);
    if (status)
    {
        ls_log_tag("ptebp", "start hook install failed tgid=%d status=%d\n", info->tgid, status);
        goto err_put_mm;
    }
    ls_log_tag("ptebp", "start hook installed tgid=%d target=0x%llx\n", info->tgid, (unsigned long long)g_ptebp_fault_hooks[0].target_addr);

    mmap_read_lock(mm);
    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_mm = mm;
    for (point_slot = 0; point_slot < ARRAY_SIZE(info->points); point_slot++)
    {
        if (!ptebp_active(&info->points[point_slot])) continue;
        status = ptebp_install_page(info, point_slot, mm);
        if (status) break;
    }
    if (!status) g_ptebp_info = info;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    mmap_read_unlock(mm);

    if (!status)
    {
        ls_log_tag("ptebp", "start ok tgid=%d mm=0x%llx\n", info->tgid, (unsigned long long)mm);
        return 0;
    }

    ls_log_tag("ptebp", "start page install failed tgid=%d slot=%zu status=%d, cleaning up\n", info->tgid, point_slot, status);
    stop_ptebp_monitor();
    return status;

err_put_mm:
    mmput(mm);
    return status;
}

#endif // 结束头文件保护