#pragma once
#include <stdio.h>
#include <stdlib.h>
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <cmath>
#include <linux/input.h>
#include <linux/uinput.h>
#include <poll.h> // 引入 poll 机制
#include "imgui/imgui.h"
#include <atomic>
#include <thread>
#include <vector>
#include <mutex>
#include <string.h>
#include <errno.h>
#include <algorithm>

#define MAX_DEVICES 5
#define MAX_FINGERS 10

// 全局状态变量
static std::atomic<uint32_t> orientation{0};
static std::atomic<float> screenHeight{0}, screenWidth{0};
static std::atomic<bool> Touch_initialized{false};

// 触摸点对象结构
struct TouchPoint
{
    bool isDown = false;
    int x = 0;
    int y = 0;
    int id = -1;
};

enum class TouchInputEventType
{
    Position,
    Button
};

struct TouchInputEvent
{
    TouchInputEventType type;
    float x = 0.0f;
    float y = 0.0f;
    bool down = false;
};

// 设备配置结构
struct DeviceConfig
{
    int deviceIndex;
    int maxX;
    int maxY;
    int deviceFd;
    std::thread task;
};

// 全局变量
static TouchPoint fingers[MAX_DEVICES][MAX_FINGERS];
static std::vector<DeviceConfig> devices;
static std::vector<TouchInputEvent> pendingTouchEvents;
static std::mutex touch_mutex; // 全局触摸数据锁

static bool testInputBit(int bit, const uint8_t *array)
{
    return (array[bit / 8] & (1u << (bit % 8))) != 0;
}

// 更新屏幕信息
inline void UpdateScreenData(int w, int h, uint32_t orientation_)
{
    screenWidth.store((float)w, std::memory_order_relaxed);
    screenHeight.store((float)h, std::memory_order_relaxed);
    orientation.store(orientation_, std::memory_order_relaxed);
}

static bool isMultiTouchDevice(int fd)
{
    uint8_t abs_bits[(ABS_MAX + 8) / 8] = {};
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0) return false;

    return testInputBit(ABS_MT_SLOT, abs_bits) && testInputBit(ABS_MT_POSITION_X, abs_bits) && testInputBit(ABS_MT_POSITION_Y, abs_bits);
}

// 触摸事件处理线程。参数按值传入，避免 devices 扩容或清理后留下悬空指针。
static void deviceHandlerThread(int deviceIndex, int fd, int maxX, int maxY)
{
    float hwMaxX = (float)(maxX > 0 ? maxX : 1);
    float hwMaxY = (float)(maxY > 0 ? maxY : 1);

    input_event inputEvents[64];
    int currentSlot = 0;

    // 线程本地缓存（等 SYN_REPORT 到了再统一提交）
    bool slot_active[MAX_FINGERS] = {false};
    int slot_raw_x[MAX_FINGERS] = {0};
    int slot_raw_y[MAX_FINGERS] = {0};
    int slot_id[MAX_FINGERS] = {-1};

    struct pollfd pfd;
    pfd.fd = fd;
    pfd.events = POLLIN;

    while (Touch_initialized.load(std::memory_order_acquire))
    {
        // 使用 poll 等待输入事件，超时设置为 50ms 以便及时响应退出指令
        int poll_ret = poll(&pfd, 1, 50);
        if (poll_ret == 0) continue;
        if (poll_ret < 0)
        {
            if (errno == EINTR) continue;
            fprintf(stderr, "[Touch Error] poll failed for event device: %s\n", strerror(errno));
            break;
        }
        if ((pfd.revents & (POLLERR | POLLHUP | POLLNVAL)) != 0)
        {
            fprintf(stderr, "[Touch Error] event device disconnected, revents=0x%x\n", pfd.revents);
            break;
        }

        auto readSize = read(fd, inputEvents, sizeof(inputEvents));
        if (readSize <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK) continue;
            fprintf(stderr, "[Touch Error] read failed for event device: %s\n", strerror(errno));
            break; // 设备断开
        }

        size_t count = size_t(readSize) / sizeof(input_event);
        for (size_t i = 0; i < count; i++)
        {
            input_event &ie = inputEvents[i];

            if (ie.type == EV_ABS)
            {
                switch (ie.code)
                {
                case ABS_MT_SLOT:
                    currentSlot = ie.value;
                    if (currentSlot >= MAX_FINGERS) currentSlot = MAX_FINGERS - 1;
                    break;
                case ABS_MT_TRACKING_ID:
                    if (ie.value == -1)
                    {
                        slot_active[currentSlot] = false;
                        slot_id[currentSlot] = -1;
                    }
                    else
                    {
                        slot_active[currentSlot] = true;
                        slot_id[currentSlot] = ie.value;
                    }
                    break;
                case ABS_MT_POSITION_X:
                    slot_raw_x[currentSlot] = ie.value;
                    break;
                case ABS_MT_POSITION_Y:
                    slot_raw_y[currentSlot] = ie.value;
                    break;
                }
            }
            else if (ie.type == EV_SYN && ie.code == SYN_REPORT)
            {
                // 收到同步信号，计算坐标并加锁提交到全局变量
                float sw = screenWidth.load(std::memory_order_relaxed);
                float sh = screenHeight.load(std::memory_order_relaxed);
                uint32_t orient = orientation.load(std::memory_order_relaxed);

                std::lock_guard<std::mutex> lock(touch_mutex); // 保护全局触摸状态和待提交事件

                bool wasDown = false;
                int previousX = 0;
                int previousY = 0;
                for (int s = 0; s < MAX_FINGERS; ++s)
                {
                    if (!fingers[deviceIndex][s].isDown) continue;
                    wasDown = true;
                    previousX = fingers[deviceIndex][s].x;
                    previousY = fingers[deviceIndex][s].y;
                    break;
                }

                for (int s = 0; s < MAX_FINGERS; s++)
                {
                    fingers[deviceIndex][s].isDown = slot_active[s];
                    fingers[deviceIndex][s].id = slot_id[s];

                    if (slot_active[s])
                    {
                        float normX = (float)slot_raw_x[s] / hwMaxX;
                        float normY = (float)slot_raw_y[s] / hwMaxY;
                        float fx = normX * sw, fy = normY * sh;

                        switch (orient)
                        {
                        case 1:
                            fx = normY * sw;
                            fy = (1.0f - normX) * sh;
                            break;
                        case 2:
                            fx = (1.0f - normX) * sw;
                            fy = (1.0f - normY) * sh;
                            break;
                        case 3:
                            fx = (1.0f - normY) * sw;
                            fy = normX * sh;
                            break;
                        }

                        fingers[deviceIndex][s].x = (int)fx;
                        fingers[deviceIndex][s].y = (int)fy;
                    }
                }

                bool isDown = false;
                int currentX = 0;
                int currentY = 0;
                for (int s = 0; s < MAX_FINGERS; ++s)
                {
                    if (!fingers[deviceIndex][s].isDown) continue;
                    isDown = true;
                    currentX = fingers[deviceIndex][s].x;
                    currentY = fingers[deviceIndex][s].y;
                    break; // 仅取第一个手指给 ImGui
                }

                if (isDown && (!wasDown || currentX != previousX || currentY != previousY))
                {
                    pendingTouchEvents.push_back({TouchInputEventType::Position, (float)currentX, (float)currentY, false});
                }
                if (wasDown != isDown)
                {
                    pendingTouchEvents.push_back({TouchInputEventType::Button, 0.0f, 0.0f, isDown});
                }
                // 注意：这里已经移除了 ImGui 的操作，交由主线程处理！
            }
        }
    }
    close(fd);
}

// 需在你的主循环中调用 (ImGui::NewFrame 之前)
void Touch_UpdateImGui()
{
    if (!Touch_initialized.load(std::memory_order_acquire)) return;

    std::vector<TouchInputEvent> events;
    {
        std::lock_guard<std::mutex> lock(touch_mutex);
        events.swap(pendingTouchEvents);
    }
    if (events.empty()) return;

    ImGuiIO &io = ImGui::GetIO();
    io.AddMouseSourceEvent(ImGuiMouseSource_TouchScreen);
    for (const TouchInputEvent &event : events)
    {
        if (event.type == TouchInputEventType::Position)
            io.AddMousePosEvent(event.x, event.y);
        else
            io.AddMouseButtonEvent(0, event.down);
    }
}

bool Touch_Init()
{
    if (Touch_initialized.load(std::memory_order_acquire)) return true;

    {
        std::lock_guard<std::mutex> lock(touch_mutex);
        pendingTouchEvents.clear();
        for (auto &deviceFingers : fingers)
            for (TouchPoint &finger : deviceFingers)
                finger = {};
    }

    DIR *dir = opendir("/dev/input/");
    if (!dir) return false;

    dirent *ptr = NULL;
    char device_path[256];

    struct DeviceInfo
    {
        int fd, eventNum, maxX, maxY;
        bool isDirect, isPointer;
        char path[256];
        char name[128];
    };
    std::vector<DeviceInfo> found_devices;

    while ((ptr = readdir(dir)) != NULL)
    {
        if (strncmp(ptr->d_name, "event", 5) == 0)
        {
            snprintf(device_path, sizeof(device_path), "/dev/input/%s", ptr->d_name);
            // 依然使用 O_NONBLOCK，配合 poll 完美运行
            int fd = open(device_path, O_RDONLY | O_NONBLOCK);
            if (fd < 0) continue;

            if (isMultiTouchDevice(fd))
            {
                DeviceInfo info;
                info.fd = fd;
                info.isDirect = false;
                info.isPointer = false;
                snprintf(info.path, sizeof(info.path), "%s", device_path);
                memset(info.name, 0, sizeof(info.name));
                if (ioctl(fd, EVIOCGNAME(sizeof(info.name)), info.name) < 0)
                    snprintf(info.name, sizeof(info.name), "unknown");
                sscanf(ptr->d_name, "event%d", &info.eventNum);

                input_absinfo absX, absY;
                if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &absX) == 0 && ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &absY) == 0)
                {
                    uint8_t prop_bits[(INPUT_PROP_MAX + 8) / 8] = {};
                    if (ioctl(fd, EVIOCGPROP(sizeof(prop_bits)), prop_bits) == 0)
                    {
                        info.isDirect = testInputBit(INPUT_PROP_DIRECT, prop_bits);
                        info.isPointer = testInputBit(INPUT_PROP_POINTER, prop_bits);
                    }

                    info.maxX = absX.maximum;
                    info.maxY = absY.maximum;
                    found_devices.push_back(info);
                }
                else
                {
                    close(fd);
                }
            }
            else
            {
                close(fd);
            }
        }
    }
    closedir(dir);

    if (found_devices.empty()) return false;

    std::sort(found_devices.begin(), found_devices.end(),
              [](const DeviceInfo &a, const DeviceInfo &b)
              {
                  if (a.isDirect != b.isDirect) return a.isDirect;
                  if (a.isPointer != b.isPointer) return !a.isPointer;

                  long long areaA = (long long)a.maxX * (long long)a.maxY;
                  long long areaB = (long long)b.maxX * (long long)b.maxY;
                  if (areaA != areaB) return areaA > areaB;

                  return a.eventNum < b.eventNum;
              });

    for (size_t i = 1; i < found_devices.size(); ++i) close(found_devices[i].fd);
    found_devices.resize(1);

    devices.clear();
    devices.reserve(found_devices.size());
    Touch_initialized.store(true, std::memory_order_release);

    for (const auto &dev_info : found_devices)
    {
        if (devices.size() >= MAX_DEVICES)
        {
            close(dev_info.fd);
            continue;
        }

        devices.emplace_back();
        DeviceConfig &config_ref = devices.back();
        config_ref.deviceIndex = devices.size() - 1;
        config_ref.deviceFd = dev_info.fd;
        config_ref.maxX = dev_info.maxX;
        config_ref.maxY = dev_info.maxY;

        fprintf(stderr,
            "[Touch] selected %s name=%s direct=%d pointer=%d range=%dx%d\n",
            dev_info.path,
            dev_info.name,
            dev_info.isDirect ? 1 : 0,
            dev_info.isPointer ? 1 : 0,
            dev_info.maxX,
            dev_info.maxY);

        // 必须在 Touch_Init() 中创建线程。很多调用方会在 main() 内先 fork()
        // 守护化；若在线程池全局构造阶段提前创建线程，fork 后子进程只保留
        // 调用线程，继承的线程池将永远没有 worker 来消费触摸任务。
        config_ref.task = std::thread(deviceHandlerThread,
                          config_ref.deviceIndex,
                          config_ref.deviceFd,
                          config_ref.maxX,
                          config_ref.maxY);
    }
    return true;
}

// 1启用独占，0取消独占
void SetInputBlocking(bool block)
{
    for (const auto &dev : devices)
    {
        if (dev.deviceFd >= 0) ioctl(dev.deviceFd, EVIOCGRAB, block ? 1 : 0);
    }
}

// 检测是否有手指在矩形区域内
bool IsUserTouchingRect(float rx, float ry, float rw, float rh)
{
    std::lock_guard<std::mutex> lock(touch_mutex);
    for (size_t d = 0; d < devices.size(); ++d)
    {
        for (int f = 0; f < MAX_FINGERS; ++f)
        {
            if (fingers[d][f].isDown)
            {
                int touchX = fingers[d][f].x;
                int touchY = fingers[d][f].y;

                if (touchX >= rx && touchX <= rx + rw && touchY >= ry && touchY <= ry + rh)
                {
                    return true;
                }
            }
        }
    }
    return false;
}

void Touch_Shutdown()
{
    if (!Touch_initialized.load(std::memory_order_acquire)) return;

    SetInputBlocking(false);
    Touch_initialized.store(false, std::memory_order_release);

    for (size_t i = 0; i < devices.size(); ++i)
        if (devices[i].task.joinable()) devices[i].task.join();
    devices.clear();

    std::lock_guard<std::mutex> lock(touch_mutex);
    pendingTouchEvents.clear();
    for (auto &deviceFingers : fingers)
        for (TouchPoint &finger : deviceFingers)
            finger = {};
}