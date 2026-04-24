#pragma once
#include <cstdint>
#include "Component.h"
#include "RenderTarget.h"
#include "Utilities.h"

namespace UI
{

enum class HAlign : std::uint8_t
{
    LEFT,
    CENTER,
    RIGHT
};
enum class VAlign : std::uint8_t
{
    TOP,
    CENTER,
    BOTTOM
};
enum class OverflowMode : std::uint8_t
{
    WRAP,
    HIDDEN,
    SCROLL
};

enum class JustifyContent : std::uint8_t
{
    START,
    CENTER,
    END,
    SPACE_BETWEEN,
    SPACE_AROUND
};

class ContainerBase : public ComponentBase
{
  protected:
    std::vector<std::shared_ptr<ComponentBase>> children;

    RenderTarget renderTarget;
    bool fboInitialized = false;
    DeferredValue<bool> renderToTexture = false;

    glm::vec2 scrollOffset = {0, 0};
    glm::vec2 contentSize = {0, 0};
    DeferredValue<OverflowMode> overflowMode = OverflowMode::HIDDEN;

  public:
    // Basic constructor
    explicit ContainerBase(Bounds bounds, bool useRenderTarget = false);

    void Initialize() override;
    void Update() override;

    void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0}) override;

    // Override MarkDirty - propagates upward, not downward
    void MarkFullDirty() override;

    // Scroll
    void SetScrollOffset(glm::vec2 offset) { scrollOffset = offset; }
    void IncrementScrollOffset(glm::vec2 offset) { scrollOffset += offset; }
    glm::vec2 GetScrollOffset() const { return scrollOffset; }
    bool UsesOwnRenderTarget() const { return renderToTexture.Get(); }

    // Content size
    glm::vec2 GetContentSize() const { return contentSize; }
    virtual glm::vec2 GetAvailableSize() const
    {
        float p = GetPadding();
        // Use the definitive scale - if zero, fall back to computing from raw bounds
        glm::vec2 size = localBounds.scale;
        if (size.x <= 0.0f || size.y <= 0.0f)
        {
            // Try to get size from pixel-defined bounds (won't work for percentage bounds)
            if (localBounds.width.type == ValueType::PIXEL && localBounds.height.type == ValueType::PIXEL)
            {
                size = glm::vec2(static_cast<float>(localBounds.width.value), static_cast<float>(localBounds.height.value));
            }
        }
        glm::vec2 available = size - glm::vec2(2.0f * p);
        return glm::max(available, glm::vec2(0.0f));
    }

    // Getters for layout (resolve with theme)
    float GetPadding() const { return padding.Get(); }
    float GetSpacing() const { return spacing.Get(); }
    size_t GetChildCount() const { return children.size(); }

    void DoSetPadding(float p);
    void DoSetSpacing(float s);
    void DoSetOverflowMode(OverflowMode mode);
    void DoSetRenderToTexture(bool enabled);
    void DoSetChildrenAllowDeform(bool deform);
    void OnChildAppearanceDirty();
    void OnChildLayoutDirty();

    // Children
    void AddChild(const std::shared_ptr<ComponentBase> &child);

  protected:
    DeferredValue<float> padding = 0.0f;
    DeferredValue<float> spacing = 0.0f;

  protected:
    void RefreshSpriteShader();
    void InitializeRenderTarget();
    virtual void RecalculateChildBounds();

    void RenderChildren();

    void UpdateTheme() override;

    // Clear a specific zone in the render target
    void ClearZone(glm::vec4 bounds);
};

// Chainable Container Wrapper
template <typename Base, typename Derived> class ChainableContainer : public ChainableComponent<Base, Derived>
{
  public:
    using ChainableComponent<Base, Derived>::ChainableComponent;

    std::shared_ptr<Derived> SetPadding(float p)
    {
        this->DoSetPadding(p);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetSpacing(float s)
    {
        this->DoSetSpacing(s);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetOverflowMode(OverflowMode mode)
    {
        this->DoSetOverflowMode(mode);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetRenderToTexture(bool enabled = true)
    {
        this->DoSetRenderToTexture(enabled);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }

    std::shared_ptr<Derived> SetChildrenDeform(bool deform)
    {
        this->DoSetChildrenAllowDeform(deform);
        return std::static_pointer_cast<Derived>(this->shared_from_this());
    }
};

// Concrete UIContainer
class Container : public ChainableContainer<ContainerBase, Container>
{
  public:
    using ChainableContainer<ContainerBase, Container>::ChainableContainer;
};

// ============ SwiftUI-style Factory Functions ============

// Factory for Container
inline std::shared_ptr<Container> CreateContainer(Bounds bounds = Bounds(), const std::vector<std::shared_ptr<ComponentBase>> &children = {},
                                                  bool renderToTexture = false)
{
    auto container = std::make_shared<Container>(bounds, renderToTexture);
    container->SetColor(glm::vec4{0.0f, 0.0f, 0.0f, 0.0f}); // Transparent by default
    for (auto &child : children)
    {
        container->AddChild(child);
    }
    return container;
}

// Factory for colored box (simple colored rectangle)
inline std::shared_ptr<Component> CreateBox(Bounds bounds, glm::vec4 color)
{
    auto box = std::make_shared<Component>(bounds);
    box->SetColor(color);
    return box;
}

} // namespace UI
