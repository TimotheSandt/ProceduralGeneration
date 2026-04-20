#include "Widgets/Text.h"

#include "Manager.h"

#include <cmath>
#include <utility>

namespace UI
{

TextWidgetBase::TextWidgetBase(Bounds bounds, std::string text, float scale, IdentifierKind defaultKind)
    : ComponentBase(bounds), displayedText(std::move(text)), textScale(scale), textAnchor(TextAnchor::TopLeft), autoSize(true)
{
    kind.ForceSet(defaultKind);
    UpdateTheme();
    ApplyAutoSize();
}

void TextWidgetBase::Initialize()
{
    ApplyAutoSize();
    ComponentBase::Initialize();
}

void TextWidgetBase::Update()
{
    ApplyAutoSize();
    ComponentBase::Update();
}

void TextWidgetBase::Draw(glm::vec2 containerSize, glm::vec2 offset)
{
    if (!visible.Get())
    {
        return;
    }

    ApplyAutoSize();

    std::shared_ptr<TextRenderer> textRenderer = GetTextRenderer();
    if (textRenderer == nullptr)
    {
        return;
    }

    if (containerSize.x <= 0.0f || containerSize.y <= 0.0f)
    {
        return;
    }

    textRenderer->updateScreenSize(static_cast<unsigned int>(containerSize.x), static_cast<unsigned int>(containerSize.y));

    const glm::vec2 drawPosition = ResolveDrawPosition(offset);
    const glm::vec3 textColor = glm::vec3(GetColor());
    textRenderer->renderText(displayedText, drawPosition.x, drawPosition.y, textScale, textColor, textAnchor);

    ClearDirty();
}

void TextWidgetBase::DoSetTextScale(float scale)
{
    if (textScale == scale)
    {
        return;
    }

    textScale = scale;
    layoutDirty = true;
    MarkAppearanceDirty();
    ApplyAutoSize();
}

void TextWidgetBase::DoSetTextAnchor(TextAnchor anchor)
{
    if (textAnchor == anchor)
    {
        return;
    }

    textAnchor = anchor;
    MarkAppearanceDirty();
}

void TextWidgetBase::DoSetAutoSize(bool enabled)
{
    if (autoSize == enabled)
    {
        return;
    }

    autoSize = enabled;
    layoutDirty = true;
    MarkAppearanceDirty();
    ApplyAutoSize();
}

void TextWidgetBase::SetDisplayedText(std::string text)
{
    if (displayedText == text)
    {
        return;
    }

    displayedText = std::move(text);
    layoutDirty = true;
    MarkAppearanceDirty();
    ApplyAutoSize();
}

std::shared_ptr<TextRenderer> TextWidgetBase::GetTextRenderer() const { return Manager::Instance().GetTextRenderer(); }

void TextWidgetBase::ApplyAutoSize()
{
    if (!layoutDirty)
    {
        return;
    }

    if (!autoSize)
    {
        layoutDirty = false;
        return;
    }

    const std::shared_ptr<TextRenderer> textRenderer = GetTextRenderer();
    if (textRenderer == nullptr)
    {
        return;
    }

    const glm::vec2 measuredSize = textRenderer->measureText(displayedText, textScale);
    if (SizeDiffers(localBounds.scale, measuredSize))
    {
        SetSize(measuredSize);
    }

    layoutDirty = false;
}

glm::vec2 TextWidgetBase::ResolveDrawPosition(glm::vec2 offset) const
{
    switch (textAnchor)
    {
        case TextAnchor::TopCenter:
            return {offset.x + localBounds.scale.x / 2.0f, offset.y};
        case TextAnchor::TopRight:
            return {offset.x + localBounds.scale.x, offset.y};
        case TextAnchor::CenterLeft:
            return {offset.x, offset.y + localBounds.scale.y / 2.0f};
        case TextAnchor::Center:
            return {offset.x + localBounds.scale.x / 2.0f, offset.y + localBounds.scale.y / 2.0f};
        case TextAnchor::CenterRight:
            return {offset.x + localBounds.scale.x, offset.y + localBounds.scale.y / 2.0f};
        case TextAnchor::BottomLeft:
            return {offset.x, offset.y + localBounds.scale.y};
        case TextAnchor::BottomCenter:
            return {offset.x + localBounds.scale.x / 2.0f, offset.y + localBounds.scale.y};
        case TextAnchor::BottomRight:
            return {offset.x + localBounds.scale.x, offset.y + localBounds.scale.y};
        case TextAnchor::TopLeft:
        default:
            return offset;
    }
}

bool TextWidgetBase::SizeDiffers(const glm::vec2 &lhs, const glm::vec2 &rhs)
{
    return std::abs(lhs.x - rhs.x) > 0.5f || std::abs(lhs.y - rhs.y) > 0.5f;
}

namespace
{

TextContent MakeStringPointerContent(const std::string *value)
{
    TextContent content;
    content.AppendValue(value);
    return content;
}

} // namespace

Text::Text(Bounds bounds, std::string text, float scale, IdentifierKind defaultKind)
    : Text(bounds, TextContent(std::move(text)), scale, defaultKind)
{
}

Text::Text(Bounds bounds, TextContent content, float scale, IdentifierKind defaultKind)
    : ChainableTextWidget<TextWidgetBase, Text>(bounds, content.BuildText(), scale, defaultKind), textContent(std::move(content))
{
    SyncDisplayText();
}

Text::Text(Bounds bounds, const std::string *value, float scale, IdentifierKind defaultKind)
    : Text(bounds, MakeStringPointerContent(value), scale, defaultKind)
{
}

void Text::Update()
{
    SyncDisplayText();
    TextWidgetBase::Update();
}

void Text::Draw(glm::vec2 containerSize, glm::vec2 offset)
{
    SyncDisplayText();
    TextWidgetBase::Draw(containerSize, offset);
}

std::shared_ptr<Text> Text::ClearContent()
{
    textContent.Clear();
    SyncDisplayText();
    return std::static_pointer_cast<Text>(this->shared_from_this());
}

std::shared_ptr<Text> Text::SetContent(TextContent content)
{
    textContent = std::move(content);
    SyncDisplayText();
    return std::static_pointer_cast<Text>(this->shared_from_this());
}

std::shared_ptr<Text> Text::SetText(std::string text)
{
    textContent.SetText(std::move(text));
    SyncDisplayText();
    return std::static_pointer_cast<Text>(this->shared_from_this());
}

std::shared_ptr<Text> Text::AppendText(std::string text)
{
    textContent.AppendText(std::move(text));
    SyncDisplayText();
    return std::static_pointer_cast<Text>(this->shared_from_this());
}

void Text::SyncDisplayText()
{
    const std::string nextText = textContent.BuildText();
    if (nextText == displayedText)
    {
        return;
    }

    SetDisplayedText(std::move(nextText));
}

} // namespace UI
