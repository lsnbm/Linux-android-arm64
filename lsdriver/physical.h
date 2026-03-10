
/*
使用了两种方案进行读写
    1.pte直接映射任意物理地址进行读写，设置页任意属性，任意写入，不区分设备内存和系统内存

(建议使用，因为都是都是通过页表建立虚拟地址→物理地址的映射。)
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
#include "ExportFun.h"
#include "io_struct.h"

//============方案1:(不建议使用：顶部有说原因)PTE读写+MMU硬件翻译地址============

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

    memset(&pte_info, 0, sizeof(pte_info));

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
    static const uint64_t FLAGS = PTE_TYPE_PAGE | PTE_VALID | PTE_AF |
                                  PTE_SHARED | PTE_PXN | PTE_UXN |
                                  PTE_ATTRINDX(MT_NORMAL);
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
    u64 tmp_daif, tmp_ttbr, tmp_par, tmp_offset;

    if (unlikely(!mm || !mm->pgd || !pa))
        return -EINVAL;

    pgd_phys = virt_to_phys(mm->pgd);

    asm volatile(
        // 关中断，防止抢占和中断嵌套
        "mrs    %[tmp_daif], daif\n"
        "msr    daifset, #0xf\n" /* 关闭所有中断(D/A/I/F) */
        "isb\n"

        // 切换 TTBR0，ASID 域清零 (bits[63:48]=0)
        "mrs    %[tmp_ttbr], ttbr0_el1\n"
        "msr    ttbr0_el1, %[pgd_phys]\n"
        "isb\n"

        // 硬件地址翻译
        "at     s1e0r, %[va]\n"
        "isb\n"
        "mrs    %[tmp_par], par_el1\n"

        // 清除 ASID=0 的 TLB 污染
        //  vaae1is: VA+所有ASID, EL1, Inner Shareable (广播所有核)
        "lsr    %[tmp_offset], %[va], #12\n"
        "tlbi   vaae1is, %[tmp_offset]\n" // 广播所有CPU，所有ASID
        "dsb    ish\n"
        "isb\n"

        // 恢复 TTBR0
        "msr    ttbr0_el1, %[tmp_ttbr]\n"
        "isb\n"

        // 恢复中断（最后恢复，缩小窗口）
        "msr    daif, %[tmp_daif]\n"
        "isb\n"

        // 检查 PAR_EL1.F (bit 0)，1 表示翻译失败
        "tbnz   %[tmp_par], #0, .L_efault%=\n"

        // 提取物理地址
        // PAR_EL1[47:12] = PA[47:12]，保留低12位页内偏移
        "ubfx   %[tmp_par], %[tmp_par], #12, #36\n"
        "lsl    %[tmp_par], %[tmp_par], #12\n"
        "and    %[tmp_offset], %[va], #0xFFF\n"
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
          [tmp_offset] "=&r"(tmp_offset)
        : [pgd_phys] "r"(pgd_phys),
          [va] "r"(va),
          [efault_val] "r"(-EFAULT)
        : "cc", "memory");

    if (ret == 0)
        *pa = phys_out;

    return ret;
}

//============方案2:(建议使用,顶部有说原因)内核已经映射的线性地址读写+手动走页表翻译地址============

static inline int linear_read_physical(phys_addr_t paddr, void *buffer, size_t size)
{
    void *kernel_vaddr = phys_to_virt(paddr);

    // 下面这个先暂时不使用，靠翻译阶段得出绝对有效物理地址
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
            // status = mmu_translate_va_to_pa(s_last_mm, current_vpn, &paddr_of_page);
            status = walk_translate_va_to_pa(s_last_mm, current_vpn, &paddr_of_page);

            if (unlikely(status != 0))
            {
                s_last_vpage_base = -1ULL;
                if (op == op_r)
                    memset((uint8_t *)buffer + bytes_copied, 0, bytes_this_page);
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

        if (unlikely(status != 0))
        {
            s_last_vpage_base = -1ULL;
            if (op == op_r)
                memset((uint8_t *)buffer + bytes_copied, 0, bytes_this_page);
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
优先级分组: 将所有内存段按权限分为三组，并按固定的优先级顺序排列它们。
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

static inline int find_or_add_module(struct module_info *modules, int *module_count, const uint8_t *name)
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

static inline void add_seg(struct module_info *m, short type_tag, uint8_t prot, uint64_t start, uint64_t end)
{
    if (m->seg_count >= MAX_SEGS_PER_MODULE)
        return;
    m->segs[m->seg_count].index = type_tag;
    m->segs[m->seg_count].prot = prot;
    m->segs[m->seg_count].start = start;
    m->segs[m->seg_count].end = end;
    m->seg_count++;
}

static inline int enum_process_memory(pid_t pid, struct memory_info *info)
{
    struct task_struct *task = NULL;
    struct mm_struct *mm = NULL;
    struct vm_area_struct *vma, *prev = NULL;
    char *path_buf, *path;
    int last_mod_idx = -1; // 用于记录上一个VMA的模块归属，极大优化BSS检测性能
    int i, j;
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

        // 条件：有prev、当前匿名(无文件)、当前为RW、地址严格首尾相连、且 prev 属于我们追踪的模块
        if (prev && !vma->vm_file && vma->vm_start == prev->vm_end && VMA_IS_RW(vma) && last_mod_idx >= 0)
        {
            // 直接复用上一轮循环得到的 last_mod_idx，极大提升性能
            add_seg(&info->modules[last_mod_idx], -1, current_prot, vma->vm_start, vma->vm_end);

            // 注意：BSS 之后通常不会紧跟同一个模块的内容了，清空归属防止误判后续的匿名堆内存
            last_mod_idx = -1;
        }
        else if (vma->vm_file)
        {
            // 每次遇到新的文件映射，先重置归属状态
            last_mod_idx = -1;

            path = d_path(&vma->vm_file->f_path, path_buf, PATH_MAX);
            if (!IS_ERR(path))
            {
                mod_accepted = false;
                for (i = 0; mod_include_prefixes[i]; i++)
                {
                    if (strncmp(path, mod_include_prefixes[i], strlen(mod_include_prefixes[i])) == 0)
                    {
                        mod_accepted = true;
                        break;
                    }
                }
                if (mod_accepted)
                {
                    last_mod_idx = find_or_add_module(info->modules, &info->module_count, path);
                    if (last_mod_idx >= 0)
                    {
                        // 标签随便给 0 即可，因为后面的“反作弊拓扑打标”会根据物理形态强制洗白并覆盖它
                        add_seg(&info->modules[last_mod_idx], 0, current_prot, vma->vm_start, vma->vm_end);
                    }
                }
            }
        }
        else
        {
            // 既不是目标文件映射，也不是紧连着的BSS，断开模块连结
            last_mod_idx = -1;
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
     * 当 Linux/Android 原生加载一个 libil2cpp.so 时，它在内存中的排布是非常连续且规律的。
     * 现代 Android 系统（特别是 LLVM/Clang 编译的 64 位）出于安全考虑，
     * 原生加载时至少会产生 4 到 6 个连续的 VMA 区段（小的 so 文件会有 1 到 2 个）：
     *   1. 头部 (RO): 包含 ELF Header 和 Program Header，真实的 Base Address 起点。
     *   2. 代码 (RX): .text 段，紧跟在头部之后。
     *   3. RELRO (RO): 系统安全机制，写完重定位表后强行锁死为只读，凭空多出第二个 RO 段。
     *   4. 数据 (RW): .data 全局变量段。
     *   5. BSS  (RW): 尾部额外分配的无文件映射的匿名读写内存。
     * 所以，即使没有反作弊，最纯净的环境也会产生 [RO -> RX -> RO -> RW -> RW] 的天然区段。
     * 此时驱动收集到的区段严格按照物理内存的先后顺序自然排列。
     *
     * 【第二阶段：反作弊系统的双重伪装攻击】
     * 现代顶级反作弊（如 ACE）为了防止外部读取和内存 Dump，会做两件极其恶心的事情：
     *
     *   攻击手段 1：VMA 碎裂
     *   反作弊为了 Hook 游戏内部函数，会高频调用 mprotect() 修改代码段的权限。
     *   Linux 内核为了管理不同的物理页权限，被迫将原本 1 个巨大的 RX 代码段，
     *   “劈碎”成了几十甚至上百个细碎的 VMA（虚拟内存区域），并且有些页被改成了 RWX 混合权限。
     *   这导致原本连贯的天然区段被无数个碎片彻底打乱。
     *
     *   攻击手段 2：远端假诱饵
     *   反作弊会在距离真实模块上百 MB 远的极低地址（例如 0x6e32250000），
     *   凭空 mmap() 申请一块虚假内存，并将其命名为 libil2cpp.so，权限设为 RO。
     *   如果我们使用常规的合并算法，会误把这个极远的假地址当成模块的起始地址，
     *   从而导致算出的 Base Address 完全错误（偏离真实基址几十上百MB），读取指针全部失效。
     *
     * 【第三阶段：我们的对抗算法 (六步降维打击)】
     * 为了获取绝对精准的真实基址并完美还原天然布局，我们采用以下物理与拓扑结合的算法：
     *
     *   步骤 1：纯物理排序
     *   无视所有权限和假象，直接把同名内存块按物理起始地址 (start) 绝对升序排列。
     *
     *   步骤 2：改进版体积聚类 (寻找生命主干)
     *   由于 ARM64 寻址限制，真实的 .so 内存必须紧凑地挨在一起。
     *   我们遍历内存块，一旦发现相邻块“缝隙”超过 16MB (0x1000000)，即视为“内存断层”，划分出不同群落。
     *   累加每个群落的真实映射体积，体积最大、最丰满的群落，绝对是真实的 .so 本体！
     *
     *   步骤 3：物理抹杀假诱饵
     *   锁定真实本体的 [best_base, best_end] 范围，将范围之外的所有假诱饵碎片从数组中彻底剔除。
     *
     *   步骤 4：严谨拓扑标记 (核心：破解权限篡改)
     *   反作弊把代码段的权限改得乱七八糟，如何还原？我们寻找天然的“防波堤”！
     *   先向后扫描找到第一个“纯原生数据段 (有W无X)”。在这个数据段之前的所有碎片（除了第0个Header），
     *   无论它现在的权限是 RO 还是 RWX，它在物理拓扑上必定属于“核心代码段”！
     *   强制将其内部拓扑标签重置为 0(RX)。跨过数据段后，则恢复原生判定，绝不越界吞噬。
     *
     *   步骤 5：拉链式精准缝合 (还原原生边界)
     *   遍历洗白后的碎片，只有当相邻碎片【首尾绝对相连】且【拓扑标签一致】时，
     *   才进行无缝拉链式融合。天然的段边界（如 RX 走到 RO）则会自然断开保留。
     *
     *   步骤 6：最终 Index 序列化
     *   给缝合后的完美区段重新发放 0, 1, 2, 3... 的连续 Index（保留 BSS 的 -1）。
     *
     * 【最终战果】：
     * 无论反作弊怎么切分、怎么放诱饵，跑完此算法后，产出结果与干净手机上的原生 ELF 映射 1:1 完全一致！
     * 外部辅助只需无脑调用：Base = info->modules[X].segs[0].start，即可获取绝对真实的基址！ */

    for (i = 0; i < info->module_count; i++)
    {
        struct module_info *m = &info->modules[i];

        if (m->seg_count > 0)
        {
            /* --- 1. 纯物理地址排序 --- */
            // 应对极端的反作弊内核级乱序映射干扰
            for (int x = 1; x < m->seg_count; x++)
            {
                struct segment_info key = m->segs[x];
                int y = x - 1;
                // 按物理起始地址绝对升序排列
                while (y >= 0 && m->segs[y].start > key.start)
                {
                    m->segs[y + 1] = m->segs[y];
                    y--;
                }
                m->segs[y + 1] = key;
            }

            /* --- 2. 改进版体积聚类 (寻找生命主干) --- */
            uint64_t current_base = m->segs[0].start;
            uint64_t current_end = m->segs[0].end;
            uint64_t current_volume = m->segs[0].end - m->segs[0].start;

            uint64_t max_volume = 0;
            uint64_t best_base = current_base;
            uint64_t best_end = current_end;

            for (j = 1; j < m->seg_count; j++)
            {
                // 增加 start >= current_end，防止反作弊恶意的错位映射引发的 uint64_t 无符号下溢出
                if (m->segs[j].start >= current_end && (m->segs[j].start - current_end > 0x1000000))
                {
                    // 结算上一个群落，看是不是体积最大的（真正的 so 本体）
                    if (current_volume > max_volume)
                    {
                        max_volume = current_volume;
                        best_base = current_base;
                        best_end = current_end;
                    }
                    // 开启新群落
                    current_base = m->segs[j].start;
                    current_end = m->segs[j].end;
                    current_volume = m->segs[j].end - m->segs[j].start;
                }
                else
                {
                    // 严谨计算增量体积。防止反作弊在同一区域内疯狂套娃重叠映射，导致体积翻倍虚高
                    if (m->segs[j].end > current_end)
                    {
                        uint64_t increment_start = (m->segs[j].start > current_end) ? m->segs[j].start : current_end;
                        current_volume += (m->segs[j].end - increment_start);
                        current_end = m->segs[j].end;
                    }
                }
            }
            // 结算最后一个群落
            if (current_volume > max_volume)
            {
                best_base = current_base;
                best_end = current_end;
            }

            /* --- 3. 物理抹杀假诱饵 --- */
            int valid_count = 0;
            for (j = 0; j < m->seg_count; j++)
            {
                if (m->segs[j].start >= best_base && m->segs[j].end <= best_end)
                {
                    m->segs[valid_count++] = m->segs[j];
                }
            }
            m->seg_count = valid_count;

            if (m->seg_count == 0)
                continue;

            /* --- 4. 严谨拓扑标记 --- */
            int first_data_idx = -1;

            // 寻找天然的数据段边界 (纯 RW 段，防跨界吞噬)
            for (j = 0; j < m->seg_count; j++)
            {
                if (m->segs[j].index == -1)
                    continue; // 忽略 BSS

                // 包含写权限(W=2) 且 不包含执行权限(X=4)，必定是原生的 .data 或 .bss 数据段
                if ((m->segs[j].prot & 2) && !(m->segs[j].prot & 4))
                {
                    first_data_idx = j;
                    break;
                }
            }

            // 精细化拓扑打标
            // 内部临时约定分类：1=RO(头部/只读), 0=RX(核心代码), 2=RW(数据)
            for (j = 0; j < m->seg_count; j++)
            {
                if (m->segs[j].index == -1)
                    continue; // 忽略 BSS

                if (j == 0)
                {
                    // 第 0 段：如果是纯 RO (无X且无W)，必定是 ELF Header (PT_LOAD[0])
                    if (!(m->segs[j].prot & 4) && !(m->segs[j].prot & 2))
                    {
                        m->segs[j].index = 1;
                    }
                    else if (m->segs[j].prot & 4)
                    {
                        m->segs[j].index = 0; // 异形 payload (无 Header，直接是代码)
                    }
                    else
                    {
                        m->segs[j].index = 2; // 极端罕见: 开头就是 RW
                    }
                }
                else if (first_data_idx != -1 && j < first_data_idx)
                {
                    // 在 Header 和第一个 Data 段之间的所有碎片 全部强制判定为核心代码段
                    m->segs[j].index = 0;
                }
                else
                {
                    // 跨过数据段边界后，恢复原生的权限分类，绝不吞噬后续段落
                    if (m->segs[j].prot & 4)
                    {
                        m->segs[j].index = 0; // 尾部独立的 RX (如 PT_LOAD[4])
                    }
                    else if (m->segs[j].prot & 2)
                    {
                        m->segs[j].index = 2; // 原生数据段 RW (如 PT_LOAD[2] / PT_LOAD[3])
                    }
                    else
                    {
                        m->segs[j].index = 1; // 尾部的其他只读段 RO
                    }
                }
            }

            /* --- 5. 拉链式精准缝合 --- */
            int out_idx = 0;
            for (j = 1; j < m->seg_count; j++)
            {
                struct segment_info *prev_seg = &m->segs[out_idx];
                struct segment_info *curr_seg = &m->segs[j];

                if (prev_seg->end == curr_seg->start && prev_seg->index == curr_seg->index)
                {
                    prev_seg->end = curr_seg->end;
                    prev_seg->prot |= curr_seg->prot;
                }
                else
                {
                    out_idx++;
                    // 避免不必要的自我覆盖
                    if (out_idx != j)
                    {
                        m->segs[out_idx] = *curr_seg;
                    }
                }
            }
            m->seg_count = out_idx + 1;

            /* --- 6. 最终 Index 序列化 --- */
            seq = 0;
            for (j = 0; j < m->seg_count; j++)
            {
                if (m->segs[j].index != -1) // 保持 BSS 的 -1 标识不变
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
