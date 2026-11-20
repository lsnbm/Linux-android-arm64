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
#include <utility>

namespace android
{
    class BinderDump
    {
    public:
        enum class Service { SurfaceFlinger, Display };

    private:
        // AServiceManager_checkService 是平台 C 接口，不在普通 NDK 的 binder_manager.h 中。
        // 动态加载三个入口，保留 android-26 链接基线；不依赖 libbinder 的私有 C++ 对象布局。
        struct BinderDumpApi
        {
            void *library = dlopen("libbinder_ndk.so", RTLD_NOW | RTLD_LOCAL);
            AIBinder *(*checkService)(const char *) = nullptr;
            binder_status_t (*dump)(AIBinder *, int, const char **, uint32_t) = nullptr;
            void (*decStrong)(AIBinder *) = nullptr;

            BinderDumpApi()
            {
                if (!library) return;
                checkService = reinterpret_cast<decltype(checkService)>(dlsym(library, "AServiceManager_checkService"));
                dump = reinterpret_cast<decltype(dump)>(dlsym(library, "AIBinder_dump"));
                decStrong = reinterpret_cast<decltype(decStrong)>(dlsym(library, "AIBinder_decStrong"));
            }

            ~BinderDumpApi() { if (library) dlclose(library); }
            BinderDumpApi(const BinderDumpApi &) = delete;
            BinderDumpApi &operator=(const BinderDumpApi &) = delete;
        };

        struct Request
        {
            std::shared_ptr<const BinderDumpApi> api;
            Service service;
            Io::Fd writer;
            std::atomic<bool> cancelled{false};
            std::mutex mutex;
            bool done = false;
            int status = 0;

            Request(std::shared_ptr<const BinderDumpApi> api, Service service) : api(std::move(api)), service(service) {}
        };

        struct CancelOnReturn
        {
            Request &request;
            ~CancelOnReturn() { request.cancelled.store(true); }
        };

        struct Channel
        {
            std::mutex mutex;
            std::weak_ptr<Request> pending;
        } surfaceFlinger, display;

        // Binder 超时不能取消同步调用。线程持有自己的引用，不依赖 BinderDump 的析构顺序。
        std::shared_ptr<const BinderDumpApi> api = std::make_shared<BinderDumpApi>();

        static void *Dump(void *argument)
        {
            auto request = std::move(*std::unique_ptr<std::shared_ptr<Request>>(static_cast<std::shared_ptr<Request> *>(argument)));
            int status = 0;
            // 调用方超时会关闭读端；进程内 Binder 写入时不能让 SIGPIPE 终止进程。
            sigset_t signals;
            sigemptyset(&signals);
            sigaddset(&signals, SIGPIPE);
            status = -pthread_sigmask(SIG_BLOCK, &signals, nullptr);
            if (status == 0)
            {
                if (request->cancelled.load()) status = -ETIMEDOUT;
                else if (!request->api->checkService || !request->api->dump || !request->api->decStrong) status = -ENOSYS;
                else if (AIBinder *binder = request->api->checkService(request->service == Service::Display ? "display" : "SurfaceFlinger"))
                {
                    status = request->cancelled.load() ? -ETIMEDOUT : request->api->dump(binder, request->writer.get(), nullptr, 0);
                    request->api->decStrong(binder);
                }
                else status = -ENOENT;
            }
            // 本地写端一直保留到事务返回。先发布结果，读端看到 EOF 后就不必再等一次。
            std::lock_guard lock(request->mutex);
            request->status = status;
            request->done = true;
            request->writer.reset();
            return nullptr;
        }

    public:
        // 只有完整 EOF、事务成功且未超时才提交输出；失败时不改写 output。
        // -t 是 dumpsys 客户端参数，不传给服务；同步 Binder 本身没有取消/超时接口。
        binder_status_t Read(Service service, std::string &output, std::chrono::milliseconds timeout,
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
                    auto &channel = service == Service::Display ? display : surfaceFlinger;
                    std::lock_guard lock(channel.mutex);
                    if (auto previous = channel.pending.lock())
                    {
                        std::lock_guard previousLock(previous->mutex);
                        if (!previous->done || !previous->cancelled.load()) return status = -EBUSY;
                    }
                    request = std::make_shared<Request>(api, service);
                    int fds[2];
                    if (pipe2(fds, O_CLOEXEC) != 0) return status = -errno;
                    reader.reset(fds[0]);
                    request->writer.reset(fds[1]);
                    const int flags = fcntl(reader.get(), F_GETFL);
                    if (flags < 0 || fcntl(reader.get(), F_SETFL, flags | O_NONBLOCK) < 0) return status = -errno;
                    // 仅读端非阻塞；写端维持阻塞语义，服务端不会因 EAGAIN 产生截断输出。
                    pthread_attr_t attributes;
                    status = -pthread_attr_init(&attributes);
                    if (status != 0) return status;
                    struct DestroyAttributes
                    {
                        pthread_attr_t &attributes;
                        ~DestroyAttributes() { pthread_attr_destroy(&attributes); }
                    } destroyAttributes{attributes};
                    status = -pthread_attr_setdetachstate(&attributes, PTHREAD_CREATE_DETACHED);
                    if (status != 0) return status;
                    auto argument = std::make_unique<std::shared_ptr<Request>>(request);
                    pthread_t thread;
                    status = -pthread_create(&thread, &attributes, Dump, argument.get());
                    if (status != 0) return status;
                    argument.release(); // 仅在线程创建成功后移交参数和请求引用。
                    channel.pending = request;
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

    inline std::unique_ptr<BinderDump> gBinderDump = nullptr;
}
