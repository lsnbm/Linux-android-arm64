#!/usr/bin/env bash

set -euo pipefail

VERSION="${1:-6.1-Android14}"
KERNEL_ROOT="${KERNELS_ROOT:-/root}/$VERSION"
TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
CLANG="$KERNEL_ROOT/prebuilts/clang/host/linux-x86/clang-r487747c/bin/clang"
MODULE="$TEST_DIR/arm64_kernel_executor_test_module.ko"
RUNNER="$TEST_DIR/executor_test_runner.c"
INSTRUCTIONS="$TEST_DIR/../instruction.txt"
OUTPUT="$TEST_DIR/executor-test-initramfs.cpio.gz"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/executor-initramfs.XXXXXX")"

trap 'rm -rf "$BUILD_DIR"' EXIT

if [[ ! -x "$CLANG" || ! -f "$MODULE" || ! -f "$RUNNER" || ! -f "$INSTRUCTIONS" ]]; then
    echo "missing ARM64 clang, executor module, runner, or instruction corpus" >&2
    exit 1
fi

"$CLANG" --target=aarch64-linux-gnu -nostdlib -static -fuse-ld=lld \
    -ffreestanding -fno-stack-protector -fno-pic -Os \
    -Wl,-e,_start -Wl,--build-id=none \
    "$TEST_DIR/executor_test_init.c" -o "$BUILD_DIR/init"

NDK_CLANG="$KERNEL_ROOT/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/bin/clang"
NDK_SYSROOT="$KERNEL_ROOT/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/sysroot"
if [[ ! -x "$NDK_CLANG" ]]; then
    echo "missing Android NDK clang: $NDK_CLANG" >&2
    exit 1
fi
"$NDK_CLANG" --target=aarch64-linux-android23 --sysroot="$NDK_SYSROOT" \
    -std=gnu11 -O2 -Wall -Wextra -Werror -fno-emulated-tls \
    -static \
    -I"$TEST_DIR" "$RUNNER" -o "$BUILD_DIR/executor_test_runner"

install -d "$BUILD_DIR/root/dev"
install -m 0755 "$BUILD_DIR/init" "$BUILD_DIR/root/init"
install -m 0755 "$BUILD_DIR/executor_test_runner" "$BUILD_DIR/root/executor_test_runner"
install -m 0644 "$MODULE" "$BUILD_DIR/root/arm64_kernel_executor_test_module.ko"
install -m 0644 "$INSTRUCTIONS" "$BUILD_DIR/root/instruction.txt"
mknod -m 0600 "$BUILD_DIR/root/dev/console" c 5 1
mknod -m 0600 "$BUILD_DIR/root/dev/arm64_executor_test" c 10 240

(
    cd "$BUILD_DIR/root"
    find . -print0 | cpio --null -o --format=newc --owner=0:0 2>/dev/null
) | gzip -9 > "$OUTPUT"
echo "built: $OUTPUT"