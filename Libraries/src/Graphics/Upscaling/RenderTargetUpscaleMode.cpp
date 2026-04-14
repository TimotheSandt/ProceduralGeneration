#include "Graphics/Upscaling/RenderTargetUpscaleMode.h"

RenderTargetUpscaleMode::RenderTargetUpscaleMode(std::string_view upscaleModeName) : name(upscaleModeName) {}

std::string_view RenderTargetUpscaleMode::GetName() const noexcept { return name; }

bool RenderTargetUpscaleMode::SupportsRenderer(const Renderer &) const noexcept { return true; }
