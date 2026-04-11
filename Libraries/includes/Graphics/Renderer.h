#pragma once

#include "Graphics/Upscaling/IUpscaleMode.h"

#include <glm/vec4.hpp>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class Renderer
{
  public:
    virtual ~Renderer() = default;

    virtual bool IsRuntimeCompatible() const noexcept = 0;
    virtual void BeginPass(int width, int height) = 0;
    virtual void EndPass() const = 0;
    virtual void Clear(const glm::vec4 &clearColor, bool clearDepth = true) const = 0;

    void BeginFrame(int width, int height) { BeginPass(width, height); }

    int GetFrameWidth() const noexcept { return frameWidth; }
    int GetFrameHeight() const noexcept { return frameHeight; }
    bool HasValidFrameExtent() const noexcept { return frameWidth > 0 && frameHeight > 0; }

    void RegisterUpscaleMode(std::unique_ptr<IUpscaleMode> mode);
    bool SetActiveUpscaleMode(std::string_view mode);
    std::string_view GetActiveUpscaleMode() const noexcept;
    std::vector<std::string_view> GetRegisteredUpscaleModes() const;

    void SetUpscalingEnabled(bool enabled) noexcept { upscalingEnabled = enabled; }
    bool IsUpscalingEnabled() const noexcept { return upscalingEnabled; }

  protected:
    void SetFrameExtent(int width, int height) noexcept
    {
        frameWidth = width;
        frameHeight = height;
    }

    void BeginUpscalePass(int width, int height);
    void EndUpscalePass() const;

  private:
    const IUpscaleMode *FindUpscaleMode(std::string_view mode) const noexcept;

    int frameWidth = 0;
    int frameHeight = 0;
    bool upscalingEnabled = false;
    std::vector<std::unique_ptr<IUpscaleMode>> upscaleModes;
    const IUpscaleMode *activeUpscaleMode = nullptr;
};
