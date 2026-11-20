#pragma once
#include <memory>
#include <future>
#include <span>
#include "../base/memory_utils.h"
#include "../architecture/disassembler.h"
#include "../../thread-pool/include/BS_thread_pool.hpp"

class MemViewer
{
private:
    uintptr_t base_ = 0;
    Types::ViewFormat format_ = Types::ViewFormat::Hexadecimal;
    std::vector<uint8_t> buffer_;
    int readStatus_ = 0;
    std::vector<Disasm::DisasmLine> disasmCache_;
    std::optional<BS::thread_pool<>> worker_;
    std::future<std::vector<Disasm::DisasmLine>> disasmFuture_;

public:
    MemViewer() : buffer_(Config::Constants::MEM_VIEW_DEFAULT_BYTES)
    {
    }

    ~MemViewer()
    {
        // 包括 refresh/clear 已丢弃 future、但仍在执行的任务。
        // main 必须先销毁 Viewer，再销毁全局 Disassembler。
        if (worker_)
        {
            worker_->purge();
            worker_->wait();
        }
    }

    // 返回当前内存浏览格式。
    Types::ViewFormat format() const noexcept
    {
        return format_;
    }
    // 返回最近一次读取是否成功。
    bool readSuccess() const noexcept
    {
        return readStatus_ > 0;
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
    std::span<const uint8_t> buffer() const noexcept
    {
        return {buffer_.data(), readSuccess() ? std::min(static_cast<size_t>(readStatus_), buffer_.size()) : 0};
    }
    const std::vector<Disasm::DisasmLine> &getDisasm() const noexcept
    {
        return disasmCache_;
    }
    bool disasmBusy() const noexcept
    {
        return disasmFuture_.valid();
    }

    int waitDisasm()
    {
        int status = 0;
        if (!disasmFuture_.valid()) return status;
        try
        {
            disasmCache_ = disasmFuture_.get();
        }
        catch (const std::bad_alloc &)
        {
            status = -ENOMEM;
            return status;
        }
        catch (const std::system_error &error)
        {
            status = error.code().value() ? -error.code().value() : -EIO;
            return status;
        }
        catch (...)
        {
            status = -EIO;
            return status;
        }
        return status;
    }

    int pollDisasm()
    {
        int status = 0;
        if (disasmFuture_.valid() && disasmFuture_.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready) status = waitDisasm();
        return status;
    }

    void clear()
    {
        if (worker_) worker_->purge();
        disasmFuture_ = {};
        base_ = 0;
        readStatus_ = 0;
        std::ranges::fill(buffer_, 0);
        disasmCache_.clear();
    }

    // 重新读取并刷新当前浏览缓存。
    int refresh()
    {
        int status = 0;
        if (worker_) worker_->purge();
        disasmFuture_ = {};
        disasmCache_.clear();
        if (!MemUtils::IsValidAddr(base_))
        {
            readStatus_ = -EINVAL;
            status = readStatus_;
            return status;
        }
        std::ranges::fill(buffer_, 0);
        readStatus_ = dr->Read(base_, buffer_.data(), buffer_.size());
        if (!readSuccess())
        {
            status = readStatus_ < 0 ? readStatus_ : -EIO;
            return status;
        }
        if (format_ != Types::ViewFormat::Disasm) return status;

        const size_t disasmSize = std::min(static_cast<size_t>(readStatus_), buffer_.size()) & ~static_cast<size_t>(3);
        if (disasmSize == 0) return status;
        if (!Disasm::gDisassembler)
        {
            status = -ENODEV;
            return status;
        }
        try
        {
            if (!worker_) worker_.emplace(1);
            disasmFuture_ = worker_->submit_task(
                [base = base_, bytes = buffer_, disasmSize]
                {
                    return Disasm::gDisassembler->Disassemble(base, bytes.data(), disasmSize, Disasm::Disassembler::DEFAULT_MAX_INSTRUCTIONS, true);
                });
        }
        catch (const std::bad_alloc &)
        {
            status = -ENOMEM;
            return status;
        }
        catch (const std::system_error &error)
        {
            status = error.code().value() ? -error.code().value() : -EIO;
            return status;
        }
        catch (...)
        {
            status = -EIO;
            return status;
        }
        return status;
    }

    int setFormat(Types::ViewFormat fmt)
    {
        int status = 0;
        format_ = fmt;
        if (format_ == Types::ViewFormat::Disasm) base_ &= ~static_cast<uintptr_t>(3);
        status = refresh();
        return status;
    }

    int open(uintptr_t addr, std::optional<Types::ViewFormat> format = std::nullopt)
    {
        int status = 0;
        if (format.has_value()) format_ = *format;
        if (format_ == Types::ViewFormat::Disasm) addr &= ~static_cast<uintptr_t>(3);
        base_ = addr;
        status = refresh();
        return status;
    }

    // 按指定方向应用无符号字节偏移。
    int applyOffset(uintptr_t offset, bool negative)
    {
        int status = 0;
        if (negative) base_ = base_ < offset ? 0 : base_ - offset;
        else base_ = offset > std::numeric_limits<uintptr_t>::max() - base_ ? std::numeric_limits<uintptr_t>::max() : base_ + offset;
        if (format_ == Types::ViewFormat::Disasm) base_ &= ~static_cast<uintptr_t>(3);
        status = refresh();
        return status;
    }

    // 按有符号字节偏移调整当前浏览基址。
    int applyOffset(int64_t offset)
    {
        int status = 0;
        const auto magnitude = offset < 0 ? static_cast<uint64_t>(-(offset + 1)) + 1 : static_cast<uint64_t>(offset);
        status = applyOffset(static_cast<uintptr_t>(magnitude), offset < 0);
        return status;
    }

    // 解析偏移字符串后调整当前浏览基址。
    int applyOffset(std::string_view offsetStr)
    {
        int status = 0;
        auto result = MemUtils::ParseHexOffset(offsetStr);
        if (!result)
        {
            status = -EINVAL;
            return status;
        }
        status = applyOffset(result->offset, result->negative);
        return status;
    }
};

inline std::unique_ptr<MemViewer> gMemViewer = nullptr;
