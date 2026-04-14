#include "Graphics/Backends/Vulkan/VulkanGraphicsDevice.h"

#include "Graphics/Backends/Vulkan/VulkanGraphicsResources.h"

namespace
{

constexpr ShaderStageMask BaseVulkanStages = ShaderStageBit(ShaderStage::Vertex) | ShaderStageBit(ShaderStage::Fragment) |
                                             ShaderStageBit(ShaderStage::Geometry) | ShaderStageBit(ShaderStage::TessellationControl) |
                                             ShaderStageBit(ShaderStage::TessellationEvaluation) | ShaderStageBit(ShaderStage::Compute);

constexpr ShaderStageMask RayTracingStages =
    ShaderStageBit(ShaderStage::RayGeneration) | ShaderStageBit(ShaderStage::AnyHit) | ShaderStageBit(ShaderStage::ClosestHit) |
    ShaderStageBit(ShaderStage::Miss) | ShaderStageBit(ShaderStage::Intersection) | ShaderStageBit(ShaderStage::Callable);

} // namespace

VulkanGraphicsDevice::VulkanGraphicsDevice(GraphicsCapabilities capabilitiesIn) : capabilities(capabilitiesIn) {}

GraphicsAPI VulkanGraphicsDevice::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanGraphicsDevice::GetDeviceName() const noexcept { return "Vulkan Device (Foundation)"; }

const GraphicsCapabilities &VulkanGraphicsDevice::GetCapabilities() const noexcept { return capabilities; }

bool VulkanGraphicsDevice::SupportsShaderStages(ShaderStageMask stages) const noexcept
{
    return (stages & GetSupportedStages()) == stages;
}

std::unique_ptr<IBufferResource> VulkanGraphicsDevice::CreateBuffer(const BufferCreateInfo &createInfo) const
{
    return std::make_unique<VulkanBufferResource>(createInfo);
}

std::unique_ptr<IGeometryResource> VulkanGraphicsDevice::CreateGeometry(const GeometryCreateInfo &createInfo) const
{
    return std::make_unique<VulkanGeometryResource>(createInfo);
}

std::unique_ptr<IShaderProgramResource> VulkanGraphicsDevice::CreateShaderProgram(const ShaderProgramCreateInfo &createInfo) const
{
    if (!SupportsShaderStages(createInfo.desc.stages))
    {
        return nullptr;
    }

    return std::make_unique<VulkanShaderProgramResource>(createInfo);
}

std::unique_ptr<ITextureResource> VulkanGraphicsDevice::CreateTexture(const TextureCreateInfo &createInfo) const
{
    return std::make_unique<VulkanTextureResource>(createInfo);
}

std::unique_ptr<IRenderTargetResource> VulkanGraphicsDevice::CreateRenderTarget(const RenderTargetCreateInfo &createInfo) const
{
    return std::make_unique<VulkanRenderTargetResource>(createInfo);
}

std::unique_ptr<IAccelerationStructureResource> VulkanGraphicsDevice::CreateAccelerationStructure(
    const AccelerationStructureCreateInfo &createInfo) const
{
    if (!capabilities.supportsAccelerationStructures)
    {
        return nullptr;
    }

    return std::make_unique<VulkanAccelerationStructureResource>(createInfo);
}

std::unique_ptr<IGPUTimestampQueryResource> VulkanGraphicsDevice::CreateTimestampQuery(const GPUTimestampQueryCreateInfo &createInfo) const
{
    return std::make_unique<VulkanTimestampQueryResource>(createInfo);
}

ShaderStageMask VulkanGraphicsDevice::GetSupportedStages() const noexcept
{
    ShaderStageMask stages = BaseVulkanStages;
    if (capabilities.supportsRayTracingPipelines)
    {
        stages |= RayTracingStages;
    }
    return stages;
}
