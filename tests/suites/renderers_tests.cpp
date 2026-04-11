#include "suites/Suites.h"

#include "Graphics/Backends/OpenGL/OpenGLGraphicsBackend.h"
#include "Graphics/Backends/Vulkan/VulkanGraphicsBackend.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Renderer2D.h"
#include "Graphics/Renderer3D.h"

namespace tests
{

TestSuite CreateRenderersSuite()
{
    TestSuite suite{"Renderers"};

    AddTest(suite, "renderer2d tracks the current frame extent",
            []
            {
                Renderer2D renderer2D;
                renderer2D.BeginPass(1280, 720);

                AssertEqual(renderer2D.GetFrameWidth(), 1280, "Renderer2D should store the current frame width");
                AssertEqual(renderer2D.GetFrameHeight(), 720, "Renderer2D should store the current frame height");
                Assert(renderer2D.HasValidFrameExtent(), "Renderer2D should consider positive extents valid");
            });

    AddTest(suite, "renderer3d tracks the current frame extent",
            []
            {
                Renderer3D renderer3D;
                renderer3D.BeginPass(1920, 1080);

                AssertEqual(renderer3D.GetFrameWidth(), 1920, "Renderer3D should store the current frame width");
                AssertEqual(renderer3D.GetFrameHeight(), 1080, "Renderer3D should store the current frame height");
                Assert(renderer3D.HasValidFrameExtent(), "Renderer3D should consider positive extents valid");
            });

    AddTest(suite, "renderers require an active opengl runtime",
            []
            {
                Renderer2D renderer2D;
                Renderer3D renderer3D;

                ClearGraphicsRuntime();

                Assert(!renderer2D.IsRuntimeCompatible(), "Renderer2D should reject an unbound runtime");
                Assert(!renderer3D.IsRuntimeCompatible(), "Renderer3D should reject an unbound runtime");

                const OpenGLGraphicsBackend backend;
                const std::unique_ptr<IGraphicsDevice> device = backend.CreateDevice({});
                BindGraphicsRuntime({.api = GraphicsAPI::OpenGL, .backend = &backend, .device = device.get()});

                Assert(renderer2D.IsRuntimeCompatible(), "Renderer2D should accept the OpenGL runtime");
                Assert(renderer3D.IsRuntimeCompatible(), "Renderer3D should accept the OpenGL runtime");

                ClearGraphicsRuntime();
            });

    AddTest(suite, "renderers reject non-opengl runtimes for now",
            []
            {
                Renderer2D renderer2D;
                Renderer3D renderer3D;
                const VulkanGraphicsBackend backend;

                BindGraphicsRuntime({.api = GraphicsAPI::Vulkan, .backend = &backend, .device = nullptr});

                Assert(!renderer2D.IsRuntimeCompatible(), "Renderer2D should reject Vulkan until the 2D Vulkan path exists");
                Assert(!renderer3D.IsRuntimeCompatible(), "Renderer3D should reject Vulkan until the 3D Vulkan path exists");

                ClearGraphicsRuntime();
            });

    AddTest(suite, "renderers expose runtime-selectable upscale modes",
            []
            {
                Renderer2D renderer2D;
                Renderer3D renderer3D;

                AssertEqual(std::string(renderer2D.GetActiveUpscaleMode()), std::string("bilinear-blit"),
                            "Renderer2D should default to its shared bilinear blit upscale path");
                AssertEqual(std::string(renderer3D.GetActiveUpscaleMode()), std::string("bilinear-blit"),
                            "Renderer3D should default to its shared bilinear blit upscale path");
                AssertEqual(renderer2D.GetRegisteredUpscaleModes().size(), std::size_t(1),
                            "Renderer2D should expose its registered upscale strategies");
                AssertEqual(renderer3D.GetRegisteredUpscaleModes().size(), std::size_t(1),
                            "Renderer3D should expose its registered upscale strategies");

                Assert(renderer2D.SetActiveUpscaleMode("disabled"), "Renderer2D should accept the disabled upscale mode");
                Assert(renderer3D.SetActiveUpscaleMode("disabled"), "Renderer3D should accept the disabled upscale mode");
                Assert(!renderer2D.SetActiveUpscaleMode("unknown-mode"), "Renderer2D should reject unknown upscale modes");
                Assert(!renderer3D.SetActiveUpscaleMode("unknown-mode"), "Renderer3D should reject unknown upscale modes");
            });

    return suite;
}

} // namespace tests
