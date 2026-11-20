#pragma once
#include <algorithm>
#include <cerrno>
#include <cstdint>
#include <cstring>
#include <format>
#include <print>
#include <ifaddrs.h>
#include <memory>
#include <optional>
#include <system_error>
#include <string>
#include <string_view>
#include <thread>
#include <utility>
#include <vector>
#include <arpa/inet.h>
#include <net/if.h>
#include <ranges>
#include <sys/socket.h>
#include <unistd.h>
#include "../../thread-pool/include/BS_thread_pool.hpp"
#include "../../logger/logger.h"
#include "../base/owned_fd.h"
#include "http_request.h"
#include "http_response.h"

class HttpServer
{
    static constexpr int kListenBacklog = 32;

    // 打印系统错误信息
    static void printErrno(std::string_view action)
    {
        const int error = errno;
        std::println(stderr, "[HTTP] {}: errno={} ({})", action, error, std::strerror(error));
    }

    static int sendHttpResponse(int fd, const Http::Response &reply, std::string_view commonHeaders)
    {
        int status = 0;
        const std::string response = std::format("HTTP/1.1 {} {}\r\n"
                                                 "{}"
                                                 "Connection: close\r\n"
                                                 "{}"
                                                 "{}"
                                                 "\r\n{}",
                                                 reply.status, reply.reason,
                                                 reply.status == 204 ? std::string{} : std::format("Content-Length: {}\r\n", reply.body.size()),
                                                 commonHeaders, reply.extraHeaders, reply.status == 204 ? std::string_view{} : std::string_view(reply.body));
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
    Io::Fd listener;
    std::optional<BS::thread_pool<>> clients;

    void shutdown()
    {
        listener.reset();
        if (clients)
        {
            clients->purge();
            clients->wait();
            clients.reset();
        }
    }

public:
    HttpServer() = default;
    HttpServer(const HttpServer &) = delete;
    HttpServer &operator=(const HttpServer &) = delete;
    ~HttpServer() { shutdown(); }

    int init(std::uint16_t port)
    {
        int status = 0;
        if (listener.get() >= 0) return status;
        struct Rollback
        {
            HttpServer *server;
            ~Rollback() { if (server) server->shutdown(); }
        } rollback{this};
        try
        {
            listener.reset(socket(AF_INET, SOCK_STREAM | SOCK_CLOEXEC, 0));
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
            address.sin_port = htons(port);

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

            std::println(stderr, "[HTTP] 服务端已监听 http://0.0.0.0:{}", port);
            do
            {
                ifaddrs *interfaces = nullptr;
                if (getifaddrs(&interfaces) != 0)
                {
                    printErrno("枚举局域网地址失败");
                    break;
                }

                const std::unique_ptr<ifaddrs, decltype(&freeifaddrs)> owned(interfaces, freeifaddrs);
                std::vector<std::string> printedAddresses;
                for (const ifaddrs *entry = owned.get(); entry != nullptr; entry = entry->ifa_next)
                {
                    if (entry->ifa_addr == nullptr || entry->ifa_addr->sa_family != AF_INET) continue;
                    if ((entry->ifa_flags & IFF_UP) == 0 || (entry->ifa_flags & IFF_LOOPBACK) != 0) continue;

                    const std::string_view interfaceName = entry->ifa_name != nullptr ? entry->ifa_name : "";
                    char addressBuffer[INET_ADDRSTRLEN]{};
                    const auto *ipv4Address = reinterpret_cast<const sockaddr_in *>(entry->ifa_addr);
                    if (inet_ntop(AF_INET, &ipv4Address->sin_addr, addressBuffer, sizeof(addressBuffer)) == nullptr) continue;

                    const std::string address = addressBuffer;
                    if (std::ranges::find(printedAddresses, address) != printedAddresses.end()) continue;
                    printedAddresses.push_back(address);
                    std::println(stderr, "[HTTP] 局域网地址 [{}]: http://{}:{}", interfaceName, address, port);
                }
                if (printedAddresses.empty()) std::println(stderr, "[HTTP] 未检测到局域网 IPv4 地址，HTTP 服务仍监听 http://0.0.0.0:{}", port);
            } while (false);

            rollback.server = nullptr;
        }
        catch (const std::bad_alloc &) { return status = -ENOMEM; }
        catch (const std::system_error &error) { return status = -error.code().value(); }
        catch (...) { return status = -EIO; }
        return status;
    }

    template <typename Handler> int run(Handler handler, std::string commonHeaders)
    {
        int status = 0;
        if (listener.get() < 0) return status = -ENODEV;
        struct StopOnReturn
        {
            HttpServer &server;
            ~StopOnReturn() { server.shutdown(); }
        } stop{*this};
        try
        {
            clients.emplace(std::max(16u, std::thread::hardware_concurrency() * 4));
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
                    return status;
                }

                // 每个连接只处理一个请求，限制空闲客户端长期占用工作线程。
                constexpr timeval timeout{15, 0};
                if (clients->get_tasks_queued() >= kListenBacklog ||
                    setsockopt(client.get(), SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout)) != 0) continue;
                clients->detach_task([handler, commonHeaders, client = std::move(client), clientAddr] {
                    try
                    {
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
                            if (requestStatus == 413) sendHttpResponse(client.get(), {413, "Payload Too Large", R"({"ok":false,"error":"请求体过大"})"}, commonHeaders);
                            else if (requestStatus == 431) sendHttpResponse(client.get(), {431, "Request Header Fields Too Large", R"({"ok":false,"error":"请求头过大"})"}, commonHeaders);
                            else if (requestStatus == 400) sendHttpResponse(client.get(), {400, "Bad Request", R"({"ok":false,"error":"HTTP 请求格式无效"})"}, commonHeaders);
                            return;
                        }

                        LS_LOGD_TAG_FMT("HTTP", "收到请求: {}:{} {} {}", clientIp, clientPort, request.method, request.path);
                        const Http::Response response = handler(request);
                        if (const int error = sendHttpResponse(client.get(), response, commonHeaders); error != 0)
                            LS_LOGW_TAG("HTTP", "发送 HTTP 回复失败: %d", error);
                    }
                    catch (const std::exception &error) { LS_LOGE_TAG("HTTP", "处理请求失败: %s", error.what()); }
                    catch (...) { LS_LOGE_TAG("HTTP", "处理请求失败: unknown exception"); }
                });
            }
        }
        catch (const std::bad_alloc &) { return status = -ENOMEM; }
        catch (const std::system_error &error) { return status = -error.code().value(); }
        catch (...) { return status = -EIO; }
        return status;
    }
};

inline std::unique_ptr<HttpServer> gHttpServer = nullptr;
