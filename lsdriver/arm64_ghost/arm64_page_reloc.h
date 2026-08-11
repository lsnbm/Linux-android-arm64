#ifndef ARM64_PAGE_RELOC_H
#define ARM64_PAGE_RELOC_H

/*
该模块头文件实现 ARM64 单页机器码重定位。
输入是一页 PAGE_SIZE 大小的原始机器码、原页地址和幽灵页地址，输出缓冲区包含同偏移
指令页、页尾返回序列，以及只为实际超范围指令连续追加的跳板槽。源页和幽灵页必须按页对齐。

重定位时每条源指令严格对应幽灵页同偏移处的一条 4 字节指令，不展开为多条指令：
1. 普通指令和非 PC 相对指令原样复制。
2. ADR、ADRP 保持原始绝对目标地址，并按幽灵页 PC 重新编码立即数。
3. literal load 和 literal prefetch 保持原始绝对目标地址，并按幽灵页 PC 重新编码立即数。
4. 其余支持的 PC 相对指令为直接分支。目标在源页内时改为幽灵页同偏移地址，页外时
    保持原始绝对目标地址，再按对应指令的立即数字段重新编码。BL 始终进入回放槽，先把
    source_pc + 4 写入 x30，再以不带链接语义的跳转进入目标，保持原始绝对返回地址。
5. 在 ghost_page + PAGE_SIZE 追加返回源页下一页的跳转；B 可达时直接跳转，只有超出
    B 指令寻址范围时才使用 LDR literal、RET 和 64 位目标地址组成的 x16 长跳转。
6. 重编码后的立即数超出原指令范围时，不写入 BRK，也不中止安装：直接分支改为跳到近端
    绝对跳转槽；ADR、ADRP 和 literal load 改为 B 到语义回放槽，执行原操作后跳回下一条
    幽灵指令；literal prefetch 作为可忽略的预取提示改为 NOP。所有槽紧跟页尾返回序列
    连续排列。分支槽到目标在 B 范围内时直接 B，不修改 x16；仅超出范围时按 AAPCS64
    veneer 约定使用 x16 作为 IP0。

模块同时保存源页与幽灵页的基地址，提供两个页面之间同偏移程序计数器的双向转换。

decoder 未返回 ARM64_DECODE_OK 的指令不参与重定位，调用方必须处理返回的错误。

本模块只处理地址计算和机器码重编码，不查找或安装幽灵页，不操作目标进程页表，不写入 BRK，
也不负责机器码缓存同步；监控点替换和输出机器码的实际安装由调用者决定。
*/

#ifdef __KERNEL__
#include <linux/errno.h>
#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/types.h>
#else
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#define PAGE_SHIFT 12
#define PAGE_SIZE  (1UL << PAGE_SHIFT)
#define PAGE_MASK  (~(PAGE_SIZE - 1))
#endif

#include "../arm64_decode/arm64_decode.h"
#include "../arm64_encode/arm64_encode.h"

#define ARM64_PAGE_RELOC_SOURCE_WORDS   (PAGE_SIZE / sizeof(uint32_t))
#define ARM64_PAGE_RELOC_RETURN_WORDS   4
#define ARM64_PAGE_RELOC_SLOT_WORDS     8
#define ARM64_PAGE_RELOC_SLOT_BASE_WORD (ARM64_PAGE_RELOC_SOURCE_WORDS + ARM64_PAGE_RELOC_RETURN_WORDS)
#define ARM64_PAGE_RELOC_BASE_BYTES     (ARM64_PAGE_RELOC_SLOT_BASE_WORD * sizeof(uint32_t))
#define ARM64_PAGE_RELOC_MAX_WORDS      (ARM64_PAGE_RELOC_SLOT_BASE_WORD + ARM64_PAGE_RELOC_SOURCE_WORDS * ARM64_PAGE_RELOC_SLOT_WORDS)
#define ARM64_PAGE_RELOC_MAX_BYTES      (ARM64_PAGE_RELOC_MAX_WORDS * sizeof(uint32_t))

_Static_assert((ARM64_PAGE_RELOC_SLOT_BASE_WORD + (ARM64_PAGE_RELOC_SOURCE_WORDS - 1) * ARM64_PAGE_RELOC_SLOT_WORDS - (ARM64_PAGE_RELOC_SOURCE_WORDS - 1)) * sizeof(uint32_t) <= ((1U << 13) - 1) * sizeof(uint32_t), "furthest relocation slot must fit TBZ/TBNZ range");

struct arm64_page_relocation
{
    uint64_t source_page;
    uint64_t ghost_page;
    size_t code_size;
    uint32_t slot_count;
};

// 生成不带链接语义的跳转；优先使用不破坏寄存器的 B，超出其寻址范围后才使用 x16 长跳转。
static inline int arm64_page_reloc_make_jump(uint64_t jump_pc, uint64_t target, uint32_t *output, uint32_t output_words)
{
    int status = arm64_encode_fill_nops(output, output_words);
    if (status) return status;

    status = arm64_encode_b(jump_pc, target, output);
    if (status == -ERANGE) return arm64_emit_abs_jump(target, 16, output, output_words);
    return status;
}

// 回放 BL：源位置用 B 进入槽，槽恢复原始绝对 LR 后再无链接跳转到调用目标。
static inline int arm64_page_reloc_call(uint64_t source_pc, uint64_t ghost_pc, uint64_t target, uint32_t *instruction, uint64_t slot_pc, uint32_t *slot)
{
    int status = arm64_encode_b(ghost_pc, slot_pc, instruction);
    if (status) return status;

    status = arm64_encode_fill_nops(slot, ARM64_PAGE_RELOC_SLOT_WORDS);
    if (status) return status;
    status = arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 30, slot_pc, slot_pc + 6 * sizeof(uint32_t), &slot[0]);
    if (status) return status;

    status = arm64_page_reloc_make_jump(slot_pc + sizeof(uint32_t), target, &slot[1], 5);
    if (status) return status;
    uint64_t return_pc = source_pc + sizeof(uint32_t);
    __builtin_memcpy(&slot[6], &return_pc, sizeof(return_pc));
    return 1;
}

// 将解码后的 literal load 语义映射到编码器的访存类型。
static inline int arm64_page_reloc_load_store_kind(const struct arm64_decoded_insn *decoded, enum arm64_encode_load_store_kind *kind)
{
    if (!decoded || !kind) return -EINVAL;

    switch (decoded->instruction)
    {
    case ARM64_INSN_LDR_LITERAL_GPR:
        if (decoded->operand_width == 32) *kind = ARM64_ENCODE_LS_GPR32;
        else if (decoded->operand_width == 64) *kind = ARM64_ENCODE_LS_GPR64;
        else return -EINVAL;
        break;
    case ARM64_INSN_LDRSW_LITERAL:
        *kind = ARM64_ENCODE_LS_LDRSW;
        break;
    case ARM64_INSN_LDR_LITERAL_FP_SIMD:
        if (decoded->access_bytes == 4) *kind = ARM64_ENCODE_LS_FP32;
        else if (decoded->access_bytes == 8) *kind = ARM64_ENCODE_LS_FP64;
        else if (decoded->access_bytes == 16) *kind = ARM64_ENCODE_LS_FP128;
        else return -EINVAL;
        break;
    default:
        return -EINVAL;
    }

    return 0;
}

// 使用解码结果重新编码同一种直接分支。
static inline int arm64_page_reloc_encode_branch(const struct arm64_decoded_insn *decoded, uint64_t pc, uint64_t target, uint32_t *instruction)
{
    switch (decoded->instruction)
    {
    case ARM64_INSN_B:
        return arm64_encode_b(pc, target, instruction);
    case ARM64_INSN_B_COND:
        return arm64_encode_b_cond(pc, target, decoded->condition, instruction);
    case ARM64_INSN_CBZ:
    case ARM64_INSN_CBNZ:
        return arm64_encode_cbz(pc, target, decoded->instruction == ARM64_INSN_CBNZ, decoded->operand_width == 64, decoded->rt, instruction);
    case ARM64_INSN_TBZ:
    case ARM64_INSN_TBNZ:
        return arm64_encode_tbz(pc, target, decoded->instruction == ARM64_INSN_TBNZ, decoded->test_bit, decoded->rt, instruction);
    default:
        return -EINVAL;
    }
}

// 重编码直接分支；超出原分支范围时跳到近端绝对跳转槽。返回 1 表示占用了当前槽。
static inline int arm64_page_reloc_branch(const struct arm64_page_relocation *relocation, const struct arm64_decoded_insn *decoded, uint64_t source_pc, uint64_t ghost_pc, uint32_t *instruction, uint64_t slot_pc, uint32_t *slot)
{
    uint64_t target = source_pc + decoded->offset;
    if (target >= relocation->source_page && target < relocation->source_page + PAGE_SIZE) target = relocation->ghost_page + (target - relocation->source_page);

    if (decoded->instruction == ARM64_INSN_BL) return arm64_page_reloc_call(source_pc, ghost_pc, target, instruction, slot_pc, slot);

    int status = arm64_page_reloc_encode_branch(decoded, ghost_pc, target, instruction);
    if (status != -ERANGE) return status;

    status = arm64_page_reloc_encode_branch(decoded, ghost_pc, slot_pc, instruction);
    if (status) return status;

    status = arm64_page_reloc_make_jump(slot_pc, target, slot, ARM64_PAGE_RELOC_SLOT_WORDS);
    if (status) return status;
    return 1;
}

// 重编码 ADR/ADRP；超出范围时在回放槽装载原始绝对目标地址。返回 1 表示占用了当前槽。
static inline int arm64_page_reloc_pc_address(const struct arm64_decoded_insn *decoded, uint64_t source_pc, uint64_t ghost_pc, uint32_t *instruction, uint64_t slot_pc, uint32_t *slot)
{
    bool page_relative = decoded->instruction == ARM64_INSN_ADRP;
    uint64_t source_base = page_relative ? source_pc & ~0xFFFULL : source_pc;
    uint64_t target = source_base + decoded->offset;

    int status = page_relative ? arm64_encode_adrp(decoded->rd, ghost_pc, target, instruction) : arm64_encode_adr(decoded->rd, ghost_pc, target, instruction);
    if (status != -ERANGE) return status;

    status = arm64_encode_b(ghost_pc, slot_pc, instruction);
    if (status) return status;

    status = arm64_encode_fill_nops(slot, ARM64_PAGE_RELOC_SLOT_WORDS);
    if (status) return status;
    status = arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, decoded->rd, slot_pc, slot_pc + 2 * sizeof(uint32_t), &slot[0]);
    if (status) return status;
    status = arm64_encode_b(slot_pc + sizeof(uint32_t), ghost_pc + sizeof(uint32_t), &slot[1]);
    if (status) return status;
    __builtin_memcpy(&slot[2], &target, sizeof(target));
    return 1;
}

// 重编码字面量访问；超出范围时在回放槽按原访问宽度和目标寄存器执行。返回 1 表示占用了当前槽。
static inline int arm64_page_reloc_literal(const struct arm64_decoded_insn *decoded, uint64_t source_pc, uint64_t ghost_pc, uint32_t *instruction, uint64_t slot_pc, uint32_t *slot)
{
    uint64_t target = source_pc + decoded->offset;
    int status;

    if (decoded->instruction == ARM64_INSN_PRFM_LITERAL) status = arm64_encode_prfm_literal(decoded->prefetch_operation, ghost_pc, target, instruction);
    else
    {
        enum arm64_encode_load_store_kind kind;
        status = arm64_page_reloc_load_store_kind(decoded, &kind);
        if (status) return status;
        status = arm64_encode_ldr_literal(kind, decoded->rt, ghost_pc, target, instruction);
    }
    if (status != -ERANGE) return status;

    // PRFM 是可被实现忽略的预取提示，超范围时用 NOP 保持无架构可见副作用。
    if (decoded->instruction == ARM64_INSN_PRFM_LITERAL) return arm64_encode_nop(instruction);

    status = arm64_encode_b(ghost_pc, slot_pc, instruction);
    if (status) return status;

    status = arm64_encode_fill_nops(slot, ARM64_PAGE_RELOC_SLOT_WORDS);
    if (status) return status;
    __builtin_memcpy(&slot[6], &target, sizeof(target));

    enum arm64_encode_load_store_kind kind;
    status = arm64_page_reloc_load_store_kind(decoded, &kind);
    if (status) return status;

    if ((decoded->instruction == ARM64_INSN_LDR_LITERAL_GPR || decoded->instruction == ARM64_INSN_LDRSW_LITERAL) && decoded->rt != 31)
    {
        status = arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, decoded->rt, slot_pc, slot_pc + 6 * sizeof(uint32_t), &slot[0]);
        if (status) return status;
        status = arm64_encode_load_store_unsigned(true, kind, decoded->rt, decoded->rt, 0, &slot[1]);
        if (status) return status;
        status = arm64_encode_b(slot_pc + 2 * sizeof(uint32_t), ghost_pc + sizeof(uint32_t), &slot[2]);
        return status ? status : 1;
    }

    status = arm64_encode_load_store_x_indexed(false, true, 16, 31, -16, &slot[0]);
    if (status) return status;
    status = arm64_encode_ldr_literal(ARM64_ENCODE_LS_GPR64, 16, slot_pc + sizeof(uint32_t), slot_pc + 6 * sizeof(uint32_t), &slot[1]);
    if (status) return status;
    status = arm64_encode_load_store_unsigned(true, kind, decoded->rt, 16, 0, &slot[2]);
    if (status) return status;
    status = arm64_encode_load_store_x_indexed(true, false, 16, 31, 16, &slot[3]);
    if (status) return status;
    status = arm64_encode_b(slot_pc + 4 * sizeof(uint32_t), ghost_pc + sizeof(uint32_t), &slot[4]);
    return status ? status : 1;
}

// 按 PC 相对指令语义选择目标地址并重编码；返回 1 表示使用槽，非 PC 相对指令返回 0。
static inline int arm64_page_reloc_instruction(const struct arm64_page_relocation *relocation, const struct arm64_decoded_insn *decoded, uint64_t source_pc, uint64_t ghost_pc, uint32_t *instruction, uint64_t slot_pc, uint32_t *slot)
{
    switch (decoded->insn_class)
    {
    case ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE:
        switch (decoded->instruction)
        {
        case ARM64_INSN_ADR:
        case ARM64_INSN_ADRP:
            return arm64_page_reloc_pc_address(decoded, source_pc, ghost_pc, instruction, slot_pc, slot);
        default:
            return 0;
        }
    case ARM64_INSN_CLASS_LOAD_STORE:
        switch (decoded->instruction)
        {
        case ARM64_INSN_LDR_LITERAL_GPR:
        case ARM64_INSN_LDRSW_LITERAL:
        case ARM64_INSN_LDR_LITERAL_FP_SIMD:
        case ARM64_INSN_PRFM_LITERAL:
            return arm64_page_reloc_literal(decoded, source_pc, ghost_pc, instruction, slot_pc, slot);
        default:
            return 0;
        }
    case ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM:
        switch (decoded->instruction)
        {
        case ARM64_INSN_B:
        case ARM64_INSN_BL:
        case ARM64_INSN_B_COND:
        case ARM64_INSN_CBZ:
        case ARM64_INSN_CBNZ:
        case ARM64_INSN_TBZ:
        case ARM64_INSN_TBNZ:
            return arm64_page_reloc_branch(relocation, decoded, source_pc, ghost_pc, instruction, slot_pc, slot);
        default:
            return 0;
        }
    default:
        return 0;
    }
}

/*
以下函数是提供给外部模块调用的 ARM64 单页重定位接口=====================================================================
*/

// 将源页地址转换为幽灵页中的同偏移地址。
static inline uint64_t arm64_page_reloc_to_ghost(const struct arm64_page_relocation *relocation, uint64_t source_pc)
{
    if (!relocation || source_pc < relocation->source_page || source_pc >= relocation->source_page + PAGE_SIZE) return 0;
    return relocation->ghost_page + (source_pc - relocation->source_page);
}

// 将幽灵页地址转换为源页中的同偏移地址。
static inline uint64_t arm64_page_reloc_to_source(const struct arm64_page_relocation *relocation, uint64_t ghost_pc)
{
    if (!relocation || ghost_pc < relocation->ghost_page || ghost_pc >= relocation->ghost_page + PAGE_SIZE) return 0;
    return relocation->source_page + (ghost_pc - relocation->ghost_page);
}

// 将完整源页按原偏移重定位到幽灵页，并在页尾追加返回原页下一页的长跳转。
static inline int arm64_page_relocate(uint64_t source_page, uint64_t ghost_page, const void *source_code, void *output_code, size_t output_capacity, struct arm64_page_relocation *relocation)
{
    if (!source_code || !output_code || !relocation || !source_page || !ghost_page) return -EINVAL;
    if ((source_page & ~PAGE_MASK) || (ghost_page & ~PAGE_MASK) || output_capacity < ARM64_PAGE_RELOC_MAX_BYTES) return -EINVAL;

    __builtin_memset(relocation, 0, sizeof(*relocation));
    relocation->source_page = source_page;
    relocation->ghost_page = ghost_page;

    const uint32_t *source = source_code;
    uint32_t *output = output_code;
    __builtin_memset(output, 0, ARM64_PAGE_RELOC_MAX_BYTES);
    uint32_t slot_index = ARM64_PAGE_RELOC_SLOT_BASE_WORD;

    for (uint32_t source_index = 0; source_index < ARM64_PAGE_RELOC_SOURCE_WORDS; source_index++)
    {
        uint64_t source_pc = source_page + (uint64_t)source_index * sizeof(uint32_t);
        uint64_t ghost_pc = ghost_page + (uint64_t)source_index * sizeof(uint32_t);
        uint64_t slot_pc = ghost_page + (uint64_t)slot_index * sizeof(uint32_t);
        struct arm64_decoded_insn decoded;

        enum arm64_decode_status decode_status = arm64_decode_insn(source[source_index], &decoded);
        if (decode_status != ARM64_DECODE_OK)
        {
            __builtin_memset(relocation, 0, sizeof(*relocation));
            return -EOPNOTSUPP;
        }

        output[source_index] = source[source_index];
        {
            int status = arm64_page_reloc_instruction(relocation, &decoded, source_pc, ghost_pc, &output[source_index], slot_pc, &output[slot_index]);

            if (status < 0)
            {
                __builtin_memset(relocation, 0, sizeof(*relocation));
                return status;
            }
            if (status > 0) slot_index += ARM64_PAGE_RELOC_SLOT_WORDS;
        }
    }

    int status = arm64_page_reloc_make_jump(ghost_page + PAGE_SIZE, source_page + PAGE_SIZE, &output[ARM64_PAGE_RELOC_SOURCE_WORDS], ARM64_PAGE_RELOC_RETURN_WORDS);
    if (status)
    {
        __builtin_memset(relocation, 0, sizeof(*relocation));
        return status;
    }
    relocation->slot_count = (slot_index - ARM64_PAGE_RELOC_SLOT_BASE_WORD) / ARM64_PAGE_RELOC_SLOT_WORDS;
    relocation->code_size = (size_t)slot_index * sizeof(uint32_t);
    return 0;
}

#endif