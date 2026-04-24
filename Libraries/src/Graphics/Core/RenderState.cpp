#include "Graphics/Core/RenderState.h"

#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Backends/Vulkan/VulkanRenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"

namespace GraphicsRenderState
{

namespace
{
bool IsOpenGLActive() noexcept { return IsGraphicsAPIActive(GraphicsAPI::OpenGL); }
bool IsVulkanActive() noexcept { return IsGraphicsAPIActive(GraphicsAPI::Vulkan); }
} // namespace

void BindDefaultFramebuffer() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::BindFramebuffer(GL_FRAMEBUFFER, 0);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::BindFramebuffer(0);
    }
}

void BindFramebuffer(std::uint32_t framebuffer) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::BindFramebuffer(GL_FRAMEBUFFER, framebuffer);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::BindFramebuffer(framebuffer);
    }
}

void SetViewport(int x, int y, int width, int height) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetViewport(x, y, width, height);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::SetViewport(x, y, width, height);
    }
}

void ClearColor(const glm::vec4 &color) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::ClearColor(color);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::ClearColor(color);
    }
}

void ClearColorBuffer() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::Clear(GL_COLOR_BUFFER_BIT);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::ClearColorBuffer();
    }
}

void ClearBuffers(bool clearColor, bool clearDepth) noexcept
{
    if (IsOpenGLActive())
    {
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
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::ClearBuffers(clearColor, clearDepth);
    }
}

void ClearTransparentColorBuffer() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::ClearTransparentColorBuffer();
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::ClearTransparentColorBuffer();
    }
}

void SetDepthTest(bool enabled) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetDepthTest(enabled);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::SetDepthTest(enabled);
    }
}

void SetWireframe(bool enabled) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetWireframe(enabled);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::SetWireframe(enabled);
    }
}

void SetBlend(bool enabled) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetBlend(enabled);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::SetBlend(enabled);
    }
}

void SetAlphaBlend() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetAlphaBlend();
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::SetAlphaBlend();
    }
}

void SetPremultipliedAlphaBlend() noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetPremultipliedAlphaBlend();
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::SetPremultipliedAlphaBlend();
    }
}

void SetScissorTest(bool enabled) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetScissorTest(enabled);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::SetScissorTest(enabled);
    }
}

void SetScissor(int x, int y, int width, int height) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::SetScissor(x, y, width, height);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::SetScissor(x, y, width, height);
    }
}

FramebufferState CaptureFramebufferState() noexcept
{
    if (IsOpenGLActive())
    {
        const OpenGLRenderState::FramebufferState state = OpenGLRenderState::CaptureFramebufferState();
        FramebufferState result{};
        result.framebuffer = static_cast<std::uint32_t>(state.framebuffer);
        for (int i = 0; i < 4; ++i)
        {
            result.viewport[i] = state.viewport[i];
        }
        result.depthTest = state.depthTest == GL_TRUE;
        result.blend = state.blend == GL_TRUE;
        result.scissorTest = state.scissorTest == GL_TRUE;
        return result;
    }

    if (IsVulkanActive())
    {
        const VulkanRenderState::FramebufferState state = VulkanRenderState::CaptureFramebufferState();
        FramebufferState result{};
        result.framebuffer = state.framebuffer;
        for (int i = 0; i < 4; ++i)
        {
            result.viewport[i] = state.viewport[i];
        }
        result.depthTest = state.depthTest;
        result.blend = state.blend;
        result.scissorTest = state.scissorTest;
        return result;
    }

    return {};
}

void RestoreFramebufferState(const FramebufferState &state) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::FramebufferState openGLState{};
        openGLState.framebuffer = static_cast<GLint>(state.framebuffer);
        for (int i = 0; i < 4; ++i)
        {
            openGLState.viewport[i] = state.viewport[i];
        }
        openGLState.depthTest = state.depthTest ? GL_TRUE : GL_FALSE;
        openGLState.blend = state.blend ? GL_TRUE : GL_FALSE;
        openGLState.scissorTest = state.scissorTest ? GL_TRUE : GL_FALSE;
        OpenGLRenderState::RestoreFramebufferState(openGLState);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::FramebufferState vulkanState{};
        vulkanState.framebuffer = state.framebuffer;
        for (int i = 0; i < 4; ++i)
        {
            vulkanState.viewport[i] = state.viewport[i];
        }
        vulkanState.depthTest = state.depthTest;
        vulkanState.blend = state.blend;
        vulkanState.scissorTest = state.scissorTest;
        VulkanRenderState::RestoreFramebufferState(vulkanState);
    }
}

void PrepareScreenPass(int width, int height) noexcept
{
    if (IsOpenGLActive())
    {
        OpenGLRenderState::PrepareScreenPass(width, height);
        return;
    }

    if (IsVulkanActive())
    {
        VulkanRenderState::PrepareScreenPass(width, height);
    }
}

} // namespace GraphicsRenderState
