#include "Graphics/Backends/OpenGL/OpenGLGraphicsBackend.h"

#include <GLFW/glfw3.h>

#ifdef _WIN32
#include <windows.h>
#endif

#include "Graphics/Backends/OpenGL/OpenGLGraphicsDevice.h"
#include "Logger.h"

GraphicsAPI OpenGLGraphicsBackend::GetAPI() const noexcept { return GraphicsAPI::OpenGL; }

bool OpenGLGraphicsBackend::Initialize()
{
    if (initialized)
    {
        return true;
    }

    if (!glfwInit())
    {
        LOG_FATAL(-1, "Failed to initialize GLFW");
        return false;
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, majorVersion);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, minorVersion);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_RESIZABLE, GL_TRUE);
    glfwWindowHint(GLFW_CENTER_CURSOR, GLFW_TRUE);
    glfwWindowHint(GLFW_DOUBLEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_VISIBLE, GLFW_TRUE);
    glfwWindowHint(GLFW_FOCUSED, GLFW_TRUE);
    glfwWindowHint(GLFW_AUTO_ICONIFY, GLFW_FALSE);
    glfwWindowHint(GLFW_FOCUS_ON_SHOW, GLFW_TRUE);

#ifdef DEBUG
    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#endif

    SetupErrorHandling();
    initialized = true;
    return true;
}

void OpenGLGraphicsBackend::Shutdown() noexcept
{
    if (!initialized)
    {
        return;
    }

    glfwSetErrorCallback(nullptr);
    glfwTerminate();
    initialized = false;
}

bool OpenGLGraphicsBackend::IsAvailable() const noexcept { return true; }

std::string OpenGLGraphicsBackend::DescribeAvailability() const { return "OpenGL backend is available."; }

const GraphicsCapabilities &OpenGLGraphicsBackend::GetCapabilities() const noexcept { return capabilities; }

std::unique_ptr<IGraphicsDevice> OpenGLGraphicsBackend::CreateDevice(const GraphicsDeviceCreateInfo &createInfo) const
{
    static_cast<void>(createInfo);
    return std::make_unique<OpenGLGraphicsDevice>(capabilities);
}

void OpenGLGraphicsBackend::SetupErrorHandling() const
{
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOALIGNMENTFAULTEXCEPT | SEM_NOOPENFILEERRORBOX);
#endif

    glfwSetErrorCallback(
        [](int errorCode, const char *description)
        {
            LOG_ERROR(errorCode, "GLFW Error: ", description);

            switch (errorCode)
            {
                case GLFW_INVALID_ENUM:
                    LOG_ERROR(errorCode, "Invalid enum parameter");
                    break;
                case GLFW_INVALID_VALUE:
                    LOG_ERROR(errorCode, "Invalid value parameter");
                    break;
                case GLFW_OUT_OF_MEMORY:
                    LOG_ERROR(errorCode, "Out of memory");
                    break;
                case GLFW_API_UNAVAILABLE:
                    LOG_ERROR(errorCode, "API unavailable");
                    break;
                case GLFW_VERSION_UNAVAILABLE:
                    LOG_ERROR(errorCode, "Version unavailable");
                    break;
                case GLFW_PLATFORM_ERROR:
                    LOG_ERROR(errorCode, "Platform error");
                    break;
                case GLFW_FORMAT_UNAVAILABLE:
                    LOG_ERROR(errorCode, "Format unavailable");
                    break;
            }
        });
}
