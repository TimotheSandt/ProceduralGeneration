#pragma once

#include <glad/glad.h>

#include "Graphics/Core/GraphicsResources.h"

class OpenGLShaderProgramResource final : public IShaderProgramResource
{
  public:
    explicit OpenGLShaderProgramResource(ShaderProgramCreateInfo createInfo);
    ~OpenGLShaderProgramResource() override;

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const ShaderProgramDesc &GetDescription() const noexcept override;
    GLuint GetProgramID() const noexcept;

  private:
    GLuint programID = 0;
    ShaderProgramDesc desc;
    std::string debugName;
};

class OpenGLTextureResource final : public ITextureResource
{
  public:
    explicit OpenGLTextureResource(TextureCreateInfo createInfo);
    ~OpenGLTextureResource() override;

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const TextureDesc &GetDescription() const noexcept override;
    GLuint GetTextureID() const noexcept;

  private:
    GLuint textureID = 0;
    TextureDesc desc;
    std::string debugName;
};
