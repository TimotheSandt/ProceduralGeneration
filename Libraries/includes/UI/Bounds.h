#pragma once


#include <glm/glm.hpp>


namespace UI {

enum class ValueType { PIXEL, PERCENT };
struct Value { double value; ValueType type = ValueType::PIXEL; };

constexpr Value operator""_pct(unsigned long long value) {
    return Value(value / 100.0, ValueType::PERCENT);
}


constexpr Value operator""_px(unsigned long long value) {
    return Value(value, ValueType::PIXEL);
}


enum class Anchor {
    TOP_LEFT, TOP_CENTER, TOP_RIGHT,
    MIDDLE_LEFT, CENTER, MIDDLE_RIGHT,
    BOTTOM_LEFT, BOTTOM_CENTER, BOTTOM_RIGHT
};



struct Bounds {
    Value width, height;
    Anchor anchor = Anchor::TOP_LEFT;
    std::array<glm::vec2, 4> pixelBounds;

    glm::vec2 scale = {0, 0};

    Bounds() {}
    Bounds(Value width, Value height) : Bounds(width, height, Anchor::TOP_LEFT) {}
    Bounds(Value width, Value height, Anchor anchor) : width(width), height(height), anchor(anchor) {}

    glm::vec2 getPixelSize() const;
    glm::vec2 getPixelSize(const glm::vec2& parentSize);

    // Get offset based on anchor relative to container size
    glm::vec2 getAnchorOffset(const glm::vec2& containerSize) const;

    std::array<glm::vec2, 4> getPixelBounds() const { return pixelBounds; }
    std::array<glm::vec2, 4> getPixelBounds(const glm::vec2& parentSize);
    bool isHover(const glm::vec2& mousePos) const;
};

}
