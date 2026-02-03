#include "UIContainer.h"

namespace UI {


void UIHBox::RecalculateChildBounds() {
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
        // Note: totalChildrenWidth included spacing, so we remove it to get raw children width for freeSpace calc if we reconstruct it,
        // OR simply: freeSpace = containerWidth - (2*padding + totalChildrenWidth).
        freeSpace = containerWidth - (2 * padding + totalChildrenWidth);

        switch (justifyContent) {
            case JustifyContent::CENTER:
                xOffset = padding + freeSpace / 2.0f;
                break;
            case JustifyContent::END:
                xOffset = containerWidth - padding - totalChildrenWidth + (visibleChildrenCount > 1 ? (visibleChildrenCount - 1) * spacing : 0);
                // Actually simpler: containerWidth - padding - (width of children sans spacing) - (spacing between them)
                // Let's use standard logic: xOffset start.
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
                    currentSpacing = spacing + extraPerItem; // Not quite, Space Around puts half space at ends
                    // Space Around: | 0.5 | Item | 1 | Item | 0.5 |
                    // Simpler impl: distribute free space
                    currentSpacing = spacing + freeSpace / visibleChildrenCount;
                    xOffset = padding + (freeSpace / visibleChildrenCount) / 2.0f;
                    // Wait, standard Space Around distributes specific way.
                    // Let's stick to standard flexbox:
                    // spread = freeSpace / count.
                    // gap = spacing + spread? No, spacing is fixed usually.
                    // If we emulate explicit spacing:
                    // Space Between ignores fixed spacing? No, usually adds to it or overrides it.
                    // Let's assume Justify overrides fixed spacing for Between/Around.
                     if (visibleChildrenCount > 1) currentSpacing = freeSpace / (visibleChildrenCount - 1);
                }
                break;
             default: break;
        }

        // Refined Space Around/Between logic for fixed spacing + justify?
        // Usually JustifyContent interacts with gap.
        // Let's stick to: Justify distributes FREE space.
        // If Justify::CENTER, we just move start.
        // If Justify::SPACE_BETWEEN, we increase spacing.

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
    // Actually we keep calculated size.
    contentSize = {containerWidth, containerHeight};
}




void UIHBox::CalculateContentSize() {
    glm::vec2 size = {0, 0};
    float maxHeight = 0.0f;
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        size.x += childSize.x;
        maxHeight = std::max(maxHeight, childSize.y);
    }
    if (!children.empty()) size.x += (children.size() - 1) * spacing;

    size.x = std::max(size.x + 2 * padding, GetPixelSize().x);
    size.y = std::max(maxHeight + 2 * padding, GetPixelSize().y);

    contentSize = size;
}

}
