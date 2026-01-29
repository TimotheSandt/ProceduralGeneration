#pragma once
#include <glm/glm.hpp>
#include <optional>
#include <memory>
#include <array>
#include <string>
#include "InputManager.h"
#include "Mesh.h"
#include "UITheme.h"


namespace UI {

enum class ValueType { PIXEL, PERCENT };
struct Value { float value; ValueType type = ValueType::PIXEL; };

enum class Anchor {
    TOP_LEFT, TOP_CENTER, TOP_RIGHT,
    MIDDLE_LEFT, CENTER, MIDDLE_RIGHT,
    BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT
};


class Bounds {
    Value width, height;
    Anchor anchor = Anchor::TOP_LEFT;
    std::array<glm::vec2, 4> pixelBounds;

    Bounds() {}
    Bounds(Value width, Value height) : width(width), height(height) {}
    Bounds(Value width, Value height, Anchor anchor) : width(width), height(height), anchor(anchor) {}

    glm::vec2 getPixelSize(const glm::vec2& parentSize) const;

    std::array<glm::vec2, 4> getPixelBounds() const { return pixelBounds; }
    std::array<glm::vec2, 4> getPixelBounds(const glm::vec2& parentSize);
    bool isHover(const glm::vec2& mousePos) const;
};

class UIComponent : public std::enable_shared_from_this<UIComponent> {
protected:
    Bounds localBounds;
    Mesh mesh;

    std::weak_ptr<UIComponent> parent;
    Anchor anchor = Anchor::TOP_LEFT;

    bool visible = true;
    bool dirty = true;
    bool focused = false;

    std::weak_ptr<UITheme> theme;
    IdentifierKind kind;

    std::optional<glm::vec4> color;

    // Animation state
    glm::vec2 offset = {0, 0};
    float scaleAnim = 1.0f;
    float rotation = 0.0f;

public:
    UIComponent(Bounds bounds);
    UIComponent(
            Bounds bounds, Anchor anchor,
            std::weak_ptr<UITheme> theme = UITheme::GetTheme("default"), IdentifierKind kind = IdentifierKind::PRIMARY,
            std::weak_ptr<UIComponent> parent = std::weak_ptr<UIComponent>()
        );
    virtual ~UIComponent() = default;


    virtual void Update() {}
    virtual void Draw(int maxW, int maxH) = 0;
    virtual void HandleInput(InputManager* input, int screenW, int screenH) {};

    // Bounds
    glm::vec2 GetPixelSize() const;
    Bounds GetAbsoluteBounds(int screenW, int screenH) const;
    bool IsMouseOver(InputManager* input, int screenW, int screenH) const;

    // Hierarchy
    void SetParent(std::weak_ptr<UIComponent> p) { parent = p; MarkDirty(); };
    void MarkDirty() { dirty = true; };

    // Style
    glm::vec4 GetColor() const;
    void SetColor(glm::vec4 c) { color = c; }
};

}
