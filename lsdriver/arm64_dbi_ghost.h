#ifndef ARM64_DBI_GHOST_H
#define ARM64_DBI_GHOST_H

#include <linux/kernel.h>
#include <linux/mm.h>
#include <linux/mutex.h>
#include <linux/pid.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/vmalloc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <asm/tlbflush.h>

#include "export_fun.h"
#include "lsdriver_log.h"

/*
 ARM64 单页 DBI 重编译器：输入一页原始指令和对应 ghost 页地址，输出可在
  ghost VA 执行的等价指令流，并维护 target PC 到 ghost PC 的映射。

  这一层只负责指令重写，不分配内存、不改页表、不处理异常入口。
*/
#define ARM64_DBI_TARGET_SIZE          4096
#define ARM64_DBI_TARGET_INSNS         (ARM64_DBI_TARGET_SIZE / 4)
#define ARM64_DBI_GHOST_MAX_INSNS      (ARM64_DBI_TARGET_INSNS * 8)
#define ARM64_DBI_GHOST_MAX_BYTES      (ARM64_DBI_GHOST_MAX_INSNS * 4)
#define ARM64_DBI_SCRATCH_REG          17
#define ARM64_DBI_MAX_PENDING_BRANCHES 512

struct arm64_dbi_pending_branch
{
    // 前向同页分支第一次扫描时目标 ghost 偏移未知，先占位，整页完成后回填。
    int ghost_idx;
    u32 enc_template;
    u16 target_tidx;
    u8 kind;
};

struct arm64_dbi_ctx
{
    // target_page/orig 是原用户页快照，ghost_page/ghost 是可执行影子页。
    u64 target_page;
    u64 ghost_page;
    const u32 *orig;
    u32 *ghost;
    int ghost_capacity;
    int ghost_count;
    // offset_map[i] 记录原页第 i 条指令在 ghost 中的指令下标。
    u16 offset_map[ARM64_DBI_TARGET_INSNS];
    struct arm64_dbi_pending_branch pending[ARM64_DBI_MAX_PENDING_BRANCHES];
    int n_pending;
    // 下面是调试统计，便于后面判断哪些指令被原样保留或展开。
    int fixed;
    int expanded;
    int passthrough;
    int failed;
    int intra_page_fixed;
};

// ARM64 立即数字段按指定位宽做符号扩展。
static inline s64 arm64_dbi_sign_extend(u64 val, int bits)
{
    s64 mask = 1LL << (bits - 1);
    return (s64)((val ^ mask) - mask);
}

// 下面这一组只负责编码少量 DBI 需要生成的 ARM64 指令。
static inline u32 arm64_dbi_enc_br(u32 rn)
{
    return 0xD61F0000U | ((rn & 0x1FU) << 5);
}

static inline u32 arm64_dbi_enc_nop(void)
{
    return 0xD503201FU;
}

static inline int arm64_dbi_enc_b(s64 offset, u32 *out)
{
    s64 imm26 = offset / 4;
    if (imm26 < -(1LL << 25) || imm26 >= (1LL << 25)) return -ERANGE;
    *out = 0x14000000U | ((u32)imm26 & 0x03FFFFFFU);
    return 0;
}

static inline int arm64_dbi_enc_bl(s64 offset, u32 *out)
{
    s64 imm26 = offset / 4;
    if (imm26 < -(1LL << 25) || imm26 >= (1LL << 25)) return -ERANGE;
    *out = 0x94000000U | ((u32)imm26 & 0x03FFFFFFU);
    return 0;
}

static inline int arm64_dbi_enc_b_cond(u32 cond, s64 offset, u32 *out)
{
    s64 imm19 = offset / 4;
    if (imm19 < -(1LL << 18) || imm19 >= (1LL << 18)) return -ERANGE;
    *out = 0x54000000U | (((u32)imm19 & 0x7FFFFU) << 5) | (cond & 0xFU);
    return 0;
}

static inline int arm64_dbi_enc_cbz(u32 sf, u32 rt, s64 offset, int is_nz, u32 *out)
{
    s64 imm19 = offset / 4;
    u32 op = is_nz ? 0x35000000U : 0x34000000U;
    if (imm19 < -(1LL << 18) || imm19 >= (1LL << 18)) return -ERANGE;
    *out = op | ((sf & 1U) << 31) | (((u32)imm19 & 0x7FFFFU) << 5) | (rt & 0x1FU);
    return 0;
}

static inline int arm64_dbi_enc_tbz(u32 rt, u32 bit, s64 offset, int is_nz, u32 *out)
{
    s64 imm14 = offset / 4;
    u32 op = is_nz ? 0x37000000U : 0x36000000U;
    u32 b5 = (bit >> 5) & 1U;
    u32 b40 = bit & 0x1FU;
    if (imm14 < -(1LL << 13) || imm14 >= (1LL << 13)) return -ERANGE;
    *out = op | (b5 << 31) | (b40 << 19) | (((u32)imm14 & 0x3FFFU) << 5) | (rt & 0x1FU);
    return 0;
}

static inline u32 arm64_dbi_enc_ldr_imm_unsigned(u32 rt, u32 rn, u32 size)
{
    return ((size & 3U) << 30) | 0x39400000U | ((rn & 0x1FU) << 5) | (rt & 0x1FU);
}

static inline u32 arm64_dbi_enc_ldrsw_imm_unsigned(u32 rt, u32 rn)
{
    return 0xB9800000U | ((rn & 0x1FU) << 5) | (rt & 0x1FU);
}

static inline u32 arm64_dbi_enc_ldr_vec_imm_unsigned(u32 rt, u32 simd_opc)
{
    u32 size_hi;
    u32 opc;

    if (simd_opc == 2)
    {
        size_hi = 0;
        opc = 3;
    }
    else if (simd_opc == 1)
    {
        size_hi = 3;
        opc = 1;
    }
    else
    {
        size_hi = 2;
        opc = 1;
    }

    return (size_hi << 30) | 0x3D400000U | (opc << 22) | ((ARM64_DBI_SCRATCH_REG & 0x1FU) << 5) | (rt & 0x1FU);
}

static inline int arm64_dbi_emit_mov_imm64(u32 rd, u64 imm, u32 *out, int max_insns)
{
    int n = 0;
    int first = 1;
    int shift;

    for (shift = 0; shift < 64; shift += 16)
    {
        u16 chunk = (u16)((imm >> shift) & 0xFFFFU);
        u32 hw = (u32)(shift / 16);

        if (chunk == 0)
        {
            if (!first) continue;
            if (imm != 0) continue;
        }

        if (n >= max_insns) return -ENOSPC;

        if (first)
        {
            out[n++] = 0xD2800000U | (hw << 21) | ((u32)chunk << 5) | (rd & 0x1FU);
            first = 0;
        }
        else
        {
            out[n++] = 0xF2800000U | (hw << 21) | ((u32)chunk << 5) | (rd & 0x1FU);
        }
    }

    if (first)
    {
        if (n >= max_insns) return -ENOSPC;
        out[n++] = 0xD2800000U | (rd & 0x1FU);
    }

    return n;
}

static inline int arm64_dbi_emit(struct arm64_dbi_ctx *ctx, u32 insn)
{
    // 所有重写指令都走统一出口，集中做 ghost 容量保护。
    if (ctx->ghost_count >= ctx->ghost_capacity)
    {
        ls_log_tag("ptebp", "ghost instruction capacity exhausted target=0x%llx count=%d capacity=%d\n", ctx->target_page, ctx->ghost_count, ctx->ghost_capacity);
        return -ENOSPC;
    }
    ctx->ghost[ctx->ghost_count++] = insn;
    return 0;
}

static inline u64 arm64_dbi_ghost_cur_pc(const struct arm64_dbi_ctx *ctx)
{
    return ctx->ghost_page + (u64)ctx->ghost_count * 4;
}

static inline int arm64_dbi_emit_far_jump(struct arm64_dbi_ctx *ctx, u64 target)
{
    // 分支距离超出 ARM64 立即数范围时，用 x17 装载绝对地址后 BR 跳转。
    u32 movs[4];
    int n = arm64_dbi_emit_mov_imm64(ARM64_DBI_SCRATCH_REG, target, movs, 4);
    int i;

    if (n < 0) return n;
    for (i = 0; i < n; i++)
    {
        int status = arm64_dbi_emit(ctx, movs[i]);
        if (status) return status;
    }
    return arm64_dbi_emit(ctx, arm64_dbi_enc_br(ARM64_DBI_SCRATCH_REG));
}

static inline int arm64_dbi_emit_skip_far_jump(struct arm64_dbi_ctx *ctx, u64 target, u32 enc_template, int imm_bits)
{
    int branch_idx = ctx->ghost_count;
    int after_idx;
    int status;

    if (arm64_dbi_emit(ctx, 0)) return -ENOSPC;
    status = arm64_dbi_emit_far_jump(ctx, target);
    if (status) return status;

    after_idx = ctx->ghost_count;
    return arm64_patch_signed_imm_field(&ctx->ghost[branch_idx], enc_template, after_idx - branch_idx, imm_bits, 5);
}

static inline int arm64_dbi_emit_load_addr(struct arm64_dbi_ctx *ctx, u32 rd, u64 addr)
{
    u32 movs[4];
    int n = arm64_dbi_emit_mov_imm64(rd, addr, movs, 4);
    int i;

    if (n < 0) return n;
    for (i = 0; i < n; i++)
    {
        int status = arm64_dbi_emit(ctx, movs[i]);
        if (status) return status;
    }
    return 0;
}

static inline bool arm64_dbi_is_intra_page(const struct arm64_dbi_ctx *ctx, u64 target)
{
    return target >= ctx->target_page && target < ctx->target_page + ARM64_DBI_TARGET_SIZE;
}

static inline u64 arm64_dbi_intra_page_target(const struct arm64_dbi_ctx *ctx, u64 target)
{
    // 后向同页分支可以直接从 offset_map 找到已生成的 ghost 目标。
    u32 tidx = (u32)((target - ctx->target_page) >> 2);
    return ctx->ghost_page + (u64)ctx->offset_map[tidx] * 4;
}

static inline int arm64_dbi_queue_branch(struct arm64_dbi_ctx *ctx, int ghost_idx, u32 enc_template, u16 target_tidx, u8 kind)
{
    // 前向同页分支目标尚未生成，先记录占位指令下标和原页目标下标。
    if (ctx->n_pending >= ARM64_DBI_MAX_PENDING_BRANCHES)
    {
        ls_log_tag("ptebp", "pending branch capacity exhausted target=0x%llx pending=%d max=%d\n", ctx->target_page, ctx->n_pending, ARM64_DBI_MAX_PENDING_BRANCHES);
        return -ENOSPC;
    }
    ctx->pending[ctx->n_pending].ghost_idx = ghost_idx;
    ctx->pending[ctx->n_pending].enc_template = enc_template;
    ctx->pending[ctx->n_pending].target_tidx = target_tidx;
    ctx->pending[ctx->n_pending].kind = kind;
    ctx->n_pending++;
    return 0;
}

static inline int arm64_dbi_emit_pending_branch(struct arm64_dbi_ctx *ctx, u32 enc_template, u16 target_tidx, u8 kind)
{
    int ghost_idx = ctx->ghost_count;

    if (arm64_dbi_emit(ctx, enc_template)) return -ENOSPC;

    ctx->intra_page_fixed++;
    return arm64_dbi_queue_branch(ctx, ghost_idx, enc_template, target_tidx, kind);
}

static inline int arm64_dbi_recomp_b(struct arm64_dbi_ctx *ctx, u32 insn, u64 orig_pc)
{
    // B 指令可以优先尝试重编码；距离过远才展开成绝对跳转。
    s64 imm26 = arm64_dbi_sign_extend(insn & 0x03FFFFFFU, 26);
    u64 target = orig_pc + ((u64)imm26 << 2);
    u32 enc;

    if (arm64_dbi_is_intra_page(ctx, target))
    {
        u32 tidx = (u32)((target - ctx->target_page) >> 2);
        u32 this_tidx = (u32)((orig_pc - ctx->target_page) >> 2);
        if (tidx < this_tidx)
        {
            u64 gtarget = arm64_dbi_intra_page_target(ctx, target);
            if (!arm64_dbi_enc_b((s64)(gtarget - arm64_dbi_ghost_cur_pc(ctx)), &enc))
            {
                ctx->intra_page_fixed++;
                return arm64_dbi_emit(ctx, enc);
            }
        }
        else
        {
            return arm64_dbi_emit_pending_branch(ctx, 0x14000000U, (u16)tidx, 3);
        }
    }

    if (!arm64_dbi_enc_b((s64)(target - arm64_dbi_ghost_cur_pc(ctx)), &enc))
    {
        ctx->fixed++;
        return arm64_dbi_emit(ctx, enc);
    }

    ctx->expanded++;
    return arm64_dbi_emit_far_jump(ctx, target);
}

static inline int arm64_dbi_recomp_bl(struct arm64_dbi_ctx *ctx, u32 insn, u64 orig_pc)
{
    s64 imm26 = arm64_dbi_sign_extend(insn & 0x03FFFFFFU, 26);
    u64 target = orig_pc + ((u64)imm26 << 2);
    u32 enc;

    if (!arm64_dbi_enc_bl((s64)(target - arm64_dbi_ghost_cur_pc(ctx)), &enc))
    {
        ctx->fixed++;
        return arm64_dbi_emit(ctx, enc);
    }

    ctx->expanded++;
    if (arm64_dbi_emit_load_addr(ctx, ARM64_DBI_SCRATCH_REG, target)) return -ENOSPC;
    return arm64_dbi_emit(ctx, 0xD63F0000U | ((u32)ARM64_DBI_SCRATCH_REG << 5));
}

static inline int arm64_dbi_recomp_bcond(struct arm64_dbi_ctx *ctx, u32 insn, u64 orig_pc)
{
    // 条件分支过远时，反转条件跳过一段远跳 stub。
    s64 imm19 = arm64_dbi_sign_extend((insn >> 5) & 0x7FFFFU, 19);
    u64 target = orig_pc + ((u64)imm19 << 2);
    u32 cond = insn & 0xFU;
    u32 enc;

    if (arm64_dbi_is_intra_page(ctx, target))
    {
        u32 tidx = (u32)((target - ctx->target_page) >> 2);
        u32 this_tidx = (u32)((orig_pc - ctx->target_page) >> 2);
        if (tidx < this_tidx)
        {
            u64 gtarget = arm64_dbi_intra_page_target(ctx, target);
            if (!arm64_dbi_enc_b_cond(cond, (s64)(gtarget - arm64_dbi_ghost_cur_pc(ctx)), &enc))
            {
                ctx->intra_page_fixed++;
                return arm64_dbi_emit(ctx, enc);
            }
        }
        else
        {
            u32 tpl = 0x54000000U | (cond & 0xFU);
            return arm64_dbi_emit_pending_branch(ctx, tpl, (u16)tidx, 0);
        }
    }

    if (!arm64_dbi_enc_b_cond(cond, (s64)(target - arm64_dbi_ghost_cur_pc(ctx)), &enc))
    {
        ctx->fixed++;
        return arm64_dbi_emit(ctx, enc);
    }

    ctx->expanded++;
    return arm64_dbi_emit_skip_far_jump(ctx, target, 0x54000000U | ((cond ^ 1U) & 0xFU), 19);
}

static inline int arm64_dbi_recomp_cbz(struct arm64_dbi_ctx *ctx, u32 insn, u64 orig_pc)
{
    int is_nz = !!(insn & (1U << 24));
    u32 sf = (insn >> 31) & 1U;
    u32 rt = insn & 0x1FU;
    s64 imm19 = arm64_dbi_sign_extend((insn >> 5) & 0x7FFFFU, 19);
    u64 target = orig_pc + ((u64)imm19 << 2);
    u32 enc;

    if (arm64_dbi_is_intra_page(ctx, target))
    {
        u32 tidx = (u32)((target - ctx->target_page) >> 2);
        u32 this_tidx = (u32)((orig_pc - ctx->target_page) >> 2);
        if (tidx < this_tidx)
        {
            u64 gtarget = arm64_dbi_intra_page_target(ctx, target);
            if (!arm64_dbi_enc_cbz(sf, rt, (s64)(gtarget - arm64_dbi_ghost_cur_pc(ctx)), is_nz, &enc))
            {
                ctx->intra_page_fixed++;
                return arm64_dbi_emit(ctx, enc);
            }
        }
        else
        {
            u32 op = is_nz ? 0x35000000U : 0x34000000U;
            u32 tpl = op | ((sf & 1U) << 31) | (rt & 0x1FU);
            return arm64_dbi_emit_pending_branch(ctx, tpl, (u16)tidx, 1);
        }
    }

    if (!arm64_dbi_enc_cbz(sf, rt, (s64)(target - arm64_dbi_ghost_cur_pc(ctx)), is_nz, &enc))
    {
        ctx->fixed++;
        return arm64_dbi_emit(ctx, enc);
    }

    ctx->expanded++;
    return arm64_dbi_emit_skip_far_jump(ctx, target, (is_nz ? 0x34000000U : 0x35000000U) | ((sf & 1U) << 31) | (rt & 0x1FU), 19);
}

static inline int arm64_dbi_recomp_tbz(struct arm64_dbi_ctx *ctx, u32 insn, u64 orig_pc)
{
    int is_nz = !!(insn & (1U << 24));
    u32 rt = insn & 0x1FU;
    u32 bit = (((insn >> 31) & 1U) << 5) | ((insn >> 19) & 0x1FU);
    s64 imm14 = arm64_dbi_sign_extend((insn >> 5) & 0x3FFFU, 14);
    u64 target = orig_pc + ((u64)imm14 << 2);
    u32 enc;

    if (arm64_dbi_is_intra_page(ctx, target))
    {
        u32 tidx = (u32)((target - ctx->target_page) >> 2);
        u32 this_tidx = (u32)((orig_pc - ctx->target_page) >> 2);
        if (tidx < this_tidx)
        {
            u64 gtarget = arm64_dbi_intra_page_target(ctx, target);
            if (!arm64_dbi_enc_tbz(rt, bit, (s64)(gtarget - arm64_dbi_ghost_cur_pc(ctx)), is_nz, &enc))
            {
                ctx->intra_page_fixed++;
                return arm64_dbi_emit(ctx, enc);
            }
        }
        else
        {
            u32 op = is_nz ? 0x37000000U : 0x36000000U;
            u32 tpl = op | (((bit >> 5) & 1U) << 31) | ((bit & 0x1FU) << 19) | (rt & 0x1FU);
            return arm64_dbi_emit_pending_branch(ctx, tpl, (u16)tidx, 2);
        }
    }

    if (!arm64_dbi_enc_tbz(rt, bit, (s64)(target - arm64_dbi_ghost_cur_pc(ctx)), is_nz, &enc))
    {
        ctx->fixed++;
        return arm64_dbi_emit(ctx, enc);
    }

    ctx->expanded++;
    return arm64_dbi_emit_skip_far_jump(ctx, target, (is_nz ? 0x36000000U : 0x37000000U) | (((bit >> 5) & 1U) << 31) | ((bit & 0x1FU) << 19) | (rt & 0x1FU), 14);
}

static inline int arm64_dbi_recomp_adrp(struct arm64_dbi_ctx *ctx, u32 insn, u64 orig_pc)
{
    // ADRP/ADR 是 PC 相对取址，搬到 ghost 后必须改为装载原始目标绝对地址。
    u32 rd = insn & 0x1FU;
    u64 immlo = (insn >> 29) & 0x3U;
    u64 immhi = (insn >> 5) & 0x7FFFFU;
    s64 imm21 = arm64_dbi_sign_extend((immhi << 2) | immlo, 21);
    u64 target = (orig_pc & ~0xFFFULL) + ((u64)imm21 << 12);
    ctx->expanded++;
    return arm64_dbi_emit_load_addr(ctx, rd, target);
}

static inline int arm64_dbi_recomp_adr(struct arm64_dbi_ctx *ctx, u32 insn, u64 orig_pc)
{
    u32 rd = insn & 0x1FU;
    u64 immlo = (insn >> 29) & 0x3U;
    u64 immhi = (insn >> 5) & 0x7FFFFU;
    s64 imm21 = arm64_dbi_sign_extend((immhi << 2) | immlo, 21);
    u64 target = orig_pc + (u64)imm21;
    ctx->expanded++;
    return arm64_dbi_emit_load_addr(ctx, rd, target);
}

static inline int arm64_dbi_recomp_ldr_lit(struct arm64_dbi_ctx *ctx, u32 insn, u64 orig_pc, int variant)
{
    // LDR literal 的数据地址也是 PC 相对，ghost 中改成先取原地址再从 [x17] 读。
    u32 rt = insn & 0x1FU;
    s64 imm19 = arm64_dbi_sign_extend((insn >> 5) & 0x7FFFFU, 19);
    u64 data_addr = orig_pc + ((u64)imm19 << 2);
    u32 ldr;

    ctx->expanded++;
    if (arm64_dbi_emit_load_addr(ctx, ARM64_DBI_SCRATCH_REG, data_addr)) return -ENOSPC;
    if (variant == 2) ldr = arm64_dbi_enc_ldrsw_imm_unsigned(rt, ARM64_DBI_SCRATCH_REG);
    else if (variant == 0) ldr = arm64_dbi_enc_ldr_imm_unsigned(rt, ARM64_DBI_SCRATCH_REG, 2);
    else ldr = arm64_dbi_enc_ldr_imm_unsigned(rt, ARM64_DBI_SCRATCH_REG, 3);
    return arm64_dbi_emit(ctx, ldr);
}

static inline int arm64_dbi_recomp_ldr_vec_lit(struct arm64_dbi_ctx *ctx, u32 insn, u64 orig_pc)
{
    u32 rt = insn & 0x1FU;
    u32 opc = (insn >> 30) & 0x3U;
    s64 imm19 = arm64_dbi_sign_extend((insn >> 5) & 0x7FFFFU, 19);
    u64 data_addr = orig_pc + ((u64)imm19 << 2);

    if (opc == 3) return -EINVAL;
    ctx->expanded++;
    if (arm64_dbi_emit_load_addr(ctx, ARM64_DBI_SCRATCH_REG, data_addr)) return -ENOSPC;
    return arm64_dbi_emit(ctx, arm64_dbi_enc_ldr_vec_imm_unsigned(rt, opc));
}

static inline int arm64_dbi_recompile_page(struct arm64_dbi_ctx *ctx)
{
    // 单次线性扫描生成 ghost 页；前向同页分支在扫描结束后统一回填。
    int i;

    if (!ctx || !ctx->orig || !ctx->ghost || ctx->ghost_capacity < ARM64_DBI_TARGET_INSNS) return -EINVAL;

    ctx->ghost_count = 0;
    ctx->fixed = 0;
    ctx->expanded = 0;
    ctx->passthrough = 0;
    ctx->failed = 0;
    ctx->intra_page_fixed = 0;
    ctx->n_pending = 0;

    for (i = 0; i < ARM64_DBI_TARGET_INSNS; i++)
    {
        u32 insn = ctx->orig[i];
        u64 orig_pc = ctx->target_page + (u64)i * 4;
        int prev_ghost_idx = ctx->ghost_count;
        int status;

        ctx->offset_map[i] = (u16)prev_ghost_idx;

        if (insn == 0xD503201FU)
        {
            ctx->passthrough++;
            status = arm64_dbi_emit(ctx, insn);
        }
        else if ((insn & 0xFC000000U) == 0x14000000U) status = arm64_dbi_recomp_b(ctx, insn, orig_pc);
        else if ((insn & 0xFC000000U) == 0x94000000U) status = arm64_dbi_recomp_bl(ctx, insn, orig_pc);
        else if ((insn & 0xFF000010U) == 0x54000000U) status = arm64_dbi_recomp_bcond(ctx, insn, orig_pc);
        else if ((insn & 0x7E000000U) == 0x34000000U) status = arm64_dbi_recomp_cbz(ctx, insn, orig_pc);
        else if ((insn & 0x7E000000U) == 0x36000000U) status = arm64_dbi_recomp_tbz(ctx, insn, orig_pc);
        else if ((insn & 0x9F000000U) == 0x90000000U) status = arm64_dbi_recomp_adrp(ctx, insn, orig_pc);
        else if ((insn & 0x9F000000U) == 0x10000000U) status = arm64_dbi_recomp_adr(ctx, insn, orig_pc);
        else if ((insn & 0xFF000000U) == 0x18000000U) status = arm64_dbi_recomp_ldr_lit(ctx, insn, orig_pc, 0);
        else if ((insn & 0xFF000000U) == 0x58000000U) status = arm64_dbi_recomp_ldr_lit(ctx, insn, orig_pc, 1);
        else if ((insn & 0xFF000000U) == 0x98000000U) status = arm64_dbi_recomp_ldr_lit(ctx, insn, orig_pc, 2);
        else if ((insn & 0xFF000000U) == 0xD8000000U)
        {
            ctx->expanded++;
            status = arm64_dbi_emit(ctx, arm64_dbi_enc_nop());
        }
        else if ((insn & 0x3F000000U) == 0x1C000000U) status = arm64_dbi_recomp_ldr_vec_lit(ctx, insn, orig_pc);
        else
        {
            ctx->passthrough++;
            status = arm64_dbi_emit(ctx, insn);
        }

        if (status)
        {
            ctx->failed++;
            ctx->ghost_count = prev_ghost_idx;
            if (arm64_dbi_emit(ctx, arm64_dbi_enc_nop())) return -ENOSPC;
        }
    }

    for (i = 0; i < ctx->n_pending; i++)
    {
        struct arm64_dbi_pending_branch *branch = &ctx->pending[i];
        u64 target_ghost = ctx->ghost_page + (u64)ctx->offset_map[branch->target_tidx] * 4;
        u64 patch_pc = ctx->ghost_page + (u64)branch->ghost_idx * 4;
        s64 delta = (s64)(target_ghost - patch_pc);
        s64 imm = delta / 4;
        u32 enc = branch->enc_template;
        int imm_bits = 26;
        int imm_shift = 0;

        if (branch->kind == 0 || branch->kind == 1)
        {
            imm_bits = 19;
            imm_shift = 5;
        }
        else if (branch->kind == 2)
        {
            imm_bits = 14;
            imm_shift = 5;
        }

        if (arm64_patch_signed_imm_field(&ctx->ghost[branch->ghost_idx], enc, imm, imm_bits, imm_shift))
        {
            ctx->ghost[branch->ghost_idx] = arm64_dbi_enc_nop();
            ctx->failed++;
            continue;
        }
    }

    return 0;
}

static inline u64 arm64_dbi_target_to_ghost_pc(const struct arm64_dbi_ctx *ctx, u64 target_pc)
{
    // fault handler 只拿到原 PC，这里负责把它映射到 ghost 中对应指令。
    u64 off;
    u32 idx;
    u32 ghost_idx;

    if (!ctx || !ctx->ghost_page || target_pc < ctx->target_page) return 0;

    off = (target_pc & ~0x3ULL) - ctx->target_page;
    if (off >= ARM64_DBI_TARGET_SIZE) return 0;

    idx = (u32)(off >> 2);
    ghost_idx = ctx->offset_map[idx];
    if (ghost_idx >= (u32)ctx->ghost_count) return 0;

    return ctx->ghost_page + (u64)ghost_idx * 4;
}

#include "virtual_memory_rw.h"

/*
    ARM64 DBI ghost 托管层：
    1. install 一次完成原页快照、DBI 重编译、ghost 映射、slot 发布和 UXN 启用；
    2. fault handler 只通过 lookup_pc 查询 target PC 对应的 ghost PC；
    3. 上层卸载时只按 pid 调用 remove_all 恢复目标页并释放资源。
 */
#ifndef PTE_UXN
#define ARM64_DBI_GHOST_UXN (_AT(pteval_t, 1) << 54)
#else
#define ARM64_DBI_GHOST_UXN PTE_UXN
#endif

// ghost 用户 PTE 权限：用户态可执行、只读、PXN，映射普通内存属性。
#define ARM64_DBI_GHOST_PTE_FLAGS  (PTE_TYPE_PAGE | PTE_VALID | PTE_AF | PTE_SHARED | PTE_USER | PTE_RDONLY | PTE_PXN | PTE_ATTRINDX(MT_NORMAL))
#define ARM64_DBI_GHOST_SLOT_COUNT 16

struct arm64_dbi_ghost
{
    // armed 表示托管 slot 已发布，释放时需要恢复目标页 PTE。
    bool armed;
    u64 target_page;
    u64 ghost_page;
    // saved_pte 是目标页原始 PTE，用于安装时启用 UXN 和释放时恢复。
    pteval_t saved_pte;
    // target_copy 保存原页快照，ghost_copy 是内核分配并映射给目标进程执行的影子页。
    void *target_copy;
    void *ghost_copy;
    phys_addr_t ghost_pa;
    struct arm64_dbi_ctx dbi;
};

struct arm64_dbi_ghost_slot
{
    // 托管表按 pid + target_page 暴露给异步 fault handler 查询。
    bool used;
    pid_t pid;
    struct arm64_dbi_ghost ghost;
};

static struct arm64_dbi_ghost_slot g_arm64_dbi_ghost_slots[ARM64_DBI_GHOST_SLOT_COUNT];
static struct arm64_dbi_ghost_slot g_arm64_dbi_ghost_release_slots[ARM64_DBI_GHOST_SLOT_COUNT];
static DEFINE_SPINLOCK(g_arm64_dbi_ghost_lock);
static DEFINE_MUTEX(g_arm64_dbi_ghost_mutex);

// 在原始 PTE 上叠加 UXN，用来制造用户态取指 permission fault。
static inline pteval_t arm64_dbi_ghost_build_armed_pte(pteval_t saved_pte)
{
    return saved_pte | ARM64_DBI_GHOST_UXN;
}

// ghost 页最大可能膨胀到 ARM64_DBI_GHOST_MAX_BYTES，这里换算 buddy order。
static inline int arm64_dbi_ghost_order(void)
{
    return get_order(ARM64_DBI_GHOST_MAX_BYTES);
}

// 返回 ghost 映射需要连续安装多少个用户 PTE。
static inline int arm64_dbi_ghost_page_count(void)
{
    return 1 << arm64_dbi_ghost_order();
}

// 返回 ghost 连续物理页的总字节数。
static inline size_t arm64_dbi_ghost_size(void)
{
    return (size_t)arm64_dbi_ghost_page_count() * PAGE_SIZE;
}

// 卸载时恢复目标页原始 PTE。
static inline int arm64_dbi_ghost_restore_target_pte(pid_t pid, const struct arm64_dbi_ghost *ghost)
{
    int status;

    if (!ghost || !ghost->target_page) return -EINVAL;

    status = write_user_pte_value_by_pid(pid, ghost->target_page, ghost->saved_pte);
    if (!status) flush_tlb_all();
    return status;
}

// 在已持有 mmap 写锁的情况下，从 near 附近找一段没有 VMA 且 PTE 为空的用户 VA 空洞。
static inline u64 arm64_dbi_ghost_find_hole_locked(struct mm_struct *mm, u64 near, size_t size)
{
    static const u64 ranges[] = {
        16ULL * 1024 * 1024, 128ULL * 1024 * 1024, 1024ULL * 1024 * 1024, 16ULL * 1024 * 1024 * 1024, ~0ULL,
    };
    u64 near_page = near & PAGE_MASK;
    u64 user_hi;
    int range_index;

    if (!mm || !size) return 0;

    user_hi = (u64)mm->task_size & PAGE_MASK;
    if (user_hi <= PAGE_SIZE || size > user_hi - PAGE_SIZE) return 0;

    /*
     * Prefer a nearby ghost VA, but do not fail just because the target SO text
     * area is densely mapped. The DBI rewriter already emits absolute far jumps
     * for branches that cannot stay PC-relative.
     */
    for (range_index = 0; range_index < ARRAY_SIZE(ranges); range_index++)
    {
        u64 range = ranges[range_index];
        u64 lo;
        u64 hi;
        u64 addr;
        u64 best = 0;
        u64 best_dist = ~0ULL;

        if (range == ~0ULL)
        {
            lo = PAGE_SIZE;
            hi = user_hi;
        }
        else
        {
            lo = near_page > range ? near_page - range : PAGE_SIZE;
            if (near_page > ~0ULL - range) hi = user_hi;
            else hi = PAGE_ALIGN(near_page + range);
            if (hi > user_hi || hi < lo) hi = user_hi;
        }

        lo &= PAGE_MASK;
        if (lo < PAGE_SIZE) lo = PAGE_SIZE;
        hi &= PAGE_MASK;
        if (hi <= lo || hi - lo < size) continue;

        addr = lo;
        while (addr < hi)
        {
            struct vm_area_struct *vma = find_vma(mm, addr);
            u64 gap_start;
            u64 gap_end;
            u64 candidate;
            u64 dist;

            if (!vma || vma->vm_start >= hi)
            {
                gap_start = addr;
                gap_end = hi;
            }
            else if (vma->vm_start > addr)
            {
                gap_start = addr;
                gap_end = vma->vm_start;
            }
            else
            {
                addr = PAGE_ALIGN(vma->vm_end);
                continue;
            }

            gap_start = PAGE_ALIGN(gap_start);
            gap_end &= PAGE_MASK;
            if (gap_end >= gap_start + size)
            {
                if (near_page >= gap_start && near_page + size <= gap_end) candidate = near_page;
                else if (near_page < gap_start) candidate = gap_start;
                else candidate = gap_end - size;

                dist = candidate > near_page ? candidate - near_page : near_page - candidate;
                if (dist < best_dist && user_pte_range_empty(mm, candidate, size))
                {
                    best = candidate;
                    best_dist = dist;
                }
            }

            if (!vma || vma->vm_start >= hi) break;
            addr = PAGE_ALIGN(vma->vm_end);
        }

        if (best) return best;
    }

    return 0;
}

// 内核写完 ghost_copy 后，刷新到用户态取指可见。
static inline void arm64_dbi_ghost_sync_icache(void *addr, size_t size)
{
    // ghost_copy 是内核写入、用户态取指；写完后要把 D-cache 内容同步到 I-cache。
    unsigned long start = (unsigned long)addr;
    unsigned long end = start + size;
    unsigned long line;

    for (line = start & ~63UL; line < end; line += 64) asm volatile("dc cvau, %0" : : "r"(line) : "memory");

    asm volatile("dsb ish\n\t"
                 "ic ialluis\n\t"
                 "dsb ish\n\t"
                 "isb\n\t"
                 :
                 :
                 : "memory");
}

// 从目标进程 target_page 对应物理页读取 4KB 原始指令快照。
static inline int arm64_dbi_ghost_read_target_page(pid_t pid, u64 target_page, void *buffer)
{
    // 通过物理地址读原页，避免目标页权限变化影响内核侧快照。
    struct mm_struct *mm;
    phys_addr_t pa = 0;
    int status;

    if (!buffer) return -EINVAL;

    mm = get_mm_by_pid(pid);
    if (!mm) return -ESRCH;

    mmap_read_lock(mm);
    status = mmu_translate_va_to_pa(mm, target_page, &pa);
    mmap_read_unlock(mm);
    mmput(mm);
    if (status) return status;

    return pte_read_physical(pa, buffer, PAGE_SIZE);
}

// 在目标进程页表中安装 ghost_page -> ghost_pa 的连续 PTE 映射。
static inline int arm64_dbi_ghost_install_ptes(struct mm_struct *mm, struct arm64_dbi_ghost *ghost)
{
    // ghost 页没有 VMA，只能直接补齐页表层级并手动写 PTE。
    int index;
    int installed = 0;
    int page_count = arm64_dbi_ghost_page_count();
    int status = 0;

    if (!mm || !ghost || !ghost->ghost_page || !ghost->ghost_pa) return -EINVAL;

    for (index = 0; index < page_count; index++)
    {
        u64 ghost_addr = ghost->ghost_page + (u64)index * PAGE_SIZE;
        phys_addr_t ghost_pa = ghost->ghost_pa + (phys_addr_t)index * PAGE_SIZE;
        pte_t *ptep;
        pte_t new_pte;

        ptep = get_or_alloc_user_pte(mm, ghost_addr);
        if (!ptep)
        {
            status = -ENOMEM;
            goto err_clear;
        }
        if (pte_present(READ_ONCE(*ptep)))
        {
            status = -EEXIST;
            goto err_clear;
        }

        new_pte = pfn_pte(__phys_to_pfn(ghost_pa), __pgprot(ARM64_DBI_GHOST_PTE_FLAGS));
        set_pte(ptep, new_pte);
        installed++;
    }

    flush_tlb_all();
    return 0;

err_clear:
    while (installed > 0)
    {
        u64 ghost_addr = ghost->ghost_page + (u64)(installed - 1) * PAGE_SIZE;
        pte_t *ptep = get_user_pte(mm, ghost_addr);

        if (ptep) set_pte(ptep, __pte(0));
        installed--;
    }
    flush_tlb_all();
    return status;
}

// 清掉目标进程页表里的 ghost PTE；页表页本身不主动释放，交给 mm 生命周期回收。
static inline void arm64_dbi_ghost_clear_ptes(struct mm_struct *mm, u64 ghost_page)
{
    // 只清掉我们安装的 ghost PTE；页表页本身交给进程 mm 生命周期回收。
    int index;
    int page_count = arm64_dbi_ghost_page_count();

    if (!mm || !ghost_page) return;

    mmap_write_lock(mm);
    for (index = 0; index < page_count; index++)
    {
        pte_t *ptep = get_user_pte(mm, ghost_page + (u64)index * PAGE_SIZE);
        if (ptep) set_pte(ptep, __pte(0));
    }
    mmap_write_unlock(mm);
    flush_tlb_all();
}

// 释放一个 ghost 资源对象：恢复目标页、清 ghost 映射、释放内核内存。
static inline void arm64_dbi_ghost_release(pid_t pid, struct arm64_dbi_ghost *ghost)
{
    struct mm_struct *mm;

    if (!ghost) return;

    if (pid > 0 && ghost->armed) arm64_dbi_ghost_restore_target_pte(pid, ghost);

    if (pid > 0 && ghost->ghost_page)
    {
        mm = get_mm_by_pid(pid);
        if (mm)
        {
            arm64_dbi_ghost_clear_ptes(mm, ghost->ghost_page);
            mmput(mm);
        }
    }

    if (ghost->ghost_copy) free_pages((unsigned long)ghost->ghost_copy, arm64_dbi_ghost_order());
    if (ghost->target_copy) vfree(ghost->target_copy);
    __builtin_memset(ghost, 0, sizeof(*ghost));
}

// 内部准备步骤：生成 ghost 资源并映射 ghost VA；不写目标页 UXN。
static inline int arm64_dbi_ghost_prepare_resource(pid_t pid, u64 target_page, const void *machine_code_4k, struct arm64_dbi_ghost *ghost)
{
    /*
    install 调用这里完成资源准备；slot 发布和 UXN 启用由安装入口统一处理。
    machine_code_4k 为 NULL 时，内部按 target_page 从目标进程读取原页快照。
    */
    struct mm_struct *mm;
    int status;
    size_t ghost_size = arm64_dbi_ghost_size();

    if (!ghost || (target_page & ~PAGE_MASK)) return -EINVAL;

    __builtin_memset(ghost, 0, sizeof(*ghost));
    ghost->target_page = target_page;
    // target_copy 永远保存一份 4KB 原始机器码，DBI 只读取这份稳定快照。
    ghost->target_copy = vmalloc(PAGE_SIZE);
    // ghost_copy 需要连续物理页，因为后面按 ghost_pa + index * PAGE_SIZE 安装 PTE。
    ghost->ghost_copy = (void *)__get_free_pages(GFP_KERNEL | __GFP_ZERO, arm64_dbi_ghost_order());
    if (!ghost->target_copy || !ghost->ghost_copy)
    {
        status = -ENOMEM;
        goto err_out;
    }
    ghost->ghost_pa = virt_to_phys(ghost->ghost_copy);

    if (machine_code_4k)
    {
        // 外部传入机器码时不再读取目标进程内容，方便其它模块复用这个通用层。
        __builtin_memcpy(ghost->target_copy, machine_code_4k, PAGE_SIZE);
    }
    else
    {
        status = arm64_dbi_ghost_read_target_page(pid, target_page, ghost->target_copy);
        if (status < 0) goto err_out;
    }

    // 记录原始 PTE，后续启用 UXN 和 release 恢复都依赖它。
    status = read_user_pte_value_by_pid(pid, target_page, &ghost->saved_pte);
    if (status) goto err_out;

    mm = get_mm_by_pid(pid);
    if (!mm)
    {
        status = -ESRCH;
        goto err_out;
    }

    // 先找 ghost 用户 VA，但暂时不改目标页 UXN。
    mmap_write_lock(mm);
    ghost->ghost_page = arm64_dbi_ghost_find_hole_locked(mm, target_page, ghost_size);
    mmap_write_unlock(mm);
    if (!ghost->ghost_page)
    {
        ls_log_tag("ptebp", "no ghost VA hole pid=%d target=0x%llx size=0x%zx task_size=0x%llx\n", pid, target_page, ghost_size, (u64)mm->task_size);
        mmput(mm);
        status = -ENOSPC;
        goto err_out;
    }
    mmput(mm);

    // 根据 target_page 和 ghost_page 重写整页指令，并填充 target PC -> ghost PC 映射表。
    ghost->dbi.target_page = target_page;
    ghost->dbi.ghost_page = ghost->ghost_page;
    ghost->dbi.orig = ghost->target_copy;
    ghost->dbi.ghost = ghost->ghost_copy;
    ghost->dbi.ghost_capacity = ARM64_DBI_GHOST_MAX_INSNS;
    status = arm64_dbi_recompile_page(&ghost->dbi);
    if (status) ls_log_tag("ptebp", "DBI recompile failed pid=%d target=0x%llx ghost=0x%llx status=%d count=%d pending=%d failed=%d\n", pid, target_page, ghost->ghost_page, status, ghost->dbi.ghost_count, ghost->dbi.n_pending, ghost->dbi.failed);
    if (status) goto err_out;

    mm = get_mm_by_pid(pid);
    if (!mm)
    {
        status = -ESRCH;
        goto err_out;
    }
    // ghost 页没有 VMA，这里直接补齐页表层级并写 PTE。
    mmap_write_lock(mm);
    status = arm64_dbi_ghost_install_ptes(mm, ghost);
    mmap_write_unlock(mm);
    mmput(mm);
    if (status) goto err_out;

    arm64_dbi_ghost_sync_icache(ghost->ghost_copy, ghost_size);
    return 0;

err_out:
    arm64_dbi_ghost_release(pid, ghost);
    return status;
}

// 以下接口让 ghost 层自己维护 pid + target_page -> ghost 的全局槽位。
static inline struct arm64_dbi_ghost_slot *arm64_dbi_ghost_find_slot_locked(pid_t pid, u64 target_page)
{
    int slot_index;

    for (slot_index = 0; slot_index < ARM64_DBI_GHOST_SLOT_COUNT; slot_index++)
    {
        struct arm64_dbi_ghost_slot *slot = &g_arm64_dbi_ghost_slots[slot_index];

        if (slot->used && slot->pid == pid && slot->ghost.target_page == target_page) return slot;
    }

    return NULL;
}

static inline struct arm64_dbi_ghost_slot *arm64_dbi_ghost_alloc_slot_locked(void)
{
    int slot_index;

    for (slot_index = 0; slot_index < ARM64_DBI_GHOST_SLOT_COUNT; slot_index++)
    {
        if (!g_arm64_dbi_ghost_slots[slot_index].used) return &g_arm64_dbi_ghost_slots[slot_index];
    }

    return NULL;
}

static inline void arm64_dbi_ghost_release_slot_copy(pid_t pid, struct arm64_dbi_ghost_slot *slot)
{
    if (!slot || !slot->used) return;

    arm64_dbi_ghost_release(pid, &slot->ghost);
    __builtin_memset(slot, 0, sizeof(*slot));
}

// 一键安装托管 ghost：准备资源、发布 slot、启用 UXN，一次完成。
static inline int arm64_dbi_ghost_install(pid_t pid, u64 target_page, const void *machine_code_4k, u64 *out_ghost_page)
{
    struct arm64_dbi_ghost *prepared;
    struct arm64_dbi_ghost_slot *slot;
    unsigned long flags;
    u64 ghost_page = 0;
    pteval_t armed_pte;
    int status = 0;

    if (pid <= 0 || (target_page & ~PAGE_MASK)) return -EINVAL;

    prepared = kzalloc(sizeof(*prepared), GFP_KERNEL);
    if (!prepared) return -ENOMEM;

    mutex_lock(&g_arm64_dbi_ghost_mutex);

    spin_lock_irqsave(&g_arm64_dbi_ghost_lock, flags);
    slot = arm64_dbi_ghost_find_slot_locked(pid, target_page);
    if (slot)
    {
        ghost_page = slot->ghost.ghost_page;
        spin_unlock_irqrestore(&g_arm64_dbi_ghost_lock, flags);
        goto out_success;
    }
    spin_unlock_irqrestore(&g_arm64_dbi_ghost_lock, flags);

    status = arm64_dbi_ghost_prepare_resource(pid, target_page, machine_code_4k, prepared);
    if (status) goto out_unlock;

    spin_lock_irqsave(&g_arm64_dbi_ghost_lock, flags);
    slot = arm64_dbi_ghost_alloc_slot_locked();
    if (!slot)
    {
        spin_unlock_irqrestore(&g_arm64_dbi_ghost_lock, flags);
        arm64_dbi_ghost_release(pid, prepared);
        ls_log_tag("ptebp", "ghost slot exhausted pid=%d target=0x%llx slots=%d\n", pid, target_page, ARM64_DBI_GHOST_SLOT_COUNT);
        status = -ENOSPC;
        goto out_unlock;
    }

    slot->used = true;
    slot->pid = pid;
    slot->ghost = *prepared;
    /*
            这里先把 slot 和 armed 状态发布给 fault handler，再写目标页 UXN。
                如果 UXN 写入后第一条异常立刻进来，异常入口只需要查询托管表。
    */
    slot->ghost.armed = true;
    ghost_page = slot->ghost.ghost_page;
    __builtin_memset(prepared, 0, sizeof(*prepared));
    spin_unlock_irqrestore(&g_arm64_dbi_ghost_lock, flags);

    armed_pte = arm64_dbi_ghost_build_armed_pte(slot->ghost.saved_pte);
    if (armed_pte != slot->ghost.saved_pte)
    {
        status = write_user_pte_value_by_pid(pid, slot->ghost.target_page, armed_pte);
        if (!status) flush_tlb_all();
    }
    if (status)
    {
        __builtin_memset(&g_arm64_dbi_ghost_release_slots[0], 0, sizeof(g_arm64_dbi_ghost_release_slots[0]));
        spin_lock_irqsave(&g_arm64_dbi_ghost_lock, flags);
        g_arm64_dbi_ghost_release_slots[0] = *slot;
        __builtin_memset(slot, 0, sizeof(*slot));
        spin_unlock_irqrestore(&g_arm64_dbi_ghost_lock, flags);
        arm64_dbi_ghost_release_slot_copy(pid, &g_arm64_dbi_ghost_release_slots[0]);
        goto out_unlock;
    }

out_success:
    if (out_ghost_page) *out_ghost_page = ghost_page;

out_unlock:
    mutex_unlock(&g_arm64_dbi_ghost_mutex);
    kfree(prepared);
    return status;
}

// fault handler 使用：在托管表里查找已 armed 的 ghost，并返回 target PC 对应的 ghost PC。
static inline bool arm64_dbi_ghost_lookup_pc(pid_t pid, u64 target_page, u64 target_pc, u64 *out_ghost_pc)
{
    struct arm64_dbi_ghost_slot *slot;
    unsigned long flags;
    u64 ghost_pc = 0;
    bool found = false;

    if (out_ghost_pc) *out_ghost_pc = 0;
    if (pid <= 0 || (target_page & ~PAGE_MASK)) return false;

    spin_lock_irqsave(&g_arm64_dbi_ghost_lock, flags);
    slot = arm64_dbi_ghost_find_slot_locked(pid, target_page);
    if (slot && slot->ghost.armed)
    {
        ghost_pc = arm64_dbi_target_to_ghost_pc(&slot->ghost.dbi, target_pc);
        found = true;
    }
    spin_unlock_irqrestore(&g_arm64_dbi_ghost_lock, flags);

    if (out_ghost_pc) *out_ghost_pc = ghost_pc;
    return found;
}

// 按 pid 移除所有托管 ghost，供上层在没有逐页列表时兜底清理。
static inline void arm64_dbi_ghost_remove_all(pid_t pid)
{
    unsigned long flags;
    int slot_index;
    int release_count = 0;

    if (pid <= 0) return;

    mutex_lock(&g_arm64_dbi_ghost_mutex);
    __builtin_memset(g_arm64_dbi_ghost_release_slots, 0, sizeof(g_arm64_dbi_ghost_release_slots));

    spin_lock_irqsave(&g_arm64_dbi_ghost_lock, flags);
    for (slot_index = 0; slot_index < ARM64_DBI_GHOST_SLOT_COUNT; slot_index++)
    {
        struct arm64_dbi_ghost_slot *slot = &g_arm64_dbi_ghost_slots[slot_index];

        if (!slot->used || slot->pid != pid) continue;

        g_arm64_dbi_ghost_release_slots[release_count++] = *slot;
        __builtin_memset(slot, 0, sizeof(*slot));
    }
    spin_unlock_irqrestore(&g_arm64_dbi_ghost_lock, flags);

    for (slot_index = 0; slot_index < release_count; slot_index++) arm64_dbi_ghost_release_slot_copy(pid, &g_arm64_dbi_ghost_release_slots[slot_index]);

    mutex_unlock(&g_arm64_dbi_ghost_mutex);
}

#endif
