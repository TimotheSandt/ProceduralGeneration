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
    void Bind() const override;
    void Unbind() const override;
    int GetUniformLocation(std::string_view name) const override;
    void SetFloatUniform(int location, const float *data, std::size_t componentCount) const override;
    void SetIntUniform(int location, const int *data, std::size_t componentCount) const override;
    void SetMatrix4Uniform(int location, const float *data) const override;
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
    void Bind() const override;
    void BindToBindingPoint(std::uint32_t bindingPoint) const override;
    void Unbind() const override;
    void UploadData(const void *data, std::size_t size, std::size_t offset) override;
    void Resize(std::size_t newSize, bool preserveData) override;
    void *Map(BufferMapAccess access) override;
    void Unmap() override;
    GLuint GetBufferID() const noexcept;

  private:
    GLenum UsageHint() const noexcept;
    void Recreate(std::size_t newSize, bool preserveData);
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
    void DrawIndexed() const override;
    void DrawIndexedInstanced() const override;

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
    void Bind(std::uint32_t slot) const override;
    void Unbind() const override;
    void Readback(std::vector<std::byte> &output) const override;
    void Resize(std::uint32_t width, std::uint32_t height) override;
    void AttachToFramebuffer(std::uint32_t framebufferHandle) const override;
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
