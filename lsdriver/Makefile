

obj-m += lsdriver.o


# 注释这行为release编译，不会有pr_debug输出
#ccflags-y += -DDEBUG

#O3
ccflags-y += -O3

# 将所有警告降级，不再视为错误
ccflags-y += -Wno-error

#禁用栈溢出保护
ccflags-y += -fno-stack-protector

# 废弃帧指针,释放 FP (x29) 寄存器作为通用计算寄存器，减少压栈动作
ccflags-y += -fomit-frame-pointer

# 循环展开: 牺牲二进制体积，减少 while/for 循环的跳转和判断开销
ccflags-y += -funroll-loops



# 严格别名优化: 允许编译器假设不同类型的指针指向不同内存，从而进行更激进的指令重排
ccflags-y += -fstrict-aliasing



# 死代码消除辅助: 将每个函数和数据放入独立段，配合链接器剔除无用代码
ccflags-y += -ffunction-sections -fdata-sections





#下面编译配置如果使用会出现意外，请注释!!!


# # 强制内联: 消除函数调用 (call/ret) 和参数压栈的开销
# ccflags-y += -finline-functions


# # 彻底关闭针对该模块的内存杀毒软件 (KASAN) 和未定义行为检查 (UBSAN)
# KASAN_SANITIZE := n
# UBSAN_SANITIZE := n
# KCSAN_SANITIZE := n

# # 关闭针对该模块的代码覆盖率统计和性能追踪
# GCOV_PROFILE := n
# KCOV_INSTRUMENT := n

# # 剔除 Ftrace (函数调用追踪) 插桩
# CFLAGS_REMOVE_lsdriver.o += $(CC_FLAGS_FTRACE)

# # 强制 64 字节对齐 (适配 ARM64 的 Cache Line)用 NOP 指令填充空白，确保 CPU 抓取关键循环时绝不跨越缓存行，零等待
# ccflags-y += -falign-functions=64
# ccflags-y += -falign-loops=64


# # 暴力循环展开与内联 (Clang 参数)
# ccflags-y += -funroll-loops
# ccflags-y += -finline-functions
# # 强行拉高 LLVM 的内联和展开阈值 (让函数扁平化)
# ccflags-y += -mllvm -inline-threshold=10000
# ccflags-y += -mllvm -unroll-threshold=10000



# # 关闭影子调用栈 
# # 原理：现代安卓内核默认开启 SCS。每次函数调用都会额外操作 x18 寄存器去备份返回地址。
# # 关闭后：每个函数砍掉 2 条内存操作指令，释放 x18 寄存器，极大降低函数调用开销。
# ccflags-y += -fno-sanitize=shadow-call-stack

# # 关闭指针身份验证与分支目标识别
# # 原理：ARMv8.3+ 以上默认开启。函数开头会有 paciasp/bti c，结尾有 autiasp，用于防 ROP 攻击。
# # 关闭后：剥离所有密码学签名校验指令，函数真正做到“纯逻辑直通”。
# ccflags-y += -mbranch-protection=none

# # 关闭幽灵漏洞的跳转表防御 
# # 原理：内核为了防 Spectre V2，会把普通的汇编 jmp/call 变成极其极其极其低效的 thunk 蹦床跳。
# # 关闭后：恢复 CPU 原生的间接跳转分支预测，switch-case 和函数指针调用速度起飞。
# ccflags-y += -fno-jump-tables


# #  强制隐藏内部符号可见性
# # 原理：内核模块对外暴露 API 依赖 EXPORT_SYMBOL 宏，不需要标准的 ELF 全局符号可见性。
# # 开启后：编译器确信所有没加 EXPORT_SYMBOL 的函数全是你内部私有的，会进行极其暴力的过程间优化 (IPO) 和寄存器传参优化。
# ccflags-y += -fvisibility=hidden

# # 禁用 PLT 表调用
# # 原理：彻底不生成过程链接表。
# # 开启后：所有外部函数调用直接用相对地址/绝对地址硬编码计算，减少一次内存中转。
# ccflags-y += -fno-plt



# 后面不写了，有多个 .c 文件可以使用内部 LTO


