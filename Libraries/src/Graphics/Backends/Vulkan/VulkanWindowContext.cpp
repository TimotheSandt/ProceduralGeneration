#include "Graphics/Backends/Vulkan/VulkanWindowContext.h"

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <limits>
#include <memory>
#include <unordered_map>
#include <vector>

#include "Graphics/Backends/Vulkan/VulkanGraphicsDevice.h"
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
    std::vector<VkImageLayout> imageLayouts;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    std::vector<VkCommandBuffer> commandBuffers;
    std::array<WindowFrameSync, MaxFramesInFlight> frameSync{};
    std::uint32_t frameIndex = 0;
};

std::unordered_map<GLFWwindow *, WindowContextState> windowContexts;

void DestroySwapchainResources(WindowContextState &state)
{
    if (state.deviceContext == nullptr || state.deviceContext->device == VK_NULL_HANDLE)
    {
        state.images.clear();
        state.imageViews.clear();
        state.imageLayouts.clear();
        state.commandBuffers.clear();
        state.swapchain = VK_NULL_HANDLE;
        return;
    }

    if (!state.commandBuffers.empty() && state.commandPool != VK_NULL_HANDLE)
    {
        vkFreeCommandBuffers(state.deviceContext->device, state.commandPool, static_cast<std::uint32_t>(state.commandBuffers.size()),
                             state.commandBuffers.data());
    }
    state.commandBuffers.clear();

    for (VkImageView imageView : state.imageViews)
    {
        if (imageView != VK_NULL_HANDLE)
        {
            vkDestroyImageView(state.deviceContext->device, imageView, nullptr);
        }
    }
    state.imageViews.clear();
    state.images.clear();
    state.imageLayouts.clear();

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
        vkGetPhysicalDeviceSurfacePresentModesKHR(state.deviceContext->backend->physicalDevice, state.surface, &presentModeCount, nullptr) !=
            VK_SUCCESS ||
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
    state.imageLayouts.assign(swapchainImageCount, VK_IMAGE_LAYOUT_UNDEFINED);

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

    state.commandBuffers.resize(swapchainImageCount, VK_NULL_HANDLE);
    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = state.commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = static_cast<std::uint32_t>(state.commandBuffers.size());

    if (vkAllocateCommandBuffers(state.deviceContext->device, &allocateInfo, state.commandBuffers.data()) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to allocate Vulkan command buffers");
        return false;
    }

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

bool RecordPresentCommand(WindowContextState &state, std::uint32_t imageIndex)
{
    VkCommandBuffer commandBuffer = state.commandBuffers[imageIndex];
    vkResetCommandBuffer(commandBuffer, 0);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to begin Vulkan present command buffer");
        return false;
    }

    if (state.imageLayouts[imageIndex] != VK_IMAGE_LAYOUT_PRESENT_SRC_KHR)
    {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = state.imageLayouts[imageIndex];
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = state.images[imageIndex];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;

        vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1,
                             &barrier);
    }

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to finalize Vulkan present command buffer");
        return false;
    }

    return true;
}

WindowContextState *FindWindowContext(GLFWwindow *window) noexcept
{
    const auto it = windowContexts.find(window);
    return it == windowContexts.end() ? nullptr : &it->second;
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

void ApplyDefaultFramebufferState(GLFWwindow *window, bool enableVsync) noexcept
{
    WindowContextState *state = FindWindowContext(window);
    if (state == nullptr)
    {
        return;
    }

    RecreateSwapchain(*state, window, enableVsync);
    VulkanRenderState::SetDepthTest(true);
}

void Present(GLFWwindow *window) noexcept
{
    WindowContextState *state = FindWindowContext(window);
    if (state == nullptr || state->swapchain == VK_NULL_HANDLE)
    {
        return;
    }

    WindowFrameSync &frameSync = state->frameSync[state->frameIndex];
    if (vkWaitForFences(state->deviceContext->device, 1, &frameSync.inFlightFence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to wait for Vulkan frame fence");
        return;
    }

    std::uint32_t imageIndex = 0;
    VkResult acquireResult =
        vkAcquireNextImageKHR(state->deviceContext->device, state->swapchain, UINT64_MAX, frameSync.imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (acquireResult == VK_ERROR_OUT_OF_DATE_KHR)
    {
        ApplyDefaultFramebufferState(window, state->presentMode == VK_PRESENT_MODE_FIFO_KHR);
        return;
    }
    if (acquireResult != VK_SUCCESS && acquireResult != VK_SUBOPTIMAL_KHR)
    {
        LOG_ERROR(static_cast<int>(acquireResult), "Failed to acquire Vulkan swapchain image");
        return;
    }

    if (!RecordPresentCommand(*state, imageIndex))
    {
        return;
    }

    vkResetFences(state->deviceContext->device, 1, &frameSync.inFlightFence);

    constexpr VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &frameSync.imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &state->commandBuffers[imageIndex];
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &frameSync.renderFinished;

    if (vkQueueSubmit(state->deviceContext->graphicsQueue, 1, &submitInfo, frameSync.inFlightFence) != VK_SUCCESS)
    {
        LOG_ERROR(1, "Failed to submit Vulkan present command buffer");
        return;
    }

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &frameSync.renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &state->swapchain;
    presentInfo.pImageIndices = &imageIndex;

    const VkResult presentResult = vkQueuePresentKHR(state->deviceContext->graphicsQueue, &presentInfo);
    if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR)
    {
        ApplyDefaultFramebufferState(window, state->presentMode == VK_PRESENT_MODE_FIFO_KHR);
    }
    else if (presentResult != VK_SUCCESS)
    {
        LOG_ERROR(static_cast<int>(presentResult), "Failed to present Vulkan swapchain image");
    }
    else
    {
        state->imageLayouts[imageIndex] = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
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

} // namespace VulkanWindowContext
