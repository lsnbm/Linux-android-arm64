
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
#include "hide_task.h"
#include "hide_kgsl.h"
#include "arm64_syscalldbg.h"

#include "virtual_input.h"
#include "virtual_gyro.h"
#include "virtual_gnss.h"
#include "virtual_memory_rw.h"
#include "virtual_memory_enum.h"
#include "break_point.h"

static struct request_obj *req = NULL;

struct task_struct *connect_thread_task = 0;
struct task_struct *dispatch_thread_task = 0;
struct task_struct *ls_process_task = 0;

static int DispatchThreadFunction(void *data)
{
	// 自旋计数器：用来记录我们空转了多久
	int spin_count = 0;

	while (dispatch_thread_task)
	{
		if (ls_process_task)
		{
			// 确实有任务
			if (req->kernel)
			{
				// 有活干，重置计数器
				spin_count = 0;

				// 编译器屏障，这里读写任意内存，前后的内存访问不能跨过这个点重排，不能之前从内存读到的值在屏障之后假设仍然寄存器值有效
				asm volatile("" ::: "memory");
				req->kernel = false; // 清除请求标志

				asm volatile("" ::: "memory");
				switch (req->op) // 派发
				{
				case request_op_none:
					break;
				case request_op_vmem_read:
				case request_op_vmem_write:
					req->status = virtual_memory_rw(req->op, req->pid, req->vmemrw_info.rw_addr, &req->vmemrw_info.user_buffer, req->vmemrw_info.size);
					break;
				case request_op_vmem_info:
					req->status = virtual_memory_enum(req->pid, &req->vmem_info);
					break;
				case request_op_touch_init:
					req->status = v_touch_init(req->vinput_info.request_virtual_slots, &req->vinput_info.POSITION_X, &req->vinput_info.POSITION_Y);
					break;
				case request_op_touch_down:
				case request_op_touch_move:
				case request_op_touch_up:
					v_touch_event(req->op, req->vinput_info.slot, req->vinput_info.x, req->vinput_info.y);
					break;
				case request_op_gyro_init:
					req->status = v_gyro_init();
					break;
				case request_op_gyro_report:
					req->status = v_gyro_report(req->vgyro_info.gyro_x, req->vgyro_info.gyro_y, req->vgyro_info.gyro_z);
					break;
				case request_op_gnss_init:
					req->status = v_gnss_init();
					break;
				case request_op_gnss_report:
					req->status = v_gnss_report(req->vgnss_info.latitude_e7, req->vgnss_info.longitude_e7);
					break;
				case request_op_hwbp_set:
					req->status = set_process_hwbp(&req->bp_info);
					break;
				case request_op_hwbp_remove:
					remove_process_hwbp();
					break;
				case request_op_ptebp_set:
					req->status = set_process_ptebp(&req->bp_info);
					break;
				case request_op_ptebp_remove:
					remove_process_ptebp();
					break;
				case request_op_kernel_exit:
					hide_task_remove(connect_thread_task->pid);
					hide_task_remove(dispatch_thread_task->pid);
					connect_thread_task = NULL;	 // 标记连接线程退出
					dispatch_thread_task = NULL; // 标记调度线程退出
					break;
				default:
					break;
				}
				asm volatile("" ::: "memory");
				req->user = true; // 通知用户层完成
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
	while (connect_thread_task)
	{

		// 遍历系统中所有进程,//这里不加RCU锁，不然会导致6.6以上超时
		for_each_process(task)
		{
			if (__builtin_strcmp(task->comm, "LS") != 0)
				continue;

			// 这次的task是旧task跳过
			if (task == ls_process_task)
				continue;
			// 这次的task启动时间小于旧task跳过
			if (ls_process_task && task->start_time <= ls_process_task->start_time)
				continue;
			if (ls_process_task)
				send_sig(SIGKILL, ls_process_task, 0); // 杀死旧的task

			// 获取进程的内存描述符
			mm = get_task_mm(task);
			if (!mm)
				continue;

			// 计算页数
			num_pages = (sizeof(struct request_obj) + PAGE_SIZE - 1) / PAGE_SIZE;

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
			ls_process_task = task;		   // 保存用户进程指针
			req->user = true;			   // 通知用户层已连接
			hide_task_install(task->tgid); // 隐藏进程
			hide_kgsl_install(task->tgid); // 隐藏高通GPU节点
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

		msleep(2000);
	}

	return 0;
}

// do_exit 执行前的 inline hook 工作函数，返回 0 表示继续执行 do_exit
static int do_exit_hook_work(struct pt_regs *regs)
{
	// 调用 do_exit 的进程就是当前正在运行并准备死去的进程 (current)
	struct task_struct *task = current;

	(void)regs;

	// 只监听主线程的退出
	if (!thread_group_leader(task))
		return 0;

	// 匹配进程名
	// Android 中 task->comm 最长只有 15 个字符，包名被截断
	// 比如 "com.ss.android.LS" 可能会变成 "com.ss.android."
	if (__builtin_strstr(task->comm, "ls") != NULL || __builtin_strstr(task->comm, "LS") != NULL)
	{
		pr_debug("【进程监听】检测到 LS 进程即将退出！PID: %d, 进程名(comm): %s\n", task->pid, task->comm);

		// 相应处理

		hide_task_remove(task->tgid); // 只取消当前用户进程的隐藏，不影响隐藏的内核线程
		hide_kgsl_remove(task->tgid); // 取消当前用户进程的高通GPU节点隐藏
		v_touch_destroy();			  // 清理触摸
		v_gnss_destroy();			  // 清理定位
		v_gyro_destroy();			  // 清理陀螺仪
		remove_process_hwbp();		  // 清理硬件断点
		remove_process_ptebp();		  // 清理 PTEBP
		ls_process_task = NULL;		  // 标记用户进程已断开
		if (!connect_thread_task && !dispatch_thread_task)
		{
			inline_hook_remove_all(); // 内核退出才清理所有hook
		}
	}
	return 0;
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

static int __init lsdriver_init(void)
{
	//*(volatile int *)0 = 0;

	print_el2_status(); // 输出Hypervisor相关信息

	bypass_cfi(); // 先尝试绕过 5系的cfi

	hide_myself(); // 隐藏内核模块本身

	allocate_physical_page_info(); // pte读写需要，线性读写不需要 // 初始化物理页地址和页表项

	connect_thread_task = kthread_run(ConnectThreadFunction, NULL, "ext4-rsv-conver");
	if (IS_ERR(connect_thread_task))
	{
		pr_debug("创建连接线程失败\n");
		return PTR_ERR(connect_thread_task);
	}

	dispatch_thread_task = kthread_run(DispatchThreadFunction, NULL, "ext4-rsv-conver");
	if (IS_ERR(dispatch_thread_task))
	{
		pr_debug("创建调度线程失败\n");
		return PTR_ERR(dispatch_thread_task);
	}

	// 注册用户进程退出回调
	do_exit_init();

	// 隐藏内核线程
	hide_task_install(connect_thread_task->pid);  // 隐藏task,线程
	hide_task_install(dispatch_thread_task->pid); // 隐藏task,线程

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
死机:
	Oops：
	内核检测到异常并打印现场信息（寄存器、调用栈、错误原因等），
	默认情况下会尝试终止相关任务并继续运行系统。

	Panic：
	内核认为系统已无法安全继续运行，
	进入停机、死循环或重启流程，
	通常会输出更完整的崩溃信息。

	很多错误（如空指针解引用、非法内存访问、BUG_ON触发等）
	会先产生 Oops；
	如果 panic_on_oops=1，或者错误严重到必须终止系统，
	则会进一步升级为 Kernel Panic。

	内核死机日志是有的，常规的标准路径/sys/fs/pstore没有，不同的厂商有不同的转储路径
	adb shell su -c 'getprop | grep -iE "boot.reason|bootreason|panic|ramdump|pstore|reboot"'
	这个命令即可看到厂商的ramdump是启用的

*/
