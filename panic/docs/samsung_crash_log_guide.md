# Samsung 新设备死机日志查询指南

本文只说明 Samsung 设备。目标是拿到一份新的 Samsung dump 后，快速找到与死机、重启或异常复位相关的日志文件；不包含某一台设备或某一次故障的分析结论。

## 1. 确认 dump 根目录

项目中 Samsung dump 按设备型号和采集批次组织，实际名称可能不同：

```text
dump/Samsung/<model>/<dump_name>/
```

把 `<dump_name>` 对应的目录记为 `$dump`。后续所有查询都从这个目录开始。

```powershell
$dump = 'C:\path\to\dump\Samsung\<model>\<dump_name>'
Get-ChildItem -LiteralPath $dump -Force
```

先确认目录没有指向压缩包内部、临时目录或其他设备的 dump。

## 2. Samsung 日志优先级

### 第一优先级：内核日志

Samsung 专用收集器读取以下设备节点：

```text
/proc/reset_klog
/proc/last_kmsg
```

在 dump 中对应为：

```text
<dump_root>/proc/reset_klog
<dump_root>/proc/last_kmsg
```

这两个文件应最先查询。它们可能是固定大小、截断或环形日志，不能只看文件末尾。

项目中的 Samsung 收集实现位于：

```text
jni/collectors/samsung_collector.h
```

### 第二优先级：崩溃和重启附加材料

如果 dump 中存在，再查询以下名称或目录：

```text
tombstone
dropbox
crash
debuggerd
logcat
dmesg
ramdump
pstore
uefi
bootstat
```

这些材料可能位于根目录、`data/`、`dev/`、`mnt/` 或厂商自定义目录，不要只检查一个固定路径。

### 第三优先级：init 配置

项目通用收集器会复制 init rc 文件。优先检查：

```text
<dump_root>/system/etc/init/
<dump_root>/system_ext/etc/init/
<dump_root>/vendor/etc/init/
<dump_root>/vendor/etc/init/hw/
<dump_root>/odm/etc/init/
<dump_root>/product/etc/init/
```

init rc 主要用于确认服务定义、服务重启关系和厂商 watchdog 配置，不替代实际崩溃日志。

## 3. 第一步：确认关键文件是否存在

```powershell
$coreLogs = @(
    "$dump\proc\reset_klog",
    "$dump\proc\last_kmsg"
)

Get-Item -LiteralPath $coreLogs -ErrorAction SilentlyContinue |
    Select-Object FullName, Length, LastWriteTime

Get-ChildItem -LiteralPath $dump -Force
```

如果核心文件不存在，先在整个 dump 中搜索同名文件和替代日志：

```powershell
rg --files $dump | rg -i `
  '(^|[\\/])(reset_klog|last_kmsg|dmesg|pmsg|kmsg|tombstone[^\\/]*|dropbox|crash|debuggerd|ramdump|pstore|uefi|bootstat)'
```

如果仍然找不到，报告中应明确记录“Samsung 核心内核日志缺失”，不要用其他文件推断完整的死机原因。

## 4. 第二步：查询内核死机和复位关键词

先对 `reset_klog` 和 `last_kmsg` 查询：

```powershell
rg -n -i -C 20 `
  'panic|oops|bug:|internal error|unable to handle|watchdog|lockup|deadlock|hang|reset|reboot|fatal|exception' `
  $coreLogs
```

重点关注以下类别：

```text
Kernel panic
Oops
BUG:
Internal error
Unable to handle
watchdog
hard lockup / soft lockup
reset reason
reboot reason
```

命中后记录文件名、行号和日志时间戳，再扩大上下文：

```powershell
rg -n -i -C 80 `
  'Kernel panic|Oops|BUG:|Internal error|Unable to handle|watchdog|lockup|reset|reboot' `
  $coreLogs
```

不要因为某条日志位于文件末尾就认定它是最后一个事件；应根据日志中的时间戳排序。

## 5. 第三步：查询用户态崩溃材料

在整个 dump 中寻找用户态崩溃文件：

```powershell
rg --files $dump | rg -i `
  'tombstone|dropbox|crash_dump|debuggerd|native.?crash|logcat|anr'
```

对文本日志查询：

```powershell
rg -n -i -C 20 `
  'Fatal signal|Abort message|SIGABRT|SIGSEGV|SIGBUS|native crash|process.*died|service.*(died|exited|restarting)' `
  $dump
```

如果找到 tombstone，优先阅读以下字段：

```text
Process name / Cmdline
pid / tid
signal / code
fault address
Abort message
backtrace
registers
```

只有信号关键字而没有 tombstone 或 backtrace 时，只能记录“发现用户态异常线索”，不能直接写成具体的 native 崩溃原因。

## 6. 第四步：按时间建立事件顺序

对每个候选事件记录：

```text
日志文件
日志行号
日志时间戳
进程名和 PID/TID（如果有）
事件类型
前后文中的关联事件
```

建议按下面的顺序排列：

```text
最后一次正常运行记录
首个异常或错误记录
首个明确的 panic/crash/watchdog 记录
服务退出或重启记录
reset/reboot 记录
重启后的启动记录
```

同名进程可能在重启后拥有新的 PID。关联记录时优先使用 `PID/TID + 启动时间 + 日志时间戳`，不要只按进程名匹配。

## 7. 查询 init rc 的方法

当日志出现服务退出、重启或启动失败时，搜索对应的 rc 配置：

```powershell
rg -n -i -C 8 `
  'service |onrestart|restart |oneshot|disabled|class |watchdog|shutdown' `
  "$dump\system\etc\init" `
  "$dump\system_ext\etc\init" `
  "$dump\vendor\etc\init" `
  "$dump\vendor\etc\init\hw" `
  "$dump\odm\etc\init" `
  "$dump\product\etc\init"
```

只使用实际存在的目录；某些 Samsung dump 不包含全部分区。

## 8. 最小查询脚本

下面的命令适合作为新 Samsung dump 的第一轮检查：

```powershell
$dump = 'C:\path\to\dump\Samsung\<model>\<dump_name>'
$coreLogs = @(
    "$dump\proc\reset_klog",
    "$dump\proc\last_kmsg"
)

Get-ChildItem -LiteralPath $dump -Force
Get-Item -LiteralPath $coreLogs -ErrorAction SilentlyContinue |
    Select-Object FullName, Length, LastWriteTime

rg -n -i -C 20 `
  'panic|oops|bug:|internal error|unable to handle|watchdog|lockup|reset|reboot|fatal|exception' `
  $coreLogs

rg --files $dump | rg -i `
  'tombstone|dropbox|crash|debuggerd|logcat|dmesg|ramdump|pstore|uefi|bootstat'
```

## 9. 查询结果记录格式

每份新 Samsung dump 至少记录以下内容：

```text
设备型号：
Dump 根目录：
收集批次：
reset_klog：存在 / 缺失
last_kmsg：存在 / 缺失
附加崩溃材料：
日志覆盖时间：
最早相关事件：
最早明确死机或复位记录：
关键证据文件和行号：
缺失材料：
```

结果只描述日志中能直接支持的事实。无法从现有文件确认的内容，标记为“缺少证据”，不要用其他设备或其他批次的结论补充。
