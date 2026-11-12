#pragma once
#include <algorithm>
#include <cstdint>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>
#include <string_view>
#include <vector>

namespace Mcp
{
    using nlohmann::json;

    struct Tool
    {
        std::string_view operation;
        json descriptor;
    };

    inline const std::vector<Tool> &Tools()
    {
        static const std::vector<Tool> tools = []
        {
            const json text = {{"type", "string"}};
            const json address = {{"type", "string"}, {"pattern", "^0[xX][0-9a-fA-F]{1,16}$"}, {"maxLength", 18}};
            const auto integer = [](int64_t minimum, int64_t maximum) -> json { return {{"type", "integer"}, {"minimum", minimum}, {"maximum", maximum}}; };
            const auto options = [](json values) -> json { return {{"type", "string"}, {"enum", std::move(values)}}; };
            const auto preset = [](json schema, json value) { schema["default"] = std::move(value); return schema; };
            const auto object = [](json properties, json required) -> json
            {
                return {{"type", "object"}, {"properties", std::move(properties)}, {"required", std::move(required)}, {"additionalProperties", false}};
            };
            const json valueType = options({"i8", "i16", "i32", "i64", "f32", "f64"});
            const json viewFormat = options({"hexadecimal", "hex", "i8", "i16", "i32", "i64", "f32", "f64", "disasm"});
            const json fileName = preset(text, "Signature.txt");
            std::vector<Tool> result;
            const auto add = [&](std::string_view name, std::string_view operation, std::string_view description,
                                 json properties = json::object(), json required = json::array())
            {
                result.push_back({operation, {{"name", name}, {"description", description}, {"inputSchema", object(std::move(properties), std::move(required))}}});
            };

            add("android_bridge_ping", "bridge.ping", "Check the native Android service.");
            add("android_target_set_pid", "target.select", "Select the target PID shared by MCP and the desktop debugger.", {{"pid", integer(1, 2147483647)}}, {"pid"});
            add("android_target_attach_package", "target.attach", "Find and select a running package.", {{"package_name", text}}, {"package_name"});
            add("android_target_find_pid", "target.find", "Find a running package PID.", {{"package_name", text}}, {"package_name"});
            add("android_target_current", "target.get", "Read the selected PID.");
            add("android_env_get_params", "env.read", "Read target TLS and PACGA parameters.", {{"thread_name", preset(text, "")}});
            add("android_memory_regions", "memory.map", "List target modules and their segments.");
            add("android_module_address", "module.resolve", "Resolve a module segment address; -1 selects BSS.",
                {{"module_name", text}, {"segment_index", preset(integer(-1, 32767), 0)}, {"which", preset(options({"start", "end"}), "start")}}, {"module_name"});
            add("android_memory_dump", "memory.dump", "Dump a module or a hexadecimal start-end range on Android.", {{"target", text}}, {"target"});

            json scanProperties = {{"mode", options({"unknown", "equal", "greater", "less", "range", "pointer", "string"})},
                {"value_type", preset(valueType, "i32")}, {"value", preset(text, "")}, {"range_max", preset(text, "")},
                {"rounding", preset(options({"rounded", "truncated", "extreme", "exact"}), "rounded")},
                {"alignment", {{"type", "integer"}, {"enum", {0, 1, 2, 4, 8}}, {"default", 0}}}};
            add("android_memory_scan_start", "scan.start", "Start a scan. Values are strings; pointer forces i64. alignment=1 scans every byte; 0 uses default alignment. Float rounding uses the decimal precision of value.", scanProperties, {"mode"});
            scanProperties["mode"] = options({"equal", "greater", "less", "increased", "decreased", "changed", "unchanged", "range", "pointer", "string"});
            add("android_memory_scan_refine", "scan.refine", "Refine the existing scan; keep its value type and alignment.", scanProperties, {"mode"});
            add("android_memory_scan_results", "scan.results", "Read a page of scan results with exact hexadecimal addresses.",
                {{"start", preset(integer(0, 9007199254740991LL), 0)}, {"count", preset(integer(1, 200), 100)},
                 {"value_type", preset(options({"i8", "i16", "i32", "i64", "f32", "f64", "string"}), "i32")}});
            add("android_memory_scan_status", "scan.get", "Read scan progress and result count.");
            add("android_memory_scan_clear", "scan.clear", "Clear the scan results.");
            add("android_memory_read", "memory.read", "Read 1..1048576 bytes at a hexadecimal address.", {{"address", address}, {"size", integer(1, 1048576)}}, {"address", "size"});
            add("android_memory_write", "memory.write", "Write up to 1048576 bytes of hex digits, without a 0x prefix; whitespace is allowed.", {{"address", address}, {"data_hex", text}}, {"address", "data_hex"});

            add("android_saved_list", "saved.list", "Read saved addresses, values and lock states.");
            add("android_saved_add", "saved.add", "Save an address. pointer forces i64; text forces i8 and uses text_length.",
                {{"address", address}, {"value_type", preset(valueType, "i32")}, {"value_kind", preset(options({"numeric", "pointer", "text"}), "numeric")},
                 {"text_length", preset(integer(1, 256), 64)}, {"note", preset(text, "")}}, {"address"});
            add("android_saved_remove", "saved.remove", "Remove a saved address and its lock.", {{"address", address}}, {"address"});
            add("android_saved_write", "saved.write", "Write a saved address using its saved type.", {{"address", address}, {"value", text}}, {"address", "value"});
            add("android_saved_set_note", "saved.note.set", "Set or clear a saved address note.", {{"address", address}, {"note", text}}, {"address", "note"});
            add("android_saved_set_locked", "saved.lock.set", "Set a saved lock. Empty value locks the current value; unlocking ignores value.",
                {{"address", address}, {"locked", {{"type", "boolean"}}}, {"value", preset(text, "")}}, {"address", "locked"});
            add("android_saved_offset", "saved.offset", "Offset all saved addresses by signed hexadecimal bytes.", {{"offset", text}}, {"offset"});
            add("android_saved_clear", "saved.clear", "Clear saved addresses and locks.");

            add("android_pointer_status", "pointer.get", "Read pointer task state and result count.");
            add("android_pointer_scan", "pointer.scan", "Start a pointer scan. manual requires manual_base; array requires array_base and array_count.",
                {{"target", address}, {"depth", integer(1, 16)}, {"max_offset", integer(1, 2147483647)},
                 {"mode", preset(options({"module", "manual", "array"}), "module")}, {"manual_base", address}, {"array_base", address},
                 {"array_count", integer(1, 1000000)}, {"module_filter", preset(text, "")}}, {"target", "depth", "max_offset"});
            add("android_pointer_merge", "pointer.merge", "Intersect saved pointer bin files by offset structure.");
            add("android_pointer_export", "pointer.export", "Export merged pointer chains as text on Android.");
            add("android_memory_view_open", "viewer.open", "Open an address and read a 100-byte snapshot.", {{"address", address}, {"view_format", preset(viewFormat, "hexadecimal")}}, {"address"});
            add("android_memory_view_offset", "viewer.seek", "Move the viewer by a signed hexadecimal byte offset.", {{"offset", text}}, {"offset"});
            add("android_memory_view_set_format", "viewer.format", "Change format and refresh the viewer.", {{"view_format", viewFormat}}, {"view_format"});
            add("android_memory_view_read", "viewer.refresh", "Refresh the current viewer snapshot and disassembly.");

            const json point = object({{"address", address}, {"bp_type", options({"read", "write", "read_write", "execute"})},
                {"bp_scope", options({"main", "other", "all"})}, {"length", integer(1, 8)}}, {"address", "bp_type", "bp_scope", "length"});
            add("android_breakpoint_get", "breakpoint.get", "Read breakpoints and registers as exact hexadecimal strings.");
            add("android_breakpoint_set", "breakpoint.set", "Set 1..16 breakpoints with nonzero hexadecimal addresses.",
                {{"mode", options({"hwbp", "ptebp", "stepbp"})}, {"points", {{"type", "array"}, {"items", point}, {"minItems", 1}, {"maxItems", 16}}}}, {"mode", "points"});
            add("android_breakpoint_clear", "breakpoint.clear", "Clear the active breakpoint mode.");
            add("android_breakpoint_record_update", "breakpoint_record.update", "Patch a register, mask or op.<register>; value is a 64/128-bit hexadecimal string.",
                {{"index", integer(0, 2147483647)}, {"field", {{"type", "string"}, {"maxLength", 32}, {"pattern", "^(?:(?:op\\.|mask\\.)?(?:pc|hit_count|lr|sp|orig_x0|syscallno|pstate|fpsr|fpcr|x(?:[0-9]|[12][0-9])|q(?:[0-9]|[12][0-9]|3[01]))|mask(?:[0-9]|1[0-7])|mask\\[(?:[0-9]|1[0-7])\\])$"}}},
                 {"value", {{"type", "string"}, {"maxLength", 34}, {"pattern", "^0[xX][0-9a-fA-F]{1,32}$"}}}}, {"index", "field", "value"});
            add("android_syscall_start", "syscall.start", "Start syscall monitoring and read the available log.");
            add("android_syscall_stop", "syscall.stop", "Stop syscall monitoring.");
            add("android_syscall_log", "syscall.read", "Read available syscall log lines.");
            add("android_cntvct_start", "cntvct.start", "Start CNTVCT_EL0 monitoring and read the available log.");
            add("android_cntvct_stop", "cntvct.stop", "Stop CNTVCT_EL0 monitoring.");
            add("android_cntvct_log", "cntvct.read", "Read available CNTVCT log lines.");
            add("android_signature_scan_address", "signature.create", "Generate a signature using range_size bytes before and after an address.",
                {{"address", address}, {"range_size", integer(1, 1200)}, {"file_name", fileName}}, {"address", "range_size"});
            add("android_signature_scan_file", "signature.scan", "Scan an Android-side signature file.", {{"file_name", fileName}});
            add("android_signature_scan_pattern", "signature.match", "Scan a byte pattern; ? and ?? are wildcards.",
                {{"pattern", {{"type", "string"}, {"minLength", 1}}}, {"range_offset", preset(integer(-2147483648LL, 2147483647), 0)}}, {"pattern"});
            add("android_signature_filter", "signature.filter", "Filter changed bytes in an Android-side signature file.", {{"address", address}, {"file_name", fileName}}, {"address"});
            return result;
        }();
        return tools;
    }

    inline std::string ValidateArguments(json &value, const json &schema, const std::string &path = "arguments")
    {
        const auto type = schema.at("type").get<std::string>();
        if ((type == "object" && !value.is_object()) || (type == "array" && !value.is_array()) ||
            (type == "string" && !value.is_string()) || (type == "integer" && !value.is_number_integer()) ||
            (type == "boolean" && !value.is_boolean())) return path + " must be " + type;
        if (schema.contains("enum") && std::find(schema["enum"].begin(), schema["enum"].end(), value) == schema["enum"].end()) return path + " has an invalid value";
        if (type == "object")
        {
            const auto &properties = schema.at("properties");
            for (const auto &key : schema.at("required"))
                if (!value.contains(key.get<std::string>())) return path + "." + key.get<std::string>() + " is required";
            for (const auto &[key, child] : value.items())
                if (!properties.contains(key)) return path + "." + key + " is unknown";
            for (const auto &[key, property] : properties.items())
            {
                if (!value.contains(key) && property.contains("default")) value[key] = property["default"];
                if (!value.contains(key)) continue;
                const auto error = ValidateArguments(value[key], property, path + "." + key);
                if (!error.empty()) return error;
            }
        }
        else if (type == "array")
        {
            if (value.size() < schema.at("minItems").get<size_t>() || value.size() > schema.at("maxItems").get<size_t>()) return path + " has invalid length";
            for (size_t index = 0; index < value.size(); ++index)
            {
                const auto error = ValidateArguments(value[index], schema.at("items"), path + "[" + std::to_string(index) + "]");
                if (!error.empty()) return error;
            }
        }
        else if (type == "integer")
        {
            if ((schema.contains("minimum") && value < schema["minimum"]) || (schema.contains("maximum") && value > schema["maximum"])) return path + " is out of range";
        }
        else if (type == "string")
        {
            const auto &text = value.get_ref<const std::string &>();
            if ((schema.contains("minLength") && text.size() < schema["minLength"].get<size_t>()) ||
                (schema.contains("maxLength") && text.size() > schema["maxLength"].get<size_t>())) return path + " has invalid length";
            if (schema.contains("pattern") && !std::regex_match(text, std::regex(schema["pattern"].get<std::string>()))) return path + " has invalid format";
        }
        return {};
    }

    inline bool SupportsVersion(std::string_view version)
    {
        return version == "2025-03-26" || version == "2025-06-18";
    }

    struct Response
    {
        int status;
        json body;
    };

    template <typename Dispatch> Response Handle(std::string_view body, Dispatch &&dispatch)
    {
        json id = nullptr;
        const auto error = [&](int code, std::string_view message, int status = 200) -> Response
        {
            return {status, {{"jsonrpc", "2.0"}, {"id", id}, {"error", {{"code", code}, {"message", message}}}}};
        };
        const auto success = [&](json result) -> Response { return {200, {{"jsonrpc", "2.0"}, {"id", id}, {"result", std::move(result)}}}; };
        const json request = json::parse(body, nullptr, false);
        if (request.is_discarded()) return error(-32700, "Parse error", 400);
        if (!request.is_object() || request.value("jsonrpc", json()) != "2.0") return error(-32600, "Invalid Request", 400);
        if (request.contains("id"))
        {
            if (!request["id"].is_string() && !request["id"].is_number_integer()) return error(-32600, "Invalid request id", 400);
            id = request["id"];
        }
        if (!request.contains("method"))
        {
            if (!id.is_null() && (request.contains("result") != request.contains("error"))) return {202, nullptr};
            return error(-32600, "Invalid Request", 400);
        }
        if (!request["method"].is_string()) return error(-32600, "Invalid method", 400);
        if (request.contains("params") && !request["params"].is_object()) return error(-32602, "params must be an object", 400);
        if (id.is_null()) return {202, nullptr};
        const auto method = request["method"].get<std::string>();
        const json params = request.value("params", json::object());
        if (method == "initialize")
        {
            if (!params.contains("protocolVersion") || !params["protocolVersion"].is_string() ||
                !params.contains("capabilities") || !params["capabilities"].is_object() ||
                !params.contains("clientInfo") || !params["clientInfo"].is_object()) return error(-32602, "Invalid initialize parameters");
            const auto version = params["protocolVersion"].get<std::string>();
            return success({{"protocolVersion", SupportsVersion(version) ? version : "2025-06-18"},
                {"capabilities", {{"tools", json::object()}, {"resources", json::object()}}},
                {"serverInfo", {{"name", "LS_KTool"}, {"version", "1.0.0"}}},
                {"instructions", "Native Android debugger. Addresses and registers must be 0x-prefixed strings, never JSON numbers. Scan and saved values are strings. Select a target before memory operations. MCP and the Windows UI share the target, scans, viewer, saved addresses and breakpoints. No bridge configuration is needed."}});
        }
        if (method == "ping") return success(json::object());
        if (method == "tools/list")
        {
            if (params.contains("cursor")) return error(-32602, "This server does not use pagination cursors");
            json tools = json::array();
            for (const auto &tool : Tools()) tools.push_back(tool.descriptor);
            return success({{"tools", std::move(tools)}});
        }
        if (method == "resources/list") return success({{"resources", {{{"uri", "android://connection"}, {"name", "Android connection"}, {"mimeType", "application/json"}}}}});
        if (method == "resources/templates/list") return success({{"resourceTemplates", json::array()}});
        if (method == "resources/read")
        {
            if (!params.contains("uri") || !params["uri"].is_string()) return error(-32602, "uri is required");
            if (params["uri"] != "android://connection") return error(-32002, "Resource not found");
            return success({{"contents", {{{"uri", "android://connection"}, {"mimeType", "application/json"},
                {"text", json({{"service", "LS_KTool"}, {"transport", "streamable-http"}, {"path", "/mcp"}, {"native", true}}).dump()}}}}});
        }
        if (method != "tools/call") return error(-32601, "Method not found");
        if (!params.contains("name") || !params["name"].is_string()) return error(-32602, "Tool name is required");
        const auto &tools = Tools();
        const auto tool = std::find_if(tools.begin(), tools.end(), [&](const Tool &entry) { return entry.descriptor["name"] == params["name"]; });
        if (tool == tools.end()) return error(-32602, "Unknown tool");
        json arguments = params.value("arguments", json::object());
        const auto validation = ValidateArguments(arguments, tool->descriptor["inputSchema"]);
        if (!validation.empty()) return error(-32602, validation);
        const auto operation = tool->operation;
        if (operation == "scan.start" || operation == "scan.refine")
        {
            if (arguments["mode"] == "pointer") arguments["value_type"] = "i64";
        }
        if (operation == "saved.add")
        {
            if (arguments["value_kind"] == "pointer") arguments["value_type"] = "i64";
            else if (arguments["value_kind"] == "text") arguments["value_type"] = "i8";
        }
        if (operation == "signature.create")
        {
            arguments["range"] = arguments["range_size"];
            arguments.erase("range_size");
        }
        try
        {
            json result = dispatch(operation, arguments);
            if (result.value("ok", false) && operation == "memory.map")
            {
                result["data"].erase("regions");
                result["data"].erase("region_count");
            }
            if (result.value("ok", false) && (operation == "syscall.start" || operation == "cntvct.start"))
            {
                const auto log = dispatch(operation == "syscall.start" ? "syscall.read" : "cntvct.read", json::object());
                if (log.value("ok", false)) result["data"].update(log["data"]);
            }
            return success({{"content", {{{"type", "text"}, {"text", result.dump(-1, ' ', false, json::error_handler_t::replace)}}}},
                {"structuredContent", result}, {"isError", !result.value("ok", false)}});
        }
        catch (const std::exception &exception)
        {
            return success({{"content", {{{"type", "text"}, {"text", exception.what()}}}}, {"isError", true}});
        }
    }

    template <typename Dispatch> Response HandleHttp(std::string_view method, const json &headers, std::string_view body, Dispatch &&dispatch)
    {
        if (headers.contains("origin")) return {403, {{"error", "Browser origins are not accepted; use a native MCP client."}}};
        const auto version = headers.value("mcp-protocol-version", std::string("2025-03-26"));
        if (!SupportsVersion(version)) return {400, {{"error", "Unsupported MCP-Protocol-Version"}}};
        if (method == "OPTIONS") return {204, nullptr};
        if (method != "POST") return {405, {{"error", "Use POST; this stateless endpoint does not offer an SSE stream."}}};
        std::string contentType = headers.value("content-type", std::string{});
        std::transform(contentType.begin(), contentType.end(), contentType.begin(), [](unsigned char character) {
            return character >= 'A' && character <= 'Z' ? static_cast<char>(character + 'a' - 'A') : static_cast<char>(character);
        });
        const auto separator = contentType.find(';');
        const auto mediaType = contentType.substr(0, separator);
        if (mediaType.substr(0, mediaType.find_last_not_of(" \t") + 1) != "application/json") return {415, {{"error", "Content-Type must be application/json"}}};
        return Handle(body, std::forward<Dispatch>(dispatch));
    }
}