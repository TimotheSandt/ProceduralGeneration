#pragma once

#include "Renderer.h"
#include "RenderTarget.h"

#include <glm/vec4.hpp>

class Camera;
class Mesh;

class Renderer3D : public Renderer
{
  public:
    Renderer3D() : Renderer(GraphicsAPI::OpenGL) {}

    void SetCamera(Camera &camera) const;
    void DrawMesh(Mesh &mesh, Camera &camera) const;

  protected:
    void OnBeginPass() override;
    void OnClear(const glm::vec4 &clearColor, bool clearDepth) const override;
    RenderTarget &GetRenderTarget() override;
    const RenderTarget &GetRenderTarget() const override;

  private:
    RenderTarget sceneRenderTarget;
};
