#ifndef BP_UAPI_H
#define BP_UAPI_H
/*
 * bp_uapi.h - userspace ABI for the bp ARM64 breakpoint driver.
 * Style base: upstream examples lsdriver_uapi.h (INFO-first, mm_cookie
 * session token, arm/status/stop semantics), extended to the three
 * breakpoint families: HWBP / PTEBP / STEPBP.
 *
 * Session rules:
 *  - Single active configuration (one tgid at a time per open device).
 *  - BP_IOC_*_ARM returns a kernel-assigned mm_cookie; STATUS/STOP and
 *    RECORD must carry it back. A cookie from a previous session is
 *    rejected (-EPERM).
 *  - expected_mm_cookie must be 0 when arming a new session.
 *  - tgid is validated together with task_start_ticks (PID reuse guard).
 *  - No function pointers cross the ABI boundary; register write-back is
 *    driven by the mask[] field only.
 */
#include <linux/ioctl.h>
#include <linux/types.h>

#define BP_ABI_MAJOR 1U
#define BP_ABI_MINOR 0U
#define BP_IOC_MAGIC 0x42U

/* Feature bits reported by BP_IOC_INFO */
#define BP_FEATURE_HWBP       (1ULL << 0)
#define BP_FEATURE_PTEBP      (1ULL << 1)
#define BP_FEATURE_STEPBP     (1ULL << 2)
#define BP_FEATURE_REG_WRITE  (1ULL << 3)

/* ARM flags */
#define BP_ARM_F_PERSISTENT (1U << 0)

/* PTE / monitor states */
enum bp_state_v1 {
    BP_STATE_IDLE = 0,
    BP_STATE_ARMED = 1,
    BP_STATE_STOPPING = 2,
    BP_STATE_ERROR = 3,
};

/* Register operation modes for mask[] (2 bits per register index) */
#define BP_OP_NONE_V1  0x0
#define BP_OP_READ_V1  0x1
#define BP_OP_WRITE_V1 0x2
#define BP_MASK_SIZE   18

#define BP_MAX_POINTS  16
#define BP_MAX_RECORDS 0x10

struct bp_info_v1 {
    __u32 abi_major;
    __u32 abi_minor;
    __u32 struct_size;
    __u32 reserved0;
    __u64 feature_flags;
    __u64 num_brps;
    __u64 num_wrps;
    /* debug counters: must be zero after every clean stop/unload */
    __u64 debug_active_sessions;
    __u64 debug_inflight;
    __u64 debug_pte_restored_pages;
    __u64 debug_ref_count;
    __u64 debug_hooks_active;
};

/* Embedded session header of every ARM request. */
struct bp_session_v1 {
    __u32 abi_major;
    __u32 struct_size;      /* size of the enclosing arm struct */
    __s32 tgid;
    __u32 flags;
    __u64 task_start_ticks; /* /proc/<pid>/stat field 22 */
    __u64 expected_mm_cookie;
    __u64 mm_cookie;        /* kernel-assigned, filled on ARM */
};

/* ---- PTEBP (PTE UXN execute breakpoint) ---- */

struct bp_pte_arm_v1 {
    struct bp_session_v1 sess;
    __u64 pc;
    __u32 s0_bits;          /* value written into S0 low 32 bits on hit */
    __u32 reserved0;
};

struct bp_pte_status_v1 {
    __u32 abi_major;
    __u32 struct_size;
    __u32 state;
    __s32 tgid;
    __u64 task_start_ticks;
    __u64 mm_cookie;
    __u64 pc;
    __u64 hit_count;
};

struct bp_pte_stop_v1 {
    __u32 abi_major;
    __u32 struct_size;
    __u64 mm_cookie;
};

/* ---- HWBP (hardware breakpoint / watchpoint) ---- */

/* breakpoint type (matches kernel enum bp_type values) */
#define BP_BT_EMPTY 0
#define BP_BT_R     1
#define BP_BT_W     2
#define BP_BT_RW    3
#define BP_BT_X     4

/* thread scope */
#define BP_BS_MAIN_THREAD   0
#define BP_BS_OTHER_THREADS 1
#define BP_BS_ALL_THREADS   2

struct bp_hwbp_arm_v1 {
    struct bp_session_v1 sess;
    __u64 hit_addr;
    __u32 bt;               /* BP_BT_* */
    __u32 bl;               /* 1..8 */
    __u32 bs;               /* BP_BS_* */
    __u32 reserved0;
    __u8 mask[BP_MASK_SIZE]; /* 2-bit ops per bp_reg index; write-back allowed */
    __u8 reserved1[14];
    __u64 seed_x0;          /* initial record value written back into x0 when
                               mask[IDX_X0] == BP_OP_WRITE_V1 */
    __u64 hit_count;
    __u32 record_count;
    __u32 reserved2;
};

struct bp_hwbp_status_v1 {
    __u32 abi_major;
    __u32 struct_size;
    __u32 state;
    __s32 tgid;
    __u64 task_start_ticks;
    __u64 mm_cookie;
    __u64 hit_count;
    __u32 record_count;
    __u32 reserved0;
};

struct bp_hwbp_stop_v1 {
    __u32 abi_major;
    __u32 struct_size;
    __u64 mm_cookie;
};

struct bp_record_req_v1 {
    __u32 abi_major;
    __u32 struct_size;
    __u64 mm_cookie;
    __u32 index;            /* record slot 0..BP_MAX_RECORDS-1 */
    __u32 reserved0;
};

struct bp_record_v1 {
    __u32 abi_major;
    __u32 struct_size;
    __u64 mm_cookie;
    __u32 index;            /* echoed request slot */
    __u32 reserved0;
    __u64 hit_count;
    __u64 pc;
    __u64 lr;
    __u64 sp;
    __u64 orig_x0;
    __s32 syscallno;
    __u32 pad0;
    __u64 pstate;
    __u64 x0, x1, x2, x3, x4, x5, x6, x7, x8, x9;
    __u64 x10, x11, x12, x13, x14, x15, x16, x17, x18, x19;
    __u64 x20, x21, x22, x23, x24, x25, x26, x27, x28, x29;
    __u32 fpsr;
    __u32 fpcr;
    __u64 q0_lo, q0_hi, q1_lo, q1_hi, q2_lo, q2_hi, q3_lo, q3_hi;
    __u64 q4_lo, q4_hi, q5_lo, q5_hi, q6_lo, q6_hi, q7_lo, q7_hi;
    __u64 q8_lo, q8_hi, q9_lo, q9_hi, q10_lo, q10_hi, q11_lo, q11_hi;
    __u64 q12_lo, q12_hi, q13_lo, q13_hi, q14_lo, q14_hi, q15_lo, q15_hi;
    __u64 q16_lo, q16_hi, q17_lo, q17_hi, q18_lo, q18_hi, q19_lo, q19_hi;
    __u64 q20_lo, q20_hi, q21_lo, q21_hi, q22_lo, q22_hi, q23_lo, q23_hi;
    __u64 q24_lo, q24_hi, q25_lo, q25_hi, q26_lo, q26_hi, q27_lo, q27_hi;
    __u64 q28_lo, q28_hi, q29_lo, q29_hi, q30_lo, q30_hi, q31_lo, q31_hi;
};

/* ---- STEPBP (single-step PC breakpoint) ---- */

struct bp_step_arm_v1 {
    struct bp_session_v1 sess;
    __u64 pc;
    __u32 bt;               /* BP_BT_X only */
    __u32 reserved0;
};

struct bp_step_status_v1 {
    __u32 abi_major;
    __u32 struct_size;
    __u32 state;
    __s32 tgid;
    __u64 task_start_ticks;
    __u64 mm_cookie;
    __u64 pc;
    __u64 hit_count;
    __u64 generation;
};

struct bp_step_stop_v1 {
    __u32 abi_major;
    __u32 struct_size;
    __u64 mm_cookie;
};

#define BP_IOC_INFO        _IOR(BP_IOC_MAGIC, 0x00, struct bp_info_v1)
#define BP_IOC_PTE_ARM     _IOWR(BP_IOC_MAGIC, 0x01, struct bp_pte_arm_v1)
#define BP_IOC_PTE_STATUS  _IOR(BP_IOC_MAGIC, 0x02, struct bp_pte_status_v1)
#define BP_IOC_PTE_STOP    _IOW(BP_IOC_MAGIC, 0x03, struct bp_pte_stop_v1)
#define BP_IOC_HWBP_ARM    _IOWR(BP_IOC_MAGIC, 0x04, struct bp_hwbp_arm_v1)
#define BP_IOC_HWBP_STATUS _IOR(BP_IOC_MAGIC, 0x05, struct bp_hwbp_status_v1)
#define BP_IOC_HWBP_STOP   _IOW(BP_IOC_MAGIC, 0x06, struct bp_hwbp_stop_v1)
#define BP_IOC_HWBP_RECORD _IOR(BP_IOC_MAGIC, 0x07, struct bp_record_v1)
#define BP_IOC_STEP_ARM    _IOWR(BP_IOC_MAGIC, 0x08, struct bp_step_arm_v1)
#define BP_IOC_STEP_STATUS _IOR(BP_IOC_MAGIC, 0x09, struct bp_step_status_v1)
#define BP_IOC_STEP_STOP   _IOW(BP_IOC_MAGIC, 0x0a, struct bp_step_stop_v1)

#endif /* BP_UAPI_H */
