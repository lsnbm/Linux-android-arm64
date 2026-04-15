#pragma once
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cctype>
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

    class Disassembler
    {
    public:
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

        bool IsValid() const { return m_valid; }

        const char *GetLastError() const
        {
            if (!m_valid)
                return "反汇编器未初始化";
            return cs_strerror(cs_errno(m_handle));
        }

        std::vector<DisasmLine> Disassemble(uint64_t address, const uint8_t *buffer,
                                            size_t size, size_t maxCount = 0,
                                            bool logInstructions = false)
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
            if (logInstructions)
                printf("[*] 反汇编 %zu 条指令:\n", count);
            for (size_t i = 0; i < count; i++)
            {
                if (!logInstructions)
                    continue;
                // 原始字节
                char bytesStr[48] = {0};
                int pos = 0;
                for (size_t j = 0; j < insn[i].size; j++)
                    pos += snprintf(bytesStr + pos, sizeof(bytesStr) - pos, "%02X ", insn[i].bytes[j]);
                if (pos > 0)
                    bytesStr[pos - 1] = '\0';

                // 大写化
                char mn[32] = {0}, op[160] = {0};
                strncpy(mn, insn[i].mnemonic, sizeof(mn) - 1);
                strncpy(op, insn[i].op_str, sizeof(op) - 1);
                for (char *p = mn; *p; ++p)
                    *p = std::toupper(static_cast<unsigned char>(*p));
                for (char *p = op; *p; ++p)
                    *p = std::toupper(static_cast<unsigned char>(*p));

                printf("  0x%llX:  %-12s  %-7s %s\n",
                       (unsigned long long)insn[i].address,
                       bytesStr, mn, op);
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

                for (char *p = line.mnemonic; *p; ++p)
                    *p = std::toupper(static_cast<unsigned char>(*p));
                for (char *p = line.op_str; *p; ++p)
                    *p = std::toupper(static_cast<unsigned char>(*p));

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
