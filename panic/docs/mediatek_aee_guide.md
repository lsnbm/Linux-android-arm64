# 联发科 AEE 解密与 Oplus 死机案例

本文补充 [Oplus 通用日志查询指南](oplus_crash_log_guide.md)，说明联发科 MediaTek / MTK 平台的 AEE 转储处理，以及本次 OnePlus PGZ110 / MT6895 故障的证据和修复。

联发科是本次转储的格式背景，不是死机根因。此次问题是驱动 hook work 在安装窗口内调用了尚未赋值的函数指针，不能据此认定为联发科、DRM 或当前任务本身的缺陷。

## 1. 适用范围

工具：[decode_mtk_aee.py](../scripts/decode_mtk_aee.py)。只读取原始包，提取和报告写入单独目录；不连接设备，不修改原 dump，也不自动下载工具。

| 功能 | 范围与限制 |
| --- | --- |
| `inspect` / `prepare` | 已验证本次 gzip/tar 内首个 AEE `.dbg` 的混淆头和 ZIP local-header 布局；不是任意 MTK AEE 的通用解析器 |
| `decrypt` | 调用 bkcrack，用当前包恢复的内部密钥解密，并逐成员校验 Deflate 完整性、原始长度和 CRC32 |
| `report` | 仅接受本次已验证样本的 SHA256；绑定该样本的 Linux 5.10 printk 布局、panic 和故障地址，其他包会明确拒绝 |

系统版本、设备型号和文件后缀不足以证明内部布局相同。新样本即使可提取、解密，也必须重新验证 printk 结构，不能删除报告的样本限制后直接套用偏移和故障地址。

原临时探测脚本中的猜口令分支未保留：它没有得到有效结果。这里使用已经成功验证的已知压缩明文恢复流程。

## 2. 依赖与命令

以下命令在仓库根目录的同一个 PowerShell 会话执行，需要 Python 3.8 或更新版本。

- 从 [bkcrack 官方发布页](https://github.com/kimci86/bkcrack/releases/tag/v1.8.1) 独立安装适合宿主系统的版本；本次验证使用 1.8.1 Windows x64。仓库不附带其二进制。
- `inspect`、`prepare`、`decrypt` 的 Python 部分仅使用标准库；只有 `report` 需要 Capstone：`python -m pip install capstone`。
- 选择与目标事件时间对应的原始 AEE 包。不要只按 dump 目录名或文件复制时间选择。

### 2.1 选择输入并检查成员

```powershell
$script = Join-Path (Get-Location) 'panic/scripts/decode_mtk_aee.py'
$archive = (Resolve-Path -LiteralPath (Read-Host '输入原始 AEE dat.gz 包路径')).Path
$bkcrack = (Resolve-Path -LiteralPath (Read-Host '输入 bkcrack 可执行文件路径')).Path
$work = Join-Path $env:TEMP ('mtk-aee-' + [guid]::NewGuid().ToString('N'))
$members = Join-Path $work 'members'
$recovery = Join-Path $work 'recovery.log'
$report = Join-Path $work 'report'

python $script --help
python $script inspect $archive
if ($LASTEXITCODE -ne 0) { throw 'AEE 格式检查失败' }
```

`inspect` 不写文件。输出 `GZIP_COMPLETE False` 表示 gzip 未读到结束，并不代表已恢复的每个成员都损坏。`TRUNCATED_MEMBER_AT` 表示后续成员不完整；只将前面完整成员交给后续校验，不宣称整个包已经恢复。

### 2.2 提取密文和已知明文候选

```powershell
python $script prepare $archive $members
if ($LASTEXITCODE -ne 0) { throw 'AEE 成员提取失败' }
if (-not (Test-Path -LiteralPath (Join-Path $members 'known.deflate'))) {
    throw '未找到匹配的全零 CCCI 压缩明文候选，不能直接使用下面的恢复命令'
}
```

生成成员密文、成员清单和候选压缩明文。已存在的目标目录会被拒绝，不覆盖旧结果。失败重试应重新选择 `$work`，不要把新旧包的成员混在同一个目录。

本次 `SYS_EXTRA_CCCI_RAW` 解压后为 32,768 个零字节，CRC32 为 `011ffca6`；raw Deflate 压缩候选为 157 字节，密文为 169 字节，差值是 12 字节的 ZipCrypto 加密头。脚本仅在长度和 CRC 满足条件时导出候选；仍须通过后续真实解密校验，不能把匹配候选本身当成成功解密。

### 2.3 恢复内部密钥

```powershell
& $bkcrack -c (Join-Path $members 'SYS_EXTRA_CCCI_RAW.cipher') `
    -p (Join-Path $members 'known.deflate') -j 4 |
    Tee-Object -Variable recoveryLines
$recoveryExitCode = $LASTEXITCODE
$recoveryLines | Set-Content -LiteralPath $recovery -Encoding utf8
if ($recoveryExitCode -ne 0) { throw 'bkcrack 密钥恢复失败' }
```

显式保存为 UTF-8，以兼容 Windows PowerShell 与 PowerShell 7。成功日志应包含一行三组八位十六进制内部密钥。它们不是原始口令，也不应跨转储复用；脚本会从该日志读取，而不是在源码中写死本次密钥。

### 2.4 解密并逐成员校验

```powershell
python $script decrypt $members $bkcrack $recovery
if ($LASTEXITCODE -ne 0) { throw '解密或成员校验失败' }
```

输出位于 `$work/decoded`，即成员目录的同级目录。每个文件必须同时通过 Deflate 结束状态、无尾随数据、原始长度和 CRC32 检查；全部成功后才输出 `ALL_COMPLETE_MEMBERS_VERIFIED`。失败时可能留有部分产物，不能把它们视为完整成功结果。

### 2.5 导出本次样本报告

仅对下文记录的原始包执行：

```powershell
python $script report $archive $members $report
if ($LASTEXITCODE -ne 0) { throw '样本报告导出失败' }
Get-ChildItem -LiteralPath $report
```

报告包含完整内核文本、panic 附近记录、当前任务可打印文本、故障指令反汇编和 JSON 校验清单。导出前会重新核对解密成员的长度与 CRC、原始包 SHA256，并交叉检查 pstore 与 printk 的故障位置。

## 3. 此样本的格式与截断处理

已验证的结构为 `gzip -> tar -> db.fatal.01.KE.dbg -> 混淆头 + ZipCrypto/Deflate 成员`：

- 仅前 256 字节需要用从 `raw[64:80]` 取得的 16 字节值循环 XOR 还原，不能对整个文件都做 XOR。
- 还原后按 ZIP local header 顺序读取完整成员，不依赖缺失的尾部中央目录；本样本 flags 为 `0x7`，method 为 `8`。
- 成员名按 `lneO` / `taZP` / `dxSA` 标记拆分，交织前三段后反转，恢复实际名称。
- 外层 gzip 未结束，内层 `.dbg` 声明 12,216,917 字节，实际可用 10,372,073 字节，尾部缺少 1,844,844 字节。
- 仍成功恢复 54 个完整成员，共 34,479,754 字节明文，全部通过原始长度与 CRC32 校验；这不等于截断部分也已恢复。

截断的发生环节未证实，不能仅凭文件大小归咎于收集器。两个 reserve/op2 副本相同也不代表包完整。

本次 printk 不是简单对二进制做 `strings`：从 `SYS_KERNEL_LOG_TP_RAW` 读取 head/tail 边界，解析 88 字节 info、24 字节 descriptor 和 data 环，筛选 committed/finalized 状态及有效 ID/位置范围，核对数据块 ID，按 `text_len` 取文本并按 sequence 排序。结果为 31,110 条有序记录，panic 窗口为 234 条。其他内核版本必须重新确认这些结构。

## 4. 本次故障证据

| 项目 | 已确认信息 |
| --- | --- |
| 设备 | OnePlus PGZ110 / MT6895，Android 15 ROM |
| 实际内核 | `5.10.226-android12-9-o-g18972ebc96a3`，不能用 ROM 的 Android 版本推断内核版本 |
| 包内事件时间 | 2026-11-26 16:58:18 CST |
| 摘要上报时间 | 2026-11-26 16:58:33 |
| 包文件名时间 | 2026-11-26 16:58:35 |
| 故障上下文 | CPU 5，PID 1224，`Comm=drmid_daemon` |
| 异常 | `ESR=0x86000005`，`EC=0x21`，当前 EL 的指令取指异常 |
| 寄存器 | `PC=0`、`X8=0`、`LR=0xffffffde41b4a63c` |

原始包位于本次 [dump 目录](../dump/OPLUS/oneplus%20ace/dump_2026_11_26_16_29) 的 `mnt/vendor/oplusreserve/media/log/minidump/`，文件名为：

```text
AEE_SYSTEM_LAST_KMSG@350e0f4234c84f931ffbe2edb86fcdc7@PGZ110_11.H.16_3160_202510141822@2026_11_26_16_58_35.dat.gz
```

原始包 SHA256：

```text
4387889dab626570b718ac17c83a13c88ec0ebd97cf375f4c28d69e9d35ac253
```

已保存的证据：[完整内核日志](../dump/OPLUS/oneplus%20ace/decoded_2026_11_26_16_58_35/kernel.log)、[故障窗口](../dump/OPLUS/oneplus%20ace/decoded_2026_11_26_16_58_35/panic.log)、[故障指令](../dump/OPLUS/oneplus%20ace/decoded_2026_11_26_16_58_35/fault-call.txt)、[校验清单](../dump/OPLUS/oneplus%20ace/decoded_2026_11_26_16_58_35/verification.json)。

现场指令直接说明空跳转发生在哪里：

```text
0xffffffde41b4a634: ldr x8, [x10, #0x200]
0xffffffde41b4a638: blr x8
0xffffffde41b4a63c: ldr x8, [sp, #0x10]
```

`LR - 4` 对应 `BLR X8`，而 `X8=0`，因此跳到空地址取指。不能只凭摘要中的 NULL 把它解释成普通数据指针解引用；`drmid_daemon` 只是触发该路径的当前任务，其创建者尚未确认。

安装日志与故障的内核时间顺序：

| 内核时间（秒） | 事件 |
| --- | --- |
| 5963.225446 | 首项 `proc_pid_readdir` hook 安装生效 |
| 5963.226881 | 并发任务命中 hook，发生空地址异常 |
| 5963.227397 | 安装线程仍在处理第二项 `proc_pid_lookup` |

结合 [hide_task.h](../../lsdriver/hide_task.h) 的调用形态和 [inline_hook_frame.h](../../lsdriver/inline_hook_frame.h) 的逐项安装行为，确定是首次安装时序窗口：首个入口已被修改，但整组 11 项安装尚未返回，`fn_proc_pid_readdir` 仍为空；旧 work 函数未判空便直接调用。判断来自运行时机器码、源码和时间线的交叉验证，不依赖缺失的旧随机符号映射，也不能归因于已停用的渲染初始化。

## 5. 已应用的修复

`proc_pid_readdir_hook_work` 先读取回放指针，再检查是否为空：

- 为空时返回 `0`，不修改寄存器或目录上下文，不做过滤，由框架恢复现场并回放原函数。
- 非空时设置本次目录上下文副本的过滤 actor，经回放指针调用原函数，同步目录位置和返回值，返回 `1`，防止框架重复执行原函数。
- 安装处保留普通函数指针赋值，work 判空处和安装处均补充了时序注释。避免本次死机的关键是调用前判空，不是给赋值增加 `WRITE_ONCE`。

已通过提取真实 work 函数的宿主定向 UBSan 检查：空指针分支不改现场，非空路径仅回放一次，并保持位置和返回值语义。尚未完成修复后内核模块重编和真机回归，不能将脚本解密验证当作设备修复验证。