#include "Renderer.h"

#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Core/GraphicsTypes.h"
#include "Graphics/Core/RenderState.h"
#include "Graphics/Upscaling/IAdvancedUpscaleMode.h"
#include "Graphics/Upscaling/Modes/BilinearBlitUpscaleMode.h"

Renderer::Renderer(GraphicsAPI requiredApi) : requiredApi(requiredApi)
{
    RegisterUpscaleMode(std::make_unique<BilinearBlitUpscaleMode>());
    SetActiveUpscaleMode("bilinear-blit");
}

Renderer::~Renderer()
{
    if (frameGenMode)
    {
        frameGenMode->Shutdown();
    }
}

bool Renderer::IsRuntimeCompatible() const noexcept
{
    return TryGetActiveGraphicsBackend() != nullptr && TryGetActiveGraphicsDevice() != nullptr &&
           (IsGraphicsAPIActive(requiredApi) || IsGraphicsAPIActive(GraphicsAPI::Vulkan));
}

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

bool Renderer::IsScaledRendering() const noexcept
{
    return upscalingEnabled && renderScale < 1.0f && outputWidth > 0 && outputHeight > 0;
}

void Renderer::GetRenderResolution(int &width, int &height) const noexcept
{
    if (IsScaledRendering())
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
    return IsScaledRendering() || !postProcessPasses.empty() || frameGenMode != nullptr
        || motionVectorsEnabled || depthAsTextureEnabled;
}

bool Renderer::NeedsTemporalResources() const noexcept
{
    if (frameGenMode != nullptr)
    {
        return true;
    }
    const UpscaleRequirements req = GetActiveUpscaleModeRequirements();
    return req.needsHistory || req.needsDepth || req.needsMotionVectors;
}

void Renderer::SetMotionVectorsEnabled(bool enabled) noexcept { motionVectorsEnabled = enabled; }

void Renderer::SetDepthAsTextureEnabled(bool enabled) noexcept { depthAsTextureEnabled = enabled; }

void Renderer::SetJitterSequenceLength(std::uint32_t length) noexcept
{
    jitterSequenceLength = length > 0 ? length : 1;
    jitterIndex = jitterIndex % jitterSequenceLength;
}

// Halton low-discrepancy sequence for one component.
// Returns a value in (0, 1) for sample index i and the given base.
static float Halton(std::uint32_t index, std::uint32_t base)
{
    float result = 0.0f;
    float denominator = 1.0f;
    while (index > 0)
    {
        denominator *= static_cast<float>(base);
        result += static_cast<float>(index % base) / denominator;
        index /= base;
    }
    return result;
}

static glm::vec2 HaltonJitter(std::uint32_t index, std::uint32_t sequenceLength,
                               int renderWidth, int renderHeight)
{
    // Sample index cycles through [1, sequenceLength] (avoid index 0 which gives (0,0)).
    const std::uint32_t sampleIndex = (index % sequenceLength) + 1;

    // Halton(2,3): X in base 2, Y in base 3 — standard choice for TAA/DLSS.
    // Map from (0,1) to (-0.5, 0.5) pixel range, then convert to NDC.
    const float jx = (Halton(sampleIndex, 2) - 0.5f) * 2.0f / static_cast<float>(renderWidth);
    const float jy = (Halton(sampleIndex, 3) - 0.5f) * 2.0f / static_cast<float>(renderHeight);
    return {jx, jy};
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

    // Advance the jitter sequence automatically when temporal resources are needed.
    if (jitterMode == JitterMode::Auto)
    {
        if (NeedsTemporalResources())
        {
            currentJitter = HaltonJitter(jitterIndex, jitterSequenceLength, width, height);
            jitterIndex = (jitterIndex + 1) % jitterSequenceLength;
        }
        else
        {
            currentJitter = {0.0f, 0.0f};
            jitterIndex = 0;
        }
    }

    if (UsesRenderTarget())
    {
        RenderTargetDesc desc;
        desc.extent = {static_cast<std::uint32_t>(width), static_cast<std::uint32_t>(height)};
        desc.colorAttachments = {TextureFormat::RGBA8};
        if (motionVectorsEnabled)
        {
            desc.colorAttachments.push_back(TextureFormat::RG16F);
        }
        desc.hasDepthBuffer = true;
        desc.depthAsTexture = depthAsTextureEnabled;

        RenderTarget &rt = GetRenderTarget();
        rt.ResizeOrReconfigure(desc);
        rt.Bind();
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
        RenderTarget &rt = GetRenderTarget();

        for (const auto &pass : postProcessPasses)
        {
            pass->Process(rt, frameWidth, frameHeight);
        }

        GraphicsRenderState::SetScissorTest(false);
        GraphicsRenderState::SetBlend(false);

        // Snapshot the color output into the history buffer BEFORE upscaling,
        // so frame generation receives a render-resolution previous frame.
        const bool needsHistory = NeedsTemporalResources();
        if (needsHistory)
        {
            historyTarget.ResizeOrReconfigure({
                .extent           = {static_cast<std::uint32_t>(frameWidth), static_cast<std::uint32_t>(frameHeight)},
                .colorAttachments = {TextureFormat::RGBA8},
                .hasDepthBuffer   = false,
            });
            rt.BlitToRenderTarget(historyTarget);  // rt (this=source) → historyTarget (destination)
        }

        if (activeUpscaleMode != nullptr)
        {
            // For advanced modes, build the full UpscaleInput so Execute() receives
            // depth, motion vectors, history, and jitter.
            if (auto *advanced = dynamic_cast<IAdvancedUpscaleMode *>(activeUpscaleMode))
            {
                UpscaleInput upscaleInput;
                upscaleInput.color            = &rt.GetTexture(0);
                upscaleInput.depth            = rt.TryGetDepthTexture();
                upscaleInput.motionVectors    = (motionVectorsEnabled && rt.GetColorAttachmentCount() > 1)
                                                    ? &rt.GetTexture(1) : nullptr;
                upscaleInput.historyColor     = historyTarget.IsInitialized()
                                                    ? &historyTarget.GetTexture(0) : nullptr;
                upscaleInput.renderResolution = {static_cast<float>(frameWidth), static_cast<float>(frameHeight)};
                upscaleInput.outputResolution = {static_cast<float>(outputWidth), static_cast<float>(outputHeight)};
                upscaleInput.jitter           = currentJitter;
                upscaleInput.deltaTimeSeconds = deltaTime;
                upscaleInput.resetHistory     = resetHistoryNextFrame;

                UpscaleOutput upscaleOutput;
                advanced->Execute(upscaleInput, upscaleOutput);
            }
            else
            {
                activeUpscaleMode->Upscale(rt, outputWidth, outputHeight);
            }
        }

        if (frameGenMode != nullptr)
        {
            FrameGenerationInput fgInput;
            fgInput.currentColor      = &rt.GetTexture(0);
            fgInput.previousColor     = historyTarget.IsInitialized() ? &historyTarget.GetTexture(0) : nullptr;
            fgInput.depth             = rt.TryGetDepthTexture();
            fgInput.motionVectors     = (motionVectorsEnabled && rt.GetColorAttachmentCount() > 1)
                                            ? &rt.GetTexture(1) : nullptr;
            fgInput.renderResolution  = {static_cast<float>(frameWidth), static_cast<float>(frameHeight)};
            fgInput.outputResolution  = {static_cast<float>(outputWidth), static_cast<float>(outputHeight)};
            fgInput.jitter            = currentJitter;
            fgInput.deltaTimeSeconds  = deltaTime;
            fgInput.resetHistory      = resetHistoryNextFrame;

            FrameGenerationOutput fgOutput;
            frameGenMode->GenerateFrame(fgInput, fgOutput);
        }

        resetHistoryNextFrame = false;
    }

    GraphicsRenderState::SetScissorTest(false);
    GraphicsRenderState::SetBlend(false);

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

bool Renderer::SetActiveUpscaleMode(std::string name)
{
    if (name == "disabled")
    {
        activeUpscaleMode = nullptr;
        return true;
    }

    for (auto &mode : upscaleModes)
    {
        if (mode && mode->GetName() == name && mode->SupportsRenderer(*this))
        {
            activeUpscaleMode = mode.get();
            return true;
        }
    }

    return false;
}

std::string Renderer::GetActiveUpscaleMode() const noexcept
{
    return activeUpscaleMode == nullptr ? std::string("disabled") : activeUpscaleMode->GetName();
}

std::vector<std::string> Renderer::GetRegisteredUpscaleModes() const
{
    std::vector<std::string> result;
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

UpscaleRequirements Renderer::GetActiveUpscaleModeRequirements() const noexcept
{
    return activeUpscaleMode != nullptr ? activeUpscaleMode->GetRequirements() : UpscaleRequirements{};
}

void Renderer::AddPostProcessPass(std::unique_ptr<IPostProcessPass> pass)
{
    if (pass)
    {
        postProcessPasses.push_back(std::move(pass));
    }
}

void Renderer::RemovePostProcessPass(std::string name)
{
    std::erase_if(postProcessPasses, [name](const std::unique_ptr<IPostProcessPass> &pass)
                  { return pass && pass->GetName() == name; });
}

void Renderer::SetFrameGenerationMode(std::unique_ptr<IFrameGenerationMode> mode)
{
    if (frameGenMode)
    {
        frameGenMode->Shutdown();
    }

    frameGenMode = std::move(mode);

    if (frameGenMode)
    {
        const IGraphicsDevice *device = TryGetActiveGraphicsDevice();
        if (device != nullptr && !frameGenMode->Initialize(*device))
        {
            frameGenMode.reset();
        }
    }
}
