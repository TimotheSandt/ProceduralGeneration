#include "Window.h"

void Window::SetRenderScale(float scale)
{
    if (scale <= 0.0f || scale > 1.0f)
    {
        LOG_WARNING("Invalid render scale: ", scale, ". Must be between 0.0 and 1.0");
        return;
    }

    parameters.renderScale = scale;
    this->UpdateRenderTargetResolution();

    // Enable upscaling automatically if scale is not 1.0
    if (scale != 1.0f)
    {
        this->EnableUpscaling(true);
    }

    LOG_DEBUGGING("Render scale set to ", scale, " (", parameters.renderWidth, "x", parameters.renderHeight, ")");
}

void Window::EnableUpscaling(bool enable)
{
    parameters.enableUpscaling = enable;
    if (!enable)
    {
        parameters.renderWidth = parameters.width;
        parameters.renderHeight = parameters.height;
    }
    else
    {
        this->UpdateRenderTargetResolution();
    }

    LOG_DEBUGGING("Upscaling ", (enable ? "enabled" : "disabled"));
}

void Window::UpdateRenderTargetResolution()
{
    parameters.renderWidth = static_cast<int>(static_cast<float>(parameters.width) * parameters.renderScale);
    parameters.renderHeight = static_cast<int>(static_cast<float>(parameters.height) * parameters.renderScale);
}
