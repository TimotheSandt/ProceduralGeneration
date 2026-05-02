#pragma once

#include "Core/Component.h"
#include "Rendering/TextRenderer.h"
#include "TextContent.h"

#include <memory>
#include <string>
#include <utility>

namespace UI
{

class TextWidgetBase : public ComponentBase
{
  protected:
    std::string displayedText;
    float textScale = 1.0f;
    TextAnchor textAnchor = TextAnchor::TopLeft;
    bool autoSize = true;
    bool layoutDirty = true;

    TextWidgetBase(Bounds bounds, std::string text = {}, float scale = 1.0f, IdentifierKind defaultKind = IdentifierKind::TEXT);

  public:
    void Initialize() override;
    void Update() override;
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;

    /** @cui-modifier scale,textScale */
    void DoSetTextScale(float scale);
    /** @cui-modifier textAnchor */
    void DoSetTextAnchor(TextAnchor anchor);
    /** @cui-modifier autoSize */
    void DoSetAutoSize(bool enabled);

    const std::string &GetText() const { return displayedText; }
    float GetTextScale() const { return textScale; }
    TextAnchor GetTextAnchor() const { return textAnchor; }
    bool IsAutoSize() const { return autoSize; }

  protected:
    void SetDisplayedText(std::string text);
    std::shared_ptr<TextRenderer> GetTextRenderer() const;

  private:
    void ApplyAutoSize();
    glm::vec2 ResolveDrawPosition(glm::vec2 offset) const;
    static bool SizeDiffers(const glm::vec2 &lhs, const glm::vec2 &rhs);
};

template <typename Base, typename Derived>
class ChainableTextWidget : public ChainableComponent<Base, Derived>
{
  public:
    using ChainableComponent<Base, Derived>::ChainableComponent;

    std::shared_ptr<Derived> SetTextScale(float scale)
    {
        this->DoSetTextScale(scale);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetTextAnchor(TextAnchor anchor)
    {
        this->DoSetTextAnchor(anchor);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetAutoSize(bool enabled)
    {
        this->DoSetAutoSize(enabled);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }
};

/**
 * @cui-component
 * @cui-accepts-children false
 * @cui-content-model text_content
 */
class Text : public ChainableTextWidget<TextWidgetBase, Text>
{
    TextContent textContent;

    void SyncDisplayText();

  public:
    Text(Bounds bounds, std::string text = {}, float scale = 1.0f, IdentifierKind defaultKind = IdentifierKind::TEXT);
    Text(Bounds bounds, TextContent content, float scale = 1.0f, IdentifierKind defaultKind = IdentifierKind::TEXT);

    void Update() override;
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;

    // ── Content ──────────────────────────────────────────────────

    std::shared_ptr<Text> SetContent(TextContent content);
    std::shared_ptr<Text> SetText(std::string text);

    TextContent       &GetContent()       { return textContent; }
    const TextContent &GetContent() const { return textContent; }
    const std::string &GetLabel()   const { return this->GetText(); }
};

// ── Factory ──────────────────────────────────────────────────────

inline std::shared_ptr<Text> CreateText(Bounds bounds = {}, std::string text = {}, float scale = 1.0f)
{
    return std::make_shared<Text>(bounds, std::move(text), scale);
}

inline std::shared_ptr<Text> CreateText(Bounds bounds, TextContent content, float scale = 1.0f, IdentifierKind kind = IdentifierKind::TEXT)
{
    return std::make_shared<Text>(bounds, std::move(content), scale, kind);
}

} // namespace UI
