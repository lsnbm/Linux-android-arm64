#include <iostream>
#include <string>
#include <limits>
#include <iomanip>
#include <chrono>
#include <random>
#include <unistd.h>
#include <stdlib.h>
#include <cstdint>
#include "../include/DriverMemory.h"

class DriverTest
{
public:
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

        // 申请内存
        int *MemoryPointer = new int(0);
        uint64_t TargetAddress = reinterpret_cast<uint64_t>(MemoryPointer);

        // 生成随机验证码用于初始化
        std::random_device RandomDevice;
        std::mt19937 Generator(RandomDevice());
        int InitialRandomValue = std::uniform_int_distribution<int>(100000, 999999)(Generator);
        *MemoryPointer = InitialRandomValue;

        // 切换驱动目标
        MDr.SetGlobalPid(SelfPid);

        const int LoopCount = 1000000; // 100万次

        // --- 1. 读取性能测试 ---
        {
            std::cout << "\n[1/3] 正在执行 " << std::dec << LoopCount << " 次读取...\n";
            volatile int FailCount = 0;
            auto StartTime = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < LoopCount; ++i)
            {
                printf("前计数%d\n", i);
                if (MDr.Read<int>(TargetAddress) != InitialRandomValue)
                {
                    FailCount++;
                }
                printf("后计数%d\n", i);
            }

            auto EndTime = std::chrono::high_resolution_clock::now();
            double TotalMs = std::chrono::duration<double, std::milli>(EndTime - StartTime).count();

            std::cout << ">>> 读取结果:\n";
            std::cout << "总耗时  : " << std::fixed << std::setprecision(2) << TotalMs << " ms\n";
            std::cout << "平均延迟: " << std::setprecision(4) << (TotalMs * 1000) / LoopCount << " us\n";
            std::cout << "吞吐量  : " << std::setprecision(0) << (LoopCount / TotalMs) * 1000 << " ops/sec\n";
            std::cout << "完整性  : " << (FailCount == 0 ? "[通过]" : "[失败]") << " 错误数: " << FailCount << "\n";
        }

        //--- 2. 写入性能测试 ---
        {
            std::cout << "\n[2/3] 正在执行 " << std::dec << LoopCount << " 次写入 (目标值: 2025)...\n";

            // 确保开始前不是 2025
            *MemoryPointer = InitialRandomValue;
            int WriteVal = 2025;

            auto StartTime = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < LoopCount; ++i)
            {
                MDr.Write<int>(TargetAddress, WriteVal);
            }

            auto EndTime = std::chrono::high_resolution_clock::now();
            double TotalMs = std::chrono::duration<double, std::milli>(EndTime - StartTime).count();

            // 验证写入结果
            bool IsSuccess = (*MemoryPointer == 2025);

            std::cout << ">>> 写入结果:\n";
            std::cout << "总耗时  : " << std::fixed << std::setprecision(2) << TotalMs << " ms\n";
            std::cout << "平均延迟: " << std::setprecision(4) << (TotalMs * 1000) / LoopCount << " us\n";
            std::cout << "吞吐量  : " << std::setprecision(0) << (LoopCount / TotalMs) * 1000 << " ops/sec\n";
            std::cout << "最后验证: " << (IsSuccess ? "[成功] 内存值为 2025" : "[失败] 内存值不匹配!") << "\n";
        }

        // --- 3. NULLIO 通信压力测试 ---
        {
            std::cout << "\n[3/3] 正在执行 " << std::dec << LoopCount << " 次空指令 (NULLIO)...\n";

            auto StartTimeIo = std::chrono::high_resolution_clock::now();

            for (int i = 0; i < LoopCount; ++i)
            {
                MDr.NullIo();
            }

            auto EndTimeIo = std::chrono::high_resolution_clock::now();
            double TotalMsIo = std::chrono::duration<double, std::milli>(EndTimeIo - StartTimeIo).count();

            std::cout << ">>> NULLIO 结果 (驱动通讯极限):\n";
            std::cout << "总耗时  : " << std::fixed << std::setprecision(2) << TotalMsIo << " ms\n";
            std::cout << "极限吞吐: " << std::setprecision(0) << (LoopCount / TotalMsIo) * 1000 << " ops/sec\n";
        }

        std::cout << "\n========== 测试结束 ==========\n";

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

};