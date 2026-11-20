#pragma once
#include <atomic>
#include <cerrno>
#include <memory>
#include <mutex>
#include <string>

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
            int status = 0;
            std::lock_guard targetLock(Config::TargetMutex);
            std::lock_guard lock(mutex_);
            const int targetPid = driver_.GetGlobalPid();
            if (targetPid <= 0) return status = EINVAL;

            const int monitoredPid = pid_.load(std::memory_order_acquire);
            if (monitoredPid == targetPid) return status;
            if (monitoredPid > 0)
            {
                status = kind_ == Kind::Syscall ? driver_.StopSyscallMonitor(monitoredPid) : driver_.StopCntvctMonitor(monitoredPid);
                if (status != 0) return status;
                pid_.store(0, std::memory_order_release);
            }

            status = kind_ == Kind::Syscall ? driver_.StartSyscallMonitor(targetPid) : driver_.StartCntvctMonitor(targetPid);
            if (status == 0) pid_.store(targetPid, std::memory_order_release);
            return status;
        }

        int stop()
        {
            int status = 0;
            std::lock_guard lock(mutex_);
            const int monitoredPid = pid_.load(std::memory_order_acquire);
            if (monitoredPid <= 0) return status;

            status = kind_ == Kind::Syscall ? driver_.StopSyscallMonitor(monitoredPid) : driver_.StopCntvctMonitor(monitoredPid);
            if (status == 0) pid_.store(0, std::memory_order_release);
            return status;
        }

        ~ProcessMonitor() { stop(); }

        int monitoredPid() const noexcept
        {
            return pid_.load(std::memory_order_acquire);
        }
    };

private:
    Driver &driver_;

public:
    MemScanner<Driver> scanner;
    MemViewer viewer;
    PointerManager pointer;
    SavedAddressManager saved;
    std::string hwbpMode;
    ProcessMonitor syscallMonitor;
    ProcessMonitor cntvctMonitor;

    explicit ApplicationRuntime(Driver &driver) : driver_(driver), scanner(driver), syscallMonitor(driver, ProcessMonitor::Kind::Syscall), cntvctMonitor(driver, ProcessMonitor::Kind::Cntvct)
    {
    }

    ApplicationRuntime(const ApplicationRuntime &) = delete;
    ApplicationRuntime &operator=(const ApplicationRuntime &) = delete;

    bool selectTarget(int pid)
    {
        std::lock_guard targetLock(Config::TargetMutex);
        if (pid <= 0) return false;
        if (pid == driver_.GetGlobalPid()) return true;
        if (scanner.isScanning() || pointer.isBusy()) return false;
        if (syscallMonitor.stop() != 0 || cntvctMonitor.stop() != 0) return false;

        if (hwbpMode == "hwbp") driver_.RemoveProcessHwbpRef();
        else if (hwbpMode == "ptebp") driver_.RemoveProcessPtebpRef();
        else if (hwbpMode == "dptbp") driver_.RemoveProcessDptbpRef();
        else if (hwbpMode == "stepbp") driver_.RemoveProcessStepbpRef();

        hwbpMode.clear();
        saved.clear();
        scanner.clear();
        viewer.clear();
        pointer.clear();
        driver_.SetGlobalPid(pid);
        return true;
    }

};
