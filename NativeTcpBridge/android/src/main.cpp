#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <print>
#include <string>
#include <string_view>
#include <vector>
#include <unordered_set>
#include <unordered_map>
#include <optional>
#include <sstream>
#include <format>
#include <iterator>
#include <utility>
#include <future>
#include <deque>
#include <thread>
#include <limits>
#include <variant>
#include <memory>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>

#include "DriverMemory.h"
#include "json.hpp"
#include "ThreadPool.h"
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

    enum class DataType : uint8_t
    {
        I8,
        I16,
        I32,
        I64,
        Float,
        Double,
        Count
    };
    enum class FuzzyMode : uint8_t
    {
        Unknown,
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
    enum class ViewFormat : uint8_t
    {
        Hex,
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

    struct MemNode
    {
        uintptr_t addr;
        uintptr_t value;
        auto operator<=>(const MemNode &) const = default;
    };

    // 编译期大小查表
    namespace Labels
    {
        inline constexpr std::array<const char *, static_cast<size_t>(DataType::Count)> TYPE = {
            "Int8",
            "Int16",
            "Int32",
            "Int64",
            "Float",
            "Double",
        };

        inline constexpr std::array<const char *, static_cast<size_t>(FuzzyMode::Count)> FUZZY = {
            "未知",
            "等于",
            "大于",
            "小于",
            "增加",
            "减少",
            "已变化",
            "未变化",
            "范围",
            "指针",
            "String",
        };

        inline constexpr std::array<const char *, static_cast<size_t>(ViewFormat::Count)> FORMAT = {
            "Hex",
            "Hex64",
            "Int8",
            "Int16",
            "Int32",
            "Int64",
            "Float",
            "Double",
            "Disasm",
        };
    } // namespace Labels

    namespace detail
    {
        constexpr std::array<size_t, 6> kDataSizes = {1, 2, 4, 8, 4, 8};
        constexpr std::array<size_t, 9> kViewSizes = {1, 8, 1, 2, 4, 8, 4, 8, 4};
    }

    // 根据数据类型返回对应字节数。
    constexpr size_t GetDataSize(DataType t) noexcept
    {
        auto i = std::to_underlying(t);
        return i < detail::kDataSizes.size() ? detail::kDataSizes[i] : 1;
    }
    // 根据浏览格式返回移动步长。
    constexpr size_t GetViewSize(ViewFormat f) noexcept
    {
        auto i = std::to_underlying(f);
        return i < detail::kViewSizes.size() ? detail::kViewSizes[i] : 1;
    }
}
// ============================================================================
// 内存工具
// ============================================================================
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

        // 辅助：获取 epsilon 和 double 转换值
        constexpr bool isFloat = std::is_floating_point_v<T>;
        constexpr double eps = isFloat ? Constants::FLOAT_EPSILON : 0.0;

        auto eq = [&](auto a, auto b)
        {
            if constexpr (isFloat)
                return std::abs(static_cast<double>(a) - static_cast<double>(b)) < eps;
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
                return static_cast<double>(value) >= lo - eps && static_cast<double>(value) <= hi + eps;
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
// RAII mmap 封装
// ============================================================================
class MappedFile
{
    int fd_ = -1;
    void *ptr_ = nullptr;
    size_t size_ = 0;

public:
    MappedFile() = default;
    ~MappedFile() { release(); }
    MappedFile(const MappedFile &) = delete;
    MappedFile &operator=(const MappedFile &) = delete;

    MappedFile(MappedFile &&o) noexcept : fd_(o.fd_), ptr_(o.ptr_), size_(o.size_)
    {
        o.fd_ = -1;
        o.ptr_ = nullptr;
        o.size_ = 0;
    }

    MappedFile &operator=(MappedFile &&o) noexcept
    {
        if (this != &o)
        {
            release();
            fd_ = o.fd_;
            ptr_ = o.ptr_;
            size_ = o.size_;
            o.fd_ = -1;
            o.ptr_ = nullptr;
            o.size_ = 0;
        }
        return *this;
    }

    // 申请并初始化底层内存映射。
    bool allocate(size_t sz)
    {
        release();
        char tpl[] = "/data/local/tmp/memscan_XXXXXX";
        fd_ = mkstemp(tpl);
        if (fd_ < 0)
            return false;
        unlink(tpl);
        if (ftruncate(fd_, static_cast<off_t>(sz)) != 0)
        {
            close(fd_);
            fd_ = -1;
            return false;
        }
        ptr_ = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (ptr_ == MAP_FAILED)
        {
            ptr_ = nullptr;
            close(fd_);
            fd_ = -1;
            return false;
        }
        size_ = sz;
        return true;
    }

    // 释放当前对象持有的底层资源。
    void release()
    {
        if (ptr_)
        {
            munmap(ptr_, size_);
            ptr_ = nullptr;
        }
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
        size_ = 0;
    }

    template <typename T = void>
    T *as() noexcept { return static_cast<T *>(ptr_); }

    template <typename T = void>
    const T *as() const noexcept { return static_cast<const T *>(ptr_); }

    // 返回当前映射区域的字节大小。
    size_t size() const noexcept { return size_; }
    // 判断当前映射指针是否有效。
    bool valid() const noexcept { return ptr_ != nullptr; }

    // 向内核提示映射区域的访问模式。
    void advise(int advice)
    {
        if (ptr_)
            madvise(ptr_, size_, advice);
    }
};

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
                // 安全限制：哪怕 buffer_ 特别大，最多只让 Capstone 一次解 500 条指令
                disasmCache_ = disasm_.Disassemble(base_, buffer_.data(), buffer_.size(), 500);
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
                // 修复：传入 Root 真实的所属级别（向下找）
                dfs(blk.sym.level - 1, root);
            }
        }

        fclose(fOut);
        std::println("导出完成: 成功向外输出了 {} 条链条！", chainCount);
    }
};

namespace
{
    using nlohmann::json;

    constexpr std::uint16_t kServerPort = 9494;
    constexpr int kListenBacklog = 4;
    std::atomic_bool gRunning{true};
    std::atomic_uint64_t gClientSessionSeed{1};
    int gServerFd = -1;
    LockManager gLockManager;
    std::mutex gDriverCommandMutex;

    struct ClientSession
    {
        std::uint64_t sessionId;
        MemScanner memScanner;
        MemViewer memViewer;
        PointerManager pointerManager;

        explicit ClientSession(std::uint64_t id)
            : sessionId(id) {}
    };

    // 打印系统错误信息
    void printErrno(std::string_view action)
    {
        std::println(stderr, "{}，错误码：{}", action, errno);
    }

    // 去除字符串末尾换行符
    void trimLineEnding(std::string &text)
    {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        {
            text.pop_back();
        }
    }

    // 清理文本中的换行字符
    std::string sanitizeLine(std::string text)
    {
        for (char &ch : text)
        {
            if (ch == '\n' || ch == '\r')
            {
                ch = ' ';
            }
        }
        return text;
    }

    // 按空白切分命令参数
    std::vector<std::string> splitTokens(const std::string &input)
    {
        std::istringstream iss(input);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    // 解析无符号64位整数
    std::optional<std::uint64_t> parseUInt64(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const unsigned long long value = std::strtoull(temp.c_str(), &end, 0);
        if (errno != 0 || end == temp.c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return static_cast<std::uint64_t>(value);
    }

    // 解析整数参数
    std::optional<int> parseInt(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const long value = std::strtol(temp.c_str(), &end, 0);
        if (errno != 0 || end == temp.c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return static_cast<int>(value);
    }

    // 解析浮点数参数
    std::optional<double> parseDouble(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const double value = std::strtod(temp.c_str(), &end);
        if (errno != 0 || end == temp.c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return value;
    }

    // 解析有符号64位整数
    std::optional<std::int64_t> parseInt64(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const long long value = std::strtoll(temp.c_str(), &end, 0);
        if (errno != 0 || end == temp.c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(value);
    }

    // 将字符串转换为小写ASCII
    std::string toLowerAscii(std::string_view input)
    {
        std::string out;
        out.reserve(input.size());
        for (const unsigned char ch : input)
        {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
        return out;
    }

    // 解析数据类型标记
    std::optional<Types::DataType> parseDataTypeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "i8" || t == "int8")
            return Types::DataType::I8;
        if (t == "i16" || t == "int16")
            return Types::DataType::I16;
        if (t == "i32" || t == "int32")
            return Types::DataType::I32;
        if (t == "i64" || t == "int64")
            return Types::DataType::I64;
        if (t == "f32" || t == "float")
            return Types::DataType::Float;
        if (t == "f64" || t == "double")
            return Types::DataType::Double;
        return std::nullopt;
    }

    // 解析扫描模式标记
    std::optional<Types::FuzzyMode> parseFuzzyModeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "unknown")
            return Types::FuzzyMode::Unknown;
        if (t == "eq" || t == "equal")
            return Types::FuzzyMode::Equal;
        if (t == "gt" || t == "greater")
            return Types::FuzzyMode::Greater;
        if (t == "lt" || t == "less")
            return Types::FuzzyMode::Less;
        if (t == "inc" || t == "increased")
            return Types::FuzzyMode::Increased;
        if (t == "dec" || t == "decreased")
            return Types::FuzzyMode::Decreased;
        if (t == "chg" || t == "changed")
            return Types::FuzzyMode::Changed;
        if (t == "unchg" || t == "unchanged")
            return Types::FuzzyMode::Unchanged;
        if (t == "range")
            return Types::FuzzyMode::Range;
        if (t == "ptr" || t == "pointer")
            return Types::FuzzyMode::Pointer;
        if (t == "str" || t == "string")
            return Types::FuzzyMode::String;
        return std::nullopt;
    }

    // 解析内存浏览显示格式
    std::optional<Types::ViewFormat> parseViewFormatToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "hex")
            return Types::ViewFormat::Hex;
        if (t == "hex64")
            return Types::ViewFormat::Hex64;
        if (t == "i8" || t == "int8")
            return Types::ViewFormat::I8;
        if (t == "i16" || t == "int16")
            return Types::ViewFormat::I16;
        if (t == "i32" || t == "int32")
            return Types::ViewFormat::I32;
        if (t == "i64" || t == "int64")
            return Types::ViewFormat::I64;
        if (t == "f32" || t == "float")
            return Types::ViewFormat::Float;
        if (t == "f64" || t == "double")
            return Types::ViewFormat::Double;
        if (t == "disasm")
            return Types::ViewFormat::Disasm;
        return std::nullopt;
    }

    // 解析硬件断点类型
    std::optional<decltype(dr)::bp_type> parseBpTypeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "0" || t == "read" || t == "r" || t == "bp_read")
            return decltype(dr)::BP_READ;
        if (t == "1" || t == "write" || t == "w" || t == "bp_write")
            return decltype(dr)::BP_WRITE;
        if (t == "2" || t == "read_write" || t == "rw" || t == "bp_read_write")
            return decltype(dr)::BP_READ_WRITE;
        if (t == "3" || t == "execute" || t == "x" || t == "exec" || t == "bp_execute")
            return decltype(dr)::BP_EXECUTE;
        return std::nullopt;
    }

    // 解析硬件断点作用线程范围
    std::optional<decltype(dr)::bp_scope> parseBpScopeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "0" || t == "main" || t == "main_thread")
            return decltype(dr)::SCOPE_MAIN_THREAD;
        if (t == "1" || t == "other" || t == "other_threads")
            return decltype(dr)::SCOPE_OTHER_THREADS;
        if (t == "2" || t == "all" || t == "all_threads")
            return decltype(dr)::SCOPE_ALL_THREADS;
        return std::nullopt;
    }

    // 将显示格式枚举转换为标记
    std::string_view viewFormatToToken(Types::ViewFormat format)
    {
        switch (format)
        {
        case Types::ViewFormat::Hex:
            return "hex";
        case Types::ViewFormat::Hex64:
            return "hex64";
        case Types::ViewFormat::I8:
            return "i8";
        case Types::ViewFormat::I16:
            return "i16";
        case Types::ViewFormat::I32:
            return "i32";
        case Types::ViewFormat::I64:
            return "i64";
        case Types::ViewFormat::Float:
            return "f32";
        case Types::ViewFormat::Double:
            return "f64";
        case Types::ViewFormat::Disasm:
            return "disasm";
        default:
            return "hex";
        }
    }

    // 将硬件断点类型转换为文本标记
    std::string_view bpTypeToToken(decltype(dr)::bp_type type)
    {
        switch (type)
        {
        case decltype(dr)::BP_READ:
            return "read";
        case decltype(dr)::BP_WRITE:
            return "write";
        case decltype(dr)::BP_READ_WRITE:
            return "read_write";
        case decltype(dr)::BP_EXECUTE:
            return "execute";
        default:
            return "unknown";
        }
    }

    // 将硬件断点线程范围转换为文本标记
    std::string_view bpScopeToToken(decltype(dr)::bp_scope scope)
    {
        switch (scope)
        {
        case decltype(dr)::SCOPE_MAIN_THREAD:
            return "main";
        case decltype(dr)::SCOPE_OTHER_THREADS:
            return "other";
        case decltype(dr)::SCOPE_ALL_THREADS:
            return "all";
        default:
            return "unknown";
        }
    }

    // 按字段名写入硬件断点记录中的寄存器或元数据。
    bool assignHwbpRecordField(Driver::hwbp_record &record, std::string_view fieldToken, std::uint64_t value)
    {
        const std::string token = toLowerAscii(fieldToken);
        if (token == "pc")
        {
            record.pc = value;
            return true;
        }
        if (token == "lr")
        {
            record.lr = value;
            return true;
        }
        if (token == "sp")
        {
            record.sp = value;
            return true;
        }
        if (token == "pstate")
        {
            record.pstate = value;
            return true;
        }
        if (token == "orig_x0")
        {
            record.orig_x0 = value;
            return true;
        }
        if (token == "syscallno")
        {
            record.syscallno = value;
            return true;
        }
        if (token == "fpsr")
        {
            record.fpsr = static_cast<std::uint32_t>(value);
            return true;
        }
        if (token == "fpcr")
        {
            record.fpcr = static_cast<std::uint32_t>(value);
            return true;
        }
        if (token == "rw")
        {
            record.rw = value != 0;
            return true;
        }
        if (token.size() >= 2 && token[0] == 'x')
        {
            const auto regIndex = parseInt(token.substr(1));
            if (regIndex.has_value() && *regIndex >= 0 && *regIndex < 30)
            {
                record.regs[*regIndex] = value;
                return true;
            }
        }
        if (token.size() >= 2 && token[0] == 'v')
        {
            auto regIndex = parseInt(token.substr(1));
            if (regIndex.has_value() && *regIndex >= 0 && *regIndex < 32)
            {
                record.vregs[*regIndex] = static_cast<__uint128_t>(value);
                return true;
            }
        }
        return false;
    }

    // 按模板类型解析扫描输入值。
    template <typename T>
    std::optional<T> parseScanValueToken(std::string_view token)
    {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
        {
            const auto parsed = parseDouble(token);
            if (!parsed.has_value())
            {
                return std::nullopt;
            }
            return static_cast<T>(*parsed);
        }
        else
        {
            const auto parsed = parseInt64(token);
            if (!parsed.has_value())
            {
                return std::nullopt;
            }
            return static_cast<T>(*parsed);
        }
    }

    // 将字节数组编码为十六进制字符串
    std::string bytesToHex(const std::uint8_t *bytes, std::size_t count)
    {
        std::string output;
        output.reserve(count * 2);
        for (std::size_t i = 0; i < count; ++i)
        {
            std::format_to(std::back_inserter(output), "{:02X}", bytes[i]);
        }
        return output;
    }

    // 解析十六进制字节流
    std::optional<std::vector<std::uint8_t>> parseHexBytes(std::string_view text)
    {
        std::string compact;
        compact.reserve(text.size());

        for (char ch : text)
        {
            if (std::isxdigit(static_cast<unsigned char>(ch)) != 0)
            {
                compact.push_back(ch);
            }
        }

        if (compact.empty() || (compact.size() % 2) != 0)
        {
            return std::nullopt;
        }

        std::vector<std::uint8_t> bytes;
        bytes.reserve(compact.size() / 2);

        for (std::size_t i = 0; i < compact.size(); i += 2)
        {
            const std::string hexPair = compact.substr(i, 2);
            char *end = nullptr;
            errno = 0;
            const unsigned long value = std::strtoul(hexPair.c_str(), &end, 16);
            if (errno != 0 || end == hexPair.c_str() || *end != '\0' || value > 0xFF)
            {
                return std::nullopt;
            }
            bytes.push_back(static_cast<std::uint8_t>(value));
        }

        return bytes;
    }

    // 合并指定起点后的参数为字符串
    std::string joinTokens(const std::vector<std::string> &tokens, std::size_t start)
    {
        if (start >= tokens.size())
        {
            return "";
        }

        std::string text = tokens[start];
        for (std::size_t i = start + 1; i < tokens.size(); ++i)
        {
            text.append(" ");
            text.append(tokens[i]);
        }
        return text;
    }

    // 生成成功响应文本
    std::string ok(std::string_view message)
    {
        return std::format("ok {}", message);
    }

    // 生成失败响应文本
    std::string err(std::string_view message)
    {
        return std::format("err {}", message);
    }

    // 构建内存信息JSON响应
    json buildMemoryInfoJson(int status, const auto &info)
    {
        json root;
        int moduleCount = info.module_count;
        if (moduleCount < 0)
        {
            moduleCount = 0;
        }
        else if (moduleCount > MAX_MODULES)
        {
            moduleCount = MAX_MODULES;
        }

        int regionCount = info.region_count;
        if (regionCount < 0)
        {
            regionCount = 0;
        }
        else if (regionCount > MAX_SCAN_REGIONS)
        {
            regionCount = MAX_SCAN_REGIONS;
        }

        root["status"] = status;
        root["module_count"] = moduleCount;
        root["region_count"] = regionCount;
        root["modules"] = json::array();
        root["regions"] = json::array();

        for (int i = 0; i < moduleCount; ++i)
        {
            const auto &mod = info.modules[i];
            int segCount = mod.seg_count;
            if (segCount < 0)
            {
                segCount = 0;
            }
            else if (segCount > MAX_SEGS_PER_MODULE)
            {
                segCount = MAX_SEGS_PER_MODULE;
            }

            json moduleItem;
            moduleItem["name"] = std::string(mod.name);
            moduleItem["seg_count"] = segCount;
            moduleItem["segs"] = json::array();

            for (int j = 0; j < segCount; ++j)
            {
                const auto &seg = mod.segs[j];
                moduleItem["segs"].push_back({
                    {"index", seg.index},
                    {"prot", static_cast<int>(seg.prot)},
                    {"start", seg.start},
                    {"end", seg.end},
                });
            }

            root["modules"].push_back(moduleItem);
        }

        for (int i = 0; i < regionCount; ++i)
        {
            const auto &region = info.regions[i];
            root["regions"].push_back({
                {"start", region.start},
                {"end", region.end},
            });
        }

        return root;
    }

    // 构建内存浏览快照JSON
    json buildViewerSnapshotJson(const MemViewer &viewer)
    {
        json root;
        root["visible"] = viewer.isVisible();
        root["read_success"] = viewer.readSuccess();
        root["base"] = static_cast<std::uint64_t>(viewer.base());
        root["base_hex"] = std::format("0x{:X}", static_cast<std::uint64_t>(viewer.base()));
        root["format"] = viewFormatToToken(viewer.format());

        const auto &buffer = viewer.buffer();
        root["byte_count"] = buffer.size();
        root["data_hex"] = bytesToHex(buffer.data(), buffer.size());

        root["disasm_scroll_idx"] = viewer.disasmScrollIdx();
        root["disasm"] = json::array();
        for (const auto &line : viewer.getDisasm())
        {
            json item;
            item["valid"] = line.valid;
            item["address"] = line.address;
            item["address_hex"] = std::format("0x{:X}", line.address);
            item["size"] = line.size;
            item["bytes_hex"] = bytesToHex(line.bytes, line.size);
            item["mnemonic"] = sanitizeLine(line.mnemonic);
            item["op_str"] = sanitizeLine(line.op_str);
            root["disasm"].push_back(std::move(item));
        }

        return root;
    }

    // 构建硬件断点信息JSON
    json buildHwbpInfoJson(const auto &info)
    {
        json root;
        int recordCount = info.record_count;
        if (recordCount < 0)
        {
            recordCount = 0;
        }
        else if (recordCount > 0x100)
        {
            recordCount = 0x100;
        }

        root["num_brps"] = info.num_brps;
        root["num_wrps"] = info.num_wrps;
        root["hit_addr"] = info.hit_addr;
        root["hit_addr_hex"] = std::format("0x{:X}", static_cast<std::uint64_t>(info.hit_addr));
        root["record_count"] = recordCount;
        root["records"] = json::array();

        for (int i = 0; i < recordCount; ++i)
        {
            const auto &rec = info.records[i];
            json item;
            item["index"] = i;
            item["rw"] = rec.rw ? "write" : "read";
            item["pc"] = rec.pc;
            item["pc_hex"] = std::format("0x{:X}", static_cast<std::uint64_t>(rec.pc));
            item["hit_count"] = rec.hit_count;
            item["lr"] = rec.lr;
            item["sp"] = rec.sp;
            item["orig_x0"] = rec.orig_x0;
            item["syscallno"] = rec.syscallno;
            item["pstate"] = rec.pstate;
            item["regs"] = json::array();
            for (const auto reg : rec.regs)
            {
                item["regs"].push_back(reg);
            }
            item["vregs"] = json::array();
            for (const auto &vreg : rec.vregs)
            {
                item["vregs"].push_back({
                    {"lo", static_cast<std::uint64_t>(vreg)},
                    {"hi", static_cast<std::uint64_t>(vreg >> 64)}
                });
            }
            item["fpsr"] = rec.fpsr;
            item["fpcr"] = rec.fpcr;
            root["records"].push_back(std::move(item));
        }

        return root;
    }

    // 构建特征码扫描结果JSON
    json buildSignatureMatchesJson(const std::vector<uintptr_t> &matches, std::int64_t range, std::string_view pattern)
    {
        constexpr std::size_t kMaxReturnedMatches = 4096;
        const std::size_t returnedCount = std::min(matches.size(), kMaxReturnedMatches);

        json root;
        root["count"] = matches.size();
        root["returned_count"] = returnedCount;
        root["truncated"] = (matches.size() > returnedCount);
        root["range"] = range;
        root["pattern"] = std::string(pattern);
        root["matches"] = json::array();

        for (std::size_t i = 0; i < returnedCount; ++i)
        {
            const auto addr = static_cast<std::uint64_t>(matches[i]);
            root["matches"].push_back({
                {"addr", addr},
                {"addr_hex", std::format("0x{:X}", addr)},
            });
        }

        return root;
    }

    // 发送完整响应数据
    bool sendAll(int fd, std::string_view data)
    {
        std::size_t sentTotal = 0;
        while (sentTotal < data.size())
        {
            const ssize_t sent = send(fd, data.data() + sentTotal, data.size() - sentTotal, 0);
            if (sent < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return false;
            }

            if (sent == 0)
            {
                return false;
            }
            sentTotal += static_cast<std::size_t>(sent);
        }
        return true;
    }

    // 内部文本命令派发
    std::string DispatchTextCommand(const std::shared_ptr<ClientSession> &session, const std::string &request)
    {
        std::lock_guard<std::mutex> driverLock(gDriverCommandMutex);
        const auto tokens = splitTokens(request);
        if (tokens.empty())
        {
            return ok("收到");
        }

        const std::string &command = tokens[0];

        if (command == "help")
        {
            return ok("支持命令: ping, pid.get, pid.set, pid.current, pid.attach, hwbp.info, hwbp.set, hwbp.remove, hwbp.record.remove, hwbp.record.set, sig.scan.addr, sig.filter, sig.scan.pattern, sig.scan.file, lock.toggle, lock.set, lock.unset, lock.status, lock.clear, scan.status, scan.clear, scan.first, scan.next, scan.page, scan.add, scan.remove, scan.offset, viewer.open, viewer.move, viewer.offset, viewer.format, viewer.get, pointer.status, pointer.scan, pointer.scan.manual, pointer.scan.array, pointer.merge, pointer.export, mem.read, mem.write, mem.read_u8/u16/u32/u64/f32/f64, mem.write_u8/u16/u32/u64/f32/f64, mem.read_str, mem.read_wstr, memory.refresh, memory.summary, memory.info.full, module.addr, touch.down, touch.move, touch.up");
        }

        if (command == "ping")
        {
            return ok("pong");
        }

        if (command == "pid.get")
        {
            if (tokens.size() < 2)
            {
                return err("用法: pid.get <包名>");
            }

            const std::string packageName = joinTokens(tokens, 1);
            const int pid = dr.GetPid(packageName);
            if (pid <= 0)
            {
                return err("未找到进程");
            }
            return ok(std::format("pid={}", pid));
        }

        if (command == "pid.set")
        {
            if (tokens.size() != 2)
            {
                return err("用法: pid.set <pid>");
            }

            const auto pid = parseInt(tokens[1]);
            if (!pid.has_value() || *pid <= 0)
            {
                return err("pid 参数无效");
            }

            dr.SetGlobalPid(*pid);
            return ok(std::format("pid={}", dr.GetGlobalPid()));
        }

        if (command == "pid.current")
        {
            return ok(std::format("pid={}", dr.GetGlobalPid()));
        }

        if (command == "pid.attach")
        {
            if (tokens.size() < 2)
            {
                return err("用法: pid.attach <包名>");
            }

            const std::string packageName = joinTokens(tokens, 1);
            const int pid = dr.GetPid(packageName);
            if (pid <= 0)
            {
                return err("未找到进程");
            }

            dr.SetGlobalPid(pid);
            return ok(std::format("pid={}", pid));
        }

        if (command == "hwbp.info")
        {
            const auto &info = dr.GetHwbpInfoRef();
            const std::string jsonText = buildHwbpInfoJson(info).dump();
            return std::format("ok hwbp.info size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "hwbp.set")
        {
            if (tokens.size() != 5)
            {
                return err("用法: hwbp.set <地址> <类型(0-3)> <范围(0-2)> <长度>");
            }

            const int pid = dr.GetGlobalPid();
            if (pid <= 0)
            {
                return err("全局PID未设置，请先执行 pid.set 或 pid.attach");
            }

            const auto targetAddr = parseUInt64(tokens[1]);
            const auto bpType = parseBpTypeToken(tokens[2]);
            const auto bpScope = parseBpScopeToken(tokens[3]);
            const auto lenBytes = parseInt(tokens[4]);
            if (!targetAddr.has_value() || *targetAddr == 0 || !bpType.has_value() || !bpScope.has_value() || !lenBytes.has_value())
            {
                return err("参数无效");
            }

            if (*lenBytes <= 0 || *lenBytes > 8)
            {
                return err("长度范围为 1-8");
            }

            const int status = dr.SetProcessHwbpRef(*targetAddr, *bpType, *bpScope, *lenBytes);
            if (status != 0)
            {
                return err(std::format("设置断点失败 status={}", status));
            }

            return ok(std::format("status=0 type={} scope={} len={}", bpTypeToToken(*bpType), bpScopeToToken(*bpScope), *lenBytes));
        }

        if (command == "hwbp.remove")
        {
            dr.RemoveProcessHwbpRef();
            return ok("done=1");
        }

        if (command == "hwbp.record.remove")
        {
            if (tokens.size() != 2)
            {
                return err("用法: hwbp.record.remove <索引>");
            }

            const auto index = parseInt(tokens[1]);
            if (!index.has_value() || *index < 0)
            {
                return err("索引无效");
            }

            (void)dr.GetHwbpInfoRef();
            dr.RemoveHwbpRecord(*index);
            const auto &updated = dr.GetHwbpInfoRef();
            return ok(std::format("record_count={}", updated.record_count));
        }

        if (command == "hwbp.record.set")
        {
            if (tokens.size() != 4)
            {
                return err("用法: hwbp.record.set <索引> <字段> <值>");
            }

            const auto index = parseInt(tokens[1]);
            const auto value = parseUInt64(tokens[3]);
            if (!index.has_value() || *index < 0 || !value.has_value())
            {
                return err("索引或值无效");
            }

            const auto &info = dr.GetHwbpInfoRef();
            if (*index >= info.record_count)
            {
                return err("索引越界");
            }

            auto copy = info.records[*index];
            if (!assignHwbpRecordField(copy, tokens[2], *value))
            {
                return err("字段无效，支持: pc/lr/sp/pstate/orig_x0/syscallno/fpsr/fpcr/rw/x0~x29/v0~v31");
            }

            copy.rw = true;
            const_cast<Driver::hwbp_record &>(dr.GetHwbpInfoRef().records[*index]) = copy;
            return ok(std::format("index={} field={} value=0x{:X}", *index, tokens[2], *value));
        }

        if (command == "hwbp.record.set.f32")
        {
            if (tokens.size() != 4)
            {
                return err("用法: hwbp.record.set.f32 <索引> <v0~v31> <浮点值>");
            }

            const auto index = parseInt(tokens[1]);
            if (!index.has_value() || *index < 0)
            {
                return err("索引无效");
            }

            const std::string field = toLowerAscii(tokens[2]);
            if (field.size() < 2 || field[0] != 'v')
            {
                return err("字段必须是 v0~v31");
            }

            auto regIndex = parseInt(field.substr(1));
            if (!regIndex.has_value() || *regIndex < 0 || *regIndex >= 32)
            {
                return err("字段必须是 v0~v31");
            }

            float fval = 0.0f;
            try
            {
                size_t pos = 0;
                fval = std::stof(std::string(tokens[3]), &pos);
            }
            catch (...)
            {
                return err("浮点值无效");
            }

            const auto &info = dr.GetHwbpInfoRef();
            if (*index >= info.record_count)
            {
                return err("索引越界");
            }

            auto copy = info.records[*index];
            uint32_t bits;
            std::memcpy(&bits, &fval, sizeof(bits));
            copy.vregs[*regIndex] = static_cast<__uint128_t>(bits);
            copy.rw = true;
            const_cast<Driver::hwbp_record &>(dr.GetHwbpInfoRef().records[*index]) = copy;
            return ok(std::format("index={} v{}={} (0x{:08X})", *index, *regIndex, fval, bits));
        }

        if (command == "hwbp.record.set.f64")
        {
            if (tokens.size() != 4)
            {
                return err("用法: hwbp.record.set.f64 <索引> <v0~v31> <浮点值>");
            }

            const auto index = parseInt(tokens[1]);
            if (!index.has_value() || *index < 0)
            {
                return err("索引无效");
            }

            const std::string field = toLowerAscii(tokens[2]);
            if (field.size() < 2 || field[0] != 'v')
            {
                return err("字段必须是 v0~v31");
            }

            auto regIndex = parseInt(field.substr(1));
            if (!regIndex.has_value() || *regIndex < 0 || *regIndex >= 32)
            {
                return err("字段必须是 v0~v31");
            }

            double fval = 0.0;
            try
            {
                size_t pos = 0;
                fval = std::stod(std::string(tokens[3]), &pos);
            }
            catch (...)
            {
                return err("浮点值无效");
            }

            const auto &info = dr.GetHwbpInfoRef();
            if (*index >= info.record_count)
            {
                return err("索引越界");
            }

            auto copy = info.records[*index];
            uint64_t bits;
            std::memcpy(&bits, &fval, sizeof(bits));
            copy.vregs[*regIndex] = static_cast<__uint128_t>(bits);
            copy.rw = true;
            const_cast<Driver::hwbp_record &>(dr.GetHwbpInfoRef().records[*index]) = copy;
            return ok(std::format("index={} v{}={} (0x{:016X})", *index, *regIndex, fval, bits));
        }

        if (command == "sig.scan.addr")
        {
            if (tokens.size() < 3)
            {
                return err("用法: sig.scan.addr <地址> <范围> [文件名]");
            }

            const auto addr = parseUInt64(tokens[1]);
            const auto range = parseInt(tokens[2]);
            if (!addr.has_value() || *addr == 0 || !range.has_value())
            {
                return err("地址或范围无效");
            }

            const std::string fileName = (tokens.size() >= 4) ? joinTokens(tokens, 3) : std::string(SignatureScanner::SIG_DEFAULT_FILE);
            const bool success = SignatureScanner::ScanAddressSignature(static_cast<uintptr_t>(*addr), *range, fileName.c_str());
            if (!success)
            {
                return err("特征码保存失败");
            }

            return ok(std::format("saved=1 file={}", fileName));
        }

        if (command == "sig.filter")
        {
            if (tokens.size() < 2)
            {
                return err("用法: sig.filter <地址> [文件名]");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            const std::string fileName = (tokens.size() >= 3) ? joinTokens(tokens, 2) : std::string(SignatureScanner::SIG_DEFAULT_FILE);
            const auto result = SignatureScanner::FilterSignature(static_cast<uintptr_t>(*addr), fileName.c_str());

            json payload;
            payload["success"] = result.success;
            payload["changed_count"] = result.changedCount;
            payload["total_count"] = result.totalCount;
            payload["old_signature"] = result.oldSignature;
            payload["new_signature"] = result.newSignature;
            payload["file"] = fileName;

            const std::string jsonText = payload.dump();
            return std::format("ok sig.filter size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "sig.scan.pattern")
        {
            if (tokens.size() < 3)
            {
                return err("用法: sig.scan.pattern <范围偏移> <特征码...>");
            }

            const auto range = parseInt64(tokens[1]);
            if (!range.has_value() || *range < static_cast<std::int64_t>(std::numeric_limits<int>::min()) || *range > static_cast<std::int64_t>(std::numeric_limits<int>::max()))
            {
                return err("范围偏移无效");
            }

            const std::string pattern = joinTokens(tokens, 2);
            if (pattern.empty())
            {
                return err("特征码不能为空");
            }

            const auto matches = SignatureScanner::ScanSignature(pattern.c_str(), static_cast<int>(*range));
            const std::string jsonText = buildSignatureMatchesJson(matches, *range, pattern).dump();
            return std::format("ok sig.scan.pattern size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "sig.scan.file")
        {
            const std::string fileName = (tokens.size() >= 2) ? joinTokens(tokens, 1) : std::string(SignatureScanner::SIG_DEFAULT_FILE);
            const auto matches = SignatureScanner::ScanSignatureFromFile(fileName.c_str());
            json payload = buildSignatureMatchesJson(matches, 0, "");
            payload["file"] = fileName;
            const std::string jsonText = payload.dump();
            return std::format("ok sig.scan.file size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "lock.toggle")
        {
            if (tokens.size() != 3)
            {
                return err("用法: lock.toggle <地址> <type>");
            }

            const auto addr = parseUInt64(tokens[1]);
            const auto dataType = parseDataTypeToken(tokens[2]);
            if (!addr.has_value() || *addr == 0 || !dataType.has_value())
            {
                return err("地址或type无效");
            }

            gLockManager.toggle(static_cast<uintptr_t>(*addr), *dataType);
            return ok(std::format("locked={}", gLockManager.isLocked(static_cast<uintptr_t>(*addr)) ? 1 : 0));
        }

        if (command == "lock.set")
        {
            if (tokens.size() < 4)
            {
                return err("用法: lock.set <地址> <type> <value>");
            }

            const auto addr = parseUInt64(tokens[1]);
            const auto dataType = parseDataTypeToken(tokens[2]);
            if (!addr.has_value() || *addr == 0 || !dataType.has_value())
            {
                return err("地址或type无效");
            }

            const std::string value = joinTokens(tokens, 3);
            gLockManager.lock(static_cast<uintptr_t>(*addr), *dataType, value);
            return ok(std::format("locked={}", gLockManager.isLocked(static_cast<uintptr_t>(*addr)) ? 1 : 0));
        }

        if (command == "lock.unset")
        {
            if (tokens.size() != 2)
            {
                return err("用法: lock.unset <地址>");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            gLockManager.unlock(static_cast<uintptr_t>(*addr));
            return ok("locked=0");
        }

        if (command == "lock.status")
        {
            if (tokens.size() != 2)
            {
                return err("用法: lock.status <地址>");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            return ok(std::format("locked={}", gLockManager.isLocked(static_cast<uintptr_t>(*addr)) ? 1 : 0));
        }

        if (command == "lock.clear")
        {
            gLockManager.clear();
            return ok("已清空所有锁定");
        }

        if (command == "scan.status")
        {
            return ok(std::format("scanning={} progress={:.4f} count={}",
                                  session->memScanner.isScanning() ? 1 : 0,
                                  session->memScanner.progress(),
                                  session->memScanner.count()));
        }

        if (command == "scan.clear")
        {
            session->memScanner.clear();
            return ok("已清空扫描结果");
        }

        if (command == "scan.add")
        {
            if (tokens.size() != 2)
            {
                return err("用法: scan.add <地址>");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            session->memScanner.add(static_cast<uintptr_t>(*addr));
            return ok(std::format("count={}", session->memScanner.count()));
        }

        if (command == "scan.remove")
        {
            if (tokens.size() != 2)
            {
                return err("用法: scan.remove <地址>");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            session->memScanner.remove(static_cast<uintptr_t>(*addr));
            return ok(std::format("count={}", session->memScanner.count()));
        }

        if (command == "scan.offset")
        {
            if (tokens.size() != 2)
            {
                return err("用法: scan.offset <有符号偏移>");
            }

            const auto offset = parseInt64(tokens[1]);
            if (!offset.has_value())
            {
                return err("偏移参数无效");
            }

            session->memScanner.applyOffset(*offset);
            return ok(std::format("count={}", session->memScanner.count()));
        }

        if (command == "scan.first" || command == "scan.next")
        {
            if (tokens.size() < 3)
            {
                return err("用法: scan.first/scan.next <type> <mode> [value] [rangeMax]");
            }

            const auto dataType = parseDataTypeToken(tokens[1]);
            if (!dataType.has_value())
            {
                return err("type 无效，支持: i8/i16/i32/i64/f32/f64");
            }

            const auto fuzzyMode = parseFuzzyModeToken(tokens[2]);
            if (!fuzzyMode.has_value())
            {
                return err("mode 无效，支持: unknown/eq/gt/lt/inc/dec/changed/unchanged/range/pointer/string");
            }

            const bool isFirst = (command == "scan.first");
            const int pid = dr.GetGlobalPid();
            if (pid <= 0)
            {
                return err("全局PID未设置，请先执行 pid.set 或 pid.attach");
            }

            if (*fuzzyMode == Types::FuzzyMode::String)
            {
                if (tokens.size() < 4)
                {
                    return err("string 模式需要 value 参数");
                }
                const std::string needle = joinTokens(tokens, 3);
                session->memScanner.scanString(pid, needle, isFirst);
                return ok(std::format("count={} progress={:.4f} scanning={}",
                                      session->memScanner.count(),
                                      session->memScanner.progress(),
                                      session->memScanner.isScanning() ? 1 : 0));
            }

            const bool needValue = (*fuzzyMode != Types::FuzzyMode::Unknown);
            if (needValue && tokens.size() < 4)
            {
                return err("当前模式需要 value 参数");
            }

            double rangeMax = 0.0;
            if (*fuzzyMode == Types::FuzzyMode::Range)
            {
                if (tokens.size() < 5)
                {
                    return err("range 模式需要 rangeMax 参数");
                }
                const auto parsedRange = parseDouble(tokens[4]);
                if (!parsedRange.has_value() || *parsedRange < 0.0)
                {
                    return err("rangeMax 无效");
                }
                rangeMax = *parsedRange;
            }
            else if (tokens.size() >= 5)
            {
                const auto parsedRange = parseDouble(tokens[4]);
                if (parsedRange.has_value() && *parsedRange >= 0.0)
                {
                    rangeMax = *parsedRange;
                }
            }

            const std::string valueToken = (needValue ? tokens[3] : "0");

            const auto result = MemUtils::DispatchType(*dataType, [&]<typename T>() -> std::string
                                                       {
                T target{};
                if (needValue)
                {
                    const auto parsedValue = parseScanValueToken<T>(valueToken);
                    if (!parsedValue.has_value())
                    {
                        return err("value 参数无效");
                    }
                    target = *parsedValue;
                }

                session->memScanner.scan<T>(pid, target, *fuzzyMode, isFirst, rangeMax);
                return ok(std::format("count={} progress={:.4f} scanning={}",
                                      session->memScanner.count(),
                                      session->memScanner.progress(),
                                      session->memScanner.isScanning() ? 1 : 0)); });

            return result;
        }

        if (command == "scan.page")
        {
            if (tokens.size() != 4)
            {
                return err("用法: scan.page <start> <count> <type>");
            }

            const auto start = parseUInt64(tokens[1]);
            const auto count = parseUInt64(tokens[2]);
            const std::string typeToken = toLowerAscii(tokens[3]);
            const bool stringType = (typeToken == "str" || typeToken == "string" || typeToken == "text");
            const auto dataType = parseDataTypeToken(tokens[3]);
            if (!start.has_value() || !count.has_value() || (!stringType && !dataType.has_value()))
            {
                return err("参数无效");
            }

            if (*count == 0 || *count > 2000)
            {
                return err("count 范围 1-2000");
            }

            const auto page = session->memScanner.getPage(static_cast<size_t>(*start), static_cast<size_t>(*count));
            json payload;
            payload["start"] = *start;
            payload["request_count"] = *count;
            payload["result_count"] = page.size();
            payload["total_count"] = session->memScanner.count();
            payload["type"] = tokens[3];
            payload["items"] = json::array();

            for (const auto addr : page)
            {
                payload["items"].push_back({
                    {"addr", static_cast<std::uint64_t>(addr)},
                    {"addr_hex", std::format("0x{:X}", static_cast<std::uint64_t>(addr))},
                    {"value", stringType ? MemUtils::ReadAsText(addr) : MemUtils::ReadAsString(addr, *dataType)},
                });
            }

            const std::string jsonText = payload.dump();
            return std::format("ok scan.page size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "viewer.open")
        {
            if (tokens.size() < 2 || tokens.size() > 3)
            {
                return err("用法: viewer.open <地址> [format]");
            }

            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
            {
                return err("地址无效");
            }

            if (tokens.size() == 3)
            {
                const auto format = parseViewFormatToken(tokens[2]);
                if (!format.has_value())
                {
                    return err("format 无效，支持: hex/hex64/i8/i16/i32/i64/f32/f64/disasm");
                }
                session->memViewer.setFormat(*format);
            }

            session->memViewer.open(static_cast<uintptr_t>(*address));
            return ok(std::format("base=0x{:X} format={} read={}",
                                  static_cast<std::uint64_t>(session->memViewer.base()),
                                  viewFormatToToken(session->memViewer.format()),
                                  session->memViewer.readSuccess() ? 1 : 0));
        }

        if (command == "viewer.move")
        {
            if (tokens.size() < 2 || tokens.size() > 3)
            {
                return err("用法: viewer.move <行数> [步长]");
            }

            const auto lines = parseInt(tokens[1]);
            if (!lines.has_value())
            {
                return err("行数参数无效");
            }

            std::size_t step = Types::GetViewSize(session->memViewer.format());
            if (step == 0)
            {
                step = 1;
            }
            if (tokens.size() == 3)
            {
                const auto parsedStep = parseUInt64(tokens[2]);
                if (!parsedStep.has_value() || *parsedStep == 0)
                {
                    return err("步长参数无效");
                }
                step = static_cast<std::size_t>(*parsedStep);
            }

            session->memViewer.move(*lines, step);
            return ok(std::format("base=0x{:X} read={}",
                                  static_cast<std::uint64_t>(session->memViewer.base()),
                                  session->memViewer.readSuccess() ? 1 : 0));
        }

        if (command == "viewer.offset")
        {
            if (tokens.size() != 2)
            {
                return err("用法: viewer.offset <偏移，如 +0x20/-0x10>");
            }

            if (!session->memViewer.applyOffset(tokens[1]))
            {
                return err("偏移参数无效");
            }

            return ok(std::format("base=0x{:X} read={}",
                                  static_cast<std::uint64_t>(session->memViewer.base()),
                                  session->memViewer.readSuccess() ? 1 : 0));
        }

        if (command == "viewer.format")
        {
            if (tokens.size() != 2)
            {
                return err("用法: viewer.format <format>");
            }

            const auto format = parseViewFormatToken(tokens[1]);
            if (!format.has_value())
            {
                return err("format 无效，支持: hex/hex64/i8/i16/i32/i64/f32/f64/disasm");
            }

            session->memViewer.setFormat(*format);
            return ok(std::format("format={}", viewFormatToToken(session->memViewer.format())));
        }

        if (command == "viewer.get")
        {
            const std::string jsonText = buildViewerSnapshotJson(session->memViewer).dump();
            return std::format("ok viewer.get size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "pointer.status")
        {
            return ok(std::format("scanning={} progress={:.4f} count={}",
                                  session->pointerManager.isScanning() ? 1 : 0,
                                  session->pointerManager.scanProgress(),
                                  session->pointerManager.count()));
        }

        if (command == "pointer.scan" || command == "pointer.scan.manual" || command == "pointer.scan.array")
        {
            const bool useManual = (command == "pointer.scan.manual");
            const bool useArray = (command == "pointer.scan.array");

            const std::size_t minTokenCount = useManual || useArray ? 6 : 4;
            if (tokens.size() < minTokenCount)
            {
                if (useManual)
                    return err("用法: pointer.scan.manual <target> <depth> <maxOffset> <manualBase> <manualMaxOffset> [模块过滤]");
                if (useArray)
                    return err("用法: pointer.scan.array <target> <depth> <maxOffset> <arrayBase> <arrayCount> [模块过滤]");
                return err("用法: pointer.scan <target> <depth> <maxOffset> [模块过滤]");
            }

            const int pid = dr.GetGlobalPid();
            if (pid <= 0)
            {
                return err("全局PID未设置，请先执行 pid.set 或 pid.attach");
            }

            const auto target = parseUInt64(tokens[1]);
            const auto depth = parseInt(tokens[2]);
            const auto maxOffset = parseInt(tokens[3]);
            if (!target.has_value() || !depth.has_value() || !maxOffset.has_value())
            {
                return err("target/depth/maxOffset 参数无效");
            }
            if (*depth <= 0 || *depth > 16)
            {
                return err("depth 范围为 1-16");
            }
            if (*maxOffset <= 0)
            {
                return err("maxOffset 必须大于 0");
            }

            std::uint64_t manualBase = 0;
            int manualMaxOffset = 0;
            std::uint64_t arrayBase = 0;
            std::size_t arrayCount = 0;
            std::size_t filterStart = 4;

            if (useManual)
            {
                const auto manualBaseParsed = parseUInt64(tokens[4]);
                const auto manualMaxOffsetParsed = parseInt(tokens[5]);
                if (!manualBaseParsed.has_value() || !manualMaxOffsetParsed.has_value() || *manualMaxOffsetParsed <= 0)
                {
                    return err("manualBase/manualMaxOffset 参数无效");
                }
                manualBase = *manualBaseParsed;
                manualMaxOffset = *manualMaxOffsetParsed;
                filterStart = 6;
            }
            else if (useArray)
            {
                const auto arrayBaseParsed = parseUInt64(tokens[4]);
                const auto arrayCountParsed = parseUInt64(tokens[5]);
                if (!arrayBaseParsed.has_value() || !arrayCountParsed.has_value() || *arrayCountParsed == 0 || *arrayCountParsed > 1000000)
                {
                    return err("arrayBase/arrayCount 参数无效");
                }
                arrayBase = *arrayBaseParsed;
                arrayCount = static_cast<std::size_t>(*arrayCountParsed);
                filterStart = 6;
            }

            const std::string filterModule = (tokens.size() > filterStart) ? joinTokens(tokens, filterStart) : "";

            if (session->pointerManager.isScanning())
            {
                return err("当前已有指针扫描任务在运行");
            }

            // 串行执行指针扫描：与全局驱动请求锁配合，确保任意连接改 PID/发请求都按顺序进入。
            session->pointerManager.scan(
                pid,
                static_cast<uintptr_t>(*target),
                *depth,
                *maxOffset,
                useManual,
                static_cast<uintptr_t>(manualBase),
                manualMaxOffset,
                useArray,
                static_cast<uintptr_t>(arrayBase),
                arrayCount,
                filterModule);

            return ok(std::format("done=1 count={}", session->pointerManager.count()));
        }

        if (command == "pointer.merge")
        {
            session->pointerManager.MergeBins();
            return ok("started=1");
        }

        if (command == "pointer.export")
        {
            session->pointerManager.ExportToTxt();
            return ok("done=1");
        }

        if (command == "memory.refresh")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }
            return ok(std::format("status={}", status));
        }

        if (command == "memory.summary")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }

            const auto &info = dr.GetMemoryInfoRef();
            return ok(std::format("status={} modules={} regions={}", status, info.module_count, info.region_count));
        }

        if (command == "module.list")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }

            const auto &info = dr.GetMemoryInfoRef();
            std::string payload = std::format("status={} count={}", status, info.module_count);
            for (int i = 0; i < info.module_count; ++i)
            {
                const auto &mod = info.modules[i];
                if (mod.name[0] == '\0')
                {
                    continue;
                }

                payload.append(std::format(";{}#{}", sanitizeLine(mod.name), mod.seg_count));
            }
            return ok(payload);
        }

        if (command == "memory.info.full")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }

            const auto &info = dr.GetMemoryInfoRef();
            const std::string jsonText = buildMemoryInfoJson(status, info).dump();
            return std::format("ok memory.info.full size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "module.addr")
        {
            if (tokens.size() != 4)
            {
                return err("用法: module.addr <模块名> <段索引> <start|end>");
            }

            const std::string moduleName = tokens[1];
            const auto segmentIndex = parseInt(tokens[2]);
            if (!segmentIndex.has_value())
            {
                return err("段索引无效");
            }

            const bool isStart = (tokens[3] == "start");
            const bool isEnd = (tokens[3] == "end");
            if (!isStart && !isEnd)
            {
                return err("第三个参数必须是 start 或 end");
            }

            std::uint64_t address = 0;
            const bool found = dr.GetModuleAddress(moduleName, static_cast<short>(*segmentIndex), &address, isStart);
            if (!found)
            {
                return err("未找到目标模块或段");
            }

            return ok(std::format("address=0x{:X}", address));
        }

        if (command == "mem.read")
        {
            if (tokens.size() != 3)
            {
                return err("用法: mem.read <地址> <大小>");
            }

            const auto address = parseUInt64(tokens[1]);
            const auto size = parseUInt64(tokens[2]);
            if (!address.has_value() || !size.has_value() || *size == 0 || *size > 4096)
            {
                return err("地址或大小无效，大小范围 1-4096");
            }

            std::vector<std::uint8_t> buffer(static_cast<std::size_t>(*size));
            const int status = dr.Read(*address, buffer.data(), buffer.size());
            if (status <= 0)
            {
                return err(std::format("读取失败 status={}", status));
            }

            return ok(std::format("hex={}", bytesToHex(buffer.data(), buffer.size())));
        }

        if (command == "mem.write")
        {
            if (tokens.size() < 3)
            {
                return err("用法: mem.write <地址> <HEX字节流>");
            }

            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
            {
                return err("地址无效");
            }

            const std::string hexText = joinTokens(tokens, 2);
            auto bytes = parseHexBytes(hexText);
            if (!bytes.has_value() || bytes->empty())
            {
                return err("HEX 字节流无效");
            }

            const bool success = dr.Write(*address, bytes->data(), bytes->size());
            if (!success)
            {
                return err("写入失败");
            }

            return ok(std::format("size={}", bytes->size()));
        }

        if (command == "mem.read_u8")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u8 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint8_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_u16")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u16 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint16_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_u32")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u32 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint32_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_u64")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u64 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint64_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_f32")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_f32 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<float>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_f64")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_f64 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<double>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.write_u8")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u8 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value() || *value > 0xFF)
                return err("参数无效");
            if (!dr.Write<std::uint8_t>(*address, static_cast<std::uint8_t>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_u16")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u16 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value() || *value > 0xFFFF)
                return err("参数无效");
            if (!dr.Write<std::uint16_t>(*address, static_cast<std::uint16_t>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_u32")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u32 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value() || *value > 0xFFFFFFFFULL)
                return err("参数无效");
            if (!dr.Write<std::uint32_t>(*address, static_cast<std::uint32_t>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_u64")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u64 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value())
                return err("参数无效");
            if (!dr.Write<std::uint64_t>(*address, *value))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_f32")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_f32 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseDouble(tokens[2]);
            if (!address.has_value() || !value.has_value())
                return err("参数无效");
            if (!dr.Write<float>(*address, static_cast<float>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_f64")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_f64 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseDouble(tokens[2]);
            if (!address.has_value() || !value.has_value())
                return err("参数无效");
            if (!dr.Write<double>(*address, *value))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.read_str")
        {
            if (tokens.size() < 2 || tokens.size() > 3)
            {
                return err("用法: mem.read_str <地址> [最大长度]");
            }

            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
            {
                return err("地址无效");
            }

            std::size_t maxLength = 128;
            if (tokens.size() == 3)
            {
                const auto value = parseUInt64(tokens[2]);
                if (!value.has_value() || *value == 0 || *value > 4096)
                {
                    return err("最大长度范围 1-4096");
                }
                maxLength = static_cast<std::size_t>(*value);
            }

            const std::string value = sanitizeLine(dr.ReadString(*address, maxLength));
            return ok(std::format("text={}", value));
        }

        if (command == "mem.read_wstr")
        {
            if (tokens.size() != 3)
            {
                return err("用法: mem.read_wstr <地址> <长度>");
            }

            const auto address = parseUInt64(tokens[1]);
            const auto length = parseUInt64(tokens[2]);
            if (!address.has_value() || !length.has_value() || *length == 0 || *length > 1024)
            {
                return err("地址或长度无效，长度范围 1-1024");
            }

            const std::string value = sanitizeLine(dr.ReadWString(*address, static_cast<std::size_t>(*length)));
            return ok(std::format("text={}", value));
        }

        if (command == "touch.down" || command == "touch.move")
        {
            if (tokens.size() != 5)
            {
                return err("用法: touch.down/touch.move <x> <y> <屏宽> <屏高>");
            }

            const auto x = parseInt(tokens[1]);
            const auto y = parseInt(tokens[2]);
            const auto screenW = parseInt(tokens[3]);
            const auto screenH = parseInt(tokens[4]);
            if (!x.has_value() || !y.has_value() || !screenW.has_value() || !screenH.has_value())
            {
                return err("坐标参数无效");
            }

            if (command == "touch.down")
            {
                dr.TouchDown(*x, *y, *screenW, *screenH);
                return ok("touch.down 已发送");
            }

            dr.TouchMove(*x, *y, *screenW, *screenH);
            return ok("touch.move 已发送");
        }

        if (command == "touch.up")
        {
            dr.TouchUp();
            return ok("touch.up 已发送");
        }

        return err("未知命令，发送 help 可查看命令列表");
    }

    // 将文本协议响应包装为统一 JSON 响应。
    json buildJsonResponseFromText(std::string_view textResponse)
    {
        json out;
        out["ok"] = false;

        if (textResponse.starts_with("err "))
        {
            out["error"] = std::string(textResponse.substr(4));
            return out;
        }

        if (!textResponse.starts_with("ok "))
        {
            out["error"] = "响应格式异常";
            out["raw"] = std::string(textResponse);
            return out;
        }

        out["ok"] = true;
        const std::string body(textResponse.substr(3));
        const auto newlinePos = body.find('\n');
        if (newlinePos == std::string::npos)
        {
            out["message"] = body;
            return out;
        }

        const std::string header = body.substr(0, newlinePos);
        const std::string payload = body.substr(newlinePos + 1);
        out["message"] = header;

        const auto parsedPayload = json::parse(payload, nullptr, false);
        if (!parsedPayload.is_discarded())
        {
            out["data"] = parsedPayload;
        }
        else
        {
            out["data_text"] = payload;
        }
        return out;
    }

    json makeProtocolError(std::string_view message, std::string_view operation = {})
    {
        json out = {
            {"ok", false},
            {"error", std::string(message)},
        };
        if (!operation.empty())
        {
            out["operation"] = std::string(operation);
        }
        return out;
    }

    std::optional<std::string> getRequiredStringParam(const json &params, std::string_view key)
    {
        const auto it = params.find(std::string(key));
        if (it == params.end())
        {
            return std::nullopt;
        }

        if (it->is_string())
        {
            return it->get<std::string>();
        }

        if (it->is_boolean() || it->is_number_integer() || it->is_number_unsigned() || it->is_number_float())
        {
            return it->dump();
        }

        return std::nullopt;
    }

    std::optional<std::string> getOptionalStringParam(const json &params, std::string_view key)
    {
        const auto it = params.find(std::string(key));
        if (it == params.end() || it->is_null())
        {
            return std::nullopt;
        }
        return getRequiredStringParam(params, key);
    }

    void appendCommandToken(std::string &command, const std::string &value)
    {
        command.push_back(' ');
        command.append(value);
    }

    json buildBridgeDescribePayload()
    {
        json payload;
        payload["protocol"] = "native-tcp-bridge";
        payload["request_shapes"] = json::array({
            {
                {"name", "structured"},
                {"fields", json::array({"operation", "params"})},
            },
        });
        payload["operations"] = json::array({
            "bridge.describe",
            "bridge.ping",
            "target.pid.get",
            "target.pid.set",
            "target.pid.current",
            "target.attach.package",
            "memory.info.full",
            "module.resolve",
            "scan.start",
            "scan.refine",
            "scan.status",
            "scan.clear",
            "scan.page",
            "viewer.open",
            "viewer.move",
            "viewer.offset",
            "viewer.set_format",
            "viewer.snapshot",
            "pointer.status",
            "pointer.scan",
            "pointer.merge",
            "pointer.export",
            "breakpoint.info",
            "breakpoint.set",
            "breakpoint.clear",
            "breakpoint.record.remove",
            "breakpoint.record.update",
            "signature.scan_address",
            "signature.scan_file",
            "signature.scan_pattern",
            "signature.filter",
            "lock.set",
            "lock.unset",
            "lock.status",
            "lock.clear",
            "memory.read_block",
            "memory.read_value",
            "memory.write_block",
        });
        return payload;
    }

    json tryDispatchStructuredOperation(const std::shared_ptr<ClientSession> &session, std::string_view operation, const json &params)
    {
        auto requireString = [&](std::string_view key, std::string_view desc) -> std::variant<std::string, json>
        {
            const auto value = getRequiredStringParam(params, key);
            if (!value.has_value() || value->empty())
            {
                return makeProtocolError(std::format("operation={} 缺少参数 {}", operation, desc), operation);
            }
            return *value;
        };

        auto optionalString = [&](std::string_view key) -> std::string
        {
            const auto value = getOptionalStringParam(params, key);
            return value.has_value() ? *value : "";
        };

        if (operation == "bridge.describe")
        {
            return json{
                {"ok", true},
                {"operation", std::string(operation)},
                {"message", "bridge.describe"},
                {"data", buildBridgeDescribePayload()},
            };
        }

        std::string textCommand;

        if (operation == "bridge.ping")
        {
            textCommand = "ping";
        }
        else if (operation == "target.pid.get")
        {
            const auto package = requireString("package_name", "package_name");
            if (std::holds_alternative<json>(package))
            {
                return std::get<json>(package);
            }
            textCommand = "pid.get";
            appendCommandToken(textCommand, std::get<std::string>(package));
        }
        else if (operation == "target.pid.set")
        {
            const auto pid = requireString("pid", "pid");
            if (std::holds_alternative<json>(pid))
            {
                return std::get<json>(pid);
            }
            textCommand = "pid.set";
            appendCommandToken(textCommand, std::get<std::string>(pid));
        }
        else if (operation == "target.pid.current")
        {
            textCommand = "pid.current";
        }
        else if (operation == "target.attach.package")
        {
            const auto package = requireString("package_name", "package_name");
            if (std::holds_alternative<json>(package))
            {
                return std::get<json>(package);
            }
            textCommand = "pid.attach";
            appendCommandToken(textCommand, std::get<std::string>(package));
        }
        else if (operation == "memory.info.full")
        {
            textCommand = "memory.info.full";
        }
        else if (operation == "module.resolve")
        {
            const auto moduleName = requireString("module_name", "module_name");
            const auto segmentIndex = requireString("segment_index", "segment_index");
            const auto which = requireString("which", "which");
            if (std::holds_alternative<json>(moduleName))
                return std::get<json>(moduleName);
            if (std::holds_alternative<json>(segmentIndex))
                return std::get<json>(segmentIndex);
            if (std::holds_alternative<json>(which))
                return std::get<json>(which);
            textCommand = "module.addr";
            appendCommandToken(textCommand, std::get<std::string>(moduleName));
            appendCommandToken(textCommand, std::get<std::string>(segmentIndex));
            appendCommandToken(textCommand, std::get<std::string>(which));
        }
        else if (operation == "scan.start" || operation == "scan.refine")
        {
            const auto type = requireString("value_type", "value_type");
            const auto mode = requireString("mode", "mode");
            if (std::holds_alternative<json>(type))
                return std::get<json>(type);
            if (std::holds_alternative<json>(mode))
                return std::get<json>(mode);

            const std::string typeValue = std::get<std::string>(type);
            const std::string modeValue = std::get<std::string>(mode);
            textCommand = (operation == "scan.start") ? "scan.first" : "scan.next";
            appendCommandToken(textCommand, typeValue);
            appendCommandToken(textCommand, modeValue);

            const std::string value = optionalString("value");
            if (modeValue != "unknown")
            {
                if (value.empty())
                {
                    return makeProtocolError(std::format("operation={} 在 mode={} 时必须提供 value", operation, modeValue), operation);
                }
                appendCommandToken(textCommand, value);
            }

            const std::string rangeMax = optionalString("range_max");
            const std::string modeLower = toLowerAscii(modeValue);
            const bool isStringMode = (modeLower == "string" || modeLower == "str");
            if (!rangeMax.empty() && !isStringMode)
            {
                if (modeValue == "unknown")
                {
                    appendCommandToken(textCommand, "0");
                }
                appendCommandToken(textCommand, rangeMax);
            }
        }
        else if (operation == "scan.status")
        {
            textCommand = "scan.status";
        }
        else if (operation == "scan.clear")
        {
            textCommand = "scan.clear";
        }
        else if (operation == "scan.page")
        {
            const auto start = requireString("start", "start");
            const auto count = requireString("count", "count");
            const auto type = requireString("value_type", "value_type");
            if (std::holds_alternative<json>(start))
                return std::get<json>(start);
            if (std::holds_alternative<json>(count))
                return std::get<json>(count);
            if (std::holds_alternative<json>(type))
                return std::get<json>(type);
            textCommand = "scan.page";
            appendCommandToken(textCommand, std::get<std::string>(start));
            appendCommandToken(textCommand, std::get<std::string>(count));
            appendCommandToken(textCommand, std::get<std::string>(type));
        }
        else if (operation == "viewer.open")
        {
            const auto address = requireString("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            textCommand = "viewer.open";
            appendCommandToken(textCommand, std::get<std::string>(address));
            const std::string viewFormat = optionalString("view_format");
            if (!viewFormat.empty())
            {
                appendCommandToken(textCommand, viewFormat);
            }
        }
        else if (operation == "viewer.move")
        {
            const auto lines = requireString("lines", "lines");
            if (std::holds_alternative<json>(lines))
                return std::get<json>(lines);
            textCommand = "viewer.move";
            appendCommandToken(textCommand, std::get<std::string>(lines));
            const std::string step = optionalString("step");
            if (!step.empty())
            {
                appendCommandToken(textCommand, step);
            }
        }
        else if (operation == "viewer.offset")
        {
            const auto offset = requireString("offset", "offset");
            if (std::holds_alternative<json>(offset))
                return std::get<json>(offset);
            textCommand = "viewer.offset";
            appendCommandToken(textCommand, std::get<std::string>(offset));
        }
        else if (operation == "viewer.set_format")
        {
            const auto viewFormat = requireString("view_format", "view_format");
            if (std::holds_alternative<json>(viewFormat))
                return std::get<json>(viewFormat);
            textCommand = "viewer.format";
            appendCommandToken(textCommand, std::get<std::string>(viewFormat));
        }
        else if (operation == "viewer.snapshot")
        {
            textCommand = "viewer.get";
        }
        else if (operation == "pointer.status")
        {
            textCommand = "pointer.status";
        }
        else if (operation == "pointer.scan")
        {
            const std::string mode = optionalString("mode").empty() ? "module" : optionalString("mode");
            const auto target = requireString("target", "target");
            const auto depth = requireString("depth", "depth");
            const auto maxOffset = requireString("max_offset", "max_offset");
            if (std::holds_alternative<json>(target))
                return std::get<json>(target);
            if (std::holds_alternative<json>(depth))
                return std::get<json>(depth);
            if (std::holds_alternative<json>(maxOffset))
                return std::get<json>(maxOffset);

            if (mode == "manual")
            {
                const auto manualBase = requireString("manual_base", "manual_base");
                const auto manualMaxOffset = requireString("manual_max_offset", "manual_max_offset");
                if (std::holds_alternative<json>(manualBase))
                    return std::get<json>(manualBase);
                if (std::holds_alternative<json>(manualMaxOffset))
                    return std::get<json>(manualMaxOffset);
                textCommand = "pointer.scan.manual";
                appendCommandToken(textCommand, std::get<std::string>(target));
                appendCommandToken(textCommand, std::get<std::string>(depth));
                appendCommandToken(textCommand, std::get<std::string>(maxOffset));
                appendCommandToken(textCommand, std::get<std::string>(manualBase));
                appendCommandToken(textCommand, std::get<std::string>(manualMaxOffset));
            }
            else if (mode == "array")
            {
                const auto arrayBase = requireString("array_base", "array_base");
                const auto arrayCount = requireString("array_count", "array_count");
                if (std::holds_alternative<json>(arrayBase))
                    return std::get<json>(arrayBase);
                if (std::holds_alternative<json>(arrayCount))
                    return std::get<json>(arrayCount);
                textCommand = "pointer.scan.array";
                appendCommandToken(textCommand, std::get<std::string>(target));
                appendCommandToken(textCommand, std::get<std::string>(depth));
                appendCommandToken(textCommand, std::get<std::string>(maxOffset));
                appendCommandToken(textCommand, std::get<std::string>(arrayBase));
                appendCommandToken(textCommand, std::get<std::string>(arrayCount));
            }
            else
            {
                textCommand = "pointer.scan";
                appendCommandToken(textCommand, std::get<std::string>(target));
                appendCommandToken(textCommand, std::get<std::string>(depth));
                appendCommandToken(textCommand, std::get<std::string>(maxOffset));
            }

            const std::string moduleFilter = optionalString("module_filter");
            if (!moduleFilter.empty())
            {
                appendCommandToken(textCommand, moduleFilter);
            }
        }
        else if (operation == "pointer.merge")
        {
            textCommand = "pointer.merge";
        }
        else if (operation == "pointer.export")
        {
            textCommand = "pointer.export";
        }
        else if (operation == "breakpoint.info")
        {
            textCommand = "hwbp.info";
        }
        else if (operation == "breakpoint.set")
        {
            const auto address = requireString("address", "address");
            const auto bpType = requireString("bp_type", "bp_type");
            const auto bpScope = requireString("bp_scope", "bp_scope");
            const auto length = requireString("length", "length");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(bpType))
                return std::get<json>(bpType);
            if (std::holds_alternative<json>(bpScope))
                return std::get<json>(bpScope);
            if (std::holds_alternative<json>(length))
                return std::get<json>(length);
            textCommand = "hwbp.set";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(bpType));
            appendCommandToken(textCommand, std::get<std::string>(bpScope));
            appendCommandToken(textCommand, std::get<std::string>(length));
        }
        else if (operation == "breakpoint.clear")
        {
            textCommand = "hwbp.remove";
        }
        else if (operation == "breakpoint.record.remove")
        {
            const auto index = requireString("index", "index");
            if (std::holds_alternative<json>(index))
                return std::get<json>(index);
            textCommand = "hwbp.record.remove";
            appendCommandToken(textCommand, std::get<std::string>(index));
        }
        else if (operation == "breakpoint.record.update")
        {
            const auto index = requireString("index", "index");
            const auto field = requireString("field", "field");
            const auto value = requireString("value", "value");
            if (std::holds_alternative<json>(index))
                return std::get<json>(index);
            if (std::holds_alternative<json>(field))
                return std::get<json>(field);
            if (std::holds_alternative<json>(value))
                return std::get<json>(value);
            textCommand = "hwbp.record.set";
            appendCommandToken(textCommand, std::get<std::string>(index));
            appendCommandToken(textCommand, std::get<std::string>(field));
            appendCommandToken(textCommand, std::get<std::string>(value));
        }
        else if (operation == "breakpoint.record.set_float")
        {
            const auto index = requireString("index", "index");
            const auto field = requireString("field", "field");
            const auto value = requireString("value", "value");
            const auto precision = optionalString("precision");
            if (std::holds_alternative<json>(index))
                return std::get<json>(index);
            if (std::holds_alternative<json>(field))
                return std::get<json>(field);
            if (std::holds_alternative<json>(value))
                return std::get<json>(value);
            std::string prec = "f32";
            if (!precision.empty())
            {
                prec = precision;
                if (prec != "f32" && prec != "f64")
                    prec = "f32";
            }
            textCommand = "hwbp.record.set." + prec;
            appendCommandToken(textCommand, std::get<std::string>(index));
            appendCommandToken(textCommand, std::get<std::string>(field));
            appendCommandToken(textCommand, std::get<std::string>(value));
        }
        else if (operation == "signature.scan_address")
        {
            const auto address = requireString("address", "address");
            const auto range = requireString("range", "range");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(range))
                return std::get<json>(range);
            textCommand = "sig.scan.addr";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(range));
            const std::string fileName = optionalString("file_name");
            if (!fileName.empty())
            {
                appendCommandToken(textCommand, fileName);
            }
        }
        else if (operation == "signature.scan_file")
        {
            textCommand = "sig.scan.file";
            const std::string fileName = optionalString("file_name");
            if (!fileName.empty())
            {
                appendCommandToken(textCommand, fileName);
            }
        }
        else if (operation == "signature.scan_pattern")
        {
            const auto rangeOffset = requireString("range_offset", "range_offset");
            const auto pattern = requireString("pattern", "pattern");
            if (std::holds_alternative<json>(rangeOffset))
                return std::get<json>(rangeOffset);
            if (std::holds_alternative<json>(pattern))
                return std::get<json>(pattern);
            textCommand = "sig.scan.pattern";
            appendCommandToken(textCommand, std::get<std::string>(rangeOffset));
            appendCommandToken(textCommand, std::get<std::string>(pattern));
        }
        else if (operation == "signature.filter")
        {
            const auto address = requireString("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            textCommand = "sig.filter";
            appendCommandToken(textCommand, std::get<std::string>(address));
            const std::string fileName = optionalString("file_name");
            if (!fileName.empty())
            {
                appendCommandToken(textCommand, fileName);
            }
        }
        else if (operation == "lock.set")
        {
            const auto address = requireString("address", "address");
            const auto valueType = requireString("value_type", "value_type");
            const auto value = requireString("value", "value");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(valueType))
                return std::get<json>(valueType);
            if (std::holds_alternative<json>(value))
                return std::get<json>(value);
            textCommand = "lock.set";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(valueType));
            appendCommandToken(textCommand, std::get<std::string>(value));
        }
        else if (operation == "lock.unset")
        {
            const auto address = requireString("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            textCommand = "lock.unset";
            appendCommandToken(textCommand, std::get<std::string>(address));
        }
        else if (operation == "lock.status")
        {
            const auto address = requireString("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            textCommand = "lock.status";
            appendCommandToken(textCommand, std::get<std::string>(address));
        }
        else if (operation == "lock.clear")
        {
            textCommand = "lock.clear";
        }
        else if (operation == "memory.read_block")
        {
            const auto address = requireString("address", "address");
            const auto size = requireString("size", "size");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(size))
                return std::get<json>(size);
            textCommand = "mem.read";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(size));
        }
        else if (operation == "memory.read_value")
        {
            const auto address = requireString("address", "address");
            const auto valueType = requireString("value_type", "value_type");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(valueType))
                return std::get<json>(valueType);

            static const std::unordered_map<std::string, std::string> valueCommandMap = {
                {"u8", "mem.read_u8"},
                {"u16", "mem.read_u16"},
                {"u32", "mem.read_u32"},
                {"u64", "mem.read_u64"},
                {"f32", "mem.read_f32"},
                {"f64", "mem.read_f64"},
            };

            const auto commandIt = valueCommandMap.find(std::get<std::string>(valueType));
            if (commandIt == valueCommandMap.end())
            {
                return makeProtocolError("memory.read_value 的 value_type 仅支持 u8/u16/u32/u64/f32/f64", operation);
            }

            textCommand = commandIt->second;
            appendCommandToken(textCommand, std::get<std::string>(address));
        }
        else if (operation == "memory.write_block")
        {
            const auto address = requireString("address", "address");
            const auto dataHex = requireString("data_hex", "data_hex");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(dataHex))
                return std::get<json>(dataHex);
            textCommand = "mem.write";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(dataHex));
        }
        else
        {
            return makeProtocolError(std::format("未知 operation: {}", operation), operation);
        }

        const std::string textResponse = DispatchTextCommand(session, textCommand);
        json out = buildJsonResponseFromText(textResponse);
        out["session_id"] = session->sessionId;
        out["operation"] = std::string(operation);
        return out;
    }

    // 统一命令派发入口：网络层仅接受 JSON 请求并返回 JSON 响应。
    std::string DispatchCommandUnified(const std::shared_ptr<ClientSession> &session, const std::string &request)
    {
        const auto parsedReq = json::parse(request, nullptr, false);
        if (parsedReq.is_discarded())
        {
            return json({{"ok", false}, {"error", "请求必须是 JSON 字符串对象"}}).dump();
        }

        if (!parsedReq.is_object())
        {
            return json({{"ok", false}, {"error", "请求必须是 JSON 对象"}}).dump();
        }

        if (parsedReq.contains("operation"))
        {
            if (!parsedReq["operation"].is_string())
            {
                return makeProtocolError("operation 字段必须是字符串").dump();
            }

            const std::string operationName = parsedReq["operation"].get<std::string>();
            json params = json::object();
            if (parsedReq.contains("params"))
            {
                if (!parsedReq["params"].is_object())
                {
                    return makeProtocolError("params 字段必须是对象", operationName).dump();
                }
                params = parsedReq["params"];
            }

            return tryDispatchStructuredOperation(session, operationName, params).dump();
        }

        return makeProtocolError("请求缺少 operation 字段").dump();
    }

    void HandleClientConnection(int clientFd, sockaddr_in clientAddr)
    {
        const auto session = std::make_shared<ClientSession>(gClientSessionSeed.fetch_add(1));

        char clientIp[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp)) == nullptr)
        {
            std::strncpy(clientIp, "未知地址", sizeof(clientIp) - 1);
            clientIp[sizeof(clientIp) - 1] = '\0';
        }

        std::println("客户端已连接：{}:{} session={}", clientIp, ntohs(clientAddr.sin_port), session->sessionId);

        std::string buffer;
        buffer.reserve(4096);
        char recvChunk[4096]{};

        while (gRunning)
        {
            const ssize_t receivedBytes = recv(clientFd, recvChunk, sizeof(recvChunk), 0);
            if (receivedBytes == 0)
            {
                std::println("客户端已断开连接：session={}", session->sessionId);
                break;
            }

            if (receivedBytes < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                printErrno("接收数据失败");
                break;
            }

            buffer.append(recvChunk, recvChunk + receivedBytes);

            for (;;)
            {
                const auto newlinePos = buffer.find('\n');
                if (newlinePos == std::string::npos)
                {
                    break;
                }

                std::string message = buffer.substr(0, newlinePos);
                buffer.erase(0, newlinePos + 1);
                trimLineEnding(message);
                if (message.empty())
                {
                    continue;
                }

                std::println("收到命令：session={} {}", session->sessionId, message);
                const std::string response = DispatchCommandUnified(session, message) + "\n";
                if (!sendAll(clientFd, response))
                {
                    printErrno("发送回复失败");
                    close(clientFd);
                    return;
                }
            }
        }

        close(clientFd);
    }
} // namespace

// 程序入口：初始化服务并处理客户端请求。
int main(int argc, char **)
{
    if (argc > 1)
    {
        std::println("提示：端口已固定为 {}，已忽略命令行参数。", kServerPort);
    }

    const int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0)
    {
        printErrno("创建套接字失败");
        return 1;
    }
    gServerFd = serverFd;

    constexpr int enableReuse = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(enableReuse)) < 0)
    {
        printErrno("设置套接字选项失败");
        close(serverFd);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(kServerPort);

    if (bind(serverFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
    {
        printErrno("绑定端口失败");
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, kListenBacklog) < 0)
    {
        printErrno("开始监听失败");
        close(serverFd);
        return 1;
    }

    std::println("TCP 服务端已监听 0.0.0.0:{}", kServerPort);

    while (gRunning)
    {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        const int clientFd = accept(serverFd, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
        if (clientFd < 0)
        {
            if (!gRunning || errno == EINTR)
            {
                continue;
            }
            printErrno("接受连接失败");
            continue;
        }

        if (!Utils::GlobalPool.post_io(HandleClientConnection, clientFd, clientAddr))
        {
            printErrno("IO线程池已停止，无法派发连接");
            close(clientFd);
        }
    }

    if (gServerFd >= 0)
    {
        close(gServerFd);
        gServerFd = -1;
    }

    Utils::GlobalPool.force_stop();

    std::println("服务端已退出。");
    return 0;
}
