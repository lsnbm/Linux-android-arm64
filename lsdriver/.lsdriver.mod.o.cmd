cmd_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.mod.o := clang -Wp,-MMD,/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/.lsdriver.mod.o.d -nostdinc -isystem /root/android13-5.10/prebuilts/clang/host/linux-x86/clang-r450784e/lib64/clang/14.0.7/include -I/root/android13-5.10/common/arch/arm64/include -I./arch/arm64/include/generated -I/root/android13-5.10/common/include -I./include -I/root/android13-5.10/common/arch/arm64/include/uapi -I./arch/arm64/include/generated/uapi -I/root/android13-5.10/common/include/uapi -I./include/generated/uapi -include /root/android13-5.10/common/include/linux/kconfig.h -include /root/android13-5.10/common/include/linux/compiler_types.h -D__KERNEL__ --target=aarch64-linux-android -integrated-as -Werror=unknown-warning-option -mlittle-endian -DKASAN_SHADOW_SCALE_SHIFT= -Qunused-arguments -fmacro-prefix-map=/root/android13-5.10/common/= -Wall -Wundef -Werror=strict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -fshort-wchar -fno-PIE -Werror=implicit-function-declaration -Werror=implicit-int -Werror=return-type -Wno-format-security -std=gnu89 -mgeneral-regs-only -DCONFIG_CC_HAS_K_CONSTRAINT=1 -Wno-psabi -fno-asynchronous-unwind-tables -fno-unwind-tables -mbranch-protection=pac-ret+leaf+bti -Wa,-march=armv8.5-a -DARM64_ASM_ARCH='"armv8.5-a"' -ffixed-x18 -DKASAN_SHADOW_SCALE_SHIFT= -fno-delete-null-pointer-checks -Wno-frame-address -Wno-address-of-packed-member -O2 -Wframe-larger-than=2048 -fstack-protector-strong -Werror -Wno-format-invalid-specifier -Wno-gnu -mno-global-merge -Wno-unused-but-set-variable -Wno-unused-const-variable -fno-omit-frame-pointer -fno-optimize-sibling-calls -ftrivial-auto-var-init=zero -enable-trivial-auto-var-init-zero-knowing-it-will-be-removed-from-clang -g -gdwarf-4 -fsanitize=shadow-call-stack -flto -fvisibility=default -Wdeclaration-after-statement -Wvla -Wno-pointer-sign -Wno-array-bounds -fno-strict-overflow -fno-stack-check -Werror=date-time -Werror=incompatible-pointer-types -fno-builtin-wcslen -Wno-initializer-overrides -Wno-format -Wno-sign-compare -Wno-format-zero-length -Wno-pointer-to-enum-cast -Wno-tautological-constant-out-of-range-compare -Wno-unaligned-access -Wno-enum-compare-conditional -mstack-protector-guard=sysreg -mstack-protector-guard-reg=sp_el0 -mstack-protector-guard-offset=1488 -fsanitize=array-bounds -fsanitize=local-bounds -fsanitize-undefined-trap-on-error -DMODULE -DKBUILD_BASENAME='"lsdriver.mod"' -DKBUILD_MODNAME='"lsdriver"' -D__KBUILD_MODNAME=kmod_lsdriver -c -o /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.mod.o /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.mod.c

source_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.mod.o := /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.mod.c

deps_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.mod.o := \
    $(wildcard include/config/module/unload.h) \
    $(wildcard include/config/retpoline.h) \
  /root/android13-5.10/common/include/linux/kconfig.h \
    $(wildcard include/config/cc/version/text.h) \
    $(wildcard include/config/cpu/big/endian.h) \
    $(wildcard include/config/booger.h) \
    $(wildcard include/config/foo.h) \
  /root/android13-5.10/common/include/linux/compiler_types.h \
    $(wildcard include/config/have/arch/compiler/h.h) \
    $(wildcard include/config/enable/must/check.h) \
    $(wildcard include/config/cc/has/asm/inline.h) \
  /root/android13-5.10/common/include/linux/compiler_attributes.h \
  /root/android13-5.10/common/include/linux/compiler-clang.h \
    $(wildcard include/config/arch/use/builtin/bswap.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/compiler.h \
  /root/android13-5.10/common/include/linux/module.h \
    $(wildcard include/config/modules.h) \
    $(wildcard include/config/sysfs.h) \
    $(wildcard include/config/modules/tree/lookup.h) \
    $(wildcard include/config/livepatch.h) \
    $(wildcard include/config/cfi/clang.h) \
    $(wildcard include/config/unused/symbols.h) \
    $(wildcard include/config/generic/bug.h) \
    $(wildcard include/config/kallsyms.h) \
    $(wildcard include/config/smp.h) \
    $(wildcard include/config/tracepoints.h) \
    $(wildcard include/config/tree/srcu.h) \
    $(wildcard include/config/bpf/events.h) \
    $(wildcard include/config/jump/label.h) \
    $(wildcard include/config/tracing.h) \
    $(wildcard include/config/event/tracing.h) \
    $(wildcard include/config/ftrace/mcount/record.h) \
    $(wildcard include/config/kprobes.h) \
    $(wildcard include/config/have/static/call/inline.h) \
    $(wildcard include/config/mitigation/its.h) \
    $(wildcard include/config/constructors.h) \
    $(wildcard include/config/function/error/injection.h) \
    $(wildcard include/config/module/sig.h) \
  /root/android13-5.10/common/include/linux/list.h \
    $(wildcard include/config/debug/list.h) \
  /root/android13-5.10/common/include/linux/types.h \
    $(wildcard include/config/have/uid16.h) \
    $(wildcard include/config/uid16.h) \
    $(wildcard include/config/arch/dma/addr/t/64bit.h) \
    $(wildcard include/config/phys/addr/t/64bit.h) \
    $(wildcard include/config/64bit.h) \
  /root/android13-5.10/common/include/uapi/linux/types.h \
  arch/arm64/include/generated/uapi/asm/types.h \
  /root/android13-5.10/common/include/uapi/asm-generic/types.h \
  /root/android13-5.10/common/include/asm-generic/int-ll64.h \
  /root/android13-5.10/common/include/uapi/asm-generic/int-ll64.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/bitsperlong.h \
  /root/android13-5.10/common/include/asm-generic/bitsperlong.h \
  /root/android13-5.10/common/include/uapi/asm-generic/bitsperlong.h \
  /root/android13-5.10/common/include/uapi/linux/posix_types.h \
  /root/android13-5.10/common/include/linux/stddef.h \
  /root/android13-5.10/common/include/uapi/linux/stddef.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/posix_types.h \
  /root/android13-5.10/common/include/uapi/asm-generic/posix_types.h \
  /root/android13-5.10/common/include/linux/poison.h \
    $(wildcard include/config/illegal/pointer/value.h) \
  /root/android13-5.10/common/include/linux/const.h \
  /root/android13-5.10/common/include/vdso/const.h \
  /root/android13-5.10/common/include/uapi/linux/const.h \
  /root/android13-5.10/common/include/linux/kernel.h \
    $(wildcard include/config/preempt/voluntary.h) \
    $(wildcard include/config/debug/atomic/sleep.h) \
    $(wildcard include/config/preempt/rt.h) \
    $(wildcard include/config/mmu.h) \
    $(wildcard include/config/prove/locking.h) \
    $(wildcard include/config/panic/timeout.h) \
  /root/android13-5.10/common/include/linux/limits.h \
  /root/android13-5.10/common/include/uapi/linux/limits.h \
  /root/android13-5.10/common/include/vdso/limits.h \
  /root/android13-5.10/common/include/linux/linkage.h \
    $(wildcard include/config/arch/use/sym/annotations.h) \
  /root/android13-5.10/common/include/linux/stringify.h \
  /root/android13-5.10/common/include/linux/export.h \
    $(wildcard include/config/modversions.h) \
    $(wildcard include/config/module/rel/crcs.h) \
    $(wildcard include/config/have/arch/prel32/relocations.h) \
    $(wildcard include/config/trim/unused/ksyms.h) \
  /root/android13-5.10/common/include/linux/compiler.h \
    $(wildcard include/config/trace/branch/profiling.h) \
    $(wildcard include/config/profile/all/branches.h) \
    $(wildcard include/config/stack/validation.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/rwonce.h \
    $(wildcard include/config/lto.h) \
    $(wildcard include/config/as/has/ldapr.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/alternative-macros.h \
  /root/android13-5.10/common/arch/arm64/include/asm/cpucaps.h \
  /root/android13-5.10/common/include/asm-generic/rwonce.h \
  /root/android13-5.10/common/include/linux/kasan-checks.h \
    $(wildcard include/config/kasan/generic.h) \
    $(wildcard include/config/kasan/sw/tags.h) \
  /root/android13-5.10/common/include/linux/kcsan-checks.h \
    $(wildcard include/config/kcsan.h) \
    $(wildcard include/config/kcsan/ignore/atomics.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/linkage.h \
    $(wildcard include/config/arm64/bti/kernel.h) \
  /root/android13-5.10/common/include/linux/bitops.h \
  /root/android13-5.10/common/include/linux/bits.h \
  /root/android13-5.10/common/include/vdso/bits.h \
  /root/android13-5.10/common/include/linux/build_bug.h \
  /root/android13-5.10/common/arch/arm64/include/asm/bitops.h \
  /root/android13-5.10/common/include/asm-generic/bitops/builtin-__ffs.h \
  /root/android13-5.10/common/include/asm-generic/bitops/builtin-ffs.h \
  /root/android13-5.10/common/include/asm-generic/bitops/builtin-__fls.h \
  /root/android13-5.10/common/include/asm-generic/bitops/builtin-fls.h \
  /root/android13-5.10/common/include/asm-generic/bitops/ffz.h \
  /root/android13-5.10/common/include/asm-generic/bitops/fls64.h \
  /root/android13-5.10/common/include/asm-generic/bitops/find.h \
    $(wildcard include/config/generic/find/first/bit.h) \
  /root/android13-5.10/common/include/asm-generic/bitops/sched.h \
  /root/android13-5.10/common/include/asm-generic/bitops/hweight.h \
  /root/android13-5.10/common/include/asm-generic/bitops/arch_hweight.h \
  /root/android13-5.10/common/include/asm-generic/bitops/const_hweight.h \
  /root/android13-5.10/common/include/asm-generic/bitops/atomic.h \
  /root/android13-5.10/common/include/linux/atomic.h \
  /root/android13-5.10/common/arch/arm64/include/asm/atomic.h \
  /root/android13-5.10/common/arch/arm64/include/asm/barrier.h \
    $(wildcard include/config/arm64/pseudo/nmi.h) \
  /root/android13-5.10/common/include/asm-generic/barrier.h \
  /root/android13-5.10/common/arch/arm64/include/asm/cmpxchg.h \
  /root/android13-5.10/common/arch/arm64/include/asm/lse.h \
    $(wildcard include/config/arm64/lse/atomics.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/atomic_ll_sc.h \
    $(wildcard include/config/cc/has/k/constraint.h) \
  /root/android13-5.10/common/include/linux/jump_label.h \
    $(wildcard include/config/have/arch/jump/label/relative.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/jump_label.h \
  /root/android13-5.10/common/arch/arm64/include/asm/insn.h \
  /root/android13-5.10/common/arch/arm64/include/asm/brk-imm.h \
  /root/android13-5.10/common/arch/arm64/include/asm/alternative.h \
  /root/android13-5.10/common/include/linux/init.h \
    $(wildcard include/config/strict/kernel/rwx.h) \
    $(wildcard include/config/strict/module/rwx.h) \
    $(wildcard include/config/lto/clang.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/atomic_lse.h \
  /root/android13-5.10/common/include/linux/atomic-arch-fallback.h \
    $(wildcard include/config/generic/atomic64.h) \
  /root/android13-5.10/common/include/asm-generic/atomic-instrumented.h \
  /root/android13-5.10/common/include/linux/instrumented.h \
  /root/android13-5.10/common/include/asm-generic/atomic-long.h \
  /root/android13-5.10/common/include/asm-generic/bitops/lock.h \
  /root/android13-5.10/common/include/asm-generic/bitops/non-atomic.h \
  /root/android13-5.10/common/include/asm-generic/bitops/le.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/byteorder.h \
  /root/android13-5.10/common/include/linux/byteorder/little_endian.h \
  /root/android13-5.10/common/include/uapi/linux/byteorder/little_endian.h \
  /root/android13-5.10/common/include/linux/swab.h \
  /root/android13-5.10/common/include/uapi/linux/swab.h \
  arch/arm64/include/generated/uapi/asm/swab.h \
  /root/android13-5.10/common/include/uapi/asm-generic/swab.h \
  /root/android13-5.10/common/include/linux/byteorder/generic.h \
  /root/android13-5.10/common/include/asm-generic/bitops/ext2-atomic-setbit.h \
  /root/android13-5.10/common/include/linux/kstrtox.h \
  /root/android13-5.10/common/include/linux/log2.h \
    $(wildcard include/config/arch/has/ilog2/u32.h) \
    $(wildcard include/config/arch/has/ilog2/u64.h) \
  /root/android13-5.10/common/include/linux/minmax.h \
  /root/android13-5.10/common/include/linux/typecheck.h \
  /root/android13-5.10/common/include/linux/printk.h \
    $(wildcard include/config/message/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/default.h) \
    $(wildcard include/config/console/loglevel/quiet.h) \
    $(wildcard include/config/early/printk.h) \
    $(wildcard include/config/printk/nmi.h) \
    $(wildcard include/config/printk.h) \
    $(wildcard include/config/dynamic/debug.h) \
    $(wildcard include/config/dynamic/debug/core.h) \
  /root/android13-5.10/common/include/linux/kern_levels.h \
  /root/android13-5.10/common/include/linux/cache.h \
    $(wildcard include/config/arch/has/cache/line/size.h) \
  /root/android13-5.10/common/include/uapi/linux/kernel.h \
  /root/android13-5.10/common/include/uapi/linux/sysinfo.h \
  /root/android13-5.10/common/arch/arm64/include/asm/cache.h \
    $(wildcard include/config/kasan/hw/tags.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/cputype.h \
  /root/android13-5.10/common/arch/arm64/include/asm/sysreg.h \
    $(wildcard include/config/broken/gas/inst.h) \
    $(wildcard include/config/arm64/pa/bits/52.h) \
    $(wildcard include/config/arm64/4k/pages.h) \
    $(wildcard include/config/arm64/16k/pages.h) \
    $(wildcard include/config/arm64/64k/pages.h) \
  /root/android13-5.10/common/include/linux/kasan-tags.h \
  /root/android13-5.10/common/include/linux/ratelimit_types.h \
  /root/android13-5.10/common/include/uapi/linux/param.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/param.h \
  /root/android13-5.10/common/include/asm-generic/param.h \
    $(wildcard include/config/hz.h) \
  /root/android13-5.10/common/include/uapi/asm-generic/param.h \
  /root/android13-5.10/common/include/linux/spinlock_types.h \
    $(wildcard include/config/debug/spinlock.h) \
    $(wildcard include/config/debug/lock/alloc.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/spinlock_types.h \
  /root/android13-5.10/common/include/asm-generic/qspinlock_types.h \
    $(wildcard include/config/nr/cpus.h) \
  /root/android13-5.10/common/include/asm-generic/qrwlock_types.h \
  /root/android13-5.10/common/include/linux/lockdep_types.h \
    $(wildcard include/config/prove/raw/lock/nesting.h) \
    $(wildcard include/config/preempt/lock.h) \
    $(wildcard include/config/lockdep.h) \
    $(wildcard include/config/lock/stat.h) \
  /root/android13-5.10/common/include/linux/rwlock_types.h \
  arch/arm64/include/generated/asm/div64.h \
  /root/android13-5.10/common/include/asm-generic/div64.h \
  /root/android13-5.10/common/include/linux/stat.h \
  /root/android13-5.10/common/arch/arm64/include/asm/stat.h \
    $(wildcard include/config/compat.h) \
  arch/arm64/include/generated/uapi/asm/stat.h \
  /root/android13-5.10/common/include/uapi/asm-generic/stat.h \
  /root/android13-5.10/common/include/linux/time.h \
    $(wildcard include/config/arch/uses/gettimeoffset.h) \
    $(wildcard include/config/posix/timers.h) \
  /root/android13-5.10/common/include/linux/math64.h \
    $(wildcard include/config/arch/supports/int128.h) \
  /root/android13-5.10/common/include/vdso/math64.h \
  /root/android13-5.10/common/include/linux/time64.h \
  /root/android13-5.10/common/include/vdso/time64.h \
  /root/android13-5.10/common/include/uapi/linux/time.h \
  /root/android13-5.10/common/include/uapi/linux/time_types.h \
  /root/android13-5.10/common/include/linux/time32.h \
  /root/android13-5.10/common/include/linux/timex.h \
  /root/android13-5.10/common/include/uapi/linux/timex.h \
  /root/android13-5.10/common/arch/arm64/include/asm/timex.h \
  /root/android13-5.10/common/arch/arm64/include/asm/arch_timer.h \
    $(wildcard include/config/arm/arch/timer/ool/workaround.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/hwcap.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/hwcap.h \
  /root/android13-5.10/common/arch/arm64/include/asm/cpufeature.h \
    $(wildcard include/config/arm64/sw/ttbr0/pan.h) \
    $(wildcard include/config/arm64/sve.h) \
    $(wildcard include/config/arm64/cnp.h) \
    $(wildcard include/config/arm64/ptr/auth.h) \
    $(wildcard include/config/arm64/mte.h) \
    $(wildcard include/config/arm64/debug/priority/masking.h) \
    $(wildcard include/config/arm64/bti.h) \
    $(wildcard include/config/arm64/tlb/range.h) \
    $(wildcard include/config/arm64/pa/bits.h) \
    $(wildcard include/config/arm64/hw/afdbm.h) \
    $(wildcard include/config/arm64/amu/extn.h) \
  /root/android13-5.10/common/include/linux/bug.h \
    $(wildcard include/config/bug/on/data/corruption.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/bug.h \
  /root/android13-5.10/common/arch/arm64/include/asm/asm-bug.h \
    $(wildcard include/config/debug/bugverbose.h) \
  /root/android13-5.10/common/include/asm-generic/bug.h \
    $(wildcard include/config/bug.h) \
    $(wildcard include/config/generic/bug/relative/pointers.h) \
  /root/android13-5.10/common/include/linux/instrumentation.h \
    $(wildcard include/config/debug/entry.h) \
  /root/android13-5.10/common/include/linux/smp.h \
    $(wildcard include/config/up/late/init.h) \
    $(wildcard include/config/debug/preempt.h) \
  /root/android13-5.10/common/include/linux/errno.h \
  /root/android13-5.10/common/include/uapi/linux/errno.h \
  arch/arm64/include/generated/uapi/asm/errno.h \
  /root/android13-5.10/common/include/uapi/asm-generic/errno.h \
  /root/android13-5.10/common/include/uapi/asm-generic/errno-base.h \
  /root/android13-5.10/common/include/linux/cpumask.h \
    $(wildcard include/config/cpumask/offstack.h) \
    $(wildcard include/config/hotplug/cpu.h) \
    $(wildcard include/config/debug/per/cpu/maps.h) \
  /root/android13-5.10/common/include/linux/threads.h \
    $(wildcard include/config/base/small.h) \
  /root/android13-5.10/common/include/linux/bitmap.h \
  /root/android13-5.10/common/include/linux/string.h \
    $(wildcard include/config/binary/printf.h) \
    $(wildcard include/config/fortify/source.h) \
  /root/android13-5.10/common/include/uapi/linux/string.h \
  /root/android13-5.10/common/arch/arm64/include/asm/string.h \
    $(wildcard include/config/arch/has/uaccess/flushcache.h) \
  /root/android13-5.10/common/include/linux/smp_types.h \
  /root/android13-5.10/common/include/linux/llist.h \
    $(wildcard include/config/arch/have/nmi/safe/cmpxchg.h) \
  /root/android13-5.10/common/include/linux/preempt.h \
    $(wildcard include/config/preempt/count.h) \
    $(wildcard include/config/trace/preempt/toggle.h) \
    $(wildcard include/config/preemption.h) \
    $(wildcard include/config/preempt/notifiers.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/preempt.h \
  /root/android13-5.10/common/include/linux/thread_info.h \
    $(wildcard include/config/thread/info/in/task.h) \
    $(wildcard include/config/have/arch/within/stack/frames.h) \
    $(wildcard include/config/hardened/usercopy.h) \
  /root/android13-5.10/common/include/linux/restart_block.h \
  /root/android13-5.10/common/arch/arm64/include/asm/current.h \
  /root/android13-5.10/common/arch/arm64/include/asm/thread_info.h \
    $(wildcard include/config/shadow/call/stack.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/memory.h \
    $(wildcard include/config/arm64/va/bits.h) \
    $(wildcard include/config/kasan/shadow/offset.h) \
    $(wildcard include/config/kasan.h) \
    $(wildcard include/config/vmap/stack.h) \
    $(wildcard include/config/debug/virtual.h) \
    $(wildcard include/config/sparsemem/vmemmap.h) \
    $(wildcard include/config/efi.h) \
    $(wildcard include/config/arm/gic/v3/its.h) \
  /root/android13-5.10/common/include/linux/sizes.h \
  /root/android13-5.10/common/arch/arm64/include/asm/page-def.h \
    $(wildcard include/config/arm64/page/shift.h) \
  /root/android13-5.10/common/include/linux/mmdebug.h \
    $(wildcard include/config/debug/vm.h) \
    $(wildcard include/config/debug/vm/pgflags.h) \
  /root/android13-5.10/common/include/asm-generic/memory_model.h \
    $(wildcard include/config/flatmem.h) \
    $(wildcard include/config/discontigmem.h) \
    $(wildcard include/config/sparsemem.h) \
  /root/android13-5.10/common/include/linux/pfn.h \
  /root/android13-5.10/common/arch/arm64/include/asm/stack_pointer.h \
  /root/android13-5.10/common/arch/arm64/include/asm/smp.h \
    $(wildcard include/config/arm64/acpi/parking/protocol.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/percpu.h \
  /root/android13-5.10/common/include/asm-generic/percpu.h \
    $(wildcard include/config/have/setup/per/cpu/area.h) \
  /root/android13-5.10/common/include/linux/percpu-defs.h \
    $(wildcard include/config/debug/force/weak/per/cpu.h) \
    $(wildcard include/config/amd/mem/encrypt.h) \
  /root/android13-5.10/common/include/clocksource/arm_arch_timer.h \
    $(wildcard include/config/arm/arch/timer.h) \
  /root/android13-5.10/common/include/linux/timecounter.h \
  /root/android13-5.10/common/include/asm-generic/timex.h \
  /root/android13-5.10/common/include/vdso/time32.h \
  /root/android13-5.10/common/include/vdso/time.h \
  /root/android13-5.10/common/arch/arm64/include/asm/compat.h \
  /root/android13-5.10/common/include/asm-generic/compat.h \
    $(wildcard include/config/compat/for/u64/alignment.h) \
  /root/android13-5.10/common/include/linux/sched.h \
    $(wildcard include/config/virt/cpu/accounting/native.h) \
    $(wildcard include/config/sched/info.h) \
    $(wildcard include/config/schedstats.h) \
    $(wildcard include/config/fair/group/sched.h) \
    $(wildcard include/config/rt/group/sched.h) \
    $(wildcard include/config/rt/mutexes.h) \
    $(wildcard include/config/uclamp/task.h) \
    $(wildcard include/config/uclamp/buckets/count.h) \
    $(wildcard include/config/cgroup/sched.h) \
    $(wildcard include/config/blk/dev/io/trace.h) \
    $(wildcard include/config/preempt/rcu.h) \
    $(wildcard include/config/tasks/rcu.h) \
    $(wildcard include/config/tasks/trace/rcu.h) \
    $(wildcard include/config/psi.h) \
    $(wildcard include/config/memcg.h) \
    $(wildcard include/config/lru/gen.h) \
    $(wildcard include/config/compat/brk.h) \
    $(wildcard include/config/cgroups.h) \
    $(wildcard include/config/blk/cgroup.h) \
    $(wildcard include/config/stackprotector.h) \
    $(wildcard include/config/arch/has/scaled/cputime.h) \
    $(wildcard include/config/cpu/freq/times.h) \
    $(wildcard include/config/virt/cpu/accounting/gen.h) \
    $(wildcard include/config/no/hz/full.h) \
    $(wildcard include/config/posix/cputimers.h) \
    $(wildcard include/config/posix/cpu/timers/task/work.h) \
    $(wildcard include/config/keys.h) \
    $(wildcard include/config/sysvipc.h) \
    $(wildcard include/config/detect/hung/task.h) \
    $(wildcard include/config/io/uring.h) \
    $(wildcard include/config/audit.h) \
    $(wildcard include/config/auditsyscall.h) \
    $(wildcard include/config/debug/mutexes.h) \
    $(wildcard include/config/trace/irqflags.h) \
    $(wildcard include/config/ubsan.h) \
    $(wildcard include/config/ubsan/trap.h) \
    $(wildcard include/config/block.h) \
    $(wildcard include/config/compaction.h) \
    $(wildcard include/config/task/xacct.h) \
    $(wildcard include/config/cpusets.h) \
    $(wildcard include/config/x86/cpu/resctrl.h) \
    $(wildcard include/config/futex.h) \
    $(wildcard include/config/perf/events.h) \
    $(wildcard include/config/numa.h) \
    $(wildcard include/config/numa/balancing.h) \
    $(wildcard include/config/rseq.h) \
    $(wildcard include/config/task/delay/acct.h) \
    $(wildcard include/config/fault/injection.h) \
    $(wildcard include/config/latencytop.h) \
    $(wildcard include/config/kunit.h) \
    $(wildcard include/config/function/graph/tracer.h) \
    $(wildcard include/config/kcov.h) \
    $(wildcard include/config/uprobes.h) \
    $(wildcard include/config/bcache.h) \
    $(wildcard include/config/security.h) \
    $(wildcard include/config/gcc/plugin/stackleak.h) \
    $(wildcard include/config/x86/mce.h) \
    $(wildcard include/config/rt/softint/optimization.h) \
    $(wildcard include/config/arch/task/struct/on/stack.h) \
    $(wildcard include/config/debug/rseq.h) \
  /root/android13-5.10/common/include/uapi/linux/sched.h \
  /root/android13-5.10/common/include/linux/pid.h \
  /root/android13-5.10/common/include/linux/rculist.h \
    $(wildcard include/config/prove/rcu/list.h) \
  /root/android13-5.10/common/include/linux/rcupdate.h \
    $(wildcard include/config/tiny/rcu.h) \
    $(wildcard include/config/tasks/rcu/generic.h) \
    $(wildcard include/config/rcu/stall/common.h) \
    $(wildcard include/config/rcu/nocb/cpu.h) \
    $(wildcard include/config/tasks/rude/rcu.h) \
    $(wildcard include/config/tree/rcu.h) \
    $(wildcard include/config/debug/objects/rcu/head.h) \
    $(wildcard include/config/prove/rcu.h) \
    $(wildcard include/config/rcu/boost.h) \
    $(wildcard include/config/arch/weak/release/acquire.h) \
  /root/android13-5.10/common/include/linux/irqflags.h \
    $(wildcard include/config/irqsoff/tracer.h) \
    $(wildcard include/config/preempt/tracer.h) \
    $(wildcard include/config/trace/irqflags/support.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/irqflags.h \
  /root/android13-5.10/common/arch/arm64/include/asm/ptrace.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/ptrace.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/sve_context.h \
  /root/android13-5.10/common/include/linux/bottom_half.h \
  /root/android13-5.10/common/include/linux/lockdep.h \
    $(wildcard include/config/debug/locking/api/selftests.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/processor.h \
    $(wildcard include/config/kuser/helpers.h) \
    $(wildcard include/config/arm64/force/52bit.h) \
    $(wildcard include/config/have/hw/breakpoint.h) \
    $(wildcard include/config/arm64/tagged/addr/abi.h) \
  /root/android13-5.10/common/include/linux/android_vendor.h \
  /root/android13-5.10/common/include/vdso/processor.h \
  /root/android13-5.10/common/arch/arm64/include/asm/vdso/processor.h \
  /root/android13-5.10/common/arch/arm64/include/asm/hw_breakpoint.h \
  /root/android13-5.10/common/arch/arm64/include/asm/virt.h \
    $(wildcard include/config/kvm.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/sections.h \
  /root/android13-5.10/common/include/asm-generic/sections.h \
  /root/android13-5.10/common/arch/arm64/include/asm/kasan.h \
  /root/android13-5.10/common/arch/arm64/include/asm/mte-kasan.h \
  /root/android13-5.10/common/arch/arm64/include/asm/mte-def.h \
  /root/android13-5.10/common/arch/arm64/include/asm/pgtable-types.h \
    $(wildcard include/config/pgtable/levels.h) \
  /root/android13-5.10/common/include/asm-generic/pgtable-nopud.h \
  /root/android13-5.10/common/include/asm-generic/pgtable-nop4d.h \
  /root/android13-5.10/common/arch/arm64/include/asm/pgtable-hwdef.h \
    $(wildcard include/config/arm64/cont/pte/shift.h) \
    $(wildcard include/config/arm64/cont/pmd/shift.h) \
    $(wildcard include/config/arm64/va/bits/52.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/pointer_auth.h \
  /root/android13-5.10/common/include/uapi/linux/prctl.h \
  /root/android13-5.10/common/include/linux/random.h \
    $(wildcard include/config/arch/random.h) \
  /root/android13-5.10/common/include/linux/once.h \
  /root/android13-5.10/common/include/uapi/linux/random.h \
  /root/android13-5.10/common/include/uapi/linux/ioctl.h \
  arch/arm64/include/generated/uapi/asm/ioctl.h \
  /root/android13-5.10/common/include/asm-generic/ioctl.h \
  /root/android13-5.10/common/include/uapi/asm-generic/ioctl.h \
  /root/android13-5.10/common/include/linux/irqnr.h \
  /root/android13-5.10/common/include/uapi/linux/irqnr.h \
  /root/android13-5.10/common/include/linux/prandom.h \
  /root/android13-5.10/common/include/linux/percpu.h \
    $(wildcard include/config/need/per/cpu/embed/first/chunk.h) \
    $(wildcard include/config/need/per/cpu/page/first/chunk.h) \
  /root/android13-5.10/common/include/linux/siphash.h \
    $(wildcard include/config/have/efficient/unaligned/access.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/archrandom.h \
  /root/android13-5.10/common/include/linux/arm-smccc.h \
    $(wildcard include/config/arm64.h) \
    $(wildcard include/config/arm.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/spectre.h \
  /root/android13-5.10/common/arch/arm64/include/asm/fpsimd.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/sigcontext.h \
  /root/android13-5.10/common/include/linux/rcutree.h \
  /root/android13-5.10/common/include/linux/wait.h \
  /root/android13-5.10/common/include/linux/spinlock.h \
  arch/arm64/include/generated/asm/mmiowb.h \
  /root/android13-5.10/common/include/asm-generic/mmiowb.h \
    $(wildcard include/config/mmiowb.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/spinlock.h \
  arch/arm64/include/generated/asm/qrwlock.h \
  /root/android13-5.10/common/include/asm-generic/qrwlock.h \
  arch/arm64/include/generated/asm/qspinlock.h \
  /root/android13-5.10/common/include/asm-generic/qspinlock.h \
  /root/android13-5.10/common/include/linux/rwlock.h \
    $(wildcard include/config/preempt.h) \
  /root/android13-5.10/common/include/linux/spinlock_api_smp.h \
    $(wildcard include/config/inline/spin/lock.h) \
    $(wildcard include/config/inline/spin/lock/bh.h) \
    $(wildcard include/config/inline/spin/lock/irq.h) \
    $(wildcard include/config/inline/spin/lock/irqsave.h) \
    $(wildcard include/config/inline/spin/trylock.h) \
    $(wildcard include/config/inline/spin/trylock/bh.h) \
    $(wildcard include/config/uninline/spin/unlock.h) \
    $(wildcard include/config/inline/spin/unlock/bh.h) \
    $(wildcard include/config/inline/spin/unlock/irq.h) \
    $(wildcard include/config/inline/spin/unlock/irqrestore.h) \
    $(wildcard include/config/generic/lockbreak.h) \
  /root/android13-5.10/common/include/linux/rwlock_api_smp.h \
    $(wildcard include/config/inline/read/lock.h) \
    $(wildcard include/config/inline/write/lock.h) \
    $(wildcard include/config/inline/read/lock/bh.h) \
    $(wildcard include/config/inline/write/lock/bh.h) \
    $(wildcard include/config/inline/read/lock/irq.h) \
    $(wildcard include/config/inline/write/lock/irq.h) \
    $(wildcard include/config/inline/read/lock/irqsave.h) \
    $(wildcard include/config/inline/write/lock/irqsave.h) \
    $(wildcard include/config/inline/read/trylock.h) \
    $(wildcard include/config/inline/write/trylock.h) \
    $(wildcard include/config/inline/read/unlock.h) \
    $(wildcard include/config/inline/write/unlock.h) \
    $(wildcard include/config/inline/read/unlock/bh.h) \
    $(wildcard include/config/inline/write/unlock/bh.h) \
    $(wildcard include/config/inline/read/unlock/irq.h) \
    $(wildcard include/config/inline/write/unlock/irq.h) \
    $(wildcard include/config/inline/read/unlock/irqrestore.h) \
    $(wildcard include/config/inline/write/unlock/irqrestore.h) \
  /root/android13-5.10/common/include/uapi/linux/wait.h \
  /root/android13-5.10/common/include/linux/refcount.h \
  /root/android13-5.10/common/include/linux/sem.h \
  /root/android13-5.10/common/include/uapi/linux/sem.h \
  /root/android13-5.10/common/include/linux/ipc.h \
  /root/android13-5.10/common/include/linux/uidgid.h \
    $(wildcard include/config/multiuser.h) \
    $(wildcard include/config/user/ns.h) \
  /root/android13-5.10/common/include/linux/highuid.h \
  /root/android13-5.10/common/include/linux/rhashtable-types.h \
  /root/android13-5.10/common/include/linux/mutex.h \
    $(wildcard include/config/mutex/spin/on/owner.h) \
  /root/android13-5.10/common/include/linux/osq_lock.h \
  /root/android13-5.10/common/include/linux/debug_locks.h \
  /root/android13-5.10/common/include/linux/workqueue.h \
    $(wildcard include/config/debug/objects/work.h) \
    $(wildcard include/config/freezer.h) \
    $(wildcard include/config/wq/watchdog.h) \
  /root/android13-5.10/common/include/linux/timer.h \
    $(wildcard include/config/debug/objects/timers.h) \
    $(wildcard include/config/no/hz/common.h) \
  /root/android13-5.10/common/include/linux/ktime.h \
  /root/android13-5.10/common/include/linux/jiffies.h \
  /root/android13-5.10/common/include/vdso/jiffies.h \
  include/generated/timeconst.h \
  /root/android13-5.10/common/include/vdso/ktime.h \
  /root/android13-5.10/common/include/linux/timekeeping.h \
  /root/android13-5.10/common/include/linux/clocksource_ids.h \
  /root/android13-5.10/common/include/linux/timekeeping32.h \
  /root/android13-5.10/common/include/linux/debugobjects.h \
    $(wildcard include/config/debug/objects.h) \
    $(wildcard include/config/debug/objects/free.h) \
  /root/android13-5.10/common/include/linux/android_kabi.h \
  /root/android13-5.10/common/include/uapi/linux/ipc.h \
  arch/arm64/include/generated/uapi/asm/ipcbuf.h \
  /root/android13-5.10/common/include/uapi/asm-generic/ipcbuf.h \
  arch/arm64/include/generated/uapi/asm/sembuf.h \
  /root/android13-5.10/common/include/uapi/asm-generic/sembuf.h \
  /root/android13-5.10/common/include/linux/shm.h \
  /root/android13-5.10/common/arch/arm64/include/asm/page.h \
  /root/android13-5.10/common/include/linux/personality.h \
  /root/android13-5.10/common/include/uapi/linux/personality.h \
  /root/android13-5.10/common/include/asm-generic/getorder.h \
  /root/android13-5.10/common/include/uapi/linux/shm.h \
  /root/android13-5.10/common/include/uapi/asm-generic/hugetlb_encode.h \
  arch/arm64/include/generated/uapi/asm/shmbuf.h \
  /root/android13-5.10/common/include/uapi/asm-generic/shmbuf.h \
  /root/android13-5.10/common/arch/arm64/include/asm/shmparam.h \
  /root/android13-5.10/common/include/asm-generic/shmparam.h \
  /root/android13-5.10/common/include/linux/plist.h \
    $(wildcard include/config/debug/plist.h) \
  /root/android13-5.10/common/include/linux/hrtimer.h \
    $(wildcard include/config/high/res/timers.h) \
    $(wildcard include/config/time/low/res.h) \
    $(wildcard include/config/timerfd.h) \
  /root/android13-5.10/common/include/linux/hrtimer_defs.h \
  /root/android13-5.10/common/include/linux/rbtree.h \
  /root/android13-5.10/common/include/linux/seqlock.h \
  /root/android13-5.10/common/include/linux/ww_mutex.h \
    $(wildcard include/config/debug/ww/mutex/slowpath.h) \
  /root/android13-5.10/common/include/linux/timerqueue.h \
  /root/android13-5.10/common/include/linux/seccomp.h \
    $(wildcard include/config/seccomp.h) \
    $(wildcard include/config/have/arch/seccomp/filter.h) \
    $(wildcard include/config/seccomp/filter.h) \
    $(wildcard include/config/checkpoint/restore.h) \
  /root/android13-5.10/common/include/uapi/linux/seccomp.h \
  /root/android13-5.10/common/arch/arm64/include/asm/seccomp.h \
  /root/android13-5.10/common/arch/arm64/include/asm/unistd.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/unistd.h \
  /root/android13-5.10/common/include/uapi/asm-generic/unistd.h \
  /root/android13-5.10/common/include/asm-generic/seccomp.h \
  /root/android13-5.10/common/include/uapi/linux/unistd.h \
  /root/android13-5.10/common/include/linux/nodemask.h \
    $(wildcard include/config/highmem.h) \
  /root/android13-5.10/common/include/linux/numa.h \
    $(wildcard include/config/nodes/shift.h) \
    $(wildcard include/config/numa/keep/meminfo.h) \
  /root/android13-5.10/common/include/linux/resource.h \
  /root/android13-5.10/common/include/uapi/linux/resource.h \
  arch/arm64/include/generated/uapi/asm/resource.h \
  /root/android13-5.10/common/include/asm-generic/resource.h \
  /root/android13-5.10/common/include/uapi/asm-generic/resource.h \
  /root/android13-5.10/common/include/linux/latencytop.h \
  /root/android13-5.10/common/include/linux/sched/prio.h \
  /root/android13-5.10/common/include/linux/sched/types.h \
  /root/android13-5.10/common/include/linux/signal_types.h \
    $(wildcard include/config/old/sigaction.h) \
  /root/android13-5.10/common/include/uapi/linux/signal.h \
  /root/android13-5.10/common/arch/arm64/include/asm/signal.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/signal.h \
  /root/android13-5.10/common/include/asm-generic/signal.h \
  /root/android13-5.10/common/include/uapi/asm-generic/signal.h \
  /root/android13-5.10/common/include/uapi/asm-generic/signal-defs.h \
  arch/arm64/include/generated/uapi/asm/siginfo.h \
  /root/android13-5.10/common/include/uapi/asm-generic/siginfo.h \
  /root/android13-5.10/common/include/linux/mm_types_task.h \
    $(wildcard include/config/arch/want/batched/unmap/tlb/flush.h) \
    $(wildcard include/config/split/ptlock/cpus.h) \
    $(wildcard include/config/arch/enable/split/pmd/ptlock.h) \
  /root/android13-5.10/common/include/linux/task_io_accounting.h \
    $(wildcard include/config/task/io/accounting.h) \
  /root/android13-5.10/common/include/linux/posix-timers.h \
  /root/android13-5.10/common/include/linux/alarmtimer.h \
    $(wildcard include/config/rtc/class.h) \
  /root/android13-5.10/common/include/linux/task_work.h \
  /root/android13-5.10/common/include/uapi/linux/rseq.h \
  /root/android13-5.10/common/include/linux/kcsan.h \
  /root/android13-5.10/common/include/linux/sched/task_stack.h \
    $(wildcard include/config/stack/growsup.h) \
    $(wildcard include/config/debug/stack/usage.h) \
  /root/android13-5.10/common/include/uapi/linux/magic.h \
  /root/android13-5.10/common/include/uapi/linux/stat.h \
  /root/android13-5.10/common/include/linux/kmod.h \
  /root/android13-5.10/common/include/linux/umh.h \
  /root/android13-5.10/common/include/linux/gfp.h \
    $(wildcard include/config/cma.h) \
    $(wildcard include/config/zone/dma.h) \
    $(wildcard include/config/zone/dma32.h) \
    $(wildcard include/config/zone/device.h) \
    $(wildcard include/config/pm/sleep.h) \
    $(wildcard include/config/contig/alloc.h) \
  /root/android13-5.10/common/include/linux/mmzone.h \
    $(wildcard include/config/force/max/zoneorder.h) \
    $(wildcard include/config/memory/isolation.h) \
    $(wildcard include/config/lru/gen/stats.h) \
    $(wildcard include/config/need/multiple/nodes.h) \
    $(wildcard include/config/memory/hotplug.h) \
    $(wildcard include/config/transparent/hugepage.h) \
    $(wildcard include/config/flat/node/mem/map.h) \
    $(wildcard include/config/page/extension.h) \
    $(wildcard include/config/deferred/struct/page/init.h) \
    $(wildcard include/config/have/memoryless/nodes.h) \
    $(wildcard include/config/sparsemem/extreme.h) \
    $(wildcard include/config/memory/hotremove.h) \
    $(wildcard include/config/have/arch/pfn/valid.h) \
    $(wildcard include/config/holes/in/zone.h) \
  /root/android13-5.10/common/include/linux/pageblock-flags.h \
    $(wildcard include/config/hugetlb/page.h) \
    $(wildcard include/config/hugetlb/page/size/variable.h) \
  /root/android13-5.10/common/include/linux/page-flags-layout.h \
  include/generated/bounds.h \
  /root/android13-5.10/common/arch/arm64/include/asm/sparsemem.h \
  /root/android13-5.10/common/include/linux/mm_types.h \
    $(wildcard include/config/have/aligned/struct/page.h) \
    $(wildcard include/config/userfaultfd.h) \
    $(wildcard include/config/swap.h) \
    $(wildcard include/config/speculative/page/fault.h) \
    $(wildcard include/config/have/arch/compat/mmap/bases.h) \
    $(wildcard include/config/membarrier.h) \
    $(wildcard include/config/aio.h) \
    $(wildcard include/config/mmu/notifier.h) \
    $(wildcard include/config/iommu/support.h) \
  /root/android13-5.10/common/include/linux/auxvec.h \
  /root/android13-5.10/common/include/uapi/linux/auxvec.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/auxvec.h \
  /root/android13-5.10/common/include/linux/kref.h \
  /root/android13-5.10/common/include/linux/rwsem.h \
    $(wildcard include/config/rwsem/spin/on/owner.h) \
    $(wildcard include/config/debug/rwsems.h) \
  /root/android13-5.10/common/include/linux/err.h \
  /root/android13-5.10/common/include/linux/completion.h \
  /root/android13-5.10/common/include/linux/swait.h \
  /root/android13-5.10/common/include/linux/uprobes.h \
  /root/android13-5.10/common/arch/arm64/include/asm/uprobes.h \
  /root/android13-5.10/common/arch/arm64/include/asm/debug-monitors.h \
  /root/android13-5.10/common/arch/arm64/include/asm/esr.h \
  /root/android13-5.10/common/arch/arm64/include/asm/probes.h \
  /root/android13-5.10/common/arch/arm64/include/asm/mmu.h \
  /root/android13-5.10/common/include/linux/page-flags.h \
    $(wildcard include/config/arch/uses/pg/uncached.h) \
    $(wildcard include/config/memory/failure.h) \
    $(wildcard include/config/page/idle/flag.h) \
    $(wildcard include/config/thp/swap.h) \
    $(wildcard include/config/ksm.h) \
  /root/android13-5.10/common/include/linux/memory_hotplug.h \
    $(wildcard include/config/arch/has/add/pages.h) \
    $(wildcard include/config/have/arch/nodedata/extension.h) \
    $(wildcard include/config/have/bootmem/info/node.h) \
  /root/android13-5.10/common/include/linux/notifier.h \
  /root/android13-5.10/common/include/linux/srcu.h \
    $(wildcard include/config/tiny/srcu.h) \
    $(wildcard include/config/srcu.h) \
  /root/android13-5.10/common/include/linux/rcu_segcblist.h \
  /root/android13-5.10/common/include/linux/srcutree.h \
  /root/android13-5.10/common/include/linux/rcu_node_tree.h \
    $(wildcard include/config/rcu/fanout.h) \
    $(wildcard include/config/rcu/fanout/leaf.h) \
  /root/android13-5.10/common/include/linux/topology.h \
    $(wildcard include/config/use/percpu/numa/node/id.h) \
    $(wildcard include/config/sched/smt.h) \
  /root/android13-5.10/common/include/linux/arch_topology.h \
    $(wildcard include/config/generic/arch/topology.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/topology.h \
  /root/android13-5.10/common/include/asm-generic/topology.h \
  /root/android13-5.10/common/include/linux/sysctl.h \
    $(wildcard include/config/sysctl.h) \
  /root/android13-5.10/common/include/uapi/linux/sysctl.h \
  /root/android13-5.10/common/include/linux/elf.h \
    $(wildcard include/config/arch/use/gnu/property.h) \
    $(wildcard include/config/arch/have/elf/prot.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/elf.h \
    $(wildcard include/config/compat/vdso.h) \
  arch/arm64/include/generated/asm/user.h \
  /root/android13-5.10/common/include/asm-generic/user.h \
  /root/android13-5.10/common/include/uapi/linux/elf.h \
  /root/android13-5.10/common/include/uapi/linux/elf-em.h \
  /root/android13-5.10/common/include/linux/fs.h \
    $(wildcard include/config/read/only/thp/for/fs.h) \
    $(wildcard include/config/fs/posix/acl.h) \
    $(wildcard include/config/cgroup/writeback.h) \
    $(wildcard include/config/ima.h) \
    $(wildcard include/config/file/locking.h) \
    $(wildcard include/config/fsnotify.h) \
    $(wildcard include/config/fs/encryption.h) \
    $(wildcard include/config/fs/verity.h) \
    $(wildcard include/config/epoll.h) \
    $(wildcard include/config/unicode.h) \
    $(wildcard include/config/quota.h) \
    $(wildcard include/config/fs/dax.h) \
    $(wildcard include/config/mandatory/file/locking.h) \
    $(wildcard include/config/migration.h) \
  /root/android13-5.10/common/include/linux/wait_bit.h \
  /root/android13-5.10/common/include/linux/kdev_t.h \
  /root/android13-5.10/common/include/uapi/linux/kdev_t.h \
  /root/android13-5.10/common/include/linux/dcache.h \
  /root/android13-5.10/common/include/linux/rculist_bl.h \
  /root/android13-5.10/common/include/linux/list_bl.h \
  /root/android13-5.10/common/include/linux/bit_spinlock.h \
  /root/android13-5.10/common/include/linux/lockref.h \
    $(wildcard include/config/arch/use/cmpxchg/lockref.h) \
  /root/android13-5.10/common/include/linux/stringhash.h \
    $(wildcard include/config/dcache/word/access.h) \
  /root/android13-5.10/common/include/linux/hash.h \
    $(wildcard include/config/have/arch/hash.h) \
  /root/android13-5.10/common/include/linux/path.h \
  /root/android13-5.10/common/include/linux/list_lru.h \
    $(wildcard include/config/memcg/kmem.h) \
  /root/android13-5.10/common/include/linux/shrinker.h \
  /root/android13-5.10/common/include/linux/radix-tree.h \
  /root/android13-5.10/common/include/linux/xarray.h \
    $(wildcard include/config/xarray/multi.h) \
  /root/android13-5.10/common/include/linux/sched/mm.h \
    $(wildcard include/config/arch/has/membarrier/callbacks.h) \
  /root/android13-5.10/common/include/linux/sync_core.h \
    $(wildcard include/config/arch/has/sync/core/before/usermode.h) \
  /root/android13-5.10/common/include/linux/local_lock.h \
  /root/android13-5.10/common/include/linux/local_lock_internal.h \
  /root/android13-5.10/common/include/linux/capability.h \
  /root/android13-5.10/common/include/uapi/linux/capability.h \
  /root/android13-5.10/common/include/linux/semaphore.h \
  /root/android13-5.10/common/include/linux/fcntl.h \
    $(wildcard include/config/arch/32bit/off/t.h) \
  /root/android13-5.10/common/include/uapi/linux/fcntl.h \
  /root/android13-5.10/common/arch/arm64/include/uapi/asm/fcntl.h \
  /root/android13-5.10/common/include/uapi/asm-generic/fcntl.h \
  /root/android13-5.10/common/include/uapi/linux/openat2.h \
  /root/android13-5.10/common/include/linux/migrate_mode.h \
  /root/android13-5.10/common/include/linux/percpu-rwsem.h \
  /root/android13-5.10/common/include/linux/rcuwait.h \
  /root/android13-5.10/common/include/linux/sched/signal.h \
    $(wildcard include/config/sched/autogroup.h) \
    $(wildcard include/config/bsd/process/acct.h) \
    $(wildcard include/config/taskstats.h) \
  /root/android13-5.10/common/include/linux/signal.h \
    $(wildcard include/config/proc/fs.h) \
  /root/android13-5.10/common/include/linux/sched/jobctl.h \
  /root/android13-5.10/common/include/linux/sched/task.h \
    $(wildcard include/config/have/exit/thread.h) \
    $(wildcard include/config/arch/wants/dynamic/task/struct.h) \
    $(wildcard include/config/have/arch/thread/struct/whitelist.h) \
  /root/android13-5.10/common/include/linux/uaccess.h \
    $(wildcard include/config/set/fs.h) \
  /root/android13-5.10/common/include/linux/fault-inject-usercopy.h \
    $(wildcard include/config/fault/injection/usercopy.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/uaccess.h \
    $(wildcard include/config/arm64/uao.h) \
    $(wildcard include/config/arm64/pan.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/kernel-pgtable.h \
    $(wildcard include/config/randomize/base.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/mte.h \
  /root/android13-5.10/common/include/linux/bitfield.h \
  /root/android13-5.10/common/arch/arm64/include/asm/extable.h \
    $(wildcard include/config/bpf/jit.h) \
  /root/android13-5.10/common/include/linux/cred.h \
    $(wildcard include/config/debug/credentials.h) \
  /root/android13-5.10/common/include/linux/key.h \
    $(wildcard include/config/key/notifications.h) \
    $(wildcard include/config/net.h) \
  /root/android13-5.10/common/include/linux/assoc_array.h \
    $(wildcard include/config/associative/array.h) \
  /root/android13-5.10/common/include/linux/sched/user.h \
    $(wildcard include/config/fanotify.h) \
    $(wildcard include/config/posix/mqueue.h) \
    $(wildcard include/config/bpf/syscall.h) \
    $(wildcard include/config/watch/queue.h) \
  /root/android13-5.10/common/include/linux/ratelimit.h \
  /root/android13-5.10/common/include/linux/rcu_sync.h \
  /root/android13-5.10/common/include/linux/delayed_call.h \
  /root/android13-5.10/common/include/linux/uuid.h \
  /root/android13-5.10/common/include/uapi/linux/uuid.h \
  /root/android13-5.10/common/include/linux/errseq.h \
  /root/android13-5.10/common/include/linux/ioprio.h \
  /root/android13-5.10/common/include/linux/sched/rt.h \
  /root/android13-5.10/common/include/linux/iocontext.h \
  /root/android13-5.10/common/include/linux/fs_types.h \
  /root/android13-5.10/common/include/uapi/linux/fs.h \
  /root/android13-5.10/common/include/linux/quota.h \
    $(wildcard include/config/quota/netlink/interface.h) \
  /root/android13-5.10/common/include/linux/percpu_counter.h \
  /root/android13-5.10/common/include/uapi/linux/dqblk_xfs.h \
  /root/android13-5.10/common/include/linux/dqblk_v1.h \
  /root/android13-5.10/common/include/linux/dqblk_v2.h \
  /root/android13-5.10/common/include/linux/dqblk_qtree.h \
  /root/android13-5.10/common/include/linux/projid.h \
  /root/android13-5.10/common/include/uapi/linux/quota.h \
  /root/android13-5.10/common/include/linux/nfs_fs_i.h \
  /root/android13-5.10/common/include/linux/kobject.h \
    $(wildcard include/config/uevent/helper.h) \
    $(wildcard include/config/debug/kobject/release.h) \
  /root/android13-5.10/common/include/linux/sysfs.h \
  /root/android13-5.10/common/include/linux/kernfs.h \
    $(wildcard include/config/kernfs.h) \
  /root/android13-5.10/common/include/linux/idr.h \
  /root/android13-5.10/common/include/linux/kobject_ns.h \
  /root/android13-5.10/common/include/linux/moduleparam.h \
    $(wildcard include/config/alpha.h) \
    $(wildcard include/config/ia64.h) \
    $(wildcard include/config/ppc64.h) \
  /root/android13-5.10/common/include/linux/rbtree_latch.h \
  /root/android13-5.10/common/include/linux/error-injection.h \
  /root/android13-5.10/common/include/asm-generic/error-injection.h \
  /root/android13-5.10/common/include/linux/tracepoint-defs.h \
  /root/android13-5.10/common/include/linux/static_key.h \
  /root/android13-5.10/common/include/linux/static_call_types.h \
    $(wildcard include/config/have/static/call.h) \
  /root/android13-5.10/common/include/linux/cfi.h \
    $(wildcard include/config/cfi/clang/shadow.h) \
  /root/android13-5.10/common/arch/arm64/include/asm/module.h \
    $(wildcard include/config/arm64/module/plts.h) \
    $(wildcard include/config/dynamic/ftrace.h) \
    $(wildcard include/config/arm64/erratum/843419.h) \
  /root/android13-5.10/common/include/asm-generic/module.h \
    $(wildcard include/config/have/mod/arch/specific.h) \
    $(wildcard include/config/modules/use/elf/rel.h) \
    $(wildcard include/config/modules/use/elf/rela.h) \
  /root/android13-5.10/common/include/linux/build-salt.h \
    $(wildcard include/config/build/salt.h) \
  /root/android13-5.10/common/include/linux/elfnote.h \
  /root/android13-5.10/common/include/linux/vermagic.h \
  include/generated/utsrelease.h \
  /root/android13-5.10/common/arch/arm64/include/asm/vermagic.h \

/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.mod.o: $(deps_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.mod.o)

$(deps_/mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.mod.o):
