#include <linux/module.h>
#define INCLUDE_VERMAGIC
#include <linux/build-salt.h>
#include <linux/vermagic.h>
#include <linux/compiler.h>

BUILD_SALT;

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
	{ 0x6584ccbd, "module_layout" },
	{ 0xdcb764ad, "memset" },
	{ 0x4829a47e, "memcpy" },
	{ 0x23f6655a, "failure_tracking" },
	{ 0xacfe4142, "page_pinner_inited" },
	{ 0xaf56600a, "arm64_use_ng_mappings" },
	{ 0xf5fdc492, "init_task" },
	{ 0x1c914fd5, "kmalloc_caches" },
	{ 0xec2fc692, "cpu_hwcap_keys" },
	{ 0x14b89635, "arm64_const_caps_ready" },
	{ 0x9688de8b, "memstart_addr" },
	{ 0x33aafbc9, "sysfs_remove_link" },
	{ 0x7c02219d, "kobject_del" },
	{ 0x4d9b652b, "rb_erase" },
	{ 0xe1537255, "__list_del_entry_valid" },
	{ 0x12a38747, "usleep_range" },
	{ 0xe1641e9, "__page_pinner_put_page" },
	{ 0x990fb212, "__put_page" },
	{ 0xf9a482f9, "msleep" },
	{ 0xe0467061, "vmap" },
	{ 0xeabc97ab, "get_user_pages_remote" },
	{ 0xe2d5255a, "strcmp" },
	{ 0x5b16f413, "wake_up_process" },
	{ 0x46dab91d, "kthread_create_on_node" },
	{ 0x2469810f, "__rcu_read_unlock" },
	{ 0x8d522714, "__rcu_read_lock" },
	{ 0xeb233a45, "__kmalloc" },
	{ 0x666e9a43, "kmem_cache_alloc_trace" },
	{ 0x296695f, "refcount_warn_saturate" },
	{ 0xf8ae89e, "__put_task_struct" },
	{ 0x6b50e951, "up_read" },
	{ 0x3355da1c, "down_read" },
	{ 0x88f5cdef, "down_read_trylock" },
	{ 0x1710aaba, "input_event" },
	{ 0xce59a2e0, "input_mt_sync_frame" },
	{ 0xd07d0958, "input_mt_report_slot_state" },
	{ 0xb7c0f443, "sort" },
	{ 0xaba95da9, "d_path" },
	{ 0x44e9533d, "find_vpid" },
	{ 0xb01f6010, "pid_task" },
	{ 0x349cba85, "strchr" },
	{ 0x2d39b0a7, "kstrdup" },
	{ 0x1e6d26a8, "strstr" },
	{ 0xa036ba5e, "get_task_mm" },
	{ 0x35daa920, "put_pid" },
	{ 0x30e1078a, "get_pid_task" },
	{ 0xa2db5d0e, "find_get_pid" },
	{ 0x8f2ab7fc, "mmput" },
	{ 0x51e77c97, "pfn_valid" },
	{ 0x999e8297, "vfree" },
	{ 0xd6ee688f, "vmalloc" },
	{ 0x37a0cba, "kfree" },
	{ 0xaff9e63c, "input_set_abs_params" },
	{ 0xc25ff85a, "put_device" },
	{ 0xdf72be1b, "get_device" },
	{ 0x2cdc3025, "class_for_each_device" },
	{ 0xc5850110, "printk" },
	{ 0xe8b268ae, "mutex_unlock" },
	{ 0xeb9065d9, "mutex_lock" },
};

MODULE_INFO(depends, "");

