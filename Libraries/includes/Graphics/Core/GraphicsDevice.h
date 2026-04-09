#pragma once

#include "Graphics/Core/GraphicsTypes.h"

#include <memory>
#include <string_view>

struct GraphicsDeviceCreateInfo
{
    bool enableValidation = false;
};

class IGraphicsDevice
{
  public:
    virtual ~IGraphicsDevice() = default;

    virtual GraphicsAPI GetAPI() const noexcept = 0;
    virtual std::string_view GetDeviceName() const noexcept = 0;
    virtual const GraphicsCapabilities &GetCapabilities() const noexcept = 0;
    virtual bool SupportsShaderStages(ShaderStageMask stages) const noexcept = 0;
};
