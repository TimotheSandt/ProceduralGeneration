#include "Renderer.h"

#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/RenderTarget.h"
#include "Graphics/Upscaling/Modes/BilinearBlitUpscaleMode.h"

Renderer::Renderer()
{
    RegisterUpscaleMode(std::make_unique<BilinearBlitUpscaleMode>());
    SetActiveUpscaleMode("bilinear-blit");
}

bool Renderer::IsRuntimeCompatible() const noexcept { return IsGraphicsAPIActive(GetRequiredAPI()); }

void Renderer::SetOutputResolution(int width, int height) noexcept
{
    outputWidth = width;
    outputHeight = height;
}

void Renderer::SetRenderScale(float scale) noexcept
{
    if (scale <= 0.0f || scale > 1.0f)
    {
        return;
    }

    renderScale = scale;
    SetUpscalingEnabled(scale < 1.0f);
}

void Renderer::SetUpscalingEnabled(bool enabled) noexcept { upscalingEnabled = enabled; }

void Renderer::GetRenderResolution(int &width, int &height) const noexcept
{
    if (upscalingEnabled && renderScale < 1.0f && outputWidth > 0 && outputHeight > 0)
    {
        width = static_cast<int>(static_cast<float>(outputWidth) * renderScale);
        height = static_cast<int>(static_cast<float>(outputHeight) * renderScale);
        return;
    }

    width = outputWidth;
    height = outputHeight;
}

bool Renderer::UsesRenderTarget() const noexcept
{
    return (upscalingEnabled && renderScale < 1.0f && outputWidth > 0 && outputHeight > 0) ||
           !postProcessPasses.empty() || frameGenMode != nullptr;
}

void Renderer::BeginPass()
{
    int width = 0;
    int height = 0;
    GetRenderResolution(width, height);
    SetFrameExtent(width, height);

    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    if (UsesRenderTarget())
    {
        GetRenderTarget().Resize(width, height);
        GetRenderTarget().Bind();
    }
    else
    {
        GraphicsRenderState::BindDefaultFramebuffer();
    }

    OnBeginPass();
}

void Renderer::EndPass()
{
    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    if (UsesRenderTarget())
    {
        for (const auto &pass : postProcessPasses)
        {
            if (pass)
            {
                pass->Process(GetRenderTarget(), frameWidth, frameHeight);
            }
        }

        GraphicsRenderState::SetScissorTest(false);
        GraphicsRenderState::SetBlend(false);

        if (activeUpscaleMode != nullptr)
        {
            activeUpscaleMode->Upscale(GetRenderTarget(), outputWidth, outputHeight);
        }
    }

    OnEndPass();
}

void Renderer::Clear(const glm::vec4 &clearColor, bool clearDepth) const
{
    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    OnClear(clearColor, clearDepth);
}

void Renderer::RegisterUpscaleMode(std::unique_ptr<IUpscaleMode> mode)
{
    if (mode == nullptr)
    {
        return;
    }

    upscaleModes.push_back(std::move(mode));
}

bool Renderer::SetActiveUpscaleMode(std::string_view name)
{
    if (name == "disabled")
    {
        activeUpscaleMode = nullptr;
        return true;
    }

    for (const auto &mode : upscaleModes)
    {
        if (mode && mode->GetName() == name && mode->SupportsRenderer(*this))
        {
            activeUpscaleMode = mode.get();
            return true;
        }
    }

    return false;
}

std::string_view Renderer::GetActiveUpscaleMode() const noexcept
{
    return activeUpscaleMode == nullptr ? std::string_view("disabled") : activeUpscaleMode->GetName();
}

std::vector<std::string_view> Renderer::GetRegisteredUpscaleModes() const
{
    std::vector<std::string_view> result;
    result.reserve(upscaleModes.size());

    for (const auto &mode : upscaleModes)
    {
        if (mode)
        {
            result.push_back(mode->GetName());
        }
    }

    return result;
}

void Renderer::AddPostProcessPass(std::unique_ptr<IPostProcessPass> pass)
{
    if (pass)
    {
        postProcessPasses.push_back(std::move(pass));
    }
}

void Renderer::RemovePostProcessPass(std::string_view name)
{
    std::erase_if(postProcessPasses, [name](const std::unique_ptr<IPostProcessPass> &pass)
                  { return pass && pass->GetName() == name; });
}

void Renderer::SetFrameGenerationMode(std::unique_ptr<IFrameGenerationMode> mode)
{
    frameGenMode = std::move(mode);
}
