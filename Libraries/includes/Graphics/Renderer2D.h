#pragma once

#include "Renderer.h"
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

class Renderer2D : public Renderer
{
  public:
    Renderer2D();

    bool IsRuntimeCompatible() const noexcept override;
    void ConfigureOutput(int outputWidth, int outputHeight);
    void SetRenderScale(float scale);
    void EnableUpscaling(bool enable);
    float GetRenderScale() const noexcept { return renderScale; }
    void BeginPass(int width, int height) override;
    void EndPass() const override;
    void Clear(const glm::vec4 &clearColor, bool clearDepth = true) const override;

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
    void GetRenderResolution(int &width, int &height) const;

  private:
    bool NeedsUpscaledCanvasPass() const noexcept;
    void PrepareUpscaledCanvasPass();
    void PresentUpscaledCanvasPass() const;

    int outputWidth = 0;
    int outputHeight = 0;
    float renderScale = 1.0f;
    RenderTarget uiRenderTarget;

    friend class Renderer2DCompositeUpscaleMode;
};
