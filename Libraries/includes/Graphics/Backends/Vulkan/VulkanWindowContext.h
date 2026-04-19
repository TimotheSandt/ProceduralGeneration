#pragma once

#include <glm/vec4.hpp>
#include <vulkan/vulkan.h>

struct GLFWwindow;

namespace VulkanWindowContext
{

bool Initialize(GLFWwindow *window, bool enableVsync, int width, int height);
void ApplyDefaultFramebufferState(GLFWwindow *window, bool enableVsync, const glm::vec4 &clearColor) noexcept;
void Present(GLFWwindow *window) noexcept;
void Shutdown(GLFWwindow *window) noexcept;

VkRenderPass GetSwapchainRenderPass(GLFWwindow *window) noexcept;
VkRenderPass GetAnySwapchainRenderPass() noexcept;

} // namespace VulkanWindowContext
