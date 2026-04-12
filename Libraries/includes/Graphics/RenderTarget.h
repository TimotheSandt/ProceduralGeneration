#pragma once

#include "Graphics/Core/GraphicsResources.h"
#include "ShaderProgram.h"
#include "Texture.h"

#include <cstdint>

class RenderTarget
{
  public:
    RenderTarget() = default;
    RenderTarget(int width, int height);

    RenderTarget(const RenderTarget &) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;

    RenderTarget(RenderTarget &&) noexcept;
    RenderTarget &operator=(RenderTarget &&) noexcept;

    ~RenderTarget();

    void Init(int width, int height);
    void Resize(int newWidth, int newHeight);
    void Destroy();
    void Bind() const;
    void Unbind() const;

    void CopyFromScreen(int srcWidth, int srcHeight) const;
    void BlitToRenderTarget(RenderTarget &destination) const;
    void BlitToScreen(int sWidth, int sHeight) const;
    void RenderScreenQuad() const;
    void RenderScreenQuad(int fWidth, int fHeight) const;

    bool IsInitialized() const noexcept { return backendRenderTarget != nullptr; }
    std::uint32_t GetID() const { return ID; }
    Texture &GetTexture() { return colorTexture; }
    std::uint32_t GetTextureID() const { return colorTexture.GetID(); }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

  private:
    void Swap(RenderTarget &other) noexcept;
    void Setup();

  private:
    std::uint32_t ID = 0;
    std::uint32_t depthBufferID = 0;
    std::unique_ptr<IRenderTargetResource> backendRenderTarget;
    Texture colorTexture;
    int width = 0, height = 0;

    std::unique_ptr<IGeometryResource> screenQuadGeometry;
    ShaderProgram screenQuadShaderProgram;
};
