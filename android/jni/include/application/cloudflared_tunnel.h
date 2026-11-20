#pragma once
#include <cerrno>
#include <csignal>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <format>
#include <memory>
#include <print>
#include <sched.h>
#include <string>
#include <string_view>
#include <sys/mount.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <system_error>
#include <unistd.h>
#include <utility>
#include "../base/owned_fd.h"

extern "C"
{
    extern const unsigned char cloudflared_blob_start[];
    extern const unsigned char cloudflared_blob_end[];
}

class CloudflaredTunnel
{
    static void printErrno(std::string_view action)
    {
        const int error = errno;
        std::println(stderr, "[HTTP] {}: errno={} ({})", action, error, std::strerror(error));
    }

    static int WritePrivateEtcFile(const char *path, std::string_view content)
    {
        int status = 0;
        Io::Fd file(open(path, O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0644));
        if (file.get() < 0) return status = -errno;
        for (size_t written = 0; written < content.size();)
        {
            const ssize_t count = write(file.get(), content.data() + written, content.size() - written);
            if (count < 0 && errno == EINTR) continue;
            if (count <= 0) return status = count < 0 ? -errno : -EIO;
            written += static_cast<size_t>(count);
        }
        if (close(file.release()) != 0) return status = -errno;
        return status;
    }

    struct TunnelChild
    {
        pid_t pid = -1;

        TunnelChild() = default;
        TunnelChild(const TunnelChild &) = delete;
        TunnelChild &operator=(const TunnelChild &) = delete;
        void reset()
        {
            if (pid <= 0) return;
            const pid_t child = std::exchange(pid, -1);
            pid_t result;
            do { result = waitpid(child, nullptr, WNOHANG); } while (result < 0 && errno == EINTR);
            if (result != 0) return;
            if (kill(child, SIGKILL) != 0 && errno != ESRCH) return;
            while (waitpid(child, nullptr, 0) < 0 && errno == EINTR) {}
        }
        ~TunnelChild() { reset(); }
    } tunnel;

public:
    CloudflaredTunnel() = default;
    CloudflaredTunnel(const CloudflaredTunnel &) = delete;
    CloudflaredTunnel &operator=(const CloudflaredTunnel &) = delete;

    int init(const std::filesystem::path &directory, std::uint16_t port)
    {
        int status = 0;
        if (tunnel.pid > 0) return status;
        struct Rollback
        {
            TunnelChild *child;
            ~Rollback() { if (child) child->reset(); }
        } rollback{&tunnel};
        try
        {
            const std::string url = std::format("http://127.0.0.1:{}", port);
            const std::size_t blobSize = static_cast<std::size_t>(cloudflared_blob_end - cloudflared_blob_start);
            if (blobSize == 0) return status = -ENODATA;

            const std::filesystem::path cloudflaredPath = directory / ".cloudflared";
            const std::filesystem::path cloudflaredTempPath = directory / ".cloudflared.tmp";
            Io::Fd executable(open(cloudflaredTempPath.c_str(), O_WRONLY | O_CREAT | O_TRUNC | O_CLOEXEC, 0700));
            if (executable.get() < 0)
            {
                status = -errno;
                printErrno("创建 cloudflared 临时文件失败");
                return status;
            }
            struct RemoveTemporary
            {
                const std::filesystem::path &path;
                ~RemoveTemporary() { unlink(path.c_str()); }
            } removeTemporary{cloudflaredTempPath};

            std::size_t written = 0;
            while (written < blobSize)
            {
                const ssize_t result = write(executable.get(), cloudflared_blob_start + written, blobSize - written);
                if (result < 0 && errno == EINTR) continue;
                if (result <= 0)
                {
                    status = result < 0 ? -errno : -EIO;
                    return status;
                }
                written += static_cast<std::size_t>(result);
            }

            if (fchmod(executable.get(), 0700) != 0)
            {
                status = -errno;
                printErrno("设置 cloudflared 权限失败");
                return status;
            }
            if (close(executable.release()) != 0)
            {
                status = -errno;
                return status;
            }

            if (rename(cloudflaredTempPath.c_str(), cloudflaredPath.c_str()) != 0)
            {
                status = -errno;
                printErrno("安装 cloudflared 失败");
                return status;
            }

            int startupPipe[2];
            if (pipe2(startupPipe, O_CLOEXEC) != 0) return status = -errno;
            Io::Fd startupReader(startupPipe[0]), startupWriter(startupPipe[1]);
            const pid_t parentPid = getpid();
            const pid_t pid = fork();
            if (pid < 0)
            {
                status = -errno;
                printErrno("启动 cloudflared 失败");
                unlink(cloudflaredPath.c_str());
                return status;
            }
            if (pid == 0)
            {
                startupReader.reset();
                if (prctl(PR_SET_PDEATHSIG, SIGTERM) != 0) status = -errno;
                else if (getppid() != parentPid) _exit(125);
                constexpr std::string_view resolvConf = "nameserver 1.1.1.1\n"
                                                        "nameserver 8.8.8.8\n"
                                                        "options timeout:2 attempts:2\n";
                constexpr std::string_view hosts = "127.0.0.1 localhost\n"
                                                   "::1 ip6-localhost\n";

                if (status == 0 && (unshare(CLONE_NEWNS) != 0 || mount(nullptr, "/", nullptr, MS_REC | MS_PRIVATE, nullptr) != 0 ||
                    mount("tmpfs", "/system/etc", "tmpfs", MS_NOSUID | MS_NODEV, "mode=0755,size=64k") != 0 ||
                    setenv("SSL_CERT_DIR", "/apex/com.android.conscrypt/cacerts", 1) != 0)) status = -errno;
                if (status == 0) status = WritePrivateEtcFile("/system/etc/resolv.conf", resolvConf);
                if (status == 0) status = WritePrivateEtcFile("/system/etc/hosts", hosts);
                if (status == 0)
                {
                    execl(cloudflaredPath.c_str(), cloudflaredPath.c_str(), "tunnel", "--no-autoupdate", "--edge-ip-version", "4", "--url", url.c_str(), static_cast<char *>(nullptr));
                    status = -errno;
                }
                const char *bytes = reinterpret_cast<const char *>(&status);
                for (size_t sent = 0; sent < sizeof(status);)
                {
                    const ssize_t count = write(startupWriter.get(), bytes + sent, sizeof(status) - sent);
                    if (count < 0 && errno == EINTR) continue;
                    if (count <= 0) break;
                    sent += static_cast<size_t>(count);
                }
                _exit(127);
            }

            tunnel.pid = pid;
            startupWriter.reset();
            int childStatus = 0;
            size_t received = 0;
            while (received < sizeof(childStatus))
            {
                const ssize_t count = read(startupReader.get(), reinterpret_cast<char *>(&childStatus) + received, sizeof(childStatus) - received);
                if (count < 0 && errno == EINTR) continue;
                if (count < 0) return status = -errno;
                if (count == 0) break; // O_CLOEXEC：exec 成功后写端关闭。
                received += static_cast<size_t>(count);
            }
            if (received != 0) return status = received == sizeof(childStatus) && childStatus != 0 ? childStatus : -EIO;
            std::println(stderr, "[HTTP] cloudflared Tunnel 已启动，PID={}，公网地址将写入 /storage/emulated/0/log.txt", pid);
            rollback.child = nullptr;
        }
        catch (const std::bad_alloc &) { return status = -ENOMEM; }
        catch (const std::system_error &error) { return status = -error.code().value(); }
        catch (...) { return status = -EIO; }
        return status;
    }
};

inline std::unique_ptr<CloudflaredTunnel> gCloudflaredTunnel = nullptr;
