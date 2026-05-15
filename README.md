# 内存调试分析驱动

> 仅供技术研究与学习，严禁用于非法用途。作者不承担任何违法责任。

交流群：
QQ:1092055800
TG:https://t.me/+ArHIx-Km9jkxNjZl  


---

## 1. 功能总览

`lsdriver` 是一个运行在 Android ARM64 内核中的内存调试与输入辅助模块。它不创建字符设备，也不通过 `ioctl`、`netlink`、`procfs` 或 `sysfs` 暴露命令接口；用户态程序通过固定地址共享内存与内核线程通信。

当前驱动侧提供这些核心能力。点击功能标题可以跳到对应的技术实现说明：

1. [进程内存读写](#5-进程内存读写实现)
   - 对指定 `pid` 的用户虚拟地址执行读写。
   - 内核侧自动跨页拆分，用户态封装侧对超过 `0x1000` 字节的大块读写继续分片，避免覆盖共享缓冲。
   - 读大块内存时遇到失败页会跳过并对失败块清零，只要成功处理过字节就返回成功字节数。

2. [进程内存布局枚举](#6-进程内存布局枚举)
   - 返回模块列表 `modules[]`，每个模块包含多个区段 `segs[]`，区段带 `index`、`prot`、`start`、`end`。
   - 返回可扫描内存区域 `regions[]`，主要用于指针扫描、特征扫描和通用内存搜索。
   - 模块收集包含 `/data/` 与 `/dev/` 前缀，支持 `/dev/zero (deleted)` 这类动态代码映射。
   - 识别紧邻模块尾部的匿名可写 BSS，并兼容被改成 `-w-p` 的 BSS。
   - 对 VMA 碎裂、远端诱饵段、权限污染进行后处理，尽量给用户态提供稳定的模块基址和区段索引。

3. [虚拟触摸注入](#7-虚拟触摸实现)
   - 自动查找多点触摸 input 设备并劫持 `input_dev->mt`。
   - 将虚拟手指固定放在 slot 9，物理手指保留在 slot 0 到 slot 8。
   - 支持 `TouchDown`、`TouchMove`、`TouchUp`，并返回触摸设备原始坐标范围。
   - 手动维护 `BTN_TOUCH`、`BTN_TOOL_FINGER`、`BTN_TOOL_DOUBLETAP`，降低真实触摸与虚拟触摸互相污染的概率。

4. [ARM64 硬件断点/观察点](#8-arm64-硬件断点实现)
   - 读取 CPU 支持的 BRP/WRP 数量。
   - 支持执行断点 `X` 与访问观察点 `R`、`W`、`RW`。
   - 支持 1 到 8 字节断点长度；AArch64 执行断点会按硬件要求修正为 4 字节。
   - 命中后按触发 PC 聚合记录，保存命中次数、PC、LR、SP、PSTATE、X0-X29、FPSR、FPCR、Q0-Q31。
   - 每个寄存器都有 2-bit mask，可选择不操作、读取现场到记录、或把记录值写回现场。

5. [ARM64 inline hook 框架](#10-inline-hook-框架)
   - 在模块 `.text` 中预留跳板槽位，用于安装运行时 hook。
   - 通过 `kallsyms_lookup_name` 找目标符号，通过 `aarch64_insn_patch_text_nosync` patch 代码。
   - 当前用于接管 `breakpoint_handler` 与 `watchpoint_handler`，把硬件断点/观察点命中转发到驱动自己的回调。
   - 跳板会保存/恢复通用寄存器和 NZCV，调用工作函数后执行被覆盖的原指令，再返回目标函数入口后的下一条指令。

6. [共享内存连接与线程调度](#3-共享内存协议)
   - 用户态进程名设置为 `LS` 后，驱动连接线程会将固定地址 `0x2025827000` 的共享内存页 pin 住并 `vmap` 到内核。
   - 调度线程轮询共享结构的 `kernel` / `user` 状态位并按 `op` 分发请求。
   - 线程运行细节见 [线程模型](#4-线程模型)。

7. [进程退出清理与模块隐藏](#9-模块可见性与生命周期处理)
   - 驱动注册 `do_exit` kprobe，检测 `LS` 相关主线程退出时释放缓存状态并销毁虚拟触摸。
   - 模块初始化时会隐藏模块链表、sysfs 模块对象、部分 vmalloc 信息和工作线程链表节点。

8. [用户态封装和辅助工具](#11-用户态封装要点)
   - `Android-LS/include/DriverMemory.h` 在这些基础能力上提供 C++ 封装和上层工具。
   - 包括读写模板、模块地址查询、扫描区域合并、模块 dump、硬件断点记录管理和特征扫描。

用户态封装主要接口：

- `Read<T>` / `Read(address, buffer, size)` / `Write(...)`
- `ReadString` / `ReadWString`
- `GetPid` / `SetGlobalPid`
- `GetMemoryInformation` / `GetModuleAddress` / `GetScanRegions`
- `DumpModule`，按枚举出的模块跨度 dump 并修复部分 ELF Program Header 和 Dynamic 表
- `GetHwbpInfoRef` / `SetProcessHwbpRef` / `RemoveProcessHwbpRef` / `RemoveHwbpRecord`
- `SignatureScanner`，基于驱动读内存和扫描区域完成特征生成、过滤和扫描

---

## 2. 代码结构

`lsdriver/` 目录下核心文件：

- `lsdriver.c`：模块入口、连接线程、调度线程、进程退出监听、模块/线程隐藏逻辑
- `io_struct.h`：共享内存协议定义，包括操作码、请求结构、内存信息结构、硬件断点记录结构
- `physical.h`：进程虚拟地址到物理地址翻译、物理内存读写、跨页读写主流程
- `process_memory_enum.h`：进程 VMA 枚举、模块收集、扫描区过滤、模块区段反诱饵后处理
- `virtual_input.h`：虚拟触摸初始化、slot 劫持、虚拟触摸事件上报与销毁
- `hwbp.h`：硬件断点用户可见接口、命中现场记录/回放逻辑
- `arm64_debug_monitor.h`：sched_switch 监听、debug monitor inline hook、BVR/BCR/WVR/WCR 写入与清理
- `arm64_reg.h`：ARM64 调试寄存器、FP/SIMD 寄存器读写辅助
- `inline_hook_frame.h`：ARM64 inline hook 跳板框架，负责保存原指令、构造跳板、patch 目标入口和卸载恢复
- `export_fun.h`：`kallsyms_lookup_name` 获取、CFI 绕过、页表辅助函数
- `Makefile`：模块编译参数

`Android-LS/include/DriverMemory.h` 是配套用户态 C++ 封装，负责共享内存映射、握手、同步、自旋锁、读写分片、坐标换算和上层辅助功能。

---

## 3. 共享内存协议

协议结构定义在 `io_struct.h` 的 `struct req_obj`。

### 3.1 连接约定

用户态初始化流程：

1. 调用 `prctl(PR_SET_NAME, "LS", 0, 0, 0)` 把进程名设置为 `LS`。
2. 使用 `mmap` 在固定地址 `0x2025827000` 创建 `sizeof(req_obj)` 大小的共享内存。
3. 清零 `req_obj` 后调用一次 `IoCommitAndWait()` 等待驱动握手。

内核连接线程流程：

1. 周期遍历进程列表，查找 `task->comm == "LS"` 的进程。
2. 对固定用户地址执行 `get_user_pages_remote`，把共享内存页 pin 住。
3. 通过 `vmap` 把这些页映射成内核可访问虚拟地址，赋给全局 `req`。
4. 设置 `ProcessExit = true`，再设置 `req->user = true` 通知用户态连接完成。

`get_user_pages_remote` 对 5.10、5.15、6.1、6.5、6.12 的参数签名做了条件编译适配。

### 3.2 同步字段

- `kernel`：用户态置 `true`，表示有请求需要内核处理；内核处理前将其清为 `false`。
- `user`：内核置 `true`，表示处理完成或握手完成；用户态等待后将其清为 `false`。

当前代码中 `kernel` / `user` 是普通 `bool` 字段，不是 `atomic_t`。用户态封装用 `SpinLock` 串行化对共享请求结构的访问，内核调度线程单线程消费请求。

### 3.3 请求字段

- `op`：操作码，类型为 `enum sm_req_op`
- `status`：返回状态或成功处理的字节数
- `pid`：目标进程 PID
- `target_addr`：目标进程虚拟地址
- `size`：读写长度
- `user_buffer[0x1000]`：单次读写数据缓冲
- `mem_info`：模块和扫描区域枚举结果
- `bt`：硬件断点类型
- `bl`：硬件断点长度
- `bs`：硬件断点线程范围配置字段
- `bp_info`：硬件断点资源信息和命中记录
- `POSITION_X` / `POSITION_Y`：触摸设备原始坐标范围
- `x` / `y`：触摸事件坐标

### 3.4 操作码

- `op_o`：空调用，用于握手或探活
- `op_r`：读目标进程内存
- `op_w`：写目标进程内存
- `op_m`：枚举目标进程内存布局
- `op_down`：虚拟触摸按下
- `op_move`：虚拟触摸移动
- `op_up`：虚拟触摸抬起
- `op_init_touch`：初始化虚拟触摸并返回触摸坐标范围
- `op_brps_weps_info`：读取 CPU BRP/WRP 数量
- `op_set_process_hwbp`：设置硬件断点/观察点
- `op_remove_process_hwbp`：删除当前硬件断点/观察点监控
- `op_kexit`：结束内核工作线程循环

---

## 4. 线程模型

模块初始化后启动两个内核线程：

- `ConnectThreadFunction`
- `DispatchThreadFunction`

### 4.1 连接线程

连接线程只在 `ProcessExit == false` 时扫描进程。找到 `LS` 后，它 pin 住共享内存页、`vmap` 到内核、设置连接状态，然后每 2 秒继续检查一次状态。

如果 `get_user_pages_remote` 失败，会通过动态解析到的 `release_pages` 回收已经 pin 的页，避免直接内联 `put_page()` 引入 Android `page_pinner` 静态键依赖。

### 4.2 调度线程

调度线程只在 `ProcessExit == true` 时消费请求：

1. 检查 `req->kernel`。
2. 关闭 DAIF 中断位，降低请求处理期间被打断的概率。
3. 清除 `req->kernel`。
4. 根据 `req->op` 分发到内存读写、内存枚举、触摸、硬件断点或退出逻辑。
5. 设置 `req->user = true` 通知用户态。
6. 恢复 DAIF。

空闲轮询策略：

- 前 `5000` 轮使用 `cpu_relax()` 忙等，追求低延迟。
- 之后使用 `usleep_range(50, 100)` 降低空闲功耗。
- 尚未连接用户进程时使用 `msleep(2000)` 深睡眠。

---

## 5. 进程内存读写实现

实现文件：`physical.h`

入口函数：

- `read_process_memory(pid, vaddr, buffer, size)`
- `write_process_memory(pid, vaddr, buffer, size)`
- 两者最终都进入 `_process_memory_rw(...)`

当前实际启用路径：

1. 使用 `mmu_translate_va_to_pa(...)` 把目标进程用户虚拟地址翻译为物理地址。
2. 使用 `linear_read_physical(...)` / `linear_write_physical(...)` 通过内核线性映射访问物理内存。

保留但当前未启用的路径：

- `walk_translate_va_to_pa(...)`：手动走用户页表获取 PFN。
- `pte_read_physical(...)` / `pte_write_physical(...)`：复用一页 `vmalloc` 空间，改写其 PTE 来映射任意物理页。

### 5.1 地址翻译

`mmu_translate_va_to_pa(...)` 的核心流程：

1. 读取并保存当前 `DAIF` 和 `TTBR0_EL1`。
2. 关闭 D/A/I/F 中断位。
3. 将目标进程 `mm->pgd` 的物理地址编码成新的 `TTBR0_EL1`，兼容 LPA2 场景下的 52 位物理地址格式。
4. 执行 `AT S1E0R, va` 触发硬件翻译。
5. 读取 `PAR_EL1`，检查失败位。
6. 从 `PAR_EL1[51:12]` 还原物理页地址，并拼回页内偏移。
7. 使用 `tlbi vaae1is` 清理本次 ASID=0 翻译带来的 TLB 污染。
8. 恢复原 `TTBR0_EL1` 和 `DAIF`。

这条路径依赖硬件翻译结果，避免手动页表遍历时对页表层级、折叠层级和内核版本差异做大量分支。

### 5.2 读写主流程

`_process_memory_rw(...)` 做了这些处理：

- 缓存上一次使用的 `mm_struct`：`s_last_pid` / `s_last_mm`
- 缓存上一次翻译的页：`s_last_vpage_base` / `s_last_ppage_base`
- 按页拆分跨页访问
- 对 1、2、4、8 字节读写使用固定大小 `__builtin_memcpy`
- 对大块读取失败页清零并继续后续页

返回值语义：

- 成功处理过字节：返回成功处理的字节数
- 完全失败：返回最后一次失败错误码
- 参数错误：返回 `-EINVAL`
- 找不到进程或 task：返回 `-ESRCH`

用户态 `DriverMemory.h` 还会对超过 `0x1000` 的读写进行二次分片，因为共享结构中的 `user_buffer` 固定为一页大小。

---

## 6. 进程内存布局枚举

实现文件：`process_memory_enum.h`

入口函数：

- `enum_process_memory(pid, &req->mem_info)`

输出结构：

- `memory_info.module_count`
- `memory_info.modules[MAX_MODULES]`
- `memory_info.region_count`
- `memory_info.regions[MAX_SCAN_REGIONS]`

### 6.1 模块收集

模块来源规则：

- 文件映射路径前缀命中 `/data/`
- 文件映射路径前缀命中 `/dev/`

保留 `/dev/` 的原因是部分保护或动态代码会通过 `/dev/zero (deleted)`、共享映射、手动 loader、JIT 或跳板方式承载代码。这样用户态可以把这类动态代码区域也按模块段处理、dump 或纳入扫描范围。

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

枚举完成后，每个模块会做一次区段清洗：

1. 按虚拟地址排序。
2. 用 16MB 断层阈值做体积聚类，选出模块主体区间。
3. 删除远离主体的诱饵 VMA。
4. 对尾部 BSS 做豁免，允许 BSS 从主体尾部继续延伸。
5. 按拓扑重新标记 RO、RX、RW、BSS。
6. 根据标记反推标准 `prot`，把异常 `-w-` BSS 规范为 RW。
7. 首尾相连且同类的区段进行拉链式合并。
8. 普通区段重新编号为 `0, 1, 2...`，BSS 保持 `-1`。

最终用户态可以通过 `module.segs[index].start/end` 获得更稳定的模块基址、代码段、数据段和 BSS 范围。

---

## 7. 虚拟触摸实现

实现文件：`virtual_input.h`

核心常量：

- `VTOUCH_TRACKING_ID_BASE = 40000`
- `TARGET_SLOT_IDX = 9`
- `PHYSICAL_SLOTS = 9`
- `TOTAL_SLOTS = 10`

初始化流程：

1. 通过 `generic_kallsyms_lookup_name("input_class")` 获取 input class。
2. 使用 `class_for_each_device` 查找具备 `EV_ABS`、`ABS_MT_SLOT`、`BTN_TOUCH` 和 `input->mt` 的触摸设备。
3. `get_device` 持有设备引用。
4. 分配新的 `input_mt`，至少保留 10 个 slot 的存储空间。
5. 复制旧 MT 状态，保存 `original_mt`，替换 `dev->mt`。
6. 对物理驱动暴露 `num_slots = 9`，对 Android 上报 `ABS_MT_SLOT` 支持到 10 个 slot。
7. 缓存设备是否支持 `ABS_MT_TOUCH_MAJOR`、`ABS_MT_WIDTH_MAJOR`、`ABS_MT_PRESSURE`。

事件上报流程：

1. 使用 `local_irq_save` 缩小真实触摸中断与虚拟注入交错的窗口。
2. 保存当前 `ABS_MT_SLOT`。
3. 临时把 `mt->num_slots` 切到 10。
4. 选择 slot 9，调用 `input_mt_report_slot_state` 上报虚拟手指状态。
5. 按下或移动时上报 X/Y，并在设备支持时伪造面积和压力。
6. 切回 `mt->num_slots = 9`。
7. 恢复原 slot。
8. 手动更新全局按键并 `input_sync`。
9. 恢复中断。

全局按键处理：

- 虚拟按下后会临时清除设备 `keybit` 中的 `BTN_TOUCH`、`BTN_TOOL_FINGER`、`BTN_TOOL_DOUBLETAP`，避免真实手指快速抬起把虚拟滑动打断。
- 虚拟抬起或销毁时恢复这些 keybit。
- `update_global_keys()` 会统计 0 到 8 号物理 slot 和虚拟 slot 状态，重新上报全局触摸按键。

销毁流程：

- 如果虚拟手指仍处于按下状态，先发送抬起。
- 恢复原始 `input_mt` 和原始 `ABS_MT_SLOT` 范围。
- 释放劫持的 `input_mt` 及 `red` 矩阵。
- `put_device` 释放触摸设备引用。

---

## 8. ARM64 硬件断点实现

主要文件：

- `hwbp.h`
- `arm64_debug_monitor.h`
- `arm64_reg.h`
- `inline_hook_frame.h`

当前实现不再走 `register_user_hw_breakpoint` / `unregister_hw_breakpoint` 的 perf 断点链路。旧 perf 方案仍保留在注释中，实际启用的是调度监听加 ARM64 debug 寄存器直写方案。

### 8.1 资源查询

`get_hw_breakpoint_info()` 通过 `id_aa64dfr0_el1` 读取：

- BRP 数量：`((dfr0 >> 12) & 0xF) + 1`
- WRP 数量：`((dfr0 >> 20) & 0xF) + 1`

结果写入 `req->bp_info.num_brps` 和 `req->bp_info.num_wrps`。

### 8.2 设置与删除

`set_process_hwbp(...)` 会填充全局 `breakpoint_config`：

- `pid`
- `bt`
- `bl`
- `bs`
- `addr`
- `on_hit`
- `bp_info`

然后调用 `start_task_run_monitor(&bp_config)`：

1. 安装 inline hook，接管 `breakpoint_handler` 与 `watchpoint_handler`。
2. 注册 `sched_switch` trace 回调。
3. 在目标线程组被调度到 CPU 时写入硬件断点寄存器。
4. 在目标线程组被切走时清空断点控制寄存器并关闭当前 CPU 的硬件调试。

`remove_process_hwbp()` 调用 `stop_task_run_monitor()`：

- 注销 `sched_switch` trace 回调
- 清空全局配置
- 移除 inline hook

注意：`bs` 字段当前会写入配置，但 `probe_sched_switch` 现有代码按 `next->tgid == pid` / `prev->tgid == pid` 生效，没有继续区分主线程、子线程或全部线程。

### 8.3 寄存器写入策略

目标线程组切入 CPU 时：

- 调用 `enable_hardware_debug_on_cpu()`：
  - 解锁 `OSLAR_EL1`
  - 设置 `MDSCR_EL1.MDE`
  - 设置 `MDSCR_EL1.KDE`
- 将用户输入转换成 `arch_hw_breakpoint` 风格的 `address` 与 `ctrl`：
  - `X` 转为 `ARM_BREAKPOINT_EXECUTE`
  - `R` 转为 `ARM_BREAKPOINT_LOAD`
  - `W` 转为 `ARM_BREAKPOINT_STORE`
  - `RW` 转为 `LOAD | STORE`
  - AArch64 执行断点长度修正为 4
  - 地址按执行断点 4 字节或观察点 8 字节边界向下对齐，长度按 offset 左移
- 执行断点写入：
  - `DBGBVR5_EL1`
  - `DBGBCR5_EL1`
- 访问观察点写入：
  - `DBGWVR3_EL1`
  - `DBGWCR3_EL1`

目标线程组切走 CPU 时：

- 清零 `DBGBCR5_EL1`
- 清零 `DBGWCR3_EL1`
- 调用 `disable_hardware_debug_on_cpu()` 关闭 MDE/KDE 并重新上 OS Lock

### 8.4 命中处理

inline hook 表：

- `breakpoint_handler` -> `work_trampoline_breakpoint`
- `watchpoint_handler` -> `work_trampoline_watchpoint`

命中后流程：

1. hook 跳板保存通用寄存器和 NZCV。
2. 调用工作函数。
3. 工作函数调用 `bp_config.on_hit(regs, bp_config)`。
4. `sample_hbp_handler()` 按 `regs->pc` 查找或创建 `hwbp_record`。
5. 新 PC 记录默认把所有寄存器 mask 设为读取。
6. 根据每个寄存器的 mask 执行读取或写回。
7. 工作函数关闭当前 BCR/WCR 的 enable bit，让当前命中指令可以步过。
8. 跳板恢复寄存器、执行被覆盖的原指令并返回原异常处理函数后续位置。

命中记录容量：

- 最多保存 `0x100` 个不同 PC 的记录。
- 每条记录保存 `hit_count`、PC、通用寄存器、状态寄存器、FP/SIMD 寄存器。

寄存器 mask 语义：

- `HWBP_OP_NONE`：不处理该寄存器
- `HWBP_OP_READ`：从命中现场读取到 `hwbp_record`
- `HWBP_OP_WRITE`：把 `hwbp_record` 中的值写回命中现场

---

## 9. 模块可见性与生命周期处理

初始化时执行：

1. `bypass_cfi()`：尝试 patch CFI slowpath，使动态函数指针调用更容易通过。
2. `hide_myself()`：
   - 6.12 以下从 `vmap_area_list` 和 `vmap_area_root` 摘除自身 vmalloc 区间。
   - 从 `THIS_MODULE->list` 摘除，使 `/proc/modules` 不显示。
   - 删除 `THIS_MODULE->mkobj.kobj`，使 `/sys/module` 不显示。
   - 清理 holder 依赖链表和 sysfs holder 链接。
3. `allocate_physical_page_info()`：为保留的 PTE 物理映射读写方案准备一页 `vmalloc` 空间。
4. 启动连接线程和调度线程。
5. 注册 `do_exit` kprobe。
6. 从 `task->tasks` 链表摘除两个工作线程。

`do_exit` kprobe 只处理线程组主线程退出。若 `task->comm` 包含 `ls` 或 `LS`：

- 主动调用一次 `read_process_memory(666666, 1, &ProcessExit, 1)`，借路径释放缓存的 `mm_struct`
- 调用 `v_touch_destroy()` 清理虚拟触摸
- 将 `ProcessExit` 置回 `false`

---

## 10. Inline Hook 框架

实现文件：`inline_hook_frame.h`

这个框架是驱动内部的 ARM64 入口 hook 工具。当前主要服务于硬件断点功能：当普通 kprobe 被 `NOKPROBE_SYMBOL` 拒绝、ftrace 又不可用时，驱动通过 inline hook 接管 debug monitor 的异常入口，把命中现场转给自己的处理逻辑。

### 10.1 Hook 描述结构

每个 hook 使用 `struct hook_entry` 描述：

- `target_sym`：目标内核函数符号名
- `target_addr`：运行时解析出的目标地址
- `work_fn`：被 hook 后先执行的工作函数
- `trampoline`：模块 `.text` 中分配到的跳板槽位
- `saved_insn`：目标入口被覆盖前的原始 4 字节指令
- `installed`：安装状态

便捷宏 `HOOK_ENTRY(sym, fn)` 用来声明 hook 表。当前 hook 表在 `arm64_debug_monitor.h` 中定义：

- `breakpoint_handler` -> `work_trampoline_breakpoint`
- `watchpoint_handler` -> `work_trampoline_watchpoint`

### 10.2 跳板槽位

框架在模块代码段中预留：

- `TRAMP_SLOT_COUNT = 10`
- `TRAMP_WORDS = 116`
- `TRAMP_BYTES = 464`

预留区初始填充 NOP，安装 hook 时会把对应槽位 patch 成完整跳板代码。这样跳板本身位于可执行代码段，不需要额外申请可执行内存。

### 10.3 跳板执行流程

`trampoline_build()` 生成的跳板逻辑：

1. 栈上开辟 272 字节临时空间。
2. 保存 X0-X30。
3. 保存 NZCV 条件标志。
4. 按 `struct pt_regs` 前缀填充 `regs[0..30]`、`sp`、`pc`、`pstate`，其中 `pstate` 当前用于保存 NZCV。
5. 设置临时帧指针，并把 `struct pt_regs *` 现场地址放入 X0。
6. 通过数据槽加载 `work_fn` 地址并 `blr x9` 调用工作函数，工作函数签名为 `int work_fn(struct pt_regs *regs)`。
7. `work_fn` 可读写 `regs->regs[0..30]`、`regs->sp`、`regs->pc` 和 `regs->pstate`。
8. 无论 `work_fn` 返回值是什么，都会先按 `regs` 中的值恢复保存现场，因此 `regs->sp` 会写回真实 SP。
9. `regs->pc` 保持为原入口地址时，`work_fn` 返回 0 会恢复现场、执行原始入口指令，再用已 patch 的 `b` 跳回 `target_addr + 4`；返回 1 会恢复现场后 `ret x30`，不再执行原函数入口。
10. `regs->pc` 被改成其他地址时，会恢复现场后用 `ret x16` 跳到新的 `regs->pc`，让 PC 修改真实生效。

动态 `regs->pc` 路径需要一个通用寄存器承载跳转目标，当前使用 X16；因此修改 PC 时，X16 会被用作最终跳转寄存器。普通继续执行路径仍会在执行原始入口指令前恢复 X16/X17。

### 10.4 安装流程

`hook_entry_install()` 的流程：

1. 通过 `generic_kallsyms_lookup_name(target_sym)` 解析目标函数地址。
2. 从预留代码段获取一个 trampoline slot。
3. 读取并保存目标入口原始指令。
4. 构造跳板，写入原指令、返回地址和工作函数地址。
5. 用 `aarch64_insn_patch_text_nosync` 把跳板代码 patch 到预留槽位。
6. 用 `arm64_make_b` 生成从目标入口跳到 trampoline 的 `b` 指令。
7. patch 目标函数入口为 `b trampoline`。
8. 标记 hook 已安装。

### 10.5 卸载流程

`hook_entry_remove()` 会把 `saved_insn` patch 回 `target_addr`，清空 trampoline 指针并标记未安装。

`inline_hook_install()` 负责批量安装 hook 表，任何一条安装失败都会回滚已经安装的项。`inline_hook_remove()` 负责逆序卸载。

### 10.6 在硬件断点中的作用

硬件断点启动时，`start_task_run_monitor()` 会安装 inline hook 并注册 `sched_switch`：

1. 目标线程被调度到 CPU 时写入 BVR/BCR 或 WVR/WCR。
2. 断点命中后进入内核原始 `breakpoint_handler` 或 `watchpoint_handler`。
3. hook 先跳到驱动 trampoline。
4. trampoline 调用 `work_trampoline_breakpoint` 或 `work_trampoline_watchpoint`。
5. 工作函数调用 `sample_hbp_handler()` 记录/回放寄存器。
6. 工作函数关闭当前断点 enable bit，让命中指令可以继续执行。
7. trampoline 执行原始入口指令并返回原异常处理函数。

这套链路让驱动可以直接拿到 `pt_regs` 现场，又不依赖 perf 硬件断点 API。

---

## 11. 用户态封装要点

实现文件：`Android-LS/include/DriverMemory.h`

### 11.1 通信

- 构造 `Driver(bool touch)` 时先 `InitCommunication()`，再按参数决定是否 `InitTouch()`。
- `InitCommunication()` 负责设置进程名、固定地址 `mmap`、清零请求结构、等待驱动握手。
- `IoCommitAndWait()` 将 `req->kernel` 置 `true`，忙等 `req->user`，完成后清除 `req->user`。
- 共享请求结构由 `SpinLock` 保护，避免多线程同时改写同一个 `req_obj`。

### 11.2 内存与模块辅助

- `Read<T>` / `Read(...)` / `Write(...)` 走驱动 `op_r` / `op_w`。
- `GetMemoryInformation()` 走驱动 `op_m`。
- `GetModuleAddress(moduleName, segmentIndex, outAddress, isStart)` 根据模块名后缀和区段 index 查询起止地址。
- `GetScanRegions()` 合并匿名扫描区与所有模块区段，并按地址排序。
- `DumpModule()` 根据模块跨度读取内存，按页抢救失败区域，输出到 `/sdcard/dump/<module>.dump.so`，并尝试修复 ELF Header、Program Header 和部分 Dynamic 指针。

### 11.3 触摸辅助

用户态 `TouchDown` / `TouchMove` / `TouchUp` 会把外部屏幕坐标换算到触摸设备原始坐标。

横屏且设备原始坐标仍是竖屏时，默认按“右侧充电口”方向换算：

- `x = (1 - normY) * POSITION_X`
- `y = normX * POSITION_Y`

### 11.4 硬件断点辅助

- `GetHwbpInfoRef()` 刷新并返回 `bp_info`
- `SetProcessHwbpRef(...)` 设置断点
- `RemoveProcessHwbpRef()` 删除断点
- `RemoveHwbpRecord(index)` 删除指定 PC 命中记录并压缩数组

### 11.5 特征扫描辅助

`SignatureScanner` 基于驱动内存读和 `GetScanRegions()` 实现：

- `ScanAddressSignature(addr, range)`：围绕目标地址生成特征码文件
- `FilterSignature(addr)`：多次运行后把变化字节过滤为通配符
- `ScanSignature(pattern, range)`：按内存区域扫描特征
- `ScanSignatureFromFile()`：从 `Signature.txt` 读取特征并扫描，结果追加写回文件

默认相对路径会回退到 `/data/akernel/`。

---

## 12. 编译说明

### 12.1 直接编译模块

在目标内核源码树执行：

```bash
make -C <KDIR> M=$PWD/lsdriver ARCH=arm64 LLVM=1 modules
```

### 12.2 Makefile 当前参数

`lsdriver/Makefile` 当前默认启用：

- `-O3`
- `-Wno-error`
- `-fno-stack-protector`
- `-fomit-frame-pointer`
- `-funroll-loops`
- `-fstrict-aliasing`
- `-ffunction-sections -fdata-sections`

`DEBUG`、KASAN/UBSAN/KCSAN 关闭、ftrace 插桩移除、强制内联、分支保护关闭等选项保留为注释，需要按目标内核情况自行启用。
