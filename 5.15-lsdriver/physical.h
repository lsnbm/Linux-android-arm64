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

// ----------------------------------------方案1:PTE读写-------------------------------------------
struct physical_page_info
{
    void *base_address;
    size_t size;
    pte_t *pte_address;
};
struct physical_page_info info;

// 直接从硬件寄存器获取内核页表基地址
static inline pgd_t __attribute__((unused)) * get_kernel_pgd_base(void)
{
    // TTBR0_EL1：对应 “低地址段虚拟地址”（如用户进程的虚拟地址，由内核管理）；
    // TTBR1_EL1：对应 “高地址段虚拟地址”（如内核自身的虚拟地址，仅内核可访问）；
    u64 ttbr1;
    phys_addr_t pgd_phys;

    // 读取 TTBR1_EL1 寄存器 (存放内核页表物理地址)
    asm volatile("mrs %0, ttbr1_el1" : "=r"(ttbr1));

    // TTBR1 包含 ASID 或其他控制位，通常低 48 位是物理地址
    // 这里做一个简单的掩码处理 (64位用48位物理寻址)
    pgd_phys = ttbr1 & 0x0000FFFFFFFFF000ULL;

    // 将物理地址转为内核虚拟地址
    return (pgd_t *)phys_to_virt(pgd_phys);
}

inline int __attribute__((unused)) allocate_physical_page_info(void)
{
    unsigned long vaddr;
    pgd_t *pgd_base;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep;
    unsigned long long val;

    pr_debug("[DEBUG] %s:%d | >>> 函数开始执行 (TTBR1) <<<\n", __func__, __LINE__);

    if (in_atomic())
    {
        pr_debug("[FATAL] 原子上下文禁止调用 vmalloc\n");
        return -EPERM;
    }

    memset(&info, 0, sizeof(struct physical_page_info));

    // 分配内存
    vaddr = (unsigned long)vmalloc(PAGE_SIZE);
    if (!vaddr)
    {
        pr_debug("[FATAL] vmalloc 失败\n");
        return -ENOMEM;
    }
    pr_debug("[DEBUG] vmalloc 成功: 0x%lx\n", vaddr);

    // 必须 memset 触发缺页，让内核填充 TTBR1 指向的页表
    memset((void *)vaddr, 0xAA, PAGE_SIZE);
    pr_debug("[DEBUG] memset 完成 (页表已物理同步)\n");

    // 获取真正的内核页表
    pgd_base = get_kernel_pgd_base();
    pr_debug("[DEBUG] 获取到内核页表基地址(TTBR1): 0x%p\n", pgd_base);

    // 计算 PGD 索引 (pgd_offset_raw)
    pgd = pgd_base + pgd_index(vaddr);
    pr_debug("[DEBUG] PGD 条目地址: 0x%p\n", pgd);

    if (pgd_none(*pgd) || pgd_bad(*pgd))
    {
        pr_debug("[FATAL] PGD 无效 (val: 0x%llx)\n", pgd_val(*pgd));
        goto err_out;
    }
    pr_debug("[DEBUG] PGD 有效 (val: 0x%llx)\n", pgd_val(*pgd));

    // P4D
    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
    {
        pr_debug("[FATAL] P4D 无效 (val: 0x%llx)\n", p4d_val(*p4d));
        goto err_out;
    }

    // PUD
    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud))
    {
        pr_debug("[FATAL] PUD 无效 (val: 0x%llx)\n", pud_val(*pud));
        goto err_out;
    }
    pr_debug("[DEBUG] PUD 有效 (val: 0x%llx)\n", pud_val(*pud));

    // PMD
    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
    {
        pr_debug("[FATAL] PMD 无效 (val: 0x%llx)\n", pmd_val(*pmd));
        goto err_out;
    }
    pr_debug("[DEBUG] PMD 有效 (val: 0x%llx)\n", pmd_val(*pmd));

    // 大页检查
    if (pmd_leaf(*pmd))
    {
        pr_debug("[FATAL] 遇到大页 (Block Mapping)，无法获取 PTE\n");
        goto err_out;
    }

    // PTE
    ptep = pte_offset_kernel(pmd, vaddr);
    if (!ptep)
    {
        pr_debug("[FATAL] PTE 指针为空\n");
        goto err_out;
    }
    pr_debug("[DEBUG] PTE 指针: 0x%p\n", ptep);

    // 验证读取
    val = pte_val(*ptep);
    pr_debug("[SUCCESS] 读取 PTE 成功! Value: 0x%llx\n", val);

    info.base_address = (void *)vaddr;
    info.size = PAGE_SIZE;
    info.pte_address = ptep;

    return 0;

err_out:
    vfree((void *)vaddr);
    return -EFAULT;
}
// 释放由 allocate_physical_page_info 分配的资源。
inline void __attribute__((unused)) free_physical_page_info(void)
{

    // 检查 info 指针及其 base_address 是否有效
    if (info.base_address)
    {
        // 释放之前通过 vmalloc 分配的虚拟内存
        vfree(info.base_address);
        // 将 base_address 设置为 NULL 以避免悬空指针
        info.base_address = NULL;
    }
}

// 通过直接操作PTE，从指定的任意物理地址读取数据。
inline void _internal_read_fast(phys_addr_t paddr, void *buffer, size_t size)
{
    // MT_NORMAL_NC无缓存只读(建议用缓存MT_NORMAL因为cpu来不及把进程数据缓存写入内存，就直接读物理会有意外如：在1000000次测试下发现数据不匹配就是应为缓存没及时写入内存)
    static const u64 FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF | PTE_SHARED | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_NORMAL);
    unsigned long pfn = __phys_to_pfn(paddr);

    // 直接修改 PTE 指向目标物理页
    set_pte(info.pte_address, pfn_pte(pfn, __pgprot(FLAGS)));

    // 内存全序屏障
    // dsb(ishst);

    // 刷新 TLB (只刷新单个页);
    flush_tlb_kernel_range((unsigned long)info.base_address, (unsigned long)info.base_address + PAGE_SIZE);
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
inline void _internal_write_fast(phys_addr_t paddr, const void *buffer, size_t size)
{
    // MT_NORMAL_NC无缓存只读(建议用缓存MT_NORMAL因为cpu来不及把进程数据缓存写入内存，就直接读物理会有意外如：在1000000次测试下发现数据不匹配原因就是缓存没及时写入内存)
    static const u64 FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF | PTE_SHARED | PTE_WRITE | PTE_PXN | PTE_UXN | PTE_ATTRINDX(MT_NORMAL);
    unsigned long pfn = __phys_to_pfn(paddr);

    set_pte(info.pte_address, pfn_pte(pfn, __pgprot(FLAGS)));
    flush_tlb_kernel_range((unsigned long)info.base_address, (unsigned long)info.base_address + PAGE_SIZE);

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
// ----------------------------------------方案1:PTE读写-------------------------------------------

// ---------------------------方案2:内核已经映射的线性地址读写----------------------------------------
//  线性地址读取
inline void _internal_read_fast_linear(phys_addr_t paddr, void *buffer, size_t size)
{

    void *kernel_vaddr;
    // ---------------------------------------------------------
    // 确保该物理地址对应的是有效的系统 RAM
    // 如果是设备内存（MMIO，比如显存寄存器），phys_to_virt 可能无效
    // ---------------------------------------------------------
    if (unlikely(!pfn_valid(__phys_to_pfn(paddr))))
    {
        return;
    }

    // 直接数学转换
    // 在 ARM64 上，这通常等价于: (void*)(paddr + PAGE_OFFSET)
    kernel_vaddr = phys_to_virt(paddr);

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
}
inline void _internal_write_fast_linear(phys_addr_t paddr, const void *buffer, size_t size)
{
    void *kernel_vaddr;
    if (unlikely(!pfn_valid(__phys_to_pfn(paddr))))
    {
        return;
    }
    kernel_vaddr = phys_to_virt(paddr);

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
}
// ---------------------------方案2:内核已经映射的线性地址读写----------------------------------------

// 只负责走页表
inline int manual_va_to_pa_arm(struct mm_struct *mm, unsigned long long vaddr, phys_addr_t *paddr)
{
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep;
    int ret = 0;

    if (!paddr || !mm)
        return -EINVAL;
    *paddr = 0;

    if (unlikely(!mmap_read_trylock(mm)))
    {
        mmap_read_lock(mm);
    }

    pgd = pgd_offset(mm, vaddr);
    if (pgd_none(*pgd) || pgd_bad(*pgd))
    {
        ret = -ENOENT;
        goto out_unlock;
    }

    p4d = p4d_offset(pgd, vaddr);
    if (p4d_none(*p4d) || p4d_bad(*p4d))
    {
        ret = -ENOENT;
        goto out_unlock;
    }

    pud = pud_offset(p4d, vaddr);
    if (pud_none(*pud) || pud_bad(*pud))
    {
        ret = -ENOENT;
        goto out_unlock;
    }

    if (pud_leaf(*pud))
    {
        *paddr = (pud_pfn(*pud) << PAGE_SHIFT) + (vaddr & ~PUD_MASK);
        goto out_unlock;
    }

    pmd = pmd_offset(pud, vaddr);
    if (pmd_none(*pmd) || pmd_bad(*pmd))
    {
        ret = -ENOENT;
        goto out_unlock;
    }

    if (pmd_leaf(*pmd))
    {
        *paddr = (pmd_pfn(*pmd) << PAGE_SHIFT) + (vaddr & ~PMD_MASK);
        goto out_unlock;
    }

    ptep = pte_offset_map(pmd, vaddr);
    if (!ptep)
    {
        ret = -ENOENT;
        goto out_unlock;
    }

    if (pte_present(*ptep))
    {
        *paddr = (pte_pfn(*ptep) << PAGE_SHIFT) + (vaddr & ~PAGE_MASK);
    }
    else
    {
        ret = -ENOENT;
    }
    pte_unmap(ptep);

out_unlock:
    mmap_read_unlock(mm);
    return ret;
}

inline int read_process_memory(pid_t pid, unsigned long long vaddr, void *buffer, size_t size)
{
    static pid_t s_last_pid = 0;
    static struct mm_struct *s_last_mm = NULL;

    phys_addr_t paddr_of_page = 0;
    unsigned long long current_vaddr = vaddr;
    size_t bytes_remaining = size;
    size_t bytes_copied = 0;
    int status = 0;

    // 局部变量，用于循环内的软件 TLB 优化
    unsigned long long loop_last_vpage_base = -1;
    phys_addr_t loop_last_ppage_base = 0;

    if (unlikely(!buffer || size == 0))
        return -EINVAL;

    // 检查 PID 是否改变
    if (unlikely(pid != s_last_pid || s_last_mm == NULL))
    {
        struct pid *pid_struct = NULL;
        struct task_struct *task = NULL;

        // 如果有释放旧的 mm
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

        // 更新缓存
        s_last_mm = get_task_mm(task); // 引用计数 +1
        put_task_struct(task);

        if (!s_last_mm)
            return -EINVAL;
        s_last_pid = pid;
    }

    while (bytes_remaining > 0)
    {
        size_t page_offset = current_vaddr & (PAGE_SIZE - 1);
        size_t bytes_to_read_this_page = PAGE_SIZE - page_offset;
        phys_addr_t full_phys_addr; // 声明前置

        if (bytes_to_read_this_page > bytes_remaining)
            bytes_to_read_this_page = bytes_remaining;

        // 循环内优化：检查是否命中局部页缓存
        if ((current_vaddr & PAGE_MASK) == loop_last_vpage_base)
        {
            paddr_of_page = loop_last_ppage_base;
        }
        else
        {
            // 直接传入缓存的 s_last_mm
            status = manual_va_to_pa_arm(s_last_mm, current_vaddr & PAGE_MASK, &paddr_of_page);
            if (status != 0)
                return status;

            loop_last_vpage_base = current_vaddr & PAGE_MASK;
            loop_last_ppage_base = paddr_of_page;
        }

        full_phys_addr = paddr_of_page + page_offset;

        _internal_read_fast_linear(full_phys_addr, (char *)buffer + bytes_copied, bytes_to_read_this_page);

        bytes_remaining -= bytes_to_read_this_page;
        bytes_copied += bytes_to_read_this_page;
        current_vaddr += bytes_to_read_this_page;
    }

    return 0;
}

inline int write_process_memory(pid_t pid, unsigned long long vaddr, const void *buffer, size_t size)
{
    static pid_t s_last_pid = 0;
    static struct mm_struct *s_last_mm = NULL;

    phys_addr_t paddr_of_page = 0;
    unsigned long long current_vaddr = vaddr;
    size_t bytes_remaining = size;
    size_t bytes_written = 0;
    int status = 0;

    unsigned long long loop_last_vpage_base = -1;
    phys_addr_t loop_last_ppage_base = 0;

    if (unlikely(!buffer || size == 0))
        return -EINVAL;

    // 核心优化：检查 PID 是否改变
    if (unlikely(pid != s_last_pid || s_last_mm == NULL))
    {
        struct pid *pid_struct = NULL;
        struct task_struct *task = NULL;

        if (s_last_mm)
        {
            mmput(s_last_mm);
            s_last_mm = NULL;
        }
        s_last_pid = 0;

        pid_struct = find_get_pid(pid);
        if (!pid_struct)
            return -ESRCH;

        task = get_pid_task(pid_struct, PIDTYPE_PID);
        put_pid(pid_struct);
        if (!task)
            return -ESRCH;

        s_last_mm = get_task_mm(task);
        put_task_struct(task);

        if (!s_last_mm)
            return -EINVAL;
        s_last_pid = pid;
    }

    while (bytes_remaining > 0)
    {
        size_t page_offset = current_vaddr & (PAGE_SIZE - 1);
        size_t bytes_to_write_this_page = PAGE_SIZE - page_offset;
        phys_addr_t full_phys_addr; // 声明前置

        if (bytes_to_write_this_page > bytes_remaining)
            bytes_to_write_this_page = bytes_remaining;

        if ((current_vaddr & PAGE_MASK) == loop_last_vpage_base)
        {
            paddr_of_page = loop_last_ppage_base;
        }
        else
        {
            status = manual_va_to_pa_arm(s_last_mm, current_vaddr & PAGE_MASK, &paddr_of_page);
            if (status != 0)
                return status;

            loop_last_vpage_base = current_vaddr & PAGE_MASK;
            loop_last_ppage_base = paddr_of_page;
        }

        full_phys_addr = paddr_of_page + page_offset;

        _internal_write_fast_linear(full_phys_addr, (char *)buffer + bytes_written, bytes_to_write_this_page);

        bytes_remaining -= bytes_to_write_this_page;
        bytes_written += bytes_to_write_this_page;
        current_vaddr += bytes_to_write_this_page;
    }

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
组内排序 (Internal Sorting): 在每一个权限组内部，所有的段都严格按照内存地址从低到高进行排序。
展平为最终列表 (Flattening): 将这三个排好序的组按优先级顺序拼接成一个大的虚拟列表，然后呈现。
先放所有排好序的 r-xp 段。
然后紧接着放所有排好序的 r--p 段。
最后放所有排好序的 rw-p 段。

*/

// 比较函数
inline int compare_ull(const void *a, const void *b)
{
    if (*(unsigned long *)a < *(unsigned long *)b)
        return -1;
    if (*(unsigned long *)a > *(unsigned long *)b)
        return 1;
    return 0;
}

inline int get_module_base(pid_t pid, const char *ModuleName, short ModifierIndex, unsigned long long *ModuleAddr)
{
    struct task_struct *task;
    struct mm_struct *mm;
    int ret = -ESRCH;

    char *path_buffer = NULL;
    char *real_module_name = NULL;
    char *temp_name_copy = NULL;
    bool find_bss_mode = false;

    if (!ModuleName || !ModuleAddr)
    {
        return -EINVAL;
    }

    // 模式判断和参数准备
    if (strstr(ModuleName, ":bss"))
    {
        find_bss_mode = true;
        temp_name_copy = kstrdup(ModuleName, GFP_KERNEL);
        if (!temp_name_copy)
            return -ENOMEM;
        *strchr(temp_name_copy, ':') = '\0';
        real_module_name = temp_name_copy;
    }
    else
    {
        find_bss_mode = false;
        real_module_name = (char *)ModuleName;
    }

    // 通用设置
    path_buffer = kmalloc(PATH_MAX, GFP_KERNEL);
    if (!path_buffer)
    {
        ret = -ENOMEM;
        goto out_free_name_copy;
    }

    // ！！！显式引用计数管理 ！！！
    rcu_read_lock(); // find_vpid 和 pid_task 需要在 RCU 读锁下进行
    task = pid_task(find_vpid(pid), PIDTYPE_PID);
    if (task)
    {
        get_task_struct(task); // 手动增加 task 的引用计数
    }
    rcu_read_unlock();

    if (!task)
    {
        ret = -ESRCH;
        goto out_free_path_buffer;
    }

    mm = get_task_mm(task);
    if (!mm)
    {
        ret = -EINVAL;
        goto out_put_task;
    }

    // 根据模式选择逻辑分支
    if (find_bss_mode)
    {
        // 分支 A: .bss 段查找逻辑

        struct vm_area_struct *vma;
        // struct vma_iterator vmi; // 5.15 不支持
        struct vm_area_struct *prev_vma = NULL;

        char *ret_path;
        mmap_read_lock(mm);
        // vma_iter_init(&vmi, mm, 0); // 5.15 不支持
        ret = -ENOENT; // 默认没找到

        // 在单次遍历中，检查 prev_vma 和 当前 vma 的关系
        // 5.15 替换为传统链表遍历
        for (vma = mm->mmap; vma; vma = vma->vm_next)
        {
            // 检查上一个VMA是否是我们要找的.data段候选者
            if (prev_vma && prev_vma->vm_file)
            {
                ret_path = d_path(&prev_vma->vm_file->f_path, path_buffer, PATH_MAX);
                if (!IS_ERR(ret_path) && strstr(ret_path, real_module_name))
                {
                    // 如果上一个是rw-p的文件映射段
                    if ((prev_vma->vm_flags & (VM_READ | VM_WRITE)) == (VM_READ | VM_WRITE) && !(prev_vma->vm_flags & VM_EXEC))
                    {
                        // 那么检查当前vma是否是紧邻的、匿名的、rw-p的段 (即.bss)
                        if (!vma->vm_file && vma->vm_start == prev_vma->vm_end &&
                            (vma->vm_flags & (VM_READ | VM_WRITE)) == (VM_READ | VM_WRITE) && !(vma->vm_flags & VM_EXEC))
                        {
                            *ModuleAddr = vma->vm_start;
                            ret = 0; // 成功！
                            break;   // 找到后退出循环
                        }
                    }
                }
            }
            prev_vma = vma; // 更新 prev_vma 以供下一次循环使用
        }

        mmap_read_unlock(mm);
        goto out_mmput;
    }
    else
    {

        // 分支 B: 详细 pr_debug 追踪执行流程

#define MAX_MODULE_SEGS 32

        // 使用 static 避免栈溢出风险
        static unsigned long rx_list[MAX_MODULE_SEGS];
        static unsigned long ro_list[MAX_MODULE_SEGS];
        static unsigned long rw_list[MAX_MODULE_SEGS];
        int rx_count = 0, ro_count = 0, rw_count = 0;
        char *ret_path;

        struct vm_area_struct *vma;
        // struct vma_iterator vmi; // 5.15 不支持

        pr_debug("====== [LSDriver-DBG] ENTER: Index Search Mode (Target Index: %d) ======\n", ModifierIndex);

        // 单次遍历
        mmap_read_lock(mm);
        // vma_iter_init(&vmi, mm, 0);
        pr_debug("[LSDriver-DBG] Starting single-pass scan for module: %s\n", real_module_name);
        // 5.15 替换为传统链表遍历
        for (vma = mm->mmap; vma; vma = vma->vm_next)
        {
            if (!vma->vm_file)
                continue;

            ret_path = d_path(&vma->vm_file->f_path, path_buffer, PATH_MAX);
            if (IS_ERR(ret_path))
                continue;

            if (strstr(ret_path, real_module_name))
            {
                unsigned long flags = vma->vm_flags;
                pr_debug("[LSDriver-DBG]   Found matching segment. Path: %s, Addr: 0x%lx\n", ret_path, vma->vm_start);
                if ((flags & VM_READ) && (flags & VM_EXEC) && !(flags & VM_WRITE))
                {
                    if (rx_count < MAX_MODULE_SEGS)
                    {
                        pr_debug("[LSDriver-DBG]     -> Adding to rx_list. New count: %d\n", rx_count + 1);
                        rx_list[rx_count++] = vma->vm_start;
                    }
                }
                else if ((flags & VM_READ) && !(flags & VM_EXEC) && !(flags & VM_WRITE))
                {
                    if (ro_count < MAX_MODULE_SEGS)
                    {
                        pr_debug("[LSDriver-DBG]     -> Adding to ro_list. New count: %d\n", ro_count + 1);
                        ro_list[ro_count++] = vma->vm_start;
                    }
                }
                else if ((flags & VM_READ) && (flags & VM_WRITE) && !(flags & VM_EXEC))
                {
                    if (rw_count < MAX_MODULE_SEGS)
                    {
                        pr_debug("[LSDriver-DBG]     -> Adding to rw_list. New count: %d\n", rw_count + 1);
                        rw_list[rw_count++] = vma->vm_start;
                    }
                }
            }
        }
        pr_debug("[LSDriver-DBG] Scan finished.\n");
        mmap_read_unlock(mm);

        pr_debug("[LSDriver-DBG] Final Counts: rx_count=%d, ro_count=%d, rw_count=%d\n", rx_count, ro_count, rw_count);

        if (rx_count == 0 && ro_count == 0 && rw_count == 0)
        {
            pr_warn("[LSDriver-DBG] No segments found for module. Exiting.\n");
            ret = -ENOENT;
            goto out_no_kmalloc;
        }

        // 排序
        if (rx_count > 1)
        {
            pr_debug("[LSDriver-DBG] Sorting rx_list (count: %d)...\n", rx_count);
            sort(rx_list, rx_count, sizeof(unsigned long), compare_ull, NULL);
            pr_debug("[LSDriver-DBG] ...rx_list sorted.\n");
        }
        if (ro_count > 1)
        {
            pr_debug("[LSDriver-DBG] Sorting ro_list (count: %d)...\n", ro_count);
            sort(ro_list, ro_count, sizeof(unsigned long), compare_ull, NULL);
            pr_debug("[LSDriver-DBG] ...ro_list sorted.\n");
        }
        if (rw_count > 1)
        {
            pr_debug("[LSDriver-DBG] Sorting rw_list (count: %d)...\n", rw_count);
            sort(rw_list, rw_count, sizeof(unsigned long), compare_ull, NULL);
            pr_debug("[LSDriver-DBG] ...rw_list sorted.\n");
        }

        // 索引 (为了最详细的日志，将 if-else if 结构改为嵌套的 if-else)
        pr_debug("[LSDriver-DBG] Starting final indexing for Target Index: %d\n", ModifierIndex);
        ret = -EINVAL;

        pr_debug("[LSDriver-DBG] -> Checking rx_list (count=%d). Condition: %d < %d ?\n", rx_count, ModifierIndex, rx_count);
        if (ModifierIndex < rx_count)
        {
            unsigned long addr_to_return;
            pr_debug("[LSDriver-DBG]   Condition TRUE. Reading from rx_list[%d]...\n", ModifierIndex);
            addr_to_return = rx_list[ModifierIndex];
            pr_debug("[LSDriver-DBG]   Read value: 0x%lx. Assigning to output pointer...\n", addr_to_return);
            *ModuleAddr = addr_to_return;
            ret = 0;
            pr_debug("[LSDriver-DBG]   Assignment complete. Success.\n");
        }
        else
        {
            int ro_idx; // 声明前置
            pr_debug("[LSDriver-DBG]   Condition FALSE. Proceeding to ro_list.\n");
            ro_idx = ModifierIndex - rx_count;
            pr_debug("[LSDriver-DBG] -> Checking ro_list (count=%d). Calculated ro_idx: %d. Condition: %d < %d ?\n", ro_count, ro_idx, ro_idx, ro_count);
            if (ro_idx < ro_count)
            {
                unsigned long addr_to_return;
                pr_debug("[LSDriver-DBG]   Condition TRUE. Reading from ro_list[%d]...\n", ro_idx);
                addr_to_return = ro_list[ro_idx];
                pr_debug("[LSDriver-DBG]   Read value: 0x%lx. Assigning to output pointer...\n", addr_to_return);
                *ModuleAddr = addr_to_return;
                ret = 0;
                pr_debug("[LSDriver-DBG]   Assignment complete. Success.\n");
            }
            else
            {
                int rw_idx; // 声明前置
                pr_debug("[LSDriver-DBG]   Condition FALSE. Proceeding to rw_list.\n");
                rw_idx = ModifierIndex - rx_count - ro_count;
                pr_debug("[LSDriver-DBG] -> Checking rw_list (count=%d). Calculated rw_idx: %d. Condition: %d < %d ?\n", rw_count, rw_idx, rw_idx, rw_count);
                if (rw_idx < rw_count)
                {
                    unsigned long addr_to_return;
                    pr_debug("[LSDriver-DBG]   Condition TRUE. Reading from rw_list[%d]...\n", rw_idx);
                    addr_to_return = rw_list[rw_idx];
                    pr_debug("[LSDriver-DBG]   Read value: 0x%lx. Assigning to output pointer...\n", addr_to_return);
                    *ModuleAddr = addr_to_return;
                    ret = 0;
                    pr_debug("[LSDriver-DBG]   Assignment complete. Success.\n");
                }
                else
                {
                    pr_warn("[LSDriver-DBG]   Condition FALSE. Index is out of bounds. Total segments found: %d.\n", rx_count + ro_count + rw_count);
                }
            }
        }

        pr_debug("[LSDriver-DBG] Final indexing complete. Final result code: %d\n", ret);

    out_no_kmalloc:;
        pr_debug("====== [LSDriver-DBG] EXIT: Index Search Mode ======\n");
    }
    // 统一的清理和返回
out_mmput:
    mmput(mm);
out_put_task:
    // ！！！与 get_task_struct 配对使用，安全地释放引用 ！！！
    if (task)
    {
        put_task_struct(task);
    }
out_free_path_buffer:
    kfree(path_buffer);
out_free_name_copy:
    if (temp_name_copy)
    {
        kfree(temp_name_copy);
    }
    return ret;
}

#endif