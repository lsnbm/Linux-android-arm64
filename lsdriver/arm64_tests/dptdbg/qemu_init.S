.section .text
.align 2
.global _start
.type _start, %function

.equ AT_FDCWD, -100
.equ O_RDWR, 2
.equ __NR_openat, 56
.equ __NR_close, 57
.equ __NR_mmap, 222
.equ __NR_mprotect, 226
.equ __NR_write, 64
.equ __NR_exit, 93
.equ __NR_delete_module, 106
.equ __NR_reboot, 142
.equ __NR_finit_module, 273
.equ LINUX_REBOOT_MAGIC1, 0xfee1dead
.equ LINUX_REBOOT_MAGIC2, 0x28121969
.equ LINUX_REBOOT_CMD_POWER_OFF, 0x4321fedc

_start:
    bl open_console

    adrp x1, message_begin
    add x1, x1, :lo12:message_begin
    mov x2, #message_begin_len
    bl write_message

    bl map_test_page
    cbnz x0, test_page_failed

    bl load_module
    cbnz x0, module_load_failed

    adrp x1, message_load_ok
    add x1, x1, :lo12:message_load_ok
    mov x2, #message_load_ok_len
    bl write_message

    bl run_test_function
    cbnz x0, test_function_failed

    adrp x1, message_result_ok
    add x1, x1, :lo12:message_result_ok
    mov x2, #message_result_ok_len
    bl write_message

    bl unload_module
    cbnz x0, module_unload_failed

    adrp x1, message_unload_ok
    add x1, x1, :lo12:message_unload_ok
    mov x2, #message_unload_ok_len
    bl write_message

    adrp x1, message_pass
    add x1, x1, :lo12:message_pass
    mov x2, #message_pass_len
    bl write_message
    b power_off

test_page_failed:
    adrp x1, message_page_failed
    add x1, x1, :lo12:message_page_failed
    mov x2, #message_page_failed_len
    bl write_message
    b power_off

module_load_failed:
    adrp x1, message_load_failed
    add x1, x1, :lo12:message_load_failed
    mov x2, #message_load_failed_len
    bl write_message
    b power_off

module_unload_failed:
    adrp x1, message_unload_failed
    add x1, x1, :lo12:message_unload_failed
    mov x2, #message_unload_failed_len
    bl write_message
    b power_off

test_function_failed:
    adrp x1, message_result_failed
    add x1, x1, :lo12:message_result_failed
    mov x2, #message_result_failed_len
    bl write_message
    b power_off

open_console:
    mov x0, #AT_FDCWD
    adrp x1, console_path
    add x1, x1, :lo12:console_path
    mov x2, #O_RDWR
    mov x3, #0
    mov x8, #__NR_openat
    svc #0
    cmp x0, #0
    b.ge console_open
    mov x0, #1
console_open:
    mov x19, x0
    ret

write_message:
    mov x0, x19
    mov x8, #__NR_write
    svc #0
    ret

map_test_page:
    mov x0, #0x40000000
    mov x1, #0x1000
    mov x2, #7
    mov x3, #0x32
    mov x4, #-1
    mov x5, #0
    mov x8, #__NR_mmap
    svc #0
    mov x6, #0x40000000
    cmp x0, x6
    b.ne map_failed

    movz w6, #0x0400
    movk w6, #0x9100, lsl #16
    str w6, [x0]
    movz w6, #0x03c0
    movk w6, #0xd65f, lsl #16
    str w6, [x0, #4]

    mov x0, #0x40000000
    mov x1, #0x1000
    mov x2, #5
    mov x8, #__NR_mprotect
    svc #0
    cmp x0, #0
    b.lt map_failed
    mov x0, #0
    ret

map_failed:
    mov x0, #1
    ret

run_test_function:
    mov x0, #0
    mov x1, #0x40000000
    mov x2, #3
    mov x3, x30
run_test_function_loop:
    blr x1
    subs x2, x2, #1
    b.ne run_test_function_loop
    mov x30, x3
    cmp x0, #3
    b.ne test_function_result_failed
    mov x0, #0
    ret

test_function_result_failed:
    mov x0, #1
    ret

load_module:
    mov x0, #AT_FDCWD
    adrp x1, module_path
    add x1, x1, :lo12:module_path
    mov x2, #0
    mov x3, #0
    mov x8, #__NR_openat
    svc #0
    cmp x0, #0
    b.lt load_failed
    mov x20, x0

    mov x0, x20
    adrp x1, empty_params
    add x1, x1, :lo12:empty_params
    mov x2, #0
    mov x8, #__NR_finit_module
    svc #0
    mov x21, x0

    mov x0, x20
    mov x8, #__NR_close
    svc #0
    mov x0, x21
    cmp x0, #0
    b.lt load_failed
    ret

load_failed:
    mov x0, #1
    ret

unload_module:
    adrp x0, module_name
    add x0, x0, :lo12:module_name
    mov x1, #0
    mov x8, #__NR_delete_module
    svc #0
    cmp x0, #0
    b.lt unload_failed
    mov x0, #0
    ret

unload_failed:
    mov x0, #1
    ret

power_off:
    movz x0, #0xdead
    movk x0, #0xfee1, lsl #16
    movz x1, #0x1969
    movk x1, #0x2812, lsl #16
    movz x2, #0xfedc
    movk x2, #0x4321, lsl #16
    mov x3, #0
    mov x8, #__NR_reboot
    svc #0
    mov x0, #0
    mov x8, #__NR_exit
    svc #0
    b .

.section .rodata
.align 3
console_path:
    .asciz "/dev/console"
module_path:
    .asciz "/arm64_dptdbg_test_module.ko"
module_name:
    .asciz "arm64_dptdbg_test_module"
empty_params:
    .asciz ""
message_begin:
    .ascii "DPTDBG_TEST_BEGIN\n"
.equ message_begin_len, . - message_begin
message_page_failed:
    .ascii "DPTDBG_TEST_PAGE_FAILED\n"
.equ message_page_failed_len, . - message_page_failed
message_load_ok:
    .ascii "DPTDBG_TEST_LOAD_OK\n"
.equ message_load_ok_len, . - message_load_ok
message_result_ok:
    .ascii "DPTDBG_TEST_RESULT_OK\n"
.equ message_result_ok_len, . - message_result_ok
message_result_failed:
    .ascii "DPTDBG_TEST_RESULT_FAILED\n"
.equ message_result_failed_len, . - message_result_failed
message_load_failed:
    .ascii "DPTDBG_TEST_LOAD_FAILED\n"
.equ message_load_failed_len, . - message_load_failed
message_unload_ok:
    .ascii "DPTDBG_TEST_UNLOAD_OK\n"
.equ message_unload_ok_len, . - message_unload_ok
message_unload_failed:
    .ascii "DPTDBG_TEST_UNLOAD_FAILED\n"
.equ message_unload_failed_len, . - message_unload_failed
message_pass:
    .ascii "DPTDBG_TEST_PASS\n"
.equ message_pass_len, . - message_pass