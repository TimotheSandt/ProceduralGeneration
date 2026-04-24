#pragma once

#include "Graphics/Upscaling/RenderTargetUpscaleMode.h"

class BilinearBlitUpscaleMode final : public RenderTargetUpscaleMode
{
  public:
    explicit BilinearBlitUpscaleMode(std::string name = "bilinear-blit");

    void Upscale(const RenderTarget &source, int outputWidth, int outputHeight) const override;
};
