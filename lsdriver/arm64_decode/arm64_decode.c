#include "arm64_decode.h"

#define ARM64_DECODE_CACHE_BITS     16U
#define ARM64_DECODE_CACHE_SIZE     (1U << ARM64_DECODE_CACHE_BITS)
#define ARM64_DECODE_CACHE_WAYS     16U
#define ARM64_DECODE_CACHE_WAY_BITS 4U
#define ARM64_DECODE_CACHE_BUCKETS  (ARM64_DECODE_CACHE_SIZE / ARM64_DECODE_CACHE_WAYS)

// 哨兵状态值
#define TAG_EMPTY 0x00000000U
#define TAG_BUSY  0x00000001U

/*
强制 64 字节对齐，让 16 个 32-bit tag 恰好占据单个 L1 D-Cache Line。
 */
struct arm64_decode_cache_bucket
{
    uint32_t tags[ARM64_DECODE_CACHE_WAYS] __attribute__((aligned(64)));
    struct arm64_decoded_insn payloads[ARM64_DECODE_CACHE_WAYS];
};

/*
这个解码器搭起来后的扩展查缓存
缓存查找和插入都不会关闭中断或禁止抢占。tag 使用原子操作同步，
payload 通过 release/acquire 协议发布，可安全处理并发和中断重入。
*/
static struct arm64_decode_cache_bucket g_arm64_decode_cache[ARM64_DECODE_CACHE_BUCKETS];

// 使用乘法结果的高位选择 bucket，避免低位编码模式形成固定冲突。
static inline uint32_t arm64_decode_cache_hash(uint32_t raw)
{
    uint32_t h = raw ^ (raw >> 16);
    return (h * 0x9E3779B1U) >> (32U - (ARM64_DECODE_CACHE_BITS - ARM64_DECODE_CACHE_WAY_BITS));
}

/*
无 NEON 的纯 GPR 查找路径，每轮并行读取和比较 4 个 tag。
 */
static int arm64_decode_cache_lookup(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    uint32_t bucket_idx = arm64_decode_cache_hash(raw);
    const struct arm64_decode_cache_bucket *bucket = &g_arm64_decode_cache[bucket_idx];
    uint32_t way;

    // 0 和 1 是内部状态，不能参与 tag 匹配。
    if (__builtin_expect(raw == TAG_EMPTY || raw == TAG_BUSY, 0)) return 0;

    /*
    4-way 手动展开，使 tag 加载和比较可以在 ARM64 超标量流水线上并行执行。
    */
    for (way = 0; way < ARM64_DECODE_CACHE_WAYS; way += 4)
    {
        // 批量加载同一 cache line 中的 4 个 tag。
        uint32_t t0 = __atomic_load_n(&bucket->tags[way + 0], __ATOMIC_RELAXED);
        uint32_t t1 = __atomic_load_n(&bucket->tags[way + 1], __ATOMIC_RELAXED);
        uint32_t t2 = __atomic_load_n(&bucket->tags[way + 2], __ATOMIC_RELAXED);
        uint32_t t3 = __atomic_load_n(&bucket->tags[way + 3], __ATOMIC_RELAXED);

        if (__builtin_expect(t0 == raw, 0))
        {
            goto hit;
        }
        if (__builtin_expect(t1 == raw, 0))
        {
            way += 1;
            goto hit;
        }
        if (__builtin_expect(t2 == raw, 0))
        {
            way += 2;
            goto hit;
        }
        if (__builtin_expect(t3 == raw, 0))
        {
            way += 3;
            goto hit;
        }

        // 插入总是占用最前面的空槽，遇到空槽即可确定 miss。
        if (t0 == TAG_EMPTY || t1 == TAG_EMPTY || t2 == TAG_EMPTY || t3 == TAG_EMPTY)
        {
            return 0;
        }
    }

    return 0;

hit:
    // 与发布 tag 的 release store 配对，保证 payload 已经完整可见。
    __atomic_thread_fence(__ATOMIC_ACQUIRE);
    *decoded = bucket->payloads[way];
    return 1;
}

/*
使用 CAS 抢占空槽并通过 release store 发布。bucket 满时放弃缓存，
调用方仍会正常返回本次解码结果，后续相同指令继续走常规解码路径。
 */
static void arm64_decode_cache_insert(uint32_t raw, const struct arm64_decoded_insn *decoded)
{
    if (__builtin_expect(raw == TAG_EMPTY || raw == TAG_BUSY, 0)) return;

    uint32_t bucket_idx = arm64_decode_cache_hash(raw);
    struct arm64_decode_cache_bucket *bucket = &g_arm64_decode_cache[bucket_idx];

    int empty_way = -1;

    for (uint32_t way = 0; way < ARM64_DECODE_CACHE_WAYS; way++)
    {
        uint32_t tag = __atomic_load_n(&bucket->tags[way], __ATOMIC_RELAXED);

        if (tag == raw) return;
        if (tag == TAG_EMPTY && empty_way < 0)
        {
            empty_way = (int)way;
        }
    }

    if (empty_way < 0) return;

    // CAS 只负责将空槽标记为 BUSY，不需要读取旧 payload。
    uint32_t expected = TAG_EMPTY;
    if (!__atomic_compare_exchange_n(&bucket->tags[empty_way], &expected, TAG_BUSY, 0, __ATOMIC_RELAXED, __ATOMIC_RELAXED))
    {
        return;
    }

    // 先写 payload，再以 release store 发布真实 tag。
    bucket->payloads[empty_way] = *decoded;
    __atomic_store_n(&bucket->tags[empty_way], raw, __ATOMIC_RELEASE);
}

enum arm64_decode_status arm64_decode_data_processing_immediate(uint32_t raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_data_processing_register(uint32_t raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_ldst(uint32_t raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_branch(uint32_t raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_simd(uint32_t raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_sve(uint32_t raw, struct arm64_decoded_insn *decoded);
enum arm64_decode_status arm64_decode_sme(uint32_t raw, struct arm64_decoded_insn *decoded);

enum arm64_decode_status arm64_decode_insn(uint32_t raw, struct arm64_decoded_insn *decoded)
{
    enum arm64_decode_status status;

    if (arm64_decode_cache_lookup(raw, decoded)) return ARM64_DECODE_OK;

    // 直接写入调用方的最终对象，避免大结构体返回临时槽及其复制。
    __builtin_memset(decoded, 0, sizeof(*decoded));

    // A64 主编码 raw[28:25] 直接确定唯一子解码器。
    switch ((raw >> 25) & 0xF)
    {
    case 0x0:
        status = arm64_decode_sme(raw, decoded);
        break;
    case 0x2:
        status = arm64_decode_sve(raw, decoded);
        break;
    case 0x4:
        status = arm64_decode_ldst(raw, decoded);
        break;
    case 0x5:
        status = arm64_decode_data_processing_register(raw, decoded);
        break;
    case 0x6:
        status = arm64_decode_ldst(raw, decoded);
        break;
    case 0x7:
        status = arm64_decode_simd(raw, decoded);
        break;
    case 0x8:
    case 0x9:
        status = arm64_decode_data_processing_immediate(raw, decoded);
        break;
    case 0xA:
    case 0xB:
        status = arm64_decode_branch(raw, decoded);
        break;
    case 0xC:
        status = arm64_decode_ldst(raw, decoded);
        break;
    case 0xD:
        status = arm64_decode_data_processing_register(raw, decoded);
        break;
    case 0xE:
        status = arm64_decode_ldst(raw, decoded);
        break;
    case 0xF:
        status = arm64_decode_simd(raw, decoded);
        break;
    default:
        status = ARM64_DECODE_UNALLOCATED;
        break;
    }

    if (status == ARM64_DECODE_OK) arm64_decode_cache_insert(raw, decoded);
    return status;
}
