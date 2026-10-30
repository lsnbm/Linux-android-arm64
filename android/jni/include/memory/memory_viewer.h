#pragma once
#include "../base/memory_utils.h"
#include "../architecture/disassembler.h"

class MemViewer
{
private:
    uintptr_t base_ = 0;
    Types::ViewFormat format_ = Types::ViewFormat::Hexadecimal;
    std::vector<uint8_t> buffer_;
    bool readSuccess_ = false;
    int readStatus_ = 0;
    std::vector<Disasm::DisasmLine> disasmCache_;
    std::future<std::vector<Disasm::DisasmLine>> disasmFuture_;
    bool disasmBusy_ = false;

public:
    MemViewer() : buffer_(Config::Constants::MEM_VIEW_DEFAULT_BYTES)
    {
    }

    // 返回当前内存浏览格式。
    Types::ViewFormat format() const noexcept
    {
        return format_;
    }
    // 返回最近一次读取是否成功。
    bool readSuccess() const noexcept
    {
        return readSuccess_;
    }
    int readStatus() const noexcept
    {
        return readStatus_;
    }
    // 返回当前浏览基址。
    uintptr_t base() const noexcept
    {
        return base_;
    }
    const std::vector<uint8_t> &buffer() const noexcept
    {
        return buffer_;
    }
    const std::vector<Disasm::DisasmLine> &getDisasm() const noexcept
    {
        return disasmCache_;
    }
    bool disasmBusy() const noexcept
    {
        return disasmBusy_;
    }

    void waitDisasm()
    {
        if (!disasmFuture_.valid()) return;

        try
        {
            disasmCache_ = disasmFuture_.get();
        }
        catch (...)
        {
            disasmCache_.clear();
        }
        disasmBusy_ = false;
    }

    void pollDisasm()
    {
        if (disasmFuture_.valid() && disasmFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) waitDisasm();
    }

    void clear()
    {
        waitDisasm();
        base_ = 0;
        readSuccess_ = false;
        readStatus_ = 0;
        std::ranges::fill(buffer_, 0);
        disasmCache_.clear();
    }

    // 重新读取并刷新当前浏览缓存。
    void refresh()
    {
        if (!MemUtils::IsValidAddr(base_))
        {
            readSuccess_ = false;
            readStatus_ = -EINVAL;
            disasmBusy_ = false;
            disasmCache_.clear();
            return;
        }
        std::ranges::fill(buffer_, 0);
        const int readBytes = dr->Read(base_, buffer_.data(), buffer_.size());
        readSuccess_ = readBytes > 0;
        readStatus_ = readBytes;
        if (!readSuccess_)
        {
            disasmBusy_ = false;
            disasmCache_.clear();
            return;
        }
        if (format_ == Types::ViewFormat::Disasm)
        {
            disasmCache_.clear();
            disasmBusy_ = false;
            const size_t disasmSize = std::min(static_cast<size_t>(readBytes), buffer_.size()) & ~static_cast<size_t>(3);
            if (disasmSize > 0)
            {
                try
                {
                    disasmFuture_ = Config::CpuThreadPool().submit_task(
                        [base = base_, bytes = buffer_, disasmSize]
                        {
                            Disasm::Disassembler disasm;
                            if (!disasm.IsValid()) return std::vector<Disasm::DisasmLine>{};
                            return disasm.Disassemble(base, bytes.data(), disasmSize, Disasm::Disassembler::DEFAULT_MAX_INSTRUCTIONS, true);
                        });
                    disasmBusy_ = true;
                }
                catch (...)
                {
                    disasmCache_.clear();
                }
            }
        }
        else
        {
            disasmBusy_ = false;
            disasmCache_.clear();
        }
    }

    void setFormat(Types::ViewFormat fmt)
    {
        format_ = fmt;
        refresh();
    }

    void open(uintptr_t addr, std::optional<Types::ViewFormat> format = std::nullopt)
    {
        if (format.has_value()) format_ = *format;
        if (format_ == Types::ViewFormat::Disasm) addr &= ~static_cast<uintptr_t>(3);
        base_ = addr;
        refresh();
    }

    // 按指定方向应用无符号字节偏移。
    void applyOffset(uintptr_t offset, bool negative)
    {
        if (negative) base_ = base_ < offset ? 0 : base_ - offset;
        else base_ = offset > std::numeric_limits<uintptr_t>::max() - base_ ? std::numeric_limits<uintptr_t>::max() : base_ + offset;
        if (format_ == Types::ViewFormat::Disasm) base_ &= ~static_cast<uintptr_t>(3);
        refresh();
    }

    // 按有符号字节偏移调整当前浏览基址。
    void applyOffset(int64_t offset)
    {
        const auto magnitude = offset < 0 ? static_cast<uint64_t>(-(offset + 1)) + 1 : static_cast<uint64_t>(offset);
        applyOffset(static_cast<uintptr_t>(magnitude), offset < 0);
    }

    // 解析偏移字符串后调整当前浏览基址。
    bool applyOffset(std::string_view offsetStr)
    {
        auto result = MemUtils::ParseHexOffset(offsetStr);
        if (!result) return false;
        applyOffset(result->offset, result->negative);
        return true;
    }
};
