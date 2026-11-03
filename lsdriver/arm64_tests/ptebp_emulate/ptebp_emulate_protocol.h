#ifndef PTEBP_EMULATE_PROTOCOL_H
#define PTEBP_EMULATE_PROTOCOL_H

#include <linux/ioctl.h>
#include <linux/types.h>

#define PTEBP_EMULATE_PROTOCOL_VERSION 1U
#define PTEBP_EMULATE_MEMORY_SIZE 4096U
#define PTEBP_EMULATE_CODE_ADDRESS 0x50000000ULL
#define PTEBP_EMULATE_DATA_ADDRESS 0x60000000ULL
#define PTEBP_EMULATE_STACK_ADDRESS 0x70000000ULL
#define PTEBP_EMULATE_CODE_OFFSET 2048U
#define PTEBP_EMULATE_MAPPING_GUARD 0x00100000ULL
#define PTEBP_EMULATE_MAPPING_SIZE 0x00200000ULL
#define PTEBP_EMULATE_CODE_MAPPING_BASE \
    (PTEBP_EMULATE_CODE_ADDRESS - PTEBP_EMULATE_MAPPING_GUARD)
#define PTEBP_EMULATE_DATA_MAPPING_BASE \
    (PTEBP_EMULATE_DATA_ADDRESS - PTEBP_EMULATE_MAPPING_GUARD)

struct ptebp_emulate_arch_state
{
    __u64 regs[31];
    __u64 sp;
    __u64 pc;
    __u64 pstate;
    __u8 q[32][16];
    __u32 fpcr;
    __u32 fpsr;
};

struct ptebp_emulate_case
{
    __u32 version;
    __u32 index;
    __u32 raw;
    __u32 reserved;
    struct ptebp_emulate_arch_state initial;
    __u8 memory[PTEBP_EMULATE_MEMORY_SIZE];
};

struct ptebp_emulate_completion
{
    __u32 version;
    __u32 index;
    __u32 raw;
    __u32 reserved;
    __u32 emulated;
    __u32 status;
    struct ptebp_emulate_arch_state emulator_state;
    __u8 emulator_memory[PTEBP_EMULATE_MEMORY_SIZE];
};

#define PTEBP_EMULATE_IOC_MAGIC 0xE8
#define PTEBP_EMULATE_PREPARE \
    _IOW(PTEBP_EMULATE_IOC_MAGIC, 0x01, struct ptebp_emulate_case)
#define PTEBP_EMULATE_COMPLETE \
    _IOWR(PTEBP_EMULATE_IOC_MAGIC, 0x02, struct ptebp_emulate_completion)
#define PTEBP_EMULATE_RESET _IO(PTEBP_EMULATE_IOC_MAGIC, 0x03)

#endif
