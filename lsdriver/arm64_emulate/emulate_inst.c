#include <linux/mm.h>
#include <asm/uaccess.h>
#include "emulate_inst.h"
#include "../klog/lsdriver_log.h"

#define ARM64_EXECUTOR_CACHE_BITS     13U
#define ARM64_EXECUTOR_CACHE_SIZE     (1U << ARM64_EXECUTOR_CACHE_BITS)
#define ARM64_EXECUTOR_CACHE_WAYS     16U
#define ARM64_EXECUTOR_CACHE_WAY_BITS 4U
#define ARM64_EXECUTOR_CACHE_BUCKETS  (ARM64_EXECUTOR_CACHE_SIZE / ARM64_EXECUTOR_CACHE_WAYS)

#define ARM64_EXECUTOR_TAG_EMPTY 0x00000000U
#define ARM64_EXECUTOR_TAG_BUSY  0x00000001U

/*
标签和执行条目分别连续存放，同一 slot 一一对应，仍是单层缓存。
每 16 个连续 slot 构成一个逻辑 bucket；payload 发布后不可变，命中直接返回条目指针。
缓存查找和插入不关闭中断或禁止抢占。tag 使用原子操作同步，payload 通过
release/acquire 协议发布，可安全处理并发和中断重入。
*/
static uint32_t g_arm64_executor_tags[ARM64_EXECUTOR_CACHE_SIZE] __attribute__((__aligned__(64)));
static struct arm64_executor_entry g_arm64_executor_payloads[ARM64_EXECUTOR_CACHE_SIZE] __attribute__((__aligned__(64)));

static inline uint32_t arm64_executor_cache_hash(uint32_t raw)
{
    return ((raw ^ (raw >> 16)) * 0x9E3779B1U) >> (32U - ARM64_EXECUTOR_CACHE_BITS);
}

/* 插入与查找使用相同的哈希起点；acquire 标签读取与 payload 发布配对。 */
static inline const struct arm64_executor_entry *arm64_executor_cache_lookup(uint32_t raw)
{
    if (__builtin_expect(raw == ARM64_EXECUTOR_TAG_EMPTY || raw == ARM64_EXECUTOR_TAG_BUSY, 0)) return NULL;

    uint32_t hash = arm64_executor_cache_hash(raw);
    uint32_t base = hash & ~(ARM64_EXECUTOR_CACHE_WAYS - 1U);
    uint32_t start = hash & (ARM64_EXECUTOR_CACHE_WAYS - 1U);
    for (uint32_t probe = 0; probe < ARM64_EXECUTOR_CACHE_WAYS; probe++)
    {
        uint32_t way = (start + probe) & (ARM64_EXECUTOR_CACHE_WAYS - 1U);
        uint32_t slot = base + way;
        uint32_t tag = __atomic_load_n(&g_arm64_executor_tags[slot], __ATOMIC_ACQUIRE);
        if (tag == raw) return &g_arm64_executor_payloads[slot];
        if (tag == ARM64_EXECUTOR_TAG_EMPTY) return NULL;
    }
    return NULL;
}

/* CAS 抢占空槽，先写完整 payload，再用 release store 发布机器码 tag。 */
static inline void arm64_executor_cache_insert(uint32_t raw, const struct arm64_executor_entry *entry)
{
    int empty_way = -1;
    uint32_t way;

    if (__builtin_expect(raw == ARM64_EXECUTOR_TAG_EMPTY || raw == ARM64_EXECUTOR_TAG_BUSY, 0)) return;

    uint32_t hash = arm64_executor_cache_hash(raw);
    uint32_t base = hash & ~(ARM64_EXECUTOR_CACHE_WAYS - 1U);
    uint32_t start = hash & (ARM64_EXECUTOR_CACHE_WAYS - 1U);
    for (uint32_t probe = 0; probe < ARM64_EXECUTOR_CACHE_WAYS; probe++)
    {
        way = (start + probe) & (ARM64_EXECUTOR_CACHE_WAYS - 1U);
        uint32_t tag = __atomic_load_n(&g_arm64_executor_tags[base + way], __ATOMIC_RELAXED);

        if (tag == raw) return;
        if (tag == ARM64_EXECUTOR_TAG_EMPTY && empty_way < 0) empty_way = way;
    }
    if (empty_way < 0) return;

    uint32_t expected = ARM64_EXECUTOR_TAG_EMPTY;
    if (!__atomic_compare_exchange_n(&g_arm64_executor_tags[base + empty_way], &expected, ARM64_EXECUTOR_TAG_BUSY, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED)) return;

    g_arm64_executor_payloads[base + empty_way] = *entry;
    __atomic_store_n(&g_arm64_executor_tags[base + empty_way], raw, __ATOMIC_RELEASE);
}

/* ======================== 已解码指令：构建不可变执行器条目 ======================== */

bool emu_build_executor_entry(const struct arm64_decoded_instruction *decoded, struct arm64_executor_entry *entry)
{
    entry->execute = NULL;
    if (decoded != &entry->decoded) entry->decoded = *decoded;

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

__attribute__((no_sanitize("cfi", "kcfi"))) void inline emu_execute_executor_entry(struct pt_regs *regs, struct fp_regs *fp_regs, const struct arm64_executor_entry *entry)
{
    /*
    PSTATE.BTYPE 描述当前落点的分支类型，并在当前指令成功执行后被消费。
    执行模板前先清除入口 BTYPE，使普通指令得到与实体 CPU 一致的执行后
    状态；分支模板仍可按目标落点写入新的 BTYPE。
    */
    regs->pstate &= ~(3ULL << 10);
    /* 执行前统一装载软件现场，避免硬件状态残留影响叶子执行。 */
    write_fpcr(fp_regs->fpcr);
    write_fpsr(fp_regs->fpsr);

    entry->execute(regs, fp_regs, entry);
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

/*
与解码缓存相同，将未命中才需要的 decoded/local_entry 和解码、构建、插入调用隔离到不可内联的慢路径，
避免这些临时对象扩大每次缓存命中的栈帧和寄存器保存范围。
命中仍直接使用已发布的不可变条目；只有执行成功才插入，满桶仍放弃缓存。
*/
static __attribute__((__noinline__)) bool emulate_inst_slow(struct pt_regs *regs, struct fp_regs *fp_regs, uint32_t inst)
{
    struct arm64_executor_entry local_entry __attribute__((__uninitialized__));

    if (arm64_decode_instruction(inst, &local_entry.decoded) != ARM64_DECODE_OK || !emu_build_executor_entry(&local_entry.decoded, &local_entry)) return false;

    emu_execute_executor_entry(regs, fp_regs, &local_entry);
    arm64_executor_cache_insert(inst, &local_entry);
    return true;
}

bool emulate_inst(struct pt_regs *regs, struct fp_regs *fp_regs, uint32_t specified_inst)
{
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
    if (!specified_inst)
    {
        specified_inst = READ_ONCE(*(const uint32_t *)(uintptr_t)regs->pc);
    }

    const struct arm64_executor_entry *entry = arm64_executor_cache_lookup(specified_inst);
    if (entry)
    {
        emu_execute_executor_entry(regs, fp_regs, entry);
    }
    else if (!emulate_inst_slow(regs, fp_regs, specified_inst))
    {
        uaccess_disable_privileged();
        ls_log_always_tag("emulate_inst", "failed pc=0x%llx inst=0x%08x bytes=%02x %02x %02x %02x\n", (unsigned long long)regs->pc, specified_inst, specified_inst & 0xff, (specified_inst >> 8) & 0xff, (specified_inst >> 16) & 0xff, specified_inst >> 24);
        return false;
    }

    uaccess_disable_privileged();
    return true;
}
