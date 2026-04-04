
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

#include "io_struct.h"
#include "export_fun.h"
#include "physical.h"
#include "hwbp.h"
#include "virtual_input.h"
#include "process_memory_enum.h"

static struct req_obj *req = NULL;

static atomic_t ProcessExit = ATOMIC_INIT(0); // 用户进程默认未启动
static atomic_t KThreadExit = ATOMIC_INIT(1); // 内核线程默认启用

static int DispatchThreadFunction(void *data)
{
	// 自旋计数器：用来记录我们空转了多久
	int spin_count = 0;

	while (atomic_read(&KThreadExit))
	{
		if (atomic_read(&ProcessExit))
		{
			// 先“偷看”一眼有没有任务 (atomic_read 开销极小)
			// 避免每次循环都执行 atomic_xchg (写操作，锁总线，慢)
			if (req && atomic_read(&req->kernel) == 1)
			{
				// 确实有任务，尝试获取锁
				if (atomic_xchg(&req->kernel, 0) == 1)
				{
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
						v_touch_event(req->op, req->x, req->y);
						break;
					case op_init_touch:
						req->status = v_touch_init(&req->POSITION_X, &req->POSITION_Y);
						break;
					case op_brps_weps_info:
						get_hw_breakpoint_info(&req->bp_info);
						break;
					case op_set_process_hwbp:
						req->status = set_process_hwbp(req->pid, req->target_addr, req->bt, req->bs, req->len_bytes, &req->bp_info);
						break;
					case op_remove_process_hwbp:
						remove_process_hwbp();
						break;
					case op_kexit:
						atomic_xchg(&KThreadExit, 0); // 标记内核线程退出
						break;
					default:
						break;
					}

					// 通知用户层完成
					atomic_set(&req->user, 1); // 这里不要使用atomic_xchg锁总线，抢锁
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
	while (atomic_read(&KThreadExit))
	{
		// 请求进程处于未启用
		if (!atomic_read(&ProcessExit))
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
				atomic_xchg(&ProcessExit, 1); // 标记用户进程已连接
				atomic_xchg(&req->user, 1);	  // 通知用户层已连接
				kfree(pages);
				pages = NULL;
				mmput(mm);
				mm = NULL;
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

// do_exit 执行前的回调函数
static int handler_pre(struct kprobe *p, struct pt_regs *regs)
{
	// 调用 do_exit 的进程就是当前正在运行并准备死去的进程 (current)
	struct task_struct *task = current;

	// 只监听主线程的退出
	if (!thread_group_leader(task))
		return 0;

	// 匹配进程名
	// Android 中 task->comm 最长只有 15 个字符，包名极可能被截断！
	// 比如 "com.ss.android.LS" 可能会变成 "com.ss.android."
	// 不是Android包名程序除外
	if (__builtin_strstr(task->comm, "ls") != NULL || __builtin_strstr(task->comm, "LS") != NULL)
	{

		pr_debug("【进程监听】检测到 LS 进程即将退出！PID: %d, 进程名(comm): %s\n", task->pid, task->comm);

		// 相应处理
		atomic_xchg(&ProcessExit, 0);	 // 标记用户进程已断开
		read_process_memory(1, 0, 0, 0); // 主动调用一下释放缓存的mm
		v_touch_destroy();				 // 清理触摸
	}

	return 0;
}
static int kprobe_do_exit_init(void)
{
	static struct kprobe kp = {
		.symbol_name = "do_exit",
	};
	int ret;

	// 绑定回调函数
	kp.pre_handler = handler_pre;

	// 注册 kprobe
	ret = register_kprobe(&kp);
	if (ret < 0)
	{
		pr_err("注册 kprobe(do_exit) 失败，错误码: %d\n", ret);
		return ret;
	}

	pr_debug("成功：Kprobe(do_exit) 已注册，开始监听 LS 退出。\n");
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
	// tasks 链表
	list_del_init(&task->tasks);
}

static int __init lsdriver_init(void)
{
	struct task_struct *chf;
	struct task_struct *dhf;

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
	kprobe_do_exit_init();

	// 隐藏内核线程
	hide_kthread(chf);
	hide_kthread(dhf);

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
