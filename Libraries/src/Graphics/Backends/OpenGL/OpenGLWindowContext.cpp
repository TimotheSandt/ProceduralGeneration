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
}

bool Initialize(GLFWwindow *window, bool enableVsync, int width, int height)
{
#ifdef DEBUG
    LOG_DEBUGGING("OpenGLWindowContext::Initialize step 1: ensure context current");
#endif
    EnsureContextCurrent(window);

#ifdef DEBUG
    LOG_DEBUGGING("OpenGLWindowContext::Initialize step 2: load GLAD");
#endif
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
    {
        LOG_FATAL(-1, "Failed to initialize GLAD");
        return false;
    }
    GRAPHICS_CHECK_ERRORS_M("gladLoadGL");

#ifdef DEBUG
    LOG_DEBUGGING("OpenGLWindowContext::Initialize step 3: set viewport");
#endif
    OpenGLRenderState::SetViewport(0, 0, width, height);

#ifdef DEBUG
    LOG_DEBUGGING("OpenGLWindowContext::Initialize step 4: set swap interval");
#endif
    glfwSwapInterval(enableVsync ? 1 : 0);
    GRAPHICS_CHECK_ERRORS_M("glfwSwapInterval");

#ifdef DEBUG
    LOG_DEBUGGING("OpenGLWindowContext::Initialize step 5: enable depth test");
#endif
    OpenGLRenderState::SetDepthTest(true);

#ifdef DEBUG
    LOG_DEBUGGING("OpenGLWindowContext::Initialize step 6: complete");
#endif

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

    int framebufferWidth = 0;
    int framebufferHeight = 0;
    glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
    GRAPHICS_CHECK_ERRORS_M("glfwGetFramebufferSize");

    if (framebufferWidth > 0 && framebufferHeight > 0)
    {
        OpenGLRenderState::SetViewport(0, 0, framebufferWidth, framebufferHeight);
    }

    OpenGLRenderState::SetDepthTest(true);
    OpenGLRenderState::ClearColor(clearColor);
}

} // namespace OpenGLWindowContext
