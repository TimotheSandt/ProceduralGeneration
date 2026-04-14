#include "Graphics/Upscaling/Modes/BilinearBlitUpscaleMode.h"

#include "Graphics/RenderTarget.h"

BilinearBlitUpscaleMode::BilinearBlitUpscaleMode(std::string_view name) : RenderTargetUpscaleMode(name) {}

void BilinearBlitUpscaleMode::Upscale(const RenderTarget &source, int outputWidth, int outputHeight) const
{
    source.BlitToScreen(outputWidth, outputHeight);
}
