#pragma once
#include <atomic>
#include <cerrno>
#include <memory>
#include <mutex>
#include <string>

#include "../../android_draw/vulkan_imgui_renderer.h"
#include "../../driver/driver.h"
#include "../base/config.h"
#include "../memory/mem_scanner.h"
#include "../memory/memory_viewer.h"
#include "../memory/pointer_manager.h"
#include "../memory/saved_address_manager.h"

class ApplicationRuntime
{
public:
    class ProcessMonitor
    {
    public:
        enum class Kind
        {
            Syscall,
            Cntvct
        };

    private:
        Driver &driver_;
        Kind kind_;
        std::mutex mutex_;
        std::atomic_int pid_{0};

    public:
        ProcessMonitor(Driver &driver, Kind kind) : driver_(driver), kind_(kind)
        {
        }

        int start()
        {
            std::lock_guard lock(mutex_);
            const int targetPid = driver_.GetGlobalPid();
            if (targetPid <= 0) return EINVAL;

            const int monitoredPid = pid_.load(std::memory_order_acquire);
            if (monitoredPid == targetPid) return 0;
            if (monitoredPid > 0)
            {
                const int status = kind_ == Kind::Syscall ? driver_.StopSyscallMonitor(monitoredPid) : driver_.StopCntvctMonitor(monitoredPid);
                if (status != 0) return status;
                pid_.store(0, std::memory_order_release);
            }

            const int status = kind_ == Kind::Syscall ? driver_.StartSyscallMonitor(targetPid) : driver_.StartCntvctMonitor(targetPid);
            if (status == 0) pid_.store(targetPid, std::memory_order_release);
            return status;
        }

        int stop()
        {
            std::lock_guard lock(mutex_);
            const int monitoredPid = pid_.load(std::memory_order_acquire);
            if (monitoredPid <= 0) return 0;

            const int status = kind_ == Kind::Syscall ? driver_.StopSyscallMonitor(monitoredPid) : driver_.StopCntvctMonitor(monitoredPid);
            if (status == 0) pid_.store(0, std::memory_order_release);
            return status;
        }

        int monitoredPid() const noexcept
        {
            return pid_.load(std::memory_order_acquire);
        }
    };

    class ResourceSet
    {
    private:
        Driver &driver_;

    public:
        MemScanner scanner;
        MemViewer viewer;
        PointerManager pointer;
        SavedAddressManager saved;
        std::string hwbpMode;
        ProcessMonitor syscallMonitor;
        ProcessMonitor cntvctMonitor;

        explicit ResourceSet(Driver &driver) : driver_(driver), syscallMonitor(driver, ProcessMonitor::Kind::Syscall), cntvctMonitor(driver, ProcessMonitor::Kind::Cntvct)
        {
        }

        ResourceSet(const ResourceSet &) = delete;
        ResourceSet &operator=(const ResourceSet &) = delete;

        int startSyscallMonitor()
        {
            std::lock_guard targetLock(Config::TargetMutex);
            return syscallMonitor.start();
        }

        int stopSyscallMonitor()
        {
            return syscallMonitor.stop();
        }

        int startCntvctMonitor()
        {
            std::lock_guard targetLock(Config::TargetMutex);
            return cntvctMonitor.start();
        }

        int stopCntvctMonitor()
        {
            return cntvctMonitor.stop();
        }

        bool selectTarget(int pid)
        {
            std::lock_guard targetLock(Config::TargetMutex);
            if (pid <= 0) return false;
            if (pid == driver_.GetGlobalPid()) return true;
            if (scanner.isScanning() || pointer.isBusy()) return false;
            if (syscallMonitor.stop() != 0 || cntvctMonitor.stop() != 0) return false;

            if (hwbpMode == "hwbp") driver_.RemoveProcessHwbpRef();
            else if (hwbpMode == "ptebp") driver_.RemoveProcessPtebpRef();
            else if (hwbpMode == "stepbp") driver_.RemoveProcessStepbpRef();

            hwbpMode.clear();
            saved.clear();
            scanner.clear();
            viewer.clear();
            pointer.clear();
            driver_.SetGlobalPid(pid);
            return true;
        }

        ~ResourceSet()
        {
            syscallMonitor.stop();
            cntvctMonitor.stop();
        }
    };

private:
    Driver &driver_;
    Config::ThreadPools threadPools_;
    std::unique_ptr<ResourceSet> resources_;
    bool graphicsInitialized_ = false;

public:
    explicit ApplicationRuntime(Driver &driver) : driver_(driver)
    {
        try
        {
            Config::InstallThreadPools(threadPools_);
            resources_ = std::make_unique<ResourceSet>(driver_);
        }
        catch (...)
        {
            resources_.reset();
            Config::UninstallThreadPools(threadPools_);
            threadPools_.shutdown();
            throw;
        }
    }

    ApplicationRuntime(const ApplicationRuntime &) = delete;
    ApplicationRuntime &operator=(const ApplicationRuntime &) = delete;

    ~ApplicationRuntime()
    {
        Config::g_Running = false;
        if (graphicsInitialized_) RenderVK::shutdown();
        threadPools_.shutdown();
        resources_.reset();
        Config::UninstallThreadPools(threadPools_);
    }

    ResourceSet &resources() noexcept
    {
        return *resources_;
    }

    bool initializeGraphics(bool allowCapture)
    {
        graphicsInitialized_ = RenderVK::init(allowCapture);
        return graphicsInitialized_;
    }
};
