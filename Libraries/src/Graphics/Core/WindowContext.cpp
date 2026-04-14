#include "Graphics/Core/WindowContext.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "Graphics/Backends/OpenGL/OpenGLWindowContext.h"
#include "Graphics/Backends/Vulkan/VulkanWindowContext.h"
#include "Graphics/Core/GraphicsDiagnostics.h"
#include "Graphics/Core/GraphicsRuntime.h"

namespace GraphicsWindowContext
{

bool Initialize(GLFWwindow *window, bool enableVsync, int width, int height)
{
    switch (GetActiveGraphicsAPI())
    {
        case GraphicsAPI::OpenGL:
            return OpenGLWindowContext::Initialize(window, enableVsync, width, height);
        case GraphicsAPI::Vulkan:
            return VulkanWindowContext::Initialize(window, enableVsync, width, height);
        case GraphicsAPI::Metal:
            return false;
    }

    return false;
}

void EnsureContextReady(GLFWwindow *window) noexcept
{
    if (window == nullptr)
    {
        return;
    }

    switch (GetActiveGraphicsAPI())
    {
        case GraphicsAPI::OpenGL:
            OpenGLWindowContext::EnsureContextCurrent(window);
            return;
        case GraphicsAPI::Vulkan:
        case GraphicsAPI::Metal:
            return;
    }
}

void ApplyDefaultFramebufferState(GLFWwindow *window, bool enableVsync, const glm::vec4 &clearColor) noexcept
{
    if (window == nullptr)
    {
        return;
    }

    switch (GetActiveGraphicsAPI())
    {
        case GraphicsAPI::OpenGL:
            OpenGLWindowContext::ApplyDefaultFramebufferState(window, enableVsync, clearColor);
            return;
        case GraphicsAPI::Vulkan:
            static_cast<void>(clearColor);
            VulkanWindowContext::ApplyDefaultFramebufferState(window, enableVsync);
            return;
        case GraphicsAPI::Metal:
            return;
    }
}

void Present(GLFWwindow *window) noexcept
{
    if (window == nullptr)
    {
        return;
    }

    switch (GetActiveGraphicsAPI())
    {
        case GraphicsAPI::OpenGL:
            glfwSwapBuffers(window);
            GRAPHICS_CHECK_ERRORS_M("glfwSwapBuffers");
            return;
        case GraphicsAPI::Vulkan:
            VulkanWindowContext::Present(window);
            return;
        case GraphicsAPI::Metal:
            return;
    }
}

void Shutdown(GLFWwindow *window) noexcept
{
    if (window == nullptr)
    {
        return;
    }

    switch (GetActiveGraphicsAPI())
    {
        case GraphicsAPI::OpenGL:
            return;
        case GraphicsAPI::Vulkan:
            VulkanWindowContext::Shutdown(window);
            return;
        case GraphicsAPI::Metal:
            return;
    }
}

} // namespace GraphicsWindowContext
