#pragma once

class Camera;
class Mesh;
class Window;
class World;

class Renderer3D
{
  public:
    bool IsRuntimeCompatible() const noexcept;
    void BeginFrame(int width, int height);

    void Clear(const Window &window) const;
    void BindCamera(Camera &camera) const;
    void RenderMesh(Mesh &mesh, Camera &camera) const;
    void RenderWorld(World &world, Camera &camera) const;

    int GetFrameWidth() const noexcept { return frameWidth; }
    int GetFrameHeight() const noexcept { return frameHeight; }
    bool HasValidFrameExtent() const noexcept { return frameWidth > 0 && frameHeight > 0; }

  private:
    int frameWidth = 0;
    int frameHeight = 0;
};
