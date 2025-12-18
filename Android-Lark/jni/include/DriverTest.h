#include <iostream>
#include <string>
#include <limits>
#include <iomanip>
#include <chrono>
#include <random>
#include <unistd.h>
#include <stdlib.h>
#include <cstdint> // 为使用 uint64_t 添加头文件
#include "../include/DriverMemory.h"

class DriverTest
{
public:
    // 修复 1: 在构造函数初始化列表中初始化 MDr 成员
    DriverTest() : MDr(false), MPid(0) {}

    ~DriverTest()
    {
        // 析构处理
    }

    void Run()
    {
        bool IsRunning = true;
        while (IsRunning)
        {
            ShowMenu();
            int Choice = Input<int>("请输入选项: ");

            switch (Choice)
            {
            case 200:
                MDr.ExitKernelThread();
                break;
            case 0:

                IsRunning = false;
                std::cout << "程序已退出。\n";
                break;
            case 1:
                HandleRead();
                break;
            case 2:
                HandleWrite();
                break;
            case 3:
                HandleGetModule();
                break;
            case 4:
                HandlePerformanceTest();
                break;
            case 5:
                UpdatePid();
                break;
            default:
                std::cout << "无效选项。\n";
                break;
            }

            if (IsRunning)
            {
                WaitForInput();
                ClearScreen();
            }
        }
    }

private:
    Driver MDr;
    int MPid;

    void HandleRead()
    {
        if (!CheckPid())
            return;

        std::cout << "请输入目标地址 (HEX): ";
        // 修复 2: 将地址类型统一为 uint64_t
        uint64_t Address;
        if (!(std::cin >> std::hex >> Address))
        {
            FixInputStream();
            return;
        }

        int Value = MDr.Read<int>(Address);

        std::cout << "----------------------------------\n";
        std::cout << "地址: 0x" << std::hex << Address << "\n";
        std::cout << "数值(Dec): " << std::dec << Value << "\n";
        std::cout << "数值(Hex): 0x" << std::hex << Value << "\n";
        std::cout << "----------------------------------\n";
    }

    void HandleWrite()
    {
        if (!CheckPid())
            return;

        std::cout << "请输入目标地址 (HEX): ";
        // 修复 2: 将地址类型统一为 uint64_t
        uint64_t Address;
        if (!(std::cin >> std::hex >> Address))
        {
            FixInputStream();
            return;
        }

        std::cout << "请输入写入数值 (Dec): ";
        long Value;
        if (!(std::cin >> std::dec >> Value))
        {
            FixInputStream();
            return;
        }

        MDr.Write(Address, Value);

        int CheckValue = MDr.Read<int>(Address);
        std::cout << "写入完成。复查值: " << std::dec << CheckValue << "\n";
    }

    void HandleGetModule()
    {
        if (!CheckPid())
            return;

        std::cout << "请输入模块名称: ";
        std::string ModuleName;
        std::cin >> ModuleName;

        int SegmentIndex = Input<int>("模块区段 (1:代码, 2:只读, 3:可读写): ");

        // 修复 2: 将地址类型统一为 uint64_t，确保与 GetModuleBase 函数参数类型匹配
        uint64_t BaseAddress = 0;
        MDr.GetModuleBase(ModuleName.c_str(), SegmentIndex, &BaseAddress);

        std::cout << "模块 [" << ModuleName << "] 基址: 0x" << std::hex << BaseAddress << "\n";
    }

    // 性能测
    void HandlePerformanceTest()
    {
        int OldPid = MPid;
        int SelfPid = getpid();

        std::cout << "========== 性能测试 (No Warm-up) ==========\n";
        std::cout << "测试对象: 自身进程 (PID=" << SelfPid << ")\n";

        int *MemoryPointer = new int(0);
        // 修复 2: 将地址类型统一为 uint64_t
        uint64_t TargetAddress = reinterpret_cast<uint64_t>(MemoryPointer);

        // 生成随机验证码
        std::random_device RandomDevice;
        std::mt19937 Generator(RandomDevice());
        int TestValue = std::uniform_int_distribution<int>(100000, 999999)(Generator);
        *MemoryPointer = TestValue;

        // 切换驱动目标
        MDr.SetGlobalPid(SelfPid);

        // 直接开始压测 (无预热检查)
        const int LoopCount = 1000000;
        volatile int FailCount = 0;

        std::cout << "正在执行 " << std::dec << LoopCount << " 次读取...\n";

        auto StartTime = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < LoopCount; ++i)
        {
            if (MDr.Read<int>(TargetAddress) != TestValue)
            {
                FailCount++;
            }
        }

        auto EndTime = std::chrono::high_resolution_clock::now();

        // 统计
        double TotalMilliseconds = std::chrono::duration<double, std::milli>(EndTime - StartTime).count();
        double AverageMicroseconds = (TotalMilliseconds * 1000) / LoopCount;
        double QueriesPerSecond = (LoopCount / TotalMilliseconds) * 1000;

        std::cout << "========== 测试结果 ==========\n";
        std::cout << "总耗时  : " << std::fixed << std::setprecision(2) << TotalMilliseconds << " ms\n";
        std::cout << "平均延迟: " << std::setprecision(4) << AverageMicroseconds << " us\n";
        std::cout << "吞吐量  : " << std::setprecision(0) << QueriesPerSecond << " ops/sec\n";

        if (FailCount == 0)
        {
            std::cout << "完整性  : [通过] 数据全部正确\n";
        }
        else
        {
            std::cout << "完整性  : [失败] 错误次数: " << FailCount << "\n";
            if (FailCount == LoopCount)
            {
                std::cout << "警告    : 所有读取均失败，请检查驱动是否支持读取自身。\n";
            }
        }

        auto StartTimeIo = std::chrono::high_resolution_clock::now();

        for (int i = 0; i < LoopCount; ++i)
        {
            MDr.NullIo();
        }

        auto EndTimeIo = std::chrono::high_resolution_clock::now();

        // 统计 NULLIO
        double TotalMillisecondsIo = std::chrono::duration<double, std::milli>(EndTimeIo - StartTimeIo).count();
        double AverageMicrosecondsIo = (TotalMillisecondsIo * 1000) / LoopCount;
        double QueriesPerSecondIo = (LoopCount / TotalMillisecondsIo) * 1000;

        std::cout << "完成\n";
        std::cout << ">>> NULLIO 结果 (通信链路极限):\n";
        std::cout << "总耗时  : " << std::fixed << std::setprecision(2) << TotalMillisecondsIo << " ms\n";
        std::cout << "单次延迟: " << std::setprecision(4) << AverageMicrosecondsIo << " us\n";
        std::cout << "极限吞吐: " << std::setprecision(0) << QueriesPerSecondIo << " ops/sec\n";

        // 清理
        delete MemoryPointer;
        if (OldPid != 0)
            MDr.SetGlobalPid(OldPid);
        else
            MPid = 0;
    }

    void UpdatePid()
    {
        MPid = Input<int>("请输入目标进程 PID: ");
        MDr.SetGlobalPid(MPid);
        std::cout << "PID 已更新为: " << MPid << "\n";
    }

    void ShowMenu()
    {
        std::cout << "===================================\n";
        std::cout << "Linux Driver Memory Tester\n";
        std::cout << "当前 PID: " << (MPid == 0 ? "未设置" : std::to_string(MPid)) << "\n";
        std::cout << "-----------------------------------\n";
        std::cout << "0. 退出\n1. 读内存\n2. 写内存\n3. 模块基址\n4. 性能测试(100万次)\n5. 换PID\n";
        std::cout << "===================================\n";
    }

    bool CheckPid()
    {
        if (MPid == 0)
        {
            std::cout << "请先设置目标 PID。\n";
            UpdatePid();
        }
        MDr.SetGlobalPid(MPid);
        return true;
    }

    template <typename T>
    T Input(const std::string &prompt)
    {
        T Value;
        while (true)
        {
            std::cout << prompt;
            if (std::cin >> Value)
                break;
            FixInputStream();
        }
        return Value;
    }

    void FixInputStream()
    {
        std::cin.clear();
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cout << "输入无效。\n";
    }

    void WaitForInput()
    {
        std::cout << "\n按回车继续...";
        std::cin.ignore(std::numeric_limits<std::streamsize>::max(), '\n');
        std::cin.get();
    }

    void ClearScreen()
    {
        system("clear");
    }
};