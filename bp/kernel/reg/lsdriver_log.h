#ifndef LSDRIVER_LOG_H
#define LSDRIVER_LOG_H

#include <linux/printk.h>

#define ls_log(fmt, ...)          pr_debug("lsdriver: " fmt, ##__VA_ARGS__)
#define ls_log_tag(tag, fmt, ...) pr_debug("lsdriver: %s: " fmt, tag, ##__VA_ARGS__)

// 直接以 KERN_INFO 写入 printk 环形缓冲区，不依赖 DEBUG 或 dynamic_debug，适合必须保留的监控日志；
static __printf(2, 3) void ls_log_always_tag(const char *tag, const char *fmt, ...)
{
    struct va_format vaf;
    va_list args;

    va_start(args, fmt);
    vaf.fmt = fmt;
    vaf.va = &args;
    printk(KERN_INFO "lsdriver: %s: %pV", tag, &vaf);
    va_end(args);
}

#endif // LSDRIVER_LOG_H