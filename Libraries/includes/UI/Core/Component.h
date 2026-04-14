#pragma once

#include <optional>
#include <memory>
#include <array>
#include <string>
#include <utility>

#include "InputManager.h"
#include "Sprite.h"

#include "Bounds.h"
#include "../Rendering/Theme.h"
#include "DeferredValue.h"

// Forward declaration to avoid circular dependency
namespace UI
{
class ContainerBase;
}

namespace UI
{

class ComponentBase : public std::enable_shared_from_this<ComponentBase>
{
  protected:
    Bounds localBounds;
    Sprite sprite;

    std::weak_ptr<ContainerBase> parent;

    DeferredValue<bool> visible = true;

    // Three-tier dirty system
    bool dirtyAppearance = true;   // Color/visibility - zone clear only
    bool dirtyChildLayout = false; // Child size/position - cascade/full clear
    bool dirtySelfLayout = true;   // Own size - full render target reset

    std::weak_ptr<Theme> theme;
    DeferredValue<IdentifierKind> kind;

    DeferredValue<glm::vec4> color;
    bool isDeformed = false;
    DeferredValue<bool> allowDeform = false;

    // Animation state
    glm::vec2 offset = {0, 0};
    float scaleAnim = 1.0f;
    float rotation = 0.0f;

    glm::vec4 cachedBoundsInParent = {0, 0, 0, 0};

  public:
    ComponentBase(Bounds bounds);

    virtual void Initialize();
    virtual void Update();
    virtual void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0});

    // Bounds
    void SetSize(glm::vec2 size)
    {
        this->localBounds.scale = size;
        this->localBounds.width = Value{static_cast<double>(size.x), ValueType::PIXEL};
        this->localBounds.height = Value{static_cast<double>(size.y), ValueType::PIXEL};
        MarkSelfLayoutDirty();
    }
    void SetPixelSize(glm::vec2 size)
    {
        this->localBounds.scale = size;
        MarkSelfLayoutDirty();
    }

    glm::vec2 CalculatePixelSize();
    glm::vec2 GetPixelSize() const { return localBounds.scale; }
    glm::vec2 GetAnchorOffset(glm::vec2 containerSize) const { return localBounds.getAnchorOffset(containerSize); }
    bool IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const;

    // Hierarchy
    void SetParent(std::weak_ptr<ContainerBase> p)
    {
        parent = std::move(p);
        CalculatePixelSize();
        MarkSelfLayoutDirty();
    };

    // Style
    glm::vec4 GetColor() const { return color.Get(); }

    // DoSet... methods (impl in .cpp)
    void DoSetColor(glm::vec4 c);
    void DoSetTheme(std::weak_ptr<Theme> t);
    void DoSetIdentifierKind(IdentifierKind k);
    void DoSetAllowDeform(bool allow);

    bool DoesAllowDeform() const { return allowDeform.Get(); }
    void DoSetDeform(bool deform)
    {
        if (!allowDeform.Get())
        {
            throw std::runtime_error("UIComponent does not allow deformations");
        }
        isDeformed = deform;
    }

    // Dirty state management - three-tier system
    void MarkAppearanceDirty();   // Color/visibility change
    void MarkChildLayoutDirty();  // Child repositioned
    void MarkSelfLayoutDirty();   // Own size changed
    virtual void MarkFullDirty(); // Everything dirty

    bool IsAppearanceDirty() const { return dirtyAppearance; }
    bool IsChildLayoutDirty() const { return dirtyChildLayout; }
    bool IsSelfLayoutDirty() const { return dirtySelfLayout; }
    bool IsDirty() const { return dirtyAppearance || dirtyChildLayout || dirtySelfLayout; }
    void ClearDirty()
    {
        dirtyAppearance = false;
        dirtyChildLayout = false;
        dirtySelfLayout = false;
    }

    glm::vec4 GetCachedBoundsInParent() const { return cachedBoundsInParent; }
    void SetCachedBoundsInParent(glm::vec4 bounds) { cachedBoundsInParent = bounds; }

  protected:
    void NotifyParentChildLayoutDirty();
    void NotifyParentFullDirty();

    virtual void UpdateTheme();
};

// Helper template for chaining
template <typename Base, typename Derived> class ChainableComponent : public Base
{
  public:
    using Base::Base;

    std::shared_ptr<Derived> SetColor(glm::vec4 c)
    {
        this->DoSetColor(c);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetTheme(std::weak_ptr<Theme> t)
    {
        this->DoSetTheme(t);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetIdentifierKind(IdentifierKind k)
    {
        this->DoSetIdentifierKind(k);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetAllowDeform(bool allow)
    {
        this->DoSetAllowDeform(allow);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetDeform(bool deform)
    {
        this->DoSetDeform(deform);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }
};

// Concrete UIComponent
class Component : public ChainableComponent<ComponentBase, Component>
{
  public:
    using ChainableComponent<ComponentBase, Component>::ChainableComponent;
};

// Factory
inline std::shared_ptr<Component> CreateComponent(Bounds bounds = Bounds()) { return std::make_shared<Component>(bounds); }

} // namespace UI
