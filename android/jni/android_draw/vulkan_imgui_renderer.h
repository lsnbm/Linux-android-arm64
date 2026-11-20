#pragma once

#include <algorithm>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstring>
#include <limits>
#include <memory>
#include <optional>
#include <span>
#include <utility>
#include <vector>

#include <android/native_window.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

// Android/ImGui 依赖
#include "android_imgui_input/imgui_touch_input.h"
#include "android_surface/surface_control_manager.h"
#include "imgui/backends/imgui_impl_android.h"
#include "imgui/backends/imgui_impl_vulkan.h"
#include "imgui/imgui.h"
#include "imgui/imgui_internal.h" // RegisterUserTexture / UnregisterUserTexture
#include "logger/logger.h"

// Vulkan 后端实现
class VulkanRenderer
{
    std::vector<std::unique_ptr<ImTextureData>> textures_;
    VkInstance instance_ = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice_ = VK_NULL_HANDLE;
    VkDevice device_ = VK_NULL_HANDLE;
    VkQueue queue_ = VK_NULL_HANDLE;
    VkSurfaceKHR surface_ = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain_ = VK_NULL_HANDLE;
    VkFormat swapchainFormat_ = VK_FORMAT_UNDEFINED;
    VkExtent2D swapchainExtent_{};
    VkRenderPass renderPass_ = VK_NULL_HANDLE;
    VkCommandPool commandPool_ = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool_ = VK_NULL_HANDLE;

    struct SwapchainImage
    {
        VkImageView view = VK_NULL_HANDLE;
        VkFramebuffer framebuffer = VK_NULL_HANDLE;
        VkSemaphore renderFinished = VK_NULL_HANDLE;
    };
    std::vector<SwapchainImage> swapchainImages_;

    static constexpr uint32_t MAX_FRAMES_IN_FLIGHT = 2;
    VkCommandBuffer commandBuffers_[MAX_FRAMES_IN_FLIGHT]{};
    VkSemaphore imageAvailableSemaphores_[MAX_FRAMES_IN_FLIGHT]{};
    VkFence inFlightFences_[MAX_FRAMES_IN_FLIGHT]{};
    uint32_t currentFrame_ = 0;
    bool swapChainRebuild_ = false;
    ImGuiContext *imgui_context = nullptr;
    bool android_backend_initialized = false;
    bool vulkan_backend_initialized = false;
    ANativeWindow *native_window = nullptr;
    android::SurfaceControlManager::DisplayInfo displayInfo{};
    std::chrono::steady_clock::time_point lastDisplayInfoQuery_{};

    struct ImGuiContextScope
    {
        ImGuiContext *previous = ImGui::GetCurrentContext();
        explicit ImGuiContextScope(ImGuiContext *current)
        {
            ImGui::SetCurrentContext(current);
        }
        ~ImGuiContextScope()
        {
            ImGui::SetCurrentContext(previous);
        }
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

    static void CheckVkResult(VkResult result)
    {
        if (result != VK_SUCCESS) LS_LOGE_TAG("RenderVK", "ImGui Vulkan VkResult=%d", result);
    }

    void CleanupSwapchain()
    {
        for (const auto &image : swapchainImages_)
        {
            vkDestroyFramebuffer(device_, image.framebuffer, nullptr);
            vkDestroyImageView(device_, image.view, nullptr);
            vkDestroySemaphore(device_, image.renderFinished, nullptr);
        }
        swapchainImages_.clear();
        vkDestroySwapchainKHR(device_, swapchain_, nullptr);
        swapchain_ = VK_NULL_HANDLE;
    }

    int CreateSwapchain()
    {
        int status = 0;
        VkSurfaceCapabilitiesKHR capabilities{};
        status = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice_, surface_, &capabilities);
        if (status != VK_SUCCESS) return status;

        uint32_t formatCount = 0;
        status = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, nullptr);
        if (status != VK_SUCCESS) return status;
        if (formatCount == 0)
        {
            status = VK_ERROR_FORMAT_NOT_SUPPORTED;
            return status;
        }
        std::vector<VkSurfaceFormatKHR> formats(formatCount);
        status = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice_, surface_, &formatCount, formats.data());
        if (status != VK_SUCCESS) return status;
        formats.resize(formatCount);

        // 重建时继续使用 ImGui 图形管线对应的 render pass 格式。
        VkSurfaceFormatKHR surfaceFormat{};
        for (const auto &format : formats)
        {
            if (format.format == VK_FORMAT_UNDEFINED)
            {
                surfaceFormat = {renderPass_ ? swapchainFormat_ : VK_FORMAT_R8G8B8A8_UNORM, format.colorSpace};
                break;
            }
            if (renderPass_ && format.format != swapchainFormat_) continue;
            if (surfaceFormat.format == VK_FORMAT_UNDEFINED) surfaceFormat = format;
            if (renderPass_ || format.format == VK_FORMAT_R8G8B8A8_UNORM || format.format == VK_FORMAT_B8G8R8A8_UNORM)
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
        createInfo.surface = surface_;
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
        // 支持时使用 identity transform，显示方向由系统显示投影处理。
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
        // 将旧交换链传给驱动；在新链的创建调用结束前不能销毁旧链。
        createInfo.oldSwapchain = swapchain_;
        VkSwapchainKHR newSwapchain = VK_NULL_HANDLE;
        status = vkCreateSwapchainKHR(device_, &createInfo, nullptr, &newSwapchain);
        // 按 vkCreateSwapchainKHR 的 oldSwapchain 约定，即便创建失败，旧链也已退役。
        CleanupSwapchain();
        if (status != VK_SUCCESS) return status;
        swapchain_ = newSwapchain;
        swapchainExtent_ = createInfo.imageExtent;

        if (!renderPass_)
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
            status = vkCreateRenderPass(device_, &renderPassInfo, nullptr, &renderPass_);
            if (status != VK_SUCCESS)
            {
                renderPass_ = VK_NULL_HANDLE;
                CleanupSwapchain();
                return status;
            }
            swapchainFormat_ = surfaceFormat.format;
        }

        uint32_t imageCount = 0;
        status = vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, nullptr);
        if (status == VK_SUCCESS && imageCount < 2) status = VK_ERROR_INITIALIZATION_FAILED;
        if (status != VK_SUCCESS)
        {
            CleanupSwapchain();
            return status;
        }
        std::vector<VkImage> images(imageCount);
        status = vkGetSwapchainImagesKHR(device_, swapchain_, &imageCount, images.data());
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
        framebufferInfo.renderPass = renderPass_;
        framebufferInfo.attachmentCount = 1;
        framebufferInfo.width = swapchainExtent_.width;
        framebufferInfo.height = swapchainExtent_.height;
        framebufferInfo.layers = 1;
        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        swapchainImages_.resize(imageCount);
        for (uint32_t i = 0; i < imageCount; ++i)
        {
            auto &image = swapchainImages_[i];
            viewInfo.image = images[i];
            status = vkCreateImageView(device_, &viewInfo, nullptr, &image.view);
            if (status != VK_SUCCESS)
            {
                image.view = VK_NULL_HANDLE;
                CleanupSwapchain();
                return status;
            }
            framebufferInfo.pAttachments = &image.view;
            status = vkCreateFramebuffer(device_, &framebufferInfo, nullptr, &image.framebuffer);
            if (status != VK_SUCCESS)
            {
                image.framebuffer = VK_NULL_HANDLE;
                CleanupSwapchain();
                return status;
            }
            status = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &image.renderFinished);
            if (status != VK_SUCCESS)
            {
                image.renderFinished = VK_NULL_HANDLE;
                CleanupSwapchain();
                return status;
            }
        }
        return status;
    }

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
        if (device_) vkDeviceWaitIdle(device_);
        CleanupRecordSurface();
        cachedRecordDisplay_.reset();
        pendingRecordDisplay_.reset();
        recordDisplayMissed_ = false;
        lastRecordDisplayScan_ = {};

        if (imgui_context)
        {
            ImGuiContextScope scope{imgui_context};
            if (scope.previous == imgui_context) scope.previous = nullptr;
            if (vulkan_backend_initialized) ImGui_ImplVulkan_Shutdown();
            // 后端在设备空闲后释放已上传的纹理；此时才可撤销注册并释放像素和描述对象。
            for (const auto &texture : textures_) ImGui::UnregisterUserTexture(texture.get());
            textures_.clear();
            if (android_backend_initialized) ImGui_ImplAndroid_Shutdown();
            ImGui::DestroyContext(imgui_context);
            imgui_context = nullptr;
        }
        vulkan_backend_initialized = false;
        android_backend_initialized = false;

        if (device_)
        {
            CleanupSwapchain();
            vkDestroyDescriptorPool(device_, descriptorPool_, nullptr);
            vkDestroyRenderPass(device_, renderPass_, nullptr);
            for (auto &semaphore : imageAvailableSemaphores_)
            {
                vkDestroySemaphore(device_, semaphore, nullptr);
                semaphore = VK_NULL_HANDLE;
            }
            for (auto &fence : inFlightFences_)
            {
                vkDestroyFence(device_, fence, nullptr);
                fence = VK_NULL_HANDLE;
            }
            vkDestroyCommandPool(device_, commandPool_, nullptr);
            vkDestroyDevice(device_, nullptr);
            descriptorPool_ = VK_NULL_HANDLE;
            renderPass_ = VK_NULL_HANDLE;
            commandPool_ = VK_NULL_HANDLE;
            device_ = VK_NULL_HANDLE;
        }
        if (instance_)
        {
            vkDestroySurfaceKHR(instance_, surface_, nullptr);
            vkDestroyInstance(instance_, nullptr);
            surface_ = VK_NULL_HANDLE;
            instance_ = VK_NULL_HANDLE;
        }
        if (native_window)
        {
            if (android::gSurfaceControl) android::gSurfaceControl->Destroy(native_window);
            ANativeWindow_release(native_window);
            native_window = nullptr;
        }
        physicalDevice_ = VK_NULL_HANDLE;
        queue_ = VK_NULL_HANDLE;
        std::fill_n(commandBuffers_, MAX_FRAMES_IN_FLIGHT, VK_NULL_HANDLE);
        swapchainFormat_ = VK_FORMAT_UNDEFINED;
        swapchainExtent_ = {};
        currentFrame_ = 0;
        swapChainRebuild_ = false;
    }

public:
    VulkanRenderer() = default;
    VulkanRenderer(const VulkanRenderer &) = delete;
    VulkanRenderer &operator=(const VulkanRenderer &) = delete;
    ~VulkanRenderer()
    {
        shutdown();
    }

    // 在渲染线程调用。引用可在本帧立即绘制，有效期至本实例析构。
    std::optional<ImTextureRef> createRgbaTexture(int width, int height, std::span<const uint8_t> pixels)
    {
        if (!vulkan_backend_initialized || width <= 0 || height <= 0) return std::nullopt;
        if (width > std::numeric_limits<int>::max() / 4 / height) return std::nullopt;
        if (pixels.size() != static_cast<size_t>(width * height * 4)) return std::nullopt;
        ImGuiContextScope scope{imgui_context};
        const auto &platform = ImGui::GetPlatformIO();
        if ((platform.Renderer_TextureMaxWidth > 0 && width > platform.Renderer_TextureMaxWidth) || (platform.Renderer_TextureMaxHeight > 0 && height > platform.Renderer_TextureMaxHeight)) return std::nullopt;

        auto texture = std::make_unique<ImTextureData>();
        texture->Create(ImTextureFormat_RGBA32, width, height);
        std::memcpy(texture->GetPixels(), pixels.data(), pixels.size());
        texture->UseColors = true;
        auto &owned = textures_.emplace_back(std::move(texture));
        ImGui::RegisterUserTexture(owned.get());
        return owned->GetTexRef();
    }

    template <class Setup> int init(bool allowCapture, Setup &&setup, const char *windowName = "ImGui", int32_t windowType = -1)
    {
        int status = 0;
        if (vulkan_backend_initialized) return status;
        if (!gTouchInput || !android::gSurfaceControl || !android::gBinderDump) return status = -ENODEV;
        // 不借用外部 context；失败回滚及退出只清理本实例创建的 context。
        if (ImGui::GetCurrentContext()) return status = -EBUSY;
        struct Rollback
        {
            VulkanRenderer *renderer;
            ~Rollback()
            {
                if (renderer) renderer->shutdown();
            }
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
        status = vkCreateInstance(&instanceInfo, nullptr, &instance_);
        if (status != VK_SUCCESS)
        {
            instance_ = VK_NULL_HANDLE;
            return status;
        }
        VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
        surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
        surfaceInfo.window = native_window;
        status = vkCreateAndroidSurfaceKHR(instance_, &surfaceInfo, nullptr, &surface_);
        if (status != VK_SUCCESS)
        {
            surface_ = VK_NULL_HANDLE;
            return status;
        }
        uint32_t gpuCount = 0;
        status = vkEnumeratePhysicalDevices(instance_, &gpuCount, nullptr);
        if (status == VK_SUCCESS && gpuCount == 0) status = VK_ERROR_INITIALIZATION_FAILED;
        if (status != VK_SUCCESS) return status;
        std::vector<VkPhysicalDevice> gpus(gpuCount);
        status = vkEnumeratePhysicalDevices(instance_, &gpuCount, gpus.data());
        if (status != VK_SUCCESS) return status;

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
                status = vkGetPhysicalDeviceSurfaceSupportKHR(gpus[gpu], i, surface_, &presentSupport);
                if (status != VK_SUCCESS) return status;
                if (!presentSupport) continue;
                queueFamily = i;
                break;
            }
            if (queueFamily == VK_QUEUE_FAMILY_IGNORED) continue;
            physicalDevice_ = gpus[gpu];
            break;
        }
        if (queueFamily == VK_QUEUE_FAMILY_IGNORED)
        {
            LS_LOGE_TAG("RenderVK", "没有找到同时支持图形和呈现的队列族");
            status = VK_ERROR_INITIALIZATION_FAILED;
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
        status = vkCreateDevice(physicalDevice_, &deviceInfo, nullptr, &device_);
        if (status != VK_SUCCESS)
        {
            device_ = VK_NULL_HANDLE;
            return status;
        }
        vkGetDeviceQueue(device_, queueFamily, 0, &queue_);

        status = CreateSwapchain();
        if (status != VK_SUCCESS)
        {
            LS_LOGE_TAG("RenderVK", "创建主交换链失败: %d", status);
            return status;
        }

        VkCommandPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
        poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
        poolInfo.queueFamilyIndex = queueFamily;
        status = vkCreateCommandPool(device_, &poolInfo, nullptr, &commandPool_);
        if (status != VK_SUCCESS)
        {
            commandPool_ = VK_NULL_HANDLE;
            return status;
        }
        VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool_;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
        status = vkAllocateCommandBuffers(device_, &allocInfo, commandBuffers_);
        if (status != VK_SUCCESS) return status;

        VkSemaphoreCreateInfo semaphoreInfo{};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
        VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
        for (uint32_t i = 0; i < MAX_FRAMES_IN_FLIGHT; ++i)
        {
            status = vkCreateSemaphore(device_, &semaphoreInfo, nullptr, &imageAvailableSemaphores_[i]);
            if (status != VK_SUCCESS)
            {
                imageAvailableSemaphores_[i] = VK_NULL_HANDLE;
                return status;
            }
            status = vkCreateFence(device_, &fenceInfo, nullptr, &inFlightFences_[i]);
            if (status != VK_SUCCESS)
            {
                inFlightFences_[i] = VK_NULL_HANDLE;
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
        status = vkCreateDescriptorPool(device_, &descPoolInfo, nullptr, &descriptorPool_);
        if (status != VK_SUCCESS)
        {
            descriptorPool_ = VK_NULL_HANDLE;
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
        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = instance_;
        initInfo.PhysicalDevice = physicalDevice_;
        initInfo.Device = device_;
        initInfo.QueueFamily = queueFamily;
        initInfo.Queue = queue_;
        initInfo.DescriptorPool = descriptorPool_;
        initInfo.MinImageCount = static_cast<uint32_t>(swapchainImages_.size());
        initInfo.ImageCount = initInfo.MinImageCount;
        initInfo.PipelineInfoMain.RenderPass = renderPass_;
        initInfo.PipelineInfoMain.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        initInfo.CheckVkResultFn = CheckVkResult;
        vulkan_backend_initialized = ImGui_ImplVulkan_Init(&initInfo);
        if (!vulkan_backend_initialized)
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
            LS_LOGE_TAG("RenderVK", "初始化触摸输入失败: %d", status);
            return status;
        }
        rollback.renderer = nullptr;
        return status;
    }

    template <typename Draw> int drawFrame(Draw &&draw)
    {
        int status = 0;
        if (!vulkan_backend_initialized || !gTouchInput || !android::gSurfaceControl)
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
            if (updated.width > 0 && updated.height > 0 && (updated.width != displayInfo.width || updated.height != displayInfo.height || updated.orientation != displayInfo.orientation))
            {
                displayInfo = updated;
                gTouchInput->setScreen(displayInfo.width, displayInfo.height, displayInfo.orientation);
                swapChainRebuild_ = true;
                // 不等待常规轮询周期，下一次呈现后立即刷新录屏投影。
                lastRecordDisplayScan_ = {};
            }
        }
        if (swapChainRebuild_)
        {
            status = vkDeviceWaitIdle(device_);
            if (status != VK_SUCCESS) return status;
            status = CreateSwapchain();
            if (status == VK_SUCCESS)
            {
                swapChainRebuild_ = false;
                ImGui_ImplVulkan_SetMinImageCount(static_cast<uint32_t>(swapchainImages_.size()));
            }
            else if (status == VK_ERROR_OUT_OF_DATE_KHR || status == VK_INCOMPLETE) status = 0;
            else return status;
        }

        gTouchInput->updateImGui();
        ImGui_ImplVulkan_NewFrame();
        ImGui_ImplAndroid_NewFrame();
        ImGui::NewFrame();
        const bool block = draw(displayInfo.width, displayInfo.height);
        ImGui::Render();
        status = gTouchInput->setBlocking(block);
        if (status != 0) return status;
        if (swapChainRebuild_) return status;

        const VkFence fence = inFlightFences_[currentFrame_];
        status = vkWaitForFences(device_, 1, &fence, VK_TRUE, UINT64_MAX);
        if (status != VK_SUCCESS) return status;

        uint32_t imageIndex;
        const VkResult acquire = vkAcquireNextImageKHR(device_, swapchain_, UINT64_MAX, imageAvailableSemaphores_[currentFrame_], VK_NULL_HANDLE, &imageIndex);
        if (acquire == VK_ERROR_OUT_OF_DATE_KHR)
        {
            swapChainRebuild_ = true;
            return status;
        }
        // Android 可能持续报告 SUBOPTIMAL，交换链仍可用；仅在显示变化或 OUT_OF_DATE 时重建。
        // 此时仍已取得图像，必须完成提交并消耗 acquire 信号量。
        if (acquire != VK_SUCCESS && acquire != VK_SUBOPTIMAL_KHR)
        {
            status = acquire;
            return status;
        }

        const VkCommandBuffer cmd = commandBuffers_[currentFrame_];
        status = vkResetCommandBuffer(cmd, 0);
        if (status != VK_SUCCESS) return status;
        VkCommandBufferBeginInfo beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        status = vkBeginCommandBuffer(cmd, &beginInfo);
        if (status != VK_SUCCESS) return status;

        const auto &image = swapchainImages_[imageIndex];
        const VkClearValue clearColor{};
        VkRenderPassBeginInfo renderPassInfo{};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = renderPass_;
        renderPassInfo.framebuffer = image.framebuffer;
        renderPassInfo.renderArea.extent = swapchainExtent_;
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
        submitInfo.pWaitSemaphores = &imageAvailableSemaphores_[currentFrame_];
        submitInfo.pWaitDstStageMask = &waitStage;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &cmd;
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &image.renderFinished;
        status = vkResetFences(device_, 1, &fence);
        if (status != VK_SUCCESS) return status;
        status = vkQueueSubmit(queue_, 1, &submitInfo, fence);
        if (status != VK_SUCCESS) return status;
        currentFrame_ = (currentFrame_ + 1) % MAX_FRAMES_IN_FLIGHT;

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &image.renderFinished;
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = &swapchain_;
        presentInfo.pImageIndices = &imageIndex;
        const VkResult present = vkQueuePresentKHR(queue_, &presentInfo);
        if (present == VK_ERROR_OUT_OF_DATE_KHR) swapChainRebuild_ = true;
        else if (present != VK_SUCCESS && present != VK_SUBOPTIMAL_KHR)
        {
            status = present;
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
                if (!recordSurface_ || !cachedRecordDisplay_ || scanned.layerStack == cachedRecordDisplay_->layerStack || (pendingRecordDisplay_ && SameRecordDisplay(scanned, *pendingRecordDisplay_)))
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
        else if (SameRecordDisplay(recordSurface_->display, target) && recordSurface_->source.orientation == displayInfo.orientation && recordSurface_->source.width == displayInfo.width && recordSurface_->source.height == displayInfo.height) return status;

        if (android::gSurfaceControl->ConfigureMirrorProjection(native_window, displayInfo, target) != 0)
        {
            CleanupRecordSurface();
            return status;
        }
        recordSurface_->display = target;
        recordSurface_->source = displayInfo;
        LS_LOGI_TAG_FMT("RenderVK", "录屏镜像层就绪 layerStack={} orientation={} stackRect=({},{}-{},{}), displayRect=({},{}-{},{}), source={}x{}", target.layerStack, target.orientation, target.layerStackRect.left, target.layerStackRect.top, target.layerStackRect.right, target.layerStackRect.bottom, target.displayRect.left, target.displayRect.top, target.displayRect.right, target.displayRect.bottom, displayInfo.width, displayInfo.height);
        return status;
    }
};

inline std::unique_ptr<VulkanRenderer> gVulkanRenderer = nullptr;
