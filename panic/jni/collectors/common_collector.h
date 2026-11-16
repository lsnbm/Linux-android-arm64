#ifndef PANIC_COMMON_COLLECTOR_H_
#define PANIC_COMMON_COLLECTOR_H_

#include "../utils/dump_utils.h"

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <string>
#include <sys/stat.h>
#include <vector>

namespace panicdump
{
    namespace commoncollector
    {

        // 通用输入：所有设备都常见的 init rc 配置目录。
        inline const std::vector<std::string> kInitRoots = {
            "/system/etc/init",
            "/system_ext/etc/init",
            "/vendor/etc/init",
            "/vendor/etc/init/hw",
            "/odm/etc/init",
            "/product/etc/init",
        };

        // 只扫描每个 init 目录当前层级的 .rc 文件，并按原始路径复制到 /sdcard/dump 下。
        inline void copyInitRcFiles(int &matchedCount, int &copiedCount)
        {
            for (const std::string &root : kInitRoots)
            {
                DIR *dir = opendir(root.c_str());
                if (dir == nullptr)
                {
                    logError("打开 init 目录失败: %s: %s\n", root.c_str(), std::strerror(errno));
                    continue;
                }

                while (dirent *entry = readdir(dir))
                {
                    if (std::strcmp(entry->d_name, ".") == 0 || std::strcmp(entry->d_name, "..") == 0 ||
                        !endsWith(entry->d_name, ".rc"))
                    {
                        continue;
                    }

                    std::string sourcePath = joinPath(root, entry->d_name);
                    struct stat sourceStat{};
                    if (lstat(sourcePath.c_str(), &sourceStat) != 0)
                    {
                        logError("读取文件状态失败: %s: %s\n", sourcePath.c_str(), std::strerror(errno));
                        continue;
                    }
                    if (!S_ISREG(sourceStat.st_mode))
                    {
                        continue;
                    }

                    matchedCount++;
                    std::string destinationPath = joinPath(kDestinationRoot, stripLeadingSlash(sourcePath));
                    if (!ensureDirectory(parentDirectory(destinationPath), 0777))
                    {
                        continue;
                    }

                    logInfo("复制 rc 文件: %s -> %s\n", sourcePath.c_str(), destinationPath.c_str());
                    if (copyFile(sourcePath, destinationPath, sourceStat.st_mode))
                    {
                        copiedCount++;
                    }
                }

                closedir(dir);
            }
        }

        // 只列出根目录 / 本身的目录项，不进入任何子目录。
        inline bool writeRootDirectoryListing(int &listedCount)
        {
            DIR *rootDir = opendir("/");
            if (rootDir == nullptr)
            {
                logError("打开根目录失败: %s\n", std::strerror(errno));
                return false;
            }

            logFileOnly("\n== root directory listing: / ==\n");
            while (dirent *rootEntry = readdir(rootDir))
            {
                if (std::strcmp(rootEntry->d_name, ".") == 0 || std::strcmp(rootEntry->d_name, "..") == 0)
                {
                    continue;
                }

                logFileOnly("%s\n", rootEntry->d_name);
                listedCount++;
            }
            logFileOnly("== end root directory listing ==\n");

            closedir(rootDir);
            return true;
        }

    } // namespace commoncollector

    // 执行所有设备通用的收集项：init rc 文件和根目录列表。
    inline bool collectCommonFiles(int &matchedRcCount, int &copiedRcCount, int &listedRootFileCount)
    {
        commoncollector::copyInitRcFiles(matchedRcCount, copiedRcCount);
        bool wroteRootFileList = commoncollector::writeRootDirectoryListing(listedRootFileCount);
        return copiedRcCount == matchedRcCount && wroteRootFileList;
    }

} // namespace panicdump

#endif // PANIC_COMMON_COLLECTOR_H_