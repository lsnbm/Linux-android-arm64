#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <asm/ptrace.h>

#include "arm64_dbi_ghost.h"
#include "inline_hook_frame.h"
#include "io_struct.h"

#define PTEBP_ESR_EC_IABT_LOW 0x20
#define PTEBP_ESR_FSC_MASK 0x3f
#define PTEBP_ESR_FSC_PERM_MIN 0x0c
#define PTEBP_ESR_FSC_PERM_MAX 0x0f

/*
 * PTEBP 当前只保留执行断点：目标页写 UXN，lower EL 取指权限异常进
 * work_trampoline_ptebp，然后把 PC 重定向到 DBI 生成的 ghost 页。
 * ghost 页资源和 pid + target_page 状态由 arm64_dbi_ghost.h 托管。
 */

struct ptebp_hit
{
	// do_mem_abort 阶段的临时命中信息，只在本次页异常处理过程中使用。
	struct bp_point *point;
	uint64_t ghost_pc;
};

static struct break_point *g_ptebp_info;
static DEFINE_SPINLOCK(g_ptebp_lock);
static DEFINE_MUTEX(g_ptebp_remove_mutex);

// 判断单个 PTEBP 点位是否具备安装和派发条件。
static inline bool ptebp_point_is_active(struct bp_point *point)
{
	return point && point->hit_addr != 0 && point->on_hit && (point->bt & BP_BREAKPOINT_X);
}

// 判断一个 break_point 配置中是否至少存在一个有效 PTEBP 点位。
static inline bool ptebp_info_has_active_point(struct break_point *info)
{
	int point_slot;

	if (!info || info->pid <= 0)
		return false;

	for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
	{
		if (ptebp_point_is_active(&info->points[point_slot]))
			return true;
	}

	return false;
}

// 判断当前线程或线程组是否属于目标进程。
static inline bool ptebp_current_task_matches(pid_t target_pid)
{
	return target_pid > 0 && (target_pid == current->tgid || target_pid == current->pid);
}

// 派发精确命中的 PTEBP 回调，让断点 handler 记录或修改寄存器现场。
static inline void arm64_ptedbg_monitor_dispatch_hit(struct pt_regs *regs, struct bp_point *point)
{
	if (!regs || !point || !point->on_hit)
		return;

	point->on_hit((void *)regs, (void *)point);
}

// 当前 PTEBP 只支持执行断点，匹配粒度固定为 4 字节指令。
static inline bool ptebp_addr_matches(struct bp_point *point, uint64_t fault_addr)
{
	if (!point || !point->hit_addr)
		return false;

	return (point->hit_addr & ~0x3ULL) == (fault_addr & ~0x3ULL);
}

// do_mem_abort inline hook 回调，只接管 lower EL instruction permission fault。
static int work_trampoline_ptebp(struct pt_regs *hook_regs)
{
	unsigned long flags;
	uint64_t esr;
	uint64_t ec;
	uint64_t ifsc;
	uint64_t fault_addr;
	uint64_t fault_page;
	uint64_t old_pc;
	int point_slot;
	int handled = 0;
	bool exact_hit = false;
	bool page_hit = false;
	struct break_point *info;
	struct ptebp_hit hit;
	struct pt_regs *regs;

	if (!hook_regs)
		return 0;

	// do_mem_abort(far, esr, regs) 的前三个参数保存在 hook_regs->regs[0..2]。
	esr = hook_regs->regs[1];
	regs = (struct pt_regs *)hook_regs->regs[2];
	if (!regs)
		return 0;

	// 只接管 lower EL 的指令 permission fault；读写断点已经明确不再支持。
	ec = (esr >> 26) & 0x3f;
	if (ec != PTEBP_ESR_EC_IABT_LOW)
		return 0;

	ifsc = esr & PTEBP_ESR_FSC_MASK;
	if (ifsc < PTEBP_ESR_FSC_PERM_MIN || ifsc > PTEBP_ESR_FSC_PERM_MAX)
		return 0;

	fault_addr = regs->pc & ~0x3ULL;
	fault_page = fault_addr & PAGE_MASK;
	__builtin_memset(&hit, 0, sizeof(hit));

	spin_lock_irqsave(&g_ptebp_lock, flags);
	info = g_ptebp_info;
	if (!info || !ptebp_current_task_matches(info->pid))
	{
		// 不是目标进程/线程触发的异常，必须继续原内核页异常处理。
		spin_unlock_irqrestore(&g_ptebp_lock, flags);
		return 0;
	}

	page_hit = arm64_dbi_ghost_lookup_pc_managed(info->pid, fault_page, fault_addr, &hit.ghost_pc);
	if (page_hit)
	{
		// 只有命中已 armed 的 PTEBP 页，才可能是我们故意制造的执行权限异常。
		for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
		{
			struct bp_point *point = &info->points[point_slot];

			if (!ptebp_point_is_active(point) ||
				(point->hit_addr & PAGE_MASK) != fault_page)
				continue;

			if (ptebp_addr_matches(point, fault_addr))
			{
				hit.point = point;
				exact_hit = true;
				break;
			}
		}
	}
	spin_unlock_irqrestore(&g_ptebp_lock, flags);

	if (exact_hit)
	{
		old_pc = regs->pc;
		arm64_ptedbg_monitor_dispatch_hit(regs, hit.point);
		// 回调如果改了 PC，说明它已经决定后续执行位置；否则执行 ghost 中的原指令。
		if ((regs->pc & ~0x3ULL) != (old_pc & ~0x3ULL))
			handled = 1;
		else
		{
			if (hit.ghost_pc)
			{
				regs->pc = hit.ghost_pc;
				handled = 1;
			}
		}
	}
	else if (page_hit)
	{
		// 同页非目标指令也会被目标页 UXN 拦住，直接跳到 DBI ghost 对应位置。
		if (hit.ghost_pc)
		{
			regs->pc = hit.ghost_pc;
			handled = 1;
		}
	}

	if (handled)
		regs->regs[0] = 0;

	// handled=1 表示 PTEBP 已改 PC 并接管异常；handled=0 则继续原 do_mem_abort。
	return handled;
}

static struct hook_entry g_ptebp_hooks[] = {
	HOOK_ENTRY("do_mem_abort", work_trampoline_ptebp),
};

// 卸载 PTEBP monitor，恢复所有仍 armed 的页面并移除 do_mem_abort hook。
static inline void arm64_ptedbg_monitor_remove(void)
{
	unsigned long flags;
	pid_t pid = 0;

	mutex_lock(&g_ptebp_remove_mutex);

	spin_lock_irqsave(&g_ptebp_lock, flags);
	if (g_ptebp_info)
		pid = g_ptebp_info->pid;
	g_ptebp_info = NULL;
	spin_unlock_irqrestore(&g_ptebp_lock, flags);

	arm64_dbi_ghost_remove_all_managed(pid);

	inline_hook_remove(g_ptebp_hooks);
	mutex_unlock(&g_ptebp_remove_mutex);
}

// 安装目标进程的 PTEBP monitor，并为每个目标页记录原始 PTE 状态。
static inline int arm64_ptedbg_monitor_set(struct break_point *info)
{
	int status;
	int point_slot;
	int page_count = 0;
	unsigned long flags;

	// 检查 info 里有没有有效执行断点，只接受 BP_BREAKPOINT_X
	if (!ptebp_info_has_active_point(info))
		return -EINVAL;
	//   清掉旧 PTEBP 状态，恢复旧页面 PTE，移除旧 hook
	arm64_ptedbg_monitor_remove();
	// 安装 do_mem_abort hook
	status = inline_hook_install(g_ptebp_hooks);
	if (status)
		return status;

	// 传递断点配置给全局，方便fault和其他所有代码访问
	spin_lock_irqsave(&g_ptebp_lock, flags);
	g_ptebp_info = info;
	spin_unlock_irqrestore(&g_ptebp_lock, flags);

	// 遍历 info->points[]，对每个执行断点，根据 hit_addr 算出 target_page
	for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
	{
		struct bp_point *point = &info->points[point_slot];
		uint64_t target_page;

		if (!ptebp_point_is_active(point))
			continue;
		// 算所在页
		target_page = point->hit_addr & PAGE_MASK;

		status = arm64_dbi_ghost_install_managed(info->pid, target_page, NULL, NULL);
		if (status)
			goto err_out;
		page_count++;
	}

	if (page_count == 0)
	{
		status = -EINVAL;
		goto err_out;
	}

	return 0;

err_out:
	arm64_ptedbg_monitor_remove();
	return status;
}

#endif // ARM64_PTEDBG_H
