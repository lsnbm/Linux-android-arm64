#pragma once

#include <cstddef>
#include <cstdlib>

#include <sys/mman.h>
#include <unistd.h>

// RAII mmap 封装
class MappedFile
{
    int fd_ = -1;
    void *ptr_ = nullptr;
    size_t size_ = 0;

public:
    MappedFile() = default;
    ~MappedFile() { release(); }
    MappedFile(const MappedFile &) = delete;
    MappedFile &operator=(const MappedFile &) = delete;

    MappedFile(MappedFile &&o) noexcept : fd_(o.fd_), ptr_(o.ptr_), size_(o.size_)
    {
        o.fd_ = -1;
        o.ptr_ = nullptr;
        o.size_ = 0;
    }

    MappedFile &operator=(MappedFile &&o) noexcept
    {
        if (this != &o)
        {
            release();
            fd_ = o.fd_;
            ptr_ = o.ptr_;
            size_ = o.size_;
            o.fd_ = -1;
            o.ptr_ = nullptr;
            o.size_ = 0;
        }
        return *this;
    }

    // 申请并初始化底层内存映射。
    bool allocate(size_t sz)
    {
        release();
        char tpl[] = "/data/local/tmp/memscan_XXXXXX";
        fd_ = mkstemp(tpl);
        if (fd_ < 0)
            return false;
        unlink(tpl);
        if (ftruncate(fd_, static_cast<off_t>(sz)) != 0)
        {
            close(fd_);
            fd_ = -1;
            return false;
        }
        ptr_ = mmap(nullptr, sz, PROT_READ | PROT_WRITE, MAP_SHARED, fd_, 0);
        if (ptr_ == MAP_FAILED)
        {
            ptr_ = nullptr;
            close(fd_);
            fd_ = -1;
            return false;
        }
        size_ = sz;
        return true;
    }

    // 释放当前对象持有的底层资源。
    void release()
    {
        if (ptr_)
        {
            munmap(ptr_, size_);
            ptr_ = nullptr;
        }
        if (fd_ >= 0)
        {
            ::close(fd_);
            fd_ = -1;
        }
        size_ = 0;
    }

    template <typename T = void>
    T *as() noexcept { return static_cast<T *>(ptr_); }

    template <typename T = void>
    const T *as() const noexcept { return static_cast<const T *>(ptr_); }

    // 返回当前映射区域的字节大小。
    size_t size() const noexcept { return size_; }
    // 判断当前映射指针是否有效。
    bool valid() const noexcept { return ptr_ != nullptr; }

    // 向内核提示映射区域的访问模式。
    void advise(int advice)
    {
        if (ptr_)
            madvise(ptr_, size_, advice);
    }
};
