#pragma once

#include "Graphics/Backend/GraphicsAPI.h"
#include "Graphics/RenderTarget.h"
#include "Graphics/Upscaling/IFrameGenerationMode.h"
#include "Graphics/Upscaling/IPostProcessPass.h"
#include "Graphics/Upscaling/IUpscaleMode.h"

#include <glm/vec2.hpp>
#include <glm/vec4.hpp>
#include <memory>
#include <string>
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
    bool SetActiveUpscaleMode(std::string name);
    std::string GetActiveUpscaleMode() const noexcept;
    std::vector<std::string> GetRegisteredUpscaleModes() const;
    UpscaleRequirements GetActiveUpscaleModeRequirements() const noexcept;

    // -------------------------------------------------------------------------
    // Post-processing passes — executed in order before upscaling.
    // -------------------------------------------------------------------------
    void AddPostProcessPass(std::unique_ptr<IPostProcessPass> pass);
    void RemovePostProcessPass(std::string name);

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

    // Jitter mode — controls how the sub-pixel camera offset is generated each frame.
    //   Auto (default): the renderer advances a Halton(2,3) sequence automatically
    //                   whenever NeedsTemporalResources() is true. No user action needed.
    //   Manual:         the renderer uses whatever value was last passed to SetCameraJitter.
    enum class JitterMode : std::uint8_t { Auto, Manual };
    void SetJitterMode(JitterMode mode) noexcept { jitterMode = mode; }
    JitterMode GetJitterMode() const noexcept { return jitterMode; }

    // Override the jitter for the current frame (switches to Manual mode implicitly).
    // Pass {0, 0} to disable jitter.
    void SetCameraJitter(glm::vec2 jitter) noexcept { currentJitter = jitter; jitterMode = JitterMode::Manual; }
    glm::vec2 GetCameraJitter() const noexcept { return currentJitter; }

    // Number of samples in the jitter sequence before it repeats (default: 16).
    // Higher values reduce repetition artifacts in long temporal accumulation.
    void SetJitterSequenceLength(std::uint32_t length) noexcept;
    std::uint32_t GetJitterSequenceLength() const noexcept { return jitterSequenceLength; }

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
    IUpscaleMode *activeUpscaleMode = nullptr;

    std::vector<std::unique_ptr<IPostProcessPass>> postProcessPasses;

    std::unique_ptr<IFrameGenerationMode> frameGenMode;

    // Temporal resource state
    bool motionVectorsEnabled = false;
    bool depthAsTextureEnabled = false;
    float deltaTime = 0.0f;
    glm::vec2 currentJitter = {0.0f, 0.0f};
    bool resetHistoryNextFrame = false;
    JitterMode jitterMode = JitterMode::Auto;
    std::uint32_t jitterSequenceLength = 16;
    std::uint32_t jitterIndex = 0;
    // Stores the previous frame's color (at render resolution) for temporal techniques.
    RenderTarget historyTarget;
};
