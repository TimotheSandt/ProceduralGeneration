#pragma once

struct GLFWwindow;

namespace VulkanWindowContext
{

bool Initialize(GLFWwindow *window, bool enableVsync, int width, int height);
void ApplyDefaultFramebufferState(GLFWwindow *window, bool enableVsync) noexcept;
void Present(GLFWwindow *window) noexcept;
void Shutdown(GLFWwindow *window) noexcept;

} // namespace VulkanWindowContext
