#pragma once

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

// OpenGL ES 后端实现
namespace RenderGL
{
    static EGLDisplay display = EGL_NO_DISPLAY;
    static EGLConfig config;
    static EGLSurface surface = EGL_NO_SURFACE;
    static EGLContext context = EGL_NO_CONTEXT;

    static ANativeWindow *native_window = nullptr;
    static android::SurfaceControlManager::DisplayInfo displayInfo{};

    inline void shutdown();

    inline bool init(bool preventCapture)
    {
        printf("[initEGLGUI] 开始初始化 EGL 和 GUI...\n");
        displayInfo = android::SurfaceControlManager::GetDisplayInfo();

        // 初始化触摸屏幕参数 (重要)
        UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);

        // 根据方向决定宽高创建窗口
        int w = displayInfo.width;
        int h = displayInfo.height;
        // 确保创建窗口时使用较大的边作为宽
        int max_side = (h > w ? h : w);

        native_window = android::SurfaceControlManager::Create("Lark", max_side, max_side, preventCapture); // true为开启防止截屏/录屏
        if (!native_window) return false;

        ANativeWindow_acquire(native_window);

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eglInitialize(display, 0, 0) != EGL_TRUE) return false;

        EGLint num_config = 0;
        const EGLint attribList[] = {EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT, EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 8, EGL_NONE};

        eglChooseConfig(display, attribList, &config, 1, &num_config);

        EGLint egl_format;
        eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format);
        ANativeWindow_setBuffersGeometry(native_window, 0, 0, egl_format);

        const EGLint attrib_list[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
        surface = eglCreateWindowSurface(display, config, native_window, nullptr);

        if (!eglMakeCurrent(display, surface, surface, context)) return false;

    // 初始化 ImGui
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = NULL;

        ImGui_ImplAndroid_Init(native_window);
        ImGui_ImplOpenGL3_Init("#version 300 es");

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
        displayInfo = android::SurfaceControlManager::GetDisplayInfo();
        Touch_UpdateImGui();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();

        // 屏幕被旋转检测
        if (orientation.load(std::memory_order_relaxed) != static_cast<uint32_t>(displayInfo.orientation))
        {
            // 屏幕旋转时更新触摸映射参数
            UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);

            ImGuiWindow *g_window = ImGui::GetCurrentWindow();
            if (g_window)
            {
                g_window->Pos.x = 100;
                g_window->Pos.y = 125;
            }
        }
    }

    inline void drawEnd()
    {
        ImGuiIO &io = ImGui::GetIO();
        glViewport(0.0f, 0.0f, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();

        if (display == EGL_NO_DISPLAY) return;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        eglSwapBuffers(display, surface);
    }

    inline void shutdown()
    {
        Touch_Shutdown();

        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplAndroid_Shutdown();
        ImGui::DestroyContext();

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
            ANativeWindow_release(native_window);
            native_window = nullptr;
        }
    }
} // namespace RenderGL