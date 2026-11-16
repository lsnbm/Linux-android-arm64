#ifndef PANIC_LOG_UTILS_H_
#define PANIC_LOG_UTILS_H_

// 头文件即实现：本文件只放 inline 定义，include 后即可直接使用。
// 约定：辅助函数定义在前，logInfo/logError/logFileOnly 等对外函数定义在后。

#include <cerrno>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <string>

namespace panicdump
{

    // 程序运行日志和根目录列表统一写入这个文件。
    inline const std::string kRuntimeLogFileName = "root_ls_la.txt";

    // 拼接日志目录和日志文件名，避免重复或漏掉路径分隔符。
    inline std::string joinLogPath(const std::string &directory, const std::string &fileName)
    {
        if (directory.empty() || directory == "/")
        {
            return directory + fileName;
        }
        return directory + "/" + fileName;
    }

    // 返回全局运行日志文件句柄的引用，供打开、写入和关闭共用同一状态。
    inline FILE *&runtimeLogFile()
    {
        static FILE *file = nullptr;
        return file;
    }

    // 在指定输出目录下打开运行日志文件，后续 logInfo/logError 会同时写入该文件。
    inline bool openRuntimeLogFile(const std::string &destinationRoot)
    {
        FILE *&file = runtimeLogFile();
        if (file != nullptr)
        {
            std::fclose(file);
            file = nullptr;
        }

        const std::string logPath = joinLogPath(destinationRoot, kRuntimeLogFileName);
        file = std::fopen(logPath.c_str(), "w");
        if (file == nullptr)
        {
            std::fprintf(stderr, "打开运行日志文件失败: %s: %s\n", logPath.c_str(), std::strerror(errno));
            return false;
        }
        return true;
    }

    // 关闭运行日志文件，并清空全局文件句柄。
    inline void closeRuntimeLogFile()
    {
        FILE *&file = runtimeLogFile();
        if (file != nullptr)
        {
            std::fclose(file);
            file = nullptr;
        }
    }

    // 把同一条格式化日志同时写入指定控制台流和运行日志文件。
    inline void logToConsoleAndFile(FILE *stream, const char *format, va_list args)
    {
        va_list fileArgs;
        va_copy(fileArgs, args);

        std::vfprintf(stream, format, args);
        std::fflush(stream);

        FILE *file = runtimeLogFile();
        if (file != nullptr)
        {
            std::vfprintf(file, format, fileArgs);
            std::fflush(file);
        }

        va_end(fileArgs);
    }

    // 输出普通运行信息到 stdout，并同步写入运行日志文件。
    inline void logInfo(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        logToConsoleAndFile(stdout, format, args);
        va_end(args);
    }

    // 输出错误信息到 stderr，并同步写入运行日志文件。
    inline void logError(const char *format, ...)
    {
        va_list args;
        va_start(args, format);
        logToConsoleAndFile(stderr, format, args);
        va_end(args);
    }

    // 只写入运行日志文件，不在控制台打印；用于根目录列表这类附加内容。
    inline void logFileOnly(const char *format, ...)
    {
        FILE *file = runtimeLogFile();
        if (file == nullptr)
        {
            return;
        }

        va_list args;
        va_start(args, format);
        std::vfprintf(file, format, args);
        va_end(args);
        std::fflush(file);
    }

} // namespace panicdump

#endif // PANIC_LOG_UTILS_H_