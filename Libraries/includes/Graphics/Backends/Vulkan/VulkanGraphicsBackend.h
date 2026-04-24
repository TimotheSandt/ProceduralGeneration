#pragma once

#include "Graphics/Core/GraphicsBackend.h"

#include <memory>

struct VulkanBackendContext;

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
    bool CreateInstance();
    bool SelectPhysicalDevice();

    GraphicsCapabilities capabilities = {.api = GraphicsAPI::Vulkan,
                                         .supportsRuntimeShaderCompilation = false,
                                         .supportsComputeShaders = true,
                                         .supportsGeometryShaders = true,
                                         .supportsTessellationShaders = true,
                                         .supportsFramebufferBlit = true,
                                         .supportsWireframeRendering = true,
                                         .supportsWindowPresentation = true,
                                         .supportsRayTracingPipelines = true,
                                         .supportsAccelerationStructures = true,
                                         .supportsRayQueries = true,
                                         .supportsTemporalUpscaling = true,
                                         .supportsFrameGeneration = true,
                                         .maxColorAttachments = 8,
                                         .maxAccelerationStructureInstances = 1024};
    std::shared_ptr<VulkanBackendContext> context;
    bool initialized = false;
};
