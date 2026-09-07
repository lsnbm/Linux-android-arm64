/*
监控指定进程中使用 CNTVCT_EL0 测量代码执行时间的位置。

监控启用后关闭目标进程的 EL0 CNTVCT_EL0 直接访问权限，使 MRS CNTVCT_EL0
进入内核异常处理。每次命中时，以异常 PC 为中心一次复制上下各 10 条 A64
指令到本地缓冲区，后续识别不再读取用户内存。

只识别下面的计时代码特征：
    ISB
    MRS ..., CNTVCT_EL0    // start_pc
    ...                    // 两条 MRS 的 PC 距离严格小于 10 条指令
    ISB
    MRS ..., CNTVCT_EL0    // end_pc

不符合特征时直接返回原异常处理流程；符合时把两条 MRS 的 PC 作为
{start_pc, end_pc} 写入固定全局表。写入前按起止地址去重，重复范围不保存，
表满后静默忽略。安装新监控目标时只重置有效表项数量。
*/
#ifndef ARM64_CNTVCT_MONITOR_H
#define ARM64_CNTVCT_MONITOR_H

#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/sched/signal.h>
#include <linux/spinlock.h>
#include <clocksource/arm_arch_timer.h>
#include <asm/esr.h>
#include <asm/ptrace.h>
#include <asm/sysreg.h>

#include "export_fun.h"
#include "inline_hook_frame.h"
#include "lsdriver_log.h"

#define CNTVCT_MONITOR_MAX_RANGES                   128                                               //表容量
#define CNTVCT_MONITOR_MAX_PC_DISTANCE_INSTRUCTIONS 10                                                //两条 MRS 的最大指令数距离
#define CNTVCT_MONITOR_SCAN_RADIUS_INSTRUCTIONS     10                                                //命中 PC时的两侧扫描半径
#define CNTVCT_MONITOR_SCAN_INSTRUCTIONS            (CNTVCT_MONITOR_SCAN_RADIUS_INSTRUCTIONS * 2 + 1) //本地扫描缓冲区大小
#define CNTVCT_MONITOR_ISB_INST                     0xD5033FDFU                                       //ISB 指令机器码
#define CNTVCT_MONITOR_SYSREG                       ARM64_SYSREG_KEY(3, 3, 14, 0, 2)                  //CNTVCT_EL0 系统寄存器编号

struct cntvct_monitor_range
{
    uint64_t start_pc;
    uint64_t end_pc;
    uint64_t execution_count;
};

static DEFINE_MUTEX(g_cntvct_monitor_mutex);
static DEFINE_SPINLOCK(g_cntvct_monitor_table_lock);
static struct cntvct_monitor_range g_cntvct_monitor_ranges[CNTVCT_MONITOR_MAX_RANGES];
static unsigned int g_cntvct_monitor_range_count;
static pid_t g_cntvct_monitor_tgid;
// 下一次允许输出全局范围表的 jiffies，多 CPU 共享此限频时间
static unsigned long g_cntvct_monitor_next_dump_jiffies;

// 解码并判断给定指令是否为 MRS CNTVCT_EL0
static bool cntvct_monitor_is_mrs_cntvct(uint32_t inst)
{
    struct arm64_decoded_instruction decoded;
    return arm64_decode_instruction(inst, &decoded) == ARM64_DECODE_OK && decoded.instruction == ARM64_INST_MRS && decoded.sysreg == CNTVCT_MONITOR_SYSREG;
}

// 记录一次完整执行，重复范围只增加执行次数
static void cntvct_monitor_record_range(uint64_t start_pc, uint64_t end_pc)
{
    unsigned long flags;

    spin_lock_irqsave(&g_cntvct_monitor_table_lock, flags);
    for (unsigned int i = 0; i < g_cntvct_monitor_range_count; i++)
    {
        if (g_cntvct_monitor_ranges[i].start_pc == start_pc && g_cntvct_monitor_ranges[i].end_pc == end_pc)
        {
            g_cntvct_monitor_ranges[i].execution_count++;
            goto out_unlock;
        }
    }

    if (g_cntvct_monitor_range_count < ARRAY_SIZE(g_cntvct_monitor_ranges))
    {
        g_cntvct_monitor_ranges[g_cntvct_monitor_range_count].start_pc = start_pc;
        g_cntvct_monitor_ranges[g_cntvct_monitor_range_count].end_pc = end_pc;
        g_cntvct_monitor_ranges[g_cntvct_monitor_range_count].execution_count = 1;
        g_cntvct_monitor_range_count++;
    }

out_unlock:
    spin_unlock_irqrestore(&g_cntvct_monitor_table_lock, flags);
}

// 在任务切换到目标 TGID 时关闭当前 CPU 的 CNTVCT_EL0 访问权限，并每秒输出一次范围表
// hook cpu_switch_to(prev, next) 时 x1 仍然是 next，且内核对 next 的 CNTKCTL_EL1
// 更新已经完成。切换到其他任务时不修改权限，由内核原有调度逻辑负责设置。
static int cntvct_monitor_switch_hook_work(struct pt_regs *hook_regs)
{
    if (!hook_regs) return 0;

    struct task_struct *next = (struct task_struct *)(uintptr_t)hook_regs->regs[1];
    pid_t target_tgid = READ_ONCE(g_cntvct_monitor_tgid);
    if (target_tgid > 0 && next && next->tgid == target_tgid)
    {
        unsigned long cntkctl = read_sysreg(cntkctl_el1);
        if (cntkctl & ARCH_TIMER_USR_VCT_ACCESS_EN)
        {
            write_sysreg(cntkctl & ~ARCH_TIMER_USR_VCT_ACCESS_EN, cntkctl_el1);
            isb();
        }

        // 每秒只允许一个 CPU 抢到本轮输出权，避免多个 CPU 重复打印同一张表
        unsigned long now = jiffies;
        unsigned long next_dump = READ_ONCE(g_cntvct_monitor_next_dump_jiffies);
        if (time_after_eq(now, next_dump) && cmpxchg(&g_cntvct_monitor_next_dump_jiffies, next_dump, now + HZ) == next_dump)
        {
            unsigned int range_count;
            unsigned long flags;

            // 在同一个锁内读取并输出全局范围表，避免创建栈快照和重复加锁
            spin_lock_irqsave(&g_cntvct_monitor_table_lock, flags);
            range_count = g_cntvct_monitor_range_count;

            // 先输出表头，再逐项输出运行时 PC 范围和完整执行次数
            ls_log_always_tag("cntvct", "ranges tgid=%d count=%u\n", target_tgid, range_count);
            for (unsigned int i = 0; i < range_count; i++)
            {
                ls_log_always_tag("cntvct", "range[%u] start_pc=0x%llx end_pc=0x%llx execution_count=%llu\n", i, (unsigned long long)g_cntvct_monitor_ranges[i].start_pc, (unsigned long long)g_cntvct_monitor_ranges[i].end_pc, (unsigned long long)g_cntvct_monitor_ranges[i].execution_count);
            }
            spin_unlock_irqrestore(&g_cntvct_monitor_table_lock, flags);
        }
    }
    return 0;
}

// 一次复制命中 PC 上下各十条指令，只在本地缓冲区识别成对的 ISB + MRS CNTVCT_EL0。
static int cntvct_monitor_read_hook_work(struct pt_regs *hook_regs)
{
    if (!hook_regs) return 0;

    pid_t target_tgid = READ_ONCE(g_cntvct_monitor_tgid);
    if (target_tgid <= 0 || current->tgid != target_tgid) return 0;

    unsigned long esr = hook_regs->regs[0];
    if ((esr & ESR_ELx_SYS64_ISS_SYS_OP_MASK) != ESR_ELx_SYS64_ISS_SYS_CNTVCT) return 0;

    struct pt_regs *regs = (struct pt_regs *)(uintptr_t)hook_regs->regs[1];
    if (!regs || !user_mode(regs)) return 0;

    uint64_t hit_pc = regs->pc;
    if ((hit_pc & 0x3) || hit_pc < CNTVCT_MONITOR_SCAN_RADIUS_INSTRUCTIONS * sizeof(uint32_t)) return 0;

    uint64_t scan_start_pc = hit_pc - CNTVCT_MONITOR_SCAN_RADIUS_INSTRUCTIONS * sizeof(uint32_t);
    uint32_t insts[CNTVCT_MONITOR_SCAN_INSTRUCTIONS];
    if (copy_from_user_inatomic_nofault(insts, (const void __user *)(uintptr_t)scan_start_pc, sizeof(insts))) return 0;
    if (insts[CNTVCT_MONITOR_SCAN_RADIUS_INSTRUCTIONS - 1] != CNTVCT_MONITOR_ISB_INST || !cntvct_monitor_is_mrs_cntvct(insts[CNTVCT_MONITOR_SCAN_RADIUS_INSTRUCTIONS])) return 0;

    for (unsigned int distance = 2; distance < CNTVCT_MONITOR_MAX_PC_DISTANCE_INSTRUCTIONS; distance++)
    {
        unsigned int start_mrs = CNTVCT_MONITOR_SCAN_RADIUS_INSTRUCTIONS - distance;
        if (insts[start_mrs - 1] != CNTVCT_MONITOR_ISB_INST || !cntvct_monitor_is_mrs_cntvct(insts[start_mrs])) continue;
        cntvct_monitor_record_range(scan_start_pc + start_mrs * sizeof(uint32_t), hit_pc);
        return 0;
    }
    return 0;
}

static struct hook_entry g_cntvct_monitor_hooks[] = {
    HOOK_ENTRY("cntvct_read_handler", cntvct_monitor_read_hook_work),
    HOOK_ENTRY("cpu_switch_to", cntvct_monitor_switch_hook_work),
};

// 安装两个内联钩子并开始监控指定线程组
static int cntvct_monitor_install(pid_t target_tgid)
{
    if (target_tgid <= 0) return -EINVAL;

    struct task_struct *task = get_task_by_pid(target_tgid);
    if (!task) return -ESRCH;

    bool valid = task->tgid == target_tgid;
    put_task_struct(task);
    if (!valid) return -ESRCH;

    mutex_lock(&g_cntvct_monitor_mutex);

    int status = inline_hook_install(g_cntvct_monitor_hooks);
    if (status < 0) goto out_unlock;

    unsigned long flags;
    spin_lock_irqsave(&g_cntvct_monitor_table_lock, flags);
    g_cntvct_monitor_range_count = 0;
    spin_unlock_irqrestore(&g_cntvct_monitor_table_lock, flags);
    WRITE_ONCE(g_cntvct_monitor_tgid, target_tgid);

    mutex_unlock(&g_cntvct_monitor_mutex);
    return 0;

out_unlock:
    mutex_unlock(&g_cntvct_monitor_mutex);
    return status;
}

// 正 TGID 匹配当前目标时停止监控，非正值表示无条件停止
static void cntvct_monitor_remove(pid_t tgid)
{
    mutex_lock(&g_cntvct_monitor_mutex);
    pid_t active_tgid = READ_ONCE(g_cntvct_monitor_tgid);
    if (tgid <= 0 || active_tgid <= 0 || tgid == active_tgid)
    {
        if (g_cntvct_monitor_hooks[0].installed || g_cntvct_monitor_hooks[1].installed || active_tgid > 0)
        {
            WRITE_ONCE(g_cntvct_monitor_tgid, 0);
            inline_hook_remove(g_cntvct_monitor_hooks);
        }
    }
    mutex_unlock(&g_cntvct_monitor_mutex);
}

#endif /* ARM64_CNTVCTDBG_H */