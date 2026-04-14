#pragma once

#include "Graphics/Backend/GraphicsAPI.h"
#include "Graphics/RenderTarget.h"
#include "Graphics/Upscaling/IFrameGenerationMode.h"
#include "Graphics/Upscaling/IPostProcessPass.h"
#include "Graphics/Upscaling/IUpscaleMode.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string_view>
#include <vector>

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

    // -------------------------------------------------------------------------
    // Temporal resource configuration.
    // Call before BeginPass; changes take effect on the next frame.
    // -------------------------------------------------------------------------

    // Enable a motion vector attachment (RG16F) on the scene render target.
    // Scene shaders must write screen-space velocity to the second color output.
    void SetMotionVectorsEnabled(bool enabled) noexcept;
    bool IsMotionVectorsEnabled() const noexcept { return motionVectorsEnabled; }

    // Make the depth buffer readable as a texture in post-process / frame-gen shaders.
    // Slightly slower than a depth renderbuffer; only enable when actually needed.
    void SetDepthAsTextureEnabled(bool enabled) noexcept;
    bool IsDepthAsTextureEnabled() const noexcept { return depthAsTextureEnabled; }

    // Per-frame delta time — forwarded into FrameGenerationInput.
    void SetDeltaTime(float dt) noexcept { deltaTime = dt; }
    float GetDeltaTime() const noexcept { return deltaTime; }

    // Sub-pixel jitter applied to the camera this frame (e.g. Halton sequence for TAA).
    // Forwarded into FrameGenerationInput so the mode can undo/account for it.
    void SetCameraJitter(glm::vec2 jitter) noexcept { currentJitter = jitter; }
    glm::vec2 GetCameraJitter() const noexcept { return currentJitter; }

    // Call once on scene cuts or camera teleports so temporal modes can discard stale history.
    void ResetTemporalHistory() noexcept { resetHistoryNextFrame = true; }

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

    // Returns true when any active mode (frame generation or advanced upscale) needs
    // temporal resources (history, depth texture, or motion vectors).
    bool NeedsTemporalResources() const noexcept;

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

    // Temporal resource state
    bool motionVectorsEnabled = false;
    bool depthAsTextureEnabled = false;
    float deltaTime = 0.0f;
    glm::vec2 currentJitter = {0.0f, 0.0f};
    bool resetHistoryNextFrame = false;
    // Stores the previous frame's color (at render resolution) for temporal techniques.
    RenderTarget historyTarget;
};
