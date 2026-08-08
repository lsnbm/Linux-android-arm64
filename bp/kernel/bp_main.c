// SPDX-License-Identifier: GPL-2.0
/*
 * bp_main.c - ARM64 breakpoint driver (HWBP / PTEBP / STEPBP).
 *
 * Derived from upstream lsdriver (lsnbm/Linux-android-arm64) breakpoint
 * subsystems: break_point.h / arm64_hwdbg.h / arm64_ptedbg.h /
 * arm64_stepdbg.h, with the shared-memory request path replaced by a
 * stable char-device ioctl ABI (include/bp_uapi.h) and a lifecycle-safe
 * session layer (task refcount, mm refcount, PID-reuse guard, exec/exit
 * invalidation, inflight drain). No module hiding, no CFI bypass, fully
 * unloadable.
 *
 * Session rules:
 *  - single active configuration (one tgid) per module instance
 *  - BP_IOC_*_ARM returns a kernel-assigned mm_cookie; STATUS/STOP must
 *    carry it back, otherwise -EPERM
 *  - task identity is verified with tgid + task start_time (PID reuse)
 *  - target exit (do_exit) and exec (do_execveat_common) auto-clean
 *    the active session via a workqueue (kprobe context cannot sleep)
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/kprobes.h>
#include <linux/sched.h>
#include <linux/sched/task.h>
#include <linux/sched/mm.h>
#include <linux/sched/stat.h>
#include <linux/mutex.h>
#include <linux/wait.h>
#include <linux/atomic.h>
#include <linux/workqueue.h>
#include <linux/ktime.h>
#include <linux/slab.h>
#include <linux/pid.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/mmu_notifier.h>
/* mmu-notifier is wired on 5.15+ except the 6.1 API-transition series where
 * concurrent re-arm races the notifier lifecycle (6.1-only); 5.10 and 6.1
 * keep the hit-time PTE match fail-closed fallback. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0) &&     !(LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0) &&       LINUX_VERSION_CODE < KERNEL_VERSION(6, 2, 0))
#define BP_MMU_NOTIFIER 1
#endif

#include "bp_uapi.h"
#include "export_fun.h"
#include "break_point.h"

#define BP_DRIVER_VERSION "1.0.0"

/* ---------------- session state ---------------- */

static DEFINE_MUTEX(g_bp_mutex);	/* serializes arm/stop/status/lifecycle */
static struct task_struct *g_bp_task;	/* target, refcounted */
static struct mm_struct *g_bp_mm;	/* target mm, refcounted */
static int g_bp_tgid;
static u64 g_bp_start_ticks;		/* user-space ticks (clock_t) */
static u64 g_bp_mm_cookie;
static bool g_bp_active;
static u64 g_bp_cookie_counter;

/* debug counters (BP_IOC_INFO) */
static atomic_t g_bp_inflight = ATOMIC_INIT(0);
static DECLARE_WAIT_QUEUE_HEAD(g_bp_drain_wq);
static atomic64_t g_dbg_pte_restored = ATOMIC64_INIT(0);
static atomic64_t g_dbg_refs = ATOMIC64_INIT(0);
static atomic64_t g_dbg_hooks = ATOMIC64_INIT(0);

/* three family configurations (stable storage for the monitor backends) */
static struct break_point g_bp_cfg_hwbp;
static struct break_point g_bp_cfg_ptebp;
static struct break_point g_bp_cfg_stepbp;
static bool g_bp_hwbp_on, g_bp_ptebp_on, g_bp_stepbp_on;

/* lifecycle */
static struct work_struct g_bp_cleanup_work;
#ifdef BP_MMU_NOTIFIER
static struct mmu_notifier g_bp_mn;
static bool g_bp_mn_registered;
static const struct mmu_notifier_ops bp_mmu_ops;
#endif
static struct kprobe g_kp_exit;
static struct kprobe g_kp_exec;
static bool g_kp_exit_reg, g_kp_exec_reg;

/* ---------------- helpers ---------------- */

static void bp_dbg_pte_restore(void)
{
	atomic64_inc(&g_dbg_pte_restored);
}

static void bp_dbg_ref_hold(void)
{
	atomic64_inc(&g_dbg_refs);
}

static void bp_dbg_ref_release(void)
{
	atomic64_dec(&g_dbg_refs);
}

static bool bp_ticks_match(struct task_struct *task, u64 user_ticks)
{
	u64 nsec, t, tol;

	if (!task || !user_ticks)
		return false;
	/* /proc/<pid>/stat field 22 (starttime) is reported in USER_HZ (100)
	 * clock ticks regardless of CONFIG_HZ; convert back to nanoseconds. */
	nsec = div_u64(user_ticks * NSEC_PER_SEC, 100);
	t = task->start_time;
	tol = NSEC_PER_SEC / 50; /* +-20ms */
	return t >= nsec - tol && t <= nsec + tol;
}

static struct task_struct *bp_find_target(int tgid, u64 start_ticks,
					  struct mm_struct **mm_out)
{
	struct pid *pid;
	struct task_struct *task;
	struct mm_struct *mm = NULL;

	*mm_out = NULL;
	if (tgid <= 0)
		return NULL;
	pid = find_get_pid(tgid);
	if (!pid)
		return NULL;
	task = get_pid_task(pid, PIDTYPE_TGID);
	put_pid(pid);
	if (!task)
		return NULL;
	if (!bp_ticks_match(task, start_ticks)) {
			put_task_struct(task);
		return NULL;
	}
	mm = get_task_mm(task);
	if (!mm) {
		put_task_struct(task);
		return NULL;
	}
	*mm_out = mm;
	return task;
}

static u64 bp_next_cookie(void)
{
	g_bp_cookie_counter++;
	return (u64)ktime_get_ns() ^ (g_bp_cookie_counter << 32) ^
	       (u64)(unsigned long)&g_bp_cookie_counter;
}

static void bp_drain_inflight(void)
{
	while (atomic_read(&g_bp_inflight)) {
		wait_event_timeout(g_bp_drain_wq,
				   atomic_read(&g_bp_inflight) == 0,
				   HZ / 10);
	}
}

/* begin a session: validate identity, take refs, assign cookie */
static int bp_begin_session(const struct bp_session_v1 *u)
{
	struct task_struct *task;
	struct mm_struct *mm;

	if (!g_bp_active) {
		if (u->expected_mm_cookie != 0) {
				return -EINVAL;
		}
	} else {
		/* same tgid re-arm replaces the session (concurrent controllers);
		 * different tgid is refused. Cookie freshness is enforced on
		 * STATUS/STOP, not on re-arm. */
		if (g_bp_tgid != u->tgid)
			return -EBUSY;
	}
	task = bp_find_target(u->tgid, u->task_start_ticks, &mm);
	if (!task)
		return -ESRCH;

	if (g_bp_active) {
		/* replace session for same tgid */
		if (g_bp_task) {
			put_task_struct(g_bp_task);
			bp_dbg_ref_release();
		}
		if (g_bp_mm) {
			mmput(g_bp_mm);
			bp_dbg_ref_release();
		}
	}
	g_bp_task = task;
	g_bp_mm = mm;
	g_bp_tgid = u->tgid;
#ifdef BP_MMU_NOTIFIER
	if (!g_bp_mn_registered) {
		g_bp_mn.ops = &bp_mmu_ops;
		if (mmu_notifier_register(&g_bp_mn, mm) == 0)
			g_bp_mn_registered = true;
	}
#endif
	g_bp_start_ticks = u->task_start_ticks;
	g_bp_mm_cookie = bp_next_cookie();
	g_bp_active = true;
	bp_dbg_ref_hold();
	bp_dbg_ref_hold();
	return 0;
}

static void bp_release_session_locked(void)
{
	if (!g_bp_active)
		return;
	if (g_bp_task) {
		put_task_struct(g_bp_task);
		bp_dbg_ref_release();
		g_bp_task = NULL;
	}
	if (g_bp_mm) {
#ifdef BP_MMU_NOTIFIER
		if (g_bp_mn_registered && g_bp_mn.mm == g_bp_mm) {
			mmu_notifier_unregister(&g_bp_mn, g_bp_mm);
			g_bp_mn_registered = false;
		}
#endif
		mmput(g_bp_mm);
		bp_dbg_ref_release();
		g_bp_mm = NULL;
	}
	g_bp_active = false;
	g_bp_tgid = 0;
	g_bp_start_ticks = 0;
	g_bp_mm_cookie = 0;
}

/* validate a STATUS/STOP request against the active session;
 * tgid/start_ticks == 0 skips that identity check (cookie is mandatory) */
static int bp_check_session_meta(int tgid, u64 start_ticks, u64 cookie)
{
	if (!g_bp_active)
		return -ENOENT;
	if (cookie != g_bp_mm_cookie)
		return -EPERM;
	if (tgid && tgid != g_bp_tgid)
		return -EPERM;
	if (start_ticks && start_ticks != g_bp_start_ticks)
		return -EPERM;
	return 0;
}

/* on_hit wrapper: inflight accounting for quiesce/drain */
static void bp_on_hit_wrapper(void *regs, void *fp_regs, void *hit_point)
{
	atomic_inc(&g_bp_inflight);
	sample_hbp_handler((struct pt_regs *)regs, (struct fp_regs *)fp_regs,
			   (struct bp_point *)hit_point);
	atomic_dec(&g_bp_inflight);
	wake_up_all(&g_bp_drain_wq);
}

/* PTEBP dedicated hit callback: write S0 low 32 bits, keep Q0 high 96 bits */
static void bp_pte_on_hit(void *regs, void *fp_regs, void *hit_point)
{
	struct fp_regs *f = (struct fp_regs *)fp_regs;
	struct bp_point *pt = (struct bp_point *)hit_point;
	__uint128_t s0;

	(void)regs;
	atomic_inc(&g_bp_inflight);
	s0 = (__uint128_t)(u32)(pt->records[0].q0 & 0xffffffffULL);
	f->q[0] = (f->q[0] & ~((__uint128_t)0xffffffffULL)) | s0;
	pt->records[0].hit_count++;
	atomic_dec(&g_bp_inflight);
	wake_up_all(&g_bp_drain_wq);
}

/* ---------------- family arm/stop primitives ---------------- */

static int bp_hwbp_arm_locked(const struct bp_hwbp_arm_v1 *u)
{
	struct bp_point *pt;

	if (u->bt != BP_BT_X && u->bt != BP_BT_R && u->bt != BP_BT_W &&
	    u->bt != BP_BT_RW)
		return -EINVAL;
	if (u->bl < 1 || u->bl > 8)
		return -EINVAL;
	if (u->bs > BP_BS_ALL_THREADS)
		return -EINVAL;
	if (!u->hit_addr || u->hit_addr >= TASK_SIZE)
		return -EINVAL;

	if (g_bp_hwbp_on) {
		stop_task_run_monitor();
		atomic64_dec(&g_dbg_hooks);
	}
	memset(&g_bp_cfg_hwbp, 0, sizeof(g_bp_cfg_hwbp));
	g_bp_cfg_hwbp.tgid = u->sess.tgid;
	pt = &g_bp_cfg_hwbp.points[0];
	pt->bt = u->bt;
	pt->bl = u->bl;
	pt->bs = u->bs;
	pt->hit_addr = u->hit_addr;
	memcpy(pt->records[0].mask, u->mask, sizeof(u->mask));
	pt->records[0].pc = u->hit_addr;
	pt->records[0].x0 = u->seed_x0;
	pt->record_count = 1;
	pt->on_hit = bp_on_hit_wrapper;

	{
		int ret = start_task_run_monitor(&g_bp_cfg_hwbp);
		if (ret)
			return ret;
	}
	g_bp_hwbp_on = true;
	atomic64_inc(&g_dbg_hooks);
	return 0;
}

static int bp_hwbp_stop_locked(void)
{
	if (!g_bp_hwbp_on)
		return -ENOENT;
	stop_task_run_monitor();
	memset(&g_bp_cfg_hwbp, 0, sizeof(g_bp_cfg_hwbp));
	g_bp_hwbp_on = false;
	atomic64_dec(&g_dbg_hooks);
	return 0;
}

static int bp_ptebp_arm_locked(const struct bp_pte_arm_v1 *u)
{
	struct bp_point *pt;

	if (!u->pc || u->pc >= TASK_SIZE) {
			return -EINVAL;
	}

	if (g_bp_ptebp_on) {
		stop_ptebp_monitor();
		atomic64_dec(&g_dbg_hooks);
	}
	memset(&g_bp_cfg_ptebp, 0, sizeof(g_bp_cfg_ptebp));
	g_bp_cfg_ptebp.tgid = u->sess.tgid;
	pt = &g_bp_cfg_ptebp.points[0];
	pt->bt = BP_BREAKPOINT_X;
	pt->bl = BP_BREAKPOINT_LEN_4;
	pt->bs = BP_SCOPE_ALL_THREADS;
	pt->hit_addr = u->pc;
	pt->records[0].pc = u->pc;
	pt->records[0].q0 = (__uint128_t)u->s0_bits;
	pt->record_count = 1;
	pt->on_hit = bp_pte_on_hit;

	{
		int ret = start_ptebp_monitor(&g_bp_cfg_ptebp);
		if (ret)
			return ret;
	}
	g_bp_ptebp_on = true;
	atomic64_inc(&g_dbg_hooks);
	return 0;
}

static int bp_ptebp_stop_locked(void)
{
	if (!g_bp_ptebp_on)
		return -ENOENT;
	stop_ptebp_monitor();
	bp_dbg_pte_restore();
	memset(&g_bp_cfg_ptebp, 0, sizeof(g_bp_cfg_ptebp));
	g_bp_ptebp_on = false;
	if (atomic64_read(&g_dbg_hooks) > 0)
		atomic64_dec(&g_dbg_hooks);
	return 0;
}

static int bp_stepbp_arm_locked(const struct bp_step_arm_v1 *u)
{
	struct bp_point *pt;

	if (u->bt != BP_BT_X)
		return -EINVAL;
	if (!u->pc || u->pc >= TASK_SIZE)
		return -EINVAL;

	if (g_bp_stepbp_on) {
		stop_stepbp_monitor();
		atomic64_dec(&g_dbg_hooks);
	}
	memset(&g_bp_cfg_stepbp, 0, sizeof(g_bp_cfg_stepbp));
	g_bp_cfg_stepbp.tgid = u->sess.tgid;
	pt = &g_bp_cfg_stepbp.points[0];
	pt->bt = BP_BREAKPOINT_X;
	pt->bl = BP_BREAKPOINT_LEN_4;
	pt->bs = BP_SCOPE_ALL_THREADS;
	pt->hit_addr = u->pc;
	pt->records[0].pc = u->pc;
	pt->record_count = 1;
	pt->on_hit = bp_on_hit_wrapper;

	{
		int ret = start_stepbp_monitor(&g_bp_cfg_stepbp);
		if (ret)
			return ret;
	}
	g_bp_stepbp_on = true;
	atomic64_inc(&g_dbg_hooks);
	return 0;
}

static int bp_stepbp_stop_locked(void)
{
	if (!g_bp_stepbp_on)
		return -ENOENT;
	stop_stepbp_monitor();
	memset(&g_bp_cfg_stepbp, 0, sizeof(g_bp_cfg_stepbp));
	g_bp_stepbp_on = false;
	atomic64_dec(&g_dbg_hooks);
	return 0;
}

/* stop every active family and release the session */
static void bp_stop_all_locked(void)
{
	if (g_bp_hwbp_on)
		bp_hwbp_stop_locked();
	if (g_bp_ptebp_on)
		bp_ptebp_stop_locked();
	if (g_bp_stepbp_on)
		bp_stepbp_stop_locked();
	bp_drain_inflight();
	bp_release_session_locked();
}

/* ---------------- ioctl handlers ---------------- */

static long bp_ioctl_info(struct bp_info_v1 __user *arg)
{
	struct bp_info_v1 out;

	memset(&out, 0, sizeof(out));
	out.abi_major = BP_ABI_MAJOR;
	out.abi_minor = BP_ABI_MINOR;
	out.struct_size = sizeof(out);
	out.feature_flags = BP_FEATURE_HWBP | BP_FEATURE_PTEBP |
			    BP_FEATURE_STEPBP | BP_FEATURE_REG_WRITE;
	out.num_brps = get_brps_num();
	out.num_wrps = get_wrps_num();
	out.debug_active_sessions = g_bp_active ? 1 : 0;
	out.debug_inflight = atomic_read(&g_bp_inflight);
	out.debug_pte_restored_pages = atomic64_read(&g_dbg_pte_restored);
	out.debug_ref_count = atomic64_read(&g_dbg_refs);
	out.debug_hooks_active = atomic64_read(&g_dbg_hooks);
	return copy_to_user(arg, &out, sizeof(out)) ? -EFAULT : 0;
}

static int bp_check_arm_header(const struct bp_session_v1 *u, u32 size)
{
	if (u->abi_major != BP_ABI_MAJOR)
		return -EINVAL;
	if (u->struct_size != size)
		return -EINVAL;
	return 0;
}

static long bp_ioctl_pte_arm(struct bp_pte_arm_v1 __user *arg)
{
	struct bp_pte_arm_v1 u;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	ret = bp_check_arm_header(&u.sess, sizeof(u));
	if (ret)
		return ret;
	mutex_lock(&g_bp_mutex);
	ret = bp_begin_session(&u.sess);
	if (!ret) {
		ret = bp_ptebp_arm_locked(&u);
		if (ret)
			bp_release_session_locked();
	}
	if (!ret) {
		u.sess.mm_cookie = g_bp_mm_cookie;
		if (copy_to_user(arg, &u, sizeof(u)))
			ret = -EFAULT;
	}
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_ioctl_pte_status(struct bp_pte_status_v1 __user *arg)
{
	struct bp_pte_status_v1 u;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	if (u.abi_major != BP_ABI_MAJOR || u.struct_size != sizeof(u))
		return -EINVAL;
	mutex_lock(&g_bp_mutex);
	ret = bp_check_session_meta(u.tgid, u.task_start_ticks, u.mm_cookie);
	if (!ret) {
		if (!g_bp_ptebp_on)
			ret = -ENOENT;
	}
	if (!ret) {
		u.state = g_bp_ptebp_on ? BP_STATE_ARMED : BP_STATE_IDLE;
		u.mm_cookie = g_bp_mm_cookie;
		u.pc = g_bp_cfg_ptebp.points[0].hit_addr;
		u.hit_count = g_bp_cfg_ptebp.points[0].records[0].hit_count;
		if (copy_to_user(arg, &u, sizeof(u)))
			ret = -EFAULT;
	}
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_ioctl_pte_stop(struct bp_pte_stop_v1 __user *arg)
{
	struct bp_pte_stop_v1 u;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	if (u.abi_major != BP_ABI_MAJOR || u.struct_size != sizeof(u))
		return -EINVAL;
	mutex_lock(&g_bp_mutex);
	ret = bp_check_session_meta(0, 0, u.mm_cookie);
	if (!ret)
		ret = bp_ptebp_stop_locked();
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_ioctl_hwbp_arm(struct bp_hwbp_arm_v1 __user *arg)
{
	struct bp_hwbp_arm_v1 u;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	ret = bp_check_arm_header(&u.sess, sizeof(u));
	if (ret)
		return ret;
	mutex_lock(&g_bp_mutex);
	ret = bp_begin_session(&u.sess);
	if (!ret) {
		ret = bp_hwbp_arm_locked(&u);
		if (ret)
			bp_release_session_locked();
	}
	if (!ret) {
		u.sess.mm_cookie = g_bp_mm_cookie;
		u.hit_count = 0;
		u.record_count = g_bp_cfg_hwbp.points[0].record_count;
		for (int ri = 0; ri < u.record_count; ri++)
			u.hit_count += g_bp_cfg_hwbp.points[0].records[ri].hit_count;
		if (copy_to_user(arg, &u, sizeof(u)))
			ret = -EFAULT;
	}
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_ioctl_hwbp_status(struct bp_hwbp_status_v1 __user *arg)
{
	struct bp_hwbp_status_v1 u;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	if (u.abi_major != BP_ABI_MAJOR || u.struct_size != sizeof(u))
		return -EINVAL;
	mutex_lock(&g_bp_mutex);
	ret = bp_check_session_meta(u.tgid, u.task_start_ticks, u.mm_cookie);
	if (!ret) {
		if (!g_bp_hwbp_on)
			ret = -ENOENT;
	}
	if (!ret) {
		u.state = BP_STATE_ARMED;
		u.mm_cookie = g_bp_mm_cookie;
		u.hit_count = 0;
		u.record_count = g_bp_cfg_hwbp.points[0].record_count;
		for (int ri = 0; ri < u.record_count; ri++)
			u.hit_count += g_bp_cfg_hwbp.points[0].records[ri].hit_count;
		if (copy_to_user(arg, &u, sizeof(u)))
			ret = -EFAULT;
	}
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_ioctl_hwbp_stop(struct bp_hwbp_stop_v1 __user *arg)
{
	struct bp_hwbp_stop_v1 u;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	if (u.abi_major != BP_ABI_MAJOR || u.struct_size != sizeof(u))
		return -EINVAL;
	mutex_lock(&g_bp_mutex);
	ret = bp_check_session_meta(0, 0, u.mm_cookie);
	if (!ret)
		ret = bp_hwbp_stop_locked();
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_ioctl_hwbp_record(struct bp_record_v1 __user *arg)
{
	struct bp_record_v1 u;
	struct bp_record *rec;
	__uint128_t q;
	u32 idx;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	if (u.abi_major != BP_ABI_MAJOR || u.struct_size != sizeof(u))
		return -EINVAL;
	idx = u.index;
	if (idx >= BP_RECORD_MAX)
		return -EINVAL;
	mutex_lock(&g_bp_mutex);
	if (!g_bp_active)
		ret = -ENOENT;
	else if (u.mm_cookie != g_bp_mm_cookie)
		ret = -EPERM;
	else if (!g_bp_hwbp_on)
		ret = -ENOENT;
	else {
		ret = 0;
		rec = &g_bp_cfg_hwbp.points[0].records[idx];
		memset(&u, 0, sizeof(u));
		u.abi_major = BP_ABI_MAJOR;
		u.struct_size = sizeof(u);
		u.mm_cookie = g_bp_mm_cookie;
		u.index = idx;
		u.hit_count = rec->hit_count;
		u.pc = rec->pc;
		u.lr = rec->lr;
		u.sp = rec->sp;
		u.orig_x0 = rec->orig_x0;
		u.syscallno = rec->syscallno;
		u.pstate = rec->pstate;
		__builtin_memcpy(&u.x0, &rec->x0, sizeof(u.x0) * 30);
		u.fpsr = rec->fpsr;
		u.fpcr = rec->fpcr;
#define BP_COPY_Q(n) q = rec->q##n; u.q##n##_lo = (u64)q; u.q##n##_hi = (u64)(q >> 64)
		BP_COPY_Q(0); BP_COPY_Q(1); BP_COPY_Q(2); BP_COPY_Q(3);
		BP_COPY_Q(4); BP_COPY_Q(5); BP_COPY_Q(6); BP_COPY_Q(7);
		BP_COPY_Q(8); BP_COPY_Q(9); BP_COPY_Q(10); BP_COPY_Q(11);
		BP_COPY_Q(12); BP_COPY_Q(13); BP_COPY_Q(14); BP_COPY_Q(15);
		BP_COPY_Q(16); BP_COPY_Q(17); BP_COPY_Q(18); BP_COPY_Q(19);
		BP_COPY_Q(20); BP_COPY_Q(21); BP_COPY_Q(22); BP_COPY_Q(23);
		BP_COPY_Q(24); BP_COPY_Q(25); BP_COPY_Q(26); BP_COPY_Q(27);
		BP_COPY_Q(28); BP_COPY_Q(29); BP_COPY_Q(30); BP_COPY_Q(31);
#undef BP_COPY_Q
		if (copy_to_user(arg, &u, sizeof(u)))
			ret = -EFAULT;
	}
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_ioctl_step_arm(struct bp_step_arm_v1 __user *arg)
{
	struct bp_step_arm_v1 u;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	ret = bp_check_arm_header(&u.sess, sizeof(u));
	if (ret)
		return ret;
	mutex_lock(&g_bp_mutex);
	ret = bp_begin_session(&u.sess);
	if (!ret) {
		ret = bp_stepbp_arm_locked(&u);
		if (ret)
			bp_release_session_locked();
	}
	if (!ret) {
		u.sess.mm_cookie = g_bp_mm_cookie;
		if (copy_to_user(arg, &u, sizeof(u)))
			ret = -EFAULT;
	}
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_ioctl_step_status(struct bp_step_status_v1 __user *arg)
{
	struct bp_step_status_v1 u;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	if (u.abi_major != BP_ABI_MAJOR || u.struct_size != sizeof(u))
		return -EINVAL;
	mutex_lock(&g_bp_mutex);
	ret = bp_check_session_meta(u.tgid, u.task_start_ticks, u.mm_cookie);
	if (!ret) {
		if (!g_bp_stepbp_on)
			ret = -ENOENT;
	}
	if (!ret) {
		u.state = BP_STATE_ARMED;
		u.mm_cookie = g_bp_mm_cookie;
		u.pc = g_bp_cfg_stepbp.points[0].hit_addr;
		u.hit_count = g_bp_cfg_stepbp.points[0].records[0].hit_count;
		u.generation = g_stepbp_generation;
		if (copy_to_user(arg, &u, sizeof(u)))
			ret = -EFAULT;
	}
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_ioctl_step_stop(struct bp_step_stop_v1 __user *arg)
{
	struct bp_step_stop_v1 u;
	int ret;

	if (copy_from_user(&u, arg, sizeof(u)))
		return -EFAULT;
	if (u.abi_major != BP_ABI_MAJOR || u.struct_size != sizeof(u))
		return -EINVAL;
	mutex_lock(&g_bp_mutex);
	ret = bp_check_session_meta(0, 0, u.mm_cookie);
	if (!ret)
		ret = bp_stepbp_stop_locked();
	mutex_unlock(&g_bp_mutex);
	return ret;
}

static long bp_unlocked_ioctl(struct file *filp, unsigned int cmd,
			      unsigned long arg)
{
	void __user *uarg = (void __user *)arg;

	switch (cmd) {
	case BP_IOC_INFO:
		return bp_ioctl_info(uarg);
	case BP_IOC_PTE_ARM:
		return bp_ioctl_pte_arm(uarg);
	case BP_IOC_PTE_STATUS:
		return bp_ioctl_pte_status(uarg);
	case BP_IOC_PTE_STOP:
		return bp_ioctl_pte_stop(uarg);
	case BP_IOC_HWBP_ARM:
		return bp_ioctl_hwbp_arm(uarg);
	case BP_IOC_HWBP_STATUS:
		return bp_ioctl_hwbp_status(uarg);
	case BP_IOC_HWBP_STOP:
		return bp_ioctl_hwbp_stop(uarg);
	case BP_IOC_HWBP_RECORD:
		return bp_ioctl_hwbp_record(uarg);
	case BP_IOC_STEP_ARM:
		return bp_ioctl_step_arm(uarg);
	case BP_IOC_STEP_STATUS:
		return bp_ioctl_step_status(uarg);
	case BP_IOC_STEP_STOP:
		return bp_ioctl_step_stop(uarg);
	default:
		return -ENOTTY;
	}
}

/* ---------------- lifecycle ---------------- */

/* 5.10 adapter: el0t_64_sync_handler has no stable kallsyms entry there;
 * the synchronous IABT path lands in do_mem_abort(addr, esr, regs) instead.
 * That path enters with PAN set (unlike the 5.15+ el0t_64_sync_handler
 * entry), so reading the user pt_regs requires an IRQ-masked PAN window. */
static int bp_ptebp_mem_abort_adapter(struct pt_regs *hook_regs)
{
	unsigned long flags;
	int ret;

	if (!hook_regs)
		return 0;
	local_irq_save(flags);
	asm volatile(".inst 0xd500409f" ::: "memory"); /* msr pan, #0 */
	ret = ptebp_handle_exec_fault(hook_regs);
	asm volatile(".inst 0xd500419f" ::: "memory"); /* msr pan, #1 */
	local_irq_restore(flags);
	return ret;
}

/* resolve the runtime symbols the hook layer needs (no CFI bypassing) */
static int bp_resolve_symbols(void)
{
	unsigned long sync_h;
	unsigned long sync15 = generic_kallsyms_lookup_name("el0t_64_sync_handler");

	fn_aarch64_insn_patch_text =
		(void *)generic_kallsyms_lookup_name("aarch64_insn_patch_text");
	if (!fn_aarch64_insn_patch_text)
		return -ENOENT;

	/* EL0 sync-vector handler name differs across kernels: 5.15+ uses
	 * el0t_64_sync_handler(regs); 5.10 hooks el0_ia, the C IABT handler
	 * that receives the user pt_regs in x0 (kernel_entry already saved it) */
	sync_h = sync15;
	if (!sync_h)
		sync_h = generic_kallsyms_lookup_name("el0_ia");
	if (!sync_h)
		return -ENOENT;
	g_ptebp_fault_hooks[0].target_addr = sync_h;
	g_ptebp_fault_hooks[0].target_sym = NULL;
	if (!sync15) {
		g_ptebp_fault_hooks[0].work_fn = bp_ptebp_mem_abort_adapter;
		g_ptebp_mem_abort_style = true;
	}
	return 0;
}

/* mmu-notifier: mapping invalidation on the target mm fails the PTE monitor
 * closed (page restored, monitoring dropped) - no stale UXN traps.
 * 5.15+ only: 5.10 concurrent re-arm races the notifier lifecycle; its
 * hit-time PTE match fail-closed remains the 5.10 fallback. */
#ifdef BP_MMU_NOTIFIER
static int bp_mmu_invalidate_start(struct mmu_notifier *mn,
				   const struct mmu_notifier_range *range)
{
	struct mm_struct *mm = range->mm;

	(void)mn;
	if (!g_bp_active || g_bp_mm != mm || !g_bp_ptebp_on)
		return 0;
	ptebp_drop_all_monitors(false);
	bp_dbg_pte_restore();
	g_bp_ptebp_on = false;
	if (atomic64_read(&g_dbg_hooks) > 0)
		atomic64_dec(&g_dbg_hooks);
	return 0;
}

static void bp_mmu_release(struct mmu_notifier *mn, struct mm_struct *mm)
{
	(void)mn;
	if (g_bp_mn.mm == mm)
		g_bp_mn_registered = false;
}

static const struct mmu_notifier_ops bp_mmu_ops = {
	.invalidate_range_start = bp_mmu_invalidate_start,
	.release = bp_mmu_release,
};
#endif

static void bp_cleanup_work_fn(struct work_struct *work)
{
	(void)work;
	mutex_lock(&g_bp_mutex);
	if (g_bp_active)
		bp_stop_all_locked();
	mutex_unlock(&g_bp_mutex);
}

static int bp_kprobe_do_exit(struct kprobe *p, struct pt_regs *regs)
{
	(void)p;
	(void)regs;
	/* only the thread-group leader exit invalidates the session;
	 * plain thread exits must not tear down the monitor */
	if (READ_ONCE(g_bp_active) && current->tgid == READ_ONCE(g_bp_tgid) &&
	    current->pid == current->tgid)
		schedule_work(&g_bp_cleanup_work);
	return 0;
}

static int bp_kprobe_do_execveat_common(struct kprobe *p, struct pt_regs *regs)
{
	(void)p;
	(void)regs;
	if (READ_ONCE(g_bp_active) && current->tgid == READ_ONCE(g_bp_tgid) &&
	    current->pid == current->tgid)
		schedule_work(&g_bp_cleanup_work);
	return 0;
}

static int bp_register_kprobes(void)
{
	int ret;

	g_kp_exit.symbol_name = "do_exit";
	g_kp_exit.pre_handler = bp_kprobe_do_exit;
	ret = register_kprobe(&g_kp_exit);
	if (ret)
		return ret;
	g_kp_exit_reg = true;

	g_kp_exec.symbol_name = "do_execveat_common";
	g_kp_exec.pre_handler = bp_kprobe_do_execveat_common;
	ret = register_kprobe(&g_kp_exec);
	if (ret) {
		/* exec detection is best-effort; exit detection is required */
		pr_warn("bp: do_execveat_common kprobe failed (%d), exec auto-clean disabled\n",
			ret);
		return 0;
	}
	g_kp_exec_reg = true;
	return 0;
}

static void bp_unregister_kprobes(void)
{
	if (g_kp_exit_reg) {
		unregister_kprobe(&g_kp_exit);
		g_kp_exit_reg = false;
	}
	if (g_kp_exec_reg) {
		unregister_kprobe(&g_kp_exec);
		g_kp_exec_reg = false;
	}
}

static int bp_open(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	return 0;
}

static int bp_release(struct inode *inode, struct file *filp)
{
	(void)inode;
	(void)filp;
	/* sessions are device-global; release keeps them alive so other
	 * controllers can status/stop them. Target exit, exec, module
	 * unload and re-arm clean up the session. */
	return 0;
}

static const struct file_operations bp_fops = {
	.owner = THIS_MODULE,
	.open = bp_open,
	.release = bp_release,
	.unlocked_ioctl = bp_unlocked_ioctl,
#ifdef CONFIG_COMPAT
	.compat_ioctl = bp_unlocked_ioctl,
#endif
};

static struct miscdevice bp_miscdev = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = "bp",
	.fops = &bp_fops,
	.mode = 0600,
};

static int __init bp_init(void)
{
	int ret;

	INIT_WORK(&g_bp_cleanup_work, bp_cleanup_work_fn);

	ret = bp_resolve_symbols();
	if (ret) {
		pr_err("bp: symbol resolution failed (%d)\n", ret);
		return ret;
	}

	ret = bp_register_kprobes();
	if (ret) {
		pr_err("bp: kprobe registration failed (%d)\n", ret);
		return ret;
	}

	ret = misc_register(&bp_miscdev);
	if (ret) {
		bp_unregister_kprobes();
		pr_err("bp: misc_register failed (%d)\n", ret);
		return ret;
	}

	pr_info("bp: %s loaded, abi %u.%u\n", BP_DRIVER_VERSION,
		BP_ABI_MAJOR, BP_ABI_MINOR);
	return 0;
}

static void __exit bp_exit(void)
{
	bp_unregister_kprobes();
	flush_work(&g_bp_cleanup_work);

	mutex_lock(&g_bp_mutex);
	if (g_bp_active)
		bp_stop_all_locked();
	bp_drain_inflight();
	mutex_unlock(&g_bp_mutex);

	misc_deregister(&bp_miscdev);
	pr_info("bp: unloaded\n");
}

module_init(bp_init);
module_exit(bp_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("ARM64 breakpoint driver (HWBP/PTEBP/STEPBP)");
MODULE_VERSION(BP_DRIVER_VERSION);
