#ifndef PANIC_DUMP_UTILS_H_
#define PANIC_DUMP_UTILS_H_

// 头文件即实现：通用 dump 工具都在这里用 inline 定义。
// 约定：路径和小工具函数定义在前，后面的复制函数再调用它们。

#include "log_utils.h"

#include <cerrno>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <string>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

namespace panicdump
{

    // 所有收集结果统一写入这个目录。
    inline const std::string kDestinationRoot = "/sdcard/dump";

    // 拼接两个路径片段，避免重复或漏掉路径分隔符。
    inline std::string joinPath(const std::string &left, const std::string &right)
    {
        if (left.empty() || left == "/")
        {
            return left + right;
        }
        return left + "/" + right;
    }

    // 判断字符串是否以指定后缀结尾，用于筛选 .rc 文件。
    inline bool endsWith(const std::string &value, const std::string &suffix)
    {
        return value.size() >= suffix.size() &&
               value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
    }

    // 去掉绝对路径开头的 /，方便把系统路径映射到目标目录下。
    inline std::string stripLeadingSlash(const std::string &path)
    {
        return !path.empty() && path[0] == '/' ? path.substr(1) : path;
    }

    // 取文件路径的父目录，用于复制文件前先创建目标目录。
    inline std::string parentDirectory(const std::string &path)
    {
        size_t slash = path.find_last_of('/');
        if (slash == std::string::npos)
        {
            return "";
        }
        if (slash == 0)
        {
            return "/";
        }
        return path.substr(0, slash);
    }

    // 递归创建目录，行为等价于 mkdir -p。
    inline bool ensureDirectory(const std::string &path, mode_t mode)
    {
        if (path.empty())
        {
            return true;
        }

        std::string current;
        size_t index = 0;
        if (path[0] == '/')
        {
            current = "/";
            index = 1;
        }

        while (index <= path.size())
        {
            size_t slash = path.find('/', index);
            std::string part = path.substr(index, slash == std::string::npos ? std::string::npos : slash - index);
            if (!part.empty())
            {
                current = current == "/" ? current + part : current + "/" + part;
                if (mkdir(current.c_str(), mode) != 0 && errno != EEXIST)
                {
                    logError("创建目录失败: %s: %s\n", current.c_str(), std::strerror(errno));
                    return false;
                }
            }
            if (slash == std::string::npos)
            {
                break;
            }
            index = slash + 1;
        }
        return true;
    }

    // 复制普通文件内容，并尽量保留原始权限位。
    inline bool copyFile(const std::string &source, const std::string &destination, mode_t mode)
    {
        bool nonBlockingKernelLog = source == "/dev/kmsg" || source == "/proc/kmsg";
        int inputFlags = O_RDONLY | O_CLOEXEC;
        if (nonBlockingKernelLog)
        {
            inputFlags |= O_NONBLOCK;
        }

        int input = open(source.c_str(), inputFlags);
        if (input < 0)
        {
            logError("打开源文件失败: %s: %s\n", source.c_str(), std::strerror(errno));
            return false;
        }

        int output = open(destination.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, mode & 0777);
        if (output < 0)
        {
            logError("打开目标文件失败: %s: %s\n", destination.c_str(), std::strerror(errno));
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
                if (nonBlockingKernelLog && (errno == EAGAIN || errno == EWOULDBLOCK))
                {
                    break;
                }
                logError("读取文件失败: %s: %s\n", source.c_str(), std::strerror(errno));
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
                    logError("写入文件失败: %s: %s\n", destination.c_str(), std::strerror(errno));
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

    // 递归复制目录树；普通文件会复制，目录会继续展开，其他类型文件会跳过。
    inline bool copyTree(const std::string &source, const std::string &destination)
    {
        struct stat sourceStat{};
        if (lstat(source.c_str(), &sourceStat) != 0)
        {
            logError("读取文件状态失败: %s: %s\n", source.c_str(), std::strerror(errno));
            return false;
        }

        if (S_ISDIR(sourceStat.st_mode))
        {
            if (!ensureDirectory(destination, sourceStat.st_mode & 0777))
            {
                return false;
            }

            DIR *dir = opendir(source.c_str());
            if (dir == nullptr)
            {
                logError("打开目录失败: %s: %s\n", source.c_str(), std::strerror(errno));
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
                if (!copyTree(childSource, childDestination))
                {
                    success = false;
                }
            }
            closedir(dir);
            return success;
        }

        if (S_ISREG(sourceStat.st_mode))
        {
            return copyFile(source, destination, sourceStat.st_mode);
        }

        logError("跳过非普通文件: %s\n", source.c_str());
        return true;
    }

} // namespace panicdump

#endif // PANIC_DUMP_UTILS_H_