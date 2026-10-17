#!/usr/bin/env bash

set -euo pipefail

TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_ROOT="${KERNELS_ROOT:-/root}/6.1-Android14"
NDK_ROOT="$KERNEL_ROOT/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64"
NDK_CLANG="$NDK_ROOT/bin/clang"
NDK_SYSROOT="$NDK_ROOT/sysroot"
OUTPUT="$TEST_DIR/perf_etm_user_test"
BUILD_DIR="$(mktemp -d "${TMPDIR:-/tmp}/perf-etm-user.XXXXXX")"

trap 'status=$?; rm -rf "$BUILD_DIR"; exit "$status"' EXIT

test -x "$NDK_CLANG"
test -d "$NDK_SYSROOT"

COMMON_FLAGS=(
    --target=aarch64-linux-android23 \
    --sysroot="$NDK_SYSROOT" \
    -std=gnu11 -Wall -Wextra -Wno-unused-parameter -Werror -fno-emulated-tls
)

"$NDK_CLANG" "${COMMON_FLAGS[@]}" -c \
    "$TEST_DIR/perf_etm_user_test.c" -o "$BUILD_DIR/perf_etm_user_test.o"
"$NDK_CLANG" "${COMMON_FLAGS[@]}" -static \
    "$BUILD_DIR/perf_etm_user_test.o" -o "$OUTPUT"

echo "built: $OUTPUT"