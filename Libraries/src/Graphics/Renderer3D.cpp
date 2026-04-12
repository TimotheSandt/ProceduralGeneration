#include "Renderer3D.h"

#include "Camera.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Upscaling/Modes/BilinearBlitUpscaleMode.h"
#include "Mesh.h"

Renderer3D::Renderer3D()
{
    RegisterUpscaleMode(std::make_unique<BilinearBlitUpscaleMode>());
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

    if (!UsesUpscaleRenderTarget())
    {
        GraphicsRenderState::BindDefaultFramebuffer();
    }
    else
    {
        BeginUpscalePass(width, height);
    }

    GraphicsRenderState::SetViewport(0, 0, GetFrameWidth(), GetFrameHeight());
    GraphicsRenderState::SetDepthTest(true);
    GraphicsRenderState::SetBlend(false);
    GraphicsRenderState::SetScissorTest(false);
}

void Renderer3D::EndPass() const
{
    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    if (!UsesUpscaleRenderTarget())
    {
        return;
    }

    EndUpscalePass();
}

void Renderer3D::Clear(const glm::vec4 &clearColor, bool clearDepth) const
{
    if (IsRuntimeCompatible() && HasValidFrameExtent())
    {
        GraphicsRenderState::ClearColor(clearColor);
        GraphicsRenderState::ClearBuffers(true, clearDepth);
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

bool Renderer3D::UsesUpscaleRenderTarget() const noexcept
{
    return IsUpscalingEnabled() && outputWidth > 0 && outputHeight > 0 &&
           (GetFrameWidth() != outputWidth || GetFrameHeight() != outputHeight);
}

RenderTarget &Renderer3D::GetUpscaleRenderTarget() { return sceneRenderTarget; }

const RenderTarget &Renderer3D::GetUpscaleRenderTarget() const { return sceneRenderTarget; }

int Renderer3D::GetUpscaleOutputWidth() const noexcept { return outputWidth; }

int Renderer3D::GetUpscaleOutputHeight() const noexcept { return outputHeight; }

void Renderer3D::PrepareUpscaleSource(RenderTarget &) {}

void Renderer3D::PrepareUpscalePresentState(const RenderTarget &) const
{
    GraphicsRenderState::SetScissorTest(false);
    GraphicsRenderState::SetBlend(false);
}
