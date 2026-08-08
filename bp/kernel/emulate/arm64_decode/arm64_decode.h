#ifndef ARM64_DECODE_H
#define ARM64_DECODE_H

/*
输入是一条 32 位指令 word，输出是与运行环境无关的结构化语义；
*/

#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif

#define ARM64_SYSREG_KEY(OP0, OP1, CRN, CRM, OP2) ((((OP0) & 0x3) << 14) | (((OP1) & 0x7) << 11) | (((CRN) & 0xF) << 7) | (((CRM) & 0xF) << 3) | ((OP2) & 0x7))

/* 修改字段含义或有效性契约时必须递增版本，并由每个调用方独立重新审阅。 */
#define ARM64_DECODE_SEMANTIC_SCHEMA_VERSION 1U

/* *_COUNT 和 *_ENUM_END 仅用于调用方锁定已审阅的语义契约，不是合法的解码输出。 */

enum arm64_decode_status
{

    ARM64_DECODE_OK,            // 指令已完整解码，decoded 中与该 opcode 对应的字段有效。
    ARM64_DECODE_UNSUPPORTED,   // 已识别指令大类，但当前还没有提供完整语义。
    ARM64_DECODE_UNPREDICTABLE, // 编码已分配，但其行为受架构 constrained-unpredictable 约束。
    ARM64_DECODE_UNALLOCATED,   // 这个机器码不是架构分配的合法指令
    ARM64_DECODE_STATUS_COUNT,
};

// 对应 Arm A64 main encoding table 的顶层指令分类。
enum arm64_insn_class
{
    ARM64_INSN_CLASS_UNKNOWN, // 无效/尚未分类，不是指令类别

    ARM64_INSN_CLASS_DATA_PROCESSING_IMMEDIATE, // 基础类别 1：数据处理（立即数）
    ARM64_INSN_CLASS_DATA_PROCESSING_REGISTER,  // 基础类别 2：数据处理（寄存器）
    ARM64_INSN_CLASS_LOAD_STORE,                // 基础类别 3：加载与存储
    ARM64_INSN_CLASS_BRANCH_EXCEPTION_SYSTEM,   // 基础类别 4：分支、异常生成与系统
    ARM64_INSN_CLASS_DATA_PROCESSING_SIMD_FP,   // 基础类别 5：数据处理（SIMD 与浮点）

    ARM64_INSN_CLASS_SVE, // SVE 可伸缩向量扩展指令，当前仅识别编码空间
    ARM64_INSN_CLASS_SME, // SME 可伸缩矩阵扩展指令，当前仅识别编码空间
    ARM64_INSN_CLASS_COUNT,
};

/*
指令编码族。opcode 描述指令采用的架构编码格式，具体运算由 operation
或 operands.simd.operation 表示，例如 ADD/SUB 共用 ADD_SUB 编码族。
*/
enum arm64_opcode
{
    ARM64_OP_UNKNOWN, // 未识别或尚未设置编码族

    // Branch, exception generation and system instructions.
    ARM64_OP_HINT,                 // 提示指令，如 NOP、YIELD、WFE、WFI、SEV、SEVL
    ARM64_OP_BARRIER,              // 屏障及独占监视器清除指令，如 DSB、DMB、ISB、CLREX
    ARM64_OP_EXCEPTION_GENERATION, // 异常生成指令，如 SVC、HVC、SMC、BRK、HLT
    ARM64_OP_EXCEPTION_RETURN,     // 异常返回指令，如 ERET、DRPS
    ARM64_OP_MRS,                  // 读取系统寄存器
    ARM64_OP_MSR_REGISTER,         // 写入系统寄存器
    ARM64_OP_B,                    // 立即数无条件分支
    ARM64_OP_BL,                   // 立即数带链接分支
    ARM64_OP_BR,                   // 寄存器无条件分支
    ARM64_OP_BLR,                  // 寄存器带链接分支
    ARM64_OP_RET,                  // 寄存器返回
    ARM64_OP_B_COND,               // 条件分支
    ARM64_OP_CBZ,                  // 比较为零并分支
    ARM64_OP_CBNZ,                 // 比较非零并分支
    ARM64_OP_TBZ,                  // 测试位为零并分支
    ARM64_OP_TBNZ,                 // 测试位非零并分支

    // Data processing -- immediate.
    ARM64_OP_ADR,                      // PC 相对地址
    ARM64_OP_ADRP,                     // PC 页相对地址
    ARM64_OP_ADD_SUB_IMMEDIATE,        // 加减立即数
    ARM64_OP_MIN_MAX_IMMEDIATE,        // 有符号/无符号最值立即数
    ARM64_OP_LOGICAL_IMMEDIATE,        // 逻辑立即数
    ARM64_OP_BITFIELD,                 // 位域移动、插入或提取
    ARM64_OP_EXTRACT,                  // 双寄存器位提取
    ARM64_OP_MOVE_WIDE,                // 宽立即数移动
    ARM64_OP_ADD_SUB_SHIFTED,          // 加减移位寄存器
    ARM64_OP_ADD_SUB_EXTENDED,         // 加减扩展寄存器
    ARM64_OP_LOGICAL_SHIFTED,          // 逻辑移位寄存器
    ARM64_OP_CONDITIONAL_SELECT,       // 条件选择
    ARM64_OP_DATA_PROCESSING_2_SOURCE, // 双源寄存器数据处理
    ARM64_OP_MULTIPLY_ADD,             // 乘加、乘减及长乘加减
    ARM64_OP_MULTIPLY_HIGH,            // 有符号/无符号高位乘法
    ARM64_OP_ADD_SUB_CARRY,            // 带进位加减
    ARM64_OP_CONDITIONAL_COMPARE,      // 条件比较
    ARM64_OP_DATA_PROCESSING_1_SOURCE, // 单源寄存器数据处理

    ARM64_OP_ATOMIC_RMW,        // 原子读改写
    ARM64_OP_CAS,               // 单寄存器原子比较并交换
    ARM64_OP_CASP,              // 寄存器对原子比较并交换
    ARM64_OP_EXCLUSIVE,         // 独占加载与存储
    ARM64_OP_ORDERED,           // 有序加载与存储
    ARM64_OP_RCPC_UNSCALED,     // RCpc 非缩放加载
    ARM64_OP_LDAPR,             // RCpc 获取加载
    ARM64_OP_PREFETCH,          // 基址寻址预取
    ARM64_OP_RPRFM,             // 范围预取内存，使用元数据寄存器描述预取范围
    ARM64_OP_LOAD_LITERAL,      // PC 相对字面量加载
    ARM64_OP_PREFETCH_LITERAL,  // PC 相对字面量预取
    ARM64_OP_LOAD_STORE_PAIR,   // 寄存器对加载与存储
    ARM64_OP_LOAD_STORE_SINGLE, // 单寄存器加载与存储

    ARM64_OP_FP_SIMD, // 标量浮点及 Advanced SIMD
    ARM64_OP_SVE,     // SVE 可伸缩向量扩展，当前仅识别编码空间
    ARM64_OP_SME,     // SME 可伸缩矩阵扩展，当前仅识别编码空间
    ARM64_OP_COUNT,
};

// 常见系统、hint、屏障和异常指令的具体语义。
enum arm64_system_operation
{
    ARM64_SYSTEM_OP_NONE,    // 未识别或尚未设置具体系统操作
    ARM64_SYSTEM_OP_NOP,     // 空操作提示
    ARM64_SYSTEM_OP_YIELD,   // 提示当前线程主动让出执行资源
    ARM64_SYSTEM_OP_WFE,     // 等待事件
    ARM64_SYSTEM_OP_WFI,     // 等待中断
    ARM64_SYSTEM_OP_SEV,     // 向系统中的处理单元发送事件
    ARM64_SYSTEM_OP_SEVL,    // 向当前处理单元发送本地事件
    ARM64_SYSTEM_OP_CLREX,   // 清除当前处理单元的独占监视状态
    ARM64_SYSTEM_OP_DSB,     // 数据同步屏障
    ARM64_SYSTEM_OP_DMB,     // 数据内存屏障
    ARM64_SYSTEM_OP_ISB,     // 指令同步屏障
    ARM64_SYSTEM_OP_SVC,     // 发起监管程序调用异常
    ARM64_SYSTEM_OP_HVC,     // 发起虚拟机监控程序调用异常
    ARM64_SYSTEM_OP_SMC,     // 发起安全监控程序调用异常
    ARM64_SYSTEM_OP_BRK,     // 发起软件断点异常
    ARM64_SYSTEM_OP_HLT,     // 停止执行的调试指令
    ARM64_SYSTEM_OP_ERET,    // 从异常返回
    ARM64_SYSTEM_OP_DRPS,    // 从调试异常恢复处理器状态
    ARM64_SYSTEM_OP_PACIASP, // 使用 IA 密钥和 SP 对返回地址 X30 签名
    ARM64_SYSTEM_OP_BTI,     // 标记允许的间接分支目标及入口类型
    ARM64_SYSTEM_OP_COUNT,
};

enum arm64_bti_type
{
    ARM64_BTI_TYPE_NONE = 0, // BTI 无目标类型限定
    ARM64_BTI_TYPE_C = 2,    // 允许间接调用到达
    ARM64_BTI_TYPE_J = 4,    // 允许间接跳转到达
    ARM64_BTI_TYPE_JC = 6,   // 允许间接跳转或调用到达
    ARM64_BTI_TYPE_ENUM_END,
};

// 整数、位运算和原子指令的具体语义操作。
enum arm64_operation
{
    ARM64_OPERATION_NONE,    // 未识别或尚未设置具体操作
    ARM64_OPERATION_ADD,     // 加法
    ARM64_OPERATION_ADDS,    // 加法并更新条件标志
    ARM64_OPERATION_SUB,     // 减法
    ARM64_OPERATION_SUBS,    // 减法并更新条件标志
    ARM64_OPERATION_AND,     // 按位与
    ARM64_OPERATION_BIC,     // 按位与取反后的第二操作数
    ARM64_OPERATION_ORR,     // 按位或
    ARM64_OPERATION_ORN,     // 按位或取反后的第二操作数
    ARM64_OPERATION_EOR,     // 按位异或
    ARM64_OPERATION_EON,     // 按位异或取反后的第二操作数
    ARM64_OPERATION_ANDS,    // 按位与并更新条件标志
    ARM64_OPERATION_BICS,    // 按位与取反后的第二操作数并更新条件标志
    ARM64_OPERATION_SBFM,    // 有符号位域移动
    ARM64_OPERATION_BFM,     // 位域移动并保留目标寄存器其他位
    ARM64_OPERATION_UBFM,    // 无符号位域移动
    ARM64_OPERATION_EXTR,    // 从两个寄存器拼接值中提取位段
    ARM64_OPERATION_MOVN,    // 宽立即数取反后移动
    ARM64_OPERATION_MOVZ,    // 宽立即数移动并将其他位清零
    ARM64_OPERATION_MOVK,    // 宽立即数移动并保留其他位
    ARM64_OPERATION_SMAX,    // 有符号最大值
    ARM64_OPERATION_SMIN,    // 有符号最小值
    ARM64_OPERATION_UMAX,    // 无符号最大值
    ARM64_OPERATION_UMIN,    // 无符号最小值
    ARM64_OPERATION_CSEL,    // 条件选择
    ARM64_OPERATION_CSINC,   // 条件选择，否则选择值加一
    ARM64_OPERATION_CSINV,   // 条件选择，否则选择值按位取反
    ARM64_OPERATION_CSNEG,   // 条件选择，否则选择值取负
    ARM64_OPERATION_ADC,     // 带进位加法
    ARM64_OPERATION_ADCS,    // 带进位加法并更新条件标志
    ARM64_OPERATION_SBC,     // 带进位减法
    ARM64_OPERATION_SBCS,    // 带进位减法并更新条件标志
    ARM64_OPERATION_CCMP,    // 条件满足时比较，否则直接设置 NZCV
    ARM64_OPERATION_CCMN,    // 条件满足时以加法比较，否则直接设置 NZCV
    ARM64_OPERATION_UDIV,    // 无符号除法
    ARM64_OPERATION_SDIV,    // 有符号除法
    ARM64_OPERATION_LSLV,    // 寄存器指定移位量的逻辑左移
    ARM64_OPERATION_LSRV,    // 寄存器指定移位量的逻辑右移
    ARM64_OPERATION_ASRV,    // 寄存器指定移位量的算术右移
    ARM64_OPERATION_RORV,    // 寄存器指定旋转量的循环右移
    ARM64_OPERATION_CRC32B,  // 对 8 位数据计算 CRC32
    ARM64_OPERATION_CRC32H,  // 对 16 位数据计算 CRC32
    ARM64_OPERATION_CRC32W,  // 对 32 位数据计算 CRC32
    ARM64_OPERATION_CRC32X,  // 对 64 位数据计算 CRC32
    ARM64_OPERATION_CRC32CB, // 对 8 位数据计算 CRC32C
    ARM64_OPERATION_CRC32CH, // 对 16 位数据计算 CRC32C
    ARM64_OPERATION_CRC32CW, // 对 32 位数据计算 CRC32C
    ARM64_OPERATION_CRC32CX, // 对 64 位数据计算 CRC32C
    ARM64_OPERATION_MADD,    // 乘加
    ARM64_OPERATION_MSUB,    // 乘减
    ARM64_OPERATION_SMADDL,  // 有符号长乘加：32 位乘积累加到 64 位
    ARM64_OPERATION_SMSUBL,  // 有符号长乘减：从 64 位值减去 32 位乘积
    ARM64_OPERATION_UMADDL,  // 无符号长乘加：32 位乘积累加到 64 位
    ARM64_OPERATION_UMSUBL,  // 无符号长乘减：从 64 位值减去 32 位乘积
    ARM64_OPERATION_SMULH,   // 有符号 64 位乘积的高 64 位
    ARM64_OPERATION_UMULH,   // 无符号 64 位乘积的高 64 位
    ARM64_OPERATION_RBIT,    // 反转所有位的顺序
    ARM64_OPERATION_REV16,   // 反转每个 16 位半字中的字节顺序
    ARM64_OPERATION_REV32,   // 反转每个 32 位字中的字节顺序
    ARM64_OPERATION_REV64,   // 反转 64 位双字中的字节顺序
    ARM64_OPERATION_CLZ,     // 统计前导零位数
    ARM64_OPERATION_CLS,     // 统计前导符号位数
    ARM64_OPERATION_CTZ,     // 统计尾随零位数
    ARM64_OPERATION_CNT,     // 统计值为一的位数
    ARM64_OPERATION_ABS,     // 有符号绝对值
    ARM64_OPERATION_LDADD,   // 原子加载并加
    ARM64_OPERATION_LDCLR,   // 原子加载并按掩码清位
    ARM64_OPERATION_LDEOR,   // 原子加载并按位异或
    ARM64_OPERATION_LDSET,   // 原子加载并按掩码置位
    ARM64_OPERATION_LDSMAX,  // 原子加载并取有符号最大值
    ARM64_OPERATION_LDSMIN,  // 原子加载并取有符号最小值
    ARM64_OPERATION_LDUMAX,  // 原子加载并取无符号最大值
    ARM64_OPERATION_LDUMIN,  // 原子加载并取无符号最小值
    ARM64_OPERATION_SWP,     // 原子交换
    ARM64_OPERATION_COUNT,
};

// 已归一化的访存寻址模式。
enum arm64_address_mode
{
    ARM64_ADDRESS_NONE,
    ARM64_ADDRESS_LITERAL,
    ARM64_ADDRESS_BASE,
    ARM64_ADDRESS_NON_TEMPORAL_OFFSET,
    ARM64_ADDRESS_UNSIGNED_OFFSET,
    ARM64_ADDRESS_UNSCALED_OFFSET,
    ARM64_ADDRESS_UNPRIVILEGED_OFFSET,
    ARM64_ADDRESS_PRE_INDEX,
    ARM64_ADDRESS_POST_INDEX,
    ARM64_ADDRESS_REGISTER_OFFSET,
    ARM64_ADDRESS_MODE_COUNT,
};

enum arm64_operand_source
{
    ARM64_OPERAND_SOURCE_NONE,      // 指令没有可切换的第二操作数来源
    ARM64_OPERAND_SOURCE_REGISTER,  // 第二操作数来自寄存器
    ARM64_OPERAND_SOURCE_IMMEDIATE, // 第二操作数来自立即数
    ARM64_OPERAND_SOURCE_COUNT,
};

enum arm64_memory_access
{
    ARM64_MEMORY_ACCESS_NONE,       // 不产生架构可见的数据传输，如预取
    ARM64_MEMORY_ACCESS_LOAD,       // 从内存加载
    ARM64_MEMORY_ACCESS_STORE,      // 向内存存储
    ARM64_MEMORY_ACCESS_READ_WRITE, // 原子读改写
    ARM64_MEMORY_ACCESS_COUNT,
};

enum arm64_memory_register_kind
{
    ARM64_MEMORY_REGISTER_NONE,    // 不传输寄存器数据
    ARM64_MEMORY_REGISTER_GPR,     // 通用寄存器
    ARM64_MEMORY_REGISTER_FP_SIMD, // 浮点或 SIMD 寄存器
    ARM64_MEMORY_REGISTER_KIND_COUNT,
};

enum arm64_memory_order
{
    ARM64_MEMORY_ORDER_NONE,            // 无额外内存顺序语义
    ARM64_MEMORY_ORDER_ACQUIRE,         // Acquire 语义
    ARM64_MEMORY_ORDER_RELEASE,         // Release 语义
    ARM64_MEMORY_ORDER_ACQUIRE_RELEASE, // Acquire 与 Release 语义
    ARM64_MEMORY_ORDER_LO_ACQUIRE,      // Load LOAcquire 语义，如 LDLAR
    ARM64_MEMORY_ORDER_LO_RELEASE,      // Store LORelease 语义，如 STLLR
    ARM64_MEMORY_ORDER_COUNT,
};

enum arm64_memory_extension
{
    ARM64_MEMORY_EXTENSION_NONE, // 不进行整数扩展
    ARM64_MEMORY_EXTENSION_ZERO, // 零扩展加载结果
    ARM64_MEMORY_EXTENSION_SIGN, // 符号扩展加载结果
    ARM64_MEMORY_EXTENSION_COUNT,
};

enum arm64_memory_transfer
{
    ARM64_MEMORY_TRANSFER_NONE,   // 不传输寄存器数据
    ARM64_MEMORY_TRANSFER_SINGLE, // 单寄存器传输
    ARM64_MEMORY_TRANSFER_PAIR,   // 寄存器对传输
    ARM64_MEMORY_TRANSFER_COUNT,
};

// FP/AdvSIMD 的操作数形态；具体动作见 arm64_simd_operation。
enum arm64_simd_form
{
    ARM64_SIMD_FORM_NONE,
    ARM64_SIMD_FORM_SCALAR_FP_IMMEDIATE,
    ARM64_SIMD_FORM_SCALAR_FP_BINARY,
    ARM64_SIMD_FORM_SCALAR_FP_UNARY,
    ARM64_SIMD_FORM_SCALAR_FP_TERNARY,
    ARM64_SIMD_FORM_SCALAR_COMPARE,
    ARM64_SIMD_FORM_SCALAR_CONDITIONAL_COMPARE,
    ARM64_SIMD_FORM_SCALAR_SELECT,
    ARM64_SIMD_FORM_FP_GPR_TRANSFER,
    ARM64_SIMD_FORM_CONVERT,
    ARM64_SIMD_FORM_VECTOR_EXTRACT,
    ARM64_SIMD_FORM_VECTOR_IMMEDIATE,
    ARM64_SIMD_FORM_VECTOR_COPY,
    ARM64_SIMD_FORM_VECTOR_SHIFT,
    ARM64_SIMD_FORM_VECTOR_PERMUTE,
    ARM64_SIMD_FORM_VECTOR_LOGICAL,
    ARM64_SIMD_FORM_VECTOR_INTEGER_3REG,
    ARM64_SIMD_FORM_VECTOR_EXTENDED_3REG,
    ARM64_SIMD_FORM_VECTOR_FP_3REG,
    ARM64_SIMD_FORM_VECTOR_FP_WIDENING_3REG,
    ARM64_SIMD_FORM_VECTOR_COMPLEX_3REG,
    ARM64_SIMD_FORM_VECTOR_FP_UNARY,
    ARM64_SIMD_FORM_VECTOR_REVERSE,
    ARM64_SIMD_FORM_SCALAR_COPY,
    ARM64_SIMD_FORM_FP_BY_ELEMENT,
    ARM64_SIMD_FORM_FP_COMPARE_ZERO,
    ARM64_SIMD_FORM_FP_REDUCE,
    ARM64_SIMD_FORM_SCALAR_SIMD_3REG,
    ARM64_SIMD_FORM_VECTOR_BY_ELEMENT,
    ARM64_SIMD_FORM_SCALAR_BY_ELEMENT,
    ARM64_SIMD_FORM_VECTOR_INTEGER_REDUCE,
    ARM64_SIMD_FORM_VECTOR_NARROW,
    ARM64_SIMD_FORM_SCALAR_NARROW,
    ARM64_SIMD_FORM_COUNT,
};

enum arm64_simd_operation
{
    ARM64_SIMD_OP_NONE,
    ARM64_SIMD_OP_FMUL,
    ARM64_SIMD_OP_FMULX,
    ARM64_SIMD_OP_FDIV,
    ARM64_SIMD_OP_FADD,
    ARM64_SIMD_OP_FSUB,
    ARM64_SIMD_OP_FMAX,
    ARM64_SIMD_OP_FMIN,
    ARM64_SIMD_OP_FMAXNM,
    ARM64_SIMD_OP_FMINNM,
    ARM64_SIMD_OP_FNMUL,
    ARM64_SIMD_OP_FMOV,
    ARM64_SIMD_OP_FABS,
    ARM64_SIMD_OP_FNEG,
    ARM64_SIMD_OP_FSQRT,
    ARM64_SIMD_OP_FRINT,
    ARM64_SIMD_OP_FMLA,
    ARM64_SIMD_OP_FMLS,
    ARM64_SIMD_OP_FMLAL,
    ARM64_SIMD_OP_FMLSL,
    ARM64_SIMD_OP_FMADD,
    ARM64_SIMD_OP_FMSUB,
    ARM64_SIMD_OP_FNMADD,
    ARM64_SIMD_OP_FNMSUB,
    ARM64_SIMD_OP_FADDP,
    ARM64_SIMD_OP_FMAXP,
    ARM64_SIMD_OP_FMINP,
    ARM64_SIMD_OP_FMAXNMP,
    ARM64_SIMD_OP_FMINNMP,
    ARM64_SIMD_OP_FABD,
    ARM64_SIMD_OP_FRECPS,
    ARM64_SIMD_OP_FRSQRTS,
    ARM64_SIMD_OP_FCMP,
    ARM64_SIMD_OP_FCMPE,
    ARM64_SIMD_OP_FCCMP,
    ARM64_SIMD_OP_FCCMPE,
    ARM64_SIMD_OP_FCSEL,
    ARM64_SIMD_OP_FCMEQ,
    ARM64_SIMD_OP_FCMGE,
    ARM64_SIMD_OP_FCMGT,
    ARM64_SIMD_OP_FCMLE,
    ARM64_SIMD_OP_FCMLT,
    ARM64_SIMD_OP_FACGE,
    ARM64_SIMD_OP_FACGT,
    ARM64_SIMD_OP_FAMAX,
    ARM64_SIMD_OP_FAMIN,
    ARM64_SIMD_OP_FSCALE,
    ARM64_SIMD_OP_FCMLA,
    ARM64_SIMD_OP_FCADD,
    ARM64_SIMD_OP_FMAXNMV,
    ARM64_SIMD_OP_FMINNMV,
    ARM64_SIMD_OP_FMAXV,
    ARM64_SIMD_OP_FMINV,
    ARM64_SIMD_OP_FMOV_GENERAL_TO_FP,
    ARM64_SIMD_OP_FMOV_FP_TO_GENERAL,
    ARM64_SIMD_OP_EXT,
    ARM64_SIMD_OP_MOVI,
    ARM64_SIMD_OP_MVNI,
    ARM64_SIMD_OP_ORR,
    ARM64_SIMD_OP_BIC,
    ARM64_SIMD_OP_DUP_GENERAL,
    ARM64_SIMD_OP_DUP_ELEMENT,
    ARM64_SIMD_OP_INS_GENERAL,
    ARM64_SIMD_OP_INS_ELEMENT,
    ARM64_SIMD_OP_UMOV,
    ARM64_SIMD_OP_SMOV,
    ARM64_SIMD_OP_UZP1,
    ARM64_SIMD_OP_TRN1,
    ARM64_SIMD_OP_ZIP1,
    ARM64_SIMD_OP_UZP2,
    ARM64_SIMD_OP_TRN2,
    ARM64_SIMD_OP_ZIP2,
    ARM64_SIMD_OP_AND,
    ARM64_SIMD_OP_ORN,
    ARM64_SIMD_OP_EOR,
    ARM64_SIMD_OP_BSL,
    ARM64_SIMD_OP_BIT,
    ARM64_SIMD_OP_BIF,
    ARM64_SIMD_OP_REV64,
    ARM64_SIMD_OP_REV32,
    ARM64_SIMD_OP_REV16,
    ARM64_SIMD_OP_ADD,
    ARM64_SIMD_OP_SUB,
    ARM64_SIMD_OP_CMEQ,
    ARM64_SIMD_OP_CMGT,
    ARM64_SIMD_OP_CMGE,
    ARM64_SIMD_OP_CMHI,
    ARM64_SIMD_OP_CMHS,
    ARM64_SIMD_OP_SHADD,
    ARM64_SIMD_OP_SQADD,
    ARM64_SIMD_OP_SRHADD,
    ARM64_SIMD_OP_SHSUB,
    ARM64_SIMD_OP_SQSUB,
    ARM64_SIMD_OP_SSHL,
    ARM64_SIMD_OP_SQSHL,
    ARM64_SIMD_OP_SRSHL,
    ARM64_SIMD_OP_SQRSHL,
    ARM64_SIMD_OP_SMAX,
    ARM64_SIMD_OP_SMIN,
    ARM64_SIMD_OP_SABD,
    ARM64_SIMD_OP_SABA,
    ARM64_SIMD_OP_CMTST,
    ARM64_SIMD_OP_MLA,
    ARM64_SIMD_OP_MUL,
    ARM64_SIMD_OP_SMAXP,
    ARM64_SIMD_OP_SMINP,
    ARM64_SIMD_OP_SQDMULH,
    ARM64_SIMD_OP_ADDP,
    ARM64_SIMD_OP_UHADD,
    ARM64_SIMD_OP_UQADD,
    ARM64_SIMD_OP_URHADD,
    ARM64_SIMD_OP_UHSUB,
    ARM64_SIMD_OP_UQSUB,
    ARM64_SIMD_OP_USHL,
    ARM64_SIMD_OP_UQSHL,
    ARM64_SIMD_OP_URSHL,
    ARM64_SIMD_OP_UQRSHL,
    ARM64_SIMD_OP_UMAX,
    ARM64_SIMD_OP_UMIN,
    ARM64_SIMD_OP_UABD,
    ARM64_SIMD_OP_UABA,
    ARM64_SIMD_OP_MLS,
    ARM64_SIMD_OP_PMUL,
    ARM64_SIMD_OP_UMAXP,
    ARM64_SIMD_OP_UMINP,
    ARM64_SIMD_OP_SQRDMULH,
    ARM64_SIMD_OP_SQRDMLAH,
    ARM64_SIMD_OP_SQRDMLSH,
    ARM64_SIMD_OP_SDOT,
    ARM64_SIMD_OP_UDOT,
    ARM64_SIMD_OP_USDOT,
    ARM64_SIMD_OP_SUDOT,
    ARM64_SIMD_OP_BFDOT,
    ARM64_SIMD_OP_BFMLAL,
    ARM64_SIMD_OP_BFMMLA,
    ARM64_SIMD_OP_SMMLA,
    ARM64_SIMD_OP_UMMLA,
    ARM64_SIMD_OP_USMMLA,
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

    ARM64_SIMD_OP_ADDV,
    ARM64_SIMD_OP_SADDLV,
    ARM64_SIMD_OP_UADDLV,
    ARM64_SIMD_OP_SMAXV,
    ARM64_SIMD_OP_SMINV,
    ARM64_SIMD_OP_UMAXV,
    ARM64_SIMD_OP_UMINV,
    ARM64_SIMD_OP_XTN,
    ARM64_SIMD_OP_SQXTN,
    ARM64_SIMD_OP_UQXTN,
    ARM64_SIMD_OP_SQXTUN,
    ARM64_SIMD_OP_COUNT,
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
    ARM64_FP_ROUND_COUNT,
};

enum arm64_simd_rotation
{
    ARM64_SIMD_ROTATION_0,
    ARM64_SIMD_ROTATION_90,
    ARM64_SIMD_ROTATION_180,
    ARM64_SIMD_ROTATION_270,
    ARM64_SIMD_ROTATION_COUNT,
};

enum arm64_simd_compare_operand
{
    ARM64_SIMD_COMPARE_NONE,     // 不使用浮点比较操作数选择
    ARM64_SIMD_COMPARE_REGISTER, // 与第二个寄存器比较
    ARM64_SIMD_COMPARE_ZERO,     // 与浮点零比较
    ARM64_SIMD_COMPARE_COUNT,
};

enum arm64_simd_half
{
    ARM64_SIMD_HALF_LOW,  // 使用或写入低半部分
    ARM64_SIMD_HALF_HIGH, // 使用或写入高半部分
    ARM64_SIMD_HALF_COUNT,
};

enum arm64_simd_element_group
{
    ARM64_SIMD_ELEMENTS_EVEN, // 使用偶数编号元素
    ARM64_SIMD_ELEMENTS_ODD,  // 使用奇数编号元素
    ARM64_SIMD_ELEMENT_GROUP_COUNT,
};

struct arm64_branch_operands
{
    // 直接分支的 PC 字节偏移。
    int64_t offset;
    uint8_t condition;
    uint8_t test_bit;
};

struct arm64_pc_relative_operands
{
    // ADR 为 PC 字节偏移，ADRP 为相对当前 PC 页基址的字节偏移。
    int64_t offset;
};

struct arm64_system_operands
{
    enum arm64_system_operation operation;
    // exception immediate 为 imm16；barrier/CLREX 使用低 4 位 option，BTI 使用 arm64_bti_type。
    uint16_t immediate;
    uint8_t option;
    uint8_t op0;
    uint8_t op1;
    uint8_t crn;
    uint8_t crm;
    uint8_t op2;
};

struct arm64_data_operands
{
    // immediate、wmask 和 tmask 已按照架构定义展开为完整值。
    uint64_t immediate;
    uint64_t wmask;
    uint64_t tmask;
    uint8_t shift_type;
    uint8_t shift_amount;
    uint8_t option;
    uint8_t immr;
    uint8_t condition;
    uint8_t nzcv;
    enum arm64_operand_source operand_source;
};

struct arm64_load_store_operands
{
    // offset 始终是字节偏移，access_bytes 是单个元素的访问字节数。
    int64_t offset;
    enum arm64_address_mode address_mode;
    enum arm64_memory_access access;
    enum arm64_memory_register_kind register_kind;
    enum arm64_memory_order memory_order;
    enum arm64_memory_extension extension;
    enum arm64_memory_transfer transfer;
    uint8_t access_bytes;
    uint8_t prefetch_operation; // PRFM 的 5 位 prfop 编码操作数；非预取指令为 0
    uint8_t extend_type;
    uint8_t shift_amount;
};

struct arm64_simd_operands
{
    enum arm64_simd_form form;
    enum arm64_simd_operation operation;
    enum arm64_fp_rounding_mode rounding_mode;
    enum arm64_simd_compare_operand compare_operand;
    enum arm64_simd_half source_half;
    enum arm64_simd_half destination_half;
    enum arm64_simd_element_group source_elements;
    uint64_t expanded_immediate;
    uint8_t condition;
    uint8_t immediate;
    uint8_t element_width;
    uint8_t result_element_width;
    uint8_t lane_index;
    uint8_t source_lane_index;
};

struct arm64_decoded_insn
{
    enum arm64_decode_status status;  // 解码状态，决定其他字段是否包含可用语义
    enum arm64_insn_class insn_class; // 属于哪一个顶层指令大类
    enum arm64_opcode opcode;         // 所属架构编码族及其操作数布局
    enum arm64_operation operation;   // 这条指令最终执行什么具体整数、位运算或原子操作

    // 以下字段对应 A64 编码图中的寄存器指定字段，保存 5 位编号；有效字段、寄存器组及编号 31 的架构含义由 opcode/form 决定。
    uint8_t rd;            // A64 编码中的 Rd 字段
    uint8_t rn;            // A64 编码中的 Rn 字段
    uint8_t rm;            // A64 编码中的 Rm 字段
    uint8_t ra;            // A64 编码中的 Ra 字段
    uint8_t rt;            // A64 编码中的 Rt 字段
    uint8_t rt2;           // A64 编码中的 Rt2 字段
    uint8_t rs;            // A64 编码中的 Rs 字段
    uint8_t operand_width; // 由指令架构语义确定的主操作数位宽或有效向量位宽，单位为位

    union
    {
        struct arm64_branch_operands branch;           // 分支偏移、条件码和测试位
        struct arm64_pc_relative_operands pc_relative; // ADR/ADRP 的 PC 相对字节偏移
        struct arm64_system_operands system;           // 系统、Hint、屏障和异常指令操作数
        struct arm64_data_operands data;               // 整数数据处理的立即数、掩码、移位和条件字段
        struct arm64_load_store_operands load_store;   // 访存偏移、寻址模式、访问宽度和寄存器扩展
        struct arm64_simd_operands simd;               // FP/AdvSIMD 的操作形态、具体操作和元素信息
    } operands;                                        // 由 insn_class 和 opcode 决定有效成员
};

/*
解码一条 AArch64 指令。

调用方提供最终结果存储地址，decoder 直接填充该对象，避免大结构体按值返回时
产生隐藏返回槽以及调用端的结构体复制。

输出结构始终包含 status。status 为 OK 时可消费完整语义；为
UNSUPPORTED 时只保证已填写的 class/opcode 等识别信息有效；其他失败状态下调用方不应执行该指令。
参数0:待解码的一条 32 位 A64 指令机器码
参数1:解码结果
*/
void arm64_decode_insn(uint32_t raw, struct arm64_decoded_insn *decoded);

#endif
