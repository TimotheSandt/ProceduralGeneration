#include "Renderer3D.h"

#include "Camera.h"
#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Mesh.h"

bool Renderer3D::IsRuntimeCompatible() const noexcept { return IsGraphicsAPIActive(GraphicsAPI::OpenGL); }

void Renderer3D::ConfigureOutput(int width, int height, bool enableUpscaling)
{
    outputWidth = width;
    outputHeight = height;
    upscalingEnabled = enableUpscaling;
}

void Renderer3D::BeginPass(int width, int height)
{
    frameWidth = width;
    frameHeight = height;

    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    if (upscalingEnabled && outputWidth > 0 && outputHeight > 0 && (frameWidth != outputWidth || frameHeight != outputHeight))
    {
        if (sceneRenderTarget.GetID() == 0)
        {
            sceneRenderTarget.Init(frameWidth, frameHeight);
        }
        else
        {
            sceneRenderTarget.Resize(frameWidth, frameHeight);
        }
        sceneRenderTarget.Bind();
    }
    else
    {
        OpenGLRenderState::BindFramebuffer(GL_FRAMEBUFFER, 0);
    }

    OpenGLRenderState::SetViewport(0, 0, frameWidth, frameHeight);
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

    if (!upscalingEnabled || outputWidth <= 0 || outputHeight <= 0 || (frameWidth == outputWidth && frameHeight == outputHeight))
    {
        return;
    }

    sceneRenderTarget.Unbind();
    OpenGLRenderState::SetScissorTest(false);
    OpenGLRenderState::SetBlend(false);
    sceneRenderTarget.BlitToScreen(outputWidth, outputHeight);
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
