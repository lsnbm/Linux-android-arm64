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

## QEMU first

From this directory:

```bash
make qemu VERSION=6.1-Android14 KERNELS_ROOT=/root
```

The command boots the 6.1 kernel Image with an initramfs, loads the test
module, runs the runner, unloads the module, and requires:

```text
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
