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
    float containerWidth = std::max(maxWidth + 2 * padding, GetPixelSize().x);
    float containerHeight = std::max(totalChildrenHeight + 2 * padding, GetPixelSize().y);

    // Calculate starting offset and extra spacing based on JustifyContent
    float currentSpacing = spacing;

    if (justifyContent.Get() != JustifyContent::START && containerHeight > totalChildrenHeight + 2 * padding) {
        float freeSpace = containerHeight - (2 * padding + totalChildrenHeight);
        switch (justifyContent.Get()) {
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
        switch (childAlignment.Get()) {
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

    contentSize.Set({containerWidth, containerHeight});
}




void UIVBoxBase::CalculateContentSize() {
    glm::vec2 size = {0, 0};
    float maxWidth = 0.0f;
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        size.y += childSize.y;
        maxWidth = std::max(maxWidth, childSize.x);
    }
    if (!children.empty()) size.y += (children.size() - 1) * spacing.Get();

    size.x = std::max(maxWidth + 2 * padding.Get(), GetPixelSize().x);
    size.y = std::max(size.y + 2 * padding.Get(), GetPixelSize().y);

    localBounds.ModifyForce([&](Bounds& b) { b.scale = size; });
    contentSize.Set(size);
}

void UIVBoxBase::DoSetChildAlignment(HAlign align) {
    childAlignment.Set(align);
}

void UIVBoxBase::DoSetJustifyContent(JustifyContent j) {
    justifyContent.Set(j);
}

void UIVBoxBase::Update() {
    // FIX: Accumuler les changements au lieu de les écraser
    bool needsLayoutUpdate = false;
    needsLayoutUpdate = needsLayoutUpdate || childAlignment.Apply();
    needsLayoutUpdate = needsLayoutUpdate || justifyContent.Apply();

    if (needsLayoutUpdate) {
        dirtyLayout = true;
    }

    UIContainerBase::Update();
}

}
