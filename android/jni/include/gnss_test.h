#pragma once

#include <chrono>
#include <cstdarg>
#include <cstdio>
#include <print>
#include <thread>

#include "driver.h"

inline void GnssLog(const char *fmt, ...)
{
    char buf[256] = {};
    va_list args;
    va_start(args, fmt);
    vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);
    std::println(stdout, "[定位测试] {}", buf);
}

inline void DoGnssHold(const char *name, int latitude_e7, int longitude_e7, int seconds)
{
    GnssLog("保持坐标: %s lat_e7=%d lon_e7=%d duration=%ds", name, latitude_e7, longitude_e7, seconds);

    for (int i = 0; i < seconds; ++i)
    {
        dr->GnssReport(latitude_e7, longitude_e7);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }
}

inline int RunGnssTest()
{
    constexpr const char *beijingName = "北京天安门";
    constexpr int beijingLatitudeE7 = 399087220;
    constexpr int beijingLongitudeE7 = 1163974990;

    GnssLog("初始化完成，开始固定上报北京坐标 30 秒");
    GnssLog("请打开地图或定位测试 App，触发单次定位或持续定位回调观察经纬度");

    DoGnssHold(beijingName, beijingLatitudeE7, beijingLongitudeE7, 30);

    GnssLog("全部测试序列执行完毕");
    return 0;
}