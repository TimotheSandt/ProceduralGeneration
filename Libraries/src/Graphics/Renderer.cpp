#include "Renderer.h"

bool Renderer::SetActiveUpscaleMode(std::string_view mode)
{
    if (mode == "disabled")
    {
        activeUpscaleMode = "disabled";
        return true;
    }

    if (!SupportsUpscaleMode(mode))
    {
        return false;
    }

    activeUpscaleMode = std::string(mode);
    return true;
}
