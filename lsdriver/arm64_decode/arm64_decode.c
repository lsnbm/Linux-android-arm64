#include "arm64_decode.h"

#define ARM64_DECODE_CACHE_BITS     13U
#define ARM64_DECODE_CACHE_SIZE     (1U << ARM64_DECODE_CACHE_BITS)
#define ARM64_DECODE_CACHE_WAYS     16U
#define ARM64_DECODE_CACHE_WAY_BITS 4U
#define ARM64_DECODE_CACHE_BUCKETS  (ARM64_DECODE_CACHE_SIZE / ARM64_DECODE_CACHE_WAYS)

#define TAG_EMPTY 0x00000000U
#define TAG_BUSY  0x00000001U

/*
标签和解码结果分别连续存放，同一 slot 一一对应，仍是单层缓存。
每 16 个连续 slot 构成一个逻辑 bucket，查询只在该 bucket 内环形探测。
缓存查找和插入都不会关闭中断或禁止抢占。tag 使用原子操作同步，
payload 通过 release/acquire 协议发布，可安全处理并发和中断重入。
*/
static uint32_t g_arm64_decode_tags[ARM64_DECODE_CACHE_SIZE] __attribute__((__aligned__(64)));
static struct arm64_decoded_instruction g_arm64_decode_payloads[ARM64_DECODE_CACHE_SIZE] __attribute__((__aligned__(64)));

// 哈希高位选择 bucket，低位选择桶内起点。
static inline uint32_t arm64_decode_cache_hash(uint32_t raw)
{
    uint32_t h = raw ^ (raw >> 16);
    return (h * 0x9E3779B1U) >> (32U - ARM64_DECODE_CACHE_BITS);
}

/*
哈希同时确定 bucket 和桶内起点；插入与查询按相同环形顺序探测。
payload 发布后不可变且槽位不删除，遇到 EMPTY 即可结束查询。
 */
static inline int arm64_decode_cache_lookup(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    // 0 和 1 是内部状态，不能参与 tag 匹配。
    if (__builtin_expect(raw == TAG_EMPTY || raw == TAG_BUSY, 0)) return 0;

    uint32_t hash = arm64_decode_cache_hash(raw);
    uint32_t base = hash & ~(ARM64_DECODE_CACHE_WAYS - 1U);
    uint32_t start = hash & (ARM64_DECODE_CACHE_WAYS - 1U);
    for (uint32_t probe = 0; probe < ARM64_DECODE_CACHE_WAYS; probe++)
    {
        uint32_t way = (start + probe) & (ARM64_DECODE_CACHE_WAYS - 1U);
        uint32_t slot = base + way;
        uint32_t tag = __atomic_load_n(&g_arm64_decode_tags[slot], __ATOMIC_ACQUIRE);
        if (tag == raw)
        {
            *decoded = g_arm64_decode_payloads[slot];
            return 1;
        }
        if (tag == TAG_EMPTY) return 0;
    }

    return 0;
}

/*
使用 CAS 抢占空槽并通过 release store 发布。bucket 满时放弃缓存，
调用方仍会正常返回本次解码结果，后续相同指令继续走常规解码路径。
 */
static inline void arm64_decode_cache_insert(uint32_t raw, const struct arm64_decoded_instruction *decoded)
{
    if (__builtin_expect(raw == TAG_EMPTY || raw == TAG_BUSY, 0)) return;

    uint32_t hash = arm64_decode_cache_hash(raw);
    uint32_t base = hash & ~(ARM64_DECODE_CACHE_WAYS - 1U);

    int empty_way = -1;
    uint32_t start = hash & (ARM64_DECODE_CACHE_WAYS - 1U);

    for (uint32_t probe = 0; probe < ARM64_DECODE_CACHE_WAYS; probe++)
    {
        uint32_t way = (start + probe) & (ARM64_DECODE_CACHE_WAYS - 1U);
        uint32_t tag = __atomic_load_n(&g_arm64_decode_tags[base + way], __ATOMIC_RELAXED);

        if (tag == raw) return;
        if (tag == TAG_EMPTY && empty_way < 0)
        {
            empty_way = (int)way;
        }
    }

    if (empty_way < 0) return;

    // CAS 只负责将空槽标记为 BUSY，不需要读取旧 payload。
    uint32_t expected = TAG_EMPTY;
    if (!__atomic_compare_exchange_n(&g_arm64_decode_tags[base + empty_way], &expected, TAG_BUSY, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
    {
        return;
    }

    // 先写 payload，再以 release store 发布真实 tag。
    g_arm64_decode_payloads[base + empty_way] = *decoded;
    __atomic_store_n(&g_arm64_decode_tags[base + empty_way], raw, __ATOMIC_RELEASE);
}

enum arm64_decode_status arm64_decode_data_processing_immediate(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_data_processing_register(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_load_store(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_branch_exception_system(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_simd_fp(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_sve(uint32_t raw, struct arm64_decoded_instruction *decoded);
enum arm64_decode_status arm64_decode_sme(uint32_t raw, struct arm64_decoded_instruction *decoded);

/*
将正常解码和插入隔离为不可内联的慢路径，避免 LTO 将其重新并入缓存命中入口。
否则编译器可能在查缓存之前，就为跨子解码器调用存活的参数保存寄存器、建立栈帧，
即使命中并提前返回也要承担保存/恢复开销。拆分后命中只查缓存并复制结果，未命中
可尾调用此函数；实际是否消除栈访问取决于编译器，需检查最终机器码。
__noinline__ 使用保留属性拼写，避免与内核的 noinline 宏冲突；缓存发布协议不变。
实际测试效率提升约10%左右
*/
static __attribute__((__noinline__)) enum arm64_decode_status arm64_decode_instruction_slow(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    enum arm64_decode_status status;

    // 直接写入调用方的最终对象，避免大结构体返回临时槽及其复制。
    __builtin_memset(decoded, 0, sizeof(*decoded));

    // A64 主编码 raw[28:25] 直接确定唯一子解码器。
    switch (ARM64_DECODE_FIELD(raw, 28, 25))
    {
    case 0x0:
        status = arm64_decode_sme(raw, decoded);
        break;
    case 0x2:
        status = arm64_decode_sve(raw, decoded);
        break;
    case 0x4:
        status = arm64_decode_load_store(raw, decoded);
        break;
    case 0x5:
        status = arm64_decode_data_processing_register(raw, decoded);
        break;
    case 0x6:
        status = arm64_decode_load_store(raw, decoded);
        break;
    case 0x7:
        status = arm64_decode_simd_fp(raw, decoded);
        break;
    case 0x8:
    case 0x9:
        status = arm64_decode_data_processing_immediate(raw, decoded);
        break;
    case 0xA:
    case 0xB:
        status = arm64_decode_branch_exception_system(raw, decoded);
        break;
    case 0xC:
        status = arm64_decode_load_store(raw, decoded);
        break;
    case 0xD:
        status = arm64_decode_data_processing_register(raw, decoded);
        break;
    case 0xE:
        status = arm64_decode_load_store(raw, decoded);
        break;
    case 0xF:
        status = arm64_decode_simd_fp(raw, decoded);
        break;
    default:
        status = ARM64_DECODE_UNALLOCATED;
        break;
    }

    if (status == ARM64_DECODE_OK) arm64_decode_cache_insert(raw, decoded);
    return status;
}

enum arm64_decode_status arm64_decode_instruction(uint32_t raw, struct arm64_decoded_instruction *decoded)
{
    if (arm64_decode_cache_lookup(raw, decoded)) return ARM64_DECODE_OK;
    return arm64_decode_instruction_slow(raw, decoded);
}
