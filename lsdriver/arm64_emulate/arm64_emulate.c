#include <linux/mm.h>
#include <asm/uaccess.h>
#include "arm64_emulate_internal.h"
#include "../lsdriver_log.h"

enum
{
    ARM64_EXECUTOR_CACHE_BITS = 16,
    ARM64_EXECUTOR_CACHE_SIZE = 1U << ARM64_EXECUTOR_CACHE_BITS,
    ARM64_EXECUTOR_CACHE_WAYS = 16,
    ARM64_EXECUTOR_CACHE_WAY_BITS = 4,
    ARM64_EXECUTOR_CACHE_BUCKETS = ARM64_EXECUTOR_CACHE_SIZE / ARM64_EXECUTOR_CACHE_WAYS,
    ARM64_EXECUTOR_TAG_EMPTY = 0,
    ARM64_EXECUTOR_TAG_BUSY = 1,
};

/*
16 个 tag 对齐到单个 64 字节 cache line。payload 发布后不可变，命中路径直接
返回条目指针，不复制 32 字节 payload。
*/
struct arm64_executor_cache_bucket
{
    uint32_t tags[ARM64_EXECUTOR_CACHE_WAYS] __attribute__((aligned(64)));
    struct arm64_executor_entry payloads[ARM64_EXECUTOR_CACHE_WAYS];
};

/*
缓存查找和插入不关闭中断或禁止抢占。tag 使用原子操作同步，payload 通过
release/acquire 协议发布，可安全处理并发和中断重入。
*/
static struct arm64_executor_cache_bucket g_arm64_executor_cache[ARM64_EXECUTOR_CACHE_BUCKETS];

static inline uint32_t arm64_executor_cache_hash(uint32_t raw)
{
    uint32_t hash = raw ^ (raw >> 16);

    return (hash * 0x9E3779B1U) >> (32U - (ARM64_EXECUTOR_CACHE_BITS - ARM64_EXECUTOR_CACHE_WAY_BITS));
}

/* 每轮并行读取和比较 4 个 tag；命中后通过 acquire fence 读取不可变 payload。 */
static const struct arm64_executor_entry *arm64_executor_cache_lookup(uint32_t raw)
{
    uint32_t bucket_index = arm64_executor_cache_hash(raw);
    const struct arm64_executor_cache_bucket *bucket = &g_arm64_executor_cache[bucket_index];
    uint32_t way;

    if (__builtin_expect(raw == ARM64_EXECUTOR_TAG_EMPTY || raw == ARM64_EXECUTOR_TAG_BUSY, 0)) return NULL;

    for (way = 0; way < ARM64_EXECUTOR_CACHE_WAYS; way += 4)
    {
        uint32_t tag0 = __atomic_load_n(&bucket->tags[way + 0], __ATOMIC_RELAXED);
        uint32_t tag1 = __atomic_load_n(&bucket->tags[way + 1], __ATOMIC_RELAXED);
        uint32_t tag2 = __atomic_load_n(&bucket->tags[way + 2], __ATOMIC_RELAXED);
        uint32_t tag3 = __atomic_load_n(&bucket->tags[way + 3], __ATOMIC_RELAXED);

        if (__builtin_expect(tag0 == raw, 0)) goto hit;
        if (__builtin_expect(tag1 == raw, 0))
        {
            way += 1;
            goto hit;
        }
        if (__builtin_expect(tag2 == raw, 0))
        {
            way += 2;
            goto hit;
        }
        if (__builtin_expect(tag3 == raw, 0))
        {
            way += 3;
            goto hit;
        }
        if (tag0 == ARM64_EXECUTOR_TAG_EMPTY || tag1 == ARM64_EXECUTOR_TAG_EMPTY || tag2 == ARM64_EXECUTOR_TAG_EMPTY || tag3 == ARM64_EXECUTOR_TAG_EMPTY) return NULL;
    }
    return NULL;

hit:
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    return &bucket->payloads[way];
}

/* CAS 抢占空槽，先写完整 payload，再用 release store 发布机器码 tag。 */
static void arm64_executor_cache_insert(uint32_t raw, const struct arm64_executor_entry *entry)
{
    uint32_t bucket_index;
    struct arm64_executor_cache_bucket *bucket;
    int empty_way = -1;
    uint32_t way;
    uint32_t expected;

    if (__builtin_expect(raw == ARM64_EXECUTOR_TAG_EMPTY || raw == ARM64_EXECUTOR_TAG_BUSY, 0)) return;

    bucket_index = arm64_executor_cache_hash(raw);
    bucket = &g_arm64_executor_cache[bucket_index];
    for (way = 0; way < ARM64_EXECUTOR_CACHE_WAYS; way++)
    {
        uint32_t tag = __atomic_load_n(&bucket->tags[way], __ATOMIC_RELAXED);

        if (tag == raw) return;
        if (tag == ARM64_EXECUTOR_TAG_EMPTY && empty_way < 0) empty_way = way;
    }
    if (empty_way < 0) return;

    expected = ARM64_EXECUTOR_TAG_EMPTY;
    if (!__atomic_compare_exchange_n(&bucket->tags[empty_way], &expected, ARM64_EXECUTOR_TAG_BUSY, false, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return;

    bucket->payloads[empty_way] = *entry;
    __atomic_store_n(&bucket->tags[empty_way], raw, __ATOMIC_RELEASE);
}

/* ======================== 已解码指令：构建不可变执行器条目 ======================== */

bool emu_build_executor_entry(const struct arm64_decoded_insn *decoded, struct arm64_executor_entry *entry)
{
    __builtin_memset(entry, 0, sizeof(*entry));

    switch (decoded->insn_class)
    {
    case ARM64_INSN_CLASS_LOAD_STORE:
        return emu_build_ldst_executor(decoded, entry);
    case ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER:
        return emu_build_register_executor(decoded, entry);
    case ARM64_INSN_CLASS_DATA_PROCESSING_SIMD_FP:
        return emu_build_simd_executor(decoded, entry);
    case ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE:
        return emu_build_immediate_executor(decoded, entry);
    case ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM:
        return emu_build_branch_executor(decoded, entry);
    default:
        return false;
    }
}

__nocfi enum emu_insn_result emu_execute_executor_entry(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    if (!entry || !entry->execute) return EMU_INSN_SKIP;
    return entry->execute(regs, fp_regs, entry);
}

/*
访存类指令使用模板汇编让硬件真实同语义需要注意一个问题：
COW:当前进程准备写入一个仍与其他进程或映射共享的物理页，而该虚拟内存区域在逻辑上属于私有可写。Linux 为避免提前复制页面，先让这些映射共享同一物理页，并将相关 PTE 设置为只读。首次写入触发权限异常后，内核为当前进程建立私有副本，将其 PTE 改为可写，然后重新执行写入指令。
这里执行访存类指令写的时候目标地址页如果刚好处于COW中就会之间panic

还需要注意的:
内核代码中几乎根本不会去写Advanced SIMD/FP类的代码,也不建议你去，所以使用任何clang版本都没有问题
不影响cpu支持这些扩展指令集，然后用户态的新clang可以编译出Advanced SIMD/FP汇编运行
这里为了模拟使用.inst直接写机器码去让cpu执行，不然编译内核的旧clang根本识别不出这些新扩展的助记符
*/

/* ======================== 总入口：执行器缓存、解码、构建与执行 ======================== */

// clang-format off
bool emulate_insn(struct pt_regs *regs, struct fp_regs *fp_regs, uint32_t specified_insn)
{
    struct arm64_decoded_insn decoded __attribute__((__uninitialized__));
    struct arm64_executor_entry local_entry __attribute__((__uninitialized__));
    const struct arm64_executor_entry *entry;
    uint64_t pc = regs->pc;
    uint32_t insn = specified_insn;
    enum emu_insn_result result = EMU_INSN_SKIP;

    /* 生产调用点来自异常/内核上下文 不建议硬编码
    asm volatile("msr PAN, #0x0" ::: "memory");
    asm volatile("msr PAN, #0x1" ::: "memory");
    安全特性	    硬件支持版本	默认状态        uaccess_enable_privileged 的操作
    MTE	           ARMv8.5+	      开启校验	        mte_disable_tco(); 开启 TCO，忽略校验
    SW PAN	       ARMv8.0	      卸载 TTBR0	   uaccess_ttbr0_disable();重新加载 TTBR0 用户页表
    HW PAN	       ARMv8.1+	      阻止内核访问 EL0	 __uaccess_enable_hw_pan();禁用 HW PAN，允许内核访问 EL0
    uaccess_ttbr0_disable是为了在没有硬件PAN时进行的软件切换基址寄存器，实现PAN,
    支持硬件PAN就不会去走软件PAN,而是快速判断进行返回
    */
    uaccess_enable_privileged();
    if (!insn) insn = (uint32_t)emu_template_ldr_w(pc);

    entry = arm64_executor_cache_lookup(insn);
    if (entry)
    {
        result = emu_execute_executor_entry(regs, fp_regs, entry);
    }
    else if (arm64_decode_insn(insn, &decoded) == ARM64_DECODE_OK && emu_build_executor_entry(&decoded, &local_entry))
    {
        result = emu_execute_executor_entry(regs, fp_regs, &local_entry);
        if (result == EMU_INSN_HANDLED) arm64_executor_cache_insert(insn, &local_entry);
    }

    uaccess_disable_privileged();

    if (unlikely(result != EMU_INSN_HANDLED))
    {
        ls_log_always_tag("emulate_insn", "failed pc=0x%llx insn=0x%08x bytes=%02x %02x %02x %02x\n", (unsigned long long)pc, insn, insn & 0xff, (insn >> 8) & 0xff, (insn >> 16) & 0xff, insn >> 24);
    }

    return result == EMU_INSN_HANDLED;
}
// clang-format on
