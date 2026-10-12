#pragma once
#include <algorithm>
#include <vector>
#include <string>
#include <cstdint>
#include <cstring>
#include <cstdio>
#include <cctype>
#include "../../capstone/include/capstone/capstone.h"

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
        static constexpr size_t DEFAULT_MAX_INSTRUCTIONS = 500;

        Disassembler() : m_handle(0), m_valid(false)
        {
            int major = 0, minor = 0;
            cs_version(&major, &minor);
            LS_LOGI_TAG("Disassembler", "Capstone 版本: %d.%d", major, minor);

            if (!cs_support(CS_ARCH_AARCH64))
            {
                LS_LOGE_TAG("Disassembler", "Capstone 库未编译 ARM64/AArch64 支持");
                return;
            }

            cs_err err = cs_open(CS_ARCH_AARCH64, CS_MODE_LITTLE_ENDIAN, &m_handle);
            if (err != CS_ERR_OK)
            {
                LS_LOGE_TAG("Disassembler", "ARM64 初始化失败: %s", cs_strerror(err));
                return;
            }

            cs_option(m_handle, CS_OPT_DETAIL, CS_OPT_OFF);
            cs_option(m_handle, CS_OPT_SKIPDATA, CS_OPT_ON);

            m_valid = true;
            LS_LOGI_TAG("Disassembler", "初始化成功: ARM64");
        }

        ~Disassembler()
        {
            if (m_handle)
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

        std::vector<DisasmLine> Disassemble(uint64_t address, const uint8_t *buffer, size_t size, size_t maxCount = DEFAULT_MAX_INSTRUCTIONS, bool logInstructions = false)
        {
            std::vector<DisasmLine> results;

            if (!m_valid)
            {
                LS_LOGE_TAG("Disassembler", "反汇编器未初始化");
                return results;
            }

            if (address & 0x3)
            {
                LS_LOGE_TAG("Disassembler", "地址未 4 字节对齐: 0x%llX", (unsigned long long)address);
                return results;
            }

            cs_insn *insn = nullptr;
            size_t count = cs_disasm(m_handle, buffer, size, address, maxCount, &insn);

            if (count == 0)
            {
                LS_LOGE_TAG("Disassembler", "反汇编失败: %s", cs_strerror(cs_errno(m_handle)));
                return results;
            }

            if (logInstructions) LS_LOGI_TAG("Disassembler", "反汇编 %zu 条指令", count);
            results.reserve(count);
            for (size_t i = 0; i < count; i++)
            {
                DisasmLine line{};
                line.valid = true;
                line.address = insn[i].address;
                line.size = insn[i].size;

                const size_t copyLen = std::min(static_cast<size_t>(insn[i].size), sizeof(line.bytes));
                memcpy(line.bytes, insn[i].bytes, copyLen);

                strncpy(line.mnemonic, insn[i].mnemonic, sizeof(line.mnemonic) - 1);
                strncpy(line.op_str, insn[i].op_str, sizeof(line.op_str) - 1);

                for (char *p = line.mnemonic; *p; ++p) *p = std::toupper(static_cast<unsigned char>(*p));
                for (char *p = line.op_str; *p; ++p) *p = std::toupper(static_cast<unsigned char>(*p));

                if (logInstructions)
                {
                    char bytesStr[48] = {};
                    int pos = 0;
                    const size_t byteCount = std::min(static_cast<size_t>(insn[i].size), (sizeof(bytesStr) - 1) / 3);
                    for (size_t j = 0; j < byteCount; ++j)
                    {
                        const int written = snprintf(bytesStr + pos, sizeof(bytesStr) - static_cast<size_t>(pos), "%02X ", insn[i].bytes[j]);
                        if (written <= 0) break;
                        pos += std::min(written, static_cast<int>(sizeof(bytesStr) - 1 - static_cast<size_t>(pos)));
                    }
                    if (pos > 0) bytesStr[pos - 1] = '\0';
                    LS_LOGI_TAG("Disassembler", "0x%llX: %-12s %-7s %s", (unsigned long long)line.address, bytesStr, line.mnemonic, line.op_str);
                }

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
