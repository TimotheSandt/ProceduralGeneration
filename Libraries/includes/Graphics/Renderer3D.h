#pragma once

#include "Renderer.h"
#include "RenderTarget.h"

#include <glm/vec4.hpp>

class Camera;
class Mesh;

class Renderer3D : public Renderer
{
  public:
    Renderer3D();

    bool IsRuntimeCompatible() const noexcept override;
    void ConfigureOutput(int outputWidth, int outputHeight, bool enableUpscaling);
    void BeginPass(int width, int height) override;
    void EndPass() const override;

    void Clear(const glm::vec4 &clearColor, bool clearDepth = true) const override;
    void SetCamera(Camera &camera) const;
    void DrawMesh(Mesh &mesh, Camera &camera) const;
    void BindCamera(Camera &camera) const { SetCamera(camera); }
    void RenderMesh(Mesh &mesh, Camera &camera) const { DrawMesh(mesh, camera); }

  private:
    bool UsesUpscaleRenderTarget() const noexcept override;
    RenderTarget &GetUpscaleRenderTarget() override;
    const RenderTarget &GetUpscaleRenderTarget() const override;
    int GetUpscaleOutputWidth() const noexcept override;
    int GetUpscaleOutputHeight() const noexcept override;
    void PrepareUpscaleSource(RenderTarget &renderTarget) override;
    void PrepareUpscalePresentState(const RenderTarget &renderTarget) const override;

    int outputWidth = 0;
    int outputHeight = 0;
    RenderTarget sceneRenderTarget;
};
