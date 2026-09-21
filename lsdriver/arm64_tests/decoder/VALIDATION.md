# ARM64 Decoder 验证

## 输入

`../instruction.txt` 是 decoder 与 executor 共用的固定语料。当前文件包含 5585 个物理行，
其中 5119 个非空记录、1844 个不同的 word，SHA-256 为：

```text
DC34125CC5138E4BE854997714824A24EAB349701B10D555BDD7A29D04C8D763
```

每个非空行必须是八位十六进制数。更新严格审计基线时需要同时核验输入元数据、
identity map 和字段检查覆盖；不得为了审计修改原始语料或补末尾换行。

## 统一契约回归

```bash
make -C lsdriver/arm64_tests/decoder contract-test
```

`arm64_decoder_contract_test.c` 验证非法编码拒绝、24 个 CAS/CASP 宽度与顺序组合，
并以固定随机种子对七个族入口进行 700000 次调用：失败时调用方输出保持不变，
成功时 instruction 不为 UNKNOWN。随机检查验证输出契约，不是编码合法性的独立 oracle。

2026-09-21 本轮验证结果：

```text
family contracts: samples=700000 success=29142 rejected=670858
decoder contracts: PASS; CAS/CASP variants=24
ARM64 instruction decoder: rows=5119 rejected=1 errors=0
host corpus TSV: identical to pre-refactor baseline
device: continuous test passed cases=5119
device: case_counts=compared=5118 skipped=1
device: runner_status=0 cleanup_status=0
```

本轮输入 SHA-256 为
`DC34125CC5138E4BE854997714824A24EAB349701B10D555BDD7A29D04C8D763`，原始字节未修改。
Android 14/Linux 6.1 executor 模块和 runner 已重建并完成真机测试；之后仅整理 C 文件格式，
整理后重新通过 host 严格编译、契约测试及 corpus 对比。

load/store 和 SIMD 使用已有 `arm64_decoded_instruction` 类型的局部结果，校验成功后
提交给调用方；未新增结构体或枚举定义。立即数、寄存器等格式先校验标量局部变量再写字段。

严格审计的行数校验按非空记录计数；物理行数包含空行。`instruction.txt` 不应为了计数
而修改或补末尾换行。

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
rows=5119
consistent_rejections=1
field_checks=117714 failures=0
llvm_failures=0
total_failures=0
ARM64 instruction.txt decoder/LLVM strict audit: PASS
```

测试全程只在主机解码机器码，不执行目标 ARM64 指令。
