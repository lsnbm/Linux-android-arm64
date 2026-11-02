# CPU DRM Framebuffer Renderer

## 1. 目标

renderer 使用 CPU 绘制一张透明 overlay 画布，并在 Android 的 DRM atomic 提交进入厂商驱动前，将画布直接 alpha 混合到本次提交使用的线性 primary framebuffer。它不创建独立 DRM framebuffer、plane 或用户态 Surface，也不申请 DRM master。

当前 backend 使用 Qualcomm MSM/SDE 的 GEM 映射接口。MediaTek 和其他 DRM 驱动不能直接复用该 buffer ABI。

## 2. 显示路径

```text
CPU raster
    -> 内核虚拟 overlay 画布
    -> atomic_commit 回调
    -> 本次提交的 primary framebuffer GEM 映射
    -> alpha 混合写入已有显示缓存
    -> 原 Qualcomm MSM/SDE atomic_commit
    -> DSI 面板
```

SurfaceFlinger/HWC 仍然拥有 DRM master。renderer 不抢占显示管线，只在原厂商提交前修改当前 primary framebuffer 的像素内容。

## 3. 初始化流程

`drm_cpu_render_init()` 只完成以下步骤：

1. 动态解析 DRM minor 和 MSM GEM 映射接口；
2. 获取 DRM primary minor，并确认驱动名为 `msm` 或 `msm_drm`；
3. 分配一张内核虚拟透明 overlay 画布；
4. 绘制初始图像；
5. 复制 `drm_mode_config_funcs`，只替换 `atomic_commit`；
6. 固定当前模块引用，防止回调执行期间代码被卸载。

renderer 不调用 `drm_client_modeset_commit()`，不创建 GEM scanout 对象，也不创建 DRM framebuffer。

## 4. 提交路径

每次目标 DRM 设备执行 atomic commit 时：

```text
HWC atomic state
    -> drm_cpu_render_atomic_commit()
    -> 遍历本次提交的 primary plane
    -> 检查 ARGB/XRGB8888、linear modifier 和 stride
    -> 映射 fb->obj[0]
    -> 将 overlay 画布混合到 framebuffer
    -> wmb()
    -> 调用原始 atomic_commit()
```

当前实现只处理满足以下条件的 framebuffer：

- plane 类型为 `DRM_PLANE_TYPE_PRIMARY`；
- 格式为 `ARGB8888` 或 `XRGB8888`；
- modifier 为 linear；
- 具有可访问的 `fb->obj[0]`；
- `msm_gem_get_vaddr()` 成功。

如果当前 buffer 是 UBWC、tile、protected 或其他不可 CPU 访问布局，renderer 会跳过本次提交，不修改 Android 图像。

## 5. 绘制和同步

CPU 绘制操作只修改内核虚拟 overlay 画布。每次绘制完成后执行 `wmb()`。atomic commit 回调持有 `draw_lock`，直到原始厂商提交返回，避免退出路径释放 hook 时仍有并发绘制或提交。

overlay 画布使用预乘 alpha 语义。透明区域保留下方 Android 内容，非透明像素通过 CPU alpha 混合写入当前 primary framebuffer。

## 6. 代码职责

| 文件 | 职责 |
| --- | --- |
| `cpu_render.c` | 公共生命周期、控制锁和绘制接口转发 |
| `drm_cpu_render.c` | 当前 framebuffer 映射、动态 DRM/MSM API、atomic commit hook 和资源释放 |
| `cpu_raster.c` | CPU 光栅绘制和 framebuffer alpha 混合 |
| `cpu_raster.h` | 光栅函数声明 |

## 7. 限制

- 当前只实现 Qualcomm MSM/SDE；
- 只支持 linear ARGB8888/XRGB8888 primary framebuffer；
- 不支持 UBWC、tile、protected 或无法 CPU 映射的 framebuffer；
- 画面会进入 Android 当前 framebuffer，因此可能出现在截图或录屏中；
- HWC 下一次提交可能覆盖之前的混合结果，renderer 会在下一次 atomic commit 再次处理；
- hook 安装期间模块代码和回调表不能被卸载，退出必须先恢复原回调表，再释放 overlay 画布和 DRM minor 引用；
- 如果原子提交尚未等待 GPU producer fence，CPU 写入可能与 GPU 生产者发生竞争，真机上必须验证目标 MSM/HWC 的 fence 时序。
