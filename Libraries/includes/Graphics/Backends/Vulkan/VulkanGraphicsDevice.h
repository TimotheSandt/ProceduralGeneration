#pragma once

#include "Graphics/Core/GraphicsDevice.h"

class VulkanGraphicsDevice final : public IGraphicsDevice
{
  public:
    explicit VulkanGraphicsDevice(GraphicsCapabilities capabilities);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDeviceName() const noexcept override;
    const GraphicsCapabilities &GetCapabilities() const noexcept override;
    bool SupportsShaderStages(ShaderStageMask stages) const noexcept override;
    std::unique_ptr<IBufferResource> CreateBuffer(const BufferCreateInfo &createInfo) const override;
    std::unique_ptr<IGeometryResource> CreateGeometry(const GeometryCreateInfo &createInfo) const override;
    std::unique_ptr<IShaderProgramResource> CreateShaderProgram(const ShaderProgramCreateInfo &createInfo) const override;
    std::unique_ptr<ITextureResource> CreateTexture(const TextureCreateInfo &createInfo) const override;
    std::unique_ptr<IRenderTargetResource> CreateRenderTarget(const RenderTargetCreateInfo &createInfo) const override;
    std::unique_ptr<IAccelerationStructureResource> CreateAccelerationStructure(
        const AccelerationStructureCreateInfo &createInfo) const override;
    std::unique_ptr<IGPUTimestampQueryResource> CreateTimestampQuery(const GPUTimestampQueryCreateInfo &createInfo) const override;

  private:
    ShaderStageMask GetSupportedStages() const noexcept;

    GraphicsCapabilities capabilities;
};
