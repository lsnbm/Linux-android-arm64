# ARM64 Executor 实体设备验证

## 输入与组件

`../instruction.txt` 按文件顺序驱动测试。`executor_test_runner.c` 生成共同寄存器和
内存输入，`arm64_kernel_executor_test.c` 调用生产 `emulate_inst()`，二者通过
`executor_protocol.h` 的 protocol v6 交换原始状态。

构建和运行依赖如下：

1. `build_kernel_executor_test.sh` 生成指令表并构建各内核版本的测试模块；
2. `build_android_executor_tests.sh` 构建静态 AArch64 runner；
3. `run_on_android_device.sh` 在设备上加载模块、运行 runner 并清理；
4. `../run-arm64-executor-test-device.ps1` 选择设备和模块、传输文件、校验日志及
   最终设备状态。

## 构建

从 `executor` 目录构建当前设备所需产物：

```bash
make VERSION=6.1-Android14 module
make device-binaries
```

构建全部支持版本：

```bash
make build
```

主机需要对应 Android 内核源码树和 NDK 工具链。设备需要 AArch64、root shell，
并允许加载匹配当前内核的测试模块。

## 执行

在 Windows PowerShell 中运行：

```powershell
& .\lsdriver\arm64_tests\run-arm64-executor-test-device.ps1
```

多设备连接时指定序列号：

```powershell
& .\lsdriver\arm64_tests\run-arm64-executor-test-device.ps1 -Serial 2912b4a6
```

每项先由持续存在的 ptrace 子进程在实体 CPU 上单步。runner 以上一个成功项的
CPU 输出为下一项现场，只为当前指令重设 PC，以及必要的访存地址或寄存器分支
目标。CPU 正常完成后才调用内核模拟器，并比较：

- X0-X30、SP、PC、PSTATE；
- Q0-Q31、FPCR、FPSR、TPIDR_EL0；
- 4096 字节数据内存。

## 判定

结果行只有以下两种通过形式：

```text
status=2 mismatch=none
status=4 action=skip emulator=not_called
```

`status=4` 仅用于实体 CPU 同步异常。该项不调用模拟器、不更新连续现场，并在下一
项重建 CPU session。CPU 正常执行但模拟器拒绝时测试失败。

当前 2047 项语料的预期汇总为：

```text
continuous test passed cases=2047
case_counts=compared=2046 skipped=1
runner_status=0
cleanup_status=0
```

index 1798 的 `0x41363A88` 是 undefined word，应输出 SKIP；其前后项目应正常
对拍。每项结果、汇总计数、protocol/build identity 和退出码均由主机脚本校验。

## 清理

设备端脚本退出时卸载模块并删除设备节点；主机脚本还会删除远端临时文件，并确认
模块未加载、设备节点不存在、SELinux 状态未变化。

删除本地生成产物：

```bash
make -C lsdriver/arm64_tests/executor clean
```

## PTE 异常出口的 FP/SIMD 回写

2026-09-17 检查现有 6.1-Android14 模块发现，`ptebp_handle_exec_fault()`
在入口保存 d8-d15，退出时在完整 Q 寄存器回写之后又恢复 d8-d15。
这些标量加载把 Q8-Q15 低 64 位覆盖为入口旧值，并将高 64 位清零；
提前返回、未命中受管页面的路径同样会执行这些加载。

原因是 `write_all_q_regs()` 的 v8-v15 clobber 触发了 C ABI 的
callee-saved 保存恢复。该函数是异常现场提交边界，必须强制内联，使用
memory clobber，并依赖调用对象的 `-mno-implicit-float` 和禁用自动向量化
配置。不得当作允许编译器持有浮点临时值的普通 C helper 使用。
更改编译选项或调用边界后，应重新检查最终模块汇编，确保 Q 回写之后没有
编译器插入的 SIMD 覆盖，尤其是 d8-d15 的恢复。

修复后的 Linux 6.1 模块构建通过，PTE handler 的 d8-d15 保存恢复已消失。
对 `instruction.txt` 第 2097-3120 行单独构建测试模块，在
`6.1.25-android14-11-o-gb65c8cff8958` 实体设备上得到：

```text
continuous test passed cases=1024
case_counts=compared=1024 skipped=0
runner_status=0
cleanup_status=0
```

日志为 `ptebp-page-2097-validation.log`。该测试比较的是测试输入下的
单指令架构语义，不覆盖真实游戏控制流、全部浮点输入或 PTE 异常退出。
因此通过单指令对比不能排除现场回写错误；游戏姿态恢复仍需加载修复后的
主驱动后复测。
