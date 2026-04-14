#pragma once

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/vec4.hpp>

namespace OpenGLWindowContext
{

bool Initialize(GLFWwindow *window, bool enableVsync, int width, int height);
void EnsureContextCurrent(GLFWwindow *window) noexcept;
void ApplyDefaultFramebufferState(GLFWwindow *window, bool enableVsync, const glm::vec4 &clearColor) noexcept;

} // namespace OpenGLWindowContext
