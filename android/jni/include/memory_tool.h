
#pragma once
#include <algorithm>
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <cinttypes>
#include <cctype>
#include <cmath>
#include <concepts>
#include <condition_variable>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <fstream>
#include <format>
#include <functional>
#include <future>
#include <iostream>
#include <iterator>
#include <limits>
#include <list>
#include <map>
#include <memory>
#include <mutex>
#include <numeric>
#include <optional>
#include <print>
#include <queue>
#include <ranges>
#include <set>
#include <shared_mutex>
#include <span>
#include <sstream>
#include <stack>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <thread>
#include <tuple>
#include <type_traits>
#include <unordered_set>
#include <utility>
#include <variant>
#include <vector>
#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <sys/mman.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <unistd.h>

#include "BS_thread_pool.hpp"
#include "driver.h"
#include "utils/mapped_file.h"
#include "disassembler.h"

// ============================================================================
// 配置模块 (Config)
// ============================================================================
namespace Config
{
    inline std::atomic<bool> g_Running{true};
    inline std::atomic<int> g_ItemsPerPage{100};
    inline std::mutex TargetMutex;

    // 函数内静态确保线程池在 daemonize() 的两次 fork() 结束后才首次创建。
    inline BS::thread_pool<> &CpuThreadPool()
    {
        static BS::thread_pool<> pool{std::max(4u, std::thread::hardware_concurrency())};
        return pool;
    }

    inline BS::thread_pool<> &IoThreadPool()
    {
        static BS::thread_pool<> pool{std::max(16u, std::thread::hardware_concurrency() * 4)};
        return pool;
    }

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

namespace SyscallLog
{
    inline std::string ReadDmesg()
    {
        std::string out;
        std::array<char, 4096> buf{};
        FILE *pipe = popen("dmesg 2>/dev/null | grep lsdriver", "r");
        if (!pipe) return {};
        while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) out += buf.data();
        pclose(pipe);
        return out;
    }
} // namespace SyscallLog

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
        Hexadecimal,
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
        case ViewFormat::Hexadecimal:
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
        inline constexpr std::array<const char *, static_cast<size_t>(DataType::Count)> TYPE = {"I8", "I16", "I32", "I64", "Float", "Double"};

        inline constexpr std::array<const char *, static_cast<size_t>(FuzzyMode::Count)> FUZZY = {"未知", "等于", "大于", "小于", "增大", "减小", "已改变", "未改变", "范围", "指针", "字符串"};

        inline constexpr std::array<const char *, static_cast<size_t>(ViewFormat::Count)> VIEW_FORMAT = {"Hex", "Hexadecimal", "I8", "I16", "I32", "I64", "Float", "Double", "Disasm"};
    } // namespace Labels
} // namespace Types

namespace MemUtils
{
    using namespace Types;
    using namespace Config;

    // 去除0xb40000高位标签
    constexpr uintptr_t Normalize(uintptr_t addr) noexcept
    {
        return addr & ~(0xFFULL << 56);
    }

    inline std::string_view BaseName(std::string_view path) noexcept
    {
        if (auto slash = path.rfind('/'); slash != std::string_view::npos) return path.substr(slash + 1);
        return path;
    }

    inline std::optional<std::uint64_t> ParseUInt64(std::string_view text, int base = 0)
    {
        if (text.empty()) return std::nullopt;

        const auto first = text.find_first_not_of(" \t\r\n");
        if (first == std::string_view::npos || text[first] == '-') return std::nullopt;

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const auto value = std::strtoull(temp.c_str(), &end, base);
        if (errno != 0 || end == temp.c_str() || *end != '\0') return std::nullopt;
        return static_cast<std::uint64_t>(value);
    }

    template <typename T> std::optional<T> ParseScanValue(std::string_view text)
    {
        static_assert(std::is_arithmetic_v<T> && (std::is_floating_point_v<T> || std::is_signed_v<T>));
        if (text.empty()) return std::nullopt;

        std::string temp(text);
        char *end = nullptr;
        errno = 0;

        if constexpr (std::is_floating_point_v<T>)
        {
            const long double value = std::strtold(temp.c_str(), &end);
            if (errno == ERANGE || end == temp.c_str() || *end != '\0' || !std::isfinite(value)) return std::nullopt;
            if (value < static_cast<long double>(std::numeric_limits<T>::lowest()) || value > static_cast<long double>(std::numeric_limits<T>::max())) return std::nullopt;
            const T converted = static_cast<T>(value);
            if (!std::isfinite(converted) || (value != 0.0L && converted == T{})) return std::nullopt;
            return converted;
        }
        else
        {
            const long long value = std::strtoll(temp.c_str(), &end, 10);
            if (errno == ERANGE || end == temp.c_str() || *end != '\0') return std::nullopt;
            if (value < static_cast<long long>(std::numeric_limits<T>::min()) || value > static_cast<long long>(std::numeric_limits<T>::max())) return std::nullopt;
            return static_cast<T>(value);
        }
    }

    inline std::optional<__uint128_t> ParseUInt128(std::string_view text, int base = 0)
    {
        if (text.empty()) return std::nullopt;

        if (base == 0)
        {
            base = 10;
            if (text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
            {
                base = 16;
                text.remove_prefix(2);
            }
        }
        else if (base == 16 && text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        {
            text.remove_prefix(2);
        }

        if (text.empty()) return std::nullopt;

        __uint128_t value = 0;
        for (const char ch : text)
        {
            int digit = -1;
            if (ch >= '0' && ch <= '9') digit = ch - '0';
            else if (base == 16 && ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
            else if (base == 16 && ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;

            if (digit < 0 || digit >= base) return std::nullopt;
            value = value * static_cast<unsigned>(base) + static_cast<unsigned>(digit);
        }
        return value;
    }

    inline std::string FormatUInt128Hex(__uint128_t value)
    {
        if (value == 0) return "0x0";

        char buf[35]{};
        int pos = 34;
        static constexpr char kHex[] = "0123456789ABCDEF";
        while (value != 0 && pos > 1)
        {
            buf[--pos] = kHex[static_cast<unsigned>(value & 0xF)];
            value >>= 4;
        }
        buf[--pos] = 'x';
        buf[--pos] = '0';
        return std::string(buf + pos);
    }

    // 验证地址合法
    constexpr bool IsValidAddr(uintptr_t addr) noexcept
    {
        uintptr_t a = Normalize(addr);
        return a > Constants::ADDR_MIN && a < Constants::ADDR_MAX;
    }

    // 验证浮点数合法性
    template <typename T> constexpr bool IsValidFloat(T value) noexcept
    {
        if constexpr (std::is_floating_point_v<T>)
        {
            return !std::isnan(value) && !std::isinf(value);
        }
        return true;
    }

    inline void HwbpRequestRead(Driver::bp_record &record, int reg)
    {
        if (reg >= 0 && reg < Driver::MAX_REG_COUNT && BP_GET_MASK(&record, reg) != BP_OP_WRITE) BP_SET_MASK(&record, reg, BP_OP_READ);
    }

    inline void HwbpRequestAll(Driver::bp_record &record)
    {
        for (int reg = Driver::IDX_PC; reg < Driver::MAX_REG_COUNT; ++reg) HwbpRequestRead(record, reg);
    }

    inline constexpr std::uint64_t Driver::bp_record::*HwbpXFields[] = {&Driver::bp_record::x0, &Driver::bp_record::x1, &Driver::bp_record::x2, &Driver::bp_record::x3, &Driver::bp_record::x4, &Driver::bp_record::x5, &Driver::bp_record::x6, &Driver::bp_record::x7, &Driver::bp_record::x8, &Driver::bp_record::x9, &Driver::bp_record::x10, &Driver::bp_record::x11, &Driver::bp_record::x12, &Driver::bp_record::x13, &Driver::bp_record::x14, &Driver::bp_record::x15, &Driver::bp_record::x16, &Driver::bp_record::x17, &Driver::bp_record::x18, &Driver::bp_record::x19, &Driver::bp_record::x20, &Driver::bp_record::x21, &Driver::bp_record::x22, &Driver::bp_record::x23, &Driver::bp_record::x24, &Driver::bp_record::x25, &Driver::bp_record::x26, &Driver::bp_record::x27, &Driver::bp_record::x28, &Driver::bp_record::x29};
    inline constexpr __uint128_t Driver::bp_record::*HwbpQFields[] = {&Driver::bp_record::q0, &Driver::bp_record::q1, &Driver::bp_record::q2, &Driver::bp_record::q3, &Driver::bp_record::q4, &Driver::bp_record::q5, &Driver::bp_record::q6, &Driver::bp_record::q7, &Driver::bp_record::q8, &Driver::bp_record::q9, &Driver::bp_record::q10, &Driver::bp_record::q11, &Driver::bp_record::q12, &Driver::bp_record::q13, &Driver::bp_record::q14, &Driver::bp_record::q15, &Driver::bp_record::q16, &Driver::bp_record::q17, &Driver::bp_record::q18, &Driver::bp_record::q19, &Driver::bp_record::q20, &Driver::bp_record::q21, &Driver::bp_record::q22, &Driver::bp_record::q23, &Driver::bp_record::q24, &Driver::bp_record::q25, &Driver::bp_record::q26, &Driver::bp_record::q27, &Driver::bp_record::q28, &Driver::bp_record::q29, &Driver::bp_record::q30, &Driver::bp_record::q31};

    inline __uint128_t HwbpReadRegisterValue(Driver::bp_record &record, int reg)
    {
        HwbpRequestRead(record, reg);
        switch (reg)
        {
        case Driver::IDX_PC:
            return record.pc;
        case Driver::IDX_HIT_COUNT:
            return record.hit_count;
        case Driver::IDX_LR:
            return record.lr;
        case Driver::IDX_SP:
            return record.sp;
        case Driver::IDX_ORIG_X0:
            return record.orig_x0;
        case Driver::IDX_SYSCALLNO:
            return record.syscallno;
        case Driver::IDX_PSTATE:
            return record.pstate;
        case Driver::IDX_FPSR:
            return record.fpsr;
        case Driver::IDX_FPCR:
            return record.fpcr;
        default:
            if (reg >= Driver::IDX_X0 && reg <= Driver::IDX_X29) return record.*HwbpXFields[reg - Driver::IDX_X0];
            if (reg >= Driver::IDX_Q0 && reg <= Driver::IDX_Q31) return record.*HwbpQFields[reg - Driver::IDX_Q0];
            return 0;
        }
    }

    inline bool HwbpWriteRegisterValue(Driver::bp_record &record, int reg, __uint128_t value)
    {
        if (reg < 0 || reg >= Driver::MAX_REG_COUNT) return false;

        BP_SET_MASK(&record, reg, BP_OP_WRITE);
        switch (reg)
        {
        case Driver::IDX_PC:
            record.pc = static_cast<std::uint64_t>(value);
            return true;
        case Driver::IDX_HIT_COUNT:
            record.hit_count = static_cast<std::uint64_t>(value);
            return true;
        case Driver::IDX_LR:
            record.lr = static_cast<std::uint64_t>(value);
            return true;
        case Driver::IDX_SP:
            record.sp = static_cast<std::uint64_t>(value);
            return true;
        case Driver::IDX_ORIG_X0:
            record.orig_x0 = static_cast<std::uint64_t>(value);
            return true;
        case Driver::IDX_SYSCALLNO:
            record.syscallno = static_cast<std::uint64_t>(value);
            return true;
        case Driver::IDX_PSTATE:
            record.pstate = static_cast<std::uint64_t>(value);
            return true;
        case Driver::IDX_FPSR:
            record.fpsr = static_cast<std::uint32_t>(value);
            return true;
        case Driver::IDX_FPCR:
            record.fpcr = static_cast<std::uint32_t>(value);
            return true;
        default:
            if (reg >= Driver::IDX_X0 && reg <= Driver::IDX_X29)
            {
                record.*HwbpXFields[reg - Driver::IDX_X0] = static_cast<std::uint64_t>(value);
                return true;
            }
            if (reg >= Driver::IDX_Q0 && reg <= Driver::IDX_Q31)
            {
                record.*HwbpQFields[reg - Driver::IDX_Q0] = value;
                return true;
            }
            return false;
        }
    }

    inline std::string HwbpLowerAscii(std::string_view input)
    {
        std::string out(input);
        for (char &ch : out)
        {
            if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }
        return out;
    }

    inline std::optional<int> HwbpParseInt(std::string_view text)
    {
        if (text.empty()) return std::nullopt;

        int value = 0;
        const char *begin = text.data();
        const char *end = begin + text.size();
        const auto [ptr, ec] = std::from_chars(begin, end, value, 10);
        if (ec != std::errc{} || ptr != end) return std::nullopt;
        return value;
    }

    inline std::optional<int> HwbpRegIndexFromToken(std::string_view fieldToken)
    {
        std::string token = HwbpLowerAscii(fieldToken);
        if (token.starts_with("op.")) token.erase(0, 3);
        else if (token.starts_with("mask.")) token.erase(0, 5);

        if (token == "pc") return Driver::IDX_PC;
        if (token == "hit_count") return Driver::IDX_HIT_COUNT;
        if (token == "lr") return Driver::IDX_LR;
        if (token == "sp") return Driver::IDX_SP;
        if (token == "pstate") return Driver::IDX_PSTATE;
        if (token == "orig_x0") return Driver::IDX_ORIG_X0;
        if (token == "syscallno") return Driver::IDX_SYSCALLNO;
        if (token == "fpsr") return Driver::IDX_FPSR;
        if (token == "fpcr") return Driver::IDX_FPCR;
        if (token.size() >= 2 && token[0] == 'x')
        {
            const auto regIndex = HwbpParseInt(std::string_view(token).substr(1));
            if (regIndex.has_value() && *regIndex >= 0 && *regIndex < 30) return Driver::IDX_X0 + *regIndex;
        }
        if (token.size() >= 2 && (token[0] == 'v' || token[0] == 'q'))
        {
            const auto regIndex = HwbpParseInt(std::string_view(token).substr(1));
            if (regIndex.has_value() && *regIndex >= 0 && *regIndex < 32) return Driver::IDX_Q0 + *regIndex;
        }
        return std::nullopt;
    }

    inline std::optional<int> HwbpMaskByteIndexFromToken(std::string_view fieldToken)
    {
        std::string token = HwbpLowerAscii(fieldToken);
        if (!token.starts_with("mask")) return std::nullopt;

        token.erase(0, 4);
        if (!token.empty() && (token.front() == '.' || token.front() == '_' || token.front() == '[')) token.erase(0, 1);
        if (!token.empty() && token.back() == ']') token.pop_back();

        const auto index = HwbpParseInt(token);
        if (!index.has_value() || *index < 0 || *index >= 18) return std::nullopt;
        return *index;
    }

    inline bool AssignHwbpRecordField(Driver::bp_record &record, std::string_view fieldToken, __uint128_t value)
    {
        const std::string token = HwbpLowerAscii(fieldToken);
        if (const auto maskIndex = HwbpMaskByteIndexFromToken(token); maskIndex.has_value())
        {
            if (value > std::numeric_limits<std::uint8_t>::max()) return false;
            record.mask[*maskIndex] = static_cast<std::uint8_t>(value);
            return true;
        }

        if (token.starts_with("op.") || token.starts_with("mask."))
        {
            const auto regIndex = HwbpRegIndexFromToken(token);
            if (!regIndex.has_value() || value > BP_OP_WRITE) return false;
            BP_SET_MASK(&record, *regIndex, static_cast<std::uint8_t>(value));
            return true;
        }

        const auto regIndex = HwbpRegIndexFromToken(token);
        if (!regIndex.has_value()) return false;
        if (*regIndex >= Driver::IDX_Q0 && *regIndex <= Driver::IDX_Q31) return HwbpWriteRegisterValue(record, *regIndex, value);
        if ((*regIndex == Driver::IDX_FPSR || *regIndex == Driver::IDX_FPCR) && value > std::numeric_limits<std::uint32_t>::max()) return false;
        if (value > std::numeric_limits<std::uint64_t>::max()) return false;
        return HwbpWriteRegisterValue(record, *regIndex, value);
    }

    // 统一的类型分发
    template <typename F> decltype(auto) DispatchType(DataType type, F &&fn)
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
        template <typename T> std::string ValueToString(T val)
        {
            if constexpr (std::is_floating_point_v<T>) return std::format("{:.11f}", val);
            else if constexpr (sizeof(T) <= 4) return std::to_string(static_cast<int>(val));
            else return std::to_string(static_cast<long long>(val));
        }
    } // namespace detail

    // 按指定类型读取内存并转为字符串。
    inline std::string ReadAsString(uintptr_t addr, DataType type)
    {
        if (!addr) return "??";
        return DispatchType(type,
                            [&]<typename T>() -> std::string
                            {
                                T value{};
                                if (dr->Read(addr, &value, sizeof(T)) != static_cast<int>(sizeof(T))) return "??";
                                return detail::ValueToString(value);
                            });
    }

    // 把字符串按指定类型写入目标地址。
    inline bool WriteFromString(uintptr_t addr, DataType type, std::string_view str)
    {
        if (!addr || str.empty()) return false;
        return DispatchType(type,
                            [&]<typename T>() -> bool
                            {
                                const auto value = ParseScanValue<T>(str);
                                return value.has_value() && dr->Write<T>(addr, *value) == static_cast<int>(sizeof(T));
                            });
    }

    // 读取指针值并格式化为十六进制文本。
    inline std::string ReadAsText(uintptr_t addr, size_t maxLen = 64)
    {
        if (!addr) return "??";

        maxLen = std::clamp<size_t>(maxLen, 1, 256);
        std::string value = dr->ReadString(addr, maxLen);
        for (char &ch : value)
        {
            unsigned char u = static_cast<unsigned char>(ch);
            if (u < 0x20 && ch != '\t') ch = '.';
        }
        return value;
    }

    inline bool WriteText(uintptr_t addr, std::string_view str)
    {
        if (!addr || str.empty()) return false;

        std::string temp(str);
        const auto size = temp.size() + 1;
        return dr->Write(addr, temp.data(), size) == static_cast<int>(size);
    }

    inline std::string ReadAsPointerString(uintptr_t addr)
    {
        if (!addr) return "??";
        int64_t value = 0;
        if (dr->Read(addr, &value, sizeof(value)) != static_cast<int>(sizeof(value))) return "??";
        return std::format("0x{:X}", Normalize(static_cast<uintptr_t>(value)));
    }

    // 把十六进制文本解析后写入指针值。
    inline bool WritePointerFromString(uintptr_t addr, std::string_view str)
    {
        if (!addr || str.empty()) return false;
        try
        {
            const auto parsed = ParseUInt64(str, 16);
            if (!parsed) return false;
            const int64_t value = static_cast<int64_t>(*parsed);
            return dr->Write<int64_t>(addr, value) == static_cast<int>(sizeof(value));
        }
        catch (...)
        {
            return false;
        }
    }

    //  按扫描模式比较当前值与目标值。
    template <typename T> bool Compare(T value, T target, FuzzyMode mode, T lastValue, T rangeMax = T{})
    {
        auto compare = [](T lhs, T rhs)
        {
            if constexpr (std::is_floating_point_v<T>)
            {
                constexpr long double absEpsilon = std::is_same_v<T, float> ? 1e-4L : 1e-8L;
                constexpr long double relEpsilon = static_cast<long double>(std::numeric_limits<T>::epsilon());
                const long double left = static_cast<long double>(lhs);
                const long double right = static_cast<long double>(rhs);
                const long double tolerance = std::max(absEpsilon, std::max(std::abs(left), std::abs(right)) * relEpsilon);
                const long double delta = left - right;
                if (delta > tolerance) return 1;
                if (delta < -tolerance) return -1;
                return 0;
            }
            else
            {
                if (lhs > rhs) return 1;
                if (lhs < rhs) return -1;
                return 0;
            }
        };

        if constexpr (std::is_floating_point_v<T>)
        {
            if (!IsValidFloat(value)) return false;
            if ((mode == FuzzyMode::Equal || mode == FuzzyMode::Greater || mode == FuzzyMode::Less) && !IsValidFloat(target)) return false;
            if ((mode == FuzzyMode::Increased || mode == FuzzyMode::Decreased || mode == FuzzyMode::Changed || mode == FuzzyMode::Unchanged) && !IsValidFloat(lastValue)) return false;
            if (mode == FuzzyMode::Range && (!IsValidFloat(target) || !IsValidFloat(rangeMax))) return false;
        }

        switch (mode)
        {
        case FuzzyMode::Equal:
            return compare(value, target) == 0;
        case FuzzyMode::Greater:
            return compare(value, target) > 0;
        case FuzzyMode::Less:
            return compare(value, target) < 0;
        case FuzzyMode::Increased:
            return compare(value, lastValue) > 0;
        case FuzzyMode::Decreased:
            return compare(value, lastValue) < 0;
        case FuzzyMode::Changed:
            return compare(value, lastValue) != 0;
        case FuzzyMode::Unchanged:
            return compare(value, lastValue) == 0;
        case FuzzyMode::Range:
        {
            T lo = target, hi = rangeMax;
            if (lo > hi) std::swap(lo, hi);
            return compare(value, lo) >= 0 && compare(value, hi) <= 0;
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
        if (str.empty()) return std::nullopt;

        // 跳过前导空格
        auto pos = str.find_first_not_of(' ');
        if (pos == std::string_view::npos) return std::nullopt;
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
        if (str.empty()) return std::nullopt;

        // 跳过 0x/0X
        if (str.size() >= 2 && str[0] == '0' && (str[1] == 'x' || str[1] == 'X')) str.remove_prefix(2);

        const auto offset = ParseUInt64(str, 16);
        if (!offset.has_value()) return std::nullopt;
        return OffsetParseResult{static_cast<uintptr_t>(*offset), negative};
    }

} // namespace MemUtils

// ============================================================================
// 特征码扫描器
// ============================================================================
namespace SignatureScanner
{
    inline constexpr int SIG_MAX_RANGE = 1200;
    inline constexpr size_t SIG_BUFFER_SIZE = 0x8000;
    inline constexpr const char *SIG_DEFAULT_FILE = "Signature.txt";

    struct SigElement
    {
        std::vector<uint8_t> bytes;
        std::vector<bool> mask;
        bool empty() const
        {
            return bytes.empty();
        }
        size_t size() const
        {
            return bytes.size();
        }
        void clear()
        {
            bytes.clear();
            mask.clear();
        }
    };

    struct SigFilterResult
    {
        bool success = false;
        int changedCount = 0;
        int totalCount = 0;
        std::string oldSignature;
        std::string newSignature;
    };

    namespace
    {
        std::string NormalizeSigFileName(const char *filename)
        {
            if (filename != nullptr && *filename != '\0') return std::string(filename);
            return std::string(SIG_DEFAULT_FILE);
        }

        bool IsAbsoluteSigPath(std::string_view path)
        {
            return !path.empty() && path.front() == '/';
        }

        std::string ResolveSigPath(std::string_view path)
        {
            if (IsAbsoluteSigPath(path)) return std::string(path);
            return std::string("/data/akernel/") + std::string(path);
        }

        std::string FormatSignature(const SigElement &sig)
        {
            if (sig.empty()) return "";
            std::string result;
            result.reserve(sig.size() * 4);
            for (size_t i = 0; i < sig.bytes.size(); ++i)
            {
                if (i > 0) result += ' ';
                if (!sig.mask[i]) result += "??";
                else std::format_to(std::back_inserter(result), "{:02X}h", sig.bytes[i]);
            }
            return result;
        }

        SigElement ParseSignature(std::string_view text)
        {
            SigElement sig;
            if (text.empty()) return sig;

            std::istringstream iss{std::string{text}};
            std::string token;

            while (iss >> token)
            {
                if (token == "??" || token == "?")
                {
                    sig.bytes.push_back(0);
                    sig.mask.push_back(false);
                }
                else
                {
                    std::string hex = token;
                    if (!hex.empty() && std::tolower(static_cast<unsigned char>(hex.back())) == 'h') hex.pop_back();

                    unsigned val = 0;
                    auto [ptr, ec] = std::from_chars(hex.data(), hex.data() + hex.size(), val, 16);
                    if (ec == std::errc() && val <= 0xFF)
                    {
                        sig.bytes.push_back(static_cast<uint8_t>(val));
                        sig.mask.push_back(true);
                    }
                    else
                    {
                        std::println(stderr, "[ParseSignature] 无法解析: '{}'", token);
                        sig.clear();
                        return sig;
                    }
                }
            }
            return sig;
        }

        bool MatchSignature(const uint8_t *data, const SigElement &sig)
        {
            for (size_t i = 0; i < sig.size(); ++i)
                if (sig.mask[i] && data[i] != sig.bytes[i]) return false;
            return true;
        }

        std::vector<uintptr_t> ScanCore(const SigElement &sig, int rangeOffset)
        {
            std::vector<uintptr_t> matches;
            if (sig.empty()) return matches;

            auto regions = dr->GetScanRegions();
            if (regions.empty()) return matches;

            const size_t sigSize = sig.size();
            std::vector<uint8_t> buffer(SIG_BUFFER_SIZE);
            const size_t step = (SIG_BUFFER_SIZE > sigSize) ? (SIG_BUFFER_SIZE - sigSize) : 1;

            for (const auto &[rStart, rEnd] : regions)
            {
                if (rEnd - rStart < sigSize) continue;

                for (uintptr_t addr = rStart; addr + sigSize <= rEnd; addr += step)
                {
                    size_t readSize = std::min(static_cast<size_t>(rEnd - addr), SIG_BUFFER_SIZE);
                    if (readSize < sigSize) break;
                    if (dr->Read(addr, buffer.data(), readSize) <= 0) continue;

                    size_t searchEnd = readSize - sigSize;
                    for (size_t off = 0; off <= searchEnd; ++off)
                    {
                        if (MatchSignature(buffer.data() + off, sig)) matches.push_back(addr + off + rangeOffset);
                    }
                }
            }
            return matches;
        }

        bool ReadSigFile(const char *filename, int &range, std::string &sigText)
        {
            std::ifstream fp(filename);
            if (!fp) return false;

            range = 0;
            sigText.clear();
            std::string line;

            while (std::getline(fp, line))
            {
                while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();

                if (line.starts_with("范围:"))
                {
                    auto sub = line.substr(line.find(':') + 1);
                    auto it = std::ranges::find_if(sub, [](char ch) { return std::isdigit(static_cast<unsigned char>(ch)); });
                    if (it != sub.end()) std::from_chars(&*it, sub.data() + sub.size(), range);
                }
                else if (line.starts_with("特征码:"))
                {
                    auto sub = line.substr(line.find(':') + 1);
                    if (auto f = sub.find_first_not_of(' '); f != std::string::npos) sigText = sub.substr(f);
                }
            }
            return (range > 0 && !sigText.empty());
        }

        bool WriteSigFile(const char *filename, uintptr_t addr, int range, const SigElement &sig)
        {
            std::ofstream fp(filename);
            if (!fp) return false;
            std::println(fp, "目标地址: 0x{:X}", addr);
            std::println(fp, "范围: {}", range);
            std::println(fp, "总字节: {}", sig.size());
            std::println(fp, "特征码: {}", FormatSignature(sig));
            return !fp.fail();
        }

        bool ReadSigFileWithFallback(const char *filename, int &range, std::string &sigText)
        {
            const std::string rawName = NormalizeSigFileName(filename);
            if (ReadSigFile(rawName.c_str(), range, sigText)) return true;
            if (!IsAbsoluteSigPath(rawName))
            {
                const std::string fallback = ResolveSigPath(rawName);
                return ReadSigFile(fallback.c_str(), range, sigText);
            }
            return false;
        }

        bool WriteSigFileWithFallback(const char *filename, uintptr_t addr, int range, const SigElement &sig)
        {
            const std::string rawName = NormalizeSigFileName(filename);
            if (WriteSigFile(rawName.c_str(), addr, range, sig)) return true;
            if (!IsAbsoluteSigPath(rawName))
            {
                const std::string fallback = ResolveSigPath(rawName);
                return WriteSigFile(fallback.c_str(), addr, range, sig);
            }
            return false;
        }
    } // namespace

    inline bool ScanAddressSignature(uintptr_t addr, int range, const char *filename = SIG_DEFAULT_FILE)
    {
        if (range <= 0 || range > SIG_MAX_RANGE)
        {
            std::println(stderr, "[找特征] range 无效: {} (1-{})", range, SIG_MAX_RANGE);
            return false;
        }
        if (addr < static_cast<uintptr_t>(range))
        {
            std::println(stderr, "[找特征] 地址过小会下溢");
            return false;
        }

        size_t totalSize = static_cast<size_t>(range) * 2;
        SigElement sig;
        sig.bytes.resize(totalSize);

        if (dr->Read(addr - range, sig.bytes.data(), totalSize) <= 0)
        {
            std::println(stderr, "[找特征] 读取失败: 0x{:X}", addr - range);
            return false;
        }

        sig.mask.assign(totalSize, true);

        if (!WriteSigFileWithFallback(filename, addr, range, sig))
        {
            std::println(stderr, "[找特征] 写文件失败: {}", filename);
            return false;
        }

        std::println("[找特征] 完成 地址:0x{:X} 范围:±{} 字节:{}", addr, range, totalSize);
        return true;
    }

    inline SigFilterResult FilterSignature(uintptr_t addr, const char *filename = SIG_DEFAULT_FILE)
    {
        SigFilterResult result;

        int range = 0;
        std::string oldSigText;
        if (!ReadSigFileWithFallback(filename, range, oldSigText))
        {
            std::println(stderr, "[过滤特征] 读取文件失败: {}", filename);
            return result;
        }

        SigElement oldSig = ParseSignature(oldSigText);
        if (oldSig.empty())
        {
            std::println(stderr, "[过滤特征] 特征码解析失败");
            return result;
        }

        if (addr < static_cast<uintptr_t>(range))
        {
            std::println(stderr, "[过滤特征] 地址过小");
            return result;
        }

        size_t totalSize = static_cast<size_t>(range) * 2;
        std::vector<uint8_t> curData(totalSize);

        if (dr->Read(addr - range, curData.data(), totalSize) <= 0)
        {
            std::println(stderr, "[过滤特征] 读取失败: 0x{:X}", addr - range);
            return result;
        }

        size_t cmpSize = std::min(oldSig.size(), curData.size());
        SigElement newSig;
        newSig.bytes.resize(cmpSize);
        newSig.mask.resize(cmpSize);
        result.totalCount = static_cast<int>(cmpSize);

        for (size_t i = 0; i < cmpSize; ++i)
        {
            if (!oldSig.mask[i])
            {
                newSig.bytes[i] = 0;
                newSig.mask[i] = false;
            }
            else if (oldSig.bytes[i] != curData[i])
            {
                newSig.bytes[i] = 0;
                newSig.mask[i] = false;
                ++result.changedCount;
            }
            else
            {
                newSig.bytes[i] = curData[i];
                newSig.mask[i] = true;
            }
        }

        result.oldSignature = oldSigText;
        result.newSignature = FormatSignature(newSig);

        WriteSigFileWithFallback(filename, addr, range, newSig);

        result.success = true;
        std::println("[过滤特征] 完成 总字节:{} 变化:{}", result.totalCount, result.changedCount);
        return result;
    }

    inline std::vector<uintptr_t> ScanSignature(const char *pattern, int range = 0)
    {
        SigElement sig = ParseSignature(pattern);
        if (sig.empty())
        {
            std::println(stderr, "[扫特征码] 解析失败");
            return {};
        }

        std::println("[扫特征码] 开始 长度:{} 偏移:{}", sig.size(), range);
        auto matches = ScanCore(sig, range);
        std::println("[扫特征码] 完成 找到 {} 个匹配", matches.size());
        return matches;
    }

    inline std::vector<uintptr_t> ScanSignatureFromFile(const char *filename = SIG_DEFAULT_FILE)
    {
        int range = 0;
        std::string sigText;
        if (!ReadSigFileWithFallback(filename, range, sigText))
        {
            std::println(stderr, "[扫特征码] 读取文件失败: {}", filename);
            return {};
        }

        SigElement sig = ParseSignature(sigText);
        if (sig.empty()) return {};

        std::println("[扫特征码] 开始 长度:{} 范围偏移:{}", sig.size(), range);
        auto matches = ScanCore(sig, range);
        std::println("[扫特征码] 完成 找到 {} 个匹配", matches.size());

        const std::string outPath = ResolveSigPath(NormalizeSigFileName(filename));
        std::ofstream out(outPath, std::ios::app);
        if (out)
        {
            std::println(out, "\n扫描结果: {} 个", matches.size());
            for (auto a : matches) std::println(out, "0x{:X}", a);
        }

        return matches;
    }
} // namespace SignatureScanner

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
        if (bits == 0 || bits > std::numeric_limits<size_t>::max() - 7) return false;
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
            if (tail) storage_.as<uint8_t>()[bytes - 1] = static_cast<uint8_t>((1u << tail) - 1);
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
    size_t totalBits() const noexcept
    {
        return totalBits_;
    }
    // 返回位图底层字节数组大小。
    size_t byteCount() const noexcept
    {
        return storage_.size();
    }
    // 判断位图底层存储是否可用。
    bool valid() const noexcept
    {
        return storage_.valid();
    }
    uint8_t *data() noexcept
    {
        return storage_.as<uint8_t>();
    }
    const uint8_t *data() const noexcept
    {
        return storage_.as<const uint8_t>();
    }

    // 读取指定位当前是否为 1。
    bool get(size_t i) const noexcept
    {
        uint8_t byte = __atomic_load_n(&data()[i / 8], __ATOMIC_RELAXED);
        return (byte >> (i % 8)) & 1;
    }

    // 把指定位设置为 1。
    void setOn(size_t i) noexcept
    {
        __atomic_fetch_or(&data()[i / 8], static_cast<uint8_t>(1u << (i % 8)), __ATOMIC_RELAXED);
    }

    // 把指定位清零为 0。
    void setOff(size_t i) noexcept
    {
        __atomic_fetch_and(&data()[i / 8], static_cast<uint8_t>(~(1u << (i % 8))), __ATOMIC_RELAXED);
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
        for (size_t i = 0; i < chunks; ++i) count += __builtin_popcountll(p64[i]);

        // 处理尾部
        for (size_t i = chunks * 8; i < bytes; ++i) count += __builtin_popcount(p[i]);

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

    struct ExplicitResult
    {
        uintptr_t address;
        std::uint64_t value;
    };

    // ── 核心状态 ──
    Bitmap bitmap_;
    MappedFile values_;
    std::vector<Region> regions_;
    std::vector<ExplicitResult> addedList_;

    size_t setBits_ = 0;
    size_t valueSize_ = 0;
    std::optional<Types::DataType> dataType_;
    Types::FuzzyMode scanMode_ = Types::FuzzyMode::Unknown;
    bool stringScan_ = false;

    mutable std::mutex operationMutex_;
    mutable std::shared_mutex mutex_;
    std::atomic<float> progress_{0.0f};
    std::atomic<bool> scanning_{false};

    struct ScanRunGuard
    {
        std::atomic<bool> &scanning;
        std::atomic<float> &progress;
        ~ScanRunGuard()
        {
            progress = 1.0f;
            scanning = false;
        }
    };

    template <typename HitBuckets> static Results mergeUniqueAddresses(HitBuckets &threadHits)
    {
        Results merged;
        size_t total = 0;
        for (auto &hits : threadHits) total += hits.size();
        merged.reserve(total);

        for (auto &hits : threadHits) merged.insert(merged.end(), hits.begin(), hits.end());
        std::sort(merged.begin(), merged.end());
        merged.erase(std::unique(merged.begin(), merged.end()), merged.end());
        return merged;
    }

    //  位 ↔ 地址映射
    size_t addrToBit(uintptr_t addr) const noexcept
    {
        // 二分查找所属区域
        auto it = std::upper_bound(regions_.begin(), regions_.end(), addr, [](uintptr_t a, const Region &r) { return a < r.end; });

        // upper_bound 找到第一个 end > addr 的区域
        if (it == regions_.end() || addr < it->start) return SIZE_MAX;

        size_t off = addr - it->start;
        if (off % valueSize_ != 0) return SIZE_MAX;

        size_t index = off / valueSize_;
        if (index >= it->bitCount) return SIZE_MAX;

        return it->bitOffset + index;
    }

    // 把位图索引换算为实际内存地址。
    uintptr_t bitToAddr(size_t gb) const noexcept
    {
        auto it = std::upper_bound(regions_.begin(), regions_.end(), gb, [](size_t b, const Region &r) { return b < r.bitOffset + r.bitCount; });
        if (it == regions_.end()) return 0;
        return it->start + (gb - it->bitOffset) * valueSize_;
    }

    // 位图初始化
    bool initStorage(size_t valSz, const std::vector<std::pair<uintptr_t, uintptr_t>> &scanRegs, bool allSet)
    {
        bitmap_.release();
        values_.release();
        regions_.clear();
        setBits_ = 0;
        valueSize_ = valSz;

        std::vector<std::pair<uintptr_t, uintptr_t>> normalized;
        normalized.reserve(scanRegs.size());
        for (const auto &[start, end] : scanRegs)
        {
            if (end > start && end - start >= valSz) normalized.emplace_back(start, end);
        }
        std::sort(normalized.begin(), normalized.end(), [](const auto &a, const auto &b) { return a.first < b.first || (a.first == b.first && a.second < b.second); });
        size_t write = 0;
        for (const auto &[start, end] : normalized)
        {
            if (write == 0 || start >= normalized[write - 1].second) normalized[write++] = {start, end};
            else if (end > normalized[write - 1].second) normalized[write - 1].second = end;
        }
        normalized.resize(write);

        size_t totalBits = 0;
        regions_.reserve(normalized.size());
        for (const auto &[start, end] : normalized)
        {
            const size_t bits = (end - start) / valSz;
            if (bits > std::numeric_limits<size_t>::max() - totalBits)
            {
                regions_.clear();
                valueSize_ = 0;
                return false;
            }
            regions_.push_back({start, end, totalBits, bits});
            totalBits += bits;
        }
        if (!totalBits || totalBits > std::numeric_limits<size_t>::max() / sizeof(std::uint64_t))
        {
            regions_.clear();
            valueSize_ = 0;
            return false;
        }

        if (!bitmap_.init(totalBits, allSet))
        {
            regions_.clear();
            valueSize_ = 0;
            return false;
        }

        size_t valBytes = totalBits * sizeof(std::uint64_t);
        if (!values_.allocate(valBytes))
        {
            bitmap_.release();
            regions_.clear();
            valueSize_ = 0;
            return false;
        }
        values_.advise(MADV_SEQUENTIAL);

        setBits_ = allSet ? totalBits : 0;
        return true;
    }

    std::uint64_t *valuesMap() noexcept
    {
        return values_.as<std::uint64_t>();
    }
    const std::uint64_t *valuesMap() const noexcept
    {
        return values_.as<const std::uint64_t>();
    }

    template <typename T> static std::uint64_t storeValue(T value) noexcept
    {
        static_assert(sizeof(T) <= sizeof(std::uint64_t));
        std::uint64_t stored = 0;
        std::memcpy(&stored, &value, sizeof(T));
        return stored;
    }

    template <typename T> static T loadValue(std::uint64_t stored) noexcept
    {
        static_assert(sizeof(T) <= sizeof(std::uint64_t));
        T value{};
        std::memcpy(&value, &stored, sizeof(T));
        return value;
    }

    static size_t scanRead(uintptr_t addr, uint8_t *buf, size_t size)
    {
        size_t done = 0;
        while (done < size)
        {
            const int result = dr->Read(addr + done, buf + done, size - done);
            if (result <= 0) break;
            const size_t read = std::min(static_cast<size_t>(result), size - done);
            if (read == 0) break;
            done += read;
        }
        return done;
    }

    static bool readStoredValue(uintptr_t addr, Types::DataType dataType, std::uint64_t &stored)
    {
        return MemUtils::DispatchType(dataType,
                                      [&]<typename T>()
                                      {
                                          T value{};
                                          if (scanRead(addr, reinterpret_cast<uint8_t *>(&value), sizeof(T)) != sizeof(T)) return false;
                                          if constexpr (std::is_floating_point_v<T>)
                                          {
                                              if (!MemUtils::IsValidFloat(value)) return false;
                                          }
                                          stored = storeValue(value);
                                          return true;
                                      });
    }

    // 并行线程分配
    unsigned threadCount() const
    {
        return std::max(1u, static_cast<unsigned>(std::min(Config::CpuThreadPool().get_thread_count(), regions_.size())));
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
            futs.push_back(Config::CpuThreadPool().submit_task(
                [&, t, chunk]
                {
                    size_t end = std::min(t * chunk + chunk, regions_.size());
                    std::vector<uint8_t> buf(Config::Constants::SCAN_BUFFER);

                    for (size_t ri = t * chunk; ri < end && Config::g_Running; ++ri)
                    {
                        auto &reg = regions_[ri];
                        for (uintptr_t addr = reg.start; addr < reg.end;)
                        {
                            size_t sz = std::min(static_cast<size_t>(reg.end - addr), Config::Constants::SCAN_BUFFER);
                            const size_t readBytes = scanRead(addr, buf.data(), sz);
                            size_t advance = sz;
                            if (readBytes < sz)
                            {
                                advance = (readBytes / valueSize_) * valueSize_;
                                if (advance == 0) advance = std::min(valueSize_, sz);
                            }
                            process(reg, buf.data(), addr, readBytes, advance);
                            addr += advance;
                        }
                        if ((done.fetch_add(1) & 0x3F) == 0) progress_ = static_cast<float>(done) / regions_.size();
                    }
                }));
        }
        for (auto &f : futs) f.get();
    }

    // 清除不可读范围对应的位标记。
    template <typename T>

    void clearUnreadableBits(const Region &reg, uintptr_t addr, size_t from, size_t to)
    {
        for (size_t off = from; off + sizeof(T) <= to; off += sizeof(T))
        {
            size_t gb = reg.bitOffset + (addr + off - reg.start) / sizeof(T);
            if (gb < bitmap_.totalBits() && bitmap_.get(gb)) bitmap_.setOff(gb);
        }
    }

    // ================================================================
    //  首扫 Unknown — bitmap 全 1 + 记录旧值
    // ================================================================
    template <typename T> void scanFirstUnknown(Types::DataType dataType)
    {
        auto scanRegs = dr->GetScanRegions();

        {
            std::unique_lock lock(mutex_);
            addedList_.clear();
            dataType_ = dataType;
            scanMode_ = Types::FuzzyMode::Unknown;
            stringScan_ = false;
            if (!initStorage(sizeof(T), scanRegs, true)) return;
        }

        parallelRegionScan(
            [this](const Region &reg, uint8_t *buf, uintptr_t addr, size_t readBytes, size_t sz)
            {
                if (readBytes == 0)
                {
                    clearUnreadableBits<T>(reg, addr, 0, sz);
                    return;
                }

                // 有效数据部分：记录值，过滤无效浮点
                for (size_t off = 0; off + sizeof(T) <= readBytes; off += sizeof(T))
                {
                    T value;
                    std::memcpy(&value, buf + off, sizeof(T));
                    size_t gb = reg.bitOffset + (addr + off - reg.start) / sizeof(T);

                    if constexpr (std::is_floating_point_v<T>)
                    {
                        if (!MemUtils::IsValidFloat(value))
                        {
                            if (gb < bitmap_.totalBits() && bitmap_.get(gb)) bitmap_.setOff(gb);
                            continue;
                        }
                    }
                    valuesMap()[gb] = storeValue(value);
                }

                // 不完整尾部：清除位
                size_t alignedEnd = readBytes & ~(sizeof(T) - 1);
                clearUnreadableBits<T>(reg, addr, alignedEnd, sz);
            });

        std::unique_lock lock(mutex_);
        setBits_ = bitmap_.popcount();
    }

    // ================================================================
    //  首扫有目标值
    // ================================================================
    template <typename T> void scanFirst(T target, Types::DataType dataType, Types::FuzzyMode mode, T rangeMax)
    {
        auto scanRegs = dr->GetScanRegions();

        {
            std::unique_lock lock(mutex_);
            addedList_.clear();
            dataType_ = dataType;
            scanMode_ = mode;
            stringScan_ = false;
            if (!initStorage(sizeof(T), scanRegs, false)) return;
        }

        // 每线程收集结果
        unsigned tc = threadCount();
        size_t chunk = (regions_.size() + tc - 1) / tc;
        std::atomic<size_t> done{0};

        struct HitEntry
        {
            uintptr_t addr;
            std::uint64_t val;
        };
        std::vector<std::deque<HitEntry>> threadHits(tc);

        std::vector<std::future<void>> futs;
        futs.reserve(tc);

        for (unsigned t = 0; t < tc; ++t)
        {
            futs.push_back(Config::CpuThreadPool().submit_task(
                [&, t, rangeMax, chunk]
                {
                    // 使用 scanRegs 而不是 regions_ 进行遍历
                    auto &myHits = threadHits[t];
                    std::vector<uint8_t> buf(Config::Constants::SCAN_BUFFER);
                    size_t end = std::min(t * chunk + chunk, regions_.size());

                    for (size_t ri = t * chunk; ri < end && Config::g_Running; ++ri)
                    {
                        auto &reg = regions_[ri];
                        for (uintptr_t addr = reg.start; addr < reg.end;)
                        {
                            size_t sz = std::min(static_cast<size_t>(reg.end - addr), Config::Constants::SCAN_BUFFER);
                            const size_t usable = scanRead(addr, buf.data(), sz);
                            for (size_t off = 0; off + sizeof(T) <= usable; off += sizeof(T))
                            {
                                T value;
                                std::memcpy(&value, buf.data() + off, sizeof(T));

                                if constexpr (std::is_floating_point_v<T>)
                                {
                                    if (!MemUtils::IsValidFloat(value)) continue;
                                }

                                if (MemUtils::Compare(value, target, mode, T{}, rangeMax))
                                {
                                    myHits.push_back({addr + off, storeValue(value)});
                                }
                            }
                            size_t advance = sz;
                            if (usable < sz)
                            {
                                advance = (usable / sizeof(T)) * sizeof(T);
                                if (advance == 0) advance = std::min(sizeof(T), sz);
                            }
                            addr += advance;
                        }
                        if ((done.fetch_add(1) & 0x7F) == 0) progress_ = static_cast<float>(done) / regions_.size();
                    }
                }));
        }
        for (auto &f : futs) f.get();

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
                    if (!bitmap_.get(gb))
                    {
                        bitmap_.setOn(gb);
                        ++actualSet;
                    }
                    valuesMap()[gb] = val;
                }
            }
        }
        setBits_ = actualSet;
    }

    // ================================================================
    //  二次扫描
    // ================================================================
    template <typename T> void scanNext(T target, Types::FuzzyMode mode, T rangeMax)
    {
        std::atomic<size_t> survived{0};

        parallelRegionScan(
            [&, rangeMax](const Region &reg, uint8_t *buf, uintptr_t addr, size_t readBytes, size_t sz)
            {
                if (readBytes == 0)
                {
                    clearUnreadableBits<T>(reg, addr, 0, sz);
                    return;
                }

                // 有效数据部分
                for (size_t off = 0; off + sizeof(T) <= readBytes; off += sizeof(T))
                {
                    size_t gb = reg.bitOffset + (addr + off - reg.start) / sizeof(T);
                    if (!bitmap_.get(gb)) continue;

                    T value;
                    std::memcpy(&value, buf + off, sizeof(T));

                    // 浮点值/旧值有效性检查
                    if constexpr (std::is_floating_point_v<T>)
                    {
                        if (!MemUtils::IsValidFloat(value))
                        {
                            bitmap_.setOff(gb);
                            continue;
                        }
                        const T oldVal = loadValue<T>(valuesMap()[gb]);
                        if (std::isnan(oldVal) || std::isinf(oldVal))
                        {
                            bitmap_.setOff(gb);
                            continue;
                        }
                    }

                    const T oldVal = loadValue<T>(valuesMap()[gb]);
                    if (MemUtils::Compare(value, target, mode, oldVal, rangeMax))
                    {
                        valuesMap()[gb] = storeValue(value);
                        survived.fetch_add(1, std::memory_order_relaxed);
                    }
                    else
                    {
                        bitmap_.setOff(gb);
                    }
                }

                // 不完整尾部
                size_t alignedEnd = readBytes & ~(sizeof(T) - 1);
                clearUnreadableBits<T>(reg, addr, alignedEnd, sz);
            });

        std::vector<ExplicitResult> explicitSurvivors;
        explicitSurvivors.reserve(addedList_.size());
        for (const auto &entry : addedList_)
        {
            T value{};
            if (scanRead(entry.address, reinterpret_cast<uint8_t *>(&value), sizeof(T)) != sizeof(T)) continue;
            if constexpr (std::is_floating_point_v<T>)
            {
                if (!MemUtils::IsValidFloat(value)) continue;
            }

            const T oldValue = loadValue<T>(entry.value);
            if (MemUtils::Compare(value, target, mode, oldValue, rangeMax)) explicitSurvivors.push_back({entry.address, storeValue(value)});
        }

        std::unique_lock lock(mutex_);
        setBits_ = survived.load();
        addedList_.swap(explicitSurvivors);
        scanMode_ = mode;
    }

    void scanFirstString(const std::string &needle)
    {
        if (needle.empty() || needle.size() > Config::Constants::SCAN_BUFFER) return;

        {
            std::unique_lock lock(mutex_);
            bitmap_.release();
            values_.release();
            regions_.clear();
            setBits_ = 0;
            valueSize_ = 0;
            addedList_.clear();
            dataType_.reset();
            scanMode_ = Types::FuzzyMode::String;
            stringScan_ = true;
        }

        auto scanRegs = dr->GetScanRegions();
        std::erase_if(scanRegs, [&](const auto &region) { return region.second <= region.first || region.second - region.first < needle.size(); });
        std::sort(scanRegs.begin(), scanRegs.end(), [](const auto &a, const auto &b) { return a.first < b.first || (a.first == b.first && a.second < b.second); });
        size_t write = 0;
        for (const auto &[start, end] : scanRegs)
        {
            if (write == 0 || start >= scanRegs[write - 1].second) scanRegs[write++] = {start, end};
            else if (end > scanRegs[write - 1].second) scanRegs[write - 1].second = end;
        }
        scanRegs.resize(write);
        if (scanRegs.empty()) return;

        const size_t patLen = needle.size();

        unsigned tc = std::max(1u, static_cast<unsigned>(std::min(Config::CpuThreadPool().get_thread_count(), scanRegs.size())));
        size_t chunk = (scanRegs.size() + tc - 1) / tc;
        std::atomic<size_t> done{0};

        std::vector<std::deque<uintptr_t>> threadHits(tc);
        std::vector<std::future<void>> futs;
        futs.reserve(tc);

        const size_t step = (Config::Constants::SCAN_BUFFER > patLen) ? (Config::Constants::SCAN_BUFFER - patLen + 1) : 1;

        for (unsigned t = 0; t < tc; ++t)
        {
            futs.push_back(Config::CpuThreadPool().submit_task(
                [&, t]
                {
                    auto &myHits = threadHits[t];
                    std::vector<uint8_t> buf(Config::Constants::SCAN_BUFFER);
                    size_t end = std::min(t * chunk + chunk, scanRegs.size());

                    for (size_t ri = t * chunk; ri < end && Config::g_Running; ++ri)
                    {
                        auto [start, finish] = scanRegs[ri];
                        if (finish <= start || static_cast<size_t>(finish - start) < patLen)
                        {
                            if ((done.fetch_add(1) & 0x3F) == 0) progress_ = static_cast<float>(done) / scanRegs.size();
                            continue;
                        }

                        for (uintptr_t addr = start; static_cast<size_t>(finish - addr) >= patLen;)
                        {
                            size_t readSize = std::min(static_cast<size_t>(finish - addr), Config::Constants::SCAN_BUFFER);
                            const size_t usable = scanRead(addr, buf.data(), readSize);
                            if (usable > 0)
                            {
                                if (usable >= patLen)
                                {
                                    size_t uniqueLimit = (addr + step < finish) ? std::min(step, usable) : usable;
                                    for (size_t off = 0; off + patLen <= usable && off < uniqueLimit; ++off)
                                    {
                                        if (std::memcmp(buf.data() + off, needle.data(), patLen) == 0) myHits.push_back(addr + off);
                                    }
                                }
                            }

                            size_t advance = step;
                            if (usable < readSize)
                            {
                                advance = usable >= patLen ? usable - patLen + 1 : usable;
                                if (advance == 0) advance = 1;
                            }
                            if (advance == 0 || advance >= static_cast<size_t>(finish - addr)) break;
                            addr += advance;
                        }

                        if ((done.fetch_add(1) & 0x3F) == 0) progress_ = static_cast<float>(done) / scanRegs.size();
                    }
                }));
        }

        for (auto &f : futs) f.get();

        auto merged = mergeUniqueAddresses(threadHits);

        std::unique_lock lock(mutex_);
        addedList_.clear();
        addedList_.reserve(merged.size());
        for (uintptr_t address : merged) addedList_.push_back({address, 0});
        setBits_ = 0;
        scanMode_ = Types::FuzzyMode::String;
        stringScan_ = true;
    }

    void scanNextString(const std::string &needle)
    {
        if (needle.empty()) return;

        std::vector<uintptr_t> current;
        {
            std::shared_lock lock(mutex_);
            current.reserve(addedList_.size());
            for (const auto &entry : addedList_) current.push_back(entry.address);
        }
        if (current.empty()) return;

        const size_t patLen = needle.size();
        unsigned tc = std::max(1u, static_cast<unsigned>(std::min(Config::CpuThreadPool().get_thread_count(), current.size())));
        size_t chunk = (current.size() + tc - 1) / tc;
        std::atomic<size_t> done{0};

        std::vector<std::vector<uintptr_t>> threadHits(tc);
        std::vector<std::future<void>> futs;
        futs.reserve(tc);

        for (unsigned t = 0; t < tc; ++t)
        {
            futs.push_back(Config::CpuThreadPool().submit_task(
                [&, t]
                {
                    auto &myHits = threadHits[t];
                    std::vector<uint8_t> buf(patLen);
                    size_t end = std::min(t * chunk + chunk, current.size());
                    for (size_t i = t * chunk; i < end && Config::g_Running; ++i)
                    {
                        uintptr_t addr = current[i];
                        const size_t readBytes = scanRead(addr, buf.data(), patLen);
                        if (readBytes >= patLen && std::memcmp(buf.data(), needle.data(), patLen) == 0)
                        {
                            myHits.push_back(addr);
                        }

                        size_t finished = done.fetch_add(1) + 1;
                        if ((finished & 0x3FF) == 0) progress_ = static_cast<float>(finished) / current.size();
                    }
                }));
        }
        for (auto &f : futs) f.get();

        auto merged = mergeUniqueAddresses(threadHits);

        std::unique_lock lock(mutex_);
        addedList_.clear();
        addedList_.reserve(merged.size());
        for (uintptr_t address : merged) addedList_.push_back({address, 0});
        setBits_ = 0;
        scanMode_ = Types::FuzzyMode::String;
    }

    template <typename T> void runScan(T target, Types::DataType dataType, Types::FuzzyMode mode, bool isFirst, T rangeMax)
    {
        std::lock_guard operationLock(operationMutex_);
        ScanRunGuard guard{scanning_, progress_};
        progress_ = 0.0f;

        if (isFirst)
        {
            if (mode == Types::FuzzyMode::Unknown) scanFirstUnknown<T>(dataType);
            else scanFirst<T>(target, dataType, mode, rangeMax);
        }
        else
        {
            scanNext<T>(target, mode, rangeMax);
        }
    }

    void runStringScan(const std::string &needle, bool isFirst)
    {
        std::lock_guard operationLock(operationMutex_);
        ScanRunGuard guard{scanning_, progress_};
        progress_ = 0.0f;
        if (isFirst) scanFirstString(needle);
        else scanNextString(needle);
    }

public:
    struct State
    {
        bool scanning;
        float progress;
        size_t count;
        std::optional<Types::DataType> dataType;
        Types::FuzzyMode mode;
        bool stringScan;
    };

    struct PageState
    {
        State state;
        Results results;
    };

    MemScanner() = default;
    ~MemScanner() = default; // RAII handles cleanup
    MemScanner(const MemScanner &) = delete;
    MemScanner &operator=(const MemScanner &) = delete;

    // 返回扫描线程当前是否在运行。
    bool isScanning() const noexcept
    {
        return scanning_;
    }
    // 返回当前扫描进度百分比(0~1)。
    float progress() const noexcept
    {
        return progress_;
    }

    State state() const
    {
        std::shared_lock lock(mutex_);
        return {scanning_.load(), progress_.load(), setBits_ + addedList_.size(), dataType_, scanMode_, stringScan_};
    }

    PageState pageState(size_t start, size_t cnt) const
    {
        std::lock_guard operationLock(operationMutex_);
        std::shared_lock lock(mutex_);

        PageState snapshot{{scanning_.load(), progress_.load(), setBits_ + addedList_.size(), dataType_, scanMode_, stringScan_}, {}};
        if (snapshot.state.scanning || snapshot.state.count == 0) return snapshot;

        snapshot.results.reserve(cnt);
        size_t skipped = 0;

        for (const auto &entry : addedList_)
        {
            if (snapshot.results.size() >= cnt) break;
            if (skipped++ < start) continue;
            snapshot.results.push_back(entry.address);
        }

        if (snapshot.results.size() < cnt && bitmap_.valid() && setBits_ > 0)
        {
            for (const auto &reg : regions_)
            {
                if (snapshot.results.size() >= cnt) break;
                size_t byteS = reg.bitOffset / 8;
                size_t byteE = (reg.bitOffset + reg.bitCount + 7) / 8;

                for (size_t b = byteS; b < byteE && snapshot.results.size() < cnt; ++b)
                {
                    uint8_t byte = bitmap_.data()[b];
                    if (!byte) continue;

                    for (int bit = 0; bit < 8 && snapshot.results.size() < cnt; ++bit)
                    {
                        if (!(byte & (1 << bit))) continue;
                        size_t gb = b * 8 + bit;
                        if (gb < reg.bitOffset || gb >= reg.bitOffset + reg.bitCount) continue;
                        if (skipped++ < start) continue;
                        snapshot.results.push_back(bitToAddr(gb));
                    }
                }
            }
        }
        return snapshot;
    }

    std::optional<Types::DataType> dataType() const
    {
        std::shared_lock lock(mutex_);
        return dataType_;
    }

    Types::FuzzyMode scanMode() const
    {
        std::shared_lock lock(mutex_);
        return scanMode_;
    }

    bool isStringScan() const
    {
        std::shared_lock lock(mutex_);
        return stringScan_;
    }

    // 返回当前结果数量。
    size_t count() const
    {
        std::shared_lock lock(mutex_);
        return setBits_ + addedList_.size();
    }

    // 结果分页获取
    Results getPage(size_t start, size_t cnt) const
    {
        if (scanning_) return {};
        std::lock_guard operationLock(operationMutex_);
        if (scanning_) return {};
        std::shared_lock lock(mutex_);
        if (setBits_ == 0 && addedList_.empty()) return {};

        Results r;
        r.reserve(cnt);
        size_t skipped = 0;

        // 手动添加列表
        for (size_t i = 0; i < addedList_.size() && r.size() < cnt; ++i)
        {
            if (skipped++ < start) continue;
            r.push_back(addedList_[i].address);
        }

        // 位图结果
        if (r.size() < cnt && bitmap_.valid() && setBits_ > 0)
        {
            for (const auto &reg : regions_)
            {
                if (r.size() >= cnt) break;
                size_t byteS = reg.bitOffset / 8;
                size_t byteE = (reg.bitOffset + reg.bitCount + 7) / 8;

                for (size_t b = byteS; b < byteE && r.size() < cnt; ++b)
                {
                    uint8_t byte = bitmap_.data()[b];
                    if (!byte) continue;

                    for (int bit = 0; bit < 8 && r.size() < cnt; ++bit)
                    {
                        if (!(byte & (1 << bit))) continue;
                        size_t gb = b * 8 + bit;
                        if (gb < reg.bitOffset || gb >= reg.bitOffset + reg.bitCount) continue;
                        if (skipped++ < start) continue;
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
        if (scanning_) return;
        std::lock_guard operationLock(operationMutex_);
        if (scanning_) return;
        std::unique_lock lock(mutex_);
        bitmap_.release();
        values_.release();
        regions_.clear();
        addedList_.clear();
        setBits_ = 0;
        valueSize_ = 0;
        dataType_.reset();
        scanMode_ = Types::FuzzyMode::Unknown;
        stringScan_ = false;
    }

    // 单项操作
    void remove(uintptr_t addr)
    {
        if (scanning_) return;
        std::lock_guard operationLock(operationMutex_);
        if (scanning_) return;
        std::unique_lock lock(mutex_);
        auto it = std::find_if(addedList_.begin(), addedList_.end(), [addr](const ExplicitResult &entry) { return entry.address == addr; });
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
        if (scanning_) return;
        std::lock_guard operationLock(operationMutex_);
        if (scanning_) return;
        std::unique_lock lock(mutex_);
        size_t gb = addrToBit(addr);
        if (gb != SIZE_MAX)
        {
            if (!bitmap_.get(gb))
            {
                if (!dataType_) return;
                std::uint64_t stored = 0;
                if (!readStoredValue(addr, *dataType_, stored)) return;
                bitmap_.setOn(gb);
                valuesMap()[gb] = stored;
                ++setBits_;
            }
        }
        else
        {
            if (std::ranges::none_of(addedList_, [addr](const ExplicitResult &entry) { return entry.address == addr; }))
            {
                std::uint64_t stored = 0;
                if (stringScan_ || (dataType_ && readStoredValue(addr, *dataType_, stored))) addedList_.push_back({addr, stored});
            }
        }
    }

    void applyOffset(uintptr_t offset, bool negative)
    {
        if (scanning_) return;
        std::lock_guard operationLock(operationMutex_);
        if (scanning_) return;
        std::unique_lock lock(mutex_);

        auto apply = [offset, negative](uintptr_t address) -> std::optional<uintptr_t>
        {
            if (negative)
            {
                if (address < offset) return std::nullopt;
                return address - offset;
            }
            if (address > std::numeric_limits<uintptr_t>::max() - offset) return std::nullopt;
            return address + offset;
        };

        std::vector<uintptr_t> shifted;
        shifted.reserve(setBits_ + addedList_.size());
        for (const auto &entry : addedList_)
        {
            if (const auto address = apply(entry.address)) shifted.push_back(*address);
        }

        if (bitmap_.valid() && setBits_ > 0)
        {
            for (const auto &reg : regions_)
            {
                size_t byteS = reg.bitOffset / 8;
                size_t byteE = (reg.bitOffset + reg.bitCount + 7) / 8;
                for (size_t b = byteS; b < byteE; ++b)
                {
                    uint8_t byte = bitmap_.data()[b];
                    if (!byte) continue;
                    for (int bit = 0; bit < 8; ++bit)
                    {
                        if (!(byte & (1 << bit))) continue;
                        size_t gb = b * 8 + bit;
                        if (gb < reg.bitOffset || gb >= reg.bitOffset + reg.bitCount) continue;
                        if (const auto address = apply(bitToAddr(gb))) shifted.push_back(*address);
                    }
                }
            }
        }

        std::sort(shifted.begin(), shifted.end());
        shifted.erase(std::unique(shifted.begin(), shifted.end()), shifted.end());

        std::vector<ExplicitResult> replacement;
        replacement.reserve(shifted.size());
        for (uintptr_t address : shifted)
        {
            std::uint64_t stored = 0;
            uint8_t readable = 0;
            if ((stringScan_ && address != 0 && scanRead(address, &readable, sizeof(readable)) == sizeof(readable)) || (dataType_ && readStoredValue(address, *dataType_, stored)))
            {
                replacement.push_back({address, stored});
            }
        }

        bitmap_.release();
        values_.release();
        regions_.clear();
        addedList_.swap(replacement);
        setBits_ = 0;
    }

    template <typename T> bool startAsync(pid_t pid, T target, Types::DataType dataType, Types::FuzzyMode mode, bool isFirst, T rangeMax = T{})
    {
        std::lock_guard targetLock(Config::TargetMutex);
        std::unique_lock lock(mutex_);
        if (scanning_.exchange(true)) return false;

        Types::DataType actualType;
        if constexpr (std::is_same_v<T, int8_t>) actualType = Types::DataType::I8;
        else if constexpr (std::is_same_v<T, int16_t>) actualType = Types::DataType::I16;
        else if constexpr (std::is_same_v<T, int32_t>) actualType = Types::DataType::I32;
        else if constexpr (std::is_same_v<T, int64_t>) actualType = Types::DataType::I64;
        else if constexpr (std::is_same_v<T, float>) actualType = Types::DataType::Float;
        else actualType = Types::DataType::Double;

        bool accepted = pid > 0 && pid == dr->GetGlobalPid() && dataType == actualType && mode != Types::FuzzyMode::String && (mode != Types::FuzzyMode::Pointer || dataType == Types::DataType::I64);
        if (isFirst)
        {
            accepted = accepted && mode != Types::FuzzyMode::Increased && mode != Types::FuzzyMode::Decreased && mode != Types::FuzzyMode::Changed && mode != Types::FuzzyMode::Unchanged;
        }
        else
        {
            accepted = accepted && dataType_.has_value() && *dataType_ == dataType && !stringScan_ && mode != Types::FuzzyMode::Unknown;
        }
        if (!accepted)
        {
            scanning_ = false;
            return false;
        }

        if (isFirst)
        {
            bitmap_.release();
            values_.release();
            regions_.clear();
            addedList_.clear();
            setBits_ = 0;
            valueSize_ = 0;
        }
        dataType_ = dataType;
        scanMode_ = mode;
        stringScan_ = false;

        progress_ = 0.0f;
        lock.unlock();
        try
        {
            Config::IoThreadPool().detach_task([this, target, dataType, mode, isFirst, rangeMax] { runScan(target, dataType, mode, isFirst, rangeMax); });
        }
        catch (...)
        {
            scanning_ = false;
            return false;
        }
        return true;
    }

    bool startStringAsync(pid_t pid, std::string needle, bool isFirst)
    {
        std::lock_guard targetLock(Config::TargetMutex);
        if (pid <= 0 || pid != dr->GetGlobalPid() || needle.empty() || needle.size() > Config::Constants::SCAN_BUFFER) return false;
        std::unique_lock lock(mutex_);
        if (scanning_.exchange(true)) return false;

        bool accepted = isFirst || stringScan_;
        if (!accepted)
        {
            scanning_ = false;
            return false;
        }

        if (isFirst)
        {
            bitmap_.release();
            values_.release();
            regions_.clear();
            addedList_.clear();
            setBits_ = 0;
            valueSize_ = 0;
        }
        dataType_.reset();
        scanMode_ = Types::FuzzyMode::String;
        stringScan_ = true;

        progress_ = 0.0f;
        lock.unlock();
        try
        {
            Config::IoThreadPool().detach_task([this, needle = std::move(needle), isFirst] { runStringScan(needle, isFirst); });
        }
        catch (...)
        {
            scanning_ = false;
            return false;
        }
        return true;
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
        return std::ranges::find_if(locks_, [addr](auto &i) { return i.addr == addr; });
    }

    // 后台循环写入被锁定的内存项。
    void writeLoop()
    {
        while (!writeStop_.load(std::memory_order_acquire) && Config::g_Running)
        {
            std::vector<LockItem> snapshot;
            {
                std::lock_guard lock(mutex_);
                snapshot.assign(locks_.begin(), locks_.end());
            }
            for (const auto &item : snapshot) MemUtils::WriteFromString(item.addr, item.type, item.value);
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
        }
    }

public:
    LockManager()
    {
        writeTask_ = Config::IoThreadPool().submit_task([this] { writeLoop(); });
    }

    ~LockManager()
    {
        writeStop_.store(true, std::memory_order_release);
        if (writeTask_.valid()) writeTask_.wait();
    }

    // 判断目标地址是否处于锁定状态。
    bool isLocked(uintptr_t addr) const
    {
        std::lock_guard lock(mutex_);
        return std::ranges::any_of(locks_, [addr](const auto &i) { return i.addr == addr; });
    }

    // 切换目标地址的锁定状态。
    void toggle(uintptr_t addr, Types::DataType type)
    {
        std::lock_guard lock(mutex_);
        if (auto it = find(addr); it != locks_.end()) locks_.erase(it);
        else locks_.push_back({addr, type, MemUtils::ReadAsString(addr, type)});
    }

    // 锁定指定地址并记录目标值。
    void lock(uintptr_t addr, Types::DataType type, const std::string &value)
    {
        std::lock_guard lk(mutex_);
        if (find(addr) == locks_.end()) locks_.push_back({addr, type, value});
    }

    // 取消指定地址的锁定。
    void unlock(uintptr_t addr)
    {
        std::lock_guard lk(mutex_);
        std::erase_if(locks_, [addr](const auto &item) { return item.addr == addr; });
    }

    // 批量锁定一组地址。
    void lockBatch(std::span<const uintptr_t> addrs, Types::DataType type)
    {
        std::lock_guard lk(mutex_);
        for (auto addr : addrs)
        {
            if (find(addr) == locks_.end()) locks_.emplace_back(addr, type, MemUtils::ReadAsString(addr, type));
        }
    }

    // 批量取消锁定一组地址。
    void unlockBatch(std::span<const uintptr_t> addrs)
    {
        std::lock_guard lk(mutex_);
        for (auto addr : addrs) std::erase_if(locks_, [addr](const auto &item) { return item.addr == addr; });
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
    Types::ViewFormat format_ = Types::ViewFormat::Hexadecimal;
    std::vector<uint8_t> buffer_;
    bool readSuccess_ = false;
    std::vector<Disasm::DisasmLine> disasmCache_;
    std::future<std::vector<Disasm::DisasmLine>> disasmFuture_;
    bool disasmBusy_ = false;

public:
    MemViewer() : buffer_(Config::Constants::MEM_VIEW_DEFAULT_BYTES)
    {
    }

    // 返回当前内存浏览格式。
    Types::ViewFormat format() const noexcept
    {
        return format_;
    }
    // 返回最近一次读取是否成功。
    bool readSuccess() const noexcept
    {
        return readSuccess_;
    }
    // 返回当前浏览基址。
    uintptr_t base() const noexcept
    {
        return base_;
    }
    const std::vector<uint8_t> &buffer() const noexcept
    {
        return buffer_;
    }
    const std::vector<Disasm::DisasmLine> &getDisasm() const noexcept
    {
        return disasmCache_;
    }
    bool disasmBusy() const noexcept
    {
        return disasmBusy_;
    }

    void pollDisasm()
    {
        if (disasmFuture_.valid() && disasmFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) waitDisasm();
    }

    // 切换浏览格式并触发刷新。
    void waitDisasm()
    {
        if (!disasmFuture_.valid()) return;

        try
        {
            disasmCache_ = disasmFuture_.get();
        }
        catch (...)
        {
            disasmCache_.clear();
        }
        disasmBusy_ = false;
    }

    void setFormat(Types::ViewFormat fmt)
    {
        format_ = fmt;
        refresh();
    }

    void clear()
    {
        waitDisasm();
        base_ = 0;
        readSuccess_ = false;
        std::ranges::fill(buffer_, 0);
        disasmCache_.clear();
    }

    // 打开指定地址并初始化浏览状态。
    void open(uintptr_t addr, std::optional<Types::ViewFormat> format = std::nullopt)
    {
        if (format.has_value()) format_ = *format;
        if (format_ == Types::ViewFormat::Disasm) addr &= ~static_cast<uintptr_t>(3); // 强制 4 字节对齐
        base_ = addr;
        refresh();
    }

    // 重新读取并刷新当前浏览缓存。
    void refresh()
    {
        if (base_ > Config::Constants::ADDR_MAX)
        {
            readSuccess_ = false;
            disasmBusy_ = false;
            disasmCache_.clear();
            return;
        }
        std::ranges::fill(buffer_, 0);
        const int readBytes = dr->Read(base_, buffer_.data(), buffer_.size());
        readSuccess_ = readBytes > 0;
        if (!readSuccess_)
        {
            disasmBusy_ = false;
            disasmCache_.clear();
            return;
        }
        if (format_ == Types::ViewFormat::Disasm)
        {
            disasmCache_.clear();
            disasmBusy_ = false;
            const size_t disasmSize = std::min(static_cast<size_t>(readBytes), buffer_.size()) & ~static_cast<size_t>(3);
            if (disasmSize > 0)
            {
                auto base = base_;
                auto bytes = buffer_;
                try
                {
                    disasmFuture_ = Config::CpuThreadPool().submit_task(
                        [base, bytes = std::move(bytes), disasmSize]() mutable
                        {
                            Disasm::Disassembler disasm;
                            if (!disasm.IsValid()) return std::vector<Disasm::DisasmLine>{};
                            return disasm.Disassemble(base, bytes.data(), disasmSize, Disasm::Disassembler::DEFAULT_MAX_INSTRUCTIONS, true);
                        });
                    disasmBusy_ = true;
                }
                catch (...)
                {
                    disasmCache_.clear();
                }
            }
        }
        else
        {
            disasmBusy_ = false;
            disasmCache_.clear();
        }
    }

    // 按指定方向应用无符号字节偏移。
    void applyOffset(uintptr_t offset, bool negative)
    {
        if (negative) base_ = base_ < offset ? 0 : base_ - offset;
        else base_ += offset;
        if (format_ == Types::ViewFormat::Disasm) base_ &= ~static_cast<uintptr_t>(3);
        refresh();
    }

    // 按有符号字节偏移调整当前浏览基址。
    void applyOffset(int64_t offset)
    {
        const auto magnitude = offset < 0 ? static_cast<uint64_t>(-(offset + 1)) + 1 : static_cast<uint64_t>(offset);
        applyOffset(static_cast<uintptr_t>(magnitude), offset < 0);
    }

    // 解析偏移字符串后调整当前浏览基址。
    bool applyOffset(std::string_view offsetStr)
    {
        auto result = MemUtils::ParseHexOffset(offsetStr);
        if (!result) return false;
        applyOffset(result->offset, result->negative);
        return true;
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
        PtrData() : address(0), value(0)
        {
        }
        PtrData(uintptr_t a, uintptr_t v) : address(a), value(v)
        {
        }
    };

    struct PtrDir
    {
        uintptr_t address, value;
        uint32_t start, end;
        PtrDir() : address(0), value(0), start(0), end(0)
        {
        }
        PtrDir(uintptr_t a, uintptr_t v, uint32_t s = 0, uint32_t e = 0) : address(a), value(v), start(s), end(e)
        {
        }
    };

    enum class BaseMode : uint8_t
    {
        Module = 0,
        Manual,
        Array
    };

    struct BaseRange
    {
        BaseMode mode = BaseMode::Module;
        uintptr_t start = 0;
        uintptr_t end = 0;
        uint64_t sourceId = 0;
        std::string name;
        int segment = 0;
        bool isBss = false;
        uintptr_t arrayBase = 0;
        size_t arrayIndex = 0;
    };

    struct PtrRange
    {
        int level = 0;
        BaseRange base;
        std::vector<PtrDir> results;
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
        uint64_t sourceId;
        uint64_t manualBase;
        uint64_t arrayBase;
        uint64_t arrayIndex;
    };

    struct BinLevel
    {
        unsigned int count;
        int level;
    };

    static constexpr int kBinVersion = 103;

    enum class Operation : uint8_t
    {
        Idle = 0,
        Scanning,
        Merging,
        Exporting
    };

    struct State
    {
        Operation operation = Operation::Idle;
        Operation lastOperation = Operation::Idle;
        float progress = 0.0f;
        size_t count = 0;
        bool completed = false;
        bool success = false;
        std::string error;
    };

private:
    struct FileCloser
    {
        void operator()(FILE *file) const noexcept
        {
            if (file) fclose(file);
        }
    };

    using FilePtr = std::unique_ptr<FILE, FileCloser>;

    std::mutex block_mtx_;
    std::condition_variable block_cv_;
    std::vector<PtrData> pointers_;
    std::vector<std::pair<uintptr_t, uintptr_t>> regions_;
    std::vector<BaseRange> moduleBases_;
    std::atomic<Operation> operation_{Operation::Idle};
    std::atomic<Operation> lastOperation_{Operation::Idle};
    std::atomic<float> scanProgress_{0.0f};
    std::atomic<size_t> chainCount_{0};
    std::atomic<bool> completed_{false};
    std::atomic<bool> success_{false};
    mutable std::mutex statusMutex_;
    std::string lastError_;

    bool beginOperation(Operation operation)
    {
        std::lock_guard lock(statusMutex_);
        Operation expected = Operation::Idle;
        if (!operation_.compare_exchange_strong(expected, operation, std::memory_order_acq_rel)) return false;
        lastOperation_.store(operation, std::memory_order_release);
        scanProgress_.store(0.0f, std::memory_order_release);
        chainCount_.store(0, std::memory_order_release);
        completed_.store(false, std::memory_order_release);
        success_.store(false, std::memory_order_release);
        lastError_.clear();
        return true;
    }

    void finishOperation(Operation operation, bool success, std::string error = {}) noexcept
    {
        try
        {
            std::lock_guard lock(statusMutex_);
            lastError_ = success ? std::string() : std::move(error);
        }
        catch (...)
        {
            success = false;
        }

        success_.store(success, std::memory_order_release);
        completed_.store(true, std::memory_order_release);
        scanProgress_.store(1.0f, std::memory_order_release);
        Operation expected = operation;
        if (!operation_.compare_exchange_strong(expected, Operation::Idle, std::memory_order_acq_rel)) operation_.store(Operation::Idle, std::memory_order_release);
    }

    static uint64_t HashSource(std::string_view text)
    {
        uint64_t hash = 1469598103934665603ULL;
        for (const unsigned char ch : text)
        {
            hash ^= ch;
            hash *= 1099511628211ULL;
        }
        return hash;
    }

    static uintptr_t OffsetEnd(uintptr_t base, size_t offset)
    {
        const uintptr_t maxAddress = Config::Constants::ADDR_MAX - 1;
        const uintptr_t last = offset > maxAddress - base ? maxAddress : base + offset;
        return last + 1;
    }

    static size_t SaturatingAdd(size_t left, size_t right)
    {
        return right > std::numeric_limits<size_t>::max() - left ? std::numeric_limits<size_t>::max() : left + right;
    }

    static void WaitFutures(std::vector<std::future<void>> &futures)
    {
        std::exception_ptr firstError;
        for (auto &future : futures)
        {
            try
            {
                if (future.valid()) future.get();
            }
            catch (...)
            {
                if (!firstError) firstError = std::current_exception();
            }
        }
        futures.clear();
        if (firstError) std::rethrow_exception(firstError);
    }

    template <typename F> void RunOperation(Operation operation, std::string_view fallbackError, F &&task) noexcept
    {
        bool succeeded = false;
        std::string error;
        struct Guard
        {
            PointerManager &owner;
            Operation operation;
            bool &succeeded;
            std::string &error;
            ~Guard()
            {
                owner.finishOperation(operation, succeeded, std::move(error));
            }
        } guard{*this, operation, succeeded, error};

        try
        {
            error = fallbackError;
            task(succeeded, error);
        }
        catch (const std::exception &ex)
        {
            try
            {
                error = ex.what();
            }
            catch (...)
            {
            }
        }
        catch (...)
        {
        }
    }

    bool SnapshotLayout(const std::string &filterModule)
    {
        regions_.clear();
        moduleBases_.clear();

        auto snapshot = std::make_unique<Driver::virtual_memory>();
        const auto &sharedInfo = dr->GetMemoryInfoRef();
        {
            std::scoped_lock<Driver::SpinLock> driverLock(dr->m_mutex);
            *snapshot = sharedInfo;
        }
        const auto &info = *snapshot;
        const int regionCount = std::clamp(info.region_count, 0, MAX_SCAN_REGIONS);
        const int moduleCount = std::clamp(info.module_count, 0, MAX_MODULES);
        auto addRegion = [this](uintptr_t rawStart, uintptr_t rawEnd)
        {
            constexpr uintptr_t alignment = sizeof(uintptr_t);
            uintptr_t start = MemUtils::Normalize(rawStart);
            uintptr_t end = MemUtils::Normalize(rawEnd);
            start = std::max(start, Config::Constants::ADDR_MIN + 1);
            end = std::min(end, Config::Constants::ADDR_MAX);
            start = (start + alignment - 1) & ~(alignment - 1);
            end &= ~(alignment - 1);
            if (start < end) regions_.emplace_back(start, end);
        };

        for (int i = 0; i < regionCount; ++i) addRegion(info.regions[i].start, info.regions[i].end);

        for (int moduleIndex = 0; moduleIndex < moduleCount; ++moduleIndex)
        {
            const auto &module = info.modules[moduleIndex];
            const size_t pathLength = strnlen(module.name, sizeof(module.name));
            const std::string fullPath(module.name, pathLength);
            const std::string name(MemUtils::BaseName(fullPath));
            const bool accepted = filterModule.empty() || name.find(filterModule) != std::string::npos;
            const uint64_t sourceId = HashSource(fullPath);
            const int segmentCount = std::clamp(module.seg_count, 0, MAX_SEGS_PER_MODULE);

            for (int segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
            {
                const auto &segment = module.segs[segmentIndex];
                const uintptr_t start = MemUtils::Normalize(segment.start);
                const uintptr_t end = MemUtils::Normalize(segment.end);
                addRegion(start, end);
                if (!accepted || !MemUtils::IsValidAddr(start) || end <= start || end > Config::Constants::ADDR_MAX) continue;

                BaseRange base;
                base.start = start;
                base.end = end;
                base.sourceId = sourceId;
                base.name = name;
                base.segment = segment.index;
                base.isBss = segment.index == -1;
                moduleBases_.push_back(std::move(base));
            }
        }

        std::sort(regions_.begin(), regions_.end());
        std::vector<std::pair<uintptr_t, uintptr_t>> merged;
        merged.reserve(regions_.size());
        for (const auto &region : regions_)
        {
            if (merged.empty() || region.first > merged.back().second) merged.push_back(region);
            else merged.back().second = std::max(merged.back().second, region.second);
        }
        regions_ = std::move(merged);

        std::sort(moduleBases_.begin(), moduleBases_.end(), [](const BaseRange &left, const BaseRange &right) { return std::tie(left.start, left.end, left.sourceId, left.segment) < std::tie(right.start, right.end, right.sourceId, right.segment); });
        moduleBases_.erase(std::unique(moduleBases_.begin(), moduleBases_.end(), [](const BaseRange &left, const BaseRange &right) { return left.start == right.start && left.end == right.end && left.sourceId == right.sourceId && left.segment == right.segment; }), moduleBases_.end());
        return !regions_.empty();
    }

    // 完整写入临时文件后原子发布为唯一结果文件。
    static bool SaveUniqueBin(FILE *source, std::string &path)
    {
        if (!source) return false;
        char tempPath[] = ".Pointer_scan_XXXXXX";
        int fd = mkstemp(tempPath);
        if (fd < 0) return false;

        FilePtr temp(fdopen(fd, "w+b"));
        if (!temp)
        {
            close(fd);
            remove(tempPath);
            return false;
        }

        rewind(source);
        std::array<char, 1 << 16> buffer{};
        bool ok = true;
        for (size_t size; (size = fread(buffer.data(), 1, buffer.size(), source)) > 0;)
        {
            if (fwrite(buffer.data(), 1, size, temp.get()) != size)
            {
                ok = false;
                break;
            }
        }
        if (ferror(source) != 0 || fflush(temp.get()) != 0 || fsync(fileno(temp.get())) != 0 || ferror(temp.get()) != 0) ok = false;
        FILE *raw = temp.release();
        if (fclose(raw) != 0) ok = false;

        char candidate[256];
        bool published = false;
        for (int i = 0; ok && i < 9999; ++i)
        {
            if (i == 0) snprintf(candidate, sizeof(candidate), "Pointer.bin");
            else snprintf(candidate, sizeof(candidate), "Pointer_%d.bin", i);

            if (link(tempPath, candidate) == 0)
            {
                path = candidate;
                published = true;
                break;
            }
            if (errno != EEXIST) break;
        }
        remove(tempPath);
        return published;
    }

    std::optional<std::vector<BaseRange>> SnapshotBases(BaseMode mode, uintptr_t manualBase, uintptr_t arrayBase, size_t arrayCount, size_t maxOffset)
    {
        if (mode == BaseMode::Module) return moduleBases_;

        std::vector<BaseRange> bases;
        if (mode == BaseMode::Manual)
        {
            BaseRange base;
            base.mode = mode;
            base.start = manualBase;
            base.end = OffsetEnd(manualBase, maxOffset);
            base.sourceId = manualBase;
            bases.push_back(std::move(base));
            return bases;
        }

        constexpr size_t kBatchEntries = PAGE_SIZE / sizeof(uintptr_t);
        std::array<uintptr_t, kBatchEntries> entries{};
        bases.reserve(arrayCount);
        auto addBase = [&](size_t index, uintptr_t objectBase)
        {
            objectBase = MemUtils::Normalize(objectBase);
            if (!MemUtils::IsValidAddr(objectBase)) return;
            BaseRange base;
            base.mode = mode;
            base.start = objectBase;
            base.end = OffsetEnd(objectBase, maxOffset);
            base.sourceId = index;
            base.arrayBase = arrayBase;
            base.arrayIndex = index;
            bases.push_back(std::move(base));
        };

        for (size_t first = 0; first < arrayCount; first += kBatchEntries)
        {
            const size_t count = std::min(kBatchEntries, arrayCount - first);
            const size_t bytes = count * sizeof(uintptr_t);
            const uintptr_t address = arrayBase + first * sizeof(uintptr_t);
            if (dr->Read(address, entries.data(), bytes) == static_cast<int>(bytes))
            {
                for (size_t i = 0; i < count; ++i) addBase(first + i, entries[i]);
                continue;
            }

            for (size_t i = 0; i < count; ++i)
            {
                uintptr_t objectBase = 0;
                if (dr->Read(address + i * sizeof(uintptr_t), &objectBase, sizeof(objectBase)) != static_cast<int>(sizeof(objectBase))) return std::nullopt;
                addBase(first + i, objectBase);
            }
        }

        std::sort(bases.begin(), bases.end(), [](const BaseRange &left, const BaseRange &right) { return std::tie(left.start, left.end, left.sourceId) < std::tie(right.start, right.end, right.sourceId); });
        return bases;
    }

    template <typename F>
    // 借用缓冲块执行任务并自动归还。
    void with_buffer_block(char **bufs, int &idx, uintptr_t start, size_t len, F &&call)
    {
        char *buf;
        {
            std::unique_lock<std::mutex> lk(block_mtx_);
            block_cv_.wait(lk, [&idx] { return idx >= 0; });
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
    bool collect_pointers_block(char *buf, uintptr_t start, size_t len, std::vector<PtrData> &out)
    {
        out.clear();
        const uintptr_t minAddress = regions_.front().first;
        const uintptr_t addressSpan = regions_.back().second - minAddress;
        size_t cursor = 0;
        out.reserve(len / sizeof(uintptr_t));

        while (cursor < len)
        {
            const size_t request = std::min(static_cast<size_t>(PAGE_SIZE), len - cursor);
            if (dr->Read(start + cursor, buf, request) != static_cast<int>(request)) return false;
            const size_t pointerCount = request / sizeof(uintptr_t);
            auto *values = reinterpret_cast<uintptr_t *>(buf);

            for (size_t i = 0; i < pointerCount; ++i)
            {
                const uintptr_t value = MemUtils::Normalize(values[i]);
                if ((value - minAddress) > addressSpan) continue;

                const auto region = std::upper_bound(regions_.begin(), regions_.end(), value, [](uintptr_t address, const auto &range) { return address < range.second; });
                if (region == regions_.end() || value < region->first) continue;

                out.emplace_back(MemUtils::Normalize(start + cursor + i * sizeof(uintptr_t)), value);
            }
            cursor += request;
        }
        return true;
    }

    // 在候选指针中筛选可匹配项。
    void search_in_pointers(std::vector<PtrDir> &input, std::vector<PtrData *> &out, size_t offset, bool use_limit, size_t limit)
    {
        if (input.empty() || pointers_.empty()) return;

        uintptr_t min_addr = regions_.front().first;
        uintptr_t sub = regions_.back().second - min_addr;
        std::vector<PtrData *> result;

        for (auto &pd : pointers_)
        {
            uintptr_t v = MemUtils::Normalize(pd.value);
            if ((v - min_addr) > sub) continue;

            const auto match = std::lower_bound(input.begin(), input.end(), v, [](const PtrDir &node, uintptr_t address) { return node.address < address; });
            if (match == input.end() || match->address - v > offset) continue;

            result.push_back(&pd);
        }

        size_t lim = use_limit ? std::min(limit, result.size()) : result.size();
        out.reserve(lim);
        for (size_t i = 0; i < lim; i++) out.push_back(result[i]);
    }

    // 通过活动区间同时匹配模块、手动和数组基址。
    void filter_to_ranges(std::vector<std::vector<PtrDir>> &dirs, std::vector<PtrRange> &ranges, std::vector<PtrData *> &curr, int level, const std::vector<BaseRange> &bases)
    {
        std::unordered_set<PtrData *> matched;
        std::set<size_t> active;
        std::priority_queue<std::pair<uintptr_t, size_t>, std::vector<std::pair<uintptr_t, size_t>>, std::greater<>> expiry;
        std::map<size_t, PtrRange> found;
        size_t nextBase = 0;

        for (auto *pointer : curr)
        {
            const uintptr_t address = MemUtils::Normalize(pointer->address);
            while (nextBase < bases.size() && bases[nextBase].start <= address)
            {
                active.insert(nextBase);
                expiry.emplace(bases[nextBase].end, nextBase);
                ++nextBase;
            }
            while (!expiry.empty() && expiry.top().first <= address)
            {
                active.erase(expiry.top().second);
                expiry.pop();
            }

            if (active.empty()) continue;
            matched.insert(pointer);
            for (const size_t baseIndex : active)
            {
                auto [entry, inserted] = found.try_emplace(baseIndex);
                if (inserted)
                {
                    entry->second.level = level;
                    entry->second.base = bases[baseIndex];
                }
                entry->second.results.emplace_back(address, MemUtils::Normalize(pointer->value), 0u, 1u);
            }
        }

        for (auto &[baseIndex, range] : found) ranges.push_back(std::move(range));

        push_unmatched(dirs, matched, curr, level);
    }

    // 把未匹配项追加到下一层处理集合。
    void push_unmatched(std::vector<std::vector<PtrDir>> &dirs, std::unordered_set<PtrData *> &matched, std::vector<PtrData *> &curr, int level)
    {
        for (auto *p : curr)
        {
            if (matched.find(p) == matched.end()) dirs[level].emplace_back(MemUtils::Normalize(p->address), MemUtils::Normalize(p->value), 0u, 1u);
        }
    }

    // 回填父子区间索引关系。
    void assoc_index(std::vector<PtrDir> &prev, PtrDir *start, size_t count, size_t offset)
    {
        for (size_t i = 0; i < count; i++)
        {
            uintptr_t normVal = MemUtils::Normalize(start[i].value);
            const uintptr_t upper = offset > std::numeric_limits<uintptr_t>::max() - normVal ? std::numeric_limits<uintptr_t>::max() : normVal + offset;
            const auto first = std::lower_bound(prev.begin(), prev.end(), normVal, [](const PtrDir &node, uintptr_t address) { return node.address < address; });
            const auto last = std::upper_bound(first, prev.end(), upper, [](uintptr_t address, const PtrDir &node) { return address < node.address; });
            start[i].start = static_cast<uint32_t>(first - prev.begin());
            start[i].end = static_cast<uint32_t>(last - prev.begin());
        }
    }

    // 并发建立各层索引关联。
    std::vector<std::future<void>> create_assoc_index(std::vector<PtrDir> &prev, std::vector<PtrDir> &curr, size_t offset)
    {
        std::vector<std::future<void>> futures;
        if (curr.empty()) return futures;
        if (prev.size() > std::numeric_limits<uint32_t>::max()) throw std::length_error("指针层节点数量超出格式上限");
        size_t total = curr.size(), pos = 0;
        try
        {
            while (pos < total)
            {
                size_t chunk = std::min(total - pos, static_cast<size_t>(10000));
                futures.push_back(Config::CpuThreadPool().submit_task([this, &prev, s = &curr[pos], chunk, offset] { assoc_index(prev, s, chunk, offset); }));
                pos += chunk;
            }
        }
        catch (...)
        {
            const auto submitError = std::current_exception();
            try
            {
                WaitFutures(futures);
            }
            catch (...)
            {
            }
            std::rethrow_exception(submitError);
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
                for (uint32_t j = p->start; j < p->end; j++) out.push_back(&base_dir[j]);
                right = p->end;
            }
            else if (right < p->end)
            {
                for (uint32_t j = right; j < p->end; j++) out.push_back(&base_dir[j]);
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
        if (ranges.empty()) return tree;

        int max_level = 0;
        for (auto &r : ranges) max_level = std::max(max_level, r.level);

        std::vector<std::vector<PtrRange *>> level_ranges(dirs.size());
        for (auto &r : ranges) level_ranges[r.level].push_back(&r);

        tree.counts.resize(max_level);
        tree.contents.resize(max_level + 1);

        for (int i = max_level; i > 0; i--)
        {
            std::vector<PtrDir *> stn;
            for (auto *r : level_ranges[i])
                for (auto &v : r->results) stn.push_back(&v);
            for (auto *p : tree.contents[i]) stn.push_back(p);

            std::sort(stn.begin(), stn.end(), [](auto a, auto b) { return a->start < b->start; });

            std::vector<PtrDir *> merged_out;
            merge_dirs(stn, dirs[i - 1].data(), merged_out);

            if (merged_out.empty()) return tree;

            tree.contents[i - 1] = std::move(merged_out);
        }

        tree.counts[0].assign(tree.contents[0].size(), 1);
        for (int i = 1; i < max_level; i++)
        {
            auto &cc = tree.counts[i];
            cc.resize(tree.contents[i].size(), 0);
            for (size_t j = 0; j < tree.contents[i].size(); j++)
            {
                const auto &node = *tree.contents[i][j];
                for (uint32_t child = node.start; child < node.end; ++child) cc[j] = SaturatingAdd(cc[j], tree.counts[i - 1][child]);
            }
        }

        tree.valid = true;
        return tree;
    }

    // 将指针树结果序列化写入文件。
    bool write_bin_file(std::vector<std::vector<PtrDir *>> &contents, std::vector<PtrRange> &ranges, FILE *f, BaseMode scanMode, uintptr_t target, uintptr_t manualBase, uintptr_t arrayBase, size_t arrayCount)
    {
        if (!f || ranges.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return false;
        if (!ranges.empty() && (contents.size() < 2 || contents.size() - 1 > static_cast<size_t>(std::numeric_limits<int>::max()))) return false;
        auto writeObject = [f](const auto &value) { return fwrite(&value, sizeof(value), 1, f) == 1; };
        auto writeArray = [f](const void *data, size_t size, size_t count) { return count == 0 || fwrite(data, size, count, f) == count; };

        BinHeader hdr{};
        strcpy(hdr.sign, ".bin pointer chain");
        hdr.size = sizeof(uintptr_t);
        hdr.version = kBinVersion;
        hdr.module_count = static_cast<int>(ranges.size());
        hdr.level = ranges.empty() ? 0 : static_cast<int>(contents.size()) - 1;
        hdr.scanBaseMode = static_cast<uint8_t>(scanMode);
        hdr.scanManualBase = scanMode == BaseMode::Manual ? MemUtils::Normalize(manualBase) : 0;
        hdr.scanArrayBase = scanMode == BaseMode::Array ? MemUtils::Normalize(arrayBase) : 0;
        hdr.scanArrayCount = scanMode == BaseMode::Array ? arrayCount : 0;
        hdr.scanTarget = MemUtils::Normalize(target);
        if (!writeObject(hdr)) return false;

        for (auto &r : ranges)
        {
            if (r.results.empty() || r.results.size() > static_cast<size_t>(std::numeric_limits<int>::max())) return false;
            BinSym sym{};
            sym.start = MemUtils::Normalize(r.base.start);
            sym.segment = r.base.segment;
            sym.isBss = r.base.isBss;
            sym.sourceMode = static_cast<uint8_t>(r.base.mode);
            sym.sourceId = r.base.sourceId;
            if (r.base.mode == BaseMode::Manual)
            {
                sym.manualBase = sym.start;
                strncpy(sym.name, "manual", sizeof(sym.name) - 1);
            }
            else if (r.base.mode == BaseMode::Array)
            {
                sym.arrayBase = MemUtils::Normalize(r.base.arrayBase);
                sym.arrayIndex = r.base.arrayIndex;
                snprintf(sym.name, sizeof(sym.name), "array[%zu]", r.base.arrayIndex);
            }
            else
            {
                strncpy(sym.name, r.base.name.data(), std::min(r.base.name.size(), sizeof(sym.name) - 1));
            }
            sym.level = r.level;
            sym.pointer_count = static_cast<int>(r.results.size());
            if (!writeObject(sym) || !writeArray(r.results.data(), sizeof(PtrDir), r.results.size())) return false;
        }

        for (size_t i = 0; !ranges.empty() && i + 1 < contents.size(); i++)
        {
            if (contents[i].empty() || contents[i].size() > std::numeric_limits<unsigned int>::max()) return false;
            BinLevel ll{};
            ll.level = static_cast<int>(i);
            ll.count = static_cast<unsigned int>(contents[i].size());
            if (!writeObject(ll)) return false;
            for (auto *p : contents[i])
                if (!writeObject(*p)) return false;
        }
        return fflush(f) == 0 && ferror(f) == 0;
    }

public:
    PointerManager() = default;
    ~PointerManager() = default;

    static std::string_view OperationName(Operation operation)
    {
        switch (operation)
        {
        case Operation::Scanning:
            return "scanning";
        case Operation::Merging:
            return "merging";
        case Operation::Exporting:
            return "exporting";
        default:
            return "idle";
        }
    }

    State state() const
    {
        std::lock_guard lock(statusMutex_);
        State result;
        result.operation = operation_.load(std::memory_order_acquire);
        result.lastOperation = lastOperation_.load(std::memory_order_acquire);
        result.progress = scanProgress_.load(std::memory_order_acquire);
        result.count = chainCount_.load(std::memory_order_acquire);
        result.completed = completed_.load(std::memory_order_acquire);
        result.success = success_.load(std::memory_order_acquire);
        result.error = lastError_;
        return result;
    }

    bool isBusy() const noexcept
    {
        return operation_.load(std::memory_order_acquire) != Operation::Idle;
    }

    // 返回指针扫描任务是否仍在运行。
    bool isScanning() const noexcept
    {
        return operation_.load(std::memory_order_acquire) == Operation::Scanning;
    }
    // 执行扫描逻辑并更新结果。
    float scanProgress() const noexcept
    {
        return scanProgress_;
    }
    // 返回当前结果数量。
    size_t count() const noexcept
    {
        return chainCount_;
    }

    void clear()
    {
        std::lock_guard lock(statusMutex_);
        if (operation_.load(std::memory_order_acquire) != Operation::Idle) return;
        pointers_.clear();
        regions_.clear();
        moduleBases_.clear();
        chainCount_ = 0;
        scanProgress_ = 0.0f;
        completed_ = false;
        success_ = false;
        lastOperation_ = Operation::Idle;
        lastError_.clear();
    }

    // 采集进程可用指针并建立初始集合。
    bool CollectPointers(size_t &pointerCount, int buf_count = 10, int buf_size = 1 << 20)
    {
        pointerCount = 0;
        pointers_.clear();
        if (regions_.empty() || buf_count <= 0 || buf_size < static_cast<int>(sizeof(uintptr_t))) return false;
        const size_t blockSize = static_cast<size_t>(buf_size) / sizeof(uintptr_t) * sizeof(uintptr_t);

        FilePtr merged(tmpfile());
        if (!merged)
        {
            std::println(stderr, "CollectPointers: failed to create merge temp file");
            return false;
        }

        int idx = buf_count - 1;
        std::vector<std::unique_ptr<char[]>> bufferStorage;
        bufferStorage.reserve(buf_count);
        std::vector<char *> bufs(buf_count);
        for (int i = 0; i < buf_count; i++)
        {
            bufferStorage.push_back(std::make_unique<char[]>(blockSize + sizeof(uintptr_t)));
            bufs[i] = bufferStorage.back().get();
        }

        std::mutex mergedMtx;
        std::atomic<bool> collectionFailed{false};
        std::vector<std::future<void>> futures;
        const size_t maxPending = std::max<size_t>(1, std::min<size_t>(static_cast<size_t>(buf_count), Config::CpuThreadPool().get_thread_count()));
        try
        {
            for (auto &[rstart, rend] : regions_)
            {
                for (uintptr_t pos = rstart; pos < rend; pos += blockSize)
                {
                    futures.push_back(Config::CpuThreadPool().submit_task(
                        [this, &bufs, &idx, pos, chunk = std::min(static_cast<size_t>(rend - pos), blockSize), file = merged.get(), &mergedMtx, &collectionFailed]
                        {
                            std::vector<PtrData> blockPointers;
                            bool blockOk = true;
                            with_buffer_block(bufs.data(), idx, pos, chunk, [this, &blockPointers, &blockOk](char *buf, uintptr_t s, size_t l) { blockOk = collect_pointers_block(buf, s, l, blockPointers); });
                            if (!blockOk)
                            {
                                collectionFailed = true;
                                return;
                            }
                            if (!blockPointers.empty())
                            {
                                std::lock_guard<std::mutex> lk(mergedMtx);
                                if (!collectionFailed && fwrite(blockPointers.data(), sizeof(PtrData), blockPointers.size(), file) != blockPointers.size()) collectionFailed = true;
                            }
                        }));
                    if (futures.size() >= maxPending) WaitFutures(futures);
                }
            }
            WaitFutures(futures);
        }
        catch (...)
        {
            try
            {
                WaitFutures(futures);
            }
            catch (...)
            {
            }
            throw;
        }

        if (collectionFailed)
        {
            std::println(stderr, "CollectPointers: temporary storage failed");
            return false;
        }

        if (fflush(merged.get()) != 0 || ferror(merged.get()) != 0)
        {
            std::println(stderr, "CollectPointers: failed to flush temporary storage");
            return false;
        }

        struct stat st;
        if (fstat(fileno(merged.get()), &st) == 0)
        {
            if (st.st_size < 0 || static_cast<size_t>(st.st_size) % sizeof(PtrData) != 0)
            {
                std::println(stderr, "CollectPointers: invalid temporary storage size");
                return false;
            }
            size_t total = static_cast<size_t>(st.st_size) / sizeof(PtrData);
            if (total > 0)
            {
                pointers_.resize(total);
                rewind(merged.get());
                if (fread(pointers_.data(), sizeof(PtrData), total, merged.get()) != total)
                {
                    pointers_.clear();
                    std::println(stderr, "CollectPointers: failed to read merged temporary storage");
                    return false;
                }
            }
        }
        else
        {
            std::println(stderr, "CollectPointers: failed to stat merge temp file");
            return false;
        }

        std::sort(pointers_.begin(), pointers_.end(), [](const PtrData &left, const PtrData &right) { return std::tie(left.address, left.value) < std::tie(right.address, right.value); });
        pointers_.erase(std::unique(pointers_.begin(), pointers_.end(), [](const PtrData &left, const PtrData &right) { return left.address == right.address && left.value == right.value; }), pointers_.end());
        pointerCount = pointers_.size();
        return true;
    }

private:
    // 执行指针链扫描主流程。
    void runScan(pid_t pid, uintptr_t target, int depth, int maxOffset, bool useManual, uintptr_t manualBase, bool useArray, uintptr_t arrayBase, size_t arrayCount, const std::string &filterModule)
    {
        bool succeeded = false;
        std::string error;
        struct ScanGuard
        {
            PointerManager &owner;
            bool &succeeded;
            std::string &error;
            ~ScanGuard()
            {
                owner.finishOperation(PointerManager::Operation::Scanning, succeeded, std::move(error));
            }
        } guard{*this, succeeded, error};

        try
        {
            if (pid <= 0 || pid != dr->GetGlobalPid())
            {
                error = "目标进程已变化";
                return;
            }

            scanProgress_ = 0.0f;
            chainCount_ = 0;

            target = MemUtils::Normalize(target);
            manualBase = MemUtils::Normalize(manualBase);
            arrayBase = MemUtils::Normalize(arrayBase);

            std::println("=== 开始指针扫描 ===");
            std::println("目标: {:x}, 深度: {}, 偏移: {}", target, depth, maxOffset);

            if (!SnapshotLayout(filterModule))
            {
                error = "无法获取有效内存布局";
                return;
            }

            const BaseMode scanMode = useManual ? BaseMode::Manual : (useArray ? BaseMode::Array : BaseMode::Module);
            auto baseSnapshot = SnapshotBases(scanMode, manualBase, arrayBase, arrayCount, static_cast<size_t>(maxOffset));
            if (!baseSnapshot)
            {
                error = "无法完整读取 Array 基址快照";
                return;
            }
            std::vector<BaseRange> bases = std::move(*baseSnapshot);
            if (scanMode == BaseMode::Module && bases.empty())
            {
                error = filterModule.empty() ? "内存布局中没有可用模块基址" : std::format("没有匹配模块过滤条件: {}", filterModule);
                return;
            }

            size_t pointerCount = 0;
            if (!CollectPointers(pointerCount))
            {
                std::println(stderr, "扫描失败: 无法创建内存快照");
                error = "无法创建内存快照";
                return;
            }
            std::println("内存快照数量: {}", pointerCount);

            FilePtr outfile(tmpfile());
            if (!outfile)
            {
                std::println(stderr, "无法创建临时文件");
                error = "无法创建结果临时文件";
                return;
            }

            std::vector<PtrRange> ranges;
            std::vector<std::vector<PtrDir>> dirs(depth + 1);
            size_t totalChains = 0;
            bool wroteResults = false;

            dirs[0].emplace_back(target, 0, 0, 1);
            std::sort(dirs[0].begin(), dirs[0].end(), [](const PtrDir &a, const PtrDir &b) { return a.address < b.address; });
            std::println("Level 0 初始化完成，目标地址数量: {}", dirs[0].size());

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
                std::sort(curr.begin(), curr.end(), [](auto a, auto b) { return a->address < b->address; });

                filter_to_ranges(dirs, ranges, curr, level, bases);

                auto futures = create_assoc_index(dirs[level - 1], dirs[level], static_cast<size_t>(maxOffset));
                WaitFutures(futures);

                scanProgress_ = static_cast<float>(level + 1) / (depth + 2);
            }

            for (auto &range : ranges)
            {
                if (range.level > 0)
                {
                    auto futures = create_assoc_index(dirs[range.level - 1], range.results, static_cast<size_t>(maxOffset));
                    WaitFutures(futures);
                }
            }

            if (!ranges.empty())
            {
                auto tree = build_dir_tree(dirs, ranges);
                if (tree.valid)
                {
                    for (auto &r : ranges)
                    {
                        if (r.level > 0 && static_cast<size_t>(r.level - 1) < tree.counts.size())
                        {
                            for (auto &v : r.results)
                            {
                                const auto &counts = tree.counts[r.level - 1];
                                if (v.start >= v.end || v.end > counts.size()) continue;
                                for (uint32_t child = v.start; child < v.end; ++child) totalChains = SaturatingAdd(totalChains, counts[child]);
                            }
                        }
                    }

                    std::println("开始写入文件，正在保存 {} 条链条...", totalChains);
                    wroteResults = write_bin_file(tree.contents, ranges, outfile.get(), scanMode, target, manualBase, arrayBase, arrayCount);
                    if (wroteResults) std::println("文件写入完成，总链数: {}", totalChains);
                    else std::println(stderr, "指针结果序列化失败");
                }
            }
            else
            {
                std::vector<std::vector<PtrDir *>> emptyContents;
                wroteResults = write_bin_file(emptyContents, ranges, outfile.get(), scanMode, target, manualBase, arrayBase, arrayCount);
                std::println("扫描结果为空，已生成空图");
            }

            if (!wroteResults)
            {
                chainCount_ = 0;
                error = "无法序列化指针结果";
                return;
            }

            if (pid != dr->GetGlobalPid())
            {
                error = "目标进程在扫描期间发生变化";
                return;
            }

            std::string autoName;
            const bool saved = SaveUniqueBin(outfile.get(), autoName);
            if (saved) std::println("结果已保存至: {}", autoName);
            else std::println(stderr, "无法原子保存指针结果文件");

            chainCount_ = saved ? totalChains : 0;
            succeeded = saved;
            if (!saved) error = "无法完整保存指针结果";
        }
        catch (const std::exception &ex)
        {
            error = ex.what();
        }
        catch (...)
        {
            error = "指针扫描发生未知异常";
        }
    }

public:
    bool startAsync(pid_t pid, uintptr_t target, int depth, int maxOffset, bool useManual, uintptr_t manualBase, bool useArray, uintptr_t arrayBase, size_t arrayCount, std::string filterModule)
    {
        std::lock_guard targetLock(Config::TargetMutex);
        if (depth <= 0 || depth > 16 || maxOffset <= 0 || useManual && useArray) return false;
        target = MemUtils::Normalize(target);
        manualBase = MemUtils::Normalize(manualBase);
        arrayBase = MemUtils::Normalize(arrayBase);
        if (!MemUtils::IsValidAddr(target)) return false;
        if (useManual && !MemUtils::IsValidAddr(manualBase)) return false;
        if (useArray)
        {
            constexpr size_t kMaxArrayCount = 1000000;
            constexpr uintptr_t kMaxArrayBase = Config::Constants::ADDR_MAX - sizeof(uintptr_t);
            if (!MemUtils::IsValidAddr(arrayBase) || arrayCount == 0 || arrayCount > kMaxArrayCount) return false;
            if (arrayBase > kMaxArrayBase || arrayCount - 1 > (kMaxArrayBase - arrayBase) / sizeof(uintptr_t)) return false;
        }
        if (!beginOperation(Operation::Scanning)) return false;
        if (pid <= 0 || pid != dr->GetGlobalPid())
        {
            finishOperation(Operation::Scanning, false, "目标进程已变化");
            return false;
        }
        scanProgress_ = 0.0f;
        try
        {
            Config::IoThreadPool().detach_task([this, pid, target, depth, maxOffset, useManual, manualBase, useArray, arrayBase, arrayCount, filterModule = std::move(filterModule)] { runScan(pid, target, depth, maxOffset, useManual, manualBase, useArray, arrayBase, arrayCount, filterModule); });
        }
        catch (const std::exception &ex)
        {
            finishOperation(Operation::Scanning, false, ex.what());
            return false;
        }
        return true;
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

        static bool ReadBytes(const char *&cur, const char *eof, void *out, size_t size)
        {
            if (cur > eof || size > static_cast<size_t>(eof - cur)) return false;
            std::memcpy(out, cur, size);
            cur += size;
            return true;
        }

        template <typename T> static bool ReadObject(const char *&cur, const char *eof, T &out)
        {
            return ReadBytes(cur, eof, &out, sizeof(T));
        }

        template <typename T> static bool ReadVector(const char *&cur, const char *eof, size_t count, std::vector<T> &out)
        {
            if (cur > eof) return false;
            if (count > static_cast<size_t>(eof - cur) / sizeof(T)) return false;
            out.resize(count);
            if (count > 0 && !ReadBytes(cur, eof, out.data(), count * sizeof(T))) return false;
            return true;
        }

        static bool WriteBytes(FILE *file, const void *data, size_t size)
        {
            return size == 0 || fwrite(data, 1, size, file) == size;
        }

        template <typename T> static bool WriteObject(FILE *file, const T &value)
        {
            return WriteBytes(file, &value, sizeof(T));
        }

        template <typename T> static bool WriteVector(FILE *file, const std::vector<T> &values)
        {
            return values.empty() || WriteBytes(file, values.data(), values.size() * sizeof(T));
        }

        bool validate() const
        {
            constexpr std::string_view signature = ".bin pointer chain";
            if (std::memcmp(hdr.sign, signature.data(), signature.size()) != 0 || hdr.sign[signature.size()] != '\0') return false;
            if (hdr.version != kBinVersion || hdr.size != static_cast<int>(sizeof(uintptr_t))) return false;
            if (hdr.module_count < 0 || static_cast<size_t>(hdr.module_count) != blocks.size()) return false;
            if (hdr.level < 0 || hdr.level > 16 || levels.size() != static_cast<size_t>(hdr.level)) return false;
            if (hdr.scanBaseMode > static_cast<uint8_t>(BaseMode::Array) || !MemUtils::IsValidAddr(static_cast<uintptr_t>(hdr.scanTarget))) return false;

            const auto mode = static_cast<BaseMode>(hdr.scanBaseMode);
            if (mode == BaseMode::Module)
            {
                if (hdr.scanManualBase != 0 || hdr.scanArrayBase != 0 || hdr.scanArrayCount != 0) return false;
            }
            else if (mode == BaseMode::Manual)
            {
                if (!MemUtils::IsValidAddr(static_cast<uintptr_t>(hdr.scanManualBase)) || hdr.scanArrayBase != 0 || hdr.scanArrayCount != 0) return false;
            }
            else
            {
                constexpr uint64_t kMaxArrayCount = 1000000;
                constexpr uintptr_t kMaxArrayBase = Config::Constants::ADDR_MAX - sizeof(uintptr_t);
                const uintptr_t arrayBase = static_cast<uintptr_t>(hdr.scanArrayBase);
                if (hdr.scanManualBase != 0 || !MemUtils::IsValidAddr(arrayBase) || hdr.scanArrayCount == 0 || hdr.scanArrayCount > kMaxArrayCount) return false;
                if (arrayBase > kMaxArrayBase || hdr.scanArrayCount - 1 > (kMaxArrayBase - arrayBase) / sizeof(uintptr_t)) return false;
            }

            if (blocks.empty()) return hdr.level == 0 && levels.empty();
            if (hdr.level == 0) return false;

            for (int level = 0; level < hdr.level; ++level)
            {
                if (levels[level].empty()) return false;
                if (levels[level].size() > std::numeric_limits<uint32_t>::max()) return false;
                if (level == 0)
                {
                    for (const auto &node : levels[level])
                        if (node.address != hdr.scanTarget || node.value != 0 || node.start != 0 || node.end != 1) return false;
                    continue;
                }
                const size_t childCount = levels[level - 1].size();
                for (const auto &node : levels[level])
                {
                    if (!MemUtils::IsValidAddr(node.address) || !MemUtils::IsValidAddr(node.value) || node.start >= node.end || node.end > childCount) return false;
                    if (!std::is_sorted(levels[level - 1].begin() + node.start, levels[level - 1].begin() + node.end, [](const PtrDir &a, const PtrDir &b) { return a.address < b.address; })) return false;
                }
            }

            int maxLevel = 0;
            std::set<std::tuple<uint8_t, uint64_t, std::string_view, int, bool, uint64_t, int>> blockIdentities;
            for (const auto &block : blocks)
            {
                if (block.sym.pointer_count <= 0 || static_cast<size_t>(block.sym.pointer_count) != block.roots.size()) return false;
                if (block.sym.level <= 0 || block.sym.level > hdr.level || block.sym.sourceMode > static_cast<uint8_t>(BaseMode::Array)) return false;
                if (block.sym.sourceMode != hdr.scanBaseMode) return false;
                if (std::memchr(block.sym.name, '\0', sizeof(block.sym.name)) == nullptr) return false;
                if (!MemUtils::IsValidAddr(static_cast<uintptr_t>(block.sym.start))) return false;

                if (mode == BaseMode::Module)
                {
                    if (block.sym.name[0] == '\0' || block.sym.segment < -1 || block.sym.isBss != (block.sym.segment == -1)) return false;
                    if (block.sym.manualBase != 0 || block.sym.arrayBase != 0 || block.sym.arrayIndex != 0) return false;
                }
                else if (mode == BaseMode::Manual)
                {
                    if (block.sym.start != hdr.scanManualBase || block.sym.manualBase != hdr.scanManualBase || block.sym.sourceId != hdr.scanManualBase) return false;
                    if (block.sym.arrayBase != 0 || block.sym.arrayIndex != 0 || block.sym.segment != 0 || block.sym.isBss) return false;
                }
                else
                {
                    if (block.sym.arrayBase != hdr.scanArrayBase || block.sym.arrayIndex >= hdr.scanArrayCount || block.sym.sourceId != block.sym.arrayIndex) return false;
                    if (block.sym.manualBase != 0 || block.sym.segment != 0 || block.sym.isBss) return false;
                }

                const std::string_view name(block.sym.name);
                if (!blockIdentities.emplace(block.sym.sourceMode, block.sym.sourceId, name, block.sym.segment, block.sym.isBss, block.sym.arrayIndex, block.sym.level).second) return false;

                const size_t childCount = levels[block.sym.level - 1].size();
                std::set<std::tuple<uintptr_t, uintptr_t, uint32_t, uint32_t>> roots;
                for (const auto &root : block.roots)
                {
                    if (!MemUtils::IsValidAddr(root.address) || !MemUtils::IsValidAddr(root.value) || root.start >= root.end || root.end > childCount) return false;
                    if (!std::is_sorted(levels[block.sym.level - 1].begin() + root.start, levels[block.sym.level - 1].begin() + root.end, [](const PtrDir &a, const PtrDir &b) { return a.address < b.address; })) return false;
                    if (!roots.emplace(root.address, root.value, root.start, root.end).second) return false;
                }
                maxLevel = std::max(maxLevel, block.sym.level);
            }
            return maxLevel == hdr.level;
        }

        // 从二进制文件加载指针图数据。
        bool load(const std::string &path)
        {
            struct Mapping
            {
                int fd = -1;
                void *data = MAP_FAILED;
                size_t size = 0;
                ~Mapping()
                {
                    if (data != MAP_FAILED) munmap(data, size);
                    if (fd >= 0) close(fd);
                }
            } mapping;

            mapping.fd = open(path.c_str(), O_RDONLY);
            if (mapping.fd < 0) return false;
            struct stat st;
            if (fstat(mapping.fd, &st) != 0 || st.st_size < static_cast<off_t>(sizeof(BinHeader))) return false;
            if (static_cast<uintmax_t>(st.st_size) > std::numeric_limits<size_t>::max()) return false;

            mapping.size = static_cast<size_t>(st.st_size);
            mapping.data = mmap(nullptr, mapping.size, PROT_READ, MAP_PRIVATE, mapping.fd, 0);
            if (mapping.data == MAP_FAILED) return false;

            const char *cur = static_cast<const char *>(mapping.data);
            const char *eof = cur + mapping.size;
            blocks.clear();
            levels.clear();
            bool ok = false;
            try
            {
                ok = ReadObject(cur, eof, hdr);
                constexpr std::string_view signature = ".bin pointer chain";
                ok = ok && std::memcmp(hdr.sign, signature.data(), signature.size()) == 0 && hdr.sign[signature.size()] == '\0';
                ok = ok && hdr.version == kBinVersion && hdr.size == static_cast<int>(sizeof(uintptr_t));
                ok = ok && hdr.module_count >= 0 && hdr.level >= 0 && hdr.level <= 16 && hdr.scanBaseMode <= static_cast<uint8_t>(BaseMode::Array);
                ok = ok && static_cast<size_t>(hdr.module_count) <= static_cast<size_t>(eof - cur) / sizeof(BinSym);

                if (ok) blocks.reserve(static_cast<size_t>(hdr.module_count));
                for (int i = 0; ok && i < hdr.module_count; ++i)
                {
                    Block block;
                    ok = ReadObject(cur, eof, block.sym);
                    ok = ok && block.sym.pointer_count > 0 && block.sym.level > 0 && block.sym.level <= hdr.level;
                    ok = ok && block.sym.sourceMode <= static_cast<uint8_t>(BaseMode::Array);
                    ok = ok && std::memchr(block.sym.name, '\0', sizeof(block.sym.name)) != nullptr;
                    if (!ok || !ReadVector(cur, eof, static_cast<size_t>(block.sym.pointer_count), block.roots))
                    {
                        ok = false;
                        break;
                    }
                    blocks.push_back(std::move(block));
                }

                if (ok) levels.resize(static_cast<size_t>(hdr.level));
                for (int expectedLevel = 0; ok && expectedLevel < hdr.level; ++expectedLevel)
                {
                    BinLevel level{};
                    ok = ReadObject(cur, eof, level);
                    ok = ok && level.level == expectedLevel && level.count > 0;
                    if (!ok || !ReadVector(cur, eof, static_cast<size_t>(level.count), levels[expectedLevel]))
                    {
                        ok = false;
                        break;
                    }
                }

                if (ok) ok = cur == eof && validate();
            }
            catch (...)
            {
                ok = false;
            }

            if (!ok)
            {
                hdr = {};
                blocks.clear();
                levels.clear();
            }
            return ok;
        }

        // 将当前指针图保存到文件。
        bool save(const std::string &path) const
        {
            if (!validate()) return false;
            FilePtr file(fopen(path.c_str(), "wb"));
            if (!file) return false;

            bool ok = WriteObject(file.get(), hdr);
            for (const auto &blk : blocks)
            {
                ok = ok && WriteObject(file.get(), blk.sym) && WriteVector(file.get(), blk.roots);
                if (!ok) break;
            }
            for (int i = 0; ok && i < hdr.level; ++i)
            {
                if (levels[i].size() > std::numeric_limits<unsigned int>::max())
                {
                    ok = false;
                    break;
                }
                BinLevel bl{};
                bl.level = i;
                bl.count = static_cast<unsigned int>(levels[i].size());
                ok = WriteObject(file.get(), bl) && WriteVector(file.get(), levels[i]);
            }
            if (ok) ok = fflush(file.get()) == 0 && fsync(fileno(file.get())) == 0 && ferror(file.get()) == 0;
            FILE *raw = file.release();
            if (fclose(raw) != 0) ok = false;
            if (!ok) remove(path.c_str());
            return ok;
        }
    };

    static uint64_t ChainBaseAddr(const BinSym &sym)
    {
        return (sym.sourceMode == 1) ? sym.manualBase : sym.start;
    }

    using RootIdentity = std::tuple<uint8_t, uint64_t, std::string, int, bool, uint64_t, bool, uint64_t, int>;

    static RootIdentity MakeRootIdentity(const BinSym &sym, const PtrDir &root)
    {
        const uint64_t base = ChainBaseAddr(sym);
        const bool negative = root.address < base;
        const uint64_t offset = negative ? base - root.address : root.address - base;
        const std::string name = sym.sourceMode == 0 ? std::string(sym.name) : std::string();
        const uint64_t sourceId = sym.sourceMode == 0 ? sym.sourceId : 0;
        const int segment = sym.sourceMode == 0 ? sym.segment : 0;
        const bool isBss = sym.sourceMode == 0 && sym.isBss;
        const uint64_t arrayIndex = sym.sourceMode == 2 ? sym.arrayIndex : 0;
        return {sym.sourceMode, sourceId, name, segment, isBss, arrayIndex, negative, offset, sym.level};
    }

    struct PrunedNode
    {
        PtrDir node;
        std::vector<PrunedNode> children;
    };

    static std::optional<PrunedNode> IntersectNode(const PtrDir &nodeA, const std::vector<const PtrDir *> &nodesB, int childLevel, const MemoryGraph &graphA, const MemoryGraph &graphB)
    {
        if (nodesB.empty()) return std::nullopt;

        PrunedNode result{nodeA, {}};
        if (childLevel < 0)
        {
            result.node.start = 0;
            result.node.end = 1;
            return result;
        }
        if (static_cast<size_t>(childLevel) >= graphA.levels.size() || static_cast<size_t>(childLevel) >= graphB.levels.size()) return std::nullopt;

        const auto &layerA = graphA.levels[childLevel];
        const auto &layerB = graphB.levels[childLevel];
        const uint32_t startA = std::min(static_cast<uint32_t>(layerA.size()), nodeA.start);
        const uint32_t endA = std::min(static_cast<uint32_t>(layerA.size()), nodeA.end);
        if (startA >= endA) return std::nullopt;

        for (uint32_t i = startA; i < endA; ++i)
        {
            if (layerA[i].address < nodeA.value) continue;
            const uint64_t offset = layerA[i].address - nodeA.value;
            std::vector<const PtrDir *> matchingChildren;

            for (const PtrDir *nodeB : nodesB)
            {
                if (offset > std::numeric_limits<uint64_t>::max() - nodeB->value) continue;
                const uint64_t expectedAddress = nodeB->value + offset;
                const uint32_t startB = std::min(static_cast<uint32_t>(layerB.size()), nodeB->start);
                const uint32_t endB = std::min(static_cast<uint32_t>(layerB.size()), nodeB->end);
                if (startB >= endB) continue;

                auto it = std::lower_bound(layerB.begin() + startB, layerB.begin() + endB, expectedAddress, [](const PtrDir &node, uint64_t address) { return node.address < address; });
                while (it != layerB.begin() + endB && it->address == expectedAddress)
                {
                    matchingChildren.push_back(&*it);
                    ++it;
                }
            }

            auto child = IntersectNode(layerA[i], matchingChildren, childLevel - 1, graphA, graphB);
            if (child) result.children.push_back(std::move(*child));
        }

        if (result.children.empty()) return std::nullopt;
        return result;
    }

    static bool FlattenPrunedNode(const PrunedNode &source, int childLevel, std::vector<std::vector<PtrDir>> &levels, PtrDir &output)
    {
        output = source.node;
        if (childLevel < 0)
        {
            output.start = 0;
            output.end = 1;
            return true;
        }
        if (source.children.empty() || static_cast<size_t>(childLevel) >= levels.size()) return false;

        auto &layer = levels[childLevel];
        if (source.children.size() > std::numeric_limits<uint32_t>::max() - layer.size()) return false;
        output.start = static_cast<uint32_t>(layer.size());
        for (const auto &child : source.children)
        {
            PtrDir flattenedChild;
            if (!FlattenPrunedNode(child, childLevel - 1, levels, flattenedChild)) return false;
            layer.push_back(flattenedChild);
        }
        output.end = static_cast<uint32_t>(layer.size());
        return output.start < output.end;
    }

    static bool IntersectGraphs(const MemoryGraph &graphA, const MemoryGraph &graphB, MemoryGraph &output, std::string &error)
    {
        if (graphA.hdr.scanBaseMode != graphB.hdr.scanBaseMode)
        {
            error = "指针文件的基址模式不一致";
            return false;
        }

        std::map<RootIdentity, std::vector<const PtrDir *>> rootIndex;
        for (const auto &block : graphB.blocks)
            for (const auto &root : block.roots) rootIndex[MakeRootIdentity(block.sym, root)].push_back(&root);

        output = {};
        output.hdr = graphA.hdr;
        output.levels.resize(static_cast<size_t>(graphA.hdr.level));

        if (graphA.blocks.empty() || graphB.blocks.empty())
        {
            output.hdr.module_count = 0;
            output.hdr.level = 0;
            output.levels.clear();
            return output.validate();
        }

        for (const auto &blockA : graphA.blocks)
        {
            MemoryGraph::Block outputBlock;
            outputBlock.sym = blockA.sym;

            for (const auto &rootA : blockA.roots)
            {
                const auto candidates = rootIndex.find(MakeRootIdentity(blockA.sym, rootA));
                if (candidates == rootIndex.end()) continue;

                auto prunedRoot = IntersectNode(rootA, candidates->second, blockA.sym.level - 1, graphA, graphB);
                if (!prunedRoot) continue;

                PtrDir flattenedRoot;
                if (!FlattenPrunedNode(*prunedRoot, blockA.sym.level - 1, output.levels, flattenedRoot))
                {
                    error = "裁剪后的指针图过大或结构无效";
                    return false;
                }
                outputBlock.roots.push_back(flattenedRoot);
            }

            if (!outputBlock.roots.empty())
            {
                if (outputBlock.roots.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
                {
                    error = "裁剪后的根节点数量超出格式上限";
                    return false;
                }
                outputBlock.sym.pointer_count = static_cast<int>(outputBlock.roots.size());
                output.blocks.push_back(std::move(outputBlock));
            }
        }

        if (output.blocks.empty())
        {
            output.hdr.module_count = 0;
            output.hdr.level = 0;
            output.levels.clear();
            return output.validate();
        }
        if (output.blocks.size() > static_cast<size_t>(std::numeric_limits<int>::max()))
        {
            error = "裁剪后的基址块数量超出格式上限";
            return false;
        }

        int maxLevel = 0;
        for (const auto &block : output.blocks) maxLevel = std::max(maxLevel, block.sym.level);
        output.hdr.level = maxLevel;
        output.hdr.module_count = static_cast<int>(output.blocks.size());
        output.levels.resize(static_cast<size_t>(maxLevel));
        if (!output.validate())
        {
            error = "裁剪后的指针图校验失败";
            return false;
        }
        return true;
    }

    static size_t CountGraphChains(const MemoryGraph &graph)
    {
        if (graph.blocks.empty()) return 0;

        std::vector<std::vector<size_t>> counts(static_cast<size_t>(graph.hdr.level));
        counts[0].assign(graph.levels[0].size(), 1);
        for (int level = 1; level < graph.hdr.level; ++level)
        {
            counts[level].resize(graph.levels[level].size(), 0);
            for (size_t i = 0; i < graph.levels[level].size(); ++i)
            {
                const auto &node = graph.levels[level][i];
                for (uint32_t child = node.start; child < node.end; ++child) counts[level][i] = SaturatingAdd(counts[level][i], counts[level - 1][child]);
            }
        }

        size_t total = 0;
        for (const auto &block : graph.blocks)
        {
            const auto &childCounts = counts[block.sym.level - 1];
            for (const auto &root : block.roots)
                for (uint32_t child = root.start; child < root.end; ++child) total = SaturatingAdd(total, childCounts[child]);
        }
        return total;
    }

    // 合并多轮扫描结果并裁剪失效链。
    bool MergeBins()
    {
        {
            std::lock_guard targetLock(Config::TargetMutex);
            if (!beginOperation(Operation::Merging)) return false;
        }

        try
        {
            Config::CpuThreadPool().detach_task(
                [this]()
                {
                    RunOperation(Operation::Merging, "指针结果合并失败",
                                 [this](bool &succeeded, std::string &error)
                                 {
                                     std::println("=== [MergeBins] 开始基于图裁剪算法的极速合并 ===");

                                     std::vector<std::string> files;
                                     if (access("Pointer.bin", F_OK) == 0) files.push_back("Pointer.bin");
                                     for (int i = 1; i < 9999; ++i)
                                     {
                                         char buf[64];
                                         snprintf(buf, 64, "Pointer_%d.bin", i);
                                         if (access(buf, F_OK) == 0) files.push_back(buf);
                                     }

                                     if (files.size() < 2)
                                     {
                                         error = "至少需要两个指针结果文件";
                                         std::println(stderr, "MergeBins: {}", error);
                                         return;
                                     }

                                     MemoryGraph GA;
                                     std::println("加载基准指针图: {}", files[0]);
                                     if (!GA.load(files[0]))
                                     {
                                         error = std::format("无法加载或校验 {}", files[0]);
                                         return;
                                     }

                                     for (size_t f_idx = 1; f_idx < files.size(); ++f_idx)
                                     {
                                         std::println("正在比对并裁剪: {}", files[f_idx]);
                                         MemoryGraph GB;
                                         if (!GB.load(files[f_idx]))
                                         {
                                             error = std::format("无法加载或校验 {}", files[f_idx]);
                                             return;
                                         }
                                         MemoryGraph nextGraph;
                                         std::string intersectError;
                                         if (!IntersectGraphs(GA, GB, nextGraph, intersectError))
                                         {
                                             error = std::format("{}: {}", files[f_idx], intersectError);
                                             return;
                                         }
                                         GA = std::move(nextGraph);
                                         scanProgress_ = static_cast<float>(f_idx + 1) / static_cast<float>(files.size() + 1);

                                         size_t remaining_roots = 0;
                                         for (auto &blk : GA.blocks) remaining_roots += blk.roots.size();
                                         std::println("  该轮裁剪完毕，剩余有效起始节点: {} 个", remaining_roots);
                                     }

                                     const size_t mergedChainCount = CountGraphChains(GA);
                                     remove("Pointer_Merged.tmp");
                                     if (!GA.save("Pointer_Merged.tmp"))
                                     {
                                         error = "无法完整写入合并临时文件";
                                         return;
                                     }
                                     if (rename("Pointer_Merged.tmp", "Pointer.bin") != 0)
                                     {
                                         error = std::format("无法替换 Pointer.bin: {}", std::strerror(errno));
                                         remove("Pointer_Merged.tmp");
                                         return;
                                     }
                                     for (const auto &fn : files)
                                     {
                                         if (fn != "Pointer.bin" && remove(fn.c_str()) != 0) std::println(stderr, "MergeBins: 无法删除旧文件 {}: {}", fn, std::strerror(errno));
                                     }

                                     chainCount_ = mergedChainCount;
                                     succeeded = true;
                                     std::println("图层合并结束！已成功剔除失效的指针树分支并生成 Pointer.bin");
                                 });
                });
        }
        catch (const std::exception &ex)
        {
            finishOperation(Operation::Merging, false, ex.what());
            return false;
        }
        catch (...)
        {
            finishOperation(Operation::Merging, false, "无法启动指针合并任务");
            return false;
        }
        return true;
    }

    // 将指针链导出为可读文本。
    bool ExportToTxt()
    {
        {
            std::lock_guard targetLock(Config::TargetMutex);
            if (!beginOperation(Operation::Exporting)) return false;
        }

        try
        {
            Config::CpuThreadPool().detach_task(
                [this]()
                {
                    RunOperation(Operation::Exporting, "指针链导出失败",
                                 [this](bool &succeeded, std::string &error)
                                 {
                                     std::println("=== 导出文本链条 ===");

                                     MemoryGraph graph;
                                     if (!graph.load("Pointer.bin"))
                                     {
                                         error = "无法加载或校验 Pointer.bin";
                                         return;
                                     }

                                     constexpr const char *tempPath = "Pointer_Export.tmp";
                                     constexpr const char *outputPath = "Pointer_Export.txt";
                                     remove(tempPath);
                                     FilePtr output(fopen(tempPath, "wb"));
                                     if (!output)
                                     {
                                         error = std::format("无法创建导出临时文件: {}", std::strerror(errno));
                                         return;
                                     }
                                     struct TempGuard
                                     {
                                         const char *path;
                                         bool committed = false;
                                         ~TempGuard()
                                         {
                                             if (!committed) remove(path);
                                         }
                                     } tempGuard{tempPath};

                                     bool writeOk = true;
                                     auto writeText = [&](const char *text)
                                     {
                                         if (writeOk && fputs(text, output.get()) == EOF) writeOk = false;
                                     };
                                     auto writeFormat = [&](const char *format, auto... args)
                                     {
                                         if (writeOk && fprintf(output.get(), format, args...) < 0) writeOk = false;
                                     };

                                     writeText("// Pointer Scan Export\n");
                                     writeFormat("// Version: %d, Depth: %d\n", graph.hdr.version, graph.hdr.level);
                                     writeFormat("// Target: 0x%llX\n", static_cast<unsigned long long>(graph.hdr.scanTarget));
                                     writeFormat("// Base Mode: %d (0=Module, 1=Manual, 2=Array)\n", graph.hdr.scanBaseMode);
                                     writeText("// ========================================\n\n");

                                     size_t exportedChains = 0;
                                     struct SignedOffset
                                     {
                                         bool negative;
                                         uint64_t magnitude;
                                     };
                                     std::array<SignedOffset, 16> offsets{};
                                     size_t offsetCount = 0;
                                     std::string currentBasePrefix;

                                     std::function<void(int, const PtrDir &)> dfs = [&](int currentLevel, const PtrDir &node)
                                     {
                                         if (!writeOk) return;
                                         if (currentLevel < 0)
                                         {
                                             writeFormat("%s", currentBasePrefix.c_str());
                                             for (size_t i = 0; i < offsetCount; ++i) writeFormat(offsets[i].negative ? " - 0x%llX" : " + 0x%llX", static_cast<unsigned long long>(offsets[i].magnitude));
                                             writeText("\n");
                                             if (exportedChains < std::numeric_limits<size_t>::max()) ++exportedChains;
                                             return;
                                         }

                                         if (static_cast<size_t>(currentLevel) >= graph.levels.size() || node.start >= node.end || node.end > graph.levels[currentLevel].size())
                                         {
                                             writeOk = false;
                                             return;
                                         }

                                         for (uint32_t i = node.start; i < node.end; ++i)
                                         {
                                             if (offsetCount >= offsets.size())
                                             {
                                                 writeOk = false;
                                                 return;
                                             }
                                             const auto &child = graph.levels[currentLevel][i];
                                             offsets[offsetCount++] = child.address < node.value ? SignedOffset{true, node.value - child.address} : SignedOffset{false, child.address - node.value};
                                             dfs(currentLevel - 1, child);
                                             --offsetCount;
                                         }
                                     };

                                     for (const auto &block : graph.blocks)
                                     {
                                         char baseText[256];
                                         uint64_t baseAddress = 0;
                                         switch (block.sym.sourceMode)
                                         {
                                         case 1:
                                             snprintf(baseText, sizeof(baseText), "\"Manual_0x%llX\"", static_cast<unsigned long long>(block.sym.manualBase));
                                             baseAddress = block.sym.manualBase;
                                             break;
                                         case 2:
                                             snprintf(baseText, sizeof(baseText), "\"Array[%llu]\"", static_cast<unsigned long long>(block.sym.arrayIndex));
                                             baseAddress = block.sym.start;
                                             break;
                                         default:
                                             snprintf(baseText, sizeof(baseText), "\"%s[%d]\"", block.sym.name, block.sym.segment);
                                             baseAddress = block.sym.start;
                                             break;
                                         }

                                         for (const auto &root : block.roots)
                                         {
                                             const bool negative = root.address < baseAddress;
                                             const uint64_t magnitude = negative ? baseAddress - root.address : root.address - baseAddress;
                                             currentBasePrefix = std::format("[{} {} 0x{:X}]", baseText, negative ? "-" : "+", magnitude);
                                             offsetCount = 0;
                                             dfs(block.sym.level - 1, root);
                                         }
                                     }

                                     if (writeOk) writeOk = fflush(output.get()) == 0 && fsync(fileno(output.get())) == 0 && ferror(output.get()) == 0;
                                     FILE *rawOutput = output.release();
                                     if (fclose(rawOutput) != 0) writeOk = false;
                                     if (!writeOk)
                                     {
                                         error = "导出文件写入不完整";
                                         return;
                                     }
                                     if (rename(tempPath, outputPath) != 0)
                                     {
                                         error = std::format("无法替换 {}: {}", outputPath, std::strerror(errno));
                                         return;
                                     }
                                     tempGuard.committed = true;

                                     chainCount_ = exportedChains;
                                     succeeded = true;
                                     std::println("导出完成: 成功输出 {} 条链条", exportedChains);
                                 });
                });
        }
        catch (const std::exception &ex)
        {
            finishOperation(Operation::Exporting, false, ex.what());
            return false;
        }
        catch (...)
        {
            finishOperation(Operation::Exporting, false, "无法启动指针导出任务");
            return false;
        }
        return true;
    }
};

// ============================================================================
// 内存工具统一运行时
// ============================================================================
namespace MemoryTool
{
    inline MemScanner &Scanner()
    {
        static MemScanner scanner;
        return scanner;
    }

    inline MemViewer &Viewer()
    {
        static MemViewer viewer;
        return viewer;
    }

    inline PointerManager &Pointer()
    {
        static PointerManager pointerManager;
        return pointerManager;
    }

    inline LockManager &Locks()
    {
        static LockManager locks;
        return locks;
    }

    inline std::string &HwbpMode()
    {
        static std::string hwbpMode;
        return hwbpMode;
    }

    inline std::atomic_int &SyscallMonitorPid()
    {
        static std::atomic_int pid{0};
        return pid;
    }

    inline std::mutex &SyscallMonitorMutex()
    {
        static std::mutex mutex;
        return mutex;
    }

    inline int StartSyscallMonitor()
    {
        std::lock_guard lock(SyscallMonitorMutex());
        const int pid = dr->GetGlobalPid();
        if (pid <= 0) return EINVAL;

        const int monitoredPid = SyscallMonitorPid().load(std::memory_order_acquire);
        if (monitoredPid == pid) return 0;
        if (monitoredPid > 0)
        {
            const int status = dr->StopSyscallMonitor(monitoredPid);
            if (status != 0) return status;
            SyscallMonitorPid().store(0, std::memory_order_release);
        }

        const int status = dr->StartSyscallMonitor(pid);
        if (status == 0) SyscallMonitorPid().store(pid, std::memory_order_release);
        return status;
    }

    inline int StopSyscallMonitor()
    {
        std::lock_guard lock(SyscallMonitorMutex());
        const int pid = SyscallMonitorPid().load(std::memory_order_acquire);
        if (pid <= 0) return 0;

        const int status = dr->StopSyscallMonitor(pid);
        if (status == 0) SyscallMonitorPid().store(0, std::memory_order_release);
        return status;
    }

    inline bool SelectTarget(int pid)
    {
        std::lock_guard targetLock(Config::TargetMutex);
        if (pid <= 0) return false;
        if (pid == dr->GetGlobalPid()) return true;
        if (Scanner().isScanning() || Pointer().isBusy()) return false;
        if (StopSyscallMonitor() != 0) return false;

        auto &mode = HwbpMode();
        if (mode == "hwbp") dr->RemoveProcessHwbpRef();
        else if (mode == "ptebp") dr->RemoveProcessPtebpRef();
        else if (mode == "stepbp") dr->RemoveProcessStepbpRef();

        mode.clear();
        Locks().clear();
        Scanner().clear();
        Viewer().clear();
        Pointer().clear();
        dr->SetGlobalPid(pid);
        return true;
    }
} // namespace MemoryTool
