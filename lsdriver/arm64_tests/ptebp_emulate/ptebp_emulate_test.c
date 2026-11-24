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
static struct break_point *ptebp_file_info;
static int ptebp_file_hits;
static pteval_t ptebp_file_guard[3];
static int target_tgid;
static unsigned long target_address;
static unsigned long target_hits;

module_param(target_tgid, int, 0400);
module_param(target_address, ulong, 0400);
module_param(target_hits, ulong, 0400);
module_param_named(target_active, g_ptebp_current.valid, int, 0400);

static void ptebp_target_hit(void *regs, void *fp_regs, void *point)
{
    __atomic_add_fetch(&target_hits, 1, __ATOMIC_RELAXED);
}

static int ptebp_target_start(void)
{
    if (target_tgid <= 0 || !target_address || (target_address & 3)) return -EINVAL;

    fn_aarch64_insn_patch_text = (void *)generic_kallsyms_lookup_name("aarch64_insn_patch_text");
    if (!fn_aarch64_insn_patch_text) return -ENOENT;

    ptebp_file_info = kvzalloc(sizeof(*ptebp_file_info), GFP_KERNEL);
    if (!ptebp_file_info) return -ENOMEM;
    ptebp_file_info->tgid = target_tgid;
    ptebp_file_info->points[0].hit_addr = target_address;
    ptebp_file_info->points[0].bt = BP_BREAKPOINT_X;
    ptebp_file_info->points[0].on_hit = ptebp_target_hit;
    int status = start_ptebp_monitor(ptebp_file_info);
    if (status)
    {
        kvfree(ptebp_file_info);
        ptebp_file_info = NULL;
    }
    return status;
}

static void ptebp_file_hit(void *regs, void *fp_regs, void *point)
{
    ((struct pt_regs *)regs)->pc += sizeof(uint32_t);
    ptebp_file_hits++;
}

static int ptebp_file_restore_lock_selftest(void)
{
    struct mm_struct *mm = g_ptebp_current.mm;
    int status = 0;

    // 用真实写锁阻止异常路径取得读锁；这里只调用 handler，不触发持锁睡眠。
    mmap_write_lock(mm);
    if (!ptebp_try_get_ref(PTEBP_REF_DATA_ABORT)) status = -EINVAL;
    else
    {
        int result = ptebp_drop_all_monitors(false, PTEBP_REF_DATA_ABORT);
        ptebp_put_ref(PTEBP_REF_DATA_ABORT);
        if (result != -EAGAIN) status = -EINVAL;
    }

    for (int kernel = 0; !status && kernel < 2; kernel++)
    {
        struct pt_regs regs = {
            .pc = g_ptebp_current.slots[0].hook_addr,
            .pstate = kernel ? PSR_MODE_EL1h : PSR_MODE_EL0t,
        };
        struct pt_regs hook_regs = {};
        hook_regs.regs[0] = g_ptebp_current.slots[0].page_vaddr;
        hook_regs.regs[1] = ((kernel ? ESR_ELx_EC_DABT_CUR : ESR_ELx_EC_DABT_LOW) << ESR_ELx_EC_SHIFT) |
                            ESR_ELx_FSC_PERM | ESR_ELx_FSC_LEVEL | ESR_ELx_WNR;
        hook_regs.regs[2] = (unsigned long)&regs;
        if (ptebp_handle_data_abort(&hook_regs) != 0 ||
            hook_regs.regs[0] != g_ptebp_current.slots[0].page_vaddr ||
            regs.pc != g_ptebp_current.slots[0].hook_addr)
            status = -EINVAL;
    }

    if (!ptebp_context_valid() || g_ptebp_current.info != ptebp_file_info ||
        g_ptebp_current.restoring || g_ptebp_current.data_abort_refs ||
        g_ptebp_current.undef_refs || g_ptebp_current.mprotect_refs ||
        g_ptebp_current.remote_read_refs || g_ptebp_current.page_copy_refs)
        status = -EINVAL;
    for (size_t index = 0; !status && index < ARRAY_SIZE(ptebp_file_guard); index++)
    {
        const struct ptebp_slot *slot = &g_ptebp_current.slots[index];
        pteval_t value;
        if (read_user_pte_value(mm, slot->page_vaddr, &value) ||
            value != ptebp_file_guard[index] ||
            READ_ONCE(*(uint32_t *)phys_to_virt(slot->paddr)) != 0)
            status = -ESTALE;
    }
    mmap_write_unlock(mm);
    if (status) ls_log_always_tag("test", "PTEBP restore lock selftest failed status=%d\n", status);
    return status;
}

static int ptebp_file_start(uint64_t address)
{
    if (ptebp_emulate_pending || ptebp_file_info) return -EBUSY;
    if (address & ~PAGE_MASK) return -EINVAL;

    if (!fn_aarch64_insn_patch_text)
        fn_aarch64_insn_patch_text = (void *)generic_kallsyms_lookup_name("aarch64_insn_patch_text");
    if (!fn_aarch64_insn_patch_text) return -ENOENT;

    ptebp_file_info = kvzalloc(sizeof(*ptebp_file_info), GFP_KERNEL);
    if (!ptebp_file_info) return -ENOMEM;

    ptebp_file_hits = 0;
    ptebp_file_info->tgid = current->tgid;
    const size_t offsets[] = {0, PAGE_SIZE - sizeof(uint32_t), PAGE_SIZE};
    for (size_t index = 0; index < ARRAY_SIZE(offsets); index++)
    {
        ptebp_file_info->points[index].hit_addr = address + offsets[index];
        ptebp_file_info->points[index].bt = BP_BREAKPOINT_X;
        ptebp_file_info->points[index].on_hit = ptebp_file_hit;
    }
    int status = start_ptebp_monitor(ptebp_file_info);
    if (status) return status;
    mmap_read_lock(g_ptebp_current.mm);
    for (size_t index = 0; index < ARRAY_SIZE(ptebp_file_guard); index++)
    {
        status = read_user_pte_value(g_ptebp_current.mm, g_ptebp_current.slots[index].page_vaddr, &ptebp_file_guard[index]);
        if (status) break;
    }
    mmap_read_unlock(g_ptebp_current.mm);
    return status ? status : ptebp_file_restore_lock_selftest();
}

static int ptebp_file_check(void)
{
    if (!ptebp_file_info || !ptebp_context_valid() ||
        g_ptebp_current.info != ptebp_file_info ||
        g_ptebp_current.remote_read_refs) return -EINVAL;

    for (size_t index = 0; index < 3; index++)
    {
        const struct ptebp_slot *slot = &g_ptebp_current.slots[index];
        pteval_t value;
        if (READ_ONCE(*(uint32_t *)phys_to_virt(slot->paddr)) != 0 ||
            read_user_pte_value(g_ptebp_current.mm, slot->page_vaddr, &value) ||
            value != ptebp_file_guard[index] ||
            value != ptebp_make_data_guard_pte(pte_val(slot->orig_pte))) return -ESTALE;
    }
    return ptebp_file_hits;
}

static int ptebp_file_stop(void)
{
    if (!ptebp_file_info) return 0;

    mutex_lock(&g_ptebp_control_lock);
    int status = ptebp_stop_locked();
    mutex_unlock(&g_ptebp_control_lock);
    if (!status)
    {
        kvfree(ptebp_file_info);
        ptebp_file_info = NULL;
    }
    return status;
}

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

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    // 6.18 由调用方提供可抢占内核 SIMD 上下文的保存空间。
    struct user_fpsimd_state kernel_fp_state;
    kernel_neon_begin(&kernel_fp_state);
#else
    kernel_neon_begin();
#endif
    emulated = ptebp_emulate_load_store(&regs, &fp_regs, request->raw);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    kernel_neon_end(&kernel_fp_state);
#else
    kernel_neon_end();
#endif

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
    if (command == PTEBP_EMULATE_FILE_START)
    {
        uint64_t address;
        status = copy_from_user(&address, (void __user *)argument, sizeof(address)) ?
                     -EFAULT : ptebp_file_start(address);
        goto out;
    }
    if (command == PTEBP_EMULATE_FILE_CHECK)
    {
        status = ptebp_file_check();
        goto out;
    }
    if (command == PTEBP_EMULATE_PREPARE)
    {
        if (ptebp_file_info)
        {
            status = -EBUSY;
            goto out;
        }
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
        status = ptebp_file_stop();
        if (!status) ptebp_emulate_release_pending();
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

static int ptebp_emulate_abort_selftest(void)
{
    static const struct
    {
        uint32_t raw;
        uint64_t expected;
    } cases[] = {
        {0x385ff820, 0x80ULL},
        {0x785ff820, 0x8080ULL},
        {0xb85ff820, 0x80808080ULL},
        {0xf85ff820, 0x8080808080808080ULL},
        {0x38dff820, 0xffffff80ULL},
        {0x389ff820, 0xffffffffffffff80ULL},
        {0x78dff820, 0xffff8080ULL},
        {0x789ff820, 0xffffffffffff8080ULL},
        {0xb89ff820, 0xffffffff80808080ULL},
    };
    void *shadow = kmalloc(PAGE_SIZE, GFP_KERNEL);
    if (!shadow) return -ENOMEM;

    memset(shadow, 0x80, PAGE_SIZE);
    *(uint32_t *)shadow = 0xf9400020;
    g_ptebp_current.mm = current->mm;
    g_ptebp_current.slots[0] = (struct ptebp_slot){
        .page_vaddr = PTEBP_EMULATE_DATA_ADDRESS,
        .hook_addr = PTEBP_EMULATE_DATA_ADDRESS,
        .shadow_page = shadow,
    };
    g_ptebp_current.valid = 1;

    struct pt_regs regs = {};
    struct pt_regs hook_regs = {};
    hook_regs.regs[0] = PTEBP_EMULATE_DATA_ADDRESS + 32;
    hook_regs.regs[1] = (ESR_ELx_EC_DABT_CUR << ESR_ELx_EC_SHIFT) | ESR_ELx_FSC_PERM | ESR_ELx_FSC_LEVEL;
    hook_regs.regs[2] = (unsigned long)&regs;
    int status = 0;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    struct user_fpsimd_state kernel_fp_state;
    kernel_neon_begin(&kernel_fp_state);
#else
    kernel_neon_begin();
#endif
    for (size_t index = 0; index < ARRAY_SIZE(cases); index++)
    {
        regs = (struct pt_regs){.pc = (unsigned long)&cases[index].raw, .pstate = PSR_MODE_EL1h};
        regs.regs[1] = PTEBP_EMULATE_DATA_ADDRESS + 33;
        hook_regs.regs[0] = PTEBP_EMULATE_DATA_ADDRESS + 32;
        if (ptebp_handle_data_abort(&hook_regs) != 1 ||
            regs.regs[0] != cases[index].expected ||
            regs.regs[1] != PTEBP_EMULATE_DATA_ADDRESS + 33 ||
            regs.pc != (unsigned long)&cases[index].raw + 4 ||
            g_ptebp_current.data_abort_refs || !g_ptebp_current.valid)
        {
            status = -EINVAL;
            goto out;
        }
    }

    regs = (struct pt_regs){.pc = PTEBP_EMULATE_DATA_ADDRESS, .pstate = PSR_MODE_EL0t};
    regs.regs[1] = PTEBP_EMULATE_DATA_ADDRESS + 32;
    hook_regs.regs[0] = PTEBP_EMULATE_DATA_ADDRESS + 32;
    hook_regs.regs[1] = (ESR_ELx_EC_DABT_LOW << ESR_ELx_EC_SHIFT) | ESR_ELx_FSC_PERM | ESR_ELx_FSC_LEVEL;
    if (ptebp_handle_data_abort(&hook_regs) != 1 ||
        regs.regs[0] != 0x8080808080808080ULL ||
        regs.pc != PTEBP_EMULATE_DATA_ADDRESS + 4 || g_ptebp_current.data_abort_refs)
    {
        status = -EINVAL;
        goto out;
    }

    regs = (struct pt_regs){.pc = (unsigned long)&cases[0].raw, .pstate = PSR_MODE_EL1h};
    hook_regs.regs[0] = PTEBP_EMULATE_DATA_ADDRESS + 32;
    hook_regs.regs[1] = (ESR_ELx_EC_DABT_CUR << ESR_ELx_EC_SHIFT) | ESR_ELx_FSC_PERM | ESR_ELx_FSC_LEVEL | ESR_ELx_FnV;
    if (ptebp_handle_data_abort(&hook_regs) || regs.pc != (unsigned long)&cases[0].raw || g_ptebp_current.data_abort_refs)
    {
        status = -EINVAL;
        goto out;
    }

    regs.pc = ~0ULL & PAGE_MASK;
    hook_regs.regs[1] &= ~ESR_ELx_FnV;
    pagefault_disable();
    int disabled_before = current->pagefault_disabled;
    int handled = ptebp_handle_data_abort(&hook_regs);
    if (handled != 1 || regs.pc != (~0ULL & PAGE_MASK) ||
        current->pagefault_disabled != disabled_before || g_ptebp_current.data_abort_refs)
        status = -EINVAL;
    pagefault_enable();

out:
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 18, 0)
    kernel_neon_end(&kernel_fp_state);
#else
    kernel_neon_end();
#endif
    memset(&g_ptebp_current, 0, sizeof(g_ptebp_current));
    kfree(shadow);
    if (status) ls_log_always_tag("test", "PTEBP abort selftest failed pc=0x%llx x0=0x%llx\n", regs.pc, regs.regs[0]);
    return status;
}

static int __init ptebp_emulate_init(void)
{
    if (target_tgid || target_address) return ptebp_target_start();

    int status = ptebp_emulate_abort_selftest();

    if (status)
        return status;
    status = misc_register(&ptebp_emulate_device);

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
    ptebp_file_stop();
    ptebp_emulate_release_pending();
    mutex_unlock(&ptebp_emulate_lock);
    if (target_tgid) return;
    if (ptebp_emulate_proc_entry)
        proc_remove(ptebp_emulate_proc_entry);
    misc_deregister(&ptebp_emulate_device);
}

module_init(ptebp_emulate_init);
module_exit(ptebp_emulate_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("PTEBP load/store emulator oracle test");
