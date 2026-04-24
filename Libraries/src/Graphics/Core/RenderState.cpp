#include "Graphics/Core/RenderState.h"

#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"

#include <algorithm>

namespace GraphicsRenderState
{

namespace
{
bool IsOpenGLActive() noexcept { return IsGraphicsAPIActive(GraphicsAPI::OpenGL); }
} // namespace

void BindDefaultFramebuffer() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::BindFramebuffer(GL_FRAMEBUFFER, 0);
    }
}

void BindFramebuffer(std::uint32_t framebuffer) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
    }
}

void SetViewport(int x, int y, int width, int height) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetViewport(x, y, width, height);
    }
}

void ClearColor(const glm::vec4 &color) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::ClearColor(color);
    }
}

void ClearColorBuffer() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::Clear(GL_COLOR_BUFFER_BIT);
    }
}

void ClearBuffers(bool clearColor, bool clearDepth) noexcept
{
    if (!IsOpenGLActive())
    {
        return;
    }

    GLbitfield mask = 0;
    if (clearColor)
    {
        mask |= GL_COLOR_BUFFER_BIT;
    }
    if (clearDepth)
    {
        mask |= GL_DEPTH_BUFFER_BIT;
    }

    if (mask != 0)
    {
        OpenGLRenderState::Clear(mask);
    }
}

void ClearTransparentColorBuffer() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::ClearTransparentColorBuffer();
    }
}

void SetDepthTest(bool enabled) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetDepthTest(enabled);
    }
}

void SetWireframe(bool enabled) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetWireframe(enabled);
    }
}

void SetBlend(bool enabled) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetBlend(enabled);
    }
}

void SetAlphaBlend() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetAlphaBlend();
    }
}

void SetPremultipliedAlphaBlend() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetPremultipliedAlphaBlend();
    }
}

void SetScissorTest(bool enabled) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetScissorTest(enabled);
    }
}

void SetScissor(int x, int y, int width, int height) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetScissor(x, y, width, height);
    }
}

FramebufferState CaptureFramebufferState() noexcept
{
    if (!IsOpenGLActive())
    {
        return {};
    }

    const OpenGLRenderState::FramebufferState state = OpenGLRenderState::CaptureFramebufferState();
    FramebufferState result{};
    result.framebuffer = static_cast<std::uint32_t>(state.framebuffer);
    std::copy(std::begin(state.viewport), std::end(state.viewport), std::begin(result.viewport));
    std::copy(std::begin(state.scissorBox), std::end(state.scissorBox), std::begin(result.scissorBox));
    result.depthTest = state.depthTest == GL_TRUE;
    result.blend = state.blend == GL_TRUE;
    result.scissorTest = state.scissorTest == GL_TRUE;
    return result;
}

void RestoreFramebufferState(const FramebufferState &state) noexcept
{
    if (!IsOpenGLActive())
    {
        return;
    }

    OpenGLRenderState::FramebufferState openGLState{};
    openGLState.framebuffer = static_cast<GLint>(state.framebuffer);
    std::copy(std::begin(state.viewport), std::end(state.viewport), std::begin(openGLState.viewport));
    std::copy(std::begin(state.scissorBox), std::end(state.scissorBox), std::begin(openGLState.scissorBox));
    openGLState.depthTest = state.depthTest ? GL_TRUE : GL_FALSE;
    openGLState.blend = state.blend ? GL_TRUE : GL_FALSE;
    openGLState.scissorTest = state.scissorTest ? GL_TRUE : GL_FALSE;
    OpenGLRenderState::RestoreFramebufferState(openGLState);
}

void PrepareScreenPass(int width, int height) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::PrepareScreenPass(width, height);
    }
}

} // namespace GraphicsRenderState
