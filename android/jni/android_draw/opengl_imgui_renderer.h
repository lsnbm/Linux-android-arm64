#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>

#include <android/native_window.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl3.h>

#include "android_imgui_input/imgui_touch_input.h"
#include "android_surface/surface_control_manager.h"
#include "font/oppo_font.h"
#include "imgui/backends/imgui_impl_android.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/imgui.h"
#include "logger/logger.h"

namespace RenderGL
{
    static EGLDisplay display = EGL_NO_DISPLAY;
    static EGLSurface surface = EGL_NO_SURFACE;
    static EGLContext context = EGL_NO_CONTEXT;
    static ANativeWindow *native_window = nullptr;
    static android::SurfaceControlManager::DisplayInfo displayInfo{};
    static std::chrono::steady_clock::time_point s_lastDisplayInfoQuery{};
    static bool imgui_context_created = false;
    static bool android_backend_initialized = false;
    static bool opengl_backend_initialized = false;

    inline void shutdown()
    {
        Touch_Shutdown();
        if (opengl_backend_initialized) ImGui_ImplOpenGL3_Shutdown();
        if (android_backend_initialized) ImGui_ImplAndroid_Shutdown();
        if (imgui_context_created) ImGui::DestroyContext();
        opengl_backend_initialized = false;
        android_backend_initialized = false;
        imgui_context_created = false;

        if (display != EGL_NO_DISPLAY)
        {
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
            android::SurfaceControlManager::Destroy(native_window);
            ANativeWindow_release(native_window);
            native_window = nullptr;
        }
    }

    inline int init(bool allowCapture)
    {
        int status = 0;
        if (opengl_backend_initialized) return status;

        LS_LOGD_TAG("Render", "开始初始化 EGL 和 GUI");
        displayInfo = android::SurfaceControlManager::GetDisplayInfo();
        s_lastDisplayInfoQuery = std::chrono::steady_clock::now();
        if (displayInfo.width <= 0 || displayInfo.height <= 0)
        {
            displayInfo.width = 1080;
            displayInfo.height = 2340;
            displayInfo.orientation = 0;
        }
        UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);

        // SurfaceFlinger 负责旋转投影；方形缓冲不随逻辑屏幕宽高交换。
        const int max_side = std::max(displayInfo.width, displayInfo.height);
        native_window = android::SurfaceControlManager::Create("Lark", max_side, max_side, !allowCapture);
        if (!native_window)
        {
            status = -ENODEV;
            return status;
        }
        ANativeWindow_acquire(native_window);

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY || !eglInitialize(display, nullptr, nullptr))
        {
            status = eglGetError();
            shutdown();
            return status;
        }

        EGLConfig config{};
        EGLint num_config = 0;
        const EGLint attribList[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 8, EGL_NONE};
        if (!eglChooseConfig(display, attribList, &config, 1, &num_config)) status = eglGetError();
        else if (num_config == 0) status = EGL_BAD_CONFIG;
        if (status != 0)
        {
            shutdown();
            return status;
        }

        EGLint egl_format = 0;
        if (!eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format))
        {
            status = eglGetError();
            shutdown();
            return status;
        }
        status = ANativeWindow_setBuffersGeometry(native_window, 0, 0, egl_format);
        if (status != 0)
        {
            shutdown();
            return status;
        }

        const EGLint contextAttribs[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, contextAttribs);
        if (context == EGL_NO_CONTEXT)
        {
            status = eglGetError();
            shutdown();
            return status;
        }
        surface = eglCreateWindowSurface(display, config, native_window, nullptr);
        if (surface == EGL_NO_SURFACE || !eglMakeCurrent(display, surface, surface, context))
        {
            status = eglGetError();
            shutdown();
            return status;
        }

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imgui_context_created = true;
        android_backend_initialized = ImGui_ImplAndroid_Init(native_window);
        if (!android_backend_initialized)
        {
            status = -EIO;
            shutdown();
            return status;
        }
        opengl_backend_initialized = ImGui_ImplOpenGL3_Init("#version 300 es");
        if (!opengl_backend_initialized)
        {
            status = -EIO;
            shutdown();
            return status;
        }

        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = nullptr;
        ImFontConfig font_cfg;
        font_cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void *)OPPOSans_H, OPPOSans_H_size, 31.0f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull());
        ImGui::GetStyle().ScaleAllSizes(3.0f);
        if (!Touch_Init())
        {
            status = -EIO;
            shutdown();
            return status;
        }
        return status;
    }

    inline void drawBegin()
    {
        if (!opengl_backend_initialized) return;

        const auto now = std::chrono::steady_clock::now();
        if (now - s_lastDisplayInfoQuery >= std::chrono::milliseconds(250))
        {
            s_lastDisplayInfoQuery = now;
            const auto updated = android::SurfaceControlManager::GetDisplayInfo();
            if (updated.width > 0 && updated.height > 0 &&
                (updated.width != displayInfo.width || updated.height != displayInfo.height || updated.orientation != displayInfo.orientation))
            {
                displayInfo = updated;
                UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);
            }
        }
        Touch_UpdateImGui();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();
    }

    inline int drawEnd()
    {
        int status = 0;
        if (!opengl_backend_initialized)
        {
            status = -ENODEV;
            return status;
        }

        ImGui::Render();
        const auto &size = ImGui::GetIO().DisplaySize;
        glViewport(0, 0, static_cast<GLsizei>(size.x), static_cast<GLsizei>(size.y));
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (!eglSwapBuffers(display, surface)) status = eglGetError();
        return status;
    }
} // namespace RenderGL