#pragma once

#include "Graphics/Backends/Vulkan/VulkanContext.h"
#include "Graphics/Core/GraphicsDevice.h"

#include <memory>

class VulkanGraphicsDevice final : public IGraphicsDevice
{
  public:
    VulkanGraphicsDevice(std::shared_ptr<VulkanBackendContext> backendContext, GraphicsCapabilities capabilities);
    ~VulkanGraphicsDevice() override;

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
    const std::shared_ptr<VulkanDeviceContext> &GetDeviceContext() const noexcept;

  private:
    ShaderStageMask GetSupportedStages() const noexcept;
    bool CreateLogicalDevice();

    std::shared_ptr<VulkanDeviceContext> deviceContext;
    GraphicsCapabilities capabilities;
    std::string deviceName;
};
