# PTEBP load/store emulator validation

This test uses the same `ptebp_emulate_test_module.ko`, AArch64 runner, and
`instruction.txt` in QEMU and on a rooted Android device.

The runner maps fixed code/data addresses, single-steps the same raw instruction
on ARM64 with `ptrace`, and compares the module result with the CPU result for GPRs,
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
addresses, file positions, buffer boundaries, actual breakpoint hits, and two
install/stop cycles are checked as well. After installing
the monitors, the runner switches the managed pages to RWX and compares the
returned buffers with volatile byte reads from the original virtual addresses,
then restores RX before stopping the monitors.

The runner also checks `process_vm_readv` across managed page boundaries with
multiple local/remote iovecs, a zero-length iovec, partial destination/source
faults, and a `PROT_NONE` source. A third install cycle copies into a guarded
destination and checks that the operation completes and monitoring can be
stopped successfully.

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

On 2026-11-15, the runner passed all 32,774 comparisons on RMX3888
(Linux 6.1.25, 4 KiB pages) and Linux 6.1 QEMU, including RWX
changes, direct-pointer comparisons, two breakpoint hits, short reads, error
handling, restart, and cleanup.

## Original program inline-hook checks

On the same RMX3888, the unmodified external `hwbreak_test` was run with
`--inline-hook-only`. The test module installed the production PTE breakpoint
on the printed `timing_probe_1` address while the program waited for input, then
allowed its existing 34 block/byte read comparisons to proceed.

| Stage | Failed checks | Program exit |
| --- | --- | --- |
| No monitor | 0 / 34 | 0 |
| Before these fixes | 12 / 34 | 2 |
| Final implementation | 0 / 34 | 0 |

The final run passed executable-file `pread`, new `mmap`, `openat/read`, and
`fopen/fread`, libc and direct-SVC `process_vm_readv`, and `/proc/.../mem`
readers. The detector source and comparisons were unchanged.

Final run: PID 6940, `timing_probe_1=0x59d86d6000`, `active=1` before and after,
exit 0. `target_hits=0` is expected because this mode only reads the probe; it
does not execute it. This result does not validate the program's timing/perf
tests or sustained execution monitoring.

SHA256 values verified against the transferred device files:

```text
hwbreak_test: aa05dabeeb3b00ac5860c2791e36e13b4cf9cc4ddd6717862b6f2741f32c4dab
ptebp_emulate_test_module.ko: cfe58fab77bb9edf7b87b235db5675614551b6e468c59b82d670a37af9ded8f9
```

The final code also passed the 32,774 mem comparisons, two execution hits,
and the process_vm boundary cases on both device and QEMU. Modules and temporary device files
were removed; SELinux mode was unchanged. The Linux 6.1 primary module built
successfully, but the phone's primary driver was not replaced. Other kernel
versions were checked for the new API signatures, not runtime-tested.

Reproduce from the repository root after building the original target binary:

```powershell
wsl.exe --exec make -C /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/arm64_tests/ptebp_emulate module KCFLAGS=-w
& .\lsdriver\arm64_tests\run-ptebp-target-test-device.ps1 `
	-Serial 2912b4a6 -TargetBinary 'C:\path\to\hwbreak_test'
```

Add `-Baseline` for the no-monitor run. The script records the program's exit
code even when checks fail, so inspect both the program and summary logs.
The test does not cover previously cached stdio buffers or all possible file
mapping types and page sizes.

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
