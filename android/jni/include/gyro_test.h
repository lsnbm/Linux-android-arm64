#pragma once

#include <chrono>
#include <cstdlib>
#include <thread>

#include "driver.h"

inline void DoGyroSweep(const char *name, int start, int end, int step, int axis)
{
    LS_LOGI_TAG("Gyro", "开始%s", name);

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
    LS_LOGI_TAG("Gyro", "初始化完成，开始自动上报测试序列");

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

    LS_LOGI_TAG("Gyro", "全部测试序列执行完毕");
    return 0;
}