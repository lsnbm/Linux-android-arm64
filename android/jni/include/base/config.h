#pragma once
#include <algorithm>
#include <atomic>
#include <cstddef>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <thread>
#include "../../thread-pool/include/BS_thread_pool.hpp"

namespace Config
{
    inline std::atomic<bool> g_Running{true};
    inline std::atomic<int> g_ItemsPerPage{100};
    inline std::mutex TargetMutex;

    class ThreadPools
    {
    private:
        std::unique_ptr<BS::thread_pool<>> cpu_;
        std::unique_ptr<BS::thread_pool<>> io_;
        std::unique_ptr<BS::thread_pool<>> client_;

        template <typename Pool> static void Shutdown(Pool &pool)
        {
            pool.purge();
            pool.wait();
        }

    public:
        BS::thread_pool<> &cpu()
        {
            if (!cpu_) cpu_ = std::make_unique<BS::thread_pool<>>(std::max(4u, std::thread::hardware_concurrency()));
            return *cpu_;
        }

        BS::thread_pool<> &io()
        {
            if (!io_) io_ = std::make_unique<BS::thread_pool<>>(std::max(16u, std::thread::hardware_concurrency() * 4));
            return *io_;
        }

        BS::thread_pool<> &client()
        {
            if (!client_) client_ = std::make_unique<BS::thread_pool<>>(std::max(16u, std::thread::hardware_concurrency() * 4));
            return *client_;
        }

        void shutdown() noexcept
        {
            try
            {
                if (client_) Shutdown(*client_);
                if (cpu_) Shutdown(*cpu_);
                if (io_) Shutdown(*io_);
            }
            catch (...)
            {
            }
            client_.reset();
            cpu_.reset();
            io_.reset();
        }

        ~ThreadPools()
        {
            shutdown();
        }
    };

    inline ThreadPools *g_ThreadPools = nullptr;

    inline void InstallThreadPools(ThreadPools &pools)
    {
        if (g_ThreadPools != nullptr) throw std::logic_error("线程池资源已安装");
        g_ThreadPools = &pools;
    }

    inline void UninstallThreadPools(ThreadPools &pools) noexcept
    {
        if (g_ThreadPools == &pools) g_ThreadPools = nullptr;
    }

    inline ThreadPools &ActiveThreadPools()
    {
        if (g_ThreadPools == nullptr) throw std::logic_error("线程池资源尚未安装");
        return *g_ThreadPools;
    }

    inline BS::thread_pool<> &CpuThreadPool()
    {
        return ActiveThreadPools().cpu();
    }

    inline BS::thread_pool<> &IoThreadPool()
    {
        return ActiveThreadPools().io();
    }

    struct Constants
    {
        // 内存浏览缓存固定为当前浏览地址开始的 100 字节。
        static constexpr size_t MEM_VIEW_DEFAULT_BYTES = 100;
        static constexpr size_t SCAN_BUFFER = 4096;
        static constexpr size_t BATCH_SIZE = 16384;
        static constexpr uintptr_t ADDR_MIN = 0x10000;
        static constexpr uintptr_t ADDR_MAX = 0x7FFFFFFFFFFF;
    };

} // namespace Config
