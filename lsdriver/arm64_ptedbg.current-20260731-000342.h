#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

/*
本头文件直接实现基于 UXN 页异常和幽灵页的 ARM64 页表执行断点。
安装、异常接管、断点回调、指令模拟和停止处理均集中在此处。
*/

#include <linux/atomic.h>
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/vmalloc.h>
#include <linux/wait.h>
#include <asm/memory.h>
#include <asm/ptrace.h>

#include "arm64_ghost_region.h"
#include "arm64_page_reloc.h"
#include "emulate_insn.h"
#include "inline_hook_frame.h"
#include "io_struct.h"
#include "lsdriver_log.h"

#define PTEBP_ESR_EC_IABT_LOW 0x20        //来自较低异常级的指令访问异常，当前内核处于 EL1，目标进程运行在 EL0，因此源页因 PTE_UXN 无法取指时，异常类别就是这个值。
#define PTEBP_ESR_FSC_PERM_L3 0x0f        //三级页表末级 PTE 的权限异常，pte是在页表第4级
#define PTEBP_ESR_FSC_MASK    0x3f        //这是用于提取 ESR_EL1.ISS.FSC 的掩码， ESR 的 [31:26] 位提取 EC
#define PTEBP_MONITOR_BRK     0xD4200000U //BRK #0的机器码

//表示一个被 PTEBP 接管的一整个受监控的源代码页，以及与它配套的幽灵区域
struct ptebp_page
{
    bool used;  //表示这个数组槽位是否已经分配给某个源页
    bool armed; //表示源页是否已经设置了 PTE_UXN，即断点是否正式启用

    pte_t orig_pte;                          // 原页表项
    uint64_t source_page;                    // 原代码页地址
    uint32_t *source_code;                   // 原始页机器码快照
    struct arm64_ghost_region ghost;         // 这个源页对应的幽灵区域句柄
    struct arm64_page_relocation relocation; //源页与幽灵页地址映射
};

/*
这里在直接持有g_ptebp_info共享内存断点配置结构体指针的情况下还创建新ptebp_point结构体
是因为PTEBP 安装后会产生外部断点配置中不存在的运行状态，例如实际幽灵地址、规范化后的源地址、被 BRK 替换的原指令和安装有效状态，因此需要用g_ptebp_points[]单独保存
*/
//记录一个具体监控地址的状态。每个有效的 info->points[index] 对应一个相同索引的 g_ptebp_points[index]
struct ptebp_point
{
    uint64_t source_pc; //用户配置的源指令地址，会进行地址标签清理和 4 字节对齐
    uint64_t ghost_pc;  //source_pc 在幽灵页中的同偏移地址
    uint32_t orig_insn; //source_pc 原本的 32 位机器码。
    bool used;          //表示这个监控点状态是否有效
};

static struct break_point *g_ptebp_info;
static struct ptebp_page g_ptebp_pages[BP_CONFIG_MAX];
static struct ptebp_point g_ptebp_points[BP_CONFIG_MAX];
static struct mm_struct *g_ptebp_mm;
static DEFINE_SPINLOCK(g_ptebp_lock);                       //运行状态 自旋锁 不能睡眠，保护异常处理路径和控制路径共同访问的全局状态
static DEFINE_MUTEX(g_ptebp_mutex);                         //实例生命周期 互斥锁 可以睡眠，用于串行化完整的启动和停止操作
static atomic_t g_ptebp_handlers_inflight = ATOMIC_INIT(0); //原子计数,记录还有多少异常处理函数正在执行
static DECLARE_WAIT_QUEUE_HEAD(g_ptebp_handler_wait);       //让调用停止函数的线程睡眠，直到原子计数归零唤醒继续清理
static bool g_ptebp_stopping;                               //阻止停止期间继续产生正常监控行为

// 判断断点配置项是否为已启用的执行断点。
static __always_inline bool ptebp_active(const struct bp_point *point)
{
    return point && READ_ONCE(point->hit_addr) && READ_ONCE(point->bt) == BP_BREAKPOINT_X;
}

// 判断机器码是否属于 ARM64 BRK 指令族。
static __always_inline bool ptebp_is_brk(uint32_t instruction)
{
    return (instruction & 0xFFE0001FU) == PTEBP_MONITOR_BRK;
}

// 增加正在执行的异常处理函数计数，阻止停止流程提前释放资源。
static inline void ptebp_handler_enter(void)
{
    atomic_inc(&g_ptebp_handlers_inflight);
}

// 减少异常处理函数计数，并在最后一个处理函数退出时唤醒停止流程。
static inline void ptebp_handler_leave(void)
{
    if (atomic_dec_and_test(&g_ptebp_handlers_inflight)) wake_up_all(&g_ptebp_handler_wait);
}

// 检查源页当前的 PTE，是否仍然是 PTEBP 安装时保存的 orig_pte，并允许调用者指定当前应该额外带有哪些标志位。
static bool ptebp_page_matches(const struct ptebp_page *page, struct mm_struct *mm, pteval_t flags)
{
    if (!page || !page->used || !mm) return false;

    pte_t *ptep = get_user_pte(mm, page->source_page);
    if (!ptep) return false;
    pte_t pte_now = READ_ONCE(*ptep);
    if (!pte_present(pte_now) || !pfn_valid(pte_pfn(pte_now))) return false;

    return !((pte_val(pte_now) ^ (pte_val(page->orig_pte) | flags)) & ~(PTE_AF | PTE_DIRTY));
}

// 根据源代码pc地址，找到它所属源页对应的 struct ptebp_page 运行状态。
static struct ptebp_page *ptebp_find_source_page(uint64_t source_pc)
{
    uint64_t source_page = source_pc & PAGE_MASK;

    for (size_t page_index = 0; page_index < ARRAY_SIZE(g_ptebp_pages); page_index++)
    {
        struct ptebp_page *page = &g_ptebp_pages[page_index];
        if (page->used && page->source_page == source_page) return page;
    }

    return NULL;
}

// 根据幽灵ghost_pc地址，找到它所属幽灵页对应的 struct ptebp_page 运行状态。
static struct ptebp_page *ptebp_find_ghost_page(uint64_t ghost_pc)
{
    for (size_t page_index = 0; page_index < ARRAY_SIZE(g_ptebp_pages); page_index++)
    {
        struct ptebp_page *page = &g_ptebp_pages[page_index];
        if (!page->used || !page->ghost.user_va) continue;
        if (ghost_pc >= page->ghost.user_va && ghost_pc < page->ghost.user_va + page->relocation.code_size) return page;
    }

    return NULL;
}

// 在全局源页状态数组 g_ptebp_pages[] 中，找到第一个尚未使用的槽位，并返回它的数组下标。
static int ptebp_find_free_page_index(void)
{
    for (size_t page_index = 0; page_index < ARRAY_SIZE(g_ptebp_pages); page_index++)
        if (!g_ptebp_pages[page_index].used) return page_index;
    return -ENOSPC;
}

/*
在内存映射写锁和 PTEBP 自旋锁均已持有时，
遍历所有已经启用的 PTEBP 源页，清除这些源页的 PTE_UXN，恢复用户态执行权限，
并将成功恢复的页面标记为 armed = false。
*/
static void ptebp_restore_pages_locked(void)
{
    if (!g_ptebp_mm) return;

    for (size_t page_index = 0; page_index < ARRAY_SIZE(g_ptebp_pages); page_index++)
    {
        struct ptebp_page *page = &g_ptebp_pages[page_index];

        if (!page->used || !page->armed) continue;
        if (ptebp_page_matches(page, g_ptebp_mm, PTE_UXN))
        {
            pte_t *ptep = get_user_pte(g_ptebp_mm, page->source_page);
            if (ptep && !write_user_pte_value(g_ptebp_mm, page->source_page, pte_val(READ_ONCE(*ptep)) & ~PTE_UXN)) page->armed = false;
        }
        else if (ptebp_page_matches(page, g_ptebp_mm, 0)) page->armed = false;
    }
}

// 持有指定进程上下文的锁和持有运行状态PTEBP自旋锁时，进行恢复该进程所有受监控源页的执行权限
static void ptebp_restore_pages(struct mm_struct *mm)
{
    if (!mm) return;

    mmap_write_lock(mm);
    {
        unsigned long flags;
        spin_lock_irqsave(&g_ptebp_lock, flags);
        if (g_ptebp_mm == mm) ptebp_restore_pages_locked();
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
    }
    mmap_write_unlock(mm);
}

// ptebp_handle_brk的异常处理失败后的紧急停用函数，只恢复所有受监控源页执行权限
static void ptebp_disable_from_handler(void)
{
    unsigned long flags;

    spin_lock_irqsave(&g_ptebp_lock, flags);
    g_ptebp_stopping = true;
    ptebp_restore_pages_locked();
    spin_unlock_irqrestore(&g_ptebp_lock, flags);
}

/*
hook工作函数，使用HOOK_ENTRY("do_mem_abort", ptebp_handle_exec_fault)挂接
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

确认该异常属于当前 PTEBP，然后把用户线程的 PC 从源页同位置切换到对应幽灵页继续执行。
*/

static int ptebp_handle_exec_fault(struct pt_regs *hook_regs)
{
    //增加计数
    ptebp_handler_enter();
    if (!hook_regs) goto out_leave;

    // do_mem_abort(far, esr, regs) 的第三个参数才是发生异常的用户线程现场。
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[2];

    /*
    只接管来自 EL0 的指令访问异常，并且要求 FSC 明确表示 L3 PTE 权限异常。
    其它异常可能是翻译失败、访问标志异常或内核态异常，必须继续交给原 do_mem_abort() 处理。
    */
    if (((hook_regs->regs[1] >> 26) & 0x3f) != PTEBP_ESR_EC_IABT_LOW || (hook_regs->regs[1] & PTEBP_ESR_FSC_MASK) != PTEBP_ESR_FSC_PERM_L3) goto out_leave;

    //PTEBP 只管理有用户 mm 的 EL0 执行现场，不能接管内核线程或 EL1 取指异常。
    if (!regs || !current->mm || !user_mode(regs)) goto out_leave;

    // 移除MTE标签 清除低2位标记 ，得到4字节对齐、纯净的指令虚拟地址
    uint64_t source_pc = untagged_addr(regs->pc) & ~0x3ULL;

    // FAR 与异常 PC 必须落在同一个 4 KiB 页内，否则这次权限异常不属于当前 PC 所在源页。
    if ((untagged_addr(hook_regs->regs[0]) & PAGE_MASK) != (source_pc & PAGE_MASK)) goto out_leave;

    /*
    自旋锁保护当前监控 mm、停止状态、页面槽位和 armed 状态。
    锁内只完成归属判断、PTE 一致性检查和地址换算；用户寄存器现场在解锁后再改写。
    */
    uint64_t ghost_pc = 0;
    bool managed = false;
    {
        unsigned long flags;
        spin_lock_irqsave(&g_ptebp_lock, flags);
        {
            //同一个虚拟地址可存在于不同进程，只允许接管安装 PTEBP 时记录的那个 mm
            if (g_ptebp_mm != current->mm) goto out_unlock;

            // 通过规范化源 PC 的页基址，找到安装阶段为该源页建立的快照和重定位关系
            struct ptebp_page *page = ptebp_find_source_page(source_pc);
            if (!page) goto out_unlock;

            if (!g_ptebp_stopping && page->armed)
            {
                /*
                正常监控路径要求源页仍然等于安装时的 PTE，并且只多出本模块设置的 PTE_UXN。
                若 PFN 或其它权限已经被外部修改，立即放弃接管，避免跳入与当前源页不匹配的幽灵代码。
                */
                if (!ptebp_page_matches(page, current->mm, PTE_UXN)) goto out_unlock;

                /*
                严格一对一重定位保证源页内偏移不变；这里把故障源 PC 换算为幽灵区域中的对应 PC。
                返回 0 表示该地址不在有效重定位范围内，此时不能声称异常已由 PTEBP 管理。
                */
                ghost_pc = arm64_page_reloc_to_ghost(&page->relocation, source_pc);
                managed = ghost_pc != 0;
            }
            else if (g_ptebp_stopping)
            {
                /*
                停止期间不再把新的执行流送入幽灵页，但仍需消费已经到达 hook 的本模块异常：
                page->armed 为 true 时，源页可能尚未清除 UXN；为 false 时，恢复流程已经清除了 UXN。
                两种状态都必须与保存的原始 PTE 一致，确认没有接管外部产生的同类异常。
                此分支故意不设置 ghost_pc，返回用户态时保留源 PC，让指令在执行权限恢复后原地重试。
                */
                if ((page->armed && ptebp_page_matches(page, current->mm, PTE_UXN)) || (!page->armed && ptebp_page_matches(page, current->mm, 0))) managed = true;
            }
        }

    out_unlock:
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
    }

    // managed 为 false 时返回 0，inline hook 框架会继续执行原 do_mem_abort()。
    if (!managed) goto out_leave;

    // 正常监控时切换到幽灵 PC；停止排空时 ghost_pc 为 0，用户 PC 保持在源指令
    if (ghost_pc) regs->pc = ghost_pc;

    //给目标函数的调用者返回0标识异常已经处理
    hook_regs->regs[0] = 0;

    //减少计数
    ptebp_handler_leave();

    //给inline hook框架返回 1 表示不需要继续执行原函数，从而跳过原 do_mem_abort()，避免内核继续按普通权限异常向用户进程发送故障信号。
    return 1;

out_leave:
    //与入口计数严格配对；最后一个处理函数退出时会唤醒正在等待排空的停止线程。
    ptebp_handler_leave();
    return 0;
}

// 接管幽灵页 BRK，调用监控回调并模拟原指令步过。
static int ptebp_handle_brk(struct pt_regs *hook_regs)
{
    //增加计数
    ptebp_handler_enter();
    if (!hook_regs) goto out_leave;

    /*
    brk_handler() 的第三个参数是触发 BRK 的用户寄存器现场；hook_regs是保存的 brk_handler() 调用现场，
    其中 x1 保存本次 BRK 的 ESR。
    */
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[2];

    //PTEBP 只接管拥有用户地址空间、并且确实来自 EL0 的 BRK 异常。
    if (!regs || !current->mm || !user_mode(regs)) goto out_leave;

    // 移除MTE标签 清除低2位标记 ，得到4字节对齐、纯净的指令虚拟地址
    uint64_t ghost_pc = untagged_addr(regs->pc) & ~0x3ULL;

    /*
    这些值都在锁内从当前 PTEBP 状态中解析出来，解锁后只使用局部副本：
    page       表示 ghost_pc 所属的源页/幽灵页运行状态；
    hit_point  表示该 BRK 对应的外部监控点，停止期间保持为 NULL，不再调用回调；
    source_pc  是 ghost_pc 映射回去的真实源指令地址；
    orig_insn  是该源地址在安装 PTEBP 前的原始机器码；
    managed    表示该 BRK 已确认属于当前 PTEBP，需要由本函数继续处理。
    */
    struct ptebp_page *page;
    struct bp_point *hit_point = NULL;
    uint64_t source_pc = 0;
    uint32_t orig_insn = 0;
    bool managed = false;
    {
        unsigned long flags;
        spin_lock_irqsave(&g_ptebp_lock, flags);

        /*
        相同虚拟地址可能存在于不同进程，因此先要求当前 mm 与安装 PTEBP 时保存的 mm 一致，
        再根据 ghost_pc 查找幽灵区域。额外限制在首个 PAGE_SIZE 内，是为了排除重定位器
        在页尾附加的跳回源页分支；只有与源页逐指令对应的幽灵页主体才可能产生受管 BRK。
        */
        page = g_ptebp_mm == current->mm ? ptebp_find_ghost_page(ghost_pc) : NULL;
        if (page && ghost_pc < page->ghost.user_va + PAGE_SIZE)
        {
            /*
            严格一对一重定位保证幽灵页和源页的页内偏移相同。先恢复源 PC，再用源页
            快照取得该位置原本的机器码；这样即使幽灵指令已被替换成 BRK，也能模拟原指令。
            */
            source_pc = arm64_page_reloc_to_source(&page->relocation, ghost_pc);
            uint32_t source_index = (source_pc - page->source_page) / sizeof(uint32_t);
            orig_insn = page->source_code[source_index];

            /*
            查找安装阶段明确替换为 BRK #0 的监控点。命中后以 point_state 中保存的地址和
            原指令为准；只有尚未进入停止流程时才保存 hit_point，解锁后据此决定是否回调。
            停止流程会等待当前在途处理函数退出，因此保存到局部变量的指针在本函数内仍有效。
            */
            for (size_t point_index = 0; point_index < ARRAY_SIZE(g_ptebp_points); point_index++)
            {
                struct ptebp_point *point_state = &g_ptebp_points[point_index];

                if (!point_state->used || point_state->ghost_pc != ghost_pc) continue;
                source_pc = point_state->source_pc;
                orig_insn = point_state->orig_insn;
                if (!g_ptebp_stopping && g_ptebp_info) hit_point = &g_ptebp_info->points[point_index];
                managed = true;
                break;
            }

            /*
            幽灵页中也可能执行到源代码本来就包含的 BRK。它不是配置的监控点，但仍属于
            当前幽灵页执行流，必须先映射回源 PC，随后再交给原 brk_handler() 保持原始语义。
            */
            if (!managed && ptebp_is_brk(orig_insn)) managed = true;
        }
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
    }

    //不属于当前 PTEBP 的 BRK 返回 0，让 inline hook 框架继续执行原 brk_handler()。
    if (!managed) goto out_leave;

    /*
    先把用户现场 PC 恢复成真实源地址，使回调看到用户配置的断点地址，并为原指令模拟
    建立正确的架构 PC。这里不是让 CPU 返回源页原生执行：当前幽灵地址上的原指令已经
    被 BRK 覆盖，直接返回 ghost_pc 只会再次触发同一 BRK。B/BL/B.cond、ADR/ADRP 和
    LDR literal 等指令依赖原指令地址，BL 还必须生成 source_pc + 4 的返回地址，因此不能
    以 ghost_pc 作为模拟基准。模拟器读取单独保存的 orig_insn，在内核中更新寄存器和最终
    regs->pc；普通顺序指令通常得到 source_pc + 4，返回用户态后再由源页 UXN 执行异常
    映射到下一条幽灵指令。回调若主动修改 regs->pc，就表示它接管了后续控制流。
    */
    regs->pc = source_pc;
    if (hit_point && hit_point->on_hit) hit_point->on_hit((void *)regs, (void *)hit_point);

    bool pass_to_original = false;

    // 只有回调没有修改源 PC 时，才执行或传递被 BRK 替换位置原本的那条指令。
    if ((untagged_addr(regs->pc) & ~0x3ULL) == source_pc)
    {
        if (ptebp_is_brk(orig_insn))
        {
            /*
            源代码原本就是 BRK 时不能把它当作监控 BRK 消费掉。将原指令 imm16 写回
            ESR.ISS[15:0]，并让本函数返回 0，使原 brk_handler() 按真实 BRK 继续处理。
            */
            hook_regs->regs[1] = (hook_regs->regs[1] & ~0xFFFFULL) | ((orig_insn >> 5) & 0xFFFFU);
            pass_to_original = true;
        }
        else
        {
            /*
            配置监控点的幽灵指令是人工插入的 BRK，需要在源 PC 语义下模拟原始指令完成步过。
            模拟器可能读写 SIMD/FP 寄存器，因此先快照 Q0-Q31，模拟后再将完整现场写回硬件。
            若原指令无法模拟，则进入紧急停止模式并恢复源页执行权限，避免线程反复命中同一 BRK。
            */
            struct fp_regs fp_regs;
            for (int qreg = 0; qreg < ARM64_FP_Q_REG_COUNT; qreg++) read_q_reg(qreg, &fp_regs.q[qreg]);
            bool emulated = emulate_insn(regs, &fp_regs, &orig_insn);
            for (int qreg = 0; qreg < ARM64_FP_Q_REG_COUNT; qreg++) write_q_reg(qreg, &fp_regs.q[qreg]);
            if (!emulated && page) ptebp_disable_from_handler();
        }
    }

    /*
    人工插入的监控 BRK 已被回调和原指令模拟完整消费；回调修改 PC 时也视为已接管。
    清零 brk_handler() 的返回值槽位，并向 inline hook 框架返回 1，跳过原 brk_handler()。
    */
    if (!pass_to_original)
    {
        hook_regs->regs[0] = 0;
        ptebp_handler_leave();
        return 1;
    }

out_leave:
    // pass_to_original 和所有未接管路径都返回 0，原 brk_handler() 将继续执行。
    ptebp_handler_leave();
    return 0;
}

static struct hook_entry g_ptebp_hooks[] = {
    HOOK_ENTRY("do_mem_abort", ptebp_handle_exec_fault),
    HOOK_ENTRY("brk_handler", ptebp_handle_brk),
};

// 销毁尚未启用的幽灵页和源页快照，并清空运行状态数组。
static void ptebp_release_resources(void)
{
    for (size_t page_index = 0; page_index < ARRAY_SIZE(g_ptebp_pages); page_index++)
    {
        struct ptebp_page *page = &g_ptebp_pages[page_index];
        if (page->used) arm64_ghost_region_destroy(&page->ghost);
        vfree(page->source_code);
    }

    __builtin_memset(g_ptebp_pages, 0, sizeof(g_ptebp_pages));
    __builtin_memset(g_ptebp_points, 0, sizeof(g_ptebp_points));
}

// 丢弃停止后的管理状态；保留幽灵物理页和映射供在途线程自然执行完毕。
static void ptebp_forget_resources(void)
{
    for (size_t page_index = 0; page_index < ARRAY_SIZE(g_ptebp_pages); page_index++)
    {
        struct ptebp_page *page = &g_ptebp_pages[page_index];
        if (!page->used) continue;
        if (page->ghost.mm) mmput(page->ghost.mm);
        kvfree(page->ghost.pages);
        vfree(page->source_code);
        page->ghost.mm = NULL;
        page->ghost.pages = NULL;
    }

    __builtin_memset(g_ptebp_pages, 0, sizeof(g_ptebp_pages));
    __builtin_memset(g_ptebp_points, 0, sizeof(g_ptebp_points));
}

// 在 PTEBP 互斥锁已持有时停止监控，只恢复源页执行权限。权限恢复后自然跑完就会回到原页，不在进入幽灵页
static void ptebp_stop_locked(struct break_point *preserve_info)
{
    struct break_point *info;
    struct mm_struct *mm;
    {
        unsigned long flags;
        spin_lock_irqsave(&g_ptebp_lock, flags);
        info = g_ptebp_info;
        mm = g_ptebp_mm;
        g_ptebp_stopping = true;
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
    }

    ptebp_restore_pages(mm);

    for (;;)
    {
        wait_event(g_ptebp_handler_wait, atomic_read(&g_ptebp_handlers_inflight) == 0);

        unsigned long flags;
        spin_lock_irqsave(&g_ptebp_lock, flags);
        if (atomic_read(&g_ptebp_handlers_inflight) == 0)
        {
            g_ptebp_info = NULL;
            g_ptebp_mm = NULL;
            spin_unlock_irqrestore(&g_ptebp_lock, flags);
            break;
        }
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
    }

    ptebp_forget_resources();
    if (mm) mmput(mm);
    if (info && info != preserve_info) __builtin_memset(info, 0, sizeof(*info));

    {
        unsigned long flags;
        spin_lock_irqsave(&g_ptebp_lock, flags);
        g_ptebp_stopping = false;
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
    }
}

// 校验源页可执行属性和页表项，并复制整页机器码及原始页表项。
static int ptebp_snapshot_source_page(struct mm_struct *mm, struct ptebp_page *page, void *source_code)
{
    struct vm_area_struct *vma = find_vma(mm, page->source_page);
    if (!vma || page->source_page < vma->vm_start || page->source_page + PAGE_SIZE > vma->vm_end) return -EFAULT;
    if (!(vma->vm_flags & VM_EXEC))
    {
        ls_log_tag("ptebp", "reject source page=0x%llx vma=[0x%lx,0x%lx) flags=0x%lx exec=%d write=%d\n", (unsigned long long)page->source_page, vma->vm_start, vma->vm_end, vma->vm_flags, !!(vma->vm_flags & VM_EXEC), !!(vma->vm_flags & VM_WRITE));
        return -EACCES;
    }

    pte_t *ptep = get_user_pte(mm, page->source_page);
    if (!ptep) return -EFAULT;

    pte_t orig_pte = READ_ONCE(*ptep);
    if (!pte_present(orig_pte) || !pfn_valid(pte_pfn(orig_pte))) return -EFAULT;
    if (pte_val(orig_pte) & PTE_UXN)
    {
        ls_log_tag("ptebp", "reject source page=0x%llx pte=0x%llx: PTE_UXN is already set\n", (unsigned long long)page->source_page, (unsigned long long)pte_val(orig_pte));
        return -EACCES;
    }

    struct page *source_page = pfn_to_page(pte_pfn(orig_pte));
    void *mapping = page_address(source_page);
    if (!mapping) return -EFAULT;

    __builtin_memcpy(source_code, mapping, PAGE_SIZE);
    page->orig_pte = orig_pte;
    return 0;
}

// 比较源页当前机器码与准备阶段保存的整页快照。
static bool ptebp_source_page_matches(const struct ptebp_page *page, struct mm_struct *mm)
{
    if (!page || !page->source_code || !mm) return false;

    pte_t *ptep = get_user_pte(mm, page->source_page);
    if (!ptep) return false;

    pte_t pte = READ_ONCE(*ptep);
    if (!pte_present(pte) || !pfn_valid(pte_pfn(pte))) return false;

    void *mapping = page_address(pfn_to_page(pte_pfn(pte)));
    return mapping && !__builtin_memcmp(page->source_code, mapping, PAGE_SIZE);
}

// 按地址顺序输出完整机器码映射，每行 32 字节。
static void ptebp_log_machine_code(const char *name, uint64_t address, const uint32_t *code, size_t size)
{
    ls_log_tag("ptebp", "%s machine code begin address=0x%llx size=%zu\n", name, (unsigned long long)address, size);
    for (size_t word_index = 0; word_index < size / sizeof(*code); word_index += 8)
    {
        ls_log_tag("ptebp", "0x%llx: %08x %08x %08x %08x %08x %08x %08x %08x\n", (unsigned long long)(address + word_index * sizeof(*code)), code[word_index], code[word_index + 1], code[word_index + 2], code[word_index + 3], code[word_index + 4], code[word_index + 5], code[word_index + 6], code[word_index + 7]);
    }
    ls_log_tag("ptebp", "%s machine code end address=0x%llx\n", name, (unsigned long long)address);
}

// 为一个源页建立快照、幽灵映射和重定位代码，并替换该页内的监控点。
static int ptebp_prepare_page(struct break_point *info, struct mm_struct *mm, uint64_t source_page, int page_index)
{
    struct ptebp_page *page = &g_ptebp_pages[page_index];
    uint32_t *source_code = vmalloc(PAGE_SIZE);
    if (!source_code) return -ENOMEM;

    uint32_t *ghost_code = kvzalloc(PAGE_ALIGN(ARM64_PAGE_RELOC_MAX_BYTES), GFP_KERNEL);
    if (!ghost_code)
    {
        vfree(source_code);
        return -ENOMEM;
    }

    page->used = true;
    page->source_page = source_page;

    mmap_read_lock(mm);
    int status = ptebp_snapshot_source_page(mm, page, source_code);
    mmap_read_unlock(mm);
    if (status) goto out;

    size_t ghost_size = ARM64_PAGE_RELOC_BASE_BYTES;
    for (;;)
    {
        status = arm64_ghost_region_reserve(info->tgid, ghost_size, source_page, &page->ghost);
        if (status) goto out;
        if (page->ghost.mm != mm)
        {
            status = -ESTALE;
            goto out;
        }

        status = arm64_page_relocate(source_page, page->ghost.user_va, source_code, ghost_code, ARM64_PAGE_RELOC_MAX_BYTES, &page->relocation);
        if (status) goto out;
        if (page->relocation.code_size <= page->ghost.mapped_size) break;

        ghost_size = page->relocation.code_size;
        arm64_ghost_region_destroy(&page->ghost);
    }

    for (size_t point_index = 0; point_index < ARRAY_SIZE(info->points); point_index++)
    {
        struct bp_point *point = &info->points[point_index];
        if (!ptebp_active(point)) continue;

        uint64_t source_pc = untagged_addr(READ_ONCE(point->hit_addr)) & ~0x3ULL;
        if ((source_pc & PAGE_MASK) != source_page) continue;

        uint32_t source_index = (source_pc - source_page) / sizeof(uint32_t);
        ghost_code[source_index] = PTEBP_MONITOR_BRK;
        g_ptebp_points[point_index] = (struct ptebp_point){
            .source_pc = source_pc,
            .ghost_pc = page->ghost.user_va + (source_pc - source_page),
            .orig_insn = source_code[source_index],
            .used = true,
        };
    }

    ls_log_tag("ptebp", "machine code dump source=0x%llx ghost=0x%llx code_size=%zu mapped_size=%zu slots=%u\n", (unsigned long long)source_page, (unsigned long long)page->ghost.user_va, page->relocation.code_size, page->ghost.mapped_size, page->relocation.slot_count);
    ptebp_log_machine_code("source", source_page, source_code, PAGE_SIZE);
    ptebp_log_machine_code("ghost", page->ghost.user_va, ghost_code, page->ghost.mapped_size);

    status = arm64_ghost_region_write(&page->ghost, ghost_code, page->relocation.code_size);
    if (!status)
    {
        page->source_code = source_code;
        source_code = NULL;
    }

out:
    kvfree(ghost_code);
    vfree(source_code);
    if (status)
    {
        arm64_ghost_region_destroy(&page->ghost);
        __builtin_memset(page, 0, sizeof(*page));
    }
    return status;
}

// 校验监控点地址、类型和唯一性，并要求至少存在一个有效执行断点。
static int ptebp_validate_points(struct break_point *info, struct mm_struct *mm)
{
    bool active = false;

    for (size_t point_index = 0; point_index < ARRAY_SIZE(info->points); point_index++)
    {
        struct bp_point *point = &info->points[point_index];
        if (!ptebp_active(point)) continue;

        active = true;
        uint64_t source_pc = untagged_addr(READ_ONCE(point->hit_addr)) & ~0x3ULL;
        uint64_t task_size = READ_ONCE(mm->task_size);
        if (!source_pc || source_pc >= task_size || sizeof(uint32_t) > task_size - source_pc) return -EFAULT;

        for (size_t previous = 0; previous < point_index; previous++)
        {
            struct bp_point *candidate = &info->points[previous];
            if (ptebp_active(candidate) && (untagged_addr(READ_ONCE(candidate->hit_addr)) & ~0x3ULL) == source_pc) return -EEXIST;
        }
    }

    return active ? 0 : -EINVAL;
}

// 按监控点涉及的不同源页逐页准备幽灵页执行环境。
static int ptebp_prepare_pages(struct break_point *info, struct mm_struct *mm)
{
    for (size_t point_index = 0; point_index < ARRAY_SIZE(info->points); point_index++)
    {
        struct bp_point *point = &info->points[point_index];
        if (!ptebp_active(point)) continue;

        uint64_t source_page = untagged_addr(READ_ONCE(point->hit_addr)) & PAGE_MASK;
        if (ptebp_find_source_page(source_page)) continue;

        int page_index = ptebp_find_free_page_index();
        if (page_index < 0) return page_index;

        int status = ptebp_prepare_page(info, mm, source_page, page_index);
        if (status) return status;
    }

    return 0;
}

// 在最终一致性检查通过后给所有源页设置 UXN，正式启用 PTEBP。
static int ptebp_arm_pages(struct mm_struct *mm)
{
    mmap_write_lock(mm);
    int status = 0;
    {
        unsigned long flags;
        spin_lock_irqsave(&g_ptebp_lock, flags);
        for (size_t page_index = 0; page_index < ARRAY_SIZE(g_ptebp_pages); page_index++)
        {
            struct ptebp_page *page = &g_ptebp_pages[page_index];
            if (!page->used) continue;

            if (!ptebp_page_matches(page, mm, 0))
            {
                status = -ESTALE;
                break;
            }
            if (!ptebp_source_page_matches(page, mm))
            {
                status = -ESTALE;
                break;
            }

            pte_t *ptep = get_user_pte(mm, page->source_page);
            if (!ptep)
            {
                status = -EFAULT;
                break;
            }

            page->orig_pte = READ_ONCE(*ptep);
            page->armed = true;
            status = write_user_pte_value(mm, page->source_page, pte_val(page->orig_pte) | PTE_UXN);
            if (status)
            {
                page->armed = false;
                break;
            }
        }
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
    }
    mmap_write_unlock(mm);
    return status;
}

/*
以下函数是提供给外部模块调用的 PTE 执行断点监控接口======================================================================
*/

// 停止旧实例并为断点配置启动新的 PTEBP 幽灵页监控。
static inline int start_ptebp_monitor(struct break_point *info)
{
    if (!info || READ_ONCE(info->tgid) <= 0) return -EINVAL;

    mutex_lock(&g_ptebp_mutex);
    ptebp_stop_locked(info);

    struct mm_struct *mm = get_mm_by_pid(info->tgid);
    int status;
    if (!mm)
    {
        status = -ESRCH;
        goto out_unlock;
    }

    status = ptebp_validate_points(info, mm);
    if (status) goto err_put_mm;

    status = ptebp_prepare_pages(info, mm);
    if (status) goto err_release;

    status = inline_hook_install(g_ptebp_hooks);
    if (status) goto err_release;

    {
        unsigned long flags;
        spin_lock_irqsave(&g_ptebp_lock, flags);
        g_ptebp_info = info;
        g_ptebp_mm = mm;
        g_ptebp_stopping = false;
        spin_unlock_irqrestore(&g_ptebp_lock, flags);
    }

    status = ptebp_arm_pages(mm);
    if (status)
    {
        ptebp_stop_locked(info);
        goto out_unlock;
    }

    ls_log_tag("ptebp", "start ok tgid=%d mm=0x%llx\n", info->tgid, (unsigned long long)mm);
    mutex_unlock(&g_ptebp_mutex);
    return 0;

err_release:
    ptebp_release_resources();
err_put_mm:
    mmput(mm);
out_unlock:
    mutex_unlock(&g_ptebp_mutex);
    return status;
}

// 串行停止当前 PTEBP 监控。
static inline void stop_ptebp_monitor(void)
{
    mutex_lock(&g_ptebp_mutex);
    ptebp_stop_locked(NULL);
    mutex_unlock(&g_ptebp_mutex);
}

#endif