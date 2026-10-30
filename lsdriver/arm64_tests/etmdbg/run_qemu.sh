#!/bin/bash

set -euo pipefail

BUILD_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
KERNELS_ROOT="${KERNELS_ROOT:-/root}"
VERSION="${VERSION:-6.1-Android14}"
KERNEL_DIR="${KERNELS_ROOT}/${VERSION}"
KERNEL_BUILD="${KERNEL_BUILD:-$(readlink -f "${KERNEL_DIR}/bazel-bin/common/kernel_aarch64")}"
KERNEL_IMAGE="${KERNEL_IMAGE:-$(find "${KERNEL_DIR}/out/bazel/output_user_root" -path '*/execroot/__main__/bazel-out/k8-fastbuild/bin/common/kernel_aarch64/Image' -type f -print -quit)}"
CLANG_DIR="${CLANG_DIR:-${KERNEL_DIR}/prebuilts/clang/host/linux-x86/clang-r487747c}"
QEMU="${QEMU:-qemu-system-aarch64}"
WORK_DIR="${WORK_DIR:-/tmp/lsdriver-etmdbg-qemu}"
ROOTFS_DIR="${WORK_DIR}/rootfs"
INITRAMFS="${WORK_DIR}/initramfs.cpio"
INIT="${WORK_DIR}/init"
INIT_SOURCE="${WORK_DIR}/qemu_init.S"
MODULE="${BUILD_ROOT}/arm64_etmdbg_test_module.ko"
LOG="${BUILD_ROOT}/qemu.log"
SOURCE="${BUILD_ROOT}/../dptdbg/qemu_init.S"

test -f "${MODULE}"
test -f "${KERNEL_IMAGE}"
test -f "${SOURCE}"
test -x "${CLANG_DIR}/bin/clang"
command -v "${QEMU}" >/dev/null
command -v cpio >/dev/null

rm -rf "${WORK_DIR}"
mkdir -p "${ROOTFS_DIR}/dev"
sed -e 's/arm64_dptdbg_test_module/arm64_etmdbg_test_module/g' \
    -e 's/DPTDBG_TEST/ETMDBG_TEST/g' \
    "${SOURCE}" > "${INIT_SOURCE}"

"${CLANG_DIR}/bin/clang" --target=aarch64-linux-gnu -c -nostdlib \
    -o "${WORK_DIR}/qemu_init.o" "${INIT_SOURCE}"
"${CLANG_DIR}/bin/ld.lld" -m aarch64elf -e _start -o "${INIT}" "${WORK_DIR}/qemu_init.o"
chmod 0755 "${INIT}"
cp "${INIT}" "${ROOTFS_DIR}/init"
cp "${MODULE}" "${ROOTFS_DIR}/arm64_etmdbg_test_module.ko"
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
    -kernel "${KERNEL_IMAGE}" \
    -initrd "${INITRAMFS}" \
    -append "console=ttyAMA0,115200 earlycon=pl011,mmio,0x09000000 loglevel=8 rdinit=/init panic=-1" \
    -display none \
    -serial stdio \
    -monitor none \
    -no-reboot </dev/null 2>&1 | tee "${LOG}"
QEMU_STATUS=${PIPESTATUS[0]}
set -e

if [[ "${QEMU_STATUS}" -eq 124 ]]; then
    echo "ETMDBG_TEST_TIMEOUT" >&2
    exit 124
fi
if [[ "${QEMU_STATUS}" -ne 0 ]]; then
    echo "ETMDBG_TEST_QEMU_FAILED status=${QEMU_STATUS}" >&2
    exit "${QEMU_STATUS}"
fi
if grep -q '^ETMDBG_TEST_PASS$' "${LOG}"; then
    echo "ETMDBG_TEST_QEMU_TRACE_PASS"
    exit 0
fi
if grep -q 'ETMDBG_TEST_UNSUPPORTED' "${LOG}"; then
    echo "ETMDBG_TEST_QEMU_UNSUPPORTED_OK"
    exit 0
fi
if grep -Eq 'Kernel panic|Internal error:|BUG:|WARNING:|ETMDBG_TEST_.*FAILED' "${LOG}"; then
    echo "ETMDBG_TEST_QEMU_GUEST_FAILED" >&2
    exit 1
fi
echo "ETMDBG_TEST_QEMU_NO_RESULT" >&2
exit 1