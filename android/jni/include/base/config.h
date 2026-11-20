#pragma once
#include <cstddef>
#include <cstdint>
#include <mutex>

namespace Config
{
    inline std::mutex TargetMutex;

    struct Constants
    {
        // 内存浏览缓存固定为当前浏览地址开始的 100 字节。
        static constexpr size_t MEM_VIEW_DEFAULT_BYTES = 100;
        static constexpr size_t SCAN_BUFFER = 4096;
        static constexpr size_t BATCH_SIZE = 16384;
        static constexpr uintptr_t ADDR_MIN = 0x10000;
        static constexpr uintptr_t ADDR_MAX = 0x7FFFFFFFFFFF;
    };

} // namespace Config
