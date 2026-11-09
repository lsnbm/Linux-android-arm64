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
            if (end == temp.c_str() || *end != '\0' || !std::isfinite(value) || (errno == ERANGE && value == T{})) return std::nullopt;
            return value;
        }
        else
        {
            const long long value = std::strtoll(temp.c_str(), &end, 10);
            if (errno == ERANGE || end == temp.c_str() || *end != '\0') return std::nullopt;
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

    template <typename T> std::optional<FloatScanRange> ParseFloatScanRange(std::string_view text, FloatRounding rounding = FloatRounding::Rounded)
    {
        static_assert(std::is_floating_point_v<T>);
        if (!ParseScanValue<T>(text)) return std::nullopt;
        if (rounding == FloatRounding::Exact || text.find_first_of("xXpP") != std::string_view::npos) return FloatScanRange{};

        const auto exponentPosition = text.find_first_of("eE");
        const auto mantissa = text.substr(0, exponentPosition);
        const auto decimalPosition = mantissa.find('.');
        const size_t decimalDigits = decimalPosition == std::string_view::npos ? 0 : mantissa.size() - decimalPosition - 1;
        long double exponent = 0;
        if (exponentPosition != std::string_view::npos)
        {
            const std::string exponentText(text.substr(exponentPosition + 1));
            exponent = std::strtold(exponentText.c_str(), nullptr);
        }
        const long double step = std::pow(10.0L, exponent - static_cast<long double>(decimalDigits));
        if (!std::isfinite(step) || step == 0) return FloatScanRange{};

        const long double center = *ParseScanValue<long double>(text);
        const long double radius = rounding == FloatRounding::Rounded ? step / 2 : step;
        const long double lower = rounding == FloatRounding::Truncated ? center : center - radius;
        const long double upper = center + radius;
        const long double lowest = std::numeric_limits<T>::lowest();
        const long double highest = std::numeric_limits<T>::max();
        T first = static_cast<T>(std::clamp(lower, lowest, highest));
        T last = static_cast<T>(std::clamp(upper, lowest, highest));
        if (static_cast<long double>(first) < lower) first = std::nextafter(first, std::numeric_limits<T>::infinity());
        if (static_cast<long double>(last) > upper || (rounding != FloatRounding::Extreme && static_cast<long double>(last) == upper)) last = std::nextafter(last, -std::numeric_limits<T>::infinity());
        return FloatScanRange{static_cast<double>(first), static_cast<double>(last), true};
    }

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