#pragma once
#include <algorithm>
#include <cstdint>
#include <memory>
#include <nlohmann/json.hpp>
#include <regex>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace Mcp
{
    class Adapter
    {
        using json = nlohmann::json;

        static json IntegerSchema(int64_t minimum, int64_t maximum)
        {
            return {{"type", "integer"}, {"minimum", minimum}, {"maximum", maximum}};
        }

        static json EnumSchema(json values)
        {
            return {{"type", "string"}, {"enum", std::move(values)}};
        }

        static json DefaultSchema(json schema, json value)
        {
            schema["default"] = std::move(value);
            return schema;
        }

        static json ObjectSchema(json properties, json required)
        {
            return {{"type", "object"}, {"properties", std::move(properties)}, {"required", std::move(required)}, {"additionalProperties", false}};
        }

        struct Tool
        {
            std::string_view operation;
            json descriptor;

            Tool(std::string_view name, std::string_view operation, std::string_view description,
                 json properties = json::object(), json required = json::array())
                : operation(operation), descriptor{{"name", name}, {"description", description}, {"inputSchema", ObjectSchema(std::move(properties), std::move(required))}}
            {
            }
        };

        static json ScanProperties(const json &valueType, const json &text, json modes)
        {
            return {{"mode", EnumSchema(std::move(modes))},
                    {"value_type", DefaultSchema(valueType, "i32")}, {"value", DefaultSchema(text, "")}, {"range_max", DefaultSchema(text, "")},
                    {"rounding", DefaultSchema(EnumSchema({"rounded", "truncated", "extreme", "exact"}), "rounded")},
                    {"alignment", {{"type", "integer"}, {"enum", {0, 1, 2, 4, 8}}, {"default", 0}}}};
        }

        std::vector<Tool> tools_;

    public:
        Adapter()
        {
            const json text = {{"type", "string"}};
            const json address = {{"type", "string"}, {"pattern", "^0[xX][0-9a-fA-F]{1,16}$"}, {"maxLength", 18}};
            const json valueType = EnumSchema({"i8", "i16", "i32", "i64", "f32", "f64"});
            const json viewFormat = EnumSchema({"hexadecimal", "hex", "i8", "i16", "i32", "i64", "f32", "f64", "disasm"});
            const json fileName = DefaultSchema(text, "Signature.txt");
            tools_ = {
                Tool("android_bridge_ping", "bridge.ping", "Check the native Android service."),
                Tool("android_target_set_pid", "target.select", "Select the target PID shared by MCP and the desktop debugger.", {{"pid", IntegerSchema(1, 2147483647)}}, {"pid"}),
                Tool("android_target_attach_package", "target.attach", "Find and select a running package.", {{"package_name", text}}, {"package_name"}),
                Tool("android_target_find_pid", "target.find", "Find a running package PID.", {{"package_name", text}}, {"package_name"}),
                Tool("android_target_current", "target.get", "Read the selected PID."),
                Tool("android_env_get_params", "env.read", "Read target TLS and PACGA parameters.", {{"thread_name", DefaultSchema(text, "")}}),
                Tool("android_memory_regions", "memory.map", "List target modules and their segments."),
                Tool("android_module_address", "module.resolve", "Resolve a module segment address; -1 selects BSS.",
                    {{"module_name", text}, {"segment_index", DefaultSchema(IntegerSchema(-1, 32767), 0)}, {"which", DefaultSchema(EnumSchema({"start", "end"}), "start")}}, {"module_name"}),
                Tool("android_memory_dump", "memory.dump", "Dump a module or a hexadecimal start-end range on Android.", {{"target", text}}, {"target"}),

                Tool("android_memory_scan_start", "scan.start", "Start a scan. Values are strings; pointer forces i64. alignment=1 scans every byte; 0 uses default alignment. Float rounding uses the decimal precision of value.", ScanProperties(valueType, text, {"unknown", "equal", "greater", "less", "range", "pointer", "string"}), {"mode"}),
                Tool("android_memory_scan_refine", "scan.refine", "Refine the existing scan; keep its value type and alignment.", ScanProperties(valueType, text, {"equal", "greater", "less", "increased", "decreased", "changed", "unchanged", "range", "pointer", "string"}), {"mode"}),
                Tool("android_memory_scan_results", "scan.results", "Read a page of scan results with exact hexadecimal addresses.",
                    {{"start", DefaultSchema(IntegerSchema(0, 9007199254740991LL), 0)}, {"count", DefaultSchema(IntegerSchema(1, 200), 100)},
                     {"value_type", DefaultSchema(EnumSchema({"i8", "i16", "i32", "i64", "f32", "f64", "string"}), "i32")}}),
                Tool("android_memory_scan_status", "scan.get", "Read scan progress and result count."),
                Tool("android_memory_scan_clear", "scan.clear", "Clear the scan results."),
                Tool("android_memory_read", "memory.read", "Read 1..1048576 bytes at a hexadecimal address.", {{"address", address}, {"size", IntegerSchema(1, 1048576)}}, {"address", "size"}),
                Tool("android_memory_write", "memory.write", "Write up to 1048576 bytes of hex digits, without a 0x prefix; whitespace is allowed.", {{"address", address}, {"data_hex", text}}, {"address", "data_hex"}),

                Tool("android_saved_list", "saved.list", "Read saved addresses, values and lock states."),
                Tool("android_saved_add", "saved.add", "Save an address. pointer forces i64; text forces i8 and uses text_length.",
                    {{"address", address}, {"value_type", DefaultSchema(valueType, "i32")}, {"value_kind", DefaultSchema(EnumSchema({"numeric", "pointer", "text"}), "numeric")},
                     {"text_length", DefaultSchema(IntegerSchema(1, 256), 64)}, {"note", DefaultSchema(text, "")}}, {"address"}),
                Tool("android_saved_remove", "saved.remove", "Remove a saved address and its lock.", {{"address", address}}, {"address"}),
                Tool("android_saved_write", "saved.write", "Write a saved address using its saved type.", {{"address", address}, {"value", text}}, {"address", "value"}),
                Tool("android_saved_set_note", "saved.note.set", "Set or clear a saved address note.", {{"address", address}, {"note", text}}, {"address", "note"}),
                Tool("android_saved_set_locked", "saved.lock.set", "Set a saved lock. Empty value locks the current value; unlocking ignores value.",
                    {{"address", address}, {"locked", {{"type", "boolean"}}}, {"value", DefaultSchema(text, "")}}, {"address", "locked"}),
                Tool("android_saved_offset", "saved.offset", "Offset all saved addresses by signed hexadecimal bytes.", {{"offset", text}}, {"offset"}),
                Tool("android_saved_clear", "saved.clear", "Clear saved addresses and locks."),

                Tool("android_pointer_status", "pointer.get", "Read pointer task state and result count."),
                Tool("android_pointer_scan", "pointer.scan", "Start a pointer scan. manual requires manual_base; array requires array_base and array_count.",
                    {{"target", address}, {"depth", IntegerSchema(1, 16)}, {"max_offset", IntegerSchema(1, 2147483647)},
                     {"mode", DefaultSchema(EnumSchema({"module", "manual", "array"}), "module")}, {"manual_base", address}, {"array_base", address},
                     {"array_count", IntegerSchema(1, 1000000)}, {"module_filter", DefaultSchema(text, "")}}, {"target", "depth", "max_offset"}),
                Tool("android_pointer_merge", "pointer.merge", "Intersect saved pointer bin files by offset structure."),
                Tool("android_pointer_export", "pointer.export", "Export merged pointer chains as text on Android."),
                Tool("android_memory_view_open", "viewer.open", "Open an address and read a 100-byte snapshot.", {{"address", address}, {"view_format", DefaultSchema(viewFormat, "hexadecimal")}}, {"address"}),
                Tool("android_memory_view_offset", "viewer.seek", "Move the viewer by a signed hexadecimal byte offset.", {{"offset", text}}, {"offset"}),
                Tool("android_memory_view_set_format", "viewer.format", "Change format and refresh the viewer.", {{"view_format", viewFormat}}, {"view_format"}),
                Tool("android_memory_view_read", "viewer.refresh", "Refresh the current viewer snapshot and disassembly."),

                Tool("android_breakpoint_get", "breakpoint.get", "Read breakpoints and registers as exact hexadecimal strings."),
                Tool("android_breakpoint_set", "breakpoint.set", "Set 1..16 breakpoints with nonzero hexadecimal addresses. DPTBP only monitors main-thread execution; addresses must be 4-byte aligned.",
                    {{"mode", EnumSchema({"hwbp", "ptebp", "dptbp", "stepbp"})}, {"points", {{"type", "array"}, {"items", ObjectSchema({{"address", address}, {"bp_type", EnumSchema({"read", "write", "read_write", "execute"})},
                    {"bp_scope", EnumSchema({"main", "other", "all"})}, {"length", IntegerSchema(1, 8)}}, {"address", "bp_type", "bp_scope", "length"})}, {"minItems", 1}, {"maxItems", 16}}}}, {"mode", "points"}),
                Tool("android_breakpoint_clear", "breakpoint.clear", "Clear the active breakpoint mode."),
                Tool("android_breakpoint_record_update", "breakpoint_record.update", "Patch a register, mask or op.<register>; value is a 64/128-bit hexadecimal string.",
                    {{"index", IntegerSchema(0, 2147483647)}, {"field", {{"type", "string"}, {"maxLength", 32}, {"pattern", "^(?:(?:op\\.|mask\\.)?(?:pc|hit_count|lr|sp|orig_x0|syscallno|pstate|fpsr|fpcr|x(?:[0-9]|[12][0-9])|q(?:[0-9]|[12][0-9]|3[01]))|mask(?:[0-9]|1[0-7])|mask\\[(?:[0-9]|1[0-7])\\])$"}}},
                     {"value", {{"type", "string"}, {"maxLength", 34}, {"pattern", "^0[xX][0-9a-fA-F]{1,32}$"}}}}, {"index", "field", "value"}),
                Tool("android_syscall_start", "syscall.start", "Start syscall monitoring and read the available log."),
                Tool("android_syscall_stop", "syscall.stop", "Stop syscall monitoring."),
                Tool("android_syscall_log", "syscall.read", "Read available syscall log lines."),
                Tool("android_cntvct_start", "cntvct.start", "Start CNTVCT_EL0 monitoring and read the available log."),
                Tool("android_cntvct_stop", "cntvct.stop", "Stop CNTVCT_EL0 monitoring."),
                Tool("android_cntvct_log", "cntvct.read", "Read available CNTVCT log lines."),
                Tool("android_signature_scan_address", "signature.create", "Generate a signature using range_size bytes before and after an address.",
                    {{"address", address}, {"range_size", IntegerSchema(1, 1200)}, {"file_name", fileName}}, {"address", "range_size"}),
                Tool("android_signature_scan_file", "signature.scan", "Scan an Android-side signature file.", {{"file_name", fileName}}),
                Tool("android_signature_scan_pattern", "signature.match", "Scan a byte pattern; ? and ?? are wildcards.",
                    {{"pattern", {{"type", "string"}, {"minLength", 1}}}, {"range_offset", DefaultSchema(IntegerSchema(-2147483648LL, 2147483647), 0)}}, {"pattern"}),
                Tool("android_signature_filter", "signature.filter", "Filter changed bytes in an Android-side signature file.", {{"address", address}, {"file_name", fileName}}, {"address"}),
            };
        }

    private:
        static std::string ValidateArguments(json &value, const json &schema, const std::string &path = "arguments")
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
                    if (!value.contains(key))
                    {
                        if (!property.contains("default")) continue;
                        value[key] = property["default"];
                    }
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

        static bool SupportsVersion(std::string_view version)
        {
            return version == "2025-03-26" || version == "2025-06-18";
        }

    public:
        Adapter(const Adapter &) = delete;
        Adapter &operator=(const Adapter &) = delete;

        struct Response
        {
            int status;
            json body;
        };

        template <typename Dispatch> Response HandleHttp(std::string_view httpMethod, const json &headers, std::string_view body, Dispatch &&dispatch) const
        {
            if (headers.contains("origin")) return {403, {{"error", "Browser origins are not accepted; use a native MCP client."}}};
            if (!SupportsVersion(headers.value("mcp-protocol-version", std::string("2025-03-26")))) return {400, {{"error", "Unsupported MCP-Protocol-Version"}}};
            if (httpMethod == "OPTIONS") return {204, nullptr};
            if (httpMethod != "POST") return {405, {{"error", "Use POST; this stateless endpoint does not offer an SSE stream."}}};
            std::string contentType = headers.value("content-type", std::string{});
            std::transform(contentType.begin(), contentType.end(), contentType.begin(), [](unsigned char character) {
                return character >= 'A' && character <= 'Z' ? static_cast<char>(character + 'a' - 'A') : static_cast<char>(character);
            });
            const auto mediaType = std::string_view(contentType).substr(0, contentType.find(';'));
            if (mediaType.substr(0, mediaType.find_last_not_of(" \t") + 1) != "application/json") return {415, {{"error", "Content-Type must be application/json"}}};
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
            const auto &method = request["method"].get_ref<const std::string &>();
            const json emptyParams = json::object();
            const auto paramsIt = request.find("params");
            const json &params = paramsIt == request.end() ? emptyParams : *paramsIt;
            if (method == "initialize")
            {
                if (!params.contains("protocolVersion") || !params["protocolVersion"].is_string() ||
                    !params.contains("capabilities") || !params["capabilities"].is_object() ||
                    !params.contains("clientInfo") || !params["clientInfo"].is_object()) return error(-32602, "Invalid initialize parameters");
                const auto &version = params["protocolVersion"].get_ref<const std::string &>();
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
                for (const auto &tool : tools_) tools.push_back(tool.descriptor);
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
            const auto tool = std::find_if(tools_.begin(), tools_.end(), [&](const Tool &entry) { return entry.descriptor["name"] == params["name"]; });
            if (tool == tools_.end()) return error(-32602, "Unknown tool");
            json arguments = params.value("arguments", json::object());
            const auto validation = ValidateArguments(arguments, tool->descriptor["inputSchema"]);
            if (!validation.empty()) return error(-32602, validation);
            const auto operation = tool->operation;
            if ((operation == "scan.start" || operation == "scan.refine") && arguments["mode"] == "pointer") arguments["value_type"] = "i64";
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
                if (result.value("ok", false))
                {
                    if (operation == "memory.map")
                    {
                        result["data"].erase("regions");
                        result["data"].erase("region_count");
                    }
                    else if (operation == "syscall.start" || operation == "cntvct.start")
                    {
                        const auto log = dispatch(operation == "syscall.start" ? "syscall.read" : "cntvct.read", json::object());
                        if (log.value("ok", false)) result["data"].update(log["data"]);
                    }
                }
                return success({{"content", {{{"type", "text"}, {"text", result.dump(-1, ' ', false, json::error_handler_t::replace)}}}},
                    {"structuredContent", result}, {"isError", !result.value("ok", false)}});
            }
            catch (const std::exception &exception)
            {
                return success({{"content", {{{"type", "text"}, {"text", exception.what()}}}}, {"isError", true}});
            }
            catch (...)
            {
                return success({{"content", {{{"type", "text"}, {"text", "Unknown operation error"}}}}, {"isError", true}});
            }
        }
    };

    inline std::unique_ptr<Adapter> gMcp = nullptr;
}