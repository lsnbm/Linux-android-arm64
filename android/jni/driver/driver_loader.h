#pragma once

#include <cerrno>
#include <cstdio>
#include <cstring>
#include <limits>
#include <optional>
#include <print>
#include <string>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include "logger/logger.h"

#ifndef SYS_init_module
#define SYS_init_module 105
#endif

namespace DriverLoader
{
    inline std::vector<std::string> GetModuleCandidates()
    {
        struct utsname kernel_info{};
        if (uname(&kernel_info) != 0)
        {
            LS_LOGE_TAG("DriverLoader", "获取内核版本失败: errno=%d (%s)", errno, strerror(errno));
            std::println(stderr, "[DriverLoader] 获取内核版本失败: errno={} ({})", errno, strerror(errno));
            std::fflush(stderr);
            return {};
        }

        int major = 0;
        int minor = 0;
        if (std::sscanf(kernel_info.release, "%d.%d", &major, &minor) != 2)
        {
            LS_LOGE_TAG("DriverLoader", "无法解析内核版本: %s", kernel_info.release);
            std::println(stderr, "[DriverLoader] 无法解析内核版本: {}", kernel_info.release);
            std::fflush(stderr);
            return {};
        }

        std::vector<std::string> candidates;
        if (major == 6 && minor == 18) candidates.emplace_back("6.18-Android17.ko");
        else if (major == 6 && minor == 12) candidates.emplace_back("6.12-Android16.ko");
        else if (major == 6 && minor == 6) candidates.emplace_back("6.6-Android15.ko");
        else if (major == 6 && minor == 1) candidates.emplace_back("6.1-Android14.ko");
        else if (major == 5 && minor == 15) candidates.emplace_back("5.15-Android13.ko");
        else if (major == 5 && minor == 10)
        {
            candidates.emplace_back("5.10-Android13.ko");
            candidates.emplace_back("5.10-Android12.ko");
        }
        else
        {
            LS_LOGE_TAG("DriverLoader", "不支持的内核版本: %s", kernel_info.release);
            std::println(stderr, "[DriverLoader] 不支持的内核版本: {}", kernel_info.release);
            std::fflush(stderr);
            return {};
        }

        LS_LOGI_TAG("DriverLoader", "内核版本 %d.%d，候选模块数量=%zu", major, minor, candidates.size());
        std::println(stderr, "[DriverLoader] 内核版本 {}.{}, 候选模块数量={}", major, minor, candidates.size());
        std::fflush(stderr);
        return candidates;
    }

    inline bool ReadCommandOutput(const std::string &command, std::vector<uint8_t> &output)
    {
        constexpr size_t MAX_MODULE_SIZE = 64ULL * 1024 * 1024;
        uint8_t buffer[16 * 1024];
        output.clear();

        FILE *stream = popen(command.c_str(), "r");
        if (!stream) return false;

        while (true)
        {
            const size_t bytes_read = fread(buffer, 1, sizeof(buffer), stream);
            if (bytes_read > 0)
            {
                if (output.size() > MAX_MODULE_SIZE - bytes_read)
                {
                    pclose(stream);
                    output.clear();
                    errno = EFBIG;
                    return false;
                }
                output.insert(output.end(), buffer, buffer + bytes_read);
            }
            if (bytes_read < sizeof(buffer)) break;
        }

        const bool read_ok = ferror(stream) == 0;
        const int command_status = pclose(stream);
        if (!read_ok || command_status == -1 || !WIFEXITED(command_status) || WEXITSTATUS(command_status) != 0 || output.empty())
        {
            output.clear();
            return false;
        }
        return true;
    }

    inline bool DownloadModule(const std::string &module_name, std::vector<uint8_t> &module_image)
    {
        const std::string url = "https://ghfast.top/https://raw.githubusercontent.com/lsnbm/Linux-android-arm64/main/lsdriver/byte_stream/" + module_name;
        const std::string curl_command = "curl -fLsS --connect-timeout 15 --max-time 40 '" + url + "'";
        if (ReadCommandOutput(curl_command, module_image)) return true;

        LS_LOGE_TAG("DriverLoader", "下载模块失败: %s", url.c_str());
        std::println(stderr, "[DriverLoader] 下载模块失败: {}", url);
        std::fflush(stderr);
        return false;
    }

    inline void PrintDmesgTail()
    {
        FILE *stream = popen("dmesg 2>&1 | tail -n 80", "r");
        if (!stream)
        {
            LS_LOGE_TAG("DriverLoader", "读取 dmesg 失败: errno=%d (%s)", errno, strerror(errno));
            std::println(stderr, "[DriverLoader] 读取 dmesg 失败: errno={} ({})", errno, strerror(errno));
            std::fflush(stderr);
            return;
        }

        char line[512];
        while (fgets(line, sizeof(line), stream))
        {
            line[strcspn(line, "\r\n")] = '\0';
            LS_LOGE_TAG("DriverLoader", "dmesg: %s", line);
            std::println(stderr, "[DriverLoader] dmesg: {}", line);
            std::fflush(stderr);
        }
        pclose(stream);
    }

    inline int LoadKernelModule()
    {
        const std::vector<std::string> candidates = GetModuleCandidates();
        if (candidates.empty()) return -ENOTSUP;

        int last_error = -EIO;
        for (const std::string &module_name : candidates)
        {
            LS_LOGI_TAG("DriverLoader", "尝试下载并加载: %s", module_name.c_str());
            std::println(stderr, "[DriverLoader] 尝试下载并加载: {}", module_name);
            std::fflush(stderr);

            std::vector<uint8_t> module_image;
            if (!DownloadModule(module_name, module_image))
            {
                last_error = -EIO;
                continue;
            }
            if (module_image.size() > std::numeric_limits<unsigned long>::max())
            {
                LS_LOGE_TAG("DriverLoader", "模块过大: %s", module_name.c_str());
                std::println(stderr, "[DriverLoader] 模块过大: {}", module_name);
                std::fflush(stderr);
                last_error = -EFBIG;
                continue;
            }

            errno = 0;
            const long result = syscall(SYS_init_module, module_image.data(), static_cast<unsigned long>(module_image.size()), "");
            if (result == 0)
            {
                LS_LOGI_TAG("DriverLoader", "内核模块加载成功: %s", module_name.c_str());
                std::println(stderr, "[DriverLoader] 内核模块加载成功: {}", module_name);
                std::fflush(stderr);
                return 0;
            }
            last_error = -errno;
            LS_LOGE_TAG("DriverLoader", "内核模块加载失败: %s errno=%d (%s)", module_name.c_str(), -last_error, strerror(-last_error));
            std::println(stderr, "[DriverLoader] 内核模块加载失败: {} errno={} ({})", module_name, -last_error, strerror(-last_error));
            std::fflush(stderr);
            PrintDmesgTail();
        }

        return last_error;
    }
} // namespace DriverLoader