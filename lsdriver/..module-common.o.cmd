savedcmd_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/.module-common.o := clang -Wp,-MMD,/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/..module-common.o.d -nostdinc -I/root/6.12/common/arch/arm64/include -I./arch/arm64/include/generated -I/root/6.12/common/include -I./include -I/root/6.12/common/arch/arm64/include/uapi -I./arch/arm64/include/generated/uapi -I/root/6.12/common/include/uapi -I./include/generated/uapi -include /root/6.12/common/include/linux/compiler-version.h -include /root/6.12/common/include/linux/kconfig.h -include /root/6.12/common/include/linux/compiler_types.h -D__KERNEL__ --target=aarch64-linux-gnu -fintegrated-as -Werror=unknown-warning-option -Werror=ignored-optimization-argument -Werror=option-ignored -Werror=unused-command-line-argument -mlittle-endian -DKASAN_SHADOW_SCALE_SHIFT= -fmacro-prefix-map=/root/6.12/common/= -Werror -std=gnu11 -fshort-wchar -funsigned-char -fno-common -fno-PIE -fno-strict-aliasing -mgeneral-regs-only -DCONFIG_CC_HAS_K_CONSTRAINT=1 -Wno-psabi -fasynchronous-unwind-tables -mbranch-protection=pac-ret -Wa,-march=armv8.5-a -DARM64_ASM_ARCH='"armv8.5-a"' -ffixed-x18 -DKASAN_SHADOW_SCALE_SHIFT= -fno-delete-null-pointer-checks -O2 -fstack-protector-strong -fno-omit-frame-pointer -fno-optimize-sibling-calls -ftrivial-auto-var-init=zero -fno-stack-clash-protection -falign-functions=4 -fstrict-flex-arrays=3 -fno-strict-overflow -fno-stack-check -fno-builtin-wcslen -fno-builtin-wcslen -Wall -Wextra -Wundef -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Werror=strict-prototypes -Wno-format-security -Wno-trigraphs -Wno-frame-address -Wno-address-of-packed-member -Wframe-larger-than=2048 -Wno-gnu -Wno-format-overflow-non-kprintf -Wno-format-truncation-non-kprintf -Wvla -Wno-pointer-sign -Wcast-function-type -Wimplicit-fallthrough -Werror=date-time -Werror=incompatible-pointer-types -Wenum-conversion -Wunused -Wno-unused-but-set-variable -Wno-unused-const-variable -Wno-format-overflow -Wno-override-init -Wno-pointer-to-enum-cast -Wno-tautological-constant-out-of-range-compare -Wno-unaligned-access -Wno-enum-compare-conditional -Wno-missing-field-initializers -Wno-type-limits -Wno-shift-negative-value -Wno-enum-enum-conversion -Wno-sign-compare -Wno-unused-parameter -g -gdwarf-5 -gz=zstd -mstack-protector-guard=sysreg -mstack-protector-guard-reg=sp_el0 -mstack-protector-guard-offset=1808 -fsanitize=array-bounds -fsanitize=local-bounds -fsanitize-trap=undefined -fdebug-info-for-profiling -mllvm -enable-fs-discriminator=true -mllvm -improved-fs-discriminator=true -DMODULE -DKBUILD_BASENAME='".module_common"' -DKBUILD_MODNAME='".module_common.o"' -D__KBUILD_MODNAME=kmod_.module_common.o -c -o /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/.module-common.o /root/6.12/common/scripts/module-common.c

source_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/.module-common.o := /root/6.12/common/scripts/module-common.c

deps_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/.module-common.o := \
    $(wildcard include/config/UNWINDER_ORC) \
    $(wildcard include/config/MITIGATION_RETPOLINE) \
  /root/6.12/common/include/linux/compiler-version.h \
    $(wildcard include/config/CC_VERSION_TEXT) \
  /root/6.12/common/include/linux/kconfig.h \
    $(wildcard include/config/CPU_BIG_ENDIAN) \
    $(wildcard include/config/BOOGER) \
    $(wildcard include/config/FOO) \
  /root/6.12/common/include/linux/compiler_types.h \
    $(wildcard include/config/DEBUG_INFO_BTF) \
    $(wildcard include/config/PAHOLE_HAS_BTF_TAG) \
    $(wildcard include/config/FUNCTION_ALIGNMENT) \
    $(wildcard include/config/CC_HAS_SANE_FUNCTION_ALIGNMENT) \
    $(wildcard include/config/X86_64) \
    $(wildcard include/config/ARM64) \
    $(wildcard include/config/LD_DEAD_CODE_DATA_ELIMINATION) \
    $(wildcard include/config/LTO_CLANG) \
    $(wildcard include/config/HAVE_ARCH_COMPILER_H) \
    $(wildcard include/config/CC_HAS_COUNTED_BY) \
    $(wildcard include/config/UBSAN_SIGNED_WRAP) \
    $(wildcard include/config/CC_HAS_ASM_INLINE) \
  /root/6.12/common/include/linux/compiler_attributes.h \
  /root/6.12/common/include/linux/compiler-clang.h \
    $(wildcard include/config/ARCH_USE_BUILTIN_BSWAP) \
  /root/6.12/common/arch/arm64/include/asm/compiler.h \
    $(wildcard include/config/ARM64_PTR_AUTH_KERNEL) \
    $(wildcard include/config/ARM64_PTR_AUTH) \
    $(wildcard include/config/BUILTIN_RETURN_ADDRESS_STRIPS_PAC) \
  /root/6.12/common/include/linux/module.h \
    $(wildcard include/config/MODULES) \
    $(wildcard include/config/SYSFS) \
    $(wildcard include/config/MODULES_TREE_LOOKUP) \
    $(wildcard include/config/LIVEPATCH) \
    $(wildcard include/config/STACKTRACE_BUILD_ID) \
    $(wildcard include/config/ARCH_USES_CFI_TRAPS) \
    $(wildcard include/config/GENERIC_BUG) \
    $(wildcard include/config/KALLSYMS) \
    $(wildcard include/config/SMP) \
    $(wildcard include/config/TRACEPOINTS) \
    $(wildcard include/config/TREE_SRCU) \
    $(wildcard include/config/BPF_EVENTS) \
    $(wildcard include/config/DEBUG_INFO_BTF_MODULES) \
    $(wildcard include/config/JUMP_LABEL) \
    $(wildcard include/config/TRACING) \
    $(wildcard include/config/EVENT_TRACING) \
    $(wildcard include/config/FTRACE_MCOUNT_RECORD) \
    $(wildcard include/config/KPROBES) \
    $(wildcard include/config/HAVE_STATIC_CALL_INLINE) \
    $(wildcard include/config/KUNIT) \
    $(wildcard include/config/PRINTK_INDEX) \
    $(wildcard include/config/MODULE_UNLOAD) \
    $(wildcard include/config/MITIGATION_ITS) \
    $(wildcard include/config/CONSTRUCTORS) \
    $(wildcard include/config/FUNCTION_ERROR_INJECTION) \
    $(wildcard include/config/DYNAMIC_DEBUG_CORE) \
    $(wildcard include/config/MODULE_SIG) \
  /root/6.12/common/include/linux/list.h \
    $(wildcard include/config/LIST_HARDENED) \
    $(wildcard include/config/DEBUG_LIST) \
  /root/6.12/common/include/linux/container_of.h \
  /root/6.12/common/include/linux/build_bug.h \
  /root/6.12/common/include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/OBJTOOL) \
    $(wildcard include/config/64BIT) \
  /root/6.12/common/arch/arm64/include/asm/rwonce.h \
    $(wildcard include/config/LTO) \
    $(wildcard include/config/AS_HAS_LDAPR) \
  /root/6.12/common/include/asm-generic/rwonce.h \
  /root/6.12/common/include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
  /root/6.12/common/include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/ARCH_32BIT_USTAT_F_TINODE) \
  /root/6.12/common/include/uapi/linux/types.h \
  arch/arm64/include/generated/uapi/asm/types.h \
  /root/6.12/common/include/uapi/asm-generic/types.h \
  /root/6.12/common/include/asm-generic/int-ll64.h \
  /root/6.12/common/include/uapi/asm-generic/int-ll64.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/bitsperlong.h \
  /root/6.12/common/include/asm-generic/bitsperlong.h \
  /root/6.12/common/include/uapi/asm-generic/bitsperlong.h \
  /root/6.12/common/include/uapi/linux/posix_types.h \
  /root/6.12/common/include/linux/stddef.h \
  /root/6.12/common/include/uapi/linux/stddef.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/posix_types.h \
  /root/6.12/common/include/uapi/asm-generic/posix_types.h \
  /root/6.12/common/include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN) \
    $(wildcard include/config/KCSAN_WEAK_MEMORY) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  /root/6.12/common/include/linux/poison.h \
    $(wildcard include/config/ILLEGAL_POINTER_VALUE) \
  /root/6.12/common/include/linux/const.h \
  /root/6.12/common/include/vdso/const.h \
  /root/6.12/common/include/uapi/linux/const.h \
  /root/6.12/common/arch/arm64/include/asm/barrier.h \
    $(wildcard include/config/ARM64_PSEUDO_NMI) \
  /root/6.12/common/arch/arm64/include/asm/alternative-macros.h \
  /root/6.12/common/include/vdso/bits.h \
  /root/6.12/common/arch/arm64/include/asm/cpucaps.h \
    $(wildcard include/config/ARM64_PAN) \
    $(wildcard include/config/ARM64_EPAN) \
    $(wildcard include/config/ARM64_SVE) \
    $(wildcard include/config/ARM64_SME) \
    $(wildcard include/config/ARM64_CNP) \
    $(wildcard include/config/ARM64_MTE) \
    $(wildcard include/config/ARM64_BTI) \
    $(wildcard include/config/ARM64_TLB_RANGE) \
    $(wildcard include/config/UNMAP_KERNEL_AT_EL0) \
    $(wildcard include/config/ARM64_ERRATUM_843419) \
    $(wildcard include/config/ARM64_ERRATUM_1742098) \
    $(wildcard include/config/ARM64_ERRATUM_2645198) \
    $(wildcard include/config/ARM64_ERRATUM_2658417) \
    $(wildcard include/config/CAVIUM_ERRATUM_23154) \
    $(wildcard include/config/NVIDIA_CARMEL_CNP_ERRATUM) \
    $(wildcard include/config/ARM64_WORKAROUND_REPEAT_TLBI) \
    $(wildcard include/config/ARM64_ERRATUM_3194386) \
  arch/arm64/include/generated/asm/cpucap-defs.h \
  /root/6.12/common/arch/arm64/include/asm/insn-def.h \
  /root/6.12/common/arch/arm64/include/asm/brk-imm.h \
  /root/6.12/common/include/linux/stringify.h \
  /root/6.12/common/include/asm-generic/barrier.h \
  /root/6.12/common/include/linux/stat.h \
  /root/6.12/common/arch/arm64/include/asm/stat.h \
    $(wildcard include/config/COMPAT) \
  arch/arm64/include/generated/uapi/asm/stat.h \
  /root/6.12/common/include/uapi/asm-generic/stat.h \
  /root/6.12/common/include/linux/time.h \
    $(wildcard include/config/POSIX_TIMERS) \
  /root/6.12/common/include/linux/cache.h \
    $(wildcard include/config/ARCH_HAS_CACHE_LINE_SIZE) \
  /root/6.12/common/include/uapi/linux/kernel.h \
  /root/6.12/common/include/uapi/linux/sysinfo.h \
  /root/6.12/common/arch/arm64/include/asm/cache.h \
    $(wildcard include/config/KASAN_HW_TAGS) \
  /root/6.12/common/include/linux/bitops.h \
  /root/6.12/common/include/linux/bits.h \
  /root/6.12/common/include/uapi/linux/bits.h \
  /root/6.12/common/include/linux/overflow.h \
  /root/6.12/common/include/linux/limits.h \
  /root/6.12/common/include/uapi/linux/limits.h \
  /root/6.12/common/include/vdso/limits.h \
  /root/6.12/common/include/linux/typecheck.h \
  /root/6.12/common/include/asm-generic/bitops/generic-non-atomic.h \
  /root/6.12/common/arch/arm64/include/asm/bitops.h \
  /root/6.12/common/include/asm-generic/bitops/builtin-__ffs.h \
  /root/6.12/common/include/asm-generic/bitops/builtin-ffs.h \
  /root/6.12/common/include/asm-generic/bitops/builtin-__fls.h \
  /root/6.12/common/include/asm-generic/bitops/builtin-fls.h \
  /root/6.12/common/include/asm-generic/bitops/ffz.h \
  /root/6.12/common/include/asm-generic/bitops/fls64.h \
  /root/6.12/common/include/asm-generic/bitops/sched.h \
  /root/6.12/common/include/asm-generic/bitops/hweight.h \
  /root/6.12/common/include/asm-generic/bitops/arch_hweight.h \
  /root/6.12/common/include/asm-generic/bitops/const_hweight.h \
  /root/6.12/common/include/asm-generic/bitops/atomic.h \
  /root/6.12/common/include/linux/atomic.h \
  /root/6.12/common/arch/arm64/include/asm/atomic.h \
  /root/6.12/common/arch/arm64/include/asm/cmpxchg.h \
  /root/6.12/common/arch/arm64/include/asm/lse.h \
    $(wildcard include/config/ARM64_LSE_ATOMICS) \
  /root/6.12/common/arch/arm64/include/asm/atomic_ll_sc.h \
    $(wildcard include/config/CC_HAS_K_CONSTRAINT) \
  /root/6.12/common/include/linux/export.h \
    $(wildcard include/config/MODVERSIONS) \
    $(wildcard include/config/GENDWARFKSYMS) \
  /root/6.12/common/include/linux/linkage.h \
    $(wildcard include/config/ARCH_USE_SYM_ANNOTATIONS) \
  /root/6.12/common/arch/arm64/include/asm/linkage.h \
  /root/6.12/common/arch/arm64/include/asm/alternative.h \
  /root/6.12/common/include/linux/init.h \
    $(wildcard include/config/MEMORY_HOTPLUG) \
    $(wildcard include/config/HAVE_ARCH_PREL32_RELOCATIONS) \
  /root/6.12/common/arch/arm64/include/asm/atomic_lse.h \
  /root/6.12/common/include/linux/atomic/atomic-arch-fallback.h \
    $(wildcard include/config/GENERIC_ATOMIC64) \
  /root/6.12/common/include/linux/atomic/atomic-long.h \
  /root/6.12/common/include/linux/atomic/atomic-instrumented.h \
  /root/6.12/common/include/linux/instrumented.h \
  /root/6.12/common/include/linux/kmsan-checks.h \
    $(wildcard include/config/KMSAN) \
  /root/6.12/common/include/asm-generic/bitops/instrumented-atomic.h \
  /root/6.12/common/include/asm-generic/bitops/lock.h \
  /root/6.12/common/include/asm-generic/bitops/instrumented-lock.h \
  /root/6.12/common/include/asm-generic/bitops/non-atomic.h \
  /root/6.12/common/include/asm-generic/bitops/non-instrumented-non-atomic.h \
  /root/6.12/common/include/asm-generic/bitops/le.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/byteorder.h \
  /root/6.12/common/include/linux/byteorder/little_endian.h \
  /root/6.12/common/include/uapi/linux/byteorder/little_endian.h \
  /root/6.12/common/include/linux/swab.h \
  /root/6.12/common/include/uapi/linux/swab.h \
  arch/arm64/include/generated/uapi/asm/swab.h \
  /root/6.12/common/include/uapi/asm-generic/swab.h \
  /root/6.12/common/include/linux/byteorder/generic.h \
  /root/6.12/common/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /root/6.12/common/include/linux/kasan-enabled.h \
    $(wildcard include/config/KASAN) \
  /root/6.12/common/include/linux/static_key.h \
  /root/6.12/common/include/linux/jump_label.h \
    $(wildcard include/config/HAVE_ARCH_JUMP_LABEL_RELATIVE) \
  /root/6.12/common/arch/arm64/include/asm/jump_label.h \
  /root/6.12/common/arch/arm64/include/asm/insn.h \
  /root/6.12/common/arch/arm64/include/asm/cputype.h \
  /root/6.12/common/arch/arm64/include/asm/sysreg.h \
    $(wildcard include/config/BROKEN_GAS_INST) \
    $(wildcard include/config/ARM64_PA_BITS_52) \
    $(wildcard include/config/ARM64_4K_PAGES) \
    $(wildcard include/config/ARM64_16K_PAGES) \
    $(wildcard include/config/ARM64_64K_PAGES) \
  /root/6.12/common/include/linux/kasan-tags.h \
  /root/6.12/common/arch/arm64/include/asm/gpr-num.h \
  arch/arm64/include/generated/asm/sysreg-defs.h \
  /root/6.12/common/include/linux/bitfield.h \
  /root/6.12/common/arch/arm64/include/asm/mte-def.h \
  /root/6.12/common/include/linux/math64.h \
    $(wildcard include/config/ARCH_SUPPORTS_INT128) \
  /root/6.12/common/include/linux/math.h \
  arch/arm64/include/generated/asm/div64.h \
  /root/6.12/common/include/asm-generic/div64.h \
  /root/6.12/common/include/vdso/math64.h \
  /root/6.12/common/include/linux/time64.h \
  /root/6.12/common/include/vdso/time64.h \
  /root/6.12/common/include/uapi/linux/time.h \
  /root/6.12/common/include/uapi/linux/time_types.h \
  /root/6.12/common/include/linux/time32.h \
  /root/6.12/common/include/linux/timex.h \
  /root/6.12/common/include/uapi/linux/timex.h \
  /root/6.12/common/include/uapi/linux/param.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/param.h \
  /root/6.12/common/include/asm-generic/param.h \
    $(wildcard include/config/HZ) \
  /root/6.12/common/include/uapi/asm-generic/param.h \
  /root/6.12/common/arch/arm64/include/asm/timex.h \
  /root/6.12/common/arch/arm64/include/asm/arch_timer.h \
    $(wildcard include/config/ARM_ARCH_TIMER_OOL_WORKAROUND) \
  /root/6.12/common/arch/arm64/include/asm/hwcap.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/hwcap.h \
  /root/6.12/common/arch/arm64/include/asm/cpufeature.h \
    $(wildcard include/config/ARM64_SW_TTBR0_PAN) \
    $(wildcard include/config/ARM64_DEBUG_PRIORITY_MASKING) \
    $(wildcard include/config/ARM64_BTI_KERNEL) \
    $(wildcard include/config/ARM64_POE) \
    $(wildcard include/config/ARM64_PA_BITS) \
    $(wildcard include/config/ARM64_HW_AFDBM) \
    $(wildcard include/config/ARM64_AMU_EXTN) \
    $(wildcard include/config/ARM64_LPA2) \
  /root/6.12/common/include/linux/bug.h \
    $(wildcard include/config/BUG_ON_DATA_CORRUPTION) \
  /root/6.12/common/arch/arm64/include/asm/bug.h \
  /root/6.12/common/arch/arm64/include/asm/asm-bug.h \
    $(wildcard include/config/DEBUG_BUGVERBOSE) \
  /root/6.12/common/include/asm-generic/bug.h \
    $(wildcard include/config/BUG) \
    $(wildcard include/config/GENERIC_BUG_RELATIVE_POINTERS) \
  /root/6.12/common/include/linux/instrumentation.h \
    $(wildcard include/config/NOINSTR_VALIDATION) \
  /root/6.12/common/include/linux/once_lite.h \
  /root/6.12/common/include/linux/panic.h \
    $(wildcard include/config/PANIC_TIMEOUT) \
  /root/6.12/common/include/linux/printk.h \
    $(wildcard include/config/MESSAGE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_QUIET) \
    $(wildcard include/config/EARLY_PRINTK) \
    $(wildcard include/config/PRINTK) \
    $(wildcard include/config/DYNAMIC_DEBUG) \
  /root/6.12/common/include/linux/stdarg.h \
  /root/6.12/common/include/linux/kern_levels.h \
  /root/6.12/common/include/linux/ratelimit_types.h \
  /root/6.12/common/include/linux/spinlock_types_raw.h \
    $(wildcard include/config/DEBUG_SPINLOCK) \
    $(wildcard include/config/DEBUG_LOCK_ALLOC) \
  /root/6.12/common/arch/arm64/include/asm/spinlock_types.h \
  /root/6.12/common/include/asm-generic/qspinlock_types.h \
    $(wildcard include/config/NR_CPUS) \
  /root/6.12/common/include/asm-generic/qrwlock_types.h \
  /root/6.12/common/include/linux/lockdep_types.h \
    $(wildcard include/config/PROVE_RAW_LOCK_NESTING) \
    $(wildcard include/config/LOCKDEP) \
    $(wildcard include/config/LOCK_STAT) \
  /root/6.12/common/include/linux/kernel.h \
    $(wildcard include/config/PREEMPT_VOLUNTARY_BUILD) \
    $(wildcard include/config/PREEMPT_DYNAMIC) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_CALL) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_KEY) \
    $(wildcard include/config/PREEMPT_) \
    $(wildcard include/config/DEBUG_ATOMIC_SLEEP) \
    $(wildcard include/config/MMU) \
    $(wildcard include/config/PROVE_LOCKING) \
  /root/6.12/common/include/linux/align.h \
  /root/6.12/common/include/linux/array_size.h \
  /root/6.12/common/include/linux/hex.h \
  /root/6.12/common/include/linux/kstrtox.h \
  /root/6.12/common/include/linux/log2.h \
    $(wildcard include/config/ARCH_HAS_ILOG2_U32) \
    $(wildcard include/config/ARCH_HAS_ILOG2_U64) \
  /root/6.12/common/include/linux/minmax.h \
  /root/6.12/common/include/linux/sprintf.h \
  /root/6.12/common/include/linux/static_call_types.h \
    $(wildcard include/config/HAVE_STATIC_CALL) \
  /root/6.12/common/include/linux/instruction_pointer.h \
  /root/6.12/common/include/linux/wordpart.h \
  /root/6.12/common/include/linux/cpumask.h \
    $(wildcard include/config/FORCE_NR_CPUS) \
    $(wildcard include/config/HOTPLUG_CPU) \
    $(wildcard include/config/DEBUG_PER_CPU_MAPS) \
    $(wildcard include/config/CPUMASK_OFFSTACK) \
  /root/6.12/common/include/linux/cleanup.h \
  /root/6.12/common/include/linux/bitmap.h \
  /root/6.12/common/include/linux/errno.h \
  /root/6.12/common/include/uapi/linux/errno.h \
  arch/arm64/include/generated/uapi/asm/errno.h \
  /root/6.12/common/include/uapi/asm-generic/errno.h \
  /root/6.12/common/include/uapi/asm-generic/errno-base.h \
  /root/6.12/common/include/linux/find.h \
  /root/6.12/common/include/linux/string.h \
    $(wildcard include/config/BINARY_PRINTF) \
    $(wildcard include/config/FORTIFY_SOURCE) \
  /root/6.12/common/include/linux/args.h \
  /root/6.12/common/include/linux/err.h \
  /root/6.12/common/include/uapi/linux/string.h \
  /root/6.12/common/arch/arm64/include/asm/string.h \
    $(wildcard include/config/ARCH_HAS_UACCESS_FLUSHCACHE) \
  /root/6.12/common/include/linux/fortify-string.h \
    $(wildcard include/config/CC_HAS_KASAN_MEMINTRINSIC_PREFIX) \
    $(wildcard include/config/GENERIC_ENTRY) \
  /root/6.12/common/include/linux/bitmap-str.h \
  /root/6.12/common/include/linux/cpumask_types.h \
  /root/6.12/common/include/linux/threads.h \
    $(wildcard include/config/BASE_SMALL) \
  /root/6.12/common/include/linux/gfp_types.h \
    $(wildcard include/config/SLAB_OBJ_EXT) \
    $(wildcard include/config/CMA) \
  /root/6.12/common/include/linux/numa.h \
    $(wildcard include/config/NODES_SHIFT) \
    $(wildcard include/config/NUMA_KEEP_MEMINFO) \
    $(wildcard include/config/NUMA) \
    $(wildcard include/config/HAVE_ARCH_NODE_DEV_GROUP) \
  /root/6.12/common/include/linux/percpu.h \
    $(wildcard include/config/MEM_ALLOC_PROFILING) \
    $(wildcard include/config/RANDOM_KMALLOC_CACHES) \
    $(wildcard include/config/PAGE_SIZE_4KB) \
    $(wildcard include/config/NEED_PER_CPU_PAGE_FIRST_CHUNK) \
    $(wildcard include/config/HAVE_SETUP_PER_CPU_AREA) \
  /root/6.12/common/include/linux/alloc_tag.h \
    $(wildcard include/config/MEM_ALLOC_PROFILING_DEBUG) \
    $(wildcard include/config/MEM_ALLOC_PROFILING_ENABLED_BY_DEFAULT) \
  /root/6.12/common/include/linux/codetag.h \
    $(wildcard include/config/CODE_TAGGING) \
  /root/6.12/common/include/linux/preempt.h \
    $(wildcard include/config/PREEMPT_RT) \
    $(wildcard include/config/PREEMPT_COUNT) \
    $(wildcard include/config/DEBUG_PREEMPT) \
    $(wildcard include/config/TRACE_PREEMPT_TOGGLE) \
    $(wildcard include/config/PREEMPTION) \
    $(wildcard include/config/PREEMPT_NOTIFIERS) \
    $(wildcard include/config/PREEMPT_NONE) \
    $(wildcard include/config/PREEMPT_VOLUNTARY) \
    $(wildcard include/config/PREEMPT) \
  /root/6.12/common/arch/arm64/include/asm/preempt.h \
  /root/6.12/common/include/linux/thread_info.h \
    $(wildcard include/config/THREAD_INFO_IN_TASK) \
    $(wildcard include/config/HAVE_ARCH_WITHIN_STACK_FRAMES) \
    $(wildcard include/config/HARDENED_USERCOPY) \
    $(wildcard include/config/SH) \
  /root/6.12/common/include/linux/restart_block.h \
  /root/6.12/common/arch/arm64/include/asm/current.h \
  /root/6.12/common/arch/arm64/include/asm/thread_info.h \
    $(wildcard include/config/SHADOW_CALL_STACK) \
  /root/6.12/common/arch/arm64/include/asm/memory.h \
    $(wildcard include/config/ARM64_VA_BITS) \
    $(wildcard include/config/KASAN_SHADOW_OFFSET) \
    $(wildcard include/config/VMAP_STACK) \
    $(wildcard include/config/RANDOMIZE_BASE) \
    $(wildcard include/config/DEBUG_VIRTUAL) \
    $(wildcard include/config/EFI) \
    $(wildcard include/config/ARM_GIC_V3_ITS) \
  /root/6.12/common/include/linux/sizes.h \
  /root/6.12/common/arch/arm64/include/asm/page-def.h \
    $(wildcard include/config/PAGE_SHIFT) \
  /root/6.12/common/include/linux/mmdebug.h \
    $(wildcard include/config/DEBUG_VM) \
    $(wildcard include/config/DEBUG_VM_IRQSOFF) \
    $(wildcard include/config/DEBUG_VM_PGFLAGS) \
  /root/6.12/common/arch/arm64/include/asm/boot.h \
  /root/6.12/common/arch/arm64/include/asm/sections.h \
  /root/6.12/common/include/asm-generic/sections.h \
    $(wildcard include/config/HAVE_FUNCTION_DESCRIPTORS) \
  /root/6.12/common/include/asm-generic/memory_model.h \
    $(wildcard include/config/FLATMEM) \
    $(wildcard include/config/SPARSEMEM_VMEMMAP) \
    $(wildcard include/config/SPARSEMEM) \
  /root/6.12/common/include/linux/pfn.h \
  /root/6.12/common/arch/arm64/include/asm/stack_pointer.h \
  /root/6.12/common/arch/arm64/include/asm/percpu.h \
  /root/6.12/common/include/asm-generic/percpu.h \
  /root/6.12/common/include/linux/percpu-defs.h \
    $(wildcard include/config/DEBUG_FORCE_WEAK_PER_CPU) \
    $(wildcard include/config/AMD_MEM_ENCRYPT) \
  /root/6.12/common/include/linux/smp.h \
    $(wildcard include/config/UP_LATE_INIT) \
    $(wildcard include/config/CSD_LOCK_WAIT_DEBUG) \
  /root/6.12/common/include/linux/smp_types.h \
  /root/6.12/common/include/linux/llist.h \
    $(wildcard include/config/ARCH_HAVE_NMI_SAFE_CMPXCHG) \
  /root/6.12/common/arch/arm64/include/asm/smp.h \
    $(wildcard include/config/ARM64_ACPI_PARKING_PROTOCOL) \
  /root/6.12/common/include/linux/irqflags.h \
    $(wildcard include/config/TRACE_IRQFLAGS) \
    $(wildcard include/config/IRQSOFF_TRACER) \
    $(wildcard include/config/PREEMPT_TRACER) \
    $(wildcard include/config/DEBUG_IRQFLAGS) \
    $(wildcard include/config/TRACE_IRQFLAGS_SUPPORT) \
  /root/6.12/common/include/linux/irqflags_types.h \
  /root/6.12/common/arch/arm64/include/asm/irqflags.h \
  /root/6.12/common/arch/arm64/include/asm/ptrace.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/ptrace.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/sve_context.h \
  /root/6.12/common/include/linux/irqchip/arm-gic-v3-prio.h \
  /root/6.12/common/include/linux/sched.h \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_NATIVE) \
    $(wildcard include/config/SCHED_INFO) \
    $(wildcard include/config/SCHEDSTATS) \
    $(wildcard include/config/SCHED_CORE) \
    $(wildcard include/config/FAIR_GROUP_SCHED) \
    $(wildcard include/config/RT_GROUP_SCHED) \
    $(wildcard include/config/RT_MUTEXES) \
    $(wildcard include/config/UCLAMP_TASK) \
    $(wildcard include/config/UCLAMP_BUCKETS_COUNT) \
    $(wildcard include/config/KMAP_LOCAL) \
    $(wildcard include/config/SCHED_CLASS_EXT) \
    $(wildcard include/config/CGROUP_SCHED) \
    $(wildcard include/config/BLK_DEV_IO_TRACE) \
    $(wildcard include/config/PREEMPT_RCU) \
    $(wildcard include/config/TASKS_RCU) \
    $(wildcard include/config/TASKS_TRACE_RCU) \
    $(wildcard include/config/MEMCG_V1) \
    $(wildcard include/config/LRU_GEN) \
    $(wildcard include/config/COMPAT_BRK) \
    $(wildcard include/config/CGROUPS) \
    $(wildcard include/config/BLK_CGROUP) \
    $(wildcard include/config/PSI) \
    $(wildcard include/config/PAGE_OWNER) \
    $(wildcard include/config/EVENTFD) \
    $(wildcard include/config/ARCH_HAS_CPU_PASID) \
    $(wildcard include/config/CPU_SUP_INTEL) \
    $(wildcard include/config/TASK_DELAY_ACCT) \
    $(wildcard include/config/STACKPROTECTOR) \
    $(wildcard include/config/ARCH_HAS_SCALED_CPUTIME) \
    $(wildcard include/config/CPU_FREQ_TIMES) \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING_GEN) \
    $(wildcard include/config/NO_HZ_FULL) \
    $(wildcard include/config/POSIX_CPUTIMERS) \
    $(wildcard include/config/POSIX_CPU_TIMERS_TASK_WORK) \
    $(wildcard include/config/KEYS) \
    $(wildcard include/config/SYSVIPC) \
    $(wildcard include/config/DETECT_HUNG_TASK) \
    $(wildcard include/config/IO_URING) \
    $(wildcard include/config/AUDIT) \
    $(wildcard include/config/AUDITSYSCALL) \
    $(wildcard include/config/SCHED_PROXY_EXEC) \
    $(wildcard include/config/DETECT_HUNG_TASK_BLOCKER) \
    $(wildcard include/config/UBSAN) \
    $(wildcard include/config/UBSAN_TRAP) \
    $(wildcard include/config/COMPACTION) \
    $(wildcard include/config/TASK_XACCT) \
    $(wildcard include/config/CPUSETS) \
    $(wildcard include/config/X86_CPU_RESCTRL) \
    $(wildcard include/config/FUTEX) \
    $(wildcard include/config/PERF_EVENTS) \
    $(wildcard include/config/NUMA_BALANCING) \
    $(wildcard include/config/RSEQ) \
    $(wildcard include/config/SCHED_MM_CID) \
    $(wildcard include/config/FAULT_INJECTION) \
    $(wildcard include/config/LATENCYTOP) \
    $(wildcard include/config/FUNCTION_GRAPH_TRACER) \
    $(wildcard include/config/KCOV) \
    $(wildcard include/config/MEMCG) \
    $(wildcard include/config/UPROBES) \
    $(wildcard include/config/BCACHE) \
    $(wildcard include/config/SECURITY) \
    $(wildcard include/config/BPF_SYSCALL) \
    $(wildcard include/config/GCC_PLUGIN_STACKLEAK) \
    $(wildcard include/config/X86_MCE) \
    $(wildcard include/config/KRETPROBES) \
    $(wildcard include/config/RETHOOK) \
    $(wildcard include/config/ARCH_HAS_PARANOID_L1D_FLUSH) \
    $(wildcard include/config/RV) \
    $(wildcard include/config/USER_EVENTS) \
  /root/6.12/common/include/uapi/linux/sched.h \
  /root/6.12/common/arch/arm64/include/asm/processor.h \
    $(wildcard include/config/KUSER_HELPERS) \
    $(wildcard include/config/ARM64_FORCE_52BIT) \
    $(wildcard include/config/HAVE_HW_BREAKPOINT) \
    $(wildcard include/config/ARM64_TAGGED_ADDR_ABI) \
  /root/6.12/common/include/linux/android_vendor.h \
    $(wildcard include/config/ANDROID_VENDOR_OEM_DATA) \
  /root/6.12/common/include/vdso/processor.h \
  /root/6.12/common/arch/arm64/include/asm/vdso/processor.h \
  /root/6.12/common/arch/arm64/include/asm/hw_breakpoint.h \
    $(wildcard include/config/CPU_PM) \
  /root/6.12/common/arch/arm64/include/asm/virt.h \
    $(wildcard include/config/KVM) \
  /root/6.12/common/arch/arm64/include/asm/kasan.h \
  /root/6.12/common/arch/arm64/include/asm/mte-kasan.h \
  /root/6.12/common/arch/arm64/include/asm/pgtable-types.h \
    $(wildcard include/config/PGTABLE_LEVELS) \
  /root/6.12/common/include/asm-generic/pgtable-nopud.h \
  /root/6.12/common/include/asm-generic/pgtable-nop4d.h \
  /root/6.12/common/arch/arm64/include/asm/pgtable-hwdef.h \
    $(wildcard include/config/ARM64_CONT_PTE_SHIFT) \
    $(wildcard include/config/ARM64_CONT_PMD_SHIFT) \
    $(wildcard include/config/ARM64_VA_BITS_52) \
  /root/6.12/common/arch/arm64/include/asm/pointer_auth.h \
  /root/6.12/common/include/uapi/linux/prctl.h \
  /root/6.12/common/include/linux/random.h \
    $(wildcard include/config/VMGENID) \
  /root/6.12/common/include/uapi/linux/random.h \
  /root/6.12/common/include/uapi/linux/ioctl.h \
  arch/arm64/include/generated/uapi/asm/ioctl.h \
  /root/6.12/common/include/asm-generic/ioctl.h \
  /root/6.12/common/include/uapi/asm-generic/ioctl.h \
  /root/6.12/common/include/linux/irqnr.h \
  /root/6.12/common/include/uapi/linux/irqnr.h \
  /root/6.12/common/include/linux/prandom.h \
  /root/6.12/common/include/linux/once.h \
  /root/6.12/common/arch/arm64/include/asm/spectre.h \
  /root/6.12/common/arch/arm64/include/asm/fpsimd.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/sigcontext.h \
  /root/6.12/common/include/linux/pid_types.h \
  /root/6.12/common/include/linux/sem_types.h \
  /root/6.12/common/include/linux/shm.h \
  /root/6.12/common/arch/arm64/include/asm/page.h \
  /root/6.12/common/include/linux/personality.h \
  /root/6.12/common/include/uapi/linux/personality.h \
  /root/6.12/common/include/asm-generic/getorder.h \
  /root/6.12/common/arch/arm64/include/asm/shmparam.h \
  /root/6.12/common/include/asm-generic/shmparam.h \
  /root/6.12/common/include/linux/kmsan_types.h \
  /root/6.12/common/include/linux/mutex_types.h \
    $(wildcard include/config/MUTEX_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_MUTEXES) \
  /root/6.12/common/include/linux/osq_lock.h \
  /root/6.12/common/include/linux/spinlock_types.h \
  /root/6.12/common/include/linux/rwlock_types.h \
  /root/6.12/common/include/linux/plist_types.h \
  /root/6.12/common/include/linux/hrtimer_types.h \
  /root/6.12/common/include/linux/timerqueue_types.h \
  /root/6.12/common/include/linux/rbtree_types.h \
  /root/6.12/common/include/linux/timer_types.h \
  /root/6.12/common/include/linux/seccomp_types.h \
    $(wildcard include/config/SECCOMP) \
  /root/6.12/common/include/linux/nodemask_types.h \
  /root/6.12/common/include/linux/refcount_types.h \
  /root/6.12/common/include/linux/resource.h \
  /root/6.12/common/include/uapi/linux/resource.h \
  arch/arm64/include/generated/uapi/asm/resource.h \
  /root/6.12/common/include/asm-generic/resource.h \
  /root/6.12/common/include/uapi/asm-generic/resource.h \
  /root/6.12/common/include/linux/latencytop.h \
  /root/6.12/common/include/linux/sched/prio.h \
  /root/6.12/common/include/linux/sched/types.h \
  /root/6.12/common/include/linux/signal_types.h \
    $(wildcard include/config/OLD_SIGACTION) \
  /root/6.12/common/include/uapi/linux/signal.h \
  /root/6.12/common/arch/arm64/include/asm/signal.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/signal.h \
  /root/6.12/common/include/asm-generic/signal.h \
  /root/6.12/common/include/uapi/asm-generic/signal.h \
  /root/6.12/common/include/uapi/asm-generic/signal-defs.h \
  arch/arm64/include/generated/uapi/asm/siginfo.h \
  /root/6.12/common/include/uapi/asm-generic/siginfo.h \
  /root/6.12/common/include/linux/spinlock.h \
  /root/6.12/common/include/linux/bottom_half.h \
  /root/6.12/common/include/linux/lockdep.h \
    $(wildcard include/config/DEBUG_LOCKING_API_SELFTESTS) \
  arch/arm64/include/generated/asm/mmiowb.h \
  /root/6.12/common/include/asm-generic/mmiowb.h \
    $(wildcard include/config/MMIOWB) \
  /root/6.12/common/arch/arm64/include/asm/spinlock.h \
  arch/arm64/include/generated/asm/qspinlock.h \
  /root/6.12/common/include/asm-generic/qspinlock.h \
  arch/arm64/include/generated/asm/qrwlock.h \
  /root/6.12/common/include/asm-generic/qrwlock.h \
  /root/6.12/common/include/linux/rwlock.h \
  /root/6.12/common/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/INLINE_SPIN_LOCK) \
    $(wildcard include/config/INLINE_SPIN_LOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK) \
    $(wildcard include/config/INLINE_SPIN_TRYLOCK_BH) \
    $(wildcard include/config/UNINLINE_SPIN_UNLOCK) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_BH) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_SPIN_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/GENERIC_LOCKBREAK) \
  /root/6.12/common/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/INLINE_READ_LOCK) \
    $(wildcard include/config/INLINE_WRITE_LOCK) \
    $(wildcard include/config/INLINE_READ_LOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_LOCK_BH) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_WRITE_LOCK_IRQSAVE) \
    $(wildcard include/config/INLINE_READ_TRYLOCK) \
    $(wildcard include/config/INLINE_WRITE_TRYLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK) \
    $(wildcard include/config/INLINE_READ_UNLOCK_BH) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_BH) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQ) \
    $(wildcard include/config/INLINE_READ_UNLOCK_IRQRESTORE) \
    $(wildcard include/config/INLINE_WRITE_UNLOCK_IRQRESTORE) \
  /root/6.12/common/include/linux/syscall_user_dispatch_types.h \
  /root/6.12/common/include/linux/mm_types_task.h \
    $(wildcard include/config/ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH) \
  /root/6.12/common/arch/arm64/include/asm/tlbbatch.h \
  /root/6.12/common/include/linux/netdevice_xmit.h \
    $(wildcard include/config/NET_EGRESS) \
  /root/6.12/common/include/linux/task_io_accounting.h \
    $(wildcard include/config/TASK_IO_ACCOUNTING) \
  /root/6.12/common/include/linux/posix-timers_types.h \
  /root/6.12/common/include/uapi/linux/rseq.h \
  /root/6.12/common/include/linux/seqlock_types.h \
  /root/6.12/common/include/linux/kcsan.h \
  /root/6.12/common/include/linux/rv.h \
    $(wildcard include/config/RV_REACTORS) \
  /root/6.12/common/include/linux/livepatch_sched.h \
  /root/6.12/common/include/linux/uidgid_types.h \
  /root/6.12/common/include/linux/android_kabi.h \
  arch/arm64/include/generated/asm/kmap_size.h \
  /root/6.12/common/include/asm-generic/kmap_size.h \
    $(wildcard include/config/DEBUG_KMAP_LOCAL) \
  /root/6.12/common/include/linux/sched/ext.h \
    $(wildcard include/config/EXT_GROUP_SCHED) \
  /root/6.12/common/include/linux/rhashtable-types.h \
  /root/6.12/common/include/linux/mutex.h \
  /root/6.12/common/include/linux/debug_locks.h \
  /root/6.12/common/include/linux/workqueue_types.h \
  /root/6.12/common/include/clocksource/arm_arch_timer.h \
    $(wildcard include/config/ARM_ARCH_TIMER) \
  /root/6.12/common/include/linux/timecounter.h \
  /root/6.12/common/include/asm-generic/timex.h \
  /root/6.12/common/include/vdso/time32.h \
  /root/6.12/common/include/vdso/time.h \
  /root/6.12/common/arch/arm64/include/asm/compat.h \
  /root/6.12/common/include/asm-generic/compat.h \
    $(wildcard include/config/COMPAT_FOR_U64_ALIGNMENT) \
  /root/6.12/common/include/linux/sched/task_stack.h \
    $(wildcard include/config/STACK_GROWSUP) \
    $(wildcard include/config/DEBUG_STACK_USAGE) \
  /root/6.12/common/include/uapi/linux/magic.h \
  /root/6.12/common/include/linux/refcount.h \
  /root/6.12/common/include/linux/kasan.h \
    $(wildcard include/config/KASAN_STACK) \
    $(wildcard include/config/KASAN_VMALLOC) \
  /root/6.12/common/include/uapi/linux/stat.h \
  /root/6.12/common/include/linux/uidgid.h \
    $(wildcard include/config/MULTIUSER) \
    $(wildcard include/config/USER_NS) \
  /root/6.12/common/include/linux/highuid.h \
  /root/6.12/common/include/linux/buildid.h \
    $(wildcard include/config/VMCORE_INFO) \
  /root/6.12/common/include/linux/kmod.h \
  /root/6.12/common/include/linux/umh.h \
  /root/6.12/common/include/linux/gfp.h \
    $(wildcard include/config/HIGHMEM) \
    $(wildcard include/config/ZONE_DMA) \
    $(wildcard include/config/ZONE_DMA32) \
    $(wildcard include/config/ZONE_DEVICE) \
    $(wildcard include/config/CONTIG_ALLOC) \
  /root/6.12/common/include/linux/mmzone.h \
    $(wildcard include/config/ARCH_FORCE_MAX_ORDER) \
    $(wildcard include/config/PAGE_BLOCK_ORDER) \
    $(wildcard include/config/MEMORY_ISOLATION) \
    $(wildcard include/config/UNACCEPTED_MEMORY) \
    $(wildcard include/config/IOMMU_SUPPORT) \
    $(wildcard include/config/SWAP) \
    $(wildcard include/config/TRANSPARENT_HUGEPAGE) \
    $(wildcard include/config/LRU_GEN_STATS) \
    $(wildcard include/config/LRU_GEN_WALKS_MMU) \
    $(wildcard include/config/MEMORY_FAILURE) \
    $(wildcard include/config/PAGE_EXTENSION) \
    $(wildcard include/config/DEFERRED_STRUCT_PAGE_INIT) \
    $(wildcard include/config/HAVE_MEMORYLESS_NODES) \
    $(wildcard include/config/SPARSEMEM_EXTREME) \
    $(wildcard include/config/HAVE_ARCH_PFN_VALID) \
  /root/6.12/common/include/linux/list_nulls.h \
  /root/6.12/common/include/linux/wait.h \
  /root/6.12/common/include/linux/seqlock.h \
  /root/6.12/common/include/linux/nodemask.h \
  /root/6.12/common/include/linux/pageblock-flags.h \
    $(wildcard include/config/HUGETLB_PAGE) \
    $(wildcard include/config/HUGETLB_PAGE_SIZE_VARIABLE) \
  /root/6.12/common/include/linux/page-flags-layout.h \
  include/generated/bounds.h \
  /root/6.12/common/arch/arm64/include/asm/sparsemem.h \
  /root/6.12/common/arch/arm64/include/asm/pgtable-prot.h \
    $(wildcard include/config/HAVE_ARCH_USERFAULTFD_WP) \
  /root/6.12/common/include/linux/mm_types.h \
    $(wildcard include/config/HAVE_ALIGNED_STRUCT_PAGE) \
    $(wildcard include/config/HUGETLB_PMD_PAGE_TABLE_SHARING) \
    $(wildcard include/config/SLAB_FREELIST_HARDENED) \
    $(wildcard include/config/USERFAULTFD) \
    $(wildcard include/config/ANON_VMA_NAME) \
    $(wildcard include/config/PER_VMA_LOCK) \
    $(wildcard include/config/HAVE_ARCH_COMPAT_MMAP_BASES) \
    $(wildcard include/config/MEMBARRIER) \
    $(wildcard include/config/AIO) \
    $(wildcard include/config/MMU_NOTIFIER) \
    $(wildcard include/config/SPLIT_PMD_PTLOCKS) \
    $(wildcard include/config/IOMMU_MM_DATA) \
    $(wildcard include/config/KSM) \
  /root/6.12/common/include/linux/auxvec.h \
  /root/6.12/common/include/uapi/linux/auxvec.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/auxvec.h \
  /root/6.12/common/include/linux/kref.h \
  /root/6.12/common/include/linux/rbtree.h \
  /root/6.12/common/include/linux/rcupdate.h \
    $(wildcard include/config/TINY_RCU) \
    $(wildcard include/config/RCU_STRICT_GRACE_PERIOD) \
    $(wildcard include/config/RCU_LAZY) \
    $(wildcard include/config/TASKS_RCU_GENERIC) \
    $(wildcard include/config/RCU_STALL_COMMON) \
    $(wildcard include/config/KVM_XFER_TO_GUEST_WORK) \
    $(wildcard include/config/RCU_NOCB_CPU) \
    $(wildcard include/config/TASKS_RUDE_RCU) \
    $(wildcard include/config/TREE_RCU) \
    $(wildcard include/config/DEBUG_OBJECTS_RCU_HEAD) \
    $(wildcard include/config/PROVE_RCU) \
    $(wildcard include/config/ARCH_WEAK_RELEASE_ACQUIRE) \
  /root/6.12/common/include/linux/context_tracking_irq.h \
    $(wildcard include/config/CONTEXT_TRACKING_IDLE) \
  /root/6.12/common/include/linux/rcutree.h \
  /root/6.12/common/include/linux/maple_tree.h \
    $(wildcard include/config/MAPLE_RCU_DISABLED) \
    $(wildcard include/config/DEBUG_MAPLE_TREE) \
  /root/6.12/common/include/linux/rwsem.h \
    $(wildcard include/config/RWSEM_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_RWSEMS) \
  /root/6.12/common/include/linux/completion.h \
  /root/6.12/common/include/linux/swait.h \
  /root/6.12/common/include/linux/uprobes.h \
  /root/6.12/common/arch/arm64/include/asm/uprobes.h \
  /root/6.12/common/arch/arm64/include/asm/debug-monitors.h \
  /root/6.12/common/arch/arm64/include/asm/esr.h \
  /root/6.12/common/arch/arm64/include/asm/probes.h \
  /root/6.12/common/include/linux/workqueue.h \
    $(wildcard include/config/DEBUG_OBJECTS_WORK) \
    $(wildcard include/config/FREEZER) \
    $(wildcard include/config/WQ_WATCHDOG) \
  /root/6.12/common/include/linux/timer.h \
    $(wildcard include/config/DEBUG_OBJECTS_TIMERS) \
  /root/6.12/common/include/linux/ktime.h \
  /root/6.12/common/include/linux/jiffies.h \
  /root/6.12/common/include/vdso/jiffies.h \
  include/generated/timeconst.h \
  /root/6.12/common/include/vdso/ktime.h \
  /root/6.12/common/include/linux/timekeeping.h \
    $(wildcard include/config/GENERIC_CMOS_UPDATE) \
  /root/6.12/common/include/linux/clocksource_ids.h \
  /root/6.12/common/include/linux/debugobjects.h \
    $(wildcard include/config/DEBUG_OBJECTS) \
    $(wildcard include/config/DEBUG_OBJECTS_FREE) \
  /root/6.12/common/include/linux/percpu_counter.h \
  /root/6.12/common/arch/arm64/include/asm/mmu.h \
    $(wildcard include/config/ARM64_E0PD) \
    $(wildcard include/config/CAVIUM_ERRATUM_27456) \
  /root/6.12/common/include/linux/page-flags.h \
    $(wildcard include/config/PAGE_IDLE_FLAG) \
    $(wildcard include/config/ARCH_USES_PG_ARCH_2) \
    $(wildcard include/config/ARCH_USES_PG_ARCH_3) \
    $(wildcard include/config/HUGETLB_PAGE_OPTIMIZE_VMEMMAP) \
    $(wildcard include/config/DEBUG_KMAP_LOCAL_FORCE_MAP) \
  /root/6.12/common/include/linux/local_lock.h \
  /root/6.12/common/include/linux/local_lock_internal.h \
  /root/6.12/common/include/linux/zswap.h \
    $(wildcard include/config/ZSWAP) \
  /root/6.12/common/include/linux/memory_hotplug.h \
    $(wildcard include/config/ARCH_HAS_ADD_PAGES) \
    $(wildcard include/config/MEMORY_HOTREMOVE) \
  /root/6.12/common/include/linux/notifier.h \
  /root/6.12/common/include/linux/srcu.h \
    $(wildcard include/config/TINY_SRCU) \
    $(wildcard include/config/NEED_SRCU_NMI_SAFE) \
  /root/6.12/common/include/linux/rcu_segcblist.h \
  /root/6.12/common/include/linux/srcutree.h \
  /root/6.12/common/include/linux/rcu_node_tree.h \
    $(wildcard include/config/RCU_FANOUT) \
    $(wildcard include/config/RCU_FANOUT_LEAF) \
  /root/6.12/common/include/linux/topology.h \
    $(wildcard include/config/USE_PERCPU_NUMA_NODE_ID) \
    $(wildcard include/config/SCHED_SMT) \
  /root/6.12/common/include/linux/arch_topology.h \
    $(wildcard include/config/GENERIC_ARCH_TOPOLOGY) \
  /root/6.12/common/arch/arm64/include/asm/topology.h \
  /root/6.12/common/include/asm-generic/topology.h \
  /root/6.12/common/include/linux/sysctl.h \
    $(wildcard include/config/SYSCTL) \
  /root/6.12/common/include/uapi/linux/sysctl.h \
  /root/6.12/common/include/linux/elf.h \
    $(wildcard include/config/ARCH_HAVE_EXTRA_ELF_NOTES) \
    $(wildcard include/config/ARCH_USE_GNU_PROPERTY) \
    $(wildcard include/config/ARCH_HAVE_ELF_PROT) \
  /root/6.12/common/arch/arm64/include/asm/elf.h \
    $(wildcard include/config/COMPAT_VDSO) \
  arch/arm64/include/generated/asm/user.h \
  /root/6.12/common/include/asm-generic/user.h \
  /root/6.12/common/include/uapi/linux/elf.h \
  /root/6.12/common/include/uapi/linux/elf-em.h \
  /root/6.12/common/include/linux/fs.h \
    $(wildcard include/config/READ_ONLY_THP_FOR_FS) \
    $(wildcard include/config/FS_POSIX_ACL) \
    $(wildcard include/config/CGROUP_WRITEBACK) \
    $(wildcard include/config/IMA) \
    $(wildcard include/config/FILE_LOCKING) \
    $(wildcard include/config/FSNOTIFY) \
    $(wildcard include/config/FS_ENCRYPTION) \
    $(wildcard include/config/FS_VERITY) \
    $(wildcard include/config/EPOLL) \
    $(wildcard include/config/UNICODE) \
    $(wildcard include/config/QUOTA) \
    $(wildcard include/config/FS_DAX) \
    $(wildcard include/config/BLOCK) \
  /root/6.12/common/include/linux/wait_bit.h \
  /root/6.12/common/include/linux/kdev_t.h \
  /root/6.12/common/include/uapi/linux/kdev_t.h \
  /root/6.12/common/include/linux/dcache.h \
  /root/6.12/common/include/linux/rculist.h \
    $(wildcard include/config/PROVE_RCU_LIST) \
  /root/6.12/common/include/linux/rculist_bl.h \
  /root/6.12/common/include/linux/list_bl.h \
  /root/6.12/common/include/linux/bit_spinlock.h \
  /root/6.12/common/include/linux/lockref.h \
    $(wildcard include/config/ARCH_USE_CMPXCHG_LOCKREF) \
  /root/6.12/common/include/linux/stringhash.h \
    $(wildcard include/config/DCACHE_WORD_ACCESS) \
  /root/6.12/common/include/linux/hash.h \
    $(wildcard include/config/HAVE_ARCH_HASH) \
  /root/6.12/common/include/linux/path.h \
  /root/6.12/common/include/linux/list_lru.h \
  /root/6.12/common/include/linux/shrinker.h \
    $(wildcard include/config/SHRINKER_DEBUG) \
  /root/6.12/common/include/linux/xarray.h \
    $(wildcard include/config/XARRAY_MULTI) \
  /root/6.12/common/include/linux/sched/mm.h \
    $(wildcard include/config/MMU_LAZY_TLB_REFCOUNT) \
    $(wildcard include/config/ARCH_HAS_MEMBARRIER_CALLBACKS) \
  /root/6.12/common/include/linux/sync_core.h \
    $(wildcard include/config/ARCH_HAS_SYNC_CORE_BEFORE_USERMODE) \
    $(wildcard include/config/ARCH_HAS_PREPARE_SYNC_CORE_CMD) \
  /root/6.12/common/include/linux/sched/coredump.h \
    $(wildcard include/config/CORE_DUMP_DEFAULT_ELF_HEADERS) \
  /root/6.12/common/include/linux/radix-tree.h \
  /root/6.12/common/include/linux/pid.h \
  /root/6.12/common/include/linux/capability.h \
  /root/6.12/common/include/uapi/linux/capability.h \
  /root/6.12/common/include/linux/semaphore.h \
  /root/6.12/common/include/linux/fcntl.h \
    $(wildcard include/config/ARCH_32BIT_OFF_T) \
  /root/6.12/common/include/uapi/linux/fcntl.h \
  /root/6.12/common/arch/arm64/include/uapi/asm/fcntl.h \
  /root/6.12/common/include/uapi/asm-generic/fcntl.h \
  /root/6.12/common/include/uapi/linux/openat2.h \
  /root/6.12/common/include/linux/migrate_mode.h \
  /root/6.12/common/include/linux/percpu-rwsem.h \
  /root/6.12/common/include/linux/rcuwait.h \
  /root/6.12/common/include/linux/sched/signal.h \
    $(wildcard include/config/SCHED_AUTOGROUP) \
    $(wildcard include/config/BSD_PROCESS_ACCT) \
    $(wildcard include/config/TASKSTATS) \
  /root/6.12/common/include/linux/signal.h \
    $(wildcard include/config/DYNAMIC_SIGFRAME) \
    $(wildcard include/config/PROC_FS) \
  /root/6.12/common/include/linux/sched/jobctl.h \
  /root/6.12/common/include/linux/sched/task.h \
    $(wildcard include/config/GKI_DYNAMIC_TASK_STRUCT_SIZE) \
    $(wildcard include/config/GKI_TASK_STRUCT_VENDOR_SIZE_MAX) \
    $(wildcard include/config/HAVE_EXIT_THREAD) \
    $(wildcard include/config/ARCH_WANTS_DYNAMIC_TASK_STRUCT) \
    $(wildcard include/config/HAVE_ARCH_THREAD_STRUCT_WHITELIST) \
  /root/6.12/common/include/linux/uaccess.h \
    $(wildcard include/config/ARCH_HAS_SUBPAGE_FAULTS) \
  /root/6.12/common/include/linux/fault-inject-usercopy.h \
    $(wildcard include/config/FAULT_INJECTION_USERCOPY) \
  /root/6.12/common/include/linux/nospec.h \
  /root/6.12/common/arch/arm64/include/asm/uaccess.h \
    $(wildcard include/config/CC_HAS_ASM_GOTO_OUTPUT) \
  /root/6.12/common/arch/arm64/include/asm/kernel-pgtable.h \
    $(wildcard include/config/RELOCATABLE) \
  /root/6.12/common/arch/arm64/include/asm/asm-extable.h \
  /root/6.12/common/arch/arm64/include/asm/mte.h \
  /root/6.12/common/arch/arm64/include/asm/extable.h \
    $(wildcard include/config/BPF_JIT) \
  /root/6.12/common/include/asm-generic/access_ok.h \
    $(wildcard include/config/ALTERNATE_USER_ADDRESS_SPACE) \
  /root/6.12/common/include/linux/cred.h \
  /root/6.12/common/include/linux/key.h \
    $(wildcard include/config/KEY_NOTIFICATIONS) \
    $(wildcard include/config/NET) \
  /root/6.12/common/include/linux/assoc_array.h \
    $(wildcard include/config/ASSOCIATIVE_ARRAY) \
  /root/6.12/common/include/linux/sched/user.h \
    $(wildcard include/config/VFIO_PCI_ZDEV_KVM) \
    $(wildcard include/config/IOMMUFD) \
    $(wildcard include/config/WATCH_QUEUE) \
  /root/6.12/common/include/linux/ratelimit.h \
  /root/6.12/common/include/linux/posix-timers.h \
  /root/6.12/common/include/linux/alarmtimer.h \
    $(wildcard include/config/RTC_CLASS) \
  /root/6.12/common/include/linux/hrtimer.h \
    $(wildcard include/config/HIGH_RES_TIMERS) \
    $(wildcard include/config/TIME_LOW_RES) \
    $(wildcard include/config/TIMERFD) \
  /root/6.12/common/include/linux/hrtimer_defs.h \
  /root/6.12/common/include/linux/timerqueue.h \
  /root/6.12/common/include/linux/rcu_sync.h \
  /root/6.12/common/include/linux/delayed_call.h \
  /root/6.12/common/include/linux/uuid.h \
  /root/6.12/common/include/linux/errseq.h \
  /root/6.12/common/include/linux/ioprio.h \
  /root/6.12/common/include/linux/sched/rt.h \
  /root/6.12/common/include/linux/iocontext.h \
    $(wildcard include/config/BLK_ICQ) \
  /root/6.12/common/include/uapi/linux/ioprio.h \
  /root/6.12/common/include/linux/fs_types.h \
  /root/6.12/common/include/linux/mount.h \
  /root/6.12/common/include/linux/mnt_idmapping.h \
  /root/6.12/common/include/linux/slab.h \
    $(wildcard include/config/FAILSLAB) \
    $(wildcard include/config/KFENCE) \
    $(wildcard include/config/SLUB_TINY) \
    $(wildcard include/config/SLUB_DEBUG) \
    $(wildcard include/config/SLAB_BUCKETS) \
  /root/6.12/common/include/linux/percpu-refcount.h \
  /root/6.12/common/include/linux/rw_hint.h \
  /root/6.12/common/include/uapi/linux/fs.h \
  /root/6.12/common/include/linux/quota.h \
    $(wildcard include/config/QUOTA_NETLINK_INTERFACE) \
  /root/6.12/common/include/uapi/linux/dqblk_xfs.h \
  /root/6.12/common/include/linux/dqblk_v1.h \
  /root/6.12/common/include/linux/dqblk_v2.h \
  /root/6.12/common/include/linux/dqblk_qtree.h \
  /root/6.12/common/include/linux/projid.h \
  /root/6.12/common/include/uapi/linux/quota.h \
  /root/6.12/common/include/linux/kobject.h \
    $(wildcard include/config/UEVENT_HELPER) \
    $(wildcard include/config/DEBUG_KOBJECT_RELEASE) \
  /root/6.12/common/include/linux/sysfs.h \
  /root/6.12/common/include/linux/kernfs.h \
    $(wildcard include/config/KERNFS) \
  /root/6.12/common/include/linux/idr.h \
  /root/6.12/common/include/linux/kobject_ns.h \
  /root/6.12/common/include/linux/moduleparam.h \
    $(wildcard include/config/ALPHA) \
    $(wildcard include/config/PPC64) \
  /root/6.12/common/include/linux/rbtree_latch.h \
  /root/6.12/common/include/linux/error-injection.h \
  /root/6.12/common/include/asm-generic/error-injection.h \
  /root/6.12/common/include/linux/tracepoint-defs.h \
  /root/6.12/common/include/linux/dynamic_debug.h \
  /root/6.12/common/arch/arm64/include/asm/module.h \
    $(wildcard include/config/DYNAMIC_FTRACE) \
  /root/6.12/common/include/asm-generic/module.h \
    $(wildcard include/config/HAVE_MOD_ARCH_SPECIFIC) \
    $(wildcard include/config/MODULES_USE_ELF_REL) \
    $(wildcard include/config/MODULES_USE_ELF_RELA) \
  /root/6.12/common/include/linux/build-salt.h \
    $(wildcard include/config/BUILD_SALT) \
  /root/6.12/common/include/linux/elfnote.h \
  /root/6.12/common/include/linux/elfnote-lto.h \
  /root/6.12/common/include/linux/vermagic.h \
    $(wildcard include/config/PREEMPT_BUILD) \
  include/generated/utsrelease.h \
  /root/6.12/common/arch/arm64/include/asm/vermagic.h \

/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/.module-common.o: $(deps_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/.module-common.o)

$(deps_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/.module-common.o):
