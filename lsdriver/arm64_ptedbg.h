#ifndef ARM64_PTEDBG_H
#define ARM64_PTEDBG_H

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/spinlock.h>
#include <linux/thread_info.h>
#include <asm/pgtable.h>
#include <asm/ptrace.h>
#include <asm/thread_info.h>
#include <asm/tlbflush.h>

#include "export_fun.h"
#include "inline_hook_frame.h"
#include "io_struct.h"

#ifndef PTE_UXN
#define PTEBP_UXN (_AT(pteval_t, 1) << 54)
#else
#define PTEBP_UXN PTE_UXN
#endif

#define PTEBP_ESR_EC_IABT_LOW 0x20
#define PTEBP_ESR_EC_DABT_LOW 0x24
#define PTEBP_ESR_FSC_MASK 0x3f
#define PTEBP_ESR_FSC_PERM_MIN 0x0c
#define PTEBP_ESR_FSC_PERM_MAX 0x0f
#define PTEBP_ESR_DABT_WNR (1ULL << 6)

#ifndef DBG_MDSCR_SS
#define PTEBP_DBG_MDSCR_SS (1ULL << 0)
#else
#define PTEBP_DBG_MDSCR_SS DBG_MDSCR_SS
#endif

#ifndef DBG_MDSCR_MDE
#define PTEBP_DBG_MDSCR_MDE (1ULL << 15)
#else
#define PTEBP_DBG_MDSCR_MDE DBG_MDSCR_MDE
#endif

#ifndef DBG_SPSR_SS
#define PTEBP_DBG_SPSR_SS (1ULL << 21)
#else
#define PTEBP_DBG_SPSR_SS DBG_SPSR_SS
#endif

enum ptebp_fault_type
{
	PTEBP_FAULT_EXEC = BP_BREAKPOINT_X,
	PTEBP_FAULT_READ = BP_BREAKPOINT_R,
	PTEBP_FAULT_WRITE = BP_BREAKPOINT_W,
};

struct ptebp_page_state
{
	// 每个槽记录一个已布防的用户页。PTEBP 是页粒度，同一页上多个断点共用这一份状态。
	bool used;
	// armed=true 表示当前 PTE 仍是断点权限；临时放行单步期间会置为 false。
	bool armed;
	uint64_t target_page;
	// saved_pte 永远保存原始 PTE，恢复执行和重新布防都以它为基准。
	pteval_t saved_pte;
	uint64_t fault_hits;
};

struct ptebp_hit
{
	// do_mem_abort 阶段的临时命中信息，只在本次页异常处理过程中使用。
	pid_t pid;
	struct bp_point *point;
	uint64_t target_page;
	pteval_t saved_pte;
};

struct ptebp_step_state
{
	// pending 记录把“页异常阶段”传递到下一次“单步异常阶段”。
	bool used;
	// 如果目标线程原本就开着 single-step，PTEBP 重布防后要把它恢复回去。
	bool had_single_step;
	pid_t pid;
	// pending 必须绑定线程 id，避免同进程其他线程的 single-step 被误吞。
	pid_t tid;
	uint64_t target_page;
	pteval_t saved_pte;
};

static struct break_point *g_ptebp_info;
static struct ptebp_page_state g_ptebp_page_states[BP_CONFIG_MAX];
static struct ptebp_step_state g_ptebp_step_states[BP_CONFIG_MAX];
static DEFINE_SPINLOCK(g_ptebp_lock);

// 判断单个 PTEBP 点位是否具备安装和派发条件。
static inline bool ptebp_point_is_active(struct bp_point *point)
{
	return point && point->hit_addr != 0 && point->on_hit;
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

// 在已持有 g_ptebp_lock 的情况下查找目标页状态。
static inline struct ptebp_page_state *ptebp_find_page_locked(uint64_t target_page)
{
	int page_slot;

	for (page_slot = 0; page_slot < BP_CONFIG_MAX; page_slot++)
	{
		if (g_ptebp_page_states[page_slot].used &&
			g_ptebp_page_states[page_slot].target_page == target_page)
			return &g_ptebp_page_states[page_slot];
	}

	return NULL;
}

// 在已持有 g_ptebp_lock 的情况下分配一个目标页状态槽。
static inline struct ptebp_page_state *ptebp_alloc_page_locked(void)
{
	int page_slot;

	for (page_slot = 0; page_slot < BP_CONFIG_MAX; page_slot++)
	{
		if (!g_ptebp_page_states[page_slot].used)
			return &g_ptebp_page_states[page_slot];
	}

	return NULL;
}

// 根据同页断点类型计算需要写入的 PTE 权限。
static inline pteval_t ptebp_build_armed_pte(pteval_t saved_pte, int page_type_mask)
{
	pteval_t armed_pte = saved_pte;

	if (page_type_mask & BP_BREAKPOINT_X)
		armed_pte |= PTEBP_UXN;
	if (page_type_mask & BP_BREAKPOINT_R)
		armed_pte &= ~PTE_USER;
	else if (page_type_mask & BP_BREAKPOINT_W)
		armed_pte |= PTE_RDONLY;

	return armed_pte;
}

// 汇总同一进程同一页的断点类型，断点列表只来自 break_point.points。
static inline int ptebp_collect_page_type_mask(struct break_point *info, uint64_t target_page)
{
	int point_slot;
	int page_type_mask = 0;

	if (!info)
		return 0;

	for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
	{
		struct bp_point *point = &info->points[point_slot];

		if (!ptebp_point_is_active(point) ||
			(point->hit_addr & PAGE_MASK) != target_page)
			continue;

		page_type_mask |= point->bt;
	}

	return page_type_mask & BP_BREAKPOINT_INVALID;
}

// 判断当前线程或线程组是否属于目标进程。
static inline bool ptebp_current_task_matches(pid_t target_pid)
{
	return target_pid > 0 && (target_pid == current->tgid || target_pid == current->pid);
}

// 写入用户页 PTE 并在成功后刷新 TLB。
static inline int ptebp_write_user_pte_value(struct mm_struct *mm, uint64_t addr, pteval_t new_pte)
{
	int status = write_user_pte_value(mm, addr, new_pte);

	if (!status)
		flush_tlb_all();

	return status;
}

// 打开当前异常返回路径的一次性用户态 single-step。
static inline void ptebp_enable_single_step(struct pt_regs *regs)
{
	if (!regs)
		return;

	// 不直接写 MDSCR_EL1.SS；让异常返回路径按内核约定打开用户态单步。
	set_ti_thread_flag(task_thread_info(current), TIF_SINGLESTEP);
	regs->pstate |= PTEBP_DBG_SPSR_SS;
}

// 关闭当前线程的 single-step 状态。
static inline void ptebp_disable_single_step(struct pt_regs *regs, bool keep_single_step)
{
	if (keep_single_step)
	{
		// PTEBP 只撤销自己临时打开的单步；原本存在的调试单步必须保留。
		if (regs)
			regs->pstate |= PTEBP_DBG_SPSR_SS;
		set_ti_thread_flag(task_thread_info(current), TIF_SINGLESTEP);
		return;
	}

	if (regs)
		regs->pstate &= ~PTEBP_DBG_SPSR_SS;
	clear_ti_thread_flag(task_thread_info(current), TIF_SINGLESTEP);
}

// 记录当前线程需要在 single-step 命中后重布防的页面。
static inline int ptebp_record_step_pending(pid_t pid, uint64_t target_page, pteval_t saved_pte)
{
	int step_slot;
	int free_slot = -1;
	unsigned long flags;

	// 页异常返回到用户态后，本函数的局部变量都会失效；pending 保存重布防所需的信息。
	spin_lock_irqsave(&g_ptebp_lock, flags);
	for (step_slot = 0; step_slot < BP_CONFIG_MAX; step_slot++)
	{
		struct ptebp_step_state *step = &g_ptebp_step_states[step_slot];

		if (!step->used)
		{
			if (free_slot < 0)
				free_slot = step_slot;
			continue;
		}

		if (step->tid == current->pid)
		{
			// 同一线程重复触发时覆盖旧 pending，保证 single-step 回来时只处理最新放行页。
			step->had_single_step = test_ti_thread_flag(task_thread_info(current), TIF_SINGLESTEP);
			step->pid = pid;
			step->target_page = target_page;
			step->saved_pte = saved_pte;
			spin_unlock_irqrestore(&g_ptebp_lock, flags);
			return 0;
		}
	}

	if (free_slot < 0)
	{
		spin_unlock_irqrestore(&g_ptebp_lock, flags);
		return -ENOSPC;
	}

	g_ptebp_step_states[free_slot].used = true;
	g_ptebp_step_states[free_slot].had_single_step = test_ti_thread_flag(task_thread_info(current), TIF_SINGLESTEP);
	g_ptebp_step_states[free_slot].pid = pid;
	g_ptebp_step_states[free_slot].tid = current->pid;
	g_ptebp_step_states[free_slot].target_page = target_page;
	g_ptebp_step_states[free_slot].saved_pte = saved_pte;
	spin_unlock_irqrestore(&g_ptebp_lock, flags);
	return 0;
}

// 取出并清空当前线程的 pending single-step 状态。
static inline bool ptebp_take_step_pending(struct ptebp_step_state *out_step)
{
	int step_slot;
	unsigned long flags;

	if (!out_step)
		return false;

	spin_lock_irqsave(&g_ptebp_lock, flags);
	for (step_slot = 0; step_slot < BP_CONFIG_MAX; step_slot++)
	{
		struct ptebp_step_state *step = &g_ptebp_step_states[step_slot];

		if (!step->used || step->tid != current->pid)
			continue;

		// 成功取出 pending 才说明这次 single-step 是 PTEBP 自己制造的，可以吞掉。
		*out_step = *step;
		__builtin_memset(step, 0, sizeof(*step));
		spin_unlock_irqrestore(&g_ptebp_lock, flags);
		return true;
	}
	spin_unlock_irqrestore(&g_ptebp_lock, flags);
	return false;
}

// 清理指定线程尚未消费的 pending single-step 状态。
static inline void ptebp_clear_step_pending(pid_t tid)
{
	int step_slot;
	unsigned long flags;

	spin_lock_irqsave(&g_ptebp_lock, flags);
	for (step_slot = 0; step_slot < BP_CONFIG_MAX; step_slot++)
	{
		if (g_ptebp_step_states[step_slot].used &&
			g_ptebp_step_states[step_slot].tid == tid)
			__builtin_memset(&g_ptebp_step_states[step_slot], 0, sizeof(g_ptebp_step_states[step_slot]));
	}
	spin_unlock_irqrestore(&g_ptebp_lock, flags);
}

// 给目标进程的指定用户页设置对应 PTEBP 权限，并保存原始 PTE 供卸载或兜底恢复使用。
static inline int ptebp_set_page_prot(pid_t pid, uint64_t target_page, int page_type_mask, pteval_t *saved_pte)
{
	int status;
	struct mm_struct *mm;
	pteval_t old_pte = 0;
	pteval_t armed_pte;

	mm = get_mm_by_pid(pid);
	if (!mm)
		return -ESRCH;

	mmap_read_lock(mm);
	status = read_user_pte_value(mm, target_page, &old_pte);
	if (!status)
	{
		armed_pte = ptebp_build_armed_pte(old_pte, page_type_mask);
		if (armed_pte == old_pte)
			status = -EACCES;
		else
			status = ptebp_write_user_pte_value(mm, target_page, armed_pte);
	}
	mmap_read_unlock(mm);
	mmput(mm);

	if (!status && saved_pte)
		*saved_pte = old_pte;

	return status;
}

// 在已持有 mm 的情况下，把指定用户页恢复为保存下来的原始 PTE。
static inline int ptebp_restore_pte_for_mm(struct mm_struct *mm, uint64_t target_page, pteval_t saved_pte)
{
	return ptebp_write_user_pte_value(mm, target_page, saved_pte);
}

// 在已保存原始 PTE 的页面上重新写入当前同页断点需要的 PTEBP 权限。
static inline int ptebp_rearm_page(pid_t pid, uint64_t target_page, pteval_t saved_pte)
{
	struct break_point *info;
	int page_type_mask;
	struct mm_struct *mm = current->mm;
	unsigned long flags;
	int status;

	if (!mm)
		return -ESRCH;

	spin_lock_irqsave(&g_ptebp_lock, flags);
	info = g_ptebp_info;
	if (!info || info->pid != pid || !ptebp_current_task_matches(pid))
	{
		spin_unlock_irqrestore(&g_ptebp_lock, flags);
		return -ESRCH;
	}
	page_type_mask = ptebp_collect_page_type_mask(info, target_page);
	spin_unlock_irqrestore(&g_ptebp_lock, flags);

	if (!page_type_mask)
		return -EINVAL;

	mmap_read_lock(mm);
	status = ptebp_restore_pte_for_mm(mm, target_page, ptebp_build_armed_pte(saved_pte, page_type_mask));
	mmap_read_unlock(mm);
	return status;
}

// 根据 pid 获取目标进程 mm，并恢复指定用户页的原始 PTE。
static inline int ptebp_restore_pte_for_pid(pid_t pid, uint64_t target_page, pteval_t saved_pte)
{
	int status;
	struct mm_struct *mm;

	mm = get_mm_by_pid(pid);
	if (!mm)
		return -ESRCH;

	mmap_read_lock(mm);
	status = ptebp_restore_pte_for_mm(mm, target_page, saved_pte);
	mmap_read_unlock(mm);
	mmput(mm);
	return status;
}

// 更新目标页的 armed 标记，记录当前 PTEBP 权限是否仍处于布防状态。
static inline void ptebp_mark_page_armed(pid_t pid, uint64_t target_page, bool armed)
{
	struct break_point *info;
	struct ptebp_page_state *page;
	unsigned long flags;

	spin_lock_irqsave(&g_ptebp_lock, flags);
	info = g_ptebp_info;
	page = ptebp_find_page_locked(target_page);
	if (info && info->pid == pid && page)
		page->armed = armed;
	spin_unlock_irqrestore(&g_ptebp_lock, flags);
}

// 将同一目标页的 PTEBP 页状态标记为已解除布防，避免临时放行期间重复处理。
static inline void ptebp_mark_page_disarmed(pid_t pid, uint64_t target_page)
{
	ptebp_mark_page_armed(pid, target_page, false);
}

// 在当前进程上下文中恢复 fault 页原始 PTE，并解除该页对应的 PTEBP 页状态。
static inline int ptebp_restore_page_current(pid_t pid, uint64_t target_page)
{
	int status;
	bool found = false;
	pteval_t saved_pte = 0;
	struct mm_struct *mm = current->mm;
	struct break_point *info;
	struct ptebp_page_state *page;
	unsigned long flags;

	if (!mm)
		return -ESRCH;

	spin_lock_irqsave(&g_ptebp_lock, flags);
	info = g_ptebp_info;
	page = ptebp_find_page_locked(target_page);
	if (info && info->pid == pid && page && page->armed)
	{
		saved_pte = page->saved_pte;
		found = true;
	}
	spin_unlock_irqrestore(&g_ptebp_lock, flags);

	if (!found)
		return -ENOENT;

	mmap_read_lock(mm);
	status = ptebp_restore_pte_for_mm(mm, target_page, saved_pte);
	mmap_read_unlock(mm);
	if (status)
		return -EFAULT;

	ptebp_mark_page_disarmed(pid, target_page);
	return 0;
}

// 派发精确命中的 PTEBP 回调，让断点 handler 记录或修改寄存器现场。
static inline void arm64_ptedbg_monitor_dispatch_hit(struct pt_regs *regs, struct bp_point *point)
{
	if (!regs || !point || !point->on_hit)
		return;

	point->on_hit((void *)regs, (void *)point);
}

// 判断本次异常类型是否符合断点声明的执行、读或写访问类型。
static inline bool ptebp_access_matches(struct bp_point *point, enum ptebp_fault_type fault_type)
{
	if (!point)
		return false;

	return !!(point->bt & fault_type);
}

// 判断 fault 地址是否落在断点地址和长度覆盖的范围内。
static inline bool ptebp_addr_matches(struct bp_point *point, uint64_t fault_addr, enum ptebp_fault_type fault_type)
{
	uint64_t target_addr;
	uint64_t target_end;
	uint64_t fault_base;
	uint64_t length;

	if (!point || !point->hit_addr)
		return false;

	if (fault_type == PTEBP_FAULT_EXEC)
		return (point->hit_addr & ~0x3ULL) == (fault_addr & ~0x3ULL);

	target_addr = point->hit_addr;
	length = point->bl ? point->bl : BP_BREAKPOINT_LEN_1;
	target_end = target_addr + length;
	fault_base = fault_addr;

	return fault_base >= target_addr && fault_base < target_end;
}
// PTE 是页粒度，同页非目标指令也会触发异常；这里统一临时放行一条，再由 single-step 回调重布防。
static inline int ptebp_begin_single_step(struct pt_regs *regs, struct ptebp_hit *hit)
{
	int status;

	if (!regs || !hit)
		return -EINVAL;

	status = ptebp_record_step_pending(hit->pid, hit->target_page, hit->saved_pte);
	if (status)
		return status;

	// 先记录 pending，再恢复 PTE。否则单步异常回来时不知道该重布防哪一页。
	status = ptebp_restore_page_current(hit->pid, hit->target_page);
	if (status)
	{
		// 恢复 PTE 失败时不能留下 pending，否则后续 unrelated single-step 会被误认为 PTEBP 的。
		ptebp_clear_step_pending(current->pid);
		return status;
	}

	// 异常返回后，用户态重新执行刚才 fault 的那条指令；执行完一条后进入 single_step_handler。
	ptebp_enable_single_step(regs);
	return 0;
}

// do_mem_abort inline hook 回调，识别执行或数据访问权限 fault 并完成 PTEBP 命中处理。
static int work_trampoline_ptebp(struct pt_regs *hook_regs)
{
	unsigned long flags;
	uint64_t far;
	uint64_t esr;
	uint64_t ec;
	uint64_t ifsc;
	uint64_t fault_addr;
	uint64_t fault_page;
	uint64_t old_pc;
	enum ptebp_fault_type fault_type;
	int point_slot;
	int handled = 0;
	bool exact_hit = false;
	bool page_hit = false;
	struct break_point *info;
	struct ptebp_page_state *page;
	struct ptebp_hit hit;
	struct pt_regs *regs;

	if (!hook_regs)
		return 0;

	// do_mem_abort(far, esr, regs) 的前三个参数保存在 hook_regs->regs[0..2]。
	far = hook_regs->regs[0];
	esr = hook_regs->regs[1];
	regs = (struct pt_regs *)hook_regs->regs[2];
	if (!regs)
		return 0;

	// 只接管 lower EL 的指令/数据 permission fault；其他页异常交回原 do_mem_abort。
	ec = (esr >> 26) & 0x3f;
	if (ec == PTEBP_ESR_EC_IABT_LOW)
		fault_type = PTEBP_FAULT_EXEC;
	else if (ec == PTEBP_ESR_EC_DABT_LOW)
		fault_type = (esr & PTEBP_ESR_DABT_WNR) ? PTEBP_FAULT_WRITE : PTEBP_FAULT_READ;
	else
		return 0;

	ifsc = esr & PTEBP_ESR_FSC_MASK;
	if (ifsc < PTEBP_ESR_FSC_PERM_MIN || ifsc > PTEBP_ESR_FSC_PERM_MAX)
		return 0;

	// 执行 fault 的地址来自 regs->pc；数据 fault 的地址来自 FAR。
	fault_addr = far & ~0x3ULL;
	if (fault_type == PTEBP_FAULT_EXEC)
		fault_addr = regs->pc & ~0x3ULL;
	fault_page = far & PAGE_MASK;
	if (fault_type == PTEBP_FAULT_EXEC)
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

	page = ptebp_find_page_locked(fault_page);
	if (page && page->armed)
	{
		// 只有命中已 armed 的 PTEBP 页，才可能是我们故意制造的权限异常。
		hit.pid = info->pid;
		hit.target_page = page->target_page;
		hit.saved_pte = page->saved_pte;

		for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
		{
			struct bp_point *point = &info->points[point_slot];

			if (!ptebp_point_is_active(point) ||
				(point->hit_addr & PAGE_MASK) != fault_page)
				continue;

			if (ptebp_access_matches(point, fault_type) &&
				ptebp_addr_matches(point, fault_addr, fault_type))
			{
				hit.point = point;
				page->fault_hits++;
				exact_hit = true;
				break;
			}
		}

		if (!exact_hit)
			page_hit = true;
	}
	spin_unlock_irqrestore(&g_ptebp_lock, flags);

	if (exact_hit)
	{
		old_pc = regs->pc;
		arm64_ptedbg_monitor_dispatch_hit(regs, hit.point);
		// 回调如果改了 PC，说明它已经决定后续执行位置，不需要单步重布防当前指令。
		if ((regs->pc & ~0x3ULL) != (old_pc & ~0x3ULL))
			handled = 1;
		else if (!ptebp_begin_single_step(regs, &hit))
			handled = 1;
	}
	else if (page_hit)
	{
		// 同页非目标访问也会被页权限拦住，必须临时放行一条，不能交给 do_mem_abort 杀进程。
		if (!ptebp_begin_single_step(regs, &hit))
			handled = 1;
	}

	// handled=1 表示 PTEBP 已恢复 PTE/改 PC 并接管异常；handled=0 则继续原 do_mem_abort。
	return handled;
}

// single-step 命中表示刚才临时放行的一条用户指令已经执行完毕，现在恢复 PTEBP 权限。
static int work_trampoline_ptebp_step(struct pt_regs *hook_regs)
{
	int status;
	struct pt_regs *regs;
	struct ptebp_step_state step;

	if (!hook_regs)
		return 0;

	// single_step_handler(regs) 的 regs 参数在 hook_regs->regs[2]。
	regs = (struct pt_regs *)hook_regs->regs[2];
	if (!regs || !ptebp_take_step_pending(&step))
		// 当前线程没有 pending，说明不是 PTEBP 临时放行制造的单步，交回原 handler。
		return 0;

	ptebp_disable_single_step(regs, step.had_single_step);
	status = ptebp_rearm_page(step.pid, step.target_page, step.saved_pte);
	if (!status)
		ptebp_mark_page_armed(step.pid, step.target_page, true);

	// 跳过原 single_step_handler 时要模拟其成功返回值，避免调试异常继续分发到用户态。
	hook_regs->regs[0] = 0;
	return 1;
}

static struct hook_entry g_ptebp_hooks[] = {
	HOOK_ENTRY("do_mem_abort", work_trampoline_ptebp),
	HOOK_ENTRY("single_step_handler", work_trampoline_ptebp_step),
};

// 卸载 PTEBP monitor，恢复所有仍 armed 的页面并移除 do_mem_abort hook。
static inline void arm64_ptedbg_monitor_remove(void)
{
	int page_slot;
	pid_t pid = 0;
	unsigned long flags;
	struct ptebp_page_state page_states[BP_CONFIG_MAX];

	__builtin_memset(page_states, 0, sizeof(page_states));

	spin_lock_irqsave(&g_ptebp_lock, flags);
	if (g_ptebp_info)
		pid = g_ptebp_info->pid;
	for (page_slot = 0; page_slot < BP_CONFIG_MAX; page_slot++)
		page_states[page_slot] = g_ptebp_page_states[page_slot];
	g_ptebp_info = NULL;
	__builtin_memset(g_ptebp_page_states, 0, sizeof(g_ptebp_page_states));
	__builtin_memset(g_ptebp_step_states, 0, sizeof(g_ptebp_step_states));
	spin_unlock_irqrestore(&g_ptebp_lock, flags);

	for (page_slot = 0; page_slot < BP_CONFIG_MAX; page_slot++)
	{
		if (pid > 0 && page_states[page_slot].used && page_states[page_slot].armed)
			ptebp_restore_pte_for_pid(pid, page_states[page_slot].target_page, page_states[page_slot].saved_pte);
	}

	inline_hook_remove(g_ptebp_hooks);
}

// 安装目标进程的 PTEBP monitor，并为每个目标页记录原始 PTE 状态。
static inline int arm64_ptedbg_monitor_set(struct break_point *info)
{
	int status;
	int point_slot;
	int page_count = 0;
	unsigned long flags;

	if (!ptebp_info_has_active_point(info))
		return -EINVAL;

	arm64_ptedbg_monitor_remove();

	status = inline_hook_install(g_ptebp_hooks);
	if (status)
		return status;

	spin_lock_irqsave(&g_ptebp_lock, flags);
	g_ptebp_info = info;
	spin_unlock_irqrestore(&g_ptebp_lock, flags);

	for (point_slot = 0; point_slot < BP_CONFIG_MAX; point_slot++)
	{
		struct bp_point *point = &info->points[point_slot];
		struct ptebp_page_state *page;
		uint64_t target_page;
		pteval_t saved_pte = 0;

		if (!ptebp_point_is_active(point))
			continue;

		target_page = point->hit_addr & PAGE_MASK;

		spin_lock_irqsave(&g_ptebp_lock, flags);
		page = ptebp_find_page_locked(target_page);
		spin_unlock_irqrestore(&g_ptebp_lock, flags);
		if (page)
			continue;

		status = ptebp_set_page_prot(info->pid, target_page,
									 ptebp_collect_page_type_mask(info, target_page),
									 &saved_pte);
		if (status)
			goto err_out;

		spin_lock_irqsave(&g_ptebp_lock, flags);
		page = ptebp_alloc_page_locked();
		if (!page)
		{
			spin_unlock_irqrestore(&g_ptebp_lock, flags);
			status = -ENOSPC;
			goto err_out;
		}
		page->used = true;
		page->armed = true;
		page->target_page = target_page;
		page->saved_pte = saved_pte;
		page->fault_hits = 0;
		spin_unlock_irqrestore(&g_ptebp_lock, flags);
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
