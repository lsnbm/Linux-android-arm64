#ifndef PANIC_LENOVO_COLLECTOR_H_
#define PANIC_LENOVO_COLLECTOR_H_

#include "../utils/dump_utils.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <unistd.h>

namespace panicdump
{
    namespace lenovocollector
    {

        struct LenovoDumpPath
        {
            const char *source;
        };

        inline const LenovoDumpPath kLenovoDumpPaths[] = {
            {"/cache/rbr_log"},
            {"/rbr/rbr_log"},
            {"/data/debugging"},
            {"/data/debugging/last_kmsg.txt"},
            {"/data/log"},
            {"/data/logswitch"},
            {"/data/misc/bluetooth"},
            {"/data/misc/bootstat"},
            {"/data/misc/boottrace"},
            {"/data/misc/dmesgd"},
            {"/data/misc/gjdw/mdm"},
            {"/data/misc/oplus_update_engine_log"},
            {"/data/misc/perfetto-configs"},
            {"/data/misc/perfetto-traces"},
            {"/data/misc/recovery"},
            {"/data/misc/wifi/wifidump"},
            {"/data/oplus/bsp_kevent"},
            {"/data/oplus/heapdump"},
            {"/data/oplus/kevent"},
            {"/data/oplus/log"},
            {"/data/oplus/mmdump"},
            {"/data/persist_log"},
            {"/data/ramdump"},
            {"/data/tango/log"},
            {"/data/tango/pretrans_log"},
            {"/data/user_de/0/com.android.shell/files/bugreports"},
            {"/data/vendor/audio/memlog"},
            {"/data/vendor/audiohal/audio_dump"},
            {"/data/vendor/audiohal/regdump"},
            {"/data/vendor/display/frame_dump_disp_id_00_builtin"},
            {"/data/vendor/explorer/explorer_logs/log"},
            {"/data/vendor/imslogs"},
            {"/data/vendor/log"},
            {"/data/vendor/mmdump"},
            {"/data/vendor/olog"},
            {"/data/vendor/oplus/log"},
            {"/data/vendor/pddump"},
            {"/data/vendor/qmipriod/log.txt"},
            {"/data/vendor/qms_logs"},
            {"/data/vendor/qti-logkit"},
            {"/data/vendor/ramdump"},
            {"/data/vendor/ramdump/bluetooth"},
            {"/data/vendor/ramdump_ssr"},
            {"/data/vendor/shsusr/shsusr_log.txt"},
            {"/data/vendor/ssrdump"},
            {"/data/vendor/tombstones"},
            {"/data/vendor/tombstones/rfs"},
            {"/data/vendor/videodump"},
            {"/data/vendor/wifi/wifi_trace"},
            {"/vendor/data/tombstones"},
            {"/data/core"},
            {"/mnt/vendor/logfs"},
            {"/mnt/vendor/oplusreserve"},
            {"/mnt/vendor/oplusreserve/media/log/criticalLog"},
            {"/mnt/vendor/oplusreserve/media/log/minidump"},
            {"/mnt/vendor/oplusreserve/media/log/vold"},
            {"/mnt/vendor/oplusreserve/rbr_log"},
            {"/mnt/vendor/oplusreserve/recovery"},
            {"/mnt/vendor/oplusreserve/update_engine_log"},
            {"/mnt/vendor/oplusreserve/factory"},
            {"/mnt/vendor/persist/wlan"},
            {"/sys/fs/pstore/console-ramoops-0"},
            {"/proc/kmsg"},
            {"/proc/minidump_vcpu_stack"},
            {"/dev/block/bootdevice/by-name/ramdump"},
            {"/dev/block/bootdevice/by-name/rawdump"},
            {"/dev/block/by-name/logfs"},
            {"/sys/kernel/debug/ipc_logging/charger_ulog_init/log"},
            {"/sys/kernel/debug/ipc_logging/charger_ulog/log_cont"},
            {"/sys/kernel/debug/tracing/trace"},
            {"/sys/kernel/debug/tracing/instances/wifi/trace"},
            {"/sys/kernel/tracing/trace"},
            {"/sys/kernel/tracing/instances/wifi/trace"},
            {"/sys/kernel/dload/emmc_dload"},
            {"/sys/kernel/dload/dload_mode"},
            {"/sys/module/qcom_ramdump/parameters/enable_dump_collection"},
            {"/sys/module/subsystem_restart/parameters/enable_ramdumps"},
            {"/sys/module/subsystem_restart/parameters/enable_debug"},
            {"/sys/module/subsys_pil_tz/parameters/enable_debug"},
            {"/proc/sys/kernel/hung_task_panic"},
            {"/proc/sys/kernel/printk_devkmsg"},
            {"/proc/oplus_init_watchdog/kick"},
            {"/proc/game_opt/dstate/dump_enable"},
            {"/sys/fs/f2fs/${dev.mnt.blk.userdata}/min_ssr_sections"},
            {"/mnt/oplus/op2"},
            {"/data/persist_log/criticallog"},
            {"/data/persist_log/oplusreserve/media/log/minidumpbackup"},
            {"/data/persist_log/storage/op2storagelog"},
            {"/mnt/vendor/oplusreserve/rbr_log/last_init_boot_failed.log"},
        };

        // 读取 Android 系统属性，供带属性占位的 Lenovo dump 路径展开使用。
        inline std::string readSystemProperty(const char *name)
        {
            char value[PROP_VALUE_MAX]{};
            int length = __system_property_get(name, value);
            if (length <= 0)
            {
                return "";
            }
            return std::string(value, static_cast<size_t>(length));
        }

        // 展开 Lenovo dump 路径模板里的 ${dev.mnt.blk.userdata} 占位符。
        inline bool expandDumpPathTemplate(const char *sourceTemplate, std::string &source)
        {
            source = sourceTemplate;

            const std::string token = "${dev.mnt.blk.userdata}";
            size_t tokenPosition = source.find(token);
            if (tokenPosition == std::string::npos)
            {
                return true;
            }

            std::string userdataBlock = readSystemProperty("dev.mnt.blk.userdata");
            if (userdataBlock.empty())
            {
                logError("无法解析 Lenovo dump 路径属性 dev.mnt.blk.userdata，跳过: %s\n", sourceTemplate);
                return false;
            }

            source.replace(tokenPosition, token.size(), userdataBlock);
            return true;
        }

        // 直接按字节复制 Lenovo 普通文件、伪文件或块设备到目标路径。
        inline bool copyReadableNode(const std::string &source, const std::string &destination)
        {
            std::string destinationParent = parentDirectory(destination);
            if (!ensureDirectory(destinationParent, 0777))
            {
                return false;
            }

            int input = open(source.c_str(), O_RDONLY | O_CLOEXEC);
            if (input < 0)
            {
                logError("打开 Lenovo dump 源路径失败: %s: %s\n", source.c_str(), std::strerror(errno));
                return false;
            }

            int output = open(destination.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
            if (output < 0)
            {
                logError("打开 Lenovo dump 目标路径失败: %s: %s\n", destination.c_str(), std::strerror(errno));
                close(input);
                return false;
            }

            char buffer[65536];
            bool success = true;
            while (true)
            {
                ssize_t readSize = read(input, buffer, sizeof(buffer));
                if (readSize == 0)
                {
                    break;
                }
                if (readSize < 0)
                {
                    logError("读取 Lenovo dump 路径失败: %s: %s\n", source.c_str(), std::strerror(errno));
                    success = false;
                    break;
                }

                char *writePtr = buffer;
                ssize_t remaining = readSize;
                while (remaining > 0)
                {
                    ssize_t written = write(output, writePtr, static_cast<size_t>(remaining));
                    if (written < 0)
                    {
                        logError("写入 Lenovo dump 路径失败: %s: %s\n", destination.c_str(), std::strerror(errno));
                        success = false;
                        break;
                    }
                    writePtr += written;
                    remaining -= written;
                }
                if (!success)
                {
                    break;
                }
            }

            close(output);
            close(input);
            return success;
        }

    } // namespace lenovocollector

    // 在 Lenovo 设备上复制旧版 Lenovo dump 目录集合。
    inline bool collectLenovoDumps(int &matchedDumpCount, int &copiedDumpCount)
    {
        bool success = true;

        for (const lenovocollector::LenovoDumpPath &dumpPath : lenovocollector::kLenovoDumpPaths)
        {
            std::string source;
            if (!lenovocollector::expandDumpPathTemplate(dumpPath.source, source))
            {
                continue;
            }

            struct stat sourceStat{};
            if (lstat(source.c_str(), &sourceStat) != 0)
            {
                if (errno == ENOENT)
                {
                    logError("未找到 Lenovo dump 路径，跳过: %s\n", source.c_str());
                    continue;
                }
                logError("检查 Lenovo dump 路径失败: %s: %s\n", source.c_str(), std::strerror(errno));
                success = false;
                continue;
            }

            ++matchedDumpCount;
            std::string destinationPath = joinPath(kDestinationRoot, stripLeadingSlash(source));
            logInfo("复制 Lenovo dump 路径: %s -> %s\n", source.c_str(), destinationPath.c_str());

            bool copied = S_ISDIR(sourceStat.st_mode)
                              ? copyTree(source, destinationPath)
                              : lenovocollector::copyReadableNode(source, destinationPath);
            if (copied)
            {
                ++copiedDumpCount;
            }
            else
            {
                success = false;
            }
        }

        return success;
    }

} // namespace panicdump

#endif // PANIC_LENOVO_COLLECTOR_H_
