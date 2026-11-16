#ifndef PANIC_OPLUS_COLLECTOR_H_
#define PANIC_OPLUS_COLLECTOR_H_

#include "../utils/dump_utils.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

namespace panicdump
{
    namespace opluscollector
    {
        inline const char *const kOplusDumpPaths[] = {
            "/mnt/vendor/oplusreserve",
            "/mnt/oplus/op2",
            "/mnt/vendor/oplusreserve/media",
            "/mnt/vendor/oplusreserve/media/log",
            "/mnt/vendor/oplusreserve/media/log/minidump",
            "/mnt/vendor/oplusreserve/media/log/criticalLog",
            "/mnt/vendor/oplusreserve/media/log/criticalLog/persist",
            "/mnt/vendor/oplusreserve/media/log/criticalLog/cache",
            "/mnt/vendor/oplusreserve/media/log/vold",
            "/mnt/vendor/oplusreserve/media/log/shutdown",
            "/mnt/vendor/oplusreserve/media/log/hang_oplus",
            "/mnt/vendor/oplusreserve/media/log/boot_log",
            "/mnt/vendor/oplusreserve/media/log/stamp",
            "/mnt/vendor/oplusreserve/media/log/usage",
            "/mnt/vendor/oplusreserve/media/log/usage/cache",
            "/mnt/vendor/oplusreserve/media/log/usage/persist",
            "/mnt/vendor/oplusreserve/media/dropbox",
            "/mnt/vendor/oplusreserve/rbr_log",
            "/mnt/vendor/oplusreserve/rbr_log/last_init_boot_failed.log",
            "/mnt/vendor/oplusreserve/recovery",
            "/mnt/vendor/oplusreserve/recovery/payload.map",
            "/mnt/vendor/oplusreserve/recovery/payload_properties.map",
            "/mnt/vendor/oplusreserve/update_engine_log",
            "/mnt/vendor/oplusreserve/factory",
            "/mnt/vendor/oplusreserve/connectivity",
            "/mnt/vendor/oplusreserve/radio",
            "/mnt/vendor/oplusreserve/system",
            "/mnt/vendor/oplusreserve/system/config",
            "/mnt/vendor/oplusreserve/system/flag",
            "/mnt/vendor/oplusreserve/system/ssv",
        };

    } // namespace opluscollector

    // 复制 Oplus reserve 和 op2 日志路径，并按原始绝对路径映射到输出目录。
    inline bool collectOplusDumps(int &matchedDumpPathCount, int &copiedDumpPathCount)
    {
        using namespace opluscollector;

        bool success = true;
        for (const char *source : kOplusDumpPaths)
        {
            struct stat sourceStat{};
            if (lstat(source, &sourceStat) != 0)
            {
                if (errno == ENOENT)
                {
                    logInfo("未找到 Oplus dump 路径，跳过: %s\n", source);
                    continue;
                }
                logError("检查 Oplus dump 路径失败: %s: %s\n", source, std::strerror(errno));
                success = false;
                continue;
            }

            ++matchedDumpPathCount;
            const std::string destination = joinPath(kDestinationRoot, stripLeadingSlash(source));
            logInfo("复制 Oplus dump 路径: %s -> %s\n", source, destination.c_str());
            if (copyTree(source, destination))
            {
                ++copiedDumpPathCount;
            }
            else
            {
                success = false;
            }
        }

        return success && copiedDumpPathCount == matchedDumpPathCount && matchedDumpPathCount > 0;
    }

} // namespace panicdump

#endif // PANIC_OPLUS_COLLECTOR_H_
