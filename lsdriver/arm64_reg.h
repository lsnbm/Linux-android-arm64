#ifndef LSDRIVER_ARM64_REG_H
#define LSDRIVER_ARM64_REG_H

#include <linux/kconfig.h>
#include <linux/bits.h>
#include <linux/types.h>
#include <asm/cpufeature.h>
#include <asm/debug-monitors.h>
#include <asm/memory.h>
#include <asm/pgtable.h>
#include <asm/sysreg.h>
#include <linux/arm-smccc.h>
#include <linux/of.h>
#include <linux/string.h>

#include "klog/lsdriver_log.h"

// Linux 6.18 使用 MDSCR_EL1_* 替代旧 DBG_MDSCR_* 名称，寄存器位定义不变。
// 仅在旧宏缺失时映射到新名称，旧内核继续使用自身定义。
// MDE(bit15): 调试监控使能；KDE(bit13): 内核态调试使能；SS(bit0): 软件单步使能。
#ifndef DBG_MDSCR_MDE
#define DBG_MDSCR_MDE MDSCR_EL1_MDE
#endif
#ifndef DBG_MDSCR_KDE
#define DBG_MDSCR_KDE MDSCR_EL1_KDE
#endif
#ifndef DBG_MDSCR_SS
#define DBG_MDSCR_SS MDSCR_EL1_SS
#endif

// 若内核头文件已定义 phys_to_ttbr 宏，先取消它，避免下方同名函数定义被宏展开。
// 此后使用本文件的 PA52 编码函数
#ifdef phys_to_ttbr
#undef phys_to_ttbr
#endif

// 无条件按 PA52 布局编码 TTBR.BADDR；PA[51:48] 为 0 时自然退化为 PA48 布局。
static inline uint64_t phys_to_ttbr(phys_addr_t phys)
{
    return (phys & GENMASK_ULL(47, 0)) | ((phys & GENMASK_ULL(51, 48)) >> 46);
}

// 无条件按 PA52 布局解码 TTBR.BADDR；TTBR[5:2] 为 0 时自然退化为 PA48 布局。
static inline phys_addr_t ttbr_to_phys(uint64_t ttbr)
{
    // GENMASK_ULL(47, PAGE_SHIFT) 生成仅 [47:PAGE_SHIFT] 为 1 的掩码；
    // 按位与后只保留 TTBR 中的低 48 位页表基址，清除 ASID、CnP 和对齐低位。
    phys_addr_t phys = ttbr & GENMASK_ULL(47, PAGE_SHIFT);

    // 取出 TTBR[5:2] 中编码的 PA[51:48]，左移恢复后拼回物理地址。
    phys |= (ttbr & GENMASK_ULL(5, 2)) << 46;

    return phys;
}

// 直接从硬件寄存器获取内核页表基地址。
static inline pgd_t *get_kernel_pgd_base(void)
{
    uint64_t ttbr1 = read_sysreg(ttbr1_el1);

    // ttbr_to_phys() 会丢弃 PAGE_SHIFT 以下的页内位，因此 VA52 回退模式下
    // TTBR1 指向 PGD 子区域的兼容偏移也会自然消失，最终始终得到完整 PGD 基址。
    return (pgd_t *)phys_to_virt(ttbr_to_phys(ttbr1));
}

// 获取硬件执行断点寄存器数量。
static inline int get_brps_num(void)
{
    uint64_t dfr0 = read_sysreg(id_aa64dfr0_el1);

#ifdef ID_AA64DFR0_EL1_BRPs_SHIFT
    return cpuid_feature_extract_unsigned_field(dfr0, ID_AA64DFR0_EL1_BRPs_SHIFT) + 1;
#else
    return cpuid_feature_extract_unsigned_field(dfr0, ID_AA64DFR0_BRPS_SHIFT) + 1;
#endif
}

// 获取硬件观察点寄存器数量。
static inline int get_wrps_num(void)
{
    uint64_t dfr0 = read_sysreg(id_aa64dfr0_el1);

#ifdef ID_AA64DFR0_EL1_WRPs_SHIFT
    return cpuid_feature_extract_unsigned_field(dfr0, ID_AA64DFR0_EL1_WRPs_SHIFT) + 1;
#else
    return cpuid_feature_extract_unsigned_field(dfr0, ID_AA64DFR0_WRPS_SHIFT) + 1;
#endif
}

// 解锁操作系统调试锁和全局启用硬件调试功能
static inline void enable_hardware_debug_on_cpu(void *unused)
{
    (void)unused;

    // 解锁当前 CPU 的自托管调试寄存器。允许访问调试寄存器
    write_sysreg(0, osdlr_el1);
    write_sysreg(0, oslar_el1);
    isb();

    /*
    读取 MDSCR_EL1，置位后写回：
    DBG_MDSCR_MDE = 1 << 15：Monitor Debug Enable，使能监控式硬件断点和观察点调试。
    DBG_MDSCR_KDE = 1 << 13：Kernel Debug Enable，配合 MDE 允许 EL1 的此类调试。
    MDE 并非仅作用于 EL0；实际触发还取决于断点/观察点寄存器配置及异常屏蔽等条件。
    下方 clear 参数为 0，仅置位 MDE/KDE，保留寄存器其他位。
    */
    sysreg_clear_set(mdscr_el1, 0, (uint64_t)(DBG_MDSCR_MDE | DBG_MDSCR_KDE));
    isb();
}

// 关闭当前 CPU 上的自托管硬件调试；重新上 OS Lock
static inline void disable_hardware_debug_on_cpu(void *unused)
{
    (void)unused;

    // 清掉 MDSCR_EL1 的 MDE(bit15) 和 KDE(bit13)
    sysreg_clear_set(mdscr_el1, (uint64_t)(DBG_MDSCR_MDE | DBG_MDSCR_KDE), 0);
    isb();

    // 重新锁住 OS Lock
    write_sysreg(1, oslar_el1);
    isb();
}

// 从 CTR_EL0.DminLine 读取当前 CPU 的最小数据缓存行大小并返回字节数。
static inline unsigned long arm64_dcache_line_size(void)
{
    return 4UL << ((read_sysreg(ctr_el0) >> 16) & 0xf);
}

// 读写调试寄存器
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
        ls_log_always_tag("driver", "attempt to read from unknown breakpoint register %d\n", n);
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
        ls_log_always_tag("driver", "attempt to write to unknown breakpoint register %d\n", n);
    }
    isb();
}

// ========== FP/SIMD 寄存器操作 ==========

#define ARM64_FP_Q_REG_COUNT 32

// Q0-Q31 统一按 128 位宽度保存；FPCR/FPSR 与向量寄存器共同组成软件 FP/SIMD 现场。
struct fp_regs
{
    __uint128_t q[ARM64_FP_Q_REG_COUNT];
    uint32_t fpcr; //浮点控制寄存器:在 99% 的运行时间几乎不变，整个系统共同遵循同一套默认规则IEEE 754。特殊场景可能会改为Flush-to-Zero
    uint32_t fpsr; //浮点状态寄存器: 浮点向量指令完成后硬件自动更新，模拟的话需要手动更
};

_Static_assert(__builtin_offsetof(struct fp_regs, q) == 0, "fp_regs q offset must match register assembly");
_Static_assert(__builtin_offsetof(struct fp_regs, fpcr) == 512, "fp_regs fpcr offset must follow Q0-Q31");
_Static_assert(__builtin_offsetof(struct fp_regs, fpsr) == 516, "fp_regs fpsr offset must follow FPCR");
_Static_assert(sizeof(struct fp_regs) == 528, "fp_regs size must preserve 16-byte alignment");
_Static_assert(__alignof__(struct fp_regs) == 16, "fp_regs must remain 16-byte aligned");

// Q寄存器名称拼接辅助宏：QREG(0) → q0, QREG(1) → q1, ...
#define QREG(n) q##n
#define VREG(n) v##n

#define READ_Q_REG_CASE(N, DST)                                                     \
    case N:                                                                         \
        asm volatile("str " __stringify(QREG(N)) ", [%0]\n" ::"r"(DST) : "memory"); \
        break

#define WRITE_Q_REG_CASE(N, SRC)                                                    \
    case N:                                                                         \
        asm volatile("ldr " __stringify(QREG(N)) ", [%0]\n" ::"r"(SRC) : "memory"); \
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
static __always_inline void read_q_reg(int n, void *dst)
{
    switch (n)
    {
        GEN_READ_Q_REG_CASES(dst);
    default:
        break;
    }
}

// n: Q寄存器编号 0~31, src: 指向 16 字节数据的只读指针
static __always_inline void write_q_reg(int n, const void *src)
{
    switch (n)
    {
        GEN_WRITE_Q_REG_CASES(src);
    default:
        break;
    }
}
/*
  FPCR 控制浮点运算“怎么计算”，例如舍入模式、FZ/DN 和异常陷阱使能；
  FPSR 记录浮点运算“发生了什么”，例如累计异常标志和 QC 状态。
*/

// 读取当前 FPCR 控制配置，用于保存浮点运算环境。
static __always_inline void read_fpcr(uint32_t *val)
{
    asm volatile("mrs %x0, fpcr" : "=r"(*val));
}

// 写入 FPCR 控制配置，用于恢复浮点运算环境；不会修改 FPSR 状态。
static __always_inline void write_fpcr(uint32_t val)
{
    asm volatile("msr fpcr, %x0" : : "r"(val));
}

// 读取当前 FPSR 状态标志，用于保存浮点运算结果状态。
static __always_inline void read_fpsr(uint32_t *val)
{
    asm volatile("mrs %x0, fpsr" : "=r"(*val));
}

// 写入 FPSR 状态标志，用于恢复累计异常等状态；不会修改 FPCR 配置。
static __always_inline void write_fpsr(uint32_t val)
{
    asm volatile("msr fpsr, %x0" : : "r"(val));
}

// 批量读取 Q0-Q31、FPCR 和 FPSR，输出到 regs 指向的软件现场。
static __always_inline void read_all_q_regs(struct fp_regs *regs)
{
    asm volatile("stp q0, q1, [%0, #0]\n"
                 "stp q2, q3, [%0, #32]\n"
                 "stp q4, q5, [%0, #64]\n"
                 "stp q6, q7, [%0, #96]\n"
                 "stp q8, q9, [%0, #128]\n"
                 "stp q10, q11, [%0, #160]\n"
                 "stp q12, q13, [%0, #192]\n"
                 "stp q14, q15, [%0, #224]\n"
                 "stp q16, q17, [%0, #256]\n"
                 "stp q18, q19, [%0, #288]\n"
                 "stp q20, q21, [%0, #320]\n"
                 "stp q22, q23, [%0, #352]\n"
                 "stp q24, q25, [%0, #384]\n"
                 "stp q26, q27, [%0, #416]\n"
                 "stp q28, q29, [%0, #448]\n"
                 "stp q30, q31, [%0, #480]\n"
                 "mrs x9, fpcr\n"
                 "str w9, [%0, #512]\n"
                 "mrs x9, fpsr\n"
                 "str w9, [%0, #516]\n"
                 :
                 : "r"(regs)
                 : "x9", "memory");
}

// 从 regs 指向的软件现场批量写入 Q0-Q31、FPCR 和 FPSR。
/*
非常严重的记录，让我非常头疼，耗费巨量时间和金钱修复
2026-09-17：PTE 断点导致人物姿态异常，但程序继续运行的问题记录。

一、最早是怎么定位的(定位问题很难受，真正找到了才发现问题是如此简单)

最初怀疑受管页指令的解码、执行器派发、C 逻辑或硬件模板参数错误。
instruction.txt 第 2097-3120 行的 1024 条指令在既定输入下与实体 CPU
单步对拍全部一致，但该测试只比较软件现场，未覆盖 PTE handler 返回时的硬件现场。

因此继续沿“保存现场 -> 模拟 -> 回写 CPU -> 函数返回”检查。
发现本函数 write_all_q_regs 原来的 clobber 列表包含 v0-v31，
其中 v8-v15 的低 64 位按 AArch64 C 调用约定属于 callee-saved，
编译器需要让它们在 C 函数返回时不变。

反汇编现有 6.1-Android14 模块的 ptebp_handle_exec_fault，确认实际顺序为：

    入口：stp d15, d14, ...；随后保存其余 d8-d13。
    回写：ldp q8, q9, ...；随后装入其余完整 Q 寄存器。
    出口：ldp d9, d8, ...；随后恢复其余 d10-d15。

本函数内联后，ABI 保存恢复发生在外层 handler，不一定能在 helper 本身看到。
修复后重新构建完整模块，确认 Q 回写保留而 d8-d15 保存恢复消失；
随后在实际场景复测确认姿态恢复。

二、会造成什么严重的情况

结论：即使解码、执行条目选择、执行函数的 C 逻辑及 C 与汇编模板的交换
全部正确，每条指令的结果也都正确写入了软件现场，这个错误仍然可以独立发生。
故障不要求执行器算错任何一条指令；它发生在正确结果提交之后的函数返回阶段。

完整过程必须区分为四步：

    1. 执行器正确计算，将最终结果保存在 fp_regs.q[0..31] 中。
    2. write_all_q_regs 的 ldp qN 正确装入完整 128 位结果；此时硬件 Q 值也正确。
    3. 外层 ptebp_handle_exec_fault 返回前，编译器插入的 ldp d8-d15 又执行一次
       ABI 恢复：把 Q8-Q15 低 64 位覆盖成 handler 入口旧值，并清零高 64 位。
    4. 返回用户态时使用的是第 3 步破坏后的硬件现场，不是第 1 步的正确软件现场。

例如用 [高64位, 低64位] 表示 Q8：入口为 [OLD_H, OLD_L]，模拟正确结果为
[NEW_H, NEW_L]。ldp q8 回写后确实得到 [NEW_H, NEW_L]，但出口恢复 d8 后
变成 [0, OLD_L]。内存中的 fp_regs.q[8] 仍是 [NEW_H, NEW_L]，并没有被改坏。
因此，只打印软件现场或只对拍执行器输出，会看到正确结果而漏掉这个错误。

不是所有计算都被撤销：这组 ABI 恢复直接破坏的是 Q8-Q15，Q0-Q7、Q16-Q31、
GPR 软件现场及此前已完成的内存写入不会被这组 ldp dN 撤销。若某次新旧低位
恰好相等且新高位本来就是零，错误可能暂时不可见；否则这些寄存器的新结果丢失。
后续矩阵、四元数和位置等运算继续使用损坏的 Q8-Q15，错误随数据依赖传播，
最终可以表现为整个人物姿态异常，而不只是某一个局部数值错误。

这些寄存器加载本身合法，不必触发异常；执行器已返回 HANDLED，失败回滚也
不会启动。原模块的未命中提前返回路径同样经过 d8-d15 恢复，会清零这些 Q
寄存器的高半部，影响不局限于实际进入批量模拟的路径。

三、修复原理

这里提交的是异常软件现场，目的是让新 Q 值一直保留到异常返回，不是普通 C
函数对调用者浮点临时值的修改。去掉向量 clobber，避免触发外层 C 函数的
callee-saved 保存恢复；使用 __always_inline 避免形成普通的函数调用边界。
仅强制内联而保留原 clobber 不能修复，保存恢复会转移到外层函数。

省略向量 clobber 是本项目异常边界的特殊约定，依赖调用对象使用
-mno-implicit-float、-fno-vectorize、-fno-slp-vectorize，且周边 C 代码
没有编译器管理的活跃 FP/SIMD 值。memory clobber 描述内存副作用，绝不等于
告诉编译器所有向量寄存器都改变了。普通允许浮点计算的 C 函数不可照搬此法；
若需要支持那种环境，应在独立汇编异常出口完成最终提交，并明确调用边界。
回写后到异常返回之间也不能再有破坏 FP/SIMD 的调用或显式汇编。

四、其他类似的编译器与汇编边界陷阱

- asm volatile 不能代替输入、输出和 clobber 约束，也不是完整的顺序屏障；
    隐含读取内存需用内存操作数或 memory clobber 描述，硬件屏障仍按架构需要使用。

- 普通内联汇编修改 NZCV 应声明 cc，读写同一操作数通常需要 + 约束；输出在
    其他输入读完前就被写入时可能需要 & early-clobber，否则寄存器分配可重叠。

- 只禁用循环和 SLP 向量化不代表没有 SIMD 指令：复制、清零、寄存器分配和
    ABI 保存恢复仍需检查；

- 本次 -mno-implicit-float 也没有阻止显式 clobber 引发的 d8-d15 保存恢复，
    不能把某一个编译选项当作完整保证。

- 硬件模板的 naked/basic asm 不会自动补齐 C ABI 保存恢复。若借用 x19-x29
    或 v8-v15，需要自己满足对应保留约定；优先按模板约定使用 caller-saved
    寄存器。asm 内隐藏的 bl 也不会自动向编译器描述整套函数调用副作用。

- write_q_reg 同样移除了向量 clobber 并强制内联，依赖上述禁止隐式 FP/SIMD
    的特殊约定；异常现场提交仍应检查最终汇编，不能只看 helper 或低 64 位结果。

- LTO、优化级别、内联决策和工具链变化可能改变序言、尾声与尾调用。升级或
    修改编译配置后要检查最终模块的所有返回路径，尤其是 Q 回写之后的 SIMD
    加载；验证应比较完整 128 位，而不是只比较 D/S 或软件现场。
*/
static __always_inline void write_all_q_regs(const struct fp_regs *regs)
{
    asm volatile("ldp q0, q1, [%0, #0]\n"
                 "ldp q2, q3, [%0, #32]\n"
                 "ldp q4, q5, [%0, #64]\n"
                 "ldp q6, q7, [%0, #96]\n"
                 "ldp q8, q9, [%0, #128]\n"
                 "ldp q10, q11, [%0, #160]\n"
                 "ldp q12, q13, [%0, #192]\n"
                 "ldp q14, q15, [%0, #224]\n"
                 "ldp q16, q17, [%0, #256]\n"
                 "ldp q18, q19, [%0, #288]\n"
                 "ldp q20, q21, [%0, #320]\n"
                 "ldp q22, q23, [%0, #352]\n"
                 "ldp q24, q25, [%0, #384]\n"
                 "ldp q26, q27, [%0, #416]\n"
                 "ldp q28, q29, [%0, #448]\n"
                 "ldp q30, q31, [%0, #480]\n"
                 "ldr w9, [%0, #512]\n"
                 "msr fpcr, x9\n"
                 "ldr w9, [%0, #516]\n"
                 "msr fpsr, x9\n"
                 :
                 : "r"(regs)
                 : "x9", "memory");
}

// ========== 系统寄存器访问与 CPU 能力查询 ==========

static inline void arm64_write_tpidr_el0(uint64_t value)
{
    write_sysreg(value, tpidr_el0);
}

static inline uint64_t arm64_read_tpidr_el0(void)
{
    return read_sysreg(tpidr_el0);
}

static inline uint64_t arm64_read_tpidrro_el0(void)
{
    return read_sysreg(tpidrro_el0);
}

static inline uint64_t arm64_read_cntvct_el0(void)
{
    return read_sysreg(cntvct_el0);
}

// 读取 CurrentEL
static inline unsigned int read_current_el(void)
{
    unsigned long val;
    asm volatile("mrs %0, CurrentEL\n\t"
                 "lsr %0, %0, #2\n\t"
                 "and %0, %0, #0x3"
                 : "=r"(val)
                 :
                 : "cc");
    return (unsigned int)val;
}

// 读取 ID_AA64PFR0_EL
static inline unsigned int read_el2_implemented(void)
{
    unsigned long val;
    asm volatile("mrs %0, ID_AA64PFR0_EL1\n\t"
                 "lsr %0, %0, #8\n\t"
                 "and %0, %0, #0xF"
                 : "=r"(val)
                 :
                 : "cc");
    return (unsigned int)val;
}

// 读取 ID_AA64MMFR1_EL1.VH[11:8]，非 0 表示支持 Virtualization Host Extensions。
static inline unsigned int read_vhe_support(void)
{
    unsigned long val;
    asm volatile("mrs %0, ID_AA64MMFR1_EL1\n\t"
                 "lsr %0, %0, #8\n\t"
                 "and %0, %0, #0xF"
                 : "=r"(val)
                 :
                 : "cc");
    return (unsigned int)val;
}

// ========== 自托管追踪寄存器与能力查询 ==========

/* ETMv4/ETE system-register encodings. */
#ifndef SYS_TRCACVR
#define SYS_TRCACVR(m) sys_reg(2, 1, 2, ((m & 7) << 1), (m >> 3))
#endif
#ifndef SYS_TRCACATR
#define SYS_TRCACATR(m) sys_reg(2, 1, 2, ((m & 7) << 1), (2 | (m >> 3)))
#endif
#ifndef SYS_TRCVICTLR
#define SYS_TRCVICTLR sys_reg(2, 1, 0, 0, 2)
#endif
#ifndef SYS_TRCVIIECTLR
#define SYS_TRCVIIECTLR sys_reg(2, 1, 0, 1, 2)
#endif
#ifndef SYS_TRCVISSCTLR
#define SYS_TRCVISSCTLR sys_reg(2, 1, 0, 2, 2)
#endif
#ifndef SYS_TRCPRGCTLR
#define SYS_TRCPRGCTLR sys_reg(2, 1, 0, 1, 0)
#endif
#ifndef SYS_TRCTRACEIDR
#define SYS_TRCTRACEIDR sys_reg(2, 1, 0, 0, 1)
#endif
#ifndef SYS_TRCDEVARCH
#define SYS_TRCDEVARCH sys_reg(2, 1, 7, 15, 6)
#endif
#ifndef SYS_TRFCR_EL1
#define SYS_TRFCR_EL1 sys_reg(3, 0, 1, 2, 1)
#endif
#ifndef SYS_OSLAR_EL1
#define SYS_OSLAR_EL1 sys_reg(2, 0, 1, 0, 4)
#endif
#ifndef SYS_OSLSR_EL1
#define SYS_OSLSR_EL1 sys_reg(2, 0, 1, 1, 4)
#endif

/* TRBE system-register encodings. */
#ifndef SYS_TRBBASER_EL1
#define SYS_TRBBASER_EL1 sys_reg(3, 0, 9, 11, 2)
#endif
#ifndef SYS_TRBPTR_EL1
#define SYS_TRBPTR_EL1 sys_reg(3, 0, 9, 11, 1)
#endif
#ifndef SYS_TRBLIMITR_EL1
#define SYS_TRBLIMITR_EL1 sys_reg(3, 0, 9, 11, 0)
#endif
#ifndef SYS_TRBSR_EL1
#define SYS_TRBSR_EL1 sys_reg(3, 0, 9, 11, 3)
#endif
#ifndef SYS_TRBIDR_EL1
#define SYS_TRBIDR_EL1 sys_reg(3, 0, 9, 11, 7)
#endif

/* BRBE system-register encodings. */
#ifndef SYS_BRBFCR_EL1
#define SYS_BRBFCR_EL1 sys_reg(2, 1, 9, 0, 1)
#endif
#ifndef SYS_BRBCR_EL1
#define SYS_BRBCR_EL1 sys_reg(2, 1, 9, 0, 0)
#endif
#ifndef SYS_BRBIDR0_EL1
#define SYS_BRBIDR0_EL1 sys_reg(2, 1, 9, 2, 0)
#endif
#ifndef SYS_BRBINF_EL1
#define SYS_BRBINF_EL1(n) sys_reg(2, 1, 8, (n & 15), (((n & 16) >> 2) | 0))
#endif
#ifndef SYS_BRBSRC_EL1
#define SYS_BRBSRC_EL1(n) sys_reg(2, 1, 8, (n & 15), (((n & 16) >> 2) | 1))
#endif
#ifndef SYS_BRBTGT_EL1
#define SYS_BRBTGT_EL1(n) sys_reg(2, 1, 8, (n & 15), (((n & 16) >> 2) | 2))
#endif

/* SPE system-register encodings. */
#ifndef SYS_PMSCR_EL1
#define SYS_PMSCR_EL1 sys_reg(3, 0, 9, 9, 0)
#endif
#ifndef SYS_PMSICR_EL1
#define SYS_PMSICR_EL1 sys_reg(3, 0, 9, 9, 2)
#endif
#ifndef SYS_PMSFCR_EL1
#define SYS_PMSFCR_EL1 sys_reg(3, 0, 9, 9, 4)
#endif
#ifndef SYS_PMSEVFR_EL1
#define SYS_PMSEVFR_EL1 sys_reg(3, 0, 9, 9, 5)
#endif
#ifndef SYS_PMSADR_EL1
#define SYS_PMSADR_EL1 sys_reg(3, 0, 9, 9, 6)
#endif
#ifndef SYS_PMBPTR_EL1
#define SYS_PMBPTR_EL1 sys_reg(3, 0, 9, 10, 0)
#endif
#ifndef SYS_PMBSR_EL1
#define SYS_PMBSR_EL1 sys_reg(3, 0, 9, 10, 1)
#endif
#ifndef SYS_PMBLIMITR_EL1
#define SYS_PMBLIMITR_EL1 sys_reg(3, 0, 9, 10, 2)
#endif

#define ARM64_TRACE_DFR0_TRACEVER_SHIFT    4
#define ARM64_TRACE_DFR0_TRACEFILT_SHIFT   40
#define ARM64_TRACE_DFR0_BRBE_SHIFT        52
#define ARM64_TRACE_DFR0_TRACEBUFFER_SHIFT 44
#define ARM64_TRACE_DFR0_PMSVER_SHIFT      32

#define ARM64_TRACE_TRCDEVARCH_PRESENT   BIT(20)
#define ARM64_TRACE_TRCDEVARCH_ARCHITECT ((0x4U << 28) | (0x3bU << 21))
#define ARM64_TRACE_TRCDEVARCH_ETE       (ARM64_TRACE_TRCDEVARCH_ARCHITECT | (0x5U << 12) | 0xa13U | ARM64_TRACE_TRCDEVARCH_PRESENT)
#define ARM64_TRACE_TRCDEVARCH_ETMV4X    (ARM64_TRACE_TRCDEVARCH_ARCHITECT | (0x4U << 12) | 0xa13U | ARM64_TRACE_TRCDEVARCH_PRESENT)
#define ARM64_TRACE_TRCDEVARCH_ID_MASK   (GENMASK(31, 21) | GENMASK(15, 0) | BIT(20))
#define ARM64_TRACE_OSLSR_OSLM_NI        0x0U
#define ARM64_TRACE_OSLSR_OSLM_PE        0x4U

#define ARM64_TRACE_TRCVICTLR_EVENT_ALWAYS 0x1U
#define ARM64_TRACE_TRCVICTLR_SSSTATUS     BIT(9)
#define ARM64_TRACE_TRCACATR_EXLEVEL_SHIFT 8
#define ARM64_TRACE_EXLEVEL_APP            BIT(4)
#define ARM64_TRACE_EXLEVEL_OS             BIT(5)
#define ARM64_TRACE_EXLEVEL_HYP            BIT(6)
#define ARM64_TRACE_TRFCR_E0TRE            BIT(0)
#define ARM64_TRACE_TRFCR_EXTRE            BIT(1)
#define ARM64_TRACE_TRFCR_TS_VIRTUAL       BIT(5)
#define ARM64_TRACE_TRBLIMITR_LIMIT_MASK   GENMASK_ULL(63, 12)
#define ARM64_TRACE_TRBLIMITR_TM_STOP      0
#define ARM64_TRACE_TRBLIMITR_TM_IRQ       BIT(3)
#define ARM64_TRACE_TRBLIMITR_E            BIT(0)
#define ARM64_TRACE_TRBSR_IRQ              BIT(22)
#define ARM64_TRACE_TRBSR_WRAP             BIT(20)
#define ARM64_TRACE_TRBSR_ABORT            BIT(18)
#define ARM64_TRACE_TRBSR_STOP             BIT(17)
#define ARM64_TRACE_TRBIDR_ALIGN_MASK      GENMASK(3, 0)
#define ARM64_TRACE_TRBIDR_PROG            BIT(4)
#define ARM64_TRACE_ID_BASE                0x10

#define ARM64_TRACE_BRBIDR_CC_MASK        GENMASK(15, 12)
#define ARM64_TRACE_BRBIDR_FORMAT_MASK    GENMASK(11, 8)
#define ARM64_TRACE_BRBIDR_NUMREC_MASK    GENMASK(7, 0)
#define ARM64_TRACE_BRBIDR_CC_20_BIT      0x5
#define ARM64_TRACE_BRBIDR_FORMAT_0       0
#define ARM64_TRACE_BRBCR_E0BRE           BIT(0)
#define ARM64_TRACE_BRBCR_EXBRE           BIT(1)
#define ARM64_TRACE_BRBCR_CC              BIT(3)
#define ARM64_TRACE_BRBCR_MPRED           BIT(4)
#define ARM64_TRACE_BRBCR_TS_VIRTUAL      BIT(5)
#define ARM64_TRACE_BRBCR_FZP             BIT(8)
#define ARM64_TRACE_BRBFCR_DIRECT         BIT(17)
#define ARM64_TRACE_BRBFCR_INDIRECT       BIT(18)
#define ARM64_TRACE_BRBFCR_RETURN         BIT(19)
#define ARM64_TRACE_BRBFCR_INDIRECT_CALL  BIT(20)
#define ARM64_TRACE_BRBFCR_DIRECT_CALL    BIT(21)
#define ARM64_TRACE_BRBFCR_COND_DIRECT    BIT(22)
#define ARM64_TRACE_BRBFCR_PAUSED         BIT(7)
#define ARM64_TRACE_BRBFCR_BANK_MASK      GENMASK(29, 28)
#define ARM64_TRACE_BRBINF_VALID_MASK     GENMASK(1, 0)
#define ARM64_TRACE_BRBINF_MISPRED        BIT(5)
#define ARM64_TRACE_BRBINF_EL_MASK        GENMASK(7, 6)
#define ARM64_TRACE_BRBINF_TYPE_MASK      GENMASK(13, 8)
#define ARM64_TRACE_BRBINF_CC_UNAVAILABLE BIT(46)

static __always_inline uint64_t arm64_reg_read_dfr0(void)
{
    return read_sysreg(id_aa64dfr0_el1);
}

static __always_inline uint64_t arm64_reg_read_etm_devarch(void)
{
    return read_sysreg_s(SYS_TRCDEVARCH);
}

static __always_inline uint64_t arm64_reg_read_etm_os_lock_status(void)
{
    return read_sysreg_s(SYS_OSLSR_EL1);
}

static __always_inline void arm64_reg_write_etm_program_control(uint64_t value)
{
    write_sysreg_s(value, SYS_TRCPRGCTLR);
}

static __always_inline void arm64_reg_write_etm_os_lock(uint64_t value)
{
    uint64_t oslsr = read_sysreg_s(SYS_OSLSR_EL1);
    uint32_t oslm = (uint32_t)(((oslsr & GENMASK(4, 3)) >> 2) | (oslsr & BIT(0)));

    if (oslm != ARM64_TRACE_OSLSR_OSLM_PE) return;
    write_sysreg_s(value, SYS_OSLAR_EL1);
    isb();
}

static __always_inline void arm64_reg_write_etm_trace_id(uint64_t value)
{
    write_sysreg_s(value, SYS_TRCTRACEIDR);
}

static __always_inline void arm64_reg_write_etm_view_inst_control(uint64_t value)
{
    write_sysreg_s(value, SYS_TRCVICTLR);
}

static __always_inline void arm64_reg_write_etm_view_inst_include(uint64_t value)
{
    write_sysreg_s(value, SYS_TRCVIIECTLR);
}

static __always_inline void arm64_reg_write_etm_view_inst_start_stop(uint64_t value)
{
    write_sysreg_s(value, SYS_TRCVISSCTLR);
}

static __always_inline void arm64_reg_write_etm_address_value(unsigned int index, uint64_t value)
{
    switch (index)
    {
    case 0:
        write_sysreg_s(value, SYS_TRCACVR(0));
        break;
    case 1:
        write_sysreg_s(value, SYS_TRCACVR(1));
        break;
    default:
        break;
    }
}

static __always_inline void arm64_reg_write_etm_address_access(unsigned int index, uint64_t value)
{
    switch (index)
    {
    case 0:
        write_sysreg_s(value, SYS_TRCACATR(0));
        break;
    case 1:
        write_sysreg_s(value, SYS_TRCACATR(1));
        break;
    default:
        break;
    }
}

static __always_inline void arm64_reg_write_etm_trace_filter(uint64_t value)
{
    write_sysreg_s(value, SYS_TRFCR_EL1);
}

static __always_inline uint64_t arm64_reg_read_etm_trace_filter(void)
{
    return read_sysreg_s(SYS_TRFCR_EL1);
}

static __always_inline uint64_t arm64_reg_read_trbe_status(void)
{
    return read_sysreg_s(SYS_TRBSR_EL1);
}

static __always_inline uint64_t arm64_reg_read_trbe_pointer(void)
{
    return read_sysreg_s(SYS_TRBPTR_EL1);
}

static __always_inline uint64_t arm64_reg_read_trbe_id(void)
{
    return read_sysreg_s(SYS_TRBIDR_EL1);
}

static __always_inline void arm64_reg_write_trbe_base(uint64_t value)
{
    write_sysreg_s(value, SYS_TRBBASER_EL1);
}

static __always_inline void arm64_reg_write_trbe_pointer(uint64_t value)
{
    write_sysreg_s(value, SYS_TRBPTR_EL1);
}

static __always_inline void arm64_reg_write_trbe_limit(uint64_t value)
{
    write_sysreg_s(value, SYS_TRBLIMITR_EL1);
}

static __always_inline void arm64_reg_write_trbe_status(uint64_t value)
{
    write_sysreg_s(value, SYS_TRBSR_EL1);
}

static __always_inline uint64_t arm64_reg_read_brbe_id(void)
{
    return read_sysreg_s(SYS_BRBIDR0_EL1);
}

static __always_inline uint64_t arm64_reg_read_brbe_control(void)
{
    return read_sysreg_s(SYS_BRBCR_EL1);
}

static __always_inline void arm64_reg_write_brbe_control(uint64_t value)
{
    write_sysreg_s(value, SYS_BRBCR_EL1);
}

static __always_inline uint64_t arm64_reg_read_brbe_filter(void)
{
    return read_sysreg_s(SYS_BRBFCR_EL1);
}

static __always_inline void arm64_reg_write_brbe_filter(uint64_t value)
{
    write_sysreg_s(value, SYS_BRBFCR_EL1);
}

#define ARM64_REG_BRBE_INDEX_CASES(op) op(0) op(1) op(2) op(3) op(4) op(5) op(6) op(7) op(8) op(9) op(10) op(11) op(12) op(13) op(14) op(15) op(16) op(17) op(18) op(19) op(20) op(21) op(22) op(23) op(24) op(25) op(26) op(27) op(28) op(29) op(30) op(31) op(32) op(33) op(34) op(35) op(36) op(37) op(38) op(39) op(40) op(41) op(42) op(43) op(44) op(45) op(46) op(47) op(48) op(49) op(50) op(51) op(52) op(53) op(54) op(55) op(56) op(57) op(58) op(59) op(60) op(61) op(62) op(63)

#define ARM64_REG_BRBE_INFO_CASE(index) \
    case index:                         \
        return read_sysreg_s(SYS_BRBINF_EL1(index));
#define ARM64_REG_BRBE_SOURCE_CASE(index) \
    case index:                           \
        return read_sysreg_s(SYS_BRBSRC_EL1(index));
#define ARM64_REG_BRBE_TARGET_CASE(index) \
    case index:                           \
        return read_sysreg_s(SYS_BRBTGT_EL1(index));

static __always_inline uint64_t arm64_reg_read_brbe_info(unsigned int index)
{
    switch (index)
    {
        ARM64_REG_BRBE_INDEX_CASES(ARM64_REG_BRBE_INFO_CASE)
    default:
        return 0;
    }
}

static __always_inline uint64_t arm64_reg_read_brbe_source(unsigned int index)
{
    switch (index)
    {
        ARM64_REG_BRBE_INDEX_CASES(ARM64_REG_BRBE_SOURCE_CASE)
    default:
        return 0;
    }
}

static __always_inline uint64_t arm64_reg_read_brbe_target(unsigned int index)
{
    switch (index)
    {
        ARM64_REG_BRBE_INDEX_CASES(ARM64_REG_BRBE_TARGET_CASE)
    default:
        return 0;
    }
}

static __always_inline void arm64_reg_brbe_invalidate(void)
{
#ifdef BRB_IALL_INSN
    isb();
    asm volatile(BRB_IALL_INSN);
    isb();
#endif
}

static __always_inline uint64_t arm64_reg_read_spe_status(void)
{
    return read_sysreg_s(SYS_PMBSR_EL1);
}

static __always_inline void arm64_reg_write_spe_control(uint64_t value)
{
    write_sysreg_s(value, SYS_PMSCR_EL1);
}

static __always_inline void arm64_reg_write_spe_pointer(uint64_t value)
{
    write_sysreg_s(value, SYS_PMBPTR_EL1);
}

static __always_inline void arm64_reg_write_spe_limit(uint64_t value)
{
    write_sysreg_s(value, SYS_PMBLIMITR_EL1);
}

static __always_inline void arm64_reg_write_spe_status(uint64_t value)
{
    write_sysreg_s(value, SYS_PMBSR_EL1);
}

static __always_inline void arm64_reg_write_spe_address(uint64_t value)
{
    write_sysreg_s(value, SYS_PMSADR_EL1);
}

static __always_inline void arm64_reg_write_spe_filter(uint64_t value)
{
    write_sysreg_s(value, SYS_PMSFCR_EL1);
}

static __always_inline void arm64_reg_write_spe_interval(uint64_t value)
{
    write_sysreg_s(value, SYS_PMSICR_EL1);
}

static __always_inline void arm64_reg_write_spe_event_filter(uint64_t value)
{
    write_sysreg_s(value, SYS_PMSEVFR_EL1);
}

static __always_inline unsigned int arm64_reg_trace_version(void)
{
    return (arm64_reg_read_dfr0() >> ARM64_TRACE_DFR0_TRACEVER_SHIFT) & 0xfU;
}

static __always_inline bool arm64_reg_supports_sysreg_trace(void)
{
    return arm64_reg_trace_version() != 0;
}

static __always_inline bool arm64_reg_supports_trace_filter(void)
{
    return ((arm64_reg_read_dfr0() >> ARM64_TRACE_DFR0_TRACEFILT_SHIFT) & 0xfU) != 0;
}

static __always_inline bool arm64_reg_supports_trbe(void)
{
    uint64_t trbidr;
    uint64_t alignment;

    if (((arm64_reg_read_dfr0() >> ARM64_TRACE_DFR0_TRACEBUFFER_SHIFT) & 0xfU) == 0) return false;
    trbidr = arm64_reg_read_trbe_id();
    if (!(trbidr & ARM64_TRACE_TRBIDR_PROG)) return false;
    alignment = 1ULL << (trbidr & ARM64_TRACE_TRBIDR_ALIGN_MASK);
    return alignment <= 2048;
}

static __always_inline bool arm64_reg_supports_spe(void)
{
    return ((arm64_reg_read_dfr0() >> ARM64_TRACE_DFR0_PMSVER_SHIFT) & 0xfU) != 0;
}

static __always_inline bool arm64_reg_supports_brbe(void)
{
    unsigned int version = (arm64_reg_read_dfr0() >> ARM64_TRACE_DFR0_BRBE_SHIFT) & 0xfU;
    uint64_t brbidr;

    if (version != 1 && version != 2) return false;

    brbidr = arm64_reg_read_brbe_id();
    return ((brbidr & ARM64_TRACE_BRBIDR_CC_MASK) >> 12) == ARM64_TRACE_BRBIDR_CC_20_BIT && ((brbidr & ARM64_TRACE_BRBIDR_FORMAT_MASK) >> 8) == ARM64_TRACE_BRBIDR_FORMAT_0;
}

static __always_inline unsigned int arm64_reg_brbe_record_count(void)
{
    uint64_t brbidr = arm64_reg_read_brbe_id();
    unsigned int count = brbidr & ARM64_TRACE_BRBIDR_NUMREC_MASK;

    return count == 8 || count == 16 || count == 32 || count == 64 ? count : 0;
}

static __always_inline uint32_t arm64_reg_trace_devarch(void)
{
    return (uint32_t)arm64_reg_read_etm_devarch();
}

static __always_inline bool arm64_reg_has_etm_source(void)
{
    uint32_t devarch;

    if (!arm64_reg_supports_sysreg_trace()) return false;

    devarch = arm64_reg_trace_devarch() & ARM64_TRACE_TRCDEVARCH_ID_MASK;
    return devarch == ARM64_TRACE_TRCDEVARCH_ETE || devarch == ARM64_TRACE_TRCDEVARCH_ETMV4X;
}

static __always_inline bool arm64_reg_has_etm_sysreg_control(void)
{
    uint64_t oslsr = arm64_reg_read_etm_os_lock_status();
    uint32_t oslm = (uint32_t)(((oslsr & GENMASK(4, 3)) >> 2) | (oslsr & BIT(0)));

    return oslm == ARM64_TRACE_OSLSR_OSLM_NI || oslm == ARM64_TRACE_OSLSR_OSLM_PE;
}

// 读取 SCTLR_EL1
static inline unsigned long read_sctlr_el1(void)
{
    unsigned long val;
    asm volatile("mrs %0, SCTLR_EL1" : "=r"(val) : : "cc");
    return val;
}

// Vendor Hypervisor UID 查询接口，用于识别 EL2 侧厂商 hypervisor 服务。
#ifndef ARM_SMCCC_VENDOR_HYP_CALL_UID_FUNC_ID
#define ARM_SMCCC_VENDOR_HYP_CALL_UID_FUNC_ID ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, ARM_SMCCC_OWNER_VENDOR_HYP, 0xff01)
#endif

// ARM 标准 workaround 1 查询 ID，常用于判断 Spectre v2 缓解接口是否存在。
#ifndef ARM_SMCCC_ARCH_WORKAROUND_1
#define ARM_SMCCC_ARCH_WORKAROUND_1 ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, ARM_SMCCC_OWNER_ARCH, 0x8000)
#endif

// ARM 标准 workaround 2 查询 ID，常用于判断 SSBD/Spectre v4 缓解接口是否存在。
#ifndef ARM_SMCCC_ARCH_WORKAROUND_2
#define ARM_SMCCC_ARCH_WORKAROUND_2 ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, ARM_SMCCC_OWNER_ARCH, 0x7fff)
#endif

// ARM 标准 workaround 3 查询 ID，常用于判断 Spectre-BHB 缓解接口是否存在。
#ifndef ARM_SMCCC_ARCH_WORKAROUND_3
#define ARM_SMCCC_ARCH_WORKAROUND_3 ARM_SMCCC_CALL_VAL(ARM_SMCCC_FAST_CALL, ARM_SMCCC_SMC_32, ARM_SMCCC_OWNER_ARCH, 0x3fff)
#endif

static void arm_smccc_call_conduit(enum arm_smccc_conduit conduit, unsigned long arg0, unsigned long arg1, unsigned long arg2, unsigned long arg3, unsigned long arg4, unsigned long arg5, unsigned long arg6, unsigned long arg7, struct arm_smccc_res *res)
{
    if (conduit == SMCCC_CONDUIT_HVC) arm_smccc_hvc(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, res);
    else arm_smccc_smc(arg0, arg1, arg2, arg3, arg4, arg5, arg6, arg7, res);
}

// 查询一个标准 SMCCC function id 是否受支持；res.a0 是 SMCCC 返回码。
static void print_smccc_arch_feature(enum arm_smccc_conduit conduit, unsigned long func_id, const char *name)
{
    struct arm_smccc_res res;

    arm_smccc_call_conduit(conduit, ARM_SMCCC_ARCH_FEATURES_FUNC_ID, func_id, 0, 0, 0, 0, 0, 0, &res);
    ls_log("SMCCC feature %-22s: %ld (0x%lx)\n", name, res.a0, res.a0);
}

// 强制使用 HVC 探测 SMCCC
static void print_smccc_probe(unsigned int current_el, unsigned int el2_implemented)
{
    ls_log("===== SMCCC Probe =====\n");

    if (current_el != 1)
    {
        ls_log("SMCCC probe        : skipped (CurrentEL is EL%u)\n", current_el);
        return;
    }

    // 调用未导出的 arm_smccc_1_1_get_conduit()
    ls_log("SMCCC kernel conduit: UNKNOWN (symbol not exported)\n");
    ls_log("SMCCC active conduit: HVC (forced)\n");

    if (!el2_implemented)
    {
        ls_log("SMCCC HVC probe    : skipped (EL2 not implemented)\n");
        return;
    }

    struct arm_smccc_res res;
    // 注释掉或移除相关代码，避免引入未导出符号
    // enum arm_smccc_conduit kernel_conduit;
    enum arm_smccc_conduit conduit = SMCCC_CONDUIT_HVC;
    arm_smccc_call_conduit(conduit, ARM_SMCCC_VERSION_FUNC_ID, 0, 0, 0, 0, 0, 0, 0, &res);
    ls_log("SMCCC version      : 0x%lx (major=%lu minor=%lu)\n", res.a0, (res.a0 >> 16) & 0xffff, res.a0 & 0xffff);

    print_smccc_arch_feature(conduit, ARM_SMCCC_VERSION_FUNC_ID, "SMCCC_VERSION");
    print_smccc_arch_feature(conduit, ARM_SMCCC_ARCH_FEATURES_FUNC_ID, "ARCH_FEATURES");
    print_smccc_arch_feature(conduit, ARM_SMCCC_ARCH_WORKAROUND_1, "ARCH_WORKAROUND_1");
    print_smccc_arch_feature(conduit, ARM_SMCCC_ARCH_WORKAROUND_2, "ARCH_WORKAROUND_2");
    print_smccc_arch_feature(conduit, ARM_SMCCC_ARCH_WORKAROUND_3, "ARCH_WORKAROUND_3");

    arm_smccc_call_conduit(conduit, ARM_SMCCC_VENDOR_HYP_CALL_UID_FUNC_ID, 0, 0, 0, 0, 0, 0, 0, &res);
    ls_log("Vendor hyp UID     : %08lx-%08lx-%08lx-%08lx\n", res.a0, res.a1, res.a2, res.a3);
}

// 输出Hypervisor相关信息
static void print_el2_status(void)
{
    struct device_node *np;
    struct property *prop;
    const char *str;
    bool hyp_hint = false;
    bool tz_hint = false;

    /*
    读取当前 CPU 正在运行的异常级别
    CurrentEL[3:2]：
    01b = EL1
    10b = EL2
    */
    unsigned int current_el = read_current_el();

    /*
    判断硬件是否实现 EL2。
    ID_AA64PFR0_EL1[11:8]
    0 = 未实现 EL2
    1 = 实现 EL2
    */
    unsigned int el2_implemented = read_el2_implemented();

    /*
    判断硬件是否支持 VHE(Virtualization Host Extensions)。
    ID_AA64MMFR1_EL1[11:8]
    非 0 表示支持虚拟机主机扩展
    */
    unsigned int vhe_supported = read_vhe_support();

    /*
    读取 SCTLR_EL1。是 MMU 与 Cache 是否启用的总开关。
    在 VHE 模式下，EL1 系统寄存器访问可能会被重定向到 EL2 侧。
    这里仅打印出来作为辅助判断信息。
    */
    unsigned long sctlr_el1 = read_sctlr_el1();

    ls_log("===== EL2 Detection =====\n");

    // 打印当前运行级别。
    ls_log("CurrentEL          : EL%u\n", current_el);

    // 打印硬件是否实现 EL2。
    ls_log("EL2 implemented    : %s (ID_AA64PFR0_EL1[11:8] = %u)\n", el2_implemented ? "YES" : "NO", el2_implemented);

    // 打印硬件是否支持 VHE。注意：硬件支持 VHE，不代表当前系统已经启用 VHE。
    ls_log("VHE supported      : %s (ID_AA64MMFR1_EL1[11:8] = %u)\n", vhe_supported ? "YES" : "NO", vhe_supported);

    // 打印 SCTLR_EL1 的当前值。该值主要用于辅助观察当前控制寄存器状态。
    ls_log("SCTLR_EL1          : 0x%016lx\n", sctlr_el1);

    // 判断 VHE 是否 active。
    ls_log("VHE mode active    : %s\n", current_el == 2 ? "YES" : "NO");

    // 判断当前是否可以直接访问 EL2 寄存器。
    ls_log("EL2 regs accessible: %s\n", current_el == 2 ? "YES" : "NO (trap)");

    // 运行在 EL2 时，读取 HCR_EL2。
    if (current_el == 2)
    {
        unsigned long hcr_el2;

        // 读取 HCR_EL2。
        asm volatile("mrs %0, HCR_EL2" : "=r"(hcr_el2) : :);

        ls_log("HCR_EL2            : 0x%016lx\n", hcr_el2);
        ls_log("  E2H bit[34]      : %lu (VHE=%s)\n", (hcr_el2 >> 34) & 1, ((hcr_el2 >> 34) & 1) ? "enabled" : "disabled");
    }
    else
    {
        ls_log("HCR_EL2            : NOT readable from EL1 (would trap)\n");
    }

    ls_log("===== Hypervisor / TrustZone / Platform Probe =====\n");

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
        ls_log("DT /hypervisor     : present\n");
        of_property_for_each_string(np, "compatible", prop, str)
        {
            ls_log("  compatible       : %s\n", str);

            if (strnstr(str, "gunyah", strlen(str)) || strnstr(str, "haven", strlen(str)) || strnstr(str, "qhee", strlen(str)) || strnstr(str, "qtee", strlen(str)))
            {
                hyp_hint = true;
            }
        }

        of_node_put(np);
    }
    else
    {
        ls_log("DT /hypervisor     : no hyp node\n");
    }
    np = of_find_node_by_path("/psci");
    if (!np) np = of_find_node_by_path("/firmware/psci");
    if (np)
    {
        ls_log("DT PSCI            : present\n");

        of_property_for_each_string(np, "compatible", prop, str)
        {
            ls_log("  compatible       : %s\n", str);

            if (strnstr(str, "psci", strlen(str)) || strnstr(str, "arm,psci", strlen(str)))
            {
                tz_hint = true;
            }
        }

        of_node_put(np);
    }
    else
    {
        ls_log("DT PSCI            : not found\n");
    }

    // 直接看芯片/平台型号
    np = of_find_node_by_path("/");
    if (np)
    {
        const char *model;

        if (!of_property_read_string(np, "model", &model)) ls_log("DT model           : %s\n", model);
        else ls_log("DT model           : unavailable\n");

        of_property_for_each_string(np, "compatible", prop, str)
        {
            ls_log("DT compatible      : %s\n", str);

            if (strnstr(str, "qcom", strlen(str))) ls_log("  platform hint    : Qualcomm SoC / board\n");

            if (strnstr(str, "gunyah", strlen(str)) || strnstr(str, "haven", strlen(str)) || strnstr(str, "qhee", strlen(str)) || strnstr(str, "qtee", strlen(str)))
            {
                hyp_hint = true;
            }
        }

        of_node_put(np);
    }
    else
    {
        ls_log("DT root            : unavailable\n");
    }

    ls_log("HV keyword hint    : %s\n", hyp_hint ? "YES" : "NO");
    ls_log("TZ/PSCI hint       : %s\n", tz_hint ? "YES" : "NO");

    print_smccc_probe(current_el, el2_implemented);

    ls_log("=========================\n");
}

#endif
