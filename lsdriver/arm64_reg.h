#ifndef LSDRIVER_ARM64_REG_H
#define LSDRIVER_ARM64_REG_H

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

#include "lsdriver_log.h"

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
    bit 15 (MDE): Monitor Debug Enable，用户态调试使能(EL0)
    bit 13 (KDE): Kernel Debug Enable，内核态调试使能(EL1)
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
        ls_log_tag("driver", "attempt to read from unknown breakpoint register %d\n", n);
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
        ls_log_tag("driver", "attempt to write to unknown breakpoint register %d\n", n);
    }
    isb();
}

// ========== FP/SIMD 寄存器操作 ==========

#define ARM64_FP_Q_REG_COUNT 32

// Q0-Q31 统一按 128 位宽度保存；V/S/D/H/B 都是这些槽位的不同视图。
struct fp_regs
{
    __uint128_t q[ARM64_FP_Q_REG_COUNT];
};

// Q寄存器名称拼接辅助宏：QREG(0) → q0, QREG(1) → q1, ...
#define QREG(n) q##n
#define VREG(n) v##n

#define READ_Q_REG_CASE(N, DST)                                        \
    case N:                                                            \
        asm volatile(".arch_extension fp\n.arch_extension simd\n"      \
                     "str " __stringify(QREG(N)) ", [%0]\n" ::"r"(DST) \
                     : "memory");                                      \
        break

#define WRITE_Q_REG_CASE(N, SRC)                                       \
    case N:                                                            \
        asm volatile(".arch_extension fp\n.arch_extension simd\n"      \
                     "ldr " __stringify(QREG(N)) ", [%0]\n" ::"r"(SRC) \
                     : "memory", __stringify(VREG(N)));                \
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

// n: Q寄存器编号 0~31, src: 指向 16 字节数据的只读指针
static inline void write_q_reg(int n, const void *src)
{
    switch (n)
    {
        GEN_WRITE_Q_REG_CASES(src);
    default:
        break;
    }
}

// 批量读取 Q0-Q31，输出到 regs 指向的软件现场。
static inline void read_all_q_regs(struct fp_regs *regs)
{
    asm volatile(".arch_extension fp\n"
                 ".arch_extension simd\n"
                 "stp q0, q1, [%0, #0]\n"
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
                 :
                 : "r"(regs)
                 : "memory");
}

// 从 regs 指向的软件现场批量写入 Q0-Q31。
static inline void write_all_q_regs(const struct fp_regs *regs)
{
    asm volatile(".arch_extension fp\n"
                 ".arch_extension simd\n"
                 "ldp q0, q1, [%0, #0]\n"
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
                 :
                 : "r"(regs)
                 : "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v8", "v9", "v10", "v11", "v12", "v13", "v14", "v15", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31");
}

/*
  FPCR 控制浮点运算“怎么计算”，例如舍入模式、FZ/DN 和异常陷阱使能；
  FPSR 记录浮点运算“发生了什么”，例如累计异常标志和 QC 状态。
*/

// 读取当前 FPCR 控制配置，用于保存浮点运算环境。
static inline uint32_t read_fpcr(void)
{
    uint64_t v;
    asm volatile(".arch_extension fp\n"
                 "mrs %0, fpcr"
                 : "=r"(v));
    return (uint32_t)v;
}

// 写入 FPCR 控制配置，用于恢复浮点运算环境；不会修改 FPSR 状态。
static inline void write_fpcr(uint32_t val)
{
    uint64_t v = val;
    asm volatile(".arch_extension fp\n"
                 "msr fpcr, %0"
                 :
                 : "r"(v));
}

// 读取当前 FPSR 状态标志，用于保存浮点运算结果状态。
static inline uint32_t read_fpsr(void)
{
    uint64_t v;
    asm volatile(".arch_extension fp\n"
                 "mrs %0, fpsr"
                 : "=r"(v));
    return (uint32_t)v;
}

// 写入 FPSR 状态标志，用于恢复累计异常等状态；不会修改 FPCR 配置。
static inline void write_fpsr(uint32_t val)
{
    uint64_t v = val;
    asm volatile(".arch_extension fp\n"
                 "msr fpsr, %0"
                 :
                 : "r"(v));
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
