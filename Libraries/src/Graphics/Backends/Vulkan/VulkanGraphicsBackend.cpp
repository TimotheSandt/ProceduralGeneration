#include "Graphics/Backends/Vulkan/VulkanGraphicsBackend.h"

#include "Logger.h"

GraphicsAPI VulkanGraphicsBackend::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

bool VulkanGraphicsBackend::Initialize()
{
    LOG_ERROR(1, DescribeAvailability());
    return false;
}

void VulkanGraphicsBackend::Shutdown() noexcept {}

bool VulkanGraphicsBackend::IsAvailable() const noexcept { return false; }

std::string VulkanGraphicsBackend::DescribeAvailability() const
{
    return "Vulkan backend selection is recognized, but this backend is not implemented yet.";
}
