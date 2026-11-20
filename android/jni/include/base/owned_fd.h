#pragma once
#include <unistd.h>
#include <utility>

namespace Io
{
    class Fd
    {
        int fd_;

    public:
        explicit Fd(int fd = -1) noexcept : fd_(fd) {}
        Fd(Fd &&other) noexcept : fd_(std::exchange(other.fd_, -1)) {}
        Fd(const Fd &) = delete;
        Fd &operator=(const Fd &) = delete;
        int get() const noexcept { return fd_; }
        int release() noexcept { return std::exchange(fd_, -1); }
        void reset(int fd = -1) noexcept
        {
            if (fd_ == fd) return;
            if (fd_ >= 0) close(fd_);
            fd_ = fd;
        }
        ~Fd() { reset(); }
    };
}