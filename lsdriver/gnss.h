

#include <linux/printk.h>

#include "inline_hook_frame.h"

/*
gnss芯片
    |
不同硬件/总线驱动收集硬件数据
串口 serdev: serial.c / mtk.c / ubx.c / sirf.c
USB: usb.c
    |
gnss_insert_raw(gdev, buf, count)不同底层驱动统一调用gnss_insert_raw上报
    |
int gnss_insert_raw(struct gnss_device *gdev, const unsigned char *buf,size_t count)
{
    int ret;
     将底层 GNSS 接收到的原始数据buf写入读 FIFO。
    ret = kfifo_in(&gdev->read_fifo, buf, count);

    wake_up_interruptible(&gdev->read_queue);

    return ret;
}
    |
/dev/gnssN
    |
用户态 GNSS HAL / daemon / 应用侧服务


gnss_insert_raw:
  标准 Linux GNSS raw 字节流路径。现代设备不用它了。
tty/serdev:
  串口 GNSS 才可能走。手机 SoC 基带定位通常不走。
unix_stream_sendmsg / sock_sendmsg:
  能看到 HAL/LocSvc 的 socket 数据，但只是字节流，协议私有，且高通/MTK 不通用。
qmi / glink / qrtr:
  高通专用传输层，能更底层，但不是通用 Android 定位接口，解析成本很高。

binder_transaction:
  最通用。App、system_server、GNSS HAL 都通过 Binder/AIDL 交互。


所以还是hook用户态的进程，内核没法
*/
