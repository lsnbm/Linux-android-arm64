#pragma once

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <print>
#include <thread>

#include "driver.h"

// 获取屏幕方向对应的逻辑尺寸
inline void GetScreenLogicalSize(int &w, int &h)
{
    // 优先通过属性读取系统屏幕分辨率
    char bufW[64] = {}, bufH[64] = {};
    int wTmp = 1080, hTmp = 2340;

    FILE *pipe = popen("wm size", "r");
    if (pipe)
    {
        char line[128] = {};
        if (fgets(line, sizeof(line), pipe))
        {
            // 格式: Physical size: 1080x2340
            if (sscanf(line, "Physical size: %dx%d", &wTmp, &hTmp) != 2) sscanf(line, "Override size: %dx%d", &wTmp, &hTmp);
        }
        pclose(pipe);
    }

    if (wTmp > 0 && hTmp > 0)
    {
        w = wTmp;
        h = hTmp;
    }
    else
    {
        w = 1080;
        h = 2340;
    }
}

inline void Log(const char *fmt, ...)
{
    char buf[256] = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::println(stdout, "[触摸测试] {}", buf);
}

// 单点连击测试：在 (x,y) 连续点击 taps 次
inline void DoTapTest(int slot, int x, int y, int screenW, int screenH, int taps = 3)
{
    for (int i = 0; i < taps; ++i)
    {
        dr->TouchDown(slot, x, y, screenW, screenH);
        std::this_thread::sleep_for(std::chrono::milliseconds(80));
        dr->TouchUp(slot);
        std::this_thread::sleep_for(std::chrono::milliseconds(120));
    }
}

// 直线滑动测试：从 (x1,y1) 滑动到 (x2,y2)，共 steps 个点
inline void DoSwipeTest(int slot, int x1, int y1, int x2, int y2, int screenW, int screenH, int steps = 30)
{
    dr->TouchDown(slot, x1, y1, screenW, screenH);
    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    for (int i = 1; i <= steps; ++i)
    {
        int x = x1 + (x2 - x1) * i / steps;
        int y = y1 + (y2 - y1) * i / steps;
        dr->TouchMove(slot, x, y, screenW, screenH);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }
    dr->TouchUp(slot);
}

// 多点组合测试：两个手指张开/捏合
inline void DoPinchTest(int screenW, int screenH)
{
    int cx = screenW / 2;
    int cy = screenH / 2;
    int r = std::min(screenW, screenH) / 4;

    dr->TouchDown(0, cx - r, cy, screenW, screenH);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    dr->TouchDown(1, cx + r, cy, screenW, screenH);
    std::this_thread::sleep_for(std::chrono::milliseconds(50));

    // 靠拢（捏合）
    for (int i = 0; i <= 20; ++i)
    {
        int x = cx - r + (r * i / 20);
        dr->TouchMove(0, x, cy, screenW, screenH);
        dr->TouchMove(1, 2 * cx - x, cy, screenW, screenH);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    // 张开
    for (int i = 0; i <= 20; ++i)
    {
        int x = cx - (r * i / 20);
        dr->TouchMove(0, x, cy, screenW, screenH);
        dr->TouchMove(1, 2 * cx - x, cy, screenW, screenH);
        std::this_thread::sleep_for(std::chrono::milliseconds(16));
    }

    dr->TouchUp(0);
    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    dr->TouchUp(1);
}

inline int RunTouchTest()
{
    int screenW = 0, screenH = 0;
    GetScreenLogicalSize(screenW, screenH);
    Log("屏幕逻辑尺寸: {}x{}", screenW, screenH);

    // 1) 单击测试：屏幕左上 1/4 区域
    Log("开始单击连点测试 (10次)");
    DoTapTest(0, screenW / 4, screenH / 4, screenW, screenH, 10);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // 2) 滑动测试：从左到右横滑
    Log("开始横向滑动测试");
    DoSwipeTest(0, screenW / 8, screenH / 2, screenW * 7 / 8, screenH / 2, screenW, screenH, 60);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // 3) 滑动测试：从上到下纵滑
    Log("开始纵向滑动测试");
    DoSwipeTest(0, screenW / 2, screenH / 8, screenW / 2, screenH * 7 / 8, screenW, screenH, 60);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // 4) 双指捏合/张开测试（进行 2 轮）
    Log("开始双指捏合/张开测试");
    DoPinchTest(screenW, screenH);
    std::this_thread::sleep_for(std::chrono::milliseconds(500));
    DoPinchTest(screenW, screenH);
    std::this_thread::sleep_for(std::chrono::milliseconds(800));

    // 5) 快速连击 stress test
    Log("开始快速连击压力测试 (50次)");
    DoTapTest(0, screenW / 2, screenH / 2, screenW, screenH, 50);

    Log("全部测试序列执行完毕");
    return 0;
}
