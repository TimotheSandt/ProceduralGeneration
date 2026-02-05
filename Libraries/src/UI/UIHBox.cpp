#include "UIHBox.h"

namespace UI {

void UIHBoxBase::Initialize() {
    childAlignment.Apply();
    justifyContent.Apply();
    UIContainerBase::Initialize();
}

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
    float containerHeight = std::max(maxHeight + 2 * padding, GetPixelSize().y);
    float containerWidth = std::max(totalChildrenWidth + 2 * padding, GetPixelSize().x);

    // Calculate starting offset and extra spacing based on JustifyContent
    float xOffset = padding;
    float currentSpacing = spacing;

    // Only apply justification if we have extra space and not START alignment
    if (justifyContent.Get() != JustifyContent::START && containerWidth > totalChildrenWidth + 2 * padding) {
        float freeSpace = containerWidth - 2 * padding - (totalChildrenWidth - (visibleChildrenCount > 1 ? (visibleChildrenCount - 1) * spacing : 0));
        freeSpace = containerWidth - (2 * padding + totalChildrenWidth);

        switch (justifyContent.Get()) {
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

                    currentSpacing = spacing + freeSpace / visibleChildrenCount;
                    xOffset = padding + (freeSpace / visibleChildrenCount) / 2.0f;
                     if (visibleChildrenCount > 1) currentSpacing = freeSpace / (visibleChildrenCount - 1);
                }
                break;
             default: break;
        }



        if (justifyContent.Get() == JustifyContent::SPACE_BETWEEN && visibleChildrenCount > 1) {
             currentSpacing = spacing + freeSpace / (visibleChildrenCount - 1);
             xOffset = padding;
        } else if (justifyContent.Get() == JustifyContent::SPACE_AROUND && visibleChildrenCount > 0) {
             float extra = freeSpace / visibleChildrenCount;
             currentSpacing = spacing + extra; // This expands spacing
             xOffset = padding + extra / 2.0f;
        }
    }

    // Second pass: set positions with vertical alignment
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();

        float yPos = padding;
        switch (childAlignment.Get()) {
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
    if (contentSize.ForceSet({containerWidth, containerHeight})) {
        InitializedFBO();
    };
}

void UIHBoxBase::DoSetChildAlignment(VAlign align) {
    childAlignment.Set(align);
}

void UIHBoxBase::DoSetJustifyContent(JustifyContent j) {
    justifyContent.Set(j);
}

void UIHBoxBase::Update() {
    dirtyLayout = dirtyLayout || childAlignment.Apply();
    dirtyLayout = dirtyLayout || justifyContent.Apply();

    UIContainerBase::Update();
}

}
