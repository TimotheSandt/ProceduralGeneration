#include "Graphics/Backends/Vulkan/VulkanGraphicsResources.h"

#include <vulkan/vulkan.h>

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <future>
#include <unordered_map>
#include <utility>

#include "Graphics/Backends/Vulkan/VulkanPipelineCache.h"
#include "Graphics/Backends/Vulkan/VulkanRenderState.h"
#include "Graphics/Backends/Vulkan/VulkanWindowContext.h"
#include "Logger.h"

namespace
{

// Forward declarations — CreateBufferHandle is defined further down in this anonymous namespace
// but the staging arena helpers (which sit above it for readability) need to call it.
bool CreateBufferHandle(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkBufferUsageFlags usage, VkDeviceSize size,
                        VkBuffer &bufferOut, VkDeviceMemory &memoryOut);
void InvalidateDescriptorCache() noexcept;

// Texture uploads queued outside a frame (e.g. at construction) are batched here and flushed
// at the start of BeginFrame, recorded into the frame command buffer.
// All staging data lives in a shared arena buffer rather than per-upload allocations — this
// turns N expensive vkAllocateMemory calls into one. With ~256 font glyphs at startup this
// is the dominant init-time speedup.
struct PendingImageUpload
{
    std::shared_ptr<VulkanDeviceContext> deviceContext;
    VkBuffer stagingBuffer = VK_NULL_HANDLE;  // not owned — the staging arena owns it
    VkDeviceSize bufferOffset = 0;            // offset into the arena's buffer where this upload's data lives
    VkImage image = VK_NULL_HANDLE;
    VkImageAspectFlags aspectMask = 0;
    std::uint32_t width = 0, height = 0, mipLevels = 1;
    VkImageLayout fromLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VkImageLayout finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
};
std::vector<PendingImageUpload> g_pendingImageUploads;

// A persistent, mapped staging buffer. Many uploads bump-allocate into this single VkBuffer.
// Rotated (retired + replaced) at flush time; the previous arena's VkBuffer is freed by the
// retirement queue once the GPU is done with the command buffer that referenced it.
struct StagingArena
{
    std::shared_ptr<VulkanDeviceContext> deviceContext;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = nullptr;
    VkDeviceSize capacity = 0;
    VkDeviceSize offset = 0;
};
StagingArena g_stagingArena;

constexpr VkDeviceSize kStagingAlignment = 64;            // covers Vulkan's optimal copy alignment
constexpr VkDeviceSize kStagingMinCapacity = 4 * 1024 * 1024;  // 4 MB seed; grows on demand

// Vertex arena — sub-allocates transient vertex/instance data (TextRenderer's per-glyph quads,
// procedural mesh updates) from a single mapped VkBuffer. Eliminates per-update vkAllocateMemory
// (each call is 100–500µs on desktop drivers; 50 glyph quads/frame = 5–25ms wasted).
struct VertexArena
{
    std::shared_ptr<VulkanDeviceContext> deviceContext;
    VkBuffer buffer = VK_NULL_HANDLE;
    VkDeviceMemory memory = VK_NULL_HANDLE;
    void *mapped = nullptr;
    VkDeviceSize capacity = 0;
    VkDeviceSize offset = 0;
};
VertexArena g_vertexArena;

constexpr VkDeviceSize kVertexAlignment = 16;
constexpr VkDeviceSize kVertexArenaMinCapacity = 1 * 1024 * 1024;  // 1 MB seed; rotates when full

VkDeviceSize StagingAlignUp(VkDeviceSize value, VkDeviceSize align) noexcept
{
    return (value + align - 1) & ~(align - 1);
}

void RetireStagingArena() noexcept
{
    if (g_stagingArena.buffer == VK_NULL_HANDLE || g_stagingArena.deviceContext == nullptr)
    {
        g_stagingArena = {};
        return;
    }
    if (g_stagingArena.mapped != nullptr)
    {
        vkUnmapMemory(g_stagingArena.deviceContext->device, g_stagingArena.memory);
    }
    VulkanRenderState::RetireBuffer(g_stagingArena.deviceContext->device, g_stagingArena.buffer, g_stagingArena.memory);
    g_stagingArena = {};
}

bool StagingArenaAllocate(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkDeviceSize size,
                          VkBuffer &outBuffer, VkDeviceSize &outOffset, void *&outPtr)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || size == 0)
    {
        return false;
    }

    const VkDeviceSize alignedOffset = StagingAlignUp(g_stagingArena.offset, kStagingAlignment);

    if (g_stagingArena.buffer == VK_NULL_HANDLE || alignedOffset + size > g_stagingArena.capacity)
    {
        RetireStagingArena();

        VkDeviceSize newCapacity = std::max<VkDeviceSize>(size, kStagingMinCapacity);
        newCapacity = StagingAlignUp(newCapacity, kStagingAlignment);

        StagingArena fresh{};
        fresh.deviceContext = deviceContext;
        fresh.capacity = newCapacity;
        if (!CreateBufferHandle(deviceContext, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, newCapacity, fresh.buffer, fresh.memory))
        {
            return false;
        }
        if (vkMapMemory(deviceContext->device, fresh.memory, 0, VK_WHOLE_SIZE, 0, &fresh.mapped) != VK_SUCCESS || fresh.mapped == nullptr)
        {
            VulkanRenderState::RetireBuffer(deviceContext->device, fresh.buffer, fresh.memory);
            return false;
        }
        g_stagingArena = fresh;
    }

    const VkDeviceSize finalOffset = StagingAlignUp(g_stagingArena.offset, kStagingAlignment);
    outOffset = finalOffset;
    outBuffer = g_stagingArena.buffer;
    outPtr = static_cast<std::byte *>(g_stagingArena.mapped) + finalOffset;
    g_stagingArena.offset = finalOffset + size;
    return true;
}

void RetireVertexArena() noexcept
{
    if (g_vertexArena.buffer == VK_NULL_HANDLE || g_vertexArena.deviceContext == nullptr)
    {
        g_vertexArena = {};
        return;
    }
    if (g_vertexArena.mapped != nullptr)
    {
        vkUnmapMemory(g_vertexArena.deviceContext->device, g_vertexArena.memory);
    }
    VulkanRenderState::RetireBuffer(g_vertexArena.deviceContext->device, g_vertexArena.buffer, g_vertexArena.memory);
    g_vertexArena = {};
}

bool VertexArenaAllocate(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkDeviceSize size,
                         VkBuffer &outBuffer, VkDeviceSize &outOffset, void *&outPtr)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || size == 0)
    {
        return false;
    }

    const VkDeviceSize alignedOffset = StagingAlignUp(g_vertexArena.offset, kVertexAlignment);

    if (g_vertexArena.buffer == VK_NULL_HANDLE || alignedOffset + size > g_vertexArena.capacity)
    {
        // Current arena is full (or doesn't exist) — retire it (the retirement queue keeps the
        // VkBuffer alive until in-flight frames that reference it have completed) and allocate
        // a fresh, larger one.
        RetireVertexArena();

        VkDeviceSize newCapacity = std::max<VkDeviceSize>(size, kVertexArenaMinCapacity);
        newCapacity = StagingAlignUp(newCapacity, kVertexAlignment);

        VertexArena fresh{};
        fresh.deviceContext = deviceContext;
        fresh.capacity = newCapacity;
        if (!CreateBufferHandle(deviceContext, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, newCapacity, fresh.buffer, fresh.memory))
        {
            return false;
        }
        if (vkMapMemory(deviceContext->device, fresh.memory, 0, VK_WHOLE_SIZE, 0, &fresh.mapped) != VK_SUCCESS || fresh.mapped == nullptr)
        {
            VulkanRenderState::RetireBuffer(deviceContext->device, fresh.buffer, fresh.memory);
            return false;
        }
        g_vertexArena = fresh;
    }

    const VkDeviceSize finalOffset = StagingAlignUp(g_vertexArena.offset, kVertexAlignment);
    outOffset = finalOffset;
    outBuffer = g_vertexArena.buffer;
    outPtr = static_cast<std::byte *>(g_vertexArena.mapped) + finalOffset;
    g_vertexArena.offset = finalOffset + size;
    return true;
}

// Persistent descriptor cache. The original code did vkAllocateDescriptorSets +
// vkUpdateDescriptorSets per draw call from a per-frame pool. With stable scenes (e.g. text
// rendering the same glyph set across frames, terrain reusing the same material) every frame
// repeated the exact same allocation and write. We now hash the binding state at draw time and
// reuse cached sets across frames; misses still pay the alloc/update cost but hits skip both.
struct DescriptorCacheKey
{
    VkDescriptorSetLayout layout = VK_NULL_HANDLE;
    std::uint64_t bindingHash = 0;
    bool operator==(const DescriptorCacheKey &other) const noexcept
    {
        return layout == other.layout && bindingHash == other.bindingHash;
    }
};
struct DescriptorCacheKeyHash
{
    std::size_t operator()(const DescriptorCacheKey &k) const noexcept
    {
        return std::hash<void *>{}(static_cast<void *>(k.layout)) ^ (k.bindingHash * 0x9E3779B97F4A7C15ULL);
    }
};
VkDescriptorPool g_persistentDescPool = VK_NULL_HANDLE;
std::unordered_map<DescriptorCacheKey, VkDescriptorSet, DescriptorCacheKeyHash> g_descriptorCache;
std::shared_ptr<VulkanDeviceContext> g_persistentDescDevice;  // remembered for invalidation cleanup

constexpr std::uint32_t kPersistentPoolMaxSets = 4096;

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

    // Defer destruction: a recorded command buffer may still reference this VkBuffer/VkDeviceMemory
    // for the next few frames. VulkanRenderState drains the retirement queue after the matching
    // frame's fence has signalled (see DrainExpiredRetirements).
    VulkanRenderState::RetireBuffer(deviceContext->device, buffer, memory);
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

    // Cancel any deferred uploads that target this image. Their staging bytes are part of the
    // shared arena buffer so we don't free anything per-upload — the wasted bytes will be
    // reclaimed when the arena rotates at the next flush.
    if (image != VK_NULL_HANDLE)
    {
        g_pendingImageUploads.erase(
            std::remove_if(g_pendingImageUploads.begin(), g_pendingImageUploads.end(),
                           [&](const PendingImageUpload &u) { return u.image == image; }),
            g_pendingImageUploads.end());
    }

    // Drop the descriptor cache: cached sets may reference this image's view/sampler, which are
    // about to be destroyed. The cache rebuilds itself lazily on subsequent draws.
    if (imageView != VK_NULL_HANDLE || sampler != VK_NULL_HANDLE)
    {
        InvalidateDescriptorCache();
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

VkPipelineStageFlags StageMaskForLayout(VkImageLayout layout) noexcept
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_PIPELINE_STAGE_TRANSFER_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT | VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_PIPELINE_STAGE_ALL_COMMANDS_BIT;
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
            return VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
        default:
            return VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
    }
}

VkAccessFlags AccessMaskForLayout(VkImageLayout layout) noexcept
{
    switch (layout)
    {
        case VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL:
            return VK_ACCESS_TRANSFER_READ_BIT;
        case VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL:
            return VK_ACCESS_TRANSFER_WRITE_BIT;
        case VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL:
            return VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
        case VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL:
            return VK_ACCESS_SHADER_READ_BIT;
        case VK_IMAGE_LAYOUT_GENERAL:
            return VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        case VK_IMAGE_LAYOUT_UNDEFINED:
        case VK_IMAGE_LAYOUT_PRESENT_SRC_KHR:
        default:
            return 0;
    }
}

void RecordImageLayoutTransition(VkCommandBuffer commandBuffer, VkImage image, VkImageAspectFlags aspectMask, std::uint32_t mipLevels,
                                 VkImageLayout oldLayout, VkImageLayout newLayout)
{
    if (image == VK_NULL_HANDLE || oldLayout == newLayout)
    {
        return;
    }

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspectMask;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = std::max(1u, mipLevels);
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = AccessMaskForLayout(oldLayout);
    barrier.dstAccessMask = AccessMaskForLayout(newLayout);

    vkCmdPipelineBarrier(commandBuffer, StageMaskForLayout(oldLayout), StageMaskForLayout(newLayout), 0, 0, nullptr, 0, nullptr, 1, &barrier);
}

template <typename Recorder>
bool SubmitImmediateCommands(const std::shared_ptr<VulkanDeviceContext> &deviceContext, Recorder &&recorder)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || deviceContext->graphicsQueue == VK_NULL_HANDLE ||
        deviceContext->backend == nullptr)
    {
        return false;
    }

    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

    VkCommandPoolCreateInfo poolCreateInfo{};
    poolCreateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolCreateInfo.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
    poolCreateInfo.queueFamilyIndex = deviceContext->backend->graphicsQueueFamilyIndex;
    if (vkCreateCommandPool(deviceContext->device, &poolCreateInfo, nullptr, &commandPool) != VK_SUCCESS)
    {
        return false;
    }

    VkCommandBufferAllocateInfo allocateInfo{};
    allocateInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocateInfo.commandPool = commandPool;
    allocateInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocateInfo.commandBufferCount = 1;
    if (vkAllocateCommandBuffers(deviceContext->device, &allocateInfo, &commandBuffer) != VK_SUCCESS)
    {
        vkDestroyCommandPool(deviceContext->device, commandPool, nullptr);
        return false;
    }

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    if (vkBeginCommandBuffer(commandBuffer, &beginInfo) != VK_SUCCESS)
    {
        vkDestroyCommandPool(deviceContext->device, commandPool, nullptr);
        return false;
    }

    recorder(commandBuffer);

    if (vkEndCommandBuffer(commandBuffer) != VK_SUCCESS)
    {
        vkDestroyCommandPool(deviceContext->device, commandPool, nullptr);
        return false;
    }

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    const bool submitted = vkQueueSubmit(deviceContext->graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE) == VK_SUCCESS &&
                           vkQueueWaitIdle(deviceContext->graphicsQueue) == VK_SUCCESS;

    vkDestroyCommandPool(deviceContext->device, commandPool, nullptr);
    return submitted;
}

bool UploadBytesToImage(const std::shared_ptr<VulkanDeviceContext> &deviceContext, const std::vector<std::byte> &bytes, VkImage image,
                        VkImageAspectFlags aspectMask, std::uint32_t width, std::uint32_t height, std::uint32_t mipLevels,
                        VkImageLayout &currentLayout, VkImageLayout finalLayout)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || image == VK_NULL_HANDLE || bytes.empty() || width == 0 || height == 0)
    {
        return false;
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceSize stagingOffset = 0;
    void *stagingPtr = nullptr;
    if (!StagingArenaAllocate(deviceContext, bytes.size(), stagingBuffer, stagingOffset, stagingPtr))
    {
        return false;
    }
    std::memcpy(stagingPtr, bytes.data(), bytes.size());

    // Defer the GPU upload — flushed as a single batch in FlushPendingTextureUploads (called at BeginFrame).
    g_pendingImageUploads.push_back(
        {deviceContext, stagingBuffer, stagingOffset, image, aspectMask, width, height, mipLevels, currentLayout, finalLayout});
    currentLayout = finalLayout;  // optimistic: correct once the batch flushes
    return true;
}

bool ReadbackImageBytes(const std::shared_ptr<VulkanDeviceContext> &deviceContext, std::vector<std::byte> &bytes, VkImage image,
                        VkImageAspectFlags aspectMask, std::uint32_t width, std::uint32_t height, std::uint32_t mipLevels,
                        VkImageLayout &currentLayout, VkImageLayout finalLayout)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || image == VK_NULL_HANDLE || bytes.empty() || width == 0 || height == 0)
    {
        return false;
    }

    VkBuffer stagingBuffer = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    if (!CreateBufferHandle(deviceContext, VK_BUFFER_USAGE_TRANSFER_DST_BIT, bytes.size(), stagingBuffer, stagingMemory))
    {
        return false;
    }

    const bool submitted = SubmitImmediateCommands(deviceContext, [&](VkCommandBuffer commandBuffer) {
        RecordImageLayoutTransition(commandBuffer, image, aspectMask, mipLevels, currentLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);

        VkBufferImageCopy region{};
        region.imageSubresource.aspectMask = aspectMask;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {width, height, 1};
        vkCmdCopyImageToBuffer(commandBuffer, image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, stagingBuffer, 1, &region);

        RecordImageLayoutTransition(commandBuffer, image, aspectMask, mipLevels, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, finalLayout);
    });

    bool copied = false;
    if (submitted)
    {
        void *mapped = nullptr;
        copied = vkMapMemory(deviceContext->device, stagingMemory, 0, bytes.size(), 0, &mapped) == VK_SUCCESS && mapped != nullptr;
        if (copied)
        {
            std::memcpy(bytes.data(), mapped, bytes.size());
            vkUnmapMemory(deviceContext->device, stagingMemory);
            currentLayout = finalLayout;
        }
    }

    DestroyBufferHandle(deviceContext, stagingBuffer, stagingMemory);
    return copied;
}

bool BlitImage(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkImage sourceImage, VkImageAspectFlags sourceAspectMask,
               std::uint32_t sourceMipLevels, VkImageLayout &sourceLayout, VkExtent2D sourceExtent, VkImage destinationImage,
               VkImageAspectFlags destinationAspectMask, std::uint32_t destinationMipLevels, VkImageLayout &destinationLayout,
               VkExtent2D destinationExtent, VkFilter filter, VkImageLayout sourceFinalLayout, VkImageLayout destinationFinalLayout)
{
    if (deviceContext == nullptr || sourceImage == VK_NULL_HANDLE || destinationImage == VK_NULL_HANDLE ||
        sourceExtent.width == 0 || sourceExtent.height == 0 || destinationExtent.width == 0 || destinationExtent.height == 0)
    {
        return false;
    }

    const bool submitted = SubmitImmediateCommands(deviceContext, [&](VkCommandBuffer commandBuffer) {
        RecordImageLayoutTransition(commandBuffer, sourceImage, sourceAspectMask, sourceMipLevels, sourceLayout, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        RecordImageLayoutTransition(commandBuffer, destinationImage, destinationAspectMask, destinationMipLevels, destinationLayout,
                                    VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkImageBlit region{};
        region.srcSubresource.aspectMask = sourceAspectMask;
        region.srcSubresource.mipLevel = 0;
        region.srcSubresource.baseArrayLayer = 0;
        region.srcSubresource.layerCount = 1;
        region.srcOffsets[1] = {static_cast<std::int32_t>(sourceExtent.width), static_cast<std::int32_t>(sourceExtent.height), 1};
        region.dstSubresource.aspectMask = destinationAspectMask;
        region.dstSubresource.mipLevel = 0;
        region.dstSubresource.baseArrayLayer = 0;
        region.dstSubresource.layerCount = 1;
        region.dstOffsets[1] = {static_cast<std::int32_t>(destinationExtent.width), static_cast<std::int32_t>(destinationExtent.height), 1};
        vkCmdBlitImage(commandBuffer, sourceImage, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, destinationImage, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1,
                       &region, filter);

        RecordImageLayoutTransition(commandBuffer, sourceImage, sourceAspectMask, sourceMipLevels, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                    sourceFinalLayout);
        RecordImageLayoutTransition(commandBuffer, destinationImage, destinationAspectMask, destinationMipLevels, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
                                    destinationFinalLayout);
    });

    if (submitted)
    {
        sourceLayout = sourceFinalLayout;
        destinationLayout = destinationFinalLayout;
    }
    return submitted;
}

VkImageLayout GetTextureRestingLayout(const TextureDesc &desc) noexcept
{
    return desc.renderTarget ? VK_IMAGE_LAYOUT_GENERAL : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
}

} // namespace

void DestroyPersistentDescriptorCache(VkDevice device) noexcept
{
    if (g_persistentDescPool != VK_NULL_HANDLE && device != VK_NULL_HANDLE)
    {
        vkDestroyDescriptorPool(device, g_persistentDescPool, nullptr);
    }
    g_persistentDescPool = VK_NULL_HANDLE;
    g_descriptorCache.clear();
    g_persistentDescDevice.reset();
}

void FlushPendingTextureUploads(VkCommandBuffer cmd) noexcept
{
    if (g_pendingImageUploads.empty() || cmd == VK_NULL_HANDLE)
    {
        return;
    }

    // Record all pending uploads directly into the frame command buffer, before the render pass
    // begins. Pipeline barriers in RecordImageLayoutTransition guarantee the copies complete and
    // images reach SHADER_READ_ONLY before any fragment shader stage reads them.
    for (const PendingImageUpload &u : g_pendingImageUploads)
    {
        RecordImageLayoutTransition(cmd, u.image, u.aspectMask, u.mipLevels, u.fromLayout, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        VkBufferImageCopy region{};
        region.bufferOffset = u.bufferOffset;
        region.imageSubresource.aspectMask = u.aspectMask;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageExtent = {u.width, u.height, 1};
        vkCmdCopyBufferToImage(cmd, u.stagingBuffer, u.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

        RecordImageLayoutTransition(cmd, u.image, u.aspectMask, u.mipLevels, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, u.finalLayout);
    }
    g_pendingImageUploads.clear();

    // The arena's VkBuffer is now referenced by recorded copy commands. Retire the whole arena
    // so it stays alive until the GPU finishes this frame, then a new arena will be allocated
    // on demand for subsequent uploads.
    RetireStagingArena();
}

VulkanShaderProgramResource::VulkanShaderProgramResource(ShaderProgramCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName)), stageSources(std::move(createInfo.stageSources))
{
    // Pass 1 — discover the bare uniforms each stage wants in its push-constant block.
    std::vector<std::vector<VulkanPipelineCache::BareUniformInfo>> perStageBareUniforms(stageSources.size());
    for (std::size_t i = 0; i < stageSources.size(); ++i)
    {
        std::vector<VulkanPipelineCache::DiscoveredBinding> discardBindings;
        std::uint32_t discardSize = 0;
        VulkanPipelineCache::PreprocessGlsl(stageSources[i].stage, stageSources[i].sourceCode, discardBindings, discardSize,
                                            &perStageBareUniforms[i]);
    }

    // Merge — keep first-seen order, dedupe by name. (If two stages declare the same name with
    // different types we just take the first; that mirrors GLSL's "no redefinition with conflict"
    // rule and would have been a shader bug anyway.)
    std::vector<VulkanPipelineCache::BareUniformInfo> unifiedUniforms;
    for (const auto &stageList : perStageBareUniforms)
    {
        for (const VulkanPipelineCache::BareUniformInfo &u : stageList)
        {
            const auto it = std::find_if(unifiedUniforms.begin(), unifiedUniforms.end(),
                                         [&u](const VulkanPipelineCache::BareUniformInfo &existing) { return existing.name == u.name; });
            if (it == unifiedUniforms.end())
            {
                unifiedUniforms.push_back(u);
            }
        }
    }

    // Compute the canonical std140 layout — name → (offset, size).
    std::vector<VulkanPipelineCache::PushConstantUniformLayout> layout;
    VulkanPipelineCache::ComputePushConstantLayout(unifiedUniforms, layout, pushConstantSize);
    pushConstantBuffer.assign(pushConstantSize, std::byte{0});
    for (const VulkanPipelineCache::PushConstantUniformLayout &slot : layout)
    {
        pushConstantSlots[slot.name] = {slot.offset, slot.sizeBytes};
    }

    // Pass 2 — re-preprocess each stage with the *unified* push-constant block injected,
    // then compile to SPIR-V. After this all stages agree on field offsets, so a single
    // vkCmdPushConstants on the merged buffer feeds them all correctly. We compile stages
    // in parallel: glslang/shaderc is the dominant init cost; with vertex+fragment in
    // parallel that halves the wall-clock time on cold-cache runs.
    // Lambdas can't capture members directly; alias them as local references for the closure.
    const std::vector<ShaderStageSource> &sources = stageSources;
    const std::string &progDebugName = debugName;
    std::vector<VulkanPipelineCache::CompiledShaderStage> stageSlots(stageSources.size());
    std::vector<std::future<bool>> compileFutures;
    compileFutures.reserve(stageSources.size());
    for (std::size_t i = 0; i < stageSources.size(); ++i)
    {
        stageSlots[i].stage = stageSources[i].stage;
        stageSlots[i].pushConstantSizeBytes = pushConstantSize;
        compileFutures.push_back(std::async(std::launch::async, [i, &sources, &stageSlots, &unifiedUniforms, &progDebugName]() {
            const ShaderStageSource &source = sources[i];
            VulkanPipelineCache::CompiledShaderStage &slot = stageSlots[i];
            const std::string vulkanGlsl = VulkanPipelineCache::PreprocessGlslWithUnifiedPushConstants(
                source.stage, source.sourceCode, unifiedUniforms, slot.bindings);
            return VulkanPipelineCache::CompileGlslToSpirv(source.stage, vulkanGlsl, progDebugName, slot.spirv);
        }));
    }

    compiledStages.reserve(stageSources.size());
    for (std::size_t i = 0; i < compileFutures.size(); ++i)
    {
        if (compileFutures[i].get())
        {
            compiledStages.push_back(std::move(stageSlots[i]));
        }
        else
        {
            LOG_ERROR(1, "[Vulkan] Failed to compile stage for '", debugName, "' stage=", static_cast<int>(stageSources[i].stage));
        }
    }
}

GraphicsAPI VulkanShaderProgramResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string VulkanShaderProgramResource::GetDebugName() const noexcept { return debugName; }

const ShaderProgramDesc &VulkanShaderProgramResource::GetDescription() const noexcept { return desc; }

namespace
{
const VulkanShaderProgramResource *g_currentBoundProgram = nullptr;
}

const VulkanShaderProgramResource *GetCurrentBoundShaderProgram() noexcept { return g_currentBoundProgram; }

void VulkanShaderProgramResource::Bind() const
{
    g_currentBoundProgram = this;
}

void VulkanShaderProgramResource::Unbind() const
{
    if (g_currentBoundProgram == this)
    {
        g_currentBoundProgram = nullptr;
    }
}

int VulkanShaderProgramResource::GetUniformLocation(std::string name) const
{
    const auto it = uniformLocations.find(name);
    if (it != uniformLocations.end())
    {
        return it->second;
    }

    const int location = nextUniformLocation++;
    uniformLocations.emplace(name, location);

    // Build direct location→slot mapping so SetXxxUniform is O(1) (no reverse scan needed).
    const auto slotIt = pushConstantSlots.find(name);
    if (slotIt != pushConstantSlots.end())
    {
        locationToSlotCache.emplace(location, slotIt->second);
    }

    return location;
}

namespace
{
// O(1) push-constant write: locationToSlotCache is populated in GetUniformLocation the first
// time a name is queried, so by draw time the map is already warm.
void WritePushConstantBytes(const std::unordered_map<int, VulkanShaderProgramResource::PushConstantSlot> &locationToSlot,
                            std::vector<std::byte> &buffer, int location, const void *data, std::size_t sizeBytes)
{
    if (data == nullptr)
    {
        return;
    }
    const auto it = locationToSlot.find(location);
    if (it == locationToSlot.end())
    {
        return;
    }
    const VulkanShaderProgramResource::PushConstantSlot &slot = it->second;
    const std::size_t writeBytes = std::min<std::size_t>(sizeBytes, slot.sizeBytes);
    if (slot.offset + writeBytes > buffer.size())
    {
        return;
    }
    std::memcpy(buffer.data() + slot.offset, data, writeBytes);
}
} // namespace

void VulkanShaderProgramResource::SetFloatUniform(int location, const float *data, std::size_t componentCount) const
{
    if (location < 0 || data == nullptr)
    {
        return;
    }
    WritePushConstantBytes(locationToSlotCache, pushConstantBuffer, location, data, componentCount * sizeof(float));
}

void VulkanShaderProgramResource::SetIntUniform(int location, const int *data, std::size_t componentCount) const
{
    if (location < 0 || data == nullptr)
    {
        return;
    }
    WritePushConstantBytes(locationToSlotCache, pushConstantBuffer, location, data, componentCount * sizeof(int));
}

void VulkanShaderProgramResource::SetMatrix4Uniform(int location, const float *data) const
{
    if (location < 0 || data == nullptr)
    {
        return;
    }
    WritePushConstantBytes(locationToSlotCache, pushConstantBuffer, location, data, 16 * sizeof(float));
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

std::string VulkanBufferResource::GetDebugName() const noexcept { return debugName; }

const BufferDesc &VulkanBufferResource::GetDescription() const noexcept { return desc; }

void VulkanBufferResource::Bind() const {}

void VulkanBufferResource::BindToBindingPoint(std::uint32_t bindingPoint) const
{
    if (buffer == VK_NULL_HANDLE)
    {
        return;
    }
    switch (desc.usage)
    {
        case BufferUsage::Uniform:
            VulkanRenderState::RegisterBufferBinding(VulkanRenderState::BufferBindingKind::Uniform, bindingPoint, buffer, desc.sizeInBytes);
            return;
        case BufferUsage::Storage:
            VulkanRenderState::RegisterBufferBinding(VulkanRenderState::BufferBindingKind::Storage, bindingPoint, buffer, desc.sizeInBytes);
            return;
        default:
            return;
    }
}

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
    // Buffer stays persistently mapped; the actual vkUnmapMemory is deferred to DestroyBuffer.
    // HOST_COHERENT memory needs no explicit flush — the GPU sees writes immediately.
    mapped = false;
}

bool VulkanBufferResource::CreateBuffer(std::size_t sizeInBytes)
{
    DestroyBuffer();
    if (!CreateBufferHandle(deviceContext, BufferUsageToVulkan(desc.usage), sizeInBytes, buffer, memory))
    {
        return false;
    }
    // Keep buffer permanently mapped for zero-cost uploads: vkMapMemory/vkUnmapMemory per write
    // adds ~200-700ns of driver overhead each time. HOST_COHERENT guarantees GPU visibility without
    // explicit flushes, so the persistent map is both safe and optimal.
    if (deviceContext != nullptr && deviceContext->device != VK_NULL_HANDLE)
    {
        vkMapMemory(deviceContext->device, memory, 0, VK_WHOLE_SIZE, 0, &mappedData);
    }
    return true;
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
    if (size == 0 || offset >= storage.size())
    {
        return;
    }

    if (mappedData != nullptr)
    {
        // Fast path: direct write into the persistently-mapped pointer, zero driver overhead.
        std::memcpy(static_cast<std::byte *>(mappedData) + offset, storage.data() + offset, size);
        return;
    }

    // Fallback (should not happen after CreateBuffer succeeds): map from offset 0 to avoid
    // alignment issues with partial maps (minMemoryMapAlignment is typically 64 bytes).
    if (memory == VK_NULL_HANDLE || deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        return;
    }
    void *gpuData = nullptr;
    if (vkMapMemory(deviceContext->device, memory, 0, VK_WHOLE_SIZE, 0, &gpuData) != VK_SUCCESS || gpuData == nullptr)
    {
        return;
    }
    std::memcpy(static_cast<std::byte *>(gpuData) + offset, storage.data() + offset, size);
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

std::string VulkanGeometryResource::GetDebugName() const noexcept { return debugName; }

const GeometryLayout &VulkanGeometryResource::GetLayout() const noexcept { return layout; }

std::size_t VulkanGeometryResource::GetIndexCount() const noexcept { return indexData.size(); }

std::size_t VulkanGeometryResource::GetInstanceCount() const noexcept
{
    if (instanceData.empty() || layout.instanceAttributes.empty())
    {
        return 1;
    }

    std::uint32_t instanceStride = 0;
    for (const std::uint32_t size : layout.instanceAttributes)
    {
        instanceStride += size;
    }

    return instanceStride == 0 ? 0 : instanceData.size() / instanceStride;
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
    const bool needsGrow = requiredFloats > vertexData.size();
    if (needsGrow)
    {
        vertexData.resize(requiredFloats);
    }

    std::copy_n(data, floatCount, vertexData.begin() + static_cast<std::ptrdiff_t>(offsetFloats));

    // If a frame is being recorded, the current VkBuffer may already be referenced by recorded
    // draw commands that have not yet executed. A host-side memcpy into the same buffer would
    // make those earlier draws read this update's data on the GPU (single-buffered aliasing —
    // see TextRenderer::renderText, which calls UpdateVertexData + DrawIndexed in a loop on the
    // same geometry). Allocate a fresh vertex buffer in that case so each in-flight draw sees
    // the vertices that were current at record time. Index/instance buffers are untouched.
    const bool insideFrame = VulkanRenderState::GetCurrentCommandBuffer() != VK_NULL_HANDLE;
    if (needsGrow || vertexBuffer == VK_NULL_HANDLE || insideFrame)
    {
        RecreateVertexBuffer();
    }
    else
    {
        UploadBuffer(vertexBuffer, vertexMemory, data, floatCount * sizeof(float), offsetFloats * sizeof(float));
    }
}

void VulkanGeometryResource::UpdateInstanceData(const float *data, std::size_t floatCount, std::size_t offsetFloats)
{
    if (data == nullptr || floatCount == 0)
    {
        return;
    }

    const std::size_t requiredFloats = offsetFloats + floatCount;
    const bool needsGrow = requiredFloats > instanceData.size();
    if (needsGrow)
    {
        instanceData.resize(requiredFloats);
    }

    std::copy_n(data, floatCount, instanceData.begin() + static_cast<std::ptrdiff_t>(offsetFloats));

    // See UpdateVertexData for rationale — in-place updates would alias across in-flight draws.
    const bool insideFrame = VulkanRenderState::GetCurrentCommandBuffer() != VK_NULL_HANDLE;
    if (needsGrow || instanceBuffer == VK_NULL_HANDLE || insideFrame)
    {
        RecreateInstanceBuffer();
    }
    else
    {
        UploadBuffer(instanceBuffer, instanceMemory, data, floatCount * sizeof(float), offsetFloats * sizeof(float));
    }
}

namespace
{

const VulkanPipelineCache::PipelineEntry *EnsurePipelineFor(const std::shared_ptr<VulkanDeviceContext> &deviceContext,
                                                            const GeometryLayout &layout, const VulkanShaderProgramResource *program)
{
    if (program == nullptr || program->GetCompiledStages().empty())
    {
        return nullptr;
    }

    VkRenderPass renderPass = VulkanRenderState::GetCurrentRenderPass();
    if (renderPass == VK_NULL_HANDLE)
    {
        renderPass = VulkanWindowContext::GetAnySwapchainRenderPass();
    }
    if (renderPass == VK_NULL_HANDLE)
    {
        return nullptr;
    }

    const VulkanRenderState::FramebufferState fbState = VulkanRenderState::CaptureFramebufferState();

    VulkanPipelineCache::PipelineKey key;
    key.programResource = program;
    key.vertexAttributes = layout.vertexAttributes;
    key.renderPass = renderPass;
    key.depthTest = fbState.depthTest;
    key.depthWrite = fbState.depthTest;  // depth write follows depth test (matches default OpenGL behavior)
    key.blendEnable = fbState.blend;

    return VulkanPipelineCache::GetOrCreatePipeline(deviceContext, key, program->GetCompiledStages());
}

bool AllBindingsSatisfiable(const std::vector<VulkanPipelineCache::DiscoveredBinding> &bindings)
{
    for (const VulkanPipelineCache::DiscoveredBinding &binding : bindings)
    {
        if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            if (VulkanRenderState::GetStorageBufferAt(binding.binding) == VK_NULL_HANDLE)
            {
                return false;
            }
        }
        else if (binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            if (VulkanRenderState::GetUniformBufferAt(binding.binding) == VK_NULL_HANDLE)
            {
                return false;
            }
        }
        else if (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            if (!VulkanRenderState::GetTextureBinding(binding.binding, nullptr, nullptr))
            {
                return false;
            }
        }
        else
        {
            return false;
        }
    }
    return true;
}

bool EnsurePersistentDescriptorPool(const std::shared_ptr<VulkanDeviceContext> &deviceContext)
{
    if (g_persistentDescPool != VK_NULL_HANDLE)
    {
        return true;
    }
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        return false;
    }
    const VkDescriptorPoolSize poolSizes[] = {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, kPersistentPoolMaxSets * 4},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, kPersistentPoolMaxSets * 4},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, kPersistentPoolMaxSets * 8},
    };
    VkDescriptorPoolCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    info.flags = 0;
    info.maxSets = kPersistentPoolMaxSets;
    info.poolSizeCount = static_cast<std::uint32_t>(std::size(poolSizes));
    info.pPoolSizes = poolSizes;
    if (vkCreateDescriptorPool(deviceContext->device, &info, nullptr, &g_persistentDescPool) != VK_SUCCESS)
    {
        g_persistentDescPool = VK_NULL_HANDLE;
        return false;
    }
    g_persistentDescDevice = deviceContext;
    return true;
}

// Hash the live binding state. The resulting key, combined with the descriptor set layout,
// uniquely identifies a descriptor set's contents — hash collisions are improbable enough
// (FNV-1a 64-bit over handle pointers) that we skip the linear-equality check.
std::uint64_t HashCurrentBindingState(const std::vector<VulkanPipelineCache::DiscoveredBinding> &bindings) noexcept
{
    std::uint64_t hash = 14695981039346656037ULL;
    auto mix = [&hash](std::uint64_t v) noexcept {
        hash ^= v;
        hash *= 1099511628211ULL;
    };
    for (const VulkanPipelineCache::DiscoveredBinding &binding : bindings)
    {
        mix(static_cast<std::uint64_t>(binding.binding) | (static_cast<std::uint64_t>(binding.type) << 32));
        if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
        {
            mix(reinterpret_cast<std::uint64_t>(VulkanRenderState::GetStorageBufferAt(binding.binding)));
        }
        else if (binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            mix(reinterpret_cast<std::uint64_t>(VulkanRenderState::GetUniformBufferAt(binding.binding)));
        }
        else if (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            VkImageView view = VK_NULL_HANDLE;
            VkSampler sampler = VK_NULL_HANDLE;
            VulkanRenderState::GetTextureBinding(binding.binding, &view, &sampler);
            mix(reinterpret_cast<std::uint64_t>(view));
            mix(reinterpret_cast<std::uint64_t>(sampler));
        }
    }
    return hash;
}

// Drop every cached descriptor set. Called when an underlying VkImageView/VkSampler/VkBuffer
// referenced by the cache is about to be destroyed (or any of those handles could be reused
// for a different resource later). Resetting the pool wholesale is O(1) and far simpler than
// scanning the cache for matching entries; cache rebuild cost is paid lazily on next draws.
void InvalidateDescriptorCache() noexcept
{
    if (g_persistentDescPool != VK_NULL_HANDLE && g_persistentDescDevice != nullptr &&
        g_persistentDescDevice->device != VK_NULL_HANDLE)
    {
        vkResetDescriptorPool(g_persistentDescDevice->device, g_persistentDescPool, 0);
    }
    g_descriptorCache.clear();
}

void UpdateDescriptorSet(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkDescriptorSet set,
                         const std::vector<VulkanPipelineCache::DiscoveredBinding> &bindings)
{
    if (set == VK_NULL_HANDLE || deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        return;
    }

    // thread_local fixed-size arrays eliminate the three std::vector heap allocations that the
    // old code paid on every descriptor-set write (which happened on every cache miss). Shaders
    // realistically use at most a handful of bindings; 16 is a comfortable upper bound.
    constexpr std::uint32_t kMaxBindings = 16;
    thread_local VkDescriptorBufferInfo bufferInfos[kMaxBindings];
    thread_local VkDescriptorImageInfo imageInfos[kMaxBindings];
    thread_local VkWriteDescriptorSet writes[kMaxBindings];
    std::uint32_t bufferCount = 0, imageCount = 0, writeCount = 0;

    for (const VulkanPipelineCache::DiscoveredBinding &binding : bindings)
    {
        if (writeCount >= kMaxBindings)
        {
            break;
        }
        if (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER || binding.type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER)
        {
            if (bufferCount >= kMaxBindings)
            {
                continue;
            }
            VkDeviceSize size = 0;
            const VkBuffer buf = (binding.type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER)
                                     ? VulkanRenderState::GetStorageBufferAt(binding.binding, &size)
                                     : VulkanRenderState::GetUniformBufferAt(binding.binding, &size);
            if (buf == VK_NULL_HANDLE)
            {
                continue;
            }

            bufferInfos[bufferCount] = {buf, 0, size > 0 ? size : VK_WHOLE_SIZE};

            VkWriteDescriptorSet &write = writes[writeCount];
            write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set;
            write.dstBinding = binding.binding;
            write.descriptorType = binding.type;
            write.descriptorCount = 1;
            write.pBufferInfo = &bufferInfos[bufferCount];
            ++bufferCount;
            ++writeCount;
        }
        else if (binding.type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER)
        {
            if (imageCount >= kMaxBindings)
            {
                continue;
            }
            VkImageView view = VK_NULL_HANDLE;
            VkSampler sampler = VK_NULL_HANDLE;
            if (!VulkanRenderState::GetTextureBinding(binding.binding, &view, &sampler) || view == VK_NULL_HANDLE ||
                sampler == VK_NULL_HANDLE)
            {
                continue;
            }

            imageInfos[imageCount] = {sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};

            VkWriteDescriptorSet &write = writes[writeCount];
            write = {};
            write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            write.dstSet = set;
            write.dstBinding = binding.binding;
            write.descriptorType = binding.type;
            write.descriptorCount = 1;
            write.pImageInfo = &imageInfos[imageCount];
            ++imageCount;
            ++writeCount;
        }
    }

    if (writeCount > 0)
    {
        vkUpdateDescriptorSets(deviceContext->device, writeCount, writes, 0, nullptr);
    }
}

bool RecordDrawSetup(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkCommandBuffer cmd, const GeometryLayout &layout,
                     const VulkanShaderProgramResource *program, VkBuffer vertexBuffer, VkDeviceSize vertexOffset, VkBuffer indexBuffer,
                     const VulkanPipelineCache::PipelineEntry **outEntry)
{
    if (program == nullptr)
    {
        return false;
    }
    const VulkanPipelineCache::PipelineEntry *entry = EnsurePipelineFor(deviceContext, layout, program);
    if (entry == nullptr)
    {
        return false;
    }

    // Gate the draw on having all bindings satisfiable. Sampler/image bindings are not yet wired
    // up to texture state, so any pipeline using them would read uninitialized descriptors and
    // likely cause device loss. Skip silently rather than crash the GPU.
    if (!AllBindingsSatisfiable(entry->mergedBindings))
    {
        return false;
    }

    *outEntry = entry;

    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, entry->pipeline);

    if (!entry->mergedBindings.empty())
    {
        // Cache hit: reuse a previously-allocated descriptor set with identical bindings,
        // skipping both vkAllocateDescriptorSets and vkUpdateDescriptorSets. For stable scenes
        // (TextRenderer drawing the same glyph set every frame, terrain reusing the same
        // material) this collapses the per-draw cost to a single vkCmdBindDescriptorSets.
        VkDescriptorSet set = VK_NULL_HANDLE;
        if (EnsurePersistentDescriptorPool(deviceContext))
        {
            const std::uint64_t bindingHash = HashCurrentBindingState(entry->mergedBindings);
            const DescriptorCacheKey cacheKey{entry->descriptorSetLayout, bindingHash};
            if (auto it = g_descriptorCache.find(cacheKey); it != g_descriptorCache.end())
            {
                set = it->second;
            }
            else
            {
                VkDescriptorSetAllocateInfo allocInfo{};
                allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
                allocInfo.descriptorPool = g_persistentDescPool;
                allocInfo.descriptorSetCount = 1;
                allocInfo.pSetLayouts = &entry->descriptorSetLayout;
                if (vkAllocateDescriptorSets(deviceContext->device, &allocInfo, &set) == VK_SUCCESS)
                {
                    UpdateDescriptorSet(deviceContext, set, entry->mergedBindings);
                    g_descriptorCache.emplace(cacheKey, set);
                }
                else
                {
                    // Pool exhausted — fall back to the per-frame transient pool. Drop the
                    // persistent cache so the next frame can rebuild from a clean slate.
                    InvalidateDescriptorCache();
                    set = VulkanPipelineCache::AllocateFrameDescriptorSet(deviceContext, entry->descriptorSetLayout);
                    if (set != VK_NULL_HANDLE)
                    {
                        UpdateDescriptorSet(deviceContext, set, entry->mergedBindings);
                    }
                }
            }
        }

        if (set != VK_NULL_HANDLE)
        {
            vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, entry->pipelineLayout, 0, 1, &set, 0, nullptr);
        }
        else
        {
            LOG_ERROR(1, "[Vulkan] Draw will proceed without descriptor set: descriptor allocation failed");
        }
    }

    if (entry->pushConstantSizeBytes > 0)
    {
        const VulkanShaderProgramResource *boundProgram = GetCurrentBoundShaderProgram();
        if (boundProgram != nullptr && boundProgram->GetPushConstantSize() > 0)
        {
            const std::vector<std::byte> &data = boundProgram->GetPushConstantData();
            const std::uint32_t pushSize = std::min<std::uint32_t>(entry->pushConstantSizeBytes, static_cast<std::uint32_t>(data.size()));
            vkCmdPushConstants(cmd, entry->pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, pushSize,
                               data.data());
        }
    }

    if (vertexBuffer != VK_NULL_HANDLE)
    {
        vkCmdBindVertexBuffers(cmd, 0, 1, &vertexBuffer, &vertexOffset);
    }
    if (indexBuffer != VK_NULL_HANDLE)
    {
        vkCmdBindIndexBuffer(cmd, indexBuffer, 0, VK_INDEX_TYPE_UINT32);
    }
    return true;
}

} // namespace

void VulkanGeometryResource::DrawIndexed() const
{
    VkCommandBuffer cmd = VulkanRenderState::GetCurrentCommandBuffer();
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }
    if (indexData.empty() || vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    const VulkanPipelineCache::PipelineEntry *entry = nullptr;
    if (!RecordDrawSetup(deviceContext, cmd, layout, GetCurrentBoundShaderProgram(), vertexBuffer, vertexBufferOffset, indexBuffer, &entry))
    {
        return;
    }

    vkCmdDrawIndexed(cmd, static_cast<std::uint32_t>(indexData.size()), 1, 0, 0, 0);
}

void VulkanGeometryResource::DrawIndexedInstanced() const
{
    VkCommandBuffer cmd = VulkanRenderState::GetCurrentCommandBuffer();
    if (cmd == VK_NULL_HANDLE || indexData.empty() || vertexBuffer == VK_NULL_HANDLE || indexBuffer == VK_NULL_HANDLE)
    {
        return;
    }

    const VulkanPipelineCache::PipelineEntry *entry = nullptr;
    if (!RecordDrawSetup(deviceContext, cmd, layout, GetCurrentBoundShaderProgram(), vertexBuffer, vertexBufferOffset, indexBuffer, &entry))
    {
        return;
    }

    vkCmdDrawIndexed(cmd, static_cast<std::uint32_t>(indexData.size()), static_cast<std::uint32_t>(GetInstanceCount()), 0, 0, 0);
}

void VulkanGeometryResource::DrawVertices(std::size_t vertexCount) const
{
    VkCommandBuffer cmd = VulkanRenderState::GetCurrentCommandBuffer();
    if (cmd == VK_NULL_HANDLE || vertexBuffer == VK_NULL_HANDLE || vertexCount == 0)
    {
        return;
    }

    const VulkanPipelineCache::PipelineEntry *entry = nullptr;
    if (!RecordDrawSetup(deviceContext, cmd, layout, GetCurrentBoundShaderProgram(), vertexBuffer, vertexBufferOffset, VK_NULL_HANDLE, &entry))
    {
        return;
    }

    vkCmdDraw(cmd, static_cast<std::uint32_t>(vertexCount), 1, 0, 0);
}

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

bool VulkanGeometryResource::RecreateVertexBuffer()
{
    // Release the previous vertex buffer reference. Arena-backed slices aren't owned by us — the
    // arena's VkBuffer is shared and lives in the retirement queue when rotated, so we just drop
    // the handle. Owned buffers go through the deferred-destroy path.
    if (vertexBufferFromArena)
    {
        vertexBuffer = VK_NULL_HANDLE;
        vertexMemory = VK_NULL_HANDLE;
    }
    else
    {
        DestroyBufferHandle(deviceContext, vertexBuffer, vertexMemory);
    }
    vertexBufferOffset = 0;
    vertexBufferFromArena = false;

    const std::size_t vertexBytes = vertexData.size() * sizeof(float);
    if (vertexBytes == 0)
    {
        return true;
    }

    // In-frame: bump-allocate from the shared transient arena instead of paying for a fresh
    // vkCreateBuffer + vkAllocateMemory + vkBindBufferMemory per call (TextRenderer hits this
    // path once per glyph). Out-of-frame (init or static updates): fall back to an owned buffer.
    if (VulkanRenderState::GetCurrentCommandBuffer() != VK_NULL_HANDLE)
    {
        VkBuffer arenaBuffer = VK_NULL_HANDLE;
        VkDeviceSize arenaOffset = 0;
        void *arenaPtr = nullptr;
        if (!VertexArenaAllocate(deviceContext, vertexBytes, arenaBuffer, arenaOffset, arenaPtr))
        {
            return false;
        }
        std::memcpy(arenaPtr, vertexData.data(), vertexBytes);
        vertexBuffer = arenaBuffer;
        vertexMemory = VK_NULL_HANDLE;  // arena owns the memory
        vertexBufferOffset = arenaOffset;
        vertexBufferFromArena = true;
        return true;
    }

    if (!CreateBufferHandle(deviceContext, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, vertexBytes, vertexBuffer, vertexMemory))
    {
        return false;
    }
    UploadBuffer(vertexBuffer, vertexMemory, vertexData.data(), vertexBytes);
    return true;
}

bool VulkanGeometryResource::RecreateInstanceBuffer()
{
    if (instanceBufferFromArena)
    {
        instanceBuffer = VK_NULL_HANDLE;
        instanceMemory = VK_NULL_HANDLE;
    }
    else
    {
        DestroyBufferHandle(deviceContext, instanceBuffer, instanceMemory);
    }
    instanceBufferOffset = 0;
    instanceBufferFromArena = false;

    const std::size_t instanceBytes = instanceData.size() * sizeof(float);
    if (instanceBytes == 0)
    {
        return true;
    }

    if (VulkanRenderState::GetCurrentCommandBuffer() != VK_NULL_HANDLE)
    {
        VkBuffer arenaBuffer = VK_NULL_HANDLE;
        VkDeviceSize arenaOffset = 0;
        void *arenaPtr = nullptr;
        if (!VertexArenaAllocate(deviceContext, instanceBytes, arenaBuffer, arenaOffset, arenaPtr))
        {
            return false;
        }
        std::memcpy(arenaPtr, instanceData.data(), instanceBytes);
        instanceBuffer = arenaBuffer;
        instanceMemory = VK_NULL_HANDLE;
        instanceBufferOffset = arenaOffset;
        instanceBufferFromArena = true;
        return true;
    }

    if (!CreateBufferHandle(deviceContext, VK_BUFFER_USAGE_VERTEX_BUFFER_BIT, instanceBytes, instanceBuffer, instanceMemory))
    {
        return false;
    }
    UploadBuffer(instanceBuffer, instanceMemory, instanceData.data(), instanceBytes);
    return true;
}

void VulkanGeometryResource::DestroyBuffers() noexcept
{
    // Arena-backed slices reference the shared arena buffer — never retire it through this path
    // (the arena owns its lifecycle); just drop the handle.
    if (vertexBufferFromArena)
    {
        vertexBuffer = VK_NULL_HANDLE;
        vertexMemory = VK_NULL_HANDLE;
    }
    else
    {
        DestroyBufferHandle(deviceContext, vertexBuffer, vertexMemory);
    }
    vertexBufferOffset = 0;
    vertexBufferFromArena = false;

    DestroyBufferHandle(deviceContext, indexBuffer, indexMemory);

    if (instanceBufferFromArena)
    {
        instanceBuffer = VK_NULL_HANDLE;
        instanceMemory = VK_NULL_HANDLE;
    }
    else
    {
        DestroyBufferHandle(deviceContext, instanceBuffer, instanceMemory);
    }
    instanceBufferOffset = 0;
    instanceBufferFromArena = false;
}

void VulkanGeometryResource::UploadBuffer(VkBuffer bufferHandle, VkDeviceMemory memoryHandle, const void *data, std::size_t size, std::size_t byteOffset) const
{
    static_cast<void>(bufferHandle);
    if (memoryHandle == VK_NULL_HANDLE || data == nullptr || size == 0 || deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        return;
    }

    void *gpuData = nullptr;
    // Map from offset 0: vkMapMemory requires the offset to be a multiple of minMemoryMapAlignment
    // (typically 64 bytes), which float-based offsets do not guarantee. Mapping the whole allocation
    // and adjusting the destination pointer sidesteps the alignment constraint entirely.
    if (vkMapMemory(deviceContext->device, memoryHandle, 0, VK_WHOLE_SIZE, 0, &gpuData) != VK_SUCCESS || gpuData == nullptr)
    {
        return;
    }

    std::memcpy(static_cast<std::byte *>(gpuData) + byteOffset, data, size);
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

std::string VulkanTextureResource::GetDebugName() const noexcept { return debugName; }

const TextureDesc &VulkanTextureResource::GetDescription() const noexcept { return desc; }

void VulkanTextureResource::Bind(std::uint32_t slot) const
{
    // The GLSL preprocessor (VulkanPipelineCache::PreprocessGlsl) auto-assigns sampler bindings
    // starting at kFirstSamplerBinding (= 8) in shader source order. We mirror that mapping here:
    // engine slot N → descriptor binding 8 + N. Typical engines bind their N-th sampler to slot N,
    // so this matches.
    constexpr std::uint32_t kFirstSamplerBinding = 8;
    // Skip empty textures so they don't wipe a previously-bound slot (e.g. a glyph for ' '
    // with no view/sampler must not erase the binding the next glyph relies on).
    if (imageView == VK_NULL_HANDLE || sampler == VK_NULL_HANDLE)
    {
        return;
    }
    VulkanRenderState::RegisterTextureBinding(kFirstSamplerBinding + slot, imageView, sampler);
}

void VulkanTextureResource::Unbind() const {}

void VulkanTextureResource::Readback(std::vector<std::byte> &output) const
{
    ReadbackImageToStorage();
    output = storage;
}

void VulkanTextureResource::Resize(std::uint32_t width, std::uint32_t height)
{
    DestroyImage();
    desc.extent = {width, height};
    const std::size_t pixelCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
    storage.resize(pixelCount * GetPixelSize(desc.format));
    if (CreateImage() && !desc.renderTarget && !storage.empty())
    {
        UploadStorageToImage();
    }
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

    currentLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    return true;
}

bool VulkanTextureResource::UploadStorageToImage()
{
    return UploadBytesToImage(deviceContext, storage, image, ToAspectMask(desc.format), desc.extent.width, desc.extent.height,
                              std::max(1u, desc.mipLevels), currentLayout, GetTextureRestingLayout(desc));
}

bool VulkanTextureResource::ReadbackImageToStorage() const
{
    return ReadbackImageBytes(deviceContext, storage, image, ToAspectMask(desc.format), desc.extent.width, desc.extent.height,
                              std::max(1u, desc.mipLevels), currentLayout, GetTextureRestingLayout(desc));
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

VulkanRenderTargetResource::~VulkanRenderTargetResource()
{
    DestroyOffscreenResources();
    UnregisterSelf();
}

GraphicsAPI VulkanRenderTargetResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string VulkanRenderTargetResource::GetDebugName() const noexcept { return debugName; }

const RenderTargetDesc &VulkanRenderTargetResource::GetDescription() const noexcept { return desc; }

void VulkanRenderTargetResource::Bind() const
{
    VkCommandBuffer cmd = VulkanRenderState::GetCurrentCommandBuffer();
    if (cmd == VK_NULL_HANDLE)
    {
        return;
    }

    const VulkanTextureResource *color = colorAttachments.empty() ? nullptr : colorAttachments[0];
    if (color == nullptr || color->imageView == VK_NULL_HANDLE)
    {
        return;
    }

    if (offscreenRenderPass == VK_NULL_HANDLE || offscreenFramebuffer == VK_NULL_HANDLE)
    {
        if (!const_cast<VulkanRenderTargetResource *>(this)->CreateOffscreenResources())
        {
            return;
        }
    }

    VkRenderPass targetRenderPass = offscreenRenderPass;
    VkClearValue clearValues[2]{};
    clearValues[0].color = {0.0f, 0.0f, 0.0f, 0.0f};
    clearValues[1].depthStencil = {1.0f, 0};
    const bool hasDepth = depthAttachment != nullptr && depthAttachment->imageView != VK_NULL_HANDLE;
    const bool resumeExistingContents = color->currentLayout != VK_IMAGE_LAYOUT_UNDEFINED;
    if (resumeExistingContents && offscreenRenderPassLoad != VK_NULL_HANDLE)
    {
        targetRenderPass = offscreenRenderPassLoad;
    }

    const std::uint32_t previousFramebuffer = VulkanRenderState::CaptureFramebufferState().framebuffer;
    if (previousFramebuffer != 0 && previousFramebuffer != handle)
    {
        if (VulkanRenderTargetResource *previousTarget = VulkanRenderTargetResource::FindByHandle(previousFramebuffer); previousTarget != nullptr)
        {
            for (const VulkanTextureResource *attachment : previousTarget->colorAttachments)
            {
                if (attachment != nullptr)
                {
                    attachment->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
                }
            }
            if (previousTarget->depthAttachment != nullptr && previousTarget->depthAttachment->imageView != VK_NULL_HANDLE)
            {
                previousTarget->depthAttachment->currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }
        }
    }

    if (VulkanRenderState::GetCurrentRenderPass() != VK_NULL_HANDLE)
    {
        vkCmdEndRenderPass(cmd);
    }

    VkRenderPassBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    beginInfo.renderPass = targetRenderPass;
    beginInfo.framebuffer = offscreenFramebuffer;
    beginInfo.renderArea.extent = {color->desc.extent.width, color->desc.extent.height};
    beginInfo.clearValueCount = resumeExistingContents ? 0u : (hasDepth ? 2u : 1u);
    beginInfo.pClearValues = resumeExistingContents ? nullptr : clearValues;

    vkCmdBeginRenderPass(cmd, &beginInfo, VK_SUBPASS_CONTENTS_INLINE);

    const float w = static_cast<float>(color->desc.extent.width);
    const float h = static_cast<float>(color->desc.extent.height);
    VkViewport viewport{0.0f, h, w, -h, 0.0f, 1.0f};
    vkCmdSetViewport(cmd, 0, 1, &viewport);

    VkRect2D scissor{{0, 0}, {color->desc.extent.width, color->desc.extent.height}};
    vkCmdSetScissor(cmd, 0, 1, &scissor);

    VulkanRenderState::SetFramebuffer(handle);
    VulkanRenderState::SetCurrentRenderPass(targetRenderPass);
}

void VulkanRenderTargetResource::Unbind() const
{
    VkCommandBuffer cmd = VulkanRenderState::GetCurrentCommandBuffer();
    if (cmd == VK_NULL_HANDLE || offscreenRenderPass == VK_NULL_HANDLE)
    {
        VulkanRenderState::SetFramebuffer(0);
        VulkanRenderState::SetCurrentRenderPass(VK_NULL_HANDLE);
        return;
    }

    if (VulkanRenderState::GetCurrentRenderPass() == VK_NULL_HANDLE)
    {
        VulkanRenderState::SetFramebuffer(0);
        VulkanRenderState::SetCurrentRenderPass(VK_NULL_HANDLE);
        return;
    }

    vkCmdEndRenderPass(cmd);
    VulkanRenderState::SetCurrentRenderPass(VK_NULL_HANDLE);

    // render pass finalLayout transitions color attachments to SHADER_READ_ONLY_OPTIMAL
    for (const VulkanTextureResource *attachment : colorAttachments)
    {
        if (attachment != nullptr)
        {
            attachment->currentLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        }
    }
    if (depthAttachment != nullptr && depthAttachment->imageView != VK_NULL_HANDLE)
    {
        depthAttachment->currentLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VulkanRenderState::SetFramebuffer(0);
    VulkanWindowContext::ResumeSwapchainRenderPass();
}

void VulkanRenderTargetResource::Resize(std::uint32_t width, std::uint32_t height)
{
    desc.extent = {width, height};
    DestroyOffscreenResources();
}

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
    const auto *vulkanDestination = dynamic_cast<const VulkanRenderTargetResource *>(&destination);
    if (vulkanDestination == nullptr)
    {
        return;
    }

    const std::uint32_t attachmentCount =
        std::min(activeColorAttachmentCount, std::min(vulkanDestination->activeColorAttachmentCount,
                                                      static_cast<std::uint32_t>(std::min(colorAttachments.size(), vulkanDestination->colorAttachments.size()))));
    for (std::uint32_t i = 0; i < attachmentCount; ++i)
    {
        const VulkanTextureResource *sourceAttachment = colorAttachments[i];
        const VulkanTextureResource *destinationAttachment = vulkanDestination->colorAttachments[i];
        if (sourceAttachment == nullptr || destinationAttachment == nullptr)
        {
            continue;
        }

        VkExtent2D sourceExtent{std::min(srcWidth, sourceAttachment->desc.extent.width), std::min(srcHeight, sourceAttachment->desc.extent.height)};
        VkExtent2D destinationExtent{std::min(dstWidth, destinationAttachment->desc.extent.width),
                                     std::min(dstHeight, destinationAttachment->desc.extent.height)};
        BlitImage(deviceContext, sourceAttachment->image, VulkanTextureResource::ToAspectMask(sourceAttachment->desc.format),
                  std::max(1u, sourceAttachment->desc.mipLevels), sourceAttachment->currentLayout, sourceExtent, destinationAttachment->image,
                  VulkanTextureResource::ToAspectMask(destinationAttachment->desc.format), std::max(1u, destinationAttachment->desc.mipLevels),
                  destinationAttachment->currentLayout, destinationExtent, VK_FILTER_NEAREST, GetTextureRestingLayout(sourceAttachment->desc),
                  GetTextureRestingLayout(destinationAttachment->desc));
    }
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

std::uint32_t VulkanRenderTargetResource::ReadPixelUInt(std::uint32_t attachmentIndex, int x, int y, int framebufferHeight) const
{
    const VulkanTextureResource *attachment = GetColorAttachment(attachmentIndex);
    if (attachment == nullptr)
    {
        return 0;
    }

    std::vector<std::byte> bytes;
    attachment->Readback(bytes);
    const std::uint32_t width = attachment->desc.extent.width;
    const std::uint32_t height = attachment->desc.extent.height;
    if (bytes.empty() || x < 0 || y < 0 || width == 0 || height == 0)
    {
        return 0;
    }

    const int flippedY = std::clamp(framebufferHeight - 1 - y, 0, static_cast<int>(height) - 1);
    const std::uint32_t clampedX = static_cast<std::uint32_t>(std::clamp(x, 0, static_cast<int>(width) - 1));
    const std::size_t pixelIndex = static_cast<std::size_t>(flippedY) * width + clampedX;

    if (attachment->desc.format == TextureFormat::R32UI)
    {
        const std::size_t offset = pixelIndex * sizeof(std::uint32_t);
        if (offset + sizeof(std::uint32_t) <= bytes.size())
        {
            std::uint32_t value = 0;
            std::memcpy(&value, bytes.data() + offset, sizeof(value));
            return value;
        }
    }

    if (attachment->desc.format == TextureFormat::R8)
    {
        return pixelIndex < bytes.size() ? static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[pixelIndex])) : 0;
    }

    const std::size_t offset = pixelIndex * 4;
    return offset < bytes.size() ? static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])) : 0;
}

glm::uvec4 VulkanRenderTargetResource::ReadPixelRGBA8(std::uint32_t attachmentIndex, int x, int y, int framebufferHeight) const
{
    const VulkanTextureResource *attachment = GetColorAttachment(attachmentIndex);
    if (attachment == nullptr)
    {
        return glm::uvec4(0);
    }

    std::vector<std::byte> bytes;
    attachment->Readback(bytes);
    const std::uint32_t width = attachment->desc.extent.width;
    const std::uint32_t height = attachment->desc.extent.height;
    if (bytes.empty() || x < 0 || y < 0 || width == 0 || height == 0)
    {
        return glm::uvec4(0);
    }

    const int flippedY = std::clamp(framebufferHeight - 1 - y, 0, static_cast<int>(height) - 1);
    const std::uint32_t clampedX = static_cast<std::uint32_t>(std::clamp(x, 0, static_cast<int>(width) - 1));
    const std::size_t pixelIndex = static_cast<std::size_t>(flippedY) * width + clampedX;

    if (attachment->desc.format == TextureFormat::R8)
    {
        const std::uint32_t value =
            pixelIndex < bytes.size() ? static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[pixelIndex])) : 0;
        return glm::uvec4(value, 0, 0, 255);
    }

    if (attachment->desc.format == TextureFormat::R32UI)
    {
        return glm::uvec4(ReadPixelUInt(attachmentIndex, x, y, framebufferHeight), 0, 0, 255);
    }

    const std::size_t offset = pixelIndex * 4;
    if (offset + 4 > bytes.size())
    {
        return glm::uvec4(0);
    }

    if (attachment->desc.format == TextureFormat::BGRA8)
    {
        return glm::uvec4(static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2])),
                          static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1])),
                          static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])),
                          static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3])));
    }

    return glm::uvec4(static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset])),
                      static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 1])),
                      static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 2])),
                      static_cast<std::uint32_t>(std::to_integer<unsigned char>(bytes[offset + 3])));
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
    DestroyOffscreenResources();
}

void VulkanRenderTargetResource::RegisterDepthAttachment(const VulkanTextureResource *texture) noexcept
{
    depthAttachment = texture;
    DestroyOffscreenResources();
}

const VulkanTextureResource *VulkanRenderTargetResource::GetColorAttachment(std::uint32_t attachmentIndex) const noexcept
{
    return attachmentIndex < colorAttachments.size() ? colorAttachments[attachmentIndex] : nullptr;
}

bool VulkanRenderTargetResource::CreateOffscreenResources() noexcept
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        return false;
    }

    const VulkanTextureResource *color = colorAttachments.empty() ? nullptr : colorAttachments[0];
    if (color == nullptr || color->imageView == VK_NULL_HANDLE)
    {
        return false;
    }

    DestroyOffscreenResources();

    const bool hasDepth = depthAttachment != nullptr && depthAttachment->imageView != VK_NULL_HANDLE;

    std::array<VkAttachmentDescription, 2> attachments{};
    attachments[0].format = VulkanTextureResource::ToVulkanFormat(color->desc.format);
    attachments[0].samples = VK_SAMPLE_COUNT_1_BIT;
    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    attachments[0].storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    attachments[0].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    attachments[0].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    attachments[0].finalLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;

    if (hasDepth)
    {
        attachments[1].format = VulkanTextureResource::ToVulkanFormat(depthAttachment->desc.format);
        attachments[1].samples = VK_SAMPLE_COUNT_1_BIT;
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
        attachments[1].storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        attachments[1].stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        attachments[1].finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    VkAttachmentReference colorRef{0, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL};
    VkAttachmentReference depthRef{1, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL};

    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorRef;
    if (hasDepth)
    {
        subpass.pDepthStencilAttachment = &depthRef;
    }

    VkRenderPassCreateInfo rpInfo{};
    rpInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    rpInfo.attachmentCount = hasDepth ? 2u : 1u;
    rpInfo.pAttachments = attachments.data();
    rpInfo.subpassCount = 1;
    rpInfo.pSubpasses = &subpass;

    if (vkCreateRenderPass(deviceContext->device, &rpInfo, nullptr, &offscreenRenderPass) != VK_SUCCESS)
    {
        return false;
    }

    attachments[0].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
    attachments[0].initialLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    if (hasDepth)
    {
        attachments[1].loadOp = VK_ATTACHMENT_LOAD_OP_LOAD;
        attachments[1].initialLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    }

    if (vkCreateRenderPass(deviceContext->device, &rpInfo, nullptr, &offscreenRenderPassLoad) != VK_SUCCESS)
    {
        DestroyOffscreenResources();
        return false;
    }

    std::array<VkImageView, 2> fbViews{color->imageView, hasDepth ? depthAttachment->imageView : VK_NULL_HANDLE};

    VkFramebufferCreateInfo fbInfo{};
    fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    fbInfo.renderPass = offscreenRenderPass;
    fbInfo.attachmentCount = hasDepth ? 2u : 1u;
    fbInfo.pAttachments = fbViews.data();
    fbInfo.width = color->desc.extent.width;
    fbInfo.height = color->desc.extent.height;
    fbInfo.layers = 1;

    if (vkCreateFramebuffer(deviceContext->device, &fbInfo, nullptr, &offscreenFramebuffer) != VK_SUCCESS)
    {
        DestroyOffscreenResources();
        return false;
    }

    return true;
}

void VulkanRenderTargetResource::DestroyOffscreenResources() noexcept
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        offscreenRenderPass = VK_NULL_HANDLE;
        offscreenRenderPassLoad = VK_NULL_HANDLE;
        offscreenFramebuffer = VK_NULL_HANDLE;
        return;
    }
    if (offscreenFramebuffer != VK_NULL_HANDLE)
    {
        vkDestroyFramebuffer(deviceContext->device, offscreenFramebuffer, nullptr);
        offscreenFramebuffer = VK_NULL_HANDLE;
    }
    if (offscreenRenderPassLoad != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(deviceContext->device, offscreenRenderPassLoad, nullptr);
        offscreenRenderPassLoad = VK_NULL_HANDLE;
    }
    if (offscreenRenderPass != VK_NULL_HANDLE)
    {
        vkDestroyRenderPass(deviceContext->device, offscreenRenderPass, nullptr);
        offscreenRenderPass = VK_NULL_HANDLE;
    }
}

void VulkanRenderTargetResource::RegisterSelf() noexcept { g_vulkanRenderTargets[handle] = this; }

void VulkanRenderTargetResource::UnregisterSelf() noexcept { g_vulkanRenderTargets.erase(handle); }

VulkanAccelerationStructureResource::VulkanAccelerationStructureResource(AccelerationStructureCreateInfo createInfo)
    : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
{
}

GraphicsAPI VulkanAccelerationStructureResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string VulkanAccelerationStructureResource::GetDebugName() const noexcept { return debugName; }

const AccelerationStructureDesc &VulkanAccelerationStructureResource::GetDescription() const noexcept { return desc; }

VulkanTimestampQueryResource::VulkanTimestampQueryResource(GPUTimestampQueryCreateInfo createInfo)
    : debugName(std::move(createInfo.debugName))
{
}

GraphicsAPI VulkanTimestampQueryResource::GetAPI() const noexcept { return GraphicsAPI::Vulkan; }

std::string VulkanTimestampQueryResource::GetDebugName() const noexcept { return debugName; }

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
