#!/bin/bash

set -euo pipefail

BUILD_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
KERNELS_ROOT="${KERNELS_ROOT:-/root}"
VERSION="${VERSION:-6.1-Android14}"
KERNEL_DIR="${KERNELS_ROOT}/${VERSION}"
KERNEL_BUILD="${KERNEL_BUILD:-$(readlink -f "${KERNEL_DIR}/bazel-bin/common/kernel_aarch64")}"
CLANG_DIR="${CLANG_DIR:-${KERNEL_DIR}/prebuilts/clang/host/linux-x86/clang-r487747c}"
QEMU="${QEMU:-qemu-system-aarch64}"
WORK_DIR="${WORK_DIR:-/tmp/lsdriver-dptdbg-qemu}"
ROOTFS_DIR="${WORK_DIR}/rootfs"
INITRAMFS="${WORK_DIR}/initramfs.cpio"
INIT="${WORK_DIR}/init"
MODULE="${BUILD_ROOT}/arm64_dptdbg_test_module.ko"
LOG="${BUILD_ROOT}/qemu.log"

test -f "${MODULE}" || {
    echo "missing module: ${MODULE}" >&2
    exit 1
}
test -f "${KERNEL_BUILD}/Image" || {
    echo "missing kernel Image: ${KERNEL_BUILD}/Image" >&2
    exit 1
}
test -x "${CLANG_DIR}/bin/clang" || {
    echo "missing clang: ${CLANG_DIR}/bin/clang" >&2
    exit 1
}
command -v "${QEMU}" >/dev/null || {
    echo "missing QEMU: ${QEMU}" >&2
    exit 1
}

rm -rf "${WORK_DIR}"
mkdir -p "${ROOTFS_DIR}/dev"

"${CLANG_DIR}/bin/clang" --target=aarch64-linux-gnu -c -nostdlib -o "${WORK_DIR}/qemu_init.o" "${BUILD_ROOT}/qemu_init.S"
"${CLANG_DIR}/bin/ld.lld" -m aarch64elf -e _start -o "${INIT}" "${WORK_DIR}/qemu_init.o"
chmod 0755 "${INIT}"
cp "${INIT}" "${ROOTFS_DIR}/init"
cp "${MODULE}" "${ROOTFS_DIR}/arm64_dptdbg_test_module.ko"
mknod -m 0600 "${ROOTFS_DIR}/dev/console" c 5 1

(
    cd "${ROOTFS_DIR}"
    find . -print0 | cpio --null --create --format=newc > "${INITRAMFS}"
)

set +e
timeout "${QEMU_TIMEOUT:-60}s" "${QEMU}" \
    -machine virt,secure=off,gic-version=3 \
    -nodefaults \
    -cpu "${QEMU_CPU:-max}" \
    -m 1024 \
    -smp 2 \
    -kernel "${KERNEL_BUILD}/Image" \
    -initrd "${INITRAMFS}" \
    -append "console=ttyAMA0,115200 earlycon=pl011,mmio,0x09000000 loglevel=8 rdinit=/init panic=-1" \
    -display none \
    -serial stdio \
    -monitor none \
    -no-reboot </dev/null 2>&1 | tee "${LOG}"
QEMU_STATUS=${PIPESTATUS[0]}
set -e

if [[ "${QEMU_STATUS}" -eq 124 ]]; then
    echo "DPTDBG_TEST_TIMEOUT" >&2
    exit 124
fi

if [[ "${QEMU_STATUS}" -ne 0 ]]; then
    echo "DPTDBG_TEST_QEMU_FAILED status=${QEMU_STATUS}" >&2
    exit "${QEMU_STATUS}"
fi

grep -q '^DPTDBG_TEST_LOAD_OK' "${LOG}"
grep -q 'DPTDBG_TEST_MONITOR_STARTED' "${LOG}"
grep -q '^DPTDBG_TEST_RESULT_OK' "${LOG}"
grep -q 'DPTDBG_TEST_FUNCTION_PASS hits=3' "${LOG}"
grep -q 'DPTDBG_TEST_UNLOAD_OK' "${LOG}"
if grep -Eq 'Kernel panic|Internal error:|BUG:|WARNING:|DPTDBG_TEST_.*FAILED' "${LOG}"; then
    echo "DPTDBG_TEST_GUEST_FAILED" >&2
    exit 1
fi
