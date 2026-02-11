#include "UIHBox.h"

namespace UI {


void UIHBoxBase::RecalculateChildBounds() {
    float padding = GetPadding();
    float spacing = GetSpacing();

    float maxHeight = 0;
    float totalChildrenWidth = 0;
    int visibleChildrenCount = 0;

    // First pass: calculate dimensions
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        maxHeight = std::max(maxHeight, childSize.y);
        totalChildrenWidth += childSize.x;
        visibleChildrenCount++;
    }

    // Add spacing to total width calculation
    if (visibleChildrenCount > 1) {
        totalChildrenWidth += (visibleChildrenCount - 1) * spacing;
    }

    // Content size
    float containerHeight = std::max(maxHeight + 2 * padding, localBounds.scale.y);
    float containerWidth = std::max(totalChildrenWidth + 2 * padding, localBounds.scale.x);

    // Calculate starting offset and extra spacing based on JustifyContent
    float xOffset = padding;
    float currentSpacing = spacing;

    // Only apply justification if we have extra space and not START alignment
    if (justifyContent != JustifyContent::START && containerWidth > totalChildrenWidth + 2 * padding) {
        float freeSpace = containerWidth - 2 * padding - (totalChildrenWidth - (visibleChildrenCount > 1 ? (visibleChildrenCount - 1) * spacing : 0));
        freeSpace = containerWidth - (2 * padding + totalChildrenWidth);

        switch (justifyContent) {
            case JustifyContent::CENTER:
                xOffset = padding + freeSpace / 2.0f;
                break;
            case JustifyContent::END:
                xOffset = containerWidth - padding - totalChildrenWidth + (visibleChildrenCount > 1 ? (visibleChildrenCount - 1) * spacing : 0);
                xOffset = containerWidth - padding - totalChildrenWidth;
                break;
            case JustifyContent::SPACE_BETWEEN:
                xOffset = padding;
                if (visibleChildrenCount > 1) {
                    currentSpacing = spacing + freeSpace / (visibleChildrenCount - 1);
                }
                break;
            case JustifyContent::SPACE_AROUND:
                if (visibleChildrenCount > 0) {
                    float extraPerItem = freeSpace / visibleChildrenCount;
                    currentSpacing = spacing + freeSpace / visibleChildrenCount;
                    xOffset = padding + (freeSpace / visibleChildrenCount) / 2.0f;
                     if (visibleChildrenCount > 1) currentSpacing = freeSpace / (visibleChildrenCount - 1);
                }
                break;
             default: break;
        }



        if (justifyContent == JustifyContent::SPACE_BETWEEN && visibleChildrenCount > 1) {
             currentSpacing = spacing + freeSpace / (visibleChildrenCount - 1);
             xOffset = padding;
        } else if (justifyContent == JustifyContent::SPACE_AROUND && visibleChildrenCount > 0) {
             float extra = freeSpace / visibleChildrenCount;
             currentSpacing = spacing + extra; // This expands spacing
             xOffset = padding + extra / 2.0f;
        }
    }

    // Second pass: set positions with vertical alignment
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();

        float yPos = padding;
        switch (childAlignment) {
            case VAlign::TOP:
                yPos = padding;
                break;
            case VAlign::CENTER:
                yPos = (containerHeight - childSize.y) / 2.0f;
                break;
            case VAlign::BOTTOM:
                yPos = containerHeight - padding - childSize.y;
                break;
        }

        child->SetCachedBoundsInParent({xOffset, yPos, childSize.x, childSize.y});
        xOffset += childSize.x + currentSpacing;
    }

    // Use calculated container size (assuming we expand to fill if justify is used, or just bounding box)
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


glm::vec2 UIHBoxBase::GetAvailableSize() const {
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
    // X: subtract horizontal padding and inter-child spacing (main axis)
    available.x -= (totalPadding + totalSpacing);
    // Y: subtract vertical padding only (cross axis)
    available.y -= totalPadding;

    // Ensure non-negative
    available.x = std::min(std::max(available.x, 0.0f), 3.0f * size.x / 4.0f);
    available.y = std::min(std::max(available.y, 0.0f), 3.0f * size.y / 4.0f);

    return available;
}

void UIHBoxBase::DoSetChildAlignment(VAlign align) {
    childAlignment = align;
    MarkChildLayoutDirty();
}

void UIHBoxBase::DoSetJustifyContent(JustifyContent j) {
    justifyContent = j;
    MarkChildLayoutDirty();
}

}
