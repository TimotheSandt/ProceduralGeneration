#pragma once

#include "Graphics/Upscaling/IUpscaleMode.h"

#include <string>
#include <string>

class RenderTargetUpscaleMode : public IUpscaleMode
{
  public:
    explicit RenderTargetUpscaleMode(std::string name);

    std::string GetName() const noexcept override;
    bool SupportsRenderer(const Renderer &renderer) const noexcept override;

  private:
    std::string name;
};
