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
    bool largeBlock;
    int operationCount;
    int protocolOperationCount;

    // Null IO
    double nullIoTotalMs;
    double nullIoAvgNs;
    double nullIoThroughputK; // K ops/s

    // Read
    double readTotalMs;
    double readAvgNs;
    double readThroughputK;
    double readBandwidthMB;
    int readFailCount;

    // Write
    double writeTotalMs;
    double writeAvgNs;
    double writeThroughputK;
    double writeBandwidthMB;
    int writeFailCount;

    // Estimated fixed protocol overhead ratio
    double readOverheadPct;
    double writeOverheadPct;
};

inline int RunReadWriteTest()
{
    constexpr size_t ARRAY_CAPACITY = 1000000;
    constexpr int TEST_COUNT = static_cast<int>(ARRAY_CAPACITY);
    constexpr int SMALL_ROUND_COUNT = 6;
    constexpr int ROUND_COUNT = 12;
    constexpr int WRITE_TARGET_VALUE = 1000;
    constexpr size_t ARRAY_BYTES = ARRAY_CAPACITY * sizeof(int);
    constexpr int LARGE_CHUNK_COUNT = static_cast<int>((ARRAY_BYTES + 0xFFF) / 0x1000);

    pid_t selfPid = getpid();
    dr->SetGlobalPid(selfPid);

    LS_LOGI_TAG_FMT("ReadWrite", "驱动读写基准测试: 前 {} 轮小块，后 {} 轮大块", SMALL_ROUND_COUNT, ROUND_COUNT - SMALL_ROUND_COUNT);
    LS_LOGI_TAG_FMT("ReadWrite", "目标 PID={}（自身进程）", selfPid);
    LS_LOGI_TAG_FMT("ReadWrite", "小块: 每次 {} 字节，共 {} 次；大块: 每次 {} 字节，共 1 次（内部 {} 个请求）", sizeof(int), TEST_COUNT, ARRAY_BYTES, LARGE_CHUNK_COUNT);

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

    double nullIoBaselineTotalMs;
    double nullIoBaselineAvgNs;
    double nullIoBaselineThroughputK;
    {
        dr->NullIo();
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < TEST_COUNT; ++i)
        {
            dr->NullIo();
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

        nullIoBaselineTotalMs = ns / 1e6;
        nullIoBaselineAvgNs = static_cast<double>(ns) / TEST_COUNT;
        nullIoBaselineThroughputK = (TEST_COUNT / (ns / 1e9)) / 1000.0;
    }
    LS_LOGI_TAG_FMT("ReadWrite", "空 IO 基准: 总 {:>14.2f}ns，单次 {:>10.2f}ns（{} 次）吞吐 {:>8.2f}K/s", nullIoBaselineTotalMs * 1e6, nullIoBaselineAvgNs, TEST_COUNT, nullIoBaselineThroughputK);

    std::array<RoundResult, ROUND_COUNT> results{};

    for (int round = 0; round < ROUND_COUNT; ++round)
    {
        RoundResult &r = results[round];
        const bool largeBlock = round >= SMALL_ROUND_COUNT;
        const int operationCount = TEST_COUNT;
        const int protocolOperationCount = largeBlock ? LARGE_CHUNK_COUNT : TEST_COUNT;
        r.largeBlock = largeBlock;
        r.operationCount = operationCount;
        r.protocolOperationCount = protocolOperationCount;

        LS_LOGI_TAG_FMT("ReadWrite", "第 {:>2}/{} 轮测试 [{}]", round + 1, ROUND_COUNT, largeBlock ? "大块" : "小块");

        r.nullIoTotalMs = nullIoBaselineTotalMs;
        r.nullIoAvgNs = nullIoBaselineAvgNs;
        r.nullIoThroughputK = nullIoBaselineThroughputK;

        {
            fillRandomValues(randomValues);
            resetTestArray(randomValues);
            std::fill(readByteCounts.begin(), readByteCounts.end(), 0);
            r.readFailCount = 0;
            size_t readTransferred = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            if (largeBlock)
            {
                int readBytes = dr->Read(testAddr, readValues.data(), ARRAY_BYTES);
                readByteCounts[0] = readBytes;
                if (readBytes > 0) readTransferred += static_cast<size_t>(readBytes);
            }
            else for (int i = 0; i < TEST_COUNT; ++i)
            {
                uint64_t currentAddr = testAddr + static_cast<uint64_t>(i * sizeof(int));
                int readBytes = dr->Read(currentAddr, &readValues[static_cast<size_t>(i)], sizeof(int));
                readByteCounts[static_cast<size_t>(i)] = readBytes;
                if (readBytes > 0) readTransferred += static_cast<size_t>(readBytes);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

            const bool readSizeOk = largeBlock ? readByteCounts[0] == static_cast<int>(ARRAY_BYTES) : true;
            if (!largeBlock)
            {
                for (size_t i = 0; i < ARRAY_CAPACITY; ++i)
                {
                    if (readByteCounts[i] != static_cast<int>(sizeof(int)) || readValues[i] != randomValues[i]) r.readFailCount++;
                }
            }
            else
            {
                for (size_t i = 0; i < ARRAY_CAPACITY; ++i)
                {
                    if (readValues[i] != randomValues[i]) r.readFailCount++;
                }
                if (!readSizeOk && r.readFailCount == 0) r.readFailCount = 1;
            }

            double totalS = ns / 1e9;
            r.readTotalMs = ns / 1e6;
            r.readAvgNs = static_cast<double>(ns) / operationCount;
            r.readThroughputK = (operationCount / totalS) / 1000.0;
            r.readBandwidthMB = static_cast<double>(readTransferred) / totalS / (1024.0 * 1024.0);
        }

        {
            resetTestArray(randomValues);
            fillRandomValues(writeValues);
            std::fill(writeByteCounts.begin(), writeByteCounts.end(), 0);
            r.writeFailCount = 0;
            size_t writeTransferred = 0;

            auto t0 = std::chrono::high_resolution_clock::now();
            if (largeBlock)
            {
                int writeBytes = dr->Write(testAddr, writeValues.data(), ARRAY_BYTES);
                writeByteCounts[0] = writeBytes;
                if (writeBytes > 0) writeTransferred += static_cast<size_t>(writeBytes);
            }
            else for (int i = 0; i < TEST_COUNT; ++i)
            {
                uint64_t currentAddr = testAddr + static_cast<uint64_t>(i * sizeof(int));
                int writeBytes = dr->Write(currentAddr, &writeValues[static_cast<size_t>(i)], sizeof(int));
                writeByteCounts[static_cast<size_t>(i)] = writeBytes;
                if (writeBytes > 0) writeTransferred += static_cast<size_t>(writeBytes);
            }
            auto t1 = std::chrono::high_resolution_clock::now();
            auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(t1 - t0).count();

            const bool writeSizeOk = largeBlock ? writeByteCounts[0] == static_cast<int>(ARRAY_BYTES) : true;
            if (!largeBlock)
            {
                for (size_t i = 0; i < ARRAY_CAPACITY; ++i)
                {
                    if (writeByteCounts[i] != static_cast<int>(sizeof(int)) || testArray[i] != writeValues[i]) r.writeFailCount++;
                }
            }
            else
            {
                for (size_t i = 0; i < ARRAY_CAPACITY; ++i)
                {
                    if (testArray[i] != writeValues[i]) r.writeFailCount++;
                }
                if (!writeSizeOk && r.writeFailCount == 0) r.writeFailCount = 1;
            }

            double totalS = ns / 1e9;
            r.writeTotalMs = ns / 1e6;
            r.writeAvgNs = static_cast<double>(ns) / operationCount;
            r.writeThroughputK = (operationCount / totalS) / 1000.0;
            r.writeBandwidthMB = static_cast<double>(writeTransferred) / totalS / (1024.0 * 1024.0);
        }

        const double readIoMs = r.readTotalMs - nullIoBaselineTotalMs;
        const double writeIoMs = r.writeTotalMs - nullIoBaselineTotalMs;
        r.readOverheadPct = (readIoMs / r.readTotalMs) * 100.0;
        r.writeOverheadPct = (writeIoMs / r.writeTotalMs) * 100.0;

        const double readTotalNs = r.readTotalMs * 1e6;
        const double writeTotalNs = r.writeTotalMs * 1e6;
        LS_LOGI_TAG_FMT("ReadWrite", "读取: 总 {:>14.2f}ns，平均 {:>10.3f}ns/int（{} 个 int）吞吐 {:>8.2f}K int/s 带宽 {:>6.2f}MB/s IO耗时 {:>12.2f}ns 占用 {:>7.2f}% 随机校验失败 {}", readTotalNs, r.readAvgNs, operationCount, r.readThroughputK, r.readBandwidthMB, readIoMs * 1e6, r.readOverheadPct, r.readFailCount);
        LS_LOGI_TAG_FMT("ReadWrite", "写入: 总 {:>14.2f}ns，平均 {:>10.3f}ns/int（{} 个 int）吞吐 {:>8.2f}K int/s 带宽 {:>6.2f}MB/s IO耗时 {:>12.2f}ns 占用 {:>7.2f}% 随机校验失败 {}", writeTotalNs, r.writeAvgNs, operationCount, r.writeThroughputK, r.writeBandwidthMB, writeIoMs * 1e6, r.writeOverheadPct, r.writeFailCount);
    }

    int totalReadFail = 0;
    int totalWriteFail = 0;
    LS_LOGI_TAG("ReadWrite", "每轮耗时（ns）: 轮次 | 读取总耗时 | 读取 ns/int | 写入总耗时 | 写入 ns/int");
    for (int i = 0; i < ROUND_COUNT; ++i)
    {
        totalReadFail += results[i].readFailCount;
        totalWriteFail += results[i].writeFailCount;
        LS_LOGI_TAG_FMT("ReadWrite", "{:>5} | {:>12.2f} | {:>11.3f} | {:>12.2f} | {:>11.3f}", i + 1, results[i].readTotalMs * 1e6, results[i].readAvgNs, results[i].writeTotalMs * 1e6, results[i].writeAvgNs);
    }

    LS_LOGI_TAG_FMT("ReadWrite", "全部 {} 轮总失败: 读取 {}，写入 {}，合计 {}", ROUND_COUNT, totalReadFail, totalWriteFail, totalReadFail + totalWriteFail);

    return 0;
}
