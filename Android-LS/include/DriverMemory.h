#pragma once
#include <cstdio>
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#include <atomic>
#include <thread>
#include <mutex>
#include <sys/prctl.h>
#include <stdio.h>
#include <iostream>
#include <vector>
#include <list>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <map>
#include <sstream>
#include <fstream>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <ranges>
#include <format>
#include <concepts>
#include <variant>
#include <optional>
#include <charconv>
#include <dirent.h>
#include <cstdio>
#include <cstdlib>
#include <unistd.h>
#include <fcntl.h>
#include <unistd.h>
#include <cstdio>
#include <cstring>
#include <atomic>
#include <sys/stat.h>
#include <sys/types.h>
#include <string>
#include <ranges>
#include <format>
#include <print>
#include <algorithm>
#include <iterator>
#include <string>
#include <vector>
#include <elf.h>

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


*/

__attribute__((noinline))                                               // 禁止该类所有成员函数成员变量内联
__attribute__((optimize("-fno-reorder-blocks,-fno-reorder-functions"))) // 禁止编译器重排代码
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

    // 断点类型
    enum bp_type
    {
        BP_READ,       // 读
        BP_WRITE,      // 写
        BP_READ_WRITE, // 读写
        BP_EXECUTE     // 执行
    } __attribute__((packed));

    // 断点作用线程范围
    enum bp_scope
    {
        SCOPE_MAIN_THREAD,   // 仅主线程
        SCOPE_OTHER_THREADS, // 仅其他子线程
        SCOPE_ALL_THREADS    // 全部线程
    } __attribute__((packed));

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

    } __attribute__((packed));

    // 存储整体命中信息
    struct hwbp_info
    {
        uint64_t num_brps;                 // 执行断点的数量
        uint64_t num_wrps;                 // 访问断点的数量
        uint64_t hit_addr;                 // 监控的地址
        int record_count;                  // 当前已记录的不同 PC 数量
        struct hwbp_record records[0x100]; // 记录不同 PC 触发状态的数组
    } __attribute__((packed));

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
    } __attribute__((packed));

    struct module_info
    {
        char name[MOD_NAME_LEN];
        int seg_count;
        struct segment_info segs[MAX_SEGS_PER_MODULE];
    } __attribute__((packed));

    struct region_info
    {
        uint64_t start;
        uint64_t end;
    } __attribute__((packed));

    struct memory_info
    {
        int module_count;                        // 总模块数量
        struct module_info modules[MAX_MODULES]; // 模块信息

        int region_count;                             // 总可扫描内存数量
        struct region_info regions[MAX_SCAN_REGIONS]; // 可扫描内存区域 (rw-p, 排除特殊区域)
    } __attribute__((packed));

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
    } __attribute__((packed));

    // 将在队列中使用的请求实例结构体
    struct req_obj
    {
        std::atomic<int> kernel; // 由用户模式设置 1 = 内核有待处理的请求, 0 = 请求已完成
        std::atomic<int> user;   // 由内核模式设置 1 = 用户模式有待处理的请求, 0 = 请求已完成

        enum sm_req_op op; // shared memory请求操作类型
        int status;        // 操作状态

        // 内存读取
        int pid;
        uint64_t target_addr;
        int size;
        uint8_t user_buffer[0x1000]; // 物理标准页大小

        // 进程内存信息
        struct memory_info mem_info;

        enum bp_type bt;          // 断点类型
        enum bp_scope bs;         // 断点作用线程范围
        int len_bytes;            // 断点长度字节
        struct hwbp_info bp_info; // 断点信息

        // 初始化触摸驱动返回屏幕维度
        int POSITION_X, POSITION_Y;
        // 触摸坐标
        int x, y;
    } __attribute__((packed));

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
        IoCommitAndWait();
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
        if (Read(address, buffer.data(), max_length))
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
        if (Read(address, buffer.data(), length * sizeof(char16_t)))
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
    void TouchDown(int x, int y, int screenW, int screenH)
    {
        HandleTouchEvent(sm_req_op::op_down, x, y, screenW, screenH);
    }

    void TouchMove(int x, int y, int screenW, int screenH)
    {
        HandleTouchEvent(sm_req_op::op_move, x, y, screenW, screenH);
    }

    void TouchUp() { HandleTouchEvent(sm_req_op::op_up, 1, 1, 1, 1); }

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

            // =============================================
            // 输出该模块的所有信息
            // =============================================
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
            // =============================================

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

        // 下面已经在内核层修复了，不管了，这里只做说明，解释原理
        /*
         * =========================================================================================
         * 反作弊 VMA 碎裂与诱饵对抗机制 (七步完全体)
         * =========================================================================================
         *
         * 【第一阶段：理想状态下的纯净内存布局 (原生 ELF 加载)】
         *
         * 当 Android 原生加载一个 libil2cpp.so 时，内存布局连续且规律。
         * 现代 64 位 Android（LLVM/Clang 编译）出于安全考虑，至少产生以下几个连续段：
         *
         *   PT_LOAD[0] (r--)  : ELF Header + 只读数据（.rodata、.eh_frame 等），真实基址起点。
         *   PT_LOAD[1] (r-x)  : .text 代码段，核心逻辑所在。
         *   PT_LOAD[2] (rw-)  : .data.rel.ro + RELRO 安全页，写完重定位后锁为只读的数据。
         *   PT_LOAD[3] (rw-)  : .data 全局变量段。
         *   BSS        (-w-/rw-) : 尾部额外分配的匿名读写内存（零初始化全局变量）。
         *
         * 即便没有反作弊，最纯净的环境也自然产生 [RO -> RX -> RW -> RW -> RW(anon)] 的天然区段。
         *
         * 【第二阶段：顶级反作弊的四重攻击手段】
         *
         *   攻击一：VMA 碎裂
         *   反作弊高频调用 mprotect() Hook 游戏函数，内核被迫将原本一整块 RX 代码段
         *   "劈碎"成几十甚至上百个细碎 VMA，部分页被改为 RWX 混合权限，
         *   彻底打乱原本连贯的天然区段。
         *
         *   攻击二：远端假诱饵
         *   反作弊在距离真实模块上百 MB 远的极低地址（如 0x6e32250000）凭空 mmap()
         *   一块假内存，命名为 libil2cpp.so，权限设为 RO。
         *   常规合并算法会误把假地址当成模块基址，导致读取指针全部失效。
         *
         *   攻击三：prot 权限污染
         *   代码段内部散布着少量 RWX 碎片（反作弊自身的 trampoline hook 页）。
         *   若在缝合阶段对权限做 OR 合并，RWX 碎片的 W 位会"传染"整个代码段，
         *   使本该是 RX 的代码段最终呈现为 RWX，干扰上层对段类型的判断。
         *
         *   攻击四：BSS 权限异化
         *   ACE 反作弊将 BSS 段的权限故意设为 -w-p（只写，无读权限）。
         *   若 BSS 检测逻辑要求 VM_READ|VM_WRITE，则此类 BSS 完全不可见，
         *   导致上层计算出的模块尾部地址偏短，BSS 内的全局变量无法定位。
         *
         * 【第三阶段：七步对抗算法 (完全体)】
         *
         *   步骤 1：纯物理排序
         *   无视所有权限和假象，按物理起始地址绝对升序排列所有碎片。
         *   应对极端的反作弊内核级乱序映射干扰。
         *
         *   步骤 2：改进版体积聚类 (寻找生命主干)
         *   ARM64 寻址限制要求真实 .so 内存紧凑相连。遍历碎片，相邻块缝隙超过
         *   16MB (0x1000000) 即视为"内存断层"，划分不同群落。
         *   累加每个群落的真实映射体积（严防重叠映射导致体积虚高），
         *   体积最大、最丰满的群落即为真实的 .so 本体。
         *
         *   步骤 3：物理抹杀假诱饵 + BSS 豁免保留
         *   锁定真实本体的 [best_base, best_end] 范围，剔除范围外的假诱饵碎片。
         *   豁免：index==-1 的匿名 BSS 段即便 end 超出 best_end，
         *   只要 start 在 best_end 附近（≤ 0x3000，一个 guard 页的余量），
         *   就视为合法的本体尾部延伸，保留并动态扩展 best_end。
         *   这直接解决了 ACE 将 BSS 权限设为 -w-p 后尾部被误杀的问题。
         *
         *   步骤 4：严谨拓扑标记 (核心：破解权限篡改)
         *   寻找天然的"防波堤"：向后扫描找到第一个"纯原生数据段 (有W无X)"。
         *   在 Header 和第一个数据段之间的所有碎片，无论现在权限是 RO 还是 RWX，
         *   物理拓扑上必定属于"核心代码段"，强制内部标签重置为 0(RX)。
         *   跨过数据段后恢复原生判定，绝不越界吞噬。
         *   内部临时标签约定：1=RO(头部), 0=RX(代码), 2=RW(数据), -1=BSS(保持不变)
         *
         *   步骤 4.5：强制规范化 prot (消除权限污染)
         *   反作弊的 RWX hook 页在步骤 4 中已被正确归入代码段（index=0），
         *   但其 W 位仍残留在 prot 字段中。
         *   此步骤根据步骤 4 确立的权威拓扑标签，强制覆写每个碎片的 prot：
         *     index=1(Header/RELRO) → prot=1(R)
         *     index=0(Code)         → prot=5(RX)
         *     index=2(Data)         → prot=3(RW)
         *     index=-1(BSS)         → prot=3(RW)  (同时修正 -w- 的异常权限)
         *   彻底断绝 prot 污染，使对外输出的 prot 与原生 ELF 加载完全一致。
         *
         *   步骤 5：拉链式精准缝合 (还原原生边界)
         *   遍历洗白后的碎片，仅当相邻碎片【首尾绝对相连】且【拓扑标签一致】时，
         *   进行无缝拉链式融合。天然的段边界（如 RX→RO、RO→RW）自然断开保留。
         *   缝合时不再合并 prot（步骤 4.5 已完成规范化，此处无需再动）。
         *
         *   步骤 6：最终 Index 序列化
         *   给缝合后的完美区段重新发放 0, 1, 2, 3... 的连续 Index，BSS 保留 -1。
         *
         * 【最终战果】：
         * 无论反作弊怎么切分、放诱饵、异化权限，跑完此算法后，
         * 产出结果与干净手机上的原生 ELF 映射 1:1 完全一致。
         *
         * 典型输出（libil2cpp.so，ACE 保护环境）：
         *   seg[0] index=0  prot=1(R)  → PT_LOAD[0] ELF Header
         *   seg[1] index=1  prot=5(RX) → PT_LOAD[1] .text 代码段
         *   seg[2] index=2  prot=3(RW) → PT_LOAD[2] .data.rel.ro
         *   seg[3] index=3  prot=1(R)  → RELRO 只读页
         *   seg[4] index=4  prot=3(RW) → PT_LOAD[3] .data
         *   seg[5] index=-1 prot=3(RW) → BSS (原始权限 -w-p，已被规范化)
         *   seg[6] index=5  prot=5(RX) → PT_LOAD[4]
         *   seg[7] index=6  prot=3(RW) → PT_LOAD[5]
         *   seg[8] index=7  prot=3(RW) → PT_LOAD[6]
         *
         * 外部调用：Base = info->modules[X].segs[0].start，即可获取绝对真实基址。
         * =========================================================================================
         */
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
    问题1:
    壳代码会通过 mmap 申请一块匿名内存（Anonymous Memory，分配时没有关联具体的文件路径）。
    壳将真正加密的核心代码、甚至整个原始的 .so 解密释放到这块匿名内存中，并赋予 PROT_EXEC (可执行) 权限。
    然后改变程序执行流，跳转到这块匿名内存中去执行。
    现象： 在调试器看，PC寄存器跑飞到了一个没有名字的内存段。它往往就紧挨着当前模块的最后一个段（通常是 .bss 段）之后被分配出来。
    问题2:
    有些大厂或恶意软件不使用系统自带的 dlopen 加载核心模块，而是自己写了一套 Linker。直接从内存或网络读取一段字节流，然后手动 mmap 一块内存，自己做符号解析和重定位。
    这种情况下，加载进来的核心代码在 /proc/pid/maps 中完全没有文件路径，自然也不属于任何已知模块。
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

            // 尝试一次性读取整个内存段，大幅提高速度
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
    int SetProcessHwbpRef(uint64_t target_addr, bp_type bt, bp_scope bs, int len_bytes)
    {
        return SetProcessHwbp(target_addr, bt, bs, len_bytes);
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
        req->kernel.store(1, std::memory_order_release);

        while (req->user.load(std::memory_order_acquire) != 1)
        {
            // 让出 CPU 时间片,100%降低性能的，不过保留主动降低性能功耗
            std::this_thread::yield();
        };

        req->user.store(0, std::memory_order_relaxed);
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
    void HandleTouchEvent(sm_req_op op, int x, int y, int screenW, int screenH)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);

        // 下面代码绝对不要使用整数除法
        if (screenW <= 0 || screenH <= 0 || req->POSITION_X <= 0 || req->POSITION_Y <= 0)
            return;

        req->op = op;

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
    int SetProcessHwbp(uint64_t target_addr, bp_type bt, bp_scope bs, int len_bytes = 8)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = op_set_process_hwbp;
        req->pid = global_pid;
        req->target_addr = target_addr;
        req->bt = bt;
        req->bs = bs;
        req->len_bytes = len_bytes;
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
