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

// 读取 Watchpoint Value Register (WVR) - 用于读写断点
static uint64_t read_wvr(int n)
{
    uint64_t val = 0;
    switch (n)
    {
    case 0:
        asm volatile("mrs %0, dbgwvr0_el1" : "=r"(val));
        break;
    case 1:
        asm volatile("mrs %0, dbgwvr1_el1" : "=r"(val));
        break;
    case 2:
        asm volatile("mrs %0, dbgwvr2_el1" : "=r"(val));
        break;
    case 3:
        asm volatile("mrs %0, dbgwvr3_el1" : "=r"(val));
        break;
    case 4:
        asm volatile("mrs %0, dbgwvr4_el1" : "=r"(val));
        break;
    case 5:
        asm volatile("mrs %0, dbgwvr5_el1" : "=r"(val));
        break;
    // 绝大多数手机只有 4~6 个 WRP，这里写到 5 足够覆盖99%的设备
    default:
        pr_err("读取未知的 WVR 槽位: %d\n", n);
    }
    return val;
}

// 写入 Watchpoint Value Register (WVR) - 用于读写断点
static void write_wvr(int n, uint64_t val)
{
    switch (n)
    {
    case 0:
        asm volatile("msr dbgwvr0_el1, %0" ::"r"(val));
        break;
    case 1:
        asm volatile("msr dbgwvr1_el1, %0" ::"r"(val));
        break;
    case 2:
        asm volatile("msr dbgwvr2_el1, %0" ::"r"(val));
        break;
    case 3:
        asm volatile("msr dbgwvr3_el1, %0" ::"r"(val));
        break;
    case 4:
        asm volatile("msr dbgwvr4_el1, %0" ::"r"(val));
        break;
    case 5:
        asm volatile("msr dbgwvr5_el1, %0" ::"r"(val));
        break;
    }
    isb(); // 必须加上指令同步屏障，确保硬件立刻生效
}

// 读取 Breakpoint Value Register (BVR) - 用于执行断点
static uint64_t read_bvr(int n)
{
    uint64_t val = 0;
    switch (n)
    {
    case 0:
        asm volatile("mrs %0, dbgbvr0_el1" : "=r"(val));
        break;
    case 1:
        asm volatile("mrs %0, dbgbvr1_el1" : "=r"(val));
        break;
    case 2:
        asm volatile("mrs %0, dbgbvr2_el1" : "=r"(val));
        break;
    case 3:
        asm volatile("mrs %0, dbgbvr3_el1" : "=r"(val));
        break;
    case 4:
        asm volatile("mrs %0, dbgbvr4_el1" : "=r"(val));
        break;
    case 5:
        asm volatile("mrs %0, dbgbvr5_el1" : "=r"(val));
        break;
    }
    return val;
}

// 写入 Breakpoint Value Register (BVR) - 用于执行断点
static void write_bvr(int n, uint64_t val)
{
    switch (n)
    {
    case 0:
        asm volatile("msr dbgbvr0_el1, %0" ::"r"(val));
        break;
    case 1:
        asm volatile("msr dbgbvr1_el1, %0" ::"r"(val));
        break;
    case 2:
        asm volatile("msr dbgbvr2_el1, %0" ::"r"(val));
        break;
    case 3:
        asm volatile("msr dbgbvr3_el1, %0" ::"r"(val));
        break;
    case 4:
        asm volatile("msr dbgbvr4_el1, %0" ::"r"(val));
        break;
    case 5:
        asm volatile("msr dbgbvr5_el1, %0" ::"r"(val));
        break;
    }
    isb();
}

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
    uint64_t lr;        // X30
    uint64_t sp;        // Stack Pointer
    uint64_t pc;        // 触发断点的汇编指令地址
    uint64_t orig_x0;   // 原始 X0 (用于系统调用重启)
    uint64_t syscallno; // 系统调用号
    uint64_t pstate;    // 处理器状态
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

    static uint64_t static_orig_addr = 0;   // 原始监控的地址
    static bool static_is_stepping = false; // 乒乓状态机标记
    static int static_slot_idx = -1;        // 记录该断点被分配到了哪个硬件槽位

    // 从 perf_event 中提取注册时传入的 context 指针
    struct hwbp_info *info = (struct hwbp_info *)bp->overflow_handler_context;

    int i;
    int max_slots;
    uint64_t current_hw_addr;
    bool is_execute_bp;
    uint64_t next_pc;

    if (!info)
        return;

    // 初始化判断类型
    is_execute_bp = (bp->attr.bp_type == HW_BREAKPOINT_X);
    if (!static_is_stepping)
    {
        // 自动从内核属性中获取并记录原始地址
        static_orig_addr = bp->attr.bp_addr;

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

        pr_info("【命中记录】目标地址: 0x%llx, 当前PC: 0x%llx\n", static_orig_addr, regs->pc);

        static_slot_idx = -1;
        max_slots = 6;
        for (i = 0; i < max_slots; i++)
        {
            current_hw_addr = is_execute_bp ? read_bvr(i) : read_wvr(i);
            if ((current_hw_addr & ~0x7ULL) == (static_orig_addr & ~0x7ULL))
            {
                static_slot_idx = i;
                break;
            }
        }

        if (static_slot_idx != -1)
        {

            next_pc = regs->pc + 4;
            if (is_execute_bp)
                write_bvr(static_slot_idx, next_pc);
            else
                write_wvr(static_slot_idx, next_pc);

            static_is_stepping = true;
        }
    }
    else
    {
        if (static_slot_idx != -1)
        {

            if (is_execute_bp)
                write_bvr(static_slot_idx, static_orig_addr);
            else
                write_wvr(static_slot_idx, static_orig_addr);
        }

        static_is_stepping = false;
    }
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
        bp_len_kernel = HW_BREAKPOINT_LEN_8; // 执行断点必须是  字节
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
    // 这会让 perf 子系统启用正确的硬件单步步过机制
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