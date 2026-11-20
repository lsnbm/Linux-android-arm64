#pragma once

#include "../logger/logger.h"
#include "imgui/imgui.h"
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <mutex>
#include <new>
#include <poll.h>
#include <sys/ioctl.h>
#include <system_error>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <utility>

class TouchInput
{
    struct Event
    {
        enum Kind { Position, Press, Release } kind;
        int x = 0, y = 0;
    };
    std::array<Event, 2048> pending;
    size_t count = 0;
    std::mutex mutex;
    int width = 0, height = 0, rotation = 0;
    int fd = -1;
    bool blocking = false;
    std::jthread worker;

    static bool hasBit(int bit, const uint8_t *bits)
    {
        return (bits[bit / 8] & (1u << (bit % 8))) != 0;
    }

public:
    ~TouchInput()
    {
        if (worker.joinable())
        {
            worker.request_stop();
            worker.join();
        }
        if (fd >= 0) close(fd); // close 同时释放 EVIOCGRAB。
    }

    void setScreen(int w, int h, int orientation)
    {
        std::lock_guard lock(mutex);
        width = w;
        height = h;
        rotation = orientation;
    }

    int start()
    {
        int status = 0;
        if (fd >= 0) return status;
        DIR *dir = opendir("/dev/input");
        if (!dir) return status = -errno;

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
            if (ioctl(candidate.fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0 || !hasBit(ABS_MT_SLOT, absBits) ||
                !hasBit(ABS_MT_POSITION_X, absBits) || !hasBit(ABS_MT_POSITION_Y, absBits) ||
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
                candidate.direct = hasBit(INPUT_PROP_DIRECT, props);
                candidate.pointer = hasBit(INPUT_PROP_POINTER, props);
            }
            // EVIOCGPROP 标识直接触摸设备；同类按轴面积和 event 编号排序。
            if (best.fd < 0 || std::tuple(candidate.direct, !candidate.pointer, candidate.area, -candidate.number) >
                               std::tuple(best.direct, !best.pointer, best.area, -best.number))
                std::swap(best, candidate);
            if (candidate.fd >= 0) close(candidate.fd);
        }
        closedir(dir);
        if (best.fd < 0) return status = -ENODEV;
        LS_LOGD_TAG("Touch", "选择设备 /dev/input/event%d range=%dx%d", best.number, best.maxX, best.maxY);
        if (ioctl(best.fd, EVIOCGRAB, 1) < 0)
        {
            status = -errno;
            close(best.fd);
            return status;
        }

        try
        {
            worker = std::jthread([this, input = best.fd, maxX = float(best.maxX), maxY = float(best.maxY)](std::stop_token stop)
            {
                struct Slot { int id = -1, x = 0, y = 0; } slots[10];
                int slot = 0, lastX = 0, lastY = 0;
                bool wasDown = false;
                pollfd pfd{input, POLLIN, 0};
                input_event events[64];
                while (!stop.stop_requested())
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
                    const ssize_t bytes = read(input, events, sizeof(events));
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
                            switch (event.code)
                            {
                            case ABS_MT_SLOT: slot = event.value >= 0 && event.value < static_cast<int>(std::size(slots)) ? event.value : -1; break;
                            case ABS_MT_TRACKING_ID: if (slot >= 0) slots[slot].id = event.value; break;
                            case ABS_MT_POSITION_X: if (slot >= 0) slots[slot].x = event.value; break;
                            case ABS_MT_POSITION_Y: if (slot >= 0) slots[slot].y = event.value; break;
                            }
                            continue;
                        }
                        if (event.type != EV_SYN || event.code != SYN_REPORT) continue;
                        std::lock_guard lock(mutex);
                        const bool resync = count + 2 > pending.size();
                        if (resync) count = 0;
                        bool down = false;
                        for (const auto &finger : slots)
                        {
                            if (finger.id < 0) continue;
                            down = true;
                            const float x = finger.x / maxX, y = finger.y / maxY;
                            int px, py;
                            switch (rotation)
                            {
                            case 1: px = y * width; py = (1 - x) * height; break;
                            case 2: px = (1 - x) * width; py = (1 - y) * height; break;
                            case 3: px = (1 - y) * width; py = x * height; break;
                            default: px = x * width; py = y * height; break;
                            }
                            if (resync || !wasDown || px != lastX || py != lastY) pending[count++] = {Event::Position, px, py};
                            lastX = px;
                            lastY = py;
                            break;
                        }
                        if (resync || down != wasDown) pending[count++] = {down ? Event::Press : Event::Release};
                        wasDown = down;
                    }
                }
                if (wasDown)
                {
                    std::lock_guard lock(mutex);
                    if (count == pending.size()) count = 0;
                    pending[count++] = {Event::Release};
                }
            });
        }
        catch (const std::system_error &error) { status = -error.code().value(); }
        catch (const std::bad_alloc &) { status = -ENOMEM; }
        if (status != 0)
        {
            close(best.fd);
            return status;
        }
        fd = best.fd;
        blocking = true;
        return status;
    }

    int setBlocking(bool block)
    {
        int status = 0;
        if (block == blocking) return status;
        if (ioctl(fd, EVIOCGRAB, static_cast<int>(block)) < 0) return status = -errno;
        blocking = block;
        return status;
    }

    void updateImGui()
    {
        std::lock_guard lock(mutex);
        if (!count) return;
        ImGuiIO &io = ImGui::GetIO();
        io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
        for (size_t i = 0; i < count; ++i)
        {
            const auto &event = pending[i];
            if (event.kind == Event::Position) io.AddMousePosEvent(event.x, event.y);
            else io.AddMouseButtonEvent(0, event.kind == Event::Press);
        }
        count = 0;
    }
};
