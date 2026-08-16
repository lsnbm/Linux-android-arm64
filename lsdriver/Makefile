

obj-m += lsdriver.o
lsdriver-y := lsdriver_main.o \
			  arm64_encode/arm64_encode.o \
			  arm64_decode/arm64_decode.o \
			  arm64_decode/arm64_decode_base.o \
			  arm64_decode/arm64_decode_ldst.o \
			  arm64_decode/arm64_decode_branch.o \
			  arm64_decode/arm64_decode_simd.o \
			  arm64_decode/arm64_decode_sve.o \
			  arm64_decode/arm64_decode_sme.o \
			  arm64_emulate/arm64_emulate.o \
			  arm64_emulate/arm64_emulate_branch.o \
			  arm64_emulate/arm64_emulate_ldst.o \
			  arm64_emulate/arm64_emulate_simd.o \
			  arm64_emulate/arm64_emulate_immediate.o \
			  arm64_emulate/arm64_emulate_register.o

$(obj)/lsdriver_main.o: $(src)/lsdriver.c FORCE
	$(call if_changed_dep,cc_o_c)

#O3优化:启用更积极的内联、循环变换、指令调度和常量传播。
ccflags-y += -O3

# Full LTO：让主驱动与 decoder/encoder 在统一模块视图中进行跨文件内联和常量传播。
ccflags-y += -flto


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
CFLAGS_REMOVE_arm64_emulate/arm64_emulate.o += -mgeneral-regs-only
CFLAGS_REMOVE_arm64_emulate/arm64_emulate_ldst.o += -mgeneral-regs-only
CFLAGS_REMOVE_arm64_emulate/arm64_emulate_simd.o += -mgeneral-regs-only
CFLAGS_REMOVE_arm64_emulate/arm64_emulate_immediate.o += -mgeneral-regs-only
CFLAGS_REMOVE_arm64_emulate/arm64_emulate_register.o += -mgeneral-regs-only

# 移除 -mgeneral-regs-only 后，仍禁止编译器自动生成或自动向量化为 FP/SIMD 指令；
# FP/SIMD 寄存器只允许由源码中可审计的显式内联汇编使用。
CFLAGS_lsdriver_main.o += -mno-implicit-float
CFLAGS_arm64_emulate/arm64_emulate.o += -mno-implicit-float
CFLAGS_arm64_emulate/arm64_emulate_ldst.o += -mno-implicit-float
CFLAGS_arm64_emulate/arm64_emulate_simd.o += -mno-implicit-float
CFLAGS_arm64_emulate/arm64_emulate_immediate.o += -mno-implicit-float
CFLAGS_arm64_emulate/arm64_emulate_register.o += -mno-implicit-float




# 将所有警告降级，不再视为错误。
ccflags-y += -Wno-error

# 驱动源码使用 C99 使用点声明与 for 初始化子句变量；旧内核默认 GNU89，需显式启用现代 GNU C 方言。
ccflags-y += -std=gnu11

# 关闭栈保护器：移除函数入口 canary 装载和返回前 canary 校验。
ccflags-y += -fno-stack-protector

# 省略帧指针：释放 x29 并减少函数入口和返回路径的压栈、出栈操作。
ccflags-y += -fomit-frame-pointer

# 尾调用优化：将函数末尾的 call + ret 合并为直接跳转，减少一次返回地址保存与恢复。
ccflags-y += -foptimize-sibling-calls

# 关闭 BTI 和返回地址指针认证：移除间接分支目标处的 BTI、函数入口 PACIASP 与返回前 AUTIASP。
ccflags-y += -mbranch-protection=none

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


