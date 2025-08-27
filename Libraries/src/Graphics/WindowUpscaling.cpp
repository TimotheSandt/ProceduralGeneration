#include "Window.h"

#include <iostream>


void Window::SetRenderScale(float scale) {
    if (scale <= 0.0f || scale > 1.0f) {
        std::cout << "Invalid render scale: " << scale << ". Must be between 0.0 and 1.0" << std::endl;
        return;
    }
    
    parameters.renderScale = scale;
    this->UpdateFBOResotution();
    
    // Enable upscaling automatically if scale is not 1.0
    if (scale != 1.0f) {
        this->EnableUpscaling(true);
    }
    
    
    std::cout << "Render scale set to " << scale << " (" << parameters.renderWidth 
              << "x" << parameters.renderHeight << ")" << std::endl;
}


void Window::EnableUpscaling(bool enable) {
    parameters.enableUpscaling = enable;
    if (!enable) {
        glViewport(0, 0, parameters.width, parameters.height);
    } else {
        this->UpdateFBOResotution();
    }
    
    std::cout << "Upscaling " << (enable ? "enabled" : "disabled") << std::endl;
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