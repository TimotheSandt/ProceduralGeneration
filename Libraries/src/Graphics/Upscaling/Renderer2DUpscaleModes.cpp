#include "Graphics/Upscaling/Renderer2DUpscaleModes.h"

#include "Graphics/Renderer2D.h"

std::string_view Renderer2DCompositeUpscaleMode::GetName() const noexcept { return "composite-blit"; }

bool Renderer2DCompositeUpscaleMode::SupportsRenderer(const Renderer &renderer) const noexcept
{
    return dynamic_cast<const Renderer2D *>(&renderer) != nullptr;
}

void Renderer2DCompositeUpscaleMode::BeginPass(Renderer &renderer, int, int) const
{
    auto &renderer2D = static_cast<Renderer2D &>(renderer);
    renderer2D.PrepareUpscaledCanvasPass();
}

void Renderer2DCompositeUpscaleMode::EndPass(const Renderer &renderer) const
{
    const auto &renderer2D = static_cast<const Renderer2D &>(renderer);
    renderer2D.PresentUpscaledCanvasPass();
}
