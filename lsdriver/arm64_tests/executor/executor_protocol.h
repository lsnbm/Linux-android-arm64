#ifndef ARM64_EXECUTOR_PROTOCOL_H
#define ARM64_EXECUTOR_PROTOCOL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define ARM64_EXECUTOR_MEMORY_SIZE 4096U
#define ARM64_EXECUTOR_PROTOCOL_VERSION 6U
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
    __u64 tpidr_el0;
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

struct arm64_executor_completion
{
    __u32 version;
    __u32 index;
    __u32 raw;
    __u32 reserved;
    struct arm64_executor_arch_state executor_state;
    __u8 executor_memory[ARM64_EXECUTOR_MEMORY_SIZE];
};

#define ARM64_EXECUTOR_IOC_MAGIC 0xE7
#define ARM64_EXECUTOR_PREPARE _IOW(ARM64_EXECUTOR_IOC_MAGIC, 0x01, struct arm64_executor_case)
#define ARM64_EXECUTOR_COMPLETE _IOWR(ARM64_EXECUTOR_IOC_MAGIC, 0x02, struct arm64_executor_completion)
#define ARM64_EXECUTOR_RESET _IO(ARM64_EXECUTOR_IOC_MAGIC, 0x03)

#endif
