
#pragma once
#include <stdio.h>
#include <iostream>
#include <vector>
#include <list>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <cstdint>
#include <cstdlib>
#include <set>
#include <cmath>
#include <cstring>
#include <algorithm>
#include <chrono>
#include <map>
#include <sstream>
#include <fstream>
#include <functional>
#include <mutex>
#include <shared_mutex>
#include <span>
#include <ranges>
#include <format>
#include <concepts>
#include <variant>
#include <optional>
#include <charconv>
#include <unordered_set>
#include <stack>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <numeric>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <print>
#include <utility>
#include <numeric>
#include <cinttypes>
#include <atomic>
#include <algorithm>
#include <condition_variable>
#include <functional>
#include <future>
#include <memory>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <array>

#include "DriverMemory.h"
#include "ThreadPool.h"
#include "MappedFile.h"
#include "Disassembler.h"

// ============================================================================
// 配置模块 (Config)
// ============================================================================
namespace Config
{
    inline std::atomic<bool> g_Running{true};
    inline std::atomic<int> g_ItemsPerPage{100};

    struct Constants
    {
        static constexpr size_t MEM_VIEW_RANGE = 50;
        // 内存浏览缓存默认保留当前地址上下各 4096 字节。
        static constexpr size_t MEM_VIEW_DEFAULT_BYTES = 8192;
        static constexpr size_t SCAN_BUFFER = 4096;
        static constexpr size_t BATCH_SIZE = 16384;
        static constexpr size_t MAX_READ_GAP = 64;
        static constexpr double FLOAT_EPSILON = 1e-4;
        static constexpr uintptr_t ADDR_MIN = 0x10000;
        static constexpr uintptr_t ADDR_MAX = 0x7FFFFFFFFFFF;
    };

}

// ============================================================================
// 类型定义
// ============================================================================
namespace Types
{
    enum class DataType : int
    {
        I8 = 0,
        I16,
        I32,
        I64,
        Float,
        Double,
        Count
    };

    enum class FuzzyMode : int
    {
        Unknown = 0,
        Equal,
        Greater,
        Less,
        Increased,
        Decreased,
        Changed,
        Unchanged,
        Range,
        Pointer,
        String,
        Count
    };

    enum class ViewFormat : int
    {
        Hex = 0,
        Hex64,
        I8,
        I16,
        I32,
        I64,
        Float,
        Double,
        Disasm,
        Count
    };

    constexpr size_t GetViewSize(ViewFormat format) noexcept
    {
        switch (format)
        {
        case ViewFormat::I8:
            return sizeof(int8_t);
        case ViewFormat::I16:
            return sizeof(int16_t);
        case ViewFormat::I32:
            return sizeof(int32_t);
        case ViewFormat::I64:
        case ViewFormat::Hex64:
            return sizeof(int64_t);
        case ViewFormat::Float:
            return sizeof(float);
        case ViewFormat::Double:
            return sizeof(double);
        case ViewFormat::Disasm:
        case ViewFormat::Hex:
        default:
            return sizeof(int32_t);
        }
    }

    namespace Labels
    {
        inline constexpr std::array<const char *, static_cast<size_t>(DataType::Count)> TYPE = {
            "I8", "I16", "I32", "I64", "Float", "Double"};

        inline constexpr std::array<const char *, static_cast<size_t>(FuzzyMode::Count)> FUZZY = {
            "Unknown", "Equal", "Greater", "Less", "Increased", "Decreased",
            "Changed", "Unchanged", "Range", "Pointer", "String"};

        inline constexpr std::array<const char *, static_cast<size_t>(ViewFormat::Count)> VIEW_FORMAT = {
            "Hex", "Hex64", "I8", "I16", "I32", "I64", "Float", "Double", "Disasm"};
    }
}

namespace MemUtils
{
    using namespace Types;
    using namespace Config;

    // 去除0xb40000高位标签
    constexpr uintptr_t Normalize(uintptr_t addr) noexcept
    {
        return addr & ~(0xFFULL << 56);
    }

    // 验证地址合法
    constexpr bool IsValidAddr(uintptr_t addr) noexcept
    {
        uintptr_t a = Normalize(addr);
        return a > Constants::ADDR_MIN && a < Constants::ADDR_MAX;
    }

    // 验证浮点数合法性
    template <typename T>
    constexpr bool IsValidFloat(T value) noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return !std::isnan(value) && !std::isinf(value) && std::fpclassify(value) != FP_SUBNORMAL;
        }
        return true;
    }

    // 统一的类型分发
    template <typename F>
    decltype(auto) DispatchType(DataType type, F &&fn)
    {
        switch (type)
        {
        case DataType::I8:
            return fn.template operator()<int8_t>();
        case DataType::I16:
            return fn.template operator()<int16_t>();
        case DataType::I32:
            return fn.template operator()<int32_t>();
        case DataType::I64:
            return fn.template operator()<int64_t>();
        case DataType::Float:
            return fn.template operator()<float>();
        case DataType::Double:
            return fn.template operator()<double>();
        default:
            return fn.template operator()<int32_t>();
        }
    }

    // 值的字符串转换
    namespace detail
    {
        // 把数值按类型格式化为字符串。
        template <typename T>
        std::string ValueToString(T val)
        {
            if constexpr (std::is_floating_point_v<T>)
                return std::format("{:.11f}", val);
            else if constexpr (sizeof(T) <= 4)
                return std::to_string(static_cast<int>(val));
            else
                return std::to_string(static_cast<long long>(val));
        }
        // 把字符串解析为目标类型数值。
        template <typename T>
        T StringToValue(const std::string &s)
        {
            if constexpr (std::is_same_v<T, float>)
                return std::stof(s);
            if constexpr (std::is_same_v<T, double>)
                return std::stod(s);
            if constexpr (sizeof(T) <= 4)
                return static_cast<T>(std::stoi(s));
            return static_cast<T>(std::stoll(s));
        }
    }

    // 按指定类型读取内存并转为字符串。
    inline std::string ReadAsString(uintptr_t addr, DataType type)
    {
        addr = Normalize(addr);
        if (!addr)
            return "??";
        return DispatchType(type, [&]<typename T>() -> std::string
                            { return detail::ValueToString(dr.Read<T>(addr)); });
    }

    // 把字符串按指定类型写入目标地址。
    inline bool WriteFromString(uintptr_t addr, DataType type, std::string_view str)
    {
        addr = Normalize(addr);
        if (!addr || str.empty())
            return false;
        try
        {
            std::string s(str);
            return DispatchType(type, [&]<typename T>() -> bool
                                { return dr.Write<T>(addr, detail::StringToValue<T>(s)); });
        }
        catch (...)
        {
            return false;
        }
    }

    // 读取指针值并格式化为十六进制文本。
    inline std::string ReadAsText(uintptr_t addr, size_t maxLen = 64)
    {
        addr = Normalize(addr);
        if (!addr)
            return "??";

        maxLen = std::clamp<size_t>(maxLen, 1, 256);
        std::string value = dr.ReadString(addr, maxLen);
        for (char &ch : value)
        {
            unsigned char u = static_cast<unsigned char>(ch);
            if (u < 0x20 && ch != '\t')
                ch = '.';
        }
        return value;
    }

    inline bool WriteText(uintptr_t addr, std::string_view str)
    {
        addr = Normalize(addr);
        if (!addr || str.empty())
            return false;

        std::string temp(str);
        return dr.Write(addr, temp.data(), temp.size()) > 0;
    }

    inline std::string ReadAsPointerString(uintptr_t addr)
    {
        addr = Normalize(addr);
        if (!addr)
            return "??";
        return std::format("{:X}", Normalize(static_cast<uintptr_t>(dr.Read<int64_t>(addr))));
    }

    // 把十六进制文本解析后写入指针值。
    inline bool WritePointerFromString(uintptr_t addr, std::string_view str)
    {
        addr = Normalize(addr);
        if (!addr || str.empty())
            return false;
        try
        {
            return dr.Write<int64_t>(addr,
                                     static_cast<int64_t>(std::strtoull(std::string(str).c_str(), nullptr, 16)));
        }
        catch (...)
        {
            return false;
        }
    }

    //  按扫描模式比较当前值与目标值。
    template <typename T>
    bool Compare(T value, T target, FuzzyMode mode, double lastValue, double rangeMax = 0.0)
    {
        // 浮点前置检查
        if constexpr (std::is_floating_point_v<T>)
        {
            if (std::isnan(value) || std::isinf(value))
                return false;
            // 依赖旧值的模式，旧值无效则失败
            constexpr auto kNeedOld = [](FuzzyMode m)
            {
                return m == FuzzyMode::Increased || m == FuzzyMode::Decreased || m == FuzzyMode::Changed || m == FuzzyMode::Unchanged;
            };
            if (kNeedOld(mode) && (std::isnan(lastValue) || std::isinf(lastValue)))
                return false;
        }

        // 获取 epsilon 和 double 转换值
        constexpr bool isFloat = std::is_floating_point_v<T>;
        // 动态 Epsilon: 对于较大的数，使用相对误差；对于较小的数，使用固定误差。
        // 搜索 12.340 但内存中是 12.340000003 时因 epsilon 过小而匹配失败的问题。
        auto get_eps = [&](auto val)
        {
            if constexpr (!isFloat)
                return 0.0;
            double v = std::abs(static_cast<double>(val));
            // 默认 1e-4 对于 12.34 这种量级的数来说，要求精度太高（需匹配到 12.3400x）
            // 调整为: max(Constants::FLOAT_EPSILON, v * 1e-5)
            // 如果用户搜 12.34，v*1e-5 是 0.0001234，这样 12.340000003 就能被搜到了。
            return std::max(Constants::FLOAT_EPSILON, v * 1e-5);
        };

        auto eq = [&](auto a, auto b)
        {
            if constexpr (isFloat)
                return std::abs(static_cast<double>(a) - static_cast<double>(b)) < get_eps(b);
            else
                return a == b;
        };

        T last = static_cast<T>(lastValue);

        switch (mode)
        {
        case FuzzyMode::Equal:
            return eq(value, target);
        case FuzzyMode::Greater:
            return value > target;
        case FuzzyMode::Less:
            return value < target;
        case FuzzyMode::Increased:
            return value > last;
        case FuzzyMode::Decreased:
            return value < last;
        case FuzzyMode::Changed:
            return !eq(value, last);
        case FuzzyMode::Unchanged:
            return eq(value, last);
        case FuzzyMode::Range:
        {
            if constexpr (isFloat)
            {
                double lo = static_cast<double>(target), hi = rangeMax;
                if (lo > hi)
                    std::swap(lo, hi);
                return static_cast<double>(value) >= lo - get_eps(lo) && static_cast<double>(value) <= hi + get_eps(hi);
            }
            else
            {
                T lo = target, hi = static_cast<T>(rangeMax);
                if (lo > hi)
                    std::swap(lo, hi);
                return value >= lo && value <= hi;
            }
        }
        case FuzzyMode::Pointer:
        {
            if constexpr (std::is_integral_v<T>)
            {
                using U = std::make_unsigned_t<T>;
                return Normalize(static_cast<uintptr_t>(static_cast<U>(value))) == Normalize(static_cast<uintptr_t>(static_cast<U>(target)));
            }
            return false;
        }
        default:
            return false;
        }
    }

    // ── HEX 偏移解析 ──
    struct OffsetParseResult
    {
        uintptr_t offset;
        bool negative;
    };

    // 解析形如 ±0xNN 的偏移文本。
    inline std::optional<OffsetParseResult> ParseHexOffset(std::string_view str)
    {
        if (str.empty())
            return std::nullopt;

        // 跳过前导空格
        auto pos = str.find_first_not_of(' ');
        if (pos == std::string_view::npos)
            return std::nullopt;
        str.remove_prefix(pos);

        bool negative = false;
        if (str.front() == '-')
        {
            negative = true;
            str.remove_prefix(1);
        }
        else if (str.front() == '+')
        {
            str.remove_prefix(1);
        }
        if (str.empty())
            return std::nullopt;

        // 跳过 0x/0X
        if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X'))
            str.remove_prefix(2);

        uintptr_t offset = 0;
        std::string buf(str);
        if (std::sscanf(buf.c_str(), "%lx", &offset) != 1)
            return std::nullopt;
        return OffsetParseResult{offset, negative};
    }

} // namespace MemUtils

// ============================================================================
// 位图包装
// ============================================================================
class Bitmap
{
    MappedFile storage_;
    size_t totalBits_ = 0;

public:
    // 按位数初始化位图存储。
    bool init(size_t bits, bool allSet)
    {
        totalBits_ = bits;
        size_t bytes = (bits + 7) / 8;
        if (!storage_.allocate(bytes))
        {
            totalBits_ = 0;
            return false;
        }

        if (allSet)
        {
            std::memset(storage_.as(), 0xFF, bytes);
            size_t tail = bits % 8;
            if (tail)
                storage_.as<uint8_t>()[bytes - 1] = static_cast<uint8_t>((1u << tail) - 1);
        }
        else
        {
            std::memset(storage_.as(), 0, bytes);
        }
        return true;
    }

    // 释放当前对象持有的底层资源。
    void release()
    {
        storage_.release();
        totalBits_ = 0;
    }

    // 返回位图可表示的总位数。
    size_t totalBits() const noexcept { return totalBits_; }
    // 返回位图底层字节数组大小。
    size_t byteCount() const noexcept { return storage_.size(); }
    // 判断位图底层存储是否可用。
    bool valid() const noexcept { return storage_.valid(); }
    uint8_t *data() noexcept { return storage_.as<uint8_t>(); }
    const uint8_t *data() const noexcept { return storage_.as<const uint8_t>(); }

    // 读取指定位当前是否为 1。
    bool get(size_t i) const noexcept
    {
        return (data()[i / 8] >> (i % 8)) & 1;
    }

    // 把指定位设置为 1。
    void setOn(size_t i) noexcept
    {
        __atomic_fetch_or(&data()[i / 8],
                          static_cast<uint8_t>(1u << (i % 8)), __ATOMIC_RELAXED);
    }

    // 把指定位清零为 0。
    void setOff(size_t i) noexcept
    {
        __atomic_fetch_and(&data()[i / 8],
                           static_cast<uint8_t>(~(1u << (i % 8))), __ATOMIC_RELAXED);
    }

    // 快速 popcount
    size_t popcount() const noexcept
    {
        size_t count = 0;
        const uint8_t *p = data();
        size_t bytes = byteCount();

        // 按 8 字节批处理
        size_t chunks = bytes / 8;
        const uint64_t *p64 = reinterpret_cast<const uint64_t *>(p);
        for (size_t i = 0; i < chunks; ++i)
            count += __builtin_popcountll(p64[i]);

        // 处理尾部
        for (size_t i = chunks * 8; i < bytes; ++i)
            count += __builtin_popcount(p[i]);

        return count;
    }
};

// ============================================================================
// 内存扫描器
// ============================================================================
class MemScanner
{
public:
    using Results = std::vector<uintptr_t>;

private:
    // ── 区域描述 ──
    struct Region
    {
        uintptr_t start, end;
        size_t bitOffset, bitCount;
    };

    // ── 核心状态 ──
    Bitmap bitmap_;
    MappedFile values_;
    std::vector<Region> regions_;
    std::vector<uintptr_t> addedList_;

    size_t setBits_ = 0;
    size_t valueSize_ = 0;

    mutable std::shared_mutex mutex_;
    std::atomic<float> progress_{0.0f};
    std::atomic<bool> scanning_{false};
    double rangeMax_ = 0.0;

    //  位 ↔ 地址映射
    size_t addrToBit(uintptr_t addr) const noexcept
    {
        // 二分查找所属区域
        auto it = std::upper_bound(regions_.begin(), regions_.end(), addr, [](uintptr_t a, const Region &r)
                                   { return a < r.end; });

        // upper_bound 找到第一个 end > addr 的区域
        if (it == regions_.end() || addr < it->start)
            return SIZE_MAX;

        size_t off = addr - it->start;
        if (off % valueSize_ != 0)
            return SIZE_MAX;

        size_t index = off / valueSize_;
        if (index >= it->bitCount)
            return SIZE_MAX;

        return it->bitOffset + index;
    }

    // 把位图索引换算为实际内存地址。
    uintptr_t bitToAddr(size_t gb) const noexcept
    {
        auto it = std::upper_bound(regions_.begin(), regions_.end(), gb,
                                   [](size_t b, const Region &r)
                                   { return b < r.bitOffset + r.bitCount; });
        if (it == regions_.end())
            return 0;
        return it->start + (gb - it->bitOffset) * valueSize_;
    }

    // 位图初始化
    bool initStorage(size_t valSz, const std::vector<std::pair<uintptr_t, uintptr_t>> &scanRegs, bool allSet)
    {
        bitmap_.release();
        values_.release();
        regions_.clear();
        valueSize_ = valSz;

        size_t totalBits = 0;
        regions_.reserve(scanRegs.size());
        for (auto &[s, e] : scanRegs)
        {
            if (e - s < valSz)
                continue;
            size_t bits = (e - s) / valSz;
            regions_.push_back({s, e, totalBits, bits});
            totalBits += bits;
        }
        if (!totalBits)
            return false;

        if (!bitmap_.init(totalBits, allSet))
            return false;

        size_t valBytes = totalBits * sizeof(double);
        if (!values_.allocate(valBytes))
        {
            bitmap_.release();
            return false;
        }
        values_.advise(MADV_SEQUENTIAL);

        setBits_ = allSet ? totalBits : 0;
        return true;
    }

    double *valuesMap() noexcept { return values_.as<double>(); }
    const double *valuesMap() const noexcept { return values_.as<const double>(); }

    // 将模板数值统一转换为 double。
    template <typename T>
    static double toDouble(T value, Types::FuzzyMode mode) noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            double d = static_cast<double>(value);
            return (std::isnan(d) || std::isinf(d)) ? 0.0 : d;
        }
        else if constexpr (std::is_integral_v<T>)
        {
            if (mode == Types::FuzzyMode::Pointer)
                return static_cast<double>(MemUtils::Normalize(
                    static_cast<uintptr_t>(static_cast<std::make_unsigned_t<T>>(value))));
            return static_cast<double>(value);
        }
        return static_cast<double>(value);
    }

    // 并行线程分配
    unsigned threadCount() const
    {
        return std::max(1u, static_cast<unsigned>(
                                std::min(static_cast<size_t>(Utils::GetThreadCount()), regions_.size())));
    }

    //  统一的区域遍历核心
    template <typename ProcessFn>
    // 并发遍历内存区域执行扫描逻辑。
    void parallelRegionScan(ProcessFn &&process)
    {
        unsigned tc = threadCount();
        size_t chunk = (regions_.size() + tc - 1) / tc;
        std::atomic<size_t> done{0};

        std::vector<std::future<void>> futs;
        futs.reserve(tc);

        for (unsigned t = 0; t < tc; ++t)
        {
            futs.push_back(Utils::GlobalPool.push([&, t, chunk]
                                                  {
                size_t end = std::min(t * chunk + chunk, regions_.size());
                std::vector<uint8_t> buf(Config::Constants::SCAN_BUFFER);

                for (size_t ri = t * chunk; ri < end && Config::g_Running; ++ri) {
                    auto& reg = regions_[ri];
                    for (uintptr_t addr = reg.start; addr < reg.end;
                         addr += Config::Constants::SCAN_BUFFER)
                    {
                        size_t sz = std::min(static_cast<size_t>(reg.end - addr),
                                             Config::Constants::SCAN_BUFFER);
                        int readBytes = dr.Read(addr, buf.data(), sz);
                        process(reg, buf.data(), addr,
                                readBytes > 0 ? static_cast<size_t>(readBytes) : 0, sz);
                    }
                    if ((done.fetch_add(1) & 0x3F) == 0)
                        progress_ = static_cast<float>(done) / regions_.size();
                } }));
        }
        for (auto &f : futs)
            f.get();
    }

    // 清除不可读范围对应的位标记。
    template <typename T>

    void clearUnreadableBits(const Region &reg, uintptr_t addr, size_t from, size_t to)
    {
        for (size_t off = from; off + sizeof(T) <= to; off += sizeof(T))
        {
            size_t gb = reg.bitOffset + (addr + off - reg.start) / sizeof(T);
            if (gb < bitmap_.totalBits() && bitmap_.get(gb))
                bitmap_.setOff(gb);
        }
    }

    // ================================================================
    //  首扫 Unknown — bitmap 全 1 + 记录旧值
    // ================================================================
    template <typename T>
    void scanFirstUnknown(pid_t /*pid*/)
    {
        auto scanRegs = dr.GetScanRegions();
        if (scanRegs.empty())
            return;

        {
            std::unique_lock lock(mutex_);
            if (!initStorage(sizeof(T), scanRegs, true))
                return;
        }

        parallelRegionScan([this](const Region &reg, uint8_t *buf,
                                  uintptr_t addr, size_t readBytes, size_t sz)
                           {
            if (readBytes == 0) {
                clearUnreadableBits<T>(reg, addr, 0, sz);
                return;
            }

            // 有效数据部分：记录值，过滤无效浮点
            for (size_t off = 0; off + sizeof(T) <= readBytes; off += sizeof(T)) {
                T value;
                std::memcpy(&value, buf + off, sizeof(T));
                size_t gb = reg.bitOffset + (addr + off - reg.start) / sizeof(T);

                if constexpr (std::is_floating_point_v<T>) {
                    if (!MemUtils::IsValidFloat(value)) {
                        if (gb < bitmap_.totalBits() && bitmap_.get(gb))
                            bitmap_.setOff(gb);
                        continue;
                    }
                }
                valuesMap()[gb] = static_cast<double>(value);
            }

            // 不完整尾部：清除位
            size_t alignedEnd = readBytes & ~(sizeof(T) - 1);
            clearUnreadableBits<T>(reg, addr, alignedEnd, sz); });

        std::unique_lock lock(mutex_);
        setBits_ = bitmap_.popcount();
    }

    // ================================================================
    //  首扫有目标值
    // ================================================================
    template <typename T>
    void scanFirst(pid_t /*pid*/, T target, Types::FuzzyMode mode)
    {
        auto scanRegs = dr.GetScanRegions();
        if (scanRegs.empty())
            return;

        {
            std::unique_lock lock(mutex_);
            if (!initStorage(sizeof(T), scanRegs, false))
                return;
        }

        double rmx = rangeMax_;

        // 每线程收集结果
        unsigned tc = threadCount();
        size_t chunk = (regions_.size() + tc - 1) / tc;
        std::atomic<size_t> done{0};

        struct HitEntry
        {
            uintptr_t addr;
            double val;
        };
        std::vector<std::deque<HitEntry>> threadHits(tc);

        std::vector<std::future<void>> futs;
        futs.reserve(tc);

        for (unsigned t = 0; t < tc; ++t)
        {
            futs.push_back(Utils::GlobalPool.push([&, t, rmx, chunk]
                                                  {
                // 使用 scanRegs 而不是 regions_ 进行遍历
                auto& myHits = threadHits[t];
                std::vector<uint8_t> buf(Config::Constants::SCAN_BUFFER);
                size_t end = std::min(t * chunk + chunk, regions_.size());

                for (size_t ri = t * chunk; ri < end && Config::g_Running; ++ri) {
                    auto& reg = regions_[ri];
                    for (uintptr_t addr = reg.start; addr < reg.end;
                         addr += Config::Constants::SCAN_BUFFER)
                    {
                        size_t sz = std::min(static_cast<size_t>(reg.end - addr),
                                             Config::Constants::SCAN_BUFFER);
                        int readBytes = dr.Read(addr, buf.data(), sz);
                        if (readBytes <= 0) continue;

                        size_t usable = static_cast<size_t>(readBytes);
                        for (size_t off = 0; off + sizeof(T) <= usable; off += sizeof(T)) {
                            T value;
                            std::memcpy(&value, buf.data() + off, sizeof(T));

                            if constexpr (std::is_floating_point_v<T>) {
                                if (!MemUtils::IsValidFloat(value)) continue;
                            }

                            if (MemUtils::Compare(value, target, mode, 0.0, rmx)) {
                                myHits.push_back({addr + off, toDouble(value, mode)});
                            }
                        }
                    }
                    if ((done.fetch_add(1) & 0x7F) == 0)
                        progress_ = static_cast<float>(done) / regions_.size();
                } }));
        }
        for (auto &f : futs)
            f.get();

        // 合并结果到位图
        std::unique_lock lock(mutex_);
        size_t actualSet = 0;
        for (auto &hits : threadHits)
        {
            for (auto &[addr, val] : hits)
            {
                size_t gb = addrToBit(addr);
                if (gb != SIZE_MAX)
                {
                    bitmap_.setOn(gb);
                    valuesMap()[gb] = val;
                    ++actualSet;
                }
            }
        }
        setBits_ = actualSet;
    }

    // ================================================================
    //  二次扫描
    // ================================================================
    template <typename T>
    void scanNext(T target, Types::FuzzyMode mode)
    {
        double rmx = rangeMax_;
        std::atomic<size_t> survived{0};

        parallelRegionScan([&, rmx](const Region &reg, uint8_t *buf,
                                    uintptr_t addr, size_t readBytes, size_t sz)
                           {
            if (readBytes == 0) {
                clearUnreadableBits<T>(reg, addr, 0, sz);
                return;
            }

            // 有效数据部分
            for (size_t off = 0; off + sizeof(T) <= readBytes; off += sizeof(T)) {
                size_t gb = reg.bitOffset + (addr + off - reg.start) / sizeof(T);
                if (!bitmap_.get(gb)) continue;

                T value;
                std::memcpy(&value, buf + off, sizeof(T));

                // 浮点值/旧值有效性检查
                if constexpr (std::is_floating_point_v<T>) {
                    if (!MemUtils::IsValidFloat(value)) {
                        bitmap_.setOff(gb);
                        continue;
                    }
                    double oldVal = valuesMap()[gb];
                    if (std::isnan(oldVal) || std::isinf(oldVal)) {
                        bitmap_.setOff(gb);
                        continue;
                    }
                }

                double oldVal = valuesMap()[gb];
                if (MemUtils::Compare(value, target, mode, oldVal, rmx)) {
                    valuesMap()[gb] = toDouble(value, mode);
                    survived.fetch_add(1, std::memory_order_relaxed);
                } else {
                    bitmap_.setOff(gb);
                }
            }

            // 不完整尾部
            size_t alignedEnd = readBytes & ~(sizeof(T) - 1);
            clearUnreadableBits<T>(reg, addr, alignedEnd, sz); });

        std::unique_lock lock(mutex_);
        setBits_ = survived.load();
    }

    void scanFirstString(const std::string &needle)
    {
        if (needle.empty())
            return;

        auto scanRegs = dr.GetScanRegions();
        if (scanRegs.empty())
            return;

        {
            std::unique_lock lock(mutex_);
            bitmap_.release();
            values_.release();
            regions_.clear();
            setBits_ = 0;
            valueSize_ = 0;
            addedList_.clear();
        }

        const size_t patLen = needle.size();
        if (patLen > Config::Constants::SCAN_BUFFER)
            return;

        unsigned tc = std::max(1u, static_cast<unsigned>(
                                       std::min(static_cast<size_t>(Utils::GetThreadCount()), scanRegs.size())));
        size_t chunk = (scanRegs.size() + tc - 1) / tc;
        std::atomic<size_t> done{0};

        std::vector<std::deque<uintptr_t>> threadHits(tc);
        std::vector<std::future<void>> futs;
        futs.reserve(tc);

        const size_t step = (Config::Constants::SCAN_BUFFER > patLen)
                                ? (Config::Constants::SCAN_BUFFER - patLen + 1)
                                : 1;

        for (unsigned t = 0; t < tc; ++t)
        {
            futs.push_back(Utils::GlobalPool.push([&, t]
                                                  {
                auto &myHits = threadHits[t];
                std::vector<uint8_t> buf(Config::Constants::SCAN_BUFFER);
                size_t end = std::min(t * chunk + chunk, scanRegs.size());

                for (size_t ri = t * chunk; ri < end && Config::g_Running; ++ri) {
                    auto [start, finish] = scanRegs[ri];
                    if (finish <= start || static_cast<size_t>(finish - start) < patLen)
                    {
                        if ((done.fetch_add(1) & 0x3F) == 0)
                            progress_ = static_cast<float>(done) / scanRegs.size();
                        continue;
                    }

                    for (uintptr_t addr = start; addr + patLen <= finish;) {
                        size_t readSize = std::min(static_cast<size_t>(finish - addr), Config::Constants::SCAN_BUFFER);
                        int readBytes = dr.Read(addr, buf.data(), readSize);
                        if (readBytes > 0) {
                            size_t usable = static_cast<size_t>(readBytes);
                            if (usable >= patLen) {
                                size_t uniqueLimit = (addr + step < finish) ? std::min(step, usable) : usable;
                                for (size_t off = 0; off + patLen <= usable && off < uniqueLimit; ++off) {
                                    if (std::memcmp(buf.data() + off, needle.data(), patLen) == 0)
                                        myHits.push_back(addr + off);
                                }
                            }
                        }

                        if (addr + step <= addr || addr + step >= finish)
                            break;
                        addr += step;
                    }

                    if ((done.fetch_add(1) & 0x3F) == 0)
                        progress_ = static_cast<float>(done) / scanRegs.size();
                } }));
        }

        for (auto &f : futs)
            f.get();

        std::vector<uintptr_t> merged;
        for (auto &hits : threadHits)
        {
            merged.insert(merged.end(), hits.begin(), hits.end());
        }
        std::sort(merged.begin(), merged.end());
        merged.erase(std::unique(merged.begin(), merged.end()), merged.end());

        std::unique_lock lock(mutex_);
        addedList_.swap(merged);
        setBits_ = 0;
    }

    void scanNextString(const std::string &needle)
    {
        if (needle.empty())
            return;

        std::vector<uintptr_t> current;
        {
            std::shared_lock lock(mutex_);
            current = addedList_;
        }
        if (current.empty())
            return;

        const size_t patLen = needle.size();
        unsigned tc = std::max(1u, static_cast<unsigned>(
                                       std::min(static_cast<size_t>(Utils::GetThreadCount()), current.size())));
        size_t chunk = (current.size() + tc - 1) / tc;
        std::atomic<size_t> done{0};

        std::vector<std::vector<uintptr_t>> threadHits(tc);
        std::vector<std::future<void>> futs;
        futs.reserve(tc);

        for (unsigned t = 0; t < tc; ++t)
        {
            futs.push_back(Utils::GlobalPool.push([&, t]
                                                  {
                auto &myHits = threadHits[t];
                std::vector<uint8_t> buf(patLen);
                size_t end = std::min(t * chunk + chunk, current.size());
                for (size_t i = t * chunk; i < end && Config::g_Running; ++i) {
                    uintptr_t addr = current[i];
                    int readBytes = dr.Read(addr, buf.data(), patLen);
                    if (readBytes > 0 && static_cast<size_t>(readBytes) >= patLen &&
                        std::memcmp(buf.data(), needle.data(), patLen) == 0) {
                        myHits.push_back(addr);
                    }

                    size_t finished = done.fetch_add(1) + 1;
                    if ((finished & 0x3FF) == 0)
                        progress_ = static_cast<float>(finished) / current.size();
                } }));
        }
        for (auto &f : futs)
            f.get();

        std::vector<uintptr_t> merged;
        for (auto &hits : threadHits)
        {
            merged.insert(merged.end(), hits.begin(), hits.end());
        }
        std::sort(merged.begin(), merged.end());
        merged.erase(std::unique(merged.begin(), merged.end()), merged.end());

        std::unique_lock lock(mutex_);
        addedList_.swap(merged);
        setBits_ = 0;
    }

public:
    MemScanner() = default;
    ~MemScanner() = default; // RAII handles cleanup
    MemScanner(const MemScanner &) = delete;
    MemScanner &operator=(const MemScanner &) = delete;

    // 返回扫描线程当前是否在运行。
    bool isScanning() const noexcept { return scanning_; }
    // 返回当前扫描进度百分比(0~1)。
    float progress() const noexcept { return progress_; }

    // 返回当前结果数量。
    size_t count() const
    {
        std::shared_lock lock(mutex_);
        return setBits_ + addedList_.size();
    }

    // 结果分页获取
    Results getPage(size_t start, size_t cnt) const
    {
        std::shared_lock lock(mutex_);
        if (setBits_ == 0 && addedList_.empty())
            return {};

        Results r;
        r.reserve(cnt);
        size_t skipped = 0;

        // 手动添加列表
        for (size_t i = 0; i < addedList_.size() && r.size() < cnt; ++i)
        {
            if (skipped++ < start)
                continue;
            r.push_back(addedList_[i]);
        }

        // 位图结果
        if (r.size() < cnt && bitmap_.valid() && setBits_ > 0)
        {
            for (const auto &reg : regions_)
            {
                if (r.size() >= cnt)
                    break;
                size_t byteS = reg.bitOffset / 8;
                size_t byteE = (reg.bitOffset + reg.bitCount + 7) / 8;

                for (size_t b = byteS; b < byteE && r.size() < cnt; ++b)
                {
                    uint8_t byte = bitmap_.data()[b];
                    if (!byte)
                        continue;

                    for (int bit = 0; bit < 8 && r.size() < cnt; ++bit)
                    {
                        if (!(byte & (1 << bit)))
                            continue;
                        size_t gb = b * 8 + bit;
                        if (gb < reg.bitOffset || gb >= reg.bitOffset + reg.bitCount)
                            continue;
                        if (skipped++ < start)
                            continue;
                        r.push_back(bitToAddr(gb));
                    }
                }
            }
        }
        return r;
    }

    // 清除
    void clear()
    {
        std::unique_lock lock(mutex_);
        bitmap_.release();
        values_.release();
        regions_.clear();
        addedList_.clear();
        setBits_ = 0;
    }

    // 单项操作
    void remove(uintptr_t addr)
    {
        std::unique_lock lock(mutex_);
        auto it = std::find(addedList_.begin(), addedList_.end(), addr);
        if (it != addedList_.end())
        {
            addedList_.erase(it);
            return;
        }

        size_t gb = addrToBit(addr);
        if (gb != SIZE_MAX && bitmap_.get(gb))
        {
            bitmap_.setOff(gb);
            --setBits_;
        }
    }

    // 向结果集合追加单个地址。
    void add(uintptr_t addr)
    {
        std::unique_lock lock(mutex_);
        size_t gb = addrToBit(addr);
        if (gb != SIZE_MAX)
        {
            if (!bitmap_.get(gb))
            {
                bitmap_.setOn(gb);
                ++setBits_;
            }
        }
        else
        {
            if (std::find(addedList_.begin(), addedList_.end(), addr) == addedList_.end())
                addedList_.push_back(addr);
        }
    }

    //  偏移应用
    void applyOffset(int64_t offset)
    {
        std::unique_lock lock(mutex_);

        auto applyOff = [offset](uintptr_t addr) -> uintptr_t
        {
            return offset > 0
                       ? addr + static_cast<uintptr_t>(offset)
                       : addr - static_cast<uintptr_t>(-offset);
        };

        // 手动列表
        for (auto &addr : addedList_)
            addr = applyOff(addr);

        // 位图
        if (!bitmap_.valid() || setBits_ == 0)
            return;

        std::vector<std::pair<uintptr_t, double>> temp;
        temp.reserve(setBits_);

        for (const auto &reg : regions_)
        {
            size_t byteS = reg.bitOffset / 8;
            size_t byteE = (reg.bitOffset + reg.bitCount + 7) / 8;
            for (size_t b = byteS; b < byteE; ++b)
            {
                uint8_t byte = bitmap_.data()[b];
                if (!byte)
                    continue;
                for (int bit = 0; bit < 8; ++bit)
                {
                    if (!(byte & (1 << bit)))
                        continue;
                    size_t gb = b * 8 + bit;
                    if (gb >= reg.bitOffset && gb < reg.bitOffset + reg.bitCount)
                        temp.push_back({applyOff(bitToAddr(gb)), valuesMap()[gb]});
                }
            }
        }

        auto scanRegs = dr.GetScanRegions();
        if (!initStorage(valueSize_, scanRegs, false))
            return;

        size_t actualSet = 0;
        for (const auto &[addr, val] : temp)
        {
            size_t gb = addrToBit(addr);
            if (gb != SIZE_MAX)
            {
                bitmap_.setOn(gb);
                valuesMap()[gb] = val;
                ++actualSet;
            }
        }
        setBits_ = actualSet;
    }

    // 执行指针链扫描主流程。
    template <typename T>

    void scan(pid_t pid, T target, Types::FuzzyMode mode, bool isFirst, double rangeMax = 0.0)
    {
        if (scanning_.exchange(true))
            return;

        // RAII guard 确保状态恢复
        struct Guard
        {
            std::atomic<bool> &s;
            std::atomic<float> &p;
            ~Guard()
            {
                s = false;
                p = 1.0f;
            }
        } guard{scanning_, progress_};

        progress_ = 0.0f;
        rangeMax_ = rangeMax;

        if (isFirst)
        {
            if (mode == Types::FuzzyMode::Unknown)
                scanFirstUnknown<T>(pid);
            else
                scanFirst<T>(pid, target, mode);
        }
        else
        {
            scanNext<T>(target, mode);
        }
    }

    void scanString(pid_t /*pid*/, const std::string &needle, bool isFirst)
    {
        if (scanning_.exchange(true))
            return;

        struct Guard
        {
            std::atomic<bool> &s;
            std::atomic<float> &p;
            ~Guard()
            {
                s = false;
                p = 1.0f;
            }
        } guard{scanning_, progress_};

        progress_ = 0.0f;
        if (isFirst)
            scanFirstString(needle);
        else
            scanNextString(needle);
    }
};

// ============================================================================
// 锁定管理器
// ============================================================================
class LockManager
{
private:
    struct LockItem
    {
        uintptr_t addr;
        Types::DataType type;
        std::string value;
    };
    std::list<LockItem> locks_;
    mutable std::mutex mutex_;
    std::future<void> writeTask_;
    std::atomic<bool> writeStop_{false};

    // 按地址查找锁定项。
    auto find(uintptr_t addr)
    {
        return std::ranges::find_if(locks_, [addr](auto &i)
                                    { return i.addr == addr; });
    }

    // 后台循环写入被锁定的内存项。
    void writeLoop()
    {
        while (!writeStop_.load(std::memory_order_acquire) && Config::g_Running)
        {
            {
                std::lock_guard lock(mutex_);
                for (auto &item : locks_)
                    MemUtils::WriteFromString(item.addr, item.type, item.value);
            }
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

public:
    LockManager()
    {
        writeTask_ = Utils::GlobalPool.push_io([this]
                                               { writeLoop(); });
    }

    ~LockManager()
    {
        writeStop_.store(true, std::memory_order_release);
        if (writeTask_.valid())
            writeTask_.wait();
    }

    // 判断目标地址是否处于锁定状态。
    bool isLocked(uintptr_t addr) const
    {
        std::lock_guard lock(mutex_);
        return std::ranges::any_of(locks_, [addr](const auto &i)
                                   { return i.addr == addr; });
    }

    // 切换目标地址的锁定状态。
    void toggle(uintptr_t addr, Types::DataType type)
    {
        std::lock_guard lock(mutex_);
        if (auto it = find(addr); it != locks_.end())
            locks_.erase(it);
        else
            locks_.push_back({addr, type, MemUtils::ReadAsString(addr, type)});
    }

    // 锁定指定地址并记录目标值。
    void lock(uintptr_t addr, Types::DataType type, const std::string &value)
    {
        std::lock_guard lk(mutex_);
        if (find(addr) == locks_.end())
            locks_.push_back({addr, type, value});
    }

    // 取消指定地址的锁定。
    void unlock(uintptr_t addr)
    {
        std::lock_guard lk(mutex_);
        std::erase_if(locks_, [addr](const auto &item)
                      { return item.addr == addr; });
    }

    // 批量锁定一组地址。
    void lockBatch(std::span<const uintptr_t> addrs, Types::DataType type)
    {
        std::lock_guard lk(mutex_);
        for (auto addr : addrs)
        {
            if (!std::ranges::any_of(locks_, [addr](const auto &item)
                                     { return item.addr == addr; }))
                locks_.emplace_back(addr, type, MemUtils::ReadAsString(addr, type));
        }
    }

    // 批量取消锁定一组地址。
    void unlockBatch(std::span<const uintptr_t> addrs)
    {
        std::lock_guard lk(mutex_);
        for (auto addr : addrs)
            std::erase_if(locks_, [addr](const auto &item)
                          { return item.addr == addr; });
    }

    // 清空当前模块维护的全部数据。
    void clear()
    {
        std::lock_guard lk(mutex_);
        locks_.clear();
    }
};

// ============================================================================
// 内存浏览器
// ============================================================================
class MemViewer
{
private:
    uintptr_t base_ = 0;
    Types::ViewFormat format_ = Types::ViewFormat::Hex;
    std::vector<uint8_t> buffer_;
    bool visible_ = false;
    bool readSuccess_ = false;
    Disasm::Disassembler disasm_;
    std::vector<Disasm::DisasmLine> disasmCache_;
    int disasmScrollIdx_ = 0;

public:
    MemViewer() : buffer_(Config::Constants::MEM_VIEW_DEFAULT_BYTES) {}

    // 返回当前视图可见状态。
    bool isVisible() const noexcept { return visible_; }
    // 设置当前视图可见状态。
    void setVisible(bool v) noexcept { visible_ = v; }
    // 返回当前内存浏览格式。
    Types::ViewFormat format() const noexcept { return format_; }
    // 返回最近一次读取是否成功。
    bool readSuccess() const noexcept { return readSuccess_; }
    // 返回当前浏览基址。
    uintptr_t base() const noexcept { return base_; }
    const std::vector<uint8_t> &buffer() const noexcept { return buffer_; }
    const std::vector<Disasm::DisasmLine> &getDisasm() const noexcept { return disasmCache_; }
    // 返回当前反汇编滚动索引。
    int disasmScrollIdx() const noexcept { return disasmScrollIdx_; }

    // 切换浏览格式并触发刷新。
    void setFormat(Types::ViewFormat fmt)
    {
        format_ = fmt;
        disasmScrollIdx_ = 0;
        refresh();
    }

    // 打开指定地址并初始化浏览状态。
    void open(uintptr_t addr)
    {
        if (format_ == Types::ViewFormat::Disasm)
            addr &= ~static_cast<uintptr_t>(3); // 强制 4 字节对齐
        base_ = addr;
        disasmScrollIdx_ = 0;
        refresh();
        visible_ = true;
    }

    // 按指定行数移动当前浏览窗口。
    void move(int lines, size_t step)
    {
        if (format_ == Types::ViewFormat::Disasm)
        {
            moveDisasm(lines);
        }
        else
        {
            int64_t delta = static_cast<int64_t>(lines) * static_cast<int64_t>(step);
            if (delta < 0 && base_ < static_cast<uintptr_t>(-delta))
                base_ = 0;
            else
                base_ += delta;
            refresh();
        }
    }

    // 重新读取并刷新当前浏览缓存。
    void refresh()
    {
        if (base_ > Config::Constants::ADDR_MAX)
        {
            readSuccess_ = false;
            disasmCache_.clear();
            return;
        }
        std::ranges::fill(buffer_, 0);
        readSuccess_ = (dr.Read(base_, buffer_.data(), buffer_.size()));
        if (!readSuccess_)
        {
            disasmCache_.clear();
            return;
        }
        if (format_ == Types::ViewFormat::Disasm)
        {
            disasmCache_.clear();
            disasmScrollIdx_ = 0;
            if (disasm_.IsValid() && !buffer_.empty())
            {
                // 安全限制：哪怕 buffer_ 特别大，最多只让 Capstone 一次解 1000 条指令
                disasmCache_ = disasm_.Disassemble(base_, buffer_.data(), buffer_.size(), 1000);
            }
        }
    }

    // 按偏移字符串调整当前浏览基址。
    bool applyOffset(std::string_view offsetStr)
    {
        auto result = MemUtils::ParseHexOffset(offsetStr);
        if (!result)
            return false;
        open(result->negative ? (base_ - result->offset) : (base_ + result->offset));
        return true;
    }

private:
    // 在反汇编模式下移动显示窗口。
    void moveDisasm(int lines)
    {
        if (lines == 0)
            return;

        int newIdx = disasmScrollIdx_ + lines;

        int margin = std::min(50, static_cast<int>(disasmCache_.size() / 4));
        if (margin < 0)
            margin = 0;

        if (disasmCache_.empty() || newIdx < 0 || newIdx + margin >= static_cast<int>(disasmCache_.size()))
        {
            // ARM64 中，1 行指令 = 4 字节
            int64_t deltaBytes = static_cast<int64_t>(newIdx) * 4;

            if (deltaBytes < 0 && base_ < static_cast<uintptr_t>(-deltaBytes))
            {
                base_ = 0;
            }
            else
            {
                base_ += deltaBytes;
            }

            // 强制 4 字节对齐，防止计算偏差
            base_ &= ~static_cast<uintptr_t>(3);

            disasmScrollIdx_ = 0;
            refresh();
        }
        else
        {
            disasmScrollIdx_ = newIdx;
        }
    }
};

// ============================================================================
// 指针管理器
// ============================================================================
class PointerManager
{
public:
    struct PtrData
    {
        uintptr_t address, value;
        PtrData() : address(0), value(0) {}
        PtrData(uintptr_t a, uintptr_t v) : address(a), value(v) {}
    };

    struct PtrDir
    {
        uintptr_t address, value;
        uint32_t start, end;
        PtrDir() : address(0), value(0), start(0), end(0) {}
        PtrDir(uintptr_t a, uintptr_t v, uint32_t s = 0, uint32_t e = 0)
            : address(a), value(v), start(s), end(e) {}
    };

    struct PtrRange
    {
        int level;
        int moduleIdx = -1;
        int segIdx = -1;
        bool isManual;
        bool isArray;
        uintptr_t manualBase;
        uintptr_t arrayBase;
        size_t arrayIndex;
        std::vector<PtrDir> results;
        PtrRange() : level(0), moduleIdx(-1), segIdx(-1), isManual(false),
                     isArray(false), manualBase(0), arrayBase(0), arrayIndex(0) {}
    };

    struct BinHeader
    {
        char sign[32];
        int module_count;
        int version;
        int size;
        int level;
        uint8_t scanBaseMode;
        uint64_t scanManualBase;
        uint64_t scanArrayBase;
        uint64_t scanArrayCount;
        uint64_t scanTarget;
    };

    struct BinSym
    {
        uint64_t start;
        char name[128];
        int segment;
        int pointer_count;
        int level;
        bool isBss;
        uint8_t sourceMode;
        uint64_t manualBase;
        uint64_t arrayBase;
        uint64_t arrayIndex;
    };

    struct BinLevel
    {
        unsigned int count;
        int level;
    };

    enum class BaseMode : int
    {
        Module = 0,
        Manual,
        Array
    };

private:
    std::mutex block_mtx_;
    std::condition_variable block_cv_;
    std::vector<PtrData> pointers_;
    std::vector<std::pair<uintptr_t, uintptr_t>> regions_;
    std::atomic<bool> scanning_{false};
    std::atomic<float> scanProgress_{0.0f};
    size_t chainCount_ = 0;

    // 生成可用的指针结果文件名。
    static std::string NextBinName()
    {
        char path[256];
        snprintf(path, sizeof(path), "Pointer.bin");
        if (access(path, F_OK) != 0)
            return path;
        for (int i = 1; i < 9999; i++)
        {
            snprintf(path, sizeof(path), "Pointer_%d.bin", i);
            if (access(path, F_OK) != 0)
                return path;
        }
        return "Pointer.bin";
    }

    template <typename F>
    // 借用缓冲块执行任务并自动归还。
    void with_buffer_block(char **bufs, int &idx, uintptr_t start, size_t len, F &&call)
    {
        char *buf;
        {
            std::unique_lock<std::mutex> lk(block_mtx_);
            block_cv_.wait(lk, [&idx]
                           { return idx >= 0; });
            buf = bufs[idx--];
        }
        struct BufGuard
        {
            char **b;
            int &i;
            char *p;
            std::mutex &m;
            std::condition_variable &cv;
            ~BufGuard()
            {
                std::lock_guard<std::mutex> lk(m);
                b[++i] = p;
                cv.notify_one();
            }
        } guard{bufs, idx, buf, block_mtx_, block_cv_};

        call(buf, start, len);
    }

    // 扫描缓冲块并提取候选指针。
    void collect_pointers_block(char *buf, uintptr_t start, size_t len, FILE *&out)
    {
        out = tmpfile();
        if (!out)
            return;

        if (dr.Read(start, buf, len) <= 0)
        {
            fclose(out);
            out = nullptr;
            return;
        }

        uintptr_t *vals = reinterpret_cast<uintptr_t *>(buf);
        size_t ptr_count = len / sizeof(uintptr_t);

        for (size_t i = 0; i < ptr_count; i++)
            vals[i] = MemUtils::Normalize(vals[i]);

        uintptr_t min_addr = regions_.front().first;
        uintptr_t sub = regions_.back().second - min_addr;

        PtrData d;
        for (size_t i = 0; i < ptr_count; i++)
        {
            if ((vals[i] - min_addr) > sub)
                continue;

            int lo = 0, hi = static_cast<int>(regions_.size()) - 1;
            while (lo <= hi)
            {
                int mid = (lo + hi) >> 1;
                if (regions_[mid].second <= vals[i])
                    lo = mid + 1;
                else
                    hi = mid - 1;
            }

            if (static_cast<size_t>(lo) >= regions_.size() || vals[i] < regions_[lo].first)
                continue;

            d.address = MemUtils::Normalize(start + i * sizeof(uintptr_t));
            d.value = vals[i];
            fwrite(&d, sizeof(d), 1, out);
        }
        fflush(out);
    }

    template <typename C, typename F, typename V>
    // 执行有序数据的二分查找定位。
    static void bin_search(C &c, F &&cmp, V target, size_t sz, int &lo, int &hi)
    {
        lo = 0;
        hi = static_cast<int>(sz) - 1;
        while (lo <= hi)
        {
            int mid = (lo + hi) >> 1;
            if (cmp(c[mid], target))
                lo = mid + 1;
            else
                hi = mid - 1;
        }
    }

    // 在候选指针中筛选可匹配项。
    void search_in_pointers(std::vector<PtrDir> &input, std::vector<PtrData *> &out, size_t offset, bool use_limit, size_t limit)
    {
        if (input.empty() || pointers_.empty())
            return;

        uintptr_t min_addr = regions_.front().first;
        uintptr_t sub = regions_.back().second - min_addr;
        size_t isz = input.size();
        std::vector<PtrData *> result;

        for (auto &pd : pointers_)
        {

            uintptr_t v = MemUtils::Normalize(pd.value);
            if ((v - min_addr) > sub)
                continue;

            int lo, hi;
            bin_search(input, [](auto &n, auto t)
                       { return n.address < t; }, v, isz, lo, hi);

            if (static_cast<size_t>(lo) >= isz)
                continue;

            if (MemUtils::Normalize(input[lo].address) - v > offset)
                continue;

            result.push_back(&pd);
        }

        size_t lim = use_limit ? std::min(limit, result.size()) : result.size();
        out.reserve(lim);
        for (size_t i = 0; i < lim; i++)
            out.push_back(result[i]);
    }

    // 按模块范围过滤并归档指针。
    void filter_to_ranges_module(std::vector<std::vector<PtrDir>> &dirs, std::vector<PtrRange> &ranges, std::vector<PtrData *> &curr, int level, const std::string &filterModule)
    {
        std::unordered_set<PtrData *> matched;
        const auto &info = dr.GetMemoryInfoRef();
        std::println("当前进程模块数量: {}", info.module_count);

        for (int mi = 0; mi < info.module_count; ++mi)
        {
            const auto &mod = info.modules[mi];
            std::string_view fullPath(mod.name);
            if (auto slash = fullPath.rfind('/'); slash != std::string_view::npos)
                fullPath = fullPath.substr(slash + 1);

            if (!filterModule.empty() && fullPath.find(filterModule) == std::string_view::npos)
                continue;

            for (int si = 0; si < mod.seg_count; ++si)
            {

                uintptr_t segStart = MemUtils::Normalize(mod.segs[si].start);
                uintptr_t segEnd = MemUtils::Normalize(mod.segs[si].end);

                PtrRange pr;
                pr.level = level;
                pr.moduleIdx = mi;
                pr.segIdx = si;
                pr.isManual = false;
                pr.isArray = false;
                for (auto *p : curr)
                {
                    uintptr_t addr = MemUtils::Normalize(p->address);
                    if (addr >= segStart && addr < segEnd)
                    {
                        if (matched.insert(p).second)
                            pr.results.emplace_back(addr, MemUtils::Normalize(p->value), 0u, 1u);
                    }
                }
                if (!pr.results.empty())
                    ranges.push_back(std::move(pr));
            }
        }
        push_unmatched(dirs, matched, curr, level);
    }

    // 按组合基址策略过滤并归档指针。
    void filter_to_ranges_combined(std::vector<std::vector<PtrDir>> &dirs, std::vector<PtrRange> &ranges, std::vector<PtrData *> &curr, int level, BaseMode scanMode, const std::string &filterModule, uintptr_t manualBase, size_t manualMaxOffset, uintptr_t arrayBase, const std::vector<std::pair<size_t, uintptr_t>> &arrayEntries, size_t maxOffset)
    {
        std::unordered_set<PtrData *> matched;
        const auto &info = dr.GetMemoryInfoRef();

        struct FlatSeg
        {
            uintptr_t start, end;
            int modIdx, segIdx;
        };
        std::vector<FlatSeg> flatSegs;
        for (int mi = 0; mi < info.module_count; ++mi)
        {
            const auto &mod = info.modules[mi];
            std::string_view fullPath(mod.name);
            if (auto slash = fullPath.rfind('/'); slash != std::string_view::npos)
                fullPath = fullPath.substr(slash + 1);

            if (!filterModule.empty() && fullPath.find(filterModule) == std::string_view::npos)
                continue;

            for (int si = 0; si < mod.seg_count; ++si)
            {

                flatSegs.push_back({MemUtils::Normalize(mod.segs[si].start),
                                    MemUtils::Normalize(mod.segs[si].end),
                                    mi, si});
            }
        }
        std::sort(flatSegs.begin(), flatSegs.end(), [](const auto &a, const auto &b)
                  { return a.start < b.start; });

        std::map<std::pair<int, int>, PtrRange> modRangeMap;

        for (auto *p : curr)
        {
            uintptr_t addr = MemUtils::Normalize(p->address);
            auto it = std::upper_bound(flatSegs.begin(), flatSegs.end(), addr, [](uintptr_t a, const FlatSeg &b)
                                       { return a < b.start; });
            if (it != flatSegs.begin())
            {
                auto prev = std::prev(it);
                if (addr >= prev->start && addr < prev->end)
                {
                    if (matched.insert(p).second)
                    {
                        auto &pr = modRangeMap[{prev->modIdx, prev->segIdx}];
                        if (pr.results.empty())
                        {
                            pr.level = level;
                            pr.moduleIdx = prev->modIdx;
                            pr.segIdx = prev->segIdx;
                            pr.isManual = false;
                            pr.isArray = false;
                        }
                        pr.results.emplace_back(addr, MemUtils::Normalize(p->value), 0u, 1u);
                    }
                }
            }
        }

        for (auto &[k, v] : modRangeMap)
            ranges.push_back(std::move(v));

        if (scanMode == BaseMode::Manual && manualBase)
        {
            uintptr_t normManualBase = MemUtils::Normalize(manualBase);
            PtrRange pr;
            pr.level = level;
            pr.moduleIdx = -1;
            pr.segIdx = -1;
            pr.isManual = true;
            pr.isArray = false;
            pr.manualBase = normManualBase;
            for (auto *p : curr)
            {
                uintptr_t addr = MemUtils::Normalize(p->address);
                if (addr >= normManualBase && (addr - normManualBase) <= manualMaxOffset)
                {
                    if (matched.insert(p).second)
                        pr.results.emplace_back(addr, MemUtils::Normalize(p->value), 0u, 1u);
                }
            }
            if (!pr.results.empty())
                ranges.push_back(std::move(pr));
        }

        if (scanMode == BaseMode::Array && !arrayEntries.empty())
        {
            for (const auto &[idx, objAddr] : arrayEntries)
            {

                PtrRange pr;
                pr.level = level;
                pr.moduleIdx = -1;
                pr.segIdx = -1;
                pr.isManual = false;
                pr.isArray = true;
                pr.arrayBase = MemUtils::Normalize(arrayBase);
                pr.arrayIndex = idx;
                for (auto *p : curr)
                {
                    uintptr_t addr = MemUtils::Normalize(p->address);
                    if (addr >= objAddr && (addr - objAddr) <= maxOffset)
                    {
                        if (matched.insert(p).second)
                            pr.results.emplace_back(addr, MemUtils::Normalize(p->value), 0u, 1u);
                    }
                }
                if (!pr.results.empty())
                    ranges.push_back(std::move(pr));
            }
        }

        push_unmatched(dirs, matched, curr, level);
    }

    // 把未匹配项追加到下一层处理集合。
    void push_unmatched(std::vector<std::vector<PtrDir>> &dirs, std::unordered_set<PtrData *> &matched, std::vector<PtrData *> &curr, int level)
    {
        for (auto *p : curr)
        {
            if (matched.find(p) == matched.end())
                dirs[level].emplace_back(MemUtils::Normalize(p->address), MemUtils::Normalize(p->value), 0u, 1u);
        }
    }

    // 回填父子区间索引关系。
    void assoc_index(std::vector<PtrDir> &prev, PtrDir *start, size_t count, size_t offset)
    {
        size_t sz = prev.size();
        for (size_t i = 0; i < count; i++)
        {
            int lo, hi;
            uintptr_t normVal = MemUtils::Normalize(start[i].value);
            bin_search(prev, [](auto &x, auto t)
                       { return x.address < t; }, normVal, sz, lo, hi);
            start[i].start = lo;
            bin_search(prev, [](auto &x, auto t)
                       { return x.address <= t; }, normVal + offset, sz, lo, hi);
            start[i].end = lo;
        }
    }

    // 并发建立各层索引关联。
    std::vector<std::future<void>> create_assoc_index(std::vector<PtrDir> &prev, std::vector<PtrDir> &curr, size_t offset)
    {
        std::vector<std::future<void>> futures;
        if (curr.empty())
            return futures;
        size_t total = curr.size(), pos = 0;
        while (pos < total)
        {
            size_t chunk = std::min(total - pos, static_cast<size_t>(10000));
            futures.push_back(Utils::GlobalPool.push(
                [this, &prev, s = &curr[pos], chunk, offset]
                { assoc_index(prev, s, chunk, offset); }));
            pos += chunk;
        }
        return futures;
    }

    struct DirTree
    {
        std::vector<std::vector<size_t>> counts;
        std::vector<std::vector<PtrDir *>> contents;
        bool valid = false;
    };

    // 合并相邻且可并入的区间节点。
    void merge_dirs(const std::vector<PtrDir *> &sorted_ptrs, PtrDir *base_dir, std::vector<PtrDir *> &out)
    {
        size_t dist = 0;
        uint32_t right = 0;
        out.reserve(sorted_ptrs.size());

        for (auto *p : sorted_ptrs)
        {
            if (right <= p->start)
            {
                dist += p->start - right;
                for (uint32_t j = p->start; j < p->end; j++)
                    out.push_back(&base_dir[j]);
                right = p->end;
            }
            else if (right < p->end)
            {
                for (uint32_t j = right; j < p->end; j++)
                    out.push_back(&base_dir[j]);
                right = p->end;
            }
            p->start -= static_cast<uint32_t>(dist);
            p->end -= static_cast<uint32_t>(dist);
        }
    }

    // 构建层级化指针目录树结构。
    DirTree build_dir_tree(std::vector<std::vector<PtrDir>> &dirs, std::vector<PtrRange> &ranges)
    {
        DirTree tree;
        if (ranges.empty())
            return tree;

        int max_level = 0;
        for (auto &r : ranges)
            max_level = std::max(max_level, r.level);

        std::vector<std::vector<PtrRange *>> level_ranges(dirs.size());
        for (auto &r : ranges)
            level_ranges[r.level].push_back(&r);

        tree.counts.resize(max_level + 1);
        tree.contents.resize(max_level + 1);

        for (int i = max_level; i > 0; i--)
        {
            std::vector<PtrDir *> stn;
            for (auto *r : level_ranges[i])
                for (auto &v : r->results)
                    stn.push_back(&v);
            for (auto *p : tree.contents[i])
                stn.push_back(p);

            std::sort(stn.begin(), stn.end(), [](auto a, auto b)
                      { return a->start < b->start; });

            std::vector<PtrDir *> merged_out;
            merge_dirs(stn, dirs[i - 1].data(), merged_out);

            if (merged_out.empty())
                return tree;

            tree.contents[i - 1] = std::move(merged_out);
        }

        tree.counts[0] = {0, 1};
        for (int i = 1; i <= max_level; i++)
        {
            auto &cc = tree.counts[i];
            size_t c = 0;
            cc.reserve(tree.contents[i - 1].size() + 1);
            cc.push_back(c);
            for (size_t j = 0; j < tree.contents[i - 1].size(); j++)
            {
                c += tree.counts[i - 1][tree.contents[i - 1][j]->end] - tree.counts[i - 1][tree.contents[i - 1][j]->start];
                cc.push_back(c);
            }
        }

        tree.valid = true;
        return tree;
    }

    // 将指针树结果序列化写入文件。
    void write_bin_file(std::vector<std::vector<PtrDir *>> &contents, std::vector<PtrRange> &ranges, FILE *f, BaseMode scanMode, uintptr_t target, uintptr_t manualBase, uintptr_t arrayBase, size_t arrayCount)
    {
        const auto &memInfo = dr.GetMemoryInfoRef();
        BinHeader hdr{};
        strcpy(hdr.sign, ".bin pointer chain");
        hdr.size = sizeof(uintptr_t);
        hdr.version = 102;
        hdr.module_count = static_cast<int>(ranges.size());
        hdr.level = static_cast<int>(contents.size()) - 1;
        hdr.scanBaseMode = static_cast<uint8_t>(scanMode);
        hdr.scanManualBase = MemUtils::Normalize(manualBase);
        hdr.scanArrayBase = MemUtils::Normalize(arrayBase);
        hdr.scanArrayCount = arrayCount;
        hdr.scanTarget = MemUtils::Normalize(target);
        fwrite(&hdr, sizeof(hdr), 1, f);

        for (auto &r : ranges)
        {
            BinSym sym{};
            if (r.isManual)
            {
                sym.sourceMode = 1;
                sym.manualBase = MemUtils::Normalize(r.manualBase);
                sym.start = sym.manualBase;
                strncpy(sym.name, "manual", sizeof(sym.name) - 1);
                sym.segment = 0;
                sym.isBss = false;
            }
            else if (r.isArray)
            {
                sym.sourceMode = 2;
                sym.arrayBase = MemUtils::Normalize(r.arrayBase);
                sym.arrayIndex = r.arrayIndex;

                uintptr_t objAddr = 0;
                dr.Read(MemUtils::Normalize(r.arrayBase) + r.arrayIndex * sizeof(uintptr_t), &objAddr, sizeof(objAddr));
                sym.start = MemUtils::Normalize(objAddr);
                char arrName[128];
                snprintf(arrName, sizeof(arrName), "array[%zu]", r.arrayIndex);
                strncpy(sym.name, arrName, sizeof(sym.name) - 1);
                sym.segment = 0;
                sym.isBss = false;
            }
            else
            {
                const auto &mod = memInfo.modules[r.moduleIdx];
                const auto &seg = mod.segs[r.segIdx];

                sym.start = MemUtils::Normalize(seg.start);
                sym.segment = seg.index;
                sym.isBss = (seg.index == -1);

                std::string_view fullPath(mod.name);
                if (auto slash = fullPath.rfind('/'); slash != std::string_view::npos)
                    fullPath = fullPath.substr(slash + 1);
                strncpy(sym.name, fullPath.data(), std::min(fullPath.size(), sizeof(sym.name) - 1));
                sym.sourceMode = 0;
            }
            sym.level = r.level;
            sym.pointer_count = static_cast<int>(r.results.size());
            fwrite(&sym, sizeof(sym), 1, f);
            fwrite(r.results.data(), sizeof(PtrDir), r.results.size(), f);
        }

        for (size_t i = 0; i + 1 < contents.size(); i++)
        {
            BinLevel ll{};
            ll.level = static_cast<int>(i);
            ll.count = static_cast<unsigned int>(contents[i].size());
            fwrite(&ll, sizeof(ll), 1, f);
            for (auto *p : contents[i])
                fwrite(p, sizeof(PtrDir), 1, f);
        }
        fflush(f);
    }

public:
    PointerManager() = default;
    ~PointerManager() = default;

    // 返回指针扫描任务是否仍在运行。
    bool isScanning() const noexcept { return scanning_; }
    // 执行扫描逻辑并更新结果。
    float scanProgress() const noexcept { return scanProgress_; }
    // 返回当前结果数量。
    size_t count() const noexcept { return chainCount_; }

    // 采集进程可用指针并建立初始集合。
    size_t CollectPointers(int buf_count = 10, int buf_size = 1 << 20)
    {
        pointers_.clear();
        if (regions_.empty())
            return 0;
        int idx = buf_count - 1;
        std::vector<char *> bufs(buf_count);
        for (int i = 0; i < buf_count; i++)
            bufs[i] = new char[buf_size];
        std::vector<FILE *> tmp_files;
        std::mutex tmp_mtx;
        std::vector<std::future<void>> futures;
        for (auto &[rstart, rend] : regions_)
        {
            for (uintptr_t pos = rstart; pos < rend; pos += buf_size)
            {
                futures.push_back(Utils::GlobalPool.push(
                    [this, &bufs, &idx, pos, chunk = std::min(static_cast<size_t>(rend - pos), static_cast<size_t>(buf_size)), &tmp_files, &tmp_mtx]
                    {
                        FILE *out = nullptr;
                        with_buffer_block(bufs.data(), idx, pos, chunk,
                                          [this, &out](char *buf, uintptr_t s, size_t l)
                                          { collect_pointers_block(buf, s, l, out); });
                        if (out)
                        {
                            std::lock_guard<std::mutex> lk(tmp_mtx);
                            tmp_files.push_back(out);
                        }
                    }));
            }
        }
        for (auto &f : futures)
            f.get();

        FILE *merged = tmpfile();
        auto *mbuf = new char[1 << 20];
        for (auto *tf : tmp_files)
        {
            rewind(tf);
            size_t sz;
            while ((sz = fread(mbuf, 1, 1 << 20, tf)) > 0)
                fwrite(mbuf, sz, 1, merged);
            fclose(tf);
        }
        delete[] mbuf;
        fflush(merged);

        struct stat st;
        fstat(fileno(merged), &st);
        size_t total = st.st_size / sizeof(PtrData);
        if (total > 0)
        {
            pointers_.resize(total);
            rewind(merged);
            fread(pointers_.data(), sizeof(PtrData), total, merged);
        }
        fclose(merged);

        for (int i = 0; i < buf_count; i++)
            delete[] bufs[i];

        return pointers_.size();
    }

    // 执行指针链扫描主流程。
    void scan(pid_t /*pid*/, uintptr_t target, int depth, int maxOffset, bool useManual, uintptr_t manualBase, int manualMaxOffset, bool useArray, uintptr_t arrayBase, size_t arrayCount, const std::string &filterModule)
    {
        if (scanning_.exchange(true))
            return;

        struct ScanGuard
        {
            std::atomic<bool> &scanning;
            std::atomic<float> &progress;
            ~ScanGuard()
            {
                scanning = false;
                progress = 1.0f;
            }
        } guard{scanning_, scanProgress_};

        scanProgress_ = 0.0f;
        chainCount_ = 0;

        target = MemUtils::Normalize(target);
        manualBase = MemUtils::Normalize(manualBase);
        arrayBase = MemUtils::Normalize(arrayBase);

        std::println("=== 开始指针扫描 ===");
        std::println("目标: {:x}, 深度: {}, 偏移: {}", target, depth, maxOffset);

        regions_ = dr.GetScanRegions();

        for (auto &[rstart, rend] : regions_)
        {
            rstart = MemUtils::Normalize(rstart);
            rend = MemUtils::Normalize(rend);
        }
        std::sort(regions_.begin(), regions_.end());

        if (CollectPointers() == 0 || pointers_.empty())
        {
            std::println(stderr, "扫描失败: 内存快照为空");
            return;
        }
        std::println("内存快照数量: {}", pointers_.size());

        BaseMode scanMode = useManual ? BaseMode::Manual : (useArray ? BaseMode::Array : BaseMode::Module);

        FILE *outfile = tmpfile();
        if (!outfile)
        {
            std::println(stderr, "无法创建临时文件");
            return;
        }

        std::vector<PtrRange> ranges;
        std::vector<std::vector<PtrDir>> dirs(depth + 1);
        size_t fidx = 0;
        uint64_t totalChains = 0;

        std::vector<std::pair<size_t, uintptr_t>> arrayEntries;
        if (scanMode == BaseMode::Array && arrayBase && arrayCount > 0)
        {
            for (size_t i = 0; i < arrayCount; i++)
            {
                uintptr_t ptr = 0;

                if (dr.Read(arrayBase + i * sizeof(uintptr_t), &ptr, sizeof(ptr)))
                {
                    ptr = MemUtils::Normalize(ptr);
                    if (MemUtils::IsValidAddr(ptr))
                        arrayEntries.emplace_back(i, ptr);
                }
            }
        }

        dirs[0].emplace_back(target, 0, 0, 1);
        std::sort(dirs[0].begin(), dirs[0].end(), [](const PtrDir &a, const PtrDir &b)
                  { return a.address < b.address; });
        std::println("Level 0 初始化完成，目标地址数量: {}", dirs[0].size());

        std::vector<std::future<void>> allFutures;

        for (int level = 1; level <= depth; level++)
        {
            std::vector<PtrData *> curr;
            search_in_pointers(dirs[level - 1], curr, static_cast<size_t>(maxOffset), false, 0);

            if (curr.empty())
            {
                std::println("扫描在 Level {} 结束: 未找到指向上级的指针", level);
                break;
            }

            std::println("Level {} 搜索结果: 找到 {} 个指针", level, curr.size());
            std::sort(curr.begin(), curr.end(), [](auto a, auto b)
                      { return a->address < b->address; });

            filter_to_ranges_combined(dirs, ranges, curr, level, scanMode, filterModule, manualBase, static_cast<size_t>(manualMaxOffset), arrayBase, arrayEntries, static_cast<size_t>(maxOffset));

            for (auto &f : create_assoc_index(dirs[level - 1], dirs[level], static_cast<size_t>(maxOffset)))
                allFutures.push_back(std::move(f));

            scanProgress_ = static_cast<float>(level + 1) / (depth + 2);
        }

        for (; fidx < ranges.size(); fidx++)
        {
            if (ranges[fidx].level > 0)
            {
                for (auto &f : create_assoc_index(dirs[ranges[fidx].level - 1], ranges[fidx].results, static_cast<size_t>(maxOffset)))
                    allFutures.push_back(std::move(f));
            }
        }

        for (auto &f : allFutures)
        {
            if (f.valid())
                f.get();
        }
        allFutures.clear();

        if (!ranges.empty())
        {
            auto tree = build_dir_tree(dirs, ranges);
            if (tree.valid)
            {
                for (auto &r : ranges)
                {
                    if (static_cast<size_t>(r.level) < tree.counts.size())
                    {
                        for (auto &v : r.results)
                        {
                            if (v.end < tree.counts[r.level].size() && v.start < tree.counts[r.level].size())
                                totalChains += tree.counts[r.level][v.end] - tree.counts[r.level][v.start];
                        }
                    }
                }

                std::println("开始写入文件，正在保存 {} 条链条...", totalChains);
                write_bin_file(tree.contents, ranges, outfile, scanMode, target, manualBase, arrayBase, arrayCount);
                std::println("文件写入完成，总链数: {}", totalChains);
            }
        }
        else
        {
            std::println("结果为空: ranges vector is empty");
        }

        std::string autoName = NextBinName();
        if (FILE *saveFile = fopen(autoName.c_str(), "w+b"))
        {
            rewind(outfile);
            char buf[1 << 16];
            size_t sz;
            while ((sz = fread(buf, 1, sizeof(buf), outfile)) > 0)
                fwrite(buf, sz, 1, saveFile);
            fflush(saveFile);
            fclose(saveFile);
            std::println("结果已保存至: {}", autoName);
        }
        else
        {
            std::println(stderr, "无法保存文件: {}", autoName);
        }

        fclose(outfile);
        chainCount_ = static_cast<size_t>(totalChains);
    }

    struct MemoryGraph
    {
        BinHeader hdr{};
        struct Block
        {
            BinSym sym;
            std::vector<PtrDir> roots;
        };
        std::vector<Block> blocks;
        std::vector<std::vector<PtrDir>> levels;

        // 从二进制文件加载指针图数据。
        bool load(const std::string &path)
        {
            int fd = open(path.c_str(), O_RDONLY);
            if (fd < 0)
                return false;
            struct stat st;
            fstat(fd, &st);
            if (st.st_size < (long)sizeof(BinHeader))
            {
                close(fd);
                return false;
            }

            char *raw = (char *)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
            if (raw == MAP_FAILED)
            {
                close(fd);
                return false;
            }

            char *cur = raw;
            char *eof = raw + st.st_size;
            hdr = *(BinHeader *)cur;
            cur += sizeof(BinHeader);

            if (hdr.level + 1 < 0 || hdr.level + 1 > 100)
            {
                munmap(raw, st.st_size);
                close(fd);
                return false;
            }

            blocks.clear();
            levels.clear();
            for (int i = 0; i < hdr.module_count; ++i)
            {
                if (cur + sizeof(BinSym) > eof)
                    break;
                BinSym *s = (BinSym *)cur;
                cur += sizeof(BinSym);
                long need = s->pointer_count * sizeof(PtrDir);
                if (cur + need > eof)
                    break;

                Block blk;
                blk.sym = *s;
                blk.roots.assign((PtrDir *)cur, (PtrDir *)(cur + need));
                blocks.push_back(std::move(blk));
                cur += need;
            }

            levels.resize(hdr.level + 1 > 0 ? hdr.level + 1 : 1);
            while (cur + sizeof(BinLevel) <= eof)
            {
                BinLevel *bl = (BinLevel *)cur;
                cur += sizeof(BinLevel);
                if (bl->level < 0 || bl->level >= (int)levels.size())
                    break;
                long need = bl->count * sizeof(PtrDir);
                if (cur + need > eof)
                    break;
                levels[bl->level].assign((PtrDir *)cur, (PtrDir *)(cur + need));
                cur += need;
            }
            munmap(raw, st.st_size);
            close(fd);
            return true;
        }

        // 将当前指针图保存到文件。
        bool save(const std::string &path)
        {
            FILE *f = fopen(path.c_str(), "wb");
            if (!f)
                return false;
            fwrite(&hdr, sizeof(BinHeader), 1, f);
            for (const auto &blk : blocks)
            {
                fwrite(&blk.sym, sizeof(BinSym), 1, f);
                if (!blk.roots.empty())
                    fwrite(blk.roots.data(), sizeof(PtrDir), blk.roots.size(), f);
            }
            for (int i = 0; i < (int)levels.size(); ++i)
            {
                BinLevel bl;
                bl.level = i;
                bl.count = levels[i].size();
                fwrite(&bl, sizeof(BinLevel), 1, f);
                if (!levels[i].empty())
                    fwrite(levels[i].data(), sizeof(PtrDir), levels[i].size(), f);
            }
            fclose(f);
            return true;
        }
    };

    // 递归校验并裁剪无效指针分支。
    static bool prune_dfs(const PtrDir &nodeA, const PtrDir &nodeB, int current_level, const MemoryGraph &GA, const MemoryGraph &GB, std::vector<std::vector<uint8_t>> &memo)
    {
        // 成功触底，返回 true
        if (current_level < 0)
            return true;

        const auto &layerA = GA.levels[current_level];
        uint32_t startA = std::min((uint32_t)layerA.size(), nodeA.start);
        uint32_t endA = std::min((uint32_t)layerA.size(), nodeA.end);
        if (startA >= endA)
            return false;

        const auto &layerB = (current_level < (int)GB.levels.size()) ? GB.levels[current_level] : std::vector<PtrDir>();
        uint32_t startB = std::min((uint32_t)layerB.size(), nodeB.start);
        uint32_t endB = std::min((uint32_t)layerB.size(), nodeB.end);

        bool any_valid = false;

        for (uint32_t i = startA; i < endA; ++i)
        {
            // 通过偏移量在进程 B 中计算期望的下级地址
            uint64_t expected_addr_B = nodeB.value + (layerA[i].address - nodeA.value);

            if (startB < endB)
            {
                auto it = std::lower_bound(layerB.begin() + startB, layerB.begin() + endB, expected_addr_B,
                                           [](const PtrDir &n, uint64_t val)
                                           { return n.address < val; });

                if (it != layerB.begin() + endB && it->address == expected_addr_B)
                {
                    // 找到了进程 B 中对应的子节点，进行下一步验证
                    if (memo[current_level][i] == 1)
                    {
                        any_valid = true;
                    }
                    else if (prune_dfs(layerA[i], *it, current_level - 1, GA, GB, memo))
                    {
                        memo[current_level][i] = 1; // 只记录成功的验证，防止假阳性污染
                        any_valid = true;
                    }
                }
            }
        }
        return any_valid;
    }
    // 合并多轮扫描结果并裁剪失效链。
    void MergeBins()
    {
        Utils::GlobalPool.post([]()
                               {
            std::println("=== [MergeBins] 开始基于图裁剪算法的极速合并 ===");

            std::vector<std::string> files;
            if (access("Pointer.bin", F_OK) == 0) files.push_back("Pointer.bin");
            for (int i = 1; i < 9999; ++i) {
                char buf[64]; snprintf(buf, 64, "Pointer_%d.bin", i);
                if (access(buf, F_OK) == 0) files.push_back(buf); else if (i > 50) break;
            }

            if (files.size() < 2) { std::println("文件不足({})，跳过合并。", files.size()); return; }

            MemoryGraph GA;
            std::println("加载基准指针图: {}", files[0]);
            if (!GA.load(files[0])) return;

            for (size_t f_idx = 1; f_idx < files.size(); ++f_idx) {
                std::println("正在比对并裁剪: {}", files[f_idx]);
                MemoryGraph GB;
                if (!GB.load(files[f_idx])) continue;

                std::vector<std::vector<uint8_t>> memo_levels(GA.levels.size());
                for (size_t i = 0; i < GA.levels.size(); ++i)
                    memo_levels[i].resize(GA.levels[i].size(), 0);

                std::vector<std::vector<uint8_t>> memo_roots(GA.blocks.size());

                for (size_t b = 0; b < GA.blocks.size(); ++b) {
                    memo_roots[b].resize(GA.blocks[b].roots.size(), 0); // 默认为0即可

                    int match_b = -1;
                    for (size_t j = 0; j < GB.blocks.size(); ++j) {
                        if (GA.blocks[b].sym.sourceMode == GB.blocks[j].sym.sourceMode &&
                            GA.blocks[b].sym.segment == GB.blocks[j].sym.segment &&
                            strcmp(GA.blocks[b].sym.name, GB.blocks[j].sym.name) == 0) {
                            match_b = j; break;
                        }
                    }
                    if (match_b == -1) continue;

                    uint64_t baseA = (GA.blocks[b].sym.sourceMode == 1) ? GA.blocks[b].sym.manualBase : GA.blocks[b].sym.start;
                    uint64_t baseB = (GB.blocks[match_b].sym.sourceMode == 1) ? GB.blocks[match_b].sym.manualBase : GB.blocks[match_b].sym.start;

                    for (size_t r = 0; r < GA.blocks[b].roots.size(); ++r) {
                        auto it = std::lower_bound(GB.blocks[match_b].roots.begin(), GB.blocks[match_b].roots.end(),
                            baseB + (GA.blocks[b].roots[r].address - baseA),
                            [](const PtrDir& n, uint64_t val) { return n.address < val; });

                        if (it != GB.blocks[match_b].roots.end() && it->address == baseB + (GA.blocks[b].roots[r].address - baseA)) {
                            // 修复：从 sym.level - 1 向下遍历
                            if (prune_dfs(GA.blocks[b].roots[r], *it, GA.blocks[b].sym.level - 1, GA, GB, memo_levels))
                                memo_roots[b][r] = 1;
                        }
                    }
                }

                MemoryGraph G_next;
                G_next.hdr = GA.hdr;
                G_next.levels.resize(GA.levels.size());

                std::vector<std::vector<uint32_t>> new_idx(GA.levels.size());
                for (int L = 0; L < (int)GA.levels.size(); ++L) {
                    new_idx[L].resize(GA.levels[L].size(), 0);
                    for (size_t i = 0; i < GA.levels[L].size(); ++i) {
                        if (memo_levels[L][i] == 1) {
                            new_idx[L][i] = G_next.levels[L].size();
                            G_next.levels[L].push_back(GA.levels[L][i]);
                        }
                    }
                }

                for (size_t b = 0; b < GA.blocks.size(); ++b) {
                    MemoryGraph::Block next_blk;
                    next_blk.sym = GA.blocks[b].sym;
                    for (size_t r = 0; r < GA.blocks[b].roots.size(); ++r) {
                        if (memo_roots[b][r] == 1) next_blk.roots.push_back(GA.blocks[b].roots[r]);
                    }
                    if (!next_blk.roots.empty()) {
                        next_blk.sym.pointer_count = next_blk.roots.size();
                        G_next.blocks.push_back(std::move(next_blk));
                    }
                }
                G_next.hdr.module_count = G_next.blocks.size();

                auto repair_links = [](std::vector<PtrDir>& parents, const std::vector<uint8_t>& child_memos, const std::vector<uint32_t>& child_new_idx) {
                    uint32_t max_child = child_memos.size();
                    for (auto& p : parents) {
                        uint32_t n_start = 0, n_end = 0; bool found = false;
                        for (uint32_t i = std::min(max_child, p.start); i < std::min(max_child, p.end); ++i) {
                            if (child_memos[i] == 1) {
                                if (!found) { n_start = child_new_idx[i]; found = true; }
                                n_end = child_new_idx[i] + 1;
                            }
                        }
                        p.start = n_start; p.end = n_end;
                    }
                };

                // 修复：重新连接树枝时匹配对应正确的下级 Level
                for (auto& blk : G_next.blocks) {
                    int child_level = blk.sym.level - 1;
                    if (child_level >= 0 && child_level < (int)memo_levels.size()) {
                        repair_links(blk.roots, memo_levels[child_level], new_idx[child_level]);
                    } else {
                        for (auto& r : blk.roots) { r.start = 0; r.end = 0; }
                    }
                }
                for (int L = 1; L < (int)G_next.levels.size(); ++L) {
                    repair_links(G_next.levels[L], memo_levels[L - 1], new_idx[L - 1]);
                }

                GA = std::move(G_next);

                size_t remaining_roots = 0;
                for(auto& blk : GA.blocks) remaining_roots += blk.roots.size();
                std::println("  该轮裁剪完毕，剩余有效起始节点: {} 个", remaining_roots);
                if (GA.blocks.empty()) break;
            }

            GA.save("Pointer_Merged.tmp");
            for (const auto& fn : files) remove(fn.c_str());
            rename("Pointer_Merged.tmp", "Pointer.bin");

            std::println("图层合并结束！已成功剔除失效的指针树分支并生成 Pointer.bin"); });
    }

    // 将指针链导出为可读文本。
    void ExportToTxt()
    {
        std::println("=== 导出文本链条  ===");

        MemoryGraph G;
        if (!G.load("Pointer.bin"))
        {
            std::println(stderr, "无法加载文件");
            return;
        }

        FILE *fOut = fopen("Pointer_Export.txt", "w");
        if (!fOut)
            return;

        fprintf(fOut, "// Pointer Scan Export\n");
        fprintf(fOut, "// Version: %d, Depth: %d\n", G.hdr.version, G.hdr.level);
        fprintf(fOut, "// Target: 0x%llX\n", (unsigned long long)G.hdr.scanTarget);
        fprintf(fOut, "// Base Mode: %d (0=Module, 1=Manual, 2=Array)\n", G.hdr.scanBaseMode);
        fprintf(fOut, "// ========================================\n\n");

        size_t chainCount = 0;
        int64_t offsets[32];
        int offsetCount = 0;
        std::string currentBasePrefix;

        // 修复：从高层级向低层级递归
        std::function<void(int, const PtrDir &)> dfs = [&](int current_level, const PtrDir &node)
        {
            // < 0 证明我们成功触底到了 Target 级别
            if (current_level < 0)
            {
                fprintf(fOut, "%s", currentBasePrefix.c_str());
                for (int i = 0; i < offsetCount; ++i)
                {
                    if (offsets[i] >= 0)
                        fprintf(fOut, " + 0x%llX", (unsigned long long)offsets[i]);
                    else
                        fprintf(fOut, " - 0x%llX", (unsigned long long)(-offsets[i]));
                }
                fprintf(fOut, "\n");
                chainCount++;
                return;
            }

            // 跳过半路夭折的断头链路
            if (node.start >= node.end)
                return;

            for (uint32_t i = node.start; i < node.end; ++i)
            {
                if (offsetCount < 32)
                {
                    offsets[offsetCount++] = (int64_t)G.levels[current_level][i].address - (int64_t)node.value;
                    dfs(current_level - 1, G.levels[current_level][i]); // 向下找
                    offsetCount--;
                }
            }
        };

        for (const auto &blk : G.blocks)
        {
            char baseStr[256];
            uint64_t baseAddr;

            switch (blk.sym.sourceMode)
            {
            case 1:
                snprintf(baseStr, sizeof(baseStr), "\"Manual_0x%llX\"", (unsigned long long)blk.sym.manualBase);
                baseAddr = blk.sym.manualBase;
                break;
            case 2:
                snprintf(baseStr, sizeof(baseStr), "\"Array[%llu]\"", (unsigned long long)blk.sym.arrayIndex);
                baseAddr = blk.sym.start;
                break;
            default:
                snprintf(baseStr, sizeof(baseStr), "\"%s[%d]\"", blk.sym.name, blk.sym.segment);
                baseAddr = blk.sym.start;
                break;
            }

            for (const auto &root : blk.roots)
            {
                int64_t rootOff = (int64_t)root.address - (int64_t)baseAddr;
                char prefixBuf[512];
                if (rootOff >= 0)
                    snprintf(prefixBuf, sizeof(prefixBuf), "[%s + 0x%llX]", baseStr, (unsigned long long)rootOff);
                else
                    snprintf(prefixBuf, sizeof(prefixBuf), "[%s - 0x%llX]", baseStr, (unsigned long long)(-rootOff));

                currentBasePrefix = prefixBuf;
                offsetCount = 0;
                dfs(blk.sym.level - 1, root);
            }
        }

        fclose(fOut);
        std::println("导出完成: 成功向外输出了 {} 条链条！", chainCount);
    }
};
