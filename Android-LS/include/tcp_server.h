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
    std::vector<std::string> splitTokens(const std::string &input)
    {
        std::istringstream iss(input);
        std::vector<std::string> tokens;
        std::string token;
        while (iss >> token)
        {
            tokens.push_back(token);
        }
        return tokens;
    }

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

    // 按字段名写入硬件断点记录中的寄存器或元数据。
    bool assignHwbpRecordField(Driver::hwbp_record &record, std::string_view fieldToken, std::uint64_t value)
    {
        const std::string token = toLowerAscii(fieldToken);
        if (token == "pc")
        {
            record.pc = value;
            return true;
        }
        if (token == "lr")
        {
            record.lr = value;
            return true;
        }
        if (token == "sp")
        {
            record.sp = value;
            return true;
        }
        if (token == "pstate")
        {
            record.pstate = value;
            return true;
        }
        if (token == "orig_x0")
        {
            record.orig_x0 = value;
            return true;
        }
        if (token == "syscallno")
        {
            record.syscallno = value;
            return true;
        }
        if (token == "fpsr")
        {
            record.fpsr = static_cast<std::uint32_t>(value);
            return true;
        }
        if (token == "fpcr")
        {
            record.fpcr = static_cast<std::uint32_t>(value);
            return true;
        }
        if (token == "rw")
        {
            record.rw = value != 0;
            return true;
        }
        if (token.size() >= 2 && token[0] == 'x')
        {
            const auto regIndex = parseInt(token.substr(1));
            if (regIndex.has_value() && *regIndex >= 0 && *regIndex < 30)
            {
                record.regs[*regIndex] = value;
                return true;
            }
        }
        if (token.size() >= 2 && token[0] == 'v')
        {
            auto regIndex = parseInt(token.substr(1));
            if (regIndex.has_value() && *regIndex >= 0 && *regIndex < 32)
            {
                record.vregs[*regIndex] = static_cast<__uint128_t>(value);
                return true;
            }
        }
        return false;
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
    std::string joinTokens(const std::vector<std::string> &tokens, std::size_t start)
    {
        if (start >= tokens.size())
        {
            return "";
        }

        std::string text = tokens[start];
        for (std::size_t i = start + 1; i < tokens.size(); ++i)
        {
            text.append(" ");
            text.append(tokens[i]);
        }
        return text;
    }

    // 生成成功响应文本
    std::string ok(std::string_view message)
    {
        return std::format("ok {}", message);
    }

    // 生成失败响应文本
    std::string err(std::string_view message)
    {
        return std::format("err {}", message);
    }

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
            const auto &rec = info.records[i];
            json item;
            item["index"] = i;
            item["rw"] = rec.rw ? "write" : "read";
            item["pc"] = rec.pc;
            item["pc_hex"] = std::format("0x{:X}", static_cast<std::uint64_t>(rec.pc));
            item["hit_count"] = rec.hit_count;
            item["lr"] = rec.lr;
            item["sp"] = rec.sp;
            item["orig_x0"] = rec.orig_x0;
            item["syscallno"] = rec.syscallno;
            item["pstate"] = rec.pstate;
            item["regs"] = json::array();
            for (const auto reg : rec.regs)
            {
                item["regs"].push_back(reg);
            }
            item["vregs"] = json::array();
            for (const auto &vreg : rec.vregs)
            {
                item["vregs"].push_back({{"lo", static_cast<std::uint64_t>(vreg)},
                                         {"hi", static_cast<std::uint64_t>(vreg >> 64)}});
            }
            item["fpsr"] = rec.fpsr;
            item["fpcr"] = rec.fpcr;
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
    std::string DispatchTextCommand(const std::shared_ptr<ClientSession> &session, const std::string &request)
    {
        std::lock_guard<std::mutex> driverLock(gDriverCommandMutex);
        const auto tokens = splitTokens(request);
        if (tokens.empty())
        {
            return ok("收到");
        }

        const std::string &command = tokens[0];

        if (command == "help")
        {
            return ok("支持命令: ping, pid.get, pid.set, pid.current, pid.attach, hwbp.info, hwbp.set, hwbp.remove, hwbp.record.remove, hwbp.record.set, sig.scan.addr, sig.filter, sig.scan.pattern, sig.scan.file, lock.toggle, lock.set, lock.unset, lock.status, lock.clear, scan.status, scan.clear, scan.first, scan.next, scan.page, scan.add, scan.remove, scan.offset, viewer.open, viewer.move, viewer.offset, viewer.format, viewer.get, pointer.status, pointer.scan, pointer.scan.manual, pointer.scan.array, pointer.merge, pointer.export, mem.read, mem.write, mem.read_u8/u16/u32/u64/f32/f64, mem.write_u8/u16/u32/u64/f32/f64, mem.read_str, mem.read_wstr, memory.refresh, memory.summary, memory.info.full, module.addr, touch.down, touch.move, touch.up");
        }

        if (command == "ping")
        {
            return ok("pong");
        }

        if (command == "pid.get")
        {
            if (tokens.size() < 2)
            {
                return err("用法: pid.get <包名>");
            }

            const std::string packageName = joinTokens(tokens, 1);
            const int pid = dr.GetPid(packageName);
            if (pid <= 0)
            {
                return err("未找到进程");
            }
            return ok(std::format("pid={}", pid));
        }

        if (command == "pid.set")
        {
            if (tokens.size() != 2)
            {
                return err("用法: pid.set <pid>");
            }

            const auto pid = parseInt(tokens[1]);
            if (!pid.has_value() || *pid <= 0)
            {
                return err("pid 参数无效");
            }

            dr.SetGlobalPid(*pid);
            return ok(std::format("pid={}", dr.GetGlobalPid()));
        }

        if (command == "pid.current")
        {
            return ok(std::format("pid={}", dr.GetGlobalPid()));
        }

        if (command == "pid.attach")
        {
            if (tokens.size() < 2)
            {
                return err("用法: pid.attach <包名>");
            }

            const std::string packageName = joinTokens(tokens, 1);
            const int pid = dr.GetPid(packageName);
            if (pid <= 0)
            {
                return err("未找到进程");
            }

            dr.SetGlobalPid(pid);
            return ok(std::format("pid={}", pid));
        }

        if (command == "hwbp.info")
        {
            const auto &info = dr.GetHwbpInfoRef();
            const std::string jsonText = buildHwbpInfoJson(info).dump();
            return std::format("ok hwbp.info size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "hwbp.set")
        {
            if (tokens.size() != 5)
            {
                return err("用法: hwbp.set <地址> <类型(0-3)> <范围(0-2)> <长度>");
            }

            const int pid = dr.GetGlobalPid();
            if (pid <= 0)
            {
                return err("全局PID未设置，请先执行 pid.set 或 pid.attach");
            }

            const auto targetAddr = parseUInt64(tokens[1]);
            const auto bpType = parseBpTypeToken(tokens[2]);
            const auto bpScope = parseBpScopeToken(tokens[3]);
            const auto lenBytes = parseInt(tokens[4]);
            if (!targetAddr.has_value() || *targetAddr == 0 || !bpType.has_value() || !bpScope.has_value() || !lenBytes.has_value())
            {
                return err("参数无效");
            }

            if (*lenBytes <= 0 || *lenBytes > 8)
            {
                return err("长度范围为 1-8");
            }

            const int status = dr.SetProcessHwbpRef(*targetAddr, *bpType, *bpScope, *lenBytes);
            if (status != 0)
            {
                return err(std::format("设置断点失败 status={}", status));
            }

            return ok(std::format("status=0 type={} scope={} len={}", bpTypeToToken(*bpType), bpScopeToToken(*bpScope), *lenBytes));
        }

        if (command == "hwbp.remove")
        {
            dr.RemoveProcessHwbpRef();
            return ok("done=1");
        }

        if (command == "hwbp.record.remove")
        {
            if (tokens.size() != 2)
            {
                return err("用法: hwbp.record.remove <索引>");
            }

            const auto index = parseInt(tokens[1]);
            if (!index.has_value() || *index < 0)
            {
                return err("索引无效");
            }

            (void)dr.GetHwbpInfoRef();
            dr.RemoveHwbpRecord(*index);
            const auto &updated = dr.GetHwbpInfoRef();
            return ok(std::format("record_count={}", updated.record_count));
        }

        if (command == "hwbp.record.set")
        {
            if (tokens.size() != 4)
            {
                return err("用法: hwbp.record.set <索引> <字段> <值>");
            }

            const auto index = parseInt(tokens[1]);
            const auto value = parseUInt64(tokens[3]);
            if (!index.has_value() || *index < 0 || !value.has_value())
            {
                return err("索引或值无效");
            }

            const auto &info = dr.GetHwbpInfoRef();
            if (*index >= info.record_count)
            {
                return err("索引越界");
            }

            auto copy = info.records[*index];
            if (!assignHwbpRecordField(copy, tokens[2], *value))
            {
                return err("字段无效，支持: pc/lr/sp/pstate/orig_x0/syscallno/fpsr/fpcr/rw/x0~x29/v0~v31");
            }

            copy.rw = true;
            const_cast<Driver::hwbp_record &>(dr.GetHwbpInfoRef().records[*index]) = copy;
            return ok(std::format("index={} field={} value=0x{:X}", *index, tokens[2], *value));
        }

        if (command == "hwbp.record.set.f32")
        {
            if (tokens.size() != 4)
            {
                return err("用法: hwbp.record.set.f32 <索引> <v0~v31> <浮点值>");
            }

            const auto index = parseInt(tokens[1]);
            if (!index.has_value() || *index < 0)
            {
                return err("索引无效");
            }

            const std::string field = toLowerAscii(tokens[2]);
            if (field.size() < 2 || field[0] != 'v')
            {
                return err("字段必须是 v0~v31");
            }

            auto regIndex = parseInt(field.substr(1));
            if (!regIndex.has_value() || *regIndex < 0 || *regIndex >= 32)
            {
                return err("字段必须是 v0~v31");
            }

            float fval = 0.0f;
            try
            {
                size_t pos = 0;
                fval = std::stof(std::string(tokens[3]), &pos);
            }
            catch (...)
            {
                return err("浮点值无效");
            }

            const auto &info = dr.GetHwbpInfoRef();
            if (*index >= info.record_count)
            {
                return err("索引越界");
            }

            auto copy = info.records[*index];
            uint32_t bits;
            std::memcpy(&bits, &fval, sizeof(bits));
            copy.vregs[*regIndex] = static_cast<__uint128_t>(bits);
            copy.rw = true;
            const_cast<Driver::hwbp_record &>(dr.GetHwbpInfoRef().records[*index]) = copy;
            return ok(std::format("index={} v{}={} (0x{:08X})", *index, *regIndex, fval, bits));
        }

        if (command == "hwbp.record.set.f64")
        {
            if (tokens.size() != 4)
            {
                return err("用法: hwbp.record.set.f64 <索引> <v0~v31> <浮点值>");
            }

            const auto index = parseInt(tokens[1]);
            if (!index.has_value() || *index < 0)
            {
                return err("索引无效");
            }

            const std::string field = toLowerAscii(tokens[2]);
            if (field.size() < 2 || field[0] != 'v')
            {
                return err("字段必须是 v0~v31");
            }

            auto regIndex = parseInt(field.substr(1));
            if (!regIndex.has_value() || *regIndex < 0 || *regIndex >= 32)
            {
                return err("字段必须是 v0~v31");
            }

            double fval = 0.0;
            try
            {
                size_t pos = 0;
                fval = std::stod(std::string(tokens[3]), &pos);
            }
            catch (...)
            {
                return err("浮点值无效");
            }

            const auto &info = dr.GetHwbpInfoRef();
            if (*index >= info.record_count)
            {
                return err("索引越界");
            }

            auto copy = info.records[*index];
            uint64_t bits;
            std::memcpy(&bits, &fval, sizeof(bits));
            copy.vregs[*regIndex] = static_cast<__uint128_t>(bits);
            copy.rw = true;
            const_cast<Driver::hwbp_record &>(dr.GetHwbpInfoRef().records[*index]) = copy;
            return ok(std::format("index={} v{}={} (0x{:016X})", *index, *regIndex, fval, bits));
        }

        if (command == "sig.scan.addr")
        {
            if (tokens.size() < 3)
            {
                return err("用法: sig.scan.addr <地址> <范围> [文件名]");
            }

            const auto addr = parseUInt64(tokens[1]);
            const auto range = parseInt(tokens[2]);
            if (!addr.has_value() || *addr == 0 || !range.has_value())
            {
                return err("地址或范围无效");
            }

            const std::string fileName = (tokens.size() >= 4) ? joinTokens(tokens, 3) : std::string(SignatureScanner::SIG_DEFAULT_FILE);
            const bool success = SignatureScanner::ScanAddressSignature(static_cast<uintptr_t>(*addr), *range, fileName.c_str());
            if (!success)
            {
                return err("特征码保存失败");
            }

            return ok(std::format("saved=1 file={}", fileName));
        }

        if (command == "sig.filter")
        {
            if (tokens.size() < 2)
            {
                return err("用法: sig.filter <地址> [文件名]");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            const std::string fileName = (tokens.size() >= 3) ? joinTokens(tokens, 2) : std::string(SignatureScanner::SIG_DEFAULT_FILE);
            const auto result = SignatureScanner::FilterSignature(static_cast<uintptr_t>(*addr), fileName.c_str());

            json payload;
            payload["success"] = result.success;
            payload["changed_count"] = result.changedCount;
            payload["total_count"] = result.totalCount;
            payload["old_signature"] = result.oldSignature;
            payload["new_signature"] = result.newSignature;
            payload["file"] = fileName;

            const std::string jsonText = payload.dump();
            return std::format("ok sig.filter size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "sig.scan.pattern")
        {
            if (tokens.size() < 3)
            {
                return err("用法: sig.scan.pattern <范围偏移> <特征码...>");
            }

            const auto range = parseInt64(tokens[1]);
            if (!range.has_value() || *range < static_cast<std::int64_t>(std::numeric_limits<int>::min()) || *range > static_cast<std::int64_t>(std::numeric_limits<int>::max()))
            {
                return err("范围偏移无效");
            }

            const std::string pattern = joinTokens(tokens, 2);
            if (pattern.empty())
            {
                return err("特征码不能为空");
            }

            const auto matches = SignatureScanner::ScanSignature(pattern.c_str(), static_cast<int>(*range));
            const std::string jsonText = buildSignatureMatchesJson(matches, *range, pattern).dump();
            return std::format("ok sig.scan.pattern size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "sig.scan.file")
        {
            const std::string fileName = (tokens.size() >= 2) ? joinTokens(tokens, 1) : std::string(SignatureScanner::SIG_DEFAULT_FILE);
            const auto matches = SignatureScanner::ScanSignatureFromFile(fileName.c_str());
            json payload = buildSignatureMatchesJson(matches, 0, "");
            payload["file"] = fileName;
            const std::string jsonText = payload.dump();
            return std::format("ok sig.scan.file size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "lock.toggle")
        {
            if (tokens.size() != 3)
            {
                return err("用法: lock.toggle <地址> <type>");
            }

            const auto addr = parseUInt64(tokens[1]);
            const auto dataType = parseDataTypeToken(tokens[2]);
            if (!addr.has_value() || *addr == 0 || !dataType.has_value())
            {
                return err("地址或type无效");
            }

            gLockManager.toggle(static_cast<uintptr_t>(*addr), *dataType);
            return ok(std::format("locked={}", gLockManager.isLocked(static_cast<uintptr_t>(*addr)) ? 1 : 0));
        }

        if (command == "lock.set")
        {
            if (tokens.size() < 4)
            {
                return err("用法: lock.set <地址> <type> <value>");
            }

            const auto addr = parseUInt64(tokens[1]);
            const auto dataType = parseDataTypeToken(tokens[2]);
            if (!addr.has_value() || *addr == 0 || !dataType.has_value())
            {
                return err("地址或type无效");
            }

            const std::string value = joinTokens(tokens, 3);
            gLockManager.lock(static_cast<uintptr_t>(*addr), *dataType, value);
            return ok(std::format("locked={}", gLockManager.isLocked(static_cast<uintptr_t>(*addr)) ? 1 : 0));
        }

        if (command == "lock.unset")
        {
            if (tokens.size() != 2)
            {
                return err("用法: lock.unset <地址>");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            gLockManager.unlock(static_cast<uintptr_t>(*addr));
            return ok("locked=0");
        }

        if (command == "lock.status")
        {
            if (tokens.size() != 2)
            {
                return err("用法: lock.status <地址>");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            return ok(std::format("locked={}", gLockManager.isLocked(static_cast<uintptr_t>(*addr)) ? 1 : 0));
        }

        if (command == "lock.clear")
        {
            gLockManager.clear();
            return ok("已清空所有锁定");
        }

        if (command == "scan.status")
        {
            return ok(std::format("scanning={} progress={:.4f} count={}",
                                  session->memScanner.isScanning() ? 1 : 0,
                                  session->memScanner.progress(),
                                  session->memScanner.count()));
        }

        if (command == "scan.clear")
        {
            session->memScanner.clear();
            return ok("已清空扫描结果");
        }

        if (command == "scan.add")
        {
            if (tokens.size() != 2)
            {
                return err("用法: scan.add <地址>");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            session->memScanner.add(static_cast<uintptr_t>(*addr));
            return ok(std::format("count={}", session->memScanner.count()));
        }

        if (command == "scan.remove")
        {
            if (tokens.size() != 2)
            {
                return err("用法: scan.remove <地址>");
            }

            const auto addr = parseUInt64(tokens[1]);
            if (!addr.has_value() || *addr == 0)
            {
                return err("地址无效");
            }

            session->memScanner.remove(static_cast<uintptr_t>(*addr));
            return ok(std::format("count={}", session->memScanner.count()));
        }

        if (command == "scan.offset")
        {
            if (tokens.size() != 2)
            {
                return err("用法: scan.offset <有符号偏移>");
            }

            const auto offset = parseInt64(tokens[1]);
            if (!offset.has_value())
            {
                return err("偏移参数无效");
            }

            session->memScanner.applyOffset(*offset);
            return ok(std::format("count={}", session->memScanner.count()));
        }

        if (command == "scan.first" || command == "scan.next")
        {
            if (tokens.size() < 3)
            {
                return err("用法: scan.first/scan.next <type> <mode> [value] [rangeMax]");
            }

            const auto dataType = parseDataTypeToken(tokens[1]);
            if (!dataType.has_value())
            {
                return err("type 无效，支持: i8/i16/i32/i64/f32/f64");
            }

            const auto fuzzyMode = parseFuzzyModeToken(tokens[2]);
            if (!fuzzyMode.has_value())
            {
                return err("mode 无效，支持: unknown/eq/gt/lt/inc/dec/changed/unchanged/range/pointer/string");
            }

            const bool isFirst = (command == "scan.first");
            const int pid = dr.GetGlobalPid();
            if (pid <= 0)
            {
                return err("全局PID未设置，请先执行 pid.set 或 pid.attach");
            }

            if (*fuzzyMode == Types::FuzzyMode::String)
            {
                if (tokens.size() < 4)
                {
                    return err("string 模式需要 value 参数");
                }
                const std::string needle = joinTokens(tokens, 3);
                session->memScanner.scanString(pid, needle, isFirst);
                return ok(std::format("count={} progress={:.4f} scanning={}",
                                      session->memScanner.count(),
                                      session->memScanner.progress(),
                                      session->memScanner.isScanning() ? 1 : 0));
            }

            const bool needValue = (*fuzzyMode != Types::FuzzyMode::Unknown);
            if (needValue && tokens.size() < 4)
            {
                return err("当前模式需要 value 参数");
            }

            double rangeMax = 0.0;
            if (*fuzzyMode == Types::FuzzyMode::Range)
            {
                if (tokens.size() < 5)
                {
                    return err("range 模式需要 rangeMax 参数");
                }
                const auto parsedRange = parseDouble(tokens[4]);
                if (!parsedRange.has_value() || *parsedRange < 0.0)
                {
                    return err("rangeMax 无效");
                }
                rangeMax = *parsedRange;
            }
            else if (tokens.size() >= 5)
            {
                const auto parsedRange = parseDouble(tokens[4]);
                if (parsedRange.has_value() && *parsedRange >= 0.0)
                {
                    rangeMax = *parsedRange;
                }
            }

            const std::string valueToken = (needValue ? tokens[3] : "0");

            const auto result = MemUtils::DispatchType(*dataType, [&]<typename T>() -> std::string
                                                       {
                T target{};
                if (needValue)
                {
                    const auto parsedValue = parseScanValueToken<T>(valueToken);
                    if (!parsedValue.has_value())
                    {
                        return err("value 参数无效");
                    }
                    target = *parsedValue;
                }

                session->memScanner.scan<T>(pid, target, *fuzzyMode, isFirst, rangeMax);
                return ok(std::format("count={} progress={:.4f} scanning={}",
                                      session->memScanner.count(),
                                      session->memScanner.progress(),
                                      session->memScanner.isScanning() ? 1 : 0)); });

            return result;
        }

        if (command == "scan.page")
        {
            if (tokens.size() != 4)
            {
                return err("用法: scan.page <start> <count> <type>");
            }

            const auto start = parseUInt64(tokens[1]);
            const auto count = parseUInt64(tokens[2]);
            const std::string typeToken = toLowerAscii(tokens[3]);
            const bool stringType = (typeToken == "str" || typeToken == "string" || typeToken == "text");
            const auto dataType = parseDataTypeToken(tokens[3]);
            if (!start.has_value() || !count.has_value() || (!stringType && !dataType.has_value()))
            {
                return err("参数无效");
            }

            if (*count == 0 || *count > 2000)
            {
                return err("count 范围 1-2000");
            }

            const auto page = session->memScanner.getPage(static_cast<size_t>(*start), static_cast<size_t>(*count));
            json payload;
            payload["start"] = *start;
            payload["request_count"] = *count;
            payload["result_count"] = page.size();
            payload["total_count"] = session->memScanner.count();
            payload["type"] = tokens[3];
            payload["items"] = json::array();

            for (const auto addr : page)
            {
                payload["items"].push_back({
                    {"addr", static_cast<std::uint64_t>(addr)},
                    {"addr_hex", std::format("0x{:X}", static_cast<std::uint64_t>(addr))},
                    {"value", stringType ? MemUtils::ReadAsText(addr) : MemUtils::ReadAsString(addr, *dataType)},
                });
            }

            const std::string jsonText = payload.dump();
            return std::format("ok scan.page size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "viewer.open")
        {
            if (tokens.size() < 2 || tokens.size() > 3)
            {
                return err("用法: viewer.open <地址> [format]");
            }

            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
            {
                return err("地址无效");
            }

            if (tokens.size() == 3)
            {
                const auto format = parseViewFormatToken(tokens[2]);
                if (!format.has_value())
                {
                    return err("format 无效，支持: hex/hex64/i8/i16/i32/i64/f32/f64/disasm");
                }
                session->memViewer.setFormat(*format);
            }

            session->memViewer.open(static_cast<uintptr_t>(*address));
            return ok(std::format("base=0x{:X} format={} read={}",
                                  static_cast<std::uint64_t>(session->memViewer.base()),
                                  viewFormatToToken(session->memViewer.format()),
                                  session->memViewer.readSuccess() ? 1 : 0));
        }

        if (command == "viewer.move")
        {
            if (tokens.size() < 2 || tokens.size() > 3)
            {
                return err("用法: viewer.move <行数> [步长]");
            }

            const auto lines = parseInt(tokens[1]);
            if (!lines.has_value())
            {
                return err("行数参数无效");
            }

            std::size_t step = Types::GetViewSize(session->memViewer.format());
            if (step == 0)
            {
                step = 1;
            }
            if (tokens.size() == 3)
            {
                const auto parsedStep = parseUInt64(tokens[2]);
                if (!parsedStep.has_value() || *parsedStep == 0)
                {
                    return err("步长参数无效");
                }
                step = static_cast<std::size_t>(*parsedStep);
            }

            session->memViewer.move(*lines, step);
            return ok(std::format("base=0x{:X} read={}",
                                  static_cast<std::uint64_t>(session->memViewer.base()),
                                  session->memViewer.readSuccess() ? 1 : 0));
        }

        if (command == "viewer.offset")
        {
            if (tokens.size() != 2)
            {
                return err("用法: viewer.offset <偏移，如 +0x20/-0x10>");
            }

            if (!session->memViewer.applyOffset(tokens[1]))
            {
                return err("偏移参数无效");
            }

            return ok(std::format("base=0x{:X} read={}",
                                  static_cast<std::uint64_t>(session->memViewer.base()),
                                  session->memViewer.readSuccess() ? 1 : 0));
        }

        if (command == "viewer.format")
        {
            if (tokens.size() != 2)
            {
                return err("用法: viewer.format <format>");
            }

            const auto format = parseViewFormatToken(tokens[1]);
            if (!format.has_value())
            {
                return err("format 无效，支持: hex/hex64/i8/i16/i32/i64/f32/f64/disasm");
            }

            session->memViewer.setFormat(*format);
            return ok(std::format("format={}", viewFormatToToken(session->memViewer.format())));
        }

        if (command == "viewer.get")
        {
            const std::string jsonText = buildViewerSnapshotJson(session->memViewer).dump();
            return std::format("ok viewer.get size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "pointer.status")
        {
            return ok(std::format("scanning={} progress={:.4f} count={}",
                                  session->pointerManager.isScanning() ? 1 : 0,
                                  session->pointerManager.scanProgress(),
                                  session->pointerManager.count()));
        }

        if (command == "pointer.scan" || command == "pointer.scan.manual" || command == "pointer.scan.array")
        {
            const bool useManual = (command == "pointer.scan.manual");
            const bool useArray = (command == "pointer.scan.array");

            const std::size_t minTokenCount = useManual ? 5 : (useArray ? 6 : 4);
            if (tokens.size() < minTokenCount)
            {
                if (useManual)
                    return err("用法: pointer.scan.manual <target> <depth> <maxOffset> <manualBase> [模块过滤]");
                if (useArray)
                    return err("用法: pointer.scan.array <target> <depth> <maxOffset> <arrayBase> <arrayCount> [模块过滤]");
                return err("用法: pointer.scan <target> <depth> <maxOffset> [模块过滤]");
            }

            const int pid = dr.GetGlobalPid();
            if (pid <= 0)
            {
                return err("全局PID未设置，请先执行 pid.set 或 pid.attach");
            }

            const auto target = parseUInt64(tokens[1]);
            const auto depth = parseInt(tokens[2]);
            const auto maxOffset = parseInt(tokens[3]);
            if (!target.has_value() || !depth.has_value() || !maxOffset.has_value())
            {
                return err("target/depth/maxOffset 参数无效");
            }
            if (*depth <= 0 || *depth > 16)
            {
                return err("depth 范围为 1-16");
            }
            if (*maxOffset <= 0)
            {
                return err("maxOffset 必须大于 0");
            }

            std::uint64_t manualBase = 0;
            std::uint64_t arrayBase = 0;
            std::size_t arrayCount = 0;
            std::size_t filterStart = 4;

            if (useManual)
            {
                const auto manualBaseParsed = parseUInt64(tokens[4]);
                const auto legacyManualMaxOffset = (tokens.size() > 5) ? parseInt(tokens[5]) : std::optional<int>{};
                if (!manualBaseParsed.has_value())
                {
                    return err("manualBase 参数无效");
                }
                manualBase = *manualBaseParsed;
                filterStart = 5;
                if (legacyManualMaxOffset.has_value())
                {
                    if (*legacyManualMaxOffset <= 0)
                        return err("manualMaxOffset invalid");
                    filterStart = 6;
                }
            }
            else if (useArray)
            {
                const auto arrayBaseParsed = parseUInt64(tokens[4]);
                const auto arrayCountParsed = parseUInt64(tokens[5]);
                if (!arrayBaseParsed.has_value() || !arrayCountParsed.has_value() || *arrayCountParsed == 0 || *arrayCountParsed > 1000000)
                {
                    return err("arrayBase/arrayCount 参数无效");
                }
                arrayBase = *arrayBaseParsed;
                arrayCount = static_cast<std::size_t>(*arrayCountParsed);
                filterStart = 6;
            }

            const std::string filterModule = (tokens.size() > filterStart) ? joinTokens(tokens, filterStart) : "";

            if (session->pointerManager.isScanning())
            {
                return err("当前已有指针扫描任务在运行");
            }

            // 串行执行指针扫描：与全局驱动请求锁配合，确保任意连接改 PID/发请求都按顺序进入。
            session->pointerManager.scan(
                pid,
                static_cast<uintptr_t>(*target),
                *depth,
                *maxOffset,
                useManual,
                static_cast<uintptr_t>(manualBase),
                useArray,
                static_cast<uintptr_t>(arrayBase),
                arrayCount,
                filterModule);

            return ok(std::format("done=1 count={}", session->pointerManager.count()));
        }

        if (command == "pointer.merge")
        {
            session->pointerManager.MergeBins();
            return ok("started=1");
        }

        if (command == "pointer.export")
        {
            session->pointerManager.ExportToTxt();
            return ok("done=1");
        }

        if (command == "memory.refresh")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }
            return ok(std::format("status={}", status));
        }

        if (command == "memory.summary")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }

            const auto &info = dr.GetMemoryInfoRef();
            return ok(std::format("status={} modules={} regions={}", status, info.module_count, info.region_count));
        }

        if (command == "module.list")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }

            const auto &info = dr.GetMemoryInfoRef();
            std::string payload = std::format("status={} count={}", status, info.module_count);
            for (int i = 0; i < info.module_count; ++i)
            {
                const auto &mod = info.modules[i];
                if (mod.name[0] == '\0')
                {
                    continue;
                }

                payload.append(std::format(";{}#{}", sanitizeLine(mod.name), mod.seg_count));
            }
            return ok(payload);
        }

        if (command == "memory.info.full")
        {
            const int status = dr.GetMemoryInformation();
            if (status != 0)
            {
                return err(std::format("刷新失败 status={}", status));
            }

            const auto &info = dr.GetMemoryInfoRef();
            const std::string jsonText = buildMemoryInfoJson(status, info).dump();
            return std::format("ok memory.info.full size={}\n{}", jsonText.size(), jsonText);
        }

        if (command == "module.addr")
        {
            if (tokens.size() != 4)
            {
                return err("用法: module.addr <模块名> <段索引> <start|end>");
            }

            const std::string moduleName = tokens[1];
            const auto segmentIndex = parseInt(tokens[2]);
            if (!segmentIndex.has_value())
            {
                return err("段索引无效");
            }

            const bool isStart = (tokens[3] == "start");
            const bool isEnd = (tokens[3] == "end");
            if (!isStart && !isEnd)
            {
                return err("第三个参数必须是 start 或 end");
            }

            std::uint64_t address = 0;
            const bool found = dr.GetModuleAddress(moduleName, static_cast<short>(*segmentIndex), &address, isStart);
            if (!found)
            {
                return err("未找到目标模块或段");
            }

            return ok(std::format("address=0x{:X}", address));
        }

        if (command == "mem.read")
        {
            if (tokens.size() != 3)
            {
                return err("用法: mem.read <地址> <大小>");
            }

            const auto address = parseUInt64(tokens[1]);
            const auto size = parseUInt64(tokens[2]);
            if (!address.has_value() || !size.has_value() || *size == 0 || *size > 4096)
            {
                return err("地址或大小无效，大小范围 1-4096");
            }

            std::vector<std::uint8_t> buffer(static_cast<std::size_t>(*size));
            const int status = dr.Read(*address, buffer.data(), buffer.size());
            if (status <= 0)
            {
                return err(std::format("读取失败 status={}", status));
            }

            return ok(std::format("hex={}", bytesToHex(buffer.data(), buffer.size())));
        }

        if (command == "mem.write")
        {
            if (tokens.size() < 3)
            {
                return err("用法: mem.write <地址> <HEX字节流>");
            }

            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
            {
                return err("地址无效");
            }

            const std::string hexText = joinTokens(tokens, 2);
            auto bytes = parseHexBytes(hexText);
            if (!bytes.has_value() || bytes->empty())
            {
                return err("HEX 字节流无效");
            }

            const bool success = dr.Write(*address, bytes->data(), bytes->size());
            if (!success)
            {
                return err("写入失败");
            }

            return ok(std::format("size={}", bytes->size()));
        }

        if (command == "mem.read_u8")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u8 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint8_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_u16")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u16 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint16_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_u32")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u32 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint32_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_u64")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_u64 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<std::uint64_t>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_f32")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_f32 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<float>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.read_f64")
        {
            if (tokens.size() != 2)
                return err("用法: mem.read_f64 <地址>");
            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
                return err("地址无效");
            const auto value = dr.Read<double>(*address);
            return ok(std::format("value={}", value));
        }

        if (command == "mem.write_u8")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u8 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value() || *value > 0xFF)
                return err("参数无效");
            if (!dr.Write<std::uint8_t>(*address, static_cast<std::uint8_t>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_u16")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u16 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value() || *value > 0xFFFF)
                return err("参数无效");
            if (!dr.Write<std::uint16_t>(*address, static_cast<std::uint16_t>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_u32")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u32 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value() || *value > 0xFFFFFFFFULL)
                return err("参数无效");
            if (!dr.Write<std::uint32_t>(*address, static_cast<std::uint32_t>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_u64")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_u64 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseUInt64(tokens[2]);
            if (!address.has_value() || !value.has_value())
                return err("参数无效");
            if (!dr.Write<std::uint64_t>(*address, *value))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_f32")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_f32 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseDouble(tokens[2]);
            if (!address.has_value() || !value.has_value())
                return err("参数无效");
            if (!dr.Write<float>(*address, static_cast<float>(*value)))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.write_f64")
        {
            if (tokens.size() != 3)
                return err("用法: mem.write_f64 <地址> <值>");
            const auto address = parseUInt64(tokens[1]);
            const auto value = parseDouble(tokens[2]);
            if (!address.has_value() || !value.has_value())
                return err("参数无效");
            if (!dr.Write<double>(*address, *value))
                return err("写入失败");
            return ok("写入成功");
        }

        if (command == "mem.read_str")
        {
            if (tokens.size() < 2 || tokens.size() > 3)
            {
                return err("用法: mem.read_str <地址> [最大长度]");
            }

            const auto address = parseUInt64(tokens[1]);
            if (!address.has_value())
            {
                return err("地址无效");
            }

            std::size_t maxLength = 128;
            if (tokens.size() == 3)
            {
                const auto value = parseUInt64(tokens[2]);
                if (!value.has_value() || *value == 0 || *value > 4096)
                {
                    return err("最大长度范围 1-4096");
                }
                maxLength = static_cast<std::size_t>(*value);
            }

            const std::string value = sanitizeLine(dr.ReadString(*address, maxLength));
            return ok(std::format("text={}", value));
        }

        if (command == "mem.read_wstr")
        {
            if (tokens.size() != 3)
            {
                return err("用法: mem.read_wstr <地址> <长度>");
            }

            const auto address = parseUInt64(tokens[1]);
            const auto length = parseUInt64(tokens[2]);
            if (!address.has_value() || !length.has_value() || *length == 0 || *length > 1024)
            {
                return err("地址或长度无效，长度范围 1-1024");
            }

            const std::string value = sanitizeLine(dr.ReadWString(*address, static_cast<std::size_t>(*length)));
            return ok(std::format("text={}", value));
        }

        if (command == "touch.down" || command == "touch.move")
        {
            if (tokens.size() != 5)
            {
                return err("用法: touch.down/touch.move <x> <y> <屏宽> <屏高>");
            }

            const auto x = parseInt(tokens[1]);
            const auto y = parseInt(tokens[2]);
            const auto screenW = parseInt(tokens[3]);
            const auto screenH = parseInt(tokens[4]);
            if (!x.has_value() || !y.has_value() || !screenW.has_value() || !screenH.has_value())
            {
                return err("坐标参数无效");
            }

            if (command == "touch.down")
            {
                dr.TouchDown(*x, *y, *screenW, *screenH);
                return ok("touch.down 已发送");
            }

            dr.TouchMove(*x, *y, *screenW, *screenH);
            return ok("touch.move 已发送");
        }

        if (command == "touch.up")
        {
            dr.TouchUp();
            return ok("touch.up 已发送");
        }

        return err("未知命令，发送 help 可查看命令列表");
    }

    // 将文本协议响应包装为统一 JSON 响应。
    json buildJsonResponseFromText(std::string_view textResponse)
    {
        json out;
        out["ok"] = false;

        if (textResponse.starts_with("err "))
        {
            out["error"] = std::string(textResponse.substr(4));
            return out;
        }

        if (!textResponse.starts_with("ok "))
        {
            out["error"] = "响应格式异常";
            out["raw"] = std::string(textResponse);
            return out;
        }

        out["ok"] = true;
        const std::string body(textResponse.substr(3));
        const auto newlinePos = body.find('\n');
        if (newlinePos == std::string::npos)
        {
            out["message"] = body;
            return out;
        }

        const std::string header = body.substr(0, newlinePos);
        const std::string payload = body.substr(newlinePos + 1);
        out["message"] = header;

        const auto parsedPayload = json::parse(payload, nullptr, false);
        if (!parsedPayload.is_discarded())
        {
            out["data"] = parsedPayload;
        }
        else
        {
            out["data_text"] = payload;
        }
        return out;
    }

    json makeProtocolError(std::string_view message, std::string_view operation = {})
    {
        json out = {
            {"ok", false},
            {"error", std::string(message)},
        };
        if (!operation.empty())
        {
            out["operation"] = std::string(operation);
        }
        return out;
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

    void appendCommandToken(std::string &command, const std::string &value)
    {
        command.push_back(' ');
        command.append(value);
    }

    json buildBridgeDescribePayload()
    {
        json payload;
        payload["protocol"] = "native-tcp-bridge";
        payload["request_shapes"] = json::array({
            {
                {"name", "structured"},
                {"fields", json::array({"operation", "params"})},
            },
        });
        payload["operations"] = json::array({
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
        return payload;
    }

    json tryDispatchStructuredOperation(const std::shared_ptr<ClientSession> &session, std::string_view operation, const json &params)
    {
        auto requireString = [&](std::string_view key, std::string_view desc) -> std::variant<std::string, json>
        {
            const auto value = getRequiredStringParam(params, key);
            if (!value.has_value() || value->empty())
            {
                return makeProtocolError(std::format("operation={} 缺少参数 {}", operation, desc), operation);
            }
            return *value;
        };

        auto optionalString = [&](std::string_view key) -> std::string
        {
            const auto value = getOptionalStringParam(params, key);
            return value.has_value() ? *value : "";
        };

        if (operation == "bridge.describe")
        {
            return json{
                {"ok", true},
                {"operation", std::string(operation)},
                {"message", "bridge.describe"},
                {"data", buildBridgeDescribePayload()},
            };
        }

        std::string textCommand;

        if (operation == "bridge.ping")
        {
            textCommand = "ping";
        }
        else if (operation == "target.pid.get")
        {
            const auto package = requireString("package_name", "package_name");
            if (std::holds_alternative<json>(package))
            {
                return std::get<json>(package);
            }
            textCommand = "pid.get";
            appendCommandToken(textCommand, std::get<std::string>(package));
        }
        else if (operation == "target.pid.set")
        {
            const auto pid = requireString("pid", "pid");
            if (std::holds_alternative<json>(pid))
            {
                return std::get<json>(pid);
            }
            textCommand = "pid.set";
            appendCommandToken(textCommand, std::get<std::string>(pid));
        }
        else if (operation == "target.pid.current")
        {
            textCommand = "pid.current";
        }
        else if (operation == "target.attach.package")
        {
            const auto package = requireString("package_name", "package_name");
            if (std::holds_alternative<json>(package))
            {
                return std::get<json>(package);
            }
            textCommand = "pid.attach";
            appendCommandToken(textCommand, std::get<std::string>(package));
        }
        else if (operation == "memory.info.full")
        {
            textCommand = "memory.info.full";
        }
        else if (operation == "module.resolve")
        {
            const auto moduleName = requireString("module_name", "module_name");
            const auto segmentIndex = requireString("segment_index", "segment_index");
            const auto which = requireString("which", "which");
            if (std::holds_alternative<json>(moduleName))
                return std::get<json>(moduleName);
            if (std::holds_alternative<json>(segmentIndex))
                return std::get<json>(segmentIndex);
            if (std::holds_alternative<json>(which))
                return std::get<json>(which);
            textCommand = "module.addr";
            appendCommandToken(textCommand, std::get<std::string>(moduleName));
            appendCommandToken(textCommand, std::get<std::string>(segmentIndex));
            appendCommandToken(textCommand, std::get<std::string>(which));
        }
        else if (operation == "scan.start" || operation == "scan.refine")
        {
            const auto type = requireString("value_type", "value_type");
            const auto mode = requireString("mode", "mode");
            if (std::holds_alternative<json>(type))
                return std::get<json>(type);
            if (std::holds_alternative<json>(mode))
                return std::get<json>(mode);

            const std::string typeValue = std::get<std::string>(type);
            const std::string modeValue = std::get<std::string>(mode);
            textCommand = (operation == "scan.start") ? "scan.first" : "scan.next";
            appendCommandToken(textCommand, typeValue);
            appendCommandToken(textCommand, modeValue);

            const std::string value = optionalString("value");
            if (modeValue != "unknown")
            {
                if (value.empty())
                {
                    return makeProtocolError(std::format("operation={} 在 mode={} 时必须提供 value", operation, modeValue), operation);
                }
                appendCommandToken(textCommand, value);
            }

            const std::string rangeMax = optionalString("range_max");
            const std::string modeLower = toLowerAscii(modeValue);
            const bool isStringMode = (modeLower == "string" || modeLower == "str");
            if (!rangeMax.empty() && !isStringMode)
            {
                if (modeValue == "unknown")
                {
                    appendCommandToken(textCommand, "0");
                }
                appendCommandToken(textCommand, rangeMax);
            }
        }
        else if (operation == "scan.status")
        {
            textCommand = "scan.status";
        }
        else if (operation == "scan.clear")
        {
            textCommand = "scan.clear";
        }
        else if (operation == "scan.page")
        {
            const auto start = requireString("start", "start");
            const auto count = requireString("count", "count");
            const auto type = requireString("value_type", "value_type");
            if (std::holds_alternative<json>(start))
                return std::get<json>(start);
            if (std::holds_alternative<json>(count))
                return std::get<json>(count);
            if (std::holds_alternative<json>(type))
                return std::get<json>(type);
            textCommand = "scan.page";
            appendCommandToken(textCommand, std::get<std::string>(start));
            appendCommandToken(textCommand, std::get<std::string>(count));
            appendCommandToken(textCommand, std::get<std::string>(type));
        }
        else if (operation == "viewer.open")
        {
            const auto address = requireString("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            textCommand = "viewer.open";
            appendCommandToken(textCommand, std::get<std::string>(address));
            const std::string viewFormat = optionalString("view_format");
            if (!viewFormat.empty())
            {
                appendCommandToken(textCommand, viewFormat);
            }
        }
        else if (operation == "viewer.move")
        {
            const auto lines = requireString("lines", "lines");
            if (std::holds_alternative<json>(lines))
                return std::get<json>(lines);
            textCommand = "viewer.move";
            appendCommandToken(textCommand, std::get<std::string>(lines));
            const std::string step = optionalString("step");
            if (!step.empty())
            {
                appendCommandToken(textCommand, step);
            }
        }
        else if (operation == "viewer.offset")
        {
            const auto offset = requireString("offset", "offset");
            if (std::holds_alternative<json>(offset))
                return std::get<json>(offset);
            textCommand = "viewer.offset";
            appendCommandToken(textCommand, std::get<std::string>(offset));
        }
        else if (operation == "viewer.set_format")
        {
            const auto viewFormat = requireString("view_format", "view_format");
            if (std::holds_alternative<json>(viewFormat))
                return std::get<json>(viewFormat);
            textCommand = "viewer.format";
            appendCommandToken(textCommand, std::get<std::string>(viewFormat));
        }
        else if (operation == "viewer.snapshot")
        {
            textCommand = "viewer.get";
        }
        else if (operation == "pointer.status")
        {
            textCommand = "pointer.status";
        }
        else if (operation == "pointer.scan")
        {
            const std::string mode = optionalString("mode").empty() ? "module" : optionalString("mode");
            const auto target = requireString("target", "target");
            const auto depth = requireString("depth", "depth");
            const auto maxOffset = requireString("max_offset", "max_offset");
            if (std::holds_alternative<json>(target))
                return std::get<json>(target);
            if (std::holds_alternative<json>(depth))
                return std::get<json>(depth);
            if (std::holds_alternative<json>(maxOffset))
                return std::get<json>(maxOffset);

            if (mode == "manual")
            {
                const auto manualBase = requireString("manual_base", "manual_base");
                if (std::holds_alternative<json>(manualBase))
                    return std::get<json>(manualBase);
                textCommand = "pointer.scan.manual";
                appendCommandToken(textCommand, std::get<std::string>(target));
                appendCommandToken(textCommand, std::get<std::string>(depth));
                appendCommandToken(textCommand, std::get<std::string>(maxOffset));
                appendCommandToken(textCommand, std::get<std::string>(manualBase));
            }
            else if (mode == "array")
            {
                const auto arrayBase = requireString("array_base", "array_base");
                const auto arrayCount = requireString("array_count", "array_count");
                if (std::holds_alternative<json>(arrayBase))
                    return std::get<json>(arrayBase);
                if (std::holds_alternative<json>(arrayCount))
                    return std::get<json>(arrayCount);
                textCommand = "pointer.scan.array";
                appendCommandToken(textCommand, std::get<std::string>(target));
                appendCommandToken(textCommand, std::get<std::string>(depth));
                appendCommandToken(textCommand, std::get<std::string>(maxOffset));
                appendCommandToken(textCommand, std::get<std::string>(arrayBase));
                appendCommandToken(textCommand, std::get<std::string>(arrayCount));
            }
            else
            {
                textCommand = "pointer.scan";
                appendCommandToken(textCommand, std::get<std::string>(target));
                appendCommandToken(textCommand, std::get<std::string>(depth));
                appendCommandToken(textCommand, std::get<std::string>(maxOffset));
            }

            const std::string moduleFilter = optionalString("module_filter");
            if (!moduleFilter.empty())
            {
                appendCommandToken(textCommand, moduleFilter);
            }
        }
        else if (operation == "pointer.merge")
        {
            textCommand = "pointer.merge";
        }
        else if (operation == "pointer.export")
        {
            textCommand = "pointer.export";
        }
        else if (operation == "breakpoint.info")
        {
            textCommand = "hwbp.info";
        }
        else if (operation == "breakpoint.set")
        {
            const auto address = requireString("address", "address");
            const auto bpType = requireString("bp_type", "bp_type");
            const auto bpScope = requireString("bp_scope", "bp_scope");
            const auto length = requireString("length", "length");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(bpType))
                return std::get<json>(bpType);
            if (std::holds_alternative<json>(bpScope))
                return std::get<json>(bpScope);
            if (std::holds_alternative<json>(length))
                return std::get<json>(length);
            textCommand = "hwbp.set";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(bpType));
            appendCommandToken(textCommand, std::get<std::string>(bpScope));
            appendCommandToken(textCommand, std::get<std::string>(length));
        }
        else if (operation == "breakpoint.clear")
        {
            textCommand = "hwbp.remove";
        }
        else if (operation == "breakpoint.record.remove")
        {
            const auto index = requireString("index", "index");
            if (std::holds_alternative<json>(index))
                return std::get<json>(index);
            textCommand = "hwbp.record.remove";
            appendCommandToken(textCommand, std::get<std::string>(index));
        }
        else if (operation == "breakpoint.record.update")
        {
            const auto index = requireString("index", "index");
            const auto field = requireString("field", "field");
            const auto value = requireString("value", "value");
            if (std::holds_alternative<json>(index))
                return std::get<json>(index);
            if (std::holds_alternative<json>(field))
                return std::get<json>(field);
            if (std::holds_alternative<json>(value))
                return std::get<json>(value);
            textCommand = "hwbp.record.set";
            appendCommandToken(textCommand, std::get<std::string>(index));
            appendCommandToken(textCommand, std::get<std::string>(field));
            appendCommandToken(textCommand, std::get<std::string>(value));
        }
        else if (operation == "breakpoint.record.set_float")
        {
            const auto index = requireString("index", "index");
            const auto field = requireString("field", "field");
            const auto value = requireString("value", "value");
            const auto precision = optionalString("precision");
            if (std::holds_alternative<json>(index))
                return std::get<json>(index);
            if (std::holds_alternative<json>(field))
                return std::get<json>(field);
            if (std::holds_alternative<json>(value))
                return std::get<json>(value);
            std::string prec = "f32";
            if (!precision.empty())
            {
                prec = precision;
                if (prec != "f32" && prec != "f64")
                    prec = "f32";
            }
            textCommand = "hwbp.record.set." + prec;
            appendCommandToken(textCommand, std::get<std::string>(index));
            appendCommandToken(textCommand, std::get<std::string>(field));
            appendCommandToken(textCommand, std::get<std::string>(value));
        }
        else if (operation == "signature.scan_address")
        {
            const auto address = requireString("address", "address");
            const auto range = requireString("range", "range");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(range))
                return std::get<json>(range);
            textCommand = "sig.scan.addr";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(range));
            const std::string fileName = optionalString("file_name");
            if (!fileName.empty())
            {
                appendCommandToken(textCommand, fileName);
            }
        }
        else if (operation == "signature.scan_file")
        {
            textCommand = "sig.scan.file";
            const std::string fileName = optionalString("file_name");
            if (!fileName.empty())
            {
                appendCommandToken(textCommand, fileName);
            }
        }
        else if (operation == "signature.scan_pattern")
        {
            const auto rangeOffset = requireString("range_offset", "range_offset");
            const auto pattern = requireString("pattern", "pattern");
            if (std::holds_alternative<json>(rangeOffset))
                return std::get<json>(rangeOffset);
            if (std::holds_alternative<json>(pattern))
                return std::get<json>(pattern);
            textCommand = "sig.scan.pattern";
            appendCommandToken(textCommand, std::get<std::string>(rangeOffset));
            appendCommandToken(textCommand, std::get<std::string>(pattern));
        }
        else if (operation == "signature.filter")
        {
            const auto address = requireString("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            textCommand = "sig.filter";
            appendCommandToken(textCommand, std::get<std::string>(address));
            const std::string fileName = optionalString("file_name");
            if (!fileName.empty())
            {
                appendCommandToken(textCommand, fileName);
            }
        }
        else if (operation == "lock.set")
        {
            const auto address = requireString("address", "address");
            const auto valueType = requireString("value_type", "value_type");
            const auto value = requireString("value", "value");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(valueType))
                return std::get<json>(valueType);
            if (std::holds_alternative<json>(value))
                return std::get<json>(value);
            textCommand = "lock.set";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(valueType));
            appendCommandToken(textCommand, std::get<std::string>(value));
        }
        else if (operation == "lock.unset")
        {
            const auto address = requireString("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            textCommand = "lock.unset";
            appendCommandToken(textCommand, std::get<std::string>(address));
        }
        else if (operation == "lock.status")
        {
            const auto address = requireString("address", "address");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            textCommand = "lock.status";
            appendCommandToken(textCommand, std::get<std::string>(address));
        }
        else if (operation == "lock.clear")
        {
            textCommand = "lock.clear";
        }
        else if (operation == "memory.read_block")
        {
            const auto address = requireString("address", "address");
            const auto size = requireString("size", "size");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(size))
                return std::get<json>(size);
            textCommand = "mem.read";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(size));
        }
        else if (operation == "memory.read_value")
        {
            const auto address = requireString("address", "address");
            const auto valueType = requireString("value_type", "value_type");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(valueType))
                return std::get<json>(valueType);

            static const std::unordered_map<std::string, std::string> valueCommandMap = {
                {"u8", "mem.read_u8"},
                {"u16", "mem.read_u16"},
                {"u32", "mem.read_u32"},
                {"u64", "mem.read_u64"},
                {"f32", "mem.read_f32"},
                {"f64", "mem.read_f64"},
            };

            const auto commandIt = valueCommandMap.find(std::get<std::string>(valueType));
            if (commandIt == valueCommandMap.end())
            {
                return makeProtocolError("memory.read_value 的 value_type 仅支持 u8/u16/u32/u64/f32/f64", operation);
            }

            textCommand = commandIt->second;
            appendCommandToken(textCommand, std::get<std::string>(address));
        }
        else if (operation == "memory.write_block")
        {
            const auto address = requireString("address", "address");
            const auto dataHex = requireString("data_hex", "data_hex");
            if (std::holds_alternative<json>(address))
                return std::get<json>(address);
            if (std::holds_alternative<json>(dataHex))
                return std::get<json>(dataHex);
            textCommand = "mem.write";
            appendCommandToken(textCommand, std::get<std::string>(address));
            appendCommandToken(textCommand, std::get<std::string>(dataHex));
        }
        else
        {
            return makeProtocolError(std::format("未知 operation: {}", operation), operation);
        }

        const std::string textResponse = DispatchTextCommand(session, textCommand);
        json out = buildJsonResponseFromText(textResponse);
        out["session_id"] = session->sessionId;
        out["operation"] = std::string(operation);
        return out;
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
                    return makeProtocolError("params 字段必须是对象", operationName).dump();
                }
                params = parsedReq["params"];
            }

            return tryDispatchStructuredOperation(session, operationName, params).dump();
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

    Utils::GlobalPool.force_stop();

    std::println("服务端已退出。");
    return 0;
}
