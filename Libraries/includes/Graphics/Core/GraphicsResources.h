#pragma once

#include "Graphics/Core/GraphicsTypes.h"

#include <cstddef>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

struct ShaderStageSource
{
    ShaderStage stage = ShaderStage::Vertex;
    std::string sourceCode;
    std::string entryPoint = "main";
};

struct ShaderProgramCreateInfo
{
    ShaderProgramDesc desc{};
    std::string debugName;
    std::vector<ShaderStageSource> stageSources;
};

struct TextureCreateInfo
{
    TextureDesc desc{};
    std::string debugName;
    std::vector<std::byte> initialData;
    bool generateMipmaps = true;
};

struct AccelerationStructureCreateInfo
{
    AccelerationStructureDesc desc{};
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

class IAccelerationStructureResource : public IGraphicsResource
{
  public:
    ~IAccelerationStructureResource() override = default;

    virtual const AccelerationStructureDesc &GetDescription() const noexcept = 0;
};
