cmd_/mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.o := clang -Wp,-MMD,/mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/.lsdriver.o.d -nostdinc -I/root/6.1/common/arch/arm64/include -I./arch/arm64/include/generated -I/root/6.1/common/include -I./include -I/root/6.1/common/arch/arm64/include/uapi -I./arch/arm64/include/generated/uapi -I/root/6.1/common/include/uapi -I./include/generated/uapi -include /root/6.1/common/include/linux/compiler-version.h -include /root/6.1/common/include/linux/kconfig.h -include /root/6.1/common/include/linux/compiler_types.h -D__KERNEL__ --target=aarch64-linux-android -fintegrated-as -Werror=unknown-warning-option -Werror=ignored-optimization-argument -mlittle-endian -DKASAN_SHADOW_SCALE_SHIFT= -Qunused-arguments -fmacro-prefix-map=/root/6.1/common/= -Wall -Wundef -Werror=strict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Wno-format-security -std=gnu11 -mgeneral-regs-only -DCONFIG_CC_HAS_K_CONSTRAINT=1 -Wno-psabi -fasynchronous-unwind-tables -mbranch-protection=pac-ret -Wa,-march=armv8.5-a -DARM64_ASM_ARCH='"armv8.5-a"' -ffixed-x18 -DKASAN_SHADOW_SCALE_SHIFT= -fno-delete-null-pointer-checks -Wno-frame-address -Wno-address-of-packed-member -O2 -Wframe-larger-than=2048 -fstack-protector-strong -Wno-gnu -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -ftrivial-auto-var-init=zero -fno-stack-clash-protection -fsanitize=kcfi -Wvla -Wno-pointer-sign -Wcast-function-type -Werror -Wimplicit-fallthrough -fno-strict-overflow -fno-stack-check -Werror=date-time -Werror=incompatible-pointer-types -fno-builtin-wcslen -Wno-initializer-overrides -Wno-sign-compare -Wno-pointer-to-enum-cast -Wno-tautological-constant-out-of-range-compare -Wno-unaligned-access -Wno-cast-function-type-strict -Wno-enum-compare-conditional -g -gdwarf-4 -mstack-protector-guard=sysreg -mstack-protector-guard-reg=sp_el0 -mstack-protector-guard-offset=1592 -O3 -fno-stack-protector -fomit-frame-pointer -funroll-loops -finline-functions -fstrict-aliasing -ffunction-sections -fdata-sections  -fsanitize=array-bounds -fsanitize=local-bounds -fsanitize-undefined-trap-on-error  -DMODULE  -DKBUILD_BASENAME='"lsdriver"' -DKBUILD_MODNAME='"lsdriver"' -D__KBUILD_MODNAME=kmod_lsdriver -c -o /mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.o /mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.c  

source_/mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.o := /mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.c

deps_/mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.o := \
  /root/6.1/common/include/linux/compiler-version.h \
    $(wildcard include/config/CC_VERSION_TEXT) \
  /root/6.1/common/include/linux/kconfig.h \
    $(wildcard include/config/CPU_BIG_ENDIAN) \
    $(wildcard include/config/BOOGER) \
    $(wildcard include/config/FOO) \
  /root/6.1/common/include/linux/compiler_types.h \
    $(wildcard include/config/DEBUG_INFO_BTF) \
    $(wildcard include/config/PAHOLE_HAS_BTF_TAG) \
    $(wildcard include/config/HAVE_ARCH_COMPILER_H) \
    $(wildcard include/config/CC_HAS_ASM_INLINE) \
  /root/6.1/common/include/linux/compiler_attributes.h \
  /root/6.1/common/include/linux/compiler-clang.h \
    $(wildcard include/config/ARCH_USE_BUILTIN_BSWAP) \
    $(wildcard include/config/CLANG_VERSION) \
  /root/6.1/common/arch/arm64/include/asm/compiler.h \
  /root/6.1/common/include/linux/module.h \
    $(wildcard include/config/MODULES) \
    $(wildcard include/config/SYSFS) \
    $(wildcard include/config/MODULES_TREE_LOOKUP) \
    $(wildcard include/config/LIVEPATCH) \
    $(wildcard include/config/STACKTRACE_BUILD_ID) \
    $(wildcard include/config/ARCH_USES_CFI_TRAPS) \
    $(wildcard include/config/ARCH_WANTS_MODULES_DATA_IN_VMALLOC) \
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
    $(wildcard include/config/RETPOLINE) \
    $(wildcard include/config/MODULE_SIG) \
  /root/6.1/common/include/linux/list.h \
    $(wildcard include/config/DEBUG_LIST) \
  /root/6.1/common/include/linux/container_of.h \
  /root/6.1/common/include/linux/build_bug.h \
  /root/6.1/common/include/linux/compiler.h \
    $(wildcard include/config/TRACE_BRANCH_PROFILING) \
    $(wildcard include/config/PROFILE_ALL_BRANCHES) \
    $(wildcard include/config/OBJTOOL) \
    $(wildcard include/config/64BIT) \
  /root/6.1/common/arch/arm64/include/asm/rwonce.h \
    $(wildcard include/config/LTO) \
    $(wildcard include/config/AS_HAS_LDAPR) \
  /root/6.1/common/include/asm-generic/rwonce.h \
  /root/6.1/common/include/linux/kasan-checks.h \
    $(wildcard include/config/KASAN_GENERIC) \
    $(wildcard include/config/KASAN_SW_TAGS) \
  /root/6.1/common/include/linux/types.h \
    $(wildcard include/config/HAVE_UID16) \
    $(wildcard include/config/UID16) \
    $(wildcard include/config/ARCH_DMA_ADDR_T_64BIT) \
    $(wildcard include/config/PHYS_ADDR_T_64BIT) \
    $(wildcard include/config/ARCH_32BIT_USTAT_F_TINODE) \
  /root/6.1/common/include/uapi/linux/types.h \
  arch/arm64/include/generated/uapi/asm/types.h \
  /root/6.1/common/include/uapi/asm-generic/types.h \
  /root/6.1/common/include/asm-generic/int-ll64.h \
  /root/6.1/common/include/uapi/asm-generic/int-ll64.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/bitsperlong.h \
  /root/6.1/common/include/asm-generic/bitsperlong.h \
  /root/6.1/common/include/uapi/asm-generic/bitsperlong.h \
  /root/6.1/common/include/uapi/linux/posix_types.h \
  /root/6.1/common/include/linux/stddef.h \
  /root/6.1/common/include/uapi/linux/stddef.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/posix_types.h \
  /root/6.1/common/include/uapi/asm-generic/posix_types.h \
  /root/6.1/common/include/linux/kcsan-checks.h \
    $(wildcard include/config/KCSAN) \
    $(wildcard include/config/KCSAN_WEAK_MEMORY) \
    $(wildcard include/config/KCSAN_IGNORE_ATOMICS) \
  /root/6.1/common/include/linux/err.h \
  arch/arm64/include/generated/uapi/asm/errno.h \
  /root/6.1/common/include/uapi/asm-generic/errno.h \
  /root/6.1/common/include/uapi/asm-generic/errno-base.h \
  /root/6.1/common/include/linux/poison.h \
    $(wildcard include/config/ILLEGAL_POINTER_VALUE) \
  /root/6.1/common/include/linux/const.h \
  /root/6.1/common/include/vdso/const.h \
  /root/6.1/common/include/uapi/linux/const.h \
  /root/6.1/common/arch/arm64/include/asm/barrier.h \
    $(wildcard include/config/ARM64_PSEUDO_NMI) \
  /root/6.1/common/include/asm-generic/barrier.h \
  /root/6.1/common/include/linux/stat.h \
  /root/6.1/common/arch/arm64/include/asm/stat.h \
    $(wildcard include/config/COMPAT) \
  arch/arm64/include/generated/uapi/asm/stat.h \
  /root/6.1/common/include/uapi/asm-generic/stat.h \
  /root/6.1/common/include/linux/time.h \
    $(wildcard include/config/POSIX_TIMERS) \
  /root/6.1/common/include/linux/cache.h \
    $(wildcard include/config/ARCH_HAS_CACHE_LINE_SIZE) \
  /root/6.1/common/include/uapi/linux/kernel.h \
  /root/6.1/common/include/uapi/linux/sysinfo.h \
  /root/6.1/common/arch/arm64/include/asm/cache.h \
    $(wildcard include/config/KASAN_HW_TAGS) \
  /root/6.1/common/include/linux/bitops.h \
  /root/6.1/common/include/linux/bits.h \
  /root/6.1/common/include/vdso/bits.h \
  /root/6.1/common/include/linux/typecheck.h \
  /root/6.1/common/include/asm-generic/bitops/generic-non-atomic.h \
  /root/6.1/common/arch/arm64/include/asm/bitops.h \
  /root/6.1/common/include/asm-generic/bitops/builtin-__ffs.h \
  /root/6.1/common/include/asm-generic/bitops/builtin-ffs.h \
  /root/6.1/common/include/asm-generic/bitops/builtin-__fls.h \
  /root/6.1/common/include/asm-generic/bitops/builtin-fls.h \
  /root/6.1/common/include/asm-generic/bitops/ffz.h \
  /root/6.1/common/include/asm-generic/bitops/fls64.h \
  /root/6.1/common/include/asm-generic/bitops/sched.h \
  /root/6.1/common/include/asm-generic/bitops/hweight.h \
  /root/6.1/common/include/asm-generic/bitops/arch_hweight.h \
  /root/6.1/common/include/asm-generic/bitops/const_hweight.h \
  /root/6.1/common/include/asm-generic/bitops/atomic.h \
  /root/6.1/common/include/linux/atomic.h \
  /root/6.1/common/arch/arm64/include/asm/atomic.h \
  /root/6.1/common/arch/arm64/include/asm/cmpxchg.h \
  /root/6.1/common/arch/arm64/include/asm/lse.h \
    $(wildcard include/config/ARM64_LSE_ATOMICS) \
  /root/6.1/common/arch/arm64/include/asm/atomic_ll_sc.h \
    $(wildcard include/config/CC_HAS_K_CONSTRAINT) \
  /root/6.1/common/include/linux/stringify.h \
  /root/6.1/common/include/linux/export.h \
    $(wildcard include/config/MODVERSIONS) \
    $(wildcard include/config/HAVE_ARCH_PREL32_RELOCATIONS) \
    $(wildcard include/config/TRIM_UNUSED_KSYMS) \
  /root/6.1/common/arch/arm64/include/asm/alternative.h \
  /root/6.1/common/arch/arm64/include/asm/alternative-macros.h \
  arch/arm64/include/generated/asm/cpucaps.h \
  /root/6.1/common/arch/arm64/include/asm/insn-def.h \
  /root/6.1/common/arch/arm64/include/asm/brk-imm.h \
  /root/6.1/common/include/linux/init.h \
    $(wildcard include/config/STRICT_KERNEL_RWX) \
    $(wildcard include/config/STRICT_MODULE_RWX) \
    $(wildcard include/config/LTO_CLANG) \
  /root/6.1/common/arch/arm64/include/asm/atomic_lse.h \
  /root/6.1/common/include/linux/atomic/atomic-arch-fallback.h \
    $(wildcard include/config/GENERIC_ATOMIC64) \
  /root/6.1/common/include/linux/atomic/atomic-long.h \
  /root/6.1/common/include/linux/atomic/atomic-instrumented.h \
  /root/6.1/common/include/linux/instrumented.h \
  /root/6.1/common/include/linux/kmsan-checks.h \
    $(wildcard include/config/KMSAN) \
  /root/6.1/common/include/asm-generic/bitops/instrumented-atomic.h \
  /root/6.1/common/include/asm-generic/bitops/lock.h \
  /root/6.1/common/include/asm-generic/bitops/instrumented-lock.h \
  /root/6.1/common/include/asm-generic/bitops/non-atomic.h \
  /root/6.1/common/include/asm-generic/bitops/non-instrumented-non-atomic.h \
  /root/6.1/common/include/asm-generic/bitops/le.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/byteorder.h \
  /root/6.1/common/include/linux/byteorder/little_endian.h \
  /root/6.1/common/include/uapi/linux/byteorder/little_endian.h \
  /root/6.1/common/include/linux/swab.h \
  /root/6.1/common/include/uapi/linux/swab.h \
  arch/arm64/include/generated/uapi/asm/swab.h \
  /root/6.1/common/include/uapi/asm-generic/swab.h \
  /root/6.1/common/include/linux/byteorder/generic.h \
  /root/6.1/common/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /root/6.1/common/include/linux/kasan-enabled.h \
    $(wildcard include/config/KASAN) \
  /root/6.1/common/include/linux/static_key.h \
  /root/6.1/common/include/linux/jump_label.h \
    $(wildcard include/config/HAVE_ARCH_JUMP_LABEL_RELATIVE) \
  /root/6.1/common/arch/arm64/include/asm/jump_label.h \
  /root/6.1/common/arch/arm64/include/asm/insn.h \
  /root/6.1/common/arch/arm64/include/asm/cputype.h \
  /root/6.1/common/arch/arm64/include/asm/sysreg.h \
    $(wildcard include/config/BROKEN_GAS_INST) \
    $(wildcard include/config/ARM64_PA_BITS_52) \
    $(wildcard include/config/ARM64_4K_PAGES) \
    $(wildcard include/config/ARM64_16K_PAGES) \
    $(wildcard include/config/ARM64_64K_PAGES) \
  /root/6.1/common/include/linux/kasan-tags.h \
  /root/6.1/common/arch/arm64/include/asm/gpr-num.h \
  arch/arm64/include/generated/asm/sysreg-defs.h \
  /root/6.1/common/include/linux/bitfield.h \
  /root/6.1/common/arch/arm64/include/asm/mte-def.h \
  /root/6.1/common/include/linux/math64.h \
    $(wildcard include/config/ARCH_SUPPORTS_INT128) \
  /root/6.1/common/include/linux/math.h \
  arch/arm64/include/generated/asm/div64.h \
  /root/6.1/common/include/asm-generic/div64.h \
  /root/6.1/common/include/vdso/math64.h \
  /root/6.1/common/include/linux/time64.h \
  /root/6.1/common/include/vdso/time64.h \
  /root/6.1/common/include/uapi/linux/time.h \
  /root/6.1/common/include/uapi/linux/time_types.h \
  /root/6.1/common/include/linux/time32.h \
  /root/6.1/common/include/linux/timex.h \
  /root/6.1/common/include/uapi/linux/timex.h \
  /root/6.1/common/include/uapi/linux/param.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/param.h \
  /root/6.1/common/include/asm-generic/param.h \
    $(wildcard include/config/HZ) \
  /root/6.1/common/include/uapi/asm-generic/param.h \
  /root/6.1/common/arch/arm64/include/asm/timex.h \
  /root/6.1/common/arch/arm64/include/asm/arch_timer.h \
    $(wildcard include/config/ARM_ARCH_TIMER_OOL_WORKAROUND) \
  /root/6.1/common/arch/arm64/include/asm/hwcap.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/hwcap.h \
  /root/6.1/common/arch/arm64/include/asm/cpufeature.h \
    $(wildcard include/config/ARM64_PAN) \
    $(wildcard include/config/ARM64_SW_TTBR0_PAN) \
    $(wildcard include/config/ARM64_SVE) \
    $(wildcard include/config/ARM64_SME) \
    $(wildcard include/config/ARM64_CNP) \
    $(wildcard include/config/ARM64_PTR_AUTH) \
    $(wildcard include/config/ARM64_MTE) \
    $(wildcard include/config/ARM64_DEBUG_PRIORITY_MASKING) \
    $(wildcard include/config/ARM64_BTI) \
    $(wildcard include/config/ARM64_TLB_RANGE) \
    $(wildcard include/config/ARM64_PA_BITS) \
    $(wildcard include/config/ARM64_HW_AFDBM) \
    $(wildcard include/config/ARM64_AMU_EXTN) \
  /root/6.1/common/include/linux/bug.h \
    $(wildcard include/config/BUG_ON_DATA_CORRUPTION) \
  /root/6.1/common/arch/arm64/include/asm/bug.h \
  /root/6.1/common/arch/arm64/include/asm/asm-bug.h \
    $(wildcard include/config/DEBUG_BUGVERBOSE) \
  /root/6.1/common/include/asm-generic/bug.h \
    $(wildcard include/config/BUG) \
    $(wildcard include/config/GENERIC_BUG_RELATIVE_POINTERS) \
  /root/6.1/common/include/linux/instrumentation.h \
    $(wildcard include/config/NOINSTR_VALIDATION) \
  /root/6.1/common/include/linux/once_lite.h \
  /root/6.1/common/include/linux/panic.h \
    $(wildcard include/config/PANIC_TIMEOUT) \
  /root/6.1/common/include/linux/printk.h \
    $(wildcard include/config/MESSAGE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_DEFAULT) \
    $(wildcard include/config/CONSOLE_LOGLEVEL_QUIET) \
    $(wildcard include/config/EARLY_PRINTK) \
    $(wildcard include/config/PRINTK) \
    $(wildcard include/config/DYNAMIC_DEBUG) \
    $(wildcard include/config/DYNAMIC_DEBUG_CORE) \
  /root/6.1/common/include/linux/stdarg.h \
  /root/6.1/common/include/linux/kern_levels.h \
  /root/6.1/common/include/linux/linkage.h \
    $(wildcard include/config/ARCH_USE_SYM_ANNOTATIONS) \
  /root/6.1/common/arch/arm64/include/asm/linkage.h \
  /root/6.1/common/include/linux/ratelimit_types.h \
  /root/6.1/common/include/linux/spinlock_types_raw.h \
    $(wildcard include/config/DEBUG_SPINLOCK) \
    $(wildcard include/config/DEBUG_LOCK_ALLOC) \
  /root/6.1/common/arch/arm64/include/asm/spinlock_types.h \
  /root/6.1/common/include/asm-generic/qspinlock_types.h \
    $(wildcard include/config/NR_CPUS) \
  /root/6.1/common/include/asm-generic/qrwlock_types.h \
  /root/6.1/common/include/linux/lockdep_types.h \
    $(wildcard include/config/PROVE_RAW_LOCK_NESTING) \
    $(wildcard include/config/LOCKDEP) \
    $(wildcard include/config/LOCK_STAT) \
  /root/6.1/common/include/linux/kernel.h \
    $(wildcard include/config/PREEMPT_VOLUNTARY_BUILD) \
    $(wildcard include/config/PREEMPT_DYNAMIC) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_CALL) \
    $(wildcard include/config/HAVE_PREEMPT_DYNAMIC_KEY) \
    $(wildcard include/config/PREEMPT_) \
    $(wildcard include/config/DEBUG_ATOMIC_SLEEP) \
    $(wildcard include/config/MMU) \
    $(wildcard include/config/PROVE_LOCKING) \
  /root/6.1/common/include/linux/align.h \
  /root/6.1/common/include/linux/limits.h \
  /root/6.1/common/include/uapi/linux/limits.h \
  /root/6.1/common/include/vdso/limits.h \
  /root/6.1/common/include/linux/kstrtox.h \
  /root/6.1/common/include/linux/log2.h \
    $(wildcard include/config/ARCH_HAS_ILOG2_U32) \
    $(wildcard include/config/ARCH_HAS_ILOG2_U64) \
  /root/6.1/common/include/linux/minmax.h \
  /root/6.1/common/include/linux/static_call_types.h \
    $(wildcard include/config/HAVE_STATIC_CALL) \
  /root/6.1/common/include/linux/instruction_pointer.h \
  /root/6.1/common/include/linux/smp.h \
    $(wildcard include/config/UP_LATE_INIT) \
    $(wildcard include/config/DEBUG_PREEMPT) \
  /root/6.1/common/include/linux/errno.h \
  /root/6.1/common/include/uapi/linux/errno.h \
  /root/6.1/common/include/linux/cpumask.h \
    $(wildcard include/config/FORCE_NR_CPUS) \
    $(wildcard include/config/HOTPLUG_CPU) \
    $(wildcard include/config/DEBUG_PER_CPU_MAPS) \
    $(wildcard include/config/CPUMASK_OFFSTACK) \
  /root/6.1/common/include/linux/threads.h \
    $(wildcard include/config/BASE_SMALL) \
  /root/6.1/common/include/linux/bitmap.h \
  /root/6.1/common/include/linux/find.h \
  /root/6.1/common/include/linux/string.h \
    $(wildcard include/config/BINARY_PRINTF) \
    $(wildcard include/config/FORTIFY_SOURCE) \
  /root/6.1/common/include/linux/overflow.h \
  /root/6.1/common/include/uapi/linux/string.h \
  /root/6.1/common/arch/arm64/include/asm/string.h \
    $(wildcard include/config/ARCH_HAS_UACCESS_FLUSHCACHE) \
  /root/6.1/common/include/linux/fortify-string.h \
  /root/6.1/common/include/linux/gfp_types.h \
    $(wildcard include/config/CMA) \
  /root/6.1/common/include/linux/numa.h \
    $(wildcard include/config/NODES_SHIFT) \
    $(wildcard include/config/NUMA_KEEP_MEMINFO) \
    $(wildcard include/config/NUMA) \
    $(wildcard include/config/HAVE_ARCH_NODE_DEV_GROUP) \
  /root/6.1/common/include/linux/smp_types.h \
  /root/6.1/common/include/linux/llist.h \
    $(wildcard include/config/ARCH_HAVE_NMI_SAFE_CMPXCHG) \
  /root/6.1/common/include/linux/preempt.h \
    $(wildcard include/config/PREEMPT_RT) \
    $(wildcard include/config/PREEMPT_COUNT) \
    $(wildcard include/config/TRACE_PREEMPT_TOGGLE) \
    $(wildcard include/config/PREEMPTION) \
    $(wildcard include/config/PREEMPT_NOTIFIERS) \
  /root/6.1/common/include/linux/cleanup.h \
  /root/6.1/common/arch/arm64/include/asm/preempt.h \
  /root/6.1/common/include/linux/thread_info.h \
    $(wildcard include/config/THREAD_INFO_IN_TASK) \
    $(wildcard include/config/GENERIC_ENTRY) \
    $(wildcard include/config/HAVE_ARCH_WITHIN_STACK_FRAMES) \
    $(wildcard include/config/HARDENED_USERCOPY) \
  /root/6.1/common/include/linux/restart_block.h \
  /root/6.1/common/arch/arm64/include/asm/current.h \
  /root/6.1/common/arch/arm64/include/asm/thread_info.h \
    $(wildcard include/config/SHADOW_CALL_STACK) \
  /root/6.1/common/arch/arm64/include/asm/memory.h \
    $(wildcard include/config/ARM64_VA_BITS) \
    $(wildcard include/config/KASAN_SHADOW_OFFSET) \
    $(wildcard include/config/VMAP_STACK) \
    $(wildcard include/config/DEBUG_VIRTUAL) \
    $(wildcard include/config/ZONE_DMA) \
    $(wildcard include/config/ZONE_DMA32) \
    $(wildcard include/config/EFI) \
    $(wildcard include/config/ARM_GIC_V3_ITS) \
  /root/6.1/common/include/linux/sizes.h \
  /root/6.1/common/arch/arm64/include/asm/page-def.h \
    $(wildcard include/config/ARM64_PAGE_SHIFT) \
  /root/6.1/common/include/linux/mmdebug.h \
    $(wildcard include/config/DEBUG_VM) \
    $(wildcard include/config/DEBUG_VM_IRQSOFF) \
    $(wildcard include/config/DEBUG_VM_PGFLAGS) \
  /root/6.1/common/include/asm-generic/memory_model.h \
    $(wildcard include/config/FLATMEM) \
    $(wildcard include/config/SPARSEMEM_VMEMMAP) \
    $(wildcard include/config/SPARSEMEM) \
  /root/6.1/common/include/linux/pfn.h \
  /root/6.1/common/arch/arm64/include/asm/stack_pointer.h \
  /root/6.1/common/arch/arm64/include/asm/smp.h \
    $(wildcard include/config/ARM64_ACPI_PARKING_PROTOCOL) \
  /root/6.1/common/arch/arm64/include/asm/percpu.h \
  /root/6.1/common/include/asm-generic/percpu.h \
    $(wildcard include/config/HAVE_SETUP_PER_CPU_AREA) \
  /root/6.1/common/include/linux/percpu-defs.h \
    $(wildcard include/config/DEBUG_FORCE_WEAK_PER_CPU) \
    $(wildcard include/config/AMD_MEM_ENCRYPT) \
  /root/6.1/common/include/clocksource/arm_arch_timer.h \
    $(wildcard include/config/ARM_ARCH_TIMER) \
  /root/6.1/common/include/linux/timecounter.h \
  /root/6.1/common/include/asm-generic/timex.h \
  /root/6.1/common/include/vdso/time32.h \
  /root/6.1/common/include/vdso/time.h \
  /root/6.1/common/arch/arm64/include/asm/compat.h \
  /root/6.1/common/include/asm-generic/compat.h \
    $(wildcard include/config/COMPAT_FOR_U64_ALIGNMENT) \
  /root/6.1/common/include/linux/sched.h \
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
    $(wildcard include/config/CGROUP_SCHED) \
    $(wildcard include/config/BLK_DEV_IO_TRACE) \
    $(wildcard include/config/PREEMPT_RCU) \
    $(wildcard include/config/TASKS_RCU) \
    $(wildcard include/config/TASKS_TRACE_RCU) \
    $(wildcard include/config/PSI) \
    $(wildcard include/config/MEMCG) \
    $(wildcard include/config/LRU_GEN) \
    $(wildcard include/config/COMPAT_BRK) \
    $(wildcard include/config/CGROUPS) \
    $(wildcard include/config/BLK_CGROUP) \
    $(wildcard include/config/PAGE_OWNER) \
    $(wildcard include/config/EVENTFD) \
    $(wildcard include/config/IOMMU_SVA) \
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
    $(wildcard include/config/DEBUG_MUTEXES) \
    $(wildcard include/config/TRACE_IRQFLAGS) \
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
    $(wildcard include/config/FAULT_INJECTION) \
    $(wildcard include/config/LATENCYTOP) \
    $(wildcard include/config/FUNCTION_GRAPH_TRACER) \
    $(wildcard include/config/KCOV) \
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
    $(wildcard include/config/ARCH_TASK_STRUCT_ON_STACK) \
    $(wildcard include/config/PREEMPT_NONE) \
    $(wildcard include/config/PREEMPT_VOLUNTARY) \
    $(wildcard include/config/PREEMPT) \
    $(wildcard include/config/DEBUG_RSEQ) \
  /root/6.1/common/include/uapi/linux/sched.h \
  /root/6.1/common/include/linux/pid.h \
  /root/6.1/common/include/linux/rculist.h \
    $(wildcard include/config/PROVE_RCU_LIST) \
  /root/6.1/common/include/linux/rcupdate.h \
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
  /root/6.1/common/include/linux/irqflags.h \
    $(wildcard include/config/IRQSOFF_TRACER) \
    $(wildcard include/config/PREEMPT_TRACER) \
    $(wildcard include/config/DEBUG_IRQFLAGS) \
    $(wildcard include/config/TRACE_IRQFLAGS_SUPPORT) \
  /root/6.1/common/arch/arm64/include/asm/irqflags.h \
  /root/6.1/common/arch/arm64/include/asm/ptrace.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/ptrace.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/sve_context.h \
  /root/6.1/common/include/linux/bottom_half.h \
  /root/6.1/common/include/linux/lockdep.h \
    $(wildcard include/config/DEBUG_LOCKING_API_SELFTESTS) \
  /root/6.1/common/arch/arm64/include/asm/processor.h \
    $(wildcard include/config/KUSER_HELPERS) \
    $(wildcard include/config/ARM64_FORCE_52BIT) \
    $(wildcard include/config/HAVE_HW_BREAKPOINT) \
    $(wildcard include/config/ARM64_PTR_AUTH_KERNEL) \
    $(wildcard include/config/ARM64_TAGGED_ADDR_ABI) \
  /root/6.1/common/include/linux/android_vendor.h \
    $(wildcard include/config/ANDROID_VENDOR_OEM_DATA) \
  /root/6.1/common/include/linux/android_kabi.h \
    $(wildcard include/config/ANDROID_KABI_RESERVE) \
  /root/6.1/common/include/vdso/processor.h \
  /root/6.1/common/arch/arm64/include/asm/vdso/processor.h \
  /root/6.1/common/arch/arm64/include/asm/hw_breakpoint.h \
  /root/6.1/common/arch/arm64/include/asm/virt.h \
    $(wildcard include/config/KVM) \
  /root/6.1/common/arch/arm64/include/asm/sections.h \
  /root/6.1/common/include/asm-generic/sections.h \
    $(wildcard include/config/HAVE_FUNCTION_DESCRIPTORS) \
  /root/6.1/common/arch/arm64/include/asm/kasan.h \
  /root/6.1/common/arch/arm64/include/asm/mte-kasan.h \
  /root/6.1/common/arch/arm64/include/asm/pgtable-types.h \
    $(wildcard include/config/PGTABLE_LEVELS) \
  /root/6.1/common/include/asm-generic/pgtable-nopud.h \
  /root/6.1/common/include/asm-generic/pgtable-nop4d.h \
  /root/6.1/common/arch/arm64/include/asm/pgtable-hwdef.h \
    $(wildcard include/config/ARM64_CONT_PTE_SHIFT) \
    $(wildcard include/config/ARM64_CONT_PMD_SHIFT) \
    $(wildcard include/config/ARM64_VA_BITS_52) \
  /root/6.1/common/arch/arm64/include/asm/pointer_auth.h \
  /root/6.1/common/include/uapi/linux/prctl.h \
  /root/6.1/common/include/linux/random.h \
    $(wildcard include/config/VMGENID) \
  /root/6.1/common/include/linux/once.h \
  /root/6.1/common/include/uapi/linux/random.h \
  /root/6.1/common/include/uapi/linux/ioctl.h \
  arch/arm64/include/generated/uapi/asm/ioctl.h \
  /root/6.1/common/include/asm-generic/ioctl.h \
  /root/6.1/common/include/uapi/asm-generic/ioctl.h \
  /root/6.1/common/include/linux/irqnr.h \
  /root/6.1/common/include/uapi/linux/irqnr.h \
  /root/6.1/common/include/linux/prandom.h \
  /root/6.1/common/include/linux/percpu.h \
    $(wildcard include/config/NEED_PER_CPU_EMBED_FIRST_CHUNK) \
    $(wildcard include/config/NEED_PER_CPU_PAGE_FIRST_CHUNK) \
  /root/6.1/common/arch/arm64/include/asm/archrandom.h \
  /root/6.1/common/include/linux/arm-smccc.h \
    $(wildcard include/config/ARM64) \
    $(wildcard include/config/HAVE_ARM_SMCCC) \
    $(wildcard include/config/ARM) \
  /root/6.1/common/arch/arm64/include/asm/spectre.h \
  /root/6.1/common/arch/arm64/include/asm/fpsimd.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/sigcontext.h \
  /root/6.1/common/include/linux/context_tracking_irq.h \
    $(wildcard include/config/CONTEXT_TRACKING_IDLE) \
  /root/6.1/common/include/linux/rcutree.h \
  /root/6.1/common/include/linux/wait.h \
  /root/6.1/common/include/linux/spinlock.h \
  arch/arm64/include/generated/asm/mmiowb.h \
  /root/6.1/common/include/asm-generic/mmiowb.h \
    $(wildcard include/config/MMIOWB) \
  /root/6.1/common/include/linux/spinlock_types.h \
  /root/6.1/common/include/linux/rwlock_types.h \
  /root/6.1/common/arch/arm64/include/asm/spinlock.h \
  arch/arm64/include/generated/asm/qspinlock.h \
  /root/6.1/common/include/asm-generic/qspinlock.h \
  arch/arm64/include/generated/asm/qrwlock.h \
  /root/6.1/common/include/asm-generic/qrwlock.h \
  /root/6.1/common/include/linux/rwlock.h \
  /root/6.1/common/include/linux/spinlock_api_smp.h \
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
  /root/6.1/common/include/linux/rwlock_api_smp.h \
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
  /root/6.1/common/include/uapi/linux/wait.h \
  /root/6.1/common/include/linux/refcount.h \
  /root/6.1/common/include/linux/sem.h \
  /root/6.1/common/include/uapi/linux/sem.h \
  /root/6.1/common/include/linux/ipc.h \
  /root/6.1/common/include/linux/uidgid.h \
    $(wildcard include/config/MULTIUSER) \
    $(wildcard include/config/USER_NS) \
  /root/6.1/common/include/linux/highuid.h \
  /root/6.1/common/include/linux/rhashtable-types.h \
  /root/6.1/common/include/linux/mutex.h \
    $(wildcard include/config/MUTEX_SPIN_ON_OWNER) \
  /root/6.1/common/include/linux/osq_lock.h \
  /root/6.1/common/include/linux/debug_locks.h \
  /root/6.1/common/include/linux/workqueue.h \
    $(wildcard include/config/DEBUG_OBJECTS_WORK) \
    $(wildcard include/config/FREEZER) \
    $(wildcard include/config/WQ_WATCHDOG) \
  /root/6.1/common/include/linux/timer.h \
    $(wildcard include/config/DEBUG_OBJECTS_TIMERS) \
  /root/6.1/common/include/linux/ktime.h \
  /root/6.1/common/include/linux/jiffies.h \
  /root/6.1/common/include/vdso/jiffies.h \
  include/generated/timeconst.h \
  /root/6.1/common/include/vdso/ktime.h \
  /root/6.1/common/include/linux/timekeeping.h \
    $(wildcard include/config/GENERIC_CMOS_UPDATE) \
  /root/6.1/common/include/linux/clocksource_ids.h \
  /root/6.1/common/include/linux/debugobjects.h \
    $(wildcard include/config/DEBUG_OBJECTS) \
    $(wildcard include/config/DEBUG_OBJECTS_FREE) \
  /root/6.1/common/include/uapi/linux/ipc.h \
  arch/arm64/include/generated/uapi/asm/ipcbuf.h \
  /root/6.1/common/include/uapi/asm-generic/ipcbuf.h \
  arch/arm64/include/generated/uapi/asm/sembuf.h \
  /root/6.1/common/include/uapi/asm-generic/sembuf.h \
  /root/6.1/common/include/linux/shm.h \
  /root/6.1/common/arch/arm64/include/asm/page.h \
  /root/6.1/common/include/linux/personality.h \
  /root/6.1/common/include/uapi/linux/personality.h \
  /root/6.1/common/include/asm-generic/getorder.h \
  /root/6.1/common/include/uapi/linux/shm.h \
  /root/6.1/common/include/uapi/asm-generic/hugetlb_encode.h \
  arch/arm64/include/generated/uapi/asm/shmbuf.h \
  /root/6.1/common/include/uapi/asm-generic/shmbuf.h \
  /root/6.1/common/arch/arm64/include/asm/shmparam.h \
  /root/6.1/common/include/asm-generic/shmparam.h \
  /root/6.1/common/include/linux/kmsan_types.h \
  /root/6.1/common/include/linux/plist.h \
    $(wildcard include/config/DEBUG_PLIST) \
  /root/6.1/common/include/linux/hrtimer.h \
    $(wildcard include/config/HIGH_RES_TIMERS) \
    $(wildcard include/config/TIME_LOW_RES) \
    $(wildcard include/config/TIMERFD) \
  /root/6.1/common/include/linux/hrtimer_defs.h \
  /root/6.1/common/include/linux/rbtree.h \
  /root/6.1/common/include/linux/rbtree_types.h \
  /root/6.1/common/include/linux/seqlock.h \
  /root/6.1/common/include/linux/timerqueue.h \
  /root/6.1/common/include/linux/seccomp.h \
    $(wildcard include/config/SECCOMP) \
    $(wildcard include/config/HAVE_ARCH_SECCOMP_FILTER) \
    $(wildcard include/config/SECCOMP_FILTER) \
    $(wildcard include/config/CHECKPOINT_RESTORE) \
    $(wildcard include/config/SECCOMP_CACHE_DEBUG) \
  /root/6.1/common/include/uapi/linux/seccomp.h \
  /root/6.1/common/arch/arm64/include/asm/seccomp.h \
  /root/6.1/common/arch/arm64/include/asm/unistd.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/unistd.h \
  /root/6.1/common/include/uapi/asm-generic/unistd.h \
  /root/6.1/common/include/asm-generic/seccomp.h \
  /root/6.1/common/include/uapi/linux/unistd.h \
  /root/6.1/common/include/linux/nodemask.h \
    $(wildcard include/config/HIGHMEM) \
  /root/6.1/common/include/linux/resource.h \
  /root/6.1/common/include/uapi/linux/resource.h \
  arch/arm64/include/generated/uapi/asm/resource.h \
  /root/6.1/common/include/asm-generic/resource.h \
  /root/6.1/common/include/uapi/asm-generic/resource.h \
  /root/6.1/common/include/linux/latencytop.h \
  /root/6.1/common/include/linux/sched/prio.h \
  /root/6.1/common/include/linux/sched/types.h \
  /root/6.1/common/include/linux/signal_types.h \
    $(wildcard include/config/OLD_SIGACTION) \
  /root/6.1/common/include/uapi/linux/signal.h \
  /root/6.1/common/arch/arm64/include/asm/signal.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/signal.h \
  /root/6.1/common/include/asm-generic/signal.h \
  /root/6.1/common/include/uapi/asm-generic/signal.h \
  /root/6.1/common/include/uapi/asm-generic/signal-defs.h \
  arch/arm64/include/generated/uapi/asm/siginfo.h \
  /root/6.1/common/include/uapi/asm-generic/siginfo.h \
  /root/6.1/common/include/linux/syscall_user_dispatch.h \
  /root/6.1/common/include/linux/mm_types_task.h \
    $(wildcard include/config/ARCH_WANT_BATCHED_UNMAP_TLB_FLUSH) \
    $(wildcard include/config/SPLIT_PTLOCK_CPUS) \
    $(wildcard include/config/ARCH_ENABLE_SPLIT_PMD_PTLOCK) \
  /root/6.1/common/include/linux/task_io_accounting.h \
    $(wildcard include/config/TASK_IO_ACCOUNTING) \
  /root/6.1/common/include/linux/posix-timers.h \
  /root/6.1/common/include/linux/alarmtimer.h \
    $(wildcard include/config/RTC_CLASS) \
  /root/6.1/common/include/uapi/linux/rseq.h \
  /root/6.1/common/include/linux/kcsan.h \
  /root/6.1/common/include/linux/rv.h \
    $(wildcard include/config/RV_REACTORS) \
  arch/arm64/include/generated/asm/kmap_size.h \
  /root/6.1/common/include/asm-generic/kmap_size.h \
    $(wildcard include/config/DEBUG_KMAP_LOCAL) \
  /root/6.1/common/include/linux/sched/task_stack.h \
    $(wildcard include/config/STACK_GROWSUP) \
    $(wildcard include/config/DEBUG_STACK_USAGE) \
  /root/6.1/common/include/uapi/linux/magic.h \
  /root/6.1/common/include/linux/kasan.h \
    $(wildcard include/config/KASAN_STACK) \
    $(wildcard include/config/KASAN_VMALLOC) \
  /root/6.1/common/include/uapi/linux/stat.h \
  /root/6.1/common/include/linux/buildid.h \
    $(wildcard include/config/CRASH_CORE) \
  /root/6.1/common/include/linux/mm_types.h \
    $(wildcard include/config/HAVE_ALIGNED_STRUCT_PAGE) \
    $(wildcard include/config/USERFAULTFD) \
    $(wildcard include/config/PER_VMA_LOCK) \
    $(wildcard include/config/SWAP) \
    $(wildcard include/config/HAVE_ARCH_COMPAT_MMAP_BASES) \
    $(wildcard include/config/MEMBARRIER) \
    $(wildcard include/config/AIO) \
    $(wildcard include/config/MMU_NOTIFIER) \
    $(wildcard include/config/TRANSPARENT_HUGEPAGE) \
    $(wildcard include/config/HUGETLB_PAGE) \
    $(wildcard include/config/KSM) \
  /root/6.1/common/include/linux/auxvec.h \
  /root/6.1/common/include/uapi/linux/auxvec.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/auxvec.h \
  /root/6.1/common/include/linux/kref.h \
  /root/6.1/common/include/linux/maple_tree.h \
    $(wildcard include/config/MAPLE_RCU_DISABLED) \
    $(wildcard include/config/DEBUG_MAPLE_TREE_VERBOSE) \
    $(wildcard include/config/DEBUG_MAPLE_TREE) \
  /root/6.1/common/include/linux/rwsem.h \
    $(wildcard include/config/RWSEM_SPIN_ON_OWNER) \
    $(wildcard include/config/DEBUG_RWSEMS) \
  /root/6.1/common/include/linux/completion.h \
  /root/6.1/common/include/linux/swait.h \
  /root/6.1/common/include/linux/uprobes.h \
  /root/6.1/common/arch/arm64/include/asm/uprobes.h \
  /root/6.1/common/arch/arm64/include/asm/debug-monitors.h \
  /root/6.1/common/arch/arm64/include/asm/esr.h \
  /root/6.1/common/arch/arm64/include/asm/probes.h \
  /root/6.1/common/include/linux/page-flags-layout.h \
  include/generated/bounds.h \
  /root/6.1/common/arch/arm64/include/asm/sparsemem.h \
  /root/6.1/common/arch/arm64/include/asm/mmu.h \
  /root/6.1/common/include/linux/kmod.h \
  /root/6.1/common/include/linux/umh.h \
  /root/6.1/common/include/linux/gfp.h \
    $(wildcard include/config/ZONE_DEVICE) \
    $(wildcard include/config/PM_SLEEP) \
    $(wildcard include/config/CONTIG_ALLOC) \
  /root/6.1/common/include/linux/mmzone.h \
    $(wildcard include/config/ARCH_FORCE_MAX_ORDER) \
    $(wildcard include/config/PAGE_BLOCK_ORDER) \
    $(wildcard include/config/MEMORY_ISOLATION) \
    $(wildcard include/config/LRU_GEN_STATS) \
    $(wildcard include/config/MEMORY_HOTPLUG) \
    $(wildcard include/config/PAGE_EXTENSION) \
    $(wildcard include/config/DEFERRED_STRUCT_PAGE_INIT) \
    $(wildcard include/config/HAVE_MEMORYLESS_NODES) \
    $(wildcard include/config/SPARSEMEM_EXTREME) \
    $(wildcard include/config/HAVE_ARCH_PFN_VALID) \
  /root/6.1/common/include/linux/list_nulls.h \
  /root/6.1/common/include/linux/pageblock-flags.h \
    $(wildcard include/config/HUGETLB_PAGE_SIZE_VARIABLE) \
  /root/6.1/common/include/linux/page-flags.h \
    $(wildcard include/config/ARCH_USES_PG_UNCACHED) \
    $(wildcard include/config/MEMORY_FAILURE) \
    $(wildcard include/config/PAGE_IDLE_FLAG) \
    $(wildcard include/config/HUGETLB_PAGE_OPTIMIZE_VMEMMAP) \
  /root/6.1/common/include/linux/local_lock.h \
  /root/6.1/common/include/linux/local_lock_internal.h \
  /root/6.1/common/include/linux/memory_hotplug.h \
    $(wildcard include/config/HAVE_ARCH_NODEDATA_EXTENSION) \
    $(wildcard include/config/ARCH_HAS_ADD_PAGES) \
    $(wildcard include/config/MEMORY_HOTREMOVE) \
  /root/6.1/common/include/linux/notifier.h \
  /root/6.1/common/include/linux/srcu.h \
    $(wildcard include/config/TINY_SRCU) \
    $(wildcard include/config/SRCU) \
  /root/6.1/common/include/linux/rcu_segcblist.h \
  /root/6.1/common/include/linux/srcutree.h \
  /root/6.1/common/include/linux/rcu_node_tree.h \
    $(wildcard include/config/RCU_FANOUT) \
    $(wildcard include/config/RCU_FANOUT_LEAF) \
  /root/6.1/common/include/linux/topology.h \
    $(wildcard include/config/USE_PERCPU_NUMA_NODE_ID) \
    $(wildcard include/config/SCHED_SMT) \
  /root/6.1/common/include/linux/arch_topology.h \
    $(wildcard include/config/ACPI_CPPC_LIB) \
    $(wildcard include/config/GENERIC_ARCH_TOPOLOGY) \
  /root/6.1/common/arch/arm64/include/asm/topology.h \
  /root/6.1/common/include/asm-generic/topology.h \
  /root/6.1/common/include/linux/sysctl.h \
    $(wildcard include/config/SYSCTL) \
  /root/6.1/common/include/uapi/linux/sysctl.h \
  /root/6.1/common/include/linux/elf.h \
    $(wildcard include/config/ARCH_USE_GNU_PROPERTY) \
    $(wildcard include/config/ARCH_HAVE_ELF_PROT) \
  /root/6.1/common/arch/arm64/include/asm/elf.h \
    $(wildcard include/config/COMPAT_VDSO) \
  arch/arm64/include/generated/asm/user.h \
  /root/6.1/common/include/asm-generic/user.h \
  /root/6.1/common/include/uapi/linux/elf.h \
  /root/6.1/common/include/uapi/linux/elf-em.h \
  /root/6.1/common/include/linux/fs.h \
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
  /root/6.1/common/include/linux/wait_bit.h \
  /root/6.1/common/include/linux/kdev_t.h \
  /root/6.1/common/include/uapi/linux/kdev_t.h \
  /root/6.1/common/include/linux/dcache.h \
  /root/6.1/common/include/linux/rculist_bl.h \
  /root/6.1/common/include/linux/list_bl.h \
  /root/6.1/common/include/linux/bit_spinlock.h \
  /root/6.1/common/include/linux/lockref.h \
    $(wildcard include/config/ARCH_USE_CMPXCHG_LOCKREF) \
  /root/6.1/common/include/linux/stringhash.h \
    $(wildcard include/config/DCACHE_WORD_ACCESS) \
  /root/6.1/common/include/linux/hash.h \
    $(wildcard include/config/HAVE_ARCH_HASH) \
  /root/6.1/common/include/linux/path.h \
  /root/6.1/common/include/linux/list_lru.h \
    $(wildcard include/config/MEMCG_KMEM) \
  /root/6.1/common/include/linux/shrinker.h \
    $(wildcard include/config/SHRINKER_DEBUG) \
  /root/6.1/common/include/linux/xarray.h \
    $(wildcard include/config/XARRAY_MULTI) \
  /root/6.1/common/include/linux/sched/mm.h \
    $(wildcard include/config/ARCH_HAS_MEMBARRIER_CALLBACKS) \
  /root/6.1/common/include/linux/sync_core.h \
    $(wildcard include/config/ARCH_HAS_SYNC_CORE_BEFORE_USERMODE) \
  /root/6.1/common/include/linux/ioasid.h \
    $(wildcard include/config/IOASID) \
  /root/6.1/common/include/linux/radix-tree.h \
  /root/6.1/common/include/linux/capability.h \
  /root/6.1/common/include/uapi/linux/capability.h \
  /root/6.1/common/include/linux/semaphore.h \
  /root/6.1/common/include/linux/fcntl.h \
    $(wildcard include/config/ARCH_32BIT_OFF_T) \
  /root/6.1/common/include/uapi/linux/fcntl.h \
  /root/6.1/common/arch/arm64/include/uapi/asm/fcntl.h \
  /root/6.1/common/include/uapi/asm-generic/fcntl.h \
  /root/6.1/common/include/uapi/linux/openat2.h \
  /root/6.1/common/include/linux/migrate_mode.h \
  /root/6.1/common/include/linux/percpu-rwsem.h \
  /root/6.1/common/include/linux/rcuwait.h \
  /root/6.1/common/include/linux/sched/signal.h \
    $(wildcard include/config/SCHED_AUTOGROUP) \
    $(wildcard include/config/BSD_PROCESS_ACCT) \
    $(wildcard include/config/TASKSTATS) \
  /root/6.1/common/include/linux/signal.h \
    $(wildcard include/config/DYNAMIC_SIGFRAME) \
    $(wildcard include/config/PROC_FS) \
  /root/6.1/common/include/linux/sched/jobctl.h \
  /root/6.1/common/include/linux/sched/task.h \
    $(wildcard include/config/HAVE_EXIT_THREAD) \
    $(wildcard include/config/ARCH_WANTS_DYNAMIC_TASK_STRUCT) \
    $(wildcard include/config/HAVE_ARCH_THREAD_STRUCT_WHITELIST) \
  /root/6.1/common/include/linux/uaccess.h \
    $(wildcard include/config/ARCH_HAS_SUBPAGE_FAULTS) \
  /root/6.1/common/include/linux/fault-inject-usercopy.h \
    $(wildcard include/config/FAULT_INJECTION_USERCOPY) \
  /root/6.1/common/arch/arm64/include/asm/uaccess.h \
  /root/6.1/common/arch/arm64/include/asm/kernel-pgtable.h \
    $(wildcard include/config/RANDOMIZE_BASE) \
  /root/6.1/common/arch/arm64/include/asm/boot.h \
  /root/6.1/common/arch/arm64/include/asm/asm-extable.h \
  /root/6.1/common/arch/arm64/include/asm/mte.h \
  /root/6.1/common/arch/arm64/include/asm/extable.h \
    $(wildcard include/config/BPF_JIT) \
  /root/6.1/common/include/asm-generic/access_ok.h \
    $(wildcard include/config/ALTERNATE_USER_ADDRESS_SPACE) \
  /root/6.1/common/include/linux/cred.h \
    $(wildcard include/config/DEBUG_CREDENTIALS) \
  /root/6.1/common/include/linux/key.h \
    $(wildcard include/config/KEY_NOTIFICATIONS) \
    $(wildcard include/config/NET) \
  /root/6.1/common/include/linux/assoc_array.h \
    $(wildcard include/config/ASSOCIATIVE_ARRAY) \
  /root/6.1/common/include/linux/sched/user.h \
    $(wildcard include/config/VFIO_PCI_ZDEV_KVM) \
    $(wildcard include/config/WATCH_QUEUE) \
  /root/6.1/common/include/linux/percpu_counter.h \
  /root/6.1/common/include/linux/ratelimit.h \
  /root/6.1/common/include/linux/rcu_sync.h \
  /root/6.1/common/include/linux/delayed_call.h \
  /root/6.1/common/include/linux/uuid.h \
  /root/6.1/common/include/uapi/linux/uuid.h \
  /root/6.1/common/include/linux/errseq.h \
  /root/6.1/common/include/linux/ioprio.h \
  /root/6.1/common/include/linux/sched/rt.h \
  /root/6.1/common/include/linux/iocontext.h \
    $(wildcard include/config/BLK_ICQ) \
  /root/6.1/common/include/uapi/linux/ioprio.h \
  /root/6.1/common/include/linux/fs_types.h \
  /root/6.1/common/include/linux/mount.h \
  /root/6.1/common/include/linux/mnt_idmapping.h \
  /root/6.1/common/include/linux/slab.h \
    $(wildcard include/config/DEBUG_SLAB) \
    $(wildcard include/config/FAILSLAB) \
    $(wildcard include/config/KFENCE) \
    $(wildcard include/config/SLAB) \
    $(wildcard include/config/SLUB) \
    $(wildcard include/config/SLOB) \
  /root/6.1/common/include/linux/percpu-refcount.h \
  /root/6.1/common/include/uapi/linux/fs.h \
  /root/6.1/common/include/linux/quota.h \
    $(wildcard include/config/QUOTA_NETLINK_INTERFACE) \
  /root/6.1/common/include/uapi/linux/dqblk_xfs.h \
  /root/6.1/common/include/linux/dqblk_v1.h \
  /root/6.1/common/include/linux/dqblk_v2.h \
  /root/6.1/common/include/linux/dqblk_qtree.h \
  /root/6.1/common/include/linux/projid.h \
  /root/6.1/common/include/uapi/linux/quota.h \
  /root/6.1/common/include/linux/nfs_fs_i.h \
  /root/6.1/common/include/linux/kobject.h \
    $(wildcard include/config/UEVENT_HELPER) \
    $(wildcard include/config/DEBUG_KOBJECT_RELEASE) \
  /root/6.1/common/include/linux/sysfs.h \
  /root/6.1/common/include/linux/kernfs.h \
    $(wildcard include/config/KERNFS) \
  /root/6.1/common/include/linux/idr.h \
  /root/6.1/common/include/linux/kobject_ns.h \
  /root/6.1/common/include/linux/moduleparam.h \
    $(wildcard include/config/ALPHA) \
    $(wildcard include/config/IA64) \
    $(wildcard include/config/PPC64) \
  /root/6.1/common/include/linux/rbtree_latch.h \
  /root/6.1/common/include/linux/error-injection.h \
  /root/6.1/common/include/asm-generic/error-injection.h \
  /root/6.1/common/include/linux/tracepoint-defs.h \
  /root/6.1/common/arch/arm64/include/asm/module.h \
    $(wildcard include/config/ARM64_MODULE_PLTS) \
    $(wildcard include/config/DYNAMIC_FTRACE) \
    $(wildcard include/config/HAVE_MOD_ARCH_SPECIFIC) \
    $(wildcard include/config/ARM64_ERRATUM_843419) \
  /root/6.1/common/include/asm-generic/module.h \
    $(wildcard include/config/MODULES_USE_ELF_REL) \
    $(wildcard include/config/MODULES_USE_ELF_RELA) \
  /root/6.1/common/include/linux/kthread.h \
  /root/6.1/common/include/linux/delay.h \
  arch/arm64/include/generated/asm/delay.h \
  /root/6.1/common/include/asm-generic/delay.h \
  /root/6.1/common/include/linux/mm.h \
    $(wildcard include/config/HAVE_ARCH_MMAP_RND_BITS) \
    $(wildcard include/config/HAVE_ARCH_MMAP_RND_COMPAT_BITS) \
    $(wildcard include/config/MEM_SOFT_DIRTY) \
    $(wildcard include/config/ARCH_USES_HIGH_VMA_FLAGS) \
    $(wildcard include/config/ARCH_HAS_PKEYS) \
    $(wildcard include/config/PPC) \
    $(wildcard include/config/X86) \
    $(wildcard include/config/PARISC) \
    $(wildcard include/config/SPARC64) \
    $(wildcard include/config/HAVE_ARCH_USERFAULTFD_MINOR) \
    $(wildcard include/config/SHMEM) \
    $(wildcard include/config/MIGRATION) \
    $(wildcard include/config/ARCH_HAS_PTE_SPECIAL) \
    $(wildcard include/config/ARCH_HAS_PTE_DEVMAP) \
    $(wildcard include/config/DEBUG_VM_RB) \
    $(wildcard include/config/HAVE_FAST_GUP) \
    $(wildcard include/config/PAGE_POISONING) \
    $(wildcard include/config/INIT_ON_ALLOC_DEFAULT_ON) \
    $(wildcard include/config/INIT_ON_FREE_DEFAULT_ON) \
    $(wildcard include/config/DEBUG_PAGEALLOC) \
    $(wildcard include/config/HUGETLBFS) \
    $(wildcard include/config/MAPPING_DIRTY_HELPERS) \
    $(wildcard include/config/ANON_VMA_NAME) \
  /root/6.1/common/include/linux/mmap_lock.h \
  /root/6.1/common/include/linux/range.h \
  /root/6.1/common/include/linux/page_ext.h \
    $(wildcard include/config/PAGE_PINNER) \
  /root/6.1/common/include/linux/stacktrace.h \
    $(wildcard include/config/ARCH_STACKWALK) \
    $(wildcard include/config/STACKTRACE) \
    $(wildcard include/config/HAVE_RELIABLE_STACKTRACE) \
  /root/6.1/common/include/linux/stackdepot.h \
    $(wildcard include/config/STACKDEPOT) \
  /root/6.1/common/include/linux/page_ref.h \
    $(wildcard include/config/DEBUG_PAGE_REF) \
  /root/6.1/common/include/linux/pgtable.h \
    $(wildcard include/config/HIGHPTE) \
    $(wildcard include/config/ARCH_HAS_NONLEAF_PMD_YOUNG) \
    $(wildcard include/config/GUP_GET_PTE_LOW_HIGH) \
    $(wildcard include/config/HAVE_ARCH_TRANSPARENT_HUGEPAGE_PUD) \
    $(wildcard include/config/HAVE_ARCH_SOFT_DIRTY) \
    $(wildcard include/config/ARCH_ENABLE_THP_MIGRATION) \
    $(wildcard include/config/HAVE_ARCH_HUGE_VMAP) \
    $(wildcard include/config/X86_ESPFIX64) \
  /root/6.1/common/arch/arm64/include/asm/pgtable.h \
    $(wildcard include/config/PAGE_TABLE_CHECK) \
  /root/6.1/common/arch/arm64/include/asm/proc-fns.h \
  /root/6.1/common/arch/arm64/include/asm/pgtable-prot.h \
    $(wildcard include/config/ARM64_BTI_KERNEL) \
  /root/6.1/common/arch/arm64/include/asm/tlbflush.h \
    $(wildcard include/config/ARM64_WORKAROUND_REPEAT_TLBI) \
  /root/6.1/common/arch/arm64/include/asm/fixmap.h \
    $(wildcard include/config/ACPI_APEI_GHES) \
    $(wildcard include/config/ARM_SDE_INTERFACE) \
    $(wildcard include/config/UNMAP_KERNEL_AT_EL0) \
    $(wildcard include/config/RELOCATABLE) \
  /root/6.1/common/include/asm-generic/fixmap.h \
  /root/6.1/common/include/linux/page_table_check.h \
  /root/6.1/common/arch/arm64/include/asm/android_erratum_pgtable.h \
    $(wildcard include/config/ANDROID_ARM64_WORKAROUND_DMA_BEYOND_POC) \
  /root/6.1/common/include/asm-generic/pgtable_uffd.h \
    $(wildcard include/config/HAVE_ARCH_USERFAULTFD_WP) \
  /root/6.1/common/include/linux/pgsize_migration_inline.h \
  /root/6.1/common/include/linux/page_pinner.h \
  /root/6.1/common/include/linux/memremap.h \
    $(wildcard include/config/DEVICE_PRIVATE) \
    $(wildcard include/config/PCI_P2PDMA) \
  /root/6.1/common/include/linux/ioport.h \
  /root/6.1/common/include/linux/huge_mm.h \
  /root/6.1/common/include/linux/sched/coredump.h \
    $(wildcard include/config/CORE_DUMP_DEFAULT_ELF_HEADERS) \
  /root/6.1/common/include/linux/vmstat.h \
    $(wildcard include/config/VM_EVENT_COUNTERS) \
    $(wildcard include/config/DEBUG_TLBFLUSH) \
    $(wildcard include/config/PER_VMA_LOCK_STATS) \
  /root/6.1/common/include/linux/vm_event_item.h \
    $(wildcard include/config/MEMORY_BALLOON) \
    $(wildcard include/config/BALLOON_COMPACTION) \
    $(wildcard include/config/ZSWAP) \
  /root/6.1/common/include/linux/pagemap.h \
  /root/6.1/common/include/linux/highmem.h \
  /root/6.1/common/include/linux/cacheflush.h \
  /root/6.1/common/arch/arm64/include/asm/cacheflush.h \
  /root/6.1/common/include/linux/kgdb.h \
    $(wildcard include/config/HAVE_ARCH_KGDB) \
    $(wildcard include/config/KGDB) \
    $(wildcard include/config/SERIAL_KGDB_NMI) \
    $(wildcard include/config/KGDB_HONOUR_BLOCKLIST) \
  /root/6.1/common/include/linux/kprobes.h \
    $(wildcard include/config/KRETPROBE_ON_RETHOOK) \
    $(wildcard include/config/KPROBES_SANITY_TEST) \
    $(wildcard include/config/OPTPROBES) \
    $(wildcard include/config/KPROBES_ON_FTRACE) \
  /root/6.1/common/include/linux/ftrace.h \
    $(wildcard include/config/FUNCTION_TRACER) \
    $(wildcard include/config/HAVE_DYNAMIC_FTRACE_WITH_ARGS) \
    $(wildcard include/config/DYNAMIC_FTRACE_WITH_REGS) \
    $(wildcard include/config/DYNAMIC_FTRACE_WITH_DIRECT_CALLS) \
    $(wildcard include/config/HAVE_DYNAMIC_FTRACE_WITH_DIRECT_CALLS) \
    $(wildcard include/config/STACK_TRACER) \
    $(wildcard include/config/FRAME_POINTER) \
    $(wildcard include/config/FUNCTION_PROFILER) \
    $(wildcard include/config/FTRACE_SYSCALLS) \
  /root/6.1/common/include/linux/trace_recursion.h \
    $(wildcard include/config/FTRACE_RECORD_RECURSION) \
  /root/6.1/common/include/linux/interrupt.h \
    $(wildcard include/config/IRQ_FORCED_THREADING) \
    $(wildcard include/config/RT_SOFTIRQ_AWARE_SCHED) \
    $(wildcard include/config/GENERIC_IRQ_PROBE) \
    $(wildcard include/config/IRQ_TIMINGS) \
  /root/6.1/common/include/linux/irqreturn.h \
  /root/6.1/common/include/linux/hardirq.h \
  /root/6.1/common/include/linux/context_tracking_state.h \
    $(wildcard include/config/CONTEXT_TRACKING_USER) \
    $(wildcard include/config/CONTEXT_TRACKING) \
  /root/6.1/common/include/linux/ftrace_irq.h \
    $(wildcard include/config/HWLAT_TRACER) \
    $(wildcard include/config/OSNOISE_TRACER) \
  /root/6.1/common/include/linux/vtime.h \
    $(wildcard include/config/VIRT_CPU_ACCOUNTING) \
    $(wildcard include/config/IRQ_TIME_ACCOUNTING) \
  /root/6.1/common/arch/arm64/include/asm/hardirq.h \
  /root/6.1/common/arch/arm64/include/asm/irq.h \
  /root/6.1/common/include/asm-generic/irq.h \
  /root/6.1/common/arch/arm64/include/asm/kvm_arm.h \
  /root/6.1/common/include/asm-generic/hardirq.h \
  /root/6.1/common/include/linux/irq.h \
    $(wildcard include/config/GENERIC_IRQ_EFFECTIVE_AFF_MASK) \
    $(wildcard include/config/GENERIC_IRQ_IPI) \
    $(wildcard include/config/IRQ_DOMAIN_HIERARCHY) \
    $(wildcard include/config/DEPRECATED_IRQ_CPU_ONOFFLINE) \
    $(wildcard include/config/GENERIC_IRQ_MIGRATION) \
    $(wildcard include/config/GENERIC_PENDING_IRQ) \
    $(wildcard include/config/HARDIRQS_SW_RESEND) \
    $(wildcard include/config/GENERIC_IRQ_LEGACY) \
    $(wildcard include/config/GENERIC_IRQ_MULTI_HANDLER) \
  /root/6.1/common/include/linux/irqhandler.h \
  /root/6.1/common/include/linux/io.h \
    $(wildcard include/config/HAS_IOPORT_MAP) \
    $(wildcard include/config/PCI) \
  /root/6.1/common/arch/arm64/include/asm/io.h \
  arch/arm64/include/generated/asm/early_ioremap.h \
  /root/6.1/common/include/asm-generic/early_ioremap.h \
    $(wildcard include/config/GENERIC_EARLY_IOREMAP) \
  /root/6.1/common/include/asm-generic/io.h \
    $(wildcard include/config/GENERIC_IOMAP) \
    $(wildcard include/config/TRACE_MMIO_ACCESS) \
    $(wildcard include/config/GENERIC_IOREMAP) \
  /root/6.1/common/include/asm-generic/pci_iomap.h \
    $(wildcard include/config/NO_GENERIC_PCI_IOPORT_MAP) \
    $(wildcard include/config/GENERIC_PCI_IOMAP) \
  /root/6.1/common/include/linux/logic_pio.h \
    $(wildcard include/config/INDIRECT_PIO) \
  /root/6.1/common/include/linux/fwnode.h \
  /root/6.1/common/include/linux/vmalloc.h \
    $(wildcard include/config/HAVE_ARCH_HUGE_VMALLOC) \
  /root/6.1/common/arch/arm64/include/asm/vmalloc.h \
    $(wildcard include/config/PTDUMP_DEBUGFS) \
  arch/arm64/include/generated/asm/irq_regs.h \
  /root/6.1/common/include/asm-generic/irq_regs.h \
  /root/6.1/common/include/linux/irqdesc.h \
    $(wildcard include/config/GENERIC_IRQ_DEBUGFS) \
    $(wildcard include/config/SPARSE_IRQ) \
    $(wildcard include/config/IRQ_DOMAIN) \
  arch/arm64/include/generated/asm/hw_irq.h \
  /root/6.1/common/include/asm-generic/hw_irq.h \
  /root/6.1/common/include/linux/trace_clock.h \
  arch/arm64/include/generated/asm/trace_clock.h \
  /root/6.1/common/include/asm-generic/trace_clock.h \
  /root/6.1/common/include/linux/kallsyms.h \
    $(wildcard include/config/KALLSYMS_ALL) \
  /root/6.1/common/include/linux/ptrace.h \
  /root/6.1/common/include/linux/pid_namespace.h \
    $(wildcard include/config/PID_NS) \
  /root/6.1/common/include/linux/nsproxy.h \
  /root/6.1/common/include/linux/ns_common.h \
  /root/6.1/common/include/uapi/linux/ptrace.h \
  /root/6.1/common/arch/arm64/include/asm/ftrace.h \
  /root/6.1/common/include/linux/compat.h \
    $(wildcard include/config/ARCH_HAS_SYSCALL_WRAPPER) \
    $(wildcard include/config/X86_X32_ABI) \
    $(wildcard include/config/COMPAT_OLD_SIGACTION) \
    $(wildcard include/config/ODD_RT_SIGACTION) \
  /root/6.1/common/include/linux/socket.h \
  arch/arm64/include/generated/uapi/asm/socket.h \
  /root/6.1/common/include/uapi/asm-generic/socket.h \
  arch/arm64/include/generated/uapi/asm/sockios.h \
  /root/6.1/common/include/uapi/asm-generic/sockios.h \
  /root/6.1/common/include/uapi/linux/sockios.h \
  /root/6.1/common/include/linux/uio.h \
    $(wildcard include/config/ARCH_HAS_COPY_MC) \
  /root/6.1/common/include/uapi/linux/uio.h \
  /root/6.1/common/include/uapi/linux/socket.h \
  /root/6.1/common/include/uapi/linux/if.h \
  /root/6.1/common/include/uapi/linux/libc-compat.h \
  /root/6.1/common/include/uapi/linux/hdlc/ioctl.h \
  /root/6.1/common/include/uapi/linux/aio_abi.h \
  /root/6.1/common/arch/arm64/include/asm/syscall_wrapper.h \
  /root/6.1/common/include/linux/freelist.h \
  /root/6.1/common/include/linux/rethook.h \
  /root/6.1/common/arch/arm64/include/asm/kprobes.h \
  /root/6.1/common/include/asm-generic/kprobes.h \
  /root/6.1/common/arch/arm64/include/asm/kgdb.h \
  /root/6.1/common/include/asm-generic/cacheflush.h \
  /root/6.1/common/include/linux/kmsan.h \
  /root/6.1/common/include/linux/dma-direction.h \
  /root/6.1/common/include/linux/highmem-internal.h \
  /root/6.1/common/include/linux/hugetlb_inline.h \
  /mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/ExportFun.h \
  include/generated/uapi/linux/version.h \
  /mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/virtual_input.h \
  /root/6.1/common/include/linux/input.h \
  /root/6.1/common/include/uapi/linux/input.h \
  /root/6.1/common/include/uapi/linux/input-event-codes.h \
  /root/6.1/common/include/linux/device.h \
    $(wildcard include/config/GENERIC_MSI_IRQ_DOMAIN) \
    $(wildcard include/config/GENERIC_MSI_IRQ) \
    $(wildcard include/config/ENERGY_MODEL) \
    $(wildcard include/config/PINCTRL) \
    $(wildcard include/config/DMA_OPS) \
    $(wildcard include/config/DMA_DECLARE_COHERENT) \
    $(wildcard include/config/DMA_CMA) \
    $(wildcard include/config/SWIOTLB) \
    $(wildcard include/config/ARCH_HAS_SYNC_DMA_FOR_DEVICE) \
    $(wildcard include/config/ARCH_HAS_SYNC_DMA_FOR_CPU) \
    $(wildcard include/config/ARCH_HAS_SYNC_DMA_FOR_CPU_ALL) \
    $(wildcard include/config/DMA_OPS_BYPASS) \
    $(wildcard include/config/OF) \
    $(wildcard include/config/DEVTMPFS) \
    $(wildcard include/config/SYSFS_DEPRECATED) \
  /root/6.1/common/include/linux/dev_printk.h \
  /root/6.1/common/include/linux/energy_model.h \
  /root/6.1/common/include/linux/sched/cpufreq.h \
    $(wildcard include/config/CPU_FREQ) \
  /root/6.1/common/include/linux/sched/topology.h \
    $(wildcard include/config/SCHED_DEBUG) \
    $(wildcard include/config/SCHED_CLUSTER) \
    $(wildcard include/config/SCHED_MC) \
    $(wildcard include/config/CPU_FREQ_GOV_SCHEDUTIL) \
  /root/6.1/common/include/linux/sched/idle.h \
  /root/6.1/common/include/linux/sched/sd_flags.h \
  /root/6.1/common/include/linux/klist.h \
  /root/6.1/common/include/linux/pm.h \
    $(wildcard include/config/VT_CONSOLE_SLEEP) \
    $(wildcard include/config/CXL_SUSPEND) \
    $(wildcard include/config/PM) \
    $(wildcard include/config/PM_CLK) \
    $(wildcard include/config/PM_GENERIC_DOMAINS) \
  /root/6.1/common/include/linux/device/bus.h \
    $(wildcard include/config/ACPI) \
  /root/6.1/common/include/linux/device/class.h \
  /root/6.1/common/include/linux/device/driver.h \
  /root/6.1/common/arch/arm64/include/asm/device.h \
  /root/6.1/common/include/linux/pm_wakeup.h \
  /root/6.1/common/include/linux/mod_devicetable.h \
  /root/6.1/common/include/linux/input/mt.h \
  /mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/physical.h \
  /root/6.1/common/include/linux/sort.h \

/mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.o: $(deps_/mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.o)

$(deps_/mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.o):
