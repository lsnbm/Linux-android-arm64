#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>

#include "../../lsdriver_log.h"
#include "../../arm64_emulate/emulate_inst.h"
#include "arm64_instruction_table.h"
#include "executor_protocol.h"

#define ARM64_EXECUTOR_BREAK_INSTRUCTION 0xd4200000U
#define ARM64_EXECUTOR_SOFTWARE_ORDER 9U

struct arm64_executor_pending_case
{
    bool prepared;
    struct arm64_executor_case request;
    struct arm64_executor_arch_state executor_state;
    __u8 executor_memory[ARM64_EXECUTOR_MEMORY_SIZE];
    unsigned long software_allocation;
    unsigned long software_page;
};

static void arm64_executor_free_software_memory(struct arm64_executor_pending_case *pending)
{
    if (pending->software_allocation)
        free_pages(pending->software_allocation, ARM64_EXECUTOR_SOFTWARE_ORDER);
    pending->software_allocation = 0;
    pending->software_page = 0;
}

static DEFINE_MUTEX(arm64_executor_case_lock);
static struct arm64_executor_pending_case *arm64_executor_pending;

static void arm64_executor_init_state(struct arm64_executor_arch_state *state,
                                      unsigned int index)
{
    unsigned int reg;
    unsigned int byte;
    __u64 seed = 0x9e3779b97f4a7c15ULL ^
                 (0xd1b54a32d192ed03ULL * (__u64)(index + 1U));

    memset(state, 0, sizeof(*state));
    for (reg = 0; reg < ARRAY_SIZE(state->regs); reg++)
        state->regs[reg] = seed ^ (0x94d049bb133111ebULL * (__u64)(reg + 1U));
    state->sp = ARM64_EXECUTOR_STACK_ADDRESS + ARM64_EXECUTOR_MEMORY_SIZE - 16U;
    state->pc = ARM64_EXECUTOR_CODE_ADDRESS + ARM64_EXECUTOR_CODE_OFFSET;
    state->pstate = PSR_MODE_EL0t | PSR_N_BIT | PSR_C_BIT;
    for (reg = 0; reg < ARRAY_SIZE(state->q); reg++)
        for (byte = 0; byte < sizeof(state->q[reg]); byte++)
            state->q[reg][byte] = (unsigned char)(seed + reg * 37U + byte * 13U);
}

static void arm64_executor_init_memory(__u8 *memory, unsigned int index)
{
    unsigned int byte;

    for (byte = 0; byte < ARM64_EXECUTOR_MEMORY_SIZE; byte++)
        memory[byte] = (unsigned char)(0x5aU ^ index ^ (byte * 29U));
}

static bool arm64_executor_uses_user_memory(const struct arm64_decoded_instruction *decoded)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_STTRB_GPR:
    case ARM64_INST_STTRH_GPR:
    case ARM64_INST_STTR_GPR:
    case ARM64_INST_LDTRB_GPR:
    case ARM64_INST_LDTRH_GPR:
    case ARM64_INST_LDTR_GPR:
    case ARM64_INST_LDTRSB_GPR:
    case ARM64_INST_LDTRSH_GPR:
    case ARM64_INST_LDTRSW_GPR:
    case ARM64_INST_STTR_FP_SIMD:
    case ARM64_INST_LDR_GPR_LITERAL:
    case ARM64_INST_LDRSW_LITERAL:
    case ARM64_INST_LDR_FP_SIMD_LITERAL:
        return true;
    default:
        return false;
    }
}

static void arm64_executor_prepare_branch_target(struct arm64_executor_arch_state *state,
                                                 const struct arm64_decoded_instruction *decoded)
{
    if ((decoded->instruction == ARM64_INST_BR ||
         decoded->instruction == ARM64_INST_BLR ||
         decoded->instruction == ARM64_INST_RET) &&
        decoded->rn < ARRAY_SIZE(state->regs))
        state->regs[decoded->rn] = ARM64_EXECUTOR_CODE_ADDRESS +
                                   ARM64_EXECUTOR_CODE_OFFSET;
}

static int arm64_executor_prepare_user_pages(const struct arm64_executor_case *request)
{
    __u32 breakpoint = ARM64_EXECUTOR_BREAK_INSTRUCTION;

    if (copy_to_user((void __user *)(unsigned long)ARM64_EXECUTOR_CODE_ADDRESS,
                     request->memory, ARM64_EXECUTOR_MEMORY_SIZE))
        return -EFAULT;
    if (copy_to_user((void __user *)(unsigned long)ARM64_EXECUTOR_DATA_ADDRESS,
                     request->memory, ARM64_EXECUTOR_MEMORY_SIZE))
        return -EFAULT;
    if (copy_to_user((void __user *)(unsigned long)(ARM64_EXECUTOR_CODE_ADDRESS +
                                                    ARM64_EXECUTOR_CODE_OFFSET),
                     &request->raw, sizeof(request->raw)))
        return -EFAULT;
    if (copy_to_user((void __user *)(unsigned long)(ARM64_EXECUTOR_CODE_ADDRESS +
                                                    ARM64_EXECUTOR_CODE_OFFSET +
                                                    sizeof(request->raw)),
                     &breakpoint, sizeof(breakpoint)))
        return -EFAULT;
    return 0;
}

static void arm64_executor_prepare_memory_address(struct arm64_executor_arch_state *state,
                                                  const struct arm64_decoded_instruction *decoded,
                                                  unsigned long address)
{
    __s64 base = (__s64)address - decoded->offset;

    if (decoded->rn == 31U)
        state->sp = (__u64)base;
    else
        state->regs[decoded->rn] = (__u64)base;
    if ((decoded->instruction == ARM64_INST_STRB_GPR_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_STRH_GPR_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_STR_GPR_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_LDRB_GPR_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_LDRH_GPR_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_LDR_GPR_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_LDRSB_GPR_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_LDRSH_GPR_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_LDRSW_GPR_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_STR_FP_SIMD_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET ||
         decoded->instruction == ARM64_INST_PRFM_REGISTER_OFFSET) &&
        decoded->rm < ARRAY_SIZE(state->regs))
        state->regs[decoded->rm] = 0;
}

static void arm64_executor_state_from_pt_regs(struct arm64_executor_arch_state *state,
                                              const struct pt_regs *regs,
                                              const struct fp_regs *fp_regs)
{
    unsigned int index;

    for (index = 0; index < ARRAY_SIZE(state->regs); index++)
        state->regs[index] = regs->regs[index];
    state->sp = regs->sp;
    state->pc = regs->pc;
    state->pstate = regs->pstate;
    memcpy(state->q, fp_regs->q, sizeof(state->q));
    state->fpcr = fp_regs->fpcr;
    state->fpsr = fp_regs->fpsr;
}

static void arm64_executor_pt_regs_from_state(struct pt_regs *regs,
                                              struct fp_regs *fp_regs,
                                              const struct arm64_executor_arch_state *state)
{
    unsigned int index;

    memset(regs, 0, sizeof(*regs));
    for (index = 0; index < ARRAY_SIZE(state->regs); index++)
        regs->regs[index] = state->regs[index];
    regs->sp = state->sp;
    regs->pc = state->pc;
    regs->pstate = state->pstate;
    memcpy(fp_regs->q, state->q, sizeof(fp_regs->q));
    fp_regs->fpcr = state->fpcr;
    fp_regs->fpsr = state->fpsr;
}

static unsigned int arm64_executor_first_bit(__u64 expected, __u64 actual)
{
    return (unsigned int)__builtin_ctzll(expected ^ actual);
}

static bool arm64_executor_compare_u64(struct arm64_executor_result *result,
                                       __u32 field_kind, __u32 field_index,
                                       __u64 expected, __u64 actual)
{
    if (expected == actual)
        return true;
    result->mismatch_kind = field_kind;
    result->mismatch_index = field_index;
    result->mismatch_bit = arm64_executor_first_bit(expected, actual);
    result->expected_value = expected;
    result->actual_value = actual;
    return false;
}

static bool arm64_executor_compare_case(const struct arm64_executor_pending_case *pending,
                                        const struct arm64_executor_result *cpu,
                                        struct arm64_executor_result *result)
{
    struct arm64_executor_arch_state expected_state = pending->executor_state;
    const struct arm64_executor_arch_state *expected = &expected_state;
    const struct arm64_executor_arch_state *actual = &cpu->cpu_state;
    unsigned int index;
    unsigned int byte;

    for (index = 0; index < ARRAY_SIZE(expected_state.regs); index++)
        if (expected_state.regs[index] >= pending->software_allocation &&
            expected_state.regs[index] < pending->software_allocation + ARM64_EXECUTOR_MAPPING_SIZE)
            expected_state.regs[index] = ARM64_EXECUTOR_DATA_MAPPING_BASE +
                                         (expected_state.regs[index] - pending->software_allocation);
    if (expected_state.sp >= pending->software_allocation &&
        expected_state.sp < pending->software_allocation + ARM64_EXECUTOR_MAPPING_SIZE)
        expected_state.sp = ARM64_EXECUTOR_DATA_MAPPING_BASE +
                            (expected_state.sp - pending->software_allocation);
    if (expected_state.pc >= pending->software_allocation &&
        expected_state.pc < pending->software_allocation + ARM64_EXECUTOR_MAPPING_SIZE)
        expected_state.pc = ARM64_EXECUTOR_CODE_MAPPING_BASE +
                            (expected_state.pc - pending->software_allocation);

    for (index = 0; index < ARRAY_SIZE(expected->regs); index++)
        if (!arm64_executor_compare_u64(result, ARM64_EXECUTOR_MISMATCH_GPR,
                                        index, expected->regs[index], actual->regs[index]))
            return false;
    if (!arm64_executor_compare_u64(result, ARM64_EXECUTOR_MISMATCH_SP, 0,
                                    expected->sp, actual->sp))
        return false;
    if (!arm64_executor_compare_u64(result, ARM64_EXECUTOR_MISMATCH_PC, 0,
                                    expected->pc, actual->pc))
        return false;
    if (!arm64_executor_compare_u64(result, ARM64_EXECUTOR_MISMATCH_PSTATE, 0,
                                    expected->pstate, actual->pstate))
        return false;
    for (index = 0; index < ARRAY_SIZE(expected->q); index++)
        for (byte = 0; byte < sizeof(expected->q[index]); byte++)
            if (expected->q[index][byte] != actual->q[index][byte])
            {
                result->mismatch_kind = ARM64_EXECUTOR_MISMATCH_Q;
                result->mismatch_index = index;
                result->mismatch_bit = byte * 8U +
                                       __builtin_ctz((unsigned int)(expected->q[index][byte] ^ actual->q[index][byte]));
                result->expected_value = expected->q[index][byte];
                result->actual_value = actual->q[index][byte];
                return false;
            }
    if (!arm64_executor_compare_u64(result, ARM64_EXECUTOR_MISMATCH_FPCR, 0,
                                    expected->fpcr, actual->fpcr))
        return false;
    if (!arm64_executor_compare_u64(result, ARM64_EXECUTOR_MISMATCH_FPSR, 0,
                                    expected->fpsr, actual->fpsr))
        return false;
    for (byte = 0; byte < ARM64_EXECUTOR_MEMORY_SIZE; byte++)
        if (pending->executor_memory[byte] != cpu->memory[byte])
        {
            result->mismatch_kind = ARM64_EXECUTOR_MISMATCH_MEMORY;
            result->memory_offset = byte;
            result->expected_byte = pending->executor_memory[byte];
            result->actual_byte = cpu->memory[byte];
            return false;
        }
    return true;
}

static long arm64_executor_ioctl(struct file *file, unsigned int command,
                                 unsigned long argument)
{
    struct arm64_executor_case request;
    struct arm64_executor_result result;
    struct pt_regs regs;
    struct fp_regs fp_regs;
    int status;

    (void)file;
    if (_IOC_TYPE(command) != ARM64_EXECUTOR_IOC_MAGIC)
        return -ENOTTY;
    mutex_lock(&arm64_executor_case_lock);
    if (command == ARM64_EXECUTOR_PREPARE)
    {
        if (copy_from_user(&request, (void __user *)argument, sizeof(request)))
        {
            status = -EFAULT;
            goto out;
        }
        if (request.version != ARM64_EXECUTOR_PROTOCOL_VERSION ||
            request.index >= ARM64_TEST_INSTRUCTION_COUNT ||
            request.raw != arm64_test_instructions[request.index])
        {
            status = -EINVAL;
            goto out;
        }
        if (arm64_executor_pending)
        {
            status = -EBUSY;
            goto out;
        }
        arm64_executor_pending = kzalloc(sizeof(*arm64_executor_pending), GFP_KERNEL);
        if (!arm64_executor_pending)
        {
            status = -ENOMEM;
            goto out;
        }
        request.raw = arm64_test_instructions[request.index];
        arm64_executor_init_state(&request.initial, request.index);
        arm64_executor_init_memory(request.memory, request.index);
        {
            struct arm64_decoded_instruction decoded;
            struct arm64_executor_arch_state software_initial;
            struct pt_regs software_regs;
            struct fp_regs software_fp_regs;
            unsigned long software_memory_address;
            bool uses_user_memory;

            if (arm64_decode_instruction(request.raw, &decoded) != ARM64_DECODE_OK)
            {
                kfree(arm64_executor_pending);
                arm64_executor_pending = NULL;
                status = -EINVAL;
                goto out;
            }
            uses_user_memory = arm64_executor_uses_user_memory(&decoded);

            arm64_executor_pending->software_allocation =
                __get_free_pages(GFP_KERNEL | __GFP_ZERO, ARM64_EXECUTOR_SOFTWARE_ORDER);
            if (!arm64_executor_pending->software_allocation)
            {
                kfree(arm64_executor_pending);
                arm64_executor_pending = NULL;
                status = -ENOMEM;
                goto out;
            }
            arm64_executor_pending->software_page = arm64_executor_pending->software_allocation +
                                                    ARM64_EXECUTOR_MAPPING_GUARD;
            memcpy((void *)arm64_executor_pending->software_page, request.memory,
                   ARM64_EXECUTOR_MEMORY_SIZE);
            software_initial = request.initial;
            if (uses_user_memory)
            {
                status = arm64_executor_prepare_user_pages(&request);
                if (status)
                {
                    arm64_executor_free_software_memory(arm64_executor_pending);
                    kfree(arm64_executor_pending);
                    arm64_executor_pending = NULL;
                    goto out;
                }
            }
            software_initial.pc = ARM64_EXECUTOR_CODE_ADDRESS +
                                  ARM64_EXECUTOR_CODE_OFFSET;
            arm64_executor_prepare_branch_target(&software_initial, &decoded);
            arm64_executor_prepare_branch_target(&request.initial, &decoded);
            if (decoded.instruction_class == ARM64_INSTRUCTION_CLASS_LOAD_STORE)
            {
                software_memory_address = uses_user_memory ?
                                          ARM64_EXECUTOR_DATA_ADDRESS :
                                          arm64_executor_pending->software_page;
                arm64_executor_prepare_memory_address(&software_initial, &decoded,
                                                      software_memory_address);
                arm64_executor_prepare_memory_address(&request.initial, &decoded,
                                                      ARM64_EXECUTOR_DATA_ADDRESS);
            }
            arm64_executor_pt_regs_from_state(&software_regs, &software_fp_regs,
                                              &software_initial);
            if (!emulate_inst(&software_regs, &software_fp_regs, request.raw))
            {
                arm64_executor_free_software_memory(arm64_executor_pending);
                kfree(arm64_executor_pending);
                arm64_executor_pending = NULL;
                status = -EOPNOTSUPP;
                goto out;
            }
            if (uses_user_memory &&
                copy_from_user((void *)arm64_executor_pending->software_page,
                               (void __user *)ARM64_EXECUTOR_DATA_ADDRESS,
                               ARM64_EXECUTOR_MEMORY_SIZE))
            {
                arm64_executor_free_software_memory(arm64_executor_pending);
                kfree(arm64_executor_pending);
                arm64_executor_pending = NULL;
                status = -EFAULT;
                goto out;
            }
            arm64_executor_state_from_pt_regs(&arm64_executor_pending->executor_state,
                                              &software_regs, &software_fp_regs);
            memcpy(arm64_executor_pending->executor_memory,
                   (void *)arm64_executor_pending->software_page,
                   ARM64_EXECUTOR_MEMORY_SIZE);
        }
        arm64_executor_pending->request = request;
        arm64_executor_pending->prepared = true;
        request.version = ARM64_EXECUTOR_PROTOCOL_VERSION;
        status = copy_to_user((void __user *)argument, &request, sizeof(request)) ? -EFAULT : 0;
        if (status)
        {
            arm64_executor_free_software_memory(arm64_executor_pending);
            kfree(arm64_executor_pending);
            arm64_executor_pending = NULL;
        }
        goto out;
    }
    if (command == ARM64_EXECUTOR_COMPLETE)
    {
        if (!arm64_executor_pending || !arm64_executor_pending->prepared)
        {
            status = -EINVAL;
            goto out;
        }
        if (copy_from_user(&result, (void __user *)argument, sizeof(result)))
        {
            status = -EFAULT;
            goto out;
        }
        if (result.version != ARM64_EXECUTOR_PROTOCOL_VERSION ||
            result.index != arm64_executor_pending->request.index ||
            result.raw != arm64_executor_pending->request.raw ||
            (result.cpu_event != ARM64_EXECUTOR_CPU_STEP_COMPLETE &&
             result.cpu_event != ARM64_EXECUTOR_CPU_EXCEPTION))
        {
            status = -EINVAL;
            goto out;
        }
        result.version = ARM64_EXECUTOR_PROTOCOL_VERSION;
        result.index = arm64_executor_pending->request.index;
        result.raw = arm64_executor_pending->request.raw;
        result.status = ARM64_EXECUTOR_STATUS_FAIL;
        result.decode_status = ARM64_DECODE_OK;
        {
            struct arm64_decoded_instruction decoded;
            arm64_decode_instruction(result.raw, &decoded);
            result.instruction_class = decoded.instruction_class;
            result.instruction = decoded.instruction;
        }
        if (result.cpu_event == ARM64_EXECUTOR_CPU_EXCEPTION)
            result.status = ARM64_EXECUTOR_STATUS_CPU_EXCEPTION;
        else if (arm64_executor_compare_case(arm64_executor_pending, &result, &result))
            result.status = ARM64_EXECUTOR_STATUS_PASS;
        status = copy_to_user((void __user *)argument, &result, sizeof(result)) ? -EFAULT : 0;
        arm64_executor_free_software_memory(arm64_executor_pending);
        kfree(arm64_executor_pending);
        arm64_executor_pending = NULL;
        goto out;
    }
    if (command == ARM64_EXECUTOR_RESET)
    {
        if (arm64_executor_pending)
        {
            arm64_executor_free_software_memory(arm64_executor_pending);
            kfree(arm64_executor_pending);
            arm64_executor_pending = NULL;
        }
        status = 0;
        goto out;
    }
    status = -ENOTTY;
out:
    mutex_unlock(&arm64_executor_case_lock);
    return status;
}

static const struct file_operations arm64_executor_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = arm64_executor_ioctl,
#ifdef CONFIG_COMPAT
    .compat_ioctl = arm64_executor_ioctl,
#endif
};

static struct miscdevice arm64_executor_device = {
    .minor = 240,
    .name = "arm64_executor_test",
    .fops = &arm64_executor_fops,
    .mode = 0600,
};

static int __init arm64_kernel_executor_test_init(void)
{
    int status = misc_register(&arm64_executor_device);

    if (status)
        return status;
    ls_log_always_tag("test", "single-case executor oracle ready rows=%u\n",
                      ARM64_TEST_INSTRUCTION_COUNT);
    return 0;
}

static void __exit arm64_kernel_executor_test_exit(void)
{
    mutex_lock(&arm64_executor_case_lock);
    if (arm64_executor_pending)
        arm64_executor_free_software_memory(arm64_executor_pending);
    kfree(arm64_executor_pending);
    arm64_executor_pending = NULL;
    mutex_unlock(&arm64_executor_case_lock);
    misc_deregister(&arm64_executor_device);
}

module_init(arm64_kernel_executor_test_init);
module_exit(arm64_kernel_executor_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Single-case ARM64 executor oracle protocol");
