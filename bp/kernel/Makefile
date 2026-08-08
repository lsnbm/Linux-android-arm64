# SPDX-License-Identifier: GPL-2.0
# bp Makefile - DDK kbuild; 7 GKI versions (5.10 .. 6.12).
# Optimization follows the kernel defaults (no -O3/-funroll-loops/
# -fstrict-aliasing/-Wno-error); warnings are errors by default.

obj-m := bp.o
bp-y := bp_main.o bp_cfi.o \
        emulate/arm64_encode/arm64_encode.o \
        emulate/arm64_decode/arm64_decode.o \
        emulate/arm64_decode/arm64_decode_base.o \
        emulate/arm64_decode/arm64_decode_branch.o \
        emulate/arm64_decode/arm64_decode_ldst.o \
        emulate/arm64_decode/arm64_decode_simd.o \
        emulate/arm64_decode/arm64_decode_sve.o \
        emulate/arm64_decode/arm64_decode_sme.o

ccflags-y := -I$(src) -I$(src)/emulate -I$(src)/reg -I$(src)/hook \
             -I$(src)/syms -I$(src)/abi \
             -mbranch-protection=bti -mcmodel=large \
             -fno-sanitize=cfi -fno-sanitize-cfi-cross-dso \
             -fno-sanitize=cfi-icall \
             -fno-asynchronous-unwind-tables -fno-unwind-tables \
             -Wframe-larger-than=8192 \
             -std=gnu11 \
             -Wno-declaration-after-statement

CFLAGS_bp_cfi.o := -fno-sanitize=cfi -fno-sanitize-cfi-cross-dso \
                   -fno-sanitize=cfi-icall

all:
	$(MAKE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) -C $(KDIR) M=$(CURDIR) clean
