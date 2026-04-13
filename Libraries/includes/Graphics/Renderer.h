#pragma once

#include "Graphics/Backend/GraphicsAPI.h"
#include "Graphics/Upscaling/IFrameGenerationMode.h"
#include "Graphics/Upscaling/IPostProcessPass.h"
#include "Graphics/Upscaling/IUpscaleMode.h"

#include <glm/vec4.hpp>
#include <memory>
#include <string_view>
#include <vector>

class RenderTarget;

class Renderer
{
  public:
    explicit Renderer(GraphicsAPI requiredApi);
    virtual ~Renderer();

    bool IsRuntimeCompatible() const noexcept;

    // -------------------------------------------------------------------------
    // Output configuration — call once on startup, again on window resize.
    // -------------------------------------------------------------------------
    void SetOutputResolution(int width, int height) noexcept;
    int GetOutputWidth() const noexcept { return outputWidth; }
    int GetOutputHeight() const noexcept { return outputHeight; }

    // -------------------------------------------------------------------------
    // Render scale — values in (0, 1) enable upscaling; 1.0 disables it.
    // -------------------------------------------------------------------------
    void SetRenderScale(float scale) noexcept;
    float GetRenderScale() const noexcept { return renderScale; }
    void SetUpscalingEnabled(bool enabled) noexcept;
    bool IsUpscalingEnabled() const noexcept { return upscalingEnabled; }

    // -------------------------------------------------------------------------
    // Frame extent — valid only after BeginPass(), reset at next BeginPass().
    // -------------------------------------------------------------------------
    int GetFrameWidth() const noexcept { return frameWidth; }
    int GetFrameHeight() const noexcept { return frameHeight; }
    bool HasValidFrameExtent() const noexcept { return frameWidth > 0 && frameHeight > 0; }

    // -------------------------------------------------------------------------
    // Per-frame pipeline.
    // -------------------------------------------------------------------------
    void BeginPass();
    void EndPass();
    void Clear(const glm::vec4 &clearColor, bool clearDepth = true) const;

    // -------------------------------------------------------------------------
    // Upscale modes.
    // -------------------------------------------------------------------------
    void RegisterUpscaleMode(std::unique_ptr<IUpscaleMode> mode);
    bool SetActiveUpscaleMode(std::string_view name);
    std::string_view GetActiveUpscaleMode() const noexcept;
    std::vector<std::string_view> GetRegisteredUpscaleModes() const;
    UpscaleRequirements GetActiveUpscaleModeRequirements() const noexcept;

    // -------------------------------------------------------------------------
    // Post-processing passes — executed in order before upscaling.
    // -------------------------------------------------------------------------
    void AddPostProcessPass(std::unique_ptr<IPostProcessPass> pass);
    void RemovePostProcessPass(std::string_view name);

    // -------------------------------------------------------------------------
    // Frame generation — executed after upscaling.
    // -------------------------------------------------------------------------
    void SetFrameGenerationMode(std::unique_ptr<IFrameGenerationMode> mode);

  protected:
    virtual void OnBeginPass() = 0;
    virtual void OnEndPass() {}
    virtual void OnClear(const glm::vec4 &clearColor, bool clearDepth) const = 0;
    virtual RenderTarget &GetRenderTarget() = 0;
    virtual const RenderTarget &GetRenderTarget() const = 0;

    void SetFrameExtent(int width, int height) noexcept
    {
        frameWidth = width;
        frameHeight = height;
    }

    bool UsesRenderTarget() const noexcept;
    void GetRenderResolution(int &width, int &height) const noexcept;

  private:
    bool IsScaledRendering() const noexcept;

    GraphicsAPI requiredApi;

    int outputWidth = 0;
    int outputHeight = 0;
    float renderScale = 1.0f;
    bool upscalingEnabled = false;

    int frameWidth = 0;
    int frameHeight = 0;

    std::vector<std::unique_ptr<IUpscaleMode>> upscaleModes;
    const IUpscaleMode *activeUpscaleMode = nullptr;

    std::vector<std::unique_ptr<IPostProcessPass>> postProcessPasses;

    std::unique_ptr<IFrameGenerationMode> frameGenMode;
};
