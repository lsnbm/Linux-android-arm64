#ifndef _EXPORT_FUN_H_
#define _EXPORT_FUN_H_

#include <linux/version.h>
#include <linux/kprobes.h>


#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0)

/* 定义函数指针类型 */
typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);

/*
 * Android 5.15+ GKI 开启了 CFI (Control Flow Integrity)。
 * 即使拿到了函数地址，直接强转调用也会触发 CFI Panic。
 * 必须使用 no_sanitize("cfi") 禁止编译器对该 wrapper 函数进行检查。
 */
#ifdef __clang__
__attribute__((no_sanitize("cfi")))
#endif
static unsigned long _bypass_cfi_call(unsigned long addr, const char *name)
{
        kallsyms_lookup_name_t fn = (kallsyms_lookup_name_t)addr;
        return fn(name);
}

static inline unsigned long generic_kallsyms_lookup_name(const char *name)
{
        static unsigned long kallsyms_addr = 0;
        struct kprobe kp = {0};
        int ret;

        // 1. 如果还没获取到 kallsyms_lookup_name 的地址，先通过 kprobe 获取
        if (!kallsyms_addr)
        {
                kp.symbol_name = "kallsyms_lookup_name";

                ret = register_kprobe(&kp);
                if (ret < 0)
                {
                        return 0;
                }

                kallsyms_addr = (unsigned long)kp.addr;
                unregister_kprobe(&kp);

                if (!kallsyms_addr)
                {
                        return 0;
                }
        }

        // 2. 使用绕过 CFI 的包装函数进行调用
        return _bypass_cfi_call(kallsyms_addr, name);
}

#else

/*
 * < 5.7.0 的内核，可以直接调用内核导出的符号
 */
static inline unsigned long generic_kallsyms_lookup_name(const char *name)
{
        return kallsyms_lookup_name(name);
}

#endif 

#endif 