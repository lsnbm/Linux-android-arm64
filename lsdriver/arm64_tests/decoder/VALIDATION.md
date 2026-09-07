# ARM64 Decoder 验证说明

## 验证范围

验证生产入口 `arm64_decode_instruction()`，检查固定语料中的：

- 返回状态、instruction class 和 instruction
- offset、immediate、位域掩码、移位/扩展、条件码、系统寄存器
- SIMD/FP 布局、lane、寄存器字段和 operand width
- 未使用字段是否保持零值

有限语料不能证明整个 AArch64 编码空间正确。本文中的“通过”仅适用于下面
声明的输入、工具链和字段范围。

## 运行方法

在 WSL 中运行：

```bash
make -C lsdriver/arm64_tests/decoder strict-test
```

编译使用 `-Wall -Wextra -Werror`。测试失败、编译警告、输入变化、行错位、
未覆盖 instruction 或字段差异都会返回非零。

固定输入基线：

```text
文件: ../instruction.txt
有效指令行数: 8339
unique word: 2423
SHA-256: 0351EFC1317351C13479F3B74ED015F4C7CB8897A22D357896282811F0C067B1
```

## 验证层次

### 生产 decoder

逐条读取原始机器码，要求全部返回 `ARM64_DECODE_OK`，并输出完整 TSV 结果。

### LLVM 独立验证

使用 Android `clang-r487747c` / LLVM 17.0.2，目标为：

```text
triple: aarch64-linux-gnu
CPU: generic
features: +lse,+rcpc
source revision: d9f89f4d16663d5012e5c09495f3b30ece3d2362
```

每条指令必须满足：

- LLVM 解码成功并消费 4 字节；
- 无 fixup；
- MCCodeEmitter 重编码后逐字节等于输入；
- 项目 instruction 与 LLVM opcode 的 alias 映射在 allowlist 中。

LLVM runner 还输出 `MCInst` 的 operand 类型和值，作为独立的 LLVM 原生证据。

### 独立字段校验

`arm64_strict_decoder_audit.py` 根据 raw 编码和独立算法重新计算字段，检查生产
decoder 的所有字段，包括规范要求为零的字段。这是独立交叉校验，不是 LLVM
字段 oracle。

## 当前限制

当前 Android LLVM 公共 MC API 没有导出完整的 AArch64 编码字段语义。尤其不能
直接从 `MCInst` 得到：

- `bitfield_wmask`、`bitfield_tmask`；
- 原始 `rd/rn/rm/ra/rt/rt2/rs` 字段身份；
- 部分 SIMD `element_width`、`lane_index`、`simd_cmode` 和 `operand_width`；
- 项目定义的 `instruction_class`。

因此当前结果可以证明固定语料中的 LLVM 解码、重编码和项目 decoder 的独立
字段计算一致，但不能宣称项目全部字段已经由 LLVM 权威逐字段验证。完成这一
要求需要带有 LLVM AArch64 生成解码表或专用字段导出 patch 的 LLVM oracle。

## 通过标准

必须同时满足：

- 生产 decoder 全部输入成功；
- LLVM 解码和 round-trip 全部成功；
- identity 映射无缺失、无未知项；
- 独立字段校验 `failures=0`；
- 没有未覆盖的 instruction 或字段。

通过固定语料不等于形式化证明整个 AArch64 指令空间正确。
