#include "Graphics/Upscaling/RenderTargetUpscaleMode.h"

#include "Graphics/Renderer.h"
#include "Graphics/RenderTarget.h"

RenderTargetUpscaleMode::RenderTargetUpscaleMode(std::string_view upscaleModeName) noexcept : name(upscaleModeName) {}

std::string_view RenderTargetUpscaleMode::GetName() const noexcept { return name; }

bool RenderTargetUpscaleMode::SupportsRenderer(const Renderer &) const noexcept { return true; }

int RenderTargetUpscaleMode::GetOutputWidth(const ConstUpscalePassContext &context) const noexcept { return context.outputWidth; }

int RenderTargetUpscaleMode::GetOutputHeight(const ConstUpscalePassContext &context) const noexcept { return context.outputHeight; }

void RenderTargetUpscaleMode::BeginPass(Renderer &renderer, int, int) const
{
    UpscalePassContext context = renderer.GetUpscalePassContext();
    if (context.renderTarget == nullptr)
    {
        return;
    }

    RenderTarget &renderTarget = *context.renderTarget;
    if (!renderTarget.IsInitialized())
    {
        renderTarget.Init(context.sourceWidth, context.sourceHeight);
    }
    else
    {
        renderTarget.Resize(context.sourceWidth, context.sourceHeight);
    }

    renderTarget.Bind();
    renderer.PrepareUpscaleSource(renderTarget);
}

void RenderTargetUpscaleMode::EndPass(const Renderer &renderer) const
{
    ConstUpscalePassContext context = renderer.GetUpscalePassContext();
    if (context.renderTarget == nullptr)
    {
        return;
    }

    const RenderTarget &renderTarget = *context.renderTarget;
    renderer.PrepareUpscalePresentState(renderTarget);
    PresentUpscaled(context, renderTarget);
}
