#pragma once

#include "Graphics/Upscaling/UpscaleTypes.h"

class IGraphicsDevice;

class IFrameGenerationMode
{
  public:
    virtual ~IFrameGenerationMode() = default;

    virtual const FrameGenerationModeDesc &GetDescription() const noexcept = 0;
    virtual bool IsSupported(const GraphicsCapabilities &capabilities) const noexcept = 0;
    virtual bool Initialize(const IGraphicsDevice &device) = 0;
    virtual void Shutdown() = 0;
    virtual bool GenerateFrame(const FrameGenerationInput &input, FrameGenerationOutput &output) = 0;
};
