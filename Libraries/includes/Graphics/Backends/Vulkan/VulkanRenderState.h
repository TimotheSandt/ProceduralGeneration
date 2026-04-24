#pragma once

#include <cstdint>
#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

namespace VulkanRenderState
{

struct FramebufferState
{
    unsigned int framebuffer = 0;
    int viewport[4] = {0, 0, 0, 0};
    bool depthTest = false;
    bool blend = false;
    bool scissorTest = false;
};

void BindFramebuffer(unsigned int framebuffer) noexcept;
void SetFramebuffer(unsigned int framebuffer) noexcept;
void SetViewport(int x, int y, int width, int height) noexcept;
void ClearColor(const glm::vec4 &color) noexcept;
void ClearColorBuffer() noexcept;
void ClearBuffers(bool clearColor, bool clearDepth) noexcept;
void ClearTransparentColorBuffer() noexcept;
void SetDepthTest(bool enabled) noexcept;
void SetWireframe(bool enabled) noexcept;
void SetBlend(bool enabled) noexcept;
void SetAlphaBlend() noexcept;
void SetPremultipliedAlphaBlend() noexcept;
void SetScissorTest(bool enabled) noexcept;
void SetScissor(int x, int y, int width, int height) noexcept;
glm::vec4 GetClearColor() noexcept;

FramebufferState CaptureFramebufferState() noexcept;
void RestoreFramebufferState(const FramebufferState &state) noexcept;
void PrepareScreenPass(int width, int height) noexcept;

// Frame command buffer — set by VulkanWindowContext at BeginFrame, cleared at EndFrame.
void SetCurrentCommandBuffer(VkCommandBuffer cmd, VkExtent2D extent) noexcept;
VkCommandBuffer GetCurrentCommandBuffer() noexcept;
VkExtent2D GetCurrentExtent() noexcept;

// Active render pass — set by VulkanWindowContext at BeginFrame and by VulkanRenderTargetResource
// when switching to/from off-screen render passes. Used by EnsurePipelineFor so pipelines are
// created against the render pass they will actually be used in.
void SetCurrentRenderPass(VkRenderPass rp) noexcept;
VkRenderPass GetCurrentRenderPass() noexcept;

// Buffer binding tracking — each buffer that calls BindToBindingPoint registers itself here
// so the geometry/draw can later look up which VkBuffer corresponds to which binding point.
enum class BufferBindingKind : std::uint8_t
{
    Uniform = 0,
    Storage = 1,
};
void RegisterBufferBinding(BufferBindingKind kind, std::uint32_t bindingPoint, VkBuffer buffer, VkDeviceSize size) noexcept;
VkBuffer GetUniformBufferAt(std::uint32_t bindingPoint, VkDeviceSize *outSize = nullptr) noexcept;
VkBuffer GetStorageBufferAt(std::uint32_t bindingPoint, VkDeviceSize *outSize = nullptr) noexcept;

// Texture binding tracking — a VulkanTextureResource registers its (imageView, sampler) at the
// binding number a draw will sample from. Cleared per-frame by VulkanWindowContext::BeginFrame.
void RegisterTextureBinding(std::uint32_t bindingPoint, VkImageView imageView, VkSampler sampler) noexcept;
bool GetTextureBinding(std::uint32_t bindingPoint, VkImageView *outView, VkSampler *outSampler) noexcept;
void ClearTextureBindings() noexcept;

// Push-constant slot for the bare wireframe int (used by default fragment shader).
void SetWireframePushConstant(int wireframe) noexcept;
int GetWireframePushConstant() noexcept;

// Deferred buffer destruction. A VkBuffer/VkDeviceMemory recorded into a still-in-flight command
// buffer must outlive that frame's GPU work — destroying it immediately when a resource gets
// resized or replaced causes intermittent VK_ERROR_DEVICE_LOST. RetireBuffer enqueues the pair;
// DrainExpiredRetirements (called once per BeginFrame after the fence wait) actually destroys
// pairs whose retirement is older than maxFramesInFlight frames. DrainAllRetirements is called
// at shutdown after vkDeviceWaitIdle to flush everything.
void RetireBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory) noexcept;
void DrainExpiredRetirements(VkDevice device, std::uint32_t maxFramesInFlight) noexcept;
void DrainAllRetirements(VkDevice device) noexcept;

} // namespace VulkanRenderState
