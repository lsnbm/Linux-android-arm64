#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;
BUILD_LTO_INFO;

MODULE_INFO(vermagic, VERMAGIC_STRING);
MODULE_INFO(name, KBUILD_MODNAME);

__visible struct module __this_module
__section(".gnu.linkonce.this_module") = {
	.name = KBUILD_MODNAME,
	.init = init_module,
#ifdef CONFIG_MODULE_UNLOAD
	.exit = cleanup_module,
#endif
	.arch = MODULE_ARCH_INIT,
};

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif

static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0x222dd63, "module_layout" },
	{ 0xdcb764ad, "memset" },
	{ 0x4829a47e, "memcpy" },
	{ 0xaf56600a, "arm64_use_ng_mappings" },
	{ 0xaf60e2d8, "init_task" },
	{ 0xea0fd5fe, "kmalloc_caches" },
	{ 0xd14fef22, "cpu_hwcap_keys" },
	{ 0x14b89635, "arm64_const_caps_ready" },
	{ 0x5efdd68b, "__tracepoint_mmap_lock_released" },
	{ 0xbe118c52, "__tracepoint_mmap_lock_acquire_returned" },
	{ 0xbd628752, "__tracepoint_mmap_lock_start_locking" },
	{ 0xa3521253, "mem_section" },
	{ 0x9688de8b, "memstart_addr" },
	{ 0xdf7a4c69, "__ubsan_handle_cfi_check_fail_abort" },
	{ 0x5b93a483, "sysfs_remove_link" },
	{ 0xdcd7f2d3, "kobject_del" },
	{ 0x4d9b652b, "rb_erase" },
	{ 0xe1537255, "__list_del_entry_valid" },
	{ 0xc3055d20, "usleep_range_state" },
	{ 0xcba94ed2, "__put_page" },
	{ 0xf9a482f9, "msleep" },
	{ 0xbfbc2abf, "vmap" },
	{ 0x1d36e9bb, "get_user_pages_remote" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x4addd462, "wake_up_process" },
	{ 0x15f9ecee, "kthread_create_on_node" },
	{ 0x2469810f, "__rcu_read_unlock" },
	{ 0x8d522714, "__rcu_read_lock" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x5239aaef, "kmem_cache_alloc_trace" },
	{ 0x296695f, "refcount_warn_saturate" },
	{ 0xa8ebe4, "__put_task_struct" },
	{ 0x86832082, "__mmap_lock_do_trace_released" },
	{ 0x6b50e951, "up_read" },
	{ 0x3355da1c, "down_read" },
	{ 0x1ed3e175, "__mmap_lock_do_trace_acquire_returned" },
	{ 0x583d61bc, "__mmap_lock_do_trace_start_locking" },
	{ 0x88f5cdef, "down_read_trylock" },
	{ 0xdd007012, "input_event" },
	{ 0x83e03fd9, "input_mt_sync_frame" },
	{ 0x7c51953, "input_mt_report_slot_state" },
	{ 0xeb78b1ed, "unregister_kprobe" },
	{ 0x472cf3b, "register_kprobe" },
	{ 0xb7c0f443, "sort" },
	{ 0xdfb814f4, "d_path" },
	{ 0x1273c714, "find_vpid" },
	{ 0x75c08240, "pid_task" },
	{ 0x349cba85, "strchr" },
	{ 0x2d39b0a7, "kstrdup" },
	{ 0x1e6d26a8, "strstr" },
	{ 0x5c9998cf, "get_task_mm" },
	{ 0x16dd6f02, "put_pid" },
	{ 0xbc24e309, "get_pid_task" },
	{ 0x90b01573, "find_get_pid" },
	{ 0xe1c11b81, "mmput" },
	{ 0x999e8297, "vfree" },
	{ 0xd6ee688f, "vmalloc" },
	{ 0x37a0cba, "kfree" },
	{ 0x420c430d, "input_set_abs_params" },
	{ 0x313a6dce, "put_device" },
	{ 0x13fca8b1, "get_device" },
	{ 0x70ae2e0e, "class_for_each_device" },
	{ 0x92997ed8, "_printk" },
	{ 0xed55cabd, "mutex_unlock" },
	{ 0xd5977bfb, "mutex_lock" },
};

MODULE_INFO(depends, "");

