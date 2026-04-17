#pragma once
#include "json.hpp"
#include "MemoryTool.h"

// ============================================================================
// TCP 服务器模块
// ============================================================================

namespace
{
    using nlohmann::json;

    constexpr std::uint16_t kServerPort = 9494;
    constexpr int kListenBacklog = 4;
    std::atomic_bool gRunning{true};
    std::atomic_uint64_t gClientSessionSeed{1};
    int gServerFd = -1;
    LockManager gLockManager;
    std::mutex gDriverCommandMutex;

    struct ClientSession
    {
        std::uint64_t sessionId;
        MemScanner memScanner;
        MemViewer memViewer;
        PointerManager pointerManager;

        explicit ClientSession(std::uint64_t id)
            : sessionId(id) {}
    };

    // 打印系统错误信息
    void printErrno(std::string_view action)
    {
        std::println(stderr, "{}，错误码：{}", action, errno);
    }

    // 去除字符串末尾换行符
    void trimLineEnding(std::string &text)
    {
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r'))
        {
            text.pop_back();
        }
    }

    // 清理文本中的换行字符
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

    // 按空白切分命令参数

    // 解析无符号64位整数
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

    // 解析整数参数
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

    // 解析浮点数参数
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

    template <typename T>
    std::optional<T> readScalarValue(std::uint64_t address)
    {
        T value{};
        if (dr.Read(address, &value, sizeof(T)) != static_cast<int>(sizeof(T)))
            return std::nullopt;
        return value;
    }

    template <typename T>
    bool writeScalarValue(std::uint64_t address, T value)
    {
        return dr.Write<T>(address, value) == static_cast<int>(sizeof(T));
    }

    // 解析有符号64位整数
    std::optional<std::int64_t> parseInt64(std::string_view text)
    {
        if (text.empty())
        {
            return std::nullopt;
        }

        std::string temp(text);
        char *end = nullptr;
        errno = 0;
        const long long value = std::strtoll(temp.c_str(), &end, 0);
        if (errno != 0 || end == temp.c_str() || *end != '\0')
        {
            return std::nullopt;
        }
        return static_cast<std::int64_t>(value);
    }

    // 将字符串转换为小写ASCII
    std::string toLowerAscii(std::string_view input)
    {
        std::string out;
        out.reserve(input.size());
        for (const unsigned char ch : input)
        {
            out.push_back(static_cast<char>(std::tolower(ch)));
        }
        return out;
    }

    // 解析数据类型标记
    std::optional<Types::DataType> parseDataTypeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "i8" || t == "int8")
            return Types::DataType::I8;
        if (t == "i16" || t == "int16")
            return Types::DataType::I16;
        if (t == "i32" || t == "int32")
            return Types::DataType::I32;
        if (t == "i64" || t == "int64")
            return Types::DataType::I64;
        if (t == "f32" || t == "float")
            return Types::DataType::Float;
        if (t == "f64" || t == "double")
            return Types::DataType::Double;
        return std::nullopt;
    }

    // 解析扫描模式标记
    std::optional<Types::FuzzyMode> parseFuzzyModeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "unknown")
            return Types::FuzzyMode::Unknown;
        if (t == "eq" || t == "equal")
            return Types::FuzzyMode::Equal;
        if (t == "gt" || t == "greater")
            return Types::FuzzyMode::Greater;
        if (t == "lt" || t == "less")
            return Types::FuzzyMode::Less;
        if (t == "inc" || t == "increased")
            return Types::FuzzyMode::Increased;
        if (t == "dec" || t == "decreased")
            return Types::FuzzyMode::Decreased;
        if (t == "chg" || t == "changed")
            return Types::FuzzyMode::Changed;
        if (t == "unchg" || t == "unchanged")
            return Types::FuzzyMode::Unchanged;
        if (t == "range")
            return Types::FuzzyMode::Range;
        if (t == "ptr" || t == "pointer")
            return Types::FuzzyMode::Pointer;
        if (t == "str" || t == "string")
            return Types::FuzzyMode::String;
        return std::nullopt;
    }

    // 解析内存浏览显示格式
    std::optional<Types::ViewFormat> parseViewFormatToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "hex")
            return Types::ViewFormat::Hex;
        if (t == "hex64")
            return Types::ViewFormat::Hex64;
        if (t == "i8" || t == "int8")
            return Types::ViewFormat::I8;
        if (t == "i16" || t == "int16")
            return Types::ViewFormat::I16;
        if (t == "i32" || t == "int32")
            return Types::ViewFormat::I32;
        if (t == "i64" || t == "int64")
            return Types::ViewFormat::I64;
        if (t == "f32" || t == "float")
            return Types::ViewFormat::Float;
        if (t == "f64" || t == "double")
            return Types::ViewFormat::Double;
        if (t == "disasm")
            return Types::ViewFormat::Disasm;
        return std::nullopt;
    }

    // 解析硬件断点类型
    std::optional<decltype(dr)::bp_type> parseBpTypeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "0" || t == "read" || t == "r" || t == "bp_read")
            return decltype(dr)::BP_READ;
        if (t == "1" || t == "write" || t == "w" || t == "bp_write")
            return decltype(dr)::BP_WRITE;
        if (t == "2" || t == "read_write" || t == "rw" || t == "bp_read_write")
            return decltype(dr)::BP_READ_WRITE;
        if (t == "3" || t == "execute" || t == "x" || t == "exec" || t == "bp_execute")
            return decltype(dr)::BP_EXECUTE;
        return std::nullopt;
    }

    // 解析硬件断点作用线程范围
    std::optional<decltype(dr)::bp_scope> parseBpScopeToken(std::string_view token)
    {
        const std::string t = toLowerAscii(token);
        if (t == "0" || t == "main" || t == "main_thread")
            return decltype(dr)::SCOPE_MAIN_THREAD;
        if (t == "1" || t == "other" || t == "other_threads")
            return decltype(dr)::SCOPE_OTHER_THREADS;
        if (t == "2" || t == "all" || t == "all_threads")
            return decltype(dr)::SCOPE_ALL_THREADS;
        return std::nullopt;
    }

    // 将显示格式枚举转换为标记
    std::string_view viewFormatToToken(Types::ViewFormat format)
    {
        switch (format)
        {
        case Types::ViewFormat::Hex:
            return "hex";
        case Types::ViewFormat::Hex64:
            return "hex64";
        case Types::ViewFormat::I8:
            return "i8";
        case Types::ViewFormat::I16:
            return "i16";
        case Types::ViewFormat::I32:
            return "i32";
        case Types::ViewFormat::I64:
            return "i64";
        case Types::ViewFormat::Float:
            return "f32";
        case Types::ViewFormat::Double:
            return "f64";
        case Types::ViewFormat::Disasm:
            return "disasm";
        default:
            return "hex";
        }
    }

    // 将硬件断点类型转换为文本标记
    std::string_view bpTypeToToken(decltype(dr)::bp_type type)
    {
        switch (type)
        {
        case decltype(dr)::BP_READ:
            return "read";
        case decltype(dr)::BP_WRITE:
            return "write";
        case decltype(dr)::BP_READ_WRITE:
            return "read_write";
        case decltype(dr)::BP_EXECUTE:
            return "execute";
        default:
            return "unknown";
        }
    }

    // 将硬件断点线程范围转换为文本标记
    std::string_view bpScopeToToken(decltype(dr)::bp_scope scope)
    {
        switch (scope)
        {
        case decltype(dr)::SCOPE_MAIN_THREAD:
            return "main";
        case decltype(dr)::SCOPE_OTHER_THREADS:
            return "other";
        case decltype(dr)::SCOPE_ALL_THREADS:
            return "all";
        default:
            return "unknown";
        }
    }

    // 按模板类型解析扫描输入值。
    template <typename T>
    std::optional<T> parseScanValueToken(std::string_view token)
    {
        if constexpr (std::is_same_v<T, float> || std::is_same_v<T, double>)
        {
            const auto parsed = parseDouble(token);
            if (!parsed.has_value())
            {
                return std::nullopt;
            }
            return static_cast<T>(*parsed);
        }
        else
        {
            const auto parsed = parseInt64(token);
            if (!parsed.has_value())
            {
                return std::nullopt;
            }
            return static_cast<T>(*parsed);
        }
    }

    // 将字节数组编码为十六进制字符串
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

    // 解析十六进制字节流
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

    // 合并指定起点后的参数为字符串

    // 生成成功响应文本

    // 生成失败响应文本

    // 构建内存信息JSON响应
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

    // 构建内存浏览快照JSON
    json buildViewerSnapshotJson(const MemViewer &viewer)
    {
        json root;
        root["visible"] = viewer.isVisible();
        root["read_success"] = viewer.readSuccess();
        root["base"] = static_cast<std::uint64_t>(viewer.base());
        root["base_hex"] = std::format("0x{:X}", static_cast<std::uint64_t>(viewer.base()));
        root["format"] = viewFormatToToken(viewer.format());

        const auto &buffer = viewer.buffer();
        root["byte_count"] = buffer.size();
        root["data_hex"] = bytesToHex(buffer.data(), buffer.size());

        root["disasm_scroll_idx"] = viewer.disasmScrollIdx();
        root["disasm"] = json::array();
        for (const auto &line : viewer.getDisasm())
        {
            json item;
            item["valid"] = line.valid;
            item["address"] = line.address;
            item["address_hex"] = std::format("0x{:X}", line.address);
            item["size"] = line.size;
            item["bytes_hex"] = bytesToHex(line.bytes, line.size);
            item["mnemonic"] = sanitizeLine(line.mnemonic);
            item["op_str"] = sanitizeLine(line.op_str);
            root["disasm"].push_back(std::move(item));
        }

        return root;
    }

    // 构建硬件断点信息JSON
    json buildHwbpInfoJson(const auto &info)
    {
        json root;
        int recordCount = info.record_count;
        if (recordCount < 0)
        {
            recordCount = 0;
        }
        else if (recordCount > 0x100)
        {
            recordCount = 0x100;
        }

        root["num_brps"] = info.num_brps;
        root["num_wrps"] = info.num_wrps;
        root["hit_addr"] = info.hit_addr;
        root["hit_addr_hex"] = std::format("0x{:X}", static_cast<std::uint64_t>(info.hit_addr));
        root["record_count"] = recordCount;
        root["records"] = json::array();

        for (int i = 0; i < recordCount; ++i)
        {
            auto &rec = const_cast<Driver::hwbp_record &>(info.records[i]);
            json item;
            item["index"] = i;
            MemUtils::HwbpRequestAll(rec);
            const auto pc = MemUtils::HwbpGetRegisterValue(rec, Driver::IDX_PC);
            const auto hitCount = MemUtils::HwbpGetRegisterValue(rec, Driver::IDX_HIT_COUNT);
            const auto lr = MemUtils::HwbpGetRegisterValue(rec, Driver::IDX_LR);
            const auto sp = MemUtils::HwbpGetRegisterValue(rec, Driver::IDX_SP);
            const auto origX0 = MemUtils::HwbpGetRegisterValue(rec, Driver::IDX_ORIG_X0);
            const auto syscallNo = MemUtils::HwbpGetRegisterValue(rec, Driver::IDX_SYSCALLNO);
            const auto pstate = MemUtils::HwbpGetRegisterValue(rec, Driver::IDX_PSTATE);
            const auto fpsr = MemUtils::HwbpGetRegisterValue(rec, Driver::IDX_FPSR);
            const auto fpcr = MemUtils::HwbpGetRegisterValue(rec, Driver::IDX_FPCR);
            item["mask"] = json::array();
            for (int m = 0; m < 18; ++m)
            {
                item["mask"].push_back(rec.mask[m]);
            }
            item["ops"] = json::object();
            item["op_values"] = json::object();
            int readOps = 0;
            int writeOps = 0;
            for (int reg = Driver::IDX_PC; reg < Driver::MAX_REG_COUNT; ++reg)
            {
                const auto op = static_cast<std::uint8_t>(HWBP_GET_MASK(&rec, reg));
                const auto name = MemUtils::HwbpRegName(reg);
                item["ops"][name] = MemUtils::HwbpOpName(op);
                item["op_values"][name] = op;
                if (op == HWBP_OP_READ)
                {
                    ++readOps;
                }
                else if (op == HWBP_OP_WRITE)
                {
                    ++writeOps;
                }
            }
            item["read_op_count"] = readOps;
            item["write_op_count"] = writeOps;
            item["rw"] = writeOps > 0 ? "write" : (readOps > 0 ? "read" : "none");
            item["pc"] = pc;
            item["pc_hex"] = std::format("0x{:X}", static_cast<std::uint64_t>(pc));
            item["hit_count"] = hitCount;
            item["lr"] = lr;
            item["sp"] = sp;
            item["orig_x0"] = origX0;
            item["syscallno"] = syscallNo;
            item["pstate"] = pstate;
            item["regs"] = json::array();
            for (int reg = 0; reg < 30; ++reg)
            {
                item["regs"].push_back(MemUtils::HwbpGetXField(rec, reg));
            }
            item["vregs"] = json::array();
            item["qregs"] = json::array();
            for (int reg = 0; reg < 32; ++reg)
            {
                const auto qreg = MemUtils::HwbpGetQField(rec, reg);
                json qitem = {{"lo", static_cast<std::uint64_t>(qreg)},
                              {"hi", static_cast<std::uint64_t>(qreg >> 64)}};
                item["vregs"].push_back(qitem);
                item["qregs"].push_back(std::move(qitem));
            }
            item["fpsr"] = fpsr;
            item["fpcr"] = fpcr;
            root["records"].push_back(std::move(item));
        }

        return root;
    }

    // 构建特征码扫描结果JSON
    json buildSignatureMatchesJson(const std::vector<uintptr_t> &matches, std::int64_t range, std::string_view pattern)
    {
        constexpr std::size_t kMaxReturnedMatches = 4096;
        const std::size_t returnedCount = std::min(matches.size(), kMaxReturnedMatches);

        json root;
        root["count"] = matches.size();
        root["returned_count"] = returnedCount;
        root["truncated"] = (matches.size() > returnedCount);
        root["range"] = range;
        root["pattern"] = std::string(pattern);
        root["matches"] = json::array();

        for (std::size_t i = 0; i < returnedCount; ++i)
        {
            const auto addr = static_cast<std::uint64_t>(matches[i]);
            root["matches"].push_back({
                {"addr", addr},
                {"addr_hex", std::format("0x{:X}", addr)},
            });
        }

        return root;
    }

    // 发送完整响应数据
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

    // 内部文本命令派发

    // 将文本协议响应包装为统一 JSON 响应。

    json makeProtocolError(std::string_view message)
    {
        return json{
            {"ok", false},
            {"error", std::string(message)},
        };
    }

    std::optional<std::string> getRequiredStringParam(const json &params, std::string_view key)
    {
        const auto it = params.find(std::string(key));
        if (it == params.end())
        {
            return std::nullopt;
        }

        if (it->is_string())
        {
            return it->get<std::string>();
        }

        if (it->is_boolean() || it->is_number_integer() || it->is_number_unsigned() || it->is_number_float())
        {
            return it->dump();
        }

        return std::nullopt;
    }

    std::optional<std::string> getOptionalStringParam(const json &params, std::string_view key)
    {
        const auto it = params.find(std::string(key));
        if (it == params.end() || it->is_null())
        {
            return std::nullopt;
        }
        return getRequiredStringParam(params, key);
    }


    const json &bridgeDescribePayload()
    {
        static const json payload = []()
        {
            json out;
            out["protocol"] = "native-tcp-bridge";
            out["request_shapes"] = json::array({
                {
                    {"name", "structured"},
                    {"fields", json::array({"operation", "params"})},
                },
            });
            out["operations"] = json::array({
                "bridge.describe",
                "bridge.ping",
                "target.pid.get",
                "target.pid.set",
                "target.pid.current",
                "target.attach.package",
                "memory.info.full",
                "module.resolve",
                "scan.start",
                "scan.refine",
                "scan.status",
                "scan.clear",
                "scan.page",
                "viewer.open",
                "viewer.move",
                "viewer.offset",
                "viewer.set_format",
                "viewer.snapshot",
                "pointer.status",
                "pointer.scan",
                "pointer.merge",
                "pointer.export",
                "breakpoint.info",
                "breakpoint.set",
                "breakpoint.clear",
                "breakpoint.record.remove",
                "breakpoint.record.update",
                "breakpoint.record.set_float",
                "signature.scan_address",
                "signature.scan_file",
                "signature.scan_pattern",
                "signature.filter",
                "lock.set",
                "lock.unset",
                "lock.status",
                "lock.clear",
                "memory.read_block",
                "memory.read_value",
                "memory.write_block",
            });
            return out;
        }();
        return payload;
    }

    json dispatchStructuredOperationDirect(const std::shared_ptr<ClientSession> &session, std::string_view operation, const json &params)
    {
        const std::string op(operation);

        auto finalize = [&](json out) -> json
        {
            out["operation"] = op;
            out["session_id"] = session->sessionId;
            return out;
        };

        auto fail = [&](std::string_view message) -> json
        {
            return finalize(makeProtocolError(message));
        };

        auto ok = [&]() -> json
        {
            return finalize(json{{"ok", true}});
        };

        auto okData = [&](json data) -> json
        {
            json out = ok();
            out["data"] = std::move(data);
            return out;
        };

        auto requiredString = [&](std::string_view key, std::string_view desc) -> std::variant<std::string, json>
        {
            const auto value = getRequiredStringParam(params, key);
            if (!value.has_value() || value->empty())
                return std::variant<std::string, json>{std::in_place_index<1>, fail(std::format("operation={} 缺少参数 {}", op, desc))};
            return std::variant<std::string, json>{std::in_place_index<0>, *value};
        };

        auto optionalString = [&](std::string_view key) -> std::string
        {
            const auto value = getOptionalStringParam(params, key);
            return value.has_value() ? *value : "";
        };

        auto requiredUInt64 = [&](std::string_view key, std::string_view desc) -> std::variant<std::uint64_t, json>
        {
            const auto text = requiredString(key, desc);
            if (std::holds_alternative<json>(text))
                return std::variant<std::uint64_t, json>{std::in_place_index<1>, std::get<json>(text)};
            const auto parsed = parseUInt64(std::get<std::string>(text));
            if (!parsed.has_value())
                return std::variant<std::uint64_t, json>{std::in_place_index<1>, fail(std::format("operation={} 参数 {} 无效", op, desc))};
            return std::variant<std::uint64_t, json>{std::in_place_index<0>, *parsed};
        };

        auto requiredInt = [&](std::string_view key, std::string_view desc) -> std::variant<int, json>
        {
            const auto text = requiredString(key, desc);
            if (std::holds_alternative<json>(text))
                return std::variant<int, json>{std::in_place_index<1>, std::get<json>(text)};
            const auto parsed = parseInt(std::get<std::string>(text));
            if (!parsed.has_value())
                return std::variant<int, json>{std::in_place_index<1>, fail(std::format("operation={} 参数 {} 无效", op, desc))};
            return std::variant<int, json>{std::in_place_index<0>, *parsed};
        };

        auto requiredInt64 = [&](std::string_view key, std::string_view desc) -> std::variant<std::int64_t, json>
        {
            const auto text = requiredString(key, desc);
            if (std::holds_alternative<json>(text))
                return std::variant<std::int64_t, json>{std::in_place_index<1>, std::get<json>(text)};
            const auto parsed = parseInt64(std::get<std::string>(text));
            if (!parsed.has_value())
                return std::variant<std::int64_t, json>{std::in_place_index<1>, fail(std::format("operation={} 参数 {} 无效", op, desc))};
            return std::variant<std::int64_t, json>{std::in_place_index<0>, *parsed};
        };

        auto requiredDouble = [&](std::string_view key, std::string_view desc) -> std::variant<double, json>
        {
            const auto text = requiredString(key, desc);
            if (std::holds_alternative<json>(text))
                return std::variant<double, json>{std::in_place_index<1>, std::get<json>(text)};
            const auto parsed = parseDouble(std::get<std::string>(text));
            if (!parsed.has_value())
                return std::variant<double, json>{std::in_place_index<1>, fail(std::format("operation={} 参数 {} 无效", op, desc))};
            return std::variant<double, json>{std::in_place_index<0>, *parsed};
        };

        auto scannerStateJson = [&]() -> json
        {
            return {
                {"scanning", session->memScanner.isScanning()},
                {"progress", session->memScanner.progress()},
                {"count", session->memScanner.count()},
            };
        };

        auto pointerStateJson = [&]() -> json
        {
            return {
                {"scanning", session->pointerManager.isScanning()},
                {"progress", session->pointerManager.scanProgress()},
                {"count", session->pointerManager.count()},
            };
        };

        if (op == "bridge.describe")
            return okData(bridgeDescribePayload());

        if (op == "bridge.ping")
            return ok();

        std::lock_guard<std::mutex> driverLock(gDriverCommandMutex);

        if (op == "target.pid.get")
        {
            const auto package = requiredString("package_name", "package_name");
            if (std::holds_alternative<json>(package))
                return std::get<json>(package);
            const int pid = dr.GetPid(std::get<std::string>(package));
            if (pid <= 0)
                return fail("未找到进程");
            return okData({{"pid", pid}});
        }

        if (op == "target.pid.set")
        {
            const auto pid = requiredInt("pid", "pid");
            if (std::holds_alternative<json>(pid))
                return std::get<json>(pid);
            if (std::get<int>(pid) <= 0)
                return fail("pid 参数无效");
            dr.SetGlobalPid(std::get<int>(pid));
            return okData({{"pid", dr.GetGlobalPid()}});
        }

        if (op == "target.pid.current")
            return okData({{"pid", dr.GetGlobalPid()}});

        if (op == "target.attach.package")
        {
            const auto package = requiredString("package_name", "package_name");
            if (std::holds_alternative<json>(package))
                return std::get<json>(package);
            const int pid = dr.GetPid(std::get<std::string>(package));
            if (pid <= 0)
                return fail("未找到进程");
            dr.SetGlobalPid(pid);
            return okData({{"pid", pid}});
        }

        if (op == "memory.info.full")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
                return fail(std::format("刷新失败 status={}", status));
            return okData(buildMemoryInfoJson(status, dr.GetMemoryInfoRef()));
        }

        if (op == "module.resolve")
        {
            const auto moduleName = requiredString("module_name", "module_name");
            const auto segmentIndex = requiredInt("segment_index", "segment_index");
            const auto which = requiredString("which", "which");
            if (std::holds_alternative<json>(moduleName))
                return std::get<json>(moduleName);
            if (std::holds_alternative<json>(segmentIndex))
                return std::get<json>(segmentIndex);
            if (std::holds_alternative<json>(which))
                return std::get<json>(which);

            const std::string whichValue = toLowerAscii(std::get<std::string>(which));
            const bool isStart = (whichValue == "start");
            const bool isEnd = (whichValue == "end");
            if (!isStart && !isEnd)
                return fail("which 必须是 start 或 end");

            std::uint64_t address = 0;
            if (!dr.GetModuleAddress(std::get<std::string>(moduleName), static_cast<short>(std::get<int>(segmentIndex)), &address, isStart))
                return fail("未找到目标模块或段");
            return okData({{"address", address}, {"address_hex", std::format("0x{:X}", address)}});
        }

        if (op == "scan.start" || op == "scan.refine")
        {
            const auto type = requiredString("value_type", "value_type");
            const auto mode = requiredString("mode", "mode");
            if (std::holds_alternative<json>(type))
                return std::get<json>(type);
            if (std::holds_alternative<json>(mode))
                return std::get<json>(mode);

            const auto dataType = parseDataTypeToken(std::get<std::string>(type));
            const auto fuzzyMode = parseFuzzyModeToken(std::get<std::string>(mode));
            if (!dataType.has_value())
                return fail("value_type 无效，支持: i8/i16/i32/i64/f32/f64");
            if (!fuzzyMode.has_value())
                return fail("mode 无效，支持: unknown/eq/gt/lt/inc/dec/changed/unchanged/range/pointer/string");

            const int pid = dr.GetGlobalPid();
            if (pid <= 0)
                return fail("全局PID未设置，请先执行 target.pid.set 或 target.attach.package");

            const bool isFirst = (op == "scan.start");
            const std::string valueToken = optionalString("value");
            if (*fuzzyMode == Types::FuzzyMode::String)
            {
                if (valueToken.empty())
                    return fail("string 模式需要 value 参数");
                session->memScanner.scanString(pid, valueToken, isFirst);
                return okData(scannerStateJson());
            }

            const bool needValue = (*fuzzyMode != Types::FuzzyMode::Unknown);
            if (needValue && valueToken.empty())
                return fail("当前模式需要 value 参数");

            double rangeMax = 0.0;
            if (*fuzzyMode == Types::FuzzyMode::Range)
            {
                const auto range = requiredDouble("range_max", "range_max");
                if (std::holds_alternative<json>(range))
                    return std::get<json>(range);
                if (std::get<double>(range) < 0.0)
                    return fail("range_max 无效");
                rangeMax = std::get<double>(range);
            }
            else
            {
                const std::string rangeToken = optionalString("range_max");
                if (!rangeToken.empty())
                {
                    const auto parsedRange = parseDouble(rangeToken);
                    if (parsedRange.has_value() && *parsedRange >= 0.0)
                        rangeMax = *parsedRange;
                }
            }

            return MemUtils::DispatchType(*dataType, [&]<typename T>() -> json
                                          {
                T target{};
                if (needValue)
                {
                    const auto parsedValue = parseScanValueToken<T>(valueToken);
                    if (!parsedValue.has_value())
                        return fail("value 参数无效");
                    target = *parsedValue;
                }
                session->memScanner.scan<T>(pid, target, *fuzzyMode, isFirst, rangeMax);
                return okData(scannerStateJson()); });
        }

        if (op == "scan.status")
            return okData(scannerStateJson());

        if (op == "scan.clear")
        {
            session->memScanner.clear();
            return okData(scannerStateJson());
        }

        if (op == "scan.page")
        {
            const auto start = requiredUInt64("start", "start");
            const auto count = requiredUInt64("count", "count");
            const auto type = requiredString("value_type", "value_type");
            if (std::holds_alternative<json>(start))
                return std::get<json>(start);
            if (std::holds_alternative<json>(count))
                return std::get<json>(count);
            if (std::holds_alternative<json>(type))
                return std::get<json>(type);
            if (std::get<std::uint64_t>(count) == 0 || std::get<std::uint64_t>(count) > 2000)
                return fail("count 范围 1-2000");

            const std::string typeToken = toLowerAscii(std::get<std::string>(type));
            const bool stringType = (typeToken == "str" || typeToken == "string" || typeToken == "text");
            const auto dataType = parseDataTypeToken(std::get<std::string>(type));
            if (!stringType && !dataType.has_value())
                return fail("value_type 参数无效");

            const auto page = session->memScanner.getPage(static_cast<size_t>(std::get<std::uint64_t>(start)), static_cast<size_t>(std::get<std::uint64_t>(count)));
            json payload;
            payload["start"] = std::get<std::uint64_t>(start);
            payload["request_count"] = std::get<std::uint64_t>(count);
            payload["result_count"] = page.size();
            payload["total_count"] = session->memScanner.count();
            payload["type"] = std::get<std::string>(type);
            payload["items"] = json::array();
            for (const auto addr : page)
            {
                payload["items"].push_back({
                    {"addr", static_cast<std::uint64_t>(addr)},
                    {"addr_hex", std::format("0x{:X}", static_cast<std::uint64_t>(addr))},
                    {"value", stringType ? MemUtils::ReadAsText(addr) : MemUtils::ReadAsString(addr, *dataType)},
                });
            }
            return okData(std::move(payload));
        }

        if (op == "viewer.open")
        {
            const auto address = requiredUInt64("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            const std::string viewFormat = optionalString("view_format");
            if (!viewFormat.empty())
            {
                const auto format = parseViewFormatToken(viewFormat);
                if (!format.has_value())
                    return fail("view_format 无效，支持: hex/hex64/i8/i16/i32/i64/f32/f64/disasm");
                session->memViewer.setFormat(*format);
            }
            session->memViewer.open(static_cast<uintptr_t>(std::get<std::uint64_t>(address)));
            return okData({{"base", static_cast<std::uint64_t>(session->memViewer.base())}, {"format", viewFormatToToken(session->memViewer.format())}, {"read", session->memViewer.readSuccess()}});
        }

        if (op == "viewer.move")
        {
            const auto lines = requiredInt("lines", "lines");
            if (std::holds_alternative<json>(lines))
                return std::get<json>(lines);
            std::size_t step = Types::GetViewSize(session->memViewer.format());
            const std::string stepToken = optionalString("step");
            if (!stepToken.empty())
            {
                const auto parsedStep = parseUInt64(stepToken);
                if (!parsedStep.has_value() || *parsedStep == 0)
                    return fail("step 参数无效");
                step = static_cast<std::size_t>(*parsedStep);
            }
            session->memViewer.move(std::get<int>(lines), step);
            return okData({{"base", static_cast<std::uint64_t>(session->memViewer.base())}, {"read", session->memViewer.readSuccess()}});
        }

        if (op == "viewer.offset")
        {
            const auto offset = requiredString("offset", "offset");
            if (std::holds_alternative<json>(offset))
                return std::get<json>(offset);
            if (!session->memViewer.applyOffset(std::get<std::string>(offset)))
                return fail("offset 参数无效");
            return okData({{"base", static_cast<std::uint64_t>(session->memViewer.base())}, {"read", session->memViewer.readSuccess()}});
        }

        if (op == "viewer.set_format")
        {
            const auto viewFormat = requiredString("view_format", "view_format");
            if (std::holds_alternative<json>(viewFormat))
                return std::get<json>(viewFormat);
            const auto format = parseViewFormatToken(std::get<std::string>(viewFormat));
            if (!format.has_value())
                return fail("view_format 无效，支持: hex/hex64/i8/i16/i32/i64/f32/f64/disasm");
            session->memViewer.setFormat(*format);
            return okData({{"format", viewFormatToToken(session->memViewer.format())}});
        }

        if (op == "viewer.snapshot")
        {
            if (session->memViewer.format() == Types::ViewFormat::Disasm)
                session->memViewer.waitDisasm();
            return okData(buildViewerSnapshotJson(session->memViewer));
        }

        if (op == "pointer.status")
            return okData(pointerStateJson());

        if (op == "pointer.scan")
        {
            const std::string modeToken = optionalString("mode");
            const std::string mode = toLowerAscii(modeToken.empty() ? "module" : modeToken);
            const auto target = requiredUInt64("target", "target");
            const auto depth = requiredInt("depth", "depth");
            const auto maxOffset = requiredInt("max_offset", "max_offset");
            if (std::holds_alternative<json>(target))
                return std::get<json>(target);
            if (std::holds_alternative<json>(depth))
                return std::get<json>(depth);
            if (std::holds_alternative<json>(maxOffset))
                return std::get<json>(maxOffset);
            if (std::get<int>(depth) <= 0 || std::get<int>(depth) > 16)
                return fail("depth 范围为 1-16");
            if (std::get<int>(maxOffset) <= 0)
                return fail("max_offset 必须大于 0");

            const bool useManual = (mode == "manual");
            const bool useArray = (mode == "array");
            std::uint64_t manualBase = 0;
            std::uint64_t arrayBase = 0;
            std::size_t arrayCount = 0;
            if (useManual)
            {
                const auto manual = requiredUInt64("manual_base", "manual_base");
                if (std::holds_alternative<json>(manual))
                    return std::get<json>(manual);
                manualBase = std::get<std::uint64_t>(manual);
            }
            else if (useArray)
            {
                const auto base = requiredUInt64("array_base", "array_base");
                const auto count = requiredUInt64("array_count", "array_count");
                if (std::holds_alternative<json>(base))
                    return std::get<json>(base);
                if (std::holds_alternative<json>(count))
                    return std::get<json>(count);
                if (std::get<std::uint64_t>(count) == 0 || std::get<std::uint64_t>(count) > 1000000)
                    return fail("array_count 范围为 1-1000000");
                arrayBase = std::get<std::uint64_t>(base);
                arrayCount = static_cast<std::size_t>(std::get<std::uint64_t>(count));
            }
            else if (mode != "module")
            {
                return fail("mode 仅支持 module/manual/array");
            }

            const int pid = dr.GetGlobalPid();
            if (pid <= 0)
                return fail("全局PID未设置，请先执行 target.pid.set 或 target.attach.package");
            if (session->pointerManager.isScanning())
                return fail("当前已有指针扫描任务在运行");

            const std::string moduleFilter = optionalString("module_filter");
            session->pointerManager.scan(pid, static_cast<uintptr_t>(std::get<std::uint64_t>(target)), std::get<int>(depth), std::get<int>(maxOffset), useManual, static_cast<uintptr_t>(manualBase), useArray, static_cast<uintptr_t>(arrayBase), arrayCount, moduleFilter);
            return okData(pointerStateJson());
        }

        if (op == "pointer.merge")
        {
            session->pointerManager.MergeBins();
            return okData(pointerStateJson());
        }

        if (op == "pointer.export")
        {
            session->pointerManager.ExportToTxt();
            return okData(pointerStateJson());
        }

        if (op == "breakpoint.info")
        {
            const auto &info = dr.GetHwbpInfoRef();
            return okData(buildHwbpInfoJson(info));
        }

        if (op == "breakpoint.set")
        {
            const auto address = requiredUInt64("address", "address");
            const auto type = requiredString("bp_type", "bp_type");
            const auto scope = requiredString("bp_scope", "bp_scope");
            const auto length = requiredInt("length", "length");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(type))
                return std::get<json>(type);
            if (std::holds_alternative<json>(scope))
                return std::get<json>(scope);
            if (std::holds_alternative<json>(length))
                return std::get<json>(length);

            const int pid = dr.GetGlobalPid();
            if (pid <= 0)
                return fail("全局PID未设置，请先执行 target.pid.set 或 target.attach.package");
            const auto bpType = parseBpTypeToken(std::get<std::string>(type));
            const auto bpScope = parseBpScopeToken(std::get<std::string>(scope));
            if (!bpType.has_value() || !bpScope.has_value() || std::get<int>(length) <= 0 || std::get<int>(length) > 8)
                return fail("断点参数无效，长度范围为 1-8");
            const int status = dr.SetProcessHwbpRef(std::get<std::uint64_t>(address), *bpType, *bpScope, std::get<int>(length));
            if (status != 0)
                return fail(std::format("设置断点失败 status={}", status));
            return okData({{"status", status}, {"type", std::string(bpTypeToToken(*bpType))}, {"scope", std::string(bpScopeToToken(*bpScope))}, {"length", std::get<int>(length)}});
        }

        if (op == "breakpoint.clear")
        {
            dr.RemoveProcessHwbpRef();
            return ok();
        }

        if (op == "breakpoint.record.remove")
        {
            const auto index = requiredInt("index", "index");
            if (std::holds_alternative<json>(index))
                return std::get<json>(index);
            if (std::get<int>(index) < 0)
                return fail("index 无效");
            const auto &info = dr.GetHwbpInfoRef();
            dr.RemoveHwbpRecord(std::get<int>(index));
            return okData({{"record_count", info.record_count}});
        }

        if (op == "breakpoint.record.update")
        {
            const auto index = requiredInt("index", "index");
            const auto field = requiredString("field", "field");
            const auto value = requiredUInt64("value", "value");
            if (std::holds_alternative<json>(index))
                return std::get<json>(index);
            if (std::holds_alternative<json>(field))
                return std::get<json>(field);
            if (std::holds_alternative<json>(value))
                return std::get<json>(value);
            const auto &info = dr.GetHwbpInfoRef();
            if (std::get<int>(index) < 0 || std::get<int>(index) >= info.record_count)
                return fail("index 越界");
            auto copy = info.records[std::get<int>(index)];
            if (!MemUtils::AssignHwbpRecordField(copy, std::get<std::string>(field), std::get<std::uint64_t>(value)))
                return fail("field 无效");
            const_cast<Driver::hwbp_record &>(info.records[std::get<int>(index)]) = copy;
            return okData({{"index", std::get<int>(index)}, {"field", std::get<std::string>(field)}, {"value", std::get<std::uint64_t>(value)}});
        }

        if (op == "breakpoint.record.set_float")
        {
            const auto index = requiredInt("index", "index");
            const auto field = requiredString("field", "field");
            const auto value = requiredDouble("value", "value");
            if (std::holds_alternative<json>(index))
                return std::get<json>(index);
            if (std::holds_alternative<json>(field))
                return std::get<json>(field);
            if (std::holds_alternative<json>(value))
                return std::get<json>(value);

            const std::string fieldToken = toLowerAscii(std::get<std::string>(field));
            if (fieldToken.size() < 2 || (fieldToken[0] != 'v' && fieldToken[0] != 'q'))
                return fail("field 必须是 q0~q31");
            const auto regIndex = parseInt(fieldToken.substr(1));
            if (!regIndex.has_value() || *regIndex < 0 || *regIndex >= 32)
                return fail("field 必须是 q0~q31");

            const auto &info = dr.GetHwbpInfoRef();
            if (std::get<int>(index) < 0 || std::get<int>(index) >= info.record_count)
                return fail("index 越界");

            const std::string precisionToken = optionalString("precision");
            const std::string precision = precisionToken.empty() ? "f32" : toLowerAscii(precisionToken);
            auto copy = info.records[std::get<int>(index)];
            if (precision == "f64")
            {
                const double fval = std::get<double>(value);
                std::uint64_t bits = 0;
                std::memcpy(&bits, &fval, sizeof(bits));
                MemUtils::HwbpWriteQField(copy, *regIndex, static_cast<__uint128_t>(bits));
                const_cast<Driver::hwbp_record &>(info.records[std::get<int>(index)]) = copy;
                return okData({{"index", std::get<int>(index)}, {"reg", *regIndex}, {"precision", "f64"}, {"bits", bits}});
            }
            if (precision != "f32")
                return fail("precision 仅支持 f32/f64");
            const float fval = static_cast<float>(std::get<double>(value));
            std::uint32_t bits = 0;
            std::memcpy(&bits, &fval, sizeof(bits));
            MemUtils::HwbpWriteQField(copy, *regIndex, static_cast<__uint128_t>(bits));
            const_cast<Driver::hwbp_record &>(info.records[std::get<int>(index)]) = copy;
            return okData({{"index", std::get<int>(index)}, {"reg", *regIndex}, {"precision", "f32"}, {"bits", bits}});
        }

        if (op == "signature.scan_address")
        {
            const auto address = requiredUInt64("address", "address");
            const auto range = requiredInt("range", "range");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(range))
                return std::get<json>(range);
            const std::string requestedFile = optionalString("file_name");
            const std::string fileName = requestedFile.empty() ? std::string(SignatureScanner::SIG_DEFAULT_FILE) : requestedFile;
            if (!SignatureScanner::ScanAddressSignature(static_cast<uintptr_t>(std::get<std::uint64_t>(address)), std::get<int>(range), fileName.c_str()))
                return fail("特征码保存失败");
            return okData({{"saved", true}, {"file", fileName}});
        }

        if (op == "signature.scan_file")
        {
            const std::string requestedFile = optionalString("file_name");
            const std::string fileName = requestedFile.empty() ? std::string(SignatureScanner::SIG_DEFAULT_FILE) : requestedFile;
            json payload = buildSignatureMatchesJson(SignatureScanner::ScanSignatureFromFile(fileName.c_str()), 0, "");
            payload["file"] = fileName;
            return okData(std::move(payload));
        }

        if (op == "signature.scan_pattern")
        {
            const auto rangeOffset = requiredInt64("range_offset", "range_offset");
            const auto pattern = requiredString("pattern", "pattern");
            if (std::holds_alternative<json>(rangeOffset))
                return std::get<json>(rangeOffset);
            if (std::holds_alternative<json>(pattern))
                return std::get<json>(pattern);
            if (std::get<std::int64_t>(rangeOffset) < static_cast<std::int64_t>(std::numeric_limits<int>::min()) || std::get<std::int64_t>(rangeOffset) > static_cast<std::int64_t>(std::numeric_limits<int>::max()))
                return fail("range_offset 无效");
            const auto matches = SignatureScanner::ScanSignature(std::get<std::string>(pattern).c_str(), static_cast<int>(std::get<std::int64_t>(rangeOffset)));
            return okData(buildSignatureMatchesJson(matches, std::get<std::int64_t>(rangeOffset), std::get<std::string>(pattern)));
        }

        if (op == "signature.filter")
        {
            const auto address = requiredUInt64("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            const std::string requestedFile = optionalString("file_name");
            const std::string fileName = requestedFile.empty() ? std::string(SignatureScanner::SIG_DEFAULT_FILE) : requestedFile;
            const auto result = SignatureScanner::FilterSignature(static_cast<uintptr_t>(std::get<std::uint64_t>(address)), fileName.c_str());
            return okData({{"success", result.success}, {"changed_count", result.changedCount}, {"total_count", result.totalCount}, {"old_signature", result.oldSignature}, {"new_signature", result.newSignature}, {"file", fileName}});
        }

        if (op == "lock.set")
        {
            const auto address = requiredUInt64("address", "address");
            const auto valueType = requiredString("value_type", "value_type");
            const auto value = requiredString("value", "value");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(valueType))
                return std::get<json>(valueType);
            if (std::holds_alternative<json>(value))
                return std::get<json>(value);
            const auto dataType = parseDataTypeToken(std::get<std::string>(valueType));
            if (!dataType.has_value())
                return fail("value_type 无效");
            gLockManager.lock(static_cast<uintptr_t>(std::get<std::uint64_t>(address)), *dataType, std::get<std::string>(value));
            return okData({{"locked", gLockManager.isLocked(static_cast<uintptr_t>(std::get<std::uint64_t>(address)))}});
        }

        if (op == "lock.unset" || op == "lock.status")
        {
            const auto address = requiredUInt64("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (op == "lock.unset")
                gLockManager.unlock(static_cast<uintptr_t>(std::get<std::uint64_t>(address)));
            return okData({{"locked", gLockManager.isLocked(static_cast<uintptr_t>(std::get<std::uint64_t>(address)))}});
        }

        if (op == "lock.clear")
        {
            gLockManager.clear();
            return ok();
        }

        if (op == "memory.read_block")
        {
            const auto address = requiredUInt64("address", "address");
            const auto size = requiredUInt64("size", "size");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(size))
                return std::get<json>(size);
            if (std::get<std::uint64_t>(size) == 0 || std::get<std::uint64_t>(size) > 4096)
                return fail("size 范围 1-4096");
            std::vector<std::uint8_t> buffer(static_cast<std::size_t>(std::get<std::uint64_t>(size)));
            const int readBytes = dr.Read(std::get<std::uint64_t>(address), buffer.data(), buffer.size());
            if (readBytes <= 0)
                return fail(std::format("读取失败 status={}", readBytes));
            return okData({{"requested_size", std::get<std::uint64_t>(size)}, {"read_size", readBytes}, {"data_hex", bytesToHex(buffer.data(), static_cast<std::size_t>(readBytes))}});
        }

        if (op == "memory.read_value")
        {
            const auto address = requiredUInt64("address", "address");
            const auto valueType = requiredString("value_type", "value_type");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(valueType))
                return std::get<json>(valueType);
            const std::string type = toLowerAscii(std::get<std::string>(valueType));
            const auto addr = std::get<std::uint64_t>(address);

            if (type == "u8")
            {
                const auto value = readScalarValue<std::uint8_t>(addr);
                return value ? okData({{"value", *value}}) : fail("读取失败");
            }
            if (type == "u16")
            {
                const auto value = readScalarValue<std::uint16_t>(addr);
                return value ? okData({{"value", *value}}) : fail("读取失败");
            }
            if (type == "u32")
            {
                const auto value = readScalarValue<std::uint32_t>(addr);
                return value ? okData({{"value", *value}}) : fail("读取失败");
            }
            if (type == "u64")
            {
                const auto value = readScalarValue<std::uint64_t>(addr);
                return value ? okData({{"value", *value}}) : fail("读取失败");
            }
            if (type == "f32")
            {
                const auto value = readScalarValue<float>(addr);
                return value ? okData({{"value", *value}}) : fail("读取失败");
            }
            if (type == "f64")
            {
                const auto value = readScalarValue<double>(addr);
                return value ? okData({{"value", *value}}) : fail("读取失败");
            }
            return fail("memory.read_value 的 value_type 仅支持 u8/u16/u32/u64/f32/f64");
        }

        if (op == "memory.write_block")
        {
            const auto address = requiredUInt64("address", "address");
            const auto dataHex = requiredString("data_hex", "data_hex");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(dataHex))
                return std::get<json>(dataHex);
            auto bytes = parseHexBytes(std::get<std::string>(dataHex));
            if (!bytes.has_value() || bytes->empty())
                return fail("data_hex 无效");
            const int writeBytes = dr.Write(std::get<std::uint64_t>(address), bytes->data(), bytes->size());
            if (writeBytes != static_cast<int>(bytes->size()))
                return fail(std::format("写入失败 status={}", writeBytes));
            return okData({{"size", bytes->size()}});
        }

        return fail(std::format("未知 operation: {}", op));
    }

    // 统一命令派发入口：网络层仅接受 JSON 请求并返回 JSON 响应。
    std::string DispatchCommandUnified(const std::shared_ptr<ClientSession> &session, const std::string &request)
    {
        const auto parsedReq = json::parse(request, nullptr, false);
        if (parsedReq.is_discarded())
        {
            return json({{"ok", false}, {"error", "请求必须是 JSON 字符串对象"}}).dump();
        }

        if (!parsedReq.is_object())
        {
            return json({{"ok", false}, {"error", "请求必须是 JSON 对象"}}).dump();
        }

        if (parsedReq.contains("operation"))
        {
            if (!parsedReq["operation"].is_string())
            {
                return makeProtocolError("operation 字段必须是字符串").dump();
            }

            const std::string operationName = parsedReq["operation"].get<std::string>();
            json params = json::object();
            if (parsedReq.contains("params"))
            {
                if (!parsedReq["params"].is_object())
                {
                    json error = makeProtocolError("params 字段必须是对象");
                    error["operation"] = operationName;
                    return error.dump();
                }
                params = parsedReq["params"];
            }

            return dispatchStructuredOperationDirect(session, operationName, params).dump();
        }

        return makeProtocolError("请求缺少 operation 字段").dump();
    }

    void HandleClientConnection(int clientFd, sockaddr_in clientAddr)
    {
        const auto session = std::make_shared<ClientSession>(gClientSessionSeed.fetch_add(1));

        char clientIp[INET_ADDRSTRLEN]{};
        if (inet_ntop(AF_INET, &clientAddr.sin_addr, clientIp, sizeof(clientIp)) == nullptr)
        {
            std::strncpy(clientIp, "未知地址", sizeof(clientIp) - 1);
            clientIp[sizeof(clientIp) - 1] = '\0';
        }

        std::println("客户端已连接：{}:{} session={}", clientIp, ntohs(clientAddr.sin_port), session->sessionId);

        std::string buffer;
        buffer.reserve(4096);
        char recvChunk[4096]{};

        while (gRunning)
        {
            const ssize_t receivedBytes = recv(clientFd, recvChunk, sizeof(recvChunk), 0);
            if (receivedBytes == 0)
            {
                std::println("客户端已断开连接：session={}", session->sessionId);
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

            buffer.append(recvChunk, recvChunk + receivedBytes);

            for (;;)
            {
                const auto newlinePos = buffer.find('\n');
                if (newlinePos == std::string::npos)
                {
                    break;
                }

                std::string message = buffer.substr(0, newlinePos);
                buffer.erase(0, newlinePos + 1);
                trimLineEnding(message);
                if (message.empty())
                {
                    continue;
                }

                std::println("收到命令：session={} {}", session->sessionId, message);
                const std::string response = DispatchCommandUnified(session, message) + "\n";
                if (!sendAll(clientFd, response))
                {
                    printErrno("发送回复失败");
                    close(clientFd);
                    return;
                }
            }
        }

        close(clientFd);
    }
} // namespace

// 程序入口：初始化服务并处理客户端请求。
int tcp_server()
{

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

        if (!Utils::GlobalPool.post_io(HandleClientConnection, clientFd, clientAddr))
        {
            printErrno("IO线程池已停止，无法派发连接");
            close(clientFd);
        }
    }

    if (gServerFd >= 0)
    {
        close(gServerFd);
        gServerFd = -1;
    }

    std::println("服务端已退出。");
    return 0;
}
