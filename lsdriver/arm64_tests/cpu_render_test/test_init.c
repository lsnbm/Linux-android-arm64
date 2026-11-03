// SPDX-License-Identifier: GPL-2.0
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/reboot.h>
#include <sys/syscall.h>
#include <unistd.h>

#ifndef SYS_finit_module
#define SYS_finit_module 273
#endif

#ifndef SYS_delete_module
#define SYS_delete_module 106
#endif

#ifndef SYS_reboot
#define SYS_reboot 142
#endif

#ifndef LINUX_REBOOT_MAGIC1
#define LINUX_REBOOT_MAGIC1 0xfee1dead
#endif

#ifndef LINUX_REBOOT_MAGIC2
#define LINUX_REBOOT_MAGIC2 0x28121969
#endif

#ifndef LINUX_REBOOT_CMD_POWER_OFF
#define LINUX_REBOOT_CMD_POWER_OFF 0x4321fedc
#endif

#define MODULE_PATH "/cpu_render_test.ko"
#define MODULE_NAME "cpu_render_test"

__asm__(
    ".section .tdata,\"awT\",%progbits\n"
    ".balign 64\n"
    ".global cpu_render_test_init_tls_alignment_anchor\n"
    ".hidden cpu_render_test_init_tls_alignment_anchor\n"
    ".type cpu_render_test_init_tls_alignment_anchor,%object\n"
    "cpu_render_test_init_tls_alignment_anchor:\n"
    ".zero 1\n"
    ".size cpu_render_test_init_tls_alignment_anchor,1\n"
    ".previous\n");

static void power_off(int status)
{
    sync();
    syscall(SYS_reboot, LINUX_REBOOT_MAGIC1, LINUX_REBOOT_MAGIC2,
            LINUX_REBOOT_CMD_POWER_OFF, 0, NULL);
    _exit(status);
}

static void mount_optional(const char *source, const char *target, const char *filesystem)
{
    if (mount(source, target, filesystem, 0, NULL) < 0)
        dprintf(STDERR_FILENO, "CPU_RENDER_MOUNT_SKIP target=%s errno=%d\n", target, errno);
}

static int load_module(void)
{
    int descriptor = open(MODULE_PATH, O_RDONLY | O_CLOEXEC);
    int result;

    if (descriptor < 0)
    {
        dprintf(STDERR_FILENO, "CPU_RENDER_MODULE_OPEN_FAIL errno=%d\n", errno);
        return -1;
    }
    result = (int)syscall(SYS_finit_module, descriptor, "", 0U);
    close(descriptor);
    if (result < 0)
    {
        dprintf(STDERR_FILENO, "CPU_RENDER_MODULE_LOAD_FAIL errno=%d\n", errno);
        return -1;
    }
    dprintf(STDOUT_FILENO, "CPU_RENDER_MODULE_LOAD_OK\n");
    return 0;
}

static int unload_module(void)
{
    int result = (int)syscall(SYS_delete_module, MODULE_NAME, 0U);

    if (result < 0)
    {
        dprintf(STDERR_FILENO, "CPU_RENDER_MODULE_UNLOAD_FAIL errno=%d\n", errno);
        return -1;
    }
    dprintf(STDOUT_FILENO, "CPU_RENDER_MODULE_UNLOAD_OK\n");
    return 0;
}

int main(void)
{
    int console;
    int status = 1;
    int module_loaded = 0;

    console = open("/dev/console", O_RDWR);
    if (console >= 0)
    {
        dup2(console, STDIN_FILENO);
        dup2(console, STDOUT_FILENO);
        dup2(console, STDERR_FILENO);
        if (console > STDERR_FILENO) close(console);
    }

    dprintf(STDOUT_FILENO, "CPU_RENDER_TEST_BEGIN\n");
    mount_optional("proc", "/proc", "proc");
    mount_optional("sysfs", "/sys", "sysfs");
    mount_optional("devtmpfs", "/dev", "devtmpfs");

    if (load_module() == 0)
    {
        module_loaded = 1;
        status = 0;
    }
    if (module_loaded && unload_module() < 0) status = 1;

    if (status == 0)
        dprintf(STDOUT_FILENO, "CPU_RENDER_TEST_PASS\n");
    else
        dprintf(STDERR_FILENO, "CPU_RENDER_TEST_FAIL\n");
    power_off(status);
}
