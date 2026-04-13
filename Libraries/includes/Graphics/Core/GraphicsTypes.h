#pragma once

#include "Graphics/Backend/GraphicsAPI.h"

#include <cstddef>
#include <cstdint>
#include <vector>

enum class ShaderStage : std::uint8_t
{
    Vertex = 0,
    Fragment,
    Geometry,
    TessellationControl,
    TessellationEvaluation,
    Compute,
    RayGeneration,
    AnyHit,
    ClosestHit,
    Miss,
    Intersection,
    Callable
};

enum class BufferUsage : std::uint8_t
{
    Vertex = 0,
    Index,
    Uniform,
    Storage,
    Staging
};

enum class TextureFormat : std::uint8_t
{
    RGBA8 = 0,
    BGRA8,
    Depth24Stencil8,
    Depth32Float,
    R8,
    R32UI  // unsigned 32-bit integer single channel — use for object ID buffers
};

enum class AccelerationStructureType : std::uint8_t
{
    BottomLevel = 0,
    TopLevel
};

enum class AccelerationStructureBuildHint : std::uint8_t
{
    PreferFastTrace = 0,
    PreferFastBuild
};

enum class PrimitiveTopology : std::uint8_t
{
    TriangleList = 0,
    TriangleStrip,
    LineList,
    LineStrip,
    PointList
};

enum class PolygonFillMode : std::uint8_t
{
    Fill = 0,
    Line
};

enum class CullMode : std::uint8_t
{
    None = 0,
    Front,
    Back
};

enum class FrontFaceWinding : std::uint8_t
{
    CounterClockwise = 0,
    Clockwise
};

using ShaderStageMask = std::uint32_t;

constexpr ShaderStageMask ShaderStageBit(ShaderStage stage) noexcept { return 1u << static_cast<std::uint32_t>(stage); }

constexpr bool HasShaderStage(ShaderStageMask mask, ShaderStage stage) noexcept { return (mask & ShaderStageBit(stage)) != 0u; }

struct Extent2D
{
    std::uint32_t width = 0;
    std::uint32_t height = 0;
};

struct BufferDesc
{
    BufferUsage usage = BufferUsage::Vertex;
    std::size_t sizeInBytes = 0;
    bool cpuWritable = false;
};

struct TextureDesc
{
    Extent2D extent{};
    TextureFormat format = TextureFormat::RGBA8;
    std::uint32_t mipLevels = 1;
    bool renderTarget = false;
};

struct RenderTargetDesc
{
    Extent2D extent{};
    // One entry per color attachment. At least one is required.
    // Common formats: RGBA8 (color), R32UI (object IDs), Depth32Float (readable depth).
    std::vector<TextureFormat> colorAttachments = {TextureFormat::RGBA8};
    bool hasDepthBuffer = true;
    // When true, depth is allocated as a readable texture (sampled in shaders).
    // When false, depth is a renderbuffer (faster, but not readable).
    bool depthAsTexture = false;
};

struct ShaderProgramDesc
{
    ShaderStageMask stages = ShaderStageBit(ShaderStage::Vertex) | ShaderStageBit(ShaderStage::Fragment);
    bool runtimeCompilation = true;
};

struct AccelerationStructureDesc
{
    AccelerationStructureType type = AccelerationStructureType::BottomLevel;
    AccelerationStructureBuildHint buildHint = AccelerationStructureBuildHint::PreferFastTrace;
    std::uint32_t primitiveCount = 0;
    std::uint32_t instanceCount = 0;
    bool allowUpdate = false;
    bool allowCompaction = false;
};

struct RasterStateDesc
{
    PolygonFillMode fillMode = PolygonFillMode::Fill;
    CullMode cullMode = CullMode::Back;
    FrontFaceWinding frontFace = FrontFaceWinding::CounterClockwise;
    bool depthTest = true;
    bool depthWrite = true;
};

struct GraphicsPipelineDesc
{
    PrimitiveTopology topology = PrimitiveTopology::TriangleList;
    RasterStateDesc rasterState{};
    ShaderProgramDesc shaderProgram{};
};

struct GraphicsCapabilities
{
    GraphicsAPI api = GraphicsAPI::OpenGL;
    bool supportsRuntimeShaderCompilation = false;
    bool supportsComputeShaders = false;
    bool supportsGeometryShaders = false;
    bool supportsTessellationShaders = false;
    bool supportsFramebufferBlit = false;
    bool supportsWireframeRendering = false;
    bool supportsWindowPresentation = false;
    bool supportsRayTracingPipelines = false;
    bool supportsAccelerationStructures = false;
    bool supportsRayQueries = false;
    bool supportsTemporalUpscaling = false;
    bool supportsFrameGeneration = false;
    bool supportsOpticalFlow = false;
    std::uint32_t maxColorAttachments = 1;
    std::uint32_t maxAccelerationStructureInstances = 0;
};
