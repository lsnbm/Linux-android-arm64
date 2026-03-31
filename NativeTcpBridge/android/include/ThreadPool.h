#pragma once

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <functional>
#include <future>
#include <mutex>
#include <queue>
#include <stdexcept>
#include <thread>
#include <tuple>
#include <type_traits>
#include <utility>
#include <vector>

namespace Utils
{
    inline unsigned GetThreadCount() noexcept
    {
        if (auto n = std::thread::hardware_concurrency(); n > 0)
            return n;
        return 4;
    }

    class ThreadPool
    {
        std::vector<std::thread> workers_;
        std::queue<std::function<void()>> tasks_;
        std::mutex mtx_;
        std::condition_variable_any cv_;
        std::condition_variable done_cv_;
        size_t active_{0};
        std::atomic<bool> stopping_{false};

    public:
        explicit ThreadPool(size_t n = GetThreadCount())
        {
            if (n == 0)
                n = 4;
            for (size_t i = 0; i < n; ++i)
            {
                workers_.emplace_back([this]
                                      {
                    while (!stopping_.load(std::memory_order_relaxed)) {
                        std::function<void()> task;
                        {
                            std::unique_lock lk(mtx_);
                            cv_.wait(lk, [&]{ return stopping_.load(std::memory_order_relaxed) || !tasks_.empty(); });
                            if (stopping_.load(std::memory_order_relaxed) && tasks_.empty()) return;
                            if (tasks_.empty()) continue;
                            task = std::move(tasks_.front());
                            tasks_.pop();
                            ++active_;
                        }
                        try {
                            task();
                        } catch (...) {
                        }
                        {
                            std::lock_guard lk(mtx_);
                            --active_;
                            if (tasks_.empty() && active_ == 0)
                                done_cv_.notify_all();
                        }
                    } });
            }
        }

        ~ThreadPool()
        {
            shutdown(false);
        }

        ThreadPool(const ThreadPool &) = delete;
        ThreadPool &operator=(const ThreadPool &) = delete;

        template <class F, class... Args>
        auto push(F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>
        {
            using R = std::invoke_result_t<F, Args...>;
            auto task = std::make_shared<std::packaged_task<R()>>(
                std::bind(std::forward<F>(f), std::forward<Args>(args)...));
            auto fut = task->get_future();
            {
                std::lock_guard lk(mtx_);
                if (stopping_.load(std::memory_order_relaxed))
                    throw std::runtime_error("ThreadPool is stopping");
                tasks_.emplace([task]
                               { (*task)(); });
            }
            cv_.notify_one();
            return fut;
        }

        template <class F, class... Args>
        bool post(F &&f, Args &&...args)
        {
            auto thunk = std::make_shared<std::tuple<std::decay_t<F>, std::decay_t<Args>...>>(
                std::forward<F>(f), std::forward<Args>(args)...);
            {
                std::lock_guard lk(mtx_);
                if (stopping_.load(std::memory_order_relaxed))
                    return false;
                tasks_.emplace([thunk]() mutable
                               {
                                   std::apply([](auto &fn, auto &...xs)
                                              { std::invoke(fn, xs...); }, *thunk);
                               });
            }
            cv_.notify_one();
            return true;
        }

        void wait_all()
        {
            std::unique_lock lk(mtx_);
            done_cv_.wait(lk, [&]
                          { return tasks_.empty() && active_ == 0; });
        }

        void shutdown(bool drop_pending)
        {
            {
                std::lock_guard lk(mtx_);
                if (stopping_.load(std::memory_order_relaxed))
                    return;
                stopping_.store(true, std::memory_order_relaxed);
                if (drop_pending)
                {
                    while (!tasks_.empty())
                        tasks_.pop();
                }
            }
            cv_.notify_all();
            done_cv_.notify_all();
            for (auto &w : workers_)
                if (w.joinable())
                    w.join();
            workers_.clear();
        }

        void force_stop()
        {
            shutdown(true);
        }
    };

    class GlobalThreadPools
    {
    public:
        ThreadPool &cpu()
        {
            static ThreadPool pool{GetThreadCount()};
            return pool;
        }

        ThreadPool &io()
        {
            static ThreadPool pool{std::max<unsigned>(16, GetThreadCount() * 4)};
            return pool;
        }

        template <class F, class... Args>
        auto push(F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>
        {
            return cpu().push(std::forward<F>(f), std::forward<Args>(args)...);
        }

        template <class F, class... Args>
        bool post(F &&f, Args &&...args)
        {
            return cpu().post(std::forward<F>(f), std::forward<Args>(args)...);
        }

        template <class F, class... Args>
        auto push_io(F &&f, Args &&...args) -> std::future<std::invoke_result_t<F, Args...>>
        {
            return io().push(std::forward<F>(f), std::forward<Args>(args)...);
        }

        template <class F, class... Args>
        bool post_io(F &&f, Args &&...args)
        {
            return io().post(std::forward<F>(f), std::forward<Args>(args)...);
        }

        void wait_all()
        {
            cpu().wait_all();
            io().wait_all();
        }

        void force_stop()
        {
            cpu().force_stop();
            io().force_stop();
        }
    };

    inline GlobalThreadPools GlobalPool{};
}
