#ifndef _EXPORT_FUN_H_
#define _EXPORT_FUN_H_
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/kprobes.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/sched/task.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#include <asm/cpufeature.h>
#include <asm/pgalloc.h>
#include <asm/pgtable.h>
#include <asm/pgtable-prot.h>
#include <asm/tlbflush.h>
#include "arm64_encode/arm64_encode.h"
#include "arm64_reg.h"
#include "lsdriver_log.h"

/*
还有注意所有地方使用函数指针调用内核api，参数类型和返回值类型一定要与内核对齐，比如这里的 unsigned long就不能写为uint64_t, uint64_t定义为unsigned long long,虽然宽度一样，但是不能混合使用
*/

// 屏蔽 CFI 检查，统一利用 kprobe 获取 kallsyms_lookup_name 地址
__attribute__((no_sanitize("cfi"))) static unsigned long generic_kallsyms_lookup_name(const char *name)
{
    unsigned long (*fn_kallsyms_lookup_name)(const char *name) = NULL;
    struct kprobe kp = {0};

    if (!fn_kallsyms_lookup_name)
    {
        kp.symbol_name = "kallsyms_lookup_name";
        if (register_kprobe(&kp) < 0) return 0;
        fn_kallsyms_lookup_name = (void *)kp.addr;
        unregister_kprobe(&kp);
    }

    if (!fn_kallsyms_lookup_name) return 0;

    return fn_kallsyms_lookup_name(name);
}
int (*fn_aarch64_insn_patch_text)(void *addrs[], uint32_t insns[], int cnt);

/*

旧版 CFI ( GKI 5.10 / 5.15):
        编译器编译时进行类型哈希计算，在间接调用前插入跳转，跳到一个集中的验证函数（就是 __cfi_slowpath）来运行时比对，
        校验失败直接panic
        你把它 patch 成 RET，相当于让验证永远通过
新版 KCFI (Kernel 6.1+):
        内核去掉了集中验证函数。编译器会在每一个间接跳转（BLR）指令的前面，内联插入几条汇编指令，
        直接比较 hash 值。如果不对，直接触发 BRK 指令宕机。
如果是 6.1+ 内核，不存在 __cfi_slowpath，

所以有人提供了一个5系的解决代码给我，所以5系就不用下面纯汇编进行间接调用了
感谢https://github.com/wangchuan2009(忘川)，bypass_cfi处理运行时5系的集中校验函数,patch为ret来过5系cfi

2026/7/25 22:23 !!!!!!
后续我实机测试发现部分内核有间接调用__cfi_slowpath或__cfi_slowpath_diag或_cfi_slowpath,
直接patch为ret后cfi倒是过了，但是部分内核部分函数间接调用cfi检查函数下 BTI直接导致内核panic
*/

__attribute__((no_sanitize("cfi"))) bool bypass_cfi(void)
{
    // 内部状态，记录是否已经热更新成功
    static bool is_cfi_bypassed = false;

    if (is_cfi_bypassed) return true;

    // 获取同步 patch 函数：内部使用 stop_machine，避免其他 CPU 执行到半补丁状态。
    /*
        注意：aarch64_insn_patch_text_nosync不同步多cpu
        在patch目标地址多行指令时很可能导致其他cpu执行到半补丁状态，
        为了防止这个情况使用aarch64_insn_patch_text，
        如继续使用nosync，需要在patch时用stop_machine 来停止所有cpu,再热循环补丁，
        不过aarch64_insn_patch_text就是用的我说的这个方式，
        aarch64_insn_patch_text内部也是stop_machine 停止其他cpu，
        在循环调用aarch64_insn_patch_text_nosync来patch指令
        */
    fn_aarch64_insn_patch_text = (void *)generic_kallsyms_lookup_name("aarch64_insn_patch_text");

    if (!fn_aarch64_insn_patch_text) return false;

    //  依次查找各个版本的 CFI slowpath 函数
    uint64_t cfi_addr = generic_kallsyms_lookup_name("__cfi_slowpath");            // 5.10
    if (!cfi_addr) cfi_addr = generic_kallsyms_lookup_name("__cfi_slowpath_diag"); // 5.15
    if (!cfi_addr) cfi_addr = generic_kallsyms_lookup_name("_cfi_slowpath");       // 5.4

    if (!cfi_addr) return false;

    // 2026/7/25 22:23实机测试 修复，保留入口第 1 条 BTI 指令，固定将第 2 条指令 Patch 成 RET。
    void *patch_addrs[1] = {(void *)(cfi_addr + 4)};
    uint32_t patch_insns[1];
    if (arm64_encode_ret(30, patch_insns)) return false;
    if (fn_aarch64_insn_patch_text(patch_addrs, patch_insns, 1) != 0) return false;

    is_cfi_bypassed = true;
    return true;
}

//------------------下面是通用，但未导出，未定义函数-----------------

// 刷新同一缓存一致性域（Inner Shareable 域）内全部 CPU 中，与指定 VA 对应的所有 ASID TLB 项。
// Android SMP SoC 中屏障范围必须与 TLBI 广播范围匹配：VAALE1IS 广播到 Inner Shareable 域，因此前后必须使用 ISHST/ISH，不能使用仅覆盖本地范围的 NSHST/NSH。
static inline void flush_tlb_addr_all_asid_all_cpus(uint64_t addr)
{
    // TLBI 操作数不是原始 VA；__TLBI_VADDR() 会去掉页内偏移并转换为架构要求的 VA[55:12] 格式。
    uint64_t tlbi_addr = __TLBI_VADDR(addr, 0);

    asm volatile( // DSB ISHST：范围与后面的 VAALE1IS 广播范围匹配，等待此前 PTE 写入对域内其他 CPU 可见。
        "dsb ishst\n\t"
        // TLBI VAALE1IS 字段：VA=按虚拟地址，A=所有 ASID，L=仅最后一级页表项，E1=EL1 Stage-1，IS=广播到一致性域。
        "tlbi vaale1is, %[tlbi_addr]\n\t"
        // DSB ISH：范围同样与 VAALE1IS 匹配，等待该域内所有目标 CPU 完成失效后才能使用新映射。
        "dsb ish\n\t"
        // ISB：清空并重新同步当前 CPU 的取指/执行流水线，使后续指令使用更新后的地址翻译环境。
        "isb\n\t"
        :
        : [tlbi_addr] "r"(tlbi_addr)
        : "memory");
}

// 刷新同一缓存一致性域内全部 CPU 中，与半开区间 [start, end) 相交页面对应的所有 ASID TLB 项。
static inline void flush_tlb_range_all_asid_all_cpus(uint64_t start, uint64_t end)
{
    if (end <= start) return;

    uint64_t first_page = start & PAGE_MASK;
    uint64_t last_page = (end - 1) & PAGE_MASK;

    // ISHST 与下面 VAALE1IS 的广播范围匹配，等待此前整批 PTE 写入都对域内其他 CPU 可见。
    asm volatile("dsb ishst" : : : "memory");

    for (uint64_t addr = first_page;; addr += PAGE_SIZE)
    {
        uint64_t tlbi_addr = __TLBI_VADDR(addr, 0);
        // 逐页广播失效：按 VA、所有 ASID、最后一级 EL1 Stage-1 页表项、Inner Shareable 域。
        asm volatile("tlbi vaale1is, %0" : : "r"(tlbi_addr) : "memory");

        if (addr == last_page) break;
    }

    // ISH 与上面的 VAALE1IS 广播范围匹配，等待域内所有目标 CPU 完成整批 TLB 失效。
    asm volatile("dsb ish\n\t"
                 // 同步当前 CPU 流水线
                 "isb\n\t"
                 :
                 :
                 : "memory");
}

// 仅刷新当前 CPU 中与指定 VA 对应的所有 ASID TLB 项；调用方必须保证不会迁移到其他 CPU。
static inline void flush_tlb_addr_all_asid_current_cpu(uint64_t addr)
{
    // TLBI 操作数使用页号格式，不包含 VA 页内偏移。
    uint64_t tlbi_addr = __TLBI_VADDR(addr, 0);

    asm volatile("dsb nshst\n\t"
                 // TLBI VAALE1：字段与 VAALE1IS 相同，但没有 IS，因此只失效当前 PE/CPU 的对应 TLB 项。
                 "tlbi vaale1, %[tlbi_addr]\n\t"
                 // 等待当前 CPU 完成本地 TLB 失效。于上面的范围匹配，nsh不是共享域同步
                 "dsb nsh\n\t"
                 // 重新同步当前 CPU 的取指/执行流水线。
                 "isb\n\t"
                 :
                 : [tlbi_addr] "r"(tlbi_addr)
                 : "memory");
}

// 内部原语：把一段连续虚拟地址对应的数据缓存行清理到统一点。
static inline void __arm64_clean_dcache_range_to_pou(const void *address, size_t size)
{
    unsigned long line_size = arm64_dcache_line_size();
    unsigned long start = (unsigned long)address;
    unsigned long end = start + size;

    if (!size) return;

    for (unsigned long line = start & ~(line_size - 1); line < end; line += line_size) asm volatile("dc cvau, %0" : : "r"(line) : "memory");
}

// 内部原语：等待数据缓存清理完成，失效内部共享域全部 CPU 的指令缓存，并同步当前 CPU 的取指流水线。
static inline void __arm64_invalidate_icache_all_cpus(void)
{
    asm volatile("dsb ish\n\t"
                 "ic ialluis\n\t"
                 "dsb ish\n\t"
                 "isb\n\t"
                 :
                 :
                 : "memory");
}

//同步一段连续地址中的新机器码：内部先用 DC CVAU 把数据缓存清理到 PoU，再失效内部共享域全部 CPU 的指令缓存
static inline int arm64_sync_code_range_all_cpus(const void *address, size_t size)
{
    unsigned long start = (unsigned long)address;

    if (!address || !size) return -EINVAL;
    if (size > ULONG_MAX - start) return -EOVERFLOW;

    __arm64_clean_dcache_range_to_pou(address, size);
    __arm64_invalidate_icache_all_cpus();
    return 0;
}

//同步一组物理页中的新机器码。物理页可以不连续；函数通过各页的内核线性映射逐页清理数据缓存，最后统一失效指令缓存。
static inline int arm64_sync_code_pages_all_cpus(struct page **pages, unsigned int page_count, size_t code_size)
{
    size_t remaining = code_size;

    if (!pages || !page_count || !code_size) return -EINVAL;
    if (code_size > (size_t)page_count * PAGE_SIZE) return -E2BIG;

    for (unsigned int index = 0; index < page_count && remaining; index++)
    {
        size_t bytes = min_t(size_t, remaining, PAGE_SIZE);

        if (!pages[index]) return -EFAULT;

        __arm64_clean_dcache_range_to_pou(page_address(pages[index]), bytes);
        remaining -= bytes;
    }

    __arm64_invalidate_icache_all_cpus();
    return 0;
}

// 获取内核态虚拟地址的pte
static inline pte_t *get_kernel_pte(uint64_t vaddr)
{
    // PGD Level
    pgd_t *pgd = get_kernel_pgd_base() + pgd_index(vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) return NULL;

    // P4D Level
    p4d_t *p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) return NULL;

    // PUD Level (可能遇到 1GB 大页)
    pud_t *pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud)) return NULL;

    // 检查是否是 1G 大页
    if (pud_leaf(*pud)) return NULL;

    if (pud_bad(*pud)) return NULL;

    // PMD Level (可能遇到 2MB 大页)
    pmd_t *pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd)) return NULL;

    // 检查是否是 2M 大页
    if (pmd_leaf(*pmd)) return NULL;

    if (pmd_bad(*pmd)) return NULL;

    // PTE Level (普通的 4KB 页)
    // 较新内核中 __pte_offset_map 不导出，对于 64位 系统直接使用 pte_offset_kernel 即可
    pte_t *ptep = pte_offset_kernel(pmd, vaddr);
    if (!ptep) return NULL;

    return ptep;
}

// 获取用户态虚拟地址的pte
static inline pte_t *get_user_pte(struct mm_struct *mm, uint64_t vaddr)
{
    if (!mm) return NULL;

    // PGD Level
    pgd_t *pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd)) return NULL;

    // P4D Level
    p4d_t *p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d)) return NULL;

    // PUD Level (可能遇到 1GB 大页)
    pud_t *pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud)) return NULL;

    // 检查是否是 1G 大页
    if (pud_leaf(*pud)) return NULL;

    if (pud_bad(*pud)) return NULL;

    // PMD Level (可能遇到 2MB 大页)
    pmd_t *pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd)) return NULL;

    // 检查是否是 2M 大页
    if (pmd_leaf(*pmd)) return NULL;

    if (pmd_bad(*pmd)) return NULL;

    // PTE Level (普通的 4KB 页)
    // 较新内核中 __pte_offset_map 不导出，对于 64位 系统直接使用 pte_offset_kernel 即可
    pte_t *ptep = pte_offset_kernel(pmd, vaddr);
    if (!ptep) return NULL;

    return ptep;
}

// 根据 pid 获取 task_struct，调用方负责 put_task_struct。
static inline struct task_struct *get_task_by_pid(pid_t pid)
{
    struct pid *pid_struct = find_get_pid(pid);
    if (!pid_struct) return NULL;

    struct task_struct *task = get_pid_task(pid_struct, PIDTYPE_PID);
    put_pid(pid_struct);
    return task;
}

// 根据 pid 获取 mm_struct，调用方负责 mmput。
static inline struct mm_struct *get_mm_by_pid(pid_t pid)
{
    struct task_struct *task = get_task_by_pid(pid);
    if (!task) return NULL;

    struct mm_struct *mm = get_task_mm(task);
    put_task_struct(task);
    return mm;
}

/*
 为用户地址补齐页表层级并返回 PTE 指针。
 调用方必须已经持有 mmap_write_lock(mm)，本函数只分配页表页，不创建 VMA，
 适合调试/影子映射这类需要在空洞地址直接安装 PTE 的场景。
*/
static inline pte_t *get_or_alloc_user_pte(struct mm_struct *mm, uint64_t vaddr)
{
    if (!mm) return NULL;

    pgd_t *pgd = pgd_offset(mm, vaddr);
    if (pgd_bad(*pgd)) return NULL;
    if (pgd_none(*pgd))
    {
        p4d_t *new_p4d = p4d_alloc_one(mm, vaddr);
        if (!new_p4d) return NULL;
        pgd_populate(mm, pgd, new_p4d);
    }

    p4d_t *p4d = p4d_offset(pgd, vaddr);
    if (p4d_bad(*p4d)) return NULL;
    if (p4d_none(*p4d))
    {
        pud_t *new_pud = pud_alloc_one(mm, vaddr);
        if (!new_pud) return NULL;
        p4d_populate(mm, p4d, new_pud);
    }

    pud_t *pud = pud_offset(p4d, vaddr);
    if (pud_leaf(*pud) || pud_bad(*pud)) return NULL;
    if (pud_none(*pud))
    {
        pmd_t *new_pmd = pmd_alloc_one(mm, vaddr);
        if (!new_pmd) return NULL;
        pud_populate(mm, pud, new_pmd);
    }

    pmd_t *pmd = pmd_offset(pud, vaddr);
    if (pmd_leaf(*pmd) || pmd_bad(*pmd)) return NULL;
    if (pmd_none(*pmd))
    {
        pgtable_t new_pte = pte_alloc_one(mm);
        if (!new_pte) return NULL;
        pmd_populate(mm, pmd, new_pte);
    }

    pte_t *ptep = pte_offset_kernel(pmd, vaddr);
    return ptep;
}

// 检查一段用户 VA 范围是否没有 present PTE，调用方负责持有合适的 mmap 锁。
static inline bool user_pte_range_empty(struct mm_struct *mm, uint64_t addr, size_t size)
{
    if (!mm) return false;

    for (uint64_t cur = addr; cur < addr + size; cur += PAGE_SIZE)
    {
        pte_t *ptep = get_user_pte(mm, cur);
        if (ptep && pte_present(READ_ONCE(*ptep))) return false;
    }

    return true;
}

// 读取用户地址所在页的 PTE 值。
static inline int read_user_pte_value(struct mm_struct *mm, uint64_t addr, pteval_t *out_pte)
{
    if (!mm || !out_pte) return -EINVAL;

    pte_t *ptep = get_user_pte(mm, addr);
    if (!ptep) return -EFAULT;

    pte_t current_pte = READ_ONCE(*ptep);
    if (!pte_present(current_pte)) return -EFAULT;

    *out_pte = pte_val(current_pte);
    return 0;
}

// 写入用户地址所在页的 PTE，并用汇编刷新该用户页 TLB。
static inline int write_user_pte_value(struct mm_struct *mm, uint64_t addr, pteval_t new_pte)
{
    if (!mm) return -EINVAL;

    struct vm_area_struct *vma = find_vma(mm, addr);
    if (!vma || addr < vma->vm_start) return -EFAULT;

    pte_t *ptep = get_user_pte(mm, addr);
    if (!ptep) return -EFAULT;

    set_pte(ptep, __pte(new_pte));
    flush_tlb_addr_all_asid_all_cpus(addr);
    return 0;
}

/*
编码一条b指令

在各个内核源码链接：
Android 12 / 5.10
MODULES_VSIZE = SZ_128M
https://android.googlesource.com/kernel/common/+/refs/heads/android12-5.10/arch/arm64/include/asm/memory.h

Android 13 / 5.15
MODULES_VSIZE = SZ_128M
https://android.googlesource.com/kernel/common/+/refs/heads/android13-5.15/arch/arm64/include/asm/memory.h

Android 14 / 6.1
MODULES_VSIZE = SZ_128M
https://android.googlesource.com/kernel/common/+/refs/heads/android14-6.1/arch/arm64/include/asm/memory.h

Android 15 / 6.6
MODULES_VSIZE = SZ_2G
https://android.googlesource.com/kernel/common/+/refs/heads/android15-6.6/arch/arm64/include/asm/memory.h

Android 16 / 6.12
MODULES_VSIZE = SZ_2G
https://android.googlesource.com/kernel/common/+/refs/heads/android16-6.12/arch/arm64/include/asm/memory.h

也就是说，外部内核模块加载时所在的内存区域是每个版本的内核不一样
5系和6.1是128M不用看了符合B指令跳转范围

6.6处理内核模块源码路径
https://android.googlesource.com/kernel/common/+/refs/heads/android15-6.6/arch/arm64/kernel/module.c
module_alloc() 优先从 128M  区分配
if (module_direct_base) {
    p = __vmalloc_node_range(size, MODULE_ALIGN,module_direct_base, module_direct_base + SZ_128M,...);
}
如果失败，再从 2G PLT 区分配：
if (!p && module_plt_base) {
    p = __vmalloc_node_range(size, MODULE_ALIGN, module_plt_base,module_plt_base + SZ_2G,...);
}
模块里调用内核 API，编译后常见就是 bl symbol，对应:
R_AARCH64_CALL26
R_AARCH64_JUMP26
loader 先尝试直接把目标地址写进 26-bit branch immediate：

ovf = reloc_insn_imm(RELOC_OP_PREL, loc, val, 2, 26, AARCH64_INSN_IMM_26);
如果超出 ±128M：
if (ovf == -ERANGE) {
    val = module_emit_plt_entry(...);
    ...
    ovf = reloc_insn_imm(... loc, val, 2, 26, ...);
}
意思是：原本 bl 内核API 跳不到内核 API，就在模块自己的 .plt 里生成一个近处跳板，然后把 bl 改成跳这个 .plt entry。

PLT entry 在 arch/arm64/kernel/module-plts.c：

plt = __get_adrp_add_pair(dst, (u64)pc, AARCH64_INSN_REG_16);
plt.br = cpu_to_le32(br);
也就是类似：
adrp x16, target_page
add  x16, x16, target_pageoff
br   x16
*/

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
#define _KCALL_CLOBBERS "x9", "x10", "x11", "x12", "x13", "x14", "x15", "x16", "x17", "lr", "cc", "memory", "v0", "v1", "v2", "v3", "v4", "v5", "v6", "v7", "v16", "v17", "v18", "v19", "v20", "v21", "v22", "v23", "v24", "v25", "v26", "v27", "v28", "v29", "v30", "v31"

// 调用 0 个参数的函数
#define KCALL_0(fn_addr, ret_type)                                                                                                   \
    ({                                                                                                                               \
        register uint64_t _x0 asm("x0");                                                                                             \
        asm volatile("blr %1\n" : "=r"(_x0) : "r"((uint64_t)(fn_addr)) : "x1", "x2", "x3", "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                              \
    })

// 调用 1 个参数的函数
#define KCALL_1(fn_addr, ret_type, a1)                                                                                               \
    ({                                                                                                                               \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                            \
        asm volatile("blr %1\n" : "+r"(_x0) : "r"((uint64_t)(fn_addr)) : "x1", "x2", "x3", "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                              \
    })

// 调用 2 个参数的函数
#define KCALL_2(fn_addr, ret_type, a1, a2)                                                                                                \
    ({                                                                                                                                    \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                 \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                 \
        asm volatile("blr %2\n" : "+r"(_x0), "+r"(_x1) : "r"((uint64_t)(fn_addr)) : "x2", "x3", "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                   \
    })

// 调用 3 个参数的函数
#define KCALL_3(fn_addr, ret_type, a1, a2, a3)                                                                                                 \
    ({                                                                                                                                         \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                      \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                      \
        register uint64_t _x2 asm("x2") = (uint64_t)(a3);                                                                                      \
        asm volatile("blr %3\n" : "+r"(_x0), "+r"(_x1), "+r"(_x2) : "r"((uint64_t)(fn_addr)) : "x3", "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                        \
    })

// 调用 4 个参数的函数
#define KCALL_4(fn_addr, ret_type, a1, a2, a3, a4)                                                                                                  \
    ({                                                                                                                                              \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                           \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                           \
        register uint64_t _x2 asm("x2") = (uint64_t)(a3);                                                                                           \
        register uint64_t _x3 asm("x3") = (uint64_t)(a4);                                                                                           \
        asm volatile("blr %4\n" : "+r"(_x0), "+r"(_x1), "+r"(_x2), "+r"(_x3) : "r"((uint64_t)(fn_addr)) : "x4", "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                             \
    })

// 调用 5 个参数的函数
#define KCALL_5(fn_addr, ret_type, a1, a2, a3, a4, a5)                                                                                                   \
    ({                                                                                                                                                   \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                                \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                                \
        register uint64_t _x2 asm("x2") = (uint64_t)(a3);                                                                                                \
        register uint64_t _x3 asm("x3") = (uint64_t)(a4);                                                                                                \
        register uint64_t _x4 asm("x4") = (uint64_t)(a5);                                                                                                \
        asm volatile("blr %5\n" : "+r"(_x0), "+r"(_x1), "+r"(_x2), "+r"(_x3), "+r"(_x4) : "r"((uint64_t)(fn_addr)) : "x5", "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                                  \
    })

// 调用 6 个参数的函数
#define KCALL_6(fn_addr, ret_type, a1, a2, a3, a4, a5, a6)                                                                                                    \
    ({                                                                                                                                                        \
        register uint64_t _x0 asm("x0") = (uint64_t)(a1);                                                                                                     \
        register uint64_t _x1 asm("x1") = (uint64_t)(a2);                                                                                                     \
        register uint64_t _x2 asm("x2") = (uint64_t)(a3);                                                                                                     \
        register uint64_t _x3 asm("x3") = (uint64_t)(a4);                                                                                                     \
        register uint64_t _x4 asm("x4") = (uint64_t)(a5);                                                                                                     \
        register uint64_t _x5 asm("x5") = (uint64_t)(a6);                                                                                                     \
        asm volatile("blr %6\n" : "+r"(_x0), "+r"(_x1), "+r"(_x2), "+r"(_x3), "+r"(_x4), "+r"(_x5) : "r"((uint64_t)(fn_addr)) : "x6", "x7", _KCALL_CLOBBERS); \
        (ret_type) _x0;                                                                                                                                       \
    })
