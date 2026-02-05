#include "UIComponent.h"

#include <algorithm>
#include "Logger.h"

namespace UI {


int getValueInPixel(Value v, int max) {
    if (v.type == ValueType::PERCENT) {
        return static_cast<int>(v.value * max);
    }
    return std::min(static_cast<int>(v.value), max);
}

glm::vec2 Bounds::getPixelSize() const {
    if (width.type == ValueType::PERCENT || height.type == ValueType::PERCENT) {
        LOG_ERROR(1, "Cannot get pixel size of a component with percentage width or height and no parent size");
        throw std::runtime_error("Cannot get pixel size of a component with percentage width or height and no parent size");
    }

    return {width.value, height.value};
}

glm::vec2 Bounds::getPixelSize(const glm::vec2& parentSize, int padding, int spacing) {
    if ((parentSize.x <= 0 || parentSize.y <= 0) && (width.type == ValueType::PERCENT || height.type == ValueType::PERCENT)) {
        LOG_WARNING("Parent size is invalid: (", parentSize.x, ", ", parentSize.y, ")");
        return {0, 0};
    }

    scale = {
        getValueInPixel(width, static_cast<int>(parentSize.x - padding)) - spacing,
        getValueInPixel(height, static_cast<int>(parentSize.y - padding)) - spacing
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

bool Bounds::isHover(const glm::vec2& mousePos) const {
    // Check if mouse is within the bounds (0 to scale)
    return mousePos.x >= 0 && mousePos.x <= scale.x &&
           mousePos.y >= 0 && mousePos.y <= scale.y;
}

}
