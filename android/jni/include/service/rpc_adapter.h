#pragma once
#include <nlohmann/json.hpp>
#include <string>
#include <string_view>

namespace Rpc
{
    using nlohmann::json;

    template <typename Dispatch> json Handle(std::string_view body, Dispatch &&dispatch)
    {
        const auto request = json::parse(body, nullptr, false);
        if (request.is_discarded()) return {{"ok", false}, {"error", "请求必须是 JSON 字符串对象"}};
        if (!request.is_object()) return {{"ok", false}, {"error", "请求必须是 JSON 对象"}};
        const auto operation = request.find("operation");
        if (operation == request.end()) return {{"ok", false}, {"error", "请求缺少 operation 字段"}};
        if (!operation->is_string()) return {{"ok", false}, {"error", "operation 字段必须是字符串"}};
        const auto &name = operation->get_ref<const std::string &>();
        const auto params = request.find("params");
        if (params != request.end() && !params->is_object()) return {{"ok", false}, {"operation", name}, {"error", "params 字段必须是对象"}};
        try
        {
            return dispatch(name, params == request.end() ? json::object() : *params);
        }
        catch (const std::exception &exception)
        {
            return {{"ok", false}, {"operation", name}, {"error", exception.what()}};
        }
    }
}