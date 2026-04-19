#include "Graphics/Backends/Vulkan/VulkanDiagnostics.h"

#include "Logger.h"

namespace VulkanDiagnostics
{

void LogErrors(std::string context) noexcept
{
    if (!context.empty())
    {
        LOG_DEBUGGING("Vulkan diagnostics checkpoint: ", context);
    }
}

} // namespace VulkanDiagnostics
