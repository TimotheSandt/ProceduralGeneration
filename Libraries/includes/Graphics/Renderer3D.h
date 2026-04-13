#pragma once

#include "Renderer.h"
#include "RenderTarget.h"

#include <glm/vec4.hpp>

class Camera;
class Mesh;

class Renderer3D : public Renderer
{
  public:
    Renderer3D() = default;

    void SetCamera(Camera &camera) const;
    void DrawMesh(Mesh &mesh, Camera &camera) const;

  protected:
    GraphicsAPI GetRequiredAPI() const noexcept override;
    void OnBeginPass() override;
    void OnClear(const glm::vec4 &clearColor, bool clearDepth) const override;
    RenderTarget &GetRenderTarget() override;
    const RenderTarget &GetRenderTarget() const override;

  private:
    RenderTarget sceneRenderTarget;
};
