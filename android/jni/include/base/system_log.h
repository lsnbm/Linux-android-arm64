#pragma once
#include <sys/klog.h>
#include <cerrno>
#include <charconv>
#include <string>
#include <string_view>

namespace SyscallLog
{
    inline std::string ReadDmesg(std::string_view tag = {})
    {
        auto readLog = [](int action, char *buffer, int size)
        {
            int result;
            do
            {
                result = klogctl(action, buffer, size);
            } while (result < 0 && errno == EINTR);
            return result;
        };
        // READ_ALL 只读取当前快照，不消费或清空日志；失败保持原接口的空字符串语义。
        const int capacity = readLog(KLOG_SIZE_BUFFER, nullptr, 0);
        if (capacity <= 0) return {};
        std::string snapshot(static_cast<size_t>(capacity), '\0');
        const int count = readLog(KLOG_READ_ALL, snapshot.data(), capacity);
        if (count <= 0) return {};

        std::string filter = "lsdriver:";
        if (!tag.empty())
        {
            filter += ' ';
            filter += tag;
            filter += ':';
        }
        std::string out;
        std::string_view remaining(snapshot.data(), static_cast<size_t>(count));
        while (!remaining.empty())
        {
            const size_t end = remaining.find('\n');
            auto line = remaining.substr(0, end);
            // 当前调用方的 sysmon/cntvct 标签按字面匹配，不经过 shell 或正则解释。
            if (line.find(filter) != std::string_view::npos)
            {
                // klogctl 文本带 <PRI>；仅去掉合法的行首优先级，保留内核时间戳及正文。
                if (line.front() == '<')
                {
                    const size_t close = line.find('>');
                    if (close > 1 && close <= 4)
                    {
                        unsigned priority = 0;
                        const auto end = line.data() + close;
                        const auto parsed = std::from_chars(line.data() + 1, end, priority);
                        if (parsed.ec == std::errc{} && parsed.ptr == end && priority <= 191) line.remove_prefix(close + 1);
                    }
                }
                out.append(line);
                out += '\n';
            }
            if (end == std::string_view::npos) break;
            remaining.remove_prefix(end + 1);
        }
        return out;
    }
} // namespace SyscallLog
