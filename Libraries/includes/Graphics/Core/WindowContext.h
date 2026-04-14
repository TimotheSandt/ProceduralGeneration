#pragma once

#include <glm/vec4.hpp>

struct GLFWwindow;

namespace GraphicsWindowContext
{

bool Initialize(GLFWwindow *window, bool enableVsync, int width, int height);
void EnsureContextReady(GLFWwindow *window) noexcept;
void ApplyDefaultFramebufferState(GLFWwindow *window, bool enableVsync, const glm::vec4 &clearColor) noexcept;
void Present(GLFWwindow *window) noexcept;

} // namespace GraphicsWindowContext
