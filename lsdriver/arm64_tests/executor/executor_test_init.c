#include <stdint.h>

#define AT_FDCWD      -100
#define O_RDONLY      0
#define SYS_OPENAT    56
#define SYS_CLOSE     57
#define SYS_WRITE     64
#define SYS_EXECVE    221
#define SYS_FINIT_MODULE 273

static long syscall1(long number, long arg0)
{
    register long x0 asm("x0") = arg0;
    register long x8 asm("x8") = number;

    asm volatile("svc #0" : "+r"(x0) : "r"(x8) : "memory", "cc");
    return x0;
}

static long syscall3(long number, long arg0, long arg1, long arg2)
{
    register long x0 asm("x0") = arg0;
    register long x1 asm("x1") = arg1;
    register long x2 asm("x2") = arg2;
    register long x8 asm("x8") = number;

    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory", "cc");
    return x0;
}

static long syscall_execve(const char *path, char *const argv[], char *const envp[])
{
    register long x0 asm("x0") = (long)path;
    register long x1 asm("x1") = (long)argv;
    register long x2 asm("x2") = (long)envp;
    register long x8 asm("x8") = SYS_EXECVE;

    asm volatile("svc #0" : "+r"(x0) : "r"(x1), "r"(x2), "r"(x8) : "memory", "cc");
    return x0;
}

static unsigned long string_length(const char *text)
{
    const char *end = text;

    while (*end) end++;
    return (unsigned long)(end - text);
}

static void write_console(const char *text)
{
    syscall3(SYS_WRITE, 1, (long)text, string_length(text));
}

__attribute__((noreturn)) void _start(void)
{
    static const char module_path[] = "/arm64_kernel_executor_test_module.ko";
    long module_fd;
    long result;

    write_console("executor-init: loading test module\n");
    module_fd = syscall3(SYS_OPENAT, AT_FDCWD, (long)module_path, O_RDONLY);
    if (module_fd < 0) {
        write_console("executor-init: open module failed\n");
        goto idle;
    }

    result = syscall3(SYS_FINIT_MODULE, module_fd, (long)"", 0);
    syscall1(SYS_CLOSE, module_fd);
    if (result < 0)
        write_console("executor-init: finit_module failed\n");
    else {
        static const char runner_path[] = "/executor_test_runner";
        static const char instruction_path[] = "/instruction.txt";
        static const char device_path[] = "/dev/arm64_executor_test";
        static char *const runner_argv[] = {
            (char *)runner_path, (char *)instruction_path, (char *)device_path, 0,
        };
        static char *const runner_envp[] = { 0 };
        syscall_execve(runner_path, runner_argv, runner_envp);
        write_console("executor-init: runner exec failed\n");
    }

idle:
    for (;;) asm volatile("wfe" ::: "memory");
}