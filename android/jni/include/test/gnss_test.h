#pragma once

#include <chrono>
#include <thread>

#include "../../driver/driver.h"

inline int RunGnssTest()
{
    constexpr int beijingLatitudeE7 = 399087220;
    constexpr int beijingLongitudeE7 = 1163974990;

    LS_LOGI_TAG("GNSS", "初始化完成，开始固定上报北京坐标 30 秒");
    LS_LOGI_TAG("GNSS", "请打开地图或定位测试 App，触发单次定位或持续定位回调观察经纬度");
    LS_LOGI_TAG("GNSS", "保持坐标: 北京天安门 lat_e7=%d lon_e7=%d duration=30s", beijingLatitudeE7, beijingLongitudeE7);

    for (int i = 0; i < 30; ++i)
    {
        dr->GnssReport(beijingLatitudeE7, beijingLongitudeE7);
        std::this_thread::sleep_for(std::chrono::seconds(1));
    }

    LS_LOGI_TAG("GNSS", "全部测试序列执行完毕");
    return 0;
}