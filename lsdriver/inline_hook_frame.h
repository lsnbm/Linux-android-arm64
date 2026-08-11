
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
#include "arm64_decode/arm64_decode.h"
#include "arm64_encode/arm64_encode.h"
#include "arm64_reg.h"
#include "export_fun.h"
#include "lsdriver_log.h"

/*
inline hook框架
kprobe 被 NOKPROBE_SYMBOL 拒绝(-EINVAL)，ftrace 未开启
因此改用 inline hook 方案：
需要注意的是paciasp 指令和bti c指令，这是函数的第一条
PAC 防返回导向攻击（ROP），BTI 防跳转导向攻击（JOP）
现在paciasp全局启用，内核90%函数都有 ，这种强语义的搬到跳板执行可能有风险
bti只限制br/blr间接跳
paciasp指令包含bti功能
内核函数90%以上都包含paciasp指令，部分用bti

2026/08/05 OnePlus 13 比较关键死机原因：
目标函数入口已经存在其他模块安装的 kprobe。ARM64 kprobe 会把被探测指令替换为 BRK #4，
并把被替换前的真实原指令保存在 struct kprobe::opcode 中。
inline hook 安装时，hook_save_orig_insns() 把目标入口现场的 BRK #4 当成普通原指令保存，
随后 trampoline_build() 又把它原样复制到跳板 replay_insn[]。执行回放时 PC 已经位于
inline_hook_trampoline_slots（本次现场为 slot 2 的 replay_insn[0]），不再是 kprobe 登记的原目标地址。
kprobe 异常处理按当前跳板 PC 调用 get_kprobe() 时找不到对应 struct kprobe，导致 BRK #4
无法作为正常 kprobe 命中处理，最终触发 kernel panic。

修复方式：hook_relocate_replay_insns() 在 relocation 阶段识别 BRK #4，按原始 source_pc
查找现有 struct kprobe，从 probe->opcode 取出真实原指令，只替换 tramp_code[] 中的回放副本；
saved_insn[] 继续保留目标入口的 BRK 现场，供卸载 inline hook 时恢复kprobe替换的brk

*/

#define HOOK_STUB_WORDS     4
#define HOOK_STUB_BYTES     (HOOK_STUB_WORDS * 4)
#define HOOK_METADATA_BYTES 32
#define HOOK_REGS_BYTES     272
#define HOOK_FRAME_BYTES    (HOOK_METADATA_BYTES + HOOK_REGS_BYTES)

#define TRAMP_WORDS             120
#define TRAMP_BYTES             (TRAMP_WORDS * 4)
#define TRAMP_SLOT_COUNT        32
#define TRAMP_REPLAY_INSN_INDEX 57
#define TRAMP_RET_TO_ORIG_INDEX 61
#define TRAMP_RET_SLOT_INDEX    116
#define TRAMP_WORK_SLOT_INDEX   118

#define HOOK_STR_1(x) #x
#define HOOK_STR(x)   HOOK_STR_1(x)

// 用符号链接下面汇编代码段，安装hook时patch为跳板代码
extern uint32_t inline_hook_trampoline_slots[];

asm(".pushsection .text\n\t"
    ".balign 8\n\t"
    ".globl inline_hook_trampoline_slots\n\t"
    "inline_hook_trampoline_slots:\n\t"
    ".rept " HOOK_STR(TRAMP_SLOT_COUNT *TRAMP_WORDS) "\n\t"
                                                     ".word 0xD503201F\n\t"
                                                     ".endr\n\t"
                                                     ".popsection\n\t");

// 一条 hook 的描述
struct hook_entry
{
    const char *target_sym; // 目标函数符号名
    uint64_t target_addr;   // 运行时填充
    void *work_fn;          // 工作函数指针: int (*)(struct pt_regs *regs),根据arm64调用约定，参数放在x0寄存器里,下面汇编会把pt_regs结构体指针放到x0传给工作函数

    /* 框架内部 */
    uint32_t *trampoline;                 // 模块代码段预留的跳板
    uint32_t saved_insn[HOOK_STUB_WORDS]; // 目标函数入口被覆盖的原始指令
    bool installed;                       // 是否已安装
    int slot_index;                       // 分配到的槽位，-1 表示未分配
};

// 槽位指针同时记录占用状态，并供卸载全部 hook 时找到保存的原始指令。
static struct hook_entry *g_slot_entries[TRAMP_SLOT_COUNT];
// get_kprobe 未导出，首次需要时通过 kallsyms 解析并缓存其精确函数签名。
static struct kprobe *(*fn_get_kprobe)(void *addr);

// 分配并获取一个槽位
static int slot_alloc(struct hook_entry *entry, uint32_t **trampoline_out)
{
    for (int i = 0; i < TRAMP_SLOT_COUNT; i++)
    {
        if (g_slot_entries[i]) continue;
        g_slot_entries[i] = entry;
        *trampoline_out = inline_hook_trampoline_slots + i * TRAMP_WORDS;
        return i;
    }
    return -ENOSPC;
}
// 释放槽位
static void slot_free(int index)
{
    g_slot_entries[index] = NULL;
}

// patch预留代码段
static int trampoline_patch(uint32_t *dst, const uint32_t *src)
{
    if (!fn_aarch64_insn_patch_text) return -ENOENT;

    void *addrs[TRAMP_WORDS];
    for (int i = 0; i < TRAMP_WORDS; i++) addrs[i] = (void *)&dst[i];

    return fn_aarch64_insn_patch_text(addrs, (uint32_t *)src, TRAMP_WORDS);
}
// 保存目标入口即将被覆盖的原始AArch64指令word。
static void hook_save_orig_insns(uint64_t addr, uint32_t *insns, int count)
{
    // AArch64指令天然4字节对齐，这里逐条READ_ONCE保存入口被覆盖的指令word。
    for (int i = 0; i < count; i++) insns[i] = READ_ONCE(*(uint32_t *)(uintptr_t)(addr + i * 4));
}

// ADR/ADRP 按跳板 PC 原地重编码，其余 PC 相对指令仍拒绝安装。
static int hook_relocate_replay_insns(uint64_t source_addr, uint64_t replay_addr, uint32_t *insns, int count)
{
    if (!insns || count <= 0 || count > HOOK_STUB_WORDS) return -EINVAL;

    // 由统一编码器生成 BRK #4 的指令
    // ARM64 kprobe 用 BRK #4 替换被探测指令，原指令保存在 struct kprobe::opcode。
    uint32_t kprobe_brk_insn;
    int ret = arm64_encode_brk(4U, &kprobe_brk_insn);
    if (ret) return ret;

    for (int i = 0; i < count; i++)
    {
        struct arm64_decoded_insn decoded;
        uint64_t source_pc = source_addr + i * sizeof(uint32_t);
        uint64_t replay_pc = replay_addr + i * sizeof(uint32_t);

        // saved_insn[] 保留目标入口的真实现场；这里只解包 tramp_code[] 中的回放副本，
        // 避免把 kprobe 的 BRK 搬到跳板后因跳板地址不在 kprobe 表中而触发未处理异常。
        if (insns[i] == kprobe_brk_insn)
        {
            struct kprobe *probe;
            // 保存从 struct kprobe::opcode 读取的原指令，即目标地址被 BRK #4 替换前的指令。
            uint32_t original_insn = 0;
            bool resolved = false;

            // kallsyms 解析内部会临时注册 kprobe，可能调度，不能放在下面的禁抢占区间中。
            if (!fn_get_kprobe) fn_get_kprobe = (void *)generic_kallsyms_lookup_name("get_kprobe");
            if (!fn_get_kprobe)
            {
                ls_log_tag("hook", "get_kprobe symbol not found for source=0x%llx\n", (unsigned long long)source_pc);
                return -ENOENT;
            }

            // get_kprobe 返回 kprobe 哈希表中的活动对象。禁抢占覆盖查表、现场复核和
            // opcode 复制全过程；恢复抢占前复制出原指令，之后不再解引用 probe。
            preempt_disable();
            probe = fn_get_kprobe((void *)(uintptr_t)source_pc);
            // 同时校验表项地址和目标现场仍为 BRK #4，拒绝使用已变化或不匹配的表项。
            if (probe && probe->addr == (kprobe_opcode_t *)(uintptr_t)source_pc && READ_ONCE(*(uint32_t *)(uintptr_t)source_pc) == kprobe_brk_insn)
            {
                original_insn = READ_ONCE(probe->opcode);
                resolved = true;
            }
            preempt_enable();

            if (!resolved)
            {
                ls_log_tag("hook", "kprobe BRK has no matching live probe source=0x%llx\n", (unsigned long long)source_pc);
                return -EBUSY;
            }
            // 防止异常或嵌套状态把另一个 BRK #4 再次复制到跳板，形成同样的崩溃路径。
            if (original_insn == kprobe_brk_insn)
            {
                ls_log_tag("hook", "kprobe opcode recursively contains BRK source=0x%llx\n", (unsigned long long)source_pc);
                return -ELOOP;
            }

            ls_log_tag("hook", "resolved kprobe BRK source=0x%llx opcode=%08x\n", (unsigned long long)source_pc, original_insn);
            // 只替换回放副本；e->saved_insn[] 仍保存 BRK 现场，卸载时按原现场恢复。
            insns[i] = original_insn;
        }

        // 对解包后的真实原指令继续执行既有 ADR/ADRP relocation 和不支持指令检查。
        enum arm64_decode_status decode_status = arm64_decode_insn(insns[i], &decoded);
        if (decode_status != ARM64_DECODE_OK)
        {
            ls_log_tag("hook", "instruction cannot be replayed status=%u addr=0x%llx insn=%08x\n", decode_status, (unsigned long long)source_pc, insns[i]);
            return -EOPNOTSUPP;
        }

        switch (decoded.insn_class)
        {
        case ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE:
        {
            if (decoded.instruction != ARM64_INSN_ADR && decoded.instruction != ARM64_INSN_ADRP) continue;

            bool page_relative = decoded.instruction == ARM64_INSN_ADRP;
            uint64_t source_base = page_relative ? source_pc & ~0xFFFULL : source_pc;
            uint64_t target = source_base + decoded.offset;
            int status = page_relative ? arm64_encode_adrp(decoded.rd, replay_pc, target, &insns[i]) : arm64_encode_adr(decoded.rd, replay_pc, target, &insns[i]);

            if (status)
            {
                ls_log_tag("hook", "%s relocation out of range source=0x%llx replay=0x%llx target=0x%llx insn=%08x\n", page_relative ? "adrp" : "adr", (unsigned long long)source_pc, (unsigned long long)replay_pc, (unsigned long long)target, insns[i]);
                return status;
            }
            break;
        }
        case ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM:
            switch (decoded.instruction)
            {
            case ARM64_INSN_B:
            case ARM64_INSN_BL:
            case ARM64_INSN_B_COND:
            case ARM64_INSN_CBZ:
            case ARM64_INSN_CBNZ:
            case ARM64_INSN_TBZ:
            case ARM64_INSN_TBNZ:
                ls_log_tag("hook", "pc-relative instruction cannot be replayed addr=0x%llx insn=%08x\n", (unsigned long long)source_pc, insns[i]);
                return -EOPNOTSUPP;
            default:
                continue;
            }
        case ARM64_INSN_CLASS_LOAD_STORE:
            switch (decoded.instruction)
            {
            case ARM64_INSN_LDR_LITERAL_GPR:
            case ARM64_INSN_LDRSW_LITERAL:
            case ARM64_INSN_LDR_LITERAL_FP_SIMD:
            case ARM64_INSN_PRFM_LITERAL:
                ls_log_tag("hook", "pc-relative instruction cannot be replayed addr=0x%llx insn=%08x\n", (unsigned long long)source_pc, insns[i]);
                return -EOPNOTSUPP;
            default:
                continue;
            }
        default:
            continue;
        }
    }

    return 0;
}

// 批量 patch 一段 AArch64 指令；aarch64_insn_patch_text 内部负责 stop_machine 同步。
static int hook_patch_words(uint64_t addr, const uint32_t *insns, int count)
{
    if (!fn_aarch64_insn_patch_text) return -ENOENT;

    if (count <= 0 || count > HOOK_STUB_WORDS) return -EINVAL;

    void *addrs[HOOK_STUB_WORDS];
    for (int i = 0; i < count; i++) addrs[i] = (void *)(uintptr_t)(addr + i * 4);

    return fn_aarch64_insn_patch_text(addrs, (uint32_t *)insns, count);
}

static inline void *hook_frame_metadata(struct pt_regs *regs)
{
    return regs ? (void *)((char *)regs - HOOK_METADATA_BYTES) : NULL;
}

// 生成模板跳板汇编代码
static void trampoline_build(uint32_t *buf, const uint32_t saved_insn[HOOK_STUB_WORDS], uint64_t work_fn, uint64_t return_addr)
{
    static const uint32_t tramp_template[TRAMP_WORDS] = {
        // 开辟304字节栈空间：前32字节供返回 hook 保存元数据，后272字节按 struct pt_regs 前缀布局。
        // synthetic pt_regs = sp + 32；regs[0..30] = 0..240，sp = 248，pc = 256，pstate = 264
        0xD104C3FF, // [0] sub sp, sp, #304

        // 所有通用寄存器入栈保存到 pt_regs.regs[0..30]
        0xA90207E0, // [1] stp x0, x1, [sp, #32]
        0xA9030FE2, // [2] stp x2, x3, [sp, #48]
        0xA90417E4, // [3] stp x4, x5, [sp, #64]
        0xA9051FE6, // [4] stp x6, x7, [sp, #80]
        0xA90627E8, // [5] stp x8, x9, [sp, #96]
        0xA9072FEA, // [6] stp x10, x11, [sp, #112]
        0xA90837EC, // [7] stp x12, x13, [sp, #128]
        0xA9093FEE, // [8] stp x14, x15, [sp, #144]
        0xA90A47F0, // [9] stp x16, x17, [sp, #160]
        0xA90B4FF2, // [10] stp x18, x19, [sp, #176]
        0xA90C57F4, // [11] stp x20, x21, [sp, #192]
        0xA90D5FF6, // [12] stp x22, x23, [sp, #208]
        0xA90E67F8, // [13] stp x24, x25, [sp, #224]
        0xA90F6FFA, // [14] stp x26, x27, [sp, #240]
        0xA91077FC, // [15] stp x28, x29, [sp, #256]
        0xF9008BFE, // [16] str x30, [sp, #272]

        // 保存进入跳板前的真实SP。当前SP已经减了304，synthetic pt_regs.sp 位于 sp + 280。
        0x9104C3E9, // [17] add x9, sp, #304
        0xF9008FE9, // [18] str x9, [sp, #280]     < pt_regs.sp

        // 保存原始PC。RET_SLOT里存 target_addr + 16，减16得到被hook覆盖的入口地址
        0x58000C29, // [19] ldr x9, [pc, #0x184]  < RET_SLOT
        0xD1004129, // [20] sub x9, x9, #16        < pt_regs.pc = target_addr
        0xF90093E9, // [21] str x9, [sp, #288]     < pt_regs.pc

        // 保存NZCV条件标志到 pt_regs.pstate。这里不是完整PSTATE，只保留本跳板能恢复的NZCV位
        0xD53B4209, // [22] mrs x9, nzcv
        0xF90097E9, // [23] str x9, [sp, #296]     < pt_regs.pstate，仅使用NZCV位

        // 给工作函数建议临时栈帧，把 struct pt_regs * 放到参数0，然后通过x9调用work_fn
        0x910003FD, // [24] mov x29, sp
        0x910083E0, // [25] add x0, sp, #32        < 参数0: struct pt_regs *
        0x58000B89, // [26] ldr x9, [pc, #0x170]  < 相对寻址到WORK_SLOT
        0xD63F0120, // [27] blr x9

        // work_fn返回值只决定后续是否继续原函数；无论返回什么，后面都会恢复保存现场
        0xF100041F, // [28] cmp x0, #1
        0x54000440, // [29] b.eq [63]

        // 返回0时，如果 work_fn 改了 pt_regs.pc，不执行原指令，直接走动态PC路径
        0x58000AC9, // [30] ldr x9, [pc, #0x158]  < RET_SLOT
        0xD1004129, // [31] sub x9, x9, #16
        0xF94093EA, // [32] ldr x10, [sp, #288]    < work_fn 修改后的 pt_regs.pc
        0xEB09015F, // [33] cmp x10, x9
        0x54000721, // [34] b.ne [91]              < pc 被修改，走动态pc路径

        // 返回0且PC没改：恢复 regs->sp / regs->pstate / regs[0..30]，回放被覆盖的4条指令，再ret跳回原函数
        // x16暂存pt_regs基址，x17暂存最终SP；切回SP后再恢复原x16/x17，避免破坏原始指令现场
        0x910083F0, // [35] add x16, sp, #32       < synthetic pt_regs 基指针
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
        0x00000000, // [57] replay_insn[0]  <动态填
        0x00000000, // [58] replay_insn[1]  <动态填
        0x00000000, // [59] replay_insn[2]  <动态填
        0x00000000, // [60] replay_insn[3]  <动态填
        0x580006F0, // [61] ldr x16, [pc, #0xDC] < RET_SLOT
        0xD65F0200, // [62] ret x16              < 跳回 target_addr + 16

        // 返回1时，不继续执行原函数；但仍先检查PC是否被改，改了就按新的regs->pc跳走
        0x580006A9, // [63] ldr x9, [pc, #0xD4]   < RET_SLOT
        0xD1004129, // [64] sub x9, x9, #16
        0xF94093EA, // [65] ldr x10, [sp, #288]    < work_fn 修改后的 pt_regs.pc
        0xEB09015F, // [66] cmp x10, x9
        0x54000301, // [67] b.ne [91]              < pc 被修改，走动态pc路径

        // 返回1且PC没改：恢复 regs->sp / regs->pstate / regs[0..30] 后 ret x30
        0x910083F0, // [68] add x16, sp, #32
        0xF9407E11, // [69] ldr x17, [x16, #248]
        0xF9408609, // [70] ldr x9, [x16, #264]
        0xD51B4209, // [71] msr nzcv, x9
        0xF9407A1E, // [72] ldr x30, [x16, #240]
        0xA94E761C, // [73] ldp x28, x29, [x16, #224]
        0xA94D6E1A, // [74] ldp x26, x27, [x16, #208]
        0xA94C6618, // [75] ldp x24, x25, [x16, #192]
        0xA94B5E16, // [76] ldp x22, x23, [x16, #176]
        0xA94A5614, // [77] ldp x20, x21, [x16, #160]
        0xA9494E12, // [78] ldp x18, x19, [x16, #144]
        0xA9473E0E, // [79] ldp x14, x15, [x16, #112]
        0xA946360C, // [80] ldp x12, x13, [x16, #96]
        0xA9452E0A, // [81] ldp x10, x11, [x16, #80]
        0xA9442608, // [82] ldp x8, x9, [x16, #64]
        0xA9431E06, // [83] ldp x6, x7, [x16, #48]
        0xA9421604, // [84] ldp x4, x5, [x16, #32]
        0xA9410E02, // [85] ldp x2, x3, [x16, #16]
        0xA9400600, // [86] ldp x0, x1, [x16]
        0x9100023F, // [87] mov sp, x17
        0xF9404611, // [88] ldr x17, [x16, #136]
        0xF9404210, // [89] ldr x16, [x16, #128]
        0xD65F03C0, // [90] ret x30

        // 动态PC路径：work_fn修改了 regs->pc，恢复现场后跳到新的PC
        // x17暂存pt_regs基址，x16承载最终跳转目标，x15暂存最终SP；最后使用 ret x16 兼容BTI场景
        0x910083F1, // [91] add x17, sp, #32       < synthetic pt_regs 基指针
        0xF9408230, // [92] ldr x16, [x17, #256]   < 动态pc 目标地址
        0xF9407E2F, // [93] ldr x15, [x17, #248]   < 恢复目标sp
        0xF9408629, // [94] ldr x9, [x17, #264]
        0xD51B4209, // [95] msr nzcv, x9
        0xF9407A3E, // [96] ldr x30, [x17, #240]
        0xA94E763C, // [97] ldp x28, x29, [x17, #224]
        0xA94D6E3A, // [98] ldp x26, x27, [x17, #208]
        0xA94C6638, // [99] ldp x24, x25, [x17, #192]
        0xA94B5E36, // [100] ldp x22, x23, [x17, #176]
        0xA94A5634, // [101] ldp x20, x21, [x17, #160]
        0xA9494E32, // [102] ldp x18, x19, [x17, #144]
        0xF9403A2E, // [103] ldr x14, [x17, #112]
        0xA946362C, // [104] ldp x12, x13, [x17, #96]
        0xA9452E2A, // [105] ldp x10, x11, [x17, #80]
        0xA9442628, // [106] ldp x8, x9, [x17, #64]
        0xA9431E26, // [107] ldp x6, x7, [x17, #48]
        0xA9421624, // [108] ldp x4, x5, [x17, #32]
        0xA9410E22, // [109] ldp x2, x3, [x17, #16]
        0xA9400620, // [110] ldp x0, x1, [x17]
        0x910001FF, // [111] mov sp, x15
        0xF9403E2F, // [112] ldr x15, [x17, #120]
        0xF9404631, // [113] ldr x17, [x17, #136]
        0xD65F0200, // [114] ret x16              < 跳到修改后的 pt_regs.pc
        0xD503201F, // [115] nop                  < 保持后面的64位数据槽8字节对齐

        // 数据槽统一放末尾，跳走后永远不会顺序执行到这里
        0x00000000, // [116] RET_SLOT low32       < target_addr + 16
        0x00000000, // [117] RET_SLOT high32
        0x00000000, // [118] WORK_SLOT low32      < work_fn
        0x00000000, // [119] WORK_SLOT high32
    };

    /*
    编译期断言宏BUILD_BUG_ON，编译期检查结构体偏移布局正确
    */
    // 跳板汇编里硬编码了 struct pt_regs 的字段偏移；布局不匹配时直接编译失败，避免运行时按错偏移恢复现场。
    BUILD_BUG_ON(offsetof(struct pt_regs, regs) != 0);
    BUILD_BUG_ON(offsetof(struct pt_regs, sp) != 248);
    BUILD_BUG_ON(offsetof(struct pt_regs, pc) != 256);
    BUILD_BUG_ON(offsetof(struct pt_regs, pstate) != 264);
    BUILD_BUG_ON(offsetof(struct pt_regs, orig_x0) != HOOK_REGS_BYTES);
    // 被覆盖的4个word回放完后，必须紧跟跳回原函数后续地址的ldr/ret序列。
    BUILD_BUG_ON(TRAMP_RET_TO_ORIG_INDEX != TRAMP_REPLAY_INSN_INDEX + HOOK_STUB_WORDS);

    // 将模板数组放到可执行段
    __builtin_memcpy(buf, tramp_template, TRAMP_BYTES);
    // 动态填入数据槽
    __builtin_memcpy(&buf[TRAMP_REPLAY_INSN_INDEX], saved_insn, HOOK_STUB_BYTES);
    __builtin_memcpy(&buf[TRAMP_RET_SLOT_INDEX], &return_addr, sizeof(uint64_t));
    __builtin_memcpy(&buf[TRAMP_WORK_SLOT_INDEX], &work_fn, sizeof(uint64_t));

    // 内核环境里memcpy()可能被架构、内存访问检查(KASAN)，边界检查(FORTIFY)、插桩(instrumentation) 等机制包装或替换
    // 直接使用__builtin_memcpy做纯数据拷贝绕过部分内核检查/插桩
}

// 安装单条hook
static int hook_entry_install(struct hook_entry *e)
{
    int ret, slot;

    if (e->installed) return 0;

    // 查符号地址
    if (!e->target_addr && e->target_sym)
    {
        e->target_addr = generic_kallsyms_lookup_name(e->target_sym);
        if (!e->target_addr)
        {
            ls_log_tag("hook", "symbol not found: %s\n", e->target_sym);
            return -ENOENT;
        }
    }
    if (!e->target_addr || !e->work_fn) return -EINVAL;

    // 保存入口即将被覆盖的原始指令。
    hook_save_orig_insns(e->target_addr, e->saved_insn, HOOK_STUB_WORDS);
    ls_log_tag("hook", "original %s: 0x%llx: %08x %08x %08x %08x\n", e->target_sym ? e->target_sym : "<addr>", e->target_addr, e->saved_insn[0], e->saved_insn[1], e->saved_insn[2], e->saved_insn[3]);

    // 分配并获取一个槽位
    slot = slot_alloc(e, &e->trampoline);
    if (slot < 0) return -ENOSPC;
    e->slot_index = slot;

    // return_addr = handler + 16(跳过被我们覆盖的4条指令)
    uint64_t return_addr = e->target_addr + HOOK_STUB_BYTES;

    // 填充跳板，
    uint32_t tramp_code[TRAMP_WORDS];
    trampoline_build(tramp_code, e->saved_insn, (uint64_t)e->work_fn, return_addr);
    //再只对回放区中的 ADR/ADRP 重编码。
    ret = hook_relocate_replay_insns(e->target_addr, (uint64_t)e->trampoline + TRAMP_REPLAY_INSN_INDEX * sizeof(uint32_t), &tramp_code[TRAMP_REPLAY_INSN_INDEX], HOOK_STUB_WORDS);
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

    // 编码入口ret跳板
    uint32_t hook_code[HOOK_STUB_WORDS];
    ret = arm64_emit_abs_jump((uint64_t)e->trampoline, 16, hook_code, HOOK_STUB_WORDS);
    if (ret)
    {
        slot_free(slot);
        e->slot_index = -1;
        e->trampoline = NULL;
        return ret;
    }

    // patch 目标函数入口；失败时恢复原始指令，避免半安装状态。
    ret = hook_patch_words(e->target_addr, hook_code, HOOK_STUB_WORDS);
    if (ret)
    {
        hook_patch_words(e->target_addr, e->saved_insn, HOOK_STUB_WORDS);
        slot_free(slot);
        e->slot_index = -1;
        e->trampoline = NULL;
        return ret;
    }

    e->installed = true;
    ls_log_tag("hook", "installed %s: target=0x%llx trampoline=0x%llx slot=%d work=0x%llx return=0x%llx hook=%08x %08x %08x %08x\n", e->target_sym ? e->target_sym : "<addr>", e->target_addr, (uint64_t)e->trampoline, e->slot_index, (uint64_t)e->work_fn, return_addr, hook_code[0], hook_code[1], hook_code[2], hook_code[3]);
    return 0;
}

// 卸载单条 hook
static void hook_entry_remove(struct hook_entry *e)
{
    if (!e->installed) return;
    // 恢复原指令
    hook_patch_words(e->target_addr, e->saved_insn, HOOK_STUB_WORDS);
    slot_free(e->slot_index);
    e->slot_index = -1;
    e->trampoline = NULL;
    e->installed = false;
    ls_log_tag("hook", "removed %s\n", e->target_sym);
}

// 批量安装卸载接口
int inline_hook_install_count(struct hook_entry *entries, int count)
{
    if (count > TRAMP_SLOT_COUNT) return -ENOSPC;

    for (int i = 0; i < count; i++)
    {
        int ret = hook_entry_install(&entries[i]);
        // 失败回退
        if (ret)
        {
            while (--i >= 0) hook_entry_remove(&entries[i]);
            return ret;
        }
    }
    return 0;
}

void inline_hook_remove_count(struct hook_entry *entries, int count)
{
    // 逆序卸载
    for (int i = count - 1; i >= 0; i--) hook_entry_remove(&entries[i]);
}

// 用于驱动/用户态退出的卸载所有hook
void inline_hook_remove_all(void)
{
    for (int i = 0; i < TRAMP_SLOT_COUNT; i++)
    {
        struct hook_entry *entry = g_slot_entries[i];
        if (!entry) continue;
        hook_entry_remove(entry);
    }
}

// 便捷宏:申明一个hook_entry
#define HOOK_ENTRY(sym, fn)  \
    {                        \
        .target_sym = (sym), \
        .target_addr = 0,    \
        .work_fn = (fn),     \
        .trampoline = NULL,  \
        .saved_insn = {0},   \
        .installed = false,  \
        .slot_index = -1,    \
    }
// 外部调用宏，宏函数计算数组数量，不要直接在函数内部使用sizeof,参数会退化为指针

#define inline_hook_install(entries) inline_hook_install_count((entries), sizeof(entries) / sizeof((entries)[0]))
#define inline_hook_remove(entries)  inline_hook_remove_count((entries), sizeof(entries) / sizeof((entries)[0]))

#endif // INLINE_HOOK_FRAME_H
