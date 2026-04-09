#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"

#include "Logger.h"

namespace OpenGLRenderState
{

void BindFramebuffer(GLenum target, GLuint framebuffer) noexcept
{
    glBindFramebuffer(target, framebuffer);
    GL_CHECK_ERROR_M("glBindFramebuffer");
}

void SetViewport(int x, int y, int width, int height) noexcept
{
    glViewport(x, y, width, height);
    GL_CHECK_ERROR_M("glViewport");
}

void ClearColor(const glm::vec4 &color) noexcept
{
    glClearColor(color.r, color.g, color.b, color.a);
    GL_CHECK_ERROR_M("glClearColor");
}

void Clear(GLbitfield mask) noexcept
{
    glClear(mask);
    GL_CHECK_ERROR_M("glClear");
}

void ClearTransparentColorBuffer() noexcept
{
    ClearColor(glm::vec4(0.0f));
    Clear(GL_COLOR_BUFFER_BIT);
}

void SetDepthTest(bool enabled) noexcept
{
    if (enabled)
    {
        glEnable(GL_DEPTH_TEST);
        GL_CHECK_ERROR_M("glEnable(GL_DEPTH_TEST)");
        return;
    }

    glDisable(GL_DEPTH_TEST);
    GL_CHECK_ERROR_M("glDisable(GL_DEPTH_TEST)");
}

void SetBlend(bool enabled) noexcept
{
    if (enabled)
    {
        glEnable(GL_BLEND);
        GL_CHECK_ERROR_M("glEnable(GL_BLEND)");
        return;
    }

    glDisable(GL_BLEND);
    GL_CHECK_ERROR_M("glDisable(GL_BLEND)");
}

void SetAlphaBlend() noexcept
{
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    GL_CHECK_ERROR_M("glBlendFunc");
}

void SetScissorTest(bool enabled) noexcept
{
    if (enabled)
    {
        glEnable(GL_SCISSOR_TEST);
        GL_CHECK_ERROR_M("glEnable(GL_SCISSOR_TEST)");
        return;
    }

    glDisable(GL_SCISSOR_TEST);
    GL_CHECK_ERROR_M("glDisable(GL_SCISSOR_TEST)");
}

void SetScissor(GLint x, GLint y, GLsizei width, GLsizei height) noexcept
{
    glScissor(x, y, width, height);
    GL_CHECK_ERROR_M("glScissor");
}

FramebufferState CaptureFramebufferState() noexcept
{
    FramebufferState state;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &state.framebuffer);
    glGetIntegerv(GL_VIEWPORT, state.viewport);
    state.depthTest = glIsEnabled(GL_DEPTH_TEST);
    state.blend = glIsEnabled(GL_BLEND);
    state.scissorTest = glIsEnabled(GL_SCISSOR_TEST);
    GL_CHECK_ERROR_M("CaptureFramebufferState");
    return state;
}

void RestoreFramebufferState(const FramebufferState &state) noexcept
{
    BindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(state.framebuffer));
    SetViewport(state.viewport[0], state.viewport[1], state.viewport[2], state.viewport[3]);
    SetDepthTest(state.depthTest == GL_TRUE);
    SetBlend(state.blend == GL_TRUE);
    SetScissorTest(state.scissorTest == GL_TRUE);
}

void PrepareScreenPass(int width, int height) noexcept
{
    BindFramebuffer(GL_FRAMEBUFFER, 0);
    SetViewport(0, 0, width, height);
}

} // namespace OpenGLRenderState
