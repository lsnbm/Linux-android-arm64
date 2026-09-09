SHELL := /bin/bash
.ONESHELL:
.DEFAULT_GOAL := help

VERSION ?= 6.1-Android14
KERNELS_ROOT ?= /root

.PHONY: help module modules device-binaries build clean

help:
	@printf '%s\n' \
		'usage: make [VERSION=6.1-Android14] <target>' \
		'' \
		'  module     build one kernel test module selected by VERSION' \
		'  modules    build kernel test modules for all supported versions' \
		'  device-binaries  build Android ARM64 test binaries' \
		'  build      build all kernel modules and Android test binaries' \
		'  clean      remove generated headers, binaries, and Kbuild output'

module:
	KERNELS_ROOT='$(KERNELS_ROOT)' ./build_kernel_executor_test.sh '$(VERSION)'

modules:
	KERNELS_ROOT='$(KERNELS_ROOT)' ./build_kernel_executor_test.sh

device-binaries:
	KERNELS_ROOT='$(KERNELS_ROOT)' ./build_android_executor_tests.sh

build: modules device-binaries

clean:
	rm -f arm64_instruction_table.h \
		arm64_kernel_executor_test_module.ko \
		arm64_kernel_executor_test_module.lds \
		[0-9]*-Android*.ko \
		executor_test_runner \
		real-device-executor-test.log
	find . ../../arm64_decode ../../arm64_emulate -type f \( \
		-name '*.o' -o \
		-name '*.o.d' -o \
		-name '*.mod' -o \
		-name '*.mod.c' -o \
		-name '*.order' -o \
		-name '*.symvers' -o \
		-name '*.cmd' -o \
		-name '*.usyms' -o \
		-name '*.o.tmp' \
	\) -delete
	find . -type d -name '.tmp_versions' -prune -exec rm -rf -- {} +