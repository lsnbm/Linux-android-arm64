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

#include <linux/kallsyms.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <linux/slab.h>

typedef struct perf_event *(*register_user_hw_breakpoint_t)(struct perf_event_attr *attr, perf_overflow_handler_t triggered, void *context, struct task_struct *tsk);
typedef void (*unregister_hw_breakpoint_t)(struct perf_event *bp);
typedef int (*modify_user_hw_breakpoint_t)(struct perf_event *bp, struct perf_event_attr *attr); // 新增API定义

static register_user_hw_breakpoint_t fn_register_user_hw_breakpoint = NULL;
static unregister_hw_breakpoint_t fn_unregister_hw_breakpoint = NULL;
static modify_user_hw_breakpoint_t fn_modify_user_hw_breakpoint = NULL;

// 每个线程独立的断点上下文，用于维持步过状态
struct bp_thread_ctx
{
    struct hwbp_info *info;           // 指向全局记录结构
    struct perf_event_attr orig_attr; // 保存原始的断点属性（方便恢复）
    bool is_stepping;                 // 标记当前是否处于“步过”状态
};

// 链表节点，用于保存注册的 perf_event 指针，方便后续删除
struct bp_node
{
    struct perf_event *bp;
    struct bp_thread_ctx *ctx; // 保存上下文指针用于释放
    struct list_head list;
};

// 全局链表头
static LIST_HEAD(bp_event_list);

// 保护全局断点事件链表的互斥锁（允许睡眠，用于注册/注销断点）
static DEFINE_MUTEX(bp_list_mutex);

// 保护 hwbp_info 数据记录的全局自旋锁（禁止睡眠，用于中断回调中保护数据）
static DEFINE_SPINLOCK(hwbp_record_lock);

/*
能力有限啊，无法写好指令模拟器，全是bug(不是指令错误就是计算有误)，只能用modify_user_hw_breakpoint来移动断点位置实现步过了
*/

// 断点触发回调函数
static inline void sample_hbp_handler(struct perf_event *bp, struct perf_sample_data *data, struct pt_regs *regs)
{
    struct bp_thread_ctx *ctx = (struct bp_thread_ctx *)bp->overflow_handler_context;
    unsigned long flags;
    struct hwbp_record *rec = NULL;
    int i;

    if (!ctx || !ctx->info)
        return;

    // 如果当前正在“步过”，说明我们命中了临时断点
    if (ctx->is_stepping)
    {
        ctx->is_stepping = false;

        // 恢复为原始的断点属性
        fn_modify_user_hw_breakpoint(bp, &ctx->orig_attr);
        return; // 直接返回，让程序继续执行
    }

    // 下面命中原始断点，执行读写逻辑，并准备进入“步过”状态
    //------------------------------------------------------------

    spin_lock_irqsave(&hwbp_record_lock, flags);

    // 直接从 perf 属性中获取原始断点地址
    ctx->info->hit_addr = bp->attr.bp_addr;

    // 唯一的一次查找：查找当前 PC 是否记录过
    for (i = 0; i < ctx->info->record_count; i++)
    {
        if (ctx->info->records[i].pc == regs->pc)
        {
            rec = &ctx->info->records[i];
            break;
        }
    }
    // 如果是新 PC 且空间足够，存储到下一个槽位
    if (!rec && ctx->info->record_count < 0x100)
    {
        rec = &ctx->info->records[ctx->info->record_count];
        rec->pc = regs->pc;
        ctx->info->record_count++;
    }

    // 用空间换时间，用循环+if或者switch方式都增加复杂度，要处理每个寄存器枚举索引的不同方式，直接平铺易于理解和轻微性能提升
    if (rec)
    {

        rec->hit_count++; // 命中计数
        uint8_t op;

        // PC
        op = HWBP_GET_MASK(rec, IDX_PC);
        if (op == HWBP_OP_READ)
            rec->pc = regs->pc;
        else if (op == HWBP_OP_WRITE)
            regs->pc = rec->pc;

        // LR (X30)
        op = HWBP_GET_MASK(rec, IDX_LR);
        if (op == HWBP_OP_READ)
            rec->lr = regs->regs[30];
        else if (op == HWBP_OP_WRITE)
            regs->regs[30] = rec->lr;

        // SP
        op = HWBP_GET_MASK(rec, IDX_SP);
        if (op == HWBP_OP_READ)
            rec->sp = regs->sp;
        else if (op == HWBP_OP_WRITE)
            regs->sp = rec->sp;

        // ORIG_X0
        op = HWBP_GET_MASK(rec, IDX_ORIG_X0);
        if (op == HWBP_OP_READ)
            rec->orig_x0 = regs->orig_x0;
        else if (op == HWBP_OP_WRITE)
            regs->orig_x0 = rec->orig_x0;

        // SYSCALLNO
        op = HWBP_GET_MASK(rec, IDX_SYSCALLNO);
        if (op == HWBP_OP_READ)
            rec->syscallno = regs->syscallno;
        else if (op == HWBP_OP_WRITE)
            regs->syscallno = rec->syscallno;

        // PSTATE
        op = HWBP_GET_MASK(rec, IDX_PSTATE);
        if (op == HWBP_OP_READ)
            rec->pstate = regs->pstate;
        else if (op == HWBP_OP_WRITE)
            regs->pstate = rec->pstate;

        // X0
        op = HWBP_GET_MASK(rec, IDX_X0);
        if (op == HWBP_OP_READ)
            rec->x0 = regs->regs[0];
        else if (op == HWBP_OP_WRITE)
            regs->regs[0] = rec->x0;

        // X1
        op = HWBP_GET_MASK(rec, IDX_X1);
        if (op == HWBP_OP_READ)
            rec->x1 = regs->regs[1];
        else if (op == HWBP_OP_WRITE)
            regs->regs[1] = rec->x1;

        // X2
        op = HWBP_GET_MASK(rec, IDX_X2);
        if (op == HWBP_OP_READ)
            rec->x2 = regs->regs[2];
        else if (op == HWBP_OP_WRITE)
            regs->regs[2] = rec->x2;

        // X3
        op = HWBP_GET_MASK(rec, IDX_X3);
        if (op == HWBP_OP_READ)
            rec->x3 = regs->regs[3];
        else if (op == HWBP_OP_WRITE)
            regs->regs[3] = rec->x3;

        // X4
        op = HWBP_GET_MASK(rec, IDX_X4);
        if (op == HWBP_OP_READ)
            rec->x4 = regs->regs[4];
        else if (op == HWBP_OP_WRITE)
            regs->regs[4] = rec->x4;

        // X5
        op = HWBP_GET_MASK(rec, IDX_X5);
        if (op == HWBP_OP_READ)
            rec->x5 = regs->regs[5];
        else if (op == HWBP_OP_WRITE)
            regs->regs[5] = rec->x5;

        // X6
        op = HWBP_GET_MASK(rec, IDX_X6);
        if (op == HWBP_OP_READ)
            rec->x6 = regs->regs[6];
        else if (op == HWBP_OP_WRITE)
            regs->regs[6] = rec->x6;

        // X7
        op = HWBP_GET_MASK(rec, IDX_X7);
        if (op == HWBP_OP_READ)
            rec->x7 = regs->regs[7];
        else if (op == HWBP_OP_WRITE)
            regs->regs[7] = rec->x7;

        // X8
        op = HWBP_GET_MASK(rec, IDX_X8);
        if (op == HWBP_OP_READ)
            rec->x8 = regs->regs[8];
        else if (op == HWBP_OP_WRITE)
            regs->regs[8] = rec->x8;

        // X9
        op = HWBP_GET_MASK(rec, IDX_X9);
        if (op == HWBP_OP_READ)
            rec->x9 = regs->regs[9];
        else if (op == HWBP_OP_WRITE)
            regs->regs[9] = rec->x9;

        // X10
        op = HWBP_GET_MASK(rec, IDX_X10);
        if (op == HWBP_OP_READ)
            rec->x10 = regs->regs[10];
        else if (op == HWBP_OP_WRITE)
            regs->regs[10] = rec->x10;

        // X11
        op = HWBP_GET_MASK(rec, IDX_X11);
        if (op == HWBP_OP_READ)
            rec->x11 = regs->regs[11];
        else if (op == HWBP_OP_WRITE)
            regs->regs[11] = rec->x11;

        // X12
        op = HWBP_GET_MASK(rec, IDX_X12);
        if (op == HWBP_OP_READ)
            rec->x12 = regs->regs[12];
        else if (op == HWBP_OP_WRITE)
            regs->regs[12] = rec->x12;

        // X13
        op = HWBP_GET_MASK(rec, IDX_X13);
        if (op == HWBP_OP_READ)
            rec->x13 = regs->regs[13];
        else if (op == HWBP_OP_WRITE)
            regs->regs[13] = rec->x13;

        // X14
        op = HWBP_GET_MASK(rec, IDX_X14);
        if (op == HWBP_OP_READ)
            rec->x14 = regs->regs[14];
        else if (op == HWBP_OP_WRITE)
            regs->regs[14] = rec->x14;

        // X15
        op = HWBP_GET_MASK(rec, IDX_X15);
        if (op == HWBP_OP_READ)
            rec->x15 = regs->regs[15];
        else if (op == HWBP_OP_WRITE)
            regs->regs[15] = rec->x15;

        // X16
        op = HWBP_GET_MASK(rec, IDX_X16);
        if (op == HWBP_OP_READ)
            rec->x16 = regs->regs[16];
        else if (op == HWBP_OP_WRITE)
            regs->regs[16] = rec->x16;

        // X17
        op = HWBP_GET_MASK(rec, IDX_X17);
        if (op == HWBP_OP_READ)
            rec->x17 = regs->regs[17];
        else if (op == HWBP_OP_WRITE)
            regs->regs[17] = rec->x17;

        // X18
        op = HWBP_GET_MASK(rec, IDX_X18);
        if (op == HWBP_OP_READ)
            rec->x18 = regs->regs[18];
        else if (op == HWBP_OP_WRITE)
            regs->regs[18] = rec->x18;

        // X19
        op = HWBP_GET_MASK(rec, IDX_X19);
        if (op == HWBP_OP_READ)
            rec->x19 = regs->regs[19];
        else if (op == HWBP_OP_WRITE)
            regs->regs[19] = rec->x19;

        // X20
        op = HWBP_GET_MASK(rec, IDX_X20);
        if (op == HWBP_OP_READ)
            rec->x20 = regs->regs[20];
        else if (op == HWBP_OP_WRITE)
            regs->regs[20] = rec->x20;

        // X21
        op = HWBP_GET_MASK(rec, IDX_X21);
        if (op == HWBP_OP_READ)
            rec->x21 = regs->regs[21];
        else if (op == HWBP_OP_WRITE)
            regs->regs[21] = rec->x21;

        // X22
        op = HWBP_GET_MASK(rec, IDX_X22);
        if (op == HWBP_OP_READ)
            rec->x22 = regs->regs[22];
        else if (op == HWBP_OP_WRITE)
            regs->regs[22] = rec->x22;

        // X23
        op = HWBP_GET_MASK(rec, IDX_X23);
        if (op == HWBP_OP_READ)
            rec->x23 = regs->regs[23];
        else if (op == HWBP_OP_WRITE)
            regs->regs[23] = rec->x23;

        // X24
        op = HWBP_GET_MASK(rec, IDX_X24);
        if (op == HWBP_OP_READ)
            rec->x24 = regs->regs[24];
        else if (op == HWBP_OP_WRITE)
            regs->regs[24] = rec->x24;

        // X25
        op = HWBP_GET_MASK(rec, IDX_X25);
        if (op == HWBP_OP_READ)
            rec->x25 = regs->regs[25];
        else if (op == HWBP_OP_WRITE)
            regs->regs[25] = rec->x25;

        // X26
        op = HWBP_GET_MASK(rec, IDX_X26);
        if (op == HWBP_OP_READ)
            rec->x26 = regs->regs[26];
        else if (op == HWBP_OP_WRITE)
            regs->regs[26] = rec->x26;

        // X27
        op = HWBP_GET_MASK(rec, IDX_X27);
        if (op == HWBP_OP_READ)
            rec->x27 = regs->regs[27];
        else if (op == HWBP_OP_WRITE)
            regs->regs[27] = rec->x27;

        // X28
        op = HWBP_GET_MASK(rec, IDX_X28);
        if (op == HWBP_OP_READ)
            rec->x28 = regs->regs[28];
        else if (op == HWBP_OP_WRITE)
            regs->regs[28] = rec->x28;

        // X29
        op = HWBP_GET_MASK(rec, IDX_X29);
        if (op == HWBP_OP_READ)
            rec->x29 = regs->regs[29];
        else if (op == HWBP_OP_WRITE)
            regs->regs[29] = rec->x29;

        // FPSR
        op = HWBP_GET_MASK(rec, IDX_FPSR);
        if (op == HWBP_OP_READ)
        {
            uint64_t v;
            asm volatile(".arch_extension fp\n"
                         "mrs %0, fpsr"
                         : "=r"(v));
            rec->fpsr = (uint32_t)v;
        }
        else if (op == HWBP_OP_WRITE)
        {
            uint64_t v = rec->fpsr;
            asm volatile(".arch_extension fp\n"
                         "msr fpsr, %0"
                         :
                         : "r"(v));
        }

        // FPCR
        op = HWBP_GET_MASK(rec, IDX_FPCR);
        if (op == HWBP_OP_READ)
        {
            uint64_t v;
            asm volatile(".arch_extension fp\n"
                         "mrs %0, fpcr"
                         : "=r"(v));
            rec->fpcr = (uint32_t)v;
        }
        else if (op == HWBP_OP_WRITE)
        {
            uint64_t v = rec->fpcr;
            asm volatile(".arch_extension fp\n"
                         "msr fpcr, %0"
                         :
                         : "r"(v));
        }

        // Q0
        op = HWBP_GET_MASK(rec, IDX_Q0);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q0, [%0]\n"
                         :
                         : "r"(&rec->q0)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q0, [%0]\n"
                         :
                         : "r"(&rec->q0)
                         : "memory");

        // Q1
        op = HWBP_GET_MASK(rec, IDX_Q1);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q1, [%0]\n"
                         :
                         : "r"(&rec->q1)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q1, [%0]\n"
                         :
                         : "r"(&rec->q1)
                         : "memory");

        // Q2
        op = HWBP_GET_MASK(rec, IDX_Q2);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q2, [%0]\n"
                         :
                         : "r"(&rec->q2)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q2, [%0]\n"
                         :
                         : "r"(&rec->q2)
                         : "memory");

        // Q3
        op = HWBP_GET_MASK(rec, IDX_Q3);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q3, [%0]\n"
                         :
                         : "r"(&rec->q3)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q3, [%0]\n"
                         :
                         : "r"(&rec->q3)
                         : "memory");

        // Q4
        op = HWBP_GET_MASK(rec, IDX_Q4);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q4, [%0]\n"
                         :
                         : "r"(&rec->q4)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q4, [%0]\n"
                         :
                         : "r"(&rec->q4)
                         : "memory");

        // Q5
        op = HWBP_GET_MASK(rec, IDX_Q5);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q5, [%0]\n"
                         :
                         : "r"(&rec->q5)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q5, [%0]\n"
                         :
                         : "r"(&rec->q5)
                         : "memory");

        // Q6
        op = HWBP_GET_MASK(rec, IDX_Q6);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q6, [%0]\n"
                         :
                         : "r"(&rec->q6)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q6, [%0]\n"
                         :
                         : "r"(&rec->q6)
                         : "memory");

        // Q7
        op = HWBP_GET_MASK(rec, IDX_Q7);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q7, [%0]\n"
                         :
                         : "r"(&rec->q7)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q7, [%0]\n"
                         :
                         : "r"(&rec->q7)
                         : "memory");

        // Q8
        op = HWBP_GET_MASK(rec, IDX_Q8);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q8, [%0]\n"
                         :
                         : "r"(&rec->q8)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q8, [%0]\n"
                         :
                         : "r"(&rec->q8)
                         : "memory");

        // Q9
        op = HWBP_GET_MASK(rec, IDX_Q9);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q9, [%0]\n"
                         :
                         : "r"(&rec->q9)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q9, [%0]\n"
                         :
                         : "r"(&rec->q9)
                         : "memory");

        // Q10
        op = HWBP_GET_MASK(rec, IDX_Q10);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q10, [%0]\n"
                         :
                         : "r"(&rec->q10)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q10, [%0]\n"
                         :
                         : "r"(&rec->q10)
                         : "memory");

        // Q11
        op = HWBP_GET_MASK(rec, IDX_Q11);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q11, [%0]\n"
                         :
                         : "r"(&rec->q11)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q11, [%0]\n"
                         :
                         : "r"(&rec->q11)
                         : "memory");

        // Q12
        op = HWBP_GET_MASK(rec, IDX_Q12);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q12, [%0]\n"
                         :
                         : "r"(&rec->q12)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q12, [%0]\n"
                         :
                         : "r"(&rec->q12)
                         : "memory");

        // Q13
        op = HWBP_GET_MASK(rec, IDX_Q13);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q13, [%0]\n"
                         :
                         : "r"(&rec->q13)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q13, [%0]\n"
                         :
                         : "r"(&rec->q13)
                         : "memory");

        // Q14
        op = HWBP_GET_MASK(rec, IDX_Q14);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q14, [%0]\n"
                         :
                         : "r"(&rec->q14)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q14, [%0]\n"
                         :
                         : "r"(&rec->q14)
                         : "memory");

        // Q15
        op = HWBP_GET_MASK(rec, IDX_Q15);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q15, [%0]\n"
                         :
                         : "r"(&rec->q15)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q15, [%0]\n"
                         :
                         : "r"(&rec->q15)
                         : "memory");

        // Q16
        op = HWBP_GET_MASK(rec, IDX_Q16);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q16, [%0]\n"
                         :
                         : "r"(&rec->q16)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q16, [%0]\n"
                         :
                         : "r"(&rec->q16)
                         : "memory");

        // Q17
        op = HWBP_GET_MASK(rec, IDX_Q17);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q17, [%0]\n"
                         :
                         : "r"(&rec->q17)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q17, [%0]\n"
                         :
                         : "r"(&rec->q17)
                         : "memory");

        // Q18
        op = HWBP_GET_MASK(rec, IDX_Q18);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q18, [%0]\n"
                         :
                         : "r"(&rec->q18)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q18, [%0]\n"
                         :
                         : "r"(&rec->q18)
                         : "memory");

        // Q19
        op = HWBP_GET_MASK(rec, IDX_Q19);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q19, [%0]\n"
                         :
                         : "r"(&rec->q19)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q19, [%0]\n"
                         :
                         : "r"(&rec->q19)
                         : "memory");

        // Q20
        op = HWBP_GET_MASK(rec, IDX_Q20);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q20, [%0]\n"
                         :
                         : "r"(&rec->q20)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q20, [%0]\n"
                         :
                         : "r"(&rec->q20)
                         : "memory");

        // Q21
        op = HWBP_GET_MASK(rec, IDX_Q21);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q21, [%0]\n"
                         :
                         : "r"(&rec->q21)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q21, [%0]\n"
                         :
                         : "r"(&rec->q21)
                         : "memory");

        // Q22
        op = HWBP_GET_MASK(rec, IDX_Q22);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q22, [%0]\n"
                         :
                         : "r"(&rec->q22)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q22, [%0]\n"
                         :
                         : "r"(&rec->q22)
                         : "memory");

        // Q23
        op = HWBP_GET_MASK(rec, IDX_Q23);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q23, [%0]\n"
                         :
                         : "r"(&rec->q23)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q23, [%0]\n"
                         :
                         : "r"(&rec->q23)
                         : "memory");

        // Q24
        op = HWBP_GET_MASK(rec, IDX_Q24);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q24, [%0]\n"
                         :
                         : "r"(&rec->q24)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q24, [%0]\n"
                         :
                         : "r"(&rec->q24)
                         : "memory");

        // Q25
        op = HWBP_GET_MASK(rec, IDX_Q25);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q25, [%0]\n"
                         :
                         : "r"(&rec->q25)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q25, [%0]\n"
                         :
                         : "r"(&rec->q25)
                         : "memory");

        // Q26
        op = HWBP_GET_MASK(rec, IDX_Q26);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q26, [%0]\n"
                         :
                         : "r"(&rec->q26)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q26, [%0]\n"
                         :
                         : "r"(&rec->q26)
                         : "memory");

        // Q27
        op = HWBP_GET_MASK(rec, IDX_Q27);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q27, [%0]\n"
                         :
                         : "r"(&rec->q27)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q27, [%0]\n"
                         :
                         : "r"(&rec->q27)
                         : "memory");

        // Q28
        op = HWBP_GET_MASK(rec, IDX_Q28);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q28, [%0]\n"
                         :
                         : "r"(&rec->q28)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q28, [%0]\n"
                         :
                         : "r"(&rec->q28)
                         : "memory");

        // Q29
        op = HWBP_GET_MASK(rec, IDX_Q29);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q29, [%0]\n"
                         :
                         : "r"(&rec->q29)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q29, [%0]\n"
                         :
                         : "r"(&rec->q29)
                         : "memory");

        // Q30
        op = HWBP_GET_MASK(rec, IDX_Q30);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q30, [%0]\n"
                         :
                         : "r"(&rec->q30)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q30, [%0]\n"
                         :
                         : "r"(&rec->q30)
                         : "memory");

        // Q31
        op = HWBP_GET_MASK(rec, IDX_Q31);
        if (op == HWBP_OP_READ)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "str q31, [%0]\n"
                         :
                         : "r"(&rec->q31)
                         : "memory");
        else if (op == HWBP_OP_WRITE)
            asm volatile(".arch_extension fp\n"
                         ".arch_extension simd\n"
                         "ldr q31, [%0]\n"
                         :
                         : "r"(&rec->q31)
                         : "memory");
    }

    spin_unlock_irqrestore(&hwbp_record_lock, flags);

    /*
    这里pc+4不用管上面写入了啥值，目的就为了移动断点位置让原始断点继续跑下去，等零时断点PC+4触发了在移动断点回来实现步过
    */
    ctx->is_stepping = true;
    struct perf_event_attr step_attr = ctx->orig_attr;
    step_attr.bp_type = HW_BREAKPOINT_X;    // 改为执行断点
    step_attr.bp_addr = regs->pc + 4;       // 下一条指令的位置
    step_attr.bp_len = HW_BREAKPOINT_LEN_8; // ARM64 执行断点长度

    fn_modify_user_hw_breakpoint(bp, &step_attr);
}

// 设置进程断点
static inline int set_process_hwbp(pid_t pid, uint64_t addr, enum bp_type type, enum bp_scope scope, int len_bytes, struct hwbp_info *info)
{
    struct perf_event_attr attr;
    struct task_struct *task, *t;
    struct perf_event *bp;
    struct bp_node *node;
    struct bp_thread_ctx *ctx;
    int bp_type_kernel;
    int bp_len_kernel;

    if (!fn_register_user_hw_breakpoint || !fn_unregister_hw_breakpoint || !fn_modify_user_hw_breakpoint)
    {
        fn_register_user_hw_breakpoint = (register_user_hw_breakpoint_t)generic_kallsyms_lookup_name("register_user_hw_breakpoint");
        fn_unregister_hw_breakpoint = (unregister_hw_breakpoint_t)generic_kallsyms_lookup_name("unregister_hw_breakpoint");
        fn_modify_user_hw_breakpoint = (modify_user_hw_breakpoint_t)generic_kallsyms_lookup_name("modify_user_hw_breakpoint");

        if (!fn_register_user_hw_breakpoint || !fn_unregister_hw_breakpoint || !fn_modify_user_hw_breakpoint)
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
            // 为每个线程分配独立的上下文
            ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
            if (!ctx)
                continue;

            ctx->info = info;
            ctx->orig_attr = attr;
            ctx->is_stepping = false;

            // 注册用户态硬件断点，并传入ctx
            bp = fn_register_user_hw_breakpoint(&attr, sample_hbp_handler, (void *)ctx, t);
            if (IS_ERR(bp))
            {
                pr_debug("无法为线程 %d 设置硬件断点: %ld\n", t->pid, PTR_ERR(bp));
                kfree(ctx);
                continue;
            }

            // 保存到链表以便后续删除
            node = kmalloc(sizeof(*node), GFP_KERNEL);
            if (node)
            {
                node->bp = bp;
                node->ctx = ctx;
                list_add(&node->list, &bp_event_list);
            }
            else
            {
                fn_unregister_hw_breakpoint(bp);
                kfree(ctx);
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
            fn_unregister_hw_breakpoint(node->bp); // 注销断点
        }
        if (node->ctx)
        {
            kfree(node->ctx); // 释放上下文
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
