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

    // Three-tier dirty system
    bool dirtyAppearance = true;    // Color/visibility - zone clear only
    bool dirtyChildLayout = false;  // Child size/position - cascade/full clear
    bool dirtySelfLayout = true;    // Own size - full FBO reset

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
    void SetPixelSize(glm::vec2 size) {
        this->localBounds.scale = size;
        this->localBounds.width = Value{static_cast<double>(size.x), ValueType::PIXEL};
        this->localBounds.height = Value{static_cast<double>(size.y), ValueType::PIXEL};
        MarkSelfLayoutDirty();
    }
    glm::vec2 GetPixelSize();
    glm::vec2 GetAnchorOffset(glm::vec2 containerSize) const { return localBounds.getAnchorOffset(containerSize); }
    bool IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const;

    // Hierarchy
    void SetParent(std::weak_ptr<UIContainerBase> p) {
        parent = p;
        GetPixelSize();
        MarkSelfLayoutDirty();
    };

    // Style
    glm::vec4 GetColor() const { return color.Get(); }

    // DoSet... methods (impl in .cpp)
    void DoSetColor(glm::vec4 c);
    void DoSetTheme(std::weak_ptr<UITheme> t);
    void DoSetIdentifierKind(IdentifierKind k);

    // Dirty state management - three-tier system
    void MarkAppearanceDirty();                     // Color/visibility change
    void MarkChildLayoutDirty();                    // Child repositioned
    void MarkSelfLayoutDirty();                     // Own size changed
    virtual void MarkFullDirty();                   // Everything dirty

    bool IsAppearanceDirty() const { return dirtyAppearance; }
    bool IsChildLayoutDirty() const { return dirtyChildLayout; }
    bool IsSelfLayoutDirty() const { return dirtySelfLayout; }
    bool IsDirty() const { return dirtyAppearance || dirtyChildLayout || dirtySelfLayout; }
    void ClearDirty() { dirtyAppearance = false; dirtyChildLayout = false; dirtySelfLayout = false; }


    glm::vec4 GetCachedBoundsInParent() const { return cachedBoundsInParent; }
    void SetCachedBoundsInParent(glm::vec4 bounds) { cachedBoundsInParent = bounds; }

protected:
    void NotifyParentChildLayoutDirty();
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
