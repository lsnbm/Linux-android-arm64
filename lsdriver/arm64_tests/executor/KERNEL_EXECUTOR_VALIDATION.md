# ARM64 执行器验证说明

## 目标

在 Android ARM64 GKI guest 内核中，使用独立测试模块和真实 ARM64 CPU 完成：

1. 从 `../instruction.txt` 生成固定 raw 指令表；
2. 用同一份初始 GPR、SP、PC、PSTATE、Q0-Q31、FPCR、FPSR 和测试内存建立两份现场；
3. 一份在内核模块中调用生产入口 `emulate_inst()`；
4. 另一份由 initramfs 中的 runner 在 guest 用户态通过 `ptrace` 单步真实 CPU 执行对应 raw 指令；
5. 在内核态比较完整寄存器现场、FP/SIMD 状态和内存快照；
6. 每条指令输出结构化结果，严格区分 `PASS`、`FAIL`、`CPU_EXCEPTION` 和 `EXECUTOR_SKIP`。

测试模块与正式驱动分离，不修改正式异常处理路径，也不把 raw 指令写入正在运行的内核文本。

## 文件职责

| 文件 | 职责 |
| --- | --- |
| `executor_protocol.h` | 内核模块与 runner 共用的 ioctl、现场和结果协议 |
| `arm64_kernel_executor_test.c` | 内核 oracle、测试内存准备和最终比较 |
| `executor_test_runner.c` | 映射测试地址、ptrace 单步 CPU 和提交 CPU 结果 |
| `executor_test_init.c` | initramfs `/init`，加载模块并启动 runner |
| `Kbuild` | 将测试模块和生产 decoder/executor 源码组成一个模块 |
| `build_kernel_executor_test.sh` | 生成指令表并构建 `.ko` |
| `build_executor_initramfs.sh` | 构建 init、runner 和 initramfs |
| `Makefile` | 对上述构建和清理脚本提供统一入口 |

`arm64_instruction_table.h`、`arm64_kernel_executor_test_module.ko` 和
`executor-test-initramfs.cpio.gz` 均为生成文件，不应提交到版本库。它们由 `make
clean` 删除，并由 `make build` 重新生成。

## 构建

要求在 WSL 中使用 Android 14 / Linux 6.1 内核树及其工具链。默认内核目录为
`/root/6.1-Android14`，也可以通过 `KERNELS_ROOT` 覆盖。

在仓库根目录执行：

```bash
make -C lsdriver/arm64_tests/decoder strict-test
make -C lsdriver/arm64_tests/executor build
```

只构建模块或 initramfs：

```bash
make -C lsdriver/arm64_tests/executor module
make -C lsdriver/arm64_tests/executor initramfs
```

构建目标版本：

```bash
make -C lsdriver/arm64_tests/executor \
	VERSION=6.1-Android14 KERNELS_ROOT=/root build
```

构建脚本会校验 `instruction.txt` 中每行是 32 位十六进制 raw，生成指令表，
编译测试模块，并在 initramfs 中放入 `/init`、runner、模块、指令语料和设备节点。
构建过程中产生的 Kbuild 中间文件会自动删除。

## 运行

运行不是只启动裸 kernel，必须同时传入构建出的 initramfs。Windows 主机上使用：

```powershell
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
	-File .\windows\start-kernel-executor-test.ps1
```

脚本默认使用：

- `D:\QEMU\acceptance\kernel-6.1-Android14-script\Image`
- `lsdriver/arm64_tests/executor/executor-test-initramfs.cpio.gz`
- QEMU `-cpu max`、4 vCPU、2 GiB、TCG multi-thread

可以通过参数覆盖 kernel、initramfs 和 CPU：

```powershell
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
	-File .\windows\start-kernel-executor-test.ps1 `
	-KernelPath D:\QEMU\acceptance\kernel-6.1-Android14-script\Image `
	-InitrdPath E:\1.CodeRepository\Android\Kernel\lsdriver\arm64_tests\executor\executor-test-initramfs.cpio.gz
```

串口摘要写入 `D:\QEMU\executor-test-serial.log`。每条结果必须检查 `status`，
不能只根据进程退出码或 `FAIL=0` 判断成功：

```text
status=2                  PASS
status=3                  FAIL，现场或内存不一致
status=4                  CPU_EXCEPTION，真实 CPU 未正常完成
status=5                  EXECUTOR_SKIP，软件执行器未实现
status=6                  DECODE_FAIL，生产 decoder 失败
```

完整比较包括 `X0-X30`、SP、PC、PSTATE、Q0-Q31、FPCR、FPSR 和 4096 字节内存。
测试地址使用 2 MiB 映射并在逻辑数据区域前保留 guard，避免大立即数访存把 harness
自身的地址错误误报为执行器语义错误。

## 当前结果

当前固定语料为 8339 条指令，最近一次 Android 14 / Linux 6.1 QEMU 全量对拍结果为：

```text
cases=8339
status=2 count=8339
FAIL=0
CPU_EXCEPTION=0
EXECUTOR_SKIP=0
mismatch_nonzero=0
last_index=8338
```

寄存器状态不一致属于失败，不作为测试噪声处理。

## 清理

删除本目录生成的指令表、模块、initramfs 和 Kbuild 中间文件：

```bash
make clean
```

## QEMU 工作区

执行器脚本依赖 `D:\QEMU` 中的以下内容：

| 路径 | 用途 |
| --- | --- |
| `qemu-modern\qemu-system-aarch64.exe` | 执行器测试使用的 QEMU |
| `acceptance\kernel-6.1-Android14-script\Image` | Android 14 / Linux 6.1 guest kernel |
| `sdk\system-images\android-34\google_apis\arm64-v8a\system.img` | Android system image |
| `sdk\system-images\android-34\google_apis\arm64-v8a\vendor.img` | Android vendor image |
| `avd\Android14_ARM64_6_1.avd\initrd-qemu11-console-permissive` | Android AVD initrd |
| `avd\Android14_ARM64_6_1.avd\*.qcow2` | AVD 可写数据盘 |

`sdk`、`jdk`、`android-home`、`qemu-modern`、`research`、`scrcpy` 和整个当前
`Android14_ARM64_6_1.avd` 目录属于独立运行环境或研究资料，不按执行器清理。
AVD 中的 raw 镜像是否能删除取决于 qcow2 的 backing chain；删除前应使用
`qemu-img info --backing-chain` 检查，清理脚本不会自动删除这些镜像。

Windows 工作区清理脚本为 `windows\clean-qemu-workspace.ps1`。它只处理明确的
执行器/AVD 日志、状态文件、SDK 未完成下载文件和旧构建日志，默认只预览：

```powershell
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
	-File .\windows\clean-qemu-workspace.ps1
```

确认预览内容后执行删除：

```powershell
& powershell.exe -NoProfile -ExecutionPolicy Bypass `
	-File .\windows\clean-qemu-workspace.ps1 -Apply
```

2026-09-06 已执行一次清理：删除 19 个明确的日志、状态文件、旧构建日志和
SDK 未完成下载缓存，共释放约 1.77 GiB。清理后 `D:\QEMU` 约 9.95 GiB，当前
执行器和 Android AVD 的硬依赖仍全部存在。

旧的 `acceptance\kernel-6.1.23-driver-test` 是历史驱动验收输出，不是当前执行器
依赖。需要回收该历史包时显式加入 `-IncludeLegacyAcceptance`；该开关不会影响
当前使用的 `kernel-6.1-Android14-script\Image`。

本次清理后重新执行 `make build`，模块和 initramfs 均生成成功。构建仍会输出
既有的 `modpost` 未解析符号警告；这些符号由目标 Android kernel 提供，不影响
本测试模块和 initramfs 产物生成。

decoder 验证的构建和结果说明见兄弟目录的 `../decoder/VALIDATION.md`。
