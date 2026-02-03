#pragma once

#include <optional>
#include <memory>
#include <array>
#include <string>

#include "InputManager.h"
#include "Mesh.h"

#include "Bounds.h"
#include "UITheme.h"

// Forward declaration to avoid circular dependency
namespace UI { class UIContainer; }


namespace UI {

class UIComponent : public std::enable_shared_from_this<UIComponent> {
protected:
    Bounds localBounds;
    Mesh mesh;

    std::weak_ptr<UIContainer> parent;

    bool visible = true;
    bool dirty = true;

    std::weak_ptr<UITheme> theme;
    IdentifierKind kind;

    glm::vec4 color ;

    // Animation state
    glm::vec2 offset = {0, 0};
    float scaleAnim = 1.0f;
    float rotation = 0.0f;

    glm::vec4 cachedBoundsInParent = {0, 0, 0, 0};

public:
    UIComponent(Bounds bounds);

    virtual void Initialize();
    virtual void Update() {}
    virtual void Draw(glm::vec2 containerSize, glm::vec2 offset = {0, 0});


    // Bounds
    void SetPixelSize(glm::vec2 size) { this->localBounds.scale = size; MarkDirty(); }
    glm::vec2 GetPixelSize();
    bool IsMouseOver(glm::vec2 mousePos, glm::vec2 offset) const;

    // Hierarchy
    void SetParent(std::weak_ptr<UIContainer> p) {
        parent = p;
        GetPixelSize();
        MarkDirty();
    };

    // Style
    glm::vec4 GetColor() const { return color; }

    std::shared_ptr<UIComponent> SetColor(glm::vec4 c) { color = c; MarkDirty(); return shared_from_this(); }
    std::shared_ptr<UIComponent> SetTheme(std::weak_ptr<UITheme> t) { theme = t; UpdateTheme(); MarkDirty(); return shared_from_this(); }
    std::shared_ptr<UIComponent> SetIdentifierKind(IdentifierKind k) { kind = k; UpdateTheme(); MarkDirty(); return shared_from_this(); }

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

}
