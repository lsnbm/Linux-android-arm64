# ARM64 Executor 实体设备验证说明

## 验证原则

Executor 会真实执行 ARM64 指令并比较寄存器、系统状态和内存副作用，因此验收
结果必须来自实体 ARM64 CPU。本项目固定使用已确认可加载测试模块的
`realme RMX3888`，目标系统为 Android 14、Linux 6.1.25。

Decoder 是跨平台纯 C 计算，其主机 LLVM 严格审计见 `../decoder/VALIDATION.md`；
本文只描述需要实体 CPU 的执行器测试。

## 连续状态模型

测试连续保存和校验体系结构状态，但不把 `../instruction.txt` 当作自然控制流
直接运行。全部 9362 条指令使用以下逐条重定位模型：

1. 用户态 runner 创建一个持续存活的 ptrace 子进程；
2. 每步开始前确认子进程状态和数据内存仍等于上一步 CPU 输出；
3. runner 以上一步 CPU 原始输出为基础，只生成一次本步可执行输入：把 PC 放到固定
   指令槽，并按本条指令设置访存基址/索引或寄存器分支目标；
4. 该输入逐字节不变地分别交给实体 CPU 和内核生产入口 `emulate_inst()`；内核
   `PREPARE` 返回后，runner 用 `memcmp` 确认内核没有改写任何输入字节；
5. 原始指令由子进程执行一次 `PTRACE_SINGLESTEP`，且停点必须是
   `SIGTRAP/TRAP_TRACE`；
6. runner 在 `COMPLETE` 前冻结 CPU 原始输出；内核只回传 executor 原始输出且无权
   生成 PASS，runner 先确认 CPU 快照未被内核改写，再逐 bit 比较两份原始输出；
7. 任意差异、CPU 异常、生产入口拒绝指令或准备失败立即返回非零，后续指令
   不再执行。

因此这里的“连续”是同一子进程中的状态链连续，不是 PC 自然流转，也不是每条
指令前完全不改写现场。准备阶段的重定位本身不属于被测指令效果。

比较范围包括：

- X0-X30、SP、PC、PSTATE；
- Q0-Q31、FPCR、FPSR；
- TPIDR_EL0；
- 4096 字节测试内存。

所有整数寄存器按完整位宽判等，Q 寄存器和内存逐字节判等；任意单 bit 变化都会
失败，并记录首个差异 bit。CPU 的 PSTATE 只移除 `PTRACE_SINGLESTEP` 注入、并非
被测指令产生的 `SS` 调试控制位，其余位原值比较。Executor 依赖生产 decoder 的指令类别、具体指令和
属性派发硬件模板，因此相关误解码通常会表现为执行后寄存器或内存差异，但这不
替代独立 decoder 测试。

代码和数据映射仅开放中央 4096 字节，周围 guard 页保持不可访问。访存地址准备
错误或超出比较快照的访问必须表现为 CPU 异常，不能在未比较区域中静默通过。

本测试不声称覆盖 SVE/SME 的 Z/P/FFR/ZA 状态，也不声称一条指令的单个语料实例
同时覆盖其所有条件路径。例如条件分支、CAS 和 STXR 的 taken/success 路径由
`instruction.txt` 中该实例执行时的链式输入决定。完整通过只证明本次 9362 个
实例在实际路径上的结果一致。

## 文件职责

| 文件 | 职责 |
| --- | --- |
| `executor_protocol.h` | 内核模块与 runner 共用的输入和 executor 原始输出 ioctl 协议 |
| `arm64_kernel_executor_test.c` | 以 runner 原始输入调用 executor，并原样返回 executor 输出 |
| `executor_test_runner.c` | 唯一生成共同输入、保护 CPU 快照、独立比较并生成 PASS |
| `build_kernel_executor_test.sh` | 构建测试模块并生成固定指令表 |
| `build_android_executor_tests.sh` | 构建静态 AArch64 runner |
| `run_on_android_device.sh` | 设备端加载、执行、卸载和清理 |
| `../run-arm64-executor-test-device.ps1` | 主机端设备识别、部署、判定和日志拉取 |

生成文件不提交到版本库：

- `arm64_instruction_table.h`；
- `arm64_kernel_executor_test_module.ko`；
- `executor_test_runner`；
- 桌面上的 `arm64-executor-*.log`。

## 环境要求

主机要求：

- Windows PowerShell；
- `adb` 可用；
- `executor` 目录中已有六个版本的 `.ko` 和 `executor_test_runner`。

设备要求：

- `uname -m` 返回 `aarch64`；
- KernelSU `su -c` 可获得 uid 0；
- 内核主次版本存在对应的已构建 `.ko`；
- USB 调试已授权。

## 一键运行

连接设备后，在 `lsdriver\arm64_tests` 目录执行：

```powershell
& .\run-arm64-executor-test-device.ps1
```

脚本依次执行：

1. 检测唯一在线 ADB 设备、AArch64 架构和 root；
2. 读取 `uname -r`，按内核主次版本自动选择对应的 `.ko`；仅当内核为 `5.10` 时读取 Android 主版本，在 Android 12 和 13 模块之间选择；
3. 以 `arm64_kernel_executor_test_module.ko` 为统一名称推送模块，同时推送 runner、`instruction.txt` 和设备端脚本；
4. 加载测试模块并连续对拍 `instruction.txt` 中全部 9362 条指令；
5. 将设备日志写入 `/data/local/tmp/arm64_executor_test.log` 并拉取到 Windows 桌面；
6. 校验结果行数、所有 `status=2`、通过标记和退出码；
7. 卸载模块、删除设备节点和远端临时文件，并确认 SELinux 状态不变。

存在多台 ADB 设备时指定序列号：

```powershell
& .\run-arm64-executor-test-device.ps1 -Serial 2912b4a6
```

完整日志会在成功或失败时拉取到 Windows 桌面，文件名包含设备、模块版本和时间。

## 状态码

每条结果都必须检查 `status`：

```text
status=2 mismatch=none  PASS
status=3  FAIL，寄存器、系统状态或内存不一致
status=4  CPU_EXCEPTION，实体 CPU 未正常完成
```

PASS 行不输出 `expected=0`、`actual=0` 等未使用的差异占位字段。runner 在首条
指令前还会验证并输出 `input_profile`：31 个 GPR 必须全部非零且互不相同，
Q0-Q31 和 4096 字节内存必须各自覆盖全部 256 种字节值。向量中的 1 个零字节
和内存中的 16 个零字节属于完整字节域的边界覆盖，不是全零输入。

只有恰好 9362 条结果、每条均为 `status=2`、最终通过标记存在且脚本返回 0，
才能认定通过。

## 正式结果

2026-09-09 在以下设备上运行：

```text
manufacturer=realme
model=RMX3888
machine=aarch64
kernel=6.1.25-android14-11-o-gb65c8cff8958
SELinux=Enforcing
```

结果：

```text
protocol_version=6
input_owner=runner
verdict_owner=runner
raw_compare=x0-x30,sp,pc,pstate,q0-q31,fpcr,fpsr,tpidr_el0,memory4096
instruction_count=9362
continuous test passed cases=9362
runner_status=0
cleanup_status=0
real-device executor test passed cases=9362
cleanup module=unloaded device=absent selinux=Enforcing
```

全部 9362 条逐条重定位状态链对拍通过，未出现任何已比较 GPR、SP、PC、PSTATE、
Q 寄存器、FPCR、FPSR、TPIDR_EL0 或 4096 字节内存差异。

## 清理

删除本地生成产物：

```bash
make -C lsdriver/arm64_tests/executor clean
```

设备端脚本和主机入口均带清理逻辑。测试成功或失败后都应满足：测试模块未加载、
`/dev/arm64_executor_test` 不存在、远端临时文件已删除、SELinux 状态不变。