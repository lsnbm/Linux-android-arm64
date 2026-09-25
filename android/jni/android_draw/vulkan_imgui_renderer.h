#pragma once

#include <algorithm>
#include <chrono>
#include <climits>
#include <cmath>
#include <cstdlib>
#include <utility>
#include <vector>

#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

// Android/ImGui 依赖
#include "imgui/backends/imgui_impl_android.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h"
#include "android_imgui_input/imgui_touch_input.h"
#include "android_surface/surface_control_manager.h"
#include "font/oppo_font.h"

// Vulkan 后端实现
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
    static bool imgui_context_created = false;
    static bool android_backend_initialized = false;
    static bool vulkan_backend_initialized = false;
    static ANativeWindow *native_window = nullptr;
    static android::SurfaceControlManager::DisplayInfo displayInfo{};
    static std::chrono::steady_clock::time_point s_lastDisplayInfoQuery{};
    static bool s_allowCapture = false;

    struct RecordSurfaceState
    {
        bool mirrorReady = false;
        uint32_t layerStack = 0;
        android::SurfaceControlManager::RecordDisplayInfo display{};
        android::SurfaceControlManager::DisplayInfo source{};
    };

    static RecordSurfaceState s_recordSurface;
    static android::SurfaceControlManager::RecordDisplayInfo s_cachedRecordDisplay{};
    static android::SurfaceControlManager::RecordDisplayInfo s_pendingRecordDisplay{};
    static bool s_hasCachedRecordDisplay = false;
    static bool s_hasPendingRecordDisplay = false;
    static int s_pendingRecordDisplayHits = 0;
    static int s_recordDisplayMisses = 0;
    static std::chrono::steady_clock::time_point s_lastRecordDisplayScan{};

    inline bool SameRecordDisplay(const android::SurfaceControlManager::RecordDisplayInfo &lhs, const android::SurfaceControlManager::RecordDisplayInfo &rhs)
    {
        return lhs.layerStack == rhs.layerStack && lhs.orientation == rhs.orientation && lhs.layerStackRect == rhs.layerStackRect && lhs.displayRect == rhs.displayRect;
    }

    inline void shutdown();

#define VK_CHECK(x)                                                                   \
    do                                                                                \
    {                                                                                 \
        VkResult err = x;                                                             \
        if (err)                                                                      \
        {                                                                             \
            LS_LOGE_TAG("RenderVK", "VkResult=%d at %s:%d", err, __FILE__, __LINE__); \
            abort();                                                                  \
        }                                                                             \
    } while (0)

    inline void CleanupSwapchain(bool destroySwapchain = true)
    {
        if (g_Device == VK_NULL_HANDLE)
        {
            g_Framebuffers.clear();
            g_SwapchainImageViews.clear();
            return;
        }

        for (auto fb : g_Framebuffers) vkDestroyFramebuffer(g_Device, fb, nullptr);
        g_Framebuffers.clear();

        for (auto iv : g_SwapchainImageViews) vkDestroyImageView(g_Device, iv, nullptr);
        g_SwapchainImageViews.clear();

        // 仅在允许销毁时释放，防止底层断开连接
        if (destroySwapchain && g_Swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(g_Device, g_Swapchain, nullptr);
            g_Swapchain = VK_NULL_HANDLE;
        }
    }

    inline void DestroyRenderFinishedSemaphores()
    {
        for (auto semaphore : g_RenderFinishedSemaphores)
            if (semaphore != VK_NULL_HANDLE) vkDestroySemaphore(g_Device, semaphore, nullptr);
        g_RenderFinishedSemaphores.clear();
    }

    inline bool CreateRenderFinishedSemaphores()
    {
        g_RenderFinishedSemaphores.resize(g_SwapchainImages.size(), VK_NULL_HANDLE);
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        for (auto &semaphore : g_RenderFinishedSemaphores)
        {
            if (vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &semaphore) == VK_SUCCESS) continue;
            DestroyRenderFinishedSemaphores();
            return false;
        }
        return !g_RenderFinishedSemaphores.empty();
    }

    inline bool CreateSwapchain(VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE)
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, g_Surface, &capabilities) != VK_SUCCESS) return false;

        uint32_t formatCount = 0;
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) return false;
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &formatCount, formats.data()) != VK_SUCCESS || formatCount == 0) return false;

        VkSurfaceFormatKHR surfaceFormat = formats[0];
        if (surfaceFormat.format == VK_FORMAT_UNDEFINED) surfaceFormat = {VK_FORMAT_R8G8B8A8_UNORM, VK_COLOR_SPACE_SRGB_NONLINEAR_KHR};
        for (const auto &availableFormat : formats)
        {
            if (availableFormat.format == VK_FORMAT_R8G8B8A8_UNORM || availableFormat.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                surfaceFormat = availableFormat;
                break;
            }
        }
        g_SwapchainFormat = surfaceFormat.format;

        if (capabilities.currentExtent.width != 0xFFFFFFFF) g_SwapchainExtent = capabilities.currentExtent;
        else g_SwapchainExtent = {(uint32_t)displayInfo.width, (uint32_t)displayInfo.height};
        if (g_SwapchainExtent.width == 0 || g_SwapchainExtent.height == 0) return false;

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) imageCount = capabilities.maxImageCount;

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
        if (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) createInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
        else createInfo.preTransform = capabilities.currentTransform;

        if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR) createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR) createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
        else if (capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR) createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;
        else return false;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR; // Vsync
        createInfo.clipped = VK_TRUE;

        // 传入旧交换链给 Android 底层，实现无缝过渡
        createInfo.oldSwapchain = oldSwapchain;

        VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
        if (vkCreateSwapchainKHR(g_Device, &createInfo, nullptr, &newSwapchain) != VK_SUCCESS) return false;

        uint32_t newImageCount = 0;
        if (vkGetSwapchainImagesKHR(g_Device, newSwapchain, &newImageCount, nullptr) != VK_SUCCESS || newImageCount < 2)
        {
            vkDestroySwapchainKHR(g_Device, newSwapchain, nullptr);
            return false;
        }

        std::vector<VkImage> newImages(newImageCount);
        if (vkGetSwapchainImagesKHR(g_Device, newSwapchain, &newImageCount, newImages.data()) != VK_SUCCESS)
        {
            vkDestroySwapchainKHR(g_Device, newSwapchain, nullptr);
            return false;
        }

        std::vector<VkImageView> newImageViews(newImageCount, VK_NULL_HANDLE);
        for (size_t i = 0; i < newImages.size(); i++)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = newImages[i];
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
            if (vkCreateImageView(g_Device, &viewInfo, nullptr, &newImageViews[i]) != VK_SUCCESS)
            {
                for (auto imageView : newImageViews)
                    if (imageView != VK_NULL_HANDLE) vkDestroyImageView(g_Device, imageView, nullptr);
                vkDestroySwapchainKHR(g_Device, newSwapchain, nullptr);
                return false;
            }
        }

        if (oldSwapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(g_Device, oldSwapchain, nullptr);
        g_Swapchain = newSwapchain;
        g_SwapchainImages = std::move(newImages);
        g_SwapchainImageViews = std::move(newImageViews);
        return true;
    }
    inline bool RecreateSwapchain()
    {
        vkDeviceWaitIdle(g_Device);
        DestroyRenderFinishedSemaphores();

        // 提取当前的交换链
        VkSwapchainKHR oldSwapchain = g_Swapchain;

        // 销毁 Framebuffer / ImageView，但保留 g_Swapchain (传 false)
        CleanupSwapchain(false);

        // 携带旧链进行重建
        if (!CreateSwapchain(oldSwapchain))
        {
            if (g_Swapchain != VK_NULL_HANDLE)
            {
                vkDestroySwapchainKHR(g_Device, g_Swapchain, nullptr);
                g_Swapchain = VK_NULL_HANDLE;
            }
            g_SwapchainImages.clear();
            return false;
        }

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
            if (vkCreateFramebuffer(g_Device, &framebufferInfo, nullptr, &g_Framebuffers[i]) != VK_SUCCESS)
            {
                CleanupSwapchain();
                return false;
            }
        }
        if (!CreateRenderFinishedSemaphores())
        {
            CleanupSwapchain();
            return false;
        }
        return true;
    }

    inline void CleanupRecordSurface()
    {
        if (s_recordSurface.mirrorReady)
        {
            android::SurfaceControlManager::DestroyMirrorOnLayerStack(native_window);
        }
        s_recordSurface.mirrorReady = false;
        s_recordSurface = {};
    }

    inline bool EnsureRecordSurface()
    {
        if (!s_allowCapture)
        {
            if (s_recordSurface.mirrorReady) CleanupRecordSurface();
            s_hasCachedRecordDisplay = false;
            s_hasPendingRecordDisplay = false;
            s_pendingRecordDisplayHits = 0;
            s_recordDisplayMisses = 0;
            return false;
        }

        // 检测 Oplus/AOSP 录屏的目标 layerStack，并把主 Lark Surface 镜像到该显示。
        auto now = std::chrono::steady_clock::now();
        bool shouldScan = s_lastRecordDisplayScan == std::chrono::steady_clock::time_point{} || (now - s_lastRecordDisplayScan) >= std::chrono::seconds(2);
        if (shouldScan)
        {
            s_lastRecordDisplayScan = now;
            android::SurfaceControlManager::RecordDisplayInfo scannedTarget{};
            bool foundRecordDisplay = android::SurfaceControlManager::FindRecordDisplay(&scannedTarget, displayInfo.width, displayInfo.height);
            if (!foundRecordDisplay)
            {
                s_hasPendingRecordDisplay = false;
                s_pendingRecordDisplayHits = 0;
                if (++s_recordDisplayMisses >= 2) s_hasCachedRecordDisplay = false;
            }
            else
            {
                s_recordDisplayMisses = 0;
                if (!s_hasCachedRecordDisplay || !s_recordSurface.mirrorReady || SameRecordDisplay(scannedTarget, s_cachedRecordDisplay))
                {
                    s_cachedRecordDisplay = scannedTarget;
                    s_hasCachedRecordDisplay = true;
                    s_hasPendingRecordDisplay = false;
                    s_pendingRecordDisplayHits = 0;
                }
                else if (scannedTarget.layerStack == s_cachedRecordDisplay.layerStack)
                {
                    s_cachedRecordDisplay = scannedTarget;
                    s_hasPendingRecordDisplay = false;
                    s_pendingRecordDisplayHits = 0;
                }
                else if (!s_hasPendingRecordDisplay || !SameRecordDisplay(scannedTarget, s_pendingRecordDisplay))
                {
                    s_pendingRecordDisplay = scannedTarget;
                    s_hasPendingRecordDisplay = true;
                    s_pendingRecordDisplayHits = 1;
                }
                else if (++s_pendingRecordDisplayHits >= 2)
                {
                    s_cachedRecordDisplay = scannedTarget;
                    s_hasCachedRecordDisplay = true;
                    s_hasPendingRecordDisplay = false;
                    s_pendingRecordDisplayHits = 0;
                }
            }
        }

        if (!s_hasCachedRecordDisplay)
        {
            if (s_recordSurface.mirrorReady) CleanupRecordSurface();
            return false;
        }

        const auto &target = s_cachedRecordDisplay;
        const int32_t sourceWidth = displayInfo.width;
        const int32_t sourceHeight = displayInfo.height;
        if (sourceWidth <= 0 || sourceHeight <= 0) return false;

        if (s_recordSurface.mirrorReady && s_recordSurface.layerStack == target.layerStack &&
            SameRecordDisplay(s_recordSurface.display, target) && s_recordSurface.source.orientation == displayInfo.orientation &&
            s_recordSurface.source.width == sourceWidth && s_recordSurface.source.height == sourceHeight) return true;

        if (!s_recordSurface.mirrorReady || s_recordSurface.layerStack != target.layerStack)
        {
            CleanupRecordSurface();
            if (!android::SurfaceControlManager::CreateMirrorOnLayerStack(native_window, target.layerStack)) return false;
        }
        if (!android::SurfaceControlManager::ConfigureMirrorProjection(native_window, displayInfo, target))
        {
            android::SurfaceControlManager::DestroyMirrorOnLayerStack(native_window);
            s_recordSurface = {};
            return false;
        }

        s_recordSurface.mirrorReady = true;
        s_recordSurface.layerStack = target.layerStack;
        s_recordSurface.display = target;
        s_recordSurface.source = displayInfo;
        LS_LOGI_TAG_FMT("RenderVK", "录屏镜像层就绪 layerStack={} orientation={} stackRect=({},{}-{},{}), displayRect=({},{}-{},{}), source={}x{}", target.layerStack, target.orientation, target.layerStackRect.left, target.layerStackRect.top, target.layerStackRect.right, target.layerStackRect.bottom, target.displayRect.left, target.displayRect.top, target.displayRect.right, target.displayRect.bottom, sourceWidth, sourceHeight);
        return true;
    }

    inline bool init(bool allowCapture)
    {
        if (imgui_context_created) return true;

        s_allowCapture = allowCapture;
        displayInfo = android::SurfaceControlManager::GetDisplayInfo();
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

        native_window = android::SurfaceControlManager::Create("Lark", max_side, max_side, !allowCapture);
        if (native_window == nullptr)
        {
            LS_LOGE_TAG("RenderVK", "创建 ANativeWindow 失败");
            return false;
        }
        LS_LOGI_TAG_FMT("RenderVK", "录屏策略 allowCapture={}", allowCapture);
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
        {
            shutdown();
            return false;
        }
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
        if (g_QueueFamily == (uint32_t)-1)
        {
            LS_LOGE_TAG("RenderVK", "没有找到同时支持图形和呈现的队列族");
            shutdown();
            return false;
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

        if (!CreateSwapchain())
        {
            LS_LOGE_TAG("RenderVK", "创建主交换链失败");
            shutdown();
            return false;
        }

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

        VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

        VkRenderPassCreateInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        renderPassInfo.attachmentCount = 1;
        renderPassInfo.pAttachments = &colorAttachment;
        renderPassInfo.subpassCount = 1;
        renderPassInfo.pSubpasses = &subpass;
        renderPassInfo.dependencyCount = 1;
        renderPassInfo.pDependencies = &dependency;
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
        g_InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            VK_CHECK(vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &g_ImageAvailableSemaphores[i]));
            VK_CHECK(vkCreateFence(g_Device, &fenceInfo, nullptr, &g_InFlightFences[i]));
        }
        if (!CreateRenderFinishedSemaphores())
        {
            LS_LOGE_TAG("RenderVK", "创建主呈现信号量失败");
            shutdown();
            return false;
        }

        VkDescriptorPoolSize pool_sizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000}, {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descPoolInfo.maxSets = 1000 * IM_ARRAYSIZE(pool_sizes);
        descPoolInfo.poolSizeCount = (uint32_t)IM_ARRAYSIZE(pool_sizes);
        descPoolInfo.pPoolSizes = pool_sizes;
        VK_CHECK(vkCreateDescriptorPool(g_Device, &descPoolInfo, nullptr, &g_DescriptorPool));

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        imgui_context_created = true;
        ImGuiIO &io = ImGui::GetIO();
        io.IniFilename = NULL;

        android_backend_initialized = ImGui_ImplAndroid_Init(native_window);

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
        {
            if (err) LS_LOGE_TAG("RenderVK", "ImGui Vulkan VkResult=%d", err);
        };

        vulkan_backend_initialized = ImGui_ImplVulkan_Init(&init_info);
        if (!android_backend_initialized || !vulkan_backend_initialized)
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
            LS_LOGE_TAG("RenderVK", "初始化触摸输入失败");
            shutdown();
            return false;
        }

        return true;
    }
    inline void drawBegin()
    {
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
        if (displayInfo.width <= 0 || displayInfo.height <= 0) return;

        bool rotated = (orientation.load(std::memory_order_relaxed) != static_cast<uint32_t>(displayInfo.orientation));

        if (g_SwapChainRebuild || rotated)
        {
            if (rotated)
            {
                UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);
                // 不等待常规轮询周期，当前帧结束时立即刷新录屏投影。
                s_lastRecordDisplayScan = {};
            }
            g_SwapChainRebuild = !RecreateSwapchain();
        }

        Touch_UpdateImGui();

        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();

        // 旋转或启动时，强制将菜单拉回左上角安全位置
        static bool s_reposition = true;
        if (rotated) s_reposition = true;

        if (s_reposition)
        {
            ImGui::SetNextWindowPos(ImVec2(100.0f, 100.0f), ImGuiCond_Always);
            s_reposition = false; // 仅生效一帧，之后允许用户自由拖拽
        }
    }

    inline void drawEnd()
    {
        if (!imgui_context_created || !vulkan_backend_initialized) return;

        ImGui::Render();
        ImDrawData *draw_data = ImGui::GetDrawData();

        if (g_Device == VK_NULL_HANDLE || g_Swapchain == VK_NULL_HANDLE || g_Framebuffers.empty()) return;

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
        if (imageIndex >= g_Framebuffers.size() || imageIndex >= g_RenderFinishedSemaphores.size())
        {
            g_SwapChainRebuild = true;
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

        VkSemaphore signalSemaphores[] = {g_RenderFinishedSemaphores[imageIndex]};
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

        if (s_allowCapture)
        {
            EnsureRecordSurface();
        }

        g_CurrentFrame = (g_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    inline void shutdown()
    {
        Touch_Shutdown();

        if (g_Device != VK_NULL_HANDLE) vkDeviceWaitIdle(g_Device);

        CleanupRecordSurface();

        if (vulkan_backend_initialized) ImGui_ImplVulkan_Shutdown();
        if (android_backend_initialized) ImGui_ImplAndroid_Shutdown();
        if (imgui_context_created) ImGui::DestroyContext();
        vulkan_backend_initialized = false;
        android_backend_initialized = false;
        imgui_context_created = false;

        if (g_Device != VK_NULL_HANDLE)
        {
            DestroyRenderFinishedSemaphores();
            CleanupSwapchain();
            vkDestroyDescriptorPool(g_Device, g_DescriptorPool, nullptr);
            vkDestroyRenderPass(g_Device, g_RenderPass, nullptr);
            for (auto semaphore : g_ImageAvailableSemaphores) vkDestroySemaphore(g_Device, semaphore, nullptr);
            for (auto fence : g_InFlightFences) vkDestroyFence(g_Device, fence, nullptr);
            vkDestroyCommandPool(g_Device, g_CommandPool, nullptr);
            vkDestroyDevice(g_Device, nullptr);
            g_Device = VK_NULL_HANDLE;
        }

        if (g_Instance != VK_NULL_HANDLE)
        {
            if (g_Surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(g_Instance, g_Surface, nullptr);
            vkDestroyInstance(g_Instance, nullptr);
            g_Surface = VK_NULL_HANDLE;
            g_Instance = VK_NULL_HANDLE;
        }

        if (native_window)
        {
            android::SurfaceControlManager::Destroy(native_window);
            ANativeWindow_release(native_window);
            native_window = nullptr;
        }
        g_PhysicalDevice = VK_NULL_HANDLE;
        g_QueueFamily = (uint32_t)-1;
        g_Queue = VK_NULL_HANDLE;
        g_Swapchain = VK_NULL_HANDLE;
        g_RenderPass = VK_NULL_HANDLE;
        g_CommandPool = VK_NULL_HANDLE;
        g_DescriptorPool = VK_NULL_HANDLE;
        g_SwapchainImages.clear();
        g_SwapchainImageViews.clear();
        g_Framebuffers.clear();
        g_CommandBuffers.clear();
        g_ImageAvailableSemaphores.clear();
        g_RenderFinishedSemaphores.clear();
        g_InFlightFences.clear();
        g_CurrentFrame = 0;
        g_SwapChainRebuild = false;
    }
} // namespace RenderVK
