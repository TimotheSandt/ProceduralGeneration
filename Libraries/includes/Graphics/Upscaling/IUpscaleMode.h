#pragma once

#include <string_view>

class Renderer;

class IUpscaleMode
{
  public:
    virtual ~IUpscaleMode() = default;

    virtual std::string_view GetName() const noexcept = 0;
    virtual bool SupportsRenderer(const Renderer &renderer) const noexcept = 0;
    virtual void BeginPass(Renderer &renderer, int width, int height) const = 0;
    virtual void EndPass(const Renderer &renderer) const = 0;
};
