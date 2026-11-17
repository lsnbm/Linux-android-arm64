#ifndef PAGE_PINNER_OVERRIDE_H
#define PAGE_PINNER_OVERRIDE_H

/*
本地 6.18 构建内核配置启用了 CONFIG_PAGE_PINNER=y，generated/autoconf.h 因而
定义 CONFIG_PAGE_PINNER 为 1。put_page() 的内联调用链会进入 page_pinner_put_page()，
使模块导入 page_pinner_inited 和 __page_pinner_put_page 两个页面跟踪调试符号。
目标设备内核无法解析这两个导出时，模块加载报 Unresolved symbol，返回 ENOENT。
即使运行时未开启跟踪，加载时仍需解析这些符号；

只在 Makefile 中设置 CONFIG_PAGE_PINNER=n 不会取消生成配置中的 C 宏；命令行 -U
又先于强制包含头文件处理，会被 autoconf.h 重新定义。因此先包含 linux/kconfig.h，
再取消该宏，并通过 Makefile 的 -include 保证这一步发生在页面管理头文件展开之前。

此覆盖仅跳过本模块中的可选 PAGE_PINNER 调试跟踪，保留正常页面引用计数和最终释放。
好处是：不修改运行中的内核，不修改内核构建目录的配置文件。
*/
#include <linux/kconfig.h>
#undef CONFIG_PAGE_PINNER

#endif