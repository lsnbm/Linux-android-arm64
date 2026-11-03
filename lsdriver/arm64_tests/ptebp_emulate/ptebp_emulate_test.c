#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/proc_fs.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/uaccess.h>
#include <asm/neon.h>

#include "../../klog/lsdriver_log.h"
#include "../../inline_hook_frame.h"
#include "../../break_point.h"
#include "ptebp_emulate_protocol.h"

#define PTEBP_EMULATE_DEVICE_NAME "ptebp_emulate_test"

struct ptebp_emulate_pending
{
    struct ptebp_emulate_completion completion;
    void *code_shadow;
    void *data_shadow;
};

static DEFINE_MUTEX(ptebp_emulate_lock);
static struct ptebp_emulate_pending *ptebp_emulate_pending;
static struct proc_dir_entry *ptebp_emulate_proc_entry;

static void ptebp_emulate_clear_slots(void)
{
    for (size_t index = 0; index < ARRAY_SIZE(g_ptebp_current.slots); index++)
        g_ptebp_current.slots[index] = (struct ptebp_slot){};
}

static void ptebp_emulate_release_pending(void)
{
    if (!ptebp_emulate_pending)
        return;

    kfree(ptebp_emulate_pending->code_shadow);
    kfree(ptebp_emulate_pending->data_shadow);
    kfree(ptebp_emulate_pending);
    ptebp_emulate_pending = NULL;
    ptebp_emulate_clear_slots();
}

static int ptebp_emulate_prepare(const struct ptebp_emulate_case *request)
{
    struct pt_regs regs = {};
    struct fp_regs fp_regs = {};
    struct ptebp_emulate_pending *pending;
    bool emulated;

    if (ptebp_emulate_pending)
        return -EBUSY;

    pending = kzalloc(sizeof(*pending), GFP_KERNEL);
    if (!pending)
        return -ENOMEM;
    pending->code_shadow = kmalloc(PAGE_SIZE, GFP_KERNEL);
    pending->data_shadow = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!pending->code_shadow || !pending->data_shadow)
    {
        kfree(pending->code_shadow);
        kfree(pending->data_shadow);
        kfree(pending);
        return -ENOMEM;
    }

    memcpy(pending->code_shadow, request->memory, PAGE_SIZE);
    memcpy(pending->data_shadow, request->memory, PAGE_SIZE);
    ptebp_emulate_clear_slots();
    g_ptebp_current.slots[0].page_vaddr = PTEBP_EMULATE_CODE_ADDRESS & PAGE_MASK;
    g_ptebp_current.slots[0].shadow_page = pending->code_shadow;
    g_ptebp_current.slots[1].page_vaddr = PTEBP_EMULATE_DATA_ADDRESS & PAGE_MASK;
    g_ptebp_current.slots[1].shadow_page = pending->data_shadow;

    memcpy(regs.regs, request->initial.regs, sizeof(regs.regs));
    regs.sp = request->initial.sp;
    regs.pc = request->initial.pc;
    regs.pstate = request->initial.pstate;
    memcpy(fp_regs.q, request->initial.q, sizeof(fp_regs.q));
    fp_regs.fpcr = request->initial.fpcr;
    fp_regs.fpsr = request->initial.fpsr;

    kernel_neon_begin();
    emulated = ptebp_emulate_load_store(&regs, &fp_regs, request->raw);
    kernel_neon_end();

    pending->completion.version = PTEBP_EMULATE_PROTOCOL_VERSION;
    pending->completion.index = request->index;
    pending->completion.raw = request->raw;
    pending->completion.emulated = emulated ? 1U : 0U;
    pending->completion.status = emulated ? 0U : 1U;
    memcpy(pending->completion.emulator_state.regs, regs.regs,
           sizeof(pending->completion.emulator_state.regs));
    pending->completion.emulator_state.sp = regs.sp;
    pending->completion.emulator_state.pc = regs.pc;
    pending->completion.emulator_state.pstate = regs.pstate;
    memcpy(pending->completion.emulator_state.q, fp_regs.q,
           sizeof(pending->completion.emulator_state.q));
    pending->completion.emulator_state.fpcr = fp_regs.fpcr;
    pending->completion.emulator_state.fpsr = fp_regs.fpsr;
    memcpy(pending->completion.emulator_memory, request->memory,
           sizeof(pending->completion.emulator_memory));
    ptebp_emulate_pending = pending;
    return 0;
}

static long ptebp_emulate_ioctl(struct file *file, unsigned int command,
                                unsigned long argument)
{
    struct ptebp_emulate_case *request = NULL;
    struct ptebp_emulate_completion *completion = NULL;
    int status;

    (void)file;
    if (_IOC_TYPE(command) != PTEBP_EMULATE_IOC_MAGIC)
        return -ENOTTY;

    mutex_lock(&ptebp_emulate_lock);
    if (command == PTEBP_EMULATE_PREPARE)
    {
        request = kzalloc(sizeof(*request), GFP_KERNEL);
        if (!request)
        {
            status = -ENOMEM;
            goto out;
        }
        if (copy_from_user(request, (void __user *)argument, sizeof(*request)))
        {
            status = -EFAULT;
            goto out;
        }
        if (request->version != PTEBP_EMULATE_PROTOCOL_VERSION ||
            request->reserved != 0U)
        {
            status = -EINVAL;
            goto out;
        }
        status = ptebp_emulate_prepare(request);
        goto out;
    }
    if (command == PTEBP_EMULATE_COMPLETE)
    {
        completion = kzalloc(sizeof(*completion), GFP_KERNEL);
        if (!completion)
        {
            status = -ENOMEM;
            goto out;
        }
        if (!ptebp_emulate_pending)
        {
            status = -EINVAL;
            goto out;
        }
        if (copy_from_user(completion, (void __user *)argument,
                           sizeof(*completion)))
        {
            status = -EFAULT;
            goto out;
        }
        if (completion->version != PTEBP_EMULATE_PROTOCOL_VERSION ||
            completion->index != ptebp_emulate_pending->completion.index ||
            completion->raw != ptebp_emulate_pending->completion.raw ||
            completion->reserved != 0U)
        {
            status = -EINVAL;
            goto out;
        }
        *completion = ptebp_emulate_pending->completion;
        status = copy_to_user((void __user *)argument, completion,
                              sizeof(*completion)) ? -EFAULT : 0;
        ptebp_emulate_release_pending();
        goto out;
    }
    if (command == PTEBP_EMULATE_RESET)
    {
        ptebp_emulate_release_pending();
        status = 0;
        goto out;
    }
    status = -ENOTTY;
out:
    kfree(request);
    kfree(completion);
    mutex_unlock(&ptebp_emulate_lock);
    return status;
}

static const struct file_operations ptebp_emulate_fops = {
    .owner = THIS_MODULE,
    .unlocked_ioctl = ptebp_emulate_ioctl,
};

static const struct proc_ops ptebp_emulate_proc_ops = {
    .proc_ioctl = ptebp_emulate_ioctl,
};

static struct miscdevice ptebp_emulate_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = PTEBP_EMULATE_DEVICE_NAME,
    .fops = &ptebp_emulate_fops,
    .mode = 0600,
};

static int __init ptebp_emulate_init(void)
{
    int status = misc_register(&ptebp_emulate_device);

    if (status)
        return status;
    ptebp_emulate_proc_entry = proc_create(PTEBP_EMULATE_DEVICE_NAME, 0600,
                                           NULL, &ptebp_emulate_proc_ops);
    if (!ptebp_emulate_proc_entry)
    {
        misc_deregister(&ptebp_emulate_device);
        return -ENOMEM;
    }
    ls_log_always_tag("test", "PTEBP load/store emulator test ready\n");
    return 0;
}

static void __exit ptebp_emulate_exit(void)
{
    mutex_lock(&ptebp_emulate_lock);
    ptebp_emulate_release_pending();
    mutex_unlock(&ptebp_emulate_lock);
    if (ptebp_emulate_proc_entry)
        proc_remove(ptebp_emulate_proc_entry);
    misc_deregister(&ptebp_emulate_device);
}

module_init(ptebp_emulate_init);
module_exit(ptebp_emulate_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PTEBP load/store emulator oracle test");
