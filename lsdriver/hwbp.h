#ifndef HWBP_H
#define HWBP_H

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/uaccess.h>
#include <asm/ptrace.h>
#include <asm/insn.h>
#include "emulate_insn.h"

typedef struct perf_event *(*register_user_hw_breakpoint_t)(struct perf_event_attr *attr, perf_overflow_handler_t triggered, void *context, struct task_struct *tsk);
typedef void (*unregister_hw_breakpoint_t)(struct perf_event *bp);

static register_user_hw_breakpoint_t fn_register_user_hw_breakpoint = NULL;
static unregister_hw_breakpoint_t fn_unregister_hw_breakpoint = NULL;

// 链表节点，用于保存注册的 perf_event 指针，方便后续删除
struct bp_node
{
    struct perf_event *bp;
    struct list_head list;
};

// 全局链表头
static LIST_HEAD(bp_event_list);

// 保护全局断点事件链表的互斥锁（允许睡眠，用于注册/注销断点）
static DEFINE_MUTEX(bp_list_mutex);

// 保护 hwbp_info 数据记录的全局自旋锁（禁止睡眠，用于中断回调中保护数据）
static DEFINE_SPINLOCK(hwbp_record_lock);

// ==========================================

// 读取当前物理 CPU 的浮点/SIMD 寄存器 (Q0 - Q31)
static inline void get_user_fp_regs(__uint128_t *vregs, uint32_t *fpsr, uint32_t *fpcr)
{
    // 加入 .arch_extension 绕过内核对浮点汇编的限制
    asm volatile(
        ".arch_extension fp\n"
        ".arch_extension simd\n"
        "stp q0, q1, [%0, #0]\n"
        "stp q2, q3, [%0, #32]\n"
        "stp q4, q5, [%0, #64]\n"
        "stp q6, q7, [%0, #96]\n"
        "stp q8, q9, [%0, #128]\n"
        "stp q10, q11, [%0, #160]\n"
        "stp q12, q13, [%0, #192]\n"
        "stp q14, q15, [%0, #224]\n"
        "stp q16, q17, [%0, #256]\n"
        "stp q18, q19, [%0, #288]\n"
        "stp q20, q21, [%0, #320]\n"
        "stp q22, q23, [%0, #352]\n"
        "stp q24, q25, [%0, #384]\n"
        "stp q26, q27, [%0, #416]\n"
        "stp q28, q29, [%0, #448]\n"
        "stp q30, q31, [%0, #480]\n"
        : : "r"(vregs) : "memory");

    // AArch64 中 MRS/MSR 必须使用 64 位寄存器 (X 寄存器)
    // 使用 uint64_t 作为中转，避免编译器报错
    uint64_t tmp_fpsr, tmp_fpcr;

    asm volatile(
        ".arch_extension fp\n"
        "mrs %0, fpsr\n"
        : "=r"(tmp_fpsr));
    *fpsr = (uint32_t)tmp_fpsr;

    asm volatile(
        ".arch_extension fp\n"
        "mrs %0, fpcr\n"
        : "=r"(tmp_fpcr));
    *fpcr = (uint32_t)tmp_fpcr;
}

// 写入数据到当前物理 CPU 的浮点/SIMD 寄存器 (Q0 - Q31)
static inline void set_user_fp_regs(__uint128_t *vregs, uint32_t fpsr, uint32_t fpcr)
{
    // 加入 .arch_extension 绕过内核对浮点汇编的限制
    asm volatile(
        ".arch_extension fp\n"
        ".arch_extension simd\n"
        "ldp q0, q1, [%0, #0]\n"
        "ldp q2, q3, [%0, #32]\n"
        "ldp q4, q5, [%0, #64]\n"
        "ldp q6, q7, [%0, #96]\n"
        "ldp q8, q9, [%0, #128]\n"
        "ldp q10, q11, [%0, #160]\n"
        "ldp q12, q13, [%0, #192]\n"
        "ldp q14, q15, [%0, #224]\n"
        "ldp q16, q17, [%0, #256]\n"
        "ldp q18, q19, [%0, #288]\n"
        "ldp q20, q21, [%0, #320]\n"
        "ldp q22, q23, [%0, #352]\n"
        "ldp q24, q25, [%0, #384]\n"
        "ldp q26, q27, [%0, #416]\n"
        "ldp q28, q29, [%0, #448]\n"
        "ldp q30, q31, [%0, #480]\n"
        : : "r"(vregs) : "memory");

    // 使用 uint64_t 中转并强转
    uint64_t tmp_fpsr = fpsr;
    uint64_t tmp_fpcr = fpcr;

    asm volatile(
        ".arch_extension fp\n"
        "msr fpsr, %0\n"
        : : "r"(tmp_fpsr));

    asm volatile(
        ".arch_extension fp\n"
        "msr fpcr, %0\n"
        : : "r"(tmp_fpcr));
}

// 断点触发回调函数
static inline void sample_hbp_handler(struct perf_event *bp, struct perf_sample_data *data, struct pt_regs *regs)
{
    struct hwbp_info *info = (struct hwbp_info *)bp->overflow_handler_context;
    unsigned long flags;
    struct hwbp_record *rec = NULL;
    int i;

    if (!info)
        return;

    // 直接从 perf 属性中获取原始断点地址
    info->hit_addr = bp->attr.bp_addr;

    spin_lock_irqsave(&hwbp_record_lock, flags);

    // 唯一的一次查找：查找当前 PC 是否记录过
    for (i = 0; i < info->record_count; i++)
    {
        if (info->records[i].pc == regs->pc)
        {
            rec = &info->records[i];
            break;
        }
    }

    if (rec && rec->rw == 1)
    {
        // 写入模式：把结构体全部数据覆盖到寄存器

        // 写入通用寄存器
        memcpy(regs->regs, rec->regs, sizeof(u64) * 30); // X0 ~ X29
        regs->regs[30] = rec->lr;                        // X30 / LR
        regs->sp = rec->sp;                              // SP
        regs->pc = rec->pc;                              // PC
        regs->orig_x0 = rec->orig_x0;                    // orig_x0
        regs->syscallno = rec->syscallno;                // syscallno
        regs->pstate = rec->pstate;                      // pstate

        // 写入浮点寄存器
        set_user_fp_regs(rec->vregs, rec->fpsr, rec->fpcr);

        rec->hit_count++;
    }
    else
    {
        // 读取模式：不同PC,分配新槽位
        if (!rec && info->record_count < 0x100)
        {
            rec = &info->records[info->record_count];
            rec->pc = regs->pc;
            rec->rw = 0; // 新槽位默认设置为读
            rec->hit_count = 0;
            info->record_count++;
        }

        // 更新寄存器上下文
        if (rec)
        {
            // 通用寄存器
            rec->hit_count++;
            memcpy(rec->regs, regs->regs, sizeof(u64) * 30); // X0 ~ X29
            rec->lr = regs->regs[30];                        // X30 / LR
            rec->sp = regs->sp;
            rec->orig_x0 = regs->orig_x0;
            rec->syscallno = regs->syscallno;
            rec->pstate = regs->pstate;

            // 读取浮点寄存器
            get_user_fp_regs(rec->vregs, &rec->fpsr, &rec->fpcr);
        }
    }

    spin_unlock_irqrestore(&hwbp_record_lock, flags);

    // 统一在最后模拟执行指令，不管读写都能步过
    emulate_insn(regs);
}

// 设置进程断点
static inline int set_process_hwbp(pid_t pid, uint64_t addr, enum bp_type type, enum bp_scope scope, int len_bytes, struct hwbp_info *info)
{
    struct perf_event_attr attr;
    struct task_struct *task, *t;
    struct perf_event *bp;
    struct bp_node *node;
    int bp_type_kernel;
    int bp_len_kernel;

    if (!fn_register_user_hw_breakpoint || !fn_unregister_hw_breakpoint)
    {
        fn_register_user_hw_breakpoint = (register_user_hw_breakpoint_t)generic_kallsyms_lookup_name("register_user_hw_breakpoint");
        fn_unregister_hw_breakpoint = (unregister_hw_breakpoint_t)generic_kallsyms_lookup_name("unregister_hw_breakpoint");

        if (!fn_register_user_hw_breakpoint || !fn_unregister_hw_breakpoint)
        {
            pr_debug("无法找到硬件断点 API 的内存地址！\n");
            return -ENOSYS;
        }
    }

    if (!info)
        return -EINVAL;

    // 映射断点类型
    switch (type)
    {
    case BP_READ:
        bp_type_kernel = HW_BREAKPOINT_R;
        break;
    case BP_WRITE:
        bp_type_kernel = HW_BREAKPOINT_W;
        break;
    case BP_READ_WRITE:
        bp_type_kernel = HW_BREAKPOINT_R | HW_BREAKPOINT_W;
        break;
    case BP_EXECUTE:
        bp_type_kernel = HW_BREAKPOINT_X;
        break;
    default:
        return -EINVAL;
    }

    // 映射断点长度 (ARM64 硬件限制)
    if (type == BP_EXECUTE)
    {
        bp_len_kernel = HW_BREAKPOINT_LEN_8; // 执行断点必须是 8字节
    }
    else
    {
        switch (len_bytes)
        {
        case 1:
            bp_len_kernel = HW_BREAKPOINT_LEN_1;
            break;
        case 2:
            bp_len_kernel = HW_BREAKPOINT_LEN_2;
            break;
        case 4:
            bp_len_kernel = HW_BREAKPOINT_LEN_4;
            break;
        case 8:
            bp_len_kernel = HW_BREAKPOINT_LEN_8;
            break;
        default:
            return -EINVAL; // ARM64 通常只支持 1, 2, 4, 8 字节的 Watchpoint
        }
    }

    // 初始化属性
    hw_breakpoint_init(&attr);
    attr.bp_addr = addr;
    attr.bp_len = bp_len_kernel;
    attr.bp_type = bp_type_kernel;

    // 必须明确排除内核态，只监听用户态进程
    attr.exclude_kernel = 1;
    attr.exclude_hv = 1;

    // 获取目标进程
    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (!task)
    {
        rcu_read_unlock();
        return -ESRCH;
    }
    get_task_struct(task); // 增加引用计数
    rcu_read_unlock();

    mutex_lock(&bp_list_mutex);

    // 遍历线程组，根据 scope 为不同线程安装断点
    for_each_thread(task, t)
    {
        bool should_install = false;

        if (t == task)
        { // 是主线程
            if (scope == SCOPE_MAIN_THREAD || scope == SCOPE_ALL_THREADS)
                should_install = true;
        }
        else
        { // 是其他子线程
            if (scope == SCOPE_OTHER_THREADS || scope == SCOPE_ALL_THREADS)
                should_install = true;
        }

        if (should_install)
        {
            // 注册用户态硬件断点，并传入info
            bp = KCALL_4(fn_register_user_hw_breakpoint, struct perf_event *, &attr, sample_hbp_handler, (void *)info, t);
            if (IS_ERR(bp))
            {
                pr_debug("无法为线程 %d 设置硬件断点: %ld\n", t->pid, PTR_ERR(bp));
                continue;
            }

            // 保存到链表以便后续删除
            node = kmalloc(sizeof(*node), GFP_KERNEL);
            if (node)
            {
                node->bp = bp;
                list_add(&node->list, &bp_event_list);
            }
            else
            {
                KCALL_1(fn_unregister_hw_breakpoint, void, bp);
            }
        }
    }

    mutex_unlock(&bp_list_mutex);
    put_task_struct(task);

    return 0;
}

// 删除进程断点
static inline void remove_process_hwbp(void)
{
    struct bp_node *node, *tmp;

    mutex_lock(&bp_list_mutex);

    // 遍历删除链表节点
    list_for_each_entry_safe(node, tmp, &bp_event_list, list)
    {
        if (node->bp)
        {
            KCALL_1(fn_unregister_hw_breakpoint, void, node->bp); // 注销断点
        }
        list_del(&node->list);
        kfree(node);
    }

    mutex_unlock(&bp_list_mutex);

    pr_debug("所有注册的硬件断点已清理完毕\n");
}

// 获取断点寄存器信息
static inline void get_hw_breakpoint_info(struct hwbp_info *info)
{
    u64 dfr0;

    // 读取 ID_AA64DFR0_EL1 寄存器
    asm volatile("mrs %0, id_aa64dfr0_el1" : "=r"(dfr0));

    // 解析硬件执行断点数量 (Bits 15:12)
    // 根据ARM手册，实际数量是提取的值加 1
    info->num_brps = ((dfr0 >> 12) & 0xF) + 1;

    // 解析硬件访问断点/观察点数量 (Bits 23:20)
    // 根据ARM手册，实际数量是提取的值加 1
    info->num_wrps = ((dfr0 >> 20) & 0xF) + 1;

    pr_debug("CPU 支持的硬件执行断点 (BRPs) 数量: %llu\n", info->num_brps);
    pr_debug("CPU 支持的硬件访问断点 (WRPs) 数量: %llu\n", info->num_wrps);
}

#endif // HWBP_H