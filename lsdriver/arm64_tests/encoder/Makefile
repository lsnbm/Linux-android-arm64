SHELL := /bin/bash
.ONESHELL:
.DEFAULT_GOAL := contract-test

TEST_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
DRIVER_DIR := $(abspath $(TEST_DIR)/../..)
CC ?= gcc

ENCODER_CFLAGS := -std=gnu11 -O2 -Wall -Wextra -Werror
ENCODER_SOURCES := \
	$(DRIVER_DIR)/arm64_encode/arm64_encode.c

.PHONY: contract-test test

contract-test test:
	set -euo pipefail
	build_dir="$$(mktemp -d "$${TMPDIR:-/tmp}/lsdriver-arm64-encoder.XXXXXX")"
	trap 'rm -rf "$$build_dir"' EXIT
	$(CC) $(ENCODER_CFLAGS) -I'$(DRIVER_DIR)' \
		'$(TEST_DIR)/arm64_encoder_contract_test.c' \
		$(ENCODER_SOURCES) \
		-o "$$build_dir/arm64_encoder_contract_test"
	"$$build_dir/arm64_encoder_contract_test"

# Encoder validation is host-only; it checks machine words and API contracts without executing ARM64 code.
