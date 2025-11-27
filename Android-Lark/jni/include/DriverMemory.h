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

class Driver
{
public:
    Driver()
    {
        initCommunication();
    }

    ~Driver()
    {
        ExitCommunication();
    }

    void ExitKernelThread()
    {
        std::lock_guard<std::mutex> lock(m_mutex); 
        m_ioPacket->Op = kexit;
        _io_commit_and_wait();
    }

    void NULLIO()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_ioPacket->Op = op_o;
        _io_commit_and_wait();
    }

    int getPID(char *PackageName)
    {
        int pid;
        FILE *fp;
        char cmd[0x100] = "pidof ";
        strcat(cmd, PackageName);
        fp = popen(cmd, "r");
        fscanf(fp, "%d", &pid);
        pclose(fp);

        return pid;
    }

    void SetQJPid(pid_t pid)
    {
        // 修改全局PID建议也加锁，防止读写期间PID被突然改变
        std::lock_guard<std::mutex> lock(m_mutex);
        QJPID = pid;
    }

    // 内核接口
    template <typename T>
    T Read(unsigned long long address)
    {
        T value = {};
        KReadProcessMemory(QJPID, address, &value, sizeof(T));
        return value;
    }

    bool Read(unsigned long long address, void *buffer, size_t size)
    {
        return KReadProcessMemory(QJPID, address, buffer, size);
    }

    template <typename T>
    bool Write(unsigned long long address, const T &value)
    {
        return KWriteProcessMemory(QJPID, address, const_cast<T *>(&value), sizeof(T));
    }

    bool Write(unsigned long long address, void *buffer, size_t size)
    {
        return KWriteProcessMemory(QJPID, address, buffer, size);
    }

    int GetModuleBase(const char MaduleName[46], short SegmentIndex, unsigned long long *ModuleAddr)
    {
        std::lock_guard<std::mutex> lock(m_mutex); // 加锁

        m_ioPacket->Op = op_m;
        m_ioPacket->TargetProcessId = QJPID;
        snprintf(m_ioPacket->ModuleName, 46, "%s", MaduleName);
        m_ioPacket->SegmentIndex = SegmentIndex;

        _io_commit_and_wait();

        *ModuleAddr = m_ioPacket->ModuleBaseAddress;
        return m_ioPacket->status == 0;
    }

    void v_touch_down(int x, int y)
    {
        std::lock_guard<std::mutex> lock(m_mutex); // 加锁
        m_ioPacket->Op = op_down;
        m_ioPacket->x = x;
        m_ioPacket->y = y;
        _io_commit_and_wait();
    }

    void v_touch_move(int x, int y)
    {
        std::lock_guard<std::mutex> lock(m_mutex); // 加锁
        m_ioPacket->Op = op_move;
        m_ioPacket->x = x;
        m_ioPacket->y = y;
        _io_commit_and_wait();
    }

    void v_touch_up()
    {
        std::lock_guard<std::mutex> lock(m_mutex); // 加锁
        m_ioPacket->Op = op_up;
        _io_commit_and_wait();
    }

private:
    std::mutex m_mutex; // 互斥锁成员变量

    typedef enum _req_op
    {
        op_o = 0,
        op_r = 1,
        op_w = 2,
        op_m = 3,
        op_down = 4,
        op_move = 5,
        op_up = 6,
        exit = 100,
        kexit = 200
    } req_op;

    typedef struct _req_Obj
    {
        std::atomic<int> kernel;
        std::atomic<int> user;
        req_op Op;
        int status;
        int TargetProcessId;
        unsigned long long TargetAddress;
        int TransferSize;
        char UserBufferAddress[1024];
        char ModuleName[46];
        short SegmentIndex;
        unsigned long long ModuleBaseAddress;
        unsigned long long ModuleSize;
        int x, y;
    } Requests;

    Requests *m_ioPacket;
    pid_t QJPID;

    inline void _io_commit_and_wait()
    {
        m_ioPacket->kernel.store(1, std::memory_order_release);

        while (m_ioPacket->user.load(std::memory_order_acquire) != 1)
        {
        };

        m_ioPacket->user.store(0, std::memory_order_relaxed);
    }

    void initCommunication()
    {
        prctl(PR_SET_NAME, "Lark", 0, 0, 0);

        m_ioPacket = (Requests *)mmap((void *)0x2025827000, sizeof(Requests), PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
        if (m_ioPacket == MAP_FAILED)
        {
            printf("[-] 分配共享内存失败，错误码: %d (%s)\n", errno, strerror(errno));
            return;
        }
        memset(m_ioPacket, 0, sizeof(Requests));

        printf("[+] 分配虚拟地址成功，地址: %p\n", m_ioPacket);
        printf("当前进程 PID: %d\n", getpid());
        printf("等待驱动握手...\n");

        while (m_ioPacket->user.load(std::memory_order_acquire) != 1)
        {
        };
        m_ioPacket->user.store(0, std::memory_order_relaxed);

        printf("驱动已经连接\n");
    }

    void ExitCommunication()
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        printf("正在通知驱动断开连接...\n");
        m_ioPacket->Op = exit;
        _io_commit_and_wait();
    }

    int KReadProcessMemory(pid_t pid, unsigned long long addr, void *buffer, size_t size)
    {
        // 保护整个“设置请求->等待->读取数据”的过程
        std::lock_guard<std::mutex> lock(m_mutex);

        m_ioPacket->Op = op_r;
        m_ioPacket->TargetProcessId = pid;
        m_ioPacket->TargetAddress = addr;
        m_ioPacket->TransferSize = size;

        _io_commit_and_wait();

        switch (size)
        {
        case 4:
            *reinterpret_cast<uint32_t *>(buffer) = *reinterpret_cast<uint32_t *>(m_ioPacket->UserBufferAddress);
            break;
        case 8:
            *reinterpret_cast<uint64_t *>(buffer) = *reinterpret_cast<uint64_t *>(m_ioPacket->UserBufferAddress);
            break;
        case 1:
            *reinterpret_cast<uint8_t *>(buffer) = *reinterpret_cast<uint8_t *>(m_ioPacket->UserBufferAddress);
            break;
        case 2:
            *reinterpret_cast<uint16_t *>(buffer) = *reinterpret_cast<uint16_t *>(m_ioPacket->UserBufferAddress);
            break;
        default:
            memcpy(buffer, m_ioPacket->UserBufferAddress, size);
            break;
        }

        return m_ioPacket->status == 0;
    }

    int KWriteProcessMemory(pid_t pid, unsigned long long addr, void *buffer, size_t size)
    {
        //  加锁
        std::lock_guard<std::mutex> lock(m_mutex);

        m_ioPacket->Op = op_w;
        m_ioPacket->TargetProcessId = pid;
        m_ioPacket->TargetAddress = addr;
        m_ioPacket->TransferSize = size;

        switch (size)
        {
        case 4:
            *reinterpret_cast<uint32_t *>(m_ioPacket->UserBufferAddress) = *reinterpret_cast<uint32_t *>(buffer);
            break;
        case 8:
            *reinterpret_cast<uint64_t *>(m_ioPacket->UserBufferAddress) = *reinterpret_cast<uint64_t *>(buffer);
            break;
        case 1:
            *reinterpret_cast<uint8_t *>(m_ioPacket->UserBufferAddress) = *reinterpret_cast<uint8_t *>(buffer);
            break;
        case 2:
            *reinterpret_cast<uint16_t *>(m_ioPacket->UserBufferAddress) = *reinterpret_cast<uint16_t *>(buffer);
            break;
        default:
            memcpy(m_ioPacket->UserBufferAddress, buffer, size);
            break;
        }

        _io_commit_and_wait();

        return m_ioPacket->status == 0;
    } // 离开作用域，自动解锁
};

#endif