#include "Graphics/Backends/Vulkan/VulkanRenderState.h"

#include "Graphics/Backends/Vulkan/VulkanGraphicsResources.h"
#include "Graphics/Backends/Vulkan/VulkanWindowContext.h"

#include <algorithm>
#include <array>
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
VkRenderPass currentRenderPass = VK_NULL_HANDLE;
constexpr std::uint32_t kFastBindingSlots = 64;
std::array<BoundBuffer, kFastBindingSlots> uniformBindingSlots{};
std::array<BoundBuffer, kFastBindingSlots> storageBindingSlots{};
std::array<BoundTexture, kFastBindingSlots> textureBindingSlots{};
std::array<bool, kFastBindingSlots> uniformBindingValid{};
std::array<bool, kFastBindingSlots> storageBindingValid{};
std::array<bool, kFastBindingSlots> textureBindingValid{};
std::unordered_map<std::uint32_t, BoundBuffer> overflowUniformBindings;
std::unordered_map<std::uint32_t, BoundBuffer> overflowStorageBindings;
std::unordered_map<std::uint32_t, BoundTexture> overflowTextureBindings;
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

void SetFramebuffer(unsigned int framebuffer) noexcept { state.framebuffer = framebuffer; }

void BindFramebuffer(unsigned int framebuffer) noexcept
{
    if (framebuffer == state.framebuffer)
    {
        if (framebuffer == 0 && currentCommandBuffer != VK_NULL_HANDLE && currentRenderPass == VK_NULL_HANDLE)
        {
            VulkanWindowContext::ResumeSwapchainRenderPass();
        }
        return;
    }

    const VkCommandBuffer cmd = currentCommandBuffer;
    if (cmd == VK_NULL_HANDLE)
    {
        SetFramebuffer(framebuffer);
        return;
    }

    const auto endActiveRenderPass = [&]()
    {
        if (currentRenderPass != VK_NULL_HANDLE)
        {
            vkCmdEndRenderPass(cmd);
            SetCurrentRenderPass(VK_NULL_HANDLE);
        }
    };

    if (framebuffer == 0)
    {
        if (VulkanRenderTargetResource *currentTarget = VulkanRenderTargetResource::FindByHandle(state.framebuffer);
            currentTarget != nullptr)
        {
            currentTarget->Unbind();
        }

        if (currentRenderPass == VK_NULL_HANDLE)
        {
            VulkanWindowContext::ResumeSwapchainRenderPass();
        }
        SetFramebuffer(0);
        return;
    }

    if (VulkanRenderTargetResource *currentTarget = VulkanRenderTargetResource::FindByHandle(state.framebuffer); currentTarget != nullptr)
    {
        currentTarget->Unbind();
    }
    endActiveRenderPass();

    if (VulkanRenderTargetResource *renderTarget = VulkanRenderTargetResource::FindByHandle(framebuffer); renderTarget != nullptr)
    {
        renderTarget->Bind();
        return;
    }

    SetFramebuffer(framebuffer);
}

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

void RestoreFramebufferState(const FramebufferState &captured) noexcept { state = captured; }

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
    auto &slots = (kind == BufferBindingKind::Storage) ? storageBindingSlots : uniformBindingSlots;
    auto &valid = (kind == BufferBindingKind::Storage) ? storageBindingValid : uniformBindingValid;
    auto &overflow = (kind == BufferBindingKind::Storage) ? overflowStorageBindings : overflowUniformBindings;
    if (bindingPoint < kFastBindingSlots)
    {
        if (buffer == VK_NULL_HANDLE)
        {
            valid[bindingPoint] = false;
            slots[bindingPoint] = {};
            return;
        }
        slots[bindingPoint] = {buffer, size};
        valid[bindingPoint] = true;
        return;
    }

    if (buffer == VK_NULL_HANDLE)
    {
        overflow.erase(bindingPoint);
        return;
    }
    overflow[bindingPoint] = {buffer, size};
}

VkBuffer GetUniformBufferAt(std::uint32_t bindingPoint, VkDeviceSize *outSize) noexcept
{
    if (bindingPoint < kFastBindingSlots)
    {
        if (!uniformBindingValid[bindingPoint])
        {
            if (outSize != nullptr)
            {
                *outSize = 0;
            }
            return VK_NULL_HANDLE;
        }
        if (outSize != nullptr)
        {
            *outSize = uniformBindingSlots[bindingPoint].size;
        }
        return uniformBindingSlots[bindingPoint].buffer;
    }

    const auto it = overflowUniformBindings.find(bindingPoint);
    if (it == overflowUniformBindings.end())
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
    if (bindingPoint < kFastBindingSlots)
    {
        if (!storageBindingValid[bindingPoint])
        {
            if (outSize != nullptr)
            {
                *outSize = 0;
            }
            return VK_NULL_HANDLE;
        }
        if (outSize != nullptr)
        {
            *outSize = storageBindingSlots[bindingPoint].size;
        }
        return storageBindingSlots[bindingPoint].buffer;
    }

    const auto it = overflowStorageBindings.find(bindingPoint);
    if (it == overflowStorageBindings.end())
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
    if (bindingPoint < kFastBindingSlots)
    {
        if (imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
        {
            textureBindingValid[bindingPoint] = false;
            textureBindingSlots[bindingPoint] = {};
            return;
        }
        textureBindingSlots[bindingPoint] = {imageView, sampler};
        textureBindingValid[bindingPoint] = true;
        return;
    }

    if (imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
    {
        overflowTextureBindings.erase(bindingPoint);
        return;
    }
    overflowTextureBindings[bindingPoint] = {imageView, sampler};
}

bool GetTextureBinding(std::uint32_t bindingPoint, VkImageView *outView, VkSampler *outSampler) noexcept
{
    BoundTexture texture{};
    if (bindingPoint < kFastBindingSlots)
    {
        if (!textureBindingValid[bindingPoint])
        {
            return false;
        }
        texture = textureBindingSlots[bindingPoint];
    }
    else
    {
        const auto it = overflowTextureBindings.find(bindingPoint);
        if (it == overflowTextureBindings.end())
        {
            return false;
        }
        texture = it->second;
    }

    if (texture.imageView == VK_NULL_HANDLE || texture.sampler == VK_NULL_HANDLE)
    {
        return false;
    }
    if (outView != nullptr)
    {
        *outView = texture.imageView;
    }
    if (outSampler != nullptr)
    {
        *outSampler = texture.sampler;
    }
    return true;
}

void ClearTextureBindings() noexcept
{
    textureBindingValid.fill(false);
    textureBindingSlots = {};
    overflowTextureBindings.clear();
}

void SetCurrentRenderPass(VkRenderPass rp) noexcept { currentRenderPass = rp; }

VkRenderPass GetCurrentRenderPass() noexcept { return currentRenderPass; }

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
                                         [maxFramesInFlight](const PendingBufferDestroy &entry)
                                         {
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
