#ifndef PROCESS_MEMORY_ENUM_H
#define PROCESS_MEMORY_ENUM_H

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/kallsyms.h>

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
        if (__builtin_strcmp((const char *)modules[i].name, (const char *)name) == 0)
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
    int last_mod_idx = -1;
    int i, j;
    short seq;
    bool excluded, mod_accepted;

    /*
    开始解释:为何我白名单把/dev/加入到模块列表了
    正常运行情况下:编译好的静态代码就放在模块里面，运行时加载到内存并分配文件映射型代码
    被保护隐藏的代码:
            我发现目标程序会用mmap通过open("/dev/zero")映射 /dev/zero 设备文件，
            来获取一段初始全为 0 的、可读、可写、可执行的共享虚拟内存，vma和maps显示/dev/zero (deleted)
            现在不需要获取文件描述符了，现代的mmap支持无设备描述符映射，
            但是创建的内存页映射来源在vma和maps显示的还是/dev/zero (deleted)
            并使用mprotect来修改权限，并在代码页上下页修改为无任何权限的保护页
    还有这个存放代码的动态内存几乎是mmap分配的，不会出现malloc分配内存来存放代码
    不用malloc有这些原因:
            1.mprotect要求起始地址必须是页的整数倍(4kb页对齐), malloc 是8字节或 16 字节对齐不满足,必须手动计算页边界，或者使用 posix_memalign,mmap分配的天然页对齐
            2.malloc返回的指针前几个字节带着内存块的控制信息(如大小、状态...),
              mprotect页改只读后free会写入控制信息触发异常崩，代码段释放到内存后一般权限就会改为rx-p,没有写权限
              但在这里观察都是rwxs-p全部给上
            3.mprotect按页生效，对10字节改权限，内核也会把10 字节所在的整个 4KB 页面全部改掉
              malloc内存可能和其他数据内存共享4KB页 ，改权限会意外导致程序其他部分异常，mmap按页分配安全
    所以加上/dev可以
        1.libso[0]+静态指针或函数地址偏移，
        2.libso[bss]+静态指针偏移
        3./dev/zero (deleted)+静态指针或函数地址偏移
        4.直接dump/dev/zero (deleted)内存字节就可获取释放到内存中代码

    这里在说一下这个模块内的静态指针偏移
        众所周知一个二进制文件在linux下要求ELF格式，在windows下要求PE格式
        这些格式核心就是二进制的节区分段：代码段(text), 数据段(data),只读数据段(ReadOnly Data),BSS段

        char *g_pData=NULL;是已初始化(显式初始为0)
        编译器放在 .data 段的指针，用区段地址+相对偏移获取: libc.so[0]+0x150000
        char *g_pData;     是未初始化(语言标准来保证它为 0)
        编译器放在 .bss 段的指针，用区段bss+相对偏移获取：libc.so[bss]+0x10000

        现代clang对于这种显式初始为0的变量进行优化了
        把已初始化和未初始化全放在.bss段来节省.data段的磁盘占用
        g_pData这种全局指针只有给非0的初始值，才会固定到.data

    这里在说一下不同起始区段到达同一个指针
        0xb40000=libc.so[0]+0x2222
        0xb40000=libc.so[1]+0x1111
        这种的段0和段1获取到的都是同一个指针
        想象:
            你在作业本纸中间写上地址0xb40000
            从第0行(libc.so[0])到中间的距离就0x2222
            从第1行(libc.so[1])到中间的距离就0x1111
        目标一样，区别就是起始地址不同，相对偏移根据起始地址位置进行变化
        */

    // 白名单(用于收集模块地址)，只搜集...开头虚拟地址区间
    static const char *const mod_include_prefixes[] = {
        "/data/", "/dev/", NULL};

    // 黑名单(用于收集可扫描内存地址)，排除...开头的虚拟地址区间
    static const char *const excl_prefixes[] = {
        "/dev/", "/system/", "/vendor/", "/apex/", NULL};
    // 黑名单(用于收集可扫描内存地址)，排除指定关键字的虚拟地址区间
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
        uint8_t current_prot = 0;
        if (vma->vm_flags & VM_READ)
            current_prot |= 1;
        if (vma->vm_flags & VM_WRITE)
            current_prot |= 2;
        if (vma->vm_flags & VM_EXEC)
            current_prot |= 4;

        /* ========== 模块收集 ========== */

        /*
         * BSS 检测条件：
         *   - 无文件映射（匿名页）
         *   - 含写权限（VM_WRITE）即可，不强求可读。
         *     ACE 反作弊会将 BSS 权限故意设为 -w-p（只写无读），
         *     原先的 VMA_IS_RW 宏要求同时具备读写，导致此类 BSS 被漏掉。
         *   - 与上一个 VMA 首尾严格相连（vm_start == prev->vm_end）
         *   - 上一个 VMA 属于我们正在追踪的模块（last_mod_idx >= 0）
         */
        if (prev && !vma->vm_file && vma->vm_start == prev->vm_end &&
            (vma->vm_flags & VM_WRITE) && last_mod_idx >= 0)
        {
            add_seg(&info->modules[last_mod_idx], -1, current_prot, vma->vm_start, vma->vm_end);
            /*
             * BSS 收集后继续保持 last_mod_idx 有效。
             * 这样如果 BSS 后面紧跟着更多匿名 RW 碎片（极少见但存在），
             * 也能被一并归入同一模块。
             */
        }
        else if (vma->vm_file)
        {
            last_mod_idx = -1;

            path = d_path(&vma->vm_file->f_path, path_buf, PATH_MAX);
            if (!IS_ERR(path))
            {
                mod_accepted = false;
                for (i = 0; mod_include_prefixes[i]; i++)
                {
                    if (__builtin_strncmp(path, mod_include_prefixes[i],
                                          __builtin_strlen(mod_include_prefixes[i])) == 0)
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
                        add_seg(&info->modules[last_mod_idx], 0, current_prot, vma->vm_start, vma->vm_end);
                    }
                }
            }
        }
        else
        {
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
                        if (__builtin_strncmp(path, excl_prefixes[i],
                                              __builtin_strlen(excl_prefixes[i])) == 0)
                        {
                            excluded = true;
                            break;
                        }
                    if (!excluded)
                        for (i = 0; excl_keywords[i]; i++)
                            if (__builtin_strstr(path, excl_keywords[i]))
                            {
                                excluded = true;
                                break;
                            }
                }
            }
            else
            {
                if (mm->start_stack >= vma->vm_start &&
                    mm->start_stack < vma->vm_end)
                    excluded = true;

                if (!excluded && vma->vm_ops && vma->vm_ops->name)
                {
                    const char *vma_name = vma->vm_ops->name(vma);
                    if (vma_name &&
                        (__builtin_strcmp(vma_name, "[vvar]") == 0 ||
                         __builtin_strcmp(vma_name, "[vdso]") == 0 ||
                         __builtin_strcmp(vma_name, "[vsyscall]") == 0))
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
     *
     * 当 Android 原生加载一个 libil2cpp.so 时，内存布局连续且规律。
     * 现代 64 位 Android（LLVM/Clang 编译）出于安全考虑，至少产生以下几个连续段：
     *
     *   PT_LOAD[0] (r--)  : ELF Header + 只读数据（.rodata、.eh_frame 等），真实基址起点。
     *   PT_LOAD[1] (r-x)  : .text 代码段，核心逻辑所在。
     *   PT_LOAD[2] (rw-)  : .data.rel.ro + RELRO 安全页，写完重定位后锁为只读的数据。
     *   PT_LOAD[3] (rw-)  : .data 全局变量段。
     *   BSS        (-w-/rw-) : 尾部额外分配的匿名读写内存（零初始化全局变量）。
     *
     * 即便没有反作弊，最纯净的环境也自然产生 [RO -> RX -> RW -> RW -> RW(anon)] 的天然区段。
     *
     * 【第二阶段：顶级反作弊的四重攻击手段】
     *
     *   攻击一：VMA 碎裂
     *   反作弊高频调用 mprotect() Hook 游戏函数，内核被迫将原本一整块 RX 代码段
     *   "劈碎"成几十甚至上百个细碎 VMA，部分页被改为 RWX 混合权限，
     *   彻底打乱原本连贯的天然区段。
     *
     *   攻击二：远端假诱饵
     *   反作弊在距离真实模块上百 MB 远的极低地址（如 0x6e32250000）凭空 mmap()
     *   一块假内存，命名为 libil2cpp.so，权限设为 RO。
     *   常规合并算法会误把假地址当成模块基址，导致读取指针全部失效。
     *
     *   攻击三：prot 权限污染
     *   代码段内部散布着少量 RWX 碎片（反作弊自身的 trampoline hook 页）。
     *   若在缝合阶段对权限做 OR 合并，RWX 碎片的 W 位会"传染"整个代码段，
     *   使本该是 RX 的代码段最终呈现为 RWX，干扰上层对段类型的判断。
     *
     *   攻击四：BSS 权限异化
     *   ACE 反作弊将 BSS 段的权限故意设为 -w-p（只写，无读权限）。
     *   若 BSS 检测逻辑要求 VM_READ|VM_WRITE，则此类 BSS 完全不可见，
     *   导致上层计算出的模块尾部地址偏短，BSS 内的全局变量无法定位。
     *
     * 【第三阶段：七步对抗算法】
     *
     *   步骤 1：纯物理排序
     *   无视所有权限和假象，按物理起始地址绝对升序排列所有碎片。
     *   应对极端的反作弊内核级乱序映射干扰。
     *
     *   步骤 2：改进版体积聚类 (寻找生命主干)
     *   ARM64 寻址限制要求真实 .so 内存紧凑相连。遍历碎片，相邻块缝隙超过
     *   16MB (0x1000000) 即视为"内存断层"，划分不同群落。
     *   累加每个群落的真实映射体积（严防重叠映射导致体积虚高），
     *   体积最大、最丰满的群落即为真实的 .so 本体。
     *
     *   步骤 3：物理抹杀假诱饵 + BSS 豁免保留
     *   锁定真实本体的 [best_base, best_end] 范围，剔除范围外的假诱饵碎片。
     *   豁免：index==-1 的匿名 BSS 段即便 end 超出 best_end，
     *   只要 start 在 best_end 附近（≤ 0x3000，一个 guard 页的余量），
     *   就视为合法的本体尾部延伸，保留并动态扩展 best_end。
     *   这直接解决了 ACE 将 BSS 权限设为 -w-p 后尾部被误杀的问题。
     *
     *   步骤 4：严谨拓扑标记 (核心：破解权限篡改)
     *   寻找天然的"防波堤"：向后扫描找到第一个"纯原生数据段 (有W无X)"。
     *   在 Header 和第一个数据段之间的所有碎片，无论现在权限是 RO 还是 RWX，
     *   物理拓扑上必定属于"核心代码段"，强制内部标签重置为 0(RX)。
     *   跨过数据段后恢复原生判定，绝不越界吞噬。
     *   内部临时标签约定：1=RO(头部), 0=RX(代码), 2=RW(数据), -1=BSS(保持不变)
     *
     *   步骤 4.5：强制规范化 prot (消除权限污染)
     *   反作弊的 RWX hook 页在步骤 4 中已被正确归入代码段（index=0），
     *   但其 W 位仍残留在 prot 字段中。
     *   此步骤根据步骤 4 确立的权威拓扑标签，强制覆写每个碎片的 prot：
     *     index=1(Header/RELRO) → prot=1(R)
     *     index=0(Code)         → prot=5(RX)
     *     index=2(Data)         → prot=3(RW)
     *     index=-1(BSS)         → prot=3(RW)
     *   彻底断绝 prot 污染，使对外输出的 prot 与原生 ELF 加载完全一致。
     *
     *   步骤 5：拉链式精准缝合 (还原原生边界)
     *   遍历洗白后的碎片，仅当相邻碎片【首尾绝对相连】且【拓扑标签一致】时，
     *   进行无缝拉链式融合。天然的段边界（如 RX→RO、RO→RW）自然断开保留。
     *   缝合时不再合并 prot（步骤 4.5 已完成规范化，此处无需再动）。
     *
     *   步骤 6：最终 Index 序列化
     *   给缝合后的完美区段重新发放 0, 1, 2, 3... 的连续 Index，BSS 保留 -1。
     *
     * 【最终结果】：
     * 无论反作弊怎么切分、放诱饵、异化权限，跑完此算法后，
     * 产出结果与干净手机上的原生 ELF 映射 1:1 完全一致。
     *
     * 典型输出（libil2cpp.so，ACE 保护环境）：
     *   seg[0] index=0  prot=1(R)  → PT_LOAD[0] ELF Header
     *   seg[1] index=1  prot=5(RX) → PT_LOAD[1] .text 代码段
     *   seg[2] index=2  prot=3(RW) → PT_LOAD[2] .data.rel.ro
     *   seg[3] index=3  prot=1(R)  → RELRO 只读页
     *   seg[4] index=4  prot=3(RW) → PT_LOAD[3] .data
     *   seg[5] index=-1 prot=3(RW) → BSS (原始权限 -w-p，已被规范化)
     *   seg[6] index=5  prot=5(RX) → PT_LOAD[4]
     *   seg[7] index=6  prot=3(RW) → PT_LOAD[5]
     *   seg[8] index=7  prot=3(RW) → PT_LOAD[6]
     *
     * 外部调用：Base = info->modules[X].segs[0].start，即可获取绝对真实基址。
     * =========================================================================================
     */

    for (i = 0; i < info->module_count; i++)
    {
        struct module_info *m = &info->modules[i];

        if (m->seg_count > 0)
        {
            /* --- 步骤 1：纯物理地址排序 --- */
            for (int x = 1; x < m->seg_count; x++)
            {
                struct segment_info key = m->segs[x];
                int y = x - 1;
                while (y >= 0 && m->segs[y].start > key.start)
                {
                    m->segs[y + 1] = m->segs[y];
                    y--;
                }
                m->segs[y + 1] = key;
            }

            /* --- 步骤 2：改进版体积聚类 (寻找生命主干) --- */
            uint64_t current_base = m->segs[0].start;
            uint64_t current_end = m->segs[0].end;
            uint64_t current_volume = m->segs[0].end - m->segs[0].start;

            uint64_t max_volume = 0;
            uint64_t best_base = current_base;
            uint64_t best_end = current_end;

            for (j = 1; j < m->seg_count; j++)
            {
                if (m->segs[j].start >= current_end &&
                    (m->segs[j].start - current_end > 0x1000000))
                {
                    if (current_volume > max_volume)
                    {
                        max_volume = current_volume;
                        best_base = current_base;
                        best_end = current_end;
                    }
                    current_base = m->segs[j].start;
                    current_end = m->segs[j].end;
                    current_volume = m->segs[j].end - m->segs[j].start;
                }
                else
                {
                    if (m->segs[j].end > current_end)
                    {
                        uint64_t increment_start = (m->segs[j].start > current_end)
                                                       ? m->segs[j].start
                                                       : current_end;
                        current_volume += (m->segs[j].end - increment_start);
                        current_end = m->segs[j].end;
                    }
                }
            }
            if (current_volume > max_volume)
            {
                best_base = current_base;
                best_end = current_end;
            }

            /* --- 步骤 3：物理抹杀假诱饵 + BSS 豁免保留 --- */
            /*
             * 常规判定：碎片必须完整落在 [best_base, best_end] 内。
             * BSS 豁免：index==-1 的匿名段，start 在 best_end 附近（≤0x3000）
             * 即视为本体尾部延伸，保留并动态扩展 best_end，
             * 防止后续 BSS 碎片因 end 超界而被误杀。
             */
            int valid_count = 0;
            for (j = 0; j < m->seg_count; j++)
            {
                if (m->segs[j].start >= best_base && m->segs[j].end <= best_end)
                {
                    m->segs[valid_count++] = m->segs[j];
                }
                else if (m->segs[j].index == -1 &&
                         m->segs[j].start >= best_base &&
                         m->segs[j].start <= best_end + 0x3000)
                {
                    m->segs[valid_count++] = m->segs[j];
                    if (m->segs[j].end > best_end)
                        best_end = m->segs[j].end;
                }
            }
            m->seg_count = valid_count;

            if (m->seg_count == 0)
                continue;

            /* --- 步骤 4：严谨拓扑标记 --- */
            int first_data_idx = -1;

            for (j = 0; j < m->seg_count; j++)
            {
                if (m->segs[j].index == -1)
                    continue;

                if ((m->segs[j].prot & 2) && !(m->segs[j].prot & 4))
                {
                    first_data_idx = j;
                    break;
                }
            }

            for (j = 0; j < m->seg_count; j++)
            {
                if (m->segs[j].index == -1)
                    continue;

                if (j == 0)
                {
                    if (!(m->segs[j].prot & 4) && !(m->segs[j].prot & 2))
                        m->segs[j].index = 1;
                    else if (m->segs[j].prot & 4)
                        m->segs[j].index = 0;
                    else
                        m->segs[j].index = 2;
                }
                else if (first_data_idx != -1 && j < first_data_idx)
                {
                    m->segs[j].index = 0;
                }
                else
                {
                    if (m->segs[j].prot & 4)
                        m->segs[j].index = 0;
                    else if (m->segs[j].prot & 2)
                        m->segs[j].index = 2;
                    else
                        m->segs[j].index = 1;
                }
            }

            /* --- 步骤 4.5：强制规范化 prot (消除反作弊权限污染) --- */
            /*
             * 步骤 4 已建立权威拓扑标签，此处根据标签反推标准 prot，
             * 同时修正 BSS 的 -w-p 异常权限为标准 RW。
             * 缝合阶段（步骤 5）不再需要合并 prot。
             */
            for (j = 0; j < m->seg_count; j++)
            {
                switch (m->segs[j].index)
                {
                case 1:
                    m->segs[j].prot = 1;
                    break; /* RO  : Header / RELRO     */
                case 0:
                    m->segs[j].prot = 5;
                    break; /* RX  : 代码段              */
                case 2:
                    m->segs[j].prot = 3;
                    break; /* RW  : 数据段              */
                case -1:
                    m->segs[j].prot = 3;
                    break; /* RW  : BSS（修正 -w- 异常）*/
                }
            }

            /* --- 步骤 5：拉链式精准缝合 --- */
            int out_idx = 0;
            for (j = 1; j < m->seg_count; j++)
            {
                struct segment_info *prev_seg = &m->segs[out_idx];
                struct segment_info *curr_seg = &m->segs[j];

                if (prev_seg->end == curr_seg->start &&
                    prev_seg->index == curr_seg->index)
                {
                    /* 首尾相连且拓扑标签一致，直接延伸尾部，prot 无需合并 */
                    prev_seg->end = curr_seg->end;
                }
                else
                {
                    out_idx++;
                    if (out_idx != j)
                        m->segs[out_idx] = *curr_seg;
                }
            }
            m->seg_count = out_idx + 1;

            /* --- 步骤 6：最终 Index 序列化 --- */
            seq = 0;
            for (j = 0; j < m->seg_count; j++)
            {
                if (m->segs[j].index != -1)
                    m->segs[j].index = seq++;
            }
        }
    }

    mmput(mm);
    put_task_struct(task);
    kfree(path_buf);
    return 0;
}

#endif // PROCESS_MEMORY_ENUM_H
