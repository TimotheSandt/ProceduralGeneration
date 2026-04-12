#pragma once

#include "Graphics/Upscaling/IUpscaleMode.h"

#include <memory>
#include <string_view>
#include <vector>

class Renderer;

class PresentationController
{
  public:
    void RegisterMode(std::unique_ptr<IUpscaleMode> mode);
    bool SetActiveMode(std::string_view mode, const Renderer &renderer);
    std::string_view GetActiveMode() const noexcept;
    std::vector<std::string_view> GetRegisteredModes() const;

    void SetEnabled(bool enabled) noexcept { enabledForRenderer = enabled; }
    bool IsEnabled() const noexcept { return enabledForRenderer; }

    void Begin(Renderer &renderer, int width, int height) const;
    void End(const Renderer &renderer) const;

  private:
    const IUpscaleMode *FindMode(std::string_view mode) const noexcept;

    bool enabledForRenderer = false;
    std::vector<std::unique_ptr<IUpscaleMode>> modes;
    const IUpscaleMode *activeMode = nullptr;
};
