
/*
使用了两种方案进行读写
    1.pte直接映射任意物理地址进行读写，设置页任意属性，任意写入，不区分设备内存和系统内存

(因为都是都是通过页表建立，虚拟地址→物理地址的映射。)
底层原理都是映射
    2.是用内核线性地址读写，只能操作系统内存


(翻译和读写可以混搭)

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

//============方案1:PTE读写+MMU硬件翻译地址(翻译和读写可以混搭)============

struct pte_physical_page_info
{
    void *base_address;
    size_t size;
    pte_t *pte_address;
};
static struct pte_physical_page_info pte_info;

// 初始化
static inline int allocate_physical_page_info(void)
{
    uint64_t vaddr;
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

    // 获取内核地址对应的 PTE 指针
    ptep = get_kernel_pte(vaddr);
    if (!ptep)
    {
        pr_debug("获取 PTE 失败\n");
        goto err_out;
    }

    pte_info.base_address = (void *)vaddr;
    pte_info.size = PAGE_SIZE;
    pte_info.pte_address = ptep;
    return 0;

err_out:
    vfree((void *)vaddr);
    __builtin_memset(&pte_info, 0, sizeof(pte_info));
    return -EFAULT;
}
// 释放
static inline void free_physical_page_info(void)
{
    if (pte_info.base_address)
    {
        // 释放之前通过 vmalloc 分配的虚拟内存
        vfree(pte_info.base_address);
        __builtin_memset(&pte_info, 0, sizeof(pte_info));
    }
}

// 验证参数并直接操作PTE建立物理页映射
static inline void *pte_map_page(phys_addr_t paddr, size_t size, const void *buffer)
{
    // 普通内存页表配置
    /*
    我建议使用MT_NORMAL(有缓存)，RAM是口语化表达广泛含义表内存，DRAM是内存硬件具体的硬件存储介质
    原因如下:
        一块普通的DRAM物理地址同时被2个或以上的虚拟地址进行了不同属性的映射
        类如:用户态虚拟地址映射这个物理页为有缓存,内核线性区映射这个物理页有缓存，这里却映射为无缓存
        虽然说3种都能访问，但是会出现数据不一致的情况
    1.映射为有缓存的用户态和线性:对地址写入很多时候还存在CPU cache(多级缓存中,常见的如L1~L3级缓存)
                                这时候进行绕过缓存读DRAM中数据，肯定是错乱的，应为cpu未把缓存写回DRAM
    2.映射为无缓存的内核态:你对这个物理页的读写都是直达DRAM,此时cpu拿缓存进行计算，修改DRMA不会实时反映到虚拟地址
    */
    static const uint64_t FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF |
                                  PTE_SHARED | PTE_PXN | PTE_UXN |
                                  PTE_ATTRINDX(MT_NORMAL);
    // // 硬件设备寄存器专用页表配置（不要使用硬件寄存器页表配置去读取普通物理页，原因不过多解释，太复杂了问AI去）
    // static const uint64_t FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF |
    //                               PTE_SHARED | PTE_PXN | PTE_UXN |
    //                               PTE_ATTRINDX(MT_DEVICE_nGnRnE);

    uint64_t pfn = __phys_to_pfn(paddr);

    // 参数检查
    if (!size || !buffer)
        return ERR_PTR(-EINVAL);
    // PFN 有效性检查：确保物理页帧在系统内存管理范围内
    if (!pfn_valid(pfn))
        return ERR_PTR(-EFAULT);
    // 跨页检查：读写可能跨越页边界，访问到未映射的下一页
    if (((paddr & ~PAGE_MASK) + size) > PAGE_SIZE)
        return ERR_PTR(-EINVAL);

    // 修改 PTE 指向目标物理页
    set_pte(pte_info.pte_address, pfn_pte(pfn, __pgprot(FLAGS)));

    // dsb(ishst); // 内存屏障(ishst只等写完成，ish等读写完成)，确保PTE写入提交内存

    // 刷新该页的 TLB, 内部含：dsb(ish) + TLBI + dsb(ish)+isb(),手写刷新需取消dsbisb注释
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
static inline int mmu_translate_va_to_pa(struct mm_struct *mm, uint64_t va, phys_addr_t *pa)
{
    uint64_t pgd_phys;
    int ret;
    uint64_t phys_out;
    uint64_t tmp_daif, tmp_ttbr, tmp_par, tmp_offset, tmp_ttbr_new;

    if (!mm || !mm->pgd || !pa)
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

        // 切换 TTBR0
        "mrs    %[tmp_ttbr], ttbr0_el1\n"
        "msr    ttbr0_el1, %[tmp_ttbr_new]\n"
        "isb\n"

        /*
        硬件地址翻译，这里会导致某个TLB entry(TLB条目)的 ASID(地址空间标识符) 中VA->PA 的被污染
        ASID允许相同虚拟地址映射不同物理地址，不同进程的地址空间分配不同的ASID到mm,运行时根据TCR_EL1.A1装载到ttbr0_el1或TTBR1_EL1
        TLB entry 1: ASID 10 + VA 0x400000 -> PA 0x10000000
        TLB entry 2: ASID 20 + VA 0x400000 -> PA 0x20000000
        */
        "at     s1e0r, %[va]\n"
        "isb\n"
        "mrs    %[tmp_par], par_el1\n"

        /*
        只清除当前va地址所有的ASID并只同步当前cpu，不用vae1清除指定ASID原因是不知道 AT 这次污染在哪个 ASID
        想要知道需要如下判断，太麻烦了
        TCR_EL1.A1 = 0  => ASID 来自 TTBR0_EL1[63:48]
        TCR_EL1.A1 = 1  => ASID 来自 TTBR1_EL1[63:48]
        */
        "lsr    %[tmp_offset], %[va], #12\n" // 清除当前va地址
        "tlbi   vaae1, %[tmp_offset]\n"      // 所有的ASID
        "dsb    nsh\n"                       // 并只同步当前cpu
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
        提取物理地址
        PAR_EL1[51:12] 存放物理页地址。
        提取从 bit 12 开始的 40 位 (即到 bit 51)。
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

//============方案2:内核已经映射的线性地址读写+手动走页表翻译地址(翻译和读写可以混搭)============
// 读取
static inline int linear_read_physical(phys_addr_t paddr, void *buffer, size_t size)
{
    void *kernel_vaddr = phys_to_virt(paddr);

    // 下面这个先暂时不使用，靠翻译阶段得出绝对有效物理地址，死机请加上
    //  // 最后的安全底线：防算错物理地址/内存空洞导致死机
    //  if (!virt_addr_valid(kernel_vaddr))
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

    // if (!virt_addr_valid(kernel_vaddr))
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
    pte_t *ptep, pte;
    unsigned long pfn;

    if (!paddr)
        return -1;
    // 获取pte指针
    ptep = get_user_pte(mm, vaddr);
    if (!ptep)
        return -1;
    pte = *ptep;
    // 必须检查 pte_present，因为页可能被换出到 Swap 分区
    // 如果 present 为 false，pfn 字段是无效的（存的是 swap offset）
    if (!pte_present(pte))
        return -1;

    // 检查 pfn
    pfn = pte_pfn(pte);
    if (!pfn_valid(pfn))
        return -1;

    *paddr = (pfn << PAGE_SHIFT) + (vaddr & ~PAGE_MASK);
    return 0;
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

    if (!buffer || size == 0)
        return -EINVAL;

    /* ---------- mm_struct 缓存 ---------- */
    if (pid != s_last_pid || s_last_mm == NULL)
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

            if (status != 0)
            {
                s_last_vpage_base = -1ULL;
                if (op == op_r && size > 8)
                    __builtin_memset((uint8_t *)buffer + bytes_copied, 0, bytes_this_page);
                goto next_chunk;
            }
            s_last_vpage_base = current_vpn;
            s_last_ppage_base = paddr_of_page;
        }

        /* 执行读/写 */
        if (op == op_r)
        {

            // status = pte_read_physical(paddr_of_page + page_offset, (uint8_t *)buffer + bytes_copied, bytes_this_page);
            status = linear_read_physical(paddr_of_page + page_offset, (uint8_t *)buffer + bytes_copied, bytes_this_page);
        }
        else
        {

            // status = pte_write_physical(paddr_of_page + page_offset, (const uint8_t *)buffer + bytes_copied, bytes_this_page);
            status = linear_write_physical(paddr_of_page + page_offset, (uint8_t *)buffer + bytes_copied, bytes_this_page);
        }

        if (status != 0)
        {
            s_last_vpage_base = -1ULL;
            if (op == op_r && size > 8)
                __builtin_memset((uint8_t *)buffer + bytes_copied, 0, bytes_this_page);
            goto next_chunk;
        }

        bytes_done += bytes_this_page;

    next_chunk:
        bytes_remaining -= bytes_this_page;
        bytes_copied += bytes_this_page;
        current_vaddr += bytes_this_page;
    }

    return (bytes_done == 0) ? status : (int)bytes_done;
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
