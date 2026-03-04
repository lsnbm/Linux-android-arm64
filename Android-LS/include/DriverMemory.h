
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

//__attribute__((noinline))                                               // 禁止该类所有成员函数成员变量内联
//__attribute__((optimize("-fno-reorder-blocks,-fno-reorder-functions"))) // 禁止编译器重排代码

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
        DelTouch(); // 先清理触摸
        ExitCommunication();
    }

    void NullIo()
    {
        std::scoped_lock<SpinLock> lock(m_mutex);
        req->op = op_o;
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
    void SetGlobalPid(pid_t pid)
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

    bool Read(uint64_t address, void *buffer, size_t size)
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
    bool Write(uint64_t address, const T &value)
    {
        return KWriteProcessMemory(address, const_cast<T *>(&value), sizeof(T));
    }

    bool Write(uint64_t address, void *buffer, size_t size)
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

        // 遍历所有模块，查找目标模块
        for (int i = 0; i < info.module_count; ++i)
        {
            const auto &mod = info.modules[i];

            std::string_view fullPath(mod.name);

            // 长度不够则跳过
            if (fullPath.length() < moduleName.length())
                continue;

            // 尾部匹配 + 前一个字符必须是 '/' 防止误匹配
            size_t pos = fullPath.length() - moduleName.length();
            if (pos > 0 && fullPath[pos - 1] != '/')
                continue;
            if (fullPath.substr(pos) != moduleName)
                continue;

            // 找到目标模块，查找目标区段
            for (int j = 0; j < mod.seg_count; ++j)
            {
                const auto &seg = mod.segs[j];
                if (seg.index != segmentIndex)
                    continue;

                *outAddress = isStart ? seg.start : seg.end;
                return true;
            }

            // 下面这个其实可以不管了!!!,不管了!!!，已经在内核层修复
            // (这里讲解一下所有问题已经在内核层已经修复了!!!)

            /*
             * =========================================================================================
             * 反作弊 VMA 碎裂与诱饵对抗机制
             * =========================================================================================
             *
             * 【第一阶段：理想状态下的纯净内存布局 (原生 ELF 加载)】
             * 当 Linux/Android 原生加载一个 libil2cpp.so 时，它在内存中的排布是非常连续且规律的：
             * - 头部 (RO): 包含 ELF Header 和 Program Header，也就是真实的基址 (Base Address)。
             * - 代码 (RX): .text 段，紧跟在头部之后。
             * - 数据 (RW): .data / .bss 段，跟在代码之后。
             * 事实上，现代 Android 系统（特别是 LLVM/Clang 编译的 64 位）出于安全考虑，
             * 原生加载时至少会 4 到 6 个 VMA 碎片，小的so文件就 1 到 2 个VMA：
             *   1. 头部 (RO): ELF Header 和 .rodata（真实的 Base Address 起点）。
             *   2. 代码 (RX): .text 段，紧跟头部。
             *   3. RELRO (RO): 系统安全机制，写完重定位表后强行锁死为只读，凭空多出一个 RO 碎片。
             *   4. 数据 (RW): .data 全局变量段。
             *   5. BSS  (RW): 尾部额外分配的无文件映射的匿名读写内存。
             * 所以，即使没有反作弊，最纯净的环境也会产生 [RO -> RX -> RO -> RW -> RW] 的轻微碎裂。
             * 此时驱动收集到的段非常完美：Index 0=RX(代码), Index 1=RO(头基址), Index 2=RW(数据)。
             *
             * 【第二阶段：反作弊系统的双重伪装攻击】
             * 现代顶级反作弊（如 ACE）为了防止外部读取和内存 Dump，会做两件极其恶心的事情：
             *
             *   攻击手段 1：VMA 碎裂
             *   反作弊为了 Hook 游戏内部函数，会高频调用 mprotect() 修改代码段的权限。
             *   Linux 内核为了管理不同的物理页权限，被迫将原本 1 个巨大的 RX 代码段，
             *   “劈碎”成了几十甚至上百个细碎的 VMA（虚拟内存区域），并且有些页被改成了 RWX 混合权限。
             *   这导致我们原本排在第 2 位的 RO 段（真实基址），被前面几十个 RX 碎片硬生生挤到了
             *   Index 8 甚至 Index 9 的位置，导致固定索引偏移失效。
             *
             *   攻击手段 2：远端假诱饵
             *   反作弊会在距离真实模块上百 MB 远的极低地址（例如 0x6e32250000），
             *   凭空 mmap() 申请一块虚假内存，并将其命名为 libil2cpp.so，权限设为 RO。
             *   如果我们使用常规的合并算法，会误把这个极远的假地址当成模块的起始地址，
             *   从而导致算出的 Base Address 完全错误（偏离真实基址几十上百MB），读取指针全部失效。
             *
             * 【第三阶段：我们的对抗算法】
             * 为了获取绝对精准的真实基址 (Real Base)，我们采用“物理聚类 + 碎片缝合”的降维打击算法：
             *
             *   步骤 1：物理排序与聚类 (寻找生命主干)
             *   无视所有的权限标签，直接把所有叫 libil2cpp.so 的内存块按物理地址 (start) 升序排列。
             *   由于 ARM64 架构指令寻址的限制，真实的 .so 内存必须紧凑地挨在一起。
             *   我们遍历这些内存块，一旦发现两个块之间的“缝隙”超过 16MB (0x1000000 阈值)，
             *   就意味着碰到了“内存断层”。此时立刻判定：那个孤零零在远处的内存绝对是反作弊的假诱饵！
             *
             *   步骤 2：诱饵物理抹杀
             *   算出体积最大的连续内存群落（即真正的 .so 主体），把不在这个范围内的假诱饵（碎片）
             *   从数组中彻底剔除、物理抹杀。
             *
             *   步骤 3：包围盒缝合
             *   将剩下的纯净真碎片，重新按照 RX -> RO -> RW 排序。
             *   针对被反作弊劈碎的同类型碎片，使用“包围盒算法”：取这些碎片的最小 start 和最大 end，
             *   =>>>这一步不仅缝合了被反作弊劈碎的几十个 RX 碎片，同时也顺手把 Android 系统原生的
             *   “头部 RO”和“RELRO RO”抹平，强行揉成了一个巨大的、完美的虚拟段！
             *
             * 【最终战果】：
             * 无论反作弊怎么切分、怎么放诱饵，跑完此算法后，产出的结果永远绝对固定：
             * - 数组 Index 0：必定是缝合后的 RX 完整代码段。
             * - 数组 Index 1：必定是剔除诱饵后的 RO 完整头数据，它的 start【绝对等于】dladdr 获取的真实基址！
             * - 数组 Index 2：必定是缝合后的 RW 完整数据段。
             *
             * 外部辅助只需无脑调用：Read(段1_Start + Golden_RVA)，即可
             * =========================================================================================
             */
            std::println(stderr, " 模块 '{}' 中未找到区段索引 {}", moduleName, segmentIndex);
            return false;
        }

        // 模块未找到
        std::println(stderr, " 未找到模块 '{}'", moduleName);
        return false;
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

            std::vector<std::pair<uintptr_t, uintptr_t>> merged;
            merged.push_back(regions[0]);

            for (size_t i = 1; i < regions.size(); ++i)
            {
                auto &last = merged.back();
                if (regions[i].first <= last.second)
                {

                    if (regions[i].second > last.second)
                        last.second = regions[i].second;
                }
                else
                {
                    merged.push_back(regions[i]);
                }
            }

            regions = std::move(merged);
        }

        return regions;
    }

    // dump so
    bool DumpModule(std::string_view moduleName)
    {
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

            if (fullPath.length() < moduleName.length())
                continue;

            size_t pos = fullPath.length() - moduleName.length();
            if (pos > 0 && fullPath[pos - 1] != '/')
                continue;

            if (fullPath.substr(pos) == moduleName)
            {
                targetMod = &mod;
                break;
            }
        }

        if (!targetMod)
        {
            std::println(stderr, "[-] Dump: 未找到模块 '{}'", moduleName);
            return false;
        }

        uint64_t minStart = ~0ULL;
        uint64_t maxEnd = 0;

        for (int i = 0; i < targetMod->seg_count; ++i)
        {
            const auto &seg = targetMod->segs[i];

            if (seg.start < minStart)
                minStart = seg.start;
            if (seg.end > maxEnd)
                maxEnd = seg.end;
        }

        if (minStart >= maxEnd || minStart == ~0ULL)
        {
            std::println(stderr, "[-] Dump: 模块边界无效 ({:X} - {:X})", minStart, maxEnd);
            return false;
        }

        std::println(stdout, "[+] 准备 Dump 模块: {}, 基址: {:X}, 结束: {:X}, 物理跨度: {:X}",
                     moduleName, minStart, maxEnd, maxEnd - minStart);

        mkdir("/sdcard/dump", 0777);

        size_t slashPos = moduleName.find_last_of('/');
        std::string_view baseName = (slashPos == std::string_view::npos) ? moduleName : moduleName.substr(slashPos + 1);
        std::string outPath = "/sdcard/dump/" + std::string(baseName);

        FILE *fp = fopen(outPath.c_str(), "wb");
        if (!fp)
        {
            std::println(stderr, "[-] Dump: 无法创建文件 {}", outPath);
            return false;
        }

        size_t pageSize = 0x1000;
        std::vector<uint8_t> buffer(pageSize, 0);
        size_t totalDumped = 0;

        for (int i = 0; i < targetMod->seg_count; ++i)
        {
            const auto &seg = targetMod->segs[i];

            if (seg.start >= seg.end)
                continue;

            if (seg.start >= maxEnd)
                continue;

            off_t fileOffset = static_cast<off_t>(seg.start - minStart);
            fseeko(fp, fileOffset, SEEK_SET);

            uint64_t actualEnd = (seg.end > maxEnd) ? maxEnd : seg.end;

            for (uint64_t addr = seg.start; addr < actualEnd;)
            {

                uint64_t next_page_boundary = (addr + pageSize) & ~(pageSize - 1ULL);
                size_t toRead = next_page_boundary - addr;

                if (toRead > (actualEnd - addr))
                {
                    toRead = actualEnd - addr;
                }

                if (KReadProcessMemory(addr, buffer.data(), toRead) == 0)
                {
                    fwrite(buffer.data(), 1, toRead, fp);
                }
                else
                {

                    memset(buffer.data(), 0, toRead);
                    fwrite(buffer.data(), 1, toRead, fp);
                }

                addr += toRead;
                totalDumped += toRead;
            }
        }

        fclose(fp);
        std::println(stdout, "[+] Dump 完成! 保存路径: {} (共提取 {} 字节有效数据)", outPath, totalDumped);

        return true;
    }

public: // 外部硬件断点接口
    // 断点类型
    enum bp_type
    {
        BP_READ,       // 读
        BP_WRITE,      // 写
        BP_READ_WRITE, // 读写
        BP_EXECUTE     // 执行
    };

    // 断点作用线程范围
    enum bp_scope
    {
        SCOPE_MAIN_THREAD,   // 仅主线程
        SCOPE_OTHER_THREADS, // 仅其他子线程
        SCOPE_ALL_THREADS    // 全部线程
    };

    // 存储命中信息
    struct hwbp_info
    {
        uint64_t num_brps;  // 执行断点的数量
        uint64_t num_wrps;  // 访问断点的数量
        uint64_t hit_addr;  // 监控地址
        uint64_t hit_count; // 命中次数
        uint64_t regs[30];  // X0 ~ X29 寄存器
        uint64_t lr;        // X30 (Link Register)
        uint64_t sp;        // Stack Pointer
        uint64_t pc;        // 触发断点的汇编指令地址
        uint64_t orig_x0;   // 原始 X0 (用于系统调用重启)
        uint64_t syscallno; // 系统调用号
        uint64_t pstate;    // 处理器状态 (CPSR/PSTATE)
    };

    // 间接调用引用
    const hwbp_info &GetHwbpInfoRef()
    {
        GetHwbpInfo();
        return req->bp_info;
    }
    int SetProcessHwbpRef(uint64_t target_addr, bp_type bt, bp_scope bs, int len_bytes)
    {
        return SetProcessHwbp(target_addr, bt, bs, len_bytes);
    }
    void RemoveProcessHwbpRef()
    {
        RemoveProcessHwbp();
    }

private: // 私有实现，外部无需关系
    // 轻量高性能自旋锁
    class SpinLock
    {
        std::atomic_flag locked = ATOMIC_FLAG_INIT;

    public:
        void lock()
        {
            // 尝试加锁，如果失败则一直循环
            while (locked.test_and_set(std::memory_order_acquire))
            {
                // 插入 CPU 级的 pause/yield 指令，防止占满流水线并降低功耗
#if defined(__i386__) || defined(__x86_64__)
                __builtin_ia32_pause();
#elif defined(__aarch64__) || defined(__arm__)
                __asm__ volatile("yield" ::: "memory"); // Android ARM 架构极速暂停
#else
                // 什么都不做，或者 std::this_thread::yield() (较慢)
#endif
            }
        }

        void unlock()
        {
            locked.clear(std::memory_order_release);
        }
    };
    SpinLock m_mutex;

    enum sm_req_op
    {
        op_o = 0, // 空调用
        op_r = 1,
        op_w = 2,
        op_m = 3, // 获取进程内存信息

        op_down = 4,
        op_move = 5,
        op_up = 6,
        op_init_touch = 50, // 初始化触摸
        op_del_touch = 60,  // 清理触摸触摸

        op_brps_weps_info = 7,      // 获取执行断点数量和访问断点数量
        op_set_process_hwbp = 8,    // 设置硬件断点
        op_remove_process_hwbp = 9, // 删除硬件断点

        exit = 100,
        kexit = 200
    };

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
        char user_buffer[0x1000]; // 物理标准页大小

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
    };

    struct req_obj *req;
    pid_t global_pid;

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

    // 初始化驱动连接断开
    void InitCommunication()
    {
        prctl(PR_SET_NAME, "Lark", 0, 0, 0);

        req = (req_obj *)mmap((void *)0x2025827000, sizeof(req_obj), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

        if (req == MAP_FAILED)
        {
            printf("[-] 分配共享内存失败，错误码: %d (%s)\n", errno, strerror(errno));
            return;
        }
        memset(req, 0, sizeof(req_obj));

        printf("[+] 分配虚拟地址成功，地址: %p  大小: %lu\n", req, sizeof(req_obj));
        printf("当前进程 PID: %d\n", getpid());
        printf("等待驱动握手...\n");

        while (req->user.load(std::memory_order_acquire) != 1)
        {
        };
        req->user.store(0, std::memory_order_relaxed);

        printf("驱动已经连接\n");
    }
    void ExitCommunication()
    {
        // 内核停止运行
        req->op = kexit;
        IoCommitAndWait();

        // // 普通断开
        // req->op = exit;
        // IoCommitAndWait();
    }
    // 初始化触摸连接断开
    void InitTouch()
    {
        req->op = op_init_touch;
        IoCommitAndWait();
    }
    void DelTouch()
    {
        req->op = op_del_touch;
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

                if (req->status != 0)
                    return req->status;
                memcpy((char *)buffer + processed, req->user_buffer, chunk);
                processed += chunk;
            }
            return req->status;
        }

        //  小数据快速通道
        req->op = op_r;
        req->pid = global_pid;
        req->target_addr = addr;
        req->size = size;

        IoCommitAndWait();
        // 失败时清空并返回错误码
        if (req->status != 0)
            return req->status;

        switch (size)
        {
        case 4:
            *reinterpret_cast<uint32_t *>(buffer) = *reinterpret_cast<uint32_t *>(req->user_buffer);
            break;
        case 8:
            *reinterpret_cast<uint64_t *>(buffer) = *reinterpret_cast<uint64_t *>(req->user_buffer);
            break;
        case 1:
            *reinterpret_cast<uint8_t *>(buffer) = *reinterpret_cast<uint8_t *>(req->user_buffer);
            break;
        case 2:
            *reinterpret_cast<uint16_t *>(buffer) = *reinterpret_cast<uint16_t *>(req->user_buffer);
            break;
        default:
            memcpy(buffer, req->user_buffer, size);
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
                memcpy(req->user_buffer, (char *)buffer + processed, chunk);
                IoCommitAndWait();

                if (req->status != 0)
                    return req->status;
                processed += chunk;
            }
            return req->status;
        }

        //  小数据快速通道
        req->op = op_w;
        req->pid = global_pid;
        req->target_addr = addr;
        req->size = size;

        switch (size)
        {
        case 4:
            *reinterpret_cast<uint32_t *>(req->user_buffer) = *reinterpret_cast<uint32_t *>(buffer);
            break;
        case 8:
            *reinterpret_cast<uint64_t *>(req->user_buffer) = *reinterpret_cast<uint64_t *>(buffer);
            break;
        case 1:
            *reinterpret_cast<uint8_t *>(req->user_buffer) = *reinterpret_cast<uint8_t *>(buffer);
            break;
        case 2:
            *reinterpret_cast<uint16_t *>(req->user_buffer) = *reinterpret_cast<uint16_t *>(buffer);
            break;
        default:
            memcpy(req->user_buffer, buffer, size);
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
            // 横屏游戏 -> 竖屏驱动 (右侧充电口模式)
            req->x = static_cast<int>((1.0 - normY) * req->POSITION_X);
            req->y = static_cast<int>(normX * req->POSITION_Y);

            // 充电口在左边的情况处理横屏映射
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

#include <string>
#include <vector>
#include <elf.h>

// 极简版内存 ELF 解析器 (支持 64 位 arm64-v8a)
namespace ElfScanner
{

    inline uintptr_t g_baseAddr = 0; // 模块基址
    inline uintptr_t g_strTab = 0;   // 字符串表 (String Table) 的绝对地址
    inline uintptr_t g_symTab = 0;   // 符号表 (Symbol Table) 的绝对地址

    // 初始化解析器
    inline bool Initialize(uintptr_t moduleBase)
    {
        if (g_baseAddr == moduleBase && g_strTab != 0 && g_symTab != 0)
            return true;

        g_baseAddr = moduleBase;
        if (g_baseAddr == 0)
            return false;

        // 读取 ELF 头部 (Ehdr)
        Elf64_Ehdr ehdr;
        if (dr.Read(g_baseAddr, &ehdr, sizeof(Elf64_Ehdr)) != 0)
            return false;

        // 验证 Magic Number (魔数)，确认它是不是一个合法的 ELF 文件 (\x7F E L F)
        if (ehdr.e_ident[0] != 0x7F || ehdr.e_ident[1] != 'E' ||
            ehdr.e_ident[2] != 'L' || ehdr.e_ident[3] != 'F')
            return false;

        // 遍历程序头表 (Program Headers)，寻找动态段 (PT_DYNAMIC)
        uintptr_t dynAddr = 0;
        for (int i = 0; i < ehdr.e_phnum; i++)
        {
            Elf64_Phdr phdr;
            dr.Read(g_baseAddr + ehdr.e_phoff + (i * sizeof(Elf64_Phdr)), &phdr, sizeof(Elf64_Phdr));
            if (phdr.p_type == PT_DYNAMIC)
            {
                dynAddr = g_baseAddr + phdr.p_vaddr;
                break;
            }
        }
        if (dynAddr == 0)
            return false;

        //  解析动态段数组，提取符号表 (DT_SYMTAB) 和 字符串表 (DT_STRTAB)
        Elf64_Dyn dyn;
        int idx = 0;
        do
        {
            dr.Read(dynAddr + (idx * sizeof(Elf64_Dyn)), &dyn, sizeof(Elf64_Dyn));
            if (dyn.d_tag == DT_STRTAB)
                g_strTab = dyn.d_un.d_ptr; // 记录字符串表偏移
            if (dyn.d_tag == DT_SYMTAB)
                g_symTab = dyn.d_un.d_ptr; // 记录符号表偏移
            idx++;
        } while (dyn.d_tag != DT_NULL && idx < 200);

        // 修正 Android Linker 的地址映射
        if (g_strTab > 0 && g_strTab < g_baseAddr)
            g_strTab += g_baseAddr;
        if (g_symTab > 0 && g_symTab < g_baseAddr)
            g_symTab += g_baseAddr;

        return (g_strTab != 0 && g_symTab != 0);
    }

    // 通过符号名字寻找内存中的绝对地址
    inline uintptr_t FindSymbol(const std::string &targetName)
    {

        if (g_strTab == 0 || g_symTab == 0)
            return 0;

        // 遍历符号表  50000 的安全上限
        for (int i = 0; i < 50000; i++)
        {
            Elf64_Sym sym;
            dr.Read(g_symTab + (i * sizeof(Elf64_Sym)), &sym, sizeof(Elf64_Sym));

            if (sym.st_name == 0 && sym.st_value == 0 && sym.st_info == 0)
            {
                if (i > 100)
                    break;
                continue;
            }

            if (sym.st_name == 0 || sym.st_value == 0)
                continue;

            char symName[128] = {0};
            dr.Read(g_strTab + sym.st_name, symName, sizeof(symName) - 1);

            // 内存绝对地址返回
            if (targetName == symName)
            {
                return g_baseAddr + sym.st_value;
            }
        }
        return 0;
    }
}

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

        2. 过滤特征（多次调用，每次传入当前地址）
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
                    if (dr.Read(addr, buffer.data(), readSize) != 0)
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

        if (dr.Read(addr - range, sig.bytes.data(), totalSize) != 0)
        {
            std::println(stderr, "[找特征] 读取失败: 0x{:X}", addr - range);
            return false;
        }

        sig.mask.assign(totalSize, true);

        if (!WriteSigFile(filename, addr, range, sig))
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
        if (!ReadSigFile(filename, range, oldSigText))
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

        if (dr.Read(addr - range, curData.data(), totalSize) != 0)
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

        WriteSigFile(filename, addr, range, newSig);

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
        if (!ReadSigFile(filename, range, sigText))
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

        std::ofstream out(filename, std::ios::app);
        if (out)
        {
            std::println(out, "\n扫描结果: {} 个", matches.size());
            for (auto a : matches)
                std::println(out, "0x{:X}", a);
        }

        return matches;
    }

}
