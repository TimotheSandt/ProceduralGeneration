#pragma once

#include "Graphics/Backends/Vulkan/VulkanContext.h"
#include "Graphics/Backends/Vulkan/VulkanPipelineCache.h"
#include "Graphics/Core/GraphicsResources.h"

#include <vulkan/vulkan.h>

#include <optional>
#include <memory>
#include <unordered_map>

class VulkanShaderProgramResource final : public IShaderProgramResource
{
  public:
    explicit VulkanShaderProgramResource(ShaderProgramCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string GetDebugName() const noexcept override;
    const ShaderProgramDesc &GetDescription() const noexcept override;
    void Bind() const override;
    void Unbind() const override;
    int GetUniformLocation(std::string name) const override;
    void SetFloatUniform(int location, const float *data, std::size_t componentCount) const override;
    void SetIntUniform(int location, const int *data, std::size_t componentCount) const override;
    void SetMatrix4Uniform(int location, const float *data) const override;
    bool GetBinary(std::vector<std::byte> &dataOut, std::uint32_t &formatOut) const override;
    bool LoadBinary(const std::vector<std::byte> &data, std::uint32_t format) override;

    const std::vector<VulkanPipelineCache::CompiledShaderStage> &GetCompiledStages() const noexcept { return compiledStages; }

    // Push-constant byte buffer + per-uniform offsets — populated at construction by merging
    // bare uniforms across all stages. SetXxxUniform writes into this buffer at the matching
    // offset; the draw setup pushes the whole buffer in one vkCmdPushConstants call.
    std::uint32_t GetPushConstantSize() const noexcept { return pushConstantSize; }
    const std::vector<std::byte> &GetPushConstantData() const noexcept { return pushConstantBuffer; }

    struct PushConstantSlot
    {
        std::uint32_t offset = 0;
        std::uint32_t sizeBytes = 0;
    };

  private:
    ShaderProgramDesc desc;
    std::string debugName;
    std::vector<ShaderStageSource> stageSources;
    std::vector<VulkanPipelineCache::CompiledShaderStage> compiledStages;
    mutable std::unordered_map<std::string, int> uniformLocations;
    mutable std::unordered_map<int, std::vector<std::byte>> uniformData;
    std::unordered_map<std::string, PushConstantSlot> pushConstantSlots;
    mutable std::vector<std::byte> pushConstantBuffer;
    std::uint32_t pushConstantSize = 0;
    std::vector<std::byte> cachedBinary;
    std::uint32_t cachedBinaryFormat = 0;
    mutable int nextUniformLocation = 0;
};

class VulkanBufferResource final : public IBufferResource
{
  public:
    VulkanBufferResource(std::shared_ptr<VulkanDeviceContext> deviceContext, BufferCreateInfo createInfo);
    ~VulkanBufferResource() override;

    GraphicsAPI GetAPI() const noexcept override;
    std::string GetDebugName() const noexcept override;
    const BufferDesc &GetDescription() const noexcept override;
    void Bind() const override;
    void BindToBindingPoint(std::uint32_t bindingPoint) const override;
    void Unbind() const override;
    void UploadData(const void *data, std::size_t size, std::size_t offset) override;
    void Resize(std::size_t newSize, bool preserveData) override;
    void *Map(BufferMapAccess access) override;
    void Unmap() override;

  private:
    bool CreateBuffer(std::size_t sizeInBytes);
    void DestroyBuffer() noexcept;
    void UploadStorageToGPU(std::size_t offset, std::size_t size) const;

    std::shared_ptr<VulkanDeviceContext> deviceContext;
    BufferDesc desc;
    std::string debugName;
    std::vector<std::byte> storage;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mappedData = nullptr;
    bool mapped = false;
};

class VulkanGeometryResource final : public IGeometryResource
{
  public:
    VulkanGeometryResource(std::shared_ptr<VulkanDeviceContext> deviceContext, GeometryCreateInfo createInfo);
    ~VulkanGeometryResource() override;

    GraphicsAPI GetAPI() const noexcept override;
    std::string GetDebugName() const noexcept override;
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
    bool CreateOrResizeBuffers();
    void DestroyBuffers() noexcept;
    void UploadBuffer(VkBuffer buffer, VkDeviceMemory memory, const void *data, std::size_t size) const;

    std::shared_ptr<VulkanDeviceContext> deviceContext;
    GeometryLayout layout;
    std::vector<float> vertexData;
    std::vector<std::uint32_t> indexData;
    std::vector<float> instanceData;
    std::string debugName;
    VkBuffer vertexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE;
    VkBuffer indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory indexMemory = VK_NULL_HANDLE;
    VkBuffer instanceBuffer = VK_NULL_HANDLE;
    VkDeviceMemory instanceMemory = VK_NULL_HANDLE;
};

class VulkanTextureResource final : public ITextureResource
{
  public:
    friend class VulkanRenderTargetResource;

    VulkanTextureResource(std::shared_ptr<VulkanDeviceContext> deviceContext, TextureCreateInfo createInfo);
    ~VulkanTextureResource() override;

    GraphicsAPI GetAPI() const noexcept override;
    std::string GetDebugName() const noexcept override;
    const TextureDesc &GetDescription() const noexcept override;
    void Bind(std::uint32_t slot) const override;
    void Unbind() const override;
    void Readback(std::vector<std::byte> &output) const override;
    void Resize(std::uint32_t width, std::uint32_t height) override;
    void AttachToFramebuffer(std::uint32_t framebufferHandle, std::uint32_t colorIndex) const override;
    void AttachAsDepthToFramebuffer(std::uint32_t framebufferHandle) const override;

  private:
    static std::size_t GetPixelSize(TextureFormat format) noexcept;
    static VkFormat ToVulkanFormat(TextureFormat format) noexcept;
    static VkImageAspectFlags ToAspectMask(TextureFormat format) noexcept;
    bool CreateImage();
    bool UploadStorageToImage();
    bool ReadbackImageToStorage() const;
    void DestroyImage() noexcept;

    std::shared_ptr<VulkanDeviceContext> deviceContext;
    TextureDesc desc;
    std::string debugName;
    mutable std::vector<std::byte> storage;
    VkImage image = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
    mutable VkImageLayout currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
};

class VulkanRenderTargetResource final : public IRenderTargetResource
{
  public:
    VulkanRenderTargetResource(std::shared_ptr<VulkanDeviceContext> deviceContext, RenderTargetCreateInfo createInfo);
    ~VulkanRenderTargetResource() override;

    GraphicsAPI GetAPI() const noexcept override;
    std::string GetDebugName() const noexcept override;
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
    std::uint32_t ReadPixelUInt(std::uint32_t attachmentIndex, int x, int y, int framebufferHeight) const override;
    glm::uvec4 ReadPixelRGBA8(std::uint32_t attachmentIndex, int x, int y, int framebufferHeight) const override;
    std::uint32_t GetHandle() const noexcept;
    static VulkanRenderTargetResource *FindByHandle(std::uint32_t handle) noexcept;
    void RegisterColorAttachment(std::uint32_t colorIndex, const VulkanTextureResource *texture) noexcept;
    void RegisterDepthAttachment(const VulkanTextureResource *texture) noexcept;

  private:
    const VulkanTextureResource *GetColorAttachment(std::uint32_t attachmentIndex) const noexcept;
    void RegisterSelf() noexcept;
    void UnregisterSelf() noexcept;
    bool CreateOffscreenResources() noexcept;
    void DestroyOffscreenResources() noexcept;

    std::shared_ptr<VulkanDeviceContext> deviceContext;
    RenderTargetDesc desc;
    std::string debugName;
    std::uint32_t activeColorAttachmentCount = 0;
    std::uint32_t handle = 0;
    std::vector<const VulkanTextureResource *> colorAttachments;
    const VulkanTextureResource *depthAttachment = nullptr;
    VkRenderPass offscreenRenderPass = VK_NULL_HANDLE;
    VkRenderPass offscreenRenderPassLoad = VK_NULL_HANDLE;
    VkFramebuffer offscreenFramebuffer = VK_NULL_HANDLE;
};

class VulkanAccelerationStructureResource final : public IAccelerationStructureResource
{
  public:
    explicit VulkanAccelerationStructureResource(AccelerationStructureCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string GetDebugName() const noexcept override;
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
    std::string GetDebugName() const noexcept override;
    void Begin() override;
    void End() override;
    bool IsReady() const override;
    std::chrono::nanoseconds GetElapsedTime() const override;

  private:
    std::string debugName;
    std::optional<std::chrono::steady_clock::time_point> beginTime;
    std::optional<std::chrono::steady_clock::time_point> endTime;
};
