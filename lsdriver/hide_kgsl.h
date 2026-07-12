// 感谢https://github.com/LinYuFlower(林雨)

#ifndef HIDE_KGSL_H
#define HIDE_KGSL_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/types.h>

#include "export_fun.h"
#include "inline_hook_frame.h"
#include "lsdriver_log.h"

/*
在高通设备上可查询 GPU/KGSL 信息的路径：

设备节点：
/dev/kgsl-3d0
/dev/dri/card0
/dev/dri/renderD128

高通 KGSL 进程节点：
/sys/class/kgsl/kgsl
/sys/class/kgsl/kgsl/proc
/sys/class/kgsl/kgsl/proc/<pid>
/sys/class/kgsl/kgsl/pagetables
/sys/class/kgsl/kgsl/pagetables/<pid>
/sys/class/kgsl/kgsl/pagetables/<pid>/mapped
/sys/class/kgsl/kgsl/pagetables/<pid>/max_mapped
/sys/class/kgsl/kgsl/pagetables/<pid>/entries

KGSL 全局内存节点：
/sys/class/kgsl/kgsl/mapped
/sys/class/kgsl/kgsl/mapped_max
/sys/class/kgsl/kgsl/secure
/sys/class/kgsl/kgsl/secure_max
/sys/class/kgsl/kgsl/vmalloc_max
/sys/class/kgsl/kgsl/coherent_max
/sys/class/kgsl/kgsl/page_alloc
/sys/class/kgsl/kgsl/max_reclaim_limit
/sys/class/kgsl/kgsl/page_reclaim_per_call
/sys/class/kgsl/kgsl/full_cache_threshold

KGSL 3D 设备状态节点：
/sys/class/kgsl/kgsl-3d0
/sys/class/kgsl/kgsl-3d0/gpu_model
/sys/class/kgsl/kgsl-3d0/gpu_busy_percentage
/sys/class/kgsl/kgsl-3d0/gpubusy
/sys/class/kgsl/kgsl-3d0/gpuclk
/sys/class/kgsl/kgsl-3d0/clock_mhz
/sys/class/kgsl/kgsl-3d0/gpu_available_frequencies
/sys/class/kgsl/kgsl-3d0/freq_table_mhz
/sys/class/kgsl/kgsl-3d0/gpu_clock_stats
/sys/class/kgsl/kgsl-3d0/temp
/sys/class/kgsl/kgsl-3d0/reset_count
/sys/class/kgsl/kgsl-3d0/preempt_count
/sys/class/kgsl/kgsl-3d0/ifpc_count
/sys/class/kgsl/kgsl-3d0/perfcounter
/sys/class/kgsl/kgsl-3d0/dev
/sys/class/kgsl/kgsl-3d0/uevent

KGSL 电源和频率控制节点：
/sys/class/kgsl/kgsl-3d0/min_pwrlevel
/sys/class/kgsl/kgsl-3d0/max_pwrlevel
/sys/class/kgsl/kgsl-3d0/default_pwrlevel
/sys/class/kgsl/kgsl-3d0/thermal_pwrlevel
/sys/class/kgsl/kgsl-3d0/min_clock_mhz
/sys/class/kgsl/kgsl-3d0/max_clock_mhz
/sys/class/kgsl/kgsl-3d0/max_gpuclk
/sys/class/kgsl/kgsl-3d0/force_clk_on
/sys/class/kgsl/kgsl-3d0/force_bus_on
/sys/class/kgsl/kgsl-3d0/force_rail_on
/sys/class/kgsl/kgsl-3d0/force_no_nap
/sys/class/kgsl/kgsl-3d0/idle_timer
/sys/class/kgsl/kgsl-3d0/pwrscale
/sys/class/kgsl/kgsl-3d0/power
/sys/class/kgsl/kgsl-3d0/power/runtime_status
/sys/class/kgsl/kgsl-3d0/power/runtime_active_time
/sys/class/kgsl/kgsl-3d0/power/runtime_suspended_time
/sys/class/kgsl/kgsl-3d0/power/autosuspend_delay_ms
/sys/class/kgsl/kgsl-3d0/power/control

KGSL devfreq 节点：
/sys/class/kgsl/kgsl-3d0/devfreq
/sys/class/kgsl/kgsl-3d0/devfreq/cur_freq
/sys/class/kgsl/kgsl-3d0/devfreq/target_freq
/sys/class/kgsl/kgsl-3d0/devfreq/min_freq
/sys/class/kgsl/kgsl-3d0/devfreq/max_freq
/sys/class/kgsl/kgsl-3d0/devfreq/available_frequencies
/sys/class/kgsl/kgsl-3d0/devfreq/gpu_load
/sys/class/kgsl/kgsl-3d0/devfreq/mod_percent
/sys/class/kgsl/kgsl-3d0/devfreq/governor
/sys/class/kgsl/kgsl-3d0/devfreq/available_governors
/sys/class/kgsl/kgsl-3d0/devfreq/trans_stat
/sys/class/kgsl/kgsl-3d0/devfreq/suspend_time
/sys/class/kgsl/kgsl-3d0/devfreq/name
/sys/class/devfreq/3d00000.qcom,kgsl-3d0
/sys/class/devfreq/kgsl-busmon

KGSL 快照和故障节点：
/sys/class/kgsl/kgsl-3d0/snapshot
/sys/class/kgsl/kgsl-3d0/snapshot/dump
/sys/class/kgsl/kgsl-3d0/snapshot/faultcount
/sys/class/kgsl/kgsl-3d0/snapshot/timestamp
/sys/class/kgsl/kgsl-3d0/snapshot/snapshot_control
/sys/class/kgsl/kgsl-3d0/snapshot/snapshot_hashid
/sys/class/kgsl/kgsl-3d0/snapshot/snapshot_legacy
/sys/class/kgsl/kgsl-3d0/snapshot/snapshot_crashdumper
/sys/class/kgsl/kgsl-3d0/snapshot/skip_ib_capture
/sys/class/kgsl/kgsl-3d0/snapshot/prioritize_unrecoverable
/sys/class/kgsl/kgsl-3d0/snapshot/minidump_test
/sys/class/kgsl/kgsl-3d0/snapshot/force_panic

其他可见的 GPU/KGSL 路径：
/proc/irq/320/kgsl_3d0_irq
/sys/devices/platform/soc/3d00000.qcom,kgsl-3d0
/sys/devices/platform/soc/3d00000.qcom,kgsl-3d0/kgsl/kgsl-3d0
/sys/class/kgsl/kgsl-3d0/device/devfreq
/sys/class/kgsl/kgsl-3d0/device/kgsl
/sys/class/kgsl/kgsl-3d0/device/kgsl-busmon
/sys/class/kgsl/kgsl-3d0/device/coresight-gfx
/sys/class/kgsl/kgsl-3d0/device/coresight-gfx-cx
*/

#define HIDE_KGSL_MAX_PIDS 8

// KGSL 隐藏表保存目标进程 tgid；0 表示空槽。
static pid_t g_hide_kgsl_pids[HIDE_KGSL_MAX_PIDS];
static DEFINE_MUTEX(g_hide_kgsl_lock);

// 快速判断 KGSL 隐藏表是否还有目标，空表时可卸载 hook。
static bool hide_kgsl_has_pid(void)
{
    int i;

    for (i = 0; i < HIDE_KGSL_MAX_PIDS; i++)
        if (READ_ONCE(g_hide_kgsl_pids[i])) return true;
    return false;
}

// 判断当前 task 是否属于需要隐藏 KGSL 节点的目标进程。
static bool should_hide(void)
{
    int i;

    for (i = 0; i < HIDE_KGSL_MAX_PIDS; i++)
    {
        pid_t hide_pid = READ_ONCE(g_hide_kgsl_pids[i]);

        if (hide_pid && current->tgid == hide_pid) return true;
    }
    return false;
}
// 判断指定对象是否为kgsl下
static bool kobj_under_kgsl(struct kobject *kobj)
{
    struct kobject *p;
    int depth = 0;

    for (p = kobj->parent; p && depth < 8; p = p->parent, depth++)
    {
        if (p->name && strstr(p->name, "kgsl")) return true;
    }
    return false;
}

// ARM64：伪造 -ENOMEM 并跳过函数体
static void fake_enomem_and_return(struct pt_regs *regs)
{
    regs->regs[0] = (u64)(long)(-ENOMEM);
    regs->pc = regs->regs[30]; /* LR -> 返回调用者 */
}

// kgsl_process_init_sysfs / kgsl_process_init_debugfs inline hook 工作函数
static int kgsl_process_init_hook_work(struct pt_regs *regs)
{
    if (should_hide())
    {
        fake_enomem_and_return(regs);
        return 1; /* 非零：恢复现场后不执行原函数 */
    }
    return 0;
}

static int sysfs_create_group_hook_work(struct pt_regs *regs)
{
    struct kobject *kobj = (struct kobject *)regs->regs[0];

    if (!kobj || !kobj->name) return 0;

    if (!kobj_under_kgsl(kobj)) return 0;

    if (should_hide())
    {
        regs->regs[0] = -ENOMEM;
        regs->pc = regs->regs[30];
        return 1;
    }
    return 0;
}

static struct hook_entry g_kgsl_hooks[] = {
    HOOK_ENTRY("kgsl_process_init_sysfs", kgsl_process_init_hook_work),
    HOOK_ENTRY("kgsl_process_init_debugfs", kgsl_process_init_hook_work),
    HOOK_ENTRY("sysfs_create_group", sysfs_create_group_hook_work),

};

// 安装隐藏支持多个 PID 同时隐藏 KGSL/GPU 节点初始化信息
int hide_kgsl_install(pid_t pid)
{
    int ret = 0;
    int i, empty = -1;

    if (pid <= 0) return -EINVAL;

    mutex_lock(&g_hide_kgsl_lock);

    ret = inline_hook_install(g_kgsl_hooks);
    if (ret)
    {
        ls_log_tag("kgsl_hide", "inline hook install failed: %d\n", ret);
        goto out_unlock;
    }
    ls_log_tag("kgsl_hide", "inline hook installed\n");

    // hook 安装成功后再写隐藏表，避免表里有 PID 但拦截点没生效。
    for (i = 0; i < HIDE_KGSL_MAX_PIDS; i++)
    {
        pid_t hidden_pid = READ_ONCE(g_hide_kgsl_pids[i]);

        if (hidden_pid == pid) goto out_unlock;
        if (!hidden_pid && empty < 0) empty = i;
    }

    if (empty < 0)
    {
        ret = -ENOSPC;
        goto out_unlock;
    }

    WRITE_ONCE(g_hide_kgsl_pids[empty], pid);
    ls_log_tag("kgsl_hide", "hidden PID %d\n", pid);

out_unlock:
    mutex_unlock(&g_hide_kgsl_lock);
    return ret;
}

// 删除指定目标 PID；如果隐藏表空了，就卸载 KGSL hooks。
void hide_kgsl_remove(pid_t pid)
{
    int i;

    if (pid <= 0) return;

    mutex_lock(&g_hide_kgsl_lock);
    for (i = 0; i < HIDE_KGSL_MAX_PIDS; i++)
    {
        if (READ_ONCE(g_hide_kgsl_pids[i]) == pid) WRITE_ONCE(g_hide_kgsl_pids[i], 0);
    }

    if (!hide_kgsl_has_pid()) inline_hook_remove(g_kgsl_hooks);
    mutex_unlock(&g_hide_kgsl_lock);
}

#endif /* HIDE_KGSL_H */
