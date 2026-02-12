#include "UIVBox.h"


namespace UI {



// ============ UIVBoxBase Implementation ============

void UIVBoxBase::RecalculateChildBounds() {
    float padding = GetPadding();
    float spacing = GetSpacing();

    float totalChildrenHeight = 0;
    float totalChildrenWidth = 0;
    int visibleChildrenCount = 0;

    // First pass: calculate dimensions
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        totalChildrenWidth = std::max(totalChildrenWidth, childSize.x);
        totalChildrenHeight += childSize.y;
        visibleChildrenCount++;
    }

    // Add spacing to total height calculation
    if (visibleChildrenCount > 1) {
        totalChildrenHeight += (visibleChildrenCount - 1) * spacing;
    }

    // Content size
    float containerWidth = std::max(totalChildrenWidth + 2 * padding, localBounds.scale.x);
    float containerHeight = std::max(totalChildrenHeight + 2 * padding, localBounds.scale.y);
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
        actualSpacing = actualSpacing * reducer.y;

        float totalReduceWidth = 0;
        float totalReduceHeight = 0;
        for (auto& child : children) {
            float ratio = std::min(reducer.x, reducer.y);
            glm::vec2 redducerChild = {
                child->DoesAllowDeform() ? reducer.x : ratio,
                child->DoesAllowDeform() ? reducer.y : ratio
            };
            child->SetPixelSize(child->GetPixelSize() * redducerChild);
            totalReduceWidth = std::max(totalReduceWidth, child->GetPixelSize().x);
            totalReduceHeight += child->GetPixelSize().y;
        }

        if (visibleChildrenCount > 1) {
            totalReduceHeight += (visibleChildrenCount - 1) * actualSpacing;
        }

        totalChildrenWidth = totalReduceWidth;
        totalChildrenHeight = totalReduceHeight;
        containerWidth = std::max(totalChildrenWidth, localBounds.scale.x);
        containerHeight = std::max(totalChildrenHeight, localBounds.scale.y);
        contentSize = {containerWidth, containerHeight};
    }

    // Calculate starting offset and extra spacing based on JustifyContent
    float yOffset = actualPadding.y;
    float currentSpacing = actualSpacing;

    // Only apply justification if we have extra space and not START alignment
    if (justifyContent != JustifyContent::START && containerHeight > totalChildrenHeight + 2 * actualPadding.y) {
        float freeSpace = containerHeight - (2 * actualPadding.y + totalChildrenHeight);

        switch (justifyContent) {
            case JustifyContent::CENTER:
                yOffset = actualPadding.y + freeSpace / 2.0f;
                break;
            case JustifyContent::END:
                yOffset = containerHeight - actualPadding.y - totalChildrenHeight;
                break;
            case JustifyContent::SPACE_BETWEEN:
                yOffset = actualPadding.y;
                if (visibleChildrenCount > 1) {
                    currentSpacing = actualSpacing + freeSpace / (visibleChildrenCount - 1);
                }
                break;
            case JustifyContent::SPACE_AROUND:
                if (visibleChildrenCount > 0) {
                    float extra = freeSpace / visibleChildrenCount;
                    currentSpacing = actualSpacing + extra;
                    yOffset = actualPadding.y + extra / 2.0f;
                    if (visibleChildrenCount > 1) currentSpacing = freeSpace / (visibleChildrenCount - 1);
                }
                break;
             default: break;
        }

        if (justifyContent == JustifyContent::SPACE_BETWEEN && visibleChildrenCount > 1) {
             currentSpacing = actualSpacing + freeSpace / (visibleChildrenCount - 1);
             yOffset = actualPadding.y;
        } else if (justifyContent == JustifyContent::SPACE_AROUND && visibleChildrenCount > 0) {
             float extra = freeSpace / visibleChildrenCount;
             currentSpacing = actualSpacing + extra;
             yOffset = actualPadding.y + extra / 2.0f;
        }
    }

    // Second pass: set positions with horizontal alignment
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();

        float xPos = actualPadding.x;
        switch (childAlignment) {
            case HAlign::LEFT:
                xPos = actualPadding.x;
                break;
            case HAlign::CENTER:
                xPos = (containerWidth - childSize.x) / 2.0f;
                break;
            case HAlign::RIGHT:
                xPos = containerWidth - actualPadding.x - childSize.x;
                break;
        }

        child->SetCachedBoundsInParent({xPos, yOffset, childSize.x, childSize.y});
        yOffset += childSize.y + currentSpacing;
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
    available.x = std::max(std::max(available.x, 0.0f), 3.0f * size.x / 4.0f);
    available.y = std::max(std::max(available.y, 0.0f), 3.0f * size.y / 4.0f);

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
