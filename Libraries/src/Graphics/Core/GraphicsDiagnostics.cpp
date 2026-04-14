#include "Graphics/Core/GraphicsDiagnostics.h"

#include "Graphics/Backends/OpenGL/OpenGLDiagnostics.h"
#include "Graphics/Backends/Vulkan/VulkanDiagnostics.h"
#include "Graphics/Core/GraphicsRuntime.h"

void LogActiveGraphicsBackendErrors(std::string_view context) noexcept
{
    if (IsGraphicsAPIActive(GraphicsAPI::OpenGL))
    {
        OpenGLDiagnostics::LogErrors(context);
        return;
    }

    if (IsGraphicsAPIActive(GraphicsAPI::Vulkan))
    {
        VulkanDiagnostics::LogErrors(context);
    }
}
