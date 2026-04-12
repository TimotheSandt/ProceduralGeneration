#include "Renderer.h"

void Renderer::RegisterUpscaleMode(std::unique_ptr<IUpscaleMode> mode) { presentationController.RegisterMode(std::move(mode)); }

bool Renderer::SetActiveUpscaleMode(std::string_view mode) { return presentationController.SetActiveMode(mode, *this); }

std::string_view Renderer::GetActiveUpscaleMode() const noexcept { return presentationController.GetActiveMode(); }

std::vector<std::string_view> Renderer::GetRegisteredUpscaleModes() const { return presentationController.GetRegisteredModes(); }

void Renderer::BeginUpscalePass(int width, int height) { presentationController.Begin(*this, width, height); }

void Renderer::EndUpscalePass() const { presentationController.End(*this); }

UpscalePassContext Renderer::GetUpscalePassContext()
{
    if (!UsesUpscaleRenderTarget())
    {
        return {};
    }

    return {
        .renderTarget = &GetUpscaleRenderTarget(),
        .sourceWidth = GetFrameWidth(),
        .sourceHeight = GetFrameHeight(),
        .outputWidth = GetUpscaleOutputWidth(),
        .outputHeight = GetUpscaleOutputHeight(),
    };
}

ConstUpscalePassContext Renderer::GetUpscalePassContext() const
{
    if (!UsesUpscaleRenderTarget())
    {
        return {};
    }

    return {
        .renderTarget = &GetUpscaleRenderTarget(),
        .sourceWidth = GetFrameWidth(),
        .sourceHeight = GetFrameHeight(),
        .outputWidth = GetUpscaleOutputWidth(),
        .outputHeight = GetUpscaleOutputHeight(),
    };
}
