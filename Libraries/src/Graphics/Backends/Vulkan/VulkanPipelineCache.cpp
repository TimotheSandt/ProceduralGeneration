#include "Graphics/Backends/Vulkan/VulkanPipelineCache.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <regex>
#include <sstream>
#include <system_error>
#include <unordered_map>

#include <shaderc/shaderc.hpp>

#include "Logger.h"

namespace VulkanPipelineCache
{

namespace
{

shaderc_shader_kind ToShadercKind(ShaderStage stage) noexcept
{
    switch (stage)
    {
        case ShaderStage::Vertex:
            return shaderc_vertex_shader;
        case ShaderStage::Fragment:
            return shaderc_fragment_shader;
        case ShaderStage::Geometry:
            return shaderc_geometry_shader;
        case ShaderStage::TessellationControl:
            return shaderc_tess_control_shader;
        case ShaderStage::TessellationEvaluation:
            return shaderc_tess_evaluation_shader;
        case ShaderStage::Compute:
            return shaderc_compute_shader;
        default:
            return shaderc_glsl_infer_from_source;
    }
}

VkShaderStageFlagBits ToVulkanStage(ShaderStage stage) noexcept
{
    switch (stage)
    {
        case ShaderStage::Vertex:
            return VK_SHADER_STAGE_VERTEX_BIT;
        case ShaderStage::Fragment:
            return VK_SHADER_STAGE_FRAGMENT_BIT;
        case ShaderStage::Geometry:
            return VK_SHADER_STAGE_GEOMETRY_BIT;
        case ShaderStage::TessellationControl:
            return VK_SHADER_STAGE_TESSELLATION_CONTROL_BIT;
        case ShaderStage::TessellationEvaluation:
            return VK_SHADER_STAGE_TESSELLATION_EVALUATION_BIT;
        case ShaderStage::Compute:
            return VK_SHADER_STAGE_COMPUTE_BIT;
        default:
            return VK_SHADER_STAGE_VERTEX_BIT;
    }
}

// Strip block-comments and line-comments from a GLSL source so the regex passes don't trip over
// commented-out bindings.
std::string StripComments(const std::string &input)
{
    std::string output;
    output.reserve(input.size());
    const std::size_t inputSize = input.size();

    for (std::size_t i = 0; i < inputSize;)
    {
        if (i + 1 < inputSize && input[i] == '/' && input[i + 1] == '/')
        {
            // Line comment — skip to end of line, keep the newline
            while (i < inputSize && input[i] != '\n')
            {
                ++i;
            }
            continue;
        }
        if (i + 1 < inputSize && input[i] == '/' && input[i + 1] == '*')
        {
            i += 2;
            while (i + 1 < inputSize && !(input[i] == '*' && input[i + 1] == '/'))
            {
                ++i;
            }
            i = std::min(i + 2, inputSize);
            continue;
        }
        output.push_back(input[i]);
        ++i;
    }

    return output;
}

// Patches `layout(binding = N, ...)` (no `set =`) → `layout(set = 0, binding = N, ...)`.
std::string AddSetZeroToBindings(const std::string &input)
{
    static const std::regex bindingRegex(R"(layout\s*\(([^)]*?\bbinding\s*=\s*\d+[^)]*)\))");
    std::string output;
    output.reserve(input.size());

    std::sregex_iterator it(input.begin(), input.end(), bindingRegex);
    std::sregex_iterator end;
    std::size_t lastIndex = 0;

    for (; it != end; ++it)
    {
        const std::smatch &match = *it;
        const std::string contents = match[1].str();
        output.append(input, lastIndex, match.position(0) - lastIndex);

        if (contents.find("set") != std::string::npos &&
            std::regex_search(contents, std::regex(R"(\bset\s*=\s*\d+)")))
        {
            // Already has a set qualifier — leave untouched
            output.append(match.str());
        }
        else
        {
            output.append("layout(set = 0, ");
            output.append(contents);
            output.append(")");
        }

        lastIndex = static_cast<std::size_t>(match.position(0)) + match.length(0);
    }

    output.append(input, lastIndex, input.size() - lastIndex);
    return output;
}

// Find bare `uniform <type> <name>;` declarations (not inside layout(...) blocks).
// For each, capture (type, name). Push-constant-eligible types go into pcUniforms;
// sampler/image types go into samplerUniforms (each gets assigned a descriptor binding later).
struct BareUniform
{
    std::string type;
    std::string name;
};

bool IsSamplerOrImageType(const std::string &type) noexcept
{
    static const char *prefixes[] = {"sampler", "image", "texture"};
    for (const char *p : prefixes)
    {
        if (type.compare(0, std::strlen(p), p) == 0)
        {
            return true;
        }
    }
    return false;
}

std::string ExtractAndRewriteBareUniforms(const std::string &input, std::vector<BareUniform> &outPcUniforms,
                                          std::vector<BareUniform> &outSamplerUniforms, std::uint32_t firstSamplerBinding)
{
    std::string output = input;

    static const std::regex bareUniformRegex(R"((^|\n)(\s*)uniform\s+([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*;)");
    std::string rewritten;
    rewritten.reserve(output.size());

    std::sregex_iterator it(output.begin(), output.end(), bareUniformRegex);
    std::sregex_iterator end;
    std::size_t lastIndex = 0;
    std::uint32_t samplerCounter = 0;

    for (; it != end; ++it)
    {
        const std::smatch &match = *it;
        const std::string prefix = match[1].str();
        const std::string indent = match[2].str();
        const std::string type = match[3].str();
        const std::string name = match[4].str();

        rewritten.append(output, lastIndex, match.position(0) - lastIndex);

        if (IsSamplerOrImageType(type))
        {
            const std::uint32_t binding = firstSamplerBinding + samplerCounter++;
            rewritten.append(prefix).append(indent);
            rewritten.append("layout(set = 0, binding = ").append(std::to_string(binding)).append(") uniform ");
            rewritten.append(type).append(" ").append(name).append(";");
            outSamplerUniforms.push_back({type, name});
        }
        else
        {
            rewritten.append(prefix).append(indent);
            rewritten.append("// (hoisted to push_constant) uniform ").append(type).append(" ").append(name).append(";");
            outPcUniforms.push_back({type, name});
        }

        lastIndex = static_cast<std::size_t>(match.position(0)) + match.length(0);
    }

    rewritten.append(output, lastIndex, output.size() - lastIndex);
    return rewritten;
}

// Wrap each bare-uniform name reference as `pc.<name>` (whole word).
std::string RewriteBareUniformReferences(const std::string &input, const std::vector<BareUniform> &uniforms)
{
    std::string output = input;
    for (const BareUniform &uniform : uniforms)
    {
        // Build a regex matching `<name>` as a whole identifier
        const std::string pattern = std::string(R"(\b)") + uniform.name + R"(\b)";
        std::regex re(pattern);
        output = std::regex_replace(output, re, std::string("pc.") + uniform.name);
    }
    return output;
}

std::string BuildPushConstantBlock(const std::vector<BareUniform> &uniforms)
{
    if (uniforms.empty())
    {
        return {};
    }

    std::ostringstream block;
    block << "layout(push_constant) uniform PushConstants {\n";
    for (const BareUniform &uniform : uniforms)
    {
        block << "    " << uniform.type << " " << uniform.name << ";\n";
    }
    block << "} pc;\n";
    return block.str();
}

// std140-ish layout info for push-constant size estimation.
struct StdLayoutInfo
{
    std::uint32_t size;
    std::uint32_t baseAlignment;
};

StdLayoutInfo Std140Info(const std::string &type) noexcept
{
    if (type == "float" || type == "int" || type == "uint" || type == "bool")
        return {4, 4};
    if (type == "vec2" || type == "ivec2" || type == "uvec2")
        return {8, 8};
    if (type == "vec3" || type == "ivec3" || type == "uvec3")
        return {12, 16};
    if (type == "vec4" || type == "ivec4" || type == "uvec4")
        return {16, 16};
    if (type == "mat2")
        return {32, 16};
    if (type == "mat3")
        return {48, 16};
    if (type == "mat4")
        return {64, 16};
    return {16, 16};
}

std::uint32_t EstimatePushConstantBytes(const std::vector<BareUniform> &uniforms) noexcept
{
    std::uint32_t offset = 0;
    for (const BareUniform &u : uniforms)
    {
        const StdLayoutInfo info = Std140Info(u.type);
        offset = (offset + info.baseAlignment - 1) & ~(info.baseAlignment - 1);
        offset += info.size;
    }
    // Round up the block size to 16 bytes.
    return (offset + 15u) & ~15u;
}

// Add explicit `layout(location = N)` qualifiers to bare `in`/`out` declarations.
// Locations are assigned in source order, separately for in and out. Bare = not preceded by `layout(`.
// Skips lines that already start with `layout(` and skips block declarations (`in <Block> { ... } name;`).
std::string AddInOutLocations(const std::string &input)
{
    // Match start-of-line (with optional whitespace), optional interpolation qualifiers,
    // then `in` or `out`, then a primitive type identifier, then a name, then `;`.
    // The negative-lookbehind for `layout(` is avoided by anchoring to start of line.
    static const std::regex inOutRegex(
        R"((^|\n)(\s*)((?:flat|smooth|noperspective|centroid|sample|patch|invariant|precise)\s+)*((?:in|out))\s+([A-Za-z_][A-Za-z0-9_]*)\s+([A-Za-z_][A-Za-z0-9_]*)\s*;)");

    std::string output;
    output.reserve(input.size() + 64);

    std::sregex_iterator it(input.begin(), input.end(), inOutRegex);
    std::sregex_iterator end;
    std::size_t lastIndex = 0;
    std::uint32_t inLocation = 0;
    std::uint32_t outLocation = 0;

    for (; it != end; ++it)
    {
        const std::smatch &match = *it;
        const std::string prefix = match[1].str();
        const std::string indent = match[2].str();
        const std::string qualifiers = match[3].str();
        const std::string direction = match[4].str();
        const std::string type = match[5].str();
        const std::string name = match[6].str();

        output.append(input, lastIndex, match.position(0) - lastIndex);
        output.append(prefix).append(indent);

        std::uint32_t location = (direction == "in") ? inLocation++ : outLocation++;
        output.append("layout(location = ").append(std::to_string(location)).append(") ");
        output.append(qualifiers).append(direction).append(" ").append(type).append(" ").append(name).append(";");

        lastIndex = static_cast<std::size_t>(match.position(0)) + match.length(0);
    }

    output.append(input, lastIndex, input.size() - lastIndex);
    return output;
}

void DiscoverBindings(const std::string &source, ShaderStage stage, std::vector<DiscoveredBinding> &outBindings)
{
    // After preprocessing every layout() has `set = 0`, but qualifiers like `std430` may appear
    // between `set` and `binding` (e.g. `layout(set = 0, std430, binding = 1) buffer ...`), so we
    // accept anything inside the parens before `binding = N`. Missing this match silently drops
    // the SSBO/UBO from the descriptor set layout — the shader then reads garbage, giving black
    // meshes and frequently device loss.
    static const std::regex bindingRegex(
        R"(layout\s*\(\s*[^)]*?\bbinding\s*=\s*(\d+)[^)]*\)\s*(?:(readonly|writeonly|coherent|volatile|restrict)\s+)?(uniform|buffer)\s+([A-Za-z_][A-Za-z0-9_]*))");

    std::sregex_iterator it(source.begin(), source.end(), bindingRegex);
    std::sregex_iterator end;
    const VkShaderStageFlags stageFlag = ToVulkanStage(stage);

    for (; it != end; ++it)
    {
        const std::smatch &match = *it;
        const std::uint32_t binding = static_cast<std::uint32_t>(std::stoul(match[1].str()));
        const std::string keyword = match[3].str();
        const std::string typeOrName = match[4].str();

        DiscoveredBinding entry;
        entry.binding = binding;
        entry.stages = stageFlag;
        if (keyword == "buffer")
        {
            entry.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        }
        else if (IsSamplerOrImageType(typeOrName))
        {
            entry.type = (typeOrName.compare(0, 5, "image") == 0) ? VK_DESCRIPTOR_TYPE_STORAGE_IMAGE
                                                                  : VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        }
        else
        {
            entry.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        }
        outBindings.push_back(entry);
    }
}

VkFormat FormatForFloatCount(std::uint32_t floatCount) noexcept
{
    switch (floatCount)
    {
        case 1:
            return VK_FORMAT_R32_SFLOAT;
        case 2:
            return VK_FORMAT_R32G32_SFLOAT;
        case 3:
            return VK_FORMAT_R32G32B32_SFLOAT;
        case 4:
            return VK_FORMAT_R32G32B32A32_SFLOAT;
        default:
            return VK_FORMAT_R32G32B32_SFLOAT;
    }
}

// Cache key: composed of program ptr + vertexAttributes vector + render pass handle + render state bits.
struct CachedKey
{
    const void *program;
    std::vector<std::uint32_t> vertexAttributes;
    VkRenderPass renderPass;
    std::uint32_t stateBits;  // packed: depthTest | depthWrite<<1 | blend<<2

    bool operator==(const CachedKey &other) const noexcept
    {
        return program == other.program && renderPass == other.renderPass && stateBits == other.stateBits &&
               vertexAttributes == other.vertexAttributes;
    }
};

struct CachedKeyHash
{
    std::size_t operator()(const CachedKey &k) const noexcept
    {
        std::size_t h = std::hash<const void *>{}(k.program);
        h ^= std::hash<const void *>{}(static_cast<const void *>(k.renderPass)) + 0x9e3779b9 + (h << 6) + (h >> 2);
        h ^= std::hash<std::uint32_t>{}(k.stateBits) + 0x9e3779b9 + (h << 6) + (h >> 2);
        for (std::uint32_t v : k.vertexAttributes)
        {
            h ^= std::hash<std::uint32_t>{}(v) + 0x9e3779b9 + (h << 6) + (h >> 2);
        }
        return h;
    }
};

struct CacheState
{
    std::unordered_map<CachedKey, PipelineEntry, CachedKeyHash> pipelines;
    std::vector<VkDescriptorPool> descriptorPools;  // One per frame slot.
    std::uint32_t activeFrameIndex = 0;
    VkPipelineCache pipelineCache = VK_NULL_HANDLE;  // Native VkPipelineCache, persisted to disk
};

CacheState g_cache;

const std::filesystem::path &GetPipelineCacheFilePath() noexcept
{
    static const std::filesystem::path path = []() {
        std::filesystem::path p = "shader_cache";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p / "pipeline_cache.bin";
    }();
    return path;
}

// Ensure the native VkPipelineCache exists, seeding it from disk if a saved blob is present.
// vkCreateGraphicsPipelines can then reuse driver-side compilation work across runs.
VkPipelineCache EnsurePipelineCache(const std::shared_ptr<VulkanDeviceContext> &deviceContext)
{
    if (g_cache.pipelineCache != VK_NULL_HANDLE)
    {
        return g_cache.pipelineCache;
    }
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }

    std::vector<char> blob;
    {
        std::error_code ec;
        const std::filesystem::path &path = GetPipelineCacheFilePath();
        if (std::filesystem::exists(path, ec))
        {
            const std::uintmax_t size = std::filesystem::file_size(path, ec);
            if (!ec && size > 0)
            {
                std::ifstream f(path, std::ios::binary);
                if (f)
                {
                    blob.resize(static_cast<std::size_t>(size));
                    f.read(blob.data(), static_cast<std::streamsize>(size));
                    if (!f.good() && !f.eof())
                    {
                        blob.clear();
                    }
                }
            }
        }
    }

    VkPipelineCacheCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_PIPELINE_CACHE_CREATE_INFO;
    info.initialDataSize = blob.size();
    info.pInitialData = blob.empty() ? nullptr : blob.data();

    if (vkCreatePipelineCache(deviceContext->device, &info, nullptr, &g_cache.pipelineCache) != VK_SUCCESS)
    {
        g_cache.pipelineCache = VK_NULL_HANDLE;
    }
    return g_cache.pipelineCache;
}

void SavePipelineCacheToDisk(const std::shared_ptr<VulkanDeviceContext> &deviceContext) noexcept
{
    if (g_cache.pipelineCache == VK_NULL_HANDLE || deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        return;
    }
    std::size_t size = 0;
    if (vkGetPipelineCacheData(deviceContext->device, g_cache.pipelineCache, &size, nullptr) != VK_SUCCESS || size == 0)
    {
        return;
    }
    std::vector<char> blob(size);
    if (vkGetPipelineCacheData(deviceContext->device, g_cache.pipelineCache, &size, blob.data()) != VK_SUCCESS)
    {
        return;
    }
    std::ofstream f(GetPipelineCacheFilePath(), std::ios::binary | std::ios::trunc);
    if (f)
    {
        f.write(blob.data(), static_cast<std::streamsize>(size));
    }
}

bool EnsureDescriptorPool(const std::shared_ptr<VulkanDeviceContext> &deviceContext, std::uint32_t frameIndex)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        return false;
    }
    if (frameIndex >= g_cache.descriptorPools.size())
    {
        g_cache.descriptorPools.resize(frameIndex + 1, VK_NULL_HANDLE);
    }
    if (g_cache.descriptorPools[frameIndex] != VK_NULL_HANDLE)
    {
        return true;
    }

    constexpr std::uint32_t MaxSetsPerType = 1024;
    VkDescriptorPoolSize poolSizes[4]{};
    poolSizes[0].type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    poolSizes[0].descriptorCount = MaxSetsPerType;
    poolSizes[1].type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
    poolSizes[1].descriptorCount = MaxSetsPerType;
    poolSizes[2].type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    poolSizes[2].descriptorCount = MaxSetsPerType;
    poolSizes[3].type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
    poolSizes[3].descriptorCount = MaxSetsPerType;

    VkDescriptorPoolCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    createInfo.flags = 0;  // We reset the whole pool each frame slot.
    createInfo.maxSets = MaxSetsPerType;
    createInfo.poolSizeCount = 4;
    createInfo.pPoolSizes = poolSizes;

    if (vkCreateDescriptorPool(deviceContext->device, &createInfo, nullptr, &g_cache.descriptorPools[frameIndex]) != VK_SUCCESS)
    {
        LOG_ERROR(1, "[Vulkan] Failed to create descriptor pool");
        return false;
    }
    return true;
}

VkShaderModule CreateShaderModule(VkDevice device, const std::vector<std::uint32_t> &spirv)
{
    VkShaderModuleCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = spirv.size() * sizeof(std::uint32_t);
    createInfo.pCode = spirv.data();

    VkShaderModule module = VK_NULL_HANDLE;
    if (vkCreateShaderModule(device, &createInfo, nullptr, &module) != VK_SUCCESS)
    {
        return VK_NULL_HANDLE;
    }
    return module;
}

} // namespace

std::string PreprocessGlsl(ShaderStage stage, const std::string &source, std::vector<DiscoveredBinding> &outBindings,
                           std::uint32_t &outPushConstantSize, std::vector<BareUniformInfo> *outPushConstantUniforms)
{
    std::string working = StripComments(source);

    // Replace any "#version 4XX core" with #version 450
    static const std::regex versionRegex(R"(#version\s+\d+(\s+core)?)");
    working = std::regex_replace(working, versionRegex, std::string("#version 450"));

    // Add explicit location qualifiers to bare in/out declarations (Vulkan SPIR-V requires them)
    working = AddInOutLocations(working);

    // Split bare uniforms into push-constant-eligible scalars/vecs/mats and sampler/image uniforms.
    // Sampler bindings are auto-assigned starting after the highest existing layout(binding=N) we'll discover.
    // We pick a safe starting point of 8 — the existing shaders use bindings 0..3, so 8+ is unused.
    constexpr std::uint32_t kFirstSamplerBinding = 8;
    std::vector<BareUniform> pcUniforms;
    std::vector<BareUniform> samplerUniforms;
    working = ExtractAndRewriteBareUniforms(working, pcUniforms, samplerUniforms, kFirstSamplerBinding);

    if (!pcUniforms.empty())
    {
        // Wrap references to push-constant uniform names in `pc.<name>`
        working = RewriteBareUniformReferences(working, pcUniforms);

        // Inject the push constant block right after the #version line
        const std::string pcBlock = BuildPushConstantBlock(pcUniforms);
        const std::size_t versionPos = working.find("#version");
        if (versionPos != std::string::npos)
        {
            const std::size_t lineEnd = working.find('\n', versionPos);
            if (lineEnd != std::string::npos)
            {
                working.insert(lineEnd + 1, pcBlock + "\n");
            }
        }

        outPushConstantSize = EstimatePushConstantBytes(pcUniforms);
    }
    else
    {
        outPushConstantSize = 0;
    }

    if (outPushConstantUniforms != nullptr)
    {
        outPushConstantUniforms->clear();
        outPushConstantUniforms->reserve(pcUniforms.size());
        for (const BareUniform &u : pcUniforms)
        {
            outPushConstantUniforms->push_back({u.type, u.name});
        }
    }

    // Add `set = 0` to every layout(binding = N) block (covers user-written and our sampler injections)
    working = AddSetZeroToBindings(working);

    // Discover bindings (UBO / SSBO / sampler / image at set 0)
    DiscoverBindings(working, stage, outBindings);

    return working;
}

std::string PreprocessGlslWithUnifiedPushConstants(ShaderStage stage, const std::string &source,
                                                   const std::vector<BareUniformInfo> &unifiedUniforms,
                                                   std::vector<DiscoveredBinding> &outBindings)
{
    std::string working = StripComments(source);

    static const std::regex versionRegex(R"(#version\s+\d+(\s+core)?)");
    working = std::regex_replace(working, versionRegex, std::string("#version 450"));

    working = AddInOutLocations(working);

    constexpr std::uint32_t kFirstSamplerBinding = 8;
    std::vector<BareUniform> pcUniformsThisStage;
    std::vector<BareUniform> samplerUniforms;
    working = ExtractAndRewriteBareUniforms(working, pcUniformsThisStage, samplerUniforms, kFirstSamplerBinding);

    // Build the canonical block from the unified list — every stage uses the same block so the
    // same name occupies the same offset everywhere.
    if (!unifiedUniforms.empty())
    {
        std::vector<BareUniform> canonical;
        canonical.reserve(unifiedUniforms.size());
        for (const BareUniformInfo &u : unifiedUniforms)
        {
            canonical.push_back({u.type, u.name});
        }
        // Rewrite any reference to a unified-uniform name as `pc.<name>` (covers names this stage
        // didn't itself declare but that ExtractAndRewriteBareUniforms therefore didn't rewrite).
        working = RewriteBareUniformReferences(working, canonical);

        const std::string pcBlock = BuildPushConstantBlock(canonical);
        const std::size_t versionPos = working.find("#version");
        if (versionPos != std::string::npos)
        {
            const std::size_t lineEnd = working.find('\n', versionPos);
            if (lineEnd != std::string::npos)
            {
                working.insert(lineEnd + 1, pcBlock + "\n");
            }
        }
    }

    working = AddSetZeroToBindings(working);
    DiscoverBindings(working, stage, outBindings);
    return working;
}

void ComputePushConstantLayout(const std::vector<BareUniformInfo> &uniforms,
                               std::vector<PushConstantUniformLayout> &outLayout, std::uint32_t &outTotalSize)
{
    outLayout.clear();
    outLayout.reserve(uniforms.size());
    std::uint32_t offset = 0;
    for (const BareUniformInfo &u : uniforms)
    {
        const StdLayoutInfo info = Std140Info(u.type);
        offset = (offset + info.baseAlignment - 1) & ~(info.baseAlignment - 1);
        outLayout.push_back({u.name, offset, info.size});
        offset += info.size;
    }
    outTotalSize = (offset + 15u) & ~15u;
}

namespace
{

// Bump this when we change anything that could affect SPIR-V output for the same input GLSL
// (e.g. shaderc target version, compile options). Old cache entries with mismatched version
// are silently rejected.
constexpr std::uint32_t kSpirvCacheVersion = 1;

// FNV-1a 64-bit, used as cache key. We don't need cryptographic strength — collisions just mean
// the cache miss falls through to a fresh compile.
std::uint64_t HashStringFnv1a64(const std::string &s) noexcept
{
    std::uint64_t hash = 14695981039346656037ULL;
    for (char c : s)
    {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    return hash;
}

const std::filesystem::path &GetShaderCacheDir() noexcept
{
    static const std::filesystem::path dir = []() {
        std::filesystem::path p = "shader_cache";
        std::error_code ec;
        std::filesystem::create_directories(p, ec);
        return p;
    }();
    return dir;
}

bool TryLoadSpirvFromCache(std::uint64_t key, std::vector<std::uint32_t> &outSpirv) noexcept
{
    char nameBuf[32];
    std::snprintf(nameBuf, sizeof(nameBuf), "%016llx.spv", static_cast<unsigned long long>(key));
    const std::filesystem::path path = GetShaderCacheDir() / nameBuf;

    std::error_code ec;
    if (!std::filesystem::exists(path, ec))
    {
        return false;
    }
    const std::uintmax_t fileSize = std::filesystem::file_size(path, ec);
    if (ec || fileSize < sizeof(std::uint32_t) * 2 || (fileSize % sizeof(std::uint32_t)) != 0)
    {
        return false;
    }

    std::ifstream f(path, std::ios::binary);
    if (!f)
    {
        return false;
    }
    std::uint32_t header[2]{};
    f.read(reinterpret_cast<char *>(header), sizeof(header));
    if (!f.good() || header[0] != kSpirvCacheVersion)
    {
        return false;
    }
    const std::size_t payloadWords = (fileSize - sizeof(header)) / sizeof(std::uint32_t);
    if (payloadWords == 0)
    {
        return false;
    }
    outSpirv.resize(payloadWords);
    f.read(reinterpret_cast<char *>(outSpirv.data()), payloadWords * sizeof(std::uint32_t));
    if (!f.good() && !f.eof())
    {
        outSpirv.clear();
        return false;
    }
    return !outSpirv.empty();
}

void SaveSpirvToCache(std::uint64_t key, const std::vector<std::uint32_t> &spirv) noexcept
{
    char nameBuf[32];
    std::snprintf(nameBuf, sizeof(nameBuf), "%016llx.spv", static_cast<unsigned long long>(key));
    const std::filesystem::path path = GetShaderCacheDir() / nameBuf;
    std::ofstream f(path, std::ios::binary);
    if (!f)
    {
        return;
    }
    const std::uint32_t header[2] = {kSpirvCacheVersion, static_cast<std::uint32_t>(spirv.size())};
    f.write(reinterpret_cast<const char *>(header), sizeof(header));
    f.write(reinterpret_cast<const char *>(spirv.data()), spirv.size() * sizeof(std::uint32_t));
}

} // namespace

bool CompileGlslToSpirv(ShaderStage stage, const std::string &vulkanGlsl, const std::string &debugName,
                        std::vector<std::uint32_t> &outSpirv)
{
    // Cache key folds the stage into the hash so two stages with identical text don't collide.
    std::string keyInput;
    keyInput.reserve(vulkanGlsl.size() + 8);
    keyInput.push_back(static_cast<char>(static_cast<int>(stage) & 0xFF));
    keyInput.push_back('|');
    keyInput.append(vulkanGlsl);
    const std::uint64_t cacheKey = HashStringFnv1a64(keyInput);

    if (TryLoadSpirvFromCache(cacheKey, outSpirv))
    {
        return true;
    }

    shaderc::Compiler compiler;
    shaderc::CompileOptions options;
    options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    options.SetSourceLanguage(shaderc_source_language_glsl);
    options.SetOptimizationLevel(shaderc_optimization_level_zero);

    const shaderc::SpvCompilationResult result = compiler.CompileGlslToSpv(vulkanGlsl, ToShadercKind(stage), debugName.c_str(), options);

    if (result.GetCompilationStatus() != shaderc_compilation_status_success)
    {
        LOG_ERROR(1, "[Vulkan] Shader compilation failed for '", debugName, "' stage=", static_cast<int>(stage),
                  ": ", result.GetErrorMessage());
        return false;
    }

    outSpirv.assign(result.cbegin(), result.cend());
    if (outSpirv.empty())
    {
        return false;
    }
    SaveSpirvToCache(cacheKey, outSpirv);
    return true;
}

const PipelineEntry *GetOrCreatePipeline(const std::shared_ptr<VulkanDeviceContext> &deviceContext, const PipelineKey &key,
                                         const std::vector<CompiledShaderStage> &stages)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || key.renderPass == VK_NULL_HANDLE || stages.empty())
    {
        return nullptr;
    }

    const std::uint32_t stateBits =
        (key.depthTest ? 1u : 0u) | (key.depthWrite ? 2u : 0u) | (key.blendEnable ? 4u : 0u);
    CachedKey cacheKey{key.programResource, key.vertexAttributes, key.renderPass, stateBits};
    if (auto it = g_cache.pipelines.find(cacheKey); it != g_cache.pipelines.end())
    {
        return &it->second;
    }

    PipelineEntry entry;

    // Merge bindings across stages — same binding number with different stages gets stage flags ORed.
    std::unordered_map<std::uint32_t, DiscoveredBinding> mergedBindingsByBinding;
    std::uint32_t totalPushConstantSize = 0;
    for (const CompiledShaderStage &stage : stages)
    {
        totalPushConstantSize = std::max(totalPushConstantSize, stage.pushConstantSizeBytes);
        for (const DiscoveredBinding &binding : stage.bindings)
        {
            auto it = mergedBindingsByBinding.find(binding.binding);
            if (it == mergedBindingsByBinding.end())
            {
                mergedBindingsByBinding.emplace(binding.binding, binding);
            }
            else
            {
                it->second.stages |= binding.stages;
            }
        }
    }

    entry.mergedBindings.reserve(mergedBindingsByBinding.size());
    for (const auto &[binding, descriptor] : mergedBindingsByBinding)
    {
        entry.mergedBindings.push_back(descriptor);
    }
    std::sort(entry.mergedBindings.begin(), entry.mergedBindings.end(),
              [](const DiscoveredBinding &a, const DiscoveredBinding &b) { return a.binding < b.binding; });
    entry.pushConstantSizeBytes = totalPushConstantSize;

    // Build descriptor set layout
    std::vector<VkDescriptorSetLayoutBinding> layoutBindings;
    layoutBindings.reserve(entry.mergedBindings.size());
    for (const DiscoveredBinding &binding : entry.mergedBindings)
    {
        VkDescriptorSetLayoutBinding b{};
        b.binding = binding.binding;
        b.descriptorType = binding.type;
        b.descriptorCount = 1;
        b.stageFlags = binding.stages;
        layoutBindings.push_back(b);
    }

    VkDescriptorSetLayoutCreateInfo dslCreateInfo{};
    dslCreateInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dslCreateInfo.bindingCount = static_cast<std::uint32_t>(layoutBindings.size());
    dslCreateInfo.pBindings = layoutBindings.data();

    if (vkCreateDescriptorSetLayout(deviceContext->device, &dslCreateInfo, nullptr, &entry.descriptorSetLayout) != VK_SUCCESS)
    {
        LOG_ERROR(1, "[Vulkan] Failed to create descriptor set layout");
        return nullptr;
    }

    // Build pipeline layout
    VkPushConstantRange pushRange{};
    pushRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pushRange.offset = 0;
    pushRange.size = entry.pushConstantSizeBytes;

    VkPipelineLayoutCreateInfo plCreateInfo{};
    plCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plCreateInfo.setLayoutCount = 1;
    plCreateInfo.pSetLayouts = &entry.descriptorSetLayout;
    if (entry.pushConstantSizeBytes > 0)
    {
        plCreateInfo.pushConstantRangeCount = 1;
        plCreateInfo.pPushConstantRanges = &pushRange;
    }

    if (vkCreatePipelineLayout(deviceContext->device, &plCreateInfo, nullptr, &entry.pipelineLayout) != VK_SUCCESS)
    {
        LOG_ERROR(1, "[Vulkan] Failed to create pipeline layout");
        vkDestroyDescriptorSetLayout(deviceContext->device, entry.descriptorSetLayout, nullptr);
        return nullptr;
    }

    // Build shader stages
    std::vector<VkPipelineShaderStageCreateInfo> stageInfos;
    std::vector<VkShaderModule> moduleHandles;
    stageInfos.reserve(stages.size());
    moduleHandles.reserve(stages.size());
    for (const CompiledShaderStage &stage : stages)
    {
        VkShaderModule module = CreateShaderModule(deviceContext->device, stage.spirv);
        if (module == VK_NULL_HANDLE)
        {
            for (VkShaderModule m : moduleHandles)
            {
                vkDestroyShaderModule(deviceContext->device, m, nullptr);
            }
            vkDestroyDescriptorSetLayout(deviceContext->device, entry.descriptorSetLayout, nullptr);
            vkDestroyPipelineLayout(deviceContext->device, entry.pipelineLayout, nullptr);
            return nullptr;
        }
        moduleHandles.push_back(module);

        VkPipelineShaderStageCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        info.stage = ToVulkanStage(stage.stage);
        info.module = module;
        info.pName = "main";
        stageInfos.push_back(info);
    }

    // Vertex input from GeometryLayout
    std::uint32_t totalStrideFloats = 0;
    for (std::uint32_t a : key.vertexAttributes)
    {
        totalStrideFloats += a;
    }

    VkVertexInputBindingDescription bindingDesc{};
    bindingDesc.binding = 0;
    bindingDesc.stride = totalStrideFloats * sizeof(float);
    bindingDesc.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    std::vector<VkVertexInputAttributeDescription> attributeDescs;
    attributeDescs.reserve(key.vertexAttributes.size());
    {
        std::uint32_t offsetFloats = 0;
        for (std::uint32_t i = 0; i < key.vertexAttributes.size(); ++i)
        {
            VkVertexInputAttributeDescription a{};
            a.binding = 0;
            a.location = i;
            a.format = FormatForFloatCount(key.vertexAttributes[i]);
            a.offset = offsetFloats * sizeof(float);
            attributeDescs.push_back(a);
            offsetFloats += key.vertexAttributes[i];
        }
    }

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    if (totalStrideFloats > 0)
    {
        vertexInput.vertexBindingDescriptionCount = 1;
        vertexInput.pVertexBindingDescriptions = &bindingDesc;
        vertexInput.vertexAttributeDescriptionCount = static_cast<std::uint32_t>(attributeDescs.size());
        vertexInput.pVertexAttributeDescriptions = attributeDescs.data();
    }

    VkPipelineInputAssemblyStateCreateInfo inputAssembly{};
    inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;

    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.scissorCount = 1;

    VkPipelineRasterizationStateCreateInfo rasterizer{};
    rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
    rasterizer.lineWidth = 1.0f;
    rasterizer.cullMode = VK_CULL_MODE_NONE;
    rasterizer.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;

    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;

    VkPipelineColorBlendAttachmentState colorBlendAttachment{};
    colorBlendAttachment.colorWriteMask =
        VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    if (key.blendEnable)
    {
        colorBlendAttachment.blendEnable = VK_TRUE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD;
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD;
    }
    else
    {
        colorBlendAttachment.blendEnable = VK_FALSE;
    }

    VkPipelineColorBlendStateCreateInfo colorBlend{};
    colorBlend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    colorBlend.attachmentCount = 1;
    colorBlend.pAttachments = &colorBlendAttachment;

    VkPipelineDepthStencilStateCreateInfo depthStencil{};
    depthStencil.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthStencil.depthTestEnable = key.depthTest ? VK_TRUE : VK_FALSE;
    depthStencil.depthWriteEnable = key.depthWrite ? VK_TRUE : VK_FALSE;
    depthStencil.depthCompareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
    depthStencil.depthBoundsTestEnable = VK_FALSE;
    depthStencil.stencilTestEnable = VK_FALSE;

    VkDynamicState dynamicStates[] = {VK_DYNAMIC_STATE_VIEWPORT, VK_DYNAMIC_STATE_SCISSOR};
    VkPipelineDynamicStateCreateInfo dynamicState{};
    dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
    dynamicState.dynamicStateCount = 2;
    dynamicState.pDynamicStates = dynamicStates;

    VkGraphicsPipelineCreateInfo pipelineCreateInfo{};
    pipelineCreateInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineCreateInfo.stageCount = static_cast<std::uint32_t>(stageInfos.size());
    pipelineCreateInfo.pStages = stageInfos.data();
    pipelineCreateInfo.pVertexInputState = &vertexInput;
    pipelineCreateInfo.pInputAssemblyState = &inputAssembly;
    pipelineCreateInfo.pViewportState = &viewportState;
    pipelineCreateInfo.pRasterizationState = &rasterizer;
    pipelineCreateInfo.pMultisampleState = &multisample;
    pipelineCreateInfo.pColorBlendState = &colorBlend;
    pipelineCreateInfo.pDepthStencilState = &depthStencil;
    pipelineCreateInfo.pDynamicState = &dynamicState;
    pipelineCreateInfo.layout = entry.pipelineLayout;
    pipelineCreateInfo.renderPass = key.renderPass;
    pipelineCreateInfo.subpass = 0;

    const VkPipelineCache nativeCache = EnsurePipelineCache(deviceContext);
    const VkResult result = vkCreateGraphicsPipelines(deviceContext->device, nativeCache, 1, &pipelineCreateInfo, nullptr, &entry.pipeline);

    // Modules can be destroyed once the pipeline references them
    for (VkShaderModule m : moduleHandles)
    {
        vkDestroyShaderModule(deviceContext->device, m, nullptr);
    }

    if (result != VK_SUCCESS)
    {
        LOG_ERROR(static_cast<int>(result), "[Vulkan] Failed to create graphics pipeline");
        vkDestroyDescriptorSetLayout(deviceContext->device, entry.descriptorSetLayout, nullptr);
        vkDestroyPipelineLayout(deviceContext->device, entry.pipelineLayout, nullptr);
        return nullptr;
    }

    auto [it, inserted] = g_cache.pipelines.emplace(cacheKey, entry);
    return &it->second;
}

void ResetFrameDescriptors(VkDevice device, std::uint32_t frameIndex) noexcept
{
    g_cache.activeFrameIndex = frameIndex;
    if (device == VK_NULL_HANDLE)
    {
        return;
    }
    if (frameIndex < g_cache.descriptorPools.size() && g_cache.descriptorPools[frameIndex] != VK_NULL_HANDLE)
    {
        vkResetDescriptorPool(device, g_cache.descriptorPools[frameIndex], 0);
    }
}

VkDescriptorSet AllocateFrameDescriptorSet(const std::shared_ptr<VulkanDeviceContext> &deviceContext, VkDescriptorSetLayout layout)
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE || layout == VK_NULL_HANDLE)
    {
        return VK_NULL_HANDLE;
    }
    const std::uint32_t frameIndex = g_cache.activeFrameIndex;
    if (!EnsureDescriptorPool(deviceContext, frameIndex))
    {
        return VK_NULL_HANDLE;
    }

    VkDescriptorSetAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    allocInfo.descriptorPool = g_cache.descriptorPools[frameIndex];
    allocInfo.descriptorSetCount = 1;
    allocInfo.pSetLayouts = &layout;

    VkDescriptorSet set = VK_NULL_HANDLE;
    if (const VkResult allocResult = vkAllocateDescriptorSets(deviceContext->device, &allocInfo, &set); allocResult != VK_SUCCESS)
    {
        LOG_ERROR(static_cast<int>(allocResult), "[Vulkan] vkAllocateDescriptorSets failed (frameIndex=", frameIndex,
                  ", layout=", static_cast<const void *>(layout), ") — pool likely exhausted (MaxSetsPerType=1024)");
        return VK_NULL_HANDLE;
    }
    return set;
}

void DestroyAll(const std::shared_ptr<VulkanDeviceContext> &deviceContext) noexcept
{
    if (deviceContext == nullptr || deviceContext->device == VK_NULL_HANDLE)
    {
        g_cache.pipelines.clear();
        g_cache.descriptorPools.clear();
        g_cache.pipelineCache = VK_NULL_HANDLE;
        return;
    }

    // Persist the driver's compiled-pipeline blob so vkCreateGraphicsPipelines on the next run
    // can skip back-end compilation work for unchanged pipelines.
    SavePipelineCacheToDisk(deviceContext);

    for (auto &[key, entry] : g_cache.pipelines)
    {
        if (entry.pipeline != VK_NULL_HANDLE)
        {
            vkDestroyPipeline(deviceContext->device, entry.pipeline, nullptr);
        }
        if (entry.pipelineLayout != VK_NULL_HANDLE)
        {
            vkDestroyPipelineLayout(deviceContext->device, entry.pipelineLayout, nullptr);
        }
        if (entry.descriptorSetLayout != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorSetLayout(deviceContext->device, entry.descriptorSetLayout, nullptr);
        }
    }
    g_cache.pipelines.clear();

    for (VkDescriptorPool pool : g_cache.descriptorPools)
    {
        if (pool != VK_NULL_HANDLE)
        {
            vkDestroyDescriptorPool(deviceContext->device, pool, nullptr);
        }
    }
    g_cache.descriptorPools.clear();

    if (g_cache.pipelineCache != VK_NULL_HANDLE)
    {
        vkDestroyPipelineCache(deviceContext->device, g_cache.pipelineCache, nullptr);
        g_cache.pipelineCache = VK_NULL_HANDLE;
    }
}

} // namespace VulkanPipelineCache
