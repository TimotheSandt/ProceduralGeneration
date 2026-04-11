#include "Graphics/Upscaling/Modes/BilinearBlitUpscaleMode.h"

#include "Graphics/RenderTarget.h"
#include "Graphics/Renderer.h"

BilinearBlitUpscaleMode::BilinearBlitUpscaleMode(std::string_view name) noexcept : RenderTargetUpscaleMode(name) {}

void BilinearBlitUpscaleMode::PresentUpscaled(const Renderer &renderer, const RenderTarget &renderTarget) const
{
    renderTarget.BlitToScreen(GetOutputWidth(renderer), GetOutputHeight(renderer));
}
