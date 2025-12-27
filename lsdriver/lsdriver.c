
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/sched.h>
#include <linux/kthread.h>
#include <linux/delay.h>

#include <linux/mm.h>
#include <linux/pagemap.h>
#include <linux/slab.h>
#include <linux/vmalloc.h>

#include <linux/list.h> // 内核链表操作
#include <linux/kobject.h>

#include <linux/kallsyms.h>

#include "ExportFun.h"
#include "virtual_input.h"

#include "physical.h"

typedef enum _req_op
{
	op_o = 0, // 空调用
	op_r = 1,
	op_w = 2,
	op_m = 3,

	op_down = 4,
	op_move = 5,
	op_up = 6,
	op_InitTouch = 50, // 初始化触摸
	op_DelTouch = 60,  // 清理触摸触摸

	exit = 100,
	kexit = 200
} req_op;

// 将在队列中使用的请求实例结构体
typedef struct _req_Obj
{
	atomic_t kernel; // 由用户模式设置 1 = 内核有待处理的请求, 0 = 请求已完成
	atomic_t user;	 // 由内核模式设置 1 = 用户模式有待处理的请求, 0 = 请求已完成

	req_op Op;	// 请求操作类型
	int status; // 操作状态

	// 内存读取
	int TargetProcessId;
	uint64_t TargetAddress;
	int TransferSize;
	char UserBufferAddress[1024];

	// 模块基地址获取
	char ModuleName[46];
	short SegmentIndex; // 模块区段
	uint64_t ModuleBaseAddress;
	uint64_t ModuleSize;

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

	// 内核线程启用
	while (KThreadExit)
	{
		if (ProcessExit)
		{
			// 先“偷看”一眼有没有任务 (atomic_read 开销极小)
			// 避免每次循环都执行 atomic_xchg (写操作，锁总线，慢)
			if (req && atomic_read(&req->kernel) == 1)
			{
				// 确实有任务，尝试获取锁
				if (atomic_xchg(&req->kernel, 0) == 1)
				{
					// === 有活干，重置计数器 ===
					spin_count = 0;

					switch (req->Op)
					{
					case op_o:
						break;
					case op_r:
						req->status = read_process_memory(req->TargetProcessId, req->TargetAddress, req->UserBufferAddress, req->TransferSize);
						break;
					case op_w:
						req->status = write_process_memory(req->TargetProcessId, req->TargetAddress, req->UserBufferAddress, req->TransferSize);
						break;
					case op_m:
						req->status = get_module_base(req->TargetProcessId, req->ModuleName, req->SegmentIndex, &req->ModuleBaseAddress);
						break;
					case op_down:
						v_touch_down(req->x, req->y);
						break;
					case op_move:
						v_touch_move(req->x, req->y);
						break;
					case op_up:
						v_touch_up();
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
					atomic_set(&req->user, 1);
				}
			}
			else
			{
				// === 暂时没活干 ===

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
	static struct mm_struct *g_mm = NULL;
	static struct page **g_pages = NULL;
	int g_num_pages = 0;

	int ret = 0;
	int i = 0;
	// 和内核线程在运行
	while (KThreadExit)
	{
		// 请求进程处于未启用
		if (!ProcessExit)
		{
			//// 启用 RCU 读锁(6.6内核会锁超时导致阻塞，其他内核版本不会，所以都不加)
			// rcu_read_lock();

			// 遍历系统中所有进程
			for_each_process(task)
			{
				if (strcmp(task->comm, "Lark") == 0)
				{
					pr_debug("发现目标进程 '%s'，PID 为 %d。\n", task->comm, task->pid);

					// 获取进程的内存描述符
					g_mm = get_task_mm(task);

					if (g_mm)
					{
						// 计算页数
						g_num_pages = (sizeof(Requests) + PAGE_SIZE - 1) / PAGE_SIZE;

						// 分配页指针数组
						g_pages = kmalloc_array(g_num_pages, sizeof(struct page *), GFP_KERNEL);
						if (!g_pages)
						{
							// 分配失败，跳转到仅释放 mm 的标签
							goto out_mm;
						}

						// 加 mm 读锁
						mmap_read_lock(g_mm);

						// 远程获取用户空间地址对应的物理页（将用户地址映射到内核）
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0) // 内核 6.5 及以上
						ret = pin_user_pages_remote(g_mm, 0x2025827000, g_num_pages, FOLL_WRITE | FOLL_LONGTERM, g_pages, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)	 // 内核 6.1 到 6.5
						ret = get_user_pages_remote(g_mm, 0x2025827000, g_num_pages, FOLL_WRITE, g_pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0) // 内核 5.15 到 6.1
						ret = get_user_pages_remote(g_mm, 0x2025827000, g_num_pages, FOLL_WRITE, g_pages, NULL, NULL);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) // 内核 5.10 到 5.15
						ret = get_user_pages_remote(g_mm, 0x2025827000, g_num_pages, FOLL_WRITE, g_pages, NULL, NULL);
#endif
						// 释放 mm 读锁
						mmap_read_unlock(g_mm);

						// 检查映射是否成功
						if (ret < g_num_pages)
						{
							pr_debug("为 PID %d 调用 get_user_pages_remote 失败 (返回值为 %d)。\n", task->pid, ret);
							goto out_free_pages;
						}

						// 映射到内核虚拟地址
						req = vmap(g_pages, g_num_pages, VM_MAP, PAGE_KERNEL);
						if (!req)
						{
							goto out_free_pages;
						}

						pr_debug("成功：已将 PID %d 的地址 0x%llx 映射到内核地址 %p。\n", task->pid, (uint64_t)0x20258270000, req);

						ProcessExit = 1;
						atomic_xchg(&req->user, 1);

						// 成功路径：不需要释放页面和数组（vmap持有），仅需释放 mm
						goto out_mm;

						// --- 统一资源释放区域 ---

					out_free_pages:
						// 释放已获取的物理页引用
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 5, 0) // 内核 6.5 及以上
						if (ret > 0)
							unpin_user_pages(g_pages, ret);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(6, 1, 0)	 // 内核 6.1 到 6.5
						for (i = 0; i < ret; i++)
							put_page(g_pages[i]);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 15, 0) // 内核 5.15 到 6.1
						for (i = 0; i < ret; i++)
							put_page(g_pages[i]);
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(5, 10, 0) // 内核 5.10 到 5.15
						for (i = 0; i < ret; i++)
							put_page(g_pages[i]);
#endif

						kfree(g_pages);
						g_pages = NULL;

						mmput(g_mm);
						g_mm = NULL;
						continue; // 失败后继续下一个进程查找

					out_mm:

						mmput(g_mm); // 释放 mm 引用,继续循环
					}
				}
			}

			//// 释放 RCU 读锁
			// rcu_read_unlock();
		}

		// 休眠2秒
		msleep(2000);
	}
	return 0;
}

static void __attribute__((unused)) hide_myself(void)
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

static int __init lsdriver_init(void)
{

	//---------初始化操作-------------
	// allocate_physical_page_info();//pte读写需要，线性读写不需要 // 初始化物理页地址和页表项

	static struct task_struct *chf;
	static struct task_struct *dhf;
	static void (*detach_pid)(struct pid *, enum pid_type);

	chf = kthread_run(ConnectThreadFunction, NULL, "C_thread");
	if (IS_ERR(chf))
	{
		pr_debug("创建连接线程(C_thread)失败。\n");
		return PTR_ERR(chf);
	}
	dhf = kthread_run(DispatchThreadFunction, NULL, "D_thread");
	if (IS_ERR(dhf))
	{
		pr_debug("创建调度线程(D_thread)失败。\n");
		return PTR_ERR(dhf);
	}

	//------------隐藏操作---------------------

	detach_pid = (void (*)(struct pid *, enum pid_type))generic_kallsyms_lookup_name("detach_pid");
	if (!detach_pid)
	{
		pr_debug("严重错误！无法找到 detach_pid 函数地址。将不做隐藏运行\n");
	}
	// 隐藏内核线程
	if (detach_pid)
	{
		// struct pid *chf_pid, *dhf_pid;

		// 从任务列表中移除，这使得任务在ps等命令中不可见
		list_del_init(&chf->tasks);
		list_del_init(&dhf->tasks);

		// //  从 task_struct 获取 pid 结构
		// chf_pid = task_pid(chf);
		// dhf_pid = task_pid(dhf);

		// 从 PID 哈希表中分离,PIDTYPE_PID表示操作的是主 PID
		// detach_pid(chf_pid, PIDTYPE_PID);	//会死机，现在无法解决
		// detach_pid(dhf_pid, PIDTYPE_PID);
	}

	// // 隐藏内核模块本身
	hide_myself();

	return 0;
}

static void __exit lsdriver_exit(void)
{
	// 由于模块已经被隐藏，这个退出函数永远不会被调用。
	// 实际的清理工作无法通过常规方式执行。
}

module_init(lsdriver_init);
module_exit(lsdriver_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Liao");
