#ifndef ARM64_GHOST_REGION_H
#define ARM64_GHOST_REGION_H

#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <asm/pgtable.h>
#include <asm/pgtable-prot.h>

#include "export_fun.h"

#define ARM64_GHOST_REGION_RX_FLAGS (PTE_TYPE_PAGE | PTE_VALID | PTE_AF | PTE_SHARED | PTE_USER | PTE_RDONLY | PTE_PXN | PTE_NG | PTE_ATTRINDX(MT_NORMAL))

/*
无 VMA 的 PTE-backed 用户执行区。
create 接收完整机器码，写入新物理页后直接安装用户只读可执行 PTE。
不登记映射，不长期持有 task/mm；每次按当前 VMA 布局重新选址。
若选中的空洞已有无 VMA PTE，则清除后覆盖。
*/

// 在已持有 mmap 写锁时查找能够容纳指定大小的最高页对齐 VMA 空洞，失败返回 0。
static inline u64 arm64_ghost_region_find_hole_locked(struct mm_struct *mm, size_t size)
{
    struct vm_area_struct *vma;
    u64 user_hi;
    u64 addr;
    u64 best = 0;

    if (!mm || !size || (size & ~PAGE_MASK)) return 0;

    user_hi = (u64)READ_ONCE(mm->task_size) & PAGE_MASK;
    if (user_hi <= PAGE_SIZE || size > user_hi - PAGE_SIZE) return 0;

    addr = PAGE_SIZE;
    while (addr <= user_hi - size)
    {
        u64 gap_end;
        u64 next;

        vma = find_vma(mm, addr);
        if (!vma || (u64)vma->vm_start >= user_hi)
        {
            best = user_hi - size;
            break;
        }

        gap_end = (u64)vma->vm_start & PAGE_MASK;
        if (gap_end >= addr && gap_end - addr >= size) best = gap_end - size;

        if ((u64)vma->vm_end >= user_hi) break;
        next = PAGE_ALIGN((u64)vma->vm_end);
        if (next <= addr) return 0;
        addr = next;
    }

    return best;
}

// 刷新指定用户虚拟地址范围在所有 ASID 中的 TLB 缓存。
static inline void arm64_ghost_region_flush_tlb(u64 user_va, unsigned int page_count)
{
    unsigned int index;

    for (index = 0; index < page_count; index++) flush_user_tlb_addr_all_asid(user_va + (u64)index * PAGE_SIZE);
}

// 在已持有 mmap 写锁时补齐页表，并按 break-before-make 顺序安装用户只读可执行 PTE。
static inline int arm64_ghost_region_install_locked(struct mm_struct *mm, struct page **pages, unsigned int page_count, u64 user_va)
{
    unsigned int index;

    for (index = 0; index < page_count; index++)
        if (!get_or_alloc_user_pte(mm, user_va + (u64)index * PAGE_SIZE)) return -ENOMEM;

    for (index = 0; index < page_count; index++)
    {
        u64 addr = user_va + (u64)index * PAGE_SIZE;
        pte_t *ptep = get_user_pte(mm, addr);

        pte_clear(mm, addr, ptep);
    }
    arm64_ghost_region_flush_tlb(user_va, page_count);

    for (index = 0; index < page_count; index++)
    {
        u64 addr = user_va + (u64)index * PAGE_SIZE;
        pte_t *ptep = get_user_pte(mm, addr);
        pte_t pte = mk_pte(pages[index], __pgprot(ARM64_GHOST_REGION_RX_FLAGS));

        set_pte_at(mm, addr, ptep, pte);
    }

    return 0;
}

// 从 CTR_EL0.DminLine 读取当前 CPU 的最小数据缓存行大小并返回字节数。
static inline unsigned long arm64_ghost_region_dcache_line_size(void)
{
    u64 ctr;

    asm volatile("mrs %0, ctr_el0" : "=r"(ctr));
    return 4UL << ((ctr >> 16) & 0xf);
}

// 同步机器码对应的 D-cache 和 I-cache，确保后续取指能够看到新写入的内容。
static inline void arm64_ghost_region_sync_icache(struct page **pages, unsigned int page_count, size_t code_size)
{
    unsigned long line_size = arm64_ghost_region_dcache_line_size();
    size_t remaining = code_size;
    unsigned int index;

    for (index = 0; index < page_count && remaining; index++)
    {
        size_t bytes = min_t(size_t, remaining, PAGE_SIZE);
        void *mapping = page_address(pages[index]);
        unsigned long start = (unsigned long)mapping;
        unsigned long end = start + bytes;
        unsigned long line;

        for (line = start & ~(line_size - 1); line < end; line += line_size) asm volatile("dc cvau, %0" : : "r"(line) : "memory");

        remaining -= bytes;
    }

    asm volatile("dsb ish\n\t"
                 "ic ialluis\n\t"
                 "dsb ish\n\t"
                 "isb\n\t"
                 :
                 :
                 : "memory");
}

// 为目标进程创建无 VMA 的用户 RX 区域并写入完整机器码，成功返回用户虚拟地址，失败返回 0。
static inline u64 arm64_ghost_region_create(pid_t pid, const void *code, size_t size)
{
    struct mm_struct *mm;
    struct page **pages;
    const u8 *source = code;
    size_t mapped_size;
    size_t page_count_size;
    size_t remaining = size;
    unsigned int page_count;
    unsigned int allocated_count = 0;
    unsigned int index;
    u64 user_va = 0;
    int status = -ENOMEM;

    if (!code || !size || pid <= 0) return 0;
    if (size > SIZE_MAX - (PAGE_SIZE - 1)) return 0;

    mapped_size = PAGE_ALIGN(size);
    page_count_size = mapped_size >> PAGE_SHIFT;
    if (!page_count_size || page_count_size > UINT_MAX) return 0;
    page_count = (unsigned int)page_count_size;

    mm = get_mm_by_pid(pid);
    if (!mm) return 0;

    pages = kvcalloc(page_count, sizeof(*pages), GFP_KERNEL);
    if (!pages) goto err_release;

    for (index = 0; index < page_count; index++)
    {
        pages[index] = alloc_page(GFP_KERNEL | __GFP_ZERO);
        if (!pages[index]) goto err_release;
        allocated_count++;
    }

    for (index = 0; index < page_count && remaining; index++)
    {
        size_t chunk = min_t(size_t, remaining, PAGE_SIZE);
        void *mapping = page_address(pages[index]);

        __builtin_memcpy(mapping, source, chunk);
        source += chunk;
        remaining -= chunk;
    }

    arm64_ghost_region_sync_icache(pages, page_count, size);

    mmap_write_lock(mm);
    if (atomic_read(&mm->mm_users) <= 1) status = -ESRCH;
    else
    {
        user_va = arm64_ghost_region_find_hole_locked(mm, mapped_size);
        if (!user_va) status = -ENOSPC;
        else status = arm64_ghost_region_install_locked(mm, pages, page_count, user_va);
    }
    mmap_write_unlock(mm);

    if (status) goto err_release;

    arm64_ghost_region_flush_tlb(user_va, page_count);
    kvfree(pages);
    mmput(mm);
    return user_va;

err_release:
    while (allocated_count > 0)
    {
        allocated_count--;
        __free_page(pages[allocated_count]);
    }
    kvfree(pages);
    mmput(mm);
    return 0;
}

#endif