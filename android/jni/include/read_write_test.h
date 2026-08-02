#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <random>
#include <unistd.h>
#include <vector>

#include "driver.h"

struct RoundResult
{
    // Null IO
    double nullIoTotalMs;
    double nullIoAvgNs;
    double nullIoThroughputK; // K ops/s

    // Read
    double readTotalMs;
    double readAvgNs;
    double readNetAvgNs;
    double readThroughputK;
    double readBandwidthMB;
    int readFailCount;

    // Write
    double writeTotalMs;
    double writeAvgNs;
    double writeNetAvgNs;
    double writeThroughputK;
    double writeBandwidthMB;
    int writeFailCount;

    // IO overhead ratio
    double readOverheadPct;
    double writeOverheadPct;
};

inline int RunReadWriteTest()
{
    constexpr size_t ARRAY_CAPACITY = 1000000;
    constexpr int TEST_COUNT = static_cast<int>(ARRAY_CAPACITY);
    constexpr int ROUND_COUNT = 12;
    constexpr int WRITE_TARGET_VALUE = 1000;

    pid_t selfPid = getpid();
    dr->SetGlobalPid(selfPid);

    LS_LOGI_TAG_FMT("ReadWrite", "驱动读写基准测试: 连续 {} 轮，每轮 {} 个 int 元素", ROUND_COUNT, TEST_COUNT);
    LS_LOGI_TAG_FMT("ReadWrite", "目标 PID={}（自身进程）", selfPid);
    LS_LOGI_TAG_FMT("ReadWrite", "数组容量={} int（{} 字节）", ARRAY_CAPACITY, ARRAY_CAPACITY * sizeof(int));

    std::vector<int> testArray(ARRAY_CAPACITY, 0);
    uint64_t testAddr = reinterpret_cast<uint64_t>(testArray.data());

    std::vector<int> randomValues(ARRAY_CAPACITY, 0);
    std::vector<int> readValues(ARRAY_CAPACITY, 0);
    std::vector<int> writeValues(ARRAY_CAPACITY, 0);
    std::vector<int> readByteCounts(ARRAY_CAPACITY, 0);
    std::vector<int> writeByteCounts(ARRAY_CAPACITY, 0);

    std::mt19937 rng(0xC0FFEEu);
    std::uniform_int_distribution<int> dist(-0x3FFFFFFF, 0x3FFFFFFF);

    auto fillRandomValues = [&](std::vector<int> &values)
    {
        for (auto &value : values)
        {
            value = dist(rng);
            if (value == WRITE_TARGET_VALUE) value = -WRITE_TARGET_VALUE;
        }
    };

    auto resetTestArray = [&](const std::vector<int> &values)
    {
        for (size_t i = 0; i < ARRAY_CAPACITY; ++i) testArray[i] = values[i];
    };

    std::array<RoundResult, ROUND_COUNT> results{};

    for (int round = 0; round < ROUND_COUNT; ++round)
    {
        RoundResult &r = results[round];

        LS_LOGI_TAG_FMT("ReadWrite", "第 {:>2}/{} 轮测试", round + 1, ROUND_COUNT);

        {
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < TEST_COUNT; ++i)
            {
                dr->NullIo();
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

            r.nullIoTotalMs = ns / 1e6;
            r.nullIoAvgNs = static_cast<double>(ns) / TEST_COUNT;
            r.nullIoThroughputK = (TEST_COUNT / (ns / 1e9)) / 1000.0;
        }

        {
            fillRandomValues(randomValues);
            resetTestArray(randomValues);
            std::fill(readValues.begin(), readValues.end(), 0);
            std::fill(readByteCounts.begin(), readByteCounts.end(), 0);
            r.readFailCount = 0;
            size_t readTransferred = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < TEST_COUNT; ++i)
            {
                uint64_t currentAddr = testAddr + static_cast<uint64_t>(i * sizeof(int));
                int readBytes = dr->Read(currentAddr, &readValues[static_cast<size_t>(i)], sizeof(int));
                readByteCounts[static_cast<size_t>(i)] = readBytes;
                if (readBytes > 0) readTransferred += static_cast<size_t>(readBytes);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

            for (size_t i = 0; i < ARRAY_CAPACITY; ++i)
            {
                if (readByteCounts[i] != static_cast<int>(sizeof(int)) || readValues[i] != randomValues[i]) r.readFailCount++;
            }

            double totalS = ns / 1e9;
            r.readTotalMs = ns / 1e6;
            r.readAvgNs = static_cast<double>(ns) / TEST_COUNT;
            r.readNetAvgNs = r.readAvgNs - r.nullIoAvgNs;
            r.readThroughputK = (TEST_COUNT / totalS) / 1000.0;
            r.readBandwidthMB = static_cast<double>(readTransferred) / totalS / (1024.0 * 1024.0);
        }

        {
            resetTestArray(randomValues);
            writeValues = randomValues;
            std::fill(writeValues.begin(), writeValues.end(), WRITE_TARGET_VALUE);
            std::fill(writeByteCounts.begin(), writeByteCounts.end(), 0);
            r.writeFailCount = 0;
            size_t writeTransferred = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < TEST_COUNT; ++i)
            {
                uint64_t currentAddr = testAddr + static_cast<uint64_t>(i * sizeof(int));
                int writeBytes = dr->Write(currentAddr, &writeValues[static_cast<size_t>(i)], sizeof(int));
                writeByteCounts[static_cast<size_t>(i)] = writeBytes;
                if (writeBytes > 0) writeTransferred += static_cast<size_t>(writeBytes);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

            for (size_t i = 0; i < ARRAY_CAPACITY; ++i)
            {
                if (writeByteCounts[i] != static_cast<int>(sizeof(int)) || testArray[i] != WRITE_TARGET_VALUE) r.writeFailCount++;
            }

            double totalS = ns / 1e9;
            r.writeTotalMs = ns / 1e6;
            r.writeAvgNs = static_cast<double>(ns) / TEST_COUNT;
            r.writeNetAvgNs = r.writeAvgNs - r.nullIoAvgNs;
            r.writeThroughputK = (TEST_COUNT / totalS) / 1000.0;
            r.writeBandwidthMB = static_cast<double>(writeTransferred) / totalS / (1024.0 * 1024.0);
        }

        r.readOverheadPct = (r.nullIoAvgNs / r.readAvgNs) * 100.0;
        r.writeOverheadPct = (r.nullIoAvgNs / r.writeAvgNs) * 100.0;

        LS_LOGI_TAG_FMT("ReadWrite", "空 IO: 总 {:>10.3f}ms 均 {:>8.2f}ns 吞吐 {:>8.2f}K/s", r.nullIoTotalMs, r.nullIoAvgNs, r.nullIoThroughputK);
        LS_LOGI_TAG_FMT("ReadWrite", "读取: 总 {:>10.3f}ms 均 {:>8.2f}ns 净 {:>8.2f}ns 吞吐 {:>8.2f}K/s 带宽 {:>6.2f}MB/s 失败索引 {}", r.readTotalMs, r.readAvgNs, r.readNetAvgNs, r.readThroughputK, r.readBandwidthMB, r.readFailCount);
        LS_LOGI_TAG_FMT("ReadWrite", "写入: 总 {:>10.3f}ms 均 {:>8.2f}ns 净 {:>8.2f}ns 吞吐 {:>8.2f}K/s 带宽 {:>6.2f}MB/s 失败索引 {}", r.writeTotalMs, r.writeAvgNs, r.writeNetAvgNs, r.writeThroughputK, r.writeBandwidthMB, r.writeFailCount);
    }

    RoundResult avg{};
    int totalReadFail = 0, totalWriteFail = 0;

    for (int i = 0; i < ROUND_COUNT; ++i)
    {
        const auto &r = results[i];
        avg.nullIoTotalMs += r.nullIoTotalMs;
        avg.nullIoAvgNs += r.nullIoAvgNs;
        avg.nullIoThroughputK += r.nullIoThroughputK;

        avg.readTotalMs += r.readTotalMs;
        avg.readAvgNs += r.readAvgNs;
        avg.readNetAvgNs += r.readNetAvgNs;
        avg.readThroughputK += r.readThroughputK;
        avg.readBandwidthMB += r.readBandwidthMB;
        totalReadFail += r.readFailCount;

        avg.writeTotalMs += r.writeTotalMs;
        avg.writeAvgNs += r.writeAvgNs;
        avg.writeNetAvgNs += r.writeNetAvgNs;
        avg.writeThroughputK += r.writeThroughputK;
        avg.writeBandwidthMB += r.writeBandwidthMB;
        totalWriteFail += r.writeFailCount;

        avg.readOverheadPct += r.readOverheadPct;
        avg.writeOverheadPct += r.writeOverheadPct;
    }

    avg.nullIoTotalMs /= ROUND_COUNT;
    avg.nullIoAvgNs /= ROUND_COUNT;
    avg.nullIoThroughputK /= ROUND_COUNT;

    avg.readTotalMs /= ROUND_COUNT;
    avg.readAvgNs /= ROUND_COUNT;
    avg.readNetAvgNs /= ROUND_COUNT;
    avg.readThroughputK /= ROUND_COUNT;
    avg.readBandwidthMB /= ROUND_COUNT;

    avg.writeTotalMs /= ROUND_COUNT;
    avg.writeAvgNs /= ROUND_COUNT;
    avg.writeNetAvgNs /= ROUND_COUNT;
    avg.writeThroughputK /= ROUND_COUNT;
    avg.writeBandwidthMB /= ROUND_COUNT;

    avg.readOverheadPct /= ROUND_COUNT;
    avg.writeOverheadPct /= ROUND_COUNT;

    double nullIoAvgNsStd = 0, readAvgNsStd = 0, writeAvgNsStd = 0;
    for (int i = 0; i < ROUND_COUNT; ++i)
    {
        nullIoAvgNsStd += (results[i].nullIoAvgNs - avg.nullIoAvgNs) * (results[i].nullIoAvgNs - avg.nullIoAvgNs);
        readAvgNsStd += (results[i].readAvgNs - avg.readAvgNs) * (results[i].readAvgNs - avg.readAvgNs);
        writeAvgNsStd += (results[i].writeAvgNs - avg.writeAvgNs) * (results[i].writeAvgNs - avg.writeAvgNs);
    }
    nullIoAvgNsStd = std::sqrt(nullIoAvgNsStd / ROUND_COUNT);
    readAvgNsStd = std::sqrt(readAvgNsStd / ROUND_COUNT);
    writeAvgNsStd = std::sqrt(writeAvgNsStd / ROUND_COUNT);

    int fastestRead = 0, slowestRead = 0;
    int fastestWrite = 0, slowestWrite = 0;
    int fastestNullIo = 0, slowestNullIo = 0;

    for (int i = 1; i < ROUND_COUNT; ++i)
    {
        if (results[i].nullIoAvgNs < results[fastestNullIo].nullIoAvgNs) fastestNullIo = i;
        if (results[i].nullIoAvgNs > results[slowestNullIo].nullIoAvgNs) slowestNullIo = i;
        if (results[i].readAvgNs < results[fastestRead].readAvgNs) fastestRead = i;
        if (results[i].readAvgNs > results[slowestRead].readAvgNs) slowestRead = i;
        if (results[i].writeAvgNs < results[fastestWrite].writeAvgNs) fastestWrite = i;
        if (results[i].writeAvgNs > results[slowestWrite].writeAvgNs) slowestWrite = i;
    }

    LS_LOGI_TAG_FMT("ReadWrite", "{} 轮测试综合汇总: 每轮 {} 个元素，共 {} 个元素", ROUND_COUNT, TEST_COUNT, static_cast<long long>(ROUND_COUNT) * TEST_COUNT);
    LS_LOGI_TAG("ReadWrite", "每轮平均延迟（ns）: 轮次 | 空 IO | 读取 | 写入");
    for (int i = 0; i < ROUND_COUNT; ++i)
    {
        LS_LOGI_TAG_FMT("ReadWrite", "{:>5} | {:>10.2f} | {:>10.2f} | {:>10.2f}", i + 1, results[i].nullIoAvgNs, results[i].readAvgNs, results[i].writeAvgNs);
    }

    LS_LOGI_TAG_FMT("ReadWrite", "平均值 空 IO: 总 {:>10.3f}ms 均 {:>10.2f}ns 吞吐 {:>10.2f}K/s", avg.nullIoTotalMs, avg.nullIoAvgNs, avg.nullIoThroughputK);
    LS_LOGI_TAG_FMT("ReadWrite", "平均值 读取: 总 {:>10.3f}ms 均 {:>10.2f}ns 吞吐 {:>10.2f}K/s", avg.readTotalMs, avg.readAvgNs, avg.readThroughputK);
    LS_LOGI_TAG_FMT("ReadWrite", "平均值 写入: 总 {:>10.3f}ms 均 {:>10.2f}ns 吞吐 {:>10.2f}K/s", avg.writeTotalMs, avg.writeAvgNs, avg.writeThroughputK);
    LS_LOGI_TAG_FMT("ReadWrite", "净延迟: 读取 {:.2f}ns，写入 {:.2f}ns", avg.readNetAvgNs, avg.writeNetAvgNs);
    LS_LOGI_TAG_FMT("ReadWrite", "平均带宽: 读取 {:.2f}MB/s，写入 {:.2f}MB/s", avg.readBandwidthMB, avg.writeBandwidthMB);
    LS_LOGI_TAG_FMT("ReadWrite", "IO 通信开销占比: 读取 {:.2f}%，写入 {:.2f}%", avg.readOverheadPct, avg.writeOverheadPct);
    LS_LOGI_TAG_FMT("ReadWrite", "稳定性标准差: 空 IO {:.2f}ns，读取 {:.2f}ns，写入 {:.2f}ns", nullIoAvgNsStd, readAvgNsStd, writeAvgNsStd);
    LS_LOGI_TAG_FMT("ReadWrite", "空 IO 极值: 最快第{}轮 ({:.2f}ns)，最慢第{}轮 ({:.2f}ns)，波动 {:.2f}ns", fastestNullIo + 1, results[fastestNullIo].nullIoAvgNs, slowestNullIo + 1, results[slowestNullIo].nullIoAvgNs, results[slowestNullIo].nullIoAvgNs - results[fastestNullIo].nullIoAvgNs);
    LS_LOGI_TAG_FMT("ReadWrite", "读取极值: 最快第{}轮 ({:.2f}ns)，最慢第{}轮 ({:.2f}ns)，波动 {:.2f}ns", fastestRead + 1, results[fastestRead].readAvgNs, slowestRead + 1, results[slowestRead].readAvgNs, results[slowestRead].readAvgNs - results[fastestRead].readAvgNs);
    LS_LOGI_TAG_FMT("ReadWrite", "写入极值: 最快第{}轮 ({:.2f}ns)，最慢第{}轮 ({:.2f}ns)，波动 {:.2f}ns", fastestWrite + 1, results[fastestWrite].writeAvgNs, slowestWrite + 1, results[slowestWrite].writeAvgNs, results[slowestWrite].writeAvgNs - results[fastestWrite].writeAvgNs);
    LS_LOGI_TAG_FMT("ReadWrite", "累计读取失败索引: {} / {} ({:.6f}%)", totalReadFail, static_cast<long long>(ROUND_COUNT) * TEST_COUNT, totalReadFail * 100.0 / (static_cast<double>(ROUND_COUNT) * TEST_COUNT));
    LS_LOGI_TAG_FMT("ReadWrite", "累计写入失败索引: {} / {} ({:.6f}%)", totalWriteFail, static_cast<long long>(ROUND_COUNT) * TEST_COUNT, totalWriteFail * 100.0 / (static_cast<double>(ROUND_COUNT) * TEST_COUNT));
    LS_LOGI_TAG_FMT("ReadWrite", "全部 {} 轮测试完成", ROUND_COUNT);

    return 0;
}
