# Vivo 设备死机日志定位与分析指南

本文只说明 Vivo/BBK/iQOO 设备。不同机型、系统版本和采集批次可能只包含其中一部分日志，因此应先扫描实际存在的文件，再选择分析入口。

适用范围：使用 Vivo 日志目录、BBK 日志目录或 Qualcomm 诊断目录的 Vivo、iQOO、BBK Android 设备。

## 1. 新 dump 的目录入口

项目中的 Vivo dump 通常位于：

```text
dump/Vivo/<model>/<dump_name>/
```

例如：

```text
dump/Vivo/vivo iqoo neo 10/dump_2026_10_24_20_34/
```

Vivo 专用收集器见：[vivo_collector.h](../jni/collectors/vivo_collector.h)。设备上的路径会去掉开头的 `/` 后保存到 dump 根目录，例如：

```text
设备：/data/bbkpanic
dump：data/bbkpanic/
```

通用收集器复制的 init rc 文件和根目录列表用于确认服务、日志路径及采集结果，不等于死机现场：

```text
<dump_root>/root_ls_la.txt
<dump_root>/system/etc/init/
<dump_root>/system_ext/etc/init/
<dump_root>/vendor/etc/init/
<dump_root>/vendor/etc/init/hw/
<dump_root>/odm/etc/init/
<dump_root>/product/etc/init/
```

## 2. Vivo 专用日志路径

收集器当前尝试复制以下目录。路径不存在时会跳过，所以实际 dump 不一定全部具备：

```text
/cache/recovery
/data/cache/recovery
/data/bbklog
/data/bbklog/fileobs
/data/bbkpanic
/data/bbkpanic/ssrminidump
/data/local/traces
/data/log_temp
/data/mblog
/data/mdl
/data/misc/a11ytrace
/data/misc/bluetooth/logs
/data/misc/boottrace
/data/misc/bootstat
/data/misc/boottrace/categories
/data/misc/dmesgd
/data/misc/nfc/logs
/data/misc/perfetto-configs
/data/misc/perfetto-traces
/data/misc/perfetto-traces/bugreport
/data/misc/perfetto-traces/profiling
/data/misc/recovery
/data/misc/recovery/proc
/data/misc/snapshotctl_log
/data/misc/trace
/data/misc/update_engine_log
/data/misc/wmtrace
/data/SecureElement/logData
/data/SecureElement/logData/log
/data/SecureElement/logData/rb
/data/SecureElement/logData/uploadFiles
/data/system/dropbox
/data/system/heapdump
/data/tombstones
/data/user_de/0/com.android.shell/files/bugreports
/data/vendor/audio
/data/vendor/audio/memlog
/data/vendor/esehal
/data/vendor/esehal/vcd
/data/vendor/imslogs
/data/vendor/log
/data/vendor/log/wifi
/data/vendor/nfc
/data/vendor/nfc/cloudlog
/data/vendor/nicmd
/data/vendor/pddump
/data/vendor/qmipriod
/data/vendor/qms_logs
/data/vendor/qms_logs/config
/data/vendor/qms_logs/main
/data/vendor/qms_logs/other
/data/vendor/qti-logkit
/data/vendor/qti-logkit/logdata
/data/vendor/qti-logkit/shared-privileged
/data/vendor/qti-logkit/shared-public
/data/vendor/qti-logkit/socket-privileged
/data/vendor/qti-logkit/socket-public
/data/vendor/ramdump
/data/vendor/ramdump/bluetooth
/data/vendor/ramdump/ssrminidump
/data/vendor/ramdump_ssr
/data/vendor/ssrdump
/data/vendor/shsusr
/data/vendor/tombstones
/data/vendor/tombstones/rfs
/data/vendor/tombstones/wifi
/dev/fscklogs
/mnt/vendor/persist/wlan
/sys/fs/pstore
/logdata
/vendor/data/tombstones
```

常见 dump 映射如下：

| 设备路径 | dump 中的路径 | 主要用途 |
| --- | --- | --- |
| `/data/bbkpanic` | `data/bbkpanic/` | Vivo/BBK 内核异常、TrustZone、QD 现场和历史 exception |
| `/data/log_temp` | `data/log_temp/` | 按启动批次保存的 kernel、main、system、radio、events 和 crash 日志 |
| `/logdata/recovery` | `logdata/recovery/` | recovery 启动记录、`last_kmsg*`、`last_log*` |
| `/data/vendor/qti-logkit` | `data/vendor/qti-logkit/` | Qualcomm/Vivo 诊断和日志工具输出 |
| `/data/vendor/ramdump*` | `data/vendor/ramdump/`、`data/vendor/ramdump_ssr/` | 子系统或硬件 ramdump |
| `/data/vendor/ssrdump` | `data/vendor/ssrdump/` | 子系统重启现场 |
| `/data/vendor/tombstones` | `data/vendor/tombstones/` | vendor native crash 和 tombstone |
| `/data/system/dropbox` | `data/system/dropbox/` | Android 用户态异常、系统事件和崩溃报告 |
| `/sys/fs/pstore` | `sys/fs/pstore/` | kernel panic、ramoops 或 pstore 记录 |

## 3. 单文件现场和 trace 缓冲区

收集器还会尝试复制以下文件：

```text
/data/vendor/esehal/vcd/halbplog
/data/vendor/esehal/vcd/omatabplog
/data/vendor/esehal/vcd/setabplog
/data/vendor/qmipriod/log.txt
/data/vendor/shsusr/shsusr_log.txt
/dev/block/by-name/logdata
/dev/block/bootdevice/by-name/logdata
/dev/block/bootdevice/by-name/ramdump
/dev/block/bootdevice/by-name/rawdump
/dev/kmsg
/proc/blog/step
/proc/kmsg
/proc/last_kmsg
/sys/kernel/boot_adsp/ssr
/sys/kernel/boot_slpi/ssr
/sys/kernel/debug/tracing/trace
/sys/kernel/tracing/trace
```

trace 目录使用专门的复制逻辑，只读取名为 `trace` 的缓冲文件，不递归读取 tracefs 的控制节点。控制节点可能阻塞、改变 tracing 状态或导致采集过程卡住。

对应的 trace 目录包括：

```text
/sys/kernel/debug/tracing/hyp
/sys/kernel/debug/tracing/instances/mm_events
/sys/kernel/debug/tracing/instances/wifi
/sys/kernel/debug/tracing/per_cpu
/sys/kernel/tracing/hyp
/sys/kernel/tracing/instances/bootreceiver
/sys/kernel/tracing/instances/mm_events
/sys/kernel/tracing/instances/wifi
/sys/kernel/tracing/per_cpu
```

`/dev/kmsg` 和 `/proc/kmsg` 使用非阻塞方式读取。dump 中的 kmsg 是采集时刻的快照，不一定包含故障发生瞬间的完整日志。

## 4. Vivo 日志分析优先级

### 4.1 第一优先级：`bbkpanic` 内核异常现场

先检查：

```text
<dump_root>/data/bbkpanic/exp_*.txt
<dump_root>/data/bbkpanic/tz_*.txt
<dump_root>/data/bbkpanic/q_qd_*.7z
```

`exp_*.txt` 通常包含 `Kernel (KE)`、异常 uptime、CPU、进程名、PC、LR 和调用栈。优先记录：

```text
Exception Log Time
Exception Class
PC / LR
Current Executing Process
Backtrace
```

`tz_*.txt` 用于查看 TrustZone 侧的 `SGIReset`、`SWDogBark`、`NSWDogBite`、`RPMWDogBite`、`NOCERR` 等复位线索。它通常只能说明安全世界或平台发生过 reset，不能单独替代 AP kernel 栈。

`q_qd_*.7z` 和根目录下的 `0x...-0x....bin` 可能是受保护的 Qualcomm/Vivo 现场。扩展名不保证一定是标准 7z；先识别格式和成员，解包失败时保留原文件并记录“格式未知或受保护”，不要把它当成空文件。

### 4.2 第二优先级：`data/log_temp` 启动批次日志

Vivo 日志通常按启动批次保存，例如：

```text
data/log_temp/boot_1__normal/
data/log_temp/boot_2__normal/
data/log_temp/boot_3__normal/
data/log_temp/errBoot/
```

优先查看这些文件名：

```text
kernel_log_*
crash_log_*
events_log_*
main_log_*
radio_log_*
sys_log_*
```

其中：

- `kernel_log_*`：内核启动、驱动、watchdog、PMIC、SSR 和 kernel warning；
- `crash_log_*`：用户态 `SIGABRT`、`SIGSEGV`、`crash_dump` 等摘要；
- `events_log_*`：audit、SELinux 和系统事件；
- `main_log_*`、`sys_log_*`、`radio_log_*`：Android framework、系统服务和 modem/radio 上下文。

同一故障要优先关联同一启动目录中的文件，不要把不同 `boot_N` 的同名异常拼成一次事件。

### 4.3 第三优先级：recovery、pstore 和 kmsg

检查：

```text
<dump_root>/logdata/recovery/last_kmsg
<dump_root>/logdata/recovery/last_kmsg.1 ...
<dump_root>/logdata/recovery/last_log
<dump_root>/sys/fs/pstore/
<dump_root>/proc/kmsg
<dump_root>/dev/kmsg
```

重点搜索：

```text
Kernel panic
Internal error
Oops
BUG:
Call trace:
watchdog
PS_HOLD
HARD_RESET
PMIC_WATCHDOG
subsystem restart
SSR
```

`last_kmsg*` 可能是历史启动的多个快照。文件后缀、文件修改时间和 dump 导出时间都不能单独证明它对应本次死机。

### 4.4 第四优先级：ramdump、SSR 和用户态崩溃

如果内核栈没有直接说明原因，再查看：

```text
data/vendor/ramdump/
data/vendor/ramdump_ssr/
data/vendor/ssrdump/
data/vendor/tombstones/
data/tombstones/
data/system/dropbox/
```

需要区分：

- AP kernel panic 或 kernel exception；
- PMIC、PS_HOLD 或 watchdog 直接复位；
- modem、ADSP、CDSP、Bluetooth 等子系统 SSR；
- 单个 native 进程崩溃；
- framework 服务退出后被 init 重启。

## 5. 第一轮查询脚本

```powershell
$dump = Read-Host '输入 Vivo dump 根目录'
if (-not (Test-Path -LiteralPath $dump -PathType Container)) {
    throw 'dump 目录不存在'
}

Get-ChildItem -LiteralPath $dump -Force

rg --files --hidden --no-ignore $dump | rg -i `
  'bbkpanic|bbklog|log_temp|last_kmsg|last_log|pstore|ramdump|ssrdump|tombstone|dropbox|crash|kmsg|qti-logkit|perfetto|boottrace|trace'
```

只列出文件路径后，再选择目标启动批次和日志文件。不要一开始对整个 dump 的二进制、压缩包和所有 logcat 混搜。

## 6. 关键字检索

### 6.1 内核异常和复位

```powershell
$kernelCandidates = Get-ChildItem -File -Recurse -LiteralPath $dump |
    Where-Object {
        $_.Name -match 'kernel_log|last_kmsg|last_log|kmsg|pstore|exp_\d+\.txt|tz_\d+\.txt'
    }

rg -a -n -i -C 25 `
  'Kernel panic|Internal error|Oops|BUG:|WARNING:|Unable to handle|Call trace:|watchdog|PS_HOLD|HARD_RESET|PMIC_WATCHDOG|subsystem restart|SSR|SGIReset' `
  $kernelCandidates.FullName
```

### 6.2 外部内核模块和 KernelSU 线索

如果设备允许加载外部模块，额外检查：

```powershell
rg -a -n -i -C 20 `
  'insmod|rmmod|finit_module|init_module|module_load|KernelSU|ReSukiSU|kernfs|sysfs_remove_group|free_module|mod_sysfs_teardown' `
  $dump
```

这类命中只能说明模块操作或内核补丁存在。只有当它和异常 uptime、进程名、PC/LR 以及重启时间对齐时，才可以作为根因候选。

### 6.3 用户态崩溃

```powershell
rg -a -n -i -C 20 `
  'Fatal signal|SIGABRT|SIGSEGV|SIGBUS|Abort message|crash_dump|tombstone|process.*died|service.*(died|exited|restarting)' `
  $dump
```

看到 `SIGABRT` 或 `SIGSEGV` 时，先确认进程是否为普通应用、framework 服务还是 `init`。用户态崩溃不自动等于整机 kernel panic。

## 7. 如何建立 Vivo 故障时间线

建议按以下顺序关联：

1. 从 `exp_*.txt`、`crash_log_*`、`last_log` 或 `bootstat` 找事件时间和 uptime；
2. 根据 `boot_N__normal`、`errBoot` 和 kernel 启动首行确定属于哪次启动；
3. 将 exception uptime 换算到对应启动，不把 dump 导出时间当成故障时间；
4. 在同一启动批次的 `kernel_log_*`、`main_log_*` 和 `events_log_*` 中搜索相邻事件；
5. 用 `last_kmsg`、pstore、PMIC PON log 和 TrustZone reset 信息确认最终复位类型；
6. 将 panic/exception 前的最后一个异常作为根因候选，将 watchdog、PS_HOLD 和重启后的启动错误标记为后续现象。

Vivo 设备可能存在以下时间问题：

- `exp_*.txt` 的墙上时间可能是无效的默认日期，uptime 更可靠；
- RTC 在启动早期可能尚未校准，kernel 日志和 Android 日志的墙上时间可能不同；
- `last_kmsg*`、`tz_*` 和 `q_qd_*` 是历史槽位，不一定都属于最近一次重启；
- 文件修改时间通常是统一导出时间，不是实际故障时间；
- PID 会跨重启复用，关联时必须同时看启动批次、进程名和 uptime。

## 8. 结果记录格式

每份 Vivo dump 至少记录：

```text
设备型号：
Dump 根目录：
收集批次：
Vivo/BBK 日志目录：存在 / 缺失
bbkpanic exception：存在 / 缺失
TrustZone reset 记录：存在 / 缺失
kernel 日志：
crash 日志：
recovery/pstore/kmsg：
ramdump/SSR：
用户态崩溃材料：
目标启动批次：
事件时间和 uptime：
复位类型：
最早明确异常：
关键证据文件和行号：
仍缺少的材料：
结论置信度：高 / 中 / 低
```

结果只描述日志能直接支持的事实。无法确认的内容标记为“缺少证据”，不要用其他 Vivo 机型、其他启动批次或经验路径补充结论。