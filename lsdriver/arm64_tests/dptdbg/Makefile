ROOT_DIR := $(abspath ../../..)
KERNELS_ROOT ?= /root
VERSION ?= 6.1-Android14
KERNEL_DIR := $(KERNELS_ROOT)/$(VERSION)/common
KERNEL_BUILD ?= $(shell readlink -f $(KERNELS_ROOT)/$(VERSION)/bazel-bin/common/kernel_aarch64)
CLANG_DIR ?= $(KERNELS_ROOT)/$(VERSION)/prebuilts/clang/host/linux-x86/clang-r487747c

export PATH := $(CLANG_DIR)/bin:$(PATH)

.PHONY: module qemu device-binaries clean

module:
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_BUILD) M=$(ROOT_DIR)/lsdriver/arm64_tests/dptdbg ARCH=arm64 LLVM=1 LLVM_IAS=1 modules

device-binaries: dptdbg_runner

dptdbg_runner: qemu_init.S
	$(CLANG_DIR)/bin/clang --target=aarch64-linux-gnu -fuse-ld=lld -nostdlib -static -Wl,-e,_start -o $@ $<

qemu: module device-binaries
	KERNELS_ROOT='$(KERNELS_ROOT)' VERSION='$(VERSION)' KERNEL_BUILD='$(KERNEL_BUILD)' ./run_qemu.sh

clean:
	$(MAKE) -C $(KERNEL_DIR) O=$(KERNEL_BUILD) M=$(ROOT_DIR)/lsdriver/arm64_tests/dptdbg ARCH=arm64 clean
	$(RM) dptdbg_runner