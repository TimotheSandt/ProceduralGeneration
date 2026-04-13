#include "Graphics/Core/GraphicsBackend.h"

#include "Graphics/Backends/Metal/MetalGraphicsBackend.h"
#include "Graphics/Backends/OpenGL/OpenGLGraphicsBackend.h"
#include "Graphics/Backends/Vulkan/VulkanGraphicsBackend.h"

std::unique_ptr<IGraphicsBackend> CreateGraphicsBackend(const GraphicsBackendCreateInfo &createInfo)
{
    switch (createInfo.api)
    {
        case GraphicsAPI::OpenGL:
            return std::make_unique<OpenGLGraphicsBackend>();
        case GraphicsAPI::Vulkan:
            return std::make_unique<VulkanGraphicsBackend>();
        case GraphicsAPI::Metal:
            return std::make_unique<MetalGraphicsBackend>();
    }

    return nullptr;
}
