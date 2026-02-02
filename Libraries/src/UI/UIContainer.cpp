#include "UIContainer.h"
#include "utilities.h"
#include <glad/glad.h>
#include "Logger.h"

namespace UI {


UIContainer::UIContainer(Bounds bounds, std::weak_ptr<UITheme> theme, IdentifierKind kind)
    : UIComponent(bounds, theme, kind) {
    contentSize = localBounds.scale;
    // Use container-specific shader with texture and scroll support
    this->mesh.SetShader(
        GET_RESOURCE_PATH("shader/UI/container.vert"),
        GET_RESOURCE_PATH("shader/UI/container.frag")
    );
}

// ... helper for handling GL state ...
void SaveFBOState(GLint& oldFBO, GLint viewport[4]) {
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &oldFBO);
    glGetIntegerv(GL_VIEWPORT, viewport);
}

void RestoreFBOState(GLint oldFBO, GLint viewport[4]) {
    glBindFramebuffer(GL_FRAMEBUFFER, oldFBO);
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}

float UIContainer::GetPadding() const {
    if (padding >= 0) return padding;
    if (auto t = theme.lock()) return t->GetPadding();
    return 0.0f;
}

float UIContainer::GetSpacing() const {
    if (spacing >= 0) return spacing;
    if (auto t = theme.lock()) return t->GetSpacing();
    return 0.0f;
}

void UIContainer::AddChild(std::shared_ptr<UIComponent> child) {
    children.push_back(child);
    child->SetParent(std::static_pointer_cast<UIContainer>(shared_from_this()));

    // Recalculate bounds when adding children
    RecalculateChildBounds();
    MarkDirty(DirtyType::ALL);
}


void UIContainer::RemoveChild(std::shared_ptr<UIComponent> child) {
    auto it = std::find(children.begin(), children.end(), child);
    if (it != children.end()) {
        children.erase(it);
        RecalculateChildBounds();
        MarkDirty(DirtyType::ALL);
    }
}


void UIContainer::EnsureFBOInitialized() {
    if (!fboInitialized ||
        fbo.GetWidth() != static_cast<int>(contentSize.x) ||
        fbo.GetHeight() != static_cast<int>(contentSize.y)) {

        if (contentSize.x > 0 && contentSize.y > 0) {
            fbo.Init(static_cast<int>(contentSize.x), static_cast<int>(contentSize.y));
            fboInitialized = true;
            GL_CHECK_ERROR_M("UIContainer FBO Init");

            // Clear FBO to transparent immediately after init
            GLint currentFBO;
            glGetIntegerv(GL_FRAMEBUFFER_BINDING, &currentFBO);
            fbo.Bind();
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClear(GL_COLOR_BUFFER_BIT);
            glBindFramebuffer(GL_FRAMEBUFFER, currentFBO);

            // Force re-render on next frame
            dirty = DirtyType::ALL;
        }
    }
}


void UIContainer::ClearZone(glm::vec4 bounds) {
    glEnable(GL_SCISSOR_TEST);
    glScissor(
        static_cast<GLint>(bounds.x),
        static_cast<GLint>(bounds.y),
        static_cast<GLsizei>(bounds.z),
        static_cast<GLsizei>(bounds.w)
    );
    glClear(GL_COLOR_BUFFER_BIT);
    glDisable(GL_SCISSOR_TEST);
}


void UIContainer::RenderToFBO() {
    GLint oldFBO;
    GLint viewport[4];
    SaveFBOState(oldFBO, viewport);

    fbo.Bind(); // This binds our FBO
    // No need to check error here as Bind() does it, but let's be safe
    GL_CHECK_ERROR_M("RenderToFBO Bind");

    // Set viewport to FBO size
    glViewport(0, 0, static_cast<GLsizei>(contentSize.x), static_cast<GLsizei>(contentSize.y));

    // Clear entire FBO with transparent color
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    // Render all children with their calculated offsets
    for (auto& child : children) {
        glm::vec2 childOffset = GetChildOffset(child);
        child->Draw(contentSize, childOffset);
        child->ClearDirty();
        // Check for error after each child draw to identify culprit
        GL_CHECK_ERROR_M("RenderToFBO Child Draw");
    }

    // Instead of fbo.Unbind(), we restore previous state
    RestoreFBOState(oldFBO, viewport);
    GL_CHECK_ERROR_M("RenderToFBO Restore");
}


void UIContainer::RenderDirtyChildren() {
    GLint oldFBO;
    GLint viewport[4];
    SaveFBOState(oldFBO, viewport);

    fbo.Bind();
    GL_CHECK_ERROR_M("RenderDirtyChildren Bind");

    // Set viewport to FBO size
    glViewport(0, 0, static_cast<GLsizei>(contentSize.x), static_cast<GLsizei>(contentSize.y));

    // Only render dirty children
    for (auto& child : children) {
        if (child->IsDirty()) {
            // Clear only the zone of this child
            ClearZone(child->GetCachedBoundsInParent());

            // Render the child with its offset
            glm::vec2 childOffset = GetChildOffset(child);
            child->Draw(contentSize, childOffset);
            child->ClearDirty();
            GL_CHECK_ERROR_M("RenderDirtyChildren Child Draw");
        }
    }

    RestoreFBOState(oldFBO, viewport);
    GL_CHECK_ERROR_M("RenderDirtyChildren Restore");
}


void UIContainer::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (!visible) return;

    // First-time initialization
    if (needsInitialize) {
        Initialize();
        needsInitialize = false;
    }

    // Update our pixel size based on parent
    GetPixelSize();

    // Calculate container's position with anchor
    glm::vec2 anchorOffset = localBounds.getAnchorOffset(containerSize);
    glm::vec2 myOffset = offset + anchorOffset;

    RecalculateChildBounds();

    // Root container (no parent) renders children directly
    // Nested containers use FBO for optimization
    bool isRoot = parent.expired();

    if (isRoot) {
        // Direct rendering for root container
        for (auto& child : children) {
            glm::vec2 childOffset = GetChildOffset(child);
            child->Draw(containerSize, myOffset + childOffset);
        }
    } else {
        // FBO rendering for nested containers
        if (contentSize.x < localBounds.scale.x) contentSize.x = localBounds.scale.x;
        if (contentSize.y < localBounds.scale.y) contentSize.y = localBounds.scale.y;

        EnsureFBOInitialized();

        if (dirty != DirtyType::NONE) {
            if (dirty == DirtyType::ALL || dirty == DirtyType::TRANSFORM) {
                RenderToFBO();
            } else {
                RenderDirtyChildren();
            }
        }

        // Draw the FBO texture
        mesh.BindShader();
        mesh.BindVAO();

        mesh.InitUniform2f("offset", glm::value_ptr(myOffset));
        mesh.InitUniform2f("scale", glm::value_ptr(localBounds.scale));
        mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
        mesh.InitUniform2f("scrollOffset", glm::value_ptr(scrollOffset));
        mesh.InitUniform2f("contentSize", glm::value_ptr(contentSize));
        mesh.InitUniform4f("color", glm::value_ptr(this->color));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.GetTextureID());
        GLint texSamplerLoc = 0;
        mesh.InitUniform1i("textureSampler", &texSamplerLoc);

        mesh.Draw();

        mesh.UnbindVAO();
        mesh.UnbindShader();
    }

    dirty = DirtyType::NONE;
}


void UIContainer::MarkDirty(DirtyType d) {
    // For containers, we don't propagate to children automatically
    // Only mark ourselves dirty and notify parent
    dirty = d;

    // If ALL or TRANSFORM, children will be re-rendered anyway in RenderToFBO
    // No need to mark them dirty individually

    NotifyParentDirty();
}


void UIContainer::RecalculateChildBounds() {
    float p = GetPadding();
    // Default implementation: stack children at origin + padding
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        child->SetCachedBoundsInParent({p, p, childSize.x, childSize.y});
    }
}


// ============ UIHBox Implementation ============

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


void UIHBox::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (!visible) return;

    if (needsInitialize) {
        Initialize();
        needsInitialize = false;
    }

    GetPixelSize();
    RecalculateChildBounds();

    // Calculate anchor offset
    glm::vec2 anchorOffset = localBounds.getAnchorOffset(containerSize);
    glm::vec2 myOffset = offset + anchorOffset;

    // Root container renders directly, nested use FBO
    bool isRoot = parent.expired();

    if (isRoot) {
        for (auto& child : children) {
            glm::vec2 childOffset = GetChildOffset(child);
            child->Draw(containerSize, myOffset + childOffset);
        }
    } else {
        // FBO rendering
        if (contentSize.x < localBounds.scale.x) contentSize.x = localBounds.scale.x;
        if (contentSize.y < localBounds.scale.y) contentSize.y = localBounds.scale.y;

        EnsureFBOInitialized();

        if (dirty != DirtyType::NONE) {
            if (dirty == DirtyType::ALL || dirty == DirtyType::TRANSFORM) {
                RenderToFBO();
            } else {
                RenderDirtyChildren();
            }
        }

        mesh.BindShader();
        mesh.BindVAO();

        mesh.InitUniform2f("offset", glm::value_ptr(myOffset));
        mesh.InitUniform2f("scale", glm::value_ptr(localBounds.scale));
        mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
        mesh.InitUniform2f("scrollOffset", glm::value_ptr(scrollOffset));
        mesh.InitUniform2f("contentSize", glm::value_ptr(contentSize));
        mesh.InitUniform4f("color", glm::value_ptr(this->color));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.GetTextureID());
        GLint texSamplerLoc = 0;
        mesh.InitUniform1i("textureSampler", &texSamplerLoc);

        mesh.Draw();

        mesh.UnbindVAO();
        mesh.UnbindShader();
    }

    dirty = DirtyType::NONE;
}


// ============ UIVBox Implementation ============

void UIVBox::RecalculateChildBounds() {
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
}


void UIVBox::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (!visible) return;

    if (needsInitialize) {
        Initialize();
        needsInitialize = false;
    }

    GetPixelSize();
    RecalculateChildBounds();

    // Calculate anchor offset
    glm::vec2 anchorOffset = localBounds.getAnchorOffset(containerSize);
    glm::vec2 myOffset = offset + anchorOffset;

    // Root container renders directly, nested use FBO
    bool isRoot = parent.expired();

    if (isRoot) {
        for (auto& child : children) {
            glm::vec2 childOffset = GetChildOffset(child);
            child->Draw(containerSize, myOffset + childOffset);
        }
    } else {
        // FBO rendering
        if (contentSize.x < localBounds.scale.x) contentSize.x = localBounds.scale.x;
        if (contentSize.y < localBounds.scale.y) contentSize.y = localBounds.scale.y;

        EnsureFBOInitialized();

        if (dirty != DirtyType::NONE) {
            if (dirty == DirtyType::ALL || dirty == DirtyType::TRANSFORM) {
                RenderToFBO();
            } else {
                RenderDirtyChildren();
            }
        }

        mesh.BindShader();
        mesh.BindVAO();

        mesh.InitUniform2f("offset", glm::value_ptr(myOffset));
        mesh.InitUniform2f("scale", glm::value_ptr(localBounds.scale));
        mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
        mesh.InitUniform2f("scrollOffset", glm::value_ptr(scrollOffset));
        mesh.InitUniform2f("contentSize", glm::value_ptr(contentSize));
        mesh.InitUniform4f("color", glm::value_ptr(this->color));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, fbo.GetTextureID());
        GLint texSamplerLoc = 0;
        mesh.InitUniform1i("textureSampler", &texSamplerLoc);

        mesh.Draw();

        mesh.UnbindVAO();
        mesh.UnbindShader();
    }

    dirty = DirtyType::NONE;
}


}
