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

class OpenGLBufferResource final : public IBufferResource
{
  public:
    explicit OpenGLBufferResource(BufferCreateInfo createInfo);
    ~OpenGLBufferResource() override;

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const BufferDesc &GetDescription() const noexcept override;
    GLuint GetBufferID() const noexcept;

  private:
    GLuint bufferID = 0;
    GLenum target = GL_ARRAY_BUFFER;
    BufferDesc desc;
    std::string debugName;
};

class OpenGLGeometryResource final : public IGeometryResource
{
  public:
    explicit OpenGLGeometryResource(GeometryCreateInfo createInfo);
    ~OpenGLGeometryResource() override;

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const GeometryLayout &GetLayout() const noexcept override;
    std::size_t GetIndexCount() const noexcept override;
    std::size_t GetInstanceCount() const noexcept override;
    void Bind() const override;
    void Unbind() const override;

  private:
    GLuint vertexArrayID = 0;
    GLuint vertexBufferID = 0;
    GLuint indexBufferID = 0;
    GLuint instanceBufferID = 0;
    GeometryLayout layout;
    std::size_t indexCount = 0;
    std::size_t instanceCount = 0;
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

class OpenGLRenderTargetResource final : public IRenderTargetResource
{
  public:
    explicit OpenGLRenderTargetResource(RenderTargetCreateInfo createInfo);
    ~OpenGLRenderTargetResource() override;

    GraphicsAPI GetAPI() const noexcept override;
    std::string_view GetDebugName() const noexcept override;
    const RenderTargetDesc &GetDescription() const noexcept override;
    GLuint GetFramebufferID() const noexcept;
    GLuint GetDepthBufferID() const noexcept;

  private:
    GLuint framebufferID = 0;
    GLuint depthBufferID = 0;
    RenderTargetDesc desc;
    std::string debugName;
};
