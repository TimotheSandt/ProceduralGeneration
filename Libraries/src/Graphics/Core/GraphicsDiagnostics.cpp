#include "Graphics/Core/GraphicsDiagnostics.h"

#include "Graphics/Backends/OpenGL/OpenGLDiagnostics.h"
#include "Graphics/Core/GraphicsRuntime.h"

void LogActiveGraphicsBackendErrors(std::string_view context) noexcept
{
    if (!IsGraphicsAPIActive(GraphicsAPI::OpenGL))
    {
        return;
    }

    OpenGLDiagnostics::LogErrors(context);
}
