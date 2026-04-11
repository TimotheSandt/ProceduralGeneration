#pragma once

#include "RenderTarget.h"

#include <glm/vec4.hpp>

class Camera;
class Mesh;

class Renderer3D
{
  public:
    bool IsRuntimeCompatible() const noexcept;
    void ConfigureOutput(int outputWidth, int outputHeight, bool enableUpscaling);
    void BeginPass(int width, int height);
    void EndPass() const;

    void Clear(const glm::vec4 &clearColor, bool clearDepth = true) const;
    void SetCamera(Camera &camera) const;
    void DrawMesh(Mesh &mesh, Camera &camera) const;

    void BeginFrame(int width, int height) { BeginPass(width, height); }
    void BindCamera(Camera &camera) const { SetCamera(camera); }
    void RenderMesh(Mesh &mesh, Camera &camera) const { DrawMesh(mesh, camera); }

    int GetFrameWidth() const noexcept { return frameWidth; }
    int GetFrameHeight() const noexcept { return frameHeight; }
    bool HasValidFrameExtent() const noexcept { return frameWidth > 0 && frameHeight > 0; }

  private:
    int outputWidth = 0;
    int outputHeight = 0;
    bool upscalingEnabled = false;
    int frameWidth = 0;
    int frameHeight = 0;
    RenderTarget sceneRenderTarget;
};
