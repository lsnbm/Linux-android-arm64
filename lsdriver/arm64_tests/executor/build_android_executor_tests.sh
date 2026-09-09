#!/usr/bin/env bash

set -euo pipefail

RUNNER_TOOLCHAIN_VERSION="6.1-Android14"
KERNEL_ROOT="${KERNELS_ROOT:-/root}/$RUNNER_TOOLCHAIN_VERSION"
TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
NDK_ROOT="$KERNEL_ROOT/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64"
NDK_CLANG="$NDK_ROOT/bin/clang"
NDK_SYSROOT="$NDK_ROOT/sysroot"
RUNNER_OUTPUT="$TEST_DIR/executor_test_runner"
RUNNER_TEMP="$RUNNER_OUTPUT.tmp.$$"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/arm64-executor-runner.XXXXXX")"

cleanup()
{
    status=$?
    rm -f "$RUNNER_TEMP"
    rm -rf "$BUILD_DIR"
    if [ "$status" -ne 0 ]; then
        rm -f "$RUNNER_OUTPUT"
    fi
    exit "$status"
}

trap cleanup EXIT

if [[ ! -x "$NDK_CLANG" || ! -d "$NDK_SYSROOT" ]]; then
    echo "missing Android ARM64 runner toolchain under: $KERNEL_ROOT" >&2
    exit 1
fi

rm -f "$RUNNER_OUTPUT"
runner_build_id="$({
    sha256sum "$TEST_DIR/executor_test_runner.c" \
        "$TEST_DIR/executor_protocol.h" \
        "$TEST_DIR/build_android_executor_tests.sh"
    find "$TEST_DIR/../../arm64_decode" -type f \( -name '*.c' -o -name '*.h' \) \
        -print0 | sort -z | xargs -0 sha256sum
} | sha256sum | awk '{print $1}')"

COMMON_FLAGS=(
    --target=aarch64-linux-android23
    --sysroot="$NDK_SYSROOT"
    -std=gnu11 -Wall -Wextra -Werror -fno-emulated-tls
    -I"$TEST_DIR" -I"$TEST_DIR/../.."
)

"$NDK_CLANG" "${COMMON_FLAGS[@]}" \
    -DARM64_EXECUTOR_RUNNER_BUILD_ID=\"$runner_build_id\" \
    -c "$TEST_DIR/executor_test_runner.c" -o "$BUILD_DIR/executor_test_runner.o"

decoder_objects=()
for source in "$TEST_DIR"/../../arm64_decode/*.c; do
    object="$BUILD_DIR/$(basename "${source%.c}").o"
    "$NDK_CLANG" "${COMMON_FLAGS[@]}" -Wno-unused-function \
        -c "$source" -o "$object"
    decoder_objects+=("$object")
done

"$NDK_CLANG" "${COMMON_FLAGS[@]}" -static \
    "$BUILD_DIR/executor_test_runner.o" "${decoder_objects[@]}" \
    -o "$RUNNER_TEMP"

embedded_build_id_count="$(strings "$RUNNER_TEMP" |
    grep -Fxc "arm64_executor_runner_build_id=$runner_build_id" || true)"
if [[ "$embedded_build_id_count" -lt 1 ]]; then
    echo "runner build identity does not match current inputs: $runner_build_id" >&2
    exit 1
fi
mv "$RUNNER_TEMP" "$RUNNER_OUTPUT"

echo "built: $RUNNER_OUTPUT build_id=$runner_build_id"