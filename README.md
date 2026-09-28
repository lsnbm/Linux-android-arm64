# 内存调试分析驱动

> 仅供技术研究与学习，严禁用于非法用途。作者不承担任何违法责任。

交流群：
QQ:1092055800
TG:https://t.me/+ArHIx-Km9jkxNjZl  


---

## 依赖初始化

Capstone、Dear ImGui、nlohmann/json 和 BS::thread_pool 通过 Git 子模块引入，分别跟踪官方 `next`、`master`、`develop` 和 `master` 分支。

首次克隆仓库时初始化子模块：

```bash
git clone --recurse-submodules <repository-url>
```

已有工作区初始化或同步到仓库记录的子模块版本：

```bash
git submodule update --init --recursive
```

需要升级依赖时再执行 `git submodule update --init --remote --recursive`。这会拉取四个子模块各自跟踪分支的最新提交，而不是使用主仓库记录的版本。

GitHub 的 Download ZIP 不包含子模块源码，请使用 Git 克隆仓库。

---

## 1. 功能总览

`lsdriver` 是一个运行在 Android ARM64 内核中的内存调试与输入辅助模块。它不创建字符设备，也不通过 `ioctl`、`netlink`、`procfs` 或 `sysfs` 暴露命令接口；用户态程序通过固定地址共享内存与内核线程通信。

主要功能：

1. [进程内存读写](#5-进程内存读写实现)
   - 对指定 `pid` 的用户虚拟地址执行读写。
   - 内核侧自动跨页拆分，用户态封装侧对超过 `0x1000` 字节的大块读写继续分片，避免覆盖共享缓冲。
   - 读大块内存时遇到失败页会跳过并对失败块清零，只要成功处理过字节就返回成功字节数。

2. [进程内存布局枚举](#6-进程内存布局枚举)
   - 返回模块列表 `modules[]`，每个模块包含多个区段 `segs[]`，区段带 `index`、`prot`、`start`、`end`。
   - 返回可扫描内存区域 `regions[]`，主要用于指针扫描、特征扫描和通用内存搜索。
   - 收集 `/data/` 下的文件模块，并将符合条件的匿名 RWX 映射作为伪模块返回。
   - 识别紧邻模块尾部的匿名可写 BSS，并兼容被改成 `-w-p` 的 BSS。
   - 对 VMA 碎裂、远端诱饵段、权限污染进行后处理，尽量给用户态提供稳定的模块基址和区段索引。

3. [虚拟触摸注入](#7-虚拟触摸实现)
   - 自动查找多点触摸 input 设备，在 10 个 slot 中划分物理和虚拟触摸槽位。
   - 虚拟槽位数量由初始化参数指定，调用方使用从 0 开始的虚拟 slot 编号。
   - 支持 `TouchDown`、`TouchMove`、`TouchUp`，并返回触摸设备原始坐标范围。
   - 手动维护 `BTN_TOUCH`、`BTN_TOOL_FINGER`、`BTN_TOOL_DOUBLETAP`，降低真实触摸与虚拟触摸互相污染的概率。

4. [ARM64 断点](#8-arm64-断点)
   - 读取 CPU 支持的 BRP/WRP 数量。
   - 硬件断点支持执行断点 `X` 与访问观察点 `R`、`W`、`RW`，另有 PTE、单步和 DPT 调试接口。
   - 支持 1 到 8 字节断点长度；AArch64 执行断点会按硬件要求修正为 4 字节。
   - 共享结构容纳 16 个断点配置，每个配置最多保存 16 个不同 PC 的命中记录；硬件实际安装数量还受可用 BRP/WRP 限制。
   - 记录包含命中次数、PC、LR、SP、PSTATE、X0-X29、orig_x0、syscallno、FPSR、FPCR、Q0-Q31。
   - 每个寄存器都有 2-bit mask，可选择不操作、读取现场到记录、或把记录值写回现场。

5. [ARM64 inline hook 框架](#10-inline-hook-框架)
   - 在模块 `.text` 中预留跳板槽位，用于安装运行时 hook。
   - 通过 `kallsyms_lookup_name` 找目标符号，通过 `aarch64_insn_patch_text_nosync` patch 代码。
   - 供断点、调度、进程退出和监控功能安装与移除 hook。

6. [共享内存连接与线程调度](#3-共享内存协议)
   - 用户态进程名设置为 `LS` 后，驱动连接线程会将固定地址 `0x2025827000` 的共享内存页 pin 住并 `vmap` 到内核。
   - 调度线程轮询共享结构的 `kernel` / `user` 状态位并按 `op` 分发请求。
   - 线程运行细节见 [线程模型](#4-线程模型)。

7. [进程退出清理与模块隐藏](#9-模块可见性与生命周期处理)
   - 通过进程退出 hook，在 `LS` 线程组最后一个线程退出时清理输入、断点和监控状态。
   - 模块和进程可见性处理分别位于模块入口、任务隐藏和 KGSL 相关实现中。

8. [用户态封装和辅助工具](#11-用户态封装要点)
   - [android/jni/driver/driver.h](android/jni/driver/driver.h) 提供共享内存通信和 C++ 接口。
   - Android 内存工具提供内存搜索、地址查看、反汇编和模块 dump，HTTP 服务供桌面客户端与 MCP 服务调用。

9. 虚拟传感器与进程监控
   - 陀螺仪与 GNSS 数据上报。
   - 指定进程的系统调用、`CNTVCT_EL0` 读取监控。
   - 查询线程的 TLS 与 PACGA 环境参数。

用户态封装主要接口：

- `Read<T>` / `Read(address, buffer, size)` / `Write(...)`
- `ReadString`
- `GetPid` / `SetGlobalPid`
- `GetMemoryInfoRef` / `GetModuleAddress` / `GetScanRegions`
- `DumpMemory`，按模块名或 `start-end` 地址范围导出内存到 `.bin` 文件
- `GetHwbpInfoRef` / `SetProcessHwbpRef` / `RemoveProcessHwbpRef`
- `SetProcessPtebpRef` / `SetProcessStepbpRef` / `SetProcessDptdbgRef`，以及对应的 `RemoveProcess...Ref`
- `TouchDown` / `TouchMove` / `TouchUp` / `GyroReport` / `GnssReport`
- `StartSyscallMonitor` / `StopSyscallMonitor` / `StartCntvctMonitor` / `StopCntvctMonitor`
- `GetEnvParams` / `GetEnvParamsRef`

---

## 2. 代码结构

| 位置 | 内容 |
| --- | --- |
| [lsdriver/lsdriver.c](lsdriver/lsdriver.c) | 模块入口、共享内存连接、请求分发、进程退出处理 |
| [lsdriver/io_struct.h](lsdriver/io_struct.h) | 共享协议、内存布局和断点记录结构 |
| [lsdriver/virtual_memory_rw.h](lsdriver/virtual_memory_rw.h) | 地址翻译和跨页内存读写 |
| [lsdriver/virtual_memory_enum.h](lsdriver/virtual_memory_enum.h) | VMA 枚举、模块区段整理和扫描区域筛选 |
| [lsdriver/virtual_input.h](lsdriver/virtual_input.h) | 虚拟触摸 |
| [lsdriver/virtual_gyro.h](lsdriver/virtual_gyro.h)、[lsdriver/virtual_gnss.h](lsdriver/virtual_gnss.h) | 陀螺仪与定位上报 |
| [lsdriver/break_point.h](lsdriver/break_point.h) | 断点公共接口和命中记录 |
| [lsdriver/arm64_hwdbg.h](lsdriver/arm64_hwdbg.h) | 硬件断点与观察点 |
| [lsdriver/arm64_ptedbg.h](lsdriver/arm64_ptedbg.h)、[lsdriver/arm64_stepdbg.h](lsdriver/arm64_stepdbg.h)、[lsdriver/arm64_dptdbg.h](lsdriver/arm64_dptdbg.h) | PTE、单步和 DPT 调试 |
| [lsdriver/arm64_reg.h](lsdriver/arm64_reg.h)、[lsdriver/inline_hook_frame.h](lsdriver/inline_hook_frame.h)、[lsdriver/export_fun.h](lsdriver/export_fun.h) | 寄存器、hook 和内核辅助函数 |
| [lsdriver/arm64_decode](lsdriver/arm64_decode)、[lsdriver/arm64_encode](lsdriver/arm64_encode)、[lsdriver/arm64_emulate](lsdriver/arm64_emulate) | ARM64 指令解码、编码和模拟执行 |
| [lsdriver/arm64_tests](lsdriver/arm64_tests) | 解码器审计与实体设备执行对拍 |
| [lsdriver/network/README.md](lsdriver/network/README.md) | 独立的内核 IPv4 DNS 与 Ping 工具 |
| [android/jni/driver/driver.h](android/jni/driver/driver.h) | 用户态 C++ 驱动封装 |
| [android/jni/src/main.cpp](android/jni/src/main.cpp) | Android 程序入口和本机界面 |
| [android/jni/include/http_server.h](android/jni/include/http_server.h) | Android HTTP 服务 |
| [windows/LuckyStar.py](windows/LuckyStar.py)、[windows/LuckyStarMcp.py](windows/LuckyStarMcp.py)、[windows/http_bridge.py](windows/http_bridge.py) | Python 桌面界面、MCP 服务和 HTTP 桥接 |

---

## 3. 共享内存协议

内核协议定义在 [lsdriver/io_struct.h](lsdriver/io_struct.h)，用户态在 [android/jni/driver/driver.h](android/jni/driver/driver.h) 中保留对应定义。两端的枚举值、字段顺序和结构布局需要一致，修改协议后应一起重新构建。

### 3.1 连接约定

用户态初始化流程：

1. 调用 `prctl(PR_SET_NAME, "LS", 0, 0, 0)` 把进程名设置为 `LS`。
2. 使用 `mmap` 在固定地址 `0x2025827000` 创建 `sizeof(request_obj)` 大小的共享内存，采用 `MAP_FIXED_NOREPLACE`。
3. 清零请求结构，等待内核设置 `user`，收到握手后清除该标志。

内核连接线程流程：

1. 周期遍历进程列表，查找 `task->comm == "LS"` 的进程。
2. 对固定用户地址执行 `get_user_pages_remote`，把共享内存页 pin 住。
3. 通过 `vmap` 把这些页映射成内核可访问虚拟地址，赋给全局 `req`。
4. 将连接进程记入 `ls_process_task`，设置 `req->user = true` 通知用户态连接完成。

连接线程每 2 秒扫描一次。它只维护一个客户端连接，发现启动时间更新的 `LS` 进程后会切换连接，并向尚未退出的旧客户端发送 `SIGKILL`。握手和请求等待都没有超时，启动用户程序前需要先加载驱动。

### 3.2 同步字段

- `kernel`：用户态置 `true`，表示有请求需要内核处理；内核处理前将其清为 `false`。
- `user`：内核置 `true`，表示处理完成或握手完成；用户态等待后将其清为 `false`。

`kernel`、`user`、`op`、`status` 带 `volatile` 限定。用户态使用 `SpinLock` 和 `std::scoped_lock` 串行化请求，内核调度线程逐个处理。`volatile` 约束编译器访问，不等同于跨 CPU 的内存顺序保证。

### 3.3 请求字段

| 字段 | 内容 |
| --- | --- |
| `op` | `enum request_op` 操作码 |
| `status` | 由具体操作返回的状态或成功读写字节数 |
| `tgid` | 目标线程组 ID |
| `vmemrw_info` | `rw_addr`、`size` 和 `user_buffer[0x1000]` |
| `vmem_info` | 模块列表与扫描区域 |
| `vinput_info` | 请求的虚拟槽位数、坐标范围、slot 和触摸坐标 |
| `vgyro_info` | 三轴陀螺仪数据，单位 mrad/s |
| `vgnss_info` | 经纬度，单位为十进制度的 `10^-7` |
| `bp_info` | 断点目标、硬件资源数量、配置与命中记录 |
| `env_info` | 线程名、TLS、PACGA 参数及各自的查询状态 |

### 3.4 操作码

| 操作码 | 用途 |
| --- | --- |
| `request_op_none` | 空调用 |
| `request_op_vmem_read` / `request_op_vmem_write` | 读写目标进程内存 |
| `request_op_vmem_info` | 枚举内存布局 |
| `request_op_touch_init` | 初始化虚拟触摸 |
| `request_op_touch_down` / `request_op_touch_move` / `request_op_touch_up` | 上报触摸事件 |
| `request_op_gyro_init` / `request_op_gyro_report` | 初始化陀螺仪与上报数据 |
| `request_op_gnss_init` / `request_op_gnss_report` | 初始化定位与上报数据 |
| `request_op_hwbp_set` / `request_op_hwbp_remove` | 设置与移除硬件断点 |
| `request_op_ptebp_set` / `request_op_ptebp_remove` | 设置与移除 PTE 断点 |
| `request_op_stepbp_set` / `request_op_stepbp_remove` | 设置与移除单步断点 |
| `request_op_dptdbg_set` / `request_op_dptdbg_remove` | 设置与移除 DPT 调试 |
| `request_op_syscall_monitor_set` / `request_op_syscall_monitor_remove` | 启停系统调用监控 |
| `request_op_cntvct_monitor_set` / `request_op_cntvct_monitor_remove` | 启停 CNTVCT_EL0 读取监控 |
| `request_op_env_get_params` | 查询线程环境参数 |
| `request_op_kernel_exit` | 停止驱动工作线程 |

---

## 4. 线程模型

模块初始化后启动两个内核线程：

- `ConnectThreadFunction`
- `DispatchThreadFunction`

### 4.1 连接线程

`ConnectThreadFunction` 查找名为 `LS` 的进程并建立共享内存映射，跳过已连接的进程和启动时间更早的进程。取页或映射失败时回收本次持有的页和 `mm` 引用。

### 4.2 调度线程

`DispatchThreadFunction` 在 `ls_process_task` 非空时消费请求：

1. 检查 `req->kernel`。
2. 清除 `req->kernel`。
3. 根据 `req->op` 调用对应处理函数。
4. 设置 `req->user = true` 通知用户态。

空闲轮询策略：

- 前 `5000` 轮使用 `cpu_relax()` 忙等，追求低延迟。
- 之后使用 `usleep_range(50, 100)` 降低空闲功耗。
- 尚未连接用户进程时使用 `msleep(2000)` 深睡眠。

连接线程使用低优先级 FIFO 调度，分发线程使用较高优先级 FIFO 调度。

---

## 5. 进程内存读写实现

内存读写由 [lsdriver/virtual_memory_rw.h](lsdriver/virtual_memory_rw.h) 中的 `virtual_memory_rw(op, pid, vaddr, buffer, size)` 处理。它按页拆分访问，通过 `walk_translate_va_to_pa()` 遍历目标页表，再由 `linear_read_physical()` 或 `linear_write_physical()` 访问内核线性映射。

### 5.1 地址翻译

页表遍历支持 PUD、PMD 大页与普通 PTE 映射，并检查物理页帧是否有效。读写入口会去掉地址标签，并缓存最近使用的目标 `mm` 和页地址翻译结果。

同一文件还保留了 `mmu_translate_va_to_pa()` 硬件翻译和 PTE 临时映射读写函数，读写主流程没有选用这两条路径。

### 5.2 读写主流程

单次共享请求的数据缓冲为 `0x1000` 字节，用户态会把更大的读写继续分片。内核按目标页边界处理每一片，遇到失败页后继续处理后续部分。

返回正数表示成功读写的字节数，不一定等于请求长度；没有成功处理任何字节时返回错误状态。大于 8 字节的读取会把失败部分清零，小尺寸读取失败时保留调用方原值。`Read<T>()` 从零初始化的值开始读取，需要判断完整性时使用带缓冲区和返回字节数的 `Read()`。

---

## 6. 进程内存布局枚举

实现位于 [lsdriver/virtual_memory_enum.h](lsdriver/virtual_memory_enum.h)。

入口函数：

- `virtual_memory_enum(tgid, &req->vmem_info)`

输出结构：

- `virtual_memory.module_count`
- `virtual_memory.modules[MAX_MODULES]`
- `virtual_memory.region_count`
- `virtual_memory.regions[MAX_SCAN_REGIONS]`

### 6.1 模块收集

文件模块按 `/data/` 路径前缀收集。匿名 RWX 映射在没有作为模块 BSS 续接时进入伪模块收集分支，有匿名标签的区段按标签归组，无标签的区段使用权限和地址范围命名。`/dev/` 文件映射不作为普通模块来源。

BSS 识别规则：

- 当前 VMA 没有文件映射
- 当前 VMA 带 `VM_WRITE`，不强求 `VM_READ`
- 当前 VMA 与上一段首尾相连
- 上一段属于正在追踪的模块

BSS 区段的 `index` 固定为 `-1`，后续不会参与普通连续 index 编号。

### 6.2 扫描区域收集

扫描区域只收集私有 `rw-p` VMA，并过滤明显无关或噪声较大的区域。

路径前缀黑名单：

- `/dev/`
- `/system/`
- `/vendor/`
- `/apex/`

关键词黑名单：

- `.oat`
- `.art`
- `.odex`
- `.vdex`
- `.dex`
- `.ttf`
- `dalvik`
- `gralloc`
- `ashmem`

匿名区域还会排除：

- 主线程栈
- `[vvar]`
- `[vdso]`
- `[vsyscall]`

用户态 `GetScanRegions()` 会把 `regions[]` 与所有模块 `segs[]` 合并后排序，因此扫描时既能扫匿名读写区域，也能扫模块静态段。

### 6.3 模块区段后处理

枚举完成后，普通模块会做一次区段整理；匿名 RWX 伪模块保留收集结果，不参与下面的聚类和权限规范化：

1. 按虚拟地址排序。
2. 用 16MB 断层阈值做体积聚类，选出模块主体区间。
3. 删除远离主体的诱饵 VMA。
4. 对尾部 BSS 做豁免，允许 BSS 从主体尾部继续延伸。
5. 按拓扑重新标记 RO、RX、RW、BSS。
6. 根据标记反推标准 `prot`，把异常 `-w-` BSS 规范为 RW。
7. 首尾相连且同类的区段进行拉链式合并。
8. 普通区段重新编号为 `0, 1, 2...`，BSS 保持 `-1`。

用户态按区段的 `index` 字段查找 `start` / `end`，而不是把它直接作为数组下标；BSS 的 `index` 为 `-1`。

---

## 7. 虚拟触摸实现

实现位于 [lsdriver/virtual_input.h](lsdriver/virtual_input.h)。初始化时查找多点触摸设备，返回面板的 X/Y 坐标范围，并在 10 个槽位中分配物理与虚拟触摸。

虚拟槽位数为 `Vslot` 时，物理部分占前 `10 - Vslot` 个槽位，虚拟部分占后面的槽位。启用时 `Vslot` 可取 1 到 9，传 0 表示不启用。调用方使用 `0` 到 `Vslot - 1` 的虚拟编号，驱动负责转换为设备槽位；例如分配 5 个虚拟槽位时，虚拟编号 0 对应设备 slot 5。

按下和移动由 `TouchDown(slot, x, y, screenW, screenH)`、`TouchMove(...)` 上报，抬起使用 `TouchUp(slot)`。用户态将屏幕坐标换算到面板坐标，内核维护各虚拟手指的 tracking ID，并根据物理与虚拟手指总数更新 `BTN_TOUCH` 等全局按键。

销毁时会抬起仍按下的虚拟手指，恢复全局按键和 10 槽位范围，再释放设备引用。

---

## 8. ARM64 断点

断点共用 [lsdriver/io_struct.h](lsdriver/io_struct.h) 中的 `break_point`、`bp_point` 和 `bp_record`。用户态通过 `std::span<const bp_point>` 传入配置，目标进程由 `SetGlobalPid()` 指定。

### 8.1 配置与记录

`break_point` 保存目标 `tgid`、硬件资源数量和 `points[BP_CONFIG_MAX]`。每个 `bp_point` 包含地址 `hit_addr`、类型 `bt`、长度 `bl`、线程范围 `bs`，以及按 PC 聚合的命中记录。

`BP_CONFIG_MAX` 和 `BP_RECORD_MAX` 均为 `0x10`，即最多容纳 16 个配置，每个配置最多记录 16 个不同 PC。记录保存命中次数、通用寄存器、系统调用现场和 FP/SIMD 寄存器。新记录的寄存器掩码默认为读取，后续按每个寄存器的 2-bit mask 处理：

| 掩码 | 行为 |
| --- | --- |
| `BP_OP_NONE` | 保持不变 |
| `BP_OP_READ` | 从命中现场读取到记录 |
| `BP_OP_WRITE` | 将记录中的值写回现场 |

`bs` 定义主线程、其他线程、全部线程三个范围。各断点实现对类型、长度和线程范围的支持不同，不能仅凭共享枚举推断所有组合都可用。

### 8.2 硬件断点与观察点

[lsdriver/arm64_hwdbg.h](lsdriver/arm64_hwdbg.h) 处理执行断点和读写观察点，通过 `breakpoint_handler`、`watchpoint_handler` 和 `finish_task_switch` 的 hook 配合调度安装寄存器。

目标线程组切入后，从 CPU 的空闲 BRP/WRP 槽位中分配资源，避开 perf 已占用的槽位；离开目标线程组时清理对应的自定义配置。因此，16 个共享配置槽不代表同时拥有 16 个硬件断点。硬件安装路径按 TGID 匹配线程组，没有按每个配置的 `bs` 再筛选。

类型支持 `X`、`R`、`W`、`RW`，长度按 ARM64 断点寄存器要求编码，AArch64 执行断点按 4 字节处理。资源数量通过设置请求写入 `bp_info.num_brps` 和 `bp_info.num_wrps`。

### 8.3 其他断点接口

| 方式 | 设置与移除接口 | 实现 |
| --- | --- | --- |
| PTE | `SetProcessPtebpRef` / `RemoveProcessPtebpRef` | [lsdriver/arm64_ptedbg.h](lsdriver/arm64_ptedbg.h) |
| 单步 PC | `SetProcessStepbpRef` / `RemoveProcessStepbpRef` | [lsdriver/arm64_stepdbg.h](lsdriver/arm64_stepdbg.h) |
| 主线程 UDF shadow-PGD | `SetProcessDptdbgRef` / `RemoveProcessDptdbgRef` | [lsdriver/arm64_dptdbg.h](lsdriver/arm64_dptdbg.h) |

这些接口复用同一块 `bp_info` 配置和记录。切换断点方式前先移除上一种配置，再设置新的配置。

---

## 9. 模块可见性与生命周期处理

模块入口位于 [lsdriver/lsdriver.c](lsdriver/lsdriver.c)。初始化后建立工作线程并安装异常、退出 hook；模块可见性由 `hide_myself()` 处理，任务和 KGSL 相关处理分别位于 [lsdriver/hide_task.h](lsdriver/hide_task.h) 与 [lsdriver/hide_kgsl.h](lsdriver/hide_kgsl.h)。

`arm64_force_sig_fault`、`do_group_exit` 和 `do_exit` 用于记录异常与退出过程。资源清理放在 `taskstats_exit` 的 `group_dead` 分支，只有线程组最后一个线程退出时才执行，不会因为主线程提前结束就清理仍在使用的状态。

任意被监控进程退出时会移除其系统调用与 CNTVCT 监控。进程名精确匹配 `LS` 时，还会释放虚拟触摸、陀螺仪、GNSS、各类断点和剩余监控状态，并将 `ls_process_task` 清空。

`ExitKernel()` 停止两个驱动工作线程，不等同于卸载模块。模块会从常规模块列表中隐藏，不能以 `lsmod` 没有记录来判断是否加载成功；客户端完成握手才表示共享连接已经建立。

---

## 10. Inline Hook 框架

[lsdriver/inline_hook_frame.h](lsdriver/inline_hook_frame.h) 提供 ARM64 函数入口 hook，供断点、调度、进程退出和监控功能共用。框架管理目标地址、原始指令、工作函数和跳板槽位，跳板位于模块代码段。

调用方使用 `HOOK_ENTRY()` 声明 `hook_entry` 数组，再通过 `inline_hook_install(entries)` 和 `inline_hook_remove(entries)` 批量安装、移除。安装失败会回滚本次已经安装的项，移除时恢复原入口指令。

跳板保存通用寄存器和状态后调用工作函数，再按处理结果恢复现场、继续原函数或转到指定位置。具体帧布局与指令重放由框架统一维护。

---

## 11. 用户态封装要点

接口位于 [android/jni/driver/driver.h](android/jni/driver/driver.h)。

### 11.1 通信

构造函数为 `Driver(int Vslot, bool initGyro, bool initGnss)`。先建立共享连接，再按参数初始化虚拟触摸、陀螺仪与 GNSS。只需要内存或断点功能时，使用 `Driver(0, false, false)`。

`IoCommitAndWait()` 设置 `kernel`，循环等待 `user`，完成后清除 `user`。请求由 `SpinLock` 保护，锁的实现使用 ARM64 LSE 指令，因此运行设备还需要支持对应指令。

### 11.2 内存与模块辅助

先通过 `GetPid(packageName)` 查找进程，再用 `SetGlobalPid(pid)` 设置内存和断点操作的目标。

- `Read<T>()`、`Read()`、`Write()` 提供模板或缓冲区读写。
- `GetMemoryInfoRef()` 请求内存布局并返回共享结果的引用。
- `GetModuleAddress(moduleName, segmentIndex, outAddress, isStart)` 按模块名和区段 `index` 查询起止地址。
- `GetScanRegions()` 汇总扫描区域和模块区段，按地址排序。
- `DumpMemory()` 支持模块名和半开地址范围 `[start, end)`，导出到 `/sdcard/dump/`，单次最多 500MB，失败的读取块以零填充。

### 11.3 触摸辅助

`TouchDown` 和 `TouchMove` 接收虚拟 slot、屏幕坐标及屏幕尺寸，内部转换为触摸面板坐标；`TouchUp` 只需要 slot。

`GyroReport(x, y, z)` 使用 mrad/s，`GnssReport(latitude_e7, longitude_e7)` 使用放大 `10^7` 倍的经纬度整数。使用前需在构造参数中启用对应设备。

### 11.4 断点与监控

`SetProcessHwbpRef(points)` 设置硬件断点，`RemoveProcessHwbpRef()` 移除配置。PTE、单步和 DPT 接口使用相同的配置结构。

`GetHwbpInfoRef()` 直接返回共享的 `bp_info` 引用，不发起刷新请求，也不是独立快照。命中记录会由内核继续更新。

`StartSyscallMonitor(pid)` / `StopSyscallMonitor(pid)` 和 `StartCntvctMonitor(pid)` / `StopCntvctMonitor(pid)` 分别管理两类进程监控。系统调用记录输出到内核日志，不从共享请求返回日志正文。

`GetEnvParams(threadName)` 查询已选目标进程的线程环境，结果通过 `GetEnvParamsRef()` 读取，其中 TLS 和 PACGA 各有独立状态字段。

---

## 12. 编译与运行

### 12.1 内核模块

在 Linux 或 WSL 的 Bash 中使用 [build_all.sh](build_all.sh)。脚本从 `/root` 下查找各版本 Android 内核源码，驱动源码路径由脚本顶部的 `DRIVER_SRC` 指定，使用前按本机目录调整 `KERNELS_ROOT` 和 `DRIVER_SRC`。

脚本提供以下构建目标：

| 目标 | 构建方式 |
| --- | --- |
| `5.10-Android12` | Legacy |
| `5.10-Android13`、`5.15-Android13` | Bazel 准备内核环境，再构建外部模块 |
| `6.1-Android14`、`6.6-Android15` | Bazel 准备内核环境，再构建外部模块 |
| `6.12-Android16`、`6.18-Android17` | Bazel 准备内核环境，再构建外部模块 |

在仓库根目录构建一个目标：

```bash
bash build_all.sh 6.1-Android14
```

不传参数会依次构建所有目标。脚本先询问是否剥离调试符号；6.6、6.12、6.18 目标保留符号。产物以版本名保存到驱动目录，随后自动调用 [packer.sh](packer.sh) 生成 [install_driver.sh](install_driver.sh)，并推进 [install_driver.version](install_driver.version) 中的下一次打包版本号。只构建一个目标时，安装包仍会收集目录里已有的其他版本模块。

已经准备好目标内核及其输出目录时，也可以从仓库根目录直接使用 Kbuild：

```bash
make -C <KDIR> O=<KOUT> M="$PWD/lsdriver" ARCH=arm64 LLVM=1 LLVM_IAS=1 modules
```

`KDIR` 为内核源码目录，`KOUT` 为对应构建输出目录，Clang 工具链需在 `PATH` 中。源码内构建可省略 `O`。模块与设备的内核配置、符号及 ABI 需要匹配，版本名称相同并不代表所有厂商内核都能加载。

[lsdriver/Makefile](lsdriver/Makefile) 将主驱动、编码器、解码器和模拟执行器链接为一个模块，使用 GNU11、`-O3` 和 Full LTO。指令集参数为 `-march=armv8.6-a+bf16+i8mm+fp16fml+fp16+lse+rcpc+crc`，关闭自动向量化；显式使用 FP/SIMD 的目标单独移除 `-mgeneral-regs-only` 并添加 `-mno-implicit-float`。

KASAN、UBSAN、KCSAN、KMSAN、GCOV、KCOV 插桩在模块构建中关闭，KCFI 保留。架构参数用于编译汇编模板，设备是否能够执行某项扩展指令仍取决于其 CPU。

DDK不在我维护范围，DDK所编译的模块在部分设备上无法加载模块OnePlus/PMB110

使用 DDK 环境时可参考https://github.com/Ylarod/ddk.git和 [ddk_build_all.sh](ddk_build_all.sh)，其中工具链、内核目录、目标名称和产物命名与主脚本不同，需要按该环境配置。


### 12.2 Android 程序

Android 端使用 NDK 构建 ARM64 可执行程序 `LS_KTool`，不是 APK。[android/jni/Application.mk](android/jni/Application.mk) 指定 `arm64-v8a`、API 26 和静态 libc++，[android/jni/Android.mk](android/jni/Android.mk) 使用 C++26。

Windows 下在仓库根目录执行：

```powershell
.\build_all_ndk.ps1 -NdkBuild 'E:\android-ndk-r29\ndk-build.cmd' -Jobs 12
```

将 `-NdkBuild` 改为本机 NDK 路径。脚本会下载并校验内嵌的 ARM64 cloudflared 资源，再调用 `ndk-build`。

构建产物位于 `android/libs/arm64-v8a/LS_KTool`。默认构建目标会通过 ADB 推送到设备，再用 `su` 安装到 `/data/akernel/LS_KTool`，因此需要将 `adb` 加入 `PATH`，连接已授权且可使用 root shell 的设备。连接多台设备时，可先设置 `ANDROID_SERIAL` 选择目标。

### 12.3 加载与启动

先将安装脚本传到设备：

```powershell
adb push install_driver.sh /data/local/tmp/install_driver.sh
adb shell
```

在设备 shell 中执行：

```sh
su
sh /data/local/tmp/install_driver.sh
cd /data/akernel
./LS_KTool
```

安装脚本按设备内核选择内嵌模块并调用 `insmod`。加载前会清空内核日志缓冲，需要保留先前日志时应先导出。

程序启动后选择模式：

| 模式 | 用途 |
| --- | --- |
| `0` | 停止驱动工作线程 |
| `1` | 内存读写测试 |
| `2` | 触摸测试 |
| `3` | 本机内存工具 |
| `4` | HTTP 服务 |
| `5` | 陀螺仪测试 |
| `6` | 定位测试 |

选择后程序转入后台，标准输出与错误输出追加到 `/storage/emulated/0/log.txt`。HTTP 模式监听端口 `9494`，RPC 路径为 `/api/rpc`；服务提供进程调试操作，应只向受信任的客户端开放。

### 12.4 Python 客户端

先在 Android 端启动模式 `4`，再运行 [windows/LuckyStar.py](windows/LuckyStar.py)：

```powershell
python -m pip install PySide6
python windows/LuckyStar.py
```

连接由 [windows/http_bridge.py](windows/http_bridge.py) 处理，支持设备地址和 HTTP(S) 隧道地址。

[windows/LuckyStarMcp.py](windows/LuckyStarMcp.py) 将同一套接口提供为 Streamable HTTP MCP 服务，依赖提供 `mcp.server.MCPServer` 的 `mcp` 包和 `pydantic`。启动示例：

```powershell
python windows/LuckyStarMcp.py --android-host auto
```

`--android-host` 可指定设备 IP、完整隧道 URL，或使用 `auto` 发现设备。MCP 监听位置由 `--mcp-host`、`--mcp-port`、`--mcp-path` 设置，连接地址会在启动时输出。

需要打包 Python 可执行文件时使用 [windows/build_executables.py](windows/build_executables.py)，它依赖 PyInstaller，支持指定要打包的脚本：

```powershell
python -m pip install pyinstaller typer
python windows/build_executables.py windows/LuckyStar.py
```

---

## 13. 测试

解码器测试在主机上运行，不执行目标 ARM64 指令。在 Linux 或 WSL 中，从仓库根目录执行：

```bash
make -C lsdriver/arm64_tests/decoder contract-test
make -C lsdriver/arm64_tests/decoder strict-test
```

`contract-test` 检查输出契约和 CAS/CASP 组合，`strict-test` 使用 Android clang-r487747c / LLVM 17.0.2 做逐项审计。默认工具链位置及固定语料基线见 [lsdriver/arm64_tests/decoder/Makefile](lsdriver/arm64_tests/decoder/Makefile)，判定规则和带日期的结果见 [lsdriver/arm64_tests/decoder/VALIDATION.md](lsdriver/arm64_tests/decoder/VALIDATION.md)。

模拟执行器测试需要实体 ARM64 设备、root 和匹配内核的测试模块。先在 Linux 或 WSL 中构建：

```bash
make -C lsdriver/arm64_tests/executor VERSION=6.1-Android14 module
make -C lsdriver/arm64_tests/executor device-binaries
```

再在 Windows PowerShell 中运行：

```powershell
.\lsdriver\arm64_tests\run-arm64-executor-test-device.ps1
```

脚本支持用 `-Serial` 选择设备。对拍、跳过项目和清理结果的判定见 [lsdriver/arm64_tests/executor/KERNEL_EXECUTOR_VALIDATION.md](lsdriver/arm64_tests/executor/KERNEL_EXECUTOR_VALIDATION.md)。修改共享指令语料后，需要重新构建测试模块和 runner。
