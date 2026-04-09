#include "Graphics/Backends/OpenGL/OpenGLWindowContext.h"

#include <glad/glad.h>

#include "Logger.h"

namespace OpenGLWindowContext
{

bool Initialize(GLFWwindow *window, bool enableVsync, int width, int height)
{
    glfwMakeContextCurrent(window);

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        LOG_FATAL(-1, "Failed to initialize GLAD");
        return false;
    }
    GL_CHECK_ERROR_M("gladLoadGL");

    glViewport(0, 0, width, height);
    GL_CHECK_ERROR_M("glViewport");

    glfwSwapInterval(enableVsync ? 1 : 0);
    GL_CHECK_ERROR_M("glfwSwapInterval");

    glEnable(GL_DEPTH_TEST);
    GL_CHECK_ERROR_M("glEnable");

    return true;
}

void ApplyDefaultFramebufferState(GLFWwindow *window, bool enableVsync, const glm::vec4 &clearColor) noexcept
{
    if (!window)
    {
        return;
    }

    glfwMakeContextCurrent(window);
    GL_CHECK_ERROR_M("glfwMakeContextCurrent");

    glfwSwapInterval(enableVsync ? 1 : 0);
    GL_CHECK_ERROR_M("glfwSwapInterval");

    glEnable(GL_DEPTH_TEST);
    GL_CHECK_ERROR_M("glEnable");

    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    GL_CHECK_ERROR_M("glClear");

    glfwSwapBuffers(window);
    GL_CHECK_ERROR_M("glfwSwapBuffers");

    glfwFocusWindow(window);
    GL_CHECK_ERROR_M("glfwFocusWindow");
}

} // namespace OpenGLWindowContext
