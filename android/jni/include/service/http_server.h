#pragma once
#include <array>
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <print>
#include <ifaddrs.h>
#include <memory>
#include <arpa/inet.h>
#include <net/if.h>
#include <ranges>
#include <sched.h>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include "../../thread-pool/include/BS_thread_pool.hpp"
#include "../../logger/logger.h"
#include "../base/owned_fd.h"
#include "http_request.h"
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

        const std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> owned(interfaces, freeifaddrs);
        std::vector<std::string> printedAddresses;
        for (const ifaddrs *entry = owned.get(); entry != nullptr; entry = entry->ifa_next)
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

    struct TunnelChild
    {
        pid_t pid = -1;

        TunnelChild() = default;
        TunnelChild(const TunnelChild &) = delete;
        TunnelChild &operator=(const TunnelChild &) = delete;
        ~TunnelChild()
        {
            if (pid <= 0) return;
            pid_t result;
            do { result = waitpid(pid, nullptr, WNOHANG); } while (result < 0 && errno == EINTR);
            if (result != 0) return;
            if (kill(pid, SIGKILL) != 0 && errno != ESRCH) return;
            while (waitpid(pid, nullptr, 0) < 0 && errno == EINTR) {}
        }
    };

    void StartCloudflared(const std::filesystem::path &directory, TunnelChild &child)
    {
        const std::size_t blobSize = static_cast<std::size_t>(cloudflared_blob_end - cloudflared_blob_start);
        if (blobSize == 0) return;

        const std::filesystem::path cloudflaredPath = directory / ".cloudflared";
        const std::filesystem::path cloudflaredTempPath = directory / ".cloudflared.tmp";
        const int fd = open(cloudflaredTempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0700);
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
                unlink(cloudflaredTempPath.c_str());
                return;
            }
            written += static_cast<std::size_t>(result);
        }

        if (fchmod(fd, 0700) != 0)
        {
            printErrno("设置 cloudflared 权限失败");
            close(fd);
            unlink(cloudflaredTempPath.c_str());
            return;
        }
        close(fd);

        if (rename(cloudflaredTempPath.c_str(), cloudflaredPath.c_str()) != 0)
        {
            printErrno("安装 cloudflared 失败");
            unlink(cloudflaredTempPath.c_str());
            return;
        }

        const pid_t pid = fork();
        if (pid < 0)
        {
            printErrno("启动 cloudflared 失败");
            unlink(cloudflaredPath.c_str());
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
            execl(cloudflaredPath.c_str(), cloudflaredPath.c_str(), "tunnel", "--no-autoupdate", "--edge-ip-version", "4", "--url", "http://127.0.0.1:9494", static_cast<char *>(nullptr));
            printErrno("执行 cloudflared 失败");
            _exit(127);
        }

        child.pid = pid;
        std::println(stderr, "[HTTP] cloudflared Tunnel 已启动，PID={}，公网地址将写入 /storage/emulated/0/log.txt", pid);
    }

    int sendHttpResponse(int fd, int statusCode, std::string_view statusText, std::string_view body, std::string_view extraHeaders = {})
    {
        int status = 0;
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
            if (sent <= 0) return status = sent < 0 ? -errno : -EPIPE;
            sentTotal += static_cast<std::size_t>(sent);
        }
        return status;
    }


} // namespace

// 程序入口：初始化 HTTP 服务并处理客户端请求。
template <typename Dispatch> int http_server(Dispatch dispatch, const std::filesystem::path &cloudflaredDirectory)
{
    int status = 0;
    const Io::Fd listener(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
    if (listener.get() < 0)
    {
        status = -errno;
        printErrno("创建套接字失败");
        return status;
    }
    constexpr int enableReuse = 1;
    if (setsockopt(listener.get(), SOL_SOCKET, SO_REUSEADDR, &enableReuse, sizeof(enableReuse)) < 0)
    {
        status = -errno;
        printErrno("设置套接字选项失败");
        return status;
    }

    sockaddr_in address{};
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(kServerPort);

    if (bind(listener.get(), reinterpret_cast<sockaddr *>(&address), sizeof(address)) < 0)
    {
        status = -errno;
        printErrno("绑定端口失败");
        return status;
    }

    if (listen(listener.get(), kListenBacklog) < 0)
    {
        status = -errno;
        printErrno("开始监听失败");
        return status;
    }

    std::println(stderr, "[HTTP] 服务端已监听 http://0.0.0.0:{}", kServerPort);
    std::println(stderr, "[MCP] Streamable HTTP: http://127.0.0.1:{}/mcp", kServerPort);
    BS::thread_pool<> clients(std::max(16u, std::thread::hardware_concurrency() * 4));
    PrintLanHttpAddresses();
    TunnelChild tunnel;
    StartCloudflared(cloudflaredDirectory, tunnel);

    try
    {
        for (;;)
        {
            sockaddr_in clientAddr{};
            socklen_t clientLen = sizeof(clientAddr);
            Io::Fd client(accept4(listener.get(), reinterpret_cast<sockaddr *>(&clientAddr), &clientLen, SOCK_CLOEXEC));
            if (client.get() < 0)
            {
                if (errno == EINTR) continue;
                status = -errno;
                printErrno("接受连接失败");
                break;
            }

            // 每个连接只处理一个请求，限制空闲客户端长期占用工作线程。
            constexpr timeval timeout{15, 0};
            if (clients.get_tasks_queued() >= kListenBacklog ||
                setsockopt(client.get(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) continue;
            clients.detach_task([dispatch, client = std::move(client), clientAddr] {
                char clientIp[INET_ADDRSTRLEN]{};
                if (inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp)) == nullptr)
                {
                    std::strncpy(clientIp, "未知地址", sizeof(clientIp) - 1);
                    clientIp[sizeof(clientIp) - 1] = '\0';
                }

                const auto clientPort = ntohs(clientAddr.sin_port);
                Http::Request request;
                const int requestStatus = Http::Receive(client.get(), request);
                if (requestStatus != 0)
                {
                    if (requestStatus == 413) sendHttpResponse(client.get(), 413, "Payload Too Large", R"({"ok":false,"error":"请求体过大"})");
                    else if (requestStatus == 431) sendHttpResponse(client.get(), 431, "Request Header Fields Too Large", R"({"ok":false,"error":"请求头过大"})");
                    else if (requestStatus == 400) sendHttpResponse(client.get(), 400, "Bad Request", R"({"ok":false,"error":"HTTP 请求格式无效"})");
                    return;
                }

                LS_LOGD_TAG_FMT("HTTP", "收到请求: {}:{} {} {}", clientIp, clientPort, request.method, request.path);
                const auto response = Http::Route(request.method, request.path, request.headers, request.body, dispatch);
                const std::string responseBody = response.body.is_null() ? "" : response.body.dump(-1, ' ', false, json::error_handler_t::replace);
                if (const int error = sendHttpResponse(client.get(), response.status, response.reason, responseBody, response.extraHeaders); error != 0)
                    LS_LOGW_TAG("HTTP", "发送 HTTP 回复失败: %d", error);
            });
        }
    }
    catch (...)
    {
        clients.purge();
        clients.wait();
        throw;
    }
    clients.purge();
    clients.wait();
    return status;
}
