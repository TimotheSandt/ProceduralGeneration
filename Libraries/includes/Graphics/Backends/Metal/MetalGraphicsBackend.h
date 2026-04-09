#pragma once

#include "Graphics/Core/GraphicsBackend.h"

class MetalGraphicsBackend final : public IGraphicsBackend
{
  public:
    GraphicsAPI GetAPI() const noexcept override;
    bool Initialize() override;
    void Shutdown() noexcept override;
    bool IsAvailable() const noexcept override;
    std::string DescribeAvailability() const override;
};
