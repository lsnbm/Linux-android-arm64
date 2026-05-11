#ifndef LSDRIVER_ARM64_REG_H
#define LSDRIVER_ARM64_REG_H

#include <linux/types.h>
#include <asm/memory.h>
#include <asm/pgtable.h>
#include <linux/of.h>
#include <linux/string.h>

// 直接从硬件寄存器获取内核页表基地址
static inline pgd_t *get_kernel_pgd_base(void)
{
    // TTBR0_EL1：对应 "低地址段虚拟地址"（如用户进程的虚拟地址，由内核管理）；
    // TTBR1_EL1：对应 "高地址段虚拟地址"（如内核自身的虚拟地址，仅内核可访问）；
    uint64_t ttbr1;

    // 读取 TTBR1_EL1 寄存器 (存放内核页表物理地址)
    asm volatile("mrs %0, ttbr1_el1" : "=r"(ttbr1));

    // TTBR1 包含 ASID 或其他控制位，通常低 48 位是物理地址
    // 这里做一个简单的掩码处理 (64位用48位物理寻址)
    // 将物理地址转为内核虚拟地址
    return (pgd_t *)phys_to_virt(ttbr1 & 0x0000FFFFFFFFF000ULL);
}

// 获取执行给观察寄存器数量
static inline int get_brps_num(void)
{
    uint64_t dfr0;
    asm volatile("mrs %0, id_aa64dfr0_el1" : "=r"(dfr0));
    return ((dfr0 >> 12) & 0xF) + 1;
}
static inline int get_wrps_num(void)
{
    uint64_t dfr0;
    asm volatile("mrs %0, id_aa64dfr0_el1" : "=r"(dfr0));
    return ((dfr0 >> 20) & 0xF) + 1;
}

// 读取 CurrentEL
static inline unsigned int read_current_el(void)
{
    unsigned long val;
    asm volatile(
        "mrs %0, CurrentEL\n\t"
        "lsr %0, %0, #2\n\t"
        "and %0, %0, #0x3"
        : "=r"(val)
        :
        : "cc");
    return (unsigned int)val;
}

// 读取 ID_AA64MMFR1_EL1
static inline unsigned int read_vhe_support(void)
{
    unsigned long val;
    asm volatile(
        "mrs %0, ID_AA64MMFR1_EL1\n\t"
        "lsr %0, %0, #4\n\t"
        "and %0, %0, #0xF"
        : "=r"(val)
        :
        : "cc");
    return (unsigned int)val;
}

// 读取 ID_AA64PFR0_EL
static inline unsigned int read_el2_implemented(void)
{
    unsigned long val;
    asm volatile(
        "mrs %0, ID_AA64PFR0_EL1\n\t"
        "lsr %0, %0, #8\n\t"
        "and %0, %0, #0xF"
        : "=r"(val)
        :
        : "cc");
    return (unsigned int)val;
}

// 读取 SCTLR_EL1
static inline unsigned long read_sctlr_el1(void)
{
    unsigned long val;
    asm volatile(
        "mrs %0, SCTLR_EL1"
        : "=r"(val)
        :
        : "cc");
    return val;
}

// 输出Hypervisor相关信息
static void print_el2_status(void)
{
    unsigned int current_el;
    unsigned int el2_implemented;
    unsigned int vhe_supported;
    unsigned long sctlr_el1;

    struct device_node *np;
    struct property *prop;
    const char *str;
    const char *model;
    bool hyp_hint = false;
    bool tz_hint = false;

    /*
    读取当前 CPU 正在运行的异常级别
    CurrentEL[3:2]：
    01b = EL1
    10b = EL2
    */
    current_el = read_current_el();

    /*
    判断硬件是否实现 EL2。
    ID_AA64PFR0_EL1[11:8]
    0 = 未实现 EL2
    1 = 实现 EL2
    */
    el2_implemented = read_el2_implemented();

    /*
    判断硬件是否支持 VHE(Virtualization Host Extensions)。
    ID_AA64MMFR1_EL1[11:8]
    非 0 表示支持虚拟机主机扩展
    */
    vhe_supported = read_vhe_support();

    /*
    读取 SCTLR_EL1。是 MMU 与 Cache 是否启用的总开关。
    在 VHE 模式下，EL1 系统寄存器访问可能会被重定向到 EL2 侧。
    这里仅打印出来作为辅助判断信息。
    */
    sctlr_el1 = read_sctlr_el1();

    pr_info("===== EL2 Detection =====\n");

    // 打印当前运行级别。
    pr_info("CurrentEL          : EL%u\n", current_el);

    // 打印硬件是否实现 EL2。
    pr_info("EL2 implemented    : %s (ID_AA64PFR0_EL1[11:8] = %u)\n",
            el2_implemented ? "YES" : "NO",
            el2_implemented);

    // 打印硬件是否支持 VHE。注意：硬件支持 VHE，不代表当前系统已经启用 VHE。
    pr_info("VHE supported      : %s (ID_AA64MMFR1_EL1[11:8] = %u)\n",
            vhe_supported ? "YES" : "NO",
            vhe_supported);

    // 打印 SCTLR_EL1 的当前值。该值主要用于辅助观察当前控制寄存器状态。
    pr_info("SCTLR_EL1          : 0x%016lx\n", sctlr_el1);

    // 判断 VHE 是否 active。
    pr_info("VHE mode active    : %s\n",
            current_el == 2 ? "YES" : "NO");

    // 判断当前是否可以直接访问 EL2 寄存器。
    pr_info("EL2 regs accessible: %s\n",
            current_el == 2 ? "YES" : "NO (trap)");

    // 运行在 EL2 时，读取 HCR_EL2。
    if (current_el == 2)
    {
        unsigned long hcr_el2;

        // 读取 HCR_EL2。
        asm volatile(
            "mrs %0, HCR_EL2"
            : "=r"(hcr_el2)
            :
            :);

        pr_info("HCR_EL2            : 0x%016lx\n", hcr_el2);
        pr_info("  E2H bit[34]      : %lu (VHE=%s)\n",
                (hcr_el2 >> 34) & 1,
                ((hcr_el2 >> 34) & 1) ? "enabled" : "disabled");
    }
    else
    {
        pr_info("HCR_EL2            : NOT readable from EL1 (would trap)\n");
    }

    pr_info("===== Hypervisor / TrustZone / Platform Probe =====\n");

    /*
    查看是否有高通 Hypervisor (Gunyah/Haven)
    dmesg | grep -iE "gunyah|haven|qhee|qtee|smmu"

    查看 TrustZone / ATF 痕迹
    dmesg | grep -iE "psci|atf|tfa|arm-tf|trust"

    看设备树里有没有 hypervisor 节点
    cat /proc/device-tree/hypervisor/compatible 2>/dev/null || echo "no hyp node"
    find /proc/device-tree -name "compatible" | xargs grep -il "hyp\|kvm" 2>/dev/null

    看 PSCI 版本（间接判断固件层级）
    dmesg | grep -i psci

    直接看芯片型号，推断用的什么方案
    cat /proc/cpuinfo | grep -i "hardware\|model"
    getprop ro.board.platform
    getprop ro.hardware
    */
    np = of_find_node_by_path("/hypervisor");
    if (np)
    {
        pr_info("DT /hypervisor     : present\n");
        of_property_for_each_string(np, "compatible", prop, str)
        {
            pr_info("  compatible       : %s\n", str);

            if (strnstr(str, "gunyah", strlen(str)) ||
                strnstr(str, "haven", strlen(str)) ||
                strnstr(str, "qhee", strlen(str)) ||
                strnstr(str, "qtee", strlen(str)))
            {
                hyp_hint = true;
            }
        }

        of_node_put(np);
    }
    else
    {
        pr_info("DT /hypervisor     : no hyp node\n");
    }
    np = of_find_node_by_path("/psci");
    if (!np)
        np = of_find_node_by_path("/firmware/psci");
    if (np)
    {
        pr_info("DT PSCI            : present\n");

        of_property_for_each_string(np, "compatible", prop, str)
        {
            pr_info("  compatible       : %s\n", str);

            if (strnstr(str, "psci", strlen(str)) ||
                strnstr(str, "arm,psci", strlen(str)))
            {
                tz_hint = true;
            }
        }

        of_node_put(np);
    }
    else
    {
        pr_info("DT PSCI            : not found\n");
    }

    // 直接看芯片/平台型号
    np = of_find_node_by_path("/");
    if (np)
    {
        if (!of_property_read_string(np, "model", &model))
            pr_info("DT model           : %s\n", model);
        else
            pr_info("DT model           : unavailable\n");

        of_property_for_each_string(np, "compatible", prop, str)
        {
            pr_info("DT compatible      : %s\n", str);

            if (strnstr(str, "qcom", strlen(str)))
                pr_info("  platform hint    : Qualcomm SoC / board\n");

            if (strnstr(str, "gunyah", strlen(str)) ||
                strnstr(str, "haven", strlen(str)) ||
                strnstr(str, "qhee", strlen(str)) ||
                strnstr(str, "qtee", strlen(str)))
            {
                hyp_hint = true;
            }
        }

        of_node_put(np);
    }
    else
    {
        pr_info("DT root            : unavailable\n");
    }

    pr_info("HV keyword hint    : %s\n", hyp_hint ? "YES" : "NO");
    pr_info("TZ/PSCI hint       : %s\n", tz_hint ? "YES" : "NO");

    pr_info("=========================\n");
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
        "mrs %[val], mdscr_el1\n\t"
        "orr %[val], %[val], %[mask]\n\t"
        "msr mdscr_el1, %[val]\n\t"
        "isb\n\t"
        : [val] "=&r"(mdscr)
        : [mask] "r"((uint64_t)((1 << 15) | (1 << 13)))
        : "memory");
}

// 关闭当前 CPU 上的自托管硬件调试；重新上 OS Lock
static inline void disable_hardware_debug_on_cpu(void *unused)
{
    uint64_t mdscr;

    (void)unused;

    // 清掉 MDSCR_EL1 的 MDE(bit15) 和 KDE(bit13)
    __asm__ volatile(
        "mrs    %[val], mdscr_el1\n\t"
        "bic    %[val], %[val], %[mask]\n\t"
        "msr    mdscr_el1, %[val]\n\t"
        "isb\n\t"
        : [val] "=&r"(mdscr)
        : [mask] "r"((uint64_t)((1UL << 15) | (1UL << 13)))
        : "memory");

    // 重新锁住 OS Lock
    __asm__ volatile(
        "mov    x0, #1\n\t"
        "msr    oslar_el1, x0\n\t"
        "isb\n\t"
        :
        :
        : "x0", "memory");
}

// 读写调试寄存器的宏
#ifndef read_sysreg
#define read_sysreg(r) ({                                  \
    uint64_t __val;                                        \
    asm volatile("mrs %0, " __stringify(r) : "=r"(__val)); \
    __val;                                                 \
})
#endif

#ifndef write_sysreg
#define write_sysreg(v, r)                         \
    do                                             \
    {                                              \
        uint64_t __val = (uint64_t)(v);            \
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
static uint64_t read_wb_reg(int reg, int n)
{
    uint64_t val = 0;

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
static void write_wb_reg(int reg, int n, uint64_t val)
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

// ========== FP/SIMD 寄存器操作 ==========

// Q寄存器名称拼接辅助宏：QREG(0) → q0, QREG(1) → q1, ...
#define QREG(n) q##n

#define READ_Q_REG_CASE(N, DST)                                                     \
    case N:                                                                         \
        asm volatile(".arch_extension fp\n.arch_extension simd\n"                   \
                     "str " __stringify(QREG(N)) ", [%0]\n" ::"r"(DST) : "memory"); \
        break

#define WRITE_Q_REG_CASE(N, SRC)                                                    \
    case N:                                                                         \
        asm volatile(".arch_extension fp\n.arch_extension simd\n"                   \
                     "ldr " __stringify(QREG(N)) ", [%0]\n" ::"r"(SRC) : "memory"); \
        break

#define GEN_READ_Q_REG_CASES(DST) \
    READ_Q_REG_CASE(0, DST);      \
    READ_Q_REG_CASE(1, DST);      \
    READ_Q_REG_CASE(2, DST);      \
    READ_Q_REG_CASE(3, DST);      \
    READ_Q_REG_CASE(4, DST);      \
    READ_Q_REG_CASE(5, DST);      \
    READ_Q_REG_CASE(6, DST);      \
    READ_Q_REG_CASE(7, DST);      \
    READ_Q_REG_CASE(8, DST);      \
    READ_Q_REG_CASE(9, DST);      \
    READ_Q_REG_CASE(10, DST);     \
    READ_Q_REG_CASE(11, DST);     \
    READ_Q_REG_CASE(12, DST);     \
    READ_Q_REG_CASE(13, DST);     \
    READ_Q_REG_CASE(14, DST);     \
    READ_Q_REG_CASE(15, DST);     \
    READ_Q_REG_CASE(16, DST);     \
    READ_Q_REG_CASE(17, DST);     \
    READ_Q_REG_CASE(18, DST);     \
    READ_Q_REG_CASE(19, DST);     \
    READ_Q_REG_CASE(20, DST);     \
    READ_Q_REG_CASE(21, DST);     \
    READ_Q_REG_CASE(22, DST);     \
    READ_Q_REG_CASE(23, DST);     \
    READ_Q_REG_CASE(24, DST);     \
    READ_Q_REG_CASE(25, DST);     \
    READ_Q_REG_CASE(26, DST);     \
    READ_Q_REG_CASE(27, DST);     \
    READ_Q_REG_CASE(28, DST);     \
    READ_Q_REG_CASE(29, DST);     \
    READ_Q_REG_CASE(30, DST);     \
    READ_Q_REG_CASE(31, DST)

#define GEN_WRITE_Q_REG_CASES(SRC) \
    WRITE_Q_REG_CASE(0, SRC);      \
    WRITE_Q_REG_CASE(1, SRC);      \
    WRITE_Q_REG_CASE(2, SRC);      \
    WRITE_Q_REG_CASE(3, SRC);      \
    WRITE_Q_REG_CASE(4, SRC);      \
    WRITE_Q_REG_CASE(5, SRC);      \
    WRITE_Q_REG_CASE(6, SRC);      \
    WRITE_Q_REG_CASE(7, SRC);      \
    WRITE_Q_REG_CASE(8, SRC);      \
    WRITE_Q_REG_CASE(9, SRC);      \
    WRITE_Q_REG_CASE(10, SRC);     \
    WRITE_Q_REG_CASE(11, SRC);     \
    WRITE_Q_REG_CASE(12, SRC);     \
    WRITE_Q_REG_CASE(13, SRC);     \
    WRITE_Q_REG_CASE(14, SRC);     \
    WRITE_Q_REG_CASE(15, SRC);     \
    WRITE_Q_REG_CASE(16, SRC);     \
    WRITE_Q_REG_CASE(17, SRC);     \
    WRITE_Q_REG_CASE(18, SRC);     \
    WRITE_Q_REG_CASE(19, SRC);     \
    WRITE_Q_REG_CASE(20, SRC);     \
    WRITE_Q_REG_CASE(21, SRC);     \
    WRITE_Q_REG_CASE(22, SRC);     \
    WRITE_Q_REG_CASE(23, SRC);     \
    WRITE_Q_REG_CASE(24, SRC);     \
    WRITE_Q_REG_CASE(25, SRC);     \
    WRITE_Q_REG_CASE(26, SRC);     \
    WRITE_Q_REG_CASE(27, SRC);     \
    WRITE_Q_REG_CASE(28, SRC);     \
    WRITE_Q_REG_CASE(29, SRC);     \
    WRITE_Q_REG_CASE(30, SRC);     \
    WRITE_Q_REG_CASE(31, SRC)

// n: Q寄存器编号 0~31, dst: 指向 16 字节缓冲区的指针
static inline void read_q_reg(int n, void *dst)
{
    switch (n)
    {
        GEN_READ_Q_REG_CASES(dst);
    default:
        break;
    }
}

// n: Q寄存器编号 0~31, src: 指向 16 字节数据的指针
static inline void write_q_reg(int n, void *src)
{
    switch (n)
    {
        GEN_WRITE_Q_REG_CASES(src);
    default:
        break;
    }
}

// 读取 FPCR (浮点控制寄存器)
static inline uint32_t read_fpcr(void)
{
    uint64_t v;
    asm volatile(".arch_extension fp\n"
                 "mrs %0, fpcr"
                 : "=r"(v));
    return (uint32_t)v;
}

// 写入 FPCR (浮点控制寄存器)
static inline void write_fpcr(uint32_t val)
{
    uint64_t v = val;
    asm volatile(".arch_extension fp\n"
                 "msr fpcr, %0"
                 :
                 : "r"(v));
}

// 读取 FPSR (浮点状态寄存器)
static inline uint32_t read_fpsr(void)
{
    uint64_t v;
    asm volatile(".arch_extension fp\n"
                 "mrs %0, fpsr"
                 : "=r"(v));
    return (uint32_t)v;
}

// 写入 FPSR (浮点状态寄存器)
static inline void write_fpsr(uint32_t val)
{
    uint64_t v = val;
    asm volatile(".arch_extension fp\n"
                 "msr fpsr, %0"
                 :
                 : "r"(v));
}

#endif
