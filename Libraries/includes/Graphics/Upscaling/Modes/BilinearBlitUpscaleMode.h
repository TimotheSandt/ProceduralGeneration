#pragma once

#include "Graphics/Upscaling/RenderTargetUpscaleMode.h"

class BilinearBlitUpscaleMode final : public RenderTargetUpscaleMode
{
  public:
    explicit BilinearBlitUpscaleMode(std::string_view name = "bilinear-blit") noexcept;

  protected:
    void PresentUpscaled(const Renderer &renderer, const RenderTarget &renderTarget) const override;
};
