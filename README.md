# 高性能隐匿物理内存读写 ——

内核 API 设计初衷是通用性和安全性，它们包含了大量的权限检查、锁机制和异常处理，这对于辅助或高性能随机内存访问增加负担。
本代码通过直接利用硬件MMU对页表项的访问的机制，去除了所有中间商环节，实现了“指哪打哪”的效果。结合 Context Cache 和 Software TLB 两级软件优化，达到了硬件物理极限的读写

## 1. 概述
代码实现了一个基于 Linux 内核模块的高性能、高隐匿性内存读写驱动。摒弃了标准的虚拟内存管理接口，转而通过手动页表遍历和直接 PTE（页表项）操纵来实现物理内存的读写。
这种方法的核心优势在于：
- **极致性能**：通过多级缓存机制（Context Cache + Software TLB）和TLB 刷新，将读写延迟压缩至微秒级。实际测试采用共享内存通信1000000次读，用户层->内核层->用户层平均单次读取在1us左右
<img width="906" height="300" alt="image" src="https://github.com/user-attachments/assets/8311b35b-5b3b-4470-9275-2a098e97f3b8" />


- **高度隐匿**：绕过内核标准内存管理，自定义页表项标志位。支持任意物理访问。

---

## 2. 核心实现机制

### "动态窗口"重映射技术 (PTE Patching)
这是本驱动核心的读写机制。传统的物理内存读写通常需要频繁建立和销毁映射（ioremap/xlate_dev_mem_ptr），开销巨大。我们采用了一种“预分配窗口 + 动态修改指针”的策略。
- **实现原理**：在驱动初始化时，使用 ：vmalloc 分配一个页面的虚拟内存作为“操作窗口”（info.base_address）。
- 手动遍历 init_mm 页表，找到该虚拟地址对应的 PTE 指针（info.pte_address）。
- 在读写时，不重新分配内存，而是直接修改这个 PTE 的值，将其指向目标物理地址（PFN）。
- 刷新 TLB，使 CPU 使用新映射，然后直接 memcpy。
- 代码对应：allocate_physical_page_info 和 _internal_read/write_from_physical_page_no_restore。

### 自由设置页表项：无缓存访问
在构建 PTE 条目时，我们强制指定了内存属性为 MT_NORMAL_NC (Non-Cached)：
```c
static const u64 FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF | PTE_SHARED | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_NORMAL_NC);
```
读写操作直接绕过 CPU 的 L1/L2/L3 缓存，直接与 DDR 交互。

---

## 3. 性能优化分析

修改页表后调用 flush_tlb_all()。这会向所有 CPU 核心发送 IPI（核间中断），强制全系统刷新 TLB，导致系统级卡顿和性能雪崩。
```c
flush_tlb_kernel_range((unsigned long)info.base_address, (unsigned long)info.base_address + PAGE_SIZE);
```

我们只刷新“操作窗口”这一个页面对应的 TLB 条目。这几乎不消耗时间，且不会影响系统其他部分的运行。

### 进程上下文静态缓存（ Context Cache）
Linux 内核中 find_get_pid 和 get_task_mm 涉及哈希表查找、RCU 锁和原子操作。在高频读写（如100 万次）中，这些开销占比高达 70%。
利用函数内的 static 变量缓存 pid 和 mm_struct。
```c
if (unlikely(pid != s_last_pid || s_last_mm == NULL)) {
    // 只有 PID 变化时才执行查找
    ...
    s_last_mm = get_task_mm(task);
}
```
将 O(N) 的查找开销降低为 O(1)，只有在切换目标进程时才有开销。

### 软件 TLB (Software TLB / Loop Optimization)
内存通常是连续读取的。如果读取 1MB 内存，涉及 256 个 4KB 页面。传统做法会执行 256 次完整的 4 级页表遍历。
在循环内部引入局部变量缓存，目标数据地址所在虚拟页不跨页，直接复用物理地址：
```c
if ((current_vaddr & PAGE_MASK) == loop_last_vpage_base) {
    // 命中！直接复用物理地址，跳过 4 级页表查找
    paddr_of_page = loop_last_ppage_base;
}
```
对于连续内存读写，减少了页表遍历次数，将复杂的内存访问退化为简单的指针加法。

---

## 4. 共享内存通信机制解析
本代码采用了用户-内核共享内存 (User-Kernel Shared Memory) 作为唯一的通信桥梁，彻底摒弃了的 ioctl、netlink 或 procfs 或 hook 机制。
下面详细分析这种设计在代码中的具体实现、巨大优势以及潜在的劣势。

### 机制实现
代码通过以下步骤实现了共享内存通信：
- **硬编码地址协商**：用户层和内核层约定了一个固定的虚拟地址 0x2025827000。
- **反向映射 (Reverse Mapping)**：通常是用户层 mmap 内核设备。
- **无锁自旋同步**：连接后利用类型的 kernel 和 user 标志位，配合 atomic_xchg 和 cpu_relax() 实现极低延迟的握手。
- **启动**：使用计数器+延迟睡眠在无任务时极低的性能消耗，在有任务时做到及时唤醒和极高的全速自旋。

### 巨大优势
- **零拷贝**
- **传统方式**：ioctl 和hook需要将参数结构体从用户栈拷贝到内核栈，处理完再拷贝回去。
- **本代码**：req 指针在内核和用户层指向同一块物理内存。用户层填好 Op、TargetAddress 等参数后，内核直接读取，无需任何内存拷贝。完全消除了每次调用的上下文切换和数据搬运开销。
- **高吞吐量**：依赖内存带宽和cpu速度做到极大的数据传输效率
- **极速响应**：结合 DispatchThreadFunction 中的自适应自旋策略：
    ```c
    if (spin_count < 5000) { cpu_relax(); } // 忙等待模式
    ```
    当用户层提交请求时，内核线程通常正处于 cpu_relax() 状态，能够在一个 CPU 时钟周期内感知到 req->kernel 的变化并立即响应。
- **无系统调用痕迹**：整个过程，用户层只需读写自己的内存地址

### 潜在劣势与风险
- **内核层崩溃**：
    - 1.如果用户层恶意不置位或乱写数据，会导致缓冲区溢出，共享内存标志位数据被破坏，导致触发意外情况导致直接死机，需要规范调用，我想也没有人会一次读取超过1024字节的内存
    - 2.用户进程自己问题意外崩溃，不会有人再来重置标志导致无法重连。
    - 3.用户进程自己问题意外崩溃，由于内核在连接后没有锁住请求进程导致共享内存被释放，代码执行在分支中对共享内存的访问会直接崩溃
    - 4.直接解引用未被映射指针，这是使用数组的原因，应为直接传入指针作为缓冲区会直接非法内存访问
    - 5.极高的速度带来的是极高的性能消耗，大部分时间都是用户层处理数据过慢几乎无法跑满上限，导致一直空转，可增加延迟做到性能与功耗平衡

## 如何编译：
理论来说在4-6系内核改动不是很大的情况都可编译，若不能请自行适配

### 第零步：下载对应通用内核源码和一个linux系统

### 第一步：清理编译环境
```bash
tools/bazel clean --expunge
```

### 第二步：在内核源码树使用Bazel编译内核
```bash
tools/bazel build //common:kernel_aarch64 //common:kernel_aarch64_modules_prepare
```

### 第三步：进入内核输出目录并准备环境
```bash
cd $(readlink -f bazel-bin/common/kernel_aarch64)
tar -xzf ../kernel_aarch64_modules_prepare/modules_prepare_outdir.tar.gz
```

上面3不设置好后就不用每次设置了可以直接编译外部模块（下面路径需替换为自己源码的路径）

### 第四步：设置外部模块编译的环境变量
```bash
export PATH=/root/6.1/prebuilts/clang/host/linux-x86/clang-r487747c/bin:$PATH
export PATH=/root/5.15/prebuilts/clang/host/linux-x86/clang-r450784e/bin:$PATH
```

### 第五步：使用Make编译外部内核模块
```bash
make -C /root/6.1/common \
O=$(readlink -f bazel-bin/common/kernel_aarch64) \
M=/mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver \
ARCH=arm64 \
LLVM=1 \
LLVM_TOOLCHAIN_PATH=/root/6.1/prebuilts/clang/host/linux-x86/clang-r487747c \
CROSS_COMPILE=/root/6.1/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android- \
modules V=1

make -C /root/5.15/common \
O=$(readlink -f bazel-bin/common/kernel_aarch64) \
M=/mnt/e/1.CodeRepository/Android/Kernel/5.15-lsdriver \
ARCH=arm64 \
LLVM=1 \
LLVM_TOOLCHAIN_PATH=/root/5.15/prebuilts/clang/host/linux-x86/clang-r450784e \
CROSS_COMPILE=/root/5.15/prebuilts/ndk-r23/toolchains/llvm/prebuilt/linux-x86_64/bin/aarch64-linux-android- \
modules V=1
```

### 第六步：绝对路径工具剥离符号
```bash
/root/6.1/prebuilts/clang/host/linux-x86/clang-r487747c/bin/llvm-strip --strip-debug  /mnt/e/1.CodeRepository/Android/Kernel/6.1-lsdriver/lsdriver.ko
/root/5.15/prebuilts/clang/host/linux-x86/clang-r450784e/bin/llvm-strip --strip-debug  /mnt/e/1.CodeRepository/Android/Kernel/5.15-lsdriver/lsdriver.ko
```
