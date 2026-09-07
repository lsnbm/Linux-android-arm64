SHELL := /bin/bash
.ONESHELL:
.DEFAULT_GOAL := strict-test

TEST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
DRIVER_DIR := $(abspath $(TEST_DIR)/../..)
CC ?= gcc
PYTHON ?= python3
LLVM_TOOLCHAIN ?= /root/6.1-Android14/prebuilts/clang/host/linux-x86/clang-r487747c
override EXPECTED_ROWS := 8339
override EXPECTED_UNIQUE_WORDS := 2423
override EXPECTED_INPUT_SHA256 := 0351EFC1317351C13479F3B74ED015F4C7CB8897A22D357896282811F0C067B1
LLVM_SOURCE_REVISION := d9f89f4d16663d5012e5c09495f3b30ece3d2362

DECODER_CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror
DECODER_SOURCES := \
	$(DRIVER_DIR)/arm64_decode/arm64_decode.c \
	$(DRIVER_DIR)/arm64_decode/arm64_decode_immediate.c \
	$(DRIVER_DIR)/arm64_decode/arm64_decode_register.c \
	$(DRIVER_DIR)/arm64_decode/arm64_decode_ldst.c \
	$(DRIVER_DIR)/arm64_decode/arm64_decode_branch.c \
	$(DRIVER_DIR)/arm64_decode/arm64_decode_simd.c \
	$(DRIVER_DIR)/arm64_decode/arm64_decode_sve.c \
	$(DRIVER_DIR)/arm64_decode/arm64_decode_sme.c

.PHONY: strict-test dynamic-bitfield-test test

strict-test test:
	set -euo pipefail
	instruction_file='$(TEST_DIR)/../instruction.txt'
	identity_map='$(TEST_DIR)/arm64_llvm_identity.tsv'
	if [[ ! -f "$$instruction_file" || ! -f "$$identity_map" ]]; then
		echo 'missing strict decoder input or identity map' >&2
		exit 1
	fi
	input_sha256="$$(sha256sum "$$instruction_file" | awk '{print toupper($$1)}')"
	if [[ "$$input_sha256" != '$(EXPECTED_INPUT_SHA256)' ]]; then
		echo "unexpected instruction.txt SHA-256: $$input_sha256" >&2
		exit 1
	fi
	unique_words="$$(sed 's/[[:space:]]*$$//' "$$instruction_file" | sed '/^[[:space:]]*$$/d' | tr '[:lower:]' '[:upper:]' | sort -u | wc -l)"
	if [[ "$$unique_words" != '$(EXPECTED_UNIQUE_WORDS)' ]]; then
		echo "unexpected instruction.txt unique word count: $$unique_words" >&2
		exit 1
	fi
	if [[ ! -x '$(LLVM_TOOLCHAIN)/bin/clang++' || ! -f '$(LLVM_TOOLCHAIN)/lib/libLLVM-17.so' ]]; then
		echo 'missing Android LLVM 17.0.2 toolchain' >&2
		exit 1
	fi
	llvm_version="$$('$(LLVM_TOOLCHAIN)/bin/clang++' --version)"
	if [[ "$$llvm_version" != *'clang version 17.0.2'* || \
	      "$$llvm_version" != *'r487747c'* || \
	      "$$llvm_version" != *'$(LLVM_SOURCE_REVISION)'* ]]; then
		echo 'strict decoder audit requires Android clang-r487747c LLVM 17.0.2' >&2
		exit 1
	fi
	build_dir="$$(mktemp -d "$${TMPDIR:-/tmp}/lsdriver-arm64-strict.XXXXXX")"
	trap 'rm -rf "$$build_dir"' EXIT
	$(CC) $(DECODER_CFLAGS) '$(TEST_DIR)/arm64_instruction_decoder.c' \
		$(DECODER_SOURCES) -o "$$build_dir/arm64_instruction_decoder"
	"$$build_dir/arm64_instruction_decoder" "$$instruction_file" > "$$build_dir/decoder.tsv"
	'$(LLVM_TOOLCHAIN)/bin/clang++' -stdlib=libc++ -std=c++17 -O2 \
		-Wall -Wextra -Werror -isystem '$(LLVM_TOOLCHAIN)/include' \
		'$(TEST_DIR)/arm64_llvm_strict_audit.cpp' \
		-L'$(LLVM_TOOLCHAIN)/lib' -lLLVM-17 \
		-Wl,-rpath,'$(LLVM_TOOLCHAIN)/lib' \
		-o "$$build_dir/arm64_llvm_strict_audit"
	LD_LIBRARY_PATH='$(LLVM_TOOLCHAIN)/lib'$${LD_LIBRARY_PATH:+:"$$LD_LIBRARY_PATH"} \
		"$$build_dir/arm64_llvm_strict_audit" "$$instruction_file" > "$$build_dir/llvm.tsv"
	$(PYTHON) '$(TEST_DIR)/arm64_strict_decoder_audit.py' \
		--expected-rows '$(EXPECTED_ROWS)' \
		--llvm-tsv "$$build_dir/llvm.tsv" \
		--identity-map "$$identity_map" \
		'$(DRIVER_DIR)/arm64_decode/arm64_decode.h' \
		"$$build_dir/decoder.tsv"
	echo 'ARM64 instruction.txt decoder/LLVM strict audit: PASS'

dynamic-bitfield-test:
	set -euo pipefail
	build_dir="$$(mktemp -d "$${TMPDIR:-/tmp}/lsdriver-arm64-bitfield.XXXXXX")"
	trap 'rm -rf "$$build_dir"' EXIT
	'$(LLVM_TOOLCHAIN)/bin/clang' --target=aarch64-linux-gnu --sysroot=/usr/aarch64-linux-gnu \
		-I/root/6.1-Android14/prebuilts/build-tools/sysroots/aarch64-unknown-linux-musl/include \
		-I/root/6.1-Android14/common/include -O2 \
		-ffreestanding -fno-stack-protector -fno-pic -nostdlib -static \
		-Wall -Wextra -Werror '$(TEST_DIR)/arm64_dynamic_bitfield_test.c' \
		-fuse-ld=lld -Wl,-e,_start -o "$$build_dir/arm64_dynamic_bitfield_test"
	qemu-aarch64 "$$build_dir/arm64_dynamic_bitfield_test"
	echo 'ARM64 dynamic bitfield templates: PASS (32/64-bit all immr/imms combinations)'
