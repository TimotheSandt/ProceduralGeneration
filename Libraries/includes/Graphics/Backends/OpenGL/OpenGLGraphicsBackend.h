#pragma once

#include "Graphics/Core/GraphicsBackend.h"

class OpenGLGraphicsBackend final : public IGraphicsBackend
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
    void SetupErrorHandling() const;

  private:
    bool initialized = false;
    int majorVersion = 4;
    int minorVersion = 3;
    GraphicsCapabilities capabilities = {.api = GraphicsAPI::OpenGL,
                                         .supportsRuntimeShaderCompilation = true,
                                         .supportsComputeShaders = true,
                                         .supportsGeometryShaders = true,
                                         .supportsTessellationShaders = true,
                                         .supportsFramebufferBlit = true,
                                         .supportsWireframeRendering = true,
                                         .supportsWindowPresentation = true,
                                         .maxColorAttachments = 8};
};
