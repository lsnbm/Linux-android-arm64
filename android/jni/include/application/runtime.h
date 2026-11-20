#pragma once
#include <atomic>
#include <cerrno>
#include <future>
#include <memory>
#include <mutex>
#include <span>
#include <string>
#include <string_view>

#include "../../driver/driver.h"
#include "../base/config.h"
#include "../memory/mem_scanner.h"
#include "../memory/memory_viewer.h"
#include "../memory/pointer_manager.h"
#include "../memory/saved_address_manager.h"

class ApplicationRuntime
{
public:
    enum class Monitor { Syscall, Cntvct };

private:
    enum class Breakpoint { None, Hardware, Pte, Dpt, Step };
    std::atomic<Breakpoint> breakpoint_{Breakpoint::None};
    std::atomic_int monitorPids_[2]{};
    std::mutex monitorMutex_;
    std::atomic_bool dumping_{false};
    std::atomic_int dumpStatus_{0};
    std::future<void> dumpTask_;

    // 调用者持有 TargetMutex，移除与模式提交不会被目标切换打断。
    void removeBreakpoints()
    {
        switch (breakpoint_.load(std::memory_order_acquire))
        {
        case Breakpoint::Hardware: dr->RemoveProcessHwbpRef(); break;
        case Breakpoint::Pte: dr->RemoveProcessPtebpRef(); break;
        case Breakpoint::Dpt: dr->RemoveProcessDptbpRef(); break;
        case Breakpoint::Step: dr->RemoveProcessStepbpRef(); break;
        case Breakpoint::None: break;
        }
        breakpoint_.store(Breakpoint::None, std::memory_order_release);
    }

public:
    ApplicationRuntime() = default;
    ApplicationRuntime(const ApplicationRuntime &) = delete;
    ApplicationRuntime &operator=(const ApplicationRuntime &) = delete;

    int startMonitor(Monitor kind)
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        std::lock_guard lock(monitorMutex_);
        const int targetPid = dr->GetGlobalPid();
        if (targetPid <= 0) return status = -EINVAL;
        const int previous = monitorPids_[static_cast<size_t>(kind)].load(std::memory_order_acquire);
        if (previous == targetPid) return status;
        if (previous > 0)
        {
            status = kind == Monitor::Syscall ? dr->StopSyscallMonitor(previous) : dr->StopCntvctMonitor(previous);
            if (status != 0) return status;
            monitorPids_[static_cast<size_t>(kind)].store(0, std::memory_order_release);
        }
        status = kind == Monitor::Syscall ? dr->StartSyscallMonitor(targetPid) : dr->StartCntvctMonitor(targetPid);
        if (status != 0) return status;
        monitorPids_[static_cast<size_t>(kind)].store(targetPid, std::memory_order_release);
        return status;
    }

    int stopMonitor(Monitor kind)
    {
        int status = 0;
        std::lock_guard lock(monitorMutex_);
        const int pid = monitorPids_[static_cast<size_t>(kind)].load(std::memory_order_acquire);
        if (pid <= 0) return status;
        status = kind == Monitor::Syscall ? dr->StopSyscallMonitor(pid) : dr->StopCntvctMonitor(pid);
        if (status != 0) return status;
        monitorPids_[static_cast<size_t>(kind)].store(0, std::memory_order_release);
        return status;
    }

    int monitoredPid(Monitor kind) const noexcept
    {
        return monitorPids_[static_cast<size_t>(kind)].load(std::memory_order_acquire);
    }

    std::string_view breakpointMode() const noexcept
    {
        switch (breakpoint_.load(std::memory_order_acquire))
        {
        case Breakpoint::Hardware: return "hwbp";
        case Breakpoint::Pte: return "ptebp";
        case Breakpoint::Dpt: return "dptbp";
        case Breakpoint::Step: return "stepbp";
        case Breakpoint::None: return {};
        }
        return {};
    }

    int setBreakpoints(std::string_view mode, std::span<const Driver::bp_point> points)
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        if (breakpoint_.load(std::memory_order_acquire) != Breakpoint::None) return status = -EBUSY;
        if (dr->GetGlobalPid() <= 0 || points.empty() || points.size() > BP_CONFIG_MAX) return status = -EINVAL;
        Breakpoint selected;
        if (mode == "hwbp")
        {
            selected = Breakpoint::Hardware;
            status = dr->SetProcessHwbpRef(points);
        }
        else if (mode == "ptebp")
        {
            selected = Breakpoint::Pte;
            status = dr->SetProcessPtebpRef(points);
        }
        else if (mode == "dptbp")
        {
            selected = Breakpoint::Dpt;
            status = dr->SetProcessDptbpRef(points);
        }
        else if (mode == "stepbp")
        {
            selected = Breakpoint::Step;
            status = dr->SetProcessStepbpRef(points);
        }
        else return status = -EINVAL;
        if (status != 0) return status;
        breakpoint_.store(selected, std::memory_order_release);
        return status;
    }

    int clearBreakpoints()
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        removeBreakpoints();
        return status;
    }

    bool dumpBusy() const noexcept { return dumping_.load(std::memory_order_acquire); }
    int dumpStatus() const noexcept { return dumpStatus_.load(std::memory_order_acquire); }

    int dumpMemory(std::string_view target, std::string *path = nullptr)
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        if (dr->GetGlobalPid() <= 0 || target.empty()) return status = -EINVAL;
        if (!dr->DumpMemory(target, path)) return status = -EIO;
        return status;
    }

    int startDump(std::string target)
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        if (dumping_.load(std::memory_order_acquire)) return status = -EBUSY;
        if (dr->GetGlobalPid() <= 0 || target.empty()) return status = -EINVAL;
        dumping_.store(true, std::memory_order_release);
        dumpStatus_.store(0, std::memory_order_release);
        try
        {
            dumpTask_ = std::async(std::launch::async, [this, target = std::move(target)]
            {
                int status = 0;
                try { status = dumpMemory(target); }
                catch (const std::bad_alloc &) { status = -ENOMEM; }
                catch (...) { status = -EIO; }
                dumpStatus_.store(status, std::memory_order_release);
                dumping_.store(false, std::memory_order_release);
            });
        }
        catch (const std::bad_alloc &) { status = -ENOMEM; }
        catch (const std::system_error &error) { status = -error.code().value(); }
        if (status != 0)
        {
            dumping_.store(false, std::memory_order_release);
            return status;
        }
        return status;
    }

    int selectTarget(int pid)
    {
        int status = 0;
        std::lock_guard targetLock(Config::TargetMutex);
        if (pid <= 0) return status = -EINVAL;
        if (pid == dr->GetGlobalPid()) return status;
        if (gMemScanner->isScanning() || gPointerManager->isBusy() || dumping_.load(std::memory_order_acquire)) return status = -EBUSY;
        status = stopMonitor(Monitor::Syscall);
        if (status != 0) return status;
        status = stopMonitor(Monitor::Cntvct);
        if (status != 0) return status;
        removeBreakpoints();
        gSavedAddresses->clear();
        gMemScanner->clear();
        gMemViewer->clear();
        gPointerManager->clear();
        dr->SetGlobalPid(pid);
        return status;
    }

    ~ApplicationRuntime()
    {
        if (dumpTask_.valid()) dumpTask_.wait();
        for (const auto kind : {Monitor::Cntvct, Monitor::Syscall})
            if (const int status = stopMonitor(kind); status != 0) LS_LOGE_TAG("Runtime", "停止监听失败: %d", status);
        clearBreakpoints();
    }
};

inline std::unique_ptr<ApplicationRuntime> gApplicationRuntime = nullptr;
