# bp — ARM64 Breakpoint Driver ABI

`bp` is an ARM64 kernel driver (module `bp`, device `/dev/bp`, mode 0600,
misc minor) providing three breakpoint families through a stable ioctl ABI:

- **PTEBP** — PTE UXN execute breakpoints with in-kernel instruction
  emulation and S0 (FP/SIMD low-32) write-back on every precise hit.
- **HWBP** — hardware breakpoints/watchpoints (X/R/W/RW, 1–8 bytes) with
  per-register 2-bit masks (NONE/READ/WRITE) and hit records.
- **STEPBP** — single-step PC breakpoints with generation tracking.

ABI version: `BP_ABI_MAJOR 1`, `BP_ABI_MINOR 0`. Ioctl magic `0x42`.
Authoritative header: `include/bp_uapi.h` (kernel mirror: `kernel/abi/bp_uapi.h`,
byte-identical).

## Session model

- One active configuration per module instance (one tgid).
- `BP_IOC_*_ARM` returns a kernel-assigned `mm_cookie`; STATUS/STOP/RECORD
  must carry it back (`-EPERM` otherwise).
- Task identity is `tgid` + `task_start_ticks` (`/proc/<pid>/stat` field 22,
  USER_HZ) — PID reuse is rejected with `-ESRCH`.
- Re-arming the same tgid replaces the session; a different tgid while a
  session is active returns `-EBUSY`.
- Target exit (thread-group leader) and exec auto-clean the session and
  restore all PTE permissions. Module unload drains all monitors first.
- No function pointers cross the ABI; register write-back is driven by
  `mask[]` only.

## Ioctls

| ioctl | direction | struct |
|---|---|---|
| `BP_IOC_INFO` | R | `bp_info_v1` (features, BRP/WRP counts, debug counters) |
| `BP_IOC_PTE_ARM` | WR | `bp_pte_arm_v1` (pc, s0_bits, persistent flag) |
| `BP_IOC_PTE_STATUS` | R | `bp_pte_status_v1` (state, hit_count) |
| `BP_IOC_PTE_STOP` | W | `bp_pte_stop_v1` |
| `BP_IOC_HWBP_ARM` | WR | `bp_hwbp_arm_v1` (bt/bl/bs, mask[18], seed_x0) |
| `BP_IOC_HWBP_STATUS` | R | `bp_hwbp_status_v1` (hit_count, record_count) |
| `BP_IOC_HWBP_STOP` | W | `bp_hwbp_stop_v1` |
| `BP_IOC_HWBP_RECORD` | R | `bp_record_v1` (per-PC hit record, full regs + Q0–Q31) |
| `BP_IOC_STEP_ARM` | WR | `bp_step_arm_v1` (pc) |
| `BP_IOC_STEP_STATUS` | R | `bp_step_status_v1` (hit_count, generation) |
| `BP_IOC_STEP_STOP` | W | `bp_step_stop_v1` |

## PTEBP semantics

- Arming marks the target page `PTE_UXN`; an EL0 instruction-abort fault is
  intercepted and the instruction stream is batch-emulated in-kernel (up to
  4096 insns per fault) while calling the hit callback on the configured PC.
  The exception entry is hooked per kernel series: 5.15+ `el0t_64_sync_handler`,
  5.10 `el0_ia` (a PAN-window adapter is used on 5.10).
- On every precise hit, the low 32 bits of S0 (via Q0) are replaced with
  `s0_bits`; the high 96 bits of Q0 and all other registers are preserved.
- Emulation failure or batch overrun restores the page permissions
  (fail-closed) so the target can never spin on the UXN page.
- STOP restores the original PTE and flushes TLBs. Mapping invalidation on
  the target mm (mmu-notifier, 5.15+/6.2+) also fails the monitor closed.

## HWBP semantics

- `bt`: X (execute, length fixed 4), R/W/RW watchpoints (1–8 bytes, exact
  address match, ESR WNR direction check).
- `mask[]`: 2 bits per register index (PC..X29, FPSR/FPCR, Q0–Q31);
  `BP_OP_READ` snapshots the register into the record, `BP_OP_WRITE` writes
  the record value back (e.g. `seed_x0` for X0).
- Hit records are keyed by PC (up to 16), each with hit_count and a full
  register snapshot.

## STEPBP semantics

- Hardware single-step (MDSCR SS + TIF_SINGLESTEP) driven per task with
  generation counters; hits increment the per-PC record and generation.

## Known limitations

- QEMU virt: STEP single-step hit delivery may surface a user-visible
  SIGTRAP (ptrace pseudo-step semantics); the kernel-side counters are
  authoritative and the hit path is intended for real-hardware validation.
- Mapping invalidation: an mmu-notifier (`invalidate_range_start`) fails the
  PTE monitor closed on 5.15+ / 6.2+ targets. 5.10 and 6.1 keep the
  hit-time PTE-match fail-closed fallback because concurrent re-arm races
  the notifier lifecycle on those series.
- Real device 5.15.180 (PLZ110, serial d312a252): the first validation
  round passed load/INFO/err/rmmod; a PTE-hit stall observed on that build
  is under investigation (later on-device re-tests were blocked by the
  device-side module-load policy).
