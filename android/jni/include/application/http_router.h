#pragma once
#include "mcp_adapter.h"
#include "../service/http_response.h"
#include "../service/rpc_adapter.h"

namespace Http
{
    template <typename Dispatch> Response Route(std::string_view method, std::string_view path, const nlohmann::json &headers, std::string_view body, Dispatch &&dispatch)
    {
        Response response;
        nlohmann::json responseBody;
        if (path == "/mcp")
        {
            auto result = Mcp::gMcp->HandleHttp(method, headers, body, dispatch);
            response = {result.status,
                result.status == 200 ? "OK" : result.status == 202 ? "Accepted" : result.status == 204 ? "No Content" :
                result.status == 403 ? "Forbidden" : result.status == 405 ? "Method Not Allowed" : result.status == 415 ? "Unsupported Media Type" : "Bad Request",
                {}, result.status == 405 ? "Allow: POST, OPTIONS\r\n" : ""};
            responseBody = std::move(result.body);
        }
        else if (method == "OPTIONS") response = {204, "No Content", {}};
        else if (method == "GET" && path == "/health")
        {
            response = {200, "OK", {}};
            responseBody = {{"ok", true}, {"service", "LS_KTool"}, {"transport", "http"}};
        }
        else if (method == "POST" && path == "/api/rpc")
        {
            response = {200, "OK", {}};
            responseBody = Rpc::Handle(body, dispatch);
        }
        else if (method != "GET" && method != "POST")
        {
            response = {405, "Method Not Allowed", {}};
            responseBody = {{"ok", false}, {"error", "不支持的 HTTP 方法"}};
        }
        else
        {
            response = {404, "Not Found", {}};
            responseBody = {{"ok", false}, {"error", "接口不存在"}};
        }
        if (!responseBody.is_null()) response.body = responseBody.dump(-1, ' ', false, nlohmann::json::error_handler_t::replace);
        return response;
    }
}