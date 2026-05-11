

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


*/

static int gnss_insert_raw_hook_work(struct gnss_device *gdev, const unsigned char *buf, size_t count)
{

    
}

static int gnss_insert_raw_init(void)
{
    static struct hook_entry gnss_insert_raw_hook[] = {
        HOOK_ENTRY("gnss_insert_raw", gnss_insert_raw_hook_work),
    };

    int ret;

    ret = inline_hook_install(gnss_insert_raw_hook);
    if (ret < 0)
    {
        pr_debug("安装 inline hook(gnss_insert_raw) 失败，错误码: %d\n", ret);
        return ret;
    }

    pr_debug("成功：inline hook(gnss_insert_raw) 已安装，开始监听 LS 退出。\n");
    return 0;
}
