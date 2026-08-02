#ifndef ARM64_GHOST_REGION_H
#define ARM64_GHOST_REGION_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <asm/pgtable.h>
#include <asm/pgtable-prot.h>

#include "../export_fun.h"
/*
该模块头文件实现无虚拟内存区域描述符、由页表项支撑的用户执行区。
区域不创建 vm_area_struct，而是在目标进程现有 VMA 之间寻找连续空洞，并直接安装末级 PTE。

区域的分配和安装流程如下：
1. 将请求长度向上按 PAGE_SIZE 对齐，分配对应数量的清零物理页。
2. 获取目标进程创建时的 mm_struct，并在内存映射写锁下扫描用户地址空间。
3. 候选地址必须同时不属于任何 VMA 且对应范围内没有有效 PTE；near 非零时优先选择距离
    near 最近的空洞，near 为零时优先选择高地址空洞。
4. 补齐目标范围所需的页表层级，再次确认所有末级 PTE 为空后，将物理页连续映射到目标地址。
5. 映射权限为用户只读可执行、内核不可执行、非全局的普通可缓存内存，安装后刷新对应 TLB。

区域支持一次性跨页写入机器码，也支持按 32 位指令索引局部写入。完整写入会清零所有映射页、
复制指定长度的数据并同步 D-cache 与所有 CPU 的 I-cache；局部写入后由调用者显式调用同步接口。

区域句柄保存物理页数组、创建时的 mm_struct、用户虚拟地址和映射长度。销毁时只清除仍然指向
本区域物理页的 PTE，再刷新 TLB、释放物理页并释放 mm 引用，避免进程号复用或目标地址后来被
其它映射占用时误删不属于本模块的页表项。

本模块只管理物理页、用户虚拟地址、页表映射和机器码缓存同步，不解释机器码内容，不执行指令
重定位，也不决定监控地址或断点处理策略。
*/

/*
PTE_TYPE_PAGE：表示末级页表项映射一个物理页。
PTE_VALID：表示页表项有效。
PTE_AF：预先设置访问标志，避免首次访问触发访问标志异常。
PTE_SHARED：将页面设置为多核共享属性。
PTE_USER：允许用户态访问页面。
PTE_RDONLY：禁止用户态写入页面。
PTE_PXN：禁止内核态执行页面中的机器码。
PTE_NG：将映射标记为非全局，使 TLB 项绑定当前地址空间标识符。
PTE_ATTRINDX(MT_NORMAL)：将页面设置为普通可缓存内存。
未设置 PTE_UXN：允许用户态执行页面中的机器码。
*/
#define ARM64_GHOST_REGION_RX_FLAGS (PTE_TYPE_PAGE | PTE_VALID | PTE_AF | PTE_SHARED | PTE_USER | PTE_RDONLY | PTE_PXN | PTE_NG | PTE_ATTRINDX(MT_NORMAL))

struct arm64_ghost_region
{
    struct page **pages;     // 幽灵区域占用的物理页
    struct mm_struct *mm;    // 所属进程的地址空间
    uint64_t user_va;        // 映射后的用户虚拟地址
    size_t mapped_size;      // 映射长度
    unsigned int page_count; // 物理页数量
};

// 在指定虚拟内存区域空隙内，从 near 附近双向查找可容纳目标区域的连续空页表项。
static inline uint64_t arm64_ghost_region_find_empty_in_gap_locked(struct mm_struct *mm, uint64_t gap_start, uint64_t gap_end, size_t size, uint64_t near)
{
    gap_start = PAGE_ALIGN(gap_start);
    gap_end &= PAGE_MASK;
    if (gap_end < gap_start || gap_end - gap_start < size) return 0;

    uint64_t max_start = gap_end - size;
    uint64_t near_page = near & PAGE_MASK;
    uint64_t candidate = near ? clamp_t(uint64_t, near_page, gap_start, max_start) : max_start;
    uint64_t lower = candidate;
    uint64_t upper = candidate;

    for (;;)
    {
        if (user_pte_range_empty(mm, lower, size)) return lower;

        bool moved = false;
        if (lower >= gap_start + PAGE_SIZE)
        {
            lower -= PAGE_SIZE;
            moved = true;
        }
        if (upper <= max_start - PAGE_SIZE)
        {
            upper += PAGE_SIZE;
            if (user_pte_range_empty(mm, upper, size)) return upper;
            moved = true;
        }
        if (!moved) return 0;
    }
}

// 在整个用户VMA地址空间内， 已持有内存映射写锁时查找无区域描述符且无有效页表项的页对齐空洞；near 为 0 时优先高地址。
static inline uint64_t arm64_ghost_region_find_hole_locked(struct mm_struct *mm, size_t size, uint64_t near)
{
    if (!mm || !size || (size & ~PAGE_MASK)) return 0;

    uint64_t user_hi = (uint64_t)READ_ONCE(mm->task_size) & PAGE_MASK;
    if (user_hi <= PAGE_SIZE || size > user_hi - PAGE_SIZE) return 0;

    uint64_t addr = PAGE_SIZE;
    uint64_t best = 0;
    uint64_t best_distance = U64_MAX;

    while (addr <= user_hi - size)
    {
        struct vm_area_struct *vma = find_vma(mm, addr);
        uint64_t gap_end = vma && (uint64_t)vma->vm_start < user_hi ? (uint64_t)vma->vm_start : user_hi;
        uint64_t candidate = arm64_ghost_region_find_empty_in_gap_locked(mm, addr, gap_end, size, near);

        if (candidate)
        {
            if (!near) best = candidate;
            else
            {
                uint64_t near_page = near & PAGE_MASK;
                uint64_t distance = candidate > near_page ? candidate - near_page : near_page - candidate;

                if (distance < best_distance)
                {
                    best = candidate;
                    best_distance = distance;
                }
            }
        }

        if (!vma || (uint64_t)vma->vm_start >= user_hi) break;

        if ((uint64_t)vma->vm_end >= user_hi) break;
        uint64_t next = PAGE_ALIGN((uint64_t)vma->vm_end);
        if (next <= addr) return 0;
        addr = next;
    }

    return best;
}

// 在已持有内存映射写锁时补齐页表，并仅在空页表项上安装用户只读可执行映射。
static inline int arm64_ghost_region_install_locked(struct mm_struct *mm, struct page **pages, unsigned int page_count, uint64_t user_va)
{
    for (unsigned int index = 0; index < page_count; index++)
        if (!get_or_alloc_user_pte(mm, user_va + (uint64_t)index * PAGE_SIZE)) return -ENOMEM;

    for (unsigned int index = 0; index < page_count; index++)
    {
        uint64_t addr = user_va + (uint64_t)index * PAGE_SIZE;
        pte_t *ptep = get_user_pte(mm, addr);

        if (pte_present(READ_ONCE(*ptep))) return -EEXIST;
    }

    for (unsigned int index = 0; index < page_count; index++)
    {
        uint64_t addr = user_va + (uint64_t)index * PAGE_SIZE;
        pte_t *ptep = get_user_pte(mm, addr);
        pte_t pte = mk_pte(pages[index], __pgprot(ARM64_GHOST_REGION_RX_FLAGS));

        // set_pte_at() 会在新 GKI 中引入模块不可用的 I-cache/cont-PTE helper；机器码写入后由本模块显式同步缓存。
        set_pte(ptep, pte);
    }

    flush_tlb_range_all_asid_all_cpus(user_va, user_va + (uint64_t)page_count * PAGE_SIZE);
    return 0;
}

/*
以下函数是提供给外部模块调用的幽灵区域操作接口=========================================================================
*/

// 分配并安装无区域描述符的用户只读可执行区域；near 为 0 时优先使用最高地址空洞。
static inline int arm64_ghost_region_reserve(pid_t pid, size_t size, uint64_t near, struct arm64_ghost_region *region)
{
    if (!region || !size || pid <= 0) return -EINVAL;
    if (region->pages || region->user_va) return -EBUSY;
    if (size > SIZE_MAX - (PAGE_SIZE - 1)) return -EOVERFLOW;

    size_t mapped_size = PAGE_ALIGN(size);
    size_t page_count_size = mapped_size >> PAGE_SHIFT;
    if (page_count_size > UINT_MAX) return -EOVERFLOW;
    unsigned int page_count = (unsigned int)page_count_size;

    struct mm_struct *mm = get_mm_by_pid(pid);
    if (!mm) return -ESRCH;

    struct page **pages = kvcalloc(page_count, sizeof(*pages), GFP_KERNEL);
    if (!pages)
    {
        mmput(mm);
        return -ENOMEM;
    }

    unsigned int allocated_count = 0;
    for (unsigned int index = 0; index < page_count; index++)
    {
        pages[index] = alloc_page(GFP_KERNEL | __GFP_ZERO);
        if (!pages[index])
        {
            while (allocated_count > 0) __free_page(pages[--allocated_count]);
            kvfree(pages);
            mmput(mm);
            return -ENOMEM;
        }
        allocated_count++;
    }

    uint64_t user_va = 0;
    int status;
    mmap_write_lock(mm);
    if (atomic_read(&mm->mm_users) <= 1) status = -ESRCH;
    else
    {
        user_va = arm64_ghost_region_find_hole_locked(mm, mapped_size, near);
        if (!user_va) status = -ENOSPC;
        else status = arm64_ghost_region_install_locked(mm, pages, page_count, user_va);
    }
    mmap_write_unlock(mm);

    if (status) goto err_release;

    *region = (struct arm64_ghost_region){
        .pages = pages,
        .mm = mm,
        .user_va = user_va,
        .mapped_size = mapped_size,
        .page_count = page_count,
    };
    return 0;

err_release:
    while (allocated_count > 0)
    {
        allocated_count--;
        __free_page(pages[allocated_count]);
    }
    kvfree(pages);
    mmput(mm);
    return status;
}

// 将完整机器码写入已保留区域并同步指令缓存；未使用的尾部保持为零。
static inline int arm64_ghost_region_write(struct arm64_ghost_region *region, const void *code, size_t size)
{
    if (!region || !region->pages || !region->user_va || !code || !size) return -EINVAL;
    if (size > region->mapped_size) return -E2BIG;

    const uint8_t *source = code;
    size_t remaining = size;

    for (unsigned int index = 0; index < region->page_count; index++)
    {
        void *mapping = page_address(region->pages[index]);
        size_t chunk = min_t(size_t, remaining, PAGE_SIZE);

        __builtin_memset(mapping, 0, PAGE_SIZE);
        if (chunk)
        {
            __builtin_memcpy(mapping, source, chunk);
            source += chunk;
            remaining -= chunk;
        }
    }

    return arm64_sync_code_pages_all_cpus(region->pages, region->page_count, region->mapped_size);
}

// 在幽灵区域内按指令索引写入一个 32 位机器码，不立即同步指令缓存。
static inline int arm64_ghost_region_store32(struct arm64_ghost_region *region, uint32_t word_index, uint32_t value)
{
    if (!region || !region->pages || !region->user_va) return -EINVAL;
    if ((uint64_t)word_index * sizeof(uint32_t) >= region->mapped_size) return -E2BIG;

    uint32_t words_per_page = PAGE_SIZE / sizeof(uint32_t);
    unsigned int page_index = word_index / words_per_page;
    uint32_t page_word_index = word_index % words_per_page;
    uint32_t *mapping = page_address(region->pages[page_index]);

    mapping[page_word_index] = value;
    return 0;
}

// 将指定长度的已写入机器码同步到指令缓存。
static inline int arm64_ghost_region_sync(struct arm64_ghost_region *region, size_t code_size)
{
    if (!region || !region->pages || !region->user_va || !code_size) return -EINVAL;
    if (code_size > region->mapped_size) return -E2BIG;

    return arm64_sync_code_pages_all_cpus(region->pages, region->page_count, code_size);
}

// 清除仍指向本区域物理页的页表项，并释放全部物理页。
static inline void arm64_ghost_region_destroy(struct arm64_ghost_region *region)
{
    if (!region) return;

    if (region->pages && region->user_va && region->mm)
    {
        struct mm_struct *mm = region->mm;

        mmap_write_lock(mm);
        for (unsigned int index = 0; index < region->page_count; index++)
        {
            uint64_t addr = region->user_va + (uint64_t)index * PAGE_SIZE;
            pte_t *ptep = get_user_pte(mm, addr);

            if (!ptep) continue;
            pte_t pte = READ_ONCE(*ptep);
            // 本模块创建的幽灵 PTE 从不带 PTE_CONT，直接清零可避免 pte_clear() 引入 cont-PTE helper。
            if (pte_present(pte) && pte_pfn(pte) == page_to_pfn(region->pages[index])) set_pte(ptep, __pte(0));
        }
        mmap_write_unlock(mm);
        flush_tlb_range_all_asid_all_cpus(region->user_va, region->user_va + region->mapped_size);
    }

    if (region->pages)
    {
        for (unsigned int index = 0; index < region->page_count; index++)
            if (region->pages[index]) __free_page(region->pages[index]);
        kvfree(region->pages);
    }
    if (region->mm) mmput(region->mm);

    __builtin_memset(region, 0, sizeof(*region));
}

#endif