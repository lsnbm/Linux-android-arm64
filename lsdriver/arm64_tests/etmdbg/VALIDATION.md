# Direct ETM/TRBE validation

This test uses the same `arm64_etmdbg_test_module.ko` on QEMU and an ADB
device. Module initialization invokes the same capture callback on each online
CPU until one can configure ETM and TRBE through system registers, executes a
fixed kernel instruction body, drains and stops the trace, and exposes the
result through `/dev/arm64_etmdbg_test`.

Build the Android 6.1 module in WSL:

```bash
make -C lsdriver/arm64_tests/etmdbg module
```

The QEMU harness reuses the existing ARM64 initramfs launcher. QEMU machines
without virtual ETM/TRBE are expected to print `ETMDBG_TEST_QEMU_UNSUPPORTED_OK`
after the module rejects the unsupported capability without a kernel fault.

On a rooted AArch64 device with matching kernel headers and ETM/TRBE hardware:

```powershell
& .\lsdriver\arm64_tests\run-arm64-etmdbg-test-device.ps1 -Serial <serial>
```

The ADB harness requires a non-empty trace payload and rejects `TRBSR.ABORT`.
The target must expose a programmable TRBE (`TRBIDR.PROG=1`); when firmware or
EL2 owns the TRBE (`TRBIDR.PROG=0`), the module reports `EOPNOTSUPP` without
writing the trace-buffer registers.

## Userspace cs_etm perf probe

The directory also contains a plain userspace probe for the kernel's existing
`cs_etm` perf PMU. It does not load the custom module or access ETM system
registers directly:

```bash
make -C lsdriver/arm64_tests/etmdbg user
```

The PMU type can normally be read from sysfs. If SELinux blocks that read on a
root shell, pass the type explicitly after checking it from an allowed shell:

```powershell
adb -s <serial> push lsdriver\arm64_tests\etmdbg\perf_etm_user_test /data/local/tmp/
adb -s <serial> shell su -c `
	"chmod 0755 /data/local/tmp/perf_etm_user_test; /data/local/tmp/perf_etm_user_test 10"
```

The probe opens one `cs_etm` event per online CPU, maps a perf AUX buffer,
executes a userspace workload, and requires non-zero AUX bytes. On the tested
device, PMU type `10` was registered but every `perf_event_open` returned
`EACCES`; `simpleperf -e cs-etm` reported `etm devices are not available`.
The `cs_etm` sysfs directory had no `cpuN` source links. Therefore this result
does not demonstrate trace production: the kernel PMU registration exists, but
the CPU ETM/ETE sources are not currently connected to the perf CoreSight path.

## Current direction

The ordinary userspace `perf cs_etm` path is currently unavailable. This means
the PMU is registered, but does not prove that the CPU ETM/ETE source is
usable. Continue from this test directory with these priorities:

1. Check why the kernel CoreSight driver did not register the CPU ETM/ETE
	source links under `cs_etm`.
2. Implement the independent MMIO path `ETM/ETE -> Funnel -> TMC-ETF/ETR/ETB`
	for the final no-external-driver implementation.
3. Use `ETM/ETE -> TRBE` only after ownership is available; `TRBIDR.PROG=0`
	currently indicates that firmware or EL2 owns TRBE, so the module must not
	write its trace-buffer registers.

QEMU and ADB continue to use the same test code. QEMU currently has no virtual
ETM/TRBE model and only validates the safe unsupported-capability path.