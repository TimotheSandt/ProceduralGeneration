#include "Window.h"



void Window::SetRenderScale(float scale) {
    if (scale <= 0.0f || scale > 1.0f) {
        LOG_WARNING("Invalid render scale: ", scale, ". Must be between 0.0 and 1.0");
        return;
    }
    
    parameters.renderScale = scale;
    this->UpdateFBOResotution();
    
    // Enable upscaling automatically if scale is not 1.0
    if (scale != 1.0f) {
        this->EnableUpscaling(true);
    }
    
    LOG_DEBUGGING("Render scale set to ", scale, " (", parameters.renderWidth, "x", parameters.renderHeight, ")");
}


void Window::EnableUpscaling(bool enable) {
    parameters.enableUpscaling = enable;
    if (!enable) {
        glViewport(0, 0, parameters.width, parameters.height);
    } else {
        this->UpdateFBOResotution();
    }
    
    LOG_DEBUGGING("Upscaling ", (enable ? "enabled" : "disabled"));
}


void Window::UpdateFBOResotution() {
    parameters.renderWidth = static_cast<int>(parameters.width * parameters.renderScale);
    parameters.renderHeight = static_cast<int>(parameters.height * parameters.renderScale);
    fboRendering.Resize(parameters.renderWidth, parameters.renderHeight);
    fboUpscaled.Resize(parameters.width, parameters.height);
}

void Window::InitFBOs() {
    fboUpscaled.Destroy();
    fboRendering.Destroy();

    fboUpscaled.Init(parameters.width, parameters.height);
    fboRendering.Init(parameters.renderWidth, parameters.renderHeight);
}

void Window::StartRenderFBO(){
    fboRendering.Bind();
}
void Window::EndRenderFBO(){
    glViewport(0, 0, parameters.width, parameters.height);

    fboRendering.Unbind();
    fboRendering.RenderScreenQuad(parameters.width, parameters.height);
    // fboRendering.BlitToScreen(parameters.width, parameters.height);

    // fboUpscaled.BlitFBO(fboRendering);
    // fboUpscaled.BlitToScreen(parameters.width, parameters.height);
    // fboUpscaled.Unbind();
}