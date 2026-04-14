#pragma once

#include <cstdint>

#include <glm/vec4.hpp>

namespace GraphicsRenderState
{

struct FramebufferState
{
    std::uint32_t framebuffer = 0;
    int viewport[4] = {0, 0, 0, 0};
    bool depthTest = false;
    bool blend = false;
    bool scissorTest = false;
};

void BindDefaultFramebuffer() noexcept;
void BindFramebuffer(std::uint32_t framebuffer) noexcept;
void SetViewport(int x, int y, int width, int height) noexcept;
void ClearColor(const glm::vec4 &color) noexcept;
void ClearColorBuffer() noexcept;
void ClearBuffers(bool clearColor, bool clearDepth) noexcept;
void ClearTransparentColorBuffer() noexcept;
void SetDepthTest(bool enabled) noexcept;
void SetWireframe(bool enabled) noexcept;
void SetBlend(bool enabled) noexcept;
void SetAlphaBlend() noexcept;
void SetPremultipliedAlphaBlend() noexcept;
void SetScissorTest(bool enabled) noexcept;
void SetScissor(int x, int y, int width, int height) noexcept;
FramebufferState CaptureFramebufferState() noexcept;
void RestoreFramebufferState(const FramebufferState &state) noexcept;
void PrepareScreenPass(int width, int height) noexcept;

} // namespace GraphicsRenderState
