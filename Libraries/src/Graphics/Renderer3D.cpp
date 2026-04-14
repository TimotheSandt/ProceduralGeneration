#include "Renderer3D.h"

#include "Camera.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Mesh.h"

void Renderer3D::OnBeginPass()
{
    GraphicsRenderState::SetViewport(0, 0, GetFrameWidth(), GetFrameHeight());
    GraphicsRenderState::SetDepthTest(true);
    GraphicsRenderState::SetBlend(false);
    GraphicsRenderState::SetScissorTest(false);
}

void Renderer3D::OnClear(const glm::vec4 &clearColor, bool clearDepth) const
{
    GraphicsRenderState::ClearColor(clearColor);
    GraphicsRenderState::ClearBuffers(true, clearDepth);
}

void Renderer3D::SetCamera(Camera &camera)
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    activeCamera = &camera;

    // Apply the current sub-pixel jitter (zero when no temporal mode is active).
    camera.SetJitter(GetCameraJitter());
    camera.UpdateMatrix();
    camera.Bind();
}

void Renderer3D::DrawMesh(Mesh &mesh, Camera &camera) const
{
    if (IsRuntimeCompatible())
    {
        mesh.Render(camera);
    }
}

RenderTarget &Renderer3D::GetRenderTarget() { return sceneRenderTarget; }

const RenderTarget &Renderer3D::GetRenderTarget() const { return sceneRenderTarget; }
