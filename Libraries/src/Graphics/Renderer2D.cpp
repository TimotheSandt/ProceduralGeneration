#include "Renderer2D.h"

#include "Graphics/Core/RenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "UI/Rendering/TextRenderer.h"

void Renderer2D::OnBeginPass()
{
    if (UsesRenderTarget())
    {
        // Copy the current screen content (e.g. the 3D scene) into the RT so
        // the UI can be composited on top before the final upscale blit.
        GetRenderTarget().CopyFromScreen(GetOutputWidth(), GetOutputHeight());
    }

    GraphicsRenderState::PrepareScreenPass(GetFrameWidth(), GetFrameHeight());
}

void Renderer2D::OnEndPass()
{
    GraphicsRenderState::SetDepthTest(true);
}

void Renderer2D::OnClear(const glm::vec4 &clearColor, bool) const
{
    GraphicsRenderState::ClearColor(clearColor);
    GraphicsRenderState::ClearColorBuffer();
}

void Renderer2D::BeginCanvasPass() const
{
    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    GraphicsRenderState::PrepareScreenPass(GetFrameWidth(), GetFrameHeight());
    GraphicsRenderState::SetDepthTest(false);
    GraphicsRenderState::SetBlend(true);
    GraphicsRenderState::SetAlphaBlend();
}

void Renderer2D::EndCanvasPass() const
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    GraphicsRenderState::SetScissorTest(false);
    GraphicsRenderState::SetBlend(false);
    GraphicsRenderState::SetDepthTest(true);
}

void Renderer2D::PushClipRect(float x, float y, float width, float height) const
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    const int scissorY = static_cast<int>(static_cast<float>(GetFrameHeight()) - (y + height));
    GraphicsRenderState::SetScissorTest(true);
    GraphicsRenderState::SetScissor(static_cast<int>(x), scissorY, static_cast<int>(width), static_cast<int>(height));
}

void Renderer2D::PopClipRect() const
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    GraphicsRenderState::SetScissorTest(false);
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

RenderTarget &Renderer2D::GetRenderTarget() { return uiRenderTarget; }

const RenderTarget &Renderer2D::GetRenderTarget() const { return uiRenderTarget; }
