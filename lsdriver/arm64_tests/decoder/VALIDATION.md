# ARM64 Decoder 验证

## 输入

`../instruction.txt` 是 decoder 与 executor 共用的固定语料。当前输入包含 2047 行、
922 个不同的 word，SHA-256 为：

```text
98D6AA19F20B20BCB700F7087B682B28120A76122C1BCD9DAB1F4C296BF4926D
```

每个非空行必须是八位十六进制数。语料变化时同步更新 `Makefile` 中的行数、
unique word 数和 SHA-256。

## 构建与运行

在仓库根目录执行：

```bash
make -C lsdriver/arm64_tests/decoder strict-test
```

测试依次构建并运行：

1. `arm64_instruction_decoder.c` 调用生产 `arm64_decode/*.c`，输出逐项解码状态和字段；
2. `arm64_llvm_strict_audit.cpp` 使用 Android clang-r487747c / LLVM 17.0.2 解码、
	重编码并输出 opcode、operand 和 immediate；
3. `arm64_strict_decoder_audit.py` 对齐两份 TSV，检查 identity 映射，并根据原始编码
	独立计算生产 decoder 字段。

## 判定

生产 decoder 与 LLVM 均成功的项必须满足：

- LLVM 恰好消费四字节、无 fixup，重编码逐字节等于输入；
- instruction 与 LLVM opcode 的组合存在于 `arm64_llvm_identity.tsv`；
- instruction class 和所有适用字段与独立计算结果一致；
- 未由该 instruction 使用的字段为零。

生产 decoder 返回 `ARM64_DECODE_UNALLOCATED` 且 LLVM 返回 `fail` 时，该项记为
一致拒绝，不执行字段审计。其他成功/失败组合均为错误。当前结果为：

```text
rows=2047
consistent_rejections=1
field_checks=47058 failures=0
llvm_failures=0
total_failures=0
ARM64 instruction.txt decoder/LLVM strict audit: PASS
```

测试全程只在主机解码机器码，不执行目标 ARM64 指令。
