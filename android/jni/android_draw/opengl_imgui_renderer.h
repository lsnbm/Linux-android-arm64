#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <memory>
#include <optional>

#include <android/native_window.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

// Android/ImGui 依赖
#include "android_imgui_input/imgui_touch_input.h"
#include "android_surface/surface_control_manager.h"
#include "imgui/backends/imgui_impl_android.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/imgui.h"
#include "logger/logger.h"

// OpenGL ES 后端实现
class OpenGLRenderer
{
    EGLDisplay display = EGL_NO_DISPLAY;
    EGLSurface surface = EGL_NO_SURFACE;
    EGLContext context = EGL_NO_CONTEXT;
    ImGuiContext *imgui_context = nullptr;
    bool android_backend_initialized = false;
    bool opengl_backend_initialized = false;
    ANativeWindow *native_window = nullptr;
    android::SurfaceControlManager::DisplayInfo displayInfo{};
    std::chrono::steady_clock::time_point lastDisplayInfoQuery_{};

    struct ImGuiContextScope
    {
        ImGuiContext *previous = ImGui::GetCurrentContext();
        explicit ImGuiContextScope(ImGuiContext *current) { ImGui::SetCurrentContext(current); }
        ~ImGuiContextScope() { ImGui::SetCurrentContext(previous); }
    };

    struct RecordSurfaceState
    {
        android::SurfaceControlManager::RecordDisplayInfo display;
        android::SurfaceControlManager::DisplayInfo source;
    };
    std::optional<RecordSurfaceState> recordSurface_;
    std::optional<android::SurfaceControlManager::RecordDisplayInfo> cachedRecordDisplay_;
    std::optional<android::SurfaceControlManager::RecordDisplayInfo> pendingRecordDisplay_;
    bool recordDisplayMissed_ = false;
    std::chrono::steady_clock::time_point lastRecordDisplayScan_{};
    bool allowCapture_ = false;

    static bool SameRecordDisplay(const android::SurfaceControlManager::RecordDisplayInfo &lhs, const android::SurfaceControlManager::RecordDisplayInfo &rhs)
    {
        return lhs.layerStack == rhs.layerStack && lhs.orientation == rhs.orientation && lhs.layerStackRect == rhs.layerStackRect && lhs.displayRect == rhs.displayRect;
    }

    void CleanupRecordSurface()
    {
        if (!recordSurface_) return;
        if (android::gSurfaceControl) android::gSurfaceControl->DestroyMirrorOnLayerStack(native_window);
        recordSurface_.reset();
    }

    void shutdown()
    {
        CleanupRecordSurface();
        cachedRecordDisplay_.reset();
        pendingRecordDisplay_.reset();
        recordDisplayMissed_ = false;
        lastRecordDisplayScan_ = {};

        if (imgui_context)
        {
            ImGuiContextScope scope{imgui_context};
            if (scope.previous == imgui_context) scope.previous = nullptr;
            if (opengl_backend_initialized) ImGui_ImplOpenGL3_Shutdown();
            if (android_backend_initialized) ImGui_ImplAndroid_Shutdown();
            ImGui::DestroyContext(imgui_context);
            imgui_context = nullptr;
        }
        opengl_backend_initialized = false;
        android_backend_initialized = false;

        if (display != EGL_NO_DISPLAY)
        {
            if (context != EGL_NO_CONTEXT && eglGetCurrentContext() == context)
                eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT) eglDestroyContext(display, context);
            if (surface != EGL_NO_SURFACE) eglDestroySurface(display, surface);
            eglTerminate(display);
            display = EGL_NO_DISPLAY;
            context = EGL_NO_CONTEXT;
            surface = EGL_NO_SURFACE;
        }
        if (native_window)
        {
            if (android::gSurfaceControl) android::gSurfaceControl->Destroy(native_window);
            ANativeWindow_release(native_window);
            native_window = nullptr;
        }
    }

public:
    OpenGLRenderer() = default;
    OpenGLRenderer(const OpenGLRenderer &) = delete;
    OpenGLRenderer &operator=(const OpenGLRenderer &) = delete;
    ~OpenGLRenderer() { shutdown(); }

    template <class Setup> int init(bool allowCapture, Setup &&setup, const char *windowName = "ImGui", int32_t windowType = -1)
    {
        int status = 0;
        if (opengl_backend_initialized) return status;
        if (!gTouchInput || !android::gSurfaceControl || !android::gBinderDump) return status = -ENODEV;
        // 不借用外部 context；失败回滚及退出只清理本实例创建的 context。
        if (ImGui::GetCurrentContext()) return status = -EBUSY;
        struct Rollback
        {
            OpenGLRenderer *renderer;
            ~Rollback() { if (renderer) renderer->shutdown(); }
        } rollback{this};

        allowCapture_ = allowCapture;
        displayInfo = android::gSurfaceControl->GetDisplayInfo();
        lastDisplayInfoQuery_ = std::chrono::steady_clock::now();
        if (displayInfo.width <= 0 || displayInfo.height <= 0)
        {
            displayInfo.width = 1080;
            displayInfo.height = 2340;
            displayInfo.orientation = 0;
        }
        // 主 SurfaceControl 只创建一次，旋转时由系统显示投影改变方向，不重建 layer。
        // 缓冲保持长边方形：setBuffersGeometry 只改缓冲像素尺寸，不改图层几何，旋转时交换宽高会拉伸画面。
        // displayInfo.width/height 仍是屏幕逻辑尺寸，用于界面布局和触摸坐标。
        const int max_side = std::max(displayInfo.width, displayInfo.height);
        native_window = android::gSurfaceControl->Create(windowName, max_side, max_side, !allowCapture, windowType);
        if (!native_window)
        {
            LS_LOGE_TAG("RenderGL", "创建 ANativeWindow 失败");
            status = -ENODEV;
            return status;
        }
        ANativeWindow_acquire(native_window);

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr))
        {
            status = eglGetError();
            return status;
        }

        EGLConfig config{};
        EGLint num_config = 0;
        const EGLint attribList[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 8, EGL_NONE};
        if (!eglChooseConfig(display, attribList, &config, 1, &num_config)) status = eglGetError();
        else if (num_config == 0) status = EGL_BAD_CONFIG;
        if (status != 0) return status;

        EGLint egl_format = 0;
        if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format))
        {
            status = eglGetError();
            return status;
        }
        status = ANativeWindow_setBuffersGeometry(native_window, 0, 0, egl_format);
        if (status != 0) return status;

        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
        if (context == EGL_NO_CONTEXT)
        {
            status = eglGetError();
            return status;
        }
        surface = eglCreateWindowSurface(display, config, native_window, nullptr);
        if (surface == EGL_NO_SURFACE || !eglMakeCurrent(display, surface, surface, context))
        {
            status = eglGetError();
            return status;
        }

        // 初始化 ImGui
        IMGUI_CHECKVERSION();
        imgui_context = ImGui::CreateContext();
        if (!imgui_context) return status = -ENOMEM;
        android_backend_initialized = ImGui_ImplAndroid_Init(native_window);
        if (!android_backend_initialized)
        {
            status = -EIO;
            return status;
        }
        opengl_backend_initialized = ImGui_ImplOpenGL3_Init("#version 300 es");
        if (!opengl_backend_initialized)
        {
            status = -EIO;
            return status;
        }

        // 回调只配置当前由本实例持有的 context，不切换或销毁它。
        setup();
        // 在输入线程启动前设置触摸坐标所用的屏幕尺寸和方向。
        gTouchInput->setScreen(displayInfo.width, displayInfo.height, displayInfo.orientation);
        status = gTouchInput->start();
        if (status != 0)
        {
            LS_LOGE_TAG("RenderGL", "初始化触摸输入失败: %d", status);
            return status;
        }
        rollback.renderer = nullptr;
        return status;
    }

    template <typename Draw> int drawFrame(Draw &&draw)
    {
        int status = 0;
        if (!opengl_backend_initialized || !gTouchInput || !android::gSurfaceControl)
        {
            status = -ENODEV;
            return status;
        }
        ImGuiContextScope scope{imgui_context};

        const auto now = std::chrono::steady_clock::now();
        if (now - lastDisplayInfoQuery_ >= std::chrono::milliseconds(250))
        {
            lastDisplayInfoQuery_ = now;
            const auto updated = android::gSurfaceControl->GetDisplayInfo();
            if (updated.width > 0 && updated.height > 0 &&
                (updated.width != displayInfo.width || updated.height != displayInfo.height || updated.orientation != displayInfo.orientation))
            {
                displayInfo = updated;
                gTouchInput->setScreen(displayInfo.width, displayInfo.height, displayInfo.orientation);
                // 不等待常规轮询周期，下一次呈现后立即刷新录屏投影。
                lastRecordDisplayScan_ = {};
            }
        }

        gTouchInput->updateImGui();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();
        const bool block = draw(displayInfo.width, displayInfo.height);
        ImGui::Render();
        status = gTouchInput->setBlocking(block);
        if (status != 0) return status;

        // EGL 管理窗口缓冲区的大小变化，不需要像 Vulkan 一样重建交换链。
        const auto &size = ImGui::GetIO().DisplaySize;
        glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!eglSwapBuffers(display, surface))
        {
            status = eglGetError();
            return status;
        }

        // 检测录屏虚拟显示的目标 layerStack，将主绘制层镜像到该显示；镜像不可用不终止主帧。
        if (!allowCapture_) return status;
        const auto presentedAt = std::chrono::steady_clock::now();
        if (lastRecordDisplayScan_ == std::chrono::steady_clock::time_point{} || presentedAt - lastRecordDisplayScan_ >= std::chrono::seconds(2))
        {
            lastRecordDisplayScan_ = presentedAt;
            android::SurfaceControlManager::RecordDisplayInfo scanned{};
            if (android::gSurfaceControl->FindRecordDisplay(&scanned, displayInfo.width, displayInfo.height) != 0)
            {
                if (recordDisplayMissed_) cachedRecordDisplay_.reset();
                recordDisplayMissed_ = true;
                pendingRecordDisplay_.reset();
            }
            else
            {
                recordDisplayMissed_ = false;
                // 已有镜像时，连续两次扫描到同一新目标才切换 layerStack。
                if (!recordSurface_ || !cachedRecordDisplay_ || scanned.layerStack == cachedRecordDisplay_->layerStack ||
                    (pendingRecordDisplay_ && SameRecordDisplay(scanned, *pendingRecordDisplay_)))
                {
                    cachedRecordDisplay_ = scanned;
                    pendingRecordDisplay_.reset();
                }
                else pendingRecordDisplay_ = scanned;
            }
        }
        if (!cachedRecordDisplay_)
        {
            CleanupRecordSurface();
            return status;
        }

        const auto &target = *cachedRecordDisplay_;
        if (!recordSurface_ || recordSurface_->display.layerStack != target.layerStack)
        {
            CleanupRecordSurface();
            if (android::gSurfaceControl->CreateMirrorOnLayerStack(native_window, target.layerStack) != 0) return status;
            recordSurface_.emplace();
        }
        else if (SameRecordDisplay(recordSurface_->display, target) && recordSurface_->source.orientation == displayInfo.orientation &&
                 recordSurface_->source.width == displayInfo.width && recordSurface_->source.height == displayInfo.height) return status;

        if (android::gSurfaceControl->ConfigureMirrorProjection(native_window, displayInfo, target) != 0)
        {
            CleanupRecordSurface();
            return status;
        }
        recordSurface_->display = target;
        recordSurface_->source = displayInfo;
        LS_LOGI_TAG_FMT("RenderGL", "录屏镜像层就绪 layerStack={} orientation={} stackRect=({},{}-{},{}), displayRect=({},{}-{},{}), source={}x{}", target.layerStack, target.orientation, target.layerStackRect.left, target.layerStackRect.top, target.layerStackRect.right, target.layerStackRect.bottom, target.displayRect.left, target.displayRect.top, target.displayRect.right, target.displayRect.bottom, displayInfo.width, displayInfo.height);
        return status;
    }
};

inline std::unique_ptr<OpenGLRenderer> gOpenGLRenderer = nullptr;
