#pragma once

#include <algorithm>
#include <chrono>
#include <climits>
#include <cstdlib>
#include <print>
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

    static ANativeWindow *native_window = nullptr;
    static android::SurfaceControlManager::DisplayInfo displayInfo{};
    static bool s_preventCapture = true;

    struct RecordTransform
    {
        float dsdx = 1.0f;
        float dtdx = 0.0f;
        float dtdy = 0.0f;
        float dsdy = 1.0f;
        float positionX = 0.0f;
        float positionY = 0.0f;
        bool valid = false;
    };

    struct RecordSurfaceState
    {
        ANativeWindow *window = nullptr;
        uint32_t layerStack = 0;
        int32_t width = 0;
        int32_t height = 0;
        int32_t sourceWidth = 0;
        int32_t sourceHeight = 0;
        android::SurfaceControlManager::RecordDisplayInfo display{};
        RecordTransform transform{};
        VkSurfaceKHR surface = VK_NULL_HANDLE;
        VkSwapchainKHR swapchain = VK_NULL_HANDLE;
        VkExtent2D extent = {};
        std::vector<VkImage> images;
        std::vector<VkImageView> imageViews;
        std::vector<VkFramebuffer> framebuffers;
        std::vector<VkCommandBuffer> commandBuffers;
        std::vector<VkSemaphore> imageAvailableSemaphores;
        std::vector<VkSemaphore> renderFinishedSemaphores;
        std::vector<VkFence> inFlightFences;
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

    inline RecordTransform BuildRecordTransform(const android::SurfaceControlManager::RecordDisplayInfo &target, const VkExtent2D bufferExtent, int32_t sourceWidth, int32_t sourceHeight)
    {
        RecordTransform transform{};
        if (bufferExtent.width == 0 || bufferExtent.height == 0 || sourceWidth <= 0 || sourceHeight <= 0) return transform;

        const float left = static_cast<float>(std::min(target.layerStackRect.left, target.layerStackRect.right));
        const float top = static_cast<float>(std::min(target.layerStackRect.top, target.layerStackRect.bottom));
        const float targetWidth = static_cast<float>(target.layerStackRect.Width());
        const float targetHeight = static_cast<float>(target.layerStackRect.Height());
        if (targetWidth <= 0.0f || targetHeight <= 0.0f) return transform;

        const float bufferWidth = static_cast<float>(bufferExtent.width);
        const float bufferHeight = static_cast<float>(bufferExtent.height);
        const bool sourceLandscape = sourceWidth >= sourceHeight;
        const bool targetLandscape = targetWidth >= targetHeight;
        const int32_t orientation = ((target.orientation % 4) + 4) % 4;
        // AOSP 通常把 layerStackRect 输出为旋转后的逻辑坐标，此时无需再次旋转。
        // 仅当录屏 ROM 输出的 layerStackRect 轴向与源坐标相反时，应用方向的逆变换。
        const bool needsAxisSwap = (orientation == 1 || orientation == 3) && sourceLandscape != targetLandscape;

        switch (needsAxisSwap ? orientation : 0)
        {
        case 0:
            transform.dsdx = targetWidth / bufferWidth;
            transform.dsdy = targetHeight / bufferHeight;
            transform.positionX = left;
            transform.positionY = top;
            break;
        case 1:
            // Android ROTATION_90 的逆变换：横屏缓冲 -> 旋转前 layerStack。
            transform.dsdx = 0.0f;
            transform.dtdx = targetWidth / bufferHeight;
            transform.dtdy = -targetHeight / bufferWidth;
            transform.dsdy = 0.0f;
            transform.positionX = left;
            transform.positionY = top + targetHeight;
            break;
        case 3:
            // Android ROTATION_270 的逆变换：横屏缓冲 -> 旋转前 layerStack。
            transform.dsdx = 0.0f;
            transform.dtdx = -targetWidth / bufferHeight;
            transform.dtdy = targetHeight / bufferWidth;
            transform.dsdy = 0.0f;
            transform.positionX = left + targetWidth;
            transform.positionY = top;
            break;
        default:
            break;
        }
        transform.valid = true;
        return transform;
    }

    inline void shutdown();

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

        if (capabilities.currentExtent.width != 0xFFFFFFFF) g_SwapchainExtent = capabilities.currentExtent;
        else g_SwapchainExtent = {(uint32_t)displayInfo.width, (uint32_t)displayInfo.height};

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

    inline void CleanupRecordSurface()
    {
        if (g_Device != VK_NULL_HANDLE) vkDeviceWaitIdle(g_Device);

        for (auto fence : s_recordSurface.inFlightFences) vkDestroyFence(g_Device, fence, nullptr);
        s_recordSurface.inFlightFences.clear();

        for (auto semaphore : s_recordSurface.renderFinishedSemaphores) vkDestroySemaphore(g_Device, semaphore, nullptr);
        s_recordSurface.renderFinishedSemaphores.clear();

        for (auto semaphore : s_recordSurface.imageAvailableSemaphores) vkDestroySemaphore(g_Device, semaphore, nullptr);
        s_recordSurface.imageAvailableSemaphores.clear();

        for (auto framebuffer : s_recordSurface.framebuffers) vkDestroyFramebuffer(g_Device, framebuffer, nullptr);
        s_recordSurface.framebuffers.clear();

        if (!s_recordSurface.commandBuffers.empty() && g_CommandPool != VK_NULL_HANDLE)
        {
            vkFreeCommandBuffers(g_Device, g_CommandPool, static_cast<uint32_t>(s_recordSurface.commandBuffers.size()), s_recordSurface.commandBuffers.data());
            s_recordSurface.commandBuffers.clear();
        }

        for (auto imageView : s_recordSurface.imageViews) vkDestroyImageView(g_Device, imageView, nullptr);
        s_recordSurface.imageViews.clear();

        if (s_recordSurface.swapchain != VK_NULL_HANDLE)
        {
            vkDestroySwapchainKHR(g_Device, s_recordSurface.swapchain, nullptr);
            s_recordSurface.swapchain = VK_NULL_HANDLE;
        }

        if (s_recordSurface.surface != VK_NULL_HANDLE)
        {
            vkDestroySurfaceKHR(g_Instance, s_recordSurface.surface, nullptr);
            s_recordSurface.surface = VK_NULL_HANDLE;
        }

        if (s_recordSurface.window)
        {
            android::SurfaceControlManager::Destroy(s_recordSurface.window);
            ANativeWindow_release(s_recordSurface.window);
            s_recordSurface.window = nullptr;
        }

        s_recordSurface = {};
    }

    inline bool CreateRecordSwapchain()
    {
        VkSurfaceCapabilitiesKHR capabilities{};
        if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, s_recordSurface.surface, &capabilities) != VK_SUCCESS) return false;

        uint32_t formatCount = 0;
        if (vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, s_recordSurface.surface, &formatCount, nullptr) != VK_SUCCESS || formatCount == 0) return false;

        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, s_recordSurface.surface, &formatCount, formats.data());

        VkSurfaceFormatKHR surfaceFormat{};
        bool foundFormat = false;
        for (const auto &availableFormat : formats)
        {
            if (availableFormat.format == g_SwapchainFormat)
            {
                surfaceFormat = availableFormat;
                foundFormat = true;
                break;
            }
        }
        if (!foundFormat) return false;

        if (capabilities.currentExtent.width != 0xFFFFFFFF) s_recordSurface.extent = capabilities.currentExtent;
        else s_recordSurface.extent = {static_cast<uint32_t>(s_recordSurface.width), static_cast<uint32_t>(s_recordSurface.height)};

        uint32_t imageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) imageCount = capabilities.maxImageCount;

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = s_recordSurface.surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = s_recordSurface.extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = (capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR) ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;

        if (vkCreateSwapchainKHR(g_Device, &createInfo, nullptr, &s_recordSurface.swapchain) != VK_SUCCESS) return false;

        vkGetSwapchainImagesKHR(g_Device, s_recordSurface.swapchain, &imageCount, nullptr);
        s_recordSurface.images.resize(imageCount);
        vkGetSwapchainImagesKHR(g_Device, s_recordSurface.swapchain, &imageCount, s_recordSurface.images.data());

        s_recordSurface.imageViews.resize(s_recordSurface.images.size());
        for (size_t i = 0; i < s_recordSurface.images.size(); i++)
        {
            VkImageViewCreateInfo viewInfo{};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = s_recordSurface.images[i];
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
            if (vkCreateImageView(g_Device, &viewInfo, nullptr, &s_recordSurface.imageViews[i]) != VK_SUCCESS) return false;
        }

        s_recordSurface.framebuffers.resize(s_recordSurface.imageViews.size());
        for (size_t i = 0; i < s_recordSurface.imageViews.size(); i++)
        {
            VkImageView attachments[] = {s_recordSurface.imageViews[i]};
            VkFramebufferCreateInfo framebufferInfo{};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = g_RenderPass;
            framebufferInfo.attachmentCount = 1;
            framebufferInfo.pAttachments = attachments;
            framebufferInfo.width = s_recordSurface.extent.width;
            framebufferInfo.height = s_recordSurface.extent.height;
            framebufferInfo.layers = 1;
            if (vkCreateFramebuffer(g_Device, &framebufferInfo, nullptr, &s_recordSurface.framebuffers[i]) != VK_SUCCESS) return false;
        }

        s_recordSurface.commandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = g_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = static_cast<uint32_t>(s_recordSurface.commandBuffers.size());
        if (vkAllocateCommandBuffers(g_Device, &allocInfo, s_recordSurface.commandBuffers.data()) != VK_SUCCESS) return false;

        s_recordSurface.imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        s_recordSurface.renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
        s_recordSurface.inFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

        for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++)
        {
            if (vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &s_recordSurface.imageAvailableSemaphores[i]) != VK_SUCCESS || vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &s_recordSurface.renderFinishedSemaphores[i]) != VK_SUCCESS || vkCreateFence(g_Device, &fenceInfo, nullptr, &s_recordSurface.inFlightFences[i]) != VK_SUCCESS) return false;
        }

        return true;
    }

    inline bool EnsureRecordSurface()
    {
        if (s_preventCapture)
        {
            if (s_recordSurface.window) CleanupRecordSurface();
            s_hasCachedRecordDisplay = false;
            s_hasPendingRecordDisplay = false;
            s_pendingRecordDisplayHits = 0;
            s_recordDisplayMisses = 0;
            return false;
        }

        // 旧 ProcessMirrorDisplay 依赖私有 mirrorSurface，Android 15/16 和部分厂商机型容易出现
        // 悬浮窗消失/崩溃。这里改为检测录屏 layerStack，并创建一个非可信副本 Surface。
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
                if (!s_hasCachedRecordDisplay || !s_recordSurface.window || SameRecordDisplay(scannedTarget, s_cachedRecordDisplay))
                {
                    s_cachedRecordDisplay = scannedTarget;
                    s_hasCachedRecordDisplay = true;
                    s_hasPendingRecordDisplay = false;
                    s_pendingRecordDisplayHits = 0;
                }
                else if (scannedTarget.layerStack == s_cachedRecordDisplay.layerStack)
                {
                    // 同一虚拟显示的旋转/分辨率变化立即生效，下一步会重建副 Surface。
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
            if (s_recordSurface.window) CleanupRecordSurface();
            return false;
        }

        const auto &target = s_cachedRecordDisplay;
        const int32_t sourceWidth = displayInfo.width;
        const int32_t sourceHeight = displayInfo.height;
        if (sourceWidth <= 0 || sourceHeight <= 0) return false;

        if (s_recordSurface.window && SameRecordDisplay(s_recordSurface.display, target) && s_recordSurface.sourceWidth == sourceWidth && s_recordSurface.sourceHeight == sourceHeight) return true;

        CleanupRecordSurface();

        s_recordSurface.layerStack = target.layerStack;
        s_recordSurface.width = sourceWidth;
        s_recordSurface.height = sourceHeight;
        s_recordSurface.sourceWidth = sourceWidth;
        s_recordSurface.sourceHeight = sourceHeight;
        s_recordSurface.display = target;
        s_recordSurface.window = android::SurfaceControlManager::CreateOnLayerStack("LarkRecord", sourceWidth, sourceHeight, target.layerStack);
        if (!s_recordSurface.window)
        {
            CleanupRecordSurface();
            return false;
        }
        ANativeWindow_setBuffersGeometry(s_recordSurface.window, sourceWidth, sourceHeight, WINDOW_FORMAT_RGBA_8888);
        ANativeWindow_acquire(s_recordSurface.window);

        VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.window = s_recordSurface.window;
        if (vkCreateAndroidSurfaceKHR(g_Instance, &surfaceInfo, nullptr, &s_recordSurface.surface) != VK_SUCCESS)
        {
            CleanupRecordSurface();
            return false;
        }

        VkBool32 presentSupport = false;
        if (vkGetPhysicalDeviceSurfaceSupportKHR(g_PhysicalDevice, g_QueueFamily, s_recordSurface.surface, &presentSupport) != VK_SUCCESS || !presentSupport)
        {
            CleanupRecordSurface();
            return false;
        }

        if (!CreateRecordSwapchain())
        {
            CleanupRecordSurface();
            return false;
        }

        s_recordSurface.transform = BuildRecordTransform(target, s_recordSurface.extent, s_recordSurface.sourceWidth, s_recordSurface.sourceHeight);
        if (!s_recordSurface.transform.valid || !android::SurfaceControlManager::ConfigureOnLayerStack(s_recordSurface.window, target.layerStack, s_recordSurface.transform.dsdx, s_recordSurface.transform.dtdx, s_recordSurface.transform.dtdy, s_recordSurface.transform.dsdy, s_recordSurface.transform.positionX, s_recordSurface.transform.positionY))
        {
            CleanupRecordSurface();
            return false;
        }

        std::println(stderr, "[RenderVK] Record overlay ready layerStack={} source={}x{} stackRect=({},{}-{},{}), displayRect=({},{}-{},{}), orientation={} extent={}x{} matrix=[{},{};{},{}] pos=({}, {})", target.layerStack, sourceWidth, sourceHeight, target.layerStackRect.left, target.layerStackRect.top, target.layerStackRect.right, target.layerStackRect.bottom, target.displayRect.left, target.displayRect.top, target.displayRect.right, target.displayRect.bottom, target.orientation, s_recordSurface.extent.width, s_recordSurface.extent.height, s_recordSurface.transform.dsdx, s_recordSurface.transform.dtdx, s_recordSurface.transform.dtdy, s_recordSurface.transform.dsdy, s_recordSurface.transform.positionX, s_recordSurface.transform.positionY);
        return true;
    }

    inline void RenderRecordSurface(ImDrawData *drawData)
    {
        // 双 Surface 方案：主 Surface 保持触摸和显示，录屏副本只负责把同一帧画面送进录屏。
        if (!drawData || drawData->TotalVtxCount <= 0 || !EnsureRecordSurface() || s_recordSurface.swapchain == VK_NULL_HANDLE) return;

        uint32_t frameIndex = g_CurrentFrame;
        VK_CHECK(vkWaitForFences(g_Device, 1, &s_recordSurface.inFlightFences[frameIndex], VK_TRUE, UINT64_MAX));

        uint32_t imageIndex = 0;
        VkResult result = vkAcquireNextImageKHR(g_Device, s_recordSurface.swapchain, UINT64_MAX, s_recordSurface.imageAvailableSemaphores[frameIndex], VK_NULL_HANDLE, &imageIndex);
        if (result == VK_ERROR_OUT_OF_DATE_KHR)
        {
            CleanupRecordSurface();
            return;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) return;

        VK_CHECK(vkResetFences(g_Device, 1, &s_recordSurface.inFlightFences[frameIndex]));

        VkCommandBuffer cmd = s_recordSurface.commandBuffers[frameIndex];
        VK_CHECK(vkResetCommandBuffer(cmd, 0));

        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        VK_CHECK(vkBeginCommandBuffer(cmd, &beginInfo));

        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = g_RenderPass;
        renderPassInfo.framebuffer = s_recordSurface.framebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = s_recordSurface.extent;

        VkClearValue clearColor = {{{0.0f, 0.0f, 0.0f, 0.0f}}};
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;

        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        ImDrawData recordDrawData = *drawData;
        recordDrawData.DisplayPos = ImVec2(0.0f, 0.0f);
        recordDrawData.DisplaySize = ImVec2(static_cast<float>(s_recordSurface.sourceWidth), static_cast<float>(s_recordSurface.sourceHeight));
        recordDrawData.FramebufferScale = ImVec2(static_cast<float>(s_recordSurface.extent.width) / static_cast<float>(s_recordSurface.sourceWidth), static_cast<float>(s_recordSurface.extent.height) / static_cast<float>(s_recordSurface.sourceHeight));
        ImGui_ImplVulkan_RenderDrawData(&recordDrawData, cmd);
        vkCmdEndRenderPass(cmd);
        VK_CHECK(vkEndCommandBuffer(cmd));

        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        VkSemaphore waitSemaphores[] = {s_recordSurface.imageAvailableSemaphores[frameIndex]};
        VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = waitSemaphores;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;

        VkSemaphore signalSemaphores[] = {s_recordSurface.renderFinishedSemaphores[frameIndex]};
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = signalSemaphores;
        VK_CHECK(vkQueueSubmit(g_Queue, 1, &submitInfo, s_recordSurface.inFlightFences[frameIndex]));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = signalSemaphores;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &s_recordSurface.swapchain;
        presentInfo.pImageIndices = &imageIndex;

        result = vkQueuePresentKHR(g_Queue, &presentInfo);
        if (result == VK_ERROR_OUT_OF_DATE_KHR) CleanupRecordSurface();
    }

    inline bool init(bool preventCapture)
    {
        s_preventCapture = preventCapture;
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

        native_window = android::SurfaceControlManager::Create("Lark", max_side, max_side, preventCapture);
        if (native_window == nullptr)
        {
            std::println(stderr, "[RenderVK Error] Failed to create ANativeWindow!");
            return false;
        }
        std::println(stderr, "[RenderVK] Capture policy preventCapture={}", preventCapture);
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
        if (gpu_count == 0) return false;
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
        {
            if (err) std::println(stderr, "[ImGui Vulkan Error] {}", (int)err);
        };

        ImGui_ImplVulkan_Init(&init_info);

        ImFontConfig font_cfg;
        font_cfg.SizePixels = 31.0f;
        font_cfg.FontDataOwnedByAtlas = false;
        io.Fonts->AddFontFromMemoryTTF((void *)OPPOSans_H, OPPOSans_H_size, 31.0f, &font_cfg, io.Fonts->GetGlyphRangesChineseFull());
        ImGui::GetStyle().ScaleAllSizes(3.0f);

        if (!Touch_Init())
        {
            std::println(stderr, "[RenderVK Error] Failed to initialize touch!");
            shutdown();
            return false;
        }

        return true;
    }
    inline void drawBegin()
    {
        displayInfo = android::SurfaceControlManager::GetDisplayInfo();

        bool rotated = (orientation.load(std::memory_order_relaxed) != static_cast<uint32_t>(displayInfo.orientation));

        if (g_SwapChainRebuild || rotated)
        {
            if (rotated)
            {
                UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);
                // 不等待常规轮询周期，当前帧结束时立即刷新录屏投影。
                s_lastRecordDisplayScan = {};
            }
            RecreateSwapchain();
            g_SwapChainRebuild = false;
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

        RenderRecordSurface(draw_data);

        g_CurrentFrame = (g_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    }

    inline void shutdown()
    {
        Touch_Shutdown();

        if (g_Device != VK_NULL_HANDLE) vkDeviceWaitIdle(g_Device);

        CleanupRecordSurface();

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
            android::SurfaceControlManager::Destroy(native_window);
            ANativeWindow_release(native_window);
            native_window = nullptr;
        }
    }
} // namespace RenderVK
