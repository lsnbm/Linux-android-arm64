#pragma once
#include <algorithm>
#include <array>
#include <cerrno>
#include <charconv>
#include <chrono>
#include <poll.h>
#include <string>
#include <string_view>
#include <sys/socket.h>
#include <utility>
#include <nlohmann/json.hpp>

namespace Http
{
    struct Request
    {
        std::string method, path, body;
        nlohmann::json headers = nlohmann::json::object();
    };

    // RFC 9110 §5.6.2：method 和 field-name 使用相同的 ASCII token 字符集。
    inline bool TokenChar(unsigned char c) noexcept
    {
        return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9') ||
               std::string_view("!#$%&'*+-.^_`|~").find(static_cast<char>(c)) != std::string_view::npos;
    }

    // 正数为 HTTP 拒绝状态，负数为传输错误；完整收到请求后才提交 output。
    inline int Receive(int fd, Request &output)
    {
        int status = 0;
        constexpr std::size_t kMaxHttpHeaderBytes = 64 * 1024;
        constexpr std::size_t kMaxHttpBodyBytes = 16 * 1024 * 1024;
        Request parsed;
        std::string request;
        request.reserve(4096);
        std::array<char, 4096> chunk{};
        std::size_t headerEnd = std::string::npos;
        const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(15);
        auto readChunk = [&](std::size_t bytes) -> int
        {
            int status = 0;
            pollfd input{fd, POLLIN, 0};
            for (;;)
            {
                const auto ms = std::chrono::ceil<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now()).count();
                if (ms <= 0) return status = -ETIMEDOUT;
                const int ready = poll(&input, 1, static_cast<int>(ms));
                if (!ready) return status = -ETIMEDOUT;
                if (ready < 0 && errno == EINTR) continue;
                if (ready < 0) return status = -errno;
                status = static_cast<int>(recv(fd, chunk.data(), bytes, MSG_DONTWAIT));
                if (status >= 0) return status;
                if (errno != EINTR && errno != EAGAIN && errno != EWOULDBLOCK) return status = -errno;
            }
        };

        while (headerEnd == std::string::npos)
        {
            const int received = readChunk(chunk.size());
            if (received == 0) return status = -ECONNRESET;
            if (received < 0) return status = received;

            request.append(chunk.data(), static_cast<std::size_t>(received));
            headerEnd = request.find("\r\n\r\n");
            if ((headerEnd == std::string::npos ? request.size() : headerEnd + 4) > kMaxHttpHeaderBytes) return status = 431;
        }

        const std::size_t requestLineEnd = request.find("\r\n");

        const std::string_view requestLine(request.data(), requestLineEnd);
        const std::size_t firstSpace = requestLine.find(' ');
        const std::size_t secondSpace = firstSpace == std::string_view::npos ? std::string_view::npos : requestLine.find(' ', firstSpace + 1);
        if (firstSpace == std::string_view::npos || secondSpace == std::string_view::npos) return status = 400;
        const auto method = requestLine.substr(0, firstSpace);
        const auto path = requestLine.substr(firstSpace + 1, secondSpace - firstSpace - 1);
        const auto version = requestLine.substr(secondSpace + 1);
        if (method.empty() || !std::ranges::all_of(method, TokenChar) || path.empty() || (version != "HTTP/1.1" && version != "HTTP/1.0") ||
            std::ranges::any_of(path, [](unsigned char c) { return c <= 0x20 || c == 0x7f; })) return status = 400;
        parsed.method = method;
        parsed.path = path;

        std::size_t contentLength = 0;
        std::size_t lineStart = requestLineEnd + 2;
        while (lineStart < headerEnd)
        {
            const std::size_t lineEnd = request.find("\r\n", lineStart);
            const std::string_view line(request.data() + lineStart, lineEnd - lineStart);
            const std::size_t colon = line.find(':');
            if (colon == std::string_view::npos || colon == 0) return status = 400;
            std::string name(line.substr(0, colon));
            for (char &c : name)
            {
                if (!TokenChar(c)) return status = 400;
                if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
            }
            std::string_view value = line.substr(colon + 1);
            if (name == "transfer-encoding" || std::ranges::any_of(value, [](unsigned char c) { return (c < 0x20 && c != '\t') || c == 0x7f; })) return status = 400;
            const std::size_t valueStart = value.find_first_not_of(" \t");
            value = valueStart == std::string_view::npos ? std::string_view{} : value.substr(valueStart, value.find_last_not_of(" \t") - valueStart + 1);
            if (parsed.headers.contains(name))
            {
                if (name != "accept") return status = 400;
                parsed.headers[name].get_ref<std::string &>().append(",").append(value);
            }
            else parsed.headers[name] = value;
            if (name == "content-length")
            {
                if (value.empty()) return status = 400;
                const auto [ptr, ec] = std::from_chars(value.data(), value.data() + value.size(), contentLength);
                if (ec != std::errc{} || ptr != value.data() + value.size()) return status = 400;
            }
            lineStart = lineEnd + 2;
        }

        if (contentLength > kMaxHttpBodyBytes) return status = 413;

        const std::size_t bodyStart = headerEnd + 4;
        while (request.size() - bodyStart < contentLength)
        {
            const int received = readChunk(std::min(chunk.size(), contentLength - (request.size() - bodyStart)));
            if (received == 0) return status = 400;
            if (received < 0) return status = received;
            request.append(chunk.data(), static_cast<std::size_t>(received));
        }

        parsed.body.assign(request.data() + bodyStart, contentLength);
        output = std::move(parsed);
        return status;
    }
}
