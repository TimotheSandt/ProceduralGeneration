#include "UIComponent.h"

#include <algorithm>

namespace UI {


int getValueInPixel(Value v, int max) {
    if (v.type == ValueType::PERCENT) {
        return static_cast<int>(v.value * max);
    }
    return std::min(static_cast<int>(v.value), max);
}

glm::vec2 Bounds::getPixelSize() const {
    if (width.type == ValueType::PERCENT || height.type == ValueType::PERCENT) {
        throw std::runtime_error("Cannot get pixel size of a component with percentage width or height and no parent size");
    }

    return {width.value, height.value};
}

glm::vec2 Bounds::getPixelSize(const glm::vec2& parentAvailableSize) {
    if ((parentAvailableSize.x <= 0 || parentAvailableSize.y <= 0) && (width.type == ValueType::PERCENT || height.type == ValueType::PERCENT)) {
        throw std::runtime_error("Cannot get pixel size of a component with percentage width or height and no parent available size");
    }

    scale = {
        getValueInPixel(width, static_cast<int>(parentAvailableSize.x)),
        getValueInPixel(height, static_cast<int>(parentAvailableSize.y))
    };

    return scale;
}

glm::vec2 Bounds::getAnchorOffset(const glm::vec2& containerSize) const {
    float xOffset = 0;
    float yOffset = 0;

    switch (anchor) {
        case Anchor::TOP_LEFT:
            xOffset = 0;
            yOffset = 0;
            break;
        case Anchor::TOP_CENTER:
            xOffset = (containerSize.x - scale.x) / 2.0f;
            yOffset = 0;
            break;
        case Anchor::TOP_RIGHT:
            xOffset = containerSize.x - scale.x;
            yOffset = 0;
            break;
        case Anchor::MIDDLE_LEFT:
            xOffset = 0;
            yOffset = (containerSize.y - scale.y) / 2.0f;
            break;
        case Anchor::CENTER:
            xOffset = (containerSize.x - scale.x) / 2.0f;
            yOffset = (containerSize.y - scale.y) / 2.0f;
            break;
        case Anchor::MIDDLE_RIGHT:
            xOffset = containerSize.x - scale.x;
            yOffset = (containerSize.y - scale.y) / 2.0f;
            break;
        case Anchor::BOTTOM_LEFT:
            xOffset = 0;
            yOffset = containerSize.y - scale.y;
            break;
        case Anchor::BOTTOM_CENTER:
            xOffset = (containerSize.x - scale.x) / 2.0f;
            yOffset = containerSize.y - scale.y;
            break;
        case Anchor::BOTTOM_RIGHT:
            xOffset = containerSize.x - scale.x;
            yOffset = containerSize.y - scale.y;
            break;
    }

    return {xOffset, yOffset};
}

std::array<glm::vec2, 4> Bounds::getPixelBounds() {
    pixelBounds[0] = {0, 0};
    pixelBounds[1] = {1, 0};
    pixelBounds[2] = {1, 1};
    pixelBounds[3] = {0, 1};

    return pixelBounds;
}

bool Bounds::isHover(const glm::vec2& mousePos) const {
    // Check if mouse is within the bounds (0 to scale)
    return mousePos.x >= 0 && mousePos.x <= scale.x &&
           mousePos.y >= 0 && mousePos.y <= scale.y;
}

}
