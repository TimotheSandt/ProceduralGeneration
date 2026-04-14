#include "Graphics/Backends/Vulkan/VulkanGraphicsResources.h"

#include <algorithm>
#include <cstring>

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

VulkanBufferResource::VulkanBufferResource(BufferCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName)), storage(std::move(createInfo.initialData))
{
    storage.resize(std::max(desc.sizeInBytes, storage.size()));
    desc.sizeInBytes = storage.size();
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
}

void *VulkanBufferResource::Map(BufferMapAccess access)
{
    static_cast<void>(access);
    mapped = true;
    return storage.empty() ? nullptr : storage.data();
}

void VulkanBufferResource::Unmap() { mapped = false; }

VulkanGeometryResource::VulkanGeometryResource(GeometryCreateInfo createInfo)
    : layout(std::move(createInfo.layout)),
      vertexData(std::move(createInfo.vertexData)),
      indexData(std::move(createInfo.indexData)),
      instanceData(std::move(createInfo.instanceData)),
      debugName(std::move(createInfo.debugName))
{
}

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
}

void VulkanGeometryResource::DrawIndexed() const {}

void VulkanGeometryResource::DrawIndexedInstanced() const {}

void VulkanGeometryResource::DrawVertices(std::size_t vertexCount) const { static_cast<void>(vertexCount); }

VulkanTextureResource::VulkanTextureResource(TextureCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName)), storage(std::move(createInfo.initialData))
{
    Resize(desc.extent.width, desc.extent.height);
}

GraphicsAPI VulkanTextureResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanTextureResource::GetDebugName() const noexcept { return debugName; }

const TextureDesc &VulkanTextureResource::GetDescription() const noexcept { return desc; }

void VulkanTextureResource::Bind(std::uint32_t slot) const { static_cast<void>(slot); }

void VulkanTextureResource::Unbind() const {}

void VulkanTextureResource::Readback(std::vector<std::byte> &output) const { output = storage; }

void VulkanTextureResource::Resize(std::uint32_t width, std::uint32_t height)
{
    desc.extent = {width, height};
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    storage.resize(pixelCount * GetPixelSize(desc.format));
}

void VulkanTextureResource::AttachToFramebuffer(std::uint32_t framebufferHandle, std::uint32_t colorIndex) const
{
    static_cast<void>(framebufferHandle);
    static_cast<void>(colorIndex);
}

void VulkanTextureResource::AttachAsDepthToFramebuffer(std::uint32_t framebufferHandle) const
{
    static_cast<void>(framebufferHandle);
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

VulkanRenderTargetResource::VulkanRenderTargetResource(RenderTargetCreateInfo createInfo)
    : desc(createInfo.desc),
      debugName(std::move(createInfo.debugName)),
      activeColorAttachmentCount(static_cast<std::uint32_t>(createInfo.desc.colorAttachments.size()))
{
}

GraphicsAPI VulkanRenderTargetResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string_view VulkanRenderTargetResource::GetDebugName() const noexcept { return debugName; }

const RenderTargetDesc &VulkanRenderTargetResource::GetDescription() const noexcept { return desc; }

void VulkanRenderTargetResource::Bind() const {}

void VulkanRenderTargetResource::Unbind() const {}

void VulkanRenderTargetResource::Resize(std::uint32_t width, std::uint32_t height) { desc.extent = {width, height}; }

bool VulkanRenderTargetResource::IsComplete() const
{
    return desc.extent.width > 0 && desc.extent.height > 0 && !desc.colorAttachments.empty();
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
