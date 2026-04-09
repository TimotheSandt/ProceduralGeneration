#include "Graphics/Backends/Metal/MetalGraphicsBackend.h"

#include "Logger.h"

GraphicsAPI MetalGraphicsBackend::GetAPI() const noexcept { return GraphicsAPI::Metal; }

bool MetalGraphicsBackend::Initialize()
{
    LOG_ERROR(1, DescribeAvailability());
    return false;
}

void MetalGraphicsBackend::Shutdown() noexcept {}

bool MetalGraphicsBackend::IsAvailable() const noexcept
{
#ifdef __APPLE__
    return false;
#else
    return false;
#endif
}

std::string MetalGraphicsBackend::DescribeAvailability() const
{
#ifdef __APPLE__
    return "Metal backend selection is recognized, but this backend is not implemented yet.";
#else
    return "Metal backend is not supported on this platform.";
#endif
}

const GraphicsCapabilities &MetalGraphicsBackend::GetCapabilities() const noexcept { return capabilities; }

std::unique_ptr<IGraphicsDevice> MetalGraphicsBackend::CreateDevice(const GraphicsDeviceCreateInfo &createInfo) const
{
    static_cast<void>(createInfo);
    return nullptr;
}
