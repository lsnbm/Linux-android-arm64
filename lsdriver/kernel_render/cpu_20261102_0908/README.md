# CPU DRM 渲染实现说明

## 不可行
自己创建 DRM client、dumb framebuffer 并调用 drm_client_modeset_commit()。这条路径要求当前 DRM client 能够提交 modeset 或 atomic state，但 Android 启动后 DRM master 通常已经由 SurfaceFlinger/HWC 持有，因此旧实现常见的结果是：

drm_client_modeset_commit()
     -> DRM master 检查
     -> -EBUSY

所以必须hook,最好的位置性能最优的就是现在活动文件夹代码的逻辑

## 1. 目标

CPU renderer 的目标是在内核态直接绘制 framebuffer，并通过 Linux DRM/KMS 将绘制结果提交到 Android 显示管线。

当前实现面向 Qualcomm 和 MediaTek 等使用 Linux DRM 的平台，使用 DRM 通用接口，不直接调用 Qualcomm `msm_*` 或 MediaTek `mtk_*` 私有 GEM、CRTC、plane 接口。



## 2. 总体架构

```text
CPU 绘制请求
    |
    v
cpu_render.c
    |  全局初始化、退出、操作转发和控制锁
    v
 drm_cpu_render.c
    |  DRM client、framebuffer、CPU 映射和 modeset
    v
Linux DRM Core
    |
    +-- Qualcomm msm DRM driver
    |
    +-- MediaTek mediatek DRM driver
    |
    +-- 其他厂商 DRM driver
    |
    v
显示控制器、合成器和面板
```

`cpu_raster.c` 只负责对线性像素内存进行 CPU 光栅化，不知道底层 GPU、DPU、显示控制器或厂商驱动的实现。

## 3. 为什么可以跨 Qualcomm 和 MediaTek

Linux DRM 将显示硬件分成通用核心和厂商驱动两层。

厂商驱动负责硬件细节，例如：

- 显示控制器寄存器和提交队列；
- CRTC、plane、connector 和 panel；
- GEM/dumb buffer 的分配和映射；
- 具体的 atomic 或 legacy modeset 提交。

DRM Core 对上层提供统一的 `drm_device`、DRM client、framebuffer 和 modeset 接口。CPU renderer 只使用这些公共接口，因此不需要知道 Qualcomm 或 MediaTek 的私有对象布局。

实际提交过程由底层厂商 DRM driver 的回调完成：

```text
统一 drm_client_modeset_commit()
    -> DRM Core
        -> 厂商 DRM atomic/legacy 提交
            -> 显示控制器
                -> 面板
```

因此，厂商差异被限制在 DRM driver 内部

## 4. 直接初始化和 KMI 兼容

当前只有一个 CPU renderer backend，因此 `cpu_render_init()` 直接选择 DRM client ops，DRM client 的符号解析、DRM minor 获取、模式探测和 framebuffer 创建都由 backend 初始化完成；任一步骤失败就返回错误。

当前实现固定获取 DRM primary minor `0`。在单显示设备的 Android 手机上通常对应主显示设备；多 GPU 或多个 DRM card 的设备若要更通用，需要进一步枚举并选择目标 DRM device。

`drm_cpu_render_api.h` 集中定义 DRM API 函数指针表。`drm_cpu_render.c` 在
renderer 初始化阶段统一填充 `drm_cpu_render_api`，后续获取设备、创建资源、
绘制和退出路径只调用 `drm_cpu_render_api.*` 表项。

处理规则如下：

1. 所有 DRM 和 minor 符号统一通过 `generic_kallsyms_lookup_name()` 动态获取地址；
2. 目标内核源码中存在但没有纳入实际设备 KMI 的符号也能通过同一路径尝试获取；
3. 任一必需函数解析失败，renderer 初始化失败，不创建不完整的 DRM client。

这样可以避免旧内核因为缺少非 KMI DRM client 导出而在模块加载时出现 `Unknown symbol`，同时保持同一套上层逻辑。

## 5. DRM client 初始化和 framebuffer 创建

绑定输出的流程位于 `drm_cpu_render_init()`，初始化成功返回时已经完成：

1. 通过 `drm_minor_acquire` 获取 DRM minor 和 `drm_device`；
2. 调用 `drm_client_init` 创建未注册的 DRM client；
3. 调用 `drm_client_modeset_probe` 探测可用输出和显示模式；
4. 从第一个可用 modeset 获取 framebuffer 宽度和高度；
5. 调用 `drm_client_framebuffer_create` 创建 dumb framebuffer；
6. 调用 `drm_client_buffer_vmap` 获取 CPU 可访问地址；
7. 将 client buffer 的 framebuffer 设置到每个 client modeset 的 `mode_set->fb`；
8. 绘制初始画面并调用 `drm_client_framebuffer_flush`；
9. 调用 `drm_client_modeset_commit` 提交显示。

创建 framebuffer 使用 `DRM_FORMAT_ARGB8888`。pitch 使用 DRM 返回值，CPU raster 不假设每行没有填充。

当前不调用 `drm_client_register`，因此 client 不加入 DRM device 的 client list，也不会依赖 device unregister callback 自动清理。renderer 自己管理 client 的创建、buffer、映射和释放顺序。

## 6. CPU raster 绘制

`cpu_raster.c` 对 DRM client 映射出的线性像素内存进行绘制，支持：

- 清屏；
- 带厚度的线段；
- 填充或描边矩形；
- 填充或描边圆形；
- 基于内置 5x7 字模的数字、英文字母和少量标点文字；
- 初始 demo 画面。

光栅层只接收以下信息：

```text
pixels + width + height + pitch + 绘制参数
```

它不访问 `drm_device`、GEM object、CRTC、plane 或厂商私有结构，因此可以独立复用。

## 7. 跨内核版本的 buffer 映射 ABI

不同 Android 内核版本的 `drm_client_buffer_vmap` 签名不同，差异被限制在 backend 内部：

| 内核版本 | 映射形式 |
| --- | --- |
| 5.10 | 返回 `void *` |
| 5.15 | 使用 `struct dma_buf_map` |
| 6.1 及以后 | 使用 `struct iosys_map` |

上层绘制逻辑统一使用 `void *pixels`。这样 CPU raster 不需要包含版本判断，也不需要知道不同内核的 map 结构。

## 8. 绘制同步

绘制操作由 `draw_lock` 串行化。每次绘制完成后：

1. 执行写内存屏障；
2. 调用 `drm_client_framebuffer_flush(buffer, NULL)` 通知 framebuffer dirty 路径；
3. 释放绘制锁。

初始 demo 在 modeset commit 前也执行 flush。flush 由底层 framebuffer dirty 回调负责处理可能存在的缓存或 DMA 同步需求。

## 9. 生命周期和失败处理

全局生命周期由 `cpu_render.c` 的 `cpu_render_control_lock` 保护，backend 自身使用 `initialized`、`attached`、`client_initialized` 和 `mapped` 状态。

`acquire_output()` 负责获取和创建显示资源，`release_output()` 是幂等的统一
释放入口。初始化失败和正常退出都按逆序释放已创建资源：

```text
drm_client_buffer_vunmap
    -> drm_client_framebuffer_delete
        -> drm_client_release
            -> drm_minor_release
```

退出流程为：

1. 阻止新的绘制操作；
2. 调用 `drm_client_modeset_dpms(..., DRM_MODE_DPMS_OFF)`；
3. 解除 buffer 映射并删除 framebuffer；
4. 释放 DRM client 和 minor 引用。

如果关闭 modeset 或其他清理步骤失败，不会提前清除初始化状态，后续可以重试退出，避免丢失仍然有效的 DRM 资源引用。


## 10. 相关文件

| 文件 | 作用 |
| --- | --- |
| `cpu_render.c` | 全局 renderer 管理、操作转发和控制锁 |
| `cpu_render.h` | 公共 CPU renderer 接口 |
| `drm_cpu_render_api.h` | 统一的跨版本 DRM API 函数指针表 |
| `drm_cpu_render.c` | DRM client backend、buffer 生命周期和 modeset |
| `cpu_raster.c` | 与平台无关的 CPU 光栅绘制 |
| `cpu_raster.h` | CPU raster 函数声明 |
