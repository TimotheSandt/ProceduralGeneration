#pragma once

#include "Graphics/RenderTarget.h"
#include "Graphics/Upscaling/IUpscaleMode.h"

class IGraphicsDevice;

// Base for temporally-aware upscaling techniques (DLSS, FSR, XeSS, custom TAA).
//
// Subclasses implement Execute() with the full UpscaleInput (depth, motion vectors,
// history, jitter, etc.). The renderer detects IAdvancedUpscaleMode at EndPass and
// calls Execute() directly with all available resources. The default Upscale()
// override below forwards to Execute() as a minimal fallback (colour only).
class IAdvancedUpscaleMode : public IUpscaleMode
{
  public:
    virtual const UpscaleModeDesc &GetDescription() const noexcept = 0;
    virtual bool IsSupported(const GraphicsCapabilities &capabilities) const noexcept = 0;
    virtual bool Initialize(const IGraphicsDevice &device) = 0;
    virtual void Shutdown() = 0;
    virtual bool Execute(const UpscaleInput &input, UpscaleOutput &output) = 0;

    UpscaleRequirements GetRequirements() const noexcept override { return GetDescription().requirements; }

    // Default implementation: forward to Execute() with colour-only input.
    // Overridden by the renderer when the full resource set is available.
    void Upscale(const RenderTarget &source, int outputWidth, int outputHeight) const override
    {
        UpscaleInput input;
        input.color            = &source.GetTexture(0);
        input.renderResolution = {static_cast<float>(source.GetWidth()), static_cast<float>(source.GetHeight())};
        input.outputResolution = {static_cast<float>(outputWidth), static_cast<float>(outputHeight)};

        UpscaleOutput output;
        // const_cast is safe: the renderer owns this object mutably; const here is
        // an artefact of IUpscaleMode::Upscale being const.
        const_cast<IAdvancedUpscaleMode *>(this)->Execute(input, output);
    }
};
