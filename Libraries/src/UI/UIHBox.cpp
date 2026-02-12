#include "UIHBox.h"

namespace UI {


void UIHBoxBase::RecalculateChildBounds() {
    float padding = GetPadding();
    float spacing = GetSpacing();

    float totalChildrenHeight = 0;
    float totalChildrenWidth = 0;
    int visibleChildrenCount = 0;

    // First pass: calculate dimensions
    for (auto& child : children) {
        // if (!child->IsVisible()) continue;
        glm::vec2 childSize = child->GetPixelSize();
        totalChildrenHeight = std::max(totalChildrenHeight, childSize.y);
        totalChildrenWidth += childSize.x;
        visibleChildrenCount++;
    }

    // Add spacing to total width calculation
    if (visibleChildrenCount > 1) {
        totalChildrenWidth += (visibleChildrenCount - 1) * spacing;
    }

    // Content size
    float containerHeight = std::max(totalChildrenHeight + 2 * padding, localBounds.scale.y);
    float containerWidth = std::max(totalChildrenWidth + 2 * padding, localBounds.scale.x);
    contentSize = {containerWidth, containerHeight};

    glm::vec2 reducer = {1.0f, 1.0f};
    glm::vec2 actualPadding = {padding, padding};
    float actualSpacing = std::max(spacing, 0.0f);

    if (this->overflowMode.Get() == OverflowMode::WRAP &&
        (contentSize.x > localBounds.scale.x || contentSize.y > localBounds.scale.y))
    {
        // Ensure non-zero contentSize to avoid division by zero
        if (contentSize.x > 0 && contentSize.y > 0) {
            reducer = {
                std::min(localBounds.scale.x / contentSize.x, 1.0f),
                std::min(localBounds.scale.y / contentSize.y, 1.0f)
            };
        } else {
            reducer = {1.0f, 1.0f};
        }

        actualPadding = actualPadding * reducer;
        actualSpacing = actualSpacing * reducer.x;

        float totalReduceWidth = 0;
        float totalReduceHeight = 0;
        for (auto& child : children) {
            float ratio = std::min(reducer.x, reducer.y);
            glm::vec2 redducerChild = {
                child->DoesAllowDeform() ? reducer.x : ratio,
                child->DoesAllowDeform() ? reducer.y : ratio
            };
            child->SetPixelSize(child->GetPixelSize() * redducerChild);
            totalReduceWidth += child->GetPixelSize().x;
            totalReduceHeight = std::max(totalReduceHeight, child->GetPixelSize().y);
        }

        if (visibleChildrenCount > 1) {
            totalReduceWidth += (visibleChildrenCount - 1) * actualSpacing;
        }

        totalChildrenWidth = totalReduceWidth;
        totalChildrenHeight = totalReduceHeight;
        containerWidth = std::max(totalChildrenWidth + 2 * actualPadding.x, localBounds.scale.x);
        containerHeight = std::max(totalChildrenHeight + 2 * actualPadding.y, localBounds.scale.y);
        contentSize = {containerWidth, containerHeight};
    }

    // Calculate starting offset and extra spacing based on JustifyContent
    float xOffset = actualPadding.x;
    float currentSpacing = actualSpacing;

    // Only apply justification if we have extra space and not START alignment
    if (justifyContent != JustifyContent::START && containerWidth > totalChildrenWidth + 2 * actualPadding.x) {
        float freeSpace = containerWidth - 2 * actualPadding.x - (totalChildrenWidth - (visibleChildrenCount > 1 ? (visibleChildrenCount - 1) * actualSpacing : 0));
        freeSpace = containerWidth - (2 * actualPadding.x + totalChildrenWidth);

        switch (justifyContent) {
            case JustifyContent::CENTER:
                xOffset = actualPadding.x + freeSpace / 2.0f;
                break;
            case JustifyContent::END:
                xOffset = containerWidth - actualPadding.x - totalChildrenWidth + (visibleChildrenCount > 1 ? (visibleChildrenCount - 1) * actualSpacing : 0);
                xOffset = containerWidth - actualPadding.x - totalChildrenWidth;
                break;
            case JustifyContent::SPACE_BETWEEN:
                xOffset = actualPadding.x;
                if (visibleChildrenCount > 1) {
                    currentSpacing = actualSpacing + freeSpace / (visibleChildrenCount - 1);
                }
                break;
            case JustifyContent::SPACE_AROUND:
                if (visibleChildrenCount > 0) {
                    float extraPerItem = freeSpace / visibleChildrenCount;
                    currentSpacing = actualSpacing + freeSpace / visibleChildrenCount;
                    xOffset = actualPadding.x + (freeSpace / visibleChildrenCount) / 2.0f;
                     if (visibleChildrenCount > 1) currentSpacing = freeSpace / (visibleChildrenCount - 1);
                }
                break;
             default: break;
        }



        if (justifyContent == JustifyContent::SPACE_BETWEEN && visibleChildrenCount > 1) {
             currentSpacing = actualSpacing + freeSpace / (visibleChildrenCount - 1);
             xOffset = actualPadding.x;
        } else if (justifyContent == JustifyContent::SPACE_AROUND && visibleChildrenCount > 0) {
             float extra = freeSpace / visibleChildrenCount;
             currentSpacing = actualSpacing + extra; // This expands actualSpacing
             xOffset = actualPadding.x + extra / 2.0f;
        }
    }

    // Second pass: set positions with vertical alignment
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();

        float yPos = actualPadding.y;
        switch (childAlignment) {
            case VAlign::TOP:
                yPos = actualPadding.y;
                break;
            case VAlign::CENTER:
                yPos = (containerHeight - childSize.y) / 2.0f;
                break;
            case VAlign::BOTTOM:
                yPos = containerHeight - actualPadding.y - childSize.y;
                break;
        }

        child->SetCachedBoundsInParent({xOffset, yPos, childSize.x, childSize.y});
        xOffset += childSize.x + currentSpacing;
    }
}

glm::vec2 UIHBoxBase::GetAvailableSize() const {
    float p = GetPadding();
    float s = GetSpacing();
    int nbChildren = static_cast<int>(children.size());

    // Total space consumed by layout: 2*padding + (n-1)*actualSpacing
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
    // X: subtract horizontal padding and inter-child actualSpacing (main axis)
    available.x -= (totalPadding + totalSpacing);
    // Y: subtract vertical padding only (cross axis)
    available.y -= totalPadding;

    // Ensure non-negative, clamped to at most 75% of total size
    available.x = std::max(std::max(available.x, 0.0f), size.x * 0.75f);
    available.y = std::max(std::max(available.y, 0.0f), size.y * 0.75f);

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
