#pragma once

#include "Graphics/Backend/GraphicsAPI.h"

#include <memory>
#include <string>

struct GraphicsBackendCreateInfo
{
    GraphicsAPI api = GraphicsAPI::OpenGL;
};

class IGraphicsBackend
{
  public:
    virtual ~IGraphicsBackend() = default;

    virtual GraphicsAPI GetAPI() const noexcept = 0;
    virtual bool Initialize() = 0;
    virtual void Shutdown() noexcept = 0;
    virtual bool IsAvailable() const noexcept = 0;
    virtual std::string DescribeAvailability() const = 0;
};

std::unique_ptr<IGraphicsBackend> CreateGraphicsBackend(const GraphicsBackendCreateInfo &createInfo);
