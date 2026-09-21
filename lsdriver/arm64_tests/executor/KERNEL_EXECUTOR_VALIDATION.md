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
