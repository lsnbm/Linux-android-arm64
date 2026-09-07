#include <linux/mm.h>
#include <asm/uaccess.h>
#include "emulate_inst.h"
#include "../lsdriver_log.h"

#define ARM64_EXECUTOR_CACHE_BITS     13U
#define ARM64_EXECUTOR_CACHE_SIZE     (1U << ARM64_EXECUTOR_CACHE_BITS)
#define ARM64_EXECUTOR_CACHE_WAYS     16U
#define ARM64_EXECUTOR_CACHE_WAY_BITS 4U
#define ARM64_EXECUTOR_CACHE_BUCKETS  (ARM64_EXECUTOR_CACHE_SIZE / ARM64_EXECUTOR_CACHE_WAYS)

#define ARM64_EXECUTOR_TAG_EMPTY 0x00000000U
#define ARM64_EXECUTOR_TAG_BUSY  0x00000001U

/*
每个 bucket 保存 16 个 tag 和对应的 payload。payload 发布后不可变，命中路径
直接返回条目指针，不复制执行缓存条目。
*/
struct arm64_executor_cache_bucket
{
    uint32_t tags[ARM64_EXECUTOR_CACHE_WAYS];
    struct arm64_executor_entry payloads[ARM64_EXECUTOR_CACHE_WAYS];
};

/*
缓存查找和插入不关闭中断或禁止抢占。tag 使用原子操作同步，payload 通过
release/acquire 协议发布，可安全处理并发和中断重入。
*/
static struct arm64_executor_cache_bucket g_arm64_executor_cache[ARM64_EXECUTOR_CACHE_BUCKETS];

static inline uint32_t arm64_executor_cache_hash(uint32_t raw)
{
    return ((raw ^ (raw >> 16)) * 0x9E3779B1U) >> (32U - (ARM64_EXECUTOR_CACHE_BITS - ARM64_EXECUTOR_CACHE_WAY_BITS));
}

/* 每轮并行读取和比较 4 个 tag；命中后通过 acquire fence 读取不可变 payload。 */
static inline const struct arm64_executor_entry *arm64_executor_cache_lookup(uint32_t raw)
{
    const struct arm64_executor_cache_bucket *bucket = &g_arm64_executor_cache[arm64_executor_cache_hash(raw)];
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
    if (__builtin_expect(__atomic_load_n(&bucket->tags[way], __ATOMIC_ACQUIRE) != raw, 0)) return NULL;
    return &bucket->payloads[way];
}

/* CAS 抢占空槽，先写完整 payload，再用 release store 发布机器码 tag。 */
static inline void arm64_executor_cache_insert(uint32_t raw, const struct arm64_executor_entry *entry)
{
    uint32_t bucket_index;
    struct arm64_executor_cache_bucket *bucket;
    int empty_way = -1;
    uint32_t way;

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

    uint32_t expected = ARM64_EXECUTOR_TAG_EMPTY;
    if (!__atomic_compare_exchange_n(&bucket->tags[empty_way], &expected, ARM64_EXECUTOR_TAG_BUSY, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return;

    bucket->payloads[empty_way] = *entry;
    __atomic_store_n(&bucket->tags[empty_way], raw, __ATOMIC_RELEASE);
}

/* ======================== 已解码指令：构建不可变执行器条目 ======================== */

bool emu_build_executor_entry(const struct arm64_decoded_instruction *decoded, struct arm64_executor_entry *entry)
{
    __builtin_memset(entry, 0, sizeof(*entry));
    entry->decoded = *decoded;

    switch (decoded->instruction_class)
    {
    case ARM64_INSTRUCTION_CLASS_LOAD_STORE:
        entry->execute = emu_build_ldst_executor(decoded);
        break;
    case ARM64_INSTRUCTION_CLASS_DATA_PROCESSING_REGISTER:
        entry->execute = emu_build_register_executor(decoded);
        break;
    case ARM64_INSTRUCTION_CLASS_DATA_PROCESSING_SIMD_FP:
        entry->execute = emu_build_simd_executor(decoded);
        break;
    case ARM64_INSTRUCTION_CLASS_DATA_PROCESSING_IMMEDIATE:
        entry->execute = emu_build_immediate_executor(decoded);
        break;
    case ARM64_INSTRUCTION_CLASS_BRANCH_EXCEPTION_SYSTEM:
        entry->execute = emu_build_branch_executor(decoded);
        break;
    default:
        return false;
    }

    return entry->execute != NULL;
}

__nocfi enum emu_inst_result emu_execute_executor_entry(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    const struct arm64_decoded_instruction *decoded;
    uint32_t initial_fpcr;
    uint32_t initial_fpsr;
    enum emu_inst_result result;

    if (!entry || !entry->execute) return EMU_INST_SKIP;
    decoded = &entry->decoded;
    //如果当前指令不需要 FPCR/FPSR 状态同步，就直接执行并返回。
    if (decoded->instruction_class != ARM64_INSTRUCTION_CLASS_DATA_PROCESSING_SIMD_FP && (decoded->instruction_class != ARM64_INSTRUCTION_CLASS_BRANCH_EXCEPTION_SYSTEM || (decoded->instruction != ARM64_INST_MSR_REGISTER && decoded->instruction != ARM64_INST_MRS) || (decoded->sysreg != ARM64_SYSREG_KEY(3, 3, 4, 4, 0) && decoded->sysreg != ARM64_SYSREG_KEY(3, 3, 4, 4, 1)))) return entry->execute(regs, fp_regs, entry);

    initial_fpcr = fp_regs->fpcr;
    initial_fpsr = fp_regs->fpsr;

    /*
    FP/SIMD 模板不会显式执行 MRS/MSR，但其指令会隐式读取 FPCR，
    并可能隐式更新 FPSR。执行前先装入软件现场，避免沿用上一次
    模板执行遗留的硬件状态，并保证本次指令使用调用者保存的状态。
    */
    write_fpcr(initial_fpcr);
    write_fpsr(initial_fpsr);

    result = entry->execute(regs, fp_regs, entry);

    /*
    执行期间可能产生 FPSR 异常/QC 标志，MSR FPCR/FPSR 也可能改变
    硬件寄存器。只有值发生变化时才需要写回；之后始终读回硬件，
    以捕获隐式副作用以及硬件清除保留位后的最终值。
    */
    if (fp_regs->fpcr != initial_fpcr) write_fpcr(fp_regs->fpcr);
    fp_regs->fpcr = read_fpcr();

    if (fp_regs->fpsr != initial_fpsr) write_fpsr(fp_regs->fpsr);
    fp_regs->fpsr = read_fpsr();

    return result;
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

bool emulate_inst(struct pt_regs *regs, struct fp_regs *fp_regs, uint32_t specified_inst)
{
    struct arm64_decoded_instruction decoded __attribute__((__uninitialized__));
    struct arm64_executor_entry local_entry __attribute__((__uninitialized__));
    const struct arm64_executor_entry *entry;
    uint64_t pc = regs->pc;
    uint32_t inst = specified_inst;
    enum emu_inst_result result = EMU_INST_SKIP;

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
    if (!inst)
    {
        inst = READ_ONCE(*(const uint32_t *)(uintptr_t)regs->pc);
    }

    entry = arm64_executor_cache_lookup(inst);
    if (entry)
    {
        result = emu_execute_executor_entry(regs, fp_regs, entry);
    }
    else if (arm64_decode_instruction(inst, &decoded) == ARM64_DECODE_OK && emu_build_executor_entry(&decoded, &local_entry))
    {
        result = emu_execute_executor_entry(regs, fp_regs, &local_entry);
        if (result == EMU_INST_HANDLED) arm64_executor_cache_insert(inst, &local_entry);
    }

    uaccess_disable_privileged();

    if (unlikely(result != EMU_INST_HANDLED))
    {
        ls_log_always_tag("emulate_inst", "failed pc=0x%llx inst=0x%08x bytes=%02x %02x %02x %02x\n", (unsigned long long)pc, inst, inst & 0xff, (inst >> 8) & 0xff, (inst >> 16) & 0xff, inst >> 24);
    }

    return result == EMU_INST_HANDLED;
}
