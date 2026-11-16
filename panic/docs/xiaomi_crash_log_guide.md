# Xiaomi 设备死机日志定位与分析指南

本文只说明 Xiaomi/MIUI/HyperOS 设备。其他厂商的 dump 目录和日志格式请参考对应设备文档。

适用范围：所有 Xiaomi/MIUI/HyperOS 手机和平板的离线 dump。不同机型、系统版本和收集方式可能只包含其中一部分日志目录，因此应先扫描实际存在的文件，再选择分析入口。

## 1. 新日志从哪里开始找

拿到一份新的 Xiaomi dump 后，先把设备目录称为 `$dump`。项目中的目录通常类似：

```text
dump/Xiaomi/<model>/<dump_name>/
```

不同版本的收集结果可能直接把日志目录放在 dump 根目录下，也可能放在 `data/`、`mnt/rescue/` 或 `mnt/rescue/mqsas/` 下。不要只固定查找一个文件名，先列出所有候选日志：

```powershell
$dump = 'C:\path\to\dump\Xiaomi\<model>\<dump_name>'

Get-ChildItem -LiteralPath $dump -Force
Get-ChildItem -File -Recurse -LiteralPath $dump |
    Where-Object {
        $_.Name -match 'last_kmsg|kmsg|hyplog|md_history|eventinfo|android-kmsg|LocalKernel|tombstone|ramoops|mtdoops|logcat|crash|reset|reboot'
    } |
    Select-Object FullName, Length, LastWriteTime
```

项目收集器的 Xiaomi 专用来源见：[mi_collector.h](../jni/collectors/mi_collector.h)。它会尝试复制以下设备路径；某个路径不存在时会跳过，因此不同设备的 dump 内容不完全相同：

```text
/data/local/log
/data/local/bootlog
/data/vendor/tz_log
/dev/logfs
/dev/rescue
/mnt/rescue
/mnt/rescue/mqsas
/cache/mqsas
/data/mqsas
/data/miuilog
/data/system/miuilog
/data/miuilog/stability/nativecrash
/data/miuilog/stability/nativecrash/tombstones
/data/miuilog/stability/nativecrash/coredump
/data/miuilog/misight
/data/misc/dmesgd
/data/vendor/ramdump
/data/vendor/ramdump_ssr
/data/vendor/tombstones
/data/vendor/diag
/data/vendor/pddump
/data/vendor/fpdump
/data/vendor/wlan_logs
/data/vendor/log/wifi
/data/vendor/imslogs
/data/vendor/charge_logger
/data/vendor/bsplog
```

离线 dump 中的设备路径会去掉开头的 `/`，所以常见对应关系是：

| 设备路径 | dump 中常见路径 | 主要用途 |
| --- | --- | --- |
| `/data/vendor/diag` | `data/vendor/diag/` | Qualcomm/MIUI 诊断、`last_kmsg`、`last_hyplog`、重启前后日志、minidump |
| `/mnt/rescue/mqsas` | `mnt/rescue/mqsas/` | MQSAS 稳定性记录、`android-kmsg.txt`、kernel exception/reboot 记录、恢复包 |
| `/data/miuilog/misight` | `data/miuilog/misight/` | Misight 事件摘要、kpanic 分类、时间和事件类型 |
| `/data/miuilog/stability/nativecrash/tombstones` | 同名 dump 路径 | native crash 的 tombstone |
| `/data/local/log` | `data/local/log/` | MIUI 本地运行日志、`mtdoops.md` 等平台日志 |
| `/data/vendor/ramdump` | `data/vendor/ramdump/` | 子系统或硬件 ramdump |

此外，通用收集器会复制 init 配置和根目录列表，见：[common_collector.h](../jni/collectors/common_collector.h)。重点目录包括：

```text
<dump>/system/etc/init/
<dump>/system_ext/etc/init/
<dump>/vendor/etc/init/
<dump>/vendor/etc/init/hw/
<dump>/odm/etc/init/
<dump>/product/etc/init/
```

## 2. 小米日志的优先级

建议按以下顺序找，不要一开始在完整 logcat 中盲目搜索。

### 2.1 第一优先级：本次启动对应的 kernel 日志

优先检查以下文件是否存在：

```text
<dump>/data/vendor/diag/last_kmsg
<dump>/data/vendor/diag/last_kmsg_1 ... last_kmsg_9
<dump>/data/vendor/diag/last_hyplog
<dump>/mnt/rescue/mqsas/android-kmsg.txt
<dump>/mnt/rescue/recovery/last_kmsg
<dump>/data/local/log/mtdoops.md
```

其中：

- `last_kmsg` 通常是最重要的 Linux kernel 现场，可能是二进制或含有 NUL 字节，读取失败时不要认为文件为空；用 `rg -a` 或 PowerShell 读取；
- `last_kmsg_1`、`last_kmsg_2` 等不一定是本次死机，必须用日志时间、uptime、版本和 reset 时间判断；
- `last_hyplog` 用于确认平台监控器是否记录 watchdog bite、复位来源及复位时的地址信息；
- `android-kmsg.txt` 常常是恢复环境保存的 kmsg，可能包含驱动加载、模块初始化和其他早期内核信息；
- `mtdoops.md`、`recovery/last_kmsg` 和 `last_kmsg` 可能互相重复，重复内容只能作为交叉验证，不能当成多次独立故障。

批量查看这些文件：

```powershell
$kernelLogs = Get-ChildItem -File -Recurse -LiteralPath $dump |
    Where-Object {
        $_.Name -match '^(last_kmsg(_\d+)?|last_hyplog|android-kmsg(\.txt)?|mtdoops(\.md)?|last_log(\.\d+)?)$'
    }

$kernelLogs | Select-Object FullName, Length, LastWriteTime
```

### 2.2 第二优先级：稳定性和事件摘要

然后查看：

```text
<dump>/data/miuilog/misight/0/eventinfo.*
<dump>/data/vendor/diag/md_history
<dump>/mnt/rescue/mqsas/stabilityrecord/LocalKernelExceptionRecord.txt
<dump>/mnt/rescue/mqsas/stabilityrecord/LocalKernelRebootRecord.txt
<dump>/mnt/rescue/mqsas/stabilityrecord/RebootChart.txt
<dump>/mnt/rescue/mqsas/stabilityrecord/HangChart.txt
```

这类文件适合回答：

- 系统把本次事件归类为 `kpanic`、`Kernel Exception`、watchdog、hang 还是其他类型；
- 事件发生的墙上时间；
- 设备是否记录过更早的历史 kernel exception；
- 是否有恢复或重启链路。

摘要文件不能替代 kernel 原始栈。即使 `eventinfo` 已经给出异常类型和几层调用栈，也要回到 `last_kmsg` 找 `pc/lr`、寄存器、`Call trace` 和 `Modules linked in`。

### 2.3 第三优先级：native crash 和用户态日志

若 kernel 日志没有明确 panic，再查：

```powershell
rg --files $dump | rg -i 'tombstone|dropbox|crash|native|debuggerd|logcat|coredump|hprof'
```

重点目录：

```text
<dump>/data/miuilog/stability/nativecrash/tombstones/
<dump>/data/vendor/tombstones/
<dump>/data/vendor/tombstones/rfs/
<dump>/vendor/data/tombstones/
<dump>/data/vendor/ramdump/
<dump>/data/vendor/ramdump_ssr/
```

native crash 至少要记录：

```text
Process name / Cmdline
pid / tid
signal、code、fault addr
Abort message
registers
backtrace
memory map
进程启动时间和退出时间
```

只有 `SIGSEGV`、`SIGBUS` 或一条 `Fatal signal` 时，不能直接断定是整机死机。还要关联 tombstone、进程退出、服务重启和系统是否继续运行。

## 3. 第一轮关键字检索

### 3.1 内核异常、panic 和内存破坏

```powershell
$allLogs = $kernelLogs.FullName

rg -a -n -i -C 12 `
  'Kernel panic|panic - not syncing|Internal error|Oops|BUG:|WARNING:|Unable to handle|NULL pointer|paging request|use-after-free|slab-out-of-bounds|KASAN|list corruption|Synchronous Abort|bad mode|Call trace|Modules linked in' `
  $allLogs
```

优先关注以下原文：

```text
Kernel panic - not syncing
Internal error:
Oops:
BUG:
Unable to handle kernel NULL pointer dereference
Unable to handle kernel paging request
Bad mode in Synchronous Abort handler
watchdog bite
hard lockup
soft lockup
RCU stall
hung task
blocked for more than
```

### 3.2 重启、watchdog、PMIC 和 hypervisor

```powershell
rg -a -n -i -C 12 `
  'watchdog|WDT|bite|reset|reboot|PS_HOLD|PMIC|PON|thermal|brownout|power key|hypervisor|subsystem restart|SSR' `
  $allLogs
```

需要区分：

- kernel 先崩溃，watchdog 后续复位；
- kernel 没有 panic，某个 CPU 或 VM 没有喂狗，最终由 watchdog 复位；
- PMIC/电源直接复位，kernel 没有机会留下完整栈；
- 子系统 SSR，只重启 modem、ADSP、CDSP、Wi-Fi 或 Bluetooth，不一定是整机死机。

### 3.3 用户态异常和服务退出

```powershell
rg -a -n -i -C 12 `
  'Fatal signal|SIGABRT|SIGSEGV|SIGBUS|Abort message|crash_dump|tombstone|process .*died|has died|service .*exited|restarting|system_server|surfaceflinger' `
  (Get-ChildItem -File -Recurse -LiteralPath $dump).FullName
```

用户态错误要和内核错误分层处理，不能看到 `system_server`、`surfaceflinger` 或 `SIGSEGV` 就直接判定为整机 kernel panic。

## 4. 如何确认哪一份日志属于本次死机

小米 dump 中最容易误判的是历史文件。按下面顺序建立时间线：

1. 从 `LocalKernelRebootRecord.txt`、`eventinfo.*`、`md_history` 找到本次重启的墙上时间；
2. 从 `last_hyplog` 或 `last_kmsg` 找到最后一次 watchdog/reset 的 uptime；
3. 在 `last_kmsg`、`android-kmsg.txt` 和其他副本中搜索同一时间、同一 uptime 或同一异常文本；
4. 只把与本次时间和版本匹配的文件作为第一现场，其余标记为历史记录；
5. 将 panic 前的最后一条异常作为候选根因，将 panic/watchdog 后的记录标记为次生事件。

推荐先执行：

```powershell
# 查看稳定性记录和事件摘要中的时间、分类
Get-Content -LiteralPath "$dump\mnt\rescue\mqsas\stabilityrecord\LocalKernelRebootRecord.txt" -ErrorAction SilentlyContinue
Get-Content -LiteralPath "$dump\mnt\rescue\mqsas\stabilityrecord\LocalKernelExceptionRecord.txt" -ErrorAction SilentlyContinue
Get-ChildItem -File -Recurse "$dump\data\miuilog\misight" -Filter 'eventinfo.*' -ErrorAction SilentlyContinue |
    ForEach-Object { Get-Content -LiteralPath $_.FullName }

# 对二进制或带 NUL 的 kernel 日志使用 -a
rg -a -n -i -C 60 `
  'Kernel panic|Bad mode|Internal error|Oops|BUG:|watchdog bite|WDT bite|Call trace|pc :|lr :' `
  "$dump\data\vendor\diag" "$dump\mnt\rescue" "$dump\data\local"
```

如果某个 `last_kmsg_N` 的 uptime 与本次事件不一致，不要因为它包含更完整的栈就把它当作当前故障。

## 5. 内核 panic 的现场提取

遇到明确 kernel panic 时，至少记录：

```text
事件墙上时间
日志 uptime
panic/Oops 原文
CPU
PID
Comm
内核版本和 Build ID
Hardware name
Tainted
ESR/FAR/SPSR（如果有）
pstate
pc/lr/sp
x0-x30（如果有）
Call trace
Modules linked in
watchdog/hypervisor/PMIC 后续记录
```

提取现场上下文：

```powershell
rg -a -n -i -C 100 `
  'Kernel panic|Bad mode|Internal error|Oops|BUG:|watchdog bite|WDT bite' `
  "$dump\data\vendor\diag\last_kmsg" `
  "$dump\data\vendor\diag\last_kmsg_1" `
  "$dump\mnt\rescue\mqsas\android-kmsg.txt"
```

如果日志出现：

```text
pc : some_function+0x... [some_module]
```

先判断该模块是否位于 `Modules linked in`，再去找对应 `.ko`、源码、Build ID 和编译版本。对于第三方、root、调试或注入模块，必须额外记录模块是否带 `(O)`、是否在 `Tainted` 中体现，以及模块加载时间。

如果 `pc` 是裸地址而没有符号：

1. 记录完整地址、内核版本、模块加载地址或模块布局；
2. 查找 `kallsyms`、`modules`、`ramdump`、符号表或对应未剥离 `.ko`；
3. 不要仅凭地址落在某个大模块附近就下确定结论；
4. 优先寻找同一故障副本中已经符号化的 `Call trace`。

## 6. 平台 watchdog 和复位判断

部分 Xiaomi 设备会在 `last_hyplog` 中记录平台监控器和 watchdog 信息。此类记录通常用于确认复位动作和时间，不一定包含真正的故障函数。

判断时要继续回到 kernel 日志：

- 如果 `last_kmsg` 在更早时间有 `Kernel panic`、同步异常或 Oops，watchdog 是后果；
- 如果只看到某个 CPU 长时间停在同一调用栈，再结合 `hard lockup`、`RCU stall` 或 `hung task` 判断卡死；
- 如果只有 watchdog 记录，没有 kernel 调用栈，只能确认 watchdog reset，不能可靠确认具体驱动或函数；
- reset reason 只说明复位类型，不能替代复位前的 kernel 异常现场。

## 7. 小米设备上常见的误判

### 7.1 把历史 `last_kmsg_1..9` 当成本次现场

这些文件可能来自之前的启动或之前的死机。必须用墙上时间、uptime、版本、异常文本和 reset 记录交叉确认。

### 7.2 把 watchdog 当成第一根因

watchdog 常常只是系统已经 panic、卡死或停止喂狗后的最终复位动作。先找 watchdog 之前的第一条明确异常。

### 7.3 把异常处理或 `trace_*` 栈当成根因

panic 处理、CPU 停止、IPI 和 trace warning 可能出现在异常处理链里。应继续向下找保存的原始 `pc/lr` 和最外层调用链；异常处理函数本身不一定是最初发生故障的函数。

### 7.4 把 native crash 当成 kernel panic

`Fatal signal`、`SIGSEGV`、`SIGBUS` 和 `tombstone` 属于用户态崩溃证据。除非同时存在明确的 `Kernel panic`、Oops、watchdog 或硬件复位证据，否则不能称为 kernel 死机。

### 7.5 把恢复阶段错误当成死机原因

重启后常见的 vibrator、panel、mount、服务不可用和初始化失败日志，可能只是系统正在恢复或硬件尚未准备好。应优先使用死机前的原始 kernel 日志。

### 7.6 忽略第三方或额外模块

`Modules linked in` 可能出现 `(O)` 模块，也可能因为日志截断而不完整。发现第三方、调试或额外模块时，应检查：

- 模块加载时间是否早于死机；
- 模块版本、Build ID 和设备实际使用的文件是否一致；
- `pc/lr` 是否落在该模块或其调用路径中；
- 模块是否改变了设备节点、硬件访问、线程、定时器或工作队列的生命周期。

## 8. 从日志追到驱动源码

确定了函数或模块后，再做源码关联：

1. 用 `Modules linked in`、`pc/lr` 和日志中的模块名确定候选模块；
2. 对照模块 Build ID、版本、编译产物和源码提交，避免拿错 `.ko`；
3. 用函数名、日志 tag、设备节点或寄存器地址在源码中搜索；
4. 读取调用函数的上下文，确认是否存在锁、引用计数、生命周期、异常返回和并发问题；
5. 将源码假设与日志中的时间、线程、CPU、寄存器和调用栈逐项核对；
6. 只有日志证据和源码控制流能相互解释时，才把“可能相关”升级为“高可信根因”。

如果日志中的 `pc/lr` 指向某个模块、驱动入口、异常处理函数或自定义代码区域，应检查：

- 模块是否在故障发生前加载，且版本、Build ID 与设备实际使用的 `.ko` 一致；
- 日志中的函数、设备节点、日志 tag 或错误码能否在源码中找到对应实现；
- 模块的编译配置、内核版本和 ABI 是否与设备实际环境匹配；
- 回调、工作队列、定时器、锁和引用计数是否存在生命周期或并发问题；
- 反汇编、符号表和源码行号是否能共同解释日志中的 `pc/lr` 与调用栈。

## 9. 建议的最小分析命令

以下命令适用于 Windows PowerShell。命令中的路径按新 dump 修改：

```powershell
$dump = 'C:\path\to\dump\Xiaomi\<model>\<dump_name>'

# 1. 确认 dump 根目录和核心日志
Get-ChildItem -LiteralPath $dump -Force
Get-ChildItem -File -Recurse -LiteralPath $dump |
    Where-Object { $_.Name -match 'last_kmsg|last_hyplog|md_history|eventinfo|android-kmsg|LocalKernel|tombstone|ramdump|logcat' } |
    Select-Object FullName, Length, LastWriteTime

# 2. 读取本次事件摘要
Get-Content -LiteralPath "$dump\mnt\rescue\mqsas\stabilityrecord\LocalKernelRebootRecord.txt" -ErrorAction SilentlyContinue
Get-ChildItem -File -Recurse "$dump\data\miuilog\misight" -Filter 'eventinfo.*' -ErrorAction SilentlyContinue |
    ForEach-Object { Get-Content -LiteralPath $_.FullName }

# 3. 搜索 kernel panic 和异常
rg -a -n -i -C 30 `
  'Kernel panic|Internal error|Oops|BUG:|Bad mode|Call trace|watchdog bite|WDT bite|hard lockup|RCU stall|hung task' `
  "$dump\data\vendor\diag" "$dump\mnt\rescue" "$dump\data\local"

# 4. 搜索用户态 crash 和服务退出
rg -a -n -i -C 20 `
  'Fatal signal|SIGABRT|SIGSEGV|SIGBUS|Abort message|tombstone|crash_dump|service .*exited|has died|restarting' `
  (Get-ChildItem -File -Recurse -LiteralPath $dump).FullName

# 5. 搜索模块、驱动和日志 tag
rg -a -n -i -C 10 `
  'Modules linked in|module|driver|firmware|device|warning|error|reset' `
  (Get-ChildItem -File -Recurse -LiteralPath $dump).FullName
```

如果 `rg` 对某个文件没有结果，先确认文件是否为二进制；对 kernel 日志使用 `rg -a`。如果文件体积很大，先限定到 `data/vendor/diag`、`mnt/rescue/mqsas` 和 `data/miuilog/misight`，再扩大范围。

## 10. 最终分析报告至少写什么

每份 Xiaomi 新日志最终至少输出：

```text
设备型号、系统版本和 dump 名称
dump 收集时间和可见日志时间范围
本次故障对应的墙上时间和 uptime
核心 kernel 日志是否存在，是否可能为历史副本
是否存在明确 Kernel panic/Oops/BUG/watchdog/硬件复位
最早的明确致命事件
最终触发重启的事件
CPU、PID、TID、Comm
pc/lr/sp、ESR/FAR、Call trace
相关模块和 Build ID
是否存在 native tombstone 或用户态服务退出
驱动源码中对应的函数和控制流
已确认的事实
高可信度推断
仍无法确认的部分
需要补采的日志、符号或源码
```

结论措辞要区分证据等级：

```text
已确认：last_kmsg 中存在明确的 Kernel panic 原文，且栈指向某模块。
高可信度：事件时间、uptime、pc/lr、模块 Build ID 和源码控制流一致。
可能：某驱动与故障时间相关，但缺少原始栈、寄存器或对应版本源码。
无法确认：只有 watchdog bite 或事件摘要，没有 HLOS kernel 栈。
```

不要把以下内容直接当成死机根因：

- 单条 `SIGSEGV`、`SIGBUS` 或 `Fatal signal`；
- 单个线程退出；
- panic、watchdog 或重启之后出现的连锁错误；
- `eventinfo` 的笼统事件分类；
- 历史 `last_kmsg_N` 中与本次时间不匹配的异常；
- 只有 hypervisor PC/FP、但没有 HLOS 符号化栈的地址；
- 仅凭进程名或模块名相同而跨启动实例关联日志。