#pragma once

#include "Graphics/Core/GraphicsTypes.h"
#include "RenderTarget.h"
#include "Sprite.h"

#include <glm/glm.hpp>
#include <string>

namespace UI
{
class TextRenderer;
class UIManager;
enum class TextAnchor : std::uint8_t;
struct TextLayoutParams;
} // namespace UI

class Renderer2D
{
  public:
    bool IsRuntimeCompatible() const noexcept;
    void BeginPass(int width, int height);
    void EndPass() const;

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

    void BeginFrame(int width, int height) { BeginPass(width, height); }

    int GetFrameWidth() const noexcept { return frameWidth; }
    int GetFrameHeight() const noexcept { return frameHeight; }
    bool HasValidFrameExtent() const noexcept { return frameWidth > 0 && frameHeight > 0; }

  private:
    int frameWidth = 0;
    int frameHeight = 0;
};
