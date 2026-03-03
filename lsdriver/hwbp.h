
#ifndef HWBP_H
#define HWBP_H
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/hw_breakpoint.h>
#include <linux/perf_event.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <asm/ptrace.h>

typedef struct perf_event *(*register_user_hw_breakpoint_t)(struct perf_event_attr *attr, perf_overflow_handler_t triggered, void *context, struct task_struct *tsk);
typedef void (*unregister_hw_breakpoint_t)(struct perf_event *bp);

static register_user_hw_breakpoint_t fn_register_user_hw_breakpoint = NULL;
static unregister_hw_breakpoint_t fn_unregister_hw_breakpoint = NULL;

// 断点类型
enum bp_type
{
    BP_READ,       // 读
    BP_WRITE,      // 写
    BP_READ_WRITE, // 读写
    BP_EXECUTE     // 执行
};

// 断点作用线程范围
enum bp_scope
{
    SCOPE_MAIN_THREAD,   // 仅主线程
    SCOPE_OTHER_THREADS, // 仅其他子线程
    SCOPE_ALL_THREADS    // 全部线程
};

// 存储命中信息
struct hwbp_info
{
    uint64_t num_brps;  // 执行断点的数量
    uint64_t num_wrps;  // 访问断点的数量
    uint64_t hit_addr;  // 监控地址
    uint64_t hit_count; // 命中次数
    uint64_t regs[30];  // X0 ~ X29 寄存器
    uint64_t lr;        // X30 (Link Register)
    uint64_t sp;        // Stack Pointer
    uint64_t pc;        // 触发断点的汇编指令地址
    uint64_t orig_x0;   // 原始 X0 (用于系统调用重启)
    uint64_t syscallno; // 系统调用号
    uint64_t pstate;    // 处理器状态 (CPSR/PSTATE)
};

// 链表节点，用于保存注册的 perf_event 指针，方便后续删除
struct bp_node
{
    struct perf_event *bp;
    struct list_head list;
};
static LIST_HEAD(bp_event_list);
static DEFINE_MUTEX(bp_list_mutex); // 保护链表的互斥锁

// 断点触发回调函数
static void sample_hbp_handler(struct perf_event *bp, struct perf_sample_data *data, struct pt_regs *regs)
{

    // 从 perf_event 中提取注册时传入的 context 指针
    struct hwbp_info *info = (struct hwbp_info *)bp->overflow_handler_context;

    if (!info)
        return;

    info->hit_addr = bp->attr.bp_addr;
    info->hit_count++;

    //  X0 ~ X29
    memcpy(info->regs, regs->regs, sizeof(u64) * 30);

    // ARM64 pt_regs 中, regs[30] 是 LR
    info->lr = regs->regs[30];
    info->sp = regs->sp;
    info->pc = regs->pc;
    info->orig_x0 = regs->orig_x0;
    info->syscallno = regs->syscallno;
    info->pstate = regs->pstate;
}

// 设置进程断点
int set_process_hwbp(pid_t pid, uint64_t addr, enum bp_type type, enum bp_scope scope, int len_bytes, struct hwbp_info *info)
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
        bp_len_kernel = HW_BREAKPOINT_LEN_4; // 执行断点必须是 4 字节 (指令长度)
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

    // 必须指定采样周期为 1 ，意思是每一次命中都要通知我
    // 这会让 perf 子系统启用正确的硬件单步步过机制，防止死循环卡死！
    attr.sample_period = 1;

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

    // 遍历线程组， 根据 scope 为不同线程安装断点
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
            bp = fn_register_user_hw_breakpoint(&attr, sample_hbp_handler, (void *)info, t);
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
                fn_unregister_hw_breakpoint(bp);
            }
        }
    }

    mutex_unlock(&bp_list_mutex);
    put_task_struct(task);

    return 0;
}

// 删除进程断点
void remove_process_hwbp(void)
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
        list_del(&node->list);
        kfree(node);
    }

    mutex_unlock(&bp_list_mutex);

    pr_debug("所有注册的硬件断点已清理完毕\n");
}

// 获取断点寄存器信息
static void get_hw_breakpoint_info(struct hwbp_info *info)
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