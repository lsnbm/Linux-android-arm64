#define _GNU_SOURCE

#include <errno.h>
#include <fcntl.h>
#include <inttypes.h>
#include <linux/perf_event.h>
#include <sched.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

__asm__(
    ".section .tdata,\"awT\",%progbits\n"
    ".balign 64\n"
    ".global perf_etm_tls_alignment_anchor\n"
    ".hidden perf_etm_tls_alignment_anchor\n"
    ".type perf_etm_tls_alignment_anchor,%object\n"
    "perf_etm_tls_alignment_anchor:\n"
    ".zero 1\n"
    ".size perf_etm_tls_alignment_anchor,1\n"
    ".previous\n");

static long perf_event_open(struct perf_event_attr *attribute, pid_t pid,
                            int cpu, int group_fd, unsigned long flags)
{
    return syscall(__NR_perf_event_open, attribute, pid, cpu, group_fd, flags);
}

static int parse_pmu_type(const char *text)
{
    char *end;
    long value;

    errno = 0;
    value = strtol(text, &end, 10);
    if (errno || end == text || *end != '\0' || value < 0 || value > UINT32_MAX)
        return -1;
    return (int)value;
}

static int read_pmu_type(void)
{
    char buffer[32];
    int descriptor;
    ssize_t length;

    descriptor = open("/sys/bus/event_source/devices/cs_etm/type", O_RDONLY | O_CLOEXEC);
    if (descriptor < 0) return -1;
    length = read(descriptor, buffer, sizeof(buffer) - 1);
    close(descriptor);
    if (length <= 0) return -1;
    buffer[length] = '\0';
    return (int)strtol(buffer, NULL, 10);
}

static uint64_t count_nonzero(const unsigned char *buffer, uint64_t length,
                              uint64_t offset, uint64_t capacity)
{
    uint64_t index;
    uint64_t nonzero = 0;

    for (index = 0; index < length; index++)
        if (buffer[(offset + index) % capacity] != 0) nonzero++;
    return nonzero;
}

static int run_on_cpu(int cpu, int pmu_type, size_t page_size)
{
    struct perf_event_attr attribute;
    struct perf_event_mmap_page *metadata;
    unsigned char *mapping;
    unsigned char *auxiliary;
    uint64_t data_pages = 1;
    uint64_t auxiliary_pages = 64;
    uint64_t auxiliary_size = auxiliary_pages * page_size;
    uint64_t head;
    uint64_t tail;
    uint64_t available;
    uint64_t nonzero;
    size_t mapping_size = (size_t)((1 + data_pages + auxiliary_pages) * page_size);
    int descriptor;
    int status;
    volatile uint64_t value = 0;

    memset(&attribute, 0, sizeof(attribute));
    attribute.size = sizeof(attribute);
    attribute.type = (uint32_t)pmu_type;
    attribute.config = 0;
    attribute.disabled = 1;
    attribute.exclude_hv = 1;
    attribute.exclude_idle = 1;
    attribute.wakeup_events = 1;

    descriptor = (int)perf_event_open(&attribute, -1, cpu, -1, 0);
    if (descriptor < 0) return -errno;

    mapping = mmap(NULL, mapping_size, PROT_READ | PROT_WRITE, MAP_SHARED,
                   descriptor, 0);
    if (mapping == MAP_FAILED)
    {
        status = -errno;
        close(descriptor);
        return status;
    }

    metadata = (struct perf_event_mmap_page *)mapping;
    metadata->aux_offset = (1 + data_pages) * page_size;
    metadata->aux_size = auxiliary_size;
    auxiliary = mapping + metadata->aux_offset;
    memset(auxiliary, 0, auxiliary_size);

    status = ioctl(descriptor, PERF_EVENT_IOC_RESET, 0);
    if (!status) status = ioctl(descriptor, PERF_EVENT_IOC_ENABLE, 0);
    if (status)
    {
        status = -errno;
        munmap(mapping, mapping_size);
        close(descriptor);
        return status;
    }

    for (uint64_t index = 0; index < 200000; index++)
        value += (uint64_t)getpid() ^ index;

    status = ioctl(descriptor, PERF_EVENT_IOC_DISABLE, 0);
    if (status)
    {
        status = -errno;
        munmap(mapping, mapping_size);
        close(descriptor);
        return status;
    }

    __sync_synchronize();
    head = metadata->aux_head;
    tail = metadata->aux_tail;
    available = head - tail;
    if (available > auxiliary_size) available = auxiliary_size;
    nonzero = count_nonzero(auxiliary, available, tail, auxiliary_size);
    printf("ETM_PERF_TEST_RESULT cpu=%d aux_head=%" PRIu64
           " aux_tail=%" PRIu64 " bytes=%" PRIu64
           " nonzero=%" PRIu64 " value=%" PRIu64 "\n",
           cpu, head, tail, available, nonzero, value);

    munmap(mapping, mapping_size);
    close(descriptor);
    return available > 0 && nonzero > 0 ? 0 : -ENODATA;
}

int main(int argc, char **argv)
{
    long cpu_count;
    size_t page_size;
    int pmu_type;
    int last_status = -ENODEV;

    pmu_type = argc == 2 ? parse_pmu_type(argv[1]) : read_pmu_type();
    if (pmu_type < 0)
    {
        fprintf(stderr, "ETM_PERF_TEST_NO_CS_ETM usage=%s [pmu_type] errno=%d (%s)\n",
                argv[0], errno, strerror(errno));
        return 1;
    }
    cpu_count = sysconf(_SC_NPROCESSORS_ONLN);
    page_size = (size_t)getpagesize();
    if (cpu_count <= 0 || page_size == 0)
    {
        fprintf(stderr, "ETM_PERF_TEST_SYSTEM_INFO_FAILED errno=%d (%s)\n",
                errno, strerror(errno));
        return 1;
    }

    printf("ETM_PERF_TEST_BEGIN type=%d cpus=%ld page_size=%zu\n",
           pmu_type, cpu_count, page_size);
    for (int cpu = 0; cpu < cpu_count; cpu++)
    {
        int status = run_on_cpu(cpu, pmu_type, page_size);

        if (!status)
        {
            printf("ETM_PERF_TEST_PASS cpu=%d\n", cpu);
            return 0;
        }
        last_status = status;
        printf("ETM_PERF_TEST_CPU_FAILED cpu=%d status=%d errno=%d (%s)\n",
               cpu, status, -status, strerror(-status));
    }
    printf("ETM_PERF_TEST_FAILED status=%d errno=%d (%s)\n",
           last_status, -last_status, strerror(-last_status));
    return 1;
}