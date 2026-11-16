#ifndef PANIC_VIVO_COLLECTOR_H_
#define PANIC_VIVO_COLLECTOR_H_

#include "../utils/dump_utils.h"

#include <cerrno>
#include <cstring>
#include <string>
#include <sys/stat.h>

namespace panicdump
{
    namespace vivocollector
    {

        inline const char *const kVivoLogDirectories[] = {
            "/cache/recovery",
            "/data/cache/recovery",
            "/data/bbklog",
            "/data/bbklog/fileobs",
            "/data/bbkpanic",
            "/data/bbkpanic/ssrminidump",
            "/data/local/traces",
            "/data/log_temp",
            "/data/mblog",
            "/data/mdl",
            "/data/misc/a11ytrace",
            "/data/misc/bluetooth/logs",
            "/data/misc/boottrace",
            "/data/misc/bootstat",
            "/data/misc/boottrace/categories",
            "/data/misc/dmesgd",
            "/data/misc/nfc/logs",
            "/data/misc/perfetto-configs",
            "/data/misc/perfetto-traces",
            "/data/misc/perfetto-traces/bugreport",
            "/data/misc/perfetto-traces/profiling",
            "/data/misc/recovery",
            "/data/misc/recovery/proc",
            "/data/misc/snapshotctl_log",
            "/data/misc/trace",
            "/data/misc/update_engine_log",
            "/data/misc/wmtrace",
            "/data/SecureElement/logData",
            "/data/SecureElement/logData/log",
            "/data/SecureElement/logData/rb",
            "/data/SecureElement/logData/uploadFiles",
            "/data/system/dropbox",
            "/data/system/heapdump",
            "/data/tombstones",
            "/data/user_de/0/com.android.shell/files/bugreports",
            "/data/vendor/audio",
            "/data/vendor/audio/memlog",
            "/data/vendor/esehal",
            "/data/vendor/esehal/vcd",
            "/data/vendor/imslogs",
            "/data/vendor/log",
            "/data/vendor/log/wifi",
            "/data/vendor/nfc",
            "/data/vendor/nfc/cloudlog",
            "/data/vendor/nicmd",
            "/data/vendor/qmipriod",
            "/data/vendor/pddump",
            "/data/vendor/qms_logs",
            "/data/vendor/qms_logs/config",
            "/data/vendor/qms_logs/main",
            "/data/vendor/qms_logs/other",
            "/data/vendor/qti-logkit",
            "/data/vendor/qti-logkit/logdata",
            "/data/vendor/qti-logkit/shared-privileged",
            "/data/vendor/qti-logkit/shared-public",
            "/data/vendor/qti-logkit/socket-privileged",
            "/data/vendor/qti-logkit/socket-public",
            "/data/vendor/ramdump",
            "/data/vendor/ramdump/bluetooth",
            "/data/vendor/ramdump/ssrminidump",
            "/data/vendor/ramdump_ssr",
            "/data/vendor/ssrdump",
            "/data/vendor/shsusr",
            "/data/vendor/tombstones",
            "/data/vendor/tombstones/rfs",
            "/data/vendor/tombstones/wifi",
            "/dev/fscklogs",
            "/mnt/vendor/persist/wlan",
            "/sys/fs/pstore",
            "/logdata",
            "/vendor/data/tombstones",
        };

        inline const char *const kVivoTraceDirectories[] = {
            "/sys/kernel/debug/tracing/hyp",
            "/sys/kernel/debug/tracing/instances/mm_events",
            "/sys/kernel/debug/tracing/instances/wifi",
            "/sys/kernel/debug/tracing/per_cpu",
            "/sys/kernel/tracing/hyp",
            "/sys/kernel/tracing/instances/bootreceiver",
            "/sys/kernel/tracing/instances/mm_events",
            "/sys/kernel/tracing/instances/wifi",
            "/sys/kernel/tracing/per_cpu",
        };

        inline const char *const kVivoLogFiles[] = {
            "/data/vendor/esehal/vcd/halbplog",
            "/data/vendor/esehal/vcd/omatabplog",
            "/data/vendor/esehal/vcd/setabplog",
            "/data/vendor/qmipriod/log.txt",
            "/data/vendor/shsusr/shsusr_log.txt",
            "/dev/block/by-name/logdata",
            "/dev/block/bootdevice/by-name/logdata",
            "/dev/block/bootdevice/by-name/ramdump",
            "/dev/block/bootdevice/by-name/rawdump",
            "/dev/kmsg",
            "/proc/blog/step",
            "/proc/kmsg",
            "/proc/last_kmsg",
            "/sys/kernel/boot_adsp/ssr",
            "/sys/kernel/boot_slpi/ssr",
            "/sys/kernel/debug/tracing/trace",
            "/sys/kernel/tracing/trace",
        };

        inline bool copyLogDirectory(const char *source)
        {
            struct stat sourceStat{};
            if (lstat(source, &sourceStat) != 0)
            {
                if (errno == ENOENT)
                {
                    logInfo("未找到 Vivo 日志目录，跳过: %s\n", source);
                    return true;
                }
                logError("检查 Vivo 日志目录失败: %s: %s\n", source, std::strerror(errno));
                return false;
            }

            if (!S_ISDIR(sourceStat.st_mode))
            {
                logError("Vivo 日志路径不是目录，跳过: %s\n", source);
                return false;
            }

            std::string destination = joinPath(kDestinationRoot, stripLeadingSlash(source));
            logInfo("复制 Vivo 日志目录: %s -> %s\n", source, destination.c_str());
            return copyTree(source, destination);
        }

        inline bool copyLogFile(const char *source)
        {
            struct stat sourceStat{};
            if (lstat(source, &sourceStat) != 0)
            {
                if (errno == ENOENT)
                {
                    logInfo("未找到 Vivo 日志现场，跳过: %s\n", source);
                    return true;
                }
                logError("检查 Vivo 日志现场失败: %s: %s\n", source, std::strerror(errno));
                return false;
            }

            if (S_ISDIR(sourceStat.st_mode))
            {
                logError("Vivo 日志现场路径是目录，跳过: %s\n", source);
                return false;
            }

            std::string destination = joinPath(kDestinationRoot, stripLeadingSlash(source));
            if (!ensureDirectory(parentDirectory(destination), 0777))
            {
                return false;
            }

            logInfo("复制 Vivo 日志现场: %s -> %s\n", source, destination.c_str());
            return copyFile(source, destination, sourceStat.st_mode);
        }

        inline bool copyTraceDirectory(const std::string &source, const std::string &destination)
        {
            struct stat sourceStat{};
            if (lstat(source.c_str(), &sourceStat) != 0)
            {
                logError("读取 Vivo trace 路径状态失败: %s: %s\n", source.c_str(), std::strerror(errno));
                return false;
            }

            if (S_ISREG(sourceStat.st_mode))
            {
                if (source.substr(source.find_last_of('/') + 1) != "trace")
                {
                    return true;
                }
                if (!ensureDirectory(parentDirectory(destination), 0777))
                {
                    return false;
                }
                return copyFile(source, destination, sourceStat.st_mode);
            }

            if (!S_ISDIR(sourceStat.st_mode))
            {
                return true;
            }
            if (!ensureDirectory(destination, sourceStat.st_mode & 0777))
            {
                return false;
            }

            DIR *dir = opendir(source.c_str());
            if (dir == nullptr)
            {
                logError("打开 Vivo trace 目录失败: %s: %s\n", source.c_str(), std::strerror(errno));
                return false;
            }

            bool success = true;
            while (dirent *entry = readdir(dir))
            {
                if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0)
                {
                    continue;
                }

                std::string childSource = joinPath(source, entry->d_name);
                std::string childDestination = joinPath(destination, entry->d_name);
                if (!copyTraceDirectory(childSource, childDestination))
                {
                    success = false;
                }
            }
            closedir(dir);
            return success;
        }

        inline bool copyTraceLogDirectory(const char *source)
        {
            struct stat sourceStat{};
            if (lstat(source, &sourceStat) != 0)
            {
                if (errno == ENOENT)
                {
                    return true;
                }
                logError("检查 Vivo trace 目录失败: %s: %s\n", source, std::strerror(errno));
                return false;
            }
            if (!S_ISDIR(sourceStat.st_mode))
            {
                logError("Vivo trace 路径不是目录，跳过: %s\n", source);
                return false;
            }

            std::string destination = joinPath(kDestinationRoot, stripLeadingSlash(source));
            logInfo("复制 Vivo trace 日志目录: %s -> %s\n", source, destination.c_str());
            return copyTraceDirectory(source, destination);
        }

    } // namespace vivocollector

    inline bool collectVivoDumps(int &matchedVivoPathCount, int &copiedVivoPathCount)
    {
        bool success = true;

        for (const char *source : vivocollector::kVivoLogDirectories)
        {
            struct stat sourceStat{};
            if (lstat(source, &sourceStat) != 0)
            {
                if (errno == ENOENT)
                {
                    continue;
                }
                logError("检查 Vivo 日志目录失败: %s: %s\n", source, std::strerror(errno));
                success = false;
                continue;
            }

            ++matchedVivoPathCount;
            if (vivocollector::copyLogDirectory(source))
            {
                ++copiedVivoPathCount;
            }
            else
            {
                success = false;
            }
        }

        for (const char *source : vivocollector::kVivoTraceDirectories)
        {
            struct stat sourceStat{};
            if (lstat(source, &sourceStat) != 0)
            {
                if (errno == ENOENT)
                {
                    continue;
                }
                logError("检查 Vivo trace 目录失败: %s: %s\n", source, std::strerror(errno));
                success = false;
                continue;
            }

            ++matchedVivoPathCount;
            if (vivocollector::copyTraceLogDirectory(source))
            {
                ++copiedVivoPathCount;
            }
            else
            {
                success = false;
            }
        }

        for (const char *source : vivocollector::kVivoLogFiles)
        {
            struct stat sourceStat{};
            if (lstat(source, &sourceStat) != 0)
            {
                if (errno == ENOENT)
                {
                    continue;
                }
                logError("检查 Vivo 日志现场失败: %s: %s\n", source, std::strerror(errno));
                success = false;
                continue;
            }

            ++matchedVivoPathCount;
            if (vivocollector::copyLogFile(source))
            {
                ++copiedVivoPathCount;
            }
            else
            {
                success = false;
            }
        }

        return success && matchedVivoPathCount == copiedVivoPathCount;
    }

} // namespace panicdump

#endif // PANIC_VIVO_COLLECTOR_H_