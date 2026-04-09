#include "Graphics/Backends/OpenGL/OpenGLWindowContext.h"

#include <glad/glad.h>

#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Core/GraphicsDiagnostics.h"
#include "Logger.h"

namespace OpenGLWindowContext
{

void EnsureContextCurrent(GLFWwindow *window) noexcept
{
    if (!window)
    {
        return;
    }

    if (glfwGetCurrentContext() == window)
    {
        return;
    }

    glfwMakeContextCurrent(window);
    GRAPHICS_CHECK_ERRORS_M("glfwMakeContextCurrent");
}

bool Initialize(GLFWwindow *window, bool enableVsync, int width, int height)
{
    EnsureContextCurrent(window);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        LOG_FATAL(-1, "Failed to initialize GLAD");
        return false;
    }
    GRAPHICS_CHECK_ERRORS_M("gladLoadGL");

    OpenGLRenderState::SetViewport(0, 0, width, height);

    glfwSwapInterval(enableVsync ? 1 : 0);
    GRAPHICS_CHECK_ERRORS_M("glfwSwapInterval");

    OpenGLRenderState::SetDepthTest(true);

    return true;
}

void ApplyDefaultFramebufferState(GLFWwindow *window, bool enableVsync, const glm::vec4 &clearColor) noexcept
{
    if (!window)
    {
        return;
    }

    EnsureContextCurrent(window);

    glfwSwapInterval(enableVsync ? 1 : 0);
    GRAPHICS_CHECK_ERRORS_M("glfwSwapInterval");

    OpenGLRenderState::SetDepthTest(true);

    OpenGLRenderState::ClearColor(clearColor);
    OpenGLRenderState::Clear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glfwSwapBuffers(window);
    GRAPHICS_CHECK_ERRORS_M("glfwSwapBuffers");

    glfwFocusWindow(window);
    GRAPHICS_CHECK_ERRORS_M("glfwFocusWindow");
}

} // namespace OpenGLWindowContext
