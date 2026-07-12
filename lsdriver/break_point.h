#ifndef BREAK_POINT_H
#define BREAK_POINT_H

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

#include <linux/kallsyms.h>
#include <linux/perf_event.h>
#include <linux/hw_breakpoint.h>
#include <linux/slab.h>

#include "io_struct.h"

// 断点触发回调函数
static inline void sample_hbp_handler(struct pt_regs *regs, struct bp_point *point)
{
    struct bp_record *rec = NULL;
    int i;

    if (!regs || !point) return;

    // 唯一的一次查找：查找当前 PC 是否记录过
    for (i = 0; i < point->record_count; i++)
    {
        if (point->records[i].pc == regs->pc)
        {
            rec = &point->records[i];
            break;
        }
    }
    // 如果是新 PC 且空间足够，存储到下一个槽位
    if (!rec && point->record_count < 0x100)
    {
        rec = &point->records[point->record_count];
        rec->pc = regs->pc;
        // 新槽位所有寄存器的mask默认读取
        for (i = IDX_PC; i < MAX_REG_COUNT; i++) BP_SET_MASK(rec, i, BP_OP_READ);
        point->record_count++;
    }

    // 用空间换时间，用循环+if或者switch方式都增加复杂度，要处理每个寄存器枚举索引的不同方式，直接平铺易于理解和轻微性能提升
    if (rec)
    {

        rec->hit_count++; // 命中计数
        uint8_t op;

        // PC
        op = BP_GET_MASK(rec, IDX_PC);
        if (op == BP_OP_READ) rec->pc = regs->pc;
        else if (op == BP_OP_WRITE) regs->pc = rec->pc;

        // LR (X30)
        op = BP_GET_MASK(rec, IDX_LR);
        if (op == BP_OP_READ) rec->lr = regs->regs[30];
        else if (op == BP_OP_WRITE) regs->regs[30] = rec->lr;

        // SP
        op = BP_GET_MASK(rec, IDX_SP);
        if (op == BP_OP_READ) rec->sp = regs->sp;
        else if (op == BP_OP_WRITE) regs->sp = rec->sp;

        // ORIG_X0
        op = BP_GET_MASK(rec, IDX_ORIG_X0);
        if (op == BP_OP_READ) rec->orig_x0 = regs->orig_x0;
        else if (op == BP_OP_WRITE) regs->orig_x0 = rec->orig_x0;

        // SYSCALLNO
        op = BP_GET_MASK(rec, IDX_SYSCALLNO);
        if (op == BP_OP_READ) rec->syscallno = regs->syscallno;
        else if (op == BP_OP_WRITE) regs->syscallno = rec->syscallno;

        // PSTATE
        op = BP_GET_MASK(rec, IDX_PSTATE);
        if (op == BP_OP_READ) rec->pstate = regs->pstate;
        else if (op == BP_OP_WRITE) regs->pstate = rec->pstate;

        // X0
        op = BP_GET_MASK(rec, IDX_X0);
        if (op == BP_OP_READ) rec->x0 = regs->regs[0];
        else if (op == BP_OP_WRITE) regs->regs[0] = rec->x0;

        // X1
        op = BP_GET_MASK(rec, IDX_X1);
        if (op == BP_OP_READ) rec->x1 = regs->regs[1];
        else if (op == BP_OP_WRITE) regs->regs[1] = rec->x1;

        // X2
        op = BP_GET_MASK(rec, IDX_X2);
        if (op == BP_OP_READ) rec->x2 = regs->regs[2];
        else if (op == BP_OP_WRITE) regs->regs[2] = rec->x2;

        // X3
        op = BP_GET_MASK(rec, IDX_X3);
        if (op == BP_OP_READ) rec->x3 = regs->regs[3];
        else if (op == BP_OP_WRITE) regs->regs[3] = rec->x3;

        // X4
        op = BP_GET_MASK(rec, IDX_X4);
        if (op == BP_OP_READ) rec->x4 = regs->regs[4];
        else if (op == BP_OP_WRITE) regs->regs[4] = rec->x4;

        // X5
        op = BP_GET_MASK(rec, IDX_X5);
        if (op == BP_OP_READ) rec->x5 = regs->regs[5];
        else if (op == BP_OP_WRITE) regs->regs[5] = rec->x5;

        // X6
        op = BP_GET_MASK(rec, IDX_X6);
        if (op == BP_OP_READ) rec->x6 = regs->regs[6];
        else if (op == BP_OP_WRITE) regs->regs[6] = rec->x6;

        // X7
        op = BP_GET_MASK(rec, IDX_X7);
        if (op == BP_OP_READ) rec->x7 = regs->regs[7];
        else if (op == BP_OP_WRITE) regs->regs[7] = rec->x7;

        // X8
        op = BP_GET_MASK(rec, IDX_X8);
        if (op == BP_OP_READ) rec->x8 = regs->regs[8];
        else if (op == BP_OP_WRITE) regs->regs[8] = rec->x8;

        // X9
        op = BP_GET_MASK(rec, IDX_X9);
        if (op == BP_OP_READ) rec->x9 = regs->regs[9];
        else if (op == BP_OP_WRITE) regs->regs[9] = rec->x9;

        // X10
        op = BP_GET_MASK(rec, IDX_X10);
        if (op == BP_OP_READ) rec->x10 = regs->regs[10];
        else if (op == BP_OP_WRITE) regs->regs[10] = rec->x10;

        // X11
        op = BP_GET_MASK(rec, IDX_X11);
        if (op == BP_OP_READ) rec->x11 = regs->regs[11];
        else if (op == BP_OP_WRITE) regs->regs[11] = rec->x11;

        // X12
        op = BP_GET_MASK(rec, IDX_X12);
        if (op == BP_OP_READ) rec->x12 = regs->regs[12];
        else if (op == BP_OP_WRITE) regs->regs[12] = rec->x12;

        // X13
        op = BP_GET_MASK(rec, IDX_X13);
        if (op == BP_OP_READ) rec->x13 = regs->regs[13];
        else if (op == BP_OP_WRITE) regs->regs[13] = rec->x13;

        // X14
        op = BP_GET_MASK(rec, IDX_X14);
        if (op == BP_OP_READ) rec->x14 = regs->regs[14];
        else if (op == BP_OP_WRITE) regs->regs[14] = rec->x14;

        // X15
        op = BP_GET_MASK(rec, IDX_X15);
        if (op == BP_OP_READ) rec->x15 = regs->regs[15];
        else if (op == BP_OP_WRITE) regs->regs[15] = rec->x15;

        // X16
        op = BP_GET_MASK(rec, IDX_X16);
        if (op == BP_OP_READ) rec->x16 = regs->regs[16];
        else if (op == BP_OP_WRITE) regs->regs[16] = rec->x16;

        // X17
        op = BP_GET_MASK(rec, IDX_X17);
        if (op == BP_OP_READ) rec->x17 = regs->regs[17];
        else if (op == BP_OP_WRITE) regs->regs[17] = rec->x17;

        // X18
        op = BP_GET_MASK(rec, IDX_X18);
        if (op == BP_OP_READ) rec->x18 = regs->regs[18];
        else if (op == BP_OP_WRITE) regs->regs[18] = rec->x18;

        // X19
        op = BP_GET_MASK(rec, IDX_X19);
        if (op == BP_OP_READ) rec->x19 = regs->regs[19];
        else if (op == BP_OP_WRITE) regs->regs[19] = rec->x19;

        // X20
        op = BP_GET_MASK(rec, IDX_X20);
        if (op == BP_OP_READ) rec->x20 = regs->regs[20];
        else if (op == BP_OP_WRITE) regs->regs[20] = rec->x20;

        // X21
        op = BP_GET_MASK(rec, IDX_X21);
        if (op == BP_OP_READ) rec->x21 = regs->regs[21];
        else if (op == BP_OP_WRITE) regs->regs[21] = rec->x21;

        // X22
        op = BP_GET_MASK(rec, IDX_X22);
        if (op == BP_OP_READ) rec->x22 = regs->regs[22];
        else if (op == BP_OP_WRITE) regs->regs[22] = rec->x22;

        // X23
        op = BP_GET_MASK(rec, IDX_X23);
        if (op == BP_OP_READ) rec->x23 = regs->regs[23];
        else if (op == BP_OP_WRITE) regs->regs[23] = rec->x23;

        // X24
        op = BP_GET_MASK(rec, IDX_X24);
        if (op == BP_OP_READ) rec->x24 = regs->regs[24];
        else if (op == BP_OP_WRITE) regs->regs[24] = rec->x24;

        // X25
        op = BP_GET_MASK(rec, IDX_X25);
        if (op == BP_OP_READ) rec->x25 = regs->regs[25];
        else if (op == BP_OP_WRITE) regs->regs[25] = rec->x25;

        // X26
        op = BP_GET_MASK(rec, IDX_X26);
        if (op == BP_OP_READ) rec->x26 = regs->regs[26];
        else if (op == BP_OP_WRITE) regs->regs[26] = rec->x26;

        // X27
        op = BP_GET_MASK(rec, IDX_X27);
        if (op == BP_OP_READ) rec->x27 = regs->regs[27];
        else if (op == BP_OP_WRITE) regs->regs[27] = rec->x27;

        // X28
        op = BP_GET_MASK(rec, IDX_X28);
        if (op == BP_OP_READ) rec->x28 = regs->regs[28];
        else if (op == BP_OP_WRITE) regs->regs[28] = rec->x28;

        // X29
        op = BP_GET_MASK(rec, IDX_X29);
        if (op == BP_OP_READ) rec->x29 = regs->regs[29];
        else if (op == BP_OP_WRITE) regs->regs[29] = rec->x29;

        // FPSR
        op = BP_GET_MASK(rec, IDX_FPSR);
        if (op == BP_OP_READ) rec->fpsr = read_fpsr();
        else if (op == BP_OP_WRITE) write_fpsr(rec->fpsr);

        // FPCR
        op = BP_GET_MASK(rec, IDX_FPCR);
        if (op == BP_OP_READ) rec->fpcr = read_fpcr();
        else if (op == BP_OP_WRITE) write_fpcr(rec->fpcr);

        // Q0
        op = BP_GET_MASK(rec, IDX_Q0);
        if (op == BP_OP_READ) read_q_reg(0, &rec->q0);
        else if (op == BP_OP_WRITE) write_q_reg(0, &rec->q0);

        // Q1
        op = BP_GET_MASK(rec, IDX_Q1);
        if (op == BP_OP_READ) read_q_reg(1, &rec->q1);
        else if (op == BP_OP_WRITE) write_q_reg(1, &rec->q1);

        // Q2
        op = BP_GET_MASK(rec, IDX_Q2);
        if (op == BP_OP_READ) read_q_reg(2, &rec->q2);
        else if (op == BP_OP_WRITE) write_q_reg(2, &rec->q2);

        // Q3
        op = BP_GET_MASK(rec, IDX_Q3);
        if (op == BP_OP_READ) read_q_reg(3, &rec->q3);
        else if (op == BP_OP_WRITE) write_q_reg(3, &rec->q3);

        // Q4
        op = BP_GET_MASK(rec, IDX_Q4);
        if (op == BP_OP_READ) read_q_reg(4, &rec->q4);
        else if (op == BP_OP_WRITE) write_q_reg(4, &rec->q4);

        // Q5
        op = BP_GET_MASK(rec, IDX_Q5);
        if (op == BP_OP_READ) read_q_reg(5, &rec->q5);
        else if (op == BP_OP_WRITE) write_q_reg(5, &rec->q5);

        // Q6
        op = BP_GET_MASK(rec, IDX_Q6);
        if (op == BP_OP_READ) read_q_reg(6, &rec->q6);
        else if (op == BP_OP_WRITE) write_q_reg(6, &rec->q6);

        // Q7
        op = BP_GET_MASK(rec, IDX_Q7);
        if (op == BP_OP_READ) read_q_reg(7, &rec->q7);
        else if (op == BP_OP_WRITE) write_q_reg(7, &rec->q7);

        // Q8
        op = BP_GET_MASK(rec, IDX_Q8);
        if (op == BP_OP_READ) read_q_reg(8, &rec->q8);
        else if (op == BP_OP_WRITE) write_q_reg(8, &rec->q8);

        // Q9
        op = BP_GET_MASK(rec, IDX_Q9);
        if (op == BP_OP_READ) read_q_reg(9, &rec->q9);
        else if (op == BP_OP_WRITE) write_q_reg(9, &rec->q9);

        // Q10
        op = BP_GET_MASK(rec, IDX_Q10);
        if (op == BP_OP_READ) read_q_reg(10, &rec->q10);
        else if (op == BP_OP_WRITE) write_q_reg(10, &rec->q10);

        // Q11
        op = BP_GET_MASK(rec, IDX_Q11);
        if (op == BP_OP_READ) read_q_reg(11, &rec->q11);
        else if (op == BP_OP_WRITE) write_q_reg(11, &rec->q11);

        // Q12
        op = BP_GET_MASK(rec, IDX_Q12);
        if (op == BP_OP_READ) read_q_reg(12, &rec->q12);
        else if (op == BP_OP_WRITE) write_q_reg(12, &rec->q12);

        // Q13
        op = BP_GET_MASK(rec, IDX_Q13);
        if (op == BP_OP_READ) read_q_reg(13, &rec->q13);
        else if (op == BP_OP_WRITE) write_q_reg(13, &rec->q13);

        // Q14
        op = BP_GET_MASK(rec, IDX_Q14);
        if (op == BP_OP_READ) read_q_reg(14, &rec->q14);
        else if (op == BP_OP_WRITE) write_q_reg(14, &rec->q14);

        // Q15
        op = BP_GET_MASK(rec, IDX_Q15);
        if (op == BP_OP_READ) read_q_reg(15, &rec->q15);
        else if (op == BP_OP_WRITE) write_q_reg(15, &rec->q15);

        // Q16
        op = BP_GET_MASK(rec, IDX_Q16);
        if (op == BP_OP_READ) read_q_reg(16, &rec->q16);
        else if (op == BP_OP_WRITE) write_q_reg(16, &rec->q16);

        // Q17
        op = BP_GET_MASK(rec, IDX_Q17);
        if (op == BP_OP_READ) read_q_reg(17, &rec->q17);
        else if (op == BP_OP_WRITE) write_q_reg(17, &rec->q17);

        // Q18
        op = BP_GET_MASK(rec, IDX_Q18);
        if (op == BP_OP_READ) read_q_reg(18, &rec->q18);
        else if (op == BP_OP_WRITE) write_q_reg(18, &rec->q18);

        // Q19
        op = BP_GET_MASK(rec, IDX_Q19);
        if (op == BP_OP_READ) read_q_reg(19, &rec->q19);
        else if (op == BP_OP_WRITE) write_q_reg(19, &rec->q19);

        // Q20
        op = BP_GET_MASK(rec, IDX_Q20);
        if (op == BP_OP_READ) read_q_reg(20, &rec->q20);
        else if (op == BP_OP_WRITE) write_q_reg(20, &rec->q20);

        // Q21
        op = BP_GET_MASK(rec, IDX_Q21);
        if (op == BP_OP_READ) read_q_reg(21, &rec->q21);
        else if (op == BP_OP_WRITE) write_q_reg(21, &rec->q21);

        // Q22
        op = BP_GET_MASK(rec, IDX_Q22);
        if (op == BP_OP_READ) read_q_reg(22, &rec->q22);
        else if (op == BP_OP_WRITE) write_q_reg(22, &rec->q22);

        // Q23
        op = BP_GET_MASK(rec, IDX_Q23);
        if (op == BP_OP_READ) read_q_reg(23, &rec->q23);
        else if (op == BP_OP_WRITE) write_q_reg(23, &rec->q23);

        // Q24
        op = BP_GET_MASK(rec, IDX_Q24);
        if (op == BP_OP_READ) read_q_reg(24, &rec->q24);
        else if (op == BP_OP_WRITE) write_q_reg(24, &rec->q24);

        // Q25
        op = BP_GET_MASK(rec, IDX_Q25);
        if (op == BP_OP_READ) read_q_reg(25, &rec->q25);
        else if (op == BP_OP_WRITE) write_q_reg(25, &rec->q25);

        // Q26
        op = BP_GET_MASK(rec, IDX_Q26);
        if (op == BP_OP_READ) read_q_reg(26, &rec->q26);
        else if (op == BP_OP_WRITE) write_q_reg(26, &rec->q26);

        // Q27
        op = BP_GET_MASK(rec, IDX_Q27);
        if (op == BP_OP_READ) read_q_reg(27, &rec->q27);
        else if (op == BP_OP_WRITE) write_q_reg(27, &rec->q27);

        // Q28
        op = BP_GET_MASK(rec, IDX_Q28);
        if (op == BP_OP_READ) read_q_reg(28, &rec->q28);
        else if (op == BP_OP_WRITE) write_q_reg(28, &rec->q28);

        // Q29
        op = BP_GET_MASK(rec, IDX_Q29);
        if (op == BP_OP_READ) read_q_reg(29, &rec->q29);
        else if (op == BP_OP_WRITE) write_q_reg(29, &rec->q29);

        // Q30
        op = BP_GET_MASK(rec, IDX_Q30);
        if (op == BP_OP_READ) read_q_reg(30, &rec->q30);
        else if (op == BP_OP_WRITE) write_q_reg(30, &rec->q30);

        // Q31
        op = BP_GET_MASK(rec, IDX_Q31);
        if (op == BP_OP_READ) read_q_reg(31, &rec->q31);
        else if (op == BP_OP_WRITE) write_q_reg(31, &rec->q31);
    }
}

static inline void sample_hbp_handler_entry(void *regs, void *self)
{
    sample_hbp_handler((struct pt_regs *)regs, (struct bp_point *)self);
}

static inline void prepare_break_point_handlers(struct break_point *info)
{
    int point_slot;

    if (!info || info->pid <= 0) return;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (info->points[point_slot].hit_addr != 0) info->points[point_slot].on_hit = sample_hbp_handler_entry;
    }
}

#include "arm64_hwdbg.h"
static inline int set_process_hwbp(struct break_point *info)
{
    int ret;

    if (!info) return -EINVAL;

    prepare_break_point_handlers(info);

    ret = start_task_run_monitor(info);
    if (ret) return ret;

    return 0;
}

static inline void remove_process_hwbp(void)
{
    stop_task_run_monitor();
}

#include "arm64_ptedbg.h"
static inline int set_process_ptebp(struct break_point *info)
{
    if (!info) return -EINVAL;

    prepare_break_point_handlers(info);

    return start_ptebp_monitor(info);
}

static inline void remove_process_ptebp(void)
{
    stop_ptebp_monitor();
}

#include "arm64_stepdbg.h"
static inline int set_process_stepbp(struct break_point *info)
{
    if (!info) return -EINVAL;

    prepare_break_point_handlers(info);

    return start_stepbp_monitor(info);
}

static inline void remove_process_stepbp(void)
{
    stop_stepbp_monitor();
}
#endif // BREAK_POINT_H
