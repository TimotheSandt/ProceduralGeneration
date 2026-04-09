#pragma once

#include "Graphics/Core/GraphicsResources.h"

class OpenGLShaderProgramResource final : public IShaderProgramResource
{
  public:
    explicit OpenGLShaderProgramResource(ShaderProgramCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const ShaderProgramDesc &GetDescription() const noexcept override;

  private:
    ShaderProgramDesc desc;
    std::string debugName;
};

class OpenGLTextureResource final : public ITextureResource
{
  public:
    explicit OpenGLTextureResource(TextureCreateInfo createInfo);

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const TextureDesc &GetDescription() const noexcept override;

  private:
    TextureDesc desc;
    std::string debugName;
};
