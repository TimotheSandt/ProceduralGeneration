#pragma once

#include "Renderer.h"

#include <glm/vec4.hpp>

class Camera;
class Mesh;

class Renderer3D : public Renderer
{
  public:
    Renderer3D() : Renderer(GraphicsAPI::OpenGL) {}

    // Applies the current camera jitter (from SetCameraJitter), recomputes the camera
    // matrix, and uploads it to the GPU. Call once per frame before draw calls.
    void SetCamera(Camera &camera);
    void DrawMesh(Mesh &mesh, Camera &camera) const;

  protected:
    void OnBeginPass() override;
    void OnClear(const glm::vec4 &clearColor, bool clearDepth) const override;
    RenderTarget &GetRenderTarget() override;
    const RenderTarget &GetRenderTarget() const override;

  private:
    RenderTarget sceneRenderTarget;
    Camera *activeCamera = nullptr;
};
