# ARM64 Decoder 主机验证说明

## 验证边界

`arm64_decode_instruction()` 只对 32 位机器码执行普通 C 计算，不执行目标
指令，也不依赖 ARM64 寄存器、异常或内存副作用。生产 decoder 是跨平台代码，
因此直接在 Windows 主机的 WSL 工具链中编译和运行即可验证，不需要连接 Android
设备。

固定语料中的每条机器码同时经过生产 decoder、固定版本 LLVM 和独立字段算法。
这种验证同时检查解码结果、LLVM 重编码一致性及项目字段契约。

## 运行

从仓库根目录在 Windows PowerShell 中执行：

```powershell
& .\windows\run-arm64-decoder-test.ps1
```

也可以直接在 WSL 中执行：

```bash
make -C lsdriver/arm64_tests/decoder strict-test
```

两种入口运行同一套主机测试。编译警告、输入变化、行错位、未覆盖 instruction
或任一字段差异都会返回非零。

## 固定输入

```text
文件: ../instruction.txt
有效指令行数: 9362
unique word: 2938
SHA-256: 2EB84464AA498B65014CE0C115C9A7948450A4E1C5039ADEC818037A9015BBEC
```

## 验证层次

### 生产 decoder

在主机编译并运行真实的 `arm64_decode/*.c`。逐条读取原始机器码，要求全部返回
`ARM64_DECODE_OK`，并导出完整字段结果。

### LLVM 严格对照

固定使用 Android `clang-r487747c` / LLVM 17.0.2：

```text
triple: aarch64-linux-gnu
CPU: generic
features: +lse,+rcpc
source revision: d9f89f4d16663d5012e5c09495f3b30ece3d2362
```

每条指令必须满足：

- LLVM 解码成功并且恰好消费 4 字节；
- 不产生 fixup；
- MCCodeEmitter 重编码后逐字节等于输入；
- 项目 instruction 与 LLVM opcode 的 identity 映射无缺失、无未知项。

### 独立字段审计

`arm64_strict_decoder_audit.py` 根据 raw 编码独立计算字段并逐项检查生产 decoder，
包括 offset、immediate、位域掩码、移位/扩展、条件码、系统寄存器、SIMD/FP
布局、lane、寄存器字段、operand width，以及规范要求为零的未使用字段。

LLVM 公共 MC API 没有直接导出所有项目字段，所以无法由 LLVM 单独给出
`bitfield_wmask`、`bitfield_tmask` 等字段；这些字段由独立算法审计，不把生产
decoder 自身当作 oracle。

## 通过标准

必须同时满足：

- 生产 decoder 9362 条全部成功；
- LLVM 解码和逐字节 round-trip 9362 条全部成功；
- identity 映射无缺失、无未知项；
- 完整字段审计 `failures=0`；
- 没有未覆盖的 instruction 或字段。

2026-09-09 的主机验证结果：

```text
ARM64 instruction decoder: rows=9362 failures=0
LLVM AArch64 strict audit: rows=9362 failures=0
decoder_contract_checks=18724
decoder_contract_failures=0
llvm_field_checks=18982
llvm_field_failures=0
field_checks=215326 failures=0
total_failures=0
ARM64 decoder host validation passed
```

通过固定语料不等于形式化证明整个 AArch64 编码空间正确；它证明当前固定输入、
固定 LLVM 版本和声明字段范围内全部一致。