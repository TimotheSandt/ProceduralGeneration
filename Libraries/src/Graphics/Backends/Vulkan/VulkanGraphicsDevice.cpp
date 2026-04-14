#include "Graphics/Backends/Vulkan/VulkanGraphicsDevice.h"

#include <vulkan/vulkan.h>

#include "Graphics/Backends/Vulkan/VulkanGraphicsResources.h"
#include "Logger.h"

namespace
{

constexpr ShaderStageMask BaseVulkanStages = ShaderStageBit(ShaderStage::Vertex) | ShaderStageBit(ShaderStage::Fragment) |
                                             ShaderStageBit(ShaderStage::Geometry) | ShaderStageBit(ShaderStage::TessellationControl) |
                                             ShaderStageBit(ShaderStage::TessellationEvaluation) | ShaderStageBit(ShaderStage::Compute);

constexpr ShaderStageMask RayTracingStages =
    ShaderStageBit(ShaderStage::RayGeneration) | ShaderStageBit(ShaderStage::AnyHit) | ShaderStageBit(ShaderStage::ClosestHit) |
    ShaderStageBit(ShaderStage::Miss) | ShaderStageBit(ShaderStage::Intersection) | ShaderStageBit(ShaderStage::Callable);

} // namespace

VulkanGraphicsDevice::VulkanGraphicsDevice(std::shared_ptr<VulkanBackendContext> backendContext, GraphicsCapabilities capabilitiesIn)
    : deviceContext(std::make_shared<VulkanDeviceContext>()), capabilities(capabilitiesIn)
{
    deviceContext->backend = std::move(backendContext);
    deviceName = deviceContext->backend != nullptr ? deviceContext->backend->deviceName : "Vulkan Device";
    if (!CreateLogicalDevice())
    {
        deviceContext.reset();
    }
}

VulkanGraphicsDevice::~VulkanGraphicsDevice()
{
    if (deviceContext != nullptr && deviceContext->device != VK_NULL_HANDLE)
    {
        vkDestroyDevice(deviceContext->device, nullptr);
        deviceContext->device = VK_NULL_HANDLE;
        deviceContext->graphicsQueue = VK_NULL_HANDLE;
    }
}

GraphicsAPI VulkanGraphicsDevice::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanGraphicsDevice::GetDeviceName() const noexcept { return deviceName; }

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

bool VulkanGraphicsDevice::CreateLogicalDevice()
{
    if (deviceContext == nullptr || deviceContext->backend == nullptr || deviceContext->backend->physicalDevice == VK_NULL_HANDLE)
    {
        return false;
    }

    const float queuePriority = 1.0f;
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.queueFamilyIndex = deviceContext->backend->graphicsQueueFamilyIndex;
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &queuePriority;

    VkPhysicalDeviceFeatures2 features2{};
    features2.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
    features2.features.fillModeNonSolid = capabilities.supportsWireframeRendering ? VK_TRUE : VK_FALSE;
    features2.features.geometryShader = capabilities.supportsGeometryShaders ? VK_TRUE : VK_FALSE;
    features2.features.tessellationShader = capabilities.supportsTessellationShaders ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceBufferDeviceAddressFeatures bufferDeviceAddressFeatures{};
    bufferDeviceAddressFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_BUFFER_DEVICE_ADDRESS_FEATURES;
    bufferDeviceAddressFeatures.bufferDeviceAddress = capabilities.supportsAccelerationStructures ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{};
    accelerationStructureFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR;
    accelerationStructureFeatures.accelerationStructure = capabilities.supportsAccelerationStructures ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{};
    rayTracingPipelineFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR;
    rayTracingPipelineFeatures.rayTracingPipeline = capabilities.supportsRayTracingPipelines ? VK_TRUE : VK_FALSE;

    VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{};
    rayQueryFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR;
    rayQueryFeatures.rayQuery = capabilities.supportsRayQueries ? VK_TRUE : VK_FALSE;

    features2.pNext = &bufferDeviceAddressFeatures;
    bufferDeviceAddressFeatures.pNext = &accelerationStructureFeatures;
    accelerationStructureFeatures.pNext = &rayTracingPipelineFeatures;
    rayTracingPipelineFeatures.pNext = &rayQueryFeatures;

    VkDeviceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    createInfo.queueCreateInfoCount = 1;
    createInfo.pQueueCreateInfos = &queueCreateInfo;
    createInfo.enabledExtensionCount = static_cast<std::uint32_t>(deviceContext->backend->enabledDeviceExtensions.size());
    createInfo.ppEnabledExtensionNames = deviceContext->backend->enabledDeviceExtensions.data();
    createInfo.pNext = &features2;

    const VkResult result = vkCreateDevice(deviceContext->backend->physicalDevice, &createInfo, nullptr, &deviceContext->device);
    if (result != VK_SUCCESS)
    {
        LOG_ERROR(static_cast<int>(result), "Failed to create Vulkan logical device");
        return false;
    }

    vkGetDeviceQueue(deviceContext->device, deviceContext->backend->graphicsQueueFamilyIndex, 0, &deviceContext->graphicsQueue);
    return deviceContext->graphicsQueue != VK_NULL_HANDLE;
}
