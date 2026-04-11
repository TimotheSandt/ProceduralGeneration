#include "Graphics/Upscaling/Renderer3DUpscaleModes.h"

#include "Graphics/Renderer3D.h"

std::string_view Renderer3DBilinearBlitUpscaleMode::GetName() const noexcept { return "bilinear-blit"; }

bool Renderer3DBilinearBlitUpscaleMode::SupportsRenderer(const Renderer &renderer) const noexcept
{
    return dynamic_cast<const Renderer3D *>(&renderer) != nullptr;
}

void Renderer3DBilinearBlitUpscaleMode::BeginPass(Renderer &renderer, int, int) const
{
    auto &renderer3D = static_cast<Renderer3D &>(renderer);
    renderer3D.PrepareUpscaledScenePass();
}

void Renderer3DBilinearBlitUpscaleMode::EndPass(const Renderer &renderer) const
{
    const auto &renderer3D = static_cast<const Renderer3D &>(renderer);
    renderer3D.PresentUpscaledScenePass();
}
