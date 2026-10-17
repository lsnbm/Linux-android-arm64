SHELL := /bin/bash
.ONESHELL:
.DEFAULT_GOAL := help

VERSION ?= 6.1-Android14
KERNELS_ROOT ?= /root
KERNEL_DIR := $(KERNELS_ROOT)/$(VERSION)
KERNEL_OUT ?= $(KERNEL_DIR)/bazel-bin/common/kernel_aarch64
CLANG_DIR ?= $(KERNEL_DIR)/prebuilts/clang/host/linux-x86/clang-r487747c

.PHONY: help module user clean

help:
	@printf '%s\n' \
		'usage: make [VERSION=6.1-Android14] [KERNELS_ROOT=/root] <target>' \
		'' \
		'  module  build the direct ETM/TRBE test module' \
		'  user    build the userspace cs_etm perf AUX test' \
		'  clean   remove Kbuild output'

module:
	@test -d '$(KERNEL_OUT)'
	@test -x '$(CLANG_DIR)/bin/clang'
	env PATH='$(CLANG_DIR)/bin:/usr/bin:/bin' make -C '$(KERNEL_DIR)/common' \
		O='$(KERNEL_OUT)' \
		M='$(CURDIR)' \
		ARCH=arm64 LLVM=1 LLVM_IAS=1 CROSS_COMPILE=aarch64-linux-gnu- \
		KBUILD_MODPOST_WARN=1 CONFIG_EXTENDED_MODVERSIONS=n modules -j$$(nproc)

user:
	./build_perf_etm_user_test.sh

clean:
	find . -maxdepth 1 -type f \( \
		-name '*.o' -o -name '*.o.d' -o -name '*.mod' -o -name '*.mod.c' \
		-name '*.ko' -o -name '*.symvers' -o -name '*.order' -o -name '*.usyms' \
		-name '*.cmd' -o -name '.*.cmd' -o -name 'qemu.log' \
		-name 'perf_etm_user_test' \
	\) -delete