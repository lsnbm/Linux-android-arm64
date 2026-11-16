#ifndef PANIC_SAMSUNG_COLLECTOR_H_
#define PANIC_SAMSUNG_COLLECTOR_H_

#include "../utils/dump_utils.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <string>
#include <unistd.h>

namespace panicdump
{
    namespace samsungcollector
    {

        struct SamsungProcNode
        {
            const char *source;
            const char *label;
        };

        // Samsung 设备常见的 proc 崩溃日志节点。
        inline constexpr const char *kSamsungResetKlogPath = "/proc/reset_klog";
        inline constexpr const char *kSamsungLastKmsgPath = "/proc/last_kmsg";

        inline const SamsungProcNode kSamsungProcNodes[] = {
            {kSamsungResetKlogPath, "reset_klog"},
            {kSamsungLastKmsgPath, "last_kmsg"},
        };

        inline bool copyReadableProcNode(const std::string &source, const std::string &destination, const char *label)
        {
            if (!ensureDirectory(parentDirectory(destination), 0777))
            {
                return false;
            }

            logInfo("复制三星 %s: %s -> %s\n", label, source.c_str(), destination.c_str());
            return copyFile(source, destination, 0644);
        }

    } // namespace samsungcollector

    // 执行 Samsung 专用崩溃日志收集；不存在的节点视为正常跳过。
    inline bool collectSamsungProcNodes(bool &copiedResetKlog, bool &copiedLastKmsg)
    {
        bool success = true;

        for (const samsungcollector::SamsungProcNode &procNode : samsungcollector::kSamsungProcNodes)
        {
            if (access(procNode.source, F_OK) != 0)
            {
                if (errno == ENOENT)
                {
                    logError("未找到三星 %s 节点，跳过: %s\n", procNode.label, procNode.source);
                    continue;
                }
                logError("检查三星 %s 节点失败: %s: %s\n", procNode.label, procNode.source, std::strerror(errno));
                success = false;
                continue;
            }

            std::string destinationPath = joinPath(kDestinationRoot, stripLeadingSlash(procNode.source));
            bool copied = samsungcollector::copyReadableProcNode(procNode.source, destinationPath, procNode.label);
            if (std::strcmp(procNode.label, "reset_klog") == 0)
            {
                copiedResetKlog = copied;
            }
            else if (std::strcmp(procNode.label, "last_kmsg") == 0)
            {
                copiedLastKmsg = copied;
            }
            if (!copied)
            {
                success = false;
            }
        }

        return success;
    }

} // namespace panicdump

#endif // PANIC_SAMSUNG_COLLECTOR_H_