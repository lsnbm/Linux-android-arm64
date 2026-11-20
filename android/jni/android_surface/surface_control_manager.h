/*
SurfaceControl 私有 ABI（arm64-v8a）。签名以目标 AOSP tag 的 libs/gui/include/gui/
SurfaceComposerClient.h、SurfaceControl.h 和设备 libgui 的动态导出为准：
https://android.googlesource.com/platform/frameworks/native/

一、Android 12–17 的版本差异
Functionals 优先读取 ro.build.version.sdk：31/32 对应 12/12L，33–37 对应 13–17；
未识别的 SDK 才回退解析 ro.build.version.release。接口仍以设备的实际导出为准：
    12/12L：setLayerStack(uint32_t)，apply(bool)，createSurface 的 android::LayerMetadata 按值传递。
    13：setLayerStack(ui::LayerStack)，apply(bool, bool)，mirrorSurface 仍为单参数。
    14–16：createSurface 的 flags 改为 int32_t，metadata 改为 android::gui::LayerMetadata 值参数。
    17：mirrorSurface(source, stopAt, cropBy) 后两项传 nullptr；createSurface 的 metadata 改为 const 引用。
12–13 使用 internal display token，14+ 使用 physical display ID。适配新版本时要同时核对
函数签名、StrongPointer 返回约定、DisplayState 布局和对象对齐；能加载同名库不代表 ABI 兼容。

二、截图与录屏开关
渲染器的 init(true) 表示允许捕获，不会启动录屏；Create 的 preventCapture 含义相反。
镜像必须挂到录屏 layerStack 的可见容器；仅有 mirrorSurface 句柄或事务成功，不代表录屏成片有内容。
display 与 SurfaceFlinger 查询使用 Binder dump，不启动 shell。字段来自显示服务、
LayerProtoParser 或前端 Layer Hierarchy 的输出，并非稳定 API；metadata 不是 Binder 身份。

三、横竖屏投影与验证边界
独立镜像不会继承系统 ContentRecorder 对其他镜像根层施加的变换。主屏和录屏方向不同时，
要先按方向差旋转，再等比缩放并居中到 layerStackRect；displayRect 投影由显示服务处理，不能重复施加。
历史案例：RMX3888 / Android 14 上，主屏 2376×1080 横屏而 Oplus 录屏仍是 1080×2376 竖屏，
正确映射为 x'=1080-y、y'=x。旧实现仅挂载镜像后，竖屏正常但横屏仍裁切。
回归时分别检查接口、图层父子关系、虚拟显示合成条目和最终保存的视频。仅在列表中看见图层，
或在一台设备上得到正常成片，都不能代替其他 ROM、反向横屏、禁止捕获和录屏重启的实测。
*/

#ifndef SURFACE_CONTROL_MANAGER_H
#define SURFACE_CONTROL_MANAGER_H

#include "../logger/logger.h"
#include "binder_dump.h"
#include <android/native_window.h>
#include <android/log.h>
#include <dlfcn.h>
#include <sys/system_properties.h>

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <charconv>
#include <climits>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
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
            any_t *pointer = nullptr;

            // 对齐 libutils sp<T> 的非平凡返回 ABI；引用由持有 Surface 的代码释放。
            ~StrongPointer()
            {
            }
        };

        struct Functionals
        {
            struct SymbolMethod
            {
                void *(*Open)(const char *filename, int flag) = nullptr;
                void *(*Find)(void *handle, const char *symbol) = nullptr;
            };

            size_t systemVersion = 0;
            bool initSuccess = false;

            void (*RefBase__IncStrong)(void *thiz, void *id) = nullptr;
            void (*RefBase__DecStrong)(void *thiz, void *id) = nullptr;

            void (*String8__Constructor)(void *thiz, const char *const data) = nullptr;
            void (*String8__Destructor)(void *thiz) = nullptr;

            void (*LayerMetadata__Constructor)(void *thiz) = nullptr;
            void (*LayerMetadata__setInt32)(void *thiz, uint32_t key, int32_t value) = nullptr;

            void (*Parcel__Constructor)(void *thiz) = nullptr;
            void (*Parcel__Destructor)(void *thiz) = nullptr;
            int32_t (*Parcel__WriteInt32)(void *thiz, int32_t value) = nullptr;

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
            void *(*SurfaceComposerClient__Transaction__SetMetadata)(void *thiz, const StrongPointer<void> &surfaceControl, uint32_t key, const void *parcel) = nullptr;
            int32_t (*SurfaceComposerClient__Transaction__ApplyLegacy)(void *thiz, bool synchronous) = nullptr;
            int32_t (*SurfaceComposerClient__Transaction__Apply)(void *thiz, bool synchronous, bool oneWay) = nullptr;

            StrongPointer<Surface> (*SurfaceControl__GetSurface)(void *thiz) = nullptr;
            StrongPointer<void> (*SurfaceControl__GetParentingLayer)(void *thiz) = nullptr;
            void (*SurfaceControl__DisConnect)(void *thiz) = nullptr;

            Functionals(const SymbolMethod &symbolMethod)
            {
                char sdkString[PROP_VALUE_MAX]{};
                __system_property_get("ro.build.version.sdk", sdkString);
                const int sdk = std::atoi(sdkString);
                if (sdk == 31 || sdk == 32) systemVersion = 12;
                else if (sdk >= 33 && sdk <= 37) systemVersion = static_cast<size_t>(sdk - 20);

                if (systemVersion == 0)
                {
                    char release[PROP_VALUE_MAX]{};
                    __system_property_get("ro.build.version.release", release);
                    const int version = std::atoi(release);
                    if (version <= 0)
                    {
                        LS_LOGE_TAG("Surface", "无法解析 Android 版本: %s", release);
                        return;
                    }
                    systemVersion = static_cast<size_t>(version);
                }

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

                if (systemVersion >= 12)
                {
#ifdef __LP64__
                    auto libbinder = symbolMethod.Open("/system/lib64/libbinder.so", RTLD_LAZY);
#else
                    auto libbinder = symbolMethod.Open("/system/lib/libbinder.so", RTLD_LAZY);
#endif
                    if (libbinder)
                    {
                        ResolveMethod(Parcel, Constructor, libbinder, "_ZN7android6ParcelC1Ev");
                        ResolveMethod(Parcel, Destructor, libbinder, "_ZN7android6ParcelD1Ev");
                        ResolveMethod(Parcel, WriteInt32, libbinder, "_ZN7android6Parcel10writeInt32Ei");
                    }
                    ResolveMethod(SurfaceControl, GetParentingLayer, libgui, "_ZN7android14SurfaceControl17getParentingLayerEv");
                    ResolveMethod(SurfaceComposerClient__Transaction, SetMetadata, libgui, "_ZN7android21SurfaceComposerClient11Transaction11setMetadataERKNS_2spINS_14SurfaceControlEEEjRKNS_6ParcelE");
                }

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
                        ResolveMethod(LayerMetadata, setInt32, libgui, "_ZN7android3gui13LayerMetadata8setInt32Eji");
                    }
                    else
                    {
                        ResolveMethod(SurfaceComposerClient, CreateSurface, libgui, "_ZN7android21SurfaceComposerClient13createSurfaceERKNS_7String8EjjijRKNS_2spINS_7IBinderEEENS_13LayerMetadataEPj");
                        ResolveMethod(LayerMetadata, Constructor, libgui, "_ZN7android13LayerMetadataC2Ev");
                        ResolveMethod(LayerMetadata, setInt32, libgui, "_ZN7android13LayerMetadata8setInt32Eji");
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
                    for (const auto &[patchTo, signature] : it->second)
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
                const bool hasSurfaceCreate = systemVersion == 9 ? SurfaceComposerClient__CreateSurface_and9 != nullptr : SurfaceComposerClient__CreateSurface != nullptr || SurfaceComposerClient__CreateSurfaceWithMetadataRef != nullptr;
                const bool hasDisplayToken = systemVersion <= 9 ? SurfaceComposerClient__GetBuiltInDisplay != nullptr : systemVersion >= 14 ? SurfaceComposerClient__GetPhysicalDisplayIds != nullptr && SurfaceComposerClient__GetPhysicalDisplayToken != nullptr : SurfaceComposerClient__GetInternalDisplayToken != nullptr;
                const bool hasDisplayInfo = systemVersion >= 11 ? SurfaceComposerClient__GetDisplayState != nullptr : SurfaceComposerClient__GetDisplayInfo != nullptr;
                const bool hasApply = SurfaceComposerClient__Transaction__Apply != nullptr || SurfaceComposerClient__Transaction__ApplyLegacy != nullptr;
                const bool hasTrustedOverlay = systemVersion < 12 || SurfaceComposerClient__Transaction__SetTrustedOverlay != nullptr;
                initSuccess = RefBase__IncStrong != nullptr && RefBase__DecStrong != nullptr && String8__Constructor != nullptr && String8__Destructor != nullptr && SurfaceComposerClient__Constructor != nullptr && SurfaceControl__GetSurface != nullptr && SurfaceControl__DisConnect != nullptr && SurfaceComposerClient__Transaction__Constructor != nullptr && SurfaceComposerClient__Transaction__SetLayer != nullptr && hasLayerMetadata && hasSurfaceCreate && hasDisplayToken && hasDisplayInfo && hasTrustedOverlay && hasApply;
                if (!initSuccess) LS_LOGE_TAG("Surface", "SurfaceComposerClient 必需符号不完整");
            }

            static const Functionals &GetInstance(const SymbolMethod &symbolMethod = {.Open = dlopen, .Find = dlsym})
            {
                static Functionals functionals(symbolMethod);
                return functionals;
            }
        };

        struct String8
        {
            alignas(std::max_align_t) char data[1024];

            explicit String8(const char *value)
            {
                Functionals::GetInstance().String8__Constructor(data, value);
            }

            ~String8()
            {
                Functionals::GetInstance().String8__Destructor(data);
            }
        };

        struct LayerMetadata
        {
            alignas(std::max_align_t) char data[1024];
            bool valid;

            LayerMetadata() : valid(Functionals::GetInstance().systemVersion >= 10)
            {
                if (valid) Functionals::GetInstance().LayerMetadata__Constructor(data);
            }

            LayerMetadata(const LayerMetadata &) = delete;
            LayerMetadata &operator=(const LayerMetadata &) = delete;

            ~LayerMetadata()
            {
                if (!valid) return;
                void **vtable;
                std::memcpy(&vtable, data, sizeof(vtable));
                reinterpret_cast<void (*)(void *)>(vtable[0])(data);
            }
        };

        struct SurfaceControl
        {
            void *data = nullptr;

            void Release(Surface *surface = nullptr)
            {
                if (!data) return;
                const auto &f = Functionals::GetInstance();
                if (surface) f.RefBase__DecStrong(reinterpret_cast<char *>(surface) - sizeof(std::max_align_t) / 2, this);
                f.SurfaceControl__DisConnect(data);
                f.RefBase__DecStrong(data, this);
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

            void SetLayerStack(StrongPointer<void> &surfaceControl, uint32_t layerStack)
            {
                auto &f = Functionals::GetInstance();
                if (f.SurfaceComposerClient__Transaction__SetLayerStack) f.SurfaceComposerClient__Transaction__SetLayerStack(data, surfaceControl, ui::LayerStack{layerStack});
                else if (f.SurfaceComposerClient__Transaction__SetLayerStackLegacy) f.SurfaceComposerClient__Transaction__SetLayerStackLegacy(data, surfaceControl, layerStack);
            }

            int32_t Apply(bool synchronous, bool oneWay)
            {
                int status = 0;
                auto &f = Functionals::GetInstance();
                if (!valid) return -ENOSYS;
                if (f.SurfaceComposerClient__Transaction__Apply) status = f.SurfaceComposerClient__Transaction__Apply(data, synchronous, oneWay);
                else if (f.SurfaceComposerClient__Transaction__ApplyLegacy) status = f.SurfaceComposerClient__Transaction__ApplyLegacy(data, synchronous);
                else return -ENOSYS;
                return status;
            }

            int SetMetadata(const StrongPointer<void> &surfaceControl, uint32_t key, int32_t value)
            {
                int status = 0;
                const auto &functions = Functionals::GetInstance();
                alignas(std::max_align_t) char parcel[1024]{};
                functions.Parcel__Constructor(parcel);
                status = functions.Parcel__WriteInt32(parcel, value);
                if (status == 0 && !functions.SurfaceComposerClient__Transaction__SetMetadata(data, surfaceControl, key, parcel)) status = -EIO;
                functions.Parcel__Destructor(parcel);
                return status;
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
                f.SurfaceComposerClient__Constructor(data);
                f.RefBase__IncStrong(data, this);
                valid = true;
            }

            SurfaceControl CreateSurface(const char *name, int32_t width, int32_t height, bool skipScrenshot, uint32_t windowFlags = 0, bool trustedOverlay = true, int64_t layerStack = -1, int32_t windowType = -1, int32_t ownerUid = -1, int32_t ownerPid = -1)
            {
                if (!valid) return {};

                auto &f = Functionals::GetInstance();
                void *parentHandle = nullptr;
                String8 windowName(name);
                LayerMetadata layerMetadata;

                if (layerMetadata.valid && f.LayerMetadata__setInt32)
                {
                    if (ownerUid >= 0) f.LayerMetadata__setInt32(layerMetadata.data, 1u, ownerUid);
                    if (windowType >= 0) f.LayerMetadata__setInt32(layerMetadata.data, 2u, windowType);
                    if (ownerPid >= 0) f.LayerMetadata__setInt32(layerMetadata.data, 6u, ownerPid);
                }
                if (skipScrenshot && (f.systemVersion == 10 || f.systemVersion == 11))
                {
                    if (!f.LayerMetadata__setInt32) return {};
                    f.LayerMetadata__setInt32(layerMetadata.data, 2u, 441731);
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
                if (f.systemVersion == 9)
                {
                    result = f.SurfaceComposerClient__CreateSurface_and9(data, windowName.data, width, height, 1, flags, parentHandle, skipScrenshot ? 441731 : windowType, ownerUid);
                }
                else if (f.SurfaceComposerClient__CreateSurfaceWithMetadataRef)
                {
                    result = f.SurfaceComposerClient__CreateSurfaceWithMetadataRef(data, windowName.data, width, height, 1, static_cast<int32_t>(flags), parentHandle, layerMetadata.data, nullptr);
                }
                else if (f.systemVersion >= 10)
                {
                    result = f.SurfaceComposerClient__CreateSurface(data, windowName.data, width, height, 1, flags, parentHandle, layerMetadata.data, nullptr);
                }

                if (12 <= f.systemVersion && result.pointer != nullptr)
                {
                    static SurfaceComposerClientTransaction transaction;
                    if (transaction.valid)
                    {
                        f.SurfaceComposerClient__Transaction__SetTrustedOverlay(transaction.data, result, trustedOverlay);
                        if (layerStack >= 0 && (f.SurfaceComposerClient__Transaction__SetLayerStack || f.SurfaceComposerClient__Transaction__SetLayerStackLegacy))
                        {
                            transaction.SetLayerStack(result, static_cast<uint32_t>(layerStack));
                            if (f.SurfaceComposerClient__Transaction__Show) f.SurfaceComposerClient__Transaction__Show(transaction.data, result);
                        }
                        transaction.Apply(false, true);
                    }
                }
                return {result.pointer};
            }

            bool GetDisplayInfo(ui::DisplayState *displayInfo)
            {
                if (!valid || !displayInfo) return false;
                auto &f = Functionals::GetInstance();
                if (!displayToken.pointer)
                {
                    // initSuccess 已按系统版本检查相应的取 token 和读显示状态接口。
                    if (f.systemVersion <= 9) displayToken = f.SurfaceComposerClient__GetBuiltInDisplay(ui::DisplayType::DisplayIdMain);
                    else if (f.systemVersion < 14) displayToken = f.SurfaceComposerClient__GetInternalDisplayToken();
                    else
                    {
                        const auto ids = f.SurfaceComposerClient__GetPhysicalDisplayIds();
                        if (ids.empty()) return false;
                        displayToken = f.SurfaceComposerClient__GetPhysicalDisplayToken(ids.front());
                    }
                    if (!displayToken.pointer) return false;
                }
                if (f.systemVersion >= 11) return f.SurfaceComposerClient__GetDisplayState(displayToken, displayInfo) == 0;

                ui::DisplayInfo legacy{};
                if (f.SurfaceComposerClient__GetDisplayInfo(displayToken, &legacy) != 0) return false;
                displayInfo->layerStackSpaceRect.width = legacy.w;
                displayInfo->layerStackSpaceRect.height = legacy.h;
                displayInfo->orientation = static_cast<ui::Rotation>(legacy.orientation);
                return true;
            }
        };

        inline std::string_view DisplayField(std::string_view block, std::string_view key)
        {
            const size_t start = block.find(key);
            if (start == std::string_view::npos) return {};
            block.remove_prefix(start + key.size());
            return block.substr(0, block.find('\n'));
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

            int64_t Width() const
            {
                return std::abs(int64_t{right} - left);
            }

            int64_t Height() const
            {
                return std::abs(int64_t{bottom} - top);
            }

            bool operator==(const RectInfo &) const = default;

            bool Parse(std::string_view value)
            {
                if (value.starts_with("Rect")) value.remove_prefix(4);
                if (!value.starts_with('(')) return false;
                value.remove_prefix(1);
                int32_t coords[4];
                constexpr char separators[] = {',', '-', ',', ')'};
                for (size_t i = 0; i < 4; ++i)
                {
                    const auto start = value.find_first_not_of(" \t\r\n");
                    if (start == value.npos) return false;
                    value.remove_prefix(start);
                    if (value.starts_with('+'))
                    {
                        value.remove_prefix(1);
                        if (value.empty() || value.front() < '0' || value.front() > '9') return false;
                    }
                    const auto parsed = std::from_chars(value.data(), value.data() + value.size(), coords[i]);
                    if (parsed.ec != std::errc{}) return false;
                    value.remove_prefix(parsed.ptr - value.data());
                    const auto separator = value.find_first_not_of(" \t\r\n");
                    if (separator == value.npos || value[separator] != separators[i]) return false;
                    value.remove_prefix(separator + 1);
                }
                if (value.find_first_not_of(" \t\r\n") != value.npos) return false;
                *this = {coords[0], coords[1], coords[2], coords[3]};
                return true;
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

        struct LayerDescription
        {
            int32_t windowType = -1;
            int32_t ownerUid = -1;
            int32_t ownerPid = -1;
        };

    private:
        struct RecordMirror
        {
            detail::SurfaceControl root;
            detail::SurfaceControl mirror;
        };

        inline static std::unordered_map<ANativeWindow *, detail::SurfaceControl> m_cachedSurfaceControl;
        inline static std::unordered_map<ANativeWindow *, LayerDescription> m_cachedRecordDescriptions;
        inline static std::unordered_map<ANativeWindow *, RecordMirror> m_cachedMirrors;

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
            detail::ui::DisplayState displayInfo{};
            if (!GetComposerInstance().GetDisplayInfo(&displayInfo)) return {};
            const auto [shortSide, longSide] = std::minmax(displayInfo.layerStackSpaceRect.width, displayInfo.layerStackSpaceRect.height);
            const int orientation = static_cast<int>(displayInfo.orientation);
            if (orientation == 1 || orientation == 3) return {orientation, longSide, shortSide};
            return {orientation, shortSide, longSide};
        }

    private:
        static int ReadSystemLayerDescription(const char *name, LayerDescription *description)
        {
            int status = 0;
            if (!name || !*name || !description) return -EINVAL;
            static detail::BinderDumpReader reader("SurfaceFlinger");
            std::string text;
            status = reader.Read(text, std::chrono::seconds(3));
            if (status != STATUS_OK)
            {
                LS_LOGE_TAG("Surface", "Binder dump SurfaceFlinger failed: %d", status);
                return status;
            }
            const std::string_view target(name);
            bool inHierarchy = false;
            bool offscreen = false;
            bool selected = false;
            int32_t layerId = -1, actualUid = -1, actualPid = -1;
            int bestScore = -1;
            int32_t bestId = INT_MAX;
            LayerDescription parsed;
            LayerDescription best;
            auto readInteger = [](std::string_view text, std::string_view field, int32_t *result, bool wholeField = false)
            {
                const size_t position = text.find(field);
                if (position == std::string_view::npos || (wholeField && position != 0)) return;
                text.remove_prefix(position + field.size());
                while (!text.empty() && (text.front() == ' ' || text.front() == '\t')) text.remove_prefix(1);
                while (!text.empty() && (text.back() == ' ' || text.back() == '\t')) text.remove_suffix(1);
                if (text.empty() || text.front() < '0' || text.front() > '9') return;
                int32_t value;
                const auto [end, ec] = std::from_chars(text.data(), text.data() + text.size(), value);
                if (ec != std::errc{}) return;
                if (end == text.data() + text.size() || (!wholeField && std::string_view(",} \t\r\n").find(*end) != std::string_view::npos)) *result = value;
            };
            auto selectName = [&](std::string_view entry)
            {
                if (entry.size() <= target.size() || entry.substr(0, target.size()) != target || entry[target.size()] != '#') return false;
                entry.remove_prefix(target.size() + 1);
                if (entry.empty() || entry.front() < '0' || entry.front() > '9') return false;
                int32_t id;
                const auto [end, ec] = std::from_chars(entry.data(), entry.data() + entry.size(), id);
                if (ec != std::errc{} || (end != entry.data() + entry.size() && *end != ' ' && *end != '\t')) return false;
                if (entry.find("handleNotAlive") != std::string_view::npos || entry.find("(Mirror") != std::string_view::npos) return false;
                layerId = id;
                return true;
            };
            auto finish = [&]
            {
                if (!selected || actualPid == getpid()) return;
                // 只使用同一条参考记录；显式 owner metadata 优先于可见的层归属信息。
                if (parsed.ownerUid < 0) parsed.ownerUid = actualUid;
                if (parsed.ownerPid < 0 && actualPid > 0) parsed.ownerPid = actualPid;
                const int fields = (parsed.windowType >= 0) + (parsed.ownerUid >= 0) + (parsed.ownerPid >= 0);
                const int score = ((actualUid == 1000 || actualUid == 1003) ? 4 : 0) + fields;
                if (score < bestScore || (score == bestScore && layerId >= bestId)) return;
                best = parsed;
                bestScore = score;
                bestId = layerId;
            };
            std::string_view remaining(text);
            while (!remaining.empty())
            {
                const size_t newline = remaining.find('\n');
                const auto line = remaining.substr(0, newline);
                remaining.remove_prefix(newline == std::string_view::npos ? remaining.size() : newline + 1);
                auto trimmed = line;
                const auto first = trimmed.find_first_not_of(" \t\r\n");
                if (first == std::string_view::npos) continue;
                trimmed.remove_prefix(first);
                trimmed = trimmed.substr(0, trimmed.find_last_not_of(" \t\r\n") + 1);
                if (trimmed == "Layer Hierarchy" || trimmed == "Offscreen Hierarchy" || trimmed == "Offscreen Layers")
                {
                    finish();
                    inHierarchy = trimmed == "Layer Hierarchy";
                    offscreen = !inHierarchy;
                    selected = false;
                    continue;
                }
                if (offscreen) continue;
                if (inHierarchy)
                {
                    while (!trimmed.empty() && (trimmed.front() == ' ' || trimmed.substr(0, 3) == "\xe2\x94\x82")) trimmed.remove_prefix(trimmed.front() == ' ' ? 1 : 3);
                    if (trimmed.substr(0, 7) != "\xe2\x94\x94\xe2\x94\x80 " && trimmed.substr(0, 7) != "\xe2\x94\x9c\xe2\x94\x80 ") continue;
                    trimmed.remove_prefix(7);
                    parsed = {};
                    actualUid = actualPid = -1;
                    selected = selectName(trimmed);
                    if (selected)
                    {
                        readInteger(trimmed, " uid=", &actualUid);
                        readInteger(trimmed, " pid=", &actualPid);
                        finish();
                        selected = false;
                    }
                    continue;
                }
                if (line.starts_with("+ "))
                {
                    finish();
                    parsed = {};
                    actualUid = actualPid = -1;
                    selected = false;
                    const size_t open = line.find(" (", 2);
                    if (open == std::string_view::npos) continue;
                    const auto type = line.substr(2, open - 2);
                    if (type != "Layer" && type != "BufferStateLayer" && type != "BufferQueueLayer" && type != "ContainerLayer" && type != "ColorLayer" && type != "EffectLayer") continue;
                    const size_t end = line.find(')', open + 2);
                    if (end == std::string_view::npos) continue;
                    selected = selectName(line.substr(open + 2, end - open - 2));
                    if (!selected) continue;
                    readInteger(line.substr(end + 1), " uid=", &actualUid);
                    readInteger(line.substr(end + 1), " pid=", &actualPid);
                }
                if (!selected) continue;
                const size_t start = trimmed.find("metadata={");
                if (start == std::string_view::npos) continue;
                const size_t end = trimmed.find('}', start);
                if (end == std::string_view::npos) continue;
                // 在完整 metadata 块内按字段边界读取，不误认 unknownOwnerPID 或 Nbytes。
                auto metadata = trimmed.substr(start + 10, end - start - 10);
                while (!metadata.empty())
                {
                    const size_t comma = metadata.find(',');
                    auto field = metadata.substr(0, comma);
                    while (!field.empty() && (field.front() == ' ' || field.front() == '\t')) field.remove_prefix(1);
                    readInteger(field, "ownerUID:", &parsed.ownerUid, true);
                    readInteger(field, "ownerPID:", &parsed.ownerPid, true);
                    readInteger(field, "windowType:", &parsed.windowType, true);
                    if (comma == std::string_view::npos) break;
                    metadata.remove_prefix(comma + 1);
                }
            }
            finish();
            if (bestScore < 0) return -ENOENT;
            if (best.windowType >= 0) description->windowType = best.windowType;
            if (best.ownerUid >= 0) description->ownerUid = best.ownerUid;
            if (best.ownerPid >= 0) description->ownerPid = best.ownerPid;
            return status;
        }

    public:
        static ANativeWindow *Create(const char *name, int32_t width, int32_t height, bool preventCapture, int32_t windowType = -1)
        {
            auto &surfaceComposerClient = GetComposerInstance();

            if (width == -1 || height == -1)
            {
                detail::ui::DisplayState displayInfo{};
                if (surfaceComposerClient.GetDisplayInfo(&displayInfo))
                {
                    width = displayInfo.layerStackSpaceRect.width;
                    height = displayInfo.layerStackSpaceRect.height;
                }
            }
            if (width <= 0) width = 1080;
            if (height <= 0) height = 2340;

            LayerDescription description{windowType};
            if (windowType >= 0 && ReadSystemLayerDescription(name, &description) != 0) LS_LOGE_TAG("Surface", "System layer description unavailable: %s", name);
            LayerDescription recordDescription;
            if (!preventCapture && windowType >= 0 && ReadSystemLayerDescription("Display Overlays", &recordDescription) != 0) LS_LOGE_TAG("Surface", "System recording layer description unavailable");
            auto surfaceControl = surfaceComposerClient.CreateSurface(name, width, height, preventCapture, 0, true, -1, description.windowType, description.ownerUid, description.ownerPid);

            if (!surfaceControl.data) return nullptr;
            const auto &f = detail::Functionals::GetInstance();
            const auto surface = f.SurfaceControl__GetSurface(surfaceControl.data);
            if (!surface.pointer)
            {
                surfaceControl.Release();
                LS_LOGE_TAG("Surface", "无法从 SurfaceControl 获取 Surface");
                return nullptr;
            }
            auto nativeWindow = reinterpret_cast<ANativeWindow *>(reinterpret_cast<char *>(surface.pointer) + sizeof(std::max_align_t) / 2);
            if (windowType >= 0 && f.systemVersion >= 12)
            {
                int status = 0;
                detail::StrongPointer<void> child;
                if (!f.SurfaceControl__GetParentingLayer || !f.SurfaceComposerClient__Transaction__SetMetadata || !f.Parcel__Constructor || !f.Parcel__Destructor || !f.Parcel__WriteInt32) status = -ENOSYS;
                else if (!(child = f.SurfaceControl__GetParentingLayer(surfaceControl.data)).pointer) status = -ENOENT;
                else
                {
                    static detail::SurfaceComposerClientTransaction transaction;
                    detail::StrongPointer<void> root{surfaceControl.data};
                    for (const auto *target : {&root, &child})
                    {
                        if (target == &child && child.pointer == root.pointer) break;
                        if (description.ownerUid >= 0 && (status = transaction.SetMetadata(*target, 1u, description.ownerUid)) != 0) break;
                        if (description.windowType >= 0 && (status = transaction.SetMetadata(*target, 2u, description.windowType)) != 0) break;
                        if (description.ownerPid >= 0 && (status = transaction.SetMetadata(*target, 6u, description.ownerPid)) != 0) break;
                    }
                    // 已排入静态事务的字段必须提交，避免遗留到下一个窗口。
                    const int applyStatus = transaction.Apply(false, true);
                    if (status == 0) status = applyStatus;
                    f.RefBase__DecStrong(child.pointer, &child);
                }
                if (status != 0) LS_LOGE_TAG("Surface", "Layer description update failed: %s (%d)", name, status);
            }
            m_cachedSurfaceControl.emplace(nativeWindow, std::move(surfaceControl));
            m_cachedRecordDescriptions.emplace(nativeWindow, recordDescription);
            return nativeWindow;
        }

        // 查找录屏/投屏虚拟屏所在的非主 layerStack，供 AOSP mirrorSurface 方案挂载镜像层。
        static bool FindRecordDisplay(RecordDisplayInfo *recordDisplay, int32_t expectedWidth = 0, int32_t expectedHeight = 0)
        {
            if (!recordDisplay || !SupportsRecordLayerStack()) return false;

            static detail::BinderDumpReader reader("display");
            std::string dumpDisplayResult;
            const auto status = reader.Read(dumpDisplayResult, std::chrono::seconds(10));
            if (status != STATUS_OK)
            {
                LS_LOGE_TAG("Surface", "Binder dump display failed: %d", status);
                return false;
            }

            constexpr std::string_view marker = "DisplayDeviceInfo";
            const std::string_view dump = dumpDisplayResult;
            int64_t bestScore = INT64_MIN;
            for (size_t start = dump.find(marker), next; start != std::string_view::npos; start = next)
            {
                next = dump.find(marker, start + marker.size());
                const auto block = dump.substr(start, next - start);
                const auto stack = detail::DisplayField(block, "mCurrentLayerStack=");
                RecordDisplayInfo candidate{};
                if (stack.empty() || stack == "-1" || !candidate.layerStackRect.Parse(detail::DisplayField(block, "mCurrentLayerStackRect="))) continue;
                try
                {
                    const auto id = std::stoul(std::string(stack));
                    if (!id || id >= UINT32_MAX) continue;
                    candidate.layerStack = static_cast<uint32_t>(id);
                    const auto orientation = detail::DisplayField(block, "mCurrentOrientation=");
                    if (orientation.find("ROTATION_90") != std::string_view::npos) candidate.orientation = 1;
                    else if (orientation.find("ROTATION_180") != std::string_view::npos) candidate.orientation = 2;
                    else if (orientation.find("ROTATION_270") != std::string_view::npos) candidate.orientation = 3;
                    else if (!orientation.empty() && orientation.find("ROTATION_0") == std::string_view::npos) candidate.orientation = (std::stoi(std::string(orientation)) % 4 + 4) % 4;
                }
                catch (const std::logic_error &)
                {
                    continue;
                }
                // 部分 ROM 不输出 displayRect；退回 layerStackRect。
                if (!candidate.displayRect.Parse(detail::DisplayField(block, "mCurrentDisplayRect="))) candidate.displayRect = candidate.layerStackRect;
                const int64_t width = candidate.layerStackRect.Width();
                const int64_t height = candidate.layerStackRect.Height();
                if (!width || width > INT32_MAX || !height || height > INT32_MAX) continue;
                candidate.width = static_cast<int32_t>(width);
                candidate.height = static_cast<int32_t>(height);

                int64_t score = candidate.layerStack;
                if (expectedWidth > 0 && expectedHeight > 0)
                {
                    const int64_t dw = candidate.displayRect.Width();
                    const int64_t dh = candidate.displayRect.Height();
                    const auto exact = std::min(std::abs(width - expectedWidth) + std::abs(height - expectedHeight), std::abs(dw - expectedWidth) + std::abs(dh - expectedHeight));
                    const auto swapped = std::min(std::abs(width - expectedHeight) + std::abs(height - expectedWidth), std::abs(dw - expectedHeight) + std::abs(dh - expectedWidth));
                    score -= std::min(exact, swapped);
                    if (exact == 0) score += 100000;
                    else if (swapped == 0) score += 50000;
                }
                if (score <= bestScore) continue;
                bestScore = score;
                *recordDisplay = candidate;
            }
            return bestScore != INT64_MIN;
        }

        static void DestroyMirrorOnLayerStack(ANativeWindow *sourceWindow)
        {
            const auto it = m_cachedMirrors.find(sourceWindow);
            if (it == m_cachedMirrors.end()) return;
            it->second.mirror.Release();
            it->second.root.Release();
            m_cachedMirrors.erase(it);
        }

        // AOSP SurfaceFlinger::mirrorLayer 创建的根层 addToRoot=false，需要挂载到可见容器。
        static bool CreateMirrorOnLayerStack(ANativeWindow *sourceWindow, uint32_t layerStack)
        {
            if (!sourceWindow || !SupportsRecordLayerStack()) return false;

            auto sourceIt = m_cachedSurfaceControl.find(sourceWindow);
            if (sourceIt == m_cachedSurfaceControl.end() || !sourceIt->second.data) return false;

            auto &functions = detail::Functionals::GetInstance();
            constexpr uint32_t containerFlags = 0x00080000;
            const auto descriptionIt = m_cachedRecordDescriptions.find(sourceWindow);
            const LayerDescription description = descriptionIt != m_cachedRecordDescriptions.end() ? descriptionIt->second : LayerDescription{};
            auto root = GetComposerInstance().CreateSurface("Display Overlays", 0, 0, false, containerFlags, false, -1, description.windowType, description.ownerUid, description.ownerPid);
            if (!root.data) return false;
            auto mirrored = functions.SurfaceComposerClient__MirrorSurface ? functions.SurfaceComposerClient__MirrorSurface(GetComposerInstance().data, sourceIt->second.data) : functions.SurfaceComposerClient__MirrorSurfaceWithBounds(GetComposerInstance().data, sourceIt->second.data, nullptr, nullptr);
            if (!mirrored.pointer)
            {
                root.Release();
                return false;
            }

            DestroyMirrorOnLayerStack(sourceWindow);
            const auto mirrorIt = m_cachedMirrors.emplace(sourceWindow, RecordMirror{root, {mirrored.pointer}}).first;

            static detail::SurfaceComposerClientTransaction transaction;
            if (!transaction.valid)
            {
                DestroyMirrorOnLayerStack(sourceWindow);
                return false;
            }

            detail::StrongPointer<void> rootPointer{mirrorIt->second.root.data};
            detail::StrongPointer<void> mirrorPointer{mirrorIt->second.mirror.data};
            transaction.SetLayerStack(rootPointer, layerStack);
            functions.SurfaceComposerClient__Transaction__SetLayer(transaction.data, rootPointer, INT_MAX);
            functions.SurfaceComposerClient__Transaction__Show(transaction.data, rootPointer);
            functions.SurfaceComposerClient__Transaction__Reparent(transaction.data, mirrorPointer, rootPointer);
            functions.SurfaceComposerClient__Transaction__SetTrustedOverlay(transaction.data, mirrorPointer, false);
            functions.SurfaceComposerClient__Transaction__SetLayer(transaction.data, mirrorPointer, 0);
            functions.SurfaceComposerClient__Transaction__Show(transaction.data, mirrorPointer);
            if (transaction.Apply(false, true) != 0)
            {
                DestroyMirrorOnLayerStack(sourceWindow);
                return false;
            }
            return true;
        }

        static bool ConfigureMirrorProjection(ANativeWindow *sourceWindow, const DisplayInfo &source, const RecordDisplayInfo &target)
        {
            const auto found = m_cachedMirrors.find(sourceWindow);
            if (found == m_cachedMirrors.end() || source.width <= 0 || source.height <= 0) return false;
            const int64_t width = int64_t{target.layerStackRect.right} - target.layerStackRect.left;
            const int64_t height = int64_t{target.layerStackRect.bottom} - target.layerStackRect.top;
            if (width <= 0 || height <= 0) return false;

            const int rotation = (source.orientation % 4 - target.orientation % 4 + 8) % 4;
            const float rotatedWidth = rotation % 2 ? source.height : source.width;
            const float rotatedHeight = rotation % 2 ? source.width : source.height;
            const float scale = std::min(width / rotatedWidth, height / rotatedHeight);
            float x = target.layerStackRect.left + (width - rotatedWidth * scale) * 0.5f;
            float y = target.layerStackRect.top + (height - rotatedHeight * scale) * 0.5f;
            float diagonal = scale, cross = 0;
            switch (rotation)
            {
            case 1:
                diagonal = 0;
                cross = scale;
                x += source.height * scale;
                break;
            case 2:
                diagonal = -scale;
                x += source.width * scale;
                y += source.height * scale;
                break;
            case 3:
                diagonal = 0;
                cross = -scale;
                y += source.width * scale;
                break;
            }

            static detail::SurfaceComposerClientTransaction transaction;
            if (!transaction.valid) return false;
            const auto &f = detail::Functionals::GetInstance();
            detail::StrongPointer<void> rootPointer{found->second.root.data};
            // setMatrix 顺序是 dsdx, dtdx, dtdy, dsdy：x'=dsdx*x+dtdy*y+tx，y'=dtdx*x+dsdy*y+ty。
            if (!f.SurfaceComposerClient__Transaction__SetCrop(transaction.data, rootPointer, {0, 0, source.width, source.height}) || !f.SurfaceComposerClient__Transaction__SetMatrix(transaction.data, rootPointer, diagonal, cross, -cross, diagonal) || !f.SurfaceComposerClient__Transaction__SetPosition(transaction.data, rootPointer, x, y)) return false;
            return transaction.Apply(false, true) == 0;
        }

        static void Destroy(ANativeWindow *nativeWindow)
        {
            if (!nativeWindow) return;
            DestroyMirrorOnLayerStack(nativeWindow);
            auto it = m_cachedSurfaceControl.find(nativeWindow);
            if (it == m_cachedSurfaceControl.end()) return;

            it->second.Release(reinterpret_cast<detail::Surface *>(nativeWindow));
            m_cachedSurfaceControl.erase(it);
            m_cachedRecordDescriptions.erase(nativeWindow);
        }
    };
} // namespace android

#undef ResolveMethod
#endif // SURFACE_CONTROL_MANAGER_H
