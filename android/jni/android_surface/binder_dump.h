#pragma once
#include "../include/base/owned_fd.h"

#include <android/binder_ibinder.h>
#include <dlfcn.h>
#include <fcntl.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>

#include <atomic>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <mutex>
#include <new>
#include <string>
#include <system_error>
#include <thread>

namespace android::detail
{
    // AServiceManager_checkService 是平台 C 接口，不在普通 NDK 的 binder_manager.h 中。
    // 动态加载三个入口，保留 android-26 链接基线；不依赖 libbinder 的私有 C++ 对象布局。
    struct BinderDumpApi
    {
        AIBinder *(*checkService)(const char *) = nullptr;
        binder_status_t (*dump)(AIBinder *, int, const char **, uint32_t) = nullptr;
        void (*decStrong)(AIBinder *) = nullptr;

        BinderDumpApi()
        {
            void *library = dlopen("libbinder_ndk.so", RTLD_NOW | RTLD_LOCAL);
            if (!library) return;
            checkService = reinterpret_cast<decltype(checkService)>(dlsym(library, "AServiceManager_checkService"));
            dump = reinterpret_cast<decltype(dump)>(dlsym(library, "AIBinder_dump"));
            decStrong = reinterpret_cast<decltype(decStrong)>(dlsym(library, "AIBinder_decStrong"));
            // 句柄保留到进程结束：超时后仍可能有 Binder 线程执行这些函数。
        }
    };

    // 每个服务复用一个 reader。超时不代表事务已取消；未完成请求阻止下一次提交。
    class BinderDumpReader
    {
        struct Request
        {
            std::string service;
            Io::Fd writer;
            std::atomic<bool> cancelled{false};
            std::mutex mutex;
            bool done = false;
            binder_status_t status = 0;

            explicit Request(const std::string &name) : service(name) {}
        };

        struct CancelOnReturn
        {
            Request &request;
            ~CancelOnReturn() { request.cancelled.store(true); }
        };

        std::string service;
        std::mutex mutex;
        std::weak_ptr<Request> pending;

    public:
        explicit BinderDumpReader(const char *name) : service(name) {}

        // 只有完整 EOF、事务成功且未超时才提交输出；失败时不改写 output。
        // -t 是 dumpsys 客户端参数，不传给服务；同步 Binder 本身没有取消/超时接口。
        binder_status_t Read(std::string &output, std::chrono::milliseconds timeout,
                             size_t maxBytes = 16 * 1024 * 1024)
        {
            int status = 0;
            if (timeout.count() <= 0 || maxBytes == 0) return status = -EINVAL;
            const auto deadline = std::chrono::steady_clock::now() + timeout;
            try
            {
                std::shared_ptr<Request> request;
                Io::Fd reader;
                {
                    std::lock_guard lock(mutex);
                    if (auto previous = pending.lock())
                    {
                        std::lock_guard previousLock(previous->mutex);
                        if (!previous->done || !previous->cancelled.load()) return status = -EBUSY;
                    }
                    request = std::make_shared<Request>(service);
                    int fds[2];
                    if (pipe2(fds, O_CLOEXEC) != 0) return status = -errno;
                    reader.reset(fds[0]);
                    request->writer.reset(fds[1]);
                    const int flags = fcntl(reader.get(), F_GETFL);
                    if (flags < 0 || fcntl(reader.get(), F_SETFL, flags | O_NONBLOCK) < 0) return status = -errno;
                    // 仅读端非阻塞；写端维持阻塞语义，服务端不会因 EAGAIN 产生截断输出。
                    pending = request;
                    std::thread([request]
                    {
                        int status = 0;
                        // 调用方超时会关闭读端；进程内 Binder 写入时不能让 SIGPIPE 终止进程。
                        sigset_t signals;
                        sigemptyset(&signals);
                        sigaddset(&signals, SIGPIPE);
                        static const BinderDumpApi api;
                        status = -pthread_sigmask(SIG_BLOCK, &signals, nullptr);
                        if (status == 0)
                        {
                            if (request->cancelled.load()) status = -ETIMEDOUT;
                            else if (!api.checkService || !api.dump || !api.decStrong) status = -ENOSYS;
                            else if (AIBinder *binder = api.checkService(request->service.c_str()))
                            {
                                status = request->cancelled.load() ? -ETIMEDOUT : api.dump(binder, request->writer.get(), nullptr, 0);
                                api.decStrong(binder);
                            }
                            else status = -ENOENT;
                        }
                        // 本地写端一直保留到事务返回。先发布结果，读端看到 EOF 后就不必再等一次。
                        std::lock_guard lock(request->mutex);
                        request->status = status;
                        request->done = true;
                        request->writer.reset();
                    }).detach();
                }
                CancelOnReturn cancel{*request};
                std::string text;
                char buffer[8192];
                for (;;)
                {
                    const auto remaining = deadline - std::chrono::steady_clock::now();
                    if (remaining <= decltype(remaining)::zero()) return status = -ETIMEDOUT;
                    const auto milliseconds = std::chrono::ceil<std::chrono::milliseconds>(remaining).count();
                    pollfd fd{reader.get(), POLLIN, 0};
                    const int result = poll(&fd, 1, milliseconds > INT_MAX ? INT_MAX : static_cast<int>(milliseconds));
                    if (result < 0)
                    {
                        if (errno == EINTR) continue;
                        return status = -errno;
                    }
                    if (result == 0) continue;
                    if (fd.revents & POLLNVAL) return status = -EBADF;
                    // POLLHUP 仍可能有尾部数据，必须读到 EOF，而不是看到 HUP 就截断。
                    const ssize_t count = read(reader.get(), buffer, sizeof(buffer));
                    if (count < 0)
                    {
                        if (errno == EINTR || errno == EAGAIN) continue;
                        return status = -errno;
                    }
                    if (count == 0) break;
                    if (static_cast<size_t>(count) > maxBytes - text.size()) return status = -EFBIG;
                    text.append(buffer, static_cast<size_t>(count));
                }
                std::lock_guard lock(request->mutex);
                if (std::chrono::steady_clock::now() >= deadline) return status = -ETIMEDOUT;
                status = request->status;
                if (status != 0) return status;
                output.swap(text);
            }
            catch (const std::bad_alloc &) { return status = -ENOMEM; }
            catch (const std::system_error &error) { return status = -error.code().value(); }
            return status;
        }
    };
}
