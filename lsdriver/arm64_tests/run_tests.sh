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