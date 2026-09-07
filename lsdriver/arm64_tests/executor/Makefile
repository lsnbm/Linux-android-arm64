SHELL := /bin/bash
.ONESHELL:
.DEFAULT_GOAL := help

VERSION ?= 6.1-Android14
KERNELS_ROOT ?= /root

.PHONY: help module initramfs build clean

help:
	@printf '%s\n' \
		'usage: make [VERSION=6.1-Android14] <target>' \
		'' \
		'  module     build the kernel test module' \
		'  initramfs  build the initramfs (also builds module)' \
		'  build      build module and initramfs' \
		'  clean      remove generated headers, images, and Kbuild output'

module:
	KERNELS_ROOT='$(KERNELS_ROOT)' ./build_kernel_executor_test.sh '$(VERSION)'

initramfs: module
	KERNELS_ROOT='$(KERNELS_ROOT)' ./build_executor_initramfs.sh '$(VERSION)'

build: initramfs

clean:
	rm -f arm64_instruction_table.h \
		arm64_kernel_executor_test_module.ko \
		executor-test-initramfs.cpio.gz
	find . -type f \( \
		-name '*.o' -o \
		-name '*.o.d' -o \
		-name '*.mod' -o \
		-name '*.mod.c' -o \
		-name '*.order' -o \
		-name '*.symvers' -o \
		-name '*.cmd' -o \
		-name '*.usyms' \
	\) -delete
	find . -type d -name '.tmp_versions' -prune -exec rm -rf -- {} +