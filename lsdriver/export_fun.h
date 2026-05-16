#ifndef _EXPORT_FUN_H_
#define _EXPORT_FUN_H_
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/kprobes.h>
#include <linux/types.h>
#include <linux/kprobes.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#include <asm/cpufeature.h>
#include <asm/pgtable.h>
#include <asm/pgtable-prot.h>
#include <asm/tlbflush.h>
#include "arm64_reg.h"

// 屏蔽 CFI 检查，统一利用 kprobe 获取 kallsyms_lookup_name 地址
__attribute__((no_sanitize("cfi"))) static unsigned long generic_kallsyms_lookup_name(const char *name)
{
        unsigned long (*fn_kallsyms_lookup_name)(const char *name) = NULL;
        struct kprobe kp = {0};

        if (!fn_kallsyms_lookup_name)
        {
                kp.symbol_name = "kallsyms_lookup_name";
                if (register_kprobe(&kp) < 0)
                        return 0;
                fn_kallsyms_lookup_name = (void *)kp.addr;
                unregister_kprobe(&kp);
        }

        if (!fn_kallsyms_lookup_name)
                return 0;

        return fn_kallsyms_lookup_name(name);
}

/*

旧版 CFI ( GKI 5.10 / 5.15):
        编译器编译时进行类型哈希计算，在间接调用前插入跳转，跳到一个集中的验证函数（就是 __cfi_slowpath）来运行时比对，
        校验失败直接panic
        你把它 patch 成 RET，相当于让验证永远通过
新版 KCFI (Kernel 6.1+):
        内核去掉了集中验证函数。编译器会在每一个间接跳转（BLR）指令的前面，内联插入几条汇编指令，
        直接比较 hash 值。如果不对，直接触发 BRK 指令宕机。
如果是 6.1+ 内核，不存在 __cfi_slowpath，

所以有好人给了一个5系的解决代码给我，所以5系就不用下面纯汇编进行间接调用了
感谢bypass_cfi由https://github.com/wangchuan2009(忘川)，处理运行时校验函数来过5系cfi
*/

int (*fn_aarch64_insn_patch_text_nosync)(void *addr, uint32_t insn);

__attribute__((no_sanitize("cfi"))) bool bypass_cfi(void)
{
        // AArch64 RET 指令机器码
#define AARCH64_RET_INSTR 0xD65F03C0
        // 内部状态，记录是否已经热更新成功
        static bool is_cfi_bypassed = false;
        uint64_t cfi_addr = 0;

        if (is_cfi_bypassed)
                return true;

        // 获取 patch 函数
        fn_aarch64_insn_patch_text_nosync =
            (void *)generic_kallsyms_lookup_name("aarch64_insn_patch_text_nosync");

        if (!fn_aarch64_insn_patch_text_nosync)
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
        if (fn_aarch64_insn_patch_text_nosync((void *)cfi_addr, AARCH64_RET_INSTR) != 0)
                return false;

        // aarch64_insn_patch_text_nosync内部一般已经处理了 缓存，
        // flush_icache_range(cfi_addr, cfi_addr + 4);

        is_cfi_bypassed = true;
        return true;
}

//------------------下面是通用，但未导出，未定义函数-----------------

// 获取内核态虚拟地址的pte
static inline pte_t *get_kernel_pte(uint64_t vaddr)
{
        pgd_t *pgd;
        p4d_t *p4d;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *ptep;

        // PGD Level
        pgd = get_kernel_pgd_base() + pgd_index(vaddr);
        if (pgd_none(*pgd) || pgd_bad(*pgd))
                return NULL;

        // P4D Level
        p4d = p4d_offset(pgd, vaddr);
        if (p4d_none(*p4d) || p4d_bad(*p4d))
                return NULL;

        // PUD Level (可能遇到 1GB 大页)
        pud = pud_offset(p4d, vaddr);
        if (pud_none(*pud))
                return NULL;

        // 检查是否是 1G 大页
        if (pud_leaf(*pud))
                return NULL;

        if (pud_bad(*pud))
                return NULL;

        // PMD Level (可能遇到 2MB 大页)
        pmd = pmd_offset(pud, vaddr);
        if (pmd_none(*pmd))
                return NULL;

        // 检查是否是 2M 大页
        if (pmd_leaf(*pmd))
                return NULL;

        if (pmd_bad(*pmd))
                return NULL;

        // PTE Level (普通的 4KB 页)
        // 较新内核中 __pte_offset_map 不导出，对于 64位 系统直接使用 pte_offset_kernel 即可
        ptep = pte_offset_kernel(pmd, vaddr);
        if (!ptep)
                return NULL;

        return ptep;
}

// 获取用户态虚拟地址的pte
static inline pte_t *get_user_pte(struct mm_struct *mm, uint64_t vaddr)
{
        pgd_t *pgd;
        p4d_t *p4d;
        pud_t *pud;
        pmd_t *pmd;
        pte_t *ptep;

        if (!mm)
                return NULL;

        // PGD Level
        pgd = pgd_offset(mm, vaddr);
        if (pgd_none(*pgd) || pgd_bad(*pgd))
                return NULL;

        // P4D Level
        p4d = p4d_offset(pgd, vaddr);
        if (p4d_none(*p4d) || p4d_bad(*p4d))
                return NULL;

        // PUD Level (可能遇到 1GB 大页)
        pud = pud_offset(p4d, vaddr);
        if (pud_none(*pud))
                return NULL;

        // 检查是否是 1G 大页
        if (pud_leaf(*pud))
                return NULL;

        if (pud_bad(*pud))
                return NULL;

        // PMD Level (可能遇到 2MB 大页)
        pmd = pmd_offset(pud, vaddr);
        if (pmd_none(*pmd))
                return NULL;

        // 检查是否是 2M 大页
        if (pmd_leaf(*pmd))
                return NULL;

        if (pmd_bad(*pmd))
                return NULL;

        // PTE Level (普通的 4KB 页)
        // 较新内核中 __pte_offset_map 不导出，对于 64位 系统直接使用 pte_offset_kernel 即可
        ptep = pte_offset_kernel(pmd, vaddr);
        if (!ptep)
                return NULL;

        return ptep;
}

// 编码一条b指令
static int arm64_make_b(unsigned long from, unsigned long to, uint32_t *insn)
{
        int64_t offset = (int64_t)to - (int64_t)from;

        if (offset < -(1LL << 27) || offset > ((1LL << 27) - 4))
                return -ERANGE;

        *insn = 0x14000000 | ((offset >> 2) & 0x03FFFFFF);
        return 0;
}

// 释放一批通过GUP获取的page *;避免使用 put_page() 把 page_pinner 拉进来。
static void release_gup_pages(struct page **pages, int nr)
{
        typedef void (*release_pages_t)(struct page **pages, int nr);
        static release_pages_t fn_release_pages;

        if (!pages || nr <= 0)
                return;

        if (!fn_release_pages)
                fn_release_pages = (release_pages_t)generic_kallsyms_lookup_name("release_pages");

        if (!fn_release_pages)
        {
                pr_debug("严重错误！无法找到 release_pages，跳过 %d 个页引用回收\n", nr);
                return;
        }

        fn_release_pages(pages, nr);
}

struct execmem_private_header
{
        uint64_t magic;    // 校验 execmem_free() 传入的指针是否来自 execmem_alloc()
        void *base;        // vzalloc() 返回的原始页对齐地址，vfree() 必须释放这个地址
        size_t alloc_size; // 实际分配并修改页权限的大小，释放时按这个范围恢复 NX
};

#define EXECMEM_PRIVATE_MAGIC 0x455845434D454DULL
#define EXECMEM_PRIVATE_ALIGN 16

// 如果内核启用了 BTI，可执行页的 Guarded Page 位要为 PTE_GP，否则为 0
#if defined(PTE_MAYBE_GP)
#define EXECMEM_PTE_MAYBE_GP PTE_MAYBE_GP
#elif defined(CONFIG_ARM64_BTI_KERNEL) && defined(PTE_GP)
#define EXECMEM_PTE_MAYBE_GP PTE_GP
#else
#define EXECMEM_PTE_MAYBE_GP 0
#endif

// 分配可执行内存页
static void *execmem_alloc(int type, size_t size)
{
        struct execmem_private_header *hdr; // 私有头，用于释放时找回 base
        void *base;
        void *code;
        size_t header_size;
        size_t alloc_size;
        unsigned long start;
        unsigned long addr;
        unsigned long end;

        (void)type;

        if (!size)
                return NULL;

        header_size = ALIGN(sizeof(struct execmem_private_header),
                            EXECMEM_PRIVATE_ALIGN);  // 对齐私有头
        alloc_size = PAGE_ALIGN(header_size + size); // 私有头 + 代码区整体按页对齐

        base = vzalloc(alloc_size);
        if (!base)
                return NULL;

        start = (unsigned long)base;
        end = start + alloc_size;

        /*
         * Inline copy of arm64 set_memory_x():
         * set PTE_MAYBE_GP, clear PTE_PXN, then flush kernel TLB.
         */
        for (addr = start; addr < end; addr += PAGE_SIZE)
        {
                pte_t *ptep = get_kernel_pte(addr);
                pteval_t pte;

                if (!ptep)
                {
                        vfree(base);
                        return NULL;
                }

                pte = READ_ONCE(pte_val(*ptep));
                pte &= ~PTE_PXN;
                pte |= EXECMEM_PTE_MAYBE_GP;
                WRITE_ONCE(pte_val(*ptep), pte);
        }

        // 刷新tlb缓存，让映射生效
        flush_tlb_kernel_range(start, end);

        hdr = (struct execmem_private_header *)base;
        hdr->magic = EXECMEM_PRIVATE_MAGIC;
        hdr->base = base;
        hdr->alloc_size = alloc_size;

        code = (void *)(start + header_size);

        flush_icache_range(start, end);

        return code;
}

// 释放可执行内存页
static void execmem_free(void *ptr)
{
        struct execmem_private_header *hdr; // 私有头地址
        size_t header_size;
        void *base;
        size_t alloc_size;
        unsigned long start;
        unsigned long addr;
        unsigned long end;

        if (!ptr)
                return;

        header_size = ALIGN(sizeof(struct execmem_private_header),
                            EXECMEM_PRIVATE_ALIGN);

        hdr = (struct execmem_private_header *)((char *)ptr - header_size); // 由代码区地址反推私有头

        if (hdr->magic != EXECMEM_PRIVATE_MAGIC ||
            !hdr->base ||
            !hdr->alloc_size)
                return;

        base = hdr->base;
        alloc_size = hdr->alloc_size;
        hdr->magic = 0;

        start = (unsigned long)base;
        end = start + alloc_size;

        /*
         * Inline copy of arm64 set_memory_nx():
         * set PTE_PXN, clear PTE_MAYBE_GP, then flush kernel TLB.
         */
        for (addr = start; addr < end; addr += PAGE_SIZE)
        {
                pte_t *ptep = get_kernel_pte(addr);
                pteval_t pte;

                if (!ptep)
                        continue;

                pte = READ_ONCE(pte_val(*ptep));
                pte |= PTE_PXN;
                pte &= ~EXECMEM_PTE_MAYBE_GP;
                WRITE_ONCE(pte_val(*ptep), pte);
        }

        flush_tlb_kernel_range(start, end);
        vfree(base);
}

#endif /* _EXPORT_FUN_H_ */

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
