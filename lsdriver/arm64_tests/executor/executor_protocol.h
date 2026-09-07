#ifndef ARM64_EXECUTOR_PROTOCOL_H
#define ARM64_EXECUTOR_PROTOCOL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define ARM64_EXECUTOR_MEMORY_SIZE 4096U
#define ARM64_EXECUTOR_PROTOCOL_VERSION 1U
#define ARM64_EXECUTOR_CODE_ADDRESS 0x50000000ULL
#define ARM64_EXECUTOR_DATA_ADDRESS 0x60000000ULL
#define ARM64_EXECUTOR_STACK_ADDRESS 0x70000000ULL
#define ARM64_EXECUTOR_CODE_OFFSET 2048U
#define ARM64_EXECUTOR_MAPPING_GUARD 0x00100000ULL
#define ARM64_EXECUTOR_MAPPING_SIZE 0x00200000ULL
#define ARM64_EXECUTOR_CODE_MAPPING_BASE (ARM64_EXECUTOR_CODE_ADDRESS - ARM64_EXECUTOR_MAPPING_GUARD)
#define ARM64_EXECUTOR_DATA_MAPPING_BASE (ARM64_EXECUTOR_DATA_ADDRESS - ARM64_EXECUTOR_MAPPING_GUARD)

struct arm64_executor_arch_state
{
    __u64 regs[31];
    __u64 sp;
    __u64 pc;
    __u64 pstate;
    __u8 q[32][16];
    __u32 fpcr;
    __u32 fpsr;
};

struct arm64_executor_exception
{
    __u64 esr;
    __u64 far;
    __u64 pc;
    __u32 signal;
    __u32 code;
};

struct arm64_executor_case
{
    __u32 version;
    __u32 index;
    __u32 raw;
    __u32 reserved;
    struct arm64_executor_arch_state initial;
    __u8 memory[ARM64_EXECUTOR_MEMORY_SIZE];
};

struct arm64_executor_result
{
    __u32 version;
    __u32 index;
    __u32 raw;
    __u32 status;
    __u32 cpu_event;
    __u32 decode_status;
    __u32 instruction_class;
    __u32 instruction;
    __u32 mismatch_kind;
    __u32 mismatch_index;
    __u32 mismatch_bit;
    __u32 reserved;
    __u64 expected_value;
    __u64 actual_value;
    __u64 memory_offset;
    __u8 expected_byte;
    __u8 actual_byte;
    __u8 padding[6];
    struct arm64_executor_exception exception;
    struct arm64_executor_arch_state cpu_state;
    __u8 memory[ARM64_EXECUTOR_MEMORY_SIZE];
};

enum arm64_executor_status
{
    ARM64_EXECUTOR_STATUS_INVALID = 0,
    ARM64_EXECUTOR_STATUS_PREPARED,
    ARM64_EXECUTOR_STATUS_PASS,
    ARM64_EXECUTOR_STATUS_FAIL,
    ARM64_EXECUTOR_STATUS_CPU_EXCEPTION,
    ARM64_EXECUTOR_STATUS_EXECUTOR_SKIP,
    ARM64_EXECUTOR_STATUS_DECODE_FAIL,
};

#define ARM64_EXECUTOR_CPU_STEP_COMPLETE 0x43505553U
#define ARM64_EXECUTOR_CPU_EXCEPTION 0x43505558U

enum arm64_executor_mismatch_kind
{
    ARM64_EXECUTOR_MISMATCH_NONE = 0,
    ARM64_EXECUTOR_MISMATCH_GPR,
    ARM64_EXECUTOR_MISMATCH_SP,
    ARM64_EXECUTOR_MISMATCH_PC,
    ARM64_EXECUTOR_MISMATCH_PSTATE,
    ARM64_EXECUTOR_MISMATCH_Q,
    ARM64_EXECUTOR_MISMATCH_FPCR,
    ARM64_EXECUTOR_MISMATCH_FPSR,
    ARM64_EXECUTOR_MISMATCH_MEMORY,
    ARM64_EXECUTOR_MISMATCH_EXCEPTION,
};

#define ARM64_EXECUTOR_IOC_MAGIC 0xE7
#define ARM64_EXECUTOR_PREPARE _IOWR(ARM64_EXECUTOR_IOC_MAGIC, 0x01, struct arm64_executor_case)
#define ARM64_EXECUTOR_COMPLETE _IOWR(ARM64_EXECUTOR_IOC_MAGIC, 0x02, struct arm64_executor_result)
#define ARM64_EXECUTOR_RESET _IO(ARM64_EXECUTOR_IOC_MAGIC, 0x03)

#endif
