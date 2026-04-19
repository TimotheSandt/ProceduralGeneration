#pragma once

#include "Graphics/Upscaling/UpscaleTypes.h"

#include <string>

class Renderer;
class RenderTarget;

class IUpscaleMode
{
  public:
    virtual ~IUpscaleMode() = default;

    virtual std::string GetName() const noexcept = 0;
    virtual bool SupportsRenderer(const Renderer &renderer) const noexcept = 0;
    virtual void Upscale(const RenderTarget &source, int outputWidth, int outputHeight) const = 0;

    virtual UpscaleRequirements GetRequirements() const noexcept { return {}; }
};
