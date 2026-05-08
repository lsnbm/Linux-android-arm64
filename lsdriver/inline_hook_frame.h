
#ifndef INLINE_HOOK_FRAME_H
#define INLINE_HOOK_FRAME_H
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/string.h>
#include <linux/types.h>
#include <linux/mm.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#include <asm/memory.h>
#include <asm/pgtable.h>
#include <asm/pgtable-prot.h>
#include <asm/tlbflush.h>
#include "arm64_reg.h"

/*
inline hook框架
kprobe 被 NOKPROBE_SYMBOL 拒绝(-EINVAL)，ftrace 未开启
因此改用 inline hook 方案：
需要注意的是paciasp 指令和bti c指令，这是函数的第一条
一般没有paciasp ，这种强语义的搬到跳板执行可能有风险
bti只限制br/blr间接跳
*/

#define TRAMP_WORDS 48
#define TRAMP_BYTES (TRAMP_WORDS * 4)
#define TRAMP_SLOT_COUNT 4

#define HOOK_STR_1(x) #x
#define HOOK_STR(x) HOOK_STR_1(x)

// 用符号链接下面汇编代码段，安装hook时patch为跳板代码
extern uint32_t inline_hook_trampoline_slots[];

asm(
    ".pushsection .text\n\t"
    ".balign 4\n\t"
    ".globl inline_hook_trampoline_slots\n\t"
    "inline_hook_trampoline_slots:\n\t"
    ".rept " HOOK_STR(TRAMP_SLOT_COUNT *TRAMP_WORDS) "\n\t"
                                                     ".word 0xD503201F\n\t"
                                                     ".endr\n\t"
                                                     ".popsection\n\t");

// 全局槽位位图，多处调用自动分配不冲突
static DECLARE_BITMAP(g_slot_used, TRAMP_SLOT_COUNT);

// 分配并获取一个槽位
static int slot_alloc(uint32_t **trampoline_out)
{
    int bit = find_first_zero_bit(g_slot_used, TRAMP_SLOT_COUNT);
    if (bit >= TRAMP_SLOT_COUNT)
        return -ENOSPC;
    set_bit(bit, g_slot_used);
    *trampoline_out = inline_hook_trampoline_slots + bit * TRAMP_WORDS;
    return bit;
}
// 释放槽位
static void slot_free(int index)
{
    clear_bit(index, g_slot_used);
}

// patch预留代码段
static int trampoline_patch(uint32_t *dst, const uint32_t *src)
{
    int ret;
    int i;

    if (!fn_aarch64_insn_patch_text_nosync)
        return -ENOENT;

    for (i = 0; i < TRAMP_WORDS; i++)
    {
        ret = fn_aarch64_insn_patch_text_nosync((void *)&dst[i], src[i]);
        if (ret)
            return ret;
    }
    return 0;
}

// 一条 hook 的描述
struct hook_entry
{
    const char *target_sym;    // 目标函数符号名
    unsigned long target_addr; // 运行时填充
    void *work_fn;             // 工作函数指针

    /* 框架内部 */
    uint32_t *trampoline; // 模块代码段预留的跳板
    uint32_t saved_insn;  // 目标函数入口被覆盖的原始指令
    bool installed;       // 是否已安装
    int slot_index;       // 分配到的槽位，-1 表示未分配
};

// 便捷宏:申明一个hook_entry
#define HOOK_ENTRY(sym, fn)  \
    {                        \
        .target_sym = (sym), \
        .target_addr = 0,    \
        .work_fn = (fn),     \
        .trampoline = NULL,  \
        .saved_insn = 0,     \
        .installed = false,  \
        .slot_index = -1,    \
    }

// 生成模板跳板汇编代码
static void trampoline_build(uint32_t *buf, uint32_t orig_insn, unsigned long work_fn, unsigned long return_addr)
{
    static const uint32_t tramp_template[] = {
        // 开辟272字节栈空间
        0xD10443FF, // [0]  sub sp, sp, #272

        // 所有通用寄存器入栈保存
        0xA90007E0, // [1]  stp x0,  x1,  [sp, #0]
        0xA9010FE2, // [2]  stp x2,  x3,  [sp, #16]
        0xA90217E4, // [3]  stp x4,  x5,  [sp, #32]
        0xA9031FE6, // [4]  stp x6,  x7,  [sp, #48]
        0xA90427E8, // [5]  stp x8,  x9,  [sp, #64]
        0xA9052FEA, // [6]  stp x10, x11, [sp, #80]
        0xA90637EC, // [7]  stp x12, x13, [sp, #96]
        0xA9073FEE, // [8]  stp x14, x15, [sp, #112]
        0xA90847F0, // [9]  stp x16, x17, [sp, #128]
        0xA9094FF2, // [10] stp x18, x19, [sp, #144]
        0xA90A57F4, // [11] stp x20, x21, [sp, #160]
        0xA90B5FF6, // [12] stp x22, x23, [sp, #176]
        0xA90C67F8, // [13] stp x24, x25, [sp, #192]
        0xA90D6FFA, // [14] stp x26, x27, [sp, #208]
        0xA90E77FC, // [15] stp x28, x29, [sp, #224]
        0xF9007BFE, // [16] str x30, [sp, #240]

        // 保存nzcv调节标志
        0xD53B4209, // [17] mrs x9, nzcv
        0xF90083E9, // [18] str x9, [sp, #256]

        // 给工作函数建议临时栈帧
        0x910003FD, // [19] mov x29, sp

        // 调用工作函数
        0x58000349, // [20] ldr x9, [pc, #0x68]   < pc相对寻址到[46][47],存到x9
        0xD63F0120, // [21] blr x9                < 进行直接跳，继承了被hook函数所有寄存器状态，回来时恢复被hook函数寄存器状态

        // 恢复nzcv条件标志
        0xF94083E9, // [22] ldr x9, [sp, #256]
        0xD51B4209, // [23] msr nzcv, x9
        // 所有寄存器出栈恢复
        0xF9407BFE, // [24] ldr x30, [sp, #240]
        0xA94E77FC, // [25] ldp x28, x29, [sp, #224]
        0xA94D6FFA, // [26] ldp x26, x27, [sp, #208]
        0xA94C67F8, // [27] ldp x24, x25, [sp, #192]
        0xA94B5FF6, // [28] ldp x22, x23, [sp, #176]
        0xA94A57F4, // [29] ldp x20, x21, [sp, #160]
        0xA9494FF2, // [30] ldp x18, x19, [sp, #144]
        0xA94847F0, // [31] ldp x16, x17, [sp, #128]
        0xA9473FEE, // [32] ldp x14, x15, [sp, #112]
        0xA94637EC, // [33] ldp x12, x13, [sp, #96]
        0xA9452FEA, // [34] ldp x10, x11, [sp, #80]
        0xA94427E8, // [35] ldp x8,  x9,  [sp, #64]
        0xA9431FE6, // [36] ldp x6,  x7,  [sp, #48]
        0xA94217E4, // [37] ldp x4,  x5,  [sp, #32]
        0xA9410FE2, // [38] ldp x2,  x3,  [sp, #16]
        0xA94007E0, // [39] ldp x0,  x1,  [sp, #0]

        // 恢复原始sp
        0x910443FF, // [40] add sp, sp, #272

        // 执行原指令
        0x00000000, // [41] orig_insn          < 动态填,被hook覆盖的原指令

        // 跳回目标地址继续执行
        0x58000050, // [42] ldr x16, [pc, #0x8] < pc相对寻址到[44][45]
        0xD65F0200, // [43] ret x16              < 5系以上启用BTI,BTI限制只允许跳到函数头，不能使用间接跳，也不能用间接跳到一个函数中间地址

        // 数据槽统一放末尾，ret跳走后永远不会顺序执行到这里
        0x00000000, // [44] RET_SLOT low32       < target_addr + 4
        0x00000000, // [45] RET_SLOT high32
        0x00000000, // [46] WORK_SLOT low32      < work_fn
        0x00000000, // [47] WORK_SLOT high32
    };

    // 将模板数组放到可执行段
    __builtin_memcpy(buf, tramp_template, TRAMP_BYTES);
    // 动态填入数据槽
    buf[41] = orig_insn;
    __builtin_memcpy(&buf[44], &return_addr, sizeof(uint64_t));
    __builtin_memcpy(&buf[46], &work_fn, sizeof(uint64_t));

    // 内核环境里memcpy()可能被架构、内存访问检查(KASAN)，边界检查(FORTIFY)、插桩(instrumentation) 等机制包装或替换
    // 直接使用__builtin_memcpy做纯数据拷贝绕过部分内核检查/插桩
}

// 安装单条hook
static int hook_entry_install(struct hook_entry *e)
{
    uint32_t tramp_code[TRAMP_WORDS];
    uint32_t hook_code;
    int ret, slot;
    unsigned long return_addr;

    if (e->installed)
        return 0;

    // 查符号地址
    if (!e->target_addr && e->target_sym)
    {
        e->target_addr = generic_kallsyms_lookup_name(e->target_sym);
        if (!e->target_addr)
        {
            pr_err("[hook] symbol not found: %s\n", e->target_sym);
            return -ENOENT;
        }
    }
    if (!e->target_addr || !e->work_fn)
        return -EINVAL;

    // 分配并获取一个槽位
    slot = slot_alloc(&e->trampoline);
    if (slot < 0)
        return -ENOSPC;
    e->slot_index = slot;

    // 保存原始指令
    e->saved_insn = READ_ONCE(*(uint32_t *)e->target_addr);

    // return_addr = handler + 4(跳过被我们覆盖的1条指令)
    return_addr = e->target_addr + 4;

    // 填充跳板
    trampoline_build(tramp_code, e->saved_insn, (unsigned long)e->work_fn, return_addr);

    // 写到预留代码段槽位
    ret = trampoline_patch(e->trampoline, tramp_code);
    if (ret)
    {
        slot_free(slot);
        e->slot_index = -1;
        e->trampoline = NULL;
        return ret;
    }

    // 编码b指令
    ret = arm64_make_b(e->target_addr, (unsigned long)e->trampoline, &hook_code);
    if (ret)
    {
        slot_free(slot);
        e->slot_index = -1;
        e->trampoline = NULL;
        return ret;
    }

    // patch 目标函数入口 → b trampoline
    ret = fn_aarch64_insn_patch_text_nosync((void *)e->target_addr, hook_code);
    if (ret)
    {
        slot_free(slot);
        e->slot_index = -1;
        e->trampoline = NULL;
        return ret;
    }

    e->installed = true;
    pr_debug("[hook] installed %s: 0x%lx -> trampoline 0x%lx\n", e->target_sym, e->target_addr, (unsigned long)e->trampoline);
    return 0;
}

// 卸载单条 hook
static void hook_entry_remove(struct hook_entry *e)
{
    if (!e->installed)
        return;
    // 恢复原指令
    fn_aarch64_insn_patch_text_nosync((void *)e->target_addr, e->saved_insn);
    slot_free(e->slot_index);
    e->slot_index = -1;
    e->trampoline = NULL;
    e->installed = false;
    pr_debug("[hook] removed %s\n", e->target_sym);
}

// 批量安装卸载接口
int inline_hook_install_count(struct hook_entry *entries, int count)
{
    int i, ret;
    if (count > TRAMP_SLOT_COUNT)
        return -ENOSPC;

    for (i = 0; i < count; i++)
    {
        ret = hook_entry_install(&entries[i]);
        // 失败回退
        if (ret)
        {
            while (--i >= 0)
                hook_entry_remove(&entries[i]);
            return ret;
        }
    }
    return 0;
}

void inline_hook_remove_count(struct hook_entry *entries, int count)
{
    // 逆序卸载
    for (int i = count - 1; i >= 0; i--)
        hook_entry_remove(&entries[i]);
}

//用于驱动/用户态退出的强行卸载所有hook
void inline_hook_remove_all(void)
{
    int i;

    for (i = 0; i < TRAMP_SLOT_COUNT; i++)
    {
        if (!test_bit(i, g_slot_used))
            continue;

        // trampoline[41] 就是 orig_insn，直接还原
        uint32_t *trampoline = inline_hook_trampoline_slots + i * TRAMP_WORDS;
        unsigned long target_addr = *(unsigned long *)&trampoline[44] - 4; // RET_SLOT存的是target+4

        fn_aarch64_insn_patch_text_nosync((void *)target_addr, trampoline[41]);
        slot_free(i);
        pr_debug("[hook] force removed slot %d, target 0x%lx\n", i, target_addr);
    }
}

// 外部调用宏，宏函数计算数组数量，不要直接在函数内部使用sizeof,参数会退化为指针
#define inline_hook_install(entries) inline_hook_install_count((entries), sizeof(entries) / sizeof((entries)[0]))
#define inline_hook_remove(entries) inline_hook_remove_count((entries), sizeof(entries) / sizeof((entries)[0]))

#endif // INLINE_HOOK_FRAME_H