#!/usr/bin/env bash

set -euo pipefail

BUILD_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
KERNELS_ROOT="${KERNELS_ROOT:-/root}"
VERSION="${VERSION:-6.1-Android14}"
KERNEL_DIR="${KERNELS_ROOT}/${VERSION}"
KERNEL_BUILD="${KERNEL_BUILD:-}"
if [[ -z "$KERNEL_BUILD" ]]; then
    KERNEL_BUILD="$(readlink -f "${KERNEL_DIR}/bazel-bin/common/kernel_aarch64")"
fi
KERNEL_IMAGE="${KERNEL_IMAGE:-${KERNEL_BUILD}/Image}"
CLANG_DIR="${CLANG_DIR:-${KERNEL_DIR}/prebuilts/clang/host/linux-x86/clang-r487747c}"
QEMU="${QEMU:-qemu-system-aarch64}"
WORK_DIR="${WORK_DIR:-/tmp/lsdriver-ptebp-emulate-qemu}"
ROOTFS_DIR="${WORK_DIR}/rootfs"
INITRAMFS="${WORK_DIR}/initramfs.cpio"
INIT="${WORK_DIR}/init"
MODULE="${BUILD_ROOT}/ptebp_emulate_test_module.ko"
RUNNER="${BUILD_ROOT}/ptebp_emulate_test_runner"
INSTRUCTIONS="${BUILD_ROOT}/instruction.txt"
LOG="${BUILD_ROOT}/qemu.log"

if [[ ! -f "$KERNEL_IMAGE" ]]; then
    KERNEL_IMAGE="$(find "${KERNEL_DIR}/out/bazel/output_user_root" -type f -path '*/bazel-out/*/bin/common/kernel_aarch64/Image' -print -quit 2>/dev/null || true)"
fi

for path in "$MODULE" "$RUNNER" "$INSTRUCTIONS" "$KERNEL_IMAGE" "$CLANG_DIR/bin/clang"; do
    test -e "$path" || { echo "missing: $path" >&2; exit 1; }
done
command -v "$QEMU" >/dev/null || { echo "missing QEMU: $QEMU" >&2; exit 1; }

rm -rf "$WORK_DIR"
mkdir -p "$ROOTFS_DIR/dev" "$ROOTFS_DIR/proc"
"${CLANG_DIR}/bin/clang" --target=aarch64-linux-gnu -c -nostdlib \
    -o "$WORK_DIR/qemu_init.o" "$BUILD_ROOT/qemu_init.S"
"${CLANG_DIR}/bin/ld.lld" -m aarch64elf -e _start \
    -o "$INIT" "$WORK_DIR/qemu_init.o"
chmod 0755 "$INIT"
cp "$INIT" "$ROOTFS_DIR/init"
cp "$MODULE" "$ROOTFS_DIR/ptebp_emulate_test_module.ko"
cp "$RUNNER" "$ROOTFS_DIR/ptebp_emulate_test_runner"
cp "$INSTRUCTIONS" "$ROOTFS_DIR/instruction.txt"
mknod -m 0600 "$ROOTFS_DIR/dev/console" c 5 1
(
    cd "$ROOTFS_DIR"
    find . -print0 | cpio --null --create --format=newc > "$INITRAMFS"
)

set +e
timeout "${QEMU_TIMEOUT:-90}s" "$QEMU" \
    -machine virt,secure=off,gic-version=3 \
    -nodefaults \
    -cpu "${QEMU_CPU:-max}" \
    -m 1024 \
    -smp 2 \
    -kernel "$KERNEL_IMAGE" \
    -initrd "$INITRAMFS" \
    -append "console=ttyAMA0,115200 earlycon=pl011,mmio,0x09000000 loglevel=8 rdinit=/init panic=-1" \
    -display none \
    -serial stdio \
    -monitor none \
    -no-reboot </dev/null 2>&1 | tee "$LOG"
qemu_status=${PIPESTATUS[0]}
set -e

if [[ "$qemu_status" -eq 124 ]]; then
    echo "PTEBP_TEST_QEMU_TIMEOUT" >&2
    exit 124
fi
if [[ "$qemu_status" -ne 0 ]]; then
    echo "PTEBP_TEST_QEMU_FAILED status=$qemu_status" >&2
    exit "$qemu_status"
fi
grep -q '^PTEBP_TEST_QEMU_PASS$' "$LOG"
if grep -Eq 'Kernel panic|Internal error:|BUG:|WARNING:|PTEBP_TEST_.*FAIL' "$LOG"; then
    echo "PTEBP_TEST_QEMU_GUEST_FAILED" >&2
    exit 1
fi
