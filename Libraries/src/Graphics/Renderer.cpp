#include "Renderer.h"

#include <utility>

void Renderer::RegisterUpscaleMode(std::unique_ptr<IUpscaleMode> mode)
{
    if (mode == nullptr)
    {
        return;
    }

    upscaleModes.push_back(std::move(mode));
}

const IUpscaleMode *Renderer::FindUpscaleMode(std::string_view mode) const noexcept
{
    for (const std::unique_ptr<IUpscaleMode> &candidate : upscaleModes)
    {
        if (candidate != nullptr && candidate->GetName() == mode)
        {
            return candidate.get();
        }
    }

    return nullptr;
}

bool Renderer::SetActiveUpscaleMode(std::string_view mode)
{
    if (mode == "disabled")
    {
        activeUpscaleMode = nullptr;
        return true;
    }

    const IUpscaleMode *candidate = FindUpscaleMode(mode);
    if (candidate == nullptr || !candidate->SupportsRenderer(*this))
    {
        return false;
    }

    activeUpscaleMode = candidate;
    return true;
}

std::string_view Renderer::GetActiveUpscaleMode() const noexcept
{
    return activeUpscaleMode == nullptr ? std::string_view("disabled") : activeUpscaleMode->GetName();
}

std::vector<std::string_view> Renderer::GetRegisteredUpscaleModes() const
{
    std::vector<std::string_view> result;
    result.reserve(upscaleModes.size());
    for (const std::unique_ptr<IUpscaleMode> &mode : upscaleModes)
    {
        if (mode != nullptr)
        {
            result.push_back(mode->GetName());
        }
    }
    return result;
}

void Renderer::BeginUpscalePass(int width, int height)
{
    if (activeUpscaleMode != nullptr)
    {
        activeUpscaleMode->BeginPass(*this, width, height);
    }
}

void Renderer::EndUpscalePass() const
{
    if (activeUpscaleMode != nullptr)
    {
        activeUpscaleMode->EndPass(*this);
    }
}

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
