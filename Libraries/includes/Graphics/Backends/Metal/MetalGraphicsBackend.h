#pragma once

#include "Graphics/Core/GraphicsBackend.h"

class MetalGraphicsBackend final : public IGraphicsBackend
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
    GraphicsCapabilities capabilities = {.api = GraphicsAPI::Metal,
                                         .supportsRuntimeShaderCompilation = false,
                                         .supportsComputeShaders = true,
                                         .supportsGeometryShaders = false,
                                         .supportsTessellationShaders = true,
                                         .supportsFramebufferBlit = true,
                                         .supportsWireframeRendering = false,
                                         .supportsWindowPresentation = true,
                                         .supportsRayTracingPipelines = false,
                                         .supportsAccelerationStructures = false,
                                         .supportsRayQueries = false,
                                         .maxColorAttachments = 8};
};
