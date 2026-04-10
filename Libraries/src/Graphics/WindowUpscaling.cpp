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
    this->UpdateUIRenderTargetResolution();

    if (scale != 1.0f)
    {
        this->EnableUIUpscaling(true);
    }

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
    parameters.enableUIUpscaling = enable;
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
    sceneRenderTarget.Resize(parameters.renderWidth, parameters.renderHeight);
    upscaledRenderTarget.Resize(parameters.width, parameters.height);
}

void Window::UpdateUIRenderTargetResolution()
{
    parameters.uiRenderWidth = static_cast<int>(static_cast<float>(parameters.width) * parameters.uiRenderScale);
    parameters.uiRenderHeight = static_cast<int>(static_cast<float>(parameters.height) * parameters.uiRenderScale);
    uiRenderTarget.Resize(parameters.uiRenderWidth, parameters.uiRenderHeight);
}

void Window::InitRenderTargets()
{
    upscaledRenderTarget.Destroy();
    sceneRenderTarget.Destroy();
    uiRenderTarget.Destroy();

    upscaledRenderTarget.Init(parameters.width, parameters.height);
    sceneRenderTarget.Init(parameters.renderWidth, parameters.renderHeight);
    uiRenderTarget.Init(parameters.uiRenderWidth, parameters.uiRenderHeight);
}

void Window::BindSceneRenderTarget() const { sceneRenderTarget.Bind(); }
void Window::BindUIRenderTarget() const { uiRenderTarget.Bind(); }
void Window::PresentSceneToScreen()
{
    if (!parameters.enableUpscaling || scenePresentedThisFrame)
    {
        return;
    }

    PresentRenderTarget();
    scenePresentedThisFrame = true;
}

void Window::PresentUIToScreen()
{
    if (!parameters.enableUIUpscaling || uiPresentedThisFrame)
    {
        return;
    }

    PresentUIRenderTarget();
    uiPresentedThisFrame = true;
}

void Window::PresentRenderTarget() const
{
    OpenGLRenderState::SetViewport(0, 0, parameters.width, parameters.height);
    OpenGLRenderState::SetScissorTest(false);
    OpenGLRenderState::SetBlend(false);

    sceneRenderTarget.Unbind();
    sceneRenderTarget.BlitToScreen(parameters.width, parameters.height);
    // upscaledRenderTarget.BlitToRenderTarget(sceneRenderTarget);
    // upscaledRenderTarget.BlitToScreen(parameters.width, parameters.height);
    // upscaledRenderTarget.Unbind();
}

void Window::PresentUIRenderTarget() const
{
    OpenGLRenderState::SetViewport(0, 0, parameters.width, parameters.height);
    OpenGLRenderState::SetScissorTest(false);
    OpenGLRenderState::SetDepthTest(false);
    OpenGLRenderState::SetBlend(true);
    OpenGLRenderState::SetAlphaBlend();

    uiRenderTarget.Unbind();
    uiRenderTarget.RenderScreenQuad(parameters.width, parameters.height);

    OpenGLRenderState::SetBlend(false);
    OpenGLRenderState::SetDepthTest(true);
}
