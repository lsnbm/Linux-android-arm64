

obj-m += lsdriver.o
lsdriver-y := lsdriver_main.o \
			  arm64_encode/arm64_encode.o \
			  arm64_decode/arm64_decode.o \
			  arm64_decode/arm64_decode_immediate.o \
			  arm64_decode/arm64_decode_register.o \
			  arm64_decode/arm64_decode_ldst.o \
			  arm64_decode/arm64_decode_branch.o \
			  arm64_decode/arm64_decode_simd.o \
			  arm64_decode/arm64_decode_sve.o \
			  arm64_decode/arm64_decode_sme.o \
			  arm64_emulate/emulate_inst.o \
			  arm64_emulate/arm64_emulate_branch.o \
			  arm64_emulate/arm64_emulate_ldst.o \
			  arm64_emulate/arm64_emulate_simd.o \
			  arm64_emulate/arm64_emulate_immediate.o \
			  arm64_emulate/arm64_emulate_register.o

$(obj)/lsdriver_main.o: $(src)/lsdriver.c FORCE
	$(call if_changed_dep,cc_o_c)

#O3优化:启用更积极的内联、循环变换、指令调度和常量传播。
ccflags-y += -O3

# 旧式 CFI 的 Linux ABI 随内核分支变化：5.10 使用 __cfi_slowpath，5.15 使用 __cfi_slowpath_diag；
# 部分厂商 KMI 不导出 CFI 失败调用的 UBSAN abort handler，例如5.15的 __ubsan_handle_cfi_check_fail_abort。
# 保留 cross-DSO CFI 和模块 __cfi_check，在模块内提供失败 handler，不改变正常 CFI 检查路径。
# 6.1+ 的 CC_FLAGS_CFI 为 -fsanitize=kcfi，不匹配此条件。
ifneq ($(filter -fsanitize=cfi,$(CC_FLAGS_CFI)),)
ccflags-y += -DLSDRIVER_LEGACY_CFI
endif

# 不为每个函数和数据对象生成独立 ELF section，减少模块加载时为 section 属性分配的内核元数据。
# 这条分配路径存在于各目标内核；6.6及以下版本内核，分配失败时仅跳过 section sysfs 属性，模块仍继续加载。
# 6.12 及以上由 mod_sysfs_setup() 传播 -ENOMEM，同样的高阶连续页分配失败会直接使 init_module() 失败。
# 外部模块由目标内核 Kbuild 编译，会继承 KBUILD_CFLAGS、Kconfig 派生选项和 module.lds；这里的kbuild只补充模块专属参数。
# 所以不在外部模块中强制启用 -flto；LTO 应由目标内核 Kconfig 决定，CONFIG_LTO_NONE 下单独启用会放大 split section 且与内核构建配置不一致。
ccflags-y += -fno-function-sections -fno-data-sections


# 开启 debug 输出；需要 pr_debug 时手动启用。
# ccflags-y += -DDEBUG

# 全模块统一启用模板显式汇编需要的 ARMv8.6、FP16、LSE、RCpc、CRC、
# Dot Product、FP16FML、FCMA、BF16 和 I8MM，让 Clang 集成汇编器识别对应助记符。
# 统一启用对应指令集后，汇编字符串无需再重复声明局部架构扩展(.arch_extension)。
# Clang 12 仍无法识别的更新指令继续在模板中使用机器码。
ccflags-y += -march=armv8.6-a+bf16+i8mm+fp16fml+fp16+lse+rcpc+crc

# 禁止优化器从普通 C 循环和基本块主动生成 Advanced SIMD 向量代码。
# 移除 -mgeneral-regs-only 的对象还会额外使用 -mno-implicit-float。
ccflags-y += -fno-vectorize -fno-slp-vectorize

# ARM64 Kbuild 默认添加 -mgeneral-regs-only，它也会禁止显式 FP/SIMD 内联汇编；
# 仅从显式使用 FP/SIMD 汇编的目标中移除此限制，不影响模块的其他目标文件。
CFLAGS_REMOVE_lsdriver_main.o += -mgeneral-regs-only
CFLAGS_REMOVE_arm64_emulate/emulate_inst.o += -mgeneral-regs-only
CFLAGS_REMOVE_arm64_emulate/arm64_emulate_ldst.o += -mgeneral-regs-only
CFLAGS_REMOVE_arm64_emulate/arm64_emulate_simd.o += -mgeneral-regs-only
CFLAGS_REMOVE_arm64_emulate/arm64_emulate_immediate.o += -mgeneral-regs-only
CFLAGS_REMOVE_arm64_emulate/arm64_emulate_register.o += -mgeneral-regs-only

# 移除 -mgeneral-regs-only 后，仍禁止编译器自动生成或自动向量化为 FP/SIMD 指令；
# FP/SIMD 寄存器只允许由源码中可审计的显式内联汇编使用。
CFLAGS_lsdriver_main.o += -mno-implicit-float
CFLAGS_arm64_emulate/emulate_inst.o += -mno-implicit-float
CFLAGS_arm64_emulate/arm64_emulate_ldst.o += -mno-implicit-float
CFLAGS_arm64_emulate/arm64_emulate_simd.o += -mno-implicit-float
CFLAGS_arm64_emulate/arm64_emulate_immediate.o += -mno-implicit-float
CFLAGS_arm64_emulate/arm64_emulate_register.o += -mno-implicit-float




# 将所有警告降级，不再视为错误。
ccflags-y += -Wno-error
ccflags-y += -Wno-unused-parameter

# 驱动源码使用 C99 使用点声明与 for 初始化子句变量；旧内核默认 GNU89，需显式启用现代 GNU C 方言。
ccflags-y += -std=gnu11

# 关闭栈保护器：移除函数入口 canary 装载和返回前 canary 校验。
ccflags-y += -fno-stack-protector

# 省略帧指针：释放 x29 并减少函数入口和返回路径的压栈、出栈操作。
ccflags-y += -fomit-frame-pointer

# 尾调用优化：将函数末尾的 call + ret 合并为直接跳转，减少一次返回地址保存与恢复。
ccflags-y += -foptimize-sibling-calls

# 关闭 BTI ：移除间接分支目标处的 BTI、。
# 这个不要进行关闭，讲一下原因
# 真正的 C 函数 init_module;sub sp, sp, #0xd0;也就是驱动的c入口正常，没有BTI/PAC
# 由于目标内核启用了 CFI/LTO，模块的 init 指针被规范化到 CFI jump table：init_module.cfi_jt: b init_module;
# 由于编译配置显式使用 -mbranch-protection=none 关闭了 ARM64 BTI/PAC 保护，
# 生成的 init_module.cfi_jt 跳板入口只有 b init_module 而缺少内核间接调用要求的 bti c
# 导致内核通过 BLR 加载驱动时触发同步终止异常(BTI) 并最终 kernel panic。
# -mbranch-protection=bti       PAC 关闭，BTI 开启
# -mbranch-protection=pac-ret   PAC 开启，BTI 关闭
# -mbranch-protection=standard  PAC 和 BTI 都开启
# -mbranch-protection=none      PAC 和 BTI 都关闭

# 仅关闭返回地址 PAC，保留 BTI；覆盖 Kbuild 继承的 pac-ret 分支保护模式。
# 原因是部分设备如oppo find x9 pro使用漏洞解锁root权限
# 不知道是那个函数的返回地址、栈指针或栈内容被漏洞修改
# 然后autiasp 验证失败，LR 会变成无效地址，随后 ret 跳转异常，导致 kernel panic, panic原因变成普通的空指针访问
ccflags-y += -mbranch-protection=bti


# 关闭自动局部变量清零：避免编译器在函数入口为未初始化栈对象插入 memset/零存储。
# 代码必须自行保证所有局部变量在首次读取前已完成初始化，否则会暴露真实的未初始化读取。
ccflags-y += -ftrivial-auto-var-init=uninitialized

# 循环展开：用代码体积换取更少的循环回边、比较和分支指令。
ccflags-y += -funroll-loops

# 严格别名优化：允许编译器基于类型别名规则进行更积极的加载消除和指令重排。
ccflags-y += -fstrict-aliasing

# 关闭 KASAN 编译插桩：避免内存访问周围生成地址检查。
KASAN_SANITIZE := n

# 关闭 UBSAN 编译插桩：避免算术、数组边界等操作生成运行时检查。
UBSAN_SANITIZE := n

# 关闭 KCSAN 编译插桩：避免并发访问周围生成采样和竞争检查。
KCSAN_SANITIZE := n

# 关闭 KMSAN 编译插桩：避免值传播路径生成未初始化状态跟踪。
KMSAN_SANITIZE := n

# 关闭 GCOV 覆盖率插桩：避免基本块计数器更新。
GCOV_PROFILE := n

# 关闭 KCOV 覆盖率插桩：避免边覆盖记录和回调。
KCOV_INSTRUMENT := n

# 保留 KCFI：启用 KCFI 的内核会间接调用模块回调，移除类型哈希会导致校验异常。
# 禁止关闭整个模块的 CFI/KCFI：5.x 内核通过模块 __cfi_check 校验回调，6.1+ 内核通过 KCFI type id 校验回调；


# 删除空指针检查：覆盖内核默认的保守语义，可能破坏依赖空指针检查的内核代码，禁止启用。
# ccflags-y += -fdelete-null-pointer-checks

# 严格有符号溢出：允许编译器假设有符号溢出永不发生，与内核默认语义冲突，禁止启用。
# ccflags-y += -fstrict-overflow

# 强制内联：-O3 已默认启用同类内联策略，单独添加通常不会继续提高内联强度，暂不启用。
# ccflags-y += -finline-functions

# 极高内联阈值：实测提高 LTO 内联阈值会明显放大代码且未消除目标热路径调用，暂不启用。
# ccflags-y += -mllvm -inline-threshold=10000

# 极高循环展开阈值：会绕过正常成本模型并显著增加 I-cache 压力，暂不启用。
# ccflags-y += -mllvm -unroll-threshold=10000



# 隐藏符号可见性：实测未改善运行时代码，并会把 init_module/cleanup_module 标记为 HIDDEN，暂不启用。
# ccflags-y += -fvisibility=hidden

# 禁用 PLT：内核模块调用由重定位直接处理，当前目标未观察到运行时收益，暂不启用。
# ccflags-y += -fno-plt


# 禁用跳转表：会把 switch 降为比较分支链，通常增加分派开销，暂不启用。
# ccflags-y += -fno-jump-tables

# 关闭影子调用栈：当前外部模块实际编译命令没有 shadow-call-stack 插桩，添加后不会产生收益。
# ccflags-y += -fno-sanitize=shadow-call-stack

# 关闭 KCFI：内核会通过间接调用进入模块回调，移除模块类型哈希可能触发运行时 CFI 异常，禁止启用。
# ccflags-y += -fno-sanitize=kcfi

# 移除 Ftrace：需先在实际编译命令中确认存在 Ftrace 插桩，再按具体目标文件移除，当前暂不启用。
# CFLAGS_REMOVE_lsdriver.o += $(CC_FLAGS_FTRACE)

# Cortex-X2 调度模型：按 Cortex-X2 的流水线延迟和吞吐特征安排指令，不扩大 -march 允许使用的指令集。
# 该模块固定用于 Cortex-X2 或调度特征相近的核心；若更换目标 CPU，需要同步修改此值并重新验证机器码性能。
#ccflags-y += -mtune=cortex-x2


