#ifndef LSDRIVER_HWBP_DEBUG_REG_H
#define LSDRIVER_HWBP_DEBUG_REG_H

// 读写调试寄存器的宏
#ifndef read_sysreg
#define read_sysreg(r) ({                                  \
    u64 __val;                                             \
    asm volatile("mrs %0, " __stringify(r) : "=r"(__val)); \
    __val;                                                 \
})
#endif

#ifndef write_sysreg
#define write_sysreg(v, r)                         \
    do                                             \
    {                                              \
        u64 __val = (u64)(v);                      \
        asm volatile("msr " __stringify(r) ", %x0" \
                     : : "rZ"(__val));             \
    } while (0)
#endif

#ifndef AARCH64_DBG_READ
#define AARCH64_DBG_READ(N, REG, VAL)         \
    do                                        \
    {                                         \
        VAL = read_sysreg(dbg##REG##N##_el1); \
    } while (0)
#endif

#ifndef AARCH64_DBG_WRITE
#define AARCH64_DBG_WRITE(N, REG, VAL)        \
    do                                        \
    {                                         \
        write_sysreg(VAL, dbg##REG##N##_el1); \
    } while (0)
#endif

#define READ_WB_REG_CASE(OFF, N, REG, VAL) \
    case (OFF + N):                        \
        AARCH64_DBG_READ(N, REG, VAL);     \
        break

#define WRITE_WB_REG_CASE(OFF, N, REG, VAL) \
    case (OFF + N):                         \
        AARCH64_DBG_WRITE(N, REG, VAL);     \
        break

#define GEN_READ_WB_REG_CASES(OFF, REG, VAL) \
    READ_WB_REG_CASE(OFF, 0, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 1, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 2, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 3, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 4, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 5, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 6, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 7, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 8, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 9, REG, VAL);      \
    READ_WB_REG_CASE(OFF, 10, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 11, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 12, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 13, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 14, REG, VAL);     \
    READ_WB_REG_CASE(OFF, 15, REG, VAL)

#define GEN_WRITE_WB_REG_CASES(OFF, REG, VAL) \
    WRITE_WB_REG_CASE(OFF, 0, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 1, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 2, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 3, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 4, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 5, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 6, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 7, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 8, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 9, REG, VAL);      \
    WRITE_WB_REG_CASE(OFF, 10, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 11, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 12, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 13, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 14, REG, VAL);     \
    WRITE_WB_REG_CASE(OFF, 15, REG, VAL)

// reg:读哪一类寄存器，n:该类寄存器中的槽位编号 return:对应寄存器中的64位值
static u64 read_wb_reg(int reg, int n)
{
    u64 val = 0;

    switch (reg + n)
    {
        GEN_READ_WB_REG_CASES(AARCH64_DBG_REG_BVR, AARCH64_DBG_REG_NAME_BVR, val);
        GEN_READ_WB_REG_CASES(AARCH64_DBG_REG_BCR, AARCH64_DBG_REG_NAME_BCR, val);
        GEN_READ_WB_REG_CASES(AARCH64_DBG_REG_WVR, AARCH64_DBG_REG_NAME_WVR, val);
        GEN_READ_WB_REG_CASES(AARCH64_DBG_REG_WCR, AARCH64_DBG_REG_NAME_WCR, val);
    default:
        pr_debug("[driver] attempt to read from unknown breakpoint register %d\n", n);
    }

    return val;
}

// reg:写哪一类寄存器，n:该类寄存器中的槽位编号，val:要写入寄存器的 64 位值
static void write_wb_reg(int reg, int n, u64 val)
{
    switch (reg + n)
    {
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_BVR, AARCH64_DBG_REG_NAME_BVR, val);
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_BCR, AARCH64_DBG_REG_NAME_BCR, val);
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_WVR, AARCH64_DBG_REG_NAME_WVR, val);
        GEN_WRITE_WB_REG_CASES(AARCH64_DBG_REG_WCR, AARCH64_DBG_REG_NAME_WCR, val);
    default:
        pr_debug("[driver] attempt to write to unknown breakpoint register %d\n", n);
    }
    isb();
}

// 获取执行给观察寄存器数量
static inline int get_brps_num(void)
{
    u64 dfr0;
    asm volatile("mrs %0, id_aa64dfr0_el1" : "=r"(dfr0));
    return ((dfr0 >> 12) & 0xF) + 1;
}
static inline int get_wrps_num(void)
{
    u64 dfr0;
    asm volatile("mrs %0, id_aa64dfr0_el1" : "=r"(dfr0));
    return ((dfr0 >> 20) & 0xF) + 1;
}

// 解锁操作系统调试锁和全局启用硬件调试功能
static inline void enable_hardware_debug_on_cpu(void *unused)
{
    uint64_t mdscr;

    (void)unused;

    // 解锁 OS Lock，允许访问调试寄存器
    __asm__ volatile(
        "msr oslar_el1, xzr\n\t"
        "isb\n\t" ::: "memory");

    /*
    读取 MDSCR_EL1，置位后写回：
    bit 15 (MDE): Monitor Debug Enable，用户态调试使能(EL0)
    bit 13 (KDE): Kernel Debug Enable，内核态调试使能(EL1)
    */
    __asm__ volatile(
        "mrs %[val], mdscr_el1\n\t"                     // 读取调试寄存器
        "orr %[val], %[val], %[mask]\n\t"               // 置位 MDE 和 KDE 位
        "msr mdscr_el1, %[val]\n\t"                     // 写回
        "isb\n\t"                                       // 指令同步
        : [val] "=&r"(mdscr)                            // 输出变量
        : [mask] "r"((uint64_t)((1 << 15) | (1 << 13))) // 输入掩码
        : "memory");                                    // 编译器屏障
}

// 关闭当前 CPU 上的自托管硬件调试；重新上 OS Lock
static inline void disable_hardware_debug_on_cpu(void *unused)
{
    uint64_t mdscr;

    (void)unused;

    // 清掉 MDSCR_EL1 的 MDE(bit15) 和 KDE(bit13)
    __asm__ volatile(
        "mrs    %[val], mdscr_el1\n\t"
        "bic    %[val], %[val], %[mask]\n\t" // 清 MDE|KDE
        "msr    mdscr_el1, %[val]\n\t"
        "isb\n\t"
        : [val] "=&r"(mdscr)
        : [mask] "r"((uint64_t)((1UL << 15) | (1UL << 13)))
        : "memory");

    // 重新锁住 OS Lock
    __asm__ volatile(
        "mov    x0, #1\n\t"
        "msr    oslar_el1, x0\n\t" // bit0=1 -> lock
        "isb\n\t"
        :
        :
        : "x0", "memory");
}

#endif
