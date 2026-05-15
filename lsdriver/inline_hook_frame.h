
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
#include <asm/ptrace.h>
#include <asm/tlbflush.h>
#include "arm64_reg.h"

/*
inline hook框架
kprobe 被 NOKPROBE_SYMBOL 拒绝(-EINVAL)，ftrace 未开启
因此改用 inline hook 方案：
需要注意的是paciasp 指令和bti c指令，这是函数的第一条
PAC 防返回导向攻击（ROP），BTI 防跳转导向攻击（JOP）
现在paciasp全局启用，内核90%函数都有 ，这种强语义的搬到跳板执行可能有风险
bti只限制br/blr间接跳
paciasp启用时只有paciasp指令，并且包含bti功能
*/

#define TRAMP_WORDS 116
#define TRAMP_BYTES (TRAMP_WORDS * 4)
#define TRAMP_SLOT_COUNT 10
#define TRAMP_ORIG_INSN_INDEX 57
#define TRAMP_RETURN_BRANCH_INDEX 58
#define TRAMP_RET_SLOT_INDEX 112
#define TRAMP_WORK_SLOT_INDEX 114

#define HOOK_STR_1(x) #x
#define HOOK_STR(x) HOOK_STR_1(x)

// 用符号链接下面汇编代码段，安装hook时patch为跳板代码
extern uint32_t inline_hook_trampoline_slots[];

asm(
    ".pushsection .text\n\t"
    ".balign 8\n\t"
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
    void *work_fn;             // 工作函数指针: int (*)(struct pt_regs *regs)

    /* 框架内部 */
    uint32_t *trampoline; // 模块代码段预留的跳板
    uint32_t saved_insn;  // 目标函数入口被覆盖的原始指令
    bool installed;       // 是否已安装
    int slot_index;       // 分配到的槽位，-1 表示未分配
};

// 生成模板跳板汇编代码
static int trampoline_build(uint32_t *buf, uint32_t orig_insn, unsigned long work_fn, unsigned long return_addr, unsigned long trampoline_addr)
{
    int ret;

    static const uint32_t tramp_template[TRAMP_WORDS] = {
        // 开辟272字节栈空间，这块空间按 struct pt_regs 前缀布局：
        // regs[0..30] = 0..240，sp = 248，pc = 256，pstate = 264
        0xD10443FF, // [0] sub sp, sp, #272

        // 所有通用寄存器入栈保存到 pt_regs.regs[0..30]
        0xA90007E0, // [1] stp x0, x1, [sp]
        0xA9010FE2, // [2] stp x2, x3, [sp, #16]
        0xA90217E4, // [3] stp x4, x5, [sp, #32]
        0xA9031FE6, // [4] stp x6, x7, [sp, #48]
        0xA90427E8, // [5] stp x8, x9, [sp, #64]
        0xA9052FEA, // [6] stp x10, x11, [sp, #80]
        0xA90637EC, // [7] stp x12, x13, [sp, #96]
        0xA9073FEE, // [8] stp x14, x15, [sp, #112]
        0xA90847F0, // [9] stp x16, x17, [sp, #128]
        0xA9094FF2, // [10] stp x18, x19, [sp, #144]
        0xA90A57F4, // [11] stp x20, x21, [sp, #160]
        0xA90B5FF6, // [12] stp x22, x23, [sp, #176]
        0xA90C67F8, // [13] stp x24, x25, [sp, #192]
        0xA90D6FFA, // [14] stp x26, x27, [sp, #208]
        0xA90E77FC, // [15] stp x28, x29, [sp, #224]
        0xF9007BFE, // [16] str x30, [sp, #240]

        // 保存进入跳板前的真实SP。当前SP已经减了272，所以 sp + 272 才是原始SP
        0x910443E9, // [17] add x9, sp, #272
        0xF9007FE9, // [18] str x9, [sp, #248]     < pt_regs.sp

        // 保存原始PC。RET_SLOT里存 target_addr + 4，减4得到被hook覆盖的入口地址
        0x58000BA9, // [19] ldr x9, [pc, #0x174]  < RET_SLOT
        0xD1001129, // [20] sub x9, x9, #4         < pt_regs.pc = target_addr
        0xF90083E9, // [21] str x9, [sp, #256]     < pt_regs.pc

        // 保存NZCV条件标志到 pt_regs.pstate。这里不是完整PSTATE，只保留本跳板能恢复的NZCV位
        0xD53B4209, // [22] mrs x9, nzcv
        0xF90087E9, // [23] str x9, [sp, #264]     < pt_regs.pstate，仅使用NZCV位

        // 给工作函数建议临时栈帧，把 struct pt_regs * 放到参数0，然后通过x9调用work_fn
        0x910003FD, // [24] mov x29, sp
        0x910003E0, // [25] mov x0, sp             < 参数0: struct pt_regs *
        0x58000B09, // [26] ldr x9, [pc, #0x160]  < 相对寻址到WORK_SLOT
        0xD63F0120, // [27] blr x9

        // work_fn返回值只决定后续是否继续原函数；无论返回什么，后面都会恢复保存现场
        0xF100041F, // [28] cmp x0, #1
        0x540003C0, // [29] b.eq [59]

        // 返回0时，如果 work_fn 改了 pt_regs.pc，不执行原指令，直接走动态PC路径
        0x58000A49, // [30] ldr x9, [pc, #0x148]  < RET_SLOT
        0xD1001129, // [31] sub x9, x9, #4
        0xF94083EA, // [32] ldr x10, [sp, #256]    < work_fn 修改后的 pt_regs.pc
        0xEB09015F, // [33] cmp x10, x9
        0x540006A1, // [34] b.ne [87]              < pc 被修改，走动态pc路径

        // 返回0且PC没改：恢复 regs->sp / regs->pstate / regs[0..30]，再执行原始入口指令
        // x16暂存pt_regs基址，x17暂存最终SP；切回SP后再恢复原x16/x17，避免破坏原始指令现场
        0x910003F0, // [35] mov x16, sp            < 栈帧基指针
        0xF9407E11, // [36] ldr x17, [x16, #248]   < 恢复目标sp
        0xF9408609, // [37] ldr x9, [x16, #264]
        0xD51B4209, // [38] msr nzcv, x9
        0xF9407A1E, // [39] ldr x30, [x16, #240]
        0xA94E761C, // [40] ldp x28, x29, [x16, #224]
        0xA94D6E1A, // [41] ldp x26, x27, [x16, #208]
        0xA94C6618, // [42] ldp x24, x25, [x16, #192]
        0xA94B5E16, // [43] ldp x22, x23, [x16, #176]
        0xA94A5614, // [44] ldp x20, x21, [x16, #160]
        0xA9494E12, // [45] ldp x18, x19, [x16, #144]
        0xA9473E0E, // [46] ldp x14, x15, [x16, #112]
        0xA946360C, // [47] ldp x12, x13, [x16, #96]
        0xA9452E0A, // [48] ldp x10, x11, [x16, #80]
        0xA9442608, // [49] ldp x8, x9, [x16, #64]
        0xA9431E06, // [50] ldp x6, x7, [x16, #48]
        0xA9421604, // [51] ldp x4, x5, [x16, #32]
        0xA9410E02, // [52] ldp x2, x3, [x16, #16]
        0xA9400600, // [53] ldp x0, x1, [x16]
        0x9100023F, // [54] mov sp, x17
        0xF9404611, // [55] ldr x17, [x16, #136]
        0xF9404210, // [56] ldr x16, [x16, #128]
        0x00000000, // [57] orig_insn  <动态填
        0xD503201F, // [58] 已被 arm64_make_b 函数修补，Hook 点位替换完成

        // 返回1时，不继续执行原函数；但仍先检查PC是否被改，改了就按新的regs->pc跳走
        0x580006A9, // [59] ldr x9, [pc, #0xD4]   < RET_SLOT
        0xD1001129, // [60] sub x9, x9, #4
        0xF94083EA, // [61] ldr x10, [sp, #256]    < work_fn 修改后的 pt_regs.pc
        0xEB09015F, // [62] cmp x10, x9
        0x54000301, // [63] b.ne [87]              < pc 被修改，走动态pc路径

        // 返回1且PC没改：恢复 regs->sp / regs->pstate / regs[0..30] 后 ret x30
        0x910003F0, // [64] mov x16, sp
        0xF9407E11, // [65] ldr x17, [x16, #248]
        0xF9408609, // [66] ldr x9, [x16, #264]
        0xD51B4209, // [67] msr nzcv, x9
        0xF9407A1E, // [68] ldr x30, [x16, #240]
        0xA94E761C, // [69] ldp x28, x29, [x16, #224]
        0xA94D6E1A, // [70] ldp x26, x27, [x16, #208]
        0xA94C6618, // [71] ldp x24, x25, [x16, #192]
        0xA94B5E16, // [72] ldp x22, x23, [x16, #176]
        0xA94A5614, // [73] ldp x20, x21, [x16, #160]
        0xA9494E12, // [74] ldp x18, x19, [x16, #144]
        0xA9473E0E, // [75] ldp x14, x15, [x16, #112]
        0xA946360C, // [76] ldp x12, x13, [x16, #96]
        0xA9452E0A, // [77] ldp x10, x11, [x16, #80]
        0xA9442608, // [78] ldp x8, x9, [x16, #64]
        0xA9431E06, // [79] ldp x6, x7, [x16, #48]
        0xA9421604, // [80] ldp x4, x5, [x16, #32]
        0xA9410E02, // [81] ldp x2, x3, [x16, #16]
        0xA9400600, // [82] ldp x0, x1, [x16]
        0x9100023F, // [83] mov sp, x17
        0xF9404611, // [84] ldr x17, [x16, #136]
        0xF9404210, // [85] ldr x16, [x16, #128]
        0xD65F03C0, // [86] ret x30

        // 动态PC路径：work_fn修改了 regs->pc，恢复现场后跳到新的PC
        // x17暂存pt_regs基址，x16承载最终跳转目标，x15暂存最终SP；最后使用 ret x16 兼容BTI场景
        0x910003F1, // [87] mov x17, sp            < pc栈帧基址
        0xF9408230, // [88] ldr x16, [x17, #256]   < 动态pc 目标地址
        0xF9407E2F, // [89] ldr x15, [x17, #248]   < 恢复目标sp
        0xF9408629, // [90] ldr x9, [x17, #264]
        0xD51B4209, // [91] msr nzcv, x9
        0xF9407A3E, // [92] ldr x30, [x17, #240]
        0xA94E763C, // [93] ldp x28, x29, [x17, #224]
        0xA94D6E3A, // [94] ldp x26, x27, [x17, #208]
        0xA94C6638, // [95] ldp x24, x25, [x17, #192]
        0xA94B5E36, // [96] ldp x22, x23, [x17, #176]
        0xA94A5634, // [97] ldp x20, x21, [x17, #160]
        0xA9494E32, // [98] ldp x18, x19, [x17, #144]
        0xF9403A2E, // [99] ldr x14, [x17, #112]
        0xA946362C, // [100] ldp x12, x13, [x17, #96]
        0xA9452E2A, // [101] ldp x10, x11, [x17, #80]
        0xA9442628, // [102] ldp x8, x9, [x17, #64]
        0xA9431E26, // [103] ldp x6, x7, [x17, #48]
        0xA9421624, // [104] ldp x4, x5, [x17, #32]
        0xA9410E22, // [105] ldp x2, x3, [x17, #16]
        0xA9400620, // [106] ldp x0, x1, [x17]
        0x910001FF, // [107] mov sp, x15
        0xF9403E2F, // [108] ldr x15, [x17, #120]
        0xF9404631, // [109] ldr x17, [x17, #136]
        0xD65F0200, // [110] ret x16              < 跳到修改后的 pt_regs.pc
        0xD503201F, // [111] nop                  < 保持后面的64位数据槽8字节对齐

        // 数据槽统一放末尾，跳走后永远不会顺序执行到这里
        0x00000000, // [112] RET_SLOT low32       < target_addr + 4
        0x00000000, // [113] RET_SLOT high32
        0x00000000, // [114] WORK_SLOT low32      < work_fn
        0x00000000, // [115] WORK_SLOT high32
    };

    BUILD_BUG_ON(offsetof(struct pt_regs, regs) != 0);
    BUILD_BUG_ON(offsetof(struct pt_regs, sp) != 248);
    BUILD_BUG_ON(offsetof(struct pt_regs, pc) != 256);
    BUILD_BUG_ON(offsetof(struct pt_regs, pstate) != 264);

    // 将模板数组放到可执行段
    __builtin_memcpy(buf, tramp_template, TRAMP_BYTES);
    // 动态填入数据槽
    buf[TRAMP_ORIG_INSN_INDEX] = orig_insn;
    __builtin_memcpy(&buf[TRAMP_RET_SLOT_INDEX], &return_addr, sizeof(uint64_t));
    __builtin_memcpy(&buf[TRAMP_WORK_SLOT_INDEX], &work_fn, sizeof(uint64_t));

    ret = arm64_make_b(trampoline_addr + TRAMP_RETURN_BRANCH_INDEX * 4, return_addr, &buf[TRAMP_RETURN_BRANCH_INDEX]);
    if (ret)
        return ret;

    // 内核环境里memcpy()可能被架构、内存访问检查(KASAN)，边界检查(FORTIFY)、插桩(instrumentation) 等机制包装或替换
    // 直接使用__builtin_memcpy做纯数据拷贝绕过部分内核检查/插桩
    return 0;
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
    ret = trampoline_build(tramp_code, e->saved_insn, (unsigned long)e->work_fn,
                           return_addr, (unsigned long)e->trampoline);
    if (ret)
    {
        slot_free(slot);
        e->slot_index = -1;
        e->trampoline = NULL;
        return ret;
    }

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

// 用于驱动/用户态退出的强行卸载所有hook
void inline_hook_remove_all(void)
{
    int i;

    for (i = 0; i < TRAMP_SLOT_COUNT; i++)
    {
        if (!test_bit(i, g_slot_used))
            continue;

        // trampoline[TRAMP_ORIG_INSN_INDEX] 就是 orig_insn，直接还原
        uint32_t *trampoline = inline_hook_trampoline_slots + i * TRAMP_WORDS;
        unsigned long target_addr = *(unsigned long *)&trampoline[TRAMP_RET_SLOT_INDEX] - 4; // RET_SLOT存的是target+4

        fn_aarch64_insn_patch_text_nosync((void *)target_addr, trampoline[TRAMP_ORIG_INSN_INDEX]);
        slot_free(i);
        pr_debug("[hook] force removed slot %d, target 0x%lx\n", i, target_addr);
    }
}

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
// 外部调用宏，宏函数计算数组数量，不要直接在函数内部使用sizeof,参数会退化为指针

#define inline_hook_install(entries) inline_hook_install_count((entries), sizeof(entries) / sizeof((entries)[0]))
#define inline_hook_remove(entries) inline_hook_remove_count((entries), sizeof(entries) / sizeof((entries)[0]))

#endif // INLINE_HOOK_FRAME_H
