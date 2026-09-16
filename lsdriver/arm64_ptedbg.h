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
#include "arm64_emulate/emulate_inst.h"
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
#define PTEBP_BATCH_INST_LIMIT 0x10000

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

//停止 PTE 执行断点，并尝试把所有受监控页面的 PTE 从 orig_pte | PTE_UXN 恢复为原始 orig_pte。
static void ptebp_drop_all_monitors(bool lock_mm)
{
    struct ptebp_page pages[ARRAY_SIZE(g_ptebp_pages)];
    struct mm_struct *mm;
    unsigned long flags;
    // 单行日志统一报告：mode 区分卸载来源(stop=主动停止/fault=异常回滚)，cause 记录首个失败原因。
    uint64_t fail_page = 0;
    const char *cause = "ok";

    spin_lock_irqsave(&g_ptebp_lock, flags);
    mm = g_ptebp_mm;
    if (!mm || (g_ptebp_stopping && !lock_mm))
    {
        cause = mm ? "already-stopping" : "no-mm";
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
    }
    else
    {
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

            // 页面 PTE 若已被其他路径修改，则不能用旧快照覆盖；只记录首个失败页。
            bool match = ptebp_page_matches(page, mm, PTE_UXN);
            if (match && write_user_pte_value(mm, page->page_vaddr, pte_val(page->orig_pte))) continue;
            live->armed = false;
            if (fail_page) continue;
            fail_page = page->page_vaddr;
            cause = match ? "pte-writeback" : "pte-mismatch";
        }

        spin_unlock_irqrestore(&g_ptebp_lock, flags);

        if (lock_mm) mmap_read_unlock(mm);
    }

    ls_log_always_tag("ptebp", "drop mode=%s cause=%s page=0x%llx\n", lock_mm ? "stop" : "fault", cause, (unsigned long long)fail_page);
}

// 清空 PTE 执行断点的全部软件状态，并释放启动监控时持有的 mm_struct 引用
static void ptebp_clear_monitors(void)
{
    struct mm_struct *mm;
    unsigned long flags;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    mm = g_ptebp_mm;
    g_ptebp_info = NULL;
    g_ptebp_mm = NULL;
    __builtin_memset(g_ptebp_pages, 0, sizeof(g_ptebp_pages));
    g_ptebp_stopping = false;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);

    if (mm) mmput(mm);
}

/*

hook工作函数，挂接内核提供的 EL0 同步异常入口
接管用户态三级指令(L3)权限异常；不同内核版本的入口符号名不同

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
    // 用户态软件寄存器现场是 el0t_64_sync_handler(regs) 的唯一参数，保存在 hook 入口 x0 中。
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[0];
    struct mm_struct *mm = current->mm;

    // info 是本次异常批量模拟使用的无锁配置快照。
    struct break_point *info;
    struct ptebp_page *page;
    struct bp_point *primary_point;
    struct fp_regs fp_regs;
    uint64_t page_start;
    uint64_t page_end;
    uint64_t esr = read_sysreg(esr_el1);
    bool batch_ok = true;

    // IABT_LOW 已确认异常来自 EL0；非 L3 指令权限异常不访问 PTEBP 状态。
    if (ESR_ELx_EC(esr) != ESR_ELx_EC_IABT_LOW || (esr & ESR_ELx_FSC) != (ESR_ELx_FSC_PERM | ESR_ELx_FSC_LEVEL)) return 0;

    // 即使虚拟地址相同，不同 mm 中也可能对应完全不同的映射，必须精确匹配目标地址空间。
    if (READ_ONCE(g_ptebp_mm) != mm) return 0;
    info = READ_ONCE(g_ptebp_info);

    // PTE_UXN 按页安装，因此故障 PC 所在页必须属于受管页面表。
    page = ptebp_find_page(g_ptebp_pages, regs->pc);
    if (!page) return 0;

    // 停止阶段的受管迟到异常已经完成归属确认，直接跳过原异常处理函数。
    if (READ_ONCE(g_ptebp_stopping)) return 1;

    // 页面必须仍处于启用状态，且整组断点配置已经完整发布。
    if (!READ_ONCE(page->armed) || !info) return 0;

    // 页面记录位于首个断点对应槽位；同页其他断点仍可通过配置表查找。C中 两个指向同一数组元素的同类型会自动除以元素大小，结果是它们之间相隔的元素数量
    primary_point = &info->points[page - g_ptebp_pages];

    // 以已确认的受管页记录固定本批模拟区间；FP/SIMD 现场整批只读取一次。
    page_start = READ_ONCE(page->page_vaddr);
    page_end = page_start + PAGE_SIZE;
    read_all_q_regs(&fp_regs);

    // 每条指令前主动确认 PC 仍在受管页 [page_start, page_end)；达到上限后返回 EL0，由 UXN 触发下一批。
    for (uint32_t executed = 0; executed < PTEBP_BATCH_INST_LIMIT && regs->pc >= page_start && regs->pc < page_end; executed++)
    {
        // 批量模拟期间监控可能被另一 CPU 停止或替换；每条指令前都验证原配置仍然有效。
        if (READ_ONCE(g_ptebp_stopping) || READ_ONCE(g_ptebp_mm) != mm || READ_ONCE(g_ptebp_info) != info) break;

        // 常见的单断点页直接命中页面所属槽位；同页其他断点才扫描配置表。
        struct bp_point *hit_point = READ_ONCE(primary_point->hit_addr) == regs->pc ? primary_point : bp_info_find_point_by_pc(info, regs->pc);
        if (hit_point && hit_point->on_hit)
        {
            // 回调直接修改本批使用的 GPR 和 FP/SIMD 软件现场，后续模拟继承修改结果。
            hit_point->on_hit(regs, &fp_regs, hit_point);
        }

        // 回调若把 PC 改出当前受管页区间，本批立即结束并从新 PC 返回用户态。
        if (regs->pc < page_start || regs->pc >= page_end) break;

        // 模拟器负责更新现场和 PC；无法模拟时撤销监控，让当前指令原生重试。
        if (!emulate_inst(regs, &fp_regs, 0))
        {
            batch_ok = false;
            break;
        }
    }

    // 将本批最终 FP/SIMD 软件现场提交回 CPU。
    write_all_q_regs(&fp_regs);

    // 模拟失败时恢复受管页执行权限，避免同一条 UXN 指令持续重入异常。
    if (!batch_ok) ptebp_drop_all_monitors(false);

    // 返回 1 让 hook 跳板跳过原 EL0 同步异常处理函数，直接进入 ret_to_user。
    return 1;
}

static struct hook_entry g_ptebp_fault_hooks[][1] = {
    {HOOK_ENTRY("el0t_64_sync_handler", ptebp_handle_exec_fault)},
    {HOOK_ENTRY("el0_sync_handler", ptebp_handle_exec_fault)},
};

// 停止 PTE 执行断点，移除异常钩子并清理全部监控状态。
static inline void stop_ptebp_monitor(void)
{
    ptebp_drop_all_monitors(true);
    for (size_t index = 0; index < ARRAY_SIZE(g_ptebp_fault_hooks); index++) inline_hook_remove(g_ptebp_fault_hooks[index]);
    ptebp_clear_monitors();
}

// 按地址顺序输出受管页的全部 ARM64 指令，每行 1 条。
static void ptebp_log_page_instructions(uint64_t page_vaddr, pte_t pte)
{
    const uint32_t *instructions = page_address(pfn_to_page(pte_pfn(pte)));

    if (!instructions)
    {
        ls_log_always_tag("ptebp", "managed page instructions unavailable page=0x%llx pfn=0x%llx\n", (unsigned long long)page_vaddr, (unsigned long long)pte_pfn(pte));
        return;
    }

    ls_log_always_tag("ptebp", "managed page instructions begin page=0x%llx count=%zu\n", (unsigned long long)page_vaddr, PAGE_SIZE / sizeof(*instructions));
    for (size_t instruction_index = 0; instruction_index < PAGE_SIZE / sizeof(*instructions); instruction_index++)
    {
        ls_log_always_tag("ptebp", "0x%010llX        %08X\n", (unsigned long long)(page_vaddr + instruction_index * sizeof(*instructions)), instructions[instruction_index]);
    }
    ls_log_always_tag("ptebp", "managed page instructions end page=0x%llx\n", (unsigned long long)page_vaddr);
}

// 为指定执行断点所在页面保存原始 PTE 并设置 UXN 监控。
static int ptebp_install_page(struct break_point *info, size_t point_slot, struct mm_struct *mm)
{
    struct bp_point *point = &info->points[point_slot];
    uint64_t hook_addr = untagged_addr(point->hit_addr) & ~0x3ULL;
    if (!hook_addr || hook_addr >= READ_ONCE(mm->task_size) || sizeof(uint32_t) > READ_ONCE(mm->task_size) - hook_addr) return -EFAULT;
    uint64_t page_vaddr = hook_addr & PAGE_MASK;

    struct bp_point *duplicate_point = bp_info_find_point_by_pc(info, hook_addr);
    if (duplicate_point != point) return -EEXIST;

    struct ptebp_page *page = ptebp_find_page(g_ptebp_pages, page_vaddr);
    if (page) return ptebp_page_matches(page, mm, PTE_UXN) ? 0 : -EFAULT;

    pte_t *ptep = get_user_pte(mm, page_vaddr);
    if (!ptep) return -EFAULT;

    pte_t orig_pte = READ_ONCE(*ptep);
    if (!pte_present(orig_pte) || !pfn_valid(pte_pfn(orig_pte))) return -EFAULT;
    if (pte_val(orig_pte) & PTE_UXN) return -EACCES;

    ptebp_log_page_instructions(page_vaddr, orig_pte);

    int status = write_user_pte_value(mm, page_vaddr, pte_val(orig_pte) | PTE_UXN);
    if (status) return status;
    page = &g_ptebp_pages[point_slot];
    *page = (struct ptebp_page){.orig_pte = orig_pte, .page_vaddr = page_vaddr, .armed = true};
    return 0;
}

// 校验断点配置、安装异常钩子并启用全部 PTE 执行断点页面。
static inline int start_ptebp_monitor(struct break_point *info)
{
    int status;
    size_t hook_index;
    size_t point_slot;
    struct mm_struct *mm;
    unsigned long flags;

    if (!bp_info_is_valid(info))
    {
        ls_log_always_tag("ptebp", "start failed tgid=%d phase=config status=%d\n", info ? info->tgid : -1, -EINVAL);
        return -EINVAL;
    }

    if (!bp_info_find_configured_type(info, BP_BREAKPOINT_X, NULL))
    {
        ls_log_always_tag("ptebp", "start failed tgid=%d phase=points status=%d\n", info->tgid, -EINVAL);
        return -EINVAL;
    }

    mm = get_mm_by_pid(info->tgid);
    if (!mm)
    {
        ls_log_always_tag("ptebp", "start failed tgid=%d phase=mm status=%d\n", info->tgid, -EINVAL);
        return -EINVAL;
    }

    status = -ENOENT;
    for (hook_index = 0; hook_index < ARRAY_SIZE(g_ptebp_fault_hooks); hook_index++)
    {
        status = inline_hook_install(g_ptebp_fault_hooks[hook_index]);
        if (!status) break;
    }
    if (status)
    {
        ls_log_always_tag("ptebp", "start failed tgid=%d phase=hook status=%d\n", info->tgid, status);
        goto err_put_mm;
    }

    mmap_read_lock(mm);
    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_mm = mm;
    size_t next_slot = 0;
    struct bp_point *point;
    while ((point = bp_info_find_configured_type(info, BP_BREAKPOINT_X, &next_slot)))
    {
        point_slot = point - info->points;
        status = ptebp_install_page(info, point_slot, mm);
        if (status) break;
    }
    if (!status) g_ptebp_info = info;
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
    mmap_read_unlock(mm);

    if (!status)
    {
        ls_log_always_tag("ptebp", "start ok tgid=%d symbol=%s target=0x%llx mm=0x%llx\n", info->tgid, g_ptebp_fault_hooks[hook_index][0].target_sym, (unsigned long long)g_ptebp_fault_hooks[hook_index][0].target_addr, (unsigned long long)mm);
        return 0;
    }

    ls_log_always_tag("ptebp", "start failed tgid=%d phase=page slot=%zu status=%d\n", info->tgid, point_slot, status);
    stop_ptebp_monitor();
    return status;

err_put_mm:
    mmput(mm);
    return status;
}

#endif // 结束头文件保护