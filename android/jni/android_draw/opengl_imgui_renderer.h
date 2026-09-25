#pragma once

#include "../logger/logger.h"
#include <atomic>
#include <cstdio>

#include <android/native_window.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES3/gl3ext.h>
#include <GLES3/gl32.h>
#include <GLES3/gl3platform.h>

// Android/ImGui 依赖
#include "imgui/backends/imgui_impl_android.h"
#include "imgui/backends/imgui_impl_opengl3.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "android_imgui_input/imgui_touch_input.h"
#include "android_surface/surface_control_manager.h"
#include "font/oppo_font.h"
#include <chrono>

// OpenGL ES 后端实现
namespace RenderGL
{
    static EGLDisplay display = EGL_NO_DISPLAY;
    static EGLConfig config;
    static EGLSurface surface = EGL_NO_SURFACE;
    static EGLContext context = EGL_NO_CONTEXT;

    static ANativeWindow *native_window = nullptr;
    static android::SurfaceControlManager::DisplayInfo displayInfo{};
    static std::chrono::steady_clock::time_point s_lastDisplayInfoQuery{};
    static bool imgui_context_created = false;
    static bool android_backend_initialized = false;
    static bool opengl_backend_initialized = false;

    inline void shutdown();

    inline bool init(bool allowCapture)
    {
        if (imgui_context_created) return true;

        LS_LOGD_TAG("Render", "开始初始化 EGL 和 GUI");
        displayInfo = android::SurfaceControlManager::GetDisplayInfo();
        if (displayInfo.width <= 0 || displayInfo.height <= 0)
        {
            displayInfo.width = 1080;
            displayInfo.height = 2340;
            displayInfo.orientation = 0;
        }

        // 初始化触摸屏幕参数 (重要)
        UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);

        // 根据方向决定宽高创建窗口
        int w = displayInfo.width;
        int h = displayInfo.height;
        // 确保创建窗口时使用较大的边作为宽
        int max_side = (h > w ? h : w);

        native_window = android::SurfaceControlManager::Create("Lark", max_side, max_side, !allowCapture);
        if (!native_window) return false;

        ANativeWindow_acquire(native_window);

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (display == EGL_NO_DISPLAY || eglInitialize(display, 0, 0) != EGL_TRUE)
        {
            shutdown();
            return false;
        }

        EGLint num_config = 0;
        const EGLint attribList[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 8, EGL_NONE};

        if (eglChooseConfig(display, attribList, &config, 1, &num_config) != EGL_TRUE || num_config <= 0)
        {
            shutdown();
            return false;
        }

        EGLint egl_format = 0;
        if (eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format) != EGL_TRUE)
        {
            shutdown();
            return false;
        }
        ANativeWindow_setBuffersGeometry(native_window, 0, 0, egl_format);

        const EGLint attrib_list[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
        if (context == EGL_NO_CONTEXT)
        {
            shutdown();
            return false;
        }
        surface = eglCreateWindowSurface(display, config, native_window, nullptr);
        if (surface == EGL_NO_SURFACE)
        {
            shutdown();
            return false;
        }

        if (!eglMakeCurrent(display, surface, surface, context))
        {
            shutdown();
            return false;
        }

        // 初始化 ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imgui_context_created = true;
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = NULL;

        android_backend_initialized = ImGui_ImplAndroid_Init(native_window);
        opengl_backend_initialized = ImGui_ImplOpenGL3_Init("#version 300 es");
        if (!android_backend_initialized || !opengl_backend_initialized)
        {
            shutdown();
            return false;
        }

        ImFontConfig font_cfg;
        font_cfg.SizePixels = 31.0f;
        font_cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void *)OPPOSans_H, OPPOSans_H_size, 31.0f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull());

        ImGui::GetStyle().ScaleAllSizes(3.0f);

        if (!Touch_Init())
        {
            shutdown();
            return false;
        }
        return true;
    }

    inline void drawBegin()
    {
        if (!imgui_context_created || !opengl_backend_initialized) return;

        const auto now = std::chrono::steady_clock::now();
        if (s_lastDisplayInfoQuery == std::chrono::steady_clock::time_point{} || now - s_lastDisplayInfoQuery >= std::chrono::milliseconds(250))
        {
            s_lastDisplayInfoQuery = now;
            const auto updatedDisplayInfo = android::SurfaceControlManager::GetDisplayInfo();
            if (updatedDisplayInfo.width > 0 && updatedDisplayInfo.height > 0)
            {
                displayInfo = updatedDisplayInfo;
            }
        }
        if (displayInfo.width <= 0 || displayInfo.height <= 0)
        {
            displayInfo.orientation = static_cast<int32_t>(orientation.load(std::memory_order_relaxed));
            displayInfo.width = static_cast<int32_t>(screenWidth.load(std::memory_order_relaxed));
            displayInfo.height = static_cast<int32_t>(screenHeight.load(std::memory_order_relaxed));
        }

        if (orientation.load(std::memory_order_relaxed) != static_cast<uint32_t>(displayInfo.orientation)) UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);

        Touch_UpdateImGui();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();
    }

    inline void drawEnd()
    {
        if (!imgui_context_created || !opengl_backend_initialized) return;

        ImGui::Render();
        if (display == EGL_NO_DISPLAY || surface == EGL_NO_SURFACE) return;

        ImGuiIO &io = ImGui::GetIO();
        glViewport(0.0f, 0.0f, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        if (eglSwapBuffers(display, surface) != EGL_TRUE)
        {
            EGLint error = eglGetError();
            LS_LOGE_TAG("Render", "eglSwapBuffers 失败: 0x%x", error);
        }
    }

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
        }
        display = EGL_NO_DISPLAY;
        context = EGL_NO_CONTEXT;
        surface = EGL_NO_SURFACE;
        if (native_window)
        {
            android::SurfaceControlManager::Destroy(native_window);
            ANativeWindow_release(native_window);
            native_window = nullptr;
        }
    }
} // namespace RenderGL