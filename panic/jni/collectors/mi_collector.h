#ifndef PANIC_MI_COLLECTOR_H_
#define PANIC_MI_COLLECTOR_H_

#include "../utils/dump_utils.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>

namespace panicdump
{
    namespace micollector
    {

        struct MiuiLogPath
        {
            const char *source;
        };

        inline const MiuiLogPath kMiuiLogPaths[] = {
            {"/data/local/log"},
            {"/data/local/bootlog"},
            {"/data/vendor/tz_log"},
            {"/dev/logfs"}, // 挂载后路径，原分区: /dev/block/bootdevice/by-name/logfs
            {"/dev/rescue"}, // 挂载后路径，原分区: /dev/block/bootdevice/by-name/rescue
            {"/dev/rescue/poweroff_charger_log"}, // rescue 分区挂载后的子目录，原分区: /dev/block/bootdevice/by-name/rescue
            {"/mnt/rescue"}, // 挂载后路径，原分区: /dev/block/bootdevice/by-name/rescue
            {"/mnt/rescue/mqsas"}, // rescue 分区挂载后的子目录，原分区: /dev/block/bootdevice/by-name/rescue
            {"/cache/mqsas"},
            {"/data/mqsas"},
            {"/data/miuilog"},
            {"/data/system/miuilog"},
            {"/data/miuilog/stability/nativecrash"},
            {"/data/miuilog/stability/nativecrash/tombstones"},
            {"/data/miuilog/stability/nativecrash/coredump"},
            {"/data/miuilog/stability/hprof"},
            {"/data/miuilog/stability/memleak"},
            {"/data/miuilog/misight"},
            {"/data/miuilog/bsp/audio/traces"},
            {"/data/misc/dmesgd"},
            {"/data/misc/perfetto-traces"},
            {"/data/misc/perfetto-traces/bugreport"},
            {"/data/vendor/ssrdump"},
            {"/data/vendor/ramdump"},
            {"/data/vendor/ramdump/bluetooth"},
            {"/data/vendor/ramdump_ssr"},
            {"/data/vendor/tombstones"},
            {"/data/vendor/tombstones/rfs"},
            {"/vendor/data/tombstones"},
            {"/data/vendor/qti-logkit"},
            {"/data/vendor/qti-logkit/logdata"},
            {"/data/vendor/diag"},
            {"/data/vendor/pddump"},
            {"/data/vendor/fpdump"},
            {"/data/vendor/wlan_logs"},
            {"/data/vendor/log/wifi"},
            {"/data/vendor/imslogs"},
            {"/data/vendor/charge_logger"},
            {"/data/vendor/bsplog"},
        };

    } // namespace micollector

    // 执行 MIUI/Qualcomm 日志目录收集；目录不存在时直接跳过。
    inline bool copyMiuiCoredumpDirectory(bool &copied)
    {
        bool success = true;

        for (const micollector::MiuiLogPath &logPath : micollector::kMiuiLogPaths)
        {
            struct stat sourceStat{};
            if (lstat(logPath.source, &sourceStat) != 0)
            {
                if (errno == ENOENT)
                {
                    logError("未找到 MIUI 日志目录，跳过: %s\n", logPath.source);
                    continue;
                }
                logError("检查 MIUI 日志目录失败: %s: %s\n", logPath.source, std::strerror(errno));
                success = false;
                continue;
            }

            if (!S_ISDIR(sourceStat.st_mode))
            {
                logError("MIUI 日志路径不是目录: %s\n", logPath.source);
                success = false;
                continue;
            }

            std::string destinationPath = joinPath(kDestinationRoot, stripLeadingSlash(logPath.source));
            logInfo("复制 MIUI 日志目录: %s -> %s\n", logPath.source, destinationPath.c_str());
            if (copyTree(logPath.source, destinationPath))
            {
                copied = true;
            }
            else
            {
                success = false;
            }
        }

        return success;
    }

} // namespace panicdump

#endif // PANIC_MI_COLLECTOR_H_
