#include "suites/Suites.h"

#include "Graphics/Backends/Metal/MetalGraphicsBackend.h"
#include "Graphics/Backends/OpenGL/OpenGLGraphicsBackend.h"
#include "Graphics/Backends/Vulkan/VulkanGraphicsBackend.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Core/GraphicsTypes.h"

namespace tests
{

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

    AddTest(suite, "buffer description starts immutable vertex oriented",
            []
            {
                const BufferDesc bufferDesc{};
                AssertEqual(bufferDesc.usage, BufferUsage::Vertex, "Buffers should default to vertex usage");
                AssertEqual(bufferDesc.sizeInBytes, static_cast<std::size_t>(0), "Buffers should start with zero bytes");
                Assert(!bufferDesc.cpuWritable, "Buffers should default to GPU-only ownership");
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

    AddTest(suite, "vulkan backend capabilities reflect explicit pipeline expectations",
            []
            {
                const VulkanGraphicsBackend backend;
                const GraphicsCapabilities &capabilities = backend.GetCapabilities();

                AssertEqual(capabilities.api, GraphicsAPI::Vulkan, "Vulkan capabilities should identify the Vulkan API");
                Assert(!capabilities.supportsRuntimeShaderCompilation, "Vulkan should not rely on runtime shader compilation");
                Assert(capabilities.supportsComputeShaders, "Vulkan should expose compute shader support");
                Assert(capabilities.supportsFramebufferBlit, "Vulkan should support blit-style transfers");
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
