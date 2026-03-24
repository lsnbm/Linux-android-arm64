# 高性能隐匿物理内存读写 + 内核触摸

> 仅供技术研究与学习，严禁用于非法用途。作者不承担任何违法责任。

本项目通过 **Context Cache** 与 **Software TLB** 两级软件优化，实现接近硬件物理极限的读写性能。

## 目录

- [第一部分：两种读写方案的实现细节与原理](#第一部分两种读写方案的实现细节与原理)
- [第二部分：代码中的性能优化点](#第二部分代码中的性能优化点)
- [第三部分：共享内存通信机制解析](#第三部分共享内存通信机制解析)
- [第四部分：无干扰虚拟触摸分析](#第四部分无干扰虚拟触摸分析)
- [如何编译](#如何编译)

---

## 第一部分：两种读写方案的实现细节与原理

### 方案 1：PTE 重映射（PTE Remapping / Windowing）

- **对应函数**：`allocate_physical_page_info`（初始化）、`_internal_read_fast`、`_internal_write_fast`

#### 原理

核心思想是“偷梁换柱”：先申请一页合法虚拟内存（作为“窗口”），再手动修改该页对应的 PTE，将其 PFN 指向目标物理地址，实现对任意物理地址的读写。

#### 实现细节

1. **初始化（`allocate_physical_page_info`）**
   - 使用 `vmalloc(PAGE_SIZE)` 分配一页内核虚拟内存。
   - 通过 `memset` 触发缺页，确保建立页表映射。
   - 读取 ARM64 寄存器 `TTBR1_EL1`，手动遍历 `PGD -> P4D -> PUD -> PMD -> PTE`。
   - 定位并保存窗口页对应的 PTE 地址（`info.pte_address`）。
2. **读写过程（`_internal_read_fast`）**
   - 将目标物理地址 `paddr` 转换为 PFN。
   - 使用 `set_pte` 修改保存的 `info.pte_address`，并设置 `MT_NORMAL`、`PTE_VALID` 等属性。
   - 调用 `flush_tlb_kernel_range` 刷新 TLB，避免命中旧映射。
   - 最终通过访问 `info.base_address` 完成对目标物理地址的数据读写。

#### 优缺点

- **优点**：可访问范围广，覆盖普通 RAM、设备寄存器（如配 `MT_DEVICE`）及部分非线性映射区域。
- **缺点**：每次读写都涉及改 PTE + 刷 TLB，开销较高。

#### 实测

- 共享内存通信下，执行 `1,000,000` 次读操作，用户层 -> 内核层 -> 用户层平均约 **1us**。
- 2025-11-27 记录：单线程且不加自旋锁时，可接近 **0.5us**。

<img width="1399" height="452" alt="image" src="https://github.com/user-attachments/assets/9d7a8215-4cd8-4f46-9ea3-5a183630e0bc" />

---

### 方案 2：线性映射直接访问（Linear Mapping / Phys-to-Virt）

- **对应函数**：`_internal_read_fast_linear`、`_internal_write_fast_linear`

#### 原理

利用 ARM64 Linux 的线性映射区（Direct/Linear Mapping）。在该模型中，大部分系统 RAM 会被映射到固定的内核虚拟地址偏移（`PAGE_OFFSET`）上。

#### 实现细节

1. **合法性检查**
   - 使用 `pfn_valid(__phys_to_pfn(paddr))` 判断目标地址是否属于有效系统 RAM。
2. **地址转换**
   - 直接调用 `phys_to_virt(paddr)`。
   - 在 ARM64 上本质是常量偏移加法，不需要页表遍历和 TLB 刷新。
3. **读写**
   - 得到内核虚拟地址后，直接解引用或 `memcpy` 读写。

#### 优缺点

- **优点**：速度极快，无改页表与刷 TLB 开销。
- **缺点**：仅适用于线性映射覆盖的内存（主要是 RAM）；I/O 或保留区地址可能失败。

#### 实测

- 共享内存通信下，执行 `1,000,000` 次读操作，用户层 -> 内核层 -> 用户层平均约 **0.3us**。

<img width="837" height="263" alt="屏幕截图 2025-12-18 175005" src="https://github.com/user-attachments/assets/113f3168-67a1-4572-a549-3559c517b058" />

---

## 第二部分：代码中的性能优化点

涉及函数：`read_process_memory`、`write_process_memory` 及底层读写路径。

### 1. 进程上下文缓存（MM Struct Caching）

- **位置**：`read_process_memory` 开头静态变量。

```c
static pid_t s_last_pid = 0;
static struct mm_struct *s_last_mm = NULL;
```

- **原理**：若连续读取同一进程，则复用上次 `mm_struct`，减少 `get_pid_task` / `get_task_mm` 的查找与引用计数开销。

### 2. 局部页缓存（Software TLB / Loop Optimization）

- **位置**：`read_process_memory` 的 `while` 循环内部。

```c
static uint64_t loop_last_vpage_base = -1;
static phys_addr_t loop_last_ppage_base = 0;

if ((current_vaddr & PAGE_MASK) == loop_last_vpage_base) {
    paddr_of_page = loop_last_ppage_base;
} else {
    // 只有跨页时才走页表遍历
    status = manual_va_to_pa_arm(...);
    ...
}
```

- **原理**：大多数连续读取落在同一页内，命中缓存时直接复用 VA -> PA 结果，避免重复页表遍历。

### 3. 小内存访问特化（Switch-Case Unrolling）

- **位置**：`_internal_read_fast` 与 `_internal_read_fast_linear` 中的 `switch (size)`。

```c
switch (size) {
    case 4: *(uint32_t *)buffer = ...; break;
    case 8: *(uint64_t *)buffer = ...; break;
    ...
    default: memcpy(...); break;
}
```

- **原理**：对 1/2/4/8 字节常见读写优先走直接赋值，减少通用 `memcpy` 的函数与对齐处理开销。

### 4. 使用 `READ_ONCE` / `WRITE_ONCE`

- **位置**：直接内存访问路径。
- **原理**：约束编译器优化，降低撕裂读写风险（在对齐前提下）。

### 5. 分支预测优化（`likely` / `unlikely`）

- **位置**：错误检查等冷路径。
- **原理**：让编译器布局热路径，减少分支预测失败带来的流水线损失。

### 6. 页表遍历中的大页支持

- **位置**：`allocate_physical_page_info` 与 `manual_va_to_pa_arm`。

```c
if (pmd_leaf(*pmd)) { ... } // 检查是否为大页
```

- **原理**：正确处理 Block Mapping（2MB/1GB）避免错误下钻导致的地址错误或崩溃。

### 总结与对比

实际在 `read_process_memory` 中主用 **方案 2（线性映射）**。

- **方案 1 的定位**：更偏向演示或特殊场景（如设备寄存器、非线性映射区）。
- **方案 2 的优势**：读写常规 RAM 时，性能明显更优。

---

## 第三部分：共享内存通信机制解析

本代码使用 **用户-内核共享内存（User-Kernel Shared Memory）** 作为主要通信桥梁，替代 `ioctl`、`netlink`、`procfs` 等方式。

### 机制实现

1. **固定地址协商**：约定固定虚拟地址 `0x2025827000`。
2. **反向映射**：用户态通过 `mmap` 与内核共享同一片内存。
3. **无锁自旋同步**：使用状态位 + `atomic_xchg` + `cpu_relax()` 进行低延迟握手。
4. **动态策略**：空闲时降低消耗，有任务时快速唤醒与高频轮询。

### 优势

1. **零拷贝**：内核和用户态直接读写同一内存，减少上下文切换与拷贝开销。
2. **高吞吐**：主要受限于内存带宽与 CPU 处理速度。
3. **低延迟**：请求到达后可在极短周期内被检测并响应。
4. **痕迹少**：用户态主要做内存读写，系统调用路径更短。

### 风险

1. 用户态异常或恶意数据可能导致共享区状态损坏，触发内核崩溃。
2. 进程异常退出后若缺少恢复流程，可能无法重连。
3. 共享区被释放但内核仍访问，会触发非法访问。
4. 直接解引用未映射指针容易崩溃，参数校验必须严格。
5. 极低延迟策略会提高功耗，需要在性能与能耗之间折中。

---

## 第四部分：无干扰虚拟触摸分析

### 1. 核心策略：制造“盲区”

- 物理触摸驱动通常会循环清理未上报槽位：`for (i = 0; i < num_slots; i++)`。
- 若 `num_slots = 10`，Slot 9 会被物理驱动在下一帧清掉。
- 将 `dev->mt->num_slots` 改为 `9` 后，物理驱动只处理 0-8，Slot 9 变为“盲区”。

### 2. Android 层的“盲区”对齐

- InputReader 常按 10 点触控（0-9）处理。
- 即使驱动侧改成 9 槽，也要用 `input_set_abs_params` 告诉 Android 支持到 Slot 9，否则会被判定为非法数据丢弃。

### 3. 内核层“按键抖动”问题

- 现象：`BTN_TOUCH` / `BTN_TOOL_FINGER` 高频 UP/DOWN 抖动。
- 原因：`input_mt_sync_frame()` 在默认 `INPUT_MT_POINTER` 行为下会自动汇总并发 UP。
- 处理：`new_mt->flags &= ~INPUT_MT_POINTER;`，改为手动控制按键上报。

### 4. 动态开关 Slot（量子态 Slot）

- 问题：固定 `num_slots = 9` 会导致 `input_mt_sync_frame` 不处理 Slot 9。
- 方案：发送期间短暂切换到 10，再立即切回 9。

```c
dev->mt->num_slots = 10; // 1. 瞬时打开
input_mt_slot(..., 9);   // 2. 写入 Slot 9
input_mt_sync_frame(...);// 3. 打包发送
dev->mt->num_slots = 9;  // 4. 立即恢复
```

### 5. 防误触机制（面积与压力）

- 若 `ABS_MT_TOUCH_MAJOR = 0` 或 `ABS_MT_PRESSURE = 0`，可能被系统当作无效触点。
- 通过构造有效值（例如 `MAJOR = 10`, `PRESSURE = 60`）提升触点可信度。

### 6. 总结

核心是在三个层面做一致性控制：

1. 对物理驱动：`num_slots = 9`，保护 Slot 9。
2. 对 Android：声明支持到 Slot 9，避免上层丢弃。
3. 对内核输入框架：关闭自动 POINTER 聚合并在发送时瞬时开放 Slot 9。

相关演示：
https://github.com/user-attachments/assets/53039be7-a21f-43ed-ac17-8bb3a841a93f

---

## 如何编译

理论上，4.x ~ 6.x 内核在改动不大时均可编译；若失败请按目标内核自行适配。

### 第 0 步：准备环境

下载对应通用内核源码，并准备 Linux 构建环境。

> 可选：使用 `build_all.sh`，并配置 `KERNELS_ROOT`、`DRIVER_SRC`；也可按下方手动编译。

### 第 1 步：清理编译环境

```bash
tools/bazel clean --expunge
```

### 第 2 步：使用 Bazel 编译内核

```bash
tools/bazel build //common:kernel_aarch64
```

### 第 3 步：准备模块编译输出目录（一次性）

```bash
cd $(readlink -f bazel-bin/common/kernel_aarch64)
tar -xzf ../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz
```

### 第 4 步：配置外部模块编译环境变量

```bash
export PATH=/root/6.1/prebuilts/clang/host/linux-x86/clang-r487747c/bin:$PATH
export PATH=/root/android13-5.10/prebuilts/clang/host/linux-x86/clang-r450784e/bin:$PATH
export PATH=/root/5.15/prebuilts/clang/host/linux-x86/clang-r450784e/bin:$PATH
export PATH=/root/6.6/prebuilts/clang/host/linux-x86/clang-r510928/bin:$PATH
```

### 第 5 步：编译外部内核模块

#### 版本 1：内核 6.1

```bash
make -C /root/6.1/common \
O=$(readlink -f bazel-bin/common/kernel_aarch64) \
M=/mnt/e/1.CodeRepository/Android/Kernel/lsdriver \
ARCH=arm64 \
LLVM=1 \
LLVM_TOOLCHAIN_PATH=/root/6.1/prebuilts/clang/host/linux-x86/clang-r487747c \
CROSS_COMPILE=/root/6.1/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android- \
modules V=1
```

#### 版本 2：Android 13 / 5.10

```bash
make -C /root/android13-5.10/common \
O=$(readlink -f bazel-bin/common/kernel_aarch64) \
M=/mnt/e/1.CodeRepository/Android/Kernel/lsdriver \
ARCH=arm64 \
LLVM=1 \
LLVM_TOOLCHAIN_PATH=/root/android13-5.10/prebuilts/clang/host/linux-x86/clang-r450784e \
CROSS_COMPILE=/root/android13-5.10/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android- \
KBUILD_MODPOST_WARN=1 \
modules V=1
```

#### 版本 3：内核 5.15

```bash
make -C /root/5.15/common \
O=$(readlink -f bazel-bin/common/kernel_aarch64) \
M=/mnt/e/1.CodeRepository/Android/Kernel/lsdriver \
ARCH=arm64 \
LLVM=1 \
LLVM_TOOLCHAIN_PATH=/root/5.15/prebuilts/clang/host/linux-x86/clang-r450784e \
CROSS_COMPILE=/root/5.15/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android- \
modules V=1
```

#### 版本 4：内核 6.6

```bash
make -C /root/6.6/common \
O=$(readlink -f bazel-bin/common/kernel_aarch64) \
M=/mnt/e/1.CodeRepository/Android/Kernel/lsdriver \
ARCH=arm64 \
LLVM=1 \
LLVM_IAS=1 \
CLANG_TRIPLE=aarch64-linux-gnu- \
CROSS_COMPILE=aarch64-linux-gnu- \
modules V=1
```

### 第 6 步：剥离调试符号并生成版本化模块

```bash
# 内核 6.1
/root/6.1/prebuilts/clang/host/linux-x86/clang-r487747c/bin/llvm-strip --strip-debug -o /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/6.1lsdriver.ko /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.ko

# Android 13 5.10
/root/android13-5.10/prebuilts/clang/host/linux-x86/clang-r450784e/bin/llvm-strip --strip-debug -o /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/13-5.10lsdriver.ko /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.ko

# 内核 5.15
/root/5.15/prebuilts/clang/host/linux-x86/clang-r450784e/bin/llvm-strip --strip-debug -o /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/5.15lsdriver.ko /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.ko

# 内核 6.6
/root/6.6/prebuilts/clang/host/linux-x86/clang-r510928/bin/llvm-strip --strip-debug -o /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/6.6lsdriver.ko /mnt/e/1.CodeRepository/Android/Kernel/lsdriver/lsdriver.ko
```

---

t.me/liaoshuangls
