#ifndef SYSCALL_READ_H
#define SYSCALL_READ_H

#include <cstdio>
#include <sys/mman.h>
#include <cerrno>
#include <cstring>
#include <atomic>
#include <thread>
#include <mutex>
#include <sys/prctl.h>

// 12月2日21:36开始记录修复问题:
/*
1.修复多线程竞争驱动资源，无锁导致的多线程修改共享内存数据状态错误导致死机
解决方案：加锁
2.用户调用read读取字节大于1024导致溢出，内存越界，导致后面变量状态错误导致的死机
解决方案：循环分片读写
3.

*/

//__attribute__((noinline)) // 禁止该类所有成员函数成员变量内联
//__attribute__((optimize("-fno-reorder-blocks,-fno-reorder-functions"))) // 禁止编译器重排代码
class Driver
{
public:
    Driver(bool touch) // 为真开启触摸
    {
        InitCommunication();
        if (touch)
        {
            InitTouch();
        }
    }

    ~Driver()
    {
        DelTouch(); // 先清理触摸
        ExitCommunication();
    }

    void ExitKernelThread()
    {
        std::lock_guard<std::mutex> lock(MMutex);
        MIoPacket->Op = kexit;
        IoCommitAndWait();
    }

    void NullIo()
    {
        std::lock_guard<std::mutex> lock(MMutex);
        MIoPacket->Op = op_o;
        IoCommitAndWait();
    }

    int GetPid(char *packageName)
    {
        int pid;
        FILE *fp;
        char cmd[0x100] = "pidof ";
        strcat(cmd, packageName);
        fp = popen(cmd, "r");
        fscanf(fp, "%d", &pid);
        pclose(fp);

        return pid;
    }

    void SetGlobalPid(pid_t pid)
    {
        // 修改全局PID建议也加锁，防止读写期间PID被突然改变
        std::lock_guard<std::mutex> lock(MMutex);
        GlobalPid = pid;
    }

    // 内核接口
    template <typename T>
    T Read(uint64_t address)
    {
        T value = {};
        KReadProcessMemory(GlobalPid, address, &value, sizeof(T));
        return value;
    }

    bool Read(uint64_t address, void *buffer, size_t size)
    {
        return KReadProcessMemory(GlobalPid, address, buffer, size);
    }

    template <typename T>
    bool Write(uint64_t address, const T &value)
    {
        return KWriteProcessMemory(GlobalPid, address, const_cast<T *>(&value), sizeof(T));
    }

    bool Write(uint64_t address, void *buffer, size_t size)
    {
        return KWriteProcessMemory(GlobalPid, address, buffer, size);
    }

    int GetModuleBase(const char moduleName[46], short segmentIndex, uint64_t *moduleAddr)
    {
        std::lock_guard<std::mutex> lock(MMutex); // 加锁

        MIoPacket->Op = op_m;
        MIoPacket->TargetProcessId = GlobalPid;
        snprintf(MIoPacket->ModuleName, 46, "%s", moduleName);
        MIoPacket->SegmentIndex = segmentIndex;

        IoCommitAndWait();

        *moduleAddr = MIoPacket->ModuleBaseAddress;
        return MIoPacket->status;
    }

    void TouchDown(int x, int y, int screenW, int screenH)
    {
        std::lock_guard<std::mutex> lock(MMutex); // 加锁

        if (screenW > 0 && screenH > 0 && MIoPacket->POSITION_X > 0 && MIoPacket->POSITION_Y > 0)
        {
            MIoPacket->Op = op_down;

            // 判断是否是横屏游戏 (宽 > 高) 且 驱动是竖屏 (驱动X < 驱动Y)
            if (screenW > screenH && MIoPacket->POSITION_X < MIoPacket->POSITION_Y)
            {
                // 充电口在右边的情况处理横屏映射
                MIoPacket->x = static_cast<int>((1.0 - (double)y / screenH) * MIoPacket->POSITION_X);
                MIoPacket->y = static_cast<int>((double)x / screenW * MIoPacket->POSITION_Y);

                // 充电口在左边的情况处理横屏映射
                // MIoPacket->x = static_cast<int>((double)y / screenH * MIoPacket->POSITION_X);
                // MIoPacket->y = static_cast<int>((1.0 - (double)x / screenW) * MIoPacket->POSITION_Y);
            }
            else
            {
                // 竖屏
                MIoPacket->x = static_cast<int>((double)x / screenW * MIoPacket->POSITION_X);
                MIoPacket->y = static_cast<int>((double)y / screenH * MIoPacket->POSITION_Y);
            }

            IoCommitAndWait();
        }
    }

    void TouchMove(int x, int y, int screenW, int screenH)
    {
        std::lock_guard<std::mutex> lock(MMutex); // 加锁

        if (screenW > 0 && screenH > 0 && MIoPacket->POSITION_X > 0 && MIoPacket->POSITION_Y > 0)
        {
            MIoPacket->Op = op_move;

            // 逻辑同上
            if (screenW > screenH && MIoPacket->POSITION_X < MIoPacket->POSITION_Y)
            {
                // 充电口在右边的情况处理横屏映射
                MIoPacket->x = static_cast<int>((1.0 - (double)y / screenH) * MIoPacket->POSITION_X);
                MIoPacket->y = static_cast<int>((double)x / screenW * MIoPacket->POSITION_Y);

                // 充电口在左边的情况处理横屏映射
                // MIoPacket->x = static_cast<int>((double)y / screenH * MIoPacket->POSITION_X);
                // MIoPacket->y = static_cast<int>((1.0 - (double)x / screenW) * MIoPacket->POSITION_Y);
            }
            else
            {
                // 正常映射
                MIoPacket->x = static_cast<int>((double)x / screenW * MIoPacket->POSITION_X);
                MIoPacket->y = static_cast<int>((double)y / screenH * MIoPacket->POSITION_Y);
            }

            IoCommitAndWait();
        }
    }

    void TouchUp()
    {
        std::lock_guard<std::mutex> lock(MMutex);
        MIoPacket->Op = op_up;
        IoCommitAndWait();
    }

private:
    std::mutex MMutex; // 互斥锁成员变量

    typedef enum _req_op
    {
        op_o = 0, // 空调用
        op_r = 1,
        op_w = 2,
        op_m = 3,

        op_down = 4,
        op_move = 5,
        op_up = 6,
        op_InitTouch = 50, // 初始化触摸
        op_DelTouch = 60,  // 清理触摸触摸

        exit = 100,
        kexit = 200
    } req_op;

    // 将在队列中使用的请求实例结构体
    typedef struct _req_Obj
    {

        std::atomic<int> Kernel; // 由用户模式设置 1 = 内核有待处理的请求, 0 = 请求已完成
        std::atomic<int> User;   // 由内核模式设置 1 = 用户模式有待处理的请求, 0 = 请求已完成

        req_op Op;  // 请求操作类型
        int status; // 操作状态

        // 内存读取
        int TargetProcessId;
        uint64_t TargetAddress;
        int TransferSize;
        char UserBufferAddress[1024];

        // 模块基地址获取
        char ModuleName[46];
        short SegmentIndex; // 模块区段
        uint64_t ModuleBaseAddress;
        uint64_t ModuleSize;

        // 初始化触摸驱动返回屏幕维度
        int POSITION_X, POSITION_Y;
        // 触摸坐标
        int x, y;

    } Requests;

    Requests *MIoPacket;
    pid_t GlobalPid;

    inline void IoCommitAndWait()
    {
        MIoPacket->Kernel.store(1, std::memory_order_release);

        while (MIoPacket->User.load(std::memory_order_acquire) != 1)
        {
        };

        MIoPacket->User.store(0, std::memory_order_relaxed);
    }

    void InitCommunication()
    {
        prctl(PR_SET_NAME, "Lark", 0, 0, 0);

        // 不再使用内核无法钉住页面且MAP_SHARED通常关联到 /dev/shm 或特殊的 inode，或者被标记为具有 IPC（进程间通信）属性
        //MIoPacket = (Requests *)mmap((void *)0x2025827000, sizeof(Requests), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

        MIoPacket = (Requests *)mmap((void *)0x2025827000, sizeof(Requests), PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);

        if (MIoPacket == MAP_FAILED)
        {
            printf("[-] 分配共享内存失败，错误码: %d (%s)\n", errno, strerror(errno));
            return;
        }
        memset(MIoPacket, 0, sizeof(Requests));

        printf("[+] 分配虚拟地址成功，地址: %p  大小: %d\n", MIoPacket, sizeof(Requests));
        printf("当前进程 PID: %d\n", getpid());
        printf("等待驱动握手...\n");

        while (MIoPacket->User.load(std::memory_order_acquire) != 1)
        {
        };
        MIoPacket->User.store(0, std::memory_order_relaxed);

        printf("驱动已经连接\n");
    }
    void ExitCommunication()
    {
        std::lock_guard<std::mutex> lock(MMutex);
        printf("正在通知驱动断开连接...\n");
        MIoPacket->Op = exit;
        IoCommitAndWait();
    }

    void InitTouch()
    {
        std::lock_guard<std::mutex> lock(MMutex); // 加锁
        MIoPacket->Op = op_InitTouch;
        IoCommitAndWait();
    }
    void DelTouch()
    {
        std::lock_guard<std::mutex> lock(MMutex); // 加锁
        MIoPacket->Op = op_DelTouch;
        IoCommitAndWait();
    }

    int KReadProcessMemory(pid_t pid, uint64_t addr, void *buffer, size_t size)
    {
        std::lock_guard<std::mutex> lock(MMutex);

        // 大数据自动分片，防止缓冲区溢出覆盖触摸数据
        if (size > 1024)
        {
            size_t processed = 0;
            while (processed < size)
            {
                size_t chunk = (size - processed > 1024) ? 1024 : (size - processed);
                MIoPacket->Op = op_r;
                MIoPacket->TargetProcessId = pid;
                MIoPacket->TargetAddress = addr + processed;
                MIoPacket->TransferSize = chunk;
                IoCommitAndWait();

                if (MIoPacket->status != 0)
                    return MIoPacket->status;
                memcpy((char *)buffer + processed, MIoPacket->UserBufferAddress, chunk);
                processed += chunk;
            }
            return MIoPacket->status;
        }

        // [原有逻辑] 小数据快速通道
        MIoPacket->Op = op_r;
        MIoPacket->TargetProcessId = pid;
        MIoPacket->TargetAddress = addr;
        MIoPacket->TransferSize = size;

        IoCommitAndWait();

        switch (size)
        {
        case 4:
            *reinterpret_cast<uint32_t *>(buffer) = *reinterpret_cast<uint32_t *>(MIoPacket->UserBufferAddress);
            break;
        case 8:
            *reinterpret_cast<uint64_t *>(buffer) = *reinterpret_cast<uint64_t *>(MIoPacket->UserBufferAddress);
            break;
        case 1:
            *reinterpret_cast<uint8_t *>(buffer) = *reinterpret_cast<uint8_t *>(MIoPacket->UserBufferAddress);
            break;
        case 2:
            *reinterpret_cast<uint16_t *>(buffer) = *reinterpret_cast<uint16_t *>(MIoPacket->UserBufferAddress);
            break;
        default:
            memcpy(buffer, MIoPacket->UserBufferAddress, size);
            break;
        }

        return MIoPacket->status;
    }

    int KWriteProcessMemory(pid_t pid, uint64_t addr, void *buffer, size_t size)
    {
        std::lock_guard<std::mutex> lock(MMutex);

        // 大数据自动分片，防止本地拷贝时溢出覆盖触摸数据
        if (size > 1024)
        {
            size_t processed = 0;
            while (processed < size)
            {
                size_t chunk = (size - processed > 1024) ? 1024 : (size - processed);
                MIoPacket->Op = op_w;
                MIoPacket->TargetProcessId = pid;
                MIoPacket->TargetAddress = addr + processed;
                MIoPacket->TransferSize = chunk;
                memcpy(MIoPacket->UserBufferAddress, (char *)buffer + processed, chunk);
                IoCommitAndWait();

                if (MIoPacket->status != 0)
                    return MIoPacket->status;
                processed += chunk;
            }
            return MIoPacket->status;
        }

        // [原有逻辑] 小数据快速通道
        MIoPacket->Op = op_w;
        MIoPacket->TargetProcessId = pid;
        MIoPacket->TargetAddress = addr;
        MIoPacket->TransferSize = size;

        switch (size)
        {
        case 4:
            *reinterpret_cast<uint32_t *>(MIoPacket->UserBufferAddress) = *reinterpret_cast<uint32_t *>(buffer);
            break;
        case 8:
            *reinterpret_cast<uint64_t *>(MIoPacket->UserBufferAddress) = *reinterpret_cast<uint64_t *>(buffer);
            break;
        case 1:
            *reinterpret_cast<uint8_t *>(MIoPacket->UserBufferAddress) = *reinterpret_cast<uint8_t *>(buffer);
            break;
        case 2:
            *reinterpret_cast<uint16_t *>(MIoPacket->UserBufferAddress) = *reinterpret_cast<uint16_t *>(buffer);
            break;
        default:
            memcpy(MIoPacket->UserBufferAddress, buffer, size);
            break;
        }

        IoCommitAndWait();

        return MIoPacket->status;
    }
};

#endif