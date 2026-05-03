#pragma once

#include "Graphics/Backends/Vulkan/VulkanContext.h"
#include "Graphics/Core/GraphicsResources.h"

#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <vulkan/vulkan.h>

namespace VulkanPipelineCache
{

// Bindings discovered in a shader source: the binding number plus its descriptor type.
struct DiscoveredBinding
{
    std::uint32_t binding = 0;
    VkDescriptorType type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    VkShaderStageFlags stages = 0;
};

// A bare `uniform <type> <name>` declaration that gets hoisted into a push-constant block.
// The shader program merges these across stages so every stage shares a single block layout —
// otherwise vertex `pc.projection` and fragment `pc.textColor` would each sit at offset 0,
// trampling each other's data when vkCmdPushConstants writes a single range.
struct BareUniformInfo
{
    std::string type;
    std::string name;
};

// Computed offset/size for one bare uniform inside the unified push-constant block.
struct PushConstantUniformLayout
{
    std::string name;
    std::uint32_t offset = 0;
    std::uint32_t sizeBytes = 0;
};

// SPIR-V bytecode plus the bindings/push constants it references.
struct CompiledShaderStage
{
    ShaderStage stage = ShaderStage::Vertex;
    std::vector<std::uint32_t> spirv;
    std::vector<DiscoveredBinding> bindings;
    std::uint32_t pushConstantSizeBytes = 0;  // 0 if no push constants
};

// Build the full descriptor set layout from the merged stages.
struct PipelineKey
{
    const void *programResource = nullptr;       // VulkanShaderProgramResource pointer (acts as identity)
    std::vector<std::uint32_t> vertexAttributes; // GeometryLayout::vertexAttributes
    VkRenderPass renderPass = VK_NULL_HANDLE;
    bool depthTest = false;
    bool depthWrite = false;
    bool blendEnable = false;
};

struct PipelineEntry
{
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorSetLayout = VK_NULL_HANDLE;
    std::vector<DiscoveredBinding> mergedBindings;
    std::uint32_t pushConstantSizeBytes = 0;
};

// Convert the raw OpenGL-style GLSL source into Vulkan-compatible GLSL and discover its bindings.
// Returns the rewritten source. `outBindings` accumulates UBO/SSBO bindings (set = 0). `outPushConstantSize`
// is set to the total push-constant block size in bytes (0 if none). `outPushConstantUniforms`, if not null,
// is filled with the bare uniforms hoisted to the push-constant block (in source order) so the program
// can merge them across stages and produce a unified layout.
std::string PreprocessGlsl(ShaderStage stage, const std::string &source, std::vector<DiscoveredBinding> &outBindings,
                           std::uint32_t &outPushConstantSize,
                           std::vector<BareUniformInfo> *outPushConstantUniforms = nullptr);

// Re-preprocess one stage's source given a unified bare-uniform list collected across all stages.
// The unified block is injected identically into every stage so the same name has the same offset
// in every shader — which is required because vkCmdPushConstants targets a single memory range.
std::string PreprocessGlslWithUnifiedPushConstants(ShaderStage stage, const std::string &source,
                                                   const std::vector<BareUniformInfo> &unifiedUniforms,
                                                   std::vector<DiscoveredBinding> &outBindings);

// Compute std140-ish offset/size for each unified push-constant uniform. The total returned via
// `outTotalSize` is rounded up to 16 bytes.
void ComputePushConstantLayout(const std::vector<BareUniformInfo> &uniforms,
                               std::vector<PushConstantUniformLayout> &outLayout, std::uint32_t &outTotalSize);

// Compile a Vulkan-compatible GLSL string to SPIR-V via shaderc. Returns true on success.
bool CompileGlslToSpirv(ShaderStage stage, const std::string &vulkanGlsl, const std::string &debugName,
                        std::vector<std::uint32_t> &outSpirv);

// Get-or-create a pipeline keyed on (program identity, vertex layout, render pass).
const PipelineEntry *GetOrCreatePipeline(const std::shared_ptr<VulkanDeviceContext> &deviceContext, const PipelineKey &key,
                                         const std::vector<CompiledShaderStage> &stages);

// Per-frame descriptor pool reset. One pool per frame slot — resetting only the slot whose previous
// GPU work has been fenced ensures we never recycle descriptors still in use by another in-flight frame.
// Caller (VulkanWindowContext::BeginFrame) must have already waited on the fence for `frameIndex`.
void ResetFrameDescriptors(VkDevice device, std::uint32_t frameIndex) noexcept;

// Allocate one descriptor set with the given layout out of the active frame's pool. The active
// frame is the one most recently passed to ResetFrameDescriptors.
VkDescriptorSet AllocateFrameDescriptorSet(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkDescriptorSetLayout layout);

// Tear down any cached pipelines / pools owned by this device. Called on graphics shutdown.
void DestroyAll(const std::shared_ptr<VulkanDeviceContext> &deviceContext) noexcept;

} // namespace VulkanPipelineCache
