#pragma once

#include <string_view>

class Renderer;
class RenderTarget;

class IUpscaleMode
{
  public:
    virtual ~IUpscaleMode() = default;

    virtual std::string_view GetName() const noexcept = 0;
    virtual bool SupportsRenderer(const Renderer &renderer) const noexcept = 0;
    virtual void Upscale(const RenderTarget &source, int outputWidth, int outputHeight) const = 0;
};
