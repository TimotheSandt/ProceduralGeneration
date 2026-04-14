#pragma once

#include "Renderer.h"
#include "Sprite.h"

#include <glm/glm.hpp>
#include <string>

namespace UI
{
class TextRenderer;
enum class TextAnchor : std::uint8_t;
struct TextLayoutParams;
} // namespace UI

class Renderer2D : public Renderer
{
  public:
    Renderer2D() : Renderer(GraphicsAPI::OpenGL) {}

    void BeginCanvasPass() const;
    void EndCanvasPass() const;
    void PushClipRect(float x, float y, float width, float height) const;
    void PopClipRect() const;

    void RenderText(UI::TextRenderer &textRenderer, const std::string &text, float x, float y, float scale, const glm::vec3 &color,
                    UI::TextAnchor anchor) const;
    void RenderTextAdvanced(UI::TextRenderer &textRenderer, const std::string &text, float x, float y, const UI::TextLayoutParams &params,
                            const glm::vec3 &color, float scale) const;
    void DrawSprite(Sprite &sprite, const SpriteDrawParams &params, const Texture *texture = nullptr) const;
    void PresentRenderTarget(const RenderTarget &renderTarget) const;

  protected:
    void OnBeginPass() override;
    void OnEndPass() override;
    void OnClear(const glm::vec4 &clearColor, bool clearDepth) const override;
    RenderTarget &GetRenderTarget() override;
    const RenderTarget &GetRenderTarget() const override;

  private:
    RenderTarget uiRenderTarget;
};
