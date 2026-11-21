#include <cerrno>
#include <climits>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <print>
#include <string>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include "android_draw/vulkan_imgui_renderer.h"
#include "application/cloudflared_tunnel.h"
#include "application/http_router.h"
#include "application/main_ui.h"
#include "application/service.h"
#include "driver_loader.h"
#include "font/oppo_font.h"
#include "imgui/imgui.h"
#include "service/http_server.h"
#include "test/gnss_test.h"
#include "test/gyro_test.h"
#include "test/read_write_test.h"
#include "test/touch_test.h"



// 脱离终端后台运行；调用前不得创建工作线程，fork() 后仅保留调用线程。
static int daemonize(const char *log_path)
{
    int status = 0;

    fflush(nullptr);

    //  第一次 fork：让父进程退出，使子进程在后台运行
    pid_t pid = fork();
    if (pid < 0) return status = -errno;
    if (pid > 0) _exit(EXIT_SUCCESS); // 父进程退出，不执行C++析构/atexit

    //  创建新会话：子进程成为新会话的首进程，完全脱离控制终端
    if (setsid() < 0) _exit(EXIT_FAILURE);

    //  忽略 SIGHUP 信号（可选，防止终端关闭时进程退出）
    signal(SIGHUP, SIG_IGN);

    // 第二次 fork：确保进程不是会话首进程，从而无法再次自动打开终端
    pid = fork();
    if (pid < 0) _exit(EXIT_FAILURE);
    if (pid > 0) _exit(EXIT_SUCCESS);

    //  修改文件权限掩码 (umask)
    umask(0);

    // 保留启动时的工作目录，不改变相对路径的含义。

    // 标准输入重定向到空设备，脱离终端输入。
    const int devnull = open("/dev/null", O_RDONLY);
    if (devnull < 0) return status = -errno;
    if (dup2(devnull, STDIN_FILENO) < 0) status = -errno;
    if (devnull != STDIN_FILENO) close(devnull);
    if (status != 0) return status;

    // 创建或清空日志文件，将标准输出和标准错误重定向到该文件。
    int fd = open(log_path, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    if (fd == -1)
    {
        status = -errno;
        std::perror("打开启动日志失败");
        return status;
    }
    //将标准输出 (1) 定向到 fd;       // 将标准错误 (2) 定向到 fd
    if (dup2(fd, STDOUT_FILENO) == -1 || dup2(fd, STDERR_FILENO) == -1)
    {
        status = -errno;
        if (fd > STDERR_FILENO) close(fd);
        errno = -status;
        std::perror("重定向启动日志失败");
        return status;
    }
    if (fd > STDERR_FILENO) close(fd);
    setvbuf(stdout, nullptr, _IONBF, 0);
    setvbuf(stderr, nullptr, _IONBF, 0);
    return status;
}

static void ConfigureProcessSurvivalPriority()
{
    errno = 0;
    if (setpriority(PRIO_PROCESS, 0, -20) == 0)
    {
        LS_LOGI_TAG("Main", "进程 nice 优先级已设置为 -20");
    }
    else
    {
        const int savedErrno = errno;
        LS_LOGW_TAG("Main", "设置进程 nice 优先级失败: errno=%d (%s)", savedErrno, std::strerror(savedErrno));
    }

    const int oomFd = open("/proc/self/oom_score_adj", O_WRONLY | O_CLOEXEC);
    if (oomFd == -1)
    {
        const int savedErrno = errno;
        LS_LOGW_TAG("Main", "打开 oom_score_adj 失败: errno=%d (%s)", savedErrno, std::strerror(savedErrno));
        return;
    }

    constexpr char kLowestOomScoreAdj[] = "-1000";
    const ssize_t written = write(oomFd, kLowestOomScoreAdj, sizeof(kLowestOomScoreAdj) - 1);
    const int savedErrno = errno;
    close(oomFd);

    if (written == static_cast<ssize_t>(sizeof(kLowestOomScoreAdj) - 1))
    {
        LS_LOGI_TAG("Main", "进程 oom_score_adj 已设置为 -1000");
    }
    else if (written < 0)
    {
        LS_LOGW_TAG("Main", "设置 oom_score_adj 失败: errno=%d (%s)", savedErrno, std::strerror(savedErrno));
    }
    else
    {
        LS_LOGW_TAG("Main", "设置 oom_score_adj 写入不足: %zd/%zu 字节", written, sizeof(kLowestOomScoreAdj) - 1);
    }
}

static void StopManagerApps()
{
    static constexpr const char *PackageNames[] = {
        "bin.mt.plus",
        "me.weishu.kernelsu",
        "com.sukisu.ultra",
    };

    DIR *ProcDirectory = opendir("/proc");
    if (!ProcDirectory)
    {
        LS_LOGE_ERRNO("扫描管理器进程失败");
        return;
    }

    const pid_t SelfPid = getpid();
    while (dirent *Entry = readdir(ProcDirectory))
    {
        if (Entry->d_name[0] < '0' || Entry->d_name[0] > '9') continue;
        char *End = nullptr;
        errno = 0;
        const long ProcessId = std::strtol(Entry->d_name, &End, 10);
        if (errno != 0 || *End != '\0' || ProcessId <= 1 || ProcessId > INT_MAX || ProcessId == SelfPid) continue;

        const std::string CmdlinePath = std::string("/proc/") + Entry->d_name + "/cmdline";
        std::ifstream Cmdline(CmdlinePath, std::ios::binary);
        std::string ProcessName;
        if (!std::getline(Cmdline, ProcessName, '\0')) continue;

        bool IsManagerApp = false;
        for (const char *PackageName : PackageNames)
        {
            const size_t PackageLength = std::strlen(PackageName);
            if (ProcessName == PackageName || (ProcessName.size() > PackageLength && ProcessName.compare(0, PackageLength, PackageName) == 0 && ProcessName[PackageLength] == ':'))
            {
                IsManagerApp = true;
                break;
            }
        }
        if (!IsManagerApp) continue;

        if (kill(static_cast<pid_t>(ProcessId), SIGKILL) == 0)
        {
            LS_LOGI_TAG("Main", "已向管理器进程 %s pid=%ld 发送 SIGKILL", ProcessName.c_str(), ProcessId);
        }
        else if (errno != ESRCH)
        {
            const int SavedErrno = errno;
            LS_LOGW_TAG("Main", "强制终止管理器进程 %s pid=%ld 失败: errno=%d (%s)", ProcessName.c_str(), ProcessId, SavedErrno, std::strerror(SavedErrno));
        }
    }
    closedir(ProcDirectory);
}


int main()
{
    int status = 0;
    std::println(stdout, "请选择启动模式：");
    std::println(stdout, "  0) 停止驱动线程");
    std::println(stdout, "  1) 读写测试");
    std::println(stdout, "  2) 触摸测试");
    std::println(stdout, "  3) 内存工具");
    std::println(stdout, "  4) HTTP/MCP服务器");
    std::println(stdout, "  5) 陀螺仪测试");
    std::println(stdout, "  6) 定位测试");
    std::print(stdout, "请输入 [0/1/2/3/4/5/6]: ");
    std::fflush(stdout);

    int mode = 0;

    if (!(std::cin >> mode))
    {
        std::println(stderr, "[错误] 输入无效。");
        return status = 1;
    }
    if (mode < 0 || mode > 6)
    {
        std::println(stderr, "[错误] 未知选项: {}", mode);
        return status = 1;
    }

    std::filesystem::path startupDirectory;
    if (mode == 4)
    {
        std::error_code error;
        startupDirectory = std::filesystem::current_path(error);
        if (error)
        {
            std::println(stderr, "[HTTP] 获取启动目录失败: {}", error.message());
            return status = 1;
        }
    }

    status = daemonize("/storage/emulated/0/log.txt");
    if (status != 0)
    {
        errno = -status;
        std::perror("后台化失败");
        return status;
    }

    LS_LOGI_TAG_FMT("Main", "启动模式: {}", mode);

    const int virtualSlots = (mode == 2 || mode == 3) ? 5 : 0;
    const bool initGyro = mode == 5;
    const bool initGnss = mode == 6;

    try
    {
        try
        {
            dr = std::make_unique<Driver>(virtualSlots, initGyro, initGnss);
        }
        catch (const std::exception &error)
        {
            LS_LOGW_TAG_FMT("Main", "驱动握手失败: {}，开始自动加载模块", error.what());
            status = DriverLoader::LoadKernelModule();
            if (status < 0)
            {
                LS_LOGE_TAG("Main", "自动加载驱动失败: error=%d", status);
                goto cleanup;
            }

            try
            {
                dr = std::make_unique<Driver>(virtualSlots, initGyro, initGnss);
            }
            catch (const std::exception &retryError)
            {
                LS_LOGE_TAG_FMT("Main", "驱动加载后仍无法连接: {}", retryError.what());
                status = 1;
                goto cleanup;
            }
        }

        if (mode == 4)
        {
            constexpr std::uint16_t port = 9494;
            gHttpServer = std::make_unique<HttpServer>();
            status = gHttpServer->init(port);
            if (status != 0)
            {
                LS_LOGE_TAG("Main", "初始化 HTTP 服务失败: %d", status);
                goto cleanup;
            }
            // 隧道 fork 必须早于内存模块和 HTTP 工作线程启动。
            gCloudflaredTunnel = std::make_unique<CloudflaredTunnel>();
            status = gCloudflaredTunnel->init(startupDirectory, port);
            if (status != 0)
            {
                LS_LOGE_TAG("Main", "初始化公网隧道失败: %d", status);
                goto cleanup;
            }
            std::println(stderr, "[HTTP] RPC: http://127.0.0.1:{}/api/rpc", port);
            std::println(stderr, "[MCP] Streamable HTTP: http://127.0.0.1:{}/mcp", port);
        }
        if (mode == 2 || mode == 3)
        {
            android::gBinderDump = std::make_unique<android::BinderDump>();
            android::gSurfaceControl = std::make_unique<android::SurfaceControlManager>();
        }
        if (mode == 3 || mode == 4)
        {
            Disasm::gDisassembler = std::make_unique<Disasm::Disassembler>();
            gMemScanner = std::make_unique<MemScanner>();
            gMemViewer = std::make_unique<MemViewer>();
            gPointerManager = std::make_unique<PointerManager>();
            gSavedAddresses = std::make_unique<SavedAddressManager>();
            gApplicationRuntime = std::make_unique<ApplicationRuntime>();
        }
        switch (mode)
        {
        case 0:
            dr->ExitKernel();
            break;
        case 1:
            status = RunReadWriteTest();
            break;
        case 2:
            status = RunTouchTest();
            break;
        case 3:
        {
            gTouchInput = std::make_unique<TouchInput>();
            gVulkanRenderer = std::make_unique<VulkanRenderer>();
            status = gVulkanRenderer->init(
                true,
                []
                {
                    ImGuiIO &io = ImGui::GetIO();
                    io.IniFilename = nullptr;
                    ImFontConfig fontConfig;
                    fontConfig.FontDataOwnedByAtlas = false;
                    io.Fonts->AddFontFromMemoryTTF((void *)OPPOSans_H, OPPOSans_H_size, 31.0f, &fontConfig, io.Fonts->GetGlyphRangesChineseFull());
                    ImGui::GetStyle().ScaleAllSizes(3.0f);
                },
                "Accessibility Overlays", 2024);
            if (status != 0)
            {
                LS_LOGE_TAG("Main", "初始化图形引擎失败: %d", status);
                goto cleanup;
            }
            gFloatingKeyboard = std::make_unique<FloatingKeyboard>();
            gMainUI = std::make_unique<MainUI>();
            while (gMainUI->isRunning())
            {
                status = gVulkanRenderer->drawFrame([](float width, float height) { return gMainUI->draw(width, height); });
                if (status != 0)
                {
                    LS_LOGE_TAG("Main", "提交渲染失败: %d", status);
                    goto cleanup;
                }
            }
            break;
        }
        case 4:
        {
            gApplicationService = std::make_unique<ApplicationService>();
            Mcp::gMcp = std::make_unique<Mcp::Adapter>();
            status = gHttpServer->run([](const Http::Request &request) { return Http::Route(request.method, request.path, request.headers, request.body, [](std::string_view operation, const nlohmann::json &params) { return gApplicationService->execute(operation, params); }); }, "Content-Type: application/json; charset=utf-8\r\n"
                                                                                                                                                                                                                                                                                       "Access-Control-Allow-Origin: *\r\n"
                                                                                                                                                                                                                                                                                       "Access-Control-Allow-Headers: Content-Type, Authorization, MCP-Protocol-Version\r\n"
                                                                                                                                                                                                                                                                                       "Access-Control-Allow-Methods: GET, POST, OPTIONS\r\n");
            break;
        }
        case 5:
            RunGyroTest();
            break;
        case 6:
            RunGnssTest();
            break;
        }
    }
    catch (const std::exception &error)
    {
        LS_LOGE_TAG_FMT("Main", "应用资源初始化或运行失败: {}", error.what());
        status = 1;
    }
    catch (...)
    {
        LS_LOGE_TAG("Main", "应用资源初始化或运行失败: unknown exception");
        status = 1;
    }

cleanup:
    gHttpServer.reset();
    gCloudflaredTunnel.reset();
    Mcp::gMcp.reset();
    gFloatingKeyboard.reset(); // 键盘可能仍借用界面的编辑缓冲区。
    gMainUI.reset();
    gTouchInput.reset();
    gVulkanRenderer.reset();
    android::gSurfaceControl.reset();
    android::gBinderDump.reset();
    gApplicationService.reset();
    gApplicationRuntime.reset();
    gSavedAddresses.reset();
    gPointerManager.reset();
    gMemViewer.reset();
    gMemScanner.reset();
    Disasm::gDisassembler.reset();
    dr.reset();
    return status;
}

/*
退出阶段oplus系列：
main 返回后，exit -> __cxa_finalize 会销毁厂商库 libpenguin_impl.so 的全局互斥锁，
但该库创建的 VsyncReceiver 线程仍可能访问它，触发 destroyed mutex / SIGABRT。
*/
