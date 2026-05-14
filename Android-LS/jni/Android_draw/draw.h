#pragma once
#include <iostream>
#include <thread>
#include <chrono>
#include <dlfcn.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <cstdio>
#include <cstring>
#include <cerrno>
#include <cstdlib>
#include <vector>
#include <print>

// Android/ImGui 依赖
#include <android/native_window.h>
#include "native_surface/ANativeWindowCreator.h"
#include "ImGui/imgui.h"
#include "ImGui/font/Font.h"
#include "ImGui/imgui_internal.h"
#include "ImGui/backends/imgui_impl_android.h"
#include "Android_touch/TouchHelperA.h"

//  公共辅助函数 (独立于渲染引擎)
namespace Utils
{
    // 启动游戏
    inline void startGameActivity(const char *packageName, const char *activityName)
    {
        char cmd[128];
        sprintf(cmd, "pm path %s > /dev/null 2>&1", packageName);
        if (system(cmd) != 0)
        {
            std::println(stderr, "[Utils] 错误: {} 版本游戏未安装，请先安装。", packageName);
            exit(1);
        }
        sprintf(cmd, "am start %s/%s > /dev/null 2>&1", packageName, activityName);
        system(cmd);
    }

    // 绘制帧率
    inline void DrawFPS(ImDrawList *drawList)
    {
        float fps = ImGui::GetIO().Framerate;
        char fpsText[32];
        snprintf(fpsText, sizeof(fpsText), "FPS: %.1f", fps);

        ImVec2 pos(10.0f, 10.0f);
        ImU32 colorText = IM_COL32(0, 255, 0, 255);
        ImU32 colorShadow = IM_COL32(0, 0, 0, 255);

        drawList->AddText(ImVec2(pos.x + 1.0f, pos.y + 1.0f), colorShadow, fpsText);
        drawList->AddText(pos, colorText, fpsText);
    }
}

//  OpenGL ES 后端实现
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES/gl.h>
#include <GLES3/gl3platform.h>
#include <GLES3/gl3ext.h>
#include <GLES3/gl32.h>
#include "ImGui/backends/imgui_impl_opengl3.h"

namespace RenderGL
{
    static EGLDisplay display = EGL_NO_DISPLAY;
    static EGLConfig config;
    static EGLSurface surface = EGL_NO_SURFACE;
    static EGLContext context = EGL_NO_CONTEXT;

    static ANativeWindow *native_window = nullptr;
    static android::ANativeWindowCreator::DisplayInfo displayInfo{};

    inline bool init()
    {
        printf("[initEGLGUI] 开始初始化 EGL 和 GUI...\n");
        displayInfo = android::ANativeWindowCreator::GetDisplayInfo();

        // 初始化触摸屏幕参数 (重要)
        UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);

        // 根据方向决定宽高创建窗口
        int w = displayInfo.width;
        int h = displayInfo.height;
        // 确保创建窗口时使用较大的边作为宽
        int max_side = (h > w ? h : w);

        native_window = android::ANativeWindowCreator::Create("Lark", max_side, max_side, false); // false为关闭防止录屏

        ANativeWindow_acquire(native_window);

        display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        if (eglInitialize(display, 0, 0) != EGL_TRUE)
            return false;

        EGLint num_config = 0;
        const EGLint attribList[] = {
            EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
            EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
            EGL_BLUE_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_RED_SIZE, 8, EGL_ALPHA_SIZE, 8,
            EGL_DEPTH_SIZE, 16, EGL_STENCIL_SIZE, 8, EGL_NONE};

        eglChooseConfig(display, attribList, &config, 1, &num_config);

        EGLint egl_format;
        eglGetConfigAttrib(display, config, EGL_NATIVE_VISUAL_ID, &egl_format);
        ANativeWindow_setBuffersGeometry(native_window, 0, 0, egl_format);

        const EGLint attrib_list[] = {EGL_CONTEXT_CLIENT_VERSION, 3, EGL_NONE};
        context = eglCreateContext(display, config, EGL_NO_CONTEXT, attrib_list);
        surface = eglCreateWindowSurface(display, config, native_window, nullptr);

        if (!eglMakeCurrent(display, surface, surface, context))
            return false;

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
        return true;
    }

    void drawBegin()
    {
        displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
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

    void drawEnd()
    {
        ImGuiIO &io = ImGui::GetIO();
        glViewport(0.0f, 0.0f, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        glFlush();

        if (display == EGL_NO_DISPLAY)
            return;

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        eglSwapBuffers(display, surface);
    }

    void shutdown()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplAndroid_Shutdown();
        ImGui::DestroyContext();

        if (display != EGL_NO_DISPLAY)
        {
            eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
            if (context != EGL_NO_CONTEXT)
                eglDestroyContext(display, context);
            if (surface != EGL_NO_SURFACE)
                eglDestroySurface(display, surface);
            eglTerminate(display);
        }
        display = EGL_NO_DISPLAY;
        context = EGL_NO_CONTEXT;
        surface = EGL_NO_SURFACE;
        ANativeWindow_release(native_window);
    }

}

//  Vulkan 后端实现
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>
#include "ImGui/backends/imgui_impl_vulkan.h"

namespace RenderVK
{
    const int MAX_FRAMES_IN_FLIGHT = 2;

    static VkInstance g_Instance = VK_NULL_HANDLE;
    static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
    static VkDevice g_Device = VK_NULL_HANDLE;
    static uint32_t g_QueueFamily = (uint32_t)-1;
    static VkQueue g_Queue = VK_NULL_HANDLE;
    static VkSurfaceKHR g_Surface = VK_NULL_HANDLE;
    static VkSwapchainKHR g_Swapchain = VK_NULL_HANDLE;
    static VkRenderPass g_RenderPass = VK_NULL_HANDLE;
    static VkCommandPool g_CommandPool = VK_NULL_HANDLE;
    static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;

    static VkFormat g_SwapchainFormat = VK_FORMAT_UNDEFINED;
    static VkExtent2D g_SwapchainExtent = {};
    static std::vector<VkImage> g_SwapchainImages;
    static std::vector<VkImageView> g_SwapchainImageViews;
    static std::vector<VkFramebuffer> g_Framebuffers;

    static std::vector<VkCommandBuffer> g_CommandBuffers;
    static std::vector<VkSemaphore> g_ImageAvailableSemaphores;
    static std::vector<VkSemaphore> g_RenderFinishedSemaphores;
    static std::vector<VkFence> g_InFlightFences;

    static uint32_t g_CurrentFrame = 0;
    static bool g_SwapChainRebuild = false;

    static ANativeWindow *native_window = nullptr;
    static android::ANativeWindowCreator::DisplayInfo displayInfo{};

#define VK_CHECK(x)                                                                                        \
    do                                                                                                     \
    {                                                                                                      \
        VkResult err = x;                                                                                  \
        if (err)                                                                                           \
        {                                                                                                  \
            std::println(stderr, "[RenderVK Error] VkResult = {} at {}:{}", (int)err, __FILE__, __LINE__); \
            abort();                                                                                       \
        }                                                                                                  \
    } while (0)

    inline void CleanupSwapchain(bool destroySwapchain = true)
    {
        for (auto fb : g_Framebuffers)
            vkDestroyFramebuffer(g_Device, fb, nullptr);
        g_Framebuffers.clear();

        for (auto iv : g_SwapchainImageViews)
            vkDestroyImageView(g_Device, iv, nullptr);
        g_SwapchainImageViews.clear();

        // 仅在允许销毁时释放，防止底层断开连接
        if (destroySwapchain && g_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(g_Device, g_Swapchain, nullptr);
            g_Swapchain = VK_NULL_HANDLE;
        }
    }

    inline void CreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE)
    {
        VkSurfaceCapabilitiesKHR capabilities;
        vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, g_Surface, &capabilities);

        uint32_t formatCount;
        vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &formatCount, nullptr);
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &formatCount, formats.data());

        VkSurfaceFormatKHR surfaceFormat = formats[0];
        for (const auto &availableFormat : formats)
        {
            if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM || availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                surfaceFormat = availableFormat;
                break;
            }
        }
        g_SwapchainFormat = surfaceFormat.format;

        if (capabilities.currentExtent.width != 0xFFFFFFFF)
            g_SwapchainExtent = capabilities.currentExtent;
        else
            g_SwapchainExtent = {(uint32_t)displayInfo.width, (uint32_t)displayInfo.height};

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount)
            imageCount = capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = g_Surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = g_SwapchainExtent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;

        // 强制 Identity Transform，防止 Android 对正方形缓冲进行额外的错误拉伸
        if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
            createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        else
            createInfo.preTransform = capabilities.currentTransform;

        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // Vsync
        createInfo.clipped = VK_TRUE;

        // 传入旧交换链给 Android 底层，实现无缝过渡
        createInfo.oldSwapchain = oldSwapchain;

        VK_CHECK(vkCreateSwapchainKHR(g_Device, &createInfo, nullptr, &g_Swapchain));

        // 新交换链连接成功后，再安全销毁旧链
        if (oldSwapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(g_Device, oldSwapchain, nullptr);
        }

        vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &imageCount, nullptr);
        g_SwapchainImages.resize(imageCount);
        vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &imageCount, g_SwapchainImages.data());

        g_SwapchainImageViews.resize(g_SwapchainImages.size());
        for (size_t i = 0; i < g_SwapchainImages.size(); i++)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = g_SwapchainImages[i];
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = g_SwapchainFormat;
            viewInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.layerCount = 1;
            VK_CHECK(vkCreateImageView(g_Device, &viewInfo, nullptr, &g_SwapchainImageViews[i]));
        }
    }
    inline void RecreateSwapchain()
    {
        vkDeviceWaitIdle(g_Device);

        // 提取当前的交换链
        VkSwapchainKHR oldSwapchain = g_Swapchain;

        // 销毁 Framebuffer / ImageView，但保留 g_Swapchain (传 false)
        CleanupSwapchain(false);

        // 携带旧链进行重建
        CreateSwapchain(oldSwapchain);

        g_Framebuffers.resize(g_SwapchainImageViews.size());
        for (size_t i = 0; i < g_SwapchainImageViews.size(); i++)
        {
            VkImageView attachments[] = {g_SwapchainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = g_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = g_SwapchainExtent.width;
            framebufferInfo.height = g_SwapchainExtent.height;
            framebufferInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(g_Device, &framebufferInfo, nullptr, &g_Framebuffers[i]));
        }
    }

    inline bool init()
    {
        displayInfo = android::ANativeWindowCreator::GetDisplayInfo();
        if (displayInfo.width == 0 || displayInfo.height == 0)
        {
            displayInfo.width = 1080;
            displayInfo.height = 2340;
            displayInfo.orientation = 0;
        }
        UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);

        int w = displayInfo.width;
        int h = displayInfo.height;
        int max_side = (h > w ? h : w);

        native_window = android::ANativeWindowCreator::Create("Lark", max_side, max_side, false);
        if (native_window == nullptr)
        {
            std::println(stderr, "[RenderVK Error] Failed to create ANativeWindow!");
            return false;
        }
        ANativeWindow_acquire(native_window);

        const char *instance_extensions[] = {"VK_KHR_surface", "VK_KHR_android_surface"};
        VkInstanceCreateInfo create_info{};
        create_info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        create_info.enabledExtensionCount = 2;
        create_info.ppEnabledExtensionNames = instance_extensions;
        VK_CHECK(vkCreateInstance(&create_info, nullptr, &g_Instance));

        VkAndroidSurfaceCreateInfoKHR surface_info{};
        surface_info.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        surface_info.window = native_window;
        VK_CHECK(vkCreateAndroidSurfaceKHR(g_Instance, &surface_info, nullptr, &g_Surface));

        uint32_t gpu_count = 0;
        VK_CHECK(vkEnumeratePhysicalDevices(g_Instance, &gpu_count, nullptr));
        if (gpu_count == 0)
            return false;
        std::vector<VkPhysicalDevice> gpus(gpu_count);
        VK_CHECK(vkEnumeratePhysicalDevices(g_Instance, &gpu_count, gpus.data()));
        g_PhysicalDevice = gpus[0];

        uint32_t queueFamilyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &queueFamilyCount, nullptr);
        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(g_PhysicalDevice, &queueFamilyCount, queueFamilies.data());
        for (uint32_t i = 0; i < queueFamilyCount; i++)
        {
            VkBool32 presentSupport = false;
            vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, i, g_Surface, &presentSupport);
            if (queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT && presentSupport)
            {
                g_QueueFamily = i;
                break;
            }
        }

        const char *device_extensions[] = {"VK_KHR_swapchain"};
        float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = g_QueueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceCreateInfo deviceCreateInfo{};
        deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.enabledExtensionCount = 1;
        deviceCreateInfo.ppEnabledExtensionNames = device_extensions;
        VK_CHECK(vkCreateDevice(g_PhysicalDevice, &deviceCreateInfo, nullptr, &g_Device));
        vkGetDeviceQueue(g_Device, g_QueueFamily, 0, &g_Queue);

        CreateSwapchain();

        VkAttachmentDescription colorAttachment{};
        colorAttachment.format = g_SwapchainFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

        VkAttachmentReference colorAttachmentRef{};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

        VkSubpassDescription subpass{};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        VK_CHECK(vkCreateRenderPass(g_Device, &renderPassInfo, nullptr, &g_RenderPass));

        g_Framebuffers.resize(g_SwapchainImageViews.size());
        for (size_t i = 0; i < g_SwapchainImageViews.size(); i++)
        {
            VkImageView attachments[] = {g_SwapchainImageViews[i]};
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = g_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = g_SwapchainExtent.width;
            framebufferInfo.height = g_SwapchainExtent.height;
            framebufferInfo.layers = 1;
            VK_CHECK(vkCreateFramebuffer(g_Device, &framebufferInfo, nullptr, &g_Framebuffers[i]));
        }

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = g_QueueFamily;
        VK_CHECK(vkCreateCommandPool(g_Device, &poolInfo, nullptr, &g_CommandPool));

        g_CommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = g_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = (uint32_t)g_CommandBuffers.size();
        VK_CHECK(vkAllocateCommandBuffers(g_Device, &allocInfo, g_CommandBuffers.data()));

        g_ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        g_RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        g_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VK_CHECK(vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &g_ImageAvailableSemaphores[i]));
            VK_CHECK(vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &g_RenderFinishedSemaphores[i]));
            VK_CHECK(vkCreateFence(g_Device, &fenceInfo, nullptr, &g_InFlightFences[i]));
        }

        VkDescriptorPoolSize pool_sizes[] = {
            {VK_DESCRIPTOR_TYPE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000},
            {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000},
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000},
            {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descPoolInfo.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        descPoolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        descPoolInfo.pPoolSizes = pool_sizes;
        VK_CHECK(vkCreateDescriptorPool(g_Device, &descPoolInfo, nullptr, &g_DescriptorPool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = NULL;

        ImGui_ImplAndroid_Init(native_window);

        ImGui_ImplVulkan_InitInfo init_info = {};
        init_info.Instance = g_Instance;
        init_info.PhysicalDevice = g_PhysicalDevice;
        init_info.Device = g_Device;
        init_info.QueueFamily = g_QueueFamily;
        init_info.Queue = g_Queue;
        init_info.PipelineCache = VK_NULL_HANDLE;
        init_info.DescriptorPool = g_DescriptorPool;
        init_info.MinImageCount = g_SwapchainImages.size();
        init_info.ImageCount = g_SwapchainImages.size();
        init_info.Allocator = nullptr;

        init_info.PipelineInfoMain.RenderPass = g_RenderPass;
        init_info.PipelineInfoMain.Subpass = 0;
        init_info.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        init_info.CheckVkResultFn = [](VkResult err)
        { if(err) std::println(stderr, "[ImGui Vulkan Error] {}", (int)err); };

        ImGui_ImplVulkan_Init(&init_info);

        ImFontConfig font_cfg;
        font_cfg.SizePixels = 31.0f;
        font_cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void *)OPPOSans_H, OPPOSans_H_size, 31.0f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull());
        ImGui::GetStyle().ScaleAllSizes(3.0f);

        return true;
    }
    inline void drawBegin()
    {
        displayInfo = android::ANativeWindowCreator::GetDisplayInfo();

        bool rotated = (orientation.load(std::memory_order_relaxed) != static_cast<uint32_t>(displayInfo.orientation));

        if (g_SwapChainRebuild || rotated)
        {
            if (rotated)
            {
                UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);
            }
            RecreateSwapchain();
            g_SwapChainRebuild = false;
        }

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();

        // 旋转或启动时，强制将菜单拉回左上角安全位置
        static bool s_reposition = true;
        if (rotated)
            s_reposition = true;

        if (s_reposition)
        {
            ImGui::SetNextWindowPos(ImVec2(100.0f, 100.0f), ImGuiCond_Always);
            s_reposition = false; // 仅生效一帧，之后允许用户自由拖拽
        }
    }

    inline void drawEnd()
    {
        ImGui::Render();
        ImDrawData *draw_data = ImGui::GetDrawData();

        VK_CHECK(vkWaitForFences(g_Device, 1, &g_InFlightFences[g_CurrentFrame], VK_TRUE, UINT64_MAX));

        uint32_t imageIndex;
        VkResult result = vkAcquireNextImageKHR(g_Device, g_Swapchain, UINT64_MAX, g_ImageAvailableSemaphores[g_CurrentFrame], VK_NULL_HANDLE, &imageIndex);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            g_SwapChainRebuild = true;
            return;
        }

        else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
        {
            return;
        }

        VK_CHECK(vkResetFences(g_Device, 1, &g_InFlightFences[g_CurrentFrame]));

        VkCommandBuffer cmd = g_CommandBuffers[g_CurrentFrame];
        VK_CHECK(vkResetCommandBuffer(cmd, 0));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = g_RenderPass;
        renderPassInfo.framebuffer = g_Framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = g_SwapchainExtent;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(draw_data, cmd);
        vkCmdEndRenderPass(cmd);
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;

        VkSemaphore waitSemaphores[] = {g_ImageAvailableSemaphores[g_CurrentFrame]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        VkSemaphore signalSemaphores[] = {g_RenderFinishedSemaphores[g_CurrentFrame]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;

        VK_CHECK(vkQueueSubmit(g_Queue, 1, &submitInfo, g_InFlightFences[g_CurrentFrame]));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &g_Swapchain;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(g_Queue, &presentInfo);

        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            g_SwapChainRebuild = true;
        }

        g_CurrentFrame = (g_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    inline void shutdown()
    {
        if (g_Device != VK_NULL_HANDLE)
            vkDeviceWaitIdle(g_Device);

        ImGui_ImplVulkan_Shutdown();
        ImGui_ImplAndroid_Shutdown();
        ImGui::DestroyContext();

        if (g_Device != VK_NULL_HANDLE)
        {
            CleanupSwapchain();
            vkDestroyDescriptorPool(g_Device, g_DescriptorPool, nullptr);
            vkDestroyRenderPass(g_Device, g_RenderPass, nullptr);
            for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
            {
                vkDestroySemaphore(g_Device, g_RenderFinishedSemaphores[i], nullptr);
                vkDestroySemaphore(g_Device, g_ImageAvailableSemaphores[i], nullptr);
                vkDestroyFence(g_Device, g_InFlightFences[i], nullptr);
            }
            vkDestroyCommandPool(g_Device, g_CommandPool, nullptr);
            vkDestroyDevice(g_Device, nullptr);
        }

        if (g_Instance != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(g_Instance, g_Surface, nullptr);
            vkDestroyInstance(g_Instance, nullptr);
        }

        if (native_window)
        {
            ANativeWindow_release(native_window);
            native_window = nullptr;
        }
    }
}
