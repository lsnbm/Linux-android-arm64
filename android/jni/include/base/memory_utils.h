#pragma once
#include <algorithm>
#include <array>
#include <charconv>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <format>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include "../../driver/driver.h"
#include "config.h"
#include "types.h"

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

        const auto baseValue = static_cast<__uint128_t>(base);
        __uint128_t value = 0;
        for (const char ch : text)
        {
            int digit = -1;
            if (ch >= '0' && ch <= '9') digit = ch - '0';
            else if (base == 16 && ch >= 'a' && ch <= 'f') digit = ch - 'a' + 10;
            else if (base == 16 && ch >= 'A' && ch <= 'F') digit = ch - 'A' + 10;

            if (digit < 0 || digit >= base) return std::nullopt;
            const auto digitValue = static_cast<__uint128_t>(digit);
            if (value > (~static_cast<__uint128_t>(0) - digitValue) / baseValue) return std::nullopt;
            value = value * baseValue + digitValue;
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
        for (int reg = Driver::IDX_PC; reg < Driver::MAX_REG_COUNT; ++reg)
            if (BP_GET_MASK(&record, reg) != BP_OP_WRITE) BP_SET_MASK(&record, reg, BP_OP_READ);
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
            return static_cast<std::uint32_t>(record.syscallno);
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
            record.syscallno = static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
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
        std::string token(fieldToken);
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
            if (regIndex && *regIndex >= 0 && *regIndex < 30) return Driver::IDX_X0 + *regIndex;
        }
        if (token.size() >= 2 && (token[0] == 'v' || token[0] == 'q'))
        {
            const auto regIndex = HwbpParseInt(std::string_view(token).substr(1));
            if (regIndex && *regIndex >= 0 && *regIndex < 32) return Driver::IDX_Q0 + *regIndex;
        }
        return std::nullopt;
    }

    inline std::optional<int> HwbpMaskByteIndexFromToken(std::string_view fieldToken)
    {
        std::string token(fieldToken);
        if (!token.starts_with("mask")) return std::nullopt;

        token.erase(0, 4);
        if (!token.empty() && (token.front() == '.' || token.front() == '_' || token.front() == '[')) token.erase(0, 1);
        if (!token.empty() && token.back() == ']') token.pop_back();

        const auto index = HwbpParseInt(token);
        if (!index || *index < 0 || *index >= 18) return std::nullopt;
        return *index;
    }

    inline bool AssignHwbpRecordField(Driver::bp_record &record, std::string_view fieldToken, __uint128_t value)
    {
        std::string token(fieldToken);
        for (char &ch : token)
        {
            if (ch >= 'A' && ch <= 'Z') ch = static_cast<char>(ch - 'A' + 'a');
        }

        if (const auto maskIndex = HwbpMaskByteIndexFromToken(token))
        {
            if (value > std::numeric_limits<std::uint8_t>::max()) return false;
            record.mask[*maskIndex] = static_cast<std::uint8_t>(value);
            return true;
        }

        if (token.starts_with("op.") || token.starts_with("mask."))
        {
            const auto regIndex = HwbpRegIndexFromToken(token);
            if (!regIndex || value > BP_OP_WRITE) return false;
            BP_SET_MASK(&record, *regIndex, static_cast<std::uint8_t>(value));
            return true;
        }

        const auto regIndex = HwbpRegIndexFromToken(token);
        if (!regIndex) return false;
        if (*regIndex >= Driver::IDX_Q0 && *regIndex <= Driver::IDX_Q31) return HwbpWriteRegisterValue(record, *regIndex, value);
        if ((*regIndex == Driver::IDX_SYSCALLNO || *regIndex == Driver::IDX_FPSR || *regIndex == Driver::IDX_FPCR) && value > std::numeric_limits<std::uint32_t>::max()) return false;
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
        const auto parsed = ParseUInt64(str, 16);
        if (!parsed) return false;
        const int64_t value = static_cast<int64_t>(*parsed);
        return dr->Write<int64_t>(addr, value) == static_cast<int>(sizeof(value));
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
