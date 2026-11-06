#!/usr/bin/env bash
set -euo pipefail

TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
QEMU_ROOT="${QEMU_ROOT:-/root/lsdriver-qemu/6.1-Android14}"
KERNEL_IMAGE="${KERNEL_IMAGE:-${QEMU_ROOT}/kernel/Image}"
NDK_ROOT="${NDK_ROOT:-/root/6.1-Android14/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64}"
QEMU="${QEMU:-qemu-system-aarch64}"
MODULE="${TEST_DIR}/cpu_render_test.ko"
LOG_FILE="${TEST_DIR}/qemu.log"
ROOTFS_DIR="$(mktemp -d)"
INITRAMFS="$(mktemp)"
INIT="${ROOTFS_DIR}/init"

cleanup()
{
    rm -rf "${ROOTFS_DIR}"
    rm -f "${INITRAMFS}"
}
trap cleanup EXIT

test -x "${NDK_ROOT}/bin/clang"
test -f "${KERNEL_IMAGE}"
test -f "${MODULE}"
command -v "${QEMU}" >/dev/null

mkdir -p "${ROOTFS_DIR}/dev" "${ROOTFS_DIR}/proc" "${ROOTFS_DIR}/sys"
cp "${MODULE}" "${ROOTFS_DIR}/cpu_render_test.ko"
"${NDK_ROOT}/bin/clang" --target=aarch64-linux-android23 \
    --sysroot="${NDK_ROOT}/sysroot" -std=gnu11 -O2 -static \
    -fno-stack-protector -Wl,--build-id=none \
    -o "${INIT}" "${TEST_DIR}/test_init.c"
chmod 0755 "${INIT}"
mknod -m 0600 "${ROOTFS_DIR}/dev/console" c 5 1
mknod -m 0666 "${ROOTFS_DIR}/dev/null" c 1 3

(
    cd "${ROOTFS_DIR}"
    find . -print0 | cpio --null --create --format=newc > "${INITRAMFS}"
)

set +e
timeout "${QEMU_TIMEOUT:-60}s" "${QEMU}" \
    -machine virt,secure=off,gic-version=3 \
    -nodefaults \
    -cpu max \
    -m 512 \
    -smp 2 \
    -kernel "${KERNEL_IMAGE}" \
    -initrd "${INITRAMFS}" \
    -append "console=ttyAMA0,115200 earlycon=pl011,mmio,0x09000000 loglevel=8 rdinit=/init panic=-1" \
    -display none \
    -serial stdio \
    -monitor none \
    -no-reboot </dev/null 2>&1 | tee "${LOG_FILE}"
QEMU_STATUS=${PIPESTATUS[0]}
set -e

if [[ "${QEMU_STATUS}" -eq 124 ]]; then
    echo "CPU_RENDER_TEST_TIMEOUT" >&2
    exit 124
fi
if [[ "${QEMU_STATUS}" -ne 0 ]]; then
    echo "CPU_RENDER_TEST_EXIT status=${QEMU_STATUS}" >&2
    exit "${QEMU_STATUS}"
fi
grep -Eq '^CPU_RENDER_TEST_PASS[[:space:]]*$' "${LOG_FILE}"
if grep -Eq 'Kernel panic|Internal error:|BUG:|Oops:|CPU_RENDER_.*FAIL' "${LOG_FILE}"; then
    echo "CPU_RENDER_TEST_GUEST_FAILED" >&2
    exit 1
fi
