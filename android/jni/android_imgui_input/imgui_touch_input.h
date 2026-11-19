#pragma once

#include "../logger/logger.h"
#include "imgui/imgui.h"
#include <array>
#include <atomic>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <exception>
#include <fcntl.h>
#include <linux/input.h>
#include <mutex>
#include <poll.h>
#include <system_error>
#include <sys/ioctl.h>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <vector>

#define MAX_DEVICES 5
#define MAX_FINGERS 10

inline std::atomic<uint32_t> orientation{0};
inline std::atomic<float> screenHeight{0}, screenWidth{0};

inline void UpdateScreenData(int w, int h, uint32_t orientation_)
{
    screenWidth.store(w, std::memory_order_relaxed);
    screenHeight.store(h, std::memory_order_relaxed);
    orientation.store(orientation_, std::memory_order_relaxed);
}

struct TouchPoint
{
    bool isDown = false;
    int x = 0;
    int y = 0;
    int id = -1;
};

inline std::mutex touch_mutex;
inline std::array<std::array<TouchPoint, MAX_FINGERS>, MAX_DEVICES> fingers{};

inline auto Touch_GetSnapshot()
{
    std::lock_guard lock(touch_mutex);
    return fingers;
}

inline bool IsUserTouchingRect(float rx, float ry, float rw, float rh)
{
    std::lock_guard lock(touch_mutex);
    for (const auto &finger : fingers[0])
        if (finger.isDown && finger.x >= rx && finger.x <= rx + rw && finger.y >= ry && finger.y <= ry + rh) return true;
    return false;
}

enum class TouchInputEventType { Position, Button };

struct TouchInputEvent
{
    TouchInputEventType type;
    float x, y;
    bool down = false;
};

inline std::vector<TouchInputEvent> pendingTouchEvents;

inline void enqueueTouchEvent(const TouchInputEvent &event)
{
    if (pendingTouchEvents.size() >= 2048) pendingTouchEvents.clear();
    pendingTouchEvents.push_back(event);
}

inline void Touch_UpdateImGui()
{
    std::vector<TouchInputEvent> events;
    {
        std::lock_guard lock(touch_mutex);
        if (pendingTouchEvents.empty()) return;
        events.swap(pendingTouchEvents);
    }
    ImGuiIO &io = ImGui::GetIO();
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    for (const auto &event : events)
    {
        if (event.type == TouchInputEventType::Position) io.AddMousePosEvent(event.x, event.y);
        else io.AddMouseButtonEvent(0, event.down);
    }
}

inline bool testInputBit(int bit, const uint8_t *bits)
{
    return (bits[bit / 8] & (1u << (bit % 8))) != 0;
}

inline std::atomic<bool> Touch_initialized{false};
inline std::thread touchThread;
inline int touchFd = -1;

inline bool Touch_Init()
{
    if (Touch_initialized.load(std::memory_order_acquire)) return true;
    DIR *dir = opendir("/dev/input");
    if (!dir) return false;

    struct Candidate
    {
        int fd = -1, number = 0, maxX = 0, maxY = 0;
        bool direct = false, pointer = false;
        int64_t area = 0;
    } best;

    while (const auto *entry = readdir(dir))
    {
        if (strncmp(entry->d_name, "event", 5) != 0) continue;
        Candidate candidate;
        const char *end = entry->d_name + strlen(entry->d_name);
        const auto number = std::from_chars(entry->d_name + 5, end, candidate.number);
        if (number.ec != std::errc{} || number.ptr != end || candidate.number < 0) continue;
        candidate.fd = openat(dirfd(dir), entry->d_name, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
        if (candidate.fd < 0) continue;

        uint8_t absBits[(ABS_MAX + 8) / 8]{};
        input_absinfo x, y;
        if (ioctl(candidate.fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0 || !testInputBit(ABS_MT_SLOT, absBits) ||
            !testInputBit(ABS_MT_POSITION_X, absBits) || !testInputBit(ABS_MT_POSITION_Y, absBits) ||
            ioctl(candidate.fd, EVIOCGABS(ABS_MT_POSITION_X), &x) < 0 || ioctl(candidate.fd, EVIOCGABS(ABS_MT_POSITION_Y), &y) < 0 ||
            x.maximum <= 0 || y.maximum <= 0)
        {
            close(candidate.fd);
            continue;
        }
        candidate.maxX = x.maximum;
        candidate.maxY = y.maximum;
        candidate.area = static_cast<int64_t>(x.maximum) * y.maximum;
        uint8_t props[(INPUT_PROP_MAX + 8) / 8]{};
        if (ioctl(candidate.fd, EVIOCGPROP(sizeof(props)), props) >= 0)
        {
            candidate.direct = testInputBit(INPUT_PROP_DIRECT, props);
            candidate.pointer = testInputBit(INPUT_PROP_POINTER, props);
        }
        // 依据 EVIOCGPROP 优先选直接触摸、非指针设备；同类取轴面积较大、event 编号较小者。
        if (best.fd < 0 || std::tuple(candidate.direct, !candidate.pointer, candidate.area, -candidate.number) >
                           std::tuple(best.direct, !best.pointer, best.area, -best.number))
            std::swap(best, candidate);
        if (candidate.fd >= 0) close(candidate.fd);
    }
    closedir(dir);
    if (best.fd < 0) return false;

    [[maybe_unused]] char name[128]{}; // 仅在启用调试日志时读取设备名称。
    LS_LOGD_TAG("Touch", "选择设备 path=/dev/input/event%d name=%s direct=%d pointer=%d range=%dx%d", best.number,
                ioctl(best.fd, EVIOCGNAME(sizeof(name)), name) < 0 ? "unknown" : name, best.direct, best.pointer, best.maxX, best.maxY);

    // 在初始化时启动线程，而非静态初始化时启动：调用方可能在此之前 fork。
    Touch_initialized.store(true, std::memory_order_release);
    try
    {
        touchThread = std::thread([fd = best.fd, maxX = float(best.maxX), maxY = float(best.maxY)]
        {
            struct Slot { int id = -1, x = 0, y = 0; } slots[MAX_FINGERS];
            int slot = 0;
            TouchPoint previous;
            pollfd pfd{fd, POLLIN, 0};
            input_event events[64];
            while (Touch_initialized.load(std::memory_order_acquire))
            {
                const int ready = poll(&pfd, 1, 50);
                if (ready == 0 || (ready < 0 && errno == EINTR)) continue;
                if (ready < 0)
                {
                    LS_LOGE_TAG("Touch", "poll 设备失败: %s", strerror(errno));
                    break;
                }
                if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
                {
                    LS_LOGE_TAG("Touch", "输入设备断开 revents=0x%x", pfd.revents);
                    break;
                }
                const ssize_t bytes = read(fd, events, sizeof(events));
                if (bytes < 0 && (errno == EINTR || errno == EAGAIN)) continue;
                if (bytes <= 0)
                {
                    LS_LOGE_TAG("Touch", "读取输入设备失败: %s", bytes == 0 ? "EOF" : strerror(errno));
                    break;
                }
                for (size_t i = 0; i < static_cast<size_t>(bytes) / sizeof(input_event); ++i)
                {
                    const auto &event = events[i];
                    if (event.type == EV_ABS)
                    {
                        if (event.code == ABS_MT_SLOT) slot = event.value >= 0 && event.value < MAX_FINGERS ? event.value : -1;
                        else if (slot >= 0)
                        {
                            switch (event.code)
                            {
                            case ABS_MT_TRACKING_ID: slots[slot].id = event.value; break;
                            case ABS_MT_POSITION_X: slots[slot].x = event.value; break;
                            case ABS_MT_POSITION_Y: slots[slot].y = event.value; break;
                            }
                        }
                        continue;
                    }
                    if (event.type != EV_SYN || event.code != SYN_REPORT) continue;
                    const float w = screenWidth.load(std::memory_order_relaxed), h = screenHeight.load(std::memory_order_relaxed);
                    const auto rotation = orientation.load(std::memory_order_relaxed);
                    std::lock_guard lock(touch_mutex);
                    TouchPoint current;
                    for (int s = 0; s < MAX_FINGERS; ++s)
                    {
                        auto &finger = fingers[0][s];
                        finger.id = slots[s].id;
                        finger.isDown = finger.id >= 0;
                        if (!finger.isDown) continue;
                        const float x = slots[s].x / maxX, y = slots[s].y / maxY;
                        switch (rotation)
                        {
                        case 1: finger.x = y * w; finger.y = (1 - x) * h; break;
                        case 2: finger.x = (1 - x) * w; finger.y = (1 - y) * h; break;
                        case 3: finger.x = (1 - y) * w; finger.y = x * h; break;
                        default: finger.x = x * w; finger.y = y * h; break;
                        }
                        if (!current.isDown) current = finger;
                    }
                    if (current.isDown && (!previous.isDown || current.x != previous.x || current.y != previous.y))
                        enqueueTouchEvent({TouchInputEventType::Position, float(current.x), float(current.y)});
                    if (current.isDown != previous.isDown) enqueueTouchEvent({TouchInputEventType::Button, 0, 0, current.isDown});
                    previous = current;
                }
            }
            std::lock_guard lock(touch_mutex);
            fingers[0] = {};
            if (previous.isDown) enqueueTouchEvent({TouchInputEventType::Button, 0, 0, false});
        });
    }
    catch (const std::exception &error)
    {
        Touch_initialized.store(false, std::memory_order_release);
        close(best.fd);
        LS_LOGE_TAG("Touch", "启动输入线程失败: %s", error.what());
        return false;
    }
    touchFd = best.fd;
    return true;
}

inline void SetInputBlocking(bool block)
{
    if (touchFd >= 0) ioctl(touchFd, EVIOCGRAB, static_cast<int>(block));
}

inline void Touch_Shutdown()
{
    if (!Touch_initialized.exchange(false, std::memory_order_acq_rel)) return;
    touchThread.join();
    close(touchFd); // 同时释放 EVIOCGRAB。
    touchFd = -1;
    std::lock_guard lock(touch_mutex);
    pendingTouchEvents.clear();
}