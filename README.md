
***

# 高性能隐匿物理内存读写+内核触摸

**Context Cache** 和 **Software TLB** 两级软件优化，达到了硬件物理极限的读写。
“仅供技术研究与学习，严禁用于非法用途，作者不承担任何违法责任”
---

## 第一部分：两种读写方案的实现细节与原理

### 方案 1：PTE 重映射 (PTE Remapping / Windowing)

*   **对应函数**：`allocate_physical_page_info` (初始化), `_internal_read_fast`, `_internal_write_fast`

#### 原理
这种方案的核心思想是**“偷梁换柱”**。内核先申请一块合法的虚拟内存（作为一个“窗口”），然后手动修改这块虚拟内存对应的页表项（PTE），将其物理页帧号（PFN）指向我们想要读写的任意物理地址。

#### 实现细节

1.  **初始化 (`allocate_physical_page_info`)**：
    *   **申请窗口**：使用 `vmalloc(PAGE_SIZE)` 分配一页内核虚拟内存。
    *   **填充页表**：对该内存 `memset`，触发缺页异常，确保内核为其分配了物理页并在页表中建立了映射。
    *   **定位 PTE**：这是最关键的一步。代码通过读取 ARM64 寄存器 `TTBR1_EL1`（内核页表基址寄存器），手动遍历页表层级 (`PGD -> P4D -> PUD -> PMD -> PTE`)。
    *   **保存指针**：最终找到控制该 vmalloc 内存的 PTE 的物理位置对应的虚拟地址 (`info.pte_address`)。保存它，以后就可以直接修改这个地址的内容来改变映射。
2.  **读写过程 (`_internal_read_fast`)**：
    *   **计算 PFN**：将目标物理地址 (`paddr`) 转换为页帧号 (`pfn`)。
    *   **修改 PTE**：使用 `set_pte` 直接修改之前保存的 `info.pte_address`，将其指向目标的 `pfn`。同时设置了页属性（FLAGS），如 `MT_NORMAL`（启用缓存或者不缓存）、`PTE_VALID` 等。
    *   **设置任意页表项**：设置了页属性（FLAGS），如 `MT_NORMAL`（启用缓存或者不缓存）不缓存读写操作直接绕过 CPU 的 L1/L2/L3 缓存，直接与 DDR 交互、`PTE_VALID` 等。
    *   **刷新 TLB**：修改页表后，CPU 的 TLB（转换后备缓冲区）中可能还缓存着旧的映射关系。必须调用 `flush_tlb_kernel_range` 强制刷新，否则 CPU 仍会访问旧地址。
    *   **数据拷贝**：此时，访问 `info.base_address`（vmalloc 的地址）实际上就是访问目标的物理地址。

#### 优缺点
*   **优点**：可以访问所有物理内存，包括显存、设备寄存器（如果配置为 `MT_DEVICE`）以及不在线性映射区的内存（HighMem，虽然 ARM64 通常没有 HighMem 问题）。
*   **缺点**：性能较差。每次读写都需要修改 PTE 并刷新 TLB。TLB 刷新是极其昂贵的操作，会打断流水线并强制同步。

#### 实际测试
采用共享内存通信 1000000 次读，用户层->内核层->用户层平均单次读取在 **1us** 左右。
*2025.11.27 (不加自旋锁可以提升到 0.5us 左右，前提是单线程调用)*

<img width="1399" height="452" alt="image" src="https://github.com/user-attachments/assets/9d7a8215-4cd8-4f46-9ea3-5a183630e0bc" />

---

### 方案 2：线性映射直接访问 (Linear Mapping / Phys-to-Virt)

*   **对应函数**：`_internal_read_fast_linear`, `_internal_write_fast_linear`

#### 原理
利用 Linux 内核的线性映射区（Direct Mapping / Linear Mapping）。在 ARM64 Linux 内核中，所有的物理 RAM（低端内存）通常都会被内核映射到一个固定的内核虚拟地址偏移处（`PAGE_OFFSET`）。

#### 实现细节

1.  **合法性检查**：使用 `pfn_valid(__phys_to_pfn(paddr))` 检查该物理地址是否对应有效的系统 RAM（由内核管理的内存页）。
2.  **地址转换**：直接调用 `phys_to_virt(paddr)`。
    *   在 ARM64 上，这是一个极其简单的数学运算，类似于：`virtual_address = physical_address + const_offset`。
    *   不需要查页表，不需要刷新 TLB。
3.  **读写**：得到转换后的内核虚拟地址（`kernel_vaddr`）后，直接使用指针解引用或进行读写。

#### 优缺点
*   **优点**：速度极快。没有页表操作，没有 TLB 刷新开销，仅仅是指针运算和内存拷贝。
*   **缺点**：只能访问被内核线性映射管理的内存（主要是 RAM）。如果物理地址属于 I/O 设备（如显存）或者是未被内核映射的保留内存，`pfn_valid` 会失败或 `phys_to_virt` 返回无效指针。

#### 实际测试
采用共享内存通信 1000000 次读，用户层->内核层->用户层平均单次读取在 **0.3us** 左右。

<img width="837" height="263" alt="屏幕截图 2025-12-18 175005" src="https://github.com/user-attachments/assets/113f3168-67a1-4572-a549-3559c517b058" />

---

## 第二部分：代码中的性能优化点

代码在 `read_process_memory` 和 `write_process_memory` 以及底层读写函数中做了大量的性能优化：

### 1. 进程上下文缓存 (MM Struct Caching)
*   **位置**：`read_process_memory` 函数开头的 static 变量。
    ```c
    static pid_t s_last_pid = 0;
    static struct mm_struct *s_last_mm = NULL;
    ```
*   **优化原理**：
    每次调用 `get_pid_task` 和 `get_task_mm` 都涉及锁操作和引用计数更新，开销较大。
    *   代码判断如果 `pid` 和上次调用一致，则直接复用上次获取的 `s_last_mm`。
    *   这极大地提高了连续读取同一个进程内存时的性能（减少了查找进程的时间）。

### 2. 局部页缓存 (Software TLB / Loop Optimization)
*   **位置**：`read_process_memory` 的 while 循环内部。
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
*   **优化原理**：
    内存读取通常是连续的。如果用户读取 100 字节，这些字节极大概率在同一个 4KB 页内。
    *   代码记录了上一次转换的虚拟页基址。
    *   如果在同一个页内（`current_vaddr & PAGE_MASK` 没变），则跳过昂贵的页表遍历（Page Table Walk），直接使用上次计算出的物理页基址。
    *   只有当读取跨越页边界时，才重新进行 VA -> PA 转换。

### 3. 小内存访问特化 (Switch-Case Unrolling)
*   **位置**：`_internal_read_fast` 和 `_internal_read_fast_linear` 中的 `switch (size)`。
    ```c
    switch (size) {
        case 4: *(uint32_t *)buffer = ...; break;
        case 8: *(uint64_t *)buffer = ...; break;
        ...
        default: memcpy(...); break;
    }
    ```
*   **优化原理**：
    对于 1, 2, 4, 8 字节这种常见的基本数据类型大小，直接使用赋值指令（如 ARM64 的 `ldr`/`str` 指令）比调用通用的 `memcpy` 函数要快得多。`memcpy` 有函数调用开销以及内部的对齐检查、循环处理开销。

### 4. 使用 READ_ONCE / WRITE_ONCE
*   **位置**：所有直接内存访问处。
*   **优化原理**：
    *   **防止编译器优化**：告诉编译器该内存地址是易变的（volatile），强制生成访存指令，防止编译器将多次读取合并，或将写入优化掉。
    *   **原子性保证（弱）**：在对齐的情况下，现代 CPU 对 1/2/4/8 字节的访问通常是原子的，防止读到一半新数据一半旧数据的“撕裂”现象。

### 5. 分支预测优化 (likely / unlikely)
*   **位置**：多处错误检查，如 `if (unlikely(!buffer || size == 0))`。
*   **优化原理**：
    使用 GCC 内置宏提示编译器，告知某个分支（如出错处理）发生概率极低。编译器会将“热代码”（正常逻辑）放在一起，减少 CPU 指令流水线的跳转预测失败，提高执行效率。

### 6. 页表遍历中的大页支持
*   **位置**：`allocate_physical_page_info` 和 `manual_va_to_pa_arm` 中。
    ```c
    if (pmd_leaf(*pmd)) { ... } // 检查是否为大页
    ```
*   **优化原理**：
    虽然这不是直接的速度优化，但正确处理 Block Mapping（大页）防止了在遇到 2MB 或 1GB 大页时错误的继续索引下一级页表，避免了段错误或读取错误地址，保证了遍历逻辑的鲁棒性。

#### 总结与对比
代码实际在 `read_process_memory` 中使用的是 **方案 2（线性映射）**。
*   **为什么不用方案 1？**
    方案 1 主要是为了演示或应对特殊情况（如想要修改映射属性为 `MT_DEVICE` 去读设备寄存器，或者目标物理地址不在内核线性映射区内）。
    但在读写常规进程内存（RAM）时，方案 2 配合手动页表转换是性能最高的组合，因为它完全避免了 TLB 刷新和修改页表的昂贵开销。

---

## 4. 共享内存通信机制解析

本代码采用了 **用户-内核共享内存 (User-Kernel Shared Memory)** 作为唯一的通信桥梁，彻底摒弃了传统的 `ioctl`、`netlink` 或 `procfs` 或 `hook` 机制。

### 机制实现
代码通过以下步骤实现了共享内存通信：
*   **硬编码地址协商**：用户层和内核层约定了一个固定的虚拟地址 `0x2025827000`。
*   **反向映射 (Reverse Mapping)**：通常是用户层 mmap 内核设备。
*   **无锁自旋同步**：连接后利用类型的 kernel 和 user 标志位，配合 `atomic_xchg` 和 `cpu_relax()` 实现极低延迟的握手。
*   **启动**：使用计数器+延迟睡眠在无任务时极低的性能消耗，在有任务时做到及时唤醒和极高的全速自旋。

### 巨大优势
1.  **零拷贝**
    *   **传统方式**：ioctl 和 hook 需要将参数结构体从用户栈拷贝到内核栈，处理完再拷贝回去。
    *   **本代码**：`req` 指针在内核和用户层指向同一块物理内存。用户层填好 Op、TargetAddress 等参数后，内核直接读取，无需任何内存拷贝。完全消除了每次调用的上下文切换和数据搬运开销。
2.  **高吞吐量**：依赖内存带宽和 CPU 速度做到极大的数据传输效率。
3.  **极速响应**：结合 `DispatchThreadFunction` 中的自适应自旋策略：
    `if (spin_count < 5000) { cpu_relax(); } // 忙等待模式`
    当用户层提交请求时，内核线程通常正处于 `cpu_relax()` 状态，能够在一个 CPU 时钟周期内感知到 `req->kernel` 的变化并立即响应。
4.  **无系统调用痕迹**：整个过程，用户层只需读写自己的内存地址。

### 潜在劣势与风险
1.  **内核层崩溃**：如果用户层恶意不置位或乱写数据，会导致缓冲区溢出，共享内存标志位数据被破坏，导致触发意外情况导致直接死机，需要规范调用，我想也没有人会一次读取超过 1024 字节的内存。
2.  用户进程自己问题意外崩溃，不会有人再来重置标志导致无法重连。
3.  用户进程自己问题意外崩溃，由于内核在连接后没有锁住请求进程导致共享内存被释放，代码执行在分支中对共享内存的访问会直接崩溃。
4.  直接解引用未被映射指针，这是使用数组的原因，应为直接传入指针作为缓冲区会直接非法内存访问。
5.  极高的速度带来的是极高的性能消耗，大部分时间都是用户层处理数据过慢几乎无法跑满上限，导致一直空转，可增加延迟做到性能与功耗平衡。

---

## 5. 无干扰虚拟触摸分析

### 1. 核心战略：制造“盲区”
*   **问题**：物理触摸屏驱动程序通常比较“死板”。每次中断来临，它会写死一个循环 `for (i = 0; i < num_slots; i++)`。如果硬件没报告 Slot i，驱动就会强制调用 `input_mt_report_slot_state(dev, tool, false)`（即抬起）。
*   **冲突**：原本 `num_slots` 是 10。驱动循环 0-9。你一旦用了 Slot 9，下一毫秒物理驱动就会把它“杀掉”（置为 UP）。
*   **对策**：我们把 `dev->mt->num_slots` 改成了 9。
*   **结果**：物理驱动现在的循环变成 0-8。Slot 9 成为了它的**“视力盲区”**，它永远不会去碰 Slot 9 的内存数据。这是我们能共存的基石。

### 2. Android 系统的“盲区”
*   **原因**：Android 上层的 InputReader（负责读取 `/dev/input/eventX` 的服务）通常硬编码了最大支持 10 个触控点（Slot 0-9）。
*   **关键操作**：我们虽然欺骗物理驱动说只有 9 个 Slot（0-8）。但我们必须通过 `input_set_abs_params` 告诉 Android 系统：“嘿，其实我支持到 Slot 9 哦”。
    如果你没做这一步，Android 以为只有 0-8，看到 Slot 9 的数据会直接当做非法数据丢掉。

### 3. 内核的 “按键抖动”
*   **现象**：`BTN_TOUCH` 和 `BTN_TOOL_FINGER` 疯狂 UP/DOWN，导致系统认为全是噪点。
*   **原因**：Linux 内核有一个 helper 函数 `input_mt_sync_frame()`。默认情况下（带有 `INPUT_MT_POINTER` 标志），它会遍历所有 Slot。
    物理驱动刚刚把 0-8 都置为 UP，内核会说：“哦，现在屏幕上没有手指了”，于是自动发出 `BTN_TOUCH UP`。
    紧接着代码发出了 Slot 9 的数据。
    结果就是：物理驱动说没手 -> 内核发 UP -> 你发 Slot9 -> 你发 DOWN。每一帧都在打架。
*   **关键操作**：`new_mt->flags &= ~INPUT_MT_POINTER;` (关掉自动计算)。
    告诉内核：“别帮我算按键了，我自己控制”。
    然后我们在代码里手动发送 `input_report_key(dev, BTN_TOUCH, 1)`。因为物理驱动变成了“哑巴”（它依赖自动计算，现在关了它就发不出 UP 了）。

### 4. 量子态的 Slot (动态开关)
*   **问题**：既然我们把 `num_slots` 改成了 9，那当我们自己调用 `input_mt_sync_frame` 时，内核也只会扫描 0-8，根本不会把我们的 Slot 9 发出去！
*   **对策**：动态开关（闪电战）：
    ```c
    dev->mt->num_slots = 10; // 1. 瞬间把门打开
    input_mt_slot(..., 9);   // 2. 塞进去数据
    input_mt_sync_frame(...);// 3. 让内核打包发送（内核此时看到是10，所以带上了Slot 9）
    dev->mt->num_slots = 9;  // 4. 瞬间把门关上
    ```
*   **精髓**：
    *   对**物理驱动**（在中断里运行）：它绝大多数时候看到的都是 9，所以它不碰 Slot 9。
    *   对**我们**（线程运行）：我们在那一微秒内把它改成 10，发完立刻改回去。
    *   利用了代码执行的时间差，实现了“同一个变量，对两个人显示不同数值”的效果。

### 5. 防误触机制 (面积与压力)
*   **现象**：最早的时候有日志但没反应。
*   **原因**：现代手机（特别是全面屏）有非常激进的防误触算法（Palm Rejection）。
    如果一个触控点 `ABS_MT_TOUCH_MAJOR`（接触面积）是 0，或者 `ABS_MT_PRESSURE`（压力）是 0，系统会认为这是“悬空的手指”或者“静电干扰”，直接丢弃。
*   **关键操作**：在代码里伪造了 `MAJOR = 10` 和 `PRESSURE = 60`。这让 Android 确信：这是一根真实的手指，结结实实地按在了屏幕上。

### 6. 总结
为了做成这件事，实际上是在三个层面上撒谎：
1.  对 **物理驱动** 撒谎：告诉它 `num_slots = 9`（保护 Slot 9 不被清洗）。
2.  对 **Android 系统** 撒谎：告诉它 `ABS_MAX_SLOT = 9`（让它接受第 10 个手指的数据）。
3.  对 **Linux 内核** 撒谎：去掉 `POINTER` 标志（剥夺内核自动发 UP 的权力），发送时临时改 `num_slots = 10`（强迫内核处理 Slot 9）。


https://github.com/user-attachments/assets/53039be7-a21f-43ed-ac17-8bb3a841a93f


---

## 如何编译

理论来说在 4-6 系内核改动不是很大的情况都可编译，若不能请自行适配。

### 第零步：下载对应通用内核源码和一个 linux 系统





# 使用build_all.sh并配置KERNELS_ROOT和DRIVER_SRC和修改执行流程build_kernel后面名称为你的内核源码目录名称或下面手动编译

## 第一步：清理编译环境
清除 Bazel 构建缓存，确保环境干净：
```bash
tools/bazel clean --expunge
```

## 第二步：使用 Bazel 编译内核
在内核源码树的根目录执行，编译 aarch64 架构内核本体：
```bash
tools/bazel build //common:kernel_aarch64
```

## 第三步：进入内核输出目录并准备模块编译环境
进入 Bazel 生成的内核输出目录，解压模块编译所需的预处理文件，**此步骤完成后无需重复执行**：
```bash
cd $(readlink -f bazel-bin/common/kernel_aarch64)
tar -xzf ../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz
```

## 第四步：设置外部模块编译环境变量
添加编译器工具链到系统 PATH，确保编译时能找到对应版本的 clang 和交叉编译工具：
```bash
export PATH=/root/6.1/prebuilts/clang/host/linux-x86/clang-r487747c/bin:$PATH
export PATH=/root/android13-5.10/prebuilts/clang/host/linux-x86/clang-r450784e/bin:$PATH
export PATH=/root/5.15/prebuilts/clang/host/linux-x86/clang-r450784e/bin:$PATH
export PATH=/root/6.6/prebuilts/clang/host/linux-x86/clang-r510928/bin:$PATH
```

## 第五步：使用 Make 编译外部内核模块
针对不同内核版本执行对应的编译命令，生成 `lsdriver.ko` 模块文件：

### 版本 1：内核 6.1
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

### 版本 2：Android 13 5.10
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

### 版本 3：内核 5.15
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

### 版本 4：内核 6.6
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

## 第六步：剥离调试符号并生成版本化模块文件
使用对应版本的 `llvm-strip` 工具移除调试信息，生成不同内核版本的模块文件，避免覆盖：
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




t.me/liaoshuangls
