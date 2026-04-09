#pragma once

#include "Graphics/Core/GraphicsTypes.h"

#include <memory>
#include <string>
#include <string_view>

struct ShaderProgramCreateInfo
{
    ShaderProgramDesc desc{};
    std::string debugName;
};

struct TextureCreateInfo
{
    TextureDesc desc{};
    std::string debugName;
};

class IGraphicsResource
{
  public:
    virtual ~IGraphicsResource() = default;

    virtual GraphicsAPI GetAPI() const noexcept = 0;
    virtual std::string_view GetDebugName() const noexcept = 0;
};

class IShaderProgramResource : public IGraphicsResource
{
  public:
    ~IShaderProgramResource() override = default;

    virtual const ShaderProgramDesc &GetDescription() const noexcept = 0;
};

class ITextureResource : public IGraphicsResource
{
  public:
    ~ITextureResource() override = default;

    virtual const TextureDesc &GetDescription() const noexcept = 0;
};
