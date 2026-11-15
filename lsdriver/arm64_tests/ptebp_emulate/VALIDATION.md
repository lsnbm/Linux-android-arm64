# PTEBP load/store emulator validation

This test uses the same `ptebp_emulate_test_module.ko`, AArch64 runner, and
`instruction.txt` in QEMU and on a rooted Android device.

The kernel test module includes the production PTEBP implementation through
`break_point.h`. Its ioctl oracle creates two shadow pages, publishes them in
the same slot lookup used by the emulator, initializes `pt_regs` and
`struct fp_regs`, then calls `ptebp_emulate_load_store()` directly.

The runner maps fixed code/data addresses, single-steps the same raw instruction
on ARM64 with `ptrace`, and compares the oracle with the CPU result for GPRs,
SP, PC, PSTATE, Q0-Q31, FPCR, FPSR, and 4096 bytes of memory.

## Process memory file reads

Before the instruction comparisons, the runner installs three real PTEBP
breakpoints on two adjacent executable pages. It reads `/proc/self/mem` using
both `openat -> lseek -> read` and `fopen -> fseek -> fread`, comparing the
returned bytes with a snapshot taken before installation.

Each path checks every byte of both managed pages individually, all lengths
from zero through `2 * PAGE_SIZE + 1` starting one byte before the first managed
page, and a four-page read spanning ordinary and managed pages. The stdio path
uses buffering and also checks multi-byte element counts. Short reads, invalid
addresses, file positions, buffer boundaries, guard PTEs, UDF markers, actual
breakpoint hits, and two install/stop cycles are checked as well.

To run only this test, use a complete kernel build containing both `Image` and
`Module.symvers`, and pass the same directory to the module build and QEMU:

```bash
FILE_READ_ONLY=1 QEMU_LOG=file-read-qemu.log \
	make qemu VERSION=6.1-Android14 KERNELS_ROOT=/root KERNEL_BUILD=/path/to/kernel_aarch64
```

The runner can also be invoked directly after loading the test module:

```bash
./ptebp_emulate_test_runner --file-read-only /proc/ptebp_emulate_test
```

On 2026-11-15, Linux 6.1 QEMU with 4 KiB pages passed 32,774 byte comparisons,
both API paths, two breakpoint hits, short reads, error handling, and restart
checks. See `file-read-qemu-20261115.log`. This file-read test has not been run
on an Android device. It covers process memory files, not arbitrary on-disk
copies of mapped executables or stdio data cached before monitoring starts.

## QEMU first

From this directory:

```bash
make qemu VERSION=6.1-Android14 KERNELS_ROOT=/root
```

The command boots the 6.1 kernel Image with an initramfs, loads the test
module, runs the runner, unloads the module, and requires:

```text
ptebp file read test passed checks=...
ptebp emulator test passed cases=...
case_counts=compared=... skipped=...
PTEBP_TEST_QEMU_PASS
```

## Android device

Run this only after QEMU passes:

```powershell
& .\lsdriver\arm64_tests\run-ptebp-emulate-test-device.ps1 -Serial 2912b4a6
```

The device script verifies transfer hashes, root access, all result rows,
module/proc cleanup, and unchanged SELinux mode.

The current corpus has 141 load instructions covering literal, unsigned,
unscaled, pre-index, post-index, register-offset, pair GPR, scalar SIMD,
128-bit SIMD, signed loads, and address writeback.
