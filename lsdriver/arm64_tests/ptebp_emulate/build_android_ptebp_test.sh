#!/usr/bin/env bash

set -euo pipefail

RUNNER_TOOLCHAIN_VERSION="6.1-Android14"
KERNEL_ROOT="${KERNELS_ROOT:-/root}/${RUNNER_TOOLCHAIN_VERSION}"
TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
NDK_ROOT="$KERNEL_ROOT/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64"
NDK_CLANG="$NDK_ROOT/bin/clang"
NDK_SYSROOT="$NDK_ROOT/sysroot"
RUNNER_OUTPUT="$TEST_DIR/ptebp_emulate_test_runner"
RUNNER_TEMP="$RUNNER_OUTPUT.tmp.$$"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/ptebp-emulate-runner.XXXXXX")"

trap '
    status=$?
    rm -f "$RUNNER_TEMP"
    rm -rf "$BUILD_DIR"
    if [ "$status" -ne 0 ]; then
        rm -f "$RUNNER_OUTPUT"
    fi
    exit "$status"
' EXIT

if [[ ! -x "$NDK_CLANG" || ! -d "$NDK_SYSROOT" ]]; then
    echo "missing Android ARM64 runner toolchain under: $KERNEL_ROOT" >&2
    exit 1
fi

runner_build_id="$({
    sha256sum "$TEST_DIR/ptebp_emulate_test_runner.c" \
        "$TEST_DIR/ptebp_emulate_protocol.h" \
        "$TEST_DIR/build_android_ptebp_test.sh"
} | sha256sum | awk '{print $1}')"

COMMON_FLAGS=(
    --target=aarch64-linux-android23
    --sysroot="$NDK_SYSROOT"
    -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Werror -fno-emulated-tls
    -I"$TEST_DIR"
)

"$NDK_CLANG" "${COMMON_FLAGS[@]}" \
    -DPTEBP_EMULATE_RUNNER_BUILD_ID=\"$runner_build_id\" \
    -c "$TEST_DIR/ptebp_emulate_test_runner.c" -o "$BUILD_DIR/runner.o"

"$NDK_CLANG" "${COMMON_FLAGS[@]}" -static \
    "$BUILD_DIR/runner.o" -o "$RUNNER_TEMP"

embedded_build_id_count="$(strings "$RUNNER_TEMP" |
    grep -Fxc "ptebp_emulate_runner_build_id=$runner_build_id" || true)"
if [[ "$embedded_build_id_count" -lt 1 ]]; then
    echo "runner build identity does not match current inputs: $runner_build_id" >&2
    exit 1
fi
mv "$RUNNER_TEMP" "$RUNNER_OUTPUT"
echo "built: $RUNNER_OUTPUT build_id=$runner_build_id"
