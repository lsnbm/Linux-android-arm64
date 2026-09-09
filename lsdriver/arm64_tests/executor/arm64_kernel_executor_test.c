#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <asm/neon.h>

#include "../../lsdriver_log.h"
#include "../../arm64_emulate/emulate_inst.h"
#include "arm64_instruction_table.h"
#include "executor_protocol.h"

#ifndef ARM64_EXECUTOR_BUILD_ID
#error "ARM64_EXECUTOR_BUILD_ID must identify the current executor build inputs"
#endif

#define ARM64_EXECUTOR_BREAK_INSTRUCTION 0xd4200000U

static const char arm64_executor_build_id[] __used =
    "arm64_executor_build_id=" ARM64_EXECUTOR_BUILD_ID;

struct arm64_executor_pending_case
{
    __u32 index;
    __u32 raw;
    struct arm64_executor_arch_state executor_state;
    __u8 executor_memory[ARM64_EXECUTOR_MEMORY_SIZE];
};

static DEFINE_MUTEX(arm64_executor_case_lock);
static struct arm64_executor_pending_case *arm64_executor_pending;

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

static long arm64_executor_ioctl(struct file *file, unsigned int command,
                                 unsigned long argument)
{
    struct arm64_executor_case request;
    struct arm64_executor_completion completion;
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
            request.raw != arm64_test_instructions[request.index] ||
            request.reserved != 0U)
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
        {
            struct arm64_executor_arch_state software_initial;
            struct pt_regs software_regs;
            struct fp_regs software_fp_regs;
            __u64 original_tpidr_el0;

            software_initial = request.initial;
            status = arm64_executor_prepare_user_pages(&request);
            if (status)
            {
                kfree(arm64_executor_pending);
                arm64_executor_pending = NULL;
                goto out;
            }
            arm64_executor_pt_regs_from_state(&software_regs, &software_fp_regs,
                                              &software_initial);
            original_tpidr_el0 = arm64_read_tpidr_el0();
            arm64_write_tpidr_el0(software_initial.tpidr_el0);
            kernel_neon_begin();
            if (!emulate_inst(&software_regs, &software_fp_regs, request.raw))
            {
                kernel_neon_end();
                arm64_write_tpidr_el0(original_tpidr_el0);
                kfree(arm64_executor_pending);
                arm64_executor_pending = NULL;
                status = -EOPNOTSUPP;
                goto out;
            }
            kernel_neon_end();
            software_initial.tpidr_el0 = arm64_read_tpidr_el0();
            arm64_write_tpidr_el0(original_tpidr_el0);
            if (copy_from_user(arm64_executor_pending->executor_memory,
                               (void __user *)ARM64_EXECUTOR_DATA_ADDRESS,
                               ARM64_EXECUTOR_MEMORY_SIZE))
            {
                kfree(arm64_executor_pending);
                arm64_executor_pending = NULL;
                status = -EFAULT;
                goto out;
            }
            arm64_executor_state_from_pt_regs(&arm64_executor_pending->executor_state,
                                              &software_regs, &software_fp_regs);
            arm64_executor_pending->executor_state.tpidr_el0 = software_initial.tpidr_el0;
        }
        arm64_executor_pending->index = request.index;
        arm64_executor_pending->raw = request.raw;
        status = 0;
        goto out;
    }
    if (command == ARM64_EXECUTOR_COMPLETE)
    {
        if (!arm64_executor_pending)
        {
            status = -EINVAL;
            goto out;
        }
        if (copy_from_user(&completion, (void __user *)argument, sizeof(completion)))
        {
            status = -EFAULT;
            goto out;
        }
        if (completion.version != ARM64_EXECUTOR_PROTOCOL_VERSION ||
            completion.index != arm64_executor_pending->index ||
            completion.raw != arm64_executor_pending->raw ||
            completion.reserved != 0U)
        {
            status = -EINVAL;
            goto out;
        }
        completion.executor_state = arm64_executor_pending->executor_state;
        memcpy(completion.executor_memory, arm64_executor_pending->executor_memory,
               sizeof(completion.executor_memory));
        status = copy_to_user((void __user *)argument, &completion,
                              sizeof(completion)) ? -EFAULT : 0;
        kfree(arm64_executor_pending);
        arm64_executor_pending = NULL;
        goto out;
    }
    if (command == ARM64_EXECUTOR_RESET)
    {
        if (arm64_executor_pending)
        {
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
};

static struct miscdevice arm64_executor_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "arm64_executor_test",
    .fops = &arm64_executor_fops,
    .mode = 0600,
};

static int __init arm64_kernel_executor_test_init(void)
{
    int status = misc_register(&arm64_executor_device);

    if (status)
        return status;
    ls_log_always_tag("test", "continuous-state executor oracle ready rows=%u build=%s\n",
                      ARM64_TEST_INSTRUCTION_COUNT, arm64_executor_build_id);
    return 0;
}

static void __exit arm64_kernel_executor_test_exit(void)
{
    mutex_lock(&arm64_executor_case_lock);
    kfree(arm64_executor_pending);
    arm64_executor_pending = NULL;
    mutex_unlock(&arm64_executor_case_lock);
    misc_deregister(&arm64_executor_device);
}

module_init(arm64_kernel_executor_test_init);
module_exit(arm64_kernel_executor_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Continuous-state ARM64 executor oracle protocol");
