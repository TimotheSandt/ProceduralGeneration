#include "Renderer3D.h"

#include "Camera.h"
#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Mesh.h"

class Renderer3DBilinearBlitUpscaleMode final : public IUpscaleMode
{
  public:
    std::string_view GetName() const noexcept override { return "bilinear-blit"; }
    bool SupportsRenderer(const Renderer &renderer) const noexcept override { return dynamic_cast<const Renderer3D *>(&renderer) != nullptr; }

    void BeginPass(Renderer &renderer, int, int) const override
    {
        auto &renderer3D = static_cast<Renderer3D &>(renderer);
        renderer3D.PrepareUpscaledScenePass();
    }

    void EndPass(const Renderer &renderer) const override
    {
        const auto &renderer3D = static_cast<const Renderer3D &>(renderer);
        renderer3D.PresentUpscaledScenePass();
    }
};

Renderer3D::Renderer3D()
{
    RegisterUpscaleMode(std::make_unique<Renderer3DBilinearBlitUpscaleMode>());
    static_cast<void>(SetActiveUpscaleMode("bilinear-blit"));
}

bool Renderer3D::IsRuntimeCompatible() const noexcept { return IsGraphicsAPIActive(GraphicsAPI::OpenGL); }

void Renderer3D::ConfigureOutput(int width, int height, bool enableUpscaling)
{
    outputWidth = width;
    outputHeight = height;
    SetUpscalingEnabled(enableUpscaling);
}

void Renderer3D::BeginPass(int width, int height)
{
    SetFrameExtent(width, height);

    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    if (!NeedsUpscaledScenePass())
    {
        OpenGLRenderState::BindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    else
    {
        BeginUpscalePass(width, height);
    }

    OpenGLRenderState::SetViewport(0, 0, GetFrameWidth(), GetFrameHeight());
    OpenGLRenderState::SetDepthTest(true);
    OpenGLRenderState::SetBlend(false);
    OpenGLRenderState::SetScissorTest(false);
}

void Renderer3D::EndPass() const
{
    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    if (!NeedsUpscaledScenePass())
    {
        return;
    }

    EndUpscalePass();
}

void Renderer3D::Clear(const glm::vec4 &clearColor, bool clearDepth) const
{
    if (IsRuntimeCompatible() && HasValidFrameExtent())
    {
        OpenGLRenderState::ClearColor(clearColor);
        OpenGLRenderState::Clear(clearDepth ? (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT) : GL_COLOR_BUFFER_BIT);
    }
}

void Renderer3D::SetCamera(Camera &camera) const
{
    if (IsRuntimeCompatible())
    {
        camera.BindUBO();
    }
}

void Renderer3D::DrawMesh(Mesh &mesh, Camera &camera) const
{
    if (IsRuntimeCompatible())
    {
        mesh.Render(camera);
    }
}

bool Renderer3D::NeedsUpscaledScenePass() const noexcept
{
    return IsUpscalingEnabled() && outputWidth > 0 && outputHeight > 0 && (GetFrameWidth() != outputWidth || GetFrameHeight() != outputHeight);
}

void Renderer3D::PrepareUpscaledScenePass()
{
    if (!NeedsUpscaledScenePass())
    {
        return;
    }

    if (sceneRenderTarget.GetID() == 0)
    {
        sceneRenderTarget.Init(GetFrameWidth(), GetFrameHeight());
    }
    else
    {
        sceneRenderTarget.Resize(GetFrameWidth(), GetFrameHeight());
    }
    sceneRenderTarget.Bind();
}

void Renderer3D::PresentUpscaledScenePass() const
{
    if (!NeedsUpscaledScenePass())
    {
        return;
    }

    sceneRenderTarget.Unbind();
    OpenGLRenderState::SetScissorTest(false);
    OpenGLRenderState::SetBlend(false);
    sceneRenderTarget.BlitToScreen(outputWidth, outputHeight);
}
