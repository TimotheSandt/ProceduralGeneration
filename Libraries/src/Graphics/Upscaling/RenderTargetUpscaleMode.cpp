#include "Graphics/Upscaling/RenderTargetUpscaleMode.h"

#include "Graphics/RenderTarget.h"
#include "Graphics/Renderer.h"

RenderTargetUpscaleMode::RenderTargetUpscaleMode(std::string_view upscaleModeName) noexcept : name(upscaleModeName) {}

std::string_view RenderTargetUpscaleMode::GetName() const noexcept { return name; }

bool RenderTargetUpscaleMode::SupportsRenderer(const Renderer &) const noexcept { return true; }

int RenderTargetUpscaleMode::GetOutputWidth(const Renderer &renderer) const noexcept { return renderer.GetUpscaleOutputWidth(); }

int RenderTargetUpscaleMode::GetOutputHeight(const Renderer &renderer) const noexcept { return renderer.GetUpscaleOutputHeight(); }

void RenderTargetUpscaleMode::BeginPass(Renderer &renderer, int, int) const
{
    if (!renderer.UsesUpscaleRenderTarget())
    {
        return;
    }

    RenderTarget &renderTarget = renderer.GetUpscaleRenderTarget();
    if (renderTarget.GetID() == 0)
    {
        renderTarget.Init(renderer.GetFrameWidth(), renderer.GetFrameHeight());
    }
    else
    {
        renderTarget.Resize(renderer.GetFrameWidth(), renderer.GetFrameHeight());
    }

    renderTarget.Bind();
    renderer.PrepareUpscaleSource(renderTarget);
}

void RenderTargetUpscaleMode::EndPass(const Renderer &renderer) const
{
    if (!renderer.UsesUpscaleRenderTarget())
    {
        return;
    }

    const RenderTarget &renderTarget = renderer.GetUpscaleRenderTarget();
    renderer.PrepareUpscalePresentState(renderTarget);
    PresentUpscaled(renderer, renderTarget);
}
