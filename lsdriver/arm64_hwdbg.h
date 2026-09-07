#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/memory.h>
#include <linux/percpu.h>
#include <linux/perf_event.h>
#include <linux/cpu.h>
#include <linux/rcupdate.h>
#include <linux/smp.h>
#include <linux/stop_machine.h>
#include <linux/version.h>
#include <linux/sched.h>
#include <asm/cacheflush.h>
#include <asm/debug-monitors.h>
#include <asm/esr.h>
#include <asm/hw_breakpoint.h>
#include <asm/insn.h>
#include <asm/virt.h>
#include "export_fun.h"
#include "arm64_reg.h"
#include "inline_hook_frame.h"
#include "lsdriver_log.h"
#include "io_struct.h"
#include "arm64_emulate/emulate_inst.h"

/*
DBGBVRn_EL1：第 n 个执行断点地址寄存器
DBGBCRn_EL1：第 n 个执行断点控制寄存器
DBGWVRn_EL1：第 n 个数据观察点地址寄存器
DBGWCRn_EL1：第 n 个数据观察点控制寄存器

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

/*
把外部断点参数转换成ARM架构内部格式，并完成基础检测/修正。
这里只处理用户态断点（EL0）场景。
在32位的task和per-cpu 场景不能按compat处理，要=0
*/
static int hw_breakpoint_parse(struct bp_point *point, bool is_compat, struct arch_hw_breakpoint *hw)
{
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

    // 长度转换：对应 arch_build_bp_info()（精确映射，防止枚举不连续）
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
    uint64_t alignment_mask;
    uint64_t offset;
    if (is_compat)
    {
        alignment_mask = (hw->ctrl.len == ARM_BREAKPOINT_LEN_8) ? 0x7 : 0x3;
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
        alignment_mask = (hw->ctrl.type == ARM_BREAKPOINT_EXECUTE) ? 0x3 : 0x7;
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
    if (!ctrl || !ctrl->len) return ~0ULL;

    fault_addr = untagged_addr(fault_addr);
    uint32_t lens = __ffs(ctrl->len);
    uint32_t lene = __fls(ctrl->len);

    uint64_t wp_low = watch_addr + lens;
    uint64_t wp_high = watch_addr + lene;

    if (fault_addr < wp_low) return wp_low - fault_addr;
    if (fault_addr > wp_high) return fault_addr - wp_high;
    return 0;
}

// ESR bit 6 表示本次访问方向：0 为读，1 为写。
static bool watchpoint_access_matches(struct arch_hw_breakpoint *info, uint64_t esr)
{
    if (!info || info->ctrl.type == ARM_BREAKPOINT_EXECUTE) return false;

    bool is_write = !!(esr & ESR_ELx_WNR);
    if (is_write) return !!(info->ctrl.type & ARM_BREAKPOINT_STORE);

    return !!(info->ctrl.type & ARM_BREAKPOINT_LOAD);
}

// 原生 ARM64 perf 断点通过 overflow_handler 是否为默认 perf 回调决定是否单步。
// 默认回调表示异常处理后仍需执行被断住的原指令；trigger 只记录命中地址，不能作为单步标志。
static bool perf_breakpoint_requires_step(struct perf_event *event)
{
    static void (*default_forward)(struct perf_event *, struct perf_sample_data *, struct pt_regs *) __read_mostly;
    static void (*default_backward)(struct perf_event *, struct perf_sample_data *, struct pt_regs *) __read_mostly;

    if (!event) return false;

    if (!default_forward || !default_backward)
    {
        default_forward = (void *)generic_kallsyms_lookup_name("perf_event_output_forward");
        default_backward = (void *)generic_kallsyms_lookup_name("perf_event_output_backward");
    }

    return (default_forward && event->overflow_handler == default_forward) || (default_backward && event->overflow_handler == default_backward);
}

// 执行断异常处理跳板工作函数
static int work_trampoline_breakpoint(struct pt_regs *hook_regs)
{
    // breakpoint_handler(unused, esr, regs)
    struct break_point *bp_info = g_bp_info;
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[2];

    if (!bp_info_targets_task(bp_info, current)) return 0;

    struct fp_regs fp_regs __attribute__((__uninitialized__));
    read_all_q_regs(&fp_regs);

    // 执行断点没有独立的命中地址参数，内核使用异常现场的 PC 进行派发。
    uint64_t current_pc = untagged_addr(regs->pc) & ~0x3ULL;
    struct perf_event **perf_slots = this_cpu_ptr(bp_on_reg);
    bool own_hit = false;
    bool perf_hit = false;
    bool perf_requires_step = false;

    size_t point_slot = 0;
    struct bp_point *point;
    while ((point = bp_info_find_active_point(bp_info, &point_slot)))
    {
        struct arch_hw_breakpoint info;

        if (hw_breakpoint_parse(point, 0, &info) || info.ctrl.type != ARM_BREAKPOINT_EXECUTE || info.address != current_pc) continue;

        own_hit = true;
        point->on_hit(regs, &fp_regs, point);
    }

    for (int slot = 0; slot < num_brps; slot++)
    {
        rcu_read_lock();
        struct perf_event *event = READ_ONCE(perf_slots[slot]);
        if (!event) goto next_perf_breakpoint;

        struct arch_hw_breakpoint *perf_info = &event->hw.info;
        if (perf_info->ctrl.type != ARM_BREAKPOINT_EXECUTE || perf_info->address != current_pc) goto next_perf_breakpoint;

        perf_hit = true;
        perf_info->trigger = current_pc;
        fn_perf_bp_event(event, regs);
        if (perf_breakpoint_requires_step(event)) perf_requires_step = true;

    next_perf_breakpoint:
        rcu_read_unlock();
    }

    if (!own_hit && !perf_hit) return 0;

    // 存在 perf 命中时沿用原 handler 的 step 决策；纯自有命中始终模拟步过。
    bool requires_step = perf_hit ? perf_requires_step : own_hit;
    if (requires_step) emulate_inst(regs, &fp_regs, 0);
    write_all_q_regs(&fp_regs);

    // 不需要步过时保留异常现场；两种情况都跳过原 handler，避免重复发送 perf 事件。
    hook_regs->regs[0] = 0;
    return 1;
}

// 访问断异常处理跳板工作函数
static int work_trampoline_watchpoint(struct pt_regs *hook_regs)
{
    // watchpoint_handler(addr, esr, regs)，
    uint64_t fault_addr = untagged_addr(hook_regs->regs[0]);
    uint64_t esr = hook_regs->regs[1];
    struct break_point *bp_info = g_bp_info;
    struct pt_regs *regs = (struct pt_regs *)hook_regs->regs[2];

    if (!bp_info_targets_task(bp_info, current)) return 0;

    struct fp_regs fp_regs __attribute__((__uninitialized__));
    read_all_q_regs(&fp_regs);
    struct perf_event **perf_slots = this_cpu_ptr(wp_on_reg);
    bool own_hit = false;
    bool perf_hit = false;
    bool perf_requires_step = false;

    size_t point_slot = 0;
    struct bp_point *point;
    while ((point = bp_info_find_active_point(bp_info, &point_slot)))
    {
        struct arch_hw_breakpoint info;

        if (hw_breakpoint_parse(point, 0, &info) || info.ctrl.type == ARM_BREAKPOINT_EXECUTE || !watchpoint_access_matches(&info, esr) || get_distance_from_watchpoint(fault_addr, info.address, &info.ctrl) != 0) continue;

        own_hit = true;
        point->on_hit(regs, &fp_regs, point);
    }

    rcu_read_lock();
    for (int slot = 0; slot < num_wrps; slot++)
    {
        struct perf_event *event = READ_ONCE(perf_slots[slot]);
        if (!event) continue;

        struct arch_hw_breakpoint *perf_info = &event->hw.info;
        if (!watchpoint_access_matches(perf_info, esr) || get_distance_from_watchpoint(fault_addr, perf_info->address, &perf_info->ctrl) != 0) continue;

        perf_hit = true;
        perf_info->trigger = fault_addr;
        if (!user_mode(regs) && perf_info->ctrl.privilege == AARCH64_BREAKPOINT_EL0)
        {
            perf_requires_step = true;
            continue;
        }

        fn_perf_bp_event(event, regs);
        if (perf_breakpoint_requires_step(event)) perf_requires_step = true;
    }
    rcu_read_unlock();

    if (!own_hit && !perf_hit) return 0;

    // 存在 perf 命中时沿用原 handler 的 step 决策；纯自有命中始终模拟步过。
    bool requires_step = perf_hit ? perf_requires_step : own_hit;
    if (requires_step) emulate_inst(regs, &fp_regs, 0);
    write_all_q_regs(&fp_regs);

    // 不需要步过时保留异常现场；两种情况都跳过原 handler，避免重复发送 perf 事件。
    hook_regs->regs[0] = 0;
    return 1;
}

/*
bp_on_reg/wp_on_reg 不是全局断点配置表，而是 perf 对各 CPU 硬件槽位的当前软件记录：
槽位中的 perf_event 表示 perf 认为该配置已经安装到对应 CPU 的 BRP/WRP 寄存器。
按 task 注册的断点会随 task 调度进出 CPU 而安装/卸载，因此不同 CPU 的槽位内容可以不同；
按 CPU 注册的断点只会出现在绑定 CPU 的槽位中。

*/
static void dump_perf_breakpoint_slots(void)
{
    int cpu;

    rcu_read_lock();
    for_each_online_cpu(cpu)
    {
        struct perf_event **breakpoint_slots = per_cpu_ptr(bp_on_reg, cpu);
        struct perf_event **watchpoint_slots = per_cpu_ptr(wp_on_reg, cpu);

        for (int slot = 0; slot < num_brps; slot++)
        {
            struct perf_event *event = READ_ONCE(breakpoint_slots[slot]);
            if (!event) continue;

            struct arch_hw_breakpoint *info = &event->hw.info;
            ls_log_always_tag("hwbp-perf", "cpu=%d kind=bp slot=%d addr=0x%llx len=%llu type=0x%x disabled=%u arch_addr=0x%llx enabled=%u privilege=%u type=0x%x len=0x%x ctrl=0x%x\n", cpu, slot, (unsigned long long)event->attr.bp_addr, (unsigned long long)event->attr.bp_len, event->attr.bp_type, event->attr.disabled, (unsigned long long)info->address, info->ctrl.enabled, info->ctrl.privilege, info->ctrl.type, info->ctrl.len, encode_ctrl_reg(info->ctrl));
        }

        for (int slot = 0; slot < num_wrps; slot++)
        {
            struct perf_event *event = READ_ONCE(watchpoint_slots[slot]);
            if (!event) continue;

            struct arch_hw_breakpoint *info = &event->hw.info;
            ls_log_always_tag("hwbp-perf", "cpu=%d kind=wp slot=%d addr=0x%llx len=%llu type=0x%x disabled=%u arch_addr=0x%llx enabled=%u privilege=%u type=0x%x len=0x%x ctrl=0x%x\n", cpu, slot, (unsigned long long)event->attr.bp_addr, (unsigned long long)event->attr.bp_len, event->attr.bp_type, event->attr.disabled, (unsigned long long)info->address, info->ctrl.enabled, info->ctrl.privilege, info->ctrl.type, info->ctrl.len, encode_ctrl_reg(info->ctrl));
        }
    }
    rcu_read_unlock();
}

// 在当前 CPU 上安装硬件断点/观察点寄存器。
static void install_hwbp_regs_on_cpu(struct break_point *bp_info)
{
    struct perf_event **breakpoint_slots = this_cpu_ptr(bp_on_reg);
    struct perf_event **watchpoint_slots = this_cpu_ptr(wp_on_reg);
    int brp_slot = 0, wrp_slot = 0;
    size_t point_slot = 0;
    struct bp_point *point;

    //这里输出的话，必须目标进程有调度才会输出一直运行的话，可能就输出一次，实际测试发现目标在断点检测下会疯狂调用ptrace让task进调度安装硬件断点
    //dump_perf_breakpoint_slots();

    while ((point = bp_info_find_active_point(bp_info, &point_slot)))
    {
        struct arch_hw_breakpoint info;
        if (hw_breakpoint_parse(point, 0, &info)) continue;

        if (info.ctrl.type == ARM_BREAKPOINT_EXECUTE)
        {
            while (brp_slot < num_brps && READ_ONCE(breakpoint_slots[brp_slot])) brp_slot++;
            if (brp_slot >= num_brps) continue;

            write_wb_reg(AARCH64_DBG_REG_BVR, brp_slot, info.address);
            write_wb_reg(AARCH64_DBG_REG_BCR, brp_slot, encode_ctrl_reg(info.ctrl) | 0x1);
            brp_slot++;
        }
        else
        {
            while (wrp_slot < num_wrps && READ_ONCE(watchpoint_slots[wrp_slot])) wrp_slot++;
            if (wrp_slot >= num_wrps) continue;

            write_wb_reg(AARCH64_DBG_REG_WVR, wrp_slot, info.address);
            write_wb_reg(AARCH64_DBG_REG_WCR, wrp_slot, encode_ctrl_reg(info.ctrl) | 0x1);
            wrp_slot++;
        }
    }
}

// 清理当前 CPU 上的自定义硬件断点/观察点，保留 perf 已占用的槽位和控制寄存器配置。
static void clear_hwbp_regs_on_cpu(void *data)
{
    struct perf_event **breakpoint_slots = this_cpu_ptr(bp_on_reg);
    struct perf_event **watchpoint_slots = this_cpu_ptr(wp_on_reg);
    struct break_point *bp_info = g_bp_info;

    (void)data;
    if (!bp_info) return;

    for (int slot = 0; slot < num_brps; slot++)
    {
        if (READ_ONCE(breakpoint_slots[slot])) continue;

        uint64_t ctrl = read_wb_reg(AARCH64_DBG_REG_BCR, slot);
        uint64_t addr = read_wb_reg(AARCH64_DBG_REG_BVR, slot);
        if (!(ctrl & 0x1)) continue;

        size_t point_slot = 0;
        struct bp_point *point;
        while ((point = bp_info_find_active_point(bp_info, &point_slot)))
        {
            struct arch_hw_breakpoint info;

            if (hw_breakpoint_parse(point, 0, &info) || info.ctrl.type != ARM_BREAKPOINT_EXECUTE || info.address != addr || (encode_ctrl_reg(info.ctrl) & ~0x1ULL) != (ctrl & ~0x1ULL)) continue;

            write_wb_reg(AARCH64_DBG_REG_BCR, slot, ctrl & ~0x1ULL);
            break;
        }
    }

    for (int slot = 0; slot < num_wrps; slot++)
    {
        if (READ_ONCE(watchpoint_slots[slot])) continue;

        uint64_t ctrl = read_wb_reg(AARCH64_DBG_REG_WCR, slot);
        uint64_t addr = read_wb_reg(AARCH64_DBG_REG_WVR, slot);
        if (!(ctrl & 0x1)) continue;

        size_t point_slot = 0;
        struct bp_point *point;
        while ((point = bp_info_find_active_point(bp_info, &point_slot)))
        {
            struct arch_hw_breakpoint info;

            if (hw_breakpoint_parse(point, 0, &info) || info.ctrl.type == ARM_BREAKPOINT_EXECUTE || info.address != addr || (encode_ctrl_reg(info.ctrl) & ~0x1ULL) != (ctrl & ~0x1ULL)) continue;

            write_wb_reg(AARCH64_DBG_REG_WCR, slot, ctrl & ~0x1ULL);
            break;
        }
    }
}

static void __attribute__((used, __noinline__)) ret_work_finish_task_switch(void)
{
    struct break_point *bp_info = g_bp_info;

    if (bp_info_targets_task(bp_info, current) && bp_info_find_active_point(bp_info, NULL))
    {
        enable_hardware_debug_on_cpu(NULL);
        install_hwbp_regs_on_cpu(bp_info);
    }
    else
    {
        clear_hwbp_regs_on_cpu(NULL);
        // disable_hardware_debug_on_cpu(NULL);
    }
}

// 返回地址hook 跳板
__attribute__((naked, used)) void ret_trampoline_finish_task_switch(void)
{
    asm volatile("str x0, [sp, #8]\n"
                 "bl ret_work_finish_task_switch\n"
                 "ldp x16, x0, [sp], #304\n"
                 "ret x16\n");
}

// finish_task_switch(prev) 入口 hook：函数返回后再覆盖 perf 写入的硬件断点寄存器。
static int work_trampoline_finish_task_switch(struct pt_regs *hook_regs)
{
    if (!g_bp_info) return 0;

    *(unsigned long *)(hook_regs->sp = (unsigned long)hook_frame_metadata(hook_regs)) = hook_regs->regs[30];
    hook_regs->regs[30] = (unsigned long)ret_trampoline_finish_task_switch;

    return 0;
}

// 硬件调试异常和调度返回 hook 统一安装、回滚与卸载。
static struct hook_entry g_hwbp_hooks[] = {
    HOOK_ENTRY("breakpoint_handler", work_trampoline_breakpoint),
    HOOK_ENTRY("watchpoint_handler", work_trampoline_watchpoint),
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

    if (!bp_info || !bp_info_find_active_point(bp_info, NULL))
    {
        ls_log_tag("hwbp", "breakpoint info error\n");
        return -EINVAL;
    }

    num_brps = get_brps_num();
    num_wrps = get_wrps_num();
    bp_info->num_brps = num_brps;
    bp_info->num_wrps = num_wrps;

    //配置存在说明安装过了，这里只替换更新配置指针
    if (g_bp_info)
    {
        g_bp_info = bp_info;
        ls_log_tag("hwbp", "monitor config updated\n");
        return 0;
    }

    bp_on_reg = (struct perf_event * __percpu *)generic_kallsyms_lookup_name("bp_on_reg");
    wp_on_reg = (struct perf_event * __percpu *)generic_kallsyms_lookup_name("wp_on_reg");
    fn_perf_bp_event = (void (*)(struct perf_event *, void *))generic_kallsyms_lookup_name("perf_bp_event");
    if (!bp_on_reg || !wp_on_reg || !fn_perf_bp_event)
    {
        ls_log_tag("hwbp", "lookup bp_on_reg/wp_on_reg/perf_bp_event failed\n");
        return -ENOENT;
    }

    // 传递上下文给全局指针，让异常处理和断点写入都能互相传递配置信息
    g_bp_info = bp_info;

    // 统一安装异常 hook 和 finish_task_switch return hook。
    ret = inline_hook_install(g_hwbp_hooks);
    if (ret)
    {
        ls_log_tag("hwbp", "inline_hook_install hwbp hooks failed: %d\n", ret);
        g_bp_info = NULL;
        return ret;
    }

    ls_log_tag("hwbp", "hwbp hooks installed\n");
    ls_log_tag("hwbp", "monitor start\n");
    return 0;
}

// 注销 hook，取消监听
static void stop_task_run_monitor(void)
{
    if (!READ_ONCE(g_bp_info)) return;

    int cpu;
    // 遍历所有在线 CPU，清理寄存器
    for_each_online_cpu(cpu) smp_call_function_single(cpu, clear_hwbp_regs_on_cpu, NULL, 1);

    inline_hook_remove(g_hwbp_hooks);
    WRITE_ONCE(g_bp_info, NULL);
    ls_log_tag("hwbp", "monitor stop\n");
}