#include <arpa/inet.h>
#include <csignal>
#include <cstdint>
#include <cctype>
#include <cstdlib>
#include <cstring>
#include <errno.h>
#include <print>
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <sstream>
#include <format>
#include <iterator>

#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <atomic>

#include "DriverMemory.h"
#include "json.hpp"

namespace
{
    using nlohmann::json;

    constexpr std::uint16_t kServerPort = 9494;
    constexpr int kListenBacklog = 4;
    std::atomic_bool gRunning{true};
    int gServerFd = -1;

    void printErrno(std::string_view action)
    {
        std::println(stderr, "{}，错误码：{}", action, errno);
    }

    void trimLineEnding(std::string &text)
    {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        {
            text.pop_back();
        }
    }

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

    std::vector<std::string> splitTokens(const std::string &input)
    {
        std::istringstream iss(input);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }

    std::optional<std::uint64_t> parseUInt64(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const unsigned long long value = std::strtoull(temp.c_str(), &end, 0);
        if (errno != 0 || end == temp.c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return static_cast<std::uint64_t>(value);
    }

    std::optional<int> parseInt(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const long value = std::strtol(temp.c_str(), &end, 0);
        if (errno != 0 || end == temp.c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return static_cast<int>(value);
    }

    std::optional<double> parseDouble(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const double value = std::strtod(temp.c_str(), &end);
        if (errno != 0 || end == temp.c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return value;
    }

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

    std::string joinTokens(const std::vector<std::string> &tokens, std::size_t start)
    {
        if (start >= tokens.size())
        {
            return "";
        }

        std::string text = tokens[start];
        for (std::size_t i = start + 1; i < tokens.size(); ++i)
        {
            text.append(" ");
            text.append(tokens[i]);
        }
        return text;
    }

    std::string ok(std::string_view message)
    {
        return std::format("ok {}", message);
    }

    std::string err(std::string_view message)
    {
        return std::format("err {}", message);
    }

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

    bool sendAll(int fd, std::string_view data)
    {
        std::size_t sentTotal = 0;
        while (sentTotal < data.size())
        {
            const ssize_t sent = send(fd, data.data() + sentTotal, data.size() - sentTotal, 0);
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

    std::string executeCommand(const std::string &request)
    {
        const auto tokens = splitTokens(request);
        if (tokens.empty())
        {
            return ok("收到");
        }

        const std::string &command = tokens[0];

        if (command == "help")
        {
            return ok("支持命令: ping, pid.get, pid.set, pid.current, pid.attach, mem.read, mem.write, mem.read_u8/u16/u32/u64/f32/f64, mem.write_u8/u16/u32/u64/f32/f64, mem.read_str, mem.read_wstr, memory.refresh, memory.summary, memory.info.full, module.addr, touch.down, touch.move, touch.up");
        }

        if (command == "ping")
        {
            return ok("pong");
        }

        if (command == "pid.get")
        {
            if (tokens.size() < 2)
            {
                return err("用法: pid.get <包名>");
            }

            const std::string packageName = joinTokens(tokens, 1);
            const int pid = dr.GetPid(packageName);
            if (pid <= 0)
            {
                return err("未找到进程");
            }
            return ok(std::format("pid={}", pid));
        }

        if (command == "pid.set")
        {
            if (tokens.size() != 2)
            {
                return err("用法: pid.set <pid>");
            }

            const auto pid = parseInt(tokens[1]);
            if (!pid.has_value() || *pid <= 0)
            {
                return err("pid 参数无效");
            }

            dr.SetGlobalPid(*pid);
            return ok(std::format("pid={}", dr.GetGlobalPid()));
        }

        if (command == "pid.current")
        {
            return ok(std::format("pid={}", dr.GetGlobalPid()));
        }

        if (command == "pid.attach")
        {
            if (tokens.size() < 2)
            {
                return err("用法: pid.attach <包名>");
            }

            const std::string packageName = joinTokens(tokens, 1);
            const int pid = dr.GetPid(packageName);
            if (pid <= 0)
            {
                return err("未找到进程");
            }

            dr.SetGlobalPid(pid);
            return ok(std::format("pid={}", pid));
        }

        if (command == "memory.refresh")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }
            return ok(std::format("status={}", status));
        }

        if (command == "memory.summary")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }

            const auto &info = dr.GetMemoryInfoRef();
            return ok(std::format("status={} modules={} regions={}", status, info.module_count, info.region_count));
        }

        if (command == "module.list")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }

            const auto &info = dr.GetMemoryInfoRef();
            std::string payload = std::format("status={} count={}", status, info.module_count);
            for (int i = 0; i < info.module_count; ++i)
            {
                const auto &mod = info.modules[i];
                if (mod.name[0] == '\0')
                {
                    continue;
                }

                payload.append(std::format(";{}#{}", sanitizeLine(mod.name), mod.seg_count));
            }
            return ok(payload);
        }

        if (command == "memory.info.full")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }

            const auto &info = dr.GetMemoryInfoRef();
            const std::string jsonText = buildMemoryInfoJson(status, info).dump();
            return std::format("ok memory.info.full size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "module.addr")
        {
            if (tokens.size() != 4)
            {
                return err("用法: module.addr <模块名> <段索引> <start|end>");
            }

            const std::string moduleName = tokens[1];
            const auto segmentIndex = parseInt(tokens[2]);
            if (!segmentIndex.has_value())
            {
                return err("段索引无效");
            }

            const bool isStart = (tokens[3] == "start");
            const bool isEnd = (tokens[3] == "end");
            if (!isStart && !isEnd)
            {
                return err("第三个参数必须是 start 或 end");
            }

            std::uint64_t address = 0;
            const bool found = dr.GetModuleAddress(moduleName, static_cast<short>(*segmentIndex), &address, isStart);
            if (!found)
            {
                return err("未找到目标模块或段");
            }

            return ok(std::format("address=0x{:X}", address));
        }

        if (command == "mem.read")
        {
            if (tokens.size() != 3)
            {
                return err("用法: mem.read <地址> <大小>");
            }

            const auto address = parseUInt64(tokens[1]);
            const auto size = parseUInt64(tokens[2]);
            if (!address.has_value() || !size.has_value() || *size == 0 || *size > 4096)
            {
                return err("地址或大小无效，大小范围 1-4096");
            }

            std::vector<std::uint8_t> buffer(static_cast<std::size_t>(*size));
            const int status = dr.Read(*address, buffer.data(), buffer.size());
            if (status <= 0)
            {
                return err(std::format("读取失败 status={}", status));
            }

            return ok(std::format("hex={}", bytesToHex(buffer.data(), buffer.size())));
        }

        if (command == "mem.write")
        {
            if (tokens.size() < 3)
            {
                return err("用法: mem.write <地址> <HEX字节流>");
            }

            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
            {
                return err("地址无效");
            }

            const std::string hexText = joinTokens(tokens, 2);
            auto bytes = parseHexBytes(hexText);
            if (!bytes.has_value() || bytes->empty())
            {
                return err("HEX 字节流无效");
            }

            const bool success = dr.Write(*address, bytes->data(), bytes->size());
            if (!success)
            {
                return err("写入失败");
            }

            return ok(std::format("size={}", bytes->size()));
        }

        if (command == "mem.read_u8")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u8 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint8_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_u16")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u16 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint16_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_u32")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u32 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint32_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_u64")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u64 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint64_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_f32")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_f32 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<float>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_f64")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_f64 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<double>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.write_u8")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u8 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value() || *value > 0xFF)
                return err("参数无效");
            if (!dr.Write<std::uint8_t>(*address, static_cast<std::uint8_t>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_u16")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u16 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value() || *value > 0xFFFF)
                return err("参数无效");
            if (!dr.Write<std::uint16_t>(*address, static_cast<std::uint16_t>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_u32")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u32 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value() || *value > 0xFFFFFFFFULL)
                return err("参数无效");
            if (!dr.Write<std::uint32_t>(*address, static_cast<std::uint32_t>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_u64")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u64 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value())
                return err("参数无效");
            if (!dr.Write<std::uint64_t>(*address, *value))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_f32")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_f32 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseDouble(tokens[2]);
            if (!address.has_value() || !value.has_value())
                return err("参数无效");
            if (!dr.Write<float>(*address, static_cast<float>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_f64")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_f64 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseDouble(tokens[2]);
            if (!address.has_value() || !value.has_value())
                return err("参数无效");
            if (!dr.Write<double>(*address, *value))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.read_str")
        {
            if (tokens.size() < 2 || tokens.size() > 3)
            {
                return err("用法: mem.read_str <地址> [最大长度]");
            }

            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
            {
                return err("地址无效");
            }

            std::size_t maxLength = 128;
            if (tokens.size() == 3)
            {
                const auto value = parseUInt64(tokens[2]);
                if (!value.has_value() || *value == 0 || *value > 4096)
                {
                    return err("最大长度范围 1-4096");
                }
                maxLength = static_cast<std::size_t>(*value);
            }

            const std::string value = sanitizeLine(dr.ReadString(*address, maxLength));
            return ok(std::format("text={}", value));
        }

        if (command == "mem.read_wstr")
        {
            if (tokens.size() != 3)
            {
                return err("用法: mem.read_wstr <地址> <长度>");
            }

            const auto address = parseUInt64(tokens[1]);
            const auto length = parseUInt64(tokens[2]);
            if (!address.has_value() || !length.has_value() || *length == 0 || *length > 1024)
            {
                return err("地址或长度无效，长度范围 1-1024");
            }

            const std::string value = sanitizeLine(dr.ReadWString(*address, static_cast<std::size_t>(*length)));
            return ok(std::format("text={}", value));
        }

        if (command == "touch.down" || command == "touch.move")
        {
            if (tokens.size() != 5)
            {
                return err("用法: touch.down/touch.move <x> <y> <屏宽> <屏高>");
            }

            const auto x = parseInt(tokens[1]);
            const auto y = parseInt(tokens[2]);
            const auto screenW = parseInt(tokens[3]);
            const auto screenH = parseInt(tokens[4]);
            if (!x.has_value() || !y.has_value() || !screenW.has_value() || !screenH.has_value())
            {
                return err("坐标参数无效");
            }

            if (command == "touch.down")
            {
                dr.TouchDown(*x, *y, *screenW, *screenH);
                return ok("touch.down 已发送");
            }

            dr.TouchMove(*x, *y, *screenW, *screenH);
            return ok("touch.move 已发送");
        }

        if (command == "touch.up")
        {
            dr.TouchUp();
            return ok("touch.up 已发送");
        }

        return err("未知命令，发送 help 可查看命令列表");
    }
} // namespace


int main(int argc, char **)
{
    if (argc > 1)
    {
        std::println("提示：端口已固定为 {}，已忽略命令行参数。", kServerPort);
    }

    const int serverFd = socket(AF_INET, SOCK_STREAM, 0);
    if (serverFd < 0)
    {
        printErrno("创建套接字失败");
        return 1;
    }
    gServerFd = serverFd;

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

    std::println("TCP 服务端已监听 0.0.0.0:{}", kServerPort);

    char buffer[4096]{};

    while (gRunning)
    {
        sockaddr_in clientAddr{};
        socklen_t clientLen = sizeof(clientAddr);
        const int clientFd = accept(serverFd, reinterpret_cast<sockaddr *>(&clientAddr), &clientLen);
        if (clientFd < 0)
        {
            if (!gRunning || errno == EINTR)
            {
                continue;
            }
            printErrno("接受连接失败");
            continue;
        }

        char clientIp[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp)) == nullptr)
        {
            std::strncpy(clientIp, "未知地址", sizeof(clientIp) - 1);
            clientIp[sizeof(clientIp) - 1] = '\0';
        }

        std::println("客户端已连接：{}:{}", clientIp, ntohs(clientAddr.sin_port));

        while (gRunning)
        {
            const ssize_t receivedBytes = recv(clientFd, buffer, sizeof(buffer) - 1, 0);
            if (receivedBytes == 0)
            {
                std::println("客户端已断开连接。");
                break;
            }

            if (receivedBytes < 0)
            {
                if (errno == EINTR)
                {
                    continue;
                }
                printErrno("接收数据失败");
                break;
            }

            buffer[receivedBytes] = '\0';
            std::string message(buffer, static_cast<std::size_t>(receivedBytes));
            trimLineEnding(message);

            std::println("收到命令：{}", message);
            const std::string response = executeCommand(message) + "\n";

            if (!sendAll(clientFd, response))
            {
                printErrno("发送回复失败");
                break;
            }
        }

        close(clientFd);
    }

    if (gServerFd >= 0)
    {
        close(gServerFd);
        gServerFd = -1;
    }

    std::println("服务端已退出。");
    return 0;
}
