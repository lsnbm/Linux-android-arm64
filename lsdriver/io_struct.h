
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

// 寄存器索引枚举 (每个索引占用 2 bits)
enum hwbp_reg_idx
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

// 寄存器操作类型定义
#define HWBP_OP_NONE 0x0  // 00: 不操作
#define HWBP_OP_READ 0x1  // 01: 读
#define HWBP_OP_WRITE 0x2 // 10: 写

// 设置掩码位的宏，参数1:结构体指针，参数2:寄存器索引，参数3:操作类型
#define HWBP_SET_MASK(record, reg, op)                          \
    do                                                          \
    {                                                           \
        int byte_idx = (reg) >> 2;                              \
        int bit_offset = ((reg) & 0x3) << 1;                    \
        (record)->mask[byte_idx] &= ~(0x3 << bit_offset);       \
        (record)->mask[byte_idx] |= ((op) & 0x3) << bit_offset; \
    } while (0)

// 获取掩码位的宏，参数1:结构体指针，参数2:寄存器索引
#define HWBP_GET_MASK(record, reg) \
    (((record)->mask[(reg) >> 2] >> (((reg) & 0x3) << 1)) & 0x3)

// 记录单个 PC（触发指令地址）的命中状态
struct hwbp_record
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
