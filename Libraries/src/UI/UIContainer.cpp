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

void UIContainer::AddChild(std::shared_ptr<UIComponent> child) {
    children.push_back(child);
    child->SetParent(shared_from_this());

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

    // Clear entire FBO
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

    // Render children directly (no FBO for now)
    RecalculateChildBounds();

    for (auto& child : children) {
        glm::vec2 childOffset = GetChildOffset(child);
        child->Draw(containerSize, myOffset + childOffset);
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
    // Default implementation: stack children at origin
    // Override in HBox/VBox for proper layout
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        child->SetCachedBoundsInParent({0, 0, childSize.x, childSize.y});
    }
}


// ============ UIHBox Implementation ============

void UIHBox::RecalculateChildBounds() {
    float xOffset = 0;
    float maxHeight = 0;

    // First pass: calculate total width and max height
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        maxHeight = std::max(maxHeight, childSize.y);
    }

    // Content size
    float containerHeight = std::max(maxHeight, localBounds.scale.y);

    // Second pass: set positions with vertical alignment
    xOffset = 0;
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();

        float yPos = 0;
        switch (childAlignment) {
            case VAlign::TOP:
                yPos = 0;
                break;
            case VAlign::CENTER:
                yPos = (containerHeight - childSize.y) / 2.0f;
                break;
            case VAlign::BOTTOM:
                yPos = containerHeight - childSize.y;
                break;
        }

        child->SetCachedBoundsInParent({xOffset, yPos, childSize.x, childSize.y});
        xOffset += childSize.x;
    }

    contentSize = {xOffset, containerHeight};
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

    // Render children directly (no FBO)
    for (auto& child : children) {
        glm::vec2 childOffset = GetChildOffset(child);
        child->Draw(containerSize, myOffset + childOffset);
    }

    dirty = DirtyType::NONE;
}


// ============ UIVBox Implementation ============

void UIVBox::RecalculateChildBounds() {
    float yOffset = 0;
    float maxWidth = 0;

    // First pass: calculate max width
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        maxWidth = std::max(maxWidth, childSize.x);
    }

    // Content width
    float containerWidth = std::max(maxWidth, localBounds.scale.x);

    // Second pass: set positions with horizontal alignment
    yOffset = 0;
    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();

        float xPos = 0;
        switch (childAlignment) {
            case HAlign::LEFT:
                xPos = 0;
                break;
            case HAlign::CENTER:
                xPos = (containerWidth - childSize.x) / 2.0f;
                break;
            case HAlign::RIGHT:
                xPos = containerWidth - childSize.x;
                break;
        }

        child->SetCachedBoundsInParent({xPos, yOffset, childSize.x, childSize.y});
        yOffset += childSize.y;
    }

    contentSize = {containerWidth, yOffset};
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

    // Render children directly (no FBO)
    for (auto& child : children) {
        glm::vec2 childOffset = GetChildOffset(child);
        child->Draw(containerSize, myOffset + childOffset);
    }

    dirty = DirtyType::NONE;
}


}
