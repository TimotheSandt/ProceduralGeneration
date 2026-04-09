#include "suites/Suites.h"

#include "Graphics/Backends/Metal/MetalGraphicsBackend.h"
#include "Graphics/Backends/OpenGL/OpenGLGraphicsBackend.h"
#include "Graphics/Backends/Vulkan/VulkanGraphicsBackend.h"
#include "Graphics/Core/GraphicsDevice.h"
#include "Graphics/Core/GraphicsResources.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Core/GraphicsTypes.h"

namespace tests
{

namespace
{

class FakeAccelerationStructureResource final : public IAccelerationStructureResource
{
  public:
    explicit FakeAccelerationStructureResource(AccelerationStructureCreateInfo createInfo)
        : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
    {
    }

    GraphicsAPI GetAPI() const noexcept override { return GraphicsAPI::Vulkan; }
    std::string_view GetDebugName() const noexcept override { return debugName; }
    const AccelerationStructureDesc &GetDescription() const noexcept override { return desc; }

  private:
    AccelerationStructureDesc desc;
    std::string debugName;
};

class FakeShaderProgramResource final : public IShaderProgramResource
{
  public:
    explicit FakeShaderProgramResource(ShaderProgramCreateInfo createInfo)
        : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
    {
    }

    GraphicsAPI GetAPI() const noexcept override { return GraphicsAPI::Vulkan; }
    std::string_view GetDebugName() const noexcept override { return debugName; }
    const ShaderProgramDesc &GetDescription() const noexcept override { return desc; }
    void Bind() const override {}
    void Unbind() const override {}
    int GetUniformLocation(std::string_view) const override { return 0; }
    void SetFloatUniform(int, const float *, std::size_t) const override {}
    void SetIntUniform(int, const int *, std::size_t) const override {}
    void SetMatrix4Uniform(int, const float *) const override {}

  private:
    ShaderProgramDesc desc;
    std::string debugName;
};

class FakeBufferResource final : public IBufferResource
{
  public:
    explicit FakeBufferResource(BufferCreateInfo createInfo) : desc(createInfo.desc), debugName(std::move(createInfo.debugName)) {}

    GraphicsAPI GetAPI() const noexcept override { return GraphicsAPI::Vulkan; }
    std::string_view GetDebugName() const noexcept override { return debugName; }
    const BufferDesc &GetDescription() const noexcept override { return desc; }
    void Bind() const override {}
    void BindToBindingPoint(std::uint32_t) const override {}
    void Unbind() const override {}
    void UploadData(const void *, std::size_t, std::size_t) override {}
    void Resize(std::size_t newSize, bool) override { desc.sizeInBytes = newSize; }
    void *Map(BufferMapAccess) override { return nullptr; }
    void Unmap() override {}

  private:
    BufferDesc desc;
    std::string debugName;
};

class FakeGeometryResource final : public IGeometryResource
{
  public:
    explicit FakeGeometryResource(GeometryCreateInfo createInfo)
        : layout(std::move(createInfo.layout)), indexCount(createInfo.indexData.size()),
          instanceCount(createInfo.instanceData.empty() ? static_cast<std::size_t>(1) : createInfo.instanceData.size()),
          debugName(std::move(createInfo.debugName))
    {
    }

    GraphicsAPI GetAPI() const noexcept override { return GraphicsAPI::Vulkan; }
    std::string_view GetDebugName() const noexcept override { return debugName; }
    const GeometryLayout &GetLayout() const noexcept override { return layout; }
    std::size_t GetIndexCount() const noexcept override { return indexCount; }
    std::size_t GetInstanceCount() const noexcept override { return instanceCount; }
    void Bind() const override {}
    void Unbind() const override {}
    void DrawIndexed() const override {}
    void DrawIndexedInstanced() const override {}

  private:
    GeometryLayout layout;
    std::size_t indexCount;
    std::size_t instanceCount;
    std::string debugName;
};

class FakeTextureResource final : public ITextureResource
{
  public:
    explicit FakeTextureResource(TextureCreateInfo createInfo) : desc(createInfo.desc), debugName(std::move(createInfo.debugName)) {}

    GraphicsAPI GetAPI() const noexcept override { return GraphicsAPI::Vulkan; }
    std::string_view GetDebugName() const noexcept override { return debugName; }
    const TextureDesc &GetDescription() const noexcept override { return desc; }
    void Bind(std::uint32_t) const override {}
    void Unbind() const override {}
    void Readback(std::vector<std::byte> &output) const override { output.clear(); }
    void Resize(std::uint32_t width, std::uint32_t height) override { desc.extent = {width, height}; }
    void AttachToFramebuffer(std::uint32_t) const override {}

  private:
    TextureDesc desc;
    std::string debugName;
};

class FakeRenderTargetResource final : public IRenderTargetResource
{
  public:
    explicit FakeRenderTargetResource(RenderTargetCreateInfo createInfo) : desc(createInfo.desc), debugName(std::move(createInfo.debugName))
    {
    }

    GraphicsAPI GetAPI() const noexcept override { return GraphicsAPI::Vulkan; }
    std::string_view GetDebugName() const noexcept override { return debugName; }
    const RenderTargetDesc &GetDescription() const noexcept override { return desc; }
    void Bind() const override {}
    void Unbind() const override {}
    void Resize(std::uint32_t width, std::uint32_t height) override { desc.extent = {width, height}; }
    bool IsComplete() const override { return true; }
    void BlitTo(const IRenderTargetResource &, std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) const override {}
    void BlitToDefault(std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t) const override {}

  private:
    RenderTargetDesc desc;
    std::string debugName;
};

class FakeRayTracingDevice final : public IGraphicsDevice
{
  public:
    GraphicsAPI GetAPI() const noexcept override { return GraphicsAPI::Vulkan; }
    std::string_view GetDeviceName() const noexcept override { return "Fake Ray Tracing Device"; }
    const GraphicsCapabilities &GetCapabilities() const noexcept override { return capabilities; }

    bool SupportsShaderStages(ShaderStageMask stages) const noexcept override { return (stages & supportedStages) == stages; }

    std::unique_ptr<IBufferResource> CreateBuffer(const BufferCreateInfo &createInfo) const override
    {
        return std::make_unique<FakeBufferResource>(createInfo);
    }

    std::unique_ptr<IGeometryResource> CreateGeometry(const GeometryCreateInfo &createInfo) const override
    {
        return std::make_unique<FakeGeometryResource>(createInfo);
    }

    std::unique_ptr<IShaderProgramResource> CreateShaderProgram(const ShaderProgramCreateInfo &createInfo) const override
    {
        if (!SupportsShaderStages(createInfo.desc.stages))
        {
            return nullptr;
        }

        return std::make_unique<FakeShaderProgramResource>(createInfo);
    }

    std::unique_ptr<ITextureResource> CreateTexture(const TextureCreateInfo &createInfo) const override
    {
        return std::make_unique<FakeTextureResource>(createInfo);
    }

    std::unique_ptr<IRenderTargetResource> CreateRenderTarget(const RenderTargetCreateInfo &createInfo) const override
    {
        return std::make_unique<FakeRenderTargetResource>(createInfo);
    }

    std::unique_ptr<IAccelerationStructureResource> CreateAccelerationStructure(
        const AccelerationStructureCreateInfo &createInfo) const override
    {
        return std::make_unique<FakeAccelerationStructureResource>(createInfo);
    }

  private:
    static constexpr ShaderStageMask supportedStages =
        ShaderStageBit(ShaderStage::RayGeneration) | ShaderStageBit(ShaderStage::Miss) | ShaderStageBit(ShaderStage::ClosestHit);

    GraphicsCapabilities capabilities = {.api = GraphicsAPI::Vulkan,
                                         .supportsRuntimeShaderCompilation = false,
                                         .supportsComputeShaders = true,
                                         .supportsGeometryShaders = false,
                                         .supportsTessellationShaders = false,
                                         .supportsFramebufferBlit = true,
                                         .supportsWireframeRendering = true,
                                         .supportsWindowPresentation = true,
                                         .supportsRayTracingPipelines = true,
                                         .supportsAccelerationStructures = true,
                                         .supportsRayQueries = true,
                                         .maxColorAttachments = 8,
                                         .maxAccelerationStructureInstances = 1024};
};

} // namespace

TestSuite CreateGraphicsCoreSuite()
{
    TestSuite suite{"GraphicsCore"};

    AddTest(suite, "shader stage bitmask tracks enabled stages",
            []
            {
                const ShaderStageMask mask = ShaderStageBit(ShaderStage::Vertex) | ShaderStageBit(ShaderStage::Fragment);
                Assert(HasShaderStage(mask, ShaderStage::Vertex), "Vertex stage should be flagged in the mask");
                Assert(HasShaderStage(mask, ShaderStage::Fragment), "Fragment stage should be flagged in the mask");
                Assert(!HasShaderStage(mask, ShaderStage::Compute), "Compute stage should stay absent from the mask");
                Assert(!HasShaderStage(mask, ShaderStage::RayGeneration), "Ray generation stage should stay absent from the mask");
            });

    AddTest(suite, "graphics pipeline defaults stay raster friendly",
            []
            {
                const GraphicsPipelineDesc pipeline{};
                AssertEqual(pipeline.topology, PrimitiveTopology::TriangleList, "Pipelines should default to triangle lists");
                AssertEqual(pipeline.rasterState.fillMode, PolygonFillMode::Fill, "Pipelines should default to solid fill");
                AssertEqual(pipeline.rasterState.cullMode, CullMode::Back, "Pipelines should default to back-face culling");
                Assert(pipeline.rasterState.depthTest, "Pipelines should keep depth testing enabled by default");
                Assert(pipeline.rasterState.depthWrite, "Pipelines should keep depth writes enabled by default");
            });

    AddTest(suite, "texture description starts with a single mip",
            []
            {
                const TextureDesc textureDesc{};
                AssertEqual(textureDesc.format, TextureFormat::RGBA8, "Texture descriptions should default to RGBA8");
                AssertEqual(textureDesc.mipLevels, 1u, "Texture descriptions should default to one mip level");
                Assert(!textureDesc.renderTarget, "Texture descriptions should not default to render targets");
            });

    AddTest(suite, "render target description defaults to depth-backed rgba",
            []
            {
                const RenderTargetDesc renderTargetDesc{};
                AssertEqual(renderTargetDesc.colorFormat, TextureFormat::RGBA8, "Render targets should default to RGBA8 color");
                Assert(renderTargetDesc.hasDepthBuffer, "Render targets should default to a depth buffer");
            });

    AddTest(suite, "buffer description starts immutable vertex oriented",
            []
            {
                const BufferDesc bufferDesc{};
                AssertEqual(bufferDesc.usage, BufferUsage::Vertex, "Buffers should default to vertex usage");
                AssertEqual(bufferDesc.sizeInBytes, static_cast<std::size_t>(0), "Buffers should start with zero bytes");
                Assert(!bufferDesc.cpuWritable, "Buffers should default to GPU-only ownership");
            });

    AddTest(suite, "opengl device creates buffer resource descriptors",
            []
            {
                const OpenGLGraphicsBackend backend;
                const std::unique_ptr<IGraphicsDevice> device = backend.CreateDevice({});
                const std::unique_ptr<IBufferResource> buffer = device->CreateBuffer(
                    {.desc = {.usage = BufferUsage::Uniform, .sizeInBytes = 128, .cpuWritable = true}, .debugName = "camera_buffer"});

                Assert(buffer != nullptr, "OpenGL should create buffer resources");
                AssertEqual(buffer->GetAPI(), GraphicsAPI::OpenGL, "Buffer resources should keep the OpenGL API tag");
                AssertEqual(buffer->GetDescription().usage, BufferUsage::Uniform, "Buffer usage should be preserved");
                AssertEqual(buffer->GetDescription().sizeInBytes, static_cast<std::size_t>(128), "Buffer size should be preserved");
            });

    AddTest(suite, "acceleration structure description defaults to conservative tracing",
            []
            {
                const AccelerationStructureDesc desc{};
                AssertEqual(desc.type, AccelerationStructureType::BottomLevel,
                            "Acceleration structures should default to bottom-level builds");
                AssertEqual(desc.buildHint, AccelerationStructureBuildHint::PreferFastTrace,
                            "Acceleration structures should default to fast tracing");
                AssertEqual(desc.primitiveCount, 0u, "Acceleration structures should default to zero primitives");
                AssertEqual(desc.instanceCount, 0u, "Acceleration structures should default to zero instances");
                Assert(!desc.allowUpdate, "Acceleration structures should default to immutable builds");
                Assert(!desc.allowCompaction, "Acceleration structures should default to uncompacted builds");
            });

    AddTest(suite, "opengl backend reports runtime shader and window support",
            []
            {
                const OpenGLGraphicsBackend backend;
                const GraphicsCapabilities &capabilities = backend.GetCapabilities();

                AssertEqual(capabilities.api, GraphicsAPI::OpenGL, "OpenGL capabilities should identify the OpenGL API");
                Assert(capabilities.supportsRuntimeShaderCompilation, "OpenGL should support runtime shader compilation");
                Assert(capabilities.supportsFramebufferBlit, "OpenGL should support framebuffer blits");
                Assert(capabilities.supportsWireframeRendering, "OpenGL should support wireframe rendering");
                Assert(capabilities.supportsWindowPresentation, "OpenGL should support presenting to a window");
                Assert(!capabilities.supportsAccelerationStructures, "OpenGL should not report acceleration structures");
                Assert(!capabilities.supportsRayTracingPipelines, "OpenGL should not report ray tracing pipelines");
            });

    AddTest(suite, "opengl backend creates a graphics device with matching capabilities",
            []
            {
                const OpenGLGraphicsBackend backend;
                const std::unique_ptr<IGraphicsDevice> device = backend.CreateDevice({});

                Assert(device != nullptr, "OpenGL should be able to create a graphics device");
                AssertEqual(device->GetAPI(), GraphicsAPI::OpenGL, "OpenGL devices should report the OpenGL API");
                Assert(device->SupportsShaderStages(ShaderStageBit(ShaderStage::Vertex) | ShaderStageBit(ShaderStage::Fragment)),
                       "OpenGL devices should support vertex and fragment shader stages");
                Assert(!device->SupportsShaderStages(1u << 31u), "OpenGL devices should reject unknown shader stage bits");
            });

    AddTest(suite, "opengl device creates shader and texture resource descriptors",
            []
            {
                const OpenGLGraphicsBackend backend;
                const std::unique_ptr<IGraphicsDevice> device = backend.CreateDevice({});

                const std::unique_ptr<IShaderProgramResource> shaderProgram = device->CreateShaderProgram(
                    {.desc = {.stages = ShaderStageBit(ShaderStage::Vertex) | ShaderStageBit(ShaderStage::Fragment)},
                     .debugName = "ui_shader"});
                const std::unique_ptr<ITextureResource> texture = device->CreateTexture(
                    {.desc = {.extent = {256, 256}, .format = TextureFormat::RGBA8, .mipLevels = 1, .renderTarget = true},
                     .debugName = "ui_target"});

                Assert(shaderProgram != nullptr, "OpenGL should create shader program resources for supported shader stages");
                Assert(texture != nullptr, "OpenGL should create texture resources");
                AssertEqual(shaderProgram->GetAPI(), GraphicsAPI::OpenGL, "Shader resources should keep the OpenGL API tag");
                AssertEqual(shaderProgram->GetDebugName(), std::string_view("ui_shader"), "Shader debug names should be preserved");
                AssertEqual(texture->GetDescription().extent.width, 256u, "Texture width should be preserved in the resource descriptor");
                Assert(texture->GetDescription().renderTarget, "Texture descriptors should preserve render-target intent");
            });

    AddTest(suite, "opengl device creates render target resource descriptors",
            []
            {
                const OpenGLGraphicsBackend backend;
                const std::unique_ptr<IGraphicsDevice> device = backend.CreateDevice({});
                const std::unique_ptr<IRenderTargetResource> renderTarget = device->CreateRenderTarget(
                    {.desc = {.extent = {640, 360}, .colorFormat = TextureFormat::RGBA8, .hasDepthBuffer = true}, .debugName = "scene_rt"});

                Assert(renderTarget != nullptr, "OpenGL should create render target resources");
                AssertEqual(renderTarget->GetAPI(), GraphicsAPI::OpenGL, "Render target resources should keep the OpenGL API tag");
                AssertEqual(renderTarget->GetDescription().extent.width, 640u, "Render target width should be preserved");
                Assert(renderTarget->GetDescription().hasDepthBuffer, "Render targets should preserve depth-buffer intent");
            });

    AddTest(suite, "opengl device creates geometry resource descriptors",
            []
            {
                const OpenGLGraphicsBackend backend;
                const std::unique_ptr<IGraphicsDevice> device = backend.CreateDevice({});
                const std::unique_ptr<IGeometryResource> geometry =
                    device->CreateGeometry({.layout = {.vertexAttributes = {3, 3, 2}},
                                            .vertexData = {0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f,
                                                           0.0f, 1.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f},
                                            .indexData = {0, 1, 2},
                                            .debugName = "triangle_geometry"});

                Assert(geometry != nullptr, "OpenGL should create geometry resources");
                AssertEqual(geometry->GetAPI(), GraphicsAPI::OpenGL, "Geometry resources should keep the OpenGL API tag");
                AssertEqual(geometry->GetLayout().vertexAttributes.size(), static_cast<std::size_t>(3),
                            "Geometry layouts should preserve their vertex attribute count");
                AssertEqual(geometry->GetIndexCount(), static_cast<std::size_t>(3), "Geometry index counts should be preserved");
            });

    AddTest(suite, "opengl device rejects unsupported shader stage sets",
            []
            {
                const OpenGLGraphicsBackend backend;
                const std::unique_ptr<IGraphicsDevice> device = backend.CreateDevice({});
                ShaderProgramCreateInfo createInfo{};
                createInfo.desc.stages = 1u << 31u;
                createInfo.debugName = "invalid_shader";

                Assert(device->CreateShaderProgram(createInfo) == nullptr,
                       "OpenGL should reject shader programs with unsupported stage masks");
            });

    AddTest(suite, "opengl device rejects acceleration structure creation",
            []
            {
                const OpenGLGraphicsBackend backend;
                const std::unique_ptr<IGraphicsDevice> device = backend.CreateDevice({});
                const std::unique_ptr<IAccelerationStructureResource> accelerationStructure = device->CreateAccelerationStructure(
                    {.desc = {.type = AccelerationStructureType::BottomLevel, .primitiveCount = 12}, .debugName = "mesh_blas"});

                Assert(accelerationStructure == nullptr, "OpenGL should reject acceleration structure creation");
            });

    AddTest(
        suite, "fake ray tracing device exercises fully implemented acceleration structure flow",
        []
        {
            const FakeRayTracingDevice device;
            const std::unique_ptr<IShaderProgramResource> rayTracingProgram =
                device.CreateShaderProgram({.desc = {.stages = ShaderStageBit(ShaderStage::RayGeneration) |
                                                               ShaderStageBit(ShaderStage::Miss) | ShaderStageBit(ShaderStage::ClosestHit),
                                                     .runtimeCompilation = false},
                                            .debugName = "path_trace_pipeline"});
            const std::unique_ptr<IAccelerationStructureResource> accelerationStructure =
                device.CreateAccelerationStructure({.desc = {.type = AccelerationStructureType::TopLevel,
                                                             .buildHint = AccelerationStructureBuildHint::PreferFastTrace,
                                                             .instanceCount = 64,
                                                             .allowUpdate = true,
                                                             .allowCompaction = true},
                                                    .debugName = "scene_tlas"});
            const std::unique_ptr<IRenderTargetResource> renderTarget =
                device.CreateRenderTarget({.desc = {.extent = {1920, 1080}, .colorFormat = TextureFormat::RGBA8, .hasDepthBuffer = true},
                                           .debugName = "lighting_rt"});
            const std::unique_ptr<IBufferResource> storageBuffer = device.CreateBuffer(
                {.desc = {.usage = BufferUsage::Storage, .sizeInBytes = 4096, .cpuWritable = true}, .debugName = "light_storage"});
            const std::unique_ptr<IGeometryResource> geometry = device.CreateGeometry(
                {.layout = {.vertexAttributes = {3, 3}},
                 .vertexData = {0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f},
                 .indexData = {0, 1, 2},
                 .debugName = "rt_geometry"});

            Assert(device.GetCapabilities().supportsAccelerationStructures,
                   "The fake device should simulate acceleration-structure support");
            Assert(device.GetCapabilities().supportsRayTracingPipelines, "The fake device should simulate ray tracing pipeline support");
            Assert(rayTracingProgram != nullptr, "The fake device should create a ray tracing shader program");
            Assert(renderTarget != nullptr, "The fake device should create a render target resource");
            Assert(storageBuffer != nullptr, "The fake device should create buffer resources");
            Assert(geometry != nullptr, "The fake device should create geometry resources");
            Assert(accelerationStructure != nullptr, "The fake device should create an acceleration structure resource");
            AssertEqual(rayTracingProgram->GetDescription().stages,
                        ShaderStageBit(ShaderStage::RayGeneration) | ShaderStageBit(ShaderStage::Miss) |
                            ShaderStageBit(ShaderStage::ClosestHit),
                        "The fake shader resource should preserve ray tracing shader stages");
            AssertEqual(renderTarget->GetDescription().extent.height, 1080u, "The fake render target should preserve its output extent");
            AssertEqual(storageBuffer->GetDescription().usage, BufferUsage::Storage,
                        "The fake buffer resource should preserve storage-buffer usage");
            AssertEqual(geometry->GetIndexCount(), static_cast<std::size_t>(3), "The fake geometry resource should preserve index counts");
            AssertEqual(accelerationStructure->GetDescription().type, AccelerationStructureType::TopLevel,
                        "The fake acceleration structure should preserve the TLAS type");
            AssertEqual(accelerationStructure->GetDescription().instanceCount, 64u,
                        "The fake acceleration structure should preserve instance counts");
            Assert(accelerationStructure->GetDescription().allowUpdate, "The fake acceleration structure should preserve update flags");
            AssertEqual(accelerationStructure->GetDebugName(), std::string_view("scene_tlas"),
                        "The fake acceleration structure should preserve debug names");
        });

    AddTest(suite, "vulkan backend capabilities reflect explicit pipeline expectations",
            []
            {
                const VulkanGraphicsBackend backend;
                const GraphicsCapabilities &capabilities = backend.GetCapabilities();

                AssertEqual(capabilities.api, GraphicsAPI::Vulkan, "Vulkan capabilities should identify the Vulkan API");
                Assert(!capabilities.supportsRuntimeShaderCompilation, "Vulkan should not rely on runtime shader compilation");
                Assert(capabilities.supportsComputeShaders, "Vulkan should expose compute shader support");
                Assert(capabilities.supportsFramebufferBlit, "Vulkan should support blit-style transfers");
                Assert(!capabilities.supportsAccelerationStructures,
                       "Vulkan should keep acceleration structures disabled until the backend is implemented");
            });

    AddTest(suite, "metal backend capabilities keep wireframe optional",
            []
            {
                const MetalGraphicsBackend backend;
                const GraphicsCapabilities &capabilities = backend.GetCapabilities();

                AssertEqual(capabilities.api, GraphicsAPI::Metal, "Metal capabilities should identify the Metal API");
                Assert(capabilities.supportsComputeShaders, "Metal should expose compute shader support");
                Assert(!capabilities.supportsWireframeRendering, "Metal should keep wireframe support conservative by default");
            });

    AddTest(suite, "stub backends do not create devices yet",
            []
            {
                const VulkanGraphicsBackend vulkanBackend;
                const MetalGraphicsBackend metalBackend;

                Assert(vulkanBackend.CreateDevice({}) == nullptr, "Vulkan should not create a device before implementation");
                Assert(metalBackend.CreateDevice({}) == nullptr, "Metal should not create a device before implementation");
            });

    AddTest(suite, "graphics runtime bindings expose the active backend and device",
            []
            {
                const OpenGLGraphicsBackend backend;
                const std::unique_ptr<IGraphicsDevice> device = backend.CreateDevice({});

                BindGraphicsRuntime({.api = GraphicsAPI::OpenGL, .backend = &backend, .device = device.get()});

                AssertEqual(GetActiveGraphicsAPI(), GraphicsAPI::OpenGL, "Runtime should expose the bound graphics API");
                Assert(TryGetActiveGraphicsBackend() == &backend, "Runtime should expose the bound backend");
                Assert(TryGetActiveGraphicsDevice() == device.get(), "Runtime should expose the bound device");
                Assert(IsGraphicsAPIActive(GraphicsAPI::OpenGL), "Runtime should report the bound API as active");
                Assert(!IsGraphicsAPIActive(GraphicsAPI::Vulkan), "Runtime should reject non-bound APIs");

                ClearGraphicsRuntime();
                Assert(TryGetActiveGraphicsBackend() == nullptr, "Runtime should clear backend bindings");
                Assert(TryGetActiveGraphicsDevice() == nullptr, "Runtime should clear device bindings");
            });

    return suite;
}

} // namespace tests
