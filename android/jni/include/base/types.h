#pragma once
#include <array>
#include <cstddef>
#include <cstdint>

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

    enum class SavedValueKind : int
    {
        Numeric = 0,
        Pointer,
        Text
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
