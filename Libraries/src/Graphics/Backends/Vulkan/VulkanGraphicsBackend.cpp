#include "Graphics/Backends/Vulkan/VulkanGraphicsBackend.h"

#include <GLFW/glfw3.h>

#include "Graphics/Backends/Vulkan/VulkanGraphicsDevice.h"
#include "Logger.h"

GraphicsAPI VulkanGraphicsBackend::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

bool VulkanGraphicsBackend::Initialize()
{
    if (initialized)
    {
        return true;
    }

    if (!glfwInit())
    {
        LOG_ERROR(1, "Failed to initialize GLFW for Vulkan backend bootstrap");
        return false;
    }

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    initialized = true;
    return true;
}

void VulkanGraphicsBackend::Shutdown() noexcept
{
    if (!initialized)
    {
        return;
    }

    glfwTerminate();
    initialized = false;
}

bool VulkanGraphicsBackend::IsAvailable() const noexcept { return false; }

std::string VulkanGraphicsBackend::DescribeAvailability() const
{
    return "Vulkan backend foundation exists, but window presentation and renderer execution are not implemented yet.";
}

const GraphicsCapabilities &VulkanGraphicsBackend::GetCapabilities() const noexcept { return capabilities; }

std::unique_ptr<IGraphicsDevice> VulkanGraphicsBackend::CreateDevice(const GraphicsDeviceCreateInfo &createInfo) const
{
    static_cast<void>(createInfo);
    return std::make_unique<VulkanGraphicsDevice>(capabilities);
}
