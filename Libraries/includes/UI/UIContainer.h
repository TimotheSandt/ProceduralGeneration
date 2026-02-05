#pragma once
#include "UIComponent.h"
#include "FBO.h"
#include "Utilities.h"


namespace UI {

enum class HAlign { LEFT, CENTER, RIGHT };
enum class VAlign { TOP, CENTER, BOTTOM };

enum class JustifyContent { START, CENTER, END, SPACE_BETWEEN, SPACE_AROUND };


class UIContainerBase : public UIComponentBase {
protected:
    std::vector<std::shared_ptr<UIComponentBase>> children;

    FBO fbo;
    bool fboInitialized = false;

    glm::vec2 scrollOffset = {0, 0};
    glm::vec2 contentSize = {0, 0};

public:

    // Basic constructor
    UIContainerBase( Bounds bounds );

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
    float GetPadding() const { return padding.Get(); }
    float GetSpacing() const { return spacing.Get(); }
    size_t GetChildCount() const { return children.size(); }

    void DoSetPadding(float p);
    void DoSetSpacing(float s);

    void AddChild(std::shared_ptr<UIComponentBase> child);

protected:
    DeferredValue<float> padding = 0.0f;
    DeferredValue<float> spacing = 0.0f;

protected:
    void InitializedFBO();
    virtual void RecalculateChildBounds();
    virtual void CalculateContentSize();

    void RenderChildren();


    void UpdateTheme() override;

    // Clear a specific zone in the FBO
    void ClearZone(glm::vec4 bounds);
};

// Chainable Container Wrapper
template<typename Base, typename Derived>
class ChainableContainer : public Chainable<Base, Derived> {
public:
    using Chainable<Base, Derived>::Chainable;

    std::shared_ptr<Derived> SetPadding(float p) {
        this->DoSetPadding(p);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetSpacing(float s) {
        this->DoSetSpacing(s);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }
};

// Concrete UIContainer
class UIContainer : public ChainableContainer<UIContainerBase, UIContainer> {
public:
    using ChainableContainer<UIContainerBase, UIContainer>::ChainableContainer;
};


// ============ SwiftUI-style Factory Functions ============

// Factory for Container
inline std::shared_ptr<UIContainer> Container(Bounds bounds = Bounds(), std::vector<std::shared_ptr<UIComponentBase>> children = {}) {
    auto container = std::make_shared<UIContainer>(bounds);
    container->SetColor(glm::vec4{0.0f, 0.0f, 0.0f, 0.0f}); // Transparent by default
    for (auto& child : children) {
        container->AddChild(child);
    }
    return container;
}


// Factory for colored box (simple colored rectangle)
inline std::shared_ptr<UIComponent> Box(Bounds bounds, glm::vec4 color) {
    auto box = std::make_shared<UIComponent>(bounds);
    box->SetColor(color);
    return box;
}

}
