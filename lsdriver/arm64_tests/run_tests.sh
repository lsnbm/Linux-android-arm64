#!/usr/bin/env bash
set -euo pipefail

test_dir="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
driver_dir="$(cd -- "$test_dir/.." && pwd)"
build_dir="$(mktemp -d "${TMPDIR:-/tmp}/lsdriver-arm64-tests.XXXXXX")"
trap 'rm -rf "$build_dir"' EXIT

cc="${CC:-gcc}"
cflags=(-std=gnu11 -O2 -Wall -Wextra -Werror)
decoder_sources=(
    "$driver_dir/arm64_decode/arm64_decode.c"
    "$driver_dir/arm64_decode/arm64_decode_base.c"
    "$driver_dir/arm64_decode/arm64_decode_ldst.c"
    "$driver_dir/arm64_decode/arm64_decode_branch.c"
    "$driver_dir/arm64_decode/arm64_decode_simd.c"
    "$driver_dir/arm64_decode/arm64_decode_sve.c"
    "$driver_dir/arm64_decode/arm64_decode_sme.c"
)

encoder_sources=(
    "$driver_dir/arm64_encode/arm64_encode.c"
)

mapfile -t executor_sources < <(
    find "$driver_dir/arm64_emulate" -maxdepth 1 -type f \
        \( -name '*.c' -o -name '*.h' \) -print
)

forbidden_context_helpers='(^|[^[:alnum:]_])(read_all_q_regs|write_all_q_regs|read_q_reg|write_q_reg|kernel_neon_begin|kernel_neon_end)[[:space:]]*\('
if grep -nHE "$forbidden_context_helpers" "${executor_sources[@]}"; then
    echo "ARM64 executor context audit: full-register save/restore helper found" >&2
    exit 1
fi

forbidden_asm_registers='(^|[^[:alnum:]_])([xw]([89]|1[0-9]|2[0-9]|30)|[vqsdhb]([4-9]|[12][0-9]|3[01]))([^[:alnum:]_]|$)'
forbidden_stack_registers='(^|[^[:alnum:]_])(sp|wsp)([^[:alnum:]_]|$)'
register_violations=$(grep -nHE "$forbidden_asm_registers" "${executor_sources[@]}" || true)
stack_violations=$(grep -nHE "$forbidden_stack_registers" "${executor_sources[@]}" | grep -v -- '->sp' || true)
if [[ -n "$register_violations$stack_violations" ]]; then
    printf '%s\n%s\n' "$register_violations" "$stack_violations" >&2
    echo "ARM64 executor context audit: explicit asm saves/restores or exceeds x0-x7/v0-v3" >&2
    exit 1
fi

echo "ARM64 executor context audit: PASS"

if [[ -f "$test_dir/arm64_decode_test.c" ]]; then
    "$cc" "${cflags[@]}" "$test_dir/arm64_decode_test.c" \
        "${decoder_sources[@]}" -o "$build_dir/arm64_decode_test"
    "$build_dir/arm64_decode_test"
    echo "ARM64 decoder tests: PASS"
fi

if [[ -f "$test_dir/arm64_encode_test.c" ]]; then
    "$cc" "${cflags[@]}" "$test_dir/arm64_encode_test.c" \
        "${encoder_sources[@]}" -o "$build_dir/arm64_encode_test"
    "$build_dir/arm64_encode_test"
    echo "ARM64 encoder tests: PASS"
fi

if [[ -f "$test_dir/arm64_page_reloc_test.c" ]]; then
    "$cc" "${cflags[@]}" "$test_dir/arm64_page_reloc_test.c" \
        "${decoder_sources[@]}" "${encoder_sources[@]}" -o "$build_dir/arm64_page_reloc_test"
    "$build_dir/arm64_page_reloc_test"
    echo "ARM64 page relocation tests: PASS"
fi