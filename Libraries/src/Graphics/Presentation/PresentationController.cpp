#include "Graphics/Presentation/PresentationController.h"

#include "Graphics/Renderer.h"

#include <utility>

void PresentationController::RegisterMode(std::unique_ptr<IUpscaleMode> mode)
{
    if (mode == nullptr)
    {
        return;
    }

    modes.push_back(std::move(mode));
}

const IUpscaleMode *PresentationController::FindMode(std::string_view mode) const noexcept
{
    for (const std::unique_ptr<IUpscaleMode> &candidate : modes)
    {
        if (candidate != nullptr && candidate->GetName() == mode)
        {
            return candidate.get();
        }
    }

    return nullptr;
}

bool PresentationController::SetActiveMode(std::string_view mode, const Renderer &renderer)
{
    if (mode == "disabled")
    {
        activeMode = nullptr;
        return true;
    }

    const IUpscaleMode *candidate = FindMode(mode);
    if (candidate == nullptr || !candidate->SupportsRenderer(renderer))
    {
        return false;
    }

    activeMode = candidate;
    return true;
}

std::string_view PresentationController::GetActiveMode() const noexcept
{
    return activeMode == nullptr ? std::string_view("disabled") : activeMode->GetName();
}

std::vector<std::string_view> PresentationController::GetRegisteredModes() const
{
    std::vector<std::string_view> result;
    result.reserve(modes.size());
    for (const std::unique_ptr<IUpscaleMode> &mode : modes)
    {
        if (mode != nullptr)
        {
            result.push_back(mode->GetName());
        }
    }
    return result;
}

void PresentationController::Begin(Renderer &renderer, int width, int height) const
{
    if (!enabledForRenderer || activeMode == nullptr)
    {
        return;
    }

    activeMode->BeginPass(renderer, width, height);
}

void PresentationController::End(const Renderer &renderer) const
{
    if (!enabledForRenderer || activeMode == nullptr)
    {
        return;
    }

    activeMode->EndPass(renderer);
}
