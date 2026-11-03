.section .text
.align 2
.global _start
.type _start, %function

.equ AT_FDCWD, -100
.equ O_RDONLY, 0
.equ O_RDWR, 2
.equ O_WRONLY, 1
.equ SIGCHLD, 17
.equ SYS_MOUNT, 40
.equ SYS_OPENAT, 56
.equ SYS_CLOSE, 57
.equ SYS_DUP3, 24
.equ SYS_WRITE, 64
.equ SYS_EXIT, 93
.equ SYS_DELETE_MODULE, 106
.equ SYS_REBOOT, 142
.equ SYS_CLONE, 220
.equ SYS_EXECVE, 221
.equ SYS_WAIT4, 260
.equ SYS_FINIT_MODULE, 273
.equ LINUX_REBOOT_MAGIC1, 0xfee1dead
.equ LINUX_REBOOT_MAGIC2, 0x28121969
.equ LINUX_REBOOT_CMD_POWER_OFF, 0x4321fedc

_start:
    bl open_console
    bl mount_proc
    cbnz x0, mount_failed
    bl load_module
    cbnz x0, load_failed_exit
    bl run_runner
    mov x20, x0
    bl unload_module
    cbnz x0, unload_failed_exit
    cbnz x20, runner_failed_exit

    adrp x1, message_pass
    add x1, x1, :lo12:message_pass
    mov x2, #message_pass_len
    bl write_message
    b power_off

qemu_failed:
    adrp x1, message_fail
    add x1, x1, :lo12:message_fail
    mov x2, #message_fail_len
    bl write_message
    b power_off

mount_failed:
    adrp x1, message_mount_failed
    add x1, x1, :lo12:message_mount_failed
    mov x2, #message_mount_failed_len
    bl write_message
    b power_off

load_failed_exit:
    adrp x1, message_load_failed
    add x1, x1, :lo12:message_load_failed
    mov x2, #message_load_failed_len
    bl write_message
    b power_off

runner_failed_exit:
    adrp x1, message_runner_failed
    add x1, x1, :lo12:message_runner_failed
    mov x2, #message_runner_failed_len
    bl write_message
    b power_off

unload_failed_exit:
    adrp x1, message_unload_failed
    add x1, x1, :lo12:message_unload_failed
    mov x2, #message_unload_failed_len
    bl write_message
    b power_off

open_console:
    mov x0, #AT_FDCWD
    adrp x1, console_path
    add x1, x1, :lo12:console_path
    mov x2, #O_RDWR
    mov x3, #0
    mov x8, #SYS_OPENAT
    svc #0
    cmp x0, #0
    b.ge console_open
    mov x0, #1
console_open:
    mov x19, x0
    ret

write_message:
    mov x0, x19
    mov x8, #SYS_WRITE
    svc #0
    ret

mount_proc:
    adrp x0, proc_source
    add x0, x0, :lo12:proc_source
    adrp x1, proc_path
    add x1, x1, :lo12:proc_path
    adrp x2, proc_type
    add x2, x2, :lo12:proc_type
    mov x3, #0
    mov x4, #0
    mov x8, #SYS_MOUNT
    svc #0
    cmp x0, #0
    cset x0, ne
    ret

load_module:
    mov x0, #AT_FDCWD
    adrp x1, module_path
    add x1, x1, :lo12:module_path
    mov x2, #O_RDONLY
    mov x3, #0
    mov x8, #SYS_OPENAT
    svc #0
    cmp x0, #0
    b.lt load_failed
    mov x20, x0
    mov x0, x20
    adrp x1, empty_params
    add x1, x1, :lo12:empty_params
    mov x2, #0
    mov x8, #SYS_FINIT_MODULE
    svc #0
    mov x21, x0
    mov x0, x20
    mov x8, #SYS_CLOSE
    svc #0
    mov x0, x21
    cmp x0, #0
    b.lt load_failed
    mov x0, #0
    ret

load_failed:
    mov x0, #1
    ret

run_runner:
    mov x0, #SIGCHLD
    mov x1, #0
    mov x2, #0
    mov x3, #0
    mov x4, #0
    mov x8, #SYS_CLONE
    svc #0
    cmp x0, #0
    b.eq runner_child
    b.lt runner_failed
    mov x20, x0
    mov x0, x20
    adrp x1, wait_status
    add x1, x1, :lo12:wait_status
    mov x2, #0
    mov x3, #0
    mov x8, #SYS_WAIT4
    svc #0
    cmp x0, #0
    b.lt runner_failed
    adrp x1, wait_status
    add x1, x1, :lo12:wait_status
    ldr w0, [x1]
    lsr w0, w0, #8
    and w0, w0, #0xff
    ret

runner_child:
    mov x0, #AT_FDCWD
    adrp x1, console_path
    add x1, x1, :lo12:console_path
    mov x2, #1
    mov x3, #0
    mov x8, #SYS_OPENAT
    svc #0
    cmp x0, #0
    b.lt runner_exec
    mov x20, x0
    mov x0, x20
    mov x1, #1
    mov x2, #0
    mov x8, #SYS_DUP3
    svc #0
    mov x0, x20
    mov x1, #2
    mov x2, #0
    mov x8, #SYS_DUP3
    svc #0
    mov x0, x20
    mov x8, #SYS_CLOSE
    svc #0
runner_exec:
    adrp x0, runner_path
    add x0, x0, :lo12:runner_path
    adrp x1, runner_argv
    add x1, x1, :lo12:runner_argv
    mov x2, #0
    mov x8, #SYS_EXECVE
    svc #0
    mov x0, #127
    mov x8, #SYS_EXIT
    svc #0
    b .

runner_failed:
    mov x0, #1
    ret

unload_module:
    adrp x0, module_name
    add x0, x0, :lo12:module_name
    mov x1, #0
    mov x8, #SYS_DELETE_MODULE
    svc #0
    cmp x0, #0
    cset x0, ne
    ret

power_off:
    movz x0, #0xdead
    movk x0, #0xfee1, lsl #16
    movz x1, #0x1969
    movk x1, #0x2812, lsl #16
    movz x2, #0xfedc
    movk x2, #0x4321, lsl #16
    mov x3, #0
    mov x8, #SYS_REBOOT
    svc #0
1:
    wfe
    b 1b

.section .rodata
.align 3
console_path: .asciz "/dev/console"
devtmpfs_source: .asciz "devtmpfs"
devtmpfs_type: .asciz "devtmpfs"
dev_path: .asciz "/dev"
module_path: .asciz "/ptebp_emulate_test_module.ko"
module_name: .asciz "ptebp_emulate_test_module"
runner_path: .asciz "/ptebp_emulate_test_runner"
instruction_path: .asciz "/instruction.txt"
device_path: .asciz "/proc/ptebp_emulate_test"
proc_source: .asciz "proc"
proc_type: .asciz "proc"
proc_path: .asciz "/proc"
empty_params: .byte 0
message_pass: .asciz "PTEBP_TEST_QEMU_PASS\n"
.set message_pass_len, . - message_pass
message_fail: .asciz "PTEBP_TEST_QEMU_FAIL\n"
.set message_fail_len, . - message_fail
message_mount_failed: .asciz "PTEBP_TEST_MOUNT_FAIL\n"
.set message_mount_failed_len, . - message_mount_failed
message_load_failed: .asciz "PTEBP_TEST_LOAD_FAIL\n"
.set message_load_failed_len, . - message_load_failed
message_runner_failed: .asciz "PTEBP_TEST_RUNNER_FAIL\n"
.set message_runner_failed_len, . - message_runner_failed
message_unload_failed: .asciz "PTEBP_TEST_UNLOAD_FAIL\n"
.set message_unload_failed_len, . - message_unload_failed

.align 3
runner_argv:
    .quad runner_path
    .quad instruction_path
    .quad device_path
    .quad 0

.section .bss
.align 3
wait_status: .skip 4
