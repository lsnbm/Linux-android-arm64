/*
Android 12-17 SurfaceControl 适配与维护说明（目标 ABI：arm64-v8a）

一、当前如何覆盖版本
     本文件通过 dlopen/dlsym 调用 libgui.so、libutils.so 的私有 C++ 接口，
     不是只改 Android 版本判断，也不意味着所有厂商 ROM 都已通过验证。
     Functionals 优先读取 ro.build.version.sdk：31/32 对应 12/12L，
     33-37 对应 13-17；未识别的 SDK 才回退解析 ro.build.version.release。
     关键接口按设备实际导出的完整符号选择，并使用对应函数指针调用：

     12/12L：setLayerStack(uint32_t)，apply(bool)，mirrorSurface(source)，
                 createSurface 使用 android::LayerMetadata 值参数。
     13：    setLayerStack(ui::LayerStack)，apply(bool, bool)，镜像仍为单参数，
                 createSurface 仍使用 android::LayerMetadata 值参数。
     14-16：事务和镜像接口沿用 13 的形式，createSurface 的 flags 改为 int32_t，
                 metadata 改为 android::gui::LayerMetadata 值参数。
     17：    mirrorSurface(source, stopAt, cropBy)，后两项显式传 nullptr；
                 createSurface 的 metadata 改为 const android::gui::LayerMetadata&。
     getSurface 优先解析 const 成员符号，缺失时尝试旧非 const 符号；
     显示 token 路径在 12-13 使用 internal token，14+ 使用 physical display ID。
     mirrorSurface 名称长度是 13，不能写成 12；默认参数也会改变 C++ 符号和 ABI。

二、截图与录屏开关
     RenderVK::init(allowCapture) 中 true 表示允许捕获，不会自动开始录屏；
     传给本文件 Create 的 preventCapture 为 !allowCapture，两个布尔含义相反。
     12+ 的 preventCapture=true 会设置 skip-screenshot 标志 0x40。
     允许录屏时，由 Vulkan 后端检测虚拟显示，将主 Lark 层镜像到目标 layerStack。
    镜像必须 reparent 到可见的 LarkRecord 容器，再对容器设置裁剪和投影；
    仅调用 mirrorSurface、setLayerStack、show 不足以保证它参与录屏合成。
     SupportsRecordLayerStack 检查初始化及镜像/事务接口是否齐备，不能仅凭版本通过；
     缺失 apply 返回 -ENOSYS。FindRecordDisplay 当前依赖 dumpsys display 的字段，
     只选择非主 layerStack；厂商录屏实现或输出格式变化时也需要重新验证此路径。

三、后续 Android/ROM 更新的适配步骤
     1. 核对新系统 SDK/release 属性，更新 Functionals 的 SDK 映射；不要直接把
         未知版本当作 17，也不要仅复制上一版本符号表就宣称支持。
     2. 对照目标 AOSP tag 和厂商实现的 libs/gui/include/gui/SurfaceComposerClient.h、
         SurfaceControl.h、LayerMetadata.h 及相应实现，检查参数、命名空间、const、
         值/引用传递、返回值和成员函数 this。参考仓库：
         https://android.googlesource.com/platform/frameworks/native/
     3. 用目标声明和 NDK 编译器生成修饰名；从真机拉取 libgui.so/libutils.so，
         用 llvm-nm -D --defined-only 和 llvm-cxxfilt 核对实际导出，再验证 dlsym。
         新签名应新增独立函数指针和调用分支，并更新 initSuccess/能力检查；
         不要把新符号强转成旧签名。构造、析构与引用计数接口需成套核对。
     4. 继续检查本文件的 StrongPointer 返回约定、DisplayState 布局、对象缓冲区
         大小/对齐以及 GetSurface 的基类偏移。符号存在不保证私有对象布局兼容；
         同时核对 ParseDumpDisplayInfo/FindRecordDisplay 的实际虚拟显示输出。
     5. 构建：E:\android-ndk-r29\ndk-build.cmd -C android LS_KTool -j12。
         然后在目标版本真机验证 true/false 两种捕获策略、截图、录屏成片、横竖屏、
         录屏启停/重启、图层释放和长期运行，检查应用、SurfaceFlinger、system_server。

四、验证边界（2026-09-25）
     已对照 12-17 的 AOSP 接口声明，42 项模拟导出/失败分支测试通过；
     RMX3888 Android 14/API 34 的真实镜像创建/释放两轮通过。
     本次挂载与投影修复后，用户已确认 Oplus 系统自带录屏的竖屏和横屏成片正常；
     32 组旋转/缩放/偏移角点测试通过，验证期间 SurfaceFlinger/system_server 未重启。
     其他系统版本、其他 ROM、反向横屏成片、禁止捕获和长期稳定性仍需分别实测。
     独立探针正常退出曾出现 VsyncReceiver destroyed-mutex，源层基线也复现；
     使用 _exit 仅隔离了静态退出问题，不表示正式程序的生命周期问题已解决。

五、本次录屏故障的发现、定位与修复
     症状：allowCapture=true 时录屏没有 ImGui；修复挂载后竖屏正常，横屏仍裁切。
     调查时保留主程序界面和系统自带录屏同时运行的现场，不用 adb screenrecord
     替代 Oplus 录屏。先比对部署文件 SHA256，再按以下顺序检查：
         adb shell dumpsys display
         adb shell dumpsys SurfaceFlinger --list
         adb shell dumpsys SurfaceFlinger
     display 用于核对 OPLUSScreenRecording 的 layerStack、orientation 和矩形；
     --list 只能确认图层存在，完整 dump 才能检查 parent、activeBuffer、可见区域、
     geomLayerTransform 和 Virtual Display 的实际合成条目。pidof 没有输出时不能
     单凭进程名判断程序未运行，还应核对图层所有者 PID 和持续更新的 buffer。

     根因 1：动态符号误写成 12mirrorSurface，正确名称长度是 13。
         真机 libgui 导出和 dlsym 对照证明旧符号缺失、新符号存在。修正后能创建镜像，
         但这仅证明接口可调用；NDK 编译和模拟导出测试都不能证明录屏成片有内容。

     根因 2：AOSP SurfaceFlinger::mirrorLayer 设置 mirrorArgs.addToRoot=false。
         镜像创建后脱离可见根层级，仅 setLayerStack/show 仍不会显示。
         CreateMirrorOnLayerStack 现在创建 eFXSurfaceContainer 类型的 LarkRecord，
         设置目标 layerStack 和层级，再将 MirrorRoot reparent 到该容器；
         DestroyMirrorOnLayerStack 同时释放镜像与容器。修复后录屏合成列表出现
         Lark 的镜像和有效 buffer，用户确认竖屏成片恢复。

     根因 3：独立镜像不会继承系统 ContentRecorder 对其他镜像根层施加的变换。
         横屏现场主屏为 2376x1080、orientation=1，Oplus 录屏仍为 1080x2376、
         orientation=0；系统 MirrorRoot 位于 (1080,0)，原 LarkRecord 却仍在原点，
         导致横屏内容直接进入竖屏坐标而被裁切。
         CalculateMirrorProjection 以主屏与目标方向差计算四方向旋转，按目标
         layerStackRect 等比缩放并居中；ConfigureMirrorProjection 对 LarkRecord
         设置源逻辑尺寸裁剪、setMatrix 和 setPosition。目标显示自身的 displayRect
         投影仍交给 SurfaceFlinger，不重复施加。EnsureRecordSurface 同时比较源方向、
         尺寸和目标矩形，保证 layerStack 未变化时也会更新投影。

     矩阵顺序通过 AOSP Layer::setMatrix 和 ui::Transform::transform 核实：
         x' = dsdx*x + dtdy*y + tx，y' = dtdx*x + dsdy*y + ty。
         setMatrix 参数顺序为 dsdx, dtdx, dtdy, dsdy，交叉项不能按行排列误传。
         本次横屏映射为 x'=1080-y、y'=x。部署后实际 dump 显示 ROT_90、
         源裁剪 2376x1080、输出可见区域 1080x2376，与系统录屏变换一致；
         最后检查保存视频并确认完整显示、方向正常
    后续回归也应按“接口、层级、合成、成片”分别取证，镜像句柄非空或事务返回成功不等同于录屏正常。
*/

#ifndef SURFACE_CONTROL_MANAGER_H
#define SURFACE_CONTROL_MANAGER_H

#include "../logger/logger.h"
#include <android/native_window.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/system_properties.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <unordered_map>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// 解析符号的宏
#define ResolveMethod(ClassName, MethodName, Handle, MethodSignature)                                                              \
    ClassName##__##MethodName = reinterpret_cast<decltype(ClassName##__##MethodName)>(symbolMethod.Find(Handle, MethodSignature)); \
    if (nullptr == ClassName##__##MethodName)                                                                                      \
    {                                                                                                                              \
        LS_LOGE_TAG("Surface", "Method not found: %s -> %s::%s", MethodSignature, #ClassName, #MethodName);                        \
    }

// 如果函数指针为空，直接返回默认值，不崩溃
#define SAFE_CALL_RET(FuncPtr, DefaultRet, ...)                    \
    if (FuncPtr != nullptr)                                        \
    {                                                              \
        return FuncPtr(__VA_ARGS__);                               \
    }                                                              \
    else                                                           \
    {                                                              \
        LS_LOGE_TAG("Surface", "Skip call: %s is null", #FuncPtr); \
        return DefaultRet;                                         \
    }

// 如果函数指针为空，直接返回，不崩溃（用于void函数）
#define SAFE_CALL_VOID(FuncPtr, ...)                               \
    if (FuncPtr != nullptr)                                        \
    {                                                              \
        FuncPtr(__VA_ARGS__);                                      \
    }                                                              \
    else                                                           \
    {                                                              \
        LS_LOGE_TAG("Surface", "Skip call: %s is null", #FuncPtr); \
        return;                                                    \
    }

namespace android
{
    namespace detail
    {
        namespace ui
        {
            struct LayerStack
            {
                uint32_t id = UINT32_MAX;
            };
            enum class Rotation
            {
                Rotation0 = 0,
                Rotation90 = 1,
                Rotation180 = 2,
                Rotation270 = 3
            };
            struct Size
            {
                int32_t width = -1;
                int32_t height = -1;
            };
            struct Rect
            {
                int32_t left;
                int32_t top;
                int32_t right;
                int32_t bottom;
            };
            struct DisplayState
            {
                LayerStack layerStack;
                Rotation orientation = Rotation::Rotation0;
                Size layerStackSpaceRect;
            };
            typedef int64_t nsecs_t;
            struct DisplayInfo
            {
                uint32_t w{0};
                uint32_t h{0};
                float xdpi{0};
                float ydpi{0};
                float fps{0};
                float density{0};
                uint8_t orientation{0};
                bool secure{false};
                nsecs_t appVsyncOffset{0};
                nsecs_t presentationDeadline{0};
                uint32_t viewportW{0};
                uint32_t viewportH{0};
            };
            enum class DisplayType
            {
                DisplayIdMain = 0,
                DisplayIdHdmi = 1
            };
            struct PhysicalDisplayId
            {
                uint64_t value;
            };
        } // namespace ui

        struct Surface
        {
        };

        template <typename any_t> struct StrongPointer
        {
            any_t *pointer;

            StrongPointer() : pointer(nullptr)
            {
            }

            StrongPointer(const StrongPointer &other) : pointer(other.pointer)
            {
            }

            StrongPointer &operator=(const StrongPointer &other)
            {
                pointer = other.pointer;
                return *this;
            }

            ~StrongPointer()
            {
            }

            inline any_t *operator->() const
            {
                return pointer;
            }
            inline any_t *get() const
            {
                return pointer;
            }
            inline explicit operator bool() const
            {
                return nullptr != pointer;
            }
        };

        struct Functionals
        {
            struct SymbolMethod
            {
                void *(*Open)(const char *filename, int flag) = nullptr;
                void *(*Find)(void *handle, const char *symbol) = nullptr;
                int (*Close)(void *handle) = nullptr;
            };

            size_t systemVersion = 0;
            bool initSuccess = false; // 标记初始化是否成功
            void *libguiHandle = nullptr;
            void *libutilsHandle = nullptr;

            void (*RefBase__IncStrong)(void *thiz, void *id) = nullptr;
            void (*RefBase__DecStrong)(void *thiz, void *id) = nullptr;

            void (*String8__Constructor)(void *thiz, const char *const data) = nullptr;
            void (*String8__Destructor)(void *thiz) = nullptr;

            void (*LayerMetadata__Constructor)(void *thiz) = nullptr;
            void (*LayerMetadata__setInt32)(void *thiz, uint32_t key, int32_t value) = nullptr;

            void (*SurfaceComposerClient__Constructor)(void *thiz) = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__CreateSurface)(void *thiz, void *name, uint32_t w, uint32_t h, int32_t format, uint32_t flags, void *parentHandle, void *layerMetadata, uint32_t *outTransformHint) = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__CreateSurfaceWithMetadataRef)(void *thiz, void *name, uint32_t w, uint32_t h, int32_t format, int32_t flags, void *parentHandle, const void *layerMetadata, uint32_t *outTransformHint) = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__CreateSurface_and9)(void *thiz, void *name, uint32_t w, uint32_t h, int32_t format, uint32_t flags, void *parentHandle, int32_t windowType, int32_t ownerUid) = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__MirrorSurface)(void *thiz, void *surfaceControl) = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__MirrorSurfaceWithBounds)(void *thiz, void *surfaceControl, void *stopAt, void *cropBy) = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__GetInternalDisplayToken)() = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__GetBuiltInDisplay)(ui::DisplayType type) = nullptr;
            int32_t (*SurfaceComposerClient__GetDisplayState)(StrongPointer<void> &display, ui::DisplayState *displayState) = nullptr;
            int32_t (*SurfaceComposerClient__GetDisplayInfo)(StrongPointer<void> &display, ui::DisplayInfo *displayInfo) = nullptr;
            std::vector<ui::PhysicalDisplayId> (*SurfaceComposerClient__GetPhysicalDisplayIds)() = nullptr;
            StrongPointer<void> (*SurfaceComposerClient__GetPhysicalDisplayToken)(ui::PhysicalDisplayId displayId) = nullptr;

            void (*SurfaceComposerClient__Transaction__Constructor)(void *thiz) = nullptr;
            void *(*SurfaceComposerClient__Transaction__SetLayer)(void *thiz, StrongPointer<void> &surfaceControl, int32_t z) = nullptr;
            void *(*SurfaceComposerClient__Transaction__SetTrustedOverlay)(void *thiz, StrongPointer<void> &surfaceControl, bool isTrustedOverlay) = nullptr;
            void *(*SurfaceComposerClient__Transaction__SetLayerStack)(void *thiz, StrongPointer<void> &surfaceControl, ui::LayerStack layerStack) = nullptr;
            void *(*SurfaceComposerClient__Transaction__SetLayerStackLegacy)(void *thiz, StrongPointer<void> &surfaceControl, uint32_t layerStack) = nullptr;
            void *(*SurfaceComposerClient__Transaction__Show)(void *thiz, StrongPointer<void> &surfaceControl) = nullptr;
            void *(*SurfaceComposerClient__Transaction__Reparent)(void *thiz, StrongPointer<void> &surfaceControl, StrongPointer<void> &parent) = nullptr;
            void *(*SurfaceComposerClient__Transaction__SetMatrix)(void *thiz, StrongPointer<void> &surfaceControl, float dsdx, float dtdx, float dtdy, float dsdy) = nullptr;
            void *(*SurfaceComposerClient__Transaction__SetPosition)(void *thiz, StrongPointer<void> &surfaceControl, float x, float y) = nullptr;
            void *(*SurfaceComposerClient__Transaction__SetCrop)(void *thiz, StrongPointer<void> &surfaceControl, const ui::Rect &crop) = nullptr;
            int32_t (*SurfaceComposerClient__Transaction__ApplyLegacy)(void *thiz, bool synchronous) = nullptr;
            int32_t (*SurfaceComposerClient__Transaction__Apply)(void *thiz, bool synchronous, bool oneWay) = nullptr;

            StrongPointer<Surface> (*SurfaceControl__GetSurface)(void *thiz) = nullptr;
            void (*SurfaceControl__DisConnect)(void *thiz) = nullptr;

            Functionals(const SymbolMethod &symbolMethod)
            {
                char sdkString[PROP_VALUE_MAX]{};
                __system_property_get("ro.build.version.sdk", sdkString);
                const int sdk = std::atoi(sdkString);
                if (sdk == 31 || sdk == 32) systemVersion = 12;
                else if (sdk >= 33 && sdk <= 37) systemVersion = static_cast<size_t>(sdk - 20);

                std::string systemVersionString(128, 0);
                systemVersionString.resize(__system_property_get("ro.build.version.release", systemVersionString.data()));
                if (systemVersion == 0 && !systemVersionString.empty())
                {
                    try
                    {
                        systemVersion = std::stoi(systemVersionString);
                    }
                    catch (...)
                    {
                        LS_LOGE_TAG("Surface", "无法解析 Android 版本: %s", systemVersionString.c_str());
                        return;
                    }
                }
                if (systemVersion == 0) return;

                LS_LOGD_TAG("Surface", "Android version=%zu", systemVersion);

                std::unordered_map<size_t, std::unordered_map<void **, const char *>> patchesTable = {
                    {
                        11,
                        {
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlENS_13LayerMetadataEPj"},
                            {reinterpret_cast<void **>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv"},
                        },
                    },
                    {
                        10,
                        {
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlENS_13LayerMetadataE"},
                            {reinterpret_cast<void **>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv"},
                        },
                    },
                    {
                        9,
                        {
                            {reinterpret_cast<void **>(&SurfaceComposerClient__CreateSurface_and9), "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijPNS_14SurfaceControlEii"},
                            {reinterpret_cast<void **>(&SurfaceComposerClient__GetBuiltInDisplay), "_ZN7android21SurfaceComposerClient17getBuiltInDisplayEi"},
                            {reinterpret_cast<void **>(&SurfaceControl__GetSurface), "_ZNK7android14SurfaceControl10getSurfaceEv"},
                        },
                    },
                };

#ifdef __LP64__
                auto libgui = symbolMethod.Open("/system/lib64/libgui.so", RTLD_LAZY);
                auto libutils = symbolMethod.Open("/system/lib64/libutils.so", RTLD_LAZY);
#else
                auto libgui = symbolMethod.Open("/system/lib/libgui.so", RTLD_LAZY);
                auto libutils = symbolMethod.Open("/system/lib/libutils.so", RTLD_LAZY);
#endif

                if (!libgui || !libutils)
                {
                    LS_LOGE_TAG("Surface", "无法打开 libgui 或 libutils");
                    return;
                }
                libguiHandle = libgui;
                libutilsHandle = libutils;

                ResolveMethod(RefBase, IncStrong, libutils, "_ZNK7android7RefBase9incStrongEPKv");
                ResolveMethod(RefBase, DecStrong, libutils, "_ZNK7android7RefBase9decStrongEPKv");

                ResolveMethod(String8, Constructor, libutils, "_ZN7android7String8C2EPKc");
                ResolveMethod(String8, Destructor, libutils, "_ZN7android7String8D2Ev");

                ResolveMethod(SurfaceComposerClient, Constructor, libgui, "_ZN7android21SurfaceComposerClientC2Ev");
                if (systemVersion >= 12)
                {
                    SurfaceComposerClient__CreateSurfaceWithMetadataRef = reinterpret_cast<decltype(SurfaceComposerClient__CreateSurfaceWithMetadataRef)>(symbolMethod.Find(libgui, "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEERKNS_3gui13LayerMetadataEPj"));
                    if (!SurfaceComposerClient__CreateSurfaceWithMetadataRef) SurfaceComposerClient__CreateSurface = reinterpret_cast<decltype(SurfaceComposerClient__CreateSurface)>(symbolMethod.Find(libgui, "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjiiRKNS_2spINS_7IBinderEEENS_3gui13LayerMetadataEPj"));
                    if (SurfaceComposerClient__CreateSurfaceWithMetadataRef || SurfaceComposerClient__CreateSurface)
                    {
                        ResolveMethod(LayerMetadata, Constructor, libgui, "_ZN7android3gui13LayerMetadataC2Ev");
                    }
                    else
                    {
                        ResolveMethod(SurfaceComposerClient, CreateSurface, libgui, "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijRKNS_2spINS_7IBinderEEENS_13LayerMetadataEPj");
                        ResolveMethod(LayerMetadata, Constructor, libgui, "_ZN7android13LayerMetadataC2Ev");
                    }
                }
                else
                {
                    ResolveMethod(LayerMetadata, Constructor, libgui, "_ZN7android13LayerMetadataC2Ev");
                    ResolveMethod(LayerMetadata, setInt32, libgui, "_ZN7android13LayerMetadata8setInt32Eji");
                    ResolveMethod(SurfaceComposerClient, CreateSurface, libgui, "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijRKNS_2spINS_7IBinderEEENS_13LayerMetadataEPj");
                }
                SurfaceComposerClient__MirrorSurface = reinterpret_cast<decltype(SurfaceComposerClient__MirrorSurface)>(symbolMethod.Find(libgui, "_ZN7android21SurfaceComposerClient13mirrorSurfaceEPNS_14SurfaceControlE"));
                if (!SurfaceComposerClient__MirrorSurface)
                {
                    ResolveMethod(SurfaceComposerClient, MirrorSurfaceWithBounds, libgui, "_ZN7android21SurfaceComposerClient13mirrorSurfaceEPNS_14SurfaceControlES2_S2_");
                }
                ResolveMethod(SurfaceComposerClient, GetInternalDisplayToken, libgui, "_ZN7android21SurfaceComposerClient23getInternalDisplayTokenEv");
                ResolveMethod(SurfaceComposerClient, GetDisplayState, libgui, "_ZN7android21SurfaceComposerClient15getDisplayStateERKNS_2spINS_7IBinderEEEPNS_2ui12DisplayStateE");
                ResolveMethod(SurfaceComposerClient, GetDisplayInfo, libgui, "_ZN7android21SurfaceComposerClient14getDisplayInfoERKNS_2spINS_7IBinderEEEPNS_11DisplayInfoE");
                ResolveMethod(SurfaceComposerClient, GetPhysicalDisplayIds, libgui, "_ZN7android21SurfaceComposerClient21getPhysicalDisplayIdsEv");
                ResolveMethod(SurfaceComposerClient, GetPhysicalDisplayToken, libgui, "_ZN7android21SurfaceComposerClient23getPhysicalDisplayTokenENS_17PhysicalDisplayIdE");
                ResolveMethod(SurfaceComposerClient__Transaction, Constructor, libgui, "_ZN7android21SurfaceComposerClient11TransactionC2Ev");
                ResolveMethod(SurfaceComposerClient__Transaction, SetLayer, libgui, "_ZN7android21SurfaceComposerClient11Transaction8setLayerERKNS_2spINS_14SurfaceControlEEEi");
                ResolveMethod(SurfaceComposerClient__Transaction, SetTrustedOverlay, libgui, "_ZN7android21SurfaceComposerClient11Transaction17setTrustedOverlayERKNS_2spINS_14SurfaceControlEEEb");
                SurfaceComposerClient__Transaction__SetLayerStack = reinterpret_cast<decltype(SurfaceComposerClient__Transaction__SetLayerStack)>(symbolMethod.Find(libgui, "_ZN7android21SurfaceComposerClient11Transaction13setLayerStackERKNS_2spINS_14SurfaceControlEEENS_2ui10LayerStackE"));
                if (!SurfaceComposerClient__Transaction__SetLayerStack)
                {
                    ResolveMethod(SurfaceComposerClient__Transaction, SetLayerStackLegacy, libgui, "_ZN7android21SurfaceComposerClient11Transaction13setLayerStackERKNS_2spINS_14SurfaceControlEEEj");
                }
                ResolveMethod(SurfaceComposerClient__Transaction, Show, libgui, "_ZN7android21SurfaceComposerClient11Transaction4showERKNS_2spINS_14SurfaceControlEEE");
                ResolveMethod(SurfaceComposerClient__Transaction, Reparent, libgui, "_ZN7android21SurfaceComposerClient11Transaction8reparentERKNS_2spINS_14SurfaceControlEEES6_");
                ResolveMethod(SurfaceComposerClient__Transaction, SetMatrix, libgui, "_ZN7android21SurfaceComposerClient11Transaction9setMatrixERKNS_2spINS_14SurfaceControlEEEffff");
                ResolveMethod(SurfaceComposerClient__Transaction, SetPosition, libgui, "_ZN7android21SurfaceComposerClient11Transaction11setPositionERKNS_2spINS_14SurfaceControlEEEff");
                ResolveMethod(SurfaceComposerClient__Transaction, SetCrop, libgui, "_ZN7android21SurfaceComposerClient11Transaction7setCropERKNS_2spINS_14SurfaceControlEEERKNS_4RectE");
                SurfaceComposerClient__Transaction__Apply = reinterpret_cast<decltype(SurfaceComposerClient__Transaction__Apply)>(symbolMethod.Find(libgui, "_ZN7android21SurfaceComposerClient11Transaction5applyEbb"));
                if (!SurfaceComposerClient__Transaction__Apply)
                {
                    ResolveMethod(SurfaceComposerClient__Transaction, ApplyLegacy, libgui, "_ZN7android21SurfaceComposerClient11Transaction5applyEb");
                }

                SurfaceControl__GetSurface = reinterpret_cast<decltype(SurfaceControl__GetSurface)>(symbolMethod.Find(libgui, "_ZNK7android14SurfaceControl10getSurfaceEv"));
                if (!SurfaceControl__GetSurface)
                {
                    ResolveMethod(SurfaceControl, GetSurface, libgui, "_ZN7android14SurfaceControl10getSurfaceEv");
                }
                ResolveMethod(SurfaceControl, DisConnect, libgui, "_ZN7android14SurfaceControl10disconnectEv");

                auto it = patchesTable.find(systemVersion);
                if (it != patchesTable.end())
                {
                    for (const auto &[patchTo, signature] : patchesTable.at(systemVersion))
                    {
                        void *foundSym = symbolMethod.Find(libgui, signature);
                        if (foundSym)
                        {
                            *patchTo = foundSym;
                        }
                        else
                        {
                            LS_LOGE_TAG("Surface", "补丁符号未找到: %s", signature);
                        }
                    }
                }
                const bool hasLayerMetadata = systemVersion < 10 || LayerMetadata__Constructor != nullptr;
                const bool hasSurfaceCreate = systemVersion == 9 ? SurfaceComposerClient__CreateSurface_and9 != nullptr && SurfaceComposerClient__GetBuiltInDisplay != nullptr : SurfaceComposerClient__CreateSurface != nullptr || SurfaceComposerClient__CreateSurfaceWithMetadataRef != nullptr;
                const bool hasDisplayToken = systemVersion <= 9 ? SurfaceComposerClient__GetBuiltInDisplay != nullptr : systemVersion >= 14 ? SurfaceComposerClient__GetPhysicalDisplayIds != nullptr && SurfaceComposerClient__GetPhysicalDisplayToken != nullptr : SurfaceComposerClient__GetInternalDisplayToken != nullptr;
                const bool hasDisplayInfo = systemVersion >= 11 ? SurfaceComposerClient__GetDisplayState != nullptr : SurfaceComposerClient__GetDisplayInfo != nullptr;
                const bool hasApply = SurfaceComposerClient__Transaction__Apply != nullptr || SurfaceComposerClient__Transaction__ApplyLegacy != nullptr;
                const bool hasTrustedOverlay = systemVersion < 12 || SurfaceComposerClient__Transaction__SetTrustedOverlay != nullptr;
                initSuccess = RefBase__IncStrong != nullptr && RefBase__DecStrong != nullptr && String8__Constructor != nullptr && String8__Destructor != nullptr && SurfaceComposerClient__Constructor != nullptr && SurfaceControl__GetSurface != nullptr && SurfaceControl__DisConnect != nullptr && SurfaceComposerClient__Transaction__Constructor != nullptr && SurfaceComposerClient__Transaction__SetLayer != nullptr && hasLayerMetadata && hasSurfaceCreate && hasDisplayToken && hasDisplayInfo && hasTrustedOverlay && hasApply;
                if (!initSuccess) LS_LOGE_TAG("Surface", "SurfaceComposerClient 必需符号不完整");
            }

            static const Functionals &GetInstance(const SymbolMethod &symbolMethod = {.Open = dlopen, .Find = dlsym, .Close = dlclose})
            {
                static Functionals functionals(symbolMethod);
                return functionals;
            }
        };

        struct String8
        {
            alignas(std::max_align_t) char data[1024];
            bool valid = false;
            String8(const char *const string)
            {
                auto &f = Functionals::GetInstance();
                if (f.String8__Constructor && f.String8__Destructor)
                {
                    f.String8__Constructor(data, string);
                    valid = true;
                }
            }
            ~String8()
            {
                if (valid)
                {
                    SAFE_CALL_VOID(Functionals::GetInstance().String8__Destructor, data);
                }
            }
            operator void *()
            {
                return valid ? reinterpret_cast<void *>(data) : nullptr;
            }
        };

        struct LayerMetadata
        {
            alignas(std::max_align_t) char data[1024];
            bool valid = false;
            LayerMetadata()
            {
                auto &f = Functionals::GetInstance();
                if (9 < f.systemVersion && f.LayerMetadata__Constructor)
                {
                    f.LayerMetadata__Constructor(data);
                    valid = true;
                }
            }
            void setInt32(uint32_t key, int32_t value)
            {
                // 修复悬挂else警告：加上大括号
                if (valid)
                {
                    SAFE_CALL_VOID(Functionals::GetInstance().LayerMetadata__setInt32, data, key, value);
                }
            }
            operator void *()
            {
                if (9 < Functionals::GetInstance().systemVersion && valid) return reinterpret_cast<void *>(data);
                return nullptr;
            }
        };

        struct SurfaceControl
        {
            void *data;
            SurfaceControl() : data(nullptr)
            {
            }
            SurfaceControl(void *data) : data(data)
            {
            }

            Surface *GetSurface()
            {
                if (nullptr == data) return nullptr;
                auto &f = Functionals::GetInstance();
                if (f.SurfaceControl__GetSurface == nullptr) return nullptr;
                auto result = f.SurfaceControl__GetSurface(data);
                if (result.pointer == nullptr) return nullptr;
                return reinterpret_cast<Surface *>(reinterpret_cast<size_t>(result.pointer) + sizeof(std::max_align_t) / 2);
            }

            void DisConnect()
            {
                if (nullptr == data) return;
                {
                    SAFE_CALL_VOID(Functionals::GetInstance().SurfaceControl__DisConnect, data);
                }
            }

            void DestroySurface(Surface *surface)
            {
                if (nullptr == data || nullptr == surface) return;
                auto &f = Functionals::GetInstance();
                {
                    SAFE_CALL_VOID(f.RefBase__DecStrong, reinterpret_cast<Surface *>(reinterpret_cast<size_t>(surface) - sizeof(std::max_align_t) / 2), this);
                }
                DisConnect();
                {
                    SAFE_CALL_VOID(f.RefBase__DecStrong, data, this);
                }
            }

            void DestroyMirror()
            {
                if (nullptr == data) return;
                DisConnect();
                SAFE_CALL_VOID(Functionals::GetInstance().RefBase__DecStrong, data, this);
                data = nullptr;
            }
        };

        struct SurfaceComposerClientTransaction
        {
            alignas(std::max_align_t) char data[1024];
            bool valid = false;

            SurfaceComposerClientTransaction()
            {
                auto &f = Functionals::GetInstance();
                if (f.SurfaceComposerClient__Transaction__Constructor)
                {
                    f.SurfaceComposerClient__Transaction__Constructor(data);
                    valid = true;
                }
            }

            void *SetLayer(StrongPointer<void> &surfaceControl, int32_t z)
            {
                if (!valid) return nullptr;
                SAFE_CALL_RET(Functionals::GetInstance().SurfaceComposerClient__Transaction__SetLayer, nullptr, data, surfaceControl, z);
            }

            void *SetTrustedOverlay(StrongPointer<void> &surfaceControl, bool isTrustedOverlay)
            {
                if (!valid) return nullptr;
                SAFE_CALL_RET(Functionals::GetInstance().SurfaceComposerClient__Transaction__SetTrustedOverlay, nullptr, data, surfaceControl, isTrustedOverlay);
            }

            void *SetLayerStack(StrongPointer<void> &surfaceControl, uint32_t layerStack)
            {
                if (!valid) return nullptr;
                auto &f = Functionals::GetInstance();
                if (f.SurfaceComposerClient__Transaction__SetLayerStack) return f.SurfaceComposerClient__Transaction__SetLayerStack(data, surfaceControl, ui::LayerStack{layerStack});
                SAFE_CALL_RET(f.SurfaceComposerClient__Transaction__SetLayerStackLegacy, nullptr, data, surfaceControl, layerStack);
            }

            void *Show(StrongPointer<void> &surfaceControl)
            {
                if (!valid) return nullptr;
                auto &f = Functionals::GetInstance();
                if (!f.SurfaceComposerClient__Transaction__Show) return nullptr;
                return f.SurfaceComposerClient__Transaction__Show(data, surfaceControl);
            }

            int32_t Apply(bool synchronous, bool oneWay)
            {
                if (!valid) return -ENOSYS;
                auto &f = Functionals::GetInstance();
                if (f.SurfaceComposerClient__Transaction__Apply) return f.SurfaceComposerClient__Transaction__Apply(data, synchronous, oneWay);
                SAFE_CALL_RET(f.SurfaceComposerClient__Transaction__ApplyLegacy, -ENOSYS, data, synchronous);
            }

            void *Reparent(StrongPointer<void> &surfaceControl, StrongPointer<void> &parent)
            {
                if (!valid) return nullptr;
                SAFE_CALL_RET(Functionals::GetInstance().SurfaceComposerClient__Transaction__Reparent, nullptr, data, surfaceControl, parent);
            }

            void *SetMatrix(StrongPointer<void> &surfaceControl, float dsdx, float dtdx, float dtdy, float dsdy)
            {
                if (!valid) return nullptr;
                SAFE_CALL_RET(Functionals::GetInstance().SurfaceComposerClient__Transaction__SetMatrix, nullptr, data, surfaceControl, dsdx, dtdx, dtdy, dsdy);
            }

            void *SetPosition(StrongPointer<void> &surfaceControl, float x, float y)
            {
                if (!valid) return nullptr;
                SAFE_CALL_RET(Functionals::GetInstance().SurfaceComposerClient__Transaction__SetPosition, nullptr, data, surfaceControl, x, y);
            }

            void *SetCrop(StrongPointer<void> &surfaceControl, const ui::Rect &crop)
            {
                if (!valid) return nullptr;
                SAFE_CALL_RET(Functionals::GetInstance().SurfaceComposerClient__Transaction__SetCrop, nullptr, data, surfaceControl, crop);
            }
        };

        struct SurfaceComposerClient
        {
            alignas(std::max_align_t) char data[1024];
            bool valid = false;
            StrongPointer<void> displayToken{};

            SurfaceComposerClient()
            {
                auto &f = Functionals::GetInstance();
                if (!f.initSuccess) return;
                {
                    SAFE_CALL_VOID(f.SurfaceComposerClient__Constructor, data);
                }
                {
                    SAFE_CALL_VOID(f.RefBase__IncStrong, data, this);
                }
                valid = true;
            }

            SurfaceControl CreateSurface(const char *name, int32_t width, int32_t height, bool skipScrenshot, uint32_t windowFlags = 0, bool trustedOverlay = true, int64_t layerStack = -1)
            {
                if (!valid) return SurfaceControl(nullptr);

                auto &f = Functionals::GetInstance();
                void *parentHandle = nullptr;
                String8 windowName(name);
                LayerMetadata layerMetadata;

                if (skipScrenshot && (f.systemVersion == 10 || f.systemVersion == 11))
                {
                    if (!f.LayerMetadata__setInt32) return SurfaceControl(nullptr);
                    layerMetadata.setInt32(2u, 441731);
                }
                uint32_t flags = windowFlags;
                if (skipScrenshot && f.systemVersion >= 12)
                {
                    flags |= 0x40;
                }

                if (12 <= f.systemVersion)
                {
                    static void *fakeParentHandleForBinder = nullptr;
                    parentHandle = &fakeParentHandleForBinder;
                }

                StrongPointer<void> result;
                result.pointer = nullptr;

                // 修复：移除 SAFE_CALL_RET，改用显式判断，因为返回类型不同 (StrongPointer -> SurfaceControl)
                if (f.systemVersion == 9)
                {
                    int32_t windowType = -1;
                    int32_t ownerUid = -1;
                    if (skipScrenshot) windowType = 441731;

                    if (f.SurfaceComposerClient__CreateSurface_and9)
                    {
                        result = f.SurfaceComposerClient__CreateSurface_and9(data, windowName, width, height, 1, flags, parentHandle, windowType, ownerUid);
                    }
                    else
                    {
                        LS_LOGE_TAG("Surface", "CreateSurface_and9 方法缺失");
                    }
                }
                else if (f.systemVersion >= 10)
                {
                    if (f.SurfaceComposerClient__CreateSurfaceWithMetadataRef)
                    {
                        result = f.SurfaceComposerClient__CreateSurfaceWithMetadataRef(data, windowName, width, height, 1, static_cast<int32_t>(flags), parentHandle, layerMetadata, nullptr);
                    }
                    else if (f.SurfaceComposerClient__CreateSurface)
                    {
                        result = f.SurfaceComposerClient__CreateSurface(data, windowName, width, height, 1, flags, parentHandle, layerMetadata, nullptr);
                    }
                    else
                    {
                        LS_LOGE_TAG("Surface", "CreateSurface 方法缺失");
                    }
                }

                if (12 <= f.systemVersion && result.pointer != nullptr)
                {
                    static SurfaceComposerClientTransaction transaction;
                    if (transaction.valid)
                    {
                        transaction.SetTrustedOverlay(result, trustedOverlay);
                        if (layerStack >= 0 && (f.SurfaceComposerClient__Transaction__SetLayerStack || f.SurfaceComposerClient__Transaction__SetLayerStackLegacy))
                        {
                            transaction.SetLayerStack(result, static_cast<uint32_t>(layerStack));
                            transaction.Show(result);
                        }
                        transaction.Apply(false, true);
                    }
                }
                return {result.get()};
            }

            bool GetDisplayInfo(ui::DisplayState *displayInfo)
            {
                if (!valid) return false;
                auto &f = Functionals::GetInstance();

                // 修复：移除 SAFE_CALL_RET，改用显式判断和赋值
                if (displayToken.pointer == nullptr && 9 >= f.systemVersion)
                {
                    if (f.SurfaceComposerClient__GetBuiltInDisplay)
                    {
                        displayToken = f.SurfaceComposerClient__GetBuiltInDisplay(ui::DisplayType::DisplayIdMain);
                    }
                }
                else if (displayToken.pointer == nullptr)
                {
                    if (14 > f.systemVersion)
                    {
                        if (f.SurfaceComposerClient__GetInternalDisplayToken)
                        {
                            displayToken = f.SurfaceComposerClient__GetInternalDisplayToken();
                        }
                    }
                    else
                    { // Android 14+
                        if (f.SurfaceComposerClient__GetPhysicalDisplayIds && f.SurfaceComposerClient__GetPhysicalDisplayToken)
                        {
                            auto displayIds = f.SurfaceComposerClient__GetPhysicalDisplayIds();
                            if (!displayIds.empty())
                            {
                                displayToken = f.SurfaceComposerClient__GetPhysicalDisplayToken(displayIds[0]);
                            }
                        }
                    }
                }

                if (nullptr == displayToken.get()) return false;

                if (11 <= f.systemVersion)
                {
                    if (f.SurfaceComposerClient__GetDisplayState)
                    {
                        return f.SurfaceComposerClient__GetDisplayState(displayToken, displayInfo) == 0;
                    }
                    return false;
                }
                else
                {
                    ui::DisplayInfo realDisplayInfo{};
                    if (f.SurfaceComposerClient__GetDisplayInfo)
                    {
                        if (0 != f.SurfaceComposerClient__GetDisplayInfo(displayToken, &realDisplayInfo)) return false;

                        displayInfo->layerStackSpaceRect.width = realDisplayInfo.w;
                        displayInfo->layerStackSpaceRect.height = realDisplayInfo.h;
                        displayInfo->orientation = static_cast<ui::Rotation>(realDisplayInfo.orientation);
                        return true;
                    }
                    return false;
                }
            }
        };

        struct DumpDisplayInfo
        {
            uint32_t currentLayerStack = 0;
            int32_t orientation = 0;
            int32_t layerStackLeft = 0;
            int32_t layerStackTop = 0;
            int32_t layerStackRight = 0;
            int32_t layerStackBottom = 0;
            int32_t displayLeft = 0;
            int32_t displayTop = 0;
            int32_t displayRight = 0;
            int32_t displayBottom = 0;
        };

        inline std::string_view SubStringView(const std::string_view &str, std::string_view start, std::string_view end, size_t startOffset = 0)
        {
            auto startIt = str.find(start, startOffset);
            if (startIt == std::string_view::npos) return {};
            auto valueStart = startIt + start.size();
            auto endIt = str.find(end, valueStart);
            if (endIt == std::string_view::npos) return {};
            return str.substr(valueStart, endIt - valueStart);
        }

        inline bool ParseDisplayRect(const std::string_view &value, int32_t *left, int32_t *top, int32_t *right, int32_t *bottom)
        {
            if (value.empty() || !left || !top || !right || !bottom) return false;

            const std::string rectString(value);
            return std::sscanf(rectString.c_str(), "Rect(%d, %d - %d, %d)", left, top, right, bottom) == 4 || std::sscanf(rectString.c_str(), "(%d, %d - %d, %d)", left, top, right, bottom) == 4;
        }

        inline int32_t ParseDisplayOrientation(const std::string_view &value)
        {
            if (value.find("ROTATION_90") != std::string_view::npos) return 1;
            if (value.find("ROTATION_180") != std::string_view::npos) return 2;
            if (value.find("ROTATION_270") != std::string_view::npos) return 3;
            if (value.find("ROTATION_0") != std::string_view::npos) return 0;
            return value.empty() ? 0 : std::stoi(std::string(value));
        }

        inline std::vector<DumpDisplayInfo> ParseDumpDisplayInfo(const std::string_view &dumpDisplayInfo)
        {
            std::vector<DumpDisplayInfo> result;
            size_t dumpDisplayInfoIt = std::string_view::npos;
            while ((dumpDisplayInfoIt = dumpDisplayInfo.find("DisplayDeviceInfo", dumpDisplayInfoIt + 1)) != std::string_view::npos)
            {
                const size_t nextDisplayInfo = dumpDisplayInfo.find("DisplayDeviceInfo", dumpDisplayInfoIt + 1);
                const size_t blockEnd = nextDisplayInfo == std::string_view::npos ? dumpDisplayInfo.size() : nextDisplayInfo;
                const std::string_view displayBlock = dumpDisplayInfo.substr(dumpDisplayInfoIt, blockEnd - dumpDisplayInfoIt);

                auto currentLayerStack = SubStringView(displayBlock, "mCurrentLayerStack=", "\n");
                auto currentLayerStackRect = SubStringView(displayBlock, "mCurrentLayerStackRect=", "\n");
                auto currentDisplayRect = SubStringView(displayBlock, "mCurrentDisplayRect=", "\n");
                auto orientation = SubStringView(displayBlock, "mCurrentOrientation=", "\n");
                if (currentLayerStack.empty() || currentLayerStack == "-1" || currentLayerStackRect.empty()) continue;

                DumpDisplayInfo info{};
                try
                {
                    info.currentLayerStack = static_cast<uint32_t>(std::stoul(std::string(currentLayerStack)));
                    info.orientation = ((ParseDisplayOrientation(orientation) % 4) + 4) % 4;
                }
                catch (...)
                {
                    continue;
                }

                if (!ParseDisplayRect(currentLayerStackRect, &info.layerStackLeft, &info.layerStackTop, &info.layerStackRight, &info.layerStackBottom)) continue;

                if (!ParseDisplayRect(currentDisplayRect, &info.displayLeft, &info.displayTop, &info.displayRight, &info.displayBottom))
                {
                    // 部分 ROM 不输出 displayRect；退回 layerStackRect，仍保持旧行为。
                    info.displayLeft = info.layerStackLeft;
                    info.displayTop = info.layerStackTop;
                    info.displayRight = info.layerStackRight;
                    info.displayBottom = info.layerStackBottom;
                }

                result.emplace_back(info);
            }
            return result;
        }
    } // namespace detail

    class SurfaceControlManager
    {
    public:
        struct DisplayInfo
        {
            int32_t orientation;
            int32_t width;
            int32_t height;
        };

        struct RectInfo
        {
            int32_t left = 0;
            int32_t top = 0;
            int32_t right = 0;
            int32_t bottom = 0;

            int32_t Width() const
            {
                return std::abs(right - left);
            }
            int32_t Height() const
            {
                return std::abs(bottom - top);
            }

            bool operator==(const RectInfo &other) const
            {
                return left == other.left && top == other.top && right == other.right && bottom == other.bottom;
            }
        };

        struct RecordDisplayInfo
        {
            uint32_t layerStack = 0;
            int32_t width = 0;
            int32_t height = 0;
            int32_t orientation = 0;
            RectInfo layerStackRect{};
            RectInfo displayRect{};
        };

        struct MirrorProjection
        {
            float dsdx = 1.0f;
            float dtdx = 0.0f;
            float dtdy = 0.0f;
            float dsdy = 1.0f;
            float x = 0.0f;
            float y = 0.0f;
        };

        static bool CalculateMirrorProjection(const DisplayInfo &source, const RecordDisplayInfo &target, MirrorProjection *projection)
        {
            const int32_t targetWidth = target.layerStackRect.right - target.layerStackRect.left;
            const int32_t targetHeight = target.layerStackRect.bottom - target.layerStackRect.top;
            if (!projection || source.width <= 0 || source.height <= 0 || targetWidth <= 0 || targetHeight <= 0) return false;

            const int rotation = ((source.orientation - target.orientation) % 4 + 4) % 4;
            const float rotatedWidth = rotation % 2 ? source.height : source.width;
            const float rotatedHeight = rotation % 2 ? source.width : source.height;
            const float scale = std::min(targetWidth / rotatedWidth, targetHeight / rotatedHeight);
            *projection = {scale, 0.0f, 0.0f, scale, target.layerStackRect.left + (targetWidth - rotatedWidth * scale) * 0.5f, target.layerStackRect.top + (targetHeight - rotatedHeight * scale) * 0.5f};
            switch (rotation)
            {
            case 1:
                projection->dsdx = projection->dsdy = 0.0f;
                projection->dtdx = scale;
                projection->dtdy = -scale;
                projection->x += source.height * scale;
                break;
            case 2:
                projection->dsdx = projection->dsdy = -scale;
                projection->x += source.width * scale;
                projection->y += source.height * scale;
                break;
            case 3:
                projection->dsdx = projection->dsdy = 0.0f;
                projection->dtdx = -scale;
                projection->dtdy = scale;
                projection->y += source.width * scale;
                break;
            }
            return true;
        }

    public:
        static detail::SurfaceComposerClient &GetComposerInstance()
        {
            static detail::SurfaceComposerClient surfaceComposerClient;
            return surfaceComposerClient;
        }

        static bool SupportsRecordLayerStack()
        {
            const auto &functions = detail::Functionals::GetInstance();
            return functions.initSuccess && functions.systemVersion >= 12 && (functions.SurfaceComposerClient__MirrorSurface || functions.SurfaceComposerClient__MirrorSurfaceWithBounds) && (functions.SurfaceComposerClient__Transaction__SetLayerStack || functions.SurfaceComposerClient__Transaction__SetLayerStackLegacy) && functions.SurfaceComposerClient__Transaction__Show && functions.SurfaceComposerClient__Transaction__Reparent && functions.SurfaceComposerClient__Transaction__SetMatrix && functions.SurfaceComposerClient__Transaction__SetPosition && functions.SurfaceComposerClient__Transaction__SetCrop;
        }

        static DisplayInfo GetDisplayInfo()
        {
            auto &surfaceComposerClient = GetComposerInstance();
            detail::ui::DisplayState displayInfo{};

            if (!surfaceComposerClient.GetDisplayInfo(&displayInfo)) return {0, 0, 0};

            DisplayInfo local_displayInfo{};
            int32_t local_orientation = static_cast<int32_t>(displayInfo.orientation);
            int32_t local_abs_x = (displayInfo.layerStackSpaceRect.width > displayInfo.layerStackSpaceRect.height ? displayInfo.layerStackSpaceRect.width : displayInfo.layerStackSpaceRect.height);
            int32_t local_abs_y = (displayInfo.layerStackSpaceRect.width < displayInfo.layerStackSpaceRect.height ? displayInfo.layerStackSpaceRect.width : displayInfo.layerStackSpaceRect.height);
            if (local_orientation == 1 || local_orientation == 3)
            {
                local_displayInfo.width = local_abs_x;
                local_displayInfo.height = local_abs_y;
            }
            else
            {
                local_displayInfo.width = local_abs_y;
                local_displayInfo.height = local_abs_x;
            }
            local_displayInfo.orientation = local_orientation;
            return local_displayInfo;
        }

        static ANativeWindow *Create(const char *name, int32_t width, int32_t height, bool preventCapture)
        {
            auto &surfaceComposerClient = GetComposerInstance();

            int retries = 0;
            while ((-1 == width || -1 == height) && retries < 1)
            {
                detail::ui::DisplayState displayInfo{};
                if (surfaceComposerClient.GetDisplayInfo(&displayInfo))
                {
                    width = displayInfo.layerStackSpaceRect.width;
                    height = displayInfo.layerStackSpaceRect.height;
                }
                retries++;
            }
            if (width <= 0) width = 1080;
            if (height <= 0) height = 2340;

            auto surfaceControl = surfaceComposerClient.CreateSurface(name, width, height, preventCapture, 0, true);

            // 修复：指明 Surface 的命名空间 detail::Surface
            detail::Surface *rawSurface = surfaceControl.GetSurface();
            if (rawSurface == nullptr)
            {
                LS_LOGE_TAG("Surface", "无法从 SurfaceControl 获取 Surface");
                return nullptr;
            }

            auto nativeWindow = reinterpret_cast<ANativeWindow *>(rawSurface);
            m_cachedSurfaceControl.emplace(nativeWindow, std::move(surfaceControl));
            return nativeWindow;
        }

        // 查找录屏/投屏虚拟屏所在的非主 layerStack，供 AOSP mirrorSurface 方案挂载镜像层。
        static bool FindRecordDisplay(RecordDisplayInfo *recordDisplay, int32_t expectedWidth = 0, int32_t expectedHeight = 0)
        {
            if (!recordDisplay || !SupportsRecordLayerStack()) return false;

            auto pipe = popen("dumpsys display", "r");
            if (!pipe) return false;

            char buffer[512]{};
            std::string dumpDisplayResult;
            while (fgets(buffer, sizeof(buffer), pipe) != nullptr) dumpDisplayResult += buffer;
            pclose(pipe);

            auto displayInfos = detail::ParseDumpDisplayInfo(dumpDisplayResult);
            int bestScore = INT_MIN;
            RecordDisplayInfo bestDisplay{};
            for (const auto &displayInfo : displayInfos)
            {
                if (displayInfo.currentLayerStack == 0) continue;

                int32_t layerStackWidth = std::abs(displayInfo.layerStackRight - displayInfo.layerStackLeft);
                int32_t layerStackHeight = std::abs(displayInfo.layerStackBottom - displayInfo.layerStackTop);
                int32_t displayWidth = std::abs(displayInfo.displayRight - displayInfo.displayLeft);
                int32_t displayHeight = std::abs(displayInfo.displayBottom - displayInfo.displayTop);
                if (layerStackWidth <= 0 || layerStackHeight <= 0) continue;

                int score = 0;
                if (expectedWidth > 0 && expectedHeight > 0)
                {
                    int layerExactDelta = std::abs(layerStackWidth - expectedWidth) + std::abs(layerStackHeight - expectedHeight);
                    int layerSwappedDelta = std::abs(layerStackWidth - expectedHeight) + std::abs(layerStackHeight - expectedWidth);
                    int displayExactDelta = std::abs(displayWidth - expectedWidth) + std::abs(displayHeight - expectedHeight);
                    int displaySwappedDelta = std::abs(displayWidth - expectedHeight) + std::abs(displayHeight - expectedWidth);
                    int bestExactDelta = std::min(layerExactDelta, displayExactDelta);
                    int bestSwappedDelta = std::min(layerSwappedDelta, displaySwappedDelta);
                    int bestDelta = std::min(bestExactDelta, bestSwappedDelta);
                    score -= bestDelta;
                    if (bestExactDelta == 0) score += 100000;
                    else if (bestSwappedDelta == 0) score += 50000;
                }
                score += static_cast<int>(displayInfo.currentLayerStack);

                if (score <= bestScore) continue;

                bestScore = score;
                bestDisplay.layerStack = displayInfo.currentLayerStack;
                bestDisplay.orientation = displayInfo.orientation;
                bestDisplay.layerStackRect = {displayInfo.layerStackLeft, displayInfo.layerStackTop, displayInfo.layerStackRight, displayInfo.layerStackBottom};
                bestDisplay.displayRect = {displayInfo.displayLeft, displayInfo.displayTop, displayInfo.displayRight, displayInfo.displayBottom};
                bestDisplay.width = layerStackWidth;
                bestDisplay.height = layerStackHeight;
            }
            if (bestScore == INT_MIN) return false;

            *recordDisplay = bestDisplay;
            return true;
        }

        // mirrorSurface 创建的根层 addToRoot=false，必须挂到录屏 layerStack 的可见容器。
        static bool CreateMirrorOnLayerStack(ANativeWindow *sourceWindow, uint32_t layerStack)
        {
            if (!sourceWindow || !SupportsRecordLayerStack()) return false;

            auto sourceIt = m_cachedSurfaceControl.find(sourceWindow);
            if (sourceIt == m_cachedSurfaceControl.end() || !sourceIt->second.data) return false;

            auto &functions = detail::Functionals::GetInstance();
            constexpr uint32_t containerFlags = 0x00080000;
            auto root = GetComposerInstance().CreateSurface("LarkRecord", 0, 0, false, containerFlags, false);
            if (!root.data) return false;
            auto mirrored = functions.SurfaceComposerClient__MirrorSurface ? functions.SurfaceComposerClient__MirrorSurface(GetComposerInstance().data, sourceIt->second.data) : functions.SurfaceComposerClient__MirrorSurfaceWithBounds(GetComposerInstance().data, sourceIt->second.data, nullptr, nullptr);
            if (!mirrored.pointer)
            {
                root.DestroyMirror();
                return false;
            }

            DestroyMirrorOnLayerStack(sourceWindow);
            auto [mirrorIt, inserted] = m_cachedMirrors.emplace(sourceWindow, RecordMirror{root, detail::SurfaceControl(mirrored.pointer)});

            static detail::SurfaceComposerClientTransaction transaction;
            if (!transaction.valid || !functions.SurfaceComposerClient__Transaction__SetLayer || !functions.SurfaceComposerClient__Transaction__Show)
            {
                DestroyMirrorOnLayerStack(sourceWindow);
                return false;
            }

            detail::StrongPointer<void> rootPointer{};
            rootPointer.pointer = mirrorIt->second.root.data;
            detail::StrongPointer<void> mirrorPointer{};
            mirrorPointer.pointer = mirrorIt->second.mirror.data;
            transaction.SetLayerStack(rootPointer, layerStack);
            transaction.SetLayer(rootPointer, INT_MAX);
            transaction.Show(rootPointer);
            transaction.Reparent(mirrorPointer, rootPointer);
            transaction.SetTrustedOverlay(mirrorPointer, false);
            transaction.SetLayer(mirrorPointer, 0);
            transaction.Show(mirrorPointer);
            if (transaction.Apply(false, true) != 0)
            {
                DestroyMirrorOnLayerStack(sourceWindow);
                return false;
            }
            return true;
        }

        static void DestroyMirrorOnLayerStack(ANativeWindow *sourceWindow)
        {
            if (!sourceWindow) return;
            auto it = m_cachedMirrors.find(sourceWindow);
            if (it == m_cachedMirrors.end()) return;
            it->second.mirror.DestroyMirror();
            it->second.root.DestroyMirror();
            m_cachedMirrors.erase(it);
        }

        static bool ConfigureMirrorProjection(ANativeWindow *sourceWindow, const DisplayInfo &source, const RecordDisplayInfo &target)
        {
            const auto found = m_cachedMirrors.find(sourceWindow);
            MirrorProjection projection;
            if (found == m_cachedMirrors.end() || !CalculateMirrorProjection(source, target, &projection)) return false;

            static detail::SurfaceComposerClientTransaction transaction;
            detail::StrongPointer<void> rootPointer;
            rootPointer.pointer = found->second.root.data;
            if (!transaction.SetCrop(rootPointer, {0, 0, source.width, source.height}) || !transaction.SetMatrix(rootPointer, projection.dsdx, projection.dtdx, projection.dtdy, projection.dsdy) || !transaction.SetPosition(rootPointer, projection.x, projection.y)) return false;
            return transaction.Apply(false, true) == 0;
        }

        static void Destroy(ANativeWindow *nativeWindow)
        {
            if (!nativeWindow) return;
            DestroyMirrorOnLayerStack(nativeWindow);
            auto it = m_cachedSurfaceControl.find(nativeWindow);
            if (it == m_cachedSurfaceControl.end()) return;

            m_cachedSurfaceControl[nativeWindow].DestroySurface(reinterpret_cast<detail::Surface *>(nativeWindow));
            m_cachedSurfaceControl.erase(nativeWindow);
        }

    private:
        struct RecordMirror
        {
            detail::SurfaceControl root;
            detail::SurfaceControl mirror;
        };

        inline static std::unordered_map<ANativeWindow *, detail::SurfaceControl> m_cachedSurfaceControl;
        inline static std::unordered_map<ANativeWindow *, RecordMirror> m_cachedMirrors;
    };
} // namespace android
#undef ResolveMethod
#undef SAFE_CALL_RET
#undef SAFE_CALL_VOID
#endif // SURFACE_CONTROL_MANAGER_H
