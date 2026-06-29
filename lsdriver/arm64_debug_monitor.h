#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/memory.h>
#include <linux/smp.h>
#include <linux/stop_machine.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <asm/debug-monitors.h>
#include <asm/insn.h>
#include <asm/virt.h>
#include "export_fun.h"
#include "arm64_reg.h"
#include "inline_hook_frame.h"

#define BP_CONFIG_MAX 16
#define ARM64_HWBKPT_ESR_ACCESS_MASK (1U << 6)

struct breakpoint_point
{
    enum hwbp_type bt;  // 断点类型
    enum hwbp_len bl;   // 断点长度
    enum hwbp_scope bs; // 断点作用线程范围
    uint64_t addr;      // 断点地址
};

struct breakpoint_config
{
    pid_t pid; // 目标进程 pid

    // 多个地址观点
    struct breakpoint_point points[BP_CONFIG_MAX];
    int hit_point_index; // 命中时的观点索引，用于传递给回调进行快速派发

    // 触发回调，命中时调用
    // regs: 命中时的寄存器现场 self: 指向本结构体自身，方便回调访问配置信息
    void (*on_hit)(struct pt_regs *regs, struct breakpoint_config *self);

    // 允许携带私有的数据
    struct hwbp_info *bp_info;
};

/*
这里用全局变量来传递异常回调和断点写入上下文
应为异常处理路径的调用约定是硬件决定的，我没办法附加参数
注册线程调度回调那个可以附加参数，但是只能附加一个参数,现在使用inline hook也无法附加参数了
既然使用全局指针传递上下文，那么<统一>使用传递的全局上下文，不在使用附带参数
内核很多子系统的做法也一样
*/
struct breakpoint_config g_bp_config[BP_CONFIG_MAX];
int num_brps, num_wrps; // 硬件执行和访问槽位总数

/*
把外部断点参数转换成ARM架构内部格式，并完成基础检测/修正。
这里只处理用户态断点（EL0）场景。
在32位的task和per-cpu 场景不能按compat处理，要=0
*/
static int hw_breakpoint_parse(struct breakpoint_point *point, bool is_compat, struct arch_hw_breakpoint *hw)
{
    uint64_t alignment_mask, offset;

    if (!point || !hw)
        return -EINVAL;

    memset(hw, 0, sizeof(*hw));

    // 类型转换：对应 arch_build_bp_info()
    switch (point->bt)
    {
    case HW_BREAKPOINT_X:
        hw->ctrl.type = ARM_BREAKPOINT_EXECUTE;
        break;
    case HW_BREAKPOINT_R:
        hw->ctrl.type = ARM_BREAKPOINT_LOAD;
        break;
    case HW_BREAKPOINT_W:
        hw->ctrl.type = ARM_BREAKPOINT_STORE;
        break;
    case HW_BREAKPOINT_RW:
        hw->ctrl.type = ARM_BREAKPOINT_LOAD | ARM_BREAKPOINT_STORE;
        break;
    default:
        return -EINVAL;
    }

    // 长度转换：对应 arch_build_bp_info()
    switch (point->bl)
    {
    case HW_BREAKPOINT_LEN_1:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_1;
        break;
    case HW_BREAKPOINT_LEN_2:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_2;
        break;
    case HW_BREAKPOINT_LEN_3:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_3;
        break;
    case HW_BREAKPOINT_LEN_4:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_4;
        break;
    case HW_BREAKPOINT_LEN_5:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_5;
        break;
    case HW_BREAKPOINT_LEN_6:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_6;
        break;
    case HW_BREAKPOINT_LEN_7:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_7;
        break;
    case HW_BREAKPOINT_LEN_8:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_8;
        break;
    default:
        return -EINVAL;
    }

    // 执行断点/观察点长度合法性检查：对应 arch_build_bp_info()
    if (hw->ctrl.type == ARM_BREAKPOINT_EXECUTE)
    {
        if (is_compat)
        {
            if (hw->ctrl.len != ARM_BREAKPOINT_LEN_2 &&
                hw->ctrl.len != ARM_BREAKPOINT_LEN_4)
                return -EINVAL;
        }
        else
        {
            // AArch64 执行断点只允许 4 字节。源码里这里不是直接报错，而是修正成 4。
            if (hw->ctrl.len != ARM_BREAKPOINT_LEN_4)
                hw->ctrl.len = ARM_BREAKPOINT_LEN_4;
        }
    }

    // 地址初始值：对应 arch_build_bp_info()
    hw->address = point->addr;

    // 权限：这里只做用户态断点
    hw->ctrl.privilege = AARCH64_BREAKPOINT_EL0;
    hw->ctrl.enabled = 1;

    // 对齐检查和修正：对应内核源码 hw_breakpoint_arch_parse()
    if (is_compat)
    {

        if (hw->ctrl.len == ARM_BREAKPOINT_LEN_8)
            alignment_mask = 0x7;
        else
            alignment_mask = 0x3;

        offset = hw->address & alignment_mask;

        switch (offset)
        {
        case 0:
            break;
        case 1:
        case 2:
            if (hw->ctrl.len == ARM_BREAKPOINT_LEN_2)
                break;
            fallthrough;
        case 3:
            if (hw->ctrl.len == ARM_BREAKPOINT_LEN_1)
                break;
            fallthrough;
        default:
            return -EINVAL;
        }
    }
    else
    {
        if (hw->ctrl.type == ARM_BREAKPOINT_EXECUTE)
            alignment_mask = 0x3;
        else
            alignment_mask = 0x7;

        offset = hw->address & alignment_mask;
    }

    // 地址向下对齐到硬件要求的边界
    hw->address &= ~alignment_mask;
    hw->ctrl.len <<= offset;

    return 0;
}

// 返回 BAS 位图中最低的有效字节位；没有有效位时返回 32。
static uint32_t hwbp_lowest_set_bit(uint32_t value)
{
    uint32_t bit;

    for (bit = 0; bit < 32; bit++)
    {
        if (value & (1U << bit))
            return bit;
    }

    return 32;
}

// 返回 BAS 位图中最高的有效字节位；没有有效位时返回 32。
static uint32_t hwbp_highest_set_bit(uint32_t value)
{
    int bit;

    for (bit = 31; bit >= 0; bit--)
    {
        if (value & (1U << bit))
            return (uint32_t)bit;
    }

    return 32;
}

// ARM64 watchpoint 可能上报 watched bytes 附近的地址；按策略计算距离。
static uint64_t ls_get_distance_from_watchpoint(uint64_t fault_addr, uint64_t watch_addr, struct arch_hw_breakpoint_ctrl *ctrl)
{
    uint64_t wp_low;
    uint64_t wp_high;
    uint32_t lens;
    uint32_t lene;

    if (!ctrl || !ctrl->len)
        return ~0ULL;

    fault_addr = untagged_addr(fault_addr);
    lens = hwbp_lowest_set_bit(ctrl->len);
    lene = hwbp_highest_set_bit(ctrl->len);
    if (lens >= 32 || lene >= 32)
        return ~0ULL;

    wp_low = watch_addr + lens;
    wp_high = watch_addr + lene;

    if (fault_addr < wp_low)
        return wp_low - fault_addr;
    if (fault_addr > wp_high)
        return fault_addr - wp_high;
    return 0;
}

// ESR bit 6 表示本次访问方向：0 为读，1 为写。
static bool watchpoint_access_matches(struct arch_hw_breakpoint *info, uint64_t esr)
{
    bool is_write;

    if (!info || info->ctrl.type == ARM_BREAKPOINT_EXECUTE)
        return false;

    is_write = !!(esr & ARM64_HWBKPT_ESR_ACCESS_MASK);
    if (is_write)
        return !!(info->ctrl.type & ARM_BREAKPOINT_STORE);

    return !!(info->ctrl.type & ARM_BREAKPOINT_LOAD);
}

// 禁用当前 CPU 上的硬件断点/观察点控制寄存器，保留原有配置位
static void clear_hwbp_regs_on_cpu(void *data)
{
    int i;
    int cfg_index;
    int point_index;
    uint32_t ctrl;
    uint32_t expected_ctrl;
    uint64_t addr;
    struct arch_hw_breakpoint info;
    bool should_disable;

    (void)data;

    for (i = 0; i < num_brps; i++)
    {
        addr = read_wb_reg(AARCH64_DBG_REG_BVR, i);
        ctrl = read_wb_reg(AARCH64_DBG_REG_BCR, i);

        if (!(ctrl & 0x1) || addr == 0)
            continue;

        should_disable = false;
        for (cfg_index = 0; cfg_index < BP_CONFIG_MAX; cfg_index++)
        {
            if (g_bp_config[cfg_index].pid <= 0 ||
                !g_bp_config[cfg_index].on_hit)
                continue;

            for (point_index = 0; point_index < BP_CONFIG_MAX; point_index++)
            {
                struct breakpoint_point *point = &g_bp_config[cfg_index].points[point_index];

                if (point->addr == 0 ||
                    point->bt != HW_BREAKPOINT_X ||
                    hw_breakpoint_parse(point, 0, &info) ||
                    info.ctrl.type != ARM_BREAKPOINT_EXECUTE ||
                    info.address != addr)
                    continue;

                expected_ctrl = encode_ctrl_reg(info.ctrl);
                if ((expected_ctrl & ~0x1) != (ctrl & ~0x1))
                    continue;

                should_disable = true;
            }
        }

        if (should_disable)
            write_wb_reg(AARCH64_DBG_REG_BCR, i, ctrl & ~0x1);
    }

    for (i = 0; i < num_wrps; i++)
    {
        addr = read_wb_reg(AARCH64_DBG_REG_WVR, i);
        ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, i);

        if (!(ctrl & 0x1) || addr == 0)
            continue;

        should_disable = false;
        for (cfg_index = 0; cfg_index < BP_CONFIG_MAX; cfg_index++)
        {
            if (g_bp_config[cfg_index].pid <= 0 ||
                !g_bp_config[cfg_index].on_hit)
                continue;

            for (point_index = 0; point_index < BP_CONFIG_MAX; point_index++)
            {
                struct breakpoint_point *point = &g_bp_config[cfg_index].points[point_index];

                if (point->addr == 0 ||
                    point->bt == HW_BREAKPOINT_X ||
                    hw_breakpoint_parse(point, 0, &info) ||
                    info.ctrl.type == ARM_BREAKPOINT_EXECUTE ||
                    info.address != addr)
                    continue;

                expected_ctrl = encode_ctrl_reg(info.ctrl);
                if ((expected_ctrl & ~0x1) != (ctrl & ~0x1))
                    continue;

                should_disable = true;
            }
        }

        if (should_disable)
            write_wb_reg(AARCH64_DBG_REG_WCR, i, ctrl & ~0x1);
    }
}

// 执行断异常处理跳板工作函数
static int work_trampoline_breakpoint(struct pt_regs *hook_regs)
{
    int i;
    int j;
    int slot;
    uint64_t addr;
    uint64_t ctrl;
    struct arch_hw_breakpoint info;
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[2];

    /*
   这里说明一下为何可以这么做进行步过
       现在代码安装断点的方式是线程被调度到cpu上就写入对应的cpu寄存器进行断点，调度走就清空控制寄存器删除断点，这样就实现了断点跟着task走
       但是呢这里的异常回调我们关闭寄存器了进行步过后，要是线程一直运行没有被调度，断点就不会被重新打开对不对!

       其实不用担心这个不会被调度问题，因为我实际测试下面这种代码
       while (1){a++;}
       这种只进行纯!算数运算!的进程才70%不会被调度走一直运行，下面有说原因
       所以一个正常的用户使用的进程,绝对不会出现这个整个进程的线程组都在无限算数运算

       一个正常进程100%会出现下面情况，这些情况都会导致被调度走，一旦线程组中有task被调度都能收到并重新安装好因步过关闭的断点
       1.当前任务主动睡眠，           不怎么出现;                             sleep() / nanosleep() / msleep()...
       2.阻塞 IO 操作，               必出现，    网络请求和系统调用和日志之类的;  printf()/ read() / recv() / send() / connect() / accept()....
       3.锁竞争会触发调度，           几乎必出现， 多线程下非常常见对资源的保护;                  std::mutex / std::shared_mutex / std::spinlock...
       4.时间片到了CFS 抢占，         必出现，     调度器的核心机制，不过要等时间片，很久才会调度
       5.高优先级任务被唤醒会触发抢占，必出现，    不过要等被抢占，不怎么会被调度
       6.硬件中断，                   必出现，    不过中断时内核可能不会运行抢占任务，不确定会不会被调度
       7.page fault 缺页，            可能出现，  访问的虚拟地址会没有对应的物理页会触发一次，因为访问了会常驻了，很久才会调度
       8.新task创建，                 不怎么出现，就创建一次长期运行
       9.图形渲染提交画面，            几乎必出现，opengl/vulkan 之类的渲染提交
       10.等等等太多了，我就只知道这一部分
       所以放心在异常回调关断步过
       */

    /*
    这里先实时读取了执行控制寄存器配置，并只修改了bit 0 enabled是否启用位
    为何不直接清空的原因就是
        用户态如果也用perf下断，原本的硬件 debug 异常入口需要控制寄存器中的len/type/privilege
        由于 BCR/WCR 被清空，原硬件 debug 异常入口无法通过 BVR/BCR 或 WVR/WCR 匹配到
        对应的 perf_event owner，也就不会执行 perf_bp_event() 和后续disable + single-step + restore 的步过状态机。
        硬件debug异常分发直接结束并返回已处理
      结果是：硬件debug异常分发结束了，但 perf子系统没有收到这次命中的信息和步过闭环，状态机推进异常就死了

    但是:你不继续执行原异常函数就不会有这个问题了，异常入口也不会上报信息给perf子系统
    自己的断点不继续执行原异常函数，就可以直接清空寄存器
    这里选择是只禁用enable位，不管是谁的断点统一继续执行原函数

    perf 子系统在调度进 CPU 安装 perf 断点配置到寄存器，会重写 BVR/WVR + BCR/WCR；
    只有异常步过和 debug_info 的临时启停，才是只改 BCR/WCR 的 enable 位
    */

    for (slot = 0; slot < num_brps; slot++)
    {
        // 获取当前cpu的指定槽位寄存器
        addr = read_wb_reg(AARCH64_DBG_REG_BVR, slot);
        ctrl = read_wb_reg(AARCH64_DBG_REG_BCR, slot);

        // 派发异常信息给不同配置
        for (i = 0; i < BP_CONFIG_MAX; i++)
        {
            if (g_bp_config[i].pid <= 0 ||
                !g_bp_config[i].on_hit)
                continue;

            // 根据不同观点派发
            for (j = 0; j < BP_CONFIG_MAX; j++)
            {
                struct breakpoint_point *point = &g_bp_config[i].points[j];

                // 地址不相等跳过
                if (hw_breakpoint_parse(point, 0, &info) ||
                    info.address != addr)
                    continue;

                /*
                上案例
                point0 配置地址 = 0x71B5654190
                point1 配置地址 = 0x71B5653A68
                point2 配置地址 = 0x71B5655590

                安装进硬件地址寄存器：
                BVR0 = 0x71B5654190
                BVR1 = 0x71B5653A68
                BVR2 = 0x71B5655590

                目标执行到：
                PC = 0x71B5655590

                CPU 触发 debug exception，真实命中的是 slot2。

                进入异常后，从 slot0 开始扫：
                slot0:addr = read_bvr(0);  // 0x71B5654190

                point0:info.address = 0x71B5654190

                if (info.address == addr)派发 point0

                bug 点：
                这里仅证明 point0 安装在 slot0，
                没证明这次异常由 slot0 触发。

                所以真实命中 slot2，但 point0 先被派发了，
                导致 point0.records 里写入了 record.pc = 0x71B5655590。
                */
                if (info.address != (regs->pc & ~0x3ULL))
                    continue;

                // 地址相等、控制码相等且当前槽位启用才派发
                if ((ctrl & 0x1) &&
                    ((encode_ctrl_reg(info.ctrl) & ~0x1ULL) == (ctrl & ~0x1ULL)) &&
                    g_bp_config[i].pid == current->tgid)
                {
                    // 传递当前观点索引
                    g_bp_config[i].hit_point_index = j;
                    g_bp_config[i].on_hit(regs, &g_bp_config[i]);
                    // 只清 enable 位，保留原有寄存器配置，继续走原异常处理链
                    write_wb_reg(AARCH64_DBG_REG_BCR, slot, ctrl & ~0x1);
                    return 0;
                }
            }
        }
    }
    return 0;
}

// 访问断异常处理跳板工作函数
static int work_trampoline_watchpoint(struct pt_regs *hook_regs)
{
    int i;
    int j;
    int slot;
    int hit_cfg_index = -1;
    int hit_point_index = -1;
    int hit_slot = -1;
    uint64_t addr;
    uint64_t ctrl;
    uint64_t fault_addr = hook_regs->regs[0];
    uint64_t esr = hook_regs->regs[1];
    uint64_t dist;
    uint64_t min_dist = ~0ULL;
    bool exact_match = false;
    struct arch_hw_breakpoint info;
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[2];

    /*
    watchpoint_handler 原型是 (addr, esr, regs)。这里用 addr 判断真实命中的访问地址，
    用 esr 判断读写方向，避免只证明 point 安装在某个 WRP 槽就误派发。
    */
    for (slot = 0; slot < num_wrps && !exact_match; slot++)
    {
        addr = read_wb_reg(AARCH64_DBG_REG_WVR, slot);
        ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, slot);

        if (!(ctrl & 0x1))
            continue;

        for (i = 0; i < BP_CONFIG_MAX && !exact_match; i++)
        {
            if (g_bp_config[i].pid <= 0 ||
                !g_bp_config[i].on_hit ||
                g_bp_config[i].pid != current->tgid)
                continue;

            for (j = 0; j < BP_CONFIG_MAX; j++)
            {
                struct breakpoint_point *point = &g_bp_config[i].points[j];

                if (hw_breakpoint_parse(point, 0, &info) ||
                    info.ctrl.type == ARM_BREAKPOINT_EXECUTE ||
                    info.address != addr ||
                    ((encode_ctrl_reg(info.ctrl) & ~0x1ULL) != (ctrl & ~0x1ULL)) ||
                    !watchpoint_access_matches(&info, esr))
                    continue;

                dist = ls_get_distance_from_watchpoint(fault_addr, addr, &info.ctrl);
                if (dist >= min_dist)
                    continue;

                min_dist = dist;
                hit_cfg_index = i;
                hit_point_index = j;
                hit_slot = slot;
                exact_match = (dist == 0);

                if (exact_match)
                    break;
            }
        }
    }

    if (hit_cfg_index < 0)
        return 0;

    g_bp_config[hit_cfg_index].hit_point_index = hit_point_index;
    g_bp_config[hit_cfg_index].on_hit(regs, &g_bp_config[hit_cfg_index]);
    // 只清 enable 位，保留原有寄存器配置，继续走原异常处理链
    write_wb_reg(AARCH64_DBG_REG_WCR, hit_slot, ctrl & ~0x1);
    return 0;
}

// 声明硬件调试异常 hook 表
static struct hook_entry g_debug_exception_hooks[] = {
    HOOK_ENTRY("breakpoint_handler", work_trampoline_breakpoint),
    HOOK_ENTRY("watchpoint_handler", work_trampoline_watchpoint),
};

// __switch_to(prev, next) 入口 hook：AArch64 参数 x0=prev, x1=next。
static int work_trampoline_switch_to(struct pt_regs *hook_regs)
{
    struct task_struct *prev = (struct task_struct *)hook_regs->regs[0];
    struct task_struct *next = (struct task_struct *)hook_regs->regs[1];
    int i;
    int next_slot = -1;
    int prev_slot = -1;

    if (!prev || !next)
        return 0;

    // 检查切入的进程pid是否在断点配置中设置
    for (i = 0; i < BP_CONFIG_MAX; i++)
    {
        // 找到了对应的配置
        if (g_bp_config[i].pid == next->tgid)
        {
            next_slot = i;
            break;
        }
    }

    // 切走的
    for (i = 0; i < BP_CONFIG_MAX; i++)
    {
        // 找到配置
        if (g_bp_config[i].pid == prev->tgid)
        {
            prev_slot = i;
            break;
        }
    }

    /*
    prev现在正在跑、准备离开 CPU 的 task
    CPU 从目标线程组切到别的线程组时，清掉当前 CPU 上残留的断点寄存器，避免断点漏到别的进程里。
    同线程组内相互切换，不要清理，下面线程组内只要有一个task切换就重复安装上断点防止被关掉
    注意了：使用fork后的子进程在内核中是单独的一个线程组也是单独的一个进程，要装断点也是给子线程组 装
    */
    if (prev_slot >= 0 && prev->tgid != next->tgid)
    {
        if (prev->pid == prev->tgid)
        {
            pr_debug("目标进程的主线程被切换走: pid=%d comm=%s cpu=%d\n", prev->pid, prev->comm, raw_smp_processor_id());
        }
        else
        {
            pr_debug("目标进程的子线程被切换走: pid=%d comm=%s cpu=%d\n", prev->pid, prev->comm, raw_smp_processor_id());
        }

        for (i = 0; i < num_brps; i++)
            write_wb_reg(AARCH64_DBG_REG_BCR, i, 0);

        for (i = 0; i < num_wrps; i++)
            write_wb_reg(AARCH64_DBG_REG_WCR, i, 0);

        if (next_slot < 0)
            // 线程组被切走cpu进行关闭OS+开启硬件调试
            disable_hardware_debug_on_cpu(NULL);
    }

    // 目标进程的线程组被切入(线程组id就是进程的pid)
    if (next_slot >= 0)
    {
        int brp_slot = 0;
        int wrp_slot = 0;

        // 线程id==线程组id就是主线程,否则子线程
        if (next->pid == next->tgid)
        {
            pr_debug("目标进程的主线程被切换进来: pid=%d comm=%s cpu=%d\n", next->pid, next->comm, raw_smp_processor_id());
        }
        else
        {
            pr_debug("目标进程的子线程被切换进来: pid=%d comm=%s cpu=%d\n", next->pid, next->comm, raw_smp_processor_id());
        }

        // task被切入到cpu进行解锁OS+开启硬件调试
        enable_hardware_debug_on_cpu(NULL);

        // 遍历所有观点分配槽位安装进cpu
        for (i = 0; i < BP_CONFIG_MAX; i++)
        {
            struct breakpoint_point *point = &g_bp_config[next_slot].points[i];
            struct arch_hw_breakpoint info;
            int reg_slot;

            // 为空的观点不设置
            if (point->addr == 0)
                continue;

            // 把观点的断点描述信息转化为arm架构内部格式
            if (hw_breakpoint_parse(point, 0, &info))
                continue;

            // 根据不同观点类型分配不同槽位
            if (info.ctrl.type == ARM_BREAKPOINT_EXECUTE)
            {
                if (brp_slot >= num_brps)
                    continue;

                reg_slot = brp_slot++;
                // 执行地址寄存器
                write_wb_reg(AARCH64_DBG_REG_BVR, reg_slot, info.address);
                // 执行控制寄存器
                //"| 0x1"表示立即生效,
                //"& ~0x1"表示写入的寄存器配置，但是禁用不生效
                //"0"给控制寄存器清0，就删除了断点
                write_wb_reg(AARCH64_DBG_REG_BCR, reg_slot, encode_ctrl_reg(info.ctrl) | 0x1);
            }
            else
            {
                if (wrp_slot >= num_wrps)
                    continue;

                reg_slot = wrp_slot++;
                // 访问地址寄存器
                write_wb_reg(AARCH64_DBG_REG_WVR, reg_slot, info.address);
                // 访问控制寄存器
                //"| 0x1"表示立即生效,
                //"& ~0x1"表示写入的寄存器配置，但是禁用不生效
                //"0"给控制寄存器清0就删除了断点
                write_wb_reg(AARCH64_DBG_REG_WCR, reg_slot, encode_ctrl_reg(info.ctrl) | 0x1);
            }
        }
    }

    return 0;
}

static struct hook_entry g_switch_to_hook[] = {
    /*
     调度切换大致顺序：
     __schedule()
       prev = current;
       next = pick_next_task(...);
       -> trace_sched_switch(..., prev, next, prev_state)
          register_trace_sched_switch() 注册的 sched_switch tracepoint 回调在这里执行
       -> context_switch(rq, prev, next, &rf)
          -> prepare_task_switch(rq, prev, next)
          -> arch_start_context_switch(prev)
          -> switch_mm_irqs_off(..., next)
          -> prepare_lock_switch(rq, next, rf)
          -> switch_to(prev, next, prev)
             -> __switch_to(prev, next)
                -> fpsimd_thread_switch(next)
                -> tls_thread_switch(next)
                -> hw_breakpoint_thread_switch(next)
                -> contextidr_thread_switch(next)
                -> entry_task_switch(next)
                -> cpu_switch_to(prev, next)
          -> finish_task_switch(prev)

     不管是之前使用 register_trace_sched_switch() 注册 sched_switch tracepoint 回调，
     还是现在 hook __switch_to，写寄存器安装断点的位置都在 hw_breakpoint_thread_switch(next)之前，
     目标程序自己给自己下断点后面就会被它会被perf系统覆盖掉。
     不过实测这样引入了一个全新的完美机制,注意注意注意！！！！！！！！！，这是我的猜测实际情况过于复杂，但是真的不知道具体为何自己的能命中，perf的一样的命中，2个都能正常命中
     先解释一下内核perf子系统的硬件断点2种情况:
        情况1按task : 硬件断点想做到跟着某个 task 走原理(也就是对用户态断点)
            内核必须有一个地方长期保存这个 task 拥有哪些 perf_event 事件。这个地方就是 task 的 perf_event_context链表
            硬件寄存器只是当前 CPU 上的瞬时编程状态，task没有长期拥有关系，task 跑到哪个 CPU，就把它需要的断点装到那个 CPU 的寄存器里。
            如果有 8 个 CPU、每个 CPU 有 6 个执行断点寄存器，那全机一共有 48 个执行断点槽位，但每个 CPU 同时最多只能生效 6 个。
            ,
            不管用户态调用ptrace或__NR_perf_event_open,还是内核态直接使用register_hw_breakpoint这个API进行注册硬件断点
            最终走的都是perf_event_create_kernel_counter，这个函数本质上是在向 perf 子系统注册一个 perf_event
            本质上是在向 perf 子系统注册一个 perf_event，如果这个 event 是硬件断点类型，
            就会把这个硬件断点纳入它的完整生命周期管理体系，但因为它是 PERF_TYPE_BREAKPOINT 类型，会走专门的perf_breakpoint PMU 分支，有自己的特殊处理逻辑。
            对于 task 绑定的断点，最终这个 event 会挂到目标 task 的 perf_event_context 上
            当调度切到某个 task 时：perf子系统 根据它的 perf_event_context 把相关 perf_event 调度进来, 并在当前 CPU 上编程对应的硬件断点寄存器；
            当 task 切走时：再把这些 perf_event 调度出去，并卸载/禁用当前 CPU 上对应的硬件断点寄存器状态。
        情况2：硬件断点直接安装到cpu（内核层断点）
            很简单了，由于不需要跟着task走就不需要一系列的复杂调度机制，直接编程指定的地址进指定cpu的调试地址寄存器，
            比如0x7000000000编程进cpu7
            调度到这个cpu7上所有的task，只要跑过这个0x7000000000，都会直接被命中,不区分是那个进程的虚拟地址空间

    好了解释完perf子系统的规则，能发现有一个关键的机制，就是跟随task的硬件断点需要知道task被调度到cpu上
    这个规则是实现全新的完美机制的关键
    既然需要知道task被调度到cpu上perf是怎么知道的呢，就是调度切换中调用的hw_breakpoint_thread_switch，用于通知perf
    然后我们hook的线程调度是__switch_to，比hw_breakpoint_thread_switch先调用，
    我们就先收到通知安装断点命中后，
    perf后收到通知在安装命中
    这样这种我们自己的断点不仅可以命中，目标进程使用perf的硬件断点一样命中，
    打收到调度的时间差实现互不干扰，无法槽位检测，命中记录检测，命中寄存器现场检测，因为用户态硬件断点都是依赖perf,我们先运行，perf后运行，2者互不干扰
    */
    HOOK_ENTRY("__switch_to", work_trampoline_switch_to),
};

// 注册线程切换回调，开始监听
static int start_task_run_monitor(struct breakpoint_config bp_config)
{
    int ret;
    int i;
    int slot = -1;
    int existing_slot = -1;

    if (bp_config.pid <= 0)
    {
        pr_debug("pid error\n");
        return -EINVAL;
    }

    // 同pid只更新配置
    for (i = 0; i < BP_CONFIG_MAX; i++)
    {
        if (g_bp_config[i].pid == bp_config.pid)
        {
            existing_slot = i;
            break;
        }
    }

    if (existing_slot >= 0)
    {
        g_bp_config[existing_slot] = bp_config;
        pr_debug("monitor config updated, target tgid=%d slot=%d\n", bp_config.pid, existing_slot);
        return 0;
    }

    // 不同pid找槽位添加
    for (i = 0; i < BP_CONFIG_MAX; i++)
    {
        if (g_bp_config[i].pid == 0)
        {
            slot = i;
            break;
        }
    }

    if (slot < 0)
    {
        pr_debug("breakpoint config slots full\n");
        return -ENOSPC;
    }

    // 传递上下文给全局数组，让异常处理和断点写入都能互相传递配置信息
    g_bp_config[slot] = bp_config;

    // 总数也是只获取一次。
    num_brps = get_brps_num();
    num_wrps = get_wrps_num();

    // 安装inline hook接管异常
    ret = inline_hook_install(g_debug_exception_hooks);
    if (ret)
    {
        pr_debug("inline_hook_install debug exception hooks failed: %d\n", ret);
        memset(&g_bp_config[slot], 0, sizeof(g_bp_config[slot]));
        return ret;
    }

    // 安装线程切换 inline hook
    ret = inline_hook_install(g_switch_to_hook);
    if (ret)
    {
        pr_debug("inline_hook_install __switch_to failed: %d\n", ret);
        memset(&g_bp_config[slot], 0, sizeof(g_bp_config[slot]));
        inline_hook_remove(g_debug_exception_hooks);
        return ret;
    }
    pr_debug("task switch hook installed: __switch_to\n");
    pr_debug("monitor start, target tgid=%d slot=%d\n", bp_config.pid, slot);
    return 0;
}

// 注销回调，取消监听
static void stop_task_run_monitor(struct breakpoint_config bp_config)
{
    int i;
    int cpu;
    bool has_config = false;

    if (bp_config.pid <= 0)
        return;

    // 遍历所有在线 CPU，清理寄存器
    for_each_online_cpu(cpu)
        smp_call_function_single(cpu, clear_hwbp_regs_on_cpu, NULL, 1);

    // 清空指定pid的槽位配置
    for (i = 0; i < BP_CONFIG_MAX; i++)
    {
        if (g_bp_config[i].pid == bp_config.pid)
        {
            __builtin_memset(&g_bp_config[i], 0, sizeof(g_bp_config[i]));
            pr_debug("monitor config removed, target tgid=%d slot=%d\n", bp_config.pid, i);
            break;
        }
    }

    // 如果数组里还有别的断点配置，说明 monitor 仍然有人在用，就只删除当前槽位，不卸载 hook
    for (i = 0; i < BP_CONFIG_MAX; i++)
    {
        if (g_bp_config[i].pid != 0)
        {
            has_config = true;
            break;
        }
    }

    if (has_config)
        return;

    pr_debug("monitor stop\n");
    inline_hook_remove(g_switch_to_hook);
    inline_hook_remove(g_debug_exception_hooks);
}

// //下面不用看了，是单步异常步过的内核api，我不使用了，上面异常回调直接关寄存器
// static struct step_hook hwbp_step_hook;
// static void (*fn_user_enable_single_step)(struct task_struct *task);
// static void (*fn_user_disable_single_step)(struct task_struct *task);
// static void (*fn_register_user_step_hook)(struct step_hook *hook);
// static void (*fn_unregister_user_step_hook)(struct step_hook *hook);

// // 命中断点后临时关闭当前控制寄存器，并开启单步执行一条指令
// static void hwbp_begin_step(struct breakpoint_config *cfg, int type, struct pt_regs *regs)
// {
//     uint32_t ctrl;

//     // 只处理用户态命中
//     if (!cfg || !user_mode(regs))
//         return;

//     if (type == 1)
//     {
//         // 清空控制寄存器
//         ctrl = read_wb_reg(AARCH64_DBG_REG_BCR, 0);
//         write_wb_reg(AARCH64_DBG_REG_BCR, 0, 0);
//     }
//     else
//     {
//         // 清空控制寄存器
//         ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, 0);
//         write_wb_reg(AARCH64_DBG_REG_WCR, 0, 0);
//     }

//     // 记录恢复所需状态，single-step 异常回来时使用
//     cfg->suspended_step = 1;
//     cfg->suspended_type = type;
//     cfg->suspended_ctrl = ctrl;
//     cfg->suspended_task = current;

//     // 开启用户态单步，返回用户态执行一条指令后会进入 hwbp_user_step_handler
//     fn_user_enable_single_step(current);
// }

// // 单步异常回调：恢复刚才临时关闭的 BCR/WCR
// static int hwbp_user_step_handler(struct pt_regs *regs, unsigned long esr)
// {
//     struct breakpoint_config *cfg = g_bp_config;

//     (void)esr;

//     // 不是我们发起的单步就交给系统原有处理链
//     if (!cfg || !cfg->suspended_step || cfg->suspended_task != current)
//         return DBG_HOOK_ERROR;

//     if (cfg->suspended_type == 1)
//         // 恢复执行断点
//         write_wb_reg(AARCH64_DBG_REG_BCR, 0, cfg->suspended_ctrl);
//     else if (cfg->suspended_type == 2)
//         // 恢复访问断点
//         write_wb_reg(AARCH64_DBG_REG_WCR, 0, cfg->suspended_ctrl);

//     // 清理步过状态
//     cfg->suspended_step = 0;
//     cfg->suspended_type = 0;
//     cfg->suspended_ctrl = 0;
//     cfg->suspended_task = NULL;

//     // 关闭本次单步，告诉异常分发器这次 single-step 已经处理
//     fn_user_disable_single_step(current);
//     return DBG_HOOK_HANDLED;
// }

// // 安装用户态 single-step hook，用来在步过后恢复断点
// static int hwbp_step_hook_install(void)
// {
//     // user_enable_single_step:返回用户态后，执行下一条指令之后触发一次硬件单步异常。
//     fn_user_enable_single_step = (void *)generic_kallsyms_lookup_name("user_enable_single_step");
//     fn_user_disable_single_step = (void *)generic_kallsyms_lookup_name("user_disable_single_step");
//     fn_register_user_step_hook = (void *)generic_kallsyms_lookup_name("register_user_step_hook");
//     fn_unregister_user_step_hook = (void *)generic_kallsyms_lookup_name("unregister_user_step_hook");

//     if (!fn_user_enable_single_step || !fn_user_disable_single_step ||
//         !fn_register_user_step_hook || !fn_unregister_user_step_hook)
//     {
//         pr_debug("[driver] cannot find single-step symbols\n");
//         return -ENOENT;
//     }

//     // 注册到 arm64 debug-monitors 的用户态单步回调链
//     memset(&hwbp_step_hook, 0, sizeof(hwbp_step_hook));
//     hwbp_step_hook.fn = (void *)hwbp_user_step_handler;
//     fn_register_user_step_hook(&hwbp_step_hook);

//     return 0;
// }
// static void hwbp_step_hook_remove(void)
// {
//     fn_unregister_user_step_hook(&hwbp_step_hook);
// }
