# Oplus 设备死机日志定位与分析指南

本文只说明 Oplus/OnePlus 设备。其他厂商的 dump 目录和日志格式请参考对应设备文档。

适用对象：项目中使用 Oplus reserve 日志目录的 OnePlus、OPLUS 及同类 Oplus Android 设备。

## 1. 新 dump 的目录入口

项目中的 Oplus dump 通常位于：

```text
dump/OPLUS/<device>/<dump_name>/
```

Oplus 收集器从设备复制的主要路径见：

```text
jni/collectors/oplus_collector.h
```

设备上的原始路径主要映射为：

```text
/mnt/vendor/oplusreserve/
/mnt/oplus/op2/
```

因此 dump 中经常会同时出现两套内容：

```text
<dump_root>/mnt/vendor/oplusreserve/
<dump_root>/mnt/oplus/op2/
```

2个路径的三个压缩包内部的 olc_get_log.txt 都明确记录：
oplus_log_olc.sh get_minidump_log
cp -rf /data/persist_log/DCS/minidump/

因此，它们是 OPLUS 从 /data/persist_log/DCS/minidump/ 收集后生成的三个历史快照，再导出到当前目录。

3个minidump文件修改时间是统一导出时间。不是实际死机发生时间；

## 2. Oplus 死机日志优先级

拿到新 dump 后，按下面的优先级寻找文件：

### 2.1 `critical.log`：先定位事件时间

首选文件：

```text
mnt/vendor/oplusreserve/media/log/criticalLog/persist/critical.log
```

可能的重复路径：

```text
mnt/oplus/op2/media/log/criticalLog/persist/critical.log
```

`critical.log` 通常是累计文件，包含多次启动和多次历史故障。它适合先回答：

- 发生过哪些类型的异常；
- 最新一条事件的 `log-time` 是什么；
- 最新事件是内核 panic、Android crash、watchdog 还是普通状态记录。

它不适合直接作为完整内核调用栈。必须继续关联对应的 minidump 或 boot log。

### 2.2 `SYSTEM_LAST_KMSG*.dat.gz` / `AEE_SYSTEM_LAST_KMSG*.dat.gz`：确认内核首现场

主要目录：

```text
mnt/vendor/oplusreserve/media/log/minidump/
```

# Oplus 新设备死机日志查询指南

本文只回答：拿到一份新的 Oplus dump 后，去哪里找日志、怎样对应故障时间、接下来打开哪个文件。不预设具体机型、芯片平台、故障模块或死机原因。

不同系统版本的目录、文件名和转储格式可能不同。下面是查询入口，不是每台设备都必须具备的文件清单。

## 1. 先枚举这一份 dump

项目内目录一般为 `dump/OPLUS/<device>/<dump_name>/`，也可以是其他位置。后续 PowerShell 命令在同一会话中执行，需要安装 `rg`；只查询所选 dump，不混搜其他设备或其他采集批次。

```powershell
$dump = Read-Host '输入新 dump 的根目录'
if (-not (Test-Path -LiteralPath $dump -PathType Container)) {
    throw 'dump 目录不存在'
}
Get-ChildItem -LiteralPath $dump -Force
$files = @(rg --files --hidden --no-ignore -- $dump)
$files | Where-Object {
    $_ -match 'critical\.log|LAST_KMSG|last_kmsg|recovery_dmesg|minidump|ramdump|boot_|pstore|ramoops|tombstone|dropbox|crash|logcat|hang|shutdown|last_init_boot_failed|merge_kernel_log'
}
```

先只列文件路径，不对整个 dump 做内容搜索。压缩包、二进制和不同启动的文本混搜，容易把无关内容当成此次死机。

## 2. 常见目录与用途

优先查看以下两个入口，哪个存在就从哪个开始；都没有时，以第 1 节的文件清单为准：

```text
<dump_root>/mnt/vendor/oplusreserve/
<dump_root>/mnt/oplus/op2/
```

以下路径相对于上述入口：

| 相对路径或文件 | 用来查什么 |
| --- | --- |
| `media/log/criticalLog/persist/critical.log` | 累计事件时间和异常摘要 |
| `media/log/criticalLog/cache/` | 其他事件记录，实际格式需检查 |
| `media/log/minidump/` | 内核现场或其他转储包 |
| `media/log/boot_log/` | 各次启动日志 |
| `recovery/last_kmsg*`、`tmp_log/recovery_dmesg_log_*` | 历史内核或 recovery 日志 |
| `media/dropbox/` | 用户态或系统异常报告 |
| `media/log/hang_oplus/`、`media/log/shutdown/` | 卡死、关机相关记录 |
| `rbr_log/last_init_boot_failed.log` | 启动失败线索 |
| `update_engine_log/` | 更新过程日志，仅在时间和现象相关时查看 |

两个挂载入口可能有重复副本。文件名相同并不保证内容相同；核对大小和内容，必要时比对哈希，确认重复后只计一次。

项目的 [Oplus 收集器](../jni/collectors/oplus_collector.h) 复制已有路径，不保证设备生成了所有日志。[通用收集器](../jni/collectors/common_collector.h) 收集的 init 配置用于查服务定义，不是死机现场。未导出的文件不能视为设备上从未存在。

## 3. 用事件摘要确定查询时间

先列出所有 `critical.log`，再选择一份读取；不要在入口缺失时仍固定使用某个路径：

```powershell
$criticalCandidates = @($files | Where-Object {
    [IO.Path]::GetFileName($_) -ieq 'critical.log'
})
$criticalCandidates
```

如果找到了候选文件：

```powershell
$critical = Read-Host '输入选定 critical.log 的完整路径'
Get-Content -LiteralPath $critical -Tail 160
rg -n -i -C 5 -- 'kernel_panic|<android-crash-begin>|system_restart_|watchdog|reboot|reset' $critical
```

尾部只是定位线索，必要时回看完整事件块。每个候选事件应记录：

- `log-time`：事件记录时间，可能是重启后上报时间，不一定是故障发生的精确时刻；
- 事件类别：内核异常、用户态异常、卡死或重启等；
- 摘要内容以及所在文件、行号；
- 相邻事件顺序和版本信息。

最新一行不一定是最新故障，充电或网络状态也会追加到累计日志。查询指定故障时以用户给出的时间段为起点；查询最新故障时找最新的可信异常事件。

不要把目录名、文件复制时间当成事件时间。遇到设备未校时、时间倒退或时区不一致，应结合启动顺序和日志内容核对，不机械选数值最大的日期。`FST_FRAME` 等字段是摘要，不是完整调用栈。

## 4. 按事件时间关联转储文件

在文件清单中找到 `SYSTEM_LAST_KMSG*`、联发科常见的 `AEE_SYSTEM_LAST_KMSG*` 等候选。某些文件使用下面的命名格式：

```text
SYSTEM_LAST_KMSG@<id>@<build>@YYYY_MM_DD_HH_MM_SS.dat.gz
AEE_SYSTEM_LAST_KMSG@<id>@<build>@YYYY_MM_DD_HH_MM_SS.dat.gz
```

对于这种格式，应提取末尾时间，而不是按完整文件名排序；前面的标识会干扰排序。下面列出时间候选，无法识别时间的文件也保留：

```powershell
$minidumps = @(
    foreach ($file in $files) {
        $name = [IO.Path]::GetFileName($file)
        if ($name -notmatch '^(?:AEE_)?SYSTEM_LAST_KMSG') { continue }
        $time = $null
        if ($name -match '@(?<stamp>\d{4}(?:_\d{2}){5})\.dat\.gz$') {
            $parsedTime = [datetime]::MinValue
            if ([datetime]::TryParseExact(
                $Matches.stamp, 'yyyy_MM_dd_HH_mm_ss',
                [Globalization.CultureInfo]::InvariantCulture,
                [Globalization.DateTimeStyles]::None, [ref]$parsedTime)) {
                $time = $parsedTime
            }
        }
        [pscustomobject]@{ RecordedTime = $time; Path = $file }
    }
)
$minidumps | Sort-Object RecordedTime -Descending |
    Format-Table RecordedTime, Path -AutoSize -Wrap
```

文件名时间用于筛选，不自动证明它就是目标故障。选取与事件记录相近的候选，再核对包内时间、版本和异常内容。若命名不同，回到第 1 节按转储目录和文件类型找，不把未知命名的文件丢弃。

## 5. 先识别格式，再打开日志

对已选定、疑似 gzip/tar 的归档，先尝试列出成员：

```powershell
$archive = Read-Host '输入已按时间核对的归档完整路径'
$members = @(tar -tzf $archive)
if ($LASTEXITCODE -ne 0) {
    throw '无法按 gzip/tar 列出，请先用归档工具识别格式'
}
$members
```

`tar` 不可用时，可用 7-Zip 查看格式和成员。列出失败可能是格式不同、文件不完整或受保护，不能直接解释为“没有日志”。扩展名相同也不保证内部格式相同。

从实际成员列表中选择文本日志，核对内容后再搜索。采集说明、设备属性、日志索引不等于内核日志，不能默认某个固定成员名包含 panic。

```powershell
$member = Read-Host '输入列表中已确认的文本日志成员名'
tar -xOzf $archive -- $member
```

此命令只用于文本成员。需要解包时使用独立分析目录，保留原始包，不覆盖原 dump。不要通过 PowerShell 文本重定向提取二进制现场，以免改变原始字节。

如果包内只有二进制现场，应记录完整包路径、设备平台及系统版本，再用匹配格式的解析工具导出内核文本。普通解包工具不一定能解析内部现场；不能因文本搜索没有命中就判定没有崩溃。本指南不假设统一的二进制布局。

### 联发科 AEE 分支

在联发科设备上，`AEE_SYSTEM_LAST_KMSG*.dat.gz` 外层可能仍是 gzip/tar，但内部 `.dbg` 并非直接可读的内核日志；部分布局包含混淆头、加密压缩成员和 printk 环形缓冲区。先记录实际成员和完整性，再按 [联发科 AEE 专题](mediatek_aee_guide.md) 使用 [解密脚本](../scripts/decode_mtk_aee.py)。

本次 MT6895 包虽然尾部截断，仍有 54 个完整成员通过长度和 CRC 校验并恢复出内核现场；

## 6. 确认找到的是哪类日志

### 内核日志

对已解包或解析导出的文本内核日志搜索，不直接搜索压缩文件：

```powershell
$kernelLog = Read-Host '输入文本内核日志的完整路径'
rg -n -i -C 25 -- 'Kernel panic|Internal error|Oops|BUG:|Unable to handle|watchdog|lockup|Call trace:' $kernelLog
```

命中后保留同一次启动内的前后文，包括内核时间戳、异常原文、CPU、PID、线程名和调用栈。内核运行时秒数不能直接与日历时间排序；需先确认属于哪次启动。

未命中时检查是否取错启动、缓冲区被覆盖、文本不完整，或故障没有留下内核异常；不要从旧转储中挑一条异常作为替代。

### 用户态或卡死记录

如果摘要指向用户态异常或卡死，继续从同一份 dump 的文件清单查找：

```powershell
$files | Where-Object {
    $_ -match 'tombstone|dropbox|crash|debuggerd|logcat|hang|anr'
}
```

按时间、进程名和 PID/TID 对应到报告正文。摘要不是完整报告，用户态崩溃也不自动等于内核死机。

### 启动和 recovery 日志

```powershell
$files | Where-Object {
    $_ -match 'boot_.*\.tar\.gz$|last_kmsg|recovery_dmesg|last_init_boot_failed|shutdown'
}
```

启动包仍按第 5 节先列成员，再读取实际文本。用包内启动时间和重启原因核对是否属于目标事件前后；不要按启动序号的字符串顺序自动选取。

`last_kmsg` 的后缀和文件名不保证它对应最近一次故障。recovery 日志也可能属于维修、升级或更早启动，只能在时间关联成立后使用。

## 7. 文件缺失时怎么办

| 当前只找到什么 | 下一步 |
| --- | --- |
| 没有预期的挂载目录 | 在该 dump 根目录枚举同类文件，不套用固定路径 |
| 没有事件摘要，但有内核现场 | 从现场内容和启动日志重建时间对应关系 |
| 只有事件摘要 | 记录异常时间和摘要，继续找转储、文本内核日志或用户态报告 |
| 有归档，但只有二进制现场 | 保留原包，确认格式与匹配的解析工具 |
| 联发科 AEE 包受保护或尾部截断 | 先确认 AEE 布局，参考专题提取完整成员并逐个校验，不把部分恢复当成完整包 |
| 只有启动或 recovery 日志 | 只能记录已见的重启线索，注明故障前现场缺失 |
| 只剩旧日志 | 明确旧日志时间，请求补采本次现场，不将旧事件当成新故障 |

需要补采时应尽量减少再次重启或清理日志造成的轮转覆盖。没有找到日志不等于设备没有发生故障，也不等于已排除某种原因。

## 8. 查询结果记录模板

```text
设备和系统版本：
dump 根目录：
要查的故障时间段：

事件摘要文件及行号：
摘要记录时间：
选定转储包及文件名时间：
包内实际日志成员或解析输出文件：
该日志所属启动及时间依据：
对应启动/recovery/用户态报告：

找到的异常原文位置：
是否有完整调用栈：
重复副本处理情况：
缺失文件或尚未解析的内容：
下一步需要读取或补采的文件：
```

完成查询的标准是：能指出本次故障对应的原始文件和具体日志位置，或清楚说明缺了什么。不要用历史设备案例或预设故障原因代替本次文件证据。