# ARM64 Encoder 验证

## 运行

编码器测试只在主机编译和运行，不执行目标 ARM64 指令：

```bash
make -C lsdriver/arm64_tests/encoder contract-test
```

测试直接链接生产 [arm64_encode.c](../../arm64_encode/arm64_encode.c)，覆盖：

- 固定系统指令、BRK、BR/BLR/RET；
- B/BL、条件分支、CBZ/CBNZ、TBZ/TBNZ 的字段和范围；
- ADR/ADRP 的 `immlo/immhi`、页对齐和范围；
- ADD/SUB 立即数与 shifted-register 形式；
- GPR、FP/SIMD、LDRSW、索引和 pair 访存；
- literal load、PRFM、MRS/MSR；
- NOP 填充和绝对跳转序列；
- 空指针、未对齐地址、越界立即数、寄存器重叠及 scratch 寄存器边界。

固定机器字来自 GNU AArch64 汇编器对照结果；错误路径检查返回码，并确认合法路径的完整输出 word。
