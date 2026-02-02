#include "UIContainer.h"
#include "utilities.h"
#include <glad/glad.h>

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
    fbo.Bind();

    // Save current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Set viewport to FBO size
    glViewport(0, 0, static_cast<GLsizei>(contentSize.x), static_cast<GLsizei>(contentSize.y));

    // Clear entire FBO
    glClear(GL_COLOR_BUFFER_BIT);

    // Render all children
    for (auto& child : children) {
        child->Draw(contentSize);
        child->ClearDirty();
    }

    fbo.Unbind();

    // Restore viewport
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}


void UIContainer::RenderDirtyChildren() {
    fbo.Bind();

    // Save current viewport
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // Set viewport to FBO size
    glViewport(0, 0, static_cast<GLsizei>(contentSize.x), static_cast<GLsizei>(contentSize.y));

    // Only render dirty children
    for (auto& child : children) {
        if (child->IsDirty()) {
            // Clear only the zone of this child
            ClearZone(child->GetCachedBoundsInParent());

            // Render the child
            child->Draw(contentSize);
            child->ClearDirty();
        }
    }

    fbo.Unbind();

    // Restore viewport
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
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

    // Ensure content size is at least as big as our visible size
    if (contentSize.x < localBounds.scale.x) contentSize.x = localBounds.scale.x;
    if (contentSize.y < localBounds.scale.y) contentSize.y = localBounds.scale.y;

    EnsureFBOInitialized();

    if (dirty != DirtyType::NONE) {
        if (dirty == DirtyType::ALL || dirty == DirtyType::TRANSFORM) {
            // Full re-render when ALL or TRANSFORM dirty
            RenderToFBO();
        } else {
            // Partial re-render for CONTENT, ANIMATION, etc.
            RenderDirtyChildren();
        }
    }

    // Clear our dirty state (but don't notify parent, we're in Draw)
    dirty = DirtyType::NONE;

    // Draw the FBO as a textured quad with scroll offset
    mesh.InitUniform2f("offset", glm::value_ptr(offset));
    mesh.InitUniform2f("scale", glm::value_ptr(localBounds.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform2f("scrollOffset", glm::value_ptr(scrollOffset));
    mesh.InitUniform2f("contentSize", glm::value_ptr(contentSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color));

    // Bind FBO texture directly
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo.GetTextureID());
    GLint texSamplerLoc = 0;
    mesh.InitUniform1i("textureSampler", &texSamplerLoc);

    mesh.Draw();
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

    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        child->SetCachedBoundsInParent({xOffset, 0, childSize.x, childSize.y});
        xOffset += childSize.x;
        maxHeight = std::max(maxHeight, childSize.y);
    }

    // Content size is total width of all children, max height
    contentSize = {xOffset, std::max(maxHeight, localBounds.scale.y)};
}


void UIHBox::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (!visible) return;

    if (needsInitialize) {
        Initialize();
        needsInitialize = false;
    }

    GetPixelSize();
    RecalculateChildBounds();
    EnsureFBOInitialized();

    if (dirty != DirtyType::NONE) {
        if (dirty == DirtyType::ALL || dirty == DirtyType::TRANSFORM) {
            RenderToFBO();
        } else {
            RenderDirtyChildren();
        }
    }

    dirty = DirtyType::NONE;

    // Draw with scroll support
    mesh.InitUniform2f("offset", glm::value_ptr(offset));
    mesh.InitUniform2f("scale", glm::value_ptr(localBounds.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform2f("scrollOffset", glm::value_ptr(scrollOffset));
    mesh.InitUniform2f("contentSize", glm::value_ptr(contentSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color));

    // Bind FBO texture directly
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo.GetTextureID());
    GLint texSamplerLoc = 0;
    mesh.InitUniform1i("textureSampler", &texSamplerLoc);

    mesh.Draw();
}


// ============ UIVBox Implementation ============

void UIVBox::RecalculateChildBounds() {
    float yOffset = 0;
    float maxWidth = 0;

    for (auto& child : children) {
        glm::vec2 childSize = child->GetPixelSize();
        child->SetCachedBoundsInParent({0, yOffset, childSize.x, childSize.y});
        yOffset += childSize.y;
        maxWidth = std::max(maxWidth, childSize.x);
    }

    // Content size is max width, total height of all children
    contentSize = {std::max(maxWidth, localBounds.scale.x), yOffset};
}


void UIVBox::Draw(glm::vec2 containerSize, glm::vec2 offset) {
    if (!visible) return;

    if (needsInitialize) {
        Initialize();
        needsInitialize = false;
    }

    GetPixelSize();
    RecalculateChildBounds();
    EnsureFBOInitialized();

    if (dirty != DirtyType::NONE) {
        if (dirty == DirtyType::ALL || dirty == DirtyType::TRANSFORM) {
            RenderToFBO();
        } else {
            RenderDirtyChildren();
        }
    }

    dirty = DirtyType::NONE;

    // Draw with scroll support
    mesh.InitUniform2f("offset", glm::value_ptr(offset));
    mesh.InitUniform2f("scale", glm::value_ptr(localBounds.scale));
    mesh.InitUniform2f("containerSize", glm::value_ptr(containerSize));
    mesh.InitUniform2f("scrollOffset", glm::value_ptr(scrollOffset));
    mesh.InitUniform2f("contentSize", glm::value_ptr(contentSize));
    mesh.InitUniform4f("color", glm::value_ptr(this->color));

    // Bind FBO texture directly
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, fbo.GetTextureID());
    GLint texSamplerLoc = 0;
    mesh.InitUniform1i("textureSampler", &texSamplerLoc);

    mesh.Draw();
}


}
