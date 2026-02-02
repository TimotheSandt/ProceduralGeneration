#pragma once
#include "UIComponent.h"
#include "FBO.h"

namespace UI {

class UIContainer : public UIComponent, public std::enable_shared_from_this<UIContainer> {
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

    UIContainer(
            Bounds bounds,
            std::weak_ptr<UITheme> theme = UITheme::GetTheme("default"),
            IdentifierKind kind = IdentifierKind::PRIMARY
        );

    // Variadic template constructor for declarative UI (Option A style)
    template<typename... Args>
    UIContainer(
            Bounds bounds,
            std::weak_ptr<UITheme> theme,
            IdentifierKind kind,
            Args&&... args
        ) : UIContainer(bounds, theme, kind) {
            (AddChild(std::forward<Args>(args)), ...);
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
};



class UIHBox : public UIContainer {
public:
    using UIContainer::UIContainer;

    void RecalculateChildBounds() override;
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;
};


class UIVBox : public UIContainer {
public:
    using UIContainer::UIContainer;

    void RecalculateChildBounds() override;
    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;
};


}
