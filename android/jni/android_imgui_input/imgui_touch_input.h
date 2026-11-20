#pragma once

#include "../logger/logger.h"
#include "imgui/imgui.h"
#include <array>
#include <cerrno>
#include <charconv>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <dirent.h>
#include <fcntl.h>
#include <linux/input.h>
#include <memory>
#include <mutex>
#include <new>
#include <poll.h>
#include <sys/ioctl.h>
#include <system_error>
#include <thread>
#include <tuple>
#include <unistd.h>
#include <utility>
#include <vector>

// 快照仅包含仍按下的触点；slot 是当前触摸设备的 ABS_MT_SLOT。
// 坐标与 setScreen 指定的屏幕尺寸/方向一致，不依赖 ImGui 帧刷新。
struct TouchPoint
{
    int slot;
    float x, y;
};

struct TouchDebugView
{
    bool showPoints, showRect;
    ImVec2 rectMin, rectMax;
    const char *hitLabel;
};

class TouchInput
{
    mutable std::mutex mutex;

    struct Event
    {
        enum Kind
        {
            Position,
            Press,
            Release
        } kind;
        int x = 0, y = 0;
    };
    std::array<Event, 2048> pending;
    size_t count = 0;
    struct Slot
    {
        int id = -1, x = 0, y = 0;
    };
    struct Contact
    {
        int slot;
        ImVec2 pos;
    };
    std::vector<Contact> reported; // 最后一次完整 SYN_REPORT，归一化坐标，受 mutex 保护。
    std::vector<Contact> frame;    // 渲染线程在 NewFrame 之前锁存的屏幕坐标。
    int width = 0, height = 0, rotation = 0;
    int fd = -1;
    int workerStatus = 0;
    bool blocking = false;
    std::jthread worker;

    static bool hasBit(int bit, const uint8_t *bits)
    {
        return (bits[bit / 8] & (1u << (bit % 8))) != 0;
    }

    ImVec2 screenPosition(ImVec2 p) const
    {
        switch (rotation)
        {
        case 1:
            return {p.y * width, (1 - p.x) * height};
        case 2:
            return {(1 - p.x) * width, (1 - p.y) * height};
        case 3:
            return {(1 - p.y) * width, p.x * height};
        default:
            return {p.x * width, p.y * height};
        }
    }

    static void ReadEvents(std::stop_token stop, TouchInput *self, int input, int minX, int minY, double dx, double dy, std::vector<Slot> slots)
    {
        // 各触点先缓存在输入线程，等 SYN_REPORT 到达再提交一批事件。
        int status = 0;
        int slot = 0, lastX = 0, lastY = 0;
        bool wasDown = false;
        pollfd pfd{input, POLLIN, 0};
        input_event events[64];
        while (!stop.stop_requested())
        {
            // 50 ms 超时使空闲设备也能及时响应退出请求。
            const int ready = poll(&pfd, 1, 50);
            if (ready == 0 || (ready < 0 && errno == EINTR)) continue;
            if (ready < 0)
            {
                status = -errno;
                LS_LOGE_TAG("Touch", "poll 设备失败: %s", strerror(-status));
                break;
            }
            if (pfd.revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                status = (pfd.revents & POLLNVAL) ? -EBADF : -ENODEV;
                LS_LOGE_TAG("Touch", "输入设备断开 revents=0x%x", pfd.revents);
                break;
            }
            const ssize_t bytes = read(input, events, sizeof(events));
            if (bytes < 0 && (errno == EINTR || errno == EAGAIN)) continue;
            if (bytes <= 0)
            {
                status = bytes == 0 ? -ENODEV : -errno;
                LS_LOGE_TAG("Touch", "读取输入设备失败: %s", bytes == 0 ? "EOF" : strerror(-status));
                break;
            }
            for (size_t i = 0; i < static_cast<size_t>(bytes) / sizeof(input_event); ++i)
            {
                const auto &event = events[i];
                if (event.type == EV_ABS)
                {
                    switch (event.code)
                    {
                    case ABS_MT_SLOT:
                        slot = event.value >= 0 && event.value < static_cast<int>(std::size(slots)) ? event.value : -1;
                        break;
                    case ABS_MT_TRACKING_ID:
                        if (slot >= 0) slots[slot].id = event.value;
                        break;
                    case ABS_MT_POSITION_X:
                        if (slot >= 0) slots[slot].x = event.value;
                        break;
                    case ABS_MT_POSITION_Y:
                        if (slot >= 0) slots[slot].y = event.value;
                        break;
                    }
                    continue;
                }
                if (event.type != EV_SYN || event.code != SYN_REPORT) continue;
                // 输入线程只加锁提交事件，不直接调用 ImGui。
                std::lock_guard lock(self->mutex);
                const bool resync = self->count + 2 > self->pending.size();
                if (resync) self->count = 0;
                self->reported.clear();
                for (size_t index = 0; index < slots.size(); ++index)
                {
                    const auto &finger = slots[index];
                    if (finger.id < 0) continue;
                    self->reported.push_back({static_cast<int>(index), {static_cast<float>((double(finger.x) - minX) / dx), static_cast<float>((double(finger.y) - minY) / dy)}});
                }
                const bool down = !self->reported.empty();
                if (down) // 菜单只收到一个主指针，其他手指只用于调试绘制。
                {
                    const auto p = self->screenPosition(self->reported.front().pos);
                    const int px = static_cast<int>(p.x), py = static_cast<int>(p.y);
                    if (resync || !wasDown || px != lastX || py != lastY) self->pending[self->count++] = {Event::Position, px, py};
                    lastX = px;
                    lastY = py;
                }
                if (resync || down != wasDown) self->pending[self->count++] = {down ? Event::Press : Event::Release};
                wasDown = down;
            }
        }
        std::lock_guard lock(self->mutex);
        self->workerStatus = status;
        self->reported.clear();
        if (wasDown)
        {
            if (self->count == self->pending.size()) self->count = 0;
            self->pending[self->count++] = {Event::Release};
        }
    }

    void shutdown()
    {
        if (worker.joinable())
        {
            worker.request_stop();
            worker.join();
        }
        if (fd >= 0) close(std::exchange(fd, -1)); // close 同时释放 EVIOCGRAB。
    }

public:
    TouchInput() = default;
    TouchInput(const TouchInput &) = delete;
    TouchInput &operator=(const TouchInput &) = delete;
    ~TouchInput() { shutdown(); }

    // 更新屏幕信息，输入线程按同一组尺寸和方向换算坐标。
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
        std::lock_guard startLock(mutex);
        if (fd >= 0) return status = workerStatus;
        DIR *dir = opendir("/dev/input");
        if (!dir) return status = -errno;

        struct Candidate
        {
            int fd = -1, number = 0, minX = 0, minY = 0, maxX = 0, maxY = 0, slotCount = 0;
            bool direct = false, pointer = false;
            int64_t area = 0;
        } best;
        int unavailable = -ENODEV;
        while (const auto *entry = readdir(dir))
        {
            if (strncmp(entry->d_name, "event", 5) != 0) continue;
            Candidate candidate;
            const char *end = entry->d_name + strlen(entry->d_name);
            const auto number = std::from_chars(entry->d_name + 5, end, candidate.number);
            if (number.ec != std::errc{} || number.ptr != end || candidate.number < 0) continue;
            // 非阻塞读取配合 poll；没有新事件时不让 read 阻塞退出。
            candidate.fd = openat(dirfd(dir), entry->d_name, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
            if (candidate.fd < 0)
            {
                if (errno == EACCES || errno == EPERM) unavailable = -errno;
                continue;
            }

            uint8_t absBits[(ABS_MAX + 8) / 8]{};
            input_absinfo x{}, y{}, slotAxis{};
            if (ioctl(candidate.fd, EVIOCGBIT(EV_ABS, sizeof(absBits)), absBits) < 0 || !hasBit(ABS_MT_SLOT, absBits) || !hasBit(ABS_MT_POSITION_X, absBits) || !hasBit(ABS_MT_POSITION_Y, absBits) || ioctl(candidate.fd, EVIOCGABS(ABS_MT_POSITION_X), &x) < 0 || ioctl(candidate.fd, EVIOCGABS(ABS_MT_POSITION_Y), &y) < 0 || ioctl(candidate.fd, EVIOCGABS(ABS_MT_SLOT), &slotAxis) < 0 || slotAxis.minimum != 0 || slotAxis.maximum < 0 || slotAxis.maximum > 65535 || x.maximum <= x.minimum || y.maximum <= y.minimum)
            {
                close(candidate.fd);
                continue;
            }
            candidate.slotCount = slotAxis.maximum + 1;
            candidate.minX = x.minimum;
            candidate.minY = y.minimum;
            candidate.maxX = x.maximum;
            candidate.maxY = y.maximum;
            candidate.area = static_cast<int64_t>(x.maximum) * y.maximum;
            uint8_t props[(INPUT_PROP_MAX + 8) / 8]{};
            if (ioctl(candidate.fd, EVIOCGPROP(sizeof(props)), props) >= 0)
            {
                candidate.direct = hasBit(INPUT_PROP_DIRECT, props);
                candidate.pointer = hasBit(INPUT_PROP_POINTER, props);
            }
            // 依据 EVIOCGPROP 优先选直接触摸、非指针设备；同类取轴面积较大、event 编号较小者。
            if (best.fd < 0 || std::tuple(candidate.direct, !candidate.pointer, candidate.area, -candidate.number) > std::tuple(best.direct, !best.pointer, best.area, -best.number)) std::swap(best, candidate);
            if (candidate.fd >= 0) close(candidate.fd);
        }
        closedir(dir);
        if (best.fd < 0) return status = unavailable;
        struct CloseOnReturn
        {
            int &fd;
            ~CloseOnReturn() { if (fd >= 0) close(fd); }
        } closeOnReturn{best.fd};
        LS_LOGD_TAG("Touch", "选择设备 /dev/input/event%d range=%dx%d", best.number, best.maxX, best.maxY);
        if (ioctl(best.fd, EVIOCGRAB, 1) < 0) return status = -errno;

        try
        {
            reported.reserve(best.slotCount);
            frame.reserve(best.slotCount);
            // 在初始化时启动线程，而非静态初始化时启动：调用方可能在此之前 fork。
            worker = std::jthread(ReadEvents, this, best.fd, best.minX, best.minY,
                                  double(best.maxX) - best.minX, double(best.maxY) - best.minY, std::vector<Slot>(best.slotCount));
        }
        catch (const std::system_error &error)
        {
            status = -error.code().value();
        }
        catch (const std::bad_alloc &)
        {
            status = -ENOMEM;
        }
        if (status != 0) return status;
        fd = std::exchange(best.fd, -1);
        blocking = true;
        return status;
    }

    // 可从任意线程读取；返回本实例最近一次完整上报中仍按下的触点副本。
    [[nodiscard]] std::vector<TouchPoint> snapshot() const
    {
        std::lock_guard lock(mutex);
        if (width <= 0 || height <= 0) return {};
        std::vector<TouchPoint> points;
        points.reserve(reported.size());
        for (const auto &contact : reported)
        {
            const auto p = screenPosition(contact.pos);
            points.push_back({contact.slot, p.x, p.y});
        }
        return points;
    }

    int setBlocking(bool block)
    {
        int status = 0;
        std::lock_guard lock(mutex);
        if (fd < 0) return status = -ENODEV;
        if (workerStatus != 0) return status = workerStatus;
        if (block == blocking) return status;
        // EVIOCGRAB：1 启用独占，0 取消独占。
        if (ioctl(fd, EVIOCGRAB, static_cast<int>(block)) < 0) return status = -errno;
        blocking = block;
        return status;
    }

    // 在渲染线程的 ImGui::NewFrame 之前消费已提交的触摸事件。
    void updateImGui()
    {
        std::lock_guard lock(mutex);
        frame.clear();
        for (const auto &contact : reported) frame.push_back({contact.slot, screenPosition(contact.pos)});
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

    // 仅在绘制回调中使用；点与矩形使用同一帧，不读取正在写入的输入状态。
    void drawTouchDebug(ImDrawList &draw, const TouchDebugView &view) const
    {
        bool hit = false;
        for (const auto &[slot, p] : frame)
        {
            if (view.showRect && p.x >= view.rectMin.x && p.x < view.rectMax.x && p.y >= view.rectMin.y && p.y < view.rectMax.y) hit = true;
            if (!view.showPoints) continue;
            draw.AddCircleFilled(p, 15.0f, IM_COL32(255, 0, 0, 200));
            char label[24];
            std::snprintf(label, sizeof(label), "ID:%d", slot);
            draw.AddText({p.x + 10.0f, p.y}, IM_COL32_WHITE, label);
        }
        if (view.showRect)
        {
            draw.AddRect(view.rectMin, view.rectMax, hit ? IM_COL32(255, 200, 0, 255) : IM_COL32(0, 255, 0, 200), 0.0f, 2.0f);
            if (hit && view.hitLabel) draw.AddText(view.rectMin, IM_COL32_WHITE, view.hitLabel);
        }
    }
};

inline std::unique_ptr<TouchInput> gTouchInput = nullptr;
