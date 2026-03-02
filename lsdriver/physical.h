
/*
使用了两种方案进行读写
    1.pte直接映射任意物理地址进行读写，设置页任意属性，任意写入，不区分设备内存和系统内存
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
#include "ExportFun.h"

// ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓方案1:PTE读写 ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓
struct physical_page_info
{
    void *base_address;
    size_t size;
    pte_t *pte_address;
};
static struct physical_page_info info;

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

    memset(&info, 0, sizeof(info));

    // 分配内存
    vaddr = (uint64_t)vmalloc(PAGE_SIZE);
    if (!vaddr)
    {
        pr_debug("vmalloc 失败\n");
        return -ENOMEM;
    }

    // 必须 memset 触发缺页，让内核填充 TTBR1 指向的页表
    memset((void *)vaddr, 0xAA, PAGE_SIZE);

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
        pr_debug("遇到大页 (Block Mapping)，无法获取 PTE\n");
        goto err_out;
    }

    // PTE
    ptep = pte_offset_kernel(pmd, vaddr);
    if (!ptep)
    {
        pr_debug("PTE 指针为空\n");
        goto err_out;
    }

    info.base_address = (void *)vaddr;
    info.size = PAGE_SIZE;
    info.pte_address = ptep;
    return 0;

err_out:
    vfree((void *)vaddr);
    return -EFAULT;
}

// 释放
static inline void free_physical_page_info(void)
{
    if (info.base_address)
    {
        // 释放之前通过 vmalloc 分配的虚拟内存
        vfree(info.base_address);
        info.base_address = NULL;
    }
}

// 通过直接操作PTE，从指定的任意物理地址读取数据。
static inline void _internal_read_fast(phys_addr_t paddr, void *buffer, size_t size)
{
    // MT_NORMAL_NC无缓存只读(建议用缓存MT_NORMAL因为cpu来不及把进程数据缓存写入内存，就直接读物理会有意外如：在1000000次测试下发现数据不匹配就是应为缓存没及时写入内存)
    static const uint64_t FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF | PTE_SHARED | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_NORMAL);

    uint64_t pfn;
    if (unlikely(!size || !buffer))
        return;

    pfn = __phys_to_pfn(paddr);

    // PFN 有效性检查：确保物理页帧在系统内存管理范围内
    if (unlikely(!pfn_valid(pfn)))
        return;

    // 直接修改 PTE 指向目标物理页
    set_pte(info.pte_address, pfn_pte(pfn, __pgprot(FLAGS)));

    // 内存全序屏障
    // dsb(ishst);

    // 刷新 TLB (只刷新单个页);
    flush_tlb_kernel_range((uint64_t)info.base_address, (uint64_t)info.base_address + PAGE_SIZE);
    // flush_tlb_all();//刷新全部cpu核心TLB
    // isb(); // 刷新流水线，确保后续读取使用新的映射

    /*
    拷贝数据(这里没用likely检查字节对齐极端情况会导致：)
    1.地址正好跨越了缓存行的边界（比如一个 float 一半在 Cache Line A，一半在 Cache Line B），CPU 必须发起两次总线事务。
        在这两次读取的间隙，用户态程序正好改写了这个值。你读到的就是“前半截是新值，后半截是旧值”的撕裂数据
    2.使用 __attribute__((packed)) 的结构体强行紧凑布局和不对齐导致
    */
    switch (size)
    {
    case 4:
    {
        *(uint32_t *)buffer = READ_ONCE(*(volatile uint32_t *)(info.base_address + (paddr & ~PAGE_MASK)));
        break;
    }
    case 8:
    {
        *(uint64_t *)buffer = READ_ONCE(*(volatile uint64_t *)(info.base_address + (paddr & ~PAGE_MASK)));
        break;
    }
    case 1:
    {
        *(uint8_t *)buffer = READ_ONCE(*(volatile uint8_t *)(info.base_address + (paddr & ~PAGE_MASK)));
        break;
    }
    case 2:
    {
        *(uint16_t *)buffer = READ_ONCE(*(volatile uint16_t *)(info.base_address + (paddr & ~PAGE_MASK)));
        break;
    }
    default:
        memcpy(buffer, (char *)info.base_address + (paddr & ~PAGE_MASK), size);
        break;
    }
}
static inline void _internal_write_fast(phys_addr_t paddr, const void *buffer, size_t size)
{
    static const uint64_t FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF | PTE_SHARED | PTE_WRITE | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_NORMAL);
    uint64_t pfn;
    if (unlikely(!size || !buffer))
        return;

    pfn = __phys_to_pfn(paddr);

    if (unlikely(!pfn_valid(pfn)))
        return;

    set_pte(info.pte_address, pfn_pte(pfn, __pgprot(FLAGS)));
    flush_tlb_kernel_range((uint64_t)info.base_address, (uint64_t)info.base_address + PAGE_SIZE);

    switch (size)
    {
    case 4:
    {
        WRITE_ONCE(*(volatile uint32_t *)(info.base_address + (paddr & ~PAGE_MASK)), *(const uint32_t *)buffer);
        break;
    }
    case 8:
    {
        WRITE_ONCE(*(volatile uint64_t *)(info.base_address + (paddr & ~PAGE_MASK)), *(const uint64_t *)buffer);
        break;
    }
    case 1:
    {
        WRITE_ONCE(*(volatile uint8_t *)(info.base_address + (paddr & ~PAGE_MASK)), *(const uint8_t *)buffer);
        break;
    }
    case 2:
    {
        WRITE_ONCE(*(volatile uint16_t *)(info.base_address + (paddr & ~PAGE_MASK)), *(const uint16_t *)buffer);
        break;
    }
    default:
        memcpy((char *)info.base_address + (paddr & ~PAGE_MASK), buffer, size);
        break;
    }
}

// ↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓方案2:内核已经映射的线性地址读写↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓↓
static inline int _internal_read_fast_linear(phys_addr_t paddr, void *buffer, size_t size)
{

    // 直接数学转换 在 ARM64 上，这通常等价于: (void*)(paddr + PAGE_OFFSET)
    void *kernel_vaddr = phys_to_virt(paddr);

    /*
    这里去掉了 virt_addr_valid。在 ARM64 上，它由于要遍历内存节点，耗极高。
    在查页表时统一拦截：把安全校验提前到了 manual_va_to_pa_arm_fast（查页表）阶段，利用 pfn_valid(pfn) 确认物理页是安全的普通内存
    只要查页表时确认这个 PFN 是合法的物理内存就有对应的线性虚拟地址
    */

    switch (size)
    {
    case 4:
        *(uint32_t *)buffer = READ_ONCE(*(volatile uint32_t *)kernel_vaddr);
        break;
    case 8:
        *(uint64_t *)buffer = READ_ONCE(*(volatile uint64_t *)kernel_vaddr);
        break;
    case 1:
        *(uint8_t *)buffer = READ_ONCE(*(volatile uint8_t *)kernel_vaddr);
        break;
    case 2:
        *(uint16_t *)buffer = READ_ONCE(*(volatile uint16_t *)kernel_vaddr);
        break;
    default:
        // 大块内存拷贝
        memcpy(buffer, kernel_vaddr, size);
        break;
    }
    return 0;
}
static inline int _internal_write_fast_linear(phys_addr_t paddr, void *buffer, size_t size)
{

    void *kernel_vaddr = phys_to_virt(paddr);

    // 写入操作
    switch (size)
    {
    case 4:
        WRITE_ONCE(*(volatile uint32_t *)kernel_vaddr, *(const uint32_t *)buffer);
        break;
    case 8:
        WRITE_ONCE(*(volatile uint64_t *)kernel_vaddr, *(const uint64_t *)buffer);
        break;
    case 1:
        WRITE_ONCE(*(volatile uint8_t *)kernel_vaddr, *(const uint8_t *)buffer);
        break;
    case 2:
        WRITE_ONCE(*(volatile uint16_t *)kernel_vaddr, *(const uint16_t *)buffer);
        break;
    default:
        memcpy(kernel_vaddr, buffer, size);
        break;
    }
    return 0;
}

// 只负责走页表（不再禁止中断，靠每级安全检查防护）
static inline int manual_va_to_pa_arm_fast(struct mm_struct *mm, uint64_t vaddr, phys_addr_t *paddr)
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
    ptep = pte_offset_map(pmd, vaddr);
    if (unlikely(!ptep))
        return -1;

    pte = *ptep;     // 原子读取 PTE 内容到栈上
    pte_unmap(ptep); // 立即释放映射

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

static inline int read_process_memory(pid_t pid, uint64_t vaddr, void *buffer, size_t size)
{
    // 使用 static 缓存 mm
    static pid_t s_last_pid = 0;
    static struct mm_struct *s_last_mm = NULL;

    // 使用 static 缓存页表映射结果
    static uint64_t loop_last_vpage_base = -1;
    static phys_addr_t loop_last_ppage_base = 0;

    phys_addr_t paddr_of_page = 0;
    uint64_t current_vaddr = vaddr;
    size_t bytes_remaining = size;
    size_t bytes_copied = 0;
    size_t bytes_real_read = 0; // 实际成功读取的字节数
    int status = 0;

    if (unlikely(!buffer || size == 0))
        return -EINVAL;

    // 检查 PID 是否改变，虽然目标进程退出了会不释放mm，不过没关系下次调用时会释放
    if (unlikely(pid != s_last_pid || s_last_mm == NULL))
    {
        struct pid *pid_struct = NULL;
        struct task_struct *task = NULL;

        // 释放旧的 mm
        if (s_last_mm)
        {
            mmput(s_last_mm); // 引用计数 -1
            s_last_mm = NULL;
        }

        // 查找新进程
        pid_struct = find_get_pid(pid);
        if (!pid_struct)
            return -ESRCH;

        task = get_pid_task(pid_struct, PIDTYPE_PID);
        put_pid(pid_struct);
        if (!task)
            return -ESRCH;

        s_last_mm = get_task_mm(task); // 引用计数 +1
        put_task_struct(task);

        if (!s_last_mm)
            return -EINVAL;

        s_last_pid = pid;

        //  切换进程后，必须作废上一个进程的地址缓存！
        loop_last_vpage_base = -1;
    }

    while (bytes_remaining > 0)
    {
        size_t page_offset = current_vaddr & (PAGE_SIZE - 1);
        size_t bytes_to_read_this_page = PAGE_SIZE - page_offset;
        uint64_t current_vpn = current_vaddr & PAGE_MASK;

        if (bytes_to_read_this_page > bytes_remaining)
            bytes_to_read_this_page = bytes_remaining;

        // 软件 TLB 优化
        if (current_vpn == loop_last_vpage_base)
        {
            paddr_of_page = loop_last_ppage_base;
        }
        else
        {
            // 查找页表
            status = manual_va_to_pa_arm_fast(s_last_mm, current_vpn, &paddr_of_page);
            if (unlikely(status != 0))
            {
                // 页表缺失或被 Swap，填0跳过这一段
                memset((char *)buffer + bytes_copied, 0, bytes_to_read_this_page);
                loop_last_vpage_base = -1;
                goto next_chunk;
            }

            // 更新缓存
            loop_last_vpage_base = current_vpn;
            loop_last_ppage_base = paddr_of_page;
        }

        // 执行物理内存读取
        status = _internal_read_fast_linear(paddr_of_page + page_offset, (char *)buffer + bytes_copied, bytes_to_read_this_page);

        if (unlikely(status != 0))
        {
            // 物理读取失败，填0跳过这一段
            memset((char *)buffer + bytes_copied, 0, bytes_to_read_this_page);
            loop_last_vpage_base = -1;
            goto next_chunk;
        }

        // 只有成功才计入实际读取量
        bytes_real_read += bytes_to_read_this_page;

    next_chunk:
        bytes_remaining -= bytes_to_read_this_page;
        bytes_copied += bytes_to_read_this_page;
        current_vaddr += bytes_to_read_this_page;
    }

    // 实际读取字节数为0才返回失败
    if (bytes_real_read == 0)
        return -EFAULT;

    return 0;
}

static inline int write_process_memory(pid_t pid, uint64_t vaddr, void *buffer, size_t size)
{
    // 使用 static 缓存 mm
    static pid_t s_last_pid = 0;
    static struct mm_struct *s_last_mm = NULL;

    // 使用 static 缓存页表映射结果
    static uint64_t loop_last_vpage_base = -1;
    static phys_addr_t loop_last_ppage_base = 0;

    phys_addr_t paddr_of_page = 0;
    uint64_t current_vaddr = vaddr;
    size_t bytes_remaining = size;
    size_t bytes_copied = 0;
    size_t bytes_real_write = 0; // 实际成功写入的字节数
    int status = 0;

    if (unlikely(!buffer || size == 0))
        return -EINVAL;

    // 检查 PID 是否改变，虽然目标进程退出了会不释放mm，不过没关系下次调用时会释放
    if (unlikely(pid != s_last_pid || s_last_mm == NULL))
    {
        struct pid *pid_struct = NULL;
        struct task_struct *task = NULL;

        // 释放旧的 mm
        if (s_last_mm)
        {
            mmput(s_last_mm); // 引用计数 -1
            s_last_mm = NULL;
        }

        // 查找新进程
        pid_struct = find_get_pid(pid);
        if (!pid_struct)
            return -ESRCH;

        task = get_pid_task(pid_struct, PIDTYPE_PID);
        put_pid(pid_struct);
        if (!task)
            return -ESRCH;

        s_last_mm = get_task_mm(task); // 引用计数 +1
        put_task_struct(task);

        if (!s_last_mm)
            return -EINVAL;

        s_last_pid = pid;

        //  切换进程后，必须作废上一个进程的地址缓存！
        loop_last_vpage_base = -1;
    }

    while (bytes_remaining > 0)
    {
        size_t page_offset = current_vaddr & (PAGE_SIZE - 1);
        size_t bytes_to_read_this_page = PAGE_SIZE - page_offset;
        uint64_t current_vpn = current_vaddr & PAGE_MASK;

        if (bytes_to_read_this_page > bytes_remaining)
            bytes_to_read_this_page = bytes_remaining;

        // 软件 TLB 优化
        if (current_vpn == loop_last_vpage_base)
        {
            paddr_of_page = loop_last_ppage_base;
        }
        else
        {
            // 查找页表
            status = manual_va_to_pa_arm_fast(s_last_mm, current_vpn, &paddr_of_page);
            if (unlikely(status != 0))
            {
                // 页表缺失或被 Swap，跳过这一段
                loop_last_vpage_base = -1;
                goto next_chunk;
            }

            // 更新缓存
            loop_last_vpage_base = current_vpn;
            loop_last_ppage_base = paddr_of_page;
        }

        // 执行物理内存写入
        status = _internal_write_fast_linear(paddr_of_page + page_offset, (char *)buffer + bytes_copied, bytes_to_read_this_page);
        if (unlikely(status != 0))
        {
            // 物理写入失败，跳过这一段
            loop_last_vpage_base = -1;
            goto next_chunk;
        }

        // 只有成功才计入实际写入量
        bytes_real_write += bytes_to_read_this_page;

    next_chunk:
        bytes_remaining -= bytes_to_read_this_page;
        bytes_copied += bytes_to_read_this_page;
        current_vaddr += bytes_to_read_this_page;
    }

    // 实际写入字节数为0才返回失败
    if (bytes_real_write == 0)
        return -EFAULT;

    return 0;
}

/*
 maps 文件
r--p (只读) 段:
7583e30000
7600a50000
r-xp (可执行) 段:
7600ef1000
760277c000
rw-p (读写) 段:
76025d4000
760264a000
7602780000
7602784000
Modifier's View :
[0] -> 7600ef1000 (第一个 r-xp)
[1] -> 760277c000 (第二个 r-xp)
[2] -> 7583e30000 (第一个 r--p)
[3] -> 7600a50000 (第二个 r--p)
[4] -> 76025d4000 (第一个 rw-p)
[5] -> 760264a000 (第二个 rw-p)
[6] -> 7602780000 (第三个 rw-p)
[7] -> 7602784000 (第四个 rw-p)
规则如下：
优先级分组 (Priority Grouping): 将所有内存段按权限分为三组，并按固定的优先级顺序排列它们。
最高优先级: r-xp (可执行)
中等优先级: r--p (只读)
最低优先级: rw-p (可读写)
组内排序 : 在每一个权限组内部，所有的段都严格按照内存地址从低到高进行排序。
展平为最终列表 : 将这三个排好序的组按优先级顺序拼接成一个大的虚拟列表，然后呈现。
先放所有排好序的 r-xp 段。
然后紧接着放所有排好序的 r--p 段。
最后放所有排好序的 rw-p 段。

*/

// 版本兼容
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)
// 内核 >= 6.1 使用 VMA 迭代器
#define DECLARE_VMA_ITER() struct vma_iterator vmi
#define INIT_VMA_ITER(mm) vma_iter_init(&vmi, mm, 0)
#define FOR_EACH_VMA_UNIFIED(vma) for_each_vma(vmi, vma)
#else
// 内核 < 6.1 使用传统链表
#define DECLARE_VMA_ITER()
#define INIT_VMA_ITER(mm) \
    do                    \
    {                     \
    } while (0)
#define FOR_EACH_VMA_UNIFIED(vma) for (vma = mm->mmap; vma; vma = vma->vm_next)
#endif

// VMA 权限检查宏
#define VMA_PERM_MASK (VM_READ | VM_WRITE | VM_EXEC)
#define VMA_IS_RX(vma) (((vma)->vm_flags & VMA_PERM_MASK) == (VM_READ | VM_EXEC))  // r-x
#define VMA_IS_RO(vma) (((vma)->vm_flags & VMA_PERM_MASK) == VM_READ)              // r--
#define VMA_IS_RW(vma) (((vma)->vm_flags & VMA_PERM_MASK) == (VM_READ | VM_WRITE)) // rw-

#define VMA_IS_RWP(vma) (VMA_IS_RW(vma) && !((vma)->vm_flags & VM_SHARED)) // rw-p (私有)

#define MAX_MODULES 512
#define MAX_SCAN_REGIONS 4096

#define MOD_NAME_LEN 256
#define MAX_SEGS_PER_MODULE 256

struct segment_info
{
    short index;  // >=0: 普通段(RX→RO→RW连续编号), -1: BSS段
    uint8_t prot; // 区段权限: 1(R), 2(W), 4(X)。例如 RX 就是 5 (1+4)
    uint64_t start;
    uint64_t end;
};

struct module_info
{
    char name[MOD_NAME_LEN];
    int seg_count;
    struct segment_info segs[MAX_SEGS_PER_MODULE];
};

struct region_info
{
    uint64_t start;
    uint64_t end;
};

struct memory_info
{

    int module_count;                        // 总模块数量
    struct module_info modules[MAX_MODULES]; // 模块信息

    int region_count;                             // 总可扫描内存数量
    struct region_info regions[MAX_SCAN_REGIONS]; // 可扫描内存区域 (rw-p, 排除特殊区域)
};

static int find_or_add_module(struct module_info *modules, int *module_count, const char *name)
{
    int i;
    for (i = 0; i < *module_count; i++)
        if (strcmp(modules[i].name, name) == 0)
            return i;
    if (*module_count >= MAX_MODULES)
        return -1;
    i = (*module_count)++;
    strscpy(modules[i].name, name, MOD_NAME_LEN);
    modules[i].seg_count = 0;
    return i;
}

static void add_seg(struct module_info *m, short type_tag, uint8_t prot, uint64_t start, uint64_t end)
{
    if (m->seg_count >= MAX_SEGS_PER_MODULE)
        return;
    m->segs[m->seg_count].index = type_tag;
    m->segs[m->seg_count].prot = prot;
    m->segs[m->seg_count].start = start;
    m->segs[m->seg_count].end = end;
    m->seg_count++;
}

// 排序: RX(0) → RO(1) → RW(2) → BSS(-1→3), 同类型按地址升序,BSS实际值是-1，排序时当3处理
static int cmp_seg(const void *a, const void *b)
{
    const struct segment_info *sa = a, *sb = b;
    int ta = sa->index == -1 ? 3 : sa->index;
    int tb = sb->index == -1 ? 3 : sb->index;
    if (ta != tb)
        return ta - tb;
    return (sa->start > sb->start) - (sa->start < sb->start);
}

static int enum_process_memory(pid_t pid, struct memory_info *info)
{
    struct task_struct *task = NULL;
    struct mm_struct *mm = NULL;
    struct vm_area_struct *vma, *prev = NULL;
    char *path_buf, *path, *prev_path;
    int idx, i, j;
    short seq;
    bool excluded, mod_accepted;

    // 模块白名单: 只收集这些前缀下的模块
    static const char *const mod_include_prefixes[] = {
        "/data/", NULL};

    // 扫描区域排除列表
    static const char *const excl_prefixes[] = {
        "/dev/", "/system/", "/vendor/", "/apex/", NULL};
    static const char *const excl_keywords[] = {
        ".oat", ".art", ".odex", ".vdex", ".dex", ".ttf",
        "dalvik", "gralloc", "ashmem", NULL};

    DECLARE_VMA_ITER();

    if (!info)
        return -EINVAL;

    path_buf = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!path_buf)
        return -ENOMEM;

    rcu_read_lock();
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (task)
        get_task_struct(task);
    rcu_read_unlock();
    if (!task)
    {
        kfree(path_buf);
        return -ESRCH;
    }

    mm = get_task_mm(task);
    if (!mm)
    {
        put_task_struct(task);
        kfree(path_buf);
        return -EINVAL;
    }

    info->module_count = 0;
    info->region_count = 0;

    mmap_read_lock(mm);
    INIT_VMA_ITER(mm);

    FOR_EACH_VMA_UNIFIED(vma)
    {

        // 直接用位运算将权限转为 1(R) 2(W) 4(X)
        uint8_t current_prot = 0;
        if (vma->vm_flags & VM_READ)
            current_prot |= 1;
        if (vma->vm_flags & VM_WRITE)
            current_prot |= 2;
        if (vma->vm_flags & VM_EXEC)
            current_prot |= 4;

        /* ========== 模块收集 (仅白名单前缀) ========== */

        // BSS检测: 前段文件映射RW, 当前段匿名+地址连续+RW
        if (prev && prev->vm_file && VMA_IS_RW(prev) &&
            !vma->vm_file && vma->vm_start == prev->vm_end && VMA_IS_RW(vma))
        {
            prev_path = d_path(&prev->vm_file->f_path, path_buf, PATH_MAX);
            if (!IS_ERR(prev_path))
            {
                mod_accepted = false;
                for (i = 0; mod_include_prefixes[i]; i++)
                    if (strncmp(prev_path, mod_include_prefixes[i],
                                strlen(mod_include_prefixes[i])) == 0)
                    {
                        mod_accepted = true;
                        break;
                    }
                if (mod_accepted)
                {
                    idx = find_or_add_module(info->modules, &info->module_count, prev_path);
                    if (idx >= 0)
                        add_seg(&info->modules[idx], -1, current_prot, vma->vm_start, vma->vm_end);
                }
            }
        }

        // 文件映射区段
        if (vma->vm_file)
        {
            path = d_path(&vma->vm_file->f_path, path_buf, PATH_MAX);
            if (!IS_ERR(path))
            {
                mod_accepted = false;
                for (i = 0; mod_include_prefixes[i]; i++)
                    if (strncmp(path, mod_include_prefixes[i],
                                strlen(mod_include_prefixes[i])) == 0)
                    {
                        mod_accepted = true;
                        break;
                    }
                if (mod_accepted)
                {
                    idx = find_or_add_module(info->modules, &info->module_count, path);
                    if (idx >= 0)
                    {
                        if (VMA_IS_RX(vma))
                            add_seg(&info->modules[idx], 0, current_prot, vma->vm_start, vma->vm_end);
                        else if (VMA_IS_RO(vma))
                            add_seg(&info->modules[idx], 1, current_prot, vma->vm_start, vma->vm_end);
                        else if (VMA_IS_RW(vma))
                            add_seg(&info->modules[idx], 2, current_prot, vma->vm_start, vma->vm_end);
                    }
                }
            }
        }

        /* ========== 扫描区域收集 ========== */

        if (VMA_IS_RWP(vma) && info->region_count < MAX_SCAN_REGIONS)
        {
            excluded = false;

            if (vma->vm_file)
            {
                path = d_path(&vma->vm_file->f_path, path_buf, PATH_MAX);
                if (!IS_ERR(path))
                {
                    for (i = 0; excl_prefixes[i]; i++)
                        if (strncmp(path, excl_prefixes[i],
                                    strlen(excl_prefixes[i])) == 0)
                        {
                            excluded = true;
                            break;
                        }
                    if (!excluded)
                        for (i = 0; excl_keywords[i]; i++)
                            if (strstr(path, excl_keywords[i]))
                            {
                                excluded = true;
                                break;
                            }
                }
            }
            else
            {
                // 栈检测
                if (mm->start_stack >= vma->vm_start &&
                    mm->start_stack < vma->vm_end)
                    excluded = true;

                // 特殊VMA名称检测
                if (!excluded && vma->vm_ops && vma->vm_ops->name)
                {
                    const char *vma_name = vma->vm_ops->name(vma);
                    if (vma_name &&
                        (strcmp(vma_name, "[vvar]") == 0 ||
                         strcmp(vma_name, "[vdso]") == 0 ||
                         strcmp(vma_name, "[vsyscall]") == 0))
                        excluded = true;
                }
            }

            if (!excluded)
            {
                info->regions[info->region_count].start = vma->vm_start;
                info->regions[info->region_count].end = vma->vm_end;
                info->region_count++;
            }
        }

        prev = vma;
    }

    mmap_read_unlock(mm);

    /*
     * =========================================================================================
     * 反作弊 VMA 碎裂与诱饵对抗机制
     * =========================================================================================
     *
     * 【第一阶段：理想状态下的纯净内存布局 (原生 ELF 加载)】
     * 当 Linux/Android 原生加载一个 libil2cpp.so 时，它在内存中的排布是非常连续且规律的：
     * - 头部 (RO): 包含 ELF Header 和 Program Header，也就是真实的基址 (Base Address)。
     * - 代码 (RX): .text 段，紧跟在头部之后。
     * - 数据 (RW): .data / .bss 段，跟在代码之后。
     * 事实上，现代 Android 系统（特别是 LLVM/Clang 编译的 64 位）出于安全考虑，
     * 原生加载时至少会 4 到 6 个 VMA 碎片，小的so文件就 1 到 2 个VMA：
     *   1. 头部 (RO): ELF Header 和 .rodata（真实的 Base Address 起点）。
     *   2. 代码 (RX): .text 段，紧跟头部。
     *   3. RELRO (RO): 系统安全机制，写完重定位表后强行锁死为只读，凭空多出一个 RO 碎片。
     *   4. 数据 (RW): .data 全局变量段。
     *   5. BSS  (RW): 尾部额外分配的无文件映射的匿名读写内存。
     * 所以，即使没有反作弊，最纯净的环境也会产生 [RO -> RX -> RO -> RW -> RW] 的轻微碎裂。
     * 此时驱动收集到的段非常完美：Index 0=RX(代码), Index 1=RO(头基址), Index 2=RW(数据)。
     *
     * 【第二阶段：反作弊系统的双重伪装攻击】
     * 现代顶级反作弊（如 ACE）为了防止外部读取和内存 Dump，会做两件极其恶心的事情：
     *
     *   攻击手段 1：VMA 碎裂
     *   反作弊为了 Hook 游戏内部函数，会高频调用 mprotect() 修改代码段的权限。
     *   Linux 内核为了管理不同的物理页权限，被迫将原本 1 个巨大的 RX 代码段，
     *   “劈碎”成了几十甚至上百个细碎的 VMA（虚拟内存区域），并且有些页被改成了 RWX 混合权限。
     *   这导致我们原本排在第 2 位的 RO 段（真实基址），被前面几十个 RX 碎片硬生生挤到了
     *   Index 8 甚至 Index 9 的位置，导致固定索引偏移失效。
     *
     *   攻击手段 2：远端假诱饵
     *   反作弊会在距离真实模块上百 MB 远的极低地址（例如 0x6e32250000），
     *   凭空 mmap() 申请一块虚假内存，并将其命名为 libil2cpp.so，权限设为 RO。
     *   如果我们使用常规的合并算法，会误把这个极远的假地址当成模块的起始地址，
     *   从而导致算出的 Base Address 完全错误（偏离真实基址几十上百MB），读取指针全部失效。
     *
     * 【第三阶段：我们的对抗算法】
     * 为了获取绝对精准的真实基址 (Real Base)，我们采用“物理聚类 + 碎片缝合”的降维打击算法：
     *
     *   步骤 1：物理排序与聚类 (寻找生命主干)
     *   无视所有的权限标签，直接把所有叫 libil2cpp.so 的内存块按物理地址 (start) 升序排列。
     *   由于 ARM64 架构指令寻址的限制，真实的 .so 内存必须紧凑地挨在一起。
     *   我们遍历这些内存块，一旦发现两个块之间的“缝隙”超过 16MB (0x1000000 阈值)，
     *   就意味着碰到了“内存断层”。此时立刻判定：那个孤零零在远处的内存绝对是反作弊的假诱饵！
     *
     *   步骤 2：诱饵物理抹杀
     *   算出体积最大的连续内存群落（即真正的 .so 主体），把不在这个范围内的假诱饵（碎片）
     *   从数组中彻底剔除、物理抹杀。
     *
     *   步骤 3：包围盒缝合
     *   将剩下的纯净真碎片，重新按照 RX -> RO -> RW 排序。
     *   针对被反作弊劈碎的同类型碎片，使用“包围盒算法”：取这些碎片的最小 start 和最大 end，
     *   =>>>这一步不仅缝合了被反作弊劈碎的几十个 RX 碎片，同时也顺手把 Android 系统原生的
     *   “头部 RO”和“RELRO RO”抹平，强行揉成了一个巨大的、完美的虚拟段！
     *
     * 【最终战果】：
     * 无论反作弊怎么切分、怎么放诱饵，跑完此算法后，产出的结果永远绝对固定：
     * - 数组 Index 0：必定是缝合后的 RX 完整代码段。
     * - 数组 Index 1：必定是剔除诱饵后的 RO 完整头数据，它的 start【绝对等于】dladdr 获取的真实基址！
     * - 数组 Index 2：必定是缝合后的 RW 完整数据段。
     *
     * 外部辅助只需无脑调用：Read(段1_Start + Golden_RVA)，即可
     * =========================================================================================
     */
    for (i = 0; i < info->module_count; i++)
    {
        struct module_info *m = &info->modules[i];

        if (m->seg_count > 0)
        {
            // 纯物理地址排序，寻找真实的模块主干
            // 使用冒泡排序按 start 升序排列 (暂时无视 RX/RO 权限)
            for (int x = 0; x < m->seg_count - 1; x++)
            {
                for (int y = x + 1; y < m->seg_count; y++)
                {
                    if (m->segs[x].start > m->segs[y].start)
                    {
                        struct segment_info temp = m->segs[x];
                        m->segs[x] = m->segs[y];
                        m->segs[y] = temp;
                    }
                }
            }

            // 聚类算法，找出真实的内存块，标记假诱饵
            // 真实模块的各个段缝隙很小，假诱饵 (如 0x6e32...) 离得极远 (>16MB)
            uint64_t temp_base = m->segs[0].start;
            uint64_t temp_end = m->segs[0].end;
            uint64_t max_chunk_size = 0;
            uint64_t best_base = temp_base;
            uint64_t best_end = temp_end;

            for (j = 1; j < m->seg_count; j++)
            {
                if (m->segs[j].start - temp_end > 0x1000000)
                { // 跨度超过16MB，判定为断层
                    uint64_t chunk_size = temp_end - temp_base;
                    if (chunk_size > max_chunk_size)
                    {
                        max_chunk_size = chunk_size;
                        best_base = temp_base;
                        best_end = temp_end;
                    }
                    temp_base = m->segs[j].start;
                    temp_end = m->segs[j].end;
                }
                else
                {
                    if (m->segs[j].end > temp_end)
                        temp_end = m->segs[j].end;
                }
            }
            if (temp_end - temp_base > max_chunk_size)
            {
                best_base = temp_base;
                best_end = temp_end;
            }

            // 物理清洗，直接剔除诱饵碎片
            int valid_count = 0;
            for (j = 0; j < m->seg_count; j++)
            {
                // 只有处于“真实主干”范围内的段才保留，彻底抹杀 0x6e32...
                if (m->segs[j].start >= best_base && m->segs[j].end <= best_end)
                {
                    m->segs[valid_count++] = m->segs[j];
                }
            }
            m->seg_count = valid_count; // 更新为清洗后的纯净数量

            // 恢复你原本的类型排序 (RX=0 -> RO=1 -> RW=2)
            if (m->seg_count > 1)
            {
                sort(m->segs, m->seg_count, sizeof(struct segment_info), cmp_seg, NULL);
            }

            // 包围盒算法，缝合同类型碎片
            int out_idx = 0;
            for (j = 1; j < m->seg_count; j++)
            {
                struct segment_info *prev = &m->segs[out_idx];
                struct segment_info *curr = &m->segs[j];

                if (prev->index == curr->index)
                {
                    // 同类型的内存碎片，合并边界
                    if (curr->end > prev->end)
                        prev->end = curr->end;
                    prev->prot |= curr->prot;
                }
                else
                {
                    out_idx++;
                    m->segs[out_idx] = *curr;
                }
            }
            m->seg_count = out_idx + 1;

            // 重新打上 0, 1, 2 的索引编号
            seq = 0;
            for (j = 0; j < m->seg_count; j++)
            {
                if (m->segs[j].index != -1)
                {
                    m->segs[j].index = seq++;
                }
            }
        }
    }

    mmput(mm);
    put_task_struct(task);
    kfree(path_buf);
    return 0;
}

#endif // PHYSICAL_H
