#pragma once

#include "Graphics/Upscaling/IUpscaleMode.h"

#include <string>
#include <string_view>

class RenderTargetUpscaleMode : public IUpscaleMode
{
  public:
    explicit RenderTargetUpscaleMode(std::string_view name);

    std::string_view GetName() const noexcept override;
    bool SupportsRenderer(const Renderer &renderer) const noexcept override;

  private:
    std::string name;
};
