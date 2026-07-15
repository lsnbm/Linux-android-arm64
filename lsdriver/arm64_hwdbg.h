#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/memory.h>
#include <linux/percpu.h>
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
#include "lsdriver_log.h"
#include "io_struct.h"
#include "emulate_insn.h"

#define ARM64_HWBKPT_ESR_ACCESS_MASK (1U << 6)

/*
这里用全局变量来传递异常回调和断点写入上下文
应为异常处理路径的调用约定是硬件决定的，我没办法附加参数
注册线程调度回调那个可以附加参数，但是只能附加一个参数,现在使用inline hook也无法附加参数了
既然使用全局指针传递上下文，那么<统一>使用传递的全局上下文，不在使用附带参数
内核很多子系统的做法也一样
*/
struct break_point *g_bp_info;
int num_brps, num_wrps; // 硬件执行和访问槽位总数
static struct perf_event * __percpu * bp_on_reg;
static struct perf_event * __percpu * wp_on_reg;
static void (*fn_perf_bp_event)(struct perf_event *event, void *data);

// 判断单个断点点位是否具备安装和派发条件。
static bool hwbp_point_is_active(struct bp_point *point)
{
    return point && point->hit_addr != 0 && point->on_hit;
}

// 判断一个 break_point 配置中是否至少存在一个有效点位。
static bool hwbp_info_has_active_point(struct break_point *info)
{
    int point_slot;

    if (!info || info->pid <= 0) return false;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        if (hwbp_point_is_active(&info->points[point_slot])) return true;
    }

    return false;
}

// 判断一个 break_point 配置中是否包含指定线程组 pid 的有效点位。
static bool hwbp_info_has_pid(struct break_point *info, pid_t pid)
{
    if (!info || pid <= 0 || info->pid != pid) return false;

    return hwbp_info_has_active_point(info);
}

/*
把外部断点参数转换成ARM架构内部格式，并完成基础检测/修正。
这里只处理用户态断点（EL0）场景。
在32位的task和per-cpu 场景不能按compat处理，要=0
*/
static int hw_breakpoint_parse(struct bp_point *point, bool is_compat, struct arch_hw_breakpoint *hw)
{
    uint64_t alignment_mask, offset;

    if (!point || !hw) return -EINVAL;

    memset(hw, 0, sizeof(*hw));

    // 类型转换：对应 arch_build_bp_info()
    switch (point->bt)
    {
    case BP_BREAKPOINT_X:
        hw->ctrl.type = ARM_BREAKPOINT_EXECUTE;
        break;
    case BP_BREAKPOINT_R:
        hw->ctrl.type = ARM_BREAKPOINT_LOAD;
        break;
    case BP_BREAKPOINT_W:
        hw->ctrl.type = ARM_BREAKPOINT_STORE;
        break;
    case BP_BREAKPOINT_RW:
        hw->ctrl.type = ARM_BREAKPOINT_LOAD | ARM_BREAKPOINT_STORE;
        break;
    default:
        return -EINVAL;
    }

    // 长度转换：对应 arch_build_bp_info()
    switch (point->bl)
    {
    case BP_BREAKPOINT_LEN_1:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_1;
        break;
    case BP_BREAKPOINT_LEN_2:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_2;
        break;
    case BP_BREAKPOINT_LEN_3:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_3;
        break;
    case BP_BREAKPOINT_LEN_4:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_4;
        break;
    case BP_BREAKPOINT_LEN_5:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_5;
        break;
    case BP_BREAKPOINT_LEN_6:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_6;
        break;
    case BP_BREAKPOINT_LEN_7:
        hw->ctrl.len = ARM_BREAKPOINT_LEN_7;
        break;
    case BP_BREAKPOINT_LEN_8:
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
            if (hw->ctrl.len != ARM_BREAKPOINT_LEN_2 && hw->ctrl.len != ARM_BREAKPOINT_LEN_4) return -EINVAL;
        }
        else
        {
            // AArch64 执行断点只允许 4 字节。源码里这里不是直接报错，而是修正成 4。
            if (hw->ctrl.len != ARM_BREAKPOINT_LEN_4) hw->ctrl.len = ARM_BREAKPOINT_LEN_4;
        }
    }

    // 地址初始值：对应 arch_build_bp_info()
    hw->address = point->hit_addr;

    // 权限：这里只做用户态断点
    hw->ctrl.privilege = AARCH64_BREAKPOINT_EL0;
    hw->ctrl.enabled = 1;

    // 对齐检查和修正：对应内核源码 hw_breakpoint_arch_parse()
    if (is_compat)
    {

        if (hw->ctrl.len == ARM_BREAKPOINT_LEN_8) alignment_mask = 0x7;
        else alignment_mask = 0x3;

        offset = hw->address & alignment_mask;

        switch (offset)
        {
        case 0:
            break;
        case 1:
        case 2:
            if (hw->ctrl.len == ARM_BREAKPOINT_LEN_2) break;
            fallthrough;
        case 3:
            if (hw->ctrl.len == ARM_BREAKPOINT_LEN_1) break;
            fallthrough;
        default:
            return -EINVAL;
        }
    }
    else
    {
        if (hw->ctrl.type == ARM_BREAKPOINT_EXECUTE) alignment_mask = 0x3;
        else alignment_mask = 0x7;

        offset = hw->address & alignment_mask;
    }

    // 地址向下对齐到硬件要求的边界
    hw->address &= ~alignment_mask;
    hw->ctrl.len <<= offset;

    return 0;
}

// ARM64 watchpoint 可能上报 watched bytes 附近的地址；按策略计算距离。
static uint64_t get_distance_from_watchpoint(uint64_t fault_addr, uint64_t watch_addr, struct arch_hw_breakpoint_ctrl *ctrl)
{
    uint64_t wp_low;
    uint64_t wp_high;
    uint32_t lens;
    uint32_t lene;

    if (!ctrl || !ctrl->len) return ~0ULL;

    fault_addr = untagged_addr(fault_addr);
    lens = lowest_set_bit32(ctrl->len);
    lene = highest_set_bit32(ctrl->len);
    if (lens >= 32 || lene >= 32) return ~0ULL;

    wp_low = watch_addr + lens;
    wp_high = watch_addr + lene;

    if (fault_addr < wp_low) return wp_low - fault_addr;
    if (fault_addr > wp_high) return fault_addr - wp_high;
    return 0;
}

// ESR bit 6 表示本次访问方向：0 为读，1 为写。
static bool watchpoint_access_matches(struct arch_hw_breakpoint *info, uint64_t esr)
{
    bool is_write;

    if (!info || info->ctrl.type == ARM_BREAKPOINT_EXECUTE) return false;

    is_write = !!(esr & ARM64_HWBKPT_ESR_ACCESS_MASK);
    if (is_write) return !!(info->ctrl.type & ARM_BREAKPOINT_STORE);

    return !!(info->ctrl.type & ARM_BREAKPOINT_LOAD);
}

// 执行断异常处理跳板工作函数
static int work_trampoline_breakpoint(struct pt_regs *hook_regs)
{
    int j;
    int slot;
    uint64_t addr;
    uint64_t ctrl;
    struct arch_hw_breakpoint info;
    struct break_point *bp_info = g_bp_info;
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[2];
    struct perf_event **slots;
    struct perf_event *bp;

    if (!bp_info) return 0;

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

        // 根据不同观点派发
        for (j = 0; j < BP_CONFIG_MAX; j++)
        {
            struct bp_point *point = &bp_info->points[j];

            // 地址不相等跳过
            if (!hwbp_point_is_active(point) || bp_info->pid != current->tgid || hw_breakpoint_parse(point, 0, &info) || info.address != addr) continue;

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
            if (info.address != (regs->pc & ~0x3ULL)) continue;

            // 地址相等、控制码相等且当前槽位启用才派发
            if ((ctrl & 0x1) && ((encode_ctrl_reg(info.ctrl) & ~0x1ULL) == (ctrl & ~0x1ULL)) && bp_info->pid == current->tgid)
            {

                point->on_hit((void *)regs, (void *)point);
                // 模拟指令步过,失败走禁用进行步过
                if (!emulate_insn(regs, NULL, NULL))
                {
                    // 只清 enable 位，保留原有寄存器配置，继续走原异常处理链
                    write_wb_reg(AARCH64_DBG_REG_BCR, slot, ctrl & ~0x1);
                }

                /*
                自己的命中了就说明把这个槽位占了，其他使用perf使用槽位要进行补
                命中自己的执行断点后不继续跑原 breakpoint_handler：并手动补发当前槽位给 perf。
                只补发当前槽位，遍历全部 slots 会把其他 perf 断点重复计数。
                */
                // slots = this_cpu_ptr(bp_on_reg);
                // if (slot >= 0 && slot < num_brps)
                // {
                //     bp = READ_ONCE(slots[slot]);
                //     if (bp)
                //         fn_perf_bp_event(bp, regs);
                // }
                // hook_regs->regs[0] = 0;// 给 breakpoint_handler返回 0，表示已处理异常
                // return 1;     // 给hook框架返回1，表示跳过原函数
                return 0;
            }
        }
    }
    return 0;
}

// 访问断异常处理跳板工作函数
static int work_trampoline_watchpoint(struct pt_regs *hook_regs)
{
    int j;
    int slot;
    int hit_slot = -1;
    uint64_t addr;
    uint64_t ctrl;
    uint64_t hit_ctrl = 0;
    uint64_t fault_addr = hook_regs->regs[0];
    uint64_t esr = hook_regs->regs[1];
    uint64_t dist;
    bool exact_match = false;
    struct arch_hw_breakpoint info;
    struct break_point *bp_info = g_bp_info;
    struct bp_point *hit_point = NULL;
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[2];
    struct perf_event **slots;
    struct perf_event *bp;

    if (!bp_info) return 0;

    /*
    watchpoint_handler 原型是 (addr, esr, regs)。这里用 addr 判断真实命中的访问地址，
    用 esr 判断读写方向，避免只证明 point 安装在某个 WRP 槽就误派发。
    */
    for (slot = 0; slot < num_wrps && !exact_match; slot++)
    {
        addr = read_wb_reg(AARCH64_DBG_REG_WVR, slot);
        ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, slot);

        if (!(ctrl & 0x1)) continue;

        for (j = 0; j < BP_CONFIG_MAX; j++)
        {
            struct bp_point *point = &bp_info->points[j];

            if (!hwbp_point_is_active(point) || bp_info->pid != current->tgid || hw_breakpoint_parse(point, 0, &info) || info.ctrl.type == ARM_BREAKPOINT_EXECUTE || info.address != addr || ((encode_ctrl_reg(info.ctrl) & ~0x1ULL) != (ctrl & ~0x1ULL)) || !watchpoint_access_matches(&info, esr)) continue;

            /*
            内核 perf 可以在没有精确命中时选择最近 watchpoint 兜底；
            这里做自定义断点计数，非精确命中会把相邻访问归到第一个点位，必须跳过。
            */
            dist = get_distance_from_watchpoint(fault_addr, addr, &info.ctrl);
            if (dist != 0) continue;

            hit_point = point;
            hit_slot = slot;
            hit_ctrl = ctrl;
            exact_match = true;

            break;
        }
    }

    if (!hit_point) return 0;

    hit_point->on_hit((void *)regs, (void *)hit_point);

    // 模拟指令步过,失败走禁用进行步过
    {
        if (!emulate_insn(regs, NULL, NULL))
        {
            // 只清 enable 位，保留原有寄存器配置，继续走原异常处理链
            write_wb_reg(AARCH64_DBG_REG_WCR, hit_slot, hit_ctrl & ~0x1);
        }
    }

    // slots = this_cpu_ptr(wp_on_reg);
    // if (hit_slot >= 0 && hit_slot < num_wrps)
    // {
    //     bp = READ_ONCE(slots[hit_slot]);
    //     if (bp)
    //         fn_perf_bp_event(bp, regs);
    // }
    // hook_regs->regs[0] = 0;
    // return 1;
    return 0;
}

// 声明硬件调试异常 hook 表
static struct hook_entry g_debug_exception_hooks[] = {
    HOOK_ENTRY("breakpoint_handler", work_trampoline_breakpoint),
    HOOK_ENTRY("watchpoint_handler", work_trampoline_watchpoint),
};

// 返回地址hook 跳板
static void __attribute__((used, __noinline__)) ret_work_finish_task_switch(void);
__attribute__((naked, used)) void ret_trampoline_finish_task_switch(void)
{
    asm volatile("str x0, [sp, #8]\n"
                 "bl ret_work_finish_task_switch\n"
                 "ldp x16, x0, [sp], #304\n"
                 "ret x16\n");
}

// 在当前 CPU 上安装硬件断点/观察点寄存器。
static void install_hwbp_regs_on_cpu(struct break_point *bp_info)
{
    int brp_slot = 0;
    int wrp_slot = 0;
    int j;

    if (!bp_info) return;

    for (j = 0; j < BP_CONFIG_MAX; j++)
    {
        struct bp_point *point = &bp_info->points[j];
        struct arch_hw_breakpoint info;
        int reg_slot;

        if (!hwbp_point_is_active(point)) continue;

        if (hw_breakpoint_parse(point, 0, &info)) continue;

        if (info.ctrl.type == ARM_BREAKPOINT_EXECUTE)
        {
            if (brp_slot >= num_brps) continue;

            reg_slot = brp_slot++;
            write_wb_reg(AARCH64_DBG_REG_BVR, reg_slot, info.address);
            write_wb_reg(AARCH64_DBG_REG_BCR, reg_slot, encode_ctrl_reg(info.ctrl) | 0x1);
        }
        else
        {
            if (wrp_slot >= num_wrps) continue;

            reg_slot = wrp_slot++;
            write_wb_reg(AARCH64_DBG_REG_WVR, reg_slot, info.address);
            write_wb_reg(AARCH64_DBG_REG_WCR, reg_slot, encode_ctrl_reg(info.ctrl) | 0x1);
        }
    }
}

// 禁用当前 CPU 上的硬件断点/观察点控制寄存器，保留原有配置位
static void clear_hwbp_regs_on_cpu(void *data)
{
    int brp_slot = 0;
    int wrp_slot = 0;
    int point_slot;
    uint32_t ctrl;
    uint32_t expected_ctrl;
    uint64_t addr;
    struct arch_hw_breakpoint info;
    struct break_point *bp_info = g_bp_info;

    (void)data;

    if (!bp_info) return;

    for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
    {
        struct bp_point *point = &bp_info->points[point_slot];

        if (!hwbp_point_is_active(point) || hw_breakpoint_parse(point, 0, &info)) continue;

        expected_ctrl = encode_ctrl_reg(info.ctrl);
        if (info.ctrl.type == ARM_BREAKPOINT_EXECUTE)
        {
            if (brp_slot >= num_brps) continue;

            addr = read_wb_reg(AARCH64_DBG_REG_BVR, brp_slot);
            ctrl = read_wb_reg(AARCH64_DBG_REG_BCR, brp_slot);

            if ((ctrl & 0x1) && info.address == addr && ((expected_ctrl & ~0x1) == (ctrl & ~0x1)))
            {
                write_wb_reg(AARCH64_DBG_REG_BCR, brp_slot, ctrl & ~0x1);
            }

            brp_slot++;
        }
        else
        {
            if (wrp_slot >= num_wrps) continue;

            addr = read_wb_reg(AARCH64_DBG_REG_WVR, wrp_slot);
            ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, wrp_slot);

            if ((ctrl & 0x1) && info.address == addr && ((expected_ctrl & ~0x1) == (ctrl & ~0x1)))
            {
                write_wb_reg(AARCH64_DBG_REG_WCR, wrp_slot, ctrl & ~0x1);
            }

            wrp_slot++;
        }
    }
}

static void __attribute__((used, __noinline__)) ret_work_finish_task_switch(void)
{
    struct break_point *bp_info = g_bp_info;

    if (bp_info)
    {
        if (hwbp_info_has_pid(bp_info, current->tgid))
        {
            if (current->pid == current->tgid)
            {
                ls_log_tag("hwbp", "目标进程的主线程被切换进来: pid=%d comm=%s cpu=%d\n", current->pid, current->comm, raw_smp_processor_id());
            }
            else
            {
                ls_log_tag("hwbp", "目标进程的子线程被切换进来: pid=%d comm=%s cpu=%d\n", current->pid, current->comm, raw_smp_processor_id());
            }

            enable_hardware_debug_on_cpu(NULL);
            install_hwbp_regs_on_cpu(bp_info);
        }
        else
        {
            clear_hwbp_regs_on_cpu(NULL);
            // disable_hardware_debug_on_cpu(NULL);
        }
    }
}

// finish_task_switch(prev) 入口 hook：函数返回后再覆盖 perf 写入的硬件断点寄存器。
static int work_trampoline_finish_task_switch(struct pt_regs *hook_regs)
{
    if (!g_bp_info) return 0;

    *(unsigned long *)(hook_regs->sp = (unsigned long)hook_frame_metadata(hook_regs)) = hook_regs->regs[30];
    hook_regs->regs[30] = (unsigned long)ret_trampoline_finish_task_switch;

    return 0;
}

static struct hook_entry g_finish_task_switch_ret_hooks[] = {
    /*
      __schedule() 调度切换层级：

      __schedule()
        prev = current;
        next = pick_next_task(...);

        if (prev != next) {
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
                   -> hw_breakpoint_thread_switch(next) // 硬件断点 perf 收到线程切换
                   -> contextidr_thread_switch(next)
                   -> entry_task_switch(next)
                   -> cpu_switch_to(prev, next)

             -> finish_task_switch(prev)
                -> vtime_task_switch(prev)
                -> perf_event_task_sched_in(prev, current)
                -> finish_task(prev)

                5.10：
                  -> finish_lock_switch(rq)
                  -> finish_arch_post_lock_switch()
                  -> kcov_finish_switch(current)
                  -> fire_sched_in_preempt_notifiers(current)
                  -> tick_nohz_task_switch()

                5.15 / 6.1 / 6.6 / 6.12：
                  -> tick_nohz_task_switch()
                  -> finish_lock_switch(rq)
                     -> __balance_callbacks(rq)
                  -> finish_arch_post_lock_switch()
                  -> kcov_finish_switch(current)
                  -> fire_sched_in_preempt_notifiers(current)
        } else {
          5.10：
            -> rq_unlock_irq(rq, &rf)

          5.15 / 6.1 / 6.6 / 6.12：
            -> rq_unpin_lock(rq, &rf)
            -> __balance_callbacks(rq)
           -> raw_spin_rq_unlock_irq(rq)
       }

       5.10：
         -> balance_callback(rq)

     不管是之前使用 register_trace_sched_switch() 注册 sched_switch tracepoint 回调，
      还是 hook __switch_to / cpu_switch_to 入口，写寄存器安装断点的位置都在
      finish_task_switch(prev) -> perf_event_task_sched_in(prev, current) 之前，
      后面的 perf_event_task_sched_in(prev, current) 仍然可能再次覆盖这里写入的硬件断点寄存器。
      如果要在 perf 调度后覆盖回去，5.15+ 可以考虑 __balance_callbacks(rq)；
      5.10 没有同样通用的普通函数入口，更稳的是 finish_task_switch 返回 hook。
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

   */
    HOOK_ENTRY("finish_task_switch", work_trampoline_finish_task_switch),
};

// 安装硬件调试异常 hook 和 finish_task_switch return hook，开始监听
static int start_task_run_monitor(struct break_point *bp_info)
{
    int ret;

    if (!bp_info || !hwbp_info_has_active_point(bp_info))
    {
        ls_log_tag("hwbp", "breakpoint info error\n");
        return -EINVAL;
    }

    if (g_bp_info)
    {
        g_bp_info = bp_info;
        num_brps = get_brps_num();
        num_wrps = get_wrps_num();
        bp_info->num_brps = num_brps;
        bp_info->num_wrps = num_wrps;
        ls_log_tag("hwbp", "monitor config updated\n");
        return 0;
    }

    // 传递上下文给全局指针，让异常处理和断点写入都能互相传递配置信息
    g_bp_info = bp_info;

    // 总数也是只获取一次。
    num_brps = get_brps_num();
    num_wrps = get_wrps_num();
    bp_on_reg = (struct perf_event * __percpu *)generic_kallsyms_lookup_name("bp_on_reg");
    wp_on_reg = (struct perf_event * __percpu *)generic_kallsyms_lookup_name("wp_on_reg");
    fn_perf_bp_event = (void (*)(struct perf_event *, void *))generic_kallsyms_lookup_name("perf_bp_event");
    if (!bp_on_reg || !wp_on_reg || !fn_perf_bp_event)
    {
        ls_log_tag("hwbp", "lookup bp_on_reg/wp_on_reg/perf_bp_event failed\n");
        g_bp_info = NULL;
        return -ENOENT;
    }
    bp_info->num_brps = num_brps;
    bp_info->num_wrps = num_wrps;

    // 安装inline hook接管异常
    ret = inline_hook_install(g_debug_exception_hooks);
    if (ret)
    {
        ls_log_tag("hwbp", "inline_hook_install debug exception hooks failed: %d\n", ret);
        g_bp_info = NULL;
        return ret;
    }

    // 安装 finish_task_switch return hook
    ret = inline_hook_install(g_finish_task_switch_ret_hooks);
    if (ret)
    {
        ls_log_tag("hwbp", "inline_hook_install finish_task_switch return hook failed: %d\n", ret);
        g_bp_info = NULL;
        inline_hook_remove(g_debug_exception_hooks);
        return ret;
    }
    ls_log_tag("hwbp", "finish_task_switch return hook installed\n");
    ls_log_tag("hwbp", "monitor start\n");
    return 0;
}

// 注销 hook，取消监听
static void stop_task_run_monitor(void)
{
    int cpu;

    if (!g_bp_info) return;

    // 遍历所有在线 CPU，清理寄存器
    for_each_online_cpu(cpu) smp_call_function_single(cpu, clear_hwbp_regs_on_cpu, NULL, 1);

    g_bp_info = NULL;
    ls_log_tag("hwbp", "monitor config removed\n");

    ls_log_tag("hwbp", "monitor stop\n");
    inline_hook_remove(g_finish_task_switch_ret_hooks);
    inline_hook_remove(g_debug_exception_hooks);
}
