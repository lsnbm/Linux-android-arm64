#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <setjmp.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <sys/ptrace.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "executor_protocol.h"

#define NT_PRSTATUS 1
#define NT_FPREGSET 2
#define SYS_IOCTL 29
#define SYS_EXIT 93
#define BRK_INSTRUCTION 0xd4200000U
#define MAX_INSTRUCTIONS 10000U
#define PSTATE_SS (1U << 21)

__asm__(
    ".section .tdata,\"awT\",%progbits\n"
    ".balign 64\n"
    ".global runner_tls_alignment_anchor\n"
    ".hidden runner_tls_alignment_anchor\n"
    ".type runner_tls_alignment_anchor,%object\n"
    "runner_tls_alignment_anchor:\n"
    ".zero 1\n"
    ".size runner_tls_alignment_anchor,1\n"
    ".previous\n");

typedef struct
{
    uint64_t regs[31];
    uint64_t sp;
    uint64_t pc;
    uint64_t pstate;
} runner_regs;

typedef struct
{
    uint8_t vregs[32][16];
    uint32_t fpsr;
    uint32_t fpcr;
} runner_fp_regs;

struct prepare_shared
{
    volatile long status;
    struct arm64_executor_case request;
};

static long raw_syscall3(long number, long arg0, long arg1, long arg2)
{
    register long x0 asm("x0") = arg0;
    register long x1 asm("x1") = arg1;
    register long x2 asm("x2") = arg2;
    register long x8 asm("x8") = number;

    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory", "cc");
    return x0;
}

static __attribute__((noreturn, noinline, no_stack_protector)) void raw_exit(int status)
{
    register long x0 asm("x0") = status;
    register long x8 asm("x8") = SYS_EXIT;

    asm volatile("svc #0" : : "r"(x0), "r"(x8) : "memory", "cc");
    __builtin_unreachable();
}

static int prepared_case_is_valid(const struct arm64_executor_case *request,
                                  const struct arm64_executor_case *expected)
{
    return request->version == ARM64_EXECUTOR_PROTOCOL_VERSION &&
           request->index == expected->index &&
           request->raw == expected->raw &&
           request->initial.pc == ARM64_EXECUTOR_CODE_ADDRESS +
                                  ARM64_EXECUTOR_CODE_OFFSET;
}

static int prepare_case_isolated(int device, struct arm64_executor_case *test_case)
{
    struct prepare_shared *shared;
    pid_t child;
    int wait_status;
    long status;

    shared = mmap(NULL, sizeof(*shared), PROT_READ | PROT_WRITE,
                  MAP_SHARED | MAP_ANONYMOUS, -1, 0);
    if (shared == MAP_FAILED)
        return -1;
    shared->status = -1;
    shared->request = *test_case;

    child = fork();
    if (child < 0)
    {
        munmap(shared, sizeof(*shared));
        return -1;
    }
    if (child == 0)
    {
        void *code_page;
        void *data_page;

        code_page = mmap((void *)ARM64_EXECUTOR_CODE_MAPPING_BASE, ARM64_EXECUTOR_MAPPING_SIZE,
                         PROT_READ | PROT_WRITE | PROT_EXEC,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                         -1, 0);
        data_page = mmap((void *)ARM64_EXECUTOR_DATA_MAPPING_BASE, ARM64_EXECUTOR_MAPPING_SIZE,
                         PROT_READ | PROT_WRITE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                         -1, 0);
        if (code_page == MAP_FAILED || data_page == MAP_FAILED)
            raw_exit(120);
        status = raw_syscall3(SYS_IOCTL, device, ARM64_EXECUTOR_PREPARE,
                              (long)&shared->request);
        shared->status = status;
        raw_exit(status < 0 ? 1 : 0);
    }

    if (waitpid(child, &wait_status, 0) != child)
    {
        munmap(shared, sizeof(*shared));
        return -1;
    }
    if (prepared_case_is_valid(&shared->request, test_case))
    {
        *test_case = shared->request;
        munmap(shared, sizeof(*shared));
        return 0;
    }
    status = shared->status;
    if (status < 0 && status >= -4095)
        errno = (int)-status;
    else if (WIFSIGNALED(wait_status))
        errno = EINTR;
    else
        errno = EIO;
    munmap(shared, sizeof(*shared));
    return -1;
}

static int read_instructions(const char *path, uint32_t **values, size_t *count)
{
    FILE *file = fopen(path, "r");
    uint32_t *buffer = calloc(MAX_INSTRUCTIONS, sizeof(*buffer));
    char line[64];
    size_t used = 0;

    if (!file || !buffer)
        return -1;
    while (fgets(line, sizeof(line), file))
    {
        char *end;
        unsigned long value;
        if (!line[0] || line[0] == '\n' || line[0] == '\r')
            continue;
        value = strtoul(line, &end, 16);
        if (end == line || value > UINT32_MAX || used == MAX_INSTRUCTIONS)
        {
            fclose(file);
            free(buffer);
            return -1;
        }
        buffer[used++] = (uint32_t)value;
    }
    fclose(file);
    *values = buffer;
    *count = used;
    return 0;
}

static void *map_fixed(uintptr_t address, size_t size, int protection)
{
    void *mapped = mmap((void *)address, size, protection,
                        MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    return mapped == MAP_FAILED ? NULL : mapped;
}

static int ptrace_regs(pid_t child, int request, unsigned long note,
                       void *data, size_t size)
{
    struct iovec iov = { .iov_base = data, .iov_len = size };
    return ptrace(request, child, (void *)note, &iov);
}

static volatile sig_atomic_t runner_current_index;
static sigjmp_buf runner_recovery;

static void runner_fatal_signal(int signal_number)
{
    static const char prefix[] = "runner: signal=";
    static const char middle[] = " index=";
    static const char suffix[] = "\n";
    char digits[12];
    unsigned int value;
    unsigned int position;

    write(STDERR_FILENO, prefix, sizeof(prefix) - 1U);
    value = (unsigned int)signal_number;
    position = sizeof(digits);
    do
    {
        digits[--position] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    write(STDERR_FILENO, digits + position, sizeof(digits) - position);
    write(STDERR_FILENO, middle, sizeof(middle) - 1U);
    value = (unsigned int)runner_current_index;
    position = sizeof(digits);
    do
    {
        digits[--position] = (char)('0' + value % 10U);
        value /= 10U;
    } while (value != 0U);
    write(STDERR_FILENO, digits + position, sizeof(digits) - position);
    write(STDERR_FILENO, suffix, sizeof(suffix) - 1U);
    siglongjmp(runner_recovery, 1);
}

static void install_runner_signal_handlers(void)
{
    struct sigaction action;

    memset(&action, 0, sizeof(action));
    action.sa_handler = runner_fatal_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = 0;
    sigaction(SIGSEGV, &action, NULL);
    sigaction(SIGBUS, &action, NULL);
    sigaction(SIGILL, &action, NULL);
    sigaction(SIGABRT, &action, NULL);
}

static int run_one_cpu_case(const struct arm64_executor_case *test_case,
                            struct arm64_executor_result *result)
{
    pid_t child;
    int wait_status;
    runner_regs regs;
    runner_fp_regs fp_regs;
    void *code;
    void *data;
    void *stack;
    const char *failure_phase = "start";

    data = mmap((void *)ARM64_EXECUTOR_DATA_MAPPING_BASE, ARM64_EXECUTOR_MAPPING_SIZE,
                PROT_READ | PROT_WRITE,
                MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (data == MAP_FAILED)
    {
        fprintf(stderr, "runner: index=%u phase=data-mmap errno=%d\n",
                test_case->index, errno);
        return -1;
    }
        memcpy((uint8_t *)data + ARM64_EXECUTOR_MAPPING_GUARD,
            test_case->memory, ARM64_EXECUTOR_MEMORY_SIZE);

    child = fork();
    if (child < 0)
    {
        munmap(data, ARM64_EXECUTOR_MAPPING_SIZE);
        return -1;
    }
    if (child == 0)
    {
        uint32_t *instruction;
        code = map_fixed(ARM64_EXECUTOR_CODE_MAPPING_BASE, ARM64_EXECUTOR_MAPPING_SIZE,
                 PROT_READ | PROT_WRITE | PROT_EXEC);
        stack = map_fixed(ARM64_EXECUTOR_STACK_ADDRESS, 4096, PROT_READ | PROT_WRITE);
        if (!code || !stack)
            _exit(120);
         memcpy((uint8_t *)code + ARM64_EXECUTOR_MAPPING_GUARD,
             test_case->memory, ARM64_EXECUTOR_MEMORY_SIZE);
         instruction = (uint32_t *)((uint8_t *)code + ARM64_EXECUTOR_MAPPING_GUARD +
                        ARM64_EXECUTOR_CODE_OFFSET);
        instruction[0] = test_case->raw;
        instruction[1] = BRK_INSTRUCTION;
        __builtin___clear_cache((char *)code, (char *)code + 8);
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0)
            _exit(121);
        raise(SIGSTOP);
        _exit(122);
    }
    failure_phase = "initial-wait";
    if (waitpid(child, &wait_status, 0) != child || !WIFSTOPPED(wait_status))
        goto fail;
    failure_phase = "set-options";
    if (ptrace(PTRACE_SETOPTIONS, child, 0, PTRACE_O_EXITKILL) < 0)
        goto fail;
    memset(&regs, 0, sizeof(regs));
    memcpy(regs.regs, test_case->initial.regs, sizeof(regs.regs));
    regs.sp = test_case->initial.sp;
    regs.pc = ARM64_EXECUTOR_CODE_ADDRESS + ARM64_EXECUTOR_CODE_OFFSET;
    regs.pstate = test_case->initial.pstate;
    failure_phase = "set-gpr-regset";
    if (ptrace_regs(child, PTRACE_SETREGSET, NT_PRSTATUS, &regs, sizeof(regs)) < 0)
        goto fail;
    memset(&fp_regs, 0, sizeof(fp_regs));
    memcpy(fp_regs.vregs, test_case->initial.q, sizeof(fp_regs.vregs));
    fp_regs.fpcr = test_case->initial.fpcr;
    fp_regs.fpsr = test_case->initial.fpsr;
    failure_phase = "set-fp-regset";
    if (ptrace_regs(child, PTRACE_SETREGSET, NT_FPREGSET, &fp_regs, sizeof(fp_regs)) < 0)
        goto fail;
    failure_phase = "single-step";
    if (ptrace(PTRACE_SINGLESTEP, child, NULL, NULL) < 0)
        goto fail;
    failure_phase = "step-wait";
    if (waitpid(child, &wait_status, 0) != child)
        goto fail;
    if (WIFSTOPPED(wait_status) && WSTOPSIG(wait_status) == SIGTRAP)
    {
        memset(&regs, 0, sizeof(regs));
        memset(&fp_regs, 0, sizeof(fp_regs));
        failure_phase = "get-gpr-regset";
        if (ptrace_regs(child, PTRACE_GETREGSET, NT_PRSTATUS, &regs, sizeof(regs)) < 0)
            goto fail;
        failure_phase = "get-fp-regset";
        if (ptrace_regs(child, PTRACE_GETREGSET, NT_FPREGSET, &fp_regs, sizeof(fp_regs)) < 0)
            goto fail;
        result->cpu_event = ARM64_EXECUTOR_CPU_STEP_COMPLETE;
        memcpy(result->cpu_state.regs, regs.regs, sizeof(regs.regs));
        result->cpu_state.sp = regs.sp;
        result->cpu_state.pc = regs.pc;
        result->cpu_state.pstate = regs.pstate & ~((uint64_t)PSTATE_SS);
        memcpy(result->cpu_state.q, fp_regs.vregs, sizeof(result->cpu_state.q));
        result->cpu_state.fpcr = fp_regs.fpcr;
        result->cpu_state.fpsr = fp_regs.fpsr;
         memcpy(result->memory, (uint8_t *)data + ARM64_EXECUTOR_MAPPING_GUARD,
             sizeof(result->memory));
        ptrace(PTRACE_KILL, child, NULL, NULL);
        waitpid(child, NULL, 0);
        munmap(data, ARM64_EXECUTOR_MAPPING_SIZE);
        return 0;
    }
    result->cpu_event = ARM64_EXECUTOR_CPU_EXCEPTION;
    result->exception.signal = WIFSTOPPED(wait_status) ? WSTOPSIG(wait_status) :
                               WIFSIGNALED(wait_status) ? WTERMSIG(wait_status) : 0;
    result->exception.code = WIFEXITED(wait_status) ? WEXITSTATUS(wait_status) :
                             WIFSIGNALED(wait_status) ? WCOREDUMP(wait_status) : 0;
    if (WIFSTOPPED(wait_status))
    {
        ptrace(PTRACE_KILL, child, NULL, NULL);
        waitpid(child, NULL, 0);
    }
    munmap(data, ARM64_EXECUTOR_MAPPING_SIZE);
    return 0;
fail:
    fprintf(stderr, "runner: index=%u phase=%s errno=%d wait_status=0x%x\n",
            test_case->index, failure_phase, errno, wait_status);
    if (WIFSTOPPED(wait_status))
    {
        ptrace(PTRACE_KILL, child, NULL, NULL);
        waitpid(child, NULL, 0);
    }
    munmap(data, ARM64_EXECUTOR_MAPPING_SIZE);
    return -1;
}

int main(int argc, char **argv)
{
    const char *instruction_path = argc > 1 ? argv[1] : "lsdriver/arm64_tests/instruction.txt";
    const char *device_path = argc > 2 ? argv[2] : "/dev/arm64_executor_test";
    uint32_t *instructions;
    size_t count;
    int device;
    size_t index;

    if (read_instructions(instruction_path, &instructions, &count) < 0)
        return 1;
    install_runner_signal_handlers();
    device = open(device_path, O_RDWR | O_CLOEXEC);
    if (device < 0)
    {
        perror(device_path);
        free(instructions);
        return 1;
    }
    for (index = 0; index < count; index++)
    {
        struct arm64_executor_case test_case = {
            .version = ARM64_EXECUTOR_PROTOCOL_VERSION,
            .index = index,
            .raw = instructions[index],
        };
        struct arm64_executor_result result = {
            .version = ARM64_EXECUTOR_PROTOCOL_VERSION,
            .index = index,
            .raw = instructions[index],
        };
        runner_current_index = (sig_atomic_t)index;
        if (sigsetjmp(runner_recovery, 1) != 0)
        {
            ioctl(device, ARM64_EXECUTOR_RESET);
            printf("index=%zu raw=0x%08x result=CPU_RUNNER_FAIL\n",
                   index, instructions[index]);
            continue;
        }
        if (prepare_case_isolated(device, &test_case) < 0)
        {
            int prepare_errno = errno;

            ioctl(device, ARM64_EXECUTOR_RESET);
            errno = prepare_errno;
            printf("index=%zu raw=0x%08x result=EXECUTOR_SKIP errno=%d\n",
                   index, instructions[index], errno);
            continue;
        }
        if (run_one_cpu_case(&test_case, &result) < 0)
        {
            fprintf(stderr, "index=%zu raw=0x%08x result=CPU_RUNNER_FAIL\n",
                    index, instructions[index]);
            ioctl(device, ARM64_EXECUTOR_RESET);
            close(device);
            free(instructions);
            return 1;
        }
        if (ioctl(device, ARM64_EXECUTOR_COMPLETE, &result) < 0)
        {
            perror("ARM64_EXECUTOR_COMPLETE");
            close(device);
            free(instructions);
            return 1;
        }
         printf("index=%zu raw=0x%08x status=%u mismatch_kind=%u mismatch_index=%u mismatch_bit=%u expected=0x%016llx actual=0x%016llx memory_offset=%llu expected_byte=0x%02x actual_byte=0x%02x\n",
               index, instructions[index], result.status, result.mismatch_kind,
             result.mismatch_index, result.mismatch_bit,
             (unsigned long long)result.expected_value,
             (unsigned long long)result.actual_value,
             (unsigned long long)result.memory_offset,
             result.expected_byte, result.actual_byte);
    }
    close(device);
    free(instructions);
    return 0;
}
