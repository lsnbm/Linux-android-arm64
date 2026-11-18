# ARM64 Decoder 验证

## 输入

`../instruction.txt` 是 decoder 与 executor 共用的固定语料。当前文件包含 6704 个物理行，
其中 6143 个非空记录、2491 个不同的 word。严格测试接受以下两种换行形式的精确 SHA-256：

```text
LF:   4369185D869CD3E4D6B8C9A968DF1BC9E753B7607E4D39BDDD152972D661F1E2
CRLF: C2196291220CFBABFD5D7EC95E550D9CDA216075A968A084178DA4E951C1CC52
```

两种形式的机器字、空行及末尾换行一致，仅行结束符不同；测试不改写输入，
也不通过去除空白或自动生成期望哈希来放宽内容校验。

每个非空行必须是八位十六进制数。更新严格审计基线时需要同时核验输入元数据、
identity map 和字段检查覆盖；不得为了审计修改原始语料或补末尾换行。

## 统一契约回归

```bash
make -C lsdriver/arm64_tests/decoder contract-test
```

`arm64_decoder_contract_test.c` 验证非法编码拒绝、24 个 CAS/CASP 宽度与顺序组合、
3 个 LDAPR/LDAPRB/LDAPRH RCpc 编码，
并以固定随机种子对七个族入口进行 700000 次调用：族入口失败时不修改传入对象，
成功时 instruction 不为 UNKNOWN。公共 `arm64_decode_instruction` 只有返回 `OK` 时结果有效；
其他状态均返回整体清零的无效对象。随机检查验证输出契约，不是编码合法性的独立 oracle。

2026-09-21 历史验证结果（对应当时的语料）：

```text
family contracts: samples=700000 success=29142 rejected=670858
decoder contracts: PASS; CAS/CASP variants=24
ARM64 instruction decoder: rows=5119 rejected=1 errors=0
host corpus TSV: identical to pre-refactor baseline
device: continuous test passed cases=5119
device: case_counts=compared=5118 skipped=1
device: runner_status=0 cleanup_status=0
```

该次输入 SHA-256 为
`DC34125CC5138E4BE854997714824A24EAB349701B10D555BDD7A29D04C8D763`，原始字节未修改。
Android 14/Linux 6.1 executor 模块和 runner 已重建并完成真机测试；之后仅整理 C 文件格式，
整理后重新通过 host 严格编译、契约测试及 corpus 对比。

load/store 和 SIMD 使用已有 `arm64_decoded_instruction` 类型的局部结果，校验成功后
提交给调用方；未新增结构体或枚举定义。立即数、寄存器等格式先校验标量局部变量再写字段。

严格审计的行数校验按非空记录计数；物理行数包含空行。`instruction.txt` 不应为了计数
而修改或补末尾换行。

## LLVM 穷举审计

`strict-test` 还会编译并运行 `arm64_llvm_exhaustive_audit.cpp`。它遍历
branch、immediate、register 和 SIMD/FP 的主编码组控制字段，并用 LLVM MCDisassembler
与生产 decoder 同时判断编码是否可达。

- 生产 decoder 成功、LLVM 明确失败的未知编码会使测试失败；这是解码器误接受。
- LLVM 成功、生产 decoder 拒绝的编码只作为覆盖报告；它们可能属于当前尚未实现的指令。
- LLVM 17 明确不支持的 FAMAX/FAMIN/FSCALE 编码单独统计，不能据此删除 ARM 指令分支。

该测试使用固定的四组寄存器字段组合覆盖控制字段空间，不声称穷举全部 $2^{32}$ 个 raw；
寄存器字段和指令语义字段仍由固定语料 strict audit 及字段契约测试覆盖。

2026-10-10 历史集成结果：

```text
total=8388608 local_accepted=3157770 llvm_accepted=3251934 unknown_local_only=0
group=5 total=1048576 local_only=0 llvm_only=0
group=7 total=1048576 local_only=1920 llvm_only=65128
group=8 total=1048576 local_only=0 llvm_only=0
group=9 total=1048576 local_only=0 llvm_only=0
group=a total=1048576 local_only=0 llvm_only=12278
group=b total=1048576 local_only=0 llvm_only=20
group=d total=1048576 local_only=0 llvm_only=362
group=f total=1048576 local_only=0 llvm_only=18296
known_llvm17_gap instruction=256 count=640
known_llvm17_gap instruction=335 count=640
known_llvm17_gap instruction=340 count=640
```

其中 instruction 256/335/340 分别对应 `FAMAX_VECTOR`、`FAMIN_VECTOR`、
`FSCALE_VECTOR`。LLVM 17 不识别这些较新的 SIMD 扩展，因此只统计为已知 gap，
不把它们当作 decoder 误接受。

## 构建与运行

在仓库根目录执行：

```bash
make -C lsdriver/arm64_tests/decoder contract-test strict-test
```

默认 C 编译器为 `$(LLVM_TOOLCHAIN)/bin/clang`，C++ 审计工具使用同目录的 `clang++`。
`LLVM_TOOLCHAIN` 默认指向
`/root/6.1-Android14/prebuilts/clang/host/linux-x86/clang-r487747c`，可通过环境或 Make
命令行指定工具链位置。严格审计仍要求 Android LLVM 17.0.2、r487747c 和源码版本
`d9f89f4d16663d5012e5c09495f3b30ece3d2362`；显式传入 `CC=...` 仍可覆盖 C 编译器。

测试依次构建并运行：

1. `arm64_instruction_decoder.c` 调用生产 `arm64_decode/*.c`，输出逐项解码状态和字段；
2. `arm64_llvm_strict_audit.cpp` 使用 Android clang-r487747c / LLVM 17.0.2 解码、
	重编码并输出 opcode、operand 和 immediate；
3. `arm64_llvm_exhaustive_audit.cpp` 对照 LLVM 遍历 8388608 个控制字段样本；
4. `arm64_strict_decoder_audit.py` 对齐两份 TSV，检查 identity 映射，并根据原始编码
	独立计算生产 decoder 字段。

## 判定

生产 decoder 与 LLVM 均成功的项必须满足：

- LLVM 恰好消费四字节、无 fixup，重编码逐字节等于输入；
- instruction 与 LLVM opcode 的组合存在于 `arm64_llvm_identity.tsv`；
- instruction class 和所有适用字段与独立计算结果一致；
- 未由该 instruction 使用的字段为零。

生产 decoder 返回 `ARM64_DECODE_UNALLOCATED` 且 LLVM 返回 `fail` 时，该项记为
一致拒绝，不执行字段审计。其他成功/失败组合均为错误。当前基线使用默认 Android Clang
通过契约测试及严格审计，结果为：

```text
family contracts: samples=700000 success=29173 rejected=670827
decoder contracts: PASS; CAS/CASP variants=24
rows=6143
decoder_contract_checks=12310
decoder_contract_failures=0
consistent_rejections=1
llvm_identity_pairs=175 allowed=242
llvm_field_checks=13167
llvm_field_failures=0
field_checks=141266 failures=0
llvm_failures=0
total_failures=0
ARM64 instruction.txt decoder/LLVM strict audit: PASS
total=8388608 local_accepted=3157770 llvm_accepted=3251934 unknown_local_only=0
ARM64 decoder LLVM exhaustive audit: PASS
```

测试全程只在主机解码机器码，不执行目标 ARM64 指令；当前基线未重新进行真机验证。
构建目录和 TSV 位于临时目录，退出时由 trap 清理。
