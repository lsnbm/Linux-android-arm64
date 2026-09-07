#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/kernel.h>
#include <linux/kallsyms.h>
#include <linux/memory.h>
#include <linux/percpu.h>
#include <linux/cpu.h>
#include <linux/rcupdate.h>
#include <linux/smp.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <asm/cacheflush.h>
#include <asm/esr.h>
#include <asm/sysreg.h>
#include "export_fun.h"
#include "arm64_reg.h"
#include "inline_hook_frame.h"
#include "lsdriver_log.h"
#include "io_struct.h"

// SPE 系统寄存器编码宏定义 (部分低版本内核头文件未定义时备用)
#ifndef SYS_PMSCR_EL1
#define SYS_PMSCR_EL1    sys_reg(3, 0, 9, 9, 0)
#endif
#ifndef SYS_PMSICR_EL1
#define SYS_PMSICR_EL1   sys_reg(3, 0, 9, 9, 2)
#endif
#ifndef SYS_PMSFCR_EL1
#define SYS_PMSFCR_EL1   sys_reg(3, 0, 9, 9, 4)
#endif
#ifndef SYS_PMSEVFR_EL1
#define SYS_PMSEVFR_EL1  sys_reg(3, 0, 9, 9, 5)
#endif
#ifndef SYS_PMSADR_EL1
#define SYS_PMSADR_EL1   sys_reg(3, 0, 9, 9, 6)
#endif
#ifndef SYS_PMBPTR_EL1
#define SYS_PMBPTR_EL1   sys_reg(3, 0, 9, 10, 0)
#endif
#ifndef SYS_PMBSR_EL1
#define SYS_PMBSR_EL1    sys_reg(3, 0, 9, 10, 1)
#endif
#ifndef SYS_PMBLIMIT_EL1
#define SYS_PMBLIMIT_EL1 sys_reg(3, 0, 9, 10, 2)
#endif

// SPE 硬件中断滑步允许的窗口大小 (16字节 = 4条指令)
#define SPE_SKID_WINDOW_BYTES 0x10
#define SPE_PERCPU_BUFFER_SIZE 4096

struct break_point *g_bp_info;
static void * __percpu * g_spe_buffers; // 每个 CPU 专属的 SPE 硬件写入缓冲区

// 检查当前 CPU 硬件是否支持 ARMv8.2+ SPE
static bool is_cpu_support_spe(void)
{
    uint64_t dfr0 = read_sysreg(id_aa64dfr0_el1);
    uint32_t pmsver = (dfr0 >> 32) & 0xF;
    return pmsver >= 1; // PMSVer >= 1 表示支持 SPE
}

// 在当前 CPU 上安装并激活 SPE 地址过滤监控
static void install_spe_regs_on_cpu(struct break_point *bp_info)
{
    size_t point_slot = 0;
    struct bp_point *point = bp_info_find_active_point(bp_info, &point_slot);
    if (!point) return;

    void *buf = *this_cpu_ptr(g_spe_buffers);
    if (!buf) return;

    uint64_t buf_addr = (uint64_t)buf;
    uint64_t target_pc = untagged_addr(point->hit_addr) & ~0x3ULL;

    // 1. 禁用当前核心的 SPE
    write_sysreg_s(0, SYS_PMSCR_EL1);
    isb();

    // 2. 清除状态寄存器
    write_sysreg_s(0, SYS_PMBSR_EL1);

    // 3. 配置硬件写入缓冲区 (首地址与极小上限，单次命中立即触发中断)
    write_sysreg_s(buf_addr, SYS_PMBPTR_EL1);
    // 限制仅允许写入 1 条样本 (64 字节)，PMBLIMIT.E=1 使能中断
    write_sysreg_s((buf_addr + 64) | 1ULL, SYS_PMBLIMIT_EL1);

    // 4. 配置地址比较器：仅监听指定 PC
    write_sysreg_s(target_pc, SYS_PMSADR_EL1);

    // 5. 配置过滤器 (PMSFCR_EL1): FE=1(开启过滤), FT=0(匹配 PC 取指)
    write_sysreg_s(1ULL, SYS_PMSFCR_EL1);

    // 6. 设置采样间隔 (PMSICR_EL1 = 1)，保证经过必抓
    write_sysreg_s(1ULL, SYS_PMSICR_EL1);

    // 7. 仅统计指令退休事件 (Retired)
    write_sysreg_s(0ULL, SYS_PMSEVFR_EL1);

    // 8. 激活 EL0 用户态 SPE 监控 (E0SPE = 1, E1SPE = 0 过滤内核态)
    uint64_t pmscr = (1ULL << 1);
    write_sysreg_s(pmscr, SYS_PMSCR_EL1);
    isb();

    ls_log_always_tag("spe", "cpu=%d arm spe for pid=%d target_pc=0x%llx\n", 
                      smp_processor_id(), bp_info->tgid, target_pc);
}

// 清理/禁用当前 CPU 上的 SPE 硬件监控
static void clear_spe_regs_on_cpu(void *data)
{
    (void)data;
    // 关闭 SPE 采样与缓冲区使能
    write_sysreg_s(0, SYS_PMSCR_EL1);
    write_sysreg_s(0, SYS_PMBLIMIT_EL1);
    write_sysreg_s(0, SYS_PMBSR_EL1);
    isb();
}

// SPE 硬件缓冲区中断 / 事件命中跳板函数
// hook 挂接在内核 SPE 中断分发入口 (如 arm_spe_pmu_irq_handler)
static int work_trampoline_spe_irq(struct pt_regs *hook_regs)
{
    struct break_point *bp_info = g_bp_info;
    if (!bp_info || !bp_info_targets_task(bp_info, current)) return 0;

    // hook_regs 即为硬件中断打断时的上下文现场
    struct pt_regs *regs = hook_regs;
    uint64_t current_pc = untagged_addr(regs->pc) & ~0x3ULL;

    struct fp_regs fp_regs __attribute__((__uninitialized__));
    read_all_q_regs(&fp_regs);

    bool hit = false;
    size_t point_slot = 0;
    struct bp_point *point;

    // 遍历活动断点，判断当前 PC 是否落在允许的滑步窗口 [hit_addr, hit_addr + 0x10]
    while ((point = bp_info_find_active_point(bp_info, &point_slot)))
    {
        uint64_t target_pc = untagged_addr(point->hit_addr) & ~0x3ULL;

        // 核心改动：允许滑步范围判定，且只读模式
        if (current_pc >= target_pc && current_pc <= (target_pc + SPE_SKID_WINDOW_BYTES))
        {
            hit = true;
            ls_log_always_tag("spe-hit", "SPE Hit! target=0x%llx actual_pc=0x%llx (skid=+%lld bytes)\n",
                              target_pc, current_pc, current_pc - target_pc);

            // 触发用户的只读监控回调
            if (point->on_hit) {
                point->on_hit(regs, &fp_regs, point);
            }
            break;
        }
    }

    if (!hit) return 0;

    // 命中后重置 SPE 缓冲区指针和中断状态，准备下一次捕获
    void *buf = *this_cpu_ptr(g_spe_buffers);
    if (buf) {
        write_sysreg_s((uint64_t)buf, SYS_PMBPTR_EL1);
        write_sysreg_s(0, SYS_PMBSR_EL1);
        isb();
    }

    // 只读监控无需 emulate_inst 模拟，直接返回 1 放行原生执行
    return 1;
}

// 调度切换返回处理：目标任务切入时装载 SPE，切出时关闭 SPE
static void __attribute__((used, __noinline__)) ret_work_finish_task_switch(void)
{
    struct break_point *bp_info = g_bp_info;

    if (bp_info_targets_task(bp_info, current) && bp_info_find_active_point(bp_info, NULL))
    {
        install_spe_regs_on_cpu(bp_info);
    }
    else
    {
        clear_spe_regs_on_cpu(NULL);
    }
}

// finish_task_switch 返回跳板
__attribute__((naked, used)) void ret_trampoline_finish_task_switch(void)
{
    asm volatile("str x0, [sp, #8]\n"
                 "bl ret_work_finish_task_switch\n"
                 "ldp x16, x0, [sp], #304\n"
                 "ret x16\n");
}

static int work_trampoline_finish_task_switch(struct pt_regs *hook_regs)
{
    if (!g_bp_info) return 0;

    *(unsigned long *)(hook_regs->sp = (unsigned long)hook_frame_metadata(hook_regs)) = hook_regs->regs[30];
    hook_regs->regs[30] = (unsigned long)ret_trampoline_finish_task_switch;

    return 0;
}

// 替换原有的 breakpoint/watchpoint hook，改挂 SPE 中断与调度
static struct hook_entry g_spe_hooks[] = {
    // 挂接内核原生的 SPE 中断处理函数
    HOOK_ENTRY("arm_spe_pmu_irq_handler", work_trampoline_spe_irq),
    HOOK_ENTRY("finish_task_switch", work_trampoline_finish_task_switch),
};

// 启动 SPE 监控
static int start_task_run_monitor(struct break_point *bp_info)
{
    int ret, cpu;

    if (!bp_info || !bp_info_find_active_point(bp_info, NULL))
    {
        ls_log_tag("spe", "breakpoint info error\n");
        return -EINVAL;
    }

    if (!is_cpu_support_spe())
    {
        ls_log_always_tag("spe", "CPU does not support ARMv8.2 SPE feature!\n");
        return -EOPNOTSUPP;
    }

    // 为每个 CPU 分配专属的 SPE DMA 缓冲区
    if (!g_spe_buffers)
    {
        g_spe_buffers = alloc_percpu(void *);
        if (!g_spe_buffers) return -ENOMEM;

        for_each_possible_cpu(cpu) {
            void *buf = kzalloc(SPE_PERCPU_BUFFER_SIZE, GFP_KERNEL);
            *per_cpu_ptr(g_spe_buffers, cpu) = buf;
        }
    }

    g_bp_info = bp_info;

    // 安装 hook
    ret = inline_hook_install(g_spe_hooks);
    if (ret)
    {
        ls_log_tag("spe", "inline_hook_install spe hooks failed: %d\n", ret);
        g_bp_info = NULL;
        return ret;
    }

    ls_log_tag("spe", "SPE monitor started successfully\n");
    return 0;
}

// 停止 SPE 监控并释放资源
static void stop_task_run_monitor(void)
{
    if (!READ_ONCE(g_bp_info)) return;

    int cpu;
    // 遍历所有 CPU 关闭 SPE 硬件
    for_each_online_cpu(cpu) {
        smp_call_function_single(cpu, clear_spe_regs_on_cpu, NULL, 1);
    }

    inline_hook_remove(g_spe_hooks);
    WRITE_ONCE(g_bp_info, NULL);

    if (g_spe_buffers) {
        for_each_possible_cpu(cpu) {
            void *buf = *per_cpu_ptr(g_spe_buffers, cpu);
            kfree(buf);
        }
        free_percpu(g_spe_buffers);
        g_spe_buffers = NULL;
    }

    ls_log_tag("spe", "SPE monitor stopped\n");
}