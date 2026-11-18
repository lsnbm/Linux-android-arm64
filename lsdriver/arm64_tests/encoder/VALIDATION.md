# ARM64 Encoder 验证

## 运行

编码器测试只在主机编译和运行，不执行目标 ARM64 指令：

```bash
make -C lsdriver/arm64_tests/encoder contract-test
```

默认 C 编译器为 `$(LLVM_TOOLCHAIN)/bin/clang`，与解码器测试使用同一套 Android
clang-r487747c / LLVM 17.0.2。`LLVM_TOOLCHAIN` 默认指向
`/root/6.1-Android14/prebuilts/clang/host/linux-x86/clang-r487747c`，可通过环境或 Make
命令行指定工具链位置；显式传入 `CC=...` 仍可覆盖 C 编译器。
构建产物位于临时目录，退出时由 trap 清理。

测试直接链接生产 [arm64_encode.c](../../arm64_encode/arm64_encode.c)，覆盖：

- 固定系统指令、BTI C/JC、BRK、BR/BLR/RET；
- B/BL、条件分支、CBZ/CBNZ、TBZ/TBNZ 的字段和范围；
- ADR/ADRP 的 `immlo/immhi`、页对齐和范围；
- ADD/SUB 立即数与 shifted-register 形式；
- GPR、FP/SIMD、LDRSW、索引和 pair 访存；
- literal load、PRFM、MRS/MSR；
- NOP 填充、绝对跳转和 BTI JC + 绝对跳转序列；
- BTI 长跳转的五 word 最小容量、尾部填充、缓冲区边界和失败时输出不变；
- 空指针、未对齐地址、越界立即数、寄存器重叠及 scratch 寄存器边界。

固定机器字来自 GNU AArch64 汇编器对照结果；错误路径检查返回码，并确认合法路径的完整输出 word。
