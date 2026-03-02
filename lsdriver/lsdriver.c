
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>
#include <linux/list.h>
#include <linux/kobject.h>
#include <linux/kallsyms.h>

#include "ExportFun.h"
#include "physical.h"

typedef enum _req_op
{
	op_o = 0, // 空调用
	op_r = 1,
	op_w = 2,
	op_m = 3, // 获取进程内存信息

	op_down = 4,
	op_move = 5,
	op_up = 6,
	op_InitTouch = 50, // 初始化触摸
	op_DelTouch = 60,  // 清理触摸触摸

	exit = 100,
	kexit = 200
} req_op;

#include "virtual_input.h" //在这里预处理展开，方便使用req_op

// 将在队列中使用的请求实例结构体
typedef struct _req_obj
{

	atomic_t Kernel; // 由用户模式设置 1 = 内核有待处理的请求, 0 = 请求已完成
	atomic_t User;	 // 由内核模式设置 1 = 用户模式有待处理的请求, 0 = 请求已完成

	req_op Op;	// 请求操作类型
	int Status; // 操作状态

	// 内存读取
	int TargetProcessId;
	uint64_t TargetAddress;
	int TransferSize;
	char UserBufferAddress[0x1000]; // 物理标准页大小

	// 进程内存信息
	struct memory_info MemoryInfo;

	// 初始化触摸驱动返回屏幕维度
	int POSITION_X, POSITION_Y;
	// 触摸坐标
	int x, y;

} Requests;

static Requests *req = NULL;

volatile static bool ProcessExit = 0; // 用户进程默认未启动

volatile static bool KThreadExit = 1; // 内核线程默认启用

static int DispatchThreadFunction(void *data)
{
	// 自旋计数器：用来记录我们空转了多久
	int spin_count = 0;

	while (KThreadExit)
	{
		if (ProcessExit)
		{
			// 先“偷看”一眼有没有任务 (atomic_read 开销极小)
			// 避免每次循环都执行 atomic_xchg (写操作，锁总线，慢)
			if (req && atomic_read(&req->Kernel) == 1)
			{
				// 确实有任务，尝试获取锁
				if (atomic_xchg(&req->Kernel, 0) == 1)
				{
					// 有活干，重置计数器
					spin_count = 0;

					switch (req->Op)
					{
					case op_o:
						break;
					case op_r:
						req->Status = read_process_memory(req->TargetProcessId, req->TargetAddress, req->UserBufferAddress, req->TransferSize);
						break;
					case op_w:
						req->Status = write_process_memory(req->TargetProcessId, req->TargetAddress, req->UserBufferAddress, req->TransferSize);
						break;
					case op_m:
						req->Status = enum_process_memory(req->TargetProcessId, &req->MemoryInfo);
						break;
					case op_down:
					case op_move:
					case op_up:
						v_touch_event(req->Op, req->x, req->y);
						break;
					case op_InitTouch:
						v_touch_init(&req->POSITION_X, &req->POSITION_Y);
						break;
					case op_DelTouch:
						v_touch_destroy();
						break;
					case exit:
						ProcessExit = 0;
						break;
					case kexit:
						KThreadExit = 0;
						break;
					default:
						break;
					}

					// 通知用户层完成
					atomic_set(&req->User, 1);
				}
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
	int i;

	// 和内核线程在运行
	while (KThreadExit)
	{
		// 请求进程处于未启用
		if (!ProcessExit)
		{
			// 遍历系统中所有进程,//这里不加RCU锁，不然会导致6.6以上超时
			for_each_process(task)
			{
				if (strcmp(task->comm, "Lark") != 0)
					continue;

				// 获取进程的内存描述符
				mm = get_task_mm(task);
				if (!mm)
					continue;

				// 计算页数
				num_pages = (sizeof(Requests) + PAGE_SIZE - 1) / PAGE_SIZE;

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

				// 成功：vmap 持有页面引用，只需释放 mm
				ProcessExit = 1;
				atomic_xchg(&req->User, 1);
				mmput(mm);
				break; // 找到目标进程，退出遍历

			out_put_pages:
				for (i = 0; i < ret; i++)
					put_page(pages[i]);
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

// 隐藏内核模块
static void hide_myself(void)
{
	struct vmap_area *va, *vtmp;
	struct module_use *use, *tmp;
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
	// tasks 链表
	list_del_init(&task->tasks);
}

static int __init lsdriver_init(void)
{
	struct task_struct *chf;
	struct task_struct *dhf;

	hide_myself(); // 隐藏内核模块本身

	// allocate_physical_page_info();//pte读写需要，线性读写不需要 // 初始化物理页地址和页表项

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

	// 隐藏内核线程
	hide_kthread(chf);
	hide_kthread(dhf);

	// // 下面隐藏没问题，但是线程运行起来没身份会立马死机，现在无法解决
	// typedef void (*detach_pid_t)(struct task_struct *task, enum pid_type type);
	// detach_pid_t detach_pid_func;
	// detach_pid_func = (detach_pid_t)generic_kallsyms_lookup_name("detach_pid");
	// if (detach_pid_func)
	// {

	// 	detach_pid_func(chf, PIDTYPE_PID);
	// 	detach_pid_func(dhf, PIDTYPE_PID);
	// 	pr_debug("隐藏内核线程成功。\n");
	// }
	// else
	// {
	// 	pr_debug("严重错误！无法找到 detach_pid 函数地址。将不做隐藏运行\n");
	// }

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
