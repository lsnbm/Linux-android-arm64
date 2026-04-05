#ifndef _EXPORT_FUN_H_
#define _EXPORT_FUN_H_

#include <linux/version.h>
#include <linux/kprobes.h>
#include <linux/types.h>
#include <linux/kprobes.h>

// 屏蔽 CFI 检查，利用 kprobe 获取 kallsyms_lookup_name 地址
__attribute__((no_sanitize("cfi"))) static unsigned long generic_kallsyms_lookup_name(const char *name)
{
        typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);
        static unsigned long kallsyms_addr = 0;
        struct kprobe kp = {0};

        if (!kallsyms_addr)
        {
                kp.symbol_name = "kallsyms_lookup_name";
                if (register_kprobe(&kp) < 0)
                        return 0;
                kallsyms_addr = (unsigned long)kp.addr;
                unregister_kprobe(&kp);
        }

        if (!kallsyms_addr)
                return 0;

        kallsyms_lookup_name_t fn = (kallsyms_lookup_name_t)kallsyms_addr;
        return fn(name);
}

/*
下面代码 由https://github.com/wangchuan2009提供
旧版 CFI ( GKI 5.10 / 5.15):
        编译器会在间接调用前插入跳转，跳到一个集中的验证函数（就是 __cfi_slowpath）。
        你把它 patch 成 RET，相当于让验证永远通过
新版 KCFI (Kernel 6.1+):
         编译器去掉了集中验证函数。KCFI 会在每一个间接跳转（BLR）指令的前面，内联插入几条汇编指令，
         直接比较 hash 值。如果不对，直接触发 BRK 指令宕机。
 如果是 6.1+ 内核，不存在 __cfi_slowpath，

 */
__attribute__((no_sanitize("cfi"))) bool bypass_cfi(void)
{
        // AArch64 RET 指令机器码
#define AARCH64_RET_INSTR 0xD65F03C0
        // 内部状态，记录是否已经热更新成功
        static bool is_cfi_bypassed = false;
        uint64_t cfi_addr = 0;

        int (*x_aarch64_insn_patch_text_nosync)(void *, u32);

        if (is_cfi_bypassed)
                return true;

        // 获取 patch 函数
        x_aarch64_insn_patch_text_nosync =
            (void *)generic_kallsyms_lookup_name("aarch64_insn_patch_text_nosync");

        if (!x_aarch64_insn_patch_text_nosync)
                return false;

        //  依次查找各个版本的 CFI slowpath 函数
        cfi_addr = generic_kallsyms_lookup_name("__cfi_slowpath"); // 5.10
        if (!cfi_addr)
                cfi_addr = generic_kallsyms_lookup_name("__cfi_slowpath_diag"); // 5.15
        if (!cfi_addr)
                cfi_addr = generic_kallsyms_lookup_name("_cfi_slowpath"); // 5.4

        if (!cfi_addr)
                return false;

        // 强行 Patch 成 RET 指令 (直接返回，使得所有 CFI 校验默认通过)
        if (x_aarch64_insn_patch_text_nosync((void *)cfi_addr, AARCH64_RET_INSTR) != 0)
                return false;

        // x_aarch64_insn_patch_text_nosync内部一般已经处理了 缓存，
        // flush_icache_range(cfi_addr, cfi_addr + 4);

        is_cfi_bypassed = true;
        return true;
}

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

#endif /* _EXPORT_FUN_H_ */