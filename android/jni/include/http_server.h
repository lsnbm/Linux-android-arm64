#pragma once
#include <ifaddrs.h>
#include <net/if.h>
#include <nlohmann/json.hpp>
#include <sched.h>
#include <sys/mount.h>
#include "BS_thread_pool.hpp"
#include "memory_tool.h"

// ============================================================================
// HTTP 服务器模块
// ============================================================================

extern "C"
{
    extern const unsigned char cloudflared_blob_start[];
    extern const unsigned char cloudflared_blob_end[];
}

namespace
{
    using nlohmann::json;

    constexpr std::uint16_t kServerPort = 9494;
    constexpr int kListenBacklog = 32;
    constexpr std::size_t kMaxHttpHeaderBytes = 64 * 1024;
    constexpr std::size_t kMaxHttpBodyBytes = 16 * 1024 * 1024;
    constexpr std::string_view kCloudflaredPath = "/data/akernel/.cloudflared";
    constexpr std::string_view kCloudflaredTempPath = "/data/akernel/.cloudflared.tmp";
    std::mutex gRequestMutex;

    // 延迟创建，避免 daemonize() 的 fork() 丢弃线程池工作线程后留下不可用的任务队列。
    BS::thread_pool<> &ClientThreadPool()
    {
        static BS::thread_pool<> pool{std::max(16u, std::thread::hardware_concurrency() * 4)};
        return pool;
    }

    // 打印系统错误信息
    void printErrno(std::string_view action)
    {
        std::println(stderr, "{}，错误码：{}", action, errno);
    }

    bool IsLanInterface(std::string_view interfaceName)
    {
        constexpr std::array<std::string_view, 6> prefixes = {"wlan", "eth", "ap", "rndis", "usb", "bt-pan"};
        return std::ranges::any_of(prefixes, [interfaceName](std::string_view prefix) { return interfaceName.starts_with(prefix); });
    }

    void PrintLanHttpAddresses()
    {
        ifaddrs *interfaces = nullptr;
        if (getifaddrs(&interfaces) != 0)
        {
            printErrno("枚举局域网地址失败");
            return;
        }

        bool found = false;
        std::vector<std::string> printedAddresses;
        for (const ifaddrs *entry = interfaces; entry != nullptr; entry = entry->ifa_next)
        {
            if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET) continue;
            if ((entry->ifa_flags & IFF_UP) == 0 || (entry->ifa_flags & IFF_LOOPBACK) != 0) continue;

            const std::string_view interfaceName = entry->ifa_name != nullptr ? entry->ifa_name : "";
            if (!IsLanInterface(interfaceName)) continue;

            char addressBuffer[INET_ADDRSTRLEN]{};
            const auto *ipv4Address = reinterpret_cast<const sockaddr_in *>(entry->ifa_addr);
            if (inet_ntop(AF_INET, &ipv4Address->sin_addr, addressBuffer, sizeof(addressBuffer)) == nullptr) continue;

            const std::string address = addressBuffer;
            if (std::ranges::find(printedAddresses, address) != printedAddresses.end()) continue;
            printedAddresses.push_back(address);
            found = true;
            std::println("局域网地址 [{}]：http://{}:{}", interfaceName, address, kServerPort);
            std::println("局域网 RPC [{}]：http://{}:{}/api/rpc", interfaceName, address, kServerPort);
        }
        freeifaddrs(interfaces);

        if (!found) std::println("未检测到局域网 IPv4 地址，HTTP 服务仍监听 http://0.0.0.0:{}", kServerPort);
    }

    bool WritePrivateEtcFile(const char *path, std::string_view content)
    {
        const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (fd < 0) return false;

        std::size_t written = 0;
        while (written < content.size())
        {
            const ssize_t result = write(fd, content.data() + written, content.size() - written);
            if (result < 0)
            {
                if (errno == EINTR) continue;
                close(fd);
                return false;
            }
            if (result == 0)
            {
                close(fd);
                return false;
            }
            written += static_cast<std::size_t>(result);
        }

        return close(fd) == 0;
    }

    bool ConfigureCloudflaredDns()
    {
        if (unshare(CLONE_NEWNS) != 0) return false;
        if (mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0) return false;
        if (mount("tmpfs", "/system/etc", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=64k") != 0) return false;

        constexpr std::string_view resolvConf = "nameserver 1.1.1.1\n"
                                                "nameserver 8.8.8.8\n"
                                                "options timeout:2 attempts:2\n";
        constexpr std::string_view hosts = "127.0.0.1 localhost\n"
                                           "::1 ip6-localhost\n";

        if (!WritePrivateEtcFile("/system/etc/resolv.conf", resolvConf)) return false;
        if (!WritePrivateEtcFile("/system/etc/hosts", hosts)) return false;
        return setenv("SSL_CERT_DIR", "/apex/com.android.conscrypt/cacerts", 1) == 0;
    }

    bool ExtractCloudflared()
    {
        const std::size_t blobSize = static_cast<std::size_t>(cloudflared_blob_end - cloudflared_blob_start);
        if (blobSize == 0) return false;

        const int fd = open(kCloudflaredTempPath.data(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0700);
        if (fd < 0)
        {
            printErrno("创建 cloudflared 临时文件失败");
            return false;
        }

        std::size_t written = 0;
        while (written < blobSize)
        {
            const ssize_t result = write(fd, cloudflared_blob_start + written, blobSize - written);
            if (result < 0)
            {
                if (errno == EINTR) continue;
                printErrno("释放 cloudflared 失败");
                close(fd);
                unlink(kCloudflaredTempPath.data());
                return false;
            }
            if (result == 0)
            {
                close(fd);
                unlink(kCloudflaredTempPath.data());
                return false;
            }
            written += static_cast<std::size_t>(result);
        }

        if (fchmod(fd, 0700) != 0)
        {
            printErrno("设置 cloudflared 权限失败");
            close(fd);
            unlink(kCloudflaredTempPath.data());
            return false;
        }
        close(fd);

        if (rename(kCloudflaredTempPath.data(), kCloudflaredPath.data()) != 0)
        {
            printErrno("安装 cloudflared 失败");
            unlink(kCloudflaredTempPath.data());
            return false;
        }
        return true;
    }

    void StartCloudflared()
    {
        if (!ExtractCloudflared()) return;

        const pid_t pid = fork();
        if (pid < 0)
        {
            printErrno("启动 cloudflared 失败");
            unlink(kCloudflaredPath.data());
            return;
        }
        if (pid == 0)
        {
            prctl(PR_SET_PDEATHSIG, SIGTERM);
            if (!ConfigureCloudflaredDns())
            {
                printErrno("配置 cloudflared 私有 DNS 失败");
                _exit(126);
            }
            execl(kCloudflaredPath.data(), kCloudflaredPath.data(), "tunnel", "--no-autoupdate", "--edge-ip-version", "4", "--url", "http://127.0.0.1:9494", static_cast<char *>(nullptr));
            _exit(127);
        }

        std::println("cloudflared Tunnel 已启动，PID={}，公网地址将写入 /sdcard/log.txt", pid);
    }

    // 去除字符串末尾换行符
    void trimLineEnding(std::string &text)
    {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        {
            text.pop_back();
        }
    }

    // 清理文本中的换行字符
    std::string sanitizeLine(std::string text)
    {
        for (char &ch : text)
        {
            if (ch == '\n' || ch == '\r')
            {
                ch = ' ';
            }
        }
        return text;
    }

    // 按空白切分命令参数

    template <typename T, typename ParseFn> std::optional<T> parseNumber(std::string_view text, ParseFn &&parse)
    {
        if (text.empty()) return std::nullopt;

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const auto value = parse(temp.c_str(), &end);
        if (errno != 0 || end == temp.c_str() || *end != '\0') return std::nullopt;
        return static_cast<T>(value);
    }

    // 解析整数参数
    std::optional<int> parseInt(std::string_view text)
    {
        const auto value = parseNumber<std::int64_t>(text, [](const char *s, char **end) { return std::strtoll(s, end, 0); });
        if (!value || *value < std::numeric_limits<int>::min() || *value > std::numeric_limits<int>::max()) return std::nullopt;
        return static_cast<int>(*value);
    }

    // 解析浮点数参数
    std::optional<double> parseDouble(std::string_view text)
    {
        return parseNumber<double>(text, [](const char *s, char **end) { return std::strtod(s, end); });
    }

    std::optional<std::int64_t> parseInt64(std::string_view text)
    {
        return parseNumber<std::int64_t>(text, [](const char *s, char **end) { return std::strtoll(s, end, 0); });
    }

    template <typename T> std::optional<T> readScalarValue(std::uint64_t address)
    {
        T value{};
        if (dr->Read(address, &value, sizeof(T)) != static_cast<int>(sizeof(T))) return std::nullopt;
        return value;
    }

    std::uint64_t readHwbp64(Driver::bp_record &record, int reg)
    {
        return static_cast<std::uint64_t>(MemUtils::HwbpReadRegisterValue(record, reg));
    }

    std::uint32_t readHwbp32(Driver::bp_record &record, int reg)
    {
        return static_cast<std::uint32_t>(MemUtils::HwbpReadRegisterValue(record, reg));
    }

    int clampHwbpRecordCount(int count)
    {
        if (count < 0) return 0;
        if (count > 0x100) return 0x100;
        return count;
    }

    Driver::bp_record *findHwbpRecordByFlatIndex(Driver::break_point &info, int index, int *pointIndex = nullptr, int *pointRecordIndex = nullptr)
    {
        if (index < 0) return nullptr;

        int flatIndex = 0;
        int currentPointIndex = 0;
        for (auto &point : info.points)
        {
            const int recordCount = clampHwbpRecordCount(point.record_count);
            if (index < flatIndex + recordCount)
            {
                const int localIndex = index - flatIndex;
                if (pointIndex) *pointIndex = currentPointIndex;
                if (pointRecordIndex) *pointRecordIndex = localIndex;
                return &point.records[localIndex];
            }

            flatIndex += recordCount;
            ++currentPointIndex;
        }

        return nullptr;
    }

    int getHwbpTotalRecordCount(const Driver::break_point &info)
    {
        int total = 0;
        for (const auto &point : info.points)
        {
            total += clampHwbpRecordCount(point.record_count);
        }
        return total;
    }

    // 将字符串转换为小写ASCII
    std::string toLowerAscii(std::string_view input)
    {
        std::string out;
        out.reserve(input.size());
        for (const unsigned char ch : input)
        {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
        return out;
    }

    // 解析数据类型标记
    std::optional<Types::DataType> parseDataTypeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "i8" || t == "int8") return Types::DataType::I8;
        if (t == "i16" || t == "int16") return Types::DataType::I16;
        if (t == "i32" || t == "int32") return Types::DataType::I32;
        if (t == "i64" || t == "int64") return Types::DataType::I64;
        if (t == "f32" || t == "float") return Types::DataType::Float;
        if (t == "f64" || t == "double") return Types::DataType::Double;
        return std::nullopt;
    }

    // 解析扫描模式标记
    std::optional<Types::FuzzyMode> parseFuzzyModeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "unknown") return Types::FuzzyMode::Unknown;
        if (t == "eq" || t == "equal") return Types::FuzzyMode::Equal;
        if (t == "gt" || t == "greater") return Types::FuzzyMode::Greater;
        if (t == "lt" || t == "less") return Types::FuzzyMode::Less;
        if (t == "inc" || t == "increased") return Types::FuzzyMode::Increased;
        if (t == "dec" || t == "decreased") return Types::FuzzyMode::Decreased;
        if (t == "chg" || t == "changed") return Types::FuzzyMode::Changed;
        if (t == "unchg" || t == "unchanged") return Types::FuzzyMode::Unchanged;
        if (t == "range") return Types::FuzzyMode::Range;
        if (t == "ptr" || t == "pointer") return Types::FuzzyMode::Pointer;
        if (t == "str" || t == "string") return Types::FuzzyMode::String;
        return std::nullopt;
    }

    std::string_view fuzzyModeToken(Types::FuzzyMode mode)
    {
        switch (mode)
        {
        case Types::FuzzyMode::Unknown:
            return "unknown";
        case Types::FuzzyMode::Equal:
            return "equal";
        case Types::FuzzyMode::Greater:
            return "greater";
        case Types::FuzzyMode::Less:
            return "less";
        case Types::FuzzyMode::Increased:
            return "increased";
        case Types::FuzzyMode::Decreased:
            return "decreased";
        case Types::FuzzyMode::Changed:
            return "changed";
        case Types::FuzzyMode::Unchanged:
            return "unchanged";
        case Types::FuzzyMode::Range:
            return "range";
        case Types::FuzzyMode::Pointer:
            return "pointer";
        case Types::FuzzyMode::String:
            return "string";
        default:
            return "";
        }
    }

    // 解析内存浏览显示格式
    std::optional<Types::ViewFormat> parseViewFormatToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "hex") return Types::ViewFormat::Hex;
        if (t == "hexadecimal") return Types::ViewFormat::Hexadecimal;
        if (t == "i8" || t == "int8") return Types::ViewFormat::I8;
        if (t == "i16" || t == "int16") return Types::ViewFormat::I16;
        if (t == "i32" || t == "int32") return Types::ViewFormat::I32;
        if (t == "i64" || t == "int64") return Types::ViewFormat::I64;
        if (t == "f32" || t == "float") return Types::ViewFormat::Float;
        if (t == "f64" || t == "double") return Types::ViewFormat::Double;
        if (t == "disasm") return Types::ViewFormat::Disasm;
        return std::nullopt;
    }

    // 解析硬件断点类型
    std::optional<Driver::bp_type> parseBpTypeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "1" || t == "read" || t == "r" || t == "bp_read") return Driver::BP_BREAKPOINT_R;
        if (t == "2" || t == "write" || t == "w" || t == "bp_write") return Driver::BP_BREAKPOINT_W;
        if (t == "3" || t == "read_write" || t == "rw" || t == "bp_read_write") return Driver::BP_BREAKPOINT_RW;
        if (t == "4" || t == "execute" || t == "x" || t == "exec" || t == "bp_execute") return Driver::BP_BREAKPOINT_X;
        return std::nullopt;
    }

    // 解析硬件断点作用线程范围
    std::optional<Driver::bp_scope> parseBpScopeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "0" || t == "main" || t == "main_thread") return Driver::BP_SCOPE_MAIN_THREAD;
        if (t == "1" || t == "other" || t == "other_threads") return Driver::BP_SCOPE_OTHER_THREADS;
        if (t == "2" || t == "all" || t == "all_threads") return Driver::BP_SCOPE_ALL_THREADS;
        return std::nullopt;
    }

    std::optional<Driver::bp_len> parseBpLengthValue(int length)
    {
        switch (length)
        {
        case 1:
            return Driver::BP_BREAKPOINT_LEN_1;
        case 2:
            return Driver::BP_BREAKPOINT_LEN_2;
        case 3:
            return Driver::BP_BREAKPOINT_LEN_3;
        case 4:
            return Driver::BP_BREAKPOINT_LEN_4;
        case 5:
            return Driver::BP_BREAKPOINT_LEN_5;
        case 6:
            return Driver::BP_BREAKPOINT_LEN_6;
        case 7:
            return Driver::BP_BREAKPOINT_LEN_7;
        case 8:
            return Driver::BP_BREAKPOINT_LEN_8;
        default:
            return std::nullopt;
        }
    }

    // 将显示格式枚举转换为标记
    std::string_view viewFormatToToken(Types::ViewFormat format)
    {
        switch (format)
        {
        case Types::ViewFormat::Hex:
            return "hex";
        case Types::ViewFormat::Hexadecimal:
            return "hexadecimal";
        case Types::ViewFormat::I8:
            return "i8";
        case Types::ViewFormat::I16:
            return "i16";
        case Types::ViewFormat::I32:
            return "i32";
        case Types::ViewFormat::I64:
            return "i64";
        case Types::ViewFormat::Float:
            return "f32";
        case Types::ViewFormat::Double:
            return "f64";
        case Types::ViewFormat::Disasm:
            return "disasm";
        default:
            return "hexadecimal";
        }
    }

    // 将硬件断点类型转换为文本标记
    std::string_view bpTypeToToken(Driver::bp_type type)
    {
        switch (type)
        {
        case Driver::BP_BREAKPOINT_R:
            return "read";
        case Driver::BP_BREAKPOINT_W:
            return "write";
        case Driver::BP_BREAKPOINT_RW:
            return "read_write";
        case Driver::BP_BREAKPOINT_X:
            return "execute";
        default:
            return "unknown";
        }
    }

    // 将硬件断点线程范围转换为文本标记
    std::string_view bpScopeToToken(Driver::bp_scope scope)
    {
        switch (scope)
        {
        case Driver::BP_SCOPE_MAIN_THREAD:
            return "main";
        case Driver::BP_SCOPE_OTHER_THREADS:
            return "other";
        case Driver::BP_SCOPE_ALL_THREADS:
            return "all";
        default:
            return "unknown";
        }
    }

    // 将字节数组编码为十六进制字符串
    std::string bytesToHex(const std::uint8_t *bytes, std::size_t count)
    {
        std::string output;
        output.reserve(count * 2);
        for (std::size_t i = 0; i < count; ++i)
        {
            std::format_to(std::back_inserter(output), "{:02X}", bytes[i]);
        }
        return output;
    }

    // 解析十六进制字节流
    std::optional<std::vector<std::uint8_t>> parseHexBytes(std::string_view text)
    {
        std::string compact;
        compact.reserve(text.size());

        for (char ch : text)
        {
            if (std::isxdigit(static_cast<unsigned char>(ch)) != 0)
            {
                compact.push_back(ch);
            }
            else if (std::isspace(static_cast<unsigned char>(ch)) == 0)
            {
                return std::nullopt;
            }
        }

        if (compact.empty() || (compact.size() % 2) != 0)
        {
            return std::nullopt;
        }

        std::vector<std::uint8_t> bytes;
        bytes.reserve(compact.size() / 2);

        for (std::size_t i = 0; i < compact.size(); i += 2)
        {
            const std::string hexPair = compact.substr(i, 2);
            char *end = nullptr;
            errno = 0;
            const unsigned long value = std::strtoul(hexPair.c_str(), &end, 16);
            if (errno != 0 || end == hexPair.c_str() || *end != '\0' || value > 0xFF)
            {
                return std::nullopt;
            }
            bytes.push_back(static_cast<std::uint8_t>(value));
        }

        return bytes;
    }

    // 合并指定起点后的参数为字符串

    // 生成成功响应文本

    // 生成失败响应文本

    // 构建内存信息JSON响应
    json buildMemoryInfoJson(int status, const auto &info)
    {
        json root;
        int moduleCount = info.module_count;
        if (moduleCount < 0)
        {
            moduleCount = 0;
        }
        else if (moduleCount > MAX_MODULES)
        {
            moduleCount = MAX_MODULES;
        }

        int regionCount = info.region_count;
        if (regionCount < 0)
        {
            regionCount = 0;
        }
        else if (regionCount > MAX_SCAN_REGIONS)
        {
            regionCount = MAX_SCAN_REGIONS;
        }

        root["status"] = status;
        root["module_count"] = moduleCount;
        root["region_count"] = regionCount;
        root["modules"] = json::array();
        root["regions"] = json::array();

        for (int i = 0; i < moduleCount; ++i)
        {
            const auto &mod = info.modules[i];
            int segCount = mod.seg_count;
            if (segCount < 0)
            {
                segCount = 0;
            }
            else if (segCount > MAX_SEGS_PER_MODULE)
            {
                segCount = MAX_SEGS_PER_MODULE;
            }

            json moduleItem;
            moduleItem["name"] = std::string(mod.name);
            moduleItem["seg_count"] = segCount;
            moduleItem["segs"] = json::array();

            for (int j = 0; j < segCount; ++j)
            {
                const auto &seg = mod.segs[j];
                moduleItem["segs"].push_back({
                    {"index", seg.index},
                    {"prot", static_cast<int>(seg.prot)},
                    {"start", seg.start},
                    {"end", seg.end},
                });
            }

            root["modules"].push_back(moduleItem);
        }

        for (int i = 0; i < regionCount; ++i)
        {
            const auto &region = info.regions[i];
            root["regions"].push_back({
                {"start", region.start},
                {"end", region.end},
            });
        }

        return root;
    }

    // 构建内存浏览快照JSON
    json buildViewerSnapshotJson(const MemViewer &viewer)
    {
        json root;
        root["read_success"] = viewer.readSuccess();
        root["base"] = static_cast<std::uint64_t>(viewer.base());
        root["base_hex"] = std::format("0x{:X}", static_cast<std::uint64_t>(viewer.base()));
        const auto format = viewer.format();
        root["format"] = viewFormatToToken(format);

        const auto &buffer = viewer.buffer();
        root["byte_count"] = buffer.size();
        root["data_hex"] = bytesToHex(buffer.data(), buffer.size());

        if (format == Types::ViewFormat::Disasm)
        {
            root["disasm"] = json::array();
            std::size_t emittedLines = 0;
            for (const auto &line : viewer.getDisasm())
            {
                if (emittedLines >= buffer.size() / 4) break;
                json item;
                item["valid"] = line.valid;
                item["address"] = line.address;
                item["address_hex"] = std::format("0x{:X}", line.address);
                item["size"] = line.size;
                item["bytes_hex"] = bytesToHex(line.bytes, line.size);
                item["mnemonic"] = sanitizeLine(line.mnemonic);
                item["op_str"] = sanitizeLine(line.op_str);
                root["disasm"].push_back(std::move(item));
                ++emittedLines;
            }
        }

        return root;
    }

    json buildBreakpointDataJson(const auto &info)
    {
        json data = {{"num_brps", info.num_brps}, {"num_wrps", info.num_wrps}, {"points", json::array()}};

        for (const auto &point : info.points)
        {
            const int pointRecordCount = clampHwbpRecordCount(point.record_count);
            json pointItem = {{"bt", static_cast<int>(point.bt)}, {"bl", static_cast<int>(point.bl)}, {"bs", static_cast<int>(point.bs)}, {"hit_addr", point.hit_addr}, {"record_count", pointRecordCount}, {"records", json::array()}};

            for (int i = 0; i < pointRecordCount; ++i)
            {
                auto &rec = const_cast<Driver::bp_record &>(point.records[i]);
                MemUtils::HwbpRequestAll(rec);
                json item = {{"mask", json::array()}, {"hit_count", readHwbp64(rec, Driver::IDX_HIT_COUNT)}, {"pc", readHwbp64(rec, Driver::IDX_PC)}, {"lr", readHwbp64(rec, Driver::IDX_LR)}, {"sp", readHwbp64(rec, Driver::IDX_SP)}, {"orig_x0", readHwbp64(rec, Driver::IDX_ORIG_X0)}, {"syscallno", readHwbp64(rec, Driver::IDX_SYSCALLNO)}, {"pstate", readHwbp64(rec, Driver::IDX_PSTATE)}, {"fpsr", readHwbp32(rec, Driver::IDX_FPSR)}, {"fpcr", readHwbp32(rec, Driver::IDX_FPCR)}};
                for (int m = 0; m < 18; ++m) item["mask"].push_back(rec.mask[m]);
                for (int reg = 0; reg < 30; ++reg) item[std::format("x{}", reg)] = readHwbp64(rec, Driver::IDX_X0 + reg);
                for (int reg = 0; reg < 32; ++reg)
                {
                    const auto qreg = MemUtils::HwbpReadRegisterValue(rec, Driver::IDX_Q0 + reg);
                    item[std::format("q{}", reg)] = {{"lo", static_cast<std::uint64_t>(qreg)}, {"hi", static_cast<std::uint64_t>(qreg >> 64)}};
                }
                pointItem["records"].push_back(std::move(item));
            }
            data["points"].push_back(std::move(pointItem));
        }
        return data;
    }

    // 构建硬件断点信息JSON
    json buildHwbpInfoJson(const auto &info)
    {
        return {{"mode", MemoryTool::HwbpMode()}, {"bp_info", buildBreakpointDataJson(info)}};
    }

    // 构建特征码扫描结果JSON
    json buildSignatureMatchesJson(const std::vector<uintptr_t> &matches, std::int64_t range, std::string_view pattern)
    {
        constexpr std::size_t kMaxReturnedMatches = 4096;
        const std::size_t returnedCount = std::min(matches.size(), kMaxReturnedMatches);

        json root;
        root["count"] = matches.size();
        root["returned_count"] = returnedCount;
        root["truncated"] = (matches.size() > returnedCount);
        root["range"] = range;
        root["pattern"] = std::string(pattern);
        root["matches"] = json::array();

        for (std::size_t i = 0; i < returnedCount; ++i)
        {
            const auto addr = static_cast<std::uint64_t>(matches[i]);
            root["matches"].push_back({
                {"addr", addr},
                {"addr_hex", std::format("0x{:X}", addr)},
            });
        }

        return root;
    }

    // 发送完整响应数据
    bool sendAll(int fd, std::string_view data)
    {
        std::size_t sentTotal = 0;
        while (sentTotal < data.size())
        {
            const ssize_t sent = send(fd, data.data() + sentTotal, data.size() - sentTotal, MSG_NOSIGNAL);
            if (sent < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                return false;
            }

            if (sent == 0)
            {
                return false;
            }
            sentTotal += static_cast<std::size_t>(sent);
        }
        return true;
    }

    bool sendHttpResponse(int fd, int statusCode, std::string_view statusText, std::string_view body)
    {
        const std::string response = std::format("HTTP/1.1 {} {}\r\n"
                                                 "Content-Type: application/json; charset=utf-8\r\n"
                                                 "Content-Length: {}\r\n"
                                                 "Connection: close\r\n"
                                                 "Access-Control-Allow-Origin: *\r\n"
                                                 "Access-Control-Allow-Headers: Content-Type, Authorization\r\n"
                                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                                 "\r\n{}",
                                                 statusCode, statusText, body.size(), body);
        return sendAll(fd, response);
    }

    bool receiveHttpRequest(int fd, std::string *method, std::string *path, std::string *body, int *errorStatus)
    {
        std::string request;
        request.reserve(4096);
        std::array<char, 4096> chunk{};
        std::size_t headerEnd = std::string::npos;

        while (headerEnd == std::string::npos)
        {
            const ssize_t received = recv(fd, chunk.data(), chunk.size(), 0);
            if (received == 0) return false;
            if (received < 0)
            {
                if (errno == EINTR) continue;
                return false;
            }

            request.append(chunk.data(), static_cast<std::size_t>(received));
            if (request.size() > kMaxHttpHeaderBytes)
            {
                *errorStatus = 431;
                return false;
            }
            headerEnd = request.find("\r\n\r\n");
        }

        const std::size_t requestLineEnd = request.find("\r\n");
        if (requestLineEnd == std::string::npos)
        {
            *errorStatus = 400;
            return false;
        }

        const std::string_view requestLine(request.data(), requestLineEnd);
        const std::size_t firstSpace = requestLine.find(' ');
        const std::size_t secondSpace = firstSpace == std::string_view::npos ? std::string_view::npos : requestLine.find(' ', firstSpace + 1);
        if (firstSpace == std::string_view::npos || secondSpace == std::string_view::npos)
        {
            *errorStatus = 400;
            return false;
        }
        *method = std::string(requestLine.substr(0, firstSpace));
        *path = std::string(requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1));

        std::size_t contentLength = 0;
        std::size_t lineStart = requestLineEnd + 2;
        while (lineStart < headerEnd)
        {
            const std::size_t lineEnd = request.find("\r\n", lineStart);
            if (lineEnd == std::string::npos || lineEnd > headerEnd) break;

            const std::string_view line(request.data() + lineStart, lineEnd - lineStart);
            const std::size_t colon = line.find(':');
            if (colon != std::string_view::npos && toLowerAscii(line.substr(0, colon)) == "content-length")
            {
                std::string_view value = line.substr(colon + 1);
                const std::size_t valueStart = value.find_first_not_of(" \t");
                const std::size_t valueEnd = value.find_last_not_of(" \t");
                if (valueStart == std::string_view::npos)
                {
                    *errorStatus = 400;
                    return false;
                }
                value = value.substr(valueStart, valueEnd - valueStart + 1);
                const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), contentLength);
                if (ec != std::errc{} || ptr != value.data() + value.size())
                {
                    *errorStatus = 400;
                    return false;
                }
            }
            lineStart = lineEnd + 2;
        }

        if (contentLength > kMaxHttpBodyBytes)
        {
            *errorStatus = 413;
            return false;
        }

        const std::size_t bodyStart = headerEnd + 4;
        while (request.size() - bodyStart < contentLength)
        {
            const ssize_t received = recv(fd, chunk.data(), chunk.size(), 0);
            if (received == 0)
            {
                *errorStatus = 400;
                return false;
            }
            if (received < 0)
            {
                if (errno == EINTR) continue;
                return false;
            }
            request.append(chunk.data(), static_cast<std::size_t>(received));
        }

        body->assign(request.data() + bodyStart, contentLength);
        return true;
    }

    // 内部文本命令派发

    // 将文本协议响应包装为统一 JSON 响应。

    json makeProtocolError(std::string_view message)
    {
        return json{
            {"ok", false},
            {"error", std::string(message)},
        };
    }

    std::optional<std::string> getRequiredStringParam(const json &params, std::string_view key)
    {
        const auto it = params.find(std::string(key));
        if (it == params.end())
        {
            return std::nullopt;
        }

        if (it->is_string())
        {
            return it->get<std::string>();
        }

        if (it->is_boolean() || it->is_number_integer() || it->is_number_unsigned() || it->is_number_float())
        {
            return it->dump();
        }

        return std::nullopt;
    }

    std::optional<std::string> getOptionalStringParam(const json &params, std::string_view key)
    {
        const auto it = params.find(std::string(key));
        if (it == params.end() || it->is_null())
        {
            return std::nullopt;
        }
        return getRequiredStringParam(params, key);
    }

    json dispatchStructuredOperationDirect(std::string_view operation, const json &params)
    {
        const std::string op(operation);

        auto finalize = [&](json out) -> json
        {
            out["operation"] = op;
            return out;
        };

        auto fail = [&](std::string_view message) -> json { return finalize(makeProtocolError(message)); };

        auto ok = [&]() -> json { return finalize(json{{"ok", true}}); };

        auto okData = [&](json data) -> json
        {
            json out = ok();
            out["data"] = std::move(data);
            return out;
        };

        using BreakpointPointsResult = std::variant<std::vector<Driver::bp_point>, json>;
        auto parseBreakpointPoints = [&]() -> BreakpointPointsResult
        {
            const auto pointsIt = params.find("points");
            if (pointsIt == params.end() || !pointsIt->is_array() || pointsIt->empty()) return BreakpointPointsResult{std::in_place_index<1>, fail(std::format("operation={} 缺少参数 points", op))};

            auto parseUInt64Value = [](const json &value) -> std::optional<std::uint64_t>
            {
                if (value.is_number_unsigned()) return value.get<std::uint64_t>();
                if (value.is_number_integer())
                {
                    const auto signedValue = value.get<std::int64_t>();
                    return signedValue >= 0 ? std::optional<std::uint64_t>(static_cast<std::uint64_t>(signedValue)) : std::nullopt;
                }
                if (value.is_string()) return MemUtils::ParseUInt64(value.get<std::string>());
                return std::nullopt;
            };
            auto parseIntValue = [](const json &value) -> std::optional<int>
            {
                if (value.is_number_unsigned())
                {
                    const auto raw = value.get<std::uint64_t>();
                    return raw <= static_cast<std::uint64_t>(std::numeric_limits<int>::max()) ? std::optional<int>(static_cast<int>(raw)) : std::nullopt;
                }
                if (value.is_number_integer())
                {
                    const auto raw = value.get<std::int64_t>();
                    return raw >= std::numeric_limits<int>::min() && raw <= std::numeric_limits<int>::max() ? std::optional<int>(static_cast<int>(raw)) : std::nullopt;
                }
                if (value.is_string()) return parseInt(value.get<std::string>());
                return std::nullopt;
            };
            auto parseStringValue = [](const json &value) -> std::optional<std::string>
            {
                if (value.is_string()) return value.get<std::string>();
                if (value.is_number_integer()) return std::format("{}", value.get<std::int64_t>());
                if (value.is_number_unsigned()) return std::format("{}", value.get<std::uint64_t>());
                return std::nullopt;
            };

            std::vector<Driver::bp_point> points;
            if (pointsIt->size() > BP_CONFIG_MAX) return BreakpointPointsResult{std::in_place_index<1>, fail(std::format("points 最多 {} 个", BP_CONFIG_MAX))};
            points.reserve(pointsIt->size());
            for (const auto &pointJson : *pointsIt)
            {
                if (!pointJson.is_object()) return BreakpointPointsResult{std::in_place_index<1>, fail("points 中存在无效断点配置")};
                const auto addressIt = pointJson.find("address");
                const auto typeIt = pointJson.find("bp_type");
                const auto scopeIt = pointJson.find("bp_scope");
                const auto lengthIt = pointJson.find("length");
                if (addressIt == pointJson.end() || typeIt == pointJson.end() || scopeIt == pointJson.end() || lengthIt == pointJson.end()) return BreakpointPointsResult{std::in_place_index<1>, fail("points 中每个断点都需要 address/bp_type/bp_scope/length")};

                const auto address = parseUInt64Value(*addressIt);
                const auto typeToken = parseStringValue(*typeIt);
                const auto scopeToken = parseStringValue(*scopeIt);
                const auto lengthValue = parseIntValue(*lengthIt);
                if (!address || *address == 0 || !typeToken || !scopeToken || !lengthValue) return BreakpointPointsResult{std::in_place_index<1>, fail("points 中存在无效断点参数")};

                const auto bpType = parseBpTypeToken(*typeToken);
                const auto bpScope = parseBpScopeToken(*scopeToken);
                if (*lengthValue < 1 || *lengthValue > 8) return BreakpointPointsResult{std::in_place_index<1>, fail("points 中存在无效断点参数，长度范围为 1-8")};
                const auto bpLength = parseBpLengthValue(*lengthValue);
                if (!bpType || !bpScope || !bpLength) return BreakpointPointsResult{std::in_place_index<1>, fail("points 中存在无效断点参数，长度范围为 1-8")};

                Driver::bp_point point{};
                point.hit_addr = *address;
                point.bt = *bpType;
                point.bl = *bpLength;
                point.bs = *bpScope;
                points.push_back(point);
            }
            if (points.empty()) return BreakpointPointsResult{std::in_place_index<1>, fail("points 为空")};
            return BreakpointPointsResult{std::in_place_index<0>, std::move(points)};
        };

        auto breakpointSetResult = [&](int status, std::string_view mode, const std::vector<Driver::bp_point> &points) -> json
        {
            MemoryTool::HwbpMode() = mode;

            json outPoints = json::array();
            for (std::size_t index = 0; index < points.size(); ++index)
            {
                outPoints.push_back({{"index", index}, {"address", points[index].hit_addr}, {"address_hex", std::format("0x{:X}", points[index].hit_addr)}, {"type", std::string(bpTypeToToken(points[index].bt))}, {"scope", std::string(bpScopeToToken(points[index].bs))}, {"length", static_cast<int>(points[index].bl)}});
            }
            return okData({{"status", status}, {"mode", mode}, {"point_count", points.size()}, {"points", std::move(outPoints)}});
        };

        auto requiredString = [&](std::string_view key, std::string_view desc) -> std::variant<std::string, json>
        {
            const auto value = getRequiredStringParam(params, key);
            if (!value.has_value() || value->empty()) return std::variant<std::string, json>{std::in_place_index<1>, fail(std::format("operation={} 缺少参数 {}", op, desc))};
            return std::variant<std::string, json>{std::in_place_index<0>, *value};
        };

        auto optionalString = [&](std::string_view key) -> std::string
        {
            const auto value = getOptionalStringParam(params, key);
            return value.has_value() ? *value : "";
        };

        auto requiredParsed = [&]<typename T>(std::string_view key, std::string_view desc, auto &&parse) -> std::variant<T, json>
        {
            const auto text = requiredString(key, desc);
            if (std::holds_alternative<json>(text)) return std::variant<T, json>{std::in_place_index<1>, std::get<json>(text)};
            const auto parsed = parse(std::get<std::string>(text));
            if (!parsed.has_value()) return std::variant<T, json>{std::in_place_index<1>, fail(std::format("operation={} 参数 {} 无效", op, desc))};
            return std::variant<T, json>{std::in_place_index<0>, *parsed};
        };

        auto requiredUInt64 = [&](std::string_view key, std::string_view desc) -> std::variant<std::uint64_t, json> { return requiredParsed.template operator()<std::uint64_t>(key, desc, [](std::string_view text) { return MemUtils::ParseUInt64(text); }); };

        auto requiredInt = [&](std::string_view key, std::string_view desc) -> std::variant<int, json> { return requiredParsed.template operator()<int>(key, desc, parseInt); };

        auto requiredInt64 = [&](std::string_view key, std::string_view desc) -> std::variant<std::int64_t, json> { return requiredParsed.template operator()<std::int64_t>(key, desc, parseInt64); };

        auto scannerStateJson = [&]() -> json
        {
            const auto state = MemoryTool::Scanner().state();
            return {
                {"scanning", state.scanning}, {"progress", state.progress}, {"count", state.count}, {"string_scan", state.stringScan}, {"value_type", state.stringScan ? "string" : state.dataType.has_value() ? Types::Labels::TYPE[static_cast<size_t>(*state.dataType)] : ""}, {"mode", fuzzyModeToken(state.mode)}, {"value_format", state.mode == Types::FuzzyMode::Pointer ? "hex_address" : state.mode == Types::FuzzyMode::String ? "text" : "number"},
            };
        };

        auto pointerStateJson = [&]() -> json
        {
            const auto state = MemoryTool::Pointer().state();
            return {
                {"busy", state.operation != PointerManager::Operation::Idle}, {"scanning", state.operation == PointerManager::Operation::Scanning}, {"operation", PointerManager::OperationName(state.operation)}, {"last_operation", PointerManager::OperationName(state.lastOperation)}, {"progress", state.progress}, {"count", state.count}, {"completed", state.completed}, {"success", state.success}, {"error", state.error},
            };
        };

        std::lock_guard<std::mutex> requestLock(gRequestMutex);

        if (op == "bridge.ping") return okData({{"protocol_version", 1}, {"target_operations", true}});

        if (op == "target.find")
        {
            const auto package = requiredString("package_name", "package_name");
            if (std::holds_alternative<json>(package)) return std::get<json>(package);
            const int pid = dr->GetPid(std::get<std::string>(package));
            if (pid <= 0) return fail("未找到进程");
            return okData({{"pid", pid}});
        }

        if (op == "target.select")
        {
            const auto pid = requiredInt("pid", "pid");
            if (std::holds_alternative<json>(pid)) return std::get<json>(pid);
            if (std::get<int>(pid) <= 0) return fail("pid 参数无效");
            if (!MemoryTool::SelectTarget(std::get<int>(pid))) return fail("扫描任务运行中或旧监听停止失败，无法切换 PID");
            return okData({{"pid", dr->GetGlobalPid()}});
        }

        if (op == "target.get") return okData({{"pid", dr->GetGlobalPid()}});

        if (op == "syscall.start" || op == "syscall.stop")
        {
            const int pid = dr->GetGlobalPid();
            if (pid <= 0) return fail("全局PID未设置，请先执行 target.select 或 target.attach");
            const int status = op == "syscall.start" ? MemoryTool::StartSyscallMonitor() : MemoryTool::StopSyscallMonitor();
            if (status != 0) return fail(std::format("系统调用监听请求失败，状态={}", status));
            return okData({{"pid", pid}, {"active", op == "syscall.start"}, {"status", status}});
        }

        if (op == "syscall.read")
        {
            const std::string log = SyscallLog::ReadDmesg();
            return okData({{"log", log}, {"line_count", std::ranges::count(log, '\n')}});
        }

        if (op == "target.attach")
        {
            const auto package = requiredString("package_name", "package_name");
            if (std::holds_alternative<json>(package)) return std::get<json>(package);
            const int pid = dr->GetPid(std::get<std::string>(package));
            if (pid <= 0) return fail("未找到进程");
            if (!MemoryTool::SelectTarget(pid)) return fail("扫描任务运行中或旧监听停止失败，无法切换 PID");
            return okData({{"pid", pid}});
        }

        if (op == "env.read")
        {
            const std::string threadName = optionalString("thread_name");

            const int pid = dr->GetGlobalPid();
            if (pid <= 0) return fail("全局PID未设置，请先执行 target.select 或 target.attach");

            if (!dr->GetEnvParams(threadName)) return fail("获取环境参数失败");
            const auto &info = dr->GetEnvParamsRef();

            return okData({
                {"pid", pid},
                {"thread_name", threadName},
                {"tpidr_el0", info.tpidr_el0},
                {"tpidr_el0_hex", std::format("0x{:X}", info.tpidr_el0)},
                {"pacga_lo", info.pacga_lo},
                {"pacga_hi", info.pacga_hi},
                {"pacga_lo_hex", std::format("0x{:X}", info.pacga_lo)},
                {"pacga_hi_hex", std::format("0x{:X}", info.pacga_hi)},
                {"tls_status", info.tls_status},
                {"pacga_status", info.pacga_status},
            });
        }

        if (op == "memory.map")
        {
            const auto &info = dr->GetMemoryInfoRef();
            return okData(buildMemoryInfoJson(0, info));
        }

        if (op == "module.resolve")
        {
            const auto moduleName = requiredString("module_name", "module_name");
            const auto segmentIndex = requiredInt("segment_index", "segment_index");
            const auto which = requiredString("which", "which");
            if (std::holds_alternative<json>(moduleName)) return std::get<json>(moduleName);
            if (std::holds_alternative<json>(segmentIndex)) return std::get<json>(segmentIndex);
            if (std::holds_alternative<json>(which)) return std::get<json>(which);

            const std::string whichValue = toLowerAscii(std::get<std::string>(which));
            const bool isStart = (whichValue == "start");
            const bool isEnd = (whichValue == "end");
            if (!isStart && !isEnd) return fail("which 必须是 start 或 end");

            std::uint64_t address = 0;
            if (!dr->GetModuleAddress(std::get<std::string>(moduleName), static_cast<short>(std::get<int>(segmentIndex)), &address, isStart)) return fail("未找到目标模块或段");
            return okData({{"address", address}, {"address_hex", std::format("0x{:X}", address)}});
        }

        if (op == "memory.dump")
        {
            const auto target = requiredString("target", "target");
            if (std::holds_alternative<json>(target)) return std::get<json>(target);
            if (dr->GetGlobalPid() <= 0) return fail("全局PID未设置，请先执行 target.select 或 target.attach");
            std::string path;
            if (!dr->DumpMemory(std::get<std::string>(target), &path)) return fail("内存 Dump 失败，请检查模块名或地址范围");
            return okData({{"target", std::get<std::string>(target)}, {"path", path}});
        }

        if (op == "scan.start" || op == "scan.refine")
        {
            const auto mode = requiredString("mode", "mode");
            if (std::holds_alternative<json>(mode)) return std::get<json>(mode);

            const auto fuzzyMode = parseFuzzyModeToken(std::get<std::string>(mode));
            if (!fuzzyMode.has_value()) return fail("mode 无效，支持: unknown/eq/gt/lt/inc/dec/changed/unchanged/range/pointer/string");

            const int pid = dr->GetGlobalPid();
            if (pid <= 0) return fail("全局PID未设置，请先执行 target.select 或 target.attach");

            const bool isFirst = (op == "scan.start");
            const std::string valueToken = optionalString("value");
            if (*fuzzyMode == Types::FuzzyMode::String)
            {
                if (valueToken.empty()) return fail("string 模式需要 value 参数");
                if (!MemoryTool::Scanner().startStringAsync(pid, valueToken, isFirst)) return fail("扫描请求被拒绝，请检查任务状态和结果类型");
                return okData(scannerStateJson());
            }

            const auto type = requiredString("value_type", "value_type");
            if (std::holds_alternative<json>(type)) return std::get<json>(type);
            const auto dataType = parseDataTypeToken(std::get<std::string>(type));
            if (!dataType.has_value()) return fail("value_type 无效，支持: i8/i16/i32/i64/f32/f64");
            if (*fuzzyMode == Types::FuzzyMode::Pointer && *dataType != Types::DataType::I64) return fail("pointer 模式只支持 i64");

            if (*fuzzyMode == Types::FuzzyMode::Pointer)
            {
                if (valueToken.empty()) return fail("pointer 模式需要 value 参数");
                const auto parsed = MemUtils::ParseUInt64(valueToken, 16);
                if (!parsed) return fail("value 参数不是有效的十六进制地址");
                const auto target = static_cast<int64_t>(MemUtils::Normalize(static_cast<uintptr_t>(*parsed)));
                if (!MemoryTool::Scanner().startAsync<int64_t>(pid, target, Types::DataType::I64, *fuzzyMode, isFirst)) return fail("扫描请求被拒绝，请检查任务状态、数据类型和扫描模式");
                return okData(scannerStateJson());
            }

            const bool needValue = *fuzzyMode == Types::FuzzyMode::Equal || *fuzzyMode == Types::FuzzyMode::Greater || *fuzzyMode == Types::FuzzyMode::Less || *fuzzyMode == Types::FuzzyMode::Range;
            if (needValue && valueToken.empty()) return fail("当前模式需要 value 参数");
            const std::string rangeToken = optionalString("range_max");
            if (*fuzzyMode == Types::FuzzyMode::Range && rangeToken.empty()) return fail("range 模式需要 range_max 参数");

            return MemUtils::DispatchType(*dataType,
                                          [&]<typename T>() -> json
                                          {
                                              if (*fuzzyMode == Types::FuzzyMode::Unknown)
                                              {
                                                  if (!MemoryTool::Scanner().startAsync<T>(pid, T{}, *dataType, *fuzzyMode, isFirst)) return fail("扫描请求被拒绝，请检查任务状态、数据类型和扫描模式");
                                                  return okData(scannerStateJson());
                                              }

                                              if (*fuzzyMode == Types::FuzzyMode::Increased || *fuzzyMode == Types::FuzzyMode::Decreased || *fuzzyMode == Types::FuzzyMode::Changed || *fuzzyMode == Types::FuzzyMode::Unchanged)
                                              {
                                                  if (!MemoryTool::Scanner().startAsync<T>(pid, T{}, *dataType, *fuzzyMode, isFirst)) return fail("扫描请求被拒绝，请检查任务状态、数据类型和扫描模式");
                                                  return okData(scannerStateJson());
                                              }

                                              const auto target = MemUtils::ParseScanValue<T>(valueToken);
                                              if (!target) return fail("value 参数超出目标有符号类型范围");

                                              T rangeMax{};
                                              if (*fuzzyMode == Types::FuzzyMode::Range)
                                              {
                                                  const auto parsedRange = MemUtils::ParseScanValue<T>(rangeToken);
                                                  if (!parsedRange) return fail("range_max 参数超出目标有符号类型范围");
                                                  rangeMax = *parsedRange;
                                              }

                                              if (!MemoryTool::Scanner().startAsync<T>(pid, *target, *dataType, *fuzzyMode, isFirst, rangeMax)) return fail("扫描请求被拒绝，请检查任务状态、数据类型和扫描模式");
                                              return okData(scannerStateJson());
                                          });
        }

        if (op == "scan.get") return okData(scannerStateJson());

        if (op == "scan.clear")
        {
            if (MemoryTool::Scanner().isScanning()) return fail("内存扫描运行中，无法清空结果");
            MemoryTool::Scanner().clear();
            return okData(scannerStateJson());
        }

        if (op == "scan.results")
        {
            const auto start = requiredUInt64("start", "start");
            const auto count = requiredUInt64("count", "count");
            const auto type = requiredString("value_type", "value_type");
            if (std::holds_alternative<json>(start)) return std::get<json>(start);
            if (std::holds_alternative<json>(count)) return std::get<json>(count);
            if (std::holds_alternative<json>(type)) return std::get<json>(type);
            if (std::get<std::uint64_t>(count) == 0 || std::get<std::uint64_t>(count) > 200) return fail("count 范围 1-200");

            const std::string typeToken = toLowerAscii(std::get<std::string>(type));
            const bool stringType = (typeToken == "str" || typeToken == "string" || typeToken == "text");
            const auto dataType = parseDataTypeToken(std::get<std::string>(type));
            const auto pageState = MemoryTool::Scanner().pageState(static_cast<size_t>(std::get<std::uint64_t>(start)), static_cast<size_t>(std::get<std::uint64_t>(count)));
            const auto &scannerState = pageState.state;
            if (scannerState.scanning) return fail("内存扫描运行中，请完成后获取结果");
            if (!stringType && !dataType.has_value()) return fail("value_type 参数无效");
            if (scannerState.stringScan != stringType) return fail("value_type 与当前扫描结果类型不一致");
            if (!stringType && (!scannerState.dataType.has_value() || *scannerState.dataType != *dataType)) return fail("value_type 与当前扫描结果类型不一致");
            const bool pointerType = scannerState.mode == Types::FuzzyMode::Pointer;

            json payload;
            payload["start"] = std::get<std::uint64_t>(start);
            payload["request_count"] = std::get<std::uint64_t>(count);
            payload["result_count"] = pageState.results.size();
            payload["total_count"] = scannerState.count;
            payload["type"] = std::get<std::string>(type);
            payload["mode"] = fuzzyModeToken(scannerState.mode);
            payload["value_format"] = pointerType ? "hex_address" : stringType ? "text" : "number";
            payload["items"] = json::array();
            for (const auto addr : pageState.results)
            {
                payload["items"].push_back({
                    {"addr", static_cast<std::uint64_t>(addr)},
                    {"addr_hex", std::format("0x{:X}", static_cast<std::uint64_t>(addr))},
                    {"value", stringType    ? MemUtils::ReadAsText(addr)
                              : pointerType ? MemUtils::ReadAsPointerString(addr)
                                            : MemUtils::ReadAsString(addr, *dataType)},
                });
            }
            return okData(std::move(payload));
        }

        if (op == "viewer.open")
        {
            const auto address = requiredUInt64("address", "address");
            if (std::holds_alternative<json>(address)) return std::get<json>(address);
            const std::string viewFormat = optionalString("view_format");
            std::optional<Types::ViewFormat> format;
            if (!viewFormat.empty())
            {
                format = parseViewFormatToken(viewFormat);
                if (!format.has_value()) return fail("view_format 无效，支持: hexadecimal/hex/i8/i16/i32/i64/f32/f64/disasm");
            }
            MemoryTool::Viewer().open(static_cast<uintptr_t>(std::get<std::uint64_t>(address)), format);
            if (MemoryTool::Viewer().format() == Types::ViewFormat::Disasm) MemoryTool::Viewer().waitDisasm();
            return okData(buildViewerSnapshotJson(MemoryTool::Viewer()));
        }

        if (op == "viewer.seek")
        {
            const auto offset = requiredString("offset", "offset");
            if (std::holds_alternative<json>(offset)) return std::get<json>(offset);
            if (!MemoryTool::Viewer().applyOffset(std::get<std::string>(offset))) return fail("offset 参数无效");
            if (MemoryTool::Viewer().format() == Types::ViewFormat::Disasm) MemoryTool::Viewer().waitDisasm();
            return okData(buildViewerSnapshotJson(MemoryTool::Viewer()));
        }

        if (op == "viewer.format")
        {
            const auto viewFormat = requiredString("view_format", "view_format");
            if (std::holds_alternative<json>(viewFormat)) return std::get<json>(viewFormat);
            const auto format = parseViewFormatToken(std::get<std::string>(viewFormat));
            if (!format.has_value()) return fail("view_format 无效，支持: hexadecimal/hex/i8/i16/i32/i64/f32/f64/disasm");
            MemoryTool::Viewer().setFormat(*format);
            if (MemoryTool::Viewer().format() == Types::ViewFormat::Disasm) MemoryTool::Viewer().waitDisasm();
            return okData(buildViewerSnapshotJson(MemoryTool::Viewer()));
        }

        if (op == "viewer.refresh")
        {
            MemoryTool::Viewer().refresh();
            if (MemoryTool::Viewer().format() == Types::ViewFormat::Disasm) MemoryTool::Viewer().waitDisasm();
            return okData(buildViewerSnapshotJson(MemoryTool::Viewer()));
        }

        if (op == "pointer.get") return okData(pointerStateJson());

        if (op == "pointer.scan")
        {
            const std::string modeToken = optionalString("mode");
            const std::string mode = toLowerAscii(modeToken.empty() ? "module" : modeToken);
            const auto target = requiredUInt64("target", "target");
            const auto depth = requiredInt("depth", "depth");
            const auto maxOffset = requiredInt("max_offset", "max_offset");
            if (std::holds_alternative<json>(target)) return std::get<json>(target);
            if (std::holds_alternative<json>(depth)) return std::get<json>(depth);
            if (std::holds_alternative<json>(maxOffset)) return std::get<json>(maxOffset);
            if (std::get<int>(depth) <= 0 || std::get<int>(depth) > 16) return fail("depth 范围为 1-16");
            if (std::get<int>(maxOffset) <= 0) return fail("max_offset 必须大于 0");

            const bool useManual = (mode == "manual");
            const bool useArray = (mode == "array");
            std::uint64_t manualBase = 0;
            std::uint64_t arrayBase = 0;
            std::size_t arrayCount = 0;
            if (useManual)
            {
                const auto manual = requiredUInt64("manual_base", "manual_base");
                if (std::holds_alternative<json>(manual)) return std::get<json>(manual);
                manualBase = std::get<std::uint64_t>(manual);
            }
            else if (useArray)
            {
                const auto base = requiredUInt64("array_base", "array_base");
                const auto count = requiredUInt64("array_count", "array_count");
                if (std::holds_alternative<json>(base)) return std::get<json>(base);
                if (std::holds_alternative<json>(count)) return std::get<json>(count);
                if (std::get<std::uint64_t>(count) == 0 || std::get<std::uint64_t>(count) > 1000000) return fail("array_count 范围为 1-1000000");
                arrayBase = std::get<std::uint64_t>(base);
                arrayCount = static_cast<std::size_t>(std::get<std::uint64_t>(count));
            }
            else if (mode != "module")
            {
                return fail("mode 仅支持 module/manual/array");
            }

            const int pid = dr->GetGlobalPid();
            if (pid <= 0) return fail("全局PID未设置，请先执行 target.select 或 target.attach");
            if (MemoryTool::Pointer().isBusy()) return fail("当前已有指针操作在运行");

            const std::string moduleFilter = optionalString("module_filter");
            if (!MemoryTool::Pointer().startAsync(pid, static_cast<uintptr_t>(std::get<std::uint64_t>(target)), std::get<int>(depth), std::get<int>(maxOffset), useManual, static_cast<uintptr_t>(manualBase), useArray, static_cast<uintptr_t>(arrayBase), arrayCount, moduleFilter)) return fail("指针扫描请求被拒绝，请检查目标、参数和当前操作状态");
            return okData(pointerStateJson());
        }

        if (op == "pointer.merge")
        {
            if (!MemoryTool::Pointer().MergeBins()) return fail("当前已有指针操作在运行，无法合并结果");
            return okData(pointerStateJson());
        }

        if (op == "pointer.export")
        {
            if (!MemoryTool::Pointer().ExportToTxt()) return fail("当前已有指针操作在运行，无法导出结果");
            return okData(pointerStateJson());
        }

        if (op == "breakpoint.get")
        {
            const auto &info = dr->GetHwbpInfoRef();
            return okData(buildHwbpInfoJson(info));
        }

        if (op == "breakpoint.set")
        {
            const auto mode = requiredString("mode", "mode");
            if (std::holds_alternative<json>(mode)) return std::get<json>(mode);
            const std::string modeName = toLowerAscii(std::get<std::string>(mode));
            if (modeName != "hwbp" && modeName != "ptebp" && modeName != "stepbp") return fail("mode 无效，支持: hwbp/ptebp/stepbp");
            if (!MemoryTool::HwbpMode().empty()) return fail(std::format("{} 设置前请先移除当前断点", modeName));

            const int pid = dr->GetGlobalPid();
            if (pid <= 0) return fail("全局PID未设置，请先执行 target.select 或 target.attach");

            auto parsedPoints = parseBreakpointPoints();
            if (std::holds_alternative<json>(parsedPoints)) return std::get<json>(parsedPoints);
            auto points = std::move(std::get<std::vector<Driver::bp_point>>(parsedPoints));

            const auto pointSpan = std::span<const Driver::bp_point>(points.data(), points.size());
            const int status = modeName == "hwbp" ? dr->SetProcessHwbpRef(pointSpan) : (modeName == "stepbp" ? dr->SetProcessStepbpRef(pointSpan) : dr->SetProcessPtebpRef(pointSpan));
            if (status != 0) return fail(std::format("设置 {} 失败 status={}", modeName, status));
            return breakpointSetResult(status, modeName, points);
        }

        if (op == "breakpoint.clear")
        {
            auto &activeMode = MemoryTool::HwbpMode();
            if (activeMode.empty()) return okData({{"mode", ""}, {"cleared", false}});

            if (activeMode == "hwbp") dr->RemoveProcessHwbpRef();
            else if (activeMode == "stepbp") dr->RemoveProcessStepbpRef();
            else dr->RemoveProcessPtebpRef();
            activeMode.clear();
            return okData({{"mode", ""}, {"cleared", true}});
        }

        if (op == "breakpoint_record.remove")
        {
            const auto index = requiredInt("index", "index");
            if (std::holds_alternative<json>(index)) return std::get<json>(index);
            if (std::get<int>(index) < 0) return fail("index 无效");
            const auto &info = dr->GetHwbpInfoRef();
            if (std::get<int>(index) >= getHwbpTotalRecordCount(info)) return fail("index 越界");
            dr->RemoveHwbpRecord(std::get<int>(index));
            return okData({{"record_count", getHwbpTotalRecordCount(info)}});
        }

        if (op == "breakpoint_record.update")
        {
            const auto index = requiredInt("index", "index");
            const auto field = requiredString("field", "field");
            const auto valueText = requiredString("value", "value");
            if (std::holds_alternative<json>(index)) return std::get<json>(index);
            if (std::holds_alternative<json>(field)) return std::get<json>(field);
            if (std::holds_alternative<json>(valueText)) return std::get<json>(valueText);
            const auto value = MemUtils::ParseUInt128(std::get<std::string>(valueText));
            if (!value.has_value()) return fail(std::format("operation={} 参数 value 无效", op));
            auto &info = const_cast<Driver::break_point &>(dr->GetHwbpInfoRef());
            int pointIndex = -1;
            int pointRecordIndex = -1;
            auto *record = findHwbpRecordByFlatIndex(info, std::get<int>(index), &pointIndex, &pointRecordIndex);
            if (!record) return fail("index 越界");
            auto copy = *record;
            if (!MemUtils::AssignHwbpRecordField(copy, std::get<std::string>(field), *value)) return fail("field 无效");
            *record = copy;
            return okData({{"index", std::get<int>(index)}, {"point_index", pointIndex}, {"point_record_index", pointRecordIndex}, {"field", std::get<std::string>(field)}, {"value_hex", MemUtils::FormatUInt128Hex(*value)}});
        }

        if (op == "signature.create")
        {
            const auto address = requiredUInt64("address", "address");
            const auto range = requiredInt("range", "range");
            if (std::holds_alternative<json>(address)) return std::get<json>(address);
            if (std::holds_alternative<json>(range)) return std::get<json>(range);
            const std::string requestedFile = optionalString("file_name");
            const std::string fileName = requestedFile.empty() ? std::string(SignatureScanner::SIG_DEFAULT_FILE) : requestedFile;
            if (!SignatureScanner::ScanAddressSignature(static_cast<uintptr_t>(std::get<std::uint64_t>(address)), std::get<int>(range), fileName.c_str())) return fail("特征码保存失败");
            return okData({{"saved", true}, {"file", fileName}});
        }

        if (op == "signature.scan")
        {
            const std::string requestedFile = optionalString("file_name");
            const std::string fileName = requestedFile.empty() ? std::string(SignatureScanner::SIG_DEFAULT_FILE) : requestedFile;
            json payload = buildSignatureMatchesJson(SignatureScanner::ScanSignatureFromFile(fileName.c_str()), 0, "");
            payload["file"] = fileName;
            return okData(std::move(payload));
        }

        if (op == "signature.match")
        {
            const auto rangeOffset = requiredInt64("range_offset", "range_offset");
            const auto pattern = requiredString("pattern", "pattern");
            if (std::holds_alternative<json>(rangeOffset)) return std::get<json>(rangeOffset);
            if (std::holds_alternative<json>(pattern)) return std::get<json>(pattern);
            if (std::get<std::int64_t>(rangeOffset) < static_cast<std::int64_t>(std::numeric_limits<int>::min()) || std::get<std::int64_t>(rangeOffset) > static_cast<std::int64_t>(std::numeric_limits<int>::max())) return fail("range_offset 无效");
            const auto matches = SignatureScanner::ScanSignature(std::get<std::string>(pattern).c_str(), static_cast<int>(std::get<std::int64_t>(rangeOffset)));
            return okData(buildSignatureMatchesJson(matches, std::get<std::int64_t>(rangeOffset), std::get<std::string>(pattern)));
        }

        if (op == "signature.filter")
        {
            const auto address = requiredUInt64("address", "address");
            if (std::holds_alternative<json>(address)) return std::get<json>(address);
            const std::string requestedFile = optionalString("file_name");
            const std::string fileName = requestedFile.empty() ? std::string(SignatureScanner::SIG_DEFAULT_FILE) : requestedFile;
            const auto result = SignatureScanner::FilterSignature(static_cast<uintptr_t>(std::get<std::uint64_t>(address)), fileName.c_str());
            return okData({{"success", result.success}, {"changed_count", result.changedCount}, {"total_count", result.totalCount}, {"old_signature", result.oldSignature}, {"new_signature", result.newSignature}, {"file", fileName}});
        }

        if (op == "lock.set")
        {
            const auto address = requiredUInt64("address", "address");
            const auto valueType = requiredString("value_type", "value_type");
            const auto value = requiredString("value", "value");
            if (std::holds_alternative<json>(address)) return std::get<json>(address);
            if (std::holds_alternative<json>(valueType)) return std::get<json>(valueType);
            if (std::holds_alternative<json>(value)) return std::get<json>(value);
            const auto dataType = parseDataTypeToken(std::get<std::string>(valueType));
            if (!dataType.has_value()) return fail("value_type 无效");
            MemoryTool::Locks().lock(static_cast<uintptr_t>(std::get<std::uint64_t>(address)), *dataType, std::get<std::string>(value));
            return okData({{"locked", MemoryTool::Locks().isLocked(static_cast<uintptr_t>(std::get<std::uint64_t>(address)))}});
        }

        if (op == "lock.remove")
        {
            const auto address = requiredUInt64("address", "address");
            if (std::holds_alternative<json>(address)) return std::get<json>(address);
            MemoryTool::Locks().unlock(static_cast<uintptr_t>(std::get<std::uint64_t>(address)));
            return okData({{"locked", MemoryTool::Locks().isLocked(static_cast<uintptr_t>(std::get<std::uint64_t>(address)))}});
        }

        if (op == "memory.read")
        {
            const auto address = requiredUInt64("address", "address");
            const auto size = requiredInt("size", "size");
            if (std::holds_alternative<json>(address)) return std::get<json>(address);
            if (std::holds_alternative<json>(size)) return std::get<json>(size);
            constexpr int MAX_TRANSFER_SIZE = 1024 * 1024;
            const int byteCount = std::get<int>(size);
            if (byteCount <= 0 || byteCount > MAX_TRANSFER_SIZE) return fail("size 范围 1-1048576");
            std::vector<std::uint8_t> bytes(static_cast<size_t>(byteCount));
            const int readBytes = dr->Read(std::get<std::uint64_t>(address), bytes.data(), bytes.size());
            if (readBytes != byteCount) return fail(std::format("读取失败 status={}", readBytes));
            return okData({
                {"address", std::get<std::uint64_t>(address)},
                {"address_hex", std::format("0x{:X}", std::get<std::uint64_t>(address))},
                {"size", byteCount},
                {"data_hex", bytesToHex(bytes.data(), bytes.size())},
            });
        }

        if (op == "memory.write")
        {
            const auto address = requiredUInt64("address", "address");
            const auto dataHex = requiredString("data_hex", "data_hex");
            if (std::holds_alternative<json>(address)) return std::get<json>(address);
            if (std::holds_alternative<json>(dataHex)) return std::get<json>(dataHex);
            auto bytes = parseHexBytes(std::get<std::string>(dataHex));
            if (!bytes.has_value() || bytes->empty()) return fail("data_hex 无效");
            constexpr size_t MAX_TRANSFER_SIZE = 1024 * 1024;
            if (bytes->size() > MAX_TRANSFER_SIZE) return fail("data_hex 最大 1048576 字节");
            const int writeBytes = dr->Write(std::get<std::uint64_t>(address), bytes->data(), bytes->size());
            if (writeBytes != static_cast<int>(bytes->size())) return fail(std::format("写入失败 status={}", writeBytes));
            std::vector<std::uint8_t> readback(bytes->size());
            const int readBytes = dr->Read(std::get<std::uint64_t>(address), readback.data(), readback.size());
            const bool readbackComplete = readBytes == static_cast<int>(readback.size());
            return okData({
                {"address", std::get<std::uint64_t>(address)},
                {"address_hex", std::format("0x{:X}", std::get<std::uint64_t>(address))},
                {"size", bytes->size()},
                {"verified", readbackComplete && readback == *bytes},
                {"readback_status", readBytes},
                {"readback_hex", readbackComplete ? bytesToHex(readback.data(), readback.size()) : ""},
            });
        }

        return fail(std::format("未知 operation: {}", op));
    }

    // 统一命令派发入口：网络层仅接受 JSON 请求并返回 JSON 响应。
    std::string DispatchCommandUnified(const std::string &request)
    {
        const auto parsedReq = json::parse(request, nullptr, false);
        if (parsedReq.is_discarded())
        {
            return json({{"ok", false}, {"error", "请求必须是 JSON 字符串对象"}}).dump();
        }

        if (!parsedReq.is_object())
        {
            return json({{"ok", false}, {"error", "请求必须是 JSON 对象"}}).dump();
        }

        if (parsedReq.contains("operation"))
        {
            if (!parsedReq["operation"].is_string())
            {
                return makeProtocolError("operation 字段必须是字符串").dump();
            }

            const std::string operationName = parsedReq["operation"].get<std::string>();
            json params = json::object();
            if (parsedReq.contains("params"))
            {
                if (!parsedReq["params"].is_object())
                {
                    json error = makeProtocolError("params 字段必须是对象");
                    error["operation"] = operationName;
                    return error.dump();
                }
                params = parsedReq["params"];
            }

            return dispatchStructuredOperationDirect(operationName, params).dump();
        }

        return makeProtocolError("请求缺少 operation 字段").dump();
    }

    void HandleClientConnection(int clientFd, sockaddr_in clientAddr)
    {
        char clientIp[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp)) == nullptr)
        {
            std::strncpy(clientIp, "未知地址", sizeof(clientIp) - 1);
            clientIp[sizeof(clientIp) - 1] = '\0';
        }

        const auto clientPort = ntohs(clientAddr.sin_port);
        std::string method;
        std::string path;
        std::string body;
        int errorStatus = 0;
        if (!receiveHttpRequest(clientFd, &method, &path, &body, &errorStatus))
        {
            if (errorStatus == 413) sendHttpResponse(clientFd, 413, "Payload Too Large", R"({"ok":false,"error":"请求体过大"})");
            else if (errorStatus == 431) sendHttpResponse(clientFd, 431, "Request Header Fields Too Large", R"({"ok":false,"error":"请求头过大"})");
            else if (errorStatus != 0) sendHttpResponse(clientFd, 400, "Bad Request", R"({"ok":false,"error":"HTTP 请求格式无效"})");
            close(clientFd);
            return;
        }

        std::println("收到 HTTP 请求：{}:{} {} {}", clientIp, clientPort, method, path);
        if (method == "OPTIONS")
        {
            sendHttpResponse(clientFd, 204, "No Content", "");
        }
        else if (method == "GET" && path == "/health")
        {
            sendHttpResponse(clientFd, 200, "OK", R"({"ok":true,"service":"LS_KTool","transport":"http"})");
        }
        else if (method == "POST" && path == "/api/rpc")
        {
            const std::string response = DispatchCommandUnified(body);
            if (!sendHttpResponse(clientFd, 200, "OK", response)) printErrno("发送 HTTP 回复失败");
        }
        else if (method != "GET" && method != "POST")
        {
            sendHttpResponse(clientFd, 405, "Method Not Allowed", R"({"ok":false,"error":"不支持的 HTTP 方法"})");
        }
        else
        {
            sendHttpResponse(clientFd, 404, "Not Found", R"({"ok":false,"error":"接口不存在"})");
        }
        close(clientFd);
    }
} // namespace

// 程序入口：初始化 HTTP 服务并处理客户端请求。
int http_server()
{
    const int serverFd = socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0);
    if (serverFd < 0)
    {
        printErrno("创建套接字失败");
        return 1;
    }
    constexpr int enableReuse = 1;
    if (setsockopt(serverFd, SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(enableReuse)) < 0)
    {
        printErrno("设置套接字选项失败");
        close(serverFd);
        return 1;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(kServerPort);

    if (bind(serverFd, reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
    {
        printErrno("绑定端口失败");
        close(serverFd);
        return 1;
    }

    if (listen(serverFd, kListenBacklog) < 0)
    {
        printErrno("开始监听失败");
        close(serverFd);
        return 1;
    }

    std::println("HTTP 服务端已监听 http://0.0.0.0:{}", kServerPort);
    PrintLanHttpAddresses();
    std::fflush(stdout);
    StartCloudflared();

    for (;;)
    {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        const int clientFd = accept(serverFd, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
        if (clientFd < 0)
        {
            if (errno == EINTR) continue;
            printErrno("接受连接失败");
            continue;
        }

        ClientThreadPool().detach_task([clientFd, clientAddr] { HandleClientConnection(clientFd, clientAddr); });
    }
}
