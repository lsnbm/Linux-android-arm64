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
#include <sys/uio.h>
#include <sys/wait.h>
#include <unistd.h>

#include "ptebp_emulate_protocol.h"

#ifndef PTEBP_EMULATE_RUNNER_BUILD_ID
#error "PTEBP_EMULATE_RUNNER_BUILD_ID must identify the current runner inputs"
#endif

#define NT_PRSTATUS 1
#define NT_FPREGSET 2
#define SYS_IOCTL 29
#define SYS_EXIT 93
#define BRK_INSTRUCTION 0xd4200000U
#define PSTATE_SS (1U << 21)

static const char runner_build_id[] __attribute__((used)) =
    "ptebp_emulate_runner_build_id=" PTEBP_EMULATE_RUNNER_BUILD_ID;

__asm__(
    ".section .tdata,\"awT\",%progbits\n"
    ".balign 64\n"
    ".global ptebp_runner_tls_alignment_anchor\n"
    ".hidden ptebp_runner_tls_alignment_anchor\n"
    ".type ptebp_runner_tls_alignment_anchor,%object\n"
    "ptebp_runner_tls_alignment_anchor:\n"
    ".zero 1\n"
    ".size ptebp_runner_tls_alignment_anchor,1\n"
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
    struct ptebp_emulate_case request;
};

enum runner_status
{
    RUNNER_STATUS_PASS = 2,
    RUNNER_STATUS_FAIL = 3,
    RUNNER_STATUS_CPU_EXCEPTION = 4,
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
    RUNNER_MISMATCH_MEMORY,
};

struct runner_result
{
    uint32_t status;
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

static void initialize_case(struct ptebp_emulate_case *test_case, size_t index,
                            uint32_t raw)
{
    memset(test_case, 0, sizeof(*test_case));
    test_case->version = PTEBP_EMULATE_PROTOCOL_VERSION;
    test_case->index = (uint32_t)index;
    test_case->raw = raw;
    test_case->initial.regs[0] = PTEBP_EMULATE_DATA_ADDRESS + 256U;
    test_case->initial.regs[1] =
        ((raw >> 13U) & 7U) == 6U ? UINT64_C(0xffffffff) : 2U;
    test_case->initial.sp = PTEBP_EMULATE_STACK_ADDRESS + 2048U;
    test_case->initial.pc = PTEBP_EMULATE_CODE_ADDRESS + PTEBP_EMULATE_CODE_OFFSET;
    test_case->initial.pstate = UINT64_C(0x60001000);
    memset(test_case->initial.q, 0xa5, sizeof(test_case->initial.q));
    for (size_t byte = 0; byte < PTEBP_EMULATE_MEMORY_SIZE; byte++)
        test_case->memory[byte] = (uint8_t)(0x5aU ^ (byte * 29U));
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
            capacity = capacity == 0 ? 32U : capacity * 2U;
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

static int ptrace_regs(pid_t child, int request, unsigned long note,
                       void *data, size_t size)
{
    struct iovec iov = { .iov_base = data, .iov_len = size };

    if (ptrace(request, child, (void *)note, &iov) < 0)
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

static struct cpu_session runner_cpu_session = { .child = -1 };
static volatile sig_atomic_t runner_current_index;
static sigjmp_buf runner_recovery;

static void runner_fatal_signal(int signal_number)
{
    dprintf(STDERR_FILENO, "runner: signal=%d index=%d\n",
            signal_number, runner_current_index);
    siglongjmp(runner_recovery, 1);
}

static void cpu_session_stop(struct cpu_session *session)
{
    if (session->child > 0)
    {
        ptrace(PTRACE_KILL, session->child, NULL, NULL);
        waitpid(session->child, NULL, 0);
    }
    if (session->code)
        munmap(session->code, PTEBP_EMULATE_MAPPING_SIZE);
    if (session->data)
        munmap(session->data, PTEBP_EMULATE_MAPPING_SIZE);
    session->child = -1;
    session->code = NULL;
    session->data = NULL;
}

static int cpu_session_start(struct cpu_session *session,
                             const struct ptebp_emulate_case *test_case)
{
    int wait_status;

    session->code = mmap((void *)PTEBP_EMULATE_CODE_MAPPING_BASE,
                         PTEBP_EMULATE_MAPPING_SIZE, PROT_NONE,
                         MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (session->code == MAP_FAILED)
    {
        session->code = NULL;
        goto fail;
    }
    if (mprotect((void *)PTEBP_EMULATE_CODE_ADDRESS,
                 PTEBP_EMULATE_MEMORY_SIZE,
                 PROT_READ | PROT_WRITE | PROT_EXEC) < 0)
        goto fail;
    session->data = mmap((void *)PTEBP_EMULATE_DATA_MAPPING_BASE,
                         PTEBP_EMULATE_MAPPING_SIZE, PROT_NONE,
                         MAP_SHARED | MAP_ANONYMOUS | MAP_FIXED_NOREPLACE, -1, 0);
    if (session->data == MAP_FAILED)
    {
        session->data = NULL;
        goto fail;
    }
    if (mprotect((void *)PTEBP_EMULATE_DATA_ADDRESS,
                 PTEBP_EMULATE_MEMORY_SIZE,
                 PROT_READ | PROT_WRITE) < 0)
        goto fail;
    memcpy((uint8_t *)session->code + PTEBP_EMULATE_MAPPING_GUARD,
           test_case->memory, PTEBP_EMULATE_MEMORY_SIZE);
    memcpy((uint8_t *)session->data + PTEBP_EMULATE_MAPPING_GUARD,
           test_case->memory, PTEBP_EMULATE_MEMORY_SIZE);

    session->child = fork();
    if (session->child < 0)
        goto fail;
    if (session->child == 0)
    {
        if (ptrace(PTRACE_TRACEME, 0, NULL, NULL) < 0)
            raw_exit(121);
        raise(SIGSTOP);
        raw_exit(122);
    }
    if (waitpid(session->child, &wait_status, 0) != session->child ||
        !WIFSTOPPED(wait_status))
        goto fail;
    if (ptrace(PTRACE_SETOPTIONS, session->child, 0, PTRACE_O_EXITKILL) < 0)
        goto fail;
    return 0;

fail:
    fprintf(stderr, "runner: cpu session setup failed index=%u errno=%d\n",
            test_case->index, errno);
    cpu_session_stop(session);
    return -1;
}

static int cpu_session_step(struct cpu_session *session,
                            const struct ptebp_emulate_case *test_case,
                            struct ptebp_emulate_arch_state *cpu_state,
                            uint8_t *cpu_memory)
{
    runner_regs regs = {};
    runner_fp_regs fp_regs = {};
    siginfo_t signal_info = {};
    int wait_status;
    uint64_t instruction_pair = test_case->raw |
                                ((uint64_t)BRK_INSTRUCTION << 32);

    memcpy((uint8_t *)session->data + PTEBP_EMULATE_MAPPING_GUARD,
           test_case->memory, PTEBP_EMULATE_MEMORY_SIZE);
    memcpy((uint8_t *)session->code + PTEBP_EMULATE_MAPPING_GUARD,
           test_case->memory, PTEBP_EMULATE_MEMORY_SIZE);
    if (ptrace(PTRACE_POKETEXT, session->child,
               (void *)(uintptr_t)(PTEBP_EMULATE_CODE_ADDRESS +
                                   PTEBP_EMULATE_CODE_OFFSET),
               (void *)(uintptr_t)instruction_pair) < 0)
        return -1;

    memcpy(regs.regs, test_case->initial.regs, sizeof(regs.regs));
    regs.sp = test_case->initial.sp;
    regs.pc = PTEBP_EMULATE_CODE_ADDRESS + PTEBP_EMULATE_CODE_OFFSET;
    regs.pstate = test_case->initial.pstate;
    if (ptrace_regs(session->child, PTRACE_SETREGSET, NT_PRSTATUS,
                    &regs, sizeof(regs)) < 0)
        return -1;
    memcpy(fp_regs.vregs, test_case->initial.q, sizeof(fp_regs.vregs));
    fp_regs.fpcr = test_case->initial.fpcr;
    fp_regs.fpsr = test_case->initial.fpsr;
    if (ptrace_regs(session->child, PTRACE_SETREGSET, NT_FPREGSET,
                    &fp_regs, sizeof(fp_regs)) < 0)
        return -1;
    if (ptrace(PTRACE_SINGLESTEP, session->child, NULL, NULL) < 0)
        return -1;
    if (waitpid(session->child, &wait_status, 0) != session->child)
        return -1;
    if (!WIFSTOPPED(wait_status) || WSTOPSIG(wait_status) != SIGTRAP)
        return 1;
    if (ptrace(PTRACE_GETSIGINFO, session->child, NULL, &signal_info) < 0)
        return -1;
    if (signal_info.si_code != TRAP_TRACE)
        return 1;
    if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_PRSTATUS,
                    &regs, sizeof(regs)) < 0)
        return -1;
    if (ptrace_regs(session->child, PTRACE_GETREGSET, NT_FPREGSET,
                    &fp_regs, sizeof(fp_regs)) < 0)
        return -1;
    memcpy(cpu_state->regs, regs.regs, sizeof(cpu_state->regs));
    cpu_state->sp = regs.sp;
    cpu_state->pc = regs.pc;
    cpu_state->pstate = regs.pstate & ~((uint64_t)PSTATE_SS);
    memcpy(cpu_state->q, fp_regs.vregs, sizeof(cpu_state->q));
    cpu_state->fpcr = fp_regs.fpcr;
    cpu_state->fpsr = fp_regs.fpsr;
    memcpy(cpu_memory,
           (uint8_t *)session->data + PTEBP_EMULATE_MAPPING_GUARD,
           PTEBP_EMULATE_MEMORY_SIZE);
    return 0;
}

static int prepare_case_isolated(int device,
                                 struct ptebp_emulate_case *test_case)
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
        status = raw_syscall3(SYS_IOCTL, device, PTEBP_EMULATE_PREPARE,
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
    else
        errno = EIO;
    munmap(shared, sizeof(*shared));
    return -1;
}

static int compare_u64(struct runner_result *result, uint32_t kind,
                       uint32_t index, uint64_t expected, uint64_t actual)
{
    if (expected == actual)
        return 1;
    result->mismatch_kind = kind;
    result->mismatch_index = index;
    result->mismatch_bit = (uint32_t)__builtin_ctzll(expected ^ actual);
    result->expected_value = expected;
    result->actual_value = actual;
    return 0;
}

static void compare_results(struct runner_result *result,
                            const struct ptebp_emulate_completion *completion,
                            const struct ptebp_emulate_arch_state *cpu,
                            const uint8_t *cpu_memory)
{
    if (!completion->emulated)
    {
        result->mismatch_kind = RUNNER_MISMATCH_NONE;
        result->expected_value = completion->status;
        return;
    }
    for (uint32_t index = 0; index < 31U; index++)
        if (!compare_u64(result, RUNNER_MISMATCH_GPR, index,
                         completion->emulator_state.regs[index], cpu->regs[index]))
            return;
    if (!compare_u64(result, RUNNER_MISMATCH_SP, 0,
                     completion->emulator_state.sp, cpu->sp) ||
        !compare_u64(result, RUNNER_MISMATCH_PC, 0,
                     completion->emulator_state.pc, cpu->pc) ||
        !compare_u64(result, RUNNER_MISMATCH_PSTATE, 0,
                     completion->emulator_state.pstate, cpu->pstate))
        return;
    for (uint32_t index = 0; index < 32U; index++)
        for (uint32_t byte = 0; byte < 16U; byte++)
            if (completion->emulator_state.q[index][byte] != cpu->q[index][byte])
            {
                result->mismatch_kind = RUNNER_MISMATCH_Q;
                result->mismatch_index = index;
                result->mismatch_bit = byte * 8U +
                    (uint32_t)__builtin_ctz((unsigned int)(
                        completion->emulator_state.q[index][byte] ^
                        cpu->q[index][byte]));
                result->expected_value = completion->emulator_state.q[index][byte];
                result->actual_value = cpu->q[index][byte];
                return;
            }
    if (!compare_u64(result, RUNNER_MISMATCH_FPCR, 0,
                     completion->emulator_state.fpcr, cpu->fpcr) ||
        !compare_u64(result, RUNNER_MISMATCH_FPSR, 0,
                     completion->emulator_state.fpsr, cpu->fpsr))
        return;
    for (uint32_t byte = 0; byte < PTEBP_EMULATE_MEMORY_SIZE; byte++)
        if (completion->emulator_memory[byte] != cpu_memory[byte])
        {
            result->mismatch_kind = RUNNER_MISMATCH_MEMORY;
            result->memory_offset = byte;
            result->mismatch_bit = (uint32_t)__builtin_ctz((unsigned int)(
                completion->emulator_memory[byte] ^ cpu_memory[byte]));
            result->expected_byte = completion->emulator_memory[byte];
            result->actual_byte = cpu_memory[byte];
            return;
        }
    result->status = RUNNER_STATUS_PASS;
}

static int check_file_bytes(int memory_fd, FILE *stream, uintptr_t base,
                            const uint8_t *expected, uint8_t *buffer,
                            size_t offset, size_t length, size_t element_size)
{
    off_t position = (off_t)(base + offset);
    buffer[0] = 0xa5;
    buffer[length + 1] = 0x5a;
    if (stream)
    {
        if (fseek(stream, (long)position, SEEK_SET) ||
            fread(buffer + 1, element_size, length / element_size, stream) != length / element_size ||
            ferror(stream) || ftell(stream) != (long)(position + length)) return -1;
    }
    else if (lseek(memory_fd, position, SEEK_SET) != position ||
             read(memory_fd, buffer + 1, length) != (ssize_t)length ||
             lseek(memory_fd, 0, SEEK_CUR) != position + (off_t)length) return -1;

    if (buffer[0] != 0xa5 || buffer[length + 1] != 0x5a ||
        memcmp(buffer + 1, expected + offset, length))
    {
        fprintf(stderr, "file_read_mismatch api=%s offset=%zu length=%zu\n",
                stream ? "fread" : "read", offset, length);
        return -1;
    }
    return 0;
}

// 同一请求包含普通前后页和中间两页，检查普通页权限确实更新，而非整次请求被跳过。
static int test_protection_ranges(int device, uint8_t *mapping, const uint8_t *expected, size_t page_size)
{
    const int protections[] = {PROT_NONE, PROT_READ | PROT_WRITE | PROT_EXEC, PROT_READ | PROT_EXEC};
    for (size_t test = 0; test < sizeof(protections) / sizeof(protections[0]); test++)
    {
        int prot = protections[test];
        if (mprotect(mapping, 4 * page_size, prot) || ioctl(device, PTEBP_EMULATE_FILE_CHECK) != 0) return -1;
        for (size_t page = 0; page < 4; page += 3)
        {
            uint8_t byte = 0;
            struct iovec local = {.iov_base = &byte, .iov_len = 1};
            struct iovec remote = {.iov_base = mapping + page * page_size, .iov_len = 1};
            errno = 0;
            ssize_t count = process_vm_readv(getpid(), &local, 1, &remote, 1, 0);
            if (prot & PROT_READ)
            {
                if (count != 1 || byte != expected[page * page_size]) return -1;
            }
            else if (count != -1 || errno != EFAULT) return -1;

            byte = expected[page * page_size];
            errno = 0;
            count = process_vm_writev(getpid(), &local, 1, &remote, 1, 0);
            if (prot & PROT_WRITE)
            {
                if (count != 1) return -1;
            }
            else if (count != -1 || errno != EFAULT) return -1;
        }
        for (size_t page = 1; page <= 2; page++)
        {
            if (*(const volatile uint8_t *)(mapping + page * page_size) != expected[page * page_size]) return -1;
        }
        if (ioctl(device, PTEBP_EMULATE_FILE_CHECK) != 0) return -1;
    }

    // 末端空洞仍返回原生错误，不吞掉普通页错误；前面的成功区间允许保留更新。
    errno = 0;
    if (mprotect(mapping, 5 * page_size, PROT_READ) != -1 || errno != ENOMEM ||
        ioctl(device, PTEBP_EMULATE_FILE_CHECK) != 0) return -1;
    if (mprotect(mapping, 4 * page_size, PROT_READ | PROT_EXEC)) return -1;
    errno = 0;
    if (mprotect(mapping + 1, page_size, PROT_NONE) != -1 || errno != EINVAL) return -1;
    if (mprotect(mapping + page_size, 0, PROT_NONE) ||
        ioctl(device, PTEBP_EMULATE_FILE_CHECK) != 0) return -1;
    printf("ptebp protection test passed mixed_range=PASS ordinary_permissions=PASS managed_unchanged=PASS errors=PASS\n");
    return 0;
}

static int test_file_reads(int device)
{
    size_t page_size = (size_t)sysconf(_SC_PAGESIZE);
    size_t total = 4 * page_size;
    uint8_t *mapping = mmap(NULL, total + page_size, PROT_READ | PROT_WRITE | PROT_EXEC,
                            MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    uint8_t *expected = malloc(total);
    uint8_t *buffer = malloc(total + 2);
    int memory_fd = openat(AT_FDCWD, "/proc/self/mem", O_RDONLY | O_CLOEXEC);
    FILE *stream = fopen("/proc/self/mem", "rb");
    char stdio_buffer[4096];
    int status = -1;
    size_t checks = 0;
    size_t offset = 0;
    size_t length = 0;

    if (mapping == MAP_FAILED || !expected || !buffer || memory_fd < 0 || !stream) goto out;
    if (setvbuf(stream, stdio_buffer, _IOFBF, sizeof(stdio_buffer))) goto out;

    for (size_t index = 0; index < total; index++) mapping[index] = (uint8_t)(index * 37 + 11);
    *(uint32_t *)(mapping + page_size) = 0xd503201fU;
    *(uint32_t *)(mapping + page_size + 4) = 0xd65f03c0U;
    *(uint32_t *)(mapping + 2 * page_size - 4) = 0xd503201fU;
    *(uint32_t *)(mapping + 2 * page_size) = 0xd503201fU;
    memcpy(expected, mapping, total);
    __builtin___clear_cache((char *)mapping, (char *)mapping + total);
    if (mprotect(mapping, total, PROT_READ | PROT_EXEC) ||
        munmap(mapping + total, page_size)) goto out;

    uint64_t address = (uintptr_t)(mapping + page_size);
    if (ioctl(device, PTEBP_EMULATE_FILE_START, &address) < 0) goto out;
    if (ioctl(device, PTEBP_EMULATE_FILE_CHECK) != 0) goto out;
    if (mprotect(mapping + page_size, 2 * page_size, PROT_READ | PROT_WRITE | PROT_EXEC)) goto out;
    if (test_protection_ranges(device, mapping, expected, page_size)) goto out;

    for (int method = 0; method < 2; method++)
    {
        FILE *selected_stream = method ? stream : NULL;
        for (offset = page_size; offset < 3 * page_size; offset++)
        {
            length = 1;
            if (check_file_bytes(memory_fd, selected_stream, (uintptr_t)mapping,
                                  expected, buffer, offset, length, 1)) goto out;
            checks++;
        }
        offset = page_size - 1;
        for (length = 0; length <= 2 * page_size + 1; length++)
        {
            if (check_file_bytes(memory_fd, selected_stream, (uintptr_t)mapping,
                                  expected, buffer, offset, length, 1)) goto out;
            checks++;
        }
        offset = 0;
        length = total;
        if (check_file_bytes(memory_fd, selected_stream, (uintptr_t)mapping,
                              expected, buffer, offset, length, method ? 8 : 1)) goto out;
        checks++;
        for (offset = 0; offset < total; offset++)
        {
            if (buffer[offset + 1] != *(const volatile uint8_t *)(mapping + offset))
            {
                fprintf(stderr, "file_pointer_mismatch api=%s offset=%zu\n", method ? "fread" : "read", offset);
                goto out;
            }
        }

        ((void (*)(void))(uintptr_t)address)();
        if (ioctl(device, PTEBP_EMULATE_FILE_CHECK) != method + 1) goto out;
        printf("file_read_path=%s status=PASS single_bytes=%zu lengths=0..%zu\n",
               method ? "fopen_fseek_fread" : "openat_lseek_read", 2 * page_size,
               2 * page_size + 1);
        fflush(stdout);
    }

    if (lseek(memory_fd, (off_t)((uintptr_t)mapping + total - 7), SEEK_SET) < 0 ||
        read(memory_fd, buffer, 32) != 7 || memcmp(buffer, expected + total - 7, 7)) goto out;
    if (fseek(stream, (long)((uintptr_t)mapping + total - 7), SEEK_SET) ||
        fread(buffer, 1, 32, stream) != 7 || !ferror(stream) ||
        memcmp(buffer, expected + total - 7, 7)) goto out;
    clearerr(stream);
    if (lseek(memory_fd, 0, SEEK_SET) != 0) goto out;
    errno = 0;
    if (read(memory_fd, buffer, 1) != -1 || errno != EIO) goto out;
    if (fseek(stream, 0, SEEK_SET)) goto out;
    errno = 0;
    if (fread(buffer, 1, 1, stream) || !ferror(stream) || errno != EIO) goto out;
    clearerr(stream);
    struct iovec local_vecs[] = {
        {.iov_base = buffer + 1, .iov_len = 1},
        {.iov_base = NULL, .iov_len = 0},
        {.iov_base = buffer + 2, .iov_len = page_size},
        {.iov_base = buffer + page_size + 2, .iov_len = page_size},
    };
    struct iovec remote_vecs[] = {
        {.iov_base = mapping + page_size - 1, .iov_len = 3},
        {.iov_base = mapping + page_size + 2, .iov_len = page_size - 2},
        {.iov_base = mapping + 2 * page_size, .iov_len = page_size},
    };
    if (process_vm_readv(getpid(), local_vecs, 4, remote_vecs, 3, 0) != (ssize_t)(2 * page_size + 1) ||
        memcmp(buffer + 1, expected + page_size - 1, 2 * page_size + 1)) goto out;

    local_vecs[0] = (struct iovec){.iov_base = buffer, .iov_len = 7};
    local_vecs[1] = (struct iovec){.iov_base = NULL, .iov_len = 1};
    remote_vecs[0] = (struct iovec){.iov_base = mapping + page_size, .iov_len = 8};
    if (process_vm_readv(getpid(), local_vecs, 2, remote_vecs, 1, 0) != 7 ||
        memcmp(buffer, expected + page_size, 7)) goto out;

    local_vecs[0].iov_len = 8;
    remote_vecs[0].iov_len = 7;
    remote_vecs[1] = (struct iovec){.iov_base = NULL, .iov_len = 1};
    if (process_vm_readv(getpid(), local_vecs, 1, remote_vecs, 2, 0) != 7 ||
        memcmp(buffer, expected + page_size, 7)) goto out;
    // 普通源页仍遵守 PROT_NONE；受管页的权限请求按当前契约被跳过。
    remote_vecs[0].iov_base = mapping + 3 * page_size;
    if (mprotect(mapping + 3 * page_size, page_size, PROT_NONE)) goto out;
    errno = 0;
    if (process_vm_readv(getpid(), local_vecs, 1, remote_vecs, 1, 0) != -1 || errno != EFAULT) goto out;
    if (mprotect(mapping + 3 * page_size, page_size, PROT_READ | PROT_EXEC)) goto out;

    if (mprotect(mapping + page_size, 2 * page_size, PROT_READ | PROT_EXEC)) goto out;
    if (ioctl(device, PTEBP_EMULATE_FILE_CHECK) != 2) goto out;
    if (ioctl(device, PTEBP_EMULATE_RESET) < 0 || memcmp(mapping, expected, total)) goto out;

    if (ioctl(device, PTEBP_EMULATE_FILE_START, &address) < 0 ||
        check_file_bytes(memory_fd, NULL, (uintptr_t)mapping, expected, buffer, page_size, 1, 1) ||
        check_file_bytes(memory_fd, stream, (uintptr_t)mapping, expected, buffer, page_size, 1, 1) ||
        ioctl(device, PTEBP_EMULATE_FILE_CHECK) != 0 ||
        ioctl(device, PTEBP_EMULATE_RESET) < 0 || memcmp(mapping, expected, total)) goto out;

    if (mprotect(mapping + page_size, page_size, PROT_READ | PROT_WRITE | PROT_EXEC) ||
        ioctl(device, PTEBP_EMULATE_FILE_START, &address) < 0) goto out;
    local_vecs[0] = (struct iovec){.iov_base = mapping + page_size + 128, .iov_len = 8};
    remote_vecs[0] = (struct iovec){.iov_base = mapping + page_size, .iov_len = 8};
    if (process_vm_readv(getpid(), local_vecs, 1, remote_vecs, 1, 0) != 8) goto out;
    if (ioctl(device, PTEBP_EMULATE_FILE_CHECK) >= 0 ||
        ioctl(device, PTEBP_EMULATE_RESET) < 0) goto out;
    memcpy(expected + page_size + 128, expected + page_size, 8);
    if (memcmp(mapping, expected, total)) goto out;
    printf("ptebp process_vm test passed iovec=PASS partial=PASS permissions=PASS guarded_destination=PASS\n");

    printf("ptebp file read test passed checks=%zu hits=2 restart=PASS short_read=PASS errors=PASS rwx=PASS pointer_view=PASS\n", checks);
    fflush(stdout);
    status = 0;
out:
    if (status) fprintf(stderr, "file_read_test=FAIL offset=%zu length=%zu errno=%d\n", offset, length, errno);
    if (ioctl(device, PTEBP_EMULATE_RESET) < 0) status = -1;
    if (stream) fclose(stream);
    if (memory_fd >= 0) close(memory_fd);
    if (mapping != MAP_FAILED) munmap(mapping, total + page_size);
    free(buffer);
    free(expected);
    return status;
}

int main(int argc, char **argv)
{
    uint32_t *instructions = NULL;
    size_t count = 0;
    int device;
    size_t compared = 0;
    size_t skipped = 0;
    int exit_code = 1;

    if (argc != 3)
    {
        fprintf(stderr, "usage: %s <instruction.txt|--file-read-only> </dev/ptebp_emulate_test>\n",
                argv[0]);
        return 1;
    }
    int file_read_only = strcmp(argv[1], "--file-read-only") == 0;
    if (!file_read_only && read_instructions(argv[1], &instructions, &count) < 0)
        return 1;
    device = open(argv[2], O_RDWR | O_CLOEXEC);
    if (device < 0)
    {
        perror(argv[2]);
        free(instructions);
        return 1;
    }

    printf("protocol_version=%u\n", PTEBP_EMULATE_PROTOCOL_VERSION);
    printf("runner_build_id=%s\n", runner_build_id);
    printf("instruction_count=%zu\n", count);
    fflush(stdout);
    if (test_file_reads(device) < 0) goto out;
    if (file_read_only)
    {
        exit_code = 0;
        goto out;
    }
    {
        struct sigaction action = { .sa_handler = runner_fatal_signal };
        sigemptyset(&action.sa_mask);
        sigaction(SIGSEGV, &action, NULL);
        sigaction(SIGBUS, &action, NULL);
        sigaction(SIGILL, &action, NULL);
        sigaction(SIGABRT, &action, NULL);
    }

    for (size_t index = 0; index < count; index++)
    {
        struct ptebp_emulate_case test_case;
        struct ptebp_emulate_completion completion = {
            .version = PTEBP_EMULATE_PROTOCOL_VERSION,
            .index = (uint32_t)index,
            .raw = instructions[index],
        };
        struct ptebp_emulate_arch_state cpu_state;
        uint8_t cpu_memory[PTEBP_EMULATE_MEMORY_SIZE];
        struct runner_result result = {
            .status = RUNNER_STATUS_FAIL,
            .mismatch_kind = RUNNER_MISMATCH_NONE,
        };
        int cpu_status;

        initialize_case(&test_case, index, instructions[index]);
        runner_current_index = (sig_atomic_t)index;
        if (sigsetjmp(runner_recovery, 1) != 0)
            goto out;
        if (runner_cpu_session.child < 0 &&
            cpu_session_start(&runner_cpu_session, &test_case) < 0)
            goto out;
        cpu_status = cpu_session_step(&runner_cpu_session, &test_case,
                                      &cpu_state, cpu_memory);
        if (cpu_status < 0)
            goto out;
        if (cpu_status > 0)
        {
            printf("index=%zu raw=0x%08x status=%u action=skip emulator=not_called\n",
                   index, instructions[index], RUNNER_STATUS_CPU_EXCEPTION);
            skipped++;
            continue;
        }
        if (prepare_case_isolated(device, &test_case) < 0)
        {
            fprintf(stderr, "index=%zu raw=0x%08x result=PREPARE_FAIL errno=%d\n",
                    index, instructions[index], errno);
            goto out;
        }
        if (ioctl(device, PTEBP_EMULATE_COMPLETE, &completion) < 0)
        {
            perror("PTEBP_EMULATE_COMPLETE");
            goto out;
        }
        if (completion.version != PTEBP_EMULATE_PROTOCOL_VERSION ||
            completion.index != index || completion.raw != instructions[index] ||
            completion.reserved != 0U)
        {
            fprintf(stderr, "index=%zu raw=0x%08x result=PROTOCOL_FAIL\n",
                    index, instructions[index]);
            goto out;
        }
        compare_results(&result, &completion, &cpu_state, cpu_memory);
        if (result.status == RUNNER_STATUS_PASS)
        {
            printf("index=%zu raw=0x%08x status=%u mismatch=none\n",
                   index, instructions[index], result.status);
            compared++;
        }
        else
        {
            printf("index=%zu raw=0x%08x status=%u mismatch_kind=%u mismatch_index=%u mismatch_bit=%u expected=0x%016llx actual=0x%016llx memory_offset=%llu expected_byte=0x%02x actual_byte=0x%02x\n",
                   index, instructions[index], result.status,
                   result.mismatch_kind, result.mismatch_index,
                   result.mismatch_bit,
                   (unsigned long long)result.expected_value,
                   (unsigned long long)result.actual_value,
                   (unsigned long long)result.memory_offset,
                   result.expected_byte, result.actual_byte);
            goto out;
        }
        fflush(stdout);
    }
    printf("ptebp emulator test passed cases=%zu\n", count);
    printf("case_counts=compared=%zu skipped=%zu\n", compared, skipped);
    exit_code = 0;
out:
    ioctl(device, PTEBP_EMULATE_RESET);
    cpu_session_stop(&runner_cpu_session);
    close(device);
    free(instructions);
    return exit_code;
}
