
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
#include <linux/types.h>
#include <asm/ptrace.h>
#include "arm64_reg.h"

#define TLS_THREAD_NAME_LEN 16

struct env_params
{
    char thread_name[TLS_THREAD_NAME_LEN];
    uint64_t tpidr_el0;
    uint64_t pacga_lo;
    uint64_t pacga_hi;
    int tls_status;
    int pacga_status;
};

// 寄存器操作类型定义
#define BP_OP_NONE    0x0 // 00: 不操作
#define BP_OP_READ    0x1 // 01: 读
#define BP_OP_WRITE   0x2 // 10: 写
#define BP_CONFIG_MAX 16

// 设置掩码位的宏，参数1:结构体指针，参数2:寄存器索引，参数3:操作类型
#define BP_SET_MASK(record, reg, op)                            \
    do                                                          \
    {                                                           \
        int byte_idx = (reg) >> 2;                              \
        int bit_offset = ((reg) & 0x3) << 1;                    \
        (record)->mask[byte_idx] &= ~(0x3 << bit_offset);       \
        (record)->mask[byte_idx] |= ((op) & 0x3) << bit_offset; \
    } while (0)

// 获取掩码位的宏，参数1:结构体指针，参数2:寄存器索引
#define BP_GET_MASK(record, reg) (((record)->mask[(reg) >> 2] >> (((reg) & 0x3) << 1)) & 0x3)

// 断点类型
enum bp_type
{
    BP_BREAKPOINT_EMPTY = 0,
    BP_BREAKPOINT_R = 1,
    BP_BREAKPOINT_W = 2,
    BP_BREAKPOINT_RW = BP_BREAKPOINT_R | BP_BREAKPOINT_W,
    BP_BREAKPOINT_X = 4,
    BP_BREAKPOINT_INVALID = BP_BREAKPOINT_RW | BP_BREAKPOINT_X,
};
// 断点长度
enum bp_len
{
    BP_BREAKPOINT_LEN_1 = 1,
    BP_BREAKPOINT_LEN_2 = 2,
    BP_BREAKPOINT_LEN_3 = 3,
    BP_BREAKPOINT_LEN_4 = 4,
    BP_BREAKPOINT_LEN_5 = 5,
    BP_BREAKPOINT_LEN_6 = 6,
    BP_BREAKPOINT_LEN_7 = 7,
    BP_BREAKPOINT_LEN_8 = 8,

};
// 断点作用线程范围
enum bp_scope
{
    BP_SCOPE_MAIN_THREAD,   // 仅主线程
    BP_SCOPE_OTHER_THREADS, // 仅其他子线程
    BP_SCOPE_ALL_THREADS    // 全部线程
};

// 寄存器索引枚举 (每个索引占用 2 bits)
enum bp_reg_idx
{
    IDX_PC = 0,
    IDX_HIT_COUNT,
    IDX_LR,
    IDX_SP,
    IDX_ORIG_X0,
    IDX_SYSCALLNO,
    IDX_PSTATE,
    IDX_X0,
    IDX_X1,
    IDX_X2,
    IDX_X3,
    IDX_X4,
    IDX_X5,
    IDX_X6,
    IDX_X7,
    IDX_X8,
    IDX_X9,
    IDX_X10,
    IDX_X11,
    IDX_X12,
    IDX_X13,
    IDX_X14,
    IDX_X15,
    IDX_X16,
    IDX_X17,
    IDX_X18,
    IDX_X19,
    IDX_X20,
    IDX_X21,
    IDX_X22,
    IDX_X23,
    IDX_X24,
    IDX_X25,
    IDX_X26,
    IDX_X27,
    IDX_X28,
    IDX_X29,
    IDX_FPSR,
    IDX_FPCR,
    IDX_Q0,
    IDX_Q1,
    IDX_Q2,
    IDX_Q3,
    IDX_Q4,
    IDX_Q5,
    IDX_Q6,
    IDX_Q7,
    IDX_Q8,
    IDX_Q9,
    IDX_Q10,
    IDX_Q11,
    IDX_Q12,
    IDX_Q13,
    IDX_Q14,
    IDX_Q15,
    IDX_Q16,
    IDX_Q17,
    IDX_Q18,
    IDX_Q19,
    IDX_Q20,
    IDX_Q21,
    IDX_Q22,
    IDX_Q23,
    IDX_Q24,
    IDX_Q25,
    IDX_Q26,
    IDX_Q27,
    IDX_Q28,
    IDX_Q29,
    IDX_Q30,
    IDX_Q31,
    MAX_REG_COUNT
};

// 记录单个 PC（触发指令地址）的命中状态
struct bp_record
{
    /*
    一个掩码位，控制所有寄存器的读写行为
    为了方便掩码位控制对应寄存器，不使用数组存储寄存器了， 方便了：阅读，理解，写代码时不再做 regs[i] / vregs[i] 的索引换算
    */
    uint8_t mask[18];

    // 通用寄存器
    uint64_t hit_count; // 该 PC 命中的次数
    uint64_t pc;        // 触发断点的汇编指令地址
    uint64_t lr;        // X30
    uint64_t sp;        // Stack Pointer
    uint64_t orig_x0;   // 原始 X0
    uint64_t syscallno; // 系统调用号
    uint64_t pstate;    // 处理器状态
    uint64_t x0, x1, x2, x3, x4, x5, x6, x7, x8, x9;
    uint64_t x10, x11, x12, x13, x14, x15, x16, x17, x18, x19;
    uint64_t x20, x21, x22, x23, x24, x25, x26, x27, x28, x29;

    // 浮点/SIMD 寄存器
    uint32_t fpsr; // 浮点状态寄存器
    uint32_t fpcr; // 浮点控制寄存器
    __uint128_t q0, q1, q2, q3, q4, q5, q6, q7, q8, q9;
    __uint128_t q10, q11, q12, q13, q14, q15, q16, q17, q18, q19;
    __uint128_t q20, q21, q22, q23, q24, q25, q26, q27, q28, q29;
    __uint128_t q30, q31;
};

// 单个观点地址结构
struct bp_point
{
    void (*on_hit)(void *regs, void *self); // 触发回调，命中时调用
    enum bp_type bt;                        // 断点类型
    enum bp_len bl;                         // 断点长度
    enum bp_scope bs;                       // 断点作用线程范围
    uint64_t hit_addr;                      // 监控的地址
    int record_count;                       // 当前已记录的不同 PC 数量
    struct bp_record records[0x100];        // 记录不同 PC 触发状态的数组
};

// 存储整体命中信息
struct break_point
{

    uint64_t num_brps;                     // 执行断点的数量
    uint64_t num_wrps;                     // 访问断点的数量
    int pid;                               // 这个 break_point 属于哪个进程
    struct bp_point points[BP_CONFIG_MAX]; // 多个观点地址
};

struct virtual_gnss
{
    int latitude_e7;
    int longitude_e7;
};

struct virtual_gyro
{
    int gyro_x_mrad_s;
    int gyro_y_mrad_s;
    int gyro_z_mrad_s;
};

struct virtual_input
{
    int request_virtual_slots;  // 初始化时请求的虚拟 slot 数量
    int POSITION_X, POSITION_Y; // 初始化触摸时返回的触摸面板 ABS 最大值
    int slot;                   // 触摸槽位
    int x, y;                   // 触摸坐标
};

#define MAX_MODULES      1024
#define MAX_SCAN_REGIONS 16534

#define MOD_NAME_LEN        256
#define MAX_SEGS_PER_MODULE 512

struct segment_info
{
    short index;  // >=0: 普通段(RX→RO→RW连续编号), -1: BSS段
    uint8_t prot; // 区段权限: 1(R), 2(W), 4(X)。例如 RX 就是 5 (1+4)
    uint64_t start;
    uint64_t end;
};

struct module_info
{
    char name[MOD_NAME_LEN];
    int seg_count;
    struct segment_info segs[MAX_SEGS_PER_MODULE];
};

struct region_info
{
    uint64_t start;
    uint64_t end;
};

struct virtual_memory
{
    int module_count;                        // 总模块数量
    struct module_info modules[MAX_MODULES]; // 模块信息

    int region_count;                             // 总可扫描内存数量
    struct region_info regions[MAX_SCAN_REGIONS]; // 可扫描内存区域 (rw-p, 排除特殊区域)
};

struct virtual_memoryrw
{
    uint64_t rw_addr;            // 读写的地址
    uint8_t user_buffer[0x1000]; // 物理标准页大小的数据缓存区
    int size;                    // 读写的大小
};

enum request_op
{
    request_op_none,       // 空调用
    request_op_vmem_read,  // 读取内存
    request_op_vmem_write, // 写入内存
    request_op_vmem_info,  // 获取进程内存信息

    request_op_touch_init, // 初始化触摸
    request_op_touch_down, // 上报按下
    request_op_touch_move, // 上报移动
    request_op_touch_up,   // 上报抬起

    request_op_gyro_init,   // 初始化陀螺仪
    request_op_gyro_report, // 上报陀螺仪数据

    request_op_gnss_init,   // 初始化虚拟定位
    request_op_gnss_report, // 上报虚拟定位数据

    request_op_hwbp_set,    // 设置硬件断点并获取执行/访问断点数量
    request_op_hwbp_remove, // 删除硬件断点

    request_op_ptebp_set,    // 设置 PTE UXN breakpoint
    request_op_ptebp_remove, // 删除 PTE UXN breakpoint

    request_op_stepbp_set,    // 设置单步 PC breakpoint
    request_op_stepbp_remove, // 删除单步 PC breakpoint

    request_op_syscall_monitor_set,    // 监控指定进程的系统调用
    request_op_syscall_monitor_remove, // 取消指定进程的系统调用监控

    request_op_env_get_params, // 获取指定task环境参数

    request_op_kernel_exit // 内核线程退出
};

// 将在队列中使用的请求实例结构体
struct request_obj
{
    bool kernel; // 由用户模式设置 true = 内核有待处理的请求, false = 请求已完成
    bool user;   // 由内核模式设置 true = 用户模式有待处理的请求, false = 请求已完成

    int pid; // 当前派发指定的pid

    enum request_op op; // 请求操作类型
    int status;         // 请求操作状态

    // 虚拟内存读写信息
    struct virtual_memoryrw vmemrw_info;
    // 虚拟内存信息
    struct virtual_memory vmem_info;
    // 虚拟触摸信息
    struct virtual_input vinput_info;
    // 虚拟陀螺仪信息
    struct virtual_gyro vgyro_info;
    // 虚拟定位信息
    struct virtual_gnss vgnss_info;
    // 断点信息
    struct break_point bp_info;
    // 环境参数信息
    struct env_params env_info;
};

#endif // IO_STRUCT_H
