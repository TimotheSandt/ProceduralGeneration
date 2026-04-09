#pragma once

#include "Graphics/Core/GraphicsResources.h"
#include "Shader.h"
#include "Texture.h"

#include <cstdint>

class FBO
{
  public:
    FBO() = default;
    FBO(int width, int height);

    FBO(const FBO &) = delete;
    FBO &operator=(const FBO &) = delete;

    FBO(FBO &&) noexcept;
    FBO &operator=(FBO &&) noexcept;

    ~FBO();

    void Init(int width, int height);
    void Resize(int newWidth, int newHeight);
    void Destroy();
    void Bind() const;
    void Unbind() const;

    void BlitFBO(FBO &oFBO) const;
    void BlitToScreen(int sWidth, int sHeight) const;
    void RenderScreenQuad() const;
    void RenderScreenQuad(int fWidth, int fHeight) const;

    std::uint32_t GetID() const { return ID; }
    Texture &GetTexture() { return TextureColor; }
    std::uint32_t GetTextureID() const { return TextureColor.GetID(); }
    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

  private:
    void Swap(FBO &other) noexcept;
    void Setup();

  private:
    std::uint32_t ID = 0;
    std::uint32_t depthBufferID = 0;
    std::unique_ptr<IRenderTargetResource> backendRenderTarget;
    Texture TextureColor;
    int width = 0, height = 0;

    std::unique_ptr<IGeometryResource> screenQuadGeometry;
    Shader screenQuadShader;
};
