#include "Renderer2D.h"

#include "Graphics/Backends/OpenGL/OpenGLRenderState.h"
#include "Graphics/Core/GraphicsRuntime.h"
#include "UI/TextRenderer.h"

bool Renderer2D::IsRuntimeCompatible() const noexcept { return IsGraphicsAPIActive(GraphicsAPI::OpenGL); }

void Renderer2D::BeginPass(int width, int height)
{
    frameWidth = width;
    frameHeight = height;

    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    OpenGLRenderState::PrepareScreenPass(frameWidth, frameHeight);
}

void Renderer2D::EndPass() const
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    OpenGLRenderState::SetScissorTest(false);
}

void Renderer2D::BeginCanvasPass() const
{
    if (!IsRuntimeCompatible() || !HasValidFrameExtent())
    {
        return;
    }

    OpenGLRenderState::PrepareScreenPass(frameWidth, frameHeight);
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
}

void Renderer2D::PushClipRect(float x, float y, float width, float height) const
{
    if (!IsRuntimeCompatible())
    {
        return;
    }

    const int scissorY = static_cast<int>(static_cast<float>(frameHeight) - (y + height));
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

    textRenderer.updateScreenSize(static_cast<unsigned int>(frameWidth), static_cast<unsigned int>(frameHeight));
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

    textRenderer.updateScreenSize(static_cast<unsigned int>(frameWidth), static_cast<unsigned int>(frameHeight));
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

    renderTarget.RenderScreenQuad(frameWidth, frameHeight);
}
