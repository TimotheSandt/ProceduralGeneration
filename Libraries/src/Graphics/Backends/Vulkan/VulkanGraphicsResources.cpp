#include "Graphics/Backends/Vulkan/VulkanGraphicsResources.h"

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>

namespace
{

std::vector<std::byte> CopyBytes(const void *data, std::size_t size)
{
    std::vector<std::byte> output(size);
    if (data != nullptr && size > 0)
    {
        std::memcpy(output.data(), data, size);
    }
    return output;
}

VkBufferUsageFlags BufferUsageToVulkan(BufferUsage usage) noexcept
{
    switch (usage)
    {
        case BufferUsage::Vertex:
            return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        case BufferUsage::Index:
            return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        case BufferUsage::Uniform:
            return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
        case BufferUsage::Storage:
            return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
        default:
            return VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT;
    }
}

std::uint32_t FindMemoryType(const VulkanBackendContext &backendContext, std::uint32_t typeFilter, VkMemoryPropertyFlags properties)
{
    for (std::uint32_t i = 0; i < backendContext.memoryProperties.memoryTypeCount; ++i)
    {
        const bool matchesType = (typeFilter & (1u << i)) != 0;
        const bool matchesProperties =
            (backendContext.memoryProperties.memoryTypes[i].propertyFlags & properties) == properties;
        if (matchesType && matchesProperties)
        {
            return i;
        }
    }

    return UINT32_MAX;
}

bool CreateBufferHandle(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkBufferUsageFlags usage, VkDeviceSize size,
                        VkBuffer &bufferOut, VkDeviceMemory &memoryOut)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || deviceContext->backend == nullptr)
    {
        return false;
    }

    VkBufferCreateInfo bufferCreateInfo{};
    bufferCreateInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferCreateInfo.size = std::max<VkDeviceSize>(size, 1);
    bufferCreateInfo.usage = usage;
    bufferCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateBuffer(deviceContext->device, &bufferCreateInfo, nullptr, &bufferOut) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetBufferMemoryRequirements(deviceContext->device, bufferOut, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex =
        FindMemoryType(*deviceContext->backend, memoryRequirements.memoryTypeBits,
                       VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (allocateInfo.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(deviceContext->device, &allocateInfo, nullptr, &memoryOut) != VK_SUCCESS)
    {
        vkDestroyBuffer(deviceContext->device, bufferOut, nullptr);
        bufferOut = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindBufferMemory(deviceContext->device, bufferOut, memoryOut, 0) != VK_SUCCESS)
    {
        vkDestroyBuffer(deviceContext->device, bufferOut, nullptr);
        vkFreeMemory(deviceContext->device, memoryOut, nullptr);
        bufferOut = VK_NULL_HANDLE;
        memoryOut = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void DestroyBufferHandle(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkBuffer &buffer, VkDeviceMemory &memory)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        buffer = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        return;
    }

    if (buffer != VK_NULL_HANDLE)
    {
        vkDestroyBuffer(deviceContext->device, buffer, nullptr);
    }
    if (memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(deviceContext->device, memory, nullptr);
    }
    buffer = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
}

VkFormat ToVulkanFormat(TextureFormat format) noexcept
{
    switch (format)
    {
        case TextureFormat::BGRA8:
            return VK_FORMAT_B8G8R8A8_UNORM;
        case TextureFormat::Depth24Stencil8:
            return VK_FORMAT_D24_UNORM_S8_UINT;
        case TextureFormat::Depth32Float:
            return VK_FORMAT_D32_SFLOAT;
        case TextureFormat::R8:
            return VK_FORMAT_R8_UNORM;
        case TextureFormat::R32UI:
            return VK_FORMAT_R32_UINT;
        case TextureFormat::RG16F:
            return VK_FORMAT_R16G16_SFLOAT;
        case TextureFormat::RGBA8:
        default:
            return VK_FORMAT_R8G8B8A8_UNORM;
    }
}

VkImageAspectFlags ToAspectMask(TextureFormat format) noexcept
{
    switch (format)
    {
        case TextureFormat::Depth24Stencil8:
            return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
        case TextureFormat::Depth32Float:
            return VK_IMAGE_ASPECT_DEPTH_BIT;
        default:
            return VK_IMAGE_ASPECT_COLOR_BIT;
    }
}

VkImageUsageFlags ToImageUsageFlags(const TextureDesc &desc) noexcept
{
    VkImageUsageFlags usage = VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    if (desc.renderTarget)
    {
        usage |= (ToAspectMask(desc.format) & VK_IMAGE_ASPECT_COLOR_BIT) != 0 ? VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT
                                                                               : VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    }
    return usage;
}

bool CreateImageHandle(const std::shared_ptr<VulkanDeviceContext> &deviceContext, const TextureDesc &desc, VkImage &imageOut,
                       VkDeviceMemory &memoryOut)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || deviceContext->backend == nullptr ||
        desc.extent.width == 0 || desc.extent.height == 0)
    {
        return false;
    }

    VkImageCreateInfo imageCreateInfo{};
    imageCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageCreateInfo.imageType = VK_IMAGE_TYPE_2D;
    imageCreateInfo.extent.width = desc.extent.width;
    imageCreateInfo.extent.height = desc.extent.height;
    imageCreateInfo.extent.depth = 1;
    imageCreateInfo.mipLevels = std::max(1u, desc.mipLevels);
    imageCreateInfo.arrayLayers = 1;
    imageCreateInfo.format = ToVulkanFormat(desc.format);
    imageCreateInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageCreateInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imageCreateInfo.usage = ToImageUsageFlags(desc);
    imageCreateInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageCreateInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    if (vkCreateImage(deviceContext->device, &imageCreateInfo, nullptr, &imageOut) != VK_SUCCESS)
    {
        return false;
    }

    VkMemoryRequirements memoryRequirements{};
    vkGetImageMemoryRequirements(deviceContext->device, imageOut, &memoryRequirements);

    VkMemoryAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocateInfo.allocationSize = memoryRequirements.size;
    allocateInfo.memoryTypeIndex =
        FindMemoryType(*deviceContext->backend, memoryRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (allocateInfo.memoryTypeIndex == UINT32_MAX ||
        vkAllocateMemory(deviceContext->device, &allocateInfo, nullptr, &memoryOut) != VK_SUCCESS)
    {
        vkDestroyImage(deviceContext->device, imageOut, nullptr);
        imageOut = VK_NULL_HANDLE;
        return false;
    }

    if (vkBindImageMemory(deviceContext->device, imageOut, memoryOut, 0) != VK_SUCCESS)
    {
        vkDestroyImage(deviceContext->device, imageOut, nullptr);
        vkFreeMemory(deviceContext->device, memoryOut, nullptr);
        imageOut = VK_NULL_HANDLE;
        memoryOut = VK_NULL_HANDLE;
        return false;
    }

    return true;
}

void DestroyImageHandle(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkImage &image, VkDeviceMemory &memory, VkImageView &imageView,
                        VkSampler &sampler)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        image = VK_NULL_HANDLE;
        memory = VK_NULL_HANDLE;
        imageView = VK_NULL_HANDLE;
        sampler = VK_NULL_HANDLE;
        return;
    }

    if (sampler != VK_NULL_HANDLE)
    {
        vkDestroySampler(deviceContext->device, sampler, nullptr);
    }
    if (imageView != VK_NULL_HANDLE)
    {
        vkDestroyImageView(deviceContext->device, imageView, nullptr);
    }
    if (image != VK_NULL_HANDLE)
    {
        vkDestroyImage(deviceContext->device, image, nullptr);
    }
    if (memory != VK_NULL_HANDLE)
    {
        vkFreeMemory(deviceContext->device, memory, nullptr);
    }

    image = VK_NULL_HANDLE;
    memory = VK_NULL_HANDLE;
    imageView = VK_NULL_HANDLE;
    sampler = VK_NULL_HANDLE;
}

} // namespace

VulkanShaderProgramResource::VulkanShaderProgramResource(ShaderProgramCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName)), stageSources(std::move(createInfo.stageSources))
{
}

GraphicsAPI VulkanShaderProgramResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanShaderProgramResource::GetDebugName() const noexcept { return debugName; }

const ShaderProgramDesc &VulkanShaderProgramResource::GetDescription() const noexcept { return desc; }

void VulkanShaderProgramResource::Bind() const {}

void VulkanShaderProgramResource::Unbind() const {}

int VulkanShaderProgramResource::GetUniformLocation(std::string_view name) const
{
    const std::string key(name);
    const auto it = uniformLocations.find(key);
    if (it != uniformLocations.end())
    {
        return it->second;
    }

    const int location = nextUniformLocation++;
    uniformLocations.emplace(key, location);
    return location;
}

void VulkanShaderProgramResource::SetFloatUniform(int location, const float *data, std::size_t componentCount) const
{
    if (location < 0 || data == nullptr)
    {
        return;
    }

    uniformData[location] = CopyBytes(data, componentCount * sizeof(float));
}

void VulkanShaderProgramResource::SetIntUniform(int location, const int *data, std::size_t componentCount) const
{
    if (location < 0 || data == nullptr)
    {
        return;
    }

    uniformData[location] = CopyBytes(data, componentCount * sizeof(int));
}

void VulkanShaderProgramResource::SetMatrix4Uniform(int location, const float *data) const
{
    if (location < 0 || data == nullptr)
    {
        return;
    }

    uniformData[location] = CopyBytes(data, 16 * sizeof(float));
}

bool VulkanShaderProgramResource::GetBinary(std::vector<std::byte> &dataOut, std::uint32_t &formatOut) const
{
    if (cachedBinary.empty())
    {
        return false;
    }

    dataOut = cachedBinary;
    formatOut = cachedBinaryFormat;
    return true;
}

bool VulkanShaderProgramResource::LoadBinary(const std::vector<std::byte> &data, std::uint32_t format)
{
    cachedBinary = data;
    cachedBinaryFormat = format;
    return !cachedBinary.empty();
}

VulkanBufferResource::VulkanBufferResource(std::shared_ptr<VulkanDeviceContext> deviceContextIn, BufferCreateInfo createInfo)
    : deviceContext(std::move(deviceContextIn)), desc(createInfo.desc), debugName(std::move(createInfo.debugName)),
      storage(std::move(createInfo.initialData))
{
    storage.resize(std::max(desc.sizeInBytes, storage.size()));
    desc.sizeInBytes = storage.size();
    CreateBuffer(desc.sizeInBytes);
    if (!storage.empty())
    {
        UploadData(storage.data(), storage.size(), 0);
    }
}

VulkanBufferResource::~VulkanBufferResource()
{
    DestroyBuffer();
}

GraphicsAPI VulkanBufferResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanBufferResource::GetDebugName() const noexcept { return debugName; }

const BufferDesc &VulkanBufferResource::GetDescription() const noexcept { return desc; }

void VulkanBufferResource::Bind() const {}

void VulkanBufferResource::BindToBindingPoint(std::uint32_t bindingPoint) const { static_cast<void>(bindingPoint); }

void VulkanBufferResource::Unbind() const {}

void VulkanBufferResource::UploadData(const void *data, std::size_t size, std::size_t offset)
{
    if (data == nullptr || size == 0)
    {
        return;
    }

    const std::size_t requiredSize = offset + size;
    if (storage.size() < requiredSize)
    {
        storage.resize(requiredSize);
    }

    std::memcpy(storage.data() + offset, data, size);
    desc.sizeInBytes = storage.size();
    UploadStorageToGPU(offset, size);
}

void VulkanBufferResource::Resize(std::size_t newSize, bool preserveData)
{
    std::vector<std::byte> resized(newSize);
    if (preserveData)
    {
        const std::size_t bytesToCopy = std::min(storage.size(), resized.size());
        std::copy_n(storage.begin(), bytesToCopy, resized.begin());
    }

    storage = std::move(resized);
    desc.sizeInBytes = storage.size();
    CreateBuffer(desc.sizeInBytes);
    if (!storage.empty())
    {
        UploadStorageToGPU(0, storage.size());
    }
}

void *VulkanBufferResource::Map(BufferMapAccess access)
{
    static_cast<void>(access);
    if (mappedData == nullptr && memory != VK_NULL_HANDLE && deviceContext != nullptr && deviceContext->device != VK_NULL_HANDLE)
    {
        vkMapMemory(deviceContext->device, memory, 0, VK_WHOLE_SIZE, 0, &mappedData);
    }
    mapped = true;
    return mappedData != nullptr ? mappedData : (storage.empty() ? nullptr : storage.data());
}

void VulkanBufferResource::Unmap()
{
    if (mappedData != nullptr && deviceContext != nullptr && deviceContext->device != VK_NULL_HANDLE)
    {
        vkUnmapMemory(deviceContext->device, memory);
        mappedData = nullptr;
    }
    mapped = false;
}

bool VulkanBufferResource::CreateBuffer(std::size_t sizeInBytes)
{
    DestroyBuffer();
    return CreateBufferHandle(deviceContext, BufferUsageToVulkan(desc.usage), sizeInBytes, buffer, memory);
}

void VulkanBufferResource::DestroyBuffer() noexcept
{
    if (mappedData != nullptr && deviceContext != nullptr && deviceContext->device != VK_NULL_HANDLE)
    {
        vkUnmapMemory(deviceContext->device, memory);
        mappedData = nullptr;
    }
    DestroyBufferHandle(deviceContext, buffer, memory);
}

void VulkanBufferResource::UploadStorageToGPU(std::size_t offset, std::size_t size) const
{
    if (memory == VK_NULL_HANDLE || deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || size == 0 || offset >= storage.size())
    {
        return;
    }

    void *gpuData = nullptr;
    if (vkMapMemory(deviceContext->device, memory, offset, size, 0, &gpuData) != VK_SUCCESS || gpuData == nullptr)
    {
        return;
    }

    std::memcpy(gpuData, storage.data() + offset, size);
    vkUnmapMemory(deviceContext->device, memory);
}

VulkanGeometryResource::VulkanGeometryResource(std::shared_ptr<VulkanDeviceContext> deviceContextIn, GeometryCreateInfo createInfo)
    : deviceContext(std::move(deviceContextIn)),
      layout(std::move(createInfo.layout)),
      vertexData(std::move(createInfo.vertexData)),
      indexData(std::move(createInfo.indexData)),
      instanceData(std::move(createInfo.instanceData)),
      debugName(std::move(createInfo.debugName))
{
    CreateOrResizeBuffers();
}

VulkanGeometryResource::~VulkanGeometryResource() { DestroyBuffers(); }

GraphicsAPI VulkanGeometryResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanGeometryResource::GetDebugName() const noexcept { return debugName; }

const GeometryLayout &VulkanGeometryResource::GetLayout() const noexcept { return layout; }

std::size_t VulkanGeometryResource::GetIndexCount() const noexcept { return indexData.size(); }

std::size_t VulkanGeometryResource::GetInstanceCount() const noexcept
{
    return instanceData.empty() ? static_cast<std::size_t>(1) : instanceData.size();
}

void VulkanGeometryResource::Bind() const {}

void VulkanGeometryResource::Unbind() const {}

void VulkanGeometryResource::UpdateVertexData(const float *data, std::size_t floatCount, std::size_t offsetFloats)
{
    if (data == nullptr || floatCount == 0)
    {
        return;
    }

    const std::size_t requiredFloats = offsetFloats + floatCount;
    if (vertexData.size() < requiredFloats)
    {
        vertexData.resize(requiredFloats);
    }

    std::copy_n(data, floatCount, vertexData.begin() + static_cast<std::ptrdiff_t>(offsetFloats));
    CreateOrResizeBuffers();
}

void VulkanGeometryResource::UpdateInstanceData(const float *data, std::size_t floatCount, std::size_t offsetFloats)
{
    if (data == nullptr || floatCount == 0)
    {
        return;
    }

    const std::size_t requiredFloats = offsetFloats + floatCount;
    if (instanceData.size() < requiredFloats)
    {
        instanceData.resize(requiredFloats);
    }

    std::copy_n(data, floatCount, instanceData.begin() + static_cast<std::ptrdiff_t>(offsetFloats));
    CreateOrResizeBuffers();
}

void VulkanGeometryResource::DrawIndexed() const {}

void VulkanGeometryResource::DrawIndexedInstanced() const {}

void VulkanGeometryResource::DrawVertices(std::size_t vertexCount) const { static_cast<void>(vertexCount); }

bool VulkanGeometryResource::CreateOrResizeBuffers()
{
    DestroyBuffers();

    const std::size_t vertexBytes = vertexData.size() * sizeof(float);
    const std::size_t indexBytes = indexData.size() * sizeof(std::uint32_t);
    const std::size_t instanceBytes = instanceData.size() * sizeof(float);

    const bool vertexOk = vertexBytes == 0 ||
                          CreateBufferHandle(deviceContext, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBytes, vertexBuffer, vertexMemory);
    const bool indexOk = indexBytes == 0 ||
                         CreateBufferHandle(deviceContext, VK_BUFFER_USAGE_INDEX_BUFFER_BIT, indexBytes, indexBuffer, indexMemory);
    const bool instanceOk = instanceBytes == 0 ||
                            CreateBufferHandle(deviceContext, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, instanceBytes, instanceBuffer, instanceMemory);

    if (!(vertexOk && indexOk && instanceOk))
    {
        DestroyBuffers();
        return false;
    }

    UploadBuffer(vertexBuffer, vertexMemory, vertexData.data(), vertexBytes);
    UploadBuffer(indexBuffer, indexMemory, indexData.data(), indexBytes);
    UploadBuffer(instanceBuffer, instanceMemory, instanceData.data(), instanceBytes);
    return true;
}

void VulkanGeometryResource::DestroyBuffers() noexcept
{
    DestroyBufferHandle(deviceContext, vertexBuffer, vertexMemory);
    DestroyBufferHandle(deviceContext, indexBuffer, indexMemory);
    DestroyBufferHandle(deviceContext, instanceBuffer, instanceMemory);
}

void VulkanGeometryResource::UploadBuffer(VkBuffer bufferHandle, VkDeviceMemory memoryHandle, const void *data, std::size_t size) const
{
    static_cast<void>(bufferHandle);
    if (memoryHandle == VK_NULL_HANDLE || data == nullptr || size == 0 || deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        return;
    }

    void *gpuData = nullptr;
    if (vkMapMemory(deviceContext->device, memoryHandle, 0, size, 0, &gpuData) != VK_SUCCESS || gpuData == nullptr)
    {
        return;
    }

    std::memcpy(gpuData, data, size);
    vkUnmapMemory(deviceContext->device, memoryHandle);
}

VulkanTextureResource::VulkanTextureResource(std::shared_ptr<VulkanDeviceContext> deviceContextIn, TextureCreateInfo createInfo)
    : deviceContext(std::move(deviceContextIn)), desc(createInfo.desc), debugName(std::move(createInfo.debugName)),
      storage(std::move(createInfo.initialData))
{
    Resize(desc.extent.width, desc.extent.height);
}

VulkanTextureResource::~VulkanTextureResource() { DestroyImage(); }

GraphicsAPI VulkanTextureResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanTextureResource::GetDebugName() const noexcept { return debugName; }

const TextureDesc &VulkanTextureResource::GetDescription() const noexcept { return desc; }

void VulkanTextureResource::Bind(std::uint32_t slot) const { static_cast<void>(slot); }

void VulkanTextureResource::Unbind() const {}

void VulkanTextureResource::Readback(std::vector<std::byte> &output) const { output = storage; }

void VulkanTextureResource::Resize(std::uint32_t width, std::uint32_t height)
{
    DestroyImage();
    desc.extent = {width, height};
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    storage.resize(pixelCount * GetPixelSize(desc.format));
    CreateImage();
}

void VulkanTextureResource::AttachToFramebuffer(std::uint32_t framebufferHandle, std::uint32_t colorIndex) const
{
    if (VulkanRenderTargetResource *renderTarget = VulkanRenderTargetResource::FindByHandle(framebufferHandle); renderTarget != nullptr)
    {
        renderTarget->RegisterColorAttachment(colorIndex, this);
    }
}

void VulkanTextureResource::AttachAsDepthToFramebuffer(std::uint32_t framebufferHandle) const
{
    if (VulkanRenderTargetResource *renderTarget = VulkanRenderTargetResource::FindByHandle(framebufferHandle); renderTarget != nullptr)
    {
        renderTarget->RegisterDepthAttachment(this);
    }
}

std::size_t VulkanTextureResource::GetPixelSize(TextureFormat format) noexcept
{
    switch (format)
    {
        case TextureFormat::R8:
            return 1;
        case TextureFormat::RG16F:
            return 4;
        case TextureFormat::R32UI:
        case TextureFormat::Depth32Float:
            return 4;
        case TextureFormat::Depth24Stencil8:
        case TextureFormat::RGBA8:
        case TextureFormat::BGRA8:
        default:
            return 4;
    }
}

VkFormat VulkanTextureResource::ToVulkanFormat(TextureFormat format) noexcept { return ::ToVulkanFormat(format); }

VkImageAspectFlags VulkanTextureResource::ToAspectMask(TextureFormat format) noexcept { return ::ToAspectMask(format); }

bool VulkanTextureResource::CreateImage()
{
    if (!CreateImageHandle(deviceContext, desc, image, memory))
    {
        return false;
    }

    VkImageViewCreateInfo imageViewCreateInfo{};
    imageViewCreateInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    imageViewCreateInfo.image = image;
    imageViewCreateInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    imageViewCreateInfo.format = ToVulkanFormat(desc.format);
    imageViewCreateInfo.subresourceRange.aspectMask = ToAspectMask(desc.format);
    imageViewCreateInfo.subresourceRange.baseMipLevel = 0;
    imageViewCreateInfo.subresourceRange.levelCount = std::max(1u, desc.mipLevels);
    imageViewCreateInfo.subresourceRange.baseArrayLayer = 0;
    imageViewCreateInfo.subresourceRange.layerCount = 1;

    if (vkCreateImageView(deviceContext->device, &imageViewCreateInfo, nullptr, &imageView) != VK_SUCCESS)
    {
        DestroyImage();
        return false;
    }

    VkSamplerCreateInfo samplerCreateInfo{};
    samplerCreateInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerCreateInfo.magFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.minFilter = VK_FILTER_LINEAR;
    samplerCreateInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
    samplerCreateInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerCreateInfo.maxLod = static_cast<float>(std::max(1u, desc.mipLevels));

    if (vkCreateSampler(deviceContext->device, &samplerCreateInfo, nullptr, &sampler) != VK_SUCCESS)
    {
        DestroyImage();
        return false;
    }

    return true;
}

void VulkanTextureResource::DestroyImage() noexcept { DestroyImageHandle(deviceContext, image, memory, imageView, sampler); }

namespace
{

std::unordered_map<std::uint32_t, VulkanRenderTargetResource *> g_vulkanRenderTargets;
std::uint32_t g_nextVulkanRenderTargetHandle = 1;

}

VulkanRenderTargetResource::VulkanRenderTargetResource(std::shared_ptr<VulkanDeviceContext> deviceContextIn, RenderTargetCreateInfo createInfo)
    : deviceContext(std::move(deviceContextIn)),
      desc(createInfo.desc),
      debugName(std::move(createInfo.debugName)),
      activeColorAttachmentCount(static_cast<std::uint32_t>(createInfo.desc.colorAttachments.size()))
{
    colorAttachments.resize(desc.colorAttachments.size(), nullptr);
    handle = g_nextVulkanRenderTargetHandle++;
    RegisterSelf();
}

VulkanRenderTargetResource::~VulkanRenderTargetResource() { UnregisterSelf(); }

GraphicsAPI VulkanRenderTargetResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanRenderTargetResource::GetDebugName() const noexcept { return debugName; }

const RenderTargetDesc &VulkanRenderTargetResource::GetDescription() const noexcept { return desc; }

void VulkanRenderTargetResource::Bind() const {}

void VulkanRenderTargetResource::Unbind() const {}

void VulkanRenderTargetResource::Resize(std::uint32_t width, std::uint32_t height) { desc.extent = {width, height}; }

bool VulkanRenderTargetResource::IsComplete() const
{
    if (desc.extent.width == 0 || desc.extent.height == 0 || colorAttachments.empty())
    {
        return false;
    }

    const std::uint32_t attachmentCount = std::min(activeColorAttachmentCount, static_cast<std::uint32_t>(colorAttachments.size()));
    for (std::uint32_t i = 0; i < attachmentCount; ++i)
    {
        if (colorAttachments[i] == nullptr || colorAttachments[i]->GetDescription().extent.width != desc.extent.width ||
            colorAttachments[i]->GetDescription().extent.height != desc.extent.height)
        {
            return false;
        }
    }

    if (desc.hasDepthBuffer && desc.depthAsTexture &&
        (depthAttachment == nullptr || depthAttachment->GetDescription().extent.width != desc.extent.width ||
         depthAttachment->GetDescription().extent.height != desc.extent.height))
    {
        return false;
    }

    return true;
}

void VulkanRenderTargetResource::BlitFromDefault(std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth,
                                                 std::uint32_t dstHeight) const
{
    static_cast<void>(srcWidth);
    static_cast<void>(srcHeight);
    static_cast<void>(dstWidth);
    static_cast<void>(dstHeight);
}

void VulkanRenderTargetResource::BlitTo(const IRenderTargetResource &destination, std::uint32_t srcWidth, std::uint32_t srcHeight,
                                        std::uint32_t dstWidth, std::uint32_t dstHeight) const
{
    static_cast<void>(destination);
    static_cast<void>(srcWidth);
    static_cast<void>(srcHeight);
    static_cast<void>(dstWidth);
    static_cast<void>(dstHeight);
}

void VulkanRenderTargetResource::BlitToDefault(std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth,
                                               std::uint32_t dstHeight) const
{
    static_cast<void>(srcWidth);
    static_cast<void>(srcHeight);
    static_cast<void>(dstWidth);
    static_cast<void>(dstHeight);
}

void VulkanRenderTargetResource::SetDrawBuffers(std::uint32_t count)
{
    activeColorAttachmentCount = std::min(count, static_cast<std::uint32_t>(desc.colorAttachments.size()));
}

std::uint32_t VulkanRenderTargetResource::GetHandle() const noexcept { return handle; }

VulkanRenderTargetResource *VulkanRenderTargetResource::FindByHandle(std::uint32_t handle) noexcept
{
    const auto it = g_vulkanRenderTargets.find(handle);
    return it == g_vulkanRenderTargets.end() ? nullptr : it->second;
}

void VulkanRenderTargetResource::RegisterColorAttachment(std::uint32_t colorIndex, const VulkanTextureResource *texture) noexcept
{
    if (colorIndex >= colorAttachments.size())
    {
        return;
    }
    colorAttachments[colorIndex] = texture;
}

void VulkanRenderTargetResource::RegisterDepthAttachment(const VulkanTextureResource *texture) noexcept { depthAttachment = texture; }

void VulkanRenderTargetResource::RegisterSelf() noexcept { g_vulkanRenderTargets[handle] = this; }

void VulkanRenderTargetResource::UnregisterSelf() noexcept { g_vulkanRenderTargets.erase(handle); }

VulkanAccelerationStructureResource::VulkanAccelerationStructureResource(AccelerationStructureCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
{
}

GraphicsAPI VulkanAccelerationStructureResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanAccelerationStructureResource::GetDebugName() const noexcept { return debugName; }

const AccelerationStructureDesc &VulkanAccelerationStructureResource::GetDescription() const noexcept { return desc; }

VulkanTimestampQueryResource::VulkanTimestampQueryResource(GPUTimestampQueryCreateInfo createInfo)
    : debugName(std::move(createInfo.debugName))
{
}

GraphicsAPI VulkanTimestampQueryResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanTimestampQueryResource::GetDebugName() const noexcept { return debugName; }

void VulkanTimestampQueryResource::Begin()
{
    beginTime = std::chrono::steady_clock::now();
    endTime.reset();
}

void VulkanTimestampQueryResource::End()
{
    if (beginTime.has_value())
    {
        endTime = std::chrono::steady_clock::now();
    }
}

bool VulkanTimestampQueryResource::IsReady() const { return beginTime.has_value() && endTime.has_value(); }

std::chrono::nanoseconds VulkanTimestampQueryResource::GetElapsedTime() const
{
    if (!IsReady())
    {
        return std::chrono::nanoseconds::zero();
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(*endTime - *beginTime);
}
