
/*
使用了两种方案进行读写
    1.pte直接映射任意物理地址进行读写，设置页任意属性，任意写入，不区分设备内存和系统内存

(因为都是都是通过页表建立，虚拟地址→物理地址的映射。)
底层原理都是映射
    2.是用内核线性地址读写，只能操作系统内存

*/

#ifndef PHYSICAL_H
#define PHYSICAL_H
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
#include "export_fun.h"
#include "io_struct.h"

//============方案1:PTE读写+MMU硬件翻译地址============

struct pte_physical_page_info
{
    void *base_address;
    size_t size;
    pte_t *pte_address;
};
static struct pte_physical_page_info pte_info;

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

// 初始化
static inline int allocate_physical_page_info(void)
{
    uint64_t vaddr;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep;

    if (in_atomic())
    {
        pr_debug("原子上下文禁止调用 vmalloc\n");
        return -EPERM;
    }

    __builtin_memset(&pte_info, 0, sizeof(pte_info));

    // 分配内存
    vaddr = (uint64_t)vmalloc(PAGE_SIZE);
    if (!vaddr)
    {
        pr_debug("vmalloc 失败\n");
        return -ENOMEM;
    }

    // 必须 memset 触发缺页，让内核填充 TTBR1 指向的页表
    __builtin_memset((void *)vaddr, 0xAA, PAGE_SIZE);

    // --- 页表 Walk: PGD → P4D → PUD → PMD → PTE ---

    // 计算 PGD 索引 (pgd_offset_raw)
    pgd = get_kernel_pgd_base() + pgd_index(vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
    {
        pr_debug("PGD 无效\n");
        goto err_out;
    }

    // P4D
    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
    {
        pr_debug("P4D 无效\n");
        goto err_out;
    }

    // PUD
    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud))
    {
        pr_debug("PUD 无效\n");
        goto err_out;
    }

    // PMD
    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
    {
        pr_debug("PMD 无效\n");
        goto err_out;
    }

    // 大页检查
    if (pmd_leaf(*pmd))
    {
        pr_debug("遇到大页，无法获取 PTE\n");
        goto err_out;
    }

    // PTE
    ptep = pte_offset_kernel(pmd, vaddr);
    if (!ptep)
    {
        pr_debug("PTE 指针为空\n");
        goto err_out;
    }

    pte_info.base_address = (void *)vaddr;
    pte_info.size = PAGE_SIZE;
    pte_info.pte_address = ptep;
    return 0;

err_out:
    vfree((void *)vaddr);
    return -EFAULT;
}

// 释放
static inline void free_physical_page_info(void)
{
    if (pte_info.base_address)
    {
        // 释放之前通过 vmalloc 分配的虚拟内存
        vfree(pte_info.base_address);
        pte_info.base_address = NULL;
    }
}

// 验证参数并直接操作PTE建立物理页映射
static inline void *pte_map_page(phys_addr_t paddr, size_t size, const void *buffer)
{
    // 普通内存页表配置
    static const uint64_t FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF |
                                  PTE_SHARED | PTE_PXN | PTE_UXN |
                                  PTE_ATTRINDX(MT_NORMAL_NC);
    // // 硬件设备寄存器专用页表配置（不要使用硬件寄存器页表配置去读取普通物理页，原因不过多解释，太复杂了问AI去）
    // static const uint64_t FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF |
    //                               PTE_SHARED | PTE_PXN | PTE_UXN |
    //                               PTE_ATTRINDX(MT_DEVICE_nGnRnE);

    uint64_t pfn = __phys_to_pfn(paddr);

    // 参数检查
    if (unlikely(!size || !buffer))
        return ERR_PTR(-EINVAL);
    // PFN 有效性检查：确保物理页帧在系统内存管理范围内
    if (unlikely(!pfn_valid(pfn)))
        return ERR_PTR(-EFAULT);
    // 跨页检查：读写可能跨越页边界，访问到未映射的下一页
    if (unlikely(((paddr & ~PAGE_MASK) + size) > PAGE_SIZE))
        return ERR_PTR(-EINVAL);

    // 修改 PTE 指向目标物理页
    set_pte(pte_info.pte_address, pfn_pte(pfn, __pgprot(FLAGS)));

    // dsb(ishst);  // 内存全序屏障
    // 刷新该页的 TLB
    flush_tlb_kernel_range((uint64_t)pte_info.base_address, (uint64_t)pte_info.base_address + PAGE_SIZE);
    // flush_tlb_all();//刷新全部cpu核心TLB
    // isb(); // 刷新流水线，确保后续读取使用新的映射

    return (uint8_t *)pte_info.base_address + (paddr & ~PAGE_MASK);
}

// 读取
static inline int pte_read_physical(phys_addr_t paddr, void *buffer, size_t size)
{
    void *mapped = pte_map_page(paddr, size, buffer);
    if (IS_ERR(mapped))
    {
        return PTR_ERR(mapped);
    }

    // 极限性能且安全的内存拷贝 (防未对齐崩溃)
    switch (size)
    {
    case 1:
        __builtin_memcpy(buffer, mapped, 1);
        break;
    case 2:
        __builtin_memcpy(buffer, mapped, 2);
        break;
    case 4:
        __builtin_memcpy(buffer, mapped, 4);
        break;
    case 8:
        __builtin_memcpy(buffer, mapped, 8);
        break;
    default:
        __builtin_memcpy(buffer, mapped, size);
        break;
    }

    return 0;
}

// 写入
static inline int pte_write_physical(phys_addr_t paddr, const void *buffer, size_t size)
{
    void *mapped = pte_map_page(paddr, size, (void *)buffer);
    if (IS_ERR(mapped))
    {
        return PTR_ERR(mapped);
    }

    switch (size)
    {
    case 1:
        __builtin_memcpy(mapped, buffer, 1);
        break;
    case 2:
        __builtin_memcpy(mapped, buffer, 2);
        break;
    case 4:
        __builtin_memcpy(mapped, buffer, 4);
        break;
    case 8:
        __builtin_memcpy(mapped, buffer, 8);
        break;
    default:
        __builtin_memcpy(mapped, buffer, size);
        break;
    }

    return 0;
}

// 硬件mmu翻译
static inline int mmu_translate_va_to_pa(struct mm_struct *mm, u64 va, phys_addr_t *pa)
{
    u64 pgd_phys;
    int ret;
    u64 phys_out;
    u64 tmp_daif, tmp_ttbr, tmp_par, tmp_offset, tmp_ttbr_new;

    if (unlikely(!mm || !mm->pgd || !pa))
        return -EINVAL;

    pgd_phys = virt_to_phys(mm->pgd);

    asm volatile(

        // 关闭所有中断和异常中断
        "mrs    %[tmp_daif], daif\n"
        "msr    daifset, #0xf\n" // 关闭所有中断(D/A/I/F)
        "isb\n"

        /*
        6.12 内核：全面完善并默认启用了 LPA2 特性（支持 4K/16K 页面的 52 位物理地址）。
            如果系统开启了 LPA2，PAR_EL1 寄存器的格式会发生变化，物理地址可以长达 52 位。
            原有代码中的 ubfx %[tmp_par], %[tmp_par], #12, #36 强行将物理地址截断在了 48 位（提取 36 位 + 偏移 12 位 = 48 位）。
            就不能这么写了
            后续当你用这个被截断的错误物理地址去读写内存时，会触发同步外部中止 (Synchronous External Abort / SError)，引发极其底层的硬件级死机。
        准备新的 TTBR0 布局 (兼容 LPA2)
        如果 pgd_phys 超过 48 位 (LPA2 开启)，
        物理地址的 [51:48] 必须移动到寄存器的 [5:2] 位。
        如果没开启 LPA2，pgd_phys[51:48] 为 0，此逻辑依然安全（不影响结果）。
         */
        "lsr    %[tmp_ttbr_new], %[pgd_phys], #48\n"                       // 提取 PA[51:48]
        "and    %[tmp_offset], %[pgd_phys], #0xffffffffffff\n"             // 提取 PA[47:0]
        "orr    %[tmp_ttbr_new], %[tmp_offset], %[tmp_ttbr_new], lsl #2\n" // 组合到新 TTBR 格式

        // 切换 TTBR0，ASID 域清零 (bits[63:48]=0)
        "mrs    %[tmp_ttbr], ttbr0_el1\n"
        "msr    ttbr0_el1, %[tmp_ttbr_new]\n"
        "isb\n"

        // 硬件地址翻译
        "at     s1e0r, %[va]\n"
        "isb\n"
        "mrs    %[tmp_par], par_el1\n"

        /*清除 ASID=0 的 TLB 污染
        vaae1is: VA+所有ASID, EL1, 广播所有核
        虽然 AT 是本地触发，但在复杂的 SMP 环境下，使用 ish 是硬件流水线一致性的最稳妥做法。

备用指令只清理本地TLB
        "lsr    %[tmp_offset], %[va], #12\n"
        "tlbi   vae1, %[tmp_offset]\n"
        "dsb    nsh\n"
        "isb\n"

        */
        "lsr    %[tmp_offset], %[va], #12\n"
        "tlbi   vaae1is, %[tmp_offset]\n"
        "dsb    ish\n"
        "isb\n"

        // 恢复原始 TTBR0
        "msr    ttbr0_el1, %[tmp_ttbr]\n"
        "isb\n"

        // 恢复原始 DAIF 状态
        "msr    daif, %[tmp_daif]\n"
        "isb\n"

        // 检查翻译是否成功 (PAR_EL1.F == 0)
        "tbnz   %[tmp_par], #0, .L_efault%=\n"

        /*
         * 提取物理地址
         * PAR_EL1[51:12] 存放物理页地址。
         * 提取从 bit 12 开始的 40 位 (即到 bit 51)。
         */
        "ubfx   %[tmp_par], %[tmp_par], #12, #40\n" // 提取 PA[51:12]
        "lsl    %[tmp_par], %[tmp_par], #12\n"      // 恢复偏移
        "and    %[tmp_offset], %[va], #0xFFF\n"     // 提取页内偏移
        "orr    %[phys_out], %[tmp_par], %[tmp_offset]\n"
        "mov    %w[ret], #0\n"
        "b      .L_end%=\n"

        ".L_efault%=:\n"
        "mov    %w[ret], %w[efault_val]\n"
        "mov    %[phys_out], #0\n"

        ".L_end%=:\n"

        : [ret] "=&r"(ret),
          [phys_out] "=&r"(phys_out),
          [tmp_daif] "=&r"(tmp_daif),
          [tmp_ttbr] "=&r"(tmp_ttbr),
          [tmp_par] "=&r"(tmp_par),
          [tmp_offset] "=&r"(tmp_offset),
          [tmp_ttbr_new] "=&r"(tmp_ttbr_new)
        : [pgd_phys] "r"(pgd_phys),
          [va] "r"(va),
          [efault_val] "r"(-EFAULT)
        : "cc", "memory");

    if (ret == 0)
        *pa = phys_out;

    return ret;
}

//============方案2:内核已经映射的线性地址读写+手动走页表翻译地址============
// 读取
static inline int linear_read_physical(phys_addr_t paddr, void *buffer, size_t size)
{
    void *kernel_vaddr = phys_to_virt(paddr);

    // 下面这个先暂时不使用，靠翻译阶段得出绝对有效物理地址，死机请加上
    //  // 最后的安全底线：防算错物理地址/内存空洞导致死机
    //  if (unlikely(!virt_addr_valid(kernel_vaddr)))
    //  {
    //      return -EFAULT;
    //  }

    // 极限性能且安全的内存拷贝 (防未对齐崩溃)
    switch (size)
    {
    case 1:
        __builtin_memcpy(buffer, kernel_vaddr, 1);
        break;
    case 2:
        __builtin_memcpy(buffer, kernel_vaddr, 2);
        break;
    case 4:
        __builtin_memcpy(buffer, kernel_vaddr, 4);
        break;
    case 8:
        __builtin_memcpy(buffer, kernel_vaddr, 8);
        break;
    default:
        __builtin_memcpy(buffer, kernel_vaddr, size);
        break;
    }

    return 0;
}

// 写入
static inline int linear_write_physical(phys_addr_t paddr, void *buffer, size_t size)
{
    void *kernel_vaddr = phys_to_virt(paddr);

    // if (unlikely(!virt_addr_valid(kernel_vaddr)))
    // {
    //     return -EFAULT;
    // }

    // 极限性能且安全的内存拷贝 (防未对齐崩溃)
    switch (size)
    {
    case 1:
        __builtin_memcpy(kernel_vaddr, buffer, 1);
        break;
    case 2:
        __builtin_memcpy(kernel_vaddr, buffer, 2);
        break;
    case 4:
        __builtin_memcpy(kernel_vaddr, buffer, 4);
        break;
    case 8:
        __builtin_memcpy(kernel_vaddr, buffer, 8);
        break;
    default:
        __builtin_memcpy(kernel_vaddr, buffer, size);
        break;
    }

    return 0;
}

// 手动走页表翻译（不再禁止中断，靠每级安全检查防护）
static inline int walk_translate_va_to_pa(struct mm_struct *mm, uint64_t vaddr, phys_addr_t *paddr)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep, pte;
    unsigned long pfn;

    if (unlikely(!mm || !paddr))
        return -1;

    // PGD Level
    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
        return -1;

    // P4D Level
    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
        return -1;

    // PUD Level (可能遇到 1GB 大页)
    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud))
        return -1;

    // 检查是否是 1G 大页
    if (pud_leaf(*pud))
    {
        // 检查pfn
        pfn = pud_pfn(*pud);
        if (unlikely(!pfn_valid(pfn)))
            return -1;

        *paddr = (pud_pfn(*pud) << PAGE_SHIFT) + (vaddr & ~PUD_MASK);
        return 0;
    }
    if (pud_bad(*pud))
        return -1;

    //  PMD Level (可能遇到 2MB 大页)
    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd))
        return -1;

    // 检查是否是 2M 大页
    if (pmd_leaf(*pmd))
    {
        // 检查pfn
        pfn = pmd_pfn(*pmd);
        if (unlikely(!pfn_valid(pfn)))
            return -1;

        *paddr = (pmd_pfn(*pmd) << PAGE_SHIFT) + (vaddr & ~PMD_MASK);
        return 0;
    }
    if (pmd_bad(*pmd))
        return -1;

    //  PTE Level (普通的 4KB 页)
    // 较新内核中 __pte_offset_map 不导出，对于 64位 系统直接使用 pte_offset_kernel 即可
    ptep = pte_offset_kernel(pmd, vaddr);
    if (unlikely(!ptep))
        return -1;

    pte = *ptep;

    // 必须检查 pte_present，因为页可能被换出到 Swap 分区
    // 如果 present 为 false，pfn 字段是无效的（存的是 swap offset）
    if (pte_present(pte))
    {
        // 检查pfn
        pfn = pte_pfn(pte);
        if (unlikely(!pfn_valid(pfn)))
            return -1;

        *paddr = (pte_pfn(pte) << PAGE_SHIFT) + (vaddr & ~PAGE_MASK);
        return 0;
    }

    return -1;
}

// 进程读写
static inline int _process_memory_rw(enum sm_req_op op, pid_t pid, uint64_t vaddr, void *buffer, size_t size)
{
    static pid_t s_last_pid = 0;
    static struct mm_struct *s_last_mm = NULL;
    static uint64_t s_last_vpage_base = -1ULL;
    static phys_addr_t s_last_ppage_base = 0;

    phys_addr_t paddr_of_page = 0;
    uint64_t current_vaddr = vaddr;
    size_t bytes_remaining = size;
    size_t bytes_copied = 0;
    size_t bytes_done = 0;
    int status = 0;

    if (unlikely(!buffer || size == 0))
        return -EINVAL;

    /* ---------- mm_struct 缓存 ---------- */
    if (unlikely(pid != s_last_pid || s_last_mm == NULL))
    {
        struct pid *pid_struct;
        struct task_struct *task;

        if (s_last_mm)
        {
            mmput(s_last_mm); // 引用计数-1
            s_last_mm = 0;
        }

        pid_struct = find_get_pid(pid);
        if (!pid_struct)
            return -ESRCH;

        task = get_pid_task(pid_struct, PIDTYPE_PID);
        put_pid(pid_struct);
        if (!task)
            return -ESRCH;

        s_last_mm = get_task_mm(task); // 引用计数+1
        put_task_struct(task);

        if (!s_last_mm)
            return -EINVAL;

        s_last_pid = pid;
        s_last_vpage_base = -1ULL;
    }

    /* ---------- 逐页循环 ---------- */
    while (bytes_remaining > 0)
    {
        size_t page_offset = current_vaddr & (PAGE_SIZE - 1);
        size_t bytes_this_page = PAGE_SIZE - page_offset;
        uint64_t current_vpn = current_vaddr & PAGE_MASK;

        if (bytes_this_page > bytes_remaining)
            bytes_this_page = bytes_remaining;

        /* 软件 TLB 缓存 */
        if (current_vpn == s_last_vpage_base)
        {
            paddr_of_page = s_last_ppage_base;
        }
        else
        {
            // 翻译地址
            status = mmu_translate_va_to_pa(s_last_mm, current_vpn, &paddr_of_page);
            // status = walk_translate_va_to_pa(s_last_mm, current_vpn, &paddr_of_page);

            if (unlikely(status != 0))
            {
                s_last_vpage_base = -1ULL;
                if (op == op_r)
                    __builtin_memset((uint8_t *)buffer + bytes_copied, 0, bytes_this_page);
                goto next_chunk;
            }
            s_last_vpage_base = current_vpn;
            s_last_ppage_base = paddr_of_page;
        }

        /* 执行读/写 */
        if (op == op_r)
        {

            status = pte_read_physical(paddr_of_page + page_offset, (uint8_t *)buffer + bytes_copied, bytes_this_page);
            // status = linear_read_physical(paddr_of_page + page_offset, (uint8_t *)buffer + bytes_copied, bytes_this_page);
        }
        else
        {

            status = pte_write_physical(paddr_of_page + page_offset, (const uint8_t *)buffer + bytes_copied, bytes_this_page);
            // status = linear_write_physical(paddr_of_page + page_offset, (uint8_t *)buffer + bytes_copied, bytes_this_page);
        }

        if (unlikely(status != 0))
        {
            s_last_vpage_base = -1ULL;
            if (op == op_r)
                __builtin_memset((uint8_t *)buffer + bytes_copied, 0, bytes_this_page);
            goto next_chunk;
        }

        bytes_done += bytes_this_page;

    next_chunk:
        bytes_remaining -= bytes_this_page;
        bytes_copied += bytes_this_page;
        current_vaddr += bytes_this_page;
    }

    return (bytes_done == 0) ? -EFAULT : (int)bytes_done;
}

/* ---------- 对外接口 ---------- */
static inline int read_process_memory(pid_t pid, uint64_t vaddr, void *buffer, size_t size)
{
    return _process_memory_rw(op_r, pid, vaddr, buffer, size);
}
static inline int write_process_memory(pid_t pid, uint64_t vaddr, void *buffer, size_t size)
{
    return _process_memory_rw(op_w, pid, vaddr, buffer, size);
}

#endif // PHYSICAL_H
