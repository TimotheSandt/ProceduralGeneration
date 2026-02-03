#pragma once
#include "UIComponent.h"
#include "FBO.h"
#include "Utilities.h"


namespace UI {

enum class HAlign { LEFT, CENTER, RIGHT };
enum class VAlign { TOP, CENTER, BOTTOM };

enum class JustifyContent { START, CENTER, END, SPACE_BETWEEN, SPACE_AROUND };


class UIContainer : public UIComponent {
protected:
    std::vector<std::shared_ptr<UIComponent>> children;

    FBO fbo;
    bool fboInitialized = false;

    glm::vec2 scrollOffset = {0, 0};
    glm::vec2 contentSize = {0, 0};

public:

    // Basic constructor
    UIContainer( Bounds bounds );

    void Initialize() override;
    void Update() override;

    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;

    // Override MarkDirty - propagates upward, not downward
    void MarkFullDirty() override;

    // Scroll
    void SetScrollOffset(glm::vec2 offset) { scrollOffset = offset; }
    void IncrementScrollOffset(glm::vec2 offset) { scrollOffset += offset; }
    glm::vec2 GetScrollOffset() const { return scrollOffset; }

    // Content size
    glm::vec2 GetContentSize() const { return contentSize; }

    // Getters for layout (resolve with theme)
    float GetPadding() const { return padding; }
    float GetSpacing() const { return spacing; }



    // Chaining methods
    std::shared_ptr<UIContainer> SetIdentifierKind(IdentifierKind k) { UIComponent::SetIdentifierKind(k); return std::static_pointer_cast<UIContainer>(shared_from_this()); }
    std::shared_ptr<UIContainer> SetPadding(float p) { padding = p; MarkDirty(); return std::static_pointer_cast<UIContainer>(shared_from_this()); }
    std::shared_ptr<UIContainer> SetSpacing(float s) { spacing = s; MarkDirty(); return std::static_pointer_cast<UIContainer>(shared_from_this()); }
    std::shared_ptr<UIContainer> SetColor(glm::vec4 c) { UIComponent::SetColor(c); return std::static_pointer_cast<UIContainer>(shared_from_this()); }

    void AddChild(std::shared_ptr<UIComponent> child);

protected:
    float padding = 0.0f;
    float spacing = 0.0f;

protected:
    void InitializedFBO();
    virtual void RecalculateChildBounds();
    virtual void CalculateContentSize();

    void RenderChildren();


    void UpdateTheme() override {
        UIComponent::UpdateTheme();
        padding = theme.lock()->GetPadding();
        spacing = theme.lock()->GetSpacing();
    }

    // Clear a specific zone in the FBO
    void ClearZone(glm::vec4 bounds);
};



class UIHBox : public UIContainer {
protected:
    VAlign childAlignment = VAlign::TOP;
    JustifyContent justifyContent = JustifyContent::START;

public:
    using UIContainer::UIContainer;

    // Chaining methods specialized for UIHBox
    std::shared_ptr<UIHBox> SetChildAlignment(VAlign align) { childAlignment = align; MarkDirty(); return std::static_pointer_cast<UIHBox>(shared_from_this()); }
    std::shared_ptr<UIHBox> SetJustifyContent(JustifyContent align) { justifyContent = align; MarkDirty(); return std::static_pointer_cast<UIHBox>(shared_from_this()); }
    std::shared_ptr<UIHBox> SetIdentifierKind(IdentifierKind k) { UIContainer::SetIdentifierKind(k); return std::static_pointer_cast<UIHBox>(shared_from_this()); }
    std::shared_ptr<UIHBox> SetPadding(float p) { UIContainer::SetPadding(p); return std::static_pointer_cast<UIHBox>(shared_from_this()); }
    std::shared_ptr<UIHBox> SetSpacing(float s) { UIContainer::SetSpacing(s); return std::static_pointer_cast<UIHBox>(shared_from_this()); }
    std::shared_ptr<UIHBox> SetColor(glm::vec4 c) { UIContainer::SetColor(c); return std::static_pointer_cast<UIHBox>(shared_from_this()); }

    VAlign GetChildAlignment() const { return childAlignment; }
    JustifyContent GetJustifyContent() const { return justifyContent; }



protected:
    void RecalculateChildBounds() override;
    void CalculateContentSize();

};


class UIVBox : public UIContainer {
protected:
    HAlign childAlignment = HAlign::LEFT;
    JustifyContent justifyContent = JustifyContent::START;

public:
    using UIContainer::UIContainer;

    // Chaining methods specialized for UIVBox
    std::shared_ptr<UIVBox> SetChildAlignment(HAlign align) { childAlignment = align; MarkDirty(); return std::static_pointer_cast<UIVBox>(shared_from_this()); }
    std::shared_ptr<UIVBox> SetJustifyContent(JustifyContent align) { justifyContent = align; MarkDirty(); return std::static_pointer_cast<UIVBox>(shared_from_this()); }
    std::shared_ptr<UIVBox> SetIdentifierKind(IdentifierKind k) { UIContainer::SetIdentifierKind(k); return std::static_pointer_cast<UIVBox>(shared_from_this()); }
    std::shared_ptr<UIVBox> SetPadding(float p) { UIContainer::SetPadding(p); return std::static_pointer_cast<UIVBox>(shared_from_this()); }
    std::shared_ptr<UIVBox> SetSpacing(float s) { UIContainer::SetSpacing(s); return std::static_pointer_cast<UIVBox>(shared_from_this()); }
    std::shared_ptr<UIVBox> SetColor(glm::vec4 c) { UIContainer::SetColor(c); return std::static_pointer_cast<UIVBox>(shared_from_this()); }

    HAlign GetChildAlignment() const { return childAlignment; }
    JustifyContent GetJustifyContent() const { return justifyContent; }



protected:
    void RecalculateChildBounds() override;
    void CalculateContentSize();

};


// ============ SwiftUI-style Factory Functions ============

// Factory for Container
std::shared_ptr<UIContainer> Container(Bounds bounds, std::vector<std::shared_ptr<UIComponent>> children) {
    auto container = std::make_shared<UIContainer>(bounds);
    container->SetColor({0.0f, 0.0f, 0.0f, 0.0f}); // Transparent by default
    for (auto& child : children) {
        container->AddChild(child);
    }
    return container;
}

// Factory for VBox
std::shared_ptr<UIVBox> VBox(Bounds bounds, std::vector<std::shared_ptr<UIComponent>> children) {
    auto vbox = std::make_shared<UIVBox>(bounds);
    for (auto& child : children) {
        vbox->AddChild(child);
    }
    return vbox;
}

// Factory for HBox
std::shared_ptr<UIHBox> HBox(Bounds bounds, std::vector<std::shared_ptr<UIComponent>> children) {
    auto hbox = std::make_shared<UIHBox>(bounds);
    for (auto& child : children) {
        hbox->AddChild(child);
    }
    return hbox;
}


// Factory for colored box (simple colored rectangle)
inline std::shared_ptr<UIComponent> Box(Bounds bounds, glm::vec4 color) {
    auto box = std::make_shared<UIComponent>(bounds);
    box->SetColor(color);
    return box;
}

}
