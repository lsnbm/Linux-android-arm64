#pragma once
#include <array>
#include <cerrno>
#include <charconv>
#include <cctype>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <format>
#include <print>
#include <ifaddrs.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <ranges>
#include <sched.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#include "../../thread-pool/include/BS_thread_pool.hpp"
#include "../../logger/logger.h"
#include "http_router.h"

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

    // 打印系统错误信息
    void printErrno(std::string_view action)
    {
        const int error = errno;
        std::println(stderr, "[HTTP] {}: errno={} ({})", action, error, std::strerror(error));
    }

    void PrintLanHttpAddresses()
    {
        ifaddrs *interfaces = nullptr;
        if (getifaddrs(&interfaces) != 0)
        {
            printErrno("枚举局域网地址失败");
            return;
        }

        std::vector<std::string> printedAddresses;
        for (const ifaddrs *entry = interfaces; entry != nullptr; entry = entry->ifa_next)
        {
            if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET) continue;
            if ((entry->ifa_flags & IFF_UP) == 0 || (entry->ifa_flags & IFF_LOOPBACK) != 0) continue;

            const std::string_view interfaceName = entry->ifa_name != nullptr ? entry->ifa_name : "";
            constexpr std::array<std::string_view, 6> prefixes = {"wlan", "eth", "ap", "rndis", "usb", "bt-pan"};
            if (std::ranges::none_of(prefixes, [interfaceName](std::string_view prefix) { return interfaceName.starts_with(prefix); })) continue;

            char addressBuffer[INET_ADDRSTRLEN]{};
            const auto *ipv4Address = reinterpret_cast<const sockaddr_in *>(entry->ifa_addr);
            if (inet_ntop(AF_INET, &ipv4Address->sin_addr, addressBuffer, sizeof(addressBuffer)) == nullptr) continue;

            const std::string address = addressBuffer;
            if (std::ranges::find(printedAddresses, address) != printedAddresses.end()) continue;
            printedAddresses.push_back(address);
            std::println(stderr, "[HTTP] 局域网地址 [{}]: http://{}:{}", interfaceName, address, kServerPort);
            std::println(stderr, "[HTTP] 局域网 RPC [{}]: http://{}:{}/api/rpc", interfaceName, address, kServerPort);
            std::println(stderr, "[MCP] 局域网 MCP [{}]: http://{}:{}/mcp", interfaceName, address, kServerPort);
        }
        freeifaddrs(interfaces);

        if (printedAddresses.empty()) std::println(stderr, "[HTTP] 未检测到局域网 IPv4 地址，HTTP 服务仍监听 http://0.0.0.0:{}", kServerPort);
    }

    bool WritePrivateEtcFile(const char *path, std::string_view content)
    {
        const int fd = open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644);
        if (fd < 0) return false;

        std::size_t written = 0;
        while (written < content.size())
        {
            const ssize_t result = write(fd, content.data() + written, content.size() - written);
            if (result < 0 && errno == EINTR) continue;
            if (result <= 0)
            {
                close(fd);
                return false;
            }
            written += static_cast<std::size_t>(result);
        }

        return close(fd) == 0;
    }

    void StartCloudflared()
    {
        const std::size_t blobSize = static_cast<std::size_t>(cloudflared_blob_end - cloudflared_blob_start);
        if (blobSize == 0) return;

        const int fd = open(kCloudflaredTempPath.data(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0700);
        if (fd < 0)
        {
            printErrno("创建 cloudflared 临时文件失败");
            return;
        }

        std::size_t written = 0;
        while (written < blobSize)
        {
            const ssize_t result = write(fd, cloudflared_blob_start + written, blobSize - written);
            if (result < 0 && errno == EINTR) continue;
            if (result <= 0)
            {
                if (result < 0) printErrno("释放 cloudflared 失败");
                close(fd);
                unlink(kCloudflaredTempPath.data());
                return;
            }
            written += static_cast<std::size_t>(result);
        }

        if (fchmod(fd, 0700) != 0)
        {
            printErrno("设置 cloudflared 权限失败");
            close(fd);
            unlink(kCloudflaredTempPath.data());
            return;
        }
        close(fd);

        if (rename(kCloudflaredTempPath.data(), kCloudflaredPath.data()) != 0)
        {
            printErrno("安装 cloudflared 失败");
            unlink(kCloudflaredTempPath.data());
            return;
        }

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
            constexpr std::string_view resolvConf = "nameserver 1.1.1.1\n"
                                                    "nameserver 8.8.8.8\n"
                                                    "options timeout:2 attempts:2\n";
            constexpr std::string_view hosts = "127.0.0.1 localhost\n"
                                               "::1 ip6-localhost\n";

            if (unshare(CLONE_NEWNS) != 0 || mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0 ||
                mount("tmpfs", "/system/etc", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=64k") != 0 ||
                !WritePrivateEtcFile("/system/etc/resolv.conf", resolvConf) || !WritePrivateEtcFile("/system/etc/hosts", hosts) ||
                setenv("SSL_CERT_DIR", "/apex/com.android.conscrypt/cacerts", 1) != 0)
            {
                printErrno("配置 cloudflared 私有 DNS 失败");
                _exit(126);
            }
            execl(kCloudflaredPath.data(), kCloudflaredPath.data(), "tunnel", "--no-autoupdate", "--edge-ip-version", "4", "--url", "http://127.0.0.1:9494", static_cast<char *>(nullptr));
            printErrno("执行 cloudflared 失败");
            _exit(127);
        }

        std::println(stderr, "[HTTP] cloudflared Tunnel 已启动，PID={}，公网地址将写入 /storage/emulated/0/log.txt", pid);
    }

    bool sendHttpResponse(int fd, int statusCode, std::string_view statusText, std::string_view body, std::string_view extraHeaders = {})
    {
        const std::string response = std::format("HTTP/1.1 {} {}\r\n"
                                                 "Content-Type: application/json; charset=utf-8\r\n"
                                                 "Content-Length: {}\r\n"
                                                 "Connection: close\r\n"
                                                 "Access-Control-Allow-Origin: *\r\n"
                                                 "Access-Control-Allow-Headers: Content-Type, Authorization, MCP-Protocol-Version\r\n"
                                                 "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n"
                                                 "{}"
                                                 "\r\n{}",
                                                 statusCode, statusText, body.size(), extraHeaders, body);
        std::size_t sentTotal = 0;
        while (sentTotal < response.size())
        {
            const ssize_t sent = send(fd, response.data() + sentTotal, response.size() - sentTotal, MSG_NOSIGNAL);
            if (sent < 0 && errno == EINTR) continue;
            if (sent <= 0) return false;
            sentTotal += static_cast<std::size_t>(sent);
        }
        return true;
    }

    bool receiveHttpRequest(int fd, std::string *method, std::string *path, std::string *body, json *headers, int *errorStatus)
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
            headerEnd = request.find("\r\n\r\n");
            if ((headerEnd == std::string::npos ? request.size() : headerEnd + 4) > kMaxHttpHeaderBytes)
            {
                *errorStatus = 431;
                return false;
            }
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
            if (colon == std::string_view::npos || colon == 0)
            {
                *errorStatus = 400;
                return false;
            }
            std::string name(line.substr(0, colon));
            std::ranges::transform(name, name.begin(), [](unsigned char character) { return static_cast<char>(std::tolower(character)); });
            std::string_view value = line.substr(colon + 1);
            const std::size_t valueStart = value.find_first_not_of(" \t");
            value = valueStart == std::string_view::npos ? std::string_view{} : value.substr(valueStart, value.find_last_not_of(" \t") - valueStart + 1);
            if (headers->contains(name))
            {
                if (name == "accept") (*headers)[name] = (*headers)[name].get<std::string>() + "," + std::string(value);
                else
                {
                    *errorStatus = 400;
                    return false;
                }
            }
            else (*headers)[name] = value;
            if (name == "transfer-encoding")
            {
                *errorStatus = 400;
                return false;
            }
            if (name == "content-length")
            {
                if (value.empty())
                {
                    *errorStatus = 400;
                    return false;
                }
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

} // namespace

// 程序入口：初始化 HTTP 服务并处理客户端请求。
template <typename Dispatch> int http_server(Dispatch dispatch, BS::thread_pool<> &clients)
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

    std::println(stderr, "[HTTP] 服务端已监听 http://0.0.0.0:{}", kServerPort);
    std::println(stderr, "[MCP] Streamable HTTP: http://127.0.0.1:{}/mcp", kServerPort);
    PrintLanHttpAddresses();
    StartCloudflared();

    try
    {
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

            clients.detach_task([dispatch, clientFd, clientAddr] {
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
                json headers = json::object();
                int errorStatus = 0;
                if (!receiveHttpRequest(clientFd, &method, &path, &body, &headers, &errorStatus))
                {
                    if (errorStatus == 413) sendHttpResponse(clientFd, 413, "Payload Too Large", R"({"ok":false,"error":"请求体过大"})");
                    else if (errorStatus == 431) sendHttpResponse(clientFd, 431, "Request Header Fields Too Large", R"({"ok":false,"error":"请求头过大"})");
                    else if (errorStatus != 0) sendHttpResponse(clientFd, 400, "Bad Request", R"({"ok":false,"error":"HTTP 请求格式无效"})");
                    close(clientFd);
                    return;
                }

                LS_LOGD_TAG_FMT("HTTP", "收到请求: {}:{} {} {}", clientIp, clientPort, method, path);
                const auto response = Http::Route(method, path, headers, body, dispatch);
                const std::string responseBody = response.body.is_null() ? "" : response.body.dump(-1, ' ', false, json::error_handler_t::replace);
                if (!sendHttpResponse(clientFd, response.status, response.reason, responseBody, response.extraHeaders)) printErrno("发送 HTTP 回复失败");
                close(clientFd);
            });
        }
    }
    catch (...)
    {
        close(serverFd);
        clients.wait();
        throw;
    }
}
