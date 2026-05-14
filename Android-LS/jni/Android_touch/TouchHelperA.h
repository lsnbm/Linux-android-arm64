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
#include "ImGui/imgui.h"
#include <atomic>
#include <future>
#include <vector>
#include <mutex>
#include <string.h>
#include <errno.h>
#include <algorithm>
#include "Utils/ThreadPool.h"

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

// 设备配置结构
struct DeviceConfig
{
    int deviceIndex;
    int maxX;
    int maxY;
    int deviceFd;
    std::future<void> task;
};

// 全局变量
static TouchPoint fingers[MAX_DEVICES][MAX_FINGERS];
static std::vector<DeviceConfig> devices;
static std::mutex touch_mutex; // 全局触摸数据锁

// 更新屏幕信息
inline void UpdateScreenData(int w, int h, uint32_t orientation_)
{
    screenWidth.store((float)w, std::memory_order_relaxed);
    screenHeight.store((float)h, std::memory_order_relaxed);
    orientation.store(orientation_, std::memory_order_relaxed);
}

static bool isMultiTouchDevice(int fd, const char *device_path)
{
    uint8_t abs_bits[(ABS_MAX + 7) / 8];
    if (ioctl(fd, EVIOCGBIT(EV_ABS, sizeof(abs_bits)), abs_bits) < 0)
        return false;

#define test_bit(bit, array) (array[bit / 8] & (1 << (bit % 8)))
    return test_bit(ABS_MT_SLOT, abs_bits) &&
           test_bit(ABS_MT_POSITION_X, abs_bits) &&
           test_bit(ABS_MT_POSITION_Y, abs_bits);
#undef test_bit
}

// 触摸事件处理线程
static void deviceHandlerThread(DeviceConfig *config)
{
    int deviceIndex = config->deviceIndex;
    int fd = config->deviceFd;

    float hwMaxX = (float)(config->maxX > 0 ? config->maxX : 1);
    float hwMaxY = (float)(config->maxY > 0 ? config->maxY : 1);

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
        if (poll_ret <= 0)
            continue; // 超时或被中断

        auto readSize = read(fd, inputEvents, sizeof(inputEvents));
        if (readSize <= 0)
        {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                continue;
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
                    if (currentSlot >= MAX_FINGERS)
                        currentSlot = MAX_FINGERS - 1;
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

                std::lock_guard<std::mutex> lock(touch_mutex); // 保护全局数组
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
                // 注意：这里已经移除了 ImGui 的操作，交由主线程处理！
            }
        }
    }
    close(fd);
}

// 需在你的主循环中调用 (ImGui::NewFrame 之前)
void Touch_UpdateImGui()
{
    if (!Touch_initialized.load(std::memory_order_acquire))
        return;

    std::lock_guard<std::mutex> lock(touch_mutex);
    ImGuiIO &io = ImGui::GetIO();

    bool is_any_down = false;
    for (size_t d = 0; d < devices.size(); ++d)
    {
        for (int f = 0; f < MAX_FINGERS; ++f)
        {
            if (fingers[d][f].isDown)
            {
                io.MousePos = ImVec2((float)fingers[d][f].x, (float)fingers[d][f].y);
                is_any_down = true;
                break; // 仅取第一个手指给 ImGui
            }
        }
        if (is_any_down)
            break;
    }

    io.MouseDown[0] = is_any_down;
}

bool Touch_Init()
{
    if (Touch_initialized.load(std::memory_order_acquire))
        return true;

    DIR *dir = opendir("/dev/input/");
    if (!dir)
        return false;

    dirent *ptr = NULL;
    char device_path[256];

    struct DeviceInfo
    {
        int fd, eventNum, maxX, maxY;
    };
    std::vector<DeviceInfo> found_devices;

    while ((ptr = readdir(dir)) != NULL)
    {
        if (strncmp(ptr->d_name, "event", 5) == 0)
        {
            snprintf(device_path, sizeof(device_path), "/dev/input/%s", ptr->d_name);
            // 依然使用 O_NONBLOCK，配合 poll 完美运行
            int fd = open(device_path, O_RDONLY | O_NONBLOCK);
            if (fd < 0)
                continue;

            if (isMultiTouchDevice(fd, device_path))
            {
                DeviceInfo info;
                info.fd = fd;
                sscanf(ptr->d_name, "event%d", &info.eventNum);

                input_absinfo absX, absY;
                if (ioctl(fd, EVIOCGABS(ABS_MT_POSITION_X), &absX) == 0 &&
                    ioctl(fd, EVIOCGABS(ABS_MT_POSITION_Y), &absY) == 0)
                {
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

    if (found_devices.empty())
        return false;

    std::sort(found_devices.begin(), found_devices.end(), [](const DeviceInfo &a, const DeviceInfo &b)
              { return a.eventNum < b.eventNum; });

    for (size_t i = 1; i < found_devices.size(); ++i)
        close(found_devices[i].fd);
    found_devices.resize(1);

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

        config_ref.task = Utils::GlobalPool.push([config = &config_ref]
                                                 { deviceHandlerThread(config); });
    }
    return true;
}

// 1启用独占，0取消独占
void SetInputBlocking(bool block)
{
    for (const auto &dev : devices)
    {
        if (dev.deviceFd >= 0)
            ioctl(dev.deviceFd, EVIOCGRAB, block ? 1 : 0);
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

                if (touchX >= rx && touchX <= rx + rw &&
                    touchY >= ry && touchY <= ry + rh)
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
    if (!Touch_initialized.load(std::memory_order_acquire))
        return;

    SetInputBlocking(false);
    Touch_initialized.store(false, std::memory_order_release);

    for (size_t i = 0; i < devices.size(); ++i)
        if (devices[i].task.valid())
            devices[i].task.wait();
    devices.clear();
}
