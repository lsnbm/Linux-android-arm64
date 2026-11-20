#pragma once
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include "types.h"

namespace MemUtils
{
    template <typename F> decltype(auto) DispatchType(Types::DataType type, F &&fn)
    {
        switch (type)
        {
        case Types::DataType::I8: return fn.template operator()<int8_t>();
        case Types::DataType::I16: return fn.template operator()<int16_t>();
        case Types::DataType::I64: return fn.template operator()<int64_t>();
        case Types::DataType::Float: return fn.template operator()<float>();
        case Types::DataType::Double: return fn.template operator()<double>();
        case Types::DataType::I32:
        default: return fn.template operator()<int32_t>();
        }
    }

    constexpr uintptr_t Normalize(uintptr_t addr) noexcept
    {
        return addr & ~(0xFFULL << 56);
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
            T value{};
            if constexpr (std::is_same_v<T, float>) value = std::strtof(temp.c_str(), &end);
            else if constexpr (std::is_same_v<T, double>) value = std::strtod(temp.c_str(), &end);
            else value = std::strtold(temp.c_str(), &end);
            if (end != temp.data() + temp.size() || !std::isfinite(value) || (errno == ERANGE && value == T{})) return std::nullopt;
            return value;
        }
        else
        {
            const long long value = std::strtoll(temp.c_str(), &end, 10);
            if (errno == ERANGE || end != temp.data() + temp.size()) return std::nullopt;
            if (value < static_cast<long long>(std::numeric_limits<T>::min()) || value > static_cast<long long>(std::numeric_limits<T>::max())) return std::nullopt;
            return static_cast<T>(value);
        }
    }

    template <typename T> constexpr bool IsValidFloat(T value) noexcept
    {
        if constexpr (std::is_floating_point_v<T>) return std::isfinite(value);
        return true;
    }

    enum class FloatRounding
    {
        Rounded,
        Truncated,
        Extreme,
        Exact
    };

    struct FloatScanRange
    {
        double lower = 0;
        double upper = 0;
        bool enabled = false;
    };

    template <typename T> bool Compare(T value, T target, Types::FuzzyMode mode, T lastValue, T rangeMax = T{}, FloatScanRange floatRange = {})
    {
        using Types::FuzzyMode;
        if (!IsValidFloat(value)) return false;

        switch (mode)
        {
        case FuzzyMode::Equal:
            if (!IsValidFloat(target)) return false;
            if constexpr (std::is_floating_point_v<T>)
            {
                if (floatRange.enabled)
                    return value == target || (value >= static_cast<T>(floatRange.lower) && value <= static_cast<T>(floatRange.upper));
            }
            return value == target;
        case FuzzyMode::Greater:
            return IsValidFloat(target) && value > target;
        case FuzzyMode::Less:
            return IsValidFloat(target) && value < target;
        case FuzzyMode::Increased:
            return IsValidFloat(lastValue) && value > lastValue;
        case FuzzyMode::Decreased:
            return IsValidFloat(lastValue) && value < lastValue;
        case FuzzyMode::Changed:
            return IsValidFloat(lastValue) && value != lastValue;
        case FuzzyMode::Unchanged:
            return IsValidFloat(lastValue) && value == lastValue;
        case FuzzyMode::Range:
            return IsValidFloat(target) && IsValidFloat(rangeMax) && value >= std::min(target, rangeMax) && value <= std::max(target, rangeMax);
        case FuzzyMode::Pointer:
            if constexpr (std::is_integral_v<T>)
            {
                using Unsigned = std::make_unsigned_t<T>;
                return Normalize(static_cast<uintptr_t>(static_cast<Unsigned>(value))) == Normalize(static_cast<uintptr_t>(static_cast<Unsigned>(target)));
            }
            return false;
        default:
            return false;
        }
    }
}