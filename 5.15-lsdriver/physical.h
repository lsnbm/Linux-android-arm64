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

struct physical_page_info
{
    void *base_address;
    size_t size;
    pte_t *pte_address;
};
struct physical_page_info info;

// 分配一个虚拟页并获取其PTE信息。
inline int allocate_physical_page_info(void)
{
    unsigned long vaddr;
    pgd_t *pgd;
    p4d_t *p4d;
    pud_t *pud;
    pmd_t *pmd;
    pte_t *ptep;
    struct mm_struct *init_mm_ptr; // 声明前置

    // 将 info 结构体清零
    memset(&info, 0, sizeof(struct physical_page_info));

    // 使用 vmalloc 分配一个页面大小的虚拟内存
    vaddr = (unsigned long)vmalloc(PAGE_SIZE);
    // 检查 vmalloc 是否成功
    if (!vaddr)
    {
        // 如果分配失败，则打印错误并返回内存不足错误
        pr_debug("Failed to allocate memory with vmalloc\n");
        return -ENOMEM;
    }

    // --- 页表遍历开始 ---
    // 获取内核符号 init_mm 的地址
    init_mm_ptr = (struct mm_struct *)generic_kallsyms_lookup_name("init_mm");

    // 从 init_mm 和虚拟地址 vaddr 获取页全局目录（PGD）的偏移量
    pgd = pgd_offset(init_mm_ptr, vaddr);
    // 检查 PGD 条目是否有效
    if (pgd_none(*pgd) || pgd_bad(*pgd))
    {
        pr_debug("Invalid PGD entry for vaddr: 0x%lx\n", vaddr);
        // 如果无效，则释放已分配的虚拟内存并返回错误
        vfree((void *)vaddr);
        return -EFAULT;
    }

    // 从 P4D 和虚拟地址 vaddr 获取页四级目录（P4D）的偏移量
    p4d = p4d_offset(pgd, vaddr);
    // 检查 P4D 条目是否有效
    if (p4d_none(*p4d) || p4d_bad(*p4d))
    {
        pr_debug("Invalid P4D entry for vaddr: 0x%lx\n", vaddr);
        // 如果无效，则释放已分配的虚拟内存并返回错误
        vfree((void *)vaddr);
        return -EFAULT;
    }

    // 从 PUD 和虚拟地址 vaddr 获取页上级目录（PUD）的偏移量
    pud = pud_offset(p4d, vaddr);
    // 检查 PUD 条目是否有效
    if (pud_none(*pud) || pud_bad(*pud))
    {
        pr_debug("Invalid PUD entry for vaddr: 0x%lx\n", vaddr);
        // 如果无效，则释放已分配的虚拟内存并返回错误
        vfree((void *)vaddr);
        return -EFAULT;
    }

    // 从 PMD 和虚拟地址 vaddr 获取页中级目录（PMD）的偏移量
    pmd = pmd_offset(pud, vaddr);
    // 检查 PMD 条目是否有效
    if (pmd_none(*pmd) || pmd_bad(*pmd))
    {
        pr_debug("Invalid PMD entry for vaddr: 0x%lx\n", vaddr);
        // 如果无效，则释放已分配的虚拟内存并返回错误
        vfree((void *)vaddr);
        return -EFAULT;
    }

    // 从 PMD 和虚拟地址 vaddr 获取页表项（PTE）的指针
    ptep = pte_offset_kernel(pmd, vaddr);
    // 检查 PTE 指针是否有效
    if (!ptep)
    {
        pr_debug("Failed to get PTE pointer for vaddr: 0x%lx\n", vaddr);
        // 如果无效，则释放已分配的虚拟内存并返回错误
        vfree((void *)vaddr);
        return -EFAULT;
    }

    // --- 填充返回结构体 ---
    // 将分配的虚拟地址保存到 info 结构体中
    info.base_address = (void *)vaddr;
    // 将页面大小保存到 info 结构体中
    info.size = PAGE_SIZE;
    // 将获取到的 PTE 地址保存到 info 结构体中
    info.pte_address = ptep;

    // 返回成功
    return 0;
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

// 通过直接操作PTE，从指定的物理地址读取数据。

// 通过直接操作PTE，从指定的物理地址读取数据。
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

        _internal_read_fast(full_phys_addr, (char *)buffer + bytes_copied, bytes_to_read_this_page);
  

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

       _internal_write_fast(full_phys_addr, (char *)buffer + bytes_written, bytes_to_write_this_page);
    

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