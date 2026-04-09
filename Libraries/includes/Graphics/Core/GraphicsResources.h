#pragma once

#include "Graphics/Core/GraphicsTypes.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct ShaderStageSource
{
    ShaderStage stage = ShaderStage::Vertex;
    std::string sourceCode;
    std::string entryPoint = "main";
};

struct ShaderProgramCreateInfo
{
    ShaderProgramDesc desc{};
    std::string debugName;
    std::vector<ShaderStageSource> stageSources;
};

struct BufferCreateInfo
{
    BufferDesc desc{};
    std::string debugName;
    std::vector<std::byte> initialData;
};

enum class BufferMapAccess
{
    ReadOnly,
    WriteOnly,
    ReadWrite,
};

struct GeometryLayout
{
    std::vector<std::uint32_t> vertexAttributes;
    std::vector<std::uint32_t> instanceAttributes;
};

struct TextureCreateInfo
{
    TextureDesc desc{};
    std::string debugName;
    std::vector<std::byte> initialData;
    bool generateMipmaps = true;
};

struct RenderTargetCreateInfo
{
    RenderTargetDesc desc{};
    std::string debugName;
};

struct GeometryCreateInfo
{
    GeometryLayout layout{};
    std::vector<float> vertexData;
    std::vector<std::uint32_t> indexData;
    std::vector<float> instanceData;
    std::string debugName;
};

struct AccelerationStructureCreateInfo
{
    AccelerationStructureDesc desc{};
    std::string debugName;
};

class IGraphicsResource
{
  public:
    virtual ~IGraphicsResource() = default;

    virtual GraphicsAPI GetAPI() const noexcept = 0;
    virtual std::string_view GetDebugName() const noexcept = 0;
};

class IShaderProgramResource : public IGraphicsResource
{
  public:
    ~IShaderProgramResource() override = default;

    virtual const ShaderProgramDesc &GetDescription() const noexcept = 0;
    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;
    virtual int GetUniformLocation(std::string_view name) const = 0;
    virtual void SetFloatUniform(int location, const float *data, std::size_t componentCount) const = 0;
    virtual void SetIntUniform(int location, const int *data, std::size_t componentCount) const = 0;
    virtual void SetMatrix4Uniform(int location, const float *data) const = 0;
};

class IBufferResource : public IGraphicsResource
{
  public:
    ~IBufferResource() override = default;

    virtual const BufferDesc &GetDescription() const noexcept = 0;
    virtual void Bind() const = 0;
    virtual void BindToBindingPoint(std::uint32_t bindingPoint) const = 0;
    virtual void Unbind() const = 0;
    virtual void UploadData(const void *data, std::size_t size, std::size_t offset) = 0;
    virtual void Resize(std::size_t newSize, bool preserveData) = 0;
    virtual void *Map(BufferMapAccess access) = 0;
    virtual void Unmap() = 0;
};

class ITextureResource : public IGraphicsResource
{
  public:
    ~ITextureResource() override = default;

    virtual const TextureDesc &GetDescription() const noexcept = 0;
    virtual void Bind(std::uint32_t slot) const = 0;
    virtual void Unbind() const = 0;
    virtual void Readback(std::vector<std::byte> &output) const = 0;
    virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual void AttachToFramebuffer(std::uint32_t framebufferHandle) const = 0;
};

class IGeometryResource : public IGraphicsResource
{
  public:
    ~IGeometryResource() override = default;

    virtual const GeometryLayout &GetLayout() const noexcept = 0;
    virtual std::size_t GetIndexCount() const noexcept = 0;
    virtual std::size_t GetInstanceCount() const noexcept = 0;
    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;
    virtual void DrawIndexed() const = 0;
    virtual void DrawIndexedInstanced() const = 0;
};

class IRenderTargetResource : public IGraphicsResource
{
  public:
    ~IRenderTargetResource() override = default;

    virtual const RenderTargetDesc &GetDescription() const noexcept = 0;
    virtual void Bind() const = 0;
    virtual void Unbind() const = 0;
    virtual void Resize(std::uint32_t width, std::uint32_t height) = 0;
    virtual bool IsComplete() const = 0;
    virtual void BlitTo(const IRenderTargetResource &destination, std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth,
                        std::uint32_t dstHeight) const = 0;
    virtual void BlitToDefault(std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth, std::uint32_t dstHeight) const = 0;
};

class IAccelerationStructureResource : public IGraphicsResource
{
  public:
    ~IAccelerationStructureResource() override = default;

    virtual const AccelerationStructureDesc &GetDescription() const noexcept = 0;
};
