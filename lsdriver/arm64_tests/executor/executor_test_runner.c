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

#include "../../arm64_decode/arm64_decode.h"
#include "executor_protocol.h"

#ifndef ARM64_EXECUTOR_RUNNER_BUILD_ID
#error "ARM64_EXECUTOR_RUNNER_BUILD_ID must identify the current runner inputs"
#endif

#define NT_PRSTATUS 1
#define NT_FPREGSET 2
#define NT_ARM_TLS 0x401
#define SYS_IOCTL 29
#define SYS_EXIT 93
#define BRK_INSTRUCTION 0xd4200000U
#define PSTATE_SS (1U << 21)
#define PSTATE_N (1U << 31)
#define PSTATE_C (1U << 29)

static const char runner_build_id[] __attribute__((used)) =
    "arm64_executor_runner_build_id=" ARM64_EXECUTOR_RUNNER_BUILD_ID;

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

enum runner_status
{
    RUNNER_STATUS_PASS = 2,
    RUNNER_STATUS_FAIL = 3,
    RUNNER_STATUS_CPU_EXCEPTION = 4,
};

enum runner_cpu_event
{
    RUNNER_CPU_STEP_COMPLETE = 0x43505553U,
    RUNNER_CPU_EXCEPTION = 0x43505558U,
};

enum runner_mismatch_kind
{
    RUNNER_MISMATCH_NONE = 0,
    RUNNER_MISMATCH_GPR,
    RUNNER_MISMATCH_SP,
    RUNNER_MISMATCH_PC,
    RUNNER_MISMATCH_PSTATE,
    RUNNER_MISMATCH_Q,
    RUNNER_MISMATCH_FPCR,
    RUNNER_MISMATCH_FPSR,
    RUNNER_MISMATCH_TPIDR_EL0,
    RUNNER_MISMATCH_MEMORY,
};

struct runner_result
{
    uint32_t status;
    uint32_t cpu_event;
    uint32_t mismatch_kind;
    uint32_t mismatch_index;
    uint32_t mismatch_bit;
    uint64_t expected_value;
    uint64_t actual_value;
    uint64_t memory_offset;
    uint8_t expected_byte;
    uint8_t actual_byte;
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

static void initialize_expected_case(struct arm64_executor_case *test_case)
{
    uint64_t seed = 0x9e3779b97f4a7c15ULL ^ 0xd1b54a32d192ed03ULL;
    unsigned int reg;
    unsigned int byte;

    memset(&test_case->initial, 0, sizeof(test_case->initial));
    for (reg = 0; reg < 31U; reg++)
        test_case->initial.regs[reg] =
            seed ^ (0x94d049bb133111ebULL * (uint64_t)(reg + 1U));
    test_case->initial.sp = ARM64_EXECUTOR_STACK_ADDRESS +
                            ARM64_EXECUTOR_MEMORY_SIZE - 16U;
    test_case->initial.pc = ARM64_EXECUTOR_CODE_ADDRESS + ARM64_EXECUTOR_CODE_OFFSET;
    test_case->initial.pstate = PSTATE_N | PSTATE_C;
    test_case->initial.tpidr_el0 = 0x71000000ULL;
    for (reg = 0; reg < 32U; reg++)
        for (byte = 0; byte < 16U; byte++)
            test_case->initial.q[reg][byte] =
                (uint8_t)(seed + reg * 37U + byte * 13U);
    for (byte = 0; byte < ARM64_EXECUTOR_MEMORY_SIZE; byte++)
        test_case->memory[byte] = (uint8_t)(0x5aU ^ (byte * 29U));
}

static int validate_and_print_initial_input_profile(
    const struct arm64_executor_case *test_case)
{
    uint8_t q_values[256] = { 0 };
    uint8_t memory_values[256] = { 0 };
    unsigned int gpr_nonzero = 0;
    unsigned int gpr_distinct = 0;
    unsigned int q_nonzero = 0;
    unsigned int q_distinct = 0;
    unsigned int memory_nonzero = 0;
    unsigned int memory_distinct = 0;
    unsigned int reg;
    unsigned int other;
    unsigned int byte;

    for (reg = 0; reg < 31U; reg++)
    {
        int unique = 1;

        if (test_case->initial.regs[reg] != 0U)
            gpr_nonzero++;
        for (other = 0; other < reg; other++)
            if (test_case->initial.regs[reg] == test_case->initial.regs[other])
            {
                unique = 0;
                break;
            }
        if (unique)
            gpr_distinct++;
    }
    for (reg = 0; reg < 32U; reg++)
        for (byte = 0; byte < 16U; byte++)
        {
            uint8_t value = test_case->initial.q[reg][byte];

            if (value != 0U)
                q_nonzero++;
            q_values[value] = 1U;
        }
    for (byte = 0; byte < ARM64_EXECUTOR_MEMORY_SIZE; byte++)
    {
        uint8_t value = test_case->memory[byte];

        if (value != 0U)
            memory_nonzero++;
        memory_values[value] = 1U;
    }
    for (byte = 0; byte < 256U; byte++)
    {
        q_distinct += q_values[byte];
        memory_distinct += memory_values[byte];
    }
    if (gpr_nonzero != 31U || gpr_distinct != 31U ||
        test_case->initial.sp == 0U || test_case->initial.pc == 0U ||
        test_case->initial.pstate == 0U ||
        test_case->initial.tpidr_el0 == 0U ||
        q_nonzero != 511U || q_distinct != 256U ||
        memory_nonzero != 4080U || memory_distinct != 256U)
        return -1;
    printf("input_profile=gpr_nonzero=%u/31,gpr_distinct=%u,q_nonzero=%u/512,q_distinct_bytes=%u,memory_nonzero=%u/4096,memory_distinct_bytes=%u\n",
           gpr_nonzero, gpr_distinct, q_nonzero, q_distinct,
           memory_nonzero, memory_distinct);
    return 0;
}

static int instruction_is_post_index(const struct arm64_decoded_instruction *decoded)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_STP_GPR_POST_INDEX:
    case ARM64_INST_LDP_GPR_POST_INDEX:
    case ARM64_INST_LDPSW_POST_INDEX:
    case ARM64_INST_LDP_FP_SIMD_POST_INDEX:
    case ARM64_INST_STP_FP_SIMD_POST_INDEX:
    case ARM64_INST_STRB_GPR_POST_INDEX:
    case ARM64_INST_STRH_GPR_POST_INDEX:
    case ARM64_INST_STR_GPR_POST_INDEX:
    case ARM64_INST_STR_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDR_FP_SIMD_POST_INDEX:
    case ARM64_INST_LDRB_GPR_POST_INDEX:
    case ARM64_INST_LDRH_GPR_POST_INDEX:
    case ARM64_INST_LDR_GPR_POST_INDEX:
    case ARM64_INST_LDRSB_GPR_POST_INDEX:
    case ARM64_INST_LDRSH_GPR_POST_INDEX:
    case ARM64_INST_LDRSW_GPR_POST_INDEX:
        return 1;
    default:
        return 0;
    }
}

static int instruction_has_base_register(const struct arm64_decoded_instruction *decoded)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_LDR_GPR_LITERAL:
    case ARM64_INST_LDRSW_LITERAL:
    case ARM64_INST_LDR_FP_SIMD_LITERAL:
    case ARM64_INST_PRFM_LITERAL:
        return 0;
    default:
        return 1;
    }
}

static int instruction_has_register_offset(const struct arm64_decoded_instruction *decoded)
{
    switch (decoded->instruction)
    {
    case ARM64_INST_STRB_GPR_REGISTER_OFFSET:
    case ARM64_INST_STRH_GPR_REGISTER_OFFSET:
    case ARM64_INST_STR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDR_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSB_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSH_GPR_REGISTER_OFFSET:
    case ARM64_INST_LDRSW_GPR_REGISTER_OFFSET:
    case ARM64_INST_STR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_LDR_FP_SIMD_REGISTER_OFFSET:
    case ARM64_INST_PRFM_REGISTER_OFFSET:
        return 1;
    default:
        return 0;
    }
}

static int prepare_common_input(struct arm64_executor_case *test_case)
{
    struct arm64_decoded_instruction decoded;
    int64_t base;

    if (arm64_decode_instruction(test_case->raw, &decoded) != ARM64_DECODE_OK)
        return -1;
    test_case->initial.pc = ARM64_EXECUTOR_CODE_ADDRESS + ARM64_EXECUTOR_CODE_OFFSET;
    if ((decoded.instruction == ARM64_INST_BR ||
         decoded.instruction == ARM64_INST_BLR ||
         decoded.instruction == ARM64_INST_RET) && decoded.rn < 31U)
        test_case->initial.regs[decoded.rn] = test_case->initial.pc;
    if (decoded.instruction_class != ARM64_INSTRUCTION_CLASS_LOAD_STORE ||
        !instruction_has_base_register(&decoded))
        return 0;
    base = instruction_is_post_index(&decoded) ?
           (int64_t)ARM64_EXECUTOR_DATA_ADDRESS :
           (int64_t)ARM64_EXECUTOR_DATA_ADDRESS - decoded.offset;
    if (decoded.rn == 31U)
        test_case->initial.sp = (uint64_t)((base + 15) & ~15LL);
    else
        test_case->initial.regs[decoded.rn] = (uint64_t)base;
    if (instruction_has_register_offset(&decoded) && decoded.rm < 31U)
        test_case->initial.regs[decoded.rm] = 0U;
    return 0;
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

        munmap((void *)ARM64_EXECUTOR_CODE_MAPPING_BASE, ARM64_EXECUTOR_MAPPING_SIZE);
        munmap((void *)ARM64_EXECUTOR_DATA_MAPPING_BASE, ARM64_EXECUTOR_MAPPING_SIZE);
        code_page = mmap((void *)ARM64_EXECUTOR_CODE_MAPPING_BASE, ARM64_EXECUTOR_MAPPING_SIZE,
                         PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                         -1, 0);
        data_page = mmap((void *)ARM64_EXECUTOR_DATA_MAPPING_BASE, ARM64_EXECUTOR_MAPPING_SIZE,
                         PROT_NONE,
                         MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE,
                         -1, 0);
        if (code_page == MAP_FAILED || data_page == MAP_FAILED ||
            mprotect((void *)ARM64_EXECUTOR_CODE_ADDRESS,
                     ARM64_EXECUTOR_MEMORY_SIZE,
                     PROT_READ | PROT_WRITE | PROT_EXEC) < 0 ||
            mprotect((void *)ARM64_EXECUTOR_DATA_ADDRESS,
                     ARM64_EXECUTOR_MEMORY_SIZE,
                     PROT_READ | PROT_WRITE) < 0)
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
    if (shared->status == 0 && WIFEXITED(wait_status) &&
        WEXITSTATUS(wait_status) == 0 &&
        memcmp(&shared->request, test_case, sizeof(*test_case)) == 0)
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

static unsigned int first_different_bit(uint64_t expected, uint64_t actual)
{
    return (unsigned int)__builtin_ctzll(expected ^ actual);
}

static int compare_u64(struct runner_result *result, uint32_t kind,
                       uint32_t index, uint64_t expected, uint64_t actual)
{
    if (expected == actual)
        return 1;
    result->mismatch_kind = kind;
    result->mismatch_index = index;
    result->mismatch_bit = first_different_bit(expected, actual);
    result->expected_value = expected;
    result->actual_value = actual;
    return 0;
}

static int compare_raw_results(struct runner_result *result,
                               const struct arm64_executor_completion *completion,
                               const struct arm64_executor_arch_state *cpu,
                               const uint8_t *cpu_memory)
{
    const struct arm64_executor_arch_state *executor = &completion->executor_state;
    unsigned int index;
    unsigned int byte;

    for (index = 0; index < 31U; index++)
        if (!compare_u64(result, RUNNER_MISMATCH_GPR, index,
                         executor->regs[index], cpu->regs[index]))
            return 0;
    if (!compare_u64(result, RUNNER_MISMATCH_SP, 0,
                     executor->sp, cpu->sp) ||
        !compare_u64(result, RUNNER_MISMATCH_PC, 0,
                     executor->pc, cpu->pc) ||
        !compare_u64(result, RUNNER_MISMATCH_PSTATE, 0,
                     executor->pstate, cpu->pstate))
        return 0;
    for (index = 0; index < 32U; index++)
        for (byte = 0; byte < 16U; byte++)
            if (executor->q[index][byte] != cpu->q[index][byte])
            {
                result->mismatch_kind = RUNNER_MISMATCH_Q;
                result->mismatch_index = index;
                result->mismatch_bit = byte * 8U +
                    (unsigned int)__builtin_ctz((unsigned int)
                        (executor->q[index][byte] ^ cpu->q[index][byte]));
                result->expected_value = executor->q[index][byte];
                result->actual_value = cpu->q[index][byte];
                return 0;
            }
    if (!compare_u64(result, RUNNER_MISMATCH_FPCR, 0,
                     executor->fpcr, cpu->fpcr) ||
        !compare_u64(result, RUNNER_MISMATCH_FPSR, 0,
                     executor->fpsr, cpu->fpsr) ||
        !compare_u64(result, RUNNER_MISMATCH_TPIDR_EL0, 0,
                     executor->tpidr_el0, cpu->tpidr_el0))
        return 0;
    for (byte = 0; byte < ARM64_EXECUTOR_MEMORY_SIZE; byte++)
        if (completion->executor_memory[byte] != cpu_memory[byte])
        {
            result->mismatch_kind = RUNNER_MISMATCH_MEMORY;
            result->memory_offset = byte;
            result->mismatch_bit = (unsigned int)__builtin_ctz((unsigned int)
                (completion->executor_memory[byte] ^ cpu_memory[byte]));
            result->expected_byte = completion->executor_memory[byte];
            result->actual_byte = cpu_memory[byte];
            return 0;
        }
    result->status = RUNNER_STATUS_PASS;
    return 1;
}

static int read_instructions(const char *path, uint32_t **values, size_t *count)
{
    FILE *file = fopen(path, "r");
    uint32_t *buffer = NULL;
    char line[64];
    size_t used = 0;
    size_t capacity = 0;

    if (!file)
        return -1;
    while (fgets(line, sizeof(line), file))
    {
        char *end;
        uint32_t *resized;
        unsigned long value;
        size_t length = strlen(line);

        while (length > 0 && (line[length - 1] == '\n' || line[length - 1] == '\r'))
            line[--length] = '\0';
        if (length == 0)
            continue;
        if (length != 8 || strspn(line, "0123456789abcdefABCDEF") != 8)
            goto invalid;
        value = strtoul(line, &end, 16);
        if (*end != '\0' || value > UINT32_MAX)
            goto invalid;
        if (used == capacity)
        {
            if (capacity > SIZE_MAX / sizeof(*buffer) / 2U)
                goto invalid;
            capacity = capacity == 0 ? 1024U : capacity * 2U;
            resized = realloc(buffer, capacity * sizeof(*buffer));
            if (!resized)
                goto invalid;
            buffer = resized;
        }
        buffer[used++] = (uint32_t)value;
    }
    if (ferror(file) || used == 0)
        goto invalid;
    fclose(file);
    *values = buffer;
    *count = used;
    return 0;

invalid:
    fclose(file);
    free(buffer);
    return -1;
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
    int status = ptrace(request, child, (void *)note, &iov);

    if (status < 0)
        return -1;
    if (iov.iov_len != size)
    {
        errno = EIO;
        return -1;
    }
    return 0;
}

struct cpu_session
{
    pid_t child;
    void *code;
    void *data;
};

static struct cpu_session runner_cpu_session = {
    .child = -1,
};

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

static void cpu_session_stop(struct cpu_session *session)
{
    if (session->child > 0)
    {
        ptrace(PTRACE_KILL, session->child, NULL, NULL);
        waitpid(session->child, NULL, 0);
    }
    if (session->code)
        munmap(session->code, ARM64_EXECUTOR_MAPPING_SIZE);
    if (session->data)
        munmap(session->data, ARM64_EXECUTOR_MAPPING_SIZE);
    session->child = -1;
    session->code = NULL;
    session->data = NULL;
}

static int cpu_session_start(struct cpu_session *session,
                             const struct arm64_executor_case *test_case)
{
    int wait_status = 0;
    const char *failure_phase = "code-mmap";

    session->code = mmap((void *)ARM64_EXECUTOR_CODE_MAPPING_BASE,
                         ARM64_EXECUTOR_MAPPING_SIZE, PROT_NONE,
                         MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (session->code == MAP_FAILED)
    {
        session->code = NULL;
        goto fail;
    }
    failure_phase = "data-mmap";
    if (mprotect((void *)ARM64_EXECUTOR_CODE_ADDRESS,
                 ARM64_EXECUTOR_MEMORY_SIZE,
                 PROT_READ | PROT_WRITE | PROT_EXEC) < 0)
        goto fail;
    failure_phase = "data-mmap";
    session->data = mmap((void *)ARM64_EXECUTOR_DATA_MAPPING_BASE,
                         ARM64_EXECUTOR_MAPPING_SIZE, PROT_NONE,
                         MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (session->data == MAP_FAILED)
    {
        session->data = NULL;
        goto fail;
    }
    if (mprotect((void *)ARM64_EXECUTOR_DATA_ADDRESS,
                 ARM64_EXECUTOR_MEMORY_SIZE,
                 PROT_READ | PROT_WRITE) < 0)
        goto fail;
    memcpy((uint8_t *)session->code + ARM64_EXECUTOR_MAPPING_GUARD,
           test_case->memory, ARM64_EXECUTOR_MEMORY_SIZE);
    memcpy((uint8_t *)session->data + ARM64_EXECUTOR_MAPPING_GUARD,
           test_case->memory, ARM64_EXECUTOR_MEMORY_SIZE);

    session->child = fork();
    if (session->child < 0)
        goto fail;
    if (session->child == 0)
    {
        void *stack = map_fixed(ARM64_EXECUTOR_STACK_ADDRESS, 4096,
                                PROT_READ | PROT_WRITE);

        if (!stack)
            _exit(120);
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0)
            _exit(121);
        raise(SIGSTOP);
        _exit(122);
    }
    failure_phase = "initial-wait";
    if (waitpid(session->child, &wait_status, 0) != session->child ||
        !WIFSTOPPED(wait_status))
        goto fail;
    failure_phase = "set-options";
    if (ptrace(PTRACE_SETOPTIONS, session->child, 0, PTRACE_O_EXITKILL) < 0)
        goto fail;
    return 0;

fail:
    fprintf(stderr, "runner: index=%u phase=%s errno=%d wait_status=0x%x\n",
            test_case->index, failure_phase, errno, wait_status);
    cpu_session_stop(session);
    return -1;
}

static int cpu_session_step(struct cpu_session *session,
                            const struct arm64_executor_case *test_case,
                            const struct arm64_executor_arch_state *previous_state,
                            struct arm64_executor_arch_state *cpu_state,
                            uint8_t *cpu_memory,
                            uint32_t *cpu_event)
{
    runner_regs regs;
    runner_fp_regs fp_regs;
    siginfo_t signal_info;
    uint64_t tpidr_el0;
    int wait_status = 0;
    uint64_t instruction_pair = test_case->raw |
                                ((uint64_t)BRK_INSTRUCTION << 32);
    const char *failure_phase = "memory-chain";

    if (memcmp((uint8_t *)session->data + ARM64_EXECUTOR_MAPPING_GUARD,
               test_case->memory, ARM64_EXECUTOR_MEMORY_SIZE) != 0)
        goto fail;
    memcpy((uint8_t *)session->code + ARM64_EXECUTOR_MAPPING_GUARD,
           test_case->memory, ARM64_EXECUTOR_MEMORY_SIZE);
    failure_phase = "set-instruction";
    if (ptrace(PTRACE_POKETEXT, session->child,
               (void *)(uintptr_t)(ARM64_EXECUTOR_CODE_ADDRESS +
                                   ARM64_EXECUTOR_CODE_OFFSET),
               (void *)(uintptr_t)instruction_pair) < 0)
        goto fail;
    if (previous_state != NULL)
    {
        memset(&regs, 0, sizeof(regs));
        failure_phase = "get-gpr-chain";
        if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_PRSTATUS,
                        &regs, sizeof(regs)) < 0)
            goto fail;
        failure_phase = "gpr-chain";
        if (memcmp(regs.regs, previous_state->regs, sizeof(regs.regs)) != 0 ||
            regs.sp != previous_state->sp ||
            regs.pc != previous_state->pc ||
            (regs.pstate & ~((uint64_t)PSTATE_SS)) != previous_state->pstate)
            goto fail;
        memset(&fp_regs, 0, sizeof(fp_regs));
        failure_phase = "get-fp-chain";
        if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_FPREGSET,
                        &fp_regs, sizeof(fp_regs)) < 0)
            goto fail;
        failure_phase = "fp-chain";
        if (memcmp(fp_regs.vregs, previous_state->q,
                   sizeof(fp_regs.vregs)) != 0 ||
            fp_regs.fpcr != previous_state->fpcr ||
            fp_regs.fpsr != previous_state->fpsr)
            goto fail;
        failure_phase = "get-tls-chain";
        if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_ARM_TLS,
                        &tpidr_el0, sizeof(tpidr_el0)) < 0)
            goto fail;
        failure_phase = "tls-chain";
        if (tpidr_el0 != previous_state->tpidr_el0)
            goto fail;
    }
    memset(&regs, 0, sizeof(regs));
    memcpy(regs.regs, test_case->initial.regs, sizeof(regs.regs));
    regs.sp = test_case->initial.sp;
    regs.pc = ARM64_EXECUTOR_CODE_ADDRESS + ARM64_EXECUTOR_CODE_OFFSET;
    regs.pstate = test_case->initial.pstate;
    failure_phase = "set-gpr-regset";
    if (ptrace_regs(session->child, PTRACE_SETREGSET, NT_PRSTATUS,
                    &regs, sizeof(regs)) < 0)
        goto fail;
    tpidr_el0 = test_case->initial.tpidr_el0;
    failure_phase = "set-tls-regset";
    if (ptrace_regs(session->child, PTRACE_SETREGSET, NT_ARM_TLS,
                    &tpidr_el0, sizeof(tpidr_el0)) < 0)
        goto fail;
    memset(&fp_regs, 0, sizeof(fp_regs));
    memcpy(fp_regs.vregs, test_case->initial.q, sizeof(fp_regs.vregs));
    fp_regs.fpcr = test_case->initial.fpcr;
    fp_regs.fpsr = test_case->initial.fpsr;
    failure_phase = "set-fp-regset";
    if (ptrace_regs(session->child, PTRACE_SETREGSET, NT_FPREGSET,
                    &fp_regs, sizeof(fp_regs)) < 0)
        goto fail;
    memset(&regs, 0, sizeof(regs));
    failure_phase = "verify-gpr-regset";
    if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_PRSTATUS,
                    &regs, sizeof(regs)) < 0 ||
        memcmp(regs.regs, test_case->initial.regs, sizeof(regs.regs)) != 0 ||
        regs.sp != test_case->initial.sp ||
        regs.pc != ARM64_EXECUTOR_CODE_ADDRESS + ARM64_EXECUTOR_CODE_OFFSET ||
        (regs.pstate & ~((uint64_t)PSTATE_SS)) != test_case->initial.pstate)
        goto fail;
    memset(&fp_regs, 0, sizeof(fp_regs));
    failure_phase = "verify-fp-regset";
    if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_FPREGSET,
                    &fp_regs, sizeof(fp_regs)) < 0 ||
        memcmp(fp_regs.vregs, test_case->initial.q, sizeof(fp_regs.vregs)) != 0 ||
        fp_regs.fpcr != test_case->initial.fpcr ||
        fp_regs.fpsr != test_case->initial.fpsr)
        goto fail;
    failure_phase = "verify-tls-regset";
    if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_ARM_TLS,
                    &tpidr_el0, sizeof(tpidr_el0)) < 0 ||
        tpidr_el0 != test_case->initial.tpidr_el0)
        goto fail;
    failure_phase = "single-step";
    if (ptrace(PTRACE_SINGLESTEP, session->child, NULL, NULL) < 0)
        goto fail;
    failure_phase = "step-wait";
    if (waitpid(session->child, &wait_status, 0) != session->child)
        goto fail;
    if (WIFSTOPPED(wait_status) && WSTOPSIG(wait_status) == SIGTRAP)
    {
        memset(&signal_info, 0, sizeof(signal_info));
        failure_phase = "get-siginfo";
        if (ptrace(PTRACE_GETSIGINFO, session->child, NULL, &signal_info) < 0)
            goto fail;
        if (signal_info.si_signo != SIGTRAP || signal_info.si_code != TRAP_TRACE)
        {
            *cpu_event = RUNNER_CPU_EXCEPTION;
            return 0;
        }
        memset(&regs, 0, sizeof(regs));
        memset(&fp_regs, 0, sizeof(fp_regs));
        failure_phase = "get-gpr-regset";
        if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_PRSTATUS,
                        &regs, sizeof(regs)) < 0)
            goto fail;
        failure_phase = "get-fp-regset";
        if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_FPREGSET,
                        &fp_regs, sizeof(fp_regs)) < 0)
            goto fail;
        failure_phase = "get-tls-regset";
        if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_ARM_TLS,
                        &tpidr_el0, sizeof(tpidr_el0)) < 0)
            goto fail;
         *cpu_event = RUNNER_CPU_STEP_COMPLETE;
         memcpy(cpu_state->regs, regs.regs, sizeof(regs.regs));
         cpu_state->sp = regs.sp;
         cpu_state->pc = regs.pc;
         cpu_state->pstate = regs.pstate & ~((uint64_t)PSTATE_SS);
         memcpy(cpu_state->q, fp_regs.vregs, sizeof(cpu_state->q));
         cpu_state->fpcr = fp_regs.fpcr;
         cpu_state->fpsr = fp_regs.fpsr;
         cpu_state->tpidr_el0 = tpidr_el0;
         memcpy(cpu_memory,
               (uint8_t *)session->data + ARM64_EXECUTOR_MAPPING_GUARD,
             ARM64_EXECUTOR_MEMORY_SIZE);
        return 0;
    }
        *cpu_event = RUNNER_CPU_EXCEPTION;
    if (!WIFSTOPPED(wait_status))
        session->child = -1;
    return 0;
fail:
    fprintf(stderr, "runner: index=%u phase=%s errno=%d wait_status=0x%x\n",
            test_case->index, failure_phase, errno, wait_status);
    return -1;
}

int main(int argc, char **argv)
{
    const char *instruction_path;
    const char *device_path;
    uint32_t *instructions;
    size_t count;
    int device;
    size_t index;
    int exit_code = 1;
    struct arm64_executor_arch_state chained_state;
    uint8_t chained_memory[ARM64_EXECUTOR_MEMORY_SIZE];

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <instruction.txt> </dev/arm64_executor_test>\n", argv[0]);
        return 1;
    }
    instruction_path = argv[1];
    device_path = argv[2];
    if (read_instructions(instruction_path, &instructions, &count) < 0)
        return 1;
    printf("protocol_version=%u\n", ARM64_EXECUTOR_PROTOCOL_VERSION);
    printf("runner_build_id=%s\n", runner_build_id);
    printf("input_owner=runner\n");
    printf("verdict_owner=runner\n");
    printf("raw_compare=x0-x30,sp,pc,pstate,q0-q31,fpcr,fpsr,tpidr_el0,memory4096\n");
    printf("instruction_count=%zu\n", count);
    fflush(stdout);
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
            .reserved = 0U,
        };
        struct runner_result result = {
            .status = RUNNER_STATUS_FAIL,
            .mismatch_kind = RUNNER_MISMATCH_NONE,
        };
        struct arm64_executor_completion completion = {
            .version = ARM64_EXECUTOR_PROTOCOL_VERSION,
            .index = index,
            .raw = instructions[index],
        };
        struct arm64_executor_arch_state cpu_state;
        uint8_t cpu_memory[ARM64_EXECUTOR_MEMORY_SIZE];
        uint32_t cpu_event;

        if (index == 0U)
        {
            initialize_expected_case(&test_case);
            if (validate_and_print_initial_input_profile(&test_case) < 0)
            {
                fprintf(stderr, "initial input profile validation failed\n");
                goto out;
            }
        }
        else
        {
            test_case.initial = chained_state;
            memcpy(test_case.memory, chained_memory, sizeof(test_case.memory));
        }
        if (prepare_common_input(&test_case) < 0)
        {
            fprintf(stderr, "index=%zu raw=0x%08x result=INPUT_PREPARE_FAIL\n",
                    index, instructions[index]);
            goto out;
        }
        runner_current_index = (sig_atomic_t)index;
        if (sigsetjmp(runner_recovery, 1) != 0)
        {
            fprintf(stderr, "index=%zu raw=0x%08x result=CPU_RUNNER_FAIL\n",
                    index, instructions[index]);
            goto out;
        }
        if (prepare_case_isolated(device, &test_case) < 0)
        {
            int prepare_errno = errno;

            ioctl(device, ARM64_EXECUTOR_RESET);
            errno = prepare_errno;
            fprintf(stderr, "index=%zu raw=0x%08x result=PREPARE_FAIL errno=%d\n",
                    index, instructions[index], errno);
            goto out;
        }
        if (index == 0U && cpu_session_start(&runner_cpu_session, &test_case) < 0)
            goto out;
        if (cpu_session_step(&runner_cpu_session, &test_case,
                             index == 0U ? NULL : &chained_state,
                             &cpu_state, cpu_memory, &cpu_event) < 0)
        {
            fprintf(stderr, "index=%zu raw=0x%08x result=CPU_RUNNER_FAIL\n",
                    index, instructions[index]);
            goto out;
        }
        if (ioctl(device, ARM64_EXECUTOR_COMPLETE, &completion) < 0)
        {
            perror("ARM64_EXECUTOR_COMPLETE");
            goto out;
        }
        if (completion.version != ARM64_EXECUTOR_PROTOCOL_VERSION ||
            completion.index != index || completion.raw != instructions[index] ||
            completion.reserved != 0U)
        {
            fprintf(stderr, "index=%zu raw=0x%08x result=PROTOCOL_FAIL\n",
                    index, instructions[index]);
            goto out;
        }
        if (cpu_event != RUNNER_CPU_STEP_COMPLETE)
        {
            result.status = RUNNER_STATUS_CPU_EXCEPTION;
            fprintf(stderr, "index=%zu raw=0x%08x result=CPU_EXCEPTION\n",
                    index, instructions[index]);
            goto out;
        }
        compare_raw_results(&result, &completion, &cpu_state, cpu_memory);
         if (result.status == RUNNER_STATUS_PASS)
             printf("index=%zu raw=0x%08x status=%u mismatch=none\n",
                 index, instructions[index], result.status);
         else
             printf("index=%zu raw=0x%08x status=%u mismatch_kind=%u mismatch_index=%u mismatch_bit=%u expected=0x%016llx actual=0x%016llx memory_offset=%llu expected_byte=0x%02x actual_byte=0x%02x\n",
                 index, instructions[index], result.status,
                 result.mismatch_kind, result.mismatch_index,
                 result.mismatch_bit,
                 (unsigned long long)result.expected_value,
                 (unsigned long long)result.actual_value,
                 (unsigned long long)result.memory_offset,
                 result.expected_byte, result.actual_byte);
        fflush(stdout);
        if (result.status != RUNNER_STATUS_PASS)
        {
            fprintf(stderr, "continuous test stopped at index=%zu status=%u\n",
                    index, result.status);
            goto out;
        }
        chained_state = cpu_state;
        memcpy(chained_memory, cpu_memory, sizeof(chained_memory));
    }
    printf("continuous test passed cases=%zu\n", count);
    exit_code = 0;
out:
    cpu_session_stop(&runner_cpu_session);
    ioctl(device, ARM64_EXECUTOR_RESET);
    close(device);
    free(instructions);
    return exit_code;
}
