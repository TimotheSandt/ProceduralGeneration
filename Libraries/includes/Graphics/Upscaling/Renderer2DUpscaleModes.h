#pragma once

#include "Graphics/Upscaling/IUpscaleMode.h"

class Renderer2D;

class Renderer2DCompositeUpscaleMode final : public IUpscaleMode
{
  public:
    std::string_view GetName() const noexcept override;
    bool SupportsRenderer(const Renderer &renderer) const noexcept override;
    void BeginPass(Renderer &renderer, int width, int height) const override;
    void EndPass(const Renderer &renderer) const override;
};
