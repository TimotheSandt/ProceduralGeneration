#include "UIComponent.h"

#include <algorithm>

namespace UI {

int getValueInPixel(Value v, int max) {
    if (v.type == ValueType::PERCENT) {
        return static_cast<int>(v.value * max);
    }
    return std::min(static_cast<int>(v.value), max);
}

glm::vec2 Bounds::getPixelSize(const glm::vec2& parentSize) const {
    if ((parentSize.x == 0 || parentSize.y == 0) && (width.type == ValueType::PERCENT || height.type == ValueType::PERCENT)) {
        throw std::runtime_error("Cannot get pixel size of a component with percentage width or height and no parent size");
    }

    int w = getValueInPixel(width, static_cast<int>(parentSize.x));
    int h = getValueInPixel(height, static_cast<int>(parentSize.y));
    return {w, h};
}

std::array<glm::vec2, 4> Bounds::getPixelBounds(const glm::vec2& parentSize) {
    glm::vec2 pixelSize = getPixelSize(parentSize);
    int w = pixelSize.x;
    int h = pixelSize.y;

    switch (anchor)
    {
    case Anchor::TOP_LEFT:
        pixelBounds[0] = {0, 0};
        pixelBounds[1] = {w, 0};
        pixelBounds[2] = {w, h};
        pixelBounds[3] = {0, h};
        break;
    case Anchor::TOP_CENTER:
        pixelBounds[0] = {parentSize.x / 2 - w / 2, 0};
        pixelBounds[1] = {parentSize.x / 2 + w / 2, 0};
        pixelBounds[2] = {parentSize.x / 2 + w / 2, h};
        pixelBounds[3] = {parentSize.x / 2 - w / 2, h};
        break;
    case Anchor::TOP_RIGHT:
        pixelBounds[0] = {parentSize.x - w, 0};
        pixelBounds[1] = {parentSize.x, 0};
        pixelBounds[2] = {parentSize.x, h};
        pixelBounds[3] = {parentSize.x - w, h};
        break;
    case Anchor::MIDDLE_LEFT:
        pixelBounds[0] = {0, parentSize.y / 2 - h / 2};
        pixelBounds[1] = {w, parentSize.y / 2 - h / 2};
        pixelBounds[2] = {w, parentSize.y / 2 + h / 2};
        pixelBounds[3] = {0, parentSize.y / 2 + h / 2};
        break;
    case Anchor::CENTER:
        pixelBounds[0] = {parentSize.x / 2 - w / 2, parentSize.y / 2 - h / 2};
        pixelBounds[1] = {parentSize.x / 2 + w / 2, parentSize.y / 2 - h / 2};
        pixelBounds[2] = {parentSize.x / 2 + w / 2, parentSize.y / 2 + h / 2};
        pixelBounds[3] = {parentSize.x / 2 - w / 2, parentSize.y / 2 + h / 2};
        break;
    case Anchor::MIDDLE_RIGHT:
        pixelBounds[0] = {parentSize.x - w, parentSize.y / 2 - h / 2};
        pixelBounds[1] = {parentSize.x, parentSize.y / 2 - h / 2};
        pixelBounds[2] = {parentSize.x, parentSize.y / 2 + h / 2};
        pixelBounds[3] = {parentSize.x - w, parentSize.y / 2 + h / 2};
        break;
    case Anchor::BOTTOM_LEFT:
        pixelBounds[0] = {0, parentSize.y - h};
        pixelBounds[1] = {w, parentSize.y - h};
        pixelBounds[2] = {w, parentSize.y};
        pixelBounds[3] = {0, parentSize.y};
        break;
    case Anchor::BOTTOM_CENTER:
        pixelBounds[0] = {parentSize.x / 2 - w / 2, parentSize.y - h};
        pixelBounds[1] = {parentSize.x / 2 + w / 2, parentSize.y - h};
        pixelBounds[2] = {parentSize.x / 2 + w / 2, parentSize.y};
        pixelBounds[3] = {parentSize.x / 2 - w / 2, parentSize.y};
        break;
    case Anchor::BOTTOM_RIGHT:
        pixelBounds[0] = {parentSize.x - w, parentSize.y - h};
        pixelBounds[1] = {parentSize.x, parentSize.y - h};
        pixelBounds[2] = {parentSize.x, parentSize.y};
        pixelBounds[3] = {parentSize.x - w, parentSize.y};
        break;
    }
    return pixelBounds;
}

bool Bounds::isHover(const glm::vec2& mousePos) const {
    return mousePos.x > pixelBounds[0].x && mousePos.x < pixelBounds[2].x &&
           mousePos.y > pixelBounds[0].y && mousePos.y < pixelBounds[2].y;
}


UIComponent::UIComponent(Bounds bounds) : UIComponent(bounds, Anchor::TOP_LEFT) {}

UIComponent::UIComponent(
        Bounds bounds, Anchor anchor,
        std::weak_ptr<UITheme> theme, IdentifierKind kind,
        std::weak_ptr<UIComponent> parent)
            : localBounds(bounds), anchor(anchor),
            theme(theme), kind(kind),
            parent(parent) {
    
    localBounds.getPixelBounds();
}


glm::vec2 UIComponent::GetPixelSize() const {
    if (parent.lock() != nullptr) {
        return localBounds.getPixelSize(parent.lock()->GetPixelSize());
    }
    return localBounds.getPixelSize({0, 0});
}

void UIComponent::Draw(int maxW, int maxH) {

}

} // namespace UI
