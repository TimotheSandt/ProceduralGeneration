#pragma once

#include "Graphics/Core/GraphicsResources.h"

#include <optional>
#include <unordered_map>

class VulkanShaderProgramResource final : public IShaderProgramResource
{
  public:
    explicit VulkanShaderProgramResource(ShaderProgramCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const ShaderProgramDesc &GetDescription() const noexcept override;
    void Bind() const override;
    void Unbind() const override;
    int GetUniformLocation(std::string_view name) const override;
    void SetFloatUniform(int location, const float *data, std::size_t componentCount) const override;
    void SetIntUniform(int location, const int *data, std::size_t componentCount) const override;
    void SetMatrix4Uniform(int location, const float *data) const override;
    bool GetBinary(std::vector<std::byte> &dataOut, std::uint32_t &formatOut) const override;
    bool LoadBinary(const std::vector<std::byte> &data, std::uint32_t format) override;

  private:
    ShaderProgramDesc desc;
    std::string debugName;
    std::vector<ShaderStageSource> stageSources;
    mutable std::unordered_map<std::string, int> uniformLocations;
    mutable std::unordered_map<int, std::vector<std::byte>> uniformData;
    std::vector<std::byte> cachedBinary;
    std::uint32_t cachedBinaryFormat = 0;
    mutable int nextUniformLocation = 0;
};

class VulkanBufferResource final : public IBufferResource
{
  public:
    explicit VulkanBufferResource(BufferCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const BufferDesc &GetDescription() const noexcept override;
    void Bind() const override;
    void BindToBindingPoint(std::uint32_t bindingPoint) const override;
    void Unbind() const override;
    void UploadData(const void *data, std::size_t size, std::size_t offset) override;
    void Resize(std::size_t newSize, bool preserveData) override;
    void *Map(BufferMapAccess access) override;
    void Unmap() override;

  private:
    BufferDesc desc;
    std::string debugName;
    std::vector<std::byte> storage;
    bool mapped = false;
};

class VulkanGeometryResource final : public IGeometryResource
{
  public:
    explicit VulkanGeometryResource(GeometryCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const GeometryLayout &GetLayout() const noexcept override;
    std::size_t GetIndexCount() const noexcept override;
    std::size_t GetInstanceCount() const noexcept override;
    void Bind() const override;
    void Unbind() const override;
    void UpdateVertexData(const float *data, std::size_t floatCount, std::size_t offsetFloats) override;
    void UpdateInstanceData(const float *data, std::size_t floatCount, std::size_t offsetFloats) override;
    void DrawIndexed() const override;
    void DrawIndexedInstanced() const override;
    void DrawVertices(std::size_t vertexCount) const override;

  private:
    GeometryLayout layout;
    std::vector<float> vertexData;
    std::vector<std::uint32_t> indexData;
    std::vector<float> instanceData;
    std::string debugName;
};

class VulkanTextureResource final : public ITextureResource
{
  public:
    explicit VulkanTextureResource(TextureCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const TextureDesc &GetDescription() const noexcept override;
    void Bind(std::uint32_t slot) const override;
    void Unbind() const override;
    void Readback(std::vector<std::byte> &output) const override;
    void Resize(std::uint32_t width, std::uint32_t height) override;
    void AttachToFramebuffer(std::uint32_t framebufferHandle, std::uint32_t colorIndex) const override;
    void AttachAsDepthToFramebuffer(std::uint32_t framebufferHandle) const override;

  private:
    static std::size_t GetPixelSize(TextureFormat format) noexcept;

    TextureDesc desc;
    std::string debugName;
    std::vector<std::byte> storage;
};

class VulkanRenderTargetResource final : public IRenderTargetResource
{
  public:
    explicit VulkanRenderTargetResource(RenderTargetCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const RenderTargetDesc &GetDescription() const noexcept override;
    void Bind() const override;
    void Unbind() const override;
    void Resize(std::uint32_t width, std::uint32_t height) override;
    bool IsComplete() const override;
    void BlitFromDefault(std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth, std::uint32_t dstHeight) const override;
    void BlitTo(const IRenderTargetResource &destination, std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth,
                std::uint32_t dstHeight) const override;
    void BlitToDefault(std::uint32_t srcWidth, std::uint32_t srcHeight, std::uint32_t dstWidth, std::uint32_t dstHeight) const override;
    void SetDrawBuffers(std::uint32_t count) override;

  private:
    RenderTargetDesc desc;
    std::string debugName;
    std::uint32_t activeColorAttachmentCount = 0;
};

class VulkanAccelerationStructureResource final : public IAccelerationStructureResource
{
  public:
    explicit VulkanAccelerationStructureResource(AccelerationStructureCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const AccelerationStructureDesc &GetDescription() const noexcept override;

  private:
    AccelerationStructureDesc desc;
    std::string debugName;
};

class VulkanTimestampQueryResource final : public IGPUTimestampQueryResource
{
  public:
    explicit VulkanTimestampQueryResource(GPUTimestampQueryCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    void Begin() override;
    void End() override;
    bool IsReady() const override;
    std::chrono::nanoseconds GetElapsedTime() const override;

  private:
    std::string debugName;
    std::optional<std::chrono::steady_clock::time_point> beginTime;
    std::optional<std::chrono::steady_clock::time_point> endTime;
};
