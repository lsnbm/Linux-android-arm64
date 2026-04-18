#pragma once

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdint>
#include <print>
#include <random>
#include <unistd.h>
#include <vector>

#include "DriverMemory.h"

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

inline int mainno()
{
    constexpr size_t ARRAY_CAPACITY = 1000000;
    constexpr int TEST_COUNT = static_cast<int>(ARRAY_CAPACITY);
    constexpr int ROUND_COUNT = 12;
    constexpr int WRITE_TARGET_VALUE = 1000;

    pid_t selfPid = getpid();
    dr.SetGlobalPid(selfPid);

    std::println(stdout, "================================================================");
    std::println(stdout, "  驱动读写性能基准测试（连续 {} 轮，每轮 {} 个 int 元素）", ROUND_COUNT, TEST_COUNT);
    std::println(stdout, "================================================================");
    std::println(stdout, "目标PID: {}（自身进程）", selfPid);
    std::println(stdout, "数组容量: {} int（{} 字节）", ARRAY_CAPACITY, ARRAY_CAPACITY * sizeof(int));
    std::println(stdout, "================================================================\n");

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
            if (value == WRITE_TARGET_VALUE)
                value = -WRITE_TARGET_VALUE;
        }
    };

    auto resetTestArray = [&](const std::vector<int> &values)
    {
        for (size_t i = 0; i < ARRAY_CAPACITY; ++i)
            testArray[i] = values[i];
    };

    std::array<RoundResult, ROUND_COUNT> results{};

    for (int round = 0; round < ROUND_COUNT; ++round)
    {
        RoundResult &r = results[round];

        std::println(stdout, "------------------------------------------------------------");
        std::println(stdout, "  第 {:>2}/{} 轮测试", round + 1, ROUND_COUNT);
        std::println(stdout, "------------------------------------------------------------");

        {
            auto t0 = std::chrono::high_resolution_clock::now();
            for (int i = 0; i < TEST_COUNT; ++i)
            {
                dr.NullIo();
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
                int readBytes = dr.Read(currentAddr, &readValues[static_cast<size_t>(i)], sizeof(int));
                readByteCounts[static_cast<size_t>(i)] = readBytes;
                if (readBytes > 0)
                    readTransferred += static_cast<size_t>(readBytes);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

            for (size_t i = 0; i < ARRAY_CAPACITY; ++i)
            {
                if (readByteCounts[i] != static_cast<int>(sizeof(int)) || readValues[i] != randomValues[i])
                    r.readFailCount++;
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
                int writeBytes = dr.Write(currentAddr, &writeValues[static_cast<size_t>(i)], sizeof(int));
                writeByteCounts[static_cast<size_t>(i)] = writeBytes;
                if (writeBytes > 0)
                    writeTransferred += static_cast<size_t>(writeBytes);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

            for (size_t i = 0; i < ARRAY_CAPACITY; ++i)
            {
                if (writeByteCounts[i] != static_cast<int>(sizeof(int)) || testArray[i] != WRITE_TARGET_VALUE)
                    r.writeFailCount++;
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

        std::println(stdout, "  空IO:  总 {:>10.3f}ms  均 {:>8.2f}ns  吞吐 {:>8.2f}K/s",
                     r.nullIoTotalMs, r.nullIoAvgNs, r.nullIoThroughputK);
        std::println(stdout, "  读取:  总 {:>10.3f}ms  均 {:>8.2f}ns  净 {:>8.2f}ns  吞吐 {:>8.2f}K/s  带宽 {:>6.2f}MB/s  失败索引 {}",
                     r.readTotalMs, r.readAvgNs, r.readNetAvgNs, r.readThroughputK, r.readBandwidthMB, r.readFailCount);
        std::println(stdout, "  写入:  总 {:>10.3f}ms  均 {:>8.2f}ns  净 {:>8.2f}ns  吞吐 {:>8.2f}K/s  带宽 {:>6.2f}MB/s  失败索引 {}",
                     r.writeTotalMs, r.writeAvgNs, r.writeNetAvgNs, r.writeThroughputK, r.writeBandwidthMB, r.writeFailCount);
        std::println(stdout, "");
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
        if (results[i].nullIoAvgNs < results[fastestNullIo].nullIoAvgNs)
            fastestNullIo = i;
        if (results[i].nullIoAvgNs > results[slowestNullIo].nullIoAvgNs)
            slowestNullIo = i;
        if (results[i].readAvgNs < results[fastestRead].readAvgNs)
            fastestRead = i;
        if (results[i].readAvgNs > results[slowestRead].readAvgNs)
            slowestRead = i;
        if (results[i].writeAvgNs < results[fastestWrite].writeAvgNs)
            fastestWrite = i;
        if (results[i].writeAvgNs > results[slowestWrite].writeAvgNs)
            slowestWrite = i;
    }

    std::println(stdout, "================================================================");
    std::println(stdout, "  {} 轮测试综合汇总（每轮 {} 个元素，共 {} 个元素）",
                 ROUND_COUNT, TEST_COUNT, static_cast<long long>(ROUND_COUNT) * TEST_COUNT);
    std::println(stdout, "================================================================");

    std::println(stdout, "\n每轮平均延迟（ns）：");
    std::println(stdout, "  轮次  |   空IO    |   读取     |   写入");
    for (int i = 0; i < ROUND_COUNT; ++i)
    {
        std::println(stdout, "  {:>5} | {:>10.2f} | {:>10.2f} | {:>10.2f}",
                     i + 1,
                     results[i].nullIoAvgNs,
                     results[i].readAvgNs,
                     results[i].writeAvgNs);
    }

    std::println(stdout, "\n平均值：");
    std::println(stdout, "  空IO:  总 {:>10.3f} ms，均 {:>10.2f} ns，吞吐 {:>10.2f} K/s",
                 avg.nullIoTotalMs, avg.nullIoAvgNs, avg.nullIoThroughputK);
    std::println(stdout, "  读取:  总 {:>10.3f} ms，均 {:>10.2f} ns，吞吐 {:>10.2f} K/s",
                 avg.readTotalMs, avg.readAvgNs, avg.readThroughputK);
    std::println(stdout, "  写入:  总 {:>10.3f} ms，均 {:>10.2f} ns，吞吐 {:>10.2f} K/s",
                 avg.writeTotalMs, avg.writeAvgNs, avg.writeThroughputK);

    std::println(stdout, "\n净延迟（去除空IO）：");
    std::println(stdout, "  读取净均耗: {:.2f} ns", avg.readNetAvgNs);
    std::println(stdout, "  写入净均耗: {:.2f} ns", avg.writeNetAvgNs);

    std::println(stdout, "\n数据带宽：");
    std::println(stdout, "  读取平均带宽: {:.2f} MB/s", avg.readBandwidthMB);
    std::println(stdout, "  写入平均带宽: {:.2f} MB/s", avg.writeBandwidthMB);

    std::println(stdout, "\nIO通信开销占比：");
    std::println(stdout, "  读取开销占比: {:.2f}%", avg.readOverheadPct);
    std::println(stdout, "  写入开销占比: {:.2f}%", avg.writeOverheadPct);

    std::println(stdout, "\n稳定性（标准差越小越稳定）：");
    std::println(stdout, "  空IO: {:.2f} ns", nullIoAvgNsStd);
    std::println(stdout, "  读取: {:.2f} ns", readAvgNsStd);
    std::println(stdout, "  写入: {:.2f} ns", writeAvgNsStd);

    std::println(stdout, "\n极值统计：");
    std::println(stdout, "  空IO: 最快第{}轮 ({:.2f} ns)，最慢第{}轮 ({:.2f} ns)，波动 {:.2f} ns",
                 fastestNullIo + 1, results[fastestNullIo].nullIoAvgNs,
                 slowestNullIo + 1, results[slowestNullIo].nullIoAvgNs,
                 results[slowestNullIo].nullIoAvgNs - results[fastestNullIo].nullIoAvgNs);
    std::println(stdout, "  读取: 最快第{}轮 ({:.2f} ns)，最慢第{}轮 ({:.2f} ns)，波动 {:.2f} ns",
                 fastestRead + 1, results[fastestRead].readAvgNs,
                 slowestRead + 1, results[slowestRead].readAvgNs,
                 results[slowestRead].readAvgNs - results[fastestRead].readAvgNs);
    std::println(stdout, "  写入: 最快第{}轮 ({:.2f} ns)，最慢第{}轮 ({:.2f} ns)，波动 {:.2f} ns",
                 fastestWrite + 1, results[fastestWrite].writeAvgNs,
                 slowestWrite + 1, results[slowestWrite].writeAvgNs,
                 results[slowestWrite].writeAvgNs - results[fastestWrite].writeAvgNs);

    std::println(stdout, "\n累计失败统计：");
    std::println(stdout, "  读取失败索引: {} / {} ({:.6f}%)",
                 totalReadFail, static_cast<long long>(ROUND_COUNT) * TEST_COUNT,
                 totalReadFail * 100.0 / (static_cast<double>(ROUND_COUNT) * TEST_COUNT));
    std::println(stdout, "  写入失败索引: {} / {} ({:.6f}%)",
                 totalWriteFail, static_cast<long long>(ROUND_COUNT) * TEST_COUNT,
                 totalWriteFail * 100.0 / (static_cast<double>(ROUND_COUNT) * TEST_COUNT));

    std::println(stdout, "\n================================================================");
    std::println(stdout, "  全部 {} 轮测试完成", ROUND_COUNT);
    std::println(stdout, "================================================================");

    return 0;
}
