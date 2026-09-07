#!/usr/bin/env bash

set -euo pipefail

VERSION="${1:-6.1-Android14}"
KERNELS_ROOT="${KERNELS_ROOT:-/root}"
TEST_DIR="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
KERNEL_DIR="$KERNELS_ROOT/$VERSION"
DRIVER_SRC="$(cd -- "$TEST_DIR/../.." && pwd)"

case "$VERSION" in
    6.1-Android14)
        CLANG_PATH="$KERNEL_DIR/prebuilts/clang/host/linux-x86/clang-r487747c"
        ;;
    *)
        echo "unsupported executor test kernel: $VERSION" >&2
        exit 2
        ;;
esac

if [[ ! -d "$KERNEL_DIR" || ! -x "$CLANG_PATH/bin/clang" ]]; then
    echo "kernel or clang toolchain not found for $VERSION" >&2
    exit 1
fi

clean_test_build() {
    find "$TEST_DIR" -type f \( \
        -name '*.o' -o \
        -name '*.o.d' -o \
        -name '*.mod' -o \
        -name '*.mod.c' -o \
        -name '*.order' -o \
        -name '*.symvers' -o \
        -name '*.cmd' -o \
        -name '*.usyms' \
    \) -delete
    find "$TEST_DIR" -type d -name '.tmp_versions' -prune -exec rm -rf -- {} +
}

trap clean_test_build EXIT

cd "$KERNEL_DIR"

{
    echo '#ifndef ARM64_INSTRUCTION_TABLE_H'
    echo '#define ARM64_INSTRUCTION_TABLE_H'
    instruction_count="$(awk '
        {
            gsub(/\r/, "")
            if ($0 !~ /[^[:space:]]/) next
            if ($0 !~ /^[[:xdigit:]]{8}$/) {
                printf "invalid instruction.txt line %u: %s\n", NR, $0 > "/dev/stderr"
                invalid = 1
                next
            }
            count++
        }
        END {
            if (invalid) exit 1
            print count + 0
        }
    ' "$TEST_DIR/../instruction.txt")" || exit 1
    echo "#define ARM64_TEST_INSTRUCTION_COUNT ${instruction_count}U"
    echo '#include <linux/types.h>'
    echo 'static const u32 arm64_test_instructions[ARM64_TEST_INSTRUCTION_COUNT] = {'
    while IFS= read -r raw_instruction; do
        raw_instruction="${raw_instruction%$'\r'}"
        [[ "$raw_instruction" =~ [^[:space:]] ]] || continue
        [[ "$raw_instruction" =~ ^[[:xdigit:]]{8}$ ]] || exit 1
        printf '    0x%sU,\n' "$raw_instruction"
    done < "$TEST_DIR/../instruction.txt"
    echo '};'
    echo '#endif'
} > "$TEST_DIR/arm64_instruction_table.h"

bazel_out="$(readlink -f bazel-bin/common/kernel_aarch64 2>/dev/null || true)"
if [[ -z "$bazel_out" || ! -d "$bazel_out" || \
      ! -f "$bazel_out/../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz" ]]; then
    tools/bazel build //common:kernel_aarch64 //common:kernel_aarch64_modules_prepare
    bazel_out="$(readlink -f bazel-bin/common/kernel_aarch64)"
fi

if [[ -f "$bazel_out/../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz" ]]; then
    tar -xzf "$bazel_out/../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz" -C "$bazel_out"
fi

if [[ ! -f "$bazel_out/include/config/auto.conf" || \
      ! -f "$bazel_out/include/generated/asm-offsets.h" ]]; then
    echo "modules_prepare output is incomplete: $bazel_out" >&2
    exit 1
fi

symvers_file="$bazel_out/Module.symvers"
symvers_backup=""
if [[ -f "$symvers_file" ]]; then
    symvers_backup="$symvers_file.executor_test_bak.$$"
    mv "$symvers_file" "$symvers_backup"
fi

set +e
env PATH="$CLANG_PATH/bin:$PATH" \
    make -C "$KERNEL_DIR/common" \
        O="$bazel_out" \
        M="$TEST_DIR" \
        ARCH=arm64 \
        LLVM=1 \
        LLVM_IAS=1 \
        CONFIG_DEBUG_INFO_BTF_MODULES= \
        CROSS_COMPILE=aarch64-linux-gnu- \
        KBUILD_MODPOST_WARN=1 \
        CONFIG_EXTENDED_MODVERSIONS=n \
        modules -j"$(nproc)"
make_status=$?
set -e

if [[ -n "$symvers_backup" ]]; then
    mv "$symvers_backup" "$symvers_file"
fi

if [[ $make_status -ne 0 ]]; then
    exit "$make_status"
fi

test -f "$TEST_DIR/arm64_kernel_executor_test_module.ko"
echo "built: $TEST_DIR/arm64_kernel_executor_test_module.ko"