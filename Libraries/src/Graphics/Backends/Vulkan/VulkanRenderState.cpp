#include "Graphics/Backends/Vulkan/VulkanRenderState.h"

#include <algorithm>
#include <cstdint>
#include <unordered_map>
#include <vector>

#include "Logger.h"

namespace VulkanRenderState
{

namespace
{

struct BoundBuffer
{
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceSize size = 0;
};

struct BoundTexture
{
    VkImageView imageView = VK_NULL_HANDLE;
    VkSampler sampler = VK_NULL_HANDLE;
};

FramebufferState state{};
glm::vec4 clearColor = glm::vec4(0.0f);
bool wireframe = false;
int scissorRect[4] = {0, 0, 0, 0};
VkCommandBuffer currentCommandBuffer = VK_NULL_HANDLE;
VkExtent2D currentExtent{};
std::unordered_map<std::uint32_t, BoundBuffer> uniformBindings;
std::unordered_map<std::uint32_t, BoundBuffer> storageBindings;
std::unordered_map<std::uint32_t, BoundTexture> textureBindings;
int wireframePushConstant = 0;

struct PendingBufferDestroy
{
    VkDevice device = VK_NULL_HANDLE;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    std::uint64_t retiredAtFrame = 0;
};

std::vector<PendingBufferDestroy> pendingDestroys;
std::uint64_t frameCounter = 0;

} // namespace

void BindFramebuffer(unsigned int framebuffer) noexcept { state.framebuffer = framebuffer; }

void SetViewport(int x, int y, int width, int height) noexcept
{
    state.viewport[0] = x;
    state.viewport[1] = y;
    state.viewport[2] = width;
    state.viewport[3] = height;
}

void ClearColor(const glm::vec4 &color) noexcept { clearColor = color; }

void ClearColorBuffer() noexcept {}

void ClearBuffers(bool clearColorBuffer, bool clearDepth) noexcept
{
    static_cast<void>(clearColorBuffer);
    static_cast<void>(clearDepth);
}

void ClearTransparentColorBuffer() noexcept { clearColor = glm::vec4(0.0f); }

void SetDepthTest(bool enabled) noexcept { state.depthTest = enabled; }

void SetWireframe(bool enabled) noexcept { wireframe = enabled; }

void SetBlend(bool enabled) noexcept { state.blend = enabled; }

void SetAlphaBlend() noexcept {}

void SetPremultipliedAlphaBlend() noexcept {}

void SetScissorTest(bool enabled) noexcept { state.scissorTest = enabled; }

void SetScissor(int x, int y, int width, int height) noexcept
{
    scissorRect[0] = x;
    scissorRect[1] = y;
    scissorRect[2] = width;
    scissorRect[3] = height;
}

glm::vec4 GetClearColor() noexcept { return clearColor; }

FramebufferState CaptureFramebufferState() noexcept { return state; }

void RestoreFramebufferState(const FramebufferState &captured) noexcept
{
    state = captured;
}

void PrepareScreenPass(int width, int height) noexcept
{
    static_cast<void>(wireframe);
    static_cast<void>(clearColor);
    static_cast<void>(scissorRect);
    SetViewport(0, 0, width, height);
}

void SetCurrentCommandBuffer(VkCommandBuffer cmd, VkExtent2D extent) noexcept
{
    currentCommandBuffer = cmd;
    currentExtent = extent;
}

VkCommandBuffer GetCurrentCommandBuffer() noexcept { return currentCommandBuffer; }

VkExtent2D GetCurrentExtent() noexcept { return currentExtent; }

void RegisterBufferBinding(BufferBindingKind kind, std::uint32_t bindingPoint, VkBuffer buffer, VkDeviceSize size) noexcept
{
    auto &map = (kind == BufferBindingKind::Storage) ? storageBindings : uniformBindings;
    if (buffer == VK_NULL_HANDLE)
    {
        map.erase(bindingPoint);
        return;
    }
    map[bindingPoint] = {buffer, size};
}

VkBuffer GetUniformBufferAt(std::uint32_t bindingPoint, VkDeviceSize *outSize) noexcept
{
    const auto it = uniformBindings.find(bindingPoint);
    if (it == uniformBindings.end())
    {
        if (outSize != nullptr)
        {
            *outSize = 0;
        }
        return VK_NULL_HANDLE;
    }
    if (outSize != nullptr)
    {
        *outSize = it->second.size;
    }
    return it->second.buffer;
}

VkBuffer GetStorageBufferAt(std::uint32_t bindingPoint, VkDeviceSize *outSize) noexcept
{
    const auto it = storageBindings.find(bindingPoint);
    if (it == storageBindings.end())
    {
        if (outSize != nullptr)
        {
            *outSize = 0;
        }
        return VK_NULL_HANDLE;
    }
    if (outSize != nullptr)
    {
        *outSize = it->second.size;
    }
    return it->second.buffer;
}

void RegisterTextureBinding(std::uint32_t bindingPoint, VkImageView imageView, VkSampler sampler) noexcept
{
    if (imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
    {
        textureBindings.erase(bindingPoint);
        return;
    }
    textureBindings[bindingPoint] = {imageView, sampler};
}

bool GetTextureBinding(std::uint32_t bindingPoint, VkImageView *outView, VkSampler *outSampler) noexcept
{
    const auto it = textureBindings.find(bindingPoint);
    if (it == textureBindings.end())
    {
        return false;
    }
    if (outView != nullptr)
    {
        *outView = it->second.imageView;
    }
    if (outSampler != nullptr)
    {
        *outSampler = it->second.sampler;
    }
    return true;
}

void ClearTextureBindings() noexcept { textureBindings.clear(); }

void SetWireframePushConstant(int wireframeValue) noexcept { wireframePushConstant = wireframeValue; }

int GetWireframePushConstant() noexcept { return wireframePushConstant; }

void RetireBuffer(VkDevice device, VkBuffer buffer, VkDeviceMemory memory) noexcept
{
    if (buffer == VK_NULL_HANDLE && memory == VK_NULL_HANDLE)
    {
        return;
    }
    pendingDestroys.push_back({device, buffer, memory, frameCounter});
}

void DrainExpiredRetirements(VkDevice device, std::uint32_t maxFramesInFlight) noexcept
{
    static_cast<void>(device);
    ++frameCounter;
    const std::size_t before = pendingDestroys.size();
    // An entry is safe to destroy once `maxFramesInFlight` full frames have elapsed since it was
    // retired — by then the GPU has finished any frame that could still reference it.
    pendingDestroys.erase(std::remove_if(pendingDestroys.begin(), pendingDestroys.end(),
                                         [maxFramesInFlight](const PendingBufferDestroy &entry) {
                                             if (frameCounter - entry.retiredAtFrame > maxFramesInFlight)
                                             {
                                                 if (entry.device != VK_NULL_HANDLE)
                                                 {
                                                     if (entry.buffer != VK_NULL_HANDLE)
                                                     {
                                                         vkDestroyBuffer(entry.device, entry.buffer, nullptr);
                                                     }
                                                     if (entry.memory != VK_NULL_HANDLE)
                                                     {
                                                         vkFreeMemory(entry.device, entry.memory, nullptr);
                                                     }
                                                 }
                                                 return true;
                                             }
                                             return false;
                                         }),
                          pendingDestroys.end());
    static_cast<void>(before);
}

void DrainAllRetirements(VkDevice device) noexcept
{
    static_cast<void>(device);
    for (const PendingBufferDestroy &entry : pendingDestroys)
    {
        if (entry.device == VK_NULL_HANDLE)
        {
            continue;
        }
        if (entry.buffer != VK_NULL_HANDLE)
        {
            vkDestroyBuffer(entry.device, entry.buffer, nullptr);
        }
        if (entry.memory != VK_NULL_HANDLE)
        {
            vkFreeMemory(entry.device, entry.memory, nullptr);
        }
    }
    pendingDestroys.clear();
}

} // namespace VulkanRenderState
