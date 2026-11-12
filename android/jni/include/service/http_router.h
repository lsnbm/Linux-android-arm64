#pragma once
#include "mcp_adapter.h"
#include "rpc_adapter.h"

namespace Http
{
    struct Response
    {
        int status;
        std::string_view reason;
        nlohmann::json body;
        std::string_view extraHeaders = {};
    };

    template <typename Dispatch> Response Route(std::string_view method, std::string_view path, const nlohmann::json &headers, std::string_view body, Dispatch &&dispatch)
    {
        if (path == "/mcp")
        {
            auto response = Mcp::HandleHttp(method, headers, body, dispatch);
            const std::string_view reason = response.status == 200 ? "OK" : response.status == 202 ? "Accepted" :
                response.status == 204 ? "No Content" : response.status == 403 ? "Forbidden" : response.status == 405 ? "Method Not Allowed" :
                response.status == 415 ? "Unsupported Media Type" : "Bad Request";
            return {response.status, reason, std::move(response.body), response.status == 405 ? "Allow: POST, OPTIONS\r\n" : ""};
        }
        if (method == "OPTIONS") return {204, "No Content", nullptr};
        if (method == "GET" && path == "/health") return {200, "OK", {{"ok", true}, {"service", "LS_KTool"}, {"transport", "http"}}};
        if (method == "POST" && path == "/api/rpc") return {200, "OK", Rpc::Handle(body, dispatch)};
        if (method != "GET" && method != "POST") return {405, "Method Not Allowed", {{"ok", false}, {"error", "不支持的 HTTP 方法"}}};
        return {404, "Not Found", {{"ok", false}, {"error", "接口不存在"}}};
    }
}