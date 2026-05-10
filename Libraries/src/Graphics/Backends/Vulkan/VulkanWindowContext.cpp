#include "Graphics/Backends/Vulkan/VulkanWindowContext.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Graphics/Backends/Vulkan/VulkanGraphicsDevice.h"
#include "Graphics/Backends/Vulkan/VulkanGraphicsResources.h"
#include "Graphics/Backends/Vulkan/VulkanPipelineCache.h"
#include "Graphics/Backends/Vulkan/VulkanRenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Logger.h"

namespace VulkanWindowContext
{

namespace
{

constexpr std::uint32_t MaxFramesInFlight = 2;

struct WindowFrameSync
{
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    VkFence inFlightFence = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
};

struct WindowContextState
{
    std::shared_ptr<VulkanDeviceContext> deviceContext;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkSurfaceFormatKHR surfaceFormat{};
    VkPresentModeKHR presentMode = VK_PRESENT_MODE_FIFO_KHR;
    VkExtent2D extent{};
    std::vector<VkImage> images;
    std::vector<VkImageView> imageViews;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthImageView = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkRenderPass renderPassLoad = VK_NULL_HANDLE; // LOAD_OP_LOAD variant for resumed swapchain pass
    std::vector<VkFramebuffer> framebuffers;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::array<WindowFrameSync, MaxFramesInFlight> frameSync{};
    std::vector<VkFence> imagesInFlight;
    std::uint32_t frameIndex = 0;
    // Per-frame state set by BeginFrame, consumed by Present
    std::uint32_t currentImageIndex = 0;
    bool frameActive = false;
    // FPS / frame counters for diagnosing device-lost timing
    std::uint64_t framesSubmitted = 0;
    std::uint32_t fpsWindowFrames = 0;
    std::chrono::steady_clock::time_point fpsWindowStart{};
};

std::unordered_map<GLFWwindow *, WindowContextState> windowContexts;

void DestroySwapchainResources(WindowContextState &state)
{
    if (state.deviceContext == nullptr || state.deviceContext->device == VK_NULL_HANDLE)
    {
        state.images.clear();
        state.imageViews.clear();
        state.framebuffers.clear();
        state.imagesInFlight.clear();
        state.swapchain = VK_NULL_HANDLE;
        return;
    }

    state.imagesInFlight.clear();

    for (VkFramebuffer framebuffer : state.framebuffers)
    {
        if (framebuffer != VK_NULL_HANDLE)
        {
            vkDestroyFramebuffer(state.deviceContext->device, framebuffer, nullptr);
        }
    }
    state.framebuffers.clear();

    if (state.depthImageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(state.deviceContext->device, state.depthImageView, nullptr);
        state.depthImageView = VK_NULL_HANDLE;
    }
    if (state.depthImage != VK_NULL_HANDLE)
    {
        vkDestroyImage(state.deviceContext->device, state.depthImage, nullptr);
        state.depthImage = VK_NULL_HANDLE;
    }
    if (state.depthMemory != VK_NULL_HANDLE)
    {
        vkFreeMemory(state.deviceContext->device, state.depthMemory, nullptr);
        state.depthMemory = VK_NULL_HANDLE;
    }
    state.depthFormat = VK_FORMAT_UNDEFINED;

    if (state.renderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(state.deviceContext->device, state.renderPass, nullptr);
        state.renderPass = VK_NULL_HANDLE;
    }

    if (state.renderPassLoad != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(state.deviceContext->device, state.renderPassLoad, nullptr);
        state.renderPassLoad = VK_NULL_HANDLE;
    }

    for (VkImageView imageView : state.imageViews)
    {
        if (imageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(state.deviceContext->device, imageView, nullptr);
        }
    }
    state.imageViews.clear();
    state.images.clear();

    if (state.swapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(state.deviceContext->device, state.swapchain, nullptr);
        state.swapchain = VK_NULL_HANDLE;
    }
}

void DestroyWindowContext(WindowContextState &state)
{
    if (state.deviceContext != nullptr && state.deviceContext->device != VK_NULL_HANDLE)
    {
        vkDeviceWaitIdle(state.deviceContext->device);
    }

    DestroySwapchainResources(state);

    if (state.deviceContext != nullptr && state.deviceContext->device != VK_NULL_HANDLE)
    {
        for (WindowFrameSync &frameSync : state.frameSync)
        {
            if (frameSync.imageAvailable != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(state.deviceContext->device, frameSync.imageAvailable, nullptr);
                frameSync.imageAvailable = VK_NULL_HANDLE;
            }
            if (frameSync.renderFinished != VK_NULL_HANDLE)
            {
                vkDestroySemaphore(state.deviceContext->device, frameSync.renderFinished, nullptr);
                frameSync.renderFinished = VK_NULL_HANDLE;
            }
            if (frameSync.inFlightFence != VK_NULL_HANDLE)
            {
                vkDestroyFence(state.deviceContext->device, frameSync.inFlightFence, nullptr);
                frameSync.inFlightFence = VK_NULL_HANDLE;
            }
            // cmd buffers are freed when the pool is destroyed
            frameSync.commandBuffer = VK_NULL_HANDLE;
        }

        if (state.commandPool != VK_NULL_HANDLE)
        {
            vkDestroyCommandPool(state.deviceContext->device, state.commandPool, nullptr);
            state.commandPool = VK_NULL_HANDLE;
        }
    }

    if (state.deviceContext != nullptr && state.deviceContext->backend != nullptr && state.surface != VK_NULL_HANDLE)
    {
        vkDestroySurfaceKHR(state.deviceContext->backend->instance, state.surface, nullptr);
        state.surface = VK_NULL_HANDLE;
    }

    state.deviceContext.reset();
}

VkSurfaceFormatKHR ChooseSurfaceFormat(const std::vector<VkSurfaceFormatKHR> &formats)
{
    for (const VkSurfaceFormatKHR &format : formats)
    {
        if (format.format == VK_FORMAT_B8G8R8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR)
        {
            return format;
        }
    }

    return formats.empty() ? VkSurfaceFormatKHR{} : formats.front();
}

VkPresentModeKHR ChoosePresentMode(const std::vector<VkPresentModeKHR> &presentModes, bool enableVsync)
{
    if (enableVsync)
    {
        return VK_PRESENT_MODE_FIFO_KHR;
    }

    if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_MAILBOX_KHR) != presentModes.end())
    {
        return VK_PRESENT_MODE_MAILBOX_KHR;
    }
    if (std::find(presentModes.begin(), presentModes.end(), VK_PRESENT_MODE_IMMEDIATE_KHR) != presentModes.end())
    {
        return VK_PRESENT_MODE_IMMEDIATE_KHR;
    }

    return VK_PRESENT_MODE_FIFO_KHR;
}

VkFormat ChooseDepthFormat(VkPhysicalDevice physicalDevice)
{
    const VkFormat candidates[] = {VK_FORMAT_D32_SFLOAT, VK_FORMAT_D32_SFLOAT_S8_UINT, VK_FORMAT_D24_UNORM_S8_UINT};
    for (VkFormat format : candidates)
    {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physicalDevice, format, &props);
        if ((props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT) != 0)
        {
            return format;
        }
    }
    return VK_FORMAT_UNDEFINED;
}

std::uint32_t FindMemoryTypeLocal(const VkPhysicalDeviceMemoryProperties &memProps, std::uint32_t typeFilter,
                                  VkMemoryPropertyFlags properties)
{
    for (std::uint32_t i = 0; i < memProps.memoryTypeCount; ++i)
    {
        if ((typeFilter & (1u << i)) != 0 && (memProps.memoryTypes[i].propertyFlags & properties) == properties)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

bool CreateDepthResources(WindowContextState &state)
{
    state.depthFormat = ChooseDepthFormat(state.deviceContext->backend->physicalDevice);
    if (state.depthFormat == VK_FORMAT_UNDEFINED)
    {
        LOG_ERROR(1, "[Vulkan] No supported depth format found");
        return false;
    }

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = state.depthFormat;
    imageInfo.extent = {state.extent.width, state.extent.height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;

    if (vkCreateImage(state.deviceContext->device, &imageInfo, nullptr, &state.depthImage) != VK_SUCCESS)
    {
        LOG_ERROR(1, "[Vulkan] Failed to create depth image");
        return false;
    }

    VkMemoryRequirements memReq{};
    vkGetImageMemoryRequirements(state.deviceContext->device, state.depthImage, &memReq);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memReq.size;
    allocInfo.memoryTypeIndex =
        FindMemoryTypeLocal(state.deviceContext->backend->memoryProperties, memReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (allocInfo.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(state.deviceContext->device, &allocInfo, nullptr, &state.depthMemory) != VK_SUCCESS)
    {
        LOG_ERROR(1, "[Vulkan] Failed to allocate depth image memory");
        return false;
    }
    vkBindImageMemory(state.deviceContext->device, state.depthImage, state.depthMemory, 0);

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = state.depthImage;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = state.depthFormat;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    viewInfo.subresourceRange.baseMipLevel = 0;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.baseArrayLayer = 0;
    viewInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(state.deviceContext->device, &viewInfo, nullptr, &state.depthImageView) != VK_SUCCESS)
    {
        LOG_ERROR(1, "[Vulkan] Failed to create depth image view");
        return false;
    }
    return true;
}

VkExtent2D ChooseExtent(const VkSurfaceCapabilitiesKHR &capabilities, int width, int height)
{
    if (capabilities.currentExtent.width != std::numeric_limits<std::uint32_t>::max())
    {
        return capabilities.currentExtent;
    }

    VkExtent2D extent{};
    extent.width =
        std::clamp(static_cast<std::uint32_t>(std::max(width, 1)), capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
    extent.height =
        std::clamp(static_cast<std::uint32_t>(std::max(height, 1)), capabilities.minImageExtent.height, capabilities.maxImageExtent.height);
    return extent;
}

bool CreateFrameSyncObjects(WindowContextState &state)
{
    VkSemaphoreCreateInfo semaphoreCreateInfo{};
    semaphoreCreateInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

    VkFenceCreateInfo fenceCreateInfo{};
    fenceCreateInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceCreateInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (WindowFrameSync &frameSync : state.frameSync)
    {
        if (vkCreateSemaphore(state.deviceContext->device, &semaphoreCreateInfo, nullptr, &frameSync.imageAvailable) != VK_SUCCESS ||
            vkCreateSemaphore(state.deviceContext->device, &semaphoreCreateInfo, nullptr, &frameSync.renderFinished) != VK_SUCCESS ||
            vkCreateFence(state.deviceContext->device, &fenceCreateInfo, nullptr, &frameSync.inFlightFence) != VK_SUCCESS)
        {
            LOG_ERROR(1, "Failed to create Vulkan frame synchronization objects");
            return false;
        }
    }

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = state.commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = MaxFramesInFlight;

    std::array<VkCommandBuffer, MaxFramesInFlight> buffers{};
    if (vkAllocateCommandBuffers(state.deviceContext->device, &allocateInfo, buffers.data()) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to allocate Vulkan per-frame command buffers");
        return false;
    }
    for (std::size_t i = 0; i < MaxFramesInFlight; ++i)
    {
        state.frameSync[i].commandBuffer = buffers[i];
    }

    return true;
}

bool CreateCommandPool(WindowContextState &state)
{
    VkCommandPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    createInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    createInfo.queueFamilyIndex = state.deviceContext->backend->graphicsQueueFamilyIndex;

    if (vkCreateCommandPool(state.deviceContext->device, &createInfo, nullptr, &state.commandPool) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to create Vulkan command pool");
        return false;
    }

    return true;
}

bool CreateSwapchain(WindowContextState &state, GLFWwindow *window, bool enableVsync, int width, int height)
{
    UNUSED(window);

    VkBool32 presentSupported = VK_FALSE;
    if (vkGetPhysicalDeviceSurfaceSupportKHR(state.deviceContext->backend->physicalDevice,
                                             state.deviceContext->backend->graphicsQueueFamilyIndex, state.surface,
                                             &presentSupported) != VK_SUCCESS ||
        presentSupported != VK_TRUE)
    {
        LOG_ERROR(1, "Selected Vulkan queue family does not support window presentation");
        return false;
    }

    VkSurfaceCapabilitiesKHR surfaceCapabilities{};
    std::uint32_t formatCount = 0;
    std::uint32_t presentModeCount = 0;
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(state.deviceContext->backend->physicalDevice, state.surface, &surfaceCapabilities) !=
            VK_SUCCESS ||
        vkGetPhysicalDeviceSurfaceFormatsKHR(state.deviceContext->backend->physicalDevice, state.surface, &formatCount, nullptr) !=
            VK_SUCCESS ||
        formatCount == 0 ||
        vkGetPhysicalDeviceSurfacePresentModesKHR(state.deviceContext->backend->physicalDevice, state.surface, &presentModeCount,
                                                  nullptr) != VK_SUCCESS ||
        presentModeCount == 0)
    {
        LOG_ERROR(1, "Failed to query Vulkan surface capabilities");
        return false;
    }

    std::vector<VkSurfaceFormatKHR> surfaceFormats(formatCount);
    std::vector<VkPresentModeKHR> presentModes(presentModeCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(state.deviceContext->backend->physicalDevice, state.surface, &formatCount, surfaceFormats.data());
    vkGetPhysicalDeviceSurfacePresentModesKHR(state.deviceContext->backend->physicalDevice, state.surface, &presentModeCount,
                                              presentModes.data());

    state.surfaceFormat = ChooseSurfaceFormat(surfaceFormats);
    state.presentMode = ChoosePresentMode(presentModes, enableVsync);
    state.extent = ChooseExtent(surfaceCapabilities, width, height);

    std::uint32_t imageCount = surfaceCapabilities.minImageCount + 1;
    if (surfaceCapabilities.maxImageCount > 0)
    {
        imageCount = std::min(imageCount, surfaceCapabilities.maxImageCount);
    }

    VkSwapchainCreateInfoKHR createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    createInfo.surface = state.surface;
    createInfo.minImageCount = imageCount;
    createInfo.imageFormat = state.surfaceFormat.format;
    createInfo.imageColorSpace = state.surfaceFormat.colorSpace;
    createInfo.imageExtent = state.extent;
    createInfo.imageArrayLayers = 1;
    createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    createInfo.preTransform = surfaceCapabilities.currentTransform;
    createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    createInfo.presentMode = state.presentMode;
    createInfo.clipped = VK_TRUE;
    createInfo.oldSwapchain = state.swapchain;

    VkSwapchainKHR oldSwapchain = state.swapchain;
    if (vkCreateSwapchainKHR(state.deviceContext->device, &createInfo, nullptr, &state.swapchain) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to create Vulkan swapchain");
        state.swapchain = oldSwapchain;
        return false;
    }

    if (oldSwapchain != VK_NULL_HANDLE)
    {
        vkDestroySwapchainKHR(state.deviceContext->device, oldSwapchain, nullptr);
    }

    std::uint32_t swapchainImageCount = 0;
    vkGetSwapchainImagesKHR(state.deviceContext->device, state.swapchain, &swapchainImageCount, nullptr);
    state.images.resize(swapchainImageCount);
    vkGetSwapchainImagesKHR(state.deviceContext->device, state.swapchain, &swapchainImageCount, state.images.data());

    state.imageViews.resize(swapchainImageCount, VK_NULL_HANDLE);
    for (std::uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        VkImageViewCreateInfo viewCreateInfo{};
        viewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewCreateInfo.image = state.images[i];
        viewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewCreateInfo.format = state.surfaceFormat.format;
        viewCreateInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        viewCreateInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewCreateInfo.subresourceRange.baseMipLevel = 0;
        viewCreateInfo.subresourceRange.levelCount = 1;
        viewCreateInfo.subresourceRange.baseArrayLayer = 0;
        viewCreateInfo.subresourceRange.layerCount = 1;

        if (vkCreateImageView(state.deviceContext->device, &viewCreateInfo, nullptr, &state.imageViews[i]) != VK_SUCCESS)
        {
            LOG_ERROR(1, "Failed to create Vulkan swapchain image view");
            return false;
        }
    }

    if (!CreateDepthResources(state))
    {
        return false;
    }

    VkAttachmentDescription attachments[2]{};
    attachments[0].format = state.surfaceFormat.format;
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    attachments[1].format = state.depthFormat;
    attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentRef{};
    colorAttachmentRef.attachment = 0;
    colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkAttachmentReference depthAttachmentRef{};
    depthAttachmentRef.attachment = 1;
    depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorAttachmentRef;
    subpass.pDepthStencilAttachment = &depthAttachmentRef;

    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    VkRenderPassCreateInfo renderPassCreateInfo{};
    renderPassCreateInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderPassCreateInfo.attachmentCount = 2;
    renderPassCreateInfo.pAttachments = attachments;
    renderPassCreateInfo.subpassCount = 1;
    renderPassCreateInfo.pSubpasses = &subpass;
    renderPassCreateInfo.dependencyCount = 1;
    renderPassCreateInfo.pDependencies = &dependency;

    if (vkCreateRenderPass(state.deviceContext->device, &renderPassCreateInfo, nullptr, &state.renderPass) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to create Vulkan swapchain render pass");
        return false;
    }

    // LOAD variant: same structure but LOAD instead of CLEAR — used when resuming the swapchain
    // render pass after an off-screen render target pass (so 3D content isn't wiped).
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR; // already written by the CLEAR pass
    attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    if (vkCreateRenderPass(state.deviceContext->device, &renderPassCreateInfo, nullptr, &state.renderPassLoad) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to create Vulkan swapchain load render pass");
        return false;
    }

    state.framebuffers.resize(swapchainImageCount, VK_NULL_HANDLE);
    for (std::uint32_t i = 0; i < swapchainImageCount; ++i)
    {
        VkImageView fbAttachments[2] = {state.imageViews[i], state.depthImageView};

        VkFramebufferCreateInfo framebufferCreateInfo{};
        framebufferCreateInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        framebufferCreateInfo.renderPass = state.renderPass;
        framebufferCreateInfo.attachmentCount = 2;
        framebufferCreateInfo.pAttachments = fbAttachments;
        framebufferCreateInfo.width = state.extent.width;
        framebufferCreateInfo.height = state.extent.height;
        framebufferCreateInfo.layers = 1;

        if (vkCreateFramebuffer(state.deviceContext->device, &framebufferCreateInfo, nullptr, &state.framebuffers[i]) != VK_SUCCESS)
        {
            LOG_ERROR(1, "Failed to create Vulkan swapchain framebuffer");
            return false;
        }
    }

    state.imagesInFlight.assign(swapchainImageCount, VK_NULL_HANDLE);

    VulkanRenderState::SetViewport(0, 0, static_cast<int>(state.extent.width), static_cast<int>(state.extent.height));

    return true;
}

bool RecreateSwapchain(WindowContextState &state, GLFWwindow *window, bool enableVsync)
{
    if (state.deviceContext == nullptr || state.deviceContext->device == VK_NULL_HANDLE)
    {
        return false;
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    if (framebufferWidth <= 0 || framebufferHeight <= 0)
    {
        return false;
    }

    vkDeviceWaitIdle(state.deviceContext->device);
    DestroySwapchainResources(state);
    return CreateSwapchain(state, window, enableVsync, framebufferWidth, framebufferHeight);
}

// BeginFrame: acquires the swapchain image, begins the command buffer and render pass.
// The render pass is left open so that draw calls recorded between BeginFrame and Present
// are executed inside it. VulkanRenderState holds the active command buffer for draw calls.
bool BeginFrame(WindowContextState &state, GLFWwindow *window)
{
    if (state.frameActive)
    {
        LOG_INFO("[Vulkan] BeginFrame called while a frame is already active — skipped");
        return true;
    }

    WindowFrameSync &frameSync = state.frameSync[state.frameIndex];

    if (const VkResult waitResult = vkWaitForFences(state.deviceContext->device, 1, &frameSync.inFlightFence, VK_TRUE, UINT64_MAX);
        waitResult != VK_SUCCESS)
    {
        LOG_ERROR(static_cast<int>(waitResult), "[Vulkan] BeginFrame: failed to wait for in-flight fence");
        return false;
    }

    VkResult acquireResult = vkAcquireNextImageKHR(state.deviceContext->device, state.swapchain, UINT64_MAX, frameSync.imageAvailable,
                                                   VK_NULL_HANDLE, &state.currentImageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        LOG_INFO("[Vulkan] BeginFrame: swapchain out of date, recreating");
        return RecreateSwapchain(state, window, state.presentMode == VK_PRESENT_MODE_FIFO_KHR);
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
    {
        LOG_ERROR(static_cast<int>(acquireResult), "[Vulkan] BeginFrame: failed to acquire swapchain image");
        return false;
    }

    // If this swapchain image was last used by a still-in-flight frame, wait for it to finish.
    if (state.currentImageIndex < state.imagesInFlight.size() && state.imagesInFlight[state.currentImageIndex] != VK_NULL_HANDLE &&
        state.imagesInFlight[state.currentImageIndex] != frameSync.inFlightFence)
    {
        vkWaitForFences(state.deviceContext->device, 1, &state.imagesInFlight[state.currentImageIndex], VK_TRUE, UINT64_MAX);
    }
    if (state.currentImageIndex < state.imagesInFlight.size())
    {
        state.imagesInFlight[state.currentImageIndex] = frameSync.inFlightFence;
    }

    VkCommandBuffer commandBuffer = frameSync.commandBuffer;
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        LOG_ERROR(1, "[Vulkan] BeginFrame: failed to begin command buffer");
        return false;
    }
    ResetVulkanResourceBindingCache();

    // Reset this frame slot's descriptor pool. Pools are per-slot so resetting one cannot recycle
    // descriptors still in use by another in-flight frame's GPU work — the fence wait above
    // guarantees the previous use of this slot is complete.
    VulkanPipelineCache::ResetFrameDescriptors(state.deviceContext->device, state.frameIndex);
    VulkanRenderState::ClearTextureBindings();

    // Free any VkBuffer/VkDeviceMemory whose retirement is now beyond the in-flight window. The
    // fence wait above covers the previous use of this slot, so resources retired then are safe
    // to destroy now.
    VulkanRenderState::DrainExpiredRetirements(state.deviceContext->device, MaxFramesInFlight);

    // Record all deferred texture uploads into the frame command buffer before the render pass
    // opens. The pipeline barriers inside FlushPendingTextureUploads ensure images reach
    // SHADER_READ_ONLY before the first draw call in the render pass reads them.
    FlushPendingTextureUploads(commandBuffer);

    const glm::vec4 clearColor = VulkanRenderState::GetClearColor();

    VkClearValue clearValues[2]{};
    clearValues[0].color = {.float32 = {clearColor.r, clearColor.g, clearColor.b, clearColor.a}};
    clearValues[1].depthStencil = {1.0f, 0};

    VkRenderPassBeginInfo renderPassBeginInfo{};
    renderPassBeginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    renderPassBeginInfo.renderPass = state.renderPass;
    renderPassBeginInfo.framebuffer = state.framebuffers[state.currentImageIndex];
    renderPassBeginInfo.renderArea.offset = {0, 0};
    renderPassBeginInfo.renderArea.extent = state.extent;
    renderPassBeginInfo.clearValueCount = 2;
    renderPassBeginInfo.pClearValues = clearValues;

    vkCmdBeginRenderPass(commandBuffer, &renderPassBeginInfo, VK_SUBPASS_CONTENTS_INLINE);

    // Set viewport and scissor dynamically so the pipeline can use them.
    // Negative-height viewport flips the Y axis so that GLSL written for OpenGL (Y up in NDC)
    // renders correctly under Vulkan (whose NDC Y is down). Requires VK_KHR_maintenance1, which
    // is core in Vulkan 1.1+ — we target 1.2.
    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = static_cast<float>(state.extent.height);
    viewport.width = static_cast<float>(state.extent.width);
    viewport.height = -static_cast<float>(state.extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = state.extent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    // Expose the command buffer globally so draw calls can record into it
    VulkanRenderState::SetCurrentCommandBuffer(commandBuffer, state.extent);
    VulkanRenderState::SetFramebuffer(0);
    VulkanRenderState::SetCurrentRenderPass(state.renderPass);

    state.frameActive = true;
    return true;
}

WindowContextState *FindWindowContext(GLFWwindow *window) noexcept
{
    const auto it = windowContexts.find(window);
    return it == windowContexts.end() ? nullptr : &it->second;
}

WindowContextState *FindActiveWindowContext() noexcept
{
    for (auto &entry : windowContexts)
    {
        if (entry.second.frameActive)
        {
            return &entry.second;
        }
    }
    return nullptr;
}

} // namespace

bool Initialize(GLFWwindow *window, bool enableVsync, int width, int height)
{
    if (window == nullptr)
    {
        return false;
    }

    const auto *device = dynamic_cast<const VulkanGraphicsDevice *>(TryGetActiveGraphicsDevice());
    if (device == nullptr || device->GetDeviceContext() == nullptr || device->GetDeviceContext()->backend == nullptr)
    {
        LOG_ERROR(1, "Active graphics device is not a Vulkan device");
        return false;
    }

    WindowContextState state{};
    state.deviceContext = device->GetDeviceContext();

    if (glfwCreateWindowSurface(state.deviceContext->backend->instance, window, nullptr, &state.surface) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to create Vulkan window surface");
        return false;
    }

    if (!CreateCommandPool(state) || !CreateFrameSyncObjects(state) || !CreateSwapchain(state, window, enableVsync, width, height))
    {
        DestroyWindowContext(state);
        return false;
    }

    windowContexts[window] = std::move(state);
    VulkanRenderState::SetDepthTest(true);
    return true;
}

void ApplyDefaultFramebufferState(GLFWwindow *window, bool enableVsync, const glm::vec4 &clearColor) noexcept
{
    WindowContextState *state = FindWindowContext(window);
    if (state == nullptr)
    {
        LOG_ERROR(1, "[Vulkan] ApplyDefaultFramebufferState: no window context found");
        return;
    }

    // Make sure the next BeginFrame uses the requested clear color
    VulkanRenderState::ClearColor(clearColor);

    // If a frame was not properly closed, close it now (shouldn't happen normally)
    if (state->frameActive)
    {
        LOG_INFO("[Vulkan] ApplyDefaultFramebufferState: previous frame still active, closing it");
        VkCommandBuffer commandBuffer = state->frameSync[state->frameIndex].commandBuffer;
        vkCmdEndRenderPass(commandBuffer);
        vkEndCommandBuffer(commandBuffer);
        VulkanRenderState::SetCurrentCommandBuffer(VK_NULL_HANDLE, {});
        VulkanRenderState::SetCurrentRenderPass(VK_NULL_HANDLE);
        VulkanRenderState::SetFramebuffer(0);
        state->frameActive = false;
    }

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

    if (framebufferWidth > 0 && framebufferHeight > 0 &&
        (static_cast<std::uint32_t>(framebufferWidth) != state->extent.width ||
         static_cast<std::uint32_t>(framebufferHeight) != state->extent.height))
    {
        LOG_INFO("[Vulkan] ApplyDefaultFramebufferState: extent changed to " + std::to_string(framebufferWidth) + "x" +
                 std::to_string(framebufferHeight) + ", recreating swapchain");
        RecreateSwapchain(*state, window, enableVsync);
    }

    VulkanRenderState::SetViewport(0, 0, static_cast<int>(state->extent.width), static_cast<int>(state->extent.height));
    VulkanRenderState::SetDepthTest(true);

    // Begin the frame: acquire image, open render pass, expose command buffer
    BeginFrame(*state, window);
}

void Present(GLFWwindow *window) noexcept
{
    WindowContextState *state = FindWindowContext(window);
    if (state == nullptr || state->swapchain == VK_NULL_HANDLE)
    {
        return;
    }

    if (!state->frameActive)
    {
        LOG_ERROR(1, "[Vulkan] Present called with no active frame — was ApplyDefaultFramebufferState called this frame?");
        return;
    }

    WindowFrameSync &frameSync = state->frameSync[state->frameIndex];
    VkCommandBuffer commandBuffer = frameSync.commandBuffer;

    // End the render pass that was opened in BeginFrame
    vkCmdEndRenderPass(commandBuffer);
    VulkanRenderState::SetCurrentRenderPass(VK_NULL_HANDLE);
    VulkanRenderState::SetFramebuffer(0);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        LOG_ERROR(1, "[Vulkan] Present: failed to end command buffer");
        VulkanRenderState::SetCurrentCommandBuffer(VK_NULL_HANDLE, {});
        VulkanRenderState::SetCurrentRenderPass(VK_NULL_HANDLE);
        VulkanRenderState::SetFramebuffer(0);
        state->frameActive = false;
        return;
    }

    // Clear the global command buffer so that any late draw calls are ignored
    VulkanRenderState::SetCurrentCommandBuffer(VK_NULL_HANDLE, {});
    state->frameActive = false;

    vkResetFences(state->deviceContext->device, 1, &frameSync.inFlightFence);

    constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frameSync.imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frameSync.renderFinished;

    if (const VkResult submitResult = vkQueueSubmit(state->deviceContext->graphicsQueue, 1, &submitInfo, frameSync.inFlightFence);
        submitResult != VK_SUCCESS)
    {
        LOG_ERROR(static_cast<int>(submitResult), "[Vulkan] Present: vkQueueSubmit failed (frameIndex=", state->frameIndex,
                  ", imageIndex=", state->currentImageIndex, " framesSinceStart=", state->framesSubmitted,
                  ") — device likely lost; subsequent fence waits will also fail");
        return;
    }

    ++state->framesSubmitted;
    {
        const auto now = std::chrono::steady_clock::now();
        if (state->fpsWindowStart.time_since_epoch().count() == 0)
        {
            state->fpsWindowStart = now;
            state->fpsWindowFrames = 0;
        }
        ++state->fpsWindowFrames;
        const auto elapsedMs = std::chrono::duration_cast<std::chrono::milliseconds>(now - state->fpsWindowStart).count();
        if (elapsedMs >= 1000)
        {
            const double fps = (static_cast<double>(state->fpsWindowFrames) * 1000.0) / static_cast<double>(elapsedMs);
            LOG_DEBUG("[Vulkan] FPS=", fps, " framesSinceStart=", state->framesSubmitted);
            state->fpsWindowStart = now;
            state->fpsWindowFrames = 0;
        }
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frameSync.renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &state->swapchain;
    presentInfo.pImageIndices = &state->currentImageIndex;

    const VkResult presentResult = vkQueuePresentKHR(state->deviceContext->graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        RecreateSwapchain(*state, window, state->presentMode == VK_PRESENT_MODE_FIFO_KHR);
    }
    else if (presentResult != VK_SUCCESS)
    {
        LOG_ERROR(static_cast<int>(presentResult), "[Vulkan] Present: failed to present swapchain image");
    }
    else
    {
        state->frameIndex = (state->frameIndex + 1) % MaxFramesInFlight;
    }
}

void Shutdown(GLFWwindow *window) noexcept
{
    const auto it = windowContexts.find(window);
    if (it == windowContexts.end())
    {
        return;
    }

    DestroyWindowContext(it->second);
    windowContexts.erase(it);
}

VkRenderPass GetSwapchainRenderPass(GLFWwindow *window) noexcept
{
    WindowContextState *state = FindWindowContext(window);
    return state == nullptr ? VK_NULL_HANDLE : state->renderPass;
}

VkRenderPass GetAnySwapchainRenderPass() noexcept
{
    for (auto &entry : windowContexts)
    {
        if (entry.second.renderPass != VK_NULL_HANDLE)
        {
            return entry.second.renderPass;
        }
    }
    return VK_NULL_HANDLE;
}

void ResumeSwapchainRenderPass() noexcept
{
    const WindowContextState *state = FindActiveWindowContext();
    if (state == nullptr || state->renderPassLoad == VK_NULL_HANDLE)
    {
        return;
    }

    VkCommandBuffer cmd = VulkanRenderState::GetCurrentCommandBuffer();
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    VkRenderPassBeginInfo rpBegin{};
    rpBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    rpBegin.renderPass = state->renderPassLoad;
    rpBegin.framebuffer = state->framebuffers[state->currentImageIndex];
    rpBegin.renderArea.offset = {0, 0};
    rpBegin.renderArea.extent = state->extent;
    rpBegin.clearValueCount = 0;
    rpBegin.pClearValues = nullptr;

    vkCmdBeginRenderPass(cmd, &rpBegin, VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{};
    viewport.x = 0.0f;
    viewport.y = static_cast<float>(state->extent.height);
    viewport.width = static_cast<float>(state->extent.width);
    viewport.height = -static_cast<float>(state->extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{};
    scissor.offset = {0, 0};
    scissor.extent = state->extent;
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VulkanRenderState::SetFramebuffer(0);
    VulkanRenderState::SetCurrentRenderPass(state->renderPassLoad);
}

VkExtent2D GetActiveSwapchainExtent() noexcept
{
    const WindowContextState *state = FindActiveWindowContext();
    return state != nullptr ? state->extent : VkExtent2D{0, 0};
}

} // namespace VulkanWindowContext
