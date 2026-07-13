#pragma once

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <print>
#include <thread>

#include "driver.h"

inline void GyroLog(const char *fmt, ...)
{
    char buf[256] = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::println(stdout, "[陀螺仪测试] {}", buf);
}

inline void DoGyroSweep(const char *name, int start, int end, int step, int axis)
{
    GyroLog("开始%s", name);

    if (step == 0) return;

    const int dir = (end >= start) ? 1 : -1;
    step = std::abs(step) * dir;

    for (int value = start; dir > 0 ? value <= end : value >= end; value += step)
    {
        int x = 0;
        int y = 0;
        int z = 0;

        if (axis == 0) x = value;
        else if (axis == 1) y = value;
        else z = value;

        dr->GyroReport(x, y, z);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }
}

inline int RunGyroTest()
{
    GyroLog("初始化完成，开始自动上报测试序列");

    for (int i = 0; i < 20; ++i)
    {
        dr->GyroReport(0, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    DoGyroSweep("X轴正向摆动", 0, 1200, 60, 0);
    DoGyroSweep("X轴反向摆动", 1200, -1200, 60, 0);
    DoGyroSweep("Y轴正向摆动", 0, 1200, 60, 1);
    DoGyroSweep("Y轴反向摆动", 1200, -1200, 60, 1);
    DoGyroSweep("Z轴旋转摆动", 0, 1800, 90, 2);
    DoGyroSweep("Z轴反向旋转", 1800, -1800, 90, 2);

    for (int i = 0; i < 20; ++i)
    {
        dr->GyroReport(0, 0, 0);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));
    }

    GyroLog("全部测试序列执行完毕");
    return 0;
}