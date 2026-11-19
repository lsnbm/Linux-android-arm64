#pragma once

#include <algorithm>
#include <cctype>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dlfcn.h>
#include <elf.h>
#include <limits>
#include <optional>
#include <print>
#include <string>
#include <string_view>
#include <sys/klog.h>
#include <sys/stat.h>
#include <sys/system_properties.h>
#include <sys/syscall.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <vector>

#ifndef SYS_init_module
#define SYS_init_module 105
#endif
//不知道为何很多设备没有curl命令行工具，但是有curl库
namespace Curl
{
    // libcurl 公开 C ABI 的稳定值；不要求迁移本头文件时额外携带 curl 开发头。
    // 运行设备须有可加载的 libcurl.so；
    enum Option : int
    {
        WriteData = 10001,
        Url = 10002,
        WriteFunction = 20011,
        Timeout = 13,
        FailOnError = 45,
        FollowLocation = 52,
        VerifyPeer = 64,
        CaInfo = 10065,
        MaxRedirects = 68,
        ConnectTimeout = 78,
        VerifyHost = 81,
        CaPath = 10097,
        NoSignal = 99,
        MaxFileSize = 114,
        Protocols = 181,
        RedirectProtocols = 182,
    };
    inline constexpr int ResponseCode = 0x200002;
    inline constexpr size_t MaxModuleSize = 64ULL * 1024 * 1024;

    // curl_version_info(CURLVERSION_FIRST) 返回结构的固定前缀。
    struct VersionInfo
    {
        int age;
        const char *version;
        unsigned version_number;
        const char *host;
        int features;
    };

    struct Api
    {
        void *library = nullptr;
        int (*global_init)(long) = nullptr;
        const VersionInfo *(*version_info)(int) = nullptr;
        void *(*easy_init)() = nullptr;
        int (*easy_setopt)(void *, int, ...) = nullptr;
        int (*easy_perform)(void *) = nullptr;
        int (*easy_getinfo)(void *, int, ...) = nullptr;
        void (*easy_cleanup)(void *) = nullptr;
        const char *(*easy_strerror)(int) = nullptr;
        const char *error = "libcurl.so 不可用";

        Api()
        {
            library = dlopen("libcurl.so", RTLD_NOW | RTLD_LOCAL);
            if (!library) return;
            global_init = reinterpret_cast<decltype(global_init)>(dlsym(library, "curl_global_init"));
            version_info = reinterpret_cast<decltype(version_info)>(dlsym(library, "curl_version_info"));
            easy_init = reinterpret_cast<decltype(easy_init)>(dlsym(library, "curl_easy_init"));
            easy_setopt = reinterpret_cast<decltype(easy_setopt)>(dlsym(library, "curl_easy_setopt"));
            easy_perform = reinterpret_cast<decltype(easy_perform)>(dlsym(library, "curl_easy_perform"));
            easy_getinfo = reinterpret_cast<decltype(easy_getinfo)>(dlsym(library, "curl_easy_getinfo"));
            easy_cleanup = reinterpret_cast<decltype(easy_cleanup)>(dlsym(library, "curl_easy_cleanup"));
            easy_strerror = reinterpret_cast<decltype(easy_strerror)>(dlsym(library, "curl_easy_strerror"));
            if (!global_init || !version_info || !easy_init || !easy_setopt || !easy_perform || !easy_getinfo || !easy_cleanup || !easy_strerror)
            {
                error = "libcurl 缺少必要接口";
                return;
            }
            if (global_init(3L) != 0)
            {
                error = "libcurl 全局初始化失败";
                return;
            }
            const VersionInfo *info = version_info(0);
            // NOSIGNAL 下同步 DNS 不能保证超时，要求 SSL 与异步解析均可用。
            if (!info || !(info->features & (1 << 2)) || !(info->features & (1 << 7)))
            {
                error = "libcurl 需要 HTTPS 和异步 DNS 支持";
                return;
            }
            error = nullptr;
        }

        // 保留到进程退出，避免超时后尚未退出的 DNS 线程执行已卸载代码。
        // 不做进程级 global_cleanup，以免影响迁移项目中其他 libcurl 使用方。
    };

    inline const Api &GetApi()
    {
        static const Api api;
        return api;
    }

    struct Transfer
    {
        const Api &api;
        void *handle;
        std::vector<uint8_t> &output;
        int result = 0;
        int write_error = 0;

        Transfer(const Api &api, std::vector<uint8_t> &output) : api(api), handle(api.easy_init()), output(output)
        {
        }
        ~Transfer()
        {
            if (handle) api.easy_cleanup(handle);
        }
        Transfer(const Transfer &) = delete;
        Transfer &operator=(const Transfer &) = delete;

        template <typename T> void Set(Option option, T value)
        {
            if (result == 0) result = api.easy_setopt(handle, option, value);
        }

        static size_t Write(char *data, size_t size, size_t count, void *context) noexcept
        {
            auto &transfer = *static_cast<Transfer *>(context);
            if (size == 0 || count == 0) return 0;
            if (count > MaxModuleSize / size || transfer.output.size() > MaxModuleSize - size * count)
            {
                transfer.write_error = EFBIG;
                return 0;
            }
            const size_t bytes = size * count;
            try
            {
                const auto *begin = reinterpret_cast<const uint8_t *>(data);
                transfer.output.insert(transfer.output.end(), begin, begin + bytes);
            }
            catch (...)
            {
                transfer.write_error = ENOMEM;
                return 0;
            }
            return bytes;
        }
    };

    inline const char *GetCaPath()
    {
        const char *path = "/apex/com.android.conscrypt/cacerts";
        struct stat info{};
        if (stat(path, &info) != 0)
        {
            if (errno != ENOENT && errno != ENOTDIR) return nullptr;
            path = "/system/etc/security/cacerts";
            if (stat(path, &info) != 0) return nullptr;
        }
        if (!S_ISDIR(info.st_mode))
        {
            errno = ENOTDIR;
            return nullptr;
        }
        if (access(path, R_OK | X_OK) != 0) return nullptr;
        return path;
    }

    inline bool DownloadHttps(const std::string &url, std::vector<uint8_t> &output)
    {
        output.clear();
        if (!url.starts_with("https://") || url.find('\0') != std::string::npos)
        {
            errno = EINVAL;
            return false;
        }
        const auto &api = GetApi();
        if (api.error)
        {
            std::println(stderr, "[DriverLoader] {}", api.error);
            errno = ENOSYS;
            return false;
        }
        const char *ca_path = GetCaPath();
        if (!ca_path) return false;
        Transfer transfer(api, output);
        if (!transfer.handle)
        {
            errno = ENOMEM;
            return false;
        }
        transfer.Set(Url, url.c_str());
        transfer.Set(WriteFunction, &Transfer::Write);
        transfer.Set(WriteData, static_cast<void *>(&transfer));
        transfer.Set(FailOnError, 1L);
        transfer.Set(FollowLocation, 1L);
        transfer.Set(MaxRedirects, 20L);
        transfer.Set(ConnectTimeout, 15L);
        transfer.Set(Timeout, 40L);
        transfer.Set(NoSignal, 1L);
        transfer.Set(VerifyPeer, 1L);
        transfer.Set(VerifyHost, 2L);
        transfer.Set(CaInfo, static_cast<const char *>(nullptr));
        transfer.Set(CaPath, ca_path);
        transfer.Set(Protocols, 2L); // CURLPROTO_HTTPS：初始请求和重定向均不允许降级。
        transfer.Set(RedirectProtocols, 2L);
        transfer.Set(MaxFileSize, static_cast<long>(MaxModuleSize));

        long status = 0;
        if (transfer.result == 0) transfer.result = api.easy_perform(transfer.handle);
        if (transfer.result == 0) transfer.result = api.easy_getinfo(transfer.handle, ResponseCode, &status);
        if (transfer.result == 0 && status >= 200 && status < 300 && !output.empty()) return true;

        const int error = transfer.write_error ? transfer.write_error : (transfer.result == 28 ? ETIMEDOUT : EIO);
        output.clear();
        std::println(stderr, "[DriverLoader] HTTPS 下载失败: curl={} ({}), HTTP={}, errno={}", transfer.result, api.easy_strerror(transfer.result), status, error);
        std::fflush(stderr);
        errno = error;
        return false;
    }
} // namespace Curl

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
    } // namespace Vivo

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

        std::println(stderr, "[DriverLoader] vermagic 修补: {} -> {}", module_vermagic, target_vermagic);
        std::fflush(stderr);
        return true;
    }

    inline std::vector<std::string> GetModuleCandidates()
    {
        struct utsname kernel_info{};
        if (uname(&kernel_info) != 0)
        {
            std::println(stderr, "[DriverLoader] 获取内核版本失败: errno={} ({})", errno, strerror(errno));
            std::fflush(stderr);
            return {};
        }

        int major = 0;
        int minor = 0;
        if (std::sscanf(kernel_info.release, "%d.%d", &major, &minor) != 2)
        {
            std::println(stderr, "[DriverLoader] 无法解析内核版本: {}", std::string_view(kernel_info.release));
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
            std::println(stderr, "[DriverLoader] 不支持的内核版本: {}", std::string_view(kernel_info.release));
            std::fflush(stderr);
            return {};
        }

        std::println(stderr, "[DriverLoader] 内核版本 {}.{}, 候选模块数量={}", major, minor, candidates.size());
        std::fflush(stderr);
        return candidates;
    }

    inline bool DownloadModule(const std::string &module_name, std::vector<uint8_t> &module_image)
    {
        const std::string url = "https://raw.githubusercontent.com/lsnbm/Linux-android-arm64/main/lsdriver/byte_stream/" + module_name;
        if (Curl::DownloadHttps(url, module_image)) return true;

        const int download_error = errno;
        std::println(stderr, "[DriverLoader] 下载模块失败: {} errno={} ({})", url, download_error, strerror(download_error));
        std::fflush(stderr);
        errno = download_error;
        return false;
    }

    inline void PrintDmesgTail()
    {
        // READ_ALL 不消费或清空日志；保留全部标签，避免漏掉内核模块加载器的诊断。
        int capacity;
        do
        {
            capacity = klogctl(KLOG_SIZE_BUFFER, nullptr, 0);
        } while (capacity < 0 && errno == EINTR);
        std::string snapshot;
        int count = capacity;
        if (capacity > 0)
        {
            snapshot.resize(static_cast<size_t>(capacity));
            do
            {
                count = klogctl(KLOG_READ_ALL, snapshot.data(), capacity);
            } while (count < 0 && errno == EINTR);
        }
        if (count < 0)
        {
            const int read_error = errno;
            std::println(stderr, "[DriverLoader] 读取 dmesg 失败: errno={} ({})", read_error, strerror(read_error));
            std::fflush(stderr);
            return;
        }
        if (count == 0) return;

        // 从快照末尾寻找最后 80 行；末尾换行不额外算作空行。
        size_t begin = static_cast<size_t>(count);
        int lines = snapshot[begin - 1] == '\n' ? 0 : 1;
        while (begin > 0)
        {
            if (snapshot[--begin] == '\n' && ++lines > 80)
            {
                ++begin;
                break;
            }
        }
        std::string_view remaining(snapshot.data() + begin, static_cast<size_t>(count) - begin);
        while (!remaining.empty())
        {
            const size_t end = remaining.find('\n');
            auto line = remaining.substr(0, end);
            line = line.substr(0, line.find('\r'));
            // 去掉合法的行首 <PRI>，保留时间戳和正文，不按固定长度截断长行。
            if (line.starts_with('<'))
            {
                const size_t close = line.find('>');
                if (close > 1 && close <= 4)
                {
                    unsigned priority = 0;
                    const auto parsed = std::from_chars(line.data() + 1, line.data() + close, priority);
                    if (parsed.ec == std::errc{} && parsed.ptr == line.data() + close && priority <= 191) line.remove_prefix(close + 1);
                }
            }
            std::println(stderr, "[DriverLoader] dmesg: {}", line);
            std::fflush(stderr);
            if (end == std::string_view::npos) break;
            remaining.remove_prefix(end + 1);
        }
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
                std::println(stderr, "[DriverLoader] 模块过大: {}", module_name);
                std::fflush(stderr);
                last_error = -EFBIG;
                continue;
            }

            if (is_vivo_device && !PatchVermagic(module_image, Vivo::VermagicSuffix))
            {
                std::println(stderr, "[DriverLoader] vivo 设备模块 vermagic 修补失败: {}", module_name);
                std::fflush(stderr);
                last_error = -EINVAL;
                continue;
            }

            errno = 0;
            const long result = syscall(SYS_init_module, module_image.data(), static_cast<unsigned long>(module_image.size()), "");
            if (result == 0)
            {
                std::println(stderr, "[DriverLoader] 内核模块加载成功: {}", module_name);
                std::fflush(stderr);
                return 0;
            }
            last_error = -errno;
            std::println(stderr, "[DriverLoader] 内核模块加载失败: {} errno={} ({})", module_name, -last_error, strerror(-last_error));
            std::fflush(stderr);
            PrintDmesgTail();
        }

        return last_error;
    }
} // namespace DriverLoader