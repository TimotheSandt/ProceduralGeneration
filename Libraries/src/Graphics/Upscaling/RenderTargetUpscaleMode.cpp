#include "Graphics/Upscaling/RenderTargetUpscaleMode.h"

RenderTargetUpscaleMode::RenderTargetUpscaleMode(std::string upscaleModeName) : name(upscaleModeName) {}

std::string RenderTargetUpscaleMode::GetName() const noexcept { return name; }

bool RenderTargetUpscaleMode::SupportsRenderer(const Renderer &) const noexcept { return true; }
