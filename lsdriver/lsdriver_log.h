#ifndef LSDRIVER_LOG_H
#define LSDRIVER_LOG_H

#include <linux/printk.h>

#define ls_log(fmt, ...) pr_debug("lsdriver: " fmt, ##__VA_ARGS__)
#define ls_log_tag(tag, fmt, ...) pr_debug("lsdriver: %s: " fmt, tag, ##__VA_ARGS__)

#endif // LSDRIVER_LOG_H