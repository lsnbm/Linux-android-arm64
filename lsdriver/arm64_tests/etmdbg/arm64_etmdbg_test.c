#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/smp.h>
#include <linux/uaccess.h>

#include "../../arm64_etmdbg.h"
#include "../../klog/lsdriver_log.h"

#define ARM64_ETMDBG_TEST_BUFFER_ORDER 1
#define ARM64_ETMDBG_TEST_MAGIC 0x45544d54U
#define ARM64_ETMDBG_TEST_VERSION 1U

struct arm64_etmdbg_test_header
{
    __u32 magic;
    __u32 version;
    __s32 status;
    __u32 cpu;
    __u64 trace_start;
    __u64 trace_stop;
    __u64 trace_size;
    __u64 trace_status;
    __u64 trace_write;
    __u64 exception_pc;
    __u64 body_result;
};

static struct arm64_etmdbg_cpu_state arm64_etmdbg_test_state;
static struct arm64_etmdbg_test_header arm64_etmdbg_test_header;

struct arm64_etmdbg_test_run
{
    unsigned int cpu;
    u64 trace_start;
    u64 trace_stop;
    int status;
};

static noinline __u64 arm64_etmdbg_test_trace_body(__u64 value)
{
    asm volatile("nop\n\t"
                 "nop\n\t"
                 "nop\n\t"
                 "nop\n\t"
                 : "+r"(value)
                 :
                 : "cc", "memory");
    value ^= 0x9e3779b97f4a7c15ULL;
    value = value * 0x100000001b3ULL + 0x123456789ULL;
    return value ^ (value >> 29);
}

static ssize_t arm64_etmdbg_test_read(struct file *file, char __user *buffer,
                                      size_t count, loff_t *position)
{
    size_t trace_size = arm64_etmdbg_trace_size(&arm64_etmdbg_test_state);
    size_t total_size = sizeof(arm64_etmdbg_test_header) + trace_size;
    size_t offset;
    size_t available;
    size_t copied;

    (void)file;
    if (*position < 0 || (u64)*position >= total_size) return 0;
    offset = (size_t)*position;
    available = total_size - offset;
    if (count > available) count = available;
    if (offset < sizeof(arm64_etmdbg_test_header))
    {
        size_t header_count = min(count, sizeof(arm64_etmdbg_test_header) - offset);

        if (copy_to_user(buffer, (const char *)&arm64_etmdbg_test_header + offset, header_count))
            return -EFAULT;
        copied = header_count;
        if (copied == count)
        {
            *position += copied;
            return copied;
        }
        if (copy_to_user(buffer + copied, arm64_etmdbg_test_state.buffer,
                         count - copied))
            return -EFAULT;
        copied = count;
    }
    else
    {
        if (copy_to_user(buffer, (const char *)arm64_etmdbg_test_state.buffer +
                         offset - sizeof(arm64_etmdbg_test_header), count))
            return -EFAULT;
        copied = count;
    }
    *position += copied;
    return copied;
}

static const struct file_operations arm64_etmdbg_test_fops = {
    .owner = THIS_MODULE,
    .read = arm64_etmdbg_test_read,
};

static struct miscdevice arm64_etmdbg_test_device = {
    .minor = MISC_DYNAMIC_MINOR,
    .name = "arm64_etmdbg_test",
    .fops = &arm64_etmdbg_test_fops,
    .mode = 0600,
};

static void arm64_etmdbg_test_run_on_cpu(void *argument)
{
    struct arm64_etmdbg_test_run *run = argument;

    run->trace_start = (u64)(unsigned long)arm64_etmdbg_test_trace_body;
    run->trace_stop = run->trace_start + 0x100;
    if (!arm64_reg_supports_sysreg_trace() ||
        !arm64_reg_has_etm_sysreg_control() ||
        !arm64_reg_supports_trbe() ||
        !arm64_reg_has_etm_source())
    {
        arm64_etmdbg_log_start_failure("capability_probe", -EOPNOTSUPP,
                                       run->trace_start, run->trace_stop);
        run->status = -EOPNOTSUPP;
        return;
    }

    run->status = arm64_etmdbg_start(&arm64_etmdbg_test_state,
                                     run->trace_start, run->trace_stop,
                                     false, true);
    if (!run->status)
        arm64_etmdbg_test_header.body_result =
            arm64_etmdbg_test_trace_body(0x13579bdf2468ace0ULL);
    arm64_etmdbg_stop(&arm64_etmdbg_test_state, NULL);
    if (!run->status &&
        (!arm64_etmdbg_trace_size(&arm64_etmdbg_test_state) ||
         (arm64_etmdbg_test_state.last_status & ARM64_TRACE_TRBSR_ABORT)))
        run->status = -EIO;
}

static int __init arm64_etmdbg_test_init(void)
{
    unsigned int cpu;
    struct arm64_etmdbg_test_run run = {0};
    int status;

    status = arm64_etmdbg_alloc_buffer(&arm64_etmdbg_test_state,
                                       ARM64_ETMDBG_TEST_BUFFER_ORDER);
    if (status) return status;

    status = -EOPNOTSUPP;
    for_each_online_cpu(cpu)
    {
        run.cpu = cpu;
        run.status = -EOPNOTSUPP;
        if (smp_call_function_single(cpu, arm64_etmdbg_test_run_on_cpu, &run, 1))
            continue;
        if (run.status == -EOPNOTSUPP) continue;
        status = run.status;
        if (!status) break;
    }

    if (status)
    {
        arm64_etmdbg_free_buffer(&arm64_etmdbg_test_state,
                                 ARM64_ETMDBG_TEST_BUFFER_ORDER);
        ls_log_always_tag("test", "ETMDBG_TEST_START_FAILED status=%d\n", status);
        return status;
    }

    arm64_etmdbg_test_header.magic = ARM64_ETMDBG_TEST_MAGIC;
    arm64_etmdbg_test_header.version = ARM64_ETMDBG_TEST_VERSION;
    arm64_etmdbg_test_header.status = 0;
    arm64_etmdbg_test_header.cpu = run.cpu;
    arm64_etmdbg_test_header.trace_start = run.trace_start;
    arm64_etmdbg_test_header.trace_stop = run.trace_stop;
    arm64_etmdbg_test_header.trace_size = arm64_etmdbg_trace_size(&arm64_etmdbg_test_state);
    arm64_etmdbg_test_header.trace_status = arm64_etmdbg_test_state.last_status;
    arm64_etmdbg_test_header.trace_write = arm64_etmdbg_test_state.last_write;
    arm64_etmdbg_test_header.exception_pc = arm64_etmdbg_test_state.last_exception_pc;
    if (!arm64_etmdbg_test_header.trace_size ||
        (arm64_etmdbg_test_header.trace_status & ARM64_TRACE_TRBSR_ABORT))
    {
        ls_log_always_tag("test", "ETMDBG_TEST_TRACE_FAILED size=%llu status=0x%llx\n",
                          arm64_etmdbg_test_header.trace_size,
                          arm64_etmdbg_test_header.trace_status);
        arm64_etmdbg_free_buffer(&arm64_etmdbg_test_state,
                                 ARM64_ETMDBG_TEST_BUFFER_ORDER);
        return -EIO;
    }

    status = misc_register(&arm64_etmdbg_test_device);
    if (status)
    {
        arm64_etmdbg_free_buffer(&arm64_etmdbg_test_state,
                                 ARM64_ETMDBG_TEST_BUFFER_ORDER);
        return status;
    }
    return 0;
}

static void __exit arm64_etmdbg_test_exit(void)
{
    misc_deregister(&arm64_etmdbg_test_device);
    arm64_etmdbg_free_buffer(&arm64_etmdbg_test_state,
                             ARM64_ETMDBG_TEST_BUFFER_ORDER);
}

module_init(arm64_etmdbg_test_init);
module_exit(arm64_etmdbg_test_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Direct ETM and TRBE instruction trace test");