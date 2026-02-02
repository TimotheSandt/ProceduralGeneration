#pragma once
#include "UIComponent.h"
#include "FBO.h"

namespace UI {

// Horizontal alignment for VBox children
enum class HAlign { LEFT, CENTER, RIGHT };

// Vertical alignment for HBox children
enum class VAlign { TOP, CENTER, BOTTOM };

// Main Axis Alignment (Flexbox style)
enum class JustifyContent { START, CENTER, END, SPACE_BETWEEN, SPACE_AROUND };


class UIContainer : public UIComponent {
protected:
    std::vector<std::shared_ptr<UIComponent>> children;

    // FBO for cached rendering
    FBO fbo;
    bool fboInitialized = false;

    // Scroll support
    glm::vec2 scrollOffset = {0, 0};

    // Total content size (may be larger than visible area for scrolling)
    glm::vec2 contentSize = {0, 0};

public:

    // Basic constructor
    UIContainer(
            Bounds bounds,
            std::weak_ptr<UITheme> theme = UITheme::GetTheme("default"),
            IdentifierKind kind = IdentifierKind::PRIMARY
        );

    // SwiftUI-style constructor: Bounds + children
    template<typename... Children>
    UIContainer(
            Bounds bounds,
            std::weak_ptr<UITheme> theme = UITheme::GetTheme("default"),
            IdentifierKind kind = IdentifierKind::PRIMARY,
            std::shared_ptr<Children>... children)
        : UIContainer(bounds, theme, kind) {
            (AddChild(children), ...);
        }

    // Add a single child
    void AddChild(std::shared_ptr<UIComponent> child);

    // Remove a child
    void RemoveChild(std::shared_ptr<UIComponent> child);

    // Get children count
    size_t GetChildCount() const { return children.size(); }

    // Override Draw
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;

    // Override MarkDirty - propagates upward, not downward
    void MarkDirty(DirtyType d = DirtyType::ALL) override;

    // Scroll
    void SetScrollOffset(glm::vec2 offset) { scrollOffset = offset; }
    glm::vec2 GetScrollOffset() const { return scrollOffset; }

    // Content size
    glm::vec2 GetContentSize() const { return contentSize; }

    // Chaining methods
    std::shared_ptr<UIContainer> SetPadding(float p) { padding = p; RecalculateChildBounds(); MarkDirty(); return std::static_pointer_cast<UIContainer>(shared_from_this()); }
    std::shared_ptr<UIContainer> SetSpacing(float s) { spacing = s; RecalculateChildBounds(); MarkDirty(); return std::static_pointer_cast<UIContainer>(shared_from_this()); }

    // Shadow SetColor to return UIContainer type
    std::shared_ptr<UIContainer> SetColor(glm::vec4 c) {
        UIComponent::SetColor(c);
        return std::static_pointer_cast<UIContainer>(shared_from_this());
    }

    // Getters for layout (resolve with theme)
    float GetPadding() const;
    float GetSpacing() const;

protected:
    // Layout overrides (-1 = use theme)
    float padding = -1.0f;
    float spacing = -1.0f;

    // Recalculate child bounds (override in HBox/VBox for layout)
    virtual void RecalculateChildBounds();

protected:
    // Ensure FBO is initialized with correct size
    void EnsureFBOInitialized();

    // Full render of all children to FBO
    void RenderToFBO();

    // Partial render: only dirty children with scissor clear
    void RenderDirtyChildren();

    // Clear a specific zone in the FBO
    void ClearZone(glm::vec4 bounds);

    // Get child offset from cached bounds
    glm::vec2 GetChildOffset(const std::shared_ptr<UIComponent>& child) const {
        glm::vec4 bounds = child->GetCachedBoundsInParent();
        return {bounds.x, bounds.y};
    }
};



class UIHBox : public UIContainer {
protected:
    VAlign childAlignment = VAlign::TOP;
    JustifyContent justifyContent = JustifyContent::START;

public:
    using UIContainer::UIContainer;

    // Chaining methods specialized for UIHBox
    std::shared_ptr<UIHBox> SetChildAlignment(VAlign align) { childAlignment = align; RecalculateChildBounds(); MarkDirty(); return std::static_pointer_cast<UIHBox>(shared_from_this()); }
    std::shared_ptr<UIHBox> SetJustifyContent(JustifyContent align) { justifyContent = align; RecalculateChildBounds(); MarkDirty(); return std::static_pointer_cast<UIHBox>(shared_from_this()); }
    std::shared_ptr<UIHBox> SetPadding(float p) { UIContainer::SetPadding(p); return std::static_pointer_cast<UIHBox>(shared_from_this()); }
    std::shared_ptr<UIHBox> SetSpacing(float s) { UIContainer::SetSpacing(s); return std::static_pointer_cast<UIHBox>(shared_from_this()); }
    std::shared_ptr<UIHBox> SetColor(glm::vec4 c) { UIContainer::SetColor(c); return std::static_pointer_cast<UIHBox>(shared_from_this()); }

    VAlign GetChildAlignment() const { return childAlignment; }
    JustifyContent GetJustifyContent() const { return justifyContent; }

    void RecalculateChildBounds() override;
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;
};


class UIVBox : public UIContainer {
protected:
    HAlign childAlignment = HAlign::LEFT;
    JustifyContent justifyContent = JustifyContent::START;

public:
    using UIContainer::UIContainer;

    // Chaining methods specialized for UIVBox
    std::shared_ptr<UIVBox> SetChildAlignment(HAlign align) { childAlignment = align; RecalculateChildBounds(); MarkDirty(); return std::static_pointer_cast<UIVBox>(shared_from_this()); }
    std::shared_ptr<UIVBox> SetJustifyContent(JustifyContent align) { justifyContent = align; RecalculateChildBounds(); MarkDirty(); return std::static_pointer_cast<UIVBox>(shared_from_this()); }
    std::shared_ptr<UIVBox> SetPadding(float p) { UIContainer::SetPadding(p); return std::static_pointer_cast<UIVBox>(shared_from_this()); }
    std::shared_ptr<UIVBox> SetSpacing(float s) { UIContainer::SetSpacing(s); return std::static_pointer_cast<UIVBox>(shared_from_this()); }
    std::shared_ptr<UIVBox> SetColor(glm::vec4 c) { UIContainer::SetColor(c); return std::static_pointer_cast<UIVBox>(shared_from_this()); }

    HAlign GetChildAlignment() const { return childAlignment; }
    JustifyContent GetJustifyContent() const { return justifyContent; }

    void RecalculateChildBounds() override;
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;
};


// ============ SwiftUI-style Factory Functions ============

// Factory for Container
template<typename... Children>
std::shared_ptr<UIContainer> Container(Bounds bounds, std::shared_ptr<Children>... children) {
    auto container = std::make_shared<UIContainer>(bounds);
    container->SetColor({0.0f, 0.0f, 0.0f, 0.0f}); // Transparent by default
    (container->AddChild(children), ...);
    return container;
}

// Factory for VBox
template<typename... Children>
std::shared_ptr<UIVBox> VBox(Bounds bounds, std::shared_ptr<Children>... children) {
    auto vbox = std::make_shared<UIVBox>(bounds);
    vbox->SetColor({0.0f, 0.0f, 0.0f, 0.0f}); // Transparent by default
    (vbox->AddChild(children), ...);
    return vbox;
}

// Factory for VBox with alignment
template<typename... Children>
std::shared_ptr<UIVBox> VBox(Bounds bounds, HAlign align, std::shared_ptr<Children>... children) {
    auto vbox = std::make_shared<UIVBox>(bounds);
    vbox->SetColor({0.0f, 0.0f, 0.0f, 0.0f}); // Transparent by default
    vbox->SetChildAlignment(align);
    (vbox->AddChild(children), ...);
    return vbox;
}

// Factory for HBox
template<typename... Children>
std::shared_ptr<UIHBox> HBox(Bounds bounds, std::shared_ptr<Children>... children) {
    auto hbox = std::make_shared<UIHBox>(bounds);
    hbox->SetColor({0.0f, 0.0f, 0.0f, 0.0f}); // Transparent by default
    (hbox->AddChild(children), ...);
    return hbox;
}

// Factory for HBox with alignment
template<typename... Children>
std::shared_ptr<UIHBox> HBox(Bounds bounds, VAlign align, std::shared_ptr<Children>... children) {
    auto hbox = std::make_shared<UIHBox>(bounds);
    hbox->SetColor({0.0f, 0.0f, 0.0f, 0.0f}); // Transparent by default
    hbox->SetChildAlignment(align);
    (hbox->AddChild(children), ...);
    return hbox;
}

// Factory for colored box (simple colored rectangle)
inline std::shared_ptr<UIComponent> Box(Bounds bounds, glm::vec4 color) {
    auto box = std::make_shared<UIComponent>(bounds);
    box->SetColor(color);
    return box;
}

// ============ Overloads with IdentifierKind ============

template<typename... Children>
std::shared_ptr<UIContainer> Container(Bounds bounds, IdentifierKind kind, std::shared_ptr<Children>... children) {
    auto container = std::make_shared<UIContainer>(bounds, UITheme::GetTheme("default"), kind);
    (container->AddChild(children), ...);
    return container;
}

template<typename... Children>
std::shared_ptr<UIVBox> VBox(Bounds bounds, IdentifierKind kind, std::shared_ptr<Children>... children) {
    auto vbox = std::make_shared<UIVBox>(bounds, UITheme::GetTheme("default"), kind);
    (vbox->AddChild(children), ...);
    return vbox;
}

template<typename... Children>
std::shared_ptr<UIVBox> VBox(Bounds bounds, IdentifierKind kind, HAlign align, std::shared_ptr<Children>... children) {
    auto vbox = std::make_shared<UIVBox>(bounds, UITheme::GetTheme("default"), kind);
    vbox->SetChildAlignment(align);
    (vbox->AddChild(children), ...);
    return vbox;
}

template<typename... Children>
std::shared_ptr<UIHBox> HBox(Bounds bounds, IdentifierKind kind, std::shared_ptr<Children>... children) {
    auto hbox = std::make_shared<UIHBox>(bounds, UITheme::GetTheme("default"), kind);
    (hbox->AddChild(children), ...);
    return hbox;
}

template<typename... Children>
std::shared_ptr<UIHBox> HBox(Bounds bounds, IdentifierKind kind, VAlign align, std::shared_ptr<Children>... children) {
    auto hbox = std::make_shared<UIHBox>(bounds, UITheme::GetTheme("default"), kind);
    hbox->SetChildAlignment(align);
    (hbox->AddChild(children), ...);
    return hbox;
}


}
