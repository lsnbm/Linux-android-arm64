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
#include <fstream>
#include <functional>
#include <iostream>
#include <iterator>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <optional>
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
#include <sys/mount.h>
#include <unistd.h>

#define PAGE_SIZE 4096
class Driver
{
public: // 共有结构体和锁
    // 轻量高性能自旋锁
    class SpinLock
    {
        unsigned char locked = 0;

    public:
        void lock() noexcept
        {
            while (__atomic_exchange_n(&locked, 1, __ATOMIC_ACQUIRE))
            {
                while (__atomic_load_n(&locked, __ATOMIC_RELAXED))
                {
                    asm volatile("yield" ::: "memory");
                }
            }
        }

        void unlock() noexcept
        {
            __atomic_store_n(&locked, 0, __ATOMIC_RELEASE);
        }
    };
    SpinLock m_mutex;

#define TLS_THREAD_NAME_LEN 16

    struct env_params
    {
        char thread_name[TLS_THREAD_NAME_LEN];
        uint64_t tpidr_el0;
        uint64_t pacga_lo;
        uint64_t pacga_hi;
        int tls_status;
        int pacga_status;
    };

// 寄存器操作类型定义
#define BP_OP_NONE    0x0 // 00: 不操作
#define BP_OP_READ    0x1 // 01: 读
#define BP_OP_WRITE   0x2 // 10: 写
#define BP_CONFIG_MAX 16

// 设置掩码位的宏，参数1:结构体指针，参数2:寄存器索引，参数3:操作类型
#define BP_SET_MASK(record, reg, op)                            \
    do                                                          \
    {                                                           \
        int byte_idx = (reg) >> 2;                              \
        int bit_offset = ((reg) & 0x3) << 1;                    \
        (record)->mask[byte_idx] &= ~(0x3 << bit_offset);       \
        (record)->mask[byte_idx] |= ((op) & 0x3) << bit_offset; \
    } while (0)

// 获取掩码位的宏，参数1:结构体指针，参数2:寄存器索引
#define BP_GET_MASK(record, reg) (((record)->mask[(reg) >> 2] >> (((reg) & 0x3) << 1)) & 0x3)

    // 断点类型
    enum bp_type
    {
        BP_BREAKPOINT_EMPTY = 0,
        BP_BREAKPOINT_R = 1,
        BP_BREAKPOINT_W = 2,
        BP_BREAKPOINT_RW = BP_BREAKPOINT_R | BP_BREAKPOINT_W,
        BP_BREAKPOINT_X = 4,
        BP_BREAKPOINT_INVALID = BP_BREAKPOINT_RW | BP_BREAKPOINT_X,
    };
    // 断点长度
    enum bp_len
    {
        BP_BREAKPOINT_LEN_1 = 1,
        BP_BREAKPOINT_LEN_2 = 2,
        BP_BREAKPOINT_LEN_3 = 3,
        BP_BREAKPOINT_LEN_4 = 4,
        BP_BREAKPOINT_LEN_5 = 5,
        BP_BREAKPOINT_LEN_6 = 6,
        BP_BREAKPOINT_LEN_7 = 7,
        BP_BREAKPOINT_LEN_8 = 8,

    };
    // 断点作用线程范围
    enum bp_scope
    {
        BP_SCOPE_MAIN_THREAD,   // 仅主线程
        BP_SCOPE_OTHER_THREADS, // 仅其他子线程
        BP_SCOPE_ALL_THREADS    // 全部线程
    };

    // 寄存器索引枚举 (每个索引占用 2 bits)
    enum bp_reg_idx
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

    // 记录单个 PC（触发指令地址）的命中状态
    struct bp_record
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

    // 单个观点地址结构
    struct bp_point
    {
        void (*on_hit)(void *regs, void *self); // 触发回调，命中时调用
        enum bp_type bt;                        // 断点类型
        enum bp_len bl;                         // 断点长度
        enum bp_scope bs;                       // 断点作用线程范围
        uint64_t hit_addr;                      // 监控的地址
        int record_count;                       // 当前已记录的不同 PC 数量
        struct bp_record records[0x100];        // 记录不同 PC 触发状态的数组
    };

    // 存储整体命中信息
    struct break_point
    {

        uint64_t num_brps;                     // 执行断点的数量
        uint64_t num_wrps;                     // 访问断点的数量
        int pid;                               // 这个 break_point 属于哪个进程
        struct bp_point points[BP_CONFIG_MAX]; // 多个观点地址
    };

    struct virtual_gnss
    {
        int latitude_e7;
        int longitude_e7;
    };

    struct virtual_gyro
    {
        int gyro_x_mrad_s;
        int gyro_y_mrad_s;
        int gyro_z_mrad_s;
    };

    struct virtual_input
    {
        int request_virtual_slots;  // 初始化时请求的虚拟 slot 数量
        int POSITION_X, POSITION_Y; // 初始化触摸时返回的触摸面板 ABS 最大值
        int slot;                   // 触摸槽位
        int x, y;                   // 触摸坐标
    };

#define MAX_MODULES      1024
#define MAX_SCAN_REGIONS 16534

#define MOD_NAME_LEN        256
#define MAX_SEGS_PER_MODULE 512

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

    struct virtual_memory
    {
        int module_count;                        // 总模块数量
        struct module_info modules[MAX_MODULES]; // 模块信息

        int region_count;                             // 总可扫描内存数量
        struct region_info regions[MAX_SCAN_REGIONS]; // 可扫描内存区域 (rw-p, 排除特殊区域)
    };

    struct virtual_memoryrw
    {
        uint64_t rw_addr;            // 读写的地址
        uint8_t user_buffer[0x1000]; // 物理标准页大小的数据缓存区
        int size;                    // 读写的大小
    };

    enum request_op
    {
        request_op_none,       // 空调用
        request_op_vmem_read,  // 读取内存
        request_op_vmem_write, // 写入内存
        request_op_vmem_info,  // 获取进程内存信息

        request_op_touch_init, // 初始化触摸
        request_op_touch_down, // 上报按下
        request_op_touch_move, // 上报移动
        request_op_touch_up,   // 上报抬起

        request_op_gyro_init,   // 初始化陀螺仪
        request_op_gyro_report, // 上报陀螺仪数据

        request_op_gnss_init,   // 初始化虚拟定位
        request_op_gnss_report, // 上报虚拟定位数据

        request_op_hwbp_set,    // 设置硬件断点并获取执行/访问断点数量
        request_op_hwbp_remove, // 删除硬件断点

        request_op_ptebp_set,    // 设置 PTE UXN breakpoint
        request_op_ptebp_remove, // 删除 PTE UXN breakpoint

        request_op_stepbp_set,    // 设置单步 PC breakpoint
        request_op_stepbp_remove, // 删除单步 PC breakpoint

        request_op_syscall_monitor_set,    // 监控指定进程的系统调用
        request_op_syscall_monitor_remove, // 取消指定进程的系统调用监控

        request_op_env_get_params, // 获取指定task环境参数

        request_op_kernel_exit // 内核线程退出
    };

    // 将在队列中使用的请求实例结构体
    struct request_obj
    {
        bool kernel; // 由用户模式设置 true = 内核有待处理的请求, false = 请求已完成
        bool user;   // 由内核模式设置 true = 用户模式有待处理的请求, false = 请求已完成

        int pid; // 当前派发指定的pid

        enum request_op op; // 请求操作类型
        int status;         // 请求操作状态

        // 虚拟内存读写信息
        struct virtual_memoryrw vmemrw_info;
        // 虚拟内存信息
        struct virtual_memory vmem_info;
        // 虚拟触摸信息
        struct virtual_input vinput_info;
        // 虚拟陀螺仪信息
        struct virtual_gyro vgyro_info;
        // 虚拟定位信息
        struct virtual_gnss vgnss_info;
        // 断点信息
        struct break_point bp_info;
        // 环境参数信息
        struct env_params env_info;
    };

public: // 外部初始化
    Driver(int Vslot, bool initGyro, bool initGnss)
    {
        InitCommunication();
        InitTouch(Vslot);
        InitGyro(initGyro);
        InitGnss(initGnss);
    }

    ~Driver()
    {
    }

public:
    void NullIo()
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = request_op_none;
        IoCommitAndWait();
    }
    void ExitKernel()
    {
        // 内核停止运行
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = request_op_kernel_exit;
        IoCommitAndWait();
    }

    int GetPid(std::string_view packageName)
    {
        if (packageName.empty()) return -1;

        DIR *dir = opendir("/proc");
        if (!dir) return -1;
        struct dirent *entry;

        char pathBuffer[64];
        char cmdlineBuffer[256];

        while ((entry = readdir(dir)) != nullptr)
        {
            const std::string_view pidText(entry->d_name);
            if (pidText.empty() || !std::ranges::all_of(pidText, [](char ch) { return ch >= '0' && ch <= '9'; })) continue;

            int pid = 0;
            const auto [ptr, ec] = std::from_chars(pidText.data(), pidText.data() + pidText.size(), pid);
            if (ec != std::errc{} || ptr != pidText.data() + pidText.size() || pid <= 0) continue;

            const int pathLength = snprintf(pathBuffer, sizeof(pathBuffer), "/proc/%d/cmdline", pid);
            if (pathLength <= 0 || static_cast<size_t>(pathLength) >= sizeof(pathBuffer)) continue;

            const int fd = open(pathBuffer, O_RDONLY | O_CLOEXEC);
            if (fd < 0) continue;

            const ssize_t bytesRead = read(fd, cmdlineBuffer, sizeof(cmdlineBuffer));
            close(fd);
            if (bytesRead <= 0) continue;

            const size_t processNameLength = strnlen(cmdlineBuffer, static_cast<size_t>(bytesRead));
            const std::string_view processName(cmdlineBuffer, processNameLength);
            if (processName == packageName)
            {
                closedir(dir);
                return pid;
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
    template <typename T> T Read(uint64_t address)
    {
        T value = {};
        HandleVirtualMemoryRWEvent(request_op_vmem_read, address, &value, sizeof(T));
        return value;
    }

    int Read(uint64_t address, void *buffer, size_t size)
    {
        return HandleVirtualMemoryRWEvent(request_op_vmem_read, address, buffer, size);
    }

    std::string ReadString(uint64_t address, size_t max_length = 128)
    {
        if (!address) return "";
        std::vector<char> buffer(max_length + 1, 0);
        if (Read(address, buffer.data(), max_length) > 0)
        {
            buffer[max_length] = '\0';
            return std::string(buffer.data());
        }
        return "";
    }

    template <typename T> int Write(uint64_t address, const T &value)
    {
        return HandleVirtualMemoryRWEvent(request_op_vmem_write, address, const_cast<T *>(&value), sizeof(T));
    }

    int Write(uint64_t address, void *buffer, size_t size)
    {
        return HandleVirtualMemoryRWEvent(request_op_vmem_write, address, buffer, size);
    }

public: // 外部输入接口
    void TouchDown(int slot, int x, int y, int screenW, int screenH)
    {
        HandleTouchEvent(request_op_touch_down, slot, x, y, screenW, screenH);
    }

    void TouchMove(int slot, int x, int y, int screenW, int screenH)
    {
        HandleTouchEvent(request_op_touch_move, slot, x, y, screenW, screenH);
    }

    void TouchUp(int slot)
    {
        HandleTouchEvent(request_op_touch_up, slot, 1, 1, 1, 1);
    }

    void GyroReport(int gyro_x_mrad_s, int gyro_y_mrad_s, int gyro_z_mrad_s)
    {
        HandleGyroReport(gyro_x_mrad_s, gyro_y_mrad_s, gyro_z_mrad_s);
    }

    void GnssReport(int latitude_e7, int longitude_e7)
    {
        HandleGnssReport(latitude_e7, longitude_e7);
    }

public: // 外部获取内存信息
    // 获取内部结构体实例 内部成员调用不需要显示使用this指针，隐式this
    const virtual_memory &GetMemoryInfoRef()
    {
        if (HandleVirtualMemoryInfo() != 0)
        {
            std::printf("获取内存信息失败!!!\n");
            __builtin_memset(&req->vmem_info, 0, sizeof(req->vmem_info));
        }
        return req->vmem_info;
    }

    // 获取模块地址，true为起始地址，false为结束地址
    bool GetModuleAddress(std::string_view moduleName, short segmentIndex, uint64_t *outAddress, bool isStart)
    {
        if (!outAddress)
        {
            std::fprintf(stderr, "outAddress 为空指针\n");
            return false;
        }

        *outAddress = 0;

        if (HandleVirtualMemoryInfo() != 0)
        {
            std::fprintf(stderr, "驱动获取内存信息失败\n");
            return false;
        }

        const auto &info = GetMemoryInfoRef();

        for (int i = 0; i < info.module_count; ++i)
        {
            const auto &mod = info.modules[i];

            std::string_view fullPath(mod.name);

            if (fullPath.length() < moduleName.length()) continue;

            size_t pos = fullPath.length() - moduleName.length();
            if (pos > 0 && fullPath[pos - 1] != '/') continue;
            if (fullPath.substr(pos) != moduleName) continue;

            std::fprintf(stderr, "========== 模块信息 ==========\n");
            std::fprintf(stderr, "  模块索引  : %d\n", i);
            std::fprintf(stderr, "  模块名称  : %s\n", mod.name);
            std::fprintf(stderr, "  区段数量  : %d\n", mod.seg_count);
            std::fprintf(stderr, "  ----------------------------\n");

            for (int j = 0; j < mod.seg_count; ++j)
            {
                const auto &seg = mod.segs[j];
                std::fprintf(stderr, "  区段[%d]:\n", j);
                std::fprintf(stderr, "    index : %d\n", seg.index);
                std::fprintf(stderr, "    start : 0x%016llX\n", (unsigned long long)seg.start);
                std::fprintf(stderr, "    end   : 0x%016llX\n", (unsigned long long)seg.end);
                std::fprintf(stderr, "    size  : 0x%llX (%llu bytes)\n", (unsigned long long)(seg.end - seg.start), (unsigned long long)(seg.end - seg.start));
                std::fprintf(stderr, "    prot  : %d\n", seg.prot);
            }

            std::fprintf(stderr, "==============================\n");

            // 查找目标区段
            for (int j = 0; j < mod.seg_count; ++j)
            {
                const auto &seg = mod.segs[j];
                if (seg.index != segmentIndex) continue;

                *outAddress = isStart ? seg.start : seg.end;
                return true;
            }

            std::fprintf(stderr, " 模块 '%.*s' 中未找到区段索引 %d\n", (int)moduleName.size(), moduleName.data(), segmentIndex);
            return false;
        }

        std::fprintf(stderr, " 未找到模块 '%.*s'\n", (int)moduleName.size(), moduleName.data());
        return false;
    }
    // 驱动获取扫描区域
    std::vector<std::pair<uintptr_t, uintptr_t>> GetScanRegions()
    {
        std::vector<std::pair<uintptr_t, uintptr_t>> regions;

        if (HandleVirtualMemoryInfo() != 0)
        {
            std::fprintf(stderr, "驱动获取内存信息失败\n");
            return regions;
        }

        const auto &info = req->vmem_info;
        const int regionCount = std::clamp(info.region_count, 0, MAX_SCAN_REGIONS);
        const int moduleCount = std::clamp(info.module_count, 0, MAX_MODULES);

        // 预分配空间 (堆内存数量 + 模块数量 * 平均段数)
        regions.reserve(static_cast<size_t>(regionCount) + static_cast<size_t>(moduleCount) * 3);

        //  压入所有匿名的堆内存区域
        for (int i = 0; i < regionCount; ++i)
        {
            const auto &r = info.regions[i];
            if (r.end > r.start) regions.emplace_back(r.start, r.end);
        }

        // 压入所有模块的静态基址区域
        for (int i = 0; i < moduleCount; ++i)
        {
            const auto &mod = info.modules[i];
            const int segmentCount = std::clamp(mod.seg_count, 0, MAX_SEGS_PER_MODULE);
            for (int j = 0; j < segmentCount; ++j)
            {
                const auto &seg = mod.segs[j];
                if (seg.end > seg.start) regions.emplace_back(seg.start, seg.end);
            }
        }

        std::sort(regions.begin(), regions.end(), [](const auto &left, const auto &right) {
            return left.first < right.first || (left.first == right.first && left.second < right.second);
        });

        size_t mergedCount = 0;
        for (const auto &region : regions)
        {
            if (mergedCount == 0 || region.first > regions[mergedCount - 1].second)
            {
                regions[mergedCount++] = region;
                continue;
            }
            regions[mergedCount - 1].second = std::max(regions[mergedCount - 1].second, region.second);
        }
        regions.resize(mergedCount);

        return regions;
    }

    // Dump 模块或指定的半开内存区间 [start, end)。
    bool DumpMemory(std::string_view target, std::string *dumpPath = nullptr)
    {
        const size_t first = target.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos)
        {
            std::fprintf(stderr, "[-] Dump: 模块名或内存范围为空\n");
            return false;
        }
        target = target.substr(first, target.find_last_not_of(" \t\r\n") - first + 1);

        auto parseAddress = [](std::string_view text, uint64_t *value)
        {
            const size_t begin = text.find_first_not_of(" \t\r\n");
            if (begin == std::string_view::npos) return false;
            text = text.substr(begin, text.find_last_not_of(" \t\r\n") - begin + 1);

            std::string token(text);
            char *end = nullptr;
            errno = 0;
            const unsigned long long parsed = std::strtoull(token.c_str(), &end, 16);
            if (errno == ERANGE || end == token.c_str() || *end != '\0') return false;
            *value = static_cast<uint64_t>(parsed);
            return true;
        };

        uint64_t baseAddr = 0;
        uint64_t maxEnd = 0;
        std::string outputName;
        bool rangeDump = false;
        int matchedModuleCount = 0;
        size_t segmentCount = 0;

        const size_t separator = target.find('-');
        if (separator != std::string_view::npos)
        {
            uint64_t rangeStart = 0;
            uint64_t rangeEnd = 0;
            if (parseAddress(target.substr(0, separator), &rangeStart) && parseAddress(target.substr(separator + 1), &rangeEnd))
            {
                baseAddr = rangeStart;
                maxEnd = rangeEnd;
                rangeDump = true;

                char name[64]{};
                std::snprintf(name, sizeof(name), "0x%llX-0x%llX.bin", (unsigned long long)baseAddr, (unsigned long long)maxEnd);
                outputName = name;
            }
        }

        if (!rangeDump)
        {
            const auto &info = GetMemoryInfoRef();
            baseAddr = ~0ULL;

            // 模块名使用包含匹配，命中的所有有效区段按完整地址跨度导出。
            for (int i = 0; i < info.module_count; ++i)
            {
                const auto &mod = info.modules[i];
                if (std::string_view(mod.name).find(target) == std::string_view::npos) continue;

                matchedModuleCount++;
                for (int j = 0; j < mod.seg_count; ++j)
                {
                    const auto &seg = mod.segs[j];
                    if (seg.start >= seg.end) continue;
                    baseAddr = std::min(baseAddr, seg.start);
                    maxEnd = std::max(maxEnd, seg.end);
                    segmentCount++;
                }
            }

            if (segmentCount == 0)
            {
                std::fprintf(stderr, "[-] Dump: 未找到模块 '%.*s' 或模块没有有效区段\n", (int)target.size(), target.data());
                return false;
            }

            const size_t slashPos = target.find_last_of("/\\");
            std::string_view baseName = slashPos == std::string_view::npos ? target : target.substr(slashPos + 1);
            const size_t extensionPos = baseName.find_last_of('.');
            if (extensionPos != std::string_view::npos && extensionPos != 0) baseName = baseName.substr(0, extensionPos);
            if (baseName.empty())
            {
                std::fprintf(stderr, "[-] Dump: 无法从模块名生成输出文件名\n");
                return false;
            }
            outputName = std::string(baseName) + ".bin";
        }

        constexpr uint64_t MAX_DUMP_SIZE = 1024ULL * 1024 * 500; // 500MB 防御 OOM
        const uint64_t spanSize = maxEnd - baseAddr;
        if (baseAddr >= maxEnd || baseAddr == ~0ULL || spanSize == 0 || spanSize > MAX_DUMP_SIZE)
        {
            std::fprintf(stderr, "[-] Dump: 地址范围无效或大小超过 500MB\n");
            return false;
        }

        std::fprintf(stdout, "[*] Dump 目标: %.*s\n", (int)target.size(), target.data());
        if (!rangeDump) std::fprintf(stdout, "[*] 匹配模块: %d 个, 区段: %zu 个\n", matchedModuleCount, segmentCount);
        std::fprintf(stdout, "[*] 基址: 0x%llX\n", (unsigned long long)baseAddr);
        std::fprintf(stdout, "[*] 结束: 0x%llX\n", (unsigned long long)maxEnd);
        std::fprintf(stdout, "[*] 输出大小: 0x%llX (%llu MB)\n", (unsigned long long)spanSize, (unsigned long long)(spanSize / 1024 / 1024));

        if (mkdir("/sdcard/dump", 0777) != 0 && errno != EEXIST)
        {
            std::fprintf(stderr, "[-] Dump: 无法创建 /sdcard/dump (%s)\n", std::strerror(errno));
            return false;
        }

        const std::string outPath = "/sdcard/dump/" + outputName;
        FILE *fp = fopen(outPath.c_str(), "wb");
        if (!fp)
        {
            std::fprintf(stderr, "[-] Dump: 无法创建文件 %s (请检查读写权限)\n", outPath.c_str());
            return false;
        }

        std::vector<uint8_t> page(PAGE_SIZE, 0);
        size_t totalRead = 0;
        size_t failedBlocks = 0;

        for (uint64_t addr = baseAddr; addr < maxEnd; addr += PAGE_SIZE)
        {
            size_t toRead = static_cast<size_t>(std::min<uint64_t>(PAGE_SIZE, maxEnd - addr));
            std::fill(page.begin(), page.begin() + toRead, 0);

            const int readBytes = Read(addr, page.data(), toRead);
            if (readBytes > 0)
            {
                totalRead += std::min<size_t>(static_cast<size_t>(readBytes), toRead);
                if (readBytes < static_cast<int>(toRead)) failedBlocks++;
            }
            else
            {
                failedBlocks++;
            }

            if (fwrite(page.data(), 1, toRead, fp) != toRead)
            {
                std::fprintf(stderr, "[-] Dump: 写入文件失败 %s (%s)\n", outPath.c_str(), std::strerror(errno));
                fclose(fp);
                remove(outPath.c_str());
                return false;
            }
        }

        fclose(fp);
        if (dumpPath) *dumpPath = outPath;
        std::fprintf(stdout, "[*] 读取完成: 成功 0x%zX 字节, 失败或部分读取 %zu 块\n", totalRead, failedBlocks);

        std::fprintf(stdout, "[+] ==========================================\n");
        std::fprintf(stdout, "[+] Dump 完成!\n");
        std::fprintf(stdout, "[+] 路径: %s\n", outPath.c_str());
        std::fprintf(stdout, "[+] 大小: 0x%llX (%llu MB)\n", (unsigned long long)spanSize, (unsigned long long)(spanSize / 1024 / 1024));
        std::fprintf(stdout, "[+] ==========================================\n");

        return true;
    }

public: // 外部硬件断点接口
    // 获取断点结构体信息
    const break_point &GetHwbpInfoRef()
    {
        return req->bp_info;
    }
    // 设置多个断点地址
    int SetProcessHwbpRef(std::span<const bp_point> points)
    {
        return HandleHwbpEvent(request_op_hwbp_set, points);
    }
    // 删除断点
    void RemoveProcessHwbpRef()
    {
        HandleHwbpEvent(request_op_hwbp_remove);
    }
    int SetProcessPtebpRef(std::span<const bp_point> points)
    {
        return HandlePtebpEvent(request_op_ptebp_set, points);
    }
    void RemoveProcessPtebpRef()
    {
        HandlePtebpEvent(request_op_ptebp_remove);
    }
    int SetProcessStepbpRef(std::span<const bp_point> points)
    {
        return HandleStepbpEvent(request_op_stepbp_set, points);
    }
    void RemoveProcessStepbpRef()
    {
        HandleStepbpEvent(request_op_stepbp_remove);
    }

    // 删除指定索引内容
    void RemoveHwbpRecord(int index)
    {
        if (index < 0) return;

        int flat_index = 0;
        for (auto &point : req->bp_info.points)
        {
            if (index >= flat_index && index < flat_index + point.record_count)
            {
                const int local_index = index - flat_index;
                const int tail_count = point.record_count - local_index - 1;
                if (tail_count > 0) __builtin_memmove(&point.records[local_index], &point.records[local_index + 1], static_cast<size_t>(tail_count) * sizeof(bp_record));
                point.record_count--;
                __builtin_memset(&point.records[point.record_count], 0, sizeof(bp_record));
                return;
            }

            flat_index += point.record_count;
        }
    }

public: // 外部系统调用监控接口
    int StartSyscallMonitor(int pid)
    {
        return HandleSyscallMonitorEvent(request_op_syscall_monitor_set, pid);
    }

    int StopSyscallMonitor(int pid)
    {
        return HandleSyscallMonitorEvent(request_op_syscall_monitor_remove, pid);
    }

public:
    // 查询指定线程的 TLS 和目标进程的 PACGA 环境参数
    bool GetEnvParams(std::string_view threadName)
    {
        return HandleEnvGetParams(threadName) == 0;
    }

    // 获取最近一次环境参数查询结果
    const env_params &GetEnvParamsRef() const
    {
        return req->env_info;
    }

private: // 私有实现，外部无需关系
    struct request_obj *req = nullptr;
    int global_pid = 0;

    inline void IoCommitAndWait()
    {
        asm volatile("" ::: "memory");
        req->kernel = true;

        // 等内核完成
        asm volatile("" ::: "memory");
        while (!req->user)
        {
            asm volatile("yield");
        }

        // 消费完成标志
        asm volatile("" ::: "memory");
        req->user = false;
    }

    // 初始化驱动
    void InitCommunication()
    {
        prctl(PR_SET_NAME, "LS", 0, 0, 0);

        req = (request_obj *)mmap((void *)0x2025827000, sizeof(request_obj), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

        if (req == MAP_FAILED)
        {
            printf("[-] 分配共享内存失败，错误码: %d (%s)\n", errno, strerror(errno));
            return;
        }
        __builtin_memset(req, 0, sizeof(request_obj));

        printf("[+] 分配虚拟地址成功，地址: %p  大小: %lu\n", req, sizeof(request_obj));
        printf("当前进程 PID: %d\n", getpid());
        printf("等待驱动握手...\n");

        IoCommitAndWait();

        printf("驱动已经连接\n");
    }

    // 初始化触摸
    // requested_slots: 希望分配给虚拟触摸的 slot 数量
    // 传 0 表示不启用虚拟触摸，内核也不会初始化触摸功能
    void InitTouch(int requested_slots)
    {
        if (requested_slots <= 0) return;
        req->op = request_op_touch_init;
        req->vinput_info.request_virtual_slots = requested_slots;
        IoCommitAndWait();
    }

    // 初始化陀螺仪
    void InitGyro(bool enable)
    {
        if (!enable) return;

        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = request_op_gyro_init;
        IoCommitAndWait();
    }

    // 初始化虚拟定位
    void InitGnss(bool enable)
    {
        if (!enable) return;

        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = request_op_gnss_init;
        IoCommitAndWait();
    }

    // 虚拟内存读写事件
    int HandleVirtualMemoryRWEvent(request_op op, uint64_t addr, void *buffer, size_t size)
    {
        if (!buffer || size == 0) return -EINVAL;
        if (op != request_op_vmem_read && op != request_op_vmem_write) return -EINVAL;

        std::scoped_lock<SpinLock> lock(m_mutex);
        const bool is_read = (op == request_op_vmem_read);
        size_t processed = 0;
        size_t successfulBytes = 0;
        int lastStatus = -EIO;
        const auto copy_virtual_memory_chunk = [](void *destination, const void *source, size_t copy_size)
        {
            switch (copy_size)
            {
            case 1:
                __builtin_memcpy(destination, source, 1);
                break;
            case 2:
                __builtin_memcpy(destination, source, 2);
                break;
            case 4:
                __builtin_memcpy(destination, source, 4);
                break;
            case 8:
                __builtin_memcpy(destination, source, 8);
                break;
            default:
                __builtin_memcpy(destination, source, copy_size);
                break;
            }
        };
        asm volatile("" ::: "memory");
        while (processed < size)
        {
            asm volatile("" ::: "memory");
            const size_t chunk = (size - processed > 0x1000) ? 0x1000 : (size - processed);
            req->op = op;
            req->pid = global_pid;
            req->vmemrw_info.rw_addr = addr + processed;
            req->vmemrw_info.size = chunk;
            req->status = 0;

            asm volatile("" ::: "memory");
            if (is_read)
            {
                if (size > 8) __builtin_memset(req->vmemrw_info.user_buffer, 0, chunk);
                else copy_virtual_memory_chunk(req->vmemrw_info.user_buffer, static_cast<uint8_t *>(buffer) + processed, chunk);
            }
            else
            {
                copy_virtual_memory_chunk(req->vmemrw_info.user_buffer, static_cast<uint8_t *>(buffer) + processed, chunk);
            }
            IoCommitAndWait();

            asm volatile("" ::: "memory");
            lastStatus = req->status;
            if (req->status > 0) successfulBytes += std::min(static_cast<size_t>(req->status), chunk);

            asm volatile("" ::: "memory");
            if (is_read) copy_virtual_memory_chunk(static_cast<uint8_t *>(buffer) + processed, req->vmemrw_info.user_buffer, chunk);

            processed += chunk;
        }
        asm volatile("" ::: "memory");
        if (successfulBytes == 0) return lastStatus;
        return successfulBytes > static_cast<size_t>(INT_MAX) ? -EOVERFLOW : static_cast<int>(successfulBytes);
    }

    // 获取进程虚拟内存信息事件
    int HandleVirtualMemoryInfo()
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = request_op_vmem_info;
        req->pid = global_pid;
        IoCommitAndWait();
        return req->status;
    }

    // 触摸事件
    void HandleTouchEvent(request_op op, int slot, int x, int y, int screenW, int screenH)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);

        // 下面代码绝对不要使用整数除法
        if (screenW <= 0 || screenH <= 0 || req->vinput_info.POSITION_X <= 0 || req->vinput_info.POSITION_Y <= 0) return;

        if (x < 0 || y < 0 || x > screenW || y > screenH) return;

        req->op = op;
        req->vinput_info.slot = slot;
        // 浮点运算提到前面，保持清晰
        double normX = static_cast<double>(x) / screenW;
        double normY = static_cast<double>(y) / screenH;

        // 横竖屏映射逻辑
        if (screenW > screenH && req->vinput_info.POSITION_X < req->vinput_info.POSITION_Y)
        {
            // 右侧充电口模式
            req->vinput_info.x = static_cast<int>((1.0 - normY) * req->vinput_info.POSITION_X);
            req->vinput_info.y = static_cast<int>(normX * req->vinput_info.POSITION_Y);

            // 左侧充电口模式
            // req->vinput_info.x = static_cast<int>((double)y / screenH * req->vinput_info.POSITION_X);
            // req->vinput_info.y = static_cast<int>((1.0 - (double)x / screenW) * req->vinput_info.POSITION_Y);
        }
        else
        {
            // 正常映射
            req->vinput_info.x = static_cast<int>(normX * req->vinput_info.POSITION_X);
            req->vinput_info.y = static_cast<int>(normY * req->vinput_info.POSITION_Y);
        }

        IoCommitAndWait();
    }

    // 陀螺仪事件，单位为 rad/s * 1000
    void HandleGyroReport(int gyro_x_mrad_s, int gyro_y_mrad_s, int gyro_z_mrad_s)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = request_op_gyro_report;
        req->vgyro_info.gyro_x_mrad_s = gyro_x_mrad_s;
        req->vgyro_info.gyro_y_mrad_s = gyro_y_mrad_s;
        req->vgyro_info.gyro_z_mrad_s = gyro_z_mrad_s;
        IoCommitAndWait();
    }

    // 虚拟定位事件，单位为 degrees * 10000000
    void HandleGnssReport(int latitude_e7, int longitude_e7)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = request_op_gnss_report;
        req->vgnss_info.latitude_e7 = latitude_e7;
        req->vgnss_info.longitude_e7 = longitude_e7;
        IoCommitAndWait();
    }

    // 硬件断点事件
    int HandleHwbpEvent(request_op op, std::span<const bp_point> points = {})
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        if (op != request_op_hwbp_set && op != request_op_hwbp_remove) return -1;

        req->op = op;
        req->status = 0;
        if (op == request_op_hwbp_set)
        {
            __builtin_memset(&req->bp_info, 0, sizeof(req->bp_info));
            req->pid = global_pid;
            req->bp_info.pid = global_pid;
            const size_t count = std::min(points.size(), std::size(req->bp_info.points));
            for (size_t i = 0; i < count; ++i)
            {
                req->bp_info.points[i].hit_addr = points[i].hit_addr;
                req->bp_info.points[i].bt = points[i].bt;
                req->bp_info.points[i].bl = points[i].bl;
                req->bp_info.points[i].bs = points[i].bs;
            }
        }
        IoCommitAndWait();
        return req->status;
    }

    // PTEBP 复用 bp_info.points 和 records 存储命中现场
    int HandlePtebpEvent(request_op op, std::span<const bp_point> points = {})
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        if (op != request_op_ptebp_set && op != request_op_ptebp_remove) return -1;

        req->op = op;
        req->status = 0;
        if (op == request_op_ptebp_set)
        {
            __builtin_memset(&req->bp_info, 0, sizeof(req->bp_info));
            req->pid = global_pid;
            req->bp_info.pid = global_pid;
            const size_t count = std::min(points.size(), std::size(req->bp_info.points));
            for (size_t index = 0; index < count; ++index)
            {
                req->bp_info.points[index].hit_addr = points[index].hit_addr;
                req->bp_info.points[index].bt = points[index].bt;
                req->bp_info.points[index].bl = points[index].bl;
                req->bp_info.points[index].bs = points[index].bs;
            }
        }
        IoCommitAndWait();
        return req->status;
    }

    // STEPBP 复用 bp_info.points 和 records 存储命中现场
    int HandleStepbpEvent(request_op op, std::span<const bp_point> points = {})
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        if (op != request_op_stepbp_set && op != request_op_stepbp_remove) return -1;

        req->op = op;
        req->status = 0;
        if (op == request_op_stepbp_set)
        {
            __builtin_memset(&req->bp_info, 0, sizeof(req->bp_info));
            req->pid = global_pid;
            req->bp_info.pid = global_pid;
            const size_t count = std::min(points.size(), std::size(req->bp_info.points));
            for (size_t index = 0; index < count; ++index)
            {
                req->bp_info.points[index].hit_addr = points[index].hit_addr;
                req->bp_info.points[index].bt = points[index].bt;
                req->bp_info.points[index].bl = points[index].bl;
                req->bp_info.points[index].bs = points[index].bs;
            }
        }
        IoCommitAndWait();
        return req->status;
    }

    // 系统调用监控事件；取消时仍使用启动监控时保存的 PID。
    // 此接口只负责启停监控，系统调用日志由驱动写入内核 printk 环形缓冲区，不通过 req 返回。
    // Android shell 查看已有输出：su -c "dmesg | grep -E 'lsdriver'"
    // Android shell 实时查看输出：su -c "dmesg -w | grep -E 'lsdriver'"
    int HandleSyscallMonitorEvent(request_op op, int pid)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        if ((op != request_op_syscall_monitor_set && op != request_op_syscall_monitor_remove) || pid <= 0) return -1;

        req->op = op;
        req->pid = pid;
        req->status = 0;
        IoCommitAndWait();
        return req->status;
    }

    // 获取指定进程的线程 TLS 或 PACGA 环境参数
    int HandleEnvGetParams(std::string_view threadName)
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        if (global_pid <= 0) return -1;

        req->op = request_op_env_get_params;
        req->pid = global_pid;
        req->status = 0;
        __builtin_memset(&req->env_info, 0, sizeof(req->env_info));
        const size_t copyLen = std::min(threadName.size(), static_cast<size_t>(TLS_THREAD_NAME_LEN - 1));
        if (copyLen > 0) __builtin_memcpy(req->env_info.thread_name, threadName.data(), copyLen);
        IoCommitAndWait();
        return req->status;
    }
};

Driver *dr = NULL;
