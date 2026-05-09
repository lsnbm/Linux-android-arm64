#pragma once
#include <algorithm>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <elf.h>
#include <fcntl.h>
#include <format>
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
#include <print>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <string>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <thread>
#include <unistd.h>
#include <variant>
#include <vector>

#define PAGE_SIZE 4096
// 12月2日21:36开始记录修复问题:
/* 变量统一使用下划线命名贴近内核，只有函数命名时驼峰命名
1.修复多线程竞争驱动资源，无锁导致的多线程修改共享内存数据状态错误导致死机
解决方案：加锁
2.用户调用read读取字节大于1024导致溢出，内存越界，导致后面变量状态错误导致的死机
解决方案：循环分片读写
3.游戏退出不能再次开启
解决方案: 析构函数主动通知驱动切换目标
4.req 是一个共享资源不能在IoCommitAndWait函数加锁
解决方案: 在任何对MIoPacket有修改的地方都需要提前加锁，而不是在通知的时候才加锁
5.读取大块内存的时候失败一次就导致整个返回失败
解决方案：内核层修复为只要不是0字节就成功，大内存读取跳过失败区域继续往后读取
6.Requests结构体不能过大，会导致mmap分配失败，后续所有使用Requests指针地方会直接段错误
解决方案: 优化布局
7.检查真实触摸进行虚拟触摸时非常频繁的真实点击抬起手指会应为掉帧、或者因为连击太快而漏发了 TouchUp 时会触发空心圆圈(触摸小白点为空心圆圈代表发生了:悬浮事件，或者触摸状态没有被完全清理干净)
解决方案:最重要的是代码流程逻辑异常错误导致TouchUp()没有被调用，让内核自己去检测物理屏幕上没有真实手指了，强行杀死虚拟手指是解决办法，但是想保留独立触摸能力

2006/3/2 17:15
8.反作弊 VMA 碎裂与诱饵对抗
解决方案:已经在内核层修复，下面有GetModuleAddress函数有注释解释

2026/4/18 12:19
9.今天一直有人反馈IoCommitAndWait导致进程卡住，或者有人说Driver驱动调用会导致卡住线程
  经过2小时的排查问题，发现是他自己写的用户层代码有问题，
  他有个线程:一边读取,一边绘制,一边画菜单ui,然后他判断里面读取失败写的一直重试，重读，导致一直读取失败一直重试跑满了占用
  还有某些人安全检查一点都不做，远程内存读取出数量，
  遍历的时候都不检查一下这个数量是不是合理值，这个是极大值(999999999999999)或负数溢出，你还真就读取这么多次，不卡死阻塞才怪
  都是这些无语小问题

*/

class Driver
{
public:                // 外部初始化
    Driver(bool touch) // 为真开启触摸
    {
        InitCommunication();
        if (touch)
        {
            InitTouch();
        }
    }

    ~Driver()
    {
        // ExitKernel();
    }

public: // 共有结构体和锁
        // 轻量高性能自旋锁
    class SpinLock
    {
        std::atomic_flag locked = ATOMIC_FLAG_INIT;

        inline void pause() const noexcept
        {
            __builtin_arm_yield();
        }

    public:
        void lock() noexcept
        {
            for (int i = 0; i < 64; ++i)
            {

                if (!locked.test_and_set(std::memory_order_acquire))
                {
                    return;
                }

                while (locked.test(std::memory_order_relaxed))
                {
                    pause();
                }
            }

            while (locked.test_and_set(std::memory_order_acquire))
            {
                locked.wait(true, std::memory_order_relaxed);
            }
        }

        void unlock() noexcept
        {
            locked.clear(std::memory_order_release);
            locked.notify_one();
        }
    };
    SpinLock m_mutex;

    // 断点类型(类型和长度完全与内核一致会冲突，所以这里HW加上BP后缀,原型没有BP)
    enum hwbp_type
    {
        HWBP_BREAKPOINT_EMPTY = 0,
        HWBP_BREAKPOINT_R = 1,
        HWBP_BREAKPOINT_W = 2,
        HWBP_BREAKPOINT_RW = HWBP_BREAKPOINT_R | HWBP_BREAKPOINT_W,
        HWBP_BREAKPOINT_X = 4,
        HWBP_BREAKPOINT_INVALID = HWBP_BREAKPOINT_RW | HWBP_BREAKPOINT_X,
    };
    // 断点长度
    enum hwbp_len
    {
        HWBP_BREAKPOINT_LEN_1 = 1,
        HWBP_BREAKPOINT_LEN_2 = 2,
        HWBP_BREAKPOINT_LEN_3 = 3,
        HWBP_BREAKPOINT_LEN_4 = 4,
        HWBP_BREAKPOINT_LEN_5 = 5,
        HWBP_BREAKPOINT_LEN_6 = 6,
        HWBP_BREAKPOINT_LEN_7 = 7,
        HWBP_BREAKPOINT_LEN_8 = 8,

    };
    // 断点作用线程范围
    enum hwbp_scope
    {
        SCOPE_MAIN_THREAD,   // 仅主线程
        SCOPE_OTHER_THREADS, // 仅其他子线程
        SCOPE_ALL_THREADS    // 全部线程
    };

    // 寄存器索引枚举 (每个索引占用 2 bits)
    enum hwbp_reg_idx
    {
        IDX_PC = 0,
        IDX_HIT_COUNT,
        IDX_LR,
        IDX_SP,
        IDX_ORIG_X0,
        IDX_SYSCALLNO,
        IDX_PSTATE,
        IDX_X0,
        IDX_X1,
        IDX_X2,
        IDX_X3,
        IDX_X4,
        IDX_X5,
        IDX_X6,
        IDX_X7,
        IDX_X8,
        IDX_X9,
        IDX_X10,
        IDX_X11,
        IDX_X12,
        IDX_X13,
        IDX_X14,
        IDX_X15,
        IDX_X16,
        IDX_X17,
        IDX_X18,
        IDX_X19,
        IDX_X20,
        IDX_X21,
        IDX_X22,
        IDX_X23,
        IDX_X24,
        IDX_X25,
        IDX_X26,
        IDX_X27,
        IDX_X28,
        IDX_X29,
        IDX_FPSR,
        IDX_FPCR,
        IDX_Q0,
        IDX_Q1,
        IDX_Q2,
        IDX_Q3,
        IDX_Q4,
        IDX_Q5,
        IDX_Q6,
        IDX_Q7,
        IDX_Q8,
        IDX_Q9,
        IDX_Q10,
        IDX_Q11,
        IDX_Q12,
        IDX_Q13,
        IDX_Q14,
        IDX_Q15,
        IDX_Q16,
        IDX_Q17,
        IDX_Q18,
        IDX_Q19,
        IDX_Q20,
        IDX_Q21,
        IDX_Q22,
        IDX_Q23,
        IDX_Q24,
        IDX_Q25,
        IDX_Q26,
        IDX_Q27,
        IDX_Q28,
        IDX_Q29,
        IDX_Q30,
        IDX_Q31,
        MAX_REG_COUNT
    };

// 寄存器操作类型定义
#define HWBP_OP_NONE 0x0  // 00: 不操作
#define HWBP_OP_READ 0x1  // 01: 读
#define HWBP_OP_WRITE 0x2 // 10: 写

// 设置掩码位的宏，参数1:结构体指针，参数2:寄存器索引，参数3:操作类型
#define HWBP_SET_MASK(record, reg, op)                          \
    do                                                          \
    {                                                           \
        int byte_idx = (reg) >> 2;                              \
        int bit_offset = ((reg) & 0x3) << 1;                    \
        (record)->mask[byte_idx] &= ~(0x3 << bit_offset);       \
        (record)->mask[byte_idx] |= ((op) & 0x3) << bit_offset; \
    } while (0)

// 获取掩码位的宏，参数1:结构体指针，参数2:寄存器索引
#define HWBP_GET_MASK(record, reg) \
    (((record)->mask[(reg) >> 2] >> (((reg) & 0x3) << 1)) & 0x3)

    // 记录单个 PC（触发指令地址）的命中状态
    struct hwbp_record
    {
        /*
        一个掩码位，控制所有寄存器的读写行为
        为了方便掩码位控制对应寄存器，不使用数组存储寄存器了， 方便了：阅读，理解，写代码时不再做 regs[i] / vregs[i] 的索引换算
        */
        uint8_t mask[18];

        // 通用寄存器
        uint64_t hit_count; // 该 PC 命中的次数
        uint64_t pc;        // 触发断点的汇编指令地址
        uint64_t lr;        // X30
        uint64_t sp;        // Stack Pointer
        uint64_t orig_x0;   // 原始 X0
        uint64_t syscallno; // 系统调用号
        uint64_t pstate;    // 处理器状态
        uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9;
        uint64_t x10, x11, x12, x13, x14, x15, x16, x17, x18, x19;
        uint64_t x20, x21, x22, x23, x24, x25, x26, x27, x28, x29;

        // 浮点/SIMD 寄存器
        uint32_t fpsr; // 浮点状态寄存器
        uint32_t fpcr; // 浮点控制寄存器
        __uint128_t q0, q1, q2, q3, q4, q5, q6, q7, q8, q9;
        __uint128_t q10, q11, q12, q13, q14, q15, q16, q17, q18, q19;
        __uint128_t q20, q21, q22, q23, q24, q25, q26, q27, q28, q29;
        __uint128_t q30, q31;
    };

    // 存储整体命中信息
    struct hwbp_info
    {
        uint64_t num_brps;                 // 执行断点的数量
        uint64_t num_wrps;                 // 访问断点的数量
        uint64_t hit_addr;                 // 监控的地址
        int record_count;                  // 当前已记录的不同 PC 数量
        struct hwbp_record records[0x100]; // 记录不同 PC 触发状态的数组
    };

#define MAX_MODULES 512
#define MAX_SCAN_REGIONS 4096

#define MOD_NAME_LEN 256
#define MAX_SEGS_PER_MODULE 256

    struct segment_info
    {
        short index;  // >=0: 普通段(RX→RO→RW连续编号), -1: BSS段
        uint8_t prot; // 区段权限: 1(R), 2(W), 4(X)。例如 RX 就是 5 (1+4)
        uint64_t start;
        uint64_t end;
    };

    struct module_info
    {
        char name[MOD_NAME_LEN];
        int seg_count;
        struct segment_info segs[MAX_SEGS_PER_MODULE];
    };

    struct region_info
    {
        uint64_t start;
        uint64_t end;
    };

    struct memory_info
    {
        int module_count;                        // 总模块数量
        struct module_info modules[MAX_MODULES]; // 模块信息

        int region_count;                             // 总可扫描内存数量
        struct region_info regions[MAX_SCAN_REGIONS]; // 可扫描内存区域 (rw-p, 排除特殊区域)
    };

    enum sm_req_op
    {
        op_o, // 空调用
        op_r,
        op_w,
        op_m, // 获取进程内存信息

        op_down,
        op_move,
        op_up,
        op_init_touch, // 初始化触摸

        op_brps_weps_info,      // 获取执行断点数量和访问断点数量
        op_set_process_hwbp,    // 设置硬件断点
        op_remove_process_hwbp, // 删除硬件断点

        op_kexit // 内核线程退出
    };

    // 将在队列中使用的请求实例结构体
    struct req_obj
    {
        bool kernel; // 由用户模式设置 true = 内核有待处理的请求, false = 请求已完成
        bool user;   // 由内核模式设置 true = 用户模式有待处理的请求, false = 请求已完成

        enum sm_req_op op; // shared memory请求操作类型
        int status;        // 操作状态

        // 内存读取
        int pid;
        uint64_t target_addr;
        int size;
        uint8_t user_buffer[0x1000]; // 物理标准页大小

        // 进程内存信息
        struct memory_info mem_info;

        enum hwbp_type bt;        // 断点类型
        enum hwbp_len bl;         // 断点长度
        enum hwbp_scope bs;       // 断点作用线程范围
        struct hwbp_info bp_info; // 断点信息

        // 初始化触摸时返回的屏幕维度
        int POSITION_X, POSITION_Y;
        // 触摸槽位
        int slot;
        // 触摸坐标
        int x, y;
    };

public:
    void NullIo()
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = op_o;
        IoCommitAndWait();
    }

    void ExitKernel()
    {
        // 内核停止运行
        req->op = op_kexit;
        req->kernel = true;
    }
    int GetPid(std::string_view packageName)
    {
        DIR *dir = opendir("/proc");
        if (!dir)
            return -1;
        struct dirent *entry;

        char pathBuffer[64];
        char cmdlineBuffer[256];

        while ((entry = readdir(dir)) != nullptr)
        {
            if (entry->d_type == DT_DIR && entry->d_name[0] >= '1' && entry->d_name[0] <= '9')
            {
                snprintf(pathBuffer, sizeof(pathBuffer), "/proc/%s/cmdline", entry->d_name);
                int fd = open(pathBuffer, O_RDONLY);
                if (fd >= 0)
                {

                    ssize_t bytesRead = read(fd, cmdlineBuffer, sizeof(cmdlineBuffer) - 1);
                    close(fd);

                    if (bytesRead > 0)
                    {

                        cmdlineBuffer[bytesRead] = '\0';

                        if (packageName == std::string_view(cmdlineBuffer))
                        {
                            closedir(dir);
                            return atoi(entry->d_name);
                        }
                    }
                }
            }
        }
        closedir(dir);
        return -1;
    }
    int GetGlobalPid()
    {
        return global_pid;
    }
    void SetGlobalPid(int pid)
    {
        global_pid = pid;
    }

public: // 外部读写接口
    template <typename T>
    T Read(uint64_t address)
    {
        T value = {};
        KReadProcessMemory(address, &value, sizeof(T));
        return value;
    }

    int Read(uint64_t address, void *buffer, size_t size)
    {
        return KReadProcessMemory(address, buffer, size);
    }

    std::string ReadString(uint64_t address, size_t max_length = 128)
    {
        if (!address)
            return "";
        std::vector<char> buffer(max_length + 1, 0);
        if (Read(address, buffer.data(), max_length) > 0)
        {
            buffer[max_length] = '\0';
            return std::string(buffer.data());
        }
        return "";
    }

    std::string ReadWString(uintptr_t address, size_t length)
    {
        if (length <= 0 || length > 1024)
            return "";
        std::vector<char16_t> buffer(length);
        if (Read(address, buffer.data(), length * sizeof(char16_t)) > 0)
        {
            std::string result;
            for (size_t i = 0; i < length; ++i)
            {
                if (buffer[i] == 0)
                    break;
                result.push_back(buffer[i] < 128 ? static_cast<char>(buffer[i]) : '?');
            }
            return result;
        }
        return "";
    }

    template <typename T>
    int Write(uint64_t address, const T &value)
    {
        return KWriteProcessMemory(address, const_cast<T *>(&value), sizeof(T));
    }

    int Write(uint64_t address, void *buffer, size_t size)
    {
        return KWriteProcessMemory(address, buffer, size);
    }

public: // 外部触摸接口
    void TouchDown(int slot, int x, int y, int screenW, int screenH)
    {
        HandleTouchEvent(sm_req_op::op_down, slot, x, y, screenW, screenH);
    }

    void TouchMove(int slot, int x, int y, int screenW, int screenH)
    {
        HandleTouchEvent(sm_req_op::op_move, slot, x, y, screenW, screenH);
    }

    void TouchUp(int slot) { HandleTouchEvent(sm_req_op::op_up, slot, 1, 1, 1, 1); }

public: // 外部获取内存信息
    // 获取进程内存信息(刷新)
    int GetMemoryInformation()
    {
        return GetMemoryInfo();
    }

    // 获取内部结构体实例 内部成员调用不需要显示使用this指针，隐式this
    const memory_info &GetMemoryInfoRef() const
    {
        return req->mem_info;
    }

    // 获取模块地址，true为起始地址，false为结束地址
    bool GetModuleAddress(std::string_view moduleName, short segmentIndex, uint64_t *outAddress, bool isStart)
    {
        if (!outAddress)
        {
            std::println(stderr, "outAddress 为空指针");
            return false;
        }

        *outAddress = 0;

        if (GetMemoryInfo() != 0)
        {
            std::println(stderr, "驱动获取内存信息失败");
            return false;
        }

        const auto &info = GetMemoryInfoRef();

        for (int i = 0; i < info.module_count; ++i)
        {
            const auto &mod = info.modules[i];

            std::string_view fullPath(mod.name);

            if (fullPath.length() < moduleName.length())
                continue;

            size_t pos = fullPath.length() - moduleName.length();
            if (pos > 0 && fullPath[pos - 1] != '/')
                continue;
            if (fullPath.substr(pos) != moduleName)
                continue;

            std::println(stderr, "========== 模块信息 ==========");
            std::println(stderr, "  模块索引  : {}", i);
            std::println(stderr, "  模块名称  : {}", mod.name);
            std::println(stderr, "  区段数量  : {}", mod.seg_count);
            std::println(stderr, "  ----------------------------");

            for (int j = 0; j < mod.seg_count; ++j)
            {
                const auto &seg = mod.segs[j];
                std::println(stderr, "  区段[{}]:", j);
                std::println(stderr, "    index : {}", seg.index);
                std::println(stderr, "    start : 0x{:016X}", seg.start);
                std::println(stderr, "    end   : 0x{:016X}", seg.end);
                std::println(stderr, "    size  : 0x{:X} ({} bytes)", seg.end - seg.start, seg.end - seg.start);
                std::println(stderr, "    prot  : {}", seg.prot);
            }

            std::println(stderr, "==============================");

            // 查找目标区段
            for (int j = 0; j < mod.seg_count; ++j)
            {
                const auto &seg = mod.segs[j];
                if (seg.index != segmentIndex)
                    continue;

                *outAddress = isStart ? seg.start : seg.end;
                return true;
            }

            std::println(stderr, " 模块 '{}' 中未找到区段索引 {}", moduleName, segmentIndex);
            return false;
        }

        std::println(stderr, " 未找到模块 '{}'", moduleName);
        return false;

        // 反作弊 VMA 碎裂与诱饵对抗机制，这个已经在内核层修复了，想知道的看lsdriver\process_memory_enum.h
    }
    // 驱动获取扫描区域
    std::vector<std::pair<uintptr_t, uintptr_t>> GetScanRegions()
    {
        std::vector<std::pair<uintptr_t, uintptr_t>> regions;

        if (GetMemoryInfo() != 0)
        {
            std::println(stderr, "驱动获取内存信息失败");
            return regions;
        }

        const auto &info = GetMemoryInfoRef();

        // 预分配空间 (堆内存数量 + 模块数量 * 平均段数)
        regions.reserve(info.region_count + info.module_count * 3);

        //  压入所有匿名的堆内存区域
        for (int i = 0; i < info.region_count; ++i)
        {
            const auto &r = info.regions[i];
            if (r.end > r.start)
                regions.emplace_back(r.start, r.end);
        }

        // 压入所有模块的静态基址区域
        for (int i = 0; i < info.module_count; ++i)
        {
            const auto &mod = info.modules[i];
            for (int j = 0; j < mod.seg_count; ++j)
            {
                const auto &seg = mod.segs[j];
                regions.emplace_back(seg.start, seg.end);
            }
        }

        if (!regions.empty())
        {
            std::sort(regions.begin(), regions.end(), [](const auto &a, const auto &b)
                      { return a.first < b.first; });
        }

        return regions;
    }

    /*
    问题:
    在 Android/Linux 中，PC 指向的代码地址不一定都来自 libxxx.so 这类
    文件映射模块。除了 linker 正常加载的 ELF 代码段以外，进程也可以在
    运行时通过 mmap/mprotect/memfd/ashmem/devzero 等方式创建一块可执行内存，然后把机器码写入这块内存并跳转执行。

    这种区域在 /proc/<pid>/maps 不会显示为某个 so 文件路径，
    而可能显示为:
        [anon:xxx]
        /dev/zero (deleted)
        /memfd:jit-cache (deleted)
        /dev/ashmem/xxx (deleted)
    或者没有路径名的匿名映射，驱动进行了排除dev路径和模块外的匿名内存

    常见来源包括:
    1. JIT / 脚本引擎 / VM 动态代码缓存
        运行时生成 ARM64 机器码，写入 mmap 出来的内存，再赋予可执行权限
        或者使用 RW/X 双映射方式执行。

    2. 壳 / 保护 / 热更新代码
        真实代码可能被加密保存在文件、资源或网络数据中，运行时解密后写入
        匿名可执行内存，再通过跳转、回调、函数指针等方式进入执行。

3. 手动 ELF Loader / 自定义 Linker
   程序不一定调用系统 dlopen 加载核心代码，而是自己解析 ELF:
       - 解析 ELF Header / Program Header
       - mmap 映射 PT_LOAD 段
       - 处理重定位
       - 修复 GOT/PLT
       - 处理符号解析
       - 调用 init_array / 构造函数
   这种情况下，代码虽然逻辑上来自某个 ELF，但在 maps 中可能没有原始
   so 文件路径，因此无法按普通模块名归属。

4. Hook /  跳板
   Hook 框架或保护代码可能申请一小段可执行内存，用来存放跳板指令、桩代码或中转逻辑。

注意:
  这种区域地址位置受 ASLR、mmap 分配策略、内存碎片等因素影响。
  权限是 rwx / rwxs，说明该区域同时可写可执行，通常更像动态代码、JIT 代码、解密代码或跳板代码。
*/
    bool DumpModule(std::string_view moduleName)
    {

        // 判断是否是需要修正地址的动态标签
        auto IsRelocatableDynamicTag = [](uint64_t tag) -> bool
        {
            return tag == DT_PLTGOT || tag == DT_HASH || tag == DT_STRTAB ||
                   tag == DT_SYMTAB || tag == DT_RELA || tag == DT_INIT ||
                   tag == DT_FINI || tag == DT_REL || tag == DT_JMPREL ||
                   tag == DT_INIT_ARRAY || tag == DT_FINI_ARRAY || tag == DT_GNU_HASH ||
                   tag == 0x6000000F /* DT_ANDROID_REL */ ||
                   tag == 0x60000011 /* DT_ANDROID_RELA */ ||
                   tag == 0x60000012 /* DT_ANDROID_REL_OFFSET */;
        };

        if (GetMemoryInformation() != 0)
        {
            std::println(stderr, "[-] Dump: 驱动获取内存信息失败");
            return false;
        }

        const auto &info = GetMemoryInfoRef();
        const module_info *targetMod = nullptr;

        for (int i = 0; i < info.module_count; ++i)
        {
            const auto &mod = info.modules[i];
            std::string_view fullPath(mod.name);

            if (fullPath.ends_with(moduleName))
            {
                if (fullPath.length() == moduleName.length() ||
                    fullPath[fullPath.length() - moduleName.length() - 1] == '/')
                {
                    targetMod = &mod;
                    break;
                }
            }
        }

        if (!targetMod)
        {
            std::println(stderr, "[-] Dump: 未找到模块 '{}'", moduleName);
            return false;
        }

        // 确定模块的内存跨度
        uint64_t baseAddr = ~0ULL;
        uint64_t maxEnd = 0;

        for (int i = 0; i < targetMod->seg_count; ++i)
        {
            const auto &seg = targetMod->segs[i];
            if (seg.start < baseAddr)
                baseAddr = seg.start;
            if (seg.end > maxEnd)
                maxEnd = seg.end;
        }

        uint64_t imageSize = maxEnd - baseAddr;
        constexpr uint64_t MAX_DUMP_SIZE = 1024ULL * 1024 * 500; // 500MB 防御 OOM

        if (baseAddr >= maxEnd || baseAddr == ~0ULL || imageSize == 0 || imageSize > MAX_DUMP_SIZE)
        {
            std::println(stderr, "[-] Dump: 模块边界无效或跨度过大 (0x{:X} 字节)", imageSize);
            return false;
        }

        std::println(stdout, "[*] 模块: {}", moduleName);
        std::println(stdout, "[*] 基址: 0x{:X}", baseAddr);
        std::println(stdout, "[*] 结束: 0x{:X}", maxEnd);
        std::println(stdout, "[*] 跨度: 0x{:X} ({} MB)", imageSize, imageSize / 1024 / 1024);

        // 分配缓冲区 (默认全 0，完美兼容 BSS 和 PROT_NONE 空白页)
        std::vector<uint8_t> image(imageSize, 0);

        // 分层内存读取
        size_t totalRead = 0;
        size_t failedPages = 0;

        for (int i = 0; i < targetMod->seg_count; ++i)
        {
            const auto &seg = targetMod->segs[i];
            if (seg.start >= seg.end)
                continue;

            uint64_t segOffset = seg.start - baseAddr;
            uint64_t segSize = seg.end - seg.start;

            // 尝试一次性读取整个内存段提高速度
            if (KReadProcessMemory(seg.start, image.data() + segOffset, segSize) > 0)
            {
                totalRead += segSize;
            }
            else
            {
                // 如果整段读取失败（中间有保护页或未分配页），降级为分页扫描抢救
                for (uint64_t off = 0; off < segSize; off += PAGE_SIZE)
                {
                    size_t toRead = std::min((uint64_t)PAGE_SIZE, segSize - off);
                    if (KReadProcessMemory(seg.start + off, image.data() + segOffset + off, toRead) > 0)
                    {
                        totalRead += toRead;
                    }
                    else
                    {
                        failedPages++; // 读取失败的页保持为 0x00，不影响整体结构
                    }
                }
            }
        }
        std::println(stdout, "[*] 读取完成: 成功 0x{:X} 字节, 失败 {} 页", totalRead, failedPages);

        // ELF 修复
        if (totalRead >= sizeof(Elf64_Ehdr))
        {
            Elf64_Ehdr *ehdr = reinterpret_cast<Elf64_Ehdr *>(image.data());

            if (__builtin_memcmp(ehdr->e_ident, ELFMAG, SELFMAG) == 0 && ehdr->e_ident[EI_CLASS] == ELFCLASS64)
            {
                std::println(stdout, "[*] ELF 虚拟基址: 0x0 (1:1 内存映射展开)");

                // 抹除无效的 Section Headers
                ehdr->e_shoff = 0;
                ehdr->e_shnum = 0;
                ehdr->e_shstrndx = SHN_UNDEF;

                // 验证 Phdr 偏移是否在安全范围内
                if (ehdr->e_phoff + (ehdr->e_phnum * sizeof(Elf64_Phdr)) <= imageSize)
                {
                    Elf64_Phdr *phdrs = reinterpret_cast<Elf64_Phdr *>(image.data() + ehdr->e_phoff);
                    Elf64_Phdr *dynPhdr = nullptr;

                    std::println(stdout, "[*] 修复 ELF Program Headers ({} 个)...", ehdr->e_phnum);

                    // 修复 Program Headers 映射
                    int load_idx = 1;
                    for (int i = 0; i < ehdr->e_phnum; ++i)
                    {
                        Elf64_Phdr &ph = phdrs[i];

                        if (ph.p_type == PT_LOAD)
                        {
                            uint64_t old_offset = ph.p_offset;
                            uint64_t old_filesz = ph.p_filesz;

                            // 强制 1:1 映射并展开 BSS
                            ph.p_offset = ph.p_vaddr;
                            ph.p_filesz = ph.p_memsz;

                            std::println(stdout, "  LOAD[{}]: vaddr=0x{:X}  offset: 0x{:X} -> 0x{:X}  filesz: 0x{:X} -> 0x{:X}  memsz=0x{:X}",
                                         load_idx++, ph.p_vaddr, old_offset, ph.p_offset, old_filesz, ph.p_filesz, ph.p_memsz);
                        }
                        else if (ph.p_type == PT_DYNAMIC)
                        {
                            dynPhdr = &ph;
                            ph.p_offset = ph.p_vaddr;
                            ph.p_filesz = ph.p_memsz;
                        }
                    }

                    //  修复 PT_DYNAMIC (恢复导入/导出/字符串表)
                    if (dynPhdr && (dynPhdr->p_offset + dynPhdr->p_filesz <= imageSize))
                    {
                        Elf64_Dyn *dynTable = reinterpret_cast<Elf64_Dyn *>(image.data() + dynPhdr->p_offset);
                        size_t dynCount = dynPhdr->p_filesz / sizeof(Elf64_Dyn);

                        for (size_t i = 0; i < dynCount; ++i)
                        {
                            if (dynTable[i].d_tag == DT_NULL)
                                break;

                            // 还原被 Linker 加上基址的绝对地址指针
                            if (IsRelocatableDynamicTag(dynTable[i].d_tag))
                            {
                                if (dynTable[i].d_un.d_ptr >= baseAddr && dynTable[i].d_un.d_ptr < maxEnd)
                                {
                                    dynTable[i].d_un.d_ptr -= baseAddr;
                                }
                            }
                        }
                        std::println(stdout, "[*] ELF 修复完成 (PT_DYNAMIC 符号表已重建)");
                    }
                }
            }
            else
            {
                std::println(stderr, "[-] 警告: ELF Magic 校验失败，可能是头部被反Dump抹除，保存原始数据");
            }
        }

        mkdir("/sdcard/dump", 0777); // 忽略已存在错误

        size_t slashPos = moduleName.find_last_of('/');
        std::string_view baseName = (slashPos == std::string_view::npos) ? moduleName : moduleName.substr(slashPos + 1);
        std::string outPath = "/sdcard/dump/" + std::string(baseName) + ".dump.so";

        FILE *fp = fopen(outPath.c_str(), "wb");
        if (!fp)
        {
            std::println(stderr, "[-] Dump: 无法创建文件 {} (请检查读写权限)", outPath);
            return false;
        }

        fwrite(image.data(), 1, imageSize, fp);
        fclose(fp);

        std::println(stdout, "[+] ==========================================");
        std::println(stdout, "[+] Dump 完成!");
        std::println(stdout, "[+] 路径: {}", outPath);
        std::println(stdout, "[+] 大小: 0x{:X} ({} MB)", imageSize, imageSize / 1024 / 1024);
        std::println(stdout, "[+] ==========================================");

        return true;
    }

public: // 外部硬件断点接口
    // 获取断点结构体信息
    const hwbp_info &GetHwbpInfoRef()
    {
        GetHwbpInfo();
        return req->bp_info;
    }
    // 设置断点
    int SetProcessHwbpRef(uint64_t target_addr, hwbp_type bt, hwbp_scope bs, hwbp_len bl)
    {
        return SetProcessHwbp(target_addr, bt, bs, bl);
    }
    // 删除断点
    void RemoveProcessHwbpRef()
    {
        RemoveProcessHwbp();
    }

    // 删除指定索引内容
    void RemoveHwbpRecord(int index)
    {
        if (index < 0 || index >= req->bp_info.record_count)
            return;
        const int tail_count = req->bp_info.record_count - index - 1;
        if (tail_count > 0)
            __builtin_memmove(&req->bp_info.records[index], &req->bp_info.records[index + 1], static_cast<size_t>(tail_count) * sizeof(hwbp_record));
        req->bp_info.record_count--;
        __builtin_memset(&req->bp_info.records[req->bp_info.record_count], 0, sizeof(hwbp_record));
    }

private: // 私有实现，外部无需关系
    struct req_obj *req = nullptr;
    int global_pid = 0;

    inline void IoCommitAndWait()
    {
        req->kernel = true;

        while (!req->user)
        {
            __builtin_arm_yield();
        };

        req->user = false;
    }

    // 初始化驱动
    void InitCommunication()
    {
        prctl(PR_SET_NAME, "LS", 0, 0, 0);

        req = (req_obj *)mmap((void *)0x2025827000, sizeof(req_obj), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

        if (req == MAP_FAILED)
        {
            printf("[-] 分配共享内存失败，错误码: %d (%s)\n", errno, strerror(errno));
            return;
        }
        __builtin_memset(req, 0, sizeof(req_obj));

        printf("[+] 分配虚拟地址成功，地址: %p  大小: %lu\n", req, sizeof(req_obj));
        printf("当前进程 PID: %d\n", getpid());
        printf("等待驱动握手...\n");

        IoCommitAndWait();

        printf("驱动已经连接\n");
    }

    // 初始化触摸
    void InitTouch()
    {
        req->op = op_init_touch;
        IoCommitAndWait();
    }

    // 读写
    int KReadProcessMemory(uint64_t addr, void *buffer, size_t size)
    {

        std::scoped_lock<SpinLock> lock(m_mutex);

        // 大数据自动分片，防止缓冲区溢出覆盖触摸数据
        if (size > 0x1000)
        {
            size_t processed = 0;
            while (processed < size)
            {
                size_t chunk = (size - processed > 0x1000) ? 0x1000 : (size - processed);
                req->op = op_r;
                req->pid = global_pid;
                req->target_addr = addr + processed;
                req->size = chunk;
                IoCommitAndWait();

                if (req->status <= 0)
                    return req->status;
                __builtin_memcpy((uint8_t *)buffer + processed, req->user_buffer, chunk);
                processed += chunk;
            }
            return req->status;
        }

        // 小数据快速通道
        req->op = op_r;
        req->pid = global_pid;
        req->target_addr = addr;
        req->size = size;

        IoCommitAndWait();

        // 失败时清空并返回错误码
        if (req->status <= 0)
            return req->status;

        // 极限性能且安全的内存拷贝 (防未对齐崩溃)
        switch (size)
        {
        case 1:
            __builtin_memcpy(buffer, req->user_buffer, 1);
            break;
        case 2:
            __builtin_memcpy(buffer, req->user_buffer, 2);
            break;
        case 4:
            __builtin_memcpy(buffer, req->user_buffer, 4);
            break;
        case 8:
            __builtin_memcpy(buffer, req->user_buffer, 8);
            break;
        default:
            __builtin_memcpy(buffer, req->user_buffer, size);
            break;
        }

        return req->status;
    }

    int KWriteProcessMemory(uint64_t addr, void *buffer, size_t size)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);

        // 大数据自动分片，防止本地拷贝时溢出覆盖触摸数据
        if (size > 0x1000)
        {
            size_t processed = 0;
            while (processed < size)
            {
                size_t chunk = (size - processed > 0x1000) ? 0x1000 : (size - processed);
                req->op = op_w;
                req->pid = global_pid;
                req->target_addr = addr + processed;
                req->size = chunk;
                __builtin_memcpy(req->user_buffer, (uint8_t *)buffer + processed, chunk);
                IoCommitAndWait();

                if (req->status <= 0)
                    return req->status;
                processed += chunk;
            }
            return req->status;
        }

        // 小数据快速通道
        req->op = op_w;
        req->pid = global_pid;
        req->target_addr = addr;
        req->size = size;

        switch (size)
        {
        case 1:
            __builtin_memcpy(req->user_buffer, buffer, 1);
            break;
        case 2:
            __builtin_memcpy(req->user_buffer, buffer, 2);
            break;
        case 4:
            __builtin_memcpy(req->user_buffer, buffer, 4);
            break;
        case 8:
            __builtin_memcpy(req->user_buffer, buffer, 8);
            break;
        default:
            __builtin_memcpy(req->user_buffer, buffer, size);
            break;
        }

        IoCommitAndWait();

        return req->status;
    }

    // 获取进程内存信息
    int GetMemoryInfo()
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = op_m;
        req->pid = global_pid;
        IoCommitAndWait();
        return req->status;
    }

    // 触摸事件
    void HandleTouchEvent(sm_req_op op, int slot, int x, int y, int screenW, int screenH)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);

        // 下面代码绝对不要使用整数除法
        if (screenW <= 0 || screenH <= 0 || req->POSITION_X <= 0 || req->POSITION_Y <= 0)
            return;

        req->op = op;
        req->slot = slot;
        // 浮点运算提到前面，保持清晰
        double normX = static_cast<double>(x) / screenW;
        double normY = static_cast<double>(y) / screenH;

        // 横竖屏映射逻辑
        if (screenW > screenH && req->POSITION_X < req->POSITION_Y)
        {
            // 右侧充电口模式
            req->x = static_cast<int>((1.0 - normY) * req->POSITION_X);
            req->y = static_cast<int>(normX * req->POSITION_Y);

            // 左侧充电口模式
            // req->x = static_cast<int>((double)y / screenH * req->POSITION_X);
            // req->y = static_cast<int>((1.0 - (double)x / screenW) * req->POSITION_Y);
        }
        else
        {
            // 正常映射
            req->x = static_cast<int>(normX * req->POSITION_X);
            req->y = static_cast<int>(normY * req->POSITION_Y);
        }

        IoCommitAndWait();
    }

    // 获取执行断点和访问断点信息
    void GetHwbpInfo()
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = op_brps_weps_info;
        IoCommitAndWait();
    }

    // 设置进程断点(断点只要触发驱动就会向hwbp_info写值，外部获取引用循环读取就行)
    int SetProcessHwbp(uint64_t target_addr, hwbp_type bt, hwbp_scope bs, hwbp_len bl = HWBP_BREAKPOINT_LEN_8)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = op_set_process_hwbp;
        req->pid = global_pid;
        req->target_addr = target_addr;
        req->bt = bt;
        req->bl = bl;
        req->bs = bs;
        IoCommitAndWait();
        return req->status;
    }

    // 删除进程断点
    void RemoveProcessHwbp()
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = op_remove_process_hwbp;
        IoCommitAndWait();
    }
};

Driver dr(1);

namespace SignatureScanner
{

    /*
    【特征码格式】仅两种 Token：
        ??    — 通配符
        XXh   — 十六进制字节 (如 A1h FFh 00h)

    【使用前提】外部已调用 dr.SetGlobalPid(pid) 设置目标进程

    【三个核心功能】
        1. 找特征  ScanAddressSignature(addr, range)
        2. 过滤特征 FilterSignature(addr)
        3. 扫特征码 ScanSignature(pattern, range) / ScanSignatureFromFile()
    【调用方式】
        外部设置好 PID
        dr.SetGlobalPid(pid);

        1. 找特征
        ScanAddressSignature(0x7A12345678, 100);

        2. 过滤特征（多次调用，每次传入当前地址或者重启后的新地址）
        FilterSignature(0x7A12345678);

        3. 扫特征码
        auto results = ScanSignature("A1h ?? FFh 00h", 100);
        或从文件扫
        auto results2 = ScanSignatureFromFile();
    */

    inline constexpr int SIG_MAX_RANGE = 1200;
    inline constexpr size_t SIG_BUFFER_SIZE = 0x8000;
    inline constexpr const char *SIG_DEFAULT_FILE = "Signature.txt";

    struct SigElement
    {
        std::vector<uint8_t> bytes;
        std::vector<bool> mask;
        bool empty() const { return bytes.empty(); }
        size_t size() const { return bytes.size(); }
        void clear()
        {
            bytes.clear();
            mask.clear();
        }
    };

    struct SigFilterResult
    {
        bool success = false;
        int changedCount = 0;
        int totalCount = 0;
        std::string oldSignature;
        std::string newSignature;
    };

    namespace
    {
        std::string NormalizeSigFileName(const char *filename)
        {
            if (filename != nullptr && *filename != '\0')
                return std::string(filename);
            return std::string(SIG_DEFAULT_FILE);
        }

        bool IsAbsoluteSigPath(std::string_view path)
        {
            return !path.empty() && path.front() == '/';
        }

        std::string ResolveSigPath(std::string_view path)
        {
            if (IsAbsoluteSigPath(path))
                return std::string(path);
            return std::string("/data/akernel/") + std::string(path);
        }

        std::string FormatSignature(const SigElement &sig)
        {
            if (sig.empty())
                return "";
            std::string result;
            result.reserve(sig.size() * 4);
            for (size_t i = 0; i < sig.bytes.size(); ++i)
            {
                if (i > 0)
                    result += ' ';
                if (!sig.mask[i])
                    result += "??";
                else
                    std::format_to(std::back_inserter(result), "{:02X}h", sig.bytes[i]);
            }
            return result;
        }

        SigElement ParseSignature(std::string_view text)
        {
            SigElement sig;
            if (text.empty())
                return sig;

            std::istringstream iss{std::string{text}};
            std::string token;

            while (iss >> token)
            {
                if (token == "??" || token == "?")
                {
                    sig.bytes.push_back(0);
                    sig.mask.push_back(false);
                }
                else
                {
                    std::string hex = token;
                    if (!hex.empty() && std::tolower(hex.back()) == 'h')
                        hex.pop_back();

                    unsigned val = 0;
                    auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), val, 16);
                    if (ec == std::errc() && val <= 0xFF)
                    {
                        sig.bytes.push_back(static_cast<uint8_t>(val));
                        sig.mask.push_back(true);
                    }
                    else
                    {
                        std::println(stderr, "[ParseSignature] 无法解析: '{}'", token);
                        sig.clear();
                        return sig;
                    }
                }
            }
            return sig;
        }

        bool MatchSignature(const uint8_t *data, const SigElement &sig)
        {
            for (size_t i = 0; i < sig.size(); ++i)
                if (sig.mask[i] && data[i] != sig.bytes[i])
                    return false;
            return true;
        }

        // 核心扫描循环
        std::vector<uintptr_t> ScanCore(const SigElement &sig, int rangeOffset)
        {
            std::vector<uintptr_t> matches;
            if (sig.empty())
                return matches;

            auto regions = dr.GetScanRegions();
            if (regions.empty())
                return matches;

            const size_t sigSize = sig.size();
            std::vector<uint8_t> buffer(SIG_BUFFER_SIZE);
            const size_t step = (SIG_BUFFER_SIZE > sigSize) ? (SIG_BUFFER_SIZE - sigSize) : 1;

            for (const auto &[rStart, rEnd] : regions)
            {
                if (rEnd - rStart < sigSize)
                    continue;

                for (uintptr_t addr = rStart; addr + sigSize <= rEnd; addr += step)
                {
                    size_t readSize = std::min(static_cast<size_t>(rEnd - addr), SIG_BUFFER_SIZE);
                    if (readSize < sigSize)
                        break;
                    if (dr.Read(addr, buffer.data(), readSize) <= 0)
                        continue;

                    size_t searchEnd = readSize - sigSize;
                    for (size_t off = 0; off <= searchEnd; ++off)
                    {
                        if (MatchSignature(buffer.data() + off, sig))
                            matches.push_back(addr + off + rangeOffset);
                    }
                }
            }
            return matches;
        }

        bool ReadSigFile(const char *filename, int &range, std::string &sigText)
        {
            std::ifstream fp(filename);
            if (!fp)
                return false;

            range = 0;
            sigText.clear();
            std::string line;

            while (std::getline(fp, line))
            {
                while (!line.empty() && (line.back() == '\n' || line.back() == '\r'))
                    line.pop_back();

                if (line.starts_with("范围:"))
                {
                    auto sub = line.substr(line.find(':') + 1);
                    auto it = std::ranges::find_if(sub, ::isdigit);
                    if (it != sub.end())
                        std::from_chars(&*it, sub.data() + sub.size(), range);
                }
                else if (line.starts_with("特征码:"))
                {
                    auto sub = line.substr(line.find(':') + 1);
                    if (auto f = sub.find_first_not_of(' '); f != std::string::npos)
                        sigText = sub.substr(f);
                }
            }
            return (range > 0 && !sigText.empty());
        }

        bool WriteSigFile(const char *filename, uintptr_t addr, int range, const SigElement &sig)
        {
            std::ofstream fp(filename);
            if (!fp)
                return false;
            std::println(fp, "目标地址: 0x{:X}", addr);
            std::println(fp, "范围: {}", range);
            std::println(fp, "总字节: {}", sig.size());
            std::println(fp, "特征码: {}", FormatSignature(sig));
            return !fp.fail();
        }

        bool ReadSigFileWithFallback(const char *filename, int &range, std::string &sigText)
        {
            const std::string rawName = NormalizeSigFileName(filename);
            if (ReadSigFile(rawName.c_str(), range, sigText))
                return true;
            if (!IsAbsoluteSigPath(rawName))
            {
                const std::string fallback = ResolveSigPath(rawName);
                return ReadSigFile(fallback.c_str(), range, sigText);
            }
            return false;
        }

        bool WriteSigFileWithFallback(const char *filename, uintptr_t addr, int range, const SigElement &sig)
        {
            const std::string rawName = NormalizeSigFileName(filename);
            if (WriteSigFile(rawName.c_str(), addr, range, sig))
                return true;
            if (!IsAbsoluteSigPath(rawName))
            {
                const std::string fallback = ResolveSigPath(rawName);
                return WriteSigFile(fallback.c_str(), addr, range, sig);
            }
            return false;
        }

    } // anonymous namespace

    // 找特征
    bool ScanAddressSignature(uintptr_t addr, int range, const char *filename = SIG_DEFAULT_FILE)
    {
        if (range <= 0 || range > SIG_MAX_RANGE)
        {
            std::println(stderr, "[找特征] range 无效: {} (1-{})", range, SIG_MAX_RANGE);
            return false;
        }
        if (addr < static_cast<uintptr_t>(range))
        {
            std::println(stderr, "[找特征] 地址过小会下溢");
            return false;
        }

        size_t totalSize = static_cast<size_t>(range) * 2;
        SigElement sig;
        sig.bytes.resize(totalSize);

        if (dr.Read(addr - range, sig.bytes.data(), totalSize) <= 0)
        {
            std::println(stderr, "[找特征] 读取失败: 0x{:X}", addr - range);
            return false;
        }

        sig.mask.assign(totalSize, true);

        if (!WriteSigFileWithFallback(filename, addr, range, sig))
        {
            std::println(stderr, "[找特征] 写文件失败: {}", filename);
            return false;
        }

        std::println("[找特征] 完成 地址:0x{:X} 范围:±{} 字节:{}", addr, range, totalSize);
        return true;
    }

    // 过滤特征
    SigFilterResult FilterSignature(uintptr_t addr, const char *filename = SIG_DEFAULT_FILE)
    {
        SigFilterResult result;

        int range = 0;
        std::string oldSigText;
        if (!ReadSigFileWithFallback(filename, range, oldSigText))
        {
            std::println(stderr, "[过滤特征] 读取文件失败: {}", filename);
            return result;
        }

        SigElement oldSig = ParseSignature(oldSigText);
        if (oldSig.empty())
        {
            std::println(stderr, "[过滤特征] 特征码解析失败");
            return result;
        }

        if (addr < static_cast<uintptr_t>(range))
        {
            std::println(stderr, "[过滤特征] 地址过小");
            return result;
        }

        size_t totalSize = static_cast<size_t>(range) * 2;
        std::vector<uint8_t> curData(totalSize);

        if (dr.Read(addr - range, curData.data(), totalSize) <= 0)
        {
            std::println(stderr, "[过滤特征] 读取失败: 0x{:X}", addr - range);
            return result;
        }

        size_t cmpSize = std::min(oldSig.size(), curData.size());
        SigElement newSig;
        newSig.bytes.resize(cmpSize);
        newSig.mask.resize(cmpSize);
        result.totalCount = static_cast<int>(cmpSize);

        for (size_t i = 0; i < cmpSize; ++i)
        {
            if (!oldSig.mask[i])
            {
                newSig.bytes[i] = 0;
                newSig.mask[i] = false;
            }
            else if (oldSig.bytes[i] != curData[i])
            {
                newSig.bytes[i] = 0;
                newSig.mask[i] = false;
                ++result.changedCount;
            }
            else
            {
                newSig.bytes[i] = curData[i];
                newSig.mask[i] = true;
            }
        }

        result.oldSignature = oldSigText;
        result.newSignature = FormatSignature(newSig);

        WriteSigFileWithFallback(filename, addr, range, newSig);

        result.success = true;
        std::println("[过滤特征] 完成 总字节:{} 变化:{}", result.totalCount, result.changedCount);
        return result;
    }

    //  扫特征码
    std::vector<uintptr_t> ScanSignature(const char *pattern, int range = 0)
    {
        SigElement sig = ParseSignature(pattern);
        if (sig.empty())
        {
            std::println(stderr, "[扫特征码] 解析失败");
            return {};
        }

        std::println("[扫特征码] 开始 长度:{} 偏移:{}", sig.size(), range);
        auto matches = ScanCore(sig, range);
        std::println("[扫特征码] 完成 找到 {} 个匹配", matches.size());
        return matches;
    }
    // 从文件中扫
    std::vector<uintptr_t> ScanSignatureFromFile(const char *filename = SIG_DEFAULT_FILE)
    {
        int range = 0;
        std::string sigText;
        if (!ReadSigFileWithFallback(filename, range, sigText))
        {
            std::println(stderr, "[扫特征码] 读取文件失败: {}", filename);
            return {};
        }

        SigElement sig = ParseSignature(sigText);
        if (sig.empty())
            return {};

        std::println("[扫特征码] 开始 长度:{} 范围偏移:{}", sig.size(), range);
        auto matches = ScanCore(sig, range);
        std::println("[扫特征码] 完成 找到 {} 个匹配", matches.size());

        const std::string outPath = ResolveSigPath(NormalizeSigFileName(filename));
        std::ofstream out(outPath, std::ios::app);
        if (out)
        {
            std::println(out, "\n扫描结果: {} 个", matches.size());
            for (auto a : matches)
                std::println(out, "0x{:X}", a);
        }

        return matches;
    }

}
