#pragma once

#include "Graphics/Upscaling/IUpscaleMode.h"

class IGraphicsDevice;

class IAdvancedUpscaleMode : public IUpscaleMode
{
  public:
    virtual const UpscaleModeDesc &GetDescription() const noexcept = 0;
    virtual bool IsSupported(const GraphicsCapabilities &capabilities) const noexcept = 0;
    virtual bool Initialize(const IGraphicsDevice &device) = 0;
    virtual void Shutdown() = 0;
    virtual bool Execute(const UpscaleInput &input, UpscaleOutput &output) = 0;

    UpscaleRequirements GetRequirements() const noexcept override { return GetDescription().requirements; }
};
