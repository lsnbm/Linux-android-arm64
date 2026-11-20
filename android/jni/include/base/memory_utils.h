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
#include "scan_value.h"

namespace MemUtils
{
    using namespace Types;
    using namespace Config;

    inline std::string_view BaseName(std::string_view path) noexcept
    {
        if (auto slash = path.rfind('/'); slash != std::string_view::npos) return path.substr(slash + 1);
        return path;
    }

    inline std::optional<std::uint64_t> ParseUInt64(std::string_view text, int base = 0)
    {
        if (text.empty()) return std::nullopt;

        const auto first = text.find_first_not_of(" \t\r\n\f\v");
        if (first == std::string_view::npos || text[first] == '-') return std::nullopt;

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const auto value = std::strtoull(temp.c_str(), &end, base);
        if (errno != 0 || end != temp.data() + temp.size()) return std::nullopt;
        return static_cast<std::uint64_t>(value);
    }

    inline std::optional<__uint128_t> ParseUInt128(std::string_view text, int base = 0)
    {
        if (text.empty()) return std::nullopt;

        if ((base == 0 || base == 16) && text.size() > 2 && text[0] == '0' && (text[1] == 'x' || text[1] == 'X'))
        {
            base = 16;
            text.remove_prefix(2);
        }
        if (base == 0) base = 10;

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


    // 验证地址合法
    constexpr bool IsValidAddr(uintptr_t addr) noexcept
    {
        uintptr_t a = Normalize(addr);
        return a > Constants::ADDR_MIN && a < Constants::ADDR_MAX;
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
        if (reg >= 0 && reg < Driver::MAX_REG_COUNT && BP_GET_MASK(&record, reg) != BP_OP_WRITE) BP_SET_MASK(&record, reg, BP_OP_READ);
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

    inline int HwbpWriteRegisterValue(Driver::bp_record &record, int reg, __uint128_t value)
    {
        int status = 0;
        if (reg < 0 || reg >= Driver::MAX_REG_COUNT)
        {
            status = -EINVAL;
            return status;
        }

        if ((reg < Driver::IDX_Q0 || reg > Driver::IDX_Q31) &&
            (value > std::numeric_limits<std::uint64_t>::max() ||
             ((reg == Driver::IDX_SYSCALLNO || reg == Driver::IDX_FPSR || reg == Driver::IDX_FPCR) && value > std::numeric_limits<std::uint32_t>::max())))
        {
            status = -ERANGE;
            return status;
        }

        switch (reg)
        {
        case Driver::IDX_PC:
            record.pc = static_cast<std::uint64_t>(value);
            break;
        case Driver::IDX_HIT_COUNT:
            record.hit_count = static_cast<std::uint64_t>(value);
            break;
        case Driver::IDX_LR:
            record.lr = static_cast<std::uint64_t>(value);
            break;
        case Driver::IDX_SP:
            record.sp = static_cast<std::uint64_t>(value);
            break;
        case Driver::IDX_ORIG_X0:
            record.orig_x0 = static_cast<std::uint64_t>(value);
            break;
        case Driver::IDX_SYSCALLNO:
            record.syscallno = static_cast<std::int32_t>(static_cast<std::uint32_t>(value));
            break;
        case Driver::IDX_PSTATE:
            record.pstate = static_cast<std::uint64_t>(value);
            break;
        case Driver::IDX_FPSR:
            record.fpsr = static_cast<std::uint32_t>(value);
            break;
        case Driver::IDX_FPCR:
            record.fpcr = static_cast<std::uint32_t>(value);
            break;
        default:
            if (reg >= Driver::IDX_X0 && reg <= Driver::IDX_X29)
                record.*HwbpXFields[reg - Driver::IDX_X0] = static_cast<std::uint64_t>(value);
            else if (reg >= Driver::IDX_Q0 && reg <= Driver::IDX_Q31)
                record.*HwbpQFields[reg - Driver::IDX_Q0] = value;
            else
            {
                status = -EINVAL;
                return status;
            }
        }
        BP_SET_MASK(&record, reg, BP_OP_WRITE);
        return status;
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


    // 按指定类型读取内存并转为字符串。
    inline std::string ReadAsString(uintptr_t addr, DataType type)
    {
        if (!addr) return "??";
        return DispatchType(type,
                            [&]<typename T>() -> std::string
                            {
                                T value{};
                                if (dr->Read(addr, &value, sizeof(T)) != static_cast<int>(sizeof(T))) return "??";
                                if constexpr (std::is_floating_point_v<T>) return std::format("{:.{}g}", value, std::numeric_limits<T>::max_digits10);
                                else return std::to_string(value);
                            });
    }

    // 把字符串按指定类型写入目标地址。
    inline int WriteFromString(uintptr_t addr, DataType type, std::string_view str)
    {
        int status = 0;
        if (!addr)
        {
            status = -EINVAL;
            return status;
        }
        status = DispatchType(type,
                            [&]<typename T>() -> int
                            {
                                int status = 0;
                                const auto value = ParseScanValue<T>(str);
                                if (!value)
                                {
                                    status = -EINVAL;
                                    return status;
                                }
                                status = dr->Write<T>(addr, *value);
                                if (status < 0) return status;
                                status = status == static_cast<int>(sizeof(T)) ? 0 : -EIO;
                                return status;
                            });
        return status;
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

    inline int WriteText(uintptr_t addr, std::string_view str)
    {
        int status = 0;
        if (!addr || str.empty())
        {
            status = -EINVAL;
            return status;
        }

        std::string temp(str);
        status = dr->Write(addr, temp.data(), temp.size() + 1);
        if (status < 0) return status;
        status = static_cast<size_t>(status) == temp.size() + 1 ? 0 : -EIO;
        return status;
    }

    inline std::string ReadAsPointerString(uintptr_t addr)
    {
        if (!addr) return "??";
        int64_t value = 0;
        if (dr->Read(addr, &value, sizeof(value)) != static_cast<int>(sizeof(value))) return "??";
        return std::format("0x{:X}", Normalize(static_cast<uintptr_t>(value)));
    }

    // 把十六进制文本解析后写入指针值。
    inline int WritePointerFromString(uintptr_t addr, std::string_view str)
    {
        int status = 0;
        const auto parsed = ParseUInt64(str, 16);
        if (!addr || !parsed)
        {
            status = -EINVAL;
            return status;
        }
        status = dr->Write<int64_t>(addr, static_cast<int64_t>(*parsed));
        if (status < 0) return status;
        status = status == static_cast<int>(sizeof(int64_t)) ? 0 : -EIO;
        return status;
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
