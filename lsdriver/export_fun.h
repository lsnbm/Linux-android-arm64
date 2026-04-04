#ifndef _EXPORT_FUN_H_
#define _EXPORT_FUN_H_

#include <linux/version.h>
#include <linux/kprobes.h>
#include <linux/types.h>

/*
 6系内核就不用这个宏了，可以直接拿着函数指针调用

 * ARM64 内联汇编调用宏 (绕过 CFI / KCFI)
 *
 * 通过纯汇编指令 (blr) 直接跳转执行目标地址，从而绕过编译器的插入cfi
 *
 * 核心寄存器保护列表
 * 遵循 AAPCS64 (ARM64 过程调用约定) 声明 Caller-saved (调用者保存 / 易失) 寄存器。
 *
 *  [1] 通用寄存器
 *      - x9 ~ x15  : 临时调用者保存寄存器。
 *      - x16 ~ x17 : 过程内调用寄存器 (IP0, IP1 / PLT 专用)。
 *       (x0~x7和x18~x30是非易失性寄存器，属于 Callee-saved，被调用函数会负责恢复，因此无需在此声明)
 *  [2] 浮点/向量寄存器
 *      - v0 ~ v7   : 浮点参数与返回值寄存器 (调用后可能被修改)。
 *      - v16 ~ v31 : 临时调用者保存寄存器。
 *      (v8~v15 是非易失性寄存器，属于 Callee-saved，被调用函数会负责恢复，因此无需在此声明。如果确认运行环境为纯整数运算不涉及浮点，可删除 v 系列以微调性能)
 *
 *  [3] 特殊标志与屏障
 *      - lr (x30)  : 链接寄存器 (blr 指令执行时必定会覆盖它)。
 *      - cc        : 状态标志寄存器 (如 NZCV，被调用函数可能会修改条件标志)。
 *      - memory    : 编译器内存屏障，强制将寄存器缓存写回内存，并防止指令重排。
 */
#define _KCALL_CLOBBERS                                                                     \
        "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "lr", "cc", "memory", \
            "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7",                                 \
            "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23",                         \
            "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"

// 调用 0 个参数的函数
#define KCALL_0(fn_addr, ret_type) ({                                                                                                \
        register uint64_t _x0 asm("x0");                                                                                             \
        asm volatile("blr %1\n" : "=r"(_x0) : "r"((uint64_t)(fn_addr)) : "x1", "x2", "x3", "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                              \
})

// 调用 1 个参数的函数
#define KCALL_1(fn_addr, ret_type, a1) ({                                                                                            \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                            \
        asm volatile("blr %1\n" : "+r"(_x0) : "r"((uint64_t)(fn_addr)) : "x1", "x2", "x3", "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                              \
})

// 调用 2 个参数的函数
#define KCALL_2(fn_addr, ret_type, a1, a2) ({                                                                                             \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                 \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                 \
        asm volatile("blr %2\n" : "+r"(_x0), "+r"(_x1) : "r"((uint64_t)(fn_addr)) : "x2", "x3", "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                   \
})

// 调用 3 个参数的函数
#define KCALL_3(fn_addr, ret_type, a1, a2, a3) ({                                                                                              \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                      \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                      \
        register uint64_t _x2 asm("x2") = (uint64_t)(a3);                                                                                      \
        asm volatile("blr %3\n" : "+r"(_x0), "+r"(_x1), "+r"(_x2) : "r"((uint64_t)(fn_addr)) : "x3", "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                        \
})

// 调用 4 个参数的函数
#define KCALL_4(fn_addr, ret_type, a1, a2, a3, a4) ({                                                                                               \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                           \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                           \
        register uint64_t _x2 asm("x2") = (uint64_t)(a3);                                                                                           \
        register uint64_t _x3 asm("x3") = (uint64_t)(a4);                                                                                           \
        asm volatile("blr %4\n" : "+r"(_x0), "+r"(_x1), "+r"(_x2), "+r"(_x3) : "r"((uint64_t)(fn_addr)) : "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                             \
})

// 调用 5 个参数的函数
#define KCALL_5(fn_addr, ret_type, a1, a2, a3, a4, a5) ({                                                                                                \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                                \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                                \
        register uint64_t _x2 asm("x2") = (uint64_t)(a3);                                                                                                \
        register uint64_t _x3 asm("x3") = (uint64_t)(a4);                                                                                                \
        register uint64_t _x4 asm("x4") = (uint64_t)(a5);                                                                                                \
        asm volatile("blr %5\n" : "+r"(_x0), "+r"(_x1), "+r"(_x2), "+r"(_x3), "+r"(_x4) : "r"((uint64_t)(fn_addr)) : "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                                  \
})

// 调用 6 个参数的函数
#define KCALL_6(fn_addr, ret_type, a1, a2, a3, a4, a5, a6) ({                                                                                                 \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                                     \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                                     \
        register uint64_t _x2 asm("x2") = (uint64_t)(a3);                                                                                                     \
        register uint64_t _x3 asm("x3") = (uint64_t)(a4);                                                                                                     \
        register uint64_t _x4 asm("x4") = (uint64_t)(a5);                                                                                                     \
        register uint64_t _x5 asm("x5") = (uint64_t)(a6);                                                                                                     \
        asm volatile("blr %6\n" : "+r"(_x0), "+r"(_x1), "+r"(_x2), "+r"(_x3), "+r"(_x4), "+r"(_x5) : "r"((uint64_t)(fn_addr)) : "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                                       \
})

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)

// 必须使用 unsigned long，与内核原生定义保持一致
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);

/*
Android 5.15+ GKI 开启了 CFI
即使拿到了函数地址，直接强转调用也会触发 CFI Panic。
必须使用 no_sanitize("cfi") 禁止编译器对该 wrapper 函数进行检查。
*/
#ifdef __clang__
__attribute__((no_sanitize("cfi")))  // 屏蔽cfi
__attribute__((no_sanitize("kcfi"))) // 屏蔽kcfi
#endif

static unsigned long generic_kallsyms_lookup_name(const char *name)
{
        static unsigned long kallsyms_addr = 0;
        struct kprobe kp = {0};
        int ret;

        if (!kallsyms_addr)
        {
                kp.symbol_name = "kallsyms_lookup_name";

                ret = register_kprobe(&kp);
                if (ret < 0)
                        return 0;

                kallsyms_addr = (unsigned long)kp.addr;
                unregister_kprobe(&kp);

                if (!kallsyms_addr)
                        return 0;
        }

        // 不使用这种方式调用函数会有cfi校验，或其他问题
        // kallsyms_lookup_name_t fn = (kallsyms_lookup_name_t)kallsyms_addr;
        // return fn(name);

        /*
          KCALL_1
          参数1:函数指针地址
          参数2:函数返回类型(强转的话直接填要转的参数就行)
          参数3:函数参数列表
          */
        return KCALL_1(kallsyms_addr, unsigned long, name);
}

#else

// < 5.7.0
static unsigned long generic_kallsyms_lookup_name(const char *name)
{
        return kallsyms_lookup_name(name);
}

#endif

#endif
