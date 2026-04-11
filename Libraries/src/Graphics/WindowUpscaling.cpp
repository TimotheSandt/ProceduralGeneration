#include "Window.h"

#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"

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

void Window::SetUIRenderScale(float scale)
{
    if (scale <= 0.0f || scale > 1.0f)
    {
        LOG_WARNING("Invalid UI render scale: ", scale, ". Must be between 0.0 and 1.0");
        return;
    }

    parameters.uiRenderScale = scale;
    parameters.enableUIUpscaling = scale != 1.0f;
    this->UpdateUIRenderTargetResolution();

    LOG_DEBUGGING("UI render scale set to ", scale, " (", parameters.uiRenderWidth, "x", parameters.uiRenderHeight, ")");
}

void Window::EnableUpscaling(bool enable)
{
    parameters.enableUpscaling = enable;
    if (!enable)
    {
        parameters.renderWidth = parameters.width;
        parameters.renderHeight = parameters.height;
        OpenGLRenderState::SetViewport(0, 0, parameters.width, parameters.height);
    }
    else
    {
        this->UpdateRenderTargetResolution();
    }

    LOG_DEBUGGING("Upscaling ", (enable ? "enabled" : "disabled"));
}

void Window::EnableUIUpscaling(bool enable)
{
    parameters.enableUIUpscaling = enable && parameters.uiRenderScale != 1.0f;
    if (!enable)
    {
        parameters.uiRenderWidth = parameters.width;
        parameters.uiRenderHeight = parameters.height;
        OpenGLRenderState::SetViewport(0, 0, parameters.width, parameters.height);
    }
    else
    {
        this->UpdateUIRenderTargetResolution();
    }

    LOG_DEBUGGING("UI upscaling ", (enable ? "enabled" : "disabled"));
}

void Window::UpdateRenderTargetResolution()
{
    parameters.renderWidth = static_cast<int>(static_cast<float>(parameters.width) * parameters.renderScale);
    parameters.renderHeight = static_cast<int>(static_cast<float>(parameters.height) * parameters.renderScale);
}

void Window::UpdateUIRenderTargetResolution()
{
    parameters.uiRenderWidth = static_cast<int>(static_cast<float>(parameters.width) * parameters.uiRenderScale);
    parameters.uiRenderHeight = static_cast<int>(static_cast<float>(parameters.height) * parameters.uiRenderScale);
    uiRenderTarget.Resize(parameters.uiRenderWidth, parameters.uiRenderHeight);
}

void Window::InitRenderTargets()
{
    uiRenderTarget.Destroy();
    uiRenderTarget.Init(parameters.uiRenderWidth, parameters.uiRenderHeight);
}

void Window::BindUIRenderTarget() const { uiRenderTarget.Bind(); }

void Window::PresentUIToScreen()
{
    if (!parameters.enableUIUpscaling || uiPresentedThisFrame)
    {
        return;
    }

    PresentUIRenderTarget();
    uiPresentedThisFrame = true;
}

void Window::PresentUIRenderTarget() const
{
    OpenGLRenderState::SetViewport(0, 0, parameters.width, parameters.height);
    OpenGLRenderState::SetScissorTest(false);
    OpenGLRenderState::SetDepthTest(false);
    OpenGLRenderState::SetBlend(true);
    OpenGLRenderState::SetPremultipliedAlphaBlend();

    uiRenderTarget.Unbind();
    uiRenderTarget.RenderScreenQuad(parameters.width, parameters.height);

    OpenGLRenderState::SetBlend(false);
    OpenGLRenderState::SetDepthTest(true);
}
