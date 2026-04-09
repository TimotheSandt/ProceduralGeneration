#include "Renderer3D.h"

#include "Camera.h"
#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Mesh.h"
#include "Window.h"
#include "World.h"

bool Renderer3D::IsRuntimeCompatible() const noexcept { return IsGraphicsAPIActive(GraphicsAPI::OpenGL); }

void Renderer3D::BeginFrame(int width, int height)
{
    frameWidth = width;
    frameHeight = height;

    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    OpenGLRenderState::PrepareScreenPass(frameWidth, frameHeight);
}

void Renderer3D::Clear(const Window &window) const
{
    if (IsRuntimeCompatible() && HasValidFrameExtent())
    {
        window.Clear();
    }
}

void Renderer3D::BindCamera(Camera &camera) const
{
    if (IsRuntimeCompatible())
    {
        camera.BindUBO();
    }
}

void Renderer3D::RenderMesh(Mesh &mesh, Camera &camera) const
{
    if (IsRuntimeCompatible())
    {
        mesh.Render(camera);
    }
}

void Renderer3D::RenderWorld(World &world, Camera &camera) const
{
    if (IsRuntimeCompatible())
    {
        world.Render(*this, camera);
    }
}
