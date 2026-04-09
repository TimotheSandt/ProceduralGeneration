#include "Graphics/Backends/OpenGL/OpenGLDiagnostics.h"

#include <glad/glad.h>

#include "Logger.h"

namespace OpenGLDiagnostics
{

void LogErrors(std::string_view context) noexcept
{
    GLenum errorCode = GL_NO_ERROR;
    while ((errorCode = glGetError()) != GL_NO_ERROR)
    {
        if (context.empty())
        {
            LOG_ERROR(static_cast<int>(errorCode), "OpenGL error");
        }
        else
        {
            LOG_ERROR(static_cast<int>(errorCode), "OpenGL error: ", context);
        }
    }
}

} // namespace OpenGLDiagnostics
