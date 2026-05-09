
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <uapi/linux/sched/types.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/kallsyms.h>

#include "io_struct.h"
#include "export_fun.h"
#include "inline_hook_frame.h"
#include "physical.h"
#include "hwbp.h"
#include "virtual_input.h"
#include "process_memory_enum.h"

static struct req_obj *req = NULL;

static bool ProcessExit = false; // 用户进程默认未启动
static bool KThreadExit = true;	 // 内核线程默认启用

static int DispatchThreadFunction(void *data)
{
	// 自旋计数器：用来记录我们空转了多久
	int spin_count = 0;

	while (KThreadExit)
	{
		if (ProcessExit)
		{
			// 确实有任务
			if (req->kernel)
			{

				// 不保存中断+恢复中断状态，这里强行关闭所有中断，后续在强行打开所有
				asm volatile("msr daifset, #0xf\n" ::: "memory");

				req->kernel = false; // 清除请求标志

				// 有活干，重置计数器
				spin_count = 0;

				switch (req->op)
				{
				case op_o:
					break;
				case op_r:
					req->status = read_process_memory(req->pid, req->target_addr, req->user_buffer, req->size);
					break;
				case op_w:
					req->status = write_process_memory(req->pid, req->target_addr, req->user_buffer, req->size);
					break;
				case op_m:
					req->status = enum_process_memory(req->pid, &req->mem_info);
					break;
				case op_down:
				case op_move:
				case op_up:
					v_touch_event(req->op, req->slot, req->x, req->y);
					break;
				case op_init_touch:
					req->status = v_touch_init(&req->POSITION_X, &req->POSITION_Y);
					break;
				case op_brps_weps_info:
					get_hw_breakpoint_info(&req->bp_info);
					break;
				case op_set_process_hwbp:
					req->status = set_process_hwbp(req->pid, req->target_addr, req->bt, req->bl, req->bs, &req->bp_info);
					break;
				case op_remove_process_hwbp:
					remove_process_hwbp();
					break;
				case op_kexit:
					KThreadExit = false; // 标记内核线程退出
					break;
				default:
					break;
				}

				req->user = true; // 通知用户层完成
				asm volatile("msr daifclr, #0xf\n" ::: "memory");
			}
			else
			{
				// 暂时没活干

				// 策略：前 5000 次循环死等（极速响应），超过后才睡觉
				if (spin_count < 5000)
				{
					spin_count++;
					cpu_relax(); // 告诉 CPU 我在忙等，降低功耗
				}
				else
				{
					// 既不占 CPU，也能快速醒来
					usleep_range(50, 100);

					// 这里不要重置 spin_count，
					// 保持睡眠状态直到下一个任务到来，做到了有任务超高性能响应，没任务超低消耗;
				}
			}
		}
		else
		{
			// 还没连接到进程，深睡眠
			msleep(2000);
		}
	}
	return 0;
}

static int ConnectThreadFunction(void *data)
{
	struct task_struct *task;
	struct mm_struct *mm = NULL;
	struct page **pages = NULL;
	int num_pages;
	int ret;

	// 和内核线程在运行
	while (KThreadExit)
	{
		// 请求进程处于未启用
		if (!ProcessExit)
		{
			// 遍历系统中所有进程,//这里不加RCU锁，不然会导致6.6以上超时
			for_each_process(task)
			{
				if (__builtin_strcmp(task->comm, "LS") != 0)
					continue;

				// 获取进程的内存描述符
				mm = get_task_mm(task);
				if (!mm)
					continue;

				// 计算页数
				num_pages = (sizeof(struct req_obj) + PAGE_SIZE - 1) / PAGE_SIZE;

				// 分配页指针数组
				pages = kmalloc_array(num_pages, sizeof(struct page *), GFP_KERNEL);
				if (!pages)
				{
					pr_debug("kmalloc_array 失败\n");
					goto out_put_mm;
				}

				// 远程获取用户空间地址对应的物理页（将用户地址映射到内核）
				mmap_read_lock(mm);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 12, 0) // 内核 6.12
				ret = get_user_pages_remote(mm, 0x2025827000, num_pages, FOLL_WRITE, pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0)	 // 内核 6.5 到 6.12
				ret = get_user_pages_remote(mm, 0x2025827000, num_pages, FOLL_WRITE, pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)	 // 内核 6.1 到 6.5
				ret = get_user_pages_remote(mm, 0x2025827000, num_pages, FOLL_WRITE, pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0) // 内核 5.15 到 6.1
				ret = get_user_pages_remote(mm, 0x2025827000, num_pages, FOLL_WRITE, pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) // 内核 5.10 到 5.15
				ret = get_user_pages_remote(mm, 0x2025827000, num_pages, FOLL_WRITE, pages, NULL, NULL);
#endif
				mmap_read_unlock(mm);

				if (ret < num_pages)
				{
					pr_debug("get_user_pages_remote 失败, ret=%d\n", ret);
					goto out_put_pages;
				}

				// 映射到内核虚拟地址
				req = vmap(pages, num_pages, VM_MAP, PAGE_KERNEL);
				if (!req)
				{
					pr_debug("vmap 失败\n");
					goto out_put_pages;
				}

				// 成功 get_user_pages_remote 持有页面引用，只需释放 mm
				ProcessExit = true; // 标记用户进程已连接
				req->user = true;	// 通知用户层已连接
				kfree(pages);
				pages = NULL;
				mmput(mm);
				mm = NULL;
				break; // 找到目标进程，退出遍历

			out_put_pages:
				release_gup_pages(pages, ret);
				kfree(pages);
				pages = NULL;

			out_put_mm:
				mmput(mm);
				mm = NULL;
			}
		}

		msleep(2000);
	}

	return 0;
}

// do_exit 执行前的 inline hook 工作函数
static void do_exit_hook_work(void)
{
	// 调用 do_exit 的进程就是当前正在运行并准备死去的进程 (current)
	struct task_struct *task = current;

	// 只监听主线程的退出
	if (!thread_group_leader(task))
		return;

	// 匹配进程名
	// Android 中 task->comm 最长只有 15 个字符，包名被截断
	// 比如 "com.ss.android.LS" 可能会变成 "com.ss.android."
	if (__builtin_strstr(task->comm, "ls") != NULL || __builtin_strstr(task->comm, "LS") != NULL)
	{
		pr_debug("【进程监听】检测到 LS 进程即将退出！PID: %d, 进程名(comm): %s\n", task->pid, task->comm);

		// 相应处理
		read_process_memory(666666, 1, &ProcessExit, 1); // 主动调用一下释放缓存的mm
		v_touch_destroy();								 // 清理触摸
		inline_hook_remove_all();						 // 清理所有hook
		ProcessExit = false;							 // 标记用户进程已断开,前面read借用了ProcessExit，这里最后置为false，保证状态正确
	}
}
static int do_exit_init(void)
{
	static struct hook_entry do_exit_hook[] = {
		HOOK_ENTRY("do_exit", do_exit_hook_work),
	};

	int ret;

	ret = inline_hook_install(do_exit_hook);
	if (ret < 0)
	{
		pr_err("安装 inline hook(do_exit) 失败，错误码: %d\n", ret);
		return ret;
	}

	pr_debug("成功：inline hook(do_exit) 已安装，开始监听 LS 退出。\n");
	return 0;
}

// 隐藏内核模块
static void hide_myself(void)
{
	// 内核模块结构体
	struct module_use *use, *tmp;
	// 小于内核 6.12才能隐藏vmap_area_list和_vmap_area_root，高版本移除了这个数据结构，由https://github.com/wenyounb，发现
#if LINUX_VERSION_CODE < KERNEL_VERSION(6, 12, 0)
	struct vmap_area *va, *vtmp;
	struct list_head *_vmap_area_list;
	struct rb_root *_vmap_area_root;

	_vmap_area_list = (struct list_head *)generic_kallsyms_lookup_name("vmap_area_list");
	_vmap_area_root = (struct rb_root *)generic_kallsyms_lookup_name("vmap_area_root");

	// 摘除vmalloc调用关系链，/proc/vmallocinfo中不可见
	list_for_each_entry_safe(va, vtmp, _vmap_area_list, list)
	{
		if ((uint64_t)THIS_MODULE > va->va_start && (uint64_t)THIS_MODULE < va->va_end)
		{
			list_del(&va->list);
			// rbtree中摘除，无法通过rbtree找到
			rb_erase(&va->rb_node, _vmap_area_root);
		}
	}

#endif

	// 摘除链表，/proc/modules 中不可见。
	list_del_init(&THIS_MODULE->list);
	// 摘除kobj，/sys/modules/中不可见。
	kobject_del(&THIS_MODULE->mkobj.kobj);
	// 摘除依赖关系，本例中nf_conntrack的holder中不可见。
	list_for_each_entry_safe(use, tmp, &THIS_MODULE->target_list, target_list)
	{
		list_del(&use->source_list);
		list_del(&use->target_list);
		sysfs_remove_link(use->target->holders_dir, THIS_MODULE->name);
		kfree(use);
	}
}
// 隐藏内核线程
static void hide_kthread(struct task_struct *task)
{
	if (!task)
		return;

	// 下面detach_pid_func隐藏没问题，但是线程运行起来没身份会立马死机，现在无法解决
	typedef void (*detach_pid_t)(struct task_struct *task, enum pid_type type);
	detach_pid_t detach_pid_func;
	detach_pid_func = (detach_pid_t)generic_kallsyms_lookup_name("detach_pid");
	if (detach_pid_func)
	{

		// detach_pid_func(chf, PIDTYPE_PID);
		// detach_pid_func(dhf, PIDTYPE_PID);
		pr_debug("隐藏内核线程成功。\n");
	}
	else
	{
		pr_debug("严重错误！无法找到 detach_pid 函数地址。将不做隐藏运行\n");
	}

	// tasks 链表
	list_del_init(&task->tasks);
}

static int __init lsdriver_init(void)
{
	struct task_struct *chf;
	struct task_struct *dhf;

	bypass_cfi(); // 先尝试绕过 5系的cfi

	hide_myself(); // 隐藏内核模块本身

	allocate_physical_page_info(); // pte读写需要，线性读写不需要 // 初始化物理页地址和页表项

	chf = kthread_run(ConnectThreadFunction, NULL, "C_thread");
	if (IS_ERR(chf))
	{
		pr_debug("创建连接线程失败\n");
		return PTR_ERR(chf);
	}

	dhf = kthread_run(DispatchThreadFunction, NULL, "D_thread");
	if (IS_ERR(dhf))
	{
		pr_debug("创建调度线程失败\n");
		return PTR_ERR(dhf);
	}

	// 注册回调
	do_exit_init();

	// 隐藏内核线程
	hide_kthread(chf);
	hide_kthread(dhf);

	return 0;
}
static void __exit lsdriver_exit(void)
{
	// 模块已隐藏，此函数不会被调用
}

module_init(lsdriver_init);
module_exit(lsdriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Liao");

/*
闲聊
统一物理寻址空间(Unified Physical Address Space)
	CPU 有一个巨大的物理地址寻址空间 (比如寻址40 位长度的空间)
	CPU 会把这些物理地址划分成不同的区域，分配给不同的硬件：
	典型的就是骁龙ARM SoC 的物理内存
		0x00000000 ~ 0x07FFFFFF	Boot ROM / 内部SRAM	      固件代码,芯片内部的启动代码
		0x08000000 ~ 0x1FFFFFFF	外设寄存器 (MMIO 区)	   外设硬件寄存器地址
		0x20000000 ~ 0x7FFFFFFF	保留区 / PCI-E 映射	       其他用途
		0x80000000 ~ 0xFFFFFFFF 主 DRAM (运行内存/内存条)  运行时数据存放地址
	在现代mmu开启的情况下cpu只能寻址虚拟地址
	    比如一个usb设备物理地址是a600000
		需要用ioremap(0xa600000,4096);来映射物理地址到虚拟地址，
		然后 CPU 读写虚拟地址时被mmu拦截，由mmu转为物理地址
		然后由 CPU 的总线接口单元(Bus Interface Unit)把这个物理地址，连同你要写的数据，打包放到 AXI 总线（或其升级版如 CHI 等片上互联总线）上。
		然后由 AXI 总线内部的地址解码器转为电信号，把这个电信号送到 USB 控制器的寄存器物理引脚上
		最后寄存器写入会触发芯片内部的数字逻辑电路完成硬件工作
		
		这就是通过读写虚拟地址来控制设备

内核子系统
        VFS 子系统
		   常见挂载目录
		   /proc    进程信息,内核状态,能查看部分设备/总线信息是历史遗留接口，现在标准是/sys
		   /sys     设备模型,驱动,总线,类,是设备对象信息
		   /dev     设备节点,用户程序读写操作设备的设备节点
		   /run     运行时状态文件
		   /dev/pts 伪终端设备
		input 子系统
			cat /proc/bus/input/devices   查看输入设备
			ls /dev/input/                查看设备节点
			ls /sys/class/input/
		perf 子系统
			我这里不使用，最常用于硬件断点
		usb 子系统
			ls /sys/class/udc/            查看最底层USB设备控制器
			   output: a600000.dwc3
					   a600000:           焊死在USB 控制器寄存器的物理地址。CPU 通过向这个地址读写数据，来控制 USB 接口的收发。
					   dwc3:			  全称是 DesignWare Cores USB 3.0。这是半导体巨头 Synopsys（新思科技）设计的 USB 控制器 IP 核心。几乎所有现代的高通骁龙芯片内部，都集成了这个 dwc3 硬件模块来管理底层的 Type-C 接口。
			getprop | grep sys.usb.config 查看上层的Android配置的usb模式
		gnss 子系统
*/