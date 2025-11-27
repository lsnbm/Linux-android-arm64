#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/elfnote-lto.h>
#include <linux/export-internal.h>
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

MODULE_INFO(scmversion, "gff992d68744d-dirty");

#ifdef CONFIG_RETPOLINE
MODULE_INFO(retpoline, "Y");
#endif


static const struct modversion_info ____versions[]
__used __section("__versions") = {
	{ 0xd6ee688f, "vmalloc" },
	{ 0x472cf3b, "register_kprobe" },
	{ 0xeb78b1ed, "unregister_kprobe" },
	{ 0x9688de8b, "memstart_addr" },
	{ 0x999e8297, "vfree" },
	{ 0x4829a47e, "memcpy" },
	{ 0xbd628752, "__tracepoint_mmap_lock_start_locking" },
	{ 0x88f5cdef, "down_read_trylock" },
	{ 0xbe118c52, "__tracepoint_mmap_lock_acquire_returned" },
	{ 0x56ac7b18, "__mmap_lock_do_trace_start_locking" },
	{ 0x5319be0e, "__mmap_lock_do_trace_acquire_returned" },
	{ 0x5efdd68b, "__tracepoint_mmap_lock_released" },
	{ 0x6b50e951, "up_read" },
	{ 0xb996c292, "__mmap_lock_do_trace_released" },
	{ 0x3355da1c, "down_read" },
	{ 0x97394d0c, "mmput" },
	{ 0xa1aafdfe, "find_get_pid" },
	{ 0xecd5f222, "get_pid_task" },
	{ 0x354db4c3, "put_pid" },
	{ 0x41436ea5, "get_task_mm" },
	{ 0x1348649e, "alt_cb_patch_nops" },
	{ 0x950c847f, "__put_task_struct" },
	{ 0x296695f, "refcount_warn_saturate" },
	{ 0x1e6d26a8, "strstr" },
	{ 0x2d39b0a7, "kstrdup" },
	{ 0x349cba85, "strchr" },
	{ 0xf15b38a, "kmalloc_caches" },
	{ 0xb78e7543, "kmalloc_trace" },
	{ 0x8d522714, "__rcu_read_lock" },
	{ 0xaf95aaf5, "find_vpid" },
	{ 0x564f2dac, "pid_task" },
	{ 0x2469810f, "__rcu_read_unlock" },
	{ 0x9acf31c6, "mas_find" },
	{ 0x4aef61ee, "d_path" },
	{ 0x37a0cba, "kfree" },
	{ 0xb7c0f443, "sort" },
	{ 0x92997ed8, "_printk" },
	{ 0xb0f4ab9f, "kthread_create_on_node" },
	{ 0x73e2bcf1, "wake_up_process" },
	{ 0xe1537255, "__list_del_entry_valid" },
	{ 0xdcb764ad, "memset" },
	{ 0x4d9b652b, "rb_erase" },
	{ 0xd56b3592, "kobject_del" },
	{ 0xdfb076f5, "sysfs_remove_link" },
	{ 0xd567d551, "init_task" },
	{ 0xf9a482f9, "msleep" },
	{ 0xe2d5255a, "strcmp" },
	{ 0xe4dd19ab, "get_user_pages_remote" },
	{ 0xaf56600a, "arm64_use_ng_mappings" },
	{ 0xad3f829d, "vmap" },
	{ 0xacfe4142, "page_pinner_inited" },
	{ 0x2c38db08, "__folio_put" },
	{ 0xbf444ebf, "__page_pinner_put_page" },
	{ 0xc3055d20, "usleep_range_state" },
	{ 0xea759d7f, "module_layout" },
};

MODULE_INFO(depends, "");

