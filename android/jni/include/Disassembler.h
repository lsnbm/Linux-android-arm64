#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <cstdlib>
#include "../capstone/include/capstone/capstone.h"

namespace Disasm
{

    struct DisasmLine
    {
        bool valid = false;
        uint64_t address = 0;
        size_t size = 0;
        uint8_t bytes[16] = {0};
        char mnemonic[32] = {0};
        char op_str[160] = {0};
    };

    /*
     作用: 规范化 Capstone 输出的汇编操作数字符串。
     说明: 去掉空白并转成大写，方便后续用简单字符串匹配处理
           "LDR X8, [X20,#0x1900]" / "ldr x8, [x20, #0x1900]" 这类格式差异。
    */
    static inline void NormalizeAsmText(const char *in, char *out, size_t outSize)
    {
        if (!out || outSize == 0) return;

        size_t n = 0;
        if (in)
        {
            for (const char *p = in; *p && n + 1 < outSize; ++p)
            {
                unsigned char ch = static_cast<unsigned char>(*p);
                if (std::isspace(ch)) continue;
                out[n++] = static_cast<char>(std::toupper(ch));
            }
        }
        out[n] = '\0';
    }

    /*
     作用: 从当前位置解析一个十六进制/十进制立即数。
     说明: 用于解析 Capstone 操作数里的 "#0x1D30"、"0x767FFC70C0" 等文本片段。
    */
    static inline bool ParseNumberAt(const char *p, uint64_t *outValue)
    {
        if (!p || !outValue) return false;

        char *end = nullptr;
        uint64_t value = std::strtoull(p, &end, 0);
        if (end == p) return false;

        *outValue = value;
        return true;
    }

    /*
     作用: 从一行操作数字符串里提取第一个 0x 开头的值。
     说明: 主要用于 BL 目标地址，如 "BL 0x767FFC7CDC"，也用于快速扫描带立即数的操作数。
    */
    static inline bool ParseFirstHex(const char *text, uint64_t *outValue)
    {
        if (!text || !outValue) return false;

        char norm[192]{};
        NormalizeAsmText(text, norm, sizeof(norm));
        const char *p = std::strstr(norm, "0X");
        if (!p) return false;

        return ParseNumberAt(p, outValue);
    }

    /*
     作用: 在指定关键字后提取立即数。
     说明: 例如从 "MOV W8,#0xFEE4" 按 "W8," 提取 0xFEE4，
           或从 "MOVK W8,#0xC444,LSL#0x10" 按 "LSL" 提取 0x10。
    */
    static inline bool ParseHashImmAfter(const char *text, const char *needle, uint64_t *outValue)
    {
        if (!text || !needle || !outValue) return false;

        char norm[192]{};
        char key[64]{};
        NormalizeAsmText(text, norm, sizeof(norm));
        NormalizeAsmText(needle, key, sizeof(key));

        const char *p = std::strstr(norm, key);
        if (!p) return false;

        p += std::strlen(key);
        if (*p == '#') ++p;

        return ParseNumberAt(p, outValue);
    }

    /*
     作用: 从形如 [baseReg,#imm] 的内存操作数中提取字段偏移。
     说明: 例如从 "LDR X1,[X20,#0x1900]" 提取 provider 偏移 0x1900；
           没有 #imm 的 "[X8]" 会返回 offset=0，表示寄存器当前已经指向字段 holder。
    */
    static inline bool ParseMemOffsetForBase(const char *op, const char *baseReg, uint64_t *outOffset)
    {
        if (!op || !baseReg || !outOffset) return false;

        char norm[192]{};
        char base[16]{};
        NormalizeAsmText(op, norm, sizeof(norm));
        NormalizeAsmText(baseReg, base, sizeof(base));

        char pattern[24]{};
        std::snprintf(pattern, sizeof(pattern), "[%s", base);

        const char *p = std::strstr(norm, pattern);
        if (!p) return false;

        const char *end = std::strchr(p, ']');
        if (!end) return false;

        const char *imm = std::strstr(p, "#0X");
        if (!imm || imm > end)
        {
            *outOffset = 0;
            return true;
        }

        return ParseNumberAt(imm + 1, outOffset);
    }

    /*
     作用: 判断规范化后的操作数字符串是否以某个寄存器/片段开头。
     说明: 用来区分 "LDR X9,..."、"LDR W1,..." 这些同 mnemonic 不同语义的指令。
    */
    static inline bool OpStartsWith(const char *op, const char *prefix)
    {
        if (!op || !prefix) return false;

        char norm[192]{};
        char key[64]{};
        NormalizeAsmText(op, norm, sizeof(norm));
        NormalizeAsmText(prefix, key, sizeof(key));
        return std::strncmp(norm, key, std::strlen(key)) == 0;
    }

    /*
     作用: 判断规范化后的操作数字符串是否包含某个片段。
     说明: 用于识别 Decode/Encode 里的 LSL/LSR 移位形式。
    */
    static inline bool OpContains(const char *op, const char *needle)
    {
        if (!op || !needle) return false;

        char norm[192]{};
        char key[64]{};
        NormalizeAsmText(op, norm, sizeof(norm));
        NormalizeAsmText(needle, key, sizeof(key));
        return std::strstr(norm, key) != nullptr;
    }

    class Disassembler
    {
    public:
        static constexpr size_t DEFAULT_MAX_INSTRUCTIONS = 500;

        Disassembler() : m_handle(0), m_valid(false)
        {
            int major = 0, minor = 0;
            cs_version(&major, &minor);
            printf("[*] Capstone 版本: %d.%d\n", major, minor);

            if (!cs_support(CS_ARCH_AARCH64))
            {
                printf("[-] 致命错误: Capstone 库未编译 ARM64/AArch64 支持！\n");
                return;
            }

            cs_err err = cs_open(CS_ARCH_AARCH64, CS_MODE_LITTLE_ENDIAN, &m_handle);
            if (err != CS_ERR_OK)
            {
                printf("[-] ARM64 初始化失败: %s\n", cs_strerror(err));
                return;
            }

            cs_option(m_handle, CS_OPT_DETAIL, CS_OPT_OFF);
            cs_option(m_handle, CS_OPT_SKIPDATA, CS_OPT_ON);

            m_valid = true;
            printf("[+] 反汇编器初始化成功: ARM64\n");
        }

        ~Disassembler()
        {
            if (m_valid && m_handle)
            {
                cs_close(&m_handle);
            }
        }

        Disassembler(const Disassembler &) = delete;
        Disassembler &operator=(const Disassembler &) = delete;

        bool IsValid() const
        {
            return m_valid;
        }

        const char *GetLastError() const
        {
            if (!m_valid) return "反汇编器未初始化";
            return cs_strerror(cs_errno(m_handle));
        }

        std::vector<DisasmLine> Disassemble(uint64_t address, const uint8_t *buffer, size_t size, size_t maxCount = DEFAULT_MAX_INSTRUCTIONS, bool logInstructions = false)
        {
            std::vector<DisasmLine> results;

            if (!m_valid)
            {
                printf("[-] 反汇编器未初始化\n");
                return results;
            }

            if (address & 0x3)
            {
                printf("[-] 错误: 地址未4字节对齐 (0x%llX)\n", (unsigned long long)address);
                return results;
            }

            cs_insn *insn = nullptr;
            size_t count = cs_disasm(m_handle, buffer, size, address, maxCount, &insn);

            if (count == 0)
            {
                printf("[-] 反汇编失败: %s\n", cs_strerror(cs_errno(m_handle)));
                return results;
            }

            // 输出反汇编结果
            if (logInstructions) printf("[*] 反汇编 %zu 条指令:\n", count);
            for (size_t i = 0; i < count; i++)
            {
                if (!logInstructions) continue;
                // 原始字节
                char bytesStr[48] = {0};
                int pos = 0;
                for (size_t j = 0; j < insn[i].size; j++) pos += snprintf(bytesStr + pos, sizeof(bytesStr) - pos, "%02X ", insn[i].bytes[j]);
                if (pos > 0) bytesStr[pos - 1] = '\0';

                // 大写化
                char mn[32] = {0}, op[160] = {0};
                strncpy(mn, insn[i].mnemonic, sizeof(mn) - 1);
                strncpy(op, insn[i].op_str, sizeof(op) - 1);
                for (char *p = mn; *p; ++p) *p = std::toupper(static_cast<unsigned char>(*p));
                for (char *p = op; *p; ++p) *p = std::toupper(static_cast<unsigned char>(*p));

                printf("  0x%llX:  %-12s  %-7s %s\n", (unsigned long long)insn[i].address, bytesStr, mn, op);
            }

            // 填充结果
            results.reserve(count);
            for (size_t i = 0; i < count; i++)
            {
                DisasmLine line;
                line.valid = true;
                line.address = insn[i].address;
                line.size = insn[i].size;

                size_t copyLen = (insn[i].size < sizeof(line.bytes)) ? insn[i].size : sizeof(line.bytes);
                memcpy(line.bytes, insn[i].bytes, copyLen);

                strncpy(line.mnemonic, insn[i].mnemonic, sizeof(line.mnemonic) - 1);
                strncpy(line.op_str, insn[i].op_str, sizeof(line.op_str) - 1);

                for (char *p = line.mnemonic; *p; ++p) *p = std::toupper(static_cast<unsigned char>(*p));
                for (char *p = line.op_str; *p; ++p) *p = std::toupper(static_cast<unsigned char>(*p));

                results.push_back(line);
            }

            cs_free(insn, count);
            return results;
        }

    private:
        csh m_handle;
        bool m_valid;
    };

} // namespace Disasm
