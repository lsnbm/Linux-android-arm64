#ifndef ARM64_DECODE_H
#define ARM64_DECODE_H

/*
输入是一条 32 位指令 word，输出是与运行环境无关的结构化语义；
*/

typedef unsigned char arm64_u8;
typedef unsigned short arm64_u16;
typedef unsigned int arm64_u32;
typedef unsigned long long arm64_u64;
typedef signed long long arm64_s64;

#define ARM64_SYSREG_KEY(OP0, OP1, CRN, CRM, OP2) ((((OP0) & 0x3) << 14) | (((OP1) & 0x7) << 11) | (((CRN) & 0xF) << 7) | (((CRM) & 0xF) << 3) | ((OP2) & 0x7))

enum arm64_decode_status
{
    // 指令已完整解码，decoded 中与该 opcode 对应的字段有效。
    ARM64_DECODE_OK,
    // 已识别指令大类，但当前还没有提供完整语义。
    ARM64_DECODE_UNSUPPORTED,
    // 编码已分配，但其行为受架构 constrained-unpredictable 约束。
    ARM64_DECODE_UNPREDICTABLE,
    // 编码属于保留/非法组合，或没有任何子解码器能够识别。
    ARM64_DECODE_UNALLOCATED,
    // 调用参数无效。
    ARM64_DECODE_INVALID_ARGUMENT,
    // 仅供子解码器内部调度：该指令不属于当前解码器。
    ARM64_DECODE_NO_MATCH,
};

// 对应 Arm A64 main encoding table 的顶层指令分类。
enum arm64_insn_class
{
    ARM64_INSN_CLASS_UNKNOWN,
    ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE,
    ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER,
    ARM64_INSN_CLASS_LOAD_STORE,
    ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM,
    ARM64_INSN_CLASS_DATA_PROCESSING_SIMD_FP,
    ARM64_INSN_CLASS_SVE,
    ARM64_INSN_CLASS_SME,
};

// 对 opcode/flags 的进一步语义归纳，供执行器、重定位器和审计器直接查询。
#define ARM64_EFFECT_READ_GPR            (1ULL << 0)
#define ARM64_EFFECT_WRITE_GPR           (1ULL << 1)
#define ARM64_EFFECT_READ_FP_SIMD        (1ULL << 2)
#define ARM64_EFFECT_WRITE_FP_SIMD       (1ULL << 3)
#define ARM64_EFFECT_READ_MEMORY         (1ULL << 4)
#define ARM64_EFFECT_WRITE_MEMORY        (1ULL << 5)
#define ARM64_EFFECT_READ_FLAGS          (1ULL << 6)
#define ARM64_EFFECT_WRITE_FLAGS         (1ULL << 7)
#define ARM64_EFFECT_CONTROL_FLOW        (1ULL << 8)
#define ARM64_EFFECT_CONDITIONAL         (1ULL << 9)
#define ARM64_EFFECT_DIRECT_TARGET       (1ULL << 10)
#define ARM64_EFFECT_INDIRECT_TARGET     (1ULL << 11)
#define ARM64_EFFECT_CALL                (1ULL << 12)
#define ARM64_EFFECT_RETURN              (1ULL << 13)
#define ARM64_EFFECT_PC_RELATIVE         (1ULL << 14)
#define ARM64_EFFECT_SYSTEM              (1ULL << 15)
#define ARM64_EFFECT_EXCEPTION           (1ULL << 16)
#define ARM64_EFFECT_BARRIER             (1ULL << 17)
#define ARM64_EFFECT_ATOMIC              (1ULL << 18)
#define ARM64_EFFECT_EXCLUSIVE           (1ULL << 19)
#define ARM64_EFFECT_PREFETCH            (1ULL << 20)
#define ARM64_EFFECT_WRITEBACK           (1ULL << 21)
#define ARM64_EFFECT_RELOCATION_REQUIRED (1ULL << 22)

/*
指令编码族/处理路径。opcode 描述指令采用哪种格式，具体运算由 operation
或 operands.simd.operation 表示，例如 ADD/SUB 共用 ADD_SUB 编码族。
*/
enum arm64_opcode
{
    ARM64_OP_UNKNOWN,

    // Branch, exception generation and system instructions.
    ARM64_OP_NOP,
    ARM64_OP_HINT,
    ARM64_OP_BARRIER,
    ARM64_OP_EXCEPTION_GENERATION,
    ARM64_OP_EXCEPTION_RETURN,
    ARM64_OP_MRS,
    ARM64_OP_MSR_REGISTER,
    ARM64_OP_B,
    ARM64_OP_BL,
    ARM64_OP_BR,
    ARM64_OP_BLR,
    ARM64_OP_RET,
    ARM64_OP_B_COND,
    ARM64_OP_CBZ,
    ARM64_OP_CBNZ,
    ARM64_OP_TBZ,
    ARM64_OP_TBNZ,

    // Data processing -- immediate.
    ARM64_OP_ADR,
    ARM64_OP_ADRP,
    ARM64_OP_ADD_SUB_IMMEDIATE,
    ARM64_OP_MIN_MAX_IMMEDIATE,
    ARM64_OP_LOGICAL_IMMEDIATE,
    ARM64_OP_BITFIELD,
    ARM64_OP_EXTRACT,
    ARM64_OP_MOVE_WIDE,
    ARM64_OP_ADD_SUB_SHIFTED,
    ARM64_OP_ADD_SUB_EXTENDED,
    ARM64_OP_LOGICAL_SHIFTED,
    ARM64_OP_CONDITIONAL_SELECT,
    ARM64_OP_DATA_PROCESSING_2_SOURCE,
    ARM64_OP_MULTIPLY_ADD,
    ARM64_OP_MULTIPLY_HIGH,
    ARM64_OP_ADD_SUB_CARRY,
    ARM64_OP_CONDITIONAL_COMPARE,
    ARM64_OP_DATA_PROCESSING_1_SOURCE,

    ARM64_OP_ATOMIC_RMW,
    ARM64_OP_CAS,
    ARM64_OP_CASP,
    ARM64_OP_EXCLUSIVE,
    ARM64_OP_RCPC_UNSCALED,
    ARM64_OP_LDAPR,
    ARM64_OP_PREFETCH,
    ARM64_OP_LOAD_LITERAL,
    ARM64_OP_PREFETCH_LITERAL,
    ARM64_OP_LOAD_STORE_PAIR,
    ARM64_OP_LOAD_STORE_SINGLE,

    ARM64_OP_FP_SIMD,
    ARM64_OP_SVE,
    ARM64_OP_SME,
};

// 常见系统、hint、屏障和异常指令的具体语义。
enum arm64_system_operation
{
    ARM64_SYSTEM_OP_NONE,
    ARM64_SYSTEM_OP_YIELD,
    ARM64_SYSTEM_OP_WFE,
    ARM64_SYSTEM_OP_WFI,
    ARM64_SYSTEM_OP_SEV,
    ARM64_SYSTEM_OP_SEVL,
    ARM64_SYSTEM_OP_CLREX,
    ARM64_SYSTEM_OP_DSB,
    ARM64_SYSTEM_OP_DMB,
    ARM64_SYSTEM_OP_ISB,
    ARM64_SYSTEM_OP_SVC,
    ARM64_SYSTEM_OP_HVC,
    ARM64_SYSTEM_OP_SMC,
    ARM64_SYSTEM_OP_BRK,
    ARM64_SYSTEM_OP_HLT,
    ARM64_SYSTEM_OP_ERET,
    ARM64_SYSTEM_OP_DRPS,
};

// 整数、位运算和原子指令的具体语义操作。
enum arm64_operation
{
    ARM64_OPERATION_NONE,
    ARM64_OPERATION_ADD,
    ARM64_OPERATION_SUB,
    ARM64_OPERATION_AND,
    ARM64_OPERATION_ORR,
    ARM64_OPERATION_EOR,
    ARM64_OPERATION_ANDS,
    ARM64_OPERATION_SBFM,
    ARM64_OPERATION_BFM,
    ARM64_OPERATION_UBFM,
    ARM64_OPERATION_EXTR,
    ARM64_OPERATION_MOVN,
    ARM64_OPERATION_MOVZ,
    ARM64_OPERATION_MOVK,
    ARM64_OPERATION_SMAX,
    ARM64_OPERATION_SMIN,
    ARM64_OPERATION_UMAX,
    ARM64_OPERATION_UMIN,
    ARM64_OPERATION_CSEL,
    ARM64_OPERATION_CSINC,
    ARM64_OPERATION_CSINV,
    ARM64_OPERATION_CSNEG,
    ARM64_OPERATION_ADC,
    ARM64_OPERATION_ADCS,
    ARM64_OPERATION_SBC,
    ARM64_OPERATION_SBCS,
    ARM64_OPERATION_CCMP,
    ARM64_OPERATION_CCMN,
    ARM64_OPERATION_UDIV,
    ARM64_OPERATION_SDIV,
    ARM64_OPERATION_LSLV,
    ARM64_OPERATION_LSRV,
    ARM64_OPERATION_ASRV,
    ARM64_OPERATION_RORV,
    ARM64_OPERATION_CRC32B,
    ARM64_OPERATION_CRC32H,
    ARM64_OPERATION_CRC32W,
    ARM64_OPERATION_CRC32X,
    ARM64_OPERATION_CRC32CB,
    ARM64_OPERATION_CRC32CH,
    ARM64_OPERATION_CRC32CW,
    ARM64_OPERATION_CRC32CX,
    ARM64_OPERATION_MADD,
    ARM64_OPERATION_MSUB,
    ARM64_OPERATION_SMADDL,
    ARM64_OPERATION_SMSUBL,
    ARM64_OPERATION_UMADDL,
    ARM64_OPERATION_UMSUBL,
    ARM64_OPERATION_SMULH,
    ARM64_OPERATION_UMULH,
    ARM64_OPERATION_RBIT,
    ARM64_OPERATION_REV16,
    ARM64_OPERATION_REV32,
    ARM64_OPERATION_REV64,
    ARM64_OPERATION_CLZ,
    ARM64_OPERATION_CLS,
    ARM64_OPERATION_CTZ,
    ARM64_OPERATION_CNT,
    ARM64_OPERATION_ABS,
    ARM64_OPERATION_LDADD,
    ARM64_OPERATION_LDCLR,
    ARM64_OPERATION_LDEOR,
    ARM64_OPERATION_LDSET,
    ARM64_OPERATION_LDSMAX,
    ARM64_OPERATION_LDSMIN,
    ARM64_OPERATION_LDUMAX,
    ARM64_OPERATION_LDUMIN,
    ARM64_OPERATION_SWP,
};

// 已归一化的访存寻址模式。
enum arm64_address_mode
{
    ARM64_ADDRESS_NONE,
    ARM64_ADDRESS_LITERAL,
    ARM64_ADDRESS_BASE,
    ARM64_ADDRESS_UNSIGNED_OFFSET,
    ARM64_ADDRESS_UNSCALED_OFFSET,
    ARM64_ADDRESS_PRE_INDEX,
    ARM64_ADDRESS_POST_INDEX,
    ARM64_ADDRESS_REGISTER_OFFSET,
};

// FP/AdvSIMD 的编码分组；具体动作见 arm64_simd_operation。
enum arm64_simd_group
{
    ARM64_SIMD_GROUP_NONE,
    ARM64_SIMD_GROUP_SCALAR_IMMEDIATE,
    ARM64_SIMD_GROUP_SCALAR_2SOURCE,
    ARM64_SIMD_GROUP_SCALAR_1SOURCE,
    ARM64_SIMD_GROUP_SCALAR_3SOURCE,
    ARM64_SIMD_GROUP_SCALAR_COMPARE,
    ARM64_SIMD_GROUP_SCALAR_CONDITIONAL_COMPARE,
    ARM64_SIMD_GROUP_SCALAR_SELECT,
    ARM64_SIMD_GROUP_FMOV_GENERAL,
    ARM64_SIMD_GROUP_CONVERT,
    ARM64_SIMD_GROUP_EXT,
    ARM64_SIMD_GROUP_VECTOR_MODIFIED_IMMEDIATE,
    ARM64_SIMD_GROUP_VECTOR_COPY,
    ARM64_SIMD_GROUP_VECTOR_SHIFT_IMMEDIATE,
    ARM64_SIMD_GROUP_VECTOR_3REG,
    ARM64_SIMD_GROUP_VECTOR_2REG,
    ARM64_SIMD_GROUP_SCALAR_COPY,
    ARM64_SIMD_GROUP_FP_BY_ELEMENT,
};

enum arm64_simd_operation
{
    ARM64_SIMD_OP_NONE,
    ARM64_SIMD_OP_FMUL,
    ARM64_SIMD_OP_FDIV,
    ARM64_SIMD_OP_FADD,
    ARM64_SIMD_OP_FSUB,
    ARM64_SIMD_OP_FMAX,
    ARM64_SIMD_OP_FMIN,
    ARM64_SIMD_OP_FMAXNM,
    ARM64_SIMD_OP_FMINNM,
    ARM64_SIMD_OP_FNMUL,
    ARM64_SIMD_OP_FMOV,
    ARM64_SIMD_OP_FMOV_IMMEDIATE,
    ARM64_SIMD_OP_FABS,
    ARM64_SIMD_OP_FNEG,
    ARM64_SIMD_OP_FSQRT,
    ARM64_SIMD_OP_FRINT,
    ARM64_SIMD_OP_FMADD,
    ARM64_SIMD_OP_FMSUB,
    ARM64_SIMD_OP_FNMADD,
    ARM64_SIMD_OP_FNMSUB,
    ARM64_SIMD_OP_FCMP,
    ARM64_SIMD_OP_FCMPE,
    ARM64_SIMD_OP_FCCMP,
    ARM64_SIMD_OP_FCCMPE,
    ARM64_SIMD_OP_FCSEL,
    ARM64_SIMD_OP_FMOV_GENERAL_TO_FP,
    ARM64_SIMD_OP_FMOV_FP_TO_GENERAL,
    ARM64_SIMD_OP_EXT,
    ARM64_SIMD_OP_MOVI,
    ARM64_SIMD_OP_MVNI,
    ARM64_SIMD_OP_ORR_IMMEDIATE,
    ARM64_SIMD_OP_BIC_IMMEDIATE,
    ARM64_SIMD_OP_DUP_GENERAL,
    ARM64_SIMD_OP_DUP_ELEMENT,
    ARM64_SIMD_OP_INS_GENERAL,
    ARM64_SIMD_OP_INS_ELEMENT,
    ARM64_SIMD_OP_UMOV,
    ARM64_SIMD_OP_SMOV,
    ARM64_SIMD_OP_ADD,
    ARM64_SIMD_OP_SUB,
    ARM64_SIMD_OP_CMEQ,
    ARM64_SIMD_OP_CMGT,
    ARM64_SIMD_OP_CMGE,
    ARM64_SIMD_OP_CMHI,
    ARM64_SIMD_OP_CMHS,
    ARM64_SIMD_OP_SHL,
    ARM64_SIMD_OP_SSHR,
    ARM64_SIMD_OP_USHR,

    ARM64_SIMD_OP_SCVTF_S_W,
    ARM64_SIMD_OP_SCVTF_S_X,
    ARM64_SIMD_OP_SCVTF_D_W,
    ARM64_SIMD_OP_SCVTF_D_X,
    ARM64_SIMD_OP_UCVTF_S_W,
    ARM64_SIMD_OP_UCVTF_S_X,
    ARM64_SIMD_OP_UCVTF_D_W,
    ARM64_SIMD_OP_UCVTF_D_X,
    ARM64_SIMD_OP_FCVT_TO_SIGNED,
    ARM64_SIMD_OP_FCVT_TO_UNSIGNED,
    ARM64_SIMD_OP_FCVT_S_D,
    ARM64_SIMD_OP_FCVT_D_S,
    ARM64_SIMD_OP_FCVT_TO_SIGNED_SIMD,
    ARM64_SIMD_OP_FCVT_TO_UNSIGNED_SIMD,
    ARM64_SIMD_OP_SCVTF_SIMD,
    ARM64_SIMD_OP_UCVTF_SIMD,

    ARM64_SIMD_OP_FADD_V2S,
    ARM64_SIMD_OP_FADD_V4S,
    ARM64_SIMD_OP_FADD_V2D,
    ARM64_SIMD_OP_FSUB_V2S,
    ARM64_SIMD_OP_FSUB_V4S,
    ARM64_SIMD_OP_FSUB_V2D,
    ARM64_SIMD_OP_FMUL_V2S,
    ARM64_SIMD_OP_FMUL_V4S,
    ARM64_SIMD_OP_FMUL_V2D,
    ARM64_SIMD_OP_FDIV_V2S,
    ARM64_SIMD_OP_FDIV_V4S,
    ARM64_SIMD_OP_FDIV_V2D,
    ARM64_SIMD_OP_FMLA_V4S,
    ARM64_SIMD_OP_FMLS_V4S,
    ARM64_SIMD_OP_FMAX_V2S,
    ARM64_SIMD_OP_FMAX_V4S,
    ARM64_SIMD_OP_FMAX_V2D,
    ARM64_SIMD_OP_FMIN_V2S,
    ARM64_SIMD_OP_FMIN_V4S,
    ARM64_SIMD_OP_FMIN_V2D,
    ARM64_SIMD_OP_FMAXNM_V2S,
    ARM64_SIMD_OP_FMAXNM_V4S,
    ARM64_SIMD_OP_FMAXNM_V2D,
    ARM64_SIMD_OP_FMINNM_V2S,
    ARM64_SIMD_OP_FMINNM_V4S,
    ARM64_SIMD_OP_FMINNM_V2D,
    ARM64_SIMD_OP_FADDP_V2S,
    ARM64_SIMD_OP_FADDP_V4S,
    ARM64_SIMD_OP_FADDP_V2D,
    ARM64_SIMD_OP_FMAXP_V2S,
    ARM64_SIMD_OP_FMAXP_V4S,
    ARM64_SIMD_OP_FMAXP_V2D,
    ARM64_SIMD_OP_FMINP_V2S,
    ARM64_SIMD_OP_FMINP_V4S,
    ARM64_SIMD_OP_FMINP_V2D,
    ARM64_SIMD_OP_FCMEQ_V2S,
    ARM64_SIMD_OP_FCMEQ_V4S,
    ARM64_SIMD_OP_FCMGE_V4S,
    ARM64_SIMD_OP_FCMGT_V4S,
    ARM64_SIMD_OP_FABS_V2S,
    ARM64_SIMD_OP_FABS_V4S,
    ARM64_SIMD_OP_FABS_V2D,
    ARM64_SIMD_OP_FNEG_V2S,
    ARM64_SIMD_OP_FNEG_V4S,
    ARM64_SIMD_OP_FNEG_V2D,
    ARM64_SIMD_OP_FSQRT_V2S,
    ARM64_SIMD_OP_FSQRT_V4S,
    ARM64_SIMD_OP_FSQRT_V2D,
    ARM64_SIMD_OP_REV64_V16B,
    ARM64_SIMD_OP_REV64_V8H,
    ARM64_SIMD_OP_REV32_V16B,
    ARM64_SIMD_OP_REV16_V16B,
    ARM64_SIMD_OP_AND_V16B,
    ARM64_SIMD_OP_ORR_V16B,
    ARM64_SIMD_OP_EOR_V16B,
    ARM64_SIMD_OP_FADDP_S_V2S,
    ARM64_SIMD_OP_FADDP_D_V2D,
    ARM64_SIMD_OP_FMAXV_S_V4S,
    ARM64_SIMD_OP_FMINV_S_V4S,
    ARM64_SIMD_OP_FMLA_BY_ELEMENT,
    ARM64_SIMD_OP_FMLS_BY_ELEMENT,
    ARM64_SIMD_OP_FMUL_BY_ELEMENT,
    ARM64_SIMD_OP_FMULX_BY_ELEMENT,
};

enum arm64_fp_rounding_mode
{
    ARM64_FP_ROUND_NONE,
    ARM64_FP_ROUND_NEAREST_EVEN,
    ARM64_FP_ROUND_PLUS_INFINITY,
    ARM64_FP_ROUND_MINUS_INFINITY,
    ARM64_FP_ROUND_ZERO,
    ARM64_FP_ROUND_NEAREST_AWAY,
    ARM64_FP_ROUND_CURRENT,
    ARM64_FP_ROUND_CURRENT_EXACT,
};

#define ARM64_SIMD_FLAG_COMPARE_ZERO (1U << 0)

// 跨指令族共享的语义修饰位，调用方无需再检查原始机器码。
#define ARM64_INSN_FLAG_64BIT        (1U << 0)
#define ARM64_INSN_FLAG_SETFLAGS     (1U << 1)
#define ARM64_INSN_FLAG_SUBTRACT     (1U << 2)
#define ARM64_INSN_FLAG_LOAD         (1U << 3)
#define ARM64_INSN_FLAG_STORE        (1U << 4)
#define ARM64_INSN_FLAG_FP           (1U << 5)
#define ARM64_INSN_FLAG_WRITEBACK    (1U << 6)
#define ARM64_INSN_FLAG_SIGN_EXTEND  (1U << 7)
#define ARM64_INSN_FLAG_ACQUIRE      (1U << 8)
#define ARM64_INSN_FLAG_RELEASE      (1U << 9)
#define ARM64_INSN_FLAG_PAIR         (1U << 10)
#define ARM64_INSN_FLAG_INVERT       (1U << 11)
#define ARM64_INSN_FLAG_ORDERED      (1U << 12)
#define ARM64_INSN_FLAG_IMMEDIATE    (1U << 13)
#define ARM64_INSN_FLAG_RELOCATABLE  (1U << 14)
#define ARM64_INSN_FLAG_NON_TEMPORAL (1U << 15)
#define ARM64_INSN_FLAG_UNPRIVILEGED (1U << 16)

struct arm64_branch_operands
{
    // 直接分支的 PC 字节偏移。
    arm64_s64 offset;
    arm64_u8 condition;
    arm64_u8 test_bit;
};

struct arm64_pc_relative_operands
{
    // ADR 为 PC 字节偏移，ADRP 为相对当前 PC 页基址的字节偏移。
    arm64_s64 offset;
};

struct arm64_system_operands
{
    enum arm64_system_operation operation;
    // exception immediate 为 imm16；barrier/CLREX 使用低 4 位 option。
    arm64_u16 immediate;
    arm64_u8 option;
    arm64_u8 op0;
    arm64_u8 op1;
    arm64_u8 crn;
    arm64_u8 crm;
    arm64_u8 op2;
};

struct arm64_data_operands
{
    // immediate、wmask 和 tmask 已展开为执行器可以直接使用的值。
    arm64_u64 immediate;
    arm64_u64 wmask;
    arm64_u64 tmask;
    arm64_u8 shift_type;
    arm64_u8 shift_amount;
    arm64_u8 option;
    arm64_u8 immr;
    arm64_u8 condition;
    arm64_u8 nzcv;
};

struct arm64_load_store_operands
{
    // offset 始终是字节偏移，access_bytes 是单个元素的访问字节数。
    arm64_s64 offset;
    enum arm64_address_mode address_mode;
    arm64_u8 access_bytes;
    arm64_u8 extend_type;
    arm64_u8 shift_amount;
};

struct arm64_memory_address
{
    arm64_u64 address;
    arm64_u64 writeback_address;
    arm64_u8 writeback;
};

struct arm64_simd_operands
{
    enum arm64_simd_group group;
    enum arm64_simd_operation operation;
    enum arm64_fp_rounding_mode rounding_mode;
    arm64_u64 expanded_immediate;
    arm64_u32 flags;
    arm64_u8 condition;
    arm64_u8 immediate;
    arm64_u8 element_width;
    arm64_u8 lane_index;
    arm64_u8 source_lane_index;
};

struct arm64_decoded_insn
{
    // raw/status 使调用方可以缓存单个结构，不必另外保存输入和返回状态。
    arm64_u32 raw;
    enum arm64_decode_status status;
    arm64_u64 effects;

    // 先按 class 分发，再结合 opcode/operation 解释 union 中的对应成员。
    enum arm64_insn_class insn_class;
    enum arm64_opcode opcode;
    enum arm64_operation operation;
    arm64_u32 flags;

    // 寄存器字段按编码位置提取；哪些字段具有语义由 opcode 决定。寄存器 31 按原值返回。
    arm64_u8 rd;
    arm64_u8 rn;
    arm64_u8 rm;
    arm64_u8 ra;
    arm64_u8 rt;
    arm64_u8 rt2;
    arm64_u8 rs;
    // 标量/GPR 指令通常为 32/64，向量指令可为 64/128。
    arm64_u8 operand_width;

    // union 的有效成员由 insn_class/opcode 决定。
    union
    {
        struct arm64_branch_operands branch;
        struct arm64_pc_relative_operands pc_relative;
        struct arm64_system_operands system;
        struct arm64_data_operands data;
        struct arm64_load_store_operands load_store;
        struct arm64_simd_operands simd;
    } operands;
};

/*
解码一条 AArch64 指令。

decoded 不能为空；函数会先将其全部清零。返回 OK 时可消费完整语义；返回
UNSUPPORTED 时只保证已填写的 class/opcode/flags 等识别信息有效；其他失败
状态下调用方不应执行该指令。
*/
enum arm64_decode_status arm64_decode_insn(arm64_u32 raw, struct arm64_decoded_insn *decoded);

// 根据 insn_class/opcode/flags 以及 SIMD group/operation 重新生成 effects。
arm64_u64 arm64_decode_effects(const struct arm64_decoded_insn *decoded);

// 解析 ADR/ADRP、直接分支和 literal load/prefetch 的绝对目标地址。
int arm64_decode_direct_target(const struct arm64_decoded_insn *decoded, arm64_u64 pc, arm64_u64 *target);

/*
根据已归一化寻址模式计算访存地址。index_value 应由调用方从 rm 读取；
扩展宽度由 extend_type 决定，无寄存器 offset 的形式可传 0。
返回非零表示成功，返回 0 表示该指令没有可解析的访存地址或参数无效。
*/
int arm64_decode_memory_address(const struct arm64_decoded_insn *decoded, arm64_u64 pc, arm64_u64 base, arm64_u64 index_value, struct arm64_memory_address *address);

#endif
