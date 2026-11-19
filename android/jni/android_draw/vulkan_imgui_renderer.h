#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <optional>
#include <vector>

#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

#include "android_imgui_input/imgui_touch_input.h"
#include "android_surface/surface_control_manager.h"
#include "font/oppo_font.h"
#include "imgui/backends/imgui_impl_android.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#include "imgui/imgui.h"
#include "logger/logger.h"

namespace RenderVK
{
    static VkInstance g_Instance = VK_NULL_HANDLE;
    static VkPhysicalDevice g_PhysicalDevice = VK_NULL_HANDLE;
    static VkDevice g_Device = VK_NULL_HANDLE;
    static VkQueue g_Queue = VK_NULL_HANDLE;
    static VkSurfaceKHR g_Surface = VK_NULL_HANDLE;
    static VkSwapchainKHR g_Swapchain = VK_NULL_HANDLE;
    static VkFormat g_SwapchainFormat = VK_FORMAT_UNDEFINED;
    static VkExtent2D g_SwapchainExtent{};
    static VkRenderPass g_RenderPass = VK_NULL_HANDLE;
    static VkCommandPool g_CommandPool = VK_NULL_HANDLE;
    static VkDescriptorPool g_DescriptorPool = VK_NULL_HANDLE;

    struct SwapchainImage
    {
        VkImageView view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
    };
    static std::vector<SwapchainImage> g_SwapchainImages;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    static VkCommandBuffer g_CommandBuffers[MAX_FRAMES_IN_FLIGHT]{};
    static VkSemaphore g_ImageAvailableSemaphores[MAX_FRAMES_IN_FLIGHT]{};
    static VkFence g_InFlightFences[MAX_FRAMES_IN_FLIGHT]{};
    static uint32_t g_CurrentFrame = 0;
    static bool g_SwapChainRebuild = false;
    static bool g_FirstFrame = true;
    static bool imgui_context_created = false;
    static bool android_backend_initialized = false;
    static bool vulkan_backend_initialized = false;
    static ANativeWindow *native_window = nullptr;
    static android::SurfaceControlManager::DisplayInfo displayInfo{};
    static std::chrono::steady_clock::time_point s_lastDisplayInfoQuery{};

    inline void CleanupSwapchain()
    {
        for (const auto &image : g_SwapchainImages)
        {
            vkDestroyFramebuffer(g_Device, image.framebuffer, nullptr);
            vkDestroyImageView(g_Device, image.view, nullptr);
            vkDestroySemaphore(g_Device, image.renderFinished, nullptr);
        }
        g_SwapchainImages.clear();
        vkDestroySwapchainKHR(g_Device, g_Swapchain, nullptr);
        g_Swapchain = VK_NULL_HANDLE;
    }

    inline int CreateSwapchain()
    {
        int status = 0;
        VkSurfaceCapabilitiesKHR capabilities{};
        status = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(g_PhysicalDevice, g_Surface, &capabilities);
        if (status != VK_SUCCESS) return status;

        uint32_t formatCount = 0;
        status = vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &formatCount, nullptr);
        if (status != VK_SUCCESS) return status;
        if (formatCount == 0)
        {
            status = VK_ERROR_FORMAT_NOT_SUPPORTED;
            return status;
        }
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        status = vkGetPhysicalDeviceSurfaceFormatsKHR(g_PhysicalDevice, g_Surface, &formatCount, formats.data());
        if (status != VK_SUCCESS) return status;
        formats.resize(formatCount);

        // 重建时继续使用 ImGui 图形管线对应的 render pass 格式。
        VkSurfaceFormatKHR surfaceFormat{};
        for (const auto &format : formats)
        {
            if (format.format == VK_FORMAT_UNDEFINED)
            {
                surfaceFormat = {g_RenderPass ? g_SwapchainFormat : VK_FORMAT_R8G8B8A8_UNORM, format.colorSpace};
                break;
            }
            if (g_RenderPass && format.format != g_SwapchainFormat) continue;
            if (surfaceFormat.format == VK_FORMAT_UNDEFINED) surfaceFormat = format;
            if (g_RenderPass || format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM)
            {
                surfaceFormat = format;
                break;
            }
        }
        if (surfaceFormat.format == VK_FORMAT_UNDEFINED)
        {
            status = VK_ERROR_FORMAT_NOT_SUPPORTED;
            return status;
        }

        VkSwapchainCreateInfoKHR createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = g_Surface;
        createInfo.minImageCount = capabilities.minImageCount + 1;
        if (capabilities.maxImageCount) createInfo.minImageCount = std::min(createInfo.minImageCount, capabilities.maxImageCount);
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = capabilities.currentExtent;
        if (createInfo.imageExtent.width == UINT32_MAX)
        {
            createInfo.imageExtent.width = std::clamp(static_cast<uint32_t>(displayInfo.width), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            createInfo.imageExtent.height = std::clamp(static_cast<uint32_t>(displayInfo.height), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
        }
        if (!createInfo.imageExtent.width || !createInfo.imageExtent.height)
        {
            status = VK_ERROR_OUT_OF_DATE_KHR;
            return status;
        }
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        createInfo.preTransform = capabilities.supportedTransforms & VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR ? VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR : capabilities.currentTransform;
        for (auto alpha : {VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR, VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR, VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR})
        {
            if (capabilities.supportedCompositeAlpha & alpha)
            {
                createInfo.compositeAlpha = alpha;
                break;
            }
        }
        createInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = g_Swapchain;
        VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
        status = vkCreateSwapchainKHR(g_Device, &createInfo, nullptr, &newSwapchain);
        // 按 vkCreateSwapchainKHR 的 oldSwapchain 约定，即便创建失败，旧链也已退役。
        CleanupSwapchain();
        if (status != VK_SUCCESS) return status;
        g_Swapchain = newSwapchain;
        g_SwapchainExtent = createInfo.imageExtent;

        if (!g_RenderPass)
        {
            VkAttachmentDescription color{};
            color.format = surfaceFormat.format;
            color.samples = VK_SAMPLE_COUNT_1_BIT;
            color.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
            color.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
            color.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
            color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
            color.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
            VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
            VkSubpassDescription subpass{};
            subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
            subpass.colorAttachmentCount = 1;
            subpass.pColorAttachments = &colorRef;
            VkSubpassDependency dependency{};
            dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
            dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
            VkRenderPassCreateInfo renderPassInfo{};
            renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            renderPassInfo.attachmentCount = 1;
            renderPassInfo.pAttachments = &color;
            renderPassInfo.subpassCount = 1;
            renderPassInfo.pSubpasses = &subpass;
            renderPassInfo.dependencyCount = 1;
            renderPassInfo.pDependencies = &dependency;
            status = vkCreateRenderPass(g_Device, &renderPassInfo, nullptr, &g_RenderPass);
            if (status != VK_SUCCESS)
            {
                g_RenderPass = VK_NULL_HANDLE;
                CleanupSwapchain();
                return status;
            }
            g_SwapchainFormat = surfaceFormat.format;
        }

        uint32_t imageCount = 0;
        status = vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &imageCount, nullptr);
        if (status == VK_SUCCESS && imageCount < 2) status = VK_ERROR_INITIALIZATION_FAILED;
        if (status != VK_SUCCESS)
        {
            CleanupSwapchain();
            return status;
        }
        std::vector<VkImage> images(imageCount);
        status = vkGetSwapchainImagesKHR(g_Device, g_Swapchain, &imageCount, images.data());
        if (status != VK_SUCCESS)
        {
            CleanupSwapchain();
            return status;
        }

        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = surfaceFormat.format;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        VkFramebufferCreateInfo framebufferInfo{};
        framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferInfo.renderPass = g_RenderPass;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.width = g_SwapchainExtent.width;
        framebufferInfo.height = g_SwapchainExtent.height;
        framebufferInfo.layers = 1;
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        g_SwapchainImages.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i)
        {
            auto &image = g_SwapchainImages[i];
            viewInfo.image = images[i];
            status = vkCreateImageView(g_Device, &viewInfo, nullptr, &image.view);
            if (status != VK_SUCCESS)
            {
                image.view = VK_NULL_HANDLE;
                CleanupSwapchain();
                return status;
            }
            framebufferInfo.pAttachments = &image.view;
            status = vkCreateFramebuffer(g_Device, &framebufferInfo, nullptr, &image.framebuffer);
            if (status != VK_SUCCESS)
            {
                image.framebuffer = VK_NULL_HANDLE;
                CleanupSwapchain();
                return status;
            }
            status = vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &image.renderFinished);
            if (status != VK_SUCCESS)
            {
                image.renderFinished = VK_NULL_HANDLE;
                CleanupSwapchain();
                return status;
            }
        }
        return status;
    }

    struct RecordSurfaceState
    {
        android::SurfaceControlManager::RecordDisplayInfo display;
        android::SurfaceControlManager::DisplayInfo source;
    };
    static std::optional<RecordSurfaceState> s_recordSurface;
    static std::optional<android::SurfaceControlManager::RecordDisplayInfo> s_cachedRecordDisplay;
    static std::optional<android::SurfaceControlManager::RecordDisplayInfo> s_pendingRecordDisplay;
    static bool s_recordDisplayMissed = false;
    static std::chrono::steady_clock::time_point s_lastRecordDisplayScan{};
    static bool s_allowCapture = false;

    inline void CleanupRecordSurface()
    {
        if (!s_recordSurface) return;
        android::SurfaceControlManager::DestroyMirrorOnLayerStack(native_window);
        s_recordSurface.reset();
    }

    inline void shutdown()
    {
        Touch_Shutdown();
        if (g_Device) vkDeviceWaitIdle(g_Device);
        CleanupRecordSurface();
        s_cachedRecordDisplay.reset();
        s_pendingRecordDisplay.reset();
        s_recordDisplayMissed = false;
        s_lastRecordDisplayScan = {};

        if (vulkan_backend_initialized) ImGui_ImplVulkan_Shutdown();
        if (android_backend_initialized) ImGui_ImplAndroid_Shutdown();
        if (imgui_context_created) ImGui::DestroyContext();
        vulkan_backend_initialized = false;
        android_backend_initialized = false;
        imgui_context_created = false;

        if (g_Device)
        {
            CleanupSwapchain();
            vkDestroyDescriptorPool(g_Device, g_DescriptorPool, nullptr);
            vkDestroyRenderPass(g_Device, g_RenderPass, nullptr);
            for (auto &semaphore : g_ImageAvailableSemaphores)
            {
                vkDestroySemaphore(g_Device, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
            for (auto &fence : g_InFlightFences)
            {
                vkDestroyFence(g_Device, fence, nullptr);
                fence = VK_NULL_HANDLE;
            }
            vkDestroyCommandPool(g_Device, g_CommandPool, nullptr);
            vkDestroyDevice(g_Device, nullptr);
            g_DescriptorPool = VK_NULL_HANDLE;
            g_RenderPass = VK_NULL_HANDLE;
            g_CommandPool = VK_NULL_HANDLE;
            g_Device = VK_NULL_HANDLE;
        }
        if (g_Instance)
        {
            vkDestroySurfaceKHR(g_Instance, g_Surface, nullptr);
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
        g_CurrentFrame = 0;
        g_FirstFrame = true;
        g_SwapChainRebuild = false;
    }

    inline int init(bool allowCapture)
    {
        int status = 0;
        if (vulkan_backend_initialized) return status;

        s_allowCapture = allowCapture;
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
        native_window = android::SurfaceControlManager::Create("Accessibility Overlays", max_side, max_side, !allowCapture, 2024);
        if (!native_window)
        {
            LS_LOGE_TAG("RenderVK", "创建 ANativeWindow 失败");
            status = -ENODEV;
            return status;
        }
        ANativeWindow_acquire(native_window);

        const char *instanceExtensions[] = {VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME};
        VkInstanceCreateInfo instanceInfo{};
        instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceInfo.enabledExtensionCount = IM_ARRAYSIZE(instanceExtensions);
        instanceInfo.ppEnabledExtensionNames = instanceExtensions;
        status = vkCreateInstance(&instanceInfo, nullptr, &g_Instance);
        if (status != VK_SUCCESS)
        {
            g_Instance = VK_NULL_HANDLE;
            shutdown();
            return status;
        }
        VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.window = native_window;
        status = vkCreateAndroidSurfaceKHR(g_Instance, &surfaceInfo, nullptr, &g_Surface);
        if (status != VK_SUCCESS)
        {
            g_Surface = VK_NULL_HANDLE;
            shutdown();
            return status;
        }
        uint32_t gpuCount = 0;
        status = vkEnumeratePhysicalDevices(g_Instance, &gpuCount, nullptr);
        if (status == VK_SUCCESS && gpuCount == 0) status = VK_ERROR_INITIALIZATION_FAILED;
        if (status != VK_SUCCESS)
        {
            shutdown();
            return status;
        }
        std::vector<VkPhysicalDevice> gpus(gpuCount);
        status = vkEnumeratePhysicalDevices(g_Instance, &gpuCount, gpus.data());
        if (status != VK_SUCCESS)
        {
            shutdown();
            return status;
        }

        uint32_t queueFamily = VK_QUEUE_FAMILY_IGNORED;
        for (uint32_t gpu = 0; gpu < gpuCount; ++gpu)
        {
            uint32_t count = 0;
            vkGetPhysicalDeviceQueueFamilyProperties(gpus[gpu], &count, nullptr);
            std::vector<VkQueueFamilyProperties> families(count);
            vkGetPhysicalDeviceQueueFamilyProperties(gpus[gpu], &count, families.data());
            for (uint32_t i = 0; i < count; ++i)
            {
                if (!(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
                VkBool32 presentSupport = VK_FALSE;
                status = vkGetPhysicalDeviceSurfaceSupportKHR(gpus[gpu], i, g_Surface, &presentSupport);
                if (status != VK_SUCCESS)
                {
                    shutdown();
                    return status;
                }
                if (!presentSupport) continue;
                queueFamily = i;
                break;
            }
            if (queueFamily == VK_QUEUE_FAMILY_IGNORED) continue;
            g_PhysicalDevice = gpus[gpu];
            break;
        }
        if (queueFamily == VK_QUEUE_FAMILY_IGNORED)
        {
            LS_LOGE_TAG("RenderVK", "没有找到同时支持图形和呈现的队列族");
            status = VK_ERROR_INITIALIZATION_FAILED;
            shutdown();
            return status;
        }

        const char *deviceExtension = VK_KHR_SWAPCHAIN_EXTENSION_NAME;
        const float queuePriority = 1.0f;
        VkDeviceQueueCreateInfo queueInfo{};
        queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueInfo.queueFamilyIndex = queueFamily;
        queueInfo.queueCount = 1;
        queueInfo.pQueuePriorities = &queuePriority;
        VkDeviceCreateInfo deviceInfo{};
        deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        deviceInfo.pQueueCreateInfos = &queueInfo;
        deviceInfo.queueCreateInfoCount = 1;
        deviceInfo.enabledExtensionCount = 1;
        deviceInfo.ppEnabledExtensionNames = &deviceExtension;
        status = vkCreateDevice(g_PhysicalDevice, &deviceInfo, nullptr, &g_Device);
        if (status != VK_SUCCESS)
        {
            g_Device = VK_NULL_HANDLE;
            shutdown();
            return status;
        }
        vkGetDeviceQueue(g_Device, queueFamily, 0, &g_Queue);

        status = CreateSwapchain();
        if (status != VK_SUCCESS)
        {
            LS_LOGE_TAG("RenderVK", "创建主交换链失败: %d", status);
            shutdown();
            return status;
        }

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamily;
        status = vkCreateCommandPool(g_Device, &poolInfo, nullptr, &g_CommandPool);
        if (status != VK_SUCCESS)
        {
            g_CommandPool = VK_NULL_HANDLE;
            shutdown();
            return status;
        }
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = g_CommandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        status = vkAllocateCommandBuffers(g_Device, &allocInfo, g_CommandBuffers);
        if (status != VK_SUCCESS)
        {
            shutdown();
            return status;
        }

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            status = vkCreateSemaphore(g_Device, &semaphoreInfo, nullptr, &g_ImageAvailableSemaphores[i]);
            if (status != VK_SUCCESS)
            {
                g_ImageAvailableSemaphores[i] = VK_NULL_HANDLE;
                shutdown();
                return status;
            }
            status = vkCreateFence(g_Device, &fenceInfo, nullptr, &g_InFlightFences[i]);
            if (status != VK_SUCCESS)
            {
                g_InFlightFences[i] = VK_NULL_HANDLE;
                shutdown();
                return status;
            }
        }

        const VkDescriptorPoolSize poolSizes[] = {{VK_DESCRIPTOR_TYPE_SAMPLER, 1000}, {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000}, {VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000}, {VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000}, {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000}, {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000}};
        VkDescriptorPoolCreateInfo descPoolInfo{};
        descPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        descPoolInfo.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        descPoolInfo.maxSets = 1000 * IM_ARRAYSIZE(poolSizes);
        descPoolInfo.poolSizeCount = IM_ARRAYSIZE(poolSizes);
        descPoolInfo.pPoolSizes = poolSizes;
        status = vkCreateDescriptorPool(g_Device, &descPoolInfo, nullptr, &g_DescriptorPool);
        if (status != VK_SUCCESS)
        {
            g_DescriptorPool = VK_NULL_HANDLE;
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
        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = g_Instance;
        initInfo.PhysicalDevice = g_PhysicalDevice;
        initInfo.Device = g_Device;
        initInfo.QueueFamily = queueFamily;
        initInfo.Queue = g_Queue;
        initInfo.DescriptorPool = g_DescriptorPool;
        initInfo.MinImageCount = static_cast<uint32_t>(g_SwapchainImages.size());
        initInfo.ImageCount = initInfo.MinImageCount;
        initInfo.PipelineInfoMain.RenderPass = g_RenderPass;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.CheckVkResultFn = [](VkResult result)
        {
            if (result != VK_SUCCESS) LS_LOGE_TAG("RenderVK", "ImGui Vulkan VkResult=%d", result);
        };
        vulkan_backend_initialized = ImGui_ImplVulkan_Init(&initInfo);
        if (!vulkan_backend_initialized)
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
            LS_LOGE_TAG("RenderVK", "初始化触摸输入失败");
            status = -EIO;
            shutdown();
            return status;
        }
        return status;
    }

    inline int drawBegin()
    {
        int status = 0;
        if (!vulkan_backend_initialized)
        {
            status = -ENODEV;
            return status;
        }

        bool rotated = false;
        const auto now = std::chrono::steady_clock::now();
        if (now - s_lastDisplayInfoQuery >= std::chrono::milliseconds(250))
        {
            s_lastDisplayInfoQuery = now;
            const auto updated = android::SurfaceControlManager::GetDisplayInfo();
            if (updated.width > 0 && updated.height > 0 &&
                (updated.width != displayInfo.width || updated.height != displayInfo.height || updated.orientation != displayInfo.orientation))
            {
                rotated = updated.orientation != displayInfo.orientation;
                displayInfo = updated;
                UpdateScreenData(displayInfo.width, displayInfo.height, displayInfo.orientation);
                g_SwapChainRebuild = true;
                s_lastRecordDisplayScan = {};
            }
        }
        if (g_SwapChainRebuild)
        {
            status = vkDeviceWaitIdle(g_Device);
            if (status != VK_SUCCESS) return status;
            status = CreateSwapchain();
            if (status == VK_SUCCESS)
            {
                g_SwapChainRebuild = false;
                ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(g_SwapchainImages.size()));
            }
            else if (status == VK_ERROR_OUT_OF_DATE_KHR || status == VK_INCOMPLETE) status = 0;
            else return status;
        }

        Touch_UpdateImGui();
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();
        if (g_FirstFrame || rotated) ImGui::SetNextWindowPos(ImVec2(100.0f, 100.0f), ImGuiCond_Always);
        g_FirstFrame = false;
        return status;
    }

    inline bool SameRecordDisplay(const android::SurfaceControlManager::RecordDisplayInfo &lhs, const android::SurfaceControlManager::RecordDisplayInfo &rhs)
    {
        return lhs.layerStack == rhs.layerStack && lhs.orientation == rhs.orientation && lhs.layerStackRect == rhs.layerStackRect && lhs.displayRect == rhs.displayRect;
    }

    inline int drawEnd()
    {
        int status = 0;
        if (!vulkan_backend_initialized)
        {
            status = -ENODEV;
            return status;
        }

        ImGui::Render();
        if (g_SwapChainRebuild) return status;

        const VkFence fence = g_InFlightFences[g_CurrentFrame];
        status = vkWaitForFences(g_Device, 1, &fence, VK_TRUE, UINT64_MAX);
        if (status != VK_SUCCESS) return status;

        uint32_t imageIndex;
        const VkResult acquire = vkAcquireNextImageKHR(g_Device, g_Swapchain, UINT64_MAX, g_ImageAvailableSemaphores[g_CurrentFrame], VK_NULL_HANDLE, &imageIndex);
        if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
        {
            g_SwapChainRebuild = true;
            return status;
        }
        // SUBOPTIMAL 仍然取得了图像；完成这次提交、消耗 acquire 信号量后再重建。
        if (acquire == VK_SUBOPTIMAL_KHR) g_SwapChainRebuild = true;
        else if (acquire != VK_SUCCESS)
        {
            status = acquire;
            return status;
        }

        const VkCommandBuffer cmd = g_CommandBuffers[g_CurrentFrame];
        status = vkResetCommandBuffer(cmd, 0);
        if (status != VK_SUCCESS) return status;
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        status = vkBeginCommandBuffer(cmd, &beginInfo);
        if (status != VK_SUCCESS) return status;

        const auto &image = g_SwapchainImages[imageIndex];
        const VkClearValue clearColor{};
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = g_RenderPass;
        renderPassInfo.framebuffer = image.framebuffer;
        renderPassInfo.renderArea.extent = g_SwapchainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        vkCmdBeginRenderPass(cmd, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), cmd);
        vkCmdEndRenderPass(cmd);
        status = vkEndCommandBuffer(cmd);
        if (status != VK_SUCCESS) return status;

        const VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = 1;
        submitInfo.pWaitSemaphores = &g_ImageAvailableSemaphores[g_CurrentFrame];
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &image.renderFinished;
        status = vkResetFences(g_Device, 1, &fence);
        if (status != VK_SUCCESS) return status;
        status = vkQueueSubmit(g_Queue, 1, &submitInfo, fence);
        if (status != VK_SUCCESS) return status;
        g_CurrentFrame = (g_CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &image.renderFinished;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &g_Swapchain;
        presentInfo.pImageIndices = &imageIndex;
        const VkResult present = vkQueuePresentKHR(g_Queue, &presentInfo);
        if (present == VK_ERROR_OUT_OF_DATE_KHR || present == VK_SUBOPTIMAL_KHR) g_SwapChainRebuild = true;
        else if (present != VK_SUCCESS)
        {
            status = present;
            return status;
        }

        if (!s_allowCapture) return status;
        const auto now = std::chrono::steady_clock::now();
        if (s_lastRecordDisplayScan == std::chrono::steady_clock::time_point{} || now - s_lastRecordDisplayScan >= std::chrono::seconds(2))
        {
            s_lastRecordDisplayScan = now;
            android::SurfaceControlManager::RecordDisplayInfo scanned{};
            if (!android::SurfaceControlManager::FindRecordDisplay(&scanned, displayInfo.width, displayInfo.height))
            {
                if (s_recordDisplayMissed) s_cachedRecordDisplay.reset();
                s_recordDisplayMissed = true;
                s_pendingRecordDisplay.reset();
            }
            else
            {
                s_recordDisplayMissed = false;
                // 已有镜像时，连续两次扫描到同一新目标才切换 layerStack。
                if (!s_recordSurface || !s_cachedRecordDisplay || scanned.layerStack == s_cachedRecordDisplay->layerStack ||
                    (s_pendingRecordDisplay && SameRecordDisplay(scanned, *s_pendingRecordDisplay)))
                {
                    s_cachedRecordDisplay = scanned;
                    s_pendingRecordDisplay.reset();
                }
                else s_pendingRecordDisplay = scanned;
            }
        }
        if (!s_cachedRecordDisplay)
        {
            CleanupRecordSurface();
            return status;
        }

        const auto &target = *s_cachedRecordDisplay;
        if (!s_recordSurface || s_recordSurface->display.layerStack != target.layerStack)
        {
            CleanupRecordSurface();
            if (!android::SurfaceControlManager::CreateMirrorOnLayerStack(native_window, target.layerStack)) return status;
            s_recordSurface.emplace();
        }
        else if (SameRecordDisplay(s_recordSurface->display, target) && s_recordSurface->source.orientation == displayInfo.orientation &&
                 s_recordSurface->source.width == displayInfo.width && s_recordSurface->source.height == displayInfo.height) return status;

        if (!android::SurfaceControlManager::ConfigureMirrorProjection(native_window, displayInfo, target))
        {
            CleanupRecordSurface();
            return status;
        }
        s_recordSurface->display = target;
        s_recordSurface->source = displayInfo;
        LS_LOGI_TAG_FMT("RenderVK", "录屏镜像层就绪 layerStack={} orientation={} stackRect=({},{}-{},{}), displayRect=({},{}-{},{}), source={}x{}", target.layerStack, target.orientation, target.layerStackRect.left, target.layerStackRect.top, target.layerStackRect.right, target.layerStackRect.bottom, target.displayRect.left, target.displayRect.top, target.displayRect.right, target.displayRect.bottom, displayInfo.width, displayInfo.height);
        return status;
    }
} // namespace RenderVK