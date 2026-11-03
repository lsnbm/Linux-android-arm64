ROOT_DIR := $(abspath ../../..)
KERNELS_ROOT ?= /root
VERSION ?= 6.1-Android14
KERNEL_DIR := $(KERNELS_ROOT)/$(VERSION)/common
KERNEL_BUILD ?= $(shell readlink -f $(KERNELS_ROOT)/$(VERSION)/bazel-bin/common/kernel_aarch64)
CLANG_DIR ?= $(KERNELS_ROOT)/$(VERSION)/prebuilts/clang/host/linux-x86/clang-r487747c

export PATH := $(CLANG_DIR)/bin:$(PATH)

.PHONY: module device-binaries qemu build clean

module:
	KBUILD_MODPOST_WARN=1 CONFIG_EXTENDED_MODVERSIONS=n \
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_BUILD) M=$(ROOT_DIR)/lsdriver/arm64_tests/ptebp_emulate \
		ARCH=arm64 LLVM=1 LLVM_IAS=1 modules

device-binaries:
	KERNELS_ROOT='$(KERNELS_ROOT)' ./build_android_ptebp_test.sh

qemu: module device-binaries
	KERNELS_ROOT='$(KERNELS_ROOT)' VERSION='$(VERSION)' KERNEL_BUILD='$(KERNEL_BUILD)' \
		CLANG_DIR='$(CLANG_DIR)' ./run_qemu.sh

build: module device-binaries

clean:
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_BUILD) M=$(ROOT_DIR)/lsdriver/arm64_tests/ptebp_emulate \
		ARCH=arm64 clean
	rm -f ptebp_emulate_test_runner ptebp_emulate_test_module.ko \
		ptebp_emulate_test_module.lds qemu.log
