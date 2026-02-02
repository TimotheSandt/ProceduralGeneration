#pragma once
#include <glm/glm.hpp>
#include <optional>
#include <memory>
#include <array>
#include <string>
#include "InputManager.h"
#include "Mesh.h"
#include "UITheme.h"

// Forward declaration to avoid circular dependency
namespace UI { class UIContainer; }


namespace UI {

enum class ValueType { PIXEL, PERCENT };
struct Value { float value; ValueType type = ValueType::PIXEL; };

enum class Anchor {
    TOP_LEFT, TOP_CENTER, TOP_RIGHT,
    MIDDLE_LEFT, CENTER, MIDDLE_RIGHT,
    BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT
};

enum class DirtyType {
    NONE,
    ALL,
    TRANSFORM,
    ANIMATION,
    CONTENT,
    THEME,
    VISIBILITY
};


struct Bounds {
    Value width, height;
    Anchor anchor = Anchor::TOP_LEFT;
    std::array<glm::vec2, 4> pixelBounds;

    glm::vec2 scale = {0, 0};

    Bounds() {}
    Bounds(Value width, Value height) : width(width), height(height) {}
    Bounds(Value width, Value height, Anchor anchor) : width(width), height(height), anchor(anchor) {}

    glm::vec2 getPixelSize() const;
    glm::vec2 getPixelSize(const glm::vec2& parentSize);

    std::array<glm::vec2, 4> getPixelBounds() const { return pixelBounds; }
    std::array<glm::vec2, 4> getPixelBounds(const glm::vec2& parentSize);
    bool isHover(const glm::vec2& mousePos) const;
};

class UIComponent {
protected:
    Bounds localBounds;
    Mesh mesh;

    std::weak_ptr<UIContainer> parent;

    bool visible = true;
    DirtyType dirty = DirtyType::ALL;
    bool focused = false;

    std::weak_ptr<UITheme> theme;
    IdentifierKind kind;

    glm::vec4 color;

    // Animation state
    glm::vec2 offset = {0, 0};
    float scaleAnim = 1.0f;
    float rotation = 0.0f;

    // Position in parent's FBO for partial clear/render
    glm::vec4 cachedBoundsInParent = {0, 0, 0, 0}; // x, y, width, height

    // First-time initialization flag
    bool needsInitialize = true;

public:
    UIComponent(
            Bounds bounds,
            std::weak_ptr<UITheme> theme = UITheme::GetTheme("default"),
            IdentifierKind kind = IdentifierKind::PRIMARY
        );
    virtual ~UIComponent() = default;


    virtual void Update() {}
    virtual void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0});

    // First-time initialization (called once before first render)
    virtual void Initialize() {}

    // Bounds
    void SetPixelSize(glm::vec2 size) { this->localBounds.scale = size; MarkDirty(); }
    glm::vec2 GetPixelSize();
    bool IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const;

    // Hierarchy
    void SetParent(std::weak_ptr<UIContainer> p) { parent = p; MarkDirty(); };
    std::weak_ptr<UIContainer> GetParent() const { return parent; }

    // Style
    glm::vec4 GetColor() const { return color;}
    void SetColor(glm::vec4 c) { color = c; MarkDirty(DirtyType::CONTENT); }

    // Dirty state management
    virtual void MarkDirty(DirtyType d = DirtyType::ALL);
    bool IsDirty() const { return dirty != DirtyType::NONE; }
    DirtyType GetDirtyType() const { return dirty; }
    void ClearDirty() { dirty = DirtyType::NONE; }

    // Cached bounds in parent's FBO (for partial rendering)
    glm::vec4 GetCachedBoundsInParent() const { return cachedBoundsInParent; }
    void SetCachedBoundsInParent(glm::vec4 bounds) { cachedBoundsInParent = bounds; }

protected:
    // Notify parent that this component changed (propagate dirty upward)
    void NotifyParentDirty();

private:
    std::vector<GLfloat> GetVertices() const;
};

}
