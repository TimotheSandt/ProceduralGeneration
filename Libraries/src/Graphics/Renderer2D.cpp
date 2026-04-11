#include "Renderer2D.h"

#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "Graphics/Upscaling/Modes/BilinearBlitUpscaleMode.h"
#include "UI/TextRenderer.h"

Renderer2D::Renderer2D()
{
    RegisterUpscaleMode(std::make_unique<BilinearBlitUpscaleMode>());
    static_cast<void>(SetActiveUpscaleMode("bilinear-blit"));
}

bool Renderer2D::IsRuntimeCompatible() const noexcept { return IsGraphicsAPIActive(GraphicsAPI::OpenGL); }

void Renderer2D::ConfigureOutput(int width, int height)
{
    outputWidth = width;
    outputHeight = height;
}

void Renderer2D::SetRenderScale(float scale)
{
    if (scale <= 0.0f || scale > 1.0f)
    {
        return;
    }

    renderScale = scale;
    EnableUpscaling(scale != 1.0f);
}

void Renderer2D::EnableUpscaling(bool enable) { SetUpscalingEnabled(enable && renderScale != 1.0f); }

void Renderer2D::GetRenderResolution(int &width, int &height) const
{
    if (UsesUpscaleRenderTarget())
    {
        width = static_cast<int>(static_cast<float>(outputWidth) * renderScale);
        height = static_cast<int>(static_cast<float>(outputHeight) * renderScale);
        return;
    }

    width = outputWidth;
    height = outputHeight;
}

void Renderer2D::BeginPass(int width, int height)
{
    SetFrameExtent(width, height);

    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    if (UsesUpscaleRenderTarget())
    {
        BeginUpscalePass(width, height);
    }

    OpenGLRenderState::PrepareScreenPass(GetFrameWidth(), GetFrameHeight());
}

void Renderer2D::Clear(const glm::vec4 &clearColor, bool) const
{
    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    OpenGLRenderState::ClearColor(clearColor);
    OpenGLRenderState::Clear(GL_COLOR_BUFFER_BIT);
}

void Renderer2D::EndPass() const
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    if (UsesUpscaleRenderTarget())
    {
        EndUpscalePass();
    }

    OpenGLRenderState::SetScissorTest(false);
    OpenGLRenderState::SetBlend(false);
    OpenGLRenderState::SetDepthTest(true);
}

void Renderer2D::BeginCanvasPass() const
{
    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    OpenGLRenderState::PrepareScreenPass(GetFrameWidth(), GetFrameHeight());
    OpenGLRenderState::SetDepthTest(false);
    OpenGLRenderState::SetBlend(true);
    OpenGLRenderState::SetAlphaBlend();
}

void Renderer2D::EndCanvasPass() const
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    OpenGLRenderState::SetScissorTest(false);
    OpenGLRenderState::SetBlend(false);
    OpenGLRenderState::SetDepthTest(true);
}

void Renderer2D::PushClipRect(float x, float y, float width, float height) const
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    const int scissorY = static_cast<int>(static_cast<float>(GetFrameHeight()) - (y + height));
    OpenGLRenderState::SetScissorTest(true);
    OpenGLRenderState::SetScissor(static_cast<int>(x), scissorY, static_cast<int>(width), static_cast<int>(height));
}

void Renderer2D::PopClipRect() const
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    OpenGLRenderState::SetScissorTest(false);
}

void Renderer2D::RenderText(UI::TextRenderer &textRenderer, const std::string &text, float x, float y, float scale, const glm::vec3 &color,
                            UI::TextAnchor anchor) const
{
    if (!HasValidFrameExtent())
    {
        return;
    }

    textRenderer.updateScreenSize(static_cast<unsigned int>(GetFrameWidth()), static_cast<unsigned int>(GetFrameHeight()));
    BeginCanvasPass();
    textRenderer.renderText(text, x, y, scale, color, anchor);
    EndCanvasPass();
}

void Renderer2D::RenderTextAdvanced(UI::TextRenderer &textRenderer, const std::string &text, float x, float y,
                                    const UI::TextLayoutParams &params, const glm::vec3 &color, float scale) const
{
    if (!HasValidFrameExtent())
    {
        return;
    }

    textRenderer.updateScreenSize(static_cast<unsigned int>(GetFrameWidth()), static_cast<unsigned int>(GetFrameHeight()));
    BeginCanvasPass();

    const bool useScissor = (params.overflow == UI::TextOverflow::Hidden || params.overflow == UI::TextOverflow::Scroll) &&
                            params.maxWidth > 0.0f && params.maxHeight > 0.0f;
    if (useScissor)
    {
        PushClipRect(x, y, params.maxWidth, params.maxHeight);
    }

    textRenderer.renderTextAdvanced(text, x, y, params, color, scale);

    if (useScissor)
    {
        PopClipRect();
    }

    EndCanvasPass();
}

void Renderer2D::DrawSprite(Sprite &sprite, const SpriteDrawParams &params, const Texture *texture) const
{
    if (!HasValidFrameExtent())
    {
        return;
    }

    BeginCanvasPass();
    sprite.Draw(params, texture);
    EndCanvasPass();
}

void Renderer2D::PresentRenderTarget(const RenderTarget &renderTarget) const
{
    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    renderTarget.RenderScreenQuad(GetFrameWidth(), GetFrameHeight());
}

bool Renderer2D::UsesUpscaleRenderTarget() const noexcept
{
    return IsUpscalingEnabled() && outputWidth > 0 && outputHeight > 0 && (GetFrameWidth() != outputWidth || GetFrameHeight() != outputHeight);
}

RenderTarget &Renderer2D::GetUpscaleRenderTarget() { return uiRenderTarget; }

const RenderTarget &Renderer2D::GetUpscaleRenderTarget() const { return uiRenderTarget; }

int Renderer2D::GetUpscaleOutputWidth() const noexcept { return outputWidth; }

int Renderer2D::GetUpscaleOutputHeight() const noexcept { return outputHeight; }

void Renderer2D::PrepareUpscaleSource(RenderTarget &renderTarget)
{
    renderTarget.CopyFromScreen(outputWidth, outputHeight);
}

void Renderer2D::PrepareUpscalePresentState(const RenderTarget &) const
{
    OpenGLRenderState::SetViewport(0, 0, outputWidth, outputHeight);
    OpenGLRenderState::SetScissorTest(false);
    OpenGLRenderState::SetDepthTest(false);
    OpenGLRenderState::SetBlend(false);
}
