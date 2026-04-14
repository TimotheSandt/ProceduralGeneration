#pragma once

#include "Graphics/Core/GraphicsResources.h"
#include "Graphics/Core/GraphicsTypes.h"
#include "ShaderProgram.h"
#include "Texture.h"

#include <cstdint>
#include <vector>

class RenderTarget
{
  public:
    RenderTarget() = default;
    // Convenience: single RGBA8 color attachment + depth renderbuffer.
    RenderTarget(int width, int height);
    // Full control: number of color attachments, formats, and depth mode are specified by desc.
    explicit RenderTarget(const RenderTargetDesc &desc);

    RenderTarget(const RenderTarget &) = delete;
    RenderTarget &operator=(const RenderTarget &) = delete;

    RenderTarget(RenderTarget &&) noexcept;
    RenderTarget &operator=(RenderTarget &&) noexcept;

    ~RenderTarget();

    // Allocate or reallocate with a single RGBA8 attachment (backward-compatible).
    void Init(int width, int height);
    // Allocate or reallocate with full attachment control.
    void Init(const RenderTargetDesc &desc);
    // Resize all attachments. Handles uninitialized state (calls Init).
    void Resize(int newWidth, int newHeight);
    // Resize if only dimensions changed; fully reinitialize if attachments/formats changed.
    void ResizeOrReconfigure(const RenderTargetDesc &desc);

    const RenderTargetDesc &GetDesc() const noexcept { return currentDesc; }
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

    // Color attachment access. Index 0 is always the primary color attachment.
    Texture &GetTexture(std::uint32_t index = 0) { return colorTextures[index]; }
    const Texture &GetTexture(std::uint32_t index = 0) const { return colorTextures[index]; }
    std::uint32_t GetColorAttachmentCount() const noexcept { return static_cast<std::uint32_t>(colorTextures.size()); }

    // Depth texture access. Only valid when the target was created with depthAsTexture = true.
    // Returns nullptr when depth is a renderbuffer (the default).
    Texture *TryGetDepthTexture() noexcept { return hasDepthTexture ? &depthTexture : nullptr; }
    const Texture *TryGetDepthTexture() const noexcept { return hasDepthTexture ? &depthTexture : nullptr; }

    int GetWidth() const { return width; }
    int GetHeight() const { return height; }

    // Legacy single-texture accessors (equivalent to GetTexture(0)).
    std::uint32_t GetTextureID() const { return colorTextures.empty() ? 0 : colorTextures[0].GetID(); }

  private:
    void InitAttachments(const RenderTargetDesc &desc);
    void Swap(RenderTarget &other) noexcept;
    void Setup();
    static bool AttachmentsMatch(const RenderTargetDesc &a, const RenderTargetDesc &b) noexcept;

  private:
    std::uint32_t ID = 0;
    std::uint32_t depthBufferID = 0;
    std::unique_ptr<IRenderTargetResource> backendRenderTarget;

    std::vector<Texture> colorTextures;
    Texture depthTexture;
    bool hasDepthTexture = false;

    int width = 0, height = 0;
    RenderTargetDesc currentDesc;

    std::unique_ptr<IGeometryResource> screenQuadGeometry;
    ShaderProgram screenQuadShaderProgram;
};
