#pragma once

#include <string>

class RenderTarget;

class IPostProcessPass
{
  public:
    virtual ~IPostProcessPass() = default;

    virtual std::string GetName() const noexcept = 0;
    virtual void Process(RenderTarget &target, int width, int height) const = 0;
};
