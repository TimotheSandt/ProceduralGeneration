#pragma once

#include "Graphics/Core/GraphicsBackend.h"

class VulkanGraphicsBackend final : public IGraphicsBackend
{
  public:
    GraphicsAPI GetAPI() const noexcept override;
    bool Initialize() override;
    void Shutdown() noexcept override;
    bool IsAvailable() const noexcept override;
    std::string DescribeAvailability() const override;
    const GraphicsCapabilities &GetCapabilities() const noexcept override;
    std::unique_ptr<IGraphicsDevice> CreateDevice(const GraphicsDeviceCreateInfo &createInfo) const override;

  private:
    GraphicsCapabilities capabilities = {.api = GraphicsAPI::Vulkan,
                                         .supportsRuntimeShaderCompilation = false,
                                         .supportsComputeShaders = true,
                                         .supportsGeometryShaders = true,
                                         .supportsTessellationShaders = true,
                                         .supportsFramebufferBlit = true,
                                         .supportsWireframeRendering = true,
                                         .supportsWindowPresentation = true,
                                         .maxColorAttachments = 8};
};
