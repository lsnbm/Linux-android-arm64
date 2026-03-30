
#ifndef IO_STRUCT_H
#define IO_STRUCT_H
#include <linux/vmalloc.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <asm/pgtable.h>
#include <asm/pgtable-prot.h>
#include <asm/memory.h>
#include <asm/barrier.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/signal.h>
#include <linux/pid.h>
#include <linux/sort.h>

// 断点类型
enum bp_type
{
    BP_READ,       // 读
    BP_WRITE,      // 写
    BP_READ_WRITE, // 读写
    BP_EXECUTE     // 执行
} __attribute__((packed));

// 断点作用线程范围
enum bp_scope
{
    SCOPE_MAIN_THREAD,   // 仅主线程
    SCOPE_OTHER_THREADS, // 仅其他子线程
    SCOPE_ALL_THREADS    // 全部线程
} __attribute__((packed));

// 记录单个 PC（触发指令地址）的命中状态
struct hwbp_record
{
    bool rw; // 是读取，还是写入

    // 通用寄存器
    uint64_t pc;        // 触发断点的汇编指令地址
    uint64_t hit_count; // 该 PC 命中的次数
    uint64_t regs[30];  // 最新的 X0 ~ X29 寄存器
    uint64_t lr;        // X30
    uint64_t sp;        // Stack Pointer
    uint64_t orig_x0;   // 原始 X0
    uint64_t syscallno; // 系统调用号
    uint64_t pstate;    // 处理器状态

    // 浮点/SIMD 寄存器
    __uint128_t vregs[32]; // V0 ~ V31 (每个128位)
    uint32_t fpsr;         // 浮点状态寄存器
    uint32_t fpcr;         // 浮点控制寄存器

} __attribute__((packed));

// 存储整体命中信息
struct hwbp_info
{
    uint64_t num_brps;                 // 执行断点的数量
    uint64_t num_wrps;                 // 访问断点的数量
    uint64_t hit_addr;                 // 监控的地址
    int record_count;                  // 当前已记录的不同 PC 数量
    struct hwbp_record records[0x100]; // 记录不同 PC 触发状态的数组
} __attribute__((packed));

#define MAX_MODULES 512
#define MAX_SCAN_REGIONS 4096

#define MOD_NAME_LEN 256
#define MAX_SEGS_PER_MODULE 256

struct segment_info
{
    short index;  // >=0: 普通段(RX→RO→RW连续编号), -1: BSS段
    uint8_t prot; // 区段权限: 1(R), 2(W), 4(X)。例如 RX 就是 5 (1+4)
    uint64_t start;
    uint64_t end;
} __attribute__((packed));

struct module_info
{
    char name[MOD_NAME_LEN];
    int seg_count;
    struct segment_info segs[MAX_SEGS_PER_MODULE];
} __attribute__((packed));

struct region_info
{
    uint64_t start;
    uint64_t end;
} __attribute__((packed));

struct memory_info
{
    int module_count;                        // 总模块数量
    struct module_info modules[MAX_MODULES]; // 模块信息

    int region_count;                             // 总可扫描内存数量
    struct region_info regions[MAX_SCAN_REGIONS]; // 可扫描内存区域 (rw-p, 排除特殊区域)
} __attribute__((packed));

enum sm_req_op
{
    op_o, // 空调用
    op_r,
    op_w,
    op_m, // 获取进程内存信息

    op_down,
    op_move,
    op_up,
    op_init_touch, // 初始化触摸

    op_brps_weps_info,      // 获取执行断点数量和访问断点数量
    op_set_process_hwbp,    // 设置硬件断点
    op_remove_process_hwbp, // 删除硬件断点

    op_kexit // 内核线程退出
} __attribute__((packed));

// 将在队列中使用的请求实例结构体
struct req_obj
{
    atomic_t kernel; // 由用户模式设置 1 = 内核有待处理的请求, 0 = 请求已完成
    atomic_t user;   // 由内核模式设置 1 = 用户模式有待处理的请求, 0 = 请求已完成

    enum sm_req_op op; // shared memory请求操作类型
    int status;        // 操作状态

    // 内存读取
    int pid;
    uint64_t target_addr;
    int size;
    uint8_t user_buffer[0x1000]; // 物理标准页大小

    // 进程内存信息
    struct memory_info mem_info;

    enum bp_type bt;          // 断点类型
    enum bp_scope bs;         // 断点作用线程范围
    int len_bytes;            // 断点长度字节
    struct hwbp_info bp_info; // 断点信息

    // 初始化触摸驱动返回屏幕维度
    int POSITION_X, POSITION_Y;
    // 触摸坐标
    int x, y;
} __attribute__((packed));

#endif // IO_STRUCT_H
