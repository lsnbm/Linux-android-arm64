#pragma once
#include <array>
#include <cstdio>
#include <format>
#include <string>
#include <string_view>

namespace SyscallLog
{
    inline std::string ReadDmesg(std::string_view tag = {})
    {
        std::string out;
        std::array<char, 4096> buf{};
        std::string command = "dmesg 2>/dev/null | grep 'lsdriver:";
        if (!tag.empty()) command += std::format(" {}:", tag);
        command += "'";
        FILE *pipe = popen(command.c_str(), "r");
        if (!pipe) return {};
        while (fgets(buf.data(), static_cast<int>(buf.size()), pipe)) out += buf.data();
        pclose(pipe);
        return out;
    }
} // namespace SyscallLog
