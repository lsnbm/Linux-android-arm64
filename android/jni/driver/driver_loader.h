#pragma once

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstring>
#include <elf.h>
#include <limits>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <sys/system_properties.h>
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
    namespace Vivo
    {
        // vivo 内核会把厂商标记写入 vermagic，但 uname -r 不会返回这个字段。
        inline constexpr std::string_view VermagicSuffix = "vivo";

        inline bool IsDevice()
        {
            constexpr const char *property_names[] = {
                "ro.product.manufacturer",
                "ro.product.brand",
                "ro.product.vendor.manufacturer",
                "ro.product.odm.manufacturer",
            };

            for (const char *property_name : property_names)
            {
                char property_value[PROP_VALUE_MAX]{};
                const int property_length = __system_property_get(property_name, property_value);
                if (property_length <= 0) continue;

                std::string normalized_value(property_value, static_cast<size_t>(property_length));
                for (char &character : normalized_value)
                {
                    character = static_cast<char>(std::tolower(static_cast<unsigned char>(character)));
                }

                if (normalized_value == "vivo" || normalized_value.starts_with("vivo-") || normalized_value.starts_with("vivo_") || normalized_value.starts_with("vivo ")) return true;
            }
            return false;
        }
    }

    inline bool IsElfRangeValid(size_t offset, size_t size, size_t image_size)
    {
        return offset <= image_size && size <= image_size - offset;
    }

    inline bool ReadElf64SectionHeader(const std::vector<uint8_t> &image, const Elf64_Ehdr &header, size_t index, Elf64_Shdr &section)
    {
        if (header.e_shentsize < sizeof(Elf64_Shdr) || index >= header.e_shnum) return false;
        if (header.e_shoff > image.size()) return false;
        const size_t section_table_offset = static_cast<size_t>(header.e_shoff);
        const size_t section_table_size = static_cast<size_t>(header.e_shentsize) * static_cast<size_t>(header.e_shnum);
        if (!IsElfRangeValid(section_table_offset, section_table_size, image.size())) return false;

        const size_t section_offset = section_table_offset + static_cast<size_t>(header.e_shentsize) * index;
        std::memcpy(&section, image.data() + section_offset, sizeof(section));
        return true;
    }

    // 通用 vermagic 修补：替换模块内核版本，并按需追加目标内核要求的厂商后缀。
    inline bool PatchVermagic(std::vector<uint8_t> &module_image, std::string_view required_suffix)
    {
        if (module_image.size() < sizeof(Elf64_Ehdr)) return false;

        Elf64_Ehdr elf_header{};
        std::memcpy(&elf_header, module_image.data(), sizeof(elf_header));
        if (std::memcmp(elf_header.e_ident, ELFMAG, SELFMAG) != 0 || elf_header.e_ident[EI_CLASS] != ELFCLASS64 || elf_header.e_ident[EI_DATA] != ELFDATA2LSB || elf_header.e_shnum == 0 || elf_header.e_shstrndx >= elf_header.e_shnum)
        {
            return false;
        }

        Elf64_Shdr section_name_table{};
        if (!ReadElf64SectionHeader(module_image, elf_header, elf_header.e_shstrndx, section_name_table) || section_name_table.sh_type != SHT_STRTAB || section_name_table.sh_offset > module_image.size() || section_name_table.sh_size > module_image.size() - section_name_table.sh_offset)
        {
            return false;
        }

        const size_t section_name_table_offset = static_cast<size_t>(section_name_table.sh_offset);
        const size_t section_name_table_size = static_cast<size_t>(section_name_table.sh_size);
        Elf64_Shdr modinfo_section{};
        size_t modinfo_index = 0;
        for (size_t index = 0; index < elf_header.e_shnum; ++index)
        {
            Elf64_Shdr section{};
            if (!ReadElf64SectionHeader(module_image, elf_header, index, section) || section.sh_name >= section_name_table_size) continue;

            const char *section_name = reinterpret_cast<const char *>(module_image.data() + section_name_table_offset + section.sh_name);
            const size_t remaining_name_size = section_name_table_size - section.sh_name;
            const void *terminator = std::memchr(section_name, '\0', remaining_name_size);
            if (terminator == nullptr) continue;

            const size_t section_name_length = static_cast<const char *>(terminator) - section_name;
            if (std::string_view(section_name, section_name_length) == ".modinfo")
            {
                modinfo_section = section;
                modinfo_index = index;
                break;
            }
        }
        if (modinfo_section.sh_type != SHT_PROGBITS || modinfo_section.sh_offset > module_image.size() || modinfo_section.sh_size > module_image.size() - modinfo_section.sh_offset)
        {
            return false;
        }

        const size_t modinfo_offset = static_cast<size_t>(modinfo_section.sh_offset);
        const size_t modinfo_size = static_cast<size_t>(modinfo_section.sh_size);
        const char vermagic_prefix[] = "vermagic=";
        const auto vermagic_begin = std::search(module_image.begin() + modinfo_offset, module_image.begin() + modinfo_offset + modinfo_size, std::begin(vermagic_prefix), std::end(vermagic_prefix) - 1);
        if (vermagic_begin == module_image.begin() + modinfo_offset + modinfo_size) return false;

        const auto vermagic_end = std::find(vermagic_begin, module_image.begin() + modinfo_offset + modinfo_size, uint8_t{0});
        if (vermagic_end == module_image.begin() + modinfo_offset + modinfo_size) return false;

        const std::string module_vermagic(vermagic_begin, vermagic_end);
        const size_t prefix_length = sizeof(vermagic_prefix) - 1;
        const size_t release_end = module_vermagic.find(' ', prefix_length);
        if (release_end == std::string::npos) return false;

        struct utsname kernel_info{};
        if (uname(&kernel_info) != 0) return false;

        std::string target_vermagic = "vermagic=";
        target_vermagic += kernel_info.release;
        target_vermagic += module_vermagic.substr(release_end);
        if (!required_suffix.empty())
        {
            const std::string suffix = " " + std::string(required_suffix);
            if (!target_vermagic.ends_with(suffix) && !target_vermagic.ends_with(suffix + " ")) target_vermagic += suffix + " ";
            else if (!target_vermagic.ends_with(' ')) target_vermagic += ' ';
        }

        const size_t vermagic_offset = static_cast<size_t>(vermagic_begin - module_image.begin());
        const size_t vermagic_size = static_cast<size_t>(vermagic_end - vermagic_begin);
        std::vector<uint8_t> patched_modinfo;
        patched_modinfo.reserve(modinfo_size + target_vermagic.size() - vermagic_size);
        patched_modinfo.insert(patched_modinfo.end(), module_image.begin() + modinfo_offset, module_image.begin() + vermagic_offset);
        patched_modinfo.insert(patched_modinfo.end(), target_vermagic.begin(), target_vermagic.end());
        patched_modinfo.push_back(0);
        patched_modinfo.insert(patched_modinfo.end(), vermagic_end + 1, module_image.begin() + modinfo_offset + modinfo_size);

        if (module_image.size() > std::numeric_limits<Elf64_Off>::max() || patched_modinfo.size() > std::numeric_limits<Elf64_Xword>::max()) return false;
        const Elf64_Off patched_modinfo_offset = static_cast<Elf64_Off>(module_image.size());
        module_image.insert(module_image.end(), patched_modinfo.begin(), patched_modinfo.end());

        modinfo_section.sh_offset = patched_modinfo_offset;
        modinfo_section.sh_size = static_cast<Elf64_Xword>(patched_modinfo.size());
        if (elf_header.e_shoff > module_image.size() || static_cast<size_t>(elf_header.e_shentsize) * modinfo_index > module_image.size() - static_cast<size_t>(elf_header.e_shoff)) return false;
        const size_t modinfo_header_offset = static_cast<size_t>(elf_header.e_shoff) + static_cast<size_t>(elf_header.e_shentsize) * modinfo_index;
        if (!IsElfRangeValid(modinfo_header_offset, sizeof(modinfo_section), module_image.size())) return false;
        std::memcpy(module_image.data() + modinfo_header_offset, &modinfo_section, sizeof(modinfo_section));

        LS_LOGI_TAG("DriverLoader", "vermagic 修补: %s -> %s", module_vermagic.c_str(), target_vermagic.c_str());
        std::println(stderr, "[DriverLoader] vermagic 修补: {} -> {}", module_vermagic, target_vermagic);
        std::fflush(stderr);
        return true;
    }

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
        const std::string url = "https://raw.githubusercontent.com/lsnbm/Linux-android-arm64/main/lsdriver/byte_stream/" + module_name;
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

        // vivo 内核要求 vermagic 额外包含厂商 token，而 uname -r 只提供版本主体；
        // 因此仅对识别出的 vivo 设备修补，其他设备沿用下载模块的原始内容。
        const bool is_vivo_device = Vivo::IsDevice();
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

            if (is_vivo_device && !PatchVermagic(module_image, Vivo::VermagicSuffix))
            {
                LS_LOGE_TAG("DriverLoader", "vivo 设备模块 vermagic 修补失败: %s", module_name.c_str());
                std::println(stderr, "[DriverLoader] vivo 设备模块 vermagic 修补失败: {}", module_name);
                std::fflush(stderr);
                last_error = -EINVAL;
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