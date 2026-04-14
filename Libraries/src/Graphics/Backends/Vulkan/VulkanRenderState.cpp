#include "Graphics/Backends/Vulkan/VulkanRenderState.h"

namespace VulkanRenderState
{

namespace
{

FramebufferState state{};
glm::vec4 clearColor = glm::vec4(0.0f);
bool wireframe = false;
int scissorRect[4] = {0, 0, 0, 0};

} // namespace

void BindFramebuffer(unsigned int framebuffer) noexcept { state.framebuffer = framebuffer; }

void SetViewport(int x, int y, int width, int height) noexcept
{
    state.viewport[0] = x;
    state.viewport[1] = y;
    state.viewport[2] = width;
    state.viewport[3] = height;
}

void ClearColor(const glm::vec4 &color) noexcept { clearColor = color; }

void ClearColorBuffer() noexcept {}

void ClearBuffers(bool clearColorBuffer, bool clearDepth) noexcept
{
    static_cast<void>(clearColorBuffer);
    static_cast<void>(clearDepth);
}

void ClearTransparentColorBuffer() noexcept { clearColor = glm::vec4(0.0f); }

void SetDepthTest(bool enabled) noexcept { state.depthTest = enabled; }

void SetWireframe(bool enabled) noexcept { wireframe = enabled; }

void SetBlend(bool enabled) noexcept { state.blend = enabled; }

void SetAlphaBlend() noexcept {}

void SetPremultipliedAlphaBlend() noexcept {}

void SetScissorTest(bool enabled) noexcept { state.scissorTest = enabled; }

void SetScissor(int x, int y, int width, int height) noexcept
{
    scissorRect[0] = x;
    scissorRect[1] = y;
    scissorRect[2] = width;
    scissorRect[3] = height;
}

FramebufferState CaptureFramebufferState() noexcept { return state; }

void RestoreFramebufferState(const FramebufferState &captured) noexcept
{
    state = captured;
}

void PrepareScreenPass(int width, int height) noexcept
{
    static_cast<void>(wireframe);
    static_cast<void>(clearColor);
    static_cast<void>(scissorRect);
    SetViewport(0, 0, width, height);
}

} // namespace VulkanRenderState
