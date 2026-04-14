#pragma once

#include "Graphics/Core/GraphicsTypes.h"

#include <memory>
#include <string>
#include <vector>

#include <vulkan/vulkan.h>

struct VulkanBackendContext
{
    VkInstance instance = VK_NULL_HANDLE;
    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    VkPhysicalDeviceProperties physicalDeviceProperties{};
    VkPhysicalDeviceMemoryProperties memoryProperties{};
    std::uint32_t graphicsQueueFamilyIndex = UINT32_MAX;
    std::vector<const char *> enabledInstanceExtensions;
    std::vector<const char *> enabledDeviceExtensions;
    GraphicsCapabilities capabilities{};
    std::string deviceName;
};

struct VulkanDeviceContext
{
    std::shared_ptr<VulkanBackendContext> backend;
    VkDevice device = VK_NULL_HANDLE;
    VkQueue graphicsQueue = VK_NULL_HANDLE;
};
