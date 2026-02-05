#pragma once

#include <optional>
#include <memory>
#include <array>
#include <string>

#include "InputManager.h"
#include "Mesh.h"

#include "Bounds.h"
#include "UITheme.h"
#include "DeferredValue.h"

// Forward declaration to avoid circular dependency
namespace UI { class UIContainerBase; }


namespace UI {

class UIComponentBase : public std::enable_shared_from_this<UIComponentBase> {
protected:
    Bounds localBounds;
    Mesh mesh;

    std::weak_ptr<UIContainerBase> parent;

    DeferredValue<bool> visible = true;
    bool dirty = true;
    bool dirtyLayout = true;

    std::weak_ptr<UITheme> theme;
    DeferredValue<IdentifierKind> kind;

    DeferredValue<glm::vec4> color;

    // Animation state
    glm::vec2 offset = {0, 0};
    float scaleAnim = 1.0f;
    float rotation = 0.0f;

    glm::vec4 cachedBoundsInParent = {0, 0, 0, 0};

public:
    UIComponentBase(Bounds bounds);

    virtual void Initialize();
    virtual void Update();
    virtual void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0});


    // Bounds
    void SetPixelSize(glm::vec2 size) { this->localBounds.scale = size; MarkDirty(); }
    glm::vec2 GetPixelSize();
    bool IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const;

    // Hierarchy
    void SetParent(std::weak_ptr<UIContainerBase> p) {
        parent = p;
        GetPixelSize();
        MarkDirty();
    };

    // Style
    glm::vec4 GetColor() const { return color.Get(); }

    // DoSet... methods (impl in .cpp)
    void DoSetColor(glm::vec4 c);
    void DoSetTheme(std::weak_ptr<UITheme> t);
    void DoSetIdentifierKind(IdentifierKind k);

    // Dirty state management
    void MarkDirty();
    virtual void MarkFullDirty();
    bool IsDirty() const { return dirty; }
    void ClearDirty() { dirty = false; }


    glm::vec4 GetCachedBoundsInParent() const { return cachedBoundsInParent; }
    void SetCachedBoundsInParent(glm::vec4 bounds) { cachedBoundsInParent = bounds; }

protected:
    void NotifyParentDirty();
    void NotifyParentFullDirty();
    void RecalculateChildBounds();

    virtual void UpdateTheme();

private:
    std::vector<GLfloat> GetVertices() const;
};

// Helper template for chaining
template<typename Base, typename Derived>
class Chainable : public Base {
public:
    using Base::Base;

    std::shared_ptr<Derived> SetColor(glm::vec4 c) {
        this->DoSetColor(c);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetTheme(std::weak_ptr<UITheme> t) {
        this->DoSetTheme(t);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetIdentifierKind(IdentifierKind k) {
        this->DoSetIdentifierKind(k);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }
};

// Concrete UIComponent
class UIComponent : public Chainable<UIComponentBase, UIComponent> {
public:
    using Chainable<UIComponentBase, UIComponent>::Chainable;
};

// Factory
inline std::shared_ptr<UIComponent> Component(Bounds bounds = Bounds()) {
    return std::make_shared<UIComponent>(bounds);
}

}
