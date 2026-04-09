#pragma once

#include <glad/glad.h>
#include <glm/vec4.hpp>

namespace OpenGLRenderState
{

struct FramebufferState
{
    GLint framebuffer = 0;
    GLint viewport[4] = {0, 0, 0, 0};
    GLboolean depthTest = GL_FALSE;
    GLboolean blend = GL_FALSE;
    GLboolean scissorTest = GL_FALSE;
};

void BindFramebuffer(GLenum target, GLuint framebuffer) noexcept;
void SetViewport(int x, int y, int width, int height) noexcept;
void ClearColor(const glm::vec4 &color) noexcept;
void Clear(GLbitfield mask) noexcept;
void ClearTransparentColorBuffer() noexcept;
void SetDepthTest(bool enabled) noexcept;
void SetBlend(bool enabled) noexcept;
void SetAlphaBlend() noexcept;
void SetScissorTest(bool enabled) noexcept;
void SetScissor(GLint x, GLint y, GLsizei width, GLsizei height) noexcept;

FramebufferState CaptureFramebufferState() noexcept;
void RestoreFramebufferState(const FramebufferState &state) noexcept;
void PrepareScreenPass(int width, int height) noexcept;

} // namespace OpenGLRenderState
