#pragma once

#include "Core/Component.h"
#include "Rendering/TextRenderer.h"
#include "Utils/TextContent.h"

#include <memory>
#include <string>
#include <type_traits>
#include <utility>

namespace UI
{

namespace textdetail
{

template <typename... Args>
struct LastType;

template <typename T>
struct LastType<T>
{
    using Type = T;
};

template <typename T, typename... Rest>
struct LastType<T, Rest...> : LastType<Rest...>
{
};

template <typename... Args>
using LastTypeT = std::remove_cv_t<std::remove_reference_t<typename LastType<Args...>::Type>>;

template <typename T>
constexpr bool IsTextConfigLike_v =
    std::is_arithmetic_v<std::remove_cv_t<std::remove_reference_t<T>>> ||
    std::is_same_v<std::remove_cv_t<std::remove_reference_t<T>>, IdentifierKind>;

template <typename... Args>
concept TextContentArgs = sizeof...(Args) > 0 &&
    !(sizeof...(Args) > 1 && IsTextConfigLike_v<LastTypeT<Args...>>);

} // namespace textdetail

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
  private:
    TextContent textContent;
    void SyncDisplayText();

  public:
    Text(Bounds bounds, std::string text = {}, float scale = 1.0f, IdentifierKind defaultKind = IdentifierKind::TEXT);
    Text(Bounds bounds, TextContent content, float scale = 1.0f, IdentifierKind defaultKind = IdentifierKind::TEXT);
    Text(Bounds bounds, const std::string *targetValue, float scale = 1.0f, IdentifierKind defaultKind = IdentifierKind::TEXT);

    template <typename... Args>
        requires textdetail::TextContentArgs<Args...>
    explicit Text(Bounds bounds, Args &&...args)
        : Text(bounds, TextContent(std::forward<Args>(args)...))
    {
    }

    void Update() override;
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;

    std::shared_ptr<Text> ClearContent();
    std::shared_ptr<Text> ClearParts() { return ClearContent(); }
    std::shared_ptr<Text> SetContent(TextContent content);
    std::shared_ptr<Text> SetText(std::string text);
    std::shared_ptr<Text> AppendText(std::string text);

    TextContent &GetContent() { return textContent; }
    const TextContent &GetContent() const { return textContent; }

    std::shared_ptr<Text> SetLabel(std::string text) { return SetText(std::move(text)); }
    std::shared_ptr<Text> AppendLabel(std::string text) { return AppendText(std::move(text)); }
    const std::string &GetLabel() const { return this->GetText(); }

    template <typename T>
    std::shared_ptr<Text> SetValue(const T *value)
    {
        textContent.Clear().AppendValue(value);
        SyncDisplayText();
        return std::static_pointer_cast<Text>(this->shared_from_this());
    }

    template <typename T>
    std::shared_ptr<Text> AppendValue(const T *value)
    {
        textContent.AppendValue(value);
        SyncDisplayText();
        return std::static_pointer_cast<Text>(this->shared_from_this());
    }

    template <typename TObject, typename Method>
    std::shared_ptr<Text> SetMethod(TObject *object, Method method)
    {
        textContent.Clear().AppendMethod(object, method);
        SyncDisplayText();
        return std::static_pointer_cast<Text>(this->shared_from_this());
    }

    template <typename TObject, typename Method>
    std::shared_ptr<Text> AppendMethod(TObject *object, Method method)
    {
        textContent.AppendMethod(object, method);
        SyncDisplayText();
        return std::static_pointer_cast<Text>(this->shared_from_this());
    }
};

inline std::shared_ptr<Text> CreateText(Bounds bounds = Bounds(), std::string text = {}, float scale = 1.0f)
{
    return std::make_shared<Text>(bounds, std::move(text), scale);
}

inline std::shared_ptr<Text> CreateText(Bounds bounds, TextContent content, float scale = 1.0f, IdentifierKind defaultKind = IdentifierKind::TEXT)
{
    return std::make_shared<Text>(bounds, std::move(content), scale, defaultKind);
}

inline std::shared_ptr<Text> CreateText(Bounds bounds, const std::string *targetValue, float scale = 1.0f, IdentifierKind defaultKind = IdentifierKind::TEXT)
{
    return std::make_shared<Text>(bounds, targetValue, scale, defaultKind);
}

template <typename... Args>
    requires textdetail::TextContentArgs<Args...>
inline std::shared_ptr<Text> CreateText(Bounds bounds, Args &&...args)
{
    return std::make_shared<Text>(bounds, TextContent(std::forward<Args>(args)...));
}

inline std::shared_ptr<Text> CreateText(Bounds bounds, const std::string *value, float scale = 1.0f)
{
    return std::make_shared<Text>(bounds, value, scale);
}

} // namespace UI
