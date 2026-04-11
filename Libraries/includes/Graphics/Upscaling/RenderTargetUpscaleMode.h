#pragma once

#include "Graphics/Renderer.h"

#include <string_view>

class RenderTarget;

class RenderTargetUpscaleMode : public IUpscaleMode
{
  public:
    explicit RenderTargetUpscaleMode(std::string_view name) noexcept;

    std::string_view GetName() const noexcept override;
    bool SupportsRenderer(const Renderer &renderer) const noexcept override;
    void BeginPass(Renderer &renderer, int width, int height) const override;
    void EndPass(const Renderer &renderer) const override;

  protected:
    int GetOutputWidth(const ConstUpscalePassContext &context) const noexcept;
    int GetOutputHeight(const ConstUpscalePassContext &context) const noexcept;
    virtual void PresentUpscaled(const ConstUpscalePassContext &context, const RenderTarget &renderTarget) const = 0;

  private:
    std::string_view name;
};
