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

glm::vec2 Bounds::getPixelSize(const glm::vec2& parentSize) {
    if ((parentSize.x == 0 || parentSize.y == 0) && (width.type == ValueType::PERCENT || height.type == ValueType::PERCENT)) {
        throw std::runtime_error("Cannot get pixel size of a component with percentage width or height and no parent size");
    }

    scale = {
        getValueInPixel(width, static_cast<int>(parentSize.x)),
        getValueInPixel(height, static_cast<int>(parentSize.y))
    };

    return scale;
}

std::array<glm::vec2, 4> Bounds::getPixelBounds(const glm::vec2& parentSize) {
    getPixelSize(parentSize);
    int w = 1;
    int h = 1;

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
    return mousePos.x > (pixelBounds[0].x * scale.x) && mousePos.x < (pixelBounds[2].x * scale.x) &&
           mousePos.y > (pixelBounds[0].y * scale.y) && mousePos.y < (pixelBounds[2].y * scale.y);
}

}
