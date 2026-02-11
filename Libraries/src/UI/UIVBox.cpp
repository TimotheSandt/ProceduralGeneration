#include "UIVBox.h"


namespace UI {



// ============ UIVBoxBase Implementation ============

void UIVBoxBase::RecalculateChildBounds() {
    float padding = GetPadding();
    float spacing = GetSpacing();

    float yOffset = padding;
    float maxWidth = 0;
    float totalChildrenHeight = 0;
    int visibleChildrenCount = 0;

    // First pass: calculate max width
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        maxWidth = std::max(maxWidth, childSize.x);
        totalChildrenHeight += childSize.y;
        visibleChildrenCount++;
    }

    if (visibleChildrenCount > 1) totalChildrenHeight += (visibleChildrenCount - 1) * spacing;

    // Content dimensions
    float containerWidth = std::max(maxWidth + 2 * padding, localBounds.scale.x);
    float containerHeight = std::max(totalChildrenHeight + 2 * padding, localBounds.scale.y);

    // Calculate starting offset and extra spacing based on JustifyContent
    float currentSpacing = spacing;

    if (justifyContent != JustifyContent::START && containerHeight > totalChildrenHeight + 2 * padding) {
        float freeSpace = containerHeight - (2 * padding + totalChildrenHeight);
        switch (justifyContent) {
            case JustifyContent::CENTER: yOffset = padding + freeSpace / 2.0f; break;
            case JustifyContent::END: yOffset = containerHeight - padding - totalChildrenHeight; break;
            case JustifyContent::SPACE_BETWEEN:
                yOffset = padding; if (visibleChildrenCount > 1) currentSpacing = spacing + freeSpace / (visibleChildrenCount - 1); break;
            case JustifyContent::SPACE_AROUND:
                if (visibleChildrenCount > 0) { float ex = freeSpace / visibleChildrenCount; currentSpacing = spacing + ex; yOffset = padding + ex / 2.0f; } break;
            default: break;
        }
    }

    // Second pass: set positions with horizontal alignment
    // yOffset is already initialized
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();

        float xPos = padding;
        switch (childAlignment) {
            case HAlign::LEFT:
                xPos = padding;
                break;
            case HAlign::CENTER:
                xPos = (containerWidth - childSize.x) / 2.0f;
                break;
            case HAlign::RIGHT:
                xPos = containerWidth - padding - childSize.x;
                break;
        }

        child->SetCachedBoundsInParent({xPos, yOffset, childSize.x, childSize.y});
        yOffset += childSize.y + currentSpacing;
    }

    if (!children.empty()) yOffset -= spacing; // Remove last spacing
    yOffset += padding;

    contentSize = {containerWidth, containerHeight};

    if (this->overflowMode.Get() == OverflowMode::WRAP) {
        if (contentSize.x > localBounds.scale.x || contentSize.y > localBounds.scale.y) {
            glm::vec2 reducer = {
                localBounds.scale.x / contentSize.x,
                localBounds.scale.y / contentSize.y
            };
            for (auto& child : children) {
                child->SetCachedBoundsInParent({
                    child->GetCachedBoundsInParent().x * reducer.x,
                    child->GetCachedBoundsInParent().y * reducer.y,
                    child->GetCachedBoundsInParent().z * reducer.x,
                    child->GetCachedBoundsInParent().w * reducer.y
                });
            }
            contentSize = localBounds.scale;
        }
    }
}



glm::vec2 UIVBoxBase::GetAvailableSize() const {
    float p = GetPadding();
    float s = GetSpacing();
    int nbChildren = static_cast<int>(children.size());

    // Total space consumed by layout: 2*padding + (n-1)*spacing
    float totalPadding = 2.0f * p;
    float totalSpacing = (nbChildren > 1) ? s * (nbChildren - 1) : 0.0f;

    // Use the definitive scale - if zero, fall back to computing from raw bounds
    glm::vec2 size = localBounds.scale;
    if (size.x <= 0.0f || size.y <= 0.0f) {
        if (localBounds.width.type == ValueType::PIXEL && localBounds.height.type == ValueType::PIXEL) {
            size = glm::vec2(static_cast<float>(localBounds.width.value), static_cast<float>(localBounds.height.value));
        }
    }

    glm::vec2 available = size;
    // X: subtract horizontal padding only
    available.x -= totalPadding;
    // Y: subtract vertical padding and inter-child spacing
    available.y -= (totalPadding + totalSpacing);

    // Ensure non-negative
    available.x = std::min(std::max(available.x, 0.0f), 3.0f * size.x / 4.0f);
    available.y = std::min(std::max(available.y, 0.0f), 3.0f * size.y / 4.0f);

    return available;
}


void UIVBoxBase::DoSetChildAlignment(HAlign align) {
    childAlignment = align;
    MarkChildLayoutDirty();
}

void UIVBoxBase::DoSetJustifyContent(JustifyContent j) {
    justifyContent = j;
    MarkChildLayoutDirty();
}

}
